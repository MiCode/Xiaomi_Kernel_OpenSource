/* Copyright (c) 2018, 2020, The Linux Foundation. All rights reserved.
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
 * |(1B) |                           |                 |        (2B)     |
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

	/*---------------------------------------------------
	 *IPA NAT Flag is interpreted as follows
	 *---------------------------------------------------
	 *|  EN   |FIN/RST|  S   | IPv4 uC activation index |
	 *| [15]  | [14]  | [13] |          [12:0]          |
	 *---------------------------------------------------
	 */
	u32 uc_activation_index: 13;
	u32 s : 1;
	u32 redirect : 1;
	u32 enable : 1;

	u32 time_stamp : 24;
	u32 protocol : 8;

	/*--------------------------------------------------
	 *32 bit sw_spec_params is interpreted as follows
	 *------------------------------------
	 *|     16 bits     |     16 bits    |
	 *------------------------------------
	 *|  index table    |  prev index    |
	 *|     entry       |                |
	 *------------------------------------
	 */
	u32 prev_index : 16;
	u32 indx_tbl_entry : 16;

	u32 rsvd2 : 11; //including next 3 reserved buts

	/*-----------------------------------------
	 *8 bit PDN info is interpreted as following
	 *-----------------------------------------------------
	 *|     4 bits      |     1 bit      |     3 bits     |
	 *-----------------------------------------------------
	 *|  PDN index      |  uC processing |     Reserved   |
	 *|      [7:4]      |       [3]      |      [2:0]     |
	 *-----------------------------------------------------
	 */
	u32 ucp : 1; /* IPA 4.0 and greater */
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
 *|   7    |      6      |  5  |  4   |      3      |   2   |   1    |   0    |
 *-----------------------------------------------------------------------------
 *|                   Outbound Src IPv6 Address (8 LSB Bytes)                 |
 *-----------------------------------------------------------------------------
 *|                   Outbound Src IPv6 Address (8 MSB Bytes)                 |
 *-----------------------------------------------------------------------------
 *|                   Outbound Dest IPv6 Address (8 LSB Bytes)                |
 *-----------------------------------------------------------------------------
 *|                   Outbound Dest IPv6 Address (8 MSB Bytes)                |
 *-----------------------------------------------------------------------------
 *|Protocol|      TimeStamp (3B)      |       Flags (2B)    |Rsvd   |S |uC ACT|
 *|  (1B)  |                          |Enable|Redirect|Resv |[15:14]|13|[12:0]|
 *-----------------------------------------------------------------------------
 *|Reserved|Settings|    Src Port(2B) |   Dest Port (2B)    |  Next Index(2B) |
 *|  (1B)  |  (1B)  |                 |                     |                 |
 *-----------------------------------------------------------------------------
 *|    SW Specific Parameters(4B)     |                Reserved (4B)          |
 *|    Prev Index (2B)   |Reserved(2B)|                                       |
 *-----------------------------------------------------------------------------
 *|                            Reserved (8B)                                  |
 *-----------------------------------------------------------------------------
 *
 * Settings
 *-----------------------------------------------
 *|IN Allowed|OUT Allowed|Reserved|uC processing|
 *|[7:7]     |[6:6]      |[5:1]   |[0:0]        |
 *-----------------------------------------------
 */
struct ipa_nat_hw_ipv6ct_entry {
	/* An IP address can't be bit-field, because its address is used */
	u64 src_ipv6_lsb;
	u64 src_ipv6_msb;
	u64 dest_ipv6_lsb;
	u64 dest_ipv6_msb;

	u64 uc_activation_index : 13;
	u64 s : 1;
	u64 rsvd1 : 16;
	u64 redirect : 1;
	u64 enable : 1;

	u64 time_stamp : 24;
	u64 protocol : 8;

	u64 next_index : 16;
	u64 dest_port : 16;
	u64 src_port : 16;
	u64 ucp : 1;
	u64 rsvd2 : 5;
	u64 out_allowed : 1;
	u64 in_allowed : 1;
	u64 rsvd3 : 8;

	u64 rsvd4 : 48;
	u64 prev_index : 16;

	u64 rsvd5 : 64;
};

int ipahal_nat_init(enum ipa_hw_type ipa_hw_type);

#endif /* _IPAHAL_NAT_I_H_ */

