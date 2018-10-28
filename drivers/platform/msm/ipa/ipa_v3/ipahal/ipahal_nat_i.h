/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#ifndef _IPAHAL_NAT_I_H_
#define _IPAHAL_NAT_I_H_

#include <linux/msm_ipa.h>

/* ----------------------- IPv4 NAT Table Entry  -------------------------
 *
 * -----------------------------------------------------------------------
 * |  7  |     6    |   5    |   4   |    3        | 2 |    1    |    0  |
 * -----------------------------------------------------------------------
 * |           Target IP(4B)         |             Private IP(4B)        |
 * -----------------------------------------------------------------------
 * |Target Port(2B) |Private Port(2B)| Public Port(2B) | Next Index(2B)  |
 * -----------------------------------------------------------------------
 * |Proto|      TimeStamp(3B)        |     Flags(2B)   |IP check sum Diff|
 * |(1B) |                           |EN|Redirect|Resv |        (2B)     |
 * -----------------------------------------------------------------------
 * |TCP/UDP checksum|  PDN info(2B)  |    SW Specific Parameters(4B)     |
 * |    diff (2B)   |Info|Resv       |index table entry|  prev index     |
 * -----------------------------------------------------------------------
 */
struct ipa_nat_hw_ipv4_entry {
	/* An IP address can't be bit-field, because its address is used */
	u32 private_ip;
	u32 target_ip;

	u32 next_index : 16;
	u32 public_port : 16;
	u32 private_port : 16;
	u32 target_port : 16;
	u32 ip_chksum : 16;

	u32 rsvd1 : 14;
	u32 redirect : 1;
	u32 enable : 1;

	u32 time_stamp : 24;
	u32 protocol : 8;

	u32 prev_index : 16;
	u32 indx_tbl_entry : 16;

	u32 rsvd2 : 12;
	u32 pdn_index : 4; /* IPA 4.0 and greater */

	u32 tcp_udp_chksum : 16;
};

/*--- IPV4 NAT Index Table Entry --
 *---------------------------------
 *|   3   |   2   |   1   |   0   |
 *---------------------------------
 *|next index(2B) |table entry(2B)|
 *---------------------------------
 */
struct ipa_nat_hw_indx_entry {
	u16 tbl_entry;
	u16 next_index;
};

/**
 * struct ipa_nat_hw_pdn_entry - IPA PDN config table entry
 * @public_ip: the PDN's public ip
 * @src_metadata: the PDN's metadata to be replaced for source NAT
 * @dst_metadata: the PDN's metadata to be replaced for destination NAT
 * @resrvd: reserved field
 * ---------------------------------
 * |   3   |   2   |   1   |   0   |
 * ---------------------------------
 * |        public_ip (4B)         |
 * ---------------------------------
 * |      src_metadata (4B)        |
 * ---------------------------------
 * |      dst_metadata (4B)        |
 * ---------------------------------
 * |         resrvd (4B)           |
 * ---------------------------------
 */
struct ipa_nat_hw_pdn_entry {
	u32 public_ip;
	u32 src_metadata;
	u32 dst_metadata;
	u32 resrvd;
};

/*-------------------------  IPV6CT Table Entry  ------------------------------
 *-----------------------------------------------------------------------------
 *|   7    |      6      |  5  |  4   |        3         |  2  |   1  |   0   |
 *-----------------------------------------------------------------------------
 *|                   Outbound Src IPv6 Address (8 LSB Bytes)                 |
 *-----------------------------------------------------------------------------
 *|                   Outbound Src IPv6 Address (8 MSB Bytes)                 |
 *-----------------------------------------------------------------------------
 *|                   Outbound Dest IPv6 Address (8 LSB Bytes)                |
 *-----------------------------------------------------------------------------
 *|                   Outbound Dest IPv6 Address (8 MSB Bytes)                |
 *-----------------------------------------------------------------------------
 *|Protocol|      TimeStamp (3B)      |       Flags (2B)       |Reserved (2B) |
 *|  (1B)  |                          |Enable|Redirect|Resv    |              |
 *-----------------------------------------------------------------------------
 *|Reserved|Direction(1B)|Src Port(2B)|     Dest Port (2B)     |Next Index(2B)|
 *|  (1B)  |IN|OUT|Resv  |            |                        |              |
 *-----------------------------------------------------------------------------
 *|    SW Specific Parameters(4B)     |                Reserved (4B)          |
 *|    Prev Index (2B)   |Reserved(2B)|                                       |
 *-----------------------------------------------------------------------------
 *|                            Reserved (8B)                                  |
 *-----------------------------------------------------------------------------
 */
struct ipa_nat_hw_ipv6ct_entry {
	/* An IP address can't be bit-field, because its address is used */
	u64 src_ipv6_lsb;
	u64 src_ipv6_msb;
	u64 dest_ipv6_lsb;
	u64 dest_ipv6_msb;

	u64 rsvd1 : 30;
	u64 redirect : 1;
	u64 enable : 1;

	u64 time_stamp : 24;
	u64 protocol : 8;

	u64 next_index : 16;
	u64 dest_port : 16;
	u64 src_port : 16;
	u64 rsvd2 : 6;
	u64 out_allowed : 1;
	u64 in_allowed : 1;
	u64 rsvd3 : 8;

	u64 rsvd4 : 48;
	u64 prev_index : 16;

	u64 rsvd5 : 64;
};

int ipahal_nat_init(enum ipa_hw_type ipa_hw_type);

#endif /* _IPAHAL_NAT_I_H_ */

