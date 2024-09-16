/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the
 * GNU General Public License version 2 as published by the Free Software
 * Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
 * PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.
 * If not, see <http://www.gnu.org/licenses/>.
 */

/*******************************************************************************
 *						C O M P I L E R	 F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *            E X T E R N A L	R E F E R E N C E S
 *******************************************************************************
 */

/*******************************************************************************
 *						C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *            D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *            M A C R O   D E C L A R A T I O N S
 *******************************************************************************
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
 *            F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *						P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *						P R I V A T E  F U N C T I O N S
 *******************************************************************************
 */

/*******************************************************************************
 *						P U B L I C  F U N C T I O N S
 *******************************************************************************
 */

#define STATS_TX_TIME_ARRIVE(__Skb__)	\
do { \
	uint64_t __SysTime; \
	__SysTime = StatsEnvTimeGet(); /* us */	\
	GLUE_SET_PKT_XTIME(__Skb__, __SysTime);	\
} while (FALSE)

uint64_t StatsEnvTimeGet(void);

void StatsEnvTxTime2Hif(IN struct ADAPTER *prAdapter,
			IN struct MSDU_INFO *prMsduInfo);

void StatsEnvRxTime2Host(IN struct ADAPTER *prAdapter,
			 struct sk_buff *prSkb);

void StatsRxPktInfoDisplay(struct SW_RFB *prSwRfb);

void StatsTxPktInfoDisplay(uint8_t *pPkt);

void StatsResetTxRx(void);

void StatsEnvSetPktDelay(IN uint8_t ucTxOrRx,
			 IN uint8_t ucIpProto, IN uint16_t u2UdpPort,
			 uint32_t u4DelayThreshold);

void StatsEnvGetPktDelay(OUT uint8_t *pucTxRxFlag,
			 OUT uint8_t *pucTxIpProto, OUT uint16_t *pu2TxUdpPort,
			 OUT uint32_t *pu4TxDelayThreshold,
			 OUT uint8_t *pucRxIpProto,
			 OUT uint16_t *pu2RxUdpPort,
			 OUT uint32_t *pu4RxDelayThreshold);
/* End of stats.h */
