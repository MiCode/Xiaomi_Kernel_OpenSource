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

/* This file is generated from btampFsm.cdd - do not edit manually*/
/* Generated on: Thu Oct 16 15:40:39 PDT 2008 */


#ifndef __BTAMPFSM_EXT_H__
#define __BTAMPFSM_EXT_H__

/* Events that can be sent to the state-machine */
typedef enum
{
  eWLAN_BAP_TIMER_CONNECT_ACCEPT_TIMEOUT=0U,
  eWLAN_BAP_MAC_CONNECT_COMPLETED
,
  eWLAN_BAP_CHANNEL_SELECTION_FAILED,
  eWLAN_BAP_MAC_CONNECT_FAILED,
  eWLAN_BAP_MAC_CONNECT_INDICATION
,
  eWLAN_BAP_MAC_KEY_SET_SUCCESS,
  eWLAN_BAP_HCI_PHYSICAL_LINK_ACCEPT,
  eWLAN_BAP_RSN_FAILURE,
  eWLAN_BAP_MAC_SCAN_COMPLETE,
  eWLAN_BAP_HCI_PHYSICAL_LINK_CREATE,
  eWLAN_BAP_MAC_READY_FOR_CONNECTIONS
,
  eWLAN_BAP_MAC_START_BSS_SUCCESS
,
  eWLAN_BAP_RSN_SUCCESS,
  eWLAN_BAP_MAC_START_FAILS,
  eWLAN_BAP_HCI_PHYSICAL_LINK_DISCONNECT,
  eWLAN_BAP_MAC_INDICATES_MEDIA_DISCONNECTION,
  eWLAN_BAP_HCI_WRITE_REMOTE_AMP_ASSOC
,
  NO_MSG
}MESSAGE_T;


#endif
