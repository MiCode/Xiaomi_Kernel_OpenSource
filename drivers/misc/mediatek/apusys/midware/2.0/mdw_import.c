// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_cmn.h"
#include "mdw_ap.h"
#include "mdw_import.h"
#include "mdw_trace.h"
#include "reviser_export.h"
#include "mnoc_api.h"
#include "apusys_power.h"

bool mdw_pwr_check(void)
{
	return apusys_power_check();
}

int mdw_rvs_set_ctx(int type, int idx, uint8_t ctx)
{
	int ret = 0;

	mdw_trace_begin("%s|type(%d) idx(%d) ctx(%u)",
		__func__, type, idx, ctx);
	ret = reviser_set_context(type, idx, ctx);
	mdw_trace_end("%s|type(%d) idx(%d) ctx(%u)",
		__func__, type, idx, ctx);

	return ret;
}

int mdw_rvs_free_vlm(uint32_t ctx)
{
	int ret = 0;

	mdw_trace_begin("%s|ctx(%u)", __func__, ctx);
	ret =  reviser_free_vlm(ctx);
	mdw_trace_end("%s|ctx(%u)", __func__, ctx);

	return ret;
}

int mdw_rvs_get_vlm(uint32_t req_size, bool force,
		uint32_t *id, uint32_t *tcm_size)
{
	int ret = 0;

	mdw_trace_begin("%s|size(%u)", __func__, req_size);
	ret = reviser_get_vlm(req_size, force, (unsigned long *)id, tcm_size);
	mdw_trace_end("%s|ctx = %u, size(%u/%u)",
		__func__, id, req_size, *tcm_size);

	return ret;
}

int mdw_rvs_get_vlm_property(uint64_t *start, uint32_t *size)
{
	return reviser_get_resource_vlm((unsigned int *)start,
		(unsigned int *)size);
}

int mdw_rvs_map_ext(uint64_t addr, uint32_t size,
	uint64_t session, uint32_t *sid)
{
	int ret = 0;

	mdw_trace_begin("%s|size(%u)", __func__, size);
	ret = reviser_alloc_external((uint32_t)addr, size, session, sid);
	mdw_trace_end("%s|size(%u)", __func__, size);

	return ret;
}

int mdw_rvs_unmap_ext(uint64_t session, uint32_t sid)
{
	int ret = 0;

	mdw_trace_begin("%s", __func__);
	ret = reviser_free_external(session, sid);
	mdw_trace_end("%s", __func__);

	return ret;
}

int mdw_rvs_import_ext(uint64_t session, uint32_t sid)
{
	int ret = 0;

	mdw_trace_begin("%s", __func__);
	ret = reviser_import_external(session, sid);
	mdw_trace_end("%s", __func__);

	return ret;
}

int mdw_rvs_unimport_ext(uint64_t session, uint32_t sid)
{
	int ret = 0;

	mdw_trace_begin("%s", __func__);
	ret = reviser_unimport_external(session, sid);
	mdw_trace_end("%s", __func__);

	return ret;
}

int mdw_qos_cmd_start(uint64_t cmd_id, uint64_t sc_id,
		int type, int core, uint32_t boost)
{
	return apu_cmd_qos_start(cmd_id, sc_id, type, core, boost);
}

int mdw_qos_cmd_end(uint64_t cmd_id, uint64_t sc_id,
		int type, int core)
{
	return apu_cmd_qos_end(cmd_id, sc_id, type, core);
}
