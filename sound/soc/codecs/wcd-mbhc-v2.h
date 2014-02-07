/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#ifndef __WCD_MBHC_V2_H__
#define __WCD_MBHC_V2_H__

#include <linux/wait.h>

enum wcd_mbhc_plug_type {
	PLUG_TYPE_INVALID = -1,
	PLUG_TYPE_NONE,
	PLUG_TYPE_HEADSET,
	PLUG_TYPE_HEADPHONE,
	PLUG_TYPE_HIGH_HPH,
	PLUG_TYPE_GND_MIC_SWAP,
};

struct wcd_mbhc_config {
	bool read_fw_bin;
	void *calibration;
	bool detect_extn_cable;
	bool mono_stero_detection;
};

struct wcd_mbhc_intr {
	int mbhc_sw_intr;
	int mbhc_btn_press_intr;
	int mbhc_btn_release_intr;
	int mbhc_hs_ins_rem_intr;
	int hph_left_ocp;
	int hph_right_ocp;
};

struct wcd_mbhc {
	int buttons_pressed;
	struct wcd_mbhc_config *mbhc_cfg;

	u32 hph_status; /* track headhpone status */
	u8 hphlocp_cnt; /* headphone left ocp retry */
	u8 hphrocp_cnt; /* headphone right ocp retry */

	wait_queue_head_t wait_btn_press;
	bool is_btn_press;
	u8 current_plug;
	bool in_swch_irq_handler;

	struct snd_soc_codec *codec;

	/* impedance of hphl and hphr */
	uint32_t zl, zr;
	bool impedance_detect;

	struct snd_soc_jack headset_jack;
	struct snd_soc_jack button_jack;
	struct mutex codec_resource_lock;

	/* Holds codec specific interrupt mapping */
	const struct wcd_mbhc_intr *intr_ids;
};
int wcd_mbhc_start(struct wcd_mbhc *mbhc,
		       struct wcd_mbhc_config *mbhc_cfg);
void wcd_mbhc_stop(struct wcd_mbhc *mbhc);
int wcd_mbhc_init(struct wcd_mbhc *mbhc, struct snd_soc_codec *codec,
		      const struct wcd_mbhc_intr *mbhc_cdc_intr_ids,
		      bool impedance_det_en);
void wcd_mbhc_deinit(struct wcd_mbhc *mbhc);
#endif /* __WCD_MBHC_V2_H__ */
