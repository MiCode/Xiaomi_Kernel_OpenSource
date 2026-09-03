/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 */

#ifndef _IMS_BRIDGE_H
#define _IMS_BRIDGE_H

#include <linux/in.h>
#include <linux/in6.h>
/**
 * enum imsbr_call_state
 *
 * @IMSBR_CALLS_END: end the calling
 * @IMSBR_CALLS_VOWIFI: start vowifi calling
 * @IMSBR_CALLS_VOLTE: start volte calling
 *
 */
enum imsbr_call_state {
	IMSBR_CALLS_UNSPEC,
	IMSBR_CALLS_END,
	IMSBR_CALLS_VOWIFI,
	IMSBR_CALLS_VOLTE,
	__IMSBR_CALLS_MAX
};

#define IMSBR_GENL_NAME		"imsbr"
#define IMSBR_GENL_VERSION	0x1

#define MAX_ESPS 10
#define MARK_FRAG_NEED 10	/* mark fragmentation needed packet */

union imsbr_inet_addr {
	__u32		all[4];
	__be32		ip;
	__be32		ip6[4];
	struct in_addr	in;
	struct in6_addr	in6;
};

/**
 * enum imsbr_media_types
 *
 * @IMSBR_MEDIA_SIP: sip flow
 * @IMSBR_MEDIA_RTP_AUDIO: rtp audio flow
 * @IMSBR_MEDIA_RTP_VIDEO: rtp video flow
 * @IMSBR_MEDIA_RTCP_AUDIO: rtcp audio flow
 * @IMSBR_MEDIA_RTCP_VIDEO: rtcp video flow
 * @IMSBR_MEDIA_IKE: ike flow
 *
 */
enum imsbr_media_types {
	IMSBR_MEDIA_UNSPEC,
	IMSBR_MEDIA_SIP,
	IMSBR_MEDIA_RTP_AUDIO,
	IMSBR_MEDIA_RTP_VIDEO,
	IMSBR_MEDIA_RTCP_AUDIO,
	IMSBR_MEDIA_RTCP_VIDEO,
	IMSBR_MEDIA_IKE,
    IMSBR_MEDIA_SS,
	__IMSBR_MEDIA_MAX
};

/**
 * enum imsbr_link_types
 *
 * @IMSBR_LINK_AP: desirable to send and receive data via AP link(WiFi?)
 * @IMSBR_LINK_CP: desirable to send and receive data via CP link(LTE?)
 *
 */
enum imsbr_link_types {
	IMSBR_LINK_UNSPEC,
	IMSBR_LINK_AP,
	IMSBR_LINK_CP,
	__IMSBR_LINK_MAX
};

/**
 * enum imsbr_socket_types
 *
 * @IMSBR_SOCKET_AP: Data processing engine is located at AP
 * @IMSBR_SOCKET_CP: Data processing engine is located at CP
 *
 */
enum imsbr_socket_types {
	IMSBR_SOCKET_UNSPEC,
	IMSBR_SOCKET_AP,
	IMSBR_SOCKET_CP,
	__IMSBR_SOCKET_MAX
};

struct imsbr_tuple {
	union imsbr_inet_addr local_addr;
	union imsbr_inet_addr peer_addr;

	__be16	local_port;
	__be16	peer_port;

	__u16	l3proto;	/* IPPROTO_IP[V6] */
	__u8	l4proto;	/* IPPROTO_UDP/TCP */
	__u8	media_type;	/* enum imsbr_media_types */

	__u16	link_type;	/* enum imsbr_link_types */
	__u16	socket_type;	/* enum imsbr_socket_types */

	__u32	sim_card;
};

enum imsbr_attrs {
	IMSBR_A_UNSPEC,
	IMSBR_A_CALL_STATE,
	IMSBR_A_TUPLE,
	IMSBR_A_SIMCARD,
	IMSBR_A_LOCALMAC,
	IMSBR_A_REMOTEADDR,
	IMSBR_A_ISV4,
	IMSBR_A_LOWPOWER_ST,
	IMSBR_A_ESP_SPI,
	__IMSBR_A_MAX
};

#define IMSBR_A_MAX (__IMSBR_A_MAX - 1)

enum imsbr_commands {
	IMSBR_C_UNSPEC,
	IMSBR_C_CALL_STATE,
	IMSBR_C_ADD_TUPLE,
	IMSBR_C_DEL_TUPLE,
	IMSBR_C_RESET_TUPLE,
	IMSBR_C_SEND_LOCALMAC,
	IMSBR_C_SEND_REMOTEMAC,
	IMSBR_C_LOWPOWER_ST,
	IMSBR_C_ADD_SPI,
	IMSBR_C_DEL_SPI,
	__IMSBR_C_MAX
};

#define IMSBR_C_MAX (__IMSBR_C_MAX - 1)

enum imsbr_lowpower_state {
	IMSBR_LOWPOWER_UNSPEC,
	IMSBR_LOWPOWER_START,
	IMSBR_LOWPOWER_END,
	__IMSBR_LOWPOWER_MAX
};

#endif
