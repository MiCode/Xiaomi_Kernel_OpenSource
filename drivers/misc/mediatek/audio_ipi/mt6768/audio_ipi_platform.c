/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <audio_ipi_platform.h>

#include <scp_ipi.h>
#include <scp_helper.h>
#include <audio_task.h>


bool is_audio_use_adsp(const uint32_t dsp_id)
{
	return false;
}


bool is_audio_use_scp(const uint32_t dsp_id)
{
#ifdef CONFIG_MTK_AUDIO_CM4_SUPPORT
	return (dsp_id == AUDIO_OPENDSP_USE_CM4_A);
#else
	return false;
#endif
}


bool is_audio_dsp_ready(const uint32_t dsp_id)
{
	return is_audio_dsp_support(dsp_id) && is_scp_ready(SCP_A_ID);
}


bool is_audio_task_dsp_ready(const uint8_t task)
{
	return is_audio_dsp_ready(audio_get_dsp_id(task));
}


uint32_t audio_get_dsp_id(const uint8_t task)
{
	return AUDIO_OPENDSP_USE_CM4_A;
}


uint32_t audio_get_ipi_id(const uint8_t task)
{
	return IPI_AUDIO;
}


bool is_audio_dsp_support(const uint32_t dsp_id)
{
	bool is_opendsp_support = false;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
#ifdef CONFIG_MTK_AUDIO_CM4_SUPPORT
		is_opendsp_support = true;
#else
		is_opendsp_support = false;
#endif
		break;
	default:
		pr_info("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
	}
	return is_opendsp_support;
}


uint8_t get_audio_controller_task(const uint32_t dsp_id)
{
	uint8_t task = TASK_SCENE_INVALID;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
		task = TASK_SCENE_AUDIO_CONTROLLER_CM4;
		break;
	default:
		break;
	}
	return task;
}


int get_reserve_mem_size(const uint32_t dsp_id,
			 uint32_t *mem_id, uint32_t *size)
{
	int ret = 0;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
		*mem_id = AUDIO_IPI_MEM_ID;
		*size = (uint32_t)scp_get_reserve_mem_size(*mem_id);
		break;
	default:
		pr_notice("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
		*mem_id = 0xFFFFFFFF;
		*size = 0;
		ret = -1;
	}
	return ret;
}


void *get_reserve_mem_virt(const uint32_t dsp_id,
			   const uint32_t mem_id)
{
	if (dsp_id != AUDIO_OPENDSP_USE_CM4_A) {
		pr_notice("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
		return NULL;
	}
	return (void *)scp_get_reserve_mem_virt(mem_id);
}


phys_addr_t get_reserve_mem_phys(const uint32_t dsp_id,
				 const uint32_t mem_id)
{
	if (dsp_id != AUDIO_OPENDSP_USE_CM4_A) {
		pr_notice("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
		return 0;
	}
	return scp_get_reserve_mem_phys(mem_id);
}


uint8_t get_cache_aligned_order(const uint32_t dsp_id)
{
	if (dsp_id != AUDIO_OPENDSP_USE_CM4_A) {
		pr_notice("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
		return 0;
	}
	return 5;
}


uint8_t get_cache_aligned_mask(const uint32_t dsp_id)
{
	uint8_t order = get_cache_aligned_order(dsp_id);

	return (1 << order) - 1;
}


