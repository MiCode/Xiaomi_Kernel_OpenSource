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
 * Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3
 *     /include/mgmt/wlan_typedef.h#1
 */

/*! \file   wlan_typedef.h
 *    \brief  Declaration of data type and return values of internal protocol
 *    stack.
 *
 *    In this file we declare the data type and return values which will be
 *    exported to all MGMT Protocol Stack.
 */


#ifndef _WLAN_TYPEDEF_H
#define _WLAN_TYPEDEF_H

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */
#define NIC_TX_ENABLE_SECOND_HW_QUEUE            0

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */
/* Type definition for BSS_INFO structure, to describe the
 * attributes used in a common BSS.
 */
struct BSS_INFO;	/* declare BSS_INFO_T */
struct BSS_INFO;	/* declare P2P_DEV_INFO_T */


struct AIS_SPECIFIC_BSS_INFO;	/* declare AIS_SPECIFIC_BSS_INFO_T */
struct P2P_SPECIFIC_BSS_INFO;	/* declare P2P_SPECIFIC_BSS_INFO_T */
struct BOW_SPECIFIC_BSS_INFO;	/* declare BOW_SPECIFIC_BSS_INFO_T */
/* CFG_SUPPORT_WFD */
struct WFD_CFG_SETTINGS;	/* declare WFD_CFG_SETTINGS_T */

/* BSS related structures */
/* Type definition for BSS_DESC structure, to describe parameter
 * sets of a particular BSS
 */
struct BSS_DESC;	/* declare BSS_DESC */

#if CFG_SUPPORT_PASSPOINT
struct HS20_INFO;	/* declare HS20_INFO_T */
#endif /* CFG_SUPPORT_PASSPOINT */

/* Tc Resource index */
enum ENUM_TRAFFIC_CLASS_INDEX {
	/*First HW queue */
	TC0_INDEX = 0,	/* HIF TX: AC0 packets */
	TC1_INDEX,		/* HIF TX: AC1 packets */
	TC2_INDEX,		/* HIF TX: AC2 packets */
	TC3_INDEX,		/* HIF TX: AC3 packets */
	TC4_INDEX,		/* HIF TX: CPU packets */

#if NIC_TX_ENABLE_SECOND_HW_QUEUE
	/* Second HW queue */
	TC5_INDEX,		/* HIF TX: AC10 packets */
	TC6_INDEX,		/* HIF TX: AC11 packets */
	TC7_INDEX,		/* HIF TX: AC12 packets */
	TC8_INDEX,		/* HIF TX: AC13 packets */
#endif

	TC_NUM			/* Maximum number of Traffic Classes. */
};

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
#endif /* _WLAN_TYPEDEF_H */
