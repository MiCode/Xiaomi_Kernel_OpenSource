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
/*! \file   "nic_ext_cmd_event.h"
 *  \brief This file contains the declairation file of the WLAN OID processing
 *	 routines of Windows driver for MediaTek Inc.
 *   802.11 Wireless LAN Adapters.
 */

#ifndef _NIC_EXT_CMD_EVENT_H
#define _NIC_EXT_CMD_EVENT_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#if (CFG_SUPPORT_CONNAC2X == 1)

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "gl_typedef.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define STAREC_COMMON_EXTRAINFO_V2		BIT(0)
#define STAREC_COMMON_EXTRAINFO_NEWSTAREC	BIT(1)

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
struct STAREC_COMMON_T {
	/* Basic STA record (Group0) */
	uint16_t u2Tag;		/* Tag = 0x00 */
	uint16_t u2Length;
	uint32_t u4ConnectionType;
	uint8_t	ucConnectionState;
	uint8_t	ucIsQBSS;
	uint16_t u2AID;
	uint8_t	aucPeerMacAddr[6];
	uint16_t u2ExtraInfo;
} __KAL_ATTRIB_PACKED__;

struct CMD_STAREC_UPDATE_T {
	uint8_t	ucBssIndex;
	uint8_t	ucWlanIdx;
	uint16_t u2TotalElementNum;
	uint8_t	ucAppendCmdTLV;
	uint8_t ucMuarIdx;
	uint8_t	aucReserve[2];
	uint8_t	aucBuffer[];
} __KAL_ATTRIB_PACKED__;

struct STAREC_HANDLE_T {
	uint32_t StaRecTag;
	uint32_t StaRecTagLen;
	int32_t (*StaRecTagHandler)(
		struct ADAPTER *pAd, uint8_t *pMsgBuf, void *args);
};

#if (CFG_SUPPORT_DMASHDL_SYSDVT)
struct EXT_CMD_CR4_DMASHDL_DVT_T {
	uint8_t ucItemNo;
	uint8_t ucSubItemNo;
	uint8_t ucReserve[2];
};
#endif /* CFG_SUPPORT_DMASHDL_SYSDVT */

struct CMD_BSSINFO_UPDATE_T {
	uint8_t	ucBssIndex;
	uint8_t	ucReserve;
	uint16_t	u2TotalElementNum;
	uint32_t u4Reserve;
	uint8_t aucBuffer[];
} __KAL_ATTRIB_PACKED__;


/* TAG ID 0x00: */
struct BSSINFO_CONNECT_OWN_DEV_T {
	/* BSS connect to own dev (Tag0) */
	uint16_t u2Tag;		/* Tag = 0x00 */
	uint16_t u2Length;
	uint8_t ucHwBSSIndex;
	uint8_t ucOwnMacIdx;
	uint8_t aucReserve[2];
	uint32_t ucConnectionType;
	uint32_t u4Reserved;
} __KAL_ATTRIB_PACKED__;

/* TAG ID 0x01: */
struct BSSINFO_BASIC_T {
	/* Basic BSS information (Tag1) */
	uint16_t u2Tag;		/* Tag = 0x01 */
	uint16_t u2Length;
	uint32_t u4NetworkType;
	uint8_t ucActive;
	uint8_t ucReserve0;
	uint16_t u2BcnInterval;
	uint8_t aucBSSID[6];
	uint8_t ucWmmIdx;
	uint8_t ucDtimPeriod;
/* indicate which wlan-idx used for MC/BC transmission */
	uint8_t ucBcMcWlanidx;
	uint8_t ucCipherSuit;
	uint8_t acuReserve[6];
} __KAL_ATTRIB_PACKED__;


struct CMD_DEVINFO_UPDATE_T {
	uint8_t ucOwnMacIdx;
	uint8_t ucReserve;
	uint16_t u2TotalElementNum;
	uint32_t aucReserve;
	uint8_t aucBuffer[];
} __KAL_ATTRIB_PACKED__;

struct CMD_DEVINFO_ACTIVE_T {
	uint16_t u2Tag;		/* Tag = 0x00 */
	uint16_t u2Length;
	uint8_t ucActive;
	uint8_t ucDbdcIdx;
	uint8_t aucOwnMacAddr[6];
	uint8_t aucReserve[4];
} __KAL_ATTRIB_PACKED__;


/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

uint32_t CmdExtStaRecUpdate2WA(
	struct ADAPTER *pAd,
	struct STA_RECORD *pStaRecCfg);

#if (CFG_SUPPORT_DMASHDL_SYSDVT)
uint32_t CmdExtDmaShdlDvt2WA(
	struct ADAPTER *pAd,
	uint8_t ucItemNo,
	uint8_t ucSubItemNo);
#endif /* CFG_SUPPORT_DMASHDL_SYSDVT */

uint32_t CmdExtBssInfoUpdate2WA(
	struct ADAPTER *pAd,
	uint8_t ucBssIndex);

uint32_t CmdExtDevInfoUpdate2WA(
	struct ADAPTER *pAd,
	uint8_t ucBssIndex);

#endif /* CFG_SUPPORT_CONNAC2X == 1 */

#endif /* _NIC_EXT_CMD_EVENT_H */
