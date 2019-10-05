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
#include "perf_ioctl.h"

#ifdef CONFIG_MTK_FPSGO
#include "tchbst.h"
#include "io_ctrl.h"
#endif


#define TAG "PERF_IOCTL"

void (*fpsgo_notify_qudeq_fp)(int qudeq,
		unsigned int startend,
		unsigned long long bufID, int pid, int queue_SF);
void (*fpsgo_notify_intended_vsync_fp)(int pid, unsigned long long frame_id);
void (*fpsgo_notify_framecomplete_fp)(int ui_pid,
		unsigned long long frame_time,
		int render_method, int render, unsigned long long frame_id);
void (*fpsgo_notify_connect_fp)(int pid,
		unsigned long long bufID, int connectedAPI);
void (*fpsgo_notify_draw_start_fp)(int pid, unsigned long long frame_id);

unsigned long perfctl_copy_from_user(void *pvTo,
		const void __user *pvFrom, unsigned long ulBytes)
{
	if (access_ok(VERIFY_READ, pvFrom, ulBytes))
		return __copy_from_user(pvTo, pvFrom, ulBytes);

	return ulBytes;
}

/*--------------------DEV OP------------------------*/
static ssize_t device_write(struct file *filp, const char *ubuf,
		size_t cnt, loff_t *data)
{
	char buf[32], cmd[32];
	int arg;

	arg = 0;

	if (cnt >= sizeof(buf))
		return -EINVAL;

	if (copy_from_user(buf, ubuf, cnt))
		return -EFAULT;
	buf[cnt] = '\0';

	if (sscanf(buf, "%31s %d", cmd, &arg) != 2)
		return -EFAULT;

	return cnt;
}

static int device_show(struct seq_file *m, void *v)
{
	return 0;
}

static int device_open(struct inode *inode, struct file *file)
{
	return single_open(file, device_show, inode->i_private);
}

static long device_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
	ssize_t ret = 0;
	struct _FPSGO_PACKAGE *msgKM = NULL,
			*msgUM = (struct _FPSGO_PACKAGE *)arg;
	struct _FPSGO_PACKAGE smsgKM;

	msgKM = &smsgKM;

	if (perfctl_copy_from_user(msgKM, msgUM,
				sizeof(struct _FPSGO_PACKAGE))) {
		ret = -EFAULT;
		goto ret_ioctl;
	}

	switch (cmd) {
#ifdef CONFIG_MTK_FPSGO
	case FPSGO_FRAME_COMPLETE:
		if (fpsgo_notify_framecomplete_fp)
			fpsgo_notify_framecomplete_fp(msgKM->tid,
					msgKM->frame_time,
					msgKM->render_method, 1,
					msgKM->frame_id);
		break;
	case FPSGO_INTENDED_VSYNC:
		if (fpsgo_notify_intended_vsync_fp)
			fpsgo_notify_intended_vsync_fp(msgKM->tid,
					msgKM->frame_id);
		break;
	case FPSGO_NO_RENDER:
		if (fpsgo_notify_framecomplete_fp)
			fpsgo_notify_framecomplete_fp(msgKM->tid,
					0, 0, 0, msgKM->frame_id);
		break;
	case FPSGO_DRAW_START:
		if (fpsgo_notify_draw_start_fp)
			fpsgo_notify_draw_start_fp(msgKM->tid,
					msgKM->frame_id);
		break;
	case FPSGO_QUEUE:
		if (fpsgo_notify_qudeq_fp)
			fpsgo_notify_qudeq_fp(1,
					msgKM->start, msgKM->bufID,
					msgKM->tid, msgKM->queue_SF);
		break;
	case FPSGO_DEQUEUE:
		if (fpsgo_notify_qudeq_fp)
			fpsgo_notify_qudeq_fp(0,
					msgKM->start, msgKM->bufID,
					msgKM->tid, msgKM->queue_SF);
		break;
	case FPSGO_QUEUE_CONNECT:
		if (fpsgo_notify_connect_fp)
			fpsgo_notify_connect_fp(msgKM->tid,
					msgKM->bufID, msgKM->connectedAPI);
		break;
	case FPSGO_TOUCH:
		usrtch_ioctl(cmd, msgKM->frame_time);
		break;
	case FPSGO_ACT_SWITCH:
		/* FALLTHROUGH */
	case FPSGO_GAME:
		/* FALLTHROUGH */
	case FPSGO_SWAP_BUFFER:
		break;
#else
	case FPSGO_TOUCH:
		/* FALLTHROUGH */
	case FPSGO_FRAME_COMPLETE:
		/* FALLTHROUGH */
	case FPSGO_QUEUE:
		/* FALLTHROUGH */
	case FPSGO_DEQUEUE:
		/* FALLTHROUGH */
	case FPSGO_QUEUE_CONNECT:
		/* FALLTHROUGH */
	case FPSGO_ACT_SWITCH:
		/* FALLTHROUGH */
	case FPSGO_GAME:
		/* FALLTHROUGH */
	case FPSGO_INTENDED_VSYNC:
		/* FALLTHROUGH */
	case FPSGO_NO_RENDER:
		/* FALLTHROUGH */
	case FPSGO_DRAW_START:
		/* FALLTHROUGH */
	case FPSGO_SWAP_BUFFER:
		break;
#endif

	default:
		pr_debug(TAG "unknown cmd %u\n", cmd);
		ret = -1;
		goto ret_ioctl;
	}

ret_ioctl:
	return ret;
}

static const struct file_operations Fops = {
	.unlocked_ioctl = device_ioctl,
	.compat_ioctl = device_ioctl,
	.open = device_open,
	.write = device_write,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

/*--------------------INIT------------------------*/
int init_perfctl(struct proc_dir_entry *parent)
{
	struct proc_dir_entry *pe;
	int ret_val = 0;


	pr_debug(TAG"Start to init perf_ioctl driver\n");

	ret_val = register_chrdev(DEV_MAJOR, DEV_NAME, &Fops);
	if (ret_val < 0) {
		pr_debug(TAG"%s failed with %d\n",
				"Registering the character device ",
				ret_val);
		goto out_wq;
	}

	pe = proc_create("perf_ioctl", 0664, parent, &Fops);
	if (!pe) {
		pr_debug(TAG"%s failed with %d\n",
				"Creating file node ",
				ret_val);
		ret_val = -ENOMEM;
		goto out_chrdev;
	}

	pr_debug(TAG"init perf_ioctl driver done\n");

	return 0;

out_chrdev:
	unregister_chrdev(DEV_MAJOR, DEV_NAME);
out_wq:
	return ret_val;
}

