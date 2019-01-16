#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <mach/ccci_config.h>
#include <mach/mt_ccci_common.h>
#include <mach/mt_boot.h>
#if CONFIG_COMPAT
#include <asm/compat.h> 
#endif
#include "ccci_core.h"
#include "ccci_support.h"
#include "ccci_bm.h"
#include "port_ipc.h"

#ifdef FEATURE_GET_MD_BAT_VOL // must be after ccci_config.h
#include <mach/battery_common.h>
#else
#define BAT_Get_Battery_Voltage(polling_mode)    ({ 0; })
#endif

#define MAX_QUEUE_LENGTH 32

static void dev_char_open_check(struct ccci_port *port)
{
    if(port->rx_ch==CCCI_FS_RX) // ccci_fsd
		port->modem->critical_user_active[0] = 1;
    if(port->rx_ch==CCCI_UART2_RX) // muxd
		port->modem->critical_user_active[1] = 1;
    if(port->rx_ch==CCCI_MD_LOG_RX) // mdlogger or meta
		port->modem->critical_user_active[2] = 1;
    if(port->rx_ch==CCCI_UART1_RX) // mdlogger
		port->modem->critical_user_active[3] = 1;
}

static int dev_char_close_check(struct ccci_port *port)
{
    if(port->rx_ch==CCCI_FS_RX && !atomic_read(&port->usage_cnt))
		port->modem->critical_user_active[0] = 0;
    if(port->rx_ch==CCCI_UART2_RX && !atomic_read(&port->usage_cnt))
		port->modem->critical_user_active[1] = 0;
    if(port->rx_ch==CCCI_MD_LOG_RX && !atomic_read(&port->usage_cnt))
		port->modem->critical_user_active[2] = 0;
    if(port->rx_ch==CCCI_UART1_RX && !atomic_read(&port->usage_cnt))
		port->modem->critical_user_active[3] = 0;
    
	if(port->modem->critical_user_active[0]==0 &&
		port->modem->critical_user_active[1]==0) {
         if (is_meta_mode() || is_advanced_meta_mode()) {
		 	if(port->modem->critical_user_active[3]==0) {
				CCCI_INF_MSG(port->modem->index, CHAR, "ready to reset MD in META mode\n");
                return 0;
             } else {
                 // this should never happen
		 		CCCI_ERR_MSG(port->modem->index, CHAR, "DHL ctrl is still open in META mode\n");
             }
         } else {
		 	if(port->modem->critical_user_active[2]==0 &&
				port->modem->critical_user_active[3]==0) {
				CCCI_INF_MSG(port->modem->index, CHAR, "ready to reset MD in normal mode\n");
                return 0;
             }
         }
    }
    return 1;
}

static int dev_char_open(struct inode *inode, struct file *file)
{
    int major = imajor(inode);
    int minor = iminor(inode);
    struct ccci_port *port;
    
    port = ccci_get_port_for_node(major, minor);
    if(atomic_read(&port->usage_cnt))
        return -EBUSY;
    CCCI_INF_MSG(port->modem->index, CHAR, "port %s open with flag %X by %s\n", port->name, file->f_flags, current->comm);
    atomic_inc(&port->usage_cnt);
    file->private_data = port;
    nonseekable_open(inode,file);
    dev_char_open_check(port);
#ifdef FEATURE_POLL_MD_EN    
    if(port->rx_ch==CCCI_MD_LOG_RX && port->modem->md_state==READY)    
        mod_timer(&port->modem->md_status_poller, jiffies+10*HZ);
#endif
    return 0;
}

static int dev_char_close(struct inode *inode, struct file *file)
{
    struct ccci_port *port = file->private_data;
    struct ccci_request *req = NULL;
    struct ccci_request *reqn;
    unsigned long flags;

    // 0. decrease usage count, so when we ask more, the packet can be dropped in recv_request
    atomic_dec(&port->usage_cnt);
    // 1. purge Rx request list
    spin_lock_irqsave(&port->rx_req_lock, flags);
    list_for_each_entry_safe(req, reqn, &port->rx_req_list, entry) {
        // 1.1. remove from list
        list_del(&req->entry);
        port->rx_length--;
        // 1.2. free it
        req->policy = RECYCLE;
        ccci_free_req(req);
    }
    // 1.3 flush Rx
    ccci_port_ask_more_request(port);
    spin_unlock_irqrestore(&port->rx_req_lock, flags);
    CCCI_INF_MSG(port->modem->index, CHAR, "port %s close rx_len=%d empty=%d\n", port->name, 
        port->rx_length, list_empty(&port->rx_req_list));
    // 2. check critical nodes for reset, run close check first, as mdlogger is killed before we gated MD when IPO shutdown
    if(dev_char_close_check(port)==0 && port->modem->md_state==GATED)
        ccci_send_virtual_md_msg(port->modem, CCCI_MONITOR_CH, CCCI_MD_MSG_READY_TO_RESET, 0);
#ifdef FEATURE_POLL_MD_EN    
    if(port->rx_ch == CCCI_MD_LOG_RX)    
        del_timer(&port->modem->md_status_poller);
#endif
    return 0;
}
static void port_ch_dump(int md_id, char* str,void* msg_buf, int len)
{
    #if 0
    unsigned char *char_ptr = (unsigned char *)msg_buf;
    char buf[100+1];
    int i;
    for(i=0;i<len && i<100;i++)
    {
        if(((32<=char_ptr[i]) && (char_ptr[i]<=126))
            ||(char_ptr[i]==0x09) //tbl
            ||(char_ptr[i]==0x0D)
            ||(char_ptr[i]==0x0A))
        {
            buf[i]=char_ptr[i];
        }
        else
        {
            buf[i]='#';
        }
    }
    buf[i]='\0';
    CCCI_INF_MSG(md_id,CHAR,"ch_dump:%s len=%d\n", str,len);
    CCCI_INF_MSG(md_id,CHAR,">%s\n",buf);
    #endif
}

static ssize_t dev_char_read(struct file *file, char *buf, size_t count, loff_t *ppos)
{
    struct ccci_port *port = file->private_data;
    struct ccci_request *req;
    struct ccci_header *ccci_h=NULL;
    int ret, read_len, full_req_done=0;
    unsigned long flags;

    // 1. get incomming request
    if(list_empty(&port->rx_req_list)) {
        if(!(file->f_flags & O_NONBLOCK)) {
            ret = wait_event_interruptible(port->rx_wq, !list_empty(&port->rx_req_list));    
            if(ret == -ERESTARTSYS) {
                ret = -EINTR;
                goto exit;
            }
        } else {
            ret = -EAGAIN;
            goto exit;
        }
    }
    CCCI_DBG_MSG(port->modem->index, CHAR, "read on %s for %zu\n", port->name, count);
    spin_lock_irqsave(&port->rx_req_lock, flags);
    req = list_first_entry(&port->rx_req_list, struct ccci_request, entry);
    // 2. caculate available data
    if(req->state != PARTIAL_READ) {
        ccci_h = (struct ccci_header *)req->skb->data;
        if(port->flags & PORT_F_USER_HEADER) { // header provide by user
            // CCCI_MON_CH should fall in here, as header must be send to md_init
            if(ccci_h->data[0] == CCCI_MAGIC_NUM) {
                read_len = sizeof(struct ccci_header);
                if(ccci_h->channel == CCCI_MONITOR_CH)
                    //ccci_h->channel = CCCI_MONITOR_CH_ID;
                    *(((u32 *)ccci_h)+2) = CCCI_MONITOR_CH_ID;
            } else {
                read_len = req->skb->len;
            }
        } else {
            // ATTENTION, if user does not provide header, it should NOT send empty packet.
            read_len = req->skb->len - sizeof(struct ccci_header);
            // remove CCCI header
            skb_pull(req->skb, sizeof(struct ccci_header));
        }
    } else {
        read_len = req->skb->len;
    }
    if(count>=read_len) {
        full_req_done = 1;
        list_del(&req->entry);
        /*
         * here we only ask for more request when rx list is empty. no need to be too gready, because
         * for most of the case, queue will not stop sending request to port.
         * actually we just need to ask by ourselves when we rejected requests before. these
         * rejected requests will staty in queue's buffer and may never get a chance to be handled again.
         */
        if(--(port->rx_length) == 0)
            ccci_port_ask_more_request(port);
        BUG_ON(port->rx_length<0);
    } else {
        req->state = PARTIAL_READ;
        read_len = count;
    }
    spin_unlock_irqrestore(&port->rx_req_lock, flags);
    if(ccci_h && ccci_h->channel == CCCI_UART2_RX)
    {
        port_ch_dump(port->modem->index,"chr_read", req->skb->data, read_len);
    }
    // 3. copy to user
    ret = copy_to_user(buf, req->skb->data, read_len);
    skb_pull(req->skb, read_len);
    //CCCI_DBG_MSG(port->modem->index, CHAR, "read done on %s l=%d r=%d pr=%d\n", port->name, read_len, ret, (req->state==PARTIAL_READ));
    // 4. free request
    if(full_req_done) {
        req->policy = RECYCLE; // Rx flow doesn't know the free policy until it reaches port (network and char are different)
#if 0
        if(port->rx_ch==CCCI_IPC_RX)
            port_ipc_rx_ack(port);
#endif
        ccci_free_req(req);
    }
exit:
    return ret?ret:read_len;
}

static ssize_t dev_char_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos)
{
    struct ccci_port *port = file->private_data;
    unsigned char blocking = !(file->f_flags&O_NONBLOCK);
    struct ccci_request *req = NULL;
    struct ccci_header *ccci_h = NULL;
    size_t actual_count = 0;
    int ret, header_len;

    if(port->tx_ch == CCCI_MONITOR_CH)
        return -EPERM;

    if (port->tx_ch == CCCI_IPC_UART_TX) {
        CCCI_INF_MSG(port->modem->index, CHAR, "port %s write: md_s=%d\n", port->name, port->modem->md_state);
    }
	
    if(port->modem->md_state==BOOTING && port->tx_ch!=CCCI_FS_TX && port->tx_ch!=CCCI_RPC_TX) {
        CCCI_INF_MSG(port->modem->index, CHAR, "port %s ch%d write fail when md_s=%d\n", port->name, port->tx_ch, port->modem->md_state);
        return -ENODEV;
    }
    if(port->modem->md_state==EXCEPTION && port->tx_ch!=CCCI_MD_LOG_TX && port->tx_ch!=CCCI_UART1_TX && port->tx_ch!=CCCI_FS_TX)
        return -ETXTBSY;
    if(port->modem->md_state==GATED || port->modem->md_state==RESET || port->modem->md_state==INVALID)
        return -ENODEV;
        
    header_len = sizeof(struct ccci_header)+(port->rx_ch==CCCI_FS_RX?sizeof(unsigned int):0); // ccci_fsd treat op_id as overhead
    if(port->flags & PORT_F_USER_HEADER) {
        if(count > (CCCI_MTU+header_len)) {
            CCCI_ERR_MSG(port->modem->index, CHAR, "reject packet(size=%zu ), larger than MTU on %s\n", count, port->name);
            return -ENOMEM;
        }
    }

    if(port->flags & PORT_F_USER_HEADER)
        actual_count = count>(CCCI_MTU+header_len)?(CCCI_MTU+header_len):count;
    else
        actual_count = count>CCCI_MTU?CCCI_MTU:count;
    CCCI_DBG_MSG(port->modem->index, CHAR, "write on %s for %zu of %zu\n", port->name, actual_count, count);
    
    req = ccci_alloc_req(OUT, actual_count, blocking, blocking);
    if(req) {
        // 1. for Tx packet, who issued it should know whether recycle it  or not
        req->policy = RECYCLE;
        // 2. prepare CCCI header, every member of header should be re-write as request may be re-used
        if(!(port->flags & PORT_F_USER_HEADER)) { 
            ccci_h = (struct ccci_header *)skb_put(req->skb, sizeof(struct ccci_header));
            ccci_h->data[0] = 0;
            ccci_h->data[1] = actual_count + sizeof(struct ccci_header);
            ccci_h->channel = port->tx_ch;
            ccci_h->reserved = 0;
        } else {
            ccci_h = (struct ccci_header *)req->skb->data;
        }
        // 3. get user data
        ret = copy_from_user(skb_put(req->skb, actual_count), buf, actual_count);
        if(ret)
            goto err_out;
        if(port->flags & PORT_F_USER_HEADER) { // header provided by user, valid after copy_from_user
            if(actual_count == sizeof(struct ccci_header))
                ccci_h->data[0] = CCCI_MAGIC_NUM;
            else
                ccci_h->data[1] = actual_count;
            ccci_h->channel = port->tx_ch; // as EEMCS VA will not fill this filed
        }
        if(port->rx_ch == CCCI_IPC_RX) {
            if((ret=port_ipc_write_check_id(port, req)) < 0)
                goto err_out;
            else
                ccci_h->reserved = ret; // Unity ID
        }
        if(ccci_h && ccci_h->channel == CCCI_UART2_TX)
        {
            port_ch_dump(port->modem->index,"chr_write", req->skb->data+sizeof(struct ccci_header), actual_count);
        }        
        // 4. send out
        ret = ccci_port_send_request(port, req); // do NOT reference request after called this, modem may have freed it, unless you get -EBUSY
        //if(port->tx_ch == CCCI_UART2_TX)
        //    CCCI_INF_MSG(port->modem->index, CHAR, "write done on %s, l=%zu r=%d\n", port->name, actual_count, ret);
        
        if(ret) {
            if(ret == -EBUSY && !req->blocking) {
                ret = -EAGAIN;
            }
            goto err_out;
        } else {
#if 0
            if(port->rx_ch==CCCI_IPC_RX)
                port_ipc_tx_wait(port);
#endif
        }
        return ret<0?ret:actual_count;
        
err_out:
        ccci_free_req(req);
        return ret;
    } else {
        // consider this case as non-blocking
        return -EBUSY;
    }
}

static int last_md_status[5];
static int md_status_show_count[5];
int scan_image_list(int md_id, char fmt[], int out_img_list[], int img_list_size);

static long dev_char_ioctl( struct file *file, unsigned int cmd, unsigned long arg)
{
    long state, ret = 0;
    struct ccci_setting *ccci_setting;
    struct ccci_port *port = file->private_data;
    struct ccci_modem *md = port->modem;
    int ch = port->rx_ch; // use Rx channel number to distinguish a port
    unsigned int sim_mode, sim_switch_type, enable_sim_type, sim_id, bat_info;
    unsigned int traffic_control = 0;
    unsigned int sim_slot_cfg[4];
	unsigned int tmp_md_img_list[MAX_IMG_NUM]; // for META
	int scaned_num;
	struct siginfo sig_info;
	unsigned int sig_pid;

    switch (cmd) {
		case CCCI_IOC_GET_MD_PROTOCOL_TYPE:
			{
				char md_protol[] = "DHL";
				unsigned int data_size = sizeof(md_protol) / sizeof(char);

				CCCI_ERR_MSG(md->index, CHAR, "Call CCCI_IOC_GET_MD_PROTOCOL_TYPE!\n");


				if (copy_to_user((void __user *)arg, md_protol, data_size)) 
				{
					CCCI_ERR_MSG(md->index, CHAR, "copy_to_user MD_PROTOCOL failed !!\n");

					return -EFAULT;
				}

				break;
			}
    case CCCI_IOC_GET_MD_STATE:
        state = md->boot_stage;
		if(state != last_md_status[md->index]) {
			last_md_status[md->index] = state;
			md_status_show_count[md->index] = 0;
		} else {
			if(md_status_show_count[md->index] < 100)
				md_status_show_count[md->index]++;
			else
				md_status_show_count[md->index]=0;
		}

		if(md_status_show_count[md->index] == 0) {
			CCCI_INF_MSG(md->index, CHAR, "MD state %ld\n", state);
			md_status_show_count[md->index]++;
		}
		
        if(state >= 0) { 
			//CCCI_DBG_MSG(md->index, CHAR, "MD state %ld\n", state);
			//state+='0'; // convert number to charactor
            ret = put_user((unsigned int)state, (unsigned int __user *)arg);
        } else {
            CCCI_ERR_MSG(md->index, CHAR, "Get MD state fail: %ld\n", state);
            ret = state;
        }
        break;
    case CCCI_IOC_PCM_BASE_ADDR:
    case CCCI_IOC_PCM_LEN:
    case CCCI_IOC_ALLOC_MD_LOG_MEM:
        // deprecated, share memory operation
        break;
    case CCCI_IOC_MD_RESET:
        CCCI_INF_MSG(md->index, CHAR, "MD reset ioctl(%d) called by %s\n", ch, current->comm);
        ret = md->ops->reset(md);
        if(ret==0)
            ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_RESET, 0);
        break;
    case CCCI_IOC_FORCE_MD_ASSERT:
        CCCI_INF_MSG(md->index, CHAR, "Force MD assert ioctl(%d) called by %s\n", ch, current->comm);
        ret = md->ops->force_assert(md, CCCI_MESSAGE);
        break;
    case CCCI_IOC_SEND_RUN_TIME_DATA:
        if(ch == CCCI_MONITOR_CH) {
			ret = md->ops->send_runtime_data(md, md->sbp_code);
        } else {
            CCCI_INF_MSG(md->index, CHAR, "Set runtime by invalid user(%u) called by %s\n", ch, current->comm);
            ret = -1;
        }
        break;
    case CCCI_IOC_GET_MD_INFO:
        state = md->img_info[IMG_MD].img_info.version;
        ret = put_user((unsigned int)state, (unsigned int __user *)arg);
        break;
    case CCCI_IOC_GET_MD_EX_TYPE:
        ret = put_user((unsigned int)md->ex_type, (unsigned int __user *)arg);
        CCCI_INF_MSG(md->index, CHAR, "get modem exception type=%d ret=%ld\n", md->ex_type, ret);
        break;
    case CCCI_IOC_SEND_STOP_MD_REQUEST:
        CCCI_INF_MSG(md->index, CHAR, "stop MD request ioctl called by %s\n", current->comm);
        ret = md->ops->reset(md);
        if(ret == 0) {
            md->ops->stop(md, 0);
            ret = ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_STOP_MD_REQUEST, 0);
        }
        break;
    case CCCI_IOC_SEND_START_MD_REQUEST:
        CCCI_INF_MSG(md->index, CHAR, "start MD request ioctl called by %s\n", current->comm);
        ret = ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_START_MD_REQUEST, 0);
        break;
    case CCCI_IOC_DO_START_MD:
        CCCI_INF_MSG(md->index, CHAR, "start MD ioctl called by %s\n", current->comm);
        ret = md->ops->start(md);
        break;
    case CCCI_IOC_DO_STOP_MD:
        CCCI_INF_MSG(md->index, CHAR, "stop MD ioctl called by %s\n", current->comm);
        ret = md->ops->stop(md, 0);
        break;
    case CCCI_IOC_ENTER_DEEP_FLIGHT:
        CCCI_INF_MSG(md->index, CHAR, "enter MD flight mode ioctl called by %s\n", current->comm);
        ret = md->ops->reset(md);
        if(ret==0) {
            md->ops->stop(md, 1000);
            ret = ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_ENTER_FLIGHT_MODE, 0);
        }
        break;
    case CCCI_IOC_LEAVE_DEEP_FLIGHT:
        CCCI_INF_MSG(md->index, CHAR, "leave MD flight mode ioctl called by %s\n", current->comm);
        ret = ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_LEAVE_FLIGHT_MODE, 0);
        break;

    case CCCI_IOC_POWER_ON_MD_REQUEST:
        CCCI_INF_MSG(md->index, CHAR, "Power on MD request ioctl called by %s\n", current->comm);
        ret = ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_POWER_ON_REQUEST, 0);
        break;

    case CCCI_IOC_POWER_OFF_MD_REQUEST:
        CCCI_INF_MSG(md->index, CHAR, "Power off MD request ioctl called by %s\n", current->comm);
        ret = ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_POWER_OFF_REQUEST, 0);
        break;
    case CCCI_IOC_POWER_ON_MD:
    case CCCI_IOC_POWER_OFF_MD:
        // abandoned
        CCCI_INF_MSG(md->index, CHAR, "Power on/off MD by user(%d) called by %s\n", ch, current->comm);
        ret = -1;
        break;
    case CCCI_IOC_SIM_SWITCH:
        if(copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
            CCCI_INF_MSG(md->index, CHAR, "IOC_SIM_SWITCH: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            switch_sim_mode(md->index, (char*)&sim_mode, sizeof(sim_mode));
            CCCI_INF_MSG(md->index, CHAR, "IOC_SIM_SWITCH(%x): %ld\n", sim_mode, ret);     
        }
        break;
    case CCCI_IOC_SIM_SWITCH_TYPE:
        sim_switch_type = get_sim_switch_type();
        ret = put_user(sim_switch_type, (unsigned int __user *)arg);
        break;
    case CCCI_IOC_GET_SIM_TYPE:
        if (md->sim_type == 0xEEEEEEEE)
            CCCI_ERR_MSG(md->index, KERN, "md has not send sim type yet(%x)", md->sim_type);
        ret = put_user(md->sim_type, (unsigned int __user *)arg);
        break;
    case CCCI_IOC_ENABLE_GET_SIM_TYPE:
        if(copy_from_user(&enable_sim_type, (void __user *)arg, sizeof(unsigned int))) {
            CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_ENABLE_GET_SIM_TYPE: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_SIM_TYPE, enable_sim_type, 1);
        }
        break;
    case CCCI_IOC_SEND_BATTERY_INFO:
        bat_info = (unsigned int)BAT_Get_Battery_Voltage(0);
        CCCI_INF_MSG(md->index, CHAR, "get bat voltage %d\n", bat_info);
        ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_GET_BATTERY_INFO, bat_info, 1);
        break;
    case CCCI_IOC_RELOAD_MD_TYPE:
        if(copy_from_user(&state, (void __user *)arg, sizeof(unsigned int))) {
            CCCI_INF_MSG(md->index, CHAR, "IOC_RELOAD_MD_TYPE: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            CCCI_INF_MSG(md->index, CHAR, "IOC_RELOAD_MD_TYPE: storing md type(%ld)!\n", state);
            ccci_reload_md_type(md, state);
        }
        break;
    case CCCI_IOC_SET_MD_IMG_EXIST:
        break;
    case CCCI_IOC_GET_MD_IMG_EXIST:
		memset(tmp_md_img_list, 0, sizeof(tmp_md_img_list));
		scaned_num = scan_image_list(md->index, "modem_%d_%s_n.img", tmp_md_img_list, MAX_IMG_NUM);

		if (copy_to_user((void __user *)arg, &tmp_md_img_list, sizeof(tmp_md_img_list))) {
			CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_GET_MD_IMG_EXIST: copy_to_user fail\n");
			ret= -EFAULT;
		}
        break;
    case CCCI_IOC_GET_MD_TYPE:
        state = md->config.load_type;
        ret = put_user((unsigned int)state, (unsigned int __user *)arg);
        break;
    case CCCI_IOC_STORE_MD_TYPE:
        if(copy_from_user(&md->config.load_type_saving, (void __user *)arg, sizeof(unsigned int))) {
            CCCI_INF_MSG(md->index, CHAR, "store md type fail: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            CCCI_INF_MSG(md->index, CHAR, "storing md type(%d) in kernel space!\n", md->config.load_type_saving);
            if (md->config.load_type_saving>=1 && md->config.load_type_saving<=MAX_IMG_NUM){
                if (md->config.load_type_saving != md->config.load_type)
                    CCCI_INF_MSG(md->index, CHAR, "Maybe Wrong: md type storing not equal with current setting!(%d %d)\n", md->config.load_type_saving, md->config.load_type);
                //Notify md_init daemon to store md type in nvram
                ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_STORE_NVRAM_MD_TYPE, 0);
            }
            else {
                CCCI_INF_MSG(md->index, CHAR, "store md type fail: invalid md type(0x%x)\n", md->config.load_type_saving);
            }
        }
        break;
    case CCCI_IOC_GET_MD_TYPE_SAVING:
        ret = put_user(md->config.load_type_saving, (unsigned int __user *)arg);
        break;
    case CCCI_IPC_RESET_RECV:
    case CCCI_IPC_RESET_SEND:
    case CCCI_IPC_WAIT_MD_READY:
        ret = port_ipc_ioctl(port, cmd, arg);
        break;
    case CCCI_IOC_GET_EXT_MD_POST_FIX:
        if(copy_to_user((void __user *)arg, md->post_fix, IMG_POSTFIX_LEN)) {
            CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_GET_EXT_MD_POST_FIX: copy_to_user fail\n");
            ret= -EFAULT;
        }
        break;
    case CCCI_IOC_SEND_ICUSB_NOTIFY:
        if(copy_from_user(&sim_id, (void __user *)arg, sizeof(unsigned int))) {
            CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_SEND_ICUSB_NOTIFY: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            ret = ccci_send_msg_to_md(md, CCCI_SYSTEM_TX, MD_ICUSB_NOTIFY, sim_id, 1);
        }
        break;
    case CCCI_IOC_DL_TRAFFIC_CONTROL:
        if(copy_from_user(&traffic_control, (void __user *)arg, sizeof(unsigned int))) {
            CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_DL_TRAFFIC_CONTROL: copy_from_user fail\n");
        }
        if(traffic_control == 1) {
            // turn off downlink queue
        } else if(traffic_control == 0) {
            // turn on donwlink queue
        } else {
        }
        ret = 0;
        break;
    case CCCI_IOC_UPDATE_SIM_SLOT_CFG:
        if(copy_from_user(&sim_slot_cfg, (void __user *)arg, sizeof(sim_slot_cfg))) {
            CCCI_INF_MSG(md->index, CHAR,  "CCCI_IOC_UPDATE_SIM_SLOT_CFG: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            int need_update;
            sim_switch_type = get_sim_switch_type();
            CCCI_INF_MSG(md->index, CHAR,  "CCCI_IOC_UPDATE_SIM_SLOT_CFG get s0:%d s1:%d s2:%d s3:%d\n", 
                        sim_slot_cfg[0], sim_slot_cfg[1], sim_slot_cfg[2],sim_slot_cfg[3]);
            ccci_setting = ccci_get_common_setting(md->index);
            need_update = sim_slot_cfg[0];
            ccci_setting->sim_mode = sim_slot_cfg[1];
            ccci_setting->slot1_mode= sim_slot_cfg[2];
            ccci_setting->slot2_mode= sim_slot_cfg[3];
            sim_mode=((sim_switch_type<<16)|ccci_setting->sim_mode);
            CCCI_INF_MSG(md->index, CHAR,  "TODO: how to map slot1 mode & slot2_mode =>sim_mode\n");
            switch_sim_mode(md->index, (char*)&sim_mode, sizeof(sim_mode));
            ccci_send_virtual_md_msg(md, CCCI_MONITOR_CH, CCCI_MD_MSG_CFG_UPDATE,need_update);
            ret = 0;
        }
    break;
    case CCCI_IOC_STORE_SIM_MODE:
        if(copy_from_user(&sim_mode, (void __user *)arg, sizeof(unsigned int))) {
            CCCI_INF_MSG(md->index, CHAR, "store sim mode fail: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
            CCCI_INF_MSG(md->index, CHAR, "store sim mode(%x) in kernel space!\n", sim_mode);
            exec_ccci_kern_func_by_md_id(0, ID_STORE_SIM_SWITCH_MODE, (char *)&sim_mode, sizeof(unsigned int));
        }
        break;
    case CCCI_IOC_GET_SIM_MODE:        
        CCCI_INF_MSG(md->index, CHAR, "get sim mode ioctl called by %s\n", current->comm);
        exec_ccci_kern_func_by_md_id(0, ID_GET_SIM_SWITCH_MODE, (char *)&sim_mode, sizeof(unsigned int));
        ret = put_user(sim_mode, (unsigned int __user *)arg);
        break;
    case CCCI_IOC_GET_CFG_SETTING:
        ccci_setting = ccci_get_common_setting(md->index);
        if (copy_to_user((void __user *)arg, ccci_setting, sizeof(struct ccci_setting))) {
            CCCI_INF_MSG(md->index, CHAR,  "CCCI_IOC_GET_CFG_SETTING: copy_to_user fail\n");
            ret= -EFAULT;
        }
        break;

    case CCCI_IOC_GET_MD_SBP_CFG:
        if (!md->sbp_code_default) {
            unsigned char *sbp_custom_value=NULL;
            if(md->index==MD_SYS1){
            #if defined(CONFIG_MTK_MD_SBP_CUSTOM_VALUE)
            sbp_custom_value=CONFIG_MTK_MD_SBP_CUSTOM_VALUE;
            #else
            sbp_custom_value="";
            #endif
            }else if(md->index==MD_SYS2){
            #if defined(CONFIG_MTK_MD2_SBP_CUSTOM_VALUE)
            sbp_custom_value=CONFIG_MTK_MD2_SBP_CUSTOM_VALUE;
            #else
            sbp_custom_value="";
            #endif
            }                
            ret = kstrtouint(sbp_custom_value, 0, &md->sbp_code_default);
            if (!ret){
               CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_GET_MD_SBP_CFG: get config sbp code:%d!\n", md->sbp_code_default);
            } else {
               CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_GET_MD_SBP_CFG: get config sbp code fail! ret:%ld, Config val:%s\n"
                        , ret, sbp_custom_value);
            }
        } else {
            CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_GET_MD_SBP_CFG: config sbp code:%d!\n", md->sbp_code_default);
        }
        ret = put_user(md->sbp_code_default, (unsigned int __user *)arg);
        break;

    case CCCI_IOC_SET_MD_SBP_CFG:
		if(copy_from_user(&md->sbp_code, (void __user *)arg, sizeof(unsigned int))) {
            CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_SET_MD_SBP_CFG: copy_from_user fail!\n");
            ret = -EFAULT;
        } else {
			CCCI_INF_MSG(md->index, CHAR, "CCCI_IOC_SET_MD_SBP_CFG: set md sbp code:0x%x!\n", md->sbp_code);
        }
        break;

    case CCCI_IOC_SET_HEADER:
        port->flags |= PORT_F_USER_HEADER;
        break;
    case CCCI_IOC_CLR_HEADER:
        port->flags &= ~PORT_F_USER_HEADER;
        break;
	
	case CCCI_IOC_SEND_SIGNAL_TO_USER:
		if(copy_from_user(&sig_pid, (void __user *)arg, sizeof(unsigned int))) {
			CCCI_INF_MSG(md->index, CHAR, "signal to rild fail: copy_from_user fail!\n");
			ret = -EFAULT;
		} else {
			unsigned int sig = (sig_pid >> 16) & 0xFFFF;
			unsigned int pid = sig_pid & 0xFFFF;
			sig_info.si_signo = sig;
			sig_info.si_code = SI_KERNEL;
			sig_info.si_pid = current->pid;
			sig_info.si_uid = current->cred->uid;
			ret = kill_proc_info(SIGUSR2, &sig_info, pid);
			CCCI_INF_MSG(md->index, CHAR, "send signal %d to rild %d ret=%ld\n", sig, pid, ret);
		}
		break;

    default:
        ret = -ENOTTY;
        break;
    }
    return ret;
}
#if CONFIG_COMPAT
static long dev_char_compat_ioctl( struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct ccci_port *port = filp->private_data;
    struct ccci_modem *md = port->modem;
	if (!filp->f_op || !filp->f_op->unlocked_ioctl)
    {
        CCCI_ERR_MSG(md->index, CHAR, "dev_char_compat_ioctl(!filp->f_op || !filp->f_op->unlocked_ioctl)\n");
        return -ENOTTY;
    }
    switch(cmd)
    {
        case CCCI_IOC_PCM_BASE_ADDR:
        case CCCI_IOC_PCM_LEN:
        case CCCI_IOC_ALLOC_MD_LOG_MEM:
        case CCCI_IOC_FORCE_FD:
        case CCCI_IOC_AP_ENG_BUILD: 
        case CCCI_IOC_GET_MD_MEM_SIZE:
        {
             CCCI_ERR_MSG(md->index, CHAR, "dev_char_compat_ioctl deprecated cmd(%d)\n",cmd);
             return 0;
        }
        default:
        {
            return filp->f_op->unlocked_ioctl(filp, cmd,
               (unsigned long)compat_ptr(arg));
        }
    }
}
#endif
unsigned int dev_char_poll(struct file *fp, struct poll_table_struct *poll)
{
    struct ccci_port *port = fp->private_data;
    unsigned int mask = 0;
    
    CCCI_DBG_MSG(port->modem->index, CHAR, "poll on %s\n", port->name);
    if(port->rx_ch == CCCI_IPC_RX) {
        mask = port_ipc_poll(fp, poll);
    } else {
        poll_wait(fp, &port->rx_wq, poll);
        // TODO: lack of poll wait for Tx
        if(!list_empty(&port->rx_req_list))
            mask |= POLLIN|POLLRDNORM;
        if(port->modem->ops->write_room(port->modem, PORT_TXQ_INDEX(port)) > 0)
            mask |= POLLOUT|POLLWRNORM;
        if(port->rx_ch==CCCI_UART1_RX && 
            port->modem->md_state!=READY && port->modem->md_state!=EXCEPTION) {
            mask |= POLLERR; // notify MD logger to save its log before md_init kills it
            CCCI_INF_MSG(port->modem->index, CHAR, "poll error for MD logger at state %d,mask=%d\n", port->modem->md_state,mask);
        }
    }

    return mask;
}

static struct file_operations char_dev_fops = {
    .owner = THIS_MODULE,
    .open = &dev_char_open,
    .read = &dev_char_read,
    .write = &dev_char_write,
    .release = &dev_char_close,
    .unlocked_ioctl = &dev_char_ioctl,
#if CONFIG_COMPAT    
    .compat_ioctl = &dev_char_compat_ioctl,
#endif
    .poll = &dev_char_poll,
};

static int port_char_init(struct ccci_port *port)
{
    struct cdev *dev;
    int ret = 0;

    CCCI_DBG_MSG(port->modem->index, CHAR, "char port %s is initializing\n", port->name);
    dev = kmalloc(sizeof(struct cdev), GFP_KERNEL);
    cdev_init(dev, &char_dev_fops);
    dev->owner = THIS_MODULE;
    if(port->rx_ch==CCCI_IPC_RX)
        port_ipc_init(port); // this will change port->minor, call it before register device
    else    
        port->private_data = dev; // not using
    ret = cdev_add(dev, MKDEV(port->modem->major, port->modem->minor_base+port->minor), 1);
    ret = ccci_register_dev_node(port->name, port->modem->major, port->modem->minor_base+port->minor);
    port->rx_length_th = MAX_QUEUE_LENGTH;
    return ret;
}
extern int process_rpc_kernel_msg(struct ccci_port *port, struct ccci_request *req);
static int port_char_recv_req(struct ccci_port *port, struct ccci_request *req)
{
    unsigned long flags; // as we can not tell the context, use spin_lock_irqsafe for safe

    if(!atomic_read(&port->usage_cnt) && (port->rx_ch!=CCCI_UART2_RX && port->rx_ch!=CCCI_FS_RX && port->rx_ch!=CCCI_RPC_RX)) // to make sure muxd always can get +EIND128 message
        goto drop;
    if(port->rx_ch==CCCI_RPC_RX && process_rpc_kernel_msg(port, req)){
        return 0;
    }
    CCCI_DBG_MSG(port->modem->index, CHAR, "recv on %s, len=%d\n", port->name, port->rx_length);    
    spin_lock_irqsave(&port->rx_req_lock, flags);
    if(port->rx_length < port->rx_length_th) {
        port->flags &= ~PORT_F_RX_FULLED;
        port->rx_length++;
        list_del(&req->entry); // dequeue from queue's list
        list_add_tail(&req->entry, &port->rx_req_list);
        spin_unlock_irqrestore(&port->rx_req_lock, flags);
        wake_lock_timeout(&port->rx_wakelock, HZ);
        wake_up_all(&port->rx_wq);
        return 0;
    } else {
        port->flags |= PORT_F_RX_FULLED;
        spin_unlock_irqrestore(&port->rx_req_lock, flags);
        if((port->flags&PORT_F_ALLOW_DROP)/* || !(port->flags&PORT_F_RX_EXCLUSIVE)*/) {
            CCCI_INF_MSG(port->modem->index, CHAR, "port %s Rx full, drop packet\n", port->name);
            goto drop;
        } else {
            return -CCCI_ERR_PORT_RX_FULL;
        }
    }

drop:
    // drop this packet
    CCCI_DBG_MSG(port->modem->index, CHAR, "drop on %s, len=%d\n", port->name, port->rx_length);
    list_del(&req->entry); 
    req->policy = RECYCLE;
    ccci_free_req(req);
    return -CCCI_ERR_DROP_PACKET;
}

static int port_char_req_match(struct ccci_port *port, struct ccci_request *req)
{
    struct ccci_header *ccci_h = (struct ccci_header *)req->skb->data;
    if(ccci_h->channel == port->rx_ch) {
        if(unlikely(port->rx_ch == CCCI_IPC_RX)) {
            return port_ipc_req_match(port, req);
        }
        return 1;
    }
    return 0;
}

static void port_char_md_state_notice(struct ccci_port *port, MD_STATE state)
{
    if(unlikely(port->rx_ch == CCCI_IPC_RX))
        port_ipc_md_state_notice(port, state);
    if(port->rx_ch==CCCI_UART1_RX && state==GATED)
        wake_up_all(&port->rx_wq); // check poll function
}

struct ccci_port_ops char_port_ops = {
    .init = &port_char_init,
    .recv_request = &port_char_recv_req,
    .req_match = &port_char_req_match,
    .md_state_notice = &port_char_md_state_notice,
};

int ccci_subsys_char_init(struct ccci_modem *md)
{
    register_chrdev_region(MKDEV(md->major, md->minor_base), 120, CCCI_DEV_NAME); // as IPC minor starts from 100
	last_md_status[5] = -1;
    return 0;
}
