/* Copyright (c) 2015-2018, The Linux Foundation. All rights reserved.
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

#ifndef _AQT1000_CLSH_H
#define _AQT1000_CLSH_H

#include <linux/module.h>
#include <linux/slab.h>
#include <sound/soc.h>
#include <linux/kernel.h>

#define CLSH_REQ_ENABLE true
#define CLSH_REQ_DISABLE false

#define AQT_CLSH_EVENT_PRE_DAC 0x01
#define AQT_CLSH_EVENT_POST_PA 0x02
/*
 * Basic states for Class H state machine.
 * represented as a bit mask within a u8 data type
 * bit 0: HPH Left mode
 * bit 1: HPH Right mode
 */
#define	AQT_CLSH_STATE_IDLE 0x00
#define	AQT_CLSH_STATE_HPHL (0x01 << 0)
#define	AQT_CLSH_STATE_HPHR (0x01 << 1)

/*
 * Though number of CLSH states are 2, max state shoulbe be 3
 * because state array index starts from 1.
 */
#define AQT_CLSH_STATE_MAX 3
#define NUM_CLSH_STATES (0x01 << AQT_CLSH_STATE_MAX)


/* Derived State: Bits 1 and 2 should be set for Headphone stereo */
#define AQT_CLSH_STATE_HPH_ST (AQT_CLSH_STATE_HPHL | \
			       AQT_CLSH_STATE_HPHR)

enum {
	CLS_H_NORMAL = 0, /* Class-H Default */
	CLS_H_HIFI, /* Class-H HiFi */
	CLS_H_LP, /* Class-H Low Power */
	CLS_AB, /* Class-AB Low HIFI*/
	CLS_H_LOHIFI, /* LoHIFI */
	CLS_H_ULP, /* Ultra Low power */
	CLS_AB_HIFI, /* Class-AB */
	CLS_NONE, /* None of the above modes */
};

enum {
	DAC_GAIN_0DB = 0,
	DAC_GAIN_0P2DB,
	DAC_GAIN_0P4DB,
	DAC_GAIN_0P6DB,
	DAC_GAIN_0P8DB,
	DAC_GAIN_M0P2DB,
	DAC_GAIN_M0P4DB,
	DAC_GAIN_M0P6DB,
};

enum {
	VREF_FILT_R_0OHM = 0,
	VREF_FILT_R_25KOHM,
	VREF_FILT_R_50KOHM,
	VREF_FILT_R_100KOHM,
};

enum {
	DELTA_I_0MA,
	DELTA_I_10MA,
	DELTA_I_20MA,
	DELTA_I_30MA,
	DELTA_I_40MA,
	DELTA_I_50MA,
};

struct aqt_imped_val {
	u32 imped_val;
	u8 index;
};

struct aqt_clsh_cdc_data {
	u8 state;
	int flyback_users;
	int buck_users;
	int clsh_users;
	int interpolator_modes[AQT_CLSH_STATE_MAX];
};

struct aqt_reg_mask_val {
	u16 reg;
	u8 mask;
	u8 val;
};

extern void aqt_clsh_fsm(struct snd_soc_codec *codec,
		struct aqt_clsh_cdc_data *cdc_clsh_d,
		u8 clsh_event, u8 req_state,
		int int_mode);

extern void aqt_clsh_init(struct aqt_clsh_cdc_data *clsh);
extern int aqt_clsh_get_clsh_state(struct aqt_clsh_cdc_data *clsh);
extern void aqt_clsh_imped_config(struct snd_soc_codec *codec, int imped,
		bool reset);

#endif /* _AQT1000_CLSH_H */
