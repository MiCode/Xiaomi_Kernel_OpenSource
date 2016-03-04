/* Copyright (c) 2010-2013, 2015-2016, The Linux Foundation. All rights reserved.
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
	struct mdss_panel_info *pinfo;
	struct cec_cbs *cbs;
	struct cec_ops *ops;
};

/**
 * hdmi_cec_isr() - interrupt handler for cec hw module
 * @cec_ctrl: pointer to cec hw module's data
 *
 * Return: irq error code
 *
 * The API can be called by HDMI Tx driver on receiving hw interrupts
 * to let the CEC related interrupts handled by this module.
 */
int hdmi_cec_isr(void *cec_ctrl);

/**
 * hdmi_cec_init() - Initialize the CEC hw module
 * @init_data: data needed to initialize the cec hw module
 *
 * Return: pointer to cec hw modules data that needs to be passed when
 * calling cec hw modules API or error code.
 *
 * The API registers CEC HW modules with the client and provides HW
 * specific operations.
 */
void *hdmi_cec_init(struct hdmi_cec_init_data *init_data);

/**
 * hdmi_cec_deinit() - de-initialize CEC HW module
 * @data: CEC HW module data
 *
 * This API release all resources allocated.
 */
void hdmi_cec_deinit(void *data);

/**
 * hdmi_cec_is_wakeup_en() - checks cec wakeup state
 * @cec_ctrl: pointer to cec hw module's data
 *
 * Return: cec wakeup state
 *
 * This API is used to query whether the cec wakeup functionality is
 * enabled or not.
 */
bool hdmi_cec_is_wakeup_en(void *cec_ctrl);

/**
 * hdmi_cec_device_suspend() - updates cec with device suspend state
 * @cec_ctrl: pointer to cec hw module's data
 * @suspend: device suspend state
 *
 * This API is used to update the CEC HW module of the device's suspend
 * state.
 */
void hdmi_cec_device_suspend(void *cec_ctrl, bool suspend);
#endif /* __MDSS_HDMI_CEC_H__ */
