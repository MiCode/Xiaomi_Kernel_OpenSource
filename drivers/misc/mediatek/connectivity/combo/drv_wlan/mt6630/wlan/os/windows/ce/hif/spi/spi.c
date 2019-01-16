/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/ce/hif/spi/spi.c#1 $
*/

/*! \file   spi.c
    \brief  This file contains the implementation of SPI glue layer
	    function for Windows.
*/



/*
** $Log: spi.c $
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
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-05-15 10:12:46 GMT mtk01461
**  Fix Port Read Error
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-04-27 12:17:20 GMT mtk01104
**  Add spin-lock protection for SPI bus access
**  \main\maintrunk.MT6620WiFiDriver_Prj\1 2009-04-24 21:13:43 GMT mtk01104
**  Initial version
*/

/*******************************************************************************
*                E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"

LINT_EXT_HEADER_BEGIN 
#include <ceddk.h>
    LINT_EXT_HEADER_END
#include "hif.h"
/*******************************************************************************
*                          C O N S T A N T S
********************************************************************************
*/
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
#ifdef _lint
BOOLEAN InterruptInitialize(UINT_32, PVOID, UINT_32, UINT_32);


NDIS_STATUS NdisMRegisterInterrupt(PVOID, UINT_32, UINT_32, UINT_32, BOOLEAN,	/* RequestIsr */
				   BOOLEAN,	/* SharedInterrupt */
				   UINT_32);

BOOLEAN
KernelIoControl(UINT_32 dwIoControlCode,
		PUCHAR lpInBuf,
		UINT_32 nInBufSize, PVOID lpOutBuf, UINT_32 nOutBufSize, PUINT_32 lpBytesReturned);

BOOL CloseHandle(UINT_32 hObject);

BOOL DisableThreadLibraryCalls(UINT_32 hModule);

VOID CTL_CODE(UINT_32 DeviceType, UINT_32 Access, UINT_32 Function, UINT_32 Method);

VOID NdisMDeregisterInterrupt(IN PUINT_32 Interrupt);

VOID
NdisWriteErrorLogEntry(IN UINT_32 NdisAdapterHandle,
		       IN UINT_32 ErrorCode, IN ULONG NumberOfErrorValues);

UINT_32
CreateEvent(PUINT_32 lpEventAttributes, BOOL bManualReset, BOOL bInitialState, PVOID lpName);

#endif				/* end of _lint */


/*******************************************************************************
*                          F U N C T I O N S
********************************************************************************
*/

LINT_EXT_HEADER_BEGIN 
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to initialize adapter members.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] rWrapperConfigurationContext windows wrapper configuration context
*
* \return NDIS_STATUS code
*
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
windowsFindAdapter(IN P_GLUE_INFO_T prGlueInfo, IN NDIS_HANDLE rWrapperConfigurationContext)
{

	ASSERT(prGlueInfo);

	platformBusInit(prGlueInfo);

	return NDIS_STATUS_SUCCESS;
}				/* windowsFindAdapter */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to register interrupt call back function.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsRegisterIsrt(IN P_GLUE_INFO_T prGlueInfo)
{
	NDIS_STATUS rStatus;
	P_GL_HIF_INFO_T prHifInfo;

	DEBUGFUNC("windowsRegisterIsrt");

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	/* Next we'll register our interrupt with the NDIS wrapper. */
	/* Hook our interrupt vector.  We used level-triggered, shared
	   interrupts with our PCI adapters. */
	INITLOG(("Register IRQ: handle=0x%x, irq=0x%x, level-triggered\n",
		 prGlueInfo->rMiniportAdapterHandle, prHifInfo->u4InterruptLevel));

	rStatus = NdisMRegisterInterrupt(&prHifInfo->rInterrupt, prGlueInfo->rMiniportAdapterHandle, (UINT) prHifInfo->u4InterruptVector, (UINT) prHifInfo->u4InterruptLevel, TRUE,	/* RequestIsr */
					 FALSE,	/* SharedInterrupt */
					 NIC_INTERRUPT_MODE);

	if (rStatus != NDIS_STATUS_SUCCESS) {
		ERRORLOG(("Interrupt conflict: status=0x%08x, IRQ=%d, level-sensitive\n",
			  rStatus, prHifInfo->u4InterruptLevel));

		NdisWriteErrorLogEntry(prGlueInfo->rMiniportAdapterHandle,
				       NDIS_ERROR_CODE_INTERRUPT_CONNECT, 1,
				       (UINT_32) prHifInfo->u4InterruptLevel);
	}

	GLUE_SET_FLAG(prGlueInfo, GLUE_FLAG_INTERRUPT_IN_USE);

	INITLOG(("Register interrupt -- OK\n"));

#if SC32442_SPI
	{
		UINT_32 g_SysIntr;

		prHifInfo->gWaitEvent = CreateEvent(NULL, FALSE, FALSE, NULL);

		if (!KernelIoControl(IOCTL_HAL_REQUEST_SYSINTR,
				     &prHifInfo->u4InterruptLevel,
				     sizeof(UINT_32), &g_SysIntr, sizeof(UINT_32), NULL)) {
			INITLOG(("ERROR:Failed to request sysintr value for Timer1 inturrupt!/n"));
		} else {
			INITLOG(("Request sysintr value %d!/r/n", g_SysIntr));
			prHifInfo->u4sysIntr = g_SysIntr;
		}

		if (!(InterruptInitialize(g_SysIntr, prHifInfo->gWaitEvent, 0, 0))) {
			INITLOG(("ERROR: Interrupt initialize failed.\n"));
		}
	}
#endif

	return rStatus;
}				/* windowsRegisterIsrt */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to un-register interrupt call back function.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsUnregisterIsrt(IN P_GLUE_INFO_T prGlueInfo)
{
	P_GL_HIF_INFO_T prHifInfo;

	DEBUGFUNC("windowsUnregisterIsrt");

	ASSERT(prGlueInfo);
	prHifInfo = &prGlueInfo->rHifInfo;

	if (GLUE_TEST_FLAG(prGlueInfo, GLUE_FLAG_INTERRUPT_IN_USE)) {
		NdisMDeregisterInterrupt(&prHifInfo->rInterrupt);
		GLUE_CLEAR_FLAG(prGlueInfo, GLUE_FLAG_INTERRUPT_IN_USE);
		INITLOG(("Interrupt deregistered\n"));
	}

	return NDIS_STATUS_SUCCESS;
}				/* windowsUnregisterIsrt */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to allocate memory mapping for the HW register.
*        We don't need it for SPI interface. If for memory concern, we can
*        define it as nothing in header file.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsMapAllocateRegister(IN P_GLUE_INFO_T prGlueInfo)
{
	return NDIS_STATUS_SUCCESS;
}				/* windowsMapAllocateRegister */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to free memory mapping for the HW register.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsUMapFreeRegister(IN P_GLUE_INFO_T prGlueInfo)
{
	/* Restore GPIO setting */
	GPIOConfigRestore();

	return NDIS_STATUS_SUCCESS;
}				/* windowsUnapFreeRegister */


/*----------------------------------------------------------------------------*/
/*!
* \brief Standard Windows DLL entrypoint. Since Windows CE NDIS.
*        miniports are implemented as DLLs, a DLL entrypoint is needed
*
* \param[in] hDll - handle to the DLL module.
* \param[in] u4Reason - Dll action
*
* \retval 0 TRUE
* \retval -1 FALSE
*/
/*----------------------------------------------------------------------------*/
BOOL DllEntry(HANDLE hDLL, DWORD u4Reason, LPVOID lpReserved)
{
	DEBUGFUNC("DllEntry");

	switch (u4Reason) {
	case DLL_PROCESS_ATTACH:
		CREATE_LOG_FILE();
		INITLOG(("MT6620: DLL_PROCESS_ATTACH\n"));
		DisableThreadLibraryCalls((HMODULE) hDLL);
		break;
	case DLL_PROCESS_DETACH:
		INITLOG(("MT6620: DLL_PROCESS_DETTACH\n"));
		break;
	default:
		INITLOG(("MT6620: Fail\n"));
		break;
	}
	return TRUE;
}				/* DllEntry */

LINT_EXT_HEADER_END
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read a 32 bit register value from device.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register the register offset.
* \param[out] pu4Value Pointer to the 32-bit value of the register been read.
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevRegRead(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, OUT PUINT_32 pu4Value)
{
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_SPI_ACCESS);

	SpiSendCmd32(SPI_FUN_RD, u4Register, pu4Value);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_SPI_ACCESS);

	return TRUE;
}				/* kalDevRegRead */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write a 32 bit register value to device.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register the register offset.
* \param[out] u4Value The 32-bit value of the register to be written.
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevRegWrite(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, IN UINT_32 u4Value)
{
	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_SPI_ACCESS);

	SpiSendCmd32(SPI_FUN_WR, u4Register, (PUINT_32) &u4Value);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_SPI_ACCESS);

	return TRUE;
}				/* kalDevRegWrite */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read port data from device in unit of 32 bit.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port the register offset.
* \param[in] u2Len the number of byte to be read.
* \param[out] pucBuf Pointer to the buffer of the port been read.
* \param[in] u2ValidOutBufSize Length of the buffer valid to be accessed
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
BOOL
kalDevPortRead(IN P_GLUE_INFO_T prGlueInfo,
	       IN UINT_32 u4Port,
	       IN UINT_32 u4Len, OUT PUINT_8 pucBuf, IN UINT_32 u4ValidOutBufSize)
{
	ASSERT(u4Len);

#if DBG
	if (IS_NOT_ALIGN_4((UINT_32) pucBuf) || (u4Len & 0x03)) {
		DBGLOG(HAL, ERROR,
		       ("kalDevPortRead error, address: 0x%p, len:%d!\n", pucBuf, u4Len));
		return FALSE;
	}
#endif

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_SPI_ACCESS);

	SpiReadWriteData32(SPI_FUN_RD, u4Port, pucBuf, u4Len);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_SPI_ACCESS);

	return TRUE;
}				/* kalDevPortRead */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write port data to device in unit of 32 bit.
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port the register offset.
* \param[in] u2Len the number of byte to be read.
* \param[in] pucBuf Pointer to the buffer of the port been read.
* \param[in] u2ValidInBufSize Length of the buffer valid to be accessed
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
BOOL
kalDevPortWrite(IN P_GLUE_INFO_T prGlueInfo,
		IN UINT_32 u4Port, IN UINT_32 u4Len, IN PUINT_8 pucBuf, IN UINT_32 u4ValidInBufSize)
{
	ASSERT(u4Len);

#if DBG
	if (((unsigned long)pucBuf & 0x03) || (u4Len & 0x03)) {
		DBGLOG(HAL, ERROR,
		       ("kalDevPortWrite error, address: 0x%p, len:%d!\n", pucBuf, u4Len));
		return FALSE;
	}
#endif

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_SPI_ACCESS);

	SpiReadWriteData32(SPI_FUN_WR, u4Port, pucBuf, u4Len);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_SPI_ACCESS);

	return TRUE;
}				/* kalDevPortWrite */
