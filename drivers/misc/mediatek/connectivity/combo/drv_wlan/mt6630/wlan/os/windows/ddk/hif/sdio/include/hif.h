/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/ddk/hif/sdio/include/hif.h#1 $
*/
/*! \file   hif.h"
    \brief  Sdio specific structure for GLUE layer on WinXP

    Sdio specific structure for GLUE layer on WinXP
*/



/*
** $Log: hif.h $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 07 08 2010 cp.wu
 *
 * [WPD00003833] [MT6620 and MT5931] Driver migration - move to new repository.
 *
 * 06 06 2010 kevin.huang
 * [WPD00003832][MT6620 5931] Create driver base
 * [MT6620 5931] Create driver base
 *
 * 03 25 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * firmware download load adress & start address are now configured from config.h
 *  *  *  *  * due to the different configurations on FPGA and ASIC
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1. eliminate improper variable in rHifInfo
 *  *  *  *  *  *  *  *  *  *  *  *  * 2. block TX/ordinary OID when RF test mode is engaged
 *  *  *  *  *  *  *  *  *  *  *  *  * 3. wait until firmware finish operation when entering into and leaving from RF test mode
 *  *  *  *  *  *  *  *  *  *  *  *  * 4. correct some HAL implementation
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-12-16 17:59:46 GMT mtk02752
**  add fields to record mailbox/interrupt-status read-clear state
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-11-09 22:56:49 GMT mtk01084
**  Add SDIO clock variable
**  \main\maintrunk.MT6620WiFiDriver_Prj\1 2009-09-09 17:30:39 GMT mtk01084
**  \main\maintrunk.MT5921\9 2008-10-13 02:18:58 GMT mtk01461
**  Add Sdio Block Mode support for XP SP3 (Only Byte Mode get works at XP SP2)
**  \main\maintrunk.MT5921\8 2008-09-22 13:18:59 GMT mtk01461
**  Update for code review
**  \main\maintrunk.MT5921\7 2008-07-16 14:49:10 GMT mtk01461
**  Revise MiniportReset() for SDIO in WinXP
**  \main\maintrunk.MT5921\6 2008-05-29 14:15:49 GMT mtk01084
**  add fgIsGlueExtension for used under oidThread()
**  \main\maintrunk.MT5921\5 2008-05-27 09:37:44 GMT mtk01461
**  Redefine REQ_FLAG_* for mpHalt() & oidThread()
**  \main\maintrunk.MT5921\4 2008-05-22 10:58:57 GMT mtk01461
**  Add an HALT flag for OID thread
**  \main\maintrunk.MT5921\3 2008-02-25 14:47:59 GMT mtk01385
**  1. XP ENE only support byte mode and up to 64 bytes.
**  \main\maintrunk.MT5921\2 2007-11-17 15:01:09 GMT mtk01385
**  fix typo.
**  \main\maintrunk.MT5921\1 2007-11-06 20:13:42 GMT mtk01385
**  init version.
**
** Revision 1.3  2007/06/27 02:18:51  MTK01461
**
*/

#ifndef _HIF_H
#define _HIF_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include <initguid.h>
#include <ntddsd.h>
#include <ntddk.h>
#include <wdm.h>

#ifdef MT5921
#define SDNDIS_REG_PATH TEXT("\\Comm\\MT5921")
#endif
#ifdef MT5922
#define SDNDIS_REG_PATH TEXT("\\Comm\\MT5922")
#endif

#ifdef __cplusplus
extern "C" {
	/* NDIS ddk header */
#include <ndis.h>
}
#endif
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define MAX_ACTIVE_REG_PATH         256
#define NIC_INTERFACE_TYPE          NdisInterfaceInternal
#define NIC_ATTRIBUTE               (NDIS_ATTRIBUTE_DESERIALIZE | NDIS_ATTRIBUTE_SURPRISE_REMOVE_OK)
#define NIC_DMA_MAPPED              0
#define NIC_MINIPORT_INT_REG        0
#define REQ_FLAG_HALT               (0x01)
#define REQ_FLAG_INT                (0x02)
#define REQ_FLAG_OID                (0x04)
#define REQ_FLAG_TIMER              (0x08)
#define REQ_FLAG_RESET              (0x10)
#define BLOCK_TRANSFER_LEN          (512)
/* Please make sure the MCR you wrote will not take any effect.
 * MCR_MIBSDR (0x00C4) has confirm with DE.
 */
#define SDIO_X86_WORKAROUND_WRITE_MCR   0x00C4
/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/ typedef struct _DEVICE_EXTENSION {
	/* The bus driver object */
	PDEVICE_OBJECT PhysicalDeviceObject;

	/* Functional Device Object */
	PDEVICE_OBJECT FunctionalDeviceObject;

	/* Device object we call when submitting SDbus cmd */
	PDEVICE_OBJECT NextLowerDriverObject;

	/*Bus interface */
	SDBUS_INTERFACE_STANDARD BusInterface;

	/* SDIO funciton number */
	UCHAR FunctionNumber;
	BOOLEAN busInited;

	BOOLEAN fgIsSdioBlockModeEnabled;

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

/* Windows glue layer's private data structure, which is
 * attached to adapter_p structure
 */
typedef struct _GL_HIF_INFO_T {
	/* using in SD open bus interface,  dbl add */
	DEVICE_EXTENSION dx;

	HANDLE rOidThreadHandle;
	PKTHREAD prOidThread;
	KEVENT rOidReqEvent;

	UINT_32 u4ReqFlag;	/* REQ_FLAG_XXX */

	UINT_32 u4BusClock;
	UINT_32 u4BusWidth;
	UINT_32 u4BlockLength;

	/* HW related */
	BOOLEAN fgIntReadClear;
	BOOLEAN fgMbxReadClear;

} GL_HIF_INFO_T, *P_GL_HIF_INFO_T;


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
*                  F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif				/* _HIF_H */
