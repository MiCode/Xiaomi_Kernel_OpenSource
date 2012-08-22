/* Copyright (c) 2010-2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __HDMI_EDID_H__
#define __HDMI_EDID_H__

#include "mdss_hdmi_util.h"

struct hdmi_edid_init_data {
	void __iomem *base;
	struct mutex *mutex;
	struct kobject *sysfs_kobj;

	struct hdmi_tx_ddc_ctrl *ddc_ctrl;
};

int hdmi_edid_read(void *edid_ctrl);
u8 hdmi_edid_get_sink_scaninfo(void *edid_ctrl, u32 resolution);
u32 hdmi_edid_get_sink_mode(void *edid_ctrl);
void hdmi_edid_set_video_resolution(void *edid_ctrl, u32 resolution);
void hdmi_edid_deinit(void *edid_ctrl);
void *hdmi_edid_init(struct hdmi_edid_init_data *init_data);

#endif /* __HDMI_EDID_H__ */
