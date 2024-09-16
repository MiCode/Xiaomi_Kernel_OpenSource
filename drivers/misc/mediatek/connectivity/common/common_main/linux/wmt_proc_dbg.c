/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/proc_fs.h>
#include "osal.h"
#include "wmt_core.h"
#include "wmt_lib.h"
#include "wmt_proc_dbg.h"

#if CFG_WMT_PROC_FOR_AEE
static struct proc_dir_entry *gWmtAeeEntry;
#define WMT_AEE_PROCNAME "driver/wmt_aee"
#define WMT_PROC_AEE_SIZE 3072
static UINT32 g_buf_len;
static PUINT8 pBuf;

static OSAL_SLEEPABLE_LOCK g_aee_read_lock;
#endif


#if CFG_WMT_PROC_FOR_DUMP_INFO

static struct proc_dir_entry *gWmtdumpinfoEntry;
#define WMT_DUMP_INFO_PROCNAME "driver/wmt_dump_info"
static CONSYS_STATE_DMP_INFO g_dmp_info;
static UINT32 g_dump_info_buf_len;
static UINT8 dmp_info_buf[DBG_LOG_STR_SIZE];
static PUINT8 dmp_info_buf_ptr;

static OSAL_SLEEPABLE_LOCK g_dump_info_read_lock;

static ssize_t wmt_dev_proc_for_dump_info_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	INT32 retval = 0;
	UINT32 len = 0;
	INT32 i, dmp_cnt = 5;

	WMT_INFO_FUNC("%s: count %lu pos %lld\n", __func__, count, *f_pos);

	if (osal_lock_sleepable_lock(&g_dump_info_read_lock)) {
		WMT_ERR_FUNC("lock failed\n");
		return 0;
	}

	if (*f_pos == 0) {
		g_dump_info_buf_len = 0;
		if (wmt_lib_dmp_consys_state(&g_dmp_info, dmp_cnt, 0) == MTK_WCN_BOOL_TRUE) {
			g_dump_info_buf_len = len;
			memset(dmp_info_buf, '\0', DBG_LOG_STR_SIZE);
			g_dump_info_buf_len = 0;
			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
						DBG_LOG_STR_SIZE - g_dump_info_buf_len,
						"0x%08x", g_dmp_info.cpu_pcr[0]);

			for (i = 1; i < dmp_cnt; i++)
				g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
						DBG_LOG_STR_SIZE - g_dump_info_buf_len,
						";0x%08x", g_dmp_info.cpu_pcr[i]);

			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
					DBG_LOG_STR_SIZE - g_dump_info_buf_len, ";0x%08x", g_dmp_info.state.lp[1]);

			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
					DBG_LOG_STR_SIZE - g_dump_info_buf_len, ";0x%08x", g_dmp_info.state.gating[1]);

			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
						DBG_LOG_STR_SIZE - len,
						";[0x%08x", g_dmp_info.state.sw_state.info_time);

			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
						DBG_LOG_STR_SIZE - len,
						";0x%08x", g_dmp_info.state.sw_state.is_gating);
			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
						DBG_LOG_STR_SIZE - len,
						";0x%08x", g_dmp_info.state.sw_state.resource_disable_sleep);
			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
						DBG_LOG_STR_SIZE - len,
						";0x%08x", g_dmp_info.state.sw_state.clock_hif_ctrl);
			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
						DBG_LOG_STR_SIZE - len,
						";0x%08x", g_dmp_info.state.sw_state.clock_umac_ctrl);
			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
						DBG_LOG_STR_SIZE - len,
						";0x%08x", g_dmp_info.state.sw_state.clock_mcu);
			g_dump_info_buf_len += snprintf(dmp_info_buf + g_dump_info_buf_len,
						DBG_LOG_STR_SIZE - len,
						";0x%08x]", g_dmp_info.state.sw_state.sub_system);


			dmp_info_buf_ptr = dmp_info_buf;
		}

		WMT_INFO_FUNC("wmt_dev:wmt for dump info buffer len(%d)\n", g_dump_info_buf_len);
	}

	if (g_dump_info_buf_len >= count) {
		retval = copy_to_user(buf, dmp_info_buf_ptr, count);
		if (retval) {
			WMT_ERR_FUNC("copy to dump info buffer failed, ret:%d\n", retval);
			retval = -EFAULT;
			goto dump_info_err_exit;
		}

		*f_pos += count;
		g_dump_info_buf_len -= count;
		dmp_info_buf_ptr += count;
		WMT_INFO_FUNC("wmt_dev:after read,wmt for dump info buffer len(%d)\n", g_dump_info_buf_len);

		retval = count;
	} else if (g_dump_info_buf_len != 0) {
		retval = copy_to_user(buf, dmp_info_buf_ptr, g_dump_info_buf_len);
		if (retval) {
			WMT_ERR_FUNC("copy to dump info buffer failed, ret:%d\n", retval);
			retval = -EFAULT;
			goto dump_info_err_exit;
		}

		*f_pos += g_dump_info_buf_len;
		len = g_dump_info_buf_len;
		g_dump_info_buf_len = 0;
		dmp_info_buf_ptr += len;
		retval = len;
		WMT_INFO_FUNC("wmt_dev:after read,wmt for dump info buffer len(%d)\n", g_dump_info_buf_len);
	} else {
		WMT_INFO_FUNC("wmt_dev: no data available for dump info\n");
		retval = 0;
	}

dump_info_err_exit:
	osal_unlock_sleepable_lock(&g_dump_info_read_lock);

	return retval;
}

static ssize_t wmt_dev_proc_for_dump_info_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	WMT_TRC_FUNC();
	return 0;
}

INT32 wmt_dev_proc_for_dump_info_setup(VOID)
{
	static const struct file_operations wmt_dump_info_fops = {
		.owner = THIS_MODULE,
		.read = wmt_dev_proc_for_dump_info_read,
		.write = wmt_dev_proc_for_dump_info_write,
	};

	osal_sleepable_lock_init(&g_dump_info_read_lock);

	gWmtdumpinfoEntry = proc_create(WMT_DUMP_INFO_PROCNAME, 0664, NULL, &wmt_dump_info_fops);
	if (gWmtdumpinfoEntry == NULL) {
		WMT_ERR_FUNC("Unable to create /proc entry\n\r");
		return -1;
	}

	return 0;
}

INT32 wmt_dev_proc_for_dump_info_remove(VOID)
{
	if (gWmtdumpinfoEntry != NULL)
		remove_proc_entry(WMT_DUMP_INFO_PROCNAME, NULL);
	osal_sleepable_lock_deinit(&g_dump_info_read_lock);

	return 0;
}
#endif /* CFG_WMT_PROC_FOR_DUMP_INFO */

#if CFG_WMT_PROC_FOR_AEE
static ssize_t wmt_dev_proc_for_aee_read(struct file *filp, char __user *buf, size_t count,
		loff_t *f_pos)
{
	INT32 retval = 0;
	UINT32 len = 0;

	WMT_INFO_FUNC("%s: count %lu pos %lld\n", __func__, count, *f_pos);

	if (osal_lock_sleepable_lock(&g_aee_read_lock)) {
		WMT_ERR_FUNC("lock failed\n");
		return 0;
	}

	if (*f_pos == 0) {
		pBuf = wmt_lib_get_cpupcr_xml_format(&len);
		g_buf_len = len;
		WMT_INFO_FUNC("wmt_dev:wmt for aee buffer len(%d)\n", g_buf_len);
	}

	if (g_buf_len >= count) {
		retval = copy_to_user(buf, pBuf, count);
		if (retval) {
			WMT_ERR_FUNC("copy to aee buffer failed, ret:%d\n", retval);
			retval = -EFAULT;
			goto err_exit;
		}

		*f_pos += count;
		g_buf_len -= count;
		pBuf += count;
		WMT_INFO_FUNC("wmt_dev:after read,wmt for aee buffer len(%d)\n", g_buf_len);

		retval = count;
	} else if (g_buf_len != 0) {
		retval = copy_to_user(buf, pBuf, g_buf_len);
		if (retval) {
			WMT_ERR_FUNC("copy to aee buffer failed, ret:%d\n", retval);
			retval = -EFAULT;
			goto err_exit;
		}

		*f_pos += g_buf_len;
		len = g_buf_len;
		g_buf_len = 0;
		pBuf += len;
		retval = len;
		WMT_INFO_FUNC("wmt_dev:after read,wmt for aee buffer len(%d)\n", g_buf_len);
	} else {
		WMT_INFO_FUNC("wmt_dev: no data available for aee\n");
		retval = 0;
	}

err_exit:
	osal_unlock_sleepable_lock(&g_aee_read_lock);

	return retval;
}

static ssize_t wmt_dev_proc_for_aee_write(struct file *filp, const char __user *buf, size_t count,
		loff_t *f_pos)
{
	WMT_TRC_FUNC();
	return 0;
}

INT32 wmt_dev_proc_for_aee_setup(VOID)
{
	static const struct file_operations wmt_aee_fops = {
		.owner = THIS_MODULE,
		.read = wmt_dev_proc_for_aee_read,
		.write = wmt_dev_proc_for_aee_write,
	};

	osal_sleepable_lock_init(&g_aee_read_lock);
	gWmtAeeEntry = proc_create(WMT_AEE_PROCNAME, 0664, NULL, &wmt_aee_fops);
	if (gWmtAeeEntry == NULL) {
		WMT_ERR_FUNC("Unable to create /proc entry\n\r");
		return -1;
	}

	return 0;
}

INT32 wmt_dev_proc_for_aee_remove(VOID)
{
	if (gWmtAeeEntry != NULL)
		remove_proc_entry(WMT_AEE_PROCNAME, NULL);
	osal_sleepable_lock_deinit(&g_aee_read_lock);

	return 0;
}
#endif /* CFG_WMT_PROC_FOR_AEE */



INT32 wmt_dev_proc_init(VOID)
{
	int ret = 0;
#if CFG_WMT_PROC_FOR_DUMP_INFO
	ret = wmt_dev_proc_for_dump_info_setup();
	if (ret)
		return ret;
#endif

#if CFG_WMT_PROC_FOR_AEE
	ret = wmt_dev_proc_for_aee_setup();
#endif
	return ret;
}
INT32 wmt_dev_proc_deinit(VOID)
{
	int ret = 0;

#if CFG_WMT_PROC_FOR_DUMP_INFO
	ret = wmt_dev_proc_for_dump_info_remove();
	if (ret)
		return ret;
#endif
#if CFG_WMT_PROC_FOR_AEE
	ret = wmt_dev_proc_for_aee_remove();
#endif
	return ret;
}


