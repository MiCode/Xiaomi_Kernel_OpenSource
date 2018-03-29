/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#include "wmt_cfg_parser.h"

/*******************************************************************************
*                             P U B L I C   D A T A
********************************************************************************
*/

unsigned int wmtCcciLogLvl = WMT_CCCI_LOG_INFO;
WMT_PARSER_CONF_FOR_CCCI gWmtCfgForCCCI;

/*******************************************************************************
*                         D A T A   T Y P E S
********************************************************************************
*/
struct parse_data {
	char *name;
	int (*parser)(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data, const char *value);
	char *(*writer)(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data);
	/*PCHAR param1, *param2, *param3; */
	char *param1;
	char *param2;
	char *param3;
};

/******************************************************************************
*                   F U N C T I O N   D E C L A R A T I O N S
*******************************************************************************
*/
static int wmt_conf_parse_char(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data, const char *pos);

static char *wmt_conf_write_char(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data);

#if 0
static int wmt_conf_parse_short(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data, const char *pos);

static char *wmt_conf_write_short(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data);
#endif

static int wmt_conf_parse_int(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data, const char *pos);

static char *wmt_conf_write_int(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data);

static int wmt_conf_parse_pair(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const char *pKey, const char *pVal);

static int wmt_conf_parse(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const char *pInBuf, unsigned int size);

static int wmt_conf_read_file(void);

static int wmt_conf_patch_get(unsigned char *pPatchName, struct firmware **ppPatch, int padSzBuf);

static int wmt_conf_patch_put(struct firmware **ppPatch);

static int wmt_conf_read_file_from_fs(unsigned char *pName, const u8 **ppBufPtr, int offset, int padSzBuf);
/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/

#define OFFSET(v) ((void *) &((P_WMT_PARSER_CONF_FOR_CCCI) 0)->v)

#define CHAR(f) \
{ \
	#f, \
	wmt_conf_parse_char, \
	wmt_conf_write_char, \
	OFFSET(rWmtCfgFile.f), \
	NULL, \
	NULL \
}

#define INT(f) \
{ \
	#f, \
	wmt_conf_parse_int, \
	wmt_conf_write_int, \
	OFFSET(rWmtCfgFile.f), \
	NULL, \
	NULL \
}

static const struct parse_data wmtcfg_fields[] = {
	CHAR(coex_wmt_ant_mode),

	CHAR(wmt_gps_lna_pin),
	CHAR(wmt_gps_lna_enable),

	INT(co_clock_flag),

};

#define NUM_WMTCFG_FIELDS (sizeof(wmtcfg_fields) / sizeof(wmtcfg_fields[0]))

/*******************************************************************************
*                          F U N C T I O N S
********************************************************************************
*/

static int wmt_conf_parse_char(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data, const char *pos)
{
	unsigned char *dst;
	long res;
	int ret;

	dst = (char *)(((unsigned char *)pWmtCcci) + (signed long)data->param1);

	if ((strlen(pos) > 2) && ((*pos) == '0') && (*(pos + 1) == 'x')) {
		ret = kstrtol(pos + 2, 16, &res);
		if (ret)
			WMT_CCCI_ERR_FUNC("fail(%d)\n", ret);
		*dst = res;
		WMT_CCCI_DBG_FUNC("wmtcfg==> %s=0x%x\n", data->name, *dst);
	} else {
		ret = kstrtol(pos, 10, &res);
		if (ret)
			WMT_CCCI_ERR_FUNC("fail(%d)\n", ret);
		*dst = res;
		WMT_CCCI_DBG_FUNC("wmtcfg==> %s=%d\n", data->name, *dst);
	}
	return 0;
}

static char *wmt_conf_write_char(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data)
{
	char *src;
	int res;
	char *value;

	src = (char *)(((unsigned char *)pWmtCcci) + (long)data->param1);

	value = vmalloc(20);
	if (value == NULL)
		return NULL;
	res = snprintf(value, 20, "0x%x", *src);
	if (res < 0 || res >= 20) {
		vfree(value);
		return NULL;
	}
	value[20 - 1] = '\0';
	return value;
}

static int wmt_conf_parse_int(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data, const char *pos)
{
	unsigned int *dst;
	long res;
	int ret;

	dst = (int *)(((unsigned char *)pWmtCcci) + (long)data->param1);

	/* WMT_INFO_FUNC(">strlen(pos)=%d\n", strlen(pos)); */

	if ((strlen(pos) > 2) && ((*pos) == '0') && (*(pos + 1) == 'x')) {
		ret = kstrtol(pos + 2, 16, &res);
		if (ret)
			WMT_CCCI_ERR_FUNC("fail(%d)\n", ret);
		*dst = res;
		WMT_CCCI_DBG_FUNC("wmtcfg==> %s=0x%x\n", data->name, *dst);
	} else {
		ret = kstrtol(pos, 10, &res);
		if (ret)
			WMT_CCCI_ERR_FUNC("fail(%d)\n", ret);
		*dst = res;
		WMT_CCCI_DBG_FUNC("wmtcfg==> %s=%d\n", data->name, *dst);
	}

	return 0;
}

static char *wmt_conf_write_int(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const struct parse_data *data)
{
	int *src;
	int res;
	char *value;

	src = (unsigned int *)(((unsigned char *)pWmtCcci) + (long)data->param1);

	value = vmalloc(20);
	if (value == NULL)
		return NULL;
	res = snprintf(value, 20, "0x%x", *src);
	if (res < 0 || res >= 20) {
		vfree(value);
		return NULL;
	}
	value[20 - 1] = '\0';
	return value;
}

static int wmt_conf_parse_pair(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const char *pKey, const char *pVal)
{
	int i = 0;
	int ret = 0;

	/* WMT_INFO_FUNC( DBG_NAME "cfg(%s) val(%s)\n", pKey, pVal); */

	for (i = 0; i < NUM_WMTCFG_FIELDS; i++) {
		const struct parse_data *field = &wmtcfg_fields[i];

		if (strcmp(pKey, field->name) != 0)
			continue;
		if (field->parser(pWmtCcci, field, pVal)) {
			WMT_CCCI_ERR_FUNC("failed to parse %s '%s'.\n", pKey, pVal);
			ret = -1;
		}
		break;
	}
	if (i == NUM_WMTCFG_FIELDS) {
		WMT_CCCI_ERR_FUNC("unknown field '%s'.\n", pKey);
		ret = -1;
	}

	return ret;
}

static int wmt_conf_parse(P_WMT_PARSER_CONF_FOR_CCCI pWmtCcci, const char *pInBuf, unsigned int size)
{
	char *pch;
	char *pBuf;
	char *pLine;
	char *pKey;
	char *pVal;
	char *pPos;
	int ret = 0;
	int i = 0;
	char *pa = NULL;

	pBuf = vmalloc(size);
	if (!pBuf)
		return -1;

	memcpy(pBuf, pInBuf, size);
	pBuf[size] = '\0';

	pch = pBuf;
	/* pch is to be updated by strsep(). Keep pBuf unchanged!! */

#if 0
	{
		PCHAR buf_ptr = pBuf;
		INT32 k = 0;

		WMT_INFO_FUNC("%s len=%d", "wmcfg.content:", size);
		for (k = 0; k < size; k++) {
			/* if(k%16 == 0)  WMT_INFO_FUNC("\n"); */
			WMT_INFO_FUNC("%c", buf_ptr[k]);
		}
		WMT_INFO_FUNC("--end\n");
	}
#endif

	while ((pLine = strsep(&pch, "\r\n")) != NULL) {
		/* pch is updated to the end of pLine by strsep() and updated to '\0' */
		/*WMT_INFO_FUNC("strsep offset(%d), char(%d, '%c' )\n", pLine-pBuf, *pLine, *pLine); */
		/* parse each line */

		/* WMT_INFO_FUNC("==> Line = (%s)\n", pLine); */

		if (!*pLine)
			continue;

		pVal = strchr(pLine, '=');
		if (!pVal) {
			WMT_CCCI_WARN_FUNC("mal-format cfg string(%s)\n", pLine);
			continue;
		}

		/* |<-pLine->|'='<-pVal->|'\n' ('\0')|  */
		*pVal = '\0';	/* replace '=' with '\0' to get key */
		/* |<-pKey->|'\0'|<-pVal->|'\n' ('\0')|  */
		pKey = pLine;

		if ((pVal - pBuf) < size)
			pVal++;

		/*key handling */
		pPos = pKey;
		/*skip space characeter */
		while (((*pPos) == ' ') || ((*pPos) == '\t') || ((*pPos) == '\n')) {
			if ((pPos - pBuf) >= size)
				break;
			pPos++;
		}
		/*key head */
		pKey = pPos;
		while (((*pPos) != ' ') && ((*pPos) != '\t') && ((*pPos) != '\0')
		       && ((*pPos) != '\n')) {
			if ((pPos - pBuf) >= size)
				break;
			pPos++;
		}
		/*key tail */
		(*pPos) = '\0';

		/*value handling */
		pPos = pVal;
		/*skip space characeter */
		while (((*pPos) == ' ') || ((*pPos) == '\t') || ((*pPos) == '\n')) {
			if ((pPos - pBuf) >= size)
				break;
			pPos++;
		}
		/*value head */
		pVal = pPos;
		while (((*pPos) != ' ') && ((*pPos) != '\t') && ((*pPos) != '\0')
		       && ((*pPos) != '\n')) {
			if ((pPos - pBuf) >= size)
				break;
			pPos++;
		}
		/*value tail */
		(*pPos) = '\0';

		/* WMT_DBG_FUNC("parse (key: #%s#, value: #%s#)\n", pKey, pVal); */
		ret = wmt_conf_parse_pair(pWmtCcci, pKey, pVal);
		WMT_CCCI_WARN_FUNC("parse (%s, %s, %d)\n", pKey, pVal, ret);
		if (ret)
			WMT_CCCI_WARN_FUNC("parse fail (%s, %s, %d)\n", pKey, pVal, ret);
	}

	for (i = 0; i < NUM_WMTCFG_FIELDS; i++) {
		const struct parse_data *field = &wmtcfg_fields[i];

		pa = field->writer(pWmtCcci, field);
		if (pa) {
			WMT_CCCI_INFO_FUNC("#%d(%s)=>%s\n", i, field->name, pa);
			vfree(pa);
		} else {
			WMT_CCCI_ERR_FUNC("failed to parse '%s'.\n", field->name);
		}
	}
	vfree(pBuf);
	return 0;
}

static int wmt_conf_read_file(void)
{
	int ret = -1;

	memset(&gWmtCfgForCCCI.rWmtCfgFile, 0, sizeof(gWmtCfgForCCCI.rWmtCfgFile));
	memset(&gWmtCfgForCCCI.pWmtCfg, 0, sizeof(gWmtCfgForCCCI.pWmtCfg));
	memset(&gWmtCfgForCCCI.cWmtCfgName[0], 0, sizeof(gWmtCfgForCCCI.cWmtCfgName));
	strncat(&(gWmtCfgForCCCI.cWmtCfgName[0]), WMT_CFG_FILE_PREFIX, sizeof(WMT_CFG_FILE_PREFIX));
	strncat(&(gWmtCfgForCCCI.cWmtCfgName[0]), WMT_CFG_FILE, sizeof(WMT_CFG_FILE));

	if (!strlen(&(gWmtCfgForCCCI.cWmtCfgName[0]))) {
		WMT_CCCI_ERR_FUNC("empty Wmtcfg name\n");
		wmt_ccci_assert(0);
		return ret;
	}
	WMT_CCCI_INFO_FUNC("WMT config file:%s\n", &(gWmtCfgForCCCI.cWmtCfgName[0]));
	if (0 == wmt_conf_patch_get(&gWmtCfgForCCCI.cWmtCfgName[0], (struct firmware **)&gWmtCfgForCCCI.pWmtCfg, 0)) {
		/*get full name patch success */
		WMT_CCCI_INFO_FUNC("get full file name(%s) buf(0x%p) size(%zd)\n",
				   &gWmtCfgForCCCI.cWmtCfgName[0], gWmtCfgForCCCI.pWmtCfg->data,
				   gWmtCfgForCCCI.pWmtCfg->size);

		if (0 == wmt_conf_parse(&gWmtCfgForCCCI, (const char *)gWmtCfgForCCCI.pWmtCfg->data,
					gWmtCfgForCCCI.pWmtCfg->size)) {
			/*config file exists */
			gWmtCfgForCCCI.rWmtCfgFile.cfgExist = 1;

			WMT_CCCI_INFO_FUNC("&gWmtCfgForCCCI.rWmtCfgFile=%p\n", &gWmtCfgForCCCI.rWmtCfgFile);
			ret = 0;
		} else {
			WMT_CCCI_ERR_FUNC("wmt conf parsing fail\n");
			wmt_ccci_assert(0);
			ret = -1;
		}
		wmt_conf_patch_put((struct firmware **)&gWmtCfgForCCCI.pWmtCfg);

		return ret;
	}
	WMT_CCCI_ERR_FUNC("read %s file fails\n", &(gWmtCfgForCCCI.cWmtCfgName[0]));
	wmt_ccci_assert(0);

	gWmtCfgForCCCI.rWmtCfgFile.cfgExist = 0;
	return ret;

}

static int wmt_conf_patch_get(unsigned char *pPatchName, struct firmware **ppPatch, int padSzBuf)
{
	int iRet = -1;
	struct firmware *pfw;
	uid_t orig_uid;
	gid_t orig_gid;

	struct cred *cred = (struct cred *)get_current_cred();

	mm_segment_t orig_fs = get_fs();

	if (*ppPatch) {
		WMT_CCCI_WARN_FUNC("f/w patch already exists\n");
		if ((*ppPatch)->data)
			vfree((*ppPatch)->data);

		kfree(*ppPatch);
		*ppPatch = NULL;
	}

	if (!strlen(pPatchName)) {
		WMT_CCCI_ERR_FUNC("empty f/w name\n");
		wmt_ccci_assert((strlen(pPatchName) > 0));
		return -1;
	}

	pfw = kzalloc(sizeof(struct firmware), /*GFP_KERNEL */ GFP_ATOMIC);
	if (!pfw) {
		WMT_CCCI_ERR_FUNC("kzalloc(%zd) fail\n", sizeof(struct firmware));
		return -2;
	}

	orig_uid = cred->fsuid;
	orig_gid = cred->fsgid;
	cred->fsuid = cred->fsgid = 0;

	set_fs(get_ds());

	/* load patch file from fs */
	iRet = wmt_conf_read_file_from_fs(pPatchName, &pfw->data, 0, padSzBuf);
	set_fs(orig_fs);

	cred->fsuid = orig_uid;
	cred->fsgid = orig_gid;

	if (iRet > 0) {
		pfw->size = iRet;
		*ppPatch = pfw;
		WMT_CCCI_DBG_FUNC("load (%s) to addr(0x%p) success\n", pPatchName, pfw->data);
		return 0;
	}
	kfree(pfw);
	*ppPatch = NULL;
	WMT_CCCI_ERR_FUNC("load file (%s) fail, iRet(%d)\n", pPatchName, iRet);
	return -1;
}

static int wmt_conf_patch_put(struct firmware **ppPatch)
{
	if (NULL != *ppPatch) {
		if ((*ppPatch)->data)
			vfree((*ppPatch)->data);
		kfree(*ppPatch);
		*ppPatch = NULL;
	}
	return 0;
}

static int wmt_conf_read_file_from_fs(unsigned char *pName, const u8 **ppBufPtr, int offset, int padSzBuf)
{
	int iRet = -1;
	struct file *fd;
	/* ssize_t iRet; */
	int file_len;
	int read_len;
	void *pBuf;

	/* struct cred *cred = get_task_cred(current); */
	const struct cred *cred = get_current_cred();

	if (!ppBufPtr) {
		WMT_CCCI_ERR_FUNC("invalid ppBufptr!\n");
		return -1;
	}
	*ppBufPtr = NULL;

	fd = filp_open(pName, O_RDONLY, 0);
	if (!fd || IS_ERR(fd) || !fd->f_op || !fd->f_op->read) {
		WMT_CCCI_ERR_FUNC("failed to open or read!(0x%p, %ld, %d, %d)\n", fd, PTR_ERR(fd),
				  cred->fsuid, cred->fsgid);
		if (IS_ERR(fd))
			WMT_CCCI_ERR_FUNC("error code:%ld\n", PTR_ERR(fd));
		return -1;
	}

	file_len = fd->f_path.dentry->d_inode->i_size;
	pBuf = vmalloc((file_len + 3) & ~0x3UL);
	if (!pBuf) {
		WMT_CCCI_ERR_FUNC("failed to vmalloc(%d)\n", (int)((file_len + 3) & ~0x3UL));
		goto read_file_done;
	}

	do {
		if (fd->f_pos != offset) {
			if (fd->f_op->llseek) {
				if (fd->f_op->llseek(fd, offset, 0) != offset) {
					WMT_CCCI_ERR_FUNC("failed to seek!!\n");
					goto read_file_done;
				}
			} else {
				fd->f_pos = offset;
			}
		}

		read_len = fd->f_op->read(fd, pBuf + padSzBuf, file_len, &fd->f_pos);
		if (read_len != file_len)
			WMT_CCCI_WARN_FUNC("read abnormal: read_len(%d), file_len(%d)\n", read_len, file_len);

	} while (false);

	iRet = 0;
	*ppBufPtr = pBuf;

read_file_done:
	if (iRet) {
		if (pBuf)
			vfree(pBuf);
	}

	filp_close(fd, NULL);

	return (iRet) ? iRet : read_len;
}

unsigned int wmt_get_coclock_setting_for_ccci(void)
{
	unsigned int value_from_cfg;
	char clk_type;
	char clk_buffer_index;
	int array_index;
	int iRet = -1;

	static const char * const clk_type_name[] = {
		"TCXO",
		"GPS_coclk",
		"coDCXO",
		"coVCTCXO"
	};

	iRet = wmt_conf_read_file();
	if (iRet) {
		WMT_CCCI_ERR_FUNC("parser WMT_SOC.cfg fail(%d)\n", iRet);
		return -1;
	}

	value_from_cfg = gWmtCfgForCCCI.rWmtCfgFile.co_clock_flag;
	clk_type = value_from_cfg & 0x000f;
	clk_buffer_index = ((value_from_cfg & 0x00f0) >> 4);
	array_index = clk_type;

	WMT_CCCI_INFO_FUNC("value_from_cfg(%d),clk_type(%d,%s),clk_buffer_index(%d)\n",
			   value_from_cfg, clk_type, clk_type_name[array_index], clk_buffer_index);

	return value_from_cfg;
}
EXPORT_SYMBOL(wmt_get_coclock_setting_for_ccci);
