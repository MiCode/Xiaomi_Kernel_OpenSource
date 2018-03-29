/*
 *Trace log blocks sent over HBUS
 * Broadcom 802.11abg Networking Device Driver
 *
 * Definitions subject to change without notice.
 *
 * Copyright (C) 1999-2014, Broadcom Corporation
 * Copyright (C) 2018 XiaoMi, Inc.
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: event_log.h 241182 2011-02-17 21:50:03Z$
 */

#ifndef	_WL_DIAG_H
#define	_WL_DIAG_H

#define DIAG_MAJOR_VERSION      1	/* 4 bits */
#define DIAG_MINOR_VERSION      0	/* 4 bits */
#define DIAG_MICRO_VERSION      0	/* 4 bits */

#define DIAG_VERSION		\
	((DIAG_MICRO_VERSION&0xF) | (DIAG_MINOR_VERSION&0xF)<<4 | \
	(DIAG_MAJOR_VERSION&0xF)<<8)
					/* bit[11:8] major ver */
					/* bit[7:4] minor ver */
					/* bit[3:0] micro ver */
#define ETHER_ADDR_PACK_LOW(addr)  (((addr)->octet[3])<<24 | ((addr)->octet[2])<<16 | \
	((addr)->octet[1])<<8 | ((addr)->octet[0]))
#define ETHER_ADDR_PACK_HI(addr)   (((addr)->octet[5])<<8 | ((addr)->octet[4]))

#define SSID_PACK(addr)   (((uint8)(addr)[0])<<24 | ((uint8)(addr)[1])<<16 | \
	((uint8)(addr)[2])<<8 | ((uint8)(addr)[3]))

/* event ID for trace purpose only, to avoid the conflict with future new
* WLC_E_ , starting from 0x8000 */
#define TRACE_FW_AUTH_STARTED			0x8000
#define TRACE_FW_ASSOC_STARTED			0x8001
#define TRACE_FW_RE_ASSOC_STARTED		0x8002
#define TRACE_G_SCAN_STARTED			0x8003
#define TRACE_ROAM_SCAN_STARTED			0x8004
#define TRACE_ROAM_SCAN_COMPLETE		0x8005
#define TRACE_FW_EAPOL_FRAME_TRANSMIT_START	0x8006
#define TRACE_FW_EAPOL_FRAME_TRANSMIT_STOP	0x8007
#define TRACE_BLOCK_ACK_NEGOTIATION_COMPLETE	0x8008	/* protocol status */
#define TRACE_BT_COEX_BT_SCO_START		0x8009
#define TRACE_BT_COEX_BT_SCO_STOP		0x800a
#define TRACE_BT_COEX_BT_SCAN_START		0x800b
#define TRACE_BT_COEX_BT_SCAN_STOP		0x800c
#define TRACE_BT_COEX_BT_HID_START		0x800d
#define TRACE_BT_COEX_BT_HID_STOP		0x800e
#define TRACE_ROAM_AUTH_STARTED			0x800f

/* Parameters of wifi logger events are TLVs */
/* Event parameters tags are defined as: */
#define TRACE_TAG_VENDOR_SPECIFIC		0	/* take a byte stream as parameter */
#define TRACE_TAG_BSSID				1	/* takes a 6 bytes MAC address as parameter */
#define TRACE_TAG_ADDR				2	/* takes a 6 bytes MAC address as parameter */
#define TRACE_TAG_SSID				3	/* takes a 32 bytes SSID address as parameter */
#define TRACE_TAG_STATUS			4	/* takes an integer as parameter */
#define TRACE_TAG_CHANNEL_SPEC			5	/* takes one or more wifi_channel_spec as */
						  /* parameter */
#define TRACE_TAG_WAKE_LOCK_EVENT		6	/* takes a wake_lock_event struct as parameter */
#define TRACE_TAG_ADDR1				7	/* takes a 6 bytes MAC address as parameter */
#define TRACE_TAG_ADDR2				8	/* takes a 6 bytes MAC address as parameter */
#define TRACE_TAG_ADDR3				9	/* takes a 6 bytes MAC address as parameter */
#define TRACE_TAG_ADDR4				10	/* takes a 6 bytes MAC address as parameter */
#define TRACE_TAG_TSF				11	/* take a 64 bits TSF value as parameter */
#define TRACE_TAG_IE				12	/* take one or more specific 802.11 IEs */
						   /* parameter, IEs are in turn indicated in */
						   /* TLV format as per 802.11 spec */
#define TRACE_TAG_INTERFACE			13	/* take interface name as parameter */
#define TRACE_TAG_REASON_CODE			14	/* take a reason code as per 802.11 */
						   /* as parameter */
#define TRACE_TAG_RATE_MBPS			15	/* take a wifi rate in 0.5 mbps */

/* for each event id with logging data, define its logging data structure */

typedef union {
	struct {
		uint16 event:16;
		uint16 version:16;
	};
	uint32 t;
} wl_event_log_id_t;

typedef union {
	struct {
		uint16 status:16;
		uint16 paraset:16;
	};
	uint32 t;
} wl_event_log_blk_ack_t;

typedef union {
	struct {
		uint8 mode:8;
		uint8 count:8;
		uint16 ch:16;
	};
	uint32 t;
} wl_event_log_csa_t;

typedef union {
	struct {
		uint8 status:1;
		uint8 eapol_idx:2;
		uint32 notused:13;
		uint16 rate0:16;
	};
	uint32 t;
} wl_event_log_eapol_tx_t;

#ifdef EVENT_LOG_COMPILE
#define WL_EVENT_LOG(tag, event, ...) \
	do {					\
		wl_event_log_id_t entry = {{event, DIAG_VERSION} }; \
		EVENT_LOG(tag, "WL event", entry.t , ## __VA_ARGS__); \
	} while (0)
#else
#define WL_EVENT_LOG(tag, event, ...)
#endif				/* EVENT_LOG_COMPILE */
#endif				/* _WL_DIAG_H */
