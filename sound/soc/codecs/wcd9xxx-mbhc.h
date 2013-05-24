/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#ifndef __WCD9XXX_MBHC_H__
#define __WCD9XXX_MBHC_H__

#include "wcd9xxx-resmgr.h"

#define WCD9XXX_CFILT_FAST_MODE 0x00
#define WCD9XXX_CFILT_SLOW_MODE 0x40
#define WCD9XXX_CFILT_EXT_PRCHG_EN 0x70
#define WCD9XXX_CFILT_EXT_PRCHG_DSBL 0x40

struct mbhc_micbias_regs {
	u16 cfilt_val;
	u16 cfilt_ctl;
	u16 mbhc_reg;
	u16 int_rbias;
	u16 ctl_reg;
	u8 cfilt_sel;
};

enum mbhc_v_index {
	MBHC_V_IDX_CFILT,
	MBHC_V_IDX_VDDIO,
	MBHC_V_IDX_NUM,
};

enum mbhc_cal_type {
	MBHC_CAL_MCLK,
	MBHC_CAL_RCO,
	MBHC_CAL_NUM,
};

/* Data used by MBHC */
struct mbhc_internal_cal_data {
	u16 dce_z;
	u16 dce_mb;
	u16 sta_z;
	u16 sta_mb;
	u32 t_sta_dce;
	u32 t_dce;
	u32 t_sta;
	u32 micb_mv;
	u16 v_ins_hu[MBHC_V_IDX_NUM];
	u16 v_ins_h[MBHC_V_IDX_NUM];
	u16 v_b1_hu[MBHC_V_IDX_NUM];
	u16 v_b1_h[MBHC_V_IDX_NUM];
	u16 v_brh[MBHC_V_IDX_NUM];
	u16 v_brl;
	u16 v_no_mic;
	s16 v_inval_ins_low;
	s16 v_inval_ins_high;
};

enum wcd9xxx_mbhc_version {
	WCD9XXX_MBHC_VERSION_UNKNOWN = 0,
	WCD9XXX_MBHC_VERSION_TAIKO,
	WCD9XXX_MBHC_VERSION_TAPAN,
};

enum wcd9xxx_mbhc_plug_type {
	PLUG_TYPE_INVALID = -1,
	PLUG_TYPE_NONE,
	PLUG_TYPE_HEADSET,
	PLUG_TYPE_HEADPHONE,
	PLUG_TYPE_HIGH_HPH,
	PLUG_TYPE_GND_MIC_SWAP,
};

enum wcd9xxx_micbias_num {
	MBHC_MICBIAS_INVALID = -1,
	MBHC_MICBIAS1,
	MBHC_MICBIAS2,
	MBHC_MICBIAS3,
	MBHC_MICBIAS4,
};

enum wcd9xx_mbhc_micbias_enable_bits {
	MBHC_MICBIAS_ENABLE_THRESHOLD_HEADSET,
	MBHC_MICBIAS_ENABLE_REGULAR_HEADSET,
};

enum wcd9xxx_mbhc_state {
	MBHC_STATE_NONE = -1,
	MBHC_STATE_POTENTIAL,
	MBHC_STATE_POTENTIAL_RECOVERY,
	MBHC_STATE_RELEASE,
};

enum wcd9xxx_mbhc_btn_det_mem {
	MBHC_BTN_DET_V_BTN_LOW,
	MBHC_BTN_DET_V_BTN_HIGH,
	MBHC_BTN_DET_N_READY,
	MBHC_BTN_DET_N_CIC,
	MBHC_BTN_DET_GAIN
};

enum wcd9xxx_mbhc_clk_freq {
	TAIKO_MCLK_12P2MHZ = 0,
	TAIKO_MCLK_9P6MHZ,
	TAIKO_NUM_CLK_FREQS,
};

enum wcd9xxx_mbhc_event_state {
	MBHC_EVENT_PA_HPHL,
	MBHC_EVENT_PA_HPHR,
};

struct wcd9xxx_mbhc_general_cfg {
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

struct wcd9xxx_mbhc_plug_detect_cfg {
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

struct wcd9xxx_mbhc_plug_type_cfg {
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

struct wcd9xxx_mbhc_btn_detect_cfg {
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
	u8 _n_ready[TAIKO_NUM_CLK_FREQS];
	u8 _n_cic[TAIKO_NUM_CLK_FREQS];
	u8 _gain[TAIKO_NUM_CLK_FREQS];
} __packed;

struct wcd9xxx_mbhc_imped_detect_cfg {
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

struct wcd9xxx_mbhc_config {
	bool read_fw_bin;
	/*
	 * void* calibration contains:
	 *  struct wcd9xxx_mbhc_general_cfg generic;
	 *  struct wcd9xxx_mbhc_plug_detect_cfg plug_det;
	 *  struct wcd9xxx_mbhc_plug_type_cfg plug_type;
	 *  struct wcd9xxx_mbhc_btn_detect_cfg btn_det;
	 *  struct wcd9xxx_mbhc_imped_detect_cfg imped_det;
	 * Note: various size depends on btn_det->num_btn
	 */
	void *calibration;
	enum wcd9xxx_micbias_num micbias;
	int (*mclk_cb_fn) (struct snd_soc_codec*, int, bool);
	unsigned int mclk_rate;
	unsigned int gpio;
	unsigned int gpio_irq;
	int gpio_level_insert;
	bool insert_detect; /* codec has own MBHC_INSERT_DETECT */
	bool detect_extn_cable;
	/* bit mask of enum wcd9xx_mbhc_micbias_enable_bits */
	unsigned long micbias_enable_flags;
	/* swap_gnd_mic returns true if extern GND/MIC swap switch toggled */
	bool (*swap_gnd_mic) (struct snd_soc_codec *);
};

struct wcd9xxx_mbhc {
	bool polling_active;
	/* Delayed work to report long button press */
	struct delayed_work mbhc_btn_dwork;
	int buttons_pressed;
	enum wcd9xxx_mbhc_state mbhc_state;
	struct wcd9xxx_mbhc_config *mbhc_cfg;

	struct mbhc_internal_cal_data mbhc_data;

	struct mbhc_micbias_regs mbhc_bias_regs;
	bool mbhc_micbias_switched;

	u32 hph_status; /* track headhpone status */
	u8 hphlocp_cnt; /* headphone left ocp retry */
	u8 hphrocp_cnt; /* headphone right ocp retry */

	/* Work to perform MBHC Firmware Read */
	struct delayed_work mbhc_firmware_dwork;
	const struct firmware *mbhc_fw;

	struct delayed_work mbhc_insert_dwork;

	u8 current_plug;
	struct work_struct correct_plug_swch;
	/*
	 * Work to perform polling on microphone voltage
	 * in order to correct plug type once plug type
	 * is detected as headphone
	 */
	struct work_struct correct_plug_noswch;
	bool hs_detect_work_stop;

	bool lpi_enabled; /* low power insertion detection */
	bool in_swch_irq_handler;

	struct wcd9xxx_resmgr *resmgr;
	struct snd_soc_codec *codec;

	bool no_mic_headset_override;

	/* track PA/DAC state to sync with userspace */
	unsigned long hph_pa_dac_state;
	/*
	 * save codec's state with resmgr event notification
	 * bit flags of enum wcd9xxx_mbhc_event_state
	 */
	unsigned long event_state;

	unsigned long mbhc_last_resume; /* in jiffies */

	bool insert_detect_level_insert;

	struct snd_soc_jack headset_jack;
	struct snd_soc_jack button_jack;

	struct notifier_block nblock;

	bool micbias_enable;
	int (*micbias_enable_cb) (struct snd_soc_codec*,  bool);

	enum wcd9xxx_mbhc_version mbhc_version;

	u32 rco_clk_rate;

#ifdef CONFIG_DEBUG_FS
	struct dentry *debugfs_poke;
	struct dentry *debugfs_mbhc;
#endif
};

#define WCD9XXX_MBHC_CAL_SIZE(buttons, rload) ( \
	sizeof(enum wcd9xxx_micbias_num) + \
	sizeof(struct wcd9xxx_mbhc_general_cfg) + \
	sizeof(struct wcd9xxx_mbhc_plug_detect_cfg) + \
	    ((sizeof(s16) + sizeof(s16)) * buttons) + \
	sizeof(struct wcd9xxx_mbhc_plug_type_cfg) + \
	sizeof(struct wcd9xxx_mbhc_btn_detect_cfg) + \
	sizeof(struct wcd9xxx_mbhc_imped_detect_cfg) + \
	    ((sizeof(u16) + sizeof(u16)) * rload) \
	)

#define WCD9XXX_MBHC_CAL_GENERAL_PTR(cali) ( \
	    (struct wcd9xxx_mbhc_general_cfg *) cali)
#define WCD9XXX_MBHC_CAL_PLUG_DET_PTR(cali) ( \
	    (struct wcd9xxx_mbhc_plug_detect_cfg *) \
	    &(WCD9XXX_MBHC_CAL_GENERAL_PTR(cali)[1]))
#define WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(cali) ( \
	    (struct wcd9xxx_mbhc_plug_type_cfg *) \
	    &(WCD9XXX_MBHC_CAL_PLUG_DET_PTR(cali)[1]))
#define WCD9XXX_MBHC_CAL_BTN_DET_PTR(cali) ( \
	    (struct wcd9xxx_mbhc_btn_detect_cfg *) \
	    &(WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(cali)[1]))
#define WCD9XXX_MBHC_CAL_IMPED_DET_PTR(cali) ( \
	    (struct wcd9xxx_mbhc_imped_detect_cfg *) \
	    (((void *)&WCD9XXX_MBHC_CAL_BTN_DET_PTR(cali)[1]) + \
	     (WCD9XXX_MBHC_CAL_BTN_DET_PTR(cali)->num_btn * \
	      (sizeof(WCD9XXX_MBHC_CAL_BTN_DET_PTR(cali)->_v_btn_low[0]) + \
	       sizeof(WCD9XXX_MBHC_CAL_BTN_DET_PTR(cali)->_v_btn_high[0])))) \
	)

/* minimum size of calibration data assuming there is only one button and
 * one rload.
 */
#define WCD9XXX_MBHC_CAL_MIN_SIZE ( \
	    sizeof(struct wcd9xxx_mbhc_general_cfg) + \
	    sizeof(struct wcd9xxx_mbhc_plug_detect_cfg) + \
	    sizeof(struct wcd9xxx_mbhc_plug_type_cfg) + \
	    sizeof(struct wcd9xxx_mbhc_btn_detect_cfg) + \
	    sizeof(struct wcd9xxx_mbhc_imped_detect_cfg) + \
	    (sizeof(u16) * 2) \
	)

#define WCD9XXX_MBHC_CAL_BTN_SZ(cfg_ptr) ( \
	    sizeof(struct wcd9xxx_mbhc_btn_detect_cfg) + \
	    (cfg_ptr->num_btn * (sizeof(cfg_ptr->_v_btn_low[0]) + \
				 sizeof(cfg_ptr->_v_btn_high[0]))))

#define WCD9XXX_MBHC_CAL_IMPED_MIN_SZ ( \
	    sizeof(struct wcd9xxx_mbhc_imped_detect_cfg) + sizeof(u16) * 2)

#define WCD9XXX_MBHC_CAL_IMPED_SZ(cfg_ptr) ( \
	    sizeof(struct wcd9xxx_mbhc_imped_detect_cfg) + \
	    (cfg_ptr->_n_rload * \
	     (sizeof(cfg_ptr->_rload[0]) + sizeof(cfg_ptr->_alpha[0]))))

int wcd9xxx_mbhc_start(struct wcd9xxx_mbhc *mbhc,
		       struct wcd9xxx_mbhc_config *mbhc_cfg);
int wcd9xxx_mbhc_init(struct wcd9xxx_mbhc *mbhc, struct wcd9xxx_resmgr *resmgr,
		      struct snd_soc_codec *codec,
		      int (*micbias_enable_cb) (struct snd_soc_codec*,  bool),
		      int version,
		      int rco_clk_rate);
void wcd9xxx_mbhc_deinit(struct wcd9xxx_mbhc *mbhc);
void *wcd9xxx_mbhc_cal_btn_det_mp(
			    const struct wcd9xxx_mbhc_btn_detect_cfg *btn_det,
			    const enum wcd9xxx_mbhc_btn_det_mem mem);
#endif /* __WCD9XXX_MBHC_H__ */
