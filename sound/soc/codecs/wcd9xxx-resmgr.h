/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#ifndef __WCD9XXX_COMMON_H__
#define __WCD9XXX_COMMON_H__

#include <linux/notifier.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>

enum wcd9xxx_bandgap_type {
	WCD9XXX_BANDGAP_OFF,
	WCD9XXX_BANDGAP_AUDIO_MODE,
	WCD9XXX_BANDGAP_MBHC_MODE,
};

enum wcd9xxx_clock_type {
	WCD9XXX_CLK_OFF,
	WCD9XXX_CLK_RCO,
	WCD9XXX_CLK_MCLK,
};

enum wcd9xxx_cfilt_sel {
	WCD9XXX_CFILT1_SEL,
	WCD9XXX_CFILT2_SEL,
	WCD9XXX_CFILT3_SEL,
	WCD9XXX_NUM_OF_CFILT,
};

struct wcd9xxx_reg_address {
	u16 micb_4_ctl;
	u16 micb_4_int_rbias;
	u16 micb_4_mbhc;
};

enum wcd9xxx_notify_event {
	WCD9XXX_EVENT_INVALID,

	WCD9XXX_EVENT_PRE_RCO_ON,
	WCD9XXX_EVENT_POST_RCO_ON,
	WCD9XXX_EVENT_PRE_RCO_OFF,
	WCD9XXX_EVENT_POST_RCO_OFF,

	WCD9XXX_EVENT_PRE_MCLK_ON,
	WCD9XXX_EVENT_POST_MCLK_ON,
	WCD9XXX_EVENT_PRE_MCLK_OFF,
	WCD9XXX_EVENT_POST_MCLK_OFF,

	WCD9XXX_EVENT_PRE_BG_OFF,
	WCD9XXX_EVENT_POST_BG_OFF,
	WCD9XXX_EVENT_PRE_BG_AUDIO_ON,
	WCD9XXX_EVENT_POST_BG_AUDIO_ON,
	WCD9XXX_EVENT_PRE_BG_MBHC_ON,
	WCD9XXX_EVENT_POST_BG_MBHC_ON,

	WCD9XXX_EVENT_PRE_MICBIAS_1_OFF,
	WCD9XXX_EVENT_POST_MICBIAS_1_OFF,
	WCD9XXX_EVENT_PRE_MICBIAS_2_OFF,
	WCD9XXX_EVENT_POST_MICBIAS_2_OFF,
	WCD9XXX_EVENT_PRE_MICBIAS_3_OFF,
	WCD9XXX_EVENT_POST_MICBIAS_3_OFF,
	WCD9XXX_EVENT_PRE_MICBIAS_4_OFF,
	WCD9XXX_EVENT_POST_MICBIAS_4_OFF,
	WCD9XXX_EVENT_PRE_MICBIAS_1_ON,
	WCD9XXX_EVENT_POST_MICBIAS_1_ON,
	WCD9XXX_EVENT_PRE_MICBIAS_2_ON,
	WCD9XXX_EVENT_POST_MICBIAS_2_ON,
	WCD9XXX_EVENT_PRE_MICBIAS_3_ON,
	WCD9XXX_EVENT_POST_MICBIAS_3_ON,
	WCD9XXX_EVENT_PRE_MICBIAS_4_ON,
	WCD9XXX_EVENT_POST_MICBIAS_4_ON,

	WCD9XXX_EVENT_PRE_CFILT_1_OFF,
	WCD9XXX_EVENT_POST_CFILT_1_OFF,
	WCD9XXX_EVENT_PRE_CFILT_2_OFF,
	WCD9XXX_EVENT_POST_CFILT_2_OFF,
	WCD9XXX_EVENT_PRE_CFILT_3_OFF,
	WCD9XXX_EVENT_POST_CFILT_3_OFF,
	WCD9XXX_EVENT_PRE_CFILT_1_ON,
	WCD9XXX_EVENT_POST_CFILT_1_ON,
	WCD9XXX_EVENT_PRE_CFILT_2_ON,
	WCD9XXX_EVENT_POST_CFILT_2_ON,
	WCD9XXX_EVENT_PRE_CFILT_3_ON,
	WCD9XXX_EVENT_POST_CFILT_3_ON,

	WCD9XXX_EVENT_PRE_HPHL_PA_ON,
	WCD9XXX_EVENT_POST_HPHL_PA_OFF,
	WCD9XXX_EVENT_PRE_HPHR_PA_ON,
	WCD9XXX_EVENT_POST_HPHR_PA_OFF,

	WCD9XXX_EVENT_POST_RESUME,

	WCD9XXX_EVENT_LAST,
};

struct wcd9xxx_resmgr {
	struct snd_soc_codec *codec;
	struct wcd9xxx *core;

	u32 rx_bias_count;

	enum wcd9xxx_bandgap_type bandgap_type;
	u16 bg_audio_users;
	u16 bg_mbhc_users;

	enum wcd9xxx_clock_type clk_type;
	u16 clk_rco_users;
	u16 clk_mclk_users;

	/* cfilt users per cfilts */
	u16 cfilt_users[WCD9XXX_NUM_OF_CFILT];

	struct wcd9xxx_reg_address *reg_addr;

	struct wcd9xxx_pdata *pdata;

	struct blocking_notifier_head notifier;
	/* Notifier needs mbhc pointer with resmgr */
	struct wcd9xxx_mbhc *mbhc;

	/*
	 * Currently, only used for mbhc purpose, to protect
	 * concurrent execution of mbhc threaded irq handlers and
	 * kill race between DAPM and MBHC. But can serve as a
	 * general lock to protect codec resource
	 */
	struct mutex codec_resource_lock;
};

int wcd9xxx_resmgr_init(struct wcd9xxx_resmgr *resmgr,
			struct snd_soc_codec *codec,
			struct wcd9xxx *wcd9xxx,
			struct wcd9xxx_pdata *pdata,
			struct wcd9xxx_reg_address *reg_addr);
void wcd9xxx_resmgr_deinit(struct wcd9xxx_resmgr *resmgr);

int wcd9xxx_resmgr_enable_config_mode(struct snd_soc_codec *codec, int enable);

void wcd9xxx_resmgr_enable_rx_bias(struct wcd9xxx_resmgr *resmgr, u32 enable);
void wcd9xxx_resmgr_get_clk_block(struct wcd9xxx_resmgr *resmgr,
				  enum wcd9xxx_clock_type type);
void wcd9xxx_resmgr_put_clk_block(struct wcd9xxx_resmgr *resmgr,
				  enum wcd9xxx_clock_type type);
void wcd9xxx_resmgr_get_bandgap(struct wcd9xxx_resmgr *resmgr,
				const enum wcd9xxx_bandgap_type choice);
void wcd9xxx_resmgr_put_bandgap(struct wcd9xxx_resmgr *resmgr,
				enum wcd9xxx_bandgap_type choice);
void wcd9xxx_resmgr_cfilt_get(struct wcd9xxx_resmgr *resmgr,
			      enum wcd9xxx_cfilt_sel cfilt_sel);
void wcd9xxx_resmgr_cfilt_put(struct wcd9xxx_resmgr *resmgr,
			      enum wcd9xxx_cfilt_sel cfilt_sel);

void wcd9xxx_resmgr_bcl_lock(struct wcd9xxx_resmgr *resmgr);
#define WCD9XXX_BCL_LOCK(resmgr)			\
{							\
	pr_debug("%s: Acquiring BCL\n", __func__);	\
	wcd9xxx_resmgr_bcl_lock(resmgr);			\
	pr_debug("%s: Acquiring BCL done\n", __func__);	\
}

void wcd9xxx_resmgr_bcl_unlock(struct wcd9xxx_resmgr *resmgr);
#define WCD9XXX_BCL_UNLOCK(resmgr)			\
{							\
	pr_debug("%s: Release BCL\n", __func__);	\
	wcd9xxx_resmgr_bcl_unlock(resmgr);			\
}

#define WCD9XXX_BCL_ASSERT_LOCKED(resmgr)		\
{							\
	WARN_ONCE(!mutex_is_locked(&resmgr->codec_resource_lock), \
		  "%s: BCL should have acquired\n", __func__); \
}

const char *wcd9xxx_get_event_string(enum wcd9xxx_notify_event type);
int wcd9xxx_resmgr_get_k_val(struct wcd9xxx_resmgr *resmgr,
			     unsigned int cfilt_mv);
int wcd9xxx_resmgr_register_notifier(struct wcd9xxx_resmgr *resmgr,
				     struct notifier_block *nblock);
int wcd9xxx_resmgr_unregister_notifier(struct wcd9xxx_resmgr *resmgr,
				       struct notifier_block *nblock);
void wcd9xxx_resmgr_notifier_call(struct wcd9xxx_resmgr *resmgr,
				  const enum wcd9xxx_notify_event e);

#endif /* __WCD9XXX_COMMON_H__ */
