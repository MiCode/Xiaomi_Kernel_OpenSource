/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_GENC_HWSCHED_H_
#define _ADRENO_GENC_HWSCHED_H_

#include "adreno_genc_hwsched_hfi.h"
#include "adreno_hwsched.h"

/**
 * struct genc_hwsched_device - Container for the genc hwscheduling device
 */
struct genc_hwsched_device {
	/** @genc_dev: Container for the genc device */
	struct genc_device genc_dev;
	/** @hwsched_hfi: Container for hwscheduling specific hfi resources */
	struct genc_hwsched_hfi hwsched_hfi;
	/** @hwsched: Container for the hardware dispatcher */
	struct adreno_hwsched hwsched;
};

/**
 * genc_hwsched_probe - Target specific probe for hwsched
 * @pdev: Pointer to the platform device
 * @chipid: Chipid of the target
 * @gpucore: Pointer to the gpucore
 *
 * The target specific probe function for hwsched enabled gmu targets.
 *
 * Return: 0 on success or negative error on failure
 */
int genc_hwsched_probe(struct platform_device *pdev,
		u32 chipid, const struct adreno_gpu_core *gpucore);

/**
 * genc_hwsched_restart - Restart the gmu and gpu
 * @adreno_dev: Pointer to the adreno device
 */
void genc_hwsched_restart(struct adreno_device *adreno_dev);

/**
 * genc_hwsched_snapshot - take genc hwsched snapshot
 * @adreno_dev: Pointer to the adreno device
 * @snapshot: Pointer to the snapshot instance
 *
 * Snapshot the faulty ib and then snapshot rest of genc gmu things
 */
void genc_hwsched_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);
#endif
