/* Copyright (c) 2010-2013, 2015, The Linux Foundation. All rights reserved.
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
#include "mdss_cec_core.h"

#define RETRANSMIT_MAX_NUM	5

/**
 * struct hdmi_cec_init_data - data needed for initializing cec hw module
 * @workq: pointer to workqueue
 * @io: pointer to register access related data
 * @pinfo: pointer to panel information data
 * @cbs: pointer to cec abstract callback functions.
 * @ops: pointer to cec hw operation functions.
 *
 * Defines the data needed to be provided while initializing cec hw module
 */
struct hdmi_cec_init_data {
	struct workqueue_struct *workq;
	struct dss_io_data *io;
	struct cec_cbs *cbs;
	struct cec_ops *ops;
};

int hdmi_cec_isr(void *cec_ctrl);
void *hdmi_cec_init(struct hdmi_cec_init_data *init_data);
void hdmi_cec_deinit(void *data);
#endif /* __MDSS_HDMI_CEC_H__ */
