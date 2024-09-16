/******************************************************************************
 *
 * This file is provided under a dual license.  When you use or
 * distribute this software, you may choose to be licensed under
 * version 2 of the GNU General Public License ("GPLv2 License")
 * or BSD License.
 *
 * GPLv2 License
 *
 * Copyright(C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 *
 * BSD LICENSE
 *
 * Copyright(C) 2016 MediaTek Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *  * Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *****************************************************************************/
/*
 ** Id: /os/linux/gl_proc.c
 */

/*! \file   "gl_proc.c"
 *  \brief  This file defines the interface which can interact with users
 *          in /proc fs.
 *
 *    Detail description.
 */


/*******************************************************************************
 *                         C O M P I L E R   F L A G S
 *******************************************************************************
 */

/*******************************************************************************
 *                    E X T E R N A L   R E F E R E N C E S
 *******************************************************************************
 */
#include "precomp.h"
#include "gl_os.h"
#include "gl_kal.h"
#include "debug.h"
#include "wlan_lib.h"
#include "debug.h"
#include "wlan_oid.h"

/*******************************************************************************
 *                              C O N S T A N T S
 *******************************************************************************
 */
#define PROC_MCR_ACCESS                         "mcr"
#define PROC_ROOT_NAME							"wlan"

#if CFG_SUPPORT_DEBUG_FS
#define PROC_ROAM_PARAM							"roam_param"
#endif
#define PROC_COUNTRY							"country"
#define PROC_DRV_STATUS                         "status"
#define PROC_RX_STATISTICS                      "rx_statistics"
#define PROC_TX_STATISTICS                      "tx_statistics"
#define PROC_DBG_LEVEL_NAME                     "dbgLevel"
#define PROC_DRIVER_CMD                         "driver"
#define PROC_CFG                                "cfg"
#define PROC_EFUSE_DUMP                         "efuse_dump"
#define PROC_GET_TXPWR_TBL                      "get_txpwr_tbl"
#define PROC_PKT_DELAY_DBG			"pktDelay"
#if CFG_SUPPORT_SET_CAM_BY_PROC
#define PROC_SET_CAM				"setCAM"
#endif
#define PROC_AUTO_PERF_CFG			"autoPerfCfg"

#if CFG_DISCONN_DEBUG_FEATURE
#define PROC_DISCONN_INFO                       "disconn_info"
#endif

#define PROC_MCR_ACCESS_MAX_USER_INPUT_LEN      20
#define PROC_RX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_TX_STATISTICS_MAX_USER_INPUT_LEN   10
#define PROC_DBG_LEVEL_MAX_USER_INPUT_LEN       20
#define PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN      30
#define PROC_UID_SHELL							2000
#define PROC_GID_WIFI							1010

/* notice: str only can be an array */
#define SNPRINTF(buf, str, arg)   {buf += \
	snprintf((char *)(buf), sizeof(str)-kalStrLen(str), PRINTF_ARG arg); }

#ifdef CFG_GET_TEMPURATURE
#define PROC_GET_TEMPETATURE			"get_temperature"
#endif


/*******************************************************************************
 *                             D A T A   T Y P E S
 *******************************************************************************
 */

/*******************************************************************************
 *                            P U B L I C   D A T A
 *******************************************************************************
 */

/*******************************************************************************
 *                           P R I V A T E   D A T A
 *******************************************************************************
 */
static struct GLUE_INFO *g_prGlueInfo_proc;
static uint32_t u4McrOffset;
static struct proc_dir_entry *gprProcRoot;
static uint8_t aucDbModuleName[][PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN] = {
	"INIT", "HAL", "INTR", "REQ", "TX", "RX", "RFTEST", "EMU",
	"SW1", "SW2", "SW3", "SW4", "HEM", "AIS", "RLM", "MEM",
	"CNM", "RSN", "BSS", "SCN", "SAA", "AAA", "P2P", "QM",
	"SEC", "BOW", "WAPI", "ROAMING", "TDLS", "PF", "OID", "NIC"
};

/* This buffer could be overwrite by any proc commands */
static uint8_t g_aucProcBuf[3000];

/* This u32 is only for DriverCmdRead/Write,
 * should not be used by other function
 */
static int32_t g_NextDriverReadLen;
/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */
#define GET_VARNAME(var) #var

/*******************************************************************************
 *                   F U N C T I O N   D E C L A R A T I O N S
 *******************************************************************************
 */
static ssize_t procDbgLevelRead(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	uint8_t *temp = &g_aucProcBuf[0];
	uint8_t *str = NULL;
	uint32_t u4CopySize = 0;
	uint16_t i;
	uint16_t u2ModuleNum = 0;
	uint32_t u4StrLen = 0;
	uint32_t u4Level1, u4Level2;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0 || buf == NULL)
		return 0;

	str = "\nTEMP|LOUD|INFO|TRACE | EVENT|STATE|WARN|ERROR\n"
	    "bit7|bit6|bit5|bit4 | bit3|bit2|bit1|bit0\n\n"
	    "Usage: Module Index:Module Level, such as 0x00:0xff\n\n"
	    "Debug Module\tIndex\tLevel\tDebug Module\tIndex\tLevel\n\n";
	u4StrLen = kalStrLen(str);
	kalStrnCpy(temp, str, u4StrLen + 1);
	temp += kalStrLen(temp);

	u2ModuleNum =
	    (sizeof(aucDbModuleName) /
	     PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0xfe;

	for (i = 0; i < u2ModuleNum; i += 2) {
		wlanGetDriverDbgLevel(i, &u4Level1);
		wlanGetDriverDbgLevel(i + 1, &u4Level2);
		SNPRINTF(temp, g_aucProcBuf,
			("DBG_%s_IDX\t(0x%02x):\t0x%02x\t"
			 "DBG_%s_IDX\t(0x%02x):\t0x%02x\n",
			 &aucDbModuleName[i][0], i, (uint8_t) u4Level1,
			 &aucDbModuleName[i + 1][0], i + 1,
			 (uint8_t) u4Level2));
	}

	if ((sizeof(aucDbModuleName) /
	     PROC_DBG_LEVEL_MAX_DISPLAY_STR_LEN) & 0x1) {
		wlanGetDriverDbgLevel(u2ModuleNum, &u4Level1);
		SNPRINTF(temp, g_aucProcBuf,
			 ("DBG_%s_IDX\t(0x%02x):\t0x%02x\n",
			  &aucDbModuleName[u2ModuleNum][0], u2ModuleNum,
			  (uint8_t) u4Level1));
	}

	u4CopySize = kalStrLen(g_aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t) u4CopySize;
}

#if WLAN_INCLUDE_PROC
#if	CFG_SUPPORT_EASY_DEBUG

static void *procEfuseDump_start(struct seq_file *s, loff_t *pos)
{
	static unsigned long counter;

	if (*pos == 0)
		counter = *pos;	/* read file init */

	if (counter >= EFUSE_ADDR_MAX)
		return NULL;
	return &counter;
}

static void *procEfuseDump_next(struct seq_file *s, void *v, loff_t *pos)
{
	unsigned long *tmp_v = (unsigned long *)v;

	(*tmp_v) += EFUSE_BLOCK_SIZE;

	if (*tmp_v >= EFUSE_ADDR_MAX)
		return NULL;
	return tmp_v;
}

static void procEfuseDump_stop(struct seq_file *s, void *v)
{
	/* nothing to do, we use a static value in start() */
}

static int procEfuseDump_show(struct seq_file *s, void *v)
{
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	uint32_t u4BufLen = 0;
	struct GLUE_INFO *prGlueInfo;
	uint32_t idx_addr, idx_value;
	struct PARAM_CUSTOM_ACCESS_EFUSE rAccessEfuseInfo = { };

	prGlueInfo = g_prGlueInfo_proc;

#if  (CFG_EEPROM_PAGE_ACCESS == 1)
	ASSERT(prGlueInfo);
	if (prGlueInfo->prAdapter &&
	    prGlueInfo->prAdapter->chip_info &&
	    !prGlueInfo->prAdapter->chip_info->is_support_efuse) {
		seq_puts(s, "efuse ops is invalid\n");
		return -EPERM; /* return negative value to stop read process */
	}

	idx_addr = *(loff_t *) v;
	rAccessEfuseInfo.u4Address =
		(idx_addr / EFUSE_BLOCK_SIZE) * EFUSE_BLOCK_SIZE;

	rStatus = kalIoctl(prGlueInfo,
		wlanoidQueryProcessAccessEfuseRead,
		&rAccessEfuseInfo,
		sizeof(struct PARAM_CUSTOM_ACCESS_EFUSE), TRUE, TRUE,
		TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		seq_printf(s, "efuse read fail (0x%03X)\n",
			rAccessEfuseInfo.u4Address);
		return 0;
	}

	for (idx_value = 0; idx_value < EFUSE_BLOCK_SIZE; idx_value++)
		seq_printf(s, "0x%03X=0x%02X\n",
			rAccessEfuseInfo.u4Address + idx_value,
			prGlueInfo->prAdapter->aucEepromVaule[idx_value]);
	return 0;
#else
	seq_puts(s, "efuse ops is invalid\n");
	return -EPERM; /* return negative value to stop read process */
#endif
}

static int procEfuseDumpOpen(struct inode *inode, struct file *file)
{
	static const struct seq_operations procEfuseDump_ops = {
		.start = procEfuseDump_start,
		.next = procEfuseDump_next,
		.stop = procEfuseDump_stop,
		.show = procEfuseDump_show
	};

	return seq_open(file, &procEfuseDump_ops);
}

static ssize_t procCfgRead(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	uint8_t *temp = &g_aucProcBuf[0];
	uint8_t *str = NULL;
	uint8_t *str2 = "\nERROR DUMP CONFIGURATION:\n";
	uint32_t u4CopySize = 0;
	uint32_t i;
	uint32_t u4StrLen = 0;

#define BUFFER_RESERVE_BYTE 50

	struct GLUE_INFO *prGlueInfo;

	struct WLAN_CFG_ENTRY *prWlanCfgEntry;
	struct ADAPTER *prAdapter;

	prGlueInfo = *((struct GLUE_INFO **)netdev_priv(gPrDev));

	if (!prGlueInfo) {
		DBGLOG(INIT, ERROR, "procCfgRead prGlueInfo is  NULL????\n");
		return 0;
	}

	prAdapter = prGlueInfo->prAdapter;

	if (!prAdapter) {
		DBGLOG(INIT, ERROR, "procCfgRead prAdapter is  NULL????\n");
		return 0;
	}

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0 || buf == NULL)
		return 0;

	str = "\nDUMP CONFIGURATION :\n"
	    "<KEY|VALUE> OR <D:KEY|VALUE>\n"
	    "'D': driver part current setting\n"
	    "===================================\n";
	u4StrLen = kalStrLen(str);
	kalStrnCpy(temp, str, u4StrLen + 1);
	temp += kalStrLen(temp);

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry = wlanCfgGetEntryByIndex(prAdapter, i, 0);

		if ((!prWlanCfgEntry) || (prWlanCfgEntry->aucKey[0] == '\0'))
			break;

		SNPRINTF(temp, g_aucProcBuf,
			("%s|%s\n", prWlanCfgEntry->aucKey,
			prWlanCfgEntry->aucValue));

		if ((temp - g_aucProcBuf) != kalStrLen(g_aucProcBuf)) {
			DBGLOG(INIT, ERROR,
			       "Dump configuration error: temp offset=%d, buf length=%u, key[%d]=[%u], val[%d]=[%u]\n",
			       (int)(temp - g_aucProcBuf),
			       (uint32_t)kalStrLen(g_aucProcBuf),
			       WLAN_CFG_VALUE_LEN_MAX,
			       (uint32_t)prWlanCfgEntry->aucKey[
				WLAN_CFG_VALUE_LEN_MAX - 1],
			       WLAN_CFG_VALUE_LEN_MAX,
			       (uint32_t)prWlanCfgEntry->aucValue[
				WLAN_CFG_VALUE_LEN_MAX - 1]);
			kalMemSet(g_aucProcBuf, ' ', u4StrLen);
			kalStrnCpy(g_aucProcBuf, str2, kalStrLen(str2) + 1);
			goto procCfgReadLabel;
		}

		if (kalStrLen(g_aucProcBuf) >
			(sizeof(g_aucProcBuf) - BUFFER_RESERVE_BYTE))
			break;
	}

	for (i = 0; i < WLAN_CFG_REC_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry = wlanCfgGetEntryByIndex(prAdapter, i, 1);


		if ((!prWlanCfgEntry) || (prWlanCfgEntry->aucKey[0] == '\0'))
			break;

		SNPRINTF(temp, g_aucProcBuf,
			("D:%s|%s\n", prWlanCfgEntry->aucKey,
			prWlanCfgEntry->aucValue));

		if ((temp - g_aucProcBuf) != kalStrLen(g_aucProcBuf)) {
			DBGLOG(INIT, ERROR,
			       "D:Dump configuration error: temp offset=%d, buf length=%u, key[%d]=[%u], val[%d]=[%u]\n",
			       (int)(temp - g_aucProcBuf),
			       (uint32_t)kalStrLen(g_aucProcBuf),
			       WLAN_CFG_VALUE_LEN_MAX,
			       (uint32_t)prWlanCfgEntry->aucKey[
				WLAN_CFG_VALUE_LEN_MAX - 1],
			       WLAN_CFG_VALUE_LEN_MAX,
			       (uint32_t)prWlanCfgEntry->aucValue[
				WLAN_CFG_VALUE_LEN_MAX - 1]);
			kalMemSet(g_aucProcBuf, ' ', u4StrLen);
			kalStrnCpy(g_aucProcBuf, str2, kalStrLen(str2) + 1);
			goto procCfgReadLabel;
		}

		if (kalStrLen(g_aucProcBuf) >
			(sizeof(g_aucProcBuf) - BUFFER_RESERVE_BYTE))
			break;
	}

procCfgReadLabel:
	u4CopySize = kalStrLen(g_aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t) u4CopySize;
}

static ssize_t procCfgWrite(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{

	/*      uint32_t u4DriverCmd, u4DriverValue;
	 *uint8_t *temp = &g_aucProcBuf[0];
	 */
	int32_t i4CopySize = sizeof(g_aucProcBuf)-8;
	struct GLUE_INFO *prGlueInfo;
	uint8_t *pucTmp;
	/* PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P; */


	kalMemSet(g_aucProcBuf, 0, i4CopySize);
	i4CopySize = (count < i4CopySize) ? count : (i4CopySize - 1);

	pucTmp = g_aucProcBuf;
	SNPRINTF(pucTmp, g_aucProcBuf, ("%s ", "set_cfg"));

	if ((i4CopySize < 0) || (copy_from_user(pucTmp, buffer, i4CopySize))) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[i4CopySize + 8] = '\0';


	prGlueInfo = g_prGlueInfo_proc;
	/* if g_u4NextDriverReadLen >0,
	 * the content for next DriverCmdRead will be
	 * in : g_aucProcBuf with length : g_u4NextDriverReadLen
	 */
	g_NextDriverReadLen =
		priv_driver_set_cfg(prGlueInfo->prDevHandler, g_aucProcBuf,
		sizeof(g_aucProcBuf));

	return count;

}

static ssize_t procDriverCmdRead(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	/* DriverCmd read should only be executed right after
	 * a DriverCmd write because content buffer 'g_aucProcBuf'
	 * is a global buffer for all proc command, otherwise ,
	 * the content could be overwrite by other proc command
	 */
	uint32_t u4CopySize = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0 || buf == NULL)
		return 0;

	if (g_NextDriverReadLen > 0)	/* Detect content to show */
		u4CopySize = g_NextDriverReadLen;

	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}
	g_NextDriverReadLen = 0;

	*f_pos += u4CopySize;
	return (ssize_t) u4CopySize;
}



static ssize_t procDriverCmdWrite(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
/*	UINT_32 u4DriverCmd, u4DriverValue;
 *	UINT_8 *temp = &g_aucProcBuf[0];
 */
	uint32_t u4CopySize = sizeof(g_aucProcBuf);
	struct GLUE_INFO *prGlueInfo;
/*	PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P; */

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';


	prGlueInfo = g_prGlueInfo_proc;
	/* if g_u4NextDriverReadLen >0,
	 * the content for next DriverCmdRead will be
	 *  in : g_aucProcBuf with length : g_u4NextDriverReadLen
	 */
	g_NextDriverReadLen =
		priv_driver_cmds(prGlueInfo->prDevHandler, g_aucProcBuf,
		sizeof(g_aucProcBuf));

	return count;
}
#endif
#endif

static ssize_t procDbgLevelWrite(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	uint32_t u4NewDbgModule, u4NewDbgLevel;
	uint8_t *temp = &g_aucProcBuf[0];
	uint32_t u4CopySize = sizeof(g_aucProcBuf);

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';

	/*add chip reset cmd for manual test*/
#if CFG_CHIP_RESET_SUPPORT
	if (temp[0] == 'R') {

		DBGLOG(INIT, ERROR, "WIFI trigger reset!!\n");
		glGetRstReason(RST_CMD_TRIGGER);
		GL_RESET_TRIGGER(g_prGlueInfo_proc->prAdapter,
					RST_FLAG_CHIP_RESET);
		temp[0] = 'X';
	}
#endif

	while (temp) {
		if (sscanf(temp,
			"0x%x:0x%x", &u4NewDbgModule, &u4NewDbgLevel) != 2) {
			DBGLOG(INIT, INFO,
				"debug module and debug level should be one byte in length\n");
			break;
		}
		if (u4NewDbgModule == 0xFF) {
			wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX,
					(u4NewDbgLevel & DBG_CLASS_MASK));
			break;
		}
		if (u4NewDbgModule >= DBG_MODULE_NUM) {
			DBGLOG(INIT, INFO,
				"debug module index should less than %d\n",
				DBG_MODULE_NUM);
			break;
		}
		wlanSetDriverDbgLevel(u4NewDbgModule,
				(u4NewDbgLevel & DBG_CLASS_MASK));
		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++;		/* skip ',' */
	}
	return count;
}

#define TXPWR_TABLE_ENTRY(_siso_mcs, _cdd_mcs, _mimo_mcs, _idx)	\
{								\
	.mcs[STREAM_SISO] = _siso_mcs,				\
	.mcs[STREAM_CDD] = _cdd_mcs,				\
	.mcs[STREAM_MIMO] = _mimo_mcs,				\
	.idx = (_idx),						\
}

static struct txpwr_table_entry dsss[] = {
	TXPWR_TABLE_ENTRY("DSSS1", "", "", MODULATION_SYSTEM_CCK_1M),
	TXPWR_TABLE_ENTRY("DSSS2", "", "", MODULATION_SYSTEM_CCK_2M),
	TXPWR_TABLE_ENTRY("CCK5", "", "", MODULATION_SYSTEM_CCK_5M),
	TXPWR_TABLE_ENTRY("CCK11", "", "", MODULATION_SYSTEM_CCK_11M),
};

static struct txpwr_table_entry ofdm[] = {
	TXPWR_TABLE_ENTRY("OFDM6", "OFDM6", "", MODULATION_SYSTEM_OFDM_6M),
	TXPWR_TABLE_ENTRY("OFDM9", "OFDM9", "", MODULATION_SYSTEM_OFDM_9M),
	TXPWR_TABLE_ENTRY("OFDM12", "OFDM12", "", MODULATION_SYSTEM_OFDM_12M),
	TXPWR_TABLE_ENTRY("OFDM18", "OFDM18", "", MODULATION_SYSTEM_OFDM_18M),
	TXPWR_TABLE_ENTRY("OFDM24", "OFDM24", "", MODULATION_SYSTEM_OFDM_24M),
	TXPWR_TABLE_ENTRY("OFDM36", "OFDM36", "", MODULATION_SYSTEM_OFDM_36M),
	TXPWR_TABLE_ENTRY("OFDM48", "OFDM48", "", MODULATION_SYSTEM_OFDM_48M),
	TXPWR_TABLE_ENTRY("OFDM54", "OFDM54", "", MODULATION_SYSTEM_OFDM_54M),
};

static struct txpwr_table_entry ht20[] = {
	TXPWR_TABLE_ENTRY("MCS0", "MCS0", "MCS8", MODULATION_SYSTEM_HT20_MCS0),
	TXPWR_TABLE_ENTRY("MCS1", "MCS1", "MCS9", MODULATION_SYSTEM_HT20_MCS1),
	TXPWR_TABLE_ENTRY("MCS2", "MCS2", "MCS10", MODULATION_SYSTEM_HT20_MCS2),
	TXPWR_TABLE_ENTRY("MCS3", "MCS3", "MCS11", MODULATION_SYSTEM_HT20_MCS3),
	TXPWR_TABLE_ENTRY("MCS4", "MCS4", "MCS12", MODULATION_SYSTEM_HT20_MCS4),
	TXPWR_TABLE_ENTRY("MCS5", "MCS5", "MCS13", MODULATION_SYSTEM_HT20_MCS5),
	TXPWR_TABLE_ENTRY("MCS6", "MCS6", "MCS14", MODULATION_SYSTEM_HT20_MCS6),
	TXPWR_TABLE_ENTRY("MCS7", "MCS7", "MCS15", MODULATION_SYSTEM_HT20_MCS7),
};
static struct txpwr_table_entry ht40[] = {
	TXPWR_TABLE_ENTRY("MCS0", "MCS0", "MCS8", MODULATION_SYSTEM_HT40_MCS0),
	TXPWR_TABLE_ENTRY("MCS1", "MCS1", "MCS9", MODULATION_SYSTEM_HT40_MCS1),
	TXPWR_TABLE_ENTRY("MCS2", "MCS2", "MCS10", MODULATION_SYSTEM_HT40_MCS2),
	TXPWR_TABLE_ENTRY("MCS3", "MCS3", "MCS11", MODULATION_SYSTEM_HT40_MCS3),
	TXPWR_TABLE_ENTRY("MCS4", "MCS4", "MCS12", MODULATION_SYSTEM_HT40_MCS4),
	TXPWR_TABLE_ENTRY("MCS5", "MCS5", "MCS13", MODULATION_SYSTEM_HT40_MCS5),
	TXPWR_TABLE_ENTRY("MCS6", "MCS6", "MCS14", MODULATION_SYSTEM_HT40_MCS6),
	TXPWR_TABLE_ENTRY("MCS7", "MCS7", "MCS15", MODULATION_SYSTEM_HT40_MCS7),
	TXPWR_TABLE_ENTRY("MCS32", "MCS32", "MCS32",
		MODULATION_SYSTEM_HT40_MCS32),
};
static struct txpwr_table_entry vht[] = {
	TXPWR_TABLE_ENTRY("MCS0", "MCS0", "MCS0", MODULATION_SYSTEM_VHT20_MCS0),
	TXPWR_TABLE_ENTRY("MCS1", "MCS1", "MCS1", MODULATION_SYSTEM_VHT20_MCS1),
	TXPWR_TABLE_ENTRY("MCS2", "MCS2", "MCS2", MODULATION_SYSTEM_VHT20_MCS2),
	TXPWR_TABLE_ENTRY("MCS3", "MCS3", "MCS3", MODULATION_SYSTEM_VHT20_MCS3),
	TXPWR_TABLE_ENTRY("MCS4", "MCS4", "MCS4", MODULATION_SYSTEM_VHT20_MCS4),
	TXPWR_TABLE_ENTRY("MCS5", "MCS5", "MCS5", MODULATION_SYSTEM_VHT20_MCS5),
	TXPWR_TABLE_ENTRY("MCS6", "MCS6", "MCS6", MODULATION_SYSTEM_VHT20_MCS6),
	TXPWR_TABLE_ENTRY("MCS7", "MCS7", "MCS7", MODULATION_SYSTEM_VHT20_MCS7),
	TXPWR_TABLE_ENTRY("MCS8", "MCS8", "MCS8", MODULATION_SYSTEM_VHT20_MCS8),
	TXPWR_TABLE_ENTRY("MCS9", "MCS9", "MCS9", MODULATION_SYSTEM_VHT20_MCS9),
};

static struct txpwr_table txpwr_tables[] = {
	{"Legacy", dsss, ARRAY_SIZE(dsss)},
	{"11g", ofdm, ARRAY_SIZE(ofdm)},
	{"11a", ofdm, ARRAY_SIZE(ofdm)},
	{"HT20", ht20, ARRAY_SIZE(ht20)},
	{"HT40", ht40, ARRAY_SIZE(ht40)},
	{"VHT20", vht, ARRAY_SIZE(vht)},
	{"VHT40", vht, ARRAY_SIZE(vht)},
	{"VHT80", vht, ARRAY_SIZE(vht)},
	{"VHT160", vht, ARRAY_SIZE(vht)},
};

#define TMP_SZ (512)
#define CDD_PWR_OFFSET (6)
#define TXPWR_DUMP_SZ (8192)
void print_txpwr_tbl(struct txpwr_table *txpwr_tbl, unsigned char ch,
		     unsigned char *tx_pwr[], char pwr_offset[],
		     char *stream_buf[], unsigned int stream_pos[])
{
	struct txpwr_table_entry *tmp_tbl = txpwr_tbl->tables;
	unsigned int idx, pwr_idx, stream_idx;
	char pwr[TXPWR_TBL_NUM] = {0}, tmp_pwr = 0;
	char prefix[5], tmp[4];
	char *buf = NULL;
	unsigned int *pos = NULL;
	int i;

	for (i = 0; i < txpwr_tbl->n_tables; i++) {
		idx = tmp_tbl[i].idx;

		for (pwr_idx = 0; pwr_idx < TXPWR_TBL_NUM; pwr_idx++) {
			if (!tx_pwr[pwr_idx]) {
				DBGLOG(REQ, WARN,
				       "Power table[%d] is NULL\n", pwr_idx);
				return;
			}
			pwr[pwr_idx] = tx_pwr[pwr_idx][idx] +
				       pwr_offset[pwr_idx];
			pwr[pwr_idx] = (pwr[pwr_idx] > MAX_TX_POWER) ?
				       MAX_TX_POWER : pwr[pwr_idx];
		}

		for (stream_idx = 0; stream_idx < STREAM_NUM; stream_idx++) {
			buf = stream_buf[stream_idx];
			pos = &stream_pos[stream_idx];

			if (tmp_tbl[i].mcs[stream_idx][0] == '\0')
				continue;

			switch (stream_idx) {
			case STREAM_SISO:
				kalStrnCpy(prefix, "siso", sizeof(prefix));
				break;
			case STREAM_CDD:
				kalStrnCpy(prefix, "cdd", sizeof(prefix));
				break;
			case STREAM_MIMO:
				kalStrnCpy(prefix, "mimo", sizeof(prefix));
				break;
			}

			*pos += kalScnprintf(buf + *pos, TMP_SZ - *pos,
					     "%s, %d, %s, %s, ",
					     prefix, ch,
					     txpwr_tbl->phy_mode,
					     tmp_tbl[i].mcs[stream_idx]);

			for (pwr_idx = 0; pwr_idx < TXPWR_TBL_NUM; pwr_idx++) {
				tmp_pwr = pwr[pwr_idx];

				tmp_pwr = (tmp_pwr > 0) ? tmp_pwr : 0;

				if (pwr_idx + 1 == TXPWR_TBL_NUM)
					kalStrnCpy(tmp, "\n", sizeof(tmp));
				else
					kalStrnCpy(tmp, ", ", sizeof(tmp));
				*pos += kalScnprintf(buf + *pos, TMP_SZ - *pos,
						     "%d.%d%s",
						     tmp_pwr / 2,
						     tmp_pwr % 2 * 5,
						     tmp);

			}
		}
	}
}

char *g_txpwr_tbl_read_buffer;
char *g_txpwr_tbl_read_buffer_head;
unsigned int g_txpwr_tbl_read_residual;

static ssize_t procGetTxpwrTblRead(struct file *filp, char __user *buf,
				   size_t count, loff_t *f_pos)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER  *prAdapter = NULL;
	struct BSS_INFO *prBssInfo = NULL;
	unsigned char ucBssIndex;
	struct NETDEV_PRIVATE_GLUE_INFO *prNetDevPrivate = NULL;
	uint32_t status;
	struct PARAM_CMD_GET_TXPWR_TBL pwr_tbl;
	struct POWER_LIMIT *tx_pwr_tbl = pwr_tbl.tx_pwr_tbl;
	char *buffer;
	unsigned int pos = 0, buf_len = TXPWR_DUMP_SZ, oid_len;
	unsigned char i, j;
	char *stream_buf[STREAM_NUM] = {NULL};
	unsigned int stream_pos[STREAM_NUM] = {0};
	unsigned char *tx_pwr[TXPWR_TBL_NUM] =  {NULL};
	char pwr_offset[TXPWR_TBL_NUM] = {0};
	int ret;

	if (*f_pos > 0) { /* re-entry */
		pos = g_txpwr_tbl_read_residual;
		buffer = g_txpwr_tbl_read_buffer;
		goto next_entry;
	}

	prGlueInfo = g_prGlueInfo_proc;
	if (!prGlueInfo)
		return -EFAULT;

	prAdapter = prGlueInfo->prAdapter;
	prNetDevPrivate =
		(struct NETDEV_PRIVATE_GLUE_INFO *) netdev_priv(gPrDev);

	if (prNetDevPrivate->prGlueInfo != prGlueInfo)
		return -EFAULT;
	ucBssIndex = prNetDevPrivate->ucBssIdx;
	prBssInfo = prAdapter->aprBssInfo[ucBssIndex];
	if (!prBssInfo)
		return -EFAULT;

	kalMemZero(&pwr_tbl, sizeof(pwr_tbl));
    /* MT7663 no DBDC design */
#if  0
	if (prAdapter->rWifiVar.fgDbDcModeEn)
		pwr_tbl.ucDbdcIdx = prBssInfo->eDBDCBand;
	else
		pwr_tbl.ucDbdcIdx = ENUM_BAND_0;
#endif
	status = kalIoctl(prGlueInfo,
			  wlanoidGetTxPwrTbl,
			  &pwr_tbl,
			  sizeof(pwr_tbl), TRUE, FALSE, TRUE, &oid_len);

	if (status != WLAN_STATUS_SUCCESS) {
		DBGLOG(REQ, WARN, "Query Tx Power Table fail\n");
		return -EINVAL;
	}

	buffer = (char *) kalMemAlloc(buf_len, VIR_MEM_TYPE);
	if (!buffer)
		return -ENOMEM;

	g_txpwr_tbl_read_buffer = buffer;
	g_txpwr_tbl_read_buffer_head = buffer;

	for (i = 0; i < STREAM_NUM; i++) {
		stream_buf[i] = (char *) kalMemAlloc(TMP_SZ, VIR_MEM_TYPE);
		if (!stream_buf[i]) {
			ret = -ENOMEM;
			goto out;
		}
	}

	pos = kalScnprintf(buffer, buf_len,
			   "\n%s",
			   "spatial stream, Channel, bw, modulation, ");
	pos += kalScnprintf(buffer + pos, buf_len - pos,
			   "%s\n",
			   "regulatory limit, board limit, target power");

	for (i = 0; i < ARRAY_SIZE(txpwr_tables); i++) {
		for (j = 0; j < STREAM_NUM; j++) {
			kalMemZero(stream_buf[j], TMP_SZ);
			stream_pos[j] = 0;
		}

		for (j = 0; j < TXPWR_TBL_NUM; j++) {
			tx_pwr[j] = NULL;
			pwr_offset[j] = 0;
		}

		switch (i) {
		case DSSS:
			if (pwr_tbl.ucCenterCh > 14)
				continue;
			for (j = 0; j < TXPWR_TBL_NUM; j++)
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_dsss;
			break;
		case OFDM_24G:
			if (pwr_tbl.ucCenterCh > 14)
				continue;
			for (j = 0; j < TXPWR_TBL_NUM; j++)
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_ofdm;
			break;
		case OFDM_5G:
			if (pwr_tbl.ucCenterCh <= 14)
				continue;
			for (j = 0; j < TXPWR_TBL_NUM; j++)
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_ofdm;
			break;
		case HT20:
			for (j = 0; j < TXPWR_TBL_NUM; j++)
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_ht20;
			break;
		case HT40:
			for (j = 0; j < TXPWR_TBL_NUM; j++)
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_ht40;
			break;
		case VHT20:
			if (pwr_tbl.ucCenterCh <= 14)
				continue;
			for (j = 0; j < TXPWR_TBL_NUM; j++)
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_vht20;
			break;
#if 0
		case VHT40:
		case VHT80:
			if (pwr_tbl.ucCenterCh <= 14)
				continue;
			offset = (i == VHT40) ?
				 PWR_Vht40_OFFSET : PWR_Vht80_OFFSET;
			for (j = 0; j < TXPWR_TBL_NUM; j++) {
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_vht20;
				pwr_offset[j] =
					tx_pwr_tbl[j].tx_pwr_vht_OFST[offset];
				/* Covert 7bit 2'complement value to 8bit */
				pwr_offset[j] |= (pwr_offset[j] & BIT(6)) ?
						 BIT(7) : 0;
			}
			break;
#else
		case VHT40:
			if (pwr_tbl.ucCenterCh <= 14)
				continue;
			for (j = 0; j < TXPWR_TBL_NUM; j++)
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_vht40;
			break;
		case VHT80:
			if (pwr_tbl.ucCenterCh <= 14)
				continue;
			for (j = 0; j < TXPWR_TBL_NUM; j++)
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_vht80;
			break;
		case VHT160:
			if (pwr_tbl.ucCenterCh <= 14)
				continue;
			for (j = 0; j < TXPWR_TBL_NUM; j++)
				tx_pwr[j] = tx_pwr_tbl[j].tx_pwr_vht160;
			break;
#endif
		default:
			break;
		}

		print_txpwr_tbl(&txpwr_tables[i], pwr_tbl.ucCenterCh,
				 tx_pwr, pwr_offset,
				 stream_buf, stream_pos);

		for (j = 0; j < STREAM_NUM; j++) {
			pos += kalScnprintf(buffer + pos, buf_len - pos,
					    "%s",
					    stream_buf[j]);
		}
	}

	g_txpwr_tbl_read_residual = pos;

next_entry:
	if (pos > count)
		pos = count;

	if (copy_to_user(buf, buffer, pos)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		ret = -EFAULT;
		goto out;
	}
	g_txpwr_tbl_read_buffer += pos;
	g_txpwr_tbl_read_residual -= pos;

	*f_pos += pos;
	ret = pos;
out:
	if (ret == 0 || ret == -ENOMEM) {
		for (i = 0; i < STREAM_NUM; i++) {
			if (stream_buf[i])
				kalMemFree(stream_buf[i], VIR_MEM_TYPE, TMP_SZ);
		}
		if (g_txpwr_tbl_read_buffer_head)
			kalMemFree(g_txpwr_tbl_read_buffer_head,
				VIR_MEM_TYPE, buf_len);
		g_txpwr_tbl_read_buffer = NULL;
		g_txpwr_tbl_read_buffer_head = NULL;
		g_txpwr_tbl_read_residual = 0;
	}

	return ret;
}

#ifdef CFG_GET_TEMPURATURE
static ssize_t proc_get_temperature(struct file *filp,
				    char __user *buf,
				    size_t count,
				    loff_t *f_pos)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	unsigned int pos = 0, buf_len = 128, oid_len;
	char *buffer;
	int temperature = 0;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	if (*f_pos > 0)
		return 0;

	prGlueInfo = g_prGlueInfo_proc;
	if (!prGlueInfo)
		return -EFAULT;

	buffer = (char *) kalMemAlloc(buf_len, VIR_MEM_TYPE);
	if (!buffer)
		return -ENOMEM;

	rStatus = kalIoctl(prGlueInfo,
			   wlanoidGetTemperature, &temperature,
			   sizeof(temperature), TRUE, TRUE, TRUE, &oid_len);

	pos = kalScnprintf(buffer, buf_len, "Temperature = %d\n", temperature);

	if (copy_to_user(buf, buffer, pos)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		kalMemFree(buffer, VIR_MEM_TYPE, buf_len);
		return -EFAULT;
	}

	*f_pos += pos;

	kalMemFree(buffer, VIR_MEM_TYPE, buf_len);
	return pos;
}
#endif

#if CFG_DISCONN_DEBUG_FEATURE
static int32_t parseTxRateInfo(IN char *pcBuffer, IN int i4Size,
	IN struct TX_VECTOR_BBP_LATCH *prTxV)
{
	uint8_t rate, txmode, frmode, sgi, ldpc, nsts, stbc;
	int8_t txpwr;
	int32_t i4BytesWritten = 0;

	rate = TX_VECTOR_GET_TX_RATE(prTxV);
	txmode = TX_VECTOR_GET_TX_MODE(prTxV);
	frmode = TX_VECTOR_GET_TX_FRMODE(prTxV);
	nsts = TX_VECTOR_GET_TX_NSTS(prTxV) + 1;
	sgi = TX_VECTOR_GET_TX_SGI(prTxV);
	ldpc = TX_VECTOR_GET_TX_LDPC(prTxV);
	stbc = TX_VECTOR_GET_TX_STBC(prTxV);
	txpwr = TX_VECTOR_GET_TX_PWR(prTxV);

	if (prTxV->u4TxVector1 == 0xFFFFFFFF) {
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten,
			"%-26s%s%s\n", "Last TX Rate", " = ", "N/A");
	} else {
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten,
			"%-26s%s", "Last TX Rate", " = ");

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"%s, ", rate < 4 ? HW_TX_RATE_CCK_STR[rate] :
					HW_TX_RATE_CCK_STR[4]);
		else if (txmode == TX_RATE_MODE_OFDM)
			i4BytesWritten += kalScnprintf(
				pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"%s, ", hw_rate_ofdm_str(rate));
		else if ((txmode == TX_RATE_MODE_HTMIX) ||
			 (txmode == TX_RATE_MODE_HTGF))
			i4BytesWritten += kalScnprintf(
				pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"MCS%d, ", rate);
		else
			i4BytesWritten += kalScnprintf(
				pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"NSS%d_MCS%d, ", nsts, rate);

		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%s, ",
			frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

		if (txmode == TX_RATE_MODE_CCK)
			i4BytesWritten += kalScnprintf(
				pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"%s, ", rate < 4 ? "LP" : "SP");
		else if (txmode == TX_RATE_MODE_OFDM)
			;
		else
			i4BytesWritten += kalScnprintf(
				pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"%s, ", sgi == 0 ? "LGI" : "SGI");

		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%s%s%s\n",
			txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			stbc ? ", STBC, " : ", ", ldpc == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;
}

static int32_t parseRxRateInfo(IN char *pcBuffer, IN int i4Size,
	IN struct STA_RECORD *prStaRec)
{
	uint32_t txmode, rate, frmode, sgi, nsts, ldpc, stbc, groupid, mu;
	int32_t i4BytesWritten = 0;
	uint32_t u4RxVector0 = 0, u4RxVector1 = 0;

	u4RxVector0 = prStaRec->u4RxVector0;
	u4RxVector1 = prStaRec->u4RxVector1;

	txmode = (u4RxVector0 & RX_VT_RX_MODE_MASK) >> RX_VT_RX_MODE_OFFSET;
	rate = (u4RxVector0 & RX_VT_RX_RATE_MASK) >> RX_VT_RX_RATE_OFFSET;
	frmode = (u4RxVector0 & RX_VT_FR_MODE_MASK) >> RX_VT_FR_MODE_OFFSET;
	nsts = ((u4RxVector1 & RX_VT_NSTS_MASK) >> RX_VT_NSTS_OFFSET);
	stbc = (u4RxVector0 & RX_VT_STBC_MASK) >> RX_VT_STBC_OFFSET;
	sgi = u4RxVector0 & RX_VT_SHORT_GI;
	ldpc = u4RxVector0 & RX_VT_LDPC;
	groupid = (u4RxVector1 & RX_VT_GROUP_ID_MASK) >> RX_VT_GROUP_ID_OFFSET;

	if (groupid && groupid != 63) {
		mu = 1;
	} else {
		mu = 0;
		nsts += 1;
	}

	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
		i4Size - i4BytesWritten, "%-26s%s", "Last RX Rate", " = ");

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%s, ",
			rate < 4 ? HW_TX_RATE_CCK_STR[rate] :
				HW_TX_RATE_CCK_STR[4]);
	else if (txmode == TX_RATE_MODE_OFDM)
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%s, ",
			hw_rate_ofdm_str(rate));
	else if ((txmode == TX_RATE_MODE_HTMIX) ||
		(txmode == TX_RATE_MODE_HTGF))
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "MCS%d, ", rate);
	else
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "NSS%d_MCS%d, ",
			nsts, rate);

	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
		i4Size - i4BytesWritten, "%s, ",
		frmode < 4 ? HW_TX_RATE_BW[frmode] : HW_TX_RATE_BW[4]);

	if (txmode == TX_RATE_MODE_CCK)
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%s, ",
			rate < 4 ? "LP" : "SP");
	else if (txmode == TX_RATE_MODE_OFDM)
		;
	else
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%s, ",
			sgi == 0 ? "LGI" : "SGI");

	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
		i4Size - i4BytesWritten, "%s", stbc == 0 ? "" : "STBC, ");

	if (mu) {
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%s, %s, %s (%d)\n",
			txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			ldpc == 0 ? "BCC" : "LDPC", "MU", groupid);
	} else {
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%s, %s\n",
			txmode < 5 ? HW_TX_MODE_STR[txmode] : HW_TX_MODE_STR[5],
			ldpc == 0 ? "BCC" : "LDPC");
	}

	return i4BytesWritten;

}

static int32_t parseRxRssiInfo(IN char *pcBuffer, IN int i4Size,
	IN struct ADAPTER *prAdapter, IN struct STA_RECORD *prStaRec)
{
	int32_t i4RSSI0 = 0, i4RSSI1 = 0, i4RSSI2 = 0, i4RSSI3 = 0;
	int32_t i4BytesWritten = 0;
	uint32_t u4RxVector3 = 0;

	u4RxVector3 = prStaRec->u4RxVector3;

	i4RSSI0 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI0_MASK) >>
						RX_VT_RCPI0_OFFSET);
	i4RSSI1 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI1_MASK) >>
						RX_VT_RCPI1_OFFSET);

	if (prAdapter->rWifiVar.ucNSS > 2) {
		i4RSSI2 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI2_MASK) >>
						RX_VT_RCPI2_OFFSET);
		i4RSSI3 = RCPI_TO_dBm((u4RxVector3 & RX_VT_RCPI3_MASK) >>
						RX_VT_RCPI3_OFFSET);

		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%-26s%s%d %d %d %d\n",
			"Last RX Data RSSI", " = ",
			i4RSSI0, i4RSSI1, i4RSSI2, i4RSSI3);
	} else
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten, "%-26s%s%d %d\n",
			"Last RX Data RSSI", " = ", i4RSSI0, i4RSSI1);

	return i4BytesWritten;

}

static uint32_t parseRxRespRssiInfo(IN char *pcBuffer, IN int i4Size,
	IN struct ADAPTER *prAdapter,
	IN struct PARAM_HW_WLAN_INFO *prHwWlanInfo)
{
	int32_t i4BytesWritten = 0;

	if (prAdapter->rWifiVar.ucNSS > 2)
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten,
			"%-26s%s%d %d %d %d\n", "Tx Response RSSI", " = ",
			RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi0),
			RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi1),
			RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi2),
			RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi3));
	else
		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
			i4Size - i4BytesWritten,
			"%-26s%s%d %d\n", "Tx Response RSSI", " = ",
			RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi0),
			RCPI_TO_dBm(prHwWlanInfo->rWtblRxCounter.ucRxRcpi1));

	return i4BytesWritten;
}

static int32_t parseTxPerInfo(IN char *pcBuffer, IN int i4Size,
	IN struct PARAM_HW_WLAN_INFO *prHwWlanInfo,
	IN struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics)
{
	uint32_t u4InstantPer;
	int32_t i4BytesWritten = 0;
	uint8_t ucSkipAr;

	ucSkipAr = prQueryStaStatistics->ucSkipAr;

	if (ucSkipAr) {
		u4InstantPer =
			(prHwWlanInfo->rWtblTxCounter.u2Rate1TxCnt == 0) ?
			(0) :
			(1000 * (prHwWlanInfo->rWtblTxCounter.u2Rate1FailCnt)
			/ (prHwWlanInfo->rWtblTxCounter.u2Rate1TxCnt));

		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"%-26s%s%d.%1d%%\n", "instant PER", " = ",
				u4InstantPer/10, u4InstantPer%10);
	} else {
		u4InstantPer = (prQueryStaStatistics->ucPer == 0) ?
				(0) : (prQueryStaStatistics->ucPer);

		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"%-26s%s%d%%\n", "instant PER", " = ",
				u4InstantPer);
	}

	return i4BytesWritten;
}

static int32_t parseRxPerInfo(IN char *pcBuffer, IN int i4Size,
	IN struct ADAPTER *prAdapter,
	IN struct PARAM_GET_STA_STATISTICS *prQueryStaStatistics)
{
	uint32_t u4InstantPer[ENUM_BAND_NUM];
	int32_t i4BytesWritten = 0;
	uint8_t ucDbdcIdx;
	struct MIB_INFO_STAT *prMibInfo;

	prMibInfo = prQueryStaStatistics->rMibInfo;

	for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {
		u4InstantPer[ucDbdcIdx] = ((prMibInfo[ucDbdcIdx].u4RxMpduCnt +
				prMibInfo[ucDbdcIdx].u4FcsError) == 0) ?
				(0) : (1000 * prMibInfo[ucDbdcIdx].u4FcsError /
				(prMibInfo[ucDbdcIdx].u4RxMpduCnt +
				prMibInfo[ucDbdcIdx].u4FcsError));

		if (prAdapter->rWifiVar.fgDbDcModeEn)
			i4BytesWritten += kalScnprintf(
				pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"[DBDC_%d] :\n",
				ucDbdcIdx);

		i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"%-26s%s%d.%1d%%\n", "instant PER", " = ",
				u4InstantPer[ENUM_BAND_0]/10,
				u4InstantPer[ENUM_BAND_0]%10);

		if (!prAdapter->rWifiVar.fgDbDcModeEn)
			break;

	}

	return i4BytesWritten;
}

static uint32_t parseTrigger(IN char *pcBuffer,
	IN int i4Size, IN uint8_t trigger)
{
	int32_t i4BytesWritten = 0;
	char *pString;

	switch (trigger) {
	case DISCONNECT_TRIGGER_RESERVED:
		pString = GET_VARNAME(DISCONNECT_TRIGGER_RESERVED);
		break;
	case DISCONNECT_TRIGGER_ACTIVE:
		pString = GET_VARNAME(DISCONNECT_TRIGGER_ACTIVE);
		break;
	case DISCONNECT_TRIGGER_PASSIVE:
		pString = GET_VARNAME(DISCONNECT_TRIGGER_PASSIVE);
		break;
	default:
		pString = "N/A";
		break;
	}

	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
					i4Size - i4BytesWritten,
					"%-26s%s%s\n",
					"Trigger",
					" = ",
					pString);

	return i4BytesWritten;
}

static uint32_t parseDisconnReasonCode(IN char *pcBuffer,
	IN int i4Size, IN uint8_t ucReason)
{
	int32_t i4BytesWritten = 0;
	char *pString;

	switch (ucReason) {
	case DISCONNECT_REASON_CODE_RESERVED:
		pString = GET_VARNAME(DISCONNECT_REASON_CODE_RESERVED);
		break;
	case DISCONNECT_REASON_CODE_RADIO_LOST:
		pString = GET_VARNAME(DISCONNECT_REASON_CODE_RADIO_LOST);
		break;
	case DISCONNECT_REASON_CODE_DEAUTHENTICATED:
		pString = GET_VARNAME(DISCONNECT_REASON_CODE_DEAUTHENTICATED);
		break;
	case DISCONNECT_REASON_CODE_DISASSOCIATED:
		pString = GET_VARNAME(DISCONNECT_REASON_CODE_DISASSOCIATED);
		break;
	case DISCONNECT_REASON_CODE_NEW_CONNECTION:
		pString = GET_VARNAME(DISCONNECT_REASON_CODE_NEW_CONNECTION);
		break;
	case DISCONNECT_REASON_CODE_REASSOCIATION:
		pString = GET_VARNAME(DISCONNECT_REASON_CODE_REASSOCIATION);
		break;
	case DISCONNECT_REASON_CODE_ROAMING:
		pString = GET_VARNAME(DISCONNECT_REASON_CODE_ROAMING);
		break;
	default:
		pString = "N/A";
		break;
	}

	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
					i4Size - i4BytesWritten,
					"%-26s%s%s\n",
					"Disconnect reason",
					" = ",
					pString);

	return i4BytesWritten;
}

static uint32_t parseBcnTimeoutReasonCode(IN char *pcBuffer,
	IN int i4Size, IN uint8_t ucReason)
{
	int32_t i4BytesWritten = 0;
	char *pString;

	switch (ucReason) {
	case BEACON_TIMEOUT_DUE_2_HW_BEACON_LOST_NONADHOC:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_HW_BEACON_LOST_NONADHOC);
		break;
	case BEACON_TIMEOUT_DUE_2_HW_BEACON_LOST_ADHOC:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_HW_BEACON_LOST_ADHOC);
		break;
	case BEACON_TIMEOUT_DUE_2_HW_TSF_DRIFT:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_HW_TSF_DRIFT);
		break;
	case BEACON_TIMEOUT_DUE_2_NULL_FRAME_THRESHOLD:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_NULL_FRAME_THRESHOLD);
		break;
	case BEACON_TIMEOUT_DUE_2_AGING_THRESHOLD:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_AGING_THRESHOLD);
		break;
	case BEACON_TIMEOUT_DUE_2_BSSID_BEACON_PEIROD_NOT_ILLIGAL:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_BSSID_BEACON_PEIROD_NOT_ILLIGAL);
		break;
	case BEACON_TIMEOUT_DUE_2_CONNECTION_FAIL:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_CONNECTION_FAIL);
		break;
	case BEACON_TIMEOUT_DUE_2_ALLOCAT_NULL_PKT_FAIL_THRESHOLD:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_ALLOCAT_NULL_PKT_FAIL_THRESHOLD);
		break;
	case BEACON_TIMEOUT_DUE_2_NO_TX_DONE_EVENT:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_NO_TX_DONE_EVENT);
		break;
	case BEACON_TIMEOUT_DUE_2_UNSPECIF_REASON:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_UNSPECIF_REASON);
		break;
	case BEACON_TIMEOUT_DUE_2_SET_CHIP:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_SET_CHIP);
		break;
	case BEACON_TIMEOUT_DUE_2_KEEP_SCAN_AP_MISS_CHECK_FAIL:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_KEEP_SCAN_AP_MISS_CHECK_FAIL);
		break;
	case BEACON_TIMEOUT_DUE_2_KEEP_UNCHANGED_LOW_RSSI_CHECK_FAIL:
		pString = GET_VARNAME(
		BEACON_TIMEOUT_DUE_2_KEEP_UNCHANGED_LOW_RSSI_CHECK_FAIL);
		break;
	case BEACON_TIMEOUT_DUE_2_NULL_FRAME_LIFE_TIMEOUT:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_NULL_FRAME_LIFE_TIMEOUT);
		break;
	case BEACON_TIMEOUT_DUE_2_APR_NO_RESPONSE:
		pString = GET_VARNAME(
			BEACON_TIMEOUT_DUE_2_APR_NO_RESPONSE);
		break;
	default:
		pString = "N/A";
		break;
	}

	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
					i4Size - i4BytesWritten,
					"%-26s%s%s\n",
					"Beacon timeout reason",
					" = ",
					pString);

	return i4BytesWritten;
}

static uint32_t parseDisassocReasonCode(IN char *pcBuffer,
	IN int i4Size, IN uint8_t ucReason)
{
	int32_t i4BytesWritten = 0;
	char *pString;

	switch (ucReason) {
	case REASON_CODE_RESERVED:
		pString = GET_VARNAME(
			REASON_CODE_RESERVED);
		break;
	case REASON_CODE_UNSPECIFIED:
		pString = GET_VARNAME(
			REASON_CODE_UNSPECIFIED);
		break;
	case REASON_CODE_PREV_AUTH_INVALID:
		pString = GET_VARNAME(
			REASON_CODE_PREV_AUTH_INVALID);
		break;
	case REASON_CODE_DEAUTH_LEAVING_BSS:
		pString = GET_VARNAME(
			REASON_CODE_DEAUTH_LEAVING_BSS);
		break;
	case REASON_CODE_DISASSOC_INACTIVITY:
		pString = GET_VARNAME(
			REASON_CODE_DISASSOC_INACTIVITY);
		break;
	case REASON_CODE_DISASSOC_AP_OVERLOAD:
		pString = GET_VARNAME(
			REASON_CODE_DISASSOC_AP_OVERLOAD);
		break;
	case REASON_CODE_CLASS_2_ERR:
		pString = GET_VARNAME(
			REASON_CODE_CLASS_2_ERR);
		break;
	case REASON_CODE_CLASS_3_ERR:
		pString = GET_VARNAME(
			REASON_CODE_CLASS_3_ERR);
		break;
	case REASON_CODE_DISASSOC_LEAVING_BSS:
		pString = GET_VARNAME(
			REASON_CODE_DISASSOC_LEAVING_BSS);
		break;
	case REASON_CODE_ASSOC_BEFORE_AUTH:
		pString = GET_VARNAME(
			REASON_CODE_ASSOC_BEFORE_AUTH);
		break;
	case REASON_CODE_DISASSOC_PWR_CAP_UNACCEPTABLE:
		pString = GET_VARNAME(
			REASON_CODE_DISASSOC_PWR_CAP_UNACCEPTABLE);
		break;
	case REASON_CODE_DISASSOC_SUP_CHS_UNACCEPTABLE:
		pString = GET_VARNAME(
			REASON_CODE_DISASSOC_SUP_CHS_UNACCEPTABLE);
		break;
	case REASON_CODE_INVALID_INFO_ELEM:
		pString = GET_VARNAME(
			REASON_CODE_INVALID_INFO_ELEM);
		break;
	case REASON_CODE_MIC_FAILURE:
		pString = GET_VARNAME(
			REASON_CODE_MIC_FAILURE);
		break;
	case REASON_CODE_4_WAY_HANDSHAKE_TIMEOUT:
		pString = GET_VARNAME(
			REASON_CODE_4_WAY_HANDSHAKE_TIMEOUT);
		break;
	case REASON_CODE_GROUP_KEY_UPDATE_TIMEOUT:
		pString = GET_VARNAME(
			REASON_CODE_GROUP_KEY_UPDATE_TIMEOUT);
		break;
	case REASON_CODE_DIFFERENT_INFO_ELEM:
		pString = GET_VARNAME(
			REASON_CODE_DIFFERENT_INFO_ELEM);
		break;
	case REASON_CODE_MULTICAST_CIPHER_NOT_VALID:
		pString = GET_VARNAME(
			REASON_CODE_MULTICAST_CIPHER_NOT_VALID);
		break;
	case REASON_CODE_UNICAST_CIPHER_NOT_VALID:
		pString = GET_VARNAME(
			REASON_CODE_UNICAST_CIPHER_NOT_VALID);
		break;
	case REASON_CODE_AKMP_NOT_VALID:
		pString = GET_VARNAME(
			REASON_CODE_AKMP_NOT_VALID);
		break;
	case REASON_CODE_UNSUPPORTED_RSNE_VERSION:
		pString = GET_VARNAME(
			REASON_CODE_UNSUPPORTED_RSNE_VERSION);
		break;
	case REASON_CODE_INVALID_RSNE_CAPABILITIES:
		pString = GET_VARNAME(
			REASON_CODE_INVALID_RSNE_CAPABILITIES);
		break;
	case REASON_CODE_IEEE_802_1X_AUTH_FAILED:
		pString = GET_VARNAME(
			REASON_CODE_IEEE_802_1X_AUTH_FAILED);
		break;
	case REASON_CODE_CIPHER_REJECT_SEC_POLICY:
		pString = GET_VARNAME(
			REASON_CODE_CIPHER_REJECT_SEC_POLICY);
		break;
	case REASON_CODE_DISASSOC_UNSPECIFIED_QOS:
		pString = GET_VARNAME(
			REASON_CODE_DISASSOC_UNSPECIFIED_QOS);
		break;
	case REASON_CODE_DISASSOC_LACK_OF_BANDWIDTH:
		pString = GET_VARNAME(
			REASON_CODE_DISASSOC_LACK_OF_BANDWIDTH);
		break;
	case REASON_CODE_DISASSOC_ACK_LOST_POOR_CHANNEL:
		pString = GET_VARNAME(
			REASON_CODE_DISASSOC_ACK_LOST_POOR_CHANNEL);
		break;
	case REASON_CODE_DISASSOC_TX_OUTSIDE_TXOP_LIMIT:
		pString = GET_VARNAME(
			REASON_CODE_DISASSOC_TX_OUTSIDE_TXOP_LIMIT);
		break;
	case REASON_CODE_PEER_WHILE_LEAVING:
		pString = GET_VARNAME(
			REASON_CODE_PEER_WHILE_LEAVING);
		break;
	case REASON_CODE_PEER_REFUSE_DLP:
		pString = GET_VARNAME(
			REASON_CODE_PEER_REFUSE_DLP);
		break;
	case REASON_CODE_PEER_SETUP_REQUIRED:
		pString = GET_VARNAME(
			REASON_CODE_PEER_SETUP_REQUIRED);
		break;
	case REASON_CODE_PEER_TIME_OUT:
		pString = GET_VARNAME(
			REASON_CODE_PEER_TIME_OUT);
		break;
	case REASON_CODE_PEER_CIPHER_UNSUPPORTED:
		pString = GET_VARNAME(
			REASON_CODE_PEER_CIPHER_UNSUPPORTED);
		break;
	case REASON_CODE_BEACON_TIMEOUT:
		pString = GET_VARNAME(
			REASON_CODE_BEACON_TIMEOUT);
		break;
	default:
		pString = "N/A";
		break;
	}

	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
					i4Size - i4BytesWritten,
					"%-26s%s%s\n",
					"Disassociation reason",
					" = ",
					pString);

	return i4BytesWritten;

}

static uint32_t parseRssiInfo(IN char *pcBuffer, IN int i4Size,
	IN struct CMD_NOISE_HISTOGRAM_REPORT *prNoise)
{
	int32_t i4BytesWritten = 0;
	struct CMD_NOISE_HISTOGRAM_REPORT rEmptyNoise;

	kalMemZero(&rEmptyNoise, sizeof(rEmptyNoise));
	rEmptyNoise.u2Type = prNoise->u2Type;
	rEmptyNoise.u2Len = prNoise->u2Len;
	rEmptyNoise.ucAction = prNoise->ucAction;

	if (kalMemCmp(prNoise, &rEmptyNoise,
		sizeof(struct CMD_NOISE_HISTOGRAM_REPORT)) == 0) {
		DBGLOG(INIT, WARN, "Empty info\n");
		return i4BytesWritten;
	}

	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n%s", "----- Noise Info  -----");

	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n       Power > -55: %10d",
				prNoise->u4IPI10);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-55 >= Power > -60: %10d",
				prNoise->u4IPI9);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-60 >= Power > -65: %10d",
				prNoise->u4IPI8);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-65 >= Power > -70: %10d",
				prNoise->u4IPI7);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-70 >= Power > -75: %10d",
				prNoise->u4IPI6);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-75 >= Power > -80: %10d",
				prNoise->u4IPI5);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-80 >= Power > -83: %10d",
				prNoise->u4IPI4);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-83 >= Power > -86: %10d",
				prNoise->u4IPI3);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-86 >= Power > -89: %10d",
				prNoise->u4IPI2);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-89 >= Power > -92: %10d",
				prNoise->u4IPI1);
	i4BytesWritten += kalScnprintf(pcBuffer + i4BytesWritten,
				i4Size - i4BytesWritten,
				"\n-92 >= Power      : %10d\n",
				prNoise->u4IPI0);

	return i4BytesWritten;
}

static ssize_t procDisconnInfoRead(struct file *filp,
	char __user *buf, size_t count, loff_t *f_pos)
{
	struct GLUE_INFO *prGlueInfo;
	struct ADAPTER *prAdapter;
	int32_t i4Count = 0;
	uint8_t ucDbdcIdx;
	uint8_t *temp = &g_aucProcBuf[0];
	struct AIS_DISCONN_INFO_T *prDisconn = NULL;
	uint8_t cnt;
	uint8_t temp_idx = 0;
	struct tm broken;
	char date[20] = {0};

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	if (g_prDisconnInfo == NULL) {
		DBGLOG(INIT, WARN, "NULL g_prDisconnInfo\n");
		return 0;
	}

	if (g_DisconnInfoIdx >= MAX_DISCONNECT_RECORD) {
		DBGLOG(AIS, LOUD, "Invalid g_DisconnInfoIdx\n");
		return 0;
	}

	prGlueInfo = g_prGlueInfo_proc;
	prAdapter = prGlueInfo->prAdapter;

	kalMemZero(g_aucProcBuf, sizeof(g_aucProcBuf));

	for (cnt = 0; cnt < MAX_DISCONNECT_RECORD; cnt++) {

		temp_idx = (g_DisconnInfoIdx + cnt) % MAX_DISCONNECT_RECORD;

		prDisconn = g_prDisconnInfo + temp_idx;

		time_to_tm(prDisconn->tv.tv_sec, 0, &broken);
		kalScnprintf(date,
			sizeof(date),
			"%02d-%02d %02d:%02d:%02d.%ld",
			broken.tm_mon + 1,
			broken.tm_mday,
			broken.tm_hour,
			broken.tm_min,
			broken.tm_sec,
			prDisconn->tv.tv_usec);

		i4Count += kalScnprintf(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				"%s %s %d %s %s %s %s",
				"===============",
				"Record",
				temp_idx,
				"(",
				date,
				")",
				"===============");

		/* Dump misc info*/
		i4Count += kalScnprintf(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				"\n%s", "<Misc Info>\n");

		i4Count += parseTrigger(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				prDisconn->ucTrigger);

		i4Count += parseDisconnReasonCode(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				prDisconn->ucDisConnReason);

		i4Count += parseBcnTimeoutReasonCode(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				prDisconn->ucBcnTimeoutReason);

		i4Count += parseDisassocReasonCode(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				prDisconn->ucDisassocReason);

		if (prDisconn->u2DisassocSeqNum != 0xFFFF) {
			i4Count += kalScnprintf(temp + i4Count,
					sizeof(g_aucProcBuf) - i4Count,
					"%-26s%s%ld\n",
					"Disassociation SeqNum",
					" = ",
					prDisconn->u2DisassocSeqNum);
		}

		/* Noise info*/
		i4Count += parseRssiInfo(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				&prDisconn->rNoise);

		/* Dump RX info */
		i4Count += kalScnprintf(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				"\n%s", "<Last Rx Info>\n");

		/* Last RX Rate info */
		i4Count += parseRxRateInfo(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				&prDisconn->rStaRec);

		/* RX PER info */
		i4Count += parseRxPerInfo(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				prAdapter,
				&prDisconn->rStaStatistics);

		/* Last RX RSSI info*/
		i4Count += parseRxRssiInfo(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				prAdapter,
				&prDisconn->rStaRec);

		/* Last RX Resp RSSI */
		i4Count += parseRxRespRssiInfo(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				prAdapter,
				&prDisconn->rHwInfo);

		/* Last Beacon RSSI */
		i4Count += kalScnprintf(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				"%-26s%s%d\n", "Beacon RSSI", " = ",
				prDisconn->rBcnRssi);

		/* Dump TX info */
		i4Count += kalScnprintf(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				"\n%s", "<Last Tx Info>\n");

		/* Last TX Rate info */
		for (ucDbdcIdx = 0; ucDbdcIdx < ENUM_BAND_NUM; ucDbdcIdx++) {

			if (prAdapter->rWifiVar.fgDbDcModeEn)

				i4Count += kalScnprintf(
						temp + i4Count,
						sizeof(g_aucProcBuf) - i4Count,
						"[DBDC_%d] :\n",
						ucDbdcIdx);

			i4Count += parseTxRateInfo(temp + i4Count,
						sizeof(g_aucProcBuf) - i4Count,
						&prDisconn->rStaStatistics
							.rTxVector[ucDbdcIdx]);

			if (!prAdapter->rWifiVar.fgDbDcModeEn)
				break;
		}

		/* TX PER info */
		i4Count += parseTxPerInfo(temp + i4Count,
				sizeof(g_aucProcBuf) - i4Count,
				&prDisconn->rHwInfo,
				&prDisconn->rStaStatistics);
	}

	if (copy_to_user(buf, g_aucProcBuf, i4Count)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += i4Count;

	return i4Count;
}
#endif /* CFG_DISCONN_DEBUG_FEATURE */



static const struct file_operations dbglevel_ops = {
	.owner = THIS_MODULE,
	.read = procDbgLevelRead,
	.write = procDbgLevelWrite,
};

#if WLAN_INCLUDE_PROC
#if	CFG_SUPPORT_EASY_DEBUG

static const struct file_operations efusedump_ops = {
	.owner = THIS_MODULE,
	.open = procEfuseDumpOpen,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

static const struct file_operations drivercmd_ops = {
	.owner = THIS_MODULE,
	.read = procDriverCmdRead,
	.write = procDriverCmdWrite,
};

static const struct file_operations cfg_ops = {
	.owner = THIS_MODULE,
	.read = procCfgRead,
	.write = procCfgWrite,
};
#endif
#endif
static const struct file_operations get_txpwr_tbl_ops = {
	.owner	 = THIS_MODULE,
	.read = procGetTxpwrTblRead,
};

#ifdef CFG_GET_TEMPURATURE
static const struct file_operations get_temperature_ops = {
	.owner	 = THIS_MODULE,
	.read = proc_get_temperature,
};
#endif

#if CFG_DISCONN_DEBUG_FEATURE
static const struct file_operations disconn_info_ops = {
	.owner = THIS_MODULE,
	.read = procDisconnInfoRead,
};
#endif

/*******************************************************************************
 *                              F U N C T I O N S
 *******************************************************************************
 */
/*----------------------------------------------------------------------------*/
/*!
 * \brief The PROC function for reading MCR register to User Space, the offset
 *        of the MCR is specified in u4McrOffset.
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
static ssize_t procMCRRead(struct file *filp, char __user *buf,
	 size_t count, loff_t *f_pos)
{
	struct GLUE_INFO *prGlueInfo;
	struct PARAM_CUSTOM_MCR_RW_STRUCT rMcrInfo;
	uint32_t u4BufLen;
	uint32_t u4Count;
	uint8_t *temp = &g_aucProcBuf[0];
	uint32_t rStatus = WLAN_STATUS_SUCCESS;

	/* Kevin: Apply PROC read method 1. */
	if (*f_pos > 0)
		return 0;	/* To indicate end of file. */

	prGlueInfo = g_prGlueInfo_proc;

	rMcrInfo.u4McrOffset = u4McrOffset;

	rStatus = kalIoctl(prGlueInfo,
		wlanoidQueryMcrRead, (void *)&rMcrInfo,
		sizeof(rMcrInfo), TRUE, TRUE, TRUE, &u4BufLen);
	kalMemZero(g_aucProcBuf, sizeof(g_aucProcBuf));
	SNPRINTF(temp, g_aucProcBuf,
		("MCR (0x%08xh): 0x%08x\n", rMcrInfo.u4McrOffset,
		rMcrInfo.u4McrData));

	u4Count = kalStrLen(g_aucProcBuf);
	if (copy_to_user(buf, g_aucProcBuf, u4Count)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4Count;

	return (int)u4Count;

} /* end of procMCRRead() */

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
	struct GLUE_INFO *prGlueInfo;
	/* + 1 for "\0" */
	char acBuf[PROC_MCR_ACCESS_MAX_USER_INPUT_LEN + 1];
	int i4CopySize;
	struct PARAM_CUSTOM_MCR_RW_STRUCT rMcrInfo;
	uint32_t u4BufLen;
	uint32_t rStatus = WLAN_STATUS_SUCCESS;
	int num = 0;

	ASSERT(data);

	i4CopySize =
	    (count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	if (copy_from_user(acBuf, buffer, i4CopySize))
		return 0;
	acBuf[i4CopySize] = '\0';

	num =
	    sscanf(acBuf, "0x%x 0x%x", &rMcrInfo.u4McrOffset,
		   &rMcrInfo.u4McrData);
	switch (num) {
	case 2:
		/* NOTE: Sometimes we want to test if bus will still be ok,
		 * after accessing the MCR which is not align to DW boundary.
		 */
		/* if (IS_ALIGN_4(rMcrInfo.u4McrOffset)) */
		{
			prGlueInfo = g_prGlueInfo_proc;

			u4McrOffset = rMcrInfo.u4McrOffset;

			/* rMcrInfo.u4McrOffset, rMcrInfo.u4McrData); */

			rStatus = kalIoctl(prGlueInfo,
					   wlanoidSetMcrWrite,
					   (void *)&rMcrInfo, sizeof(rMcrInfo),
					   FALSE, FALSE, TRUE, &u4BufLen);

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

#if CFG_SUPPORT_SET_CAM_BY_PROC
static ssize_t procSetCamCfgWrite(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
#define MODULE_NAME_LEN_1 5

	uint32_t u4CopySize = sizeof(g_aucProcBuf);
	uint8_t *temp = &g_aucProcBuf[0];
	u_int8_t fgSetCamCfg = FALSE;
	uint8_t aucModule[MODULE_NAME_LEN_1];
	uint32_t u4Enabled;
	uint8_t aucModuleArray[MODULE_NAME_LEN_1] = "CAM";
	u_int8_t fgParamValue = TRUE;
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';
	temp = &g_aucProcBuf[0];
	while (temp) {
		kalMemSet(aucModule, 0, MODULE_NAME_LEN_1);

		/* pick up a string and teminated after meet : */
		if (sscanf(temp, "%4s %d", aucModule, &u4Enabled) != 2) {
			DBGLOG(INIT, INFO,
				"read param fail, aucModule=%s\n", aucModule);
			fgParamValue = FALSE;
			break;
		}

		if (kalStrnCmp
			(aucModule, aucModuleArray, MODULE_NAME_LEN_1) == 0) {
			if (u4Enabled)
				fgSetCamCfg = TRUE;
			else
				fgSetCamCfg = FALSE;
		}
		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++;		/* skip ',' */
	}

	if (fgParamValue) {
		prGlueInfo = wlanGetGlueInfo();
		if (!prGlueInfo)
			return count;

		prAdapter = prGlueInfo->prAdapter;
		if (!prAdapter)
			return count;

		nicConfigProcSetCamCfgWrite(prAdapter, fgSetCamCfg);
	}

	return count;
}

static const struct file_operations proc_set_cam_ops = {
	.owner = THIS_MODULE,
	.write = procSetCamCfgWrite,
};
#endif /*CFG_SUPPORT_SET_CAM_BY_PROC */

static ssize_t procPktDelayDbgCfgRead(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	uint8_t *temp = &g_aucProcBuf[0];
	uint8_t *str = NULL;
	uint32_t u4CopySize = 0;
	uint8_t ucTxRxFlag;
	uint8_t ucTxIpProto;
	uint16_t u2TxUdpPort;
	uint32_t u4TxDelayThreshold;
	uint8_t ucRxIpProto;
	uint16_t u2RxUdpPort;
	uint32_t u4RxDelayThreshold;
	uint32_t u4StrLen = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0 || buf == NULL)
		return 0;

	str = "\nUsage: txLog/rxLog/reset 1(ICMP)/6(TCP)/11(UDP) Dst/SrcPortNum DelayThreshold(us)\n"
		"Print tx delay log,                                   such as: echo txLog 0 0 0 > pktDelay\n"
		"Print tx UDP delay log,                               such as: echo txLog 11 0 0 > pktDelay\n"
		"Print tx UDP dst port19305 delay log,                 such as: echo txLog 11 19305 0 > pktDelay\n"
		"Print rx UDP src port19305 delay more than 500us log, such as: echo rxLog 11 19305 500 > pktDelay\n"
		"Print tx TCP delay more than 500us log,               such as: echo txLog 6 0 500 > pktDelay\n"
		"Close log,                                            such as: echo reset 0 0 0 > pktDelay\n\n";
	u4StrLen = kalStrLen(str);
	kalStrnCpy(temp, str, u4StrLen + 1);
	temp += kalStrLen(temp);

	StatsEnvGetPktDelay(&ucTxRxFlag, &ucTxIpProto, &u2TxUdpPort,
			&u4TxDelayThreshold, &ucRxIpProto, &u2RxUdpPort,
			&u4RxDelayThreshold);

	if (ucTxRxFlag & BIT(0)) {
		SNPRINTF(temp, g_aucProcBuf,
			("txLog %x %d %d\n", ucTxIpProto, u2TxUdpPort,
			u4TxDelayThreshold));
		temp += kalStrLen(temp);
	}
	if (ucTxRxFlag & BIT(1)) {
		SNPRINTF(temp, g_aucProcBuf,
			("rxLog %x %d %d\n", ucRxIpProto, u2RxUdpPort,
			u4RxDelayThreshold));
		temp += kalStrLen(temp);
	}
	if (ucTxRxFlag == 0)
		SNPRINTF(temp, g_aucProcBuf,
			("reset 0 0 0, there is no tx/rx delay log\n"));

	u4CopySize = kalStrLen(g_aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t) u4CopySize;
}

static ssize_t procPktDelayDbgCfgWrite(struct file *file, const char *buffer,
	size_t count, loff_t *data)
{
#define MODULE_NAME_LENGTH 7
#define MODULE_RESET 0
#define MODULE_TX 1
#define MODULE_RX 2

	uint32_t u4CopySize = sizeof(g_aucProcBuf);
	uint8_t *temp = &g_aucProcBuf[0];
	uint8_t aucModule[MODULE_NAME_LENGTH];
	uint32_t u4DelayThreshold = 0;
	uint32_t u4PortNum = 0;
	uint32_t u4IpProto = 0;
	uint8_t aucResetArray[MODULE_NAME_LENGTH] = "reset";
	uint8_t aucTxArray[MODULE_NAME_LENGTH] = "txLog";
	uint8_t aucRxArray[MODULE_NAME_LENGTH] = "rxLog";
	uint8_t ucTxOrRx = 0;

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';

	while (temp) {
		kalMemSet(aucModule, 0, MODULE_NAME_LENGTH);

		/* pick up a string and teminated after meet : */
		if (sscanf
		    (temp, "%6s %x %d %d", aucModule, &u4IpProto, &u4PortNum,
		     &u4DelayThreshold) != 4) {
			DBGLOG(INIT, INFO,
				"read param fail, aucModule=%s\n", aucModule);
			break;
		}

		if (kalStrnCmp
			(aucModule, aucResetArray, MODULE_NAME_LENGTH) == 0) {
			ucTxOrRx = MODULE_RESET;
		} else if (kalStrnCmp
			(aucModule, aucTxArray, MODULE_NAME_LENGTH) == 0) {
			ucTxOrRx = MODULE_TX;
		} else if (kalStrnCmp
			(aucModule, aucRxArray, MODULE_NAME_LENGTH) == 0) {
			ucTxOrRx = MODULE_RX;
		} else {
			DBGLOG(INIT, INFO, "input module error!\n");
			break;
		}

		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++;		/* skip ',' */
	}

	StatsEnvSetPktDelay(ucTxOrRx, (uint8_t) u4IpProto, (uint16_t) u4PortNum,
		u4DelayThreshold);

	return count;
}

static const struct file_operations proc_pkt_delay_dbg_ops = {
	.owner = THIS_MODULE,
	.read = procPktDelayDbgCfgRead,
	.write = procPktDelayDbgCfgWrite,
};

#if CFG_SUPPORT_DEBUG_FS
static ssize_t procRoamRead(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	uint32_t u4CopySize;
	uint32_t rStatus;
	uint32_t u4BufLen;

	/* if *f_pos > 0, it means has read successed last time,
	 * don't try again
	 */
	if (*f_pos > 0 || buf == NULL)
		return 0;

	rStatus =
	    kalIoctl(g_prGlueInfo_proc, wlanoidGetRoamParams, g_aucProcBuf,
		     sizeof(g_aucProcBuf), TRUE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to read roam params\n");
		return -EINVAL;
	}

	u4CopySize = kalStrLen(g_aucProcBuf);
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (int32_t) u4CopySize;
}

static ssize_t procRoamWrite(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	uint32_t rStatus;
	uint32_t u4BufLen = 0;
	uint32_t u4CopySize = sizeof(g_aucProcBuf);

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';

	if (kalStrnCmp(g_aucProcBuf, "force_roam", 10) == 0)
		rStatus =
		    kalIoctl(g_prGlueInfo_proc, wlanoidSetForceRoam, NULL, 0,
			     FALSE, FALSE, TRUE, &u4BufLen);
	else
		rStatus =
		    kalIoctl(g_prGlueInfo_proc, wlanoidSetRoamParams,
			     g_aucProcBuf, kalStrLen(g_aucProcBuf), FALSE,
			     FALSE, TRUE, &u4BufLen);

	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed to set roam params: %s\n",
		       g_aucProcBuf);
		return -EINVAL;
	}
	return count;
}

static const struct file_operations roam_ops = {
	.owner = THIS_MODULE,
	.read = procRoamRead,
	.write = procRoamWrite,
};
#endif

static ssize_t procCountryRead(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	uint32_t u4CopySize;
	uint32_t country = 0;

	/* if *f_pos > 0, it means has read successed last time */
	if (*f_pos > 0)
		return 0;

	country = rlmDomainGetCountryCode();

	kalMemZero(g_aucProcBuf, sizeof(g_aucProcBuf));
	if (country)
		kalSnprintf(g_aucProcBuf, sizeof(g_aucProcBuf),
			"Current Country Code: %s\n", &country);
	else
		kalSnprintf(g_aucProcBuf, sizeof(g_aucProcBuf),
			"Current Country Code: NULL\n");

	u4CopySize = kalStrLen(g_aucProcBuf);
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, ERROR, "copy to user failed\n");
		return -EFAULT;
	}
	*f_pos += u4CopySize;

	return (int32_t) u4CopySize;
}

static ssize_t procCountryWrite(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	uint32_t u4BufLen = 0;
	uint32_t rStatus;
	uint32_t u4CopySize = sizeof(g_aucProcBuf);

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, ERROR, "error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';

	rStatus = kalIoctl(g_prGlueInfo_proc, wlanoidSetCountryCode,
			   &g_aucProcBuf[0], 2, FALSE, FALSE, TRUE, &u4BufLen);
	if (rStatus != WLAN_STATUS_SUCCESS) {
		DBGLOG(INIT, INFO, "failed set country code: %s\n",
			g_aucProcBuf);
		return -EINVAL;
	}
	return count;
}

static const struct file_operations country_ops = {
	.owner = THIS_MODULE,
	.read = procCountryRead,
	.write = procCountryWrite,
};

static ssize_t procAutoPerfCfgRead(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	uint8_t *temp = &g_aucProcBuf[0];
	uint8_t *str = NULL;
	uint32_t u4CopySize = 0;
	uint32_t u4StrLen = 0;

	/* if *f_ops>0, we should return 0 to make cat command exit */
	if (*f_pos > 0)
		return 0;

	str = "Auto Performance Configure Usage:\n"
	    "\n"
	    "echo ForceEnable:0 or 1 > /proc/net/wlan/autoPerfCfg\n"
	    "     1: always enable performance monitor\n"
	    "     0: restore performance monitor's default strategy\n";
	u4StrLen = kalStrLen(str);
	kalStrnCpy(temp, str, u4StrLen + 1);

	u4CopySize = kalStrLen(g_aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		DBGLOG(INIT, WARN, "copy_to_user error\n");
		return -EFAULT;
	}

	*f_pos += u4CopySize;
	return (ssize_t) u4CopySize;
}

static ssize_t procAutoPerfCfgWrite(struct file *file, const char *buffer,
	size_t count, loff_t *data)
{
	uint32_t u4CoreNum = 0;
	uint32_t u4CoreFreq = 0;
	uint8_t *temp = &g_aucProcBuf[0];
	uint32_t u4CopySize = count;
	uint8_t i = 0;
	uint32_t u4ForceEnable = 0;
	uint8_t aucBuf[32];

	if (u4CopySize >= sizeof(g_aucProcBuf))
		u4CopySize = sizeof(g_aucProcBuf) - 1;

	kalMemSet(g_aucProcBuf, 0, u4CopySize);

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		DBGLOG(INIT, WARN, "copy_from_user error\n");
		return -EFAULT;
	}

	g_aucProcBuf[u4CopySize] = '\0';

	i = sscanf(temp, "%d:%d", &u4CoreNum, &u4CoreFreq);
	if (i == 2) {
		DBGLOG(INIT, INFO, "u4CoreNum:%d, u4CoreFreq:%d\n", u4CoreNum,
			u4CoreFreq);
		kalSetCpuNumFreq(u4CoreNum, u4CoreFreq);
		return u4CopySize;
	}

	if (strlen(temp) > sizeof(aucBuf)) {
		DBGLOG(INIT, WARN,
			"input string(%s) len is too long, over %d\n",
			g_aucProcBuf, (uint32_t) sizeof(aucBuf));
		return -EFAULT;
	}

	i = sscanf(temp, "%11s:%d", aucBuf, &u4ForceEnable);

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


int32_t procInitFs(void)
{
	struct proc_dir_entry *prEntry;

	g_NextDriverReadLen = 0;

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
	proc_set_user(gprProcRoot, KUIDT_INIT(PROC_UID_SHELL),
		      KGIDT_INIT(PROC_GID_WIFI));

	prEntry =
	    proc_create(PROC_DBG_LEVEL_NAME, 0664, gprProcRoot, &dbglevel_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR,
			"Unable to create /proc entry dbgLevel\n\r");
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL),
		      KGIDT_INIT(PROC_GID_WIFI));

	prEntry =
	    proc_create(PROC_AUTO_PERF_CFG, 0664, gprProcRoot, &auto_perf_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry %s/n",
		       PROC_AUTO_PERF_CFG);
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL),
		      KGIDT_INIT(PROC_GID_WIFI));

	return 0;
}				/* end of procInitProcfs() */

int32_t procUninitProcFs(void)
{
#if KERNEL_VERSION(3, 9, 0) <= LINUX_VERSION_CODE
	remove_proc_subtree(PROC_AUTO_PERF_CFG, gprProcRoot);
	remove_proc_subtree(PROC_DBG_LEVEL_NAME, gprProcRoot);

	/*
	 * move PROC_ROOT_NAME to last since it's root directory of the others
	 * incorrect sequence would cause use-after-free error
	 */
	remove_proc_subtree(PROC_ROOT_NAME, init_net.proc_net);
#else
	remove_proc_entry(PROC_AUTO_PERF_CFG, gprProcRoot);
	remove_proc_entry(PROC_DBG_LEVEL_NAME, gprProcRoot);

	/*
	 * move PROC_ROOT_NAME to last since it's root directory of the others
	 * incorrect sequence would cause use-after-free error
	 */
	remove_proc_entry(PROC_ROOT_NAME, init_net.proc_net);
#endif

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
int32_t procRemoveProcfs(void)
{
	remove_proc_entry(PROC_MCR_ACCESS, gprProcRoot);
	remove_proc_entry(PROC_DRIVER_CMD, gprProcRoot);
	remove_proc_entry(PROC_CFG, gprProcRoot);
	remove_proc_entry(PROC_EFUSE_DUMP, gprProcRoot);
	remove_proc_entry(PROC_GET_TXPWR_TBL, gprProcRoot);
	remove_proc_entry(PROC_PKT_DELAY_DBG, gprProcRoot);
	remove_proc_entry(PROC_COUNTRY, gprProcRoot);
#if CFG_SUPPORT_SET_CAM_BY_PROC
	remove_proc_entry(PROC_SET_CAM, gprProcRoot);
#endif
#ifdef CFG_GET_TEMPURATURE
	remove_proc_entry(PROC_GET_TEMPETATURE, gprProcRoot);
#endif
#if CFG_SUPPORT_DEBUG_FS
	remove_proc_entry(PROC_ROAM_PARAM, gprProcRoot);
#endif
#if CFG_DISCONN_DEBUG_FEATURE
	remove_proc_entry(PROC_DISCONN_INFO, gprProcRoot);
#endif

	return 0;
} /* end of procRemoveProcfs() */

int32_t procCreateFsEntry(struct GLUE_INFO *prGlueInfo)
{
	struct proc_dir_entry *prEntry;

	DBGLOG(INIT, INFO, "[%s]\n", __func__);
	g_prGlueInfo_proc = prGlueInfo;

	prEntry = proc_create(PROC_MCR_ACCESS, 0664, gprProcRoot, &mcr_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry mcr\n\r");
		return -1;
	}

	prEntry =
	    proc_create(PROC_PKT_DELAY_DBG, 0664, gprProcRoot,
			&proc_pkt_delay_dbg_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR,
		       "Unable to create /proc entry pktDelay\n\r");
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL),
		      KGIDT_INIT(PROC_GID_WIFI));

#if CFG_SUPPORT_SET_CAM_BY_PROC
	prEntry =
	    proc_create(PROC_SET_CAM, 0664, gprProcRoot, &proc_set_cam_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry SetCAM\n\r");
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL),
		      KGIDT_INIT(PROC_GID_WIFI));
#endif
#if CFG_SUPPORT_DEBUG_FS
	prEntry = proc_create(PROC_ROAM_PARAM, 0664, gprProcRoot, &roam_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR,
		       "Unable to create /proc entry roam_param\n\r");
		return -1;
	}
#endif
	prEntry = proc_create(PROC_COUNTRY, 0664, gprProcRoot, &country_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry country\n\r");
		return -1;
	}

#if	CFG_SUPPORT_EASY_DEBUG

	prEntry =
		proc_create(PROC_DRIVER_CMD, 0664, gprProcRoot, &drivercmd_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR,
			"Unable to create /proc entry for driver command\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_CFG, 0664, gprProcRoot, &cfg_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR,
			"Unable to create /proc entry for driver cfg\n\r");
		return -1;
	}

	prEntry =
		proc_create(PROC_EFUSE_DUMP, 0664, gprProcRoot, &efusedump_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry efuse\n\r");
		return -1;
	}
#endif
	prEntry = proc_create(PROC_GET_TXPWR_TBL, 0664, gprProcRoot,
			      &get_txpwr_tbl_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR,
			"Unable to create /proc entry TXPWR Table\n\r");
		return -1;
	}
#ifdef CFG_GET_TEMPURATURE
	prEntry = proc_create(PROC_GET_TEMPETATURE, 0664, gprProcRoot,
			      &get_temperature_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry efuse\n\r");
		return -1;
	}
#endif

#if CFG_DISCONN_DEBUG_FEATURE
	prEntry = proc_create(PROC_DISCONN_INFO, 0664, gprProcRoot,
			&disconn_info_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR,
			"Unable to create /proc entry disconn_info\n\r");
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
static int procDrvStatusRead(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	struct GLUE_INFO *prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	uint32_t u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SNPRINTF(p, page, ("GLUE LAYER STATUS:"));
	SNPRINTF(p, page, ("\n=================="));

	SNPRINTF(p, page,
		("\n* Number of Pending Frames: %ld\n",
		prGlueInfo->u4TxPendingFrameNum));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryDrvStatusForLinuxProc(prGlueInfo->prAdapter, p, &u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (uint32_t) (p - page);

	*eof = 1;

	return (int)u4Count;

} /* end of procDrvStatusRead() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief The PROC function for reading Driver RX Statistic Counters
 *        to User Space.
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
static int procRxStatisticsRead(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	struct GLUE_INFO *prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	uint32_t u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SNPRINTF(p, page, ("RX STATISTICS (Write 1 to clear):"));
	SNPRINTF(p, page, ("\n=================================\n"));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryRxStatisticsForLinuxProc(prGlueInfo->prAdapter, p,
		&u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (uint32_t) (p - page);

	*eof = 1;

	return (int)u4Count;

} /* end of procRxStatisticsRead() */

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
static int procRxStatisticsWrite(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	struct GLUE_INFO *prGlueInfo = ((struct net_device *)data)->priv;
	/* + 1 for "\0" */
	char acBuf[PROC_RX_STATISTICS_MAX_USER_INPUT_LEN + 1];
	uint32_t u4CopySize;
	uint32_t u4ClearCounter;
	int32_t rv;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	u4CopySize =
		(count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	rv = kstrtoint(acBuf, 0, &u4ClearCounter);
	if (rv == 1) {
		if (u4ClearCounter == 1) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

			wlanoidSetRxStatisticsForLinuxProc(prGlueInfo->
				prAdapter);

			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);
		}
	}

	return count;

} /* end of procRxStatisticsWrite() */

/*----------------------------------------------------------------------------*/
/*!
 * \brief The PROC function for reading Driver TX Statistic Counters
 *        to User Space.
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
static int procTxStatisticsRead(char *page, char **start, off_t off, int count,
	int *eof, void *data)
{
	struct GLUE_INFO *prGlueInfo = ((struct net_device *)data)->priv;
	char *p = page;
	uint32_t u4Count;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	/* Kevin: Apply PROC read method 1. */
	if (off != 0)
		return 0;	/* To indicate end of file. */

	SNPRINTF(p, page, ("TX STATISTICS (Write 1 to clear):"));
	SNPRINTF(p, page, ("\n=================================\n"));

	GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	wlanoidQueryTxStatisticsForLinuxProc(prGlueInfo->prAdapter, p,
		&u4Count);

	GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

	u4Count += (uint32_t) (p - page);

	*eof = 1;

	return (int)u4Count;

} /* end of procTxStatisticsRead() */

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
static int procTxStatisticsWrite(struct file *file, const char *buffer,
	unsigned long count, void *data)
{
	struct GLUE_INFO *prGlueInfo = ((struct net_device *)data)->priv;
	/* + 1 for "\0" */
	char acBuf[PROC_RX_STATISTICS_MAX_USER_INPUT_LEN + 1];
	uint32_t u4CopySize;
	uint32_t u4ClearCounter;
	int32_t rv;

	GLUE_SPIN_LOCK_DECLARATION();

	ASSERT(data);

	u4CopySize =
		(count < (sizeof(acBuf) - 1)) ? count : (sizeof(acBuf) - 1);
	copy_from_user(acBuf, buffer, u4CopySize);
	acBuf[u4CopySize] = '\0';

	rv = kstrtoint(acBuf, 0, &u4ClearCounter);
	if (rv == 1) {
		if (u4ClearCounter == 1) {
			GLUE_ACQUIRE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);

			wlanoidSetTxStatisticsForLinuxProc(prGlueInfo->
				prAdapter);

			GLUE_RELEASE_SPIN_LOCK(prGlueInfo, SPIN_LOCK_FSM);
		}
	}

	return count;

} /* end of procTxStatisticsWrite() */
#endif

#ifdef FW_CFG_SUPPORT
#define MAX_CFG_OUTPUT_BUF_LENGTH 1024
static uint8_t aucCfgBuf[CMD_FORMAT_V1_LENGTH];
static uint8_t aucCfgQueryKey[MAX_CMD_NAME_MAX_LENGTH];
static uint8_t aucCfgOutputBuf[MAX_CFG_OUTPUT_BUF_LENGTH];

static ssize_t cfgRead(struct file *filp, char __user *buf, size_t count,
	loff_t *f_pos)
{
	uint32_t rStatus = WLAN_STATUS_FAILURE;
	uint8_t *temp = &aucCfgOutputBuf[0];
	uint32_t u4CopySize = 0;

	struct CMD_HEADER cmdV1Header;
	struct CMD_FORMAT_V1 *pr_cmd_v1 =
		(struct CMD_FORMAT_V1 *)cmdV1Header.buffer;

	/* if *f_pos >  0, we should return 0 to make cat command exit */
	if (*f_pos > 0 || gprGlueInfo == NULL)
		return 0;
	if (!kalStrLen(aucCfgQueryKey))
		return 0;

	kalMemSet(aucCfgOutputBuf, 0, MAX_CFG_OUTPUT_BUF_LENGTH);

	SNPRINTF(temp, aucCfgOutputBuf,
		("\nprocCfgRead() %s:\n", aucCfgQueryKey));

	/* send to FW */
	cmdV1Header.cmdVersion = CMD_VER_1;
	cmdV1Header.cmdType = CMD_TYPE_QUERY;
	cmdV1Header.itemNum = 1;
	cmdV1Header.cmdBufferLen = sizeof(struct CMD_FORMAT_V1);
	kalMemSet(cmdV1Header.buffer, 0, MAX_CMD_BUFFER_LENGTH);

	pr_cmd_v1->itemStringLength = kalStrLen(aucCfgQueryKey);

	kalMemCopy(pr_cmd_v1->itemString, aucCfgQueryKey,
		kalStrLen(aucCfgQueryKey));

	rStatus = kalIoctl(gprGlueInfo,
		wlanoidQueryCfgRead,
		(void *)&cmdV1Header,
		sizeof(cmdV1Header), TRUE, TRUE, TRUE, &u4CopySize);
	if (rStatus == WLAN_STATUS_FAILURE)
		DBGLOG(INIT, ERROR,
			"kalIoctl wlanoidQueryCfgRead fail 0x%x\n",
			rStatus);

	SNPRINTF(temp, aucCfgOutputBuf,
		("%s\n", cmdV1Header.buffer));

	u4CopySize = kalStrLen(aucCfgOutputBuf);
	if (u4CopySize > count)
		u4CopySize = count;

	if (copy_to_user(buf, aucCfgOutputBuf, u4CopySize))
		DBGLOG(INIT, ERROR, "copy to user failed\n");

	*f_pos += u4CopySize;
	return (ssize_t) u4CopySize;
}

static ssize_t cfgWrite(struct file *filp, const char __user *buf,
	size_t count, loff_t *f_pos)
{
	/* echo xxx xxx > /proc/net/wlan/cfg */
	uint8_t i = 0;
	uint32_t u4CopySize = sizeof(aucCfgBuf);
	uint8_t token_num = 1;

	kalMemSet(aucCfgBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

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
		memcpy(aucCfgQueryKey, aucCfgBuf, u4CopySize);
		if (aucCfgQueryKey[u4CopySize - 1] == 0x0a)
			aucCfgQueryKey[u4CopySize - 1] = '\0';
	} else {
		if (u4CopySize)
			wlanFwCfgParse(gprGlueInfo->prAdapter, aucCfgBuf);
	}

	return count;
}

static const struct file_operations fwcfg_ops = {
	.owner = THIS_MODULE,
	.read = cfgRead,
	.write = cfgWrite,
};

int32_t cfgRemoveProcEntry(void)
{
	remove_proc_entry(PROC_CFG_NAME, gprProcRoot);
	return 0;
}

int32_t cfgCreateProcEntry(struct GLUE_INFO *prGlueInfo)
{
	struct proc_dir_entry *prEntry;

	prGlueInfo->pProcRoot = gprProcRoot;
	gprGlueInfo = prGlueInfo;

	prEntry = proc_create(PROC_CFG_NAME, 0664, gprProcRoot, &fwcfg_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry cfg\n\r");
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL),
		KGIDT_INIT(PROC_GID_WIFI));

	return 0;
}
#endif
