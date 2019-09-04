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
#include <linux/uaccess.h>
#include "mt-plat/mtk_thermal_monitor.h"
#include "mach/mtk_thermal.h"

#define MAX_NUM_INSTANCE_MTK_COOLER_CAM  3

static kuid_t uid = KUIDT_INIT(0);
static kgid_t gid = KGIDT_INIT(1000);

#define mtk_cooler_cam_dprintk_always(fmt, args...) \
pr_notice("[Thermal/TC/cam]" fmt, ##args)

#define mtk_cooler_cam_dprintk(fmt, args...) \
do { \
	if (cl_cam_klog_on == 1) \
		pr_notice("[Thermal/TC/cam]" fmt, ##args); \
} while (0)


#define MAX_LEN (256)

static int cl_cam_klog_on;
static struct thermal_cooling_device
	*cl_cam_dev[MAX_NUM_INSTANCE_MTK_COOLER_CAM] = { 0 };
static unsigned long cl_cam_state[MAX_NUM_INSTANCE_MTK_COOLER_CAM] = { 0 };
static unsigned int _cl_cam;

static struct thermal_cooling_device *cl_cam_urgent_dev;
static unsigned long cl_cam_urgent_state;
static unsigned int _cl_cam_urgent;
static unsigned int _cl_cam_status;
static unsigned int _cl_cam_dual_off;
/* < 20C: DualCam off, >= 20C: DualCam on */
static int dualcam_Tj_jump_threshold = 10000;
/*single cam = 1, dual cam = 0, default = 0*/
static int single_cam_flag;
/*10000=>10'C*/
static int dualcam_Tj_hysteresis = 10000;
static int ttj_offset;

/*
 * The cooler status of exit camera.
 * This is for camera app reference.
 */
enum {
	CL_CAM_DEACTIVE = 0,
	CL_CAM_ACTIVE = 1,
	CL_CAM_URGENT = 2
};

int cl_cam_dualcam_off(void)
{

	int tj_headroom = 0;
	int curr_tj = mtk_thermal_get_temp(MTK_THERMAL_SENSOR_CPU);
	int curr_ttj = get_cpu_target_tj();



	/* if any error */
	if (curr_tj < 0 || curr_ttj < 0) {
		mtk_cooler_cam_dprintk_always("%s error: %d %d\n", __func__,
							curr_tj, curr_ttj);
		return 1;
	}
	tj_headroom = curr_ttj + ttj_offset - curr_tj;

	mtk_cooler_cam_dprintk(
		"%s ttj = %d, tj %d, tj_headroom %d, single_cam_flag <%d>\n",
		__func__, curr_ttj, curr_tj, tj_headroom, single_cam_flag);


	if ((tj_headroom <= 0) || (tj_headroom <  dualcam_Tj_jump_threshold)) {
		single_cam_flag = 1;
		mtk_cooler_cam_dprintk(
			"%s tj_headroom:%d < Tj jump threshold:%d\n",
			__func__, tj_headroom, dualcam_Tj_jump_threshold);
		return 1; /*single cam*/
	}


	if ((single_cam_flag == 1)
		&& (curr_tj < (curr_ttj - dualcam_Tj_jump_threshold
					- dualcam_Tj_hysteresis))) {
		single_cam_flag = 0;
		mtk_cooler_cam_dprintk(
			"%s Tj:%d TTj:%d Tj jump threshold:%d Tj_hys:%d, %d\n",
			__func__, curr_tj, curr_ttj, dualcam_Tj_jump_threshold,
			dualcam_Tj_hysteresis, (curr_ttj -
			dualcam_Tj_jump_threshold - dualcam_Tj_hysteresis));

		return 0;/*dual cam*/
	}



	mtk_cooler_cam_dprintk("Tj:%d TTj:%d Tj jump thd:%d Tj_hys:%d, %d\n",
		curr_tj, curr_ttj, dualcam_Tj_jump_threshold,
		dualcam_Tj_hysteresis, (curr_ttj + ttj_offset -
			dualcam_Tj_jump_threshold - dualcam_Tj_hysteresis));


	return 0;/*dual cam*/

}

static void cl_cam_status_update(void)
{
	if (_cl_cam_urgent)
		_cl_cam_status = CL_CAM_URGENT;
	else if (_cl_cam)
		_cl_cam_status = CL_CAM_ACTIVE;
	else
		_cl_cam_status = CL_CAM_DEACTIVE;
}

static ssize_t _cl_cam_write
	(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	int ret = 0;
	char tmp[MAX_LEN] = { 0 };

	len = (len < (MAX_LEN - 1)) ? len : (MAX_LEN - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtouint(tmp, 10, &_cl_cam);
	if (ret)
		WARN_ON_ONCE(1);

	mtk_cooler_cam_dprintk_always("%s %s = %d\n", __func__, tmp, _cl_cam);

	return len;
}

static int _cl_cam_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", _cl_cam);

	return 0;
}

static int _cl_cam_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_cam_read, PDE_DATA(inode));
}

static const struct file_operations _cl_cam_fops = {
	.owner = THIS_MODULE,
	.open = _cl_cam_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_cam_write,
	.release = single_release,
};

static int mtk_cl_cam_get_max_state
	(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int mtk_cl_cam_get_cur_state
	(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = *((unsigned long *)cdev->devdata);

	return 0;
}

static int mtk_cl_cam_set_cur_state
	(struct thermal_cooling_device *cdev, unsigned long state)
{
	/* mtk_cooler_cam_dprintk("%s %s %d\n", __func__, cdev->type, state); */

	*((unsigned long *)cdev->devdata) = state;

	if (state == 1)
		_cl_cam = 1;
	else {
		int i;

		for (i = 0; i < MAX_NUM_INSTANCE_MTK_COOLER_CAM; i++) {
			if (cl_cam_state[i])
				break;
		}

		if (i == MAX_NUM_INSTANCE_MTK_COOLER_CAM)
			_cl_cam = 0;
	}

	cl_cam_status_update();

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_cam_ops = {
	.get_max_state = mtk_cl_cam_get_max_state,
	.get_cur_state = mtk_cl_cam_get_cur_state,
	.set_cur_state = mtk_cl_cam_set_cur_state,
};

static ssize_t _cl_cam_status_write
	(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	int ret = 0;
	char tmp[128] = { 0 };

	len = (len < (128 - 1)) ? len : (128 - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	ret = kstrtouint(tmp, 10, &_cl_cam_status);
	if (ret)
		WARN_ON_ONCE(1);

	mtk_cooler_cam_dprintk_always(
		"%s %s = %d\n", __func__, tmp, _cl_cam_status);

	return len;
}

static int _cl_cam_status_read(struct seq_file *m, void *v)
{
	seq_printf(m, "%d\n", _cl_cam_status);

	if (_cl_cam_status)
		mtk_cooler_cam_dprintk_always(
			"%s: %d\n", __func__, _cl_cam_status);

	return 0;
}

static int _cl_cam_status_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_cam_status_read, PDE_DATA(inode));
}

static const struct file_operations _cl_cam_status_fops = {
	.owner = THIS_MODULE,
	.open = _cl_cam_status_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_cam_status_write,
	.release = single_release,
};

static ssize_t _cl_cam_dual_off_write
	(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	char tmp[128] = { 0 };
	int klog_on, tj_jump_threshold, tj_hysteresis, tj_offset;

	len = (len < (128 - 1)) ? len : (128 - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	if (data == NULL) {
		mtk_cooler_cam_dprintk_always("%s null data\n", __func__);
		return -EINVAL;
	}
	if (sscanf(tmp, "%d %d %d %d", &klog_on, &tj_jump_threshold,
					&tj_hysteresis, &tj_offset) >= 1) {

		if (klog_on == 0 || klog_on == 1)
			cl_cam_klog_on = klog_on;

		dualcam_Tj_hysteresis = tj_hysteresis;
		dualcam_Tj_jump_threshold = tj_jump_threshold;
		ttj_offset = tj_offset;


		mtk_cooler_cam_dprintk_always("%s : %d %d %d\n",
					__func__, dualcam_Tj_hysteresis,
					dualcam_Tj_jump_threshold, ttj_offset);


		return len;
	}

	return len;
}

static int _cl_cam_dual_off_read(struct seq_file *m, void *v)
{
	_cl_cam_dual_off = cl_cam_dualcam_off();
	seq_printf(m, "%d\n", _cl_cam_dual_off);

	return 0;
}

static int _cl_cam_dual_off_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_cam_dual_off_read, PDE_DATA(inode));
}

static const struct file_operations _cl_cam_dual_off_fops = {
	.owner = THIS_MODULE,
	.open = _cl_cam_dual_off_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_cam_dual_off_write,
	.release = single_release,
};



static ssize_t _cl_cam_dual_off_setting_write
	(struct file *filp, const char __user *buf, size_t len, loff_t *data)
{
	char tmp[128] = { 0 };
	int klog_on, tj_jump_threshold, tj_hysteresis, tj_offset;

	len = (len < (128 - 1)) ? len : (128 - 1);
	/* write data to the buffer */
	if (copy_from_user(tmp, buf, len))
		return -EFAULT;

	if (data == NULL) {
		mtk_cooler_cam_dprintk_always("%s null data\n", __func__);
		return -EINVAL;
	}
	if (sscanf(tmp, "%d %d %d %d", &klog_on, &tj_jump_threshold,
				&tj_hysteresis, &tj_offset) >= 1) {

		if (klog_on == 0 || klog_on == 1)
			cl_cam_klog_on = klog_on;

		dualcam_Tj_hysteresis = tj_hysteresis;
		dualcam_Tj_jump_threshold = tj_jump_threshold;
		ttj_offset = tj_offset;

		mtk_cooler_cam_dprintk_always("%s : %d %d %d\n",
				__func__, dualcam_Tj_hysteresis,
				dualcam_Tj_jump_threshold, ttj_offset);

		return len;
	}
	return len;
}

static int _cl_cam_dual_off_setting_read(struct seq_file *m, void *v)
{
	_cl_cam_dual_off = cl_cam_dualcam_off();
	seq_printf(m, "%d\n", _cl_cam_dual_off);
	seq_printf(m, "%d\n", dualcam_Tj_hysteresis);
	seq_printf(m, "%d\n", dualcam_Tj_jump_threshold);
	seq_printf(m, "%d\n", ttj_offset);

	return 0;
}

static int _cl_cam_dual_off_setting_open(struct inode *inode, struct file *file)
{
	return single_open(file, _cl_cam_dual_off_setting_read,
						PDE_DATA(inode));
}

static const struct file_operations _cl_cam_dual_off_fops_setting = {
	.owner = THIS_MODULE,
	.open = _cl_cam_dual_off_setting_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.write = _cl_cam_dual_off_setting_write,
	.release = single_release,
};

static int mtk_cl_cam_urgent_get_max_state
	(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = 1;
	return 0;
}

static int mtk_cl_cam_urgent_get_cur_state
	(struct thermal_cooling_device *cdev, unsigned long *state)
{
	*state = *((unsigned long *)cdev->devdata);
	return 0;
}

static int mtk_cl_cam_urgent_set_cur_state
	(struct thermal_cooling_device *cdev, unsigned long state)
{
	/* mtk_cooler_cam_dprintk("%s %s %d\n", __func__, cdev->type, state); */

	*((unsigned long *)cdev->devdata) = state;

	if (state == 1)
		_cl_cam_urgent = 1;
	else
		_cl_cam_urgent = 0;

	cl_cam_status_update();

	return 0;
}

/* bind fan callbacks to fan device */
static struct thermal_cooling_device_ops mtk_cl_cam_urgent_ops = {
	.get_max_state = mtk_cl_cam_urgent_get_max_state,
	.get_cur_state = mtk_cl_cam_urgent_get_cur_state,
	.set_cur_state = mtk_cl_cam_urgent_set_cur_state,
};

static int mtk_cooler_cam_register_ltf(void)
{
	int i;

	/* mtk_cooler_cam_dprintk("%s\n", __func__); */

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_CAM; i-- > 0;) {
		char temp[20] = { 0 };

		sprintf(temp, "mtk-cl-cam%02d", i);
		cl_cam_dev[i] = mtk_thermal_cooling_device_register(temp,
						(void *)&cl_cam_state[i],
						&mtk_cl_cam_ops);
	}

	return 0;
}

static void mtk_cooler_cam_unregister_ltf(void)
{
	int i;

	/* mtk_cooler_cam_dprintk("%s\n", __func__); */

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_CAM; i-- > 0;) {
		if (cl_cam_dev[i]) {
			mtk_thermal_cooling_device_unregister(cl_cam_dev[i]);
			cl_cam_dev[i] = NULL;
			cl_cam_state[i] = 0;
		}
	}
}

static int mtk_cooler_cam_urgent_register_ltf(void)
{
	/* mtk_cooler_cam_dprintk("%s\n", __func__); */

	cl_cam_urgent_dev = mtk_thermal_cooling_device_register(
						"mtk-cl-cam-urgent",
						(void *)&cl_cam_urgent_state,
						&mtk_cl_cam_urgent_ops);

	return 0;
}

static void mtk_cooler_cam_urgent_unregister_ltf(void)
{
	/* mtk_cooler_cam_dprintk("%s\n", __func__); */

	if (cl_cam_urgent_dev) {
		mtk_thermal_cooling_device_unregister(cl_cam_urgent_dev);
		cl_cam_urgent_dev = NULL;
		cl_cam_urgent_state = 0;
	}
}

static int __init mtk_cooler_cam_init(void)
{
	int err = 0;
	int i;
	struct proc_dir_entry *entry = NULL;
	struct proc_dir_entry *camsetting_dir = NULL;

	for (i = MAX_NUM_INSTANCE_MTK_COOLER_CAM; i-- > 0;) {
		cl_cam_dev[i] = NULL;
		cl_cam_state[i] = 0;
	}

	/* mtk_cooler_cam_dprintk("%s\n", __func__); */
{

	struct proc_dir_entry *entry;

#if 0
	entry = create_proc_entry("driver/cl_cam", 0644, NULL);
	if (entry != NULL) {
		entry->read_proc = _cl_cam_read;
		entry->write_proc = _cl_cam_write;
	}
#endif
	entry = proc_create("driver/cl_cam", 0644, NULL, &_cl_cam_fops);

	if (!entry)
		mtk_cooler_cam_dprintk(
			"%s driver/cl_cam creation failed\n", __func__);

	entry = proc_create("driver/cl_cam_status", 0644,
					NULL, &_cl_cam_status_fops);
	if (!entry)
		mtk_cooler_cam_dprintk(
			"%s driver/cl_cam_status creation failed\n", __func__);

	entry = proc_create("driver/cl_cam_dual_off", 0664, NULL,
							&_cl_cam_dual_off_fops);
	if (!entry)
		mtk_cooler_cam_dprintk(
			"%s driver/cl_cam_dual creation failed\n", __func__);
}
	camsetting_dir = mtk_thermal_get_proc_drv_therm_dir_entry();

	if (!camsetting_dir)
		mtk_cooler_cam_dprintk_always(
			"[%s]: mkdir /proc/driver/thermal failed\n", __func__);
	else {
		entry = proc_create("cl_cam_dual_off_setting",
				0664,
				camsetting_dir, &_cl_cam_dual_off_fops_setting);
		if (entry)
			proc_set_user(entry, uid, gid);
	}

	err = mtk_cooler_cam_register_ltf();
	if (err)
		goto err_unreg;

	err = mtk_cooler_cam_urgent_register_ltf();
	if (err)
		goto err_unreg;

	return 0;

 err_unreg:
	mtk_cooler_cam_unregister_ltf();
	mtk_cooler_cam_urgent_unregister_ltf();
	return err;
}

static void __exit mtk_cooler_cam_exit(void)
{
	/* mtk_cooler_cam_dprintk("%s\n", __func__); */

	mtk_cooler_cam_unregister_ltf();
	mtk_cooler_cam_urgent_unregister_ltf();
}
module_init(mtk_cooler_cam_init);
module_exit(mtk_cooler_cam_exit);
