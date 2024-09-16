/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#ifndef _VERSION_H
#define _VERSION_H
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

#ifndef NIC_AUTHOR
#define NIC_AUTHOR      "NIC_AUTHOR"
#endif
#ifndef NIC_DESC
#define NIC_DESC        "NIC_DESC"
#endif

#ifndef NIC_NAME
#if defined(MT6620)
#define NIC_NAME            "MT6620"
#define NIC_DEVICE_ID       "MT6620"
#define NIC_DEVICE_ID_LOW   "mt6620"
#elif defined(MT6628)
#define NIC_NAME            "MT6582"
#define NIC_DEVICE_ID       "MT6582"
#define NIC_DEVICE_ID_LOW   "mt6582"
#endif
#endif

/* NIC driver information */
#define NIC_VENDOR                      "MediaTek Inc."
#define NIC_VENDOR_OUI                  {0x00, 0x0C, 0xE7}

#if defined(MT6620)
#define NIC_PRODUCT_NAME                "MediaTek Inc. MT6620 Wireless LAN Adapter"
#define NIC_DRIVER_NAME                 "MediaTek Inc. MT6620 Wireless LAN Adapter Driver"
#elif defined(MT6628)
/* #define NIC_PRODUCT_NAME                "MediaTek Inc. MT6628 Wireless LAN Adapter" */
/* #define NIC_DRIVER_NAME                 "MediaTek Inc. MT6628 Wireless LAN Adapter Driver" */
#define NIC_PRODUCT_NAME                "MediaTek Inc. MT6582 Wireless LAN Adapter"
#define NIC_DRIVER_NAME                 "MediaTek Inc. MT6582 Wireless LAN Adapter Driver"
#endif

/* Define our driver version */
#define NIC_DRIVER_MAJOR_VERSION        2
#define NIC_DRIVER_MINOR_VERSION        0
#define NIC_DRIVER_SERIAL_VERSION       1
#define NIC_DRIVER_VERSION_STRING       "201607271200"

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

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/

#endif /* _VERSION_H */
