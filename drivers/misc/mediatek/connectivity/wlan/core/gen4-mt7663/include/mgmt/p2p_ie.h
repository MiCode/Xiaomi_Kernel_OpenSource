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
#ifndef _P2P_IE_H
#define _P2P_IE_H

#if CFG_SUPPORT_WFD

#define ELEM_MAX_LEN_WFD 62	/* TODO: Move to appropriate place */

/*---------------- WFD Data Element Definitions ----------------*/
/* WFD 4.1.1 - WFD IE format */
#define WFD_OUI_TYPE_LEN                            4

/* == OFFSET_OF(IE_P2P_T,*/
/*aucP2PAttributes[0]) */
#define WFD_IE_OUI_HDR    (ELEM_HDR_LEN + WFD_OUI_TYPE_LEN)

/* WFD 4.1.1 - General WFD Attribute */
#define WFD_ATTRI_HDR_LEN    3	/* ID(1 octet) + Length(2 octets) */

/* WFD Attribute Code */
#define WFD_ATTRI_ID_DEV_INFO                                 0
#define WFD_ATTRI_ID_ASSOC_BSSID                          1
#define WFD_ATTRI_ID_COUPLED_SINK_INFO                 6
#define WFD_ATTRI_ID_EXT_CAPABILITY                        7
#define WFD_ATTRI_ID_SESSION_INFO                           9
#define WFD_ATTRI_ID_ALTER_MAC_ADDRESS                10

/* Maximum Length of WFD Attributes */
#define WFD_ATTRI_MAX_LEN_DEV_INFO           6	/* 0 */
#define WFD_ATTRI_MAX_LEN_ASSOC_BSSID        6	/* 1 */
#define WFD_ATTRI_MAX_LEN_COUPLED_SINK_INFO 7	/* 6 */
#define WFD_ATTRI_MAX_LEN_EXT_CAPABILITY     2	/* 7 */
#define WFD_ATTRI_MAX_LEN_SESSION_INFO       0	/* 9 */	/* 24 * #Clients */
#define WFD_ATTRI_MAX_LEN_ALTER_MAC_ADDRESS 6	/* 10 */

struct WFD_DEVICE_INFORMATION_IE {
	uint8_t ucElemID;
	uint16_t u2Length;
	uint16_t u2WfdDevInfo;
	uint16_t u2SessionMgmtCtrlPort;
	uint16_t u2WfdDevMaxSpeed;
} __KAL_ATTRIB_PACKED__;

#endif

uint32_t p2pCalculate_IEForAssocReq(IN struct ADAPTER *prAdapter,
		IN uint8_t ucBssIndex, IN struct STA_RECORD *prStaRec);

void p2pGenerate_IEForAssocReq(IN struct ADAPTER *prAdapter,
		IN struct MSDU_INFO *prMsduInfo);

#endif
