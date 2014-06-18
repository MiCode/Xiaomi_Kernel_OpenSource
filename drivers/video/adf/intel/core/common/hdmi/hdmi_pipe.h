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

#ifndef HDMI_PIPE_H_
#define HDMI_PIPE_H_

#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <video/adf.h>
#include "core/intel_dc_config.h"
#include "core/common/intel_dc_regs.h"
#include "intel_adf_device.h"
#include "pwr_mgmt.h"
#include "adf_hdmi_audio_if.h"
#include "intel_adf.h"

#define MSIC_HPD_GPIO_PIN_NAME "HDMI_HPD"
#define MSIC_LS_EN_GPIO_PIN_NAME "HDMI_LS_EN"
#define HDMI_I2C_ADAPTER_NUM 10

#define HDMI_MAX_DISPLAY_H 1920
#define HDMI_MAX_DISPLAY_V 1080

#define HDMI_MODE_INVALID 1

/* DPLL registers on IOSF */
#define PLLA_DWORD3_1   0x800C
#define PLLA_DWORD3_2   0x802C
#define PLLA_DWORD5_1   0x8014
#define PLLA_DWORD5_2   0x8034
#define PLLA_DWORD7_1   0x801C
#define PLLA_DWORD7_2   0x803C
#define PLLB_DWORD8     0x8040
#define PLLB_DWORD10_1  0x8048
#define PLLB_DWORD10_2  0x8068
#define CMN_DWORD3      0x810C
#define CMN_DWORD8      0x8100
#define REF_DWORD18     0x80C0
#define REF_DWORD22     0x80D0
#define DPLL_CML_CLK1   0x8238
#define DPLL_CML_CLK2   0x825C
#define DPLL_LRC_CLK    0x824C
#define DPLL_Tx_GRC     0x8244
#define PCS_DWORD12_1   0x0230
#define PCS_DWORD12_2   0x0430
#define TX_SWINGS_1     0x8294
#define TX_SWINGS_2     0x8290
#define TX_SWINGS_3     0x8288
#define TX_SWINGS_4     0x828C
#define TX_SWINGS_5     0x0690
#define TX_SWINGS_6     0x822C
#define TX_SWINGS_7     0x8224
#define TX_GROUP_1      0x82AC
#define TX_GROUP_2      0x82B8

#define DPLL_IOSF_EP 0x13

#define MAX_ELD_LENGTH 128

#define HDMI_DIP_PACKET_HEADER_LEN	3
#define HDMI_DIP_PACKET_DATA_LEN	28

struct  avi_info_packet {
	uint8_t header[HDMI_DIP_PACKET_HEADER_LEN];
	union {
		uint8_t data[HDMI_DIP_PACKET_DATA_LEN];
		uint32_t data32[HDMI_DIP_PACKET_DATA_LEN/4];
	};
};

struct hdmi_platform_config {
	u8 platform_id;  /* Medfield, Merrifield, CTP, Moorefield*/
	int gpio_hpd_pin;
	int gpio_ls_en_pin;
	int last_pin_value;
	uint32_t pci_device_id;
	uint32_t irq_number;
	uint32_t min_clock;
	uint32_t max_clock;
};

struct hdmi_hw_context {
	u32 vgacntr;

	/*plane*/
	u32 dspcntr;
	u32 dspsize;
	u32 dspsurf;
	u32 dsppos;
	u32 dspstride;
	u32 dsplinoff;

	/*pipe regs*/
	u32 htotal;
	u32 hblank;
	u32 hsync;
	u32 vtotal;
	u32 vblank;
	u32 vsync;
	u32 pipestat;

	u32 pipesrc;

	u32 div;
	u32 pipeconf;

	/*hdmi port*/
	u32 hdmib;

};

struct hdmi_mode_info {
	struct list_head head;
	struct drm_mode_modeinfo drm_mode;
	int mode_status;
};

struct hdmi_hotplug_context {
	atomic_t is_asserted;
	atomic_t is_connected;
};

struct hdmi_monitor {
	struct edid *raw_edid;

	uint8_t eld[MAX_ELD_LENGTH];

	/* information parsed from edid*/
	bool is_hdmi;

	struct list_head probedModes;

	struct drm_mode_modeinfo *preferred_mode;
	struct avi_info_packet avi_packet;

	int screen_width_mm;
	int screen_height_mm;
	bool quant_range_selectable;
	u8 video_code;
};

struct hdmi_audio {
	had_event_call_back callbacks;
	uint32_t tmds_clock;
	struct snd_intel_had_interface *had_interface;
	void *had_pvt_data;
	struct work_struct hdmi_audio_work;
	struct work_struct hdmi_bufferdone_work;
	struct work_struct hdmi_underrun_work;
};

struct hdmi_pipe {
	struct intel_pipe base;
	struct hdmi_platform_config config;
	struct hdmi_monitor monitor;
	struct hdmi_audio audio;

	struct hdmi_hw_context ctx;

	struct workqueue_struct *hotplug_register_wq;
	struct work_struct hotplug_register_work;
	struct hdmi_hotplug_context hpd_ctx;

	/* this is for HPD device */
	struct pci_driver hdmi_hpd_driver;
	struct pci_device_id id_table[1];
	irqreturn_t (*hotplug_irq_cb)(int, void *);
	void *hotplug_data;

	/*this is for EDID R/W and HDCP control*/
	struct i2c_adapter *adapter;
};

static inline struct hdmi_pipe *to_hdmi_pipe(struct intel_pipe *pipe)
{
	return container_of(pipe, struct hdmi_pipe, base);
}

extern int hdmi_pipe_init(struct hdmi_pipe *pipe, struct device *dev,
	struct intel_plane *primary_plane, u8 idx);
extern void hdmi_pipe_destroy(struct hdmi_pipe *pipe);
int  mofd_get_platform_configs(struct hdmi_platform_config *config);
bool mofd_hdmi_enable_hpd(bool enable);
bool mofd_hdmi_get_cable_status(struct hdmi_platform_config *config);
extern void adf_hdmi_audio_init(struct hdmi_pipe *hdmi_pipe);
extern void adf_hdmi_audio_signal_event(enum had_event_type event);

#endif /* HDMI_PIPE_H_ */
