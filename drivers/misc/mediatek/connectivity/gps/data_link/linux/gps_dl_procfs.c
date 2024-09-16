/*
 * Copyright (C) 2019 MediaTek Inc.
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
#include "gps_dl_config.h"

#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include "gps_dl_osal.h"
#include "gps_dl_procfs.h"
#include "gps_dl_context.h"
#include "gps_each_link.h"
#include "gps_dl_subsys_reset.h"

int gps_dl_procfs_dummy_op(int y, int z)
{
	GDL_LOGW("do nothing: y = %d, z = %d", y, z);
	return 0;
}

int gps_dl_procfs_set_opt(int y, int z)
{
	if (y == 0)
		gps_dl_log_info_show();
	else if (y == 1) {
		enum gps_dl_log_level_enum level_old, level_new;

		level_old = gps_dl_log_level_get();
		gps_dl_log_level_set(z);
		level_new = gps_dl_log_level_get();
		GDL_LOGW("log level change: %d to %d", level_old, level_new);
	} else if (y == 2) {
		unsigned int mod_old, mod_new;

		mod_old = gps_dl_log_mod_bitmask_get();
		gps_dl_log_mod_bitmask_set(z);
		mod_new = gps_dl_log_level_get();
		GDL_LOGW("log modules change: 0x%x to 0x%x", mod_old, mod_new);
	} else if (y == 3) {
		unsigned int mod_old, mod_new;

		mod_old = gps_dl_log_mod_bitmask_get();
		gps_dl_log_mod_on(z);
		mod_new = gps_dl_log_level_get();
		GDL_LOGW("log modules change: 0x%x to 0x%x", mod_old, mod_new);
	} else if (y == 4) {
		unsigned int mod_old, mod_new;

		mod_old = gps_dl_log_mod_bitmask_get();
		gps_dl_log_mod_off(z);
		mod_new = gps_dl_log_level_get();
		GDL_LOGW("log modules change: 0x%x to 0x%x", mod_old, mod_new);
	} else if (y == 5) {
		bool rrw_old, rrw_new;

		rrw_old = gps_dl_show_reg_rw_log();
		if (z == 0)
			gps_dl_set_show_reg_rw_log(false);
		else if (z == 1)
			gps_dl_set_show_reg_rw_log(true);
		rrw_new = gps_dl_show_reg_rw_log();
		GDL_LOGW("log rrw change: %d to %d", rrw_old, rrw_new);
	}

	return 0;
}

int gps_dl_procfs_trigger_reset(int y, int z)
{
	if (y == 0)
		gps_dl_trigger_connsys_reset();
	else if (y == 1)
		gps_dl_trigger_gps_subsys_reset((bool)z);
	else if (y == 2 && (z >= 0 && z <= GPS_DATA_LINK_NUM))
		gps_each_link_reset(z);
	else if (y == 3)
		gps_dl_trigger_gps_print_hw_status();
	else if (y == 4 && (z >= 0 && z <= GPS_DATA_LINK_NUM))
		gps_dl_test_mask_hasdata_irq_set(z, true);
	else if (y == 6 && (z >= 0 && z <= GPS_DATA_LINK_NUM))
		gps_dl_test_mask_mcub_irq_on_open_set(z, true);
	else if (y == 7)
		gps_dl_trigger_gps_print_data_status();
	return 0;
}

gps_dl_procfs_test_func_type g_gps_dl_proc_test_func_list[] = {
	[0x00] = gps_dl_procfs_dummy_op,
	/* [0x01] = TODO: reg read */
	/* [0x02] = TODO: reg write */
	[0x03] = gps_dl_procfs_set_opt,
	[0x04] = gps_dl_procfs_trigger_reset,
};

#define UNLOCK_MAGIC 0xDB9DB9
#define PROCFS_WR_BUF_SIZE 256
ssize_t gps_dl_procfs_write(struct file *filp, const char __user *buffer, size_t count, loff_t *f_pos)
{
	size_t len = count, sub_len;
	char buf[PROCFS_WR_BUF_SIZE];
	char *pBuf;
	int x = 0, y = 0, z = 0;
	char *pToken = NULL;
	long res;
	static bool gpsdl_dbg_enabled;

	GDL_LOGD("write parameter len = %d", (int)len);
	if (len >= PROCFS_WR_BUF_SIZE) {
		GDL_LOGE("input handling fail!");
		return -1;
	}

	if (copy_from_user(buf, buffer, len))
		return -EFAULT;

	buf[len] = '\0';
	pBuf = buf;
	do {
		if (!pBuf) {
			GDL_LOGW("x,y,z use default value - case0");
			break;
		}
		res = 0;
		sub_len = strlen(pBuf);
		GDL_LOGD("write parameter data = %s, len = %ld", pBuf, sub_len);
		if (sub_len != 0)
			pToken = gps_dl_osal_strsep(&pBuf, "\t\n\r ");
		if (sub_len != 0 && pToken != NULL) {
			gps_dl_osal_strtol(pToken, 16, &res);
			x = (int)res;
		} else {
			GDL_LOGW("x use default value");
			break;
		}

		if (!pBuf) {
			GDL_LOGW("y use default value - case1");
			break;
		}
		res = 0;
		sub_len = strlen(pBuf);
		GDL_LOGD("write parameter data = %s, len = %ld", pBuf, sub_len);
		if (sub_len != 0)
			pToken = gps_dl_osal_strsep(&pBuf, "\t\n\r ");
		if (sub_len != 0 && pToken != NULL) {
			gps_dl_osal_strtol(pToken, 16, &res);
			y = (int)res;
		} else {
			GDL_LOGW("y use default value - case2");
			break;
		}

		if (!pBuf) {
			GDL_LOGW("z use default value - case1");
			break;
		}
		res = 0;
		sub_len = strlen(pBuf);
		GDL_LOGD("write parameter data = %s, len = %ld", pBuf, sub_len);
		if (sub_len != 0)
			pToken = gps_dl_osal_strsep(&pBuf, "\t\n\r ");
		if (sub_len != 0 && pToken != NULL) {
			gps_dl_osal_strtol(pToken, 16, &res);
			z = (int)res;
		} else {
			GDL_LOGW("z use default value - case2");
			break;
		}
	} while (0);
	GDL_LOGW("x = 0x%08x, y = 0x%08x, z = 0x%08x", x, y, z);

	/* For eng and userdebug load, have to enable gpsdl_dbg by
	 * writing 0xDB9DB9 to * "/proc/driver/gpsdl_dbg" to avoid
	 * some malicious use
	 */
	if (x == UNLOCK_MAGIC) {
		gpsdl_dbg_enabled = true;
		return len;
	}

	if (!gpsdl_dbg_enabled) {
		GDL_LOGW("please enable gpsdl_dbg firstly");
		return len;
	}

	if (ARRAY_SIZE(g_gps_dl_proc_test_func_list) > x && NULL != g_gps_dl_proc_test_func_list[x])
		(*g_gps_dl_proc_test_func_list[x])(y, z);
	else
		GDL_LOGW("no handler defined for command id, x = 0x%08x", x);

	return len;
}

ssize_t gps_dl_procfs_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos)
{
	return 0;
}

static const struct file_operations gps_dl_procfs_fops = {
	.owner = THIS_MODULE,
	.read = gps_dl_procfs_read,
	.write = gps_dl_procfs_write,
};

static struct proc_dir_entry *g_gps_dl_procfs_entry;
#define GPS_DL_PROCFS_NAME "driver/gpsdl_dbg"

int gps_dl_procfs_setup(void)
{

	int i_ret = 0;

	g_gps_dl_procfs_entry = proc_create(GPS_DL_PROCFS_NAME,
		0664, NULL, &gps_dl_procfs_fops);

	if (g_gps_dl_procfs_entry == NULL) {
		GDL_LOGE("Unable to create gps proc entry");
		i_ret = -1;
	}

	return i_ret;
}

int gps_dl_procfs_remove(void)
{
	if (g_gps_dl_procfs_entry != NULL)
		proc_remove(g_gps_dl_procfs_entry);
	return 0;
}

