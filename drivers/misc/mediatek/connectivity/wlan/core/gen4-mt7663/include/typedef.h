/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 ** Id: include/typedef.h
 */

/*! \file   typedef.h
 *  \brief  Declaration of data type and return values of
 *          internal protocol stack.
 *
 *    In this file we declare the data type and return values
 *    which will be exported to the GLUE Layer.
 */

#ifndef _TYPEDEF_H
#define _TYPEDEF_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/* ieee80211.h of linux has duplicated definitions */
#if defined(WLAN_STATUS_SUCCESS)
#undef WLAN_STATUS_SUCCESS
#endif

#define WLAN_STATUS_SUCCESS                     ((uint32_t) 0x00000000L)
#define WLAN_STATUS_PENDING                     ((uint32_t) 0x00000103L)
#define WLAN_STATUS_NOT_ACCEPTED                ((uint32_t) 0x00010003L)

#define WLAN_STATUS_MEDIA_CONNECT               ((uint32_t) 0x4001000BL)
#define WLAN_STATUS_MEDIA_DISCONNECT            ((uint32_t) 0x4001000CL)
#define WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY	((uint32_t) 0x4001000DL)
#define WLAN_STATUS_MEDIA_SPECIFIC_INDICATION   ((uint32_t) 0x40010012L)

#define WLAN_STATUS_SCAN_COMPLETE               ((uint32_t) 0x60010001L)
#define WLAN_STATUS_MSDU_OK                     ((uint32_t) 0x60010002L)

/* TODO(Kevin): double check if 0x60010001 & 0x60010002 is proprietary */
#define WLAN_STATUS_ROAM_OUT_FIND_BEST          ((uint32_t) 0x60010101L)
#define WLAN_STATUS_ROAM_DISCOVERY              ((uint32_t) 0x60010102L)

#define WLAN_STATUS_FAILURE                     ((uint32_t) 0xC0000001L)
#define WLAN_STATUS_RESOURCES                   ((uint32_t) 0xC000009AL)
#define WLAN_STATUS_NOT_SUPPORTED               ((uint32_t) 0xC00000BBL)

#define WLAN_STATUS_MULTICAST_FULL              ((uint32_t) 0xC0010009L)
#define WLAN_STATUS_INVALID_PACKET              ((uint32_t) 0xC001000FL)
#define WLAN_STATUS_ADAPTER_NOT_READY           ((uint32_t) 0xC0010011L)
#define WLAN_STATUS_NOT_INDICATING              ((uint32_t) 0xC0010013L)
#define WLAN_STATUS_INVALID_LENGTH              ((uint32_t) 0xC0010014L)
#define WLAN_STATUS_INVALID_DATA                ((uint32_t) 0xC0010015L)
#define WLAN_STATUS_BUFFER_TOO_SHORT            ((uint32_t) 0xC0010016L)
#define WLAN_STATUS_BWCS_UPDATE                 ((uint32_t) 0xC0010017L)

#define WLAN_STATUS_JOIN_FAILURE                ((uint32_t) 0xc0010018L)

/* NIC status flags */
#define ADAPTER_FLAG_HW_ERR                     0x00400000

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/* Type definition for GLUE_INFO structure */
struct GLUE_INFO;	/* declare GLUE_INFO_T */

/* Type definition for WLAN STATUS */

/* Type definition for ADAPTER structure */
struct ADAPTER;	/* declare ADAPTER_T */

/* Type definition for MESSAGE HEADER structure */
struct MSG_HDR;	/* declare MSG_HDR_T */

/* Type definition for Pointer to OS Native Packet */

/* Type definition for WLAN configuration */
struct WLAN_CFG;	/* declare WLAN_CFG_T */

struct WLAN_CFG_REC;	/* declare WLAN_CFG_REC_T */


/* Type definition for WLAN configuration entry */
struct WLAN_CFG_ENTRY;	/* declare WLAN_CFG_ENTRY_T */

/* Type definition for WLAN configuration callback */
typedef uint32_t(*WLAN_CFG_SET_CB) (struct ADAPTER
				    *prAdapter,
				    uint8_t *pucKey,
					uint8_t *pucValue,
					void *pPrivate,
				    uint32_t u4Flags);

/* Type definition for STA_RECORD_T structure to handle the connectivity
 * and packet reception for a particular STA.
 */
struct STA_RECORD;	/* declare STA_RECORD_T */

/* CMD_INFO_T is used by Glue Layer to send a cluster of Command(OID)
 * information to the TX Path to reduce the parameters of a function call.
 */
struct CMD_INFO;	/* declare CMD_INFO_T */

/* Following typedef should be removed later, because Glue Layer
 * should not be aware of following data type.
 */
struct SW_RFB;	/* declare SW_RFB_T */

struct MSDU_INFO;	/* declare MSDU_INFO_T */

struct REG_ENTRY;	/* declare REG_ENTRY_T */

/* IST handler definition */
typedef void(*IST_EVENT_FUNCTION) (struct ADAPTER *);

/* Type definition for function pointer of timer handler */
typedef void(*PFN_TIMER_CALLBACK) (IN struct GLUE_INFO *);

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
#endif /* _TYPEDEF_H */
