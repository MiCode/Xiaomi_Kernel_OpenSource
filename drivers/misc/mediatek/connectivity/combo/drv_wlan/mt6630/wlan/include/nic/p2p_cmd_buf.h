/*
** $Id:
*/

/*! \file   "p2p_cmd_buf.h"
 *  \brief  In this file we define the structure for Command Packet.
 *
 *              In this file we define the structure for Command Packet and the control unit
 *  of MGMT Memory Pool.
 */



/*
** $Log: p2p_cmd_buf.h $
**
** 07 25 2014 eason.tsai
** AOSP
**
** 03 07 2013 yuche.tsai
** [BORA00002398] [MT6630][Volunteer Patch] P2P Driver Re-Design for Multiple BSS support
** Add wlan_p2p.c, but still need to FIX many place.
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
*
* 07 17 2012 yuche.tsai
* NULL
* Compile no error before trial run.
*
* 12 22 2010 cp.wu
* [WCXRP00000283] [MT6620 Wi-Fi][Driver][Wi-Fi Direct] Implementation of interface for supporting Wi-Fi Direct Service
*Discovery
* 1. header file restructure for more clear module isolation
* 2. add function interface definition for implementing Service Discovery callbacks
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
wlanoidSendSetQueryP2PCmd(IN P_ADAPTER_T prAdapter, IN UINT_8 ucCID, IN UINT_8 ucBssIdx, IN BOOLEAN fgSetQuery, IN
			  BOOLEAN fgNeedResp, IN BOOLEAN fgIsOid, IN PFN_CMD_DONE_HANDLER pfCmdDoneHandler, IN
			  PFN_CMD_TIMEOUT_HANDLER
			  pfCmdTimeoutHandler, IN UINT_32 u4SetQueryInfoLen, IN PUINT_8 pucInfoBuffer, OUT PVOID
			  pvSetQueryBuffer, IN UINT_32
			  u4SetQueryBufferLen);




#endif				/* _P2P_CMD_BUF_H */
