/* Copyright (c) 2002,2008-2013, The Linux Foundation. All rights reserved.
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
#include "kgsl_cffdump.h"

#include "a2xx_reg.h"

unsigned int kgsl_cff_dump_enable;

DEFINE_SIMPLE_ATTRIBUTE(kgsl_cff_dump_enable_fops, kgsl_cff_dump_enable_get,
			kgsl_cff_dump_enable_set, "%llu\n");

static int _active_count_get(void *data, u64 *val)
{
	struct kgsl_device *device = data;
	unsigned int i = atomic_read(&device->active_cnt);

	*val = (u64) i;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(_active_count_fops, _active_count_get, NULL, "%llu\n");

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
	/*
	 * FT policy can be set to any of the options below.
	 * KGSL_FT_OFF -> BIT(0) Set to turn off FT
	 * KGSL_FT_REPLAY  -> BIT(1) Set to enable replay
	 * KGSL_FT_SKIPIB  -> BIT(2) Set to skip IB
	 * KGSL_FT_SKIPFRAME -> BIT(3) Set to skip frame
	 * KGSL_FT_DISABLE -> BIT(4) Set to disable FT for faulting context
	 * by default set FT policy to KGSL_FT_DEFAULT_POLICY
	 */
	adreno_dev->ft_policy = KGSL_FT_DEFAULT_POLICY;
	debugfs_create_u32("ft_policy", 0644, device->d_debugfs,
			   &adreno_dev->ft_policy);
	/* By default enable long IB detection */
	adreno_dev->long_ib_detect = 1;
	debugfs_create_u32("long_ib_detect", 0644, device->d_debugfs,
			   &adreno_dev->long_ib_detect);

	/*
	 * FT pagefault policy can be set to any of the options below.
	 * KGSL_FT_PAGEFAULT_INT_ENABLE -> BIT(0) set to enable pagefault INT
	 * KGSL_FT_PAGEFAULT_GPUHALT_ENABLE  -> BIT(1) Set to enable GPU HALT on
	 * pagefaults. This stalls the GPU on a pagefault on IOMMU v1 HW.
	 * KGSL_FT_PAGEFAULT_LOG_ONE_PER_PAGE  -> BIT(2) Set to log only one
	 * pagefault per page.
	 * KGSL_FT_PAGEFAULT_LOG_ONE_PER_INT -> BIT(3) Set to log only one
	 * pagefault per INT.
	 */
	 adreno_dev->ft_pf_policy = KGSL_FT_PAGEFAULT_DEFAULT_POLICY;
	 debugfs_create_u32("ft_pagefault_policy", 0644, device->d_debugfs,
			&adreno_dev->ft_pf_policy);

	debugfs_create_file("active_cnt", 0444, device->d_debugfs, device,
			    &_active_count_fops);
}
