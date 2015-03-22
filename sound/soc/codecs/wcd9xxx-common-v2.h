/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#ifndef _WCD9XXX_COMMON_V2

#define _WCD9XXX_COMMON_V2

#define CLSH_REQ_ENABLE true
#define CLSH_REQ_DISABLE false

#define WCD_CLSH_EVENT_PRE_DAC 0x01
#define WCD_CLSH_EVENT_POST_PA 0x02

/*
 * Basic states for Class H state machine.
 * represented as a bit mask within a u8 data type
 * bit 0: EAR mode
 * bit 1: HPH Left mode
 * bit 2: HPH Right mode
 * bit 3: Lineout mode
 */
#define	WCD_CLSH_STATE_IDLE 0x00
#define	WCD_CLSH_STATE_EAR (0x01 << 0)
#define	WCD_CLSH_STATE_HPHL (0x01 << 1)
#define	WCD_CLSH_STATE_HPHR (0x01 << 2)
#define	WCD_CLSH_STATE_LO (0x01 << 3)
#define WCD_CLSH_STATE_MAX 4
#define NUM_CLSH_STATES (0x01 << WCD_CLSH_STATE_MAX)


/* Derived State: Bits 1 and 2 should be set for Headphone stereo */
#define WCD_CLSH_STATE_HPH_ST (WCD_CLSH_STATE_HPHL | \
			       WCD_CLSH_STATE_HPHR)

#define WCD_CLSH_STATE_HPHL_LO (WCD_CLSH_STATE_HPHL | \
				    WCD_CLSH_STATE_LO)
#define WCD_CLSH_STATE_HPHR_LO (WCD_CLSH_STATE_HPHR | \
				    WCD_CLSH_STATE_LO)
#define WCD_CLSH_STATE_HPH_ST_LO (WCD_CLSH_STATE_HPH_ST | \
				      WCD_CLSH_STATE_LO)
#define WCD_CLSH_STATE_EAR_LO (WCD_CLSH_STATE_EAR | \
				   WCD_CLSH_STATE_LO)
#define WCD_CLSH_STATE_HPHL_EAR (WCD_CLSH_STATE_HPHL | \
				     WCD_CLSH_STATE_EAR)
#define WCD_CLSH_STATE_HPHR_EAR (WCD_CLSH_STATE_HPHR | \
				     WCD_CLSH_STATE_EAR)
#define WCD_CLSH_STATE_HPH_ST_EAR (WCD_CLSH_STATE_HPH_ST | \
				       WCD_CLSH_STATE_EAR)

enum {
	CLS_H_NORMAL = 0, /* Class-H Default */
	CLS_H_HIFI, /* Class-H HiFi */
	CLS_H_LP, /* Class-H Low Power */
	CLS_AB, /* Class-AB */
	CLS_NONE, /* None of the above modes */
};

/* Class H data that the codec driver will maintain */
struct wcd_clsh_cdc_data {
	u8 state;
	int flyback_users;
	int buck_users;
	int clsh_users;
	int interpolator_modes[WCD_CLSH_STATE_MAX];
};

extern void wcd_clsh_fsm(struct snd_soc_codec *codec,
		struct wcd_clsh_cdc_data *cdc_clsh_d,
		u8 clsh_event, u8 req_state,
		int int_mode);

extern void wcd_clsh_init(struct wcd_clsh_cdc_data *clsh);

#endif
