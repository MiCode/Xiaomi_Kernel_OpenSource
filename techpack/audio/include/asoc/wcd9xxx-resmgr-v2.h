/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2018, 2020 The Linux Foundation. All rights reserved.
 */
#ifndef __WCD9XXX_COMMON_V2_H__
#define __WCD9XXX_COMMON_V2_H__

#include <audio/linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include "core.h"

enum wcd_clock_type {
	WCD_CLK_OFF,
	WCD_CLK_RCO,
	WCD_CLK_MCLK,
};

enum {
	SIDO_SOURCE_INTERNAL,
	SIDO_SOURCE_RCO_BG,
};

struct wcd_resmgr_cb {
	int (*cdc_rco_ctrl)(struct snd_soc_component *component, bool enable);
};

struct wcd9xxx_resmgr_v2 {
	struct snd_soc_component *component;
	struct wcd9xxx_core_resource *core_res;

	int master_bias_users;
	int clk_mclk_users;
	int clk_rco_users;

	struct mutex codec_bg_clk_lock;
	struct mutex master_bias_lock;

	enum codec_variant codec_type;
	enum wcd_clock_type clk_type;

	const struct wcd_resmgr_cb *resmgr_cb;
	int sido_input_src;
};

#define WCD9XXX_V2_BG_CLK_LOCK(resmgr)			\
{							\
	struct wcd9xxx_resmgr_v2 *__resmgr = resmgr;	\
	pr_debug("%s: Acquiring BG_CLK\n", __func__);	\
	mutex_lock(&__resmgr->codec_bg_clk_lock);	\
	pr_debug("%s: Acquiring BG_CLK done\n", __func__);	\
}

#define WCD9XXX_V2_BG_CLK_UNLOCK(resmgr)			\
{							\
	struct wcd9xxx_resmgr_v2 *__resmgr = resmgr;	\
	pr_debug("%s: Releasing BG_CLK\n", __func__);	\
	mutex_unlock(&__resmgr->codec_bg_clk_lock);	\
}

#define WCD9XXX_V2_BG_CLK_ASSERT_LOCKED(resmgr)		\
{							\
	WARN_ONCE(!mutex_is_locked(&resmgr->codec_bg_clk_lock), \
		  "%s: BG_CLK lock should have acquired\n", __func__); \
}

int wcd_resmgr_enable_master_bias(struct wcd9xxx_resmgr_v2 *resmgr);
int wcd_resmgr_disable_master_bias(struct wcd9xxx_resmgr_v2 *resmgr);
struct wcd9xxx_resmgr_v2 *wcd_resmgr_init(
		struct wcd9xxx_core_resource *core_res,
		struct snd_soc_component *component);
void wcd_resmgr_remove(struct wcd9xxx_resmgr_v2 *resmgr);
int wcd_resmgr_post_init(struct wcd9xxx_resmgr_v2 *resmgr,
			 const struct wcd_resmgr_cb *resmgr_cb,
			 struct snd_soc_component *component);
int wcd_resmgr_enable_clk_block(struct wcd9xxx_resmgr_v2 *resmgr,
				enum wcd_clock_type type);
int wcd_resmgr_disable_clk_block(struct wcd9xxx_resmgr_v2 *resmgr,
				enum wcd_clock_type type);
int wcd_resmgr_get_clk_type(struct wcd9xxx_resmgr_v2 *resmgr);
void wcd_resmgr_post_ssr_v2(struct wcd9xxx_resmgr_v2 *resmgr);
void wcd_resmgr_set_sido_input_src_locked(struct wcd9xxx_resmgr_v2 *resmgr,
					  int sido_src);
void wcd_resmgr_set_sido_input_src(struct wcd9xxx_resmgr_v2 *resmgr,
					  int sido_src);

#endif
