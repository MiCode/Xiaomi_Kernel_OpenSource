/*
** $Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/windows/common/gl_req.c#1 $
*/

/*! \file gl_req.c
    \brief This file contains the WLAN OID processing routines which don't need
	   to access the ADAPTER_T or ARBITER FSM of WLAN Driver Core.

    This file contains the WLAN OID processing routines which don't need
    to access the ADAPTER_T or ARBITER FSM of WLAN Driver Core.
*/



/*
** $Log: gl_req.c $
**
** 09 17 2012 cm.chang
** [BORA00002149] [MT6630 Wi-Fi] Initial software development
** Duplicate source from MT6620 v2.3 driver branch
** (Davinci label: MT6620_WIFI_Driver_V2_3_120913_1942_As_MT6630_Base)
 *
 * 09 03 2010 kevin.huang
 * NULL
 * Refine #include sequence and solve recursive/nested #include issue
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
 * 05 19 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) do not take timeout mechanism for power mode oids
 * 2) retrieve network type from connection status
 * 3) after disassciation, set radio state to off
 * 4) TCP option over IPv6 is supported
 *
 * 05 18 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * implement Wakeup-on-LAN except firmware integration part
 *
 * 05 17 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add CFG_STARTUP_DEBUG for debugging starting up issue.
 *
 * 04 06 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * code refine: fgTestMode should be at adapter rather than glue due to the device/fw is also involved
 *
 * 03 19 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) add ACPI D0/D3 state switching support
 *  *  *  *  *  *  *  *  *  *  * 2) use more formal way to handle interrupt when the status is retrieved from enhanced RX response
 *
 * 02 23 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * add checksum offloading support.
 *
 * 02 10 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) remove unused function in nic_rx.c [which has been handled in que_mgt.c]
 *  *  *  *  *  *  *  *  * 2) firmware image length is now retrieved via NdisFileOpen
 *  *  *  *  *  *  *  *  * 3) firmware image is not structured by (P_IMG_SEC_HDR_T) anymore
 *  *  *  *  *  *  *  *  * 4) nicRxWaitResponse() revised
 *  *  *  *  *  *  *  *  * 5) another set of TQ counter default value is added for fw-download state
 *  *  *  *  *  *  *  *  * 6) Wi-Fi load address is now retrieved from registry too
 *
 * 01 27 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * 1) implement timeout mechanism when OID is pending for longer than 1 second
 *  *  *  *  *  * 2) allow OID_802_11_CONFIGURATION to be executed when RF test mode is turned on
 *
 * 01 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * .implement Set/Query BeaconInterval/AtimWindow
 *
 * 01 07 2010 cp.wu
 * [WPD00001943]Create WiFi test driver framework on WinXP
 * .Set/Get AT Info is not blocked even when driver is not in fg test mode
**  \main\maintrunk.MT6620WiFiDriver_Prj\5 2009-12-08 17:40:27 GMT mtk02752
**  allow OID_802_11_CONFIGURATION only when rf test mode is turned on, then frequency information will be passed to F/W
**  \main\maintrunk.MT6620WiFiDriver_Prj\4 2009-12-03 16:15:53 GMT mtk01461
**  Add debug message
**  \main\maintrunk.MT6620WiFiDriver_Prj\3 2009-04-08 17:48:08 GMT mtk01084
**  modify the interface of downloading image from D3 to D0
**  \main\maintrunk.MT6620WiFiDriver_Prj\2 2009-03-10 20:37:05 GMT mtk01426
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

extern WLAN_REQ_ENTRY arWlanOidReqTable[];

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*  */
/* This miniport only supports one Encapsultion type: IEEE_802_3_Encapsulation */
/* one task version: NDIS_TASK_OFFLOAD_VERSION. Modify the code below OID_TCP_ */
/* TASK_OFFLOAD in query and setting information functions to make it support */
/* more than one encapsulation type and task version */
/*  */
/* Define the task offload the miniport currently supports. */
/* This miniport only supports two kinds of offload tasks: */
/* TCP/IP checksum offload and Segmentation large TCP packet offload */
/* Later if it can supports more tasks, just redefine this task array */
/*  */
NDIS_TASK_OFFLOAD arOffloadTasks[] = {
	{
	 NDIS_TASK_OFFLOAD_VERSION,
	 sizeof(NDIS_TASK_OFFLOAD),
	 TcpIpChecksumNdisTask,
	 0,
	 sizeof(NDIS_TASK_TCP_IP_CHECKSUM)
	 }
};


/*  */
/* Get the number of offload tasks this miniport supports */
/*  */
UINT_32 u4OffloadTasksCount = sizeof(arOffloadTasks) / sizeof(arOffloadTasks[0]);

/* V4 TX */
#define  OFFLOAD_V4_TX_IP_OPT_SUPPORT           1
#define  OFFLOAD_V4_TX_TCP_OPT_SUPPORT          1
#define  OFFLOAD_V4_TX_TCP_CHKSUM_SUPPORT       1
#define  OFFLOAD_V4_TX_UDP_CHKSUM_SUPPORT       1
#define  OFFLOAD_V4_TX_IP_CHKSUM_SUPPORT        1

/* V4 RX */
#define  OFFLOAD_V4_RX_IP_OPT_SUPPORT           1
#define  OFFLOAD_V4_RX_TCP_OPT_SUPPORT          1
#define  OFFLOAD_V4_RX_TCP_CHKSUM_SUPPORT       1
#define  OFFLOAD_V4_RX_UDP_CHKSUM_SUPPORT       1
#define  OFFLOAD_V4_RX_IP_CHKSUM_SUPPORT        1

/* V6 TX */
#define  OFFLOAD_V6_TX_IP_OPT_SUPPORT           0
#define  OFFLOAD_V6_TX_TCP_OPT_SUPPORT          1
#define  OFFLOAD_V6_TX_TCP_CHKSUM_SUPPORT       1
#define  OFFLOAD_V6_TX_UDP_CHKSUM_SUPPORT       1

/* V6 RX */
#define  OFFLOAD_V6_RX_IP_OPT_SUPPORT           0
#define  OFFLOAD_V6_RX_TCP_OPT_SUPPORT          1
#define  OFFLOAD_V6_RX_TCP_CHKSUM_SUPPORT       1
#define  OFFLOAD_V6_RX_UDP_CHKSUM_SUPPORT       1

/*  */
/* Specify TCP/IP checksum offload task, the miniport can only supports, for now, */
/* TCP checksum and IP checksum on the sending side, also it supports TCP and IP */
/* options, J: includes the additional RX offload */
/*  */
NDIS_TASK_TCP_IP_CHECKSUM rTcpIpChecksumTask = {
	/* V4Transmit; */
	{
	 OFFLOAD_V4_TX_IP_OPT_SUPPORT,
	 OFFLOAD_V4_TX_TCP_OPT_SUPPORT,
	 OFFLOAD_V4_TX_TCP_CHKSUM_SUPPORT,
	 OFFLOAD_V4_TX_UDP_CHKSUM_SUPPORT,
	 OFFLOAD_V4_TX_IP_CHKSUM_SUPPORT}
	,
	/* V4Receive; */
	{
	 OFFLOAD_V4_RX_IP_OPT_SUPPORT,
	 OFFLOAD_V4_RX_TCP_OPT_SUPPORT,
	 OFFLOAD_V4_RX_TCP_CHKSUM_SUPPORT,
	 OFFLOAD_V4_RX_UDP_CHKSUM_SUPPORT,
	 OFFLOAD_V4_RX_IP_CHKSUM_SUPPORT}
	,
	/* V6Transmit; */
	{
	 OFFLOAD_V6_TX_IP_OPT_SUPPORT,
	 OFFLOAD_V6_TX_TCP_OPT_SUPPORT,
	 OFFLOAD_V6_TX_TCP_CHKSUM_SUPPORT,
	 OFFLOAD_V6_TX_UDP_CHKSUM_SUPPORT}
	,
	/* V6Receive; */
	{
	 OFFLOAD_V6_RX_IP_OPT_SUPPORT,
	 OFFLOAD_V6_RX_TCP_OPT_SUPPORT,
	 OFFLOAD_V6_RX_TCP_CHKSUM_SUPPORT,
	 OFFLOAD_V6_RX_UDP_CHKSUM_SUPPORT}
	,
};
#endif				/* CFG_TCP_IP_CHKSUM_OFFLOAD */


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
/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the the vendor-assigned version number
*        of the NIC driver.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[out] pvQryBuf      Pointer to the buffer that holds the result of the query.
* \param[in] u4QryBufLen    The length of the query buffer.
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryVendorDriverVersion(IN P_GLUE_INFO_T prGlueInfo,
			    OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	DEBUGFUNC("reqQueryVendorDriverVersion");


	ASSERT(prGlueInfo);
	ASSERT(pu4QryInfoLen);

	*pu4QryInfoLen = sizeof(UINT_32);

	if (u4QryBufLen < sizeof(UINT_32)) {
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQryBuf);


	*(PUINT_32) pvQryBuf = ((UINT_32) NIC_DRIVER_MAJOR_VERSION << 16) +
	    (UINT_32) NIC_DRIVER_MINOR_VERSION;

	DBGLOG(REQ, INFO, ("Vendor driver version: 0x%08x\n", *(PUINT_32) pvQryBuf));

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryVendorDriverVersion() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the hardware status of the NIC.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[out] pvQryBuf      Pointer to the buffer that holds the result of the query.
* \param[in] u4QryBufLen    The length of the query buffer.
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryHardwareStatus(IN P_GLUE_INFO_T prGlueInfo,
		       OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	DEBUGFUNC("reqQueryHardwareStatus");


	ASSERT(prGlueInfo);
	ASSERT(pu4QryInfoLen);

	*pu4QryInfoLen = sizeof(NDIS_HARDWARE_STATUS);

	if (u4QryBufLen < sizeof(NDIS_HARDWARE_STATUS)) {
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQryBuf);

	*(PNDIS_HARDWARE_STATUS) pvQryBuf = NdisHardwareStatusReady;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryHardwareStatus() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the media types supported or in use.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[out] pvQryBuf      Pointer to the buffer that holds the result of the query.
* \param[in] u4QryBufLen    The length of the query buffer.
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryMedia(IN P_GLUE_INFO_T prGlueInfo,
	      OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	DEBUGFUNC("reqQueryMedia");


	ASSERT(prGlueInfo);
	ASSERT(pu4QryInfoLen);

	*pu4QryInfoLen = sizeof(PNDIS_MEDIUM);

	if (u4QryBufLen < sizeof(PNDIS_MEDIUM)) {
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQryBuf);

	*(PNDIS_MEDIUM) pvQryBuf = NdisMedium802_3;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryMedia() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the vendor description.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[out] pvQryBuf      Pointer to the buffer that holds the result of the query.
* \param[in] u4QryBufLen    The length of the query buffer.
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryVendorDescription(IN P_GLUE_INFO_T prGlueInfo,
			  OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{

	DEBUGFUNC("reqQueryVendorDescription");


	ASSERT(prGlueInfo);
	ASSERT(pu4QryInfoLen);

	*pu4QryInfoLen = (UINT_32) prGlueInfo->ucDriverDescLen;

	if (u4QryBufLen < (UINT_32) prGlueInfo->ucDriverDescLen) {
		/* Not enough room for the query information. */
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQryBuf);

	NdisMoveMemory(pvQryBuf,
		       (PVOID) prGlueInfo->aucDriverDesc, (UINT_32) prGlueInfo->ucDriverDescLen);

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryVendorDescription() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the NDIS version number used by the
*        driver.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[out] pvQryBuf      Pointer to the buffer that holds the result of the query.
* \param[in] u4QryBufLen    The length of the query buffer.
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryDriverVersion(IN P_GLUE_INFO_T prGlueInfo,
		      OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	DEBUGFUNC("reqQueryDriverVersion");


	ASSERT(prGlueInfo);
	ASSERT(pu4QryInfoLen);

	*pu4QryInfoLen = 2;

	if (u4QryBufLen < 2) {
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQryBuf);

	*(PUINT_16) pvQryBuf = prGlueInfo->u2NdisVersion;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryDriverVersion() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the physical media supported by the
*        driver.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[out] pvQryBuf      Pointer to the buffer that holds the result of the query.
* \param[in] u4QryBufLen    The length of the query buffer.
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryPhysicalMedium(IN P_GLUE_INFO_T prGlueInfo,
		       OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	DEBUGFUNC("reqQueryPhysicalMedium");


	ASSERT(prGlueInfo);
	ASSERT(pu4QryInfoLen);

	*pu4QryInfoLen = sizeof(NDIS_PHYSICAL_MEDIUM);

	if (u4QryBufLen < sizeof(NDIS_PHYSICAL_MEDIUM)) {
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQryBuf);

	*(PNDIS_PHYSICAL_MEDIUM) pvQryBuf = NdisPhysicalMediumWirelessLan;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryPhysicalMedium() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the optional NIC flags.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[out] pvQryBuf      Pointer to the buffer that holds the result of the query.
* \param[in] u4QryBufLen    The length of the query buffer.
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryMacOptions(IN P_GLUE_INFO_T prGlueInfo,
		   OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	DEBUGFUNC("reqQueryMacOptions");


	ASSERT(prGlueInfo);
	ASSERT(pu4QryInfoLen);

	*pu4QryInfoLen = sizeof(UINT_32);

	if (u4QryBufLen < sizeof(UINT_32)) {
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQryBuf);


	/* NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA is set to indicate to the
	   protocol that it can access the lookahead data by any means that
	   it wishes.  On some systems there are fast copy routines that
	   may have trouble accessing shared memory.  Netcard drivers that
	   indicate data out of shared memory, should not have this flag
	   set on these troublesome systems  For the time being this driver
	   will set this flag.  This should be safe because the data area
	   of the RFDs is contained in uncached memory. */

	/* NOTE: Don't set NDIS_MAC_OPTION_RECEIVE_SERIALIZED if we are
	   doing multipacket (ndis4) style receives. */

#if defined(WINDOWS_CE)		/* No Windows XP until 802.1p mechanism is clear */
	*(PUINT_32) pvQryBuf = (UINT_32) (NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
					  NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
					  NDIS_MAC_OPTION_NO_LOOPBACK |
					  NDIS_MAC_OPTION_8021P_PRIORITY);
#else
	*(PUINT_32) pvQryBuf = (UINT_32) (NDIS_MAC_OPTION_TRANSFERS_NOT_PEND |
					  NDIS_MAC_OPTION_COPY_LOOKAHEAD_DATA |
					  NDIS_MAC_OPTION_NO_LOOPBACK);
#endif

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryMacOptions() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the connection status of the NIC on
*        the network.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[out] pvQryBuf      Pointer to the buffer that holds the result of the query.
* \param[in] u4QryBufLen    The length of the query buffer.
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryMediaConnectStatus(IN P_GLUE_INFO_T prGlueInfo,
			   OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	DEBUGFUNC("wlanoidQueryMediaConnectStatus");


	ASSERT(prGlueInfo);
	ASSERT(pu4QryInfoLen);

	*pu4QryInfoLen = sizeof(ENUM_PARAM_MEDIA_STATE_T);

	if (u4QryBufLen < sizeof(ENUM_PARAM_MEDIA_STATE_T)) {
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQryBuf);

	/* Now we simply return our status (NdisMediaState[Dis]Connected) */
	*(P_ENUM_PARAM_MEDIA_STATE_T) pvQryBuf = prGlueInfo->eParamMediaStateIndicated;

	DBGLOG(REQ, INFO, ("Media State: %s\n",
			   ((prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED) ?
			    "Connected" : "Disconnected")));

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryMediaConnectStatus() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the maximum frame size in bytes.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuf        Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufLen      The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryMaxFrameSize(IN P_GLUE_INFO_T prGlueInfo,
		     OUT PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("reqQueryMaxFrameSize");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufLen < sizeof(UINT_32)) {
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQueryBuf);

	*(PUINT_32) pvQueryBuf = ETHERNET_MAX_PKT_SZ - ETHERNET_HEADER_SZ;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryMaxFrameSize() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the amount of memory, in bytes,
*        for the TX buffer to transmit data.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuf        Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufLen      The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryTxBufferSpace(IN P_GLUE_INFO_T prGlueInfo,
		      OUT PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("reqQueryTxBufferSpace");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufLen < sizeof(UINT_32)) {
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuf);

	*(PUINT_32) pvQueryBuf = (UINT_32) CFG_RX_MAX_PKT_NUM * ETHERNET_MAX_PKT_SZ;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryTxBufferSpace() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the amount of memory, in bytes,
*        for the RX buffer to receive data.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuf        Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufLen      The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryRxBufferSpace(IN P_GLUE_INFO_T prGlueInfo,
		      OUT PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("reqQueryRxBufferSpace");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufLen < sizeof(UINT_32)) {
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuf);

	*(PUINT_32) pvQueryBuf = (UINT_32) CFG_RX_MAX_PKT_NUM * ETHERNET_MAX_PKT_SZ;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryRxBufferSpace() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the maximum total packet length
*        in bytes.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuf        Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufLen      The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryMaxTotalSize(IN P_GLUE_INFO_T prGlueInfo,
		     OUT PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("reqQueryMaxTotalSize");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufLen < sizeof(UINT_32)) {
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuf);

	*(PUINT_32) pvQueryBuf = ETHERNET_MAX_PKT_SZ;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryMaxTotalSize() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the number of frames received with
*        alignment errors.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuf        Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufLen      The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryRcvErrorAlignment(IN P_GLUE_INFO_T prGlueInfo,
			  OUT PVOID pvQueryBuf,
			  IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("reqQueryRcvErrorAlignment");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufLen < sizeof(UINT_32)) {
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuf);

	*(PUINT_32) pvQueryBuf = 0;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryRcvErrorAlignment() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set a suggested value for the number of bytes
*        of received packet data that will be indicated to the protocol driver.
*        We just accept the set and ignore this value.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] pvSetBuffer    A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqSetCurrentLookahead(IN P_GLUE_INFO_T prGlueInfo,
		       IN PVOID prSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	DEBUGFUNC("reqSetCurrentLookahead");

	ASSERT(prGlueInfo);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(UINT_32);

	return WLAN_STATUS_SUCCESS;

}				/* end of reqSetCurrentLookahead() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the maximum number of send packets
*        the driver can accept per call to its MiniportSendPackets function.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuf        Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufLen      The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryMaxSendPackets(IN P_GLUE_INFO_T prGlueInfo,
		       OUT PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen)
{
	DEBUGFUNC("reqQueryMaxSendPackets");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(UINT_32);

	if (u4QueryBufLen < sizeof(UINT_32)) {
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuf);

	*(PUINT_32) pvQueryBuf = MAX_ARRAY_SEND_PACKETS;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryMaxSendPackets() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the maximum number of multicast addresses
*        the NIC driver can manage.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[out] pvQryBuf      Pointer to the buffer that holds the result of the query.
* \param[in] u4QryBufLen    The length of the query buffer.
* \param[out] pu4QryInfoLen If the call is successful, returns the number of
*                           bytes written into the query buffer. If the call
*                           failed due to invalid length of the query buffer,
*                           returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryMaxListSize(IN P_GLUE_INFO_T prGlueInfo,
		    OUT PVOID pvQryBuf, IN UINT_32 u4QryBufLen, OUT PUINT_32 pu4QryInfoLen)
{
	DEBUGFUNC("reqQueryMaxListSize");


	ASSERT(prGlueInfo);
	ASSERT(pu4QryInfoLen);

	*pu4QryInfoLen = sizeof(UINT_32);

	if (u4QryBufLen < sizeof(UINT_32)) {
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQryBuf);

	*(PUINT_32) pvQryBuf = MAX_NUM_GROUP_ADDR;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryMaxListSize() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the NIC's wake-up capabilities
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuf        Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufLen      The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_BUFFER_TOO_SHORT
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryPnPCapabilities(IN P_GLUE_INFO_T prGlueInfo,
			IN PVOID pvQueryBuf, IN UINT_32 u4QueryBufLen, OUT PUINT_32 pu4QueryInfoLen)
{
	PNDIS_PNP_CAPABILITIES prPwrMgtCap = (PNDIS_PNP_CAPABILITIES) pvQueryBuf;

	DEBUGFUNC("pwrmgtQueryPnPCapabilities");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(NDIS_PNP_CAPABILITIES);

	if (u4QueryBufLen < sizeof(NDIS_PNP_CAPABILITIES)) {
		return WLAN_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQueryBuf);

#if CFG_ENABLE_WAKEUP_ON_LAN
	prPwrMgtCap->Flags = PARAM_DEVICE_WAKE_UP_ENABLE;

	prPwrMgtCap->WakeUpCapabilities.MinMagicPacketWakeUp = NdisDeviceStateD2;

	prPwrMgtCap->WakeUpCapabilities.MinPatternWakeUp = NdisDeviceStateD2;
#else
	prPwrMgtCap->Flags = 0;

	prPwrMgtCap->WakeUpCapabilities.MinMagicPacketWakeUp = ParamDeviceStateUnspecified;

	prPwrMgtCap->WakeUpCapabilities.MinPatternWakeUp = ParamDeviceStateUnspecified;
#endif

	prPwrMgtCap->WakeUpCapabilities.MinLinkChangeWakeUp = ParamDeviceStateUnspecified;

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryPnPCapabilities() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the information elements
*        used in the last association/reassociation request to an
*        AP and in the last association/reassociation response from the AP.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuffer     Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqQueryAssocInfo(IN P_GLUE_INFO_T prGlueInfo,
		  OUT PVOID pvQueryBuffer,
		  IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{

#if DBG
	PNDIS_802_11_ASSOCIATION_INFORMATION prAssocInfo =
	    (PNDIS_802_11_ASSOCIATION_INFORMATION) pvQueryBuffer;
	PUINT_8 cp;
#endif

	DEBUGFUNC("wlanoidQueryAssocInfo");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(NDIS_802_11_ASSOCIATION_INFORMATION) +
	    prGlueInfo->rNdisAssocInfo.RequestIELength +
	    prGlueInfo->rNdisAssocInfo.ResponseIELength;

	if (u4QueryBufferLen < *pu4QueryInfoLen) {
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);

	kalMemCopy(pvQueryBuffer, (PVOID) & prGlueInfo->rNdisAssocInfo, *pu4QueryInfoLen);

#if DBG
	/* Dump the PARAM_ASSOCIATION_INFORMATION content. */
	DBGLOG(REQ, INFO, ("QUERY: Assoc Info - Length: %d\n", prAssocInfo->Length));

	DBGLOG(REQ, INFO, ("AvailableRequestFixedIEs: 0x%04x\n",
			   prAssocInfo->AvailableRequestFixedIEs));
	DBGLOG(REQ, INFO, ("Request Capabilities: 0x%04x\n",
			   prAssocInfo->RequestFixedIEs.Capabilities));
	DBGLOG(REQ, INFO, ("Request Listen Interval: 0x%04x\n",
			   prAssocInfo->RequestFixedIEs.ListenInterval));
	cp = (PUINT_8) &prAssocInfo->RequestFixedIEs.CurrentAPAddress;
	DBGLOG(REQ, INFO, ("CurrentAPAddress: %02x-%02x-%02x-%02x-%02x-%02x\n",
			   cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]));
	DBGLOG(REQ, INFO, ("Request IEs: length=%d, offset=%d\n",
			   prAssocInfo->RequestIELength, prAssocInfo->OffsetRequestIEs));

	cp = (PUINT_8) pvQueryBuffer + sizeof(NDIS_802_11_ASSOCIATION_INFORMATION);
	DBGLOG_MEM8(REQ, INFO, cp, prAssocInfo->RequestIELength);
	cp += prAssocInfo->RequestIELength;

	DBGLOG(REQ, INFO, ("AvailableResponseFixedIEs: 0x%04x\n",
			   prAssocInfo->AvailableResponseFixedIEs));
	DBGLOG(REQ, INFO, ("Response Capabilities: 0x%04x\n",
			   prAssocInfo->ResponseFixedIEs.Capabilities));
	DBGLOG(REQ, INFO, ("StatusCode: 0x%04x\n", prAssocInfo->ResponseFixedIEs.StatusCode));
	DBGLOG(REQ, INFO, ("AssociationId: 0x%04x\n", prAssocInfo->ResponseFixedIEs.AssociationId));
	DBGLOG(REQ, INFO, ("Response IEs: length=%d, offset=%d\n",
			   prAssocInfo->ResponseIELength, prAssocInfo->OffsetResponseIEs));
	DBGLOG_MEM8(REQ, INFO, cp, prAssocInfo->ResponseIELength);
#endif

	return WLAN_STATUS_SUCCESS;

}				/* end of reqQueryAssocInfo() */

/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to query the radio configuration used in IBSS
*        mode and RF test mode.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuffer     Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqExtQueryConfiguration(IN P_GLUE_INFO_T prGlueInfo,
			 OUT PVOID pvQueryBuffer,
			 IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	P_PARAM_802_11_CONFIG_T prQueryConfig = (P_PARAM_802_11_CONFIG_T) pvQueryBuffer;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	UINT_32 u4QueryInfoLen = 0;

	DEBUGFUNC("wlanoidQueryConfiguration");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	*pu4QueryInfoLen = sizeof(PARAM_802_11_CONFIG_T);
	if (u4QueryBufferLen < sizeof(PARAM_802_11_CONFIG_T)) {
		return WLAN_STATUS_INVALID_LENGTH;
	}

	ASSERT(pvQueryBuffer);

	kalMemZero(prQueryConfig, sizeof(PARAM_802_11_CONFIG_T));

	/* Update the current radio configuration. */
	prQueryConfig->u4Length = sizeof(PARAM_802_11_CONFIG_T);

	/* beacon interval */
	rStatus = wlanoidQueryBeaconInterval(prGlueInfo->prAdapter,
					     &prQueryConfig->u4BeaconPeriod,
					     sizeof(UINT_32), &u4QueryInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;

	/* ATIM Window */
	rStatus = wlanoidQueryAtimWindow(prGlueInfo->prAdapter,
					 &prQueryConfig->u4ATIMWindow,
					 sizeof(UINT_32), &u4QueryInfoLen);

	if (rStatus != WLAN_STATUS_SUCCESS)
		return rStatus;

	/* frequency setting */
	rStatus = wlanoidQueryFrequency(prGlueInfo->prAdapter,
					&prQueryConfig->u4DSConfig,
					sizeof(UINT_32), &u4QueryInfoLen);

	prQueryConfig->rFHConfig.u4Length = sizeof(PARAM_802_11_CONFIG_FH_T);

	return rStatus;

}				/* end of reqExtQueryConfiguration() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set the radio configuration used in IBSS
*        mode.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] pvSetBuffer    A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_LENGTH
* \retval WLAN_STATUS_NOT_ACCEPTED
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqExtSetConfiguration(IN P_GLUE_INFO_T prGlueInfo,
		       IN PVOID pvSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	P_PARAM_802_11_CONFIG_T prNewConfig = (P_PARAM_802_11_CONFIG_T) pvSetBuffer;
	UINT_32 u4SetInfoLen = 0;

	DEBUGFUNC("wlanoidSetConfiguration");

	ASSERT(prGlueInfo);
	ASSERT(pu4SetInfoLen);

	*pu4SetInfoLen = sizeof(PARAM_802_11_CONFIG_T);

	if (u4SetBufferLen < *pu4SetInfoLen) {
		return WLAN_STATUS_INVALID_LENGTH;
	}

	/* OID_802_11_CONFIGURATION. If associated, NOT_ACCEPTED shall be returned. */
	if (wlanQueryTestMode(prGlueInfo->prAdapter) == FALSE &&
	    prGlueInfo->eParamMediaStateIndicated == PARAM_MEDIA_STATE_CONNECTED) {
		return WLAN_STATUS_NOT_ACCEPTED;
	}

	ASSERT(pvSetBuffer);

	/* beacon interval */
	rStatus = wlanoidSetBeaconInterval(prGlueInfo->prAdapter,
					   &prNewConfig->u4BeaconPeriod,
					   sizeof(UINT_32), pu4SetInfoLen);

	if (wlanQueryTestMode(prGlueInfo->prAdapter) == FALSE && rStatus != WLAN_STATUS_SUCCESS) {
		return rStatus;
	}
	/* ATIM Window */
	rStatus = wlanoidSetAtimWindow(prGlueInfo->prAdapter,
				       &prNewConfig->u4ATIMWindow, sizeof(UINT_32), pu4SetInfoLen);

	if (wlanQueryTestMode(prGlueInfo->prAdapter) == FALSE && rStatus != WLAN_STATUS_SUCCESS) {
		return rStatus;
	}
	/* frequency setting */
	rStatus = wlanoidSetFrequency(prGlueInfo->prAdapter,
				      &prNewConfig->u4DSConfig, sizeof(UINT_32), pu4SetInfoLen);

	return rStatus;

}				/* end of reqExtSetConfiguration() */


#if CFG_TCP_IP_CHKSUM_OFFLOAD
/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to query the task offload capability.
*
* \param[in] prGlueInfo         Pointer to the GLUE_INFO_T structure.
* \param[out] pvQueryBuffer     Pointer to the buffer that holds the result of the query.
* \param[in] u4QueryBufferLen   The length of the query buffer.
* \param[out] pu4QueryInfoLen   If the call is successful, returns the number of
*                               bytes written into the query buffer. If the call
*                               failed due to invalid length of the query buffer,
*                               returns the amount of storage needed.
*
* \retval NDIS_STATUS_SUCCESS
* \retval NDIS_STATUS_BUFFER_TOO_SHORT
* \retval NDIS_STATUS_NOT_SUPPORTED
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
reqQueryTaskOffload(IN P_GLUE_INFO_T prGlueInfo,
		    OUT PVOID pvQueryBuffer,
		    IN UINT_32 u4QueryBufferLen, OUT PUINT_32 pu4QueryInfoLen)
{
	PNDIS_TASK_OFFLOAD_HEADER prNdisTaskOffloadHdr;
	PNDIS_TASK_OFFLOAD prTaskOffload;
	PNDIS_TASK_TCP_IP_CHECKSUM prTcpIpChecksumTask;
	UINT_32 u4InfoLen;
	UINT_32 i;

	DEBUGFUNC("reqQueryTaskOffload");


	ASSERT(prGlueInfo);
	ASSERT(pu4QueryInfoLen);

	/*  */
	/* Calculate the information buffer length we need to write the offload */
	/* capabilities */
	/*  */
	u4InfoLen = sizeof(NDIS_TASK_OFFLOAD_HEADER) +
	    FIELD_OFFSET(NDIS_TASK_OFFLOAD, TaskBuffer) + sizeof(NDIS_TASK_TCP_IP_CHECKSUM);

	if (u4InfoLen > u4QueryBufferLen) {
		*pu4QueryInfoLen = u4InfoLen;
		DBGLOG(REQ, TRACE, ("ulInfoLen(%d) > queryBufferLen(%d)\n",
				    u4InfoLen, u4QueryBufferLen));
		return NDIS_STATUS_BUFFER_TOO_SHORT;
	}

	ASSERT(pvQueryBuffer);

	/*  */
	/* check version and Encapsulation Type */
	/*  */
	prNdisTaskOffloadHdr = (PNDIS_TASK_OFFLOAD_HEADER) pvQueryBuffer;

	/*  */
	/* Assume the miniport only supports IEEE_802_3_Encapsulation type */
	/*  */
	if (prNdisTaskOffloadHdr->EncapsulationFormat.Encapsulation != IEEE_802_3_Encapsulation) {
		DBGLOG(REQ, TRACE, ("Encapsulation  type is not supported.\n"));

		prNdisTaskOffloadHdr->OffsetFirstTask = 0;
		return NDIS_STATUS_NOT_SUPPORTED;
	}
	/*  */
	/* Assume the miniport only supports task version of NDIS_TASK_OFFLOAD_VERSION */
	/*  */
	if ((prNdisTaskOffloadHdr->Size != sizeof(NDIS_TASK_OFFLOAD_HEADER)) ||
	    (prNdisTaskOffloadHdr->Version != NDIS_TASK_OFFLOAD_VERSION)) {
		DBGLOG(REQ, TRACE, ("Size or Version is not correct.\n"));

		prNdisTaskOffloadHdr->OffsetFirstTask = 0;
		return NDIS_STATUS_NOT_SUPPORTED;
	}
	/*  */
	/* If no capabilities supported, OffsetFirstTask should be set to 0 */
	/* Currently we support TCP/IP checksum and TCP large send, so set */
	/* OffsetFirstTask to indicate the offset of the first offload task */
	/*  */
	prNdisTaskOffloadHdr->OffsetFirstTask = prNdisTaskOffloadHdr->Size;

	/*  */
	/* Fill TCP/IP checksum and TCP large send task offload structures */
	/*  */
	prTaskOffload = (PNDIS_TASK_OFFLOAD) ((PUCHAR) (pvQueryBuffer) +
					      prNdisTaskOffloadHdr->Size);
	/*  */
	/* Fill all the offload capabilities the miniport supports. */
	/*  */
	for (i = 0; i < u4OffloadTasksCount; i++) {
		prTaskOffload->Size = arOffloadTasks[i].Size;
		prTaskOffload->Version = arOffloadTasks[i].Version;
		prTaskOffload->Task = arOffloadTasks[i].Task;
		prTaskOffload->TaskBufferLength = arOffloadTasks[i].TaskBufferLength;

		/*  */
		/* Not the last task */
		/*  */
		if (i != u4OffloadTasksCount - 1) {
			prTaskOffload->OffsetNextTask =
			    FIELD_OFFSET(NDIS_TASK_OFFLOAD,
					 TaskBuffer) + prTaskOffload->TaskBufferLength;
		} else {
			prTaskOffload->OffsetNextTask = 0;
		}

		switch (arOffloadTasks[i].Task) {
			/*  */
			/* TCP/IP checksum task offload */
			/*  */
		case TcpIpChecksumNdisTask:
			prTcpIpChecksumTask =
			    (PNDIS_TASK_TCP_IP_CHECKSUM) prTaskOffload->TaskBuffer;

			NdisMoveMemory(prTcpIpChecksumTask,
				       &rTcpIpChecksumTask, sizeof(rTcpIpChecksumTask));
			break;

		default:
			break;
		}

		/*  */
		/* Points to the next task offload */
		/*  */
		if (i != u4OffloadTasksCount) {
			prTaskOffload = (PNDIS_TASK_OFFLOAD)
			    ((PUCHAR) prTaskOffload + prTaskOffload->OffsetNextTask);
		}
	}

	/*  */
	/* So far, everything is setup, so return to the caller */
	/*  */
	*pu4QueryInfoLen = u4InfoLen;

	DBGLOG(REQ, TRACE, ("Offloading is set.\n"));
	/* DBGLOG_MEM8(REQ, TRACE, queryBuffer_p, queryBufferLen); */

	return NDIS_STATUS_SUCCESS;

}				/* end of reqQueryTaskOffload() */


/*----------------------------------------------------------------------------*/
/*!
* \brief This function is used to set the task offload capability.
*
* \param[in] prGlueInfo     Pointer to the GLUE_INFO_T structure.
* \param[in] pvSetBuffer    A pointer to the buffer that holds the data to be set.
* \param[in] u4SetBufferLen The length of the set buffer.
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*                           bytes read from the set buffer. If the call failed
*                           due to invalid length of the set buffer, returns
*                           the amount of storage needed.
*
* \retval NDIS_STATUS_SUCCESS
* \retval NDIS_STATUS_INVALID_DATA
* \retval NDIS_STATUS_INVALID_LENGTH
* \retval NDIS_STATUS_FAILURE
* \retval NDIS_STATUS_NOT_SUPPORTED
*/
/*----------------------------------------------------------------------------*/
NDIS_STATUS
reqExtSetTaskOffload(IN P_GLUE_INFO_T prGlueInfo,
		     IN PVOID prSetBuffer, IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	PNDIS_TASK_OFFLOAD_HEADER pNdisTaskOffloadHdr;
	PNDIS_TASK_OFFLOAD TaskOffload;
	PNDIS_TASK_OFFLOAD TmpOffload;
	PNDIS_TASK_TCP_IP_CHECKSUM pTcpIpChecksumTask;
	UINT i;
	UINT_32 u4BytesRead = 0;
	UINT_32 u4FlagTcpIpChksum = 0;
	UINT_32 u4ExtBytesRead = 0;
	NDIS_STATUS rStatus = NDIS_STATUS_SUCCESS;

	DEBUGFUNC("reqExtSetTaskOffload");


	ASSERT(prGlueInfo);
	ASSERT(pu4SetInfoLen);

	DBGLOG_MEM8(REQ, TRACE, prSetBuffer, u4SetBufferLen);

	u4BytesRead = sizeof(NDIS_TASK_OFFLOAD_HEADER);

	/*  */
	/* Assume miniport only supports IEEE_802_3_Encapsulation */
	/* Check to make sure that TCP/IP passed down the correct encapsulation type */
	/*  */
	pNdisTaskOffloadHdr = (PNDIS_TASK_OFFLOAD_HEADER) prSetBuffer;
	if (pNdisTaskOffloadHdr->EncapsulationFormat.Encapsulation != IEEE_802_3_Encapsulation) {
		DBGLOG(REQ, TRACE,
		       ("pNdisTaskOffloadHdr->EncapsulationFormat.Encapsulation != IEEE_802_3_Encapsulation\n"));
		pNdisTaskOffloadHdr->OffsetFirstTask = 0;
		return NDIS_STATUS_INVALID_DATA;
	}
	/*  */
	/* No offload task to be set */
	/*  */
	if (pNdisTaskOffloadHdr->OffsetFirstTask == 0) {
		*pu4SetInfoLen = u4SetBufferLen;
		DBGLOG(REQ, TRACE, ("No offload task is set!!\n"));

		/* Disable HW engine for checksum offload function */
		u4FlagTcpIpChksum = 0;
		wlanSetInformation(prGlueInfo->prAdapter,
				   wlanoidSetCSUMOffload,
				   &u4FlagTcpIpChksum, sizeof(u4FlagTcpIpChksum), &u4ExtBytesRead);

		return NDIS_STATUS_PENDING;
	}
	/*  */
	/* OffsetFirstTask is not valid */
	/*  */
	if (pNdisTaskOffloadHdr->OffsetFirstTask < pNdisTaskOffloadHdr->Size) {
		DBGLOG(REQ, TRACE,
		       ("pNdisTaskOffloadHdr->OffsetFirstTask (%d) < pNdisTaskOffloadHdr->Size (%d)\n",
			pNdisTaskOffloadHdr->OffsetFirstTask, pNdisTaskOffloadHdr->Size));
		pNdisTaskOffloadHdr->OffsetFirstTask = 0;
		return NDIS_STATUS_FAILURE;
	}
	/*  */
	/* The length can't hold one task */
	/*  */
	if (u4SetBufferLen < (pNdisTaskOffloadHdr->OffsetFirstTask + sizeof(NDIS_TASK_OFFLOAD))) {
		*pu4SetInfoLen = pNdisTaskOffloadHdr->OffsetFirstTask + sizeof(NDIS_TASK_OFFLOAD);

		DBGLOG(REQ, TRACE,
		       ("response of task offload does not have sufficient space even for 1 offload task!!\n"));
		return NDIS_STATUS_INVALID_LENGTH;
	}
	/*  */
	/* Check to make sure we support the task offload requested */
	/*  */
	TaskOffload = (NDIS_TASK_OFFLOAD *)
	    ((PUCHAR) pNdisTaskOffloadHdr + pNdisTaskOffloadHdr->OffsetFirstTask);

	TmpOffload = TaskOffload;

	/*  */
	/* Check the task in the buffer and enable the offload capabilities */
	/*  */
	while (TmpOffload) {

		u4BytesRead += FIELD_OFFSET(NDIS_TASK_OFFLOAD, TaskBuffer);

		switch (TmpOffload->Task) {

		case TcpIpChecksumNdisTask:
			/*  */
			/* Invalid information buffer length */
			/*  */
			if (u4SetBufferLen < u4BytesRead + sizeof(NDIS_TASK_TCP_IP_CHECKSUM)) {
				*pu4SetInfoLen = u4BytesRead + sizeof(NDIS_TASK_TCP_IP_CHECKSUM);
				return NDIS_STATUS_INVALID_LENGTH;
			}
			/*  */
			/* Check version */
			/*  */
			for (i = 0; i < u4OffloadTasksCount; i++) {
				if ((arOffloadTasks[i].Task == TmpOffload->Task) &&
				    (arOffloadTasks[i].Version == TmpOffload->Version)) {
					break;
				}
			}
			/*  */
			/* Version is mismatched */
			/*  */
			if (i == u4OffloadTasksCount) {
				return NDIS_STATUS_NOT_SUPPORTED;
			}
			/*  */
			/* This miniport support TCP/IP checksum offload only with sending TCP */
			/* and IP checksum with TCP/IP options. */
			/* check if the fields in NDIS_TASK_TCP_IP_CHECKSUM is set correctly */
			/*  */

			pTcpIpChecksumTask = (PNDIS_TASK_TCP_IP_CHECKSUM) TmpOffload->TaskBuffer;

			if (pTcpIpChecksumTask->V4Transmit.IpChecksum) {
				/*  */
				/* If the miniport doesn't support sending IP checksum, we can't enable */
				/* this capabilities */
				/*  */
				if (rTcpIpChecksumTask.V4Transmit.IpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}

				DBGLOG(REQ, TRACE, ("Set Sending IP offloading.\n"));
				/*  */
				/* Enable sending IP checksum */
				/*  */
				u4FlagTcpIpChksum |= CSUM_OFFLOAD_EN_TX_IP;
			}

			if (pTcpIpChecksumTask->V4Transmit.TcpChecksum) {
				/*  */
				/* If miniport doesn't support sending TCP checksum, we can't enable */
				/* this capability */
				/*  */
				if (rTcpIpChecksumTask.V4Transmit.TcpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}

				DBGLOG(REQ, TRACE, ("Set Sending TCP offloading.\n"));
				/*  */
				/* Enable sending TCP checksum */
				/*  */
				u4FlagTcpIpChksum |= CSUM_OFFLOAD_EN_TX_TCP;
			}

			if (pTcpIpChecksumTask->V4Transmit.UdpChecksum) {
				/*  */
				/* If the miniport doesn't support sending UDP checksum, we can't */
				/* enable this capability */
				/*  */
				if (rTcpIpChecksumTask.V4Transmit.UdpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}

				DBGLOG(REQ, TRACE, ("Set Transmit UDP offloading.\n"));
				/*  */
				/* Enable sending UDP checksum */
				/*  */
				u4FlagTcpIpChksum |= CSUM_OFFLOAD_EN_TX_UDP;
			}

			if (pTcpIpChecksumTask->V6Transmit.TcpChecksum) {
				/*  */
				/* IF the miniport doesn't support receiving UDP checksum, we can't */
				/* enable this capability */
				/*  */
				if (rTcpIpChecksumTask.V6Transmit.TcpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}
				DBGLOG(REQ, TRACE, ("Set IPv6 Transmit TCP offloading.\n"));
				/*  */
				/* Enable receiving UDP checksum */
				/*  */
				u4FlagTcpIpChksum |= CSUM_OFFLOAD_EN_TX_TCP;
			}

			if (pTcpIpChecksumTask->V6Transmit.UdpChecksum) {
				/*  */
				/* IF the miniport doesn't support receiving UDP checksum, we can't */
				/* enable this capability */
				/*  */
				if (rTcpIpChecksumTask.V6Transmit.UdpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}
				DBGLOG(REQ, TRACE, ("Set IPv6 Transmit UDP offloading.\n"));
				/*  */
				/* Enable receiving UDP checksum */
				/*  */
				u4FlagTcpIpChksum |= CSUM_OFFLOAD_EN_TX_UDP;
			}
			/*  */
			/* left for recieve and other IP and UDP checksum offload */
			/*  */
			if (pTcpIpChecksumTask->V4Receive.IpChecksum) {
				/*  */
				/* If the miniport doesn't support receiving IP checksum, we can't */
				/* enable this capability */
				/*  */
				if (rTcpIpChecksumTask.V4Receive.IpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}
				DBGLOG(REQ, TRACE, ("Set Recieve IP offloading.\n"));
				/*  */
				/* Enable recieving IP checksum */
				/*  */
				u4FlagTcpIpChksum |= CSUM_OFFLOAD_EN_RX_IPv4;
			}

			if (pTcpIpChecksumTask->V4Receive.TcpChecksum) {
				/*  */
				/* If the miniport doesn't support receiving TCP checksum, we can't */
				/* enable this capability */
				/*  */
				if (rTcpIpChecksumTask.V4Receive.TcpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}
				DBGLOG(REQ, TRACE, ("Set Recieve TCP offloading.\n"));
				/*  */
				/* Enable recieving TCP checksum */
				/*  */
				u4FlagTcpIpChksum |= CSUM_OFFLOAD_EN_RX_TCP;
			}

			if (pTcpIpChecksumTask->V4Receive.UdpChecksum) {
				/*  */
				/* IF the miniport doesn't support receiving UDP checksum, we can't */
				/* enable this capability */
				/*  */
				if (rTcpIpChecksumTask.V4Receive.UdpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}
				DBGLOG(REQ, TRACE, ("Set Recieve UDP offloading.\n"));
				/*  */
				/* Enable receiving UDP checksum */
				/*  */
				u4FlagTcpIpChksum |= CSUM_OFFLOAD_EN_RX_UDP;
			}

			if (pTcpIpChecksumTask->V6Receive.TcpChecksum) {
				/*  */
				/* IF the miniport doesn't support receiving UDP checksum, we can't */
				/* enable this capability */
				/*  */
				if (rTcpIpChecksumTask.V6Receive.TcpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}
				DBGLOG(REQ, TRACE, ("Set IPv6 Recieve TCP offloading.\n"));
				/*  */
				/* Enable receiving TCP checksum */
				/*  */
				u4FlagTcpIpChksum |=
				    (CSUM_OFFLOAD_EN_RX_IPv6 | CSUM_OFFLOAD_EN_RX_TCP);
			}

			if (pTcpIpChecksumTask->V6Receive.UdpChecksum) {
				/*  */
				/* IF the miniport doesn't support receiving UDP checksum, we can't */
				/* enable this capability */
				/*  */
				if (rTcpIpChecksumTask.V6Receive.UdpChecksum == 0) {
					return NDIS_STATUS_NOT_SUPPORTED;
				}
				DBGLOG(REQ, TRACE, ("Set IPv6 Recieve UDP offloading.\n"));
				/*  */
				/* Enable receiving UDP checksum */
				/*  */
				u4FlagTcpIpChksum |=
				    (CSUM_OFFLOAD_EN_RX_IPv6 | CSUM_OFFLOAD_EN_RX_UDP);
			}
			/* check for V6 setting, because this miniport doesn't support IP otions of */
			/* checksum offload for V6, so we just return NDIS_STATUS_NOT_SUPPORTED */
			/* if the protocol tries to set these capabilities */
			/*  */
			if (pTcpIpChecksumTask->V6Transmit.IpOptionsSupported ||
			    pTcpIpChecksumTask->V6Receive.IpOptionsSupported) {
				return NDIS_STATUS_NOT_SUPPORTED;
			}

			/* set HW engine for checksum offload attributes */
			wlanSetInformation(prGlueInfo->prAdapter,
					   wlanoidSetCSUMOffload,
					   &u4FlagTcpIpChksum,
					   sizeof(u4FlagTcpIpChksum), &u4ExtBytesRead);

			rStatus = NDIS_STATUS_PENDING;

			u4BytesRead += sizeof(NDIS_TASK_TCP_IP_CHECKSUM);
			break;

/* default: */
			/*  */
			/* Because this miniport doesn't implement IPSec offload, so it doesn't */
			/* support IPSec offload. Tasks other then these 3 task are not supported */
			/*  */
/* return NDIS_STATUS_NOT_SUPPORTED; */
		}

		/*  */
		/* Go on to the next offload structure */
		/*  */
		if (TmpOffload->OffsetNextTask) {
			TmpOffload = (PNDIS_TASK_OFFLOAD)
			    ((PUCHAR) TmpOffload + TmpOffload->OffsetNextTask);
		} else {
			TmpOffload = NULL;
		}

	}			/* while */

	*pu4SetInfoLen = u4BytesRead;

	return rStatus;

}				/* end of reqExtSetTaskOffload() */
#endif				/* CFG_TCP_IP_CHKSUM_OFFLOAD */


/*----------------------------------------------------------------------------*/
/*!
* \brief This routine is called to set beacon detection function enable/disable state
*        This is mainly designed for usage under BT inquiry state (disable function).
*
* \param[in] pvAdapter Pointer to the Adapter structure
* \param[in] pvSetBuffer A pointer to the buffer that holds the data to be set
* \param[in] u4SetBufferLen The length of the set buffer
* \param[out] pu4SetInfoLen If the call is successful, returns the number of
*   bytes read from the set buffer. If the call failed due to invalid length of
*   the set buffer, returns the amount of storage needed.
*
* \retval WLAN_STATUS_SUCCESS
* \retval WLAN_STATUS_INVALID_DATA If new setting value is wrong.
* \retval WLAN_STATUS_INVALID_LENGTH
*
*/
/*----------------------------------------------------------------------------*/
WLAN_STATUS
reqExtSetAcpiDevicePowerState(IN P_GLUE_INFO_T prGlueInfo,
			      IN PVOID pvSetBuffer,
			      IN UINT_32 u4SetBufferLen, OUT PUINT_32 pu4SetInfoLen)
{
	ASSERT(prGlueInfo);
	ASSERT(pvSetBuffer);
	ASSERT(pu4SetInfoLen);

	return wlanSetInformation(prGlueInfo->prAdapter,
				  wlanoidSetAcpiDevicePowerState,
				  pvSetBuffer, u4SetBufferLen, pu4SetInfoLen);
}

/*----------------------------------------------------------------------------*/
/*!
* \brief It is to open a file, and to map the content to a buffer.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param rFileName      The file name of the file to be opened with.
* @param pFileHandle    The file handle for the file been opened with.
* @param ppvMapFileBuf  The buffer of the file content to be mapped with.
*
* @retval TRUE          Success
* @retval FALSE         Fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN
imageFileMapping(IN NDIS_STRING rFileName,
		 OUT NDIS_HANDLE * pFileHandle,
		 OUT PVOID * ppvMapFileBuf, OUT PUINT_32 pu4FileLength)
{
	NDIS_STATUS rStatus = NDIS_STATUS_FAILURE;
	NDIS_PHYSICAL_ADDRESS high = NDIS_PHYSICAL_ADDRESS_CONST(-1, -1);
	UINT FileLength;

	NdisOpenFile(&rStatus, pFileHandle, &FileLength, &rFileName, high);

	if (rStatus != NDIS_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, ("Use builded image\n"));
		return FALSE;
	} else {
		rStatus = NDIS_STATUS_FAILURE;
		NdisMapFile(&rStatus, ppvMapFileBuf, *pFileHandle);
		if (rStatus != NDIS_STATUS_SUCCESS) {
			NdisCloseFile(*pFileHandle);
			DBGLOG(REQ, INFO, ("map file fail!!\n"));
			return FALSE;
		}
	}

	*pu4FileLength = FileLength;

	return TRUE;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief It is to unmap the content of the buffer, and close the file.
*
* @param prAdapter      Pointer to the Adapter structure.
* @param pFileHandle    The file handle for the file been opened with.
* @param pvMapFileBuf   The buffer of the file content to be mapped with.
*
* @retval TRUE          Success
* @retval FALSE         Fail
*/
/*----------------------------------------------------------------------------*/
BOOLEAN imageFileUnMapping(IN NDIS_HANDLE rFileHandle, OUT PVOID pvMapFileBuf)
{
	if (rFileHandle) {
		if (pvMapFileBuf) {
			NdisUnmapFile(rFileHandle);
		}
		DBGLOG(REQ, INFO, ("closed open file\n"));
		NdisCloseFile(rFileHandle);
	}
	return TRUE;
}
