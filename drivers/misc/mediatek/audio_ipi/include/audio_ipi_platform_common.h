/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __AUDIO_IPI_PLATFORM_COMMON_H__
#define __AUDIO_IPI_PLATFORM_COMMON_H__

#include <linux/types.h>

enum dsp_id_t {
	AUDIO_OPENDSP_USE_CM4_A, /* => SCP_A_ID */
	AUDIO_OPENDSP_USE_CM4_B, /* => SCP_B_ID */
	AUDIO_OPENDSP_USE_HIFI3_A, /* => ADSP_A_ID */
	AUDIO_OPENDSP_USE_HIFI3_B, /* => ADSP_B_ID */
	AUDIO_OPENDSP_USE_RV_A, /* => SCP_A_ID */
	NUM_OPENDSP_TYPE,
	AUDIO_OPENDSP_ID_INVALID
};


/* adsp */
uint32_t adsp_cid_to_ipi_dsp_id(const uint32_t core_id);
uint32_t ipi_dsp_id_to_adsp_cid(const uint32_t dsp_id);
bool is_audio_use_adsp(const uint32_t dsp_id);

/* scp */
uint32_t scp_cid_to_ipi_dsp_id(const uint32_t core_id);
uint32_t ipi_dsp_id_to_scp_cid(const uint32_t dsp_id);
bool is_audio_use_scp(const uint32_t dsp_id);
bool is_audio_scp_support(void);

/* common */
bool is_audio_dsp_support(const uint32_t dsp_id);
bool is_audio_dsp_ready(const uint32_t dsp_id);
bool is_audio_task_dsp_ready(const uint8_t task);

uint32_t audio_get_audio_ipi_id_by_dsp(const uint32_t dsp_id);
uint32_t audio_get_audio_ipi_id(const uint8_t task);

uint8_t get_audio_controller_task(const uint32_t dsp_id);

int get_reserve_mem_size(const uint32_t dsp_id,
			 uint32_t *mem_id, uint32_t *size);
void *get_reserve_mem_virt(const uint32_t dsp_id, const uint32_t mem_id);
phys_addr_t get_reserve_mem_phys(const uint32_t dsp_id, const uint32_t mem_id);

uint8_t get_cache_aligned_order(const uint32_t dsp_id);
uint8_t get_cache_aligned_mask(const uint32_t dsp_id);

#endif /*__AUDIO_IPI_PLATFORM_COMMON_H__ */
