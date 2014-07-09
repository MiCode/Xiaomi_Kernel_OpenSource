/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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
#include "ipa_hw_defs.h"
#include "ipa_ram_mmap.h"
#include "ipa_reg.h"

#define DRV_NAME "ipa"
#define IPA_COOKIE 0x57831603
#define MTU_BYTE 1500

#define IPA_NUM_PIPES 0x14
#define IPA_SYS_DESC_FIFO_SZ 0x800
#define IPA_SYS_TX_DATA_DESC_FIFO_SZ 0x1000
#define IPA_LAN_RX_HEADER_LENGTH (2)
#define IPA_QMAP_HEADER_LENGTH (4)
#define IPA_DL_CHECKSUM_LENGTH (8)
#define IPA_NUM_DESC_PER_SW_TX (2)

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
#define IPA_STATS_INC_CNT(val) do {			\
				++val;			\
			} while (0)
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
#define IPA_IPV4_IS_FRAG		BIT(15)

#define IPA_HDR_BIN0 0
#define IPA_HDR_BIN1 1
#define IPA_HDR_BIN2 2
#define IPA_HDR_BIN3 3
#define IPA_HDR_BIN4 4
#define IPA_HDR_BIN_MAX 5

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

#define IPA_CLIENT_IS_PROD(x) (x >= IPA_CLIENT_PROD && x < IPA_CLIENT_CONS)
#define IPA_CLIENT_IS_CONS(x) (x >= IPA_CLIENT_CONS && x < IPA_CLIENT_MAX)
#define IPA_SETFIELD(val, shift, mask) (((val) << (shift)) & (mask))
#define IPA_SETFIELD_IN_REG(reg, val, shift, mask) \
			(reg |= ((val) << (shift)) & (mask))

#define IPA_HW_TABLE_ALIGNMENT(start_ofst) \
	(((start_ofst) + 127) & ~127)
#define IPA_RT_FLT_HW_RULE_BUF_SIZE	(128)

#define MAX_RESOURCE_TO_CLIENTS (5)
struct ipa_client_names {
	enum ipa_client_type names[MAX_RESOURCE_TO_CLIENTS];
	int length;
};

/**
 * struct ipa_mem_buffer - IPA memory buffer
 * @base: base
 * @phys_base: physical base address
 * @size: size of memory buffer
 */
struct ipa_mem_buffer {
	void *base;
	dma_addr_t phys_base;
	u32 size;
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
 * @ref_cnt: reference counter of raouting table
 * @set: collection of routing tables
 * @cookie: cookie used for validity check
 * @in_sys: flag indicating if the table is located in system memory
 * @sz: the size of the routing table
 * @curr_mem: current routing tables block in sys memory
 * @prev_mem: previous routing table block in sys memory
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
 * @is_partial: flag indicating if header table entry is partial
 * @offset_entry: entry's offset
 * @cookie: cookie used for validity check
 * @ref_cnt: reference counter of raouting table
 * @is_eth2_ofst_valid: is eth2_ofst field valid?
 * @eth2_ofst: offset to start of Ethernet-II/802.3 header
 */
struct ipa_hdr_entry {
	struct list_head link;
	u8 hdr[IPA_HDR_MAX_SIZE];
	u32 hdr_len;
	char name[IPA_RESOURCE_NAME_MAX];
	u8 is_partial;
	struct ipa_hdr_offset_entry *offset_entry;
	u32 cookie;
	u32 ref_cnt;
	int id;
	u8 is_eth2_ofst_valid;
	u16 eth2_ofst;
};

/**
 * struct ipa_hdr_offset_entry - IPA header offset entry
 * @link: entry's link in global header offset entries list
 * @offset: the offset
 * @bin: bin
 */
struct ipa_hdr_offset_entry {
	struct list_head link;
	u32 offset;
	u32 bin;
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
 * @hw_len: the length of the table
 */
struct ipa_rt_entry {
	struct list_head link;
	struct ipa_rt_rule rule;
	u32 cookie;
	struct ipa_rt_tbl *tbl;
	struct ipa_hdr_entry *hdr;
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
 *	specifed Status End Point. Status endpoint needs to be
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
	bool resume_on_connect;
	struct ipa_wlan_stats wstats;
	u32 wdi_state;

	/* sys MUST be the last element of this struct */
	struct ipa_sys_context *sys;
};

enum ipa_sys_pipe_policy {
	IPA_POLICY_INTR_MODE,
	IPA_POLICY_NOINTR_MODE,
	IPA_POLICY_INTR_POLL_MODE,
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
	struct sk_buff *(*get_skb)(unsigned int len, gfp_t flags);
	void (*free_skb)(struct sk_buff *skb);
	u32 rx_buff_sz;
	u32 rx_pool_sz;
	struct sk_buff *prev_skb;
	unsigned int len_rem;
	unsigned int len_pad;
	unsigned int len_partial;
	struct work_struct work;
	void (*sps_callback)(struct sps_event_notify *notify);
	enum sps_option sps_option;
	struct delayed_work replenish_rx_work;

	/* ordering is important - mutable fields go above */
	struct ipa_ep_context *ep;
	struct list_head head_desc_list;
	spinlock_t spinlock;
	struct workqueue_struct *wq;
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
 * >1 and <0xFFFF for first of a "multiple" tranfer,
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
	struct mutex lock;
	void *nat_base_address;
	char *ipv4_rules_addr;
	char *ipv4_expansion_rules_addr;
	char *index_table_addr;
	char *index_table_expansion_addr;
	u32 size_base_tables;
	u32 size_expansion_tables;
	u32 public_ip_addr;
};

/**
 * enum ipa_hw_type - IPA hardware version type
 * @IPA_HW_None: IPA hardware version not defined
 * @IPA_HW_v1_0: IPA hardware version 1.0, corresponding to ELAN 1.0
 * @IPA_HW_v1_1: IPA hardware version 1.1, corresponding to ELAN 2.0
 * @IPA_HW_v2_0: IPA hardware version 2.0
 */
enum ipa_hw_type {
	IPA_HW_None = 0,
	IPA_HW_v1_0 = 1,
	IPA_HW_v1_1 = 2,
	IPA_HW_v2_0 = 3
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
};

struct ipa_active_clients {
	struct mutex mutex;
	spinlock_t spinlock;
	bool mutex_locked;
	int cnt;
};

struct ipa_controller;

struct ipa_wdi_ctx {
	bool uc_loaded;
	bool uc_failed;
	struct IpaHwSharedMemWdiMapping_t *ipa_sram_mmio;
	struct mutex lock;
	struct completion cmd_rsp;
	u32 pending_cmd;
	u32 last_resp;
	struct dma_pool *dma_pool;
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
 * @flt_tbl: list of all IPA filter tables
 * @mode: IPA operating mode
 * @mmio: iomem
 * @ipa_wrapper_base: IPA wrapper base address
 * @glob_flt_tbl: global filter table
 * @hdr_tbl: IPA header table
 * @rt_tbl_set: list of routing tables each of which is a list of rules
 * @reap_rt_tbl_set: list of sys mem routing tables waiting to be reaped
 * @flt_rule_cache: filter rule cache
 * @rt_rule_cache: routing rule cache
 * @hdr_cache: header cache
 * @hdr_offset_cache: header offset cache
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
 * @hdr_mem: header memory
 * @ip4_rt_tbl_lcl: where ip4 rt tables reside 1-local; 0-system
 * @ip6_rt_tbl_lcl: where ip6 rt tables reside 1-local; 0-system
 * @ip4_flt_tbl_lcl: where ip4 flt tables reside 1-local; 0-system
 * @ip6_flt_tbl_lcl: where ip6 flt tables reside 1-local; 0-system
 * @empty_rt_tbl_mem: empty routing tables memory
 * @power_mgmt_wq: workqueue for power management
 * @tag_process_before_gating: indicates whether to start tag process before
 *  gating IPA clocks
 * @pipe_mem_pool: pipe memory pool
 * @dma_pool: special purpose DMA pool
 * @ipa_active_clients: structure for reference counting connected IPA clients
 * @ipa_hw_type: type of IPA HW type (e.g. IPA 1.0, IPA 1.1 etc')
 * @ipa_hw_mode: mode of IPA HW mode (e.g. Normal, Virtual or over PCIe)
 * @use_ipa_teth_bridge: use tethering bridge driver
 * @ipa_bus_hdl: msm driver handle for the data path bus
 * @ctrl: holds the core specific operations based on
 *  core version (vtable like)
 * @enable_clock_scaling: clock scaling is enabled ?
 * @curr_ipa_clk_rate: ipa_clk current rate
 * @wcstats: wlan common buffer stats

 * IPA context - holds all relevant info about IPA driver and its state
 */
struct ipa_context {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	unsigned long bam_handle;
	struct ipa_ep_context ep[IPA_NUM_PIPES];
	bool skip_ep_cfg_shadow[IPA_NUM_PIPES];
	struct ipa_flt_tbl flt_tbl[IPA_NUM_PIPES][IPA_IP_MAX];
	void __iomem *mmio;
	u32 ipa_wrapper_base;
	struct ipa_flt_tbl glob_flt_tbl[IPA_IP_MAX];
	struct ipa_hdr_tbl hdr_tbl;
	struct ipa_rt_tbl_set rt_tbl_set[IPA_IP_MAX];
	struct ipa_rt_tbl_set reap_rt_tbl_set[IPA_IP_MAX];
	struct kmem_cache *flt_rule_cache;
	struct kmem_cache *rt_rule_cache;
	struct kmem_cache *hdr_cache;
	struct kmem_cache *hdr_offset_cache;
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
	struct ipa_mem_buffer hdr_mem;
	bool ip4_rt_tbl_lcl;
	bool ip6_rt_tbl_lcl;
	bool ip4_flt_tbl_lcl;
	bool ip6_flt_tbl_lcl;
	struct ipa_mem_buffer empty_rt_tbl_mem;
	struct gen_pool *pipe_mem_pool;
	struct dma_pool *dma_pool;
	struct ipa_active_clients ipa_active_clients;
	struct workqueue_struct *power_mgmt_wq;
	bool tag_process_before_gating;
	u32 clnt_hdl_cmd;
	u32 clnt_hdl_data_in;
	u32 clnt_hdl_data_out;
	u8 a5_pipe_index;
	struct list_head intf_list;
	struct list_head msg_list;
	struct list_head pull_msg_list;
	struct mutex msg_lock;
	wait_queue_head_t msg_waitq;
	enum ipa_hw_type ipa_hw_type;
	enum ipa_hw_mode ipa_hw_mode;
	bool use_ipa_teth_bridge;
	/* featurize if memory footprint becomes a concern */
	struct ipa_stats stats;
	void *smem_pipe_mem;
	u32 ipa_bus_hdl;
	struct ipa_controller *ctrl;
	struct idr ipa_idr;
	struct device *pdev;
	spinlock_t idr_lock;
	u32 enable_clock_scaling;
	u32 curr_ipa_clk_rate;
	bool q6_proxy_clk_vote_valid;

	struct ipa_wlan_comm_memb wc_memb;
	struct ipa_wdi_ctx wdi;
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
};

struct ipa_controller {
	u32 ipa_clk_rate_hi;
	u32 ipa_clk_rate_lo;
	u32 clock_scaling_bw_threshold;
	void (*ipa_sram_read_settings)(void);
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

int ipa_send_one(struct ipa_sys_context *sys, struct ipa_desc *desc,
		bool in_atomic);
int ipa_send(struct ipa_sys_context *sys, u32 num_desc, struct ipa_desc *desc,
		bool in_atomic);
int ipa_get_ep_mapping(enum ipa_client_type client);
enum ipa_client_type ipa_get_client_mapping(int pipe_idx);
enum ipa_rm_resource_name ipa_get_rm_resource_from_ep(int pipe_idx);

int ipa_generate_hw_rule(enum ipa_ip_type ip,
			 const struct ipa_rule_attrib *attrib,
			 u8 **buf,
			 u16 *en_rule);
u8 *ipa_write_32(u32 w, u8 *dest);
u8 *ipa_write_16(u16 hw, u8 *dest);
u8 *ipa_write_8(u8 b, u8 *dest);
u8 *ipa_pad_to_32(u8 *dest);
int ipa_init_hw(void);
struct ipa_rt_tbl *__ipa_find_rt_tbl(enum ipa_ip_type ip, const char *name);
int ipa_set_single_ndp_per_mbim(bool);
int ipa_set_hw_timer_fix_for_mbim_aggr(bool);
void ipa_debugfs_init(void);
void ipa_debugfs_remove(void);

void ipa_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size);
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
void ipa_inc_client_enable_clks(void);
int ipa_inc_client_enable_clks_no_block(void);
void ipa_dec_client_disable_clks(void);
int ipa_interrupts_init(u32 ipa_irq, u32 ee, struct device *ipa_dev);
int __ipa_del_rt_rule(u32 rule_hdl);
int __ipa_del_hdr(u32 hdr_hdl);
int __ipa_release_hdr(u32 hdr_hdl);
int _ipa_read_gen_reg_v1_0(char *buff, int max_len);
int _ipa_read_gen_reg_v1_1(char *buff, int max_len);
int _ipa_read_gen_reg_v2_0(char *buff, int max_len);
int _ipa_read_ep_reg_v1_0(char *buf, int max_len, int pipe);
int _ipa_read_ep_reg_v1_1(char *buf, int max_len, int pipe);
int _ipa_read_ep_reg_v2_0(char *buf, int max_len, int pipe);
void _ipa_write_dbg_cnt_v1(int option);
void _ipa_write_dbg_cnt_v2_0(int option);
int _ipa_read_dbg_cnt_v1(char *buf, int max_len);
int _ipa_read_dbg_cnt_v2_0(char *buf, int max_len);
void _ipa_enable_clks_v1(void);
void _ipa_enable_clks_v2_0(void);
void _ipa_disable_clks_v1(void);
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

int __ipa_commit_flt_v1(enum ipa_ip_type ip);
int __ipa_commit_flt_v2(enum ipa_ip_type ip);
int __ipa_commit_rt_v1(enum ipa_ip_type ip);
int __ipa_commit_rt_v2(enum ipa_ip_type ip);
int __ipa_commit_hdr_v1(void);
int __ipa_commit_hdr_v2(void);
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

int ipa_set_required_perf_profile(enum ipa_voltage_level floor_voltage,
				  u32 bandwidth_mbps);

int ipa_cfg_ep_status(u32 clnt_hdl, const struct ipa_ep_cfg_status *ipa_ep_cfg);

int ipa_suspend_resource_no_block(enum ipa_rm_resource_name name);
int ipa_suspend_resource_sync(enum ipa_rm_resource_name name);
int ipa_resume_resource(enum ipa_rm_resource_name name);
bool ipa_should_pipe_be_suspended(enum ipa_client_type client);
int ipa_tag_aggr_force_close(int pipe_num);

void ipa_active_clients_lock(void);
int ipa_active_clients_trylock(void);
void ipa_active_clients_unlock(void);
int ipa_wdi_init(void);
int ipa_write_qmapid_wdi_pipe(u32 clnt_hdl, u8 qmap_id);
int ipa_tag_process(struct ipa_desc *desc, int num_descs,
		    unsigned long timeout);

int ipa_q6_cleanup(void);
int ipa_init_q6_smem(void);
#endif /* _IPA_I_H_ */
