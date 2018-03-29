/*
 *drivers/mmc/card/modem_sdio.c
 *
 *VIA CBP SDIO driver for Linux
 *
 *Copyright (C) 2009 VIA TELECOM Corporation, Inc.
 *Author: VIA TELECOM Corporation, Inc.
 *
 *This package is free software; you can redistribute it and/or modify
 *it under the terms of the GNU General Public License version 2 as
 *published by the Free Software Foundation.
 *
 *THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 *IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 *WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include <linux/module.h>
#include <linux/mod_devicetable.h>
#include <linux/mmc/core.h>
#include <linux/mmc/card.h>
#include <linux/mmc/sdio.h>
#include <linux/mmc/sdio_func.h>
#include <linux/mmc/sdio_ids.h>
#include <linux/platform_device.h>
#include <linux/irq.h>
#include <linux/circ_buf.h>
#include <linux/kfifo.h>
#include <linux/slab.h>
#include <linux/fs.h>

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/mutex.h>
#include <linux/mmc/host.h>
#include <linux/gpio.h>
#include <linux/wait.h>
#include <linux/workqueue.h>
#include <linux/version.h>
#include "viatel_rawbulk.h"
#include "modem_sdio.h"
#include "c2k_hw.h"
#ifdef CONFIG_MTK_AEE_FEATURE
#include <mt-plat/aee.h>
#else
#define DB_OPT_DEFAULT	(0)	/*Dummy macro define to avoid build error */
#define DB_OPT_FTRACE   (0)	/*Dummy macro define to avoid build error */
#endif

#if ENABLE_CHAR_DEV
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/poll.h>
#else
#include <linux/tty.h>
#include <linux/tty_flip.h>
#endif

#if ENABLE_CCMNI
#include <linux/skbuff.h>
/*#include "c2k_ccmni_ccci_intf.h"*/

#include "ccmni.h"
#endif

static int sdio_tx_cnt;
static int sdio_rx_cnt;
#define FIFO_SIZE				(8*PAGE_SIZE)	/*transmit fifo size for each tty port */
#define SDIO_WAKEUP_CHARS		(8*256)	/*when data in transmit buffer less than this, ask for more from tty */

#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
#define TRANSMIT_SHIFT	(10)
#else
#define TRANSMIT_SHIFT	(11)
#endif
#define TRANSMIT_BUFFER_SIZE	(1UL << TRANSMIT_SHIFT)
#define TRANSMIT_MAX_SIZE		((1UL << TRANSMIT_SHIFT)  - sizeof(struct sdio_msg_head))
#define RECORD_DUMP_MAX			(20)

#define PADDING_BY_BLOCK_SIZE	(0)

#if USE_CCIF_INTR
#define RX_DONE_CH				(1)
#define TX_DATA_CH				(0)
#endif

static struct tty_driver *modem_sdio_tty_driver;

int sdio_log_level = LOG_ERR;

struct sdio_modem *c2k_modem = NULL;

static unsigned int dtr_value;
static unsigned int dcd_state;

static char *type_str[] = {[md_type_invalid] = "invalid",
	[modem_2g] = "2g",
	[modem_3g] = "3g",
	[modem_wg] = "wg",
	[modem_tg] = "tg",
	[modem_lwg] = "lwg",
	[modem_ltg] = "ltg",
	[modem_sglte] = "sglte"
};

static char *product_str[] = {[INVALID_VARSION] = INVALID_STR,
	[DEBUG_VERSION] = DEBUG_STR,
	[RELEASE_VERSION] = RELEASE_STR
};

char c2k_img_info_str[256];

#if ENABLE_CCMNI
/************************ccmni struct define***********************/
#define CCMNI_INTF_COUNT		(3)
#define SDIO_MD_ID				(2)
#define CCMNI_CH_ID				DATA_CH_ID

/************************ccmni struct define***********************/

int sdio_modem_get_ccmni_ch(int md_id, int ccmni_idx, struct ccmni_ch *channel)
{
	int ret = 0;

	if ((ccmni_idx >= 0) && (ccmni_idx < CCMNI_INTF_COUNT)) {
		channel->rx = ccmni_idx;
		channel->rx_ack = 0xFF;
		channel->tx = ccmni_idx;
		channel->tx_ack = 0xFF;
		channel->dl_ack = ccmni_idx;
		channel->multiq = 0;
	} else {
		pr_debug("[ccci%d/net] invalid ccmni index=%d\n", md_id,
			 ccmni_idx);
		ret = -1;
	}

	return ret;
}

struct ccmni_ccci_ops sdio_ccmni_ops = {
	.ccmni_ver = CCMNI_DRV_V0,	/*CCMNI_DRV_VER */
	.ccmni_num = CCMNI_INTF_COUNT,
	.name = "cc3mni",	/*"ccmni" or "cc2mni" or "ccemni" */
#if defined CONFIG_MTK_IRAT_SUPPORT
#if defined CONFIG_MTK_C2K_SLOT2_SUPPORT
	.md_ability = MODEM_CAP_CCMNI_IRAT | MODEM_CAP_TXBUSY_STOP | MODEM_CAP_WORLD_PHONE,
#else
	.md_ability = MODEM_CAP_CCMNI_IRAT | MODEM_CAP_TXBUSY_STOP,
#endif
	.irat_md_id = MD_SYS1,
#else
	.md_ability = MODEM_CAP_TXBUSY_STOP,
	.irat_md_id = -1,
#endif
	.napi_poll_weigh = 0,
	.send_pkt = sdio_modem_ccmni_send_pkt,
	.napi_poll = NULL,
	.get_ccmni_ch = sdio_modem_get_ccmni_ch,
};

#endif

/*static unsigned int modem_remove = 1;*/
/*static spinlock_t modem_remove_lock;*/

/*#define TX_DONE_TRACE*/
#ifdef TX_DONE_TRACE
struct timer_list timer_wait_tx_done;

static void wait_tx_done_timer(unsigned long data)
{
	msdc_c2k_dump_int_register();
	mod_timer(&timer_wait_tx_done, jiffies + msecs_to_jiffies(1000));
}

#endif

static inline int calc_payload_len(const struct sdio_msg_head *head,
				   unsigned int *offset)
{
	unsigned int len = 0;
	unsigned int payload_offset = 0;
#if 0
	struct sdio_msg_head *head = NULL;

	if (buf) {
		head = (struct sdio_msg_head *)buf;
		len = (((head->tranHi & 0x0F) << 8) | (head->tranLow & 0xFF));
	}
#else
	if (head) {
		payload_offset = ((head->tranHi & 0xC0) >> 6);
		if (payload_offset) {
			LOGPRT(LOG_DEBUG, "%s %d: payload_offset = %d.\n",
			       __func__, __LINE__, payload_offset);
		}
		len = (((head->tranHi & 0x0F) << 8) | (head->tranLow & 0xFF));
		len -= payload_offset;
	}
#endif
	if (offset)
		*offset = payload_offset;

	return len;
}

/**
 *sdio_tx_rx_printk - print sdio tx and rx data, when log level is LOG_NOTICE or larger
 *@buf: the point of data buffer
 *@type: print type, 0:rx  1:tx
 *
 *no return
 */
static void sdio_tx_rx_printk(const void *buf, unsigned char type)
{
	unsigned int count;
	const unsigned char *print_buf = (const unsigned char *)buf;
	const struct sdio_msg_head *head = NULL;
	/*int i; */
	/*return; */
	if (buf)
		head = (struct sdio_msg_head *)buf;
	else
		return;

	count = calc_payload_len(head, NULL);
	if (type == 1)
		LOGPRT(LOG_INFO, "write %d to channel%d/[%d]>>",
			 count, head->chanInfo + (head->tranHi & EXTEND_CH_BIT), sdio_tx_cnt);
	else
		LOGPRT(LOG_INFO, "read %d from channel%d/[%d]<<",
			 count, head->chanInfo + (head->tranHi & EXTEND_CH_BIT), sdio_rx_cnt);

	/*if(count > RECORD_DUMP_MAX) */
	/*count = RECORD_DUMP_MAX; */

	LOGPRT
	    (LOG_INFO, "%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x-%x\n",
	     *print_buf, *(print_buf + 1), *(print_buf + 2), *(print_buf + 3),
	     *(print_buf + 4), *(print_buf + 5), *(print_buf + 6),
	     *(print_buf + 7), *(print_buf + 8), *(print_buf + 9),
	     *(print_buf + 10), *(print_buf + 11));
/*
	for(i = 0; i < count + sizeof(struct sdio_msg_head); i++)
	{
		printk(KERN_CONT "%x-", *(print_buf+i));
	}
	printk(KERN_CONT "\n");
*/
}

#if 0
static void sdio_rx_check(struct sdio_modem *modem, const void *buf)
{
	unsigned int count;
	const unsigned char *print_buf = (const unsigned char *)buf;
	int i;
	const struct sdio_msg_head *head = NULL;

	if (buf)
		head = (struct sdio_msg_head *)buf;

	return;
	/*calc payload length */
	count = calc_payload_len(head, NULL);

	pr_debug("[MODEM SDIO] rx_check chan [%d], data_len [%d]\n",
		 *(print_buf + 1), count);

	for (i = 0; i < count + sizeof(struct sdio_msg_head); i++) {
		if (i % 16 == 0)
			pr_debug("[MODEM SDIO] rx_check ");

		pr_debug("%02X-", *(print_buf + i));
		if ((i + 1) % 16 == 0)
			pr_debug("\n");
	}
	pr_debug("\n");
}
#endif

static struct sdio_modem_port *sdio_modem_tty_port_get(unsigned index)
{
	struct sdio_modem_port *port = NULL;
	/*unsigned long flags = 0; */

	if (index >= SDIO_TTY_NR)
		return NULL;

	if (c2k_modem)
		port = c2k_modem->port[index];

	return port;
}

static void sdio_modem_port_destroy(struct kref *kref)
{
	struct sdio_modem_port *port =
	    container_of(kref, struct sdio_modem_port, kref);
	int index;

	if (port) {
		index = port->index;
		LOGPRT(LOG_INFO, "%s %d: index = %d .\n", __func__, __LINE__,
		       index);
		kfifo_free(&port->transmit_fifo);
		kfree(port);
	} else {
		LOGPRT(LOG_ERR, "%s %d: invalid port.\n", __func__, __LINE__);
	}
}

/*we just call kref_get in modem_tty_install once, so when port not exist any more, call this function to destroy it.*/
static void sdio_modem_tty_port_put(struct sdio_modem_port *port)
{
	if (port) {
		LOGPRT(LOG_INFO, "%s %d: port %d.\n", __func__, __LINE__,
		       port->index);
		kref_put(&port->kref, sdio_modem_port_destroy);
	} else {
		LOGPRT(LOG_ERR, "%s %d: invalid port.\n", __func__, __LINE__);
	}
}

/*check if port is valid*/
static int check_port(struct sdio_modem_port *port)
{
	struct sdio_modem *modem = NULL;
	int ret = 0;

	if (!port) {
		LOGPRT(LOG_ERR, "%s port NULL\n", __func__);
		ret = -ENODEV;
		return ret;
	} else {
		modem = port->modem;
		if (!port->func) {
			LOGPRT(LOG_ERR, "%s %d: port%d func NULL\n", __func__,
			       __LINE__, port->index);
			ret = -ENODEV;
		} else if (modem->status == MD_OFF) {
			LOGPRT(LOG_ERR, "%s %d: modem is off now.(%d)\n",
			       __func__, __LINE__, port->index);
			ret = -ENODEV;
		}
	}
	/*WARN_ON(!port->port.count); */
	LOGPRT(LOG_INFO, "%s %d: check port OK.(%d)\n",
			       __func__, __LINE__, port->index);
	return ret;
}

static void modem_sdio_write(struct sdio_modem *modem, int addr,
			     void *buf, size_t len);

/*CBP control message type */
enum cbp_contrl_message_type {
	CHAN_ONOFF_MSG_ID = 0,
	MDM_STATUS_IND_MSG_ID,
	MDM_STATUS_QUERY_MSG_ID,
	CHAN_SWITCH_REQ_MSG_ID,
	CHAN_STATUS_QUERY_MSG_ID,
	FLOW_CONTROL_MSG_ID,
	CHAN_LOOPBACK_TST_MSG_ID,
	HEART_BEAT_MSG_ID,
	FORCE_ASSERT_MSG_ID,
	MESSAGE_COUNT,
};

enum {
	CHAN_OFF = 0,
	CHAN_ON,
};

enum {
	OPT_LOOPBACK_NON = 0,	/*no operation, default 0 */
	OPT_LOOPBACK_OPEN = 1,	/*open loopback test */
	OPT_LOOPBACK_CLOSE = 2,	/*close loopback test */
	OPT_LOOPBACK_QUERY = 3,	/*query loopback test */
	OPT_LOOPBACK_NUM
};

static int contruct_ctrl_chan_msg(struct sdio_modem_ctrl_port *ctrl_port,
				  int msg, unsigned char chan_num,
				  unsigned char opt)
{
	if (unlikely(ctrl_port == NULL)) {
		LOGPRT(LOG_ERR, "%s %d: control channel is null.\n", __func__,
		       __LINE__);
		return -EINVAL;
	}

	ctrl_port->chan_ctrl_msg.head.start_flag = MSG_START_FLAG;
	ctrl_port->chan_ctrl_msg.head.chanInfo = 0;
	ctrl_port->chan_ctrl_msg.head.tranHi = 0;	/*High byte of the following payload length */
	ctrl_port->chan_ctrl_msg.head.tranLow = 4;	/*Low byte of the following payload length */

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	ctrl_port->chan_ctrl_msg.head.hw_head.len_hi =
	    0xFF & (sizeof(struct ctrl_port_msg) >> 8);
	ctrl_port->chan_ctrl_msg.head.hw_head.len_low =
	    0xFF & sizeof(struct ctrl_port_msg);
#endif
	/*High byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID */
	ctrl_port->chan_ctrl_msg.id_hi = msg >> 8;
	/*Low byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID */
	ctrl_port->chan_ctrl_msg.id_low = msg;
	ctrl_port->chan_ctrl_msg.chan_num = chan_num;	/*ChanNum ,same as ChanInfo */
	ctrl_port->chan_ctrl_msg.option = opt;

	return 0;
}

static unsigned char loop_back[12];

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
static unsigned short hear_beat_counter;
static void modem_heart_beat_poll_work(struct work_struct *work)
{
	struct sdio_modem *modem =
	    container_of(work, struct sdio_modem, poll_hb_work);

	struct sdio_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;
#if ENABLE_CCMNI
	int rx_ch = 0;
#endif

	LOGPRT(LOG_NOTICE, "%s: enter\n", __func__);
	if (!modem || (modem->status != MD_READY)) {
		LOGPRT(LOG_ERR, "%s: md not available now\n", __func__);
		goto down_out;
	}

	/*dump ccmni status */
#if ENABLE_CCMNI
	while (rx_ch < CCMNI_INTF_COUNT) {
		ccmni_ops.dump(SDIO_MD_ID, rx_ch, 0);
		rx_ch++;
	}
#endif

	ctrl_port = modem->ctrl_port;

	wait_event(ctrl_port->sflow_ctrl_wait_q,
		   (SFLOW_CTRL_DISABLE ==
		    atomic_read(&ctrl_port->sflow_ctrl_state)
		    || (modem->status != MD_READY)));
	if (down_interruptible(&modem->sem)) {
		LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n", __func__,
		       __LINE__);
		ret = -ERESTARTSYS;
		goto down_out;
	}

	LOGPRT(LOG_ERR, "start heart beat timer %x\n", hear_beat_counter);
	/*start heart beat timer, if this timer expires, and still no response from c2k, we assume c2k is dead. */
	/*when receive heart beat msg from c2k, delete this timer. */
	mod_timer(&modem->heart_beat_timer,
		  jiffies + msecs_to_jiffies(HEART_BEAT_TIMEOUT));

	ret =
	    contruct_ctrl_chan_msg(ctrl_port, HEART_BEAT_MSG_ID,
				   (hear_beat_counter >> 8) & 0xFF,
				   hear_beat_counter & 0xFF);
	hear_beat_counter++;
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s contruct_ctrl_chan_msg failed\n", __func__);
		goto up_sem;
	}
	msg_len = sizeof(struct ctrl_port_msg);
	msg_len = (msg_len + 3) & ~0x03;	/*Round up to nearest multiple of 4 */
	modem_sdio_write(modem, SDIO_WRITE_ADDR, &(ctrl_port->chan_ctrl_msg),
			 msg_len);

	mod_timer(&modem->poll_timer,
		  jiffies + msecs_to_jiffies(POLLING_INTERVAL));

 up_sem:
	up(&modem->sem);
 down_out:
	return;

}

int force_c2k_assert(struct sdio_modem *modem)
{
	struct sdio_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;

	LOGPRT(LOG_INFO, "%s: enter\n", __func__);
	if (!modem || (modem->status != MD_READY)) {
		LOGPRT(LOG_ERR, "%s: md not available now\n", __func__);
		del_timer(&modem->force_assert_timer);
		goto down_out;
	}

	ctrl_port = modem->ctrl_port;

	wait_event(ctrl_port->sflow_ctrl_wait_q,
		   (SFLOW_CTRL_DISABLE ==
		    atomic_read(&ctrl_port->sflow_ctrl_state)
		    || (modem->status != MD_READY)));
	if (down_interruptible(&modem->sem)) {
		LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n", __func__,
		       __LINE__);
		ret = -ERESTARTSYS;
		goto down_out;
	}

	ret = contruct_ctrl_chan_msg(ctrl_port, FORCE_ASSERT_MSG_ID, 0, 0);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s contruct_ctrl_chan_msg failed\n", __func__);
		goto up_sem;
	}
	msg_len = sizeof(struct ctrl_port_msg);
	msg_len = (msg_len + 3) & ~0x03;	/*Round up to nearest multiple of 4 */
	modem_sdio_write(modem, SDIO_WRITE_ADDR, &(ctrl_port->chan_ctrl_msg),
			 msg_len);
 up_sem:
	up(&modem->sem);
 down_out:
	return ret;
}

static void modem_force_assert_work(struct work_struct *work)
{
	struct sdio_modem *modem =
	    container_of(work, struct sdio_modem, force_assert_work);

	force_c2k_assert(modem);
}

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
static void modem_smem_read_done_work(struct work_struct *work)
{
	struct sdio_modem *modem =
	    container_of(work, struct sdio_modem, smem_read_done_work);
	struct sdio_msg_head *head_to_write;

	if (down_interruptible(&modem->sem)) {
		LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n", __func__,
		       __LINE__);
		return;
	}
	modem->curr_log_blk.id = LOG_AP_DATA_DONE_MSG;
	LOGPRT(LOG_INFO, "logging done %x %x %llu\n",
	       modem->curr_log_blk.address,
	       modem->curr_log_blk.length, modem->log_blk_stamp);
	head_to_write = (struct sdio_msg_head *)modem->trans_buffer;
	head_to_write->start_flag = MSG_START_FLAG;
	/*port->index start from 0, chanInfo start from 1, chan0 is ctrl channel. */
	head_to_write->chanInfo = 0x0F & MD_LOG2_CH_ID;
	head_to_write->tranHi = 0x0F & (sizeof(modem->curr_log_blk) >> 8);
	head_to_write->tranLow = 0xFF & sizeof(modem->curr_log_blk);
	head_to_write->hw_head.len_hi =
	    0xFF & ((sizeof(modem->curr_log_blk) + sizeof(struct sdio_msg_head))
		    >> 8);
	head_to_write->hw_head.len_low =
	    0xFF & (sizeof(modem->curr_log_blk) + sizeof(struct sdio_msg_head));

	memcpy(modem->trans_buffer + sizeof(struct sdio_msg_head),
	       &modem->curr_log_blk, sizeof(modem->curr_log_blk));
	/*ccci_mem_dump(-1, modem->trans_buffer, (sizeof(modem->curr_log_blk) + sizeof(struct sdio_msg_head))); */
	modem_sdio_write(modem, SDIO_WRITE_ADDR, modem->trans_buffer,
			 (sizeof(modem->curr_log_blk) +
			  sizeof(struct sdio_msg_head)));
	up(&modem->sem);
}
#endif

static void c2k_heart_beat_timer(unsigned long data)
{
	struct sdio_modem *modem = (struct sdio_modem *)data;

	if (modem->status != MD_READY) {
		LOGPRT(LOG_ERR, "heart beat timer expired, but modem is not ready!\n");
		return;
	}
	LOGPRT(LOG_ERR,
	       "heart beat timer expired, cancel poll timer and force c2k assert now!\n");
	del_timer(&modem->poll_timer);
	schedule_work(&modem->force_assert_work);
	/*
	   start force assert timer, if this timer expires,
	   and still no response from c2k, we should trigger md long time no response exception.
	 */
	/*when receive MD_EX msg from c2k, delete this timer. */
	mod_timer(&modem->force_assert_timer,
		  jiffies + msecs_to_jiffies(FORCE_ASSERT_TIMEOUT));
}

static void c2k_poll_status_timer(unsigned long data)
{
	struct sdio_modem *modem = (struct sdio_modem *)data;

	LOGPRT(LOG_INFO, "poll c2k now!\n");
	schedule_work(&modem->poll_hb_work);
	/*poll_c2k(); */
}

static void c2k_force_assert_timer(unsigned long data)
{
	struct sdio_modem *modem = (struct sdio_modem *)data;
	int db_opt = DB_OPT_DEFAULT;
	char ex_info[EE_BUF_LEN] = "";	/*attention, be careful with string length! */
	char buff[AED_STR_LEN];

	LOGPRT(LOG_ERR,
	       "force assert timer expired, trigger md long time no response now!\n");
	if (!modem || (modem->status != MD_READY)) {
		LOGPRT(LOG_ERR, "%s: md not available now\n", __func__);
		goto down_out;
	}

	dump_c2k_iram();

	strcpy(ex_info, "\n[Others] MD3 long time no response\n");
	db_opt |= DB_OPT_FTRACE;

	snprintf(buff, AED_STR_LEN, "md3:%s%s", ex_info, c2k_img_info_str);

#if defined CONFIG_MTK_AEE_FEATURE
	aed_md_exception_api(NULL, 0, NULL, 0, buff, db_opt);
#endif

 down_out:
	return;
}

#endif				/*CONFIG_EVDO_DT_VIA_SUPPORT */

int modem_on_off_ctrl_chan(unsigned char on)
{
	struct sdio_modem *modem = c2k_modem;
	struct sdio_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;

	LOGPRT(LOG_INFO, "%s: enter, on = %d\n", __func__, on);
	if (!modem || (modem->status != MD_READY)) {
		LOGPRT(LOG_ERR, "%s: md not available now\n", __func__);
		goto down_out;
	}

	ctrl_port = modem->ctrl_port;

	wait_event(ctrl_port->sflow_ctrl_wait_q,
		   (SFLOW_CTRL_DISABLE ==
		    atomic_read(&ctrl_port->sflow_ctrl_state)
		    || (modem->status != MD_READY)));
	if (down_interruptible(&modem->sem)) {
		LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n", __func__,
		       __LINE__);
		ret = -ERESTARTSYS;
		goto down_out;
	}

	ret = contruct_ctrl_chan_msg(ctrl_port, CHAN_ONOFF_MSG_ID, 0, on);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s contruct_ctrl_chan_msg failed\n", __func__);
		goto up_sem;
	}
	msg_len = sizeof(struct ctrl_port_msg);
	msg_len = (msg_len + 3) & ~0x03;	/*Round up to nearest multiple of 4 */
	modem_sdio_write(modem, SDIO_WRITE_ADDR, &(ctrl_port->chan_ctrl_msg),
			 msg_len);

#ifndef	CONFIG_EVDO_DT_VIA_SUPPORT
	if (on) {
		LOGPRT(LOG_INFO, "%s start polling c2k's status now...\n",
		       __func__);
		/*start poll c2k's status now */
		mod_timer(&modem->poll_timer,
			  jiffies + msecs_to_jiffies(POLLING_INTERVAL));
	}
#endif				/*CONFIG_EVDO_DT_VIA_SUPPORT */
 up_sem:
	up(&modem->sem);
 down_out:
	return ret;
}

/*used by USB for ETS*/
int modem_dtr_set(int on, int low_latency)
{
	struct sdio_modem *modem = c2k_modem;
	unsigned long flags;
	int ret = 0;

	spin_lock_irqsave(&modem->status_lock, flags);
	if (modem->status == MD_READY) {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		dtr_value = on;
		if (low_latency)
			modem_dtr_send(&modem->dtr_work);
		else
			schedule_work(&modem->dtr_work);
	} else {
		spin_unlock_irqrestore(&modem->status_lock, flags);
	}
	return ret;
}

void modem_dtr_send(struct work_struct *work)
{
	struct sdio_modem_port *port;
	struct sdio_modem_ctrl_port *ctrl_port;
	unsigned char control_signal = 0;
	unsigned char msg_len = 0;
	int ret = 0;
	unsigned int on = 0;
	struct sdio_modem *modem =
	    container_of(work, struct sdio_modem, dtr_work);

	on = dtr_value;
	LOGPRT(LOG_INFO, "%s: enter, on =%d\n", __func__, on);
	port = sdio_modem_tty_port_get(0);
	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d check_port failed\n", __func__,
		       __LINE__);
		goto down_out;
	}
	modem = port->modem;
	ctrl_port = modem->ctrl_port;

	wait_event(ctrl_port->sflow_ctrl_wait_q,
		   (SFLOW_CTRL_DISABLE ==
		    atomic_read(&ctrl_port->sflow_ctrl_state)
		    || (modem->status == MD_OFF)));
	if (down_interruptible(&modem->sem)) {
		LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n", __func__,
		       __LINE__);
		ret = -ERESTARTSYS;
		goto down_out;
	}

	if (ctrl_port->chan_state == CHAN_ON) {
		if (on)
			control_signal |= 0x04;
		else
			control_signal &= 0xFB;

		ret =
		    contruct_ctrl_chan_msg(ctrl_port, MDM_STATUS_IND_MSG_ID, 2,
					   control_signal);
		if (ret < 0) {
			LOGPRT(LOG_ERR, "%s contruct_ctrl_chan_msg failed\n",
			       __func__);
			goto up_sem;
		}
		msg_len = sizeof(struct ctrl_port_msg);
		msg_len = (msg_len + 3) & ~0x03;	/*Round up to nearest multiple of 4 */
		modem_sdio_write(modem, SDIO_WRITE_ADDR,
				 &(ctrl_port->chan_ctrl_msg), msg_len);
	} else {
		ret = -1;
		LOGPRT(LOG_ERR,
		       "%s: ctrl channel is off, please turn on first\n",
		       __func__);
	}

 up_sem:
	up(&modem->sem);
 down_out:
	return;
}

/*used by USB for ETS*/
int modem_dcd_state(void)
{
	unsigned long flags;
	/*int ret = 0; */
	struct sdio_modem *modem = c2k_modem;

	spin_lock_irqsave(&modem->status_lock, flags);
	if (modem->status == MD_READY) {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		schedule_work(&modem->dcd_query_work);
	} else {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		dcd_state = 0;
	}
	return dcd_state;
}

void modem_dcd_query(struct work_struct *work)
{
	struct sdio_modem *modem =
	    container_of(work, struct sdio_modem, dcd_query_work);
	struct sdio_modem_ctrl_port *ctrl_port;
	struct sdio_modem_port *port = NULL;
	unsigned char msg_len = 0;
	int ret = 0;

	LOGPRT(LOG_NOTICE, "%s: enter\n", __func__);
	/*why use tty port 0???? haow */
	port = sdio_modem_tty_port_get(0);
	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d check_port failed\n", __func__,
		       __LINE__);
		goto down_out;
	}

	ctrl_port = modem->ctrl_port;

	if (ctrl_port->chan_state == CHAN_ON) {
		wait_event(ctrl_port->sflow_ctrl_wait_q,
			   (SFLOW_CTRL_DISABLE ==
			    atomic_read(&ctrl_port->sflow_ctrl_state)
			    || (modem->status != MD_READY)));
		if (down_interruptible(&modem->sem)) {
			LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n",
			       __func__, __LINE__);
			ret = -ERESTARTSYS;
			goto down_out;
		}

		ret = contruct_ctrl_chan_msg(ctrl_port, MDM_STATUS_QUERY_MSG_ID, 2, 0);	/*why use channel 2??? haow */
		if (ret < 0) {
			LOGPRT(LOG_ERR, "%s contruct_ctrl_chan_msg failed\n",
			       __func__);
			goto up_sem;
		}
		msg_len = sizeof(struct ctrl_port_msg);
		msg_len = (msg_len + 3) & ~0x03;	/*Round up to nearest multiple of 4 */
		modem_sdio_write(modem, SDIO_WRITE_ADDR,
				 &(ctrl_port->chan_ctrl_msg), msg_len);
 up_sem:
		up(&modem->sem);
		msleep(20);
		dcd_state = port->dtr_state;	/*why?? */
	} else {
		dcd_state = 0;
		LOGPRT(LOG_ERR,
		       "%s: ctrl channel is off, please turn on first\n",
		       __func__);
	}
 down_out:
	return;
}

int modem_loop_back_chan(unsigned char chan_num, unsigned char opt)
{
	struct sdio_modem *modem = c2k_modem;
	struct sdio_modem_ctrl_port *ctrl_port;
	unsigned char msg_len = 0;
	int ret = 0;

	LOGPRT(LOG_NOTICE, "%s: enter\n", __func__);

	ctrl_port = modem->ctrl_port;
	wait_event(ctrl_port->sflow_ctrl_wait_q,
		   (SFLOW_CTRL_DISABLE ==
		    atomic_read(&ctrl_port->sflow_ctrl_state)
		    || (modem->status == MD_OFF)));
	if (down_interruptible(&modem->sem)) {
		LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n", __func__,
		       __LINE__);
		ret = -ERESTARTSYS;
		goto down_out;
	}

	if (ctrl_port->chan_state == CHAN_ON) {
		loop_back[0] = MSG_START_FLAG;
		loop_back[1] = 0;
		loop_back[2] = 0;	/*High byte of the following payload length */
		loop_back[3] = 6;	/*Low byte of the following payload length */
		loop_back[4] = 0x00;	/*High byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID */
		loop_back[5] = 0x06;	/*Low byte of control message ID,for onoff request ID=CHAN_ONOFF_MSG_ID */
		loop_back[6] = 3;	/*device id sdio = 3 */
		loop_back[7] = opt;
		loop_back[8] = chan_num;	/*ChanNum ,same as ChanInfo */
		loop_back[9] = 0;

		msg_len = 12;
		msg_len = (msg_len + 3) & ~0x03;	/*Round up to nearest multiple of 4 */
		modem_sdio_write(modem, SDIO_WRITE_ADDR, &(loop_back[0]),
				 msg_len);
	} else {
		ret = -1;
		LOGPRT(LOG_ERR,
		       "%s: ctrl channel is off, please turn on first\n",
		       __func__);
	}
	up(&modem->sem);
 down_out:
	return ret;
}

static int ctrl_msg_analyze(struct sdio_modem *modem)
{
	struct sdio_modem_ctrl_port *ctrl_port;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	struct ctrl_port_msg *msg =
	    (struct ctrl_port_msg *)modem->as_packet->buffer;
#else
	struct ctrl_port_msg *msg = modem->msg;
#endif

	unsigned int msg_id = (msg->id_hi << 8) + msg->id_low;
	unsigned char option = msg->option;
	unsigned char chan_num = msg->chan_num;

	unsigned char tty_port_idx = 0;
	struct sdio_modem_port *port;
	unsigned char res;

	ctrl_port = modem->ctrl_port;

	switch (msg_id) {
	case CHAN_ONOFF_MSG_ID:
		if (option == 1) {
			ctrl_port->chan_state = CHAN_ON;
			LOGPRT(LOG_INFO, "%s: ctrl channel is open\n",
			       __func__);
		} else if (option == 0) {
			ctrl_port->chan_state = CHAN_OFF;
			LOGPRT(LOG_INFO, "%s: ctrl channel is close\n",
			       __func__);
		} else {
			LOGPRT(LOG_ERR, "%s: err option value = %d\n",
			       __func__, option);
		}
		break;
	case MDM_STATUS_IND_MSG_ID:
		port = modem->port[0];	/*why use tty port 0??? haow */
		if (option & 0x80) {	/*connect */
			port->dtr_state = 1;
		} else {	/*disconnect */

			port->dtr_state = 0;
		}
		break;
	case MDM_STATUS_QUERY_MSG_ID:
		port = modem->port[0];	/*why use tty port 0??? haow */
		if (option & 0x80) {	/*connect */
			port->dtr_state = 1;
		} else {	/*disconnect */

			port->dtr_state = 0;
		}
		/*to be contionue */
		break;
	case CHAN_LOOPBACK_TST_MSG_ID:
		{
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
			chan_num =
			    *(modem->as_packet->buffer +
			      sizeof(struct sdio_msg_head) + 4);
			res =
			    *(modem->as_packet->buffer +
			      sizeof(struct sdio_msg_head) + 5);
#else
			chan_num = *(modem->msg->buffer + 4);
			res = *(modem->msg->buffer + 5);
#endif
			if (option == OPT_LOOPBACK_OPEN) {	/*open */
				LOGPRT(LOG_NOTICE,
				       "%s %d: open chan %d, result = %d\n",
				       __func__, __LINE__, chan_num, res);
			} else if (option == OPT_LOOPBACK_CLOSE) {	/*close */
				LOGPRT(LOG_NOTICE,
				       "%s %d: close chan %d, result = %d\n",
				       __func__, __LINE__, chan_num, res);
			} else if (option == OPT_LOOPBACK_QUERY) {	/*close */
				LOGPRT(LOG_NOTICE,
				       "%s %d: query chan %d, result = %d\n",
				       __func__, __LINE__, chan_num, res);
			} else {
				LOGPRT(LOG_ERR, "%s %d: unknown option %d\n",
				       __func__, __LINE__, option);
			}
		}
		break;
	case FLOW_CONTROL_MSG_ID:
		chan_num = msg->chan_num;
		if (chan_num > 0 && chan_num < (SDIO_TTY_NR + 1)) {
			tty_port_idx = chan_num - 1;
			port = modem->port[tty_port_idx];
			if (option == SFLOW_CTRL_ENABLE) {
				LOGPRT(LOG_INFO,
				       "%s %d: channel%d soft flow ctrl enable!\n",
				       __func__, __LINE__, (port->index + 1));
				atomic_set(&port->sflow_ctrl_state,
					   SFLOW_CTRL_ENABLE);
			} else if (option == SFLOW_CTRL_DISABLE) {
				LOGPRT(LOG_INFO,
				       "%s %d: channel%d soft flow ctrl disable!\n",
				       __func__, __LINE__, (port->index + 1));
				atomic_set(&port->sflow_ctrl_state,
					   SFLOW_CTRL_DISABLE);
				wake_up(&port->sflow_ctrl_wait_q);
			}
		} else if (chan_num == 0) {
			if (option == SFLOW_CTRL_ENABLE) {
				LOGPRT(LOG_INFO,
				       "%s %d: ctrl channel soft flow ctrl enable!\n",
				       __func__, __LINE__);
				atomic_set(&ctrl_port->sflow_ctrl_state,
					   SFLOW_CTRL_ENABLE);
			} else if (option == SFLOW_CTRL_DISABLE) {
				LOGPRT(LOG_INFO,
				       "%s %d: ctrl channel soft flow ctrl disable!\n",
				       __func__, __LINE__);
				atomic_set(&ctrl_port->sflow_ctrl_state,
					   SFLOW_CTRL_DISABLE);
				wake_up(&ctrl_port->sflow_ctrl_wait_q);
			}
		} else {
			LOGPRT(LOG_ERR, "%s %d: unknown channel num%d!\n",
			       __func__, __LINE__, chan_num);
		}
		break;
	case CHAN_SWITCH_REQ_MSG_ID:
		/*to be contionue */
		break;
	case CHAN_STATUS_QUERY_MSG_ID:
		/*to be contionue */
		break;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	case HEART_BEAT_MSG_ID:
		LOGPRT(LOG_INFO, "heart beat msg received %x\n",
		       (msg->chan_num << 8) | msg->option);
		del_timer(&modem->heart_beat_timer);
		break;
#endif
	default:
		LOGPRT(LOG_ERR, "%s %d: unknown control message received\n",
		       __func__, __LINE__);
		goto err_wrong_format;
	}
	return 0;

 err_wrong_format:
	return -1;
}

#if !ENABLE_CHAR_DEV
static void sdio_buffer_in_print(struct sdio_modem_port *port,
				 struct sdio_buf_in_packet *packet)
{
	unsigned int count;
	int i;

	pr_debug("[MODEM SDIO] sdio channel%d buffer in %d bytes data<<",
		 (port->index + 1), packet->size);
	count = packet->size;
	if (count > 20)
		count = 20;

	for (i = 0; i < count; i++)
		pr_debug("%x-", *(packet->buffer + i));

	pr_debug("\n");
}

/*when tty port is closed, modem may send data to the corresponding channel too. We buffered those data.*/
/*So, when tty port opened, we should push those buffered data to user.*/
static void sdio_buf_in_tty_work(struct sdio_modem_port *port)
{
	struct sdio_buf_in_packet *packet = NULL;
	struct tty_struct *tty;
	int room;

	tty = tty_port_tty_get(&port->port);
	if (tty) {
		while (!list_empty(&port->sdio_buf_in_list)) {
			packet =
			    list_first_entry(&port->sdio_buf_in_list,
					     struct sdio_buf_in_packet, node);

			room =
			    tty_buffer_request_room(&port->port, packet->size);

			if (room < packet->size) {
				LOGPRT(LOG_ERR,
				       "%s %d: no room in tty rx buffer!\n",
				       __func__, __LINE__);
			} else {
				room =
				    tty_insert_flip_string(&port->port,
							   packet->buffer,
							   packet->size);

				if (room < packet->size) {
					LOGPRT(LOG_ERR,
					       "%s %d: couldn't insert all characters (TTY is full?)!\n",
					       __func__, __LINE__);
				} else {

					tty_flip_buffer_push(&port->port);
				}
			}

			sdio_buffer_in_print(port, packet);

			list_del(&packet->node);
			if (packet) {
				port->sdio_buf_in_size -= packet->size;
				kfree(packet->buffer);
				kfree(packet);
			}
			port->sdio_buf_in_num--;
		}
	}
	tty_kref_put(tty);
}

/*****************************************************************************
 *tty driver interface functions
 *****************************************************************************/
/**
 *sdio_uart_install	-	install method
 *@driver: the driver in use (sdio_uart in our case)
 *@tty: the tty being bound
 *
 *Look up and bind the tty and the driver together. Initialize
 *any needed private data (in our case the termios)
 */

static int modem_tty_install(struct tty_driver *driver, struct tty_struct *tty)
{
	struct sdio_modem_port *port;
	int idx = tty->index;
	int ret;

	port = sdio_modem_tty_port_get(idx);
	LOGPRT(LOG_INFO, "%s %d: port %d.\n", __func__, __LINE__, port->index);
	if (!port) {
		tty->driver_data = NULL;
		LOGPRT(LOG_ERR, "%s %d can't find sdio modem port.\n", __func__,
		       __LINE__);
		return -ENODEV;
	}

	kref_get(&port->kref);

	ret = tty_port_install(&port->port, driver, tty);

	if (ret == 0)
		/*This is the ref sdio_uart_port get provided */
		tty->driver_data = port;
	else
		sdio_modem_tty_port_put(port);
	return ret;
}

/**
 *sdio_uart_cleanup	-	called on the last tty kref drop
 *@tty: the tty being destroyed
 *
 *Called asynchronously when the last reference to the tty is dropped.
 *We cannot destroy the tty->driver_data port kref until this point
 */

static void modem_tty_cleanup(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;

	tty->driver_data = NULL;	/*Bug trap */
	if (port) {
		LOGPRT(LOG_INFO, "%s %d: port %d.\n", __func__, __LINE__,
		       port->index);
		sdio_modem_tty_port_put(port);
	} else {
		LOGPRT(LOG_ERR, "%s %d: invalid port.\n", __func__, __LINE__);
	}
}

static int modem_tty_open(struct tty_struct *tty, struct file *filp)
{
	struct sdio_modem_port *port = tty->driver_data;

	LOGPRT(LOG_INFO, "%s %d: port %d.\n", __func__, __LINE__, port->index);
	return tty_port_open(&port->port, tty, filp);
}

static void modem_tty_close(struct tty_struct *tty, struct file *filp)
{
	struct sdio_modem_port *port = tty->driver_data;

	LOGPRT(LOG_INFO, "%s %d: port %d.\n", __func__, __LINE__, port->index);
	tty_port_close(&port->port, tty, filp);
}

static void modem_tty_hangup(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;

	LOGPRT(LOG_INFO, "%s %d: port %d.\n", __func__, __LINE__, port->index);
	tty_port_hangup(&port->port);
}

static int modem_tty_write(struct tty_struct *tty, const unsigned char *buf,
			   int count)
{
	struct sdio_modem_port *port = tty->driver_data;
	struct sdio_modem *modem = c2k_modem;
	unsigned long flags;
	int ret = 0;

	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d check_port failed\n", __func__,
		       __LINE__);
		return ret;
	}

	if (port->inception)
		return -EBUSY;

	if (count > FIFO_SIZE) {
		LOGPRT(LOG_ERR, "%s %d FIFO size is not enough!\n", __func__,
		       __LINE__);
		return -1;
	}

	spin_lock_irqsave(&modem->status_lock, flags);
	if (modem->status != MD_OFF) {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		ret =
		    kfifo_in_locked(&port->transmit_fifo, buf, count,
				    &port->write_lock);
		queue_work(port->write_q, &port->write_work);
	} else {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		LOGPRT(LOG_ERR, "%s %d: port%d is removed!\n", __func__,
		       __LINE__, port->index);
	}

	LOGPRT(LOG_DEBUG, "%s %d: port%d\n", __func__, __LINE__, port->index);

	return ret;
}

static int modem_tty_write_room(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	unsigned long flags = 0;
	unsigned int data_len = 0;
	int ret;

	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d check_port failed\n", __func__,
		       __LINE__);
		return ret;
	}

	spin_lock_irqsave(&port->write_lock, flags);
	data_len = FIFO_SIZE - kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	LOGPRT(LOG_DEBUG, "%s %d: port %d free size %d.\n", __func__, __LINE__,
	       port->index, data_len);
	return data_len;
}

#if 0
static void modem_tty_flush_chars(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	struct sdio_modem *modem;
	unsigned int count;
	unsigned int left, todo;
	unsigned int write_len;
	unsigned int fifo_size;
	unsigned long flags = 0;
	int ret = 0;

	modem = port->modem;
	if (down_interruptible(&modem->sem)) {
		LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n", __func__,
		       __LINE__);
		ret = -ERESTARTSYS;
		goto down_out;
	}
	spin_lock_irqsave(&port->write_lock, flags);
	count = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	if (count == 0) {
		up(&modem->sem);
		goto down_out;
	}

	left = count;
	do {
		todo = left;
		if (todo > TRANSMIT_MAX_SIZE - 1)
			todo = TRANSMIT_MAX_SIZE;
		else if (todo > 508)
			todo = 508;

		*modem->trans_buffer = MSG_START_FLAG;
		*(modem->trans_buffer + 1) = 0x0F & (port->index + 1);
		*(modem->trans_buffer + 2) = 0x0F & (todo >> 8);
		*(modem->trans_buffer + 3) = 0xFF & todo;

		fifo_size =
		    kfifo_out_locked(&port->transmit_fifo,
				     modem->trans_buffer + 4, todo,
				     &port->write_lock);
		if (todo != fifo_size) {
			LOGPRT(LOG_ERR,
			       "%s %d: port%d todo !=  kfifo lock out size.\n",
			       __func__, __LINE__, port->index);
			todo = fifo_size;
		}

		write_len = (todo + 4 + 3) & ~0x03;	/*Round up to nearest multiple of 4 */
		modem_sdio_write(modem, SDIO_WRITE_ADDR, modem->trans_buffer,
				 write_len);
		left -= todo;
	} while (left);
	up(&modem->sem);

 down_out:
	/*for compile warning */
	LOGPRT(LOG_DEBUG, "%s %d: port%d.\n", __func__, __LINE__, port->index);
	ret = ret;
}
#endif
static int modem_tty_chars_in_buffer(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	struct sdio_modem *modem = port->modem;
	unsigned long flags = 0;
	unsigned int data_len = 0;
	int ret;

	/*return 0 when modem is off, because tty driver will wait 30s until chars is buffer return valid value. */
	/*if exit flight mode, flashless will take 30s to exit. and exit flight mode time will be too long. */
	if (modem && (modem->status == MD_OFF))
		return 0;

	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}

	spin_lock_irqsave(&port->write_lock, flags);
	data_len = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	LOGPRT(LOG_DEBUG, "%s %d: port %d chars in buffer %d.\n", __func__,
	       __LINE__, port->index, data_len);
	return data_len;
}

static void modem_tty_set_termios(struct tty_struct *tty,
				  struct ktermios *old_termios)
{
	struct sdio_modem_port *port = tty->driver_data;
	int ret;

	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d ret=%d\n", __func__, __LINE__, ret);
		return;
	}

	tty_termios_copy_hw(&tty->termios, old_termios);

}

static int modem_tty_tiocmget(struct tty_struct *tty)
{
	struct sdio_modem_port *port = tty->driver_data;
	int ret;

	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}
	return 0;
}

static int modem_tty_tiocmset(struct tty_struct *tty,
			      unsigned int set, unsigned int clear)
{
	struct sdio_modem_port *port = tty->driver_data;
	int ret;

	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d ret=%d\n", __func__, __LINE__, ret);
		return ret;
	}
	return 0;
}

/*called from tty_port_open*/
static int sdio_modem_activate(struct tty_port *tport, struct tty_struct *tty)
{
	struct sdio_modem_port *port = NULL;

	LOGPRT(LOG_INFO, "%s %d: enter.\n", __func__, __LINE__);
	port = container_of(tport, struct sdio_modem_port, port);

	kfifo_reset(&port->transmit_fifo);
	mutex_lock(&port->sdio_buf_in_mutex);
	if (port->sdio_buf_in == 1) {
		sdio_buf_in_tty_work(port);	/*maybe pending data in buffer, push to user */
		port->sdio_buf_in = 0;
	}
	mutex_unlock(&port->sdio_buf_in_mutex);

	/*
	 *If not set this flag, when user's writing size exceeds 2048,
	 *tty will split those data into 2048 + left size.
	 */
	set_bit(TTY_NO_WRITE_SPLIT, &tty->flags);

	LOGPRT(LOG_INFO, "%s %d: Leave.\n", __func__, __LINE__);
	return 0;
}

/*called when the last close completes or a hangup finishes*/
static void sdio_modem_shutdown(struct tty_port *tport)
{
	struct sdio_modem_port *port = NULL;
	struct sdio_buf_in_packet *packet = NULL;

	LOGPRT(LOG_INFO, "%s %d: enter.\n", __func__, __LINE__);
	port = container_of(tport, struct sdio_modem_port, port);
	mutex_lock(&port->sdio_buf_in_mutex);
	/*clear pending data */
	while (!list_empty(&port->sdio_buf_in_list)) {
		packet =
		    list_first_entry(&port->sdio_buf_in_list,
				     struct sdio_buf_in_packet, node);
		list_del(&packet->node);
		if (packet) {
			kfree(packet->buffer);
			kfree(packet);
		}
	}
	mutex_unlock(&port->sdio_buf_in_mutex);
	LOGPRT(LOG_INFO, "%s %d: Leave.\n", __func__, __LINE__);
}

static const struct tty_port_operations sdio_modem_port_ops = {
	.shutdown = sdio_modem_shutdown,
	.activate = sdio_modem_activate,
};

static const struct tty_operations modem_tty_ops = {
	.open = modem_tty_open,
	.close = modem_tty_close,
	.write = modem_tty_write,
	.write_room = modem_tty_write_room,
	.chars_in_buffer = modem_tty_chars_in_buffer,
	/*.flush_chars          = modem_tty_flush_chars, */
	.set_termios = modem_tty_set_termios,
	.tiocmget = modem_tty_tiocmget,
	.tiocmset = modem_tty_tiocmset,
	.hangup = modem_tty_hangup,
	.install = modem_tty_install,
	.cleanup = modem_tty_cleanup,
};
#endif

#if ENABLE_CCMNI
/*When send data to ccmni port, we should analyze msg header first, so we separate ccmni port work from others*/
static void sdio_write_ccmni_work(struct work_struct *work)
{
	struct sdio_modem_port *ccmni_port = NULL;
	struct sdio_modem *modem = NULL;
	unsigned int left = 0;
	unsigned int data_len = 0;
	unsigned long flags = 0;
	unsigned int fifo_total_count = 0;
	unsigned int todo = 0;
	unsigned int fifo_read_size = 0;
	unsigned int tx_ch = 0;
	unsigned int write_len = 0;
	struct sdio_msg_head msg_head;
	struct sdio_msg_head *head_to_write;
	struct sk_buff *skb = NULL;
	int ret = 0;

	ccmni_port =
	    container_of(work, struct sdio_modem_port, write_ccmni_work);
	modem = ccmni_port->modem;

	LOGPRT(LOG_NOTICE, "%s %d enter.\n", __func__, __LINE__);

	if (down_interruptible(&ccmni_port->write_sem)) {
		LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n", __func__,
		       __LINE__);
		ret = -ERESTARTSYS;
		goto down_out;
	}

	spin_lock_irqsave(&ccmni_port->write_lock, flags);
	fifo_total_count = kfifo_len(&ccmni_port->transmit_fifo);
	spin_unlock_irqrestore(&ccmni_port->write_lock, flags);

 retry_get_skb:
	if (ccmni_port->index == CCMNI_AP_LOOPBACK_CH - 1) {
		/*for loopback */
		LOGPRT(LOG_INFO, "%s %d request skb from kernel.\n", __func__,
		       __LINE__);

		skb = dev_alloc_skb(1500);
		if (!skb) {
			msleep(100);
			goto retry_get_skb;
		}
		LOGPRT(LOG_INFO, "%s %d got skb.\n", __func__, __LINE__);
	}
	while (fifo_total_count > 0) {

		todo = sizeof(struct sdio_msg_head);

		fifo_read_size =
		    kfifo_out_locked(&ccmni_port->transmit_fifo, &msg_head,
				     todo, &ccmni_port->write_lock);

		fifo_total_count -= fifo_read_size;

		if (fifo_read_size != todo) {
			LOGPRT(LOG_ERR,
			       "%s %d: fail to get msg head size, just got %d bytes.\n",
			       __func__, __LINE__, fifo_read_size);
			/*todo */
		}

		LOGPRT(LOG_DEBUG,
		       "%s %d read %d bytes, check header!(0x%x, 0x%x)\n",
		       __func__, __LINE__, fifo_read_size, msg_head.start_flag,
		       msg_head.chanInfo);
		/*sanity check */
		if ((msg_head.start_flag != 0xFE)
		    || ((msg_head.chanInfo & 0x0F) != ccmni_port->index + 1)) {
			LOGPRT(LOG_ERR,
			       "%s %d check head fail when write.(0x%x, 0x%x)\n",
			       __func__, __LINE__, msg_head.start_flag,
			       msg_head.chanInfo);
			ret = -1;
			goto head_err_out;
		}

		data_len = (((msg_head.tranHi & 0x0F) << 8) |
			    (msg_head.tranLow & 0xFF));

		tx_ch = (msg_head.chanInfo & 0xF0) >> 4;

		LOGPRT(LOG_DEBUG,
		       "%s %d: sdio head 0x%x, ch%d, tx_id%d, len%d\n",
		       __func__, __LINE__, msg_head.start_flag,
		       msg_head.chanInfo & 0x0F, tx_ch, data_len);

		left = data_len;

		if (ccmni_port->index != CCMNI_AP_LOOPBACK_CH - 1) {
			wait_event(ccmni_port->sflow_ctrl_wait_q,
				   (SFLOW_CTRL_DISABLE ==
				    atomic_read(&ccmni_port->sflow_ctrl_state)
				    || (modem->status == MD_OFF)));
		} else {	/*just for AP loopback */
			LOGPRT(LOG_INFO, "%s: data from ccmni...\n", __func__);
		}

		if (down_interruptible(&modem->sem)) {
			LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n",
			       __func__, __LINE__);
			ret = -ERESTARTSYS;
			goto down_sem_fail;
		}

		do {
			todo = left;

			if (todo > TRANSMIT_MAX_SIZE)
				todo = TRANSMIT_MAX_SIZE;
#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
			if (todo > 508)
				todo = 508;
#endif
			head_to_write =
			    (struct sdio_msg_head *)modem->trans_buffer;
			head_to_write->start_flag = MSG_START_FLAG;
			/*port->index start from 0, chanInfo start from 1, chan0 is ctrl channel. */
			head_to_write->chanInfo =
			    (0x0F & (ccmni_port->index + 1)) +
			    (0xF0 & (tx_ch << 4));
			head_to_write->tranHi = 0x0F & (todo >> 8);
			head_to_write->tranLow = 0xFF & todo;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
			head_to_write->hw_head.len_hi =
			    0xFF & ((todo + sizeof(struct sdio_msg_head)) >> 8);
			head_to_write->hw_head.len_low =
			    0xFF & (todo + sizeof(struct sdio_msg_head));
#endif
			/*to indicate there is data left, not whole packet */
			if (left > todo)
				head_to_write->tranHi |= MORE_DATA_FOLLOWING;

			fifo_read_size =
			    kfifo_out_locked(&ccmni_port->transmit_fifo,
					     modem->trans_buffer +
					     sizeof(struct sdio_msg_head), todo,
					     &ccmni_port->write_lock);

			LOGPRT(LOG_DEBUG, "%s %d: fifo_read_size(%d).\n",
			       __func__, __LINE__, fifo_read_size);

			if (todo != fifo_read_size) {
				LOGPRT(LOG_ERR,
				       "%s %d: port%d todo(%d) !=  kfifo_lock_out size(%d).\n",
				       __func__, __LINE__, ccmni_port->index,
				       todo, fifo_read_size);
				todo = fifo_read_size;
			}
			/*Round up to nearest multiple of 4 */
			write_len =
			    (todo + sizeof(struct sdio_msg_head) + 3) & ~0x03;

			/*for loop back */
			if (ccmni_port->index == CCMNI_AP_LOOPBACK_CH - 1) {
				memcpy(skb_put(skb, todo),
				       modem->trans_buffer +
				       sizeof(struct sdio_msg_head), todo);
				/*the last packet of skb received */
				if ((head_to_write->tranHi & 0x20) == 0) {
					LOGPRT(LOG_INFO,
					       "%s: data loopback to ccmni...\n",
					       __func__);
					sdio_tx_rx_printk(skb, 1);
					ccmni_ops.rx_callback(SDIO_MD_ID, tx_ch,
							      skb, NULL);
				}
			} else {
				LOGPRT(LOG_DEBUG,
				       "%s %d: port%d sending to md(len %d).\n",
				       __func__, __LINE__, ccmni_port->index,
				       write_len);
				modem_sdio_write(modem, SDIO_WRITE_ADDR,
						 modem->trans_buffer,
						 write_len);
			}
			left -= todo;
		} while (left);
		fifo_total_count -= data_len;

		spin_lock_irqsave(&ccmni_port->tx_state_lock, flags);
		if (ccmni_port->tx_state == CCMNI_TX_STOP) {
			if (kfifo_len(&ccmni_port->transmit_fifo) <= FIFO_SIZE/2) {
				/*resume tx queue */
				LOGPRT(LOG_INFO, "ccmni(ch:%d) is resumed.\n",
				       tx_ch);
				ccmni_ops.md_state_callback(SDIO_MD_ID, tx_ch,
							    TX_IRQ, 0);
				ccmni_port->tx_state = CCMNI_TX_READY;
			} else {
				LOGPRT(LOG_INFO,
				       "ccmni(ch:%d) is stopped, fifo_total_count(%d).\n",
				       tx_ch, fifo_total_count);
			}
		}
		spin_unlock_irqrestore(&ccmni_port->tx_state_lock, flags);

		up(&modem->sem);
	}
 head_err_out:

 down_sem_fail:
	up(&ccmni_port->write_sem);
 down_out:
	/*for compile warning */
	return;

}

#endif

static void sdio_write_port_work(struct work_struct *work)
{
	struct sdio_modem_port *port;
	struct sdio_modem *modem;
	struct sdio_msg_head *msg_head;
	/*struct tty_struct *tty; */
	unsigned int count;
	unsigned int left, todo;
	unsigned int write_len;
	unsigned int fifo_size;
	unsigned long flags = 0;
	unsigned int ready_data_count = 0;
	int ret = 0;

	port = container_of(work, struct sdio_modem_port, write_work);
	modem = port->modem;

	if (down_interruptible(&port->write_sem)) {
		LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n", __func__,
		       __LINE__);
		ret = -ERESTARTSYS;
		goto down_out;
	}
	spin_lock_irqsave(&port->write_lock, flags);
	ready_data_count = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

	if ((port->index == (EXCP_CTRL_CH_ID - 1))
	    || (port->index == (EXCP_MSG_CH_ID - 1))) {
		LOGPRT(LOG_INFO, "port%d write work sched(len%d,fc%d)\n",
		       port->index, ready_data_count,
		       atomic_read(&port->sflow_ctrl_state));
	}

	while (ready_data_count > 0) {
		/*for AT command problem of /r; */
		count = ready_data_count;
		if (count == 0) {
			up(&port->write_sem);
			goto down_out;
			/*md side buffer max size is 5200, so we set this limitation */
		} else if (count > ONE_PACKET_MAX_SIZE) {
			LOGPRT(LOG_DEBUG,
			       "%s %d more than packet limit data in fifo (%d)...\n",
			       __func__, __LINE__, count);
			if (port->index == (DATA_CH_ID - 1))
				count = ONE_PACKET_MAX_SIZE;
		}

		left = count;
		wait_event(port->sflow_ctrl_wait_q,
			   (SFLOW_CTRL_DISABLE ==
			    atomic_read(&port->sflow_ctrl_state)
			    || (modem->status == MD_OFF)));
		/*make sure whole packet sent to modem without disturbance. */
		if (down_interruptible(&modem->sem)) {
			LOGPRT(LOG_ERR, "%s %d down_interruptible failed.\n",
			       __func__, __LINE__);
			ret = -ERESTARTSYS;
			goto down_sem_fail;
		}
		do {
			todo = left;
			if (todo > TRANSMIT_MAX_SIZE)
				todo = TRANSMIT_MAX_SIZE;

#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
			if (todo > 508)
				todo = 508;

#endif

			msg_head = (struct sdio_msg_head *)modem->trans_buffer;

			msg_head->start_flag = MSG_START_FLAG;
			/*port->index start from 0, chanInfo start from 1, chan0 is ctrl channel. */
			msg_head->chanInfo = 0x0F & (port->index + 1);
			msg_head->tranHi = 0x0F & (todo >> 8);
			if (port->index >= SDIO_AT4_CHANNEL_NUM - 1)
				msg_head->tranHi |= EXTEND_CH_BIT;
			msg_head->tranLow = 0xFF & todo;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
			msg_head->hw_head.len_hi =
			    0xFF & ((todo + sizeof(struct sdio_msg_head)) >> 8);
			msg_head->hw_head.len_low =
			    0xFF & (todo + sizeof(struct sdio_msg_head));
#endif
			fifo_size =
			    kfifo_out_locked(&port->transmit_fifo,
					     modem->trans_buffer +
					     sizeof(struct sdio_msg_head), todo,
					     &port->write_lock);
			if (todo != fifo_size) {
				LOGPRT(LOG_ERR,
				       "%s %d: port%d todo(%d) !=  kfifo lock out size(%d).\n",
				       __func__, __LINE__, port->index, todo,
				       fifo_size);
				todo = fifo_size;
			}
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
			if (fifo_size < left)
				msg_head->tranHi |= MORE_DATA_FOLLOWING;
#endif
			/*Round up to nearest multiple of 4 */
			write_len =
			    (todo + sizeof(struct sdio_msg_head) + 3) & ~0x03;
			LOGPRT(LOG_DEBUG, "%s %d write %d bytes.\n", __func__,
			       __LINE__, write_len);
			modem_sdio_write(modem, SDIO_WRITE_ADDR,
					 modem->trans_buffer, write_len);
			left -= todo;

		} while (left);
		up(&modem->sem);
		ready_data_count -= count;
		LOGPRT(LOG_DEBUG, "%s %d data count %d, just send %d.\n",
		       __func__, __LINE__, ready_data_count, left);
	}

	spin_lock_irqsave(&port->write_lock, flags);
	count = kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);

#if !ENABLE_CHAR_DEV
	if (count < SDIO_WAKEUP_CHARS) {
		tty = tty_port_tty_get(&port->port);
		if (tty) {
			/*to inform the line discipline that driver is ready to receive more output data */
			tty_wakeup(tty);
			tty_kref_put(tty);	/*tty_port_tty_get() has get reference of tty, should release it */
		}
	}
#endif

 down_sem_fail:
	up(&port->write_sem);
 down_out:
	return;
}

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT

static int sdio_func1_wr(struct sdio_func *func, unsigned int addr, void *src,
			 int count)
{
	int ret = 0;
	unsigned int cnt = 500;

	while (cnt--) {
		ret = sdio_writesb(func, addr, src, count);
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: write 0x%x failed ret=%d, retry\n",
			       __func__, __LINE__, addr, ret);
			msleep(20);
			continue;
		}
		return 0;
	}
	LOGPRT(LOG_ERR, "%s %d: write 0x%x failed ret=%d\n", __func__, __LINE__,
	       addr, ret);
	return -1;

}

static int sdio_func1_rd(struct sdio_func *func, void *dst, unsigned int addr,
			 int count)
{
	int ret = 0;
	unsigned int cnt = 500;

	while (cnt--) {
		ret = sdio_readsb(func, dst, addr, count);
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: read 0x%x failed ret=%d, retry\n",
			       __func__, __LINE__, addr, ret);
			msleep(20);
			continue;
		}
		return 0;
	}
	LOGPRT(LOG_ERR, "%s %d: read 0x%x failed ret=%d\n", __func__, __LINE__,
	       addr, ret);
	return -1;
}

int sdio_pio_rx_pkt(struct sdio_func *func, char *rx_buf, int pkt_len)
{
	int blk_sz = DEFAULT_BLK_SIZE;
	int rx_len;
	int ret = 0;
	/*int cnt = 0; */

	if (pkt_len > RX_FIFO_SZ) {
		LOGPRT(LOG_ERR, "rx pkt len %d bytes > DEV_RX_FIFO_SZ:%d\n",
		       pkt_len, RX_FIFO_SZ);
		return -1;
	}

	if (pkt_len == 0) {
		LOGPRT(LOG_ERR, "!!!! rx pkt len %d bytes !!!!\n", pkt_len);
		return -1;
	}

	if (pkt_len < blk_sz) {
#if PADDING_BY_BLOCK_SIZE
		rx_len = blk_sz;
#else
		rx_len = (pkt_len + 3) / 4 * 4;
#endif
	} else {
		rx_len = (pkt_len + blk_sz - 1) / blk_sz * blk_sz;
	}

	ret = sdio_func1_rd(func, rx_buf, SDIO_CRDR, rx_len);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read rx data failed ret=%d\n", __func__,
		       __LINE__, ret);
	}

	LOGPRT(LOG_NOTICE, "read %d bytes: ", rx_len);

#if USE_CCIF_INTR
	ccif_notify_c2k(RX_DONE_CH);
#endif

#if 0
	int i = 0;

	for (i = 0; i < rx_len; i++)
		LOGPRT(LOG_DEBUG, "%x-", *(rx_buf + i));

	LOGPRT(LOG_NOTICE, "\n");
#endif
	return ret ? (-1) : 0;
}

int sdio_pio_enable_interrupt(struct sdio_func *func)
{
	unsigned char raw_val;
	int err_ret = 0;

	raw_val = sdio_f0_readb(func, 0x04, &err_ret);
	if (err_ret) {
		LOGPRT(LOG_ERR, "%s %d: read 0x04 err ret= %d\n", __func__,
		       __LINE__, err_ret);
		goto err_out;
	}
	LOGPRT(LOG_DEBUG, "%s %d: read addr(0x04) 0x%x\n", __func__, __LINE__,
	       raw_val);

	if (raw_val & (0x1 << 1)) {
		LOGPRT(LOG_DEBUG,
		       "%s %d: The interrupt of pio-based function have been enabled\n",
		       __func__, __LINE__);
		return 0;
	}

	raw_val |= (0x1 << 1);	/*for PIO Only Function number change t0 "1" */
	/*raw_val = 3; */

	sdio_f0_writeb(func, raw_val, 0x04, &err_ret);
	if (err_ret) {
		LOGPRT(LOG_ERR, "%s %d: write 0x04 err ret= %d\n", __func__,
		       __LINE__, err_ret);
		LOGPRT(LOG_ERR,
		       "%s %d: Enable pio-based function's interrupt failed\n",
		       __func__, __LINE__);
		goto err_out;
	} else {
		LOGPRT(LOG_DEBUG,
		       "%s %d: The interrupt of pio-based function enabled success 0x%x\n",
		       __func__, __LINE__, raw_val);
		raw_val = sdio_f0_readb(func, 0x04, &err_ret);
		if (err_ret) {
			LOGPRT(LOG_ERR, "%s %d: read 0x04 err ret= %d\n",
			       __func__, __LINE__, err_ret);
			/*goto err_out; */
		} else
			LOGPRT(LOG_DEBUG, "%s %d: read CCCR success 0x%x\n",
			       __func__, __LINE__, raw_val);
		return 0;
	}

 err_out:
	return -1;
}

int sdio_pio_disable_interrupt(struct sdio_func *func)
{
	unsigned char raw_val;
	int err_ret = 0;

	raw_val = sdio_f0_readb(func, 0x04, &err_ret);
	if (err_ret) {
		LOGPRT(LOG_ERR, "%s %d: read 0x04 err ret= %d\n", __func__,
		       __LINE__, err_ret);
		goto err_out;
	}
	LOGPRT(LOG_DEBUG, "%s %d: read addr(0x04) 0x%x\n", __func__, __LINE__,
	       raw_val);
	if (raw_val & (0x1 << 1)) {
		LOGPRT(LOG_DEBUG,
		       "%s %d: The interrupt of pio-based function have been disabled\n",
		       __func__, __LINE__);
		return 0;
	}

	raw_val &= ~(0x1 << 1);	/*for PIO Only Function number change t0 "1" */

	sdio_f0_writeb(func, raw_val, 0x04, &err_ret);
	if (err_ret) {
		LOGPRT(LOG_ERR, "%s %d: write 0x04 err ret= %d\n", __func__,
		       __LINE__, err_ret);
		LOGPRT(LOG_ERR,
		       "%s %d: Disable pio-based function's interrupt failed\n",
		       __func__, __LINE__);
		goto err_out;
	} else {
		LOGPRT(LOG_DEBUG,
		       "%s %d: The interrupt of pio-based function disabled success (0x%x)\n",
		       __func__, __LINE__, raw_val);
		raw_val = sdio_f0_readb(func, 0x04, &err_ret);
		if (err_ret) {
			LOGPRT(LOG_ERR, "%s %d: read 0x04 err ret= %d\n",
			       __func__, __LINE__, err_ret);
			/*goto err_out; */
		} else
			LOGPRT(LOG_DEBUG, "%s %d: read CCCR success 0x%x\n",
			       __func__, __LINE__, raw_val);
		return 0;
	}
 err_out:
	return -1;
}

void loopback_to_c2k(struct work_struct *work)
{
	/*int ret = 0; */
	struct sdio_modem *modem =
	    container_of(work, struct sdio_modem, loopback_work);
	char *buf = NULL;
	unsigned int len = 0;
	/*int cnt = 0; */
	/*unsigned int timeout = 500; */

	struct sdio_msg_head *head = NULL;
	struct sdio_msg_head *head_orig = NULL;
	int left = 0;
	int todo = 0;
	int transed = 0;
	int write_len = 0;

	LOGPRT(LOG_NOTICE, "%s %d loopback_to_c2k.\n", __func__, __LINE__);

	if (!modem || !modem->as_packet) {
		LOGPRT(LOG_ERR, "%s %d bad parameters.\n", __func__, __LINE__);
		return;
	}
	buf = modem->as_packet->buffer;
	len = modem->as_packet->size;
	head = (struct sdio_msg_head *)modem->trans_buffer;
	head_orig = (struct sdio_msg_head *)buf;

	left = len;

	/*printk("[C2K] loopback_to_c2k 1\n"); */

	do {
		todo = left;
		/*printk("[C2K] loopback_to_c2k 2\n"); */

		/*
		   if(down_interruptible(&modem->sem)){
		   LOGPRT(LOG_ERR,  "%s %d down_interruptible failed.\n", __func__,__LINE__);
		   ret =  -ERESTARTSYS;
		   goto down_sem_fail;
		   } */

		atomic_set(&modem->as_packet->occupied, 1);
		/*printk("[C2K] loopback_to_c2k 3\n"); */
		if (todo > TRANSMIT_MAX_SIZE)
			todo = TRANSMIT_MAX_SIZE;

		head->start_flag = 0xFE;
		head->chanInfo = head_orig->chanInfo;
		head->tranHi = (todo & 0xFF00) >> 8;
		head->tranLow = todo & 0xFF;

		head->hw_head.len_hi =
		    ((todo + sizeof(struct sdio_msg_head)) & 0xFF00) >> 8;
		head->hw_head.len_low =
		    (todo + sizeof(struct sdio_msg_head)) & 0xFF;

		if (left > todo)
			head->tranHi |= 0x20;	/*to indicate there is data left, not whole packet */

		memcpy(modem->trans_buffer + sizeof(struct sdio_msg_head),
		       buf + sizeof(struct sdio_msg_head) + transed, todo);

		atomic_set(&modem->as_packet->occupied, 0);
		write_len = (todo + sizeof(struct sdio_msg_head) + 3) & ~0x03;	/*Round up to nearest multiple of 4 */

		/*
		   printk("[C2K] loopback_to_c2k 4\n");

		   while (atomic_read(&modem->tx_fifo_cnt) < write_len) {
		   msleep(1);
		   cnt ++;

		   if (cnt > timeout) {
		   LOGPRT(LOG_ERR,  "%s write_len=%d wait %dms for TX_FIFO_CNT timeout!\n",
		   __func__, write_len, timeout);
		   return;
		   }
		   } */

		pr_debug("[C2K] write %d back to c2k\n", write_len);

		/*for loop back */
		modem_sdio_write(modem, SDIO_CTDR, modem->trans_buffer,
				 write_len);

		/*atomic_sub(write_len, &modem->tx_fifo_cnt); */

		left -= todo;
		transed += todo;
		/*up(&modem->sem); */
	} while (left);

	atomic_set(&modem->as_packet->occupied, 0);

/*head_err_out:*/

/*down_sem_fail:*/

/*down_out:*/
	/*for compile warning */
	return;

}
#endif

void exception_data_dump(const char *buf, unsigned int len)
{
	const unsigned char *print_buf = (const unsigned char *)buf;
	int i;

	if (!buf || (len <= 0)) {
		LOGPRT(LOG_ERR, "[MODEM SDIO] %s: Bad parameters!\n", __func__);
		goto err_exit;
	}
	LOGPRT(LOG_INFO, "[MODEM SDIO] Exception data dump begin\n");
	for (i = 0; i < len; i++) {
		if (i % 16 == 0)
			pr_debug(" ");

		pr_debug("%02X-", *(print_buf + i));
		if ((i + 1) % 16 == 0)
			pr_debug("\n");
	}
	pr_debug("\n");
	LOGPRT(LOG_INFO, "Exception data dump end\n");

 err_exit:
	return;

}

#define eint_read32(b, a)				ioread32((void __iomem *)((b)+(a)))
#define CIRQ_BASE						(0x10204000)

int dump_c2k_sdio_status(struct sdio_modem *modem)
{
	int ret = 0;
	/*union sdio_pio_int_sts_reg int_sts;*/
	unsigned int con;
	unsigned int eint_reg[][2] = {
		/*address, value */
		{0x0010, 0},
		{0x0050, 0},
		{0x0090, 0},
		{0x00D0, 0},
		{0x0110, 0},
		{0x0150, 0},
		{0x0190, 0},
		{0x01D0, 0},
		{0x0210, 0},
		{0x0250, 0},
		{0x0290, 0},
		{0, 0},		/*the end */
	};

	void __iomem *eint_iobase = ioremap(CIRQ_BASE, 0x400);

	LOGPRT(LOG_ERR, "%s: enter!!\n", __func__);

	/*read CIRQ_CON 0x10204300 */
	con = eint_read32(eint_iobase, 0x300);
	if (con & 0x1) {
		unsigned int i = 0;

		while (eint_reg[i][0]) {
			eint_reg[i][1] =
			    eint_read32(eint_iobase, eint_reg[i][0]);
			LOGPRT(LOG_ERR, "eint reg[%x]=%x\n", eint_reg[i][0],
			       eint_reg[i][1]);
			i++;
		}
	} else {
		LOGPRT(LOG_ERR, "eint con reg =%x\n", con);
	}

	/*dump gic */
	mt_irq_dump_status(262);
	/*mt_eint_dump_status(78); */

	if (modem->func == NULL) {
		LOGPRT(LOG_ERR, "%s %d: no func is NULL!!\n", __func__,
		       __LINE__);
		return -1;
	}
	/*
	   ret = sdio_func1_rd(modem->func, &int_sts, SDIO_CHISR, sizeof(sdio_pio_int_sts_reg));
	   if (ret){
	   LOGPRT(LOG_ERR,  "%s %d: get interrupt status failed ret=%d\n", __func__, __LINE__, ret);
	   return ret;
	   }

	LOGPRT(LOG_ERR, "%s %d: orig int(0x%x)\n", __func__, __LINE__,
	       int_sts.raw_val);
	*/
	return ret;
}

/*Parse exception info received from md, and tranlate into AP side exception structure*/
static void sdio_md_exception(struct sdio_modem *md)
{
	struct _ex_exception_log_t *ex_info;
	int ee_type;
	struct dump_debug_info *debug_info = &md->debug_info;

	if (debug_info == NULL)
		return;

	ex_info = &md->ex_info;

	memset(debug_info, 0, sizeof(struct dump_debug_info));
	ee_type = ex_info->header.ex_type;
	debug_info->type = ee_type;

	if (*((char *)ex_info + CCCI_EXREC_OFFSET_OFFENDER) != 0xCC) {
		memcpy(debug_info->fatal_error.offender,
		       (char *)ex_info + CCCI_EXREC_OFFSET_OFFENDER,
		       sizeof(debug_info->fatal_error.offender) - 1);
		debug_info->fatal_error.offender[sizeof
						 (debug_info->
						  fatal_error.offender) - 1] =
		    '\0';
	} else {
		debug_info->fatal_error.offender[0] = '\0';
	}

	switch (ee_type) {
	case MD_EX_TYPE_INVALID:
		debug_info->name = "INVALID";
		break;

	case MD_EX_TYPE_UNDEF:
		debug_info->name = "Fatal error (undefine)";
		debug_info->fatal_error.err_code1 =
		    ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 =
		    ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_SWI:
		debug_info->name = "Fatal error (swi)";
		debug_info->fatal_error.err_code1 =
		    ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 =
		    ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_PREF_ABT:
		debug_info->name = "Fatal error (prefetch abort)";
		debug_info->fatal_error.err_code1 =
		    ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 =
		    ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_DATA_ABT:
		debug_info->name = "Fatal error (data abort)";
		debug_info->fatal_error.err_code1 =
		    ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 =
		    ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_ASSERT:
		debug_info->name = "ASSERT";
		snprintf(debug_info->assert.file_name,
			 sizeof(debug_info->assert.file_name),
			 ex_info->content.assert.filename);
		debug_info->assert.line_num =
		    ex_info->content.assert.linenumber;
		debug_info->assert.parameters[0] =
		    ex_info->content.assert.parameters[0];
		debug_info->assert.parameters[1] =
		    ex_info->content.assert.parameters[1];
		debug_info->assert.parameters[2] =
		    ex_info->content.assert.parameters[2];
		break;

	case MD_EX_TYPE_FATALERR_TASK:
		debug_info->name = "Fatal error (task)";
		debug_info->fatal_error.err_code1 =
		    ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 =
		    ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_FATALERR_BUF:
		debug_info->name = "Fatal error (buff)";
		debug_info->fatal_error.err_code1 =
		    ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 =
		    ex_info->content.fatalerr.error_code.code2;
		break;

	case MD_EX_TYPE_LOCKUP:
		debug_info->name = "Lockup";
		break;

	case MD_EX_TYPE_ASSERT_DUMP:
		debug_info->name = "ASSERT DUMP";
		snprintf(debug_info->assert.file_name,
			 sizeof(debug_info->assert.file_name),
			 ex_info->content.assert.filename);
		debug_info->assert.line_num =
		    ex_info->content.assert.linenumber;
		break;

	case DSP_EX_TYPE_ASSERT:
		debug_info->name = "MD DMD ASSERT";
		snprintf(debug_info->dsp_assert.file_name,
			 sizeof(debug_info->dsp_assert.file_name),
			 ex_info->content.assert.filename);
		debug_info->dsp_assert.line_num =
		    ex_info->content.assert.linenumber;
		snprintf(debug_info->dsp_assert.execution_unit,
			 sizeof(debug_info->dsp_assert.execution_unit),
			 ex_info->envinfo.execution_unit);
		debug_info->dsp_assert.parameters[0] =
		    ex_info->content.assert.parameters[0];
		debug_info->dsp_assert.parameters[1] =
		    ex_info->content.assert.parameters[1];
		debug_info->dsp_assert.parameters[2] =
		    ex_info->content.assert.parameters[2];
		break;

	case DSP_EX_TYPE_EXCEPTION:
		debug_info->name = "MD DMD Exception";
		snprintf(debug_info->dsp_exception.execution_unit,
			 sizeof(debug_info->dsp_exception.execution_unit),
			 ex_info->envinfo.execution_unit);
		debug_info->dsp_exception.code1 =
		    ex_info->content.fatalerr.error_code.code1;
		break;

	case DSP_EX_FATAL_ERROR:
		debug_info->name = "MD DMD FATAL ERROR";
		snprintf(debug_info->dsp_fatal_err.execution_unit,
			 sizeof(debug_info->dsp_fatal_err.execution_unit),
			 ex_info->envinfo.execution_unit);
		debug_info->dsp_fatal_err.err_code[0] =
		    ex_info->content.fatalerr.error_code.code1;
		debug_info->dsp_fatal_err.err_code[1] =
		    ex_info->content.fatalerr.error_code.code2;
		break;
	case CC_MD1_EXCEPTION:
		debug_info->name = "Fatal error (LTE_EXP)";
		debug_info->fatal_error.err_code1 =
		    ex_info->content.fatalerr.error_code.code1;
		debug_info->fatal_error.err_code2 =
		    ex_info->content.fatalerr.error_code.code2;
		break;
	default:
		debug_info->name = "UNKNOWN Exception";
		break;
	}

	debug_info->ext_mem = ex_info;
	debug_info->ext_size = sizeof(struct _ex_exception_log_t);
}

static int modem_exception_handler(struct sdio_modem *modem)
{
	unsigned int excp_msg_len = 0;
	/*u32 *msg_ptr = NULL; */
	/*ccci_msg_t *ccci_msg = NULL; */
	struct dump_debug_info *debug_info = NULL;
	char ex_info[EE_BUF_LEN] = "";	/*attention, be careful with string length! */
	int db_opt = DB_OPT_DEFAULT;
	char buff[AED_STR_LEN];
	struct _exception_msg *excp_msg = NULL;
	unsigned long flags;
	/*int show_aee = 1; */

	excp_msg_len = calc_payload_len(&modem->msg->head, NULL);
	/*prepare for send ack back to modem */
	excp_msg_len =
	    (excp_msg_len + sizeof(struct sdio_msg_head) + 3) & ~0x03;

	excp_msg = (struct _exception_msg *)(&modem->msg->buffer[0]);

	spin_lock_irqsave(&modem->status_lock, flags);
	modem->status = MD_EXCEPTION_ONGOING;
	spin_unlock_irqrestore(&modem->status_lock, flags);
	/*md will not response to any interrupt when EE happened, so make sure 4-line and data ack are disabled. */
	modem->cbp_data->ipc_enable = false;
	modem->cbp_data->data_ack_enable = false;

	if (excp_msg->ccci_head.data1 == C2K_MD_EX) {
		if (excp_msg->ccci_head.reserved == C2K_MD_EX_CHK_ID) {
#ifndef	CONFIG_EVDO_DT_VIA_SUPPORT
			del_timer(&modem->force_assert_timer);
#endif
			LOGPRT(LOG_INFO, "MD_EX received\n");
			dump_c2k_iram();
		} else {
			LOGPRT(LOG_INFO, "Invalid MD_EX received\n");
		}
		excp_msg->ccci_head.channel = CCCI_CONTROL_TX_CH;
		modem_sdio_write(modem, SDIO_WRITE_ADDR, modem->msg, excp_msg_len);	/*MD_EX_ACK */
	} else if (excp_msg->ccci_head.data1 == C2K_MD_EX_REC_OK) {
		if (excp_msg->ccci_head.reserved == C2K_MD_EX_REC_OK_CHK_ID)
			LOGPRT(LOG_INFO, "MD_EX_OK received\n");
		else
			LOGPRT(LOG_INFO, "Invalid MD_EX_OK received\n");

		exception_data_dump(excp_msg->buffer,
				    excp_msg_len -
				    sizeof(struct sdio_msg_head) -
				    sizeof(struct _ccci_msg));

		/*parse 512B exception data */
		memcpy(&modem->ex_info, &excp_msg->buffer,
		       sizeof(struct _ex_exception_log_t));
		sdio_md_exception(modem);
		debug_info = &modem->debug_info;

		switch (debug_info->type) {
		case MD_EX_TYPE_ASSERT_DUMP:
		case MD_EX_TYPE_ASSERT:
			LOGPRT(LOG_INFO, "filename = %s\n",
			       debug_info->assert.file_name);
			LOGPRT(LOG_INFO, "line = %d\n",
			       debug_info->assert.line_num);
			LOGPRT(LOG_INFO, "para0 = %d, para1 = %d, para2 = %d\n",
			       debug_info->assert.parameters[0],
			       debug_info->assert.parameters[1],
			       debug_info->assert.parameters[2]);
			snprintf(ex_info, EE_BUF_LEN,
				 "\n[%s] file:%s line:%d\np1:0x%08x\np2:0x%08x\np3:0x%08x\n",
				 debug_info->name, debug_info->assert.file_name,
				 debug_info->assert.line_num,
				 debug_info->assert.parameters[0],
				 debug_info->assert.parameters[1],
				 debug_info->assert.parameters[2]);
			break;
		case MD_EX_TYPE_UNDEF:
		case MD_EX_TYPE_SWI:
		case MD_EX_TYPE_PREF_ABT:
		case MD_EX_TYPE_DATA_ABT:
		case MD_EX_TYPE_FATALERR_BUF:
		case MD_EX_TYPE_FATALERR_TASK:
		case CC_MD1_EXCEPTION:
			LOGPRT(LOG_INFO, "fatal error code 1 = %d\n",
			       debug_info->fatal_error.err_code1);
			LOGPRT(LOG_INFO, "fatal error code 2 = %d\n",
			       debug_info->fatal_error.err_code2);
			snprintf(ex_info, EE_BUF_LEN,
				 "\n[%s] err_code1:%d err_code2:%d\n",
				 debug_info->name,
				 debug_info->fatal_error.err_code1,
				 debug_info->fatal_error.err_code2);
			break;
		case MD_EX_TYPE_EMI_CHECK:
			LOGPRT(LOG_INFO,
			       "md_emi_check: %08X, %08X, %02d, %08X\n",
			       debug_info->data.data0, debug_info->data.data1,
			       debug_info->data.channel,
			       debug_info->data.reserved);
			snprintf(ex_info, EE_BUF_LEN,
				 "\n[emi_chk] %08X, %08X, %02d, %08X\n",
				 debug_info->data.data0, debug_info->data.data1,
				 debug_info->data.channel,
				 debug_info->data.reserved);
			break;
		case DSP_EX_TYPE_ASSERT:
			LOGPRT(LOG_INFO, "filename = %s\n",
			       debug_info->dsp_assert.file_name);
			LOGPRT(LOG_INFO, "line = %d\n",
			       debug_info->dsp_assert.line_num);
			LOGPRT(LOG_INFO, "exec unit = %s\n",
			       debug_info->dsp_assert.execution_unit);
			LOGPRT(LOG_INFO, "para0 = %d, para1 = %d, para2 = %d\n",
			       debug_info->dsp_assert.parameters[0],
			       debug_info->dsp_assert.parameters[1],
			       debug_info->dsp_assert.parameters[2]);
			snprintf(ex_info, EE_BUF_LEN,
				 "\n[%s] file:%s line:%d\nexec:%s\np1:%d\np2:%d\np3:%d\n",
				 debug_info->name, debug_info->assert.file_name,
				 debug_info->assert.line_num,
				 debug_info->dsp_assert.execution_unit,
				 debug_info->dsp_assert.parameters[0],
				 debug_info->dsp_assert.parameters[1],
				 debug_info->dsp_assert.parameters[2]);
			break;
		case DSP_EX_TYPE_EXCEPTION:
			LOGPRT(LOG_INFO, "exec unit = %s, code1:0x%08x\n",
			       debug_info->dsp_exception.execution_unit,
			       debug_info->dsp_exception.code1);
			snprintf(ex_info, EE_BUF_LEN,
				 "\n[%s] exec:%s code1:0x%08x\n",
				 debug_info->name,
				 debug_info->dsp_exception.execution_unit,
				 debug_info->dsp_exception.code1);
			break;
		case DSP_EX_FATAL_ERROR:
			LOGPRT(LOG_INFO, "exec unit = %s\n",
			       debug_info->dsp_fatal_err.execution_unit);
			LOGPRT(LOG_INFO,
			       "err_code0 = 0x%08x, err_code1 = 0x%08x\n",
			       debug_info->dsp_fatal_err.err_code[0],
			       debug_info->dsp_fatal_err.err_code[1]);

			snprintf(ex_info, EE_BUF_LEN,
				 "\n[%s] exec:%s err_code1:0x%08x err_code2:0x%08x\n",
				 debug_info->name,
				 debug_info->dsp_fatal_err.execution_unit,
				 debug_info->dsp_fatal_err.err_code[0],
				 debug_info->dsp_fatal_err.err_code[1]);
			break;
		default:	/*Only display exception name */
			snprintf(ex_info, EE_BUF_LEN, "\n[%s]\n",
				 debug_info->name);
			break;
		}
		snprintf(buff, AED_STR_LEN, "md3:%s%s", ex_info,
			 c2k_img_info_str);
#if defined CONFIG_MTK_AEE_FEATURE
		if (debug_info->type == CC_MD1_EXCEPTION
		    && debug_info->fatal_error.err_code1 ==
		    MD_EX_LTE_FATAL_ERROR) {
			LOGPRT(LOG_ERR, "LTE EE, no need to trigger aee\n");
		} else {
			aed_md_exception_api(NULL, 0, NULL, 0, buff, db_opt);
		}
#endif
	}
	return 0;
}

#if 0
/*query modem func's pending irq flag*/
static int modem_irq_query(struct sdio_func *func, unsigned char *pendingirq)
{
	int func_num = 0;
	int ret = 0;

	/*Hack to access Function-0 */
	func_num = func->num;
	func->num = 0;

	*pendingirq = sdio_readb(func, SDIO_CCCR_INTx, &ret);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CCCR_INTx err ret= %d\n",
		       __func__, __LINE__, ret);
	}
	func->num = func_num;

	return ret;
}
#endif

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT

static int sdio_modem_log_input(struct sdio_modem *modem, unsigned int index)
{
	unsigned char *log_addr = NULL;
	int read_ptr = 0, read_len = 0;
	int ret = 0;
	struct sdio_buf_in_packet *packet = NULL;
	struct sdio_modem_port *port;

	port = modem->port[MD_LOG_CH_ID - 1];
	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR,
		       "%s %d: check port error\n", __func__, __LINE__);
		return ret;	/*goto out; */
	}

	memcpy(&modem->curr_log_blk,
	       modem->as_packet->buffer +
	       sizeof(struct sdio_msg_head), sizeof(modem->curr_log_blk));
	LOGPRT(LOG_INFO, "logging in %x %x\n",
	       modem->curr_log_blk.address, modem->curr_log_blk.length);
	modem->log_blk_stamp = sched_clock();
	log_addr =
	    ioremap_nocache(md3_mem_base +
			    modem->curr_log_blk.address,
			    modem->curr_log_blk.length <
			    16 ? 16 : modem->curr_log_blk.length);
	if (port->inception) {
		while (read_ptr < modem->curr_log_blk.length) {
			read_len = modem->curr_log_blk.length - read_ptr;
			read_len = read_len < 4096 ? read_len : 4096;
			ret =
			    rawbulk_push_upstream_buffer
			    (MD_LOG_CH_ID - 1, log_addr + read_ptr, read_len);
			if (ret > 0)
				read_ptr += read_len;
			else
				mdelay(1);
		}
	} else {
		while (read_ptr < modem->curr_log_blk.length) {
			read_len = modem->curr_log_blk.length - read_ptr;
			read_len = read_len < 4096 ? read_len : 4096;
 retry_get_log_in_room:
			mutex_lock(&port->sdio_buf_in_mutex);
			port->sdio_buf_in_size += read_len;
			if (port->sdio_buf_in_size > SDIO_BUF_IN_MAX_SIZE) {
				port->sdio_buf_in_size -= read_len;
				mutex_unlock(&port->sdio_buf_in_mutex);
				msleep(20);
				goto retry_get_log_in_room;
			} else {
				packet =
				    kzalloc(sizeof
					    (struct
					     sdio_buf_in_packet), GFP_KERNEL);
				if (!packet) {
					LOGPRT(LOG_ERR,
					       "%s %d: kzalloc packet error for log\n",
					       __func__, __LINE__);
					port->sdio_buf_in_size -= read_len;
					mutex_unlock(&port->sdio_buf_in_mutex);
					msleep(20);
					goto retry_get_log_in_room;
				}
				INIT_LIST_HEAD(&packet->node);
				packet->size = read_len;
				packet->o_size = packet->size;
				packet->offset = 0;
				packet->buffer =
				    kzalloc(packet->size, GFP_KERNEL);
				if (!packet->buffer) {
					LOGPRT(LOG_ERR,
					       "%s %d: kzalloc packet buffer error for log\n",
					       __func__, __LINE__);
					port->sdio_buf_in_size -= read_len;
					kfree(packet);
					mutex_unlock(&port->sdio_buf_in_mutex);
					msleep(20);
					goto retry_get_log_in_room;
				}
				LOGPRT(LOG_INFO,
				       "logging copy from %p to %p\n",
				       (log_addr + read_ptr), packet->buffer);
				memcpy(packet->buffer,
				       log_addr + read_ptr, packet->size);

				list_add_tail
				    (&packet->node, &port->sdio_buf_in_list);
				port->sdio_buf_in_num++;
				port->sdio_buf_in = 1;
				LOGPRT(LOG_DEBUG,
				       "%s %d: ttySDIO%u data buffered %d for log!\n",
				       __func__, __LINE__, index, packet->size);
				mutex_unlock(&port->sdio_buf_in_mutex);
				wake_up_all(&port->rx_wq);
			}
			read_ptr += read_len;
		}
	}

	schedule_work(&modem->smem_read_done_work);
	iounmap(log_addr);
	return 0;
}

static int sdio_modem_char_input(struct sdio_modem *modem,
				 unsigned int index,
				 unsigned int payload_offset)
{
	int ret = 0;
	struct sdio_buf_in_packet *packet = NULL;
	unsigned int retry_cnt = 0;
	struct sdio_modem_port *port;

	port = modem->port[index];

 retry_get_buf_in_room:
	mutex_lock(&port->sdio_buf_in_mutex);
	port->sdio_buf_in_size += (modem->data_length - payload_offset);
	if (port->sdio_buf_in_size > SDIO_BUF_IN_MAX_SIZE) {
		if (modem->status ==
		    MD_EXCEPTION || modem->status == MD_EXCEPTION_ONGOING) {
			port->sdio_buf_in_size
			    -= (modem->data_length - payload_offset);
			mutex_unlock(&port->sdio_buf_in_mutex);
			msleep(20);
			if (retry_cnt % 20 == 0)
				LOGPRT
				    (LOG_INFO,
				     "retry_get_buf_in_room: port%u, retry_cnt=%u\n",
				     index, retry_cnt);
			retry_cnt++;
			goto retry_get_buf_in_room;
		} else {
			port->sdio_buf_in_size
			    -= (modem->data_length - payload_offset);
			mutex_unlock(&port->sdio_buf_in_mutex);
			pr_debug
			    ("[C2K] ttySDIO%u data buffer overrun %d!\n",
			     index, (modem->data_length - payload_offset));
		}
	} else {
		packet = kzalloc(sizeof(struct sdio_buf_in_packet), GFP_KERNEL);
		if (!packet) {
			LOGPRT(LOG_ERR,
			       "%s %d: kzalloc packet error\n",
			       __func__, __LINE__);
			port->sdio_buf_in_size
			    -= (modem->data_length - payload_offset);
			ret = -ENOMEM;
			mutex_unlock(&port->sdio_buf_in_mutex);
			return ret;	/*goto wait_ack; */
		}
		INIT_LIST_HEAD(&packet->node);
		packet->size = modem->data_length - payload_offset;
#if ENABLE_CHAR_DEV
		packet->o_size = packet->size;
		packet->offset = 0;
#endif
		packet->buffer = kzalloc(packet->size, GFP_KERNEL);
		if (!packet->buffer) {
			LOGPRT(LOG_ERR,
			       "%s %d: kzalloc packet buffer error\n",
			       __func__, __LINE__);
			port->sdio_buf_in_size
			    -= (modem->data_length - payload_offset);
			ret = -ENOMEM;
			kfree(packet);
			mutex_unlock(&port->sdio_buf_in_mutex);
			return ret;	/*goto wait_ack; */
		}
		memcpy(packet->buffer,
		       (modem->as_packet->buffer + sizeof(struct sdio_msg_head)
			+ payload_offset), packet->size);
#if ENABLE_CHAR_DEV
		list_add_tail(&packet->node, &port->sdio_buf_in_list);
		port->sdio_buf_in_num++;
#else
		if (port->sdio_buf_in_num < port->sdio_buf_in_max_num) {
			list_add_tail(&packet->node, &port->sdio_buf_in_list);
			port->sdio_buf_in_num++;
		} else {
			struct
			    sdio_buf_in_packet
			*old_packet;
			old_packet = list_first_entry(&port->sdio_buf_in_list, struct
						      sdio_buf_in_packet, node);
			list_del(&old_packet->node);
			if (old_packet) {
				port->sdio_buf_in_size -= old_packet->size;
				kfree(old_packet->buffer);
				kfree(old_packet);
			}
			list_add_tail(&packet->node, &port->sdio_buf_in_list);
		}
#endif
		port->sdio_buf_in = 1;
		LOGPRT(LOG_DEBUG,
		       "%s %d: ttySDIO%d data buffered %d!\n",
		       __func__, __LINE__, index, packet->size);
		mutex_unlock(&port->sdio_buf_in_mutex);
#if ENABLE_CHAR_DEV
		wake_up_all(&port->rx_wq);
#endif
	}
	return 0;
}

#if !ENABLE_CHAR_DEV
static int sdio_modem_tty_input(struct sdio_modem *modem, unsigned int index)
{
	int ret = 0;
	struct sdio_modem_port *port;

	port = modem->port[index];

	if (port->sdio_buf_in == 1) {
		/*make sure data in list bufeer had been pushed to tty buffer */
		mutex_lock(&port->sdio_buf_in_mutex);
		mutex_unlock(&port->sdio_buf_in_mutex);
	}
 retry_get_tty_room:
	ret =
	    tty_buffer_request_room
	    (&port->port, modem->data_length - payload_offset);

	if (ret < (modem->data_length - payload_offset)) {
		if (modem->status ==
		    MD_EXCEPTION || modem->status == MD_EXCEPTION_ONGOING) {
			msleep(20);
			goto retry_get_tty_room;
		} else
			LOGPRT(LOG_ERR,
			       "%s %d: ttySDIO%d no room in tty rx buffer!(md status %d)\n",
			       __func__, __LINE__, index, modem->status);
	} else {
		ret =
		    tty_insert_flip_string
		    (&port->port,
		     (modem->as_packet->buffer
		      + payload_offset +
		      sizeof(struct
			     sdio_msg_head)),
		     (modem->data_length - payload_offset));

		if (ret < (modem->data_length - payload_offset)) {
			LOGPRT(LOG_ERR,
			       "%s %d: ttySDIO%d couldn't insert all characters (TTY is full?)!\n",
			       __func__, __LINE__, index);
		} else {
			tty_flip_buffer_push(&port->port);
		}
	}

	return 0;
}
#endif

static void sdio_pio_intr_handler(struct sdio_func *func)
{
	unsigned int pure_int;
	static int interrupt_cnt;
	struct sdio_modem *modem;
	struct sdio_modem_port *port;
	/*unsigned char reg = 0; */
	/*int  bytecnt = 0; */
	int ret = 0;
	/*int iir =0; */
	/*int readcnt = 0; */
	struct tty_struct *tty;
	unsigned char index = 0;
	unsigned char payload_offset = 0;
	/*struct sdio_buf_in_packet *packet = NULL; */
	static int keep_skb;

	/*unsigned int excp_msg_len = 0; */
	/*u32 *msg_ptr = NULL; */
	/*DEBUG_INFO_T *debug_info = NULL; */
	/*char ex_info[EE_BUF_LEN]="";  attention, be careful with string length! */
	/*int db_opt = DB_OPT_DEFAULT; */
	/*char buff[AED_STR_LEN]; */
	int crplr = 0;
	/*unsigned char pending = 0; */
	unsigned int hw_len;
	int raw_val;
	int ch_id = 0;

	static struct sdio_msg_head *msg_head;
	static int throughput_count;
	static int total_copy;
	static int keep_recv;
	static int dump_exp_data = 1;

	union sdio_pio_int_sts_reg *int_sts;
	union sdio_pio_int_mask_reg *int_mask;

	interrupt_cnt++;
	LOGPRT(LOG_DEBUG, "%s enter %d times...\n", __func__, interrupt_cnt);

/*ret = modem_irq_query(func,&pending);
	if (ret) {
		LOGPRT(LOG_ERR,  "read SDIO_CCCR_INTx err ret= %d\n", __func__,__LINE__,ret);
		goto err_out;
	}
	if((pending & SDIO_FUNC_1) ==0){
		LOGPRT(LOG_NOTICE2,  "pending=%d ret= %d\n", pending,ret);
		goto out;
	}
*/
	modem = sdio_get_drvdata(func);

	int_sts = &modem->int_sts;
	int_mask = &modem->int_mask;

	sdio_pio_disable_interrupt(func);

	ret = sdio_func1_rd(func, &raw_val, SDIO_CHLPCR, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CHLPCR failed ret=%d\n",
		       __func__, __LINE__, ret);
	} else
		LOGPRT(LOG_DEBUG, "%s %d: SDIO_CHLPCR(0x%x)\n", __func__,
		       __LINE__, raw_val);

	ret = sdio_func1_rd(func, &raw_val, SDIO_CHIER, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CHLPCR failed ret=%d\n",
		       __func__, __LINE__, ret);
	} else
		LOGPRT(LOG_DEBUG, "%s %d: SDIO_CHIER(0x%x)\n", __func__,
		       __LINE__, raw_val);

	/*for (;;) { */
	ret =
	    sdio_func1_rd(func, int_sts, SDIO_CHISR,
			  sizeof(union sdio_pio_int_sts_reg));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: get interrupt status failed ret=%d\n",
		       __func__, __LINE__, ret);
		goto end;
	}

	LOGPRT(LOG_DEBUG, "%s %d: orig int(0x%x)\n", __func__, __LINE__,
	       int_sts->raw_val);

	pure_int =
	    int_sts->raw_val & (int_mask->raw_val | SDIO_CHISR_TX_CMPLT_CNT);

	LOGPRT(LOG_DEBUG, "%s %d: pure_int(0x%x)\n", __func__, __LINE__,
	       pure_int);

	if (!pure_int)
		goto end;

	if (modem->int_clr_ctl == SDIO_INT_CTL_W1C) {
		ret =
		    sdio_func1_wr(func, SDIO_CHISR, &pure_int,
				  sizeof(union sdio_pio_int_sts_reg));
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: write SDIO_CHISR failed ret=%d\n",
			       __func__, __LINE__, ret);
		}
	}

	if (pure_int & SDIO_CHISR_FW_OWN_BACK)
		modem->fw_own = 0;

	if (pure_int & SDIO_CHISR_TX_EMPTY) {
		LOGPRT(LOG_DEBUG, "got tx done\n");
#ifdef TX_DONE_TRACE
		del_timer(&timer_wait_tx_done);
#endif
#if USE_CCIF_INTR
		dump_ccif();
#endif
		/*Can do Tx */
		atomic_set(&modem->tx_fifo_cnt, TX_FIFO_SZ);
		wake_up(&modem->wait_tx_done_q);
	}

	if (pure_int & SDIO_CHISR_TX_UNDER_THOLD) {
		/*Can do Tx */
		atomic_set(&modem->tx_fifo_cnt, TX_FIFO_SZ - DEFAULT_TX_THOLD);
	}

	if (pure_int & SDIO_CHISR_TX_OVERFLOW) {
		/*g_tx_overflow_sts = 1; */
		/*Error!!!! */
		LOGPRT(LOG_ERR,
		       "!!!! sdio pio function got TX_OVERFLOW interrupt !!!!\n");
	}

	if (pure_int & SDIO_CHISR_RX_RDY) {
		LOGPRT(LOG_DEBUG, "%s %d: rx ready\n", __func__, __LINE__);

		/*Check CHISR.RX_PKT_LEN == CRPLR.RX_PKT_LEN */
		ret = sdio_func1_rd(func, &crplr, SDIO_CRPLR, sizeof(crplr));
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: read Rx Packet Length Register failed ret=%d\n",
			       __func__, __LINE__, ret);
		} else {
			if (((crplr & SDIO_CRPLR_RX_PKT_LEN) >> 16) !=
			    int_sts->u.rx_pkt_len) {
				LOGPRT(LOG_ERR,
				       "!!!! CHISR.RX_PKT_LEN(%d) != CRPLR.RX_PKT_LEN(%d) !!!!\n",
				       int_sts->u.rx_pkt_len,
				       (crplr & SDIO_CRPLR_RX_PKT_LEN) >> 16);
			}
			LOGPRT(LOG_DEBUG, "rx pkt len (%d)",
			       int_sts->u.rx_pkt_len);
		}

		modem->msg->head.start_flag = 0;
		modem->msg->head.chanInfo = 0;
		modem->msg->head.tranHi = 0;
		modem->msg->head.tranLow = 0;
		memset(modem->msg->buffer, 0, sizeof(modem->msg->buffer));

		if (modem->cbp_data->data_ack_enable) {
			atomic_set(&modem->cbp_data->cbp_data_ack->state,
				   MODEM_ST_TX_RX);
		}
		if (0 ==
		    sdio_pio_rx_pkt(func, (char *)modem->msg,
				    int_sts->u.rx_pkt_len)) {
			sdio_rx_cnt++;
			total_copy += int_sts->u.rx_pkt_len;
			if (total_copy > SDIO_ASSEMBLE_MAX) {
				LOGPRT(LOG_ERR,
				       "%s %d error: packet size too large.\n",
				       __func__, __LINE__);
				total_copy = 0;
				goto end;
			}
		}

		if ((!(modem->msg->head.tranHi & EXTEND_CH_BIT) &&
			((modem->msg->head.chanInfo == SDIO_AT_CHANNEL_NUM) ||
		    (modem->msg->head.chanInfo == SDIO_AT2_CHANNEL_NUM) ||
		    (modem->msg->head.chanInfo == EXCP_MSG_CH_ID) ||
		    (modem->msg->head.chanInfo == EXCP_CTRL_CH_ID) ||
		    (modem->msg->head.chanInfo == AGPS_CH_ID) ||
		    (modem->msg->head.chanInfo == SDIO_AT3_CHANNEL_NUM))) ||
		    (modem->msg->head.tranHi & EXTEND_CH_BIT)) {
			sdio_tx_rx_printk(modem->msg, 0);
		}

		if ((modem->status == MD_EXCEPTION
		     || modem->status == MD_EXCEPTION_ONGOING)) {
			if (dump_exp_data
			    && (!(modem->msg->head.tranHi & EXTEND_CH_BIT) &&
			    modem->msg->head.chanInfo == EXCP_DATA_CH_ID)) {
				sdio_tx_rx_printk(modem->msg, 0);
				dump_exp_data = 0;
			}
			/*make sure each crash file can be dumpped */
			if (!(modem->msg->head.tranHi & EXTEND_CH_BIT) &&
				modem->msg->head.chanInfo == EXCP_MSG_CH_ID)
				dump_exp_data = 1;

		}
		if (dump_exp_data == 0 && modem->status == MD_READY)
			/*reset it after modem is ready */
			dump_exp_data = 1;

		/*index = modem->msg->head.chanInfo -1; */
		/*port = modem->port[index]; */
		hw_len =
		    (modem->msg->head.hw_head.len_hi << 8) +
		    modem->msg->head.hw_head.len_low;

		LOGPRT(LOG_DEBUG, "%s: hw_len (%d)\n", __func__, hw_len);

		if (!keep_recv) {
			LOGPRT(LOG_DEBUG, "%s %d new packet\n", __func__,
			       __LINE__);
			while (atomic_read(&modem->as_packet->occupied))
				msleep(20);

			LOGPRT(LOG_DEBUG, "%s %d as_packet available now\n",
			       __func__, __LINE__);
			if (modem->msg->head.tranHi & MORE_DATA_FOLLOWING) {
				LOGPRT(LOG_DEBUG,
				       "%s %d now can begin assemble...\n",
				       __func__, __LINE__);
				keep_recv = 1;
			}
			msg_head =
			    (struct sdio_msg_head *)modem->as_packet->buffer;
			msg_head->chanInfo = modem->msg->head.chanInfo;
			msg_head->start_flag = 0xFE;

			modem->as_packet->size = 0;
		}

		modem->data_length = (((modem->msg->head.tranHi & 0x0F) << 8) |
				      (modem->msg->head.tranLow & 0xFF));
		payload_offset = (modem->msg->head.tranHi & 0xC0) >> 6;
		modem->data_length -= payload_offset;

		/*if (keep_recv){ */
		memcpy(modem->as_packet->buffer + sizeof(struct sdio_msg_head) +
		       modem->as_packet->size,
		       modem->msg->buffer + payload_offset, modem->data_length);
		modem->as_packet->size += modem->data_length;

		if (!(modem->msg->head.tranHi & MORE_DATA_FOLLOWING)) {
			LOGPRT(LOG_DEBUG, "No data following\n");

			msg_head->tranHi =
			    (modem->as_packet->size & 0xFF00) >> 8;
			msg_head->tranLow = modem->as_packet->size & 0xFF;
			msg_head->hw_head.len_hi =
			    ((modem->as_packet->size +
			      sizeof(struct sdio_msg_head)) & 0xFF00) >> 8;
			msg_head->hw_head.len_low =
			    (modem->as_packet->size +
			     sizeof(struct sdio_msg_head)) & 0xFF;

			LOGPRT(LOG_DEBUG, "hw_len low(0x%x), hi(0x%x)\n",
			       msg_head->hw_head.len_low,
			       msg_head->hw_head.len_hi);
			LOGPRT(LOG_DEBUG, "as_packet(0x%x, 0x%x)\n",
			       *modem->as_packet->buffer,
			       *(modem->as_packet->buffer + 1));

#if 1
			/*Just for test */

			if (!(modem->msg->head.tranHi & EXTEND_CH_BIT) &&
				modem->msg->head.chanInfo == CCMNI_AP_LOOPBACK_CH) {
				if (!throughput_count) {
					LOGPRT(LOG_INFO,
					       "C2K throughput test begin....\n");
				}
				throughput_count++;
				if (throughput_count == 5000) {
					throughput_count = 0;
					LOGPRT(LOG_INFO,
					       "C2K throughput test end....\n");
				}
				schedule_work(&modem->loopback_work);
			}
#endif
			total_copy = 0;
			keep_recv = 0;

			if (msg_head->start_flag != 0xFE) {
				LOGPRT(LOG_ERR,
				       "%s %d: start_flag != 0xFE and value is 0x%x, go out.\n",
				       __func__, __LINE__,
				       modem->msg->head.start_flag);
				goto out;
			}
			if (!(modem->msg->head.tranHi & EXTEND_CH_BIT) &&
				modem->msg->head.chanInfo == EXCP_CTRL_CH_ID
			    && (modem->status == MD_EXCEPTION
				|| modem->status == MD_EXCEPTION_ONGOING))
				modem_exception_handler(modem);

			if (!(modem->msg->head.tranHi & EXTEND_CH_BIT) &&
				((modem->msg->head.chanInfo == EXCP_MSG_CH_ID)
			    || (modem->msg->head.chanInfo == EXCP_DATA_CH_ID))) {
				LOGPRT(LOG_DEBUG,
				       "excp msg/data received ch[%d]\n",
				       modem->msg->head.chanInfo);
			}
#if ENABLE_CCMNI
			if (!(modem->msg->head.tranHi & EXTEND_CH_BIT) &&
				((modem->msg->head.chanInfo & 0x0F) == CCMNI_CH_ID)) {
				index = CCMNI_CH_ID - 1;
				port = modem->port[index];
				if (!port->inception) {
					static struct sk_buff *skb;
					static int total_copy;
					unsigned int rx_ch;
					unsigned int rx_len;

					rx_len =
					    (((modem->msg->
					       head.tranHi & 0x0F) << 8) |
					     (modem->msg->head.tranLow & 0xFF));
					rx_ch =
					    (modem->msg->
					     head.chanInfo & 0xF0) >> 4;
					modem->msg->head.chanInfo &= 0x0F;

 retry_get_skb:
					if (!keep_skb || !skb) {
						LOGPRT(LOG_DEBUG,
						       "%s %d request skb from kernel.\n",
						       __func__, __LINE__);
						skb = dev_alloc_skb(1600);
						total_copy = 0;
					}
					if (!skb) {
						LOGPRT(LOG_ERR,
						       "%s %d retry.\n",
						       __func__, __LINE__);
						msleep(20);
						goto retry_get_skb;
					}
					LOGPRT(LOG_DEBUG, "%s %d got skb.\n",
					       __func__, __LINE__);
					if (rx_len > 1600 || total_copy > 1600) {
						LOGPRT(LOG_ERR,
						       "%s %d error: skb too large.\n",
						       __func__, __LINE__);
						goto out;
					}
					memcpy(skb_put(skb, rx_len),
					       modem->as_packet->buffer +
					       sizeof(struct sdio_msg_head),
					       rx_len);
					total_copy += rx_len;
					keep_skb = 1;
					/*the last packet of skb received */
					if (!(modem->msg->head.tranHi & MORE_DATA_FOLLOWING)) {
						LOGPRT(LOG_DEBUG,
						       "%s: data to ccmni...\n",
						       __func__);
						/* sdio_tx_rx_printk(skb, 0); */
						keep_skb = 0;
						ccmni_ops.rx_callback
						    (SDIO_MD_ID, rx_ch, skb,
						     NULL);
					} else
						LOGPRT(LOG_DEBUG,
						       "%s: data to ccmni pending 0x%x...\n",
						       __func__,
						       modem->msg->head.tranHi);
				}
			}
#endif
#if ENABLE_CHAR_DEV
			if (!(modem->msg->head.tranHi & EXTEND_CH_BIT) &&
				((modem->msg->head.chanInfo & 0x0F) == MD_LOG2_CH_ID)) {
				ret = sdio_modem_log_input(modem, index);
				if (unlikely(ret < 0))
					goto out;
			}
#endif

			ch_id = (modem->msg->head.chanInfo & 0x0F) +
					(modem->msg->head.tranHi & EXTEND_CH_BIT);
			if (ch_id > 0
			    && ch_id < (SDIO_TTY_NR + 1)) {
				/*pay attention to channel mapping with rawbulk in rawbulk_push_upstream_buffer() */
				index = ch_id - 1;

				/*
				   because we've already processed offset info when copy data to as_packet buffer.
				   so ignore it here.
				 */
				/*payload_offset = ((msg_head->tranHi & 0xC0) >> 6); */
				payload_offset = 0;
				if (payload_offset) {
					LOGPRT(LOG_DEBUG,
					       "%s %d: payload_offset = %d.\n",
					       __func__, __LINE__,
					       payload_offset);
				}

				/*
				   tranHi comes from as_packet size, no other info.
				   audio packet size may be up to 16KB, so we should & 0xFF but not 0x0F here.
				 */
				modem->data_length =
				    (((msg_head->tranHi & 0xFF) << 8) |
				     (msg_head->tranLow & 0xFF));
				if (modem->data_length == 0) {
					LOGPRT(LOG_ERR,
					       "%s %d: data_length is 0\n",
					       __func__, __LINE__);
					goto out;
				}
				port = modem->port[index];
				ret = check_port(port);
				if (ret < 0) {
					LOGPRT(LOG_ERR,
					       "%s %d: check port error\n",
					       __func__, __LINE__);
					goto out;
				}

				if (port->inception) {
					rawbulk_push_upstream_buffer(index,
								     (modem->as_packet->buffer
								      +
								      sizeof
								      (struct
								       sdio_msg_head)
								      +
								      payload_offset),
								     (modem->data_length
								      -
								      payload_offset));
#if ENABLE_CCMNI
				} else if (ch_id != CCMNI_CH_ID) {
#else
				} else {
#endif
#if ENABLE_CHAR_DEV
					tty = NULL;
#else
					tty = tty_port_tty_get(&port->port);
#endif
					if (!tty) {
						ret =
						    sdio_modem_char_input
						    (modem, index,
						     payload_offset);
					}
					if (!tty && ret < 0)
						goto wait_ack;

#if !ENABLE_CHAR_DEV
					if (tty && modem->data_length)
						sdio_modem_tty_input(modem,
								     index);
					if (tty)
						tty_kref_put(tty);
#endif
				}
			} else if (ch_id == 0) {	/*control message analyze */
				ctrl_msg_analyze(modem);
			} else {
#if ENABLE_CCMNI
				if ((modem->msg->head.chanInfo & 0x0F) !=
				    CCMNI_CH_ID) {
					LOGPRT(LOG_ERR,
					       "%s %d: error chanInfo is %d, go out.\n",
					       __func__, __LINE__,
					       modem->msg->head.chanInfo);
					goto out;
				}
#else
				LOGPRT(LOG_ERR,
				       "%s %d: error chanInfo is %d, go out.\n",
				       __func__, __LINE__,
				       modem->msg->head.chanInfo);
				goto out;
#endif
			}
		}
 wait_ack:
		/*
		   if(modem->cbp_data->data_ack_enable){
		   modem->cbp_data->data_ack_wait_event(modem->cbp_data->cbp_data_ack);
		   } */
 out:
		LOGPRT(LOG_DEBUG, "%s %d: out.\n", __func__, __LINE__);
		/*return; */

		/*err_out: */
		/*LOGPRT(LOG_ERR,  "%s %d: let cbp die now.\n",__func__, __LINE__); */
		/*modem_err_indication_usr(1); */
		/*return; */
	}

	if (pure_int & SDIO_CHISR_FW_INT_INDICATOR) {
		/*handle fw int */
		/*mt_sdio_pio_handle_fw_intr(); */
	}
	/*} */
 end:
	sdio_pio_enable_interrupt(func);
#ifdef TX_DONE_TRACE
	pr_debug("[C2K] intr handler exit....");
#endif
}
#else

/*
 *This SDIO interrupt handler.
 */
static void modem_sdio_irq(struct sdio_func *func)
{
	struct sdio_modem *modem;
	struct sdio_modem_port *port;
	unsigned char reg = 0;
	int bytecnt = 0;
	int ret = 0;
	int iir = 0;
	int readcnt = 0;
	struct tty_struct *tty;
	unsigned char index = 0;
	unsigned int payload_offset;
	struct sdio_buf_in_packet *packet = NULL;
	unsigned char pending = 0;
	static int keep_skb;

	/*get pending interrupt */
	ret = modem_irq_query(func, &pending);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CCCR_INTx err ret= %d\n",
		       __func__, __LINE__, ret);
		goto err_out;
	}
	if ((pending & SDIO_FUNC_1_IRQ) == 0) {
		LOGPRT(LOG_NOTICE2, "pending=%d ret= %d\n", pending, ret);
		goto out;
	}

	modem = sdio_get_drvdata(func);
	do {
		/*Reading the IIR register on the slave clears the interrupt. Since host and
		   slave run asynchronously, must ensure int bit is set before reading
		   transfer count register  */
		iir = sdio_readb(func, 0x04, &ret);
	} while ((iir != 1) && (readcnt++ <= 10));

	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read iir err ret= %d\n", __func__,
		       __LINE__, ret);
		goto err_out;
	}

	if (iir != 1) {
		LOGPRT(LOG_ERR, "%s %d error iir value = %d!!!\n", __func__,
		       __LINE__, iir);
		goto out;
	}

	/*Read byte count */
	reg = sdio_readb(func, 0x08, &ret);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read data cnt err ret= %d\n", __func__,
		       __LINE__, ret);
		goto err_out;
	}

	bytecnt = reg;
	reg = sdio_readb(func, 0x09, &ret);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read data cnt ret= %d\n", __func__,
		       __LINE__, ret);
		goto err_out;
	}
	bytecnt |= (reg << 8);

	if (bytecnt == 0) {
		LOGPRT(LOG_ERR, "%s %d error read size %d.\n", __func__,
		       __LINE__, bytecnt);
		goto out;
	}
	modem->msg->head.start_flag = 0;
	modem->msg->head.chanInfo = 0;
	modem->msg->head.tranHi = 0;
	modem->msg->head.tranLow = 0;
	memset(modem->msg->buffer, 0, sizeof(modem->msg->buffer));

	if (modem->cbp_data->data_ack_enable) {
		atomic_set(&modem->cbp_data->cbp_data_ack->state,
			   MODEM_ST_TX_RX);
	}

	ret = sdio_readsb(func, modem->msg, 0x00, bytecnt);
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: port%d sdio read with error code = %d, read bytecount = %d!!!\n",
		       __func__, __LINE__, modem->msg->head.chanInfo, ret,
		       bytecnt);
		goto err_out;
	}

	if ((modem->msg->head.chanInfo == SDIO_AT_CHANNEL_NUM) ||
	    (modem->msg->head.chanInfo == SDIO_AT2_CHANNEL_NUM) ||
	    (modem->msg->head.chanInfo == SDIO_AT3_CHANNEL_NUM) ||
	    (sdio_log_level > LOG_NOTICE)) {
		sdio_tx_rx_printk(modem->msg, 0);
	}
	/*sdio_tx_rx_printk(modem->msg, 0); */

	if (modem->msg->head.start_flag != MSG_START_FLAG) {
		LOGPRT(LOG_ERR,
		       "%s %d: start_flag != MSG_START_FLAG and value is 0x%x, go out.\n",
		       __func__, __LINE__, modem->msg->head.start_flag);
		goto out;
	}

	if (modem->msg->head.chanInfo == EXCP_CTRL_CH_ID)
		LOGPRT(LOG_DEBUG, "modem status[%d]\n", modem->status);

	if (modem->msg->head.chanInfo == EXCP_CTRL_CH_ID
	    && (modem->status == MD_EXCEPTION
		|| modem->status == MD_EXCEPTION_ONGOING)) {
		modem_exception_handler(modem);
	}
	if ((modem->msg->head.chanInfo == EXCP_MSG_CH_ID)
	    || (modem->msg->head.chanInfo == EXCP_DATA_CH_ID)) {
		LOGPRT(LOG_DEBUG,
		       "[MODEM SDIO] excp msg/data received ch[%d]\n",
		       modem->msg->head.chanInfo);
	}
#if ENABLE_CCMNI

	if ((modem->msg->head.chanInfo & 0x0F) == CCMNI_CH_ID) {
		static struct sk_buff *skb;
		static int total_copy;
		unsigned int rx_ch;
		unsigned int rx_len = 0;

		rx_len = (((modem->msg->head.tranHi & 0x0F) << 8) |
			  (modem->msg->head.tranLow & 0xFF));
		rx_ch = (modem->msg->head.chanInfo & 0xF0) >> 4;
		modem->msg->head.chanInfo &= 0x0F;

 retry_get_skb:
		if (!keep_skb || !skb) {
			LOGPRT(LOG_INFO, "%s %d request skb from kernel.\n",
			       __func__, __LINE__);
			skb = dev_alloc_skb(1600);
			total_copy = 0;
		}
		if (!skb) {
			LOGPRT(LOG_INFO, "%s %d retry.\n", __func__, __LINE__);
			msleep(20);
			goto retry_get_skb;
		}
		LOGPRT(LOG_INFO, "%s %d got skb.\n", __func__, __LINE__);
		if (total_copy > 1600) {
			LOGPRT(LOG_ERR, "%s %d error: skb too large.\n",
			       __func__, __LINE__);
			goto out;
		}
		memcpy(skb_put(skb, rx_len), modem->msg->buffer, rx_len);
		total_copy += rx_len;
		keep_skb = 1;
		if (!(modem->msg->head.tranHi & 0x20)) {	/*the last packet of skb received */
			LOGPRT(LOG_INFO, "%s: data to ccmni...\n", __func__);
			/* sdio_tx_rx_printk(skb, 0); */
			keep_skb = 0;
			ccmni_ops.rx_callback(SDIO_MD_ID, rx_ch, skb, NULL);
		} else
			LOGPRT(LOG_INFO, "%s: data to ccmni pending 0x%x...\n",
			       __func__, modem->msg->head.tranHi);
	}
#endif

	if (modem->msg->head.chanInfo > 0
	    && modem->msg->head.chanInfo < (SDIO_TTY_NR + 1)) {

		index = modem->msg->head.chanInfo - 1;
		modem->data_length =
		    calc_payload_len(&modem->msg->head, &payload_offset);

		if (modem->data_length == 0) {
			LOGPRT(LOG_ERR, "%s %d: data_length is 0\n", __func__,
			       __LINE__);
			goto out;
		}
		port = modem->port[index];
		ret = check_port(port);
		if (ret < 0) {
			LOGPRT(LOG_ERR, "%s %d: check port error\n", __func__,
			       __LINE__);
			goto out;
		}

		port->rx_count++;

		if (port->inception) {
			rawbulk_push_upstream_buffer(index,
						     (modem->msg->buffer +
						      payload_offset),
						     modem->data_length);
		} else {
			tty = tty_port_tty_get(&port->port);
			if (!tty) {
				LOGPRT(LOG_ERR, "tty is NULL");
				mutex_lock(&port->sdio_buf_in_mutex);
				port->sdio_buf_in_size += modem->data_length;
				if (port->sdio_buf_in_size >
				    SDIO_BUF_IN_MAX_SIZE) {
					port->sdio_buf_in_size -=
					    modem->data_length;
					mutex_unlock(&port->sdio_buf_in_mutex);
					LOGPRT(LOG_ERR,
					       "%s %d: ttySDIO%d data buffer overrun!\n",
					       __func__, __LINE__, index);
				} else {
					packet =
					    kzalloc(sizeof
						    (struct sdio_buf_in_packet),
						    GFP_KERNEL);
					if (!packet) {
						LOGPRT(LOG_ERR,
						       "%s %d: kzalloc packet error\n",
						       __func__, __LINE__);
						ret = -ENOMEM;
						mutex_unlock
						    (&port->sdio_buf_in_mutex);
						goto wait_ack;
					}
					packet->size = modem->data_length;
					packet->buffer =
					    kzalloc(packet->size, GFP_KERNEL);
					if (!packet->buffer) {
						LOGPRT(LOG_ERR,
						       "%s %d: kzalloc packet buffer error\n",
						       __func__, __LINE__);
						ret = -ENOMEM;
						kfree(packet);
						mutex_unlock
						    (&port->sdio_buf_in_mutex);
						goto wait_ack;
					}
					memcpy(packet->buffer,
					       (modem->msg->buffer +
						payload_offset), packet->size);

					if (port->sdio_buf_in_num <
					    port->sdio_buf_in_max_num) {
						list_add_tail(&packet->node,
							      &port->sdio_buf_in_list);
						port->sdio_buf_in_num++;
					} else {
						struct sdio_buf_in_packet
						*old_packet = NULL;
						old_packet =
						    list_first_entry
						    (&port->sdio_buf_in_list,
						     struct
						     sdio_buf_in_packet, node);
						list_del(&old_packet->node);
						/*if (old_packet) { */
						port->sdio_buf_in_size
						    -= old_packet->size;
						kfree(old_packet->buffer);
						kfree(old_packet);

						list_add_tail(&packet->node,
							      &port->sdio_buf_in_list);
					}
					port->sdio_buf_in = 1;
					mutex_unlock(&port->sdio_buf_in_mutex);
					LOGPRT(LOG_ERR,
					       "%s %d: ttySDIO%d data buffered!\n",
					       __func__, __LINE__, index);
				}
			}

			if (tty && modem->data_length) {
				if (port->sdio_buf_in == 1) {
					/*make sure data in list buffer had been pushed to tty buffer */
					mutex_lock(&port->sdio_buf_in_mutex);
					mutex_unlock(&port->sdio_buf_in_mutex);
				}

				ret =
				    tty_buffer_request_room(&port->port,
							    modem->data_length);

				if (ret < modem->data_length) {
					LOGPRT(LOG_ERR,
					       "%s %d: ttySDIO%d no room in tty rx buffer!\n",
					       __func__, __LINE__, index);
				} else {
					ret =
					    tty_insert_flip_string(&port->port,
								   (modem->
								    msg->buffer
								    +
								    payload_offset),
								   modem->data_length);

					if (ret < modem->data_length) {
						LOGPRT(LOG_ERR,
						       "%s %d: ttySDIO%d couldn't insert all characters!\n",
						       __func__, __LINE__,
						       index);
					} else {
						tty_flip_buffer_push
						    (&port->port);

					}
				}
			}
			if (tty)
				tty_kref_put(tty);
		}
	} else if (modem->msg->head.chanInfo == CTRL_CH_ID) {	/*control message analyze */
		ctrl_msg_analyze(modem);
	} else {
		LOGPRT(LOG_ERR, "%s %d: error chanInfo is %d, go out.\n",
		       __func__, __LINE__, modem->msg->head.chanInfo);
		goto out;
	}
 wait_ack:
	/*LOGPRT(LOG_ERR,  "%s %d: port%d data ack before!\n", __func__, __LINE__, port->index); */
	if (modem->cbp_data->data_ack_enable) {
		modem->cbp_data->data_ack_wait_event(modem->
						     cbp_data->cbp_data_ack);
	}
	/*LOGPRT(LOG_ERR,  "%s %d: port%d data ack after!\n", __func__, __LINE__, port->index); */
 out:
	return;

 err_out:
	LOGPRT(LOG_ERR, "%s %d: let cbp die now.\n", __func__, __LINE__);
	modem_err_indication_usr(1);
}
#endif

static int func_enable_irq(struct sdio_func *func, int enable)
{
	int func_num = 0;
	u8 cccr = 0;
	int ret = 0;

	if (!func->card)
		return -1;
	/*Hack to access Function-0 */
	func_num = func->num;
	func->num = 0;

	cccr = sdio_readb(func, SDIO_CCCR_IENx, &ret);
	if (WARN_ON(ret))
		goto set_func;

	if (enable) {
		/*Master interrupt enable ... */
		cccr |= BIT(0);
		/*... for our function */
		cccr |= BIT(func_num);
	} else {
		/*Master interrupt enable ... */
		cccr &= ~(BIT(0));
		/*... for our function */
		cccr &= ~(BIT(func_num));
	}

	sdio_writeb(func, cccr, SDIO_CCCR_IENx, &ret);
	if (WARN_ON(ret))
		goto set_func;

	/*Restore the modem function number */
	func->num = func_num;
	return 0;

 set_func:
	func->num = func_num;
	return ret;
}

static void modem_sdio_write(struct sdio_modem *modem, int addr,
			     void *buf, size_t len)
{
	struct sdio_func *func = modem->func;
	/*struct mmc_host *host = func->card->host; */
	unsigned char *print_buf = NULL;
	struct sdio_msg_head *msg_head = NULL;

#if PADDING_BY_BLOCK_SIZE
	unsigned int block_cnt = 0;
	unsigned int padding_len = 0;
	int padding_index = 0;
	char *pad_begin_ptr = NULL;
#endif

	unsigned char ch_id;
	unsigned char tport_id;
	int err_flag = 0;
	int ret;
	int tx_ready = -1;
	static int modem_fc_flag;
	unsigned long flags;

	if (buf) {
		msg_head = (struct sdio_msg_head *)buf;
		print_buf = (unsigned char *)buf;
	} else {
		LOGPRT(LOG_ERR, "%s %d: buf is NULL\n",
					       __func__, __LINE__);
		return;
	}
	ch_id = (msg_head->chanInfo & 0x0F) + (msg_head->tranHi & EXTEND_CH_BIT);
	tport_id = ch_id - 1;

	if (tport_id >= 0 && tport_id < SDIO_TTY_NR)
		modem->port[tport_id]->tx_count++;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	if (ch_id == CTRL_CH_ID)
		pr_debug("[C2K] before wait tx done\n");

	spin_lock_irqsave(&modem->status_lock, flags);
	if (modem->status == MD_OFF) {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		LOGPRT(LOG_NOTICE, "c2k off now, ignore\n");
		goto terminate;
	}
	spin_unlock_irqrestore(&modem->status_lock, flags);

	wait_event(modem->wait_tx_done_q,
		   (TX_FIFO_SZ == atomic_read(&modem->tx_fifo_cnt)));
	if (ch_id == CTRL_CH_ID)
		pr_debug("[C2K] after wait tx done\n");

	spin_lock_irqsave(&modem->status_lock, flags);
	if (modem->status == MD_OFF) {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		LOGPRT(LOG_NOTICE, "c2k off now, ignore\n");
		goto terminate;
	}
	spin_unlock_irqrestore(&modem->status_lock, flags);
#endif

	if (modem->cbp_data->flow_ctrl_enable) {
		if (c2k_gpio_get_value
		    (modem->cbp_data->cbp_flow_ctrl->wait_gpio) !=
		    modem->cbp_data->gpio_flow_ctrl_polar) {
			if (FLOW_CTRL_ENABLE ==
			    atomic_read(&modem->cbp_data->cbp_flow_ctrl->state))
				atomic_set(&modem->cbp_data->
					   cbp_flow_ctrl->state,
					   FLOW_CTRL_DISABLE);
		} else {
			while (1) {	/*print added for testing,to be removed */
				if (modem->status == MD_OFF
				    ||
				    ((modem->status == MD_EXCEPTION
				      || modem->status == MD_EXCEPTION_ONGOING)
				     && (ch_id != EXCP_CTRL_CH_ID)
				     && (ch_id != EXCP_MSG_CH_ID))) {
					LOGPRT(LOG_ERR,
					       "%s %d: card is removed when channel%d flow is enable,data is dropped\n",
					       __func__, __LINE__, ch_id);
					sdio_tx_rx_printk(buf, 1);
					goto terminate;
				}

				if (modem_fc_flag < MODEM_FC_PRINT_MAX)
					LOGPRT(LOG_ERR,
					       "%s %d: channel%d flow ctrl before!\n",
					       __func__, __LINE__, ch_id);
				atomic_set(&modem->cbp_data->
					   cbp_flow_ctrl->state,
					   FLOW_CTRL_ENABLE);
				modem->cbp_data->
				    flow_ctrl_wait_event
				    (modem->cbp_data->cbp_flow_ctrl);
				if (modem_fc_flag < MODEM_FC_PRINT_MAX) {
					LOGPRT(LOG_ERR,
					       "%s %d: channel%d flow ctrl after!\n",
					       __func__, __LINE__, ch_id);
					modem_fc_flag++;
				}
				if (c2k_gpio_get_value
				    (modem->cbp_data->
				     cbp_flow_ctrl->wait_gpio) !=
				    modem->cbp_data->gpio_flow_ctrl_polar) {
					LOGPRT(LOG_ERR,
					       "%s %d: channel%d flow ctrl ok!\n",
					       __func__, __LINE__, ch_id);
					atomic_set(&modem->
						   cbp_data->cbp_flow_ctrl->
						   state, FLOW_CTRL_DISABLE);
					modem_fc_flag = 0;
					break;
				}
			}
		}
	}

	if ((modem->status == MD_EXCEPTION
	     || modem->status == MD_EXCEPTION_ONGOING)
	    && (ch_id != EXCP_CTRL_CH_ID) && (ch_id != EXCP_MSG_CH_ID)) {
		LOGPRT(LOG_ERR,
		       "%s %d: modem exception now. channel%d data is dropped\n",
		       __func__, __LINE__, ch_id);
		/* sdio_tx_rx_printk(buf, 1); */
		goto terminate;
	}

	/*temp. if you want to disable 4-line, simply unmark the next line. */
	/*modem->cbp_data->ipc_enable = 0; */
	/*if 4-line enabled, do this to make sure md is awake */
	if (modem->cbp_data->ipc_enable) {
		asc_tx_ready_count(modem->cbp_data->tx_handle->name, 1);
		tx_ready =
		    asc_tx_auto_ready(modem->cbp_data->tx_handle->name,
				      1);
		if (tx_ready != 0)
			asc_tx_ready_count(modem->cbp_data->
					   tx_handle->name, 0);
	}
	if (modem->status == MD_OFF) {
		LOGPRT(LOG_ERR,
		       "%s %d: card is removed when channel%d flow is enable,data is dropped\n",
		       __func__, __LINE__, ch_id);
		/* sdio_tx_rx_printk(buf, 1); */
		goto terminate;
	}
	if (func == modem->func && func && func->card) {
		sdio_claim_host(func);
	} else {
		LOGPRT(LOG_ERR,
		       "%s %d: func changed during writing, terminate\n",
		       __func__, __LINE__);
		goto terminate;
	}

	/*if hw just support one channel, cannot tx/rx at the same time, we should disable irq here */
	if (modem->cbp_data->tx_disable_irq) {
		ret = func_enable_irq(func, 0);
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: channel%d func_disable_irq failed ret=%d\n",
			       __func__, __LINE__, ch_id, ret);
			err_flag = 1;
			goto release_host;
		}
	}

	if (modem->cbp_data->data_ack_enable) {
		atomic_set(&modem->cbp_data->cbp_data_ack->state,
			   MODEM_ST_TX_RX);
	}

	if ((ch_id == DATA_CH_ID) || (ch_id == SDIO_AT_CHANNEL_NUM)
	    || (ch_id == SDIO_AT2_CHANNEL_NUM)
	    || (ch_id == SDIO_AT3_CHANNEL_NUM)
	    || (ch_id == EXCP_CTRL_CH_ID) || (ch_id == EXCP_MSG_CH_ID)
	    || (ch_id == AGPS_CH_ID) || (ch_id == MD_LOG_CH_ID)
	    || (ch_id == SDIO_AT4_CHANNEL_NUM) || (ch_id == SDIO_AT5_CHANNEL_NUM)
	    || (ch_id == SDIO_AT6_CHANNEL_NUM) || (ch_id == SDIO_AT7_CHANNEL_NUM)
	    || (ch_id == SDIO_AT8_CHANNEL_NUM)) {
		LOGPRT(LOG_NOTICE, "ch_id(%d)\n", ch_id);
		sdio_tx_rx_printk(buf, 1);
	}

#if PADDING_BY_BLOCK_SIZE
	/*sdio_tx_rx_printk(buf, 1); */
	block_cnt = len / DEFAULT_BLK_SIZE;
	if (len % DEFAULT_BLK_SIZE)
		padding_len = (block_cnt + 1) * DEFAULT_BLK_SIZE - len;

	LOGPRT(LOG_INFO, "padding len %d\n", padding_len);
	pad_begin_ptr = (char *)buf + len;

	if (padding_len) {
		memset(pad_begin_ptr, 0, padding_len);
		len += padding_len;
		msg_head->hw_head.len_hi = (len & 0xFF00) >> 8;
		msg_head->hw_head.len_low = len & 0xFF;
		LOGPRT(LOG_INFO, "change hw_head to %d %d\n",
		       msg_head->hw_head.len_low, msg_head->hw_head.len_hi);
	}
#endif

#ifdef TX_DONE_TRACE
	pr_debug("[C2K] dump before tx\n");
	msdc_c2k_dump_int_register();
#endif
	/*LOGPRT(LOG_DEBUG,  "%s %d: write %d bytes to addr 0x%x\n", __func__, __LINE__, len, addr); */
	if ((ch_id == CTRL_CH_ID) && len == 12) {
		pr_debug("[C2K SDIO] write ctrl channel start, len = %zd\n",
			 len);
	}
	ret = sdio_writesb(func, addr, buf, len);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: channel%d failed ret=%d\n", __func__,
		       __LINE__, ch_id, ret);
		err_flag = 1;
		goto release_host;
	}
	if (ch_id == CTRL_CH_ID)
		pr_debug("[C2K SDIO] write ctrl channel done, len = %zd\n",
			 len);

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	atomic_sub(len, &modem->tx_fifo_cnt);
#ifdef TX_DONE_TRACE
	mod_timer(&timer_wait_tx_done, jiffies + msecs_to_jiffies(1000));
#endif
#if USE_CCIF_INTR
	ccif_notify_c2k(TX_DATA_CH);
#endif
#endif
	/*LOGPRT(LOG_ERR,  "%s %d: channel%d data ack before!\n", __func__, __LINE__, index); */
	if (modem->cbp_data->data_ack_enable) {
		modem->cbp_data->data_ack_wait_event(modem->
						     cbp_data->cbp_data_ack);
	}
	/*LOGPRT(LOG_ERR,  "%s %d: channel%d data ack after!\n", __func__, __LINE__, index); */

	if (modem->cbp_data->tx_disable_irq) {
		ret = func_enable_irq(func, 1);
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: channel%d func_enable_irq failed ret=%d\n",
			       __func__, __LINE__, ch_id, ret);
			err_flag = 1;
		}
	}
 release_host:
	sdio_release_host(func);
	if (err_flag != 0) {
		LOGPRT(LOG_ERR,
		       "%s %d: channel%d ret =%d signal err to user space\n",
		       __func__, __LINE__, ch_id, ret);
		modem_err_indication_usr(1);
	}
 terminate:
	if (tx_ready == 0)
		asc_tx_ready_count(modem->cbp_data->tx_handle->name, 0);
}

static void modem_port_remove(struct sdio_modem *modem)
{
	struct sdio_modem_port *port;
	/*struct tty_struct *tty; */
	/*unsigned long flags = 0; */
	int index;

	LOGPRT(LOG_NOTICE, "%s %d: Enter.\n", __func__, __LINE__);

	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = modem->port[index];
		atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
		wake_up(&port->sflow_ctrl_wait_q);
		atomic_set(&modem->ctrl_port->sflow_ctrl_state,
			   SFLOW_CTRL_DISABLE);
		wake_up(&modem->ctrl_port->sflow_ctrl_wait_q);
		atomic_set(&modem->cbp_data->cbp_flow_ctrl->state,
			   FLOW_CTRL_DISABLE);
		wake_up(&modem->cbp_data->cbp_flow_ctrl->wait_q);
		atomic_set(&modem->cbp_data->cbp_data_ack->state,
			   MODEM_ST_READY);
		wake_up(&modem->cbp_data->cbp_data_ack->wait_q);
		if (port->write_q) {
			LOGPRT(LOG_NOTICE,
			       "%s %d: port%d cancel_work_sync before.\n",
			       __func__, __LINE__, index);
			cancel_work_sync(&port->write_work);
			LOGPRT(LOG_NOTICE,
			       "%s %d: port%d cancel_work_sync after.\n",
			       __func__, __LINE__, index);
			destroy_workqueue(port->write_q);
			LOGPRT(LOG_NOTICE,
			       "%s %d: port%d destroy queue after.\n", __func__,
			       __LINE__, index);
		}

		LOGPRT(LOG_NOTICE, "%s %d: sdio_modem_table cleared.\n",
		       __func__, __LINE__);
#if !ENABLE_CHAR_DEV
		mutex_lock(&port->port.mutex);
		port->func = NULL;
		tty = tty_port_tty_get(&port->port);
		/*tty_hangup is async so is this safe as is ?? */
		if (tty) {
			LOGPRT(LOG_NOTICE,
			       "%s %d destroy tty,index=%d port->index=%d\n",
			       __func__, __LINE__, index, port->index);
			tty_hangup(tty);
			tty_kref_put(tty);
		}
		mutex_unlock(&port->port.mutex);
#endif

		sdio_modem_tty_port_put(port);
	}
	LOGPRT(LOG_NOTICE, "%s %d: Leave.\n", __func__, __LINE__);
}

static void sdio_buffer_in_set_max_len(struct sdio_modem_port *port)
{
	unsigned int index = port->index;

	switch (index) {
	case 0:
		port->sdio_buf_in_max_num = SDIO_PPP_BUF_IN_MAX_NUM;
		break;
	case 1:
		port->sdio_buf_in_max_num = SDIO_ETS_BUF_IN_MAX_NUM;
		break;
	case 2:
		port->sdio_buf_in_max_num = SDIO_IFS_BUF_IN_MAX_NUM;
		break;
	case 3:
		port->sdio_buf_in_max_num = SDIO_AT_BUF_IN_MAX_NUM;
		break;
	case 4:
		port->sdio_buf_in_max_num = SDIO_PCV_BUF_IN_MAX_NUM;
		break;
	default:
		port->sdio_buf_in_max_num = SDIO_DEF_BUF_IN_MAX_NUM;
		break;
	}
}

static int sdio_modem_port_init(struct sdio_modem_port *port, int index)
{
	int ret = 0;
	/*unsigned long flags = 0; */

	kref_init(&port->kref);
	spin_lock_init(&port->write_lock);

	if (kfifo_alloc(&port->transmit_fifo, FIFO_SIZE, GFP_KERNEL)) {
		LOGPRT(LOG_ERR, "%s %d : Couldn't allocate transmit_fifo\n",
		       __func__, __LINE__);
		return -ENOMEM;
	}

	/*create port's write work queue */
	port->name = "modem_sdio_write_wq";
	snprintf(port->work_name, 64, "%s%d", port->name, index);
	port->write_q = create_singlethread_workqueue(port->work_name);
	if (port->write_q == NULL) {
		LOGPRT(LOG_ERR, "%s %d error creat write workqueue\n", __func__,
		       __LINE__);
		return -ENOMEM;
	}

	port->index = index;

#if ENABLE_CCMNI
	if (port->index != CCMNI_CH_ID - 1) {
		INIT_WORK(&port->write_work, sdio_write_port_work);
	} else {
		INIT_WORK(&port->write_ccmni_work, sdio_write_ccmni_work);
		INIT_WORK(&port->write_work, sdio_write_port_work);
	}
	port->tx_state = CCMNI_TX_READY;
	spin_lock_init(&port->tx_state_lock);
#else
	INIT_WORK(&port->write_work, sdio_write_port_work);
#endif

	mutex_init(&port->sdio_buf_in_mutex);
	INIT_LIST_HEAD(&port->sdio_buf_in_list);
	port->sdio_buf_in = 0;
	port->sdio_buf_in_num = 0;
	port->sdio_buf_in_size = 0;
	sdio_buffer_in_set_max_len(port);

	port->tx_count = 0;
	port->rx_count = 0;

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	mutex_init(&port->sdio_assemble_mutex);
	INIT_LIST_HEAD(&port->sdio_assemble_list);
#endif

	init_waitqueue_head(&port->sflow_ctrl_wait_q);
	atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
	sema_init(&port->write_sem, 1);

#if ENABLE_CHAR_DEV
	init_waitqueue_head(&port->rx_wq);
#endif

	return ret;
}

static ssize_t modem_log_level_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	char *s = buf;

	s += snprintf(s, 32, "%d\n", sdio_log_level);

	return s - buf;
}

static ssize_t modem_log_level_store(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	sdio_log_level = val;

	return n;
}

static ssize_t modem_refer_show(struct kobject *kobj,
				struct kobj_attribute *attr, char *buf)
{
	char *s = buf;
	struct sdio_modem *modem = c2k_modem;
	int i = 0;

	if (!modem)
		return -ENODEV;

	for (i = 0; i < SDIO_TTY_NR; i++) {
		s += snprintf(s, 64, "TTY port%d Tx:  times %d\n", i,
			     modem->port[i]->tx_count);
		s += snprintf(s, 64, "\n");
		s += snprintf(s, 64, "TTY port%d Rx:  times %d\n", i,
			     modem->port[i]->rx_count);
	}
	return s - buf;
}

static ssize_t modem_refer_store(struct kobject *kobj,
				 struct kobj_attribute *attr, const char *buf,
				 size_t n)
{
	return n;
}

static ssize_t modem_ctrl_on_show(struct kobject *kobj,
				  struct kobj_attribute *attr, char *buf)
{
	struct sdio_modem_ctrl_port *ctrl_port = NULL;
	struct sdio_modem *modem = c2k_modem;
	/*struct sdio_modem_port *port; */
	char *s = buf;
	/*int ret=-1; */

	LOGPRT(LOG_NOTICE, "%s: enter\n", __func__);

	ctrl_port = modem->ctrl_port;

/*out:*/
	s += snprintf(s, 64, "ctrl state: %s\n",
		     ctrl_port->chan_state ? "enable" : "disable");
	return s - buf;
}

static ssize_t modem_ctrl_on_store(struct kobject *kobj,
				   struct kobj_attribute *attr, const char *buf,
				   size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val)
		modem_on_off_ctrl_chan(1);
	else
		modem_on_off_ctrl_chan(0);

	return n;
}

static ssize_t modem_dtr_send_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	/*char query_mode=1; */
	int status = -1, ret = -1;
	char *s = buf;

	LOGPRT(LOG_NOTICE, "%s: enter\n", __func__);

	status = modem_dcd_state();
	if (ret < 0) {
		LOGPRT(LOG_NOTICE,
		       "query cp ctrl channel state failed ret=%d\n", ret);
	}
	s += snprintf(s, 64, "ctrl state: %d\n", status);

	return s - buf;
}

static ssize_t modem_dtr_send_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	dtr_value = val;
	modem_dtr_set(val, 1);

	return n;
}

static ssize_t modem_dtr_query_show(struct kobject *kobj,
				    struct kobj_attribute *attr, char *buf)
{
	/*char query_mode=1; */
	int status = -1, ret = -1;
	char *s = buf;

	LOGPRT(LOG_NOTICE, "%s: enter\n", __func__);

	status = modem_dcd_state();
	if (status < 0) {
		LOGPRT(LOG_NOTICE,
		       "query cp ctrl channel state failed ret=%d\n", ret);
		s += snprintf(s, 64, "ctrl state: %s\n", "N/A");
	} else {
		s += snprintf(s, 64, "ctrl state: %d\n", status);
	}
	return s - buf;
}

static ssize_t modem_dtr_query_store(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     const char *buf, size_t n)
{
	unsigned long val;
	/*int data; */
	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	modem_dcd_state();
	return n;
}

static unsigned char loop_back_chan;
static ssize_t modem_loop_back_chan_show(struct kobject *kobj,
					 struct kobj_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t modem_loop_back_chan_store(struct kobject *kobj,
					  struct kobj_attribute *attr,
					  const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;
	if (val < 6) {
		loop_back_chan = val;
	} else {
		LOGPRT(LOG_ERR, "%s %d error channel select, please < 6!\n",
		       __func__, __LINE__);
	}

	return n;
}

static ssize_t modem_loop_back_mod_show(struct kobject *kobj,
					struct kobj_attribute *attr, char *buf)
{
	return 0;
}

static ssize_t modem_loop_back_mod_store(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	if (val < 0)
		return -EINVAL;

	if (val < 4) {
		modem_loop_back_chan(loop_back_chan, val);
	} else {
		LOGPRT(LOG_ERR,
		       "%s %d error channel select, please check the option!\n",
		       __func__, __LINE__);
	}
	return n;
}

static ssize_t modem_ack_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	char *s = buf;
	struct sdio_modem *modem = c2k_modem;
	struct cbp_platform_data *cbp_pdata = NULL;

	/*int ret =-1; */

	if (modem)
		cbp_pdata = modem->cbp_data;

	if ((cbp_pdata != NULL) && (cbp_pdata->cbp_data_ack != NULL)) {
		s += snprintf(s, 64, "gpio[%d]\t state:[%d]\t polar[%d]\t ",
			     cbp_pdata->cbp_data_ack->wait_gpio,
			     atomic_read(&cbp_pdata->cbp_data_ack->state),
			     cbp_pdata->cbp_data_ack->wait_polar);

		s += snprintf(s, 64, "stored:[%d]\n",
			     c2k_gpio_get_value(modem->cbp_data->
						cbp_flow_ctrl->wait_gpio));
	}
/*out:*/
	return s - buf;
}

static ssize_t modem_ack_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{

	return n;
}

static ssize_t modem_flw_show(struct kobject *kobj, struct kobj_attribute *attr,
			      char *buf)
{
	char *s = buf;
	struct sdio_modem *modem = c2k_modem;
	struct cbp_platform_data *cbp_pdata = NULL;
	/*struct sdio_modem_port *port; */
	/*int ret =-1; */

	if (modem)
		cbp_pdata = modem->cbp_data;

	if ((cbp_pdata != NULL) && (cbp_pdata->cbp_flow_ctrl != NULL)) {
		s += snprintf(s, 64, "gpio[%d] \tstate:[%d]\t polar[%d]\t ",
			     cbp_pdata->cbp_flow_ctrl->wait_gpio,
			     atomic_read(&cbp_pdata->cbp_flow_ctrl->state),
			     cbp_pdata->cbp_flow_ctrl->wait_polar);

		s += snprintf(s, 64, "stored:[%d]\n",
			     c2k_gpio_get_value(modem->cbp_data->
						cbp_flow_ctrl->wait_gpio));
	}
/*out:*/
	return s - buf;
}

static ssize_t modem_flw_store(struct kobject *kobj,
			       struct kobj_attribute *attr, const char *buf,
			       size_t n)
{

	return n;
}

#define modem_attr(_name) \
static struct kobj_attribute _name##_attr = {	\
	.attr	= {				\
		.name = __stringify(_name),	\
		.mode = 0660,			\
	},					\
	.show	= modem_##_name##_show,			\
	.store	= modem_##_name##_store,		\
}
modem_attr(log_level);
modem_attr(refer);
modem_attr(ctrl_on);
modem_attr(dtr_send);
modem_attr(dtr_query);
modem_attr(loop_back_chan);
modem_attr(loop_back_mod);
modem_attr(ack);
modem_attr(flw);

static struct attribute *modem_sdio_attr[] = {
	&log_level_attr.attr,
	&refer_attr.attr,
	&ctrl_on_attr.attr,
	&dtr_send_attr.attr,
	&dtr_query_attr.attr,
	&loop_back_chan_attr.attr,
	&loop_back_mod_attr.attr,
	&ack_attr.attr,
	&flw_attr.attr,
	NULL,
};

/*static struct kobject *modem_sdio_kobj;*/
static struct attribute_group g_modem_attr_group = {
	.attrs = modem_sdio_attr,
};

int sdio_rawbulk_intercept(int port_num, unsigned int inception)
{
	int ret = -ENODEV;
	struct sdio_modem_port *port;

	if (port_num >= RAWBULK_TID_AT)	/*skip flashless port */
		port_num++;
	port = sdio_modem_tty_port_get(port_num);

	if (!port || !port->func) {
		LOGPRT(LOG_ERR, "%s %d failed\n", __func__, __LINE__);
		return ret;
	}
	LOGPRT(LOG_DEBUG, "modem inception = %d\n", inception);
	spin_lock(&port->inception_lock);
	if ((!!inception) == port->inception) {
		spin_unlock(&port->inception_lock);
		return 0;
	}
	spin_unlock(&port->inception_lock);

	spin_lock(&port->inception_lock);
	if (inception != port->inception)
		port->inception = !!inception;
	spin_unlock(&port->inception_lock);

	return 0;
}

int modem_buffer_push(int port_num, void *buf, int count)
{
	int ret, data_len;
	unsigned long flags;
	struct sdio_modem *modem;
	int chars_in_fifo = 0;
	struct sdio_modem_port *port;

	if (port_num >= RAWBULK_TID_AT)	/*skip flashless port */
		port_num++;
	port = sdio_modem_tty_port_get(port_num);

	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d invalid port\n", __func__, __LINE__);
		return ret;
	}

	modem = port->modem;

	if (count == 0)
		return 0;

	spin_lock_irqsave(&port->write_lock, flags);
	data_len = FIFO_SIZE - kfifo_len(&port->transmit_fifo);
	spin_unlock_irqrestore(&port->write_lock, flags);
	if (data_len < count) {
		LOGPRT(LOG_DEBUG, "%s %d: SDIO driver buffer is full!\n",
		       __func__, __LINE__);
		return -ENOMEM;
	}

	spin_lock_irqsave(&modem->status_lock, flags);
	if (modem->status != MD_OFF) {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		chars_in_fifo = kfifo_len(&port->transmit_fifo);
		if (count > (ONE_PACKET_MAX_SIZE - chars_in_fifo)) {
			pr_debug("[C2K] port%d too many data pending...\n",
				 port->index);
			return -ENOMEM;
		}
		ret =
		    kfifo_in_locked(&port->transmit_fifo, buf, count,
				    &port->write_lock);
		queue_work(port->write_q, &port->write_work);
	} else {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		LOGPRT(LOG_NOTICE, "%s %d: port%d is removed!\n", __func__,
		       __LINE__, port->index);
	}

	return 0;
}

#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
static int via_sdio_probe_func(struct sdio_modem *modem, struct sdio_func *func)
{
	int ret = 0;
	int index = 0;
	struct sdio_modem_port *port = NULL;
	unsigned long flags;

	sdio_claim_host(func);

	ret = sdio_enable_func(func);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d sdio enable func failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto err_enable_func;
	}

	ret = sdio_set_block_size(func, 512);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: set block size failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto error_set_block_size;
	}

	sdio_writeb(func, 0x01, 0x28, &ret);
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: sdio_writeb 0x28 failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto error_set_block_size;
	}

	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = modem->port[index];
		port->func = func;
		/*LOGPRT(LOG_INFO,  "%s %d %d port(0x%x), func(0x%x).\n",__func__,__LINE__, index, port, port->func); */
	}

	ret = sdio_claim_irq(func, modem_sdio_irq);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d sdio claim irq failed.\n", __func__,
		       __LINE__);
		goto err_sdio_claim_irq;
	}

	spin_lock_irqsave(&modem->status_lock, flags);
	/*when md exception, sdio will be remounted. but we should keep status not changed. */
	if (modem->status != MD_EXCEPTION) {
		modem->status = MD_READY;
		LOGPRT(LOG_INFO, "%s %d: set md status ready.\n", __func__,
		       __LINE__);
	}
	spin_unlock_irqrestore(&modem->status_lock, flags);

	LOGPRT(LOG_NOTICE, "%s %d: exit.\n", __func__, __LINE__);
	sdio_release_host(func);

	return ret;

 err_sdio_claim_irq:
 error_set_block_size:
	sdio_disable_func(func);
 err_enable_func:
	sdio_release_host(func);

	return ret;

}
#else

static int sdio_pio_func_wait_rdy(struct sdio_func *func)
{
	unsigned int cnt = 500;
	int raw_val;
	int ret = 0;

	while (cnt--) {
		ret = sdio_func1_rd(func, &raw_val, SDIO_CCIR, sizeof(raw_val));
		if (ret) {
			LOGPRT(LOG_ERR, "%s %d: read SDIO_CCIR failed ret=%d\n",
			       __func__, __LINE__, ret);
			msleep(100);
			continue;
		}

		if (raw_val &
		    (SDIO_CCIR_F_FUNC_RDY | SDIO_CCIR_G_FUNC_RDY |
		     SDIO_CCIR_B_FUNC_RDY)) {
			return 0;
		}
		msleep(100);
	}

	return -1;
}

/*should already claimed host when call this function*/
int sdio_pio_get_drv_own(struct sdio_func *func, struct sdio_modem *modem)
{
	int ret;
	int raw_val;
	unsigned int cnt = 500;

	if ((func == NULL) || (modem == NULL))
		return -1;

	/*already get the driver own */
	if (modem->fw_own == 0)
		return 0;

	raw_val = SDIO_CHLPCR_FW_OWN_REQ_CLR;
	ret = sdio_func1_wr(func, SDIO_CHLPCR, &raw_val, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: write SDIO_CHLPCR failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	/*check */
	while (cnt--) {
		ret =
		    sdio_func1_rd(func, &raw_val, SDIO_CHLPCR, sizeof(raw_val));
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: read SDIO_CHLPCR failed ret=%d\n",
			       __func__, __LINE__, ret);
			return -1;
		}

		if (modem->fw_own == 0)
			return 0;	/*set by interrupt handler */

		if (raw_val & SDIO_CHLPCR_FW_OWN_REQ_SET) {	/*Read will get the DRV own status */
			modem->fw_own = 0;
			break;
		}
		msleep(20);
	}

	return (modem->fw_own == 0) ? 0 : (-1);
}

/*should already claimed host when call this function*/
int sdio_pio_give_fw_own(struct sdio_func *func, struct sdio_modem *modem)
{
	unsigned int raw_val;
	int ret;
	int cnt = 500;

	int int_enable, err;

	int_enable = sdio_f0_readb(func, 0x04, &err);
	if (err)
		LOGPRT(LOG_ERR, "%s %d read interrupt enable reg fail %d.\n",
		       __func__, __LINE__, err);
	else {
		LOGPRT(LOG_INFO,
		       "%s %d read interrupt enable reg success 0x%x.\n",
		       __func__, __LINE__, int_enable);
	}

	raw_val = SDIO_CHLPCR_FW_OWN_REQ_SET;
	ret = sdio_func1_wr(func, SDIO_CHLPCR, &raw_val, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: write SDIO_CHLPCR failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	/*check */
	while (cnt--) {
		ret =
		    sdio_func1_rd(func, &raw_val, SDIO_CHLPCR, sizeof(raw_val));
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: read SDIO_CHLPCR failed ret=%d\n",
			       __func__, __LINE__, ret);
			return -1;
		}

		if ((raw_val & SDIO_CHLPCR_FW_OWN_REQ_SET) == 0) {	/*Read will get the DRV own status */
			modem->fw_own = 1;
			break;
		}
		if (cnt % 10 == 0) {
			LOGPRT(LOG_ERR, "%s %d: polling fw own retry (0x%x)\n",
			       __func__, __LINE__, raw_val);
#if 0
			int_enable = sdio_f0_readb(func, 0x04, &err);
			if (err)
				LOGPRT(LOG_ERR,
				       "%s %d read interrupt enable reg fail %d.\n",
				       __func__, __LINE__, err);
			else {
				LOGPRT(LOG_INFO,
				       "%s %d read interrupt enable reg success 0x%x.\n",
				       __func__, __LINE__, int_enable);
			}
#endif
		}
		msleep(20);
	}

	return (modem->fw_own == 1) ? 0 : (-1);
}

int sdio_pio_enable_com_interrupt(struct sdio_func *func)
{
	unsigned int raw_val;
	int ret = 0;

	ret = sdio_func1_rd(func, &raw_val, SDIO_CHLPCR, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CHLPCR failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	if (raw_val & SDIO_CHLPCR_INT_EN_SET) {
		LOGPRT(LOG_ERR,
		       "%s %d: The interrupt of pio-based function have been enabled\n",
		       __func__, __LINE__);
		return 0;
	}

	/*raw_val |= SDIO_CHLPCR_INT_EN_SET; */
	raw_val = SDIO_CHLPCR_INT_EN_SET;

	ret = sdio_func1_wr(func, SDIO_CHLPCR, &raw_val, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: Enable pio-based function's interrupt failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	LOGPRT(LOG_INFO,
	       "%s %d: Enable pio-based function's com interrupt success (0x%x)\n",
	       __func__, __LINE__, raw_val);

	ret = sdio_func1_rd(func, &raw_val, SDIO_CHLPCR, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CHLPCR failed ret=%d\n",
		       __func__, __LINE__, ret);
		/*return -1; */
	} else
		LOGPRT(LOG_INFO, "%s %d: read SDIO_CHLPCR success 0x%x\n",
		       __func__, __LINE__, raw_val);

	return 0;
}

int sdio_pio_disable_com_interrupt(struct sdio_func *func)
{
	unsigned int raw_val;
	int ret = 0;

	ret = sdio_func1_rd(func, &raw_val, SDIO_CHLPCR, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CHLPCR failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	if (!(raw_val & SDIO_CHLPCR_INT_EN_SET)) {
		LOGPRT(LOG_ERR,
		       "%s %d: The interrupt of pio-based function have not been enabled\n",
		       __func__, __LINE__);
		return 0;
	}

	/*raw_val |= SDIO_CHLPCR_INT_EN_CLR; */
	raw_val = SDIO_CHLPCR_INT_EN_CLR;

	ret = sdio_func1_wr(func, SDIO_CHLPCR, &raw_val, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: Disable pio-based function's interrupt failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	LOGPRT(LOG_INFO,
	       "%s %d: Disable pio-based function's interrupt success (0x%x)\n",
	       __func__, __LINE__, raw_val);

	ret = sdio_func1_rd(func, &raw_val, SDIO_CHLPCR, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CHLPCR failed ret=%d\n",
		       __func__, __LINE__, ret);
		/*return -1; */
	} else
		LOGPRT(LOG_INFO, "%s %d: read SDIO_CHLPCR success 0x%x\n",
		       __func__, __LINE__, raw_val);

	return 0;
}

int sdio_pio_set_int_clr_ctl(struct sdio_func *func, enum sdio_int_clr_ctl mod)
{
	unsigned int raw_val = 0;
	int ret;

	ret = sdio_func1_rd(func, &raw_val, SDIO_CHCR, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CHCR failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	if (mod == SDIO_INT_CTL_W1C) {
		raw_val |= SDIO_CHCR_INT_CLR_CTRL;
		ret = sdio_func1_wr(func, SDIO_CHCR, &raw_val, sizeof(raw_val));
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: Set interrupt write 1 clear failed ret=%d\n",
			       __func__, __LINE__, ret);
			return -1;
		}
	}

	if (mod == SDIO_INT_CTL_RC) {
		raw_val &= ~SDIO_CHCR_INT_CLR_CTRL;
		ret = sdio_func1_wr(func, SDIO_CHCR, &raw_val, sizeof(raw_val));
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: Set interrupt read clear failed ret=%d\n",
			       __func__, __LINE__, ret);
			return -1;
		}
	}

	return 0;
}

int sdio_pio_set_int_mask(struct sdio_func *func, int set_bits, int clr_bits)
{
	struct sdio_modem *modem = sdio_get_drvdata(func);
	int mask = 0;
	int ret = 0;
	int loop = 0;

	mask |= set_bits;
	mask &= ~clr_bits;

	ret = sdio_func1_wr(func, SDIO_CHIER, &mask, sizeof(mask));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: Set interrupt mask failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}
	modem->int_mask.raw_val = mask;
	LOGPRT(LOG_INFO, "%s %d: set interrupt mask success (0x%x)\n", __func__,
	       __LINE__, mask);

	for (loop = 0; loop < 1; loop++) {
		ret = sdio_func1_rd(func, &mask, SDIO_CHIER, sizeof(mask));
		if (ret) {
			LOGPRT(LOG_ERR,
			       "%s %d: read SDIO_CHIER failed ret=%d\n",
			       __func__, __LINE__, ret);
			/*return -1; */
		} else
			LOGPRT(LOG_INFO,
			       "%s %d: read SDIO_CHIER success 0x%x\n",
			       __func__, __LINE__, mask);
		/*msleep(100); */
	}
	return 0;
}

/*should already claimed host when call this function*/
int sdio_pio_enable_async_interrupt(struct sdio_func *func)
{
	unsigned int raw_val;
	int ret = 0;

	ret = sdio_func1_rd(func, &raw_val, SDIO_CSDIOCSR, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CSDIOCSR failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	if (raw_val & SDIO_CSDIOCSR_SDIO_INT_CTL) {
		LOGPRT(LOG_INFO,
		       "%s %d: The async interrupt of pio-based function have been enabled\n",
		       __func__, __LINE__);
		return 0;
	}

	raw_val |= SDIO_CSDIOCSR_SDIO_INT_CTL;

	ret = sdio_func1_wr(func, SDIO_CSDIOCSR, &raw_val, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: Enable pio-based function's async interrupt failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	LOGPRT(LOG_INFO,
	       "%s %d: Enable pio-based function's async interrupt success\n",
	       __func__, __LINE__);

	return 0;
}

int sdio_pio_disable_async_interrupt(struct sdio_func *func)
{
	unsigned int raw_val;
	int ret = 0;

	ret = sdio_func1_rd(func, &raw_val, SDIO_CSDIOCSR, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: read SDIO_CSDIOCSR failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	if (!(raw_val & SDIO_CSDIOCSR_SDIO_INT_CTL)) {
		LOGPRT(LOG_INFO,
		       "%s %d: The async interrupt of pio-based function have been disabled\n",
		       __func__, __LINE__);
		return 0;
	}

	raw_val &= ~SDIO_CSDIOCSR_SDIO_INT_CTL;

	ret = sdio_func1_wr(func, SDIO_CSDIOCSR, &raw_val, sizeof(raw_val));
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: Disable pio-based function's async interrupt failed ret=%d\n",
		       __func__, __LINE__, ret);
		return -1;
	}

	LOGPRT(LOG_INFO,
	       "%s %d: Disable pio-based function's async interrupt success\n",
	       __func__, __LINE__);
	return 0;
}

int check_img_header(struct sdio_modem *modem)
{
	struct file *filp = NULL;
	struct inode *inode = NULL;
	loff_t fsize;
	loff_t offset;
	/*loff_t *pos; */
	mm_segment_t old_fs;
	int ret = 0;
	struct md_check_header *header = NULL;
	struct ccci_image_info *img;
	char *img_str = c2k_img_info_str;
	static int check_init;

	if (check_init) {
		LOGPRT(LOG_INFO, "%s: header already checked!\n", __func__);
		LOGPRT(LOG_INFO, "%s\n", c2k_img_info_str);
		return 0;
	}

	filp = filp_open(C2K_IMG_PATH, O_RDONLY, 0644);
	if (IS_ERR(filp)) {
		LOGPRT(LOG_ERR, "%s: open c2k md image fail!\n", __func__);
		return -1;
	}

	if (!modem) {
		LOGPRT(LOG_ERR, "%s: sdio_modem NULL, exit!\n", __func__);
		goto close_out;
	}
	img = &modem->img_info;

	inode = filp->f_dentry->d_inode;

	fsize = inode->i_size;
	LOGPRT(LOG_INFO, "%s: %s size %lld\n", __func__, C2K_IMG_PATH, fsize);

	if (fsize < sizeof(struct md_check_header)) {
		LOGPRT(LOG_ERR, "%s: %s size too small, go out!\n", __func__,
		       C2K_IMG_PATH);
		goto close_out;
	}

	offset = 0 - sizeof(struct md_check_header);

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	header = kzalloc(sizeof(struct md_check_header), GFP_KERNEL);
	if (!header) {
		LOGPRT(LOG_ERR, "%s: alloc check header fail!\n", __func__);
		goto close_out;
	}
	/*seek to the begin of check header */
	filp->f_op->llseek(filp, offset, SEEK_END);
	ret =
	    filp->f_op->read(filp, (char *)header,
			     sizeof(struct md_check_header), &filp->f_pos);

	set_fs(old_fs);

	if (ret > 0) {
		ret = strncmp(header->check_header, MD_HEADER_MAGIC_NO, 12);
		if (ret) {
			LOGPRT(LOG_ERR, "md check header not exist!\n");
			ret = 0;
		} else {
			img->ap_info.image_type = type_str[header->image_type];
			img->ap_info.platform = get_ap_platform();
			img->ap_info.mem_size = get_c2k_reserve_mem_size();
			img->img_info.image_type = type_str[header->image_type];
			img->img_info.platform = header->platform;
			img->img_info.build_time = header->build_time;
			img->img_info.build_ver = header->build_ver;
			img->img_info.product_ver =
			    product_str[header->product_ver];
			img->img_info.version = header->product_ver;
			img->img_info.mem_size = header->mem_size;

			/*LOGPRT(LOG_INFO,  "%s: platform = %s, build_time = %s, build_ver = %s\n",
			   __func__, img_info->platform, img_info->build_time, img_info->build_ver); */

			snprintf(img_str, 256,
				"MD:%s*%s*%s*%s*%s\nAP:%s*%s*%08x (MD)%08x\n",
				img->img_info.image_type,
				img->img_info.platform, img->img_info.build_ver,
				img->img_info.build_time,
				img->img_info.product_ver,
				img->ap_info.image_type, img->ap_info.platform,
				img->ap_info.mem_size, img->img_info.mem_size);

			LOGPRT(LOG_INFO, "%s\n", img_str);
			check_init = 1;
		}
	} else {
		LOGPRT(LOG_ERR,
		       "%s: read c2k md img fail, cannot get check header!\n",
		       __func__);
	}

 close_out:
	filp_close(filp, NULL);

	return 0;
}

static int c2k_sdio_probe_func(struct sdio_modem *modem, struct sdio_func *func)
{
	struct sdio_modem_port *port = NULL;
	int ret = 0;
	int index = 0;
	unsigned long flags;
	struct sdio_buf_in_packet *packet = NULL;

	sdio_claim_host(func);

	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = modem->port[index];
		port->func = func;
		atomic_set(&port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
	}
	modem->func = func;

	ret = sdio_claim_irq(func, sdio_pio_intr_handler);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d sdio claim irq failed.\n", __func__,
		       __LINE__);
		goto err_sdio_modem_port_init;
	}

	ret = sdio_enable_func(func);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d sdio enable func failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto err_enable_func;
	}

	ret = sdio_pio_func_wait_rdy(func);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: wait func ready failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto disable_func;
	}

	LOGPRT(LOG_INFO, "%s %d: func ready\n", __func__, __LINE__);

	ret = sdio_set_block_size(func, DEFAULT_BLK_SIZE);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: set block size failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto disable_func;
	}

	ret = sdio_pio_get_drv_own(func, modem);
	if (ret) {
		LOGPRT(LOG_ERR, "%s %d: get driver own failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto disable_func;
	}

	ret = sdio_pio_disable_com_interrupt(func);
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: disable interrupt output own failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto give_own;
	}
	LOGPRT(LOG_INFO, "%s %d: sdio_pio_disable_com_interrupt done\n",
	       __func__, __LINE__);

	ret = sdio_pio_set_int_clr_ctl(func, SDIO_INT_CTL_W1C);
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: set int write 1 clear failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto give_own;
	}
	modem->int_clr_ctl = SDIO_INT_CTL_W1C;
	LOGPRT(LOG_INFO, "%s %d: sdio_pio_set_int_clr_ctl done\n", __func__,
	       __LINE__);

	ret = sdio_pio_enable_async_interrupt(func);
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: enable async interrupt failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto give_own;
	}
	LOGPRT(LOG_INFO, "%s %d: sdio_pio_enable_async_interrupt done\n",
	       __func__, __LINE__);

	ret = sdio_pio_set_int_mask(func,	/*SDIO_CHIER_FW_OWN_BACK_EN| */
				    SDIO_CHIER_RX_RDY_EN |
				    SDIO_CHIER_TX_EMPTY_EN |
				    SDIO_CHIER_TX_UNDER_THOLD_EN |
				    SDIO_CHIER_TX_OVERFLOW_EN |
				    SDIO_CHIER_FW_INT_INDICATOR_EN |
				    SDIO_CHIER_FIRMWARE_INT_EN, 0);
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: set interrupts' mask failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto give_own;
	}
	LOGPRT(LOG_INFO, "%s %d: sdio_pio_set_int_mask done\n", __func__,
	       __LINE__);

	spin_lock_irqsave(&modem->status_lock, flags);
	/*when md exception, sdio will be remounted. but we should keep status not changed. */
	if (modem->status != MD_EXCEPTION) {
		del_timer(&modem->heart_beat_timer);
		del_timer(&modem->poll_timer);
		del_timer(&modem->force_assert_timer);
		modem->status = MD_READY;
		/*enable 4-line sync */
		modem->cbp_data->ipc_enable = true;
		LOGPRT(LOG_INFO, "%s %d: set md status ready.\n", __func__,
		       __LINE__);
		spin_unlock_irqrestore(&modem->status_lock, flags);
		cancel_work_sync(&modem->force_assert_work);
		cancel_work_sync(&modem->poll_hb_work);
#if ENABLE_CHAR_DEV
		port = modem->port[MD_LOG_CH_ID - 1];
		mutex_lock(&port->sdio_buf_in_mutex);
		/*clear pending data for mdlog channel */
		while (!list_empty(&port->sdio_buf_in_list)) {
			packet =
			    list_first_entry(&port->sdio_buf_in_list,
					     struct sdio_buf_in_packet, node);
			list_del(&packet->node);
			port->sdio_buf_in_num--;
			port->sdio_buf_in_size -= packet->o_size;
			kfree(packet->buffer);
			kfree(packet);
		}
		mutex_unlock(&port->sdio_buf_in_mutex);
		LOGPRT(LOG_INFO,
		       "%s: clear pending data for log channel done\n",
		       __func__);
#endif

	} else {
		spin_unlock_irqrestore(&modem->status_lock, flags);
	}
	LOGPRT(LOG_INFO, "%s %d: md status (%d).\n", __func__, __LINE__,
	       modem->status);

	check_img_header(modem);

	ret = sdio_pio_enable_com_interrupt(func);
	if (ret) {
		LOGPRT(LOG_ERR,
		       "%s %d: enable interrupt out failed with ret = %d\n",
		       __func__, __LINE__, ret);
		goto give_own;
	}
	LOGPRT(LOG_INFO, "%s %d: sdio_pio_enable_com_interrupt done\n",
	       __func__, __LINE__);

	LOGPRT(LOG_NOTICE, "%s %d: exit.\n", __func__, __LINE__);
	sdio_release_host(func);

	return ret;

 err_sdio_modem_port_init:
	modem_port_remove(modem);
	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = modem->port[index];
		kfree(port);
		port = NULL;
	}
 give_own:
	sdio_pio_give_fw_own(func, modem);

 disable_func:
	sdio_disable_func(func);
 err_enable_func:
	sdio_release_host(func);
	return ret;

}
#endif

static int modem_sdio_probe(struct sdio_func *func,
			    const struct sdio_device_id *id)
{
	struct sdio_modem *modem = NULL;
	int ret = 0;
	int index = 0;
	/*unsigned long flags; */

	LOGPRT(LOG_NOTICE, "%s %d: enter.\n", __func__, __LINE__);
	LOGPRT(LOG_INFO, "modem_sdio_probe from %ps\n",
	       __builtin_return_address(0));

	modem = c2k_modem;
	modem->func = func;
	sdio_set_drvdata(func, modem);

#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
	ret = via_sdio_probe_func(modem, func);
#else
	c2k_sdio_install_eirq();
	ret = c2k_sdio_probe_func(modem, func);
#endif

	if (ret)
		return ret;

	for (index = 0; index < SDIO_TTY_NR; index++)
		rawbulk_bind_sdio_channel(index);

	SRC_trigger_signal(1);
	/*VIA_trigger_signal(2); */
	return ret;

}

void modem_reset_handler(void)
{
	struct sdio_modem *modem = c2k_modem;
	/*struct sdio_func *func = modem->func; */
	struct sdio_func *func = NULL;

	int ret = -1;
	unsigned long flags;

	LOGPRT(LOG_INFO, "%s %d: Enter.\n", __func__, __LINE__);
	if (modem == NULL) {
		LOGPRT(LOG_ERR, "%s %d: modem is NULL.\n", __func__, __LINE__);
		goto out;
	}

	spin_lock_irqsave(&modem->status_lock, flags);
	if (modem->status == MD_OFF) {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		asc_tx_reset(modem->cbp_data->tx_handle->name);	/*for IPO-H */
		LOGPRT(LOG_ERR, "%s %d: md removed already.\n", __func__,
		       __LINE__);
		goto out;
	}

	spin_unlock_irqrestore(&modem->status_lock, flags);

	asc_tx_reset(modem->cbp_data->tx_handle->name);
	func = modem->func;

	spin_lock_irqsave(&modem->status_lock, flags);
	/*when md exception, we will trigger this reset function. but we should keep status not changed. */
	if (modem->status != MD_EXCEPTION) {
		modem->status = MD_OFF;
		LOGPRT(LOG_INFO, "%s %d: set md status off.\n", __func__,
		       __LINE__);
	}
	spin_unlock_irqrestore(&modem->status_lock, flags);
	LOGPRT(LOG_NOTICE, "%s %d: cancel_work_sync(&dtr_work) before.\n",
	       __func__, __LINE__);
	cancel_work_sync(&modem->dtr_work);
	LOGPRT(LOG_NOTICE, "%s %d: cancel_work_sync(&dtr_work) after.\n",
	       __func__, __LINE__);
	cancel_work_sync(&modem->dcd_query_work);
	LOGPRT(LOG_NOTICE, "%s %d: cancel_work_sync(&dcd_query_work) after.\n",
	       __func__, __LINE__);
	dcd_state = 0;

	/*modem_port_remove(modem); */

	sdio_claim_host(func);
	ret = sdio_disable_func(func);
	if (ret < 0)
		LOGPRT(LOG_ERR, "%s: sdio_disable_func failed.\n", __func__);

	ret = sdio_release_irq(func);
	if (ret < 0)
		LOGPRT(LOG_ERR, "%s: sdio_release_irq failed.\n", __func__);

	sdio_release_host(func);
 out:
	LOGPRT(LOG_INFO, "%s %d: Leave.\n", __func__, __LINE__);
}

void modem_pre_stop(void)
{
	struct sdio_modem *modem = c2k_modem;
	/*struct sdio_func *func = modem->func; */
	struct sdio_func *func = NULL;
	int ret = 0;

	func = modem->func;

	if (!func || !func->card) {
		LOGPRT(LOG_INFO, "%s %d: card removed, exit.\n", __func__, __LINE__);
		return;
	}
	LOGPRT(LOG_INFO, "%s %d: Enter.\n", __func__, __LINE__);
	sdio_claim_host(func);
	ret = sdio_disable_func(func);
	if (ret < 0)
		LOGPRT(LOG_ERR, "%s: sdio_disable_func failed.\n", __func__);

	ret = sdio_release_irq(func);
	if (ret < 0)
		LOGPRT(LOG_ERR, "%s: sdio_release_irq failed.\n", __func__);

	sdio_release_host(func);

	modem->func = NULL;

	LOGPRT(LOG_INFO, "%s %d: Leave.\n", __func__, __LINE__);
}

static void modem_sdio_remove(struct sdio_func *func)
{
	LOGPRT(LOG_NOTICE, "%s %d: Enter.\n", __func__, __LINE__);
	LOGPRT(LOG_INFO, "modem_sdio_remove from %ps\n",
	       __builtin_return_address(0));
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	c2k_sdio_uninstall_eirq();
#endif
	LOGPRT(LOG_NOTICE, "%s %d: Leave.\n", __func__, __LINE__);
}

#ifdef CONFIG_EVDO_DT_VIA_SUPPORT
#define SDIO_VENDOR_ID_CBP		0x0296
#define SDIO_DEVICE_ID_CBP		0x5347
#else
#define SDIO_VENDOR_ID_CBP		0x037A	/*0x0296 */
#define SDIO_DEVICE_ID_CBP		0xC200	/*0x5347 */
#endif

static const struct sdio_device_id modem_sdio_ids[] = {
	{SDIO_DEVICE(SDIO_VENDOR_ID_CBP, SDIO_DEVICE_ID_CBP)},	/*VIA-Telecom CBP */
	{}			/*Terminating entry */
};

MODULE_DEVICE_TABLE(sdio, modem_sdio_ids);

static int c2k_sdio_suspend(struct device *dev)
{
	struct sdio_func *func = dev_to_sdio_func(dev);
	int ret;

	if (func) {
		LOGPRT(LOG_INFO, "c2k_sdio_suspend\n");
		ret = sdio_set_host_pm_flags(func, MMC_PM_KEEP_POWER);
	}
	return 0;
}

static int c2k_sdio_resume(struct device *dev)
{
	return 0;
}

static const struct dev_pm_ops c2k_sdio_pm_ops = {
	.suspend = c2k_sdio_suspend,
	.resume = c2k_sdio_resume,
};


static struct sdio_driver modem_sdio_driver = {
	.probe = modem_sdio_probe,
	.remove = modem_sdio_remove,
	.name = "modem_sdio",
	.id_table = modem_sdio_ids,
	.drv = {
		.owner = THIS_MODULE,
		.pm = &c2k_sdio_pm_ops,
	}
};

#if ENABLE_CCMNI

int sdio_modem_ccmni_send_pkt(int md_id, int ccmni_idx, void *data, int is_ack)
{
	struct sdio_modem *modem = NULL;
	struct sdio_modem_port *ccmni_port = NULL;
	struct sk_buff *skb = NULL;
	struct sdio_msg_head *msg_head = NULL;
	unsigned long flags;
	int ret = 0;
	unsigned int data_len;
	unsigned int todo;
	int chars_in_fifo = 0;
	struct ccmni_ch *channel = ccmni_ops.get_ch(md_id, ccmni_idx);
	int tx_ch = is_ack ? channel->dl_ack : channel->tx;

	LOGPRT(LOG_DEBUG, "%s: enter...\n", __func__);
	if (md_id != SDIO_MD_ID) {
		LOGPRT(LOG_ERR,
		       "%s: sdio_modem_send_pkt failed: wrong md_id.\n",
		       __func__);
		return CCMNI_ERR_TX_INVAL;
	}

	ccmni_port = sdio_modem_tty_port_get(CCMNI_CH_ID - 1);
	if (!ccmni_port) {
		LOGPRT(LOG_ERR,
		       "%s: sdio_modem_send_pkt failed: ccmni port is NULL.\n",
		       __func__);
		goto md_not_ready_err_exit;
	}
	modem = ccmni_port->modem;
	if (modem->status == 0) {
		LOGPRT(LOG_ERR,
		       "%s: sdio_modem_send_pkt failed: modem not ready.\n",
		       __func__);
		goto md_not_ready_err_exit;
	}
	LOGPRT(LOG_DEBUG, "%s: get port done...\n", __func__);

	skb = (struct sk_buff *)data;
	todo = skb->len;
	msg_head =
	    (struct sdio_msg_head *)skb_push(skb, sizeof(struct sdio_msg_head));
	msg_head->start_flag = 0xFE;
	msg_head->chanInfo =
	    (0x0F & (ccmni_port->index + 1)) + (0xF0 & (tx_ch << 4));
	msg_head->tranHi = 0x0F & (todo >> 8);
	msg_head->tranLow = 0xFF & todo;
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	msg_head->hw_head.len_hi =
	    ((todo + sizeof(struct sdio_msg_head)) & 0xFF00) >> 8;
	msg_head->hw_head.len_low =
	    (todo + sizeof(struct sdio_msg_head)) & 0xFF;
#endif
	data_len = skb->len;	/*as skb->len already included sdio_msg_head after skb_push */

	data = (void *)(skb->data);

	LOGPRT(LOG_DEBUG,
	       "%s %d msg_head(0x%x, 0x%x, 0x%x, 0x%x), data_len(%d), todo(%d)\n",
	       __func__, __LINE__, msg_head->start_flag, msg_head->chanInfo,
	       msg_head->tranHi, msg_head->tranLow, data_len, todo);

	LOGPRT(LOG_DEBUG, "%s %d msg_head addr(%p) data addr(%p)\n", __func__,
	       __LINE__, (int *)msg_head, (int *)data);

	if (ccmni_port->inception) {
		skb_pull(skb, sizeof(struct sdio_msg_head));
		LOGPRT(LOG_ERR, "%s %d check why come here\n", __func__,
		       __LINE__);
		return CCMNI_ERR_TX_BUSY;
	}

	chars_in_fifo = kfifo_len(&ccmni_port->transmit_fifo);
	if (data_len > (FIFO_SIZE - chars_in_fifo)) {
		LOGPRT(LOG_DEBUG, "%s %d FIFO size is not enough!(%d)\n",
		       __func__, __LINE__, chars_in_fifo);
		skb_pull(skb, sizeof(struct sdio_msg_head));
		spin_lock_irqsave(&ccmni_port->tx_state_lock, flags);

		/* stop tx queue */
		LOGPRT(LOG_INFO, "ccmni port %d is stopped.\n",
			       ccmni_port->index);
		ccmni_ops.md_state_callback(md_id, tx_ch, TX_FULL, 0);
		ccmni_port->tx_state = CCMNI_TX_STOP;

		spin_unlock_irqrestore(&ccmni_port->tx_state_lock, flags);
		return CCMNI_ERR_TX_BUSY;
	}

	spin_lock_irqsave(&modem->status_lock, flags);
	if (modem->status != MD_OFF) {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		ret =
		    kfifo_in_locked(&ccmni_port->transmit_fifo, data, data_len,
				    &ccmni_port->write_lock);
		if (ret != data_len) {
			LOGPRT(LOG_ERR,
			       "%s %d not all data pushed into fifo(%d/%d)!\n",
			       __func__, __LINE__, ret, data_len);
		}
		/*free skb if we return tx OK */
		dev_kfree_skb_any(skb);
		LOGPRT(LOG_DEBUG,
		       "%s %d push %d bytes to fifo!(0x%x, 0x%x, 0x%x, 0x%x)\n",
		       __func__, __LINE__, ret, *(char *)data,
		       *((char *)data + 1), *((char *)data + 2),
		       *((char *)data + 3));
		queue_work(ccmni_port->write_q, &ccmni_port->write_ccmni_work);
	} else {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		LOGPRT(LOG_NOTICE, "%s %d: port%d is removed!\n", __func__,
		       __LINE__, ccmni_port->index);
		goto md_not_ready_err_exit;
	}

	LOGPRT(LOG_DEBUG, "%s %d: port%d write done\n", __func__, __LINE__,
	       ccmni_port->index);

	return data_len;

 md_not_ready_err_exit:
	return CCMNI_ERR_MD_NO_READY;
}

#endif

#if ENABLE_CHAR_DEV
static unsigned int char_dev_major;
static void *dev_class;

static int dev_char_open(struct inode *inode, struct file *file)
{
	/*int major = imajor(inode); */
	int minor = iminor(inode);
	struct sdio_modem_port *port;

	port = sdio_modem_tty_port_get(minor);
	if (atomic_read(&port->usage_cnt))
		return -EBUSY;
	LOGPRT(LOG_INFO, "port %d open with flag %X by %s\n", port->index,
	       file->f_flags, current->comm);
	kfifo_reset(&port->transmit_fifo);
	LOGPRT(LOG_INFO, "port %d kfifo len %d\n", port->index,
	       kfifo_len(&port->transmit_fifo));
	atomic_inc(&port->usage_cnt);
	file->private_data = port;
	nonseekable_open(inode, file);
	return 0;
}

static int dev_char_close(struct inode *inode, struct file *file)
{
	struct sdio_modem_port *port =
	    (struct sdio_modem_port *)file->private_data;
	/*unsigned long flags; */
	struct sdio_buf_in_packet *packet = NULL;

	LOGPRT(LOG_INFO, "port %d close by %s\n", port->index, current->comm);
	atomic_dec(&port->usage_cnt);

	mutex_lock(&port->sdio_buf_in_mutex);
	/*clear pending data */
	while (!list_empty(&port->sdio_buf_in_list)) {
		packet =
		    list_first_entry(&port->sdio_buf_in_list,
				     struct sdio_buf_in_packet, node);
		list_del(&packet->node);
		port->sdio_buf_in_num--;
		port->sdio_buf_in_size -= packet->o_size;
		kfree(packet->buffer);
		kfree(packet);
	}
	mutex_unlock(&port->sdio_buf_in_mutex);
	return 0;
}

static long dev_char_ioctl(struct file *file, unsigned int cmd,
			   unsigned long arg)
{
	struct sdio_modem_port *port =
	    (struct sdio_modem_port *)file->private_data;

	LOGPRT(LOG_INFO, "ioctl is not supported on port %d by %s, %x\n",
	       port->index, current->comm, cmd);
	return 0;
}

#ifdef CONFIG_COMPAT
static long dev_char_compat_ioctl(struct file *filp, unsigned int cmd,
				  unsigned long arg)
{
	return filp->f_op->unlocked_ioctl(filp, cmd,
					  (unsigned long)compat_ptr(arg));
}
#endif

static ssize_t dev_char_read(struct file *file, char *buf, size_t count,
			     loff_t *ppos)
{
	struct sdio_buf_in_packet *packet = NULL;
	int read_len = 0, read_curr = 0, full_pkt_done = 0, user_available =
	    count, ret = 0;
	struct sdio_modem_port *port =
	    (struct sdio_modem_port *)file->private_data;

	if (list_empty(&port->sdio_buf_in_list)) {
		if (!(file->f_flags & O_NONBLOCK)) {
			ret =
			    wait_event_interruptible(port->rx_wq,
						     !list_empty
						     (&port->sdio_buf_in_list));
			if (ret == -ERESTARTSYS) {
				ret = -EINTR;
				goto exit;
			}
		} else {
			ret = -EAGAIN;
			goto exit;
		}
	}
	/*LOGPRT(LOG_INFO, "reading on port%d count=%zu\n", port->index, count); */

 get_one:
	full_pkt_done = 0;
	mutex_lock(&port->sdio_buf_in_mutex);
	/*make sure list is not empty before dequeueing packet */
	if (list_empty(&port->sdio_buf_in_list)) {
		LOGPRT(LOG_ERR, "unexpected exit:port%d read_len=%d\n",
		       port->index, read_len);
		mutex_unlock(&port->sdio_buf_in_mutex);
		goto exit;
	}
	packet =
	    list_first_entry(&port->sdio_buf_in_list, struct sdio_buf_in_packet,
			     node);
	if (user_available >= packet->size) {
		full_pkt_done = 1;
		read_curr = packet->size;
		user_available -= packet->size;
	} else {
		read_curr = user_available;
		user_available = 0;
	}

	if (!packet->buffer || (packet->offset + read_curr) > packet->o_size) {
		LOGPRT(LOG_INFO,
		       "reading on port%d %p o_size=%d size=%d offset=%d read_curr=%d\n",
		       port->index, packet, packet->o_size, packet->size,
		       packet->offset, read_curr);
		BUG_ON(1);
	}
	if (copy_to_user
	    (buf + read_len, packet->buffer + packet->offset, read_curr)) {
		LOGPRT(LOG_ERR,
		       "reading on port%d %p copy_to_user fail: o_size=%d size=%d offset=%d read_curr=%d\n",
		       port->index, packet, packet->o_size, packet->size,
		       packet->offset, read_curr);
		ret = -EFAULT;
		mutex_unlock(&port->sdio_buf_in_mutex);
		goto exit;
	}
	packet->size -= read_curr;
	packet->offset += read_curr;
	read_len += read_curr;

	if (full_pkt_done) {
		list_del(&packet->node);
		port->sdio_buf_in_num--;
		port->sdio_buf_in_size -= packet->o_size;
		kfree(packet->buffer);
		kfree(packet);
		/*LOGPRT(LOG_INFO, "reading on port%d free %p\n", port->index, packet); */
		if (!list_empty(&port->sdio_buf_in_list) && user_available) {
			LOGPRT(LOG_DEBUG, "read on port%d more, %d/%d/%zu\n",
			       port->index, ret, read_len, count);
			mutex_unlock(&port->sdio_buf_in_mutex);
			goto get_one;
		}
	}
	mutex_unlock(&port->sdio_buf_in_mutex);

 exit:
	if (port->index == EXCP_MSG_CH_ID - 1
	    || port->index == SDIO_AT_CHANNEL_NUM - 1
	    || port->index == SDIO_AT2_CHANNEL_NUM - 1
	    || port->index == EXCP_CTRL_CH_ID - 1
	    || port->index == AGPS_CH_ID - 1
	    || port->index == SDIO_AT3_CHANNEL_NUM - 1)
		LOGPRT(LOG_INFO, "read on port%d done, %d/%d/%zu\n",
		       port->index, ret, read_len, count);
	else
		LOGPRT(LOG_DEBUG, "read on port%d done, %d/%d/%zu\n",
		       port->index, ret, read_len, count);
	return ret ? ret : read_len;
}

static ssize_t dev_char_write(struct file *file, const char __user *buf,
			      size_t count, loff_t *ppos)
{
	struct sdio_modem_port *port =
	    (struct sdio_modem_port *)file->private_data;
	struct sdio_modem *modem = c2k_modem;
	unsigned long flags, fflags;
	int ret = 0;
	unsigned int copied = 0;
	int chars_in_fifo = 0;

	ret = check_port(port);
	if (ret < 0) {
		LOGPRT(LOG_ERR, "%s %d check_port failed\n", __func__,
		       __LINE__);
		return ret;
	}
	if ((port->index != (FLS_CH_ID - 1)) && c2k_modem_not_ready()) {
		LOGPRT(LOG_ERR, "%s %d: modem is reset now.(%d)\n", __func__,
		       __LINE__, port->index);
		return -ENODEV;
	}

	if ((port->index == (EXCP_CTRL_CH_ID - 1))
	    || (port->index == (EXCP_MSG_CH_ID - 1))) {
		LOGPRT(LOG_INFO, "write on port%d\n", port->index);
	}

	if (port->inception)
		return -EBUSY;

	if (count > FIFO_SIZE) {
		LOGPRT(LOG_ERR, "%s %d FIFO size is not enough!\n", __func__,
		       __LINE__);
		return -1;
	}

	spin_lock_irqsave(&modem->status_lock, flags);
	if (modem->status != MD_OFF) {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		chars_in_fifo = kfifo_len(&port->transmit_fifo);
		if ((port->index != (FLS_CH_ID - 1))
		    && (count > (ONE_PACKET_MAX_SIZE - chars_in_fifo))) {
			pr_debug
			    ("[C2K]port%d too many(%d) data pending...flow ctrl(%d)\n",
			     port->index, chars_in_fifo,
			     atomic_read(&port->sflow_ctrl_state));
			return -EBUSY;
		}
		/*ret = kfifo_in_locked(&port->transmit_fifo, buf, count, &port->write_lock); */
		spin_lock_irqsave(&port->write_lock, fflags);
		ret =
		    kfifo_from_user(&port->transmit_fifo, buf, count, &copied);
		ret = ret == 0 ? copied : ret;
		spin_unlock_irqrestore(&port->write_lock, fflags);
		queue_work(port->write_q, &port->write_work);
	} else {
		spin_unlock_irqrestore(&modem->status_lock, flags);
		LOGPRT(LOG_ERR, "%s %d: port%d is removed!\n", __func__,
		       __LINE__, port->index);
	}

	if ((port->index == (EXCP_CTRL_CH_ID - 1))
	    || (port->index == (EXCP_MSG_CH_ID - 1))) {
		LOGPRT(LOG_INFO, "write on port%d for %d/%d/%zu\n", port->index,
		       ret, copied, count);
	}
	LOGPRT(LOG_DEBUG, "write on port%d for %d/%d/%zu\n", port->index, ret,
	       copied, count);
	return ret;
}

static unsigned int dev_char_poll(struct file *fp,
				  struct poll_table_struct *poll)
{
	struct sdio_modem_port *port =
	    (struct sdio_modem_port *)fp->private_data;
	unsigned int mask = 0;

	/*printk("[C2K] poll on %d\n", port->index); */
	poll_wait(fp, &port->rx_wq, poll);
	/*TODO: lack of poll wait for Tx */
	if (!list_empty(&port->sdio_buf_in_list))
		mask |= POLLIN | POLLRDNORM;
	if (check_port(port) < 0)
		mask |= POLLERR;

	/*pr_debug("[C2K] poll done on %d, mask=%x\n", port->index, mask); */

	return mask;
}

static const struct file_operations char_dev_fops = {
	.owner = THIS_MODULE,
	.open = &dev_char_open,
	.read = &dev_char_read,
	.write = &dev_char_write,
	.release = &dev_char_close,
	.unlocked_ioctl = &dev_char_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = &dev_char_compat_ioctl,
#endif
	.poll = &dev_char_poll,
};
#endif
int modem_sdio_init(struct cbp_platform_data *pdata)
{
	int ret;
#if ENABLE_CHAR_DEV
	dev_t char_dev_num;
	/*struct cdev *dev; */
#else
	struct tty_driver *tty_drv;
#endif
	struct sdio_modem *modem = NULL;
	struct sdio_modem_port *port = NULL;
	int index = 0;
	unsigned long flags;

	pr_debug("%s %d: Enter.\n", __func__, __LINE__);

#if ENABLE_CHAR_DEV
	ret =
	    alloc_chrdev_region(&char_dev_num, 0, SDIO_TTY_NR, "c2k_ccci_node");
	if (ret)
		return -ENOMEM;

	char_dev_major = MAJOR(char_dev_num);
	dev_class = class_create(THIS_MODULE, "c2k_ccci_node");
#else
	modem_sdio_tty_driver = tty_drv = alloc_tty_driver(SDIO_TTY_NR);
	if (!tty_drv)
		return -ENOMEM;

	tty_drv->owner = THIS_MODULE;
	tty_drv->driver_name = "modem_sdio";
	tty_drv->name = "ttySDIO";
	tty_drv->major = 0;	/*dynamically allocated */
	tty_drv->minor_start = 0;
	tty_drv->type = TTY_DRIVER_TYPE_SERIAL;
	tty_drv->subtype = SERIAL_TYPE_NORMAL;
	tty_drv->flags = TTY_DRIVER_REAL_RAW | TTY_DRIVER_DYNAMIC_DEV;
	tty_drv->init_termios = tty_std_termios;
	tty_drv->init_termios.c_cflag = B9600 | CS8 | CREAD | HUPCL | CLOCAL;
	tty_drv->init_termios.c_cflag &= ~(CSIZE | CSTOPB | PARENB | PARODD);
	tty_drv->init_termios.c_cflag |= CREAD | CLOCAL | CS8;
	tty_drv->init_termios.c_cflag &= ~(CRTSCTS);
	tty_drv->init_termios.c_lflag &=
	    ~(ICANON | ECHO | ECHOE | ECHOK | ECHONL | ISIG);
	tty_drv->init_termios.c_iflag &=
	    ~(INPCK | IGNPAR | PARMRK | ISTRIP | IXANY | ICRNL);
	tty_drv->init_termios.c_iflag &= ~(IXON | IXOFF);
	tty_drv->init_termios.c_oflag &= ~(OPOST | OCRNL);
	tty_drv->init_termios.c_ispeed = 9600;
	tty_drv->init_termios.c_ospeed = 9600;
	tty_set_operations(tty_drv, &modem_tty_ops);

	ret = tty_register_driver(tty_drv);
	if (ret) {
		LOGPRT(LOG_ERR, "%s: tty_register_driver failed.\n", __func__);
		goto exit_reg_driver;
	}
#endif

	modem = kzalloc(sizeof(struct sdio_modem), GFP_KERNEL);
	if (!modem) {
		LOGPRT(LOG_ERR, "%s %d kzalloc sdio_modem failed.\n", __func__,
		       __LINE__);
		ret = -ENOMEM;
		goto err_kzalloc_sdio_modem;
	}

	modem->ctrl_port =
	    kzalloc(sizeof(struct sdio_modem_ctrl_port), GFP_KERNEL);
	if (!modem->ctrl_port) {
		LOGPRT(LOG_ERR, "%s %d kzalloc ctrl_port failed\n", __func__,
		       __LINE__);
		ret = -ENOMEM;
		goto err_kzalloc_ctrl_port;
	}

	modem->msg = kzalloc(sizeof(struct sdio_msg), GFP_KERNEL);
	if (!modem->msg) {
		LOGPRT(LOG_ERR, "%s %d kzalloc sdio_msg failed\n", __func__,
		       __LINE__);
		ret = -ENOMEM;
		goto err_kzalloc_sdio_msg;
	}

	modem->trans_buffer = kzalloc(TRANSMIT_BUFFER_SIZE, GFP_KERNEL);
	if (!modem->trans_buffer) {
		LOGPRT(LOG_ERR, "%s %d kzalloc trans_buffer failed\n", __func__,
		       __LINE__);
		ret = -ENOMEM;
		goto err_kzalloc_trans_buffer;
	}

	sema_init(&modem->sem, 1);
	spin_lock_init(&modem->status_lock);

	LOGPRT(LOG_INFO, "%s %d set md off!\n", __func__, __LINE__);
	spin_lock_irqsave(&modem->status_lock, flags);
	modem->status = MD_OFF;
	spin_unlock_irqrestore(&modem->status_lock, flags);
	modem->cbp_data = pdata;
	pdata->modem = modem;
	modem->ctrl_port->chan_state = CHAN_OFF;
	init_waitqueue_head(&modem->ctrl_port->sflow_ctrl_wait_q);
	atomic_set(&modem->ctrl_port->sflow_ctrl_state, SFLOW_CTRL_DISABLE);
	INIT_WORK(&modem->dtr_work, modem_dtr_send);
	INIT_WORK(&modem->dcd_query_work, modem_dcd_query);
#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	INIT_WORK(&modem->poll_hb_work, modem_heart_beat_poll_work);
	INIT_WORK(&modem->force_assert_work, modem_force_assert_work);
	INIT_WORK(&modem->smem_read_done_work, modem_smem_read_done_work);
#endif

#if ENABLE_CCMNI
	ccmni_ops.init(SDIO_MD_ID, &sdio_ccmni_ops);

#endif

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	modem->as_packet =
	    kzalloc(sizeof(struct sdio_assemble_packet), GFP_KERNEL);
	if (!modem->as_packet) {
		LOGPRT(LOG_ERR, "%s %d: kzalloc as_packet error\n", __func__,
		       __LINE__);
		ret = -ENOMEM;
		/*mutex_unlock(&port->sdio_assemble_mutex); */
		goto err_kzalloc_trans_buffer;	/*may need modify */
	}
	modem->as_packet->buffer = kzalloc(SDIO_ASSEMBLE_MAX, GFP_KERNEL);
	if (!modem->as_packet->buffer) {
		LOGPRT(LOG_ERR, "%s %d: kzalloc as_packet buffer error\n",
		       __func__, __LINE__);
		ret = -ENOMEM;
		kfree(modem->as_packet);
		/*mutex_unlock(&port->sdio_assemble_mutex); */
		goto err_kzalloc_trans_buffer;	/*may need modify */
	}
	modem->as_packet->size = 0;

	init_waitqueue_head(&modem->wait_tx_done_q);
	modem->fw_own = 1;
	atomic_set(&modem->as_packet->occupied, 0);
	atomic_set(&modem->tx_fifo_cnt, TX_FIFO_SZ);
	INIT_WORK(&modem->loopback_work, loopback_to_c2k);
#endif

	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = kzalloc(sizeof(struct sdio_modem_port), GFP_KERNEL);
		if (!port) {
			LOGPRT(LOG_ERR,
			       "%s %d kzalloc sdio_modem_port %d failed.\n",
			       __func__, __LINE__, index);
			ret = -ENOMEM;
			goto err_kazlloc_sdio_modem_port;
		}
		/*printk("[MODEM SDIO] %s index[%d] 0x%x\n", __func__, index, port); */
#if !ENABLE_CHAR_DEV
		tty_port_init(&port->port);
		port->port.ops = &sdio_modem_port_ops;
#endif
		port->modem = modem;
		modem->port[index] = port;
		spin_lock_init(&port->inception_lock);
		port->inception = false;
	}

	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = modem->port[index];
		ret = sdio_modem_port_init(port, index);
		if (ret) {
			LOGPRT(LOG_ERR, "%s %d sdio add port failed.\n",
			       __func__, __LINE__);
			goto err_sdio_modem_port_init;
		} else {
#if ENABLE_CHAR_DEV
			struct device *dev;

			cdev_init(&port->char_dev, &char_dev_fops);
			port->char_dev.owner = THIS_MODULE;
			ret =
			    cdev_add(&port->char_dev,
				     MKDEV(char_dev_major, port->index), 1);
			dev =
			    device_create(dev_class, NULL,
					  MKDEV(char_dev_major, port->index),
					  NULL, "%s%d", "ttySDIO", port->index);
			if (IS_ERR(dev)) {
				ret = PTR_ERR(dev);
				LOGPRT(LOG_ERR, "%s %d char create failed\n",
				       __func__, __LINE__);
				goto err_sdio_modem_port_init;
			}
#else
			struct device *dev;

			dev =
			    tty_port_register_device(&port->port,
						     modem_sdio_tty_driver,
						     port->index, NULL);

			if (IS_ERR(dev)) {
				ret = PTR_ERR(dev);
				LOGPRT(LOG_ERR, "%s %d tty register failed\n",
				       __func__, __LINE__);
				goto err_sdio_modem_port_init;
			}
#endif
		}
	}

	modem->c2k_kobj = c2k_kobject_add("modem_sdio");
	if (!modem->c2k_kobj) {
		ret = -ENOMEM;
		goto err_create_kobj;
	}
	ret = sysfs_create_group(modem->c2k_kobj, &g_modem_attr_group);

	c2k_modem = modem;

	ret = sdio_register_driver(&modem_sdio_driver);
	if (ret) {
		LOGPRT(LOG_ERR, "%s: sdio_register_driver failed.\n", __func__);
		goto exit_tty;
	}
#ifdef TX_DONE_TRACE
	setup_timer(&timer_wait_tx_done, wait_tx_done_timer,
		    (unsigned long)"C2K_TX");
#endif

#ifndef CONFIG_EVDO_DT_VIA_SUPPORT
	setup_timer(&modem->heart_beat_timer, c2k_heart_beat_timer,
		    (unsigned long)modem);
	setup_timer(&modem->poll_timer, c2k_poll_status_timer,
		    (unsigned long)modem);
	setup_timer(&modem->force_assert_timer, c2k_force_assert_timer,
		    (unsigned long)modem);
#endif
	LOGPRT(LOG_INFO, " %s: sdio driver is initialized!\n", __func__);
	pr_debug("%s %d: Exit.\n", __func__, __LINE__);
	return ret;

 err_create_kobj:
 err_sdio_modem_port_init:
	modem_port_remove(modem);
 err_kazlloc_sdio_modem_port:
	for (index = 0; index < SDIO_TTY_NR; index++) {
		port = modem->port[index];
		kfree(port);
	}

 err_kzalloc_trans_buffer:
	kfree(modem->msg);
 err_kzalloc_sdio_msg:
	kfree(modem->ctrl_port);
 err_kzalloc_ctrl_port:
	kfree(modem);
 err_kzalloc_sdio_modem:
	return ret;

 exit_tty:
#if !ENABLE_CHAR_DEV
	tty_unregister_driver(tty_drv);
#endif
/*exit_reg_driver:*/
	LOGPRT(LOG_ERR, "%s: returning with error %d\n", __func__, ret);
#if !ENABLE_CHAR_DEV
	put_tty_driver(tty_drv);
#endif
	return ret;
}

void modem_sdio_exit(void)
{
	sdio_unregister_driver(&modem_sdio_driver);
	tty_unregister_driver(modem_sdio_tty_driver);
	put_tty_driver(modem_sdio_tty_driver);
}
