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



bool audio_opendsp_id_ready(const uint8_t opendsp_id)
{
	return is_scp_ready(opendsp_id);
}


bool audio_opendsp_ready(const uint8_t task)
{
	return audio_opendsp_id_ready(audio_get_opendsp_id(task));
}


uint32_t audio_get_opendsp_id(const uint8_t task)
{
	return AUDIO_OPENDSP_USE_CM4_A;
}


uint32_t audio_get_ipi_id(const uint8_t task)
{
	return IPI_AUDIO;
}


bool audio_get_opendsp_support(const uint8_t opendsp_id)
{
	bool is_opendsp_support = false;

	switch (opendsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
		is_opendsp_support = true;
		break;
	default:
		pr_info("%s(), opendsp_id %d not support!!\n",
			 __func__, opendsp_id);
	}
	return is_opendsp_support;
}


uint8_t get_audio_controller_task(const uint8_t opendsp_id)
{
	uint8_t task = TASK_SCENE_INVALID;

	switch (opendsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
		task = TASK_SCENE_AUDIO_CONTROLLER_CM4;
		break;
	default:
		break;
	}
	return task;
}


int get_reserve_mem_size(const uint8_t opendsp_id,
	uint32_t *mem_id, uint32_t *size)
{
	switch (opendsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
		*mem_id = AUDIO_IPI_MEM_ID;
		*size = (uint32_t)scp_get_reserve_mem_size(*mem_id);
		break;
	default:
		pr_notice("%s(), opendsp_id %d not support!!\n",
			__func__, opendsp_id);
		WARN_ON(1);
	}
	return 0;
}


phys_addr_t get_reserve_mem_virt(const uint8_t opendsp_id,
	const uint32_t mem_id)
{
	if (opendsp_id != AUDIO_OPENDSP_USE_CM4_A) {
		pr_notice("%s(), opendsp_id %d not support!!\n",
			__func__, opendsp_id);
		WARN_ON(1);
		return 0;
	}
	return scp_get_reserve_mem_virt(mem_id);
}


phys_addr_t get_reserve_mem_phys(const uint8_t opendsp_id,
	const uint32_t mem_id)
{
	if (opendsp_id != AUDIO_OPENDSP_USE_CM4_A) {
		pr_notice("%s(), opendsp_id %d not support!!\n",
			__func__, opendsp_id);
		WARN_ON(1);
		return 0;
	}
	return scp_get_reserve_mem_phys(mem_id);
}


uint8_t get_cache_aligned_order(const uint8_t opendsp_id)
{
	if (opendsp_id != AUDIO_OPENDSP_USE_CM4_A) {
		pr_notice("%s(), opendsp_id %d not support!!\n",
			__func__, opendsp_id);
		WARN_ON(1);
		return 0;
	}
	return 5;
}


uint8_t get_cache_aligned_mask(const uint8_t opendsp_id)
{
	uint8_t order = get_cache_aligned_order(opendsp_id);

	return (1 << order) - 1;
}


