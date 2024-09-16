/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _QOSMAP_H
#define _QOSMAP_H

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
#define DSCP_SUPPORT 1
/*******************************************************************************
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
VOID handleQosMapConf(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

int qosHandleQosMapConfigure(IN P_ADAPTER_T prAdapter, IN P_SW_RFB_T prSwRfb);

VOID qosMapSetInit(IN P_STA_RECORD_T prStaRec);

VOID qosParseQosMapSet(IN P_ADAPTER_T prAdapter, IN P_STA_RECORD_T prStaRec, IN PUINT_8 qosMapSet);

UINT_8 getUpFromDscp(IN P_GLUE_INFO_T prGlueInfo, IN int type, IN int dscp);

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _QOSMAP_H */
