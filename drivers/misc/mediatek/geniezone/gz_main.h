/* SPDX-License-Identifier: GPL-2.0 */

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */

#ifndef __GZ_MAIN_H
#define __GZ_MAIN_H

/* GP memory parameter max len. Needs to be synced with GZ */
#define GP_MEM_MAX_LEN 1024

#define KREE_SESSION_HANDLE_MAX_SIZE 512

static int __init gz_init(void);
static void __exit gz_exit(void);

static int gz_dev_open(struct inode *inode, struct file *filp);
static int gz_dev_release(struct inode *inode, struct file *filp);

static long gz_ioctl(struct file *filep, unsigned int cmd, unsigned long arg);
static long gz_compat_ioctl(struct file *filep, unsigned int cmd,
			    unsigned long arg);

#endif /* __GZ_MAIN_H */

