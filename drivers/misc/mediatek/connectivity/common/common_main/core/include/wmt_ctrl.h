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
/*! \file
*    \brief  Declaration of library functions
*
*    Any definitions in this file will be shared among GLUE Layer and internal Driver Stack.
*/



#ifndef _WMT_CTRL_H_
#define _WMT_CTRL_H_

#include "osal.h"
#include "stp_exp.h"
/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define DWCNT_CTRL_DATA  (16)


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

typedef struct _WMT_CTRL_DATA_ {
	UINT32 ctrlId;
	SIZE_T au4CtrlData[DWCNT_CTRL_DATA];
} WMT_CTRL_DATA, *P_WMT_CTRL_DATA;

typedef enum _ENUM_WMT_CTRL_T {
	WMT_CTRL_HW_PWR_OFF = 0,	/* whole chip power off */
	WMT_CTRL_HW_PWR_ON = 1,	/* whole chip power on */
	WMT_CTRL_HW_RST = 2,	/* whole chip reset */
	WMT_CTRL_STP_CLOSE = 3,
	WMT_CTRL_STP_OPEN = 4,
	WMT_CTRL_STP_CONF = 5,
	WMT_CTRL_FREE_PATCH = 6,
	WMT_CTRL_GET_PATCH = 7,
	WMT_CTRL_GET_PATCH_NAME = 8,
	WMT_CTRL_HOST_BAUDRATE_SET = 9,
	WMT_CTRL_SDIO_HW = 10,	/* enable/disable SDIO1/2 of combo chip */
	WMT_CTRL_SDIO_FUNC = 11,	/* probe/remove STP/Wi-Fi driver in SDIO1/2 of combo chip */
	WMT_CTRL_HWIDVER_SET = 12,	/* TODO: rename this and add chip id information in addition to chip version */
	WMT_CTRL_HWVER_GET = 13,	/* TODO: [FixMe][GeorgeKuo] remove unused functions */
	WMT_CTRL_STP_RST = 14,
	WMT_CTRL_GET_WMT_CONF = 15,
	WMT_CTRL_TX = 16,	/* [FixMe][GeorgeKuo]: to be removed by Sean's stp integration */
	WMT_CTRL_RX = 17,	/* [FixMe][GeorgeKuo]: to be removed by Sean's stp integration */
	WMT_CTRL_RX_FLUSH = 18,	/* [FixMe][SeanWang]: to be removed by Sean's stp integration */
	WMT_CTRL_GPS_SYNC_SET = 19,
	WMT_CTRL_GPS_LNA_SET = 20,
	WMT_CTRL_PATCH_SEARCH = 21,
	WMT_CTRL_CRYSTAL_TRIMING_GET = 22,
	WMT_CTRL_CRYSTAL_TRIMING_PUT = 23,
	WMT_CTRL_HW_STATE_DUMP = 24,
	WMT_CTRL_GET_PATCH_NUM = 25,
	WMT_CTRL_GET_PATCH_INFO = 26,
	WMT_CTRL_SOC_PALDO_CTRL = 27,
	WMT_CTRL_SOC_WAKEUP_CONSYS = 28,
	WMT_CTRL_SET_STP_DBG_INFO = 29,
	WMT_CTRL_BGW_DESENSE_CTRL = 30,
	WMT_CTRL_TRG_ASSERT = 31,
#if CFG_WMT_LTE_COEX_HANDLING
	WMT_CTRL_GET_TDM_REQ_ANTSEL = 32,
#endif
	WMT_CTRL_EVT_PARSER = 33,
	WMT_CTRL_GET_ROM_PATCH_INFO = 34,
	WMT_CTRL_UPDATE_PATCH_VERSION = 35,
	WMT_CTRL_MAX
} ENUM_WMT_CTRL_T, *P_ENUM_WMT_CTRL_T;

typedef INT32(*WMT_CTRL_FUNC) (P_WMT_CTRL_DATA);

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

extern INT32 wmt_ctrl(P_WMT_CTRL_DATA pWmtCtrlData);

extern INT32
wmt_ctrl_tx_ex(const PUINT8 pData,
	       const UINT32 size, PUINT32 writtenSize, const MTK_WCN_BOOL bRawFlag);


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/



#endif				/* _WMT_CTRL_H_ */
