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
** Id: //Department/DaVinci/TRUNK/WiFi_P2P_Driver/include/nic/p2p_nic.h#1
*/

/*! \file   "p2p_nic.h"
 *  \brief  The declaration of nic functions
 *
 *  Detail description.
 */

#ifndef _P2P_NIC_H
#define _P2P_NIC_H

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
 *                                 M A C R O S
 ********************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 ********************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 ********************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 ********************************************************************************
 */

/*******************************************************************************
 *                  F U N C T I O N   D E C L A R A T I O N S
 ********************************************************************************
 */

VOID
nicP2pMediaStateChange(IN P_ADAPTER_T prAdapter, IN UINT_8 ucBssIndex, IN P_EVENT_CONNECTION_STATUS prConnectionStatus);

VOID
nicRxAddP2pDevice(IN P_ADAPTER_T prAdapter, IN P_EVENT_P2P_DEV_DISCOVER_RESULT_T prP2pResult, IN PUINT_8 pucRxIEBuf, IN
		  UINT_16 u2RxIELength);

#endif
