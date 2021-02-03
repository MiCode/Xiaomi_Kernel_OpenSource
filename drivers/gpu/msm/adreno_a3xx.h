/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2016, 2019-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __A3XX_H
#define __A3XX_H

#include "a3xx_reg.h"
/**
 * struct adreno_a3xx_core - a3xx specific GPU core definitions
 */
struct adreno_a3xx_core {
	/** @base: Container for the generic &struct adreno_gpu_core */
	struct adreno_gpu_core base;
	/** pm4fw_name: Name of the PM4 microcode file */
	const char *pm4fw_name;
	/** pfpfw_name: Name of the PFP microcode file */
	const char *pfpfw_name;
	/** @vbif: List of registers and values to write for VBIF */
	const struct kgsl_regmap_list *vbif;
	/** @vbif_count: Number of registers in @vbif */
	u32 vbif_count;
};

struct adreno_device;

/**
 * to_a3xx_core - return the a3xx specific GPU core struct
 * @adreno_dev: An Adreno GPU device handle
 *
 * Returns:
 * A pointer to the a3xx specific GPU core struct
 */
static inline const struct adreno_a3xx_core *
to_a3xx_core(struct adreno_device *adreno_dev)
{
	const struct adreno_gpu_core *core = adreno_dev->gpucore;

	return container_of(core, struct adreno_a3xx_core, base);
}

void a3xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);

extern const struct adreno_perfcounters adreno_a3xx_perfcounters;

/**
 * a3xx_ringbuffer_init - Initialize the ringbuffer
 * @adreno_dev: An Adreno GPU handle
 *
 * Initialize the ringbuffer for a3xx.
 * Return: 0 on success or negative on failure
 */
int a3xx_ringbuffer_init(struct adreno_device *adreno_dev);

/**
 * a3xx_ringbuffer_submitcmd - Submit a user command to the ringbuffer
 * @adreno_dev: An Adreno GPU handle
 * @cmdobj: Pointer to a user command object
 * @flags: Internal submit flags
 * @time: Optional pointer to a adreno_submit_time container
 *
 * Return: 0 on success or negative on failure
 */
int a3xx_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj, u32 flags,
		struct adreno_submit_time *time);

#endif /*__A3XX_H */
