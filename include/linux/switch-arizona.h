/*
 * extcon-arizona.h - Extcon driver Wolfson Arizona devices
 *
 *  Copyright (C) 2014 Wolfson Microelectronics plc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SWITCH_ARIZONA_H_
#define _SWITCH_ARIZONA_H_

#include <linux/switch.h>
#include <linux/mfd/arizona/registers.h>

#define ARIZONA_ACCDET_MODE_MIC     0
#define ARIZONA_ACCDET_MODE_HPL     1
#define ARIZONA_ACCDET_MODE_HPR     2
#define ARIZONA_ACCDET_MODE_HPM     4
#define ARIZONA_ACCDET_MODE_ADC     7
#define ARIZONA_ACCDET_MODE_INVALID 8

enum {
	MICD_LVL_1_TO_7 = ARIZONA_MICD_LVL_1 | ARIZONA_MICD_LVL_2 |
			  ARIZONA_MICD_LVL_3 | ARIZONA_MICD_LVL_4 |
			  ARIZONA_MICD_LVL_5 | ARIZONA_MICD_LVL_6 |
			  ARIZONA_MICD_LVL_7,

	MICD_LVL_0_TO_7 = ARIZONA_MICD_LVL_0 | MICD_LVL_1_TO_7,

	MICD_LVL_0_TO_8 = MICD_LVL_0_TO_7 | ARIZONA_MICD_LVL_8,
};

struct arizona_extcon_info;

struct arizona_jd_state {
	int mode;

	int (*start)(struct arizona_extcon_info *);
	void (*restart)(struct arizona_extcon_info *);
	int (*reading)(struct arizona_extcon_info *, int);
	void (*stop)(struct arizona_extcon_info *);

	int (*timeout_ms)(struct arizona_extcon_info *);
	void (*timeout)(struct arizona_extcon_info *);
};

int arizona_jds_set_state(struct arizona_extcon_info *info,
			  const struct arizona_jd_state *new_state);

extern const struct arizona_jd_state arizona_hpdet_left;
extern const struct arizona_jd_state arizona_micd_button;
extern const struct arizona_jd_state arizona_micd_microphone;

extern int arizona_hpdet_start(struct arizona_extcon_info *info);
extern void arizona_hpdet_restart(struct arizona_extcon_info *info);
extern void arizona_hpdet_stop(struct arizona_extcon_info *info);
extern int arizona_hpdet_reading(struct arizona_extcon_info *info, int val);

extern int arizona_micd_start(struct arizona_extcon_info *info);
extern void arizona_micd_stop(struct arizona_extcon_info *info);
extern int arizona_micd_button_reading(struct arizona_extcon_info *info,
				       int val);

extern int arizona_micd_mic_start(struct arizona_extcon_info *info);
extern void arizona_micd_mic_stop(struct arizona_extcon_info *info);
extern int arizona_micd_mic_reading(struct arizona_extcon_info *info, int val);
extern int arizona_micd_mic_timeout_ms(struct arizona_extcon_info *info);
extern void arizona_micd_mic_timeout(struct arizona_extcon_info *info);

#endif
