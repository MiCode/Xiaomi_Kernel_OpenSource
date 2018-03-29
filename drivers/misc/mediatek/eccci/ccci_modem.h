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

#ifndef __CCCI_MODEM_H__
#define __CCCI_MODEM_H__

#include <linux/wait.h>
#include <linux/skbuff.h>
#include <linux/timer.h>
#include <linux/types.h>
#include <linux/ktime.h>
#include <linux/netdevice.h>
#include <linux/platform_device.h>
#include <linux/wakelock.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <mt-plat/mt_ccci_common.h>
#include "ccci_core.h"
#include "port_proxy.h"

#define BOOT_TIMER_HS1 10
#define BOOT_TIMER_HS2 10
struct ccci_modem;
struct ccci_md_attribute {
	struct attribute attr;
	struct ccci_modem *modem;
	 ssize_t (*show)(struct ccci_modem *md, char *buf);
	 ssize_t (*store)(struct ccci_modem *md, const char *buf, size_t count);
};

#define CCCI_MD_ATTR(_modem, _name, _mode, _show, _store)	\
static struct ccci_md_attribute ccci_md_attr_##_name = {	\
	.attr = {.name = __stringify(_name), .mode = _mode },	\
	.modem = _modem,					\
	.show = _show,						\
	.store = _store,					\
}

struct ccci_modem_ops {
	/* must-have */
	int (*init)(struct ccci_modem *md);
	int (*start)(struct ccci_modem *md);
	int (*pre_stop)(struct ccci_modem *md, unsigned int stop_type);
	int (*stop)(struct ccci_modem *md, unsigned int stop_type);
	int (*soft_start)(struct ccci_modem *md, unsigned int mode);
	int (*soft_stop)(struct ccci_modem *md, unsigned int mode);
	int (*send_skb)(struct ccci_modem *md, int txqno, struct sk_buff *skb, int skb_from_pool, int blocking);
	int (*give_more)(struct ccci_modem *md, unsigned char rxqno);
	int (*write_room)(struct ccci_modem *md, unsigned char txqno);
	int (*start_queue)(struct ccci_modem *md, unsigned char qno, DIRECTION dir);
	int (*stop_queue)(struct ccci_modem *md, unsigned char qno, DIRECTION dir);
	int (*napi_poll)(struct ccci_modem *md, unsigned char rxqno, struct napi_struct *napi, int weight);
	int (*send_runtime_data)(struct ccci_modem *md, unsigned int tx_ch, unsigned int txqno, int skb_from_pool);
	int (*broadcast_state)(struct ccci_modem *md, MD_STATE state);
	int (*force_assert)(struct ccci_modem *md, MD_COMM_TYPE type);
	int (*dump_info)(struct ccci_modem *md, MODEM_DUMP_FLAG flag, void *buff, int length);
	int (*low_power_notify)(struct ccci_modem *md, LOW_POEWR_NOTIFY_TYPE type, int level);
	int (*ee_callback)(struct ccci_modem *md, MODEM_EE_FLAG flag);
	int (*send_ccb_tx_notify)(struct ccci_modem *md, int core_id);
	int (*reset_pccif)(struct ccci_modem *md);
	int (*is_epon_set)(struct ccci_modem *md);
	int (*set_dsp_mpu)(struct ccci_modem *md, int is_loaded);
};

struct ccci_modem {
	unsigned char index;
	unsigned char *private_data;
	short seq_nums[2][CCCI_MAX_CH_NUM];
	unsigned int capability;
	unsigned int napi_queue_mask;

	volatile MD_STATE md_state;	/* check comments below, put it here for cache benefit */
	struct ccci_modem_ops *ops;
	atomic_t wakeup_src;
	/* refer to port_proxy obj, no need used in sub class,
	* if realy want to use, please define delegant api in ccci_modem class
	*/
	void *port_proxy_obj;
	void *mdee_obj;
	struct list_head entry;
	char post_fix[IMG_POSTFIX_LEN];
	struct kobject kobj;
	struct ccci_mem_layout mem_layout;
	struct ccci_smem_layout smem_layout;
	struct ccci_image_info img_info[IMG_NUM];
	unsigned int sbp_code;
	unsigned int mdlg_mode;
	unsigned int md_dbg_dump_flag;
	MD_BOOT_MODE md_boot_mode;
	struct platform_device *plat_dev;
	/*
	 * the following members are readonly for CCCI core. they are maintained by modem and
	 * port_kernel.c.
	 * port_kernel.c should not be considered as part of CCCI core, we just move common part
	 * of modem message handling into this file. current modem all follows the same message
	 * protocol during bootup and exception. if future modem abandoned this protocl, we can
	 * simply replace function set of kernel port to support it.
	 */
	unsigned int is_in_ee_dump;
	unsigned int is_force_asserted;
	phys_addr_t invalid_remap_base;
	volatile struct ccci_modem_cfg config;
	unsigned short heart_beat_counter;
#if PACKET_HISTORY_DEPTH
	struct ccci_log tx_history[MAX_TXQ_NUM][PACKET_HISTORY_DEPTH];
	struct ccci_log rx_history[MAX_RXQ_NUM][PACKET_HISTORY_DEPTH];
	int tx_history_ptr[MAX_TXQ_NUM];
	int rx_history_ptr[MAX_RXQ_NUM];
#endif
	unsigned long logic_ch_pkt_cnt[CCCI_MAX_CH_NUM];
	unsigned long logic_ch_pkt_pre_cnt[CCCI_MAX_CH_NUM];

	unsigned long long latest_isr_time;
	unsigned long long latest_q_rx_isr_time[MAX_RXQ_NUM];
	unsigned long long latest_q_rx_time[MAX_RXQ_NUM];
#ifdef CCCI_SKB_TRACE
	unsigned long long netif_rx_profile[8];
#endif
	int runtime_version;
	int data_usb_bypass;
#ifdef FEATURE_SCP_CCCI_SUPPORT
	struct work_struct scp_md_state_sync_work;
#endif
	struct ccci_fsm_ctl fsm;
};
/****************************************************************************************************************/
/* API Region called by sub-modem class, reuseable API */
/****************************************************************************************************************/
struct ccci_modem *ccci_md_alloc(int private_size);
int ccci_md_register(struct ccci_modem *modem);
int ccci_md_force_assert(struct ccci_modem *md, MD_FORCE_ASSERT_TYPE type, char *param, int len);
int ccci_md_send_msg_to_user(struct ccci_modem *md, CCCI_CH ch, CCCI_MD_MSG msg, u32 resv);
int ccci_send_msg_to_md(struct ccci_modem *md, CCCI_CH ch, u32 msg, u32 resv, int blocking);

void ccci_md_status_notice(struct ccci_modem *md, DIRECTION dir, int filter_ch_no,
								int filter_queue_idx, MD_STATE state);
void ccci_md_dump_port_status(struct ccci_modem *md);

static inline void ccci_reset_seq_num(struct ccci_modem *md)
{
	/* it's redundant to use 2 arrays, but this makes sequence checking easy */
	memset(md->seq_nums[OUT], 0, sizeof(md->seq_nums[OUT]));
	memset(md->seq_nums[IN], -1, sizeof(md->seq_nums[IN]));
}

/*
* as one channel can only use one hardware queue,
* so it's safe we call this function in hardware queue's lock protection
*/
static inline void ccci_md_inc_tx_seq_num(struct ccci_modem *md, struct ccci_header *ccci_h)
{
#ifdef FEATURE_SEQ_CHECK_EN
	if (ccci_h->channel >= ARRAY_SIZE(md->seq_nums[OUT]) || ccci_h->channel < 0) {
		CCCI_NORMAL_LOG(md->index, CORE, "ignore seq inc on channel %x\n", *(((u32 *) ccci_h) + 2));
		return;		/* for force assert channel, etc. */
	}
	ccci_h->seq_num = md->seq_nums[OUT][ccci_h->channel]++;
	ccci_h->assert_bit = 1;

	/* for rpx channel, can only set assert_bit when md is in single-task phase. */
	/* when md is in multi-task phase, assert bit should be 0, since ipc task are preemptible */
	if ((ccci_h->channel == CCCI_RPC_TX || ccci_h->channel == CCCI_FS_TX) && md->md_state != BOOT_WAITING_FOR_HS2)
		ccci_h->assert_bit = 0;

#endif
}

static inline void ccci_md_check_rx_seq_num(struct ccci_modem *md, struct ccci_header *ccci_h, int qno)
{
#ifdef FEATURE_SEQ_CHECK_EN
	u16 channel, seq_num, assert_bit;
	unsigned int param[3] = {0};

	channel = ccci_h->channel;
	seq_num = ccci_h->seq_num;
	assert_bit = ccci_h->assert_bit;

	if (assert_bit && md->seq_nums[IN][channel] != 0 && ((seq_num - md->seq_nums[IN][channel]) & 0x7FFF) != 1) {
		CCCI_ERROR_LOG(md->index, CORE, "channel %d seq number out-of-order %d->%d\n",
			     channel, seq_num, md->seq_nums[IN][channel]);
		if (md->is_force_asserted == 0) {
			md->ops->dump_info(md, DUMP_FLAG_CLDMA, NULL, qno);
			param[0] = channel;
			param[1] = md->seq_nums[IN][channel];
			param[2] = seq_num;
			ccci_md_force_assert(md, MD_FORCE_ASSERT_BY_MD_SEQ_ERROR, (char *)param, sizeof(param));
		}
	} else {
		md->seq_nums[IN][channel] = seq_num;
	}
#endif
}

static inline void ccci_channel_update_packet_counter(struct ccci_modem *md, struct ccci_header *ccci_h)
{
	if ((ccci_h->channel & 0xFF) < CCCI_MAX_CH_NUM)
		md->logic_ch_pkt_cnt[ccci_h->channel]++;
}

static inline void ccci_channel_dump_packet_counter(struct ccci_modem *md)
{
	CCCI_REPEAT_LOG(md->index, CORE, "traffic(ch): tx:[%d]%ld, [%d]%ld, [%d]%ld rx:[%d]%ld, [%d]%ld, [%d]%ld\n",
		     CCCI_PCM_TX, md->logic_ch_pkt_cnt[CCCI_PCM_TX],
		     CCCI_UART2_TX, md->logic_ch_pkt_cnt[CCCI_UART2_TX],
		     CCCI_FS_TX, md->logic_ch_pkt_cnt[CCCI_FS_TX],
		     CCCI_PCM_RX, md->logic_ch_pkt_cnt[CCCI_PCM_RX],
		     CCCI_UART2_RX, md->logic_ch_pkt_cnt[CCCI_UART2_RX], CCCI_FS_RX, md->logic_ch_pkt_cnt[CCCI_FS_RX]);
	CCCI_REPEAT_LOG(md->index, CORE,
		     "traffic(net): tx: [%d]%ld %ld, [%d]%ld %ld, [%d]%ld %ld, rx:[%d]%ld, [%d]%ld, [%d]%ld\n",
		     CCCI_CCMNI1_TX, md->logic_ch_pkt_pre_cnt[CCCI_CCMNI1_TX], md->logic_ch_pkt_cnt[CCCI_CCMNI1_TX],
		     CCCI_CCMNI2_TX, md->logic_ch_pkt_pre_cnt[CCCI_CCMNI2_TX], md->logic_ch_pkt_cnt[CCCI_CCMNI2_TX],
		     CCCI_CCMNI3_TX, md->logic_ch_pkt_pre_cnt[CCCI_CCMNI3_TX], md->logic_ch_pkt_cnt[CCCI_CCMNI3_TX],
		     CCCI_CCMNI1_RX, md->logic_ch_pkt_cnt[CCCI_CCMNI1_RX],
		     CCCI_CCMNI2_RX, md->logic_ch_pkt_cnt[CCCI_CCMNI2_RX],
		     CCCI_CCMNI3_RX, md->logic_ch_pkt_cnt[CCCI_CCMNI3_RX]);
}

void ccci_md_dump_log_history(struct ccci_modem *md, int dump_multi_rec, int tx_queue_num, int rx_queue_num);
void ccci_md_add_log_history(struct ccci_modem *md, DIRECTION dir, int queue_index,
				struct ccci_header *msg, int is_dropped);
/****************************************************************************************************************/
/* API Region called by port_proxy class */
/****************************************************************************************************************/
struct ccci_modem *ccci_md_get_modem_by_id(int md_id);
struct port_proxy *ccci_md_get_port_proxy(int md_id, int major);
struct ccci_modem *ccci_md_get_another(int md_id);
void ccci_md_set_reload_type(struct ccci_modem *md, int type);
MD_STATE_FOR_USER get_md_state_for_user(struct ccci_modem *md);

int ccci_md_set_boot_data(struct ccci_modem *md, unsigned int data[], int len);
int ccci_md_check_ee_done(struct ccci_modem *md, int timeout);
int ccci_md_store_load_type(struct ccci_modem *md, int type);
int ccci_md_prepare_runtime_data(struct ccci_modem *md, struct sk_buff *skb);
struct ccci_runtime_feature *ccci_md_get_rt_feature_by_id(struct ccci_modem *md, u8 feature_id, u8 ap_query_md);
int ccci_md_parse_rt_feature(struct ccci_modem *md, struct ccci_runtime_feature *rt_feature, void *data, u32 data_len);
int ccci_md_get_ex_type(struct ccci_modem *md);

static inline struct port_proxy *ccci_md_get_port_proxy_by_id(int md_id)
{
	return ccci_md_get_port_proxy(md_id, -1);
}

static inline struct port_proxy *ccci_md_get_port_proxy_by_major(int major)
{
	return ccci_md_get_port_proxy(-1, major);
}
static inline int ccci_md_is_in_debug(struct ccci_modem *md)
{
	return 0;
}

static inline int ccci_md_send_ccb_tx_notify(struct ccci_modem *md, int core_id)
{
	return md->ops->send_ccb_tx_notify(md, core_id);
}

static inline int ccci_md_pre_stop(struct ccci_modem *md, unsigned int stop_type)
{
	return md->ops->pre_stop(md, stop_type);
}

static inline int ccci_md_stop(struct ccci_modem *md, unsigned int stop_type)
{
	return md->ops->stop(md, stop_type);
}
static inline int ccci_md_start(struct ccci_modem *md)
{
	if (md->md_state != GATED)
		return -1;
	return md->ops->start(md);
}
static inline int ccci_md_soft_stop(struct ccci_modem *md, unsigned int sim_mode)
{
	if (md->ops->soft_stop)
		return md->ops->soft_stop(md, sim_mode);
	return -1;
}
static inline int ccci_md_soft_start(struct ccci_modem *md, unsigned int sim_mode)
{
	if (md->ops->soft_start)
		return md->ops->soft_start(md, sim_mode);
	return -1;
}

static inline int ccci_md_send_runtime_data(struct ccci_modem *md, unsigned int tx_ch,
		unsigned int txqno, int skb_from_pool)
{
	return md->ops->send_runtime_data(md, tx_ch, txqno, skb_from_pool);
}

static inline int ccci_md_send_skb(struct ccci_modem *md, int qno,
		struct sk_buff *skb, int skb_from_pool, int is_blocking)
{
	return md->ops->send_skb(md, qno, skb, skb_from_pool, is_blocking);
}

static inline int ccci_md_napi_poll(struct ccci_modem *md, unsigned char rxqno,
					struct napi_struct *napi, int weight)
{
	return md->ops->napi_poll(md, rxqno, napi, weight);
}

static inline int ccci_md_reset_pccif(struct ccci_modem *md)
{
	if (md->ops->reset_pccif)
		return md->ops->reset_pccif(md);
	return -1;
}

static inline int ccci_md_set_dsp_protection(struct ccci_modem *md, int is_loaded)
{
	if (md->ops->set_dsp_mpu)
		return md->ops->set_dsp_mpu(md, is_loaded);
	return -1;
}
static inline int ccci_md_write_room(struct ccci_modem *md, int qno)
{
	return md->ops->write_room(md, qno);
}
static inline int ccci_md_ask_more_request(struct ccci_modem *md, int rxq_no)
{
	return md->ops->give_more(md, rxq_no);
}
static inline void ccci_md_ee_callback(struct ccci_modem *md, MODEM_EE_FLAG flag)
{
	md->ops->ee_callback(md, flag);
}
static inline MD_STATE ccci_md_get_state(struct ccci_modem *md)
{
	return md->md_state;
}

static inline MD_BOOT_MODE ccci_md_get_boot_mode(struct ccci_modem *md)
{
	return md->md_boot_mode;
}

static inline unsigned int ccci_md_get_dbg_dump_flag(struct ccci_modem *md)
{
	return md->md_dbg_dump_flag;
}

static inline int ccci_md_get_state_for_user(struct ccci_modem *md)
{
	return get_md_state_for_user(md);
}

static inline int ccci_md_get_load_type(struct ccci_modem *md)
{
	return md->config.load_type;
}
static inline int ccci_md_get_load_saving_type(struct ccci_modem *md)
{
	return md->config.load_type_saving;
}
static inline char *ccci_md_get_post_fix(struct ccci_modem *md)
{
	return md->post_fix;
}
static inline int ccci_md_get_img_version(struct ccci_modem *md)
{
	return md->img_info[IMG_MD].img_info.version;
}
static inline unsigned long long ccci_md_get_latest_poll_isr_time(struct ccci_modem *md)
{
	return md->latest_q_rx_isr_time[0];
}
static inline unsigned long long ccci_md_get_latest_isr_time(struct ccci_modem *md)
{
	return md->latest_isr_time;
}

static inline unsigned long long ccci_md_get_latest_q0_rx_time(struct ccci_modem *md)
{
	return md->latest_q_rx_time[0];
}
static inline unsigned int ccci_md_get_seq_num(struct ccci_modem *md, DIRECTION dir, CCCI_CH ch)
{
	return md->seq_nums[dir][ch];
}
static inline struct ccci_mem_layout *ccci_md_get_mem(struct ccci_modem *md)
{
	return &md->mem_layout;
}
static inline struct ccci_smem_layout *ccci_md_get_smem(struct ccci_modem *md)
{
	return &md->smem_layout;
}
static inline void ccci_md_set_usb_data_bypass(struct ccci_modem *md, unsigned int usb_data_bypass)
{
	md->data_usb_bypass = usb_data_bypass;
}
static inline void ccci_md_dump_info(struct ccci_modem *md, MODEM_DUMP_FLAG flag, void *buff, int length)
{
	md->ops->dump_info(md, flag, buff, length);
}

static inline int ccci_md_start_queue(struct ccci_modem *md, unsigned char qno, DIRECTION dir)
{
	return md->ops->start_queue(md, qno, dir);
}
static inline void *ccci_md_get_mdee(struct ccci_modem *md)
{
	return md->mdee_obj;
}

static inline int ccci_md_recv_skb(struct ccci_modem *md, struct sk_buff *skb)
{
	return port_proxy_recv_skb(md->port_proxy_obj, skb);
}

static inline int ccci_md_napi_check_and_notice(struct ccci_modem *md, int qno)
{
	if (port_proxy_is_napi_queue(md->port_proxy_obj, qno)) {
		port_proxy_napi_queue_notice(md->port_proxy_obj, qno);
		return 1;
	} else
		return 0;
}

static inline unsigned long long *ccci_md_get_net_rx_profile(struct ccci_modem *md)
{
#ifdef CCCI_SKB_TRACE
		return md->netif_rx_profile;
#else
		return NULL;
#endif
}

#ifdef FEATURE_SCP_CCCI_SUPPORT
static inline int ccci_md_scp_ipi_send(struct ccci_modem *md, int op_id, void *data)
{
	return ccci_scp_ipi_send(md->index, op_id, data);
}
static inline void ccci_md_scp_state_sync(struct ccci_modem *md)
{
	schedule_work(&md->scp_md_state_sync_work);
}
#endif

static inline int ccci_md_broadcast_state(struct ccci_modem *md, MD_STATE state)
{
	int ret = md->ops->broadcast_state(md, state);

#ifdef FEATURE_SCP_CCCI_SUPPORT
	ccci_md_scp_state_sync(md);
#endif
	return ret;
}
/****************************************************************************************************************/
/* API Region called by ccci modem object */
/****************************************************************************************************************/
extern void ccci_md_exception_notify(struct ccci_modem *md, MD_EX_STAGE stage);

#if defined(FEATURE_SYNC_C2K_MEID)
extern unsigned char tc1_read_meid_syncform(unsigned char *meid, int leng);
#endif

#if defined(FEATURE_TC1_CUSTOMER_VAL)
extern int get_md_customer_val(unsigned char *value, unsigned int len);
#endif

#endif	/* __CCCI_MODEM_H__ */
