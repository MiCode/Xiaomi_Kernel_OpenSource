// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#include "mdw_cmn.h"
#include "mdw_ap.h"
#include "mdw_import.h"
#include "reviser_export.h"
#include "mnoc_api.h"
#include "apusys_power.h"

bool mdw_pwr_check(void)
{
	return apusys_power_check();
}

int mdw_rvs_set_ctx(int type, int idx, uint8_t ctx)
{
	return reviser_set_context(type, idx, ctx);
}

int mdw_rvs_free_vlm(uint32_t ctx)
{
	return reviser_free_vlm(ctx);
}

int mdw_rvs_get_vlm(uint32_t req_size, bool force,
		uint32_t *id, uint32_t *tcm_size)
{
	return reviser_get_vlm(req_size, force, (unsigned long *)id, tcm_size);
}

int mdw_rvs_get_vlm_property(uint64_t *start, uint32_t *size)
{
	return reviser_get_resource_vlm((unsigned int *)start,
		(unsigned int *)size);
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
