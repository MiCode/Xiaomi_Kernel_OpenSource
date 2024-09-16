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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_umac.c#5
*/

/*! \file   nic_umac.c
*    \brief  Functions that used for debug UMAC
*
*    This file includes the functions used do umac debug
*
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "precomp.h"
#include "que_mgt.h"

#ifndef LINUX
#include <limits.h>
#else
#include <linux/limits.h>
#endif

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/



/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

typedef struct _UMAC_PG_INFO_AND_RESERVE_CNT_CR_OFFSET_MAP_T {
	UINT_8 ucGroupID;
	UINT_32 u4PgReservePageCntRegOffset;
	UINT_32 u4PgInfoRegOffset;
} UMAC_PG_INFO_AND_RESERVE_CNT_CR_OFFSET_MAP_T, *P_UMAC_PG_INFO_AND_RESERVE_CNT_CR_OFFSET_MAP_T;

typedef struct _UMAC_PG_MAX_MIN_QUOTA_SET_T {
	UINT_8 ucPageGroupID;
	UINT_16 u2MaxPageQuota;
	UINT_16 u2MinPageQuota;
} UMAC_PG_MAX_MIN_QUOTA_SET_T, *P_UMAC_PG_MAX_MIN_QUOTA_SET_T;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

const UMAC_PG_INFO_AND_RESERVE_CNT_CR_OFFSET_MAP_T  g_arPlePgInfoAndReserveCrOffsetMap[] = {
	{ UMAC_PG_HIF0_GROUP_0,
		UMAC_PG_HIF0_GROUP(UMAC_PLE_CFG_POOL_INDEX),
		UMAC_HIF0_PG_INFO(UMAC_PLE_CFG_POOL_INDEX) },
	{ UMAC_PG_HIF0_GROUP_0,
		UMAC_PG_HIF0_GROUP(UMAC_PLE_CFG_POOL_INDEX),
		UMAC_HIF0_PG_INFO(UMAC_PLE_CFG_POOL_INDEX) },
	{ UMAC_PG_CPU_GROUP_2,
		UMAC_PG_CPU_GROUP(UMAC_PLE_CFG_POOL_INDEX),
		UMAC_CPU_PG_INFO(UMAC_PLE_CFG_POOL_INDEX) },
};

const UMAC_PG_INFO_AND_RESERVE_CNT_CR_OFFSET_MAP_T  g_arPsePgInfoAndReserveCrOffsetMap[] = {
	{ UMAC_PG_HIF0_GROUP_0,
		UMAC_PG_HIF0_GROUP(UMAC_PSE_CFG_POOL_INDEX),
		UMAC_HIF0_PG_INFO(UMAC_PSE_CFG_POOL_INDEX) },
	{ UMAC_PG_HIF1_GROUP_1,
		UMAC_PG_HIF1_GROUP(UMAC_PSE_CFG_POOL_INDEX),
		UMAC_HIF1_PG_INFO(UMAC_PSE_CFG_POOL_INDEX) },
	{ UMAC_PG_CPU_GROUP_2,
		UMAC_PG_CPU_GROUP(UMAC_PSE_CFG_POOL_INDEX),
		UMAC_CPU_PG_INFO(UMAC_PSE_CFG_POOL_INDEX) },
	{ UMAC_PG_LMAC0_GROUP_3,
		UMAC_PG_LMAC0_GROUP(UMAC_PSE_CFG_POOL_INDEX),
		UMAC_LMAC0_PG_INFO(UMAC_PSE_CFG_POOL_INDEX) },
	{ UMAC_PG_LMAC1_GROUP_4,
		UMAC_PG_LMAC1_GROUP(UMAC_PSE_CFG_POOL_INDEX),
		UMAC_LMAC1_PG_INFO(UMAC_PSE_CFG_POOL_INDEX) },
	{ UMAC_PG_LMAC2_GROUP_5,
		UMAC_PG_LMAC2_GROUP(UMAC_PSE_CFG_POOL_INDEX),
		UMAC_LMAC2_PG_INFO(UMAC_PSE_CFG_POOL_INDEX) },
	{ UMAC_PG_PLE_GROUP_6,
		UMAC_PG_PLE_GROUP(UMAC_PSE_CFG_POOL_INDEX),
		UMAC_PLE_PG_INFO(UMAC_PSE_CFG_POOL_INDEX) },
};

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* @brief halUmacWrapSourcePortSanityCheck:
*
* @param IN BOOLEAN                      fgPsePleFlag,
*	 IN UINT_8                       ucPageGroupID
* @return TRUE/FALSE
*/
/*----------------------------------------------------------------------------*/

OUT BOOLEAN halUmacWrapSourcePortSanityCheck(IN BOOLEAN fgPsePleFlag, IN UINT_8 ucPageGroupID)
{

	if (fgPsePleFlag == UMAC_PSE_CFG_POOL_INDEX) {
		if (ucPageGroupID > UMAC_PG_PLE_GROUP_6)
			return FALSE;
	} else if (fgPsePleFlag == UMAC_PLE_CFG_POOL_INDEX) {
		if ((ucPageGroupID != UMAC_PG_HIF0_GROUP_0) && (ucPageGroupID != UMAC_PG_CPU_GROUP_2))
			return FALSE;
	} else
		return FALSE;

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* @brief halUmacWrapRsvPgCnt:
*
* @param IN P_ADAPTER_T prAdapter
*	IN BOOLEAN	fgPsePleFlag,
*	IN UINT_8	ucPageGroupID
* @return UINT_16
*/
/*----------------------------------------------------------------------------*/

OUT UINT_16 halUmacWrapRsvPgCnt(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgPsePleFlag, IN UINT_8 ucPageGroupID)
{
	UINT_32 u4RegAddr = 0;
	UINT_32 u4Value = 0;

	if (halUmacWrapSourcePortSanityCheck(fgPsePleFlag, ucPageGroupID) == FALSE)
		return UMAC_FID_FAULT;

	if (fgPsePleFlag == UMAC_PSE_CFG_POOL_INDEX)
		u4RegAddr = g_arPsePgInfoAndReserveCrOffsetMap[ucPageGroupID].u4PgInfoRegOffset;
	else if (fgPsePleFlag == UMAC_PLE_CFG_POOL_INDEX)
		u4RegAddr = g_arPlePgInfoAndReserveCrOffsetMap[ucPageGroupID].u4PgInfoRegOffset;

	HAL_MCR_RD(prAdapter, u4RegAddr, &u4Value);

	return (UINT_16) (u4Value & BITS(0, 11));
}


/*----------------------------------------------------------------------------*/
/*!
* @brief halUmacWrapSrcPgCnt:
*
* @param IN P_ADAPTER_T prAdapter
*	IN BOOLEAN	fgPsePleFlag,
*	IN UINT_8	ucPageGroupID
* @return UINT_16
*/
/*----------------------------------------------------------------------------*/

OUT UINT_16 halUmacWrapSrcPgCnt(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgPsePleFlag, IN UINT_8 ucPageGroupID)
{
	UINT_32 u4RegAddr = 0;
	UINT_32 u4Value = 0;

	if (halUmacWrapSourcePortSanityCheck(fgPsePleFlag, ucPageGroupID) == FALSE)
		return UMAC_FID_FAULT;

	if (fgPsePleFlag == UMAC_PSE_CFG_POOL_INDEX)
		u4RegAddr = g_arPsePgInfoAndReserveCrOffsetMap[ucPageGroupID].u4PgInfoRegOffset;
	else if (fgPsePleFlag == UMAC_PLE_CFG_POOL_INDEX)
		u4RegAddr = g_arPlePgInfoAndReserveCrOffsetMap[ucPageGroupID].u4PgInfoRegOffset;

	HAL_MCR_RD(prAdapter, u4RegAddr, &u4Value);

	return (UINT_16) ((u4Value & BITS(16, 27)) >> 16);
}


/*----------------------------------------------------------------------------*/
/*!
* @brief halUmacPbufCtrlTotalPageNum:
*
* @param IN P_ADAPTER_T prAdapter
*	IN BOOLEAN	fgPsePleFlag,
* @return UINT_16
*/
/*----------------------------------------------------------------------------*/

OUT UINT_16 halUmacPbufCtrlTotalPageNum(IN P_ADAPTER_T prAdapter, IN UINT_16 fgPsePleFlag)
{
	UINT_32 u4Value = 0;

	HAL_MCR_RD(prAdapter, UMAC_PBUF_CTRL(fgPsePleFlag), &u4Value);

	return (UINT_16) (u4Value & UMAC_PBUF_CTRL_TOTAL_PAGE_NUM_MASK);
}


/*----------------------------------------------------------------------------*/
/*!
* @brief halUmacWrapFrePageCnt:
*
* @param IN P_ADAPTER_T prAdapter
*	IN BOOLEAN	fgPsePleFlag,
* @return UINT_16
*/
/*----------------------------------------------------------------------------*/

OUT UINT_16 halUmacWrapFrePageCnt(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgPsePleFlag)
{
	UINT_32 u4Value = 0;

	HAL_MCR_RD(prAdapter, UMAC_FREEPG_CNT(fgPsePleFlag), &u4Value);
	return (u4Value & UMAC_FREEPG_CNT_FREEPAGE_CNT_MASK) >> UMAC_FREEPG_CNT_FREEPAGE_CNT_OFFSET;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief halUmacWrapFfaCnt:
*
* @param IN P_ADAPTER_T prAdapter
*	IN BOOLEAN fgPsePleFlag,
* @return UINT_16
*/
/*----------------------------------------------------------------------------*/

OUT UINT_16 halUmacWrapFfaCnt(IN P_ADAPTER_T prAdapter, IN BOOLEAN fgPsePleFlag)
{
	UINT_32 u4Value = 0;

	HAL_MCR_RD(prAdapter, UMAC_FREEPG_CNT(fgPsePleFlag), &u4Value);
	return (u4Value & UMAC_FREEPG_CNT_FFA_CNT_MASK) >> UMAC_FREEPG_CNT_FFA_CNT_OFFSET;
}


/*----------------------------------------------------------------------------*/
/*!
* @brief halUmacInfoGetMiscStatus:
*
* @param IN P_ADAPTER_T prAdapter
*	IN P_UMAC_STAT2_GET_T pUmacStat2Get,
* @return UINT_16
*/
/*----------------------------------------------------------------------------*/


OUT BOOLEAN halUmacInfoGetMiscStatus(IN P_ADAPTER_T prAdapter, IN P_UMAC_STAT2_GET_T pUmacStat2Get)
{
	pUmacStat2Get->u2PleRevPgHif0Group0 =
		halUmacWrapRsvPgCnt(prAdapter, UMAC_PLE_CFG_POOL_INDEX, UMAC_PG_HIF0_GROUP_0);

	pUmacStat2Get->u2PleRevPgCpuGroup2 =
		halUmacWrapRsvPgCnt(prAdapter, UMAC_PLE_CFG_POOL_INDEX, UMAC_PG_CPU_GROUP_2);

	pUmacStat2Get->u2PseRevPgHif0Group0 =
		halUmacWrapRsvPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_HIF0_GROUP_0);

	pUmacStat2Get->u2PseRevPgHif1Group1 =
		halUmacWrapRsvPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_HIF1_GROUP_1);

	pUmacStat2Get->u2PseRevPgCpuGroup2 =
		halUmacWrapRsvPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_CPU_GROUP_2);

	pUmacStat2Get->u2PseRevPgLmac0Group3 =
		halUmacWrapRsvPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_LMAC0_GROUP_3);

	pUmacStat2Get->u2PseRevPgLmac1Group4 =
		halUmacWrapRsvPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_LMAC1_GROUP_4);

	pUmacStat2Get->u2PseRevPgLmac2Group5 =
		halUmacWrapRsvPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_LMAC2_GROUP_5);

	pUmacStat2Get->u2PseRevPgPleGroup6 =
		halUmacWrapRsvPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_PLE_GROUP_6);

	pUmacStat2Get->u2PleSrvPgHif0Group0 =
		halUmacWrapSrcPgCnt(prAdapter, UMAC_PLE_CFG_POOL_INDEX, UMAC_PG_HIF0_GROUP_0);

	pUmacStat2Get->u2PleSrvPgCpuGroup2 =
		halUmacWrapSrcPgCnt(prAdapter, UMAC_PLE_CFG_POOL_INDEX, UMAC_PG_CPU_GROUP_2);

	pUmacStat2Get->u2PseSrvPgHif0Group0 =
		halUmacWrapSrcPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_HIF0_GROUP_0);

	pUmacStat2Get->u2PseSrvPgHif1Group1 =
		halUmacWrapSrcPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_HIF1_GROUP_1);

	pUmacStat2Get->u2PseSrvPgCpuGroup2 =
		halUmacWrapSrcPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_CPU_GROUP_2);

	pUmacStat2Get->u2PseSrvPgLmac0Group3 =
		halUmacWrapSrcPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_LMAC0_GROUP_3);

	pUmacStat2Get->u2PseSrvPgLmac1Group4 =
		halUmacWrapSrcPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_LMAC1_GROUP_4);

	pUmacStat2Get->u2PseSrvPgLmac2Group5 =
		halUmacWrapSrcPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_LMAC2_GROUP_5);

	pUmacStat2Get->u2PseSrvPgPleGroup6 =
		halUmacWrapSrcPgCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX, UMAC_PG_PLE_GROUP_6);


	pUmacStat2Get->u2PleTotalPageNum = halUmacPbufCtrlTotalPageNum(prAdapter, UMAC_PLE_CFG_POOL_INDEX);

	pUmacStat2Get->u2PseTotalPageNum = halUmacPbufCtrlTotalPageNum(prAdapter, UMAC_PSE_CFG_POOL_INDEX);

	pUmacStat2Get->u2PleFreePageNum = halUmacWrapFrePageCnt(prAdapter, UMAC_PLE_CFG_POOL_INDEX);

	pUmacStat2Get->u2PseFreePageNum = halUmacWrapFrePageCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX);

	pUmacStat2Get->u2PleFfaNum = halUmacWrapFfaCnt(prAdapter, UMAC_PLE_CFG_POOL_INDEX);

	pUmacStat2Get->u2PseFfaNum = halUmacWrapFfaCnt(prAdapter, UMAC_PSE_CFG_POOL_INDEX);

	return TRUE;
}
