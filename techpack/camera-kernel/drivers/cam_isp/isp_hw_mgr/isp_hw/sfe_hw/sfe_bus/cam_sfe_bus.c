// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 */

#include "cam_sfe_bus.h"
#include "cam_sfe_bus_rd.h"
#include "cam_sfe_bus_wr.h"
#include "cam_debug_util.h"

int cam_sfe_bus_init(
	uint32_t                       bus_version,
	int                            bus_type,
	struct cam_hw_soc_info        *soc_info,
	struct cam_hw_intf            *hw_intf,
	void                          *bus_hw_info,
	void                          *sfe_irq_controller,
	struct cam_sfe_bus           **sfe_bus)
{
	int rc = -ENODEV;

	switch (bus_type) {
	case BUS_TYPE_SFE_WR:
		switch (bus_version) {
		case CAM_SFE_BUS_WR_VER_1_0:
			rc = cam_sfe_bus_wr_init(soc_info, hw_intf,
				bus_hw_info, sfe_irq_controller, sfe_bus);
			break;
		default:
			CAM_ERR(CAM_SFE, "Unsupported Bus WR Version 0x%x",
				bus_version);
			break;
		}
		break;
	case BUS_TYPE_SFE_RD:
		switch (bus_version) {
		case CAM_SFE_BUS_RD_VER_1_0:
			rc = cam_sfe_bus_rd_init(soc_info, hw_intf,
				bus_hw_info, sfe_irq_controller, sfe_bus);
			break;
		default:
			CAM_ERR(CAM_SFE, "Unsupported Bus RD Version 0x%x",
				bus_version);
			break;
		}
		break;
	default:
		CAM_ERR(CAM_SFE, "Unsupported Bus type %d", bus_type);
		break;
	}

	return rc;
}

int cam_sfe_bus_deinit(
	uint32_t              bus_version,
	int                   bus_type,
	struct cam_sfe_bus  **sfe_bus)
{
	int rc = -ENODEV;

	switch (bus_type) {
	case BUS_TYPE_SFE_WR:
		switch (bus_version) {
		case CAM_SFE_BUS_WR_VER_1_0:
			rc = cam_sfe_bus_wr_deinit(sfe_bus);
			break;
		default:
			CAM_ERR(CAM_SFE, "Unsupported Bus WR Version 0x%x",
				bus_version);
			break;
		}
		break;
	case BUS_TYPE_SFE_RD:
		switch (bus_version) {
		case CAM_SFE_BUS_RD_VER_1_0:
			rc = cam_sfe_bus_rd_deinit(sfe_bus);
			break;
		default:
			CAM_ERR(CAM_SFE, "Unsupported Bus RD Version 0x%x",
				bus_version);
			break;
		}
		break;
	default:
		CAM_ERR(CAM_SFE, "Unsupported Bus type %d", bus_type);
		break;
	}

	return rc;
}

static inline int __cam_sfe_bus_validate_alloc_type(
	uint32_t alloc_type) {

	if ((alloc_type >= CACHE_ALLOC_NONE) &&
		(alloc_type <= CACHE_ALLOC_TBH_ALLOC))
		return 1;
	else
		return 0;
}

void cam_sfe_bus_parse_cache_cfg(
	bool is_read,
	uint32_t debug_val,
	struct cam_sfe_bus_cache_dbg_cfg *dbg_cfg)
{
	uint32_t scratch_alloc_shift = 0, buf_alloc_shift = 0;
	uint32_t scratch_cfg, buf_cfg, alloc_type;

	if (debug_val >= DISABLE_CACHING_FOR_ALL) {
		dbg_cfg->disable_all = true;
		goto end;
	}

	if (is_read) {
		scratch_alloc_shift = CACHE_SCRATCH_RD_ALLOC_SHIFT;
		buf_alloc_shift = CACHE_BUF_RD_ALLOC_SHIFT;
	} else {
		scratch_alloc_shift = CACHE_SCRATCH_WR_ALLOC_SHIFT;
		buf_alloc_shift = CACHE_BUF_WR_ALLOC_SHIFT;
	}

	scratch_cfg = (debug_val >> CACHE_SCRATCH_DEBUG_SHIFT) & 0xF;
	buf_cfg = (debug_val >> CACHE_BUF_DEBUG_SHIFT) & 0xF;

	/* Check for scratch cfg */
	if (scratch_cfg == 0xF) {
		dbg_cfg->disable_for_scratch = true;
	} else if (scratch_cfg == 1) {
		alloc_type =
			(debug_val >> scratch_alloc_shift) & 0xF;
		if (__cam_sfe_bus_validate_alloc_type(alloc_type)) {
			dbg_cfg->scratch_alloc = alloc_type;
			dbg_cfg->scratch_dbg_cfg = true;
		}
		dbg_cfg->disable_for_scratch = false;
	} else {
		/* Reset to default */
		dbg_cfg->disable_for_scratch = false;
		dbg_cfg->scratch_dbg_cfg = false;
	}

	/* Check for buf cfg */
	if (buf_cfg == 0xF) {
		dbg_cfg->disable_for_buf = true;
	} else if (buf_cfg == 1) {
		alloc_type =
			(debug_val >> buf_alloc_shift) & 0xF;
		if (__cam_sfe_bus_validate_alloc_type(alloc_type)) {
			dbg_cfg->buf_alloc = alloc_type;
			dbg_cfg->buf_dbg_cfg = true;
		}
		dbg_cfg->disable_for_buf = false;
	} else {
		/* Reset to default */
		dbg_cfg->disable_for_buf = false;
		dbg_cfg->buf_dbg_cfg = false;
	}

	/* Reset to default */
	dbg_cfg->disable_all = false;

end:
	return;
}
