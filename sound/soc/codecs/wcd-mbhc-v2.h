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
#include "wcdcal-hwdep.h"

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

enum wcd_mbhc_event_state {
	WCD_MBHC_EVENT_PA_HPHL,
	WCD_MBHC_EVENT_PA_HPHR,
};
struct wcd_mbhc_general_cfg {
	u8 t_ldoh;
	u8 t_bg_fast_settle;
	u8 t_shutdown_plug_rem;
	u8 mbhc_nsa;
	u8 mbhc_navg;
	u8 v_micbias_l;
	u8 v_micbias;
	u8 mbhc_reserved;
	u16 settle_wait;
	u16 t_micbias_rampup;
	u16 t_micbias_rampdown;
	u16 t_supply_bringup;
} __packed;

struct wcd_mbhc_plug_detect_cfg {
	u32 mic_current;
	u32 hph_current;
	u16 t_mic_pid;
	u16 t_ins_complete;
	u16 t_ins_retry;
	u16 v_removal_delta;
	u8 micbias_slow_ramp;
	u8 reserved0;
	u8 reserved1;
	u8 reserved2;
} __packed;

struct wcd_mbhc_plug_type_cfg {
	u8 av_detect;
	u8 mono_detect;
	u8 num_ins_tries;
	u8 reserved0;
	s16 v_no_mic;
	s16 v_av_min;
	s16 v_av_max;
	s16 v_hs_min;
	s16 v_hs_max;
	u16 reserved1;
} __packed;

struct wcd_mbhc_btn_detect_cfg {
	s8 c[8];
	u8 nc;
	u8 n_meas;
	u8 mbhc_nsc;
	u8 n_btn_meas;
	u8 n_btn_con;
	u8 num_btn;
	u8 reserved0;
	u8 reserved1;
	u16 t_poll;
	u16 t_bounce_wait;
	u16 t_rel_timeout;
	s16 v_btn_press_delta_sta;
	s16 v_btn_press_delta_cic;
	u16 t_btn0_timeout;
	s16 _v_btn_low[0]; /* v_btn_low[num_btn] */
	s16 _v_btn_high[0]; /* v_btn_high[num_btn] */
	u8 _n_ready[2];
	u8 _n_cic[2];
	u8 _gain[2];
} __packed;

struct wcd_mbhc_imped_detect_cfg {
	u8 _hs_imped_detect;
	u8 _n_rload;
	u8 _hph_keep_on;
	u8 _repeat_rload_calc;
	u16 _t_dac_ramp_time;
	u16 _rhph_high;
	u16 _rhph_low;
	u16 _rload[0]; /* rload[n_rload] */
	u16 _alpha[0]; /* alpha[n_rload] */
	u16 _beta[3];
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
	int mbhc_hs_ins_intr;
	int mbhc_hs_rem_intr;
	int hph_left_ocp;
	int hph_right_ocp;
};

struct wcd_mbhc_cb {
	int (*enable_mb_source) (struct snd_soc_codec *, bool);
	void (*trim_btn_reg) (struct snd_soc_codec *);
	void (*compute_impedance) (s16 , s16 , uint32_t *, uint32_t *, bool);
	void (*set_micbias_value) (struct snd_soc_codec *);
	void (*set_auto_zeroing) (struct snd_soc_codec *, bool);
	struct firmware_cal * (*get_hwdep_fw_cal)(struct snd_soc_codec *,
			enum wcd_cal_type);
	void (*set_cap_mode)(struct snd_soc_codec *, bool, bool);
	void (*skip_imped_detect)(struct snd_soc_codec *);
};

struct wcd_mbhc {
	/* Delayed work to report long button press */
	struct delayed_work mbhc_btn_dwork;
	int buttons_pressed;
	struct wcd_mbhc_config *mbhc_cfg;
	const struct wcd_mbhc_cb *mbhc_cb;

	u32 hph_status; /* track headhpone status */
	u8 hphlocp_cnt; /* headphone left ocp retry */
	u8 hphrocp_cnt; /* headphone right ocp retry */

	wait_queue_head_t wait_btn_press;
	bool is_btn_press;
	u8 current_plug;
	bool in_swch_irq_handler;
	bool hphl_swh; /*track HPHL switch NC / NO */
	bool gnd_swh; /*track GND switch NC / NO */
	u8 micbias1_cap_mode; /* track ext cap setting */
	u8 micbias2_cap_mode; /* track ext cap setting */
	bool hs_detect_work_stop;
	bool micbias_enable;
	bool btn_press_intr;
	bool is_hs_recording;
	bool is_extn_cable;
	bool skip_imped_detection;

	struct snd_soc_codec *codec;
	/* Work to perform MBHC Firmware Read */
	struct delayed_work mbhc_firmware_dwork;
	const struct firmware *mbhc_fw;
	struct firmware_cal *mbhc_cal;

	/* track PA/DAC state to sync with userspace */
	unsigned long hph_pa_dac_state;
	unsigned long event_state;
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
#define WCD_MBHC_CAL_SIZE(buttons, rload) ( \
	sizeof(struct wcd_mbhc_general_cfg) + \
	sizeof(struct wcd_mbhc_plug_detect_cfg) + \
	((sizeof(s16) + sizeof(s16)) * buttons) + \
	    sizeof(struct wcd_mbhc_plug_type_cfg) + \
	sizeof(struct wcd_mbhc_btn_detect_cfg) + \
	sizeof(struct wcd_mbhc_imped_detect_cfg) + \
		((sizeof(u16) + sizeof(u16)) * rload) \
	)


#define WCD_MBHC_CAL_GENERAL_PTR(cali) ( \
	(struct wcd_mbhc_general_cfg *) cali)
#define WCD_MBHC_CAL_PLUG_DET_PTR(cali) ( \
	(struct wcd_mbhc_plug_detect_cfg *) \
	&(WCD_MBHC_CAL_GENERAL_PTR(cali)[1]))
#define WCD_MBHC_CAL_PLUG_TYPE_PTR(cali) ( \
	(struct wcd_mbhc_plug_type_cfg *) \
	&(WCD_MBHC_CAL_PLUG_DET_PTR(cali)[1]))
#define WCD_MBHC_CAL_BTN_DET_PTR(cali) ( \
	    (struct wcd_mbhc_btn_detect_cfg *) \
	&(WCD_MBHC_CAL_PLUG_TYPE_PTR(cali)[1]))
#define WCD_MBHC_CAL_IMPED_DET_PTR(cali) ( \
	(struct wcd_mbhc_imped_detect_cfg *) \
	(((void *)&WCD_MBHC_CAL_BTN_DET_PTR(cali)[1]) + \
	(WCD_MBHC_CAL_BTN_DET_PTR(cali)->num_btn * \
	(sizeof(WCD_MBHC_CAL_BTN_DET_PTR(cali)->_v_btn_low[0]) + \
	sizeof(WCD_MBHC_CAL_BTN_DET_PTR(cali)->_v_btn_high[0])))) \
	)

#define WCD_MBHC_CAL_MIN_SIZE ( \
	sizeof(struct wcd_mbhc_general_cfg) + \
	sizeof(struct wcd_mbhc_plug_detect_cfg) + \
	sizeof(struct wcd_mbhc_plug_type_cfg) + \
	sizeof(struct wcd_mbhc_btn_detect_cfg) + \
	sizeof(struct wcd_mbhc_imped_detect_cfg) + \
	(sizeof(u16)*2)  \
	)

#define WCD_MBHC_CAL_BTN_SZ(cfg_ptr) ( \
	sizeof(struct wcd_mbhc_btn_detect_cfg) + \
	(cfg_ptr->num_btn * (sizeof(cfg_ptr->_v_btn_low[0]) + \
			sizeof(cfg_ptr->_v_btn_high[0]))))

#define WCD_MBHC_CAL_IMPED_MIN_SZ ( \
	sizeof(struct wcd_mbhc_imped_detect_cfg) + sizeof(u16) * 2)

#define WCD_MBHC_CAL_IMPED_SZ(cfg_ptr) ( \
	sizeof(struct wcd_mbhc_imped_detect_cfg) + \
	(cfg_ptr->_n_rload * \
	(sizeof(cfg_ptr->_rload[0]) + sizeof(cfg_ptr->_alpha[0]))))

int wcd_mbhc_start(struct wcd_mbhc *mbhc,
		       struct wcd_mbhc_config *mbhc_cfg);
void wcd_mbhc_stop(struct wcd_mbhc *mbhc);
int wcd_mbhc_init(struct wcd_mbhc *mbhc, struct snd_soc_codec *codec,
		      const struct wcd_mbhc_cb *mbhc_cb,
		      const struct wcd_mbhc_intr *mbhc_cdc_intr_ids,
		      bool impedance_det_en);
int wcd_mbhc_get_impedance(struct wcd_mbhc *mbhc, uint32_t *zl,
			   uint32_t *zr);
void wcd_mbhc_deinit(struct wcd_mbhc *mbhc);
#endif /* __WCD_MBHC_V2_H__ */
