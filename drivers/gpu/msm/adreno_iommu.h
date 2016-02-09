/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef __ADRENO_IOMMU_H
#define __ADRENO_IOMMU_H

#ifdef CONFIG_MSM_KGSL_IOMMU
int adreno_iommu_set_pt_ctx(struct adreno_ringbuffer *rb,
			struct kgsl_pagetable *new_pt,
			struct adreno_context *drawctxt);

int adreno_iommu_init(struct adreno_device *adreno_dev);

unsigned int adreno_iommu_set_pt_generate_cmds(
				struct adreno_ringbuffer *rb,
				unsigned int *cmds,
				struct kgsl_pagetable *pt);
#else
static inline int adreno_iommu_init(struct adreno_device *adreno_dev)
{
	return 0;
}

static inline int adreno_iommu_set_pt_ctx(struct adreno_ringbuffer *rb,
			struct kgsl_pagetable *new_pt,
			struct adreno_context *drawctxt)
{
	return 0;
}

static inline unsigned int adreno_iommu_set_pt_generate_cmds(
				struct adreno_ringbuffer *rb,
				unsigned int *cmds,
				struct kgsl_pagetable *pt)
{
	return 0;
}

#endif
#endif
