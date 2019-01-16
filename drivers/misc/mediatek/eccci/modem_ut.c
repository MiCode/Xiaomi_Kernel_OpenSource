/*
 * this is a dummy modem driver for loopback test and prototype design.
 * all future modem should follow this driver's behavior to match with CCCI core.
 *
 * V0.1: Xiao Wang <xiao.wang@mediatek.com>
 */

#include <linux/list.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/kdev_t.h>
#include <linux/slab.h>
#include <linux/skbuff.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/rtpm_prio.h>
#include <linux/platform_device.h>
#include <linux/ip.h>
#include "ccci_core.h"
#include "ccci_bm.h"
#include "ccci_dfo.h"
#include "ccci_platform.h"
#include "ccci_sysfs.h"
#include "modem_ut.h"
#ifdef STATISTIC
#define CREATE_TRACE_POINTS
#include "modem_ut_events.h"
#endif

extern void md_ex_monitor_func(unsigned long data);
extern void md_bootup_timeout_func(unsigned long data);

extern struct ccci_port_ops char_port_ops;
extern struct ccci_port_ops net_port_ops;
extern struct ccci_port_ops kernel_port_ops;
extern struct ccci_port_ops ipc_port_ack_ops;
static struct ccci_port md_ut_ports_normal[] = {
	{CCCI_MONITOR_CH, CCCI_MONITOR_CH, 0, 0, 0, 0, 4, &char_port_ops, MD_UT_MAJOR, 0, "ccci_monitor", },
		
	{CCCI_IPC_UART_TX, CCCI_IPC_UART_RX, 0, 0, 0xFF, 0xFF, 0, &char_port_ops, MD_UT_MAJOR, 1, "ut_ipc_uart", },
	{CCCI_IPC_TX, CCCI_IPC_RX, 0, 0, 0xFF, 0xFF, 0, &char_port_ops, MD_UT_MAJOR, 0, "ut_ipc", },
	{CCCI_IPC_RX_ACK, CCCI_IPC_TX_ACK, 0, 0, 0xFF, 0xFF, 0, &ipc_port_ack_ops, 0, 0, "ut_ipc_ack", },

	{CCCI_DUMMY_CH, CCCI_DUMMY_CH, 1, 1, 1, 1, 0, &char_port_ops, MD_UT_MAJOR, 2, "ccci_lp", },
	{CCCI_CCMNI1_TX, CCCI_CCMNI1_RX, 2, 2, 0xFF, 0xFF, 0, &net_port_ops, 0, 0, "ccemni5", },

	{CCCI_CONTROL_TX, CCCI_CONTROL_RX, 0, 0, 0, 0, 0, &kernel_port_ops, 0, 0, "ut_ctrl", },
	{CCCI_SYSTEM_TX, CCCI_SYSTEM_RX, 0, 0, 0xFF, 0xFF, 0, &kernel_port_ops, 0, 0, "ut_sys", },
	{CCCI_RPC_TX, CCCI_RPC_RX, 0, 0, 0xFF, 0xFF, 0, &kernel_port_ops, 0, 0, "ut_rpc", },
};

#define TAG "mut"

/*
 * do NOT add any static data, data should be in modem's instance
 */

static int md_ut_txq_process(struct md_ut_queue *queue)
{
	struct ccci_request *req = NULL;
	unsigned long flags;
	unsigned int count = 0;
	unsigned int packet_count = 0;
	struct ccci_modem *md = queue->modem;
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	int qno = queue->index;
	struct ccci_header* ccci_h;
	struct iphdr *ip_h;

	while(!list_empty(&queue->req_list)) {
		spin_lock_irqsave(&queue->req_lock, flags);
		req = list_first_entry(&queue->req_list, struct ccci_request, entry);
		CCCI_DBG_MSG(md->index, TAG, "process %d Tx request %p/%p on q%d\n", count, req, req->skb, queue->index);
		count++;
#ifdef LOOP_BACK
		/*
		 * LIMIT!loop back can only be done between Tx and Rx queues with the same index, 
		 * so the loop back port must be using Tx and Rx queues with the same index. and as
		 * we don't have port reference here, the loopback port's Tx/Rx channel ID must be the
		 * same, we won't modify it.
		 *
		 * ATTENTION! here we are holding Rx queue's lock
		 */
#ifndef NO_RX_Q_LOCK_IN_TX
		/*
		 * this nested lock here hurts performance, other Rx queue's locks seems won't.
		 * it also causes a kernel warning of "debug_lock off",  as ProveLock thinks this is a 
		 * recursive lock due to we acquire a queue's lock before.
		 */
		spin_lock(&md_ctrl->rxq[qno].req_lock);
#endif
		if(md_ctrl->rxq[qno].length < md_ctrl->rxq[qno].length_th) {
			// remove from Tx list
			list_del(&req->entry);
			queue->length--;
			// deal with network loopback
			ccci_h = (struct ccci_header*)req->skb->data;
			if(ccci_h->channel == CCCI_CCMNI1_TX) {
				__be32 temp;
				ccci_h->channel = CCCI_CCMNI1_RX;
				ip_h = ip_hdr(req->skb);
				temp = ip_h->saddr;
				ip_h->saddr = ip_h->daddr;
				ip_h->daddr = temp;
				skb_reset_transport_header(req->skb);
				skb_reset_network_header(req->skb);
				skb_reset_mac_header(req->skb);
			}
			if(ccci_h->channel==CCCI_DUMMY_CH && ccci_h->data[0]!=CCCI_MAGIC_NUM) {
				// insert to Rx list
				md_ctrl->rxq[qno].length++;
				list_add_tail(&req->entry, &md_ctrl->rxq[qno].req_list);
			} else {
				CCCI_DBG_MSG(md->index, TAG, "drop a Tx CCCI msg(0x%x) ch%d q%d\n", ccci_h->data[1], ccci_h->channel, queue->index);
				ccci_free_req(req);
			}
#ifndef NO_RX_Q_LOCK_IN_TX
			spin_unlock(&md_ctrl->rxq[qno].req_lock);
#endif
			spin_unlock_irqrestore(&queue->req_lock, flags);	
			packet_count++;
		} else {
			// do nothing here, let the request remains in Tx list.
#ifndef NO_RX_Q_LOCK_IN_TX
			spin_unlock(&md_ctrl->rxq[qno].req_lock);
#endif
			spin_unlock_irqrestore(&queue->req_lock, flags);
			break;
		}
#else // not loopback
		list_del(&req->entry);
		queue->length--;
		spin_unlock_irqrestore(&queue->req_lock, flags);
		CCCI_DBG_MSG(md->index, TAG, "dump a Tx request on q%d\n", queue->index);
		ccci_dump_req(req);
		ccci_free_req(req);
#endif
		if(count>=queue->budget)
			break;
	}
	wake_up_nr(&queue->req_wq, packet_count);
	return 0;
}

static int md_ut_rxq_process(struct md_ut_queue *queue)
{
	struct ccci_request *req = NULL;
	unsigned long flags;
	unsigned int count = 0;
	unsigned int packet_count = 0;
	int ret, skb_len;
	struct ccci_modem *md = queue->modem;

	while(!list_empty(&queue->req_list)) {
		/*
		 * as request is removed from queue's list in port layer, so we put ccci_port_recv_request
		 * inside spinlock. therefore, port's recv_request and req_math must be as shor as possible.
		 */
#ifndef NO_RX_Q_LOCK
		/*
		 * for normal flow, we do need lock Rx queue as it's handled only inside sched_thread, but
		 * unfortunately, we have md_ut_rx_gen_func and md_ut_stop function also touched Rx queue.
		 */
		spin_lock_irqsave(&queue->req_lock, flags);
#endif
		req = list_first_entry(&queue->req_list, struct ccci_request, entry);
		CCCI_DBG_MSG(md->index, TAG, "process %d Rx request %p/%p on q%d\n", count, req, req->skb, queue->index);
		count++;
		skb_len = req->skb->len;
		/*
		 * it's NOT safe to reference request after ccci_port_recv_request, port may do
		 * something thing to the request by then.
		 */
		ret = ccci_port_recv_request(md, req);
		/*
		 * be carefull with req->entry, if port refuse this request, req->entry will not be changed,
		 * that means it still occupied a slot in queue's list.
		 * otherwise, port will delete req->entry from queue's list and insert it into port's list.
		 * this cross-layer design can save one operation of checking whether port has space for
		 * a request before actually remove request from queue's list.
		 *
		 * BUT, virtual char message is an exception, it is not in any queue's list, so a dummy list
		 * is used.
		 * ALSO, due to req->entry is deleted from queue's list at port layer, please pay extra 
		 * attention when you try to modify current execution order of request.
		 */
		if(ret>=0 || ret==-CCCI_ERR_DROP_PACKET) {
			// list_del(&req->entry); // do NOT, req->entry is already changed when it's inserted into port's list
			queue->length--;
			packet_count++;
#ifdef STATISTIC
			queue->data_count += skb_len;
#endif
		} else {
#ifndef NO_RX_Q_LOCK
			spin_unlock_irqrestore(&queue->req_lock, flags);
#endif
			if(ret==-CCCI_ERR_CHANNEL_NUM_MIS_MATCH || ret==-CCCI_ERR_INVALID_LOGIC_CHANNEL_ID) {
				struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
				// should not happen	
				CCCI_ERR_MSG(md->index, CORE, "received illegle message from ch0x%X on q%d\n", ccci_h->channel, queue->index);
			}
			/*
			 * as we break here, so count should be equal to packet_count, it's reserved in case we need retry.
			 * but, due to we break out here, one port may block other port by not processing request.
			 */
			break;
		}
#ifndef NO_RX_Q_LOCK
		spin_unlock_irqrestore(&queue->req_lock, flags);
#endif
		
		if(count>=queue->budget)
			break;
	}
#ifdef STATISTIC
	queue->process_count+=count;
	queue->not_complet_count+=(count-packet_count);
#endif
	return 0;
}

static void kick_sched_thread(struct md_ut_ctrl *md_ctrl)
{
	md_ctrl->sched_thread_kick = 1;
	wake_up_all(&md_ctrl->sched_thread_wq);
}

static int pseudo_bootup = 0;
static const char pseudo_msg[] = "hello world 00000000\n";
#define RX_TIMER_INTVAL 1 //seconds, don't make it too short when not using Rx queue lock

static void md_ut_inject_packet(struct ccci_modem *md, struct ccci_request *req, int qno)
{
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	unsigned long flags;
	
	if(req) {
		CCCI_DBG_MSG(md->index, TAG, "inject on q%d req=%p skb=%p, len=%d\n", qno, req, req->skb, req->skb->len);
		// insert to Rx queue
		spin_lock_irqsave(&md_ctrl->rxq[qno].req_lock, flags);
		if(md_ctrl->rxq[qno].length < md_ctrl->rxq[qno].length_th) {
			md_ctrl->rxq[qno].length++;
			list_add_tail(&req->entry, &md_ctrl->rxq[qno].req_list);
		} else {
			CCCI_DBG_MSG(md->index, TAG, "dropping rx gen data\n");
			req->policy = RECYCLE;
			ccci_free_req(req);
		}
		spin_unlock_irqrestore(&md_ctrl->rxq[qno].req_lock, flags);
		// start dispatcher thread
		kick_sched_thread(md_ctrl);
	}
}

static void md_ut_rx_gen_func(unsigned long data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	struct ccci_request *req;
	struct ccci_header *ccci_h;
	unsigned int *int_ptr;

	if(pseudo_bootup>3)
		return;

	CCCI_DBG_MSG(md->index, TAG, "rx gen data %d\n", pseudo_bootup);
		
	req = ccci_alloc_req(IN, 1500, 1, 0);
	ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
	switch(pseudo_bootup) {
	case 0:
		ccci_h->data[0] = CCCI_MAGIC_NUM;
		ccci_h->data[1] = MD_INIT_START_BOOT;
		ccci_h->reserved = MD_INIT_CHK_ID;
		ccci_h->channel = CCCI_CONTROL_RX;
		break;
	case 1:
		ccci_h->data[0] = CCCI_MAGIC_NUM;
		ccci_h->data[1] = NORMAL_BOOT_ID;
		ccci_h->channel = CCCI_CONTROL_RX;
		break;
	case 2: // IPC
		ccci_h->data[0] = 0;
		ccci_h->data[1] = sizeof(pseudo_msg) + sizeof(struct ccci_header);
		ccci_h->channel = CCCI_IPC_RX;
		ccci_h->reserved = (1<<31)|0; // AP unity ID 0
		snprintf(skb_put(req->skb, sizeof(pseudo_msg)), sizeof(pseudo_msg), "this is IPC %08X\n", pseudo_bootup);
		break;
	case 3: // RPC
		ccci_h->data[0] = 0;
		ccci_h->data[1] = 16 + sizeof(struct ccci_header); // payload is 4 integer
		ccci_h->channel = CCCI_RPC_RX;
		ccci_h->reserved = 0;
		int_ptr = (unsigned int *)(req->skb->data + sizeof(struct ccci_header));
		*(int_ptr++) = 0x4006; // IPC_RPC_GET_GPIO_VAL_OP
		*(int_ptr++) = 1; // 1 parameter
		*(int_ptr++) = 4; // length = sizeof(int)
		*(int_ptr++) = 1; // GPIO number
		break;
	default:
		ccci_h->data[1] = sizeof(pseudo_msg) + sizeof(struct ccci_header);
		ccci_h->channel = CCCI_MONITOR_CH;
		// prepare dummy message
		//strcpy(skb_put(req->skb, sizeof(pseudo_msg)), pseudo_msg);
		snprintf(skb_put(req->skb, sizeof(pseudo_msg)), sizeof(pseudo_msg), "hello world %08X\n", pseudo_bootup);
		break;
	};
	pseudo_bootup++;
	md_ut_inject_packet(md, req, 0);
	mod_timer(&md_ctrl->rx_gen_timer, jiffies+RX_TIMER_INTVAL*HZ);
}

#ifdef STATISTIC
#define STATISTIC_DUMP_INTERVAL 10 // seconds
void md_ut_statistic_timer_func(unsigned long data)
{
	static unsigned long long sched_time;
	unsigned int temp, temp2, txqC=0, rxqC=0, txqR=0, rxqR=0; // hardcode
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	int i;
	
	if((sched_clock()-sched_time)>((unsigned long long)STATISTIC_DUMP_INTERVAL*1000000000)) {
		sched_time = sched_clock();
		for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
			if(md_ctrl->txq[i].process_count>0) {
				temp = 100-md_ctrl->txq[i].not_complet_count*100/md_ctrl->txq[i].process_count;
				printk("[CCCI-Q]Txq%d %d/%d %d\n", i, 
					md_ctrl->txq[i].not_complet_count, 
					md_ctrl->txq[i].process_count,
					temp2 = md_ctrl->txq[i].data_count/1000/STATISTIC_DUMP_INTERVAL);
				if(i==1) {
					txqC = temp;
					txqR = temp2;
				}
				md_ctrl->txq[i].not_complet_count = 0;
				md_ctrl->txq[i].process_count = 0;
				md_ctrl->txq[i].data_count = 0;
			}
		}
		for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
			if(md_ctrl->rxq[i].process_count>0) {
				temp = 100-md_ctrl->rxq[i].not_complet_count*100/md_ctrl->rxq[i].process_count;
				printk("[CCCI-Q]Rxq%d %d/%d %d\n", i, 
					md_ctrl->rxq[i].not_complet_count, 
					md_ctrl->rxq[i].process_count,
					temp2 = md_ctrl->rxq[i].data_count/1000/STATISTIC_DUMP_INTERVAL);
				if(i==1) {
					rxqC = temp;
					rxqR = temp2;
				}
				md_ctrl->rxq[i].not_complet_count = 0;
				md_ctrl->rxq[i].process_count = 0;
				md_ctrl->rxq[i].data_count = 0;
			}
		}
		trace_process_count(txqC, rxqC, txqR, rxqR);
		sched_time = sched_clock();
	}
	mod_timer(&md_ctrl->statistic_timer, jiffies+STATISTIC_DUMP_INTERVAL*HZ);
}
#endif

static int md_ut_sched_thread(void *arg)
{
	struct ccci_modem *md = arg;
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	int i;
	struct sched_param param = { .sched_priority = RTPM_PRIO_MTLTE_SYS_SDIO_THREAD };

	CCCI_DBG_MSG(md->index, TAG, "modem UT's sched thread runnning\n");
	sched_setscheduler(current, SCHED_FIFO, &param);

	while (1) {
		CCCI_DBG_MSG(md->index, TAG, "modem UT's sched thread loop\n");
		wait_event_interruptible(
				md_ctrl->sched_thread_wq,
				(kthread_should_stop()
				|| md_ctrl->sched_thread_kick));	
		md_ctrl->sched_thread_kick = 0;
		
		if (kthread_should_stop()){
				break ;
		}

		for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) { // for loopback, process Tx first
			md_ut_txq_process(&md_ctrl->txq[i]);
		}
		for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
			md_ut_rxq_process(&md_ctrl->rxq[i]);
		}
		/*
		 * when Rx is full and Tx stops, Tx must be check again after flushed Rx.
		 * this is just a workaround, after this round of Tx, data is in Rx and still not handled.
		 * the right solution should be check Rx processing result and run Tx&Rx again.
		 */
		for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) { 
			md_ut_txq_process(&md_ctrl->txq[i]);
		}
	}

	return 0;
}

static int md_ut_broadcast_state(struct ccci_modem *md, MD_STATE state)
{
	int i;
	struct ccci_port *port;

	if(md->md_state == state)
		return 1;
	
	md->md_state = state;
	for(i=0;i<md->port_number;i++) {
		port = md->ports + i;
		if(port->ops->md_state_notice)
			port->ops->md_state_notice(port, state);
	}
	return 0;
}

static int md_ut_init(struct ccci_modem *md)
{
	int i;
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	struct ccci_port *port = NULL;

	CCCI_INF_MSG(md->index, TAG, "UT modem is initializing\n");
	// 1. init queue
	for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
		md_ut_queue_struct_init(&md_ctrl->txq[i], md, OUT, i, MAX_QUEUE_LENGTH, QUEUE_BUDGET);
	}
	for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
		md_ut_queue_struct_init(&md_ctrl->rxq[i], md, IN, i, MAX_QUEUE_LENGTH, QUEUE_BUDGET);
	}
	// 2. init port
	for(i=0; i<md->port_number; i++) {
		port = md->ports + i;
		ccci_port_struct_init(port, md);
		port->ops->init(port);
	}
	ccci_setup_channel_mapping(md);
	// 3. update state
	md->md_state = GATED;
	return 0;
}

static int md_ut_start(struct ccci_modem *md)
{
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	char img_err_str[IMG_ERR_STR_LEN];
	int ret;
	
	CCCI_INF_MSG(md->index, TAG, "UT modem is starting\n");
	// 1. load modem image
	if(md->config.setting&MD_SETTING_FIRST_BOOT || md->config.setting&MD_SETTING_RELOAD) {
		ret = ccci_load_firmware(md, IMG_MD, img_err_str);
		if(ret<0) {
			CCCI_ERR_MSG(md->index, TAG, "load firmware fail, %s\n", img_err_str);
		}
		md->config.setting &= ~MD_SETTING_RELOAD;
	}
	// 2. start thread
	md_ctrl->sched_thread = kthread_run(md_ut_sched_thread, md, "md_ut_sched%d", md->index);
	// 3. enable interrupt
	// 4. ungate modem or release reset pin
	ccci_set_mem_access_protection(md);
	// 5. update mutex
	atomic_set(&md_ctrl->reset_on_going, 0);
	// 6. start timer
	mod_timer(&md->bootup_timer, jiffies+5*HZ);
	mod_timer(&md_ctrl->rx_gen_timer, jiffies+RX_TIMER_INTVAL*HZ);
#ifdef STATISTIC
	mod_timer(&md_ctrl->statistic_timer, jiffies+STATISTIC_DUMP_INTERVAL*HZ);
#endif
	return 0;
}

static void md_ut_clear_queue(struct ccci_modem *md, DIRECTION dir)
{
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	unsigned long flags;
	int i;
	struct ccci_request *req = NULL;
	struct ccci_request *reqn;

	if(dir == OUT) {	
		for(i=0; i<QUEUE_LEN(md_ctrl->txq); i++) {
			spin_lock_irqsave(&md_ctrl->txq[i].req_lock, flags);
			list_for_each_entry_safe(req, reqn, &md_ctrl->txq[i].req_list, entry) {
				list_del(&req->entry);
				md_ctrl->txq[i].length--;
				ccci_free_req(req);
			}
			INIT_LIST_HEAD(&md_ctrl->txq[i].req_list);
			spin_unlock_irqrestore(&md_ctrl->txq[i].req_lock, flags);
		}
	} else if(dir == IN) {
		for(i=0; i<QUEUE_LEN(md_ctrl->rxq); i++) {
			spin_lock_irqsave(&md_ctrl->rxq[i].req_lock, flags);
			list_for_each_entry_safe(req, reqn, &md_ctrl->rxq[i].req_list, entry) {
				list_del(&req->entry);
				md_ctrl->rxq[i].length--;
				req->policy = RECYCLE;
				ccci_free_req(req);
			}
			INIT_LIST_HEAD(&md_ctrl->rxq[i].req_list);
			spin_unlock_irqrestore(&md_ctrl->rxq[i].req_lock, flags);
		}
	}
}

static int md_ut_stop(struct ccci_modem *md, unsigned int timeout)
{
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	
	// 1. stop thread
	kthread_stop(md_ctrl->sched_thread);
	// 2. purge queues
	md_ut_clear_queue(md, OUT);
	md_ut_clear_queue(md, IN);
	// 3. update state
	md->ops->broadcast_state(md, GATED);
	// 4. stop boot up timer
	/* 
	 * when encrypted phone boot up, before user input pin code, phone will let MD enter flight mode,
	 * and this may happen before MD finished boot up, so we need to stop boot up check timer to 
	 * avoid false alarm.
	 */
	del_timer(&md->bootup_timer);
	return 0;
}

static int md_ut_reset(struct ccci_modem *md)
{
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	
	// 1. mutex check
	if(atomic_inc_and_test(&md_ctrl->reset_on_going) > 1){
		CCCI_INF_MSG(md->index, TAG, "One reset flow is on-going\n");
		return -CCCI_ERR_MD_IN_RESET;
	}
	// 2. disable IRQ (use nosync)
	// 3. hold reset pin
	// 4. update state
	md->boot_stage = MD_BOOT_STAGE_0;
	md->ops->broadcast_state(md, RESET);
	return 0;
}

static int md_ut_write_room(struct ccci_modem *md, unsigned char qno)
{
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	if(qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	return md_ctrl->txq[qno].length_th - md_ctrl->txq[qno].length;
}

static int md_ut_send_request(struct ccci_modem *md, unsigned char qno, struct ccci_request* req)
{
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	int ret;
	unsigned long flags;
#ifdef STATISTIC
	md_ctrl->txq[qno].process_count++;
#endif
	
	CCCI_DBG_MSG(md->index, TAG, "get a Tx req on q%d\n", qno);
	if(qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	if(req->blocking) {
retry:
		spin_lock_irqsave(&md_ctrl->txq[qno].req_lock, flags);
		if(md_ctrl->txq[qno].length < md_ctrl->txq[qno].length_th) { // tx queue threshold check
			md_ctrl->txq[qno].length++;
#ifdef STATISTIC
			md_ctrl->txq[qno].data_count += req->skb->len;
#endif
			list_add_tail(&req->entry, &md_ctrl->txq[qno].req_list);
			/*
			 * it's NOT safe to access request after put it in queue, as sched_thread may 
			 * be wake and have processed this request.
			 */
			spin_unlock_irqrestore(&md_ctrl->txq[qno].req_lock, flags);
			kick_sched_thread(md_ctrl);
		} else {		
			spin_unlock_irqrestore(&md_ctrl->txq[qno].req_lock, flags);	
#ifdef STATISTIC
			md_ctrl->txq[qno].not_complet_count++;
#endif
			CCCI_DBG_MSG(md->index, TAG, "waiting on q%d\n", qno);	
			/*
			 * as port is multiplexed over queue, so queue does not know a request it sends belongs to which port.
			 * so we use wait_exclusive and wake_up_nr to make sure the first N events we wake up (as reqeusts send out) 
			 * are exactly the first N ports who are waiting for them.
			 */
			ret = wait_event_interruptible_exclusive(md_ctrl->txq[qno].req_wq, (md_ctrl->txq[qno].length<md_ctrl->txq[qno].length_th));
			if(ret == -ERESTARTSYS) {
				return -EINTR;
			}
			goto retry;
		}
	} else {
		spin_lock_irqsave(&md_ctrl->txq[qno].req_lock, flags);
		if(md_ctrl->txq[qno].length < md_ctrl->txq[qno].length_th) {
			md_ctrl->txq[qno].length++;
			list_add_tail(&req->entry, &md_ctrl->txq[qno].req_list);
			spin_unlock_irqrestore(&md_ctrl->txq[qno].req_lock, flags);
			kick_sched_thread(md_ctrl);
		} else {
			spin_unlock_irqrestore(&md_ctrl->txq[qno].req_lock, flags);
			CCCI_DBG_MSG(md->index, TAG, "busy on q%d\n", qno);
			// modem driver does NOT drop Tx packet, as network will handle this by its own
			return -EBUSY;
		}
	}
	return 0;
}

static int md_ut_give_more(struct ccci_modem *md, unsigned char qno)
{
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	if(qno == 0xFF)
		return -CCCI_ERR_INVALID_QUEUE_INDEX;
	/*
	 * as port may refuse request provided by queue, so port must notify queue when it's ready
	 * to receive more requests.
	 * 
	 * e.x. RX interrupt triggered a Rx queue processing routine, but port refused all requests. 
	 * then as RX interrupt won't come again, so it's port's duty to let queue know that it's time
	 * to run Rx routine again.
	 */
#ifdef LOOP_BACK
	if(md_ctrl->txq[qno].length >= md_ctrl->txq[qno].length_th) {
		// if this condition is true, Tx flow is waiting for available buffer slot in Tx queue's list.
		CCCI_DBG_MSG(md->index, TAG, "ask more on q%d\n", qno);
		kick_sched_thread(md_ctrl);
	}
#endif
	return 0;
}

static struct ccci_port* md_ut_get_port_by_minor(struct ccci_modem *md, int minor)
{
	int i;
	struct ccci_port *port;
	
	for(i=0; i<md->port_number; i++) {
		port = md->ports + i;
		if(port->minor == minor)
			return port;
	}
	return NULL;
}


static struct ccci_port* md_ut_get_port_by_channel(struct ccci_modem *md, CCCI_CH ch)
{
	int i;
	struct ccci_port *port;
	
	for(i=0; i<md->port_number; i++) {
		port = md->ports + i;
		if(port->rx_ch == ch || port->tx_ch == ch)
			return port;
	}
	return NULL;
}

static int md_ut_send_runtime_data(struct ccci_modem *md, unsigned int sbp_code)
{
	CCCI_INF_MSG(md->index, TAG, "SMEM Exception start=0x%x, size=0x%x\n",
		md->smem_layout.ccci_exp_smem_base_phy - md->mem_layout.smem_offset_AP_to_MD,
		md->smem_layout.ccci_exp_dump_size);
	ccci_set_ap_region_protection(md);
	mod_timer(&md->bootup_timer, jiffies+10*HZ);
	return 0;
}

static int md_ut_force_assert(struct ccci_modem *md, MD_COMM_TYPE type))
{
	struct ccci_request *req = NULL;
	struct ccci_header *ccci_h;

	req = ccci_alloc_req(OUT, sizeof(struct ccci_header), 1, 0);
	if(req) {
		req->policy = RECYCLE;
		ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
		ccci_h->data[0] = 0xFFFFFFFF;
		ccci_h->data[1] = 0x5A5A5A5A;
		ccci_h->channel = CCCI_FORCE_ASSERT_CH;
		ccci_h->reserved = 0xA5A5A5A5;
		return md->ops->send_request(md, 0, req);
	}
	return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
}

static int md_ut_dump_info(struct ccci_modem *md, MODEM_DUMP_FLAG flag, void *buff, int length)
{
	return 0;
}

static struct ccci_modem_ops md_ut_ops = {
	.init = &md_ut_init,
	.start = &md_ut_start,
	.stop = &md_ut_stop,
	.reset = &md_ut_reset,
	.send_request = &md_ut_send_request,
	.give_more = &md_ut_give_more,
	.write_room = &md_ut_write_room,
	.send_runtime_data = &md_ut_send_runtime_data,
	.broadcast_state = &md_ut_broadcast_state,
	.force_assert = &md_ut_force_assert,
	.dump_info = &md_cd_dump_info,
	.get_port_by_minor = &md_ut_get_port_by_minor,
	.get_port_by_channel = &md_ut_get_port_by_channel,
};

static irqreturn_t md_ut_wdt_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	
	// 1. clear MD register
	// 2. reset
	md->ops->reset(md);
	// 4. wakelock
	wake_lock_timeout(&md_ctrl->trm_wake_lock, 10*HZ);
	// 5. send message
	ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_RESET, 0);
	return IRQ_HANDLED;
}

static void md_ut_exception(struct ccci_modem *md, HIF_EX_STAGE stage)
{
	switch(stage) {
	case HIF_EX_INIT:
		ccci_md_exception_notify(md, EX_INIT);
		// mask Rx interrupt
		// purge Tx queue
		md_ut_clear_queue(md, OUT);
		// Rx dispatch does NOT depend on queue index in port structure, so it still can find right port.
		break;
	default:
		break;
	};
}

static irqreturn_t md_ut_exception_isr(int irq, void *data)
{
	struct ccci_modem *md = (struct ccci_modem *)data;
	
	CCCI_INF_MSG(md->index, TAG, "MD exception IRQ!\n");
	md_ut_exception(md, HIF_EX_INIT);
	return IRQ_HANDLED;
}

static ssize_t md_ut_parameter_show(struct ccci_modem *md, char *buf)
{	
	struct md_cd_ctrl *md_ctrl = (struct md_cd_ctrl *)md->private_data;
	int count = 0;
	
	count += snprintf(buf, 128, "QUEUE_BUDGET=%d\n", QUEUE_BUDGET);
	count += snprintf(buf+count, 128, "MAX_QUEUE_LENGTH=%d\n", MAX_QUEUE_LENGTH);
	return count;
}

static ssize_t md_ut_parameter_store(struct ccci_modem *md, const char *buf, size_t count)
{
	char id = *buf-'0';
	struct md_ut_ctrl *md_ctrl = (struct md_ut_ctrl *)md->private_data;
	struct ccci_request *req = NULL;
	struct ccci_header *ccci_h;
	unsigned int *int_ptr;
	
	CCCI_INF_MSG(md->index, TAG, "generate message %d\n", id);
	switch(id) {
	case 0:
		md_ut_exception_isr(0, md);
		break;
	case 1: // MD_EX
		req = ccci_alloc_req(IN, 1500, 1, 0);
		ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
		ccci_h->data[0] = CCCI_MAGIC_NUM;
		ccci_h->data[1] = MD_EX;
		ccci_h->channel = CCCI_CONTROL_RX;
		ccci_h->reserved = MD_EX_CHK_ID;
		break;
	case 2: // MD_EX_REC_OK
		req = ccci_alloc_req(IN, 1500, 1, 0);
		ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
		ccci_h->data[0] = CCCI_MAGIC_NUM;
		ccci_h->data[1] = MD_EX_REC_OK;
		ccci_h->channel = CCCI_CONTROL_RX;
		ccci_h->reserved = MD_EX_REC_OK_CHK_ID;
		memset(skb_put(req->skb, sizeof(EX_LOG_T)), 0, sizeof(EX_LOG_T));
		break;
	default:
		break;
	};
	md_ut_inject_packet(md, req, 0);
	return count;
}

CCCI_MD_ATTR(NULL, parameter, 0660, md_ut_parameter_show, md_ut_parameter_store);

static void md_ut_sysfs_init(struct ccci_modem *md)
{
	int ret;
	ccci_md_attr_parameter.modem = md;
	ret = sysfs_create_file(&md->kobj, &ccci_md_attr_parameter.attr);
	if(ret)
		CCCI_ERR_MSG(md->index, TAG, "fail to add sysfs node %s %d\n", ccci_md_attr_parameter.attr.name, ret);
}

static void md_ut_modem_setup(struct ccci_modem *md)
{
	struct md_ut_ctrl *md_ctrl;
	static char wakelock_name[16]; // must use static, as wakelock won't make hardcopy
	// init modem structure
	md->index = MD_SYS1;
	md->major = MD_UT_MAJOR;
	md->ops = &md_ut_ops;
	md->ports = md_ut_ports_normal;
	md->port_number = ARRAY_SIZE(md_ut_ports_normal);
	md->bootup_timer.function = md_bootup_timeout_func;
	md->bootup_timer.data = (unsigned long)md;
	md->ex_monitor.function = md_ex_monitor_func;
	md->ex_monitor.data = (unsigned long)md;
	md->config.setting |= MD_SETTING_DUMMY;	
	// init modem private data
	md_ctrl = (struct md_ut_ctrl *)md->private_data;
	init_waitqueue_head(&md_ctrl->sched_thread_wq);
	md_ctrl->sched_thread_kick = 0;
	snprintf(wakelock_name, sizeof(wakelock_name), "ccci%d_trm", md->index);
	wake_lock_init(&md_ctrl->trm_wake_lock, WAKE_LOCK_SUSPEND, wakelock_name);
	init_timer(&md_ctrl->rx_gen_timer);
	md_ctrl->rx_gen_timer.function = md_ut_rx_gen_func;
	md_ctrl->rx_gen_timer.data = (unsigned long)md;
#ifdef STATISTIC
	init_timer(&md_ctrl->statistic_timer);
	md_ctrl->statistic_timer.function = md_ut_statistic_timer_func;
	md_ctrl->statistic_timer.data = (unsigned long)md;
#endif
}

static int ccci_modem_probe(struct platform_device *dev)
{
	struct ccci_modem *md_ut;

	CCCI_INF_MSG(-1, TAG, "modem UT module init\n");
	md_ut = ccci_allocate_modem(sizeof(struct md_ut_ctrl));
	if(!md_ut)
		return 0;
	// register modem
	ccci_register_modem(md_ut);
	md_ut_sysfs_init(md_ut);

	return 0;
}

static int ccci_modem_remove(struct platform_device *dev)
{
	return 0;
}

static void ccci_modem_shutdown(struct platform_device *dev)
{
}

static int ccci_modem_suspend(struct platform_device *dev, pm_message_t state)
{
	return 0;
}

static int ccci_modem_resume(struct platform_device *dev)
{
	return 0;
}

static int ccci_modem_pm_suspend(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return ccci_modem_suspend(pdev, PMSG_SUSPEND);
}

static int ccci_modem_pm_resume(struct device *device)
{
    struct platform_device *pdev = to_platform_device(device);
    BUG_ON(pdev == NULL);

    return ccci_modem_resume(pdev);
}

static int ccci_modem_pm_restore_noirq(struct device *device)
{
	// IPO-H
	struct ccci_modem *md = (struct ccci_modem *)device->platform_data;
	// set flag for next md_start
	md->config.setting |= MD_SETTING_RELOAD;
    return 0;
}

static struct dev_pm_ops ccci_modem_pm_ops = {
    .suspend = ccci_modem_pm_suspend,
    .resume = ccci_modem_pm_resume,
    .freeze = ccci_modem_pm_suspend,
    .thaw = ccci_modem_pm_resume,
    .poweroff = ccci_modem_pm_suspend,
    .restore = ccci_modem_pm_resume,
    .restore_noirq = ccci_modem_pm_restore_noirq,
};

static struct platform_driver ccci_modem_driver =
{
	.driver = {
		.name = "ut_modem",
#ifdef CONFIG_PM
		.pm = &ccci_modem_pm_ops,
#endif
	},
	.probe = ccci_modem_probe,
	.remove = ccci_modem_remove,
	.shutdown = ccci_modem_shutdown,
	.suspend = ccci_modem_suspend,
	.resume = ccci_modem_resume,
};

static struct platform_device ccci_ut_device = {
	.name = "ut_modem",
	.id = -1,
};

static int __init modem_ut_init(void)
{
	int ret;
	ret = platform_driver_register(&ccci_modem_driver);
	if (ret) {
		CCCI_ERR_MSG(-1, TAG, "ut modem platform driver register fail(%d)\n", ret);
		return ret;
	}

	ret = platform_device_register(&ccci_ut_device);
	if (ret) {
		CCCI_ERR_MSG(-1, TAG, "ut modem platform device register fail(%d)\n", ret);
		return ret;
	}

	return 0;
}

module_init(modem_ut_init);

MODULE_AUTHOR("Xiao Wang <xiao.wang@mediatek.com>");
MODULE_DESCRIPTION("Loopback modem driver v0.1");
MODULE_LICENSE("GPL");
