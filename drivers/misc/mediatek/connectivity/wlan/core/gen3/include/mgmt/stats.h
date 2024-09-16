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
** Id: stats.h#1
*/

/*
 * ! \file stats.h
 *  \brief This file includes statistics support.
 */

/*******************************************************************************
 *						C O M P I L E R	 F L A G S
 ********************************************************************************
 */

/*******************************************************************************
 *						E X T E R N A L	R E F E R E N C E S
 ********************************************************************************
 */

/*******************************************************************************
*						C O N S T A N T S
********************************************************************************
*/

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*						M A C R O   D E C L A R A T I O N S
********************************************************************************
*/

#include <linux/rtc.h>

#if (CFG_SUPPORT_STATISTICS == 1)
#define STATS_RX_PKT_INFO_DISPLAY			StatsRxPktInfoDisplay
#define STATS_TX_PKT_INFO_DISPLAY			StatsTxPktInfoDisplay
#else
#define STATS_RX_PKT_INFO_DISPLAY(__Pkt__)
#define STATS_TX_PKT_INFO_DISPLAY(__Pkt__)
#endif /* CFG_SUPPORT_STATISTICS */

/*******************************************************************************
*						F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*						P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*						P R I V A T E  F U N C T I O N S
********************************************************************************
*/

/*******************************************************************************
*						P U B L I C  F U N C T I O N S
********************************************************************************
*/

#define STATS_TX_TIME_ARRIVE(__Skb__)										\
do {														\
	UINT_64 __SysTime;											\
	__SysTime = StatsEnvTimeGet(); /* us */									\
	GLUE_SET_PKT_XTIME(__Skb__, __SysTime);									\
} while (FALSE)

UINT_64 StatsEnvTimeGet(VOID);

VOID StatsEnvTxTime2Hif(IN P_ADAPTER_T prAdapter, IN P_MSDU_INFO_T prMsduInfo);

VOID StatsEnvRxTime2Host(IN P_ADAPTER_T prAdapter, struct sk_buff *prSkb);

VOID StatsResetTxRx(VOID);

VOID StatsEnvSetPktDelay(IN UINT_8 ucTxOrRx, IN UINT_8 ucIpProto, IN UINT_16 u2UdpPort, UINT_32 u4DelayThreshold);

VOID StatsEnvGetPktDelay(OUT PUINT_8 pucTxRxFlag, OUT PUINT_8 pucTxIpProto, OUT PUINT_16 pu2TxUdpPort,
	OUT PUINT_32 pu4TxDelayThreshold, OUT PUINT_8 pucRxIpProto,
	OUT PUINT_16 pu2RxUdpPort, OUT PUINT_32 pu4RxDelayThreshold);

VOID StatsRxPktInfoDisplay(P_ADAPTER_T prAdapter, P_SW_RFB_T prSwRfb);

VOID StatsTxPktInfoDisplay(UINT_8 *pPkt);

/* End of stats.h */
