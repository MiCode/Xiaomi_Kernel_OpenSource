// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/errno.h>
#include <linux/slab.h>
#include "reviser_drv.h"
#include "apusys_device.h"
#include "reviser_cmn.h"
#include "reviser_export.h"
#include "reviser_mem_mgt.h"
#include "reviser_hw.h"

extern struct reviser_dev_info *g_reviser_device;
int reviser_get_vlm(uint32_t request_size, bool force,
		unsigned long *id, uint32_t *tcm_size)
{
	int ret = 0;

	DEBUG_TAG;

	if (g_reviser_device == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	if (!reviser_table_get_vlm(g_reviser_device,
			request_size, force,
			id, tcm_size)) {
		LOG_DEBUG("request(0x%x) force(%d) ctxid(%lu) tcm_size(0x%x)\n",
				request_size, force, *id, *tcm_size);
	} else {
		ret = -EINVAL;
	}

	return ret;
}
EXPORT_SYMBOL(reviser_get_vlm);

int reviser_free_vlm(uint32_t ctxid)
{
	int ret = 0;

	DEBUG_TAG;

	if (g_reviser_device == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	if (reviser_table_free_vlm(g_reviser_device, ctxid)) {
		LOG_ERR("Free VLM Fail: ctxID: %d\n", ctxid);
		ret = -EINVAL;
		return ret;
	}

	LOG_DEBUG("ctxid(%d)\n", ctxid);
	return ret;
}
EXPORT_SYMBOL(reviser_free_vlm);

int reviser_set_context(int type,
		int index, uint8_t ctxid)
{
	int ret = 0;
	enum REVISER_DEVICE_E reviser_type = REVISER_DEVICE_NONE;

	DEBUG_TAG;

	if (g_reviser_device == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	if (type == APUSYS_DEVICE_SAMPLE) {
		LOG_DEBUG("Ignore Set context\n");
		return ret;
	}

	if (reviser_type_convert(type, &reviser_type)) {
		LOG_ERR("Invalid type\n");
		ret = -EINVAL;
		return ret;
	}
	if (!reviser_is_power(g_reviser_device)) {
		LOG_ERR("Can Not set contxet when power disable\n");
		ret = -EINVAL;
		return ret;
	}

	if (reviser_set_context_ID(g_reviser_device,
			reviser_type, index, ctxid)) {
		LOG_ERR("Set reviser Ctx Fail\n");
		ret = -EINVAL;
		return ret;
	}

	LOG_DEBUG("type/index/ctxid(%d/%d/%d)\n", type, index, ctxid);

	return ret;
}
EXPORT_SYMBOL(reviser_set_context);

int reviser_get_resource_vlm(uint32_t *addr, uint32_t *size)
{
	int ret = 0;

	if (g_reviser_device == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	*addr = (uint32_t) g_reviser_device->vlm.iova;
	*size = (uint32_t) g_reviser_device->tcm.size;

	return 0;
}
EXPORT_SYMBOL(reviser_get_resource_vlm);
