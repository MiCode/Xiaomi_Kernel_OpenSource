/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __SLA_H__
#define __SLA_H__

/* Includes */
#include <linux/kernel.h>
#include <linux/timer.h>
#include <linux/ip.h>
#include <net/tcp.h>
#include <net/udp.h>
#include <net/genetlink.h>
#include <linux/sipa.h>
#include <linux/types.h>
#include <linux/kern_levels.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <linux/jhash.h>
#include <linux/atomic.h>
#include <linux/refcount.h>
#include <linux/netdevice.h>
#include <linux/of_device.h>

#define SLA_OK		0x00  /* Operation succeeded */
#define SLA_FAIL	0x01   /* Operation failed */

extern unsigned int sfp_stats_bytes;

enum {
	IP_L4_PROTO_NULL	= 0,
	IP_L4_PROTO_ICMP	= 1,
	IP_L4_PROTO_TCP		= 6,	 /* Transmission Control Protocol */
	IP_L4_PROTO_UDP		= 17,	/* User Datagram Protocol */
	IP_L4_PROTO_ICMP6	= 58,
	IP_L4_PROTO_RAW		= 255,	/* Raw IP packets */
	IP_L4_PROTO_MAX
};

enum sfp_tcp_flag_set {
	TCP_SYN_SET,
	TCP_SYNACK_SET,
	TCP_FIN_SET,
	TCP_ACK_SET,
	TCP_RST_SET,
	TCP_NONE_SET,
};

enum sla_attrs {
	SLA_A_UNSPEC,
	SLA_A_ENABLE,
	SLA_A_IFACE,
	SLA_A_PID,
	SLA_A_APP_UID,
	SLA_A_WHITE_LIST_APP,
	SLA_A_WIFI_SCORE,
	SLA_A_CELLUAR_SCORE,
	__SLA_A_MAX
};

#define SLA_A_MAX (__SLA_A_MAX - 1)
#define IFACE_LEN 64
#define SLA_DEV_TYPE_MAX (__SLA_DEV_TYPE_MAX - 1)
enum dev_type {
	WEIGHT_STATE_UNSPEC,
	WEIGHT_STATE_USELESS,
	WEIGHT_STATE_NORMAL,
	__WEIGHT_STATE_MAX
};

enum congestion_level {
	CONGESTION_LEVE_UNSPEC,
	CONGESTION_LEVEL_NORMAL,
	CONGESTION_LEVEL_HIGH,
	__CONGESTION_LEVE_MAX
};

enum weight_state {
	SLA_DEV_TYPE_UNSPEC,
	SLA_DEV_TYPE_WLAN,
	SLA_DEV_TYPE_CELLUAR,
	__SLA_DEV_TYPE_MAX
};

enum work_mode {
	SLA_WORK_MODE_UNSPEC,
	SLA_WORK_MODE_DUAL_WIFI,
	SLA_WORK_MODE_WIF_CELLUAR,
	__SLA_WORK_MODE_MAX
};

enum sla_commands {
	__SLA_CMD_UNSPEC,
	SLA_NL_CMD_SLA_ENABLE,
	SLA_NL_CMD_SLA_DISABLE,
	SLA_NL_CMD_SLA_IFACE_CHANGED,
	SLA_NL_CMD_NOTIFY_PID,
	SLA_NL_CMD_APP_UID,
	SLA_NL_CMD_WHITE_LIST_APP,
	SLA_NL_CMD_WIFI_SCORE,
	SLA_NL_CMD_CELLUAR_SCORE,
	SLA_NL_NOTIFY_ENABLE,
	SLA_NL_NOTIFY_DISABLE,
	SLA_NL_NOTIFY_GAME_RTT,
	SLA_NL_NOTIFY_SPEED_RTT,
	SLA_NL_NOTIFY_ENABLED,
	SLA_NL_NOTIFY_DISABLED,
	SLA_NL_NOTIFY_SHOW_DIALOG_NOW,
	SLA_NL_NOTIFY_APP_TRAFFIC,
	SLA_NL_NOTIFY_GAME_APP_STATISTIC,
	SLA_NL_NOTIFY_GAME_RX_PKT,
	SLA_CMD_MAX,
};

struct sla_dev_info {
	bool need_up;
	bool need_disable;
	int max_speed;
	int download_speed;
	int dl_max_speed;
	int download_num;
	int little_speed_num;
	int tmp_little_speed;
	int dl_little_speed;
	int dual_wifi_download;
	int cur_speed;
	int left_speed;
	int minute_speed;
	int download_flag;
	int congestion_flag;
	int if_up;
	int syn_retran;
	int wlan_score;
	int wlan_score_bad_count;
	int weight;
	int weight_state;
	int rtt_index;
	u32 mark;
	u32 avg_rtt;
	u32 sum_rtt;
	u32 sla_rtt_num;
	u32 sla_avg_rtt;
	u64 total_bytes;
	u64 minute_rx_bytes;
	u64 minute_tx_bytes;
	u64 dl_total_bytes;
	char ifname[IFACE_LEN];
};

struct sla_net_stats_info {
	int		msg;		/* message type */
	int		if_index;	/* network if devie index */
	unsigned long	rtt;		/* the average rtt on the network if, unit is ms */
	unsigned long	rx_rate;	/* the rx rate on the network if, unit is bps */
	unsigned long	tx_rate;	/* the tx rate on the network interface, unit is bps */
	unsigned int	retran_rate;	/* retran rate on the net if, example 2  starnd for 2% */
};

#define IP_SFT(ip, x) (((ip) >> (x)) & 0xFF)

// /* Debug control */
#define SLA_DEBUG

#define SLA_PRT_NO		0x0000
#define SLA_PRT_ERR		0x0001
#define SLA_PRT_WARN		0x0002
#define SLA_PRT_DEBUG		0x0004
#define SLA_PRT_INFO		0x0008
#define SLA_PRT_DETAIL		0x0010
#define SLA_PRT_ALL		0x3

#ifdef SLA_DEBUG
extern unsigned int sla_dbg_lvl;
#define SLA_LOG_TAG  "SLA"
#define SLA_PRT_DBG(FLG, fmt, arg...) {\
	if (sla_dbg_lvl & (FLG))\
		pr_info("SLA:" fmt, ##arg);\
	}
#else
#define SLA_PRT_DBG(FLG, fmt, arg...)
#endif

#define IPID "id(%x)"
#define TCP_FMT "seq: %x, ack: %x, %d -> %d"

#define TCPF_SYN			0X02
#define TCPF_RST			0X04
#define TCPF_ACK			0X10
#define TCPF_FINACK			0X11
#define TCPF_SYNACK			0X12
#define TCPF_RSTACK			0X14
#define TCPF_PUSHACK			0X18
#define TCPF_FINPUSHACK			0X19

static inline const char *get_tcp_flag(struct tcphdr *hp)
{
	static const char s_tcp_state[9][16] = {
		"UNKNOWN",
		"TCP_SYN",
		"TCP_RST",
		"TCP_ACK",
		"TCP_FINACK",
		"TCP_SYNACK",
		"TCP_RSTACK",
		"TCP_PUSHACK",
		"TCP_FINPUSHACK",
	};
	char *p = (char *)hp;
	char flag = *(p + 13) & 0x1f;

	switch (flag) {
	case TCPF_SYN:
		return s_tcp_state[1];
	case TCPF_RST:
		return s_tcp_state[2];
	case TCPF_ACK:
		return s_tcp_state[3];
	case TCPF_FINACK:
		return s_tcp_state[4];
	case TCPF_SYNACK:
		return s_tcp_state[5];
	case TCPF_RSTACK:
		return s_tcp_state[6];
	case TCPF_PUSHACK:
		return s_tcp_state[7];
	case TCPF_FINPUSHACK:
		return s_tcp_state[8];
	default:
		break;
	}

	return s_tcp_state[0];
}

int sla_netlink_init(void);
void sla_netlink_exit(void);
int nf_sla_hook_init(void);
void nf_sla_hook_uninit(void);
int sla_net_stats_init(void);
void sla_net_stats_exit(void);
int sla_net_stats_get_netinfo(int if_index, struct sla_net_stats_info *info);
void sla_net_stats_set(int val);

#endif /*__SLA_H__*/
