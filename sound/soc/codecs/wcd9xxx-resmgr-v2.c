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
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mfd/wcd9335/registers.h>
#include <sound/soc.h>
#include "wcd9xxx-resmgr-v2.h"

#define WCD9XXX_RCO_CALIBRATION_DELAY_INC_US 5000

static const char *wcd_resmgr_clk_type_to_str(enum wcd_clock_type clk_type)
{
	if (clk_type == WCD_CLK_OFF)
		return "WCD_CLK_OFF";
	else if (clk_type == WCD_CLK_RCO)
		return "WCD_CLK_RCO";
	else if (clk_type == WCD_CLK_MCLK)
		return "WCD_CLK_MCLK";
	else
		return "WCD_CLK_UNDEFINED";
}

static int wcd_resmgr_codec_reg_update_bits(struct wcd9xxx_resmgr_v2 *resmgr,
					    u16 reg, u8 mask, u8 val)
{
	int change;
	if (resmgr->codec)
		change = snd_soc_update_bits(resmgr->codec, reg, mask, val);
	else
		change = wcd9xxx_reg_update_bits(resmgr->core_res, reg,
						 mask, val);

	return change;
}

static int wcd_resmgr_codec_reg_read(struct wcd9xxx_resmgr_v2 *resmgr,
				     unsigned int reg)
{
	int val;

	if (resmgr->codec)
		val = snd_soc_read(resmgr->codec, reg);
	else
		val = wcd9xxx_reg_read(resmgr->core_res, reg);

	return val;
}

/*
 * wcd_resmgr_get_clk_type()
 * Returns clk type that is currently enabled
 */
int wcd_resmgr_get_clk_type(struct wcd9xxx_resmgr_v2 *resmgr)
{
	if (!resmgr) {
		pr_err("%s: resmgr not initialized\n", __func__);
		return -EINVAL;
	}
	return resmgr->clk_type;
}

static void wcd_resmgr_cdc_specific_get_clk(struct wcd9xxx_resmgr_v2 *resmgr,
						int clk_users)
{
	/* Caller of this function should have acquired BG_CLK lock */
	WCD9XXX_V2_BG_CLK_UNLOCK(resmgr);
	if (clk_users) {
		if (resmgr->resmgr_cb &&
		    resmgr->resmgr_cb->cdc_rco_ctrl) {
			while (clk_users--)
				resmgr->resmgr_cb->cdc_rco_ctrl(resmgr->codec,
								true);
		}
	}
	/* Acquire BG_CLK lock before return */
	WCD9XXX_V2_BG_CLK_LOCK(resmgr);
}

void wcd_resmgr_post_ssr_v2(struct wcd9xxx_resmgr_v2 *resmgr)
{
	int old_bg_audio_users;
	int old_clk_rco_users, old_clk_mclk_users;

	WCD9XXX_V2_BG_CLK_LOCK(resmgr);

	old_bg_audio_users = resmgr->master_bias_users;
	old_clk_mclk_users = resmgr->clk_mclk_users;
	old_clk_rco_users = resmgr->clk_rco_users;
	resmgr->master_bias_users = 0;
	resmgr->clk_mclk_users = 0;
	resmgr->clk_rco_users = 0;
	resmgr->clk_type = WCD_CLK_OFF;

	pr_debug("%s: old_bg_audio_users=%d old_clk_mclk_users=%d old_clk_rco_users=%d\n",
		 __func__, old_bg_audio_users,
		 old_clk_mclk_users, old_clk_rco_users);

	if (old_bg_audio_users) {
		while (old_bg_audio_users--)
			wcd_resmgr_enable_master_bias(resmgr);
	}

	if (old_clk_mclk_users) {
		while (old_clk_mclk_users--)
			wcd_resmgr_enable_clk_block(resmgr, WCD_CLK_MCLK);
	}

	if (old_clk_rco_users)
		wcd_resmgr_cdc_specific_get_clk(resmgr, old_clk_rco_users);

	WCD9XXX_V2_BG_CLK_UNLOCK(resmgr);
}


/*
 * wcd_resmgr_enable_master_bias: enable codec master bias
 * @resmgr: handle to struct wcd9xxx_resmgr_v2
 */
int wcd_resmgr_enable_master_bias(struct wcd9xxx_resmgr_v2 *resmgr)
{
	mutex_lock(&resmgr->master_bias_lock);

	resmgr->master_bias_users++;
	if (resmgr->master_bias_users == 1) {
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_BIAS,
						 0x80, 0x80);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_BIAS,
						 0x40, 0x40);
		/*
		 * 1ms delay is required after pre-charge is enabled
		 * as per HW requirement
		 */
		usleep_range(1000, 1100);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_BIAS,
						 0x40, 0x00);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_BIAS,
						 0x20, 0x00);
	}

	pr_debug("%s: current master bias users: %d\n", __func__,
		 resmgr->master_bias_users);

	mutex_unlock(&resmgr->master_bias_lock);
	return 0;
}

/*
 * wcd_resmgr_disable_master_bias: disable codec master bias
 * @resmgr: handle to struct wcd9xxx_resmgr_v2
 */
int wcd_resmgr_disable_master_bias(struct wcd9xxx_resmgr_v2 *resmgr)
{
	mutex_lock(&resmgr->master_bias_lock);
	if (resmgr->master_bias_users <= 0) {
		mutex_unlock(&resmgr->master_bias_lock);
		return -EINVAL;
	}

	resmgr->master_bias_users--;
	if (resmgr->master_bias_users == 0) {
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_BIAS,
						 0x80, 0x00);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_BIAS,
						 0x20, 0x00);
	}
	mutex_unlock(&resmgr->master_bias_lock);
	return 0;
}

static int wcd_resmgr_enable_clk_mclk(struct wcd9xxx_resmgr_v2 *resmgr)
{
	/* Enable mclk requires master bias to be enabled first */
	if (resmgr->master_bias_users <= 0) {
		pr_err("%s: Cannot turn on MCLK, BG is not enabled\n",
			__func__);
		return -EINVAL;
	}

	if (((resmgr->clk_mclk_users == 0) &&
	     (resmgr->clk_type == WCD_CLK_MCLK)) ||
	    ((resmgr->clk_mclk_users > 0) &&
	    (resmgr->clk_type != WCD_CLK_MCLK))) {
		pr_err("%s: Error enabling MCLK, clk_type: %s\n",
			__func__,
			wcd_resmgr_clk_type_to_str(resmgr->clk_type));
		return -EINVAL;
	}

	if (++resmgr->clk_mclk_users == 1) {

		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_CLK_TOP,
						 0x80, 0x80);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_CLK_TOP,
						 0x08, 0x00);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_CLK_TOP,
						 0x04, 0x04);
		wcd_resmgr_codec_reg_update_bits(resmgr,
				WCD9335_CDC_CLK_RST_CTRL_FS_CNT_CONTROL,
				0x01, 0x01);
		wcd_resmgr_codec_reg_update_bits(resmgr,
					WCD9335_CDC_CLK_RST_CTRL_MCLK_CONTROL,
					0x01, 0x01);
		/*
		 * 10us sleep is required after clock is enabled
		 * as per HW requirement
		 */
		usleep_range(10, 15);
	}

	resmgr->clk_type = WCD_CLK_MCLK;

	pr_debug("%s: mclk_users: %d, clk_type: %s\n", __func__,
		 resmgr->clk_mclk_users,
		 wcd_resmgr_clk_type_to_str(resmgr->clk_type));

	return 0;
}

static int wcd_resmgr_disable_clk_mclk(struct wcd9xxx_resmgr_v2 *resmgr)
{
	if (resmgr->clk_mclk_users <= 0) {
		pr_err("%s: No mclk users, cannot disable mclk\n", __func__);
		return -EINVAL;
	}

	if (--resmgr->clk_mclk_users == 0) {
		if (resmgr->clk_rco_users > 0) {
			/* MCLK to RCO switch */
			wcd_resmgr_codec_reg_update_bits(resmgr,
					WCD9335_ANA_CLK_TOP,
					0x08, 0x08);
			resmgr->clk_type = WCD_CLK_RCO;
		} else {
			wcd_resmgr_codec_reg_update_bits(resmgr,
					WCD9335_ANA_CLK_TOP,
					0x04, 0x00);
			resmgr->clk_type = WCD_CLK_OFF;
		}

		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_CLK_TOP,
						 0x80, 0x00);
	}

	pr_debug("%s: mclk_users: %d, clk_type: %s\n", __func__,
		 resmgr->clk_mclk_users,
		 wcd_resmgr_clk_type_to_str(resmgr->clk_type));

	return 0;
}

static int wcd_resmgr_enable_clk_rco(struct wcd9xxx_resmgr_v2 *resmgr)
{
	bool rco_cal_done = true;

	resmgr->clk_rco_users++;
	if ((resmgr->clk_rco_users == 1) &&
	    ((resmgr->clk_type == WCD_CLK_OFF) ||
	     (resmgr->clk_mclk_users == 0))) {
		pr_warn("%s: RCO enable requires MCLK to be ON first\n",
			__func__);
		resmgr->clk_rco_users--;
		return -EINVAL;
	} else if ((resmgr->clk_rco_users == 1) &&
		   (resmgr->clk_mclk_users)) {
		/* RCO Enable */
		if (resmgr->sido_input_src == SIDO_SOURCE_INTERNAL)
			wcd_resmgr_codec_reg_update_bits(resmgr,
							 WCD9335_ANA_RCO,
							 0x80, 0x80);
		/*
		 * 20us required after RCO BG is enabled as per HW
		 * requirements
		 */
		usleep_range(20, 25);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_RCO,
						 0x40, 0x40);
		/*
		 * 20us required after RCO is enabled as per HW
		 * requirements
		 */
		usleep_range(20, 25);
		/* RCO Calibration */
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_RCO,
						 0x04, 0x04);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_RCO,
						 0x04, 0x00);

		/* RCO calibration takes app. 5ms to complete */
		usleep_range(WCD9XXX_RCO_CALIBRATION_DELAY_INC_US,
		       WCD9XXX_RCO_CALIBRATION_DELAY_INC_US + 100);
		if (wcd_resmgr_codec_reg_read(resmgr, WCD9335_ANA_RCO) & 0x02)
			rco_cal_done = false;

		WARN((!rco_cal_done), "RCO Calibration failed\n");

		/* Switch MUX to RCO */
		if (resmgr->clk_mclk_users == 1) {
			wcd_resmgr_codec_reg_update_bits(resmgr,
							WCD9335_ANA_CLK_TOP,
							0x08, 0x08);
			resmgr->clk_type = WCD_CLK_RCO;
		}
	}
	pr_debug("%s: rco clk users: %d, clk_type: %s\n", __func__,
		 resmgr->clk_rco_users,
		 wcd_resmgr_clk_type_to_str(resmgr->clk_type));

	return 0;
}

static int wcd_resmgr_disable_clk_rco(struct wcd9xxx_resmgr_v2 *resmgr)
{
	if ((resmgr->clk_rco_users <= 0) ||
	    (resmgr->clk_type == WCD_CLK_OFF)) {
		pr_err("%s: No RCO Clk users, cannot disable\n", __func__);
		return -EINVAL;
	}

	resmgr->clk_rco_users--;

	if ((resmgr->clk_rco_users == 0) &&
	    (resmgr->clk_type == WCD_CLK_RCO)) {
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_CLK_TOP,
						 0x08, 0x00);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_CLK_TOP,
						 0x04, 0x00);
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_RCO,
						 0x40, 0x00);
		if (resmgr->sido_input_src == SIDO_SOURCE_INTERNAL)
			wcd_resmgr_codec_reg_update_bits(resmgr,
							 WCD9335_ANA_RCO,
							 0x80, 0x00);
		resmgr->clk_type = WCD_CLK_OFF;
	} else if ((resmgr->clk_rco_users == 0) &&
	      (resmgr->clk_mclk_users)) {
		/* Disable RCO while MCLK is ON */
		wcd_resmgr_codec_reg_update_bits(resmgr, WCD9335_ANA_RCO,
						 0x40, 0x00);
		if (resmgr->sido_input_src == SIDO_SOURCE_INTERNAL)
			wcd_resmgr_codec_reg_update_bits(resmgr,
							 WCD9335_ANA_RCO,
							 0x80, 0x00);
	}
	pr_debug("%s: rco clk users: %d, clk_type: %s\n", __func__,
		 resmgr->clk_rco_users,
		 wcd_resmgr_clk_type_to_str(resmgr->clk_type));

	return 0;
}

/*
 * wcd_resmgr_enable_clk_block: enable MCLK or RCO
 * @resmgr: handle to struct wcd9xxx_resmgr_v2
 * @type: Clock type to enable
 */
int wcd_resmgr_enable_clk_block(struct wcd9xxx_resmgr_v2 *resmgr,
				enum wcd_clock_type type)
{
	int ret;

	switch (type) {
	case WCD_CLK_MCLK:
		ret = wcd_resmgr_enable_clk_mclk(resmgr);
		break;
	case WCD_CLK_RCO:
		ret = wcd_resmgr_enable_clk_rco(resmgr);
		break;
	default:
		pr_err("%s: Unknown Clock type: %s\n", __func__,
			wcd_resmgr_clk_type_to_str(type));
		ret = -EINVAL;
		break;
	};

	if (ret)
		pr_err("%s: Enable clock %s failed\n", __func__,
			wcd_resmgr_clk_type_to_str(type));

	return ret;
}

/*
 * wcd_resmgr_disable_clk_block: disable MCLK or RCO
 * @resmgr: handle to struct wcd9xxx_resmgr_v2
 * @type: Clock type to disable
 */
int wcd_resmgr_disable_clk_block(struct wcd9xxx_resmgr_v2 *resmgr,
				enum wcd_clock_type type)
{
	int ret;

	switch (type) {
	case WCD_CLK_MCLK:
		ret = wcd_resmgr_disable_clk_mclk(resmgr);
		break;
	case WCD_CLK_RCO:
		ret = wcd_resmgr_disable_clk_rco(resmgr);
		break;
	default:
		pr_err("%s: Unknown Clock type: %s\n", __func__,
			wcd_resmgr_clk_type_to_str(type));
		ret = -EINVAL;
		break;
	};

	if (ret)
		pr_err("%s: Disable clock %s failed\n", __func__,
			wcd_resmgr_clk_type_to_str(type));

	return ret;
}

/*
 * wcd_resmgr_init: initialize wcd resource manager
 * @core_res: handle to struct wcd9xxx_core_resource
 *
 * Early init call without a handle to snd_soc_codec *
 */
struct wcd9xxx_resmgr_v2 *wcd_resmgr_init(
		struct wcd9xxx_core_resource *core_res,
		struct snd_soc_codec *codec)
{
	struct wcd9xxx_resmgr_v2 *resmgr;

	resmgr = kzalloc(sizeof(struct wcd9xxx_resmgr_v2), GFP_KERNEL);
	if (!resmgr) {
		pr_err("%s: Cannot allocate memory for wcd resmgr\n", __func__);
		return ERR_PTR(-ENOMEM);
	}

	mutex_init(&resmgr->codec_bg_clk_lock);
	mutex_init(&resmgr->master_bias_lock);
	resmgr->master_bias_users = 0;
	resmgr->clk_mclk_users = 0;
	resmgr->clk_rco_users = 0;
	resmgr->master_bias_users = 0;
	resmgr->codec = codec;
	resmgr->core_res = core_res;
	resmgr->sido_input_src = SIDO_SOURCE_INTERNAL;

	return resmgr;
}

/*
 * wcd_resmgr_remove: Clean-up wcd resource manager
 * @resmgr: handle to struct wcd9xxx_resmgr_v2
 */
void wcd_resmgr_remove(struct wcd9xxx_resmgr_v2 *resmgr)
{
	mutex_destroy(&resmgr->master_bias_lock);
	kfree(resmgr);
}

/*
 * wcd_resmgr_post_init: post init call to assign codec handle
 * @resmgr: handle to struct wcd9xxx_resmgr_v2 created during early init
 * @resmgr_cb: codec callback function for resmgr
 * @codec: handle to struct snd_soc_codec
 */
int wcd_resmgr_post_init(struct wcd9xxx_resmgr_v2 *resmgr,
			 const struct wcd_resmgr_cb *resmgr_cb,
			 struct snd_soc_codec *codec)
{
	if (!resmgr) {
		pr_err("%s: resmgr not allocated\n", __func__);
		return -EINVAL;
	}

	if (!codec) {
		pr_err("%s: Codec memory is NULL, nothing to post init\n",
			__func__);
		return -EINVAL;
	}

	resmgr->codec = codec;

	return 0;
}
MODULE_DESCRIPTION("wcd9xxx resmgr v2 module");
MODULE_LICENSE("GPL v2");
