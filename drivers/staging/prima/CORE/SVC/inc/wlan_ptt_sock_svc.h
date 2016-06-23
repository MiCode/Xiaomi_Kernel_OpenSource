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

/******************************************************************************
 * wlan_ptt_sock_svc.c
 *
 ******************************************************************************/
#ifndef PTT_SOCK_SVC_H
#define PTT_SOCK_SVC_H
#include <wlan_nlink_srv.h>
#include <halTypes.h>
#include <vos_status.h>
#include <wlan_hdd_includes.h>
#include <vos_trace.h>
#include <wlan_nlink_common.h>
/*
 * Quarky Message Format:
 * The following is the messaging protocol between Quarky and PTT Socket App.
 * The totalMsgLen is the length from Radio till msgBody. The value of Radio
 * is always defaulted to 0. The MsgLen is the length from msgId till msgBody.
 * The length of the msgBody varies with respect to the MsgId. Buffer space
 * for MsgBody is already allocated in the received buffer. So in case of READ
 * we just need to populate the values in the received message and send it
 * back
 * +------------+-------+-------+--------+-------+---------+
 * |TotalMsgLen | Radio | MsgId | MsgLen |Status |MsgBody  |
 * +------------+-------+-------|--------+-------+---------+
 * <------4----><--4---><---2--><---2---><---4--><--------->
 */
// PTT Socket App Message Ids
#define PTT_MSG_READ_REGISTER       0x3040
#define PTT_MSG_WRITE_REGISTER      0x3041
#define PTT_MSG_READ_MEMORY         0x3044
#define PTT_MSG_WRITE_MEMORY        0x3045
#define PTT_MSG_LOG_DUMP_DBG        0x32A1
#define PTT_MSG_FTM_CMDS_TYPE           0x4040
#define ANI_DRIVER_MSG_START         0x0001
#define ANI_MSG_APP_REG_REQ         (ANI_DRIVER_MSG_START + 0)
#define ANI_MSG_APP_REG_RSP         (ANI_DRIVER_MSG_START + 1)
#define ANI_MAX_RADIOS      3
#define ANI_NL_MSG_OK       0
#define ANI_NL_MSG_ERROR    -1
#define ANI_NL_MSG_OVERHEAD (NLMSG_SPACE(tAniHdr + 4))
/*
 * Packet Format for READ_REGISTER & WRITE_REGISTER:
 * TotalMsgLen : 4 bytes  [value=20 bytes]
 * Radio       : 4 bytes
 * MsgId       : 2 bytes
 * MsgLen      : 2 bytes
 * Status      : 4 bytes
 * Address     : 4 bytes
 * Payload     : 4 bytes
*/
/*
 * Packet Format for READ_MEMORY & WRITE_MEMORY :
 * TotalMsgLen : 4 bytes [value= 20+LEN_PAYLOAD bytes]
 * Radio       : 4 bytes
 * MsgId       : 2 bytes
 * MsgLen      : 2 bytes
 * Status      : 4 bytes
 * Address     : 4 bytes
 * Length      : 4 bytes [LEN_PAYLOAD]
 * Payload     : LEN_PAYLOAD bytes
*/
int ptt_sock_activate_svc(void *pAdapter);
int ptt_sock_send_msg_to_app(tAniHdr *wmsg, int radio, int src_mod, int pid, int flag);

/*
 * Format of message exchanged between the PTT Socket App in userspace and the
 * WLAN Driver, in either direction. Each msg will begin with this header and
 * will followed by the Quarky message
 */
typedef struct sAniNlMsg {
    struct  nlmsghdr nlh;             // Netlink Header
    int radio;                        // unit number of the radio
    tAniHdr wmsg;                     // Airgo Message Header
} tAniNlHdr;
typedef struct sAniAppRegReq {
    tAniNlModTypes type;              // module id
    int pid;                          // process id
} tAniNlAppRegReq;
typedef struct sAniNlAppRegRsp {
    struct nlmsghdr nlh;              // NetLink Msg Header
    int radio;                        // Radio unit
    tAniHdr wniHdr;                   // Generic WNI msg header
    tAniNlAppRegReq regReq;           // The original request msg
    int ret;                          // Return code
} tAniNlAppRegRsp;
#endif
