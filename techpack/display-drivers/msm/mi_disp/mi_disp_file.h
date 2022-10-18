/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 * Copyright (c) 2020 XiaoMi, Inc. All rights reserved.
 */

#ifndef _MI_DISP_FILE_H_
#define _MI_DISP_FILE_H_

#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/wait.h>

#include <drm/mi_disp.h>

struct disp_display_client {
	unsigned long evbit[BITS_TO_LONGS(MI_DISP_EVENT_MAX)];
};

struct disp_feature_client {
	struct disp_feature *df;
	struct list_head link;
	wait_queue_head_t event_wait;
	struct disp_display_client disp[MI_DISP_MAX];
	struct list_head event_list;
	struct mutex event_lock;
	struct mutex client_lock;
	u32 event_space;
};

typedef int disp_ioctl_func_t(struct disp_feature_client *, void *data);

struct disp_ioctl_desc {
	unsigned int cmd;
	disp_ioctl_func_t *func;
	const char *name;
};

#define DISP_IOCTL_DEF(ioctl, _func)	\
	[_IOC_NR(ioctl)] = {				\
		.cmd = ioctl,					\
		.func = _func,					\
		.name = #ioctl					\
	}

int mi_disp_open(struct inode *inode, struct file *file);
int mi_disp_release(struct inode *inode, struct file *filp);
__poll_t mi_disp_poll(struct file *filp, struct poll_table_struct *wait);
ssize_t mi_disp_read(struct file *filp, char __user *buffer,
		size_t count, loff_t *offset);
long mi_disp_ioctl(struct file *filp, unsigned int cmd, unsigned long arg);
#ifdef CONFIG_COMPAT
long mi_disp_ioctl_compat(struct file *file, unsigned int cmd,
		unsigned long arg);
#endif
__poll_t mi_disp_poll(struct file *filp, struct poll_table_struct *wait);

#endif /* _MI_DISP_FILE_H_ */
