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

#ifndef _TYPEDEF_H
#define _TYPEDEF_H

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

/* ieee80211.h of linux has duplicated definitions */
#if defined(WLAN_STATUS_SUCCESS)
#undef WLAN_STATUS_SUCCESS
#endif

#define WLAN_STATUS_SUCCESS                     ((WLAN_STATUS) 0x00000000L)
#define WLAN_STATUS_PENDING                     ((WLAN_STATUS) 0x00000103L)
#define WLAN_STATUS_NOT_ACCEPTED                ((WLAN_STATUS) 0x00010003L)

#define WLAN_STATUS_MEDIA_CONNECT               ((WLAN_STATUS) 0x4001000BL)
#define WLAN_STATUS_MEDIA_DISCONNECT            ((WLAN_STATUS) 0x4001000CL)
#define WLAN_STATUS_MEDIA_DISCONNECT_LOCALLY	((WLAN_STATUS) 0x4001000DL)

#define WLAN_STATUS_MEDIA_SPECIFIC_INDICATION   ((WLAN_STATUS) 0x40010012L)

#define WLAN_STATUS_SCAN_COMPLETE               ((WLAN_STATUS) 0x60010001L)
#define WLAN_STATUS_MSDU_OK                     ((WLAN_STATUS) 0x60010002L)

/* TODO(Kevin): double check if 0x60010001 & 0x60010002 is proprietary */
#define WLAN_STATUS_ROAM_OUT_FIND_BEST          ((WLAN_STATUS) 0x60010101L)
#define WLAN_STATUS_ROAM_DISCOVERY              ((WLAN_STATUS) 0x60010102L)

#define WLAN_STATUS_FAILURE                     ((WLAN_STATUS) 0xC0000001L)
#define WLAN_STATUS_RESOURCES                   ((WLAN_STATUS) 0xC000009AL)
#define WLAN_STATUS_NOT_SUPPORTED               ((WLAN_STATUS) 0xC00000BBL)

#define WLAN_STATUS_MULTICAST_FULL              ((WLAN_STATUS) 0xC0010009L)
#define WLAN_STATUS_INVALID_PACKET              ((WLAN_STATUS) 0xC001000FL)
#define WLAN_STATUS_ADAPTER_NOT_READY           ((WLAN_STATUS) 0xC0010011L)
#define WLAN_STATUS_NOT_INDICATING              ((WLAN_STATUS) 0xC0010013L)
#define WLAN_STATUS_INVALID_LENGTH              ((WLAN_STATUS) 0xC0010014L)
#define WLAN_STATUS_INVALID_DATA                ((WLAN_STATUS) 0xC0010015L)
#define WLAN_STATUS_BUFFER_TOO_SHORT            ((WLAN_STATUS) 0xC0010016L)
#define WLAN_STATUS_BWCS_UPDATE                 ((WLAN_STATUS) 0xC0010017L)

#define WLAN_STATUS_JOIN_FAILURE                ((WLAN_STATUS) 0xC0010018L)

/* NIC status flags */
#define ADAPTER_FLAG_HW_ERR                     0x00400000

/* Type Length */
#define TL_IPV4     0x0008
#define TL_IPV6     0xDD86

/*******************************************************************************
*                             D A T A   T Y P E S
********************************************************************************
*/
/* Type definition for GLUE_INFO structure */
typedef struct _GLUE_INFO_T GLUE_INFO_T, *P_GLUE_INFO_T;

/* Type definition for WLAN STATUS */
typedef UINT_32 WLAN_STATUS, *P_WLAN_STATUS;

/* Type definition for ADAPTER structure */
typedef struct _ADAPTER_T ADAPTER_T, *P_ADAPTER_T;

/* Type definition for MESSAGE HEADER structure */
typedef struct _MSG_HDR_T MSG_HDR_T, *P_MSG_HDR_T;

/* Type definition for WLAN configuration */
typedef struct _WLAN_CFG_T WLAN_CFG_T, *P_WLAN_CFG_T;

/* Type definition for WLAN configuration entry */
typedef struct _WLAN_CFG_ENTRY_T WLAN_CFG_ENTRY_T, *P_WLAN_CFG_ENTRY_T;

/* Type definition for WLAN configuration callback */
typedef WLAN_STATUS(*WLAN_CFG_SET_CB) (P_ADAPTER_T prAdapter,
				       PUINT_8 pucKey, PUINT_8 pucValue, PVOID pPrivate, UINT_32 u4Flags);

/* Type definition for Pointer to OS Native Packet */
typedef void *P_NATIVE_PACKET;

/* Type definition for STA_RECORD_T structure to handle the connectivity and packet reception
 * for a particular STA.
 */
typedef struct _STA_RECORD_T STA_RECORD_T, *P_STA_RECORD_T, **PP_STA_RECORD_T;

/* CMD_INFO_T is used by Glue Layer to send a cluster of Command(OID) information to
 * the TX Path to reduce the parameters of a function call.
 */
typedef struct _CMD_INFO_T CMD_INFO_T, *P_CMD_INFO_T;

/* Following typedef should be removed later, because Glue Layer should not
 * be aware of following data type.
 */
typedef struct _SW_RFB_T SW_RFB_T, *P_SW_RFB_T, **PP_SW_RFB_T;

typedef struct _MSDU_INFO_T MSDU_INFO_T, *P_MSDU_INFO_T;

typedef struct _REG_ENTRY_T REG_ENTRY_T, *P_REG_ENTRY_T;

/* IST handler definition */
typedef VOID(*IST_EVENT_FUNCTION) (P_ADAPTER_T);

/* Type definition for function pointer of timer handler */
typedef VOID(*PFN_TIMER_CALLBACK) (IN P_GLUE_INFO_T);


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
#endif /* _TYPEDEF_H */
