/*
 * Copyright (C) 2014, Intel Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Create on 21 Jan 2015
 * Author: Akashdeep Sharma <akashdeep.sharma@intel.com>
 */

#ifndef GEN_HDMI_AUDIO_H_
#define GEN_HDMI_AUDIO_H_

#include <linux/types.h>

/* HDMI AUDIO INTERRUPT TYPE */
#define HDMI_AUDIO_UNDERRUN     (1UL<<0)
#define HDMI_AUDIO_BUFFER_DONE  (1UL<<1)

enum had_caps_list {
	HAD_GET_ELD = 1,
	HAD_GET_SAMPLING_FREQ,
	HAD_GET_DISPLAY_RATE,
	HAD_GET_HDCP_STATUS,
	HAD_GET_AUDIO_STATUS,
	HAD_SET_ENABLE_AUDIO,
	HAD_SET_DISABLE_AUDIO,
	HAD_SET_ENABLE_AUDIO_INT,
	HAD_SET_DISABLE_AUDIO_INT,
	OTHERS_TBD,
};

enum had_event_type {
	HAD_EVENT_HOT_PLUG = 1,
	HAD_EVENT_HOT_UNPLUG,
	HAD_EVENT_MODE_CHANGING,
	HAD_EVENT_PM_CHANGING,
	HAD_EVENT_AUDIO_BUFFER_DONE,
	HAD_EVENT_AUDIO_BUFFER_UNDERRUN,
	HAD_EVENT_QUERY_IS_AUDIO_BUSY,
	HAD_EVENT_QUERY_IS_AUDIO_SUSPENDED,
};

/**
 * HDMI Display Controller Audio Interface
 */
typedef int (*had_event_call_back)(enum had_event_type event_type,
			void *ctxt_info);

struct  hdmi_audio_registers_ops {
	int (*hdmi_audio_get_register_base)(uint32_t *reg_base);
	int (*hdmi_audio_read_register)(uint32_t reg_addr, uint32_t *data);
	int (*hdmi_audio_write_register)(uint32_t reg_addr, uint32_t data);
	int (*hdmi_audio_read_modify)(uint32_t reg_addr,
			uint32_t data, uint32_t mask);
};

struct hdmi_audio_query_set_ops {
	int (*hdmi_audio_get_caps)(enum had_caps_list query_element,
					void *capabilties);
	int (*hdmi_audio_set_caps)(enum had_caps_list set_element,
					void *capabilties);
};

struct hdmi_audio_event {
	int type;
};

struct snd_intel_had_interface {
	const char *name;
	int (*query)(void *had_data, struct hdmi_audio_event event);
	int (*suspend)(void *had_data, struct hdmi_audio_event event);
	int (*resume)(void *had_data);
};

struct hdmi_audio_priv {
	had_event_call_back callbacks;
	struct snd_intel_had_interface *had_interface;
	void *had_pvt_data;
};

extern void adf_hdmi_audio_signal_event(enum had_event_type event);
#endif /* GEN_HDMI_AUDIO_H_ */
