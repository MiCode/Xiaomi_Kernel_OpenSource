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
	const struct adreno_reglist *vbif;
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

unsigned int a3xx_irq_pending(struct adreno_device *adreno_dev);

void a3xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);

extern const struct adreno_perfcounters adreno_a3xx_perfcounters;

#endif /*__A3XX_H */
