/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/ddk/hif/sdio/sdio.c#1 $
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
 * 11 08 2010 cp.wu
 * [WCXRP00000166] [MT6620 Wi-Fi][Driver] use SDIO CMD52 for enabling/disabling interrupt to reduce transaction period
 * change to use CMD52 for enabling/disabling interrupt to reduce SDIO transaction time
 *
 * 09 01 2010 cp.wu
 * NULL
 * move HIF CR initialization from where after sdioSetupCardFeature() to wlanAdapterStart()
 *
 * 08 04 2010 cp.wu
 * NULL
 * fix for check build WHQL testing:
 * 1) do not assert query buffer if indicated buffer length is zero
 * 2) sdio.c has bugs which cause freeing same pointer twice
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 02 11 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. add logic for firmware download
 *  *  *  *  *  * 2. firmware image filename and start/load address are now retrieved from registry
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 *  *  *  *  *  *  *  *  *  *  *  *  *  * 2. block TX/ordinary OID when RF test mode is engaged
 *  *  *  *  *  *  *  *  *  *  *  *  *  * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 *  *  *  *  *  *  *  *  *  *  *  *  *  * 4. correct some HAL implementation
 *
 * 01 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * remove unused code, apply ENE workaround for port read as well
 *
 * 12 18 2009 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * use 16500KHz instead of 8250KHz for SDIO clock
**  \main\maintrunk.MT6620WiFiDriver_Prj\10 2009-12-16 18:00:51 GMT mtk02752
**  set read-clear status inside windowsFindAdapter()
**  \main\maintrunk.MT6620WiFiDriver_Prj\9 2009-12-04 17:59:17 GMT mtk02752
**  to pass free-build compilation check
**  \main\maintrunk.MT6620WiFiDriver_Prj\8 2009-11-26 10:17:32 GMT mtk02752
**  WDK 6001.18002 have no idea about SDBUS_CALLBACK_ROUTINE
**  \main\maintrunk.MT6620WiFiDriver_Prj\7 2009-11-26 09:39:46 GMT mtk02752
**  surpress PERfast warning
**  \main\maintrunk.MT6620WiFiDriver_Prj\6 2009-11-11 10:36:27 GMT mtk01084
**  set event on ISR
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-11-09 22:56:46 GMT mtk01084
**  modify SDIO HW access routines
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-10-23 16:08:42 GMT mtk01084
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-10-13 22:14:26 GMT mtk01084
**  fix data-type warning
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-10-13 21:59:34 GMT mtk01084
**  modify bus access functions
**  \main\maintrunk.MT6620WiFiDriver_Prj\1 2009-09-09 17:30:11 GMT mtk01084
**  \main\maintrunk.MT5921\26 2008-12-08 16:14:44 GMT mtk01461
**  Revise kalDevPortWrite/Read by checking u2ValidInBufSize before using additional Block instead of Byte mode.
**  \main\maintrunk.MT5921\25 2008-10-22 11:07:07 GMT mtk01461
**  Update for lint diagnosis support
**  \main\maintrunk.MT5921\24 2008-10-16 15:44:58 GMT mtk01461
**  Move the error handling routine of Release Tx Pending Packets from oidThread() to mpPnPEventNotify()
**  \main\maintrunk.MT5921\23 2008-10-13 02:21:26 GMT mtk01461
**  Add Sdio Block Mode support for XP SP3 (Only Byte Mode get works at XP SP2)
**  \main\maintrunk.MT5921\22 2008-09-22 13:19:10 GMT mtk01461
**  Update for code review
**  \main\maintrunk.MT5921\21 2008-09-03 13:39:44 GMT mtk01084
**  add input buffer length check
**  \main\maintrunk.MT5921\20 2008-08-10 18:46:49 GMT mtk01461
**  Update for Driver Review
**  \main\maintrunk.MT5921\19 2008-07-16 14:48:57 GMT mtk01461
**  Revise MiniportReset() for SDIO in WinXP
**  \main\maintrunk.MT5921\18 2008-06-02 23:23:07 GMT mtk01461
**  Remove DbgPrint
**  \main\maintrunk.MT5921\17 2008-05-29 14:15:52 GMT mtk01084
**  modify the process of OID handler in oidThread()
**  \main\maintrunk.MT5921\16 2008-05-28 11:04:51 GMT mtk01461
**  Remove DbgPrint
**  \main\maintrunk.MT5921\15 2008-05-27 09:40:04 GMT mtk01461
**  Revise oidThread() for wiating RX return packet during halt process
**  \main\maintrunk.MT5921\14 2008-05-23 10:29:19 GMT mtk01084
**  modify wlanISR interface
**  \main\maintrunk.MT5921\13 2008-05-22 10:55:25 GMT mtk01461
**  Revise OID thread to handle returned RX packet while MP Halt.
**  \main\maintrunk.MT5921\12 2008-05-03 15:44:58 GMT mtk01461
**  Move IST process to OID thread to overcome unbalanced thread priority
**  \main\maintrunk.MT5921\11 2008-04-22 22:59:08 GMT mtk01084
**  modify port access function prototype
**  \main\maintrunk.MT5921\10 2008-03-26 23:53:00 GMT mtk01084
**  remove assertion on port access
**  \main\maintrunk.MT5921\9 2008-03-17 10:23:48 GMT mtk01084
**  add function description
**  \main\maintrunk.MT5921\8 2008-03-14 18:03:23 GMT mtk01084
**  refine register and port access function
**  \main\maintrunk.MT5921\7 2008-03-14 17:39:50 GMT mtk01385
**  1. fix pointer check code in OID set function in oidthread.
**  \main\maintrunk.MT5921\6 2008-03-03 11:18:44 GMT mtk01385
**  1. split DDK queue spin lock protection into tx/rx queue two groups.
**  \main\maintrunk.MT5921\5 2008-02-25 16:38:15 GMT mtk01385
**  1. Remove DbgPrint function call in Non-Debug Build code.
**  \main\maintrunk.MT5921\4 2008-02-25 15:53:39 GMT mtk01385
**  1. fix oid return length for 64 bit oid.
**  \main\maintrunk.MT5921\3 2008-02-25 14:55:00 GMT mtk01385
**  1. XP ENE only support 64 bytes CMD53 byte mode.
**  \main\maintrunk.MT5921\2 2008-02-25 12:08:11 GMT mtk01385
**  1. fix SPIN lock at DISPATCH level issue on XP SDIO.
**  \main\maintrunk.MT5921\1 2007-11-06 20:14:26 GMT mtk01385
**  init version.
** Revision 1.1  2007/07/05 14:06:38  MTK01385
** Initial version
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
/* This definition can not be larger than 512 */
#define MAX_SD_RW_BYTES         512

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
VOID oidThread(IN PVOID pvGlueContext);

VOID
sdioMpSendPackets(IN NDIS_HANDLE miniportAdapterContext,
		  IN PPNDIS_PACKET packetArray_p, IN UINT numberOfPackets);


VOID emuInitChkCis(IN P_ADAPTER_T prAdapter);

VOID emuChkIntEn(IN P_ADAPTER_T prAdapter);

#if 0
/* WDK 6001.18002 didn't support line below */
SDBUS_CALLBACK_ROUTINE sdioINTerruptCallback;
#endif

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief A function for SDIO command 52 to read/write a single byte
*
* \param[in] prDx       Pointer to DEVICE EXTENSION Data Structure
* \param[in] u4Address  Address to read/write.
* \param[in] pucData    Pointer to Data for read/write.
* \param[in] ucFuncNo   The number of the function on the I/O card.
* \param[in] rRwFlag    Specify Read or Write Direction.
*
* \retval TRUE      Successfully Read/Write
* \retval FALSE     Fail to Read/Write
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
sdioCmd52ByteReadWrite(PDEVICE_EXTENSION prDx,
		       UINT_32 u4Address,
		       PUCHAR pucData, UCHAR ucFuncNo, SD_TRANSFER_DIRECTION rRwFlag)
{
	PSDBUS_REQUEST_PACKET prSDRP = (PSDBUS_REQUEST_PACKET) NULL;
	SD_RW_DIRECT_ARGUMENT rSdIoArgument;
	NDIS_STATUS rStatus;
	const SDCMD_DESCRIPTOR rReadWriteIoDirectDesc = { SDCMD_IO_RW_DIRECT,
		SDCC_STANDARD,
		rRwFlag,
		SDTT_CMD_ONLY,
		SDRT_5
	};


	ASSERT(prDx);
	ASSERT(pucData);

	prSDRP = ExAllocatePoolWithTag(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET), 'SDIO');

	if (!prSDRP) {
		ERRORLOG(("ExAllocatePool prSDRP  fail!\n"));
		return FALSE;
	}

	RtlZeroMemory(prSDRP, sizeof(SDBUS_REQUEST_PACKET));

	prSDRP->RequestFunction = SDRF_DEVICE_COMMAND;
	prSDRP->Parameters.DeviceCommand.CmdDesc = rReadWriteIoDirectDesc;

	/* Set up the argument and command descriptor */
	rSdIoArgument.u.AsULONG = 0;
	rSdIoArgument.u.bits.Address = u4Address;

	/* Function # must be initialized by SdBus GetProperty call */
	rSdIoArgument.u.bits.Function = ucFuncNo;

	/* Submit the request */
	if (rRwFlag == SDTD_WRITE) {
		rSdIoArgument.u.bits.WriteToDevice = 1;
		rSdIoArgument.u.bits.Data = *pucData;
	}

	prSDRP->Parameters.DeviceCommand.Argument = rSdIoArgument.u.AsULONG;

	rStatus = SdBusSubmitRequest(prDx->BusInterface.Context, prSDRP);

	if (rRwFlag == SDTD_READ) {
		*pucData = prSDRP->ResponseData.AsUCHAR[0];
	}

	ExFreePool(prSDRP);

	if (NT_SUCCESS(rStatus)) {
		return TRUE;
	} else {
		if (rRwFlag == SDTD_READ) {
			ERRORLOG(("CMD52 RD FAIL!, status:%x\n", rStatus));
		} else {
			ERRORLOG(("CMD52 WR FAIL!, status:%x\n", rStatus));
		}
		return FALSE;
	}

}				/* end of sdioCmd52ByteReadWrite() */


/*----------------------------------------------------------------------------*/
/*!
* \brief A function for SDIO command 53 to write data by Byte Mode.
*
* \param[in] prDx           Pointer to DEVICE EXTENSION Data Structure
* \param[in] pucBuffer      Pointer to data buffer for write
* \param[in] u4Address      Address to write.
* \param[in] u2ByteCount    The length of data buffer to write
*
* \retval TRUE      Successfully Read/Write
* \retval FALSE     Fail to Read/Write
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
sdioCmd53ByteWrite(PDEVICE_EXTENSION prDx, PUCHAR pucBuffer, UINT_32 u4Address, UINT_16 u2ByteCount)
{
	PSDBUS_REQUEST_PACKET prSDRP = (PSDBUS_REQUEST_PACKET) NULL;
	SD_RW_EXTENDED_ARGUMENT rSdIoArgument;
	PMDL prMdl = (PMDL) NULL;
	NDIS_STATUS rStatus;
	const SDCMD_DESCRIPTOR rReadWriteIoExtendedDesc = { SDCMD_IO_RW_EXTENDED,
		SDCC_STANDARD,
		SDTD_WRITE,
		SDTT_SINGLE_BLOCK,
		SDRT_5
	};


	/* First get a MDL to map the data. This code assumes the
	 * caller passed a buffer to nonpaged pool.
	 */
	prMdl = IoAllocateMdl(pucBuffer, u2ByteCount, FALSE, FALSE, NULL);

	if (!prMdl) {
		ERRORLOG(("IoAllocateMdl prMdl  fail!\n"));
		return FALSE;
	}
	MmBuildMdlForNonPagedPool(prMdl);

	/* Now allocate a request packet for the arguments of the command */
	prSDRP = ExAllocatePoolWithTag(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET), 'SDIO');
	if (!prSDRP) {
		IoFreeMdl(prMdl);
		ERRORLOG(("ExAllocatePool prSDRP  fail!\n"));
		return FALSE;
	}

	RtlZeroMemory(prSDRP, sizeof(SDBUS_REQUEST_PACKET));

	prSDRP->RequestFunction = SDRF_DEVICE_COMMAND;
	prSDRP->Parameters.DeviceCommand.CmdDesc = rReadWriteIoExtendedDesc;

	/* Set up the argument and command descriptor */
	rSdIoArgument.u.AsULONG = 0;
	rSdIoArgument.u.bits.Count = u2ByteCount;
	rSdIoArgument.u.bits.Address = u4Address;
	rSdIoArgument.u.bits.OpCode = 0;

	/* function # must be initialized by SdBus GetProperty call */
	rSdIoArgument.u.bits.Function = prDx->FunctionNumber;

	/* Submit the request */
	rSdIoArgument.u.bits.WriteToDevice = 1;
	prSDRP->Parameters.DeviceCommand.Argument = rSdIoArgument.u.AsULONG;
	prSDRP->Parameters.DeviceCommand.Mdl = prMdl;
	prSDRP->Parameters.DeviceCommand.Length = u2ByteCount;

	rStatus = SdBusSubmitRequest(prDx->BusInterface.Context, prSDRP);

	IoFreeMdl(prMdl);
	ExFreePool(prSDRP);

	if (NT_SUCCESS(rStatus)) {
		return TRUE;
	} else {
		ERRORLOG(("CMD53 BYTE WR FAIL!, addr: %#08lx, status: %x\n", u4Address, rStatus));
		return FALSE;
	}

}				/* end of sdioCmd53byteWrite() */


/*----------------------------------------------------------------------------*/
/*!
* \brief A function for SDIO command 53 to read data by Byte Mode.
*
* \param[in] prDx           Pointer to DEVICE EXTENSION Data Structure
* \param[in] pucBuffer      Pointer to data buffer for write
* \param[in] u4Address      Address to write.
* \param[in] u2ByteCount    The length of data buffer to write
*
* \retval TRUE      Successfully Read/Write
* \retval FALSE     Fail to Read/Write
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
sdioCmd53ByteRead(PDEVICE_EXTENSION prDx, PUCHAR pucBuffer, UINT_32 u4Address, UINT_16 u2ByteCount)
{
	PSDBUS_REQUEST_PACKET prSDRP = (PSDBUS_REQUEST_PACKET) NULL;
	SD_RW_EXTENDED_ARGUMENT rSdIoArgument;
	PMDL prMdl = (PMDL) NULL;
	NDIS_STATUS rStatus;
	const SDCMD_DESCRIPTOR rReadWriteIoExtendedDesc = { SDCMD_IO_RW_EXTENDED,
		SDCC_STANDARD,
		SDTD_READ,
		SDTT_SINGLE_BLOCK,
		SDRT_5
	};


	/* First get a MDL to map the data. This code assumes the
	 * caller passed a buffer to nonpaged pool.
	 */
	prMdl = IoAllocateMdl(pucBuffer, u2ByteCount, FALSE, FALSE, NULL);

	if (!prMdl) {
		ERRORLOG(("IoAllocateMdl prMdl  fail!\n"));
		return FALSE;
	}
	MmBuildMdlForNonPagedPool(prMdl);

	/* Now allocate a request packet for the arguments of the command */
	prSDRP = ExAllocatePoolWithTag(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET), 'SDIO');
	if (!prSDRP) {
		IoFreeMdl(prMdl);
		ERRORLOG(("ExAllocatePool prSDRP  fail!\n"));
		return FALSE;
	}

	RtlZeroMemory(prSDRP, sizeof(SDBUS_REQUEST_PACKET));

	prSDRP->RequestFunction = SDRF_DEVICE_COMMAND;
	prSDRP->Parameters.DeviceCommand.CmdDesc = rReadWriteIoExtendedDesc;

	/* Set up the argument and command descriptor */
	rSdIoArgument.u.AsULONG = 0;
	rSdIoArgument.u.bits.Count = u2ByteCount;
	rSdIoArgument.u.bits.Address = u4Address;
	rSdIoArgument.u.bits.OpCode = 0;

	/* Function # must be initialized by SdBus GetProperty call */
	rSdIoArgument.u.bits.Function = prDx->FunctionNumber;

	/* Submit the request */
	/* rSdIoArgument.u.bits.WriteToDevice = 0; */
	prSDRP->Parameters.DeviceCommand.Argument = rSdIoArgument.u.AsULONG;
	prSDRP->Parameters.DeviceCommand.Mdl = prMdl;
	prSDRP->Parameters.DeviceCommand.Length = u2ByteCount;

	rStatus = SdBusSubmitRequest(prDx->BusInterface.Context, prSDRP);

	IoFreeMdl(prMdl);
	ExFreePool(prSDRP);

	if (NT_SUCCESS(rStatus)) {
		return TRUE;
	} else {
		ERRORLOG(("CMD53 BYTE RD FAIL!, addr: %#08lx, status: %x\n", u4Address, rStatus));
		return FALSE;
	}

}				/* end of sdioCmd53byteRead() */


/*----------------------------------------------------------------------------*/
/*!
* \brief A function for SDIO command 53 to read data by Block Mode.
*
* \param[in] prDx           Pointer to DEVICE EXTENSION Data Structure
* \param[in] u2Port         Address to write.
* \param[in] pucBuffer      Pointer to data buffer for write
* \param[in] ucBlockNo      The number of block to write
*
* \retval TRUE      Successfully Read/Write
* \retval FALSE     Fail to Read/Write
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
sdioCmd53BlockRead(PDEVICE_EXTENSION prDx, UINT_16 u2Port, PUCHAR pucBuffer, UINT_16 u4ByteCount)
{
	PSDBUS_REQUEST_PACKET prSDRP = (PSDBUS_REQUEST_PACKET) NULL;
	SD_RW_EXTENDED_ARGUMENT rSdIoArgument;
	PMDL prMdl = (PMDL) NULL;
	NDIS_STATUS rStatus;
	const SDCMD_DESCRIPTOR rReadWriteIoExtendedDesc = { SDCMD_IO_RW_EXTENDED,
		SDCC_STANDARD,
		SDTD_READ,
		SDTT_MULTI_BLOCK_NO_CMD12,
		SDRT_5
	};


	ASSERT(prDx);
	ASSERT(pucBuffer);

	/* First get a MDL to map the data. This code assumes the
	 * caller passed a buffer to nonpaged pool.
	 */
	prMdl = IoAllocateMdl(pucBuffer, u4ByteCount, FALSE, FALSE, NULL);

	if (!prMdl) {
		ERRORLOG(("IoAllocateMdl prMdl  fail!\n"));
		return FALSE;
	}
	MmBuildMdlForNonPagedPool(prMdl);

	/* Now allocate a request packet for the arguments of the command */
	prSDRP = ExAllocatePoolWithTag(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET), 'SDIO');
	if (!prSDRP) {
		IoFreeMdl(prMdl);
		ERRORLOG(("ExAllocatePool prSDRP  fail!\n"));
		return FALSE;
	}

	RtlZeroMemory(prSDRP, sizeof(SDBUS_REQUEST_PACKET));

	prSDRP->RequestFunction = SDRF_DEVICE_COMMAND;
	prSDRP->Parameters.DeviceCommand.CmdDesc = rReadWriteIoExtendedDesc;

	/* Set up the argument and command descriptor */
	rSdIoArgument.u.AsULONG = 0;
	rSdIoArgument.u.bits.Count = u4ByteCount;	/* Will be ignored when submitting a device command. */
	rSdIoArgument.u.bits.Address = u2Port;
	rSdIoArgument.u.bits.OpCode = 0;
	rSdIoArgument.u.bits.BlockMode = 1;	/* NOTE(Kevin): Block Mode in XP SP2 test fail and crash. */

	/* Function # must be initialized by SdBus GetProperty call */
	rSdIoArgument.u.bits.Function = prDx->FunctionNumber;

	/* Submit the request */
	/* rSdIoArgument.u.bits.WriteToDevice = 0; */
	prSDRP->Parameters.DeviceCommand.Argument = rSdIoArgument.u.AsULONG;
	prSDRP->Parameters.DeviceCommand.Mdl = prMdl;
	prSDRP->Parameters.DeviceCommand.Length = u4ByteCount;

	rStatus = SdBusSubmitRequest(prDx->BusInterface.Context, prSDRP);

	IoFreeMdl(prMdl);
	ExFreePool(prSDRP);

	if (NT_SUCCESS(rStatus)) {
		return TRUE;
	} else {
		ERRORLOG(("CMD53 BLOCK RD FAIL!, addr: %#04x, status: %x\n", u2Port, rStatus));
		return FALSE;
	}
}				/* end of sdioCmd53BlockRead() */


/*----------------------------------------------------------------------------*/
/*!
* \brief A function for SDIO command 53 to write data by Block Mode.
*
* \param[in] prDx           Pointer to DEVICE EXTENSION Data Structure
* \param[in] u2Port         Address to write.
* \param[in] pucBuffer      Pointer to data buffer for write
* \param[in] ucBlockNo      The number of block to write
*
* \retval TRUE      Successfully Read/Write
* \retval FALSE     Fail to Read/Write
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
sdioCmd53BlockWrite(PDEVICE_EXTENSION prDx, UINT_16 u2Port, PUCHAR pucBuffer, UINT_16 u4ByteCount)
{
	PSDBUS_REQUEST_PACKET prSDRP = (PSDBUS_REQUEST_PACKET) NULL;
	SD_RW_EXTENDED_ARGUMENT rSdIoArgument;
	PMDL prMdl = (PMDL) NULL;
	NDIS_STATUS rStatus;
	const SDCMD_DESCRIPTOR rReadWriteIoExtendedDesc = { SDCMD_IO_RW_EXTENDED,
		SDCC_STANDARD,
		SDTD_WRITE,
		SDTT_MULTI_BLOCK_NO_CMD12,
		SDRT_5
	};


	ASSERT(prDx);
	ASSERT(pucBuffer);

	/* First get a MDL to map the data. This code assumes the
	 * caller passed a buffer to nonpaged pool.
	 */
	prMdl = IoAllocateMdl(pucBuffer, u4ByteCount, FALSE, FALSE, NULL);
	if (!prMdl) {
		ERRORLOG(("IoAllocateMdl prMdl  fail!\n"));
		return FALSE;
	}
	MmBuildMdlForNonPagedPool(prMdl);

	/* Now allocate a request packet for the arguments of the command */
	prSDRP = ExAllocatePoolWithTag(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET), 'SDIO');

	if (!prSDRP) {
		IoFreeMdl(prMdl);
		ERRORLOG(("ExAllocatePool prSDRP  fail!\n"));
		return FALSE;
	}

	RtlZeroMemory(prSDRP, sizeof(SDBUS_REQUEST_PACKET));

	prSDRP->RequestFunction = SDRF_DEVICE_COMMAND;
	prSDRP->Parameters.DeviceCommand.CmdDesc = rReadWriteIoExtendedDesc;

	/* Set up the argument and command descriptor */
	rSdIoArgument.u.AsULONG = 0;
	rSdIoArgument.u.bits.Count = u4ByteCount;	/* Will be ignored when submitting a device command. */
	rSdIoArgument.u.bits.Address = u2Port;
	rSdIoArgument.u.bits.OpCode = 0;
	rSdIoArgument.u.bits.BlockMode = 1;	/* NOTE(Kevin): Block Mode in XP SP2 test fail and crash. */


	/* Function # must be initialized by SdBus GetProperty call */
	rSdIoArgument.u.bits.Function = prDx->FunctionNumber;

	/* Submit the request */
	rSdIoArgument.u.bits.WriteToDevice = 1;
	prSDRP->Parameters.DeviceCommand.Argument = rSdIoArgument.u.AsULONG;
	prSDRP->Parameters.DeviceCommand.Mdl = prMdl;
	prSDRP->Parameters.DeviceCommand.Length = u4ByteCount;

	rStatus = SdBusSubmitRequest(prDx->BusInterface.Context, prSDRP);

	IoFreeMdl(prMdl);
	ExFreePool(prSDRP);

	if (NT_SUCCESS(rStatus)) {
		return TRUE;

	} else {
		ERRORLOG(("CMD53 BLOCK WR FAIL!, addr: %#04x, status: %x\n", u2Port, rStatus));
		return FALSE;
	}

}				/* end of sdioCmd53BlockWrite() */


/*----------------------------------------------------------------------------*/
/*!
* \brief Interrupt handler for SDIO I/F
*
* \param[in] prGlueInfo         Pointer to GLUE Data Structure
* \param[in] u4INTerruptType    Must be set to SDBUS_INTTYPE_DEVICE
*
* \return (none)
*/
/*----------------------------------------------------------------------------*/
VOID sdioINTerruptCallback(IN PVOID context, IN UINT_32 u4INTerruptType)
{
	P_GLUE_INFO_T prGlueInfo;
	PDEVICE_EXTENSION prDevExt;
	NTSTATUS rStatus;

	ASSERT(context);
	prGlueInfo = (P_GLUE_INFO_T) context;

	prDevExt = (PDEVICE_EXTENSION) &prGlueInfo->rHifInfo.dx;


	if (prGlueInfo->rHifInfo.u4ReqFlag & REQ_FLAG_HALT) {
		if (prDevExt->BusInterface.AcknowledgeInterrupt) {
			rStatus =
			    (prDevExt->BusInterface.AcknowledgeInterrupt) (prDevExt->BusInterface.
									   Context);
		}
		return;
	}

	wlanISR(prGlueInfo->prAdapter, TRUE);

	if (prDevExt->BusInterface.AcknowledgeInterrupt) {
		rStatus =
		    (prDevExt->BusInterface.AcknowledgeInterrupt) (prDevExt->BusInterface.Context);
	}

	_InterlockedOr(&prGlueInfo->rHifInfo.u4ReqFlag, REQ_FLAG_INT);
/* KeSetEvent(&prGlueInfo->rHifInfo.rOidReqEvent, EVENT_INCREMENT, FALSE); */
	/* Set EVENT */
	NdisSetEvent(&prGlueInfo->rTxReqEvent);

	return;
}				/* end of sdioINTerruptCallback() */

NDIS_STATUS
sdioConfigProperty(IN PDEVICE_EXTENSION prDevExt,
		   IN SD_REQUEST_FUNCTION eRequestFunction,
		   IN SDBUS_PROPERTY eProperty, OUT PUINT_8 aucBuffer, IN UINT_32 u4BufLen)
{
	PSDBUS_REQUEST_PACKET prSDRP = NULL;
	NDIS_STATUS rStatus;

	/* retrieve the function number from the bus driver */
	prSDRP = ExAllocatePoolWithTag(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET), 'SDIO');

	if (!prSDRP) {
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	RtlZeroMemory(prSDRP, sizeof(SDBUS_REQUEST_PACKET));

	prSDRP->RequestFunction = eRequestFunction;
	prSDRP->Parameters.GetSetProperty.Property = eProperty;
	prSDRP->Parameters.GetSetProperty.Buffer = aucBuffer;
	prSDRP->Parameters.GetSetProperty.Length = u4BufLen;

	rStatus = SdBusSubmitRequest(prDevExt->BusInterface.Context, prSDRP);
	ExFreePool(prSDRP);

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail, status:%x\n", rStatus));
	}

	return rStatus;
}

NDIS_STATUS sdioSetupCardFeature(IN P_GLUE_INFO_T prGlueInfo, IN PDEVICE_EXTENSION prDevExt)
{
	NDIS_STATUS rStatus;
	UINT_8 ucBusWidth = (UINT_8) prGlueInfo->rRegInfo.u4SdBusWidth;
	/* NOTE(Kevin): in ntddsd.h - Length of SDP_FUNCTION_BLOCK_LENGTH == USHORT */
	UINT_16 u2BlockLength = (UINT_16) prGlueInfo->rRegInfo.u4SdBlockSize;
	UINT_16 u2HostBlockLength;
	/* NOTE(Kevin): in ntddsd.h - Length of SDP_BUS_WIDTH == UCHAR */
	UINT_32 u4BusDriverVer;
	UINT_32 u4BusClock = prGlueInfo->rRegInfo.u4SdClockRate;


	/* 4 <1> Check Function Number */
	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_GET_PROPERTY,
				     SDP_FUNCTION_NUMBER,
				     &prDevExt->FunctionNumber, sizeof(prDevExt->FunctionNumber));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail, status:%x\n", rStatus));
		return rStatus;
	} else {
		INITLOG(("[SDIO] get func. no is %d\n", prDevExt->FunctionNumber));
	}

#if 1				/* 20091012 George */
	/*  */
	/* SDIO bus driver version */
	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_GET_PROPERTY,
				     SDP_BUS_DRIVER_VERSION,
				     (PUINT_8) & u4BusDriverVer, sizeof(u4BusDriverVer));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail, status:%x\n", rStatus));
		return rStatus;
	} else {
		INITLOG(("[SDIO] bus driver version is %d\n", u4BusDriverVer));
	}

	/*  */
	/* SDIO HOST_BLOCK_LENGTH */
	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_GET_PROPERTY,
				     SDP_HOST_BLOCK_LENGTH,
				     (PUINT_8) & u2HostBlockLength, sizeof(u2HostBlockLength));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail, status:%x\n", rStatus));
		return rStatus;
	} else {
		INITLOG(("[SDIO] host block length is %d\n", u2HostBlockLength));
	}

	/*  */
	/* SDIO SDP_FN0_BLOCK_LENGTH */
	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_GET_PROPERTY,
				     SDP_FN0_BLOCK_LENGTH,
				     (PUINT_8) & u2HostBlockLength, sizeof(u2HostBlockLength));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail, status:%x\n", rStatus));
		return rStatus;
	} else {
		INITLOG(("[SDIO] function 0 block length is %d\n", u2HostBlockLength));
	}

#endif

	/* 4 <2> Setup Block Length */
#ifdef SDBUS_DRIVER_VERSION_2
	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_SET_PROPERTY,
				     SDP_FUNCTION_BLOCK_LENGTH,
				     (PUINT_8) & u2BlockLength, sizeof(u2BlockLength));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail to set block size, status:%x\n", rStatus));
		prGlueInfo->rHifInfo.dx.fgIsSdioBlockModeEnabled = FALSE;
	} else {
		INITLOG(("[SDIO] set Block size %d\n", u2BlockLength));
		prGlueInfo->rHifInfo.dx.fgIsSdioBlockModeEnabled = TRUE;
	}


	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_GET_PROPERTY,
				     SDP_FUNCTION_BLOCK_LENGTH,
				     (PUINT_8) & u2BlockLength, sizeof(u2BlockLength));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail to set block size, status:%x\n", rStatus));
		prGlueInfo->rHifInfo.dx.fgIsSdioBlockModeEnabled = FALSE;
		prGlueInfo->rHifInfo.u4BlockLength = BLOCK_TRANSFER_LEN;
	} else {
		prGlueInfo->rHifInfo.u4BlockLength = (UINT_32) u2BlockLength;
		INITLOG(("[SDIO] get Block size %d\n", u2BlockLength));
	}


	/* 4 <3> Setup Bus Width */
	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_SET_PROPERTY,
				     SDP_BUS_WIDTH, &ucBusWidth, sizeof(ucBusWidth));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail to set bus width, status:%x\n", rStatus));
	} else {
		INITLOG(("[SDIO] set Bus width %d\n", ucBusWidth));
	}

	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_GET_PROPERTY,
				     SDP_BUS_WIDTH, &ucBusWidth, sizeof(ucBusWidth));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail to set bus width, status:%x\n", rStatus));
	} else {
		prGlueInfo->rHifInfo.u4BusWidth = (UINT_32) ucBusWidth;
		INITLOG(("[SDIO] get Bus width %d\n", prGlueInfo->rHifInfo.u4BusWidth));
	}

	/* 4 <3> Setup Bus Clock */
	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_SET_PROPERTY,
				     SDP_BUS_CLOCK, (PUINT_8) & u4BusClock, sizeof(u4BusClock));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail to set bus clock, status:%x\n", rStatus));
	} else {
		INITLOG(("[SDIO] set Bus clock %d\n", u4BusClock));
	}

	rStatus = sdioConfigProperty(prDevExt,
				     SDRF_GET_PROPERTY,
				     SDP_BUS_CLOCK, (PUINT_8) & u4BusClock, sizeof(u4BusClock));

	if (!NT_SUCCESS(rStatus)) {
		ERRORLOG(("SdBusSubmitRequest fail to get bus clock, status:%x\n", rStatus));
	} else {
		prGlueInfo->rHifInfo.u4BusClock = u4BusClock;
		INITLOG(("[SDIO] get Bus clock %d\n", prGlueInfo->rHifInfo.u4BusClock));
	}
#else
	prGlueInfo->rHifInfo.dx.fgIsSdioBlockModeEnabled = FALSE;
	prGlueInfo->rHifInfo.u4BlockLength = BLOCK_TRANSFER_LEN;
#endif				/* SDP_FUNCTION_BLOCK_LENGTH */

/* emuInitChkCis(prGlueInfo->prAdapter); */
/* emuChkIntEn(prGlueInfo->prAdapter); */

	return STATUS_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to free memory mapping for the HW register.
*        (SDIO does not need Free Register)
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsUMapFreeRegister(IN P_GLUE_INFO_T prGlueInfo)
{
	return NDIS_STATUS_SUCCESS;
}				/* end of windowsUMapFreeRegister() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to register interrupt call back function.
*        (SDIO DDK does not need Register ISRT)
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsRegisterIsrt(IN P_GLUE_INFO_T prGlueInfo)
{
	return NDIS_STATUS_SUCCESS;
}				/* end of windowsRegisterIsrt() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to un-register interrupt call back function.
*        (SDIO DDK does not need Un-register ISRT)
*
* \param[in] prGlueInfo Pointer to the GLUE_INFO_T structure.
*
* \return NDIS_STATUS code
*
/*----------------------------------------------------------------------------*/
NDIS_STATUS windowsUnregisterIsrt(IN P_GLUE_INFO_T prGlueInfo)
{
	return NDIS_STATUS_SUCCESS;
}				/* end of windowsUnregisterIsrt() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is to initialize adapter members.
*
* \param[in] prGlueInfo                     Pointer to the GLUE_INFO_T structure.
* \param[in] rWrapperConfigurationContext   A handle used only during initialization
*                                           for calls to NdisXxx configuration and
*                                           initialization functions.
*
* \return NDIS_STATUS code
*
/*----------------------------------------------------------------------------*/
NDIS_STATUS
windowsFindAdapter(IN P_GLUE_INFO_T prGlueInfo, IN NDIS_HANDLE rWrapperConfigurationContext)
{
	NDIS_HANDLE rMiniportAdapterHandle;
	PDEVICE_EXTENSION prDevExt;
	NDIS_STATUS rStatus;
	INT i;
	UCHAR ucTmp;
	SDBUS_INTERFACE_PARAMETERS rInterfaceParameters = { 0 };

	DEBUGFUNC("windowsFindAdapter");
	DBGLOG(INIT, TRACE, ("\n"));


	ASSERT(prGlueInfo);

	INITLOG(("windowsFindAdapter\n"));

	rMiniportAdapterHandle = prGlueInfo->rMiniportAdapterHandle;

	prDevExt = &prGlueInfo->rHifInfo.dx;

	NdisMGetDeviceProperty(rMiniportAdapterHandle,
			       &prDevExt->PhysicalDeviceObject,
			       &prDevExt->FunctionalDeviceObject,
			       &prDevExt->NextLowerDriverObject, NULL, NULL);

	rStatus = SdBusOpenInterface(prDevExt->PhysicalDeviceObject,
				     &prDevExt->BusInterface,
				     sizeof(SDBUS_INTERFACE_STANDARD), SDBUS_INTERFACE_VERSION);

	INITLOG(("SdBusOpenInterface: (status=0x%x)\n", rStatus));
	INITLOG(("Size: (0x%x)\n", prDevExt->BusInterface.Size));
	INITLOG(("Version: (0x%x)\n", prDevExt->BusInterface.Version));
	INITLOG(("Context: (0x%x)\n", prDevExt->BusInterface.Context));
	INITLOG(("InterfaceReference: (0x%x)\n", prDevExt->BusInterface.InterfaceReference));
	INITLOG(("InterfaceDereference: (0x%x)\n", prDevExt->BusInterface.InterfaceDereference));
	INITLOG(("InitializeInterface: (0x%x)\n", prDevExt->BusInterface.InitializeInterface));
	INITLOG(("AcknowledgeInterrupt: (0x%x)\n", prDevExt->BusInterface.AcknowledgeInterrupt));

	if (NT_SUCCESS(rStatus)) {
		rInterfaceParameters.Size = sizeof(SDBUS_INTERFACE_PARAMETERS);
		rInterfaceParameters.TargetObject = prDevExt->NextLowerDriverObject;

		/* INTerrupt callback function */
		rInterfaceParameters.DeviceGeneratesInterrupts = TRUE;
		rInterfaceParameters.CallbackAtDpcLevel = FALSE;	/* passive level for synchronous I/O */
		rInterfaceParameters.CallbackRoutine = sdioINTerruptCallback;
		rInterfaceParameters.CallbackRoutineContext = prGlueInfo;
		rStatus = STATUS_UNSUCCESSFUL;


		if (prDevExt->BusInterface.InitializeInterface) {
			INITLOG(("pDevExt->BusINTerface.InitializeINTerface exists\n"));
			rStatus = (prDevExt->BusInterface.InitializeInterface)
			    (prDevExt->BusInterface.Context, &rInterfaceParameters);
		}
		/* dump sdbus parameter */


		/* dump sdbus INTerface standard after init */

		if (NT_SUCCESS(rStatus)) {
			INITLOG(("INTial SD-bus INTerface OK!!\n"));
			prDevExt->busInited = TRUE;
		} else {
			ERRORLOG(("INTial SD-bus INTerface fail,status:%x\n", rStatus));
			if (prDevExt->BusInterface.InterfaceDereference) {
				(prDevExt->BusInterface.InterfaceDereference) (prDevExt->
									       BusInterface.
									       Context);
				RtlZeroMemory(&prDevExt->BusInterface,
					      sizeof(SDBUS_INTERFACE_STANDARD));
			}
			return rStatus;
		}

	} else {		/* open SD-bus INTerface fail */
		ERRORLOG(("open SD-bus INTerface fail, status:%x\n", rStatus));
		return rStatus;
	}

	rStatus = sdioSetupCardFeature(prGlueInfo, prDevExt);

	return rStatus;

}				/* end of windowsFindAdapter() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read a 32 bit register value from device.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register     The register offset.
* \param[out] pu4Value      Pointer to the 32-bit value of the register been read.
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevRegRead(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, OUT PUINT_32 pu4Value)
{
	PDEVICE_EXTENSION prDevExt = &prGlueInfo->rHifInfo.dx;
	PSDBUS_REQUEST_PACKET prSDRP = (PSDBUS_REQUEST_PACKET) NULL;
	SD_RW_EXTENDED_ARGUMENT rSdIoArgument;
	PMDL prMdl = (PMDL) NULL;
	UINT_32 u4Length;
	NDIS_STATUS rStatus;
	const SDCMD_DESCRIPTOR rReadIoExtendedDesc = { SDCMD_IO_RW_EXTENDED,
		SDCC_STANDARD,
		SDTD_READ,
		SDTT_SINGLE_BLOCK,
		SDRT_5
	};

	ASSERT(prGlueInfo);
	ASSERT(pu4Value);

	/* First get a MDL to map the data. This code assumes the
	 * caller passed a buffer to nonpaged pool.
	 */
	u4Length = 4;

	prMdl = IoAllocateMdl(pu4Value, u4Length, FALSE, FALSE, NULL);
	if (!prMdl) {
		ERRORLOG(("IoAllocateMdl prMdl  fail!\n"));
		return FALSE;
	}
	MmBuildMdlForNonPagedPool(prMdl);

	/* Now allocate a request packet for the arguments of the command */
	prSDRP = ExAllocatePoolWithTag(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET), 'SDIO');

	if (!prSDRP) {
		IoFreeMdl(prMdl);
		ERRORLOG(("ExAllocatePool prSDRP  fail!\n"));
		return FALSE;
	}

	RtlZeroMemory(prSDRP, sizeof(SDBUS_REQUEST_PACKET));

	prSDRP->RequestFunction = SDRF_DEVICE_COMMAND;
	prSDRP->Parameters.DeviceCommand.CmdDesc = rReadIoExtendedDesc;

	/* Set up the argument and command descriptor */
	rSdIoArgument.u.AsULONG = 0;

	/* NOTE(Kevin): This will be ignored when using a device command */
	rSdIoArgument.u.bits.Count = u4Length;

	rSdIoArgument.u.bits.Address = u4Register;
	rSdIoArgument.u.bits.OpCode = 1;

	/* Function # must be initialized by SdBus GetProperty call */
	rSdIoArgument.u.bits.Function = prDevExt->FunctionNumber;

	/* Submit the request */
	rSdIoArgument.u.bits.WriteToDevice = 0;
	prSDRP->Parameters.DeviceCommand.Argument = rSdIoArgument.u.AsULONG;
	prSDRP->Parameters.DeviceCommand.Mdl = prMdl;
	prSDRP->Parameters.DeviceCommand.Length = u4Length;	/* unit in bytes */

	rStatus = SdBusSubmitRequest(prDevExt->BusInterface.Context, prSDRP);

	IoFreeMdl(prMdl);
	ExFreePool(prSDRP);

	if (NT_SUCCESS(rStatus)) {
		return TRUE;
	} else {
		ERRORLOG(("MCR RD FAIL!, addr: %#08lx, status: %x\n", u4Register, rStatus));
		return FALSE;
	}

}				/* end of kalDevRegRead() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write a 32 bit register value to device.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] u4Register     The register offset.
* \param[out] u4Value       The 32-bit value of the register to be written.
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevRegWrite(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Register, IN UINT_32 u4Value)
{
	PDEVICE_EXTENSION prDevExt = &prGlueInfo->rHifInfo.dx;
	PSDBUS_REQUEST_PACKET prSDRP = (PSDBUS_REQUEST_PACKET) NULL;
	SD_RW_EXTENDED_ARGUMENT rSdIoArgument;
	PMDL prMdl = (PMDL) NULL;
	NDIS_STATUS rStatus;
	UINT_32 u4Length;
	UINT_32 u4Data = u4Value;
	const SDCMD_DESCRIPTOR rWriteIoExtendedDesc = { SDCMD_IO_RW_EXTENDED,
		SDCC_STANDARD,
		SDTD_WRITE,
		SDTT_SINGLE_BLOCK,
		SDRT_5
	};

	ASSERT(prGlueInfo);

	/* First get a MDL to map the data. This code assumes the
	 * caller passed a buffer to nonpaged pool.
	 */
	u4Length = 4;

	prMdl = IoAllocateMdl(&u4Data, u4Length, FALSE, FALSE, NULL);

	if (!prMdl) {
		ERRORLOG(("IoAllocateMdl prMdl  fail!\n"));
		return FALSE;
	}
	MmBuildMdlForNonPagedPool(prMdl);


	/* Now allocate a request packet for the arguments of the command */
	prSDRP = ExAllocatePoolWithTag(NonPagedPool, sizeof(SDBUS_REQUEST_PACKET), 'SDIO');
	if (!prSDRP) {
		IoFreeMdl(prMdl);
		ERRORLOG(("ExAllocatePool prSDRP  fail!\n"));
		return FALSE;
	}

	RtlZeroMemory(prSDRP, sizeof(SDBUS_REQUEST_PACKET));

	prSDRP->RequestFunction = SDRF_DEVICE_COMMAND;
	prSDRP->Parameters.DeviceCommand.CmdDesc = rWriteIoExtendedDesc;

	/* Set up the argument and command descriptor */
	rSdIoArgument.u.AsULONG = 0;

	/* NOTE(Kevin): This will be ignored when using a device command */
	rSdIoArgument.u.bits.Count = u4Length;

	rSdIoArgument.u.bits.Address = u4Register;
	rSdIoArgument.u.bits.OpCode = 1;

	/* Function # must be initialized by SdBus GetProperty call */
	rSdIoArgument.u.bits.Function = prDevExt->FunctionNumber;

	/* Submit the request */
	rSdIoArgument.u.bits.WriteToDevice = 1;
	prSDRP->Parameters.DeviceCommand.Argument = rSdIoArgument.u.AsULONG;
	prSDRP->Parameters.DeviceCommand.Mdl = prMdl;
	prSDRP->Parameters.DeviceCommand.Length = u4Length;	/* unit in bytes */

	rStatus = SdBusSubmitRequest(prDevExt->BusInterface.Context, prSDRP);

	IoFreeMdl(prMdl);
	ExFreePool(prSDRP);

	if (NT_SUCCESS(rStatus)) {
		return TRUE;
	} else {
		ERRORLOG(("MCR WR FAIL!, addr: %#08lx, status: %x\n", u4Register, rStatus));
		return FALSE;
	}
}				/* end of kalDevRegWrite() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to read port data from device in unit of byte.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port             The register offset.
* \param[in] u2Len              The number of byte to be read.
* \param[out] pucBuf            Pointer to data buffer for read
* \param[in] u2ValidOutBufSize  Length of the buffer valid to be accessed
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
	PDEVICE_EXTENSION prDevExt = &prGlueInfo->rHifInfo.dx;
	UINT_8 ucBlockNo;
	UINT_32 u4ByteNo;
	UINT_32 u4BlockLength;


	ASSERT(prGlueInfo);
	ASSERT(pucBuf);
	ASSERT(u4ValidOutBufSize >= u4Len);

	u4BlockLength = (UINT_32) prGlueInfo->rHifInfo.u4BlockLength;

	if (prGlueInfo->rHifInfo.dx.fgIsSdioBlockModeEnabled) {

		ucBlockNo = (UINT_8) (u4Len / u4BlockLength);
		u4ByteNo = u4Len - (UINT_32) (ucBlockNo * u4BlockLength);

/* DbgPrint("kalDevPortRead(): Port: %#04x, Len: %d, Block No: %d, Remain Byte No: %d\n", */
/* u4Port, u4Len, ucBlockNo, u4ByteNo); */

		if (ucBlockNo > 0) {

#if 1
			if (u4ByteNo != 0) {
				if (u4ValidOutBufSize >= ((ucBlockNo + 1) * u4BlockLength)) {
					ucBlockNo++;
					u4ByteNo = 0;
				}
			}
#endif

			ASSERT(u4ValidOutBufSize >= ucBlockNo * u4BlockLength + u4ByteNo);

			if (!sdioCmd53BlockRead
			    (prDevExt, (UINT_16) u4Port, pucBuf,
			     (UINT_16) (ucBlockNo * u4BlockLength))) {
				/* SDIO Request Failed */
				return FALSE;
			}
		}
	} else {
		u4ByteNo = u4Len;
		ucBlockNo = 0;
	}

	if (u4ByteNo > 0) {
		UINT_32 u4ByteNoPerCmd;

		pucBuf = pucBuf + ucBlockNo * u4BlockLength;

		do {
			u4ByteNoPerCmd = (u4ByteNo > MAX_SD_RW_BYTES) ? MAX_SD_RW_BYTES : u4ByteNo;
			u4ByteNo -= u4ByteNoPerCmd;

			if (!sdioCmd53ByteRead(prDevExt, pucBuf, u4Port, (UINT_16) u4ByteNoPerCmd)) {
				/* SDIO Request Failed */
				return FALSE;
			}

			pucBuf = pucBuf + u4ByteNoPerCmd;
		}
		while (u4ByteNo > 0);

	}
	/* use a non-used register to avoid the ENE bug */
	else if (ucBlockNo && prGlueInfo->rHifInfo.dx.fgIsSdioBlockModeEnabled) {
		kalDevRegWrite(prGlueInfo, SDIO_X86_WORKAROUND_WRITE_MCR, 0x0);
	}

	return TRUE;

}				/* end of kalDevPortRead() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is used to write port data to device in unit of byte.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u2Port             The register offset.
* \param[in] u2Len              The number of byte to be write.
* \param[out] pucBuf            Pointer to data buffer for write
* \param[in] u2ValidOutBufSize  Length of the buffer valid to be accessed
*
* \retval TRUE
* \retval FALSE
*/
/*----------------------------------------------------------------------------*/
BOOL
kalDevPortWrite(IN P_GLUE_INFO_T prGlueInfo,
		IN UINT_32 u4Port, IN UINT_32 u4Len, IN PUINT_8 pucBuf, IN UINT_32 u4ValidInBufSize)
{
	PDEVICE_EXTENSION prDevExt = &prGlueInfo->rHifInfo.dx;
	UINT_8 ucBlockNo;
	UINT_32 u4ByteNo;
	UINT_32 u4BlockLength;


	ASSERT(prGlueInfo);
	ASSERT(pucBuf);
	ASSERT(u4ValidInBufSize >= u4Len);

	u4BlockLength = (UINT_16) prGlueInfo->rHifInfo.u4BlockLength;

	if (prGlueInfo->rHifInfo.dx.fgIsSdioBlockModeEnabled) {

		ucBlockNo = (UINT_8) (u4Len / u4BlockLength);
		u4ByteNo = u4Len - ucBlockNo * u4BlockLength;

		/* DbgPrint("kalDevPortWrite(): Port: %#04x, Len: %d, Block No: %d, Remain Byte No: %d\n", */
		/* u4Port, u4Len, ucBlockNo, u4ByteNo); */

		if (ucBlockNo > 0) {
#if 1
			if (u4ByteNo != 0) {
				if (u4ValidInBufSize >= ((ucBlockNo + 1) * u4BlockLength)) {
					ucBlockNo++;
					u4ByteNo = 0;
				}
			}
#endif

			ASSERT(u4ValidInBufSize >= ucBlockNo * u4BlockLength + u4ByteNo);

			if (!sdioCmd53BlockWrite
			    (prDevExt, (UINT_16) u4Port, pucBuf,
			     (UINT_16) (ucBlockNo * u4BlockLength))) {
				/* SDIO Request Failed */
				return FALSE;
			}
		}
	} else {
		u4ByteNo = u4Len;
		ucBlockNo = 0;
	}

	if (u4ByteNo > 0) {
		UINT_32 u4ByteNoPerCmd;

		pucBuf = pucBuf + ucBlockNo * u4BlockLength;

		do {
			u4ByteNoPerCmd = (u4ByteNo > MAX_SD_RW_BYTES) ? MAX_SD_RW_BYTES : u4ByteNo;
			u4ByteNo -= u4ByteNoPerCmd;

			if (!sdioCmd53ByteWrite(prDevExt, pucBuf, u4Port, (UINT_16) u4ByteNoPerCmd)) {
				/* SDIO Request Failed */
				return FALSE;
			}

			pucBuf = pucBuf + u4ByteNoPerCmd;
		}
		while (u4ByteNo > 0);

	}
	/* use a non-used register to avoid the ENE bug */
	else if (ucBlockNo && prGlueInfo->rHifInfo.dx.fgIsSdioBlockModeEnabled) {
		kalDevRegWrite(prGlueInfo, SDIO_X86_WORKAROUND_WRITE_MCR, 0x0);
	}

	return TRUE;

}				/* end of kalDevPortWrite() */

/*----------------------------------------------------------------------------*/
/*!
* \brief Write device I/O port in byte with CMD52
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[in] u4Addr             I/O port offset
* \param[in] ucData             single byte of data to be written
* \param[in] u4ValidInBufSize   Length of the buffer valid to be accessed
*
* \retval TRUE          operation success
* \retval FALSE         operation fail
*/
/*----------------------------------------------------------------------------*/
BOOL kalDevWriteWithSdioCmd52(IN P_GLUE_INFO_T prGlueInfo, IN UINT_32 u4Addr, IN UINT_8 ucData)
{
	PDEVICE_EXTENSION prDevExt = &prGlueInfo->rHifInfo.dx;

	ASSERT(prGlueInfo);
	ASSERT(ucData);

	return sdioCmd52ByteReadWrite(prDevExt,
				      u4Addr, &ucData, prDevExt->FunctionNumber, SDTD_WRITE);
}				/* end of kalDevWriteWithSdioCmd52() */
