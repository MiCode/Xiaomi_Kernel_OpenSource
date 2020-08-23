// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include "apusys_device.h"
#include "reviser_cmn.h"
#include "reviser_export.h"
#include "reviser_table_mgt.h"
#include "reviser_hw_mgt.h"



/**
 * reviser_get_vlm - get continuous memory which is consists of TCM/DRAM/System-Memory
 * @request_size: the request size of the memory
 * @force: use tcm and block function until get it
 * @ctx: the id of continuous memory
 * @tcm_size: the real TCM size of continuous memory
 *
 * This function creates contiguous memory from TCM/DRAM/System-Memory.
 */
int reviser_get_vlm(uint32_t request_size, bool force,
		unsigned long *ctx, uint32_t *tcm_size)
{
	int ret = 0;

	DEBUG_TAG;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	if (!reviser_table_get_vlm(g_rdv,
			request_size, force,
			ctx, tcm_size)) {
		LOG_DEBUG("request(0x%x) force(%d) ctx(%lu) tcm_size(0x%x)\n",
				request_size, force, *ctx, *tcm_size);
	} else {
		ret = -EINVAL;
	}

	return ret;
}
/**
 * reviser_free_vlm - free continuous memory which is consists of TCM/DRAM/System-Memory
 * @ctx: the id of continuous memory
 *
 * This function free contiguous memory by id.
 */
int reviser_free_vlm(uint32_t ctx)
{
	int ret = 0;

	DEBUG_TAG;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	if (reviser_table_free_vlm(g_rdv, ctx)) {
		LOG_ERR("Free VLM Fail: ctx: %d\n", ctx);
		ret = -EINVAL;
		return ret;
	}

	LOG_DEBUG("ctx(%d)\n", ctx);
	return ret;
}

/**
 * reviser_set_context - set context id for specific hardware
 * @type: the hardware type
 * @index: the index of specific hardware
 * @ctx: the id of continuous memory
 *
 * This function set context id for specific hardware for using continuous memory.
 */
int reviser_set_context(int type,
		int index, uint8_t ctx)
{
	int ret = 0;

	DEBUG_TAG;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	if (reviser_mgt_set_ctx(g_rdv,
			type, index, ctx)) {
		LOG_ERR("Set reviser Ctx Fail\n");
		ret = -EINVAL;
		return ret;
	}



	LOG_DEBUG("type/index/ctx(%d/%d/%d)\n", type, index, ctx);

	return ret;
}


