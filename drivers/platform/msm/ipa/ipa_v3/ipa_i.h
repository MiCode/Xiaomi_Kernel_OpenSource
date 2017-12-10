/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _IPA3_I_H_
#define _IPA3_I_H_

#include <linux/bitops.h>
#include <linux/cdev.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/ipa.h>
#include <linux/ipa_usb.h>
#include <linux/msm-sps.h>
#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include "ipa_hw_defs.h"
#include "ipa_ram_mmap.h"
#include "ipa_qmi_service.h"
#include "../ipa_api.h"
#include "ipahal/ipahal_reg.h"
#include "ipahal/ipahal.h"
#include "../ipa_common_i.h"
#include "ipa_uc_offload_i.h"

#define DRV_NAME "ipa"
#define IPA_COOKIE 0x57831603
#define IPA_RT_RULE_COOKIE 0x57831604
#define IPA_RT_TBL_COOKIE 0x57831605
#define IPA_FLT_COOKIE 0x57831606
#define IPA_HDR_COOKIE 0x57831607
#define IPA_PROC_HDR_COOKIE 0x57831608

#define MTU_BYTE 1500

#define IPA_EP_NOT_ALLOCATED (-1)
#define IPA3_MAX_NUM_PIPES 31
#define IPA_SYS_DESC_FIFO_SZ 0x800
#define IPA_SYS_TX_DATA_DESC_FIFO_SZ 0x1000
#define IPA_COMMON_EVENT_RING_SIZE 0x7C00
#define IPA_LAN_RX_HEADER_LENGTH (2)
#define IPA_QMAP_HEADER_LENGTH (4)
#define IPA_DL_CHECKSUM_LENGTH (8)
#define IPA_NUM_DESC_PER_SW_TX (3)
#define IPA_GENERIC_RX_POOL_SZ 192
#define IPA_UC_FINISH_MAX 6
#define IPA_UC_WAIT_MIN_SLEEP 1000
#define IPA_UC_WAII_MAX_SLEEP 1200

#define IPA_MAX_STATUS_STAT_NUM 30

#define IPA_IPC_LOG_PAGES 50

#define IPA_PM_THRESHOLD_MAX 2
#define IPA_MAX_NUM_REQ_CACHE 10

#define IPADBG(fmt, args...) \
	do { \
		pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
		if (ipa3_ctx) { \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf, \
				DRV_NAME " %s:%d " fmt, ## args); \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define IPADBG_LOW(fmt, args...) \
	do { \
		pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
		if (ipa3_ctx) \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAERR(fmt, args...) \
	do { \
		pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args);\
		if (ipa3_ctx) { \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf, \
				DRV_NAME " %s:%d " fmt, ## args); \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define IPAERR_RL(fmt, args...) \
	do { \
		pr_err_ratelimited(DRV_NAME " %s:%d " fmt, __func__,\
		__LINE__, ## args);\
		if (ipa3_ctx) { \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf, \
				DRV_NAME " %s:%d " fmt, ## args); \
			IPA_IPC_LOGGING(ipa3_ctx->logbuf_low, \
				DRV_NAME " %s:%d " fmt, ## args); \
		} \
	} while (0)

#define WLAN_AMPDU_TX_EP 15
#define WLAN_PROD_TX_EP  19
#define WLAN1_CONS_RX_EP  14
#define WLAN2_CONS_RX_EP  16
#define WLAN3_CONS_RX_EP  17
#define WLAN4_CONS_RX_EP  18

#define IPA_STATS

#ifdef IPA_STATS
#define IPA_STATS_INC_CNT(val) (++val)
#define IPA_STATS_DEC_CNT(val) (--val)
#define IPA_STATS_EXCP_CNT(__excp, __base) do {				\
	if (__excp < 0 || __excp >= IPAHAL_PKT_STATUS_EXCEPTION_MAX)	\
		break;							\
	++__base[__excp];						\
	} while (0)
#else
#define IPA_STATS_INC_CNT(x) do { } while (0)
#define IPA_STATS_DEC_CNT(x)
#define IPA_STATS_EXCP_CNT(__excp, __base) do { } while (0)
#endif

#define IPA_TOS_EQ			BIT(0)
#define IPA_PROTOCOL_EQ			BIT(1)
#define IPA_TC_EQ			BIT(2)
#define IPA_OFFSET_MEQ128_0		BIT(3)
#define IPA_OFFSET_MEQ128_1		BIT(4)
#define IPA_OFFSET_MEQ32_0		BIT(5)
#define IPA_OFFSET_MEQ32_1		BIT(6)
#define IPA_IHL_OFFSET_MEQ32_0		BIT(7)
#define IPA_IHL_OFFSET_MEQ32_1		BIT(8)
#define IPA_METADATA_COMPARE		BIT(9)
#define IPA_IHL_OFFSET_RANGE16_0	BIT(10)
#define IPA_IHL_OFFSET_RANGE16_1	BIT(11)
#define IPA_IHL_OFFSET_EQ_32		BIT(12)
#define IPA_IHL_OFFSET_EQ_16		BIT(13)
#define IPA_FL_EQ			BIT(14)
#define IPA_IS_FRAG			BIT(15)

#define IPA_HDR_BIN0 0
#define IPA_HDR_BIN1 1
#define IPA_HDR_BIN2 2
#define IPA_HDR_BIN3 3
#define IPA_HDR_BIN4 4
#define IPA_HDR_BIN_MAX 5

#define IPA_HDR_PROC_CTX_BIN0 0
#define IPA_HDR_PROC_CTX_BIN1 1
#define IPA_HDR_PROC_CTX_BIN_MAX 2

#define IPA_EVENT_THRESHOLD 0x10

/*
 * Due to ZLT issue with USB 3.0 core, IPA BAM threashold need to be set
 * to max packet size + 1. After setting the threshold, USB core
 * will not be notified on ZLTs
 */
#define IPA_USB_EVENT_THRESHOLD 0x4001

#define IPA_RX_POOL_CEIL 32
#define IPA_RX_SKB_SIZE 1792

#define IPA_A5_MUX_HDR_NAME "ipa_excp_hdr"
#define IPA_LAN_RX_HDR_NAME "ipa_lan_hdr"
#define IPA_INVALID_L4_PROTOCOL 0xFF

#define IPA_HW_TABLE_ALIGNMENT(start_ofst) \
	(((start_ofst) + 127) & ~127)
#define IPA_RT_FLT_HW_RULE_BUF_SIZE	(256)

#define IPA_HW_TBL_WIDTH (8)
#define IPA_HW_TBL_SYSADDR_ALIGNMENT (127)
#define IPA_HW_TBL_LCLADDR_ALIGNMENT (7)
#define IPA_HW_TBL_ADDR_MASK (127)
#define IPA_HW_TBL_BLK_SIZE_ALIGNMENT (127)
#define IPA_HW_TBL_HDR_WIDTH (8)
#define IPA_HW_RULE_START_ALIGNMENT (7)

/*
 * for local tables (at sram) offsets is used as tables addresses
 * offset need to be in 8B units (local address aligned) and
 * left shifted to its place. Local bit need to be enabled.
 */
#define IPA_HW_TBL_OFSET_TO_LCLADDR(__ofst) \
	( \
	(((__ofst)/(IPA_HW_TBL_LCLADDR_ALIGNMENT+1)) * \
	(IPA_HW_TBL_ADDR_MASK + 1)) + 1 \
	)

#define IPA_RULE_MAX_PRIORITY (0)
#define IPA_RULE_MIN_PRIORITY (1023)

#define IPA_RULE_ID_MIN_VAL (0x01)
#define IPA_RULE_ID_MAX_VAL (0x1FF)
#define IPA_RULE_ID_RULE_MISS (0x3FF)

#define IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE 8
#define IPA_HDR_PROC_CTX_TABLE_ALIGNMENT(start_ofst) \
	(((start_ofst) + IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE - 1) & \
	~(IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE - 1))

#define MAX_RESOURCE_TO_CLIENTS (IPA_CLIENT_MAX)
#define IPA_MEM_PART(x_) (ipa3_ctx->ctrl->mem_partition.x_)

#define IPA_GSI_CHANNEL_STOP_MAX_RETRY 10
#define IPA_GSI_CHANNEL_STOP_PKT_SIZE 1

#define IPA_GSI_CHANNEL_EMPTY_MAX_RETRY 15
#define IPA_GSI_CHANNEL_EMPTY_SLEEP_MIN_USEC (1000)
#define IPA_GSI_CHANNEL_EMPTY_SLEEP_MAX_USEC (2000)

#define IPA_SLEEP_CLK_RATE_KHZ (32)

#define IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES 120
#define IPA3_ACTIVE_CLIENTS_LOG_LINE_LEN 96
#define IPA3_ACTIVE_CLIENTS_LOG_HASHTABLE_SIZE 50
#define IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN 40

struct ipa3_active_client_htable_entry {
	struct hlist_node list;
	char id_string[IPA3_ACTIVE_CLIENTS_LOG_NAME_LEN];
	int count;
	enum ipa_active_client_log_type type;
};

struct ipa3_active_clients_log_ctx {
	char *log_buffer[IPA3_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES];
	int log_head;
	int log_tail;
	bool log_rdy;
	struct hlist_head htable[IPA3_ACTIVE_CLIENTS_LOG_HASHTABLE_SIZE];
};

struct ipa3_client_names {
	enum ipa_client_type names[MAX_RESOURCE_TO_CLIENTS];
	int length;
};

struct ipa_smmu_cb_ctx {
	bool valid;
	struct device *dev;
	struct dma_iommu_mapping *mapping;
	struct iommu_domain *iommu;
	unsigned long next_addr;
	u32 va_start;
	u32 va_size;
	u32 va_end;
};

/**
 * struct ipa3_flt_entry - IPA filtering table entry
 * @link: entry's link in global filtering enrties list
 * @rule: filter rule
 * @cookie: cookie used for validity check
 * @tbl: filter table
 * @rt_tbl: routing table
 * @hw_len: entry's size
 * @id: rule handle - globally unique
 * @prio: rule 10bit priority which defines the order of the rule
 *  among other rules at the same integrated table
 * @rule_id: rule 10bit ID to be returned in packet status
 */
struct ipa3_flt_entry {
	struct list_head link;
	u32 cookie;
	struct ipa_flt_rule rule;
	struct ipa3_flt_tbl *tbl;
	struct ipa3_rt_tbl *rt_tbl;
	u32 hw_len;
	int id;
	u16 prio;
	u16 rule_id;
};

/**
 * struct ipa3_rt_tbl - IPA routing table
 * @link: table's link in global routing tables list
 * @head_rt_rule_list: head of routing rules list
 * @name: routing table name
 * @idx: routing table index
 * @rule_cnt: number of rules in routing table
 * @ref_cnt: reference counter of routing table
 * @set: collection of routing tables
 * @cookie: cookie used for validity check
 * @in_sys: flag indicating if the table is located in system memory
 * @sz: the size of the routing table
 * @curr_mem: current routing tables block in sys memory
 * @prev_mem: previous routing table block in sys memory
 * @id: routing table id
 * @rule_ids: idr structure that holds the rule_id for each rule
 */
struct ipa3_rt_tbl {
	struct list_head link;
	u32 cookie;
	struct list_head head_rt_rule_list;
	char name[IPA_RESOURCE_NAME_MAX];
	u32 idx;
	u32 rule_cnt;
	u32 ref_cnt;
	struct ipa3_rt_tbl_set *set;
	bool in_sys[IPA_RULE_TYPE_MAX];
	u32 sz[IPA_RULE_TYPE_MAX];
	struct ipa_mem_buffer curr_mem[IPA_RULE_TYPE_MAX];
	struct ipa_mem_buffer prev_mem[IPA_RULE_TYPE_MAX];
	int id;
	struct idr rule_ids;
};

/**
 * struct ipa3_hdr_entry - IPA header table entry
 * @link: entry's link in global header table entries list
 * @hdr: the header
 * @hdr_len: header length
 * @name: name of header table entry
 * @type: l2 header type
 * @is_partial: flag indicating if header table entry is partial
 * @is_hdr_proc_ctx: false - hdr entry resides in hdr table,
 * true - hdr entry resides in DDR and pointed to by proc ctx
 * @phys_base: physical address of entry in DDR when is_hdr_proc_ctx is true,
 * else 0
 * @proc_ctx: processing context header
 * @offset_entry: entry's offset
 * @cookie: cookie used for validity check
 * @ref_cnt: reference counter of routing table
 * @id: header entry id
 * @is_eth2_ofst_valid: is eth2_ofst field valid?
 * @eth2_ofst: offset to start of Ethernet-II/802.3 header
 * @user_deleted: is the header deleted by the user?
 */
struct ipa3_hdr_entry {
	struct list_head link;
	u32 cookie;
	u8 hdr[IPA_HDR_MAX_SIZE];
	u32 hdr_len;
	char name[IPA_RESOURCE_NAME_MAX];
	enum ipa_hdr_l2_type type;
	u8 is_partial;
	bool is_hdr_proc_ctx;
	dma_addr_t phys_base;
	struct ipa3_hdr_proc_ctx_entry *proc_ctx;
	struct ipa_hdr_offset_entry *offset_entry;
	u32 ref_cnt;
	int id;
	u8 is_eth2_ofst_valid;
	u16 eth2_ofst;
	bool user_deleted;
};

/**
 * struct ipa3_hdr_tbl - IPA header table
 * @head_hdr_entry_list: header entries list
 * @head_offset_list: header offset list
 * @head_free_offset_list: header free offset list
 * @hdr_cnt: number of headers
 * @end: the last header index
 */
struct ipa3_hdr_tbl {
	struct list_head head_hdr_entry_list;
	struct list_head head_offset_list[IPA_HDR_BIN_MAX];
	struct list_head head_free_offset_list[IPA_HDR_BIN_MAX];
	u32 hdr_cnt;
	u32 end;
};

/**
 * struct ipa3_hdr_offset_entry - IPA header offset entry
 * @link: entry's link in global processing context header offset entries list
 * @offset: the offset
 * @bin: bin
 */
struct ipa3_hdr_proc_ctx_offset_entry {
	struct list_head link;
	u32 offset;
	u32 bin;
};

/**
 struct ipa3_hdr_proc_ctx_entry - IPA processing context header table entry
 * @link: entry's link in global header table entries list
 * @type: header processing context type
 * @l2tp_params: L2TP parameters
 * @offset_entry: entry's offset
 * @hdr: the header
 * @cookie: cookie used for validity check
 * @ref_cnt: reference counter of routing table
 * @id: processing context header entry id
 * @user_deleted: is the hdr processing context deleted by the user?
 */
struct ipa3_hdr_proc_ctx_entry {
	struct list_head link;
	u32 cookie;
	enum ipa_hdr_proc_type type;
	struct ipa_l2tp_hdr_proc_ctx_params l2tp_params;
	struct ipa3_hdr_proc_ctx_offset_entry *offset_entry;
	struct ipa3_hdr_entry *hdr;
	u32 ref_cnt;
	int id;
	bool user_deleted;
};

/**
 * struct ipa3_hdr_proc_ctx_tbl - IPA processing context header table
 * @head_proc_ctx_entry_list: header entries list
 * @head_offset_list: header offset list
 * @head_free_offset_list: header free offset list
 * @proc_ctx_cnt: number of processing context headers
 * @end: the last processing context header index
 * @start_offset: offset in words of processing context header table
 */
struct ipa3_hdr_proc_ctx_tbl {
	struct list_head head_proc_ctx_entry_list;
	struct list_head head_offset_list[IPA_HDR_PROC_CTX_BIN_MAX];
	struct list_head head_free_offset_list[IPA_HDR_PROC_CTX_BIN_MAX];
	u32 proc_ctx_cnt;
	u32 end;
	u32 start_offset;
};

/**
 * struct ipa3_flt_tbl - IPA filter table
 * @head_flt_rule_list: filter rules list
 * @rule_cnt: number of filter rules
 * @in_sys: flag indicating if filter table is located in system memory
 * @sz: the size of the filter tables
 * @end: the last header index
 * @curr_mem: current filter tables block in sys memory
 * @prev_mem: previous filter table block in sys memory
 * @rule_ids: idr structure that holds the rule_id for each rule
 */
struct ipa3_flt_tbl {
	struct list_head head_flt_rule_list;
	u32 rule_cnt;
	bool in_sys[IPA_RULE_TYPE_MAX];
	u32 sz[IPA_RULE_TYPE_MAX];
	struct ipa_mem_buffer curr_mem[IPA_RULE_TYPE_MAX];
	struct ipa_mem_buffer prev_mem[IPA_RULE_TYPE_MAX];
	bool sticky_rear;
	struct idr rule_ids;
};

/**
 * struct ipa3_rt_entry - IPA routing table entry
 * @link: entry's link in global routing table entries list
 * @rule: routing rule
 * @cookie: cookie used for validity check
 * @tbl: routing table
 * @hdr: header table
 * @proc_ctx: processing context table
 * @hw_len: the length of the table
 * @id: rule handle - globaly unique
 * @prio: rule 10bit priority which defines the order of the rule
 *  among other rules at the integrated same table
 * @rule_id: rule 10bit ID to be returned in packet status
 */
struct ipa3_rt_entry {
	struct list_head link;
	u32 cookie;
	struct ipa_rt_rule rule;
	struct ipa3_rt_tbl *tbl;
	struct ipa3_hdr_entry *hdr;
	struct ipa3_hdr_proc_ctx_entry *proc_ctx;
	u32 hw_len;
	int id;
	u16 prio;
	u16 rule_id;
	u16 rule_id_valid;
};

/**
 * struct ipa3_rt_tbl_set - collection of routing tables
 * @head_rt_tbl_list: collection of routing tables
 * @tbl_cnt: number of routing tables
 */
struct ipa3_rt_tbl_set {
	struct list_head head_rt_tbl_list;
	u32 tbl_cnt;
};

/**
 * struct ipa3_wlan_stats - Wlan stats for each wlan endpoint
 * @rx_pkts_rcvd: Packets sent by wlan driver
 * @rx_pkts_status_rcvd: Status packets received from ipa hw
 * @rx_hd_processed: Data Descriptors processed by IPA Driver
 * @rx_hd_reply: Data Descriptors recycled by wlan driver
 * @rx_hd_rcvd: Data Descriptors sent by wlan driver
 * @rx_pkt_leak: Packet count that are not recycled
 * @rx_dp_fail: Packets failed to transfer to IPA HW
 * @tx_pkts_rcvd: SKB Buffers received from ipa hw
 * @tx_pkts_sent: SKB Buffers sent to wlan driver
 * @tx_pkts_dropped: Dropped packets count
 */
struct ipa3_wlan_stats {
	u32 rx_pkts_rcvd;
	u32 rx_pkts_status_rcvd;
	u32 rx_hd_processed;
	u32 rx_hd_reply;
	u32 rx_hd_rcvd;
	u32 rx_pkt_leak;
	u32 rx_dp_fail;
	u32 tx_pkts_rcvd;
	u32 tx_pkts_sent;
	u32 tx_pkts_dropped;
};

/**
 * struct ipa3_wlan_comm_memb - Wlan comm members
 * @wlan_spinlock: protects wlan comm buff list and its size
 * @ipa_tx_mul_spinlock: protects tx dp mul transfer
 * @wlan_comm_total_cnt: wlan common skb buffers allocated count
 * @wlan_comm_free_cnt: wlan common skb buffer free count
 * @total_tx_pkts_freed: Recycled Buffer count
 * @wlan_comm_desc_list: wlan common skb buffer list
 */
struct ipa3_wlan_comm_memb {
	spinlock_t wlan_spinlock;
	spinlock_t ipa_tx_mul_spinlock;
	u32 wlan_comm_total_cnt;
	u32 wlan_comm_free_cnt;
	u32 total_tx_pkts_freed;
	struct list_head wlan_comm_desc_list;
	atomic_t active_clnt_cnt;
};

struct ipa_gsi_ep_mem_info {
	u16 evt_ring_len;
	u64 evt_ring_base_addr;
	void *evt_ring_base_vaddr;
	u16 chan_ring_len;
	u64 chan_ring_base_addr;
	void *chan_ring_base_vaddr;
};

struct ipa3_status_stats {
	struct ipahal_pkt_status status[IPA_MAX_STATUS_STAT_NUM];
	unsigned int curr;
};

/**
 * struct ipa3_ep_context - IPA end point context
 * @valid: flag indicating id EP context is valid
 * @client: EP client type
 * @ep_hdl: EP's client SPS handle
 * @gsi_chan_hdl: EP's GSI channel handle
 * @gsi_evt_ring_hdl: EP's GSI channel event ring handle
 * @gsi_mem_info: EP's GSI channel rings info
 * @chan_scratch: EP's GSI channel scratch info
 * @cfg: EP cionfiguration
 * @dst_pipe_index: destination pipe index
 * @rt_tbl_idx: routing table index
 * @connect: SPS connect
 * @priv: user provided information which will forwarded once the user is
 *        notified for new data avail
 * @client_notify: user provided CB for EP events notification, the event is
 *                 data revived.
 * @desc_fifo_in_pipe_mem: flag indicating if descriptors FIFO uses pipe memory
 * @data_fifo_in_pipe_mem: flag indicating if data FIFO uses pipe memory
 * @desc_fifo_pipe_mem_ofst: descriptors FIFO pipe memory offset
 * @data_fifo_pipe_mem_ofst: data FIFO pipe memory offset
 * @desc_fifo_client_allocated: if descriptors FIFO was allocated by a client
 * @data_fifo_client_allocated: if data FIFO was allocated by a client
 * @skip_ep_cfg: boolean field that determines if EP should be configured
 *  by IPA driver
 * @keep_ipa_awake: when true, IPA will not be clock gated
 * @disconnect_in_progress: Indicates client disconnect in progress.
 * @qmi_request_sent: Indicates whether QMI request to enable clear data path
 *					request is sent or not.
 */
struct ipa3_ep_context {
	int valid;
	enum ipa_client_type client;
	struct sps_pipe *ep_hdl;
	unsigned long gsi_chan_hdl;
	unsigned long gsi_evt_ring_hdl;
	struct ipa_gsi_ep_mem_info gsi_mem_info;
	union __packed gsi_channel_scratch chan_scratch;
	bool bytes_xfered_valid;
	u16 bytes_xfered;
	dma_addr_t phys_base;
	struct ipa_ep_cfg cfg;
	struct ipa_ep_cfg_holb holb;
	struct ipahal_reg_ep_cfg_status status;
	u32 dst_pipe_index;
	u32 rt_tbl_idx;
	struct sps_connect connect;
	void *priv;
	void (*client_notify)(void *priv, enum ipa_dp_evt_type evt,
		       unsigned long data);
	bool desc_fifo_in_pipe_mem;
	bool data_fifo_in_pipe_mem;
	u32 desc_fifo_pipe_mem_ofst;
	u32 data_fifo_pipe_mem_ofst;
	bool desc_fifo_client_allocated;
	bool data_fifo_client_allocated;
	atomic_t avail_fifo_desc;
	u32 dflt_flt4_rule_hdl;
	u32 dflt_flt6_rule_hdl;
	bool skip_ep_cfg;
	bool keep_ipa_awake;
	struct ipa3_wlan_stats wstats;
	u32 uc_offload_state;
	bool disconnect_in_progress;
	u32 qmi_request_sent;
	bool ep_delay_set;

	/* sys MUST be the last element of this struct */
	struct ipa3_sys_context *sys;
};

/**
 * ipa_usb_xdci_chan_params - xDCI channel related properties
 *
 * @ipa_ep_cfg:          IPA EP configuration
 * @client:              type of "client"
 * @priv:                callback cookie
 * @notify:              callback
 *           priv - callback cookie evt - type of event data - data relevant
 *           to event.  May not be valid. See event_type enum for valid
 *           cases.
 * @skip_ep_cfg:         boolean field that determines if EP should be
 *                       configured by IPA driver
 * @keep_ipa_awake:      when true, IPA will not be clock gated
 * @evt_ring_params:     parameters for the channel's event ring
 * @evt_scratch:         parameters for the channel's event ring scratch
 * @chan_params:         parameters for the channel
 * @chan_scratch:        parameters for the channel's scratch
 *
 */
struct ipa_request_gsi_channel_params {
	struct ipa_ep_cfg ipa_ep_cfg;
	enum ipa_client_type client;
	void *priv;
	ipa_notify_cb notify;
	bool skip_ep_cfg;
	bool keep_ipa_awake;
	struct gsi_evt_ring_props evt_ring_params;
	union __packed gsi_evt_scratch evt_scratch;
	struct gsi_chan_props chan_params;
	union __packed gsi_channel_scratch chan_scratch;
};

enum ipa3_sys_pipe_policy {
	IPA_POLICY_INTR_MODE,
	IPA_POLICY_NOINTR_MODE,
	IPA_POLICY_INTR_POLL_MODE,
};

struct ipa3_repl_ctx {
	struct ipa3_rx_pkt_wrapper **cache;
	atomic_t head_idx;
	atomic_t tail_idx;
	u32 capacity;
};

/**
 * struct ipa3_sys_context - IPA endpoint context for system to BAM pipes
 * @head_desc_list: header descriptors list
 * @len: the size of the above list
 * @spinlock: protects the list and its size
 * @event: used to request CALLBACK mode from SPS driver
 * @ep: IPA EP context
 *
 * IPA context specific to the system-bam pipes a.k.a LAN IN/OUT and WAN
 */
struct ipa3_sys_context {
	u32 len;
	u32 len_pending_xfer;
	struct sps_register_event event;
	atomic_t curr_polling_state;
	struct delayed_work switch_to_intr_work;
	enum ipa3_sys_pipe_policy policy;
	bool use_comm_evt_ring;
	int (*pyld_hdlr)(struct sk_buff *skb, struct ipa3_sys_context *sys);
	struct sk_buff * (*get_skb)(unsigned int len, gfp_t flags);
	void (*free_skb)(struct sk_buff *skb);
	void (*free_rx_wrapper)(struct ipa3_rx_pkt_wrapper *rk_pkt);
	u32 rx_buff_sz;
	u32 rx_pool_sz;
	struct sk_buff *prev_skb;
	unsigned int len_rem;
	unsigned int len_pad;
	unsigned int len_partial;
	bool drop_packet;
	struct work_struct work;
	void (*sps_callback)(struct sps_event_notify *notify);
	enum sps_option sps_option;
	struct delayed_work replenish_rx_work;
	struct work_struct repl_work;
	void (*repl_hdlr)(struct ipa3_sys_context *sys);
	struct ipa3_repl_ctx repl;

	/* ordering is important - mutable fields go above */
	struct ipa3_ep_context *ep;
	struct list_head head_desc_list;
	struct list_head rcycl_list;
	spinlock_t spinlock;
	struct hrtimer db_timer;
	struct workqueue_struct *wq;
	struct workqueue_struct *repl_wq;
	struct ipa3_status_stats *status_stat;
	/* ordering is important - other immutable fields go below */
};

/**
 * enum ipa3_desc_type - IPA decriptors type
 *
 * IPA decriptors type, IPA supports DD and ICD but no CD
 */
enum ipa3_desc_type {
	IPA_DATA_DESC,
	IPA_DATA_DESC_SKB,
	IPA_DATA_DESC_SKB_PAGED,
	IPA_IMM_CMD_DESC,
};

/**
 * struct ipa3_tx_pkt_wrapper - IPA Tx packet wrapper
 * @type: specify if this packet is for the skb or immediate command
 * @mem: memory buffer used by this Tx packet
 * @work: work struct for current Tx packet
 * @link: linked to the wrappers on that pipe
 * @callback: IPA client provided callback
 * @user1: cookie1 for above callback
 * @user2: cookie2 for above callback
 * @sys: corresponding IPA sys context
 * @mult: valid only for first of a "multiple" transfer,
 * holds info for the "sps_transfer" buffer
 * @cnt: 1 for single transfers,
 * >1 and <0xFFFF for first of a "multiple" transfer,
 * 0xFFFF for last desc, 0 for rest of "multiple' transfer
 * @bounce: va of bounce buffer
 * @unmap_dma: in case this is true, the buffer will not be dma unmapped
 *
 * This struct can wrap both data packet and immediate command packet.
 */
struct ipa3_tx_pkt_wrapper {
	enum ipa3_desc_type type;
	struct ipa_mem_buffer mem;
	struct work_struct work;
	struct list_head link;
	void (*callback)(void *user1, int user2);
	void *user1;
	int user2;
	struct ipa3_sys_context *sys;
	struct ipa_mem_buffer mult;
	u32 cnt;
	void *bounce;
	bool no_unmap_dma;
};

/**
 * struct ipa3_dma_xfer_wrapper - IPADMA transfer descr wrapper
 * @phys_addr_src: physical address of the source data to copy
 * @phys_addr_dest: physical address to store the copied data
 * @len: len in bytes to copy
 * @link: linked to the wrappers list on the proper(sync/async) cons pipe
 * @xfer_done: completion object for sync_memcpy completion
 * @callback: IPADMA client provided completion callback
 * @user1: cookie1 for above callback
 *
 * This struct can wrap both sync and async memcpy transfers descriptors.
 */
struct ipa3_dma_xfer_wrapper {
	u64 phys_addr_src;
	u64 phys_addr_dest;
	u16 len;
	struct list_head link;
	struct completion xfer_done;
	void (*callback)(void *user1);
	void *user1;
};

/**
 * struct ipa3_desc - IPA descriptor
 * @type: skb or immediate command or plain old data
 * @pyld: points to skb
 * @frag: points to paged fragment
 * or kmalloc'ed immediate command parameters/plain old data
 * @dma_address: dma mapped address of pyld
 * @dma_address_valid: valid field for dma_address
 * @len: length of the pyld
 * @opcode: for immediate commands
 * @callback: IPA client provided completion callback
 * @user1: cookie1 for above callback
 * @user2: cookie2 for above callback
 * @xfer_done: completion object for sync completion
 * @skip_db_ring: specifies whether GSI doorbell should not be rang
 */
struct ipa3_desc {
	enum ipa3_desc_type type;
	void *pyld;
	skb_frag_t *frag;
	dma_addr_t dma_address;
	bool dma_address_valid;
	u16 len;
	u16 opcode;
	void (*callback)(void *user1, int user2);
	void *user1;
	int user2;
	struct completion xfer_done;
	bool skip_db_ring;
};

/**
 * struct ipa3_rx_pkt_wrapper - IPA Rx packet wrapper
 * @skb: skb
 * @dma_address: DMA address of this Rx packet
 * @link: linked to the Rx packets on that pipe
 * @len: how many bytes are copied into skb's flat buffer
 */
struct ipa3_rx_pkt_wrapper {
	struct list_head link;
	struct ipa_rx_data data;
	u32 len;
	struct work_struct work;
	struct ipa3_sys_context *sys;
};

/**
 * struct ipa3_nat_mem - IPA NAT memory description
 * @class: pointer to the struct class
 * @dev: the dev_t of the device
 * @cdev: cdev of the device
 * @dev_num: device number
 * @vaddr: virtual address
 * @dma_handle: DMA handle
 * @size: NAT memory size
 * @is_mapped: flag indicating if NAT memory is mapped
 * @is_sys_mem: flag indicating if NAT memory is sys memory
 * @is_dev_init: flag indicating if NAT device is initialized
 * @lock: NAT memory mutex
 * @nat_base_address: nat table virutal address
 * @ipv4_rules_addr: base nat table address
 * @ipv4_expansion_rules_addr: expansion table address
 * @index_table_addr: index table address
 * @index_table_expansion_addr: index expansion table address
 * @size_base_tables: base table size
 * @size_expansion_tables: expansion table size
 * @public_ip_addr: ip address of nat table
 */
struct ipa3_nat_mem {
	struct class *class;
	struct device *dev;
	struct cdev cdev;
	dev_t dev_num;
	void *vaddr;
	dma_addr_t dma_handle;
	size_t size;
	bool is_mapped;
	bool is_sys_mem;
	bool is_dev_init;
	bool is_dev;
	struct mutex lock;
	void *nat_base_address;
	char *ipv4_rules_addr;
	char *ipv4_expansion_rules_addr;
	char *index_table_addr;
	char *index_table_expansion_addr;
	u32 size_base_tables;
	u32 size_expansion_tables;
	u32 public_ip_addr;
	void *tmp_vaddr;
	dma_addr_t tmp_dma_handle;
	bool is_tmp_mem;
};

/**
 * enum ipa3_hw_mode - IPA hardware mode
 * @IPA_HW_Normal: Regular IPA hardware
 * @IPA_HW_Virtual: IPA hardware supporting virtual memory allocation
 * @IPA_HW_PCIE: IPA hardware supporting memory allocation over PCIE Bridge
 */
enum ipa3_hw_mode {
	IPA_HW_MODE_NORMAL  = 0,
	IPA_HW_MODE_VIRTUAL = 1,
	IPA_HW_MODE_PCIE    = 2
};

enum ipa3_config_this_ep {
	IPA_CONFIGURE_THIS_EP,
	IPA_DO_NOT_CONFIGURE_THIS_EP,
};

struct ipa3_stats {
	u32 tx_sw_pkts;
	u32 tx_hw_pkts;
	u32 rx_pkts;
	u32 rx_excp_pkts[IPAHAL_PKT_STATUS_EXCEPTION_MAX];
	u32 rx_repl_repost;
	u32 tx_pkts_compl;
	u32 rx_q_len;
	u32 msg_w[IPA_EVENT_MAX_NUM];
	u32 msg_r[IPA_EVENT_MAX_NUM];
	u32 stat_compl;
	u32 aggr_close;
	u32 wan_aggr_close;
	u32 wan_rx_empty;
	u32 wan_repl_rx_empty;
	u32 lan_rx_empty;
	u32 lan_repl_rx_empty;
	u32 flow_enable;
	u32 flow_disable;
	u32 tx_non_linear;
};

struct ipa3_active_clients {
	struct mutex mutex;
	spinlock_t spinlock;
	bool mutex_locked;
	int cnt;
};

struct ipa3_wakelock_ref_cnt {
	spinlock_t spinlock;
	int cnt;
};

struct ipa3_tag_completion {
	struct completion comp;
	atomic_t cnt;
};

/**
 * struct ipa3_debugfs_rt_entry - IPA routing table entry for debugfs
 * @eq_attrib: equation attributes for the rule
 * @retain_hdr: retain header when hit this rule
 * @prio: rule 10bit priority which defines the order of the rule
 * @rule_id: rule 10bit ID to be returned in packet status
 * @dst: destination endpoint
 * @hdr_ofset: header offset to be added
 * @system: rule resides in system memory
 * @is_proc_ctx: indicates whether the rules points to proc_ctx or header
 */
struct ipa3_debugfs_rt_entry {
	struct ipa_ipfltri_rule_eq eq_attrib;
	uint8_t retain_hdr;
	u16 prio;
	u16 rule_id;
	u8 dst;
	u8 hdr_ofset;
	u8 system;
	u8 is_proc_ctx;
};

struct ipa3_controller;

/**
 * struct ipa3_uc_hdlrs - IPA uC callback functions
 * @ipa_uc_loaded_hdlr: Function handler when uC is loaded
 * @ipa_uc_event_hdlr: Event handler function
 * @ipa3_uc_response_hdlr: Response handler function
 * @ipa_uc_event_log_info_hdlr: Log event handler function
 */
struct ipa3_uc_hdlrs {
	void (*ipa_uc_loaded_hdlr)(void);
	void (*ipa_uc_event_hdlr)
		(struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio);
	int (*ipa3_uc_response_hdlr)
		(struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio,
		u32 *uc_status);
	void (*ipa_uc_event_log_info_hdlr)
		(struct IpaHwEventLogInfoData_t *uc_event_top_mmio);
};

/**
 * enum ipa3_hw_flags - flags which defines the behavior of HW
 *
 * @IPA_HW_FLAG_HALT_SYSTEM_ON_ASSERT_FAILURE: Halt system in case of assert
 *	failure.
 * @IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_ERORR: Channel error would be reported
 *	in the event ring only. No event to CPU.
 * @IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_WAKE_UP: No need to report event
 *	IPA_HW_2_CPU_EVENT_MHI_WAKE_UP_REQUEST
 * @IPA_HW_FLAG_WORK_OVER_DDR: Perform all transaction to external addresses by
 *	QMB (avoid memcpy)
 * @IPA_HW_FLAG_NO_REPORT_OOB: If set do not report that the device is OOB in
 *	IN Channel
 * @IPA_HW_FLAG_NO_REPORT_DB_MODE: If set, do not report that the device is
 *	entering a mode where it expects a doorbell to be rung for OUT Channel
 * @IPA_HW_FLAG_NO_START_OOB_TIMER
 */
enum ipa3_hw_flags {
	IPA_HW_FLAG_HALT_SYSTEM_ON_ASSERT_FAILURE	= 0x01,
	IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_ERORR		= 0x02,
	IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_WAKE_UP	= 0x04,
	IPA_HW_FLAG_WORK_OVER_DDR			= 0x08,
	IPA_HW_FLAG_NO_REPORT_OOB			= 0x10,
	IPA_HW_FLAG_NO_REPORT_DB_MODE			= 0x20,
	IPA_HW_FLAG_NO_START_OOB_TIMER			= 0x40
};

/**
 * struct ipa3_uc_ctx - IPA uC context
 * @uc_inited: Indicates if uC interface has been initialized
 * @uc_loaded: Indicates if uC has loaded
 * @uc_failed: Indicates if uC has failed / returned an error
 * @uc_lock: uC interface lock to allow only one uC interaction at a time
 * @uc_spinlock: same as uc_lock but for irq contexts
 * @uc_completation: Completion mechanism to wait for uC commands
 * @uc_sram_mmio: Pointer to uC mapped memory
 * @pending_cmd: The last command sent waiting to be ACKed
 * @uc_status: The last status provided by the uC
 * @uc_error_type: error type from uC error event
 * @uc_error_timestamp: tag timer sampled after uC crashed
 */
struct ipa3_uc_ctx {
	bool uc_inited;
	bool uc_loaded;
	bool uc_failed;
	struct mutex uc_lock;
	spinlock_t uc_spinlock;
	struct completion uc_completion;
	struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio;
	struct IpaHwEventLogInfoData_t *uc_event_top_mmio;
	u32 uc_event_top_ofst;
	u32 pending_cmd;
	u32 uc_status;
	u32 uc_error_type;
	u32 uc_error_timestamp;
	phys_addr_t rdy_ring_base_pa;
	phys_addr_t rdy_ring_rp_pa;
	u32 rdy_ring_size;
	phys_addr_t rdy_comp_ring_base_pa;
	phys_addr_t rdy_comp_ring_wp_pa;
	u32 rdy_comp_ring_size;
	u32 *rdy_ring_rp_va;
	u32 *rdy_comp_ring_wp_va;
};

/**
 * struct ipa3_uc_wdi_ctx
 * @wdi_uc_top_ofst:
 * @wdi_uc_top_mmio:
 * @wdi_uc_stats_ofst:
 * @wdi_uc_stats_mmio:
 */
struct ipa3_uc_wdi_ctx {
	/* WDI specific fields */
	u32 wdi_uc_stats_ofst;
	struct IpaHwStatsWDIInfoData_t *wdi_uc_stats_mmio;
	void *priv;
	ipa_uc_ready_cb uc_ready_cb;
};

/**
 * struct ipa3_transport_pm - transport power management related members
 * @lock: lock for ensuring atomic operations
 * @res_granted: true if SPS requested IPA resource and IPA granted it
 * @res_rel_in_prog: true if releasing IPA resource is in progress
 * @transport_pm_mutex: Mutex to protect the transport_pm functionality.
 */
struct ipa3_transport_pm {
	spinlock_t lock;
	bool res_granted;
	bool res_rel_in_prog;
	atomic_t dec_clients;
	atomic_t eot_activity;
	struct mutex transport_pm_mutex;
};

/**
 * struct ipa3cm_client_info - the client-info indicated from IPACM
 * @ipacm_client_enum: the enum to indicate tether-client
 * @ipacm_client_uplink: the bool to indicate pipe for uplink
 */
struct ipa3cm_client_info {
	enum ipacm_client_enum client_enum;
	bool uplink;
};

struct ipa3_smp2p_info {
	u32 out_base_id;
	u32 in_base_id;
	bool ipa_clk_on;
	bool res_sent;
};

/**
 * struct ipa3_ready_cb_info - A list of all the registrations
 *  for an indication of IPA driver readiness
 *
 * @link: linked list link
 * @ready_cb: callback
 * @user_data: User data
 *
 */
struct ipa3_ready_cb_info {
	struct list_head link;
	ipa_ready_cb ready_cb;
	void *user_data;
};

struct ipa_dma_task_info {
	struct ipa_mem_buffer mem;
	struct ipahal_imm_cmd_pyld *cmd_pyld;
};

struct ipa_cne_evt {
	struct ipa_wan_msg wan_msg;
	struct ipa_msg_meta msg_meta;
};

/**
 * struct ipa3_context - IPA context
 * @class: pointer to the struct class
 * @dev_num: device number
 * @dev: the dev_t of the device
 * @cdev: cdev of the device
 * @bam_handle: IPA driver's BAM handle
 * @ep: list of all end points
 * @skip_ep_cfg_shadow: state to update filter table correctly across
  power-save
 * @ep_flt_bitmap: End-points supporting filtering bitmap
 * @ep_flt_num: End-points supporting filtering number
 * @resume_on_connect: resume ep on ipa3_connect
 * @flt_tbl: list of all IPA filter tables
 * @mode: IPA operating mode
 * @mmio: iomem
 * @ipa_wrapper_base: IPA wrapper base address
 * @hdr_tbl: IPA header table
 * @hdr_proc_ctx_tbl: IPA processing context table
 * @rt_tbl_set: list of routing tables each of which is a list of rules
 * @reap_rt_tbl_set: list of sys mem routing tables waiting to be reaped
 * @flt_rule_cache: filter rule cache
 * @rt_rule_cache: routing rule cache
 * @hdr_cache: header cache
 * @hdr_offset_cache: header offset cache
 * @hdr_proc_ctx_cache: processing context cache
 * @hdr_proc_ctx_offset_cache: processing context offset cache
 * @rt_tbl_cache: routing table cache
 * @tx_pkt_wrapper_cache: Tx packets cache
 * @rx_pkt_wrapper_cache: Rx packets cache
 * @rt_idx_bitmap: routing table index bitmap
 * @lock: this does NOT protect the linked lists within ipa3_sys_context
 * @smem_sz: shared memory size available for SW use starting
 *  from non-restricted bytes
 * @smem_restricted_bytes: the bytes that SW should not use in the shared mem
 * @nat_mem: NAT memory
 * @excp_hdr_hdl: exception header handle
 * @dflt_v4_rt_rule_hdl: default v4 routing rule handle
 * @dflt_v6_rt_rule_hdl: default v6 routing rule handle
 * @aggregation_type: aggregation type used on USB client endpoint
 * @aggregation_byte_limit: aggregation byte limit used on USB client endpoint
 * @aggregation_time_limit: aggregation time limit used on USB client endpoint
 * @hdr_tbl_lcl: where hdr tbl resides 1-local, 0-system
 * @hdr_proc_ctx_tbl_lcl: where proc_ctx tbl resides true-local, false-system
 * @hdr_mem: header memory
 * @hdr_proc_ctx_mem: processing context memory
 * @ip4_rt_tbl_lcl: where ip4 rt tables reside 1-local; 0-system
 * @ip6_rt_tbl_lcl: where ip6 rt tables reside 1-local; 0-system
 * @ip4_flt_tbl_lcl: where ip4 flt tables reside 1-local; 0-system
 * @ip6_flt_tbl_lcl: where ip6 flt tables reside 1-local; 0-system
 * @empty_rt_tbl_mem: empty routing tables memory
 * @power_mgmt_wq: workqueue for power management
 * @transport_power_mgmt_wq: workqueue transport related power management
 * @tag_process_before_gating: indicates whether to start tag process before
 *  gating IPA clocks
 * @transport_pm: transport power management related information
 * @disconnect_lock: protects LAN_CONS packet receive notification CB
 * @pipe_mem_pool: pipe memory pool
 * @dma_pool: special purpose DMA pool
 * @ipa3_active_clients: structure for reference counting connected IPA clients
 * @ipa_hw_type: type of IPA HW type (e.g. IPA 1.0, IPA 1.1 etc')
 * @ipa3_hw_mode: mode of IPA HW mode (e.g. Normal, Virtual or over PCIe)
 * @use_ipa_teth_bridge: use tethering bridge driver
 * @ipa_bam_remote_mode: ipa bam is in remote mode
 * @modem_cfg_emb_pipe_flt: modem configure embedded pipe filtering rules
 * @logbuf: ipc log buffer for high priority messages
 * @logbuf_low: ipc log buffer for low priority messages
 * @ipa_wdi2: using wdi-2.0
 * @use_64_bit_dma_mask: using 64bits dma mask
 * @ipa_bus_hdl: msm driver handle for the data path bus
 * @ctrl: holds the core specific operations based on
 *  core version (vtable like)
 * @enable_clock_scaling: clock scaling is enabled ?
 * @curr_ipa_clk_rate: ipa3_clk current rate
 * @wcstats: wlan common buffer stats
 * @uc_ctx: uC interface context
 * @uc_wdi_ctx: WDI specific fields for uC interface
 * @ipa_num_pipes: The number of pipes used by IPA HW
 * @skip_uc_pipe_reset: Indicates whether pipe reset via uC needs to be avoided
 * @ipa_client_apps_wan_cons_agg_gro: RMNET_IOCTL_INGRESS_FORMAT_AGG_DATA
 * @apply_rg10_wa: Indicates whether to use register group 10 workaround
 * @gsi_ch20_wa: Indicates whether to apply GSI physical channel 20 workaround
 * @w_lock: Indicates the wakeup source.
 * @wakelock_ref_cnt: Indicates the number of times wakelock is acquired
 * @ipa_initialization_complete: Indicates that IPA is fully initialized
 * @ipa_ready_cb_list: A list of all the clients who require a CB when IPA
 *  driver is ready/initialized.
 * @init_completion_obj: Completion object to be used in case IPA driver hasn't
 *  finished initializing. Example of use - IOCTLs to /dev/ipa
 * IPA context - holds all relevant info about IPA driver and its state
 */
struct ipa3_context {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	unsigned long bam_handle;
	struct ipa3_ep_context ep[IPA3_MAX_NUM_PIPES];
	bool skip_ep_cfg_shadow[IPA3_MAX_NUM_PIPES];
	u32 ep_flt_bitmap;
	u32 ep_flt_num;
	bool resume_on_connect[IPA_CLIENT_MAX];
	struct ipa3_flt_tbl flt_tbl[IPA3_MAX_NUM_PIPES][IPA_IP_MAX];
	void __iomem *mmio;
	u32 ipa_wrapper_base;
	u32 ipa_wrapper_size;
	struct ipa3_hdr_tbl hdr_tbl;
	struct ipa3_hdr_proc_ctx_tbl hdr_proc_ctx_tbl;
	struct ipa3_rt_tbl_set rt_tbl_set[IPA_IP_MAX];
	struct ipa3_rt_tbl_set reap_rt_tbl_set[IPA_IP_MAX];
	struct kmem_cache *flt_rule_cache;
	struct kmem_cache *rt_rule_cache;
	struct kmem_cache *hdr_cache;
	struct kmem_cache *hdr_offset_cache;
	struct kmem_cache *hdr_proc_ctx_cache;
	struct kmem_cache *hdr_proc_ctx_offset_cache;
	struct kmem_cache *rt_tbl_cache;
	struct kmem_cache *tx_pkt_wrapper_cache;
	struct kmem_cache *rx_pkt_wrapper_cache;
	unsigned long rt_idx_bitmap[IPA_IP_MAX];
	struct mutex lock;
	u16 smem_sz;
	u16 smem_restricted_bytes;
	u16 smem_reqd_sz;
	struct ipa3_nat_mem nat_mem;
	u32 excp_hdr_hdl;
	u32 dflt_v4_rt_rule_hdl;
	u32 dflt_v6_rt_rule_hdl;
	uint aggregation_type;
	uint aggregation_byte_limit;
	uint aggregation_time_limit;
	bool hdr_tbl_lcl;
	bool hdr_proc_ctx_tbl_lcl;
	struct ipa_mem_buffer hdr_mem;
	struct ipa_mem_buffer hdr_proc_ctx_mem;
	bool ip4_rt_tbl_hash_lcl;
	bool ip4_rt_tbl_nhash_lcl;
	bool ip6_rt_tbl_hash_lcl;
	bool ip6_rt_tbl_nhash_lcl;
	bool ip4_flt_tbl_hash_lcl;
	bool ip4_flt_tbl_nhash_lcl;
	bool ip6_flt_tbl_hash_lcl;
	bool ip6_flt_tbl_nhash_lcl;
	struct ipa_mem_buffer empty_rt_tbl_mem;
	struct gen_pool *pipe_mem_pool;
	struct dma_pool *dma_pool;
	struct ipa3_active_clients ipa3_active_clients;
	struct ipa3_active_clients_log_ctx ipa3_active_clients_logging;
	struct workqueue_struct *power_mgmt_wq;
	struct workqueue_struct *transport_power_mgmt_wq;
	bool tag_process_before_gating;
	struct ipa3_transport_pm transport_pm;
	unsigned long gsi_evt_comm_hdl;
	u32 gsi_evt_comm_ring_rem;
	u32 clnt_hdl_cmd;
	u32 clnt_hdl_data_in;
	u32 clnt_hdl_data_out;
	spinlock_t disconnect_lock;
	u8 a5_pipe_index;
	struct list_head intf_list;
	struct list_head msg_list;
	struct list_head pull_msg_list;
	struct mutex msg_lock;
	wait_queue_head_t msg_waitq;
	enum ipa_hw_type ipa_hw_type;
	enum ipa3_hw_mode ipa3_hw_mode;
	bool ipa_config_is_mhi;
	bool use_ipa_teth_bridge;
	bool ipa_bam_remote_mode;
	bool modem_cfg_emb_pipe_flt;
	bool ipa_wdi2;
	bool use_64_bit_dma_mask;
	/* featurize if memory footprint becomes a concern */
	struct ipa3_stats stats;
	void *smem_pipe_mem;
	void *logbuf;
	void *logbuf_low;
	u32 ipa_bus_hdl;
	struct ipa3_controller *ctrl;
	struct idr ipa_idr;
	struct device *pdev;
	struct device *uc_pdev;
	spinlock_t idr_lock;
	u32 enable_clock_scaling;
	u32 curr_ipa_clk_rate;
	bool q6_proxy_clk_vote_valid;
	struct mutex q6_proxy_clk_vote_mutex;
	u32 ipa_num_pipes;
	dma_addr_t pkt_init_imm[IPA3_MAX_NUM_PIPES];

	struct ipa3_wlan_comm_memb wc_memb;

	struct ipa3_uc_ctx uc_ctx;

	struct ipa3_uc_wdi_ctx uc_wdi_ctx;
	struct ipa3_uc_ntn_ctx uc_ntn_ctx;
	u32 wan_rx_ring_size;
	u32 lan_rx_ring_size;
	bool skip_uc_pipe_reset;
	enum ipa_transport_type transport_prototype;
	unsigned long gsi_dev_hdl;
	u32 ee;
	bool apply_rg10_wa;
	bool gsi_ch20_wa;
	bool smmu_present;
	bool smmu_s1_bypass;
	unsigned long peer_bam_iova;
	phys_addr_t peer_bam_pa;
	u32 peer_bam_map_size;
	unsigned long peer_bam_dev;
	u32 peer_bam_map_cnt;
	u32 wdi_map_cnt;
	struct wakeup_source w_lock;
	struct ipa3_wakelock_ref_cnt wakelock_ref_cnt;
	/* RMNET_IOCTL_INGRESS_FORMAT_AGG_DATA */
	bool ipa_client_apps_wan_cons_agg_gro;
	/* M-release support to know client pipes */
	struct ipa3cm_client_info ipacm_client[IPA3_MAX_NUM_PIPES];
	bool tethered_flow_control;
	bool ipa_initialization_complete;
	struct list_head ipa_ready_cb_list;
	struct completion init_completion_obj;
	struct completion uc_loaded_completion_obj;
	struct ipa3_smp2p_info smp2p_info;
	u32 ipa_tz_unlock_reg_num;
	struct ipa_tz_unlock_reg_info *ipa_tz_unlock_reg;
	struct ipa_dma_task_info dma_task_info;
	struct ipa_cne_evt ipa_cne_evt_req_cache[IPA_MAX_NUM_REQ_CACHE];
	int num_ipa_cne_evt_req;
	struct mutex ipa_cne_evt_lock;
};

/**
 * enum ipa3_pipe_mem_type - IPA pipe memory type
 * @IPA_SPS_PIPE_MEM: Default, SPS dedicated pipe memory
 * @IPA_PRIVATE_MEM: IPA's private memory
 * @IPA_SYSTEM_MEM: System RAM, requires allocation
 */
enum ipa3_pipe_mem_type {
	IPA_SPS_PIPE_MEM = 0,
	IPA_PRIVATE_MEM  = 1,
	IPA_SYSTEM_MEM   = 2,
};

struct ipa3_plat_drv_res {
	bool use_ipa_teth_bridge;
	u32 ipa_mem_base;
	u32 ipa_mem_size;
	u32 transport_mem_base;
	u32 transport_mem_size;
	u32 ipa_irq;
	u32 transport_irq;
	u32 ipa_pipe_mem_start_ofst;
	u32 ipa_pipe_mem_size;
	enum ipa_hw_type ipa_hw_type;
	enum ipa3_hw_mode ipa3_hw_mode;
	u32 ee;
	bool ipa_bam_remote_mode;
	bool modem_cfg_emb_pipe_flt;
	bool ipa_wdi2;
	u32 default_threshold[IPA_PM_THRESHOLD_MAX];
	bool use_64_bit_dma_mask;
	u32 wan_rx_ring_size;
	u32 lan_rx_ring_size;
	bool skip_uc_pipe_reset;
	enum ipa_transport_type transport_prototype;
	bool apply_rg10_wa;
	bool gsi_ch20_wa;
	bool tethered_flow_control;
	u32 ipa_tz_unlock_reg_num;
	struct ipa_tz_unlock_reg_info *ipa_tz_unlock_reg;
};

struct ipa3_mem_partition {
	u16 ofst_start;
	u16 nat_ofst;
	u16 nat_size;
	u16 v4_flt_hash_ofst;
	u16 v4_flt_hash_size;
	u16 v4_flt_hash_size_ddr;
	u16 v4_flt_nhash_ofst;
	u16 v4_flt_nhash_size;
	u16 v4_flt_nhash_size_ddr;
	u16 v6_flt_hash_ofst;
	u16 v6_flt_hash_size;
	u16 v6_flt_hash_size_ddr;
	u16 v6_flt_nhash_ofst;
	u16 v6_flt_nhash_size;
	u16 v6_flt_nhash_size_ddr;
	u16 v4_rt_num_index;
	u16 v4_modem_rt_index_lo;
	u16 v4_modem_rt_index_hi;
	u16 v4_apps_rt_index_lo;
	u16 v4_apps_rt_index_hi;
	u16 v4_rt_hash_ofst;
	u16 v4_rt_hash_size;
	u16 v4_rt_hash_size_ddr;
	u16 v4_rt_nhash_ofst;
	u16 v4_rt_nhash_size;
	u16 v4_rt_nhash_size_ddr;
	u16 v6_rt_num_index;
	u16 v6_modem_rt_index_lo;
	u16 v6_modem_rt_index_hi;
	u16 v6_apps_rt_index_lo;
	u16 v6_apps_rt_index_hi;
	u16 v6_rt_hash_ofst;
	u16 v6_rt_hash_size;
	u16 v6_rt_hash_size_ddr;
	u16 v6_rt_nhash_ofst;
	u16 v6_rt_nhash_size;
	u16 v6_rt_nhash_size_ddr;
	u16 modem_hdr_ofst;
	u16 modem_hdr_size;
	u16 apps_hdr_ofst;
	u16 apps_hdr_size;
	u16 apps_hdr_size_ddr;
	u16 modem_hdr_proc_ctx_ofst;
	u16 modem_hdr_proc_ctx_size;
	u16 apps_hdr_proc_ctx_ofst;
	u16 apps_hdr_proc_ctx_size;
	u16 apps_hdr_proc_ctx_size_ddr;
	u16 modem_comp_decomp_ofst;
	u16 modem_comp_decomp_size;
	u16 modem_ofst;
	u16 modem_size;
	u16 uc_event_ring_ofst;
	u16 uc_event_ring_size;
	u16 apps_v4_flt_hash_ofst;
	u16 apps_v4_flt_hash_size;
	u16 apps_v4_flt_nhash_ofst;
	u16 apps_v4_flt_nhash_size;
	u16 apps_v6_flt_hash_ofst;
	u16 apps_v6_flt_hash_size;
	u16 apps_v6_flt_nhash_ofst;
	u16 apps_v6_flt_nhash_size;
	u16 uc_info_ofst;
	u16 uc_info_size;
	u16 end_ofst;
	u16 apps_v4_rt_hash_ofst;
	u16 apps_v4_rt_hash_size;
	u16 apps_v4_rt_nhash_ofst;
	u16 apps_v4_rt_nhash_size;
	u16 apps_v6_rt_hash_ofst;
	u16 apps_v6_rt_hash_size;
	u16 apps_v6_rt_nhash_ofst;
	u16 apps_v6_rt_nhash_size;
};

struct ipa3_controller {
	struct ipa3_mem_partition mem_partition;
	u32 ipa_clk_rate_turbo;
	u32 ipa_clk_rate_nominal;
	u32 ipa_clk_rate_svs;
	u32 clock_scaling_bw_threshold_turbo;
	u32 clock_scaling_bw_threshold_nominal;
	u32 ipa_reg_base_ofst;
	u32 max_holb_tmr_val;
	void (*ipa_sram_read_settings)(void);
	int (*ipa_init_sram)(void);
	int (*ipa_init_hdr)(void);
	int (*ipa_init_rt4)(void);
	int (*ipa_init_rt6)(void);
	int (*ipa_init_flt4)(void);
	int (*ipa_init_flt6)(void);
	int (*ipa3_read_ep_reg)(char *buff, int max_len, int pipe);
	int (*ipa3_commit_flt)(enum ipa_ip_type ip);
	int (*ipa3_commit_rt)(enum ipa_ip_type ip);
	int (*ipa_generate_rt_hw_rule)(enum ipa_ip_type ip,
		struct ipa3_rt_entry *entry, u8 *buf);
	int (*ipa3_commit_hdr)(void);
	void (*ipa3_enable_clks)(void);
	void (*ipa3_disable_clks)(void);
	struct msm_bus_scale_pdata *msm_bus_data_ptr;
};

extern struct ipa3_context *ipa3_ctx;

/* public APIs */
/*
 * Connect / Disconnect
 */
int ipa3_connect(const struct ipa_connect_params *in,
		struct ipa_sps_params *sps,
		u32 *clnt_hdl);
int ipa3_disconnect(u32 clnt_hdl);

/* Generic GSI channels functions */
int ipa3_request_gsi_channel(struct ipa_request_gsi_channel_params *params,
			     struct ipa_req_chan_out_params *out_params);

int ipa3_release_gsi_channel(u32 clnt_hdl);

int ipa3_start_gsi_channel(u32 clnt_hdl);

int ipa3_stop_gsi_channel(u32 clnt_hdl);

int ipa3_reset_gsi_channel(u32 clnt_hdl);

int ipa3_reset_gsi_event_ring(u32 clnt_hdl);

/* Specific xDCI channels functions */
int ipa3_set_usb_max_packet_size(
	enum ipa_usb_max_usb_packet_size usb_max_packet_size);

int ipa3_xdci_start(u32 clnt_hdl, u8 xferrscidx, bool xferrscidx_valid);

int ipa3_xdci_connect(u32 clnt_hdl);

int ipa3_xdci_disconnect(u32 clnt_hdl, bool should_force_clear, u32 qmi_req_id);

void ipa3_xdci_ep_delay_rm(u32 clnt_hdl);

int ipa3_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	bool should_force_clear, u32 qmi_req_id, bool is_dpl);

int ipa3_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl, bool is_dpl);

/*
 * Resume / Suspend
 */
int ipa3_reset_endpoint(u32 clnt_hdl);

/*
 * Remove ep delay
 */
int ipa3_clear_endpoint_delay(u32 clnt_hdl);

/*
 * Configuration
 */
int ipa3_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg);

int ipa3_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ipa_ep_cfg);

int ipa3_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ipa_ep_cfg);

int ipa3_cfg_ep_hdr_ext(u32 clnt_hdl,
			const struct ipa_ep_cfg_hdr_ext *ipa_ep_cfg);

int ipa3_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ipa_ep_cfg);

int ipa3_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ipa_ep_cfg);

int ipa3_cfg_ep_deaggr(u32 clnt_hdl,
		      const struct ipa_ep_cfg_deaggr *ipa_ep_cfg);

int ipa3_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ipa_ep_cfg);

int ipa3_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa3_cfg_ep_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_cfg *ipa_ep_cfg);

int ipa3_cfg_ep_metadata_mask(u32 clnt_hdl,
		const struct ipa_ep_cfg_metadata_mask *ipa_ep_cfg);

int ipa3_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa3_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl);

/*
 * Header removal / addition
 */
int ipa3_add_hdr(struct ipa_ioc_add_hdr *hdrs);

int ipa3_del_hdr(struct ipa_ioc_del_hdr *hdls);

int ipa3_del_hdr_by_user(struct ipa_ioc_del_hdr *hdls, bool by_user);

int ipa3_commit_hdr(void);

int ipa3_reset_hdr(void);

int ipa3_get_hdr(struct ipa_ioc_get_hdr *lookup);

int ipa3_put_hdr(u32 hdr_hdl);

int ipa3_copy_hdr(struct ipa_ioc_copy_hdr *copy);

/*
 * Header Processing Context
 */
int ipa3_add_hdr_proc_ctx(struct ipa_ioc_add_hdr_proc_ctx *proc_ctxs);

int ipa3_del_hdr_proc_ctx(struct ipa_ioc_del_hdr_proc_ctx *hdls);

int ipa3_del_hdr_proc_ctx_by_user(struct ipa_ioc_del_hdr_proc_ctx *hdls,
	bool by_user);

/*
 * Routing
 */
int ipa3_add_rt_rule(struct ipa_ioc_add_rt_rule *rules);

int ipa3_add_rt_rule_ext(struct ipa_ioc_add_rt_rule_ext *rules);

int ipa3_add_rt_rule_after(struct ipa_ioc_add_rt_rule_after *rules);

int ipa3_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls);

int ipa3_commit_rt(enum ipa_ip_type ip);

int ipa3_reset_rt(enum ipa_ip_type ip);

int ipa3_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup);

int ipa3_put_rt_tbl(u32 rt_tbl_hdl);

int ipa3_query_rt_index(struct ipa_ioc_get_rt_tbl_indx *in);

int ipa3_mdfy_rt_rule(struct ipa_ioc_mdfy_rt_rule *rules);

/*
 * Filtering
 */
int ipa3_add_flt_rule(struct ipa_ioc_add_flt_rule *rules);

int ipa3_add_flt_rule_after(struct ipa_ioc_add_flt_rule_after *rules);

int ipa3_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls);

int ipa3_mdfy_flt_rule(struct ipa_ioc_mdfy_flt_rule *rules);

int ipa3_commit_flt(enum ipa_ip_type ip);

int ipa3_reset_flt(enum ipa_ip_type ip);

/*
 * NAT
 */
int ipa3_allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem);
int ipa3_allocate_nat_table(
	struct ipa_ioc_nat_ipv6ct_table_alloc *table_alloc);

int ipa3_nat_init_cmd(struct ipa_ioc_v4_nat_init *init);

int ipa3_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma);

int ipa3_nat_del_cmd(struct ipa_ioc_v4_nat_del *del);
int ipa3_del_nat_table(struct ipa_ioc_nat_ipv6ct_table_del *del);

/*
 * Messaging
 */
int ipa3_send_msg(struct ipa_msg_meta *meta, void *buff,
		  ipa_msg_free_fn callback);
int ipa3_register_pull_msg(struct ipa_msg_meta *meta, ipa_msg_pull_fn callback);
int ipa3_deregister_pull_msg(struct ipa_msg_meta *meta);

/*
 * Interface
 */
int ipa3_register_intf(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx);
int ipa3_register_intf_ext(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx,
		       const struct ipa_ext_intf *ext);
int ipa3_deregister_intf(const char *name);

/*
 * Aggregation
 */
int ipa3_set_aggr_mode(enum ipa_aggr_mode mode);

int ipa3_set_qcncm_ndp_sig(char sig[3]);

int ipa3_set_single_ndp_per_mbim(bool enable);

/*
 * Data path
 */
int ipa3_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

/*
 * To transfer multiple data packets
 * While passing the data descriptor list, the anchor node
 * should be of type struct ipa_tx_data_desc not list_head
*/
int ipa3_tx_dp_mul(enum ipa_client_type dst,
			struct ipa_tx_data_desc *data_desc);

void ipa3_free_skb(struct ipa_rx_data *);

/*
 * System pipes
 */
int ipa3_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl);

int ipa3_teardown_sys_pipe(u32 clnt_hdl);

int ipa3_sys_setup(struct ipa_sys_connect_params *sys_in,
	unsigned long *ipa_bam_hdl,
	u32 *ipa_pipe_num, u32 *clnt_hdl, bool en_status);

int ipa3_sys_teardown(u32 clnt_hdl);

int ipa3_sys_update_gsi_hdls(u32 clnt_hdl, unsigned long gsi_ch_hdl,
	unsigned long gsi_ev_hdl);

int ipa3_connect_wdi_pipe(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out);
int ipa3_disconnect_wdi_pipe(u32 clnt_hdl);
int ipa3_enable_wdi_pipe(u32 clnt_hdl);
int ipa3_disable_wdi_pipe(u32 clnt_hdl);
int ipa3_resume_wdi_pipe(u32 clnt_hdl);
int ipa3_suspend_wdi_pipe(u32 clnt_hdl);
int ipa3_get_wdi_stats(struct IpaHwStatsWDIInfoData_t *stats);
u16 ipa3_get_smem_restr_bytes(void);
int ipa3_setup_uc_ntn_pipes(struct ipa_ntn_conn_in_params *in,
		ipa_notify_cb notify, void *priv, u8 hdr_len,
		struct ipa_ntn_conn_out_params *outp);
int ipa3_tear_down_uc_offload_pipes(int ipa_ep_idx_ul, int ipa_ep_idx_dl);
int ipa3_ntn_uc_reg_rdyCB(void (*ipauc_ready_cb)(void *), void *priv);
void ipa3_ntn_uc_dereg_rdyCB(void);
int ipa3_conn_wdi3_pipes(struct ipa_wdi3_conn_in_params *in,
	struct ipa_wdi3_conn_out_params *out);
int ipa3_disconn_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx);
int ipa3_enable_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx);
int ipa3_disable_wdi3_pipes(int ipa_ep_idx_tx, int ipa_ep_idx_rx);

/*
 * To retrieve doorbell physical address of
 * wlan pipes
 */
int ipa3_uc_wdi_get_dbpa(struct ipa_wdi_db_params *out);

/*
 * To register uC ready callback if uC not ready
 * and also check uC readiness
 * if uC not ready only, register callback
 */
int ipa3_uc_reg_rdyCB(struct ipa_wdi_uc_ready_params *param);
/*
 * To de-register uC ready callback
 */
int ipa3_uc_dereg_rdyCB(void);

/*
 * Tethering bridge (Rmnet / MBIM)
 */
int ipa3_teth_bridge_init(struct teth_bridge_init_params *params);

int ipa3_teth_bridge_disconnect(enum ipa_client_type client);

int ipa3_teth_bridge_connect(struct teth_bridge_connect_params *connect_params);

/*
 * Tethering client info
 */
void ipa3_set_client(int index, enum ipacm_client_enum client, bool uplink);

enum ipacm_client_enum ipa3_get_client(int pipe_idx);

bool ipa3_get_client_uplink(int pipe_idx);

/*
 * IPADMA
 */
int ipa3_dma_init(void);

int ipa3_dma_enable(void);

int ipa3_dma_disable(void);

int ipa3_dma_sync_memcpy(u64 dest, u64 src, int len);

int ipa3_dma_async_memcpy(u64 dest, u64 src, int len,
			void (*user_cb)(void *user1), void *user_param);

int ipa3_dma_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len);

void ipa3_dma_destroy(void);

/*
 * MHI
 */

int ipa3_mhi_init_engine(struct ipa_mhi_init_engine *params);

int ipa3_connect_mhi_pipe(
		struct ipa_mhi_connect_params_internal *in,
		u32 *clnt_hdl);

int ipa3_disconnect_mhi_pipe(u32 clnt_hdl);

bool ipa3_mhi_stop_gsi_channel(enum ipa_client_type client);

int ipa3_mhi_reset_channel_internal(enum ipa_client_type client);

int ipa3_mhi_start_channel_internal(enum ipa_client_type client);

bool ipa3_has_open_aggr_frame(enum ipa_client_type client);

int ipa3_mhi_resume_channels_internal(enum ipa_client_type client,
		bool LPTransitionRejected, bool brstmode_enabled,
		union __packed gsi_channel_scratch ch_scratch, u8 index);

int ipa3_mhi_destroy_channel(enum ipa_client_type client);

/*
 * mux id
 */
int ipa3_write_qmap_id(struct ipa_ioc_write_qmapid *param_in);

/*
 * interrupts
 */
int ipa3_add_interrupt_handler(enum ipa_irq_type interrupt,
		ipa_irq_handler_t handler,
		bool deferred_flag,
		void *private_data);

int ipa3_remove_interrupt_handler(enum ipa_irq_type interrupt);

/*
 * Miscellaneous
 */
void ipa3_bam_reg_dump(void);

int ipa3_get_ep_mapping(enum ipa_client_type client);

bool ipa3_is_ready(void);

void ipa3_proxy_clk_vote(void);
void ipa3_proxy_clk_unvote(void);

bool ipa3_is_client_handle_valid(u32 clnt_hdl);

enum ipa_client_type ipa3_get_client_mapping(int pipe_idx);

void ipa_init_ep_flt_bitmap(void);

bool ipa_is_ep_support_flt(int pipe_idx);

enum ipa_rm_resource_name ipa3_get_rm_resource_from_ep(int pipe_idx);

bool ipa3_get_modem_cfg_emb_pipe_flt(void);

u8 ipa3_get_qmb_master_sel(enum ipa_client_type client);

/* internal functions */

int ipa3_bind_api_controller(enum ipa_hw_type ipa_hw_type,
	struct ipa_api_controller *api_ctrl);

bool ipa_is_modem_pipe(int pipe_idx);

int ipa3_send_one(struct ipa3_sys_context *sys, struct ipa3_desc *desc,
		bool in_atomic);
int ipa3_send(struct ipa3_sys_context *sys,
		u32 num_desc,
		struct ipa3_desc *desc,
		bool in_atomic);
int ipa3_get_ep_mapping(enum ipa_client_type client);
int ipa_get_ep_group(enum ipa_client_type client);

int ipa3_generate_hw_rule(enum ipa_ip_type ip,
			 const struct ipa_rule_attrib *attrib,
			 u8 **buf,
			 u16 *en_rule);
u8 *ipa3_write_64(u64 w, u8 *dest);
u8 *ipa3_write_32(u32 w, u8 *dest);
u8 *ipa3_write_16(u16 hw, u8 *dest);
u8 *ipa3_write_8(u8 b, u8 *dest);
u8 *ipa3_pad_to_32(u8 *dest);
u8 *ipa3_pad_to_64(u8 *dest);
int ipa3_init_hw(void);
struct ipa3_rt_tbl *__ipa3_find_rt_tbl(enum ipa_ip_type ip, const char *name);
int ipa3_set_single_ndp_per_mbim(bool);
void ipa3_debugfs_init(void);
void ipa3_debugfs_remove(void);

void ipa3_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size);
#ifdef IPA_DEBUG
#define IPA_DUMP_BUFF(base, phy_base, size) \
	ipa3_dump_buff_internal(base, phy_base, size)
#else
#define IPA_DUMP_BUFF(base, phy_base, size)
#endif
int ipa3_controller_static_bind(struct ipa3_controller *controller,
		enum ipa_hw_type ipa_hw_type);
int ipa3_cfg_route(struct ipahal_reg_route *route);
int ipa3_send_cmd_timeout(u16 num_desc, struct ipa3_desc *descr, u32 timeout);
int ipa3_send_cmd(u16 num_desc, struct ipa3_desc *descr);
int ipa3_cfg_filter(u32 disable);
int ipa3_pipe_mem_init(u32 start_ofst, u32 size);
int ipa3_pipe_mem_alloc(u32 *ofst, u32 size);
int ipa3_pipe_mem_free(u32 ofst, u32 size);
int ipa3_straddle_boundary(u32 start, u32 end, u32 boundary);
struct ipa3_context *ipa3_get_ctx(void);
void ipa3_enable_clks(void);
void ipa3_disable_clks(void);
void ipa3_inc_client_enable_clks(struct ipa_active_client_logging_info *id);
int ipa3_inc_client_enable_clks_no_block(struct ipa_active_client_logging_info
		*id);
void ipa3_dec_client_disable_clks(struct ipa_active_client_logging_info *id);
void ipa3_active_clients_log_dec(struct ipa_active_client_logging_info *id,
		bool int_ctx);
void ipa3_active_clients_log_inc(struct ipa_active_client_logging_info *id,
		bool int_ctx);
int ipa3_active_clients_log_print_buffer(char *buf, int size);
int ipa3_active_clients_log_print_table(char *buf, int size);
void ipa3_active_clients_log_clear(void);
int ipa3_interrupts_init(u32 ipa_irq, u32 ee, struct device *ipa_dev);
int __ipa3_del_rt_rule(u32 rule_hdl);
int __ipa3_del_hdr(u32 hdr_hdl, bool by_user);
int __ipa3_release_hdr(u32 hdr_hdl);
int __ipa3_release_hdr_proc_ctx(u32 proc_ctx_hdl);
int _ipa_read_ep_reg_v3_0(char *buf, int max_len, int pipe);
void _ipa_enable_clks_v3_0(void);
void _ipa_disable_clks_v3_0(void);
struct device *ipa3_get_dma_dev(void);
void ipa3_suspend_active_aggr_wa(u32 clnt_hdl);
void ipa3_suspend_handler(enum ipa_irq_type interrupt,
				void *private_data,
				void *interrupt_data);

ssize_t ipa3_read(struct file *filp, char __user *buf, size_t count,
		 loff_t *f_pos);
int ipa3_pull_msg(struct ipa_msg_meta *meta, char *buff, size_t count);
int ipa3_query_intf(struct ipa_ioc_query_intf *lookup);
int ipa3_query_intf_tx_props(struct ipa_ioc_query_intf_tx_props *tx);
int ipa3_query_intf_rx_props(struct ipa_ioc_query_intf_rx_props *rx);
int ipa3_query_intf_ext_props(struct ipa_ioc_query_intf_ext_props *ext);

void wwan_cleanup(void);

int ipa3_teth_bridge_driver_init(void);
void ipa3_lan_rx_cb(void *priv, enum ipa_dp_evt_type evt, unsigned long data);

int _ipa_init_sram_v3_0(void);
int _ipa_init_sram_v3_5(void);

int _ipa_init_hdr_v3_0(void);
int _ipa_init_rt4_v3(void);
int _ipa_init_rt6_v3(void);
int _ipa_init_flt4_v3(void);
int _ipa_init_flt6_v3(void);

int __ipa_commit_flt_v3(enum ipa_ip_type ip);
int __ipa_commit_rt_v3(enum ipa_ip_type ip);
int __ipa_generate_rt_hw_rule_v3_0(enum ipa_ip_type ip,
	struct ipa3_rt_entry *entry, u8 *buf);

int __ipa_commit_hdr_v3_0(void);
int ipa3_generate_flt_eq(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_attrib);
void ipa3_skb_recycle(struct sk_buff *skb);
void ipa3_install_dflt_flt_rules(u32 ipa_ep_idx);
void ipa3_delete_dflt_flt_rules(u32 ipa_ep_idx);

int ipa3_enable_data_path(u32 clnt_hdl);
int ipa3_disable_data_path(u32 clnt_hdl);
int ipa3_alloc_rule_id(struct idr *rule_ids);
int ipa3_id_alloc(void *ptr);
void *ipa3_id_find(u32 id);
void ipa3_id_remove(u32 id);

int ipa3_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
				  u32 bandwidth_mbps);

int ipa3_cfg_ep_status(u32 clnt_hdl,
		const struct ipahal_reg_ep_cfg_status *ipa_ep_cfg);

int ipa3_suspend_resource_no_block(enum ipa_rm_resource_name name);
int ipa3_suspend_resource_sync(enum ipa_rm_resource_name name);
int ipa3_resume_resource(enum ipa_rm_resource_name name);
bool ipa3_should_pipe_be_suspended(enum ipa_client_type client);
int ipa3_tag_aggr_force_close(int pipe_num);

void ipa3_active_clients_lock(void);
int ipa3_active_clients_trylock(unsigned long *flags);
void ipa3_active_clients_unlock(void);
void ipa3_active_clients_trylock_unlock(unsigned long *flags);
int ipa3_wdi_init(void);
int ipa3_write_qmapid_wdi_pipe(u32 clnt_hdl, u8 qmap_id);
int ipa3_tag_process(struct ipa3_desc *desc, int num_descs,
		    unsigned long timeout);

void ipa3_q6_pre_shutdown_cleanup(void);
void ipa3_q6_post_shutdown_cleanup(void);
int ipa3_init_q6_smem(void);

int ipa3_sps_connect_safe(struct sps_pipe *h, struct sps_connect *connect,
			 enum ipa_client_type ipa_client);

int ipa3_mhi_handle_ipa_config_req(struct ipa_config_req_msg_v01 *config_req);
int ipa3_mhi_query_ch_info(enum ipa_client_type client,
		struct gsi_chan_info *ch_info);

int ipa3_uc_interface_init(void);
int ipa3_uc_reset_pipe(enum ipa_client_type ipa_client);
int ipa3_uc_is_gsi_channel_empty(enum ipa_client_type ipa_client);
int ipa3_uc_state_check(void);
int ipa3_uc_loaded_check(void);
void ipa3_uc_load_notify(void);
int ipa3_uc_send_cmd(u32 cmd, u32 opcode, u32 expected_status,
		    bool polling_mode, unsigned long timeout_jiffies);
void ipa3_uc_register_handlers(enum ipa3_hw_features feature,
			      struct ipa3_uc_hdlrs *hdlrs);
int ipa3_create_nat_device(void);
int ipa3_uc_notify_clk_state(bool enabled);
void ipa3_dma_async_memcpy_notify_cb(void *priv,
		enum ipa_dp_evt_type evt, unsigned long data);

int ipa3_uc_update_hw_flags(u32 flags);

int ipa3_uc_mhi_init(void (*ready_cb)(void), void (*wakeup_request_cb)(void));
void ipa3_uc_mhi_cleanup(void);
int ipa3_uc_mhi_send_dl_ul_sync_info(union IpaHwMhiDlUlSyncCmdData_t *cmd);
int ipa3_uc_mhi_init_engine(struct ipa_mhi_msi_info *msi, u32 mmio_addr,
	u32 host_ctrl_addr, u32 host_data_addr, u32 first_ch_idx,
	u32 first_evt_idx);
int ipa3_uc_mhi_init_channel(int ipa_ep_idx, int channelHandle,
	int contexArrayIndex, int channelDirection);
int ipa3_uc_mhi_reset_channel(int channelHandle);
int ipa3_uc_mhi_suspend_channel(int channelHandle);
int ipa3_uc_mhi_resume_channel(int channelHandle, bool LPTransitionRejected);
int ipa3_uc_mhi_stop_event_update_channel(int channelHandle);
int ipa3_uc_mhi_print_stats(char *dbg_buff, int size);
int ipa3_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len);
void ipa3_tag_destroy_imm(void *user1, int user2);
const struct ipa_gsi_ep_config *ipa3_get_gsi_ep_info
	(enum ipa_client_type client);
void ipa3_uc_rg10_write_reg(enum ipahal_reg_name reg, u32 n, u32 val);

u32 ipa3_get_num_pipes(void);
struct ipa_smmu_cb_ctx *ipa3_get_smmu_ctx(void);
struct ipa_smmu_cb_ctx *ipa3_get_wlan_smmu_ctx(void);
struct ipa_smmu_cb_ctx *ipa3_get_uc_smmu_ctx(void);
struct iommu_domain *ipa3_get_smmu_domain(void);
struct iommu_domain *ipa3_get_uc_smmu_domain(void);
struct iommu_domain *ipa3_get_wlan_smmu_domain(void);
int ipa3_iommu_map(struct iommu_domain *domain, unsigned long iova,
	phys_addr_t paddr, size_t size, int prot);
int ipa3_ap_suspend(struct device *dev);
int ipa3_ap_resume(struct device *dev);
int ipa3_init_interrupts(void);
struct iommu_domain *ipa3_get_smmu_domain(void);
int ipa3_release_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info);
int ipa3_create_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info);
int ipa3_set_flt_tuple_mask(int pipe_idx, struct ipahal_reg_hash_tuple *tuple);
int ipa3_set_rt_tuple_mask(int tbl_idx, struct ipahal_reg_hash_tuple *tuple);
void ipa3_set_resorce_groups_min_max_limits(void);
void ipa3_suspend_apps_pipes(bool suspend);
void ipa3_flow_control(enum ipa_client_type ipa_client, bool enable,
			uint32_t qmap_id);
int ipa3_generate_eq_from_hw_rule(
	struct ipa_ipfltri_rule_eq *attrib, u8 *buf, u8 *rule_size);
int ipa3_flt_read_tbl_from_hw(u32 pipe_idx,
	enum ipa_ip_type ip_type,
	bool hashable,
	struct ipa3_flt_entry entry[],
	int *num_entry);
int ipa3_rt_read_tbl_from_hw(u32 tbl_idx,
	enum ipa_ip_type ip_type,
	bool hashable,
	struct ipa3_debugfs_rt_entry entry[],
	int *num_entry);
int ipa3_calc_extra_wrd_bytes(const struct ipa_ipfltri_rule_eq *attrib);
int ipa3_restore_suspend_handler(void);
int ipa3_inject_dma_task_for_gsi(void);
int ipa3_uc_panic_notifier(struct notifier_block *this,
	unsigned long event, void *ptr);
void ipa3_inc_acquire_wakelock(void);
void ipa3_dec_release_wakelock(void);
int ipa3_load_fws(const struct firmware *firmware, phys_addr_t gsi_mem_base);
int ipa3_register_ipa_ready_cb(void (*ipa_ready_cb)(void *), void *user_data);
const char *ipa_hw_error_str(enum ipa3_hw_errors err_type);
int ipa_gsi_ch20_wa(void);
int ipa3_ntn_init(void);
int ipa3_get_ntn_stats(struct Ipa3HwStatsNTNInfoData_t *stats);
int ipa3_smmu_map_peer_reg(phys_addr_t phys_addr, bool map);
int ipa3_smmu_map_peer_buff(u64 iova, phys_addr_t phys_addr,
	u32 size, bool map);
void ipa3_reset_freeze_vote(void);
struct dentry *ipa_debugfs_get_root(void);
bool ipa3_is_msm_device(void);
int ipa3_tz_unlock_reg(struct ipa_tz_unlock_reg_info *reg_info, u16 num_regs);
struct device *ipa3_get_pdev(void);
void ipa3_enable_dcd(void);
void ipa3_disable_prefetch(enum ipa_client_type client);
int ipa3_alloc_common_event_ring(void);
int ipa3_allocate_dma_task_for_gsi(void);
void ipa3_free_dma_task_for_gsi(void);
#endif /* _IPA3_I_H_ */
