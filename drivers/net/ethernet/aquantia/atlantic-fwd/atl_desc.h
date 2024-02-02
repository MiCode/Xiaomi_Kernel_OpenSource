/* SPDX-License-Identifier: GPL-2.0-only */
/* Atlantic Network Driver
 *
 * Copyright (C) 2017 aQuantia Corporation
 * Copyright (C) 2019-2020 Marvell International Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _ATL_DESC_H_
#define _ATL_DESC_H_

#include <linux/kernel.h>

#if defined(__LITTLE_ENDIAN_BITFIELD)
struct atl_tx_ctx {
	unsigned long long :40; //0
	unsigned tun_len:8;     //40
	unsigned out_len:16;    //48
	unsigned type:3;        //64
	unsigned idx:1;         //67
	unsigned vlan_tag:16;   //68
	unsigned cmd:4;         //84
	unsigned l2_len:7;      //88
	unsigned l3_len:9;      //95
	unsigned l4_len:8;      //104
	unsigned mss_len:16;    //112
} __attribute__((packed));

struct atl_tx_desc {
	unsigned long long daddr:64; //0
	unsigned type:3;        //64
	unsigned :1;            //67
	unsigned len:16;        //68
	unsigned dd:1;          //84
	unsigned eop:1;         //85
	unsigned cmd:8;         //86
	unsigned :14;           //94
	unsigned ct_idx:1;      //108
	unsigned ct_en:1;       //109
	unsigned pay_len:18;    //110
} __attribute__((packed));

#define ATL_DATA_PER_TXD 16384 // despite ->len being 16 bits

enum atl_tx_desc_type {
	tx_desc_type_desc = 1,
	tx_desc_type_context = 2,
};

enum atl_tx_desc_cmd {
	tx_desc_cmd_vlan = 1,
	tx_desc_cmd_fcs = 2,
	tx_desc_cmd_ipv4cs = 4,
	tx_desc_cmd_l4cs = 8,
	tx_desc_cmd_lso = 0x10,
	tx_desc_cmd_wb = 0x20,
};

enum atl_tx_ctx_cmd {
	ctx_cmd_snap = 1, // SNAP / ~802.3
	ctx_cmd_ipv6 = 2, // IPv6 / ~IPv4
	ctx_cmd_tcp = 4,  // TCP / ~UDP
};

struct atl_rx_desc {
	uint64_t daddr;      			//0
	union {
		struct {
			unsigned dd:1;		//64
			uint64_t haddr63:63;	//65
		};
		uint64_t haddr;
	};
} __attribute__((packed));

struct atl_rx_desc_wb {
	unsigned rss_type:4;    //0
	unsigned pkt_type:8;    //4
	unsigned rdm_err:1;     //12
	unsigned :6;            //13
	unsigned rx_cntl:2;     //19
	unsigned sph:1;         //21
	unsigned hdr_len:10;    //22
	unsigned rss_hash:32;   //32
	unsigned dd:1;          //64
	unsigned eop:1;         //65
	unsigned rx_stat:4;     //66
	unsigned rx_estat:6;    //70
	unsigned rsc_cnt:4;     //76
	unsigned pkt_len:16;    //80
	unsigned next_desp:16;  //96
	unsigned vlan_tag:16;   //112
} __attribute__((packed));

enum atl_rx_stat {
	atl_rx_stat_mac_err = 1,
	atl_rx_stat_ipv4_err = 2,
	atl_rx_stat_l4_err = 4,
	atl_rx_stat_l4_valid = 8,
	atl_rx_stat_err_msk = atl_rx_stat_mac_err | atl_rx_stat_ipv4_err |
		atl_rx_stat_l4_err,
};

enum atl_rx_estat {
	atl_rx_estat_vlan_stripped = 1,
	atl_rx_estat_l2_ucast_match = 2,
	atl_rx_estat_vxlan = 1 << 2,
	atl_rx_estat_nvgre = 2 << 2,
	atl_rx_estat_geneve = 3 << 2,
	atl_rx_estat_tun_msk = 3 << 2,
	atl_rx_estat_outer_ipv4_err = 16,
	atl_rx_estat_outer_ipv4_valid = 32,
};

enum atl_rx_pkt_type {
	atl_rx_pkt_type_ipv4 = 0,
	atl_rx_pkt_type_ipv6 = 1,
	atl_rx_pkt_type_l3_other = 2,
	atl_rx_pkt_type_l3_arp = 3,
	atl_rx_pkt_type_l3_msk = 3,
	atl_rx_pkt_type_tcp = 0 << 2,
	atl_rx_pkt_type_udp = 1 << 2 ,
	atl_rx_pkt_type_sctp = 2 << 2,
	atl_rx_pkt_type_icmp = 3 << 2,
	atl_rx_pkt_type_l4_msk = ((1 << 3) - 1) << 2,
	atl_rx_pkt_type_vlan = 1 << 5,
	atl_rx_pkt_type_dbl_vlan = 2 << 5,
	atl_rx_pkt_type_vlan_msk = ((1 << 2) - 1) << 5,
};

#else // defined(__LITTLE_ENDIAN_BITFIELD)
#error XXX Fix bigendian bitfields
#endif // defined(__LITTLE_ENDIAN_BITFIELD)

union atl_desc{
	struct atl_rx_desc rx;
	struct atl_rx_desc_wb wb;
	struct atl_tx_ctx ctx;
	struct atl_tx_desc tx;
	uint8_t raw[16];
}__attribute__((packed));


#endif
