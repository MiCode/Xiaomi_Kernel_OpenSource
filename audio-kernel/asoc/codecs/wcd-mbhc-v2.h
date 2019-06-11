/* Copyright (c) 2014-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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
#include <linux/stringify.h>
#include <linux/power_supply.h>
#include "wcdcal-hwdep.h"
#include <sound/jack.h>

#define TOMBAK_MBHC_NC	0
#define TOMBAK_MBHC_NO	1
#define WCD_MBHC_DEF_BUTTONS 8
#define WCD_MBHC_KEYCODE_NUM 8
#define WCD_MBHC_USLEEP_RANGE_MARGIN_US 100
#define WCD_MBHC_THR_HS_MICB_MV  2700
/* z value defined in Ohms */
#define WCD_MONO_HS_MIN_THR	2
#define WCD_MBHC_STRINGIFY(s)  __stringify(s)

#define WCD_MBHC_REGISTER(rid, rreg, rmask, rshift, rinvert) \
{ .id = rid, .reg = rreg, .mask = rmask, .offset = rshift, .invert = rinvert }

#define WCD_MBHC_RSC_LOCK(mbhc)			\
{							\
	pr_debug("%s: Acquiring BCL\n", __func__);	\
	mutex_lock(&mbhc->codec_resource_lock);		\
	pr_debug("%s: Acquiring BCL done\n", __func__);	\
}

#define WCD_MBHC_RSC_UNLOCK(mbhc)			\
{							\
	pr_debug("%s: Release BCL\n", __func__);	\
	mutex_unlock(&mbhc->codec_resource_lock);	\
}

#define WCD_MBHC_RSC_ASSERT_LOCKED(mbhc)		\
{							\
	WARN_ONCE(!mutex_is_locked(&mbhc->codec_resource_lock), \
		  "%s: BCL should have acquired\n", __func__); \
}

/*
 * Macros to update and read mbhc register bits. Check for
 * "0" before updating or reading the register, because it
 * is possible that one codec wants to write to that bit and
 * other codec does not.
 */
#define WCD_MBHC_REG_UPDATE_BITS(function, val)         \
do {                                                    \
	if (mbhc->wcd_mbhc_regs[function].reg) {        \
		snd_soc_update_bits(mbhc->codec,	\
		mbhc->wcd_mbhc_regs[function].reg,	\
		mbhc->wcd_mbhc_regs[function].mask,	\
		val << (mbhc->wcd_mbhc_regs[function].offset)); \
	}                                               \
} while (0)

#define WCD_MBHC_REG_READ(function, val)	        \
do {                                                    \
	if (mbhc->wcd_mbhc_regs[function].reg) {        \
		val = (((snd_soc_read(mbhc->codec,	\
		mbhc->wcd_mbhc_regs[function].reg)) &	\
		(mbhc->wcd_mbhc_regs[function].mask)) >> \
		(mbhc->wcd_mbhc_regs[function].offset)); \
	} else {                                         \
		val = -EINVAL;                           \
	}                                                \
} while (0)

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

#define WCD_MBHC_JACK_MASK (SND_JACK_HEADSET | SND_JACK_OC_HPHL | \
			   SND_JACK_OC_HPHR | SND_JACK_LINEOUT | \
			   SND_JACK_MECHANICAL | SND_JACK_MICROPHONE2 | \
			   SND_JACK_UNSUPPORTED)

#define WCD_MBHC_JACK_BUTTON_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
				  SND_JACK_BTN_2 | SND_JACK_BTN_3 | \
				  SND_JACK_BTN_4 | SND_JACK_BTN_5)
#define OCP_ATTEMPT 20
#define HS_DETECT_PLUG_TIME_MS (3 * 1000)
#define SPECIAL_HS_DETECT_TIME_MS (2 * 1000)
#define MBHC_BUTTON_PRESS_THRESHOLD_MIN 250
#define GND_MIC_SWAP_THRESHOLD 4
#define GND_MIC_USBC_SWAP_THRESHOLD 2
#define WCD_FAKE_REMOVAL_MIN_PERIOD_MS 100
#define HS_VREF_MIN_VAL 1400
#define FW_READ_ATTEMPTS 15
#define FW_READ_TIMEOUT 4000000
#define FAKE_REM_RETRY_ATTEMPTS 3
#define MAX_IMPED 60000

#define WCD_MBHC_BTN_PRESS_COMPL_TIMEOUT_MS  50
#define ANC_DETECT_RETRY_CNT 7
#define WCD_MBHC_SPL_HS_CNT  1

enum wcd_mbhc_detect_logic {
	WCD_DETECTION_LEGACY,
	WCD_DETECTION_ADC,
};

enum wcd_mbhc_cs_mb_en_flag {
	WCD_MBHC_EN_CS = 0,
	WCD_MBHC_EN_MB,
	WCD_MBHC_EN_PULLUP,
	WCD_MBHC_EN_NONE,
};

enum {
	WCD_MBHC_ELEC_HS_INS,
	WCD_MBHC_ELEC_HS_REM,
};

struct wcd_mbhc;
enum wcd_mbhc_register_function {
	WCD_MBHC_L_DET_EN,
	WCD_MBHC_GND_DET_EN,
	WCD_MBHC_MECH_DETECTION_TYPE,
	WCD_MBHC_MIC_CLAMP_CTL,
	WCD_MBHC_ELECT_DETECTION_TYPE,
	WCD_MBHC_HS_L_DET_PULL_UP_CTRL,
	WCD_MBHC_HS_L_DET_PULL_UP_COMP_CTRL,
	WCD_MBHC_HPHL_PLUG_TYPE,
	WCD_MBHC_GND_PLUG_TYPE,
	WCD_MBHC_SW_HPH_LP_100K_TO_GND,
	WCD_MBHC_ELECT_SCHMT_ISRC,
	WCD_MBHC_FSM_EN,
	WCD_MBHC_INSREM_DBNC,
	WCD_MBHC_BTN_DBNC,
	WCD_MBHC_HS_VREF,
	WCD_MBHC_HS_COMP_RESULT,
	WCD_MBHC_IN2P_CLAMP_STATE,
	WCD_MBHC_MIC_SCHMT_RESULT,
	WCD_MBHC_HPHL_SCHMT_RESULT,
	WCD_MBHC_HPHR_SCHMT_RESULT,
	WCD_MBHC_OCP_FSM_EN,
	WCD_MBHC_BTN_RESULT,
	WCD_MBHC_BTN_ISRC_CTL,
	WCD_MBHC_ELECT_RESULT,
	WCD_MBHC_MICB_CTRL,    /* Pull-up and micb control */
	WCD_MBHC_HPH_CNP_WG_TIME,
	WCD_MBHC_HPHR_PA_EN,
	WCD_MBHC_HPHL_PA_EN,
	WCD_MBHC_HPH_PA_EN,
	WCD_MBHC_SWCH_LEVEL_REMOVE,
	WCD_MBHC_PULLDOWN_CTRL,
	WCD_MBHC_ANC_DET_EN,
	WCD_MBHC_FSM_STATUS,
	WCD_MBHC_MUX_CTL,
	WCD_MBHC_MOISTURE_STATUS,
	WCD_MBHC_HPHR_GND,
	WCD_MBHC_HPHL_GND,
	WCD_MBHC_HPHL_OCP_DET_EN,
	WCD_MBHC_HPHR_OCP_DET_EN,
	WCD_MBHC_HPHL_OCP_STATUS,
	WCD_MBHC_HPHR_OCP_STATUS,
	WCD_MBHC_ADC_EN,
	WCD_MBHC_ADC_COMPLETE,
	WCD_MBHC_ADC_TIMEOUT,
	WCD_MBHC_ADC_RESULT,
	WCD_MBHC_MICB2_VOUT,
	WCD_MBHC_ADC_MODE,
	WCD_MBHC_DETECTION_DONE,
	WCD_MBHC_ELECT_ISRC_EN,
	WCD_MBHC_REG_FUNC_MAX,
};

enum wcd_mbhc_plug_type {
	MBHC_PLUG_TYPE_INVALID = -1,
	MBHC_PLUG_TYPE_NONE,
	MBHC_PLUG_TYPE_HEADSET,
	MBHC_PLUG_TYPE_HEADPHONE,
	MBHC_PLUG_TYPE_HIGH_HPH,
	MBHC_PLUG_TYPE_GND_MIC_SWAP,
	MBHC_PLUG_TYPE_ANC_HEADPHONE,
};

enum pa_dac_ack_flags {
	WCD_MBHC_HPHL_PA_OFF_ACK = 0,
	WCD_MBHC_HPHR_PA_OFF_ACK,
};

enum anc_ack_flags {
	WCD_MBHC_ANC0_OFF_ACK = 0,
	WCD_MBHC_ANC1_OFF_ACK,
};

enum wcd_mbhc_btn_det_mem {
	WCD_MBHC_BTN_DET_V_BTN_LOW,
	WCD_MBHC_BTN_DET_V_BTN_HIGH
};

enum {
	MIC_BIAS_1 = 1,
	MIC_BIAS_2,
	MIC_BIAS_3,
	MIC_BIAS_4
};

enum {
	MICB_PULLUP_ENABLE,
	MICB_PULLUP_DISABLE,
	MICB_ENABLE,
	MICB_DISABLE,
};

enum {
	MBHC_COMMON_MICB_PRECHARGE,
	MBHC_COMMON_MICB_SET_VAL,
	MBHC_COMMON_MICB_TAIL_CURR,
};

enum wcd_notify_event {
	WCD_EVENT_INVALID,
	/* events for micbias ON and OFF */
	WCD_EVENT_PRE_MICBIAS_2_OFF,
	WCD_EVENT_POST_MICBIAS_2_OFF,
	WCD_EVENT_PRE_MICBIAS_2_ON,
	WCD_EVENT_POST_MICBIAS_2_ON,
	WCD_EVENT_PRE_DAPM_MICBIAS_2_OFF,
	WCD_EVENT_POST_DAPM_MICBIAS_2_OFF,
	WCD_EVENT_PRE_DAPM_MICBIAS_2_ON,
	WCD_EVENT_POST_DAPM_MICBIAS_2_ON,
	/* events for PA ON and OFF */
	WCD_EVENT_PRE_HPHL_PA_ON,
	WCD_EVENT_POST_HPHL_PA_OFF,
	WCD_EVENT_PRE_HPHR_PA_ON,
	WCD_EVENT_POST_HPHR_PA_OFF,
	WCD_EVENT_PRE_HPHL_PA_OFF,
	WCD_EVENT_PRE_HPHR_PA_OFF,
	WCD_EVENT_OCP_OFF,
	WCD_EVENT_OCP_ON,
	WCD_EVENT_LAST,
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

enum wcd_mbhc_hph_type {
	WCD_MBHC_HPH_NONE = 0,
	WCD_MBHC_HPH_MONO,
	WCD_MBHC_HPH_STEREO,
};

/*
 * These enum definitions are directly mapped to the register
 * definitions
 */
enum mbhc_moisture_vref {
	V_OFF,
	V_45_MV,
	V_100_MV,
	V_225_MV,
};

enum mbhc_hs_pullup_iref {
	I_DEFAULT = -1,
	I_OFF = 0,
	I_1P0_UA,
	I_2P0_UA,
	I_3P0_UA,
};

enum mbhc_hs_pullup_iref_v2 {
	HS_PULLUP_I_DEFAULT = -1,
	HS_PULLUP_I_3P0_UA = 0,
	HS_PULLUP_I_2P25_UA,
	HS_PULLUP_I_1P5_UA,
	HS_PULLUP_I_0P75_UA,
	HS_PULLUP_I_1P125_UA = 0x05,
	HS_PULLUP_I_0P375_UA = 0x07,
	HS_PULLUP_I_2P0_UA,
	HS_PULLUP_I_1P0_UA = 0x0A,
	HS_PULLUP_I_0P5_UA,
	HS_PULLUP_I_0P25_UA = 0x0F,
	HS_PULLUP_I_0P125_UA = 0x17,
	HS_PULLUP_I_OFF,
};

enum mbhc_moisture_rref {
	R_OFF,
	R_24_KOHM,
	R_84_KOHM,
	R_184_KOHM,
};

struct usbc_ana_audio_config {
	int usbc_en1_gpio;
	int usbc_en2_gpio;
	int usbc_force_gpio;
	int euro_us_hw_switch_gpio;
	int uart_audio_switch_gpio;
	int subpcb_id_gpio;
	struct device_node *usbc_en1_gpio_p; /* used by pinctrl API */
	struct device_node *usbc_en2_gpio_p; /* used by pinctrl API */
	struct device_node *usbc_force_gpio_p; /* used by pinctrl API */
	struct device_node *euro_us_hw_switch_gpio_p; /* used by pinctrl API */
	struct device_node *uart_audio_switch_gpio_p; /* used by pinctrl API */
	struct device_node *subpcb_id_gpio_p; /* used by pinctrl API */
};

struct wcd_mbhc_config {
	bool read_fw_bin;
	void *calibration;
	bool detect_extn_cable;
	bool mono_stero_detection;
	bool (*swap_gnd_mic)(struct snd_soc_codec *codec, bool active);
	bool hs_ext_micbias;
	bool gnd_det_en;
	int key_code[WCD_MBHC_KEYCODE_NUM];
	uint32_t linein_th;
	bool moisture_en;
	int mbhc_micbias;
	int anc_micbias;
	bool enable_anc_mic_detect;
	u32 enable_usbc_analog;
	struct usbc_ana_audio_config usbc_analog_cfg;
	u32 use_fsa4476_gpio;
	bool moisture_duty_cycle_en;
	void (*enable_dual_adc_gpio)(struct device_node *node, bool en);
	struct device_node *dual_adc_gpio_node;
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

struct wcd_mbhc_register {
	const char *id;
	u16 reg;
	u8 mask;
	u8 offset;
	u8 invert;
};

struct wcd_mbhc_cb {
	int (*enable_mb_source)(struct wcd_mbhc *, bool);
	void (*trim_btn_reg)(struct snd_soc_codec *);
	void (*compute_impedance)(struct wcd_mbhc *, uint32_t *, uint32_t *);
	void (*set_micbias_value)(struct snd_soc_codec *);
	void (*set_auto_zeroing)(struct snd_soc_codec *, bool);
	struct firmware_cal * (*get_hwdep_fw_cal)(struct wcd_mbhc *,
			enum wcd_cal_type);
	void (*set_cap_mode)(struct snd_soc_codec *, bool, bool);
	int (*register_notifier)(struct wcd_mbhc *,
				 struct notifier_block *nblock,
				 bool enable);
	int (*request_irq)(struct snd_soc_codec *,
			int, irq_handler_t, const char *, void *);
	void (*irq_control)(struct snd_soc_codec *,
			int irq, bool enable);
	int (*free_irq)(struct snd_soc_codec *,
			int irq, void *);
	void (*clk_setup)(struct snd_soc_codec *, bool);
	int (*map_btn_code_to_num)(struct snd_soc_codec *);
	bool (*lock_sleep)(struct wcd_mbhc *, bool);
	bool (*micbias_enable_status)(struct wcd_mbhc *, int);
	void (*mbhc_bias)(struct snd_soc_codec *, bool);
	void (*mbhc_common_micb_ctrl)(struct snd_soc_codec *,
				      int event, bool);
	void (*micb_internal)(struct snd_soc_codec *,
			int micb_num, bool);
	bool (*hph_pa_on_status)(struct snd_soc_codec *);
	void (*set_btn_thr)(struct snd_soc_codec *, s16 *, s16 *,
			    int num_btn, bool);
	void (*hph_pull_up_control)(struct snd_soc_codec *,
				    enum mbhc_hs_pullup_iref);
	int (*mbhc_micbias_control)(struct snd_soc_codec *, int, int req);
	void (*mbhc_micb_ramp_control)(struct snd_soc_codec *, bool);
	void (*skip_imped_detect)(struct snd_soc_codec *);
	bool (*extn_use_mb)(struct snd_soc_codec *);
	int (*mbhc_micb_ctrl_thr_mic)(struct snd_soc_codec *, int, bool);
	void (*mbhc_gnd_det_ctrl)(struct snd_soc_codec *, bool);
	void (*hph_pull_down_ctrl)(struct snd_soc_codec *, bool);
	void (*mbhc_moisture_config)(struct wcd_mbhc *);
	bool (*hph_register_recovery)(struct wcd_mbhc *);
	void (*update_anc_state)(struct snd_soc_codec *codec,
				 bool enable, int anc_num);
	bool (*is_anc_on)(struct wcd_mbhc *mbhc);
	void (*hph_pull_up_control_v2)(struct snd_soc_codec *, int);
	bool (*mbhc_get_moisture_status)(struct wcd_mbhc *);
	void (*mbhc_moisture_polling_ctrl)(struct wcd_mbhc *, bool);
	void (*mbhc_moisture_detect_en)(struct wcd_mbhc *, bool);
};

struct wcd_mbhc_fn {
	irqreturn_t (*wcd_mbhc_hs_ins_irq)(int irq, void *data);
	irqreturn_t (*wcd_mbhc_hs_rem_irq)(int irq, void *data);
	void (*wcd_mbhc_detect_plug_type)(struct wcd_mbhc *mbhc);
	bool (*wcd_mbhc_detect_anc_plug_type)(struct wcd_mbhc *mbhc);
	void (*wcd_cancel_hs_detect_plug)(struct wcd_mbhc *mbhc,
					  struct work_struct *work);
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
	u32 hs_thr;
	u32 hph_thr;
	u32 swap_thr;
	u32 moist_vref;
	u32 moist_iref;
	u32 moist_rref;
	u8 micbias1_cap_mode; /* track ext cap setting */
	u8 micbias2_cap_mode; /* track ext cap setting */
	bool hs_detect_work_stop;
	bool micbias_enable;
	bool btn_press_intr;
	bool is_hs_recording;
	bool is_extn_cable;
	bool skip_imped_detection;
	bool is_btn_already_regd;
	bool extn_cable_hph_rem;

	struct snd_soc_codec *codec;
	/* Work to perform MBHC Firmware Read */
	struct delayed_work mbhc_firmware_dwork;
	const struct firmware *mbhc_fw;
	struct firmware_cal *mbhc_cal;

	/* track PA/DAC state to sync with userspace */
	unsigned long hph_pa_dac_state;
	unsigned long hph_anc_state;
	unsigned long event_state;
	unsigned long jiffies_atreport;

	/* impedance of hphl and hphr */
	uint32_t zl, zr;
	bool impedance_detect;

	/* Holds type of Headset - Mono/Stereo */
	enum wcd_mbhc_hph_type hph_type;

	struct snd_soc_jack headset_jack;
	struct snd_soc_jack button_jack;
	struct mutex codec_resource_lock;

	/* Holds codec specific interrupt mapping */
	const struct wcd_mbhc_intr *intr_ids;

	/* Work to correct accessory type */
	struct work_struct correct_plug_swch;
	struct notifier_block nblock;

	struct wcd_mbhc_register *wcd_mbhc_regs;

	struct completion btn_press_compl;
	struct mutex hphl_pa_lock;
	struct mutex hphr_pa_lock;
	bool deinit_in_progress;

	/* Holds mbhc detection method - ADC/Legacy */
	unsigned int mbhc_detection_logic;

	unsigned long intr_status;
	bool is_hph_ocp_pending;

	bool usbc_force_pr_mode;
	int usbc_mode;
	struct notifier_block psy_nb;
	struct power_supply *usb_psy;
	struct work_struct usbc_analog_work;

	struct wcd_mbhc_fn *mbhc_fn;
	bool force_linein;
	struct device_node *fsa_np;
	struct notifier_block fsa_nb;
};

void wcd_mbhc_find_plug_and_report(struct wcd_mbhc *mbhc,
				   enum wcd_mbhc_plug_type plug_type);
void wcd_mbhc_hs_elec_irq(struct wcd_mbhc *mbhc, int irq_type, bool enable);
void wcd_mbhc_elec_hs_report_unplug(struct wcd_mbhc *mbhc);
bool wcd_swch_level_remove(struct wcd_mbhc *mbhc);
void wcd_enable_curr_micbias(const struct wcd_mbhc *mbhc,
			     const enum wcd_mbhc_cs_mb_en_flag cs_mb_en);
void wcd_mbhc_jack_report(struct wcd_mbhc *mbhc,
			  struct snd_soc_jack *jack, int status, int mask);
int wcd_cancel_btn_work(struct wcd_mbhc *mbhc);
int wcd_mbhc_get_button_mask(struct wcd_mbhc *mbhc);
void wcd_mbhc_report_plug(struct wcd_mbhc *mbhc, int insertion,
			enum snd_jack_types jack_type);

#endif /* __WCD_MBHC_V2_H__ */
