/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2015, 2017-2020, The Linux Foundation. All rights reserved.
 */

#ifndef _LINUX_MSM_AUDIO_ION_H
#define _LINUX_MSM_AUDIO_ION_H
#include <dsp/q6asm-v2.h>
#include <sound/pcm.h>
#include <linux/msm_ion.h>
#include <linux/dma-mapping.h>

enum {
	MSM_AUDIO_ION_INV_CACHES = 0,
	MSM_AUDIO_ION_CLEAN_CACHES,
};

int msm_audio_ion_alloc(struct dma_buf **dma_buf, size_t bufsz,
			dma_addr_t *paddr, size_t *pa_len, void **vaddr);

int msm_audio_ion_import(struct dma_buf **dma_buf, int fd,
			unsigned long *ionflag, size_t bufsz,
			dma_addr_t *paddr, size_t *pa_len, void **vaddr);
int msm_audio_ion_free(struct dma_buf *dma_buf);
int msm_audio_ion_import_cma(struct dma_buf **dma_buf, int fd,
			     unsigned long *ionflag, size_t bufsz,
			     dma_addr_t *paddr, size_t *pa_len, void **vaddr);
int msm_audio_ion_free_cma(struct dma_buf *dma_buf);
int msm_audio_ion_mmap(struct audio_buffer *abuff, struct vm_area_struct *vma);
int msm_audio_ion_cache_operations(struct audio_buffer *abuff, int cache_op);

u32 msm_audio_populate_upper_32_bits(dma_addr_t pa);
int msm_audio_ion_get_smmu_info(struct device **cb_dev, u64 *smmu_sid);

int msm_audio_ion_dma_map(dma_addr_t *phys_addr, dma_addr_t *iova_base,
			u32 size, enum dma_data_direction dir);
#endif /* _LINUX_MSM_AUDIO_ION_H */
