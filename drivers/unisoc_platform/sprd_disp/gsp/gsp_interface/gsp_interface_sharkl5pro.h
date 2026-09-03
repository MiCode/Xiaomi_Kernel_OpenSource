/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _GSP_INTERFACE_SHARKL5PRO_H
#define _GSP_INTERFACE_SHARKL5PRO_H

#include <linux/of.h>
#include <linux/regmap.h>
#include "../gsp_interface.h"


#define GSP_SHARKL5PRO "sharkl5pro"

#define SHARKL5PRO_AP_AHB_DISP_EB_NAME	  ("clk_ap_ahb_disp_eb")

struct gsp_interface_sharkl5pro {
	struct gsp_interface common;

	struct clk *clk_ap_ahb_disp_eb;
	struct regmap *module_en_regmap;
	struct regmap *reset_regmap;
};

int gsp_interface_sharkl5pro_parse_dt(struct gsp_interface *intf,
				  struct device_node *node);

int gsp_interface_sharkl5pro_init(struct gsp_interface *intf);
int gsp_interface_sharkl5pro_deinit(struct gsp_interface *intf);

int gsp_interface_sharkl5pro_prepare(struct gsp_interface *intf);
int gsp_interface_sharkl5pro_unprepare(struct gsp_interface *intf);

int gsp_interface_sharkl5pro_reset(struct gsp_interface *intf);

void gsp_interface_sharkl5pro_dump(struct gsp_interface *intf);

#endif
