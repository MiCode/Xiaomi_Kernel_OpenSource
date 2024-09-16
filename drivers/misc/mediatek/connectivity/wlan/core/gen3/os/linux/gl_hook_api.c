/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software: you can redistribute it and/or modify it under the terms of the
* GNU General Public License version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
* without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See the GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License along with this program.
* If not, see <http://www.gnu.org/licenses/>.
*/
/*
 ***************************************************************************

	Module Name:
	gl_ate_agent.c
*/
/*******************************************************************************
 *						C O M P I L E R	 F L A G S
 ********************************************************************************
 */

/*******************************************************************************
 *						E X T E R N A L	R E F E R E N C E S
 ********************************************************************************
 */
#include "precomp.h"
#if CFG_SUPPORT_QA_TOOL
#include "gl_wext.h"
#include "gl_cfg80211.h"
#include "gl_ate_agent.h"
#include "gl_qa_agent.h"
/* #if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 8, 0) */
#include <uapi/linux/nl80211.h>
/* #endif */
/*******************************************************************************
*						C O N S T A N T S
********************************************************************************
*/

enum {
	ATE_LOG_RXV = 1,
	ATE_LOG_RDD,
	ATE_LOG_RE_CAL,
	ATE_LOG_TYPE_NUM,
	ATE_LOG_RXINFO,
	ATE_LOG_TXDUMP,
	ATE_LOG_TEST,
};

enum {
	ATE_LOG_OFF,
	ATE_LOG_ON,
	ATE_LOG_DUMP,
	ATE_LOG_CTRL_NUM,
};

/*******************************************************************************
*						F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Enter Test Mode.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEStart(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetATE = %s\n", prInBuf);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetTestMode,	/* pfnOidHandler */
			    NULL,	/* pvInfoBuf */
			    0,	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Enter ICAP Mode.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ICAPStart(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetATE = %s\n", prInBuf);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetTestIcapMode,	/* pfnOidHandler */
			    NULL,	/* pvInfoBuf */
			    0,	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Abort Test Mode.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEStop(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetATE = %s\n", prInBuf);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAbortTestMode,	/* pfnOidHandler */
			    NULL,	/* pvInfoBuf */
			    0,	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Start auto Tx test in packet format and the driver will enter auto Tx test mode.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEStartTX(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetATE = %s\n", prInBuf);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
	rRfATInfo.u4FuncData = RF_AT_COMMAND_STARTTX;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Stop TX/RX test action if the driver is in any test mode.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEStopTX(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetATE = %s\n", prInBuf);

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
	rRfATInfo.u4FuncData = RF_AT_COMMAND_STOPTEST;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Start auto Rx test and the driver will enter auto Rx test mode.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEStartRX(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetATE = %s\n", prInBuf);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
	rRfATInfo.u4FuncData = RF_AT_COMMAND_STARTRX;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Stop TX/RX test action if the driver is in any test mode.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] prInBuf
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEStopRX(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetATE = %s\n", prInBuf);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
	rRfATInfo.u4FuncData = RF_AT_COMMAND_STOPTEST;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Channel Frequency.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4SetFreq		Center frequency in unit of KHz
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetChannel(struct net_device *prNetDev, UINT_32 u4SXIdx, UINT_32 u4SetFreq)
{
	UINT_32 u4BufLen = 0;
	UINT_32 i4SetChan;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	i4SetChan = nicFreq2ChannelNum(u4SetFreq);

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetChannel=%d, Freq=%d\n", i4SetChan, u4SetFreq);

	if (u4SetFreq == 0)
		return -EINVAL;

	if (u4SXIdx == 0) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_CHNL_FREQ;
		rRfATInfo.u4FuncData = u4SetFreq;
	} else {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_CHNL_FREQ | BIT(16);
		rRfATInfo.u4FuncData = u4SetFreq;
	}

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Preamble.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4Mode		depends on Rate. 0--> normal, 1--> CCK short preamble, 2: 11n MM, 3: 11n GF 4: 11ac VHT
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetPreamble(struct net_device *prNetDev, UINT_32 u4Mode)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetPreamble=%d\n", u4Mode);

	if (u4Mode > 4)
		return -EINVAL;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_PREAMBLE;
	rRfATInfo.u4FuncData = u4Mode;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Channel Bandwidth.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4BW			Choose Channel Bandwidth 0: 20 / 1: 40 / 2: 80 / 3: 160
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetSystemBW(struct net_device *prNetDev, UINT_32 u4BW)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BWMapping;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetSystemBW=%d\n", u4BW);

	if (u4BW > 6)
		return -EINVAL;

	if (u4BW == 0)
		u4BWMapping = 0;
	else if (u4BW == 1)
		u4BWMapping = 1;
	else if (u4BW == 2)
		u4BWMapping = 2;
	else if (u4BW == 3)
		u4BWMapping = 6;
	else if (u4BW == 4)
		u4BWMapping = 5;
	else if (u4BW == 5)
		u4BWMapping = 3;
	else if (u4BW == 6)
		u4BWMapping = 4;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_CBW;
	rRfATInfo.u4FuncData = u4BWMapping;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set TX Length.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4TxLength		Packet length (MPDU)
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxLength(struct net_device *prNetDev, UINT_32 u4TxLength)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetTxLength=%d\n", u4TxLength);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_PKTLEN;
	rRfATInfo.u4FuncData = u4TxLength;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set TX Count.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4TxCount		Total packet count to send. 0 : unlimited, until stopped
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxCount(struct net_device *prNetDev, UINT_32 u4TxCount)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetTxCount=%d\n", u4TxCount);

	if (u4TxCount < 0)
		return -EINVAL;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_PKTCNT;
	rRfATInfo.u4FuncData = u4TxCount;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set TX Inter-Packet Guard.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4TxIPG		In unit of us. The min value is 19us and max value is 2314us.
* \                         It will be round-up to (19+9n) us.
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxIPG(struct net_device *prNetDev, UINT_32 u4TxIPG)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetTxIPG=%d\n", u4TxIPG);

	if (u4TxIPG > 2314 || u4TxIPG < 19)
		return -EINVAL;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_PKTINTERVAL;
	rRfATInfo.u4FuncData = u4TxIPG;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set WF0 TX Power.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4TxPower0	Tx Gain of RF. The value is signed absolute power
*            (2's complement representation) in unit of 0.5 dBm.
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxPower0(struct net_device *prNetDev, UINT_32 u4TxPower0)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetTxPower0=0x%02x\n", u4TxPower0);

	if (u4TxPower0 > 0x3F) {
		u4TxPower0 += 128;
		DBGLOG(RFTEST, INFO, "QA_ATE_HOOK Negative Power =0x%02x\n", u4TxPower0);
	}

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_POWER;
	rRfATInfo.u4FuncData = u4TxPower0;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Per Packet BW.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4BW			0: 20 / 1: 40 / 2: 80 / 3: 160
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetPerPacketBW(struct net_device *prNetDev, UINT_32 u4BW)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 u4BWMapping;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetPerPacketBW=%d\n", u4BW);

	if (u4BW > 6)
		return -EINVAL;

	if (u4BW == 0)
		u4BWMapping = 0;
	else if (u4BW == 1)
		u4BWMapping = 1;
	else if (u4BW == 2)
		u4BWMapping = 2;
	else if (u4BW == 3)
		u4BWMapping = 6;
	else if (u4BW == 4)
		u4BWMapping = 5;
	else if (u4BW == 5)
		u4BWMapping = 3;
	else if (u4BW == 6)
		u4BWMapping = 4;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_DBW;
	rRfATInfo.u4FuncData = u4BWMapping;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Primary Channel Setting.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4PrimaryCh	The range is from 0~7
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEPrimarySetting(struct net_device *prNetDev, UINT_32 u4PrimaryCh)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK PrimarySetting=%d\n", u4PrimaryCh);

	if (u4PrimaryCh > 7)
		return -EINVAL;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_PRIMARY_CH;
	rRfATInfo.u4FuncData = u4PrimaryCh;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set TX Guard Interval.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4SetTxGi		0: Normal GI, 1: Short GI
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxGi(struct net_device *prNetDev, UINT_32 u4SetTxGi)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetTxGi=%d\n", u4SetTxGi);

	if (u4SetTxGi != 0 && u4SetTxGi != 1)
		return -EINVAL;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_GI;
	rRfATInfo.u4FuncData = u4SetTxGi;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set TX Path.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4Tx_path		0: All Tx, 1: WF0, 2: WF1, 3: WF0+WF1
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxPath(struct net_device *prNetDev, UINT_32 u4Tx_path)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK u4Tx_path=%d\n", u4Tx_path);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TX_PATH;
	rRfATInfo.u4FuncData = u4Tx_path;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set TX Payload Fix/Random.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4Stbc	       0: Disable , 1 : Enable
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxPayLoad(struct net_device *prNetDev, UINT_32 u4Gen_payload_rule, UINT_8 ucPayload)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK rule=%d, len =0x%x\n", u4Gen_payload_rule, ucPayload);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_PAYLOAD;
	rRfATInfo.u4FuncData = ((u4Gen_payload_rule << 16) | ucPayload);

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set TX STBC.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4Stbc	       0: Disable , 1 : Enable
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxSTBC(struct net_device *prNetDev, UINT_32 u4Stbc)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK u4Stbc=%d\n", u4Stbc);

	if (u4Stbc > 1)
		return -EINVAL;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_STBC;
	rRfATInfo.u4FuncData = u4Stbc;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set TX Nss.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4Nss		       1/2
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxVhtNss(struct net_device *prNetDev, UINT_32 u4VhtNss)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK u4Nss=%d\n", u4VhtNss);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_NSS;
	rRfATInfo.u4FuncData = u4VhtNss;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Rate.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4Rate		Rate
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetRate(struct net_device *prNetDev, UINT_32 u4Rate)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetRate=0x%08x\n", u4Rate);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_RATE;
	rRfATInfo.u4FuncData = u4Rate;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Encode Mode.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4Ldpc		0: BCC / 1: LDPC
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetEncodeMode(struct net_device *prNetDev, UINT_32 u4Ldpc)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetEncodeMode=%d\n", u4Ldpc);

	if (u4Ldpc != 0 && u4Ldpc != 1)
		return -EINVAL;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ENCODE_MODE;
	rRfATInfo.u4FuncData = u4Ldpc;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set iBF Enable.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4iBF		0: disable / 1: enable
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetiBFEnable(struct net_device *prNetDev, UINT_32 u4iBF)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetiBFEnable=%d\n", u4iBF);

	if (u4iBF != 0 && u4iBF != 1)
		return -EINVAL;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_IBF_ENABLE;
	rRfATInfo.u4FuncData = u4iBF;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set eBF Enable.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4eBF		0: disable / 1: enable
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESeteBFEnable(struct net_device *prNetDev, UINT_32 u4eBF)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SeteBFEnable=%d\n", u4eBF);

	if (u4eBF != 0 && u4eBF != 1)
		return -EINVAL;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_EBF_ENABLE;
	rRfATInfo.u4FuncData = u4eBF;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set MAC Address.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4Type		Address type
* \param[in] ucAddr		Address ready to set
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetMACAddress(struct net_device *prNetDev, UINT_32 u4Type, PUINT_8 ucAddr)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, ERROR, "QA_ATE_HOOK SetMACAddress Type = %d, Addr = %02x:%02x:%02x:%02x:%02x:%02x\n",
	       u4Type, ucAddr[0], ucAddr[1], ucAddr[2], ucAddr[3], ucAddr[4], ucAddr[5]);

#if 1
	rRfATInfo.u4FuncIndex = u4Type;
	memcpy(&rRfATInfo.u4FuncData, ucAddr, 4);

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */
	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;
#endif
	rRfATInfo.u4FuncIndex = u4Type | BIT(18);
	memset(&rRfATInfo.u4FuncData, 0, sizeof(rRfATInfo.u4FuncData));
	memcpy(&rRfATInfo.u4FuncData, ucAddr + 4, 2);

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for RX Vector Dump.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4Type
* \param[in] u4On_off
* \param[in] u4Size
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATELogOnOff(struct net_device *prNetDev, UINT_32 u4Type, UINT_32 u4On_off, UINT_32 u4Size)
{
	INT_32 i4Status = 0, i4RXVLength, i, i4TargetLength = 36 * 30;
	UINT_32 u4BufLen = 0, rxv;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATELogOnOff\n");

	switch (u4Type) {
	case ATE_LOG_RXV:
		DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATELogOnOff : ATE_LOG_RXV\n\n");
		break;
	case ATE_LOG_RDD:
		DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATELogOnOff : ATE_LOG_RDD\n\n");
		break;
	case ATE_LOG_RE_CAL:
		DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATELogOnOff : ATE_LOG_RE_CAL\n\n");
		break;
	case ATE_LOG_RXINFO:
		DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATELogOnOff : ATE_LOG_RXINFO\n\n");
		break;
	case ATE_LOG_TXDUMP:
		DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATELogOnOff : ATE_LOG_TXDUMP\n\n");
		break;
	case ATE_LOG_TEST:
		DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATELogOnOff : ATE_LOG_TEST\n\n");
		break;
	default:
		DBGLOG(RFTEST, INFO, "QA_ATE_HOOK log type %d not supported\n\n", u4Type);
	}

	if ((u4On_off == ATE_LOG_DUMP) && (u4Type == ATE_LOG_RXV)) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_RXOK_COUNT;
		rRfATInfo.u4FuncData = 0;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidRftestQueryAutoTest,
				    &rRfATInfo, sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

		if (i4Status == 0) {
			i4TargetLength = rRfATInfo.u4FuncData * 36;

			if (i4TargetLength >= 3600)
				i4TargetLength = 3600;
		}

		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_RESULT_INFO;
		rRfATInfo.u4FuncData = RF_AT_FUNCID_RXV_DUMP;

		i4Status = kalIoctl(prGlueInfo,
				    wlanoidRftestQueryAutoTest,
				    &rRfATInfo, sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

		if (i4Status == 0) {
			i4RXVLength = rRfATInfo.u4FuncData;
			DBGLOG(RFTEST, ERROR, "QA_ATE_HOOK Get RX Vector Total size = %d\n", i4RXVLength);
		} else {
			DBGLOG(RFTEST, ERROR, "QA_ATE_HOOK Get RX Vector Total Size Error!!!!\n\n");
		}

		TOOL_PRINTLOG(RFTEST, ERROR, "[LOG DUMP START]\n");

		for (i = 0; i < i4TargetLength; i += 4) {
			rRfATInfo.u4FuncIndex = RF_AT_FUNCID_RXV_DUMP;
			rRfATInfo.u4FuncData = i;

			i4Status = kalIoctl(prGlueInfo,
					    wlanoidRftestQueryAutoTest,
					    &rRfATInfo, sizeof(rRfATInfo), TRUE, TRUE, TRUE, &u4BufLen);

			if (i4Status == 0) {
				rxv = rRfATInfo.u4FuncData;

				if (i % 36 == 0)
					TOOL_PRINTLOG(RFTEST, ERROR, "[RXV DUMP START][%d]\n", (i / 36) + 1);

				TOOL_PRINTLOG(RFTEST, ERROR, "[RXVD%d]%08x\n", ((i % 36) / 4) + 1, rxv);

				if (((i % 36) / 4) + 1 == 9)
					TOOL_PRINTLOG(RFTEST, ERROR, "[RXV DUMP END]\n");
			}
		}

		TOOL_PRINTLOG(RFTEST, ERROR, "[LOG DUMP END]\n");
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Reset Counter.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEResetTXRXCounter(struct net_device *prNetDev)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATEResetTXRXCounter\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_RESETTXRXCOUNTER;
	rRfATInfo.u4FuncData = 0;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set DBDC Band Index.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4BandIdx       Band Index Number ready to set
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetDBDCBandIndex(struct net_device *prNetDev, UINT_32 u4BandIdx)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATESetDBDCBandIndex\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_DBDC_BAND_IDX;
	rRfATInfo.u4FuncData = u4BandIdx;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Band. (2G or 5G)
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] i4Band		Band to set
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetBand(struct net_device *prNetDev, INT_32 i4Band)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATESetBand\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_BAND;
	rRfATInfo.u4FuncData = i4Band;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Tx Tone Type. (2G or 5G)
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] i4ToneType	Set Single or Two Tone.
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxToneType(struct net_device *prNetDev, INT_32 i4ToneType)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATESetTxToneType\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TONE_TYPE;
	rRfATInfo.u4FuncData = i4ToneType;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Tx Tone Frequency. (DC/5M/10M/20M/40M)
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] i4ToneFreq	Set Tx Tone Frequency.
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxToneBW(struct net_device *prNetDev, INT_32 i4ToneFreq)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATESetTxToneBW\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TONE_BW;
	rRfATInfo.u4FuncData = i4ToneFreq;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Tx Tone DC Offset. (DC Offset I / DC Offset Q)
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] i4DcOffsetI	Set Tx Tone DC Offset I.
* \param[in] i4DcOffsetQ	Set Tx Tone DC Offset Q.
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetTxToneDCOffset(struct net_device *prNetDev, INT_32 i4DcOffsetI, INT_32 i4DcOffsetQ)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATESetTxToneDCOffset\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TONE_DC_OFFSET;
	rRfATInfo.u4FuncData = i4DcOffsetQ << 16 | i4DcOffsetI;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set Tx Tone Power. (RF and Digital)
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] i4AntIndex
* \param[in] i4RF_Power
* \param[in] i4Digi_Power
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetDBDCTxTonePower(struct net_device *prNetDev, INT_32 i4AntIndex, INT_32 i4RF_Power, INT_32 i4Digi_Power)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATESetDBDCTxTonePower\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TONE_RF_GAIN;
	rRfATInfo.u4FuncData = i4AntIndex << 16 | i4RF_Power;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TONE_DIGITAL_GAIN;
	rRfATInfo.u4FuncData = i4AntIndex << 16 | i4Digi_Power;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Start Tx Tone.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] i4Control		Start or Stop TX Tone.
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEDBDCTxTone(struct net_device *prNetDev, INT_32 i4Control)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATEDBDCTxTone\n");

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	if (i4Control) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
		rRfATInfo.u4FuncData = RF_AT_COMMAND_SINGLE_TONE;
	} else {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
		rRfATInfo.u4FuncData = RF_AT_COMMAND_STOPTEST;
	}

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Set TX Mac Header.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u4BandIdx       Band Index Number ready to set
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATESetMacHeader(struct net_device *prNetDev, UINT_32 u4FrameCtrl, UINT_32 u4DurationID, UINT_32 u4SeqCtrl)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATESetMacHeader\n");

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_MAC_HEADER;
	rRfATInfo.u4FuncData = u4FrameCtrl || (u4DurationID << 16);

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_SEQ_CTRL;
	rRfATInfo.u4FuncData = u4SeqCtrl;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for IRR Set ADC. (RF_AT_FUNCID_SET_ADC)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATE_IRRSetADC(struct net_device *prNetDev,
			UINT_32 u4WFIdx,
			UINT_32 u4ChFreq,
			UINT_32 u4BW, UINT_32 u4Sx, UINT_32 u4Band, UINT_32 u4RunType, UINT_32 u4FType)
{
	UINT_32 u4BufLen = 0, i = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 au4Param[7];

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATE_IRRSetADC\n");

	if (u4BW == 3 || u4BW == 4 || u4BW > 5)
		return -EINVAL;

	if (u4BW == 5)		/* For BW160, UI will pass "5" */
		u4BW = 3;

	au4Param[0] = u4ChFreq;
	au4Param[1] = u4WFIdx;
	au4Param[2] = u4BW;
	au4Param[3] = u4Sx;
	au4Param[4] = u4Band;
	au4Param[5] = u4RunType;
	au4Param[6] = u4FType;

	for (i = 0; i < 8; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_ADC | (i << 16);
		if (i < 7)
			rRfATInfo.u4FuncData = au4Param[i];
		else
			rRfATInfo.u4FuncData = 0;

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for IRR Set RX Gain. (RF_AT_FUNCID_SET_RX_GAIN)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATE_IRRSetRxGain(struct net_device *prNetDev,
			   UINT_32 u4PgaLpfg, UINT_32 u4Lna, UINT_32 u4Band, UINT_32 u4WF_inx, UINT_32 u4Rfdgc)
{
	UINT_32 u4BufLen = 0, i = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 au4Param[5];

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATE_IRRSetRxGain\n");

	au4Param[0] = u4PgaLpfg;
	au4Param[1] = u4Lna;
	au4Param[2] = u4Band;
	au4Param[3] = u4WF_inx;
	au4Param[4] = u4Rfdgc;

	for (i = 0; i < 6; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_RX_GAIN | (i << 16);
		if (i < 5)
			rRfATInfo.u4FuncData = au4Param[i];
		else
			rRfATInfo.u4FuncData = 0;

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for IRR Set TTG. (RF_AT_FUNCID_SET_TTG)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATE_IRRSetTTG(struct net_device *prNetDev,
			UINT_32 u4TTGPwrIdx, UINT_32 u4ChFreq, UINT_32 u4FIToneFreq, UINT_32 u4Band)
{
	UINT_32 u4BufLen = 0, i = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 au4Param[4];

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATE_IRRSetTTG\n");

	au4Param[0] = u4ChFreq;
	au4Param[1] = u4FIToneFreq;
	au4Param[2] = u4TTGPwrIdx;
	au4Param[3] = u4Band;

	for (i = 0; i < 5; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TTG | (i << 16);
		if (i < 4)
			rRfATInfo.u4FuncData = au4Param[i];
		else
			rRfATInfo.u4FuncData = 0;

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for IRR Set TTG On/Off. (RF_AT_FUNCID_TTG_ON_OFF)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATE_IRRSetTrunOnTTG(struct net_device *prNetDev, UINT_32 u4TTGOnOff, UINT_32 u4Band, UINT_32 u4WF_inx)
{
	UINT_32 u4BufLen = 0, i = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;
	UINT_32 au4Param[3];

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATE_IRRSetTrunOnTTG\n");

	au4Param[0] = u4TTGOnOff;
	au4Param[1] = u4Band;
	au4Param[2] = u4WF_inx;

	for (i = 0; i < 4; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_TTG_ON_OFF | (i << 16);
		if (i < 3)
			rRfATInfo.u4FuncData = au4Param[i];
		else
			rRfATInfo.u4FuncData = 0;

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for IRR Set TTG On/Off.
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATE_TMRSetting(struct net_device *prNetDev,
			 UINT_32 u4Setting, UINT_32 u4Version, UINT_32 u4MPThres, UINT_32 u4MPIter)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATE_TMRSetting\n");

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TMR_ROLE;
	rRfATInfo.u4FuncData = u4Setting;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TMR_MODULE;
	rRfATInfo.u4FuncData = u4Version;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TMR_DBM;
	rRfATInfo.u4FuncData = u4MPThres;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_TMR_ITER;
	rRfATInfo.u4FuncData = u4MPIter;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for MPS Setting. (Set Seq Data)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEMPSSetSeqData(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4Phy, UINT_32 u4Band)
{
	UINT_32 u4BufLen = 0, i;
	INT_32 i4Status;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATEMPSSetSeqData\n");

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_MPS_SIZE;
	rRfATInfo.u4FuncData = u4TestNum;

	i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
			    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
			    &rRfATInfo,	/* pvInfoBuf */
			    sizeof(rRfATInfo),	/* u4InfoBufLen */
			    FALSE,	/* fgRead */
			    FALSE,	/* fgWaitResp */
			    TRUE,	/* fgCmd */
			    &u4BufLen);	/* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	for (i = 0 ; i < u4TestNum ; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_MPS_SEQ_DATA | (i << 16);
		rRfATInfo.u4FuncData = pu4Phy[i];

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for MPS Setting. (Set Payload Length)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEMPSSetPayloadLength(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4Length, UINT_32 u4Band)
{
	UINT_32 u4BufLen = 0, i;
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATEMPSSetPayloadLength\n");

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	for (i = 0 ; i < u4TestNum ; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_MPS_PAYLOAD_LEN | (i << 16);
		rRfATInfo.u4FuncData = pu4Length[i];

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for MPS Setting. (Set Packet Count)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEMPSSetPacketCount(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4PktCnt, UINT_32 u4Band)
{
	UINT_32 u4BufLen = 0, i;
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATEMPSSetPacketCount\n");

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	for (i = 0 ; i < u4TestNum ; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_MPS_PKT_CNT | (i << 16);
		rRfATInfo.u4FuncData = pu4PktCnt[i];

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for MPS Setting. (Set Power Gain)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEMPSSetPowerGain(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4PwrGain, UINT_32 u4Band)
{
	UINT_32 u4BufLen = 0, i;
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATEMPSSetPowerGain\n");

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	for (i = 0 ; i < u4TestNum ; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_MPS_PWR_GAIN | (i << 16);
		rRfATInfo.u4FuncData = pu4PwrGain[i];

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for MPS Setting. (Set NSS)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEMPSSetNss(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4Nss, UINT_32 u4Band)
{
	UINT_32 u4BufLen = 0, i;
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATEMPSSetNss\n");

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	for (i = 0 ; i < u4TestNum ; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_MPS_NSS | (i << 16);
		rRfATInfo.u4FuncData = pu4Nss[i];

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for MPS Setting. (Set NSS)
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEMPSSetPerpacketBW(struct net_device *prNetDev,
			UINT_32 u4TestNum, UINT_32 *pu4PerPktBW, UINT_32 u4Band)
{
	UINT_32 u4BufLen = 0, i;
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK MT_ATEMPSSetPerpacketBW\n");

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	for (i = 0 ; i < u4TestNum ; i++) {
		rRfATInfo.u4FuncIndex = RF_AT_FUNCID_SET_MPS_PACKAGE_BW | (i << 16);
		rRfATInfo.u4FuncData = pu4PerPktBW[i];

		i4Status = kalIoctl(prGlueInfo,	/* prGlueInfo */
				    wlanoidRftestSetAutoTest,	/* pfnOidHandler */
				    &rRfATInfo,	/* pvInfoBuf */
				    sizeof(rRfATInfo),	/* u4InfoBufLen */
				    FALSE,	/* fgRead */
				    FALSE,	/* fgWaitResp */
				    TRUE,	/* fgCmd */
				    &u4BufLen);	/* pu4QryInfoLen */

		if (i4Status != WLAN_STATUS_SUCCESS)
			return -EFAULT;
	}

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Start RDD.
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATERDDStart(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetATE = %s\n", prInBuf);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
	rRfATInfo.u4FuncData = RF_AT_COMMAND_RDD;

	i4Status = kalIoctl(prGlueInfo, /* prGlueInfo */
				wlanoidRftestSetAutoTest,   /* pfnOidHandler */
				&rRfATInfo, /* pvInfoBuf */
				sizeof(rRfATInfo),  /* u4InfoBufLen */
				FALSE,  /* fgRead */
				FALSE,  /* fgWaitResp */
				TRUE,   /* fgCmd */
				&u4BufLen); /* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Stop RDD.
*
* \param[in] prNetDev		Pointer to the Net Device
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATERDDStop(struct net_device *prNetDev, UINT_8 *prInBuf)
{
	UINT_32 u4BufLen = 0;
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	PARAM_MTK_WIFI_TEST_STRUCT_T rRfATInfo;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));

	DBGLOG(RFTEST, INFO, "QA_ATE_HOOK SetATE = %s\n", prInBuf);

	rRfATInfo.u4FuncIndex = RF_AT_FUNCID_COMMAND;
	rRfATInfo.u4FuncData = RF_AT_COMMAND_RDD_OFF;

	i4Status = kalIoctl(prGlueInfo, /* prGlueInfo */
				wlanoidRftestSetAutoTest,   /* pfnOidHandler */
				&rRfATInfo, /* pvInfoBuf */
				sizeof(rRfATInfo),  /* u4InfoBufLen */
				FALSE,  /* fgRead */
				FALSE,  /* fgWaitResp */
				TRUE,   /* fgCmd */
				&u4BufLen); /* pu4QryInfoLen */

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;

}


/*----------------------------------------------------------------------------*/
/*!
* \brief  Hook API for Write Efuse.
*
* \param[in] prNetDev		Pointer to the Net Device
* \param[in] u2Offset		Efuse offset
* \param[in] u2Content		Efuse content
* \param[out] None
*
* \retval 0				On success.
* \retval -EFAULT			If kalIoctl return nonzero.
* \retval -EINVAL			If invalid argument.
*/
/*----------------------------------------------------------------------------*/
INT_32 MT_ATEWriteEfuse(struct net_device *prNetDev, UINT_16 u2Offset, UINT_16 u2Content)
{
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_ACCESS_EFUSE_T rAccessEfuseInfoRead, rAccessEfuseInfoWrite;
	P_GLUE_INFO_T prGlueInfo = NULL;
	WLAN_STATUS i4Status = WLAN_STATUS_SUCCESS;
	UINT_8  u4Index = 0, u4Loop = 0;

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	kalMemSet(&rAccessEfuseInfoRead, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));
	kalMemSet(&rAccessEfuseInfoWrite, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));



	/* Read */
	DBGLOG(INIT, INFO, "QA_AGENT HQA_WriteBulkEEPROM  Read\n");
	kalMemSet(&rAccessEfuseInfoRead, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));
	rAccessEfuseInfoRead.u4Address = (u2Offset / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;
	i4Status = kalIoctl(prGlueInfo,
				wlanoidQueryProcessAccessEfuseRead,
				&rAccessEfuseInfoRead,
				sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), TRUE, TRUE, TRUE, &u4BufLen);


	/* Write */
	kalMemSet(&rAccessEfuseInfoWrite, 0, sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T));
	u4Index = u2Offset % EFUSE_BLOCK_SIZE;

	if (u4Index == 15) {
		DBGLOG(INIT, ERROR, "u4Index == 15, u2Offset = %u.\n", u2Offset);
		return -EFAULT;
	}

	prGlueInfo->prAdapter->aucEepromVaule[u4Index] = u2Content;
	prGlueInfo->prAdapter->aucEepromVaule[u4Index+1] = u2Content >> 8 & 0xff;

	kalMemCopy(rAccessEfuseInfoWrite.aucData, prGlueInfo->prAdapter->aucEepromVaule, 16);

	for (u4Loop = 0; u4Loop < (EFUSE_BLOCK_SIZE); u4Loop++) {
		DBGLOG(INIT, INFO, "QA_AGENT aucEepromVaule u4Loop=%d  u4Value=%x\n",
			u4Loop, prGlueInfo->prAdapter->aucEepromVaule[u4Loop]);

		DBGLOG(INIT, INFO, "QA_AGENT rAccessEfuseInfoWrite.aucData u4Loop=%d  u4Value=%x\n",
			u4Loop, rAccessEfuseInfoWrite.aucData[u4Loop]);
	}

	rAccessEfuseInfoWrite.u4Address = (u2Offset / EFUSE_BLOCK_SIZE)*EFUSE_BLOCK_SIZE;

	i4Status = kalIoctl(prGlueInfo,
				wlanoidQueryProcessAccessEfuseWrite,
				&rAccessEfuseInfoWrite,
				sizeof(PARAM_CUSTOM_ACCESS_EFUSE_T), FALSE, TRUE, TRUE, &u4BufLen);

	if (i4Status != WLAN_STATUS_SUCCESS)
		return -EFAULT;

	return i4Status;
}


#if CFG_SUPPORT_TX_BF
INT_32 TxBfProfileTag_InValid(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucInValid)
{
	INT_32 i4Status = 0;

	prPfmuTag1->rField.ucInvalidProf = ucInValid;

	return i4Status;
}

INT_32 TxBfProfileTag_PfmuIdx(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucProfileIdx)
{
	INT_32 i4Status = 0;

	prPfmuTag1->rField.ucProfileID = ucProfileIdx;

	return i4Status;
}

INT_32 TxBfProfileTag_TxBfType(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucBFType)
{
	INT_32 i4Status = 0;

	prPfmuTag1->rField.ucTxBf = ucBFType;

	return i4Status;
}

INT_32 TxBfProfileTag_DBW(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucBW)
{
	INT_32 i4Status = 0;

	prPfmuTag1->rField.ucDBW = ucBW;

	return i4Status;
}

INT_32 TxBfProfileTag_SuMu(struct net_device *prNetDev, P_PFMU_PROFILE_TAG1 prPfmuTag1, UINT_8 ucSuMu)
{
	INT_32 i4Status = 0;

	prPfmuTag1->rField.ucSU_MU = ucSuMu;

	return i4Status;
}

INT_32 TxBfProfileTag_Mem(struct net_device *prNetDev,
			  P_PFMU_PROFILE_TAG1 prPfmuTag1, PUINT_8 aucMemAddrColIdx, PUINT_8 aucMemAddrRowIdx)
{
	INT_32 i4Status = 0;

	prPfmuTag1->rField.ucMemAddr1ColIdx = aucMemAddrColIdx[0];
	prPfmuTag1->rField.ucMemAddr1RowIdx = aucMemAddrRowIdx[0];
	prPfmuTag1->rField.ucMemAddr2ColIdx = aucMemAddrColIdx[1];
	prPfmuTag1->rField.ucMemAddr2RowIdx = aucMemAddrRowIdx[1] & 0x1F;
	prPfmuTag1->rField.ucMemAddr2RowIdxMsb = aucMemAddrRowIdx[1] >> 5;
	prPfmuTag1->rField.ucMemAddr3ColIdx = aucMemAddrColIdx[2];
	prPfmuTag1->rField.ucMemAddr3RowIdx = aucMemAddrRowIdx[2];
	prPfmuTag1->rField.ucMemAddr4ColIdx = aucMemAddrColIdx[3];
	prPfmuTag1->rField.ucMemAddr4RowIdx = aucMemAddrRowIdx[3];

	return i4Status;
}

INT_32 TxBfProfileTag_Matrix(struct net_device *prNetDev,
			     P_PFMU_PROFILE_TAG1 prPfmuTag1,
			     UINT_8 ucNrow,
			     UINT_8 ucNcol, UINT_8 ucNgroup, UINT_8 ucLM, UINT_8 ucCodeBook, UINT_8 ucHtcExist)
{
	INT_32 i4Status = 0;

	prPfmuTag1->rField.ucNrow = ucNrow;
	prPfmuTag1->rField.ucNcol = ucNcol;
	prPfmuTag1->rField.ucNgroup = ucNgroup;
	prPfmuTag1->rField.ucLM = ucLM;
	prPfmuTag1->rField.ucCodeBook = ucCodeBook;
	prPfmuTag1->rField.ucHtcExist = ucHtcExist;

	return i4Status;
}

INT_32 TxBfProfileTag_SNR(struct net_device *prNetDev,
			  P_PFMU_PROFILE_TAG1 prPfmuTag1,
			  UINT_8 ucSNR_STS0, UINT_8 ucSNR_STS1, UINT_8 ucSNR_STS2, UINT_8 ucSNR_STS3)
{
	INT_32 i4Status = 0;

	prPfmuTag1->rField.ucSNR_STS0 = ucSNR_STS0;
	prPfmuTag1->rField.ucSNR_STS1 = ucSNR_STS1;
	prPfmuTag1->rField.ucSNR_STS2 = ucSNR_STS2;
	prPfmuTag1->rField.ucSNR_STS3 = ucSNR_STS3;

	return i4Status;
}

INT_32 TxBfProfileTag_SmtAnt(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucSmartAnt)
{
	INT_32 i4Status = 0;

	prPfmuTag2->rField.u2SmartAnt = ucSmartAnt;

	return i4Status;
}

INT_32 TxBfProfileTag_SeIdx(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucSeIdx)
{
	INT_32 i4Status = 0;

	prPfmuTag2->rField.ucSEIdx = ucSeIdx;

	return i4Status;
}

INT_32 TxBfProfileTag_RmsdThd(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucRmsdThrd)
{
	INT_32 i4Status = 0;

	prPfmuTag2->rField.ucRMSDThd = ucRmsdThrd;

	return i4Status;
}

INT_32 TxBfProfileTag_McsThd(struct net_device *prNetDev,
			     P_PFMU_PROFILE_TAG2 prPfmuTag2, PUINT_8 pMCSThLSS, PUINT_8 pMCSThSSS)
{
	INT_32 i4Status = 0;

	prPfmuTag2->rField.ucMCSThL1SS = pMCSThLSS[0];
	prPfmuTag2->rField.ucMCSThS1SS = pMCSThSSS[0];
	prPfmuTag2->rField.ucMCSThL2SS = pMCSThLSS[1];
	prPfmuTag2->rField.ucMCSThS2SS = pMCSThSSS[1];
	prPfmuTag2->rField.ucMCSThL3SS = pMCSThLSS[2];
	prPfmuTag2->rField.ucMCSThS3SS = pMCSThSSS[2];

	return i4Status;
}

INT_32 TxBfProfileTag_TimeOut(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucTimeOut)
{
	INT_32 i4Status = 0;

	prPfmuTag2->rField.uciBfTimeOut = ucTimeOut;

	return i4Status;
}

INT_32 TxBfProfileTag_DesiredBW(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucDesiredBW)
{
	INT_32 i4Status = 0;

	prPfmuTag2->rField.uciBfDBW = ucDesiredBW;

	return i4Status;
}

INT_32 TxBfProfileTag_DesiredNc(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucDesiredNc)
{
	INT_32 i4Status = 0;

	prPfmuTag2->rField.uciBfNcol = ucDesiredNc;

	return i4Status;
}

INT_32 TxBfProfileTag_DesiredNr(struct net_device *prNetDev, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 ucDesiredNr)
{
	INT_32 i4Status = 0;

	prPfmuTag2->rField.uciBfNrow = ucDesiredNr;

	return i4Status;
}

INT_32 TxBfProfileTagWrite(struct net_device *prNetDev,
			   P_PFMU_PROFILE_TAG1 prPfmuTag1, P_PFMU_PROFILE_TAG2 prPfmuTag2, UINT_8 profileIdx)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : au4RawData[0] = 0x%08x\n", prPfmuTag1->au4RawData[0]);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : au4RawData[1] = 0x%08x\n", prPfmuTag1->au4RawData[1]);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : au4RawData[2] = 0x%08x\n", prPfmuTag1->au4RawData[2]);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : au4RawData[3] = 0x%08x\n", prPfmuTag1->au4RawData[3]);

	DBGLOG(RFTEST, ERROR, "prPfmuTag2 : au4RawData[0] = 0x%08x\n", prPfmuTag2->au4RawData[0]);
	DBGLOG(RFTEST, ERROR, "prPfmuTag2 : au4RawData[1] = 0x%08x\n", prPfmuTag2->au4RawData[1]);
	DBGLOG(RFTEST, ERROR, "prPfmuTag2 : au4RawData[2] = 0x%08x\n", prPfmuTag2->au4RawData[2]);

	DBGLOG(RFTEST, ERROR,
		"prPfmuTag1 : prPfmuTag1->rField.ucProfileID= %d\n", prPfmuTag1->rField.ucProfileID);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucTxBf= %d\n", prPfmuTag1->rField.ucTxBf);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucDBW= %d\n", prPfmuTag1->rField.ucDBW);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucSU_MU= %d\n", prPfmuTag1->rField.ucSU_MU);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucInvalidProf= %d\n",
	       prPfmuTag1->rField.ucInvalidProf);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucRMSD= %d\n", prPfmuTag1->rField.ucRMSD);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucMemAddr1ColIdx= %d\n",
	       prPfmuTag1->rField.ucMemAddr1ColIdx);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucMemAddr1RowIdx= %d\n",
	       prPfmuTag1->rField.ucMemAddr1RowIdx);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucMemAddr2ColIdx= %d\n",
	       prPfmuTag1->rField.ucMemAddr2ColIdx);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucMemAddr2RowIdx= %d\n",
	       prPfmuTag1->rField.ucMemAddr2RowIdx);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucMemAddr2RowIdxMsb= %d\n",
	       prPfmuTag1->rField.ucMemAddr2RowIdxMsb);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucMemAddr3ColIdx= %d\n",
	       prPfmuTag1->rField.ucMemAddr3ColIdx);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucMemAddr3RowIdx= %d\n",
	       prPfmuTag1->rField.ucMemAddr3RowIdx);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucMemAddr4ColIdx= %d\n",
	       prPfmuTag1->rField.ucMemAddr4ColIdx);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucMemAddr4RowIdx= %d\n",
	       prPfmuTag1->rField.ucMemAddr4RowIdx);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucReserved= %d\n", prPfmuTag1->rField.ucReserved);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucNrow= %d\n", prPfmuTag1->rField.ucNrow);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucNcol= %d\n", prPfmuTag1->rField.ucNcol);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucNgroup= %d\n", prPfmuTag1->rField.ucNgroup);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucLM= %d\n", prPfmuTag1->rField.ucLM);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucCodeBook= %d\n", prPfmuTag1->rField.ucCodeBook);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucHtcExist= %d\n", prPfmuTag1->rField.ucHtcExist);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag1 : prPfmuTag1->rField.ucReserved1= %d\n", prPfmuTag1->rField.ucReserved1);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucSNR_STS0= %d\n", prPfmuTag1->rField.ucSNR_STS0);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucSNR_STS1= %d\n", prPfmuTag1->rField.ucSNR_STS1);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucSNR_STS2= %d\n", prPfmuTag1->rField.ucSNR_STS2);
	DBGLOG(RFTEST, ERROR, "prPfmuTag1 : prPfmuTag1->rField.ucSNR_STS3= %d\n", prPfmuTag1->rField.ucSNR_STS3);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag1 : prPfmuTag1->rField.ucIBfLnaIdx= %d\n", prPfmuTag1->rField.ucIBfLnaIdx);

	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.u2SmartAnt = %d\n", prPfmuTag2->rField.u2SmartAnt);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucReserved0 = %d\n", prPfmuTag2->rField.ucReserved0);
	DBGLOG(RFTEST, ERROR, "prPfmuTag2 : prPfmuTag2->rField.ucSEIdx = %d\n", prPfmuTag2->rField.ucSEIdx);
	DBGLOG(RFTEST, ERROR, "prPfmuTag2 : prPfmuTag2->rField.ucRMSDThd = %d\n", prPfmuTag2->rField.ucRMSDThd);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucReserved1 = %d\n", prPfmuTag2->rField.ucReserved1);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucMCSThL1SS = %d\n", prPfmuTag2->rField.ucMCSThL1SS);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucMCSThS1SS = %d\n", prPfmuTag2->rField.ucMCSThS1SS);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucMCSThL2SS = %d\n", prPfmuTag2->rField.ucMCSThL2SS);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucMCSThS2SS = %d\n", prPfmuTag2->rField.ucMCSThS2SS);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucMCSThL3SS = %d\n", prPfmuTag2->rField.ucMCSThL3SS);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucMCSThS3SS = %d\n", prPfmuTag2->rField.ucMCSThS3SS);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.uciBfTimeOut = %d\n",
	       prPfmuTag2->rField.uciBfTimeOut);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucReserved2 = %d\n", prPfmuTag2->rField.ucReserved2);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucReserved3 = %d\n", prPfmuTag2->rField.ucReserved3);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.ucReserved4 = %d\n", prPfmuTag2->rField.ucReserved4);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.uciBfDBW = %d\n", prPfmuTag2->rField.uciBfDBW);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.uciBfNcol = %d\n", prPfmuTag2->rField.uciBfNcol);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.uciBfNrow = %d\n", prPfmuTag2->rField.uciBfNrow);
	DBGLOG(RFTEST, ERROR,
		"prPfmuTag2 : prPfmuTag2->rField.u2Reserved5 = %d\n", prPfmuTag2->rField.u2Reserved5);

	rTxBfActionInfo.rProfileTagWrite.ucTxBfCategory = BF_PFMU_TAG_WRITE;
	rTxBfActionInfo.rProfileTagWrite.ucPfmuId = profileIdx;
	memcpy(&rTxBfActionInfo.rProfileTagWrite.ucBuffer, prPfmuTag1, sizeof(PFMU_PROFILE_TAG1));
	memcpy(&rTxBfActionInfo.rProfileTagWrite.ucBuffer[16], prPfmuTag2, sizeof(PFMU_PROFILE_TAG2));

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction,
			    &rTxBfActionInfo, sizeof(rTxBfActionInfo), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfProfileTagRead(struct net_device *prNetDev, UINT_8 profileIdx, UINT_8 fgBFer)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "TxBfProfileTagRead : profileIdx = 0x%08x\n", profileIdx);
	DBGLOG(RFTEST, ERROR, "TxBfProfileTagRead : fgBFer = 0x%08x\n", fgBFer);

	rTxBfActionInfo.rProfileTagRead.ucTxBfCategory = BF_PFMU_TAG_READ;
	rTxBfActionInfo.rProfileTagRead.ucProfileIdx = profileIdx;
	rTxBfActionInfo.rProfileTagRead.fgBfer = fgBFer;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction, &rTxBfActionInfo, sizeof(rTxBfActionInfo), TRUE, TRUE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 StaRecCmmUpdate(struct net_device *prNetDev,
		       UINT_8 ucWlanId, UINT_8 ucBssId, UINT_8 u4Aid, UINT_8 aucMacAddr[MAC_ADDR_LEN]
)
{
	CMD_STAREC_COMMON_T rStaRecCmm;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	INT_32 i4Status = 0;

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rStaRecCmm, sizeof(CMD_STAREC_COMMON_T));
	/* Tag assignment */
	rStaRecCmm.u2Tag = STA_REC_BASIC;
	rStaRecCmm.u2Length = sizeof(CMD_STAREC_COMMON_T);

	/* content */
	kalMemCopy(rStaRecCmm.aucPeerMacAddr, aucMacAddr, MAC_ADDR_LEN);
	rStaRecCmm.ucConnectionState = TRUE;
	rStaRecCmm.u2AID = u4Aid;
	rStaRecCmm.u2Reserve1 = ucWlanId;

	DBGLOG(RFTEST, ERROR, "ucWlanId = 0x%08x\n", ucWlanId);

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidStaRecUpdate,
			    &rStaRecCmm, sizeof(CMD_STAREC_COMMON_T), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 StaRecBfUpdate(struct net_device *prNetDev,
		      STA_REC_BF_UPD_ARGUMENT rStaRecBfUpdArg, UINT_8 aucMemRow[4], UINT_8 aucMemCol[4]
)
{
	CMD_STAREC_BF rStaRecBF;
	/* PARAM_CUSTOM_STA_REC_UPD_STRUCT_T rStaRecUpdateInfo = {0}; */
	/* P_STA_RECORD_T                        prStaRec; */
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	INT_32 i4Status = 0;

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rStaRecBF, sizeof(CMD_STAREC_BF));
	/* Tag assignment */
	rStaRecBF.u2Tag = STA_REC_BF;
	rStaRecBF.u2Length = sizeof(CMD_STAREC_BF);
	rStaRecBF.ucReserved[0] = rStaRecBfUpdArg.u4BssId;
	rStaRecBF.ucReserved[1] = rStaRecBfUpdArg.u4WlanId;
	/* content */
	rStaRecBF.rTxBfPfmuInfo.u2PfmuId = rStaRecBfUpdArg.u4PfmuId;
	rStaRecBF.rTxBfPfmuInfo.ucTotMemRequire = rStaRecBfUpdArg.u4TotalMemReq;
	rStaRecBF.rTxBfPfmuInfo.ucMemRequire20M = rStaRecBfUpdArg.u4MemReq20M;
	rStaRecBF.rTxBfPfmuInfo.ucMemRow0 = aucMemRow[0];
	rStaRecBF.rTxBfPfmuInfo.ucMemCol0 = aucMemCol[0];
	rStaRecBF.rTxBfPfmuInfo.ucMemRow1 = aucMemRow[1];
	rStaRecBF.rTxBfPfmuInfo.ucMemCol1 = aucMemCol[1];
	rStaRecBF.rTxBfPfmuInfo.ucMemRow2 = aucMemRow[2];
	rStaRecBF.rTxBfPfmuInfo.ucMemCol2 = aucMemCol[2];
	rStaRecBF.rTxBfPfmuInfo.ucMemRow3 = aucMemRow[3];
	rStaRecBF.rTxBfPfmuInfo.ucMemCol3 = aucMemCol[3];
	/* 0 : SU, 1 : MU */
	rStaRecBF.rTxBfPfmuInfo.fgSU_MU = rStaRecBfUpdArg.u4SuMu;
	/* 0: iBF, 1: eBF */
	rStaRecBF.rTxBfPfmuInfo.fgETxBfCap = rStaRecBfUpdArg.u4eTxBfCap;
	/* 0: legacy, 1: OFDM, 2: HT, 4: VHT */
	rStaRecBF.rTxBfPfmuInfo.ucSoundingPhy = 1;
	rStaRecBF.rTxBfPfmuInfo.ucNdpaRate = rStaRecBfUpdArg.u4NdpaRate;
	rStaRecBF.rTxBfPfmuInfo.ucNdpRate = rStaRecBfUpdArg.u4NdpRate;
	rStaRecBF.rTxBfPfmuInfo.ucReptPollRate = rStaRecBfUpdArg.u4ReptPollRate;
	/* 0: legacy, 1: OFDM, 2: HT, 4: VHT */
	rStaRecBF.rTxBfPfmuInfo.ucTxMode = rStaRecBfUpdArg.u4TxMode;
	rStaRecBF.rTxBfPfmuInfo.ucNc = rStaRecBfUpdArg.u4Nc;
	rStaRecBF.rTxBfPfmuInfo.ucNr = rStaRecBfUpdArg.u4Nr;
	/* 0 : 20M, 1 : 40M, 2 : 80M, 3 : 80 + 80M */
	rStaRecBF.rTxBfPfmuInfo.ucCBW = rStaRecBfUpdArg.u4Bw;
	rStaRecBF.rTxBfPfmuInfo.ucSEIdx = rStaRecBfUpdArg.u4SpeIdx;
	/* Default setting */
	rStaRecBF.rTxBfPfmuInfo.u2SmartAnt = 0;
	rStaRecBF.rTxBfPfmuInfo.uciBfTimeOut = 0;
	rStaRecBF.rTxBfPfmuInfo.uciBfDBW = 0;
	rStaRecBF.rTxBfPfmuInfo.uciBfNcol = 0;
	rStaRecBF.rTxBfPfmuInfo.uciBfNrow = 0;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidStaRecBFUpdate, &rStaRecBF, sizeof(CMD_STAREC_BF), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 DevInfoUpdate(struct net_device *prNetDev, UINT_8 ucOwnMacIdx, UINT_8 fgBand, UINT_8 aucMacAddr[MAC_ADDR_LEN])
{
	CMD_DEVINFO_ACTIVE_T rDevInfo;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	INT_32 i4Status = 0;

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rDevInfo, sizeof(CMD_DEVINFO_ACTIVE_T));
	/* Tag assignment */
	rDevInfo.u2Tag = DEV_INFO_ACTIVE;
	rDevInfo.u2Length = sizeof(CMD_DEVINFO_ACTIVE_T);
	/* content */
	kalMemCopy(rDevInfo.aucOwnMacAddr, aucMacAddr, MAC_ADDR_LEN);
	rDevInfo.ucActive = TRUE;
	rDevInfo.ucBandNum = 0;
	rDevInfo.aucReserve[0] = ucOwnMacIdx;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidDevInfoActive,
			    &rDevInfo, sizeof(CMD_DEVINFO_ACTIVE_T), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 BssInfoUpdate(struct net_device *prNetDev, UINT_8 ucOwnMacIdx, UINT_8 ucBssIdx, UINT_8 ucBssId[MAC_ADDR_LEN])
{
	CMD_BSSINFO_BASIC_T rBssInfo;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	INT_32 i4Status = 0;

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rBssInfo, sizeof(CMD_BSSINFO_BASIC_T));
	/* Tag assignment */
	rBssInfo.u2Tag = BSS_INFO_BASIC;
	rBssInfo.u2Length = sizeof(CMD_BSSINFO_BASIC_T);
	/* content */
	kalMemCopy(rBssInfo.aucBSSID, ucBssId, MAC_ADDR_LEN);
	rBssInfo.ucBcMcWlanidx = ucBssIdx;
	rBssInfo.ucActive = TRUE;
	rBssInfo.u4NetworkType = NETWORK_TYPE_AIS;
	rBssInfo.u2BcnInterval = 100;
	rBssInfo.ucDtimPeriod = 1;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidBssInfoBasic, &rBssInfo, sizeof(CMD_BSSINFO_BASIC_T), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfProfileDataRead(struct net_device *prNetDev,
			   UINT_8 profileIdx, UINT_8 fgBFer, UINT_8 ucSubCarrIdxMsb, UINT_8 ucSubCarrIdxLsb)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "TxBfProfileDataRead : ucPfmuIdx = 0x%08x\n", profileIdx);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataRead : fgBFer = 0x%08x\n", fgBFer);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataRead : ucSubCarrIdxMsb = 0x%08x\n", ucSubCarrIdxMsb);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataRead : ucSubCarrIdxLsb = 0x%08x\n", ucSubCarrIdxLsb);

	rTxBfActionInfo.rProfileDataRead.ucTxBfCategory = BF_PROFILE_READ;
	rTxBfActionInfo.rProfileDataRead.ucPfmuIdx = profileIdx;
	rTxBfActionInfo.rProfileDataRead.fgBFer = fgBFer;
	rTxBfActionInfo.rProfileDataRead.ucSubCarrIdxMsb = ucSubCarrIdxMsb;
	rTxBfActionInfo.rProfileDataRead.ucSubCarrIdxLsb = ucSubCarrIdxLsb;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction, &rTxBfActionInfo, sizeof(rTxBfActionInfo), TRUE, TRUE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfProfileDataWrite(struct net_device *prNetDev,
			    UINT_8 profileIdx,
			    UINT_16 u2SubCarrIdx, UINT_16 au2Phi[6], UINT_8 aucPsi[6], UINT_8 aucDSnr[4]
)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : ucPfmuIdx = 0x%08x\n", profileIdx);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : u2SubCarrIdx = 0x%08x\n", u2SubCarrIdx);

	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : au2Phi[0] = 0x%08x\n", au2Phi[0]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : au2Phi[1] = 0x%08x\n", au2Phi[1]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : au2Phi[2] = 0x%08x\n", au2Phi[2]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : au2Phi[3] = 0x%08x\n", au2Phi[3]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : au2Phi[4] = 0x%08x\n", au2Phi[4]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : au2Phi[5] = 0x%08x\n", au2Phi[5]);

	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucPsi[0] = 0x%08x\n", aucPsi[0]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucPsi[1] = 0x%08x\n", aucPsi[1]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucPsi[2] = 0x%08x\n", aucPsi[2]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucPsi[3] = 0x%08x\n", aucPsi[3]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucPsi[4] = 0x%08x\n", aucPsi[4]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucPsi[5] = 0x%08x\n", aucPsi[5]);

	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucDSnr[0] = 0x%x\n", aucDSnr[0]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucDSnr[1] = 0x%x\n", aucDSnr[1]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucDSnr[2] = 0x%x\n", aucDSnr[2]);
	DBGLOG(RFTEST, ERROR, "TxBfProfileDataWrite : aucDSnr[3] = 0x%x\n", aucDSnr[3]);

	rTxBfActionInfo.rProfileDataWrite.ucTxBfCategory = BF_PROFILE_WRITE;
	rTxBfActionInfo.rProfileDataWrite.ucPfmuIdx = profileIdx;
	rTxBfActionInfo.rProfileDataWrite.u2SubCarrIdxLsb = u2SubCarrIdx;
	rTxBfActionInfo.rProfileDataWrite.u2SubCarrIdxMsb = u2SubCarrIdx >> 8;
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2Phi11 = au2Phi[0];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2Phi21 = au2Phi[1];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2Phi31 = au2Phi[2];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2Phi22 = au2Phi[3];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2Phi32 = au2Phi[4];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2Phi33 = au2Phi[5];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.ucPsi21 = aucPsi[0];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.ucPsi31 = aucPsi[1];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.ucPsi41 = aucPsi[2];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.ucPsi32 = aucPsi[3];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.ucPsi42 = aucPsi[4];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.ucPsi43 = aucPsi[5];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2dSNR00 = aucDSnr[0];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2dSNR01 = aucDSnr[1];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2dSNR02 = aucDSnr[2];
	rTxBfActionInfo.rProfileDataWrite.rTxBfPfmuData.rField.u2dSNR03 = aucDSnr[3];

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction,
			    &rTxBfActionInfo, sizeof(rTxBfActionInfo), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfProfilePnRead(struct net_device *prNetDev, UINT_8 profileIdx)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "TxBfProfilePnRead : ucPfmuIdx = 0x%08x\n", profileIdx);

	rTxBfActionInfo.rProfilePnRead.ucTxBfCategory = BF_PN_READ;
	rTxBfActionInfo.rProfilePnRead.ucPfmuIdx = profileIdx;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction,
			    &rTxBfActionInfo, sizeof(rTxBfActionInfo), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfProfilePnWrite(struct net_device *prNetDev, UINT_8 profileIdx, UINT_16 u2bw, UINT_16 au2XSTS[12])
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : ucPfmuIdx = 0x%08x\n", profileIdx);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : u2bw = 0x%08x\n", u2bw);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[0] = 0x%08x\n", au2XSTS[0]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[1] = 0x%08x\n", au2XSTS[1]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[2] = 0x%08x\n", au2XSTS[2]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[3] = 0x%08x\n", au2XSTS[3]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[4] = 0x%08x\n", au2XSTS[4]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[5] = 0x%08x\n", au2XSTS[5]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[6] = 0x%08x\n", au2XSTS[6]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[7] = 0x%08x\n", au2XSTS[7]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[8] = 0x%08x\n", au2XSTS[8]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[9] = 0x%08x\n", au2XSTS[9]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[10] = 0x%08x\n", au2XSTS[10]);
	DBGLOG(RFTEST, ERROR, "TxBfProfilePnWrite : au2XSTS[11] = 0x%08x\n", au2XSTS[11]);

	rTxBfActionInfo.rProfilePnWrite.ucTxBfCategory = BF_PN_WRITE;
	rTxBfActionInfo.rProfilePnWrite.ucPfmuIdx = profileIdx;
	rTxBfActionInfo.rProfilePnWrite.u2bw = u2bw;
	memcpy(&rTxBfActionInfo.rProfilePnWrite.ucBuf[0], au2XSTS, 24);

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction,
			    &rTxBfActionInfo, sizeof(rTxBfActionInfo), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfSounding(struct net_device *prNetDev, UINT_8 ucSuMu,	/* 0/1/2/3 */
		    UINT_8 ucNumSta,	/* 00~04 */
		    UINT_8 ucSndInterval,	/* 00~FF */
		    UINT_8 ucWLan0,	/* 00~7F */
		    UINT_8 ucWLan1,	/* 00~7F */
		    UINT_8 ucWLan2,	/* 00~7F */

		    UINT_8 ucWLan3	/* 00~7F */
)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "TxBfSounding : ucSuMu = 0x%08x\n", ucSuMu);
	DBGLOG(RFTEST, ERROR, "TxBfSounding : ucNumSta = 0x%08x\n", ucNumSta);
	DBGLOG(RFTEST, ERROR, "TxBfSounding : ucSndInterval = 0x%08x\n", ucSndInterval);
	DBGLOG(RFTEST, ERROR, "TxBfSounding : ucWLan0 = 0x%08x\n", ucWLan0);
	DBGLOG(RFTEST, ERROR, "TxBfSounding : ucWLan1 = 0x%08x\n", ucWLan1);
	DBGLOG(RFTEST, ERROR, "TxBfSounding : ucWLan2 = 0x%08x\n", ucWLan2);
	DBGLOG(RFTEST, ERROR, "TxBfSounding : ucWLan3 = 0x%08x\n", ucWLan3);

	switch (ucSuMu) {
	case MU_SOUNDING:

	case MU_PERIODIC_SOUNDING:
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfMuSndPeriodicTriggerCtrl.ucCmdCategoryID =
		    BF_SOUNDING_ON;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfMuSndPeriodicTriggerCtrl.ucSuMuSndMode =
		    ucSuMu;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfMuSndPeriodicTriggerCtrl.ucStaNum =
		    ucNumSta;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.
		    rExtCmdExtBfMuSndPeriodicTriggerCtrl.u4SoundingInterval = ucSndInterval;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfMuSndPeriodicTriggerCtrl.ucWlanId[0] =
		    ucWLan0;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfMuSndPeriodicTriggerCtrl.ucWlanId[1] =
		    ucWLan1;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfMuSndPeriodicTriggerCtrl.ucWlanId[2] =
		    ucWLan2;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfMuSndPeriodicTriggerCtrl.ucWlanId[3] =
		    ucWLan3;
		break;

	case SU_SOUNDING:
	case SU_PERIODIC_SOUNDING:
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfSndPeriodicTriggerCtrl.ucCmdCategoryID =
		    BF_SOUNDING_ON;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfSndPeriodicTriggerCtrl.ucSuMuSndMode =
		    ucSuMu;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfSndPeriodicTriggerCtrl.u4SoundingInterval =
		    ucSndInterval;
		rTxBfActionInfo.rTxBfSoundingStart.rTxBfSounding.rExtCmdExtBfSndPeriodicTriggerCtrl.ucWlanIdx = ucWLan0;
		break;
	default:
		break;
	}

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction,
			    &rTxBfActionInfo, sizeof(rTxBfActionInfo), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfSoundingStop(struct net_device *prNetDev)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "TxBfSoundingStop\n");

	rTxBfActionInfo.rTxBfSoundingStop.ucTxBfCategory = BF_SOUNDING_OFF;
	rTxBfActionInfo.rTxBfSoundingStop.ucSndgStop = 1;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction,
			    &rTxBfActionInfo, sizeof(rTxBfActionInfo), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfTxApply(struct net_device *prNetDev, UINT_8 ucWlanId, UINT_8 fgETxBf, UINT_8 fgITxBf, UINT_8 fgMuTxBf)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR,
	       "TxBfTxApply : ucWlanId = 0x%08x, fgETxBf = 0x%08x,fgITxBf = 0x%08x,fgMuTxBf = 0x%08x\n",
	       ucWlanId, fgETxBf, fgITxBf, fgMuTxBf);

	rTxBfActionInfo.rTxBfTxApply.ucTxBfCategory = BF_DATA_PACKET_APPLY;
	rTxBfActionInfo.rTxBfTxApply.ucWlanId = ucWlanId;
	rTxBfActionInfo.rTxBfTxApply.fgETxBf = fgETxBf;
	rTxBfActionInfo.rTxBfTxApply.fgITxBf = fgITxBf;
	rTxBfActionInfo.rTxBfTxApply.fgMuTxBf = fgMuTxBf;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction,
			    &rTxBfActionInfo, sizeof(rTxBfActionInfo), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfPfmuMemAlloc(struct net_device *prNetDev, UINT_8 ucSuMuMode, UINT_8 ucWlanIdx)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR,
		"TxBfPfmuMemAlloc : ucSuMuMode = 0x%08x, ucWlanIdx = 0x%08x\n", ucSuMuMode, ucWlanIdx);

	rTxBfActionInfo.rTxBfPfmuMemAlloc.ucTxBfCategory = BF_PFMU_MEM_ALLOCATE;
	rTxBfActionInfo.rTxBfPfmuMemAlloc.ucSuMuMode = ucSuMuMode;
	rTxBfActionInfo.rTxBfPfmuMemAlloc.ucWlanIdx = ucWlanIdx;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction,
			    &rTxBfActionInfo, sizeof(rTxBfActionInfo), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfPfmuMemRelease(struct net_device *prNetDev, UINT_8 ucWlanId)
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "TxBfPfmuMemRelease : ucWlanId = 0x%08x\n", ucWlanId);

	rTxBfActionInfo.rTxBfPfmuMemRls.ucTxBfCategory = BF_PFMU_MEM_RELEASE;
	rTxBfActionInfo.rTxBfPfmuMemRls.ucWlanId = ucWlanId;

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidTxBfAction,
			    &rTxBfActionInfo, sizeof(rTxBfActionInfo), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

INT_32 TxBfBssInfoUpdate(struct net_device *prNetDev, UINT_8 ucOwnMacIdx, UINT_8 ucBssIdx, UINT_8 ucBssId[MAC_ADDR_LEN])
{
	INT_32 i4Status = 0;
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	/* UINT_32 u4BufLen = 0; */
	PARAM_CUSTOM_TXBF_ACTION_STRUCT_T rTxBfActionInfo;
	P_BSS_INFO_T prBssInfo;

	kalMemZero(&rTxBfActionInfo, sizeof(rTxBfActionInfo));

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	DBGLOG(RFTEST, ERROR, "TxBfBssInfoUpdate : ucOwnMacIdx = 0x%08x\n", ucOwnMacIdx);
	DBGLOG(RFTEST, ERROR, "TxBfBssInfoUpdate : ucBssIdx = 0x%08x\n", ucBssIdx);
	DBGLOG(RFTEST, ERROR, "TxBfBssInfoUpdate : ucBssId[0] = 0x%08x\n", ucBssId[0]);
	DBGLOG(RFTEST, ERROR, "TxBfBssInfoUpdate : ucBssId[1] = 0x%08x\n", ucBssId[1]);
	DBGLOG(RFTEST, ERROR, "TxBfBssInfoUpdate : ucBssId[2] = 0x%08x\n", ucBssId[2]);
	DBGLOG(RFTEST, ERROR, "TxBfBssInfoUpdate : ucBssId[3] = 0x%08x\n", ucBssId[3]);
	DBGLOG(RFTEST, ERROR, "TxBfBssInfoUpdate : ucBssId[4] = 0x%08x\n", ucBssId[4]);
	DBGLOG(RFTEST, ERROR, "TxBfBssInfoUpdate : ucBssId[5] = 0x%08x\n", ucBssId[5]);

	prBssInfo = prAdapter->aprBssInfo[ucBssIdx];

	if (!prBssInfo)
		return WLAN_STATUS_FAILURE;
	prBssInfo->ucOwnMacIndex = ucOwnMacIdx;
	memcpy(&prBssInfo->aucBSSID, &ucBssId, MAC_ADDR_LEN);

	nicUpdateBss(prAdapter, prBssInfo->ucBssIndex);

	return i4Status;
}

/* iwpriv ra0 set assoc=[mac:hh:hh:hh:hh:hh:hh]-[wtbl:dd]-
 * [ownmac:dd]-[type:xx]-[mode:mmm]-[bw:dd]-[nss:ss]-[maxrate:kkk_dd]
 */
INT_32 TxBfManualAssoc(struct net_device *prNetDev, UINT_8 aucMac[MAC_ADDR_LEN], UINT_8 ucType,	/* no use */
		       UINT_8 ucWtbl,
		       UINT_8 ucOwnmac,
		       UINT_8 ucMode,
		       UINT_8 ucBw,
		       UINT_8 ucNss, UINT_8 ucPfmuId, UINT_8 ucMarate, UINT_8 ucSpeIdx, UINT_8 ucRca2, UINT_8 ucRv)
{
	CMD_MANUAL_ASSOC_STRUCT_T rManualAssoc;
	/* P_STA_RECORD_T prStaRec; */
	P_GLUE_INFO_T prGlueInfo = NULL;
	P_ADAPTER_T prAdapter = NULL;
	UINT_32 u4BufLen = 0;
	INT_32 i4Status = 0;
	/* UINT_8 ucNsts; */
	/* UINT_32 i; */

	ASSERT(prNetDev);
	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv(prNetDev));
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(&rManualAssoc, sizeof(CMD_MANUAL_ASSOC_STRUCT_T));
	/* Tag assignment */
	rManualAssoc.u2Tag = STA_REC_MAUNAL_ASSOC;
	rManualAssoc.u2Length = sizeof(CMD_MANUAL_ASSOC_STRUCT_T);
	/* content */
	kalMemCopy(rManualAssoc.aucMac, aucMac, MAC_ADDR_LEN);
	rManualAssoc.ucType = ucType;
	rManualAssoc.ucWtbl = ucWtbl;
	rManualAssoc.ucOwnmac = ucOwnmac;
	rManualAssoc.ucMode = ucMode;
	rManualAssoc.ucBw = ucBw;
	rManualAssoc.ucNss = ucNss;
	rManualAssoc.ucPfmuId = ucPfmuId;
	rManualAssoc.ucMarate = ucMarate;
	rManualAssoc.ucSpeIdx = ucSpeIdx;
	rManualAssoc.ucaid = ucRca2;

#if 0
	switch (ucMode) {
	case 0:		/* abggnanac */
		prStaRec->ucDesiredPhyTypeSet = aucPhyCfg2PhyTypeSet[PHY_TYPE_SET_802_11ABGNAC];
		break;
	case 1:		/* bggnan */
		prStaRec->ucDesiredPhyTypeSet = aucPhyCfg2PhyTypeSet[PHY_TYPE_SET_802_11ABGN];
		break;
	case 2:		/* aanac */
		prStaRec->ucDesiredPhyTypeSet = aucPhyCfg2PhyTypeSet[PHY_TYPE_SET_802_11ANAC];
		break;
	default:
		prStaRec->ucDesiredPhyTypeSet = aucPhyCfg2PhyTypeSet[PHY_TYPE_SET_802_11ABGNAC];
		break;
	}

	prStaRec->rTxBfPfmuStaInfo.u2PfmuId = ucPfmuId;

	memcpy(prStaRec->aucMacAddr, aucMac, MAC_ADDR_LEN);

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidStaRecUpdate,
			    &rStaRecUpdateInfo,
			    sizeof(PARAM_CUSTOM_STA_REC_UPD_STRUCT_T), FALSE, FALSE, TRUE, &u4BufLen);
#endif

	i4Status = kalIoctl(prGlueInfo,
			    wlanoidManualAssoc,
			    &rManualAssoc, sizeof(CMD_MANUAL_ASSOC_STRUCT_T), FALSE, FALSE, TRUE, &u4BufLen);

	return i4Status;
}

#endif
#endif /*CFG_SUPPORT_QA_TOOL */
