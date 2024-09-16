/*  SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

/*******************************************************************************
*                         C O M P I L E R   F L A G S
********************************************************************************
*/

/*******************************************************************************
*                                 M A C R O S
********************************************************************************
*/
#include "osal_typedef.h"
#include "wmt_user_proc.h"
#include "wmt_lib.h"
#include "stp_core.h"
#include "connsys_debug_utility.h"
#include "wmt_alarm.h"

#ifdef DFT_TAG
#undef DFT_TAG
#define DFT_TAG         "[WMT-DEV]"
#endif

#define WMT_USER_PROCNAME "driver/wmt_user_proc"
static struct proc_dir_entry *gWmtUserProcEntry;

static ssize_t wmt_user_proc_write(struct file *filp, const char __user *buf, size_t count, loff_t *f_pos);

static INT32 wmt_user_proc_func_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_user_proc_wmt_assert_ctrl(INT32 par1, INT32 par2, INT32 par3);
static INT32 wmt_user_proc_suspend_debug(INT32 par1, INT32 offset, INT32 size);
static INT32 wmt_user_proc_alarm_ctrl(INT32 par1, INT32 offset, INT32 size);
static INT32 wmt_user_proc_set_bt_link_status(INT32 par1, INT32 par2, INT32 par3);

static const WMT_DEV_USER_PROC_FUNC wmt_dev_user_proc_func[] = {
	[0x0] = wmt_user_proc_func_ctrl,
	[0x1] = wmt_user_proc_wmt_assert_ctrl,
	[0x2] = wmt_user_proc_suspend_debug,
	[0x3] = wmt_user_proc_set_bt_link_status,
	[0x4] = wmt_user_proc_alarm_ctrl,
};

INT32 wmt_user_proc_func_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	MTK_WCN_BOOL ret = MTK_WCN_BOOL_FALSE;

	if (par2 < WMTDRV_TYPE_WMT || par2 == WMTDRV_TYPE_LPBK) {
		if (par3 == 0) {
			WMT_INFO_FUNC("function off test, type(%d)\n", par2);
			ret = mtk_wcn_wmt_func_off(par2);
		} else {
			WMT_INFO_FUNC("function on test, type(%d)\n", par2);
			ret = mtk_wcn_wmt_func_on(par2);
		}
		WMT_INFO_FUNC("function test return %d\n", ret);
	} else
		WMT_INFO_FUNC("function ctrl test, invalid type(%d)\n", par2);

	return 0;
}

INT32 wmt_user_proc_wmt_assert_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	if (par2 > 2 || par2 < 0)
		return -1;

	mtk_wcn_stp_coredump_flag_ctrl(par2);

	return 0;
}

/********************************************************/
/* par2:       */
/*     0: Off  */
/*     others: alarm time (seconds) */
/********************************************************/
static INT32 wmt_user_proc_suspend_debug(INT32 par1, INT32 par2, INT32 par3)
{
	if (par2 > 0)
		connsys_log_alarm_enable(par2);
	else
		connsys_log_alarm_disable();
	return 0;
}

static INT32 wmt_user_proc_alarm_ctrl(INT32 par1, INT32 par2, INT32 par3)
{
	if (par2 > 0)
		wmt_alarm_start(par2);
	else
		wmt_alarm_cancel();
	return 0;
}

static INT32 wmt_user_proc_set_bt_link_status(INT32 par1, INT32 par2, INT32 par3)
{
	if (par2 != 0 && par2 != 1)
		return 0;

	wmt_lib_set_bt_link_status(par2, par3);
	return 0;
}

ssize_t wmt_user_proc_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
	ULONG len = count;
	INT8 buf[256];
	PINT8 pBuf;
	INT32 x = 0, y = 0, z = 0;
	PINT8 pToken = NULL;
	PINT8 pDelimiter = " \t";
	LONG res = 0;

	WMT_INFO_FUNC("write parameter len = %d\n\r", (INT32) len);
	if (len >= osal_sizeof(buf)) {
		WMT_ERR_FUNC("input handling fail!\n");
		len = osal_sizeof(buf) - 1;
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	WMT_INFO_FUNC("write parameter data = %s\n\r", buf);

	pBuf = buf;
	pToken = osal_strsep(&pBuf, pDelimiter);
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		x = (INT32)res;
	} else {
		x = 0;
	}

	pToken = osal_strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		osal_strtol(pToken, 16, &res);
		y = (INT32)res;
		WMT_INFO_FUNC("y = 0x%08x\n\r", y);
	} else {
		y = 3000;
	}

	pToken = osal_strsep(&pBuf, "\t\n ");
	if (pToken != NULL) {
		if (0x3 == x)
			z = osal_strcmp(pToken, "true") ? 0 : 1;
		else {
			osal_strtol(pToken, 16, &res);
			z = (INT32)res;
		}
	} else {
		z = 10;
	}

	WMT_INFO_FUNC("x(0x%08x), y(0x%08x), z(0x%08x)\n\r", x, y, z);

	if (osal_array_size(wmt_dev_user_proc_func) > x && NULL != wmt_dev_user_proc_func[x])
		(*wmt_dev_user_proc_func[x]) (x, y, z);
	else
		WMT_WARN_FUNC("no handler defined for command id(0x%08x)\n\r", x);

	return len;
}

INT32 wmt_dev_user_proc_setup(VOID)
{
	static const struct file_operations wmt_user_proc_fops = {
		.owner = THIS_MODULE,
		.write = wmt_user_proc_write,
	};
	INT32 i_ret = 0;

	gWmtUserProcEntry = proc_create(WMT_USER_PROCNAME, 0664, NULL, &wmt_user_proc_fops);
	if (gWmtUserProcEntry == NULL) {
		WMT_ERR_FUNC("Unable to create / wmt_user_proc proc entry\n\r");
		i_ret = -1;
	}

	return i_ret;
}

INT32 wmt_dev_user_proc_remove(VOID)
{
	if (gWmtUserProcEntry != NULL)
		proc_remove(gWmtUserProcEntry);
	gWmtUserProcEntry = NULL;

	return 0;
}
