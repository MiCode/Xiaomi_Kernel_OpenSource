/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/nic/nic_cmd_event.c#3 $
*/

/*! \file   nic_cmd_event.c
    \brief  Callback functions for Command packets.

	Various Event packet handlers which will be setup in the callback function of
    a command packet.
*/



/*
** $Log: nic_cmd_event.c $
**
** 06 12 2014 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch]
**	update BLBIST dump burst mode
**
**	Review: http://mtksap20:8080/go?page=NewReview&reviewid=110351
**
** 04 08 2014 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch]
** add for BLBIST dump index
**
** 01 15 2014 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch][MT6630][Driver]MT6630 Wi-Fi Patch
** Merging
**
**	//ALPS_SW/DEV/ALPS.JB2.MT6630.DEV/alps/mediatek/kernel/drivers/combo/drv_wlan/mt6630/wlan/...
**
**	to //ALPS_SW/TRUNK/KK/alps/mediatek/kernel/drivers/combo/drv_wlan/mt6630/wlan/...
**
** 12 27 2013 eason.tsai
** [ALPS01070904] [Need Patch] [Volunteer Patch][MT6630][Driver]MT6630 Wi-Fi Patch
** update code for ICAP & nvram
**
** 08 20 2013 eason.tsai
** [BORA00002255] [MT6630 Wi-Fi][Driver] develop
** Icap function
**
** 08 20 2013 eason.tsai
** [BORA00002255] [MT6630 Wi-Fi][Driver] develop
** ICAP part for win32
**
** 08 09 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** 1. integrate scheduled scan functionality
** 2. condition compilation for linux-3.4 & linux-3.8 compatibility
** 3. correct CMD queue access to reduce lock scope
**
** 06 19 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** update MAC address handling logic
**
** 06 18 2013 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Get MAC address by NIC_CAPABILITY command
**
** 06 18 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update for 1st connection
**
** 02 19 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** enable build for nic_rx.c & nic_cmd_event.c
**
** 01 22 2013 cp.wu
** [BORA00002253] [MT6630 Wi-Fi][Driver][Firmware] Add NLO and timeout mechanism to SCN module
** modification for ucBssIndex migration
**
** 10 25 2012 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** sync with MT6630 HIFSYS update.
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
**
** 09 04 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** sync RSSI ignoring when BSS is disconnected
**
** 08 24 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** .
**
** 08 24 2012 cp.wu
** [WCXRP00001269] [MT6620 Wi-Fi][Driver] cfg80211 porting merge back to DaVinci
** cfg80211 support merge back from ALPS.JB to DaVinci - MT6620 Driver v2.3 branch.
 *
 * 04 10 2012 yuche.tsai
 * NULL
 * Update address for wifi direct connection issue.
 *
 * 06 15 2011 cm.chang
 * [WCXRP00000785] [MT6620 Wi-Fi][Driver][FW] P2P/BOW MAC address is XOR with AIS MAC address
 * P2P/BOW mac address XOR with local bit instead of OR
 *
 * 03 05 2011 wh.su
 * [WCXRP00000506] [MT6620 Wi-Fi][Driver][FW] Add Security check related code
 * add the code to get the check rsponse and indicate to app.
 *
 * 03 02 2011 wh.su
 * [WCXRP00000506] [MT6620 Wi-Fi][Driver][FW] Add Security check related code
 * Add security check code.
 *
 * 02 24 2011 cp.wu
 * [WCXRP00000493] [MT6620 Wi-Fi][Driver] Do not indicate redundant disconnection to host when entering into RF test mode
 * only indicate DISCONNECTION to host when entering RF test if necessary (connected -> disconnected cases)
 *
 * 01 20 2011 eddie.chen
 * [WCXRP00000374] [MT6620 Wi-Fi][DRV] SW debug control
 * Add Oid for sw control debug command
 *
 * 12 31 2010 cp.wu
 * [WCXRP00000335] [MT6620 Wi-Fi][Driver] change to use milliseconds sleep instead of delay to avoid blocking to system scheduling
 * change to use msleep() and shorten waiting interval to reduce blocking to other task while Wi-Fi driver is being loaded
 *
 * 12 01 2010 cp.wu
 * [WCXRP00000223] MT6620 Wi-Fi][Driver][FW] Adopt NVRAM parameters when enter/exit RF test mode
 * reload NVRAM settings before entering RF test mode and leaving from RF test mode.
 *
 * 11 01 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000150] [MT6620 Wi-Fi][Driver] Add implementation for querying current TX rate from firmware auto rate module
 * 1) Query link speed (TX rate) from firmware directly with buffering mechanism to reduce overhead
 * 2) Remove CNM CH-RECOVER event handling
 * 3) cfg read/write API renamed with kal prefix for unified naming rules.
 *
 * 10 20 2010 cp.wu
 * [WCXRP00000117] [MT6620 Wi-Fi][Driver] Add logic for suspending driver when MT6620 is not responding anymore
 * use OID_CUSTOM_TEST_MODE as indication for driver reset
 * by dropping pending TX packets
 *
 * 10 18 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000086] [MT6620 Wi-Fi][Driver] The mac address is all zero at android
 * complete implementation of Android NVRAM access
 *
 * 09 21 2010 cp.wu
 * [WCXRP00000053] [MT6620 Wi-Fi][Driver] Reset incomplete and might leads to BSOD when entering RF test with AIS associated
 * Do a complete reset with STA-REC null checking for RF test re-entry
 *
 * 09 15 2010 yuche.tsai
 * NULL
 * Start to test AT GO only when P2P state is not IDLE.
 *
 * 09 09 2010 yuche.tsai
 * NULL
 * Add AT GO Test mode after MAC address available.
 *
 * 09 03 2010 kevin.huang
 * NULL
 * Refine #include sequence and solve recursive/nested #include issue
 *
 * 08 30 2010 cp.wu
 * NULL
 * eliminate klockwork errors
 *
 * 08 16 2010 cp.wu
 * NULL
 * Replace CFG_SUPPORT_BOW by CFG_ENABLE_BT_OVER_WIFI.
 * There is no CFG_SUPPORT_BOW in driver domain source.
 *
 * 08 12 2010 cp.wu
 * NULL
 * [AIS-FSM] honor registry setting for adhoc running mode. (A/B/G)
 *
 * 08 11 2010 yuche.tsai
 * NULL
 * Add support for P2P Device Address query from FW.
 *
 * 08 03 2010 cp.wu
 * NULL
 * Centralize mgmt/system service procedures into independent calls.
 *
 * 08 02 2010 cp.wu
 * NULL
 * reset FSMs before entering RF test mode.
 *
 * 07 22 2010 cp.wu
 *
 * 1) refine AIS-FSM indent.
 * 2) when entering RF Test mode, flush 802.1X frames as well
 * 3) when entering D3 state, flush 802.1X frames as well
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 07 05 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 1) change fake BSS_DESC from channel 6 to channel 1 due to channel switching is not done yet.
 * 2) after MAC address is queried from firmware, all related variables in driver domain should be updated as well
 *
 * 06 21 2010 wh.su
 * [WPD00003840][MT6620 5931] Security migration
 * remove duplicate variable for migration.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 05 29 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * change upon request: indicate as disconnected in driver domain when leaving from RF test mode
 *
 * 05 24 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * do not clear scanning list array after disassociation
 *
 * 05 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) disable NETWORK_LAYER_ADDRESSES handling temporally.
 * 2) finish statistics OIDs
 *
 * 05 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * change OID behavior to meet WHQL requirement.
 *
 * 05 20 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) integrate OID_GEN_NETWORK_LAYER_ADDRESSES with CMD_ID_SET_IP_ADDRESS
 * 2) buffer statistics data for 2 seconds
 * 3) use default value for adhoc parameters instead of 0
 *
 * 05 19 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) do not take timeout mechanism for power mode oids
 * 2) retrieve network type from connection status
 * 3) after disassciation, set radio state to off
 * 4) TCP option over IPv6 is supported
 *
 * 05 17 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * correct OID_802_11_DISASSOCIATE handling.
 *
 * 05 17 2010 cp.wu
 * [WPD00003831][MT6620 Wi-Fi] Add framework for Wi-Fi Direct support
 * 1) add timeout handler mechanism for pending command packets
 * 2) add p2p add/removal key
 *
 * 04 16 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * treat BUS access failure as kind of card removal.
 *
 * 04 14 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * information buffer for query oid/ioctl is now buffered in prCmdInfo
 *  *  *  *  *  * instead of glue-layer variable to improve multiple oid/ioctl capability
 *
 * 04 13 2010 cp.wu
 * [WPD00003823][MT6620 Wi-Fi] Add Bluetooth-over-Wi-Fi support
 * add framework for BT-over-Wi-Fi support.
 *  *  *  *  *  *  *  *  *  *  *  *  *  * 1) prPendingCmdInfo is replaced by queue for multiple handler capability
 *  *  *  *  *  *  *  *  *  *  *  *  *  * 2) command sequence number is now increased atomically
 *  *  *  *  *  *  *  *  *  *  *  *  *  * 3) private data could be hold and taken use for other purpose
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * accessing to firmware load/start address, and access to OID handling information
 * are now handled in glue layer
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * rWlanInfo should be placed at adapter rather than glue due to most operations
 *  *  *  * are done in adapter layer.
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add KAL API: kalFlushPendingTxPackets(), and take use of the API
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * eliminate direct access to prGlueInfo->rWlanInfo.eLinkAttr.ucMediaStreamMode from non-glue layer.
 *
 * 04 06 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * improve none-glude code portability
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * sync statistics data structure definition with firmware implementation
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code refine: fgTestMode should be at adapter rather than glue due to the device/fw is also involved
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * eliminate direct access for prGlueInfo->fgIsCardRemoved in non-glue layer
 *
 * 03 30 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * statistics information OIDs are now handled by querying from firmware domain
 *
 * 03 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * indicate media stream mode after set is done
 *
 * 03 24 2010 jeffrey.chang
 * [WPD00003826]Initial import for Linux port
 * initial import for Linux port
 *
 * 03 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement custom OID: EEPROM read/write access
 *
 * 03 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement OID_802_3_MULTICAST_LIST oid handling
 *
 * 02 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * limit RSSI return value to microsoft defined range.
 *
 * 02 09 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. Permanent and current MAC address are now retrieved by CMD/EVENT packets instead of hard-coded address
 *  *  *  *  *  *  * 2. follow MSDN defined behavior when associates to another AP
 *  *  *  *  *  *  * 3. for firmware download, packet size could be up to 2048 bytes
 *
 * 01 29 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * block until firmware finished RF test enter/leave then indicate completion to upper layer
 *
 * 01 29 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * when entering RF test mode and leaving from RF test mode, wait for W_FUNC_RDY bit to be asserted forever until it is set or card is removed.
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 *  *  *  *  *  *  *  * 2. block TX/ordinary OID when RF test mode is engaged
 *  *  *  *  *  *  *  * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 *  *  *  *  *  *  *  * 4. correct some HAL implementation
 *
 * 01 26 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * Under WinXP with SDIO, use prGlueInfo->rHifInfo.pvInformationBuffer instead of prGlueInfo->pvInformationBuffer
 *
 * 01 22 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement following 802.11 OIDs:
 *  *  *  *  * OID_802_11_RSSI,
 *  *  *  *  * OID_802_11_RSSI_TRIGGER,
 *  *  *  *  * OID_802_11_STATISTICS,
 *  *  *  *  * OID_802_11_DISASSOCIATE,
 *  *  *  *  * OID_802_11_POWER_MODE
 *
 * 01 21 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement OID_802_11_MEDIA_STREAM_MODE
 *
 * 12 30 2009 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) According to CMD/EVENT documentation v0.8,
 *  *  *  *  *  *  *  * OID_CUSTOM_TEST_RX_STATUS & OID_CUSTOM_TEST_TX_STATUS is no longer used,
 *  *  *  *  *  *  *  * and result is retrieved by get ATInfo instead
 *  *  *  *  *  *  *  * 2) add 4 counter for recording aggregation statistics
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-12-10 16:47:47 GMT mtk02752
**  only handle MCR read when accessing FW domain register
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-12-08 17:37:28 GMT mtk02752
**  * refine nicCmdEventQueryMcrRead
**  + add TxStatus/RxStatus for RF test QueryInformation OIDs
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-12-02 22:05:45 GMT mtk02752
**  kalOidComplete() will decrease i4OidPendingCount
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-12-01 23:02:57 GMT mtk02752
**  remove unnecessary spin locks
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-12-01 22:51:18 GMT mtk02752
**  maintein i4OidPendingCount
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-11-30 10:55:03 GMT mtk02752
**  modify for compatibility
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-11-23 14:46:32 GMT mtk02752
**  add another version of command-done handler upon new event structure
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-04-29 15:42:33 GMT mtk01461
**  Add comment
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-04-21 19:32:42 GMT mtk01461
**  Add nicCmdEventSetCommon() for general set OID
**  \main\maintrunk.MT6620WiFiDriver_Prj\1 2009-04-21 01:40:35 GMT mtk01461
**  Command Done Handler
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

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/

VOID
nicCmdEventQueryMcrRead(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_PARAM_CUSTOM_MCR_RW_STRUC_T prMcrRdInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_ACCESS_REG prCmdAccessReg;


	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prCmdAccessReg = (P_CMD_ACCESS_REG) (pucEventBuf);

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_MCR_RW_STRUC_T);

		prMcrRdInfo = (P_PARAM_CUSTOM_MCR_RW_STRUC_T) prCmdInfo->pvInformationBuffer;
		prMcrRdInfo->u4McrOffset = prCmdAccessReg->u4Address;
		prMcrRdInfo->u4McrData = prCmdAccessReg->u4Data;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
	}

	return;

}


VOID
nicCmdEventQuerySwCtrlRead(IN P_ADAPTER_T prAdapter,
			   IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_PARAM_CUSTOM_SW_CTRL_STRUC_T prSwCtrlInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_SW_DBG_CTRL_T prCmdSwCtrl;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prCmdSwCtrl = (P_CMD_SW_DBG_CTRL_T) (pucEventBuf);

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_SW_CTRL_STRUC_T);

		prSwCtrlInfo = (P_PARAM_CUSTOM_SW_CTRL_STRUC_T) prCmdInfo->pvInformationBuffer;
		prSwCtrlInfo->u4Id = prCmdSwCtrl->u4Id;
		prSwCtrlInfo->u4Data = prCmdSwCtrl->u4Data;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
	}

	return;

}

VOID
nicCmdEventQueryChipConfig(IN P_ADAPTER_T prAdapter,
			   IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_PARAM_CUSTOM_CHIP_CONFIG_STRUC_T prChipConfigInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_CMD_CHIP_CONFIG_T prCmdChipConfig;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prCmdChipConfig = (P_CMD_CHIP_CONFIG_T) (pucEventBuf);

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_CHIP_CONFIG_STRUC_T);

		if (prCmdInfo->u4InformationBufferLength < sizeof(PARAM_CUSTOM_CHIP_CONFIG_STRUC_T)) {
			DBGLOG(REQ, INFO,
			       ("Chip config u4InformationBufferLength %u is not valid (event)\n",
				prCmdInfo->u4InformationBufferLength));
		}
		prChipConfigInfo =
		    (P_PARAM_CUSTOM_CHIP_CONFIG_STRUC_T) prCmdInfo->pvInformationBuffer;
		prChipConfigInfo->ucRespType = prCmdChipConfig->ucRespType;
		prChipConfigInfo->u2MsgSize = prCmdChipConfig->u2MsgSize;
		DBGLOG(REQ, INFO,
		       ("%s: RespTyep  %u\n", __func__, prChipConfigInfo->ucRespType));
		DBGLOG(REQ, INFO,
		       ("%s: u2MsgSize %u\n", __func__, prChipConfigInfo->u2MsgSize));

		if (prChipConfigInfo->u2MsgSize > CHIP_CONFIG_RESP_SIZE) {
			DBGLOG(REQ, INFO,
			       ("Chip config Msg Size %u is not valid (event)\n",
				prChipConfigInfo->u2MsgSize));
			prChipConfigInfo->u2MsgSize = CHIP_CONFIG_RESP_SIZE;
		}
		kalMemCopy(prChipConfigInfo->aucCmd, prCmdChipConfig->aucCmd,
			   prChipConfigInfo->u2MsgSize);
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
	}

	return;

}


VOID
nicCmdEventSetCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Infomation Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery,
			       prCmdInfo->u4InformationBufferLength, WLAN_STATUS_SUCCESS);
	}

	return;
}

VOID
nicCmdEventSetDisassociate(IN P_ADAPTER_T prAdapter,
			   IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Infomation Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, 0, WLAN_STATUS_SUCCESS);
	}

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo, WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);

#if !defined(LINUX)
	prAdapter->fgIsRadioOff = TRUE;
#endif

	return;
}

VOID
nicCmdEventSetIpAddress(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4Count;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	u4Count = (prCmdInfo->u4SetInfoLen - OFFSET_OF(CMD_SET_NETWORK_ADDRESS_LIST, arNetAddress))
	    / sizeof(IPV4_NETWORK_ADDRESS);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Infomation Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery,
			       OFFSET_OF(PARAM_NETWORK_ADDRESS_LIST, arAddress) + u4Count *
			       (OFFSET_OF(PARAM_NETWORK_ADDRESS, aucAddress) +
				sizeof(PARAM_NETWORK_ADDRESS_IP)), WLAN_STATUS_SUCCESS);
	}

	return;
}

VOID
nicCmdEventQueryRfTestATInfo(IN P_ADAPTER_T prAdapter,
			     IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_TEST_STATUS prTestStatus, prQueryBuffer;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prTestStatus = (P_EVENT_TEST_STATUS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prQueryBuffer = (P_EVENT_TEST_STATUS) prCmdInfo->pvInformationBuffer;

		kalMemCopy(prQueryBuffer, prTestStatus, sizeof(EVENT_TEST_STATUS));

		u4QueryInfoLen = sizeof(EVENT_TEST_STATUS);

		/* Update Query Infomation Length */
		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}

	return;
}

VOID
nicCmdEventQueryLinkQuality(IN P_ADAPTER_T prAdapter,
			    IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	PARAM_RSSI rRssi, *prRssi;
	P_EVENT_LINK_QUALITY prLinkQuality;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prLinkQuality = (P_EVENT_LINK_QUALITY) pucEventBuf;

	rRssi = (PARAM_RSSI) prLinkQuality->cRssi;	/* ranged from (-128 ~ 30) in unit of dBm */

	if (prAdapter->prAisBssInfo->eConnectionState == PARAM_MEDIA_STATE_CONNECTED) {
		if (rRssi > PARAM_WHQL_RSSI_MAX_DBM)
			rRssi = PARAM_WHQL_RSSI_MAX_DBM;
		else if (rRssi < PARAM_WHQL_RSSI_MIN_DBM)
			rRssi = PARAM_WHQL_RSSI_MIN_DBM;
	} else {
		rRssi = PARAM_WHQL_RSSI_MIN_DBM;
	}

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prRssi = (PARAM_RSSI *) prCmdInfo->pvInformationBuffer;

		kalMemCopy(prRssi, &rRssi, sizeof(PARAM_RSSI));
		u4QueryInfoLen = sizeof(PARAM_RSSI);

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This routine is in response of OID_GEN_LINK_SPEED query request
*
* @param prAdapter      Pointer to the Adapter structure.
* @param prCmdInfo      Pointer to the pending command info
* @param pucEventBuf
*
* @retval none
*/
/*----------------------------------------------------------------------------*/
VOID
nicCmdEventQueryLinkSpeed(IN P_ADAPTER_T prAdapter,
			  IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_LINK_QUALITY prLinkQuality;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4LinkSpeed;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prLinkQuality = (P_EVENT_LINK_QUALITY) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		pu4LinkSpeed = (PUINT_32) (prCmdInfo->pvInformationBuffer);

        *pu4LinkSpeed = prLinkQuality->u2LinkSpeed * 5000;

		u4QueryInfoLen = sizeof(UINT_32);

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


VOID
nicCmdEventQueryStatistics(IN P_ADAPTER_T prAdapter,
			   IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_PARAM_802_11_STATISTICS_STRUCT_T prStatistics;
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		u4QueryInfoLen = sizeof(PARAM_802_11_STATISTICS_STRUCT_T);
		prStatistics = (P_PARAM_802_11_STATISTICS_STRUCT_T) prCmdInfo->pvInformationBuffer;

		prStatistics->u4Length = sizeof(PARAM_802_11_STATISTICS_STRUCT_T);
		prStatistics->rTransmittedFragmentCount
		    = prEventStatistics->rTransmittedFragmentCount;
		prStatistics->rMulticastTransmittedFrameCount
		    = prEventStatistics->rMulticastTransmittedFrameCount;
		prStatistics->rFailedCount = prEventStatistics->rFailedCount;
		prStatistics->rRetryCount = prEventStatistics->rRetryCount;
		prStatistics->rMultipleRetryCount = prEventStatistics->rMultipleRetryCount;
		prStatistics->rRTSSuccessCount = prEventStatistics->rRTSSuccessCount;
		prStatistics->rRTSFailureCount = prEventStatistics->rRTSFailureCount;
		prStatistics->rACKFailureCount = prEventStatistics->rACKFailureCount;
		prStatistics->rFrameDuplicateCount = prEventStatistics->rFrameDuplicateCount;
		prStatistics->rReceivedFragmentCount = prEventStatistics->rReceivedFragmentCount;
		prStatistics->rMulticastReceivedFrameCount
		    = prEventStatistics->rMulticastReceivedFrameCount;
		prStatistics->rFCSErrorCount = prEventStatistics->rFCSErrorCount;
		prStatistics->rTKIPLocalMICFailures.QuadPart = 0;
		prStatistics->rTKIPICVErrors.QuadPart = 0;
		prStatistics->rTKIPCounterMeasuresInvoked.QuadPart = 0;
		prStatistics->rTKIPReplays.QuadPart = 0;
		prStatistics->rCCMPFormatErrors.QuadPart = 0;
		prStatistics->rCCMPReplays.QuadPart = 0;
		prStatistics->rCCMPDecryptErrors.QuadPart = 0;
		prStatistics->rFourWayHandshakeFailures.QuadPart = 0;
		prStatistics->rWEPUndecryptableCount.QuadPart = 0;
		prStatistics->rWEPICVErrorCount.QuadPart = 0;
		prStatistics->rDecryptSuccessCount.QuadPart = 0;
		prStatistics->rDecryptFailureCount.QuadPart = 0;

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID
nicCmdEventEnterRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4WHISR = 0, u4Value = 0;
	UINT_16 au2TxCount[16];

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	/* [driver-land] */
	/* prAdapter->fgTestMode = TRUE; */
	if (prAdapter->fgTestMode) {
		prAdapter->fgTestMode = FALSE;
	} else {
		prAdapter->fgTestMode = TRUE;
	}

	/* 0. always indicate disconnection */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_DISCONNECTED) {
		kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
					     WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);
	}
	/* 1. Remove pending TX */
	nicTxRelease(prAdapter, TRUE);

	/* 1.1 clear pending Security / Management Frames */
	kalClearSecurityFrames(prAdapter->prGlueInfo);
	kalClearMgmtFrames(prAdapter->prGlueInfo);

	/* 1.2 clear pending TX packet queued in glue layer */
	kalFlushPendingTxPackets(prAdapter->prGlueInfo);

	/* 2. Reset driver-domain FSMs */
	nicUninitMGMT(prAdapter);

	nicResetSystemService(prAdapter);
	nicInitMGMT(prAdapter, NULL);

	/* 3. Disable Interrupt */
	HAL_INTR_DISABLE(prAdapter);

	/* 4. Block til firmware completed entering into RF test mode */
	kalMsleep(500);
	while (1) {
		HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

		if (u4Value & WCIR_WLAN_READY) {
			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
			   || fgIsBusAccessFailed == TRUE) {
			if (prCmdInfo->fgIsOid) {
				/* Update Set Infomation Length */
				kalOidComplete(prAdapter->prGlueInfo,
					       prCmdInfo->fgSetQuery,
					       prCmdInfo->u4SetInfoLen, WLAN_STATUS_NOT_SUPPORTED);

			}
			return;
		} else
			kalMsleep(10);
	}

	/* 5. Clear Interrupt Status */
	HAL_READ_INTR_STATUS(prAdapter, 4, (PUINT_8) & u4WHISR);
	if (HAL_IS_TX_DONE_INTR(u4WHISR)) {
		HAL_READ_TX_RELEASED_COUNT(prAdapter, au2TxCount);
	}
	/* 6. Reset TX Counter */
	nicTxResetResource(prAdapter);

	/* 7. Re-enable Interrupt */
	HAL_INTR_ENABLE(prAdapter);

	/* 8. completion indication */
	if (prCmdInfo->fgIsOid) {
		/* Update Set Infomation Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, WLAN_STATUS_SUCCESS);
	}
#if CFG_SUPPORT_NVRAM
	/* 9. load manufacture data */
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE) {
		wlanLoadManufactureData(prAdapter, kalGetConfiguration(prAdapter->prGlueInfo));
	} else {
		DBGLOG(REQ, WARN, ("%s: load manufacture data fail\n", __func__));	
	}
#endif

	return;
}

VOID
nicCmdEventLeaveRfTest(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4WHISR = 0, u4Value = 0;
	UINT_16 au2TxCount[16];

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	/* 1. Disable Interrupt */
	HAL_INTR_DISABLE(prAdapter);

	/* 2. Block til firmware completed leaving from RF test mode */
	kalMsleep(500);
	while (1) {
		HAL_MCR_RD(prAdapter, MCR_WCIR, &u4Value);

		if (u4Value & WCIR_WLAN_READY) {
			break;
		} else if (kalIsCardRemoved(prAdapter->prGlueInfo) == TRUE
			   || fgIsBusAccessFailed == TRUE) {
			if (prCmdInfo->fgIsOid) {
				/* Update Set Infomation Length */
				kalOidComplete(prAdapter->prGlueInfo,
					       prCmdInfo->fgSetQuery,
					       prCmdInfo->u4SetInfoLen, WLAN_STATUS_NOT_SUPPORTED);

			}
			return;
		} else {
			kalMsleep(10);
		}
	}

	/* 3. Clear Interrupt Status */
	HAL_READ_INTR_STATUS(prAdapter, 4, (PUINT_8) & u4WHISR);
	if (HAL_IS_TX_DONE_INTR(u4WHISR)) {
		HAL_READ_TX_RELEASED_COUNT(prAdapter, au2TxCount);
	}
	/* 4. Reset TX Counter */
	nicTxResetResource(prAdapter);

	/* 5. Re-enable Interrupt */
	HAL_INTR_ENABLE(prAdapter);

	/* 6. set driver-land variable */
	prAdapter->fgTestMode = FALSE;
	prAdapter->fgIcapMode = FALSE;

	/* 7. completion indication */
	if (prCmdInfo->fgIsOid) {
		/* Update Set Infomation Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, WLAN_STATUS_SUCCESS);
	}

	/* 8. Indicate as disconnected */
	if (kalGetMediaStateIndicated(prAdapter->prGlueInfo) != PARAM_MEDIA_STATE_DISCONNECTED) {

		kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
					     WLAN_STATUS_MEDIA_DISCONNECT, NULL, 0);

		prAdapter->rWlanInfo.u4SysTime = kalGetTimeTick();
	}
#if CFG_SUPPORT_NVRAM
	/* 9. load manufacture data */
	if (kalIsConfigurationExist(prAdapter->prGlueInfo) == TRUE) {
		wlanLoadManufactureData(prAdapter, kalGetConfiguration(prAdapter->prGlueInfo));
	} else {
		DBGLOG(REQ, WARN, ("%s: load manufacture data fail\n", __func__));	
	}
#endif

	/* 10. Override network address */
	wlanUpdateNetworkAddress(prAdapter);

	return;
}


VOID
nicCmdEventQueryMcastAddr(IN P_ADAPTER_T prAdapter,
			  IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_MAC_MCAST_ADDR prEventMacMcastAddr;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventMacMcastAddr = (P_EVENT_MAC_MCAST_ADDR) (pucEventBuf);

		u4QueryInfoLen = prEventMacMcastAddr->u4NumOfGroupAddr * MAC_ADDR_LEN;

		/* buffer length check */
		if (prCmdInfo->u4InformationBufferLength < u4QueryInfoLen) {
			kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
				       WLAN_STATUS_BUFFER_TOO_SHORT);
		} else {
			kalMemCopy(prCmdInfo->pvInformationBuffer,
				   prEventMacMcastAddr->arAddress,
				   prEventMacMcastAddr->u4NumOfGroupAddr * MAC_ADDR_LEN);

			kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
				       WLAN_STATUS_SUCCESS);
		}
	}
}

VOID
nicCmdEventQueryEepromRead(IN P_ADAPTER_T prAdapter,
			   IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_PARAM_CUSTOM_EEPROM_RW_STRUC_T prEepromRdInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_ACCESS_EEPROM prEventAccessEeprom;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventAccessEeprom = (P_EVENT_ACCESS_EEPROM) (pucEventBuf);

		u4QueryInfoLen = sizeof(PARAM_CUSTOM_EEPROM_RW_STRUC_T);

		prEepromRdInfo = (P_PARAM_CUSTOM_EEPROM_RW_STRUC_T) prCmdInfo->pvInformationBuffer;
		prEepromRdInfo->ucEepromIndex = (UINT_8) (prEventAccessEeprom->u2Offset);
		prEepromRdInfo->u2EepromData = prEventAccessEeprom->u2Data;

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
	}

	return;

}


VOID
nicCmdEventSetMediaStreamMode(IN P_ADAPTER_T prAdapter,
			      IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	PARAM_MEDIA_STREAMING_INDICATION rParamMediaStreamIndication;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	if (prCmdInfo->fgIsOid) {
		/* Update Set Infomation Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, prCmdInfo->u4SetInfoLen, WLAN_STATUS_SUCCESS);
	}

	rParamMediaStreamIndication.rStatus.eStatusType = ENUM_STATUS_TYPE_MEDIA_STREAM_MODE;
	rParamMediaStreamIndication.eMediaStreamMode =
	    prAdapter->rWlanInfo.eLinkAttr.ucMediaStreamMode == 0 ?
	    ENUM_MEDIA_STREAM_OFF : ENUM_MEDIA_STREAM_ON;

	kalIndicateStatusAndComplete(prAdapter->prGlueInfo,
				     WLAN_STATUS_MEDIA_SPECIFIC_INDICATION,
				     (PVOID) & rParamMediaStreamIndication,
				     sizeof(PARAM_MEDIA_STREAMING_INDICATION));
}


VOID
nicCmdEventSetStopSchedScan(IN P_ADAPTER_T prAdapter,
			    IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{

    //DBGLOG(SCN, INFO, ("--->nicCmdEventSetStopSchedScan  \n" ));
	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
    //DBGLOG(SCN, INFO, ("<--kalSchedScanStopped  \n" ));

	if (prCmdInfo->fgIsOid) {
		/* Update Set Infomation Length */
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery,
			       prCmdInfo->u4InformationBufferLength, WLAN_STATUS_SUCCESS);
	}


    DBGLOG(SCN, INFO, ("nicCmdEventSetStopSchedScan OID done, release lock and send event to uplayer \n" ));   

    kalSchedScanStopped(prAdapter->prGlueInfo);  /*Due to dead lock issue, need to release the IO control before calling kernel APIs*/


	return;
}


/* Statistics responder */
VOID
nicCmdEventQueryXmitOk(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rTransmittedFragmentCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = prEventStatistics->rTransmittedFragmentCount.QuadPart;
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


VOID
nicCmdEventQueryRecvOk(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rReceivedFragmentCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = prEventStatistics->rReceivedFragmentCount.QuadPart;
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}

VOID
nicCmdEventQueryXmitError(IN P_ADAPTER_T prAdapter,
			  IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rFailedCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = (UINT_64) prEventStatistics->rFailedCount.QuadPart;
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


VOID
nicCmdEventQueryRecvError(IN P_ADAPTER_T prAdapter,
			  IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rFCSErrorCount.QuadPart;
			/* @FIXME, RX_ERROR_DROP_COUNT/RX_FIFO_FULL_DROP_COUNT is not calculated */
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = prEventStatistics->rFCSErrorCount.QuadPart;
			/* @FIXME, RX_ERROR_DROP_COUNT/RX_FIFO_FULL_DROP_COUNT is not calculated */
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


VOID
nicCmdEventQueryRecvNoBuffer(IN P_ADAPTER_T prAdapter,
			     IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = 0;	/* @FIXME? */
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = 0;	/* @FIXME? */
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


VOID
nicCmdEventQueryRecvCrcError(IN P_ADAPTER_T prAdapter,
			     IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rFCSErrorCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = prEventStatistics->rFCSErrorCount.QuadPart;
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


VOID
nicCmdEventQueryRecvErrorAlignment(IN P_ADAPTER_T prAdapter,
				   IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) 0;	/* @FIXME */
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = 0;	/* @FIXME */
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


VOID
nicCmdEventQueryXmitOneCollision(IN P_ADAPTER_T prAdapter,
				 IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data =
			    (UINT_32) (prEventStatistics->rMultipleRetryCount.QuadPart -
				       prEventStatistics->rRetryCount.QuadPart);
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data =
			    (UINT_64) (prEventStatistics->rMultipleRetryCount.QuadPart -
				       prEventStatistics->rRetryCount.QuadPart);
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


VOID
nicCmdEventQueryXmitMoreCollisions(IN P_ADAPTER_T prAdapter,
				   IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rMultipleRetryCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = (UINT_64) prEventStatistics->rMultipleRetryCount.QuadPart;
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


VOID
nicCmdEventQueryXmitMaxCollisions(IN P_ADAPTER_T prAdapter,
				  IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	P_EVENT_STATISTICS prEventStatistics;
	P_GLUE_INFO_T prGlueInfo;
	UINT_32 u4QueryInfoLen;
	PUINT_32 pu4Data;
	PUINT_64 pu8Data;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);

	prEventStatistics = (P_EVENT_STATISTICS) pucEventBuf;

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;

		if (prCmdInfo->u4InformationBufferLength == sizeof(UINT_32)) {
			u4QueryInfoLen = sizeof(UINT_32);

			pu4Data = (PUINT_32) prCmdInfo->pvInformationBuffer;
			*pu4Data = (UINT_32) prEventStatistics->rFailedCount.QuadPart;
		} else {
			u4QueryInfoLen = sizeof(UINT_64);

			pu8Data = (PUINT_64) prCmdInfo->pvInformationBuffer;
			*pu8Data = (UINT_64) prEventStatistics->rFailedCount.QuadPart;
		}

		kalOidComplete(prGlueInfo,
			       prCmdInfo->fgSetQuery, u4QueryInfoLen, WLAN_STATUS_SUCCESS);
	}
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when command by OID/ioctl has been timeout
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
*
* @return TRUE
*         FALSE
*/
/*----------------------------------------------------------------------------*/
VOID nicOidCmdTimeoutCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	ASSERT(prAdapter);

	if (prCmdInfo->fgIsOid) {
		kalOidComplete(prAdapter->prGlueInfo,
			       prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
	}
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is a generic command timeout handler
*
* @param pfnOidHandler      Pointer to the OID handler
*
* @return none
*/
/*----------------------------------------------------------------------------*/
VOID nicCmdTimeoutCommon(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	ASSERT(prAdapter);
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when command for entering RF test has
*        failed sending due to timeout (highly possibly by firmware crash)
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID nicOidCmdEnterRFTestTimeout(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo)
{
	ASSERT(prAdapter);

	/* 1. Remove pending TX frames */
	nicTxRelease(prAdapter, TRUE);

	/* 1.1 clear pending Security / Management Frames */
	kalClearSecurityFrames(prAdapter->prGlueInfo);
	kalClearMgmtFrames(prAdapter->prGlueInfo);

	/* 1.2 clear pending TX packet queued in glue layer */
	kalFlushPendingTxPackets(prAdapter->prGlueInfo);

	/* 2. indiate for OID failure */
	kalOidComplete(prAdapter->prGlueInfo, prCmdInfo->fgSetQuery, 0, WLAN_STATUS_FAILURE);
}


/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called to handle dump burst event
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo         Pointer to the command information
* @param pucEventBuf       Pointer to event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/


VOID nicEventQueryMemDump(IN P_ADAPTER_T prAdapter, IN PUINT_8 pucEventBuf)
{
	P_EVENT_DUMP_MEM_T prEventDumpMem;
	static UINT_8 aucPath[256];
	static UINT_8 aucPath_done[300];
	static UINT_32 u4CurTimeTick;

	ASSERT(prAdapter);
	ASSERT(pucEventBuf);

	sprintf(aucPath, "/data/blbist/dump_%05ld.hex", g_u2DumpIndex);

	prEventDumpMem = (P_EVENT_DUMP_MEM_T) (pucEventBuf);

	if (kalCheckPath(aucPath) == -1) {
		kalMemSet(aucPath, 0x00, 256);
		sprintf(aucPath, "/data/dump_%05ld.hex", g_u2DumpIndex);
	}


	if (prEventDumpMem->ucFragNum == 1) {
		/* Store memory dump into sdcard,
		 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
		 */
		u4CurTimeTick = kalGetTimeTick();
#if defined(LINUX)

		/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
		sprintf(aucPath, "/data/blbist/dump_%05ld.hex", g_u2DumpIndex);
		if (kalCheckPath(aucPath) == -1) {
			kalMemSet(aucPath, 0x00, 256);
			sprintf(aucPath, "/data/dump_%05ld.hex", g_u2DumpIndex);
		}
#else
		kal_sprintf_ddk(aucPath, sizeof(aucPath),
				u4CurTimeTick,
				prEventDumpMem->u4Address,
				prEventDumpMem->u4Length + prEventDumpMem->u4RemainLength);
#endif
		kalWriteToFile(aucPath, FALSE, &prEventDumpMem->aucBuffer[0],
			       prEventDumpMem->u4Length);
	} else {
		/* Append current memory dump to the hex file */
		kalWriteToFile(aucPath, TRUE, &prEventDumpMem->aucBuffer[0],
			       prEventDumpMem->u4Length);
	}
	DBGLOG(INIT, INFO,
	       (": ==> (u4RemainLength = %x, u4Address=%x )\n", prEventDumpMem->u4RemainLength,
		prEventDumpMem->u4Address));

	if (prEventDumpMem->u4RemainLength == 0 || prEventDumpMem->u4Address == 0xFFFFFFFF) {

		/* The request is finished or firmware response a error */
		/* Reply time tick to iwpriv */

		g_bIcapEnable = FALSE;
		g_bCaptureDone = TRUE;

		sprintf(aucPath_done, "/data/blbist/file_dump_done.txt");
		if (kalCheckPath(aucPath_done) == -1) {
			kalMemSet(aucPath_done, 0x00, 256);
			sprintf(aucPath_done, "/data/file_dump_done.txt");
		}
		DBGLOG(INIT, INFO, (": ==> gen done_file\n"));
		kalWriteToFile(aucPath_done, FALSE, aucPath_done, sizeof(aucPath_done));
		g_u2DumpIndex++;

	}

}

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when command for memory dump has
*        replied a event.
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo         Pointer to the command information
* @param pucEventBuf       Pointer to event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID
nicCmdEventQueryMemDump(IN P_ADAPTER_T prAdapter, IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_PARAM_CUSTOM_MEM_DUMP_STRUC_T prMemDumpInfo;
	P_GLUE_INFO_T prGlueInfo;
	P_EVENT_DUMP_MEM_T prEventDumpMem;
	static UINT_8 aucPath[256];
	static UINT_8 aucPath_done[300];
	static UINT_32 u4CurTimeTick;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (1) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventDumpMem = (P_EVENT_DUMP_MEM_T) (pucEventBuf);

		u4QueryInfoLen = sizeof(P_PARAM_CUSTOM_MEM_DUMP_STRUC_T);

		prMemDumpInfo = (P_PARAM_CUSTOM_MEM_DUMP_STRUC_T) prCmdInfo->pvInformationBuffer;
		prMemDumpInfo->u4Address = prEventDumpMem->u4Address;
		prMemDumpInfo->u4Length = prEventDumpMem->u4Length;
		prMemDumpInfo->u4RemainLength = prEventDumpMem->u4RemainLength;
		prMemDumpInfo->ucFragNum = prEventDumpMem->ucFragNum;

#if 0
		do {
			UINT_32 i = 0;
			printk("Rx dump address 0x%X, Length %d, FragNum %d, remain %d\n",
			       prEventDumpMem->u4Address,
			       prEventDumpMem->u4Length,
			       prEventDumpMem->ucFragNum, prEventDumpMem->u4RemainLength);
#if 0
			for (i = 0; i < prEventDumpMem->u4Length; i++) {
				printk("%02X ", prEventDumpMem->aucBuffer[i]);
				if (i % 32 == 31) {
					printk("\n");
				}
			}
#endif
		} while (FALSE);
#endif

		if (prEventDumpMem->ucFragNum == 1) {
			/* Store memory dump into sdcard,
			 * path /sdcard/dump_<current  system tick>_<memory address>_<memory length>.hex
			 */
			u4CurTimeTick = kalGetTimeTick();
#if defined(LINUX)
#if 0
			sprintf(aucPath, "/sdcard/dump_%ld_0x%08lX_%ld.hex",
				u4CurTimeTick,
				prEventDumpMem->u4Address,
				prEventDumpMem->u4Length + prEventDumpMem->u4RemainLength);
#else

			/*if blbist mkdir undre /data/blbist, the dump files wouls put on it */
			sprintf(aucPath, "/data/blbist/dump_%05ld.hex", g_u2DumpIndex);
			if (kalCheckPath(aucPath) == -1) {
				kalMemSet(aucPath, 0x00, 256);
				sprintf(aucPath, "/data/dump_%05ld.hex", g_u2DumpIndex);
			}
#endif
#else
			kal_sprintf_ddk(aucPath, sizeof(aucPath),
					u4CurTimeTick,
					prEventDumpMem->u4Address,
					prEventDumpMem->u4Length + prEventDumpMem->u4RemainLength);
			/* strcpy(aucPath, "dump.hex"); */
#endif
			kalWriteToFile(aucPath, FALSE, &prEventDumpMem->aucBuffer[0],
				       prEventDumpMem->u4Length);
		} else {
			/* Append current memory dump to the hex file */
			kalWriteToFile(aucPath, TRUE, &prEventDumpMem->aucBuffer[0],
				       prEventDumpMem->u4Length);
		}

		if (prEventDumpMem->u4RemainLength == 0 || prEventDumpMem->u4Address == 0xFFFFFFFF) {
			/* The request is finished or firmware response a error */
			/* Reply time tick to iwpriv */
			if (prCmdInfo->fgIsOid) {

				/* the oid would be complete only in oid-trigger  mode, that is no need to if the event-trigger */
				if (g_bIcapEnable == FALSE) {
					*((PUINT_32) prCmdInfo->pvInformationBuffer) =
					    u4CurTimeTick;
					kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery,
						       u4QueryInfoLen, WLAN_STATUS_SUCCESS);
				}
			}
			g_bIcapEnable = FALSE;
			g_bCaptureDone = TRUE;
#if defined(LINUX)
			sprintf(aucPath_done, "/data/blbist/file_dump_done.txt");
			if (kalCheckPath(aucPath_done) == -1) {
				kalMemSet(aucPath_done, 0x00, 256);
				sprintf(aucPath_done, "/data/file_dump_done.txt");
			}
			DBGLOG(INIT, INFO, (": ==> gen done_file\n"));
			kalWriteToFile(aucPath_done, FALSE, aucPath_done, sizeof(aucPath_done));
			g_u2DumpIndex++;

#else
			kal_sprintf_done_ddk(aucPath_done, sizeof(aucPath_done));
			kalWriteToFile(aucPath_done, FALSE, aucPath_done, sizeof(aucPath_done));
#endif
		} else {
#if defined(LINUX)

#else				/* 2013/05/26 fw would try to send the buffer successfully */
			/* The memory dump request is not finished, Send next command */
			wlanSendMemDumpCmd(prAdapter,
					   prCmdInfo->pvInformationBuffer,
					   prCmdInfo->u4InformationBufferLength);
#endif
		}
	}

	return;

}


#if CFG_SUPPORT_BATCH_SCAN
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when event for SUPPORT_BATCH_SCAN
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
* @param pucEventBuf        Pointer to the event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID
nicCmdEventBatchScanResult(IN P_ADAPTER_T prAdapter,
			   IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_EVENT_BATCH_RESULT_T prEventBatchResult;
	P_GLUE_INFO_T prGlueInfo;

	DBGLOG(SCN, TRACE, ("nicCmdEventBatchScanResult"));

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEventBatchResult = (P_EVENT_BATCH_RESULT_T) pucEventBuf;

		u4QueryInfoLen = sizeof(EVENT_BATCH_RESULT_T);
		kalMemCopy(prCmdInfo->pvInformationBuffer, prEventBatchResult,
			   sizeof(EVENT_BATCH_RESULT_T));

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
	}

	return;
}
#endif


#if CFG_SUPPORT_BUILD_DATE_CODE
/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when event for build date code information
*        has been retrieved
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
* @param pucEventBuf        Pointer to the event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID
nicCmdEventBuildDateCode(IN P_ADAPTER_T prAdapter,
			 IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_EVENT_BUILD_DATE_CODE prEvent;
	P_GLUE_INFO_T prGlueInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);

	/* 4 <2> Update information of OID */
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEvent = (P_EVENT_BUILD_DATE_CODE) pucEventBuf;

		u4QueryInfoLen = sizeof(UINT_8) * 16;
		kalMemCopy(prCmdInfo->pvInformationBuffer, prEvent->aucDateCode,
			   sizeof(UINT_8) * 16);

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
	}

	return;
}
#endif



/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when event for query STA link status
*        has been retrieved
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
* @param pucEventBuf        Pointer to the event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID
nicCmdEventQueryStaStatistics(IN P_ADAPTER_T prAdapter,
			      IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_EVENT_STA_STATISTICS_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	P_PARAM_GET_STA_STATISTICS prStaStatistics;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);
	ASSERT(prCmdInfo->pvInformationBuffer);

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEvent = (P_EVENT_STA_STATISTICS_T) pucEventBuf;
		prStaStatistics = (P_PARAM_GET_STA_STATISTICS) prCmdInfo->pvInformationBuffer;

		u4QueryInfoLen = sizeof(PARAM_GET_STA_STA_STATISTICS);

		/* Statistics from FW is valid */
		if (prEvent->u4Flags & BIT(0)) {
			prStaStatistics->ucPer = prEvent->ucPer;
			prStaStatistics->ucRcpi = prEvent->ucRcpi;
			prStaStatistics->u4PhyMode = prEvent->u4PhyMode;
			prStaStatistics->u2LinkSpeed = prEvent->u2LinkSpeed;

			prStaStatistics->u4TxFailCount = prEvent->u4TxFailCount;
			prStaStatistics->u4TxLifeTimeoutCount = prEvent->u4TxLifeTimeoutCount;

			if (prEvent->u4TxCount) {
				UINT_32 u4TxDoneAirTimeMs =
				    USEC_TO_MSEC(prEvent->u4TxDoneAirTime * 32);

				prStaStatistics->u4TxAverageAirTime =
				    (u4TxDoneAirTimeMs / prEvent->u4TxCount);
			} else {
				prStaStatistics->u4TxAverageAirTime = 0;
			}
		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
	}

}

#if CFG_AUTO_CHANNEL_SEL_SUPPORT

/* 4  Auto Channel Selection */

/*----------------------------------------------------------------------------*/
/*!
* @brief This function is called when event for query STA link status
*        has been retrieved
*
* @param prAdapter          Pointer to the Adapter structure.
* @param prCmdInfo          Pointer to the command information
* @param pucEventBuf        Pointer to the event buffer
*
* @return none
*
*/
/*----------------------------------------------------------------------------*/
VOID
nicCmdEventQueryChannelLoad(IN P_ADAPTER_T prAdapter,
			    IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_EVENT_CHN_LOAD_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	P_PARAM_GET_CHN_LOAD prChnLoad;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);
	ASSERT(prCmdInfo->pvInformationBuffer);

	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEvent = (P_EVENT_CHN_LOAD_T) pucEventBuf;	/* 4 The firmware responsed data */
		prChnLoad = (P_PARAM_GET_CHN_LOAD) prCmdInfo->pvInformationBuffer;	/* 4 Fill the firmware data in and send it back to host */

		u4QueryInfoLen = sizeof(PARAM_GET_CHN_LOAD);

		/* Statistics from FW is valid */
		if (prEvent->u4Flags & BIT(0)) {
			prChnLoad->rEachChnLoad[0].ucChannel = prEvent->ucChannel;
			prChnLoad->rEachChnLoad[0].u2ChannelLoad = prEvent->u2ChannelLoad;
			printk("CHN[%d]=%d\n", prEvent->ucChannel, prEvent->u2ChannelLoad);

		}

		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
	}

}

VOID
nicCmdEventQueryLTESafeChn(IN P_ADAPTER_T prAdapter,
			   IN P_CMD_INFO_T prCmdInfo, IN PUINT_8 pucEventBuf)
{
	UINT_32 u4QueryInfoLen;
	P_EVENT_LTE_MODE_T prEvent;
	P_GLUE_INFO_T prGlueInfo;
	P_PARAM_GET_CHN_LOAD prLteSafeChnInfo;

	ASSERT(prAdapter);
	ASSERT(prCmdInfo);
	ASSERT(pucEventBuf);
	ASSERT(prCmdInfo->pvInformationBuffer);
	if (prCmdInfo->fgIsOid) {
		prGlueInfo = prAdapter->prGlueInfo;
		prEvent = (P_EVENT_LTE_MODE_T) pucEventBuf;	/* 4 The firmware responsed data */

		prLteSafeChnInfo = (P_PARAM_GET_CHN_LOAD) prCmdInfo->pvInformationBuffer;

		u4QueryInfoLen = sizeof(PARAM_GET_CHN_LOAD);

		/* Statistics from FW is valid */
		if (prEvent->u4Flags & BIT(0)) {
			/* prLteSafeChnInfo->rLteSafeChnList.ucChannelHigh= prEvent->rLteSafeChn.ucChannelHigh; */
			/* prLteSafeChnInfo->rLteSafeChnList.ucChannelLow= prEvent->rLteSafeChn.ucChannelLow; */
			prLteSafeChnInfo->rLteSafeChnList.u4SafeChannelBitmask[0] = prEvent->rLteSafeChn.u4SafeChannelBitmask[0];
        		if (prEvent->ucVersion != 0) {
            			prLteSafeChnInfo->rLteSafeChnList.u4SafeChannelBitmask[1] = prEvent->rLteSafeChn.u4SafeChannelBitmask[1];
            			prLteSafeChnInfo->rLteSafeChnList.u4SafeChannelBitmask[2] = prEvent->rLteSafeChn.u4SafeChannelBitmask[2];
            			prLteSafeChnInfo->rLteSafeChnList.u4SafeChannelBitmask[3] = prEvent->rLteSafeChn.u4SafeChannelBitmask[3];
        		}
			DBGLOG(P2P, INFO,("[Query-info Auto Channel]LTE safe channels 0x%08x\n", prLteSafeChnInfo->rLteSafeChnList.u4SafeChannelBitmask[0]));
        	}
		kalOidComplete(prGlueInfo, prCmdInfo->fgSetQuery, u4QueryInfoLen,
			       WLAN_STATUS_SUCCESS);
	}

}
#endif
