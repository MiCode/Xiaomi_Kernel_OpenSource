/* Copyright (c) 2011-2012, The Linux Foundation. All rights reserved.
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
#include <sound/soc.h>
#include <sound/jack.h>
#include <linux/mfd/wcd9xxx/wcd9xxx-slimslave.h>

#define TABLA_NUM_REGISTERS 0x400
#define TABLA_MAX_REGISTER (TABLA_NUM_REGISTERS-1)
#define TABLA_CACHE_SIZE TABLA_NUM_REGISTERS
#define TABLA_1_X_ONLY_REGISTERS 3
#define TABLA_2_HIGHER_ONLY_REGISTERS 3

#define TABLA_REG_VAL(reg, val)		{reg, 0, val}

#define DEFAULT_DCE_STA_WAIT 55
#define DEFAULT_DCE_WAIT 60000
#define DEFAULT_STA_WAIT 5000
#define VDDIO_MICBIAS_MV 1800

#define STA 0
#define DCE 1

#define TABLA_JACK_BUTTON_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
				SND_JACK_BTN_2 | SND_JACK_BTN_3 | \
				SND_JACK_BTN_4 | SND_JACK_BTN_5 | \
				SND_JACK_BTN_6 | SND_JACK_BTN_7)

extern const u8 tabla_reg_readable[TABLA_CACHE_SIZE];
extern const u32 tabla_1_reg_readable[TABLA_1_X_ONLY_REGISTERS];
extern const u32 tabla_2_reg_readable[TABLA_2_HIGHER_ONLY_REGISTERS];
extern const u8 tabla_reg_defaults[TABLA_CACHE_SIZE];

enum tabla_micbias_num {
	TABLA_MICBIAS1 = 0,
	TABLA_MICBIAS2,
	TABLA_MICBIAS3,
	TABLA_MICBIAS4,
};

enum tabla_pid_current {
	TABLA_PID_MIC_2P5_UA,
	TABLA_PID_MIC_5_UA,
	TABLA_PID_MIC_10_UA,
	TABLA_PID_MIC_20_UA,
};

struct tabla_reg_mask_val {
	u16	reg;
	u8	mask;
	u8	val;
};

enum tabla_mbhc_clk_freq {
	TABLA_MCLK_12P2MHZ = 0,
	TABLA_MCLK_9P6MHZ,
	TABLA_NUM_CLK_FREQS,
};

enum tabla_mbhc_analog_pwr_cfg {
	TABLA_ANALOG_PWR_COLLAPSED = 0,
	TABLA_ANALOG_PWR_ON,
	TABLA_NUM_ANALOG_PWR_CONFIGS,
};

enum tabla_mbhc_btn_det_mem {
	TABLA_BTN_DET_V_BTN_LOW,
	TABLA_BTN_DET_V_BTN_HIGH,
	TABLA_BTN_DET_N_READY,
	TABLA_BTN_DET_N_CIC,
	TABLA_BTN_DET_GAIN
};

struct tabla_mbhc_general_cfg {
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

struct tabla_mbhc_plug_detect_cfg {
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

struct tabla_mbhc_plug_type_cfg {
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


struct tabla_mbhc_btn_detect_cfg {
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
	u8 _n_ready[TABLA_NUM_CLK_FREQS];
	u8 _n_cic[TABLA_NUM_CLK_FREQS];
	u8 _gain[TABLA_NUM_CLK_FREQS];
} __packed;

struct tabla_mbhc_imped_detect_cfg {
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

struct tabla_mbhc_config {
	struct snd_soc_jack *headset_jack;
	struct snd_soc_jack *button_jack;
	bool read_fw_bin;
	/* void* calibration contains:
	 *  struct tabla_mbhc_general_cfg generic;
	 *  struct tabla_mbhc_plug_detect_cfg plug_det;
	 *  struct tabla_mbhc_plug_type_cfg plug_type;
	 *  struct tabla_mbhc_btn_detect_cfg btn_det;
	 *  struct tabla_mbhc_imped_detect_cfg imped_det;
	 * Note: various size depends on btn_det->num_btn
	 */
	void *calibration;
	enum tabla_micbias_num micbias;
	int (*mclk_cb_fn) (struct snd_soc_codec*, int, bool);
	unsigned int mclk_rate;
	unsigned int gpio;
	unsigned int gpio_irq;
	int gpio_level_insert;
	/* swap_gnd_mic returns true if extern GND/MIC swap switch toggled */
	bool (*swap_gnd_mic) (struct snd_soc_codec *);
};

extern int tabla_hs_detect(struct snd_soc_codec *codec,
			   const struct tabla_mbhc_config *cfg);

struct anc_header {
	u32 reserved[3];
	u32 num_anc_slots;
};

extern int tabla_mclk_enable(struct snd_soc_codec *codec, int mclk_enable,
			     bool dapm);

extern void *tabla_mbhc_cal_btn_det_mp(const struct tabla_mbhc_btn_detect_cfg
				       *btn_det,
				       const enum tabla_mbhc_btn_det_mem mem);

#define TABLA_MBHC_CAL_SIZE(buttons, rload) ( \
	sizeof(enum tabla_micbias_num) + \
	sizeof(struct tabla_mbhc_general_cfg) + \
	sizeof(struct tabla_mbhc_plug_detect_cfg) + \
	    ((sizeof(s16) + sizeof(s16)) * buttons) + \
	sizeof(struct tabla_mbhc_plug_type_cfg) + \
	sizeof(struct tabla_mbhc_btn_detect_cfg) + \
	sizeof(struct tabla_mbhc_imped_detect_cfg) + \
	    ((sizeof(u16) + sizeof(u16)) * rload) \
	)

#define TABLA_MBHC_CAL_GENERAL_PTR(cali) ( \
	    (struct tabla_mbhc_general_cfg *) cali)
#define TABLA_MBHC_CAL_PLUG_DET_PTR(cali) ( \
	    (struct tabla_mbhc_plug_detect_cfg *) \
	    &(TABLA_MBHC_CAL_GENERAL_PTR(cali)[1]))
#define TABLA_MBHC_CAL_PLUG_TYPE_PTR(cali) ( \
	    (struct tabla_mbhc_plug_type_cfg *) \
	    &(TABLA_MBHC_CAL_PLUG_DET_PTR(cali)[1]))
#define TABLA_MBHC_CAL_BTN_DET_PTR(cali) ( \
	    (struct tabla_mbhc_btn_detect_cfg *) \
	    &(TABLA_MBHC_CAL_PLUG_TYPE_PTR(cali)[1]))
#define TABLA_MBHC_CAL_IMPED_DET_PTR(cali) ( \
	    (struct tabla_mbhc_imped_detect_cfg *) \
	    (((void *)&TABLA_MBHC_CAL_BTN_DET_PTR(cali)[1]) + \
	     (TABLA_MBHC_CAL_BTN_DET_PTR(cali)->num_btn * \
	      (sizeof(TABLA_MBHC_CAL_BTN_DET_PTR(cali)->_v_btn_low[0]) + \
	       sizeof(TABLA_MBHC_CAL_BTN_DET_PTR(cali)->_v_btn_high[0])))) \
	)

/* minimum size of calibration data assuming there is only one button and
 * one rload.
 */
#define TABLA_MBHC_CAL_MIN_SIZE ( \
	sizeof(struct tabla_mbhc_general_cfg) + \
	sizeof(struct tabla_mbhc_plug_detect_cfg) + \
	sizeof(struct tabla_mbhc_plug_type_cfg) + \
	sizeof(struct tabla_mbhc_btn_detect_cfg) + \
	sizeof(struct tabla_mbhc_imped_detect_cfg) + \
	(sizeof(u16) * 2))

#define TABLA_MBHC_CAL_BTN_SZ(cfg_ptr) ( \
	    sizeof(struct tabla_mbhc_btn_detect_cfg) + \
	    (cfg_ptr->num_btn * (sizeof(cfg_ptr->_v_btn_low[0]) + \
				 sizeof(cfg_ptr->_v_btn_high[0]))))

#define TABLA_MBHC_CAL_IMPED_MIN_SZ ( \
	    sizeof(struct tabla_mbhc_imped_detect_cfg) + \
	    sizeof(u16) * 2)

#define TABLA_MBHC_CAL_IMPED_SZ(cfg_ptr) ( \
	    sizeof(struct tabla_mbhc_imped_detect_cfg) + \
	    (cfg_ptr->_n_rload * (sizeof(cfg_ptr->_rload[0]) + \
				 sizeof(cfg_ptr->_alpha[0]))))


