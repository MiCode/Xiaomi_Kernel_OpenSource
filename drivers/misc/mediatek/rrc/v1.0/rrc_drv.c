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

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mm_types.h>
#include <linux/module.h>
#include <generated/autoconf.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/kdev_t.h>
#include <linux/delay.h>
#include <linux/ioport.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/spinlock.h>
#include <linux/param.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/slab.h>

#include <linux/fb.h>
#include <linux/kthread.h>
#include <linux/vmalloc.h>
#include <linux/semaphore.h>
#include <linux/mutex.h>
#include <linux/suspend.h>
#include <linux/of_fdt.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/dma-buf.h>
#include <linux/atomic.h>
#include <asm/cacheflush.h>
#include <linux/io.h>

#include <linux/compat.h>

#include <linux/ktime.h>
#include <linux/of_irq.h>



#include <asm/io.h>
#include <linux/timer.h>
#include <linux/ioctl.h>

#if IS_ENABLED(CONFIG_COMPAT)
#include <linux/uaccess.h>
#endif

#include "rrc_def.h"

#include "rrc_drv.h"

#define RRC_DEVNAME "mtk_rrc"


#define RRC_PROCESS 0x2


/* -------------------------------------------------------------------------- */
/*	*/
/* -------------------------------------------------------------------------- */

/* global function */
static unsigned int _rrc_int_status;

/* device and driver */
static dev_t rrc_devno;
static struct cdev *rrc_cdev;
static struct class *rrc_class;


/* static wait_queue_head_t enc_wait_queue; */
static spinlock_t rrc_lock;
static int rrc_status;


static int scenario_back_status[RRC_DRV_TYPE_MAX_SIZE];

static int scenario_status[RRC_DRV_TYPE_MAX_SIZE];

static int new_event_flag;

/* static int scenario_status_next = 1; */

static int is_touch_event;
static int is_video_scenario;


#if 0
static int _rrc_sample_cnt;
#endif


#define RRC_TOUCH_120HZ_MONITOR_WINDOW (500)
#define RRC_TOUCH_120HZ_MONITOR_FPS_COUNT (30)
#define RRC_DRV_LOW_SAMPLE_COUNT (5*3)
#define RRC_SAMPLE_INTERVAL (200)


static wait_queue_head_t set_refresh_rate_wq;
static struct task_struct *set_refresh_rate_task;
static int set_refresh_rate_task_wakeup;
static wait_queue_head_t set_low_refresh_rate_wq;
static struct task_struct *set_low_refresh_rate_task;
static int set_low_refresh_rate_task_wakeup;
static int rrc_curr_refresh_rate;
static int _rrc_avg_fps_range;
static int _is_new_change_refresh_event;
static int _is_video_120hz_en;


static int _rrc_refresh_state = RRC_STATE_NORMAL;



static int rrc_set_refresh_state(int state)
{
	_rrc_refresh_state = state;
	return _rrc_refresh_state;
}

static int rrc_get_refresh_state(void)
{
	return _rrc_refresh_state;
}

#if 0
static int rrc_get_ddp_fps(void)
{
	int avg_fps;
	/* get avg FPS API */
	RRC_WRN("[RRC] get ddp fps !!\n");
	avg_fps = primary_display_get_hwc_refresh_rate();
	return avg_fps;
}

static int rrc_get_ddp_refresh_rate(void)
{
	int refresh_rate;
	/* get refresh_rate API */
	RRC_WRN("[RRC] get ddp refresh rate !!\n");
	refresh_rate = primary_display_get_lcm_refresh_rate();
	return refresh_rate;
}
#endif

static int rrc_set_ddp_refresh_rate(int refresh_rate)
{
	/* set refresh_rate API */
	int target_rate = 60;

	if (refresh_rate == RRC_DRV_120Hz)
		target_rate = 120;
	else
		target_rate = 60;

	RRC_WRN("[RRC] set ddp refresh rate %d(%d)!!\n", target_rate, refresh_rate);

	primary_display_set_lcm_refresh_rate(target_rate);
	return 0;
}



static int rrc_set_curr_refresh_rate(int refresh_rate)
{
	rrc_curr_refresh_rate = refresh_rate;
	/* call ddp to switch refresh rate */
	rrc_set_ddp_refresh_rate(refresh_rate);
	return rrc_curr_refresh_rate;

}

static int rrc_get_curr_refresh_rate(void)
{
	int state = rrc_get_refresh_state();
	int refresh;

	/* get display refresh_rate */

	switch (state) {
	case RRC_STATE_HIGH_120Hz:
	case RRC_STATE_VIDEO_120Hz:
		refresh = RRC_DRV_120Hz;
		break;
	case RRC_STATE_NORMAL:
	case RRC_STATE_VIDEO:
	case RRC_STATE_HIGH_60Hz:
	default:
		refresh = RRC_DRV_60Hz;
		break;
	}

	return refresh;

}


static int rrc_get_avg_fps_range(void)
{
	int fps_range = _rrc_avg_fps_range;

	/* check if avg FPS is lower than 60FPS */
	return fps_range;
}

static int rrc_sample_scenario_state(void)
{
	int i = 0;

	if (new_event_flag) {
		for (i = 0; i < RRC_DRV_TYPE_MAX_SIZE; i++)
			scenario_status[i] = scenario_back_status[i];

		new_event_flag = 0;
	}



	return 0;
}


static int rrc_get_next_refresh_rate(void)
{
	int i = 0;
	int video_state = 0;
	int touch_state = 0;
	/* int j = scenario_status_next & 0x1; */

	for (i = RRC_DRV_TYPE_CAMERA_PREVIEW; i <= RRC_DRV_TYPE_VIDEO_WIFI_DISPLAY; i++) {
		if (scenario_status[i] > 0) {
			video_state++;
			if (_is_video_120hz_en
				&& (i == RRC_DRV_TYPE_VIDEO_SWDEC_PLAYBACK || i == RRC_DRV_TYPE_VIDEO_PLAYBACK)) {
				rrc_set_refresh_state(RRC_STATE_VIDEO_120Hz);
				break;
			}

			rrc_set_refresh_state(RRC_STATE_VIDEO);

		}
	}

	if (video_state == 0) {

		touch_state = scenario_status[RRC_DRV_TYPE_TOUCH_EVENT];

		if (touch_state > 0) {
			/* if avg fps can exceed 120FPS, will change to 60Hz */
			if (rrc_get_avg_fps_range() == RRC_DRV_120Hz)
				rrc_set_refresh_state(RRC_STATE_HIGH_120Hz);
			else
				rrc_set_refresh_state(RRC_STATE_HIGH_60Hz);

		} else {
			rrc_set_refresh_state(RRC_STATE_NORMAL);
		}
	}

	return rrc_get_curr_refresh_rate();

}

#if 0
static int rrc_is_touch_event(void)
{
	return scenario_status[RRC_DRV_TYPE_TOUCH_EVENT];
}

static int rrc_monitor_fps_kthread_func(void *data)
{

	struct sched_param param = { .sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);

	while (1) {
		wait_event_interruptible(set_refresh_rate_wq, set_refresh_rate_task_wakeup);
		set_refresh_rate_task_wakeup = 0;

		/* TODO: monitor FPS to change refresh range (while user is keep touching) */

	}

	return 0;

}

#endif


static int rrc_set_low_refresh_rate_kthread_func(void *data)
{
	int rrc_low_sample_cnt = 0;
	int curr_refresh;
	int state;
	int run_flag;

	struct sched_param param = { .sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);


	while (1) {
		RRC_WRN("[RRC_DRV] kthread_func : rrc_set_low_refresh_rate_kthread_func %d (sleep)!!\n",
			set_low_refresh_rate_task_wakeup);
		wait_event_interruptible(set_low_refresh_rate_wq, set_low_refresh_rate_task_wakeup);
		set_low_refresh_rate_task_wakeup = 0;
		RRC_WRN("[RRC_DRV] kthread_func : rrc_set_low_refresh_rate_kthread_func %d (wake)!!\n",
			set_low_refresh_rate_task_wakeup);

		/* reset new change event */
		/* _is_new_change_refresh_event = 0; */


		run_flag = 1;
		rrc_low_sample_cnt = 0;


		while (run_flag) {

			state = rrc_get_refresh_state();
			curr_refresh = rrc_get_curr_refresh_rate();

			if (RRC_STATE_VIDEO == state) {
				/* video state needs set low refresh rate immediately */
				RRC_WRN("[RRC_DRV] state %d, force set low refresh rate now!!\n", state);
				rrc_set_curr_refresh_rate(RRC_DRV_60Hz);
				run_flag = 0;
			}

			/* check new refresh rate when new event happens */
			if (run_flag && _is_new_change_refresh_event) {

				_is_new_change_refresh_event = 0;
				rrc_low_sample_cnt = 0;

				if (curr_refresh == RRC_DRV_120Hz) {
					/* go back to sleep for next touch DOWN event */
					run_flag = 0;
					RRC_WRN("[RRC_DRV] sample new 120Hz event, skip set low refresh rate!!\n");
				}
			}

			/* increment low sample count */
			if (run_flag && (++rrc_low_sample_cnt == RRC_DRV_LOW_SAMPLE_COUNT)) {
				rrc_low_sample_cnt = 0;
				if (curr_refresh == RRC_DRV_60Hz) {
					/* go back to sleep for next touch DOWN event */
					rrc_set_curr_refresh_rate(RRC_DRV_60Hz);
					run_flag = 0;
				}
			}

			if (run_flag) {
				/* sleep for a while*/
				msleep_interruptible(RRC_SAMPLE_INTERVAL);
			}

		}

	}

	return 0;


}


static int rrc_set_refresh_rate_kthread_func(void *data)
{

	int target_refresh = 0;
	int curr_refresh = 0;
	/* int event;	*/
	/* int enable; */

	struct sched_param param = { .sched_priority = 94 };

	sched_setscheduler(current, SCHED_RR, &param);



	while (1) {
		RRC_WRN("[RRC_DRV] kthread_func : set_refresh_rate_task_wakeup %d (sleep)!!\n",
			set_refresh_rate_task_wakeup);
		wait_event_interruptible(set_refresh_rate_wq, set_refresh_rate_task_wakeup);
		set_refresh_rate_task_wakeup = 0;
		RRC_WRN("[RRC_DRV] kthread_func : set_refresh_rate_task_wakeup %d (wake)!!\n",
			set_refresh_rate_task_wakeup);

		/* get current refresh state */
		curr_refresh = rrc_get_curr_refresh_rate();

		/* sample event change scenario state */
		rrc_sample_scenario_state();

		/* check if need to change next refresh rate */
		target_refresh = rrc_get_next_refresh_rate();

		RRC_WRN("[RRC_DRV] kthread_func : curr %d, target %d!!\n", curr_refresh, target_refresh);

		_is_new_change_refresh_event = 1;


		if (target_refresh != RRC_DRV_NONE) {

			if (target_refresh == RRC_DRV_60Hz) {

				/* trigger set low refresh rate thread */
				set_low_refresh_rate_task_wakeup = 1;
				wake_up_interruptible(&set_low_refresh_rate_wq);
			} else {
				/* update current refresh rate */
				rrc_set_curr_refresh_rate(target_refresh);
			}

			/* TODO: 60Hz->120Hz wake up FPS monitor to check FPS capability */

			/* TODO: 120Hz->60Hz disable FPS monitor to check FPS capability */
		}

	}

	return 0;


}

#if 0


static int rrc_reset_sample_count(void)
{
	_rrc_sample_cnt = 0;
	return _rrc_sample_cnt;
}

static int rrc_get_sample_count(void)
{
	return _rrc_sample_cnt;
}

static int rrc_inc_sample_count(void)
{
	_rrc_sample_cnt++;
	return _rrc_sample_cnt;
}


#endif



static int rrc_set_scenario_state(int scenario, int enable)
{
	int switch_flag = 0;
	/* int j = scenario_status_next & 0x1; */
	int status = 0;


	if (enable) {
		if (scenario == RRC_DRV_TYPE_TOUCH_EVENT) {
			status = scenario_back_status[scenario] = 1;
		} else {
			status = ++scenario_back_status[scenario];
			if (enable == 2 &&
				(scenario == RRC_DRV_TYPE_VIDEO_SWDEC_PLAYBACK ||
				scenario == RRC_DRV_TYPE_VIDEO_PLAYBACK)) {
				_is_video_120hz_en = 1;
				switch_flag = 1;
				new_event_flag = 1;
			}

		}

		if (status == 1) {
			switch_flag = 1;
			new_event_flag = 1;
		}
	} else {
		status = scenario_back_status[scenario];
		if (_is_video_120hz_en &&
			(scenario == RRC_DRV_TYPE_VIDEO_SWDEC_PLAYBACK || scenario == RRC_DRV_TYPE_VIDEO_PLAYBACK)) {
			_is_video_120hz_en = 0;
			switch_flag = 1;
			new_event_flag = 1;
		}
		if (status > 0) {
			if (--scenario_back_status[scenario] == 0) {
				switch_flag = 1;
				new_event_flag = 1;
			}
		}

	}
	RRC_WRN("[RRC_DRV]set_scenario: scenario %d, status %d, enable %d, switch %d, new_event %d!!\n",
		scenario, status, enable, switch_flag, new_event_flag);

	return switch_flag;

}


static int rrc_notify_scenario_event(int scenario, int enable)
{

	/* int next_rate; */
	/* int curr_rate; */

	if (scenario > RRC_DRV_TYPE_NONE && scenario < RRC_DRV_TYPE_MAX_SIZE) {


		if (rrc_set_scenario_state(scenario, enable)) {
			set_refresh_rate_task_wakeup = 1;
			wake_up_interruptible(&set_refresh_rate_wq);
		}

		/* reset fps count */
		/* rrc_reset_sample_count(); */

		return 0;

	}

	return -1;
}



static long rrc_ioctl(unsigned int cmd, unsigned long arg, struct file *file)
{
	RRC_DRV_DATA drv_data;
	/* unsigned int max_safe_size; */
	unsigned int result;

	unsigned int *pStatus;

	pStatus = (unsigned int *)file->private_data;

	if (NULL == pStatus) {
		RRC_WRN("[RRC_DRV] Private data is null in flush operation. HOW COULD THIS HAPPEN ??\n");
		return -EFAULT;
	}
	/* RRC_DBG("[RRC_DRV] RRC Driver rrc_ioctl cmd %d, set_type %lx, 64data %lx, 32data %lx!!\n",
		cmd, RRC_IOCTL_CMD_SET_SCENARIO_TYPE,sizeof(RRC_DRV_DATA), sizeof(RRC_DRV_COMPAT_DATA)); */

	switch (cmd) {

	/* initial and reset RRC */
	case RRC_IOCTL_CMD_INIT:
		RRC_DBG("[RRC_DRV] RRC Driver Initial and Lock\n");

		*pStatus = RRC_PROCESS;

		break;

	case RRC_IOCTL_CMD_SET_SCENARIO_TYPE:
		RRC_DBG("[RRC_DRV] RRC Driver SET scenario type!!\n");
		if (*pStatus != RRC_PROCESS) {
			RRC_WRN("Permission Denied! This process can not access RRC Driver");
			return -EFAULT;
		}


		/* RRC_DBG("[RRC_DRV] RRC Driver SET scenario type copy_from_user!!\n"); */
		if (copy_from_user(&drv_data, (void *)arg, sizeof(RRC_DRV_DATA))) {
			RRC_WRN("[RRC_DRV] RRC Driver : Copy from user error\n");
			return -EFAULT;
		}


		RRC_DBG("[RRC_DRV] RRC Driver SET scenario type rrc_notify_scenario_event!!\n");
		result = rrc_notify_scenario_event(drv_data.scenario, drv_data.enable);
		if (result < 0) {
			RRC_DBG("[RRC_DRV] RRC Driver SET scenario type error %d!!\n", result);
			return -EFAULT;
		}

		/*
		RRC_DBG("[RRC_DRV] RRC Driver SET scenario type copy_to_user!!\n");
		if (copy_to_user(drv_data.result, &result, sizeof(unsigned int)))
		{
		RRC_WRN("[RRC_DRV] RRC Driver : Copy to user error (result)\n");
		return -EFAULT;
		}
		*/
		break;


	case RRC_IOCTL_CMD_DEINIT:
		/* copy input parameters */
		RRC_DBG("[RRC_DRV] RRC Driver Deinit!!\n");
		if (*pStatus != RRC_PROCESS) {
			RRC_WRN("[RRC_DRV] Permission Denied! This process can not access RRC Driver");
			return -EFAULT;
		}

		*pStatus = 0;

		return 0L;

	}

	return 0L;
}
/* static int rrc_ioctl(struct inode *inode, struct file *file, unsigned int cmd, unsigned long arg) */
static long rrc_unlocked_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	switch (cmd) {

	case RRC_IOCTL_CMD_INIT:
	case RRC_IOCTL_CMD_SET_SCENARIO_TYPE:
	case RRC_IOCTL_CMD_DEINIT:
		return rrc_ioctl(cmd, arg, file);

	default:
		break;
	}

	return -EINVAL;
}

#if IS_ENABLED(CONFIG_COMPAT)

typedef struct {
	compat_uint_t scenario;
	compat_uint_t enable;
	/* compat_uptr_t *result; */

} RRC_DRV_COMPAT_DATA;


#define COMPAT_RRC_IOCTL_CMD_SET_SCENARIO_TYPE	_IOWR(RRC_IOCTL_MAGIC,	12, RRC_DRV_COMPAT_DATA)




static int compat_get_scenario_type_data(
	RRC_DRV_COMPAT_DATA __user *data32,
	RRC_DRV_DATA __user *data)
{
	compat_uint_t i;
	int err;

	err	= get_user(i, &data32->scenario);
	err |= put_user(i, &data->scenario);
	err |= get_user(i, &data32->enable);
	err |= put_user(i, &data->enable);
	/* err |= get_user(i, &data32->result);	*/
	/* err |= put_user(compat_ptr(i), &data->result);	*/
	return err;
}





long compat_rrc_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;

	RRC_WRN("compat_rrc_ioctl go L:%d, cmd %x, SCENTYPE %lx!!\n",
		__LINE__, cmd, COMPAT_RRC_IOCTL_CMD_SET_SCENARIO_TYPE);

	if (!filp->f_op || !filp->f_op->unlocked_ioctl) {
		RRC_WRN("compat_rrc_ioctl go L:%d!!\n", __LINE__);
		return -ENOTTY;
	}

	switch (cmd) {

	/*
	case COMPAT_MTK_IOC_SMI_BWC_CONFIG:
	{
	if (COMPAT_MTK_IOC_SMI_BWC_CONFIG == MTK_IOC_SMI_BWC_CONFIG)
	{
	SMIMSG("COMPAT_MTK_IOC_SMI_BWC_CONFIG\n");
	return filp->f_op->unlocked_ioctl(filp, cmd,(unsigned long)compat_ptr(arg));
	} else{

	MTK_SMI_COMPAT_BWC_CONFIG __user *data32;
	MTK_SMI_BWC_CONFIG __user *data;
	int err;

	data32 = compat_ptr(arg);
	data = compat_alloc_user_space(sizeof(MTK_SMI_BWC_CONFIG));

	if (data == NULL)
	return -EFAULT;

	err = compat_get_smi_bwc_config_struct(data32, data);
	if (err)
	return err;

	ret = filp->f_op->unlocked_ioctl(filp, MTK_IOC_SMI_BWC_CONFIG,
	(unsigned long)data);
	return ret;
	}
	}

	case COMPAT_JPEG_DEC_IOCTL_WAIT:
	{
	compat_JPEG_DEC_DRV_OUT __user *data32;
	JPEG_DEC_DRV_OUT __user *data;
	int err;

	data32 = compat_ptr(arg);
	data = compat_alloc_user_space(sizeof(*data));
	if (data == NULL)
	return -EFAULT;

	err = compat_get_jpeg_dec_ioctl_wait_data(data32, data);
	if (err)
	return err;
	ret = filp->f_op->unlocked_ioctl(filp, JPEG_DEC_IOCTL_WAIT,(unsigned long)data);
	err = compat_put_jpeg_dec_ioctl_wait_data(data32, data);
	return ret ? ret : err;
	}
	*/

	case COMPAT_RRC_IOCTL_CMD_SET_SCENARIO_TYPE:
	{
		RRC_DRV_COMPAT_DATA __user *data32;
		RRC_DRV_DATA __user *data;
		int err;

		if (COMPAT_RRC_IOCTL_CMD_SET_SCENARIO_TYPE == RRC_IOCTL_CMD_SET_SCENARIO_TYPE) {
			RRC_WRN("COMPAT_RRC_IOCTL_CMD_SET_SCENARIO_TYPE equl\n");
			return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
		}

		RRC_WRN("COMPAT_RRC_IOCTL_CMD_SET_SCENARIO_TYPE go!!\n");

		data32 = compat_ptr(arg);
		data = compat_alloc_user_space(sizeof(*data));
		if (data == NULL)
			return -EFAULT;

		err = compat_get_scenario_type_data(data32, data);
		if (err)
			return err;
		ret = filp->f_op->unlocked_ioctl(filp, RRC_IOCTL_CMD_SET_SCENARIO_TYPE, (unsigned long)data);
		/* err = compat_put_jpeg_dec_ioctl_wait_data(data32, data); */
		return ret;

	}

	case RRC_IOCTL_CMD_INIT:
	case RRC_IOCTL_CMD_DEINIT:

		return filp->f_op->unlocked_ioctl(filp, cmd, (unsigned long)compat_ptr(arg));
	default:
		return -ENOIOCTLCMD;
	}

}



#endif

static int rrc_open(struct inode *inode, struct file *file)
{
	unsigned int *pStatus;
	/* Allocate and initialize private data */
	file->private_data = kmalloc(sizeof(unsigned int) , GFP_ATOMIC);

	if (NULL == file->private_data) {
		RRC_WRN("Not enough entry for RRC open operation\n");
		return -ENOMEM;
	}

	pStatus = (unsigned int *)file->private_data;
	*pStatus = 0;

	return 0;
}

static ssize_t rrc_read(struct file *file, char __user *data, size_t len, loff_t *ppos)
{
	RRC_DBG("RRC driver read\n");
	return 0;
}

static int rrc_release(struct inode *inode, struct file *file)
{
	if (NULL != file->private_data) {
		kfree(file->private_data);
		file->private_data = NULL;
	}
	return 0;
}

static int rrc_flush(struct file *a_pstFile , fl_owner_t a_id)
{
	unsigned int *pStatus;

	pStatus = (unsigned int *)a_pstFile->private_data;

	if (NULL == pStatus) {
		RRC_WRN("Private data is null in flush operation. HOW COULD THIS HAPPEN ??\n");
		return -EFAULT;
	}


	return 0;
}

/* Kernel interface */
static const struct file_operations rrc_fops = {
	.owner		= THIS_MODULE,
	/* .ioctl		= rrc_ioctl, */
	.unlocked_ioctl = rrc_unlocked_ioctl,
#if IS_ENABLED(CONFIG_COMPAT)
	.compat_ioctl	= compat_rrc_ioctl,
#endif
	.open		= rrc_open,
	.release	= rrc_release,
	.flush		= rrc_flush,
	.read	= rrc_read,
};

static int rrc_probe(struct platform_device *pdev)
{
	struct class_device;

	int ret;
	struct class_device *class_dev = NULL;
	int i = 0;

	RRC_DBG("-------------rrc driver probe-------\n");
	ret = alloc_chrdev_region(&rrc_devno, 0, 1, RRC_DEVNAME);

	if (ret)
		RRC_ERR("Error: Can't Get Major number for RRC Device\n");
	else
		RRC_DBG("Get RRC Device Major number (%d)\n", rrc_devno);


	rrc_cdev = cdev_alloc();
	rrc_cdev->owner = THIS_MODULE;
	rrc_cdev->ops = &rrc_fops;

	ret = cdev_add(rrc_cdev, rrc_devno, 1);

	rrc_class = class_create(THIS_MODULE, RRC_DEVNAME);
	class_dev = (struct class_device *)device_create(rrc_class, NULL, rrc_devno, NULL, RRC_DEVNAME);

	spin_lock_init(&rrc_lock);

	/* initial driver, register driver ISR */
	rrc_status = 0;
	_rrc_int_status = 0;

	for (i = 0; i < RRC_DRV_TYPE_MAX_SIZE-1; i++) {
		scenario_back_status[i] = 0;
		scenario_status[i] = 0;
		new_event_flag = 0;
	}
	is_touch_event = 0;
	is_video_scenario = 0;
	_rrc_avg_fps_range = RRC_DRV_120Hz;

	RRC_DBG("RRC Probe Done\n");

	/* NOT_REFERENCED(class_dev); */
	return 0;
}

static int rrc_remove(struct platform_device *pdev)
{
	RRC_DBG("RRC driver remove\n");
	RRC_DBG("Done\n");
	return 0;
}

static void rrc_shutdown(struct platform_device *pdev)
{
	RRC_DBG("RRC driver shutdown\n");
	/* Nothing yet */
}

/* PM suspend */
static int rrc_suspend(struct platform_device *pdev, pm_message_t mesg)
{
	/* rrc_drv_dec_deinit(); */
	/* rrc_drv_enc_deinit(); */
	return 0;
}

/* PM resume */
static int rrc_resume(struct platform_device *pdev)
{
	return 0;
}


static struct platform_driver rrc_driver = {
	.probe		= rrc_probe,
	.remove		= rrc_remove,
	.shutdown	= rrc_shutdown,
	.suspend	= rrc_suspend,
	.resume		= rrc_resume,
	.driver	= {
		.name = RRC_DEVNAME,
	},
};

static void rrc_device_release(struct device *dev)
{
	/* Nothing to release? */
}

static u64 rrc_dmamask = ~(u32)0;

static struct platform_device rrc_device = {
	.name	 = RRC_DEVNAME,
	.id	= 0,
	.dev	= {
		.release = rrc_device_release,
		.dma_mask = &rrc_dmamask,
		.coherent_dma_mask = 0xffffffff,
	},
	.num_resources = 0,
};

static int __init rrc_init(void)
{
	int ret;

	RRC_DBG("RRC driver initialize\n");

	RRC_DBG("Register the RRC driver device\n");
	if (platform_device_register(&rrc_device)) {
		RRC_ERR("failed to register rrc driver device\n");
		ret = -ENODEV;
		return ret;
	}

	RRC_DBG("Register the RRC driver\n");
	if (platform_driver_register(&rrc_driver)) {
		RRC_ERR("failed to register rrc driver\n");
		platform_device_unregister(&rrc_device);
		ret = -ENODEV;
		return ret;
	}

	/* change refresh rate */

	init_waitqueue_head(&set_refresh_rate_wq);

	set_refresh_rate_task = kthread_create(
	rrc_set_refresh_rate_kthread_func, NULL, "rrc_set_refresh_rate_kthread_func");

	if (IS_ERR(set_refresh_rate_task)) {
		RRC_WRN("RRC: Cannot create rrc_set_refresh_rate_kthread_func!!\n");
		ret = -ENODEV;
		return ret;
	}
	wake_up_process(set_refresh_rate_task);


	/* set low frame rate */

	init_waitqueue_head(&set_low_refresh_rate_wq);

	set_low_refresh_rate_task = kthread_create(
	rrc_set_low_refresh_rate_kthread_func, NULL, "rrc_set_low_refresh_rate_kthread_func");

	if (IS_ERR(set_low_refresh_rate_task)) {
		RRC_WRN("RRC: Cannot create rrc_set_low_refresh_rate_kthread_func!!\n");
		ret = -ENODEV;
		return ret;
	}
	wake_up_process(set_low_refresh_rate_task);

	return 0;
}

static void __exit rrc_exit(void)
{
	cdev_del(rrc_cdev);
	unregister_chrdev_region(rrc_devno, 1);
	/* RRC_WRN("Unregistering driver\n"); */
	platform_driver_unregister(&rrc_driver);
	platform_device_unregister(&rrc_device);

	device_destroy(rrc_class, rrc_devno);
	class_destroy(rrc_class);

	RRC_DBG("Done\n");
}

module_init(rrc_init);
module_exit(rrc_exit);
MODULE_AUTHOR("Otis, Huang <otis.huang@mediatek.com>");
MODULE_DESCRIPTION("RRC driver");
MODULE_LICENSE("GPL");
