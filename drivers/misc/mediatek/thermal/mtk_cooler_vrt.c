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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/version.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/kobject.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include "mt-plat/mtk_thermal_monitor.h"

#define MAX_NUM_INSTANCE_MTK_COOLER_VRT  1

#if 1
#define mtk_cooler_vrt_dprintk(fmt, args...) \
pr_debug("thermal/cooler/vrt " fmt, ##args)
#else
#define mtk_cooler_vrt_dprintk(fmt, args...)
#endif

static struct thermal_cooling_device *cl_vrt_dev[MAX_NUM_INSTANCE_MTK_COOLER_VRT] = { 0 };
static unsigned long cl_vrt_state[MAX_NUM_INSTANCE_MTK_COOLER_VRT] = { 0 };

static unsigned int _cl_vrt;

#define MAX_LEN (256)

static ssize_t _cl_vrt_write(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = { 0 };

	len = (len < (MAX_LEN - 1)) ? len : (MAX_LEN - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtouint(tmp, 10, &_cl_vrt);
	if (ret)
		WARN_ON(1);

	mtk_cooler_vrt_dprintk("%s %s = %d\n", __func__, tmp, _cl_vrt);

	return len;
}

static int _cl_vrt_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", _cl_vrt);
	mtk_cooler_vrt_dprintk("%s %d\n", __func__, _cl_vrt);

	return 0;
}

static int _cl_vrt_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_vrt_read, PDE_DATA(inode));
}

static const struct file_operations _cl_vrt_fops = {
	.owner = THIS_MODULE,
	.open = _cl_vrt_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_vrt_write,
	.release = single_release,
};


static int mtk_cl_vrt_get_max_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	/* mtk_cooler_vrt_dprintk("mtk_cl_vrt_get_max_state() %s %d\n", cdev->type, *state); */
	return 0;
}

static int mtk_cl_vrt_get_cur_state(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = *((unsigned long *)cdev->devdata);
	/* mtk_cooler_vrt_dprintk("mtk_cl_vrt_get_cur_state() %s %d\n", cdev->type, *state); */
	return 0;
}

static int mtk_cl_vrt_set_cur_state(struct thermal_cooling_device *cdev, unsigned long state)
{
	/* mtk_cooler_vrt_dprintk("mtk_cl_vrt_set_cur_state() %s %d\n", cdev->type, state); */

	*((unsigned long *)cdev->devdata) = state;

	if (1 == state)
		_cl_vrt = 1;
	else
		_cl_vrt = 0;

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_vrt_ops = {
	.get_max_state = mtk_cl_vrt_get_max_state,
	.get_cur_state = mtk_cl_vrt_get_cur_state,
	.set_cur_state = mtk_cl_vrt_set_cur_state,
};

static int mtk_cooler_vrt_register_ltf(void)
{
	int i;

	mtk_cooler_vrt_dprintk("register ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_VRT; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-vrt%02d", i);
		cl_vrt_dev[i] = mtk_thermal_cooling_device_register(temp,
								    (void *)&cl_vrt_state[i],
								    &mtk_cl_vrt_ops);
	}

	return 0;
}

static void mtk_cooler_vrt_unregister_ltf(void)
{
	int i;

	mtk_cooler_vrt_dprintk("unregister ltf\n");

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_VRT; i-- > 0;) {
		if (cl_vrt_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_vrt_dev[i]);
			cl_vrt_dev[i] = NULL;
			cl_vrt_state[i] = 0;
		}
	}
}


static int __init mtk_cooler_vrt_init(void)
{
	int err = 0;
	int i;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_VRT; i-- > 0;) {
		cl_vrt_dev[i] = NULL;
		cl_vrt_state[i] = 0;
	}

	mtk_cooler_vrt_dprintk("init\n");

	{
		struct proc_dir_entry *entry;

#if 0
		entry = create_proc_entry("driver/cl_vrt", S_IRUGO | S_IWUSR, NULL);
		if (NULL != entry) {
			entry->read_proc = _cl_vrt_read;
			entry->write_proc = _cl_vrt_write;
		}
#endif
		entry = proc_create("driver/cl_vrt", S_IRUGO | S_IWUSR, NULL, &_cl_vrt_fops);
		if (!entry)
			mtk_cooler_vrt_dprintk("%s driver/cl_vrt creation failed\n", __func__);
	}

	err = mtk_cooler_vrt_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

 err_unreg:
	mtk_cooler_vrt_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_vrt_exit(void)
{
	mtk_cooler_vrt_dprintk("exit\n");

	mtk_cooler_vrt_unregister_ltf();
}
module_init(mtk_cooler_vrt_init);
module_exit(mtk_cooler_vrt_exit);
