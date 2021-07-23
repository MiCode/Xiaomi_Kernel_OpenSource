/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mtk-scp-spk-misc.c --  Mediatek scp spk platform
 *
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Shane Chien <shane.chien@mediatek.com>
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/notifier.h>
#include <linux/uaccess.h> /* needed by copy_to_user */

#include "scp_helper.h"
#include "scp_ipi.h"
#include "scp_feature_define.h"

#define AUDIO_SCP_IOC_MAGIC 's'
#define AUDIO_SCP_IOCTL_QUERY_STATUS \
	_IOR(AUDIO_SCP_IOC_MAGIC, 0x0, unsigned int)
#define AUDIO_SCP_IOCTL_RESET_CBK \
	_IOR(AUDIO_SCP_IOC_MAGIC, 0x1, unsigned int)


static DECLARE_WAIT_QUEUE_HEAD(scp_status_wq);
static unsigned int last_scp_status;
static int status_update_flag;

void scp_read_status_release(const unsigned long scp_event)
{
	last_scp_status = scp_event;
	if (status_update_flag == 0) {
		status_update_flag = 1;
		pr_info("wake up event: %lu\n", scp_event);
		wake_up_interruptible(&scp_status_wq);
	}
}

static int scp_read_status_blocked(void)
{
	int status = 0;
	int retval = 0;

	retval = wait_event_interruptible(scp_status_wq,
					  (status_update_flag > 0));
	if (retval == -ERESTARTSYS) {
		pr_info("query scp status -ERESTARTSYS\n");
		status = -EINTR;
	} else if (retval == 0) {
		status = last_scp_status;
		status_update_flag = 0;
		pr_info("query scp status wakeup %d\n", status);
	} else
		status = -1;

	return status;
}

#if defined(SCP_RECOVERY_SUPPORT)
static int audio_scp_event_receive(struct notifier_block *this,
				   unsigned long event, void *ptr)
{
	scp_read_status_release(event);
	return NOTIFY_DONE;
}

static struct notifier_block scp_spk_ready_notifier = {
	.notifier_call = audio_scp_event_receive,
};
#endif

#ifdef CONFIG_COMPAT
static long audio_scp_misc_compat_ioctl(struct file *fp, unsigned int cmd,
					unsigned long arg)
{
	long ret;

	if (!fp->f_op || !fp->f_op->unlocked_ioctl)
		return -ENOTTY;
	ret = fp->f_op->unlocked_ioctl(fp, cmd, arg);
	if (ret < 0)
		pr_err("%s(), fail, ret %ld, cmd 0x%x, arg %lu\n",
		       __func__, ret, cmd, arg);
	return ret;
}
#endif

static long audio_scp_misc_ioctl(struct file *fp,
				 unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int scp_status;

	switch (cmd) {
	case AUDIO_SCP_IOCTL_QUERY_STATUS:
		scp_status = is_scp_ready(SCP_A_ID);
		ret = copy_to_user((void __user *)arg,
				   &scp_status, sizeof(scp_status));
		if (ret)
			pr_warn("Fail copy to user Ptr:%p, r_sz:%zu\n",
				(char *)&scp_status, sizeof(scp_status));

		pr_debug("%s(), AUDIO_SCP_IOCTL_QUERY_STATUS(%d)\n",
			 __func__, scp_status);
		break;
	case AUDIO_SCP_IOCTL_RESET_CBK:
		ret = scp_read_status_blocked();
		pr_info("%s(), AUDIO_SCP_IOCTL_RESET_CBK(%d)\n",
			__func__, ret);
		break;
	default:
		pr_warn("%s(), cmd: %d\n", __func__, cmd);
		break;
	};

	return ret;
}

static int audio_scp_misc_open(struct inode *inode, struct file *file)
{
	/*pr_info("%s()\n", __func__);*/
	return nonseekable_open(inode, file);
}

static const struct file_operations audio_scp_misc_fops = {
	.owner = THIS_MODULE,
	.open = audio_scp_misc_open,
	.unlocked_ioctl = audio_scp_misc_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = audio_scp_misc_compat_ioctl,
#endif
};

static struct miscdevice audio_scp_miscdevice = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "audio_scp",
	.fops = &audio_scp_misc_fops
};

static int __init audio_scp_misc_init(void)
{
	int ret = 0;

	ret = misc_register(&audio_scp_miscdevice);
	if (ret) {
		pr_err("%s(), cannot audio_scp_miscdevice miscdev on minor %d, ret %d\n",
		       __func__, audio_scp_miscdevice.minor, ret);
	}
#if defined(SCP_RECOVERY_SUPPORT)
	scp_A_register_notify(&scp_spk_ready_notifier);
#endif
	return ret;
}

static void __exit audio_scp_misc_exit(void)
{
	misc_deregister(&audio_scp_miscdevice);
}

module_init(audio_scp_misc_init);
module_exit(audio_scp_misc_exit);

MODULE_DESCRIPTION("Mediatek audio scp misc driver");
MODULE_AUTHOR("Shane Chien <Shane.Chien@mediatek.com>");
MODULE_LICENSE("GPL v2");

