/* Copyright (c) 2010-2013, The Linux Foundation. All rights reserved.
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

#ifndef __MDSS_HDMI_CEC_H__
#define __MDSS_HDMI_CEC_H__

#include "mdss_hdmi_util.h"

struct hdmi_cec_init_data {
	struct workqueue_struct *workq;
	struct kobject *sysfs_kobj;
	struct dss_io_data *io;
};

int hdmi_cec_deconfig(void *cec_ctrl);
int hdmi_cec_config(void *cec_ctrl);
int hdmi_cec_isr(void *cec_ctrl);
void hdmi_cec_deinit(void *cec_ctrl);
void *hdmi_cec_init(struct hdmi_cec_init_data *init_data);
#endif /* __MDSS_HDMI_CEC_H__ */
