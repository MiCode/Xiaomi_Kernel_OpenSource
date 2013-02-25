/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_HW_DEFS_H
#define _IPA_HW_DEFS_H
#include <linux/bitops.h>

/* This header defines various HW related data types */

/* immediate command op-codes */
#define IPA_DECIPH_INIT        (1)
#define IPA_PPP_FRM_INIT       (2)
#define IPA_IP_V4_FILTER_INIT  (3)
#define IPA_IP_V6_FILTER_INIT  (4)
#define IPA_IP_V4_NAT_INIT     (5)
#define IPA_IP_V6_NAT_INIT     (6)
#define IPA_IP_V4_ROUTING_INIT (7)
#define IPA_IP_V6_ROUTING_INIT (8)
#define IPA_HDR_INIT_LOCAL     (9)
#define IPA_HDR_INIT_SYSTEM   (10)
#define IPA_DECIPH_SETUP      (11)
#define IPA_INSERT_NAT_RULE   (12)
#define IPA_DELETE_NAT_RULE   (13)
#define IPA_NAT_DMA           (14)
#define IPA_IP_PACKET_TAG     (15)
#define IPA_IP_PACKET_INIT    (16)

#define IPA_INTERFACE_ID_EXCEPTION         (0)
#define IPA_INTERFACE_ID_A2_WWAN        (0x10)
#define IPA_INTERFACE_ID_HSUSB_RMNET1   (0x21)
#define IPA_INTERFACE_ID_HSUSB_RMNET2   (0x22)
#define IPA_INTERFACE_ID_HSUSB_RMNET3   (0x23)
#define IPA_INTERFACE_ID_HSIC_WLAN_WAN  (0x31)
#define IPA_INTERFACE_ID_HSIC_WLAN_LAN1 (0x32)
#define IPA_INTERFACE_ID_HSIC_WLAN_LAN2 (0x33)
#define IPA_INTERFACE_ID_HSIC_RMNET1    (0x41)
#define IPA_INTERFACE_ID_HSIC_RMNET2    (0x42)
#define IPA_INTERFACE_ID_HSIC_RMNET3    (0x43)
#define IPA_INTERFACE_ID_HSIC_RMNET4    (0x44)
#define IPA_INTERFACE_ID_HSIC_RMNET5    (0x45)

/**
 * struct ipa_flt_rule_hw_hdr - HW header of IPA filter rule
 * @word: filtering rule properties
 * @en_rule: enable rule
 * @action: post routing action
 * @rt_tbl_idx: index in routing table
 * @rsvd: reserved
 */
struct ipa_flt_rule_hw_hdr {
	union {
		u32 word;
		struct {
			u32 en_rule:16;
			u32 action:5;
			u32 rt_tbl_idx:5;
			u32 rsvd:6;
		} hdr;
	} u;
};

/**
 * struct ipa_rt_rule_hw_hdr - HW header of IPA routing rule
 * @word: filtering rule properties
 * @en_rule: enable rule
 * @pipe_dest_idx: destination pipe index
 * @system: changed from local to system due to HW change
 * @hdr_offset: header offset
 */
struct ipa_rt_rule_hw_hdr {
	union {
		u32 word;
		struct {
			u32 en_rule:16;
			u32 pipe_dest_idx:5;
			u32 system:1;
			u32 hdr_offset:10;
		} hdr;
	} u;
};

/**
 * struct ipa_ip_v4_filter_init - IPA_IP_V4_FILTER_INIT command payload
 * @ipv4_rules_addr: address of ipv4 rules
 * @size_ipv4_rules: size of the above
 * @ipv4_addr: ipv4 address
 * @rsvd: reserved
 */
struct ipa_ip_v4_filter_init {
	u64 ipv4_rules_addr:32;
	u64 size_ipv4_rules:12;
	u64 ipv4_addr:16;
	u64 rsvd:4;
};

/**
 * struct ipa_ip_v6_filter_init - IPA_IP_V6_FILTER_INIT command payload
 * @ipv6_rules_addr: address of ipv6 rules
 * @size_ipv6_rules: size of the above
 * @ipv6_addr: ipv6 address
 */
struct ipa_ip_v6_filter_init {
	u64 ipv6_rules_addr:32;
	u64 size_ipv6_rules:16;
	u64 ipv6_addr:16;
};

/**
 * struct ipa_ip_v4_routing_init - IPA_IP_V4_ROUTING_INIT command payload
 * @ipv4_rules_addr: address of ipv4 rules
 * @size_ipv4_rules: size of the above
 * @ipv4_addr: ipv4 address
 * @rsvd: reserved
 */
struct ipa_ip_v4_routing_init {
	u64 ipv4_rules_addr:32;
	u64 size_ipv4_rules:12;
	u64 ipv4_addr:16;
	u64 rsvd:4;
};

/**
 * struct ipa_ip_v6_routing_init - IPA_IP_V6_ROUTING_INIT command payload
 * @ipv6_rules_addr: address of ipv6 rules
 * @size_ipv6_rules: size of the above
 * @ipv6_addr: ipv6 address
 */
struct ipa_ip_v6_routing_init {
	u64 ipv6_rules_addr:32;
	u64 size_ipv6_rules:16;
	u64 ipv6_addr:16;
};

/**
 * struct ipa_hdr_init_local - IPA_HDR_INIT_LOCAL command payload
 * @hdr_table_addr: address of header table
 * @size_hdr_table: size of the above
 * @hdr_addr: header address
 * @rsvd: reserved
 */
struct ipa_hdr_init_local {
	u64 hdr_table_addr:32;
	u64 size_hdr_table:12;
	u64 hdr_addr:16;
	u64 rsvd:4;
};

/**
 * struct ipa_hdr_init_system - IPA_HDR_INIT_SYSTEM command payload
 * @hdr_table_addr: address of header table
 * @rsvd: reserved
 */
struct ipa_hdr_init_system {
	u64 hdr_table_addr:32;
	u64 rsvd:32;
};

#define IPA_A5_MUX_HDR_EXCP_FLAG_IP		BIT(7)
#define IPA_A5_MUX_HDR_EXCP_FLAG_NAT		BIT(6)
#define IPA_A5_MUX_HDR_EXCP_FLAG_SW_FLT	BIT(5)
#define IPA_A5_MUX_HDR_EXCP_FLAG_TAG		BIT(4)
#define IPA_A5_MUX_HDR_EXCP_FLAG_REPLICATED	BIT(3)
#define IPA_A5_MUX_HDR_EXCP_FLAG_IHL		BIT(2)

/**
 * struct ipa_a5_mux_hdr - A5 MUX header definition
 * @interface_id: interface ID
 * @src_pipe_index: source pipe index
 * @flags: flags
 * @metadata: metadata
 *
 * A5 MUX header is in BE, A5 runs in LE. This struct definition
 * allows A5 SW to correctly parse the header
 */
struct ipa_a5_mux_hdr {
	u16 interface_id;
	u8 src_pipe_index;
	u8 flags;
	u32 metadata;
};

/**
 * struct ipa_nat_dma - IPA_NAT_DMA command payload
 * @table_index: NAT table index
 * @rsvd1: reserved
 * @base_addr: base address
 * @rsvd2: reserved
 * @offset: offset
 * @data: metadata
 * @rsvd3: reserved
 */
struct ipa_nat_dma {
	u64 table_index:3;
	u64 rsvd1:1;
	u64 base_addr:2;
	u64 rsvd2:2;
	u64 offset:32;
	u64 data:16;
	u64 rsvd3:8;
};

/**
 * struct ipa_nat_dma - IPA_IP_PACKET_INIT command payload
 * @destination_pipe_index: destination pipe index
 * @rsvd1: reserved
 * @metadata: metadata
 * @rsvd2: reserved
 */
struct ipa_ip_packet_init {
	u64 destination_pipe_index:5;
	u64 rsvd1:3;
	u64 metadata:32;
	u64 rsvd2:24;
};

/**
 * struct ipa_nat_dma - IPA_IP_V4_NAT_INIT command payload
 * @ipv4_rules_addr: ipv4 rules address
 * @ipv4_expansion_rules_addr: ipv4 expansion rules address
 * @index_table_addr: index tables address
 * @index_table_expansion_addr: index expansion table address
 * @table_index: index in table
 * @ipv4_rules_addr_type: ipv4 address type
 * @ipv4_expansion_rules_addr_type: ipv4 expansion address type
 * @index_table_addr_type: index table address type
 * @index_table_expansion_addr_type: index expansion table type
 * @size_base_tables: size of base tables
 * @size_expansion_tables: size of expansion tables
 * @rsvd2: reserved
 * @public_ip_addr: public IP address
 */
struct ipa_ip_v4_nat_init {
	u64 ipv4_rules_addr:32;
	u64 ipv4_expansion_rules_addr:32;
	u64 index_table_addr:32;
	u64 index_table_expansion_addr:32;
	u64 table_index:3;
	u64 rsvd1:1;
	u64 ipv4_rules_addr_type:1;
	u64 ipv4_expansion_rules_addr_type:1;
	u64 index_table_addr_type:1;
	u64 index_table_expansion_addr_type:1;
	u64 size_base_tables:12;
	u64 size_expansion_tables:10;
	u64 rsvd2:2;
	u64 public_ip_addr:32;
};

/**
 * struct ipa_ip_packet_tag - IPA_IP_PACKET_TAG command payload
 * @tag: tag value returned with response
 */
struct ipa_ip_packet_tag {
	u32 tag;
};

#endif /* _IPA_HW_DEFS_H */
