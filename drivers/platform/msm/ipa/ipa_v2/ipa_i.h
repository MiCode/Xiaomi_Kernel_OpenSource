/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_I_H_
#define _IPA_I_H_

#include <linux/bitops.h>
#include <linux/cdev.h>
#include <linux/export.h>
#include <linux/idr.h>
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/ipa.h>
#include <linux/msm-sps.h>
#include <linux/platform_device.h>
#include <asm/dma-iommu.h>
#include <linux/iommu.h>
#include <linux/ipa_uc_offload.h>
#include "ipa_hw_defs.h"
#include "ipa_ram_mmap.h"
#include "ipa_reg.h"
#include "ipa_qmi_service.h"
#include "../ipa_api.h"
#include "../ipa_common_i.h"
#include "ipa_uc_offload_i.h"

#define DRV_NAME "ipa"
#define NAT_DEV_NAME "ipaNatTable"
#define IPA_COOKIE 0x57831603
#define MTU_BYTE 1500

#define IPA_MAX_NUM_PIPES 0x14
#define IPA_SYS_DESC_FIFO_SZ 0x2000
#define IPA_SYS_TX_DATA_DESC_FIFO_SZ 0x1000
#define IPA_LAN_RX_HEADER_LENGTH (2)
#define IPA_QMAP_HEADER_LENGTH (4)
#define IPA_DL_CHECKSUM_LENGTH (8)
#define IPA_NUM_DESC_PER_SW_TX (2)
#define IPA_GENERIC_RX_POOL_SZ 1000
#define IPA_UC_FINISH_MAX 6
#define IPA_UC_WAIT_MIN_SLEEP 1000
#define IPA_UC_WAII_MAX_SLEEP 1200
#define IPA_WAN_NAPI_CONS_RX_POOL_SZ (IPA_GENERIC_RX_POOL_SZ*3)
#define IPA_WAN_CONS_DESC_FIFO_SZ (IPA_SYS_DESC_FIFO_SZ*3)

#define IPA_MAX_STATUS_STAT_NUM 30

#define IPADBG(fmt, args...) \
	pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
#define IPAERR(fmt, args...) \
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

#define WLAN_AMPDU_TX_EP 15
#define WLAN_PROD_TX_EP  19
#define WLAN1_CONS_RX_EP  14
#define WLAN2_CONS_RX_EP  16
#define WLAN3_CONS_RX_EP  17
#define WLAN4_CONS_RX_EP  18

#define MAX_NUM_EXCP     8

#define IPA_STATS

#ifdef IPA_STATS
#define IPA_STATS_INC_CNT(val) (++val)
#define IPA_STATS_DEC_CNT(val) (--val)
#define IPA_STATS_EXCP_CNT(flags, base) do {			\
			int i;					\
			for (i = 0; i < MAX_NUM_EXCP; i++)	\
				if (flags & BIT(i))		\
					++base[i];		\
			if (flags == 0)				\
				++base[MAX_NUM_EXCP - 1];	\
			} while (0)
#else
#define IPA_STATS_INC_CNT(x) do { } while (0)
#define IPA_STATS_DEC_CNT(x)
#define IPA_STATS_EXCP_CNT(flags, base) do { } while (0)
#endif

#define IPA_TOS_EQ			BIT(0)
#define IPA_PROTOCOL_EQ			BIT(1)
#define IPA_OFFSET_MEQ32_0		BIT(2)
#define IPA_OFFSET_MEQ32_1		BIT(3)
#define IPA_IHL_OFFSET_RANGE16_0	BIT(4)
#define IPA_IHL_OFFSET_RANGE16_1	BIT(5)
#define IPA_IHL_OFFSET_EQ_16		BIT(6)
#define IPA_IHL_OFFSET_EQ_32		BIT(7)
#define IPA_IHL_OFFSET_MEQ32_0		BIT(8)
#define IPA_OFFSET_MEQ128_0		BIT(9)
#define IPA_OFFSET_MEQ128_1		BIT(10)
#define IPA_TC_EQ			BIT(11)
#define IPA_FL_EQ			BIT(12)
#define IPA_IHL_OFFSET_MEQ32_1		BIT(13)
#define IPA_METADATA_COMPARE		BIT(14)
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

#define IPA_SETFIELD(val, shift, mask) (((val) << (shift)) & (mask))
#define IPA_SETFIELD_IN_REG(reg, val, shift, mask) \
			(reg |= ((val) << (shift)) & (mask))

#define IPA_HW_TABLE_ALIGNMENT(start_ofst) \
	(((start_ofst) + 127) & ~127)
#define IPA_RT_FLT_HW_RULE_BUF_SIZE	(256)

#define IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE 8
#define IPA_HDR_PROC_CTX_TABLE_ALIGNMENT(start_ofst) \
	(((start_ofst) + IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE - 1) & \
	~(IPA_HDR_PROC_CTX_TABLE_ALIGNMENT_BYTE - 1))

#define MAX_RESOURCE_TO_CLIENTS (IPA_CLIENT_MAX)
#define IPA_MEM_PART(x_) (ipa_ctx->ctrl->mem_partition.x_)

#define IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES 120
#define IPA2_ACTIVE_CLIENTS_LOG_LINE_LEN 96
#define IPA2_ACTIVE_CLIENTS_LOG_HASHTABLE_SIZE 50
#define IPA2_ACTIVE_CLIENTS_LOG_NAME_LEN 40

struct ipa2_active_client_htable_entry {
	struct hlist_node list;
	char id_string[IPA2_ACTIVE_CLIENTS_LOG_NAME_LEN];
	int count;
	enum ipa_active_client_log_type type;
};

struct ipa2_active_clients_log_ctx {
	char *log_buffer[IPA2_ACTIVE_CLIENTS_LOG_BUFFER_SIZE_LINES];
	int log_head;
	int log_tail;
	bool log_rdy;
	struct hlist_head htable[IPA2_ACTIVE_CLIENTS_LOG_HASHTABLE_SIZE];
};


struct ipa_client_names {
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
 * struct ipa_flt_entry - IPA filtering table entry
 * @link: entry's link in global filtering enrties list
 * @rule: filter rule
 * @cookie: cookie used for validity check
 * @tbl: filter table
 * @rt_tbl: routing table
 * @hw_len: entry's size
 */
struct ipa_flt_entry {
	struct list_head link;
	struct ipa_flt_rule rule;
	u32 cookie;
	struct ipa_flt_tbl *tbl;
	struct ipa_rt_tbl *rt_tbl;
	u32 hw_len;
	int id;
};

/**
 * struct ipa_rt_tbl - IPA routing table
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
 */
struct ipa_rt_tbl {
	struct list_head link;
	struct list_head head_rt_rule_list;
	char name[IPA_RESOURCE_NAME_MAX];
	u32 idx;
	u32 rule_cnt;
	u32 ref_cnt;
	struct ipa_rt_tbl_set *set;
	u32 cookie;
	bool in_sys;
	u32 sz;
	struct ipa_mem_buffer curr_mem;
	struct ipa_mem_buffer prev_mem;
	int id;
};

/**
 * struct ipa_hdr_entry - IPA header table entry
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
 */
struct ipa_hdr_entry {
	struct list_head link;
	u8 hdr[IPA_HDR_MAX_SIZE];
	u32 hdr_len;
	char name[IPA_RESOURCE_NAME_MAX];
	enum ipa_hdr_l2_type type;
	u8 is_partial;
	bool is_hdr_proc_ctx;
	dma_addr_t phys_base;
	struct ipa_hdr_proc_ctx_entry *proc_ctx;
	struct ipa_hdr_offset_entry *offset_entry;
	u32 cookie;
	u32 ref_cnt;
	int id;
	u8 is_eth2_ofst_valid;
	u16 eth2_ofst;
};

/**
 * struct ipa_hdr_tbl - IPA header table
 * @head_hdr_entry_list: header entries list
 * @head_offset_list: header offset list
 * @head_free_offset_list: header free offset list
 * @hdr_cnt: number of headers
 * @end: the last header index
 */
struct ipa_hdr_tbl {
	struct list_head head_hdr_entry_list;
	struct list_head head_offset_list[IPA_HDR_BIN_MAX];
	struct list_head head_free_offset_list[IPA_HDR_BIN_MAX];
	u32 hdr_cnt;
	u32 end;
};

/**
 * struct ipa_hdr_offset_entry - IPA header offset entry
 * @link: entry's link in global processing context header offset entries list
 * @offset: the offset
 * @bin: bin
 */
struct ipa_hdr_proc_ctx_offset_entry {
	struct list_head link;
	u32 offset;
	u32 bin;
};

/**
 * struct ipa_hdr_proc_ctx_add_hdr_seq -
 * IPA processing context header - add header sequence
 * @hdr_add: add header command
 * @end: tlv end command (cmd.type must be 0)
 */
struct ipa_hdr_proc_ctx_add_hdr_seq {
	struct ipa_hdr_proc_ctx_hdr_add hdr_add;
	struct ipa_hdr_proc_ctx_tlv end;
};

/**
 * struct ipa_hdr_proc_ctx_add_hdr_cmd_seq -
 * IPA processing context header - process command sequence
 * @hdr_add: add header command
 * @cmd: tlv processing command (cmd.type must be 3)
 * @end: tlv end command (cmd.type must be 0)
 */
struct ipa_hdr_proc_ctx_add_hdr_cmd_seq {
	struct ipa_hdr_proc_ctx_hdr_add hdr_add;
	struct ipa_hdr_proc_ctx_tlv cmd;
	struct ipa_hdr_proc_ctx_tlv end;
};

/**
 struct ipa_hdr_proc_ctx_entry - IPA processing context header table entry
 * @link: entry's link in global header table entries list
 * @type:
 * @offset_entry: entry's offset
 * @hdr: the header
 * @cookie: cookie used for validity check
 * @ref_cnt: reference counter of routing table
 * @id: processing context header entry id
 */
struct ipa_hdr_proc_ctx_entry {
	struct list_head link;
	enum ipa_hdr_proc_type type;
	struct ipa_hdr_proc_ctx_offset_entry *offset_entry;
	struct ipa_hdr_entry *hdr;
	u32 cookie;
	u32 ref_cnt;
	int id;
};

/**
 * struct ipa_hdr_proc_ctx_tbl - IPA processing context header table
 * @head_proc_ctx_entry_list: header entries list
 * @head_offset_list: header offset list
 * @head_free_offset_list: header free offset list
 * @proc_ctx_cnt: number of processing context headers
 * @end: the last processing context header index
 * @start_offset: offset in words of processing context header table
 */
struct ipa_hdr_proc_ctx_tbl {
	struct list_head head_proc_ctx_entry_list;
	struct list_head head_offset_list[IPA_HDR_PROC_CTX_BIN_MAX];
	struct list_head head_free_offset_list[IPA_HDR_PROC_CTX_BIN_MAX];
	u32 proc_ctx_cnt;
	u32 end;
	u32 start_offset;
};

/**
 * struct ipa_flt_tbl - IPA filter table
 * @head_flt_rule_list: filter rules list
 * @rule_cnt: number of filter rules
 * @in_sys: flag indicating if filter table is located in system memory
 * @sz: the size of the filter table
 * @end: the last header index
 * @curr_mem: current filter tables block in sys memory
 * @prev_mem: previous filter table block in sys memory
 */
struct ipa_flt_tbl {
	struct list_head head_flt_rule_list;
	u32 rule_cnt;
	bool in_sys;
	u32 sz;
	struct ipa_mem_buffer curr_mem;
	struct ipa_mem_buffer prev_mem;
	bool sticky_rear;
};

/**
 * struct ipa_rt_entry - IPA routing table entry
 * @link: entry's link in global routing table entries list
 * @rule: routing rule
 * @cookie: cookie used for validity check
 * @tbl: routing table
 * @hdr: header table
 * @proc_ctx: processing context table
 * @hw_len: the length of the table
 */
struct ipa_rt_entry {
	struct list_head link;
	struct ipa_rt_rule rule;
	u32 cookie;
	struct ipa_rt_tbl *tbl;
	struct ipa_hdr_entry *hdr;
	struct ipa_hdr_proc_ctx_entry *proc_ctx;
	u32 hw_len;
	int id;
};

/**
 * struct ipa_rt_tbl_set - collection of routing tables
 * @head_rt_tbl_list: collection of routing tables
 * @tbl_cnt: number of routing tables
 */
struct ipa_rt_tbl_set {
	struct list_head head_rt_tbl_list;
	u32 tbl_cnt;
};

/**
 * struct ipa_ep_cfg_status - status configuration in IPA end-point
 * @status_en: Determines if end point supports Status Indications. SW should
 *	set this bit in order to enable Statuses. Output Pipe - send
 *	Status indications only if bit is set. Input Pipe - forward Status
 *	indication to STATUS_ENDP only if bit is set. Valid for Input
 *	and Output Pipes (IPA Consumer and Producer)
 * @status_ep: Statuses generated for this endpoint will be forwarded to the
 *	specified Status End Point. Status endpoint needs to be
 *	configured with STATUS_EN=1 Valid only for Input Pipes (IPA
 *	Consumer)
 */
struct ipa_ep_cfg_status {
	bool status_en;
	u8 status_ep;
};

/**
 * struct ipa_wlan_stats - Wlan stats for each wlan endpoint
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
struct ipa_wlan_stats {
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
 * struct ipa_wlan_comm_memb - Wlan comm members
 * @wlan_spinlock: protects wlan comm buff list and its size
 * @ipa_tx_mul_spinlock: protects tx dp mul transfer
 * @wlan_comm_total_cnt: wlan common skb buffers allocated count
 * @wlan_comm_free_cnt: wlan common skb buffer free count
 * @total_tx_pkts_freed: Recycled Buffer count
 * @wlan_comm_desc_list: wlan common skb buffer list
 */
struct ipa_wlan_comm_memb {
	spinlock_t wlan_spinlock;
	spinlock_t ipa_tx_mul_spinlock;
	u32 wlan_comm_total_cnt;
	u32 wlan_comm_free_cnt;
	u32 total_tx_pkts_freed;
	struct list_head wlan_comm_desc_list;
	atomic_t active_clnt_cnt;
};

struct ipa_status_stats {
	struct ipa_hw_pkt_status status[IPA_MAX_STATUS_STAT_NUM];
	int curr;
};

enum ipa_wakelock_ref_client {
	IPA_WAKELOCK_REF_CLIENT_TX  = 0,
	IPA_WAKELOCK_REF_CLIENT_LAN_RX = 1,
	IPA_WAKELOCK_REF_CLIENT_WAN_RX = 2,
	IPA_WAKELOCK_REF_CLIENT_WLAN_RX = 3,
	IPA_WAKELOCK_REF_CLIENT_ODU_RX = 4,
	IPA_WAKELOCK_REF_CLIENT_SPS = 5,
	IPA_WAKELOCK_REF_CLIENT_MAX
};

/**
 * struct ipa_ep_context - IPA end point context
 * @valid: flag indicating id EP context is valid
 * @client: EP client type
 * @ep_hdl: EP's client SPS handle
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
 * @rx_replenish_threshold: Indicates the WM value which requires the RX
 *                          descriptors replenish function to be called to
 *                          avoid the RX pipe to run out of descriptors
 *                          and cause HOLB.
 * @disconnect_in_progress: Indicates client disconnect in progress.
 * @qmi_request_sent: Indicates whether QMI request to enable clear data path
 *					request is sent or not.
 * @napi_enabled: when true, IPA call client callback to start polling
 */
struct ipa_ep_context {
	int valid;
	enum ipa_client_type client;
	struct sps_pipe *ep_hdl;
	struct ipa_ep_cfg cfg;
	struct ipa_ep_cfg_holb holb;
	struct ipa_ep_cfg_status status;
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
	struct ipa_wlan_stats wstats;
	u32 uc_offload_state;
	u32 rx_replenish_threshold;
	bool disconnect_in_progress;
	u32 qmi_request_sent;
	enum ipa_wakelock_ref_client wakelock_client;
	bool napi_enabled;
	bool switch_to_intr;
	int inactive_cycles;
	u32 eot_in_poll_err;
	bool ep_disabled;

	/* sys MUST be the last element of this struct */
	struct ipa_sys_context *sys;
};

enum ipa_sys_pipe_policy {
	IPA_POLICY_INTR_MODE,
	IPA_POLICY_NOINTR_MODE,
	IPA_POLICY_INTR_POLL_MODE,
};

struct ipa_repl_ctx {
	struct ipa_rx_pkt_wrapper **cache;
	atomic_t head_idx;
	atomic_t tail_idx;
	u32 capacity;
};

/**
 * struct ipa_sys_context - IPA endpoint context for system to BAM pipes
 * @head_desc_list: header descriptors list
 * @len: the size of the above list
 * @spinlock: protects the list and its size
 * @event: used to request CALLBACK mode from SPS driver
 * @ep: IPA EP context
 *
 * IPA context specific to the system-bam pipes a.k.a LAN IN/OUT and WAN
 */
struct ipa_sys_context {
	u32 len;
	struct sps_register_event event;
	atomic_t curr_polling_state;
	struct delayed_work switch_to_intr_work;
	enum ipa_sys_pipe_policy policy;
	int (*pyld_hdlr)(struct sk_buff *skb, struct ipa_sys_context *sys);
	struct sk_buff * (*get_skb)(unsigned int len, gfp_t flags);
	void (*free_skb)(struct sk_buff *skb);
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
	void (*repl_hdlr)(struct ipa_sys_context *sys);
	struct ipa_repl_ctx repl;
	unsigned int repl_trig_cnt;
	unsigned int repl_trig_thresh;

	/* ordering is important - mutable fields go above */
	struct ipa_ep_context *ep;
	struct list_head head_desc_list;
	struct list_head rcycl_list;
	spinlock_t spinlock;
	struct workqueue_struct *wq;
	struct workqueue_struct *repl_wq;
	struct ipa_status_stats *status_stat;
	/* ordering is important - other immutable fields go below */
};

/**
 * enum ipa_desc_type - IPA decriptors type
 *
 * IPA decriptors type, IPA supports DD and ICD but no CD
 */
enum ipa_desc_type {
	IPA_DATA_DESC,
	IPA_DATA_DESC_SKB,
	IPA_DATA_DESC_SKB_PAGED,
	IPA_IMM_CMD_DESC
};

/**
 * struct ipa_tx_pkt_wrapper - IPA Tx packet wrapper
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
struct ipa_tx_pkt_wrapper {
	enum ipa_desc_type type;
	struct ipa_mem_buffer mem;
	struct work_struct work;
	struct list_head link;
	void (*callback)(void *user1, int user2);
	void *user1;
	int user2;
	struct ipa_sys_context *sys;
	struct ipa_mem_buffer mult;
	u32 cnt;
	void *bounce;
	bool no_unmap_dma;
};

/**
 * struct ipa_desc - IPA descriptor
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
 */
struct ipa_desc {
	enum ipa_desc_type type;
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
};

/**
 * struct ipa_rx_pkt_wrapper - IPA Rx packet wrapper
 * @skb: skb
 * @dma_address: DMA address of this Rx packet
 * @link: linked to the Rx packets on that pipe
 * @len: how many bytes are copied into skb's flat buffer
 */
struct ipa_rx_pkt_wrapper {
	struct list_head link;
	struct ipa_rx_data data;
	u32 len;
	struct work_struct work;
	struct ipa_sys_context *sys;
};

/**
 * struct ipa_nat_mem - IPA NAT memory description
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
struct ipa_nat_mem {
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
 * enum ipa_hw_mode - IPA hardware mode
 * @IPA_HW_Normal: Regular IPA hardware
 * @IPA_HW_Virtual: IPA hardware supporting virtual memory allocation
 * @IPA_HW_PCIE: IPA hardware supporting memory allocation over PCIE Bridge
 */
enum ipa_hw_mode {
	IPA_HW_MODE_NORMAL  = 0,
	IPA_HW_MODE_VIRTUAL = 1,
	IPA_HW_MODE_PCIE    = 2
};

enum ipa_config_this_ep {
	IPA_CONFIGURE_THIS_EP,
	IPA_DO_NOT_CONFIGURE_THIS_EP,
};

struct ipa_stats {
	u32 tx_sw_pkts;
	u32 tx_hw_pkts;
	u32 rx_pkts;
	u32 rx_excp_pkts[MAX_NUM_EXCP];
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

struct ipa_active_clients {
	struct mutex mutex;
	spinlock_t spinlock;
	bool mutex_locked;
	int cnt;
};

struct ipa_wakelock_ref_cnt {
	spinlock_t spinlock;
	u32 cnt;
};

struct ipa_tag_completion {
	struct completion comp;
	atomic_t cnt;
};

struct ipa_controller;

/**
 * struct ipa_uc_hdlrs - IPA uC callback functions
 * @ipa_uc_loaded_hdlr: Function handler when uC is loaded
 * @ipa_uc_event_hdlr: Event handler function
 * @ipa_uc_response_hdlr: Response handler function
 * @ipa_uc_event_log_info_hdlr: Log event handler function
 */
struct ipa_uc_hdlrs {
	void (*ipa_uc_loaded_hdlr)(void);

	void (*ipa_uc_event_hdlr)
		(struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio);
	int (*ipa_uc_response_hdlr)
		(struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio,
		u32 *uc_status);
	void (*ipa_uc_event_log_info_hdlr)
		(struct IpaHwEventLogInfoData_t *uc_event_top_mmio);
};

/**
 * enum ipa_hw_flags - flags which defines the behavior of HW
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
enum ipa_hw_flags {
	IPA_HW_FLAG_HALT_SYSTEM_ON_ASSERT_FAILURE	= 0x01,
	IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_ERORR		= 0x02,
	IPA_HW_FLAG_NO_REPORT_MHI_CHANNEL_WAKE_UP	= 0x04,
	IPA_HW_FLAG_WORK_OVER_DDR			= 0x08,
	IPA_HW_FLAG_NO_REPORT_OOB			= 0x10,
	IPA_HW_FLAG_NO_REPORT_DB_MODE			= 0x20,
	IPA_HW_FLAG_NO_START_OOB_TIMER			= 0x40
};

/**
 * struct ipa_uc_ctx - IPA uC context
 * @uc_inited: Indicates if uC interface has been initialized
 * @uc_loaded: Indicates if uC has loaded
 * @uc_failed: Indicates if uC has failed / returned an error
 * @uc_lock: uC interface lock to allow only one uC interaction at a time
 * @uc_completation: Completion mechanism to wait for uC commands
 * @uc_sram_mmio: Pointer to uC mapped memory
 * @pending_cmd: The last command sent waiting to be ACKed
 * @uc_status: The last status provided by the uC
 * @uc_zip_error: uC has notified the APPS upon a ZIP engine error
 * @uc_error_type: error type from uC error event
 */
struct ipa_uc_ctx {
	bool uc_inited;
	bool uc_loaded;
	bool uc_failed;
	struct mutex uc_lock;
	struct completion uc_completion;
	struct IpaHwSharedMemCommonMapping_t *uc_sram_mmio;
	struct IpaHwEventLogInfoData_t *uc_event_top_mmio;
	u32 uc_event_top_ofst;
	u32 pending_cmd;
	u32 uc_status;
	bool uc_zip_error;
	u32 uc_error_type;
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
 * struct ipa_uc_wdi_ctx
 * @wdi_uc_top_ofst:
 * @wdi_uc_top_mmio:
 * @wdi_uc_stats_ofst:
 * @wdi_uc_stats_mmio:
 */
struct ipa_uc_wdi_ctx {
	/* WDI specific fields */
	u32 wdi_uc_stats_ofst;
	struct IpaHwStatsWDIInfoData_t *wdi_uc_stats_mmio;
	void *priv;
	ipa_uc_ready_cb uc_ready_cb;
};

/**
 * struct ipa_sps_pm - SPS power management related members
 * @dec_clients: true if need to decrease active clients count
 * @eot_activity: represent EOT interrupt activity to determine to reset
 *  the inactivity timer
 * @sps_pm_lock: Lock to protect the sps_pm functionality.
 */
struct ipa_sps_pm {
	atomic_t dec_clients;
	atomic_t eot_activity;
	struct mutex sps_pm_lock;
};

/**
 * struct ipacm_client_info - the client-info indicated from IPACM
 * @ipacm_client_enum: the enum to indicate tether-client
 * @ipacm_client_uplink: the bool to indicate pipe for uplink
 */
struct ipacm_client_info {
	enum ipacm_client_enum client_enum;
	bool uplink;
};

/**
 * struct ipa_context - IPA context
 * @class: pointer to the struct class
 * @dev_num: device number
 * @dev: the dev_t of the device
 * @cdev: cdev of the device
 * @bam_handle: IPA driver's BAM handle
 * @ep: list of all end points
 * @skip_ep_cfg_shadow: state to update filter table correctly across
  power-save
 * @resume_on_connect: resume ep on ipa_connect
 * @flt_tbl: list of all IPA filter tables
 * @mode: IPA operating mode
 * @mmio: iomem
 * @ipa_wrapper_base: IPA wrapper base address
 * @glob_flt_tbl: global filter table
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
 * @lock: this does NOT protect the linked lists within ipa_sys_context
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
 * @sps_power_mgmt_wq: workqueue SPS related power management
 * @tag_process_before_gating: indicates whether to start tag process before
 *  gating IPA clocks
 * @sps_pm: sps power management related information
 * @disconnect_lock: protects LAN_CONS packet receive notification CB
 * @pipe_mem_pool: pipe memory pool
 * @dma_pool: special purpose DMA pool
 * @ipa_active_clients: structure for reference counting connected IPA clients
 * @ipa_hw_type: type of IPA HW type (e.g. IPA 1.0, IPA 1.1 etc')
 * @ipa_hw_mode: mode of IPA HW mode (e.g. Normal, Virtual or over PCIe)
 * @use_ipa_teth_bridge: use tethering bridge driver
 * @ipa_bam_remote_mode: ipa bam is in remote mode
 * @modem_cfg_emb_pipe_flt: modem configure embedded pipe filtering rules
 * @ipa_wdi2: using wdi-2.0
 * @ipa_bus_hdl: msm driver handle for the data path bus
 * @ctrl: holds the core specific operations based on
 *  core version (vtable like)
 * @enable_clock_scaling: clock scaling is enabled ?
 * @curr_ipa_clk_rate: ipa_clk current rate
 * @wcstats: wlan common buffer stats
 * @uc_ctx: uC interface context
 * @uc_wdi_ctx: WDI specific fields for uC interface
 * @ipa_num_pipes: The number of pipes used by IPA HW
 * @skip_uc_pipe_reset: Indicates whether pipe reset via uC needs to be avoided
 * @ipa_client_apps_wan_cons_agg_gro: RMNET_IOCTL_INGRESS_FORMAT_AGG_DATA
 * @w_lock: Indicates the wakeup source.
 * @wakelock_ref_cnt: Indicates the number of times wakelock is acquired

 * IPA context - holds all relevant info about IPA driver and its state
 */
struct ipa_context {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	unsigned long bam_handle;
	struct ipa_ep_context ep[IPA_MAX_NUM_PIPES];
	bool skip_ep_cfg_shadow[IPA_MAX_NUM_PIPES];
	bool resume_on_connect[IPA_CLIENT_MAX];
	struct ipa_flt_tbl flt_tbl[IPA_MAX_NUM_PIPES][IPA_IP_MAX];
	void __iomem *mmio;
	u32 ipa_wrapper_base;
	u32 ipa_wrapper_size;
	struct ipa_flt_tbl glob_flt_tbl[IPA_IP_MAX];
	struct ipa_hdr_tbl hdr_tbl;
	struct ipa_hdr_proc_ctx_tbl hdr_proc_ctx_tbl;
	struct ipa_rt_tbl_set rt_tbl_set[IPA_IP_MAX];
	struct ipa_rt_tbl_set reap_rt_tbl_set[IPA_IP_MAX];
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
	struct ipa_nat_mem nat_mem;
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
	bool ip4_rt_tbl_lcl;
	bool ip6_rt_tbl_lcl;
	bool ip4_flt_tbl_lcl;
	bool ip6_flt_tbl_lcl;
	struct ipa_mem_buffer empty_rt_tbl_mem;
	struct gen_pool *pipe_mem_pool;
	struct dma_pool *dma_pool;
	struct ipa_active_clients ipa_active_clients;
	struct ipa2_active_clients_log_ctx ipa2_active_clients_logging;
	struct workqueue_struct *power_mgmt_wq;
	struct workqueue_struct *sps_power_mgmt_wq;
	bool tag_process_before_gating;
	struct ipa_sps_pm sps_pm;
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
	enum ipa_hw_mode ipa_hw_mode;
	bool use_ipa_teth_bridge;
	bool ipa_bam_remote_mode;
	bool modem_cfg_emb_pipe_flt;
	bool ipa_wdi2;
	/* featurize if memory footprint becomes a concern */
	struct ipa_stats stats;
	void *smem_pipe_mem;
	u32 ipa_bus_hdl;
	struct ipa_controller *ctrl;
	struct idr ipa_idr;
	struct device *pdev;
	struct device *uc_pdev;
	spinlock_t idr_lock;
	u32 enable_clock_scaling;
	u32 curr_ipa_clk_rate;
	bool q6_proxy_clk_vote_valid;
	u32 ipa_num_pipes;

	struct ipa_wlan_comm_memb wc_memb;

	struct ipa_uc_ctx uc_ctx;

	struct ipa_uc_wdi_ctx uc_wdi_ctx;
	struct ipa_uc_ntn_ctx uc_ntn_ctx;
	u32 wan_rx_ring_size;
	u32 lan_rx_ring_size;
	bool skip_uc_pipe_reset;
	bool smmu_present;
	bool smmu_s1_bypass;
	unsigned long peer_bam_iova;
	phys_addr_t peer_bam_pa;
	u32 peer_bam_map_size;
	unsigned long peer_bam_dev;
	u32 peer_bam_map_cnt;
	u32 wdi_map_cnt;
	bool use_dma_zone;
	struct wakeup_source w_lock;
	struct ipa_wakelock_ref_cnt wakelock_ref_cnt;

	/* RMNET_IOCTL_INGRESS_FORMAT_AGG_DATA */
	bool ipa_client_apps_wan_cons_agg_gro;
	/* M-release support to know client pipes */
	struct ipacm_client_info ipacm_client[IPA_MAX_NUM_PIPES];
	bool tethered_flow_control;
	u32 ipa_rx_min_timeout_usec;
	u32 ipa_rx_max_timeout_usec;
	u32 ipa_polling_iteration;
};

/**
 * struct ipa_route - IPA route
 * @route_dis: route disable
 * @route_def_pipe: route default pipe
 * @route_def_hdr_table: route default header table
 * @route_def_hdr_ofst: route default header offset table
 * @route_frag_def_pipe: Default pipe to route fragmented exception
 *    packets and frag new rule statues, if source pipe does not have
 *    a notification status pipe defined.
 */
struct ipa_route {
	u32 route_dis;
	u32 route_def_pipe;
	u32 route_def_hdr_table;
	u32 route_def_hdr_ofst;
	u8  route_frag_def_pipe;
};

/**
 * enum ipa_pipe_mem_type - IPA pipe memory type
 * @IPA_SPS_PIPE_MEM: Default, SPS dedicated pipe memory
 * @IPA_PRIVATE_MEM: IPA's private memory
 * @IPA_SYSTEM_MEM: System RAM, requires allocation
 */
enum ipa_pipe_mem_type {
	IPA_SPS_PIPE_MEM = 0,
	IPA_PRIVATE_MEM  = 1,
	IPA_SYSTEM_MEM   = 2,
};

struct ipa_plat_drv_res {
	bool use_ipa_teth_bridge;
	u32 ipa_mem_base;
	u32 ipa_mem_size;
	u32 bam_mem_base;
	u32 bam_mem_size;
	u32 ipa_irq;
	u32 bam_irq;
	u32 ipa_pipe_mem_start_ofst;
	u32 ipa_pipe_mem_size;
	enum ipa_hw_type ipa_hw_type;
	enum ipa_hw_mode ipa_hw_mode;
	u32 ee;
	bool ipa_bam_remote_mode;
	bool modem_cfg_emb_pipe_flt;
	bool ipa_wdi2;
	u32 wan_rx_ring_size;
	u32 lan_rx_ring_size;
	bool skip_uc_pipe_reset;
	bool use_dma_zone;
	bool tethered_flow_control;
	u32 ipa_rx_polling_sleep_msec;
	u32 ipa_polling_iteration;
};

struct ipa_mem_partition {
	u16 ofst_start;
	u16 nat_ofst;
	u16 nat_size;
	u16 v4_flt_ofst;
	u16 v4_flt_size;
	u16 v4_flt_size_ddr;
	u16 v6_flt_ofst;
	u16 v6_flt_size;
	u16 v6_flt_size_ddr;
	u16 v4_rt_ofst;
	u16 v4_num_index;
	u16 v4_modem_rt_index_lo;
	u16 v4_modem_rt_index_hi;
	u16 v4_apps_rt_index_lo;
	u16 v4_apps_rt_index_hi;
	u16 v4_rt_size;
	u16 v4_rt_size_ddr;
	u16 v6_rt_ofst;
	u16 v6_num_index;
	u16 v6_modem_rt_index_lo;
	u16 v6_modem_rt_index_hi;
	u16 v6_apps_rt_index_lo;
	u16 v6_apps_rt_index_hi;
	u16 v6_rt_size;
	u16 v6_rt_size_ddr;
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
	u16 apps_v4_flt_ofst;
	u16 apps_v4_flt_size;
	u16 apps_v6_flt_ofst;
	u16 apps_v6_flt_size;
	u16 uc_info_ofst;
	u16 uc_info_size;
	u16 end_ofst;
	u16 apps_v4_rt_ofst;
	u16 apps_v4_rt_size;
	u16 apps_v6_rt_ofst;
	u16 apps_v6_rt_size;
};

struct ipa_controller {
	struct ipa_mem_partition mem_partition;
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
	void (*ipa_cfg_ep_hdr)(u32 pipe_number,
			const struct ipa_ep_cfg_hdr *ipa_ep_hdr_cfg);
	int (*ipa_cfg_ep_hdr_ext)(u32 pipe_number,
		const struct ipa_ep_cfg_hdr_ext *ipa_ep_hdr_ext_cfg);
	void (*ipa_cfg_ep_aggr)(u32 pipe_number,
			const struct ipa_ep_cfg_aggr *ipa_ep_agrr_cfg);
	int (*ipa_cfg_ep_deaggr)(u32 pipe_index,
			const struct ipa_ep_cfg_deaggr *ep_deaggr);
	void (*ipa_cfg_ep_nat)(u32 pipe_number,
			const struct ipa_ep_cfg_nat *ipa_ep_nat_cfg);
	void (*ipa_cfg_ep_mode)(u32 pipe_number, u32 dst_pipe_number,
			const struct ipa_ep_cfg_mode *ep_mode);
	void (*ipa_cfg_ep_route)(u32 pipe_index, u32 rt_tbl_index);
	void (*ipa_cfg_ep_holb)(u32 pipe_index,
			const struct ipa_ep_cfg_holb *ep_holb);
	void (*ipa_cfg_route)(struct ipa_route *route);
	int (*ipa_read_gen_reg)(char *buff, int max_len);
	int (*ipa_read_ep_reg)(char *buff, int max_len, int pipe);
	void (*ipa_write_dbg_cnt)(int option);
	int (*ipa_read_dbg_cnt)(char *buf, int max_len);
	void (*ipa_cfg_ep_status)(u32 clnt_hdl,
			const struct ipa_ep_cfg_status *ep_status);
	int (*ipa_commit_flt)(enum ipa_ip_type ip);
	int (*ipa_commit_rt)(enum ipa_ip_type ip);
	int (*ipa_generate_rt_hw_rule)(enum ipa_ip_type ip,
		struct ipa_rt_entry *entry, u8 *buf);
	int (*ipa_commit_hdr)(void);
	void (*ipa_cfg_ep_cfg)(u32 clnt_hdl,
			const struct ipa_ep_cfg_cfg *cfg);
	void (*ipa_cfg_ep_metadata_mask)(u32 clnt_hdl,
			const struct ipa_ep_cfg_metadata_mask *metadata_mask);
	void (*ipa_enable_clks)(void);
	void (*ipa_disable_clks)(void);
	struct msm_bus_scale_pdata *msm_bus_data_ptr;

	void (*ipa_cfg_ep_metadata)(u32 pipe_number,
			const struct ipa_ep_cfg_metadata *);
};

extern struct ipa_context *ipa_ctx;

/* public APIs */
/*
 * Connect / Disconnect
 */
int ipa2_connect(const struct ipa_connect_params *in,
		struct ipa_sps_params *sps, u32 *clnt_hdl);
int ipa2_disconnect(u32 clnt_hdl);

/*
 * Resume / Suspend
 */
int ipa2_reset_endpoint(u32 clnt_hdl);

/*
 * Remove ep delay
 */
int ipa2_clear_endpoint_delay(u32 clnt_hdl);

/*
 * Disable ep
 */
int ipa2_disable_endpoint(u32 clnt_hdl);

/*
 * Configuration
 */
int ipa2_cfg_ep(u32 clnt_hdl, const struct ipa_ep_cfg *ipa_ep_cfg);

int ipa2_cfg_ep_nat(u32 clnt_hdl, const struct ipa_ep_cfg_nat *ipa_ep_cfg);

int ipa2_cfg_ep_hdr(u32 clnt_hdl, const struct ipa_ep_cfg_hdr *ipa_ep_cfg);

int ipa2_cfg_ep_hdr_ext(u32 clnt_hdl,
			const struct ipa_ep_cfg_hdr_ext *ipa_ep_cfg);

int ipa2_cfg_ep_mode(u32 clnt_hdl, const struct ipa_ep_cfg_mode *ipa_ep_cfg);

int ipa2_cfg_ep_aggr(u32 clnt_hdl, const struct ipa_ep_cfg_aggr *ipa_ep_cfg);

int ipa2_cfg_ep_deaggr(u32 clnt_hdl,
		      const struct ipa_ep_cfg_deaggr *ipa_ep_cfg);

int ipa2_cfg_ep_route(u32 clnt_hdl, const struct ipa_ep_cfg_route *ipa_ep_cfg);

int ipa2_cfg_ep_holb(u32 clnt_hdl, const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa2_cfg_ep_cfg(u32 clnt_hdl, const struct ipa_ep_cfg_cfg *ipa_ep_cfg);

int ipa2_cfg_ep_metadata_mask(u32 clnt_hdl,
	const struct ipa_ep_cfg_metadata_mask *ipa_ep_cfg);

int ipa2_cfg_ep_holb_by_client(enum ipa_client_type client,
				const struct ipa_ep_cfg_holb *ipa_ep_cfg);

int ipa2_cfg_ep_ctrl(u32 clnt_hdl, const struct ipa_ep_cfg_ctrl *ep_ctrl);

/*
 * Header removal / addition
 */
int ipa2_add_hdr(struct ipa_ioc_add_hdr *hdrs);

int ipa2_del_hdr(struct ipa_ioc_del_hdr *hdls);

int ipa2_commit_hdr(void);

int ipa2_reset_hdr(void);

int ipa2_get_hdr(struct ipa_ioc_get_hdr *lookup);

int ipa2_put_hdr(u32 hdr_hdl);

int ipa2_copy_hdr(struct ipa_ioc_copy_hdr *copy);

/*
 * Header Processing Context
 */
int ipa2_add_hdr_proc_ctx(struct ipa_ioc_add_hdr_proc_ctx *proc_ctxs);

int ipa2_del_hdr_proc_ctx(struct ipa_ioc_del_hdr_proc_ctx *hdls);

/*
 * Routing
 */
int ipa2_add_rt_rule(struct ipa_ioc_add_rt_rule *rules);

int ipa2_del_rt_rule(struct ipa_ioc_del_rt_rule *hdls);

int ipa2_commit_rt(enum ipa_ip_type ip);

int ipa2_reset_rt(enum ipa_ip_type ip);

int ipa2_get_rt_tbl(struct ipa_ioc_get_rt_tbl *lookup);

int ipa2_put_rt_tbl(u32 rt_tbl_hdl);

int ipa2_query_rt_index(struct ipa_ioc_get_rt_tbl_indx *in);

int ipa2_mdfy_rt_rule(struct ipa_ioc_mdfy_rt_rule *rules);

/*
 * Filtering
 */
int ipa2_add_flt_rule(struct ipa_ioc_add_flt_rule *rules);

int ipa2_del_flt_rule(struct ipa_ioc_del_flt_rule *hdls);

int ipa2_mdfy_flt_rule(struct ipa_ioc_mdfy_flt_rule *rules);

int ipa2_commit_flt(enum ipa_ip_type ip);

int ipa2_reset_flt(enum ipa_ip_type ip);

/*
 * NAT
 */
int ipa2_allocate_nat_device(struct ipa_ioc_nat_alloc_mem *mem);

int ipa2_nat_init_cmd(struct ipa_ioc_v4_nat_init *init);

int ipa2_nat_dma_cmd(struct ipa_ioc_nat_dma_cmd *dma);

int ipa2_nat_del_cmd(struct ipa_ioc_v4_nat_del *del);

/*
 * Messaging
 */
int ipa2_send_msg(struct ipa_msg_meta *meta, void *buff,
		  ipa_msg_free_fn callback);
int ipa2_register_pull_msg(struct ipa_msg_meta *meta, ipa_msg_pull_fn callback);
int ipa2_deregister_pull_msg(struct ipa_msg_meta *meta);

/*
 * Interface
 */
int ipa2_register_intf(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx);
int ipa2_register_intf_ext(const char *name, const struct ipa_tx_intf *tx,
		       const struct ipa_rx_intf *rx,
		       const struct ipa_ext_intf *ext);
int ipa2_deregister_intf(const char *name);

/*
 * Aggregation
 */
int ipa2_set_aggr_mode(enum ipa_aggr_mode mode);

int ipa2_set_qcncm_ndp_sig(char sig[3]);

int ipa2_set_single_ndp_per_mbim(bool enable);

/*
 * Data path
 */
int ipa2_tx_dp(enum ipa_client_type dst, struct sk_buff *skb,
		struct ipa_tx_meta *metadata);

/*
 * To transfer multiple data packets
 * While passing the data descriptor list, the anchor node
 * should be of type struct ipa_tx_data_desc not list_head
*/
int ipa2_tx_dp_mul(enum ipa_client_type dst,
			struct ipa_tx_data_desc *data_desc);

void ipa2_free_skb(struct ipa_rx_data *);

/*
 * System pipes
 */
int ipa2_setup_sys_pipe(struct ipa_sys_connect_params *sys_in, u32 *clnt_hdl);

int ipa2_teardown_sys_pipe(u32 clnt_hdl);

int ipa2_sys_setup(struct ipa_sys_connect_params *sys_in,
	unsigned long *ipa_bam_hdl,
	u32 *ipa_pipe_num, u32 *clnt_hdl, bool en_status);

int ipa2_sys_teardown(u32 clnt_hdl);

int ipa2_sys_update_gsi_hdls(u32 clnt_hdl, unsigned long gsi_ch_hdl,
	unsigned long gsi_ev_hdl);

int ipa2_connect_wdi_pipe(struct ipa_wdi_in_params *in,
		struct ipa_wdi_out_params *out);
int ipa2_disconnect_wdi_pipe(u32 clnt_hdl);
int ipa2_enable_wdi_pipe(u32 clnt_hdl);
int ipa2_disable_wdi_pipe(u32 clnt_hdl);
int ipa2_resume_wdi_pipe(u32 clnt_hdl);
int ipa2_suspend_wdi_pipe(u32 clnt_hdl);
int ipa2_get_wdi_stats(struct IpaHwStatsWDIInfoData_t *stats);
u16 ipa2_get_smem_restr_bytes(void);
int ipa2_setup_uc_ntn_pipes(struct ipa_ntn_conn_in_params *inp,
		ipa_notify_cb notify, void *priv, u8 hdr_len,
		struct ipa_ntn_conn_out_params *outp);
int ipa2_tear_down_uc_offload_pipes(int ipa_ep_idx_ul, int ipa_ep_idx_dl);

/*
 * To retrieve doorbell physical address of
 * wlan pipes
 */
int ipa2_uc_wdi_get_dbpa(struct ipa_wdi_db_params *out);

/*
 * To register uC ready callback if uC not ready
 * and also check uC readiness
 * if uC not ready only, register callback
 */
int ipa2_uc_reg_rdyCB(struct ipa_wdi_uc_ready_params *param);
/*
 * To de-register uC ready callback
 */
int ipa2_uc_dereg_rdyCB(void);

/*
 * Tethering bridge (Rmnet / MBIM)
 */
int ipa2_teth_bridge_init(struct teth_bridge_init_params *params);

int ipa2_teth_bridge_disconnect(enum ipa_client_type client);

int ipa2_teth_bridge_connect(struct teth_bridge_connect_params *connect_params);

/*
 * Tethering client info
 */
void ipa2_set_client(int index, enum ipacm_client_enum client, bool uplink);

enum ipacm_client_enum ipa2_get_client(int pipe_idx);

bool ipa2_get_client_uplink(int pipe_idx);

/*
 * IPADMA
 */
int ipa2_dma_init(void);

int ipa2_dma_enable(void);

int ipa2_dma_disable(void);

int ipa2_dma_sync_memcpy(u64 dest, u64 src, int len);

int ipa2_dma_async_memcpy(u64 dest, u64 src, int len,
			void (*user_cb)(void *user1), void *user_param);

int ipa2_dma_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len);

void ipa2_dma_destroy(void);

/*
 * MHI APIs for IPA MHI client driver
 */
int ipa2_init_mhi(struct ipa_mhi_init_params *params);

int ipa2_mhi_init_engine(struct ipa_mhi_init_engine *params);

int ipa2_connect_mhi_pipe(struct ipa_mhi_connect_params_internal *in,
		u32 *clnt_hdl);

int ipa2_disconnect_mhi_pipe(u32 clnt_hdl);

bool ipa2_mhi_sps_channel_empty(enum ipa_client_type client);

int ipa2_disable_sps_pipe(enum ipa_client_type client);

int ipa2_mhi_reset_channel_internal(enum ipa_client_type client);

int ipa2_mhi_start_channel_internal(enum ipa_client_type client);

int ipa2_mhi_suspend_ul_channels(void);

int ipa2_mhi_resume_channels_internal(enum ipa_client_type client,
		bool LPTransitionRejected, bool brstmode_enabled,
		union __packed gsi_channel_scratch ch_scratch, u8 index);

/*
 * mux id
 */
int ipa2_write_qmap_id(struct ipa_ioc_write_qmapid *param_in);

/*
 * interrupts
 */
int ipa2_add_interrupt_handler(enum ipa_irq_type interrupt,
		ipa_irq_handler_t handler,
		bool deferred_flag,
		void *private_data);

int ipa2_remove_interrupt_handler(enum ipa_irq_type interrupt);

/*
 * Miscellaneous
 */
void ipa2_bam_reg_dump(void);

int ipa2_get_ep_mapping(enum ipa_client_type client);

bool ipa2_is_ready(void);

void ipa2_proxy_clk_vote(void);
void ipa2_proxy_clk_unvote(void);

bool ipa2_is_client_handle_valid(u32 clnt_hdl);

enum ipa_client_type ipa2_get_client_mapping(int pipe_idx);

enum ipa_rm_resource_name ipa2_get_rm_resource_from_ep(int pipe_idx);

bool ipa2_get_modem_cfg_emb_pipe_flt(void);

/* internal functions */

int ipa2_bind_api_controller(enum ipa_hw_type ipa_hw_type,
	struct ipa_api_controller *api_ctrl);

int ipa_send_one(struct ipa_sys_context *sys, struct ipa_desc *desc,
		bool in_atomic);
int ipa_send(struct ipa_sys_context *sys, u32 num_desc, struct ipa_desc *desc,
		bool in_atomic);
int ipa2_get_ep_mapping(enum ipa_client_type client);

int ipa_generate_hw_rule(enum ipa_ip_type ip,
			 const struct ipa_rule_attrib *attrib,
			 u8 **buf,
			 u16 *en_rule);
int ipa_init_hw(void);
struct ipa_rt_tbl *__ipa_find_rt_tbl(enum ipa_ip_type ip, const char *name);
int ipa_set_single_ndp_per_mbim(bool);
int ipa_set_hw_timer_fix_for_mbim_aggr(bool);
void ipa_debugfs_init(void);
void ipa_debugfs_remove(void);

void ipa_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size);

void ipa_rx_timeout_min_max_calc(u32 *min, u32 *max, s8 time);

#ifdef IPA_DEBUG
#define IPA_DUMP_BUFF(base, phy_base, size) \
	ipa_dump_buff_internal(base, phy_base, size)
#else
#define IPA_DUMP_BUFF(base, phy_base, size)
#endif
int ipa_controller_static_bind(struct ipa_controller *controller,
		enum ipa_hw_type ipa_hw_type);
int ipa_cfg_route(struct ipa_route *route);
int ipa_send_cmd(u16 num_desc, struct ipa_desc *descr);
int ipa_cfg_filter(u32 disable);
int ipa_pipe_mem_init(u32 start_ofst, u32 size);
int ipa_pipe_mem_alloc(u32 *ofst, u32 size);
int ipa_pipe_mem_free(u32 ofst, u32 size);
int ipa_straddle_boundary(u32 start, u32 end, u32 boundary);
struct ipa_context *ipa_get_ctx(void);
void ipa_enable_clks(void);
void ipa_disable_clks(void);
void ipa2_inc_client_enable_clks(struct ipa_active_client_logging_info *id);
int ipa2_inc_client_enable_clks_no_block(struct ipa_active_client_logging_info
		*id);
void ipa2_dec_client_disable_clks(struct ipa_active_client_logging_info *id);
void ipa2_active_clients_log_dec(struct ipa_active_client_logging_info *id,
		bool int_ctx);
void ipa2_active_clients_log_inc(struct ipa_active_client_logging_info *id,
		bool int_ctx);
int ipa2_active_clients_log_print_buffer(char *buf, int size);
int ipa2_active_clients_log_print_table(char *buf, int size);
void ipa2_active_clients_log_clear(void);
int ipa_interrupts_init(u32 ipa_irq, u32 ee, struct device *ipa_dev);
int __ipa_del_rt_rule(u32 rule_hdl);
int __ipa_del_hdr(u32 hdr_hdl);
int __ipa_release_hdr(u32 hdr_hdl);
int __ipa_release_hdr_proc_ctx(u32 proc_ctx_hdl);
int _ipa_read_gen_reg_v1_1(char *buff, int max_len);
int _ipa_read_gen_reg_v2_0(char *buff, int max_len);
int _ipa_read_ep_reg_v1_1(char *buf, int max_len, int pipe);
int _ipa_read_ep_reg_v2_0(char *buf, int max_len, int pipe);
void _ipa_write_dbg_cnt_v1_1(int option);
void _ipa_write_dbg_cnt_v2_0(int option);
int _ipa_read_dbg_cnt_v1_1(char *buf, int max_len);
int _ipa_read_dbg_cnt_v2_0(char *buf, int max_len);
void _ipa_enable_clks_v1_1(void);
void _ipa_enable_clks_v2_0(void);
void _ipa_disable_clks_v1_1(void);
void _ipa_disable_clks_v2_0(void);

static inline u32 ipa_read_reg(void *base, u32 offset)
{
	return ioread32(base + offset);
}

static inline u32 ipa_read_reg_field(void *base, u32 offset,
		u32 mask, u32 shift)
{
	return (ipa_read_reg(base, offset) & mask) >> shift;
}

static inline void ipa_write_reg(void *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
}

int ipa_bridge_init(void);
void ipa_bridge_cleanup(void);

ssize_t ipa_read(struct file *filp, char __user *buf, size_t count,
		 loff_t *f_pos);
int ipa_pull_msg(struct ipa_msg_meta *meta, char *buff, size_t count);
int ipa_query_intf(struct ipa_ioc_query_intf *lookup);
int ipa_query_intf_tx_props(struct ipa_ioc_query_intf_tx_props *tx);
int ipa_query_intf_rx_props(struct ipa_ioc_query_intf_rx_props *rx);
int ipa_query_intf_ext_props(struct ipa_ioc_query_intf_ext_props *ext);

void wwan_cleanup(void);

int teth_bridge_driver_init(void);
void ipa_lan_rx_cb(void *priv, enum ipa_dp_evt_type evt, unsigned long data);

int _ipa_init_sram_v2(void);
int _ipa_init_sram_v2_5(void);
int _ipa_init_sram_v2_6L(void);
int _ipa_init_hdr_v2(void);
int _ipa_init_hdr_v2_5(void);
int _ipa_init_hdr_v2_6L(void);
int _ipa_init_rt4_v2(void);
int _ipa_init_rt6_v2(void);
int _ipa_init_flt4_v2(void);
int _ipa_init_flt6_v2(void);

int __ipa_commit_flt_v1_1(enum ipa_ip_type ip);
int __ipa_commit_flt_v2(enum ipa_ip_type ip);
int __ipa_commit_rt_v1_1(enum ipa_ip_type ip);
int __ipa_commit_rt_v2(enum ipa_ip_type ip);
int __ipa_generate_rt_hw_rule_v2(enum ipa_ip_type ip,
	struct ipa_rt_entry *entry, u8 *buf);
int __ipa_generate_rt_hw_rule_v2_5(enum ipa_ip_type ip,
	struct ipa_rt_entry *entry, u8 *buf);
int __ipa_generate_rt_hw_rule_v2_6L(enum ipa_ip_type ip,
	struct ipa_rt_entry *entry, u8 *buf);

int __ipa_commit_hdr_v1_1(void);
int __ipa_commit_hdr_v2(void);
int __ipa_commit_hdr_v2_5(void);
int __ipa_commit_hdr_v2_6L(void);
int ipa_generate_flt_eq(enum ipa_ip_type ip,
		const struct ipa_rule_attrib *attrib,
		struct ipa_ipfltri_rule_eq *eq_attrib);
void ipa_skb_recycle(struct sk_buff *skb);
void ipa_install_dflt_flt_rules(u32 ipa_ep_idx);
void ipa_delete_dflt_flt_rules(u32 ipa_ep_idx);

int ipa_enable_data_path(u32 clnt_hdl);
int ipa_disable_data_path(u32 clnt_hdl);
int ipa_id_alloc(void *ptr);
void *ipa_id_find(u32 id);
void ipa_id_remove(u32 id);

int ipa2_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
				  u32 bandwidth_mbps);

int ipa2_cfg_ep_status(u32 clnt_hdl,
			const struct ipa_ep_cfg_status *ipa_ep_cfg);
int ipa_cfg_aggr_cntr_granularity(u8 aggr_granularity);
int ipa_cfg_eot_coal_cntr_granularity(u8 eot_coal_granularity);

int ipa2_suspend_resource_no_block(enum ipa_rm_resource_name name);
int ipa2_suspend_resource_sync(enum ipa_rm_resource_name name);
int ipa2_resume_resource(enum ipa_rm_resource_name name);
bool ipa_should_pipe_be_suspended(enum ipa_client_type client);
int ipa_tag_aggr_force_close(int pipe_num);

void ipa_active_clients_lock(void);
int ipa_active_clients_trylock(unsigned long *flags);
void ipa_active_clients_unlock(void);
void ipa_active_clients_trylock_unlock(unsigned long *flags);
int ipa_wdi_init(void);
int ipa_write_qmapid_wdi_pipe(u32 clnt_hdl, u8 qmap_id);
int ipa_tag_process(struct ipa_desc *desc, int num_descs,
		    unsigned long timeout);

int ipa_q6_pre_shutdown_cleanup(void);
int ipa_q6_post_shutdown_cleanup(void);
int ipa_init_q6_smem(void);
int ipa_q6_monitor_holb_mitigation(bool enable);

int ipa_sps_connect_safe(struct sps_pipe *h, struct sps_connect *connect,
			 enum ipa_client_type ipa_client);

int ipa_uc_interface_init(void);
int ipa_uc_reset_pipe(enum ipa_client_type ipa_client);
int ipa_uc_monitor_holb(enum ipa_client_type ipa_client, bool enable);
int ipa2_uc_state_check(void);
int ipa_uc_loaded_check(void);
int ipa_uc_send_cmd(u32 cmd, u32 opcode, u32 expected_status,
		    bool polling_mode, unsigned long timeout_jiffies);
void ipa_register_panic_hdlr(void);
void ipa_uc_register_handlers(enum ipa_hw_features feature,
			      struct ipa_uc_hdlrs *hdlrs);
int create_nat_device(void);
int ipa_uc_notify_clk_state(bool enabled);
void ipa_dma_async_memcpy_notify_cb(void *priv,
		enum ipa_dp_evt_type evt, unsigned long data);

int ipa_uc_update_hw_flags(u32 flags);

int ipa2_uc_mhi_init(void (*ready_cb)(void), void (*wakeup_request_cb)(void));
void ipa2_uc_mhi_cleanup(void);
int ipa2_uc_mhi_send_dl_ul_sync_info(union IpaHwMhiDlUlSyncCmdData_t *cmd);
int ipa_uc_mhi_init_engine(struct ipa_mhi_msi_info *msi, u32 mmio_addr,
	u32 host_ctrl_addr, u32 host_data_addr, u32 first_ch_idx,
	u32 first_evt_idx);
int ipa_uc_mhi_init_channel(int ipa_ep_idx, int channelHandle,
	int contexArrayIndex, int channelDirection);
int ipa2_uc_mhi_reset_channel(int channelHandle);
int ipa2_uc_mhi_suspend_channel(int channelHandle);
int ipa_uc_mhi_resume_channel(int channelHandle, bool LPTransitionRejected);
int ipa2_uc_mhi_stop_event_update_channel(int channelHandle);
int ipa2_uc_mhi_print_stats(char *dbg_buff, int size);
int ipa_uc_memcpy(phys_addr_t dest, phys_addr_t src, int len);
u32 ipa_get_num_pipes(void);
u32 ipa_get_sys_yellow_wm(struct ipa_sys_context *sys);
struct ipa_smmu_cb_ctx *ipa2_get_smmu_ctx(void);
struct ipa_smmu_cb_ctx *ipa2_get_wlan_smmu_ctx(void);
struct ipa_smmu_cb_ctx *ipa2_get_uc_smmu_ctx(void);
struct iommu_domain *ipa_get_uc_smmu_domain(void);
struct iommu_domain *ipa2_get_wlan_smmu_domain(void);
int ipa2_ap_suspend(struct device *dev);
int ipa2_ap_resume(struct device *dev);
struct iommu_domain *ipa2_get_smmu_domain(void);
struct device *ipa2_get_dma_dev(void);
int ipa2_release_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info);
int ipa2_create_wdi_mapping(u32 num_buffers, struct ipa_wdi_buffer_info *info);
void ipa_suspend_apps_pipes(bool suspend);
void ipa_update_repl_threshold(enum ipa_client_type ipa_client);
void ipa_flow_control(enum ipa_client_type ipa_client, bool enable,
			uint32_t qmap_id);
int ipa2_restore_suspend_handler(void);
void ipa_sps_irq_control_all(bool enable);
void ipa_inc_acquire_wakelock(enum ipa_wakelock_ref_client ref_client);
void ipa_dec_release_wakelock(enum ipa_wakelock_ref_client ref_client);
int ipa_iommu_map(struct iommu_domain *domain, unsigned long iova,
	phys_addr_t paddr, size_t size, int prot);
int ipa2_rx_poll(u32 clnt_hdl, int budget);
void ipa2_recycle_wan_skb(struct sk_buff *skb);
int ipa_ntn_init(void);
int ipa2_get_ntn_stats(struct IpaHwStatsNTNInfoData_t *stats);
int ipa2_register_ipa_ready_cb(void (*ipa_ready_cb)(void *),
				void *user_data);
#endif /* _IPA_I_H_ */
