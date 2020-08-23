// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/list.h>
#include <linux/slab.h>
#include "mdw_import.h"

#ifdef APUSYS_MDW_MNOC_SUPPORT
#include "mnoc_api.h"
#endif
#ifdef APUSYS_MDW_REVISER_SUPPORT
#include "reviser_export.h"
#endif
#ifdef APUSYS_MDW_POWER_SUPPORT
#include "apusys_power.h"
#endif


bool mdw_pwr_check(void)
{
#ifdef APUSYS_MDW_POWER_SUPPORT
	return apusys_power_check();
#else
	return true;
#endif
}

int mdw_rvs_set_ctx(int type, int idx, uint8_t ctx)
{
#ifdef APUSYS_MDW_REVISER_SUPPORT
	return reviser_set_context(type, idx, ctx);
#else
	return 0;
#endif
}

int mdw_rvs_free_vlm(uint32_t ctx)
{
#ifdef APUSYS_MDW_REVISER_SUPPORT
	return reviser_free_vlm(ctx);
#else
	return 0;
#endif
}

int mdw_rvs_get_vlm(uint32_t req_size, bool force,
		unsigned long *id, uint32_t *tcm_size)
{
#ifdef APUSYS_MDW_REVISER_SUPPORT
	return reviser_get_vlm(req_size, force, id, tcm_size);
#else
	return 0;
#endif
}

int mdw_qos_cmd_start(uint64_t cmd_id, uint64_t sc_id,
		int type, int core, uint32_t boost)
{
#ifdef APUSYS_MDW_MNOC_SUPPORT
	return apu_cmd_qos_start(cmd_id, sc_id, type, core, boost);
#else
	return 0;
#endif
}

int mdw_qos_cmd_end(uint64_t cmd_id, uint64_t sc_id,
		int type, int core)
{
#ifdef APUSYS_MDW_MNOC_SUPPORT
	return apu_cmd_qos_end(cmd_id, sc_id, type, core);
#else
	return 0;
#endif
}
