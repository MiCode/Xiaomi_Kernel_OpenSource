/* Copyright (c) 2002,2008-2012, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/export.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/uaccess.h>
#include <linux/io.h>

#include "kgsl.h"
#include "adreno.h"

#include "a2xx_reg.h"

unsigned int kgsl_cff_dump_enable;

static int kgsl_cff_dump_enable_set(void *data, u64 val)
{
#ifdef CONFIG_MSM_KGSL_CFF_DUMP
	kgsl_cff_dump_enable = (val != 0);
	return 0;
#else
	return -EINVAL;
#endif
}

static int kgsl_cff_dump_enable_get(void *data, u64 *val)
{
	*val = kgsl_cff_dump_enable;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(kgsl_cff_dump_enable_fops, kgsl_cff_dump_enable_get,
			kgsl_cff_dump_enable_set, "%llu\n");

typedef void (*reg_read_init_t)(struct kgsl_device *device);
typedef void (*reg_read_fill_t)(struct kgsl_device *device, int i,
	unsigned int *vals, int linec);

void adreno_debugfs_init(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!device->d_debugfs || IS_ERR(device->d_debugfs))
		return;

	debugfs_create_file("cff_dump", 0644, device->d_debugfs, device,
			    &kgsl_cff_dump_enable_fops);
	debugfs_create_u32("wait_timeout", 0644, device->d_debugfs,
		&adreno_dev->wait_timeout);
	debugfs_create_u32("ib_check", 0644, device->d_debugfs,
			   &adreno_dev->ib_check_level);
	/* By Default enable fast hang detection */
	adreno_dev->fast_hang_detect = 1;
	debugfs_create_u32("fast_hang_detect", 0644, device->d_debugfs,
			   &adreno_dev->fast_hang_detect);

}
