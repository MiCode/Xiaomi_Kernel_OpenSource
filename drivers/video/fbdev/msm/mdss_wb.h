/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2013,2016,2018,2020, The Linux Foundation. All rights reserved. */

#ifndef MDSS_WB_H
#define MDSS_WB_H

#include <linux/extcon.h>
#include <../../../extcon/extcon.h>

struct mdss_wb_ctrl {
	struct platform_device *pdev;
	struct mdss_panel_data pdata;
	struct extcon_dev sdev;
};

#endif
