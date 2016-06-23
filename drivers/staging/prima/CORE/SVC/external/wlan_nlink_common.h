/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*===========================================================================
  \file wlan_nlink_common.h
  
  Exports and types for the Netlink Service interface. This header file contains
  message types and definitions that is shared between the user space service
  (e.g. BTC service) and WLAN kernel module.

  Copyright (c) 2009 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary

===========================================================================*/

#ifndef WLAN_NLINK_COMMON_H__
#define WLAN_NLINK_COMMON_H__

#include <linux/netlink.h>

/*---------------------------------------------------------------------------
 * External Functions
 *-------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
 * Preprocessor Definitions and Constants
 *-------------------------------------------------------------------------*/
#define WLAN_NL_MAX_PAYLOAD   256     /* maximum size for netlink message*/
#define WLAN_NLINK_PROTO_FAMILY  NETLINK_USERSOCK
#define WLAN_NLINK_MCAST_GRP_ID  0x01 

/*---------------------------------------------------------------------------
 * Type Declarations
 *-------------------------------------------------------------------------*/

/* 
 * The following enum defines the target service within WLAN driver for which the
 * message is intended for. Each service along with its counterpart 
 * in the user space, define a set of messages they recognize.
 * Each of this message will have an header of type tAniMsgHdr defined below.
 * Each Netlink message to/from a kernel module will contain only one
 * message which is preceded by a tAniMsgHdr. The maximun size (in bytes) of
 * a netlink message is assumed to be MAX_PAYLOAD bytes.
 *
 *         +------------+-------+----------+----------+
 *         |Netlink hdr | Align |tAniMsgHdr| msg body |
 *         +------------+-------+----------|----------+
 */

// Message Types 
#define WLAN_BTC_QUERY_STATE_REQ    0x01  // BTC  --> WLAN
#define WLAN_BTC_BT_EVENT_IND       0x02  // BTC  --> WLAN
#define WLAN_BTC_QUERY_STATE_RSP    0x03  // WLAN -->  BTC
#define WLAN_MODULE_UP_IND          0x04  // WLAN -->  BTC
#define WLAN_MODULE_DOWN_IND        0x05  // WLAN -->  BTC
#define WLAN_STA_ASSOC_DONE_IND     0x06  // WLAN -->  BTC
#define WLAN_STA_DISASSOC_DONE_IND  0x07  // WLAN -->  BTC

// Special Message Type used by AMP, intercepted by send_btc_nlink_msg() and
// replaced by WLAN_STA_ASSOC_DONE_IND or WLAN_STA_DISASSOC_DONE_IND
#define WLAN_AMP_ASSOC_DONE_IND     0x10

// Special Message Type used by SoftAP, intercepted by send_btc_nlink_msg() and
// replaced by WLAN_STA_ASSOC_DONE_IND
#define WLAN_BTC_SOFTAP_BSS_START      0x11

#define WLAN_SVC_SAP_RESTART_IND 0x108
// Event data for WLAN_BTC_QUERY_STATE_RSP & WLAN_STA_ASSOC_DONE_IND
typedef struct
{
   unsigned char channel;  // 0 implies STA not associated to AP
} tWlanAssocData;

#define ANI_NL_MSG_BASE     0x10    /* Some arbitrary base */

typedef enum eAniNlModuleTypes {
   ANI_NL_MSG_PUMAC = ANI_NL_MSG_BASE + 0x01,// PTT Socket App
   ANI_NL_MSG_PTT   = ANI_NL_MSG_BASE + 0x07,// Quarky GUI
   WLAN_NL_MSG_BTC,
   ANI_NL_MSG_LOG   = ANI_NL_MSG_BASE + 0x0C,
   WLAN_NL_MSG_SVC,
   ANI_NL_MSG_MAX  
} tAniNlModTypes, tWlanNlModTypes;

#define WLAN_NL_MSG_BASE ANI_NL_MSG_BASE
#define WLAN_NL_MSG_MAX  ANI_NL_MSG_MAX

//All Netlink messages must contain this header
typedef struct sAniHdr {
   unsigned short type;
   unsigned short length;
} tAniHdr, tAniMsgHdr;

#endif //WLAN_NLINK_COMMON_H__
