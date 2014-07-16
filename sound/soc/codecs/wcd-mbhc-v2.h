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

#define TOMBAK_MBHC_NC	0
#define TOMBAK_MBHC_NO	1
#define WCD_MBHC_DEF_BUTTONS 5

enum wcd_mbhc_plug_type {
	MBHC_PLUG_TYPE_INVALID = -1,
	MBHC_PLUG_TYPE_NONE,
	MBHC_PLUG_TYPE_HEADSET,
	MBHC_PLUG_TYPE_HEADPHONE,
	MBHC_PLUG_TYPE_HIGH_HPH,
	MBHC_PLUG_TYPE_GND_MIC_SWAP,
};

enum pa_dac_ack_flags {
	WCD_MBHC_HPHL_PA_OFF_ACK = 0,
	WCD_MBHC_HPHR_PA_OFF_ACK,
};

enum wcd_mbhc_btn_det_mem {
	WCD_MBHC_BTN_DET_V_BTN_LOW,
	WCD_MBHC_BTN_DET_V_BTN_HIGH
};

struct wcd_mbhc_btn_detect_cfg {
	u8 num_btn;
	s16 _v_btn_low[WCD_MBHC_DEF_BUTTONS];
	s16 _v_btn_high[WCD_MBHC_DEF_BUTTONS];
} __packed;

struct wcd_mbhc_config {
	bool read_fw_bin;
	void *calibration;
	bool detect_extn_cable;
	bool mono_stero_detection;
	bool (*swap_gnd_mic) (struct snd_soc_codec *codec);
	bool hs_ext_micbias;
};

struct wcd_mbhc_intr {
	int mbhc_sw_intr;
	int mbhc_btn_press_intr;
	int mbhc_btn_release_intr;
	int mbhc_hs_ins_rem_intr;
	int hph_left_ocp;
	int hph_right_ocp;
};

struct wcd_mbhc_cb {
	int (*enable_mb_source) (struct snd_soc_codec *, bool);
};

struct wcd_mbhc {
	int buttons_pressed;
	struct wcd_mbhc_config *mbhc_cfg;
	const struct wcd_mbhc_cb *mbhc_cb;

	u32 hph_status; /* track headhpone status */
	u8 hphlocp_cnt; /* headphone left ocp retry */
	u8 hphrocp_cnt; /* headphone right ocp retry */

	wait_queue_head_t wait_btn_press;
	bool is_btn_press;
	bool is_hs_inserted;
	u8 current_plug;
	bool in_swch_irq_handler;
	bool hphl_swh; /*track HPHL switch NC / NO */
	bool gnd_swh; /*track GND switch NC / NO */
	bool hs_detect_work_stop;
	bool micbias_enable;
	bool btn_press_intr;
	bool is_hs_recording;

	struct snd_soc_codec *codec;

	/* track PA/DAC state to sync with userspace */
	unsigned long hph_pa_dac_state;
	unsigned long jiffies_atreport;

	/* impedance of hphl and hphr */
	uint32_t zl, zr;
	bool impedance_detect;

	struct snd_soc_jack headset_jack;
	struct snd_soc_jack button_jack;
	struct mutex codec_resource_lock;

	/* Holds codec specific interrupt mapping */
	const struct wcd_mbhc_intr *intr_ids;

	/* Work to correct accessory type */
	struct work_struct correct_plug_swch;
	struct notifier_block nblock;
};

#define WCD_MBHC_CAL_BTN_DET_PTR(cali) ( \
	    (struct wcd_mbhc_btn_detect_cfg *) cali)

int wcd_mbhc_start(struct wcd_mbhc *mbhc,
		       struct wcd_mbhc_config *mbhc_cfg);
void wcd_mbhc_stop(struct wcd_mbhc *mbhc);
int wcd_mbhc_init(struct wcd_mbhc *mbhc, struct snd_soc_codec *codec,
		      const struct wcd_mbhc_cb *mbhc_cb,
		      const struct wcd_mbhc_intr *mbhc_cdc_intr_ids,
		      bool impedance_det_en);
void wcd_mbhc_deinit(struct wcd_mbhc *mbhc);
#endif /* __WCD_MBHC_V2_H__ */
