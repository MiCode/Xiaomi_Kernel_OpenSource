/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef _MC_MAIN_H_
#define _MC_MAIN_H_

#include <linux/device.h>	/* dev_* macros */
#include <linux/slab.h>		/* gfp_t */
#include <linux/fs.h>		/* struct inode and struct file */
#include <linux/mutex.h>
#include <linux/version.h>

#define MC_VERSION(major, minor) \
		((((major) & 0x0000ffff) << 16) | ((minor) & 0x0000ffff))
#define MC_VERSION_MAJOR(x) ((x) >> 16)
#define MC_VERSION_MINOR(x) ((x) & 0xffff)

#define mc_dev_err(__ret__, fmt, ...) \
	dev_err(g_ctx.mcd, "ERROR %d %s: " fmt "\n", \
		__ret__, __func__, ##__VA_ARGS__)

#define mc_dev_info(fmt, ...) \
	dev_info(g_ctx.mcd, "%s: " fmt "\n", __func__, ##__VA_ARGS__)

#ifdef DEBUG
#define mc_dev_devel(fmt, ...) \
	dev_info(g_ctx.mcd, "%s: " fmt "\n", __func__, ##__VA_ARGS__)
#else /* DEBUG */
#define mc_dev_devel(...)		do {} while (0)
#endif /* !DEBUG */

#define TEEC_TT_LOGIN_KERNEL	0x80000000

#define TEE_START_NOT_TRIGGERED 1

/* MobiCore Driver Kernel Module context data. */
struct mc_device_ctx {
	struct device		*mcd;
	/* debugfs root */
	struct dentry		*debug_dir;

	/* Debug counters */
	atomic_t		c_clients;
	atomic_t		c_cbufs;
	atomic_t		c_cwsms;
	atomic_t		c_sessions;
	atomic_t		c_wsms;
	atomic_t		c_mmus;
	atomic_t		c_maps;
	atomic_t		c_slots;
	atomic_t		c_xen_maps;
	atomic_t		c_xen_fes;
	u32 real_drv;
};

extern struct mc_device_ctx g_ctx;

/* Debug stuff */
struct kasnprintf_buf {
	struct mutex mutex;	/* Protect buf/size/off access */
	gfp_t gfp;
	void *buf;
	int size;
	int off;
};

/* Wait for TEE to start and get status */
int mc_wait_tee_start(void);

extern __printf(2, 3)
int kasnprintf(struct kasnprintf_buf *buf, const char *fmt, ...);
ssize_t debug_generic_read(struct file *file, char __user *user_buf,
			   size_t count, loff_t *ppos,
			   int (*function)(struct kasnprintf_buf *buf));
int debug_generic_open(struct inode *inode, struct file *file);
int debug_generic_release(struct inode *inode, struct file *file);

#endif /* _MC_MAIN_H_ */
