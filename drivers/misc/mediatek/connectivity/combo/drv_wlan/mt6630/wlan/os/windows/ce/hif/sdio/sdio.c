/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/ce/hif/sdio/sdio.c#1 $
*/

/*! \file   sdio.c
    \brief  Define SDIO setup/destroy functions

*/



/*
** $Log: sdio.c $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 03 14 2011 terry.wu
 * [WCXRP00000521] [MT6620 Wi-Fi][Driver] Remove non-standard debug message
 * Revert windows debug message.
 *
 * 11 10 2010 cp.wu
 * [WCXRP00000166] [MT6620 Wi-Fi][Driver] use SDIO CMD52 for enabling/disabling interrupt to reduce transaction period
 * add kalDevWriteWithSdioCmd52() for CE platform
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-05-07 16:46:13 GMT mtk01426
**  Add CIS Content Dump
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-03-24 09:46:56 GMT mtk01084
**  fix LINT error
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-03-23 16:56:52 GMT mtk01084
**  add kalDevReadAfterWriteWithSdioCmd52()
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-03-17 10:40:52 GMT mtk01426
**  Move TxServiceThread to Kal layer
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-03-17 09:48:50 GMT mtk01426
**  Update TxServiceThread to kalTxServiceThread
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-03-17 09:46:43 GMT mtk01426
**  Move TxServiceThread to kal layer
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-03-16 16:40:31 GMT mtk01461
**  Fix for coding style - rTxReqEvent
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-03-16 09:11:40 GMT mtk01461
**  Update TX PATH API
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:29:30 GMT mtk01426
**  Init for develop
**
*/

/*******************************************************************************
*                E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"
#include <ceddk.h>

#include "hif.h"

/*******************************************************************************
*                          C O N S T A N T S
********************************************************************************
*/
#define CUSTOM_TUPLE            0x81
#define CUSTOM_TUPLE_BODY       0x24

/* This definition can not be larger than 512 */
#define MAX_SD_RW_BYTES         512

/*  initialize debug zones */
#define  SDNDIS_ZONE_SEND       SDCARD_ZONE_0
#define  ENABLE_ZONE_SEND       ZONE_ENABLE_0
#define  SDNDIS_ZONE_RCV        SDCARD_ZONE_1
#define  ENABLE_ZONE_RCV        ZONE_ENABLE_1

#define  SDNDIS_ZONE_SPECIAL    SDCARD_ZONE_2
#define  ENABLE_ZONE_SPECIAL    ZONE_ENABLE_2

#define  SDNDIS_ZONE_MCR_CNT    SDCARD_ZONE_3
#define  ENABLE_ZONE_MCR_CNT    ZONE_ENABLE_3

SD_DEBUG_INSTANTIATE_ZONES(TEXT("SDIO NDIS Sample"),	/*  module name */
			   (ZONE_ENABLE_INIT | ZONE_ENABLE_INFO | ZONE_ENABLE_ERROR | ZONE_ENABLE_WARN | ENABLE_ZONE_SEND | ENABLE_ZONE_SPECIAL),	/*  initial settings */
			   TEXT(""),
			   TEXT(""),
			   TEXT(""),
			   TEXT(""),
			   TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""), TEXT(""));

#define SD_BUS_WIDTH_REGISTRY_1BIT_MODE         1

#define SD_BUS_WIDTH_REGISTRY_4BIT_MODE         4


#define SD_BUS_BLOCK_SIZE_REGISTRY_USD_DEFAULT  0
#define SD_BUS_CLOCK_RATE_REGISTRY_USD_DEFAULT  0

/* NOTE(George Kuo): If sdcardddk.h is not updated to SDBUS2, the following
**  definitions are needed or compile error occurs. To correct this problem,
**  please update ce5.0/6.0 to yearly full update package 2007 is a minimum
**  requirement.
*/
#ifndef SD_IS_FAST_PATH_AVAILABLE
#define SD_IS_FAST_PATH_AVAILABLE (SD_IO_FUNCTION_ENABLE + 12)
#endif
#ifndef SD_API_STATUS_FAST_PATH_SUCCESS
#define SD_API_STATUS_FAST_PATH_SUCCESS ((SD_API_STATUS)0x00000002L)
#endif
#ifndef SD_FAST_PATH_ENABLE
#define SD_FAST_PATH_ENABLE (SD_IS_FAST_PATH_AVAILABLE + 2)
#endif
#ifndef SD_SYNCHRONOUS_REQUEST
#define SD_SYNCHRONOUS_REQUEST  (0x00000004)
#endif
#ifndef SD_FAST_PATH_AVAILABLE
#define SD_FAST_PATH_AVAILABLE  (0x80000000)
#endif
/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/

/*******************************************************************************
*                        P U B L I C   D A T A
********************************************************************************
*/

/*******************************************************************************
*                       P R I V A T E   D A T A
********************************************************************************
*/

/*******************************************************************************
*                             M A C R O S
********************************************************************************
*/

/*******************************************************************************
*              F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

#if CFG_SDIO_PATHRU_MODE
SD_API_STATUS sdioInterruptCallback(SD_DEVICE_HANDLE hDevice, PVOID pvContext);
#endif

/*******************************************************************************
*                          F U N C T I O N S
********************************************************************************
*/

#if CFG_SDIO_PATHRU_MODE
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to protect PATHRU operation.
*
* \param[in] prPathruInfo Pointer to the GL_PATHRU_INFO_T structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static inline VOID sdioLockPathru(IN P_GL_PATHRU_INFO_T prPathruInfo)
{
	ASSERT(prPathruInfo);
	EnterCriticalSection(&prPathruInfo->rLock);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to protect PATHRU operation.
*
* \param[in] prPathruInfo Pointer to the GL_PATHRU_INFO_T structure.
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
static inline VOID sdioUnlockPathru(IN P_GL_PATHRU_INFO_T prPathruInfo)
{
	ASSERT(prPathruInfo);
	LeaveCriticalSection(&prPathruInfo->rLock);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is state change event handler in PATHRU.
*
* \param[in] pHCContext Pointer to context of host controller
* \param[in] SlotNumber Slot index of the single host
* \param[in] Event Slot event indicated by host controller driver
* \param[in] pvClientContext Pointer to context of client (GLUE_INFO_T)
*
* \return (none)
*
* \notes The function only handles DeviceInterrupting events. Others are
*        redirected to and handled by bus driver in the original way.
*/
/*----------------------------------------------------------------------------*/
static VOID
sdioIndicateSlotStateChange(IN PSDCARD_HC_CONTEXT pHCContext,
			    IN DWORD SlotNumber, IN SD_SLOT_EVENT Event, IN PVOID pvClientContext)
{
	SD_API_STATUS sdStatus = SD_API_STATUS_UNSUCCESSFUL;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_GL_PATHRU_INFO_OUT_T prPathruOutInfo = NULL;

	ASSERT(pHCContext);
	ASSERT(pvClientContext);

	if ((NULL == pHCContext) || (NULL == pvClientContext)) {
		DBGLOG(HAL, ERROR,
		       ("PATHRU: status indication from SDHC but parameter error:0x%08x 0x%08x\n",
			pHCContext, pvClientContext));
		return;
	}
	prGlueInfo = (P_GLUE_INFO_T) pvClientContext;
	prPathruOutInfo = &prGlueInfo->rHifInfo.rPathruInfo.rInfoOut;

	if (DeviceInterrupting == Event) {
		/* Skip CMD52 Read INT Pending. Callback and then ack interrupt. */
		sdioInterruptCallback(prGlueInfo->rHifInfo.hDevice, prGlueInfo);
		sdStatus = pHCContext->pSlotOptionHandler(pHCContext,
							  SlotNumber,
							  SDHCDAckSDIOInterrupt, NULL, 0);
	} else {
		if (DeviceEjected == Event) {
			/* Disable PATHRU here? */
		}
		prPathruOutInfo->pIndicateSlotStateChange(pHCContext, SlotNumber, Event);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to initialize PATHRU parameters.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return (none)
*
* \notes PATHRU resources are initialized in this function. SDHC handle is
*        created by a CreateFile() call. Initialization is success only if the
*        handle is valid.
*/
/*----------------------------------------------------------------------------*/
VOID sdioInitPathruMode(IN P_GLUE_INFO_T prGlueInfo)
{
	P_GL_PATHRU_INFO_T prPathruInfo = NULL;

	ASSERT(prGlueInfo);
	DBGLOG(INIT, TRACE, ("SDNdis: sdioInitPathruMode\n"));

	prPathruInfo = &prGlueInfo->rHifInfo.rPathruInfo;

	/* 4 <1> Check if initialized */
	if (FALSE != prPathruInfo->fgInitialized) {
		DBGLOG(INIT, WARN, ("SDNdis: init PATHRU mode but is ALREADY initialized\n"));
		return;
	}
	/* 4 <2> Initialize SW structure */
	prPathruInfo->fgInitialized = FALSE;
	prPathruInfo->fgEnabled = FALSE;
	prPathruInfo->hSHCDev = INVALID_HANDLE_VALUE;
	_tcscpy(prPathruInfo->szSHCDevName, TEXT("INVALID"));
	prPathruInfo->pSHCContext = NULL;

	/* 4 <3> load PATHRU parameters */
	_tcscpy(prPathruInfo->szSHCDevName, SDIO_PATHRU_SHC_NAME);
	/* Slot number can be 'extracted' from the handle value:
	 ** prGlueInfo->rHifInfo.hDevice(SDBUS2). It is used only when the sd host
	 ** supports multiple slots at the same time.
	 **
	 ** How to get slot number dynamically?
	 */
	prPathruInfo->dwSlotNumber = 0;	/*  For single slot host, only 0 is used. */

	/* 4 <4> Open handle to SDHC host controller device */
	prPathruInfo->hSHCDev = CreateFile(prPathruInfo->szSHCDevName,
					   GENERIC_READ | GENERIC_WRITE,
					   FILE_SHARE_READ | FILE_SHARE_WRITE,
					   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

	if (INVALID_HANDLE_VALUE == prPathruInfo->hSHCDev) {
		DBGLOG(INIT, ERROR, ("SDNdis: CreateFile() Failed: SHC1\n"));
	} else {
		InitializeCriticalSection(&prPathruInfo->rLock);

		/* 4 <5> Initialization complete */
		prPathruInfo->fgInitialized = TRUE;
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to de-initialize PATHRU parameters.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return (none)
*
* \notes PATHRU resources are de-initialized in this function. If PATHRU is
*        still enabled, disable it first. SDHC handle is closed and
*        CriticalSection object is deleted.
*/
/*----------------------------------------------------------------------------*/
VOID sdioDeinitPathruMode(IN P_GLUE_INFO_T prGlueInfo)
{
	P_GL_PATHRU_INFO_T prPathruInfo = NULL;

	ASSERT(prGlueInfo);

	DBGLOG(INIT, TRACE, ("SDNdis: sdioDeinitPathruMode\n"));

	prPathruInfo = &prGlueInfo->rHifInfo.rPathruInfo;

	/* 4 <1> Check if initialized */
	if (FALSE == prPathruInfo->fgInitialized) {
		DBGLOG(INIT, WARN, ("SDNdis: deinit PATHRU mode but is not initialized\n"));
		return;
	}
	/* 4 <2> Disable PATHRU mode first if needed */
	if (FALSE != prPathruInfo->fgEnabled) {
		sdioEnablePathruMode(prGlueInfo, FALSE);
	}
	/* 4 <3> Close handle to SDHC host controller device */
	if (INVALID_HANDLE_VALUE != prPathruInfo->hSHCDev) {
		CloseHandle(prPathruInfo->hSHCDev);
	}
	/* 4 <4> De-initialization complete */
	prPathruInfo->dwSlotNumber = 0xFFFFFFFF;
	prPathruInfo->pSHCContext = NULL;	/* Referenced from I/O control. Can't free it directly */
	_tcscpy(prPathruInfo->szSHCDevName, TEXT("INVALID"));
	prPathruInfo->hSHCDev = INVALID_HANDLE_VALUE;
	prPathruInfo->fgEnabled = FALSE;
	prPathruInfo->fgInitialized = FALSE;

	DeleteCriticalSection(&prPathruInfo->rLock);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to enable and disable PATHRU mode.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure
* \param[in] fgEnable Flag to enable or disable PATHRU
*
* \retval TRUE  success
* \retval FALSE failure
*
* \notes No operation if PATHRU is not initialized. Send IOCTL to SDHC driver to
*        enable or disable it. When enabling it, SDHC context and original event
*        indicating function is returned from IOCTL.
*/
/*----------------------------------------------------------------------------*/
BOOLEAN sdioEnablePathruMode(IN P_GLUE_INFO_T prGlueInfo, IN BOOLEAN fgEnable)
{
	P_GL_PATHRU_INFO_T prPathruInfo = NULL;
	P_GL_PATHRU_INFO_IN_T pInfoIn = NULL;
	DWORD dwBytesReturned = 0;
	BOOL fgResult = FALSE;

	ASSERT(prGlueInfo);
	DBGLOG(INIT, TRACE, ("SDNdis: sdioEnablePathruMode :%d\n", (FALSE != fgEnable) ? 1 : 0));

	prPathruInfo = &prGlueInfo->rHifInfo.rPathruInfo;
	pInfoIn = &prPathruInfo->rInfoIn;

	/* 4 <1> Check if initialized */
	if (FALSE == prPathruInfo->fgInitialized) {
		DBGLOG(INIT, WARN,
		       ("SDNdis: enable/disable PATHRU mode but is not initialized\n"));
		goto exit_en_pathru;
	}
	ASSERT(INVALID_HANDLE_VALUE != prPathruInfo->hSHCDev);

	/* 4 <2> Check if already enabled/disabled */
	if (fgEnable == prPathruInfo->fgEnabled) {
		DBGLOG(INIT, WARN,
		       ("SDNdis: enable/disable PATHRU mode but is already enabled/disabled :%d\n",
			(FALSE != fgEnable) ? 1 : 0));
		fgResult = TRUE;
		goto exit_en_pathru;
	}
	/* 4 <3> Enable/disable PATHRU mode */
	if (FALSE != fgEnable) {
		/* 4 <3.1> Send I/O control to SDHC host controller device */
		pInfoIn->dwEnable = 1;
		pInfoIn->dwSlotNumber = prPathruInfo->dwSlotNumber;
		pInfoIn->pIndicateSlotStateChange = sdioIndicateSlotStateChange;
		pInfoIn->pvClientContext = (PVOID) prGlueInfo;

		fgResult = DeviceIoControl(prPathruInfo->hSHCDev,
					   IOCTL_SDHC_PATHRU,
					   &prPathruInfo->rInfoIn,
					   sizeof(prPathruInfo->rInfoIn),
					   &prPathruInfo->rInfoOut,
					   sizeof(prPathruInfo->rInfoOut), &dwBytesReturned, NULL);

		if (FALSE == fgResult) {
			DBGLOG(INIT, ERROR,
			       ("SDNdis: DeviceIoControl() failed to enable SDHC PATHRU mode\n"));
			prPathruInfo->fgEnabled = FALSE;
		} else {
			prPathruInfo->pSHCContext = prPathruInfo->rInfoOut.pHcd;
			prPathruInfo->fgEnabled = TRUE;	/* PATHRU mode is enabled successfully */
		}
	} else {
		/* PATHRU mode is disabled from now on */
		prPathruInfo->fgEnabled = FALSE;
		prPathruInfo->pSHCContext = NULL;

		/* 4 <3.2> Send I/O control to SDHC host controller device */
		pInfoIn->dwEnable = 0;
		pInfoIn->dwSlotNumber = prPathruInfo->dwSlotNumber;
		pInfoIn->pIndicateSlotStateChange = NULL;
		pInfoIn->pvClientContext = NULL;

		fgResult = DeviceIoControl(prPathruInfo->hSHCDev,
					   IOCTL_SDHC_PATHRU,
					   &prPathruInfo->rInfoIn,
					   sizeof(prPathruInfo->rInfoIn),
					   &prPathruInfo->rInfoOut,
					   sizeof(prPathruInfo->rInfoOut), &dwBytesReturned, NULL);

		if (FALSE == fgResult) {
			DBGLOG(INIT, ERROR,
			       ("SDNdis: DeviceIoControl() failed to disable SDHC PATHRU mode\n"));
		}
	}

 exit_en_pathru:
	return fgResult;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to send sd bus request in PATHRU mode.
*
* \param[in] prHifInfo Pointer to the GL_HIF_INFO_T structure
* \param[in] ucCmd SD bus command code
* \param[in] dwArg SD bus command argument (32-bit)
* \param[in] tClass SD transfer class (read data/write data/command only)
* \param[in] rType SD bus response type
* \param[in] pResp Pointer to SD response buffer
* \param[in] u4BlockCount SD data block count to be transferred on bus
* \param[in] u4BlockSize SD data block size
* \param[in] pBuff Pointer to buffer to be read or written
* \param[in] dwFlags SD request flags
*
* \return SD_API_STATUS code
*
* \notes SDHC bus request handler function is called directly.
*/
/*----------------------------------------------------------------------------*/
static SD_API_STATUS
sdioPathruSyncReq(IN P_GL_HIF_INFO_T prHifInfo,
		  IN UCHAR ucCmd,
		  IN DWORD dwArg,
		  IN SD_TRANSFER_CLASS tClass,
		  IN SD_RESPONSE_TYPE rType,
		  IN PSD_COMMAND_RESPONSE pResp,
		  IN ULONG u4BlockCount, IN ULONG u4BlockSize, IN PUCHAR pBuff, IN DWORD dwFlags)
{
	SD_BUS_REQUEST rRequest;
	SD_API_STATUS sdStatus = SD_API_STATUS_UNSUCCESSFUL;
	PSDCARD_HC_CONTEXT pHCContext = NULL;
	P_GL_PATHRU_INFO_T prPathruInfo = &prHifInfo->rPathruInfo;

	ASSERT(prHifInfo);
	ASSERT(prPathruInfo->pSHCContext);
	pHCContext = prPathruInfo->pSHCContext;

	/* Build-up request structure */
	kalMemZero(&rRequest, sizeof(SD_BUS_REQUEST));

	/* rRequest.ListEntry; // list entry */
	/* ?PATHRU rRequest.hDevice = prHifInfo->hDevice; // the device this request belongs to */
	/* rRequest.SystemFlags; // system flags */
	rRequest.TransferClass = tClass;	/* transfer class */
	rRequest.CommandCode = ucCmd;	/* command code */
	rRequest.CommandArgument = dwArg;	/* command argument */
	rRequest.CommandResponse.ResponseType = rType;	/* command response */
	/* PATHRU rRequest.RequestParam = 0; // optional request parameter */
	/* rRequest.Status;           // completion status */
	rRequest.NumBlocks = u4BlockCount;	/* number of blocks */
	rRequest.BlockSize = u4BlockSize;	/* size of each block */
	/* rRequest.HCParam;            // host controller parameter, reserved for HC drivers */
	rRequest.pBlockBuffer = pBuff;	/* buffer holding block data */
	rRequest.pCallback = NULL;	/* callback when the request completes */
	/* rRequest.DataAccessClocks;   // data access clocks for data transfers (READ or WRITE), reserved for HC driver */
	rRequest.Flags = dwFlags;	/* request flags */
	/* rRequest.cbSizeOfPhysList */
	/* rRequest.pPhysBuffList */

	if (FALSE != prHifInfo->fgSDIOFastPathEnable) {
		rRequest.Flags |= SD_SYNCHRONOUS_REQUEST;
		rRequest.SystemFlags |= SD_FAST_PATH_AVAILABLE;
	} else {
		/* Non-fast-path mode, not supported yet. */
		ASSERT_REPORT(0, ("PATHRU+Non-fast-path mode is NOT supported yet."));
		sdStatus = SD_API_STATUS_NOT_IMPLEMENTED;
		goto exit_pathru_sync_req;
	}

	sdioLockPathru(prPathruInfo);
	sdStatus = pHCContext->pBusRequestHandler(pHCContext,
						  prPathruInfo->dwSlotNumber, &rRequest);
	sdioUnlockPathru(prPathruInfo);

	if (sdStatus == SD_API_STATUS_FAST_PATH_SUCCESS) {
		/* Restore to original success code */
		sdStatus = SD_API_STATUS_SUCCESS;
	} else {
		DBGLOG(HAL, ERROR,
		       ("PATHRU+Fast-path: request handler do NOT return Fast-path success: 0x%08x\n",
			sdStatus));
	}

 exit_pathru_sync_req:
	return sdStatus;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to send sd bus request.
*
* \param[in] prHifInfo Pointer to the GL_HIF_INFO_T structure
* \param[in] ucCmd SD bus command code
* \param[in] dwArg SD bus command argument (32-bit)
* \param[in] tClass SD transfer class (read data/write data/command only)
* \param[in] rType SD bus response type
* \param[in] pResp Pointer to SD response buffer
* \param[in] u4BlockCount SD data block count to be transferred on bus
* \param[in] u4BlockSize SD data block size
* \param[in] pBuff Pointer to buffer to be read or written
* \param[in] dwFlags SD request flags
*
* \return SD_API_STATUS code
*
* \notes If PATHRU is enabled, redirect this request to sdioPathruSyncReq.
*        Otherwise, call bus request API provided by SDBUS driver as usual.
*/
/*----------------------------------------------------------------------------*/
static inline SD_API_STATUS
SD_SYNC_BUS_REQ(IN P_GL_HIF_INFO_T prHifInfo,
		IN UCHAR ucCmd,
		IN DWORD dwArg,
		IN SD_TRANSFER_CLASS tClass,
		IN SD_RESPONSE_TYPE type,
		IN PSD_COMMAND_RESPONSE pResp,
		IN ULONG u4BlockCount, IN ULONG u4BlockSize, IN PUCHAR pBuff, IN DWORD dwFlags)
{
	ASSERT(prHifInfo);

	if (FALSE != prHifInfo->rPathruInfo.fgEnabled) {
		return sdioPathruSyncReq(prHifInfo, ucCmd, dwArg, tClass, type, pResp,
					 u4BlockCount, u4BlockSize, pBuff, dwFlags);
	} else {
		return SDSynchronousBusRequest(prHifInfo->hDevice, ucCmd, dwArg, tClass,
					       type, pResp, u4BlockCount, u4BlockSize, pBuff,
					       dwFlags);
	}
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to connect sdio interrupt with bus driver.
*
* \param[in] prHifInfo Pointer to the GL_HIF_INFO_T structure
* \param[in] pfnISTHandler Pointer to IST handler function
*
* \return (none)
*
* \notes No special operation for PATHRU is conducted now. Register to SDBUS
*        driver as usual for PATHRU disabled case.
*/
/*----------------------------------------------------------------------------*/
static inline SD_API_STATUS
SDIO_CONNECT_INTERRUPT(IN P_GL_HIF_INFO_T prHifInfo, IN PSD_INTERRUPT_CALLBACK pfnISTHandler)
{
	ASSERT(prHifInfo);
	ASSERT(pfnISTHandler);

	if (FALSE != prHifInfo->rPathruInfo.fgEnabled) {
		/* Nothing to do? */
	}

	return SDIOConnectInterrupt(prHifInfo->hDevice, pfnISTHandler);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to disconnect sdio interrupt with bus driver.
*
* \param[in] prHifInfo Pointer to the GL_HIF_INFO_T structure
* \param[in] pfnISTHandler Pointer to IST handler function
*
* \return (none)
*
* \notes No special operation for PATHRU is conducted now. Disconnect with SDBUS
*        driver as usual for PATHRU disabled case.
*/
/*----------------------------------------------------------------------------*/
static inline VOID SDIO_DISCONNECT_INTERRUPT(IN P_GL_HIF_INFO_T prHifInfo)
{
	ASSERT(prHifInfo);

	if (FALSE != prHifInfo->rPathruInfo.fgEnabled) {
		/* Nothing to do? */
	}
	SDIODisconnectInterrupt(prHifInfo->hDevice);
}

#else

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to send sd bus request.
*
* \param[in] prHifInfo Pointer to the GL_HIF_INFO_T structure
* \param[in] ucCmd SD bus command code
* \param[in] dwArg SD bus command argument (32-bit)
* \param[in] tClass SD transfer class (read data/write data/command only)
* \param[in] rType SD bus response type
* \param[in] pResp Pointer to SD response buffer
* \param[in] u4BlockCount SD data block count to be transferred on bus
* \param[in] u4BlockSize SD data block size
* \param[in] pBuff Pointer to buffer to be read or written
* \param[in] dwFlags SD request flags
*
* \return SD_API_STATUS code
*
* \notes Call bus request API provided by SDBUS driver.
*/
/*----------------------------------------------------------------------------*/
static inline SD_API_STATUS
SD_SYNC_BUS_REQ(IN P_GL_HIF_INFO_T prHifInfo,
		IN UCHAR ucCmd,
		IN DWORD dwArg,
		IN SD_TRANSFER_CLASS tClass,
		IN SD_RESPONSE_TYPE type,
		IN PSD_COMMAND_RESPONSE pResp,
		IN ULONG u4BlockCount, IN ULONG u4BlockSize, IN PUCHAR pBuff, IN DWORD dwFlags)
{
	ASSERT(prHifInfo);
	return SDSynchronousBusRequest(prHifInfo->hDevice, ucCmd, dwArg, tClass,
				       type, pResp, u4BlockCount, u4BlockSize, pBuff, dwFlags);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to connect sdio interrupt with bus driver.
*
* \param[in] prHifInfo Pointer to the GL_HIF_INFO_T structure
* \param[in] pfnISTHandler Pointer to IST handler function
*
* \return (none)
*
* \notes Register IST handler to SDBUS driver.
*/
/*----------------------------------------------------------------------------*/
static inline SD_API_STATUS
SDIO_CONNECT_INTERRUPT(IN P_GL_HIF_INFO_T prHifInfo, IN PSD_INTERRUPT_CALLBACK pfnISTHandler)
{
	ASSERT(prHifInfo);
	ASSERT(pfnISTHandler);

	return SDIOConnectInterrupt(prHifInfo->hDevice, pfnISTHandler);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to disconnect sdio interrupt with bus driver.
*
* \param[in] prHifInfo Pointer to the GL_HIF_INFO_T structure
* \param[in] pfnISTHandler Pointer to IST handler function
*
* \return (none)
*
* \notes Disconnect IST with SDBUS driver.
*/
/*----------------------------------------------------------------------------*/
static inline VOID SDIO_DISCONNECT_INTERRUPT(IN P_GL_HIF_INFO_T prHifInfo)
{
	ASSERT(prHifInfo);
	SDIODisconnectInterrupt(prHifInfo->hDevice);
}

#endif				/* end of CFG_SDIO_PATHRU_MODE */


#if 0
/*******************************************************************************
**  sdioMpSendPackets
**
**  descriptions:
**     Insert NDIS packets in glue layer's NDIS queue. Then notify thread to handle this
**  parameters:
**      miniportAdapterContext - Adapter Structure pointer
**      packetArray_p - an array of pointers to NDIS_PACKET structs
**      numberOfPackets - The number of packets in PacketArray
**  return:       (none)
**  note:
*******************************************************************************/
VOID sdioMpSendPacket(IN NDIS_HANDLE miniportAdapterContext, IN PNDIS_PACKET prNdisPacket)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) miniportAdapterContext;
	P_ADAPTER_T prAdapter = prGlueInfo->prAdapter;
	PNDIS_PACKET prPacket;
	PACKET_INFO_T rPacketInfo;
	UINT_32 u4PacketLen;
	ULONG u4SysTime;

	/* Priority Parameter in 6.2.1.1.2 Semantics of the service primitive */
	UINT_8 ucPriorityParam;
	UINT_8 ucMacHeaderLen;
	UINT_16 u2PayloadLen;
	UINT_8 aucEthDestAddr[PARAM_MAC_ADDR_LEN];
	UINT_32 i;
	BOOLEAN fgIs1x = FALSE;
	WLAN_STATUS rStatus;

	DEBUGFUNC("sdioMpSendPackets");
	DBGLOG(TX, TRACE, ("+\n"));

	for (i = 0; i < numberOfPackets; i++) {

		prPacket = packetArray_p[i];

		if (wlanQoSFrameClassifierAndPacketInfo(prGlueInfo,
							prPacket,
							&ucPriorityParam,
							&u4PacketLen,
							aucEthDestAddr, &fgIs1x) == FALSE) {

			NdisMSendComplete(prGlueInfo->rMiniportAdapterHandle,
					  prPacket, NDIS_STATUS_INVALID_PACKET);
			continue;
		}

		/* Save the value of Priority Parameter */
		GLUE_SET_PKT_TID_IS1X(prPacket, ucPriorityParam, fgIs1x);

		/* TODO(Kevin): For Vista, we may process the Native 802.11 Frame here */

		ucMacHeaderLen = ETH_HLEN;

		/* Save the value of Header Length */
		GLUE_SET_PKT_HEADER_LEN(prPacket, ucMacHeaderLen);

		u2PayloadLen = (UINT_16) (u4PacketLen - ETH_HLEN);

		/* Save the value of Payload Length */
		GLUE_SET_PKT_PAYLOAD_LEN(prPacket, u2PayloadLen);

		/* Save the value of Payload Length */
		NdisGetSystemUpTime(&u4SysTime);
		GLUE_SET_PKT_ARRIVAL_TIME(prPacket, u4SysTime);

		PACKET_INFO_INIT(&rPacketInfo,
				 (BOOLEAN) FALSE,
				 fgIs1x,
				 (P_NATIVE_PACKET) prPacket,
				 ucPriorityParam,
				 ucMacHeaderLen, u2PayloadLen, (PUINT_8) aucEthDestAddr);

		prGlueInfo->u4TxPendingFrameNum++;

		if ((rStatus = wlanSendPacket(prAdapter, &rPacketInfo))
		    == WLAN_STATUS_PENDING) {

			NDIS_SET_PACKET_STATUS(prPacket, NDIS_STATUS_PENDING);
		} else if (rStatus == WLAN_STATUS_SUCCESS) {
			/* NOTE: When rStatus == WLAN_STATUS_SUCCESS, packet will be freed
			 * by WLAN driver internal.
			 */

			/* Do nothing */
		} else if (rStatus == WLAN_STATUS_FAILURE) {

			NdisMSendComplete(prGlueInfo->rMiniportAdapterHandle,
					  prPacket, NDIS_STATUS_INVALID_PACKET);

			prGlueInfo->u4TxPendingFrameNum--;
		} else {
			ASSERT(0);	/* NOTE: Currently we didn't have other return status */
		}
	}

	DBGLOG(TX, TRACE, ("Tx pending %d\n", prGlueInfo->u4TxPendingFrameNum));

	DBGLOG(TX, TRACE, ("-\n"));

}				/* sdioMpSendPackets */
#endif

BOOL sdioDumpCISTuple(P_GL_HIF_INFO_T pDevice, UINT_8 ucTupleCode, BOOL fgCommonCIS)
{
	ULONG length;		/* tuple length */
	SD_API_STATUS status;	/* status */
	UCHAR buffer[128];	/* manufacturer ID */

	length = 0;

	/* try to get the MANFID tuple from the common CIS */
	status = SDGetTuple(pDevice->hDevice, ucTupleCode, NULL, &length, fgCommonCIS);

	if (!SD_API_SUCCESS(status)) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("SDApiTest- SDGetTuple failed 0x%08X, ucTupleCode = 0x%x\n"),
			    status, ucTupleCode));
		return FALSE;
	}

	if (length == 0) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("SDApiTest- ucTupleCode = 0x%x, tuple is missing\n"),
			    ucTupleCode));
		return FALSE;
	}

	/* go fetch it */
	status = SDGetTuple(pDevice->hDevice, ucTupleCode, buffer, &length, fgCommonCIS);

	if ((length == 0) || !SD_API_SUCCESS(status)) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT
			    ("SDApiTest- SDGetTuple failed 0x%08X, length =%d\n, ucTupleCode = 0x%x"),
			    status, length, ucTupleCode));
		return FALSE;
	}

	SDOutputBuffer(buffer, length);

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read the CIS from the common and function areas.
*
* \param[in] pDevice Pointer to the P_GL_HIF_INFO_T structure.
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
BOOL sdioCisTest(P_GL_HIF_INFO_T pDevice)
{
#if DBG & 0
	/* for new sdbus driver v2.0, miniport driver access function 0 register */
	/* will cause bus driver assert. */

	ULONG length;		/* tuple length */
	SD_API_STATUS status;	/* status */
	UCHAR manfID[SD_CISTPL_MANFID_BODY_SIZE];	/* manufacturer ID */
	UCHAR custom[CUSTOM_TUPLE_BODY];	/* custom tuple data */

	length = 0;

	/* try to get the MANFID tuple from the common CIS */
	status = SDGetTuple(pDevice->hDevice, SD_CISTPL_MANFID, NULL, &length, TRUE);

	if (!SD_API_SUCCESS(status)) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("SDApiTest- SDGetTuple failed 0x%08X\n"), status));
		return FALSE;
	}

	if (length == 0) {
		DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("SDApiTest- MANFID tuple is missing\n")));
		return FALSE;
	}

	if (length != SD_CISTPL_MANFID_BODY_SIZE) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("SDApiTest- MANFID tuple reports size of %d bytes\n"), length));
		return FALSE;
	}

	/* go fetch it */
	status = SDGetTuple(pDevice->hDevice, SD_CISTPL_MANFID, manfID, &length, TRUE);

	if ((length == 0) || !SD_API_SUCCESS(status)) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("SDApiTest- SDGetTuple failed 0x%08X, %d\n"), status, length));
		return FALSE;
	}

	DbgPrintZo(1,
		   (TEXT("SDApiTest- MANFID : 0x%02X 0x%02X 0x%02X 0x%02X\n"), manfID[3],
		    manfID[2], manfID[1], manfID[0]));

	length = 0;

	/* try to get a vendor specific tuple from the function CIS */
	status = SDGetTuple(pDevice->hDevice, CUSTOM_TUPLE, NULL, &length, FALSE);

	if (!SD_API_SUCCESS(status)) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("SDApiTest- SDGetTuple failed 0x%08X\n"), status));
		return FALSE;
	}

	if (length == 0) {
		DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("SDApiTest- CUSTOM tuple is missing\n")));
		return FALSE;
	}

	if (length != CUSTOM_TUPLE_BODY) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT
			    ("SDApiTest- custom tuple reports size of %d bytes, should be %d\n"),
			    length, CUSTOM_TUPLE_BODY));
		return FALSE;
	}

	/* try to get a vendor specific tuple from the function CIS */
	status = SDGetTuple(pDevice->hDevice, CUSTOM_TUPLE, custom, &length, FALSE);
	if ((length == 0) || !SD_API_SUCCESS(status)) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("SDApiTest- SDGetTuple custom failed 0x%08X, %d\n"), status,
			    length));
		return FALSE;
	}

	SDOutputBuffer(custom, length);

	return TRUE;

#else				/* DBG & 0 */

	/* Common CIS */
	sdioDumpCISTuple(pDevice, SD_CISTPL_FUNCID, TRUE);
	sdioDumpCISTuple(pDevice, SD_CISTPL_FUNCE, TRUE);
	sdioDumpCISTuple(pDevice, SD_CISTPL_MANFID, TRUE);
	sdioDumpCISTuple(pDevice, SD_CISTPL_VERS_1, TRUE);

	/* Specific CIS */
	sdioDumpCISTuple(pDevice, SD_CISTPL_FUNCID, FALSE);
	sdioDumpCISTuple(pDevice, SD_CISTPL_FUNCE, FALSE);

	return TRUE;
#endif
}				/* CISTest */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is the SDIO interrupt handler.
*
* \param[in] hDevice handle to the SDIO device.
* \param[in] pvContext device specific context that was registered
*
* \retval SD_API_STATUS code
*/
/*----------------------------------------------------------------------------*/
SD_API_STATUS sdioInterruptCallback(SD_DEVICE_HANDLE hDevice, PVOID pvContext)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) pvContext;

	/* modify wlanISR interface to decide whether to control GINT or not */
	if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_HALT)) {
		/* NOTE(Kevin): We should diable GINT if we are going to HALT the driver. */
		wlanISR(prGlueInfo->prAdapter, TRUE);
	} else if (
#if BUILD_ENE_INTR_WORKAROUND
			  wlanISR(prGlueInfo->prAdapter, TRUE)
#else
			  wlanISR(prGlueInfo->prAdapter, FALSE)
#endif
	    ) {
		wlanIST(prGlueInfo->prAdapter);
	}

	return SD_API_STATUS_SUCCESS;

}				/* sdioInterruptCallback */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is the slot event callback for fast-path events.
*
* \param[in] hDevice handle to the SDIO device.
* \param[in] pvContext device specific context that was registered
* \param[in] SlotEventType slot event type
* \param[in] pData Slot event data (can be NULL)
* \param[in] DataLength length of slot event data (can be 0)
*
* \return (none)
*
* \notes If this callback is registered the client driver can be notified of
*        slot events (such as device removal) using a fast path mechanism.  This
*        is useful if a driver must be notified of device removal
*        before its XXX_Deinit is called.
*        This callback can be called at a high thread priority and should only
*        set flags or set events.  This callback must not perform any
*        bus requests or call any apis that can perform bus requests.
*/
/*----------------------------------------------------------------------------*/
VOID
sdioSlotEventCallBack(SD_DEVICE_HANDLE hDevice,
		      PVOID pContext,
		      SD_SLOT_EVENT_TYPE SlotEventType, PVOID pData, DWORD DataLength)
{
	P_GLUE_INFO_T prGlueInfo = (P_GLUE_INFO_T) pContext;

	ASSERT(prGlueInfo);

	DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDNdis: +SlotEventCallBack - %d\n"), SlotEventType));

	switch (SlotEventType) {
	case SDCardEjected:

		wlanCardEjected(prGlueInfo->prAdapter);
		DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDNdis: Card Ejected\n")));

		break;

	default:
		break;
	}

	DbgPrintZo(SDCARD_ZONE_INIT, (TEXT("SDNdis: -SlotEventCallBack\n")));
}				/* sdioSlotEventCallBack */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to get the SD Device handle.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] rWrapperConfigurationContext windows wrapper configuration context
*
* \return NDIS_STATUS code
*
* \notes The bus driver loads NDIS and stores the SD Device Handle context
*        in NDIS's device active key.  This function scans the NDIS
*        configuration for the ActivePath to feed into the
*        SDGetDeviceHandle API.
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
sdioGetDeviceHandle(IN P_GLUE_INFO_T prGlueInfo, IN NDIS_HANDLE rWrapperConfigurationContext)
{
	NDIS_STATUS status;
	NDIS_HANDLE configHandle;
	NDIS_STRING activePathKey = NDIS_STRING_CONST("ActivePath");
	PNDIS_CONFIGURATION_PARAMETER pConfigParm;

	DEBUGFUNC("sdioGetDeviceHandle");

	/* Open the registry for this adapter */
	NdisOpenConfiguration(&status, &configHandle, rWrapperConfigurationContext);

	if (status != NDIS_STATUS_SUCCESS) {
		ERRORLOG(("NdisOpenConfiguration failed (status = 0x%x)\n", status));
		return status;
	}

	/* read the ActivePath key set by the NDIS loader driver */
	NdisReadConfiguration(&status,
			      &pConfigParm, configHandle, &activePathKey, NdisParameterString);

	if (!NDIS_SUCCESS(status)) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("SDNdis: Failed to get active path key (0x%08X)\n"), status));

		switch (status) {
		case NDIS_STATUS_RESOURCES:
			DbgPrintZo(SDCARD_ZONE_ERROR,
				   (TEXT("SDNdis: %s\n"), "NDIS_STATUS_RESOURCES"));
			break;
		case NDIS_STATUS_FAILURE:
			DbgPrintZo(SDCARD_ZONE_ERROR,
				   (TEXT("SDNdis: %s\n"), "NDIS_STATUS_FAILURE"));
			break;
		}

		/* close our registry configuration */
		NdisCloseConfiguration(configHandle);
		return status;
	}

	if (NdisParameterString != pConfigParm->ParameterType) {
		DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("SDNdis: PARAMETER TYPE NOT STRING!!!\n")));
		/* close our registry configuration */
		NdisCloseConfiguration(configHandle);
		return status;
	}

	if (pConfigParm->ParameterData.StringData.Length > sizeof(prGlueInfo->rHifInfo.ActivePath)) {
		DbgPrintZo(SDCARD_ZONE_ERROR, (TEXT("SDNdis: Active path too long!\n")));
		return NDIS_STATUS_FAILURE;
	}

	/* copy the counted string over */
	memcpy(prGlueInfo->rHifInfo.ActivePath,
	       pConfigParm->ParameterData.StringData.Buffer,
	       pConfigParm->ParameterData.StringData.Length);

	DbgPrintZo(SDCARD_ZONE_INIT,
		   (TEXT("SDNdis: Active Path Retrieved: %s\n"), prGlueInfo->rHifInfo.ActivePath));

	/* now get the device handle */
	prGlueInfo->rHifInfo.hDevice =
	    SDGetDeviceHandle((DWORD) prGlueInfo->rHifInfo.ActivePath, NULL);
	status = prGlueInfo->rHifInfo.hDevice ? NDIS_STATUS_SUCCESS : NDIS_STATUS_FAILURE;

	/* close our registry configuration */
	NdisCloseConfiguration(configHandle);
	return status;
}				/* sdioGetDeviceHandle */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to setup card feature.
*
* \param[in] pDevice Pointer to the GL_HIF_INFO_T structure.
*
* \return SD_API_STATUS code
*/
/*----------------------------------------------------------------------------*/
SD_API_STATUS sdioSetupCardFeature(P_GL_HIF_INFO_T pDevice)
{
	SD_API_STATUS status;	/* intermediate status  */
	SD_CARD_INTERFACE cardInterface;	/* card interface information */
	SDIO_CARD_INFO sdioInfo;	/* sdio info */
	DWORD blockLength;	/* block length */
	BOOL retValue;		/* return value */
	SD_HOST_BLOCK_CAPABILITY sdHostBlockCap;
	P_REG_INFO_T prReginfo;
	SD_IO_FUNCTION_ENABLE_INFO functionEnable;	/* enable sd card function */

	DEBUGFUNC("sdioSetUpCardFeature");


	ASSERT(pDevice);

	prReginfo = &pDevice->prGlueInfo->rRegInfo;

	/* query the RCA */
	status = SDCardInfoQuery(pDevice->hDevice,
				 SD_INFO_REGISTER_RCA, &pDevice->RCA, sizeof(pDevice->RCA));

	if (!SD_API_SUCCESS(status)) {
		DBGLOG(INIT, ERROR, ("MT6620 NDIS: Failed to query RCA ! 0x%08X \n", status));
		return status;
	}
	DBGLOG(INIT, TRACE, ("MT6620 NDIS: RCA: 0x%04X\n", pDevice->RCA));

	/* query the card interface */
	status = SDCardInfoQuery(pDevice->hDevice,
				 SD_INFO_CARD_INTERFACE, &cardInterface, sizeof(cardInterface));

	if (!SD_API_SUCCESS(status)) {
		DBGLOG(INIT, ERROR, ("MT6620 NDIS: Failed to query interface ! 0x%08X \n",
				     status));
		return status;
	}

	if (cardInterface.ClockRate == 0) {
		DBGLOG(INIT, ERROR, ("MT6620 NDIS: Device interface rate is zero!\n"));
		return SD_API_STATUS_UNSUCCESSFUL;
	}
	DBGLOG(INIT, TRACE, ("MT6620 NDIS: Interface Clock : %d Hz\n", cardInterface.ClockRate));

	if (cardInterface.InterfaceMode == SD_INTERFACE_SD_MMC_1BIT) {
		DBGLOG(INIT, TRACE, ("MT6620 NDIS: Bus return 1 Bit interface mode\n"));
	} else if (cardInterface.InterfaceMode == SD_INTERFACE_SD_4BIT) {
		DBGLOG(INIT, TRACE, ("MT6620 NDIS: Bus return 4 bit interface mode\n"));
	} else {
		DBGLOG(INIT, TRACE, ("MT6620 NDIS: Bus return unknown interface mode! %d\n",
				     cardInterface.InterfaceMode));
		return SD_API_STATUS_UNSUCCESSFUL;
	}

	/* set clock rate and bit mode */
	switch (prReginfo->u4SdClockRate) {
	case SD_BUS_CLOCK_RATE_REGISTRY_USD_DEFAULT:
		DBGLOG(INIT, TRACE, ("MT6620 NDIS:registry use bus defualt clock rate %d\n",
				     cardInterface.ClockRate));
		break;
	default:
		cardInterface.ClockRate = prReginfo->u4SdClockRate;
		DBGLOG(INIT, TRACE, ("MT6620 NDIS:registry use registry clock rate %d\n",
				     cardInterface.ClockRate));
		break;
	}


	switch (prReginfo->u4SdBusWidth) {
	case SD_BUS_WIDTH_REGISTRY_4BIT_MODE:
		cardInterface.InterfaceMode = SD_INTERFACE_SD_4BIT;
		DBGLOG(INIT, TRACE, ("MT6620 NDIS:registry 4 bit interface mode\n"));
		break;
	case SD_BUS_WIDTH_REGISTRY_1BIT_MODE:
		cardInterface.InterfaceMode = SD_INTERFACE_SD_MMC_1BIT;
		DBGLOG(INIT, TRACE, ("MT6620 NDIS:registry 1 bit interface mode\n"));
		break;
	default:
		DBGLOG(INIT, TRACE,
		       ("MT6620 NDIS:registry use default mode  %d. ( 0:1bit, 1:4bits)\n",
			cardInterface.InterfaceMode));
		break;
	}

	/* Clock rate is chanaged during this function call. */
	status = SDSetCardFeature(pDevice->hDevice,
				  SD_SET_CARD_INTERFACE, &cardInterface, sizeof(cardInterface));
	if (!SD_API_SUCCESS(status)) {
		DBGLOG(INIT, ERROR, ("MT6620 NDIS: Failed to set SDIO info clock rate! 0x%08X\n",
				     status));
		return status;
	}

	/* set up the function enable struct */
	/* TODO use the appropriate retry and interval count for the function */
	functionEnable.Interval = 500;
	functionEnable.ReadyRetryCount = 3;

	DBGLOG(INIT, TRACE, ("MT6620 NDIS : Enabling Card ...\n"));

	/*turn on our function */
	status = SDSetCardFeature(pDevice->hDevice,
				  SD_IO_FUNCTION_ENABLE,
				  &functionEnable, sizeof(SD_IO_FUNCTION_ENABLE_INFO));

	if (!SD_API_SUCCESS(status)) {
		DBGLOG(INIT, ERROR, ("MT6620 NDIS: Failed to enable Function:0x%08X\n", status));
		return status;
	}

	/* query the SDIO information */
	status = SDCardInfoQuery(pDevice->hDevice, SD_INFO_SDIO, &sdioInfo, sizeof(sdioInfo));

	if (!SD_API_SUCCESS(status)) {
		DBGLOG(INIT, ERROR,
		       ("MT6620 NDIS: Failed to query SDIO info ! 0x%08X \n", status));
		return status;
	}

	/* this card only has one function */
	if (sdioInfo.FunctionNumber != 1) {
		DBGLOG(INIT, ERROR, ("MT6620 NDIS: Function number %d is incorrect!\n",
				     sdioInfo.FunctionNumber));
		return SD_API_STATUS_UNSUCCESSFUL;
	}

	/* save off function number */
	pDevice->Function = sdioInfo.FunctionNumber;

	DBGLOG(INIT, TRACE, ("MT6620 NDIS: Function: %d\n", pDevice->Function));
	DBGLOG(INIT, TRACE, ("MT6620 NDIS: Device Code: %d\n", sdioInfo.DeviceCode));
	DBGLOG(INIT, TRACE, ("MT6620 NDIS: CISPointer: 0x%08X\n", sdioInfo.CISPointer));
	DBGLOG(INIT, TRACE, ("MT6620 NDIS: CSAPointer: 0x%08X\n", sdioInfo.CSAPointer));
	DBGLOG(INIT, TRACE, ("MT6620 NDIS: CardCaps: 0x%02X\n", sdioInfo.CardCapability));


	sdHostBlockCap.ReadBlockSize = BLOCK_TRANSFER_LEN;
	sdHostBlockCap.WriteBlockSize = BLOCK_TRANSFER_LEN;
	sdHostBlockCap.ReadBlocks = 511;
	sdHostBlockCap.WriteBlocks = 511;

	switch (prReginfo->u4SdBlockSize) {
	case SD_BUS_BLOCK_SIZE_REGISTRY_USD_DEFAULT:
		DBGLOG(INIT, TRACE, ("MT6620 NDIS:registry use defualt block size %d\n",
				     sdHostBlockCap.ReadBlockSize));
		break;
	default:
		sdHostBlockCap.ReadBlockSize = (UINT_16) prReginfo->u4SdBlockSize;
		sdHostBlockCap.WriteBlockSize = (UINT_16) prReginfo->u4SdBlockSize;
		DBGLOG(INIT, TRACE, ("MT6620 NDIS:registry block size = %d\n",
				     sdHostBlockCap.ReadBlockSize));
		break;
	}

	/* query the card interface */
	status = SDCardInfoQuery(pDevice->hDevice,
				 SD_INFO_HOST_BLOCK_CAPABILITY,
				 &sdHostBlockCap, sizeof(sdHostBlockCap));

	DBGLOG(INIT, TRACE,
	       ("MT6620 NDIS: SDIO host R_BlockSize %d W_BlockSize %d R_Blocks %d W_Blocks%d\n",
		sdHostBlockCap.ReadBlockSize, sdHostBlockCap.WriteBlockSize,
		sdHostBlockCap.ReadBlocks, sdHostBlockCap.WriteBlocks));

	blockLength = sdHostBlockCap.WriteBlockSize;

	/* NOTE(Kevin): Shouldn't reject blockLength > 512. Add following workaround
	 * only if bus driver can't accept > 512.
	 */
#if 0

	if (sdHostBlockCap.WriteBlockSize < BLOCK_TRANSFER_LEN) {
		blockLength = sdHostBlockCap.WriteBlockSize;
	} else {
		blockLength = BLOCK_TRANSFER_LEN;
	}
#endif

	/* set the block length for this function */
	status = SDSetCardFeature(pDevice->hDevice,
				  SD_IO_FUNCTION_SET_BLOCK_SIZE, &blockLength, sizeof(blockLength));

	if (!SD_API_SUCCESS(status)) {
		DBGLOG(INIT, ERROR, ("MT6620 NDIS: Failed to set Block Length ! 0x%08X\n", status));
		return status;
	}

	DBGLOG(INIT, TRACE, ("MT6620 NDIS: Block Size set to %d bytes\n", blockLength));

	if (blockLength == 64) {
		/* This is a workaround for PXA 255 platform */
		cardInterface.InterfaceMode = SD_INTERFACE_SD_MMC_1BIT;
		SDSetCardFeature(pDevice->hDevice,
				 SD_SET_CARD_INTERFACE, &cardInterface, sizeof(SD_CARD_INTERFACE));
	}

	pDevice->sdHostBlockCap.ReadBlockSize = (USHORT) blockLength;
	pDevice->sdHostBlockCap.WriteBlockSize = (USHORT) blockLength;


	switch (sdHostBlockCap.WriteBlockSize) {
	case 64:
		pDevice->WBlkBitSize = 6;
		break;
	case 128:
		pDevice->WBlkBitSize = 7;
		break;
	case 256:
		pDevice->WBlkBitSize = 8;
		break;
	case 512:
		pDevice->WBlkBitSize = 9;
		break;
	}

	retValue = sdioCisTest(pDevice);
	/* sdioCisTest is disabled and result is ignored. */

#if CFG_SDIO_PATHRU_MODE
	/* Fast-path is default disabled */
	pDevice->fgSDIOFastPathEnable = FALSE;
#endif
	/* NOTE(George Kuo): Check if FAST_PATH is available */
	status = SDSetCardFeature(pDevice->hDevice, SD_IS_FAST_PATH_AVAILABLE, NULL, 0);
	if (!SD_API_SUCCESS(status)) {
		DBGLOG(INIT, TRACE, ("MT6620 NDIS: SD bus driver Fast-Path is not available\n"));
	} else {
		/* try to enable FAST_PATH */
		status = SDSetCardFeature(pDevice->hDevice, SD_FAST_PATH_ENABLE, NULL, 0);
		if (!SD_API_SUCCESS(status)) {
			DBGLOG(INIT, TRACE,
			       ("MT6620 NDIS: SD bus driver Fast-Path is available but failed to enable\n"));
		} else {
			DBGLOG(INIT, TRACE,
			       ("MT6620 NDIS: SD bus driver Fast-Path is available and enabled\n"));
#if CFG_SDIO_PATHRU_MODE
			/* Fast-path is enabled */
			pDevice->fgSDIOFastPathEnable = TRUE;
#endif
		}
	}

	return SD_API_STATUS_SUCCESS;
}				/* sdioSetUpCardFeature */

#if 0
/*******************************************************************************
**  sdiosetBlockSize - setup sdio block size
**  Input:  pvAdapter
**          u4Register - register offset
**  Output:
**  Return:  TRUE if access successful
**  Notes:
*******************************************************************************/
TODO WLAN_STATUS
sdioSetBlockSize(IN PVOID pvAdapter,
		 IN PVOID setBuffer_p, IN UINT_32 setBufferLen, OUT PUINT_32 setInfoLen_p)
{
	P_GLUE_INFO_T prGlueInfo = wlanGetAdapterPrivate(pvAdapter);
	P_GL_HIF_INFO_T prSdInfo = &prGlueInfo->rHifInfo;
	SD_API_STATUS status;
	SD_HOST_BLOCK_CAPABILITY sdHostBlockCap;
	UINT_32 u4blocksize = *(PUINT_32) setBuffer_p;

	/* query the card interface */
	status = SDCardInfoQuery(prSdInfo->hDevice,
				 SD_INFO_HOST_BLOCK_CAPABILITY,
				 &sdHostBlockCap, sizeof(sdHostBlockCap));
	KdPrint(("Set sdHostBlockCap : %d\n", sdHostBlockCap.WriteBlockSize));

	prSdInfo->sdHostBlockCap.ReadBlockSize = (USHORT) u4blocksize;
	prSdInfo->sdHostBlockCap.WriteBlockSize = (USHORT) u4blocksize;

	/* set the block length for this function */
	status = SDSetCardFeature(prSdInfo->hDevice,
				  SD_IO_FUNCTION_SET_BLOCK_SIZE, &u4blocksize, sizeof(u4blocksize));
	if (!SD_API_SUCCESS(status)) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("IPN2128 NDIS: Failed to set Block Length ! 0x%08X \n"), status));
		KdPrint(("sdiosetBlockSize set to %d failed\n", u4blocksize));

		return FALSE;
	}

	return TRUE;
}				/* sdioSetBlockSize */
#endif

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to allocate memory mapping for the HW register.
*        (SDIO does not need to map or allocate any memory)
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsMapAllocateRegister(IN P_GLUE_INFO_T prGlueInfo)
{
	return NDIS_STATUS_SUCCESS;
}				/* windowsMapAllocateRegister */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to free memory mapping for the HW register.
*        (SDIO does not need Free Register)
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsUMapFreeRegister(IN P_GLUE_INFO_T prGlueInfo)
{
	return NDIS_STATUS_SUCCESS;
}				/* windowsUMapFreeRegister */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to register interrupt call back function.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsRegisterIsrt(IN P_GLUE_INFO_T prGlueInfo)
{
	SD_API_STATUS sdStatus;
	NDIS_STATUS status;

	/* connect the interrupt callback */
#if CFG_SDIO_PATHRU_MODE
	sdStatus = SDIO_CONNECT_INTERRUPT(&prGlueInfo->rHifInfo,
					  (PSD_INTERRUPT_CALLBACK) sdioInterruptCallback);
#else
	sdStatus = SDIOConnectInterrupt(prGlueInfo->rHifInfo.hDevice,
					(PSD_INTERRUPT_CALLBACK) sdioInterruptCallback);
#endif

	if (!SD_API_SUCCESS(sdStatus)) {
		DbgPrintZo(SDCARD_ZONE_ERROR,
			   (TEXT("SDNDIS: Failed to connect interrupt: 0x%08X\n"), sdStatus));
		status = NDIS_STATUS_FAILURE;
		return status;
	}

	GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_INTERRUPT_IN_USE);
	return NDIS_STATUS_SUCCESS;
}				/* windowsRegisterIsrt */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to un-register interrupt call back function.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsUnregisterIsrt(IN P_GLUE_INFO_T prGlueInfo)
{
	DEBUGFUNC("windowsUnregisterIsrt");

	if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_INTERRUPT_IN_USE)) {

#if CFG_SDIO_PATHRU_MODE
		SDIO_DISCONNECT_INTERRUPT(&prGlueInfo->rHifInfo);
#else
		SDIODisconnectInterrupt(prGlueInfo->rHifInfo.hDevice);
#endif
		GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_INTERRUPT_IN_USE);
		INITLOG(("Interrupt deregistered\n"));
	}
	return NDIS_STATUS_SUCCESS;
}				/* windowsUnregisterIsrt */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to initialize adapter members.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] rWrapperConfigurationContext windows wrapper configuration context
*
* \return NDIS_STATUS code
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
windowsFindAdapter(IN P_GLUE_INFO_T prGlueInfo, IN NDIS_HANDLE rWrapperConfigurationContext)
{
	NDIS_STATUS NdisStatus;	/* intermediate status */
	NDIS_STATUS status;
	SDCARD_CLIENT_REGISTRATION_INFO clientInfo;	/* client registration */
	SD_API_STATUS sdStatus;	/* SD Status */
	P_GL_HIF_INFO_T pDevice;
	BOOLEAN fgBusInit;
	HANDLE hThread = NULL;

	DEBUGFUNC("windowsFindAdapter");

	status = sdioGetDeviceHandle(prGlueInfo, rWrapperConfigurationContext);
	if (status != NDIS_STATUS_SUCCESS) {
		return status;
	}

	fgBusInit = platformBusInit(prGlueInfo);
	if (FALSE == fgBusInit) {
		ERRORLOG(("SDIO platformBusInit fail\n"));
		return NDIS_STATUS_FAILURE;
	}

	pDevice = &prGlueInfo->rHifInfo;
	pDevice->prGlueInfo = prGlueInfo;

#if CFG_SDIO_PATHRU_MODE
	/* Try to initialize PATHRU mode */
	sdioInitPathruMode(prGlueInfo);
#endif

	DBGLOG(INIT, TRACE, ("SDNdis: SDNdisInitializeAdapter\n"));

	NdisStatus = NDIS_STATUS_FAILURE;

	if (pDevice->hDevice != NULL) {
		memset(&clientInfo, 0, sizeof(clientInfo));

		/* set client options and register as a client device */
		_tcscpy(clientInfo.ClientName, TEXT("NIC_DEVICE_ID"));

		/* set the event callback */
		clientInfo.pSlotEventCallBack = sdioSlotEventCallBack;

		sdStatus = SDRegisterClient(pDevice->hDevice, prGlueInfo, &clientInfo);

		if (!SD_API_SUCCESS(sdStatus)) {
			DBGLOG(INIT, ERROR,
			       ("SDNDIS: Failed to register client : 0x%08X\n", sdStatus));
			return NdisStatus;
		}

		/* set up card */
		sdStatus = sdioSetupCardFeature(pDevice);
		if (!SD_API_SUCCESS(sdStatus)) {
			DBGLOG(INIT, ERROR,
			       ("SDNDIS: sdioSetUpCardFeature return failed :0x%08X", sdStatus));
			return NdisStatus;
		}
		DBGLOG(INIT, TRACE, ("SDNDIS : Card ready\n"));

#if CFG_SDIO_PATHRU_MODE
		if (FALSE != pDevice->fgSDIOFastPathEnable) {
			/* enable PATHRU mode if fast-path is enabled */
			if (FALSE != sdioEnablePathruMode(prGlueInfo, TRUE)) {
				DBGLOG(INIT, TRACE, ("SDNDIS : PATHRU enabled\n"));
			} else {
				DBGLOG(INIT, WARN, ("SDNDIS : Failed to enable PATHRU\n"));
			}
		}
#endif

	} else {
		DEBUG_ASSERT(FALSE);
	}

	return NDIS_STATUS_SUCCESS;
}				/* windowsFindAdapter */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read a 32 bit register value from device.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register the register offset.
* \param[out] pu4Value Pointer to the 32-bit value of the register been read.
*
* \retval TRUE  success
* \retval FALSE failure
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevRegRead(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, OUT PUINT_32 pu4Value)
{
	P_GL_HIF_INFO_T prSdInfo = &prGlueInfo->rHifInfo;
	SD_API_STATUS status;	/* intermediate status */
	DWORD argument;		/* argument */
	SD_COMMAND_RESPONSE response;	/* IO response status */



	argument = BUILD_IO_RW_EXTENDED_ARG(SD_IO_OP_READ,
					    SD_IO_BYTE_MODE,
					    prSdInfo->Function,
					    u4Register, SD_IO_INCREMENT_ADDRESS, 4);

#if CFG_SDIO_PATHRU_MODE
	status = SD_SYNC_BUS_REQ(prSdInfo, SD_CMD_IO_RW_EXTENDED, argument, SD_READ, ResponseR5, &response, 1,	/* block number */
				 4,	/* block size */
				 (PUCHAR) pu4Value, 0);
#else
	status = SDSynchronousBusRequest(prSdInfo->hDevice, SD_CMD_IO_RW_EXTENDED, argument, SD_READ, ResponseR5, &response, 1,	/* block number */
					 4,	/* block size */
					 (PUCHAR) pu4Value, 0);
#endif
	return SD_API_SUCCESS(status) ? TRUE : FALSE;
}				/* kalDevRegRead */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write a 32 bit register value to device.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register the register offset.
* \param[out] u4Value The 32-bit value of the register to be written.
*
* \retval TRUE  success
* \retval FALSE failure
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevRegWrite(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, IN UINT_32 u4Value)
{
	P_GL_HIF_INFO_T prSdInfo = &prGlueInfo->rHifInfo;
	SD_API_STATUS status;	/* intermediate status */
	DWORD argument;		/* argument */
	SD_COMMAND_RESPONSE response;	/* IO response status */

	argument = BUILD_IO_RW_EXTENDED_ARG(SD_IO_OP_WRITE,
					    SD_IO_BYTE_MODE,
					    prSdInfo->Function,
					    u4Register, SD_IO_INCREMENT_ADDRESS, 4);

#if CFG_SDIO_PATHRU_MODE
	status = SD_SYNC_BUS_REQ(prSdInfo, SD_CMD_IO_RW_EXTENDED, argument, SD_WRITE, ResponseR5, &response, 1,	/* block number */
				 4,	/* block size */
				 (PUCHAR) & u4Value, 0);
#else
	status = SDSynchronousBusRequest(prSdInfo->hDevice, SD_CMD_IO_RW_EXTENDED, argument, SD_WRITE, ResponseR5, &response, 1,	/* block number */
					 4,	/* block size */
					 (PUCHAR) & u4Value, 0);
#endif
	return SD_API_SUCCESS(status) ? TRUE : FALSE;
}				/* kalDevRegWrite */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read port data from device in unit of 32 bit.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Port the register offset.
* \param[in] u4Len the number of byte to be read.
* \param[out] pucBuf Pointer to the buffer of the port been read.
* \param[in] u2ValidOutBufSize Length of the buffer valid to be accessed
*
* \retval TRUE  success
* \retval FALSE failure
*/
/*----------------------------------------------------------------------------*/
BOOL
kalDevPortRead(IN P_GLUE_INFO_T prGlueInfo,
	       IN UINT_32 u4Port,
	       IN UINT_32 u4Len, OUT PUINT_8 pucBuf, IN UINT_32 u4ValidOutBufSize)
{
	P_GL_HIF_INFO_T prSdInfo = &prGlueInfo->rHifInfo;
	SD_API_STATUS status = SD_API_STATUS_SUCCESS;	/* intermediate status */
	DWORD argument;		/* argument */
	SD_COMMAND_RESPONSE response;	/* IO response status */
	UINT_32 u4BlockNum, u4ByteNum;
	UINT_32 u4Bytes;

	ASSERT(u4ValidOutBufSize >= u4Len);

	/* Check if we apply block mode to transfer data */
	if (u4Len > 64 && prSdInfo->sdHostBlockCap.ReadBlockSize >= 32) {
		u4BlockNum = u4Len / prSdInfo->sdHostBlockCap.ReadBlockSize;
		u4ByteNum = u4Len % prSdInfo->sdHostBlockCap.ReadBlockSize;
	} else {
		/* Use byte mode */
		u4BlockNum = 0;
		u4ByteNum = u4Len;
	}

	/* Only use block mode to read all data */
	if (u4BlockNum != 0 && u4ByteNum != 0) {
		if (u4ValidOutBufSize >=
		    ((u4BlockNum + 1) * prSdInfo->sdHostBlockCap.ReadBlockSize)) {
			u4BlockNum++;
			u4ByteNum = 0;
		}
	}

	ASSERT(u4ValidOutBufSize >=
	       u4BlockNum * prSdInfo->sdHostBlockCap.ReadBlockSize + u4ByteNum);

	if (u4BlockNum > 0) {
		argument = BUILD_IO_RW_EXTENDED_ARG(SD_IO_OP_READ,
						    SD_IO_BLOCK_MODE,
						    prSdInfo->Function,
						    u4Port, SD_IO_FIXED_ADDRESS, u4BlockNum);

#if CFG_SDIO_PATHRU_MODE
		status = SD_SYNC_BUS_REQ(prSdInfo, SD_CMD_IO_RW_EXTENDED, argument, SD_READ, ResponseR5, &response, u4BlockNum,	/* block number */
					 prSdInfo->sdHostBlockCap.ReadBlockSize,	/* block size */
					 pucBuf, 0);
#else
		status = SDSynchronousBusRequest(prSdInfo->hDevice, SD_CMD_IO_RW_EXTENDED, argument, SD_READ, ResponseR5, &response, u4BlockNum,	/* block number */
						 prSdInfo->sdHostBlockCap.ReadBlockSize,	/* block size */
						 pucBuf, 0);
#endif
		pucBuf += (u4BlockNum * prSdInfo->sdHostBlockCap.ReadBlockSize);
	}

	while (u4ByteNum > 0 && SD_API_SUCCESS(status)) {
		u4Bytes = (u4ByteNum > MAX_SD_RW_BYTES) ? MAX_SD_RW_BYTES : u4ByteNum;
		argument = BUILD_IO_RW_EXTENDED_ARG(SD_IO_OP_READ,
						    SD_IO_BYTE_MODE,
						    prSdInfo->Function,
						    u4Port, SD_IO_FIXED_ADDRESS, u4Bytes);

#if CFG_SDIO_PATHRU_MODE
		status = SD_SYNC_BUS_REQ(prSdInfo, SD_CMD_IO_RW_EXTENDED, argument, SD_READ, ResponseR5, &response, 1,	/* block number */
					 u4Bytes,	/* block size */
					 (PUCHAR) pucBuf, 0);
#else
		status = SDSynchronousBusRequest(prSdInfo->hDevice, SD_CMD_IO_RW_EXTENDED, argument, SD_READ, ResponseR5, &response, 1,	/* block number */
						 u4Bytes,	/* block size */
						 (PUCHAR) pucBuf, 0);
#endif
		pucBuf += u4Bytes;
		u4ByteNum -= u4Bytes;
	}			/* end of while loop */

#ifdef X86_CPU
	/* use a non-used register to avoid the ENE bug */
	if (u4BlockNum && u4ByteNum == 0) {
		kalDevRegWrite(prGlueInfo, SDIO_X86_WORKAROUND_WRITE_MCR, 0x0);
	}
#endif

	return SD_API_SUCCESS(status) ? TRUE : FALSE;
}				/* kalDevPortRead */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write port data to device in unit of 32 bit.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Port the register offset.
* \param[in] u4Len the number of byte to be read.
* \param[in] pucBuf Pointer to the buffer of the port been read.
* \param[in] u4ValidInBufSize Length of the buffer valid to be accessed
*
* \retval TRUE  success
* \retval FALSE failure
*/
/*----------------------------------------------------------------------------*/
BOOL
kalDevPortWrite(IN P_GLUE_INFO_T prGlueInfo,
		IN UINT_32 u4Port, IN UINT_32 u4Len, IN PUINT_8 pucBuf, IN UINT_32 u4ValidInBufSize)
{
	P_GL_HIF_INFO_T prSdInfo = &prGlueInfo->rHifInfo;
	SD_API_STATUS status = SD_API_STATUS_SUCCESS;	/* intermediate status */
	DWORD argument;		/* argument */
	SD_COMMAND_RESPONSE response;	/* IO response status */
	UINT_32 u4BlockNum, u4ByteNum;
	UINT_32 u4Bytes;


	ASSERT(u4ValidInBufSize >= u4Len);

	/* Check if we apply block mode to transfer data */
	if (u4Len > 64 && prSdInfo->sdHostBlockCap.WriteBlockSize >= 32) {
		u4BlockNum = u4Len / prSdInfo->sdHostBlockCap.WriteBlockSize;
		u4ByteNum = u4Len % prSdInfo->sdHostBlockCap.WriteBlockSize;
	} else {
		/* Use byte mode */
		u4BlockNum = 0;
		u4ByteNum = u4Len;
	}

	/* Only use block mode to write all data */
	if (u4BlockNum != 0 && u4ByteNum != 0) {
		if (u4ValidInBufSize >=
		    ((u4BlockNum + 1) * prSdInfo->sdHostBlockCap.WriteBlockSize)) {
			u4BlockNum++;
			u4ByteNum = 0;
		}
	}

	ASSERT(u4ValidInBufSize >=
	       u4BlockNum * prSdInfo->sdHostBlockCap.WriteBlockSize + u4ByteNum);

	if (u4BlockNum > 0) {
		argument = BUILD_IO_RW_EXTENDED_ARG(SD_IO_OP_WRITE,
						    SD_IO_BLOCK_MODE,
						    prSdInfo->Function,
						    u4Port, SD_IO_FIXED_ADDRESS, u4BlockNum);

#if CFG_SDIO_PATHRU_MODE
		status = SD_SYNC_BUS_REQ(prSdInfo, SD_CMD_IO_RW_EXTENDED, argument, SD_WRITE, ResponseR5, &response, u4BlockNum,	/* block number */
					 prSdInfo->sdHostBlockCap.WriteBlockSize,	/* block size */
					 pucBuf, 0);
#else
		status = SDSynchronousBusRequest(prSdInfo->hDevice, SD_CMD_IO_RW_EXTENDED, argument, SD_WRITE, ResponseR5, &response, u4BlockNum,	/* block number */
						 prSdInfo->sdHostBlockCap.WriteBlockSize,	/* block size */
						 pucBuf, 0);
#endif
		pucBuf += (u4BlockNum * prSdInfo->sdHostBlockCap.WriteBlockSize);
	}

	while (u4ByteNum > 0 && SD_API_SUCCESS(status)) {
		u4Bytes = (u4ByteNum > MAX_SD_RW_BYTES) ? MAX_SD_RW_BYTES : u4ByteNum;
		argument = BUILD_IO_RW_EXTENDED_ARG(SD_IO_OP_WRITE,
						    SD_IO_BYTE_MODE,
						    prSdInfo->Function,
						    u4Port, SD_IO_FIXED_ADDRESS, u4Bytes);

#if CFG_SDIO_PATHRU_MODE
		status = SD_SYNC_BUS_REQ(prSdInfo, SD_CMD_IO_RW_EXTENDED, argument, SD_WRITE, ResponseR5, &response, 1,	/* block number */
					 u4Bytes,	/* block size */
					 (PUCHAR) pucBuf, 0);
#else
		status = SDSynchronousBusRequest(prSdInfo->hDevice, SD_CMD_IO_RW_EXTENDED, argument, SD_WRITE, ResponseR5, &response, 1,	/* block number */
						 u4Bytes,	/* block size */
						 (PUCHAR) pucBuf, 0);
#endif
		pucBuf += u4Bytes;
		u4ByteNum -= u4Bytes;
	}			/* end of while loop */

#ifdef X86_CPU
	/* use a non-used register to avoid the ENE bug */
	if (u4BlockNum && u4ByteNum == 0) {
		kalDevRegWrite(prGlueInfo, SDIO_X86_WORKAROUND_WRITE_MCR, 0x0);
	}
#endif

	return SD_API_SUCCESS(status) ? TRUE : FALSE;
}				/* kalDevPortWrite */


/*----------------------------------------------------------------------------*/
/*!
* \brief Write device I/O port in byte with CMD52
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u4Addr             I/O port offset
* \param[in] ucData             Single byte of data to be written
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevWriteWithSdioCmd52(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Addr, IN UINT_8 ucData)
{
	P_GL_HIF_INFO_T prSdInfo = &prGlueInfo->rHifInfo;
	SD_API_STATUS status = SD_API_STATUS_SUCCESS;	/* intermediate status */
	UINT_8 ucRwBuffer = ucData;

	status = SDReadWriteRegistersDirect(prSdInfo->hDevice,
					    SD_WRITE,
					    prSdInfo->Function,
					    u4Addr, TRUE, &ucRwBuffer, sizeof(UINT_8));

	return SD_API_SUCCESS(status) ? TRUE : FALSE;
}


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write port data to device in unit of 32 bit.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Port the register offset.
* \param[in] u4Len the number of byte to be read.
* \param[in] pucBuf Pointer to the buffer of the port been read.
* \param[in] u4ValidInBufSize Length of the buffer valid to be accessed
*
* \retval TRUE  success
* \retval FALSE failure
*/
/*----------------------------------------------------------------------------*/
BOOL
kalDevReadAfterWriteWithSdioCmd52(IN P_GLUE_INFO_T prGlueInfo,
				  IN UINT_32 u4Addr,
				  IN OUT PUINT_8 pucRwBuffer, IN UINT_32 u4RwBufLen)
{
	P_GL_HIF_INFO_T prSdInfo = &prGlueInfo->rHifInfo;
	SD_API_STATUS status = SD_API_STATUS_SUCCESS;	/* intermediate status */

	status = SDReadWriteRegistersDirect(prSdInfo->hDevice,
					    SD_WRITE,
					    prSdInfo->Function,
					    u4Addr, TRUE, pucRwBuffer, u4RwBufLen);

	return SD_API_SUCCESS(status) ? TRUE : FALSE;
}
