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

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                    E X T E R N A L   R E F E R E N C E S
********************************************************************************
*/

#include "precomp.h"

#ifdef FW_CFG_SUPPORT
#include "fwcfg.h"
#endif
/* #include "wlan_lib.h" */
/* #include "debug.h" */

/*******************************************************************************
*                              C O N S T A N T S
********************************************************************************
*/

#define PROC_WLAN_THERMO                        "wlanThermo"
#define PROC_COUNTRY                            "country"
#define PROC_DRV_STATUS                         "status"
#define PROC_RX_STATISTICS                      "rx_statistics"
#define PROC_TX_STATISTICS                      "tx_statistics"
#define PROC_DBG_LEVEL_NAME						"dbgLevel"
#define PROC_NEED_TX_DONE						"TxDoneCfg"
#define PROC_AUTO_PERF_CFG						"autoPerfCfg"
#define PROC_ROOT_NAME			"wlan"
#define PROC_CMD_DEBUG_NAME		"cmdDebug"
#define PROC_CFG_NAME			"cfg"
#if CFG_SUPPORT_SET_CAM_BY_PROC
#define PROC_SET_CAM							"setCAM"
#endif

#define PROC_MCR_ACCESS_MAX_USER_INPUT_LEN      20
#define PROC_RX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_TX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_DBG_LEVEL_MAX_USER_INPUT_LEN       (20*10)
#define PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN      8

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
/* static UINT_32 u4McrOffset; */
#if CFG_SUPPORT_THERMO_THROTTLING
static P_GLUE_INFO_T g_prGlueInfo_proc;
#endif
#if FW_CFG_SUPPORT
static P_GLUE_INFO_T gprGlueInfo;
#endif

struct proc_dir_entry *prEntry;
typedef struct _PROC_CFG_ENTRY {
	struct proc_dir_entry *prEntryWlanThermo;
	struct proc_dir_entry *prEntryCountry;
	struct proc_dir_entry *prEntryStatus;
	struct proc_dir_entry *prEntryRxStatis;
	struct proc_dir_entry *prEntryTxStatis;
	struct proc_dir_entry *prEntryDbgLevel;
	struct proc_dir_entry *prEntryTxDoneCfg;
	struct proc_dir_entry *prEntryAutoPerCfg;
	struct proc_dir_entry *prEntryCmdDbg;
	struct proc_dir_entry *prEntryCfg;
#if CFG_SUPPORT_SET_CAM_BY_PROC
	struct proc_dir_entry *prEntrySetCAM;
#endif
} PROC_CFG_ENTRY, *P_PROC_CFG_ENTRY;
static P_PROC_CFG_ENTRY gprProcCfgEntry;

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
#if 0
static int procMCRRead(char *page, char **start, off_t off, int count, int *eof, void *data)
{
	P_GLUE_INFO_T prGlueInfo;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
	UINT_32 u4BufLen;
	char *p = page;
	UINT_32 u4Count;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv((struct net_device *)data));

	rMcrInfo.u4McrOffset = u4McrOffset;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidQueryMcrRead,
			   (PVOID)&rMcrInfo, sizeof(rMcrInfo), TRUE, TRUE, TRUE, FALSE, &u4BufLen);

	/* SPRINTF(p, ("MCR (0x%08lxh): 0x%08lx\n", */
	/* rMcrInfo.u4McrOffset, rMcrInfo.u4McrData)); */

	u4Count = (UINT_32) (p - page);

	*eof = 1;

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
static int procMCRWrite(struct file *file, const char *buffer, unsigned long count, void *data)
{
	P_GLUE_INFO_T prGlueInfo;
	char acBuf[PROC_MCR_ACCESS_MAX_USER_INPUT_LEN + 1];	/* + 1 for "\0" */
	int i4CopySize;
	PARAM_CUSTOM_MCR_RW_STRUCT_T rMcrInfo;
	UINT_32 u4BufLen;
	WLAN_STATUS rStatus = WLAN_STATUS_SUCCESS;

	ASSERT(data);

	i4CopySize = (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	if (copy_from_user(acBuf, buffer, i4CopySize))
		return 0;
	acBuf[i4CopySize] = '\0';

	if (sscanf(acBuf, "0x%lx 0x%lx", &rMcrInfo.u4McrOffset, &rMcrInfo.u4McrData) == 2) {
		/* NOTE: Sometimes we want to test if bus will still be ok, after accessing
		 * the MCR which is not align to DW boundary.
		 */
		/* if (IS_ALIGN_4(rMcrInfo.u4McrOffset)) */
		prGlueInfo = *((P_GLUE_INFO_T *) netdev_priv((struct net_device *)data));

		u4McrOffset = rMcrInfo.u4McrOffset;

		/* printk("Write 0x%lx to MCR 0x%04lx\n", */
		/* rMcrInfo.u4McrOffset, rMcrInfo.u4McrData); */

		rStatus = kalIoctl(prGlueInfo,
				wlanoidSetMcrWrite,
				(PVOID)&rMcrInfo, sizeof(rMcrInfo), FALSE, FALSE, TRUE, FALSE, &u4BufLen);
	}

	if (sscanf(acBuf, "0x%lx 0x%lx", &rMcrInfo.u4McrOffset, &rMcrInfo.u4McrData) == 1) {
		/* if (IS_ALIGN_4(rMcrInfo.u4McrOffset)) */
		u4McrOffset = rMcrInfo.u4McrOffset;
	}

	return count;

}				/* end of procMCRWrite() */
#endif

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

	if (kstrtoint(acBuf, 10, &u4ClearCounter) == 1) {
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

	if (kstrtoint(acBuf, 10, &u4ClearCounter) == 1) {
		if (u4ClearCounter == 1) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

			wlanoidSetTxStatisticsForLinuxProc(prGlueInfo->prAdapter);

			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);
		}
	}

	return count;

}				/* end of procTxStatisticsWrite() */
#endif
static struct proc_dir_entry *gprProcRoot;
static UINT_8 aucDbModuleName[][PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN] = {
	"INIT", "HAL", "INTR", "REQ", "TX", "RX", "RFTEST", "EMU", "SW1", "SW2",
	"SW3", "SW4", "HEM", "AIS", "RLM", "MEM", "CNM", "RSN", "BSS", "SCN",
	"SAA", "AAA", "P2P", "QM", "SEC", "BOW", "WAPI", "ROAMING", "TDLS", "OID",
	"NIC", "WNM", "WMM"
};
static UINT_8 aucProcBuf[1536];
static ssize_t procDbgLevelRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_8 *temp = &aucProcBuf[0];
	UINT_32 u4CopySize = 0;
	UINT_16 i;
	UINT_16 u2ModuleNum = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	kalStrCpy(temp, "\nTEMP |LOUD |INFO |TRACE|EVENT|STATE|WARN |ERROR\n"
			"bit7 |bit6 |bit5 |bit4 |bit3 |bit2 |bit1 |bit0\n\n"
			"Debug Module\tIndex\tLevel\tDebug Module\tIndex\tLevel\n\n");
	temp += kalStrLen(temp);

	u2ModuleNum = (sizeof(aucDbModuleName) / PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0xfe;
	for (i = 0; i < u2ModuleNum; i += 2)
		SPRINTF(temp, ("DBG_%s_IDX\t(0x%02x):\t0x%02x\tDBG_%s_IDX\t(0x%02x):\t0x%02x\n",
				&aucDbModuleName[i][0], i, aucDebugModule[i],
				&aucDbModuleName[i+1][0], i+1, aucDebugModule[i+1]));

	if ((sizeof(aucDbModuleName) / PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0x1)
		SPRINTF(temp, ("DBG_%s_IDX\t(0x%02x):\t0x%02x\n",
				&aucDbModuleName[u2ModuleNum][0], u2ModuleNum, aucDebugModule[u2ModuleNum]));

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		kalPrint("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static ssize_t procDbgLevelWrite(struct file *file, const char *buffer, size_t count, loff_t *data)
{
	UINT_32 u4NewDbgModule, u4NewDbgLevel;
	UINT_8 i = 0;
	UINT_32 u4CopySize = sizeof(aucProcBuf);
	UINT_8 *temp = &aucProcBuf[0];

	kalMemSet(aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		kalPrint("error of copy from user\n");
		return -EFAULT;
	}
	aucProcBuf[u4CopySize] = '\0';

	while (temp) {
		if (sscanf(temp, "0x%x:0x%x", &u4NewDbgModule, &u4NewDbgLevel) != 2)  {
			kalPrint("debug module and debug level should be one byte in length\n");
			break;
		}
		if (u4NewDbgModule == 0xFF) {
			for (i = 0; i < DBG_MODULE_NUM; i++)
				aucDebugModule[i] = u4NewDbgLevel & DBG_CLASS_MASK;

			break;
		} else if (u4NewDbgModule >= DBG_MODULE_NUM) {
			kalPrint("debug module index should less than %d\n", DBG_MODULE_NUM);
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

static ssize_t procTxDoneCfgRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_8 *temp = &aucProcBuf[0];
	UINT_32 u4CopySize = 0;
	UINT_16 u2TxDoneCfg = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	u2TxDoneCfg = StatsGetCfgTxDone();
	SPRINTF(temp, ("Tx Done Configure:\nARP %d\tDNS %d\nTCP %d\tUDP %d\nEAPOL %d\tDHCP %d\nICMP %d\n",
			!!(u2TxDoneCfg & CFG_ARP), !!(u2TxDoneCfg & CFG_DNS), !!(u2TxDoneCfg & CFG_TCP),
			!!(u2TxDoneCfg & CFG_UDP), !!(u2TxDoneCfg & CFG_EAPOL), !!(u2TxDoneCfg & CFG_DHCP),
			!!(u2TxDoneCfg & CFG_ICMP)));

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		kalPrint("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static ssize_t procTxDoneCfgWrite(struct file *file, const char *buffer, size_t count, loff_t *data)
{
#define MODULE_NAME_LENGTH 6

	UINT_8 i = 0;
	UINT_32 u4CopySize = sizeof(aucProcBuf);
	UINT_8 *temp = &aucProcBuf[0];
	UINT_16 u2SetTxDoneCfg = 0;
	UINT_16 u2ClsTxDoneCfg = 0;
	UINT_8 aucModule[MODULE_NAME_LENGTH];
	UINT_32 u4Enabled;
	UINT_8 aucModuleArray[][MODULE_NAME_LENGTH] = {"ARP", "DNS", "TCP", "UDP", "EAPOL", "DHCP", "ICMP"};

	kalMemSet(aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		kalPrint("error of copy from user\n");
		return -EFAULT;
	}
	aucProcBuf[u4CopySize] = '\0';
	temp = &aucProcBuf[0];
	while (temp) {
		kalMemSet(aucModule, 0, MODULE_NAME_LENGTH);

		/* pick up a string and teminated after meet : */
		if (sscanf(temp, "%5s %d", aucModule, &u4Enabled) != 2)  {
			kalPrint("read param fail, aucModule=%s\n", aucModule);
			break;
		}
		for (i = 0; i < sizeof(aucModuleArray)/MODULE_NAME_LENGTH; i++) {
			if (kalStrniCmp(aucModule, aucModuleArray[i], MODULE_NAME_LENGTH) == 0) {
				if (u4Enabled)
					u2SetTxDoneCfg |= 1 << i;
				else
					u2ClsTxDoneCfg |= 1 << i;
				break;
			}
		}
		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++; /* skip ',' */
	}
	if (u2SetTxDoneCfg)
		StatsSetCfgTxDone(u2SetTxDoneCfg, TRUE);

	if (u2ClsTxDoneCfg)
		StatsSetCfgTxDone(u2ClsTxDoneCfg, FALSE);
	return count;
}

static const struct file_operations proc_txdone_ops = {
	.owner = THIS_MODULE,
	.read = procTxDoneCfgRead,
	.write = procTxDoneCfgWrite,
};

static ssize_t procAutoPerfCfgRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_8 *temp = &aucProcBuf[0];
	UINT_32 u4CopySize = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	SPRINTF(temp, ("Auto Performance Configure:\ncore_num:frequence\n"));

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		kalPrint("copy to user failed\n");
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

	DBGLOG(INIT, WARN, "%s\n", __func__);

	if (u4CopySize >= sizeof(aucProcBuf))
		u4CopySize = sizeof(aucProcBuf) - 1;

	kalMemSet(aucProcBuf, 0, u4CopySize);

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		kalPrint("error of copy from user\n");
		return -EFAULT;
	}

	aucProcBuf[u4CopySize] = '\0';

	if (sscanf(temp, "%d:%d", &u4CoreNum, &u4CoreFreq) != 2)  {
		DBGLOG(INIT, WARN, "parameter format should be u4CoreNum:u4CoreFreq\n");
		return -EFAULT;
	}
	DBGLOG(INIT, WARN, "u4CoreNum:%d, u4CoreFreq:%d\n", u4CoreNum, u4CoreFreq);

	kalSetCpuNumFreq(u4CoreNum, u4CoreFreq);

	return u4CopySize;
}

static const struct file_operations auto_perf_ops = {
	.owner = THIS_MODULE,
	.read = procAutoPerfCfgRead,
	.write = procAutoPerfCfgWrite,
};


static ssize_t procCmdDebug(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	wlanDumpTcResAndTxedCmd(aucProcBuf, sizeof(aucProcBuf));

	u4CopySize = kalStrLen(aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		kalPrint("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t)u4CopySize;
}

static const struct file_operations proc_CmdDebug_ops = {
	.owner = THIS_MODULE,
	.read = procCmdDebug,
};

static ssize_t procCountryRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	UINT_32 u4CopySize;
	UINT_16 u2CountryCode = 0;

	/* if *f_pos > 0, it means has read successed last time, don't try again */
	if (*f_pos > 0)
		return 0;

	if (g_prGlueInfo_proc && g_prGlueInfo_proc->prAdapter)
		u2CountryCode = g_prGlueInfo_proc->prAdapter->rWifiVar.rConnSettings.u2CountryCode;

	if (u2CountryCode)
		kalSprintf(aucProcBuf, "Current Country Code: %c%c\n", (u2CountryCode>>8) & 0xff, u2CountryCode & 0xff);
	else
		kalStrnCpy(aucProcBuf, "Current Country Code: NULL\n", strlen("Current Country Code: NULL\n") + 1);

	u4CopySize = kalStrLen(aucProcBuf);
	if (copy_to_user(buf, aucProcBuf, u4CopySize)) {
		pr_info("copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (INT_32)u4CopySize;
}

static ssize_t procCountryWrite(struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	UINT_32 u4BufLen = 0;
	WLAN_STATUS rStatus;
	UINT_32 u4CopySize = sizeof(aucProcBuf);

	kalMemSet(aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_info("error of copy from user\n");
		return -EFAULT;
	}

	aucProcBuf[u4CopySize] = '\0';
	rStatus = kalIoctl(g_prGlueInfo_proc,
				wlanoidSetCountryCode, &aucProcBuf[0], 2, FALSE, FALSE, TRUE, FALSE, &u4BufLen);
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

/*----------------------------------------------------------------------------*/
/*!
* \brief This function create a PROC fs in linux /proc/net subdirectory.
*
* \param[in] prDev      Pointer to the struct net_device.
* \param[in] pucDevName Pointer to the name of net_device.
*
* \return N/A
*/
/*----------------------------------------------------------------------------*/

#if CFG_SUPPORT_THERMO_THROTTLING

/**
 * This function is called then the /proc file is read
 *
 */
typedef struct _COEX_BUF1 {
	UINT8 buffer[128];
	INT32 availSize;
} COEX_BUF1, *P_COEX_BUF1;

COEX_BUF1 gCoexBuf1;

static ssize_t procfile_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{

	INT32 retval = 0;
	INT32 i_ret = 0;
	CHAR *warn_msg = "no data available, please run echo 15 xx > /proc/driver/wmt_psm first\n";

	if (*f_pos > 0) {
		retval = 0;
	} else {
		/*len = sprintf(page, "%d\n", g_psm_enable); */
		if (gCoexBuf1.availSize <= 0) {
			DBGLOG(INIT, WARN, "no data available\n");
			retval = strlen(warn_msg) + 1;
			if (count < retval)
				retval = count;
			i_ret = copy_to_user(buf, warn_msg, retval);
			if (i_ret) {
				DBGLOG(INIT, ERROR, "copy to buffer failed, ret:%d\n", retval);
				retval = -EFAULT;
				goto err_exit;
			}
			*f_pos += retval;
		} else {
			INT32 i = 0;
			INT32 len = 0;
			CHAR msg_info[128];
			INT32 max_num = 0;
			/*
			 * we do not check page buffer, because there are only 100 bytes in g_coex_buf, no reason page
			 * buffer is not enough, a bomb is placed here on unexpected condition
			 */

			DBGLOG(INIT, TRACE, "%d bytes available\n", gCoexBuf1.availSize);
			max_num = ((sizeof(msg_info) > count ? sizeof(msg_info) : count) - 1) / 5;

			if (max_num > gCoexBuf1.availSize)
				max_num = gCoexBuf1.availSize;
			else
				DBGLOG(INIT, TRACE,
				"round to %d bytes due to local buffer size limitation\n", max_num);

			for (i = 0; i < max_num; i++)
				len += sprintf(msg_info + len, "%d", gCoexBuf1.buffer[i]);

			len += sprintf(msg_info + len, "\n");
			retval = len;

			i_ret = copy_to_user(buf, msg_info, retval);
			if (i_ret) {
				DBGLOG(INIT, ERROR, "copy to buffer failed, ret:%d\n", retval);
				retval = -EFAULT;
				goto err_exit;
			}
			*f_pos += retval;
		}
	}
	gCoexBuf1.availSize = 0;
err_exit:

	return retval;
}

typedef INT32 (*WLAN_DEV_DBG_FUNC)(void);
static INT32 wlan_get_thermo_power(void);
static INT32 wlan_get_link_mode(void);

static const WLAN_DEV_DBG_FUNC wlan_dev_dbg_func[] = {
	[0] = wlan_get_thermo_power,
	[1] = wlan_get_link_mode,

};

INT32 wlan_get_thermo_power(void)
{
	P_ADAPTER_T prAdapter;

	prAdapter = g_prGlueInfo_proc->prAdapter;

	if (prAdapter->u4AirDelayTotal > 100)
		gCoexBuf1.buffer[0] = 100;
	else
		gCoexBuf1.buffer[0] = prAdapter->u4AirDelayTotal;
	gCoexBuf1.availSize = 1;
	DBGLOG(RLM, TRACE, "PROC %s thrmo_power(%d)\n", __func__, gCoexBuf1.buffer[0]);

	return 0;
}

INT32 wlan_get_link_mode(void)
{
	UINT_8 ucLinkMode = 0;
	P_ADAPTER_T prAdapter;
	BOOLEAN fgIsAPmode;

	prAdapter = g_prGlueInfo_proc->prAdapter;
	fgIsAPmode = p2pFuncIsAPMode(prAdapter->rWifiVar.prP2pFsmInfo);

	DBGLOG(RLM, TRACE, "PROC %s AIS(%d)P2P(%d)AP(%d)\n",
			   __func__,
			   prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].eConnectionState,
			   prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX].eConnectionState, fgIsAPmode);


	if (prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_AIS_INDEX].eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
		ucLinkMode |= BIT(0);
	if (prAdapter->rWifiVar.arBssInfo[NETWORK_TYPE_P2P_INDEX].eConnectionState == PARAM_MEDIA_STATE_CONNECTED)
		ucLinkMode |= BIT(1);
	if (fgIsAPmode)
		ucLinkMode |= BIT(2);

	gCoexBuf1.buffer[0] = ucLinkMode;
	gCoexBuf1.availSize = 1;

	return 0;
}

static ssize_t procfile_write(struct file *filp, const char __user *buffer,
			      size_t count, loff_t *f_pos)
{
	char buf[256];
	char *pBuf;
	ULONG len = count;
	unsigned int x = 0;
	char *pToken = NULL;
	char *pDelimiter = " \t";
	INT32 i4Ret = -1;

	DBGLOG(INIT, TRACE, "write parameter len = %d\n\r", (INT32) len);
	if (len >= sizeof(buf)) {
		DBGLOG(INIT, ERROR, "input handling fail!\n");
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	DBGLOG(INIT, TRACE, "write parameter data = %s\n\r", buf);
	pBuf = buf;
	pToken = strsep(&pBuf, pDelimiter);
	if (pToken) {
		i4Ret = kalkStrtou32(pToken, 16, &x);
		if (!i4Ret)
			DBGLOG(INIT, TRACE, " x(0x%08x)\n\r", x);
	}

	if ((!i4Ret) && (ARRAY_SIZE(wlan_dev_dbg_func) > x) &&
	    (NULL != wlan_dev_dbg_func[x]))
		(*wlan_dev_dbg_func[x]) ();
	else
		DBGLOG(INIT, ERROR,
		       "no handler defined for command id(0x%08x), pToken=%p, i4Ret=%d\n\r",
		       x, pToken, i4Ret);

	return len;
}

static const struct file_operations proc_fops = {
	.owner = THIS_MODULE,
	.read = procfile_read,
	.write = procfile_write,
};
#endif

#if CFG_SUPPORT_SET_CAM_BY_PROC
static ssize_t procSetCamCfgWrite(struct file *file, const char *buffer, size_t count, loff_t *data)
{
#define MODULE_NAME_LEN_1 5

	UINT_32 u4CopySize = sizeof(aucProcBuf);
	UINT_8 *temp = &aucProcBuf[0];
	BOOLEAN fgSetCamCfg = FALSE;
	UINT_8 aucModule[MODULE_NAME_LEN_1];
	UINT_32 u4Enabled;
	UINT_8 aucModuleArray[MODULE_NAME_LEN_1] = "CAM";

	kalMemSet(aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(aucProcBuf, buffer, u4CopySize)) {
		pr_info("error of copy from user\n");
		return -EFAULT;
	}
	aucProcBuf[u4CopySize] = '\0';
	temp = &aucProcBuf[0];
	while (temp) {
		kalMemSet(aucModule, 0, MODULE_NAME_LEN_1);

		/* pick up a string and teminated after meet : */
		if (sscanf(temp, "%4s %d", aucModule, &u4Enabled) != 2)  {
			pr_info("read param fail, aucModule=%s\n", aucModule);
			break;
		}

		if (kalStrnCmp(aucModule, aucModuleArray, MODULE_NAME_LEN_1) == 0) {
			if (u4Enabled)
				fgSetCamCfg = TRUE;
			else
				fgSetCamCfg = FALSE;
			break;
		}
		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++; /* skip ',' */
	}

	nicConfigProcSetCamCfgWrite(fgSetCamCfg);

	return count;
}

static const struct file_operations proc_set_cam_ops = {
	.owner = THIS_MODULE,
	.write = procSetCamCfgWrite,
};
#endif

INT_32 procInitFs(VOID)
{
	if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
		DBGLOG(INIT, ERROR, "init proc fs fail: proc_net == NULL\n");
		return -ENOENT;
	}

	/*
	 * Directory: Root (/proc/net/wlan0)
	 */

	gprProcRoot = proc_mkdir(PROC_ROOT_NAME, init_net.proc_net);
	if (!gprProcRoot) {
		DBGLOG(INIT, ERROR, "gprProcRoot == NULL\n");
		return -ENOENT;
	}
	proc_set_user(gprProcRoot, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	gprProcCfgEntry = kalMemAlloc(sizeof(PROC_CFG_ENTRY), PHY_MEM_TYPE);
	kalMemZero(gprProcCfgEntry, sizeof(PROC_CFG_ENTRY));

	gprProcCfgEntry->prEntryDbgLevel = proc_create(PROC_DBG_LEVEL_NAME, 0664, gprProcRoot, &dbglevel_ops);

	if (gprProcCfgEntry->prEntryDbgLevel == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry %s/n", PROC_DBG_LEVEL_NAME);
		return -1;
	}
	proc_set_user(gprProcCfgEntry->prEntryDbgLevel, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	gprProcCfgEntry->prEntryTxDoneCfg = proc_create(PROC_NEED_TX_DONE, 0664, gprProcRoot, &proc_txdone_ops);
	if (gprProcCfgEntry->prEntryTxDoneCfg == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry %s/n", PROC_NEED_TX_DONE);
		return -1;
	}
	proc_set_user(gprProcCfgEntry->prEntryTxDoneCfg, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	gprProcCfgEntry->prEntryAutoPerCfg = proc_create(PROC_AUTO_PERF_CFG, 0664, gprProcRoot, &auto_perf_ops);
	if (gprProcCfgEntry->prEntryAutoPerCfg == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry %s/n", PROC_AUTO_PERF_CFG);
		return -1;
	}
	proc_set_user(gprProcCfgEntry->prEntryAutoPerCfg, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	gprProcCfgEntry->prEntryCountry = proc_create(PROC_COUNTRY, 0664, gprProcRoot, &country_ops);
	if (gprProcCfgEntry->prEntryCountry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry %s/n", PROC_COUNTRY);
		return -1;
	}

	return 0;
}				/* end of procInitProcfs() */

INT_32 procUninitProcFs(VOID)
{
	if (gprProcRoot) {
		if (gprProcCfgEntry->prEntryDbgLevel)
			remove_proc_entry(PROC_DBG_LEVEL_NAME, gprProcRoot);
		else
			DBGLOG(INIT, ERROR, "%s PROC_DBG_LEVEL_NAME is null/n", __func__);

		if (gprProcCfgEntry->prEntryAutoPerCfg)
			remove_proc_entry(PROC_AUTO_PERF_CFG, gprProcRoot);
		else
			DBGLOG(INIT, ERROR, "%s PROC_AUTO_PERF_CFG is null/n", __func__);

		if (gprProcCfgEntry->prEntryCountry)
			remove_proc_entry(PROC_COUNTRY, gprProcRoot);
		else
			DBGLOG(INIT, ERROR, "%s PROC_COUNTRY is null/n", __func__);

		remove_proc_subtree(PROC_ROOT_NAME, init_net.proc_net);

		if (gprProcCfgEntry)
			kalMemFree(gprProcCfgEntry, sizeof(PROC_CFG_ENTRY), PHY_MEM_TYPE);

	} else {
		DBGLOG(INIT, ERROR, "%s gprProcRoot ist null/n", __func__);
	}
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
	/* remove root directory (proc/net/wlan0) */
	/* remove_proc_entry(pucDevName, init_net.proc_net); */
	if (gprProcCfgEntry->prEntryWlanThermo) {
		remove_proc_entry(PROC_WLAN_THERMO, gprProcRoot);
		gprProcCfgEntry->prEntryWlanThermo = NULL;
	} else
		DBGLOG(INIT, ERROR, "%s PROC_WLAN_THERMO is null\n", __func__);

	if (gprProcCfgEntry->prEntryCmdDbg) {
		remove_proc_entry(PROC_CMD_DEBUG_NAME, gprProcRoot);
		gprProcCfgEntry->prEntryCmdDbg = NULL;
	} else
		DBGLOG(INIT, ERROR, "%s PROC_CMD_DEBUG_NAME is null\n", __func__);

#if CFG_SUPPORT_SET_CAM_BY_PROC
	if (gprProcCfgEntry->prEntrySetCAM) {
		remove_proc_entry(PROC_SET_CAM, gprProcRoot);
		gprProcCfgEntry->prEntrySetCAM = NULL;
	} else
		DBGLOG(INIT, ERROR, "%s PROC_SET_CAM is null\n", __func__);
#endif

#if CFG_SUPPORT_THERMO_THROTTLING
	g_prGlueInfo_proc = NULL;
#endif
	return 0;
}				/* end of procRemoveProcfs() */

INT_32 procCreateFsEntry(P_GLUE_INFO_T prGlueInfo)
{

	DBGLOG(INIT, TRACE, "[%s]\n", __func__);

#if CFG_SUPPORT_THERMO_THROTTLING
	g_prGlueInfo_proc = prGlueInfo;
#endif

	prGlueInfo->pProcRoot = gprProcRoot;
	if (gprProcCfgEntry->prEntryWlanThermo)
		DBGLOG(INIT, WARN, "proc entry %s is exist\n", PROC_WLAN_THERMO);

	gprProcCfgEntry->prEntryWlanThermo = proc_create(PROC_WLAN_THERMO, 0664, gprProcRoot, &proc_fops);
	if (gprProcCfgEntry->prEntryWlanThermo == NULL) {
		DBGLOG(INIT, WARN, "Unable to create /proc entry %s\n", PROC_WLAN_THERMO);
		return -1;
	}

	if (gprProcCfgEntry->prEntryCmdDbg)
		DBGLOG(INIT, WARN, "proc entry %s is exist\n", PROC_CMD_DEBUG_NAME);

	gprProcCfgEntry->prEntryCmdDbg = proc_create(PROC_CMD_DEBUG_NAME, 0444, gprProcRoot, &proc_CmdDebug_ops);
	if (gprProcCfgEntry->prEntryCmdDbg == NULL) {
		DBGLOG(INIT, WARN, "Unable to create /proc entry %s\n", PROC_CMD_DEBUG_NAME);
		return -1;
	}
	proc_set_user(gprProcCfgEntry->prEntryCmdDbg, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

#if CFG_SUPPORT_SET_CAM_BY_PROC
	if (gprProcCfgEntry->prEntrySetCAM)
		DBGLOG(INIT, WARN, "proc entry %s is exist\n", PROC_SET_CAM);

	gprProcCfgEntry->prEntrySetCAM = proc_create(PROC_SET_CAM, 0664, gprProcRoot, &proc_set_cam_ops);
	if (gprProcCfgEntry->prEntrySetCAM == NULL) {
		DBGLOG(INIT, WARN, "Unable to create /proc entry %s\n", PROC_SET_CAM);
		return -1;
	}
	proc_set_user(gprProcCfgEntry->prEntrySetCAM, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));
#endif

	return 0;
}

#ifdef FW_CFG_SUPPORT
#define MAX_CFG_OUTPUT_BUF_LENGTH 1024
static UINT_8 aucCfgBuf[CMD_FORMAT_V1_LENGTH];
static UINT_8 aucCfgQueryKey[MAX_CMD_NAME_MAX_LENGTH];
static UINT_8 aucCfgOutputBuf[MAX_CFG_OUTPUT_BUF_LENGTH];

static ssize_t cfgRead(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	WLAN_STATUS rStatus = WLAN_STATUS_FAILURE;
	UINT_8 *temp = &aucCfgOutputBuf[0];
	UINT_32 u4CopySize = 0;
	UINT_32 u4Offset = 0;
	PCHAR pHeadOutputString = "\nprocCfgRead() ";
	struct _CMD_HEADER_T cmdV1Header;
	struct _CMD_FORMAT_V1_T *pr_cmd_v1 = (struct _CMD_FORMAT_V1_T *) cmdV1Header.buffer;

	/* if *f_pos >  0, we should return 0 to make cat command exit */
	if (*f_pos > 0 || gprGlueInfo == NULL)
		return 0;

	kalMemSet(aucCfgOutputBuf, '\0', MAX_CFG_OUTPUT_BUF_LENGTH);

	u4Offset += kalSnprintf(temp + u4Offset, MAX_CFG_OUTPUT_BUF_LENGTH - u4Offset, "%s",
		  pHeadOutputString);

	u4Offset += kalSnprintf(temp + u4Offset, MAX_CFG_OUTPUT_BUF_LENGTH - u4Offset, "%s:\n",
			  aucCfgQueryKey);

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
			FALSE,
			&u4CopySize);
	if (rStatus == WLAN_STATUS_FAILURE)
		DBGLOG(INIT, ERROR, "prCmdV1Header kalIoctl wlanoidQueryCfgRead fail 0x%x\n", rStatus);

	u4Offset += kalSnprintf(temp + u4Offset, MAX_CFG_OUTPUT_BUF_LENGTH - u4Offset, "%s\n",
		  cmdV1Header.buffer);

	u4CopySize = kalStrLen(aucCfgOutputBuf);
	DBGLOG(INIT, INFO, "cfgRead: count:%zu  u4CopySize:%d,[%s]\n", count, u4CopySize, aucCfgOutputBuf);
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

	kalMemSet(aucCfgBuf, '\0', u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(aucCfgBuf, buf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy from user failed\n");
		return -EFAULT;
	}

	if (u4CopySize > 0)
		aucCfgBuf[u4CopySize - 1] = '\0';

	for (; i < u4CopySize; i++) {
		if (aucCfgBuf[i] == ' ') {
			token_num++;
			break;
		}
	}

	DBGLOG(INIT, INFO, "procCfgWrite :%s token_num:%d input_size :%zu\n"
		, aucCfgBuf, token_num, count);

	if (token_num == 1) {
		kalMemSet(aucCfgQueryKey, 0, sizeof(aucCfgQueryKey));
		u4CopySize = ((u4CopySize > sizeof(aucCfgQueryKey)) ? sizeof(aucCfgQueryKey) : u4CopySize);
		memcpy(aucCfgQueryKey, aucCfgBuf, u4CopySize);
		/*replace Carriage Return (0x0a) to string end of terminal */
		if ((u4CopySize > 0) && (aucCfgQueryKey[u4CopySize - 1] == 0x0a))
			aucCfgQueryKey[u4CopySize - 1] = '\0';
	} else {
		if (u4CopySize)
			wlanFwCfgParse(gprGlueInfo->prAdapter, aucCfgBuf);
	}

	return count;
}

static const struct file_operations cfg_ops = {
	.owner = THIS_MODULE,
	.read = cfgRead,
	.write = cfgWrite,
};

INT_32 cfgRemoveProcEntry(void)
{
	if (gprProcCfgEntry->prEntryCfg) {
		remove_proc_entry(PROC_CFG_NAME, gprProcRoot);
		gprProcCfgEntry->prEntryCfg = NULL;
	} else
		DBGLOG(INIT, ERROR, "%s PROC_CFG_NAME is null\n", __func__);
	return 0;
}

INT_32 cfgCreateProcEntry(P_GLUE_INFO_T prGlueInfo)
{
	prGlueInfo->pProcRoot = gprProcRoot;
	gprGlueInfo = prGlueInfo;

	gprProcCfgEntry->prEntryCfg = proc_create(PROC_CFG_NAME, 0664, gprProcRoot, &cfg_ops);

	if (gprProcCfgEntry->prEntryCfg == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry cfg\n\r");
		return -1;
	}
	proc_set_user(gprProcCfgEntry->prEntryCfg, KUIDT_INIT(PROC_UID_SHELL), KGIDT_INIT(PROC_GID_WIFI));

	return 0;
}
#endif
