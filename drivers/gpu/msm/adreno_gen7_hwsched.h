/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_GEN7_HWSCHED_H_
#define _ADRENO_GEN7_HWSCHED_H_

#include "adreno_gen7_hwsched_hfi.h"

/**
 * struct gen7_hwsched_device - Container for the gen7 hwscheduling device
 */
struct gen7_hwsched_device {
	/** @gen7_dev: Container for the gen7 device */
	struct gen7_device gen7_dev;
	/** @hwsched_hfi: Container for hwscheduling specific hfi resources */
	struct gen7_hwsched_hfi hwsched_hfi;
};

/**
 * gen7_hwsched_probe - Target specific probe for hwsched
 * @pdev: Pointer to the platform device
 * @chipid: Chipid of the target
 * @gpucore: Pointer to the gpucore
 *
 * The target specific probe function for hwsched enabled gmu targets.
 *
 * Return: 0 on success or negative error on failure
 */
int gen7_hwsched_probe(struct platform_device *pdev,
		u32 chipid, const struct adreno_gpu_core *gpucore);

/**
 * gen7_hwsched_reset - Restart the gmu and gpu
 * @adreno_dev: Pointer to the adreno device
 *
 * Return: 0 on success or negative error on failure
 */
int gen7_hwsched_reset(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_snapshot - take gen7 hwsched snapshot
 * @adreno_dev: Pointer to the adreno device
 * @snapshot: Pointer to the snapshot instance
 *
 * Snapshot the faulty ib and then snapshot rest of gen7 gmu things
 */
void gen7_hwsched_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);

/**
 * gen7_hwsched_handle_watchdog - Handle watchdog interrupt
 * @adreno_dev: Pointer to the adreno device
 */
void gen7_hwsched_handle_watchdog(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_active_count_get - Increment the active count
 * @adreno_dev: Pointer to the adreno device
 *
 * This function increments the active count. If active count
 * is 0, this function also powers up the device.
 *
 * Return: 0 on success or negative error on failure
 */
int gen7_hwsched_active_count_get(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_active_count_put - Put back the active count
 * @adreno_dev: Pointer to the adreno device
 *
 * This function decrements the active count sets the idle
 * timer if active count is zero.
 */
void gen7_hwsched_active_count_put(struct adreno_device *adreno_dev);

/**
 * gen7_hwsched_add_to_minidump - Register hwsched_device with va minidump
 * @adreno_dev: Pointer to the adreno device
 */
int gen7_hwsched_add_to_minidump(struct adreno_device *adreno_dev);

#endif
