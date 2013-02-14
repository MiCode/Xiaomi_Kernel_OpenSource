/* Copyright (c) 2009, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __ARCH_ARM_MACH_PMIC_H
#define __ARCH_ARM_MACH_PMIC_H

#include <mach/proc_comm.h>

enum spkr_left_right {
	LEFT_SPKR,
	RIGHT_SPKR,
};

enum spkr_gain {
	SPKR_GAIN_MINUS16DB,      /* -16 db */
	SPKR_GAIN_MINUS12DB,      /* -12 db */
	SPKR_GAIN_MINUS08DB,      /* -08 db */
	SPKR_GAIN_MINUS04DB,      /* -04 db */
	SPKR_GAIN_00DB,           /*  00 db */
	SPKR_GAIN_PLUS04DB,       /* +04 db */
	SPKR_GAIN_PLUS08DB,       /* +08 db */
	SPKR_GAIN_PLUS12DB,       /* +12 db */
};

enum spkr_dly {
	SPKR_DLY_10MS,            /* ~10  ms delay */
	SPKR_DLY_100MS,           /* ~100 ms delay */
};

enum spkr_hpf_corner_freq {
	SPKR_FREQ_1_39KHZ,         /* 1.39 kHz */
	SPKR_FREQ_0_64KHZ,         /* 0.64 kHz */
	SPKR_FREQ_0_86KHZ,         /* 0.86 kHz */
	SPKR_FREQ_0_51KHZ,         /* 0.51 kHz */
	SPKR_FREQ_1_06KHZ,         /* 1.06 kHz */
	SPKR_FREQ_0_57KHZ,         /* 0.57 kHz */
	SPKR_FREQ_0_73KHZ,         /* 0.73 kHz */
	SPKR_FREQ_0_47KHZ,         /* 0.47 kHz */
	SPKR_FREQ_1_20KHZ,         /* 1.20 kHz */
	SPKR_FREQ_0_60KHZ,         /* 0.60 kHz */
	SPKR_FREQ_0_76KHZ,         /* 0.76 kHz */
	SPKR_FREQ_0_49KHZ,         /* 0.49 kHz */
	SPKR_FREQ_0_95KHZ,         /* 0.95 kHz */
	SPKR_FREQ_0_54KHZ,         /* 0.54 kHz */
	SPKR_FREQ_0_68KHZ,         /* 0.68 kHz */
	SPKR_FREQ_0_45KHZ,         /* 0.45 kHz */
};

/* Turn the speaker on or off and enables or disables mute.*/
enum spkr_cmd {
	SPKR_DISABLE,  /* Enable Speaker */
	SPKR_ENABLE,   /* Disable Speaker */
	SPKR_MUTE_OFF, /* turn speaker mute off, SOUND ON */
	SPKR_MUTE_ON,  /* turn speaker mute on, SOUND OFF */
	SPKR_OFF,      /* turn speaker OFF (speaker disable and mute on) */
	SPKR_ON,        /* turn speaker ON (speaker enable and mute off)  */
	SPKR_SET_FREQ_CMD,    /* set speaker frequency */
	SPKR_GET_FREQ_CMD,    /* get speaker frequency */
	SPKR_SET_GAIN_CMD,    /* set speaker gain */
	SPKR_GET_GAIN_CMD,    /* get speaker gain */
	SPKR_SET_DELAY_CMD,   /* set speaker delay */
	SPKR_GET_DELAY_CMD,   /* get speaker delay */
	SPKR_SET_PDM_MODE,
	SPKR_SET_PWM_MODE,
};

struct spkr_config_mode {
	uint32_t is_right_chan_en;
	uint32_t is_left_chan_en;
	uint32_t is_right_left_chan_added;
	uint32_t is_stereo_en;
	uint32_t is_usb_with_hpf_20hz;
	uint32_t is_mux_bypassed;
	uint32_t is_hpf_en;
	uint32_t is_sink_curr_from_ref_volt_cir_en;
};

enum mic_volt {
	MIC_VOLT_2_00V,            /*  2.00 V  */
	MIC_VOLT_1_93V,            /*  1.93 V  */
	MIC_VOLT_1_80V,            /*  1.80 V  */
	MIC_VOLT_1_73V,            /*  1.73 V  */
};

enum ledtype {
	LED_LCD,
	LED_KEYPAD,
};

enum flash_led_mode {
	FLASH_LED_MODE__MANUAL,
	FLASH_LED_MODE__DBUS1,
	FLASH_LED_MODE__DBUS2,
	FLASH_LED_MODE__DBUS3,
};

enum flash_led_pol {
	FLASH_LED_POL__ACTIVE_HIGH,
	FLASH_LED_POL__ACTIVE_LOW,
};

enum switch_cmd {
	OFF_CMD,
	ON_CMD
};

enum vreg_lp_id {
	PM_VREG_LP_MSMA_ID,
	PM_VREG_LP_MSMP_ID,
	PM_VREG_LP_MSME1_ID,
	PM_VREG_LP_GP3_ID,
	PM_VREG_LP_MSMC_ID,
	PM_VREG_LP_MSME2_ID,
	PM_VREG_LP_GP4_ID,
	PM_VREG_LP_GP1_ID,
	PM_VREG_LP_RFTX_ID,
	PM_VREG_LP_RFRX1_ID,
	PM_VREG_LP_RFRX2_ID,
	PM_VREG_LP_WLAN_ID,
	PM_VREG_LP_MMC_ID,
	PM_VREG_LP_RUIM_ID,
	PM_VREG_LP_MSMC0_ID,
	PM_VREG_LP_GP2_ID,
	PM_VREG_LP_GP5_ID,
	PM_VREG_LP_GP6_ID,
	PM_VREG_LP_MPLL_ID,
	PM_VREG_LP_RFUBM_ID,
	PM_VREG_LP_RFA_ID,
	PM_VREG_LP_CDC2_ID,
	PM_VREG_LP_RFTX2_ID,
	PM_VREG_LP_USIM_ID,
	PM_VREG_LP_USB2P6_ID,
	PM_VREG_LP_TCXO_ID,
	PM_VREG_LP_USB3P3_ID,

	PM_VREG_LP_MSME_ID = PM_VREG_LP_MSME1_ID,
	/* backward compatible enums only */
	PM_VREG_LP_CAM_ID = PM_VREG_LP_GP1_ID,
	PM_VREG_LP_MDDI_ID = PM_VREG_LP_GP2_ID,
	PM_VREG_LP_RUIM2_ID = PM_VREG_LP_GP3_ID,
	PM_VREG_LP_AUX_ID = PM_VREG_LP_GP4_ID,
	PM_VREG_LP_AUX2_ID = PM_VREG_LP_GP5_ID,
	PM_VREG_LP_BT_ID = PM_VREG_LP_GP6_ID,
	PM_VREG_LP_MSMC_LDO_ID = PM_VREG_LP_MSMC_ID,
	PM_VREG_LP_MSME1_LDO_ID = PM_VREG_LP_MSME1_ID,
	PM_VREG_LP_MSME2_LDO_ID = PM_VREG_LP_MSME2_ID,
	PM_VREG_LP_RFA1_ID = PM_VREG_LP_RFRX2_ID,
	PM_VREG_LP_RFA2_ID = PM_VREG_LP_RFTX2_ID,
	PM_VREG_LP_XO_ID = PM_VREG_LP_TCXO_ID
};

enum mpp_which {
	PM_MPP_1,
	PM_MPP_2,
	PM_MPP_3,
	PM_MPP_4,
	PM_MPP_5,
	PM_MPP_6,
	PM_MPP_7,
	PM_MPP_8,
	PM_MPP_9,
	PM_MPP_10,
	PM_MPP_11,
	PM_MPP_12,
	PM_MPP_13,
	PM_MPP_14,
	PM_MPP_15,
	PM_MPP_16,
	PM_MPP_17,
	PM_MPP_18,
	PM_MPP_19,
	PM_MPP_20,
	PM_MPP_21,
	PM_MPP_22,

	PM_NUM_MPP_HAN = PM_MPP_4 + 1,
	PM_NUM_MPP_KIP = PM_MPP_4 + 1,
	PM_NUM_MPP_EPIC = PM_MPP_4 + 1,
	PM_NUM_MPP_PM7500 = PM_MPP_22 + 1,
	PM_NUM_MPP_PM6650 = PM_MPP_12 + 1,
	PM_NUM_MPP_PM6658 = PM_MPP_12 + 1,
	PM_NUM_MPP_PANORAMIX = PM_MPP_2 + 1,
	PM_NUM_MPP_PM6640 = PM_NUM_MPP_PANORAMIX,
	PM_NUM_MPP_PM6620 = PM_NUM_MPP_PANORAMIX
};

enum mpp_dlogic_level {
	PM_MPP__DLOGIC__LVL_MSME,
	PM_MPP__DLOGIC__LVL_MSMP,
	PM_MPP__DLOGIC__LVL_RUIM,
	PM_MPP__DLOGIC__LVL_MMC,
	PM_MPP__DLOGIC__LVL_VDD,
};

enum mpp_dlogic_in_dbus {
	PM_MPP__DLOGIC_IN__DBUS_NONE,
	PM_MPP__DLOGIC_IN__DBUS1,
	PM_MPP__DLOGIC_IN__DBUS2,
	PM_MPP__DLOGIC_IN__DBUS3,
};

enum mpp_dlogic_out_ctrl {
	PM_MPP__DLOGIC_OUT__CTRL_LOW,
	PM_MPP__DLOGIC_OUT__CTRL_HIGH,
	PM_MPP__DLOGIC_OUT__CTRL_MPP,
	PM_MPP__DLOGIC_OUT__CTRL_NOT_MPP,
};

enum mpp_i_sink_level {
	PM_MPP__I_SINK__LEVEL_5mA,
	PM_MPP__I_SINK__LEVEL_10mA,
	PM_MPP__I_SINK__LEVEL_15mA,
	PM_MPP__I_SINK__LEVEL_20mA,
	PM_MPP__I_SINK__LEVEL_25mA,
	PM_MPP__I_SINK__LEVEL_30mA,
	PM_MPP__I_SINK__LEVEL_35mA,
	PM_MPP__I_SINK__LEVEL_40mA,
};

enum mpp_i_sink_switch {
	PM_MPP__I_SINK__SWITCH_DIS,
	PM_MPP__I_SINK__SWITCH_ENA,
	PM_MPP__I_SINK__SWITCH_ENA_IF_MPP_HIGH,
	PM_MPP__I_SINK__SWITCH_ENA_IF_MPP_LOW,
};

enum pm_vib_mot_mode {
	PM_VIB_MOT_MODE__MANUAL,
	PM_VIB_MOT_MODE__DBUS1,
	PM_VIB_MOT_MODE__DBUS2,
	PM_VIB_MOT_MODE__DBUS3,
};

enum pm_vib_mot_pol {
	PM_VIB_MOT_POL__ACTIVE_HIGH,
	PM_VIB_MOT_POL__ACTIVE_LOW,
};

struct rtc_time {
	uint  sec;
};

enum rtc_alarm {
	PM_RTC_ALARM_1,
};


int pmic_lp_mode_control(enum switch_cmd cmd, enum vreg_lp_id id);
int pmic_secure_mpp_control_digital_output(enum mpp_which which,
		enum mpp_dlogic_level level, enum mpp_dlogic_out_ctrl out);
int pmic_secure_mpp_config_i_sink(enum mpp_which which,
		enum mpp_i_sink_level level, enum mpp_i_sink_switch onoff);
int pmic_secure_mpp_config_digital_input(enum mpp_which	which,
		enum mpp_dlogic_level level, enum mpp_dlogic_in_dbus dbus);
int pmic_speaker_cmd(const enum spkr_cmd cmd);
int pmic_set_spkr_configuration(struct spkr_config_mode	*cfg);
int pmic_spkr_en_right_chan(uint enable);
int pmic_spkr_en_left_chan(uint enable);
int pmic_spkr_en(enum spkr_left_right left_right, uint enabled);
int pmic_spkr_set_gain(enum spkr_left_right left_right, enum spkr_gain gain);
int pmic_set_speaker_gain(enum spkr_gain gain);
int pmic_set_speaker_delay(enum spkr_dly delay);
int pmic_speaker_1k6_zin_enable(uint enable);
int pmic_spkr_set_mux_hpf_corner_freq(enum spkr_hpf_corner_freq	freq);
int pmic_spkr_select_usb_with_hpf_20hz(uint enable);
int pmic_spkr_bypass_mux(uint enable);
int pmic_spkr_en_hpf(uint enable);
int pmic_spkr_en_sink_curr_from_ref_volt_cir(uint enable);
int pmic_spkr_set_delay(enum spkr_left_right left_right, enum spkr_dly delay);
int pmic_spkr_en_mute(enum spkr_left_right left_right, uint enabled);
int pmic_mic_en(uint enable);
int pmic_mic_set_volt(enum mic_volt vol);
int pmic_set_led_intensity(enum ledtype type, int level);
int pmic_flash_led_set_current(uint16_t milliamps);
int pmic_flash_led_set_mode(enum flash_led_mode mode);
int pmic_flash_led_set_polarity(enum flash_led_pol pol);
int pmic_spkr_add_right_left_chan(uint enable);
int pmic_spkr_en_stereo(uint enable);
int pmic_vib_mot_set_volt(uint vol);
int pmic_vib_mot_set_mode(enum pm_vib_mot_mode mode);
int pmic_vib_mot_set_polarity(enum pm_vib_mot_pol pol);
int pmic_vid_en(uint enable);
int pmic_vid_load_detect_en(uint enable);

#endif
