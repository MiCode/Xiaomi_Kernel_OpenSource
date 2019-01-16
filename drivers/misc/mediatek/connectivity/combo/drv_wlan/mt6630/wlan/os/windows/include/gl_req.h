/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/include/gl_req.h#1 $
*/

/*! \file   gl_req.h
    \brief  This file contains the declaration of WLAN OID processing routines
	    which don't need to access the ADAPTER_T or ARBITER FSM of WLAN
	    Driver Core.

    This file contains the declarations of WLAN OID processing routines which
    don't need to access the ADAPTER_T or ARBITER FSM of WLAN Driver Core.
*/



/*
** $Log: Gl_req.h $
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
 * 05 20 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) integrate OID_GEN_NETWORK_LAYER_ADDRESSES with CMD_ID_SET_IP_ADDRESS
 * 2) buffer statistics data for 2 seconds
 * 3) use default value for adhoc parameters instead of 0
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove unused function in nic_rx.c [which has been handled in que_mgt.c]
 *  *  *  *  *  *  *  *  *  * 2) firmware image length is now retrieved via NdisFileOpen
 *  *  *  *  *  *  *  *  *  * 3) firmware image is not structured by (P_IMG_SEC_HDR_T) anymore
 *  *  *  *  *  *  *  *  *  * 4) nicRxWaitResponse() revised
 *  *  *  *  *  *  *  *  *  * 5) another set of TQ counter default value is added for fw-download state
 *  *  *  *  *  *  *  *  *  * 6) Wi-Fi load address is now retrieved from registry too
 *
 * 02 09 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * .
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-04-15 20:18:45 GMT mtk01084
**  prevent LINT error on NDIS definition
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-04-08 17:48:11 GMT mtk01084
**  modify the interface of downloading image from D3 to D0
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:40:48 GMT mtk01426
**  Init for develop
**
*/

#ifndef _GL_REQ_H
#define _GL_REQ_H

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_typedef.h"

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
/*----------------------------------------------------------------------------*/
/* Routines in gl_req.c                                                       */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryVendorDriverVersion(IN P_GLUE_INFO_T prGlueInfo,
			    OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
reqQueryHardwareStatus(IN P_GLUE_INFO_T prGlueInfo,
		       OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
reqQueryMedia(IN P_GLUE_INFO_T prGlueInfo,
	      OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
reqQueryVendorDescription(IN P_GLUE_INFO_T prGlueInfo,
			  OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
reqQueryDriverVersion(IN P_GLUE_INFO_T prGlueInfo,
		      OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
reqQueryPhysicalMedium(IN P_GLUE_INFO_T prGlueInfo,
		       OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
reqQueryMacOptions(IN P_GLUE_INFO_T prGlueInfo,
		   OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
reqQueryMediaConnectStatus(IN P_GLUE_INFO_T prGlueInfo,
			   OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
reqQueryMaxFrameSize(IN P_GLUE_INFO_T prGlueInfo,
		     IN PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
reqQueryTxBufferSpace(IN P_GLUE_INFO_T prGlueInfo,
		      OUT PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
reqQueryRxBufferSpace(IN P_GLUE_INFO_T prGlueInfo,
		      OUT PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
reqQueryMaxTotalSize(IN P_GLUE_INFO_T prGlueInfo,
		     OUT PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
reqQueryRcvErrorAlignment(IN P_GLUE_INFO_T prGlueInfo,
			  IN PVOID pvQueryBuf,
			  IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
reqSetCurrentLookahead(IN P_GLUE_INFO_T prGlueInfo,
		       IN PVOID prSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

WLAN_STATUS
reqQueryMaxSendPackets(IN P_GLUE_INFO_T prGlueInfo,
		       OUT PVOID pvQueryBuf,
		       IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
reqQueryMaxListSize(IN P_GLUE_INFO_T prGlueInfo,
		    OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);


WLAN_STATUS
reqQueryPnPCapabilities(IN P_GLUE_INFO_T prGlueInfo,
			IN PVOID pvQueryBuf,
			IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
reqQueryAssocInfo(IN P_GLUE_INFO_T prGlueInfo,
		  OUT PVOID pvQueryBuffer,
		  IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
reqExtQueryConfiguration(IN P_GLUE_INFO_T prGlueInfo,
			 OUT PVOID pvQueryBuffer,
			 IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

WLAN_STATUS
reqExtSetConfiguration(IN P_GLUE_INFO_T prGlueInfo,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

#if CFG_TCP_IP_CHKSUM_OFFLOAD
NDIS_STATUS
reqQueryTaskOffload(IN P_GLUE_INFO_T prGlueInfo,
		    OUT PVOID pvQueryBuffer,
		    IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen);

NDIS_STATUS
reqExtSetTaskOffload(IN P_GLUE_INFO_T prGlueInfo,
		     IN PVOID prSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);
#endif				/* CFG_TCP_IP_CHKSUM_OFFLOAD */

/*----------------------------------------------------------------------------*/
/* Routines in gl_oid.c                                                       */
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQuerySupportedList(IN P_GLUE_INFO_T prGlueInfo,
		      OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen);

WLAN_STATUS
reqExtSetAcpiDevicePowerState(IN P_GLUE_INFO_T prGlueInfo,
			      IN PVOID pvSetBuffer,
			      IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen);

LINT_EXT_HEADER_BEGIN
    BOOLEAN
imageFileMapping(IN NDIS_STRING rFileName,
		 OUT NDIS_HANDLE * pFileHandle,
		 OUT PVOID * ppvMapFileBuf, OUT PUINT_32 pu4FileLength);

BOOLEAN imageFileUnMapping(IN NDIS_HANDLE rFileHandle, OUT PVOID pvMapFileBuf);
LINT_EXT_HEADER_END
/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
#endif				/* _GL_REQ_H */
