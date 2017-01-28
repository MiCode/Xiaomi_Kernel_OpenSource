/* include/linux/msm_ext_display.h
 *
 * Copyright (c) 2014-2017, The Linux Foundation. All rights reserved.
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
#ifndef _MSM_EXT_DISPLAY_H_
#define _MSM_EXT_DISPLAY_H_

#include <linux/device.h>
#include <linux/platform_device.h>

#define AUDIO_ACK_SET_ENABLE BIT(5)
#define AUDIO_ACK_ENABLE BIT(4)
#define AUDIO_ACK_CONNECT BIT(0)

/**
 *  Flags to be used with the HPD operation of the external display
 *  interface:
 *  MSM_EXT_DISP_HPD_AUDIO: audio will be routed to external display
 *  MSM_EXT_DISP_HPD_VIDEO: video will be routed to external display
 */
#define MSM_EXT_DISP_HPD_AUDIO BIT(0)
#define MSM_EXT_DISP_HPD_VIDEO BIT(1)

/**
 * struct ext_disp_cable_notify - cable notify handler structure
 * @link: a link for the linked list
 * @status: current status of HDMI/DP cable connection
 * @hpd_notify: callback function to provide cable status
 */
struct ext_disp_cable_notify {
	struct list_head link;
	int status;
	void (*hpd_notify)(struct ext_disp_cable_notify *h);
};

struct msm_ext_disp_audio_edid_blk {
	u8 *audio_data_blk;
	unsigned int audio_data_blk_size; /* in bytes */
	u8 *spk_alloc_data_blk;
	unsigned int spk_alloc_data_blk_size; /* in bytes */
};

struct msm_ext_disp_audio_setup_params {
	u32 sample_rate_hz;
	u32 num_of_channels;
	u32 channel_allocation;
	u32 level_shift;
	bool down_mix;
	u32 sample_present;
};

/**
 * External Display identifier for use to determine which interface
 * the audio driver is interacting with.
 */
enum msm_ext_disp_type {
	EXT_DISPLAY_TYPE_HDMI,
	EXT_DISPLAY_TYPE_DP,
	EXT_DISPLAY_TYPE_MAX
};

/**
 * External Display cable state used by display interface to indicate
 * connect/disconnect of interface.
 */
enum msm_ext_disp_cable_state {
	EXT_DISPLAY_CABLE_DISCONNECT,
	EXT_DISPLAY_CABLE_CONNECT,
	EXT_DISPLAY_CABLE_STATE_MAX
};

/**
 * External Display power state used by display interface to indicate
 * power on/off of the interface.
 */
enum msm_ext_disp_power_state {
	EXT_DISPLAY_POWER_OFF,
	EXT_DISPLAY_POWER_ON,
	EXT_DISPLAY_POWER_MAX
};

/**
 * struct msm_ext_disp_intf_ops - operations exposed to display interface
 * @hpd: updates external display interface state
 * @notify: updates audio framework with interface state
 */
struct msm_ext_disp_intf_ops {
	int (*hpd)(struct platform_device *pdev,
			enum msm_ext_disp_type type,
			enum msm_ext_disp_cable_state state,
			u32 flags);
	int (*notify)(struct platform_device *pdev,
			enum msm_ext_disp_cable_state state);
	int (*ack)(struct platform_device *pdev,
			u32 ack);
};

/**
 * struct msm_ext_disp_audio_codec_ops - operations exposed to audio codec
 * @audio_info_setup: configure audio on interface
 * @get_audio_edid_blk: retrieve audio edid block
 * @cable_status: cable connected/disconnected
 * @get_intf_id: id of connected interface
 */
struct msm_ext_disp_audio_codec_ops {
	int (*audio_info_setup)(struct platform_device *pdev,
		struct msm_ext_disp_audio_setup_params *params);
	int (*get_audio_edid_blk)(struct platform_device *pdev,
		struct msm_ext_disp_audio_edid_blk *blk);
	int (*cable_status)(struct platform_device *pdev, u32 vote);
	int (*get_intf_id)(struct platform_device *pdev);
	void (*teardown_done)(struct platform_device *pdev);
};

/*
 * struct msm_ext_disp_init_data - data needed to register the display interface
 * @disp: external display type
 * @intf_ops: external display interface operations
 * @codec_ops: audio codec operations
 */
struct msm_ext_disp_init_data {
	enum msm_ext_disp_type type;
	struct kobject *kobj;
	struct msm_ext_disp_intf_ops intf_ops;
	struct msm_ext_disp_audio_codec_ops codec_ops;
	struct platform_device *pdev;
};

/*
 * msm_ext_disp_register_audio_codec() - audio codec registration
 * @pdev: platform device pointer
 * @codec_ops: audio codec operations
 */
int msm_ext_disp_register_audio_codec(struct platform_device *pdev,
		struct msm_ext_disp_audio_codec_ops *ops);

/*
 * msm_hdmi_register_audio_codec() - wrapper for hdmi audio codec registration
 * @pdev: platform device pointer
 * @codec_ops: audio codec operations
 */
int msm_hdmi_register_audio_codec(struct platform_device *pdev,
	struct msm_ext_disp_audio_codec_ops *ops);
/*
 * msm_ext_disp_register_intf() - display interface registration
 * @init_data: data needed to register the display interface
 */
int msm_ext_disp_register_intf(struct platform_device *pdev,
		struct msm_ext_disp_init_data *init_data);

#endif /*_MSM_EXT_DISPLAY_H_*/
