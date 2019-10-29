/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include <linux/debugfs.h>
#include <linux/delay.h>

#include "synx_private.h"

/**
 * @brief: Initializes debugfs
 *
 * @param dev : Pointer to synx device structure
 */
struct dentry *synx_init_debugfs_dir(struct synx_device *dev);

/**
 * @brief: Removes debugfs
 *
 * @param dev : Pointer to synx device structure
 */
void synx_remove_debugfs_dir(struct synx_device *dev);

#define NAME_COLUMN 0x0001
#define ID_COLUMN 0x0002
#define BOUND_COLUMN 0x0004
#define STATE_COLUMN  0x0008
#define ERROR_CODES  0x8000
