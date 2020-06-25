/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (c) 2016, 2018,The Linux Foundation. All rights reserved. */

#ifndef __MDSS_HDMI_AUDIO_H__
#define __MDSS_HDMI_AUDIO_H__

#include <linux/mdss_io_util.h>
#include <linux/msm_ext_display.h>

/**
 * struct hdmi_audio_ops - audio operations for clients to call
 * @on: function pointer to enable audio
 * @reset: function pointer to reset the audio current status to default
 * @status: function pointer to get the current status of audio
 * @notify: function pointer to notify other modules for audio routing
 * @ack: function pointer to acknowledge audio routing change
 *
 * Provides client operations for audio functionalities
 */
struct hdmi_audio_ops {
	int (*on)(void *ctx, u32 pclk,
		struct msm_ext_disp_audio_setup_params *params);
	void (*off)(void *ctx);
	void (*reset)(void *ctx);
};

/**
 * struct hdmi_audio_init_data - data needed for initializing audio module
 * @io: pointer to register access related data
 * @ops: pointer to populate operation functions.
 *
 * Defines the data needed to be provided while initializing audio module
 */
struct hdmi_audio_init_data {
	struct dss_io_data *io;
	struct hdmi_audio_ops *ops;
};

void *hdmi_audio_register(struct hdmi_audio_init_data *data);
void hdmi_audio_unregister(void *data);

#endif /* __MDSS_HDMI_AUDIO_H__ */
