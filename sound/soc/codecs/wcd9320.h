/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef WCD9320_H
#define WCD9320_H

#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>

#define TAIKO_NUM_REGISTERS 0x400
#define TAIKO_MAX_REGISTER (TAIKO_NUM_REGISTERS-1)
#define TAIKO_CACHE_SIZE TAIKO_NUM_REGISTERS

#define TAIKO_REG_VAL(reg, val)		{reg, 0, val}

#define DEFAULT_DCE_STA_WAIT 55
#define DEFAULT_DCE_WAIT 60000
#define DEFAULT_STA_WAIT 5000
#define VDDIO_MICBIAS_MV 1800

#define STA 0
#define DCE 1

#define TAIKO_JACK_BUTTON_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
				SND_JACK_BTN_2 | SND_JACK_BTN_3 | \
				SND_JACK_BTN_4 | SND_JACK_BTN_5 | \
				SND_JACK_BTN_6 | SND_JACK_BTN_7)

extern const u8 taiko_reg_readable[TAIKO_CACHE_SIZE];
extern const u8 taiko_reg_defaults[TAIKO_CACHE_SIZE];

enum taiko_micbias_num {
	TAIKO_MICBIAS1 = 0,
	TAIKO_MICBIAS2,
	TAIKO_MICBIAS3,
	TAIKO_MICBIAS4,
};

enum taiko_pid_current {
	TAIKO_PID_MIC_2P5_UA,
	TAIKO_PID_MIC_5_UA,
	TAIKO_PID_MIC_10_UA,
	TAIKO_PID_MIC_20_UA,
};

struct taiko_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

enum taiko_mbhc_clk_freq {
	TAIKO_MCLK_12P2MHZ = 0,
	TAIKO_MCLK_9P6MHZ,
	TAIKO_NUM_CLK_FREQS,
};

enum taiko_mbhc_analog_pwr_cfg {
	TAIKO_ANALOG_PWR_COLLAPSED = 0,
	TAIKO_ANALOG_PWR_ON,
	TAIKO_NUM_ANALOG_PWR_CONFIGS,
};

enum taiko_mbhc_btn_det_mem {
	TAIKO_BTN_DET_V_BTN_LOW,
	TAIKO_BTN_DET_V_BTN_HIGH,
	TAIKO_BTN_DET_N_READY,
	TAIKO_BTN_DET_N_CIC,
	TAIKO_BTN_DET_GAIN
};

struct taiko_mbhc_general_cfg {
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

struct taiko_mbhc_plug_detect_cfg {
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

struct taiko_mbhc_plug_type_cfg {
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


struct taiko_mbhc_btn_detect_cfg {
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

struct taiko_mbhc_imped_detect_cfg {
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

struct taiko_mbhc_config {
	struct snd_soc_jack *headset_jack;
	struct snd_soc_jack *button_jack;
	bool read_fw_bin;
	/* void* calibration contains:
	 *  struct taiko_mbhc_general_cfg generic;
	 *  struct taiko_mbhc_plug_detect_cfg plug_det;
	 *  struct taiko_mbhc_plug_type_cfg plug_type;
	 *  struct taiko_mbhc_btn_detect_cfg btn_det;
	 *  struct taiko_mbhc_imped_detect_cfg imped_det;
	 * Note: various size depends on btn_det->num_btn
	 */
	void *calibration;
	enum taiko_micbias_num micbias;
	int (*mclk_cb_fn) (struct snd_soc_codec*, int, bool);
	unsigned int mclk_rate;
	unsigned int gpio;
	unsigned int gpio_irq;
	int gpio_level_insert;
	/* swap_gnd_mic returns true if extern GND/MIC swap switch toggled */
	bool (*swap_gnd_mic) (struct snd_soc_codec *);
};

extern int taiko_hs_detect(struct snd_soc_codec *codec,
			   const struct taiko_mbhc_config *cfg);

struct anc_header {
	u32 reserved[3];
	u32 num_anc_slots;
};

extern int taiko_mclk_enable(struct snd_soc_codec *codec, int mclk_enable,
			     bool dapm);

extern void *taiko_mbhc_cal_btn_det_mp(const struct taiko_mbhc_btn_detect_cfg
				       *btn_det,
				       const enum taiko_mbhc_btn_det_mem mem);

#define TAIKO_MBHC_CAL_SIZE(buttons, rload) ( \
	sizeof(enum taiko_micbias_num) + \
	sizeof(struct taiko_mbhc_general_cfg) + \
	sizeof(struct taiko_mbhc_plug_detect_cfg) + \
	    ((sizeof(s16) + sizeof(s16)) * buttons) + \
	sizeof(struct taiko_mbhc_plug_type_cfg) + \
	sizeof(struct taiko_mbhc_btn_detect_cfg) + \
	sizeof(struct taiko_mbhc_imped_detect_cfg) + \
	    ((sizeof(u16) + sizeof(u16)) * rload) \
	)

#define TAIKO_MBHC_CAL_GENERAL_PTR(cali) ( \
	    (struct taiko_mbhc_general_cfg *) cali)
#define TAIKO_MBHC_CAL_PLUG_DET_PTR(cali) ( \
	    (struct taiko_mbhc_plug_detect_cfg *) \
	    &(TAIKO_MBHC_CAL_GENERAL_PTR(cali)[1]))
#define TAIKO_MBHC_CAL_PLUG_TYPE_PTR(cali) ( \
	    (struct taiko_mbhc_plug_type_cfg *) \
	    &(TAIKO_MBHC_CAL_PLUG_DET_PTR(cali)[1]))
#define TAIKO_MBHC_CAL_BTN_DET_PTR(cali) ( \
	    (struct taiko_mbhc_btn_detect_cfg *) \
	    &(TAIKO_MBHC_CAL_PLUG_TYPE_PTR(cali)[1]))
#define TAIKO_MBHC_CAL_IMPED_DET_PTR(cali) ( \
	    (struct taiko_mbhc_imped_detect_cfg *) \
	    (((void *)&TAIKO_MBHC_CAL_BTN_DET_PTR(cali)[1]) + \
	     (TAIKO_MBHC_CAL_BTN_DET_PTR(cali)->num_btn * \
	      (sizeof(TAIKO_MBHC_CAL_BTN_DET_PTR(cali)->_v_btn_low[0]) + \
	       sizeof(TAIKO_MBHC_CAL_BTN_DET_PTR(cali)->_v_btn_high[0])))) \
	)

/* minimum size of calibration data assuming there is only one button and
 * one rload.
 */
#define TAIKO_MBHC_CAL_MIN_SIZE ( \
	sizeof(struct taiko_mbhc_general_cfg) + \
	sizeof(struct taiko_mbhc_plug_detect_cfg) + \
	sizeof(struct taiko_mbhc_plug_type_cfg) + \
	sizeof(struct taiko_mbhc_btn_detect_cfg) + \
	sizeof(struct taiko_mbhc_imped_detect_cfg) + \
	(sizeof(u16) * 2))

#define TAIKO_MBHC_CAL_BTN_SZ(cfg_ptr) ( \
	    sizeof(struct taiko_mbhc_btn_detect_cfg) + \
	    (cfg_ptr->num_btn * (sizeof(cfg_ptr->_v_btn_low[0]) + \
				 sizeof(cfg_ptr->_v_btn_high[0]))))

#define TAIKO_MBHC_CAL_IMPED_MIN_SZ ( \
	    sizeof(struct taiko_mbhc_imped_detect_cfg) + \
	    sizeof(u16) * 2)

#define TAIKO_MBHC_CAL_IMPED_SZ(cfg_ptr) ( \
	    sizeof(struct taiko_mbhc_imped_detect_cfg) + \
	    (cfg_ptr->_n_rload * (sizeof(cfg_ptr->_rload[0]) + \
				 sizeof(cfg_ptr->_alpha[0]))))

#endif
