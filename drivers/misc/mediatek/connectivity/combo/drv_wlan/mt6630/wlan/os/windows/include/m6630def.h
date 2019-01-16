/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/include/m6630def.h#1 $
*/

/*! \file   m6630def.h
    \brief  Define some driver message strings

*/



/*
** $Log: m6630def.h $
**
** 10 25 2012 cp.wu
** [BORA00002227] [MT6630 Wi-Fi][Driver] Update for Makefile and HIFSYS modifications
** update for windows build system
** a) remove MT6620/MT5931/MT6628 related part
** b) add for MT6630 build
**
*/

#ifndef _M6630DEF_H
#define _M6630DEF_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
/* NIC driver information */
#define NIC_VENDOR                      "MediaTek Inc."
#define NIC_VENDOR_OUI                  {0x00, 0x0C, 0xE7}

/* #define NIC_DEVICE_ID                   "MT6630" */
#define NIC_PRODUCT_NAME                "MediaTek Inc. MT6630 Wireless LAN Adapter"
#define NIC_DRIVER_NAME                 "MediaTek Inc. MT6630 Wireless LAN Adapter Driver"

#ifdef WINDOWS_CE
#ifdef _HIF_SDIO
#define FILE_NAME                   "MT6630SD.DLL"
#endif
#ifdef _HIF_EHPI
#define FILE_NAME                   "MT6630EH.DLL"
#endif
#ifdef _HIF_SPI
#define FILE_NAME                   "MT6630SP.DLL"
#endif
#else
#ifdef _HIF_SDIO
#define FILE_NAME                   "MT6630SDx.DLL"
#endif
#ifdef _HIF_EHPI
#define FILE_NAME                   "MT6630EHx.DLL"
#endif
#endif

#define NIC_DRIVER_INTERNAL_NAME        FILE_NAME
#define NIC_DRIVER_ORIGINAL_FILE_NAME   FILE_NAME


#ifdef WINDOWS_CE
#define NIC_DRIVER_FILE_DESCRIPTION "NDIS 5.1/5.0 WINCE Driver"
#else
#ifdef NDIS51_MINIPORT
#define NIC_DRIVER_FILE_DESCRIPTION "NT 5 (NDIS 5.1/5.0) x86 Driver"
#endif

#ifdef NDIS50_MINIPORT
#error "No support for NDIS 5.0"
#endif
#endif

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

#endif				/* _M6630DEF_H */
