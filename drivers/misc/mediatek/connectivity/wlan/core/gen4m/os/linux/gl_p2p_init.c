/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 ** Id: @(#) gl_p2p_init.c@@
 */

/*! \file   gl_p2p_init.c
 *    \brief  init and exit routines of Linux driver interface for Wi-Fi Direct
 *
 *    This file contains the main routines
 *    of Linux driver for MediaTek Inc. 802.11
 *    Wireless LAN Adapters.
 */

/******************************************************************************
 *                         C O M P I L E R   F L A G S
 ******************************************************************************
 */

/******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 ******************************************************************************
 */

#include "precomp.h"

/******************************************************************************
 *                              C O N S T A N T S
 ******************************************************************************
 */

#define P2P_INF_NAME "p2p%d"
#define AP_INF_NAME  "ap%d"

/******************************************************************************
 *                             D A T A   T Y P E S
 ******************************************************************************
 */

/******************************************************************************
 *                            P U B L I C   D A T A
 ******************************************************************************
 */

/******************************************************************************
 *                           P R I V A T E   D A T A
 ******************************************************************************
 */
static uint8_t *ifname = P2P_INF_NAME;
static uint8_t *ifname2 = P2P_INF_NAME;
static uint16_t mode = RUNNING_P2P_MODE;


/******************************************************************************
 *                                 M A C R O S
 ******************************************************************************
 */

/******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 ******************************************************************************
 */

/******************************************************************************
 *                              F U N C T I O N S
 ******************************************************************************
 */

void p2pSetSuspendMode(struct GLUE_INFO *prGlueInfo, u_int8_t fgEnable)
{
	struct net_device *prDev = NULL;
	struct GL_P2P_INFO *prP2PInfo = NULL;

	if (!prGlueInfo)
		return;

	if (!prGlueInfo->prAdapter->fgIsP2PRegistered) {
		DBGLOG(INIT, INFO, "%s: P2P is not enabled, SKIP!\n", __func__);
		return;
	}

	/* For P2P interfaces, prDevHandler points to the net_device of
	 * p2p0 interface. And aprRoleHandler points to the net_device
	 * of p2p virtual interface (i.e., p2p1) when it was created.
	 * And when p2p virtual interface is deleted, aprRoleHandler
	 * will change to point to prDevHandler. Hence, when
	 * aprRoleHandler & prDevHandler are pointing to different
	 * addresses, it means vif p2p1 exists. Otherwise it means p2p1
	 * was already deleted.
	 */
	prP2PInfo = prGlueInfo->prP2PInfo[0];
	if ((prP2PInfo->aprRoleHandler != NULL) &&
		(prP2PInfo->aprRoleHandler != prP2PInfo->prDevHandler)) {
		prDev = prP2PInfo->aprRoleHandler;
	} else {
		prDev = prP2PInfo->prDevHandler;
	}

	if (!prDev) {
		DBGLOG(INIT, INFO,
			"%s: P2P dev is not available, SKIP!\n", __func__);
		return;
	}

	kalSetNetAddressFromInterface(prGlueInfo, prDev, fgEnable);
	wlanNotifyFwSuspend(prGlueInfo, prDev, fgEnable);
}

/*---------------------------------------------------------------------------*/
/*!
 * \brief
 *       run p2p init procedure, glue register p2p and set p2p registered flag
 *
 * \retval 1     Success
 */
/*---------------------------------------------------------------------------*/
u_int8_t p2pLaunch(struct GLUE_INFO *prGlueInfo)
{
	if (prGlueInfo->prAdapter->fgIsP2PRegistered == TRUE) {
		DBGLOG(P2P, INFO, "p2p is already registered\n");
		return FALSE;
	}

	if (!glRegisterP2P(prGlueInfo, ifname, ifname2, mode)) {
		DBGLOG(P2P, ERROR, "Launch failed\n");
		return FALSE;
	}

	prGlueInfo->prAdapter->fgIsP2PRegistered = TRUE;
	prGlueInfo->prAdapter->p2p_scan_report_all_bss =
		CFG_P2P_SCAN_REPORT_ALL_BSS;
	DBGLOG(P2P, TRACE, "Launch success, fgIsP2PRegistered TRUE\n");
	return TRUE;
}

void p2pSetMode(IN uint8_t ucAPMode)
{
	uint8_t *prAPInfName = AP_INF_NAME;
	uint8_t *prP2PInfName = P2P_INF_NAME;

#ifdef CFG_DRIVER_INF_NAME_CHANGE

	if (kalStrLen(gprifnamep2p) > 0) {
		prP2PInfName = kalStrCat(gprifnamep2p, "%d");
		DBGLOG(INIT, WARN,
			"P2P ifname customized, use %s\n", prP2PInfName);
	}

	if (kalStrLen(gprifnameap) > 0) {
		prAPInfName = kalStrCat(gprifnameap, "%d");
		DBGLOG(INIT, WARN,
			"AP ifname customized, use %s\n", prAPInfName);
	}

#endif /* CFG_DRIVER_INF_NAME_CHANGE */

	switch (ucAPMode) {
	case 0:
		mode = RUNNING_P2P_MODE;
		ifname = prP2PInfName;
		break;
	case 1:
		mode = RUNNING_AP_MODE;
		ifname = prAPInfName;
		break;
	case 2:
		mode = RUNNING_DUAL_AP_MODE;
		ifname = prAPInfName;
		break;
	case 3:
		mode = RUNNING_P2P_AP_MODE;
		ifname = prP2PInfName;
		ifname2 = prAPInfName;
		break;
	}
}				/* p2pSetMode */

/*---------------------------------------------------------------------------*/
/*!
 * \brief
 *       run p2p exit procedure, glue unregister p2p and set p2p registered flag
 *
 * \retval 1     Success
 */
/*---------------------------------------------------------------------------*/
u_int8_t p2pRemove(struct GLUE_INFO *prGlueInfo)
{
	u_int8_t idx = 0;

	if (prGlueInfo->prAdapter->fgIsP2PRegistered == FALSE) {
		DBGLOG(P2P, INFO, "p2p is not registered\n");
		return FALSE;
	}

	prGlueInfo->prAdapter->fgIsP2PRegistered = FALSE;
	prGlueInfo->prAdapter->p2p_scan_report_all_bss = FALSE;

	glUnregisterP2P(prGlueInfo, 0xff);

	/* Release ap0 wdev.
	 * ap0 wdev is created in wlanProbe. So we need to release it in
	 * wlanRemove. Other wdevs shall be released in exitWlan.
	 */
	for (idx = 0 ; idx < KAL_P2P_NUM; idx++) {
		if (gprP2pRoleWdev[idx] == NULL)
			continue;
#if CFG_ENABLE_UNIFY_WIPHY
		if (wlanIsAisDev(gprP2pRoleWdev[idx]->netdev)) {
			/* This is AIS/AP Interface */
			gprP2pRoleWdev[idx] = NULL;
			continue;
		}
#endif
		/* free gprP2pWdev in wlanDestroyAllWdev */
		if (gprP2pRoleWdev[idx] == gprP2pWdev)
			continue;

		DBGLOG(INIT, INFO, "Unregister gprP2pRoleWdev[%d]\n", idx);
#if (CFG_ENABLE_UNIFY_WIPHY == 0)
		set_wiphy_dev(gprP2pRoleWdev[idx]->wiphy, NULL);
		wiphy_unregister(gprP2pRoleWdev[idx]->wiphy);
		wiphy_free(gprP2pRoleWdev[idx]->wiphy);
#endif
		kfree(gprP2pRoleWdev[idx]);
		gprP2pRoleWdev[idx] = NULL;
		break;
	}
#if (CFG_ENABLE_UNIFY_WIPHY == 0)
	/* gprP2pWdev: base P2P dev
	 * Becase the interface dev (ex: usb_device) would be free
	 * after un-plug event. Should set the wiphy->dev->parent which
	 * pointer to the interface dev to NULL. Otherwise, the corresponding
	 * system operation (poweroff, suspend) might reference it.
	 * set_wiphy_dev(wiphy, NULL): set the wiphy->dev->parent = NULL
	 */
	if (gprP2pWdev != NULL)
		set_wiphy_dev(gprP2pWdev->wiphy, NULL);
#endif

	return TRUE;
}

