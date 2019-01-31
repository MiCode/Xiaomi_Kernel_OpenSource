/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/thermal.h>
#include <linux/platform_device.h>
#include <mt-plat/aee.h>
#include <linux/types.h>
#include <linux/proc_fs.h>
#include "mt-plat/mtk_thermal_monitor.h"
#if defined(CONFIG_MTK_CLKMGR)
#include <mach/mtk_clkmgr.h>
#else
#include <linux/clk.h>
#endif
#include <linux/slab.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/uaccess.h>

/*=============================================================
 *Macro definition
 *=============================================================
 */
#define CLVR_FPS_LOG_TAG	"[Cooler_VR_FPS]"

#define clVR_FPS_dprintk(fmt, args...)   \
	do {                                    \
		if (clVR_FPS_debug_log == 1) {                \
			pr_notice(CLVR_FPS_LOG_TAG fmt, ##args); \
		}                                   \
	} while (0)

#define clVR_FPS_printk(fmt, args...) pr_notice(CLVR_FPS_LOG_TAG fmt, ##args)
/*=============================================================
 *Local variable definition
 *=============================================================
 */
static unsigned int clVR_FPS_debug_log;

static unsigned int cl_dev_VR_FPS_state;
static struct thermal_cooling_device *cl_dev_VR_FPS;

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static struct proc_dir_entry *clVR_FPS_status;
/*=============================================================
 */
static ssize_t clVR_FPS_status_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	char arg_name[32] = { 0 };
	char trailing[32] = { 0 };
	int isEnabled, len = 0, arg_val = 0;


	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &isEnabled) == 0) {
		clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
		cl_dev_VR_FPS_state = isEnabled;

		return count;
	} else if (sscanf(
		desc, "%31s %d %31s", arg_name, &arg_val, trailing) >= 2) {
		if (strncmp(arg_name, "debug", 5) == 0) {
			clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
			clVR_FPS_debug_log = arg_val;
			return count;
		}
	}

	clVR_FPS_printk("clVR_FPS_status_write bad argument\n");

	return -EINVAL;
}

static int clVR_FPS_status_read(struct seq_file *m, void *v)
{
	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
	seq_printf(m, "%u\n", cl_dev_VR_FPS_state);

	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static int clVR_FPS_status_open(struct inode *inode, struct file *file)
{
	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);

	return single_open(file, clVR_FPS_status_read, NULL);
}

static int clVR_FPS_status_close(struct inode *inode, struct file *file)
{
	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);

	return 0;
}

static const struct file_operations clVR_FPS_status_fops = {
	.owner = THIS_MODULE,
	.open = clVR_FPS_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = clVR_FPS_status_write,
	.release = clVR_FPS_status_close,
};

/*
 * cooling device callback functions (clVR_FPS_cooling_VR_FPS_ops)
 * 1 : ON and 0 : OFF
 */
static int clVR_FPS_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;

	return 0;
}

static int clVR_FPS_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_VR_FPS_state;

	return 0;
}

static int clVR_FPS_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_VR_FPS_state = state;

	if (cl_dev_VR_FPS_state == 1)
		clVR_FPS_dprintk("mtkclVR_FPS triggered\n");
	else
		clVR_FPS_dprintk("mtkclVR_FPS exited\n");

	return 0;
}

static struct thermal_cooling_device_ops mtkclVR_FPS_ops = {
	.get_max_state = clVR_FPS_get_max_state,
	.get_cur_state = clVR_FPS_get_cur_state,
	.set_cur_state = clVR_FPS_set_cur_state,
};

static int __init mtk_cooler_VR_FPS_init(void)
{
	struct proc_dir_entry *cooler_dir = NULL;

	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
	cl_dev_VR_FPS = mtk_thermal_cooling_device_register(
					"mtkclVR_FPS", NULL, &mtkclVR_FPS_ops);

	cooler_dir = mtk_thermal_get_proc_drv_therm_dir_entry();

	if (!cooler_dir) {
		clVR_FPS_printk("[%s]: mkdir /proc/driver/thermal failed\n",
				__func__);
	} else {
		clVR_FPS_status =
			proc_create("clVR_FPS_status", 0664,
					cooler_dir, &clVR_FPS_status_fops);

		if (clVR_FPS_status)
			proc_set_user(clVR_FPS_status, uid, gid);
	}

	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static void __exit mtk_cooler_VR_FPS_exit(void)
{

	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
	if (cl_dev_VR_FPS) {
		mtk_thermal_cooling_device_unregister(cl_dev_VR_FPS);
		cl_dev_VR_FPS = NULL;
	}

	proc_remove(clVR_FPS_status);
	clVR_FPS_dprintk("%s %d\n", __func__, __LINE__);
}
module_init(mtk_cooler_VR_FPS_init);
module_exit(mtk_cooler_VR_FPS_exit);
