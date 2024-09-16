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
** Id: //Department/DaVinci/BRANCHES/MT6620_WIFI_DRIVER_V2_3/os/linux/gl_proc.c#2
*/

/*
 * ! \file   "gl_proc.c"
 *  \brief  This file defines the interface which can interact with users in /proc fs.
 *
 *   Detail description.
 */

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/
#include "gl_os.h"
#include "gl_kal.h"
#include "debug.h"
#include "wlan_lib.h"
#include "debug.h"

#ifdef FW_CFG_SUPPORT
#include "fwcfg.h"
#endif
/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/
#define PROC_MCR_ACCESS                         "mcr"
#define PROC_ROOT_NAME							"wlan"
#ifdef FW_CFG_SUPPORT
#define PROC_CFG_NAME							"cfg"
#endif

#if CFG_SUPPORT_DEBUG_FS
#define PROC_ROAM_PARAM							"roam_param"
#define PROC_COUNTRY							"country"
#endif
#define PROC_DRV_STATUS                         "status"
#define PROC_RX_STATISTICS                      "rx_statistics"
#define PROC_TX_STATISTICS                      "tx_statistics"
#define PROC_DBG_LEVEL_NAME                     "dbgLevel"
#define PROC_PKT_DELAY_DBG			"pktDelay"
#define PROC_SET_CAM				"setCAM"
#define PROC_AUTO_PERF_CFG			"autoPerfCfg"
#define PROC_SET_WIFI_CFG			"wificfg"

#define PROC_MCR_ACCESS_MAX_USER_INPUT_LEN      20
#define PROC_RX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_TX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_DBG_LEVEL_MAX_USER_INPUT_LEN       20
#define PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN      30
#define PROC_UID_SHELL							2000
#define PROC_GID_WIFI							1010

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
static P_GLUE_INFO_T g_prGlueInfo_proc;
static UINT_32 u4McrOffset;
static struct proc_dir_entry *gprProcNetRoot;
static struct proc_dir_entry *gprProcRoot;
static UINT_8 aucDbModuleName[][PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN] = {
	"INIT", "HAL", "INTR", "REQ", "TX", "RX", "RFTEST", "EMU", "SW1", "SW2",
	"SW3", "SW4", "HEM", "AIS", "RLM", "MEM", "CNM", "RSN", "BSS", "SCN",
	"SAA", "AAA", "P2P", "QM", "SEC", "BOW", "WAPI", "ROAMING", "TDLS", "OID",
	"NIC", "WNM", "WMM"
};
static UINT_8 aucProcBuf[1536];

#if FW_CFG_SUPPORT
static P_GLUE_INFO_T gprGlueInfo;
#endif

#define DRV_STATUS_BUF_LEN 2048
static wait_queue_head_t waitqDrvStatus;
static struct mutex drvStatusLock;
static UINT_8 aucDrvStatus[DRV_STATUS_BUF_LEN];
static INT_64 i8WrStatusPos;
static BOOLEAN fgDrvStatus;
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static ssize_t procDbgLevelRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_8 *temp = &aucProcBuf[0];
	UINT_32 u4CopySize = sizeof(aucProcBuf);
	UINT_16 i;
	UINT_16 u2ModuleNum = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	kalStrnCpy(temp, "\nTEMP|LOUD|INFO|TRACE | EVENT|STATE|WARN|ERROR\n"
			 "bit7|bit6|bit5|bit4 | bit3|bit2|bit1|bit0\n\n"
			 "Usage: Module Index:Module Level, such as 0x00:0xff\n\n"
			 "Debug Module\tIndex\tLevel\tDebug Module\tIndex\tLevel\n\n",
			 sizeof(aucProcBuf));
	temp += kalStrLen(temp);
	u4CopySize -= kalStrLen(temp);

	u2ModuleNum = (sizeof(aucDbModuleName) / PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0xfe;
	for (i = 0; i < u2ModuleNum; i += 2)
		SNPRINTF(temp, u4CopySize, ("DBG_%s_IDX\t(0x%02x):\t0x%02x\tDBG_%s_IDX\t(0x%02x):\t0x%02x\n",
				&aucDbModuleName[i][0], i, aucDebugModule[i],
				&aucDbModuleName[i+1][0], i+1, aucDebugModule[i+1]));

	if ((sizeof(aucDbModuleName) / PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0x1)
		SNPRINTF(temp, u4CopySize, ("DBG_%s_IDX\t(0x%02x):\t0x%02x\n",
				&aucDbModuleName[u2ModuleNum][0], u2ModuleNum, aucDebugModule[u2ModuleNum]));

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		DBGLOG(HAL, ERROR, "copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static ssize_t procDbgLevelWrite(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	UINT_32 u4NewDbgModule, u4NewDbgLevel;
	UINT_8 *temp = &aucProcBuf[0];
	UINT_32 u4CopySize = sizeof(aucProcBuf);

	kalMemSet(aucProcBuf, 0, u4CopySize);
	if (u4CopySize > count)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	aucProcBuf[u4CopySize] = '\0';


	while (temp) {
		if (sscanf(temp, "0x%x:0x%x", &u4NewDbgModule, &u4NewDbgLevel) != 2)  {
			pr_info("debug module and debug level should be one byte in length\n");
			break;
		}
		if (u4NewDbgModule == 0xFF) {
			UINT_8 i = 0;

			for (; i < DBG_MODULE_NUM; i++)
				aucDebugModule[i] = u4NewDbgLevel & DBG_CLASS_MASK;

			break;
		} else if (u4NewDbgModule >= DBG_MODULE_NUM) {
			pr_info("debug module index should less than %d\n", DBG_MODULE_NUM);
			break;
		}
		aucDebugModule[u4NewDbgModule] =  u4NewDbgLevel & DBG_CLASS_MASK;
		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++; /* skip ',' */
	}
	wlanDbgLevelSync();
	return count;
}


static const struct file_operations dbglevel_ops = {
	.owner = THIS_MODULE,
	.read = procDbgLevelRead,
	.write = procDbgLevelWrite,
};

static ssize_t procPktDelayDbgCfgRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_8 *temp = &aucProcBuf[0];
	UINT_32 u4CopySize = sizeof(aucProcBuf);
	UINT_8 ucTxRxFlag;
	UINT_8 ucTxIpProto;
	UINT_16 u2TxUdpPort;
	UINT_32 u4TxDelayThreshold;
	UINT_8 ucRxIpProto;
	UINT_16 u2RxUdpPort;
	UINT_32 u4RxDelayThreshold;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	kalStrnCpy(temp,
		"\nUsage: txLog/rxLog/reset 1(ICMP)/6(TCP)/11(UDP) Dst/SrcPortNum DelayThreshold(us)\n"
		"Print tx delay log,                                   such as: echo txLog 0 0 0 > pktDelay\n"
		"Print tx UDP delay log,                               such as: echo txLog 11 0 0 > pktDelay\n"
		"Print tx UDP dst port19305 delay log,                 such as: echo txLog 11 19305 0 > pktDelay\n"
		"Print rx UDP src port19305 delay more than 500us log, such as: echo rxLog 11 19305 500 > pktDelay\n"
		"Print tx TCP delay more than 500us log,               such as: echo txLog 6 0 500 > pktDelay\n"
		"Close log,                                            such as: echo reset 0 0 0 > pktDelay\n\n",
		sizeof(aucProcBuf));
	temp += kalStrLen(temp);
	u4CopySize -= kalStrLen(temp);

	StatsEnvGetPktDelay(&ucTxRxFlag, &ucTxIpProto, &u2TxUdpPort, &u4TxDelayThreshold,
		&ucRxIpProto, &u2RxUdpPort, &u4RxDelayThreshold);

	if (ucTxRxFlag & BIT(0))
		SNPRINTF(temp, u4CopySize, ("txLog %x %d %d\n", ucTxIpProto, u2TxUdpPort, u4TxDelayThreshold));

	if (ucTxRxFlag & BIT(1))
		SNPRINTF(temp, u4CopySize, ("rxLog %x %d %d\n", ucRxIpProto, u2RxUdpPort, u4RxDelayThreshold));

	if (!ucTxRxFlag)
		SNPRINTF(temp, u4CopySize, ("reset 0 0 0, there is no tx/rx delay log\n"));

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static ssize_t procPktDelayDbgCfgWrite(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
#define MODULE_NAME_LENGTH 7
#define MODULE_RESET 0
#define MODULE_TX 1
#define MODULE_RX 2

	UINT_32 u4CopySize = sizeof(aucProcBuf);
	UINT_8 *temp = &aucProcBuf[0];
	UINT_8 aucModule[MODULE_NAME_LENGTH];
	UINT_32 u4DelayThreshold = 0;
	UINT_16 u2PortNum = 0;
	UINT_32 u4IpProto = 0;
	UINT_8 aucResetArray[MODULE_NAME_LENGTH] = "reset";
	UINT_8 aucTxArray[MODULE_NAME_LENGTH] = "txLog";
	UINT_8 aucRxArray[MODULE_NAME_LENGTH] = "rxLog";
	UINT_8 ucTxOrRx = 0;

	kalMemSet(aucProcBuf, 0, u4CopySize);
	if (u4CopySize > count)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	aucProcBuf[u4CopySize] = '\0';

	while (temp) {
		/* pick up a string and teminated after meet : */
		if (sscanf(temp, "%6s %x %hu %u", aucModule, &u4IpProto, &u2PortNum, &u4DelayThreshold) != 4)  {
			pr_info("read param fail, aucModule=%s\n", aucModule);
			break;
		}

		if (kalStrnCmp(aucModule, aucResetArray, MODULE_NAME_LENGTH) == 0) {
			ucTxOrRx = MODULE_RESET;
		} else if (kalStrnCmp(aucModule, aucTxArray, MODULE_NAME_LENGTH) == 0) {
			ucTxOrRx = MODULE_TX;
		} else if (kalStrnCmp(aucModule, aucRxArray, MODULE_NAME_LENGTH) == 0) {
			ucTxOrRx = MODULE_RX;
		} else {
			pr_info("input module error!\n");
			break;
		}

		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++; /* skip ',' */
	}

	StatsEnvSetPktDelay(ucTxOrRx, (UINT_8)u4IpProto, u2PortNum, u4DelayThreshold);

	return count;
}

static const struct file_operations proc_pkt_delay_dbg_ops = {
	.owner = THIS_MODULE,
	.read  = procPktDelayDbgCfgRead,
	.write = procPktDelayDbgCfgWrite,
};

static ssize_t procSetCamCfgWrite(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
#define MODULE_NAME_LEN_1 5

	UINT_32 u4CopySize = sizeof(aucProcBuf);
	UINT_8 *temp = &aucProcBuf[0];
	BOOLEAN fgSetCamCfg = FALSE;
	UINT_8 aucModule[MODULE_NAME_LEN_1];
	UINT_32 u4Enabled;
	UINT_8 aucModuleArray[MODULE_NAME_LEN_1] = "CAM";
	BOOLEAN fgParamValue = TRUE;

	kalMemSet(aucProcBuf, 0, u4CopySize);
	if (u4CopySize > count)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	aucProcBuf[u4CopySize] = '\0';
	temp = &aucProcBuf[0];
	while (temp) {
		/* pick up a string and teminated after meet : */
		if (sscanf(temp, "%4s %d", aucModule, &u4Enabled) != 2)  {
			pr_info("read param fail, aucModule=%s\n", aucModule);
			fgParamValue = FALSE;
			break;
		}

		if (kalStrnCmp(aucModule, aucModuleArray, MODULE_NAME_LEN_1) == 0) {
			if (u4Enabled)
				fgSetCamCfg = TRUE;
			else
				fgSetCamCfg = FALSE;
		}
		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++; /* skip ',' */
	}

	if (fgParamValue)
		nicConfigProcSetCamCfgWrite(fgSetCamCfg);

	return count;
}

static const struct file_operations proc_set_cam_ops = {
	.owner = THIS_MODULE,
	.write = procSetCamCfgWrite,
};

/*******************************************************************************
*                              F U N C T I O N S
********************************************************************************
*/
/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading MCR register to User Space, the offset of
*        the MCR is specified in u4McrOffset.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static ssize_t procMCRRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	P_GLUE_INFO_T prGlueInfo;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
	UINT_32 u4BufLen;
	UINT_32 u4Count = sizeof(aucProcBuf);
	UINT_8 *temp = &aucProcBuf[0];
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	/* Kevin: Apply PROC read method 1. */
	if (*f_pos > 0)
		return 0;	/* To indicate end of file. */

	prGlueInfo = g_prGlueInfo_proc;

	rMcrInfo.u4McrOffset = u4McrOffset;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryMcrRead, (PVOID)&rMcrInfo, sizeof(rMcrInfo), TRUE, TRUE, TRUE, &u4BufLen);
	kalMemZero(aucProcBuf, sizeof(aucProcBuf));
	SNPRINTF(temp, u4Count, ("MCR (0x%08xh): 0x%08x\n", rMcrInfo.u4McrOffset, rMcrInfo.u4McrData));

	u4Count = kalStrLen(aucProcBuf);
	if (copy_to_user(buf, aucProcBuf, u4Count)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4Count;

	return (int)u4Count;

}				/* end of procMCRRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for writing MCR register to HW or update u4McrOffset
*        for reading MCR later.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
static ssize_t procMCRWrite(struct file *file, const char __user *buffer,
										size_t count, loff_t *data)
{
	P_GLUE_INFO_T prGlueInfo;
	char acBuf[PROC_MCR_ACCESS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	UINT_32 i4CopySize;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
	UINT_32 u4BufLen;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;
	int num = 0;

	ASSERT(data);

	i4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	if (copy_from_user(acBuf, buffer, i4CopySize))
		return 0;
	acBuf[i4CopySize] = '\0';

	num = sscanf(acBuf, "0x%x 0x%x", &rMcrInfo.u4McrOffset, &rMcrInfo.u4McrData);
	switch (num) {
	case 2:
		/* NOTE: Sometimes we want to test if bus will still be ok, after accessing
		 * the MCR which is not align to DW boundary.
		 */
		/* if (IS_ALIGN_4(rMcrInfo.u4McrOffset)) */
		{
			prGlueInfo = (P_GLUE_INFO_T) netdev_priv((struct net_device *)data);

			u4McrOffset = rMcrInfo.u4McrOffset;

			/* printk("Write 0x%lx to MCR 0x%04lx\n", */
			/* rMcrInfo.u4McrOffset, rMcrInfo.u4McrData); */

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetMcrWrite,
					   (PVOID)&rMcrInfo, sizeof(rMcrInfo), FALSE, FALSE, TRUE, &u4BufLen);

		}
		break;

	case 1:
		/* if (IS_ALIGN_4(rMcrInfo.u4McrOffset)) */
		{
			u4McrOffset = rMcrInfo.u4McrOffset;
		}
		break;

	default:
		break;
	}

	return count;

}				/* end of procMCRWrite() */

static const struct file_operations mcr_ops = {
	.owner = THIS_MODULE,
	.read = procMCRRead,
	.write = procMCRWrite,
};

#if CFG_SUPPORT_DEBUG_FS
static ssize_t procRoamRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen;

	/* if *f_pos > 0, it means has read successed last time, don't try again */
	if (*f_pos > 0)
		return 0;

	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidGetRoamParams, aucProcBuf, sizeof(aucProcBuf),
							TRUE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to read roam params\n");
		return -EINVAL;
	}

	u4CopySize = kalStrLen(aucProcBuf);
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}

static ssize_t procRoamWrite(struct file *file, const char __user *buffer,
										size_t count, loff_t *data)
{
	WLAN_STATUS rStatus;
	UINT_32 u4BufLen = 0;
	UINT_32 u4CopySize = sizeof(aucProcBuf);

	kalMemSet(aucProcBuf, 0, u4CopySize);
	if (u4CopySize >= count+1)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	aucProcBuf[u4CopySize] = '\0';

	if (kalStrnCmp(aucProcBuf, "force_roam", 10) == 0)
		rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetForceRoam, NULL, 0,
						FALSE, FALSE, TRUE, &u4BufLen);
	else
		rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetRoamParams, aucProcBuf,
					kalStrLen(aucProcBuf), FALSE, FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to set roam params: %s\n", aucProcBuf);
		return -EINVAL;
	}
	return count;
}

static const struct file_operations roam_ops = {
	.owner = THIS_MODULE,
	.read = procRoamRead,
	.write = procRoamWrite,
};

static ssize_t procCountryRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	UINT_16 u2CountryCode = 0;
	UINT_32 u4BufLen;
	WLAN_STATUS rStatus;

	/* if *f_pos > 0, it means has read successed last time, don't try again */
	if (*f_pos > 0)
		return 0;

	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidGetCountryCode, &u2CountryCode, 2, TRUE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to get country code\n");
		return -EINVAL;
	}
	if (u2CountryCode)
		kalSprintf(aucProcBuf, "Current Country Code: %c%c\n", (u2CountryCode>>8) & 0xff, u2CountryCode & 0xff);
	else
		kalStrCpy(aucProcBuf, "Current Country Code: NULL\n");

	u4CopySize = kalStrLen(aucProcBuf);
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}

static ssize_t procCountryWrite(struct file *file, const char __user *buffer,
										size_t count, loff_t *data)
{
	UINT_32 u4BufLen = 0;
	WLAN_STATUS rStatus;
	UINT_32 u4CopySize = sizeof(aucProcBuf);

	kalMemSet(aucProcBuf, 0, u4CopySize);
	if (u4CopySize >= count+1)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}

	aucProcBuf[u4CopySize] = '\0';
	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetCountryCode, &aucProcBuf[0], 2, FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed set country code: %s\n", aucProcBuf);
		return -EINVAL;
	}
	return count;
}

static const struct file_operations country_ops = {
	.owner = THIS_MODULE,
	.read = procCountryRead,
	.write = procCountryWrite,
};
#endif

#if CFG_SUPPORT_CFG_FILE
static ssize_t procWifiCfgRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	P_WLAN_CFG_ENTRY_T prCfgEntry = NULL;
	PUINT_8 pucBuf = &aucProcBuf[0];
	UINT_32 i;
	P_ADAPTER_T prAdapter = NULL;
	INT_32 i4CopySize = 0;
	UINT_32 u4CopySize = 0;

	if (*f_pos >= WLAN_CFG_ENTRY_NUM_MAX || !g_prGlueInfo_proc)
		return 0;

	prAdapter = g_prGlueInfo_proc->prAdapter;
	kalMemSet(aucProcBuf, 0, sizeof(aucProcBuf));
	prCfgEntry = &prAdapter->prWlanCfg->arWlanCfgBuf[0];

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++, prCfgEntry++) {
		if (prCfgEntry->aucKey[0] == '\0') {
			if (!i) {
				u4CopySize = 19;
				kalMemCopy(pucBuf, "No configure items\n", u4CopySize);
			}
			DBGLOG(INIT, INFO, "i=%u\n", i);
			i = WLAN_CFG_ENTRY_NUM_MAX;
			break;
		}
		i4CopySize = kalSnprintf(pucBuf, sizeof(aucProcBuf) - u4CopySize, "%s %s\n",
			prCfgEntry->aucKey, prCfgEntry->aucValue);
		if (i4CopySize < 0) {
			i = WLAN_CFG_ENTRY_NUM_MAX;
			DBGLOG(INIT, WARN, "snprintf fail, key %s\n", prCfgEntry->aucKey);
			break;
		}
		pucBuf += i4CopySize;
		u4CopySize += i4CopySize;
		if (u4CopySize + WLAN_CFG_KEY_LEN_MAX + WLAN_CFG_KEY_LEN_MAX + 1 > count)
			break;
		if (u4CopySize  >= sizeof(aucProcBuf)) {
			i = WLAN_CFG_ENTRY_NUM_MAX;
			DBGLOG(INIT, WARN, "too many configure items, buffer full\n");
			break;
		}
	}
	*f_pos = i;
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}

	return u4CopySize;
}

static ssize_t procWifiCfgWrite(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	UINT_32 u4CopySize = sizeof(aucProcBuf);
	P_ADAPTER_T prAdapter = NULL;

	if (!g_prGlueInfo_proc)
		return 0;
	prAdapter = g_prGlueInfo_proc->prAdapter;
	kalMemSet(aucProcBuf, 0, u4CopySize);

	u4CopySize = (u4CopySize >= count + 1) ? count : u4CopySize - 1;

	if (!u4CopySize || copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	aucProcBuf[u4CopySize] = '\0';
	wlanCfgParse(prAdapter, aucProcBuf, u4CopySize);
	wlanInitFeatureOption(prAdapter);
	return count;
}

static const struct file_operations wifi_cfg_ops = {
	.owner = THIS_MODULE,
	.read = procWifiCfgRead,
	.write = procWifiCfgWrite,
};
#endif
static ssize_t procAutoPerfCfgRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_8 *temp = &aucProcBuf[0];
	UINT_32 u4CopySize = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	kalStrnCpy(temp, "Auto Performance Configure usage:\nForceEnable:0 or 1\n"
			 "e.g. ForceEnable:1 to always enable performance monitor even\n",
			 sizeof(aucProcBuf));

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, WARN, "copy_to_user error\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static ssize_t procAutoPerfCfgWrite(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	UINT_32 u4CoreNum = 0;
	UINT_32 u4CoreFreq = 0;
	UINT_8 *temp = &aucProcBuf[0];
	UINT_32 u4CopySize = count;
	UINT_8 i = 0;
	UINT_32 u4ForceEnable = 0;
	UINT_8 aucBuf[32];


	if (u4CopySize >= sizeof(aucProcBuf))
		u4CopySize = sizeof(aucProcBuf) - 1;

	kalMemSet(aucProcBuf, 0, u4CopySize);

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, WARN, "copy_from_user error\n");
		return -EFAULT;
	}

	aucProcBuf[u4CopySize] = '\0';

	i = sscanf(temp, "%d:%d", &u4CoreNum, &u4CoreFreq);
	if (i == 2) {
		DBGLOG(INIT, INFO, "u4CoreNum:%d, u4CoreFreq:%d\n", u4CoreNum, u4CoreFreq);
		kalSetCpuNumFreq(u4CoreNum, u4CoreFreq);
		return u4CopySize;
	}

	if (strlen(temp) > sizeof(aucBuf)) {
		DBGLOG(INIT, WARN, "input string(%s) len is too long, over %zu\n", aucProcBuf, sizeof(aucBuf));
		return -EFAULT;
	}

	i = sscanf(temp, "%11s:%u", aucBuf, &u4ForceEnable);

	if ((i == 2) && strstr(aucBuf, "ForceEnable")) {
		kalPerMonSetForceEnableFlag(u4ForceEnable);
		return u4CopySize;
	}

	DBGLOG(INIT, WARN, "parameter format should be ForceEnable:0 or 1\n");

	return -EFAULT;
}

static const struct file_operations auto_perf_ops = {
	.owner = THIS_MODULE,
	.read = procAutoPerfCfgRead,
	.write = procAutoPerfCfgWrite,
};

VOID glWriteStatus(PPUINT_8 ppucWrPos, PUINT_32 pu4RemainLen, PUINT_8 pucFwt, ...)
{
#define TEMP_BUF_LEN 280
	PUINT_8 pucTemp = NULL;
	INT_32 i4BufUsed = 0;
	INT_32 i4TimeUsed = 0;
	va_list ap;
	struct timeval tval;
	struct rtc_time tm;
	static UINT_8 aucBuf[TEMP_BUF_LEN];

	pucTemp = &aucBuf[0];
	do_gettimeofday(&tval);
	tval.tv_sec -= sys_tz.tz_minuteswest * 60;
	rtc_time_to_tm(tval.tv_sec, &tm);
	i4TimeUsed = kalSnprintf(pucTemp, TEMP_BUF_LEN, "%04d-%02d-%02d %02d:%02d:%02d.%03d ",
				 tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday, tm.tm_hour,
				 tm.tm_min, tm.tm_sec, (INT_32)(tval.tv_usec / USEC_PER_MSEC));
	if (i4TimeUsed < 0) {
		DBGLOG(INIT, INFO, "error to sprintf time\n");
		return;
	}
	va_start(ap, pucFwt);
	i4BufUsed = vsnprintf(pucTemp + i4TimeUsed, TEMP_BUF_LEN - i4TimeUsed, pucFwt, ap);
	va_end(ap);
	if (i4BufUsed < 0) {
		DBGLOG(INIT, INFO, "error to sprintf %s\n", pucFwt);
		return;
	}
	i4BufUsed += i4TimeUsed;

	if (i4BufUsed > *pu4RemainLen) {
		kalMemCopy(*ppucWrPos, pucTemp, *pu4RemainLen);
		pucTemp += *pu4RemainLen;
		i4BufUsed -= *pu4RemainLen;
		i8WrStatusPos += (INT_64)*pu4RemainLen;
		*pu4RemainLen = DRV_STATUS_BUF_LEN;
		*ppucWrPos = &aucDrvStatus[0];
	}
	kalMemCopy(*ppucWrPos, pucTemp, i4BufUsed);
	*ppucWrPos += i4BufUsed;
	*pu4RemainLen -= i4BufUsed;
	i8WrStatusPos += (INT_64)i4BufUsed;
}

/* Provide a real-time monitor mechanism to end-user to monitor wlan status */
VOID glNotifyDrvStatus(enum DRV_STATUS_T eDrvStatus, PVOID pvInfo)
{
	UINT_32 u4WrLen = i8WrStatusPos % DRV_STATUS_BUF_LEN;
	UINT_32 u4RemainLen = DRV_STATUS_BUF_LEN - u4WrLen;
	PUINT_8 pucRealWrPos = &aucDrvStatus[u4WrLen];
#define WRITE_STATUS(_fmt, ...)\
	glWriteStatus(&pucRealWrPos, &u4RemainLen, _fmt, ##__VA_ARGS__)

	if (!fgDrvStatus)
		return;

	mutex_lock(&drvStatusLock);
	switch (eDrvStatus) {
	case UNSOL_BTM_REQ:
	case SOL_BTM_REQ:
	{
		P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo = (P_AIS_SPECIFIC_BSS_INFO_T)pvInfo;
		P_LINK_T prApList = &prAisSpecBssInfo->rNeighborApList.rUsingLink;
		struct NEIGHBOR_AP_T *prNeighborAP = NULL;

		if (!prAisSpecBssInfo) {
			DBGLOG(INIT, ERROR, "prAisSpecBssInfo is NULL\n");
			break;
		}
		WRITE_STATUS("Receive %s Btm Req with Mode:%d\n", eDrvStatus == SOL_BTM_REQ ?
			     "solicited" : "unsolicited", prAisSpecBssInfo->rBTMParam.ucRequestMode);
		if (!(prAisSpecBssInfo->rBTMParam.ucRequestMode & BTM_REQ_MODE_CAND_INCLUDED_BIT))
			break;
		WRITE_STATUS("Candidate List(Total %u), Bssid / Preference / Channel:\n", prApList->u4NumElem);
		LINK_FOR_EACH_ENTRY(prNeighborAP, prApList, rLinkEntry, struct NEIGHBOR_AP_T) {
			WRITE_STATUS("%pM / %d / %d\n", prNeighborAP->aucBssid, prNeighborAP->ucPreference,
				     prNeighborAP->ucChannel);
		}
		break;
	}
	case NEIGHBOR_AP_REP:
	{
		P_AIS_SPECIFIC_BSS_INFO_T prAisSpecBssInfo = (P_AIS_SPECIFIC_BSS_INFO_T)pvInfo;
		P_LINK_T prApList = &prAisSpecBssInfo->rNeighborApList.rUsingLink;
		struct NEIGHBOR_AP_T *prNeighborAP = NULL;

		if (!prAisSpecBssInfo) {
			DBGLOG(INIT, ERROR, "prAisSpecBssInfo is NULL\n");
			break;
		}
		WRITE_STATUS("Receive Neighbor AP report\nCandidate List(Total %u), Bssid / Preference / Channel:\n",
			     prApList->u4NumElem);
		LINK_FOR_EACH_ENTRY(prNeighborAP, prApList, rLinkEntry, struct NEIGHBOR_AP_T) {
			WRITE_STATUS("%pM / %d / %d\n", prNeighborAP->aucBssid, prNeighborAP->ucPreference,
				     prNeighborAP->ucChannel);
		}
		break;
	}
	case SND_BTM_RSP:
	{
		struct BSS_TRANSITION_MGT_PARAM_T *prBtm = (struct BSS_TRANSITION_MGT_PARAM_T *)pvInfo;

		if (!prBtm) {
			DBGLOG(INIT, ERROR, "prBtm is NULL\n");
			break;
		}
		if (prBtm->ucStatusCode == BSS_TRANSITION_MGT_STATUS_ACCEPT)
			WRITE_STATUS("Send Btm Response, Roaming Target:%pM\n", prBtm->aucTargetBssid);
		else
			WRITE_STATUS("Send Btm Response, Reject reason:%d\n", prBtm->ucStatusCode);
		break;
	}
	case CONNECT_AP:
		WRITE_STATUS("Connect to %pM\n", (PUINT_8)pvInfo);
		break;
	case JOIN_FAIL:
	{
		P_STA_RECORD_T prStaRec = (P_STA_RECORD_T)pvInfo;

		if (!prStaRec) {
			DBGLOG(INIT, ERROR, "prStaRec is NULL\n");
			break;
		}
		WRITE_STATUS("Connect with %pM was rejected %d\n", prStaRec->aucMacAddr, prStaRec->u2StatusCode);
		break;
	}
	case DISCONNECT_AP:
	{
		P_BSS_INFO_T prBssInfo = (P_BSS_INFO_T)pvInfo;

		if (!prBssInfo)
			WRITE_STATUS("Disconnected reason: unknown\n");
		else
			WRITE_STATUS("Disconnected reason: %d, bssid %pM\n",
				     prBssInfo->u2DeauthReason, prBssInfo->aucBSSID);
		break;
	}
	case BEACON_TIMEOUT:
		WRITE_STATUS("Beacon timeout with %pM\n", (PUINT_8)pvInfo);
		break;
	case RCV_FW_ROAMING:
		WRITE_STATUS("%s\n", "Receive FW roaming event");
		break;
	case ROAMING_SCAN_START:
	{
		P_MSG_SCN_SCAN_REQ_V2 prMsg = (P_MSG_SCN_SCAN_REQ_V2)pvInfo;

		if (!prMsg) {
			DBGLOG(INIT, ERROR, "prMsg is NULL\n");
			break;
		}
		if (prMsg->eScanChannel == SCAN_CHANNEL_SPECIFIED) {
			if (prMsg->u2ChannelDwellTime > 0)
				WRITE_STATUS("Roaming Scan channel num:%d, dwell time %d\n", prMsg->ucChannelListNum,
					     prMsg->u2ChannelDwellTime);
			else
				WRITE_STATUS("Roaming Scan channel num:%d, default dwell time\n",
					     prMsg->ucChannelListNum);
		} else
			WRITE_STATUS("Roaming Full Scan, excluded channel num:%d\n", prMsg->ucChannelListNum);
		break;
	}
	case ROAMING_SCAN_DONE:
		WRITE_STATUS("%s\n", "Roaming Scan done");
		break;
	default:
		break;
	}
	mutex_unlock(&drvStatusLock);
	/* Wake up all readers if at least one is waiting */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 13, 0))
	if (!list_empty(&waitqDrvStatus.task_list))
#else
	if (!list_empty(&waitqDrvStatus.head))
#endif
		wake_up_interruptible(&waitqDrvStatus);
}

/* Read callback function
** *f_pos: read position of current reader, max size: 4G * 4G bytes
** i8WrStatusPos: writing position of writer, max size: 4G * 4G bytes
*/
static ssize_t procReadDrvStatus(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
#define NOT_ENABLE "Driver Status is not enabled"
#define TO_ENABLE "echo enable > /proc/net/wlan/status to enable"
#define TO_DISABLE "echo disable > /proc/net/wlan/status to disable\n"
	PUINT_8 pucRdPos = NULL;
	UINT_32 u4CopySize = 0;
	INT_32 ret = -1;

	while (ret) {
		ret = wait_event_interruptible(waitqDrvStatus, (!fgDrvStatus || i8WrStatusPos != *f_pos));
		if (ret == -ERESTARTSYS) {
			DBGLOG(INIT, INFO, "May be pending signal, return and let user space handle it\n");
			return 1;
		}
	}

	if (!fgDrvStatus) {
		PUINT_8 pucErrMsg = NOT_ENABLE"\n"TO_ENABLE"\n"TO_DISABLE;
		UINT_32 u4Len = kalStrLen(pucErrMsg);

		if (*f_pos == u4Len)
			return 0;
		if (copy_to_user(buf, pucErrMsg, u4Len)) {
			DBGLOG(INIT, WARN, "copy_to_user error\n");
			return -EFAULT;
		}
		*f_pos = u4Len;
		return u4Len;
	}

	if (i8WrStatusPos < *f_pos) {
		DBGLOG(INIT, INFO, "exit WR:%lld, RD:%lld\n", i8WrStatusPos, *f_pos);
		return 0;
	}
	mutex_lock(&drvStatusLock);
	if (*f_pos > 0) {/* Read again */
		if (i8WrStatusPos - *f_pos > DRV_STATUS_BUF_LEN) {
			u4CopySize = (UINT_32)(i8WrStatusPos % DRV_STATUS_BUF_LEN);
			pucRdPos = &aucDrvStatus[u4CopySize];
			u4CopySize = DRV_STATUS_BUF_LEN - u4CopySize;
			DBGLOG(INIT, INFO, "Status Info Lost %lld bytes, WR:%lld, RD:%lld, MaxRd:%u bytes\n",
			       (i8WrStatusPos - *f_pos - DRV_STATUS_BUF_LEN),
			       i8WrStatusPos, *f_pos, u4CopySize);
			*f_pos = i8WrStatusPos - DRV_STATUS_BUF_LEN;
		} else {
			u4CopySize = (UINT_32)(*f_pos % DRV_STATUS_BUF_LEN);
			pucRdPos = &aucDrvStatus[u4CopySize];
			if (i8WrStatusPos - *f_pos > DRV_STATUS_BUF_LEN - u4CopySize)
				u4CopySize = DRV_STATUS_BUF_LEN - u4CopySize;
			else
				u4CopySize = i8WrStatusPos - *f_pos;
			DBGLOG(INIT, INFO, "Continue to read, WR:%lld, RD:%lld, MaxRd:%u bytes\n",
			       i8WrStatusPos, *f_pos, u4CopySize);
		}
	} else {/* The first time t read for current reader */
		if (i8WrStatusPos > DRV_STATUS_BUF_LEN) {
			u4CopySize = (UINT_32)(i8WrStatusPos % DRV_STATUS_BUF_LEN);
			pucRdPos = &aucDrvStatus[u4CopySize];
			u4CopySize = DRV_STATUS_BUF_LEN - u4CopySize;
			*f_pos = i8WrStatusPos - DRV_STATUS_BUF_LEN;
		} else {
			pucRdPos = &aucDrvStatus[0];
			u4CopySize = (UINT_32)i8WrStatusPos;
		}
		DBGLOG(INIT, INFO, "First time to read, WR:%lld, RD:%lld, MaxRd:%u bytes\n",
		       i8WrStatusPos, *f_pos, u4CopySize);
	}
	mutex_unlock(&drvStatusLock);

	if (u4CopySize > count)
		u4CopySize = count;
	DBGLOG(INIT, TRACE, "Read %u bytes\n", u4CopySize);
	if (copy_to_user(buf, pucRdPos, u4CopySize)) {
		DBGLOG(INIT, WARN, "copy_to_user error\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static ssize_t procDrvStatusCfg(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	if (count >= sizeof(aucProcBuf))
		count = sizeof(aucProcBuf) - 1;

	kalMemSet(aucProcBuf, 0, sizeof(aucProcBuf));

	if (copy_from_user(aucProcBuf, buffer, count)) {
		DBGLOG(INIT, WARN, "copy_from_user error\n");
		return -EFAULT;
	}

	aucProcBuf[count] = '\0';

	if (!kalStrnCmp(aucProcBuf, "enable", 6)) {
		fgDrvStatus = TRUE;
		i8WrStatusPos = 0;
		return 6;
	} else if (!kalStrnCmp(aucProcBuf, "disable", 7)) {
		fgDrvStatus = FALSE;
		wake_up_interruptible(&waitqDrvStatus);
		return 7;
	}
	return -EINVAL;
}

static const struct file_operations drv_status_ops = {
	.owner = THIS_MODULE,
	.read = procReadDrvStatus,
	.write = procDrvStatusCfg,
};

INT_32 procInitFs(VOID)
{
	struct proc_dir_entry *prEntry;

	/* Create folder /proc/wlan/ to avoid dump by other processes, like netdiag */
	gprProcRoot = proc_mkdir(PROC_ROOT_NAME, NULL);
	if (!gprProcRoot) {
		pr_err("gprProcRoot == NULL\n");
		return -ENOENT;
	}

	if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
		pr_err("init proc fs fail: proc_net == NULL\n");
		return -ENOENT;
	}

	/* Create folder /proc/net/wlan */
	gprProcNetRoot = proc_mkdir(PROC_ROOT_NAME, init_net.proc_net);
	if (!gprProcNetRoot) {
		pr_err("gprProcNetRoot == NULL\n");
		return -ENOENT;
	}
	proc_set_user(gprProcNetRoot, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	prEntry = proc_create(PROC_DBG_LEVEL_NAME, 0664, gprProcNetRoot, &dbglevel_ops);
	if (!prEntry) {
		pr_err("Unable to create /proc entry dbgLevel\n\r");
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	prEntry = proc_create(PROC_AUTO_PERF_CFG, 0664, gprProcNetRoot, &auto_perf_ops);
	if (!prEntry) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry %s/n", PROC_AUTO_PERF_CFG);
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	init_waitqueue_head(&waitqDrvStatus);
	mutex_init(&drvStatusLock);
	fgDrvStatus = TRUE;
	prEntry = proc_create(PROC_DRV_STATUS, 0664, gprProcRoot, &drv_status_ops);
	if (!prEntry) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry %s/n", PROC_DRV_STATUS);
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));
	return 0;
}				/* end of procInitProcfs() */

INT_32 procUninitProcFs(VOID)
{
	remove_proc_entry(PROC_DBG_LEVEL_NAME, gprProcNetRoot);
	remove_proc_entry(PROC_AUTO_PERF_CFG, gprProcNetRoot);
	remove_proc_entry(PROC_DRV_STATUS, gprProcRoot);
	remove_proc_subtree(PROC_ROOT_NAME, init_net.proc_net);
	return 0;
}

/*----------------------------------------------------------------------------*/
/*!
* \brief This function clean up a PROC fs created by procInitProcfs().
*
* \param[in] prDev      Pointer to the struct net_device.
* \param[in] pucDevName Pointer to the name of net_device.
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/
INT_32 procRemoveProcfs(VOID)
{
	remove_proc_entry(PROC_MCR_ACCESS, gprProcNetRoot);
	remove_proc_entry(PROC_PKT_DELAY_DBG, gprProcNetRoot);
	remove_proc_entry(PROC_SET_CAM, gprProcNetRoot);
#if CFG_SUPPORT_DEBUG_FS
	remove_proc_entry(PROC_ROAM_PARAM, gprProcNetRoot);
	remove_proc_entry(PROC_COUNTRY, gprProcNetRoot);

#endif

#if CFG_SUPPORT_CFG_FILE
	remove_proc_entry(PROC_SET_WIFI_CFG, gprProcNetRoot);
#endif
	return 0;
} /* end of procRemoveProcfs() */

INT_32 procCreateFsEntry(P_GLUE_INFO_T prGlueInfo)
{
	struct proc_dir_entry *prEntry;

	DBGLOG(INIT, LOUD, "[%s]\n", __func__);
	g_prGlueInfo_proc = prGlueInfo;

	prEntry = proc_create(PROC_MCR_ACCESS, 0664, gprProcNetRoot, &mcr_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_PKT_DELAY_DBG, 0664, gprProcNetRoot, &proc_pkt_delay_dbg_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry PktDelayDug\n\r");
		return -ENOENT;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	prEntry = proc_create(PROC_SET_CAM, 0664, gprProcNetRoot, &proc_set_cam_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry SetCAM\n\r");
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));
#if CFG_SUPPORT_DEBUG_FS
	prEntry = proc_create(PROC_ROAM_PARAM, 0664, gprProcNetRoot, &roam_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}
	prEntry = proc_create(PROC_COUNTRY, 0664, gprProcNetRoot, &country_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}
#endif

#if CFG_SUPPORT_CFG_FILE
	prEntry = proc_create(PROC_SET_WIFI_CFG, 0664, gprProcNetRoot, &wifi_cfg_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry\n\r");
		return -1;
	}
#endif
	return 0;
}

#if 0
/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading Driver Status to User Space.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procDrvStatusRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	UINT_32 u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SPRINTF(p, ("GLUE LAYER STATUS:"));
	SPRINTF(p, ("\n=================="));

	SPRINTF(p, ("\n* Number of Pending Frames: %ld\n", prGlueInfo->u4TxPendingFrameNum));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryDrvStatusForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (UINT_32) (p - page);

	*eof = 1;

	return (int)u4Count;

}				/* end of procDrvStatusRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading Driver RX Statistic Counters to User Space.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procRxStatisticsRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	UINT_32 u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SPRINTF(p, ("RX STATISTICS (Write 1 to clear):"));
	SPRINTF(p, ("\n=================================\n"));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryRxStatisticsForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (UINT_32) (p - page);

	*eof = 1;

	return (int)u4Count;

}				/* end of procRxStatisticsRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reset Driver RX Statistic Counters.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procRxStatisticsWrite(struct file *file, const char *buffer, unsigned long count, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char acBuf[PROC_RX_STATISTICS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	UINT_32 u4CopySize;
	UINT_32 u4ClearCounter;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (kstrtouint(acBuf, 0, &u4ClearCounter) == 1) {
		if (u4ClearCounter == 1) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

			wlanoidSetRxStatisticsForLinuxProc(prGlueInfo->prAdapter);

			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);
		}
	}

	return count;

}				/* end of procRxStatisticsWrite() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reading Driver TX Statistic Counters to User Space.
*
* \param[in] page       Buffer provided by kernel.
* \param[in out] start  Start Address to read(3 methods).
* \param[in] off        Offset.
* \param[in] count      Allowable number to read.
* \param[out] eof       End of File indication.
* \param[in] data       Pointer to the private data structure.
*
* \return number of characters print to the buffer from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procTxStatisticsRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	UINT_32 u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SPRINTF(p, ("TX STATISTICS (Write 1 to clear):"));
	SPRINTF(p, ("\n=================================\n"));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryTxStatisticsForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (UINT_32) (p - page);

	*eof = 1;

	return (int)u4Count;

}				/* end of procTxStatisticsRead() */

/*----------------------------------------------------------------------------*/
/*!
* \brief The PROC function for reset Driver TX Statistic Counters.
*
* \param[in] file   pointer to file.
* \param[in] buffer Buffer from user space.
* \param[in] count  Number of characters to write
* \param[in] data   Pointer to the private data structure.
*
* \return number of characters write from User Space.
*/
/*----------------------------------------------------------------------------*/
static int procTxStatisticsWrite(struct file *file, const char *buffer, unsigned long count, void *data)
{
	P_GLUE_INFO_T prGlueInfo = ((struct net_device *)data)->priv;
	char acBuf[PROC_RX_STATISTICS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	UINT_32 u4CopySize;
	UINT_32 u4ClearCounter;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	u4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	if (kstrtouint(acBuf, 0, &u4ClearCounter) == 1) {
		if (u4ClearCounter == 1) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

			wlanoidSetTxStatisticsForLinuxProc(prGlueInfo->prAdapter);

			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);
		}
	}

	return count;

}				/* end of procTxStatisticsWrite() */
#endif

#ifdef FW_CFG_SUPPORT
#define MAX_CFG_OUTPUT_BUF_LENGTH 1024
static UINT_8 aucCfgBuf[CMD_FORMAT_V1_LENGTH];
static UINT_8 aucCfgQueryKey[MAX_CMD_NAME_MAX_LENGTH];
static UINT_8 aucCfgOutputBuf[MAX_CFG_OUTPUT_BUF_LENGTH];

static ssize_t cfgRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	UINT_8 *temp = &aucCfgOutputBuf[0];
	UINT_32 u4CopySize = sizeof(aucCfgOutputBuf);
	UINT_32 u4RetValue = 0;

	struct _CMD_HEADER_T cmdV1Header;
	struct _CMD_FORMAT_V1_T *pr_cmd_v1 = (struct _CMD_FORMAT_V1_T *) cmdV1Header.buffer;

	/* if *f_pos >  0, we should return 0 to make cat command exit */
	if (*f_pos > 0 || gprGlueInfo == NULL)
		return 0;

	kalMemSet(aucCfgOutputBuf, 0, MAX_CFG_OUTPUT_BUF_LENGTH);

	SNPRINTF(temp, u4CopySize, ("\nprocCfgRead() %s:\n", aucCfgQueryKey));

	/* send to FW */
	cmdV1Header.cmdVersion = CMD_VER_1;
	cmdV1Header.cmdType = CMD_TYPE_QUERY;
	cmdV1Header.itemNum = 1;
	cmdV1Header.cmdBufferLen = sizeof(struct _CMD_FORMAT_V1_T);
	kalMemSet(cmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);

	pr_cmd_v1->itemStringLength = kalStrLen(aucCfgQueryKey);

	kalMemCopy(pr_cmd_v1->itemString, aucCfgQueryKey, kalStrLen(aucCfgQueryKey));

	rStatus = kalIoctl(gprGlueInfo,
			wlanoidQueryCfgRead,
			(PVOID)&cmdV1Header,
			sizeof(cmdV1Header),
			TRUE,
			TRUE,
			TRUE,
			&u4RetValue);
	if (rStatus == WLAN_STATUS_FAILURE)
		DBGLOG(INIT, ERROR, "prCmdV1Header kalIoctl wlanoidQueryCfgRead fail 0x%x\n", rStatus);

	SNPRINTF(temp, u4CopySize, ("%s\n", cmdV1Header.buffer));

	u4CopySize = kalStrLen(aucCfgOutputBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, aucCfgOutputBuf, u4CopySize))
		DBGLOG(INIT, ERROR, "copy to user failed\n");

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static ssize_t cfgWrite(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos)
{
	/* echo xxx xxx > /proc/net/wlan/cfg */
	UINT_8 i = 0;
	UINT_32 u4CopySize = sizeof(aucCfgBuf);
	UINT_8 token_num = 1;

	if (!count)
		return -EINVAL;

	kalMemSet(aucCfgBuf, 0, u4CopySize);

	if (u4CopySize > count)
		u4CopySize = count;
	else
		u4CopySize = u4CopySize - 1;

	if (copy_from_user(aucCfgBuf, buf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy from user failed\n");
		return -EFAULT;
	}
	aucCfgBuf[u4CopySize] = '\0';

	for (; i < u4CopySize; i++) {
		if (aucCfgBuf[i] == ' ') {
			token_num++;
			break;
		}
	}

	if (token_num == 1) {
		kalMemSet(aucCfgQueryKey, 0, sizeof(aucCfgQueryKey));
		/* remove the 0x0a */
		if (u4CopySize > sizeof(aucCfgQueryKey))
			return -EINVAL;
		memcpy(aucCfgQueryKey, aucCfgBuf, u4CopySize);
		if (aucCfgQueryKey[u4CopySize - 1] == 0x0a)
			aucCfgQueryKey[u4CopySize - 1] = '\0';
	} else
		wlanFwCfgParse(gprGlueInfo->prAdapter, aucCfgBuf);

	return count;
}

static const struct file_operations cfg_ops = {
	.owner = THIS_MODULE,
	.read = cfgRead,
	.write = cfgWrite,
};

INT_32 cfgRemoveProcEntry(void)
{
	remove_proc_entry(PROC_CFG_NAME, gprProcNetRoot);
	return 0;
}

INT_32 cfgCreateProcEntry(P_GLUE_INFO_T prGlueInfo)
{
	struct proc_dir_entry *prEntry;

	prGlueInfo->pProcRoot = gprProcNetRoot;
	gprGlueInfo = prGlueInfo;

	prEntry = proc_create(PROC_CFG_NAME, 0664, gprProcNetRoot, &cfg_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry cfg\n\r");
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	return 0;
}
#endif
