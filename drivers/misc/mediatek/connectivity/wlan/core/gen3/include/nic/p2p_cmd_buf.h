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
** Id:
*/

/*! \file   "p2p_cmd_buf.h"
 *  \brief  In this file we define the structure for Command Packet.
 *
 *              In this file we define the structure for Command Packet and the control unit
 *  of MGMT Memory Pool.
 */

#ifndef _P2P_CMD_BUF_H
#define _P2P_CMD_BUF_H

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
 *                                 M A C R O S
 ********************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 ********************************************************************************
 */

/*--------------------------------------------------------------*/
/* Firmware Command Packer                                      */
/*--------------------------------------------------------------*/
WLAN_STATUS
wlanoidSendSetQueryP2PCmd(IN P_ADAPTER_T prAdapter, IN UINT_8 ucCID, IN UINT_8 ucBssIdx,
			  IN BOOLEAN fgSetQuery, IN BOOLEAN fgNeedResp, IN BOOLEAN fgIsOid,
			  IN PFN_CMD_DONE_HANDLER pfCmdDoneHandler, IN PFN_CMD_TIMEOUT_HANDLER pfCmdTimeoutHandler,
			  IN UINT_32 u4SetQueryInfoLen, IN PUINT_8 pucInfoBuffer,
			  OUT PVOID pvSetQueryBuffer, IN UINT_32 u4SetQueryBufferLen);

#endif /* _P2P_CMD_BUF_H */
