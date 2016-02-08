/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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

#ifndef _IPAHAL_H_
#define _IPAHAL_H_

#include <linux/msm_ipa.h>

/*
 * Immediate command names
 *
 * NOTE:: Any change to this enum, need to change to ipahal_imm_cmd_name_to_str
 *	array as well.
 */
enum ipahal_imm_cmd_name {
	IPA_IMM_CMD_IP_V4_FILTER_INIT,
	IPA_IMM_CMD_IP_V6_FILTER_INIT,
	IPA_IMM_CMD_IP_V4_NAT_INIT,
	IPA_IMM_CMD_IP_V4_ROUTING_INIT,
	IPA_IMM_CMD_IP_V6_ROUTING_INIT,
	IPA_IMM_CMD_HDR_INIT_LOCAL,
	IPA_IMM_CMD_HDR_INIT_SYSTEM,
	IPA_IMM_CMD_REGISTER_WRITE,
	IPA_IMM_CMD_NAT_DMA,
	IPA_IMM_CMD_IP_PACKET_INIT,
	IPA_IMM_CMD_DMA_SHARED_MEM,
	IPA_IMM_CMD_IP_PACKET_TAG_STATUS,
	IPA_IMM_CMD_DMA_TASK_32B_ADDR,
	IPA_IMM_CMD_MAX,
};

/* Immediate commands abstracted structures */

/*
 * struct ipahal_imm_cmd_ip_v4_filter_init - IP_V4_FILTER_INIT cmd payload
 * Inits IPv4 filter block.
 * @hash_rules_addr: Addr in sys mem where ipv4 hashable flt tbl starts
 * @hash_rules_size: Size in bytes of the hashable tbl to cpy to local mem
 * @hash_local_addr: Addr in shared mem where ipv4 hashable flt tbl should
 *  be copied to
 * @nhash_rules_addr: Addr in sys mem where ipv4 non-hashable flt tbl starts
 * @nhash_rules_size: Size in bytes of the non-hashable tbl to cpy to local mem
 * @nhash_local_addr: Addr in shared mem where ipv4 non-hashable flt tbl should
 *  be copied to
 */
struct ipahal_imm_cmd_ip_v4_filter_init {
	u64 hash_rules_addr;
	u32 hash_rules_size;
	u32 hash_local_addr;
	u64 nhash_rules_addr;
	u32 nhash_rules_size;
	u32 nhash_local_addr;
};

/*
 * struct ipahal_imm_cmd_ip_v6_filter_init - IP_V6_FILTER_INIT cmd payload
 * Inits IPv6 filter block.
 * @hash_rules_addr: Addr in sys mem where ipv6 hashable flt tbl starts
 * @hash_rules_size: Size in bytes of the hashable tbl to cpy to local mem
 * @hash_local_addr: Addr in shared mem where ipv6 hashable flt tbl should
 *  be copied to
 * @nhash_rules_addr: Addr in sys mem where ipv6 non-hashable flt tbl starts
 * @nhash_rules_size: Size in bytes of the non-hashable tbl to cpy to local mem
 * @nhash_local_addr: Addr in shared mem where ipv6 non-hashable flt tbl should
 *  be copied to
 */
struct ipahal_imm_cmd_ip_v6_filter_init {
	u64 hash_rules_addr;
	u32 hash_rules_size;
	u32 hash_local_addr;
	u64 nhash_rules_addr;
	u32 nhash_rules_size;
	u32 nhash_local_addr;
};

/*
 * struct ipahal_imm_cmd_ip_v4_nat_init - IP_V4_NAT_INIT cmd payload
 * Inits IPv4 NAT block. Initiate NAT table with it dimensions, location
 *  cache address abd itger related parameters.
 * @table_index: For future support of multiple NAT tables
 * @ipv4_rules_addr: Addr in sys/shared mem where ipv4 NAT rules start
 * @ipv4_rules_addr_shared: ipv4_rules_addr in shared mem (if not, then sys)
 * @ipv4_expansion_rules_addr: Addr in sys/shared mem where expantion NAT
 *  table starts. IPv4 NAT rules that result in NAT collision are located
 *  in this table.
 * @ipv4_expansion_rules_addr_shared: ipv4_expansion_rules_addr in
 *  shared mem (if not, then sys)
 * @index_table_addr: Addr in sys/shared mem where index table, which points
 *  to NAT table starts
 * @index_table_addr_shared: index_table_addr in shared mem (if not, then sys)
 * @index_table_expansion_addr: Addr in sys/shared mem where expansion index
 *  table starts
 * @index_table_expansion_addr_shared: index_table_expansion_addr in
 *  shared mem (if not, then sys)
 * @size_base_tables: Num of entries in NAT tbl and idx tbl (each)
 * @size_expansion_tables: Num of entries in NAT expantion tbl and expantion
 *  idx tbl (each)
 * @public_ip_addr: public IP address
 */
struct ipahal_imm_cmd_ip_v4_nat_init {
	u8 table_index;
	u64 ipv4_rules_addr;
	bool ipv4_rules_addr_shared;
	u64 ipv4_expansion_rules_addr;
	bool ipv4_expansion_rules_addr_shared;
	u64 index_table_addr;
	bool index_table_addr_shared;
	u64 index_table_expansion_addr;
	bool index_table_expansion_addr_shared;
	u16 size_base_tables;
	u16 size_expansion_tables;
	u32 public_ip_addr;
};

/*
 * struct ipahal_imm_cmd_ip_v4_routing_init - IP_V4_ROUTING_INIT cmd payload
 * Inits IPv4 routing table/structure - with the rules and other related params
 * @hash_rules_addr: Addr in sys mem where ipv4 hashable rt tbl starts
 * @hash_rules_size: Size in bytes of the hashable tbl to cpy to local mem
 * @hash_local_addr: Addr in shared mem where ipv4 hashable rt tbl should
 *  be copied to
 * @nhash_rules_addr: Addr in sys mem where ipv4 non-hashable rt tbl starts
 * @nhash_rules_size: Size in bytes of the non-hashable tbl to cpy to local mem
 * @nhash_local_addr: Addr in shared mem where ipv4 non-hashable rt tbl should
 *  be copied to
 */
struct ipahal_imm_cmd_ip_v4_routing_init {
	u64 hash_rules_addr;
	u32 hash_rules_size;
	u32 hash_local_addr;
	u64 nhash_rules_addr;
	u32 nhash_rules_size;
	u32 nhash_local_addr;
};

/*
 * struct ipahal_imm_cmd_ip_v6_routing_init - IP_V6_ROUTING_INIT cmd payload
 * Inits IPv6 routing table/structure - with the rules and other related params
 * @hash_rules_addr: Addr in sys mem where ipv6 hashable rt tbl starts
 * @hash_rules_size: Size in bytes of the hashable tbl to cpy to local mem
 * @hash_local_addr: Addr in shared mem where ipv6 hashable rt tbl should
 *  be copied to
 * @nhash_rules_addr: Addr in sys mem where ipv6 non-hashable rt tbl starts
 * @nhash_rules_size: Size in bytes of the non-hashable tbl to cpy to local mem
 * @nhash_local_addr: Addr in shared mem where ipv6 non-hashable rt tbl should
 *  be copied to
 */
struct ipahal_imm_cmd_ip_v6_routing_init {
	u64 hash_rules_addr;
	u32 hash_rules_size;
	u32 hash_local_addr;
	u64 nhash_rules_addr;
	u32 nhash_rules_size;
	u32 nhash_local_addr;
};

/*
 * struct ipahal_imm_cmd_hdr_init_local - HDR_INIT_LOCAL cmd payload
 * Inits hdr table within local mem with the hdrs and their length.
 * @hdr_table_addr: Word address in sys mem where the table starts (SRC)
 * @size_hdr_table: Size of the above (in bytes)
 * @hdr_addr: header address in IPA sram (used as DST for memory copy)
 * @rsvd: reserved
 */
struct ipahal_imm_cmd_hdr_init_local {
	u64 hdr_table_addr;
	u32 size_hdr_table;
	u32 hdr_addr;
};

/*
 * struct ipahal_imm_cmd_hdr_init_system - HDR_INIT_SYSTEM cmd payload
 * Inits hdr table within sys mem with the hdrs and their length.
 * @hdr_table_addr: Word address in system memory where the hdrs tbl starts.
 */
struct ipahal_imm_cmd_hdr_init_system {
	u64 hdr_table_addr;
};

/*
 * struct ipahal_imm_cmd_nat_dma - NAT_DMA cmd payload
 * Perform DMA operation on NAT related mem addressess. Copy data into
 *  different locations within NAT associated tbls. (For add/remove NAT rules)
 * @table_index: NAT tbl index. Defines the NAT tbl on which to perform DMA op.
 * @base_addr: Base addr to which the DMA operation should be performed.
 * @offset: offset in bytes from base addr to write 'data' to
 * @data: data to be written
 */
struct ipahal_imm_cmd_nat_dma {
	u8 table_index;
	u8 base_addr;
	u32 offset;
	u16 data;
};

/*
 * struct ipahal_imm_cmd_ip_packet_init - IP_PACKET_INIT cmd payload
 * Configuration for specific IP pkt. Shall be called prior to an IP pkt
 *  data. Pkt will not go through IP pkt processing.
 * @destination_pipe_index: Destination pipe index  (in case routing
 *  is enabled, this field will overwrite the rt  rule)
 */
struct ipahal_imm_cmd_ip_packet_init {
	u32 destination_pipe_index;
};

/*
 * enum ipa_pipeline_clear_option - Values for pipeline clear waiting options
 * @IPAHAL_HPS_CLEAR: Wait for HPS clear. All queues except high priority queue
 *  shall not be serviced until HPS is clear of packets or immediate commands.
 *  The high priority Rx queue / Q6ZIP group shall still be serviced normally.
 *
 * @IPAHAL_SRC_GRP_CLEAR: Wait for originating source group to be clear
 *  (for no packet contexts allocated to the originating source group).
 *  The source group / Rx queue shall not be serviced until all previously
 *  allocated packet contexts are released. All other source groups/queues shall
 *  be serviced normally.
 *
 * @IPAHAL_FULL_PIPELINE_CLEAR: Wait for full pipeline to be clear.
 *  All groups / Rx queues shall not be serviced until IPA pipeline is fully
 *  clear. This should be used for debug only.
 */
enum ipahal_pipeline_clear_option {
	IPAHAL_HPS_CLEAR,
	IPAHAL_SRC_GRP_CLEAR,
	IPAHAL_FULL_PIPELINE_CLEAR
};

/*
 * struct ipahal_imm_cmd_register_write - REGISTER_WRITE cmd payload
 * Write value to register. Allows reg changes to be synced with data packet
 *  and other immediate commands. Can be used to access the sram
 * @offset: offset from IPA base address - Lower 16bit of the IPA reg addr
 * @value: value to write to register
 * @value_mask: mask specifying which value bits to write to the register
 * @skip_pipeline_clear: if to skip pipeline clear waiting (don't wait)
 * @pipeline_clear_option: options for pipeline clear waiting
 */
struct ipahal_imm_cmd_register_write {
	u32 offset;
	u32 value;
	u32 value_mask;
	bool skip_pipeline_clear;
	enum ipahal_pipeline_clear_option pipeline_clear_options;
};

/*
 * struct ipahal_imm_cmd_dma_shared_mem - DMA_SHARED_MEM cmd payload
 * Perform mem copy into or out of the SW area of IPA local mem
 * @size: Size in bytes of data to copy. Expected size is up to 2K bytes
 * @local_addr: Address in IPA local memory
 * @is_read: Read operation from local memory? If not, then write.
 * @skip_pipeline_clear: if to skip pipeline clear waiting (don't wait)
 * @pipeline_clear_option: options for pipeline clear waiting
 * @system_addr: Address in system memory
 */
struct ipahal_imm_cmd_dma_shared_mem {
	u32 size;
	u32 local_addr;
	bool is_read;
	bool skip_pipeline_clear;
	enum ipahal_pipeline_clear_option pipeline_clear_options;
	u64 system_addr;
};

/*
 * struct ipahal_imm_cmd_ip_packet_tag_status - IP_PACKET_TAG_STATUS cmd payload
 * This cmd is used for to allow SW to track HW processing by setting a TAG
 *  value that is passed back to SW inside Packet Status information.
 *  TAG info will be provided as part of Packet Status info generated for
 *  the next pkt transferred over the pipe.
 *  This immediate command must be followed by a packet in the same transfer.
 * @tag: Tag that is provided back to SW
 */
struct ipahal_imm_cmd_ip_packet_tag_status {
	u64 tag;
};

/*
 * struct ipahal_imm_cmd_dma_task_32b_addr - IPA_DMA_TASK_32B_ADDR cmd payload
 * Used by clients using 32bit addresses. Used to perform DMA operation on
 *  multiple descriptors.
 *  The Opcode is dynamic, where it holds the number of buffer to process
 * @cmplt: Complete flag: If true, IPA interrupt SW when the entire
 *  DMA related data was completely xfered to its destination.
 * @eof: Enf Of Frame flag: If true, IPA assert the EOT to the
 *  dest client. This is used used for aggr sequence
 * @flsh: Flush flag: If true pkt will go through the IPA blocks but
 *  will not be xfered to dest client but rather will be discarded
 * @lock: Lock pipe flag: If true, IPA will stop processing descriptors
 *  from other EPs in the same src grp (RX queue)
 * @unlock: Unlock pipe flag: If true, IPA will stop exclusively
 *  servicing current EP out of the src EPs of the grp (RX queue)
 * @size1: Size of buffer1 data
 * @addr1: Pointer to buffer1 data
 * @packet_size: Total packet size. If a pkt send using multiple DMA_TASKs,
 *  only the first one needs to have this field set. It will be ignored
 *  in subsequent DMA_TASKs until the packet ends (EOT). First DMA_TASK
 *  must contain this field (2 or more buffers) or EOT.
 */
struct ipahal_imm_cmd_dma_task_32b_addr {
	bool cmplt;
	bool eof;
	bool flsh;
	bool lock;
	bool unlock;
	u32 size1;
	u32 addr1;
	u32 packet_size;
};

/*
 * struct ipahal_imm_cmd_pyld - Immediate cmd payload information
 * @len: length of the buffer
 * @data: buffer contains the immediate command payload. Buffer goes
 *  back to back with this structure
 */
struct ipahal_imm_cmd_pyld {
	u16 len;
	u8 data[0];
};


/* Immediate command Function APIs */

/*
 * ipahal_imm_cmd_name_str() - returns string that represent the imm cmd
 * @cmd_name: [in] Immediate command name
 */
const char *ipahal_imm_cmd_name_str(enum ipahal_imm_cmd_name cmd_name);

/*
 * ipahal_imm_cmd_get_opcode() - Get the fixed opcode of the immediate command
 */
u16 ipahal_imm_cmd_get_opcode(enum ipahal_imm_cmd_name cmd);

/*
 * ipahal_imm_cmd_get_opcode_param() - Get the opcode of an immediate command
 *  that supports dynamic opcode
 * Some commands opcode are not totaly fixed, but part of it is
 *  a supplied parameter. E.g. Low-Byte is fixed and Hi-Byte
 *  is a given parameter.
 * This API will return the composed opcode of the command given
 *  the parameter
 * Note: Use this API only for immediate comamnds that support Dynamic Opcode
 */
u16 ipahal_imm_cmd_get_opcode_param(enum ipahal_imm_cmd_name cmd, int param);

/*
 * ipahal_construct_imm_cmd() - Construct immdiate command
 * This function builds imm cmd bulk that can be be sent to IPA
 * The command will be allocated dynamically.
 * After done using it, call ipahal_destroy_imm_cmd() to release it
 */
struct ipahal_imm_cmd_pyld *ipahal_construct_imm_cmd(
	enum ipahal_imm_cmd_name cmd, const void *params, bool is_atomic_ctx);

/*
 * ipahal_construct_nop_imm_cmd() - Construct immediate comamnd for NO-Op
 * Core driver may want functionality to inject NOP commands to IPA
 *  to ensure e.g., PIPLINE clear before someother operation.
 * The functionality given by this function can be reached by
 *  ipahal_construct_imm_cmd(). This function is helper to the core driver
 *  to reach this NOP functionlity easily.
 * @skip_pipline_clear: if to skip pipeline clear waiting (don't wait)
 * @pipline_clr_opt: options for pipeline clear waiting
 * @is_atomic_ctx: is called in atomic context or can sleep?
 */
struct ipahal_imm_cmd_pyld *ipahal_construct_nop_imm_cmd(
	bool skip_pipline_clear,
	enum ipahal_pipeline_clear_option pipline_clr_opt,
	bool is_atomic_ctx);

/*
 * ipahal_destroy_imm_cmd() - Destroy/Release bulk that was built
 *  by the construction functions
 */
static inline void ipahal_destroy_imm_cmd(struct ipahal_imm_cmd_pyld *pyld)
{
	kfree(pyld);
}

int ipahal_init(enum ipa_hw_type ipa_hw_type, void __iomem *base);
void ipahal_destroy(void);

#endif /* _IPAHAL_H_ */
