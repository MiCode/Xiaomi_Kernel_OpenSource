/*
 * Platform data for MAX98090
 *
 * Copyright 2011-2012 Maxim Integrated Products
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __SOUND_MAX98090_PDATA_H__
#define __SOUND_MAX98090_PDATA_H__

/* Equalizer filter response configuration
 * There are 5 coefs per band, 3 bytes per coef, and up to 7 bands
 */
struct max98090_eq_cfg {
	const char *name;
	unsigned int rate;
	unsigned int bands;
	u8 coef[5 * 3 * 7];
};

/* Biquad filter response configuration
 * There are 5 coefs per band, 3 bytes per coef
 */
struct max98090_biquad_cfg {
	const char *name;
	unsigned int rate;
	u8 coef[5 * 3];
};

/* codec platform data */
struct max98090_pdata {

	int irq;

	/* Equalizers for DAC */
	struct max98090_eq_cfg *eq_cfg;
	unsigned int eq_cfgcnt;

	/* Biquad filter for ADC */
	struct max98090_biquad_cfg *bq_cfg;
	unsigned int bq_cfgcnt;

	/* DMIC34 Biquad filter for ADC */
	struct max98090_biquad_cfg *dmic34bq_cfg;
	unsigned int dmic34bq_cfgcnt;

	/* Analog/digital microphone configuration:
	 * 0 = analog microphone input (normal setting)
	 * 1 = digital microphone input
	 */
	unsigned int digmic_left_mode:1;
	unsigned int digmic_right_mode:1;
	unsigned int digmic_3_mode:1;
	unsigned int digmic_4_mode:1;
};

#endif
