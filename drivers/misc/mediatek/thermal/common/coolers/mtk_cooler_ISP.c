// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
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
#include <linux/pm_qos.h>
#include <linux/soc/mediatek/mtk-pm-qos.h>
#include <mmdvfs_pmqos.h>


/*=============================================================
 *Local variable definition
 *=============================================================
 */
static unsigned int clVR_ISP_debug_log;

static unsigned int cl_dev_VR_ISP_state;
static unsigned int cl_dev_VR_ISP_cur_state;
static struct thermal_cooling_device *cl_dev_VR_ISP;

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static struct proc_dir_entry *clVR_ISP_status;
/*=============================================================
 */
/*=============================================================
 *Macro definition
 *=============================================================
 */
#define CLVR_ISP_LOG_TAG	"[Thermal/CL/ISP]"

#define clVR_ISP_dprintk(fmt, args...)   \
	do {                                    \
		if (clVR_ISP_debug_log == 1) {                \
			pr_notice(CLVR_ISP_LOG_TAG fmt, ##args); \
		}                                   \
	} while (0)

#define clVR_ISP_printk(fmt, args...) pr_notice(CLVR_ISP_LOG_TAG fmt, ##args)



void __attribute__ ((weak))
mmdvfs_qos_limit_config(u32 pm_qos_class, u32 limit_value,
	enum mmdvfs_limit_source source)
{
	pr_notice("E_WF: %s doesn't exist\n", __func__);
}

static ssize_t clVR_ISP_status_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	char arg_name[32] = { 0 };
	char trailing[32] = { 0 };
	int isEnabled, len = 0, arg_val = 0;


	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &isEnabled) == 0) {
		clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
		cl_dev_VR_ISP_state = isEnabled;

		return count;
	} else if (sscanf(
		desc, "%31s %d %31s", arg_name, &arg_val, trailing) >= 2) {
		if (strncmp(arg_name, "debug", 5) == 0) {
			clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
			clVR_ISP_debug_log = arg_val;
			return count;
		}
	}

	clVR_ISP_printk("%s bad argument\n", __func__);

	return -EINVAL;
}

static int clVR_ISP_status_read(struct seq_file *m, void *v)
{
	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
	seq_printf(m, "%u\n", cl_dev_VR_ISP_state);

	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static int clVR_ISP_status_open(struct inode *inode, struct file *file)
{
	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);

	return single_open(file, clVR_ISP_status_read, NULL);
}

static const struct file_operations clVR_ISP_status_fops = {
	.owner = THIS_MODULE,
	.open = clVR_ISP_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = clVR_ISP_status_write,
	.release = single_release,
};

/*
 * cooling device callback functions (clVR_FPS_cooling_VR_ISP_ops)
 * 1 : ON and 0 : OFF
 */
static int clVR_ISP_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	clVR_ISP_dprintk("%s\n", __func__);

	return 0;
}

static int clVR_ISP_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_VR_ISP_state;
	clVR_ISP_dprintk("%s %d, %d\n", __func__,
		cl_dev_VR_ISP_state, cl_dev_VR_ISP_cur_state);
	return 0;
}

static int clVR_ISP_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_VR_ISP_state = state;

	clVR_ISP_dprintk("%s %d,%d\n", __func__,
		cl_dev_VR_ISP_state, cl_dev_VR_ISP_cur_state);

	if ((cl_dev_VR_ISP_state == 1) && (cl_dev_VR_ISP_cur_state == 0)) {
		clVR_ISP_dprintk("mtkclVR_ISP triggered\n");
		mmdvfs_qos_limit_config(PM_QOS_IMG_FREQ, 1,
			MMDVFS_LIMIT_THERMAL);
		cl_dev_VR_ISP_cur_state = 1;
	}

	if ((cl_dev_VR_ISP_state == 0) && (cl_dev_VR_ISP_cur_state == 1)) {
		clVR_ISP_dprintk("mtkclVR_ISP exited\n");
		mmdvfs_qos_limit_config(PM_QOS_IMG_FREQ, 0,
			MMDVFS_LIMIT_THERMAL);
		cl_dev_VR_ISP_cur_state = 0;
	}
	return 0;
}

static struct thermal_cooling_device_ops mtkclVR_ISP_ops = {
	.get_max_state = clVR_ISP_get_max_state,
	.get_cur_state = clVR_ISP_get_cur_state,
	.set_cur_state = clVR_ISP_set_cur_state,
};

static int __init mtk_cooler_VR_ISP_init(void)
{
	struct proc_dir_entry *cooler_dir = NULL;

	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
	cl_dev_VR_ISP = mtk_thermal_cooling_device_register(
					"mtkclVR_ISP", NULL, &mtkclVR_ISP_ops);

	cooler_dir = mtk_thermal_get_proc_drv_therm_dir_entry();

	if (!cooler_dir) {
		clVR_ISP_printk("[%s]: mkdir /proc/driver/thermal failed\n",
				__func__);
	} else {
		clVR_ISP_status =
			proc_create("clVR_ISP_status", 0664,
					cooler_dir, &clVR_ISP_status_fops);

		if (clVR_ISP_status)
			proc_set_user(clVR_ISP_status, uid, gid);
	}

	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static void __exit mtk_cooler_VR_ISP_exit(void)
{

	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
	if (cl_dev_VR_ISP) {
		mtk_thermal_cooling_device_unregister(cl_dev_VR_ISP);
		cl_dev_VR_ISP = NULL;
	}

	proc_remove(clVR_ISP_status);
	clVR_ISP_dprintk("%s %d\n", __func__, __LINE__);
}
module_init(mtk_cooler_VR_ISP_init);
module_exit(mtk_cooler_VR_ISP_exit);
