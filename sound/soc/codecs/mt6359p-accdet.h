/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 * Author: Argus Lin <argus.lin@mediatek.com>
 */

#ifndef _ACCDET_H_
#define _ACCDET_H_

#include <linux/ctype.h>
#include <linux/string.h>

#define ACCDET_DEVNAME "accdet"
/* SW ENV define */
#define NO_KEY			(0x0)
#define UP_KEY			(0x01)
#define MD_KEY			(0x02)
#define DW_KEY			(0x04)
#define AS_KEY			(0x08)

#define HEADSET_MODE_1		(1)
#define HEADSET_MODE_2		(2)
#define HEADSET_MODE_6		(6)

/* IOCTL */
#define ACCDET_IOC_MAGIC 'A'
#define ACCDET_INIT _IO(ACCDET_IOC_MAGIC, 0)
#define SET_CALL_STATE _IO(ACCDET_IOC_MAGIC, 1)
#define GET_BUTTON_STATUS _IO(ACCDET_IOC_MAGIC, 2)

/* 400us, Accdet irq clear timeout  */
#define ACCDET_TIME_OUT		0x61A80

/* cable type recognized by accdet, and report to WiredAccessoryManager */
enum accdet_report_state {
	NO_DEVICE =		0,
	HEADSET_MIC =		1,
	HEADSET_NO_MIC =	2,
	HEADSET_FIVE_POLE =	3,
	LINE_OUT_DEVICE =	4,
};

/* accdet status got from accdet FSM  */
enum accdet_status {
	PLUG_OUT =		0,
	MIC_BIAS =		1,
	HOOK_SWITCH =		2,
	BI_MIC_BIAS =		3,
	LINE_OUT =		4,
	STAND_BY =		5
};

enum accdet_eint_ID {
	NO_PMIC_EINT =		0,
	PMIC_EINT0 =		1,
	PMIC_EINT1 =		2,
	PMIC_BIEINT =		3,
};

/* EINT state when moisture enable  */
enum eint_moisture_status {
	M_PLUG_IN =		0,
	M_WATER_IN =		1,
	M_HP_PLUG_IN =		2,
	M_PLUG_OUT =		3,
	M_NO_ACT =		4,
	M_UNKNOWN =		5,
};

struct three_key_threshold {
	unsigned int mid;
	unsigned int up;
	unsigned int down;
};
struct four_key_threshold {
	unsigned int mid;
	unsigned int voice;
	unsigned int up;
	unsigned int down;
};

struct pwm_deb_settings {
	unsigned int pwm_width;
	unsigned int pwm_thresh;
	unsigned int fall_delay;
	unsigned int rise_delay;
	unsigned int debounce0;
	unsigned int debounce1;
	unsigned int debounce3;
	unsigned int debounce4;
	unsigned int eint_pwm_width;
	unsigned int eint_pwm_thresh;
	unsigned int eint_debounce0;
	unsigned int eint_debounce1;
	unsigned int eint_debounce2;
	unsigned int eint_debounce3;
	unsigned int eint_inverter_debounce;

};

struct head_dts_data {
	unsigned int mic_vol;
	unsigned int mic_mode;
	unsigned int plugout_deb;
	unsigned int eint_pol;
	struct pwm_deb_settings pwm_deb;
	struct three_key_threshold three_key;
	struct four_key_threshold four_key;
	unsigned int moisture_detect_enable;
	unsigned int eint_detect_mode;
	unsigned int eint_use_ext_res;
	unsigned int moisture_detect_mode;
	unsigned int moisture_comp_vth;
	unsigned int moisture_comp_vref2;
	unsigned int moisture_use_ext_res;
};

enum {
	accdet_state000 = 0,
	accdet_state001,
	accdet_state010,
	accdet_state011,
	accdet_auxadc,
	eint_state000,
	eint_state001,
	eint_state010,
	eint_state011,
	eint_inverter_state000,
};

/* just be called by audio module for DC trim */
extern void accdet_late_init(unsigned long data);
extern const struct file_operations *accdet_get_fops(void);
extern void mt_accdet_suspend(void);
extern void mt_accdet_resume(void);
extern void accdet_set_debounce(int state, unsigned int debounce);
extern void accdet_modify_vref_volt(void);
extern int mt6359p_accdet_init(struct snd_soc_component *component,
			struct snd_soc_card *card);
#endif
