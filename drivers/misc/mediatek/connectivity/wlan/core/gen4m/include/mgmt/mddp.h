/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
/*
 ** Id: //Department/DaVinci/BRANCHES/
 *      MT6620_WIFI_DRIVER_V2_3/include/mgmt/mddp.h#1
 */

/*! \file   "mddp.h"
 *    \brief  The declaration of nic functions
 *
 *    Detail description.
 */


#ifndef _MDDP_H
#define _MDDP_H

#if CFG_MTK_MCIF_WIFI_SUPPORT

/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */

/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "mddp_export.h"
#include "mddp.h"
/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
void mddpInit(void);
int32_t mddpMdNotifyInfo(struct mddpw_md_notify_info_t *prMdInfo);
int32_t mddpChangeState(enum mddp_state_e event, void *buf, uint32_t *buf_len);
int32_t mddpGetMdStats(IN struct net_device *prDev);
int32_t mddpSetTxDescTemplate(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec,
	IN uint8_t fgActivate);
void mddpUpdateReorderQueParm(struct ADAPTER *prAdapter,
			      struct RX_BA_ENTRY *prReorderQueParm,
			      struct SW_RFB *prSwRfb);
int32_t mddpNotifyDrvMac(IN struct ADAPTER *prAdapter);
int32_t mddpNotifyDrvTxd(IN struct ADAPTER *prAdapter,
	IN struct STA_RECORD *prStaRec,
	IN uint8_t fgActivate);
int32_t mddpNotifyStaTxd(IN struct ADAPTER *prAdapter);
void mddpNotifyWifiOnStart(void);
int32_t mddpNotifyWifiOnEnd(void);
void mddpNotifyWifiOffStart(void);
void mddpNotifyWifiOffEnd(void);
void setMddpSupportRegister(IN struct ADAPTER *prAdapter);

#endif
#endif /* _MDDP_H */
