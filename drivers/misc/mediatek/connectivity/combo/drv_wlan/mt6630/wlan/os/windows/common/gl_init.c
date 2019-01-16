/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/common/gl_init.c#1 $
*/

/*! \file   gl_init.c
    \brief  Windows OS related initialization routins

*/



/*
** $Log: gl_init.c $
**
** 08 05 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** for windows build success
**
** 07 23 2013 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** 1. build success for win32 port
** 2. add SDIO test read/write pattern for HQA tests (default off)
**
** 01 21 2013 terry.wu
** [BORA00002207] [MT6630 Wi-Fi] TXM & MQM Implementation
** Update TX path based on new ucBssIndex modifications.
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 09 13 2011 pat.lu
 * [WCXRP00000981] [WiFi][Win Driver] SDIO interface should be released in free adapter API when the driver in not initialized successful
 * Add SDIO interface release call at freeadapter API for XP driver unsuccessful initialization case.
 *
 * 08 15 2011 cp.wu
 * [WCXRP00000913] [MT6620 Wi-Fi] create repository of source code dedicated for MT6620 E6 ASIC
 * support to load different firmware image for E3/E4/E5 and E6 ASIC on win32 platforms.
 *
 * 03 14 2011 terry.wu
 * [WCXRP00000521] [MT6620 Wi-Fi][Driver] Remove non-standard debug message
 * Revert windows debug message.
 *
 * 01 04 2011 cp.wu
 * [WCXRP00000338] [MT6620 Wi-Fi][Driver] Separate kalMemAlloc into kmalloc and vmalloc implementations to ease physically continous memory demands
 * separate kalMemAlloc() into virtually-continous and physically-continous type to ease slab system pressure
 *
 * 11 01 2010 cp.wu
 * [WCXRP00000056] [MT6620 Wi-Fi][Driver] NVRAM implementation with Version Check[WCXRP00000150] [MT6620 Wi-Fi][Driver] Add implementation for querying current TX rate from firmware auto rate module
 * 1) Query link speed (TX rate) from firmware directly with buffering mechanism to reduce overhead
 * 2) Remove CNM CH-RECOVER event handling
 * 3) cfg read/write API renamed with kal prefix for unified naming rules.
 *
 * 09 09 2010 yuche.tsai
 * NULL
 * Remove AT GO Test mode start event in mpinitialize.
 *
 * 08 26 2010 yuche.tsai
 * NULL
 * Add AT GO test configure mode under WinXP.
 * Please enable 1. CFG_ENABLE_WIFI_DIRECT, 2. CFG_TEST_WIFI_DIRECT_GO, 3. CFG_SUPPORT_AAA
 *
 * 08 04 2010 cp.wu
 * NULL
 * create working thread after wlanAdpaterStart() succeeded.
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 24 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * 802.1x and bluetooth-over-Wi-Fi security frames are now delievered to firmware via command path instead of data path.
 *
 * 06 08 2010 cp.wu
 * [WPD00003833][MT6620 and MT5931] Driver migration
 * cnm_timer has been migrated.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 06 03 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * move timer callback to glue layer.
 *
 * 05 17 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add CFG_STARTUP_DEBUG for debugging starting up issue.
 *
 * 04 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * rWlanInfo should be placed at adapter rather than glue due to most operations
 *  *  *  *  *  *  *  *  *  *  * are done in adapter layer.
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code refine: fgTestMode should be at adapter rather than glue due to the device/fw is also involved
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) for some OID, never do timeout expiration
 *  *  *  *  * 2) add 2 kal API for later integration
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) eliminate unused definitions
 *  *  * 2) ready bit will be polled for limited iteration
 *
 * 03 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add another spin-lock to protect MsduInfoList due to it might be accessed by different thread.
 *  *  *  *  * 2) change own-back acquiring procedure to wait for up to 16.67 seconds
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) the use of prPendingOid revised, all accessing are now protected by spin lock
 *  *  *  *  * 2) ensure wlanReleasePendingOid will clear all command queues
 *
 * 03 02 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add mutex to avoid multiple access to qmTxQueue simultaneously.
 *
 * 02 11 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. add logic for firmware download
 *  *  *  * 2. firmware image filename and start/load address are now retrieved from registry
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement host-side firmware download logic
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove unused function in nic_rx.c [which has been handled in que_mgt.c]
 *  *  *  *  *  *  * 2) firmware image length is now retrieved via NdisFileOpen
 *  *  *  *  *  *  * 3) firmware image is not structured by (P_IMG_SEC_HDR_T) anymore
 *  *  *  *  *  *  * 4) nicRxWaitResponse() revised
 *  *  *  *  *  *  * 5) another set of TQ counter default value is added for fw-download state
 *  *  *  *  *  *  * 6) Wi-Fi load address is now retrieved from registry too
 *
 * 02 08 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * prepare for implementing fw download logic
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) implement timeout mechanism when OID is pending for longer than 1 second
 *  *  *  * 2) allow OID_802_11_CONFIGURATION to be executed when RF test mode is turned on
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 *  *  *  *  *  *  *  *  *  * 2. block TX/ordinary OID when RF test mode is engaged
 *  *  *  *  *  *  *  *  *  * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 *  *  *  *  *  *  *  *  *  * 4. correct some HAL implementation
 *
 * 01 14 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * enable to connect to ad-hoc network
**  \main\maintrunk.MT6620WiFiDriver_Prj\32 2009-12-10 16:50:33 GMT mtk02752
**  code clean
**  \main\maintrunk.MT6620WiFiDriver_Prj\31 2009-12-03 09:58:39 GMT mtk02752
**  release pending OID and TX packet when SupriseRemoval is notified
**  \main\maintrunk.MT6620WiFiDriver_Prj\30 2009-12-02 22:06:03 GMT mtk02752
**  kalOidComplete() will decrease i4OidPendingCount
**  \main\maintrunk.MT6620WiFiDriver_Prj\29 2009-12-01 23:02:17 GMT mtk02752
**  remove unnecessary spinlock
**  \main\maintrunk.MT6620WiFiDriver_Prj\28 2009-12-01 22:56:03 GMT mtk02752
**  + refuse SendPacket when card is removed
**  + when card is removed surprisingly, clear TX/CMD queue
**  \main\maintrunk.MT6620WiFiDriver_Prj\27 2009-11-25 18:19:34 GMT mtk02752
**  simplify prGlueInfo->rWlanInfo initialization by filling with zero
**  \main\maintrunk.MT6620WiFiDriver_Prj\26 2009-11-24 19:53:30 GMT mtk02752
**  mpSendPacket() is not used in new data path
**  \main\maintrunk.MT6620WiFiDriver_Prj\25 2009-11-17 22:41:04 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\24 2009-11-16 21:42:18 GMT mtk02752
**  * rename wlanDestroyPacketPool to windowsDestroyPacketPool
**  - remove wlanQoSFrameClassifierAndPacketInfo
**  \main\maintrunk.MT6620WiFiDriver_Prj\23 2009-11-13 10:53:02 GMT mtk02752
**  for initialize/free wlaninfo
**  \main\maintrunk.MT6620WiFiDriver_Prj\22 2009-11-11 13:48:34 GMT mtk02752
**  rOidReqEvent is not used anymore
**  \main\maintrunk.MT6620WiFiDriver_Prj\21 2009-10-29 19:57:18 GMT mtk01084
**  modify for emulation
**  \main\maintrunk.MT6620WiFiDriver_Prj\20 2009-10-23 16:08:38 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\19 2009-10-02 14:16:42 GMT mtk01725
**  \main\maintrunk.MT6620WiFiDriver_Prj\18 2009-10-02 14:02:23 GMT mtk01725
**  \main\maintrunk.MT6620WiFiDriver_Prj\17 2009-09-09 17:26:18 GMT mtk01084
**  modify for DDK related functions
**  \main\maintrunk.MT6620WiFiDriver_Prj\16 2009-05-19 10:53:13 GMT mtk01461
**  Revise the order - Register INT must placed before nicEnableInterrupt
**  \main\maintrunk.MT6620WiFiDriver_Prj\15 2009-05-12 09:45:58 GMT mtk01461
**  Update mpReset() to handle pending OID
**  \main\maintrunk.MT6620WiFiDriver_Prj\14 2009-04-21 09:46:34 GMT mtk01461
**  Fix mpSendPackets() by adding GLUE_SPIN_LOCK_DECLARATION()
**  \main\maintrunk.MT6620WiFiDriver_Prj\13 2009-04-17 18:17:34 GMT mtk01426
**  Don't use dynamic memory allocate for debug message
**  \main\maintrunk.MT6620WiFiDriver_Prj\12 2009-04-16 09:40:41 GMT mtk01426
**  Fixed imageFileUnMapping() ASSERT issue
**  \main\maintrunk.MT6620WiFiDriver_Prj\11 2009-04-08 17:48:01 GMT mtk01084
**  modify the interface of downloading image from D3 to D0
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-04-08 17:03:59 GMT mtk01084
**  fix compiler issue
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-04-08 16:51:22 GMT mtk01084
**  Update for the image download part
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-04-03 15:01:26 GMT mtk01426
**  Enable TX flow
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-04-01 10:59:39 GMT mtk01461
**  Add NdisSetEvent() to mpSendPackets()
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-03-23 21:55:51 GMT mtk01461
**  Add code - mpSendPacket() from kalTxServiceThread and wlanQoSFrameClassifier()
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-03-17 10:42:32 GMT mtk01426
**  Move TxServiceThread to Kal layer
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-16 16:41:18 GMT mtk01461
**  Fix for coding rule - arSpinLock[]
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-16 09:12:06 GMT mtk01461
**  Update TX PATH API
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:36:49 GMT mtk01426
**  Init for develop
**
*/

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "config.h"
#include "gl_os.h"

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
#if DBG
UINT_8 aucDebugModule[DBG_MODULE_NUM];
UINT_32 u4DebugModule = 0;
#endif				/* DBG */

/*******************************************************************************
*                           P R I V A T E   D A T A
********************************************************************************
*/
#ifdef NDIS51_MINIPORT
static UINT_8 mpNdisMajorVersion = 5;
static UINT_8 mpNdisMinorVersion = 1;
#else
static UINT_8 mpNdisMajorVersion = 5;
static UINT_8 mpNdisMinorVersion;
#endif

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
BOOLEAN mpCheckForHang(NDIS_HANDLE miniportAdapterContext);

VOID
mpIsr(OUT PBOOLEAN interruptRecognized_p,
      OUT PBOOLEAN queueMiniportHandleInterrupt_p, IN NDIS_HANDLE miniportAdapterContext);

VOID mpHandleInterrupt(IN NDIS_HANDLE miniportAdapterContext);

NDIS_STATUS
mpInitialize(OUT PNDIS_STATUS prOpenErrorStatus,
	     OUT PUINT prSelectedMediumIndex,
	     IN PNDIS_MEDIUM prMediumArray,
	     IN UINT u4MediumArraySize,
	     IN NDIS_HANDLE rMiniportAdapterHandle, IN NDIS_HANDLE rWrapperConfigurationContext);

VOID
mpSendPackets(IN NDIS_HANDLE miniportAdapterContext,
	      IN PPNDIS_PACKET packetArray_p, IN UINT numberOfPackets);

VOID mpReturnPacket(IN NDIS_HANDLE miniportAdapterContext, IN PNDIS_PACKET prPacket);

NDIS_STATUS mpReset(OUT PBOOLEAN addressingReset_p, IN NDIS_HANDLE miniportAdapterContext);

VOID mpHalt(IN NDIS_HANDLE miniportAdapterContext);

NDIS_STATUS
mpQueryInformation(IN NDIS_HANDLE miniportAdapterContext,
		   IN NDIS_OID oid,
		   IN PVOID pvInfomationBuffer,
		   IN UINT_32 u4InformationBufferLength,
		   OUT PUINT_32 pu4ByteWritten, OUT PUINT_32 pu4ByteNeeded);

NDIS_STATUS
mpSetInformation(IN NDIS_HANDLE miniportAdapterContext,
		 IN NDIS_OID oid,
		 IN PVOID pvInfomationBuffer,
		 IN UINT_32 u4InformationBufferLength,
		 OUT PUINT_32 pu4ByteRead, OUT PUINT_32 pu4ByteNeeded);

VOID mpShutdown(IN PVOID shutdownContext);

P_GLUE_INFO_T windowsCreateGlue(NDIS_HANDLE rMiniportAdapterHandle, UINT_16 u2NdisVersion);

VOID mpFreeAdapterObject(IN P_GLUE_INFO_T prGlueInfo);

NDIS_STATUS
windowsReadRegistryParameters(IN P_GLUE_INFO_T prGlueInfo,
			      IN NDIS_HANDLE wrapperConfigurationContext);

UINT_32 windowsInitRxPacketPool(P_GLUE_INFO_T prGlueInfo, UINT_32 u4NumPkt, UINT_32 u4MaxPktSz);

#if DBG
BOOLEAN reqCheckOrderOfSupportedOids(IN PVOID pvAdapter);
#endif

#ifdef NDIS51_MINIPORT
VOID
mpPnPEventNotify(IN NDIS_HANDLE miniportAdapterContext,
		 IN NDIS_DEVICE_PNP_EVENT pnpEvent,
		 IN PVOID informationBuffer_p, IN UINT_32 informationBufferLength);

VOID mpShutdown(IN PVOID shutdownContext);
#endif				/* DBG */


/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief Driver entry. It is the first routine called after a driver is loaded,
*        and is responsible for initializing the driver
*
* \param[in] driverObject Caller-supplied pointer to a DRIVER_OBJECT structure.
*                         This is the driver's driver object.
* \param[in] registryPath Pointer to a counted Unicode string specifying the
*                         path to the driver's registry key.
*
* \return If the routine succeeds, it must return STATUS_SUCCESS.
*         Otherwise, it must return one of the error status values defined
*         in ntstatus.h.
*/
/*----------------------------------------------------------------------------*/
NTSTATUS DriverEntry(IN PDRIVER_OBJECT driverObject, IN PUNICODE_STRING registryPath)
{
	NDIS_STATUS status;
	NDIS_HANDLE ndisWrapperHandle;
	NDIS_MINIPORT_CHARACTERISTICS mpChar;
#if DBG
	INT i;
#endif

	DEBUGFUNC("DriverEntry");

#if DBG
	/* Initialize debug class of each module */
	for (i = 0; i < DBG_MODULE_NUM; i++) {
		aucDebugModule[i] = DBG_CLASS_ERROR |
		    DBG_CLASS_WARN | DBG_CLASS_STATE | DBG_CLASS_EVENT;
	}
	aucDebugModule[DBG_INIT_IDX] |= DBG_CLASS_TRACE | DBG_CLASS_INFO;
/* aucDebugModule[DBG_TX_IDX] |= DBG_CLASS_TRACE | DBG_CLASS_INFO; */
	aucDebugModule[DBG_RX_IDX] |= DBG_CLASS_TRACE | DBG_CLASS_INFO;
	aucDebugModule[DBG_RFTEST_IDX] |= DBG_CLASS_TRACE | DBG_CLASS_INFO;

	aucDebugModule[DBG_REQ_IDX] &= ~DBG_CLASS_WARN;

	aucDebugModule[DBG_EMU_IDX] |= DBG_CLASS_TRACE | DBG_CLASS_INFO;
/* aucDebugModule[DBG_TX_IDX] |= DBG_CLASS_TRACE; */
	aucDebugModule[DBG_TX_IDX] &= ~DBG_CLASS_EVENT;
	aucDebugModule[DBG_RX_IDX] &= ~DBG_CLASS_EVENT;

#endif				/* DBG */

	DBGLOG(INIT, TRACE, ("\n"));
	DBGLOG(INIT, TRACE, ("DriverEntry: Driver object @0x%p\n", driverObject));

	/* Now we must initialize the wrapper, and then register the Miniport */
	NdisMInitializeWrapper(&ndisWrapperHandle, driverObject, registryPath, NULL);

	if (ndisWrapperHandle == NULL) {
		status = NDIS_STATUS_FAILURE;

		DBGLOG(INIT, ERROR, ("Init wrapper ==> FAILED (status=0x%x)\n", status));
		return status;
	}

	NdisZeroMemory(&mpChar, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

	/* Initialize the Miniport characteristics for the call to
	   NdisMRegisterMiniport. */
	mpChar.MajorNdisVersion = mpNdisMajorVersion;
	mpChar.MinorNdisVersion = mpNdisMinorVersion;
	mpChar.CheckForHangHandler = NULL;	/* mpCheckForHang; */
	mpChar.DisableInterruptHandler = NULL;
	mpChar.EnableInterruptHandler = NULL;
	mpChar.HaltHandler = mpHalt;
	mpChar.HandleInterruptHandler = mpHandleInterrupt;
	mpChar.InitializeHandler = mpInitialize;
	mpChar.ISRHandler = mpIsr;
	mpChar.QueryInformationHandler = mpQueryInformation;
	/*mpChar.ReconfigureHandler      = NULL; */
	mpChar.ResetHandler = mpReset;
	mpChar.SetInformationHandler = mpSetInformation;
	mpChar.SendHandler = NULL;
	mpChar.SendPacketsHandler = mpSendPackets;
	mpChar.ReturnPacketHandler = mpReturnPacket;
	mpChar.TransferDataHandler = NULL;
	mpChar.AllocateCompleteHandler = NULL;
#ifdef NDIS51_MINIPORT
	mpChar.CancelSendPacketsHandler = NULL;
	/*mpChar.CancelSendPacketsHandler = MPCancelSendPackets; */
	mpChar.PnPEventNotifyHandler = mpPnPEventNotify;
	mpChar.AdapterShutdownHandler = mpShutdown;
#endif

	/* Register this driver to use the NDIS library of version the same as
	   the default setting of the build environment. */
	status = NdisMRegisterMiniport(ndisWrapperHandle,
				       &mpChar, sizeof(NDIS_MINIPORT_CHARACTERISTICS));

	DBGLOG(INIT, TRACE, ("NdisMRegisterMiniport (NDIS %d.%d) returns 0x%x\n",
			     mpNdisMajorVersion, mpNdisMinorVersion, status));

#ifndef _WIN64
#ifdef NDIS51_MINIPORT
	/* If the current platform cannot support NDIS 5.1, we attempt to declare
	   ourselves as an NDIS 5.0 miniport driver. */
	if (status == NDIS_STATUS_BAD_VERSION) {
		mpNdisMinorVersion = 0;
		mpChar.MinorNdisVersion = 0;
		/* Register this driver to use the NDIS 5.0 library. */
		status = NdisMRegisterMiniport(ndisWrapperHandle, &mpChar,
					       sizeof(NDIS50_MINIPORT_CHARACTERISTICS));

		DBGLOG(INIT, TRACE, ("NdisMRegisterMiniport (NDIS %d.%d) returns 0x%x\n",
				     mpNdisMajorVersion, mpNdisMinorVersion, status));
	}
#endif
#endif

	if (status != NDIS_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR, ("Register NDIS %d.%d miniport ==> FAILED (status=0x%x)\n",
				     mpNdisMajorVersion, mpNdisMinorVersion, status));

		NdisTerminateWrapper(ndisWrapperHandle, NULL);

		return status;
	}

	return STATUS_SUCCESS;
}				/* DriverEntry */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function if the driver's network adapter
*        generates interrupts. Note the spin lock is needed for eHPI interface
*        and spin lock is permitted only for WinCE OS.
*
* \param[out] interruptRecognized_p          Follow MSDN definition.
* \param[out] queueMiniportHandleInterrupt_p Follow MSDN definition.
* \param[in]  miniportAdapterContext         Follow MSDN definition.
*
* \return none
*/
/*----------------------------------------------------------------------------*/
VOID
mpIsr(OUT PBOOLEAN interruptRecognized_p,
      OUT PBOOLEAN queueMiniportHandleInterrupt_p, IN NDIS_HANDLE miniportAdapterContext)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;

	if (wlanISR(prGlueInfo->prAdapter, TRUE)) {
		*interruptRecognized_p = TRUE;
		*queueMiniportHandleInterrupt_p = TRUE;
	} else {
		*interruptRecognized_p = FALSE;
		*queueMiniportHandleInterrupt_p = FALSE;
	}

}				/* mpIsr */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function if a driver's network adapter
*        generates interrupts. This function does the deferred processing of
*        all outstanding interrupt operations
*
* \param[in] miniportAdapterContext Registried GLUE data structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID mpHandleInterrupt(IN NDIS_HANDLE miniportAdapterContext)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;

	wlanIST(prGlueInfo->prAdapter);

}				/* mpHandleInterrupt */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function that sets up a network adapter,
*        or virtual network adapter, for network I/O operations, claims all
*        hardware resources necessary to the network adapter in the registry,
*        and allocates resources the driver needs to carry out network I/O
	 operations.
*
* \param[out] prOpenErrorStatus              Follow MSDN definition.
* \param[out] prSelectedMediumIndex          Follow MSDN definition.
* \param[in]  prMediumArray                  Follow MSDN definition.
* \param[in]  u4MediumArraySize              Follow MSDN definition.
* \param[in]  rMiniportAdapterHandle         Follow MSDN definition.
* \param[in]  rWrapperConfigurationContext   Follow MSDN definition.
*
* \retval NDIS_STATUS_SUCCESS   success
* \retval others                fail for some reasons.
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
mpInitialize(OUT PNDIS_STATUS prOpenErrorStatus,
	     OUT PUINT prSelectedMediumIndex,
	     IN PNDIS_MEDIUM prMediumArray,
	     IN UINT u4MediumArraySize,
	     IN NDIS_HANDLE rMiniportAdapterHandle, IN NDIS_HANDLE rWrapperConfigurationContext)
{
	P_ADAPTER_T prAdapter = NULL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	UINT i;
	NDIS_STATUS status;
#if DBG
	CHAR companyName[] = NIC_VENDOR;
	CHAR productName[] = NIC_PRODUCT_NAME;
	CHAR driverVersion[] = NIC_DRIVER_VERSION_STRING;
#endif
	UINT_8 desc[] = NIC_DRIVER_NAME;
	PVOID pvFwImageMapFile = NULL;
	NDIS_HANDLE rFileHandleFwImg = NULL;
	NDIS_STRING rFileWifiRam;
	UINT_32 u4FwImageFileLength = 0;
#if defined(WINDOWS_DDK)
	NTSTATUS rStatus = 0;
#endif


	DEBUGFUNC("MPInitialize");
	DBGLOG(INIT, TRACE, ("\n"));
	DBGPRINTF("MPInitialize() / Current IRQL = %d\n", KeGetCurrentIrql());

	DBGLOG(INIT, TRACE, ("%s\n", productName));
	DBGLOG(INIT, TRACE, ("(C) Copyright 2002-2007 %s\n", companyName));

	DBGLOG(INIT, TRACE, ("Version %s (NDIS 5.1/5.0 Checked Build)\n", driverVersion));

	DBGLOG(INIT, TRACE, ("***** BUILD TIME: %s %s *****\n", __DATE__, __TIME__));
	DBGLOG(INIT, TRACE, ("***** Current Platform: NDIS %d.%d *****\n",
			     mpNdisMajorVersion, mpNdisMinorVersion));

	do {
		/* Find the media type we support. */
		for (i = 0; i < u4MediumArraySize; i++) {
			if (prMediumArray[i] == NIC_MEDIA_TYPE) {
				break;
			}
		}

		if (i == u4MediumArraySize) {
			DBGLOG(INIT, ERROR, ("Supported media type ==> Not found\n"));
			status = NDIS_STATUS_UNSUPPORTED_MEDIA;
			break;
		}

		/* Select ethernet (802.3). */
		*prSelectedMediumIndex = i;

		/* Allocate OS glue object */
		prGlueInfo = windowsCreateGlue(rMiniportAdapterHandle,
					       (UINT_16) ((UINT_16) mpNdisMajorVersion * 0x100 +
							  (UINT_16) mpNdisMinorVersion));
		if (prGlueInfo == NULL) {
			status = WLAN_STATUS_FAILURE;
			break;
		}


		prGlueInfo->ucDriverDescLen = (UINT_8) strlen(desc) + 1;
		if (prGlueInfo->ucDriverDescLen >= sizeof(prGlueInfo->aucDriverDesc)) {
			prGlueInfo->ucDriverDescLen = sizeof(prGlueInfo->aucDriverDesc);
		}
		strncpy(prGlueInfo->aucDriverDesc, desc, prGlueInfo->ucDriverDescLen);
		prGlueInfo->aucDriverDesc[prGlueInfo->ucDriverDescLen - 1] = '\0';


		prGlueInfo->eParamMediaStateIndicated = PARAM_MEDIA_STATE_DISCONNECTED;
		prGlueInfo->fgIsCardRemoved = FALSE;

#if CFG_SUPPORT_SDIO_READ_WRITE_PATTERN
		/* initialize SDIO read-write pattern control */
		prGlueInfo->fgEnSdioTestPattern = FALSE;
		prGlueInfo->fgIsSdioTestInitialized = FALSE;
#endif

		/* Allocate adapter object */
		prAdapter = wlanAdapterCreate(prGlueInfo);
		if (prAdapter == NULL) {
			status = WLAN_STATUS_FAILURE;
			break;
		}

		DBGLOG(INIT, TRACE, ("Adapter structure pointer @0x%p\n", prAdapter));

		/* link glue info and adapter with each other */
		prGlueInfo->prAdapter = prAdapter;

		/* Read the registry parameters. */
		kalMemZero(&(prGlueInfo->rRegInfo), sizeof(REG_INFO_T));
		status = windowsReadRegistryParameters(prGlueInfo, rWrapperConfigurationContext);
		DBGPRINTF("windowsReadRegistryParameters() = %08x\n", status);
		if (status != NDIS_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR,
			       ("Read registry parameters FAILED (status=0x%x)\n", status));
			break;
		}
		DBGLOG(INIT, TRACE, ("Read registry parameters -- OK\n"));

		/* Inform NDIS of the attributes of our adapter.
		   This has to be done before calling NdisMRegisterXxx or NdisXxxx
		   function that depends on the information supplied to
		   NdisMSetAttributesEx.
		   e.g. NdisMAllocateMapRegisters  */
		NdisMSetAttributesEx(rMiniportAdapterHandle,
				     (NDIS_HANDLE) prGlueInfo,
				     0, (ULONG) NIC_ATTRIBUTE, NIC_INTERFACE_TYPE);
		DBGLOG(INIT, TRACE, ("Set attributes -- OK\n"));

		/* initialize timer for OID timeout checker */
		kalOsTimerInitialize(prGlueInfo, kalTimeoutHandler);

		/* Allocate SPIN LOCKs */
		for (i = 0; i < SPIN_LOCK_NUM; i++) {
			NdisAllocateSpinLock(&(prGlueInfo->arSpinLock[i]));
		}
		GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_SPIN_LOCK);

		/* Setup Packet pool for Rx frame indication packets */
		status = windowsInitRxPacketPool(prGlueInfo,
						 CFG_RX_MAX_PKT_NUM, CFG_RX_MAX_PKT_SIZE);
		DBGPRINTF("windowsInitRxPacketPool() = %08x\n", status);

		if (status != NDIS_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, ("Allocate Rx packet pool ==> FAILED (status=0x%x)\n",
					     status));
			break;
		}

		DBGLOG(INIT, TRACE, ("Init packet pool -- OK\n"));

		NdisInitializeEvent(&prGlueInfo->rTxReqEvent);

		/* initialize remaining adapter members and initialize the card */
		status = windowsFindAdapter(prGlueInfo, rWrapperConfigurationContext);
		DBGPRINTF("windowsFindAdapter() = %08x\n", status);

		if (status != NDIS_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, ("Cannot find the adapter\n"));
			break;
		}
		DBGLOG(INIT, TRACE, ("Adapter Found -- OK\n"));


		/* Register a shutdown handler for NDIS50 or earlier miniports.
		   For NDIS 5.1 miniports, set AdapterShutdownHandler as shown
		   above. */
		if (prGlueInfo->u2NdisVersion == 0x500) {
			NdisMRegisterAdapterShutdownHandler(prGlueInfo->rMiniportAdapterHandle,
							    (PVOID) prAdapter,
							    (ADAPTER_SHUTDOWN_HANDLER) mpShutdown);
		}

		/* Register interrupt handler to OS. */
		windowsRegisterIsrt(prGlueInfo);
		DBGLOG(INIT, TRACE, ("Register interrupt handler -- OK\n"));

#if CFG_ENABLE_FW_DOWNLOAD
		/* Mapping FW image to be downloaded from file */
#if defined(MT6620) && CFG_MULTI_ECOVER_SUPPORT
		if (wlanGetEcoVersion(prAdapter) >= 6) {
			NdisInitUnicodeString(&rFileWifiRam,
					      prGlueInfo->rRegInfo.aucFwImgFilenameE6);
		} else {
			NdisInitUnicodeString(&rFileWifiRam, prGlueInfo->rRegInfo.aucFwImgFilename);
		}
#else
		NdisInitUnicodeString(&rFileWifiRam, prGlueInfo->rRegInfo.aucFwImgFilename);
#endif
		if (!imageFileMapping
		    (rFileWifiRam, &rFileHandleFwImg, &pvFwImageMapFile, &u4FwImageFileLength)) {
			DBGLOG(INIT, ERROR, ("Fail to load FW image from file!\n"));
		}
#endif

		/* Start adapter */
		status = wlanAdapterStart((PVOID) prAdapter,
					  &prGlueInfo->rRegInfo,
					  pvFwImageMapFile, u4FwImageFileLength);
		DBGPRINTF("wlanAdapterStart() = %08x\n", status);

#if CFG_ENABLE_FW_DOWNLOAD
		/* Un-Mapping FW image from file */
		imageFileUnMapping(rFileHandleFwImg, pvFwImageMapFile);
#endif

		if (status != NDIS_STATUS_SUCCESS) {
			DBGLOG(INIT, ERROR, ("Start adapter ==> FAILED (status=0x%x)\n", status));
			break;
		}

		/*Create Event, Threads for Windows SDIO driver */
		/*20091011: initialize rOidReqEvent earlier than kalTxServiceThread creation */
		KeInitializeEvent(&prGlueInfo->rHifInfo.rOidReqEvent, SynchronizationEvent, FALSE);

		/* Create Tx Service System Thread */
#if defined(WINDOWS_DDK)
		KAL_CREATE_THREAD(prGlueInfo->hTxService, kalTxServiceThread, prGlueInfo,
				  &prGlueInfo->pvKThread);
#else
		KAL_CREATE_THREAD(prGlueInfo->hTxService, kalTxServiceThread, prGlueInfo);
#endif
		if (prGlueInfo->hTxService) {
			DBGLOG(INIT, TRACE, ("Thread has been created successfully\n"));

			KeWaitForSingleObject(&prGlueInfo->rHifInfo.rOidReqEvent, Executive, KernelMode, FALSE, NULL);	/* NULL, wait endless */
		} else {
			DBGLOG(INIT, WARN, ("CreateThread Failed\n"));
			break;
		}
		DBGLOG(INIT, TRACE, ("...done\n"));

		GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_WLAN_PROBE);
		DBGLOG(INIT, TRACE, ("Start adapter -- OK\n"));

#if DBG
		/* Check the order of supported OIDs. */
		reqCheckOrderOfSupportedOids(prAdapter);
#endif


	} while (FALSE);

	DBGPRINTF("MPInitialize Completed: %08X (%d)\n", status, prGlueInfo->fgIsCardRemoved);

	if (prAdapter && status != NDIS_STATUS_SUCCESS) {
		/* Undo everything if it failed. */
		GLUE_DEC_REF_CNT(prGlueInfo->exitRefCount);
		mpFreeAdapterObject(prGlueInfo);
		return status;
	}

	return status;
}				/* mpInitialize */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is a required function in drivers that indicate receives
*        with NdisMIndicateReceivePacket.
*
* \param[in] miniportAdapterContext Follow MSDN definition.
* \param[in] prPacket               Follow MSDN definition.
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID mpReturnPacket(IN NDIS_HANDLE miniportAdapterContext, IN PNDIS_PACKET prPacket)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;
	KIRQL currentIrql;
	P_QUE_ENTRY_T prQueEentry;

	DEBUGFUNC("mpReturnPacket");

	GLUE_SPIN_LOCK_DECLARATION();


	ASSERT(miniportAdapterContext);
	ASSERT(prPacket);

	/* DBGLOG(RX, TRACE, ("\n")); */

	currentIrql = KeGetCurrentIrql();

	prQueEentry = (P_QUE_ENTRY_T) MP_GET_PKT_MR(prPacket);

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_RX_RETURN_QUE);
	QUEUE_INSERT_TAIL(&prGlueInfo->rReturnQueue, prQueEentry);
	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_RX_RETURN_QUE);

	/* 4 The Pending Count should > 0. */
	ASSERT(prGlueInfo->i4RxPendingFrameNum > 0);
	InterlockedDecrement(&prGlueInfo->i4RxPendingFrameNum);

	GLUE_SET_EVENT(prGlueInfo);

	ASSERT(currentIrql == KeGetCurrentIrql());

	return;
}				/* end of mpReturnPacket() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function transfers some number of packets, specified as an
*        array of packet pointers, over the network.
*
* \param[in] miniportAdapterContext Follow MSDN definition.
* \param[in] packetArray_p          Follow MSDN definition.
* \param[in] numberOfPackets        Follow MSDN definition.
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID
mpSendPackets(IN NDIS_HANDLE miniportAdapterContext,
	      IN PPNDIS_PACKET packetArray_p, IN UINT numberOfPackets)
{
	P_GLUE_INFO_T prGlueInfo;
	PNDIS_PACKET prNdisPacket;
	P_QUE_ENTRY_T prQueueEntry;
	P_QUE_T prTxQue;
	UINT i;
	UINT_8 ucBssIndex;

	GLUE_SPIN_LOCK_DECLARATION();

	DEBUGFUNC("mpSendPackets");

	ASSERT(miniportAdapterContext);
	prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;
	prTxQue = &prGlueInfo->rTxQueue;

	ucBssIndex = wlanGetAisBssIndex(prGlueInfo->prAdapter);

	for (i = 0; i < numberOfPackets; i++) {
		prNdisPacket = packetArray_p[i];

		if (prGlueInfo->fgIsCardRemoved) {
			NdisMSendComplete(prGlueInfo->rMiniportAdapterHandle,
					  prNdisPacket, NDIS_STATUS_NOT_ACCEPTED);
			continue;
		}
		if (wlanQueryTestMode(prGlueInfo->prAdapter) == TRUE) {
			NdisMSendComplete(prGlueInfo->rMiniportAdapterHandle,
					  prNdisPacket, NDIS_STATUS_NETWORK_UNREACHABLE);
			continue;
		}

		NDIS_SET_PACKET_STATUS(prNdisPacket, NDIS_STATUS_PENDING);

		GLUE_CLEAR_PKT_RSVD(prNdisPacket);

		GLUE_SET_PKT_BSS_IDX(prNdisPacket, ucBssIndex);

		if (wlanProcessSecurityFrame(prGlueInfo->prAdapter, (P_NATIVE_PACKET) prNdisPacket)
		    == FALSE) {
			prQueueEntry = (P_QUE_ENTRY_T) GLUE_GET_PKT_QUEUE_ENTRY(prNdisPacket);

			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);
			QUEUE_INSERT_TAIL(prTxQue, prQueueEntry);
			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_TX_QUE);

			GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingFrameNum);
		} else {
			GLUE_INC_REF_CNT(prGlueInfo->i4TxPendingSecurityFrameNum);
		}
	}

	/* Set EVENT */
	NdisSetEvent(&prGlueInfo->rTxReqEvent);

	return;

}				/* mpSendPackets */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function de-allocates resources when the network adapter is
*        removed and halts the network adapter.
*
* \param[in] miniportAdapterContext Follow MSDN definition.
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID mpHalt(IN NDIS_HANDLE miniportAdapterContext)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;

	DEBUGFUNC("MPHalt");

	INITLOG(("\n"));
	DBGPRINTF("MPHalt (Card Removed: %d)\n", prGlueInfo->fgIsCardRemoved);

	/* WinCE didn't has InterlockedOr() */
	GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_HALT);

	kalCancelTimer(prGlueInfo);

#if defined(WINDOWS_DDK)
	KAL_KILL_THREAD(&prGlueInfo->rTxReqEvent, prGlueInfo->pvKThread);
	prGlueInfo->hTxService = NULL;
#else
	/* Notify TxServiceThread to terminate it. */
	NdisSetEvent(&prGlueInfo->rTxReqEvent);

	DBGLOG(INIT, TRACE, ("Notify TxServiceThread to terminate it\n"));

	if (prGlueInfo->hTxService) {
		DBGLOG(INIT, TRACE,
		       ("KAL_WAIT_FOR_SINGLE_OBJECT (0x%x)\n", prGlueInfo->hTxService));
		KAL_WAIT_FOR_SINGLE_OBJECT(prGlueInfo->hTxService);

		KAL_CLOSE_HANDLE(prGlueInfo->hTxService);
		prGlueInfo->hTxService = NULL;
	}
#endif

#if defined(WINDOWS_CE)
	NdisFreeEvent(&prGlueInfo->rTxReqEvent);
#endif

	while (prGlueInfo->i4RxPendingFrameNum > 0) {
		/* Wait for MPReturnPacket to return indicated packets */
		/* DbgPrint("Wait for RX return, left = %d\n", prGlueInfo->u4RxPendingFrameNum); */
		NdisMSleep(100000);	/* Unit in microseconds. Sleep 100ms */
	}

	wlanAdapterStop(prGlueInfo->prAdapter);
	DBGLOG(INIT, TRACE, ("wlanAdapterStop\n"));


	/* For NDIS 5.0 driver, deregister shutdown handler because we are halting
	   now. */
#ifdef NDIS51_MINIPORT
	if (prGlueInfo->u2NdisVersion == 0x500)
#endif
	{
		NdisMDeregisterAdapterShutdownHandler(prGlueInfo->rMiniportAdapterHandle);

		INITLOG(("Shutdown handler deregistered\n"));
	}
#if defined(WINDOWS_DDK) && defined(_HIF_SDIO)
	/* According to MSDN, we should dereference count after receiving removal.
	 * If we didn't do this, disable/enable device fails. However, if we did
	 * dereference the INTerface and we use continually packet transmission,
	 * driver fault happens after it's disabled.
	 */
	if (prGlueInfo->rHifInfo.dx.BusInterface.InterfaceDereference &&
	    prGlueInfo->rHifInfo.dx.busInited) {
		(prGlueInfo->rHifInfo.dx.BusInterface.InterfaceDereference)
		    (prGlueInfo->rHifInfo.dx.BusInterface.Context);
		RtlZeroMemory(&prGlueInfo->rHifInfo.dx.BusInterface,
			      sizeof(SDBUS_INTERFACE_STANDARD));
		prGlueInfo->rHifInfo.dx.busInited = FALSE;
	}
#endif

	/* Free the entire adapter object, including the shared memory
	   structures. */
	mpFreeAdapterObject((PVOID) prGlueInfo);

	INITLOG(("Halt handler -- Completed\n"));

}				/* mpHalt */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function issues a hardware reset to the network adapter
*        and/or resets the driver's software state.
*        (Now not supported yet!)
*
* \param[out] addressingReset_p         Follow MSDN definition.
* \param[in]  miniportAdapterContext    Follow MSDN definition.
*
* \retval NDIS_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS mpReset(OUT PBOOLEAN addressingReset_p, IN NDIS_HANDLE miniportAdapterContext)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;


	ASSERT(prGlueInfo);

	DBGLOG(INIT, ERROR, ("mpReset()\n"));

	if ((prGlueInfo->i4OidPendingCount)
	    || (prGlueInfo->i4TxPendingFrameNum)
	    || (prGlueInfo->i4TxPendingSecurityFrameNum)) {

		/* WinCE didn't has InterlockedOr() */
		GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_RESET);

		/* Notify TxServiceThread to terminate it. */
		GLUE_SET_EVENT(prGlueInfo);

		return NDIS_STATUS_PENDING;
	} else {
		return NDIS_STATUS_SUCCESS;
	}

}				/* end of mpReset() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function will free all resources, which are allocated in function
*        windowsInitRxPacketPool().
*
* \param[in] prGlueInfo     Pointer to the glue structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID windowsDestroyPacketPool(P_GLUE_INFO_T prGlueInfo)
{
	UINT_32 i;
	PNDIS_PACKET prPktDscr;	/* Packet descriptor */

	DEBUGFUNC("windowsDestroyPacketPool");

	/* Free packet descriptors */
	if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_PKT_DESCR)) {

		for (i = 0; i < prGlueInfo->u4PktPoolSz; i++) {
			prPktDscr = getPoolPacket(prGlueInfo);

			if (prPktDscr == NULL) {
				ASSERT(prPktDscr);
				break;
			}
			NdisFreePacket(prPktDscr);
		}
		GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_PKT_DESCR);
	}
#if DBG
	do {
		UINT_32 count;

		/* Assert if our pool isn't empty. */
		count = NdisPacketPoolUsage(prGlueInfo->hPktPool);
		ASSERT(count == 0);
	} while (0);
#endif

	/* Free buffer descriptor pool */
	if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_BUF_POOL)) {
		NdisFreeBufferPool(prGlueInfo->hBufPool);
		GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_BUF_POOL);
	}

	/* Free packet descriptor pool */
	if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_PKT_POOL)) {
		NdisFreePacketPool(prGlueInfo->hPktPool);
		GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_PKT_POOL);
	}

	/* Free payload buffer pool */
	if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_PAYLOAD_POOL)) {
		kalMemFree(prGlueInfo->pucPayloadPool, VIR_MEM_TYPE, prGlueInfo->u4PayloadPoolSz);
		GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_PAYLOAD_POOL);
	}

	return;
}				/* windowsDestroyPacketPool */

/*----------------------------------------------------------------------------*/
/*!
* \brief This function free GLUE_INFO_T memory, which is created in
*        function windowsCreateGlue().
*
* \param[in] prGlueInfo     The start address of GLUE_INFO_T memory
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID windowsDestroyGlue(IN P_GLUE_INFO_T prGlueInfo)
{
	if (prGlueInfo) {
		NdisFreeMemory(prGlueInfo, sizeof(GLUE_INFO_T), 0);
	}

	return;
}				/* windowsDestroyGlue */



/*----------------------------------------------------------------------------*/
/*!
* \brief This routine releases all resources defined in the
**       ADAPTER object and returns the ADAPTER object memory to
**       the free pool.
*
* \param[in] pvAdapter Pointer to the Adapter structure
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID mpFreeAdapterObject(IN P_GLUE_INFO_T prGlueInfo)
{
	UINT i;

	DEBUGFUNC("mpFreeAdapterObject");

	INITLOG(("\n"));

	if (prGlueInfo == NULL) {
		return;
	}

	if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_WLAN_PROBE)) {
		GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_WLAN_PROBE);
		INITLOG(("Hardware stopped\n"));
	}

	/* Release interrupt resources. */
	windowsUnregisterIsrt(prGlueInfo);

#if CFG_SDIO_PATHRU_MODE
	/* Disable PATHRU after unregister ISR/T */
	sdioEnablePathruMode(prGlueInfo, FALSE);
	/* Deinit PATHRU, free allocated resources */
	sdioDeinitPathruMode(prGlueInfo);
#endif

#if defined(_HIF_SDIO) && defined(WINDOWS_CE)
	sdioBusDeinit(prGlueInfo);
#endif

	windowsUMapFreeRegister(prGlueInfo);

#if defined(WINDOWS_DDK) && defined(_HIF_SDIO)
	/* According to MSDN, we should dereference count after receiving removal.
	 * If we didn't do this, disable/enable device fails. However, if we did
	 * dereference the INTerface and we use continually packet transmission,
	 * driver fault happens after it's disabled.
	 */
	if (prGlueInfo->rHifInfo.dx.BusInterface.InterfaceDereference &&
	    prGlueInfo->rHifInfo.dx.busInited) {
		(prGlueInfo->rHifInfo.dx.BusInterface.InterfaceDereference)
		    (prGlueInfo->rHifInfo.dx.BusInterface.Context);
		RtlZeroMemory(&prGlueInfo->rHifInfo.dx.BusInterface,
			      sizeof(SDBUS_INTERFACE_STANDARD));
		prGlueInfo->rHifInfo.dx.busInited = FALSE;
	}
#endif

	/* Before we destroy Packet pool, we have recover TFCB and RFB for all
	 * outstanding driver packets
	 */
	if (prGlueInfo->pvPktDescrHead) {
		windowsDestroyPacketPool(prGlueInfo);
	}

	if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_SPIN_LOCK)) {
		for (i = 0; i < SPIN_LOCK_NUM; i++) {
			NdisFreeSpinLock(&prGlueInfo->arSpinLock[i]);
		}
	}
	INITLOG(("Spin lock freed\n"));

	wlanAdapterDestroy(prGlueInfo->prAdapter);
	prGlueInfo->prAdapter = NULL;

	windowsDestroyGlue(prGlueInfo);

}				/* mpFreeAdapterObject */



/*----------------------------------------------------------------------------*/
/*!
* \brief This function allocates GLUE_INFO_T memory and its initialized
*        procedure.
*
* \param[in] rMiniportAdapterHandle Windows provided adapter handle
* \param[in] u2NdisVersion      16-bits value with major[15:8].minor[7:0] format
*                               for NDIS version expression
*
* \return Virtual address of allocated GLUE_INFO_T strucutre
*/
/*----------------------------------------------------------------------------*/
P_GLUE_INFO_T windowsCreateGlue(IN NDIS_HANDLE rMiniportAdapterHandle, IN UINT_16 u2NdisVersion)
{
	NDIS_STATUS status;
	P_GLUE_INFO_T prGlueInfo;

	DEBUGFUNC("windowsCreateGlue");

	/* Setup private information of glue layer for this adapter */
	status = NdisAllocateMemoryWithTag((PVOID *) &prGlueInfo,
					   sizeof(GLUE_INFO_T), NIC_MEM_TAG);
	if (status != NDIS_STATUS_SUCCESS) {
		return NULL;
	}

	NdisZeroMemory(prGlueInfo, sizeof(GLUE_INFO_T));

	/* set miniport handler supplied by OS */
	prGlueInfo->rMiniportAdapterHandle = rMiniportAdapterHandle;

	GLUE_INC_REF_CNT(prGlueInfo->exitRefCount);

	prGlueInfo->u2NdisVersion = u2NdisVersion;

	/* Initialize the structure used to query and set
	   OID_802_11_ASSOCIATION_INFORMATION. */
	prGlueInfo->rNdisAssocInfo.Length = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);
	prGlueInfo->rNdisAssocInfo.OffsetRequestIEs = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);
	prGlueInfo->rNdisAssocInfo.OffsetResponseIEs = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);

	return prGlueInfo;
}				/* windowsCreateGlue */



/*----------------------------------------------------------------------------*/
/*!
* \brief This function is to allocate necessary payload buffer,
*        packet descriptors and Mbuf descriptors. Call windowsDestroyPacketPool()
*        to free these allocated memory.
*
* \param[in] prGlueInfo     Pointer of GLUE Data Structure
* \param[in] u4NumPkt       Need packet numbers
* \param[in] u4MaxPktSz     Packet payload size of each RX packet
*
* \retval 0         success
* \retval others    fail
*/
/*----------------------------------------------------------------------------*/
UINT_32 windowsInitRxPacketPool(P_GLUE_INFO_T prGlueInfo, UINT_32 u4NumPkt, UINT_32 u4MaxPktSz)
{
	UINT_32 u4TotalPayloadBufSz;	/* Total payload buffer size */
	PVOID prBufVirAddr;	/* Virtual address */
	PUINT_8 prPktBufStartAddr;
	PNDIS_PACKET prPktDescriptor;
	NDIS_STATUS status;
	UINT_32 i;

	DEBUGFUNC("windowsInitRxPacketPool");

	/* Allocate payload buffer pool, this should be multiple of 4. The payload
	   buffer pool is the actual memory space to store the Rx frame, and
	   indicated packet to OS */
#if DBG
	if (IS_NOT_ALIGN_4(u4MaxPktSz)) {
		/* If the packet size is not multiple of 4, just assert */
		ASSERT(FALSE);
	}
#endif
	u4TotalPayloadBufSz = u4NumPkt * u4MaxPktSz;

	prBufVirAddr = kalMemAlloc(u4TotalPayloadBufSz, VIR_MEM_TYPE);

	if (prBufVirAddr == NULL) {
		DBGLOG(INIT, ERROR, ("Could not allocate the Rx payload buffer: %d (bytes)\n",
				     u4TotalPayloadBufSz));
		return ERRLOG_OUT_OF_SHARED_MEMORY;
	}

	prGlueInfo->pucPayloadPool = prBufVirAddr;
	prGlueInfo->u4PayloadPoolSz = u4TotalPayloadBufSz;

	GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_PAYLOAD_POOL);

	/* Allocate packet descriptor pool:
	   Set up a pool of data for us to build our packet array out of
	   for indicating groups of packets to NDIS.
	   This could be quite the memory hog, but makes management
	   of the pointers associated with Asynchronous memory allocation
	   easier. */
	NdisAllocatePacketPoolEx(&status,
				 &prGlueInfo->hPktPool,
				 (UINT) u4NumPkt, (UINT) u4NumPkt, 4 * sizeof(PVOID));

	if (status != NDIS_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       ("Could not allocate a Rx packet pool of %d descriptors (max. %d)\n",
			u4NumPkt, u4NumPkt));
		return ERRLOG_OUT_OF_PACKET_POOL;
	}

	GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_PKT_POOL);

	DBGLOG(INIT, TRACE, ("Allocate Rx packet pool of %d descriptors\n", u4NumPkt));

	/* Allocate packet descriptors and link the them with payload buffer into
	   packet pool */
	prPktBufStartAddr = (PUINT_8) prBufVirAddr;
	for (i = 0; i < u4NumPkt; i++) {
		NdisAllocatePacket(&status, &prPktDescriptor, prGlueInfo->hPktPool);

		if (status != NDIS_STATUS_SUCCESS) {
			break;
		}

		putPoolPacket(prGlueInfo, prPktDescriptor, prPktBufStartAddr);

		prPktBufStartAddr += u4MaxPktSz;
	}

	if (i == 0) {
		DBGLOG(INIT, ERROR, ("Fail to allocate any packet descriptor\n"));
		return ERRLOG_OUT_OF_PACKET_POOL;
	}

	if (i != u4NumPkt) {
		DBGLOG(INIT, WARN, ("Only %d packet descriptors are allocated. (%d should be "
				    "allocated)\n", i, u4NumPkt));
	}

	prGlueInfo->u4PktPoolSz = i;

	GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_PKT_DESCR);

	DBGLOG(INIT, TRACE, ("Allocate %d descriptors\n", prGlueInfo->u4PktPoolSz));

	/* Allocate buffer descriptor pool:
	   We will at most have 1 per packet, so just allocate as
	   many buffers as we have packets. */
	NdisAllocateBufferPool(&status, &prGlueInfo->hBufPool, (UINT) u4NumPkt);

	if (status != NDIS_STATUS_SUCCESS) {
		DBGLOG(INIT, ERROR,
		       ("Could not allocate buffer descriptor pool of %d descriptors\n", u4NumPkt));
		return ERRLOG_OUT_OF_BUFFER_POOL;
	}

	GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_BUF_POOL);

	DBGLOG(INIT, TRACE, ("Allocate buffer descriptor pool of %d descriptors\n", u4NumPkt));

	return 0;
}				/* windowsInitRxPacketPool */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to put the packet to packet pool
*
* \param[in] prGlueInfo Pointer to the glue structure
* \param[in] prPktDscr Pointer to the packet descriptor
* \param[in] pvPayloadBuf Pointer to payload buffer address. NULL means that
*                         original buffer is retained.
*
* \retval none
*
* \note - &ndisPkt_p->MiniportReservedEx[sizeof(PVOID)] is used to store next
*         NDIS_PACKET descriptor
*
*       - &ndisPkt_p->MiniportReservedEx[sizeof(PVOID) * 2] is used to store the
*         address of buffer
*/
/*----------------------------------------------------------------------------*/
VOID putPoolPacket(IN P_GLUE_INFO_T prGlueInfo, IN PNDIS_PACKET prPktDscr, IN PVOID pvPayloadBuf)
{
	PNDIS_PACKET *pprPkt;	/* Pointer to next packet descriptor */
	PVOID *ppvBuf;		/* Pointer to starting address of payload buffer */

	ASSERT(prPktDscr);

	pprPkt =
	    (PNDIS_PACKET *) &prPktDscr->
	    MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, prNextPkt)];
	*pprPkt = prGlueInfo->pvPktDescrHead;

	if (pvPayloadBuf) {
		ppvBuf =
		    (PVOID *) &prPktDscr->MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, pvBuf)];
		*ppvBuf = pvPayloadBuf;

		ASSERT((UINT_32) pvPayloadBuf >= (UINT_32) prGlueInfo->pucPayloadPool);
		ASSERT((UINT_32) pvPayloadBuf <
		       (UINT_32) (prGlueInfo->pucPayloadPool + prGlueInfo->u4PayloadPoolSz));
	}
#if DBG
	else {
		UINT_32 buffer_addr =
		    *(PUINT_32) & prPktDscr->
		    MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, pvBuf)];
		ASSERT(buffer_addr >= (UINT_32) prGlueInfo->pucPayloadPool);
		ASSERT(buffer_addr <
		       (UINT_32) (prGlueInfo->pucPayloadPool + prGlueInfo->u4PayloadPoolSz));
	}
#endif
	prGlueInfo->pvPktDescrHead = prPktDscr;
	prGlueInfo->u4PktDescrFreeNum++;

}				/* putPoolPacket */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to get a packet from the head of packet pool
*
* \param[in] prGlueInfo - Pointer to the glue structure
*
* \return Packet descriptor if packet pool is available
* \retval NULL If no pakcet in packet pool, return NULL pointer
*/
/*----------------------------------------------------------------------------*/
PNDIS_PACKET getPoolPacket(IN P_GLUE_INFO_T prGlueInfo)
{
	PNDIS_PACKET prPktDscr;
	PNDIS_PACKET *pprPkt;


	prPktDscr = (PNDIS_PACKET) prGlueInfo->pvPktDescrHead;
	if (prPktDscr) {
		/* get the next pktDscr */
		pprPkt =
		    (PNDIS_PACKET *) &prPktDscr->
		    MiniportReservedEx[OFFSET_OF(PKT_INFO_RESERVED, prNextPkt)];

		ASSERT(prGlueInfo->u4PktDescrFreeNum >= 1);
		prGlueInfo->pvPktDescrHead = *pprPkt;
		prGlueInfo->u4PktDescrFreeNum--;
	}

	return prPktDscr;
}				/* getPoolPacket */




#ifdef NDIS51_MINIPORT
/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID
mpPnPEventNotify(IN NDIS_HANDLE miniportAdapterContext,
		 IN NDIS_DEVICE_PNP_EVENT pnpEvent,
		 IN PVOID informationBuffer_p, IN UINT_32 informationBufferLength)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;
	P_QUE_ENTRY_T prQueueEntry;
	P_QUE_T prTxQue;
	PNDIS_PACKET prNdisPacket;

	DEBUGFUNC("MPPnPEventNotify");

	INITLOG(("\n"));

	prTxQue = &prGlueInfo->rTxQueue;

	switch (pnpEvent) {
	case NdisDevicePnPEventQueryRemoved:
		INITLOG(("NdisDevicePnPEventQueryRemoved\n"));
		break;

	case NdisDevicePnPEventRemoved:
		INITLOG(("NdisDevicePnPEventRemoved\n"));
		break;

	case NdisDevicePnPEventSurpriseRemoved:
		DBGPRINTF("NdisDevicePnPEventSurpriseRemoved\n");
		prGlueInfo->fgIsCardRemoved = TRUE;

		/* Release pending OID */
		if (prGlueInfo->i4OidPendingCount) {
			wlanReleasePendingOid(prGlueInfo->prAdapter, 0);
		}
		/* Remove pending TX */
		if (prGlueInfo->i4TxPendingFrameNum) {
			kalFlushPendingTxPackets(prGlueInfo);
		}
		/* Remove pending security frames */
		if (prGlueInfo->i4TxPendingSecurityFrameNum) {
			kalClearSecurityFrames(prGlueInfo);
		}

		wlanCardEjected(prGlueInfo->prAdapter);
		INITLOG(("NdisDevicePnPEventSurpriseRemoved\n"));
		DBGPRINTF("NdisDevicePnPEventSurpriseRemoved -- Completed.\n");
		break;

	case NdisDevicePnPEventQueryStopped:
		INITLOG(("NdisDevicePnPEventQueryStopped\n"));
		break;

	case NdisDevicePnPEventStopped:
		INITLOG(("NdisDevicePnPEventStopped\n"));
		break;

	case NdisDevicePnPEventPowerProfileChanged:
#if DBG
		switch (*((PUINT_32) informationBuffer_p)) {
		case 0:	/* NdisPowerProfileBattery */
			INITLOG(("NdisDevicePnPEventPowerProfileChanged: NdisPowerProfileBattery\n"));
			break;

		case 1:	/* NdisPowerProfileAcOnline */
			INITLOG(("NdisDevicePnPEventPowerProfileChanged: NdisPowerProfileAcOnline\n"));
			break;

		default:
			INITLOG(("unknown power profile mode: %d\n",
				 *((PUINT_32) informationBuffer_p)));
		}
#endif

		break;

	default:
		INITLOG(("unknown PnP event 0x%x\n", pnpEvent));
	}



}				/* MPPnPEventNotify */

/*----------------------------------------------------------------------------*/
/*!
* \brief
*
* \param
*
* \retval none
*/
/*----------------------------------------------------------------------------*/
VOID mpShutdown(IN PVOID shutdownContext)
{
}				/* mpShutdown */

#endif				/* NDIS51_MINIPORT */
