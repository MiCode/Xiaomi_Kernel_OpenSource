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

/*=============================================================
 *Macro definition
 *=============================================================
 */
#define CLNR_LOG_TAG	"[Cooler_NR]"

#define clNR_dprintk(fmt, args...)   \
	do {                                    \
		if (clNR_debug_log == 1) {                \
			pr_notice(CLNR_LOG_TAG fmt, ##args); \
		}                                   \
	} while (0)

#define clNR_printk(fmt, args...) pr_notice(CLNR_LOG_TAG fmt, ##args)
/*=============================================================
 *Local variable definition
 *=============================================================
 */
static unsigned int clNR_debug_log = 1;

static unsigned int cl_dev_NR_state;
static struct thermal_cooling_device *cl_dev_NR;

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);
static struct proc_dir_entry *clNR_status;
static char *clNR_mmap;
/*=============================================================
 */

static vm_fault_t mmap_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	struct page *page;
	char *info;

	clNR_dprintk("%s %d\n", __func__, __LINE__);
	/* the data is in vma->vm_private_data */
	info = (char *)vma->vm_private_data;

	if (!info) {
		clNR_printk("no data\n");
		return -1;
	}

	/* get the page */
	page = virt_to_page(info);

	get_page(page);
	vmf->page = page;
	clNR_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static const struct vm_operations_struct clNR_mmap_vm_ops = {
	.fault =   mmap_fault,
};

static int clNR_status_mmap(struct file *file, struct vm_area_struct *vma)
{
	clNR_dprintk("%s %d\n", __func__, __LINE__);
	vma->vm_ops = &clNR_mmap_vm_ops;
	vma->vm_flags |= VM_IO;
	/* assign the file private data to the vm private data */
	vma->vm_private_data = clNR_mmap;
	clNR_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static ssize_t clNR_status_write(
struct file *file, const char __user *buffer, size_t count, loff_t *data)
{
	char desc[32];
	char arg_name[32] = { 0 };
	char trailing[32] = { 0 };
	int isEnabled, len = 0, arg_val = 0;


	clNR_dprintk("%s %d\n", __func__, __LINE__);
	len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
	if (copy_from_user(desc, buffer, len))
		return 0;

	desc[len] = '\0';

	if (kstrtoint(desc, 10, &isEnabled) == 0) {
		cl_dev_NR_state = isEnabled;
		if (isEnabled == 1)
			*(unsigned int *)(clNR_mmap + 0x00) = 0x1;
		else
			*(unsigned int *)(clNR_mmap + 0x00) = 0x0;

		clNR_dprintk("%s %d\n", __func__, __LINE__);
		return count;
	} else if (sscanf(
		desc, "%31s %d %31s", arg_name, &arg_val, trailing) >= 2) {
		if (strncmp(arg_name, "debug", 5) == 0) {
			clNR_dprintk("%s %d\n", __func__, __LINE__);
			clNR_debug_log = arg_val;
			return count;
		}
	}

	clNR_printk("%s bad argument\n", __func__);

	return -EINVAL;
}

static int clNR_status_read(struct seq_file *m, void *v)
{
	clNR_dprintk("%s %d\n", __func__, __LINE__);
	seq_printf(m, CLNR_LOG_TAG "clNR_status= %u clNR_mmap= 0x%x\n",
			cl_dev_NR_state, *clNR_mmap);

	clNR_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static int clNR_status_open(struct inode *inode, struct file *file)
{
	clNR_dprintk("%s %d\n", __func__, __LINE__);

	return single_open(file, clNR_status_read, NULL);
}

static const struct file_operations clNR_status_fops = {
	.owner = THIS_MODULE,
	.open = clNR_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = clNR_status_write,
	.release = single_release,
	.mmap = clNR_status_mmap,
};

/*
 * cooling device callback functions (clNR_cooling_NR_ops)
 * 1 : ON and 0 : OFF
 */
static int clNR_get_max_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int clNR_get_cur_state(
struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = cl_dev_NR_state;
	return 0;
}

static int clNR_set_cur_state(
struct thermal_cooling_device *cdev, unsigned long state)
{
	cl_dev_NR_state = state;

	if (cl_dev_NR_state == 1) {
		clNR_dprintk("mtkclNR triggered\n");
		*(unsigned int *)(clNR_mmap + 0x00) = 0x1;
	} else {
		clNR_dprintk("mtkclNR exited\n");
		*(unsigned int *)(clNR_mmap + 0x00) = 0x0;
	}

	return 0;
}
static struct thermal_cooling_device_ops mtkclNR_ops = {
	.get_max_state = clNR_get_max_state,
	.get_cur_state = clNR_get_cur_state,
	.set_cur_state = clNR_set_cur_state,
};

static int __init mtk_cooler_NR_init(void)
{
	struct proc_dir_entry *cooler_dir = NULL;

	clNR_dprintk("%s %d\n", __func__, __LINE__);
	cl_dev_NR = mtk_thermal_cooling_device_register("mtkclNR", NULL,
								&mtkclNR_ops);

	cooler_dir = mtk_thermal_get_proc_drv_therm_dir_entry();

	if (!cooler_dir) {
		clNR_printk("[%s]: mkdir /proc/driver/thermal failed\n",
								__func__);
	} else {
		clNR_status =
			proc_create("clNR_status", 0664,
						cooler_dir, &clNR_status_fops);

		if (clNR_status)
			proc_set_user(clNR_status, uid, gid);

		clNR_mmap = (char *)get_zeroed_page(GFP_KERNEL);
		*(unsigned int *)(clNR_mmap + 0x00) = 0x0;
	}

	clNR_dprintk("%s %d\n", __func__, __LINE__);
	return 0;
}

static void __exit mtk_cooler_NR_exit(void)
{

	clNR_dprintk("%s %d\n", __func__, __LINE__);
	if (cl_dev_NR) {
		mtk_thermal_cooling_device_unregister(cl_dev_NR);
		cl_dev_NR = NULL;
	}

	free_page((unsigned long) clNR_mmap);
	proc_remove(clNR_status);
	clNR_dprintk("%s %d\n", __func__, __LINE__);
}
module_init(mtk_cooler_NR_init);
module_exit(mtk_cooler_NR_exit);
