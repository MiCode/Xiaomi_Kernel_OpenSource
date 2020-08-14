/* SPDX-License-Identifier: GPL-2.0 */
/*
 * mddp_dev.h - Structure/API of MDDP device node control.
 *
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MDDP_DEV_H
#define __MDDP_DEV_H

#include <linux/types.h>

#include "mddp_ctrl.h"

//------------------------------------------------------------------------------
// Struct definition.
// -----------------------------------------------------------------------------
#ifdef CONFIG_MTK_ENG_BUILD
#define MDDP_EM_SUPPORT	1                   /**< Engineer mode support */
#endif

//------------------------------------------------------------------------------
// Private prototype.
// -----------------------------------------------------------------------------
int32_t mddp_dev_open(struct inode *inode,
	struct file *file);
int32_t mddp_dev_close(struct inode *inode,
	struct file *file);
ssize_t mddp_dev_read(struct file *file,
	char *buf, size_t count, loff_t *ppos);
ssize_t mddp_dev_write(struct file *file,
	const char __user *buf, size_t count, loff_t *ppos);
long mddp_dev_ioctl(struct file *file,
	unsigned int cmd, unsigned long arg);
long mddp_dev_compat_ioctl(struct file *filp,
	unsigned int cmd, unsigned long arg);
unsigned int mddp_dev_poll(struct file *fp,
	struct poll_table_struct *poll);

//------------------------------------------------------------------------------
// Public functions.
// -----------------------------------------------------------------------------
int32_t mddp_dev_init(void);
void mddp_dev_uninit(void);
void mddp_dev_response(enum mddp_app_type_e type,
		enum mddp_ctrl_msg_e msg, bool is_success,
		uint8_t *data, uint32_t data_len);

#endif /* __MDDP_DEV_H */
