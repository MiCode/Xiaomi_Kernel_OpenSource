/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <mach/irqs.h>
#include <linux/kallsyms.h>
#include <linux/delay.h>
#include <linux/wakelock.h>
#include <ccci.h>

#define FIRST_PENDING        (1<<0)
#define PENDING_50MS        (1<<1)
/* #define PENDING_100MS        (1<<2) */
#define PENDING_1500MS        (1<<2)

const struct logic_channel_static_info_t logic_ch_static_info_tab[] = {
	{CCCI_CONTROL_RX, 8, "ctl_rx", L_CH_MUST_RDY_FOR_BOOT},
	{CCCI_CONTROL_TX, 0, "ctl_tx",
	 L_CH_ATTR_TX | L_CH_ATTR_PRVLG1 | L_CH_ATTR_PRVLG0 | L_CH_ATTR_PRVLG2},
	{CCCI_SYSTEM_RX, 16, "sys_rx", 0},
	{CCCI_SYSTEM_TX, 0, "sys_tx", L_CH_ATTR_TX},
	{CCCI_PCM_RX, 128, "audio_rx", 0},
	{CCCI_PCM_TX, 0, "audio_tx", L_CH_ATTR_TX},
	{CCCI_UART1_RX, 8, "meta_rx", L_CH_DROP_TOLERATED},
	{CCCI_UART1_RX_ACK, 0, "meta_rx_ack",
	 L_CH_ATTR_TX | L_CH_ATTR_DUMMY_WRITE},
	{CCCI_UART1_TX, 0, "meta_tx", L_CH_ATTR_TX | L_CH_ATTR_DUMMY_WRITE},
	{CCCI_UART1_TX_ACK, 8, "meta_tx_ack", L_CH_DROP_TOLERATED},
	{CCCI_UART2_RX, 8, "muxd_rx", L_CH_DROP_TOLERATED},
	{CCCI_UART2_RX_ACK, 0, "muxd_rx_ack", L_CH_ATTR_TX},
	{CCCI_UART2_TX, 0, "muxd_tx", L_CH_ATTR_TX},
	{CCCI_UART2_TX_ACK, 8, "muxd_tx_ack", L_CH_DROP_TOLERATED},
	{CCCI_FS_RX, 16, "md_nvram_rx", 0},
	{CCCI_FS_TX, 0, "md_nvram_tx",
	 L_CH_ATTR_TX | L_CH_ATTR_PRVLG1 | L_CH_ATTR_OPEN_CLEAR |
	 L_CH_ATTR_PRVLG0 | L_CH_ATTR_PRVLG2},
	{CCCI_PMIC_RX, 0, "pmic_rx", 0},
	{CCCI_PMIC_TX, 0, "pmic_tx", L_CH_ATTR_TX},
	{CCCI_UEM_RX, 8, "uem_rx", 0},
	{CCCI_UEM_TX, 0, "uem_tx", L_CH_ATTR_TX},
	{CCCI_CCMNI1_RX, 8, "ccmni1_rx", L_CH_DROP_TOLERATED},
	{CCCI_CCMNI1_RX_ACK, 0, "ccmni1_rx_ack", L_CH_ATTR_TX},
	{CCCI_CCMNI1_TX, 0, "ccmni1_tx", L_CH_ATTR_TX},
	{CCCI_CCMNI1_TX_ACK, 8, "ccmni1_tx_ack", L_CH_DROP_TOLERATED},
	{CCCI_CCMNI2_RX, 8, "ccmni2_rx", L_CH_DROP_TOLERATED},
	{CCCI_CCMNI2_RX_ACK, 0, "ccmni2_rx_ack", L_CH_ATTR_TX},
	{CCCI_CCMNI2_TX, 0, "ccmni2_tx", L_CH_ATTR_TX},
	{CCCI_CCMNI2_TX_ACK, 8, "ccmni2_tx_ack", L_CH_DROP_TOLERATED},
	{CCCI_CCMNI3_RX, 8, "ccmni3_rx", L_CH_DROP_TOLERATED},
	{CCCI_CCMNI3_RX_ACK, 0, "ccmni3_rx_ack", L_CH_ATTR_TX},
	{CCCI_CCMNI3_TX, 0, "ccmni3_tx", L_CH_ATTR_TX},
	{CCCI_CCMNI3_TX_ACK, 8, "ccmni3_tx_ack", L_CH_DROP_TOLERATED},
	{CCCI_RPC_RX, 8, "rpc_rx", 0},
	{CCCI_RPC_TX, 0, "rpc_tx", L_CH_ATTR_TX | L_CH_ATTR_PRVLG1},
	{CCCI_IPC_RX, 8, "ipc_rx", 0},
	{CCCI_IPC_RX_ACK, 0, "ipc_rx_ack", L_CH_ATTR_TX},
	{CCCI_IPC_TX, 0, "ipc_tx", L_CH_ATTR_TX},
	{CCCI_IPC_TX_ACK, 8, "ipc_tx_ack", 0},
	{CCCI_IPC_UART_RX, 8, "ipc_uart_rx", L_CH_DROP_TOLERATED},
	{CCCI_IPC_UART_RX_ACK, 0, "ipc_uart_rx_ack", L_CH_ATTR_TX},
	{CCCI_IPC_UART_TX, 0, "ipc_uart_tx", L_CH_ATTR_TX},
	{CCCI_IPC_UART_TX_ACK, 8, "ipc_uart_tx_ack", L_CH_DROP_TOLERATED},
	{CCCI_MD_LOG_RX, 256, "md_log_rx", 0},
	{CCCI_MD_LOG_TX, 0, "md_log_tx",
	 L_CH_ATTR_TX | L_CH_ATTR_PRVLG1 | L_CH_ATTR_PRVLG2},
#ifdef CONFIG_MTK_ICUSB_SUPPORT
	{CCCI_ICUSB_RX, 8, "icusb_rx", L_CH_DROP_TOLERATED},
	{CCCI_ICUSB_RX_ACK, 0, "icusb_rx_ack", L_CH_ATTR_TX},
	{CCCI_ICUSB_TX, 0, "icusb_tx", L_CH_ATTR_TX},
	{CCCI_ICUSB_TX_ACK, 8, "icusb_tx_ack", L_CH_DROP_TOLERATED},
#endif
};

#define MAX_LOGIC_CH_ID        (sizeof(logic_ch_static_info_tab)/sizeof(struct logic_channel_static_info_t))

static struct logic_dispatch_ctl_block_t *logic_dispatch_ctlb[MAX_MD_NUM];
static unsigned char md_enabled[MAX_MD_NUM];	/*  Boot up time will determine this */
static unsigned char active_md[MAX_MD_NUM];
static unsigned int max_md_sys;
/****************************************************************************/
/* update&get md sys info                                                   */
/*                                                                          */
/****************************************************************************/
void set_md_sys_max_num(unsigned int max_num)
{
	max_md_sys = max_num;
}

void set_md_enable(int md_id, int en)
{
	md_enabled[md_id] = en;
}

void update_active_md_sys_state(int md_id, int active)
{
	if (md_enabled[md_id]) {
		active_md[md_id] = active;
		if (active) {
			/* CCCI_DBG_MSG(md_id, "cci", "enable modem intr\n"); */
			ccci_enable_md_intr(md_id);
		}
	} else {
		CCCI_MSG("md_sys%d is not enable\n", md_id);
	}
}

int get_md_wakeup_src(int md_id, char *buf, unsigned int len)
{
	unsigned int i, rx, ch;
	struct ccif_msg_t data;
	unsigned int rx_ch[CCIF_STD_V1_MAX_CH_NUM][2] = {
		{-1, 0}, {-1, 0}, {-1, 0}, {-1, 0}, {-1, 0},
		{-1, 0}, {-1, 0}, {-1, 0}
	 };
	char str[64];
	char log_buf[256] = "";
	int ret = 0;
	char *channel_name;
	struct ccif_t *ccif;
	int curr_str_len = 0;

	struct logic_dispatch_ctl_block_t *ctlb;

	ctlb = logic_dispatch_ctlb[md_id];
	ccif = ctlb->m_ccif;

	rx = ccif->ccif_get_rx_ch(ccif);
	if (rx == 0)
		return ret;

	for (i = 0; i < CCIF_STD_V1_MAX_CH_NUM; i++) {
		if (rx & (1 << i)) {
			ccif->ccif_read_phy_ch_data(ccif, i,
						    (unsigned int *)&data);
			for (ch = 0; ch < i; ch++)
				if (data.channel == rx_ch[ch][0])
					break;

			rx_ch[ch][0] = data.channel;
			rx_ch[ch][1]++;
		}
	}

	for (i = 0; i < CCIF_STD_V1_MAX_CH_NUM; i++) {
		if (rx_ch[i][1]) {
			if ((rx_ch[i][0] >= 0)
			    && (rx_ch[i][0] < MAX_LOGIC_CH_ID)) {
				channel_name =
				    logic_ch_static_info_tab[rx_ch[i]
							     [0]].m_ch_name;
				snprintf(str, sizeof(str), "%s(%d,%d) ", channel_name,
					rx_ch[i][0], rx_ch[i][1]);
			} else
				snprintf(str, sizeof(str), "%s(%d,%d) ", "unknown",
					rx_ch[i][0], rx_ch[i][1]);
			if (curr_str_len + strlen(str) < sizeof(log_buf) - 1) {
				strncat(log_buf, str, strlen(str));
				curr_str_len += strlen(str);
			}
		}
	}
	if (curr_str_len > sizeof(log_buf)) {
		CCCI_MSG
		    ("[ccci/ctl] wakeup source buffer not enough(req:%d>255) for MD%d\n",
		     curr_str_len, md_id + 1);
	}

	CCCI_MSG("[ccci/ctl] (%d)CCIF_MD%d wakeup source: %s\n", md_id + 1,
		 md_id + 1, log_buf);

	return ret;
}

/****************************************************************************/
/* logical channel handle function                                          */
/*                                                                          */
/****************************************************************************/
int register_to_logic_ch(int md_id, int ch, void (*func) (void *), void *owner)
{
	struct logic_channel_info_t *ch_info;
	int ret = 0;
	unsigned long flags;
	struct logic_dispatch_ctl_block_t *ctl_b;

	ctl_b = logic_dispatch_ctlb[md_id];
	ch_info = &(ctl_b->m_logic_ch_table[ch]);

	if ((ch_info->m_attrs & L_CH_ATTR_TX) && (func != NULL))
		return 0;

	spin_lock_irqsave(&ch_info->m_lock, flags);
	/*  Check whether call back function has been registered */
	if (!ch_info->m_register) {
		ch_info->m_register = 1;
		ch_info->m_call_back = func;
		ch_info->m_owner = owner;
	} else {
		CCCI_MSG_INF(md_id, "cci",
			     "%s fail: %s(ch%d) cb func has registered\n",
			     __func__, ch_info->m_ch_name,
			     ch_info->m_ch_id);
		ret = -CCCI_ERR_LOGIC_CH_HAS_REGISTERED;
	}
	spin_unlock_irqrestore(&ch_info->m_lock, flags);

	return ret;
}

int un_register_to_logic_ch(int md_id, int ch)
{
	struct logic_channel_info_t *ch_info;
	unsigned long flags;
	struct logic_dispatch_ctl_block_t *ctl_b;

	if (unlikely(ch >= CCCI_MAX_CH_NUM)) {
		CCCI_MSG_INF(md_id, "cci", "%s fail: invalid logic ch%d\n",
			     __func__, ch);
		return -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
	}

	ctl_b = logic_dispatch_ctlb[md_id];
	ch_info = &(ctl_b->m_logic_ch_table[ch]);

	spin_lock_irqsave(&ch_info->m_lock, flags);
	if (ch_info->m_register == 0)
		CCCI_MSG_INF(md_id, "cci", "ch%d not registered yet\n", ch);
	ch_info->m_call_back = NULL;
	ch_info->m_owner = NULL;
	ch_info->m_register = 0;
	spin_unlock_irqrestore(&ch_info->m_lock, flags);

	return 0;
}

int get_logic_ch_data(struct logic_channel_info_t *ch_info, struct ccci_msg_t *msg)
{
	if (unlikely(ch_info == NULL)) {
		CCCI_MSG("%s fail: get invalid ch info\n", __func__);
		return -CCCI_ERR_GET_NULL_POINTER;
	}

	if (unlikely(ch_info->m_attrs & L_CH_ATTR_TX)) {
		CCCI_MSG_INF(ch_info->m_md_id, "cci",
			"%s fail: %s(ch%d) is tx\n", __func__,
			ch_info->m_ch_name, msg->channel);
		return -CCCI_ERR_GET_RX_DATA_FROM_TX_CHANNEL;
	}

	/*  check whether fifo is ready */
	if (unlikely(!ch_info->m_kfifo_ready)) {
		CCCI_MSG_INF(ch_info->m_md_id, "cci",
			     "%s fail: %s(ch%d) kfifo not ready\n",
			     __func__, ch_info->m_ch_name, msg->channel);
		return -CCCI_ERR_KFIFO_IS_NOT_READY;
	}

	/*  Check fifo if has data */
	if (kfifo_is_empty(&ch_info->m_kfifo))
		return 0;

	/*  Pop data */
	return kfifo_out(&ch_info->m_kfifo, msg, sizeof(struct ccif_msg_t));
}

int get_logic_ch_data_len(struct logic_channel_info_t *ch_info)
{
	if (unlikely(ch_info == NULL)) {
		CCCI_MSG("%s get invalid ch info\n", __func__);
		return 0;
	}

	if (unlikely(ch_info->m_attrs & L_CH_ATTR_TX)) {
		CCCI_MSG_INF(ch_info->m_md_id, "cci",
			     "%s fail: %s(ch%d) is tx\n", __func__,
			     ch_info->m_ch_name, ch_info->m_ch_id);
		return 0;
	}

	/*  check whether fifo is ready */
	if (unlikely(!ch_info->m_kfifo_ready)) {
		CCCI_MSG_INF(ch_info->m_md_id, "cci",
			     "%s fail: %s(ch%d) kfifo not ready\n",
			     __func__, ch_info->m_ch_name,
			     ch_info->m_ch_id);
		return 0;
	}

	/*  Check fifo data length */
	return kfifo_len(&ch_info->m_kfifo);
}

struct logic_channel_info_t *get_logic_ch_info(int md_id, int ch_id)
{
	struct logic_channel_info_t *ch_info;
	struct logic_dispatch_ctl_block_t *ctl_block;

	if (unlikely(ch_id >= CCCI_MAX_CH_NUM)) {
		CCCI_MSG_INF(md_id, "cci", "%s fail: invalid logic ch%d\n",
			     __func__, ch_id);
		return NULL;
	}

	ctl_block = logic_dispatch_ctlb[md_id];
	ch_info = &(ctl_block->m_logic_ch_table[ch_id]);
	return ch_info;
}

static int __logic_dispatch_push(struct ccif_msg_t *msg, void *ctl_b)
{
	struct logic_channel_info_t *ch_info;
	int ret = 0;
	struct logic_dispatch_ctl_block_t *ctl_block =
	    (struct logic_dispatch_ctl_block_t *) ctl_b;
	int md_id = ctl_block->m_md_id;
	int drop = 1;

	if (unlikely(msg->channel >= CCCI_MAX_CH_NUM)) {
		CCCI_MSG_INF(md_id, "cci", "%s get invalid logic ch id:%d\n",
			     __func__, msg->channel);
		ret = -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
		goto _out;
	}

	ch_info = &(ctl_block->m_logic_ch_table[msg->channel]);
	if (unlikely(ch_info->m_attrs & L_CH_ATTR_TX)) {
		CCCI_MSG_INF(md_id, "cci", "%s CH:%d %s is tx channel\n",
			     __func__, msg->channel, ch_info->m_ch_name);
		ret = -CCCI_ERR_PUSH_RX_DATA_TO_TX_CHANNEL;
		goto _out;
	}

	/*  check whether fifo is ready */
	if (!ch_info->m_kfifo_ready) {
		CCCI_MSG_INF(md_id, "cci", "%s CH:%d %s's kfifo is not ready\n",
			     __func__, msg->channel, ch_info->m_ch_name);
		ret = -CCCI_ERR_KFIFO_IS_NOT_READY;
		goto _out;
	}

	/*  Check fifo free space */
	if (kfifo_is_full(&ch_info->m_kfifo)) {
		if (ch_info->m_attrs & L_CH_DROP_TOLERATED) {
			CCCI_CTL_MSG(md_id,
				     "Drop (%08X %08X %02d %08X) is tolerated\n",
				     msg->data[0], msg->data[1], msg->channel,
				     msg->reserved);
			ret = sizeof(struct ccif_msg_t);
		} else {
			/*  message should NOT be droped */
			CCCI_DBG_MSG(md_id, "cci",
				     "kfifo full: ch:%s size:%d (%08X %08X %02d %08X)\n",
				     ch_info->m_ch_name,
				     kfifo_size(&ch_info->m_kfifo),
				     msg->data[0], msg->data[1], msg->channel,
				     msg->reserved);
			/*  disalbe CCIF interrupt here???? */
			ret = 0;	/*  Fix this!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! */
		}
		goto _out;
	}

	/*  Push data */
	ret = kfifo_in(&ch_info->m_kfifo, msg, sizeof(struct ccif_msg_t));
	WARN_ON(ret != sizeof(struct ccif_msg_t));
	ctl_block->m_has_pending_data = 1;
	drop = 0;
 _out:
	add_logic_layer_record(md_id, (struct ccci_msg_t *) msg, drop);
	return ret;
}

static void __logic_layer_tasklet(unsigned long data)
{
	struct logic_dispatch_ctl_block_t *logic_ctlb =
	    (struct logic_dispatch_ctl_block_t *) data;
	struct logic_channel_info_t *ch_info;
	int i = 0;

	logic_ctlb->m_running = 1;
	while (logic_ctlb->m_has_pending_data) {
		logic_ctlb->m_has_pending_data = 0;
		for (i = 0; i < CCCI_MAX_CH_NUM; i++) {
			ch_info = &logic_ctlb->m_logic_ch_table[i];
			if (!ch_info->m_kfifo_ready)
				continue;

			if (!kfifo_is_empty(&ch_info->m_kfifo)) {
				/*  Note here, register call back should using spinlock_irqsave */
				spin_lock(&ch_info->m_lock);
				/*  Has data to process */
				if (ch_info->m_call_back != NULL) {
					/* 1.Check if this channel is active
						has call back means this channel is active */
					ch_info->m_call_back(ch_info);
				} else if (ch_info->m_attrs &
					   L_CH_MUST_RDY_FOR_BOOT) {
					/*  2.Important channel not ready, show waring message */
					CCCI_DBG_MSG(ch_info->m_md_id, "cci",
						     "Has pending msg for ch:%d\n",
						     ch_info->m_ch_id);
				}
				spin_unlock(&ch_info->m_lock);
			}
		}
	}
	logic_ctlb->m_running = 0;
	/* wake_lock_timeout(&logic_ctlb->m_wakeup_wake_lock, 3*HZ/2); */
}

static void __let_logic_dispatch_tasklet_run(void *ctl_b)
{
	struct logic_dispatch_ctl_block_t *ctl_block =
	    (struct logic_dispatch_ctl_block_t *) ctl_b;
	tasklet_schedule(&ctl_block->m_dispatch_tasklet);
}

void freeze_logic_layer_tx(int md_id)
{
	struct logic_dispatch_ctl_block_t *ctl_b;

	ctl_b = logic_dispatch_ctlb[md_id];
	ctl_b->m_freezed = 1;
}

void freeze_all_logic_layer(int md_id)
{
	struct logic_dispatch_ctl_block_t *ctl_b;
	struct ccif_t *ccif;

	ctl_b = logic_dispatch_ctlb[md_id];
	ccif = ctl_b->m_ccif;

	ccif->ccif_dis_intr(ccif);
	ctl_b->m_freezed = 1;
}

int logic_layer_reset(int md_id)
{
	struct logic_dispatch_ctl_block_t *ctl_b;
	struct ccif_t *ccif;
	u64 ref_jiffies = get_jiffies_64();
	int i;

	ctl_b = logic_dispatch_ctlb[md_id];
	ccif = ctl_b->m_ccif;

	/*  Check whether there is on-going isr/tasklet */
	while ((CCIF_TOP_HALF_RUNNING & ccif->m_status) || ctl_b->m_running
	       || ctl_b->m_has_pending_data) {
		if ((get_jiffies_64() - ref_jiffies) > 2 * HZ) {
			CCCI_MSG_INF(ctl_b->m_md_id, "cci",
				     "%s wait isr/tasklet more than 2 seconds\n",
				     __func__);
			break;
		}
	}
	/*  isr/tasklet done, then reset ccif and logic channel */
	ccif->ccif_reset(ccif);
	for (i = 0; i < CCCI_MAX_CH_NUM; i++) {
		if (ctl_b->m_logic_ch_table[i].m_kfifo_ready)
			kfifo_reset(&(ctl_b->m_logic_ch_table[i].m_kfifo));
	}
	ctl_b->m_has_pending_data = 0;
	ctl_b->m_freezed = 0;
	ctl_b->m_running = 0;
	/* ctl_b->m_privilege = MD_BOOT_STAGE_0; */
	ctl_b->m_status_flag = 0;
	return 0;
}

/****************************************************************************/
/* logical channel handle function                                           */
/*                                                                           */
/****************************************************************************/
int bind_to_low_layer_notify(int md_id, void (*isr_func)(int),
			     void (*send_func)(int, unsigned int))
{
	struct logic_dispatch_ctl_block_t *ctl_b;
	struct ccif_t *ccif;
	int ret = 0;

	ctl_b = logic_dispatch_ctlb[md_id];

	/*  Check whether call back function has been registered */
	if (NULL != ctl_b->m_send_notify_cb) {
		ret = -CCCI_ERR_MD_CB_HAS_REGISTER;
		return ret;
	}
	ctl_b->m_send_notify_cb = send_func;

	if (isr_func) {
		ccif = ctl_b->m_ccif;
		ret = ccif->register_isr_notify_func(ccif, isr_func);
	}
	return ret;
}

/*  Support function for ccci char/tty/ccmni/fs/rpc/ipc/...  */
int ccci_message_send(int md_id, struct ccci_msg_t *msg, int retry_en)
{
	struct logic_dispatch_ctl_block_t *ctl_b;
	struct ccif_t *ccif;
	int ret = 0;
	int args = 0;
	int drop = 0;
	unsigned long flags;
	int need_notify = 0;
	int md_stage = 0;

	ctl_b = logic_dispatch_ctlb[md_id];
	ccif = ctl_b->m_ccif;

	if (unlikely(ctl_b->m_freezed)) {
		CCCI_MSG_INF(md_id, "cci", "%s fail: ccci is freezed\n",
			     __func__);
		ret = -CCCI_ERR_MD_NOT_READY;
		goto out;
	}

	if (unlikely(msg->channel >= CCCI_MAX_CH_NUM)) {
		if (msg->channel == CCCI_FORCE_ASSERT_CH) {
			ret =
			    ccif->ccif_write_phy_ch_data(ccif,
							 (unsigned int *)msg,
							 retry_en);
			goto out;
		} else {
			CCCI_MSG_INF(md_id, "cci",
				     "%s fail: invalid logic ch(%d)\n",
				     __func__, msg->channel);
			ret = -CCCI_ERR_INVALID_LOGIC_CHANNEL_ID;
			goto out;
		}
	}

	md_stage = get_curr_md_state(md_id);
	if (unlikely(md_stage == MD_BOOT_STAGE_0)) {	/*  PRIVILEGE 0 <-- */
		/*  At ccci privilege mode, only privilege channel can send data to modem */
		if (ctl_b->
		    m_logic_ch_table[msg->channel].m_attrs & L_CH_ATTR_PRVLG0) {
			ret =
			    ccif->ccif_write_phy_ch_data(ccif,
							 (unsigned int *)msg,
							 retry_en);
		} else {
			ret = -ENODEV;
		}
	} else if (unlikely(md_stage == MD_BOOT_STAGE_1)) {	/*  PRIVILEGE 1 <-- */
		if (ctl_b->
		    m_logic_ch_table[msg->channel].m_attrs & L_CH_ATTR_PRVLG1) {
			ret =
			    ccif->ccif_write_phy_ch_data(ccif,
							 (unsigned int *)msg,
							 retry_en);
		} else {
			ret = -ENODEV;
		}
	} else if (unlikely(md_stage == MD_BOOT_STAGE_EXCEPTION)) {	/*  PRIVILEGE 2 <-- */
		if (ctl_b->
		    m_logic_ch_table[msg->channel].m_attrs & L_CH_ATTR_PRVLG2) {
			ret =
			    ccif->ccif_write_phy_ch_data(ccif,
							 (unsigned int *)msg,
							 retry_en);
		} else if (ctl_b->
			   m_logic_ch_table[msg->
					    channel].m_attrs &
			   L_CH_ATTR_DUMMY_WRITE) {
			ret = sizeof(struct ccci_msg_t);	/*  Dummy write here, MD using polling */
		} else {
			ret = -ETXTBSY;
		}
	} else {
		ret =
		    ccif->ccif_write_phy_ch_data(ccif, (unsigned int *)msg,
						 retry_en);
	}

 out:
	spin_lock_irqsave(&ctl_b->m_lock, flags);
	if (ret == -CCCI_ERR_CCIF_NO_PHYSICAL_CHANNEL) {
		drop = 1;
		if ((get_debug_mode_flag() & (DBG_FLAG_JTAG | DBG_FLAG_DEBUG))
		    == 0) {
			need_notify = 1;
			if ((ctl_b->m_status_flag & FIRST_PENDING) == 0) {
				ctl_b->m_status_flag |= FIRST_PENDING;
				ctl_b->m_last_send_ref_jiffies = get_jiffies_64();	/*  Update jiffies; */
			} else {
				if (((get_jiffies_64() -
				      ctl_b->m_last_send_ref_jiffies) > 5)
				    && ((ctl_b->m_status_flag & PENDING_50MS) == 0)) {	/* 50ms */
					/*  Dump EE memory */
					args = 1;
					ctl_b->m_status_flag |= PENDING_50MS;
				} else if ((get_jiffies_64() - ctl_b->m_last_send_ref_jiffies) > 150) {	/* 1500ms */
					/* Trigger EE */
					args = 2;
					/* ctl_b->m_status_flag |= PENDING_100MS; */
					ctl_b->m_status_flag |= PENDING_1500MS;
				}
			}
		}
	} else {
		ctl_b->m_status_flag = 0;
	}
	spin_unlock_irqrestore(&ctl_b->m_lock, flags);
	if ((NULL != ctl_b->m_send_notify_cb) && need_notify)
		ctl_b->m_send_notify_cb(md_id, args);

	add_logic_layer_record(md_id, msg, drop);

	return ret;
}

void ccci_disable_md_intr(int md_id)
{
	struct ccif_t *ccif_obj;

	ccif_obj = logic_dispatch_ctlb[md_id]->m_ccif;
	ccif_obj->ccif_dis_intr(ccif_obj);
}

void ccci_enable_md_intr(int md_id)
{
	struct ccif_t *ccif_obj;

	ccif_obj = logic_dispatch_ctlb[md_id]->m_ccif;
	ccif_obj->ccif_en_intr(ccif_obj);
}

void ccci_hal_reset(int md_id)
{
	struct ccif_t *ccif_obj;

	ccif_obj = logic_dispatch_ctlb[md_id]->m_ccif;
	ccif_obj->ccif_reset(ccif_obj);
}

void ccci_hal_irq_register(int md_id)
{
	struct ccif_t *ccif_obj;

	ccif_obj = logic_dispatch_ctlb[md_id]->m_ccif;
	ccif_obj->ccif_register_intr(ccif_obj);
}

int ccci_write_runtime_data(int md_id, unsigned char buf[], int len)
{
	struct ccif_t *ccif_obj;
	int tmp;

	tmp = (int)buf;
	if ((tmp & (~0x3)) != tmp)
		return -CCCI_ERR_START_ADDR_NOT_4BYTES_ALIGN;

	if ((len & (~0x3)) != len)
		return -CCCI_ERR_NOT_DIVISIBLE_BY_4;

	ccif_obj = logic_dispatch_ctlb[md_id]->m_ccif;
	return ccif_obj->ccif_write_runtime_data(ccif_obj, (unsigned int *)buf,
						 len >> 2);
}

void ccci_dump_logic_layer_info(int md_id, unsigned int buf[], int len)
{
	struct ccif_t *ccif;
	struct logic_dispatch_ctl_block_t *ctl_b;

	ctl_b = logic_dispatch_ctlb[md_id];

	if (ctl_b != NULL) {
		/*  1. Dump CCIF Info */
		ccif = ctl_b->m_ccif;
		ccif->ccif_dump_reg(ccif, buf, len);
		/*  2. Dump logic layer info */
		dump_logical_layer_tx_rx_histroy(md_id);
	}
}

void ccci_dump_hw_reg_val(int md_id, unsigned int buf[], int len)
{
	struct ccif_t *ccif;
	struct logic_dispatch_ctl_block_t *ctl_b;

	ctl_b = logic_dispatch_ctlb[md_id];

	if (ctl_b != NULL) {
		/*  1. Dump CCIF Info */
		ccif = ctl_b->m_ccif;
		ccif->ccif_dump_reg(ccif, buf, len);
	}
}

/****************************************************************************/
/* ccci logical layer initial                                               */
/*                                                                          */
/****************************************************************************/
int ccci_logic_ctlb_init(int md_id)
{
	int ret = 0;
	struct ccif_t *ccif;
	struct logic_channel_info_t *ch_info;
	int ch_id, ch_attr, i;
	struct logic_dispatch_ctl_block_t *ctl_b;
	struct ccif_hw_info_t ccif_hw_inf;

	CCCI_FUNC_ENTRY(md_id);

	/*  Channel number check */
	if ((sizeof(logic_ch_static_info_tab) /
	     sizeof(struct logic_channel_static_info_t)) != CCCI_MAX_CH_NUM) {
		CCCI_MSG_INF(md_id, "cci",
			     "%s: channel max number mis-match fail, %d:%d\n",
			     __func__, sizeof(logic_ch_static_info_tab)/sizeof(struct logic_channel_static_info_t),
			     CCCI_MAX_CH_NUM);
		return -CCCI_ERR_CHANNEL_NUM_MIS_MATCH;
	}

	/*  Allocate ctl block memory */
	ctl_b = (struct logic_dispatch_ctl_block_t *)
	    kzalloc(sizeof(struct logic_dispatch_ctl_block_t), GFP_KERNEL);
	if (ctl_b == NULL) {
		CCCI_MSG_INF(md_id, "cci",
			     "%s: alloc memory fail for logic_dispatch_ctlb\n",
			     __func__);
		return -CCCI_ERR_ALLOCATE_MEMORY_FAIL;
	}
	logic_dispatch_ctlb[md_id] = ctl_b;

	/*  Get CCIF HW info */
	if (get_ccif_hw_info(md_id, &ccif_hw_inf) < 0) {
		CCCI_MSG_INF(md_id, "cci", "%s: get ccif%d hw info fail\n",
			     __func__, md_id + 1);
		ret = -CCCI_ERR_CCIF_GET_HW_INFO_FAIL;
		goto _ccif_instance_create_fail;
	}

	/*  Create ccif instance */
	ccif = ccif_create_instance(&ccif_hw_inf, ctl_b, md_id);
	if (ccif == NULL) {
		CCCI_MSG_INF(md_id, "cci", "%s: create ccif instance fail\n",
			     __func__);
		ret = -CCCI_ERR_CREATE_CCIF_INSTANCE_FAIL;
		goto _ccif_instance_create_fail;
	}

	ccif->ccif_init(ccif);
	ctl_b->m_ccif = ccif;

	/*  Initialize logic channel and its kfifo */
	/*  Step1, set all runtime channel id to CCCI_INVALID_CH_ID means default state */
	/*  So, even if static channel table is out of order, we can make sure logic_dispatch_ctlb's channel */
	/*  table is in order */
	for (i = 0; i < CCCI_MAX_CH_NUM; i++) {
		ch_info = &ctl_b->m_logic_ch_table[i];
		ch_info->m_ch_id = CCCI_INVALID_CH_ID;
	}

	/*  Step2, set all runtime channel info according to static channel info, make it in order */
	for (i = 0; i < CCCI_MAX_CH_NUM; i++) {
		ch_id = logic_ch_static_info_tab[i].m_ch_id;
		ch_info = &ctl_b->m_logic_ch_table[ch_id];

		if (ch_info->m_ch_id != CCCI_INVALID_CH_ID) {
			CCCI_MSG_INF(md_id, "cci",
				     "[Error]%s: ch%d has registered\n",
				     __func__, ch_id);
			ret = -CCCI_ERR_REPEAT_CHANNEL_ID;
			goto _ccif_logic_channel_init_fail;
		} else {
			ch_info->m_ch_id = ch_id;
			ch_info->m_attrs = logic_ch_static_info_tab[i].m_attrs;
			ch_info->m_ch_name =
			    logic_ch_static_info_tab[i].m_ch_name;
			ch_info->m_call_back = NULL;

			if (logic_ch_static_info_tab[i].m_kfifo_size) {
				if (0 !=
				    kfifo_alloc(&ch_info->m_kfifo,
						sizeof(struct ccif_msg_t) *
						logic_ch_static_info_tab
						[i].m_kfifo_size, GFP_KERNEL)) {
					CCCI_MSG_INF(md_id, "cci",
						     "%s: alloc kfifo fail for %s(ch%d)\n",
						     __func__,
						     ch_info->m_ch_name, ch_id);

					ch_info->m_kfifo_ready = 0;
					ret = CCCI_ERR_ALLOCATE_MEMORY_FAIL;
					goto _ccif_logic_channel_init_fail;
				} else {
					ch_info->m_kfifo_ready = 1;
					ch_info->m_md_id = md_id;
				}
			} else {
				ch_info->m_kfifo_ready = 0;
			}

			spin_lock_init(&ch_info->m_lock);
		}

		/* initial channel recording info */
		if (logic_ch_static_info_tab[i].m_attrs & L_CH_ATTR_TX)
			ch_attr = CCCI_LOG_TX;
		else
			ch_attr = CCCI_LOG_RX;
		statistics_init_ch_dir(md_id, ch_id, ch_attr,
				       ch_info->m_ch_name);
	}

	/*  Init logic_dispatch_ctlb */
	tasklet_init(&ctl_b->m_dispatch_tasklet, __logic_layer_tasklet,
		     (unsigned long)ctl_b);
	ctl_b->m_has_pending_data = 0;
	ctl_b->m_freezed = 0;
	ctl_b->m_running = 0;
	/* ctl_b->m_privilege = MD_BOOT_STAGE_0; */
	ctl_b->m_md_id = md_id;
	snprintf(ctl_b->m_wakelock_name, sizeof(ctl_b->m_wakelock_name),
		 "ccci%d_logic", (md_id + 1));
	wake_lock_init(&ctl_b->m_wakeup_wake_lock, WAKE_LOCK_SUSPEND,
		       ctl_b->m_wakelock_name);
	ctl_b->m_send_notify_cb = NULL;
	spin_lock_init(&ctl_b->m_lock);

	/*  Init CCIF now */
	ccif->register_call_back_func(ccif, __logic_dispatch_push,
				      __let_logic_dispatch_tasklet_run);
	/* ccif->ccif_register_intr(ccif); */

	/*  Init done */
	/* CCCI_DBG_MSG(md_id, "cci", "ccci_logic_ctlb_init success!\n"); */
	return ret;

 _ccif_logic_channel_init_fail:
	for (i = 0; i < CCCI_MAX_CH_NUM; i++) {
		ch_info = &ctl_b->m_logic_ch_table[i];
		if (ch_info->m_kfifo_ready) {
			kfifo_free(&ch_info->m_kfifo);
			ch_info->m_kfifo_ready = 0;
		}
	}

 _ccif_instance_create_fail:
	kfree(ctl_b);
	logic_dispatch_ctlb[md_id] = NULL;

	return ret;
}

void ccci_logic_ctlb_deinit(int md_id)
{
	struct ccif_t *ccif;
	struct logic_channel_info_t *ch_info;
	int i;
	struct logic_dispatch_ctl_block_t *ctl_b;

	ctl_b = logic_dispatch_ctlb[md_id];

	if (ctl_b != NULL) {
		/*  Step 1, freeze ccci */
		ctl_b->m_freezed = 1;
		/*  Step 2, de-init ccif */
		ccif = logic_dispatch_ctlb[md_id]->m_ccif;
		ccif->ccif_de_init(ccif);
		/*  Step 3, kill ccci dispatch tasklet */
		tasklet_kill(&ctl_b->m_dispatch_tasklet);
		/*  Step 4, free kfifo memory */
		for (i = 0; i < CCCI_MAX_CH_NUM; i++) {
			ch_info = &ctl_b->m_logic_ch_table[i];
			if (ch_info->m_kfifo_ready) {
				kfifo_free(&ch_info->m_kfifo);
				ch_info->m_kfifo_ready = 0;
			}
		}
		/*  Step 5, destroy wake lock */
		wake_lock_destroy(&ctl_b->m_wakeup_wake_lock);
		/*  Step 6, free logic_dispatch_ctlb memory */
		kfree(ctl_b);
		logic_dispatch_ctlb[md_id] = NULL;
	}
}

int ccci_logic_layer_init(int md_id)
{
	int ret = 0;

	ret = ccci_logic_ctlb_init(md_id);

	return ret;
}

void ccci_logic_layer_exit(int md_id)
{
	ccci_logic_ctlb_deinit(md_id);
}
