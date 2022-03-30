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
#include "reviser_drv.h"
#include "reviser_remote_cmd.h"
#include "reviser_import.h"

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
		LOG_DBG_RVR_VLM("request(0x%x) force(%d) ctx(%lu) tcm_size(0x%x)\n",
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

	LOG_DBG_RVR_VLM("ctx(%d)\n", ctx);
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



	LOG_DBG_RVR_VLM("type/index/ctx(%d/%d/%d)\n", type, index, ctx);

	return ret;
}

/**
 * reviser_get_resource_vlm - get vlm address and available TCM size
 * @addr: the address of specific hardware (VLM)
 * @size: the size of specific hardware (TCM)
 *
 * This function get vlm address and size from dts.
 */

int reviser_get_resource_vlm(uint32_t *addr, uint32_t *size)
{
	int ret = 0;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	*addr = (uint32_t) g_rdv->plat.vlm_addr;
	*size = (uint32_t) g_rdv->plat.vlm_size;

	LOG_DBG_RVR_VLM("VLM addr(0x%x) size(0x%x)core\n", *addr, *size);

	return 0;
}


int reviser_get_pool_size(uint32_t type, uint32_t *size)
{
	int ret = 0;
	uint32_t ret_size = 0;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	ret = reviser_remote_get_mem_info(g_rdv, type);
	if (ret) {
		LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	switch (type) {
	case REVISER_MEM_TYPE_TCM:
		ret_size = g_rdv->plat.pool_size[REVSIER_POOL_TCM];
		break;
	case REVISER_MEM_TYPE_SLBS:
		ret_size = g_rdv->plat.pool_size[REVSIER_POOL_SLBS];
		break;
	default:
		LOG_ERR("Invalid type\n", type);
		ret = -EINVAL;
		goto out;
	}

	*size = ret_size;

	LOG_DBG_RVR_VLM("Get Pool Info (%u/0x%x)\n", type, ret_size);
out:

	return ret;
}

int reviser_alloc_mem(uint32_t type, uint32_t size, uint64_t *addr, uint32_t *sid)
{
	int ret = 0, check = 0;
	uint64_t input_addr = 0, ret_addr = 0, input_size = 0;
	uint32_t ret_id = 0;


	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}
	LOG_DBG_RVR_VLM("[Alloc] Mem (%lu/0x%lx)\n", type, size);

	switch (type) {
	case REVISER_MEM_TYPE_EXT:
	case REVISER_MEM_TYPE_RSV_S:
		ret = reviser_alloc_slb(type, size, &input_addr, &input_size);
		if (ret)
			goto out;
		break;
	case REVISER_MEM_TYPE_RSV_T:
		input_addr = 0;
		input_size = size;
		break;

	case REVISER_MEM_TYPE_VLM:
		input_addr = 0;
		input_size = size;
		break;
	default:
		LOG_ERR("Invalid type %u\n", type);
		ret = -EINVAL;
		goto out;
	}
	LOG_DBG_RVR_VLM("[Alloc] Mem (%lu/0x%lx/0x%llx/0x%lx)\n",
				type, size, input_addr, input_size);

	ret = reviser_remote_alloc_mem(g_rdv, type, input_addr, input_size, &ret_addr, &ret_id);
	if (ret) {
		LOG_ERR("Remote Handshake fail %d\n", ret);

		// Free SLB if fail
		if ((type == REVISER_MEM_TYPE_EXT) || (type == REVISER_MEM_TYPE_RSV_S)) {
			check = reviser_free_slb(type, input_addr);
			if (check)
				goto out;
		}

		goto out;
	}

	*addr = ret_addr;
	*sid = ret_id;

	LOG_DBG_RVR_VLM("[Alloc][Done] Mem (%lu/0x%lx/0x%llx/0x%lx)\n",
			type, size, ret_addr, ret_id);

	return ret;
out:
	LOG_ERR("[Alloc][Fail] Mem (%lu/0x%lx/0x%llx/0x%lx)\n", type, size, ret_addr, ret_id);
	return ret;
}

int reviser_free_mem(uint32_t sid)
{
	int ret = 0;
	uint32_t out_type = 0, out_size = 0;
	uint64_t out_addr = 0;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}
	LOG_DBG_RVR_VLM("[Free] Mem (0x%x)\n", sid);

	ret = reviser_remote_free_mem(g_rdv, sid, &out_type, &out_addr, &out_size);
	if (ret) {
		LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	switch (out_type) {
	case REVISER_MEM_TYPE_VLM:
	case REVISER_MEM_TYPE_RSV_T:
		break;
	case REVISER_MEM_TYPE_EXT:
	case REVISER_MEM_TYPE_RSV_S:
		ret = reviser_free_slb(out_type, out_addr);
		if (ret)
			goto out;
		break;
	default:
		LOG_ERR("Invalid type %u\n", out_type);
		ret = -EINVAL;
		goto out;
	}

	LOG_DBG_RVR_VLM("[Free][Done] Mem (0x%x) (%lu/0x%x/0x%x)\n",
				sid, out_type, out_addr, out_size);
	return ret;
out:
	LOG_ERR("[Free][Fail] Mem (0x%x) (%lu/0x%x/0x%x)\n",
			sid, out_type, out_addr, out_size);

	return ret;
}

int reviser_import_mem(uint64_t session, uint32_t sid)
{
	int ret = 0;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}
	LOG_DBG_RVR_VLM("[Import] Mem (0x%llx/0x%x)\n", session, sid);

	ret = reviser_remote_import_mem(g_rdv, session, sid);
	if (ret) {
		LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	LOG_DBG_RVR_VLM("[Import][Done] Mem (0x%llx/0x%x)\n", session, sid);

	return ret;
out:
	LOG_ERR("[Import][Fail] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
}

int reviser_unimport_mem(uint64_t session, uint32_t sid)
{
	int ret = 0;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	LOG_DBG_RVR_VLM("[UnImport] Mem (0x%llx/0x%x)\n", session, sid);

	ret = reviser_remote_unimport_mem(g_rdv, session, sid);
	if (ret) {
		LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	LOG_DBG_RVR_VLM("[UnImport][Done] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
out:
	LOG_ERR("[UnImport][Fail] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
}



int reviser_map_mem(uint64_t session, uint32_t sid, uint64_t *addr)
{
	int ret = 0;
	uint64_t ret_addr = 0;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	LOG_DBG_RVR_VLM("[Map] mem (0x%llx/0x%x/0x%x)\n", session, sid, ret_addr);

	ret = reviser_remote_map_mem(g_rdv, session, sid, &ret_addr);
	if (ret) {
		LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}
	*addr = ret_addr;

	LOG_DBG_RVR_VLM("[Map][Done] Mem (0x%llx/0x%x/0x%llx)\n", session, sid, ret_addr);
	return ret;
out:

	LOG_ERR("[Map][Fail] Mem (0x%llx/0x%x/0x%llx)\n", session, sid, ret_addr);
	return ret;
}

int reviser_unmap_mem(uint64_t session, uint32_t sid)
{
	int ret = 0;

	if (g_rdv == NULL) {
		LOG_ERR("Invalid reviser_device\n");
		ret = -EINVAL;
		return ret;
	}

	LOG_DBG_RVR_VLM("[Unmap] mem (0x%llx/0x%x)\n", session, sid);

	ret = reviser_remote_unmap_mem(g_rdv, session, sid);
	if (ret) {
		LOG_ERR("Remote Handshake fail %d\n", ret);
		goto out;
	}

	LOG_DBG_RVR_VLM("[Unmap][Done] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
out:
	LOG_ERR("[Unmap][Fail] Mem (0x%llx/0x%x)\n", session, sid);
	return ret;
}
