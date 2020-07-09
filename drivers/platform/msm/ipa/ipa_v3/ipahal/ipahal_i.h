/* Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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

#ifndef _IPAHAL_I_H_
#define _IPAHAL_I_H_

#include <linux/ipa.h>
#include "../../ipa_common_i.h"

#define IPAHAL_DRV_NAME "ipahal"

#define IPAHAL_DBG(fmt, args...) \
	do { \
		pr_debug(IPAHAL_DRV_NAME " %s:%d " fmt, __func__, __LINE__, \
			## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPAHAL_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPAHAL_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAHAL_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPAHAL_DRV_NAME " %s:%d " fmt, __func__, __LINE__, \
			## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPAHAL_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAHAL_ERR(fmt, args...) \
	do { \
		pr_err(IPAHAL_DRV_NAME " %s:%d " fmt, __func__, __LINE__, \
			## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPAHAL_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPAHAL_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPAHAL_ERR_RL(fmt, args...) \
		do { \
			pr_err_ratelimited_ipa(IPAHAL_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
			IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
				IPAHAL_DRV_NAME " %s:%d " fmt, ## args); \
			IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				IPAHAL_DRV_NAME " %s:%d " fmt, ## args); \
		} while (0)

#define IPAHAL_DBG_REG(fmt, args...) \
	do { \
		pr_err(fmt, ## args); \
		IPA_IPC_LOGGING(ipahal_ctx->regdumpbuf, \
			" %s:%d " fmt, ## args); \
	} while (0)

#define IPAHAL_DBG_REG_IPC_ONLY(fmt, args...) \
	do { \
		IPA_IPC_LOGGING(ipahal_ctx->regdumpbuf, \
			" %s:%d " fmt, ## args); \
	} while (0)

#define IPAHAL_MEM_ALLOC(__size, __is_atomic_ctx) \
	(kzalloc((__size), ((__is_atomic_ctx) ? GFP_ATOMIC : GFP_KERNEL)))

#define IPAHAL_IPC_LOG_PAGES 50

#define IPAHAL_PKT_STATUS_FLTRT_RULE_MISS_ID 0x3ff

/*
 * struct ipahal_context - HAL global context data
 * @hw_type: IPA H/W type/version.
 * @base: Base address to be used for accessing IPA memory. This is
 *  I/O memory mapped address.
 *  Controlled by debugfs. default is off
 * @dent: Debugfs folder dir entry
 * @ipa_pdev: IPA Platform Device. Will be used for DMA memory
 * @empty_fltrt_tbl: Empty table to be used at tables init.
 */
struct ipahal_context {
	enum ipa_hw_type hw_type;
	void __iomem *base;
	struct dentry *dent;
	struct device *ipa_pdev;
	struct ipa_mem_buffer empty_fltrt_tbl;
	void *regdumpbuf;
};

extern struct ipahal_context *ipahal_ctx;



/* Immediate commands H/W structures */

/*
 * struct ipa_imm_cmd_hw_ip_v4_filter_init - IP_V4_FILTER_INIT command payload
 *  in H/W format.
 * Inits IPv4 filter block.
 * @hash_rules_addr: Addr in system mem where ipv4 hashable flt rules starts
 * @hash_rules_size: Size in bytes of the hashable tbl to cpy to local mem
 * @hash_local_addr: Addr in shared mem where ipv4 hashable flt tbl should
 *  be copied to
 * @nhash_rules_size: Size in bytes of the non-hashable tbl to cpy to local mem
 * @nhash_local_addr: Addr in shared mem where ipv4 non-hashable flt tbl should
 *  be copied to
 * @rsvd: reserved
 * @nhash_rules_addr: Addr in sys mem where ipv4 non-hashable flt tbl starts
 */
struct ipa_imm_cmd_hw_ip_v4_filter_init {
	u64 hash_rules_addr:64;
	u64 hash_rules_size:12;
	u64 hash_local_addr:16;
	u64 nhash_rules_size:12;
	u64 nhash_local_addr:16;
	u64 rsvd:8;
	u64 nhash_rules_addr:64;
};

/*
 * struct ipa_imm_cmd_hw_ip_v6_filter_init - IP_V6_FILTER_INIT command payload
 *  in H/W format.
 * Inits IPv6 filter block.
 * @hash_rules_addr: Addr in system mem where ipv6 hashable flt rules starts
 * @hash_rules_size: Size in bytes of the hashable tbl to cpy to local mem
 * @hash_local_addr: Addr in shared mem where ipv6 hashable flt tbl should
 *  be copied to
 * @nhash_rules_size: Size in bytes of the non-hashable tbl to cpy to local mem
 * @nhash_local_addr: Addr in shared mem where ipv6 non-hashable flt tbl should
 *  be copied to
 * @rsvd: reserved
 * @nhash_rules_addr: Addr in sys mem where ipv6 non-hashable flt tbl starts
 */
struct ipa_imm_cmd_hw_ip_v6_filter_init {
	u64 hash_rules_addr:64;
	u64 hash_rules_size:12;
	u64 hash_local_addr:16;
	u64 nhash_rules_size:12;
	u64 nhash_local_addr:16;
	u64 rsvd:8;
	u64 nhash_rules_addr:64;
};

/*
 * struct ipa_imm_cmd_hw_ip_v4_nat_init - IP_V4_NAT_INIT command payload
 *  in H/W format.
 * Inits IPv4 NAT block. Initiate NAT table with it dimensions, location
 *  cache address and other related parameters.
 * @ipv4_rules_addr: Addr in sys/shared mem where ipv4 NAT rules start
 * @ipv4_expansion_rules_addr: Addr in sys/shared mem where expansion NAT
 *  table starts. IPv4 NAT rules that result in hash collision are located
 *  in this table.
 * @index_table_addr: Addr in sys/shared mem where index table, which points
 *  to NAT table starts
 * @index_table_expansion_addr: Addr in sys/shared mem where expansion index
 *  table starts
 * @table_index: For future support of multiple NAT tables
 * @rsvd1: reserved
 * @ipv4_rules_addr_type: ipv4_rules_addr in sys or shared mem
 * @ipv4_expansion_rules_addr_type: ipv4_expansion_rules_addr in
 *  sys or shared mem
 * @index_table_addr_type: index_table_addr in sys or shared mem
 * @index_table_expansion_addr_type: index_table_expansion_addr in
 *  sys or shared mem
 * @size_base_tables: Num of entries in NAT tbl and idx tbl (each)
 * @size_expansion_tables: Num of entries in NAT expansion tbl and expansion
 *  idx tbl (each)
 * @rsvd2: reserved
 * @public_addr_info: Public IP addresses info suitable to the IPA H/W version
 *                    IPA H/W >= 4.0 - PDN config table offset in SMEM
 *                    IPA H/W < 4.0  - The public IP address
 */
struct ipa_imm_cmd_hw_ip_v4_nat_init {
	u64 ipv4_rules_addr:64;
	u64 ipv4_expansion_rules_addr:64;
	u64 index_table_addr:64;
	u64 index_table_expansion_addr:64;
	u64 table_index:3;
	u64 rsvd1:1;
	u64 ipv4_rules_addr_type:1;
	u64 ipv4_expansion_rules_addr_type:1;
	u64 index_table_addr_type:1;
	u64 index_table_expansion_addr_type:1;
	u64 size_base_tables:12;
	u64 size_expansion_tables:10;
	u64 rsvd2:2;
	u64 public_addr_info:32;
};

/*
 * struct ipa_imm_cmd_hw_ip_v6_ct_init - IP_V6_CONN_TRACK_INIT command payload
 *  in H/W format.
 * Inits IPv6CT block. Initiate IPv6CT table with it dimensions, location
 *  cache address and other related parameters.
 * @table_addr: Address in sys/shared mem where IPv6CT rules start
 * @expansion_table_addr: Address in sys/shared mem where IPv6CT expansion
 *  table starts. IPv6CT rules that result in hash collision are located
 *  in this table.
 * @table_index: For future support of multiple IPv6CT tables
 * @rsvd1: reserved
 * @table_addr_type: table_addr in sys or shared mem
 * @expansion_table_addr_type: expansion_table_addr in sys or shared mem
 * @rsvd2: reserved
 * @size_base_tables: Number of entries in IPv6CT table
 * @size_expansion_tables: Number of entries in IPv6CT expansion table
 * @rsvd3: reserved
 */
struct ipa_imm_cmd_hw_ip_v6_ct_init {
	u64 table_addr:64;
	u64 expansion_table_addr:64;
	u64 table_index:3;
	u64 rsvd1:1;
	u64 table_addr_type:1;
	u64 expansion_table_addr_type:1;
	u64 rsvd2:2;
	u64 size_base_table:12;
	u64 size_expansion_table:10;
	u64 rsvd3:34;
};

/*
 * struct ipa_imm_cmd_hw_ip_v4_routing_init - IP_V4_ROUTING_INIT command payload
 *  in H/W format.
 * Inits IPv4 routing table/structure - with the rules and other related params
 * @hash_rules_addr: Addr in system mem where ipv4 hashable rt rules starts
 * @hash_rules_size: Size in bytes of the hashable tbl to cpy to local mem
 * @hash_local_addr: Addr in shared mem where ipv4 hashable rt tbl should
 *  be copied to
 * @nhash_rules_size: Size in bytes of the non-hashable tbl to cpy to local mem
 * @nhash_local_addr: Addr in shared mem where ipv4 non-hashable rt tbl should
 *  be copied to
 * @rsvd: reserved
 * @nhash_rules_addr: Addr in sys mem where ipv4 non-hashable rt tbl starts
 */
struct ipa_imm_cmd_hw_ip_v4_routing_init {
	u64 hash_rules_addr:64;
	u64 hash_rules_size:12;
	u64 hash_local_addr:16;
	u64 nhash_rules_size:12;
	u64 nhash_local_addr:16;
	u64 rsvd:8;
	u64 nhash_rules_addr:64;
};

/*
 * struct ipa_imm_cmd_hw_ip_v6_routing_init - IP_V6_ROUTING_INIT command payload
 *  in H/W format.
 * Inits IPv6 routing table/structure - with the rules and other related params
 * @hash_rules_addr: Addr in system mem where ipv6 hashable rt rules starts
 * @hash_rules_size: Size in bytes of the hashable tbl to cpy to local mem
 * @hash_local_addr: Addr in shared mem where ipv6 hashable rt tbl should
 *  be copied to
 * @nhash_rules_size: Size in bytes of the non-hashable tbl to cpy to local mem
 * @nhash_local_addr: Addr in shared mem where ipv6 non-hashable rt tbl should
 *  be copied to
 * @rsvd: reserved
 * @nhash_rules_addr: Addr in sys mem where ipv6 non-hashable rt tbl starts
 */
struct ipa_imm_cmd_hw_ip_v6_routing_init {
	u64 hash_rules_addr:64;
	u64 hash_rules_size:12;
	u64 hash_local_addr:16;
	u64 nhash_rules_size:12;
	u64 nhash_local_addr:16;
	u64 rsvd:8;
	u64 nhash_rules_addr:64;
};

/*
 * struct ipa_imm_cmd_hw_hdr_init_local - HDR_INIT_LOCAL command payload
 *  in H/W format.
 * Inits hdr table within local mem with the hdrs and their length.
 * @hdr_table_addr: Word address in sys mem where the table starts (SRC)
 * @size_hdr_table: Size of the above (in bytes)
 * @hdr_addr: header address in IPA sram (used as DST for memory copy)
 * @rsvd: reserved
 */
struct ipa_imm_cmd_hw_hdr_init_local {
	u64 hdr_table_addr:64;
	u64 size_hdr_table:12;
	u64 hdr_addr:16;
	u64 rsvd:4;
};

/*
 * struct ipa_imm_cmd_hw_nat_dma - NAT_DMA command payload
 *  in H/W format
 * Perform DMA operation on NAT related mem addressess. Copy data into
 *  different locations within NAT associated tbls. (For add/remove NAT rules)
 * @table_index: NAT tbl index. Defines the NAT tbl on which to perform DMA op.
 * @rsvd1: reserved
 * @base_addr: Base addr to which the DMA operation should be performed.
 * @rsvd2: reserved
 * @offset: offset in bytes from base addr to write 'data' to
 * @data: data to be written
 * @rsvd3: reserved
 */
struct ipa_imm_cmd_hw_nat_dma {
	u64 table_index:3;
	u64 rsvd1:1;
	u64 base_addr:2;
	u64 rsvd2:2;
	u64 offset:32;
	u64 data:16;
	u64 rsvd3:8;
};

/*
 * struct ipa_imm_cmd_hw_table_dma_ipav4 - TABLE_DMA command payload
 *  in H/W format
 * Perform DMA operation on NAT and ipv6 connection tracking related mem
 * addresses. Copy data into different locations within NAT associated tbls
 * (For add/remove NAT rules)
 * @table_index: NAT tbl index. Defines the NAT tbl on which to perform DMA op.
 * @rsvd1: reserved
 * @base_addr: Base addr to which the DMA operation should be performed.
 * @rsvd2: reserved
 * @offset: offset in bytes from base addr to write 'data' to
 * @data: data to be written
 * @rsvd3: reserved
 */
struct ipa_imm_cmd_hw_table_dma_ipav4 {
	u64 table_index : 3;
	u64 rsvd1 : 1;
	u64 base_addr : 3;
	u64 rsvd2 : 1;
	u64 offset : 32;
	u64 data : 16;
	u64 rsvd3 : 8;
};

/*
 * struct ipa_imm_cmd_hw_hdr_init_system - HDR_INIT_SYSTEM command payload
 *  in H/W format.
 * Inits hdr table within sys mem with the hdrs and their length.
 * @hdr_table_addr: Word address in system memory where the hdrs tbl starts.
 */
struct ipa_imm_cmd_hw_hdr_init_system {
	u64 hdr_table_addr:64;
};

/*
 * struct ipa_imm_cmd_hw_ip_packet_init - IP_PACKET_INIT command payload
 *  in H/W format.
 * Configuration for specific IP pkt. Shall be called prior to an IP pkt
 *  data. Pkt will not go through IP pkt processing.
 * @destination_pipe_index: Destination pipe index  (in case routing
 *  is enabled, this field will overwrite the rt  rule)
 * @rsvd: reserved
 */
struct ipa_imm_cmd_hw_ip_packet_init {
	u64 destination_pipe_index:5;
	u64 rsv1:59;
};

/*
 * struct ipa_imm_cmd_hw_register_write - REGISTER_WRITE command payload
 *  in H/W format.
 * Write value to register. Allows reg changes to be synced with data packet
 *  and other immediate command. Can be used to access the sram
 * @sw_rsvd: Ignored by H/W. May be used by S/W
 * @skip_pipeline_clear: 0 to wait until IPA pipeline is clear. 1 don't wait
 * @offset: offset from IPA base address - Lower 16bit of the IPA reg addr
 * @value: value to write to register
 * @value_mask: mask specifying which value bits to write to the register
 * @pipeline_clear_options: options for pipeline to clear
 *	0: HPS - no pkt inside HPS (not grp specific)
 *	1: source group - The immediate cmd src grp does not use any pkt ctxs
 *	2: Wait until no pkt reside inside IPA pipeline
 *	3: reserved
 * @rsvd: reserved - should be set to zero
 */
struct ipa_imm_cmd_hw_register_write {
	u64 sw_rsvd:15;
	u64 skip_pipeline_clear:1;
	u64 offset:16;
	u64 value:32;
	u64 value_mask:32;
	u64 pipeline_clear_options:2;
	u64 rsvd:30;
};

/*
 * struct ipa_imm_cmd_hw_register_write - REGISTER_WRITE command payload
 *  in H/W format.
 * Write value to register. Allows reg changes to be synced with data packet
 *  and other immediate command. Can be used to access the sram
 * @sw_rsvd: Ignored by H/W. May be used by S/W
 * @offset_high: high bits of the Offset field - bits 17-20
 * @rsvd: reserved - should be set to zero
 * @offset: offset from IPA base address - Lower 16bit of the IPA reg addr
 * @value: value to write to register
 * @value_mask: mask specifying which value bits to write to the register
 * @rsvd2: reserved - should be set to zero
 */
struct ipa_imm_cmd_hw_register_write_v_4_0 {
	u64 sw_rsvd:11;
	u64 offset_high:4;
	u64 rsvd:1;
	u64 offset:16;
	u64 value:32;
	u64 value_mask:32;
	u64 rsvd2:32;
};

/*
 * struct ipa_imm_cmd_hw_dma_shared_mem - DMA_SHARED_MEM command payload
 *  in H/W format.
 * Perform mem copy into or out of the SW area of IPA local mem
 * @sw_rsvd: Ignored by H/W. My be used by S/W
 * @size: Size in bytes of data to copy. Expected size is up to 2K bytes
 * @local_addr: Address in IPA local memory
 * @direction: Read or write?
 *	0: IPA write, Write to local address from system address
 *	1: IPA read, Read from local address to system address
 * @skip_pipeline_clear: 0 to wait until IPA pipeline is clear. 1 don't wait
 * @pipeline_clear_options: options for pipeline to clear
 *	0: HPS - no pkt inside HPS (not grp specific)
 *	1: source group - The immediate cmd src grp does npt use any pkt ctxs
 *	2: Wait until no pkt reside inside IPA pipeline
 *	3: reserved
 * @rsvd: reserved - should be set to zero
 * @system_addr: Address in system memory
 */
struct ipa_imm_cmd_hw_dma_shared_mem {
	u64 sw_rsvd:16;
	u64 size:16;
	u64 local_addr:16;
	u64 direction:1;
	u64 skip_pipeline_clear:1;
	u64 pipeline_clear_options:2;
	u64 rsvd:12;
	u64 system_addr:64;
};

/*
 * struct ipa_imm_cmd_hw_dma_shared_mem - DMA_SHARED_MEM command payload
 *  in H/W format.
 * Perform mem copy into or out of the SW area of IPA local mem
 * @sw_rsvd: Ignored by H/W. My be used by S/W
 * @size: Size in bytes of data to copy. Expected size is up to 2K bytes
 * @clear_after_read: Clear local memory at the end of a read operation allows
 *  atomic read and clear if HPS is clear. Ignore for writes.
 * @local_addr: Address in IPA local memory
 * @direction: Read or write?
 *	0: IPA write, Write to local address from system address
 *	1: IPA read, Read from local address to system address
 * @rsvd: reserved - should be set to zero
 * @system_addr: Address in system memory
 */
struct ipa_imm_cmd_hw_dma_shared_mem_v_4_0 {
	u64 sw_rsvd:15;
	u64 clear_after_read:1;
	u64 size:16;
	u64 local_addr:16;
	u64 direction:1;
	u64 rsvd:15;
	u64 system_addr:64;
};

/*
 * struct ipa_imm_cmd_hw_ip_packet_tag_status -
 *  IP_PACKET_TAG_STATUS command payload in H/W format.
 * This cmd is used for to allow SW to track HW processing by setting a TAG
 *  value that is passed back to SW inside Packet Status information.
 *  TAG info will be provided as part of Packet Status info generated for
 *  the next pkt transferred over the pipe.
 *  This immediate command must be followed by a packet in the same transfer.
 * @sw_rsvd: Ignored by H/W. My be used by S/W
 * @tag: Tag that is provided back to SW
 */
struct ipa_imm_cmd_hw_ip_packet_tag_status {
	u64 sw_rsvd:16;
	u64 tag:48;
};

/*
 * struct ipa_imm_cmd_hw_dma_task_32b_addr -
 *	IPA_DMA_TASK_32B_ADDR command payload in H/W format.
 * Used by clients using 32bit addresses. Used to perform DMA operation on
 *  multiple descriptors.
 *  The Opcode is dynamic, where it holds the number of buffer to process
 * @sw_rsvd: Ignored by H/W. My be used by S/W
 * @cmplt: Complete flag: When asserted IPA will interrupt SW when the entire
 *  DMA related data was completely xfered to its destination.
 * @eof: Enf Of Frame flag: When asserted IPA will assert the EOT to the
 *  dest client. This is used used for aggr sequence
 * @flsh: Flush flag: When asserted, pkt will go through the IPA blocks but
 *  will not be xfered to dest client but rather will be discarded
 * @lock: Lock pipe flag: When asserted, IPA will stop processing descriptors
 *  from other EPs in the same src grp (RX queue)
 * @unlock: Unlock pipe flag: When asserted, IPA will stop exclusively
 *  servicing current EP out of the src EPs of the grp (RX queue)
 * @size1: Size of buffer1 data
 * @addr1: Pointer to buffer1 data
 * @packet_size: Total packet size. If a pkt send using multiple DMA_TASKs,
 *  only the first one needs to have this field set. It will be ignored
 *  in subsequent DMA_TASKs until the packet ends (EOT). First DMA_TASK
 *  must contain this field (2 or more buffers) or EOT.
 */
struct ipa_imm_cmd_hw_dma_task_32b_addr {
	u64 sw_rsvd:11;
	u64 cmplt:1;
	u64 eof:1;
	u64 flsh:1;
	u64 lock:1;
	u64 unlock:1;
	u64 size1:16;
	u64 addr1:32;
	u64 packet_size:16;
};



/* IPA Status packet H/W structures and info */

/*
 * struct ipa_status_pkt_hw - IPA status packet payload in H/W format.
 *  This structure describes the status packet H/W structure for the
 *   following statuses: IPA_STATUS_PACKET, IPA_STATUS_DROPPED_PACKET,
 *   IPA_STATUS_SUSPENDED_PACKET.
 *  Other statuses types has different status packet structure.
 * @status_opcode: The Type of the status (Opcode).
 * @exception: (not bitmask) - the first exception that took place.
 *  In case of exception, src endp and pkt len are always valid.
 * @status_mask: Bit mask specifying on which H/W blocks the pkt was processed.
 * @pkt_len: Pkt pyld len including hdr, include retained hdr if used. Does
 *  not include padding or checksum trailer len.
 * @endp_src_idx: Source end point index.
 * @rsvd1: reserved
 * @endp_dest_idx: Destination end point index.
 *  Not valid in case of exception
 * @rsvd2: reserved
 * @metadata: meta data value used by packet
 * @flt_local: Filter table location flag: Does matching flt rule belongs to
 *  flt tbl that resides in lcl memory? (if not, then system mem)
 * @flt_hash: Filter hash hit flag: Does matching flt rule was in hash tbl?
 * @flt_global: Global filter rule flag: Does matching flt rule belongs to
 *  the global flt tbl? (if not, then the per endp tables)
 * @flt_ret_hdr: Retain header in filter rule flag: Does matching flt rule
 *  specifies to retain header?
 *  Starting IPA4.5, this will be true only if packet has L2 header.
 * @flt_rule_id: The ID of the matching filter rule. This info can be combined
 *  with endp_src_idx to locate the exact rule. ID=0x3FF reserved to specify
 *  flt miss. In case of miss, all flt info to be ignored
 * @rt_local: Route table location flag: Does matching rt rule belongs to
 *  rt tbl that resides in lcl memory? (if not, then system mem)
 * @rt_hash: Route hash hit flag: Does matching rt rule was in hash tbl?
 * @ucp: UC Processing flag.
 * @rt_tbl_idx: Index of rt tbl that contains the rule on which was a match
 * @rt_rule_id: The ID of the matching rt rule. This info can be combined
 *  with rt_tbl_idx to locate the exact rule. ID=0x3FF reserved to specify
 *  rt miss. In case of miss, all rt info to be ignored
 * @nat_hit: NAT hit flag: Was their NAT hit?
 * @nat_entry_idx: Index of the NAT entry used of NAT processing
 * @nat_type: Defines the type of the NAT operation:
 *	00: No NAT
 *	01: Source NAT
 *	10: Destination NAT
 *	11: Reserved
 * @tag_info: S/W defined value provided via immediate command
 * @seq_num: Per source endp unique packet sequence number
 * @time_of_day_ctr: running counter from IPA clock
 * @hdr_local: Header table location flag: In header insertion, was the header
 *  taken from the table resides in local memory? (If no, then system mem)
 * @hdr_offset: Offset of used header in the header table
 * @frag_hit: Frag hit flag: Was their frag rule hit in H/W frag table?
 * @frag_rule: Frag rule index in H/W frag table in case of frag hit
 * @hw_specific: H/W specific reserved value
 */
struct ipa_pkt_status_hw {
	u64 status_opcode:8;
	u64 exception:8;
	u64 status_mask:16;
	u64 pkt_len:16;
	u64 endp_src_idx:5;
	u64 rsvd1:3;
	u64 endp_dest_idx:5;
	u64 rsvd2:3;
	u64 metadata:32;
	u64 flt_local:1;
	u64 flt_hash:1;
	u64 flt_global:1;
	u64 flt_ret_hdr:1;
	u64 flt_rule_id:10;
	u64 rt_local:1;
	u64 rt_hash:1;
	u64 ucp:1;
	u64 rt_tbl_idx:5;
	u64 rt_rule_id:10;
	u64 nat_hit:1;
	u64 nat_entry_idx:13;
	u64 nat_type:2;
	u64 tag_info:48;
	u64 seq_num:8;
	u64 time_of_day_ctr:24;
	u64 hdr_local:1;
	u64 hdr_offset:10;
	u64 frag_hit:1;
	u64 frag_rule:4;
	u64 hw_specific:16;
};

/* Size of H/W Packet Status */
#define IPA3_0_PKT_STATUS_SIZE 32

/* Headers and processing context H/W structures and definitions */

/* uCP command numbers */
#define IPA_HDR_UCP_802_3_TO_802_3		6
#define IPA_HDR_UCP_802_3_TO_ETHII		7
#define IPA_HDR_UCP_ETHII_TO_802_3		8
#define IPA_HDR_UCP_ETHII_TO_ETHII		9
#define IPA_HDR_UCP_L2TP_HEADER_ADD		10
#define IPA_HDR_UCP_L2TP_HEADER_REMOVE		11
#define IPA_HDR_UCP_L2TP_UDP_HEADER_ADD		12
#define IPA_HDR_UCP_L2TP_UDP_HEADER_REMOVE	13
#define IPA_HDR_UCP_ETHII_TO_ETHII_EX		14
#define IPA_HDR_UCP_SET_DSCP		16

/* Processing context TLV type */
#define IPA_PROC_CTX_TLV_TYPE_END 0
#define IPA_PROC_CTX_TLV_TYPE_HDR_ADD 1
#define IPA_PROC_CTX_TLV_TYPE_PROC_CMD 3

/**
 * struct ipa_hw_hdr_proc_ctx_tlv -
 * HW structure of IPA processing context header - TLV part
 * @type: 0 - end type
 *        1 - header addition type
 *        3 - processing command type
 * @length: number of bytes after tlv
 *        for type:
 *        0 - needs to be 0
 *        1 - header addition length
 *        3 - number of 32B including type and length.
 * @value: specific value for type
 *        for type:
 *        0 - needs to be 0
 *        1 - header length
 *        3 - command ID (see IPA_HDR_UCP_* definitions)
 */
struct ipa_hw_hdr_proc_ctx_tlv {
	u32 type:8;
	u32 length:8;
	u32 value:16;
};

/**
 * struct ipa_hw_hdr_proc_ctx_hdr_add -
 * HW structure of IPA processing context - add header tlv
 * @tlv: IPA processing context TLV
 * @hdr_addr: processing context header address
 */
struct ipa_hw_hdr_proc_ctx_hdr_add {
	struct ipa_hw_hdr_proc_ctx_tlv tlv;
	u32 hdr_addr;
	u32 hdr_addr_hi;
};

/**
 * struct ipa_hw_hdr_proc_ctx_l2tp_add_hdr -
 * HW structure of IPA processing context - add l2tp header tlv
 * @tlv: IPA processing context TLV
 * @l2tp_params: l2tp parameters
 */
struct ipa_hw_hdr_proc_ctx_l2tp_add_hdr {
	struct ipa_hw_hdr_proc_ctx_tlv tlv;
	struct ipa_l2tp_header_add_procparams l2tp_params;
};

/**
 * struct ipa_hw_hdr_proc_ctx_l2tp_remove_hdr -
 * HW structure of IPA processing context - remove l2tp header tlv
 * @tlv: IPA processing context TLV
 * @l2tp_params: l2tp parameters
 */
struct ipa_hw_hdr_proc_ctx_l2tp_remove_hdr {
	struct ipa_hw_hdr_proc_ctx_tlv tlv;
	struct ipa_l2tp_header_remove_procparams l2tp_params;
};

/**
 * struct ipa_hw_hdr_proc_ctx_add_hdr_seq -
 * IPA processing context header - add header sequence
 * @hdr_add: add header command
 * @end: tlv end command (cmd.type must be 0)
 */
struct ipa_hw_hdr_proc_ctx_add_hdr_seq {
	struct ipa_hw_hdr_proc_ctx_hdr_add hdr_add;
	struct ipa_hw_hdr_proc_ctx_tlv end;
};

/**
 * struct ipa_hw_hdr_proc_ctx_add_hdr_cmd_seq -
 * IPA processing context header - process command sequence
 * @hdr_add: add header command
 * @cmd: tlv processing command (cmd.type must be 3)
 * @end: tlv end command (cmd.type must be 0)
 */
struct ipa_hw_hdr_proc_ctx_add_hdr_cmd_seq {
	struct ipa_hw_hdr_proc_ctx_hdr_add hdr_add;
	struct ipa_hw_hdr_proc_ctx_tlv cmd;
	struct ipa_hw_hdr_proc_ctx_tlv end;
};

/**
 * struct ipa_hw_hdr_proc_ctx_add_l2tp_hdr_cmd_seq -
 * IPA processing context header - process command sequence
 * @hdr_add: add header command
 * @l2tp_params: l2tp params for header addition
 * @end: tlv end command (cmd.type must be 0)
 */
struct ipa_hw_hdr_proc_ctx_add_l2tp_hdr_cmd_seq {
	struct ipa_hw_hdr_proc_ctx_hdr_add hdr_add;
	struct ipa_hw_hdr_proc_ctx_l2tp_add_hdr l2tp_params;
	struct ipa_hw_hdr_proc_ctx_tlv end;
};

/**
 * struct ipa_hw_hdr_proc_ctx_remove_l2tp_hdr_cmd_seq -
 * IPA processing context header - process command sequence
 * @hdr_add: add header command
 * @l2tp_params: l2tp params for header removal
 * @end: tlv end command (cmd.type must be 0)
 */
struct ipa_hw_hdr_proc_ctx_remove_l2tp_hdr_cmd_seq {
	struct ipa_hw_hdr_proc_ctx_hdr_add hdr_add;
	struct ipa_hw_hdr_proc_ctx_l2tp_remove_hdr l2tp_params;
	struct ipa_hw_hdr_proc_ctx_tlv end;
};

/**
 * struct ipa_hw_hdr_proc_ctx_add_hdr_ex -
 * HW structure of IPA processing context - add generic header
 * @tlv: IPA processing context TLV
 * @params: generic eth2 to eth2 parameters
 */
struct ipa_hw_hdr_proc_ctx_add_hdr_ex {
	struct ipa_hw_hdr_proc_ctx_tlv tlv;
	struct ipa_eth_II_to_eth_II_ex_procparams params;
};

/**
 * struct ipa_hw_hdr_proc_ctx_add_hdr_cmd_seq_ex -
 * IPA processing context header - process command sequence
 * @hdr_add: add header command
 * @params: params for header generic header add
 * @end: tlv end command (cmd.type must be 0)
 */
struct ipa_hw_hdr_proc_ctx_add_hdr_cmd_seq_ex {
	struct ipa_hw_hdr_proc_ctx_hdr_add hdr_add;
	struct ipa_hw_hdr_proc_ctx_add_hdr_ex hdr_add_ex;
	struct ipa_hw_hdr_proc_ctx_tlv end;
};

/**
 * struct ipa_hw_hdr_proc_ctx_remove_l2tp_udp_hdr_cmd_seq -
 * IPA processing context header - process command sequence
 * @l2tp_params: l2tp params for header removal
 * @end: tlv end command (cmd.type must be 0)
 */
struct ipa_hw_hdr_proc_ctx_remove_l2tp_udp_hdr_cmd_seq {
	struct ipa_hw_hdr_proc_ctx_l2tp_remove_hdr l2tp_params;
	struct ipa_hw_hdr_proc_ctx_tlv end;
};

#endif /* _IPAHAL_I_H_ */
