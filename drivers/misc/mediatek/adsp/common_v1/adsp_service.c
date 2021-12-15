// SPDX-License-Identifier: GPL-2.0
//
// adsp_service.c --	Mediatek ADSP service for ioctrl handling
//
// Copyright (c) 2018 MediaTek Inc.
// Author: HsinYi Chang <hsin-yi.chang@mediatek.com>


#include <linux/module.h>       /* needed by all modules */
#include <linux/init.h>         /* needed by module macros */
#include <linux/fs.h>           /* needed by file_operations* */
#include <linux/miscdevice.h>   /* needed by miscdevice* */
#include <linux/sysfs.h>
#include <linux/device.h>       /* needed by device_* */
#include <linux/vmalloc.h>      /* needed by kmalloc */
#include <linux/uaccess.h>      /* needed by copy_to_user */
#include <linux/slab.h>         /* needed by kmalloc */
#include <linux/poll.h>         /* needed by poll */
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/interrupt.h>
#include <linux/suspend.h>
#include <linux/timer.h>
#include <linux/notifier.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#include <linux/of_fdt.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <adsp_ipi.h>
#include <adsp_helper.h>
#include <adsp_dvfs.h>

/*==============================================================================
 *                     ioctl
 *==============================================================================
 */
#define AUDIO_DSP_DEVICE_PATH "/dev/adsp"
#define AUDIO_DSP_IOC_MAGIC 'a'

#define AUDIO_DSP_IOCTL_ADSP_REG_FEATURE  \
	_IOW(AUDIO_DSP_IOC_MAGIC, 0, unsigned int)
#define AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS \
	_IOR(AUDIO_DSP_IOC_MAGIC, 1, unsigned int)
#define AUDIO_DSP_IOCTL_ADSP_RESET_CBK \
	_IOR(AUDIO_DSP_IOC_MAGIC, 2, unsigned int)

/*
 * =============================================================================
 *                     struct
 * =============================================================================
 */

struct audio_dsp_reg_feature_t {
	uint16_t reg_flag;
	uint16_t feature_id;
};

struct audio_dsp_query_status_t {
	int16_t ready_flag;
	uint16_t core_id;
};
/*
 * =============================================================================
 *                     static var
 * =============================================================================
 */
static long status_update_flag;
static DECLARE_WAIT_QUEUE_HEAD(status_wq);
static unsigned long last_dsp_status;
static int16_t last_ready_flag;
static int adsp_hal_feature_table[ADSP_NUM_FEATURE_ID];


/*==============================================================================
 *                     functions - implementation
 *==============================================================================
 */
void adsp_read_status_release(const unsigned long dsp_event)
{
	last_dsp_status = dsp_event;
	if (status_update_flag == 0) {
		status_update_flag = 1;
		pr_info("wake up event: %lu\n", dsp_event);
		wake_up_interruptible(&status_wq);
	}
}

static int adsp_read_status_blocked(void)
{
	int status = 0;
	int retval = 0;

	retval = wait_event_interruptible(status_wq,
				 (status_update_flag > 0));
	if (retval == -ERESTARTSYS) {
		pr_info("query adsp status -ERESTARTSYS");
		status = -EINTR;
	} else if (retval == 0) {
		status = last_dsp_status;
		status_update_flag = 0;
		pr_info("query adsp status wakeup  %d\n", status);
	} else
		status = -1;

	return status;
}

#ifdef CONFIG_COMPAT
long adsp_driver_compat_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{
	if (!file->f_op || !file->f_op->unlocked_ioctl) {
		pr_notice("op null\n");
		return -ENOTTY;
	}
	return file->f_op->unlocked_ioctl(file, cmd, arg);
}
#endif

void reset_hal_feature_table(void)
{
	int i;

	for (i = 0; i < ADSP_NUM_FEATURE_ID; i++) {
		while (adsp_hal_feature_table[i] > 0) {
			adsp_deregister_feature(i);
			adsp_hal_feature_table[i]--;
		}
	}
}

long adsp_driver_ioctl(
	struct file *file, unsigned int cmd, unsigned long arg)
{

	struct audio_dsp_reg_feature_t feat_reg;
	struct audio_dsp_query_status_t adsp_status;

	int retval = 0;
	unsigned int magic[2];

	switch (cmd) {
	case AUDIO_DSP_IOCTL_ADSP_REG_FEATURE: {
		if (((void __user *)arg) == NULL) {
			retval = -1;
			break;
		}
		retval = copy_from_user(
				 &feat_reg,
				 (void __user *)arg,
				 sizeof(struct audio_dsp_reg_feature_t));
		if (retval != 0) {
			pr_notice("%s(), feature reg copy_from_user retval %d\n",
				  __func__, retval);
			break;
		}
		if (feat_reg.feature_id >= ADSP_NUM_FEATURE_ID) {
			retval = -EINVAL;
			break;
		}
		if (feat_reg.reg_flag)
			retval = adsp_register_feature(feat_reg.feature_id);
		else
			retval = adsp_deregister_feature(feat_reg.feature_id);

		if (retval == 0) {
			if (feat_reg.reg_flag)
				adsp_hal_feature_table[feat_reg.feature_id]++;
			else
				adsp_hal_feature_table[feat_reg.feature_id]--;

			if (adsp_hal_feature_table[feat_reg.feature_id] < 0)
				adsp_hal_feature_table[feat_reg.feature_id] = 0;
		}
		break;
	}
	case AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS: {
		if (((void __user *)arg) == NULL) {
			retval = -EINVAL;
			break;
		}
		retval = copy_from_user(
				 &adsp_status,
				 (void __user *)arg,
				 sizeof(struct audio_dsp_query_status_t));
		if (retval)
			pr_debug("%s copy_from_user fail line %d\n",
				  __func__, __LINE__);

		adsp_status.ready_flag = is_adsp_ready(adsp_status.core_id);

		retval = copy_to_user((void __user *)arg, &adsp_status,
				 sizeof(struct audio_dsp_reg_feature_t));
		if (retval)
			pr_debug("%s copy_to_user fail line %d\n",
				  __func__, __LINE__);
		if (adsp_status.ready_flag != last_ready_flag)
			pr_debug("%s(), AUDIO_DSP_IOCTL_ADSP_QUERY_STATUS(%d)\n",
				  __func__, adsp_status.ready_flag);
		last_ready_flag = adsp_status.ready_flag;

		break;
	}
	case AUDIO_DSP_IOCTL_ADSP_RESET_CBK: {
		if (copy_from_user(&magic, (void *)arg, sizeof(magic))) {
			retval = -EINVAL;
			break;
		}
		if (magic[0] + magic[1] == 0xFFFFFFFF) {
			retval = adsp_read_status_blocked();
			pr_debug("%s(), AUDIO_DSP_IOCTL_ADSP_RESET_CBK(%d)\n",
				 __func__, retval);
		}
		break;
	}
	}

	return retval;
}

