/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _WCD_CLSH
#define _WCD_CLSH

#include <linux/stringify.h>

#define WCD_CLSH_STRINGIFY(s)  __stringify(s)
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
 * bit 3: AUX mode
 */
#define	WCD_CLSH_STATE_IDLE 0x00
#define	WCD_CLSH_STATE_EAR (0x01 << 0)
#define	WCD_CLSH_STATE_HPHL (0x01 << 1)
#define	WCD_CLSH_STATE_HPHR (0x01 << 2)
#define	WCD_CLSH_STATE_AUX (0x01 << 3)

/*
 * Though number of CLSH states is 4, max state should be 5
 * because state array index starts from 1.
 */
#define WCD_CLSH_STATE_MAX 5
#define NUM_CLSH_STATES (0x01 << WCD_CLSH_STATE_MAX)

/* Derived State: Bits 1 and 2 should be set for Headphone stereo */
#define WCD_CLSH_STATE_HPH_ST (WCD_CLSH_STATE_HPHL | \
			       WCD_CLSH_STATE_HPHR)

#define WCD_CLSH_STATE_HPHL_AUX (WCD_CLSH_STATE_HPHL | \
				    WCD_CLSH_STATE_AUX)
#define WCD_CLSH_STATE_HPHR_AUX (WCD_CLSH_STATE_HPHR | \
				    WCD_CLSH_STATE_AUX)
#define WCD_CLSH_STATE_HPH_ST_AUX (WCD_CLSH_STATE_HPH_ST | \
				      WCD_CLSH_STATE_AUX)
#define WCD_CLSH_STATE_EAR_AUX (WCD_CLSH_STATE_EAR | \
				   WCD_CLSH_STATE_AUX)
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
	CLS_AB, /* Class-AB Low HIFI*/
	CLS_H_LOHIFI, /* LoHIFI */
	CLS_H_ULP, /* Ultra Low power */
	CLS_AB_HIFI, /* Class-AB */
	CLS_AB_LP, /* Class-AB Low Power */
	CLS_AB_LOHIFI, /* Class-AB Low HIFI */
	CLS_NONE, /* None of the above modes */
};

/* Class H data that the codec driver will maintain */
struct wcd_clsh_cdc_info {
	u8 state;
	int flyback_users;
	int buck_users;
	int interpolator_modes[WCD_CLSH_STATE_MAX];
};

#ifdef CONFIG_SND_SOC_WCD9XXX_V2
extern void wcd_cls_h_fsm(struct snd_soc_component *component,
		struct wcd_clsh_cdc_info *cdc_clsh_d,
		u8 clsh_event, u8 req_state,
		int int_mode);

extern void wcd_cls_h_init(struct wcd_clsh_cdc_info *clsh);
extern void wcd_clsh_set_hph_mode(struct snd_soc_component *component,
				  int mode);
#else
static inline void wcd_cls_h_fsm(struct snd_soc_component *component,
		struct wcd_clsh_cdc_info *cdc_clsh_d,
		u8 clsh_event, u8 req_state,
		int int_mode)
{
}

static inline extern void wcd_cls_h_init(struct wcd_clsh_cdc_info *clsh)
{
}

static inline extern void wcd_clsh_set_hph_mode(struct snd_soc_component *component,
				  int mode)
{
}
#endif /* CONFIG_SND_SOC_WCD9XXX_V2 */

#endif
