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

/*! \file stats.h
    \brief This file includes statistics support.
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

VOID StatsRxPktInfoDisplay(UINT_8 *pPkt);

VOID StatsTxPktInfoDisplay(UINT_8 *pPkt);

/* End of stats.h */
