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
#include <linux/rtc.h>

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
#define PROC_PKT_DELAY_DBG			"pktDelay"
#if CFG_SUPPORT_SET_CAM_BY_PROC
#define PROC_SET_CAM				"setCAM"
#endif
#define PROC_AUTO_PERF_CFG			"autoPerfCfg"

#if (CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)
#define PROC_CAL_RESULT				"cal_result"
#endif /*(CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)*/

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
static int32_t g_i4NextDriverReadLen;
/*******************************************************************************
 *                                 M A C R O S
 *******************************************************************************
 */

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
	kalStrnCpy(temp, str, u4StrLen);
	temp += u4StrLen;

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
		pr_err("copy to user failed\n");
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
	if (prGlueInfo == NULL) {
		seq_puts(s, "prGlueInfo is null\n");
		return -EPERM;
	}

	if (prGlueInfo &&
	    prGlueInfo->prAdapter &&
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
		pr_err("procCfgRead prGlueInfo is  NULL????\n");
		return 0;
	}

	prAdapter = prGlueInfo->prAdapter;

	if (!prAdapter) {
		pr_err("procCfgRead prAdapter is  NULL????\n");
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
	kalStrnCpy(temp, str, u4StrLen);
	temp += u4StrLen;

	for (i = 0; i < WLAN_CFG_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry =
			wlanCfgGetEntryByIndex(prAdapter, i, WLAN_CFG_DEFAULT);

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
			       WLAN_CFG_KEY_LEN_MAX,
			       (uint32_t)prWlanCfgEntry->aucKey[
				WLAN_CFG_KEY_LEN_MAX - 1],
			       WLAN_CFG_VALUE_LEN_MAX,
			       (uint32_t)prWlanCfgEntry->aucValue[
				WLAN_CFG_VALUE_LEN_MAX - 1]);
			kalMemSet(g_aucProcBuf, ' ', u4StrLen);
			kalStrnCpy(g_aucProcBuf, str2, kalStrLen(str2) + 1);
			g_aucProcBuf[u4StrLen-1] = 0;
			goto procCfgReadLabel;
		}

		if (kalStrLen(g_aucProcBuf) >
			(sizeof(g_aucProcBuf) - BUFFER_RESERVE_BYTE))
			break;
	}

	for (i = 0; i < WLAN_CFG_REC_ENTRY_NUM_MAX; i++) {
		prWlanCfgEntry =
			wlanCfgGetEntryByIndex(prAdapter, i, WLAN_CFG_REC);

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
			       WLAN_CFG_KEY_LEN_MAX,
			       (uint32_t)prWlanCfgEntry->aucKey[
				WLAN_CFG_KEY_LEN_MAX - 1],
			       WLAN_CFG_VALUE_LEN_MAX,
			       (uint32_t)prWlanCfgEntry->aucValue[
				WLAN_CFG_VALUE_LEN_MAX - 1]);
			kalMemSet(g_aucProcBuf, ' ', u4StrLen);
			kalStrnCpy(g_aucProcBuf, str2, kalStrLen(str2) + 1);
			g_aucProcBuf[u4StrLen-1] = 0;
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
		pr_err("copy to user failed\n");
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
	uint32_t u4CopySize = sizeof(g_aucProcBuf)-8;
	struct GLUE_INFO *prGlueInfo;
	uint8_t *pucTmp;
	/* PARAM_CUSTOM_P2P_SET_STRUCT_T rSetP2P; */
	uint32_t i = 0;

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	pucTmp = g_aucProcBuf;
	SNPRINTF(pucTmp, g_aucProcBuf, ("%s ", "set_cfg"));

	if (copy_from_user(pucTmp, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize + 8] = '\0';

	for (i = 8 ; i < u4CopySize+8; i++) {
		if (!isalnum(g_aucProcBuf[i]) && /* alphanumeric */
			g_aucProcBuf[i] != 0x20 && /* space */
			g_aucProcBuf[i] != 0x0a && /* control char */
			g_aucProcBuf[i] != 0x0d) {
			DBGLOG(INIT, ERROR, "wrong char[%d] 0x%x\n",
				i, g_aucProcBuf[i]);
			return -EFAULT;
		}
	}

	prGlueInfo = g_prGlueInfo_proc;
	/* if g_i4NextDriverReadLen >0,
	 * the content for next DriverCmdRead will be
	 * in : g_aucProcBuf with length : g_i4NextDriverReadLen
	 */
	g_i4NextDriverReadLen =
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

	if (g_i4NextDriverReadLen > 0)	/* Detect content to show */
		u4CopySize = g_i4NextDriverReadLen;

	if (u4CopySize > count) {
		pr_err("count is too small: u4CopySize=%u, count=%u\n",
		       u4CopySize, (uint32_t)count);
		return -EFAULT;
	}

	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}
	g_i4NextDriverReadLen = 0;

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
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';


	prGlueInfo = g_prGlueInfo_proc;
	/* if g_u4NextDriverReadLen >0,
	 * the content for next DriverCmdRead will be
	 *  in : g_aucProcBuf with length : g_u4NextDriverReadLen
	 */
	if (strlen(g_aucProcBuf) > 0) {
		g_i4NextDriverReadLen =
			priv_driver_cmds(prGlueInfo->prDevHandler, g_aucProcBuf,
			sizeof(g_aucProcBuf));
	}

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
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';

	while (temp) {
		if (sscanf(temp,
			"0x%x:0x%x", &u4NewDbgModule, &u4NewDbgLevel) != 2) {
			pr_info("debug module and debug level should be one byte in length\n");
			break;
		}
		if (u4NewDbgModule == 0xFF) {
			wlanSetDriverDbgLevel(DBG_ALL_MODULE_IDX,
					(u4NewDbgLevel & DBG_CLASS_MASK));
			break;
		}
		if (u4NewDbgModule >= DBG_MODULE_NUM) {
			pr_info("debug module index should less than %d\n",
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
		pr_err("copy to user failed\n");
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
	if (copy_from_user(acBuf, buffer, i4CopySize) ||
		i4CopySize < 0 ||
		i4CopySize > PROC_MCR_ACCESS_MAX_USER_INPUT_LEN)
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

			/* printk("Write 0x%lx to MCR 0x%04lx\n", */
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
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';
	temp = &g_aucProcBuf[0];
	while (temp) {
		kalMemSet(aucModule, 0, MODULE_NAME_LEN_1);

		/* pick up a string and teminated after meet : */
		if (sscanf(temp, "%4s %d", aucModule, &u4Enabled) != 2) {
			pr_info("read param fail, aucModule=%s\n", aucModule);
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
		uint8_t i;

		prGlueInfo = wlanGetGlueInfo();
		if (!prGlueInfo)
			return count;

		prAdapter = prGlueInfo->prAdapter;
		if (!prAdapter)
			return count;

		for (i = 0; i < KAL_AIS_NUM; i++) {
			nicConfigProcSetCamCfgWrite(prAdapter,
				fgSetCamCfg,
				i);
		}
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
	uint8_t ucTxRxFlag = 0;
	uint8_t ucTxIpProto = 0;
	uint16_t u2TxUdpPort = 0;
	uint32_t u4TxDelayThreshold = 0;
	uint8_t ucRxIpProto = 0;
	uint16_t u2RxUdpPort = 0;
	uint32_t u4RxDelayThreshold = 0;
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
	kalStrnCpy(temp, str, u4StrLen);
	temp += u4StrLen;

#if (CFG_SUPPORT_STATISTICS == 1)
	StatsEnvGetPktDelay(&ucTxRxFlag, &ucTxIpProto, &u2TxUdpPort,
			&u4TxDelayThreshold, &ucRxIpProto, &u2RxUdpPort,
			&u4RxDelayThreshold);
#endif

	if (ucTxRxFlag & BIT(0)) {
		SNPRINTF(temp, g_aucProcBuf,
			("txLog %x %d %d\n", ucTxIpProto, u2TxUdpPort,
			u4TxDelayThreshold));
	}
	if (ucTxRxFlag & BIT(1)) {
		SNPRINTF(temp, g_aucProcBuf,
			("rxLog %x %d %d\n", ucRxIpProto, u2RxUdpPort,
			u4RxDelayThreshold));
	}
	if (ucTxRxFlag == 0)
		SNPRINTF(temp, g_aucProcBuf,
			("reset 0 0 0, there is no tx/rx delay log\n"));

	u4CopySize = kalStrLen(g_aucProcBuf);
	if (u4CopySize > count)
		u4CopySize = count;
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
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
		pr_err("error of copy from user\n");
		return -EFAULT;
	}
	g_aucProcBuf[u4CopySize] = '\0';

	while (temp) {
		kalMemSet(aucModule, 0, MODULE_NAME_LENGTH);

		/* pick up a string and teminated after meet : */
		if (sscanf
		    (temp, "%6s %x %d %d", aucModule, &u4IpProto, &u4PortNum,
		     &u4DelayThreshold) != 4) {
			pr_info("read param fail, aucModule=%s\n", aucModule);
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
			pr_info("input module error!\n");
			break;
		}

		temp = kalStrChr(temp, ',');
		if (!temp)
			break;
		temp++;		/* skip ',' */
	}

#if (CFG_SUPPORT_STATISTICS == 1)
	StatsEnvSetPktDelay(ucTxOrRx, (uint8_t) u4IpProto, (uint16_t) u4PortNum,
		u4DelayThreshold);
#endif
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
		pr_err("copy to user failed\n");
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
		pr_err("error of copy from user\n");
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
			"Current Country Code: %d\n", country);
	else
		kalSnprintf(g_aucProcBuf, sizeof(g_aucProcBuf),
			"Current Country Code: NULL\n");

	u4CopySize = kalStrLen(g_aucProcBuf);
	if (copy_to_user(buf, g_aucProcBuf, u4CopySize)) {
		pr_err("copy to user failed\n");
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
		pr_err("error of copy from user\n");
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

#if (CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)
static ssize_t procCalResultRead(struct file *filp, char __user *buf,
	size_t count, loff_t *f_pos)
{
	struct GLUE_INFO *prGlueInfo = NULL;
	struct ADAPTER *prAdapter = NULL;
	struct mt66xx_chip_info *prChipInfo = NULL;
	uint32_t u4CalSize = 0;
	uint8_t *prCalResult = NULL;

	/* if *f_pos > 0, it means has read successed last time */
	if (*f_pos > 0)
		return 0;

	prGlueInfo = wlanGetGlueInfo();
	if (!prGlueInfo)
		return 0;

	prAdapter = prGlueInfo->prAdapter;
	if (!prAdapter)
		return 0;

	prChipInfo = prAdapter->chip_info;
	if (!prChipInfo)
		return 0;

	if (prChipInfo->getCalResult)
		prCalResult = prChipInfo->getCalResult(&u4CalSize);
	else
		return 0;

	if ((prCalResult == NULL) || (u4CalSize == 0)) {
		DBGLOG(INIT, WARN, "prCalResult NULL\n");
		return 0;
	}

	if (copy_to_user(buf, prCalResult, u4CalSize)) {
		pr_err("copy to user failed\n");
		return -EFAULT;
	}

	*f_pos += u4CalSize;

	return (int32_t)u4CalSize;
}

static ssize_t procCalResultWrite(struct file *file, const char __user *buffer,
	size_t count, loff_t *data)
{
	uint32_t u4CopySize = sizeof(g_aucProcBuf);

	kalMemSet(g_aucProcBuf, 0, u4CopySize);
	u4CopySize = (count < u4CopySize) ? count : (u4CopySize - 1);

	if (copy_from_user(g_aucProcBuf, buffer, u4CopySize)) {
		pr_err("error of copy from user\n");
		return -EFAULT;
	}

	g_aucProcBuf[u4CopySize] = '\0';

	return count;
}

static const struct file_operations cal_result_ops = {
	.owner = THIS_MODULE,
	.read = procCalResultRead,
	.write = procCalResultWrite,
};
#endif /*(CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)*/

int32_t procInitFs(void)
{
	struct proc_dir_entry *prEntry;

	g_i4NextDriverReadLen = 0;

	if (init_net.proc_net == (struct proc_dir_entry *)NULL) {
		pr_err("init proc fs fail: proc_net == NULL\n");
		return -ENOENT;
	}

	/*
	 * Directory: Root (/proc/net/wlan0)
	 */

	gprProcRoot = proc_mkdir(PROC_ROOT_NAME, init_net.proc_net);
	if (!gprProcRoot) {
		pr_err("gprProcRoot == NULL\n");
		return -ENOENT;
	}
	proc_set_user(gprProcRoot, KUIDT_INIT(PROC_UID_SHELL),
		      KGIDT_INIT(PROC_GID_WIFI));

	prEntry =
	    proc_create(PROC_DBG_LEVEL_NAME, 0664, gprProcRoot, &dbglevel_ops);
	if (prEntry == NULL) {
		pr_err("Unable to create /proc entry dbgLevel\n\r");
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

#if (CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)
	prEntry = proc_create(PROC_CAL_RESULT,
							0664,
							gprProcRoot,
							&cal_result_ops);
	if (prEntry == NULL) {
		DBGLOG(INIT, ERROR, "Unable to create /proc entry %s/n",
				PROC_CAL_RESULT);
		return -1;
	}
	proc_set_user(prEntry, KUIDT_INIT(PROC_UID_SHELL),
					KGIDT_INIT(PROC_GID_WIFI));
#endif /*(CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)*/

	return 0;
}				/* end of procInitProcfs() */

int32_t procUninitProcFs(void)
{
#if KERNEL_VERSION(3, 9, 0) <= LINUX_VERSION_CODE

#if (CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)
	remove_proc_subtree(PROC_CAL_RESULT, gprProcRoot);
#endif /*(CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)*/

	remove_proc_subtree(PROC_AUTO_PERF_CFG, gprProcRoot);
	remove_proc_subtree(PROC_DBG_LEVEL_NAME, gprProcRoot);

	/*
	 * move PROC_ROOT_NAME to last since it's root directory of the others
	 * incorrect sequence would cause use-after-free error
	 */
	remove_proc_subtree(PROC_ROOT_NAME, init_net.proc_net);
#else

#if (CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)
	remove_proc_entry(PROC_CAL_RESULT, gprProcRoot);
#endif /*(CFG_SUPPORT_PRE_ON_PHY_ACTION == 1)*/

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
	remove_proc_entry(PROC_PKT_DELAY_DBG, gprProcRoot);
#if CFG_SUPPORT_SET_CAM_BY_PROC
	remove_proc_entry(PROC_SET_CAM, gprProcRoot);
#endif
#if CFG_SUPPORT_DEBUG_FS
	remove_proc_entry(PROC_ROAM_PARAM, gprProcRoot);
#endif
	remove_proc_entry(PROC_COUNTRY, gprProcRoot);
	return 0;
} /* end of procRemoveProcfs() */

int32_t procCreateFsEntry(struct GLUE_INFO *prGlueInfo)
{
	struct proc_dir_entry *prEntry;

	DBGLOG(INIT, TRACE, "[%s]\n", __func__);
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
		pr_err("Unable to create /proc entry for driver command\n\r");
		return -1;
	}

	prEntry = proc_create(PROC_CFG, 0664, gprProcRoot, &cfg_ops);
	if (prEntry == NULL) {
		pr_err("Unable to create /proc entry for driver cfg\n\r");
		return -1;
	}

	prEntry =
		proc_create(PROC_EFUSE_DUMP, 0664, gprProcRoot, &efusedump_ops);
	if (prEntry == NULL) {
		pr_err("Unable to create /proc entry efuse\n\r");
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
