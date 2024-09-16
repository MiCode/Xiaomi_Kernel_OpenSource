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


#include "fwcfg.h"

/*******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
********************************************************************************
*/
static char *strtok_r(char *s, const char *delim, char **last);
/*******************************************************************************
*                            P U B L I C   D A T A
********************************************************************************
*/
struct _FW_CFG __weak fwCfgArray[] = {

};
/* ******************************************************************************
*                              F U N C T I O N S
*********************************************************************************
*/
INT_32 __weak getFwCfgItemNum()
{
	return ARRAY_SIZE(fwCfgArray);
}

PUINT_8 __weak getFwCfgItemKey(UINT_8 i)
{
	if (i < ARRAY_SIZE(fwCfgArray))
		return fwCfgArray[i].key;
	else
		return NULL;
}

PUINT_8 __weak getFwCfgItemValue(UINT_8 i)
{
	if (i < ARRAY_SIZE(fwCfgArray))
		return fwCfgArray[i].value;
	else
		return NULL;
}

void wlanCfgFwSetParam(PUINT_8 fwBuffer, PCHAR cmdStr, PCHAR value, int num, int type)
{
	struct _CMD_FORMAT_V1_T *cmd = (struct _CMD_FORMAT_V1_T *)fwBuffer + num;

	kalMemSet(cmd, 0, sizeof(struct _CMD_FORMAT_V1_T));
	cmd->itemType = type;

	cmd->itemStringLength = strlen(cmdStr);
	if (cmd->itemStringLength > MAX_CMD_NAME_MAX_LENGTH)
		cmd->itemStringLength = MAX_CMD_NAME_MAX_LENGTH;

	/* here will not ensure the end will be '\0' */
	kalMemCopy(cmd->itemString, cmdStr, cmd->itemStringLength);

	cmd->itemValueLength = strlen(value);
	if (cmd->itemValueLength > MAX_CMD_VALUE_MAX_LENGTH)
		cmd->itemValueLength = MAX_CMD_VALUE_MAX_LENGTH;

	/* here will not ensure the end will be '\0' */
	kalMemCopy(cmd->itemValue, value, cmd->itemValueLength);
}

WLAN_STATUS wlanCfgSetGetFw(IN P_ADAPTER_T prAdapter, const PCHAR fwBuffer, int cmdNum, enum _CMD_TYPE_T cmdType)
{
	struct _CMD_HEADER_T *pcmdV1Header = NULL;

	pcmdV1Header = (struct _CMD_HEADER_T *) kalMemAlloc(sizeof(struct _CMD_HEADER_T), VIR_MEM_TYPE);

	if (pcmdV1Header == NULL)
		return WLAN_STATUS_FAILURE;

	kalMemSet(pcmdV1Header->buffer, 0, MAX_CMD_BUFFER_LENGTH);
	pcmdV1Header->cmdType = cmdType;
	pcmdV1Header->cmdVersion = CMD_VER_1_EXT;
	pcmdV1Header->itemNum = cmdNum;
	pcmdV1Header->cmdBufferLen = cmdNum * sizeof(struct _CMD_FORMAT_V1_T);
	kalMemCopy(pcmdV1Header->buffer, fwBuffer, pcmdV1Header->cmdBufferLen);

	wlanSendSetQueryCmd(prAdapter, CMD_ID_GET_SET_CUSTOMER_CFG,
				TRUE, FALSE, FALSE,
				NULL, NULL,
				sizeof(struct _CMD_HEADER_T),
				(PUINT_8) pcmdV1Header,
				NULL, 0);
	kalMemFree(pcmdV1Header, VIR_MEM_TYPE, sizeof(struct _CMD_HEADER_T));
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS wlanFwArrayCfg(IN P_ADAPTER_T prAdapter)
{
	int kk = 0;
	PUINT_8 cmdBuffer = NULL;
	int fwCfgItemNum = getFwCfgItemNum();

	if (!fwCfgItemNum)
		return WLAN_STATUS_FAILURE;

	cmdBuffer = kalMemAlloc(MAX_CMD_BUFFER_LENGTH, VIR_MEM_TYPE);

	if (cmdBuffer == 0)
		return WLAN_STATUS_FAILURE;

	kalMemSet(cmdBuffer, 0, MAX_CMD_BUFFER_LENGTH);

	for (; kk < fwCfgItemNum;) {
		wlanCfgFwSetParam(cmdBuffer, getFwCfgItemKey(kk),
			getFwCfgItemValue(kk), (kk % MAX_CMD_ITEM_MAX), 1);
		kk++;
		if (kk % MAX_CMD_ITEM_MAX == 0) {
			wlanCfgSetGetFw(prAdapter, cmdBuffer, MAX_CMD_ITEM_MAX, CMD_TYPE_SET);
			kalMemSet(cmdBuffer, 0, MAX_CMD_BUFFER_LENGTH);
		}
	}
	if (kk % MAX_CMD_ITEM_MAX)
		wlanCfgSetGetFw(prAdapter, cmdBuffer, (kk % MAX_CMD_ITEM_MAX), CMD_TYPE_SET);

	kalMemFree(cmdBuffer, VIR_MEM_TYPE, MAX_CMD_BUFFER_LENGTH);
	return WLAN_STATUS_SUCCESS;
}

WLAN_STATUS wlanFwFileCfg(IN P_ADAPTER_T prAdapter)
{
	UINT_32 u4FwCfgReadLen = 0;
	PUINT_8 pucFwCfgBuf = (PUINT_8) kalMemAlloc(WLAN_CFG_FILE_BUF_SIZE, VIR_MEM_TYPE);
	INT32 ret = WLAN_STATUS_FAILURE;

	if (!pucFwCfgBuf) {
		DBGLOG(INIT, INFO, "omega, pucFwCfgBuf alloc fail!");
		return WLAN_STATUS_FAILURE;
	}

	kalMemZero(pucFwCfgBuf, WLAN_CFG_FILE_BUF_SIZE);

	if (kalReadToFile(FW_CFG_FILE_1, pucFwCfgBuf,
		WLAN_CFG_FILE_BUF_SIZE, &u4FwCfgReadLen) == 0)
		DBGLOG(INIT, INFO, "read wifi_fw.cfg :%s\n", FW_CFG_FILE_1);
	else if (kalReadToFile(FW_CFG_FILE_2, pucFwCfgBuf,
		WLAN_CFG_FILE_BUF_SIZE, &u4FwCfgReadLen) == 0)
		DBGLOG(INIT, INFO, "read wifi_fw.cfg :%s\n", FW_CFG_FILE_2);
	else if (kalReadToFile(FW_CFG_FILE_3, pucFwCfgBuf,
		WLAN_CFG_FILE_BUF_SIZE, &u4FwCfgReadLen) == 0)
		DBGLOG(INIT, INFO, "read wifi_fw.cfg :%s\n", FW_CFG_FILE_3);

	if (pucFwCfgBuf[0] != '\0' && u4FwCfgReadLen > 0) {
		/* Here limited the file length < 2048, bcz only for dbg purpose
		 * Meanwhile, if the file length == 2048, it MAY cause the last
		 * several <= 4 cmd failed
		 */
		if (u4FwCfgReadLen == WLAN_CFG_FILE_BUF_SIZE)
			pucFwCfgBuf[WLAN_CFG_FILE_BUF_SIZE - 1] = '\0';

		wlanFwCfgParse(prAdapter, pucFwCfgBuf);

		ret = WLAN_STATUS_SUCCESS;
	} else {
		DBGLOG(INIT, INFO, "read wifi_fw.cfg fail\n");
		ret = WLAN_STATUS_FAILURE;
	}

	kalMemFree(pucFwCfgBuf, VIR_MEM_TYPE, WLAN_CFG_FILE_BUF_SIZE);
	return ret;

}

WLAN_STATUS wlanFwCfgParse(IN P_ADAPTER_T prAdapter, PUINT_8 pucConfigBuf)
{
	/* here return a list should be better */
	char *saveptr1, *saveptr2;
	char *cfgItems = pucConfigBuf;
	UINT_8 cmdNum = 0;

	PUINT_8 cmdBuffer = kalMemAlloc(MAX_CMD_BUFFER_LENGTH, VIR_MEM_TYPE);

	if (cmdBuffer == 0) {
		DBGLOG(INIT, INFO, "omega, cmd buffer return fail!");
		return WLAN_STATUS_FAILURE;
	}
	kalMemSet(cmdBuffer, 0, MAX_CMD_BUFFER_LENGTH);

	while (1) {
		char *keyStr = NULL;
		char *valueStr = NULL;
		char *cfgEntry = strtok_r(cfgItems, "\n\r", &saveptr1);

		if (!cfgEntry) {
			if (cmdNum)
				wlanCfgSetGetFw(prAdapter, cmdBuffer, cmdNum, CMD_TYPE_SET);

			if (cmdBuffer)
				kalMemFree(cmdBuffer, VIR_MEM_TYPE, MAX_CMD_BUFFER_LENGTH);
			return WLAN_STATUS_SUCCESS;
		}
		cfgItems = NULL;

		keyStr = strtok_r(cfgEntry, " \t", &saveptr2);
		valueStr = strtok_r(NULL, "\0", &saveptr2);

		/* maybe a blank line, but with some tab or whitespace */
		if (!keyStr)
			continue;

		/* here take '#' at the beginning of line as comment */
		if (keyStr[0] == '#')
			continue;

		/* remove the \t " " at the beginning of the valueStr */
		while (valueStr && (*valueStr == '\t' || *valueStr == ' '))
			valueStr++;

		if (keyStr && valueStr) {
			wlanCfgFwSetParam(cmdBuffer, keyStr, valueStr, cmdNum, 1);
			cmdNum++;
			if (cmdNum == MAX_CMD_ITEM_MAX) {
				wlanCfgSetGetFw(prAdapter, cmdBuffer, MAX_CMD_ITEM_MAX, CMD_TYPE_SET);
				kalMemSet(cmdBuffer, 0, MAX_CMD_BUFFER_LENGTH);
				cmdNum = 0;
			}
		} else {
			/* here will not to try send the cmd has been parsed, but not sent yet */
			if (cmdBuffer)
				kalMemFree(cmdBuffer, VIR_MEM_TYPE, MAX_CMD_BUFFER_LENGTH);
			return WLAN_STATUS_FAILURE;
		}
	}
}

/*
 * This func is mainly from bionic's strtok.c
 */
static char *strtok_r(char *s, const char *delim, char **last)
{
	char *spanp;
	int c, sc;
	char *tok;


	if (s == NULL) {
		s = *last;
		if (s == 0)
			return 0;
	}
cont:
	c = *s++;
	for (spanp = (char *)delim; (sc = *spanp++) != 0;) {
		if (c == sc)
			goto cont;
	}

	if (c == 0) {		/* no non-delimiter characters */
		*last = NULL;
		return NULL;
	}
	tok = s - 1;

	for (;;) {
		c = *s++;
		spanp = (char *)delim;
		do {
			sc = *spanp++;
			if (sc == c) {
				if (c == 0)
					s = NULL;
				else
					s[-1] = 0;
				*last = s;
				return tok;
			}
		} while (sc != 0);
	}
}
