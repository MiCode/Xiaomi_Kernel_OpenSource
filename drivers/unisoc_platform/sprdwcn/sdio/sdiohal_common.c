// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Communications Inc.
 *
 * Filename : sdiohal_common.c
 * Abstract : This file is an implementation for wcn sdio hal function
 */
#include <linux/mmc/core.h>
#include <linux/mmc/host.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include "sdiohal.h"

void sdiohal_debug_point_show(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct sdiohal_xmit_debug_point *txp = p_data->sdcb.tx_list_push;
	struct sdiohal_xmit_debug_point *rxp = p_data->sdcb.rx_list_dispatch;
	int i = 0;

	pr_info("rx_irq_ns(%lld %lld), tx_sch_ns(%lld %lld).\n",
		timespec64_to_ns(&p_data->tm_begin_irq), timespec64_to_ns(&p_data->tm_end_irq),
		timespec64_to_ns(&p_data->tm_begin_sch), timespec64_to_ns(&p_data->tm_end_sch));

	pr_info("Last xmit_lock: Enter task(%s) caller: %ps, time=%llu\n",
		p_data->sdcb.op_enter_comm, p_data->sdcb.op_enter_builtin_addr[0],
		p_data->op_enter_ns);
	pr_info("Last xmit_lock: Leave task(%s) caller: %ps, time=%llu\n",
		p_data->sdcb.op_leave_comm, p_data->sdcb.op_leave_builtin_addr[0],
		p_data->op_leave_ns);

	if (p_data->op_enter_ns > p_data->op_leave_ns)
		pr_info("WARNING:Task(%s) holds xmit_lock!!!", p_data->sdcb.op_enter_comm);

	pr_info("SDIOHAL TX DEBUG POINT[%d]:\n", p_data->sdcb.tx_list_push_index - 1);
	for (i = 0; i < SDIO_DEBUG_POINT_NUM; i++) {
		pr_info("[%d]chn:%d, time=%llu, %p, %p, %d, %d%s", i, txp[i].channel,
			txp[i].cur_time, txp[i].head, txp[i].tail, txp[i].num, txp[i].tx_driect,
			(i == p_data->sdcb.tx_list_push_index - 1) ? "[LAST]":"");
	}

	pr_info("SDIOHAL RX DEBUG POINT[%d]:\n", p_data->sdcb.rx_list_dispatch_index - 1);
	for (i = 0; i < SDIO_DEBUG_POINT_NUM; i++) {
		pr_info("[%d]chn:%d, time=%llu, %p, %p, %d, %d%s", i, rxp[i].channel,
			rxp[i].cur_time, rxp[i].head, rxp[i].tail, rxp[i].num, rxp[i].tx_driect,
			(i == p_data->sdcb.rx_list_dispatch_index - 1) ? "[LAST]":"");
	}
}

void __sdiohal_debug_point_store(int channel, int num, struct mbuf_t *head,
	struct mbuf_t *tail, struct sdiohal_xmit_debug_point *point, bool tx_direct)
{
	point->channel = channel;
	point->head = head;
	point->tail = tail;
	point->num = num;
	point->cur_time = ktime_get_boot_fast_ns();
	point->tx_driect = tx_direct;
}

void sdiohal_debug_point_store(int type, int channel, int num, struct mbuf_t *head,
		struct mbuf_t *tail, bool tx_direct)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	int *index = NULL;
	struct sdiohal_xmit_debug_point *point = NULL;

	WARN(type <= TX_LIST_PUSH && channel >= SDIO_CHN_TX_NUM,
			"debug point check(1) %d,%d\n", type, channel);
	WARN(type >= RX_LIST_DISPATCH && channel < SDIO_CHN_TX_NUM,
			"debug point check(2) %d,%d\n", type, channel);
	WARN((channel >= SDIO_CHN_TX_NUM) && tx_direct,
			"debug point check(3) %d,%d\n", type, channel);

	switch (type) {
	case TX_LIST_PUSH:
		point = p_data->sdcb.tx_list_push;
		index = &p_data->sdcb.tx_list_push_index;
		break;
	case RX_LIST_DISPATCH:
		point =  p_data->sdcb.rx_list_dispatch;
		index = &p_data->sdcb.rx_list_dispatch_index;
		break;
	default:
		pr_err("unexpected!\n");
		break;
	};

	if (index != NULL && *index >= SDIO_DEBUG_POINT_NUM)
		*index = 0;

	__sdiohal_debug_point_store(channel, num, head, tail, &point[(*index)++], tx_direct);
}

void sdiohal_print_list_data(struct sdiohal_list_t *data_list,
			     const char *func, int loglevel)
{
	struct mbuf_t *node;
	int i;
	unsigned short print_len;
	char print_str[64];

	if (!data_list || !data_list->mbuf_head) {
		WARN_ON_ONCE(1);
		return;
	}

	sprintf(print_str, "%s list: ", func);
	node = data_list->mbuf_head;
	for (i = 0; i < data_list->node_num; i++, node = node->next) {
		if (!node)
			break;
		print_len = node->len + SDIO_PUB_HEADER_SIZE;
		sdiohal_pr_data(KERN_WARNING, print_str,
				DUMP_PREFIX_NONE, 16, 1, node->buf,
				(print_len < SDIOHAL_PRINTF_LEN ?
				print_len : SDIOHAL_PRINTF_LEN), true,
				loglevel);
	}
}

void sdiohal_print_mbuf_data(int channel, struct mbuf_t *head,
			     struct mbuf_t *tail, int num,
			     const char *func,
			     int loglevel)
{
	struct mbuf_t *node;
	int i;
	unsigned short print_len;
	char print_str[64];

	if (!head) {
		WARN_ON_ONCE(1);
		return;
	}

	sprintf(print_str, "%s mbuf: ", func);

	node = head;
	for (i = 0; i < num; i++, node = node->next) {
		if (!node)
			break;
		print_len = node->len + SDIO_PUB_HEADER_SIZE;
		sdiohal_pr_data(KERN_WARNING, print_str,
				DUMP_PREFIX_NONE, 16, 1, node->buf,
				(print_len < SDIOHAL_PRINTF_LEN ?
				print_len : SDIOHAL_PRINTF_LEN), true,
				loglevel);
	}
}

void sdiohal_list_check(struct sdiohal_list_t *data_list,
			const char *func, bool dir)
{
	struct mbuf_t *node;
	int i;

	if (!data_list || !data_list->mbuf_head) {
		WARN_ON_ONCE(1);
		return;
	}

	sdiohal_pr_list(SDIOHAL_LIST_LEVEL,
			"%s dir:%s data_list:%p node_num:%d,\n",
			func, dir ? "tx" : "rx", data_list,
			data_list->node_num);
	node = data_list->mbuf_head;
	for (i = 0; i < data_list->node_num; i++, node = node->next) {
		if (!node) {
			WARN_ON_ONCE(1);
			return;
		}
		sdiohal_pr_list(SDIOHAL_LIST_LEVEL, "%s node:%p buf:%p\n",
			func, node, node->buf);
	}

	if (node) {
		pr_err("%s node:%p buf:%p\n", func, node, node->buf);
		WARN_ON_ONCE(1);
	}
}

void sdiohal_mbuf_list_check(int channel, struct mbuf_t *head,
			     struct mbuf_t *tail, int num,
			     const char *func, bool dir, int loglevel)
{
	struct mbuf_t *node;
	int i;

	if (!head) {
		WARN_ON_ONCE(1);
		return;
	}

	sdiohal_pr_list(loglevel, "%s dir:%s chn:%d head:%p tail:%p num:%d\n",
			func, dir ? "tx" : "rx", channel, head, tail, num);
	node = head;
	for (i = 0; i < num; i++, node = node->next) {
		if (!node) {
			WARN_ON_ONCE(1);
			return;
		}
		sdiohal_pr_list(SDIOHAL_LIST_LEVEL, "%s node:%p buf:%p\n",
			func, node, node->buf);
	}

	if (node) {
		pr_err("%s node:%p buf:%p\n", func, node, node->buf);
		WARN_ON_ONCE(1);
	}
}

/* for list manger */
void sdiohal_atomic_add(int count, atomic_t *value)
{
	atomic_add(count, value);
}

void sdiohal_atomic_sub(int count, atomic_t *value)
{
	if (atomic_read(value) == 0)
		return;

	atomic_sub(count, value);
}

/* seam for thread */
void sdiohal_tx_down(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	wait_for_completion(&p_data->tx_completed);
}

void sdiohal_tx_up(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	complete(&p_data->tx_completed);
}

void sdiohal_rx_down(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	wait_for_completion(&p_data->rx_completed);
}

void sdiohal_rx_up(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	complete(&p_data->rx_completed);
}

static void sdiohal_completion_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	init_completion(&p_data->tx_completed);
	init_completion(&p_data->rx_completed);
	init_completion(&p_data->scan_done);
}

void sdiohal_lock_tx_ws(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (atomic_read(&p_data->flag_suspending))
		return;

	sdiohal_atomic_add(1, &p_data->tx_wake_flag);
	if (atomic_read(&p_data->tx_wake_flag) > 1)
		return;

	__pm_stay_awake(p_data->tx_ws);
}

void sdiohal_unlock_tx_ws(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	sdiohal_atomic_sub(1, &p_data->tx_wake_flag);
	if (atomic_read(&p_data->tx_wake_flag))
		return;

	__pm_relax(p_data->tx_ws);
}

void sdiohal_lock_rx_ws(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (atomic_read(&p_data->flag_suspending) ||
		atomic_read(&p_data->rx_wake_flag))
		return;

	atomic_set(&p_data->rx_wake_flag, 1);
	__pm_stay_awake(p_data->rx_ws);
}

void sdiohal_unlock_rx_ws(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (!atomic_read(&p_data->rx_wake_flag))
		return;

	atomic_set(&p_data->rx_wake_flag, 0);
	__pm_relax(p_data->rx_ws);
}

void sdiohal_lock_scan_ws(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	__pm_stay_awake(p_data->scan_ws);
}

void sdiohal_unlock_scan_ws(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	__pm_relax(p_data->scan_ws);
}

static void sdiohal_wakelock_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	p_data->tx_ws = wakeup_source_create("sdiohal_tx_wakelock");
	wakeup_source_add(p_data->tx_ws);
	p_data->rx_ws = wakeup_source_create("sdiohal_rx_wakelock");
	wakeup_source_add(p_data->rx_ws);
	p_data->scan_ws = wakeup_source_create("sdiohal_scan_wakelock");
	wakeup_source_add(p_data->scan_ws);
}

static void sdiohal_wakelock_deinit(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	wakeup_source_remove(p_data->tx_ws);
	wakeup_source_destroy(p_data->tx_ws);
	wakeup_source_remove(p_data->rx_ws);
	wakeup_source_destroy(p_data->rx_ws);
	wakeup_source_remove(p_data->scan_ws);
	wakeup_source_destroy(p_data->scan_ws);
}

static void sdiohal_waitqueue_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	init_waitqueue_head(&p_data->resume_waitq);
}

/* for callback */
void sdiohal_callback_lock(struct mutex *callback_mutex)
{
	mutex_lock(callback_mutex);
}

void sdiohal_callback_unlock(struct mutex *callback_mutex)
{
	mutex_unlock(callback_mutex);
}

static void sdiohal_callback_lock_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mutex *chn_callback = p_data->callback_lock;
	int channel;

	for (channel = 0; channel < SDIO_CHANNEL_NUM; channel++)
		mutex_init(&chn_callback[channel]);
}

void sdiohal_callback_lock_deinit(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mutex *chn_callback = p_data->callback_lock;
	int channel;

	for (channel = 0; channel < SDIO_CHANNEL_NUM; channel++)
		mutex_destroy(&chn_callback[channel]);
}

void sdiohal_spinlock_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	spin_lock_init(&p_data->tx_spinlock);
	spin_lock_init(&p_data->rx_spinlock);
}

/* for sleep, WCN_SLP */
void sdiohal_cp_tx_sleep(enum slp_subsys subsys)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && !g_match_config->unisoc_wcn_slp)
		return;
	sdiohal_debug("%s subsys:%d count:%d\n",
		      __func__, subsys,
		      atomic_read(&p_data->tx_wake_cp_count[subsys]));

	sdiohal_atomic_sub(1, &p_data->tx_wake_cp_count[subsys]);
	if (atomic_read(&p_data->tx_wake_cp_count[subsys]))
		return;

	slp_mgr_drv_sleep(subsys, true);
}

void sdiohal_cp_tx_wakeup(enum slp_subsys subsys)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && !g_match_config->unisoc_wcn_slp)
		return;
	sdiohal_debug("%s subsys:%d count:%d\n",
		      __func__, subsys,
		      atomic_read(&p_data->tx_wake_cp_count[subsys]));

	sdiohal_atomic_add(1, &p_data->tx_wake_cp_count[subsys]);
	slp_mgr_drv_sleep(subsys, false);
	slp_mgr_wakeup(subsys);
}

void sdiohal_cp_rx_sleep(enum slp_subsys subsys)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && !g_match_config->unisoc_wcn_slp)
		return;
	sdiohal_debug("%s subsys:%d count:%d\n",
		      __func__, subsys,
		      atomic_read(&p_data->rx_wake_cp_count[subsys]));

	sdiohal_atomic_sub(1, &p_data->rx_wake_cp_count[subsys]);
	if (atomic_read(&p_data->rx_wake_cp_count[subsys]))
		return;

	slp_mgr_drv_sleep(subsys, true);
}

void sdiohal_cp_rx_wakeup(enum slp_subsys subsys)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct wcn_match_data *g_match_config = get_wcn_match_config();

	if (g_match_config && !g_match_config->unisoc_wcn_slp)
		return;
	sdiohal_debug("%s subsys:%d count:%d\n",
		      __func__, subsys,
		      atomic_read(&p_data->rx_wake_cp_count[subsys]));

	sdiohal_atomic_add(1, &p_data->rx_wake_cp_count[subsys]);
	slp_mgr_drv_sleep(subsys, false);
	slp_mgr_wakeup(subsys);
}

bool sdiohal_is_resumed(bool important)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (!important && unlikely(atomic_read(&p_data->flag_suspending))) {
		/* Except the data that must be sent during suspend */
		return false;
	}

	return !!atomic_read(&p_data->flag_resume);
}

void sdiohal_resume_check(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	long ret = -1;
	bool show = false;

	if (!atomic_read(&p_data->flag_resume)) {
		show = true;
		pr_err("Operate SDIO bus after suspend");
		dump_stack();
	}
	ret = wait_event_killable_timeout(p_data->resume_waitq, atomic_read(&p_data->flag_resume),
			msecs_to_jiffies(SDIOHAL_RESUME_TIMEOUT));
	WARN(!ret, "timeout(%lu s) for system resume", SDIOHAL_RESUME_TIMEOUT / MSEC_PER_SEC);

	if (show)
		pr_info("resume time:%ums\n", SDIOHAL_RESUME_TIMEOUT - jiffies_to_msecs(ret));
}

void sdiohal_op_enter(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	mutex_lock(&p_data->xmit_lock);
	p_data->op_enter_ns = ktime_get_boot_fast_ns();
	p_data->sdcb.op_enter_builtin_addr[0] = __builtin_return_address(0);
	memcpy(p_data->sdcb.op_enter_comm, current->comm, sizeof(p_data->sdcb.op_enter_comm));
}

void sdiohal_op_leave(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	u64 expire_time_ns = NSEC_PER_SEC;

	p_data->op_leave_ns = ktime_get_boot_fast_ns();
	if (unlikely(p_data->op_leave_ns - p_data->op_enter_ns > expire_time_ns)) {
		p_data->op_expire_cnt++;
		pr_err_ratelimited("%s %ps->%ps->%ps->%ps expire_%llums_cnt %llu(%llu, %llu).\n",
		current->comm, __builtin_return_address(3), __builtin_return_address(2),
		__builtin_return_address(1), __builtin_return_address(0),
		expire_time_ns/NSEC_PER_MSEC, p_data->op_expire_cnt,
		p_data->op_enter_ns, p_data->op_leave_ns);
	}
	p_data->sdcb.op_leave_builtin_addr[0] = __builtin_return_address(0);
	memcpy(p_data->sdcb.op_leave_comm, current->comm, sizeof(p_data->sdcb.op_leave_comm));
	mutex_unlock(&p_data->xmit_lock);
}

void sdiohal_sdma_enter(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	mutex_lock(&p_data->xmit_sdma);
}

void sdiohal_sdma_leave(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	mutex_unlock(&p_data->xmit_sdma);
}

static void sdiohal_mutex_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	p_data->op_enter_ns = 0;
	p_data->op_leave_ns = 0;
	p_data->op_expire_cnt = 0;
	mutex_init(&p_data->xmit_lock);
	mutex_init(&p_data->xmit_sdma);
}

static void sdiohal_mutex_deinit(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	mutex_destroy(&p_data->xmit_lock);
	mutex_destroy(&p_data->xmit_sdma);
}

static void sdiohal_sleep_flag_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	atomic_set(&p_data->flag_resume, 1);
}

void sdiohal_channel_to_hwtype(int inout, int channel,
			       unsigned int *type, unsigned int *subtype)
{
	if (!inout)
		channel -= SDIO_CHN_TX_NUM;
	*type = 0;
	*subtype = channel;

	sdiohal_debug("%s type:%d, subtype:%d\n", __func__, *type, *subtype);
}

int sdiohal_hwtype_to_channel(int inout, unsigned int type,
			      unsigned int subtype)
{
	int channel;

	if (inout)
		channel = subtype;
	else
		channel = subtype + SDIO_CHN_TX_NUM;

	sdiohal_debug("%s channel:%d,inout:%d\n", __func__, channel, inout);

	return channel;
}

bool sdiohal_is_tx_list_empty(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (atomic_read(&p_data->tx_mbuf_num) != 0)
		return false;

	return true;
}

int sdiohal_tx_packer(struct sdiohal_sendbuf_t *send_buf,
		      struct sdiohal_list_t *data_list,
		      struct mbuf_t *mbuf_node)
{
	if ((!send_buf) || (!data_list) || (!mbuf_node))
		return -EINVAL;

	memcpy(send_buf->buf + send_buf->used_len, mbuf_node->buf,
	       mbuf_node->len + sizeof(struct bus_puh_t));

	send_buf->used_len += sizeof(struct bus_puh_t) +
			      SDIOHAL_ALIGN_4BYTE(mbuf_node->len);

	return 0;
}

int sdiohal_tx_set_eof(struct sdiohal_sendbuf_t *send_buf,
		       unsigned char *eof_buf)
{
	if ((!send_buf) || (!eof_buf))
		return -EINVAL;

	memcpy((void *)(send_buf->buf + send_buf->used_len),
	       (void *)eof_buf, sizeof(struct bus_puh_t));
	send_buf->used_len += sizeof(struct bus_puh_t);

	return 0;
}

static int sdiohal_tx_fill_puh(int channel, struct mbuf_t *head,
			       struct mbuf_t *tail, int num)
{
	struct bus_puh_t *puh = NULL;
	struct mbuf_t *mbuf_node;
	unsigned int type, subtype;
	int inout = 1;
	int i;

	sdiohal_channel_to_hwtype(inout, channel, &type, &subtype);

	mbuf_node = head;
	for (i = 0; i < num; i++, mbuf_node = mbuf_node->next) {
		if (!mbuf_node) {
			pr_err("%s tx fill puh, mbuf ptr error:%p\n",
				__func__, mbuf_node);

			return -EFAULT;
		}
		puh = (struct bus_puh_t *)mbuf_node->buf;
		puh->type = type;
		puh->subtype = subtype;
		puh->len = mbuf_node->len;
		puh->eof = 0;
		puh->pad = 0;
	}

	return 0;
}

void sdiohal_tx_list_enq(int channel, struct mbuf_t *head,
			 struct mbuf_t *tail, int num)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	sdiohal_tx_fill_puh(channel, head, tail, num);

	spin_lock_bh(&p_data->tx_spinlock);
	if (atomic_read(&p_data->tx_mbuf_num) == 0)
		p_data->tx_list_head.mbuf_head = head;
	else
		p_data->tx_list_head.mbuf_tail->next = head;
	p_data->tx_list_head.mbuf_tail = tail;
	p_data->tx_list_head.mbuf_tail->next = NULL;
	p_data->tx_list_head.node_num += num;
	sdiohal_atomic_add(num, &p_data->tx_mbuf_num);
	spin_unlock_bh(&p_data->tx_spinlock);
}

void sdiohal_tx_find_data_list(struct sdiohal_list_t *data_list)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mbuf_t *mbuf_node;
	int num, i;

	spin_lock_bh(&p_data->tx_spinlock);
	num = p_data->tx_list_head.node_num;
	if (num > MAX_CHAIN_NODE_NUM)
		num = MAX_CHAIN_NODE_NUM;

	mbuf_node = p_data->tx_list_head.mbuf_head;
	for (i = 1; i < num; i++)
		mbuf_node = mbuf_node->next;

	data_list->mbuf_head = p_data->tx_list_head.mbuf_head;
	data_list->mbuf_tail = mbuf_node;
	data_list->node_num = num;

	p_data->tx_list_head.node_num -= num;
	sdiohal_atomic_sub(num, &p_data->tx_mbuf_num);
	if (atomic_read(&p_data->tx_mbuf_num) == 0) {
		p_data->tx_list_head.mbuf_head = NULL;
		p_data->tx_list_head.mbuf_tail = NULL;
	} else
		p_data->tx_list_head.mbuf_head = mbuf_node->next;
	data_list->mbuf_tail->next = NULL;
	spin_unlock_bh(&p_data->tx_spinlock);
	sdiohal_list_check(data_list, __func__, SDIOHAL_WRITE);
}

static int sdiohal_tx_pop_assignment(struct sdiohal_list_t *data_list)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct sdiohal_list_t *tx_list;
	struct bus_puh_t *puh;
	struct mbuf_t *mbuf_node, *mbuf_next;
	int inout = 1, channel;
	unsigned int node_num, i;

	sdiohal_list_check(data_list, __func__, SDIOHAL_WRITE);
	node_num = data_list->node_num;
	mbuf_next = data_list->mbuf_head;
	for (i = 0; i < node_num; i++) {
		mbuf_node = mbuf_next;
		if (!mbuf_node) {
			pr_err("%s tx pop mbuf ptr error:%p\n",
			       __func__, mbuf_node);

			return -EFAULT;
		}
		mbuf_next = mbuf_next->next;
		puh = (struct bus_puh_t *)mbuf_node->buf;
		channel = sdiohal_hwtype_to_channel(inout,
			  puh->type, puh->subtype);
		if (channel >= SDIO_CHN_TX_NUM) {
			pr_err("%s tx pop channel error:%d\n",
			       __func__, channel);
			continue;
		}

		tx_list = p_data->list_tx[channel];
		mbuf_node->next = NULL;
		if (tx_list->node_num == 0)
			tx_list->mbuf_head = mbuf_node;
		else
			tx_list->mbuf_tail->next = mbuf_node;

		tx_list->mbuf_tail = mbuf_node;
		tx_list->type = puh->type;
		tx_list->subtype = puh->subtype;
		tx_list->node_num++;
	}

	return 0;
}

int sdiohal_tx_list_denq(struct sdiohal_list_t *data_list)
{
	struct list_head *list_head, *pos;
	struct sdiohal_list_t *tx_list;
	int channel, inout = 1;
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mutex *chn_callback = p_data->callback_lock;
	struct mchn_ops_t *sdiohal_ops;

	struct timespec64 tm_begin, tm_end;
	static long time_total_ns;
	static int times_count;

	sdiohal_tx_pop_assignment(data_list);

	list_head = &p_data->list_tx[0]->head;
	for (pos = list_head; pos->next != list_head; pos = pos->next) {
		tx_list = (struct sdiohal_list_t *)list_entry(pos,
				struct sdiohal_list_t, head);
		if (tx_list->node_num == 0)
			continue;

		sdiohal_list_check(tx_list, __func__, SDIOHAL_WRITE);
		sdiohal_print_list_data(tx_list, __func__,
					SDIOHAL_NORMAL_LEVEL);

		channel = sdiohal_hwtype_to_channel(inout, tx_list->type,
						    tx_list->subtype);
		if (channel >= SDIO_CHN_TX_NUM) {
			pr_err("%s tx pop channel error:%d\n",
			       __func__, channel);
			continue;
		}

		ktime_get_real_ts64(&tm_begin);

		sdiohal_callback_lock(&chn_callback[channel]);
		sdiohal_ops = chn_ops(channel);
		sdiohal_mbuf_list_check(channel, tx_list->mbuf_head,
					tx_list->mbuf_tail,
					tx_list->node_num,
					__func__, SDIOHAL_WRITE,
					SDIOHAL_NORMAL_LEVEL);
		if (sdiohal_ops && sdiohal_ops->pop_link) {
			sdiohal_ops->pop_link(channel, tx_list->mbuf_head,
					      tx_list->mbuf_tail,
					      tx_list->node_num);
		} else
			pr_err("%s no tx ops channel:%d\n", __func__, channel);

		tx_list->node_num = 0;
		sdiohal_callback_unlock(&chn_callback[channel]);

		ktime_get_real_ts64(&tm_end);
		time_total_ns += timespec64_to_ns(&tm_end) -
				timespec64_to_ns(&tm_begin);
		times_count++;
		if (!(times_count % PERFORMANCE_COUNT)) {
			sdiohal_pr_perf("tx pop callback,avg time:%ld\n",
					(time_total_ns / PERFORMANCE_COUNT));
			time_total_ns = 0;
			times_count = 0;
		}
	}

	return 0;
}

int sdiohal_rx_list_dispatch(void)
{
	struct list_head *list_head, *pos;
	struct sdiohal_list_t *rx_list;
	int inout = 0, channel;
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mutex *chn_callback = p_data->callback_lock;
	struct mchn_ops_t *sdiohal_ops;

	struct timespec64 tm_begin, tm_end;
	static long time_total_ns;
	static int times_count;

	if (unlikely(p_data->flag_init != true))
		return -ENODEV;

	list_head = &p_data->list_rx[0]->head;
	for (pos = list_head; pos->next != list_head; pos = pos->next) {
		rx_list = (struct sdiohal_list_t *)list_entry(pos,
				struct sdiohal_list_t, head);
		if (rx_list->node_num == 0)
			continue;

		sdiohal_list_check(rx_list, __func__, SDIOHAL_READ);
		sdiohal_print_list_data(rx_list, __func__,
					SDIOHAL_NORMAL_LEVEL);

		channel = sdiohal_hwtype_to_channel(inout, rx_list->type,
						    rx_list->subtype);
		if (channel >= SDIO_CHANNEL_NUM) {
			pr_err("%s rx pop channel error:%d\n",
			       __func__, channel);
			continue;
		}

		ktime_get_real_ts64(&tm_begin);

		sdiohal_callback_lock(&chn_callback[channel]);
		sdiohal_ops = chn_ops(channel);
		sdiohal_mbuf_list_check(channel, rx_list->mbuf_head,
					rx_list->mbuf_tail,
					rx_list->node_num,
					__func__, SDIOHAL_READ,
					SDIOHAL_NORMAL_LEVEL);
		sdiohal_rx_list_dispatch_dp(channel, rx_list->mbuf_head,
				rx_list->mbuf_tail, rx_list->node_num);
		if (sdiohal_ops && sdiohal_ops->pop_link) {
			sdiohal_ops->pop_link(channel, rx_list->mbuf_head,
					      rx_list->mbuf_tail,
					      rx_list->node_num);
		} else {
			pr_err("%s no rx ops channel:%d\n",
			       __func__, channel);
			sdiohal_rx_list_free(rx_list->mbuf_head,
					     rx_list->mbuf_tail,
					     rx_list->node_num);
		}
		rx_list->node_num = 0;
		sdiohal_callback_unlock(&chn_callback[channel]);

		ktime_get_real_ts64(&tm_end);
		time_total_ns += timespec64_to_ns(&tm_end)
			- timespec64_to_ns(&tm_begin);
		times_count++;
		if (!(times_count % PERFORMANCE_COUNT)) {
			sdiohal_pr_perf("rx pop callback,avg time:%ld\n",
					(time_total_ns / PERFORMANCE_COUNT));
			time_total_ns = 0;
			times_count = 0;
		}
	}

	return 0;
}

struct sdiohal_list_t *sdiohal_get_rx_channel_list(int channel)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	if (unlikely(p_data->flag_init != true))  {
		pr_err("%s sdiohal not init\n", __func__);
		return NULL;
	}

	channel -= SDIO_CHN_TX_NUM;
	if (channel >= SDIO_CHN_RX_NUM) {
		pr_err("%s rx error channel:%d\n", __func__, channel);
		return NULL;
	}

	return p_data->list_rx[channel];
}

int sdiohal_rx_list_free(struct mbuf_t *mbuf_head,
			 struct mbuf_t *mbuf_tail, int num)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	void *data = NULL;
	struct mbuf_t *mbuf_node;
	int i;

	mbuf_node = mbuf_head;
	for (i = 0; i < num; i++, mbuf_node = mbuf_node->next) {
		if (mbuf_node->buf) {
			data = mbuf_node->buf;
			sdiohal_debug("%s, before put page addr:%p,count:%d\n",
				      __func__, virt_to_head_page(data),
			     atomic_read(&virt_to_head_page(data)->_refcount));
			put_page(virt_to_head_page(data));
			sdiohal_debug("%s, after put page addr:%p,count:%d\n",
				      __func__, virt_to_head_page(data),
			atomic_read(&virt_to_head_page(data)->_refcount));
			mbuf_node->buf = NULL;
		}
		mbuf_node->len = 0;
	}

	spin_lock_bh(&p_data->rx_spinlock);
	if (p_data->list_rx_buf.node_num == 0)
		p_data->list_rx_buf.mbuf_head = mbuf_head;
	else
		p_data->list_rx_buf.mbuf_tail->next = mbuf_head;
	p_data->list_rx_buf.mbuf_tail = mbuf_tail;
	p_data->list_rx_buf.node_num += num;
	spin_unlock_bh(&p_data->rx_spinlock);

	return 0;
}

static void *sdiohal_alloc_frag(unsigned int fragsz, gfp_t gfp_mask)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct sdiohal_frag_mg *frag_ctl;
	void *data;
	int order;
	unsigned long flags;

	local_irq_save(flags);
	frag_ctl = &p_data->frag_ctl;
	if (unlikely(!frag_ctl->frag.page)) {
refill:
#if (BITS_PER_LONG > 32) || (PAGE_SIZE >= 65536)
		order = SDIOHAL_FRAG_PAGE_MAX_ORDER;
#else
		order = SDIOHAL_FRAG_PAGE_MAX_ORDER_32_BIT;
#endif
		for (; ;) {
			gfp_t gfp = gfp_mask;

			if (order)
				gfp |= __GFP_COMP | __GFP_NOWARN;
			frag_ctl->frag.page = alloc_pages(gfp, order);
			if (likely(frag_ctl->frag.page))
				break;
			if (--order < 0)
				goto fail;
		}
		frag_ctl->frag.size = PAGE_SIZE << order;
		if (frag_ctl->frag.size < fragsz) {
			pr_err("alloc 0x%x mem, need:0x%x\n",
			       frag_ctl->frag.size, fragsz);
			put_page(frag_ctl->frag.page);
			goto fail;
		}

		/*
		 * Even if we own the page, we do not use atomic_set().
		 * This would break get_page_unless_zero() users.
		 */
		atomic_add(SDIOHAL_PAGECNT_MAX_BIAS - 1,
			   &frag_ctl->frag.page->_refcount);
		frag_ctl->pagecnt_bias = SDIOHAL_PAGECNT_MAX_BIAS;
		frag_ctl->frag.offset = 0;
	}

	if (frag_ctl->frag.offset + fragsz > frag_ctl->frag.size) {
		if (atomic_read(&frag_ctl->frag.page->_refcount)
			!= frag_ctl->pagecnt_bias) {
			if (!atomic_sub_and_test(frag_ctl->pagecnt_bias,
				&frag_ctl->frag.page->_refcount))
				goto refill;
			/* OK, page count is 0, we can safely set it */
			atomic_set(&frag_ctl->frag.page->_refcount,
				   SDIOHAL_PAGECNT_MAX_BIAS);
		} else {
			atomic_add(SDIOHAL_PAGECNT_MAX_BIAS
				- frag_ctl->pagecnt_bias,
				&frag_ctl->frag.page->_refcount);
		}
		frag_ctl->pagecnt_bias = SDIOHAL_PAGECNT_MAX_BIAS;
		frag_ctl->frag.offset = 0;
	}

	data = page_address(frag_ctl->frag.page) + frag_ctl->frag.offset;
	frag_ctl->frag.offset += fragsz;
	if (p_data->adma_rx_enable)
		frag_ctl->pagecnt_bias--;

	local_irq_restore(flags);
	return data;
fail:
	local_irq_restore(flags);
	pr_err("alloc mem fail\n");
	return NULL;
}

/* mbuf node no data buf pointer */
struct sdiohal_list_t *sdiohal_get_rx_mbuf_node(int num)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct sdiohal_list_t *idle_list;
	struct mbuf_t *mbuf_head, *mbuf_tail;
	int i;

	if (num == 0) {
		pr_err("num err:%d\n", num);
		goto err;
	}

	if (num > p_data->list_rx_buf.node_num) {
		pr_err("no rx mbuf node, need num:%d, list node num:%d\n",
		       num, p_data->list_rx_buf.node_num);
		goto err;
	}

	idle_list = kzalloc(sizeof(struct sdiohal_list_t), GFP_KERNEL);
	if (!idle_list)
		goto err;

	spin_lock_bh(&p_data->rx_spinlock);
	mbuf_head = mbuf_tail = p_data->list_rx_buf.mbuf_head;
	for (i = 1; i < num; i++)
		mbuf_tail = mbuf_tail->next;

	p_data->list_rx_buf.node_num -= num;
	if (p_data->list_rx_buf.node_num == 0) {
		p_data->list_rx_buf.mbuf_head = NULL;
		p_data->list_rx_buf.mbuf_tail = NULL;
	} else
		p_data->list_rx_buf.mbuf_head = mbuf_tail->next;

	idle_list->mbuf_head = mbuf_head;
	idle_list->mbuf_tail = mbuf_tail;
	idle_list->mbuf_tail->next = NULL;
	idle_list->node_num = num;
	spin_unlock_bh(&p_data->rx_spinlock);

	return idle_list;

err:
	return NULL;
}

/* for adma,mbuf list had data buf pointer */
struct sdiohal_list_t *sdiohal_get_rx_mbuf_list(int num)
{
	struct sdiohal_list_t *idle_list;
	struct mbuf_t *mbuf_temp;
	int i;

	idle_list = sdiohal_get_rx_mbuf_node(num);
	if (!idle_list)
		goto err;

	mbuf_temp = idle_list->mbuf_head;
	for (i = 0; i < num; i++) {
		mbuf_temp->buf = sdiohal_alloc_frag(MAX_MBUF_SIZE,
				 GFP_ATOMIC);
		if (!mbuf_temp->buf) {
			sdiohal_rx_list_free(idle_list->mbuf_head,
				idle_list->mbuf_tail, num);
			kfree(idle_list);
			goto err;
		}
		WARN_ON_ONCE((unsigned long int)
			     ((void *)(mbuf_temp->buf)) % 64);
		mbuf_temp = mbuf_temp->next;
	}

	sdiohal_list_check(idle_list, __func__, SDIOHAL_READ);

	return idle_list;
err:
	return NULL;
}

/* for normal dma idle buf */
void *sdiohal_get_rx_free_buf(void)
{
	void *p;

	p = sdiohal_alloc_frag(SDIOHAL_RX_RECVBUF_LEN,
			       GFP_ATOMIC);

	WARN_ON_ONCE(((unsigned long int)p) % 64);

	return p;
}

static int sdiohal_rx_mbuf_create(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	p_data->rx_mbuf_cache = kmem_cache_create("wcn_rx_mbuf",
						  sizeof(struct mbuf_t),
						  0, 0, NULL);
	if (p_data->rx_mbuf_cache)
		return 0;

	pr_info("%s no mem for mbuf cache\n", __func__);

	return -ENOMEM;
}

static void sdiohal_rx_mbuf_destroy(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	kmem_cache_destroy(p_data->rx_mbuf_cache);
	p_data->rx_mbuf_cache = NULL;
}

static int sdiohal_alloc_rx_mbuf_nodes(struct sdiohal_list_t *plist, int num)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mbuf_t *mbuf_node, *mbuf_temp = NULL;
	int i;

	if (unlikely(!p_data->rx_mbuf_cache))
		return -ENOMEM;

	for (i = 0; i < num; i++) {
		mbuf_node = kmem_cache_alloc(p_data->rx_mbuf_cache, GFP_KERNEL);
		memset(mbuf_node, 0, sizeof(struct mbuf_t));
		if (i == 0) {
			plist->mbuf_head = mbuf_node;
			plist->mbuf_tail = mbuf_node;
		} else
			mbuf_temp->next = mbuf_node;

		mbuf_temp = mbuf_node;
		plist->node_num++;
	}
	mbuf_temp->next = NULL;
	plist->mbuf_tail = mbuf_temp;

	return 0;
}

static void sdiohal_rx_buf_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	sdiohal_alloc_rx_mbuf_nodes(&p_data->list_rx_buf, SDIOHAL_RX_NODE_NUM);
}

static int sdiohal_free_rx_mbuf_nodes(struct sdiohal_list_t *plist, int num)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct mbuf_t *mbuf_node = NULL, *mbuf_temp;
	int i;

	if (unlikely(!p_data->rx_mbuf_cache))
		return -ENOMEM;

	mbuf_node = plist->mbuf_head;
	for (i = 0; i < num; i++) {
		mbuf_temp = mbuf_node;
		if (mbuf_temp->next) {
			mbuf_node = mbuf_temp->next;
			mbuf_temp->next = NULL;
			kmem_cache_free(p_data->rx_mbuf_cache, mbuf_temp);
		} else {
			if (i < num - 1)
				pr_err("%s mbuf_node error\n", __func__);
			kmem_cache_free(p_data->rx_mbuf_cache, mbuf_temp);
			break;
		}
	}

	plist->mbuf_head = NULL;
	plist->mbuf_tail = NULL;
	plist->node_num = 0;

	return 0;
}

static void sdiohal_rx_buf_deinit(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	sdiohal_free_rx_mbuf_nodes(&p_data->list_rx_buf, SDIOHAL_RX_NODE_NUM);
}

int sdiohal_list_push(int channel, struct mbuf_t *head,
		      struct mbuf_t *tail, int num)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct timespec64 tm_begin, tm_end;
	static long time_total_ns;
	static int times_count;
	struct mbuf_t *mbuf_node;
	int i;

	ktime_get_real_ts64(&tm_begin);

	if (unlikely(p_data->flag_init != true))
		return -ENODEV;

	/* May need to use: marlin_dev->power_state? */
	if (unlikely(p_data->card_dump_flag == true || !WCN_CARD_EXIST(&p_data->xmit_cnt)))
		return -ENODEV;

	sdiohal_mbuf_list_check(channel, head, tail, num, __func__,
				(channel < SDIO_CHN_TX_NUM ?
				SDIOHAL_WRITE : SDIOHAL_READ),
				(channel < SDIO_CHN_TX_NUM ?
				SDIOHAL_NORMAL_LEVEL :
				SDIOHAL_LIST_LEVEL));
	if ((channel < 0) || (channel >= SDIO_CHANNEL_NUM) ||
	    (!head) || (!tail) || (num <= 0)) {
		dump_stack();
		return -EINVAL;
	}

	mbuf_node = head;
	for (i = 0; i < num; i++, mbuf_node = mbuf_node->next) {
		if (!mbuf_node) {
			dump_stack();
			return -EFAULT;
		}
	}

	if (channel < SDIO_CHN_TX_NUM) {
		sdiohal_print_mbuf_data(channel, head, tail, num,
					__func__, SDIOHAL_DATA_LEVEL);
		sdiohal_tx_list_enq(channel, head, tail, num);

		ktime_get_real_ts64(&tm_end);
		time_total_ns += timespec64_to_ns(&tm_end) -
				 timespec64_to_ns(&tm_begin);
		times_count++;
		if (!(times_count % PERFORMANCE_COUNT)) {
			sdiohal_pr_perf("tx avg time:%ld\n",
					(time_total_ns / PERFORMANCE_COUNT));
			time_total_ns = 0;
			times_count = 0;
		}
		ktime_get_real_ts64(&p_data->tm_begin_sch);
		sdiohal_tx_list_push_dp(channel, head, tail, num);
		sdiohal_tx_up();
	} else
		sdiohal_rx_list_free(head, tail, num);

	return 0;
}

int sdiohal_list_direct_write(int channel, struct mbuf_t *head,
			      struct mbuf_t *tail, int num)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	struct sdiohal_list_t data_list;
	int ret = 0;

	if (unlikely(p_data->flag_init != true))
		return -ENODEV;

	/* May need to use: marlin_dev->power_state? */
	if (unlikely(p_data->card_dump_flag == true || !WCN_CARD_EXIST(&p_data->xmit_cnt)))
		return -ENODEV;

	sdiohal_lock_tx_ws();
	sdiohal_resume_check();
	sdiohal_cp_tx_wakeup(PACKER_DT_TX);
	sdiohal_tx_fill_puh(channel, head, tail, num);

	data_list.mbuf_head = head;
	data_list.mbuf_tail = tail;
	data_list.node_num = num;
	data_list.mbuf_tail->next = NULL;
	sdiohal_tx_list_push_direct_dp(channel, head, tail, num);

	if (p_data->adma_tx_enable)
		ret = sdiohal_adma_pt_write(&data_list);
	else
		ret = sdiohal_tx_data_list_send(&data_list);

	sdiohal_cp_tx_sleep(PACKER_DT_TX);
	sdiohal_unlock_tx_ws();

	return ret;
}

static int sdiohal_list_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	unsigned int channel;

	for (channel = 0; channel < SDIO_TX_LIST_NUM; channel++) {
		p_data->list_tx[channel] =
					kzalloc(sizeof(struct sdiohal_list_t),
					GFP_KERNEL);
		if (!p_data->list_tx[channel])
			return -ENOMEM;

		if (channel == 0)
			INIT_LIST_HEAD(&p_data->list_tx[channel]->head);
		else
			list_add_tail(&p_data->list_tx[channel]->head,
				      &p_data->list_tx[0]->head);
		p_data->list_tx[channel]->node_num = 0;
		p_data->list_tx[channel]->mbuf_head = NULL;
	}

	for (channel = 0; channel < SDIO_RX_LIST_NUM; channel++) {
		p_data->list_rx[channel] =
			kzalloc(sizeof(struct sdiohal_list_t), GFP_KERNEL);
		if (!p_data->list_rx[channel])
			return -ENOMEM;

		if (channel == 0)
			INIT_LIST_HEAD(&p_data->list_rx[channel]->head);
		else
			list_add_tail(&p_data->list_rx[channel]->head,
				      &p_data->list_rx[0]->head);
		p_data->list_rx[channel]->node_num = 0;
		p_data->list_rx[channel]->mbuf_head = NULL;
	}

	return 0;
}

static void sdiohal_list_deinit(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();
	unsigned int channel;
	struct list_head *pos, *next;

	for (channel = 0; channel < SDIO_TX_LIST_NUM; channel++) {
		list_for_each_safe(pos, next, &p_data->list_tx[channel]->head) {
			list_del_init(pos);
		}
		kfree(p_data->list_tx[channel]);
	}

	for (channel = 0; channel < SDIO_RX_LIST_NUM; channel++) {
		list_for_each_safe(pos, next, &p_data->list_rx[channel]->head) {
			list_del_init(pos);
		}
		kfree(p_data->list_rx[channel]);
	}
}

static int sdiohal_tx_sendbuf_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	p_data->send_buf.buf = kzalloc(SDIOHAL_TX_SENDBUF_LEN,
				       GFP_KERNEL);
	if (!p_data->send_buf.buf)
		return -ENOMEM;

	return 0;
}

static void sdiohal_tx_sendbuf_deinit(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	kfree(p_data->send_buf.buf);
	p_data->send_buf.buf = NULL;
	p_data->send_buf.retry_buf = NULL;
}

void sdiohal_tx_init_retrybuf(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	p_data->send_buf.retry_buf = p_data->send_buf.buf;
	p_data->send_buf.retry_len = p_data->send_buf.used_len;
}

static int sdiohal_eof_buf_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	struct bus_puh_t *puh;

	p_data->eof_buf = kzalloc(MAX_MBUF_SIZE, GFP_KERNEL);
	if (!p_data->eof_buf)
		return -ENOMEM;

	puh = (struct bus_puh_t *)(p_data->eof_buf);
	puh->type = 0;
	puh->subtype = 0;
	puh->len = 0;
	puh->eof = 1;
	puh->pad = 0;

	return 0;
}

static void sdiohal_eof_buf_deinit(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	kfree(p_data->eof_buf);
}

static int sdiohal_dtbs_buf_init(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	p_data->dtbs_buf = kzalloc(MAX_MBUF_SIZE, GFP_KERNEL);
	if (!p_data->dtbs_buf)
		return -ENOMEM;

	return 0;
}

static int sdiohal_dtbs_buf_deinit(void)
{
	struct sdiohal_data_t *p_data = sdiohal_get_data();

	kfree(p_data->dtbs_buf);
	p_data->dtbs_buf = NULL;

	return 0;
}

int sdiohal_misc_init(void)
{
	int ret;

	sdiohal_completion_init();
	sdiohal_wakelock_init();
	sdiohal_callback_lock_init();
	sdiohal_spinlock_init();
	sdiohal_sleep_flag_init();
	sdiohal_mutex_init();
	sdiohal_rx_mbuf_create();
	sdiohal_rx_buf_init();
	sdiohal_dtbs_buf_init();
	ret = sdiohal_list_init();
	if (ret < 0)
		pr_err("alloc list err\n");

	sdiohal_tx_sendbuf_init();
	sdiohal_waitqueue_init();
	ret = sdiohal_eof_buf_init();

	return ret;
}

void sdiohal_misc_deinit(void)
{
	sdiohal_eof_buf_deinit();
	sdiohal_tx_sendbuf_deinit();
	sdiohal_list_deinit();
	sdiohal_dtbs_buf_deinit();
	sdiohal_rx_buf_deinit();
	sdiohal_rx_mbuf_destroy();
	sdiohal_mutex_deinit();
	sdiohal_callback_lock_deinit();
	sdiohal_wakelock_deinit();
}
