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

#include <linux/printk.h>
#include <linux/bug.h>


#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
#include <scp_ipi.h>
#include <scp_helper.h>
#endif

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_ipi.h>
#include <adsp_helper.h>
#endif

#include <audio_task.h>


bool audio_opendsp_id_ready(const uint8_t opendsp_id)
{
	bool ret = false;

	switch (opendsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
	case AUDIO_OPENDSP_USE_CM4_B:
#ifdef CONFIG_MTK_TINYSYS_SCP_SUPPORT
		if (opendsp_id >= SCP_CORE_TOTAL) {
			pr_notice("opendsp_id %u/%u not support!!\n",
				  opendsp_id, SCP_CORE_TOTAL);
			ret = false;
			break;
		}
		ret = is_scp_ready((enum scp_core_id)opendsp_id);
#else
		pr_notice("%s(), opendsp_id %u not build!!\n",
			  __func__, opendsp_id);
		ret = false;
		WARN_ON(1);
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3:
#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
		ret = (is_adsp_ready(ADSP_A_ID) == 1);
#else
		pr_notice("%s(), opendsp_id %u not build!!\n",
			  __func__, opendsp_id);
		ret = false;
		WARN_ON(1);
#endif
		break;
	default:
		pr_notice("%s(), opendsp_id %u not support!!\n",
			  __func__, opendsp_id);
		WARN_ON(1);
	}

	return ret;
}


bool audio_opendsp_ready(const uint8_t task)
{
	return audio_opendsp_id_ready(audio_get_opendsp_id(task));
}


uint32_t audio_get_opendsp_id(const uint8_t task)
{
	uint32_t opendsp_id = AUDIO_OPENDSP_ID_INVALID;

	switch (task) {
	case TASK_SCENE_VOICE_ULTRASOUND:
	case TASK_SCENE_SPEAKER_PROTECTION:
	case TASK_SCENE_VOW:
	case TASK_SCENE_AUDIO_CONTROLLER_CM4:
		opendsp_id = AUDIO_OPENDSP_USE_CM4_A;
		break;
	case TASK_SCENE_PHONE_CALL:
	case TASK_SCENE_PLAYBACK_MP3:
	case TASK_SCENE_RECORD:
	case TASK_SCENE_VOIP:
	case TASK_SCENE_PRIMARY:
	case TASK_SCENE_DEEPBUFFER:
	case TASK_SCENE_AUDPLAYBACK:
	case TASK_SCENE_CAPTURE_UL1:
	case TASK_SCENE_A2DP:
	case TASK_SCENE_DATAPROVIDER:
	case TASK_SCENE_AUD_DAEMON:
	case TASK_SCENE_AUDIO_CONTROLLER_HIFI3:
	case TASK_SCENE_CALL_FINAL:
		opendsp_id = AUDIO_OPENDSP_USE_HIFI3;
		break;
	default:
		pr_notice("%s(), task %d not support!!\n",
			  __func__, task);
		opendsp_id = AUDIO_OPENDSP_ID_INVALID;
		WARN_ON(1);
	}

	return opendsp_id;
}


uint32_t audio_get_ipi_id(const uint8_t task)
{
	uint32_t opendsp_id = audio_get_opendsp_id(task);
	uint32_t ipi_id = 0xFFFFFFFF;

	switch (opendsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
	case AUDIO_OPENDSP_USE_CM4_B:
#if defined(CONFIG_MTK_TINYSYS_SCP_SUPPORT)
		ipi_id = IPI_AUDIO;
#else
		pr_notice("%s(), opendsp_id %u task %d not build!!\n",
			  __func__, opendsp_id, task);
		ipi_id = 0xFFFFFFFF;
		WARN_ON(1);
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3:
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
		ipi_id = ADSP_IPI_AUDIO;
#else
		pr_notice("%s(), opendsp_id %u task %d not build!!\n",
			  __func__, opendsp_id, task);
		ipi_id = 0xFFFFFFFF;
		WARN_ON(1);
#endif
		break;
	default:
		pr_notice("%s(), opendsp_id %u task %d not support!!\n",
			  __func__, opendsp_id, task);
		WARN_ON(1);
		ipi_id = 0xFFFFFFFF;
	}

	return ipi_id;
}


bool audio_get_opendsp_support(const uint8_t opendsp_id)
{
	bool is_opendsp_support = false;

	switch (opendsp_id) {
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
	case AUDIO_OPENDSP_USE_CM4_A:
		is_opendsp_support = true;
		break;
#endif
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	case AUDIO_OPENDSP_USE_HIFI3:
		is_opendsp_support = true;
		break;
#endif
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
	case AUDIO_OPENDSP_USE_HIFI3:
		task = TASK_SCENE_AUDIO_CONTROLLER_HIFI3;
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
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
	case AUDIO_OPENDSP_USE_CM4_A:
		*mem_id = AUDIO_IPI_MEM_ID;
		*size = (uint32_t)scp_get_reserve_mem_size(*mem_id);
		break;
#endif
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	case AUDIO_OPENDSP_USE_HIFI3:
		*mem_id = ADSP_A_IPI_MEM_ID;
		*size = (uint32_t)adsp_get_reserve_mem_size(*mem_id);
		break;
#endif
	default:
		pr_notice("%s(), opendsp_id %d not support!!\n",
			__func__, opendsp_id);
		WARN_ON(1);
	}
	return 0;
}


void *get_reserve_mem_virt(const uint8_t opendsp_id,
	const uint32_t mem_id)
{
	void *addr_mem_virt = NULL;

	switch (opendsp_id) {
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
	case AUDIO_OPENDSP_USE_CM4_A:
		addr_mem_virt = (void *)scp_get_reserve_mem_virt(mem_id);
		break;
#endif
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	case AUDIO_OPENDSP_USE_HIFI3:
		addr_mem_virt = adsp_get_reserve_mem_virt(mem_id);
		break;
#endif
	default:
		pr_notice("%s(), opendsp_id %d not support!!\n",
			__func__, opendsp_id);
		WARN_ON(1);
	}
	return addr_mem_virt;
}


phys_addr_t get_reserve_mem_phys(const uint8_t opendsp_id,
	const uint32_t mem_id)
{
	phys_addr_t addr_mem_phys = 0;

	switch (opendsp_id) {
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
	case AUDIO_OPENDSP_USE_CM4_A:
		addr_mem_phys = scp_get_reserve_mem_phys(mem_id);
		break;
#endif
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	case AUDIO_OPENDSP_USE_HIFI3:
		addr_mem_phys = adsp_get_reserve_mem_phys(mem_id);
		break;
#endif
	default:
		pr_notice("%s(), opendsp_id %d not support!!\n",
			 __func__, opendsp_id);
		WARN_ON(1);
	}
	return addr_mem_phys;
}


uint8_t get_cache_aligned_order(const uint8_t opendsp_id)
{
	uint8_t order = 0;

	switch (opendsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
		order = 5;
		break;
	case AUDIO_OPENDSP_USE_CM4_B:
		order = 5;
		break;
	case AUDIO_OPENDSP_USE_HIFI3:
		order = 7;
		break;
	default:
		pr_notice("%s(), opendsp_id %d not support!!\n",
			__func__, opendsp_id);
	}
	return order;
}


uint8_t get_cache_aligned_mask(const uint8_t opendsp_id)
{
	uint8_t order = get_cache_aligned_order(opendsp_id);

	return (1 << order) - 1;
}


