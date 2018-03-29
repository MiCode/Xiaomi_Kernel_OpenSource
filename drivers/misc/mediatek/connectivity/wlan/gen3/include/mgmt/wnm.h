/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/

/*
** Id: //Department/DaVinci/TRUNK/MT6620_5931_WiFi_Driver/include/mgmt/wnm.h#1
*/

/*! \file  wnm.h
    \brief This file contains the IEEE 802.11 family related 802.11v network management
	   for MediaTek 802.11 Wireless LAN Adapters.
*/

#ifndef _WNM_H
#define _WNM_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/

typedef struct _TIMINGMSMT_PARAM_T {
	BOOLEAN fgInitiator;
	UINT_8 ucTrigger;
	UINT_8 ucDialogToken;	/* Dialog Token */
	UINT_8 ucFollowUpDialogToken;	/* Follow Up Dialog Token */
	UINT_32 u4ToD;		/* Timestamp of Departure [10ns] */
	UINT_32 u4ToA;		/* Timestamp of Arrival [10ns] */
} TIMINGMSMT_PARAM_T, *P_TIMINGMSMT_PARAM_T;

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

VOID wnmWNMAction(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID wnmReportTimingMeas(IN P_ADAPTER_T prAdapter, IN UINT_8 ucStaRecIndex, IN UINT_32 u4ToD, IN UINT_32 u4ToA);

#define WNM_UNIT_TEST 1

#if WNM_UNIT_TEST
VOID wnmTimingMeasUnitTest1(P_ADAPTER_T prAdapter, UINT_8 ucStaRecIndex);
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _WNM_H */
