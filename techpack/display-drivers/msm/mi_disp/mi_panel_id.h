/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (C) 2021 XiaoMi, Inc.
 */

#ifndef _MI_PANEL_ID_H_
#define _MI_PANEL_ID_H_

#include <linux/types.h>
#include "dsi_panel.h"

#define L2_38_0C_0A_PANEL_ID   0x00004C3200380C00
#define L3_42_02_0A_PANEL_ID   0x00004C3300420200

/* PA: Primary display, First selection screen
 * PB: Primary display, Second selection screen
 * SA: Secondary display, First selection screen
 * SB: Secondary display, Second selection screen
 */
enum mi_project_panel_id {
	PANEL_ID_INVALID = 0,
	L2_PANEL_PA,
	L3_PANEL_PA,
	PANEL_ID_MAX
};

static inline enum mi_project_panel_id mi_get_panel_id(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case L2_38_0C_0A_PANEL_ID:
		return L2_PANEL_PA;
	case L3_42_02_0A_PANEL_ID:
		return L3_PANEL_PA;
	default:
		return PANEL_ID_INVALID;
	}
}

static inline const char *mi_get_panel_id_name(u64 mi_panel_id)
{
	switch (mi_get_panel_id(mi_panel_id)) {
	case L2_PANEL_PA:
		return "L2_PANEL_PA";
	case L3_PANEL_PA:
		return "L3_PANEL_PA";
	default:
		return "unknown";
	}
}

static inline bool is_use_nvt_dsc_config(u64 mi_panel_id)
{
	switch(mi_panel_id) {
	case L3_42_02_0A_PANEL_ID:
		return true;
	default:
		return false;
	}
}

enum mi_project_panel_id mi_get_panel_id_by_dsi_panel(struct dsi_panel *panel);
enum mi_project_panel_id mi_get_panel_id_by_disp_id(int disp_id);

#endif /* _MI_PANEL_ID_H_ */
