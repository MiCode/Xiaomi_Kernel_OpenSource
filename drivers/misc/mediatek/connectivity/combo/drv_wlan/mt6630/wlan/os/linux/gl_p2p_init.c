/*
** $Id: @(#) gl_p2p_init.c@@
*/

/*! \file   gl_p2p_init.c
    \brief  init and exit routines of Linux driver interface for Wi-Fi Direct

    This file contains the main routines of Linux driver for MediaTek Inc. 802.11
    Wireless LAN Adapters.
*/



/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "precomp.h"


/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define P2P_MODE_INF_NAME "p2p%d";
#define AP_MODE_INF_NAME "ap%d";
/* #define MAX_INF_NAME_LEN 15 */
/* #define MIN_INF_NAME_LEN 1 */

#define RUNNING_P2P_MODE 0
#define RUNNING_AP_MODE 1

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

/*  Get interface name and running mode from module insertion parameter
*       Usage: insmod p2p.ko mode=1
*       default: interface name is p2p%d
*                   running mode is P2P
*/
static PUCHAR ifname = P2P_MODE_INF_NAME;
static UINT_16 mode = RUNNING_P2P_MODE;

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/


/*----------------------------------------------------------------------------*/
/*!
* \brief    check interface name parameter is valid or not
*             if invalid, set ifname to P2P_MODE_INF_NAME
*
*
* \retval
*/
/*----------------------------------------------------------------------------*/
VOID p2pCheckInterfaceName(VOID)
{

	if (mode) {
		mode = RUNNING_AP_MODE;
		ifname = AP_MODE_INF_NAME;
	}
#if 0
	UINT_32 ifLen = 0;

	if (ifname) {
		ifLen = strlen(ifname);

		if (ifLen > MAX_INF_NAME_LEN) {
			ifname[MAX_INF_NAME_LEN] = '\0';
		} else if (ifLen < MIN_INF_NAME_LEN) {
			ifname = P2P_MODE_INF_NAME;
		}
	} else {
		ifname = P2P_MODE_INF_NAME;
	}
#endif
}

VOID p2pSetSuspendMode(P_GLUE_INFO_T prGlueInfo, BOOLEAN fgEnable)
{
	struct net_device *prDev = NULL;

	if (!prGlueInfo) {
		return;
	}

	if (!prGlueInfo->prAdapter->fgIsP2PRegistered) {
		DBGLOG(INIT, INFO, ("%s: P2P is not enabled, SKIP!\n", __func__));
		return;
	}

	prDev = prGlueInfo->prP2PInfo->prDevHandler;
	if (!prDev) {
		DBGLOG(INIT, INFO, ("%s: P2P dev is not availiable, SKIP!\n", __func__));
		return;
	}

	kalSetNetAddressFromInterface(prGlueInfo, prDev, fgEnable);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief
*       run p2p init procedure, include register pointer to wlan
*                                                     glue register p2p
*                                                     set p2p registered flag
* \retval 1     Success
*/
/*----------------------------------------------------------------------------*/
BOOLEAN p2pLaunch(P_GLUE_INFO_T prGlueInfo)
{
	printk("p2p Launch\n");

	if (prGlueInfo->prAdapter->fgIsP2PRegistered == TRUE) {
		printk("p2p already registered\n");
		return FALSE;
	} else if (glRegisterP2P(prGlueInfo, ifname, (BOOLEAN) mode)) {
		prGlueInfo->prAdapter->fgIsP2PRegistered = TRUE;
		printk("Launch success, fgIsP2PRegistered TRUE.\n");

		return TRUE;
	} else {
		printk("Launch Fail\n");
	}

	return FALSE;
}


VOID p2pSetMode(IN BOOLEAN fgIsAPMOde)
{
	if (fgIsAPMOde) {
		mode = RUNNING_AP_MODE;
		ifname = AP_MODE_INF_NAME;
	} else {
		mode = RUNNING_P2P_MODE;
		ifname = P2P_MODE_INF_NAME;
	}

	return;
}				/* p2pSetMode */


/*----------------------------------------------------------------------------*/
/*!
* \brief
*       run p2p exit procedure, include unregister pointer to wlan
*                                                     glue unregister p2p
*                                                     set p2p registered flag

* \retval 1     Success
*/
/*----------------------------------------------------------------------------*/
BOOLEAN p2pRemove(P_GLUE_INFO_T prGlueInfo)
{
	printk("p2p Remove\n");

	if (prGlueInfo->prAdapter->fgIsP2PRegistered == FALSE) {
		printk("p2p is not Registered.\n");
		return FALSE;
	} else {
		prGlueInfo->prAdapter->fgIsP2PRegistered = FALSE;
		glUnregisterP2P(prGlueInfo);
		/*p2p is removed successfully */
		return TRUE;
	}

	return FALSE;
}

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief Driver entry point when the driver is configured as a Linux Module, and
*        is called once at module load time, by the user-level modutils
*        application: insmod or modprobe.
*
* \retval 0     Success
*/
/*----------------------------------------------------------------------------*/
static int initP2P(void)
{
	P_GLUE_INFO_T prGlueInfo;

	/*check interface name validation */
	p2pCheckInterfaceName();

	printk(KERN_INFO DRV_NAME "InitP2P, Ifname: %s, Mode: %s\n", ifname, mode ? "AP" : "P2P");

	/*register p2p init & exit function to wlan sub module handler */
	wlanSubModRegisterInitExit(p2pLaunch, p2pRemove, P2P_MODULE);

	/*if wlan is not start yet, do nothing
	 * p2pLaunch will be called by txthread while wlan start
	 */
	/*if wlan is not started yet, return FALSE */
	if (wlanExportGlueInfo(&prGlueInfo)) {
		wlanSubModInit(prGlueInfo);
		return prGlueInfo->prAdapter->fgIsP2PRegistered ? 0 : -EIO;
	}

	return 0;
}				/* end of initP2P() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Driver exit point when the driver as a Linux Module is removed. Called
*        at module unload time, by the user level modutils application: rmmod.
*        This is our last chance to clean up after ourselves.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
/* 1 Module Leave Point */
static VOID __exit exitP2P(void)
{
	P_GLUE_INFO_T prGlueInfo;

	printk(KERN_INFO DRV_NAME "ExitP2P\n");

	/*if wlan is not started yet, return FALSE */
	if (wlanExportGlueInfo(&prGlueInfo)) {
		wlanSubModExit(prGlueInfo);
	}
	/*UNregister p2p init & exit function to wlan sub module handler */
	wlanSubModRegisterInitExit(NULL, NULL, P2P_MODULE);
}				/* end of exitP2P() */
#endif
