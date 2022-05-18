// SPDX-License-Identifier: GPL-2.0
//
// Copyright (c) 2020 MediaTek Inc.

#include <audio_ipi_platform_common.h>
#include <audio_ipi_platform.h>

#include <linux/printk.h>
#include <linux/bug.h>


#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
#include <scp_audio_ipi.h>
#include <scp_helper.h>
#endif

#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
#include <adsp_helper.h>
#endif

#include <audio_task.h>


static bool is_audio_scp_ipi_support;

/*
 * =============================================================================
 *                     log
 * =============================================================================
 */

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "[IPI][COM] %s(), " fmt "\n", __func__


/*
 * =============================================================================
 *                     adsp
 * =============================================================================
 */

uint32_t adsp_cid_to_ipi_dsp_id(const uint32_t core_id) /* enum adsp_core_id */
{
	uint32_t dsp_id = AUDIO_OPENDSP_ID_INVALID;

#if !IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
	pr_notice("adsp not enabled!! core_id %u", core_id);
	return dsp_id;
#else
	if (core_id >= get_adsp_core_total()) {
		pr_notice("invalid cid %u, total %u", core_id, get_adsp_core_total());
		return AUDIO_OPENDSP_ID_INVALID;
	}

	dsp_id = core_id + AUDIO_OPENDSP_USE_HIFI3_A;
	if (dsp_id > AUDIO_OPENDSP_USE_HIFI3_B) {
		pr_notice("invalid core_id %u", core_id);
		return AUDIO_OPENDSP_ID_INVALID;
	}

	return dsp_id;
#endif
}

uint32_t ipi_dsp_id_to_adsp_cid(const uint32_t dsp_id)
{
	uint32_t core_id = 0xFFFFFFFF;

#if !IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
	pr_notice("adsp not enabled!! dsp_id %u", dsp_id);
	return core_id;
#else
	if (dsp_id < AUDIO_OPENDSP_USE_HIFI3_A ||
	    dsp_id > AUDIO_OPENDSP_USE_HIFI3_B) {
		return 0xFFFFFFFF;
	}

	core_id = dsp_id - AUDIO_OPENDSP_USE_HIFI3_A;
	if (core_id >= get_adsp_core_total()) {
		pr_notice("invalid cid %u, total %u", core_id, get_adsp_core_total());
		return 0xFFFFFFFF;
	}

	return core_id;
#endif
}

bool is_audio_use_adsp(const uint32_t dsp_id)
{
#if !IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
	return false;
#else
	if (dsp_id >= NUM_OPENDSP_TYPE)
		return false;

	if (ipi_dsp_id_to_adsp_cid(dsp_id) >= get_adsp_core_total())
		return false;

	return true;
#endif
}

/*
 * =============================================================================
 *                     scp
 * =============================================================================
 */

uint32_t scp_cid_to_ipi_dsp_id(const uint32_t core_id) /* enum scp_core_id */
{
	uint32_t dsp_id = AUDIO_OPENDSP_ID_INVALID;

#if !IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
	pr_notice("scp not enabled!! core_id %u", core_id);
	return dsp_id;
#else
	if (core_id >= SCP_CORE_TOTAL) {
		pr_notice("invalid cid %u, total %u", core_id, SCP_CORE_TOTAL);
		return AUDIO_OPENDSP_ID_INVALID;
	}

	dsp_id = core_id + AUDIO_OPENDSP_USE_RV_A;
	if (dsp_id > NUM_OPENDSP_TYPE) {
		pr_notice("invalid core_id %u", core_id);
		return AUDIO_OPENDSP_ID_INVALID;
	}

	return dsp_id;
#endif
}

uint32_t ipi_dsp_id_to_scp_cid(const uint32_t dsp_id)
{
	uint32_t core_id = 0xFFFFFFFF;

#if !IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
	pr_notice("scp not enabled!! dsp_id %u", dsp_id);
	return core_id;
#else
	if (dsp_id != AUDIO_OPENDSP_USE_RV_A)
		return 0xFFFFFFFF;

	core_id = dsp_id - AUDIO_OPENDSP_USE_RV_A;
	if (core_id >= SCP_CORE_TOTAL) {
		pr_notice("invalid cid %u, total %u", core_id, SCP_CORE_TOTAL);
		return 0xFFFFFFFF;
	}

	return core_id;
#endif
}


bool is_audio_use_scp(const uint32_t dsp_id)
{
#if !IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
	return false;
#else
	if (dsp_id >= NUM_OPENDSP_TYPE)
		return false;

	if (!is_audio_scp_support())
		return false;

	if (ipi_dsp_id_to_scp_cid(dsp_id) >= SCP_CORE_TOTAL)
		return false;

	return true;
#endif
}

bool is_audio_scp_support(void)
{
	static bool init_flag;

	if (init_flag == false) {
#if !IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		is_audio_scp_ipi_support = false;
#else
		is_audio_scp_ipi_support = is_audio_mbox_init_done();
#endif
		init_flag = true;
		pr_info("%s is_audio_mbox_init_done %d",
			__func__, is_audio_scp_ipi_support);
	}
	return is_audio_scp_ipi_support;
}
EXPORT_SYMBOL_GPL(is_audio_scp_support);
/*
 * =============================================================================
 *                     common
 * =============================================================================
 */

bool is_audio_dsp_support(const uint32_t dsp_id)
{
	bool ret = false;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
	case AUDIO_OPENDSP_USE_CM4_B:
	case AUDIO_OPENDSP_USE_RV_A:
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		ret = is_audio_use_scp(dsp_id);
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
		ret = is_audio_use_adsp(dsp_id);
#endif
		break;
	default:
		pr_info("dsp_id %u not support!!", dsp_id);
	}

	return ret;
}

bool is_audio_dsp_ready(const uint32_t dsp_id)
{
	bool ret = false;

	if (dsp_id >= NUM_OPENDSP_TYPE)
		return false;

	if (is_audio_use_adsp(dsp_id)) {
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
		ret = (is_adsp_ready(ipi_dsp_id_to_adsp_cid(dsp_id)) == 1);
#endif
	} else if (is_audio_use_scp(dsp_id)) {
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		ret = (is_scp_ready(ipi_dsp_id_to_scp_cid(dsp_id)) == 1);
#endif
	} else
		pr_notice("dsp_id %u not support!!", dsp_id);

	return ret;
}

bool is_audio_task_dsp_ready(const uint8_t task)
{
	return is_audio_dsp_ready(audio_get_dsp_id(task));
}
EXPORT_SYMBOL_GPL(is_audio_task_dsp_ready);

uint32_t audio_get_audio_ipi_id_by_dsp(const uint32_t dsp_id)
{
	uint32_t audio_ipi_id = 0xFFFFFFFF;

	if (dsp_id >= NUM_OPENDSP_TYPE)
		return 0xFFFFFFFF;

	if (is_audio_use_adsp(dsp_id)) {
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
		audio_ipi_id = (uint32_t) ADSP_IPI_AUDIO;
#endif
	} else if (is_audio_use_scp(dsp_id)) {
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		audio_ipi_id = (uint32_t) SCP_AUDIO_IPI_AUDIO;
#endif
	} else
		pr_notice("dsp_id %u not support!!", dsp_id);

	return audio_ipi_id;
}

uint32_t audio_get_audio_ipi_id(const uint8_t task)
{
	return audio_get_audio_ipi_id_by_dsp(audio_get_dsp_id(task));
}

uint8_t get_audio_controller_task(const uint32_t dsp_id)
{
	uint8_t task = TASK_SCENE_INVALID;

	switch (dsp_id) {
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
	case AUDIO_OPENDSP_USE_CM4_A:
		task = TASK_SCENE_AUDIO_CONTROLLER_CM4;
		break;
	case AUDIO_OPENDSP_USE_RV_A:
		task = TASK_SCENE_AUDIO_CONTROLLER_RV;
		break;
#endif
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
	case AUDIO_OPENDSP_USE_HIFI3_A:
		task = TASK_SCENE_AUDIO_CONTROLLER_HIFI3_A;
		break;
	case AUDIO_OPENDSP_USE_HIFI3_B:
		task = TASK_SCENE_AUDIO_CONTROLLER_HIFI3_B;
		break;
#endif
	default:
		break;
	}

	return task;
}

int get_reserve_mem_size(const uint32_t dsp_id,
			 uint32_t *mem_id, uint32_t *size)
{
	int ret = -1;

	if (!mem_id || !size)
		return -EINVAL;
	*mem_id = 0xFFFFFFFF;
	*size = 0;

	if (!is_audio_dsp_support(dsp_id))
		return -1;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
	case AUDIO_OPENDSP_USE_RV_A:
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		*mem_id = AUDIO_IPI_MEM_ID;
		*size = (uint32_t)scp_get_reserve_mem_size(*mem_id);
		ret = 0;
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
		*mem_id = ADSP_A_IPI_DMA_MEM_ID;
		*size = (uint32_t)adsp_get_reserve_mem_size(*mem_id);
		ret = 0;
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_B:
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
		*mem_id = ADSP_B_IPI_DMA_MEM_ID;
		*size = (uint32_t)adsp_get_reserve_mem_size(*mem_id);
		ret = 0;
#endif
		break;
	default:
		pr_notice("dsp_id %u not support!!", dsp_id);
	}

	return ret;
}

void *get_reserve_mem_virt(const uint32_t dsp_id, const uint32_t mem_id)
{
	void *addr_mem_virt = NULL;

	if (!is_audio_dsp_support(dsp_id))
		return NULL;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
	case AUDIO_OPENDSP_USE_RV_A:
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		addr_mem_virt = (void *)scp_get_reserve_mem_virt(mem_id);
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
		addr_mem_virt = adsp_get_reserve_mem_virt(mem_id);
#endif
		break;
	default:
		pr_notice("dsp_id %u not support!!", dsp_id);
	}

	return addr_mem_virt;
}

phys_addr_t get_reserve_mem_phys(const uint32_t dsp_id, const uint32_t mem_id)
{
	phys_addr_t addr_mem_phys = 0;

	if (!is_audio_dsp_support(dsp_id))
		return 0;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
	case AUDIO_OPENDSP_USE_RV_A:
#if IS_ENABLED(CONFIG_MTK_SCP_AUDIO)
		addr_mem_phys = scp_get_reserve_mem_phys(mem_id);
#endif
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
#if IS_ENABLED(CONFIG_MTK_AUDIODSP_SUPPORT)
		addr_mem_phys = adsp_get_reserve_mem_phys(mem_id);
#endif
		break;
	default:
		pr_notice("dsp_id %u not support!!", dsp_id);
	}

	return addr_mem_phys;
}

uint8_t get_cache_aligned_order(const uint32_t dsp_id)
{
	uint8_t order = 0;

	switch (dsp_id) {
	case AUDIO_OPENDSP_USE_CM4_A:
	case AUDIO_OPENDSP_USE_CM4_B:
		order = 5;
		break;
	case AUDIO_OPENDSP_USE_HIFI3_A:
	case AUDIO_OPENDSP_USE_HIFI3_B:
	case AUDIO_OPENDSP_USE_RV_A:
		order = 7;
		break;
	default:
		pr_notice("dsp_id %u not support!!", dsp_id);
	}
	return order;
}

uint8_t get_cache_aligned_mask(const uint32_t dsp_id)
{
	uint8_t order = get_cache_aligned_order(dsp_id);

	return (1 << order) - 1;
}

