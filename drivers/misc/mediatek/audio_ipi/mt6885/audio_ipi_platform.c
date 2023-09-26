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


#ifdef CONFIG_MTK_AUDIO_CM4_SUPPORT
#include <scp_ipi.h>
#include <scp_helper.h>
#endif

#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
#include <adsp_helper.h>
#endif

#include <audio_task.h>


bool is_audio_use_adsp(const uint32_t dsp_id)
{
#if !defined(CONFIG_MTK_AUDIODSP_SUPPORT)
	return false;
#else
	return (dsp_id == AUDIO_OPENDSP_USE_HIFI3_A ||
		dsp_id == AUDIO_OPENDSP_USE_HIFI3_B);
#endif
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
	bool ret = false;

	if (is_audio_dsp_support(dsp_id) == false)
		return false;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
	case AUDIO_OPENDSP_USE_CM4_B:
#ifdef CONFIG_MTK_AUDIO_CM4_SUPPORT
		if (dsp_id >= SCP_CORE_TOTAL) {
			pr_notice("dsp_id %u/%u not support!!\n",
				  dsp_id, SCP_CORE_TOTAL);
			ret = false;
			break;
		}
		ret = is_scp_ready((enum scp_core_id)dsp_id);
#else
		pr_notice("%s(), dsp_id %u not build!!\n", __func__, dsp_id);
		ret = false;
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
#ifdef CONFIG_MTK_AUDIODSP_SUPPORT
		ret = (is_adsp_ready(dsp_id - AUDIO_OPENDSP_USE_HIFI3_A) == 1);
#else
		pr_notice("%s(), dsp_id %u not build!!\n", __func__, dsp_id);
		ret = false;
#endif
		break;
	default:
		pr_notice("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
	}

	return ret;
}


bool is_audio_task_dsp_ready(const uint8_t task)
{
	return is_audio_dsp_ready(audio_get_dsp_id(task));
}


uint32_t audio_get_dsp_id(const uint8_t task)
{
	uint32_t dsp_id = AUDIO_OPENDSP_ID_INVALID;

	switch (task) {
	case TASK_SCENE_VOICE_ULTRASOUND:
	case TASK_SCENE_SPEAKER_PROTECTION:
	case TASK_SCENE_VOW:
	case TASK_SCENE_AUDIO_CONTROLLER_CM4:
		dsp_id = AUDIO_OPENDSP_USE_CM4_A;
		break;
	case TASK_SCENE_PLAYBACK_MP3:
	case TASK_SCENE_PRIMARY:
	case TASK_SCENE_DEEPBUFFER:
	case TASK_SCENE_AUDPLAYBACK:
	case TASK_SCENE_A2DP:
	case TASK_SCENE_DATAPROVIDER:
	case TASK_SCENE_AUD_DAEMON_A:
	case TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A:
	case TASK_SCENE_CALL_FINAL:
	case TASK_SCENE_MUSIC:
	case TASK_SCENE_FAST:
	case TASK_SCENE_FM_ADSP:
	case TASK_SCENE_PHONE_CALL_SUB:
	case TASK_SCENE_BLECALLDL:
		dsp_id = AUDIO_OPENDSP_USE_HIFI3_A;
		break;
	case TASK_SCENE_PHONE_CALL:
	case TASK_SCENE_RECORD:
	case TASK_SCENE_VOIP:
	case TASK_SCENE_CAPTURE_UL1:
	case TASK_SCENE_AUD_DAEMON_B:
	case TASK_SCENE_AUDIO_CONTROLLER_HIFI3_B:
	case TASK_SCENE_KTV:
	case TASK_SCENE_CAPTURE_RAW:
	case TASK_SCENE_BLECALLUL:
		dsp_id = AUDIO_OPENDSP_USE_HIFI3_B;
		break;
	default:
		pr_notice("%s(), task %d not support!!\n", __func__, task);
		dsp_id = AUDIO_OPENDSP_ID_INVALID;
	}

	return dsp_id;
}


uint32_t audio_get_ipi_id(const uint8_t task)
{
	uint32_t dsp_id = audio_get_dsp_id(task);
	uint32_t ipi_id = 0xFFFFFFFF;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
	case AUDIO_OPENDSP_USE_CM4_B:
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
		ipi_id = IPI_AUDIO;
#else
		pr_notice("%s(), dsp_id %u task %d not build!!\n",
			  __func__, dsp_id, task);
		ipi_id = 0xFFFFFFFF;
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
		ipi_id = ADSP_IPI_AUDIO;
#else
		pr_notice("%s(), dsp_id %u task %d not build!!\n",
			  __func__, dsp_id, task);
		ipi_id = 0xFFFFFFFF;
#endif
		break;
	default:
		pr_notice("%s(), dsp_id %u task %d not support!!\n",
			  __func__, dsp_id, task);
		ipi_id = 0xFFFFFFFF;
	}

	return ipi_id;
}


bool is_audio_dsp_support(const uint32_t dsp_id)
{
	bool is_opendsp_support = false;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
		is_opendsp_support = true;
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
		is_opendsp_support = true;
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
	case AUDIO_OPENDSP_USE_HIFI3_A:
		task = TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A;
		break;
	case AUDIO_OPENDSP_USE_HIFI3_B:
		task = TASK_SCENE_AUDIO_CONTROLLER_HIFI3_B;
		break;
	default:
		break;
	}
	return task;
}


int get_reserve_mem_size(const uint32_t dsp_id,
			 uint32_t *mem_id, uint32_t *size)
{
	int ret = -1;

	*mem_id = 0xFFFFFFFF;
	*size = 0;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
		*mem_id = AUDIO_IPI_MEM_ID;
		*size = (uint32_t)scp_get_reserve_mem_size(*mem_id);
		ret = 0;
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
		*mem_id = ADSP_A_IPI_DMA_MEM_ID;
		*size = (uint32_t)adsp_get_reserve_mem_size(*mem_id);
		ret = 0;
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_B:
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
		*mem_id = ADSP_B_IPI_DMA_MEM_ID;
		*size = (uint32_t)adsp_get_reserve_mem_size(*mem_id);
		ret = 0;
#endif
		break;
	default:
		pr_notice("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
	}

	return ret;
}


void *get_reserve_mem_virt(const uint32_t dsp_id, const uint32_t mem_id)
{
	void *addr_mem_virt = NULL;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
		addr_mem_virt = (void *)scp_get_reserve_mem_virt(mem_id);
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
		addr_mem_virt = adsp_get_reserve_mem_virt(mem_id);
#endif
		break;
	default:
		pr_notice("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
	}
	return addr_mem_virt;
}


phys_addr_t get_reserve_mem_phys(const uint32_t dsp_id, const uint32_t mem_id)
{
	phys_addr_t addr_mem_phys = 0;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
#if defined(CONFIG_MTK_AUDIO_CM4_SUPPORT)
		addr_mem_phys = scp_get_reserve_mem_phys(mem_id);
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
#if defined(CONFIG_MTK_AUDIODSP_SUPPORT)
		addr_mem_phys = adsp_get_reserve_mem_phys(mem_id);
#endif
		break;
	default:
		pr_notice("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
	}
	return addr_mem_phys;
}


uint8_t get_cache_aligned_order(const uint32_t dsp_id)
{
	uint8_t order = 0;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
		order = 5;
		break;
	case AUDIO_OPENDSP_USE_CM4_B:
		order = 5;
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
		order = 7;
		break;
	default:
		pr_notice("%s(), dsp_id %u not support!!\n", __func__, dsp_id);
	}
	return order;
}


uint8_t get_cache_aligned_mask(const uint32_t dsp_id)
{
	uint8_t order = get_cache_aligned_order(dsp_id);

	return (1 << order) - 1;
}


