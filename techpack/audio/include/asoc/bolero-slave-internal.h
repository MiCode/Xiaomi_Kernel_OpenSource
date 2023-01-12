/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

/* from Slave to bolero events */
enum {
	SLV_BOLERO_EVT_RX_MUTE = 1, /* for RX mute/unmute */
	SLV_BOLERO_EVT_IMPED_TRUE,   /* for imped true */
	SLV_BOLERO_EVT_IMPED_FALSE,  /* for imped false */
	SLV_BOLERO_EVT_RX_COMPANDER_SOFT_RST,
	SLV_BOLERO_EVT_BCS_CLK_OFF,
	SLV_BOLERO_EVT_RX_PA_GAIN_UPDATE,
	SLV_BOLERO_EVT_HPHL_HD2_ENABLE, /* to enable hd2 config for hphl */
	SLV_BOLERO_EVT_HPHR_HD2_ENABLE, /* to enable hd2 config for hphr */
};

/* from bolero to SLV events */
enum {
	BOLERO_SLV_EVT_TX_CH_HOLD_CLEAR = 1,
	BOLERO_SLV_EVT_PA_OFF_PRE_SSR,
	BOLERO_SLV_EVT_SSR_DOWN,
	BOLERO_SLV_EVT_SSR_UP,
	BOLERO_SLV_EVT_PA_ON_POST_FSCLK,
	BOLERO_SLV_EVT_PA_ON_POST_FSCLK_ADIE_LB,
	BOLERO_SLV_EVT_CLK_NOTIFY,
};
