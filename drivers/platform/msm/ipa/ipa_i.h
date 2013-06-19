/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/list.h>
#include <linux/mutex.h>
#include <linux/skbuff.h>
#include <linux/slab.h>
#include <mach/ipa.h>
#include <mach/sps.h>
#include "ipa_hw_defs.h"
#include "ipa_ram_mmap.h"
#include "ipa_reg.h"

#define DRV_NAME "ipa"
#define IPA_COOKIE 0xfacefeed

#define IPA_NUM_PIPES 0x14
#define IPA_SYS_DESC_FIFO_SZ 0x800
#define IPA_SYS_TX_DATA_DESC_FIFO_SZ 0x1000

#ifdef IPA_DEBUG
#define IPADBG(fmt, args...) \
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)
/*	pr_debug(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args) */
#else
#define IPADBG(fmt, args...)
#endif

#define WLAN_AMPDU_TX_EP 15
#define WLAN_PROD_TX_EP  19

#define MAX_NUM_EXCP     8
#define MAX_NUM_IMM_CMD 17

#define IPA_STATS

#ifdef IPA_STATS
#define IPA_STATS_INC_CNT(val) do {			\
				++val;			\
			} while (0)
#define IPA_STATS_INC_CNT_SAFE(val) do {		\
				atomic_inc(&val);	\
			} while (0)
#define IPA_STATS_EXCP_CNT(flags, base) do {			\
			int i;					\
			for (i = 0; i < MAX_NUM_EXCP; i++)	\
				if (flags & BIT(i))		\
					++base[i];		\
			} while (0)
#define IPA_STATS_INC_TX_CNT(ep, sw, hw) do {		\
			if (ep == WLAN_AMPDU_TX_EP)	\
				++hw;			\
			else				\
				++sw;			\
			} while (0)
#define IPA_STATS_INC_IC_CNT(num, base, stat_base) do {			\
			int i;						\
			for (i = 0; i < num; i++)			\
				++stat_base[base[i].opcode];		\
			} while (0)
#define IPA_STATS_INC_BRIDGE_CNT(type, dir, base) do {		\
			++base[type][dir];			\
			} while (0)
#else
#define IPA_STATS_INC_CNT(x) do { } while (0)
#define IPA_STATS_INC_CNT_SAFE(x) do { } while (0)
#define IPA_STATS_EXCP_CNT(flags, base) do { } while (0)
#define IPA_STATS_INC_TX_CNT(ep, sw, hw) do { } while (0)
#define IPA_STATS_INC_IC_CNT(num, base, stat_base) do { } while (0)
#define IPA_STATS_INC_BRIDGE_CNT(type, dir, base) do { } while (0)
#endif

#define IPAERR(fmt, args...) \
	pr_err(DRV_NAME " %s:%d " fmt, __func__, __LINE__, ## args)

#define IPA_TOS_EQ			BIT(0)
#define IPA_PROTOCOL_EQ		BIT(1)
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
#define IPA_HDR_BIN_MAX 4

#define IPA_EVENT_THRESHOLD 0x10

#define IPA_RX_POOL_CEIL 32
#define IPA_RX_SKB_SIZE 2048

#define IPA_DFLT_HDR_NAME "ipa_excp_hdr"
#define IPA_INVALID_L4_PROTOCOL 0xFF

#define IPA_CLIENT_IS_PROD(x) (x >= IPA_CLIENT_PROD && x < IPA_CLIENT_CONS)
#define IPA_CLIENT_IS_CONS(x) (x >= IPA_CLIENT_CONS && x < IPA_CLIENT_MAX)
#define IPA_SETFIELD(val, shift, mask) (((val) << (shift)) & (mask))

#define IPA_HW_TABLE_ALIGNMENT(start_ofst) \
	(((start_ofst) + 127) & ~127)
#define IPA_RT_FLT_HW_RULE_BUF_SIZE	(128)

/**
 * enum ipa_sys_pipe - 5 A5-IPA pipes
 *
 * 5 A5-IPA pipes (all system mode)
 */
enum ipa_sys_pipe {
	IPA_A5_UNUSED,
	IPA_A5_CMD,
	IPA_A5_LAN_WAN_OUT,
	IPA_A5_LAN_WAN_IN,
	IPA_A5_WLAN_AMPDU_OUT,
	IPA_A5_SYS_MAX
};

/**
 * enum ipa_operating_mode - IPA operating mode
 *
 * IPA operating mode
 */
enum ipa_operating_mode {
	IPA_MODE_USB_DONGLE,
	IPA_MODE_MSM,
	IPA_MODE_EXT_APPS,
	IPA_MODE_MOBILE_AP_WAN,
	IPA_MODE_MOBILE_AP_WLAN,
	IPA_MODE_MOBILE_AP_ETH,
	IPA_MODE_MAX
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
 * struct ipa_tree_node - handle database entry
 * @node: RB node
 * @hdl: handle
 */
struct ipa_tree_node {
	struct rb_node node;
	u32 hdl;
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
 * @suspended: valid for B2B pipes, whether IPA EP is suspended
 */
struct ipa_ep_context {
	int valid;
	enum ipa_client_type client;
	struct sps_pipe *ep_hdl;
	struct ipa_ep_cfg cfg;
	struct ipa_ep_cfg_holb holb;
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
	bool suspended;
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
	struct list_head head_desc_list;
	u32 len;
	spinlock_t spinlock;
	struct sps_register_event event;
	struct ipa_ep_context *ep;
	atomic_t curr_polling_state;
	struct delayed_work switch_to_intr_work;
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
 *
 * This struct can wrap both data packet and immediate command packet.
 */
struct ipa_tx_pkt_wrapper {
	enum ipa_desc_type type;
	struct ipa_mem_buffer mem;
	struct work_struct work;
	struct list_head link;
	void (*callback)(void *user1, void *user2);
	void *user1;
	void *user2;
	struct ipa_sys_context *sys;
	struct ipa_mem_buffer mult;
	u32 cnt;
	void *bounce;
};

/**
 * struct ipa_desc - IPA descriptor
 * @type: skb or immediate command or plain old data
 * @pyld: points to skb
 * or kmalloc'ed immediate command parameters/plain old data
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
	u16 len;
	u16 opcode;
	void (*callback)(void *user1, void *user2);
	void *user1;
	void *user2;
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
	struct sk_buff *skb;
	dma_addr_t dma_address;
	struct list_head link;
	u32 len;
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


struct ipa_stats {
	u32 imm_cmds[MAX_NUM_IMM_CMD];
	u32 tx_sw_pkts;
	u32 tx_hw_pkts;
	u32 rx_pkts;
	u32 rx_excp_pkts[MAX_NUM_EXCP];
	u32 bridged_pkts[IPA_BRIDGE_TYPE_MAX][IPA_BRIDGE_DIR_MAX];
	u32 rx_repl_repost;
	u32 x_intr_repost;
	u32 x_intr_repost_tx;
	u32 rx_q_len;
	u32 msg_w[IPA_EVENT_MAX];
	u32 msg_r[IPA_EVENT_MAX];
	u32 a2_power_on_reqs_in;
	u32 a2_power_on_reqs_out;
	u32 a2_power_off_reqs_in;
	u32 a2_power_off_reqs_out;
	u32 a2_power_modem_acks;
	u32 a2_power_apps_acks;
};

/**
 * struct ipa_context - IPA context
 * @class: pointer to the struct class
 * @dev_num: device number
 * @dev: the dev_t of the device
 * @cdev: cdev of the device
 * @bam_handle: IPA driver's BAM handle
 * @ep: list of all end points
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
 * @tree_node_cache: tree nodes cache
 * @rt_idx_bitmap: routing table index bitmap
 * @lock: this does NOT protect the linked lists within ipa_sys_context
 * @sys: IPA sys context for system-bam pipes
 * @rx_wq: Rx packets work queue
 * @tx_wq: Tx packets work queue
 * @smem_sz: shared memory size
 * @hdr_hdl_tree: header handles tree
 * @rt_rule_hdl_tree: routing rule handles tree
 * @rt_tbl_hdl_tree: routing table handles tree
 * @flt_rule_hdl_tree: filtering rule handles tree
 * @nat_mem: NAT memory
 * @excp_hdr_hdl: exception header handle
 * @dflt_v4_rt_rule_hdl: default v4 routing rule handle
 * @dflt_v6_rt_rule_hdl: default v6 routing rule handle
 * @polling_mode: 1 - pure polling mode; 0 - interrupt+polling mode
 * @aggregation_type: aggregation type used on USB client endpoint
 * @aggregation_byte_limit: aggregation byte limit used on USB client endpoint
 * @aggregation_time_limit: aggregation time limit used on USB client endpoint
 * @curr_polling_state: current polling state
 * @poll_work: polling work
 * @hdr_tbl_lcl: where hdr tbl resides 1-local, 0-system
 * @hdr_mem: header memory
 * @ip4_rt_tbl_lcl: where ip4 rt tables reside 1-local; 0-system
 * @ip6_rt_tbl_lcl: where ip6 rt tables reside 1-local; 0-system
 * @ip4_flt_tbl_lcl: where ip4 flt tables reside 1-local; 0-system
 * @ip6_flt_tbl_lcl: where ip6 flt tables reside 1-local; 0-system
 * @empty_rt_tbl_mem: empty routing tables memory
 * @pipe_mem_pool: pipe memory pool
 * @dma_pool: special purpose DMA pool
 * @ipa_hw_type: type of IPA HW type (e.g. IPA 1.0, IPA 1.1 etc')
 * @ipa_hw_mode: mode of IPA HW mode (e.g. Normal, Virtual or over PCIe)
 *
 * IPA context - holds all relevant info about IPA driver and its state
 */
struct ipa_context {
	struct class *class;
	dev_t dev_num;
	struct device *dev;
	struct cdev cdev;
	u32 bam_handle;
	struct ipa_ep_context ep[IPA_NUM_PIPES];
	struct ipa_flt_tbl flt_tbl[IPA_NUM_PIPES][IPA_IP_MAX];
	enum ipa_operating_mode mode;
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
	struct kmem_cache *tree_node_cache;
	unsigned long rt_idx_bitmap[IPA_IP_MAX];
	struct mutex lock;
	struct ipa_sys_context sys[IPA_A5_SYS_MAX];
	struct workqueue_struct *rx_wq;
	struct workqueue_struct *tx_wq;
	u16 smem_sz;
	struct rb_root hdr_hdl_tree;
	struct rb_root rt_rule_hdl_tree;
	struct rb_root rt_tbl_hdl_tree;
	struct rb_root flt_rule_hdl_tree;
	struct ipa_nat_mem nat_mem;
	u32 excp_hdr_hdl;
	u32 dflt_v4_rt_rule_hdl;
	u32 dflt_v6_rt_rule_hdl;
	bool polling_mode;
	uint aggregation_type;
	uint aggregation_byte_limit;
	uint aggregation_time_limit;
	struct delayed_work poll_work;
	bool hdr_tbl_lcl;
	struct ipa_mem_buffer hdr_mem;
	bool ip4_rt_tbl_lcl;
	bool ip6_rt_tbl_lcl;
	bool ip4_flt_tbl_lcl;
	bool ip6_flt_tbl_lcl;
	struct ipa_mem_buffer empty_rt_tbl_mem;
	struct gen_pool *pipe_mem_pool;
	struct dma_pool *dma_pool;
	struct mutex ipa_active_clients_lock;
	int ipa_active_clients;
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
	/* featurize if memory footprint becomes a concern */
	struct ipa_stats stats;
	void *smem_pipe_mem;
};

/**
 * struct ipa_route - IPA route
 * @route_dis: route disable
 * @route_def_pipe: route default pipe
 * @route_def_hdr_table: route default header table
 * @route_def_hdr_ofst: route default header offset table
 */
struct ipa_route {
	u32 route_dis;
	u32 route_def_pipe;
	u32 route_def_hdr_table;
	u32 route_def_hdr_ofst;
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

/**
 * enum a2_mux_pipe_direction - IPA-A2 pipe direction
 */
enum a2_mux_pipe_direction {
	A2_TO_IPA = 0,
	IPA_TO_A2 = 1
};

/**
 * struct a2_mux_pipe_connection - A2 MUX pipe connection
 * @src_phy_addr: source physical address
 * @src_pipe_index: source pipe index
 * @dst_phy_addr: destination physical address
 * @dst_pipe_index: destination pipe index
 * @mem_type: pipe memory type
 * @data_fifo_base_offset: data FIFO base offset
 * @data_fifo_size: data FIFO size
 * @desc_fifo_base_offset: descriptors FIFO base offset
 * @desc_fifo_size: descriptors FIFO size
 */
struct a2_mux_pipe_connection {
	int			src_phy_addr;
	int			src_pipe_index;
	int			dst_phy_addr;
	int			dst_pipe_index;
	enum ipa_pipe_mem_type	mem_type;
	int			data_fifo_base_offset;
	int			data_fifo_size;
	int			desc_fifo_base_offset;
	int			desc_fifo_size;
};

struct ipa_plat_drv_res {
	u32 ipa_mem_base;
	u32 ipa_mem_size;
	u32 bam_mem_base;
	u32 bam_mem_size;
	u32 a2_bam_mem_base;
	u32 a2_bam_mem_size;
	u32 ipa_irq;
	u32 bam_irq;
	u32 a2_bam_irq;
	u32 ipa_pipe_mem_start_ofst;
	u32 ipa_pipe_mem_size;
	enum ipa_hw_type ipa_hw_type;
	enum ipa_hw_mode ipa_hw_mode;
	struct a2_mux_pipe_connection a2_to_ipa_pipe;
	struct a2_mux_pipe_connection ipa_to_a2_pipe;
};

extern struct ipa_context *ipa_ctx;

int ipa_get_a2_mux_pipe_info(enum a2_mux_pipe_direction pipe_dir,
				struct a2_mux_pipe_connection *pipe_connect);
int ipa_get_a2_mux_bam_info(u32 *a2_bam_mem_base, u32 *a2_bam_mem_size,
			    u32 *a2_bam_irq);
void teth_bridge_get_client_handles(u32 *producer_handle,
		u32 *consumer_handle);
int ipa_send_one(struct ipa_sys_context *sys, struct ipa_desc *desc,
		bool in_atomic);
int ipa_send(struct ipa_sys_context *sys, u32 num_desc, struct ipa_desc *desc,
		bool in_atomic);
int ipa_get_ep_mapping(enum ipa_operating_mode mode,
		       enum ipa_client_type client);
int ipa_get_client_mapping(enum ipa_operating_mode mode, int pipe_idx);
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
void ipa_dump(void);
int ipa_generate_hdr_hw_tbl(struct ipa_mem_buffer *mem);
int ipa_generate_rt_hw_tbl(enum ipa_ip_type ip, struct ipa_mem_buffer *mem);
int ipa_generate_flt_hw_tbl(enum ipa_ip_type ip, struct ipa_mem_buffer *mem);
int ipa_set_single_ndp_per_mbim(bool);
int ipa_set_hw_timer_fix_for_mbim_aggr(bool);
void ipa_debugfs_init(void);
void ipa_debugfs_remove(void);

int ipa_insert(struct rb_root *root, struct ipa_tree_node *data);
struct ipa_tree_node *ipa_search(struct rb_root *root, u32 hdl);
void ipa_dump_buff_internal(void *base, dma_addr_t phy_base, u32 size);

#ifdef IPA_DEBUG
#define IPA_DUMP_BUFF(base, phy_base, size) \
	ipa_dump_buff_internal(base, phy_base, size)
#else
#define IPA_DUMP_BUFF(base, phy_base, size)
#endif

int ipa_cfg_route(struct ipa_route *route);
int ipa_send_cmd(u16 num_desc, struct ipa_desc *descr);
void ipa_replenish_rx_cache(void);
void ipa_cleanup_rx(void);
int ipa_cfg_filter(u32 disable);
void ipa_wq_write_done(struct work_struct *work);
int ipa_handle_rx_core(struct ipa_sys_context *sys, bool process_all,
		bool in_poll_state);
int ipa_handle_tx_core(struct ipa_sys_context *sys, bool process_all,
		bool in_poll_state);
int ipa_pipe_mem_init(u32 start_ofst, u32 size);
int ipa_pipe_mem_alloc(u32 *ofst, u32 size);
int ipa_pipe_mem_free(u32 ofst, u32 size);
int ipa_straddle_boundary(u32 start, u32 end, u32 boundary);
struct ipa_context *ipa_get_ctx(void);
void ipa_enable_clks(void);
void ipa_disable_clks(void);
void ipa_inc_client_enable_clks(void);
void ipa_dec_client_disable_clks(void);
int __ipa_del_rt_rule(u32 rule_hdl);
int __ipa_del_hdr(u32 hdr_hdl);
int __ipa_release_hdr(u32 hdr_hdl);

static inline u32 ipa_read_reg(void *base, u32 offset)
{
	u32 val = ioread32(base + offset);
	IPADBG("0x%x(va) read reg 0x%x r_val 0x%x.\n",
		(u32)base, offset, val);
	return val;
}

static inline void ipa_write_reg(void *base, u32 offset, u32 val)
{
	iowrite32(val, base + offset);
	IPADBG("0x%x(va) write reg 0x%x w_val 0x%x.\n",
		(u32)base, offset, val);
}

int ipa_bridge_init(void);
void ipa_bridge_cleanup(void);

ssize_t ipa_read(struct file *filp, char __user *buf, size_t count,
		 loff_t *f_pos);
int ipa_pull_msg(struct ipa_msg_meta *meta, char *buff, size_t count);
int ipa_query_intf(struct ipa_ioc_query_intf *lookup);
int ipa_query_intf_tx_props(struct ipa_ioc_query_intf_tx_props *tx);
int ipa_query_intf_rx_props(struct ipa_ioc_query_intf_rx_props *rx);

int a2_mux_init(void);
int a2_mux_exit(void);

void wwan_cleanup(void);

int teth_bridge_driver_init(void);

#endif /* _IPA_I_H_ */
