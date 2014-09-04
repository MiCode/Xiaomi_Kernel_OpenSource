/**************************************************************************
 * Copyright (c) 2007, Intel Corporation.
 * All Rights Reserved.
 * Copyright (c) 2008, Tungsten Graphics, Inc. Cedar Park, TX., USA.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 **************************************************************************/

#include "core/common/dsi/dsi_panel.h"
#include "dsi_vbt.h"

#ifndef CONFIG_ADF_INTEL_VLV
extern int PanelID;

/*declare get panel callbacks*/
extern const struct dsi_panel *cmi_get_panel(void);
extern struct dsi_panel *jdi_cmd_get_panel(void);
extern struct dsi_panel *jdi_vid_get_panel(void);
extern struct dsi_panel *sharp_10x19_cmd_get_panel(void);
extern struct dsi_panel *sharp_10x19_dual_cmd_get_panel(void);
extern struct dsi_panel *sharp_25x16_vid_get_panel(void);
extern struct dsi_panel *sharp_25x16_cmd_get_panel(void);
#endif

struct supported_panel_item {
	u8 id;
	const struct dsi_panel* (*get_panel)(void);
};

static const struct supported_panel_item supported_panels[] = {
	{MIPI_DSI_GENERIC_PANEL_ID, get_generic_panel},
#ifdef SUPPORT_ALL_PANELS
	{CMI_7x12_CMD, cmi_get_panel},
	{JDI_7x12_CMD, jdi_cmd_get_panel},
	{JDI_7x12_VID, jdi_vid_get_panel},
	{SHARP_10x19_CMD, sharp_10x19_cmd_get_panel},
	{SHARP_10x19_DUAL_CMD, sharp_10x19_dual_cmd_get_panel},
	{SHARP_25x16_VID, sharp_25x16_vid_get_panel},
	{SHARP_25x16_CMD, sharp_25x16_cmd_get_panel},
#endif
};

struct dsi_panel *get_dsi_panel_by_id(u8 id)
{
	int i;

	pr_debug("ADF: %s\n", __func__);
#ifndef CONFIG_ADF_INTEL_VLV
	if (id == GCT_DETECT) {
		pr_err("%s: invalid panel id\n", __func__);
		return NULL;
	}
#endif
	for (i = 0; i < ARRAY_SIZE(supported_panels); i++) {
		if (id == supported_panels[i].id)
			return (struct dsi_panel *)
					supported_panels[i].get_panel();
	}

	return NULL;
}

#ifndef CONFIG_ADF_INTEL_VLV
const struct dsi_panel *get_dsi_panel(void)
{
	return get_dsi_panel_by_id((u8)PanelID);
}
#endif
