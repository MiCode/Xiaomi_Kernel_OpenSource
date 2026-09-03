/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SIPA_PRIV_H_
#define _SIPA_PRIV_H_

#include <linux/alarmtimer.h>
#include <linux/skbuff.h>
#include <linux/sipa.h>
#include <linux/interrupt.h>
#include <linux/cdev.h>
#include <linux/regmap.h>
#include <linux/kfifo.h>
#include <linux/spinlock.h>

#define SIPA_DEF_OFFSET	64

#define SIPA_RECV_EVT (SIPA_HAL_INTR_BIT | \
		       SIPA_HAL_TX_FIFO_THRESHOLD_SW | SIPA_HAL_DELAY_TIMER)

#define SIPA_RECV_WARN_EVT (SIPA_HAL_TXFIFO_FULL_INT | \
			    SIPA_HAL_TXFIFO_OVERFLOW | \
			    SIPA_HAL_ERRORCODE_IN_TX_FIFO)

#define SIPA_RECV_NO_EVT SIPA_HAL_ERROR

#define SIPA_MULTI_IRQ_NUM	8

#define SIPA_EB_NUM	2

#define SIPA_WWAN_CONS_TIMER	100

#define IPA_TOSTR1(x) #x
#define IPA_TOSTR(x) IPA_TOSTR1(x)

#define IPA_PASTE1(x, y) x ## y
#define IPA_PASTE(x, y) IPA_PASTE1(x, y)

#define IPA_STI_64BIT(l_val, h_val) ((u64)((l_val) | ((u64)(h_val) << 32)))

#define SIPA_FIFO_REG_SIZE	0x80

#define SIPA_SW_TO_LITTLECORE_THRD 3   //3 times
#define SIPA_SW_TO_MIDDLECORE_THRD 300  //300 nodes in common tx fifo
#define SW_TO_LITTLECORE_NODE_NUM 2000  //recv packets per second

enum sipa_hal_evt_type {
	SIPA_HAL_TX_FIFO_THRESHOLD_SW	= 0x00400000,
	SIPA_HAL_EXIT_FLOW_CTRL		= 0x00100000,
	SIPA_HAL_ENTER_FLOW_CTRL	= 0x00080000,
	SIPA_HAL_TXFIFO_FULL_INT	= 0x00040000,
	SIPA_HAL_TXFIFO_OVERFLOW	= 0x00020000,
	SIPA_HAL_ERRORCODE_IN_TX_FIFO	= 0x00010000,
	SIPA_HAL_INTR_BIT		= 0x00008000,
	SIPA_HAL_THRESHOLD		= 0x00004000,
	SIPA_HAL_DELAY_TIMER		= 0x00002000,
	SIPA_HAL_DROP_PACKT_OCCUR	= 0x00001000,
	SIPA_HAL_ERROR			= 0x0,
};

enum sipa_nic_status_e {
	NIC_OPEN,
	NIC_CLOSE,
};

enum sipa_suspend_stage_e {
	SIPA_THREAD_SUSPEND = BIT(0),
	SIPA_EP_SUSPEND = BIT(1),
	SIPA_BACKUP_SUSPEND = BIT(2),
	SIPA_EB_SUSPEND = BIT(3),
	SIPA_FORCE_SUSPEND = BIT(4),
	SIPA_ACTION_SUSPEND = BIT(5),
};

#define SIPA_SUSPEND_MASK (SIPA_THREAD_SUSPEND | \
			   SIPA_EP_SUSPEND | \
			   SIPA_BACKUP_SUSPEND | \
			   SIPA_EB_SUSPEND | \
			   SIPA_FORCE_SUSPEND | \
			   SIPA_ACTION_SUSPEND)

typedef void (*sipa_hal_notify_cb)(void *priv, enum sipa_hal_evt_type evt,
				   unsigned long data, int irq);
enum sipa_cmn_fifo_index {
	SIPA_FIFO_USB_UL,
	SIPA_FIFO_WIFI_UL,
	SIPA_FIFO_PCIE_UL,
	SIPA_FIFO_WIAP_DL,
	SIPA_FIFO_MAP_IN,
	SIPA_FIFO_USB_DL,
	SIPA_FIFO_WIFI_DL,
	SIPA_FIFO_PCIE_DL,
	SIPA_FIFO_WIAP_UL,
	SIPA_FIFO_MAP0_OUT,
	SIPA_FIFO_MAP1_OUT,
	SIPA_FIFO_MAP2_OUT,
	SIPA_FIFO_MAP3_OUT,
	SIPA_FIFO_MAP4_OUT,
	SIPA_FIFO_MAP5_OUT,
	SIPA_FIFO_MAP6_OUT,
	SIPA_FIFO_MAP7_OUT,

	SIPA_FIFO_MAX,
};

struct sipa_node_desc_tag {
	/*soft need to set*/
	u64 address : 40;
	/*soft need to set*/
	u32 length : 20;
	/*soft need to set*/
	u16 offset : 12;
	/*soft need to set*/
	u8	net_id;
	/*soft need to set*/
	u8	src : 5;
	/*soft need to set*/
	u8	dst : 5;
	u8	rsvd1 : 3;
	u8	fwd_ap : 1;
	u8	n_ip_pkt : 1;
	u8	flow_ctrl : 1;
	u8	err_code : 4;
	/*soft need to set*/
	u8	intr : 1;
	/*soft need to set*/
	u8	indx : 1;
	u8	rsvd2 : 4;
	u8	cs_en : 1;
	u8	cs_vld :1;
	/*soft need to set*/
	u8	hash : 4;
	u16	checksum;
} __attribute__((__packed__));

enum flow_ctrl_mode_e {
	flow_ctrl_rx_empty,
	flow_ctrl_tx_full,
	flow_ctrl_rx_empty_and_tx_full,
};

enum flow_ctrl_irq_e {
	enter_flow_ctrl,
	exit_flow_ctrl,
	enter_exit_flow_ctrl,
};

struct sipa_cmn_fifo_info {
	const char *fifo_name;
	const char *tx_fifo;
	const char *rx_fifo;

	enum sipa_ep_id relate_ep;
	enum sipa_term_type src_id;
	enum sipa_term_type dst_id;

	/* centered on IPA*/
	bool is_to_ipa;
	bool is_pam;
};

struct sipa_common_fifo {
	enum sipa_cmn_fifo_index idx;

	struct sipa_fifo_attrs tx_fifo;
	struct sipa_fifo_attrs rx_fifo;

	enum sipa_term_type dst_id;
	enum sipa_term_type src_id;

	bool is_receiver;
	bool is_pam;
};

struct sipa_open_fifo_param {
	bool open_flag;
	struct sipa_comm_fifo_params *attr;
	struct sipa_ext_fifo_params *ext_attr;
	bool force_sw_intr;
	sipa_hal_notify_cb cb;
	void *priv;
};

struct sipa_cmn_fifo_tag {
	u32 depth;
	u32 wr;
	u32 rd;
	bool in_iram;

	u32 fifo_base_addr_l;
	u32 fifo_base_addr_h;

	void *virt_addr;
};

struct sipa_cmn_fifo_cfg_tag {
	const char *fifo_name;

	enum sipa_cmn_fifo_index fifo_id;
	enum sipa_cmn_fifo_index fifo_id_dst;

	bool is_recv;
	bool is_pam;

	u32 state;
	u32 pending;
	u32 dst;
	u32 src;

	u32 irq_eb;

	u64 fifo_phy_addr;

	void *priv;
	void __iomem *fifo_reg_base;

	struct kfifo rx_priv_fifo;
	struct kfifo tx_priv_fifo;
	struct sipa_cmn_fifo_tag rx_fifo;
	struct sipa_cmn_fifo_tag tx_fifo;

	u32 enter_flow_ctrl_cnt;
	u32 exit_flow_ctrl_cnt;

	sipa_hal_notify_cb fifo_irq_callback;
};

struct ipa_register_map {
	char *name;
	u32 offset;
	u32 size;
};

struct sipa_pcie_mem_intr_cfg {
	bool eb;
	u64 pcie_mem_intr_reg[4];
	u32 pcie_mem_intr_pattern[4];
};

struct sipa_hw_data {
	const u32 ahb_regnum;
	const struct ipa_register_map *ahb_reg;
	const bool standalone_subsys;
	const u64 dma_mask;
};

struct sipa_endpoint {
	enum sipa_ep_id id;

	struct device *dev;

	/* Centered on CPU/PAM */
	struct sipa_common_fifo send_fifo;
	struct sipa_common_fifo recv_fifo;

	struct sipa_comm_fifo_params send_fifo_param;
	struct sipa_comm_fifo_params recv_fifo_param;

	sipa_notify_cb send_notify;
	sipa_notify_cb recv_notify;

	/* private data for sipa_notify_cb */
	void *send_priv;
	void *recv_priv;

	bool inited;
	bool connected;
	bool suspended;
};

struct sipa_skb_sender {
	struct device *dev;
	struct sipa_endpoint *ep;
	atomic_t left_cnt;
	spinlock_t nic_lock;	/*spinlock for nic lock handling*/
	spinlock_t send_lock;	/*spinlock for send lock handling*/
	struct list_head nic_list;
	struct list_head sending_list;
	struct list_head pair_free_list;
	struct sipa_skb_dma_addr_pair *pair_cache;

	bool free_notify_net;
	bool send_notify_net;

	wait_queue_head_t send_waitq;
	wait_queue_head_t free_waitq;

	struct task_struct *free_thread;
	struct task_struct *send_thread;

	bool init_flag;

	atomic_t check_suspend;
	atomic_t check_flag;

	u32 no_mem_cnt;
	u32 no_free_cnt;
	u32 enter_flow_ctrl_cnt;
	u32 exit_flow_ctrl_cnt;
};

struct sipa_skb_dma_addr_pair {
	struct sk_buff *skb;
	dma_addr_t dma_addr;
	struct list_head list;
	bool need_unmap;
};

struct sipa_skb_array {
	struct sipa_skb_dma_addr_pair *array;
	u32 rp;
	u32 wp;
	u32 depth;
	atomic_t need_fill_cnt;
	struct list_head mem_list;
};

struct sipa_recv_mem_list {
	size_t size;
	void *virt;
	dma_addr_t dma_addr;
	struct page *page;
	struct list_head list;
};

typedef bool (*sipa_check_send_completed)(struct sipa_skb_sender *sender);

struct sipa_nic_cons_res {
	bool initied;
	enum sipa_rm_res_id cons;
	sipa_check_send_completed chk_func;
	struct sipa_skb_sender *chk_priv;
	spinlock_t lock;	/*spinlock for nic_cons_res lock handling*/
	struct delayed_work work;
	bool resource_requested;
	bool reschedule_work;
	bool release_in_progress;
	bool need_request;
	bool request_in_progress;
	bool rm_flow_ctrl;
	unsigned long jiffies;
	struct alarm delay_timer;
};

struct sipa_nic {
	enum sipa_nic_id nic_id;
	struct sipa_endpoint *send_ep;
	int need_notify;
	u32 src_mask;
	int netid;
	struct list_head list;
	sipa_notify_cb cb;
	void *cb_priv;
	atomic_t status;
	bool flow_ctrl_status;
	struct sipa_nic_cons_res rm_res;
};

struct sipa_skb_receiver {
	struct device *dev;
	struct sipa_endpoint *ep;
	u32 rsvd;
	struct sipa_skb_array *fill_array[SIPA_RECV_CMN_FIFO_NUM];

	spinlock_t lock;	/*spinlock for skb_recv lock handling*/
	u32 nic_cnt;
	struct sipa_nic *nic_array[SIPA_NIC_MAX];

	atomic_t check_suspend;
	atomic_t check_flag;

	wait_queue_head_t fill_recv_waitq;

	struct task_struct *fill_recv_thread;

	bool init_flag;
	u32 tx_danger_cnt;
	u32 rx_danger_cnt;
	u32 alloc_skb_cnt;
	u32 wifi_ul_flowctrl;
};

struct sipa_fifo_phy_ops {
	int (*open)(enum sipa_cmn_fifo_index id,
		    struct sipa_cmn_fifo_cfg_tag *cfg_base, void *cookie);
	int (*close)(enum sipa_cmn_fifo_index id,
		     struct sipa_cmn_fifo_cfg_tag *cfg_base);
	int (*reset_fifo)(enum sipa_cmn_fifo_index id,
			  struct sipa_cmn_fifo_cfg_tag *cfg_base);
	int (*set_rx_depth)(enum sipa_cmn_fifo_index id,
			    struct sipa_cmn_fifo_cfg_tag *cfg_base,
			    u32 depth);
	int (*get_rx_depth)(enum sipa_cmn_fifo_index id,
			    struct sipa_cmn_fifo_cfg_tag *cfg_base);
	int (*set_tx_depth)(enum sipa_cmn_fifo_index id,
			    struct sipa_cmn_fifo_cfg_tag *cfg_base,
			    u32 depth);
	int (*get_tx_depth)(enum sipa_cmn_fifo_index id,
			    struct sipa_cmn_fifo_cfg_tag *cfg_base);
	int (*set_intr_errcode)(enum sipa_cmn_fifo_index id,
				struct sipa_cmn_fifo_cfg_tag *base,
				bool enable);
	int (*set_intr_timeout)(enum sipa_cmn_fifo_index id,
				struct sipa_cmn_fifo_cfg_tag *base,
				bool enable, u32 time);
	int (*set_hw_intr_timeout)(enum sipa_cmn_fifo_index id,
				   struct sipa_cmn_fifo_cfg_tag *b,
				   bool enable, u32 time);
	int (*set_intr_thres)(enum sipa_cmn_fifo_index id,
			      struct sipa_cmn_fifo_cfg_tag *b,
			      bool enable, u32 cnt);
	int (*set_hw_intr_thres)(enum sipa_cmn_fifo_index id,
				 struct sipa_cmn_fifo_cfg_tag *b,
				 u32 enable, u32 cnt);
	int (*set_src_dst_term)(enum sipa_cmn_fifo_index id,
				struct sipa_cmn_fifo_cfg_tag *b,
				u32 src, u32 dst);
	int (*enable_flowctrl_irq)(enum sipa_cmn_fifo_index id,
				   struct sipa_cmn_fifo_cfg_tag *b,
				   u32 enable, u32 irq_mode);
	int (*set_flowctrl_mode)(enum sipa_cmn_fifo_index id,
				 struct sipa_cmn_fifo_cfg_tag *b,
				 u32 work_mode,
				 u32 tx_entry_watermark,
				 u32 tx_exit_watermark,
				 u32 rx_entry_watermark,
				 u32 rx_exit_watermark);
	void (*cmn_fifo_non_stop_on_flowctl)(void __iomem *fifo_base, bool status);
	bool (*check_cmn_fifo_flowctl)(void __iomem *fifo_base);
	bool (*check_cmn_fifo_enter_flowctl)(void __iomem *fifo_base);
	bool (*check_cmn_fifo_exit_flowctl)(void __iomem *fifo_base);
	void (*cmn_fifo_flowctl_recover)(void __iomem *fifo_base);
	void (*clr_cmn_fifo_flowctl_interrupt)(void __iomem *fifo_base);
	void (*clr_cfifo_flowctl_enter_inter)(void __iomem *fifo_base);
	void (*clr_cfifo_flowctl_exit_inter)(void __iomem *fifo_base);
	int (*set_node_intr)(enum sipa_cmn_fifo_index id,
			     struct sipa_cmn_fifo_cfg_tag *b,
			     u32 enable);
	int (*set_overflow_intr)(enum sipa_cmn_fifo_index id,
				 struct sipa_cmn_fifo_cfg_tag *b,
				 u32 enable);
	int (*set_tx_full_intr)(enum sipa_cmn_fifo_index id,
				struct sipa_cmn_fifo_cfg_tag *b,
				u32 enable);
	int (*put_node_to_rx_fifo)(struct device *dev,
				   enum sipa_cmn_fifo_index id,
				   struct sipa_cmn_fifo_cfg_tag *b,
				   struct sipa_node_desc_tag *node,
				   u32 num);
	int (*put_node_to_tx_fifo)(struct device *dev,
				   enum sipa_cmn_fifo_index id,
				   struct sipa_cmn_fifo_cfg_tag *b,
				   struct sipa_node_desc_tag *node,
				   u32 num);
	int (*get_tx_left_cnt)(enum sipa_cmn_fifo_index id,
			       struct sipa_cmn_fifo_cfg_tag *b);
	u32 (*recv_node_from_tx_fifo)(struct device *dev,
				      enum sipa_cmn_fifo_index id,
				      struct sipa_cmn_fifo_cfg_tag *cfg_base,
				      struct sipa_node_desc_tag *node,
				      u32 num);
	u32 (*sync_node_from_tx_fifo)(struct device *dev,
				      enum sipa_cmn_fifo_index id,
				      struct sipa_cmn_fifo_cfg_tag *cfg_base,
				      u32 num);
	u32 (*sync_node_to_rx_fifo)(struct device *dev,
				    enum sipa_cmn_fifo_index id,
				    struct sipa_cmn_fifo_cfg_tag *cfg_base,
				    u32 num);
	int (*get_rx_ptr)(enum sipa_cmn_fifo_index id,
			  struct sipa_cmn_fifo_cfg_tag *cfg_base,
			  u32 *wr, u32 *rd);
	int (*get_tx_ptr)(enum sipa_cmn_fifo_index id,
			  struct sipa_cmn_fifo_cfg_tag *cfg_base,
			  u32 *wr, u32 *rd);
	int (*get_filled_depth)(enum sipa_cmn_fifo_index id,
				struct sipa_cmn_fifo_cfg_tag *b,
				u32 *rx, u32 *tx);
	int (*get_tx_full_status)(enum sipa_cmn_fifo_index id,
				  struct sipa_cmn_fifo_cfg_tag *b);
	int (*get_tx_empty_status)(enum sipa_cmn_fifo_index id,
				   struct sipa_cmn_fifo_cfg_tag *b);
	int (*get_rx_full_status)(enum sipa_cmn_fifo_index id,
				  struct sipa_cmn_fifo_cfg_tag *b);
	int (*get_rx_empty_status)(enum sipa_cmn_fifo_index id,
				   struct sipa_cmn_fifo_cfg_tag *b);
	int (*stop_recv)(enum sipa_cmn_fifo_index id,
			 struct sipa_cmn_fifo_cfg_tag *cfg_base,
			 bool stop);
	void *(*get_tx_node_rptr)(enum sipa_cmn_fifo_index id,
				  struct sipa_cmn_fifo_cfg_tag *b,
				  u32 index);
	int (*add_tx_fifo_rptr)(enum sipa_cmn_fifo_index id,
				struct sipa_cmn_fifo_cfg_tag *b,
				u32 tx_rd);
	void *(*get_rx_node_wptr)(enum sipa_cmn_fifo_index id,
				  struct sipa_cmn_fifo_cfg_tag *b,
				  u32 index);
	int (*add_rx_fifo_wptr)(enum sipa_cmn_fifo_index id,
				struct sipa_cmn_fifo_cfg_tag *b,
				u32 index);
	int (*reclaim_cmn_fifo)(enum sipa_cmn_fifo_index id,
				struct sipa_cmn_fifo_cfg_tag *b);
	int (*restore_irq_map_in)(struct sipa_cmn_fifo_cfg_tag *base);
	int (*restore_irq_map_out)(struct sipa_cmn_fifo_cfg_tag *base);
	int (*traverse_int_bit)(enum sipa_cmn_fifo_index id,
				struct sipa_cmn_fifo_cfg_tag *base, int irq);
};

struct sipa_glb_phy_ops {
	int (*set_work_mode)(void __iomem *reg_base, bool is_bypass);
	int (*get_work_mode)(void __iomem *reg_base);
	int (*set_usb_mode)(void __iomem *reg_base, u32 mode);
	int (*set_need_cp_through_pcie)(void __iomem *reg_base, bool enable);
	int (*ctrl_ipa_action)(void __iomem *reg_base, bool enable);
	int (*ctrl_hash_en)(void __iomem *reg_base, u32 term, bool enable);
	int (*ctrl_chksum_en)(void __iomem *reg_base, u32 term, bool enable);
	bool (*get_pause_status)(void __iomem *reg_base);
	bool (*get_resume_status)(void __iomem *reg_base);
	int (*monitor_ipa_or_tft)(void __iomem *reg_base, u32 flag);
	int (*to_pcie_no_mac_hdr)(void __iomem *reg_base, bool enable);
	int (*from_pcie_no_mac_hdr)(void __iomem *reg_base, bool enable);
	int (*dbg_clk_gate_en)(void __iomem *reg_base, bool enable);
	int (*cp_work_status)(void __iomem *reg_base, bool enable);
	bool (*get_wiap_ul_flow_ctrl_status)(void __iomem *reg_base);
	bool (*get_wifi_dl_flow_ctrl_status)(void __iomem *reg_base);
	bool (*get_usb_dl_flow_ctrl_status)(void __iomem *reg_base);
	bool (*get_pcie_dl_flow_ctrl_status)(void __iomem *reg_base);
	bool (*get_cp_ul_flow_ctrl_status)(void __iomem *reg_base);
	int (*input_filter_en)(void __iomem *reg_base, bool enable);
	int (*output_filter_en)(void __iomem *reg_base, bool enable);
	int (*htable_cache_en)(void __iomem *reg_base, bool enable);
	bool (*get_cp_remote_flow_ctrl_status)(void __iomem *reg_base);
	void (*set_flow_ctl_to_src_blk)(void __iomem *reg_base,
					u32 chan, bool enable);
	void (*set_force_to_ap)(void __iomem *reg_base, u32 mask,
				bool enable);
	void (*tcp_special_leave_to_ap)(void __iomem *reg_base);
	void (*cp_dl_flow_ctrl_recover)(void __iomem *reg_base);
	void (*errnode_int_clr)(void __iomem *reg_base);
	void (*map_out_free_fifo_fatal_int_clr)(void __iomem *reg_base);
	void (*dc_int_clr)(void __iomem *reg_base);
	void (*general_int_sel)(void __iomem *reg_base, u32 mask,
				bool enable);
	u32 (*get_general_int_sts)(void __iomem *reg_base);
	void (*cp_dl_flow_ctrl_sel)(void __iomem *reg_base, u32 mode);
	void (*cp_dl_cur_term_num)(void __iomem *reg_base, u32 term);
	void (*cp_dl_dst_term_num)(void __iomem *reg_base, u32 term);
	void (*cp_dl_priority)(void __iomem *reg_base, u32 pri);
	void (*cp_ul_flow_ctrl_sel)(void __iomem *reg_base, u32 mode);
	void (*cp_ul_cur_term_num)(void __iomem *reg_base, u32 term);
	void (*cp_ul_dst_term_num)(void __iomem *reg_base, u32 term);
	void (*cp_ul_priority)(void __iomem *reg_base, u32 pri);
	u32 (*get_ifilter_drop_cnt)(void __iomem *reg_base);
	u32 (*get_ofilter_drop_cnt)(void __iomem *reg_base);
	u32 (*get_htable_count_index)(void __iomem *reg_base);
	u32 (*get_htable_timestamp)(void __iomem *reg_base);
	void (*htable_cache_hit_cnt_en)(void __iomem *reg_base, bool enable);
	u32 (*get_cache_hit_cnt)(void __iomem *reg_base);
	void (*htable_cache_hit_cnt_clr)(void __iomem *reg_base);
	u32 (*get_htable_hit_cnt)(void __iomem *reg_base);
	void (*bypass_dstid_chk_sel)(void __iomem *reg_base, bool node_id);
	void (*bypass_srcid_chk_sel)(void __iomem *reg_base, bool node_id);
	void (*node_dstid_chk_en)(void __iomem *reg_base, bool enable);
	void (*node_srcid_chk_en)(void __iomem *reg_base, bool enable);
	void (*ofilter_drop_cnt_en)(void __iomem *reg_base, bool enable);
	void (*ifilter_drop_cnt_en)(void __iomem *reg_base, bool enable);
	void (*ofilter_drop_clr)(void __iomem *reg_base);
	void (*ifilter_drop_clr)(void __iomem *reg_base);
	void (*set_sw_wr_trans_num_mask)(void __iomem *reg_base, u32 val);
	void (*set_sw_rd_trans_num_mask)(void __iomem *reg_base, u32 val);
	void (*set_ofilter_depth_ipv6)(void __iomem *reg_base, u32 depth);
	void (*set_ofilter_depth_ipv4)(void __iomem *reg_base, u32 depth);
	bool (*get_ofilter_locked_status)(void __iomem *reg_base, bool enable);
	void (*ofilter_ctrl)(void __iomem *reg_base, bool enable);
	void (*set_ifilter_depth_ipv6)(void __iomem *reg_base, u32 depth);
	void (*set_ifilter_depth_ipv4)(void __iomem *reg_base, u32 depth);
	void (*ifilter_ctrl)(void __iomem *reg_base, bool enable);
	void (*set_default_hash)(void __iomem *reg_base, u32 hash);
	void (*set_htable_max_entry_len)(void __iomem *reg_base, u32 hash);
	void (*set_htable_max_rd_len)(void __iomem *reg_base, u32 len);
	void (*map_fifo_sel_mode)(void __iomem *reg_base, bool mode);
	void (*set_map_fifo_cnt)(void __iomem *reg_base, u32 cnt);
	void (*htable_limit_tmr_clr)(void __iomem *reg_base);
	void (*htable_limit_tmr_scale)(void __iomem *reg_base, u32 scale);
	void (*htable_timestamp_tmr_clr)(void __iomem *reg_base);
	void (*htable_timestamp_tmr_scale)(void __iomem *reg_base, u32 scale);
	void (*htable_base_tmr_en)(void __iomem *reg_base, bool enable);
	int (*set_htable)(void __iomem *reg_base, u32 htable_l,
			  u32 htable_h, u32 len);
	bool (*get_cache_sync_done)(void __iomem *reg_base);
	bool (*get_htable_hw_valid)(void __iomem *reg_base);
	bool (*get_ifilter_locked_status)(void __iomem *reg_base, bool enable);
	u32 (*get_cache_wait_update_cnt)(void __iomem *reg_base);
	u32 (*get_htable_errcode)(void __iomem *reg_base);
	u32 (*get_errnode_info_l)(void __iomem *reg_base);
	u32 (*get_errnode_info_m)(void __iomem *reg_base);
	u32 (*get_errnode_info_h)(void __iomem *reg_base);
	void (*set_cache_sync_req)(void __iomem *reg_base);
	void (*set_route_mode_ihl_ctrl_bit)(void __iomem *reg_base,
					    bool enable);
	void (*htable_sw_en)(void __iomem *reg_base, bool enable);
	void (*set_map_hash_mask)(void __iomem *reg_base, u32 mask);
	void (*map_multi_fifo_mode_en)(void __iomem *reg_base, bool enable);
	u32 (*map_multi_fifo_mode)(void __iomem *reg_base);
	void (*errcode_int_en)(void __iomem *reg_base, u32 mode);
	void (*dl_pcie_dma_en)(void __iomem *reg_base, bool enable);
	void (*set_pcie_msi_int_mode)(void __iomem *reg_base, bool enable);
	void (*pcie_hw_int_en)(void __iomem *reg_base, bool enable);
	void (*wiap_ul_dma_en)(void __iomem *reg_base, bool enable);
	void (*set_dfs_cycle_cnt_val)(void __iomem *reg_base, u32 val);
	void (*set_dfs_period)(void __iomem *reg_base, u32 period);
	void (*dfs_en)(void __iomem *reg_base, bool enable);
	void (*dfs_th3)(void __iomem *reg_base, u32 val);
	void (*dfs_th2)(void __iomem *reg_base, u32 val);
	void (*dfs_th1)(void __iomem *reg_base, u32 val);
	void (*dfs_th0)(void __iomem *reg_base, u32 val);
	void (*dc_ip_sel)(void __iomem *reg_base, bool is_tft);
	void (*dc_en)(void __iomem *reg_base, bool enable);
	void (*dc_init)(void __iomem *reg_base);
	void (*dc_loop_mode)(void __iomem *reg_base, bool enable);
	void (*dc_chn_sel)(void __iomem *reg_base, u32 chn);
	void (*dc_stop_sel)(void __iomem *reg_base, u32 chn);
	void (*dc_start_sel)(void __iomem *reg_base, u32 chn);
	void (*dc_sw_stop)(void __iomem *reg_base);
	void (*dc_sw_start)(void __iomem *reg_base);
	void (*dc_sw_len)(void __iomem *reg_base, u32 len);
	void (*set_dc_data_mask)(void __iomem *reg_base, u32 mask);
	void (*set_dc_meet_cond)(void __iomem *reg_base, u32 cond);
	void (*set_dc_init_raddr)(void __iomem *reg_base, u32 addr);
	void (*set_dc_raddr_load)(void __iomem *reg_base);
	void (*set_fifo_monitor_sel0)(void __iomem *reg_base, u32 chn);
	void (*set_ip_pkt_fifo_monitor_sel)(void __iomem *reg_base, u32 val);
	void (*set_output_chn_fifo_monitor_sel)(void __iomem *reg_base,
						u32 val);
	void (*set_input_chn_fifo_monitor_sel)(void __iomem *reg_base, u32 val);
	void (*set_map3_free_fifo_pri)(void __iomem *reg_base, u32 pri);
	void (*set_map2_free_fifo_pri)(void __iomem *reg_base, u32 pri);
	void (*set_map1_free_fifo_pri)(void __iomem *reg_base, u32 pri);
	void (*set_map0_free_fifo_pri)(void __iomem *reg_base, u32 pri);
	void (*set_map7_free_fifo_pri)(void __iomem *reg_base, u32 pri);
	void (*set_map6_free_fifo_pri)(void __iomem *reg_base, u32 pri);
	void (*set_map5_free_fifo_pri)(void __iomem *reg_base, u32 pri);
	void (*set_map4_free_fifo_pri)(void __iomem *reg_base, u32 pri);
	void (*out_map_en)(void __iomem *reg_base, u32 mask);
	void (*out_wiap_en)(void __iomem *reg_base, bool enable);
	void (*out_pcie_en)(void __iomem *reg_base, bool enable);
	void (*out_wifi_en)(void __iomem *reg_base, bool enable);
	void (*out_usb_en)(void __iomem *reg_base, bool enable);
	void (*in_map_en)(void __iomem *reg_base, bool enable);
	void (*in_wiap_en)(void __iomem *reg_base, bool enable);
	void (*in_pcie_en)(void __iomem *reg_base, bool enable);
	void (*in_wifi_en)(void __iomem *reg_base, bool enable);
	void (*in_usb_en)(void __iomem *reg_base, bool enable);
	void (*set_pcie_dl_tx_fifo_int_addr_low)(void __iomem *reg_base,
						 u32 addr);
	void (*set_pcie_dl_rx_fifo_int_addr_low)(void __iomem *reg_base,
						 u32 addr);
	void (*set_pcie_ul_tx_fifo_int_addr_low)(void __iomem *reg_base,
						 u32 addr);
	void (*set_pcie_ul_rx_fifo_int_addr_low)(void __iomem *reg_base,
						 u32 addr);
	void (*set_pcie_dl_tx_fifo_int_addr_high)(void __iomem *reg_base,
						  u32 addr);
	void (*set_pcie_dl_rx_fifo_int_addr_high)(void __iomem *reg_base,
						  u32 addr);
	void (*set_pcie_ul_tx_fifo_int_addr_high)(void __iomem *reg_base,
						  u32 addr);
	void (*set_pcie_ul_rx_fifo_int_addr_high)(void __iomem *reg_base,
						  u32 addr);
	void (*set_pcie_dl_tx_fifo_int_pattern)(void __iomem *reg_base,
						u32 addr);
	void (*set_pcie_dl_rx_fifo_int_pattern)(void __iomem *reg_base,
						u32 addr);
	void (*set_pcie_ul_tx_fifo_int_pattern)(void __iomem *reg_base,
						u32 addr);
	void (*set_pcie_ul_rx_fifo_int_pattern)(void __iomem *reg_base,
						u32 addr);
	void (*traffic_counter_clr)(void __iomem *reg_base);
	void (*traffic_counter_en)(void __iomem *reg_base, bool enable);
	void (*set_wifi_ul_map0_int_sel)(void __iomem *reg_base,
					 bool status);
	void (*map0_interrupt_src_en)(void __iomem *reg_base, u32 src,
				      bool enable);
	void (*map1_interrupt_src_en)(void __iomem *reg_base, u32 src,
				      bool enable);
	void (*map2_interrupt_src_en)(void __iomem *reg_base, u32 src,
				      bool enable);
	void (*map3_interrupt_src_en)(void __iomem *reg_base, u32 src,
				      bool enable);
	void (*map4_interrupt_src_en)(void __iomem *reg_base, u32 src,
				      bool enable);
	void (*map5_interrupt_src_en)(void __iomem *reg_base, u32 src,
				      bool enable);
	void (*map6_interrupt_src_en)(void __iomem *reg_base, u32 src,
				      bool enable);
	void (*map7_interrupt_src_en)(void __iomem *reg_base, u32 src,
				      bool enable);
	bool (*get_dc_done_status)(void __iomem *reg_base);
	u64 (*get_usb_push_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_usb_push_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_usb_push_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_usb_push_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_usb_push_node_l4_cnt)(void __iomem *reg_base);
	u64 (*get_usb_fetch_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_usb_fetch_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_usb_fetch_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_usb_fetch_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_usb_fetch_node_l4_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_push_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_push_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_push_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_push_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_push_node_l4_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_fetch_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_fetch_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_fetch_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_fetch_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_pcie_fetch_node_l4_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_push_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_push_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_push_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_push_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_push_node_l4_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_fetch_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_fetch_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_fetch_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_fetch_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_wifi_fetch_node_l4_cnt)(void __iomem *reg_base);
	u64 (*get_map_push_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_map_push_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_map_push_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_map_push_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_map_push_node_l4_cnt)(void __iomem *reg_base);
	u64 (*get_map_fetch_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_map_fetch_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_map_fetch_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_map_fetch_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_map_fetch_node_l4_cnt)(void __iomem *reg_base);
	u64 (*get_cp_push_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_cp_push_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_cp_push_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_cp_push_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_cp_push_node_l4_cnt)(void __iomem *reg_base);
	u64 (*get_cp_fetch_node_total_cnt)(void __iomem *reg_base);
	u64 (*get_cp_fetch_node_l1_cnt)(void __iomem *reg_base);
	u64 (*get_cp_fetch_node_l2_cnt)(void __iomem *reg_base);
	u64 (*get_cp_fetch_node_l3_cnt)(void __iomem *reg_base);
	u64 (*get_cp_fetch_node_l4_cnt)(void __iomem *reg_base);
	u32 (*get_dc_vld_clk_cnt)(void __iomem *reg_base);
	u32 (*get_dc_raddr)(void __iomem *reg_base);
	u32 (*get_ip_pkt_fifo_empty_hit)(void __iomem *reg_base);
	u32 (*get_ip_pkt_fifo_full_hit)(void __iomem *reg_base);
	u32 (*get_data_chn_fifo_empty_hit)(void __iomem *reg_base);
	u32 (*get_data_chn_fifo_full_hit)(void __iomem *reg_base);
	u32 (*get_input_chn_fifo_empty_hit)(void __iomem *reg_base);
	u32 (*get_input_chn_fifo_full_hit)(void __iomem *reg_base);
	u32 (*get_output_chn_fifo_empty_hit)(void __iomem *reg_base);
	u32 (*get_output_chn_fifo_full_hit)(void __iomem *reg_base);
	u32 (*get_clk_gate_aon_0)(void __iomem *reg_base);
	u32 (*get_clk_gate_aon_1)(void __iomem *reg_base);
	u32 (*get_map0_int_sts)(void __iomem *reg_base);
	u32 (*get_map1_int_sts)(void __iomem *reg_base);
	u32 (*get_map2_int_sts)(void __iomem *reg_base);
	u32 (*get_map3_int_sts)(void __iomem *reg_base);
	u32 (*get_map4_int_sts)(void __iomem *reg_base);
	u32 (*get_map5_int_sts)(void __iomem *reg_base);
	u32 (*get_map6_int_sts)(void __iomem *reg_base);
	u32 (*get_map7_int_sts)(void __iomem *reg_base);
	u32 (*get_fifo_irq_status)(int cpu, void __iomem *reg_base);
	int (*ctrl_def_hash_en)(void __iomem *reg_base);
	int (*ctrl_def_chksum_en)(void __iomem *reg_base);
	void (*enable_def_interrupt_src)(void __iomem *reg_base);
	void (*set_def_flow_ctl_to_src_blk)(void __iomem *reg_base);
	void (*set_map_flow_ctl_to_wifi)(void __iomem *reg_base,
					 bool status);
	void (*fill_ifilter_ipv4)(void __iomem *reg_base, u32 data);
	void (*fill_ifilter_ipv6)(void __iomem *reg_base, u32 data);
	void (*fill_ofilter_ipv4)(void __iomem *reg_base, u32 data);
	void (*fill_ofilter_ipv6)(void __iomem *reg_base, u32 data);
};

struct sipa_eb_register {
	struct regmap *enable_rmap;
	u32 enable_reg;
	u32 enable_mask;
};

struct sipa_plat_drv_cfg {
	struct device *dev;

	struct sipa_endpoint *eps[SIPA_EP_MAX];

	/* protect ipa and tft eb bit */
	spinlock_t enable_lock;
	struct sipa_eb_register eb_regs[SIPA_EB_NUM];

	/* avoid pam connect and power_wq race */
	struct mutex resume_lock;

	struct work_struct flow_ctrl_work;

	struct delayed_work power_work;
	struct workqueue_struct *power_wq;

	/* ipa power status */
	bool power_flag;

	/* IPA NIC interface */
	spinlock_t mode_lock;
	int mode_state;
	struct sipa_nic *nic[SIPA_NIC_MAX];

	/* sender & receiver */
	struct sipa_skb_sender *sender;
	struct sipa_skb_receiver *receiver;

	/* usb rm completion */
	struct completion usb_rm_comp;

	/* ipa producer */
	struct sipa_rm_create_params ipa_rm;

	/* ipa clk */
	struct clk *ipa_core_clk;
	struct clk *ipa_core_parent;
	struct clk *ipa_core_default;

	/* vip attrs */
	struct sipa_vip_attrs *vip_attrs;

	u32 enable_cnt;
	u32 suspend_stage;

	int general_intr;
	int multi_intr[SIPA_MULTI_IRQ_NUM];
	char *multi_irq_name[SIPA_MULTI_IRQ_NUM];
	struct cpumask cpu_mask;

	int is_bypass;
	bool wiap_ul_dma;
	bool pcie_dl_dma;
	bool need_through_pcie;
	u32 sipa_sys_eb;
	u32 sipa_cpu_type;

	phys_addr_t glb_phy;
	resource_size_t glb_size;
	void  __iomem *glb_virt_base;

	phys_addr_t iram_phy;
	resource_size_t iram_size;
	void  __iomem *iram_virt_base;

	const struct sipa_hw_data *hw_data;
	u32 suspend_cnt;
	u32 resume_cnt;

	u64 last_idle_time[NR_CPUS];
	u32 idle_perc[NR_CPUS];
	struct hrtimer daemon_timer;
	u32 cpu_num;
	u32 cpu_num_ano;
	u32 fifo_rate[SIPA_MULTI_IRQ_NUM];
	int user_set;
	bool udp_frag;
	bool udp_port;
	atomic_t udp_port_num;
	bool multi_mode;
	wait_queue_head_t set_rps_waitq;
	struct task_struct *set_rps_thread;
	int set_rps;
	int cp_flow;
	bool hrtimer_eb;
	/*protect cp flow ctrl */
	spinlock_t flow_lock;

	/* common TX fifo > 500, switch to middle core, and when node's
	 * count in common TX fido < 2000, switch to little core
	 */
	bool sw_mc_set_rps_doing;
	bool is_middle_core;
	struct hrtimer sw_little_core_timer;
	u32 fifo_rate2[SIPA_MULTI_IRQ_NUM];
	u32 rx_filled, tx_filled;
	u32 low_rate_cont_times;  //lower rate continuous times
	bool enable_sw_core;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_root;
#endif
	struct sipa_fifo_phy_ops fifo_ops;
	struct sipa_glb_phy_ops glb_ops;
	struct sipa_open_fifo_param fifo_param[SIPA_FIFO_MAX];
	struct sipa_cmn_fifo_cfg_tag cmn_fifo_cfg[SIPA_FIFO_MAX];
};

irqreturn_t sipa_multi_int_callback_func(int evt, void *cookie);
void sipa_fifo_ops_init(struct sipa_fifo_phy_ops *ops);
void sipa_glb_ops_init(struct sipa_glb_phy_ops *ops);

int sipa_create_skb_sender(struct sipa_plat_drv_cfg *ipa,
			   struct sipa_endpoint *ep,
			   struct sipa_skb_sender **sender_pp);

void destroy_sipa_skb_sender(struct sipa_skb_sender *sender);

u32 sipa_get_suspend_status(void);

struct sipa_plat_drv_cfg *sipa_get_ctrl_pointer(void);

int sipa_skb_sender_send_data(struct sipa_skb_sender *sender,
			      struct sk_buff *skb,
			      enum sipa_term_type dst,
			      u8 netid);

bool sipa_skb_sender_check_send_complete(struct sipa_skb_sender *sender);

void sipa_skb_sender_add_nic(struct sipa_skb_sender *sender,
			     struct sipa_nic *nic);

void sipa_skb_sender_remove_nic(struct sipa_skb_sender *sender,
				struct sipa_nic *nic);

int sipa_create_skb_receiver(struct sipa_plat_drv_cfg *ipa,
			     struct sipa_endpoint *ep,
			     struct sipa_skb_receiver **receiver_pp);

void sipa_receiver_add_nic(struct sipa_skb_receiver *receiver,
			   struct sipa_nic *nic);

void sipa_nic_try_notify_recv(struct sipa_nic *nic);

void sipa_nic_notify_evt(struct sipa_nic *nic, enum sipa_evt_type evt);

void sipa_nic_push_skb(struct sipa_nic *nic, struct sk_buff *skb);

int sipa_nic_rx_has_data(enum sipa_nic_id nic_id);

int sipa_sender_prepare_suspend(struct sipa_skb_sender *sender);
int sipa_sender_prepare_resume(struct sipa_skb_sender *sender);

int sipa_receiver_prepare_suspend(struct sipa_skb_receiver *receiver);
int sipa_receiver_prepare_resume(struct sipa_skb_receiver *receiver);

void sipa_init_free_fifo(struct sipa_skb_receiver *receiver, u32 cnt,
			 enum sipa_cmn_fifo_index id);

void sipa_reinit_recv_array(struct device *dev);

struct sk_buff *sipa_recv_skb(struct sipa_skb_receiver *receiver,
			      struct sipa_node_info *node_info,
			      u32 index, int fifoid);
void sipa_single_middle_core(void);
#endif /* _SIPA_PRIV_H_ */
