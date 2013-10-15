/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
#include <linux/firmware.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <linux/mfd/wcd9xxx/core.h>
#include <linux/mfd/wcd9xxx/core-resource.h>
#include <linux/mfd/wcd9xxx/wcd9xxx_registers.h>
#include <linux/mfd/wcd9xxx/wcd9320_registers.h>
#include <linux/mfd/wcd9xxx/pdata.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/gpio.h>
#include <linux/input.h>
#include "wcd9320.h"
#include "wcd9306.h"
#include "wcd9xxx-mbhc.h"
#include "wcd9xxx-resmgr.h"
#include "wcd9xxx-common.h"

#define WCD9XXX_JACK_MASK (SND_JACK_HEADSET | SND_JACK_OC_HPHL | \
			   SND_JACK_OC_HPHR | SND_JACK_LINEOUT | \
			   SND_JACK_UNSUPPORTED)
#define WCD9XXX_JACK_BUTTON_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
				  SND_JACK_BTN_2 | SND_JACK_BTN_3 | \
				  SND_JACK_BTN_4 | SND_JACK_BTN_5 | \
				  SND_JACK_BTN_6 | SND_JACK_BTN_7)

#define NUM_DCE_PLUG_DETECT 3
#define NUM_DCE_PLUG_INS_DETECT 5
#define NUM_ATTEMPTS_INSERT_DETECT 25
#define NUM_ATTEMPTS_TO_REPORT 5

#define FAKE_INS_LOW 10
#define FAKE_INS_HIGH 80
#define FAKE_INS_HIGH_NO_SWCH 150
#define FAKE_REMOVAL_MIN_PERIOD_MS 50
#define FAKE_INS_DELTA_SCALED_MV 300

#define BUTTON_MIN 0x8000
#define STATUS_REL_DETECTION 0x0C

#define HS_DETECT_PLUG_TIME_MS (5 * 1000)
#define HS_DETECT_PLUG_INERVAL_MS 100
#define SWCH_REL_DEBOUNCE_TIME_MS 50
#define SWCH_IRQ_DEBOUNCE_TIME_US 5000
#define BTN_RELEASE_DEBOUNCE_TIME_MS 25

#define GND_MIC_SWAP_THRESHOLD 2
#define OCP_ATTEMPT 1

#define FW_READ_ATTEMPTS 15
#define FW_READ_TIMEOUT 2000000

#define BUTTON_POLLING_SUPPORTED true

#define MCLK_RATE_12288KHZ 12288000
#define MCLK_RATE_9600KHZ 9600000

#define DEFAULT_DCE_STA_WAIT 55
#define DEFAULT_DCE_WAIT 60000
#define DEFAULT_STA_WAIT 5000

#define VDDIO_MICBIAS_MV 1800

#define WCD9XXX_MICBIAS_PULLDOWN_SETTLE_US 5000

#define WCD9XXX_HPHL_STATUS_READY_WAIT_US 1000
#define WCD9XXX_MUX_SWITCH_READY_WAIT_MS 50
#define WCD9XXX_MEAS_DELTA_MAX_MV 50
#define WCD9XXX_MEAS_INVALD_RANGE_LOW_MV 20
#define WCD9XXX_MEAS_INVALD_RANGE_HIGH_MV 80

/*
 * Invalid voltage range for the detection
 * of plug type with current source
 */
#define WCD9XXX_CS_MEAS_INVALD_RANGE_LOW_MV 110
#define WCD9XXX_CS_MEAS_INVALD_RANGE_HIGH_MV 265

/*
 * Threshold used to detect euro headset
 * with current source
 */
#define WCD9XXX_CS_GM_SWAP_THRES_MIN_MV 10
#define WCD9XXX_CS_GM_SWAP_THRES_MAX_MV 40

#define WCD9XXX_MBHC_NSC_CS 9
#define WCD9XXX_GM_SWAP_THRES_MIN_MV 150
#define WCD9XXX_GM_SWAP_THRES_MAX_MV 650
#define WCD9XXX_THRESHOLD_MIC_THRESHOLD 200

#define WCD9XXX_USLEEP_RANGE_MARGIN_US 100

/* RX_HPH_CNP_WG_TIME increases by 0.24ms */
#define WCD9XXX_WG_TIME_FACTOR_US	240

#define WCD9XXX_IRQ_MBHC_JACK_SWITCH_DEFAULT 28

#define WCD9XXX_V_CS_HS_MAX 500
#define WCD9XXX_V_CS_NO_MIC 5
#define WCD9XXX_MB_MEAS_DELTA_MAX_MV 80
#define WCD9XXX_CS_MEAS_DELTA_MAX_MV 10

static int impedance_detect_en;
module_param(impedance_detect_en, int,
			S_IRUGO | S_IWUSR | S_IWGRP);
MODULE_PARM_DESC(impedance_detect_en, "enable/disable impedance detect");

static bool detect_use_vddio_switch = true;

struct wcd9xxx_mbhc_detect {
	u16 dce;
	u16 sta;
	u16 hphl_status;
	bool swap_gnd;
	bool vddio;
	bool hwvalue;
	bool mic_bias;
	/* internal purpose from here */
	bool _above_no_mic;
	bool _below_v_hs_max;
	s16 _vdces;
	enum wcd9xxx_mbhc_plug_type _type;
};

enum meas_type {
	STA = 0,
	DCE,
};

enum {
	MBHC_USE_HPHL_TRIGGER = 1,
	MBHC_USE_MB_TRIGGER = 2
};

/*
 * Flags to track of PA and DAC state.
 * PA and DAC should be tracked separately as AUXPGA loopback requires
 * only PA to be turned on without DAC being on.
 */
enum pa_dac_ack_flags {
	WCD9XXX_HPHL_PA_OFF_ACK = 0,
	WCD9XXX_HPHR_PA_OFF_ACK,
	WCD9XXX_HPHL_DAC_OFF_ACK,
	WCD9XXX_HPHR_DAC_OFF_ACK
};

enum wcd9xxx_current_v_idx {
	WCD9XXX_CURRENT_V_INS_H,
	WCD9XXX_CURRENT_V_INS_HU,
	WCD9XXX_CURRENT_V_B1_H,
	WCD9XXX_CURRENT_V_B1_HU,
	WCD9XXX_CURRENT_V_BR_H,
};

static int wcd9xxx_detect_impedance(struct wcd9xxx_mbhc *mbhc, uint32_t *zl,
				    uint32_t *zr);
static s16 wcd9xxx_get_current_v(struct wcd9xxx_mbhc *mbhc,
				 const enum wcd9xxx_current_v_idx idx);
static void wcd9xxx_get_z(struct wcd9xxx_mbhc *mbhc, s16 *dce_z, s16 *sta_z);
static void wcd9xxx_mbhc_calc_thres(struct wcd9xxx_mbhc *mbhc);

static bool wcd9xxx_mbhc_polling(struct wcd9xxx_mbhc *mbhc)
{
	return mbhc->polling_active;
}

static void wcd9xxx_turn_onoff_override(struct wcd9xxx_mbhc *mbhc, bool on)
{
	struct snd_soc_codec *codec = mbhc->codec;
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL,
			    0x04, on ? 0x04 : 0x00);
}

/* called under codec_resource_lock acquisition */
static void wcd9xxx_pause_hs_polling(struct wcd9xxx_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);
	if (!mbhc->polling_active) {
		pr_debug("polling not active, nothing to pause\n");
		return;
	}

	/* Soft reset MBHC block */
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	pr_debug("%s: leave\n", __func__);
}

/* called under codec_resource_lock acquisition */
static void wcd9xxx_start_hs_polling(struct wcd9xxx_mbhc *mbhc)
{
	s16 v_brh, v_b1_hu;
	struct snd_soc_codec *codec = mbhc->codec;
	int mbhc_state = mbhc->mbhc_state;

	pr_debug("%s: enter\n", __func__);
	if (!mbhc->polling_active) {
		pr_debug("Polling is not active, do not start polling\n");
		return;
	}
	snd_soc_write(codec, WCD9XXX_A_MBHC_SCALING_MUX_1, 0x04);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mux_bias_block)
		mbhc->mbhc_cb->enable_mux_bias_block(codec);
	else
		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
				    0x80, 0x80);

	if (!mbhc->no_mic_headset_override &&
	    mbhc_state == MBHC_STATE_POTENTIAL) {
		pr_debug("%s recovering MBHC state machine\n", __func__);
		mbhc->mbhc_state = MBHC_STATE_POTENTIAL_RECOVERY;
		/* set to max button press threshold */
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B2_CTL, 0x7F);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B1_CTL, 0xFF);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B4_CTL, 0x7F);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B3_CTL, 0xFF);
		/* set to max */
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B6_CTL, 0x7F);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B5_CTL, 0xFF);

		v_brh = wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_BR_H);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B10_CTL,
			      (v_brh >> 8) & 0xFF);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B9_CTL,
			      v_brh & 0xFF);
		v_b1_hu = wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_B1_HU);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B3_CTL,
			      v_b1_hu & 0xFF);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B4_CTL,
			      (v_b1_hu >> 8) & 0xFF);
	}

	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x1);
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8, 0x0);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x1);
	pr_debug("%s: leave\n", __func__);
}

static int __wcd9xxx_resmgr_get_k_val(struct wcd9xxx_mbhc *mbhc,
		unsigned int cfilt_mv)
{
	if (mbhc->mbhc_cb &&
			mbhc->mbhc_cb->get_cdc_type() ==
					WCD9XXX_CDC_TYPE_HELICON)
		return 0x18;

	return wcd9xxx_resmgr_get_k_val(mbhc->resmgr, cfilt_mv);
}

/*
 * called under codec_resource_lock acquisition
 * return old status
 */
static bool __wcd9xxx_switch_micbias(struct wcd9xxx_mbhc *mbhc,
				     int vddio_switch, bool restartpolling,
				     bool checkpolling)
{
	bool ret;
	int cfilt_k_val;
	bool override;
	struct snd_soc_codec *codec;
	struct mbhc_internal_cal_data *d = &mbhc->mbhc_data;

	codec = mbhc->codec;

	if (mbhc->micbias_enable) {
		pr_debug("%s: micbias is already on\n", __func__);
		ret = mbhc->mbhc_micbias_switched;
		return ret;
	}

	ret = mbhc->mbhc_micbias_switched;
	if (vddio_switch && !mbhc->mbhc_micbias_switched &&
	    (!checkpolling || mbhc->polling_active)) {
		if (restartpolling)
			wcd9xxx_pause_hs_polling(mbhc);
		override = snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B1_CTL) &
			   0x04;
		if (!override)
			wcd9xxx_turn_onoff_override(mbhc, true);
		/* Adjust threshold if Mic Bias voltage changes */
		if (d->micb_mv != VDDIO_MICBIAS_MV) {
			cfilt_k_val = __wcd9xxx_resmgr_get_k_val(mbhc,
							      VDDIO_MICBIAS_MV);
			usleep_range(10000, 10000);
			snd_soc_update_bits(codec,
					mbhc->mbhc_bias_regs.cfilt_val,
					0xFC, (cfilt_k_val << 2));
			usleep_range(10000, 10000);
			/* Threshods for insertion/removal */
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B1_CTL,
				      d->v_ins_hu[MBHC_V_IDX_VDDIO] & 0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B2_CTL,
				      (d->v_ins_hu[MBHC_V_IDX_VDDIO] >> 8) &
				      0xFF);
			/* Threshods for button press */
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B3_CTL,
				      d->v_b1_hu[MBHC_V_IDX_VDDIO] & 0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B4_CTL,
				      (d->v_b1_hu[MBHC_V_IDX_VDDIO] >> 8) &
				      0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B5_CTL,
				      d->v_b1_h[MBHC_V_IDX_VDDIO] & 0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B6_CTL,
				      (d->v_b1_h[MBHC_V_IDX_VDDIO] >> 8) &
				      0xFF);
			/* Threshods for button release */
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B9_CTL,
				      d->v_brh[MBHC_V_IDX_VDDIO] & 0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B10_CTL,
				      (d->v_brh[MBHC_V_IDX_VDDIO] >> 8) & 0xFF);
			pr_debug("%s: Programmed MBHC thresholds to VDDIO\n",
				 __func__);
		}

		/* Enable MIC BIAS Switch to VDDIO */
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg,
				    0x80, 0x80);
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg,
				    0x10, 0x00);
		if (!override)
			wcd9xxx_turn_onoff_override(mbhc, false);
		if (restartpolling)
			wcd9xxx_start_hs_polling(mbhc);

		mbhc->mbhc_micbias_switched = true;
		pr_debug("%s: VDDIO switch enabled\n", __func__);
	} else if (!vddio_switch && mbhc->mbhc_micbias_switched) {
		if ((!checkpolling || mbhc->polling_active) &&
		    restartpolling)
			wcd9xxx_pause_hs_polling(mbhc);
		/* Reprogram thresholds */
		if (d->micb_mv != VDDIO_MICBIAS_MV) {
			cfilt_k_val =
			    __wcd9xxx_resmgr_get_k_val(mbhc,
						     d->micb_mv);
			snd_soc_update_bits(codec,
					mbhc->mbhc_bias_regs.cfilt_val,
					0xFC, (cfilt_k_val << 2));
			usleep_range(10000, 10000);
			/* Revert threshods for insertion/removal */
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B1_CTL,
					d->v_ins_hu[MBHC_V_IDX_CFILT] & 0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B2_CTL,
					(d->v_ins_hu[MBHC_V_IDX_CFILT] >> 8) &
					0xFF);
			/* Revert threshods for button press */
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B3_CTL,
				      d->v_b1_hu[MBHC_V_IDX_CFILT] & 0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B4_CTL,
				      (d->v_b1_hu[MBHC_V_IDX_CFILT] >> 8) &
				      0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B5_CTL,
				      d->v_b1_h[MBHC_V_IDX_CFILT] & 0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B6_CTL,
				      (d->v_b1_h[MBHC_V_IDX_CFILT] >> 8) &
				      0xFF);
			/* Revert threshods for button release */
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B9_CTL,
				      d->v_brh[MBHC_V_IDX_CFILT] & 0xFF);
			snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B10_CTL,
				      (d->v_brh[MBHC_V_IDX_CFILT] >> 8) & 0xFF);
			pr_debug("%s: Programmed MBHC thresholds to MICBIAS\n",
					__func__);
		}

		/* Disable MIC BIAS Switch to VDDIO */
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg, 0x80,
				    0x00);
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg, 0x10,
				    0x00);

		if ((!checkpolling || mbhc->polling_active) && restartpolling)
			wcd9xxx_start_hs_polling(mbhc);

		mbhc->mbhc_micbias_switched = false;
		pr_debug("%s: VDDIO switch disabled\n", __func__);
	}

	return ret;
}

static void wcd9xxx_switch_micbias(struct wcd9xxx_mbhc *mbhc, int vddio_switch)
{
	__wcd9xxx_switch_micbias(mbhc, vddio_switch, true, true);
}

static s16 wcd9xxx_get_current_v(struct wcd9xxx_mbhc *mbhc,
				 const enum wcd9xxx_current_v_idx idx)
{
	enum mbhc_v_index vidx;
	s16 ret = -EINVAL;

	if ((mbhc->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) &&
	    mbhc->mbhc_micbias_switched)
		vidx = MBHC_V_IDX_VDDIO;
	else
		vidx = MBHC_V_IDX_CFILT;

	switch (idx) {
	case WCD9XXX_CURRENT_V_INS_H:
		ret = (s16)mbhc->mbhc_data.v_ins_h[vidx];
		break;
	case WCD9XXX_CURRENT_V_INS_HU:
		ret = (s16)mbhc->mbhc_data.v_ins_hu[vidx];
		break;
	case WCD9XXX_CURRENT_V_B1_H:
		ret = (s16)mbhc->mbhc_data.v_b1_h[vidx];
		break;
	case WCD9XXX_CURRENT_V_B1_HU:
		ret = (s16)mbhc->mbhc_data.v_b1_hu[vidx];
		break;
	case WCD9XXX_CURRENT_V_BR_H:
		ret = (s16)mbhc->mbhc_data.v_brh[vidx];
		break;
	}

	return ret;
}

void *wcd9xxx_mbhc_cal_btn_det_mp(
			    const struct wcd9xxx_mbhc_btn_detect_cfg *btn_det,
			    const enum wcd9xxx_mbhc_btn_det_mem mem)
{
	void *ret = &btn_det->_v_btn_low;

	switch (mem) {
	case MBHC_BTN_DET_GAIN:
		ret += sizeof(btn_det->_n_cic);
	case MBHC_BTN_DET_N_CIC:
		ret += sizeof(btn_det->_n_ready);
	case MBHC_BTN_DET_N_READY:
		ret += sizeof(btn_det->_v_btn_high[0]) * btn_det->num_btn;
	case MBHC_BTN_DET_V_BTN_HIGH:
		ret += sizeof(btn_det->_v_btn_low[0]) * btn_det->num_btn;
	case MBHC_BTN_DET_V_BTN_LOW:
		/* do nothing */
		break;
	default:
		ret = NULL;
	}

	return ret;
}
EXPORT_SYMBOL(wcd9xxx_mbhc_cal_btn_det_mp);

static void wcd9xxx_calibrate_hs_polling(struct wcd9xxx_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;
	const s16 v_ins_hu = wcd9xxx_get_current_v(mbhc,
						   WCD9XXX_CURRENT_V_INS_HU);
	const s16 v_b1_hu = wcd9xxx_get_current_v(mbhc,
						  WCD9XXX_CURRENT_V_B1_HU);
	const s16 v_b1_h = wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_B1_H);
	const s16 v_brh = wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_BR_H);

	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B1_CTL, v_ins_hu & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B2_CTL,
		      (v_ins_hu >> 8) & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B3_CTL, v_b1_hu & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B4_CTL,
		      (v_b1_hu >> 8) & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B5_CTL, v_b1_h & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B6_CTL,
		      (v_b1_h >> 8) & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B9_CTL, v_brh & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B10_CTL,
		      (v_brh >> 8) & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B11_CTL,
		      mbhc->mbhc_data.v_brl & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B12_CTL,
		      (mbhc->mbhc_data.v_brl >> 8) & 0xFF);
}

static void wcd9xxx_codec_switch_cfilt_mode(struct wcd9xxx_mbhc *mbhc,
					    bool fast)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct wcd9xxx_cfilt_mode cfilt_mode;

	if (mbhc->mbhc_cb && mbhc->mbhc_cb->switch_cfilt_mode) {
		cfilt_mode = mbhc->mbhc_cb->switch_cfilt_mode(mbhc, fast);
	} else {
		if (fast)
			cfilt_mode.reg_mode_val = WCD9XXX_CFILT_FAST_MODE;
		else
			cfilt_mode.reg_mode_val = WCD9XXX_CFILT_SLOW_MODE;

		cfilt_mode.reg_mask = 0x40;
		cfilt_mode.cur_mode_val =
		    snd_soc_read(codec, mbhc->mbhc_bias_regs.cfilt_ctl) & 0x40;
	}

	if (cfilt_mode.cur_mode_val
			!= cfilt_mode.reg_mode_val) {
		if (mbhc->polling_active)
			wcd9xxx_pause_hs_polling(mbhc);
		snd_soc_update_bits(codec,
				    mbhc->mbhc_bias_regs.cfilt_ctl,
					cfilt_mode.reg_mask,
					cfilt_mode.reg_mode_val);
		if (mbhc->polling_active)
			wcd9xxx_start_hs_polling(mbhc);
		pr_debug("%s: CFILT mode change (%x to %x)\n", __func__,
			cfilt_mode.cur_mode_val,
			cfilt_mode.reg_mode_val);
	} else {
		pr_debug("%s: CFILT Value is already %x\n",
			 __func__, cfilt_mode.cur_mode_val);
	}
}

static void wcd9xxx_jack_report(struct wcd9xxx_mbhc *mbhc,
				struct snd_soc_jack *jack, int status, int mask)
{
	if (jack == &mbhc->headset_jack) {
		wcd9xxx_resmgr_cond_update_cond(mbhc->resmgr,
						WCD9XXX_COND_HPH_MIC,
						status & SND_JACK_MICROPHONE);
		wcd9xxx_resmgr_cond_update_cond(mbhc->resmgr,
						WCD9XXX_COND_HPH,
						status & SND_JACK_HEADPHONE);
	}

	snd_soc_jack_report_no_dapm(jack, status, mask);
}

static void __hphocp_off_report(struct wcd9xxx_mbhc *mbhc, u32 jack_status,
				int irq)
{
	struct snd_soc_codec *codec;

	pr_debug("%s: clear ocp status %x\n", __func__, jack_status);
	codec = mbhc->codec;
	if (mbhc->hph_status & jack_status) {
		mbhc->hph_status &= ~jack_status;
		wcd9xxx_jack_report(mbhc, &mbhc->headset_jack,
				    mbhc->hph_status, WCD9XXX_JACK_MASK);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_OCP_CTL, 0x10,
				    0x00);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_OCP_CTL, 0x10,
				    0x10);
		/*
		 * reset retry counter as PA is turned off signifying
		 * start of new OCP detection session
		 */
		if (WCD9XXX_IRQ_HPH_PA_OCPL_FAULT)
			mbhc->hphlocp_cnt = 0;
		else
			mbhc->hphrocp_cnt = 0;
		wcd9xxx_enable_irq(mbhc->resmgr->core_res, irq);
	}
}

static void hphrocp_off_report(struct wcd9xxx_mbhc *mbhc, u32 jack_status)
{
	__hphocp_off_report(mbhc, SND_JACK_OC_HPHR,
			    WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
}

static void hphlocp_off_report(struct wcd9xxx_mbhc *mbhc, u32 jack_status)
{
	__hphocp_off_report(mbhc, SND_JACK_OC_HPHL,
			    WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
}

static void wcd9xxx_get_mbhc_micbias_regs(struct wcd9xxx_mbhc *mbhc,
					struct mbhc_micbias_regs *micbias_regs)
{
	unsigned int cfilt;
	struct wcd9xxx_pdata *pdata = mbhc->resmgr->pdata;

	if (mbhc->mbhc_cb &&
			mbhc->mbhc_cb->get_cdc_type() ==
					WCD9XXX_CDC_TYPE_HELICON) {
		micbias_regs->mbhc_reg = WCD9XXX_A_MICB_1_MBHC;
		micbias_regs->int_rbias = WCD9XXX_A_MICB_1_INT_RBIAS;
		micbias_regs->ctl_reg = WCD9XXX_A_MICB_1_CTL;
		micbias_regs->cfilt_val = WCD9XXX_A_MICB_CFILT_1_VAL;
		micbias_regs->cfilt_ctl = WCD9XXX_A_MICB_CFILT_1_CTL;
		mbhc->mbhc_data.micb_mv = 1800;
		return;
	}

	switch (mbhc->mbhc_cfg->micbias) {
	case MBHC_MICBIAS1:
		cfilt = pdata->micbias.bias1_cfilt_sel;
		micbias_regs->mbhc_reg = WCD9XXX_A_MICB_1_MBHC;
		micbias_regs->int_rbias = WCD9XXX_A_MICB_1_INT_RBIAS;
		micbias_regs->ctl_reg = WCD9XXX_A_MICB_1_CTL;
		break;
	case MBHC_MICBIAS2:
		cfilt = pdata->micbias.bias2_cfilt_sel;
		micbias_regs->mbhc_reg = WCD9XXX_A_MICB_2_MBHC;
		micbias_regs->int_rbias = WCD9XXX_A_MICB_2_INT_RBIAS;
		micbias_regs->ctl_reg = WCD9XXX_A_MICB_2_CTL;
		break;
	case MBHC_MICBIAS3:
		cfilt = pdata->micbias.bias3_cfilt_sel;
		micbias_regs->mbhc_reg = WCD9XXX_A_MICB_3_MBHC;
		micbias_regs->int_rbias = WCD9XXX_A_MICB_3_INT_RBIAS;
		micbias_regs->ctl_reg = WCD9XXX_A_MICB_3_CTL;
		break;
	case MBHC_MICBIAS4:
		cfilt = pdata->micbias.bias4_cfilt_sel;
		micbias_regs->mbhc_reg = mbhc->resmgr->reg_addr->micb_4_mbhc;
		micbias_regs->int_rbias =
		    mbhc->resmgr->reg_addr->micb_4_int_rbias;
		micbias_regs->ctl_reg = mbhc->resmgr->reg_addr->micb_4_ctl;
		break;
	default:
		/* Should never reach here */
		pr_err("%s: Invalid MIC BIAS for MBHC\n", __func__);
		return;
	}

	micbias_regs->cfilt_sel = cfilt;

	switch (cfilt) {
	case WCD9XXX_CFILT1_SEL:
		micbias_regs->cfilt_val = WCD9XXX_A_MICB_CFILT_1_VAL;
		micbias_regs->cfilt_ctl = WCD9XXX_A_MICB_CFILT_1_CTL;
		mbhc->mbhc_data.micb_mv =
		    mbhc->resmgr->pdata->micbias.cfilt1_mv;
		break;
	case WCD9XXX_CFILT2_SEL:
		micbias_regs->cfilt_val = WCD9XXX_A_MICB_CFILT_2_VAL;
		micbias_regs->cfilt_ctl = WCD9XXX_A_MICB_CFILT_2_CTL;
		mbhc->mbhc_data.micb_mv =
		    mbhc->resmgr->pdata->micbias.cfilt2_mv;
		break;
	case WCD9XXX_CFILT3_SEL:
		micbias_regs->cfilt_val = WCD9XXX_A_MICB_CFILT_3_VAL;
		micbias_regs->cfilt_ctl = WCD9XXX_A_MICB_CFILT_3_CTL;
		mbhc->mbhc_data.micb_mv =
		    mbhc->resmgr->pdata->micbias.cfilt3_mv;
		break;
	}
}

static void wcd9xxx_clr_and_turnon_hph_padac(struct wcd9xxx_mbhc *mbhc)
{
	bool pa_turned_on = false;
	struct snd_soc_codec *codec = mbhc->codec;
	u8 wg_time;

	wg_time = snd_soc_read(codec, WCD9XXX_A_RX_HPH_CNP_WG_TIME) ;
	wg_time += 1;

	if (test_and_clear_bit(WCD9XXX_HPHR_DAC_OFF_ACK,
			       &mbhc->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_R_DAC_CTL,
				    0xC0, 0xC0);
	}
	if (test_and_clear_bit(WCD9XXX_HPHL_DAC_OFF_ACK,
				&mbhc->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable DAC\n", __func__);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_L_DAC_CTL,
				    0x80, 0x80);
	}

	if (test_and_clear_bit(WCD9XXX_HPHR_PA_OFF_ACK,
			       &mbhc->hph_pa_dac_state)) {
		pr_debug("%s: HPHR clear flag and enable PA\n", __func__);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_CNP_EN, 0x10,
				    1 << 4);
		pa_turned_on = true;
	}
	if (test_and_clear_bit(WCD9XXX_HPHL_PA_OFF_ACK,
			       &mbhc->hph_pa_dac_state)) {
		pr_debug("%s: HPHL clear flag and enable PA\n", __func__);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_CNP_EN, 0x20, 1
				    << 5);
		pa_turned_on = true;
	}

	if (pa_turned_on) {
		pr_debug("%s: PA was turned off by MBHC and not by DAPM\n",
			 __func__);
		usleep_range(wg_time * 1000, wg_time * 1000);
	}
}

static int wcd9xxx_cancel_btn_work(struct wcd9xxx_mbhc *mbhc)
{
	int r;
	r = cancel_delayed_work_sync(&mbhc->mbhc_btn_dwork);
	if (r)
		/* if scheduled mbhc.mbhc_btn_dwork is canceled from here,
		 * we have to unlock from here instead btn_work */
		wcd9xxx_unlock_sleep(mbhc->resmgr->core_res);
	return r;
}

static bool wcd9xxx_is_hph_dac_on(struct snd_soc_codec *codec, int left)
{
	u8 hph_reg_val = 0;
	if (left)
		hph_reg_val = snd_soc_read(codec, WCD9XXX_A_RX_HPH_L_DAC_CTL);
	else
		hph_reg_val = snd_soc_read(codec, WCD9XXX_A_RX_HPH_R_DAC_CTL);

	return (hph_reg_val & 0xC0) ? true : false;
}

static bool wcd9xxx_is_hph_pa_on(struct snd_soc_codec *codec)
{
	u8 hph_reg_val = 0;
	hph_reg_val = snd_soc_read(codec, WCD9XXX_A_RX_HPH_CNP_EN);

	return (hph_reg_val & 0x30) ? true : false;
}

/* called under codec_resource_lock acquisition */
static void wcd9xxx_set_and_turnoff_hph_padac(struct wcd9xxx_mbhc *mbhc)
{
	u8 wg_time;
	struct snd_soc_codec *codec = mbhc->codec;

	wg_time = snd_soc_read(codec, WCD9XXX_A_RX_HPH_CNP_WG_TIME);
	wg_time += 1;

	/* If headphone PA is on, check if userspace receives
	 * removal event to sync-up PA's state */
	if (wcd9xxx_is_hph_pa_on(codec)) {
		pr_debug("%s PA is on, setting PA_OFF_ACK\n", __func__);
		set_bit(WCD9XXX_HPHL_PA_OFF_ACK, &mbhc->hph_pa_dac_state);
		set_bit(WCD9XXX_HPHR_PA_OFF_ACK, &mbhc->hph_pa_dac_state);
	} else {
		pr_debug("%s PA is off\n", __func__);
	}

	if (wcd9xxx_is_hph_dac_on(codec, 1))
		set_bit(WCD9XXX_HPHL_DAC_OFF_ACK, &mbhc->hph_pa_dac_state);
	if (wcd9xxx_is_hph_dac_on(codec, 0))
		set_bit(WCD9XXX_HPHR_DAC_OFF_ACK, &mbhc->hph_pa_dac_state);

	snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_CNP_EN, 0x30, 0x00);
	snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_L_DAC_CTL, 0x80, 0x00);
	snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_R_DAC_CTL, 0xC0, 0x00);
	usleep_range(wg_time * 1000, wg_time * 1000);
}

static void wcd9xxx_insert_detect_setup(struct wcd9xxx_mbhc *mbhc, bool ins)
{
	if (!mbhc->mbhc_cfg->insert_detect)
		return;
	pr_debug("%s: Setting up %s detection\n", __func__,
		 ins ? "insert" : "removal");
	/* Disable detection to avoid glitch */
	snd_soc_update_bits(mbhc->codec, WCD9XXX_A_MBHC_INSERT_DETECT, 1, 0);
	if (mbhc->mbhc_cfg->gpio_level_insert)
		snd_soc_write(mbhc->codec, WCD9XXX_A_MBHC_INSERT_DETECT,
			      (0x68 | (ins ? (1 << 1) : 0)));
	else
		snd_soc_write(mbhc->codec, WCD9XXX_A_MBHC_INSERT_DETECT,
			      (0x6C | (ins ? (1 << 1) : 0)));
	/* Re-enable detection */
	snd_soc_update_bits(mbhc->codec, WCD9XXX_A_MBHC_INSERT_DETECT, 1, 1);
}

/* called under codec_resource_lock acquisition */
static void wcd9xxx_report_plug(struct wcd9xxx_mbhc *mbhc, int insertion,
				enum snd_jack_types jack_type)
{
	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);

	pr_debug("%s: enter insertion %d hph_status %x\n",
		 __func__, insertion, mbhc->hph_status);
	if (!insertion) {
		/* Report removal */
		mbhc->hph_status &= ~jack_type;
		/*
		 * cancel possibly scheduled btn work and
		 * report release if we reported button press
		 */
		if (wcd9xxx_cancel_btn_work(mbhc))
			pr_debug("%s: button press is canceled\n", __func__);
		else if (mbhc->buttons_pressed) {
			pr_debug("%s: release of button press%d\n",
				 __func__, jack_type);
			wcd9xxx_jack_report(mbhc, &mbhc->button_jack, 0,
					    mbhc->buttons_pressed);
			mbhc->buttons_pressed &=
				~WCD9XXX_JACK_BUTTON_MASK;
		}

		if (mbhc->micbias_enable && mbhc->micbias_enable_cb) {
			pr_debug("%s: Disabling micbias\n", __func__);
			mbhc->micbias_enable_cb(mbhc->codec, false);
			mbhc->micbias_enable = false;
		}
		mbhc->zl = mbhc->zr = 0;
		pr_debug("%s: Reporting removal %d(%x)\n", __func__,
			 jack_type, mbhc->hph_status);
		wcd9xxx_jack_report(mbhc, &mbhc->headset_jack, mbhc->hph_status,
				    WCD9XXX_JACK_MASK);
		wcd9xxx_set_and_turnoff_hph_padac(mbhc);
		hphrocp_off_report(mbhc, SND_JACK_OC_HPHR);
		hphlocp_off_report(mbhc, SND_JACK_OC_HPHL);
		mbhc->current_plug = PLUG_TYPE_NONE;
		mbhc->polling_active = false;
	} else {
		if (mbhc->mbhc_cfg->detect_extn_cable) {
			/* Report removal of current jack type */
			if (mbhc->hph_status && mbhc->hph_status != jack_type) {
				if (mbhc->micbias_enable &&
				    mbhc->micbias_enable_cb &&
				    mbhc->hph_status == SND_JACK_HEADSET) {
					pr_debug("%s: Disabling micbias\n",
						 __func__);
					mbhc->micbias_enable_cb(mbhc->codec,
								false);
					mbhc->micbias_enable = false;
				}
				pr_debug("%s: Reporting removal (%x)\n",
						__func__, mbhc->hph_status);
				mbhc->zl = mbhc->zr = 0;
				wcd9xxx_jack_report(mbhc, &mbhc->headset_jack,
						    0, WCD9XXX_JACK_MASK);
				mbhc->hph_status = 0;
			}
		}
		/* Report insertion */
		mbhc->hph_status |= jack_type;

		if (jack_type == SND_JACK_HEADPHONE) {
			mbhc->current_plug = PLUG_TYPE_HEADPHONE;
		} else if (jack_type == SND_JACK_UNSUPPORTED) {
			mbhc->current_plug = PLUG_TYPE_GND_MIC_SWAP;
		} else if (jack_type == SND_JACK_HEADSET) {
			mbhc->polling_active = BUTTON_POLLING_SUPPORTED;
			mbhc->current_plug = PLUG_TYPE_HEADSET;
			mbhc->update_z = true;
		} else if (jack_type == SND_JACK_LINEOUT) {
			mbhc->current_plug = PLUG_TYPE_HIGH_HPH;
		}

		if (mbhc->micbias_enable && mbhc->micbias_enable_cb) {
			pr_debug("%s: Enabling micbias\n", __func__);
			mbhc->micbias_enable_cb(mbhc->codec, true);
		}

		if (mbhc->impedance_detect && impedance_detect_en)
			wcd9xxx_detect_impedance(mbhc, &mbhc->zl, &mbhc->zr);

		pr_debug("%s: Reporting insertion %d(%x)\n", __func__,
			 jack_type, mbhc->hph_status);
		wcd9xxx_jack_report(mbhc, &mbhc->headset_jack,
				    mbhc->hph_status, WCD9XXX_JACK_MASK);
		wcd9xxx_clr_and_turnon_hph_padac(mbhc);
	}
	/* Setup insert detect */
	wcd9xxx_insert_detect_setup(mbhc, !insertion);

	pr_debug("%s: leave hph_status %x\n", __func__, mbhc->hph_status);
}

/* should be called under interrupt context that hold suspend */
static void wcd9xxx_schedule_hs_detect_plug(struct wcd9xxx_mbhc *mbhc,
					    struct work_struct *work)
{
	pr_debug("%s: scheduling wcd9xxx_correct_swch_plug\n", __func__);
	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);
	mbhc->hs_detect_work_stop = false;
	wcd9xxx_lock_sleep(mbhc->resmgr->core_res);
	schedule_work(work);
}

/* called under codec_resource_lock acquisition */
static void wcd9xxx_cancel_hs_detect_plug(struct wcd9xxx_mbhc *mbhc,
					 struct work_struct *work)
{
	pr_debug("%s: Canceling correct_plug_swch\n", __func__);
	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);
	mbhc->hs_detect_work_stop = true;
	wmb();
	WCD9XXX_BCL_UNLOCK(mbhc->resmgr);
	if (cancel_work_sync(work)) {
		pr_debug("%s: correct_plug_swch is canceled\n",
			 __func__);
		wcd9xxx_unlock_sleep(mbhc->resmgr->core_res);
	}
	WCD9XXX_BCL_LOCK(mbhc->resmgr);
}

static s16 scale_v_micb_vddio(struct wcd9xxx_mbhc *mbhc, int v, bool tovddio)
{
	int r;
	int vddio_k, mb_k;
	vddio_k = __wcd9xxx_resmgr_get_k_val(mbhc, VDDIO_MICBIAS_MV);
	mb_k = __wcd9xxx_resmgr_get_k_val(mbhc, mbhc->mbhc_data.micb_mv);
	if (tovddio)
		r = v * (vddio_k + 4) / (mb_k + 4);
	else
		r = v * (mb_k + 4) / (vddio_k + 4);
	return r;
}

static s16 wcd9xxx_get_current_v_hs_max(struct wcd9xxx_mbhc *mbhc)
{
	s16 v_hs_max;
	struct wcd9xxx_mbhc_plug_type_cfg *plug_type;

	plug_type = WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(mbhc->mbhc_cfg->calibration);
	if ((mbhc->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) &&
	    mbhc->mbhc_micbias_switched)
		v_hs_max = scale_v_micb_vddio(mbhc, plug_type->v_hs_max, true);
	else
		v_hs_max = plug_type->v_hs_max;
	return v_hs_max;
}

static short wcd9xxx_read_sta_result(struct snd_soc_codec *codec)
{
	u8 bias_msb, bias_lsb;
	short bias_value;

	bias_msb = snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B3_STATUS);
	bias_lsb = snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B2_STATUS);
	bias_value = (bias_msb << 8) | bias_lsb;
	return bias_value;
}

static short wcd9xxx_read_dce_result(struct snd_soc_codec *codec)
{
	u8 bias_msb, bias_lsb;
	short bias_value;

	bias_msb = snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B5_STATUS);
	bias_lsb = snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B4_STATUS);
	bias_value = (bias_msb << 8) | bias_lsb;
	return bias_value;
}

static void wcd9xxx_turn_onoff_rel_detection(struct snd_soc_codec *codec,
					     bool on)
{
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x02, on << 1);
}

static short __wcd9xxx_codec_sta_dce(struct wcd9xxx_mbhc *mbhc, int dce,
				     bool override_bypass, bool noreldetection)
{
	short bias_value;
	struct snd_soc_codec *codec = mbhc->codec;

	wcd9xxx_disable_irq(mbhc->resmgr->core_res, WCD9XXX_IRQ_MBHC_POTENTIAL);
	if (noreldetection)
		wcd9xxx_turn_onoff_rel_detection(codec, false);

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x2, 0x0);
	/* Turn on the override */
	if (!override_bypass)
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x4, 0x4);
	if (dce) {
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8,
				    0x8);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x4);
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8,
				    0x0);
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x2,
				    0x2);
		usleep_range(mbhc->mbhc_data.t_sta_dce,
			     mbhc->mbhc_data.t_sta_dce);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x4);
		usleep_range(mbhc->mbhc_data.t_dce, mbhc->mbhc_data.t_dce);
		bias_value = wcd9xxx_read_dce_result(codec);
	} else {
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8,
				    0x8);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x2);
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8,
				    0x0);
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x2,
				    0x2);
		usleep_range(mbhc->mbhc_data.t_sta_dce,
			     mbhc->mbhc_data.t_sta_dce);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x2);
		usleep_range(mbhc->mbhc_data.t_sta,
			     mbhc->mbhc_data.t_sta);
		bias_value = wcd9xxx_read_sta_result(codec);
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8,
				    0x8);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x0);
	}
	/* Turn off the override after measuring mic voltage */
	if (!override_bypass)
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x04,
				    0x00);

	if (noreldetection)
		wcd9xxx_turn_onoff_rel_detection(codec, true);
	wcd9xxx_enable_irq(mbhc->resmgr->core_res, WCD9XXX_IRQ_MBHC_POTENTIAL);

	return bias_value;
}

static short wcd9xxx_codec_sta_dce(struct wcd9xxx_mbhc *mbhc, int dce,
				   bool norel)
{
	return __wcd9xxx_codec_sta_dce(mbhc, dce, false, norel);
}

static s32 __wcd9xxx_codec_sta_dce_v(struct wcd9xxx_mbhc *mbhc, s8 dce,
				     u16 bias_value, s16 z, u32 micb_mv)
{
	s16 value, mb;
	s32 mv;

	value = bias_value;
	if (dce) {
		mb = (mbhc->mbhc_data.dce_mb);
		mv = (value - z) * (s32)micb_mv / (mb - z);
	} else {
		mb = (mbhc->mbhc_data.sta_mb);
		mv = (value - z) * (s32)micb_mv / (mb - z);
	}

	return mv;
}

static s32 wcd9xxx_codec_sta_dce_v(struct wcd9xxx_mbhc *mbhc, s8 dce,
				   u16 bias_value)
{
	s16 z;
	z = dce ? (s16)mbhc->mbhc_data.dce_z : (s16)mbhc->mbhc_data.sta_z;
	return __wcd9xxx_codec_sta_dce_v(mbhc, dce, bias_value, z,
					 mbhc->mbhc_data.micb_mv);
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static short wcd9xxx_mbhc_setup_hs_polling(struct wcd9xxx_mbhc *mbhc,
					   bool is_cs_enable)
{
	struct snd_soc_codec *codec = mbhc->codec;
	short bias_value;
	u8 cfilt_mode;
	s16 reg;
	int change;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_det;
	s16 sta_z = 0, dce_z = 0;

	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);

	pr_debug("%s: enter\n", __func__);
	if (!mbhc->mbhc_cfg->calibration) {
		pr_err("%s: Error, no calibration exists\n", __func__);
		return -ENODEV;
	}

	btn_det = WCD9XXX_MBHC_CAL_BTN_DET_PTR(mbhc->mbhc_cfg->calibration);
	/* Enable external voltage source to micbias if present */
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mb_source)
		mbhc->mbhc_cb->enable_mb_source(codec, true);

	/*
	 * setup internal micbias if codec uses internal micbias for
	 * headset detection
	 */
	if (mbhc->mbhc_cfg->use_int_rbias) {
		if (mbhc->mbhc_cb && mbhc->mbhc_cb->setup_int_rbias)
			mbhc->mbhc_cb->setup_int_rbias(codec, true);
	else
		pr_err("%s: internal bias is requested but codec did not provide callback\n",
			 __func__);
	}

	/*
	 * Request BG and clock.
	 * These will be released by wcd9xxx_cleanup_hs_polling
	 */
	WCD9XXX_BG_CLK_LOCK(mbhc->resmgr);
	wcd9xxx_resmgr_get_bandgap(mbhc->resmgr, WCD9XXX_BANDGAP_AUDIO_MODE);
	wcd9xxx_resmgr_get_clk_block(mbhc->resmgr, WCD9XXX_CLK_RCO);
	WCD9XXX_BG_CLK_UNLOCK(mbhc->resmgr);

	snd_soc_update_bits(codec, WCD9XXX_A_CLK_BUFF_EN1, 0x05, 0x01);

	/* Make sure CFILT is in fast mode, save current mode */
	cfilt_mode = snd_soc_read(codec, mbhc->mbhc_bias_regs.cfilt_ctl);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->cfilt_fast_mode)
		mbhc->mbhc_cb->cfilt_fast_mode(codec, mbhc);
	else
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.cfilt_ctl,
				    0x70, 0x00);

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_write(codec, WCD9XXX_A_MBHC_SCALING_MUX_1, 0x04);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mux_bias_block)
		mbhc->mbhc_cb->enable_mux_bias_block(codec);
	else
		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
				    0x80, 0x80);

	snd_soc_update_bits(codec, WCD9XXX_A_TX_7_MBHC_EN, 0x80, 0x80);
	snd_soc_update_bits(codec, WCD9XXX_A_TX_7_MBHC_EN, 0x1F, 0x1C);
	snd_soc_update_bits(codec, WCD9XXX_A_TX_7_MBHC_TEST_CTL, 0x40, 0x40);

	snd_soc_update_bits(codec, WCD9XXX_A_TX_7_MBHC_EN, 0x80, 0x00);
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8, 0x00);

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x8, 0x8);

	/* don't flip override */
	bias_value = __wcd9xxx_codec_sta_dce(mbhc, 1, true, true);
	snd_soc_write(codec, mbhc->mbhc_bias_regs.cfilt_ctl, cfilt_mode);
	snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x13, 0x00);

	/* recalibrate dce_z and sta_z */
	reg = snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B1_CTL);
	change = snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x78,
				     btn_det->mbhc_nsc << 3);
	wcd9xxx_get_z(mbhc, &dce_z, &sta_z);
	if (change)
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, reg);
	if (dce_z && sta_z) {
		pr_debug("%s: sta_z 0x%x -> 0x%x, dce_z 0x%x -> 0x%x\n",
			 __func__,
			 mbhc->mbhc_data.sta_z, sta_z & 0xffff,
			 mbhc->mbhc_data.dce_z, dce_z & 0xffff);
		mbhc->mbhc_data.dce_z = dce_z;
		mbhc->mbhc_data.sta_z = sta_z;
		wcd9xxx_mbhc_calc_thres(mbhc);
		wcd9xxx_calibrate_hs_polling(mbhc);
	} else {
		pr_warn("%s: failed get new dce_z/sta_z 0x%x/0x%x\n", __func__,
			dce_z, sta_z);
	}

	if (is_cs_enable) {
		/* recalibrate dce_nsc_cs_z */
		reg = snd_soc_read(mbhc->codec, WCD9XXX_A_CDC_MBHC_B1_CTL);
		snd_soc_update_bits(mbhc->codec, WCD9XXX_A_CDC_MBHC_B1_CTL,
				    0x78, WCD9XXX_MBHC_NSC_CS << 3);
		wcd9xxx_get_z(mbhc, &dce_z, NULL);
		snd_soc_write(mbhc->codec, WCD9XXX_A_CDC_MBHC_B1_CTL, reg);
		if (dce_z) {
			pr_debug("%s: dce_nsc_cs_z 0x%x -> 0x%x\n", __func__,
				 mbhc->mbhc_data.dce_nsc_cs_z, dce_z & 0xffff);
			mbhc->mbhc_data.dce_nsc_cs_z = dce_z;
		} else {
			pr_debug("%s: failed get new dce_nsc_cs_z\n", __func__);
		}
	}

	return bias_value;
}

static void wcd9xxx_shutdown_hs_removal_detect(struct wcd9xxx_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;
	const struct wcd9xxx_mbhc_general_cfg *generic =
	    WCD9XXX_MBHC_CAL_GENERAL_PTR(mbhc->mbhc_cfg->calibration);

	/* Need MBHC clock */
	WCD9XXX_BG_CLK_LOCK(mbhc->resmgr);
	wcd9xxx_resmgr_get_clk_block(mbhc->resmgr, WCD9XXX_CLK_RCO);
	WCD9XXX_BG_CLK_UNLOCK(mbhc->resmgr);

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x2, 0x2);
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x6, 0x0);
	__wcd9xxx_switch_micbias(mbhc, 0, false, false);

	usleep_range(generic->t_shutdown_plug_rem,
		     generic->t_shutdown_plug_rem);

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0xA, 0x8);

	WCD9XXX_BG_CLK_LOCK(mbhc->resmgr);
	/* Put requested CLK back */
	wcd9xxx_resmgr_put_clk_block(mbhc->resmgr, WCD9XXX_CLK_RCO);
	WCD9XXX_BG_CLK_UNLOCK(mbhc->resmgr);

	snd_soc_write(codec, WCD9XXX_A_MBHC_SCALING_MUX_1, 0x00);
}

static void wcd9xxx_cleanup_hs_polling(struct wcd9xxx_mbhc *mbhc)
{

	pr_debug("%s: enter\n", __func__);
	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);

	wcd9xxx_shutdown_hs_removal_detect(mbhc);

	/* Release clock and BG requested by wcd9xxx_mbhc_setup_hs_polling */
	WCD9XXX_BG_CLK_LOCK(mbhc->resmgr);
	wcd9xxx_resmgr_put_clk_block(mbhc->resmgr, WCD9XXX_CLK_RCO);
	wcd9xxx_resmgr_put_bandgap(mbhc->resmgr, WCD9XXX_BANDGAP_MBHC_MODE);
	WCD9XXX_BG_CLK_UNLOCK(mbhc->resmgr);

	/* Disable external voltage source to micbias if present */
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mb_source)
		mbhc->mbhc_cb->enable_mb_source(mbhc->codec, false);

	mbhc->polling_active = false;
	mbhc->mbhc_state = MBHC_STATE_NONE;
	pr_debug("%s: leave\n", __func__);
}

/* called under codec_resource_lock acquisition */
static void wcd9xxx_codec_hphr_gnd_switch(struct snd_soc_codec *codec, bool on)
{
	snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x01, on);
	if (on)
		usleep_range(5000, 5000);
}

static void wcd9xxx_onoff_vddio_switch(struct wcd9xxx_mbhc *mbhc, bool on)
{
	pr_debug("%s: vddio %d\n", __func__, on);

	if (mbhc->mbhc_cb && mbhc->mbhc_cb->pull_mb_to_vddio) {
		mbhc->mbhc_cb->pull_mb_to_vddio(mbhc->codec, on);
		goto exit;
	}

	if (on) {
		snd_soc_update_bits(mbhc->codec, mbhc->mbhc_bias_regs.mbhc_reg,
				    1 << 7, 1 << 7);
		snd_soc_update_bits(mbhc->codec, WCD9XXX_A_MAD_ANA_CTRL,
				    1 << 4, 0);
	} else {
		snd_soc_update_bits(mbhc->codec, WCD9XXX_A_MAD_ANA_CTRL,
				    1 << 4, 1 << 4);
		snd_soc_update_bits(mbhc->codec, mbhc->mbhc_bias_regs.mbhc_reg,
				    1 << 7, 0);
	}

exit:
	/*
	 * Wait for the micbias to settle down to vddio
	 * when the micbias to vddio switch is enabled.
	 */
	if (on)
		usleep_range(10000, 10000);
}

static int wcd9xxx_hphl_status(struct wcd9xxx_mbhc *mbhc)
{
	u16 hph, status;
	struct snd_soc_codec *codec = mbhc->codec;

	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);
	hph = snd_soc_read(codec, WCD9XXX_A_MBHC_HPH);
	snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x12, 0x02);
	usleep_range(WCD9XXX_HPHL_STATUS_READY_WAIT_US,
		     WCD9XXX_HPHL_STATUS_READY_WAIT_US +
		     WCD9XXX_USLEEP_RANGE_MARGIN_US);
	status = snd_soc_read(codec, WCD9XXX_A_RX_HPH_L_STATUS);
	snd_soc_write(codec, WCD9XXX_A_MBHC_HPH, hph);
	return status;
}

static enum wcd9xxx_mbhc_plug_type
wcd9xxx_cs_find_plug_type(struct wcd9xxx_mbhc *mbhc,
			  struct wcd9xxx_mbhc_detect *dt, const int size,
			  bool highhph,
			  unsigned long event_state)
{
	int i;
	int vdce, mb_mv;
	int ch, sz, delta_thr;
	int minv = 0, maxv = INT_MIN;
	struct wcd9xxx_mbhc_detect *d = dt;
	struct wcd9xxx_mbhc_detect *dprev = d, *dmicbias = NULL, *dgnd = NULL;
	enum wcd9xxx_mbhc_plug_type type = PLUG_TYPE_INVALID;

	const struct wcd9xxx_mbhc_plug_type_cfg *plug_type =
	    WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(mbhc->mbhc_cfg->calibration);
	s16 hs_max, no_mic, dce_z;

	pr_debug("%s: enter\n", __func__);
	pr_debug("%s: event_state 0x%lx\n", __func__, event_state);

	sz = size - 1;
	for (i = 0, d = dt, ch = 0; i < sz; i++, d++) {
		if (d->mic_bias) {
			dce_z = mbhc->mbhc_data.dce_z;
			mb_mv = mbhc->mbhc_data.micb_mv;
			hs_max = plug_type->v_hs_max;
			no_mic = plug_type->v_no_mic;
		} else {
			dce_z = mbhc->mbhc_data.dce_nsc_cs_z;
			mb_mv = VDDIO_MICBIAS_MV;
			hs_max = WCD9XXX_V_CS_HS_MAX;
			no_mic = WCD9XXX_V_CS_NO_MIC;
		}

		vdce = __wcd9xxx_codec_sta_dce_v(mbhc, true, d->dce,
						 dce_z, (u32)mb_mv);

		d->_vdces = vdce;
		if (d->_vdces < no_mic)
			d->_type = PLUG_TYPE_HEADPHONE;
		else if (d->_vdces >= hs_max)
			d->_type = PLUG_TYPE_HIGH_HPH;
		else
			d->_type = PLUG_TYPE_HEADSET;

		pr_debug("%s: DCE #%d, %04x, V %04d(%04d), HPHL %d TYPE %d\n",
			 __func__, i, d->dce, vdce, d->_vdces,
			 d->hphl_status & 0x01,
			 d->_type);

		ch += d->hphl_status & 0x01;
		if (!d->swap_gnd && !d->mic_bias) {
			if (maxv < d->_vdces)
				maxv = d->_vdces;
			if (!minv || minv > d->_vdces)
				minv = d->_vdces;
		}
		if ((!d->mic_bias &&
		    (d->_vdces >= WCD9XXX_CS_MEAS_INVALD_RANGE_LOW_MV &&
		     d->_vdces <= WCD9XXX_CS_MEAS_INVALD_RANGE_HIGH_MV)) ||
		    (d->mic_bias &&
		    (d->_vdces >= WCD9XXX_MEAS_INVALD_RANGE_LOW_MV &&
		     d->_vdces <= WCD9XXX_MEAS_INVALD_RANGE_HIGH_MV))) {
			pr_debug("%s: within invalid range\n", __func__);
			type = PLUG_TYPE_INVALID;
			goto exit;
		}
	}

	if (event_state & (1 << MBHC_EVENT_PA_HPHL)) {
		pr_debug("%s: HPHL PA was ON\n", __func__);
	} else if (ch != sz && ch > 0) {
		pr_debug("%s: Invalid, inconsistent HPHL\n", __func__);
		type = PLUG_TYPE_INVALID;
		goto exit;
	}

	delta_thr = highhph ? WCD9XXX_MB_MEAS_DELTA_MAX_MV :
			      WCD9XXX_CS_MEAS_DELTA_MAX_MV;

	for (i = 0, d = dt; i < sz; i++, d++) {
		if ((i > 0) && !d->mic_bias && !d->swap_gnd &&
		    (d->_type != dprev->_type)) {
			pr_debug("%s: Invalid, inconsistent types\n", __func__);
			type = PLUG_TYPE_INVALID;
			goto exit;
		}

		if (!d->swap_gnd && !d->mic_bias &&
		    (abs(minv - d->_vdces) > delta_thr ||
		     abs(maxv - d->_vdces) > delta_thr)) {
			pr_debug("%s: Invalid, delta %dmv, %dmv and %dmv\n",
				 __func__, d->_vdces, minv, maxv);
			type = PLUG_TYPE_INVALID;
			goto exit;
		} else if (d->swap_gnd) {
			dgnd = d;
		}

		if (!d->mic_bias && !d->swap_gnd)
			dprev = d;
		else if (d->mic_bias)
			dmicbias = d;
	}
	if (dgnd && dt->_type != PLUG_TYPE_HEADSET &&
	    dt->_type != dgnd->_type) {
		pr_debug("%s: Invalid, inconsistent types\n", __func__);
		type = PLUG_TYPE_INVALID;
		goto exit;
	}

	type = dt->_type;
	if (dmicbias) {
		if (dmicbias->_type == PLUG_TYPE_HEADSET &&
		    (dt->_type == PLUG_TYPE_HIGH_HPH ||
		     dt->_type == PLUG_TYPE_HEADSET)) {
			type = PLUG_TYPE_HEADSET;
			if (dt->_type == PLUG_TYPE_HIGH_HPH) {
				pr_debug("%s: Headset with threshold on MIC detected\n",
					 __func__);
				if (mbhc->mbhc_cfg->micbias_enable_flags &
				 (1 << MBHC_MICBIAS_ENABLE_THRESHOLD_HEADSET))
					mbhc->micbias_enable = true;
			}
		}
	}

	if (!(event_state & (1UL << MBHC_EVENT_PA_HPHL))) {
		if (((type == PLUG_TYPE_HEADSET ||
		      type == PLUG_TYPE_HEADPHONE) && ch != sz)) {
			pr_debug("%s: Invalid, not fully inserted, TYPE %d\n",
				 __func__, type);
			type = PLUG_TYPE_INVALID;
		}
	}
	if (type == PLUG_TYPE_HEADSET && dgnd && !dgnd->mic_bias) {
		if ((dgnd->_vdces + WCD9XXX_CS_GM_SWAP_THRES_MIN_MV <
		     minv) &&
		    (dgnd->_vdces + WCD9XXX_CS_GM_SWAP_THRES_MAX_MV >
		     maxv))
			type = PLUG_TYPE_GND_MIC_SWAP;
		else if (dgnd->_type != PLUG_TYPE_HEADSET && !dmicbias) {
			pr_debug("%s: Invalid, inconsistent types\n", __func__);
			type = PLUG_TYPE_INVALID;
		}
	}
exit:
	pr_debug("%s: Plug type %d detected\n", __func__, type);
	return type;
}

/*
 * wcd9xxx_find_plug_type : Find out and return the best plug type with given
 *			    list of wcd9xxx_mbhc_detect structure.
 * param mbhc wcd9xxx_mbhc structure
 * param dt collected measurements
 * param size array size of dt
 * param event_state mbhc->event_state when dt is collected
 */
static enum wcd9xxx_mbhc_plug_type
wcd9xxx_find_plug_type(struct wcd9xxx_mbhc *mbhc,
		       struct wcd9xxx_mbhc_detect *dt, const int size,
		       unsigned long event_state)
{
	int i;
	int ch;
	enum wcd9xxx_mbhc_plug_type type;
	int vdce;
	struct wcd9xxx_mbhc_detect *d, *dprev, *dgnd = NULL, *dvddio = NULL;
	int maxv = 0, minv = 0;
	const struct wcd9xxx_mbhc_plug_type_cfg *plug_type =
	    WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(mbhc->mbhc_cfg->calibration);
	const s16 hs_max = plug_type->v_hs_max;
	const s16 no_mic = plug_type->v_no_mic;

	pr_debug("%s: event_state 0x%lx\n", __func__, event_state);

	for (i = 0, d = dt, ch = 0; i < size; i++, d++) {
		vdce = wcd9xxx_codec_sta_dce_v(mbhc, true, d->dce);
		if (d->vddio)
			d->_vdces = scale_v_micb_vddio(mbhc, vdce, false);
		else
			d->_vdces = vdce;

		if (d->_vdces >= no_mic && d->_vdces < hs_max)
			d->_type = PLUG_TYPE_HEADSET;
		else if (d->_vdces < no_mic)
			d->_type = PLUG_TYPE_HEADPHONE;
		else
			d->_type = PLUG_TYPE_HIGH_HPH;

		ch += d->hphl_status & 0x01;
		if (!d->swap_gnd && !d->hwvalue && !d->vddio) {
			if (maxv < d->_vdces)
				maxv = d->_vdces;
			if (!minv || minv > d->_vdces)
				minv = d->_vdces;
		}

		pr_debug("%s: DCE #%d, %04x, V %04d(%04d), GND %d, VDDIO %d, HPHL %d TYPE %d\n",
			 __func__, i, d->dce, vdce, d->_vdces,
			 d->swap_gnd, d->vddio, d->hphl_status & 0x01,
			 d->_type);


		/*
		 * If GND and MIC prongs are aligned to HPHR and GND of
		 * headphone, codec measures the voltage based on
		 * impedance between HPHR and GND which results in ~80mv.
		 * Avoid this.
		 */
		if (d->_vdces >= WCD9XXX_MEAS_INVALD_RANGE_LOW_MV &&
		    d->_vdces <= WCD9XXX_MEAS_INVALD_RANGE_HIGH_MV) {
			pr_debug("%s: within invalid range\n", __func__);
			type = PLUG_TYPE_INVALID;
			goto exit;
		}
	}

	if (event_state & (1 << MBHC_EVENT_PA_HPHL)) {
		pr_debug("%s: HPHL PA was ON\n", __func__);
	} else if (ch != size && ch > 0) {
		pr_debug("%s: Invalid, inconsistent HPHL\n", __func__);
		type = PLUG_TYPE_INVALID;
		goto exit;
	}

	for (i = 0, dprev = NULL, d = dt; i < size; i++, d++) {
		if (d->vddio) {
			dvddio = d;
			continue;
		}

		if ((i > 0) && (d->_type != dprev->_type)) {
			pr_debug("%s: Invalid, inconsistent types\n", __func__);
			type = PLUG_TYPE_INVALID;
			goto exit;
		}

		if (!d->swap_gnd && !d->hwvalue &&
		    (abs(minv - d->_vdces) > WCD9XXX_MEAS_DELTA_MAX_MV ||
		     abs(maxv - d->_vdces) > WCD9XXX_MEAS_DELTA_MAX_MV)) {
			pr_debug("%s: Invalid, delta %dmv, %dmv and %dmv\n",
				 __func__, d->_vdces, minv, maxv);
			type = PLUG_TYPE_INVALID;
			goto exit;
		} else if (d->swap_gnd) {
			dgnd = d;
		}
		dprev = d;
	}

	WARN_ON(i != size);
	type = dt->_type;
	if (type == PLUG_TYPE_HEADSET && dgnd) {
		if ((dgnd->_vdces + WCD9XXX_GM_SWAP_THRES_MIN_MV <
		     minv) &&
		    (dgnd->_vdces + WCD9XXX_GM_SWAP_THRES_MAX_MV >
		     maxv))
			type = PLUG_TYPE_GND_MIC_SWAP;
	}

	/* if HPHL PA was on, we cannot use hphl status */
	if (!(event_state & (1UL << MBHC_EVENT_PA_HPHL))) {
		if (((type == PLUG_TYPE_HEADSET ||
		      type == PLUG_TYPE_HEADPHONE) && ch != size) ||
		    (type == PLUG_TYPE_GND_MIC_SWAP && ch)) {
			pr_debug("%s: Invalid, not fully inserted, TYPE %d\n",
				 __func__, type);
			type = PLUG_TYPE_INVALID;
		}
	}

	if (type == PLUG_TYPE_HEADSET && dvddio) {
		if ((dvddio->_vdces > hs_max) ||
		    (dvddio->_vdces > minv + WCD9XXX_THRESHOLD_MIC_THRESHOLD)) {
			pr_debug("%s: Headset with threshold on MIC detected\n",
				 __func__);
			if (mbhc->mbhc_cfg->micbias_enable_flags &
			    (1 << MBHC_MICBIAS_ENABLE_THRESHOLD_HEADSET))
				mbhc->micbias_enable = true;
		} else {
			pr_debug("%s: Headset with regular MIC detected\n",
				 __func__);
			if (mbhc->mbhc_cfg->micbias_enable_flags &
			    (1 << MBHC_MICBIAS_ENABLE_REGULAR_HEADSET))
				mbhc->micbias_enable = true;
		}
	}
exit:
	pr_debug("%s: Plug type %d detected, micbias_enable %d\n", __func__,
		 type, mbhc->micbias_enable);
	return type;
}

/*
 * Pull down MBHC micbias for provided duration in microsecond.
 */
static int wcd9xxx_pull_down_micbias(struct wcd9xxx_mbhc *mbhc, int us)
{
	bool micbiasconn = false;
	struct snd_soc_codec *codec = mbhc->codec;
	const u16 ctlreg = mbhc->mbhc_bias_regs.ctl_reg;

	/*
	 * Disable MBHC to micbias connection to pull down
	 * micbias and pull down micbias for a moment.
	 */
	if ((snd_soc_read(mbhc->codec, ctlreg) & 0x01)) {
		WARN_ONCE(1, "MBHC micbias is already pulled down unexpectedly\n");
		return -EFAULT;
	}

	if ((snd_soc_read(mbhc->codec, WCD9XXX_A_MAD_ANA_CTRL) & 1 << 4)) {
		snd_soc_update_bits(mbhc->codec, WCD9XXX_A_MAD_ANA_CTRL,
				    1 << 4, 0);
		micbiasconn = true;
	}

	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x01, 0x01);

	/*
	 * Pull down for 1ms to discharge bias. Give small margin (10us) to be
	 * able to get consistent result across DCEs.
	 */
	usleep_range(1000, 1000 + 10);

	if (micbiasconn)
		snd_soc_update_bits(mbhc->codec, WCD9XXX_A_MAD_ANA_CTRL,
				    1 << 4, 1 << 4);
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x01, 0x00);
	usleep_range(us, us + WCD9XXX_USLEEP_RANGE_MARGIN_US);

	return 0;
}

void wcd9xxx_turn_onoff_current_source(struct wcd9xxx_mbhc *mbhc, bool on,
				       bool highhph)
{

	struct snd_soc_codec *codec;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_det;
	const struct wcd9xxx_mbhc_plug_detect_cfg *plug_det =
	    WCD9XXX_MBHC_CAL_PLUG_DET_PTR(mbhc->mbhc_cfg->calibration);

	btn_det = WCD9XXX_MBHC_CAL_BTN_DET_PTR(mbhc->mbhc_cfg->calibration);
	codec = mbhc->codec;

	if (on) {
		pr_debug("%s: enabling current source\n", __func__);
		/* Nsc to 9 */
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL,
				    0x78, 0x48);
		/* pull down diode bit to 0 */
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg,
				    0x01, 0x00);
		/*
		 * Keep the low power insertion/removal
		 * detection (reg 0x3DD) disabled
		 */
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_INT_CTL,
				    0x01, 0x00);
		/*
		 * Enable the Mic Bias current source
		 * Write bits[6:5] of register MICB_2_MBHC to 0x3 (V_20_UA)
		 * Write bit[7] of register MICB_2_MBHC to 1
		 * (INS_DET_ISRC_EN__ENABLE)
		 * MICB_2_MBHC__SCHT_TRIG_EN to 1
		 */
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg,
				    0xF0, 0xF0);
		/* Disconnect MBHC Override from MicBias and LDOH */
		snd_soc_update_bits(codec, WCD9XXX_A_MAD_ANA_CTRL, 0x10, 0x00);
	} else {
		pr_debug("%s: disabling current source\n", __func__);
		/* Connect MBHC Override from MicBias and LDOH */
		snd_soc_update_bits(codec, WCD9XXX_A_MAD_ANA_CTRL, 0x10, 0x10);
		/* INS_DET_ISRC_CTL to acdb value */
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg,
				    0x60, plug_det->mic_current << 5);
		if (!highhph) {
			/* INS_DET_ISRC_EN__ENABLE to 0 */
			snd_soc_update_bits(codec,
					    mbhc->mbhc_bias_regs.mbhc_reg,
					    0x80, 0x00);
			/* MICB_2_MBHC__SCHT_TRIG_EN  to 0 */
			snd_soc_update_bits(codec,
					    mbhc->mbhc_bias_regs.mbhc_reg,
					    0x10, 0x00);
		}
		/* Nsc to acdb value */
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x78,
				    btn_det->mbhc_nsc << 3);
	}
}

static enum wcd9xxx_mbhc_plug_type
wcd9xxx_codec_cs_get_plug_type(struct wcd9xxx_mbhc *mbhc, bool highhph)
{
	struct snd_soc_codec *codec = mbhc->codec;
	struct wcd9xxx_mbhc_detect rt[NUM_DCE_PLUG_INS_DETECT];
	enum wcd9xxx_mbhc_plug_type type = PLUG_TYPE_INVALID;
	int i;

	pr_debug("%s: enter\n", __func__);
	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);

	BUG_ON(NUM_DCE_PLUG_INS_DETECT < 4);

	rt[0].swap_gnd = false;
	rt[0].vddio = false;
	rt[0].hwvalue = true;
	rt[0].hphl_status = wcd9xxx_hphl_status(mbhc);
	rt[0].dce = wcd9xxx_mbhc_setup_hs_polling(mbhc, true);
	rt[0].mic_bias = false;

	for (i = 1; i < NUM_DCE_PLUG_INS_DETECT - 1; i++) {
		rt[i].swap_gnd = (i == NUM_DCE_PLUG_INS_DETECT - 3);
		rt[i].mic_bias = ((i == NUM_DCE_PLUG_INS_DETECT - 4) &&
				   highhph);
		rt[i].hphl_status = wcd9xxx_hphl_status(mbhc);
		if (rt[i].swap_gnd)
			wcd9xxx_codec_hphr_gnd_switch(codec, true);

		if (rt[i].mic_bias)
			wcd9xxx_turn_onoff_current_source(mbhc, false, false);

		rt[i].dce = __wcd9xxx_codec_sta_dce(mbhc, 1, !highhph, true);
		if (rt[i].mic_bias)
			wcd9xxx_turn_onoff_current_source(mbhc, true, false);
		if (rt[i].swap_gnd)
			wcd9xxx_codec_hphr_gnd_switch(codec, false);
	}

	type = wcd9xxx_cs_find_plug_type(mbhc, rt, ARRAY_SIZE(rt), highhph,
					 mbhc->event_state);

	pr_debug("%s: plug_type:%d\n", __func__, type);

	return type;
}

static enum wcd9xxx_mbhc_plug_type
wcd9xxx_codec_get_plug_type(struct wcd9xxx_mbhc *mbhc, bool highhph)
{
	int i;
	bool vddioon;
	struct wcd9xxx_mbhc_plug_type_cfg *plug_type_ptr;
	struct wcd9xxx_mbhc_detect rt[NUM_DCE_PLUG_INS_DETECT];
	enum wcd9xxx_mbhc_plug_type type = PLUG_TYPE_INVALID;
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);
	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);

	/* make sure override is on */
	WARN_ON(!(snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B1_CTL) & 0x04));

	/* GND and MIC swap detection requires at least 2 rounds of DCE */
	BUG_ON(NUM_DCE_PLUG_INS_DETECT < 2);

	/*
	 * There are chances vddio switch is on and cfilt voltage is adjusted
	 * to vddio voltage even after plug type removal reported.
	 */
	vddioon = __wcd9xxx_switch_micbias(mbhc, 0, false, false);
	pr_debug("%s: vddio switch was %s\n", __func__, vddioon ? "on" : "off");

	plug_type_ptr =
	    WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(mbhc->mbhc_cfg->calibration);

	/*
	 * cfilter in fast mode requires 1ms to charge up and down micbias
	 * fully.
	 */
	(void) wcd9xxx_pull_down_micbias(mbhc,
					 WCD9XXX_MICBIAS_PULLDOWN_SETTLE_US);
	rt[0].hphl_status = wcd9xxx_hphl_status(mbhc);
	rt[0].dce = wcd9xxx_mbhc_setup_hs_polling(mbhc, false);
	rt[0].swap_gnd = false;
	rt[0].vddio = false;
	rt[0].hwvalue = true;
	for (i = 1; i < NUM_DCE_PLUG_INS_DETECT; i++) {
		rt[i].swap_gnd = (i == NUM_DCE_PLUG_INS_DETECT - 2);
		if (detect_use_vddio_switch)
			rt[i].vddio = (i == 1);
		else
			rt[i].vddio = false;
		rt[i].hphl_status = wcd9xxx_hphl_status(mbhc);
		rt[i].hwvalue = false;
		if (rt[i].swap_gnd)
			wcd9xxx_codec_hphr_gnd_switch(codec, true);
		if (rt[i].vddio)
			wcd9xxx_onoff_vddio_switch(mbhc, true);
		/*
		 * Pull down micbias to detect headset with mic which has
		 * threshold and to have more consistent voltage measurements.
		 *
		 * cfilter in fast mode requires 1ms to charge up and down
		 * micbias fully.
		 */
		(void) wcd9xxx_pull_down_micbias(mbhc,
					    WCD9XXX_MICBIAS_PULLDOWN_SETTLE_US);
		rt[i].dce = __wcd9xxx_codec_sta_dce(mbhc, 1, true, true);
		if (rt[i].vddio)
			wcd9xxx_onoff_vddio_switch(mbhc, false);
		if (rt[i].swap_gnd)
			wcd9xxx_codec_hphr_gnd_switch(codec, false);
	}

	if (vddioon)
		__wcd9xxx_switch_micbias(mbhc, 1, false, false);

	type = wcd9xxx_find_plug_type(mbhc, rt, ARRAY_SIZE(rt),
				      mbhc->event_state);

	pr_debug("%s: leave\n", __func__);
	return type;
}

static bool wcd9xxx_swch_level_remove(struct wcd9xxx_mbhc *mbhc)
{
	if (mbhc->mbhc_cfg->gpio)
		return (gpio_get_value_cansleep(mbhc->mbhc_cfg->gpio) !=
			mbhc->mbhc_cfg->gpio_level_insert);
	else if (mbhc->mbhc_cfg->insert_detect)
		return snd_soc_read(mbhc->codec,
				    WCD9XXX_A_MBHC_INSERT_DET_STATUS) &
				    (1 << 2);
	else
		WARN(1, "Invalid jack detection configuration\n");

	return true;
}

static bool is_clk_active(struct snd_soc_codec *codec)
{
	return !!(snd_soc_read(codec, WCD9XXX_A_CDC_CLK_MCLK_CTL) & 0x05);
}

static int wcd9xxx_enable_hs_detect(struct wcd9xxx_mbhc *mbhc,
				    int insertion, int trigger, bool padac_off)
{
	struct snd_soc_codec *codec = mbhc->codec;
	int central_bias_enabled = 0;
	const struct wcd9xxx_mbhc_general_cfg *generic =
	    WCD9XXX_MBHC_CAL_GENERAL_PTR(mbhc->mbhc_cfg->calibration);
	const struct wcd9xxx_mbhc_plug_detect_cfg *plug_det =
	    WCD9XXX_MBHC_CAL_PLUG_DET_PTR(mbhc->mbhc_cfg->calibration);

	pr_debug("%s: enter insertion(%d) trigger(0x%x)\n",
		 __func__, insertion, trigger);

	if (!mbhc->mbhc_cfg->calibration) {
		pr_err("Error, no wcd9xxx calibration\n");
		return -EINVAL;
	}

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_INT_CTL, 0x1, 0);

	/*
	 * Make sure mic bias and Mic line schmitt trigger
	 * are turned OFF
	 */
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x01, 0x01);
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);

	if (insertion) {
		wcd9xxx_switch_micbias(mbhc, 0);

		/* DAPM can manipulate PA/DAC bits concurrently */
		if (padac_off == true)
			wcd9xxx_set_and_turnoff_hph_padac(mbhc);

		if (trigger & MBHC_USE_HPHL_TRIGGER) {
			/* Enable HPH Schmitt Trigger */
			snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x11,
					0x11);
			snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x0C,
					plug_det->hph_current << 2);
			snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x02,
					0x02);
		}
		if (trigger & MBHC_USE_MB_TRIGGER) {
			/* enable the mic line schmitt trigger */
			snd_soc_update_bits(codec,
					mbhc->mbhc_bias_regs.mbhc_reg,
					0x60, plug_det->mic_current << 5);
			snd_soc_update_bits(codec,
					mbhc->mbhc_bias_regs.mbhc_reg,
					0x80, 0x80);
			usleep_range(plug_det->t_mic_pid, plug_det->t_mic_pid);
			snd_soc_update_bits(codec,
					mbhc->mbhc_bias_regs.ctl_reg, 0x01,
					0x00);
			snd_soc_update_bits(codec,
					mbhc->mbhc_bias_regs.mbhc_reg,
					0x10, 0x10);
		}

		/* setup for insetion detection */
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_INT_CTL, 0x2, 0);
	} else {
		pr_debug("setup for removal detection\n");
		/* Make sure the HPH schmitt trigger is OFF */
		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x12, 0x00);

		/* enable the mic line schmitt trigger */
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg,
				    0x01, 0x00);
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg, 0x60,
				    plug_det->mic_current << 5);
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg,
				    0x80, 0x80);
		usleep_range(plug_det->t_mic_pid, plug_det->t_mic_pid);
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg,
				    0x10, 0x10);

		/* Setup for low power removal detection */
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_INT_CTL, 0x2,
				    0x2);
	}

	if (snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B1_CTL) & 0x4) {
		/* called by interrupt */
		if (!is_clk_active(codec)) {
			wcd9xxx_resmgr_enable_config_mode(mbhc->resmgr, 1);
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL,
					0x06, 0);
			usleep_range(generic->t_shutdown_plug_rem,
					generic->t_shutdown_plug_rem);
			wcd9xxx_resmgr_enable_config_mode(mbhc->resmgr, 0);
		} else
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL,
					0x06, 0);
	}

	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.int_rbias, 0x80, 0);

	/* If central bandgap disabled */
	if (!(snd_soc_read(codec, WCD9XXX_A_PIN_CTL_OE1) & 1)) {
		snd_soc_update_bits(codec, WCD9XXX_A_PIN_CTL_OE1, 0x3, 0x3);
		usleep_range(generic->t_bg_fast_settle,
			     generic->t_bg_fast_settle);
		central_bias_enabled = 1;
	}

	/* If LDO_H disabled */
	if (snd_soc_read(codec, WCD9XXX_A_PIN_CTL_OE0) & 0x80) {
		snd_soc_update_bits(codec, WCD9XXX_A_PIN_CTL_OE0, 0x10, 0);
		snd_soc_update_bits(codec, WCD9XXX_A_PIN_CTL_OE0, 0x80, 0x80);
		usleep_range(generic->t_ldoh, generic->t_ldoh);
		snd_soc_update_bits(codec, WCD9XXX_A_PIN_CTL_OE0, 0x80, 0);

		if (central_bias_enabled)
			snd_soc_update_bits(codec, WCD9XXX_A_PIN_CTL_OE1, 0x1,
					    0);
	}

	if (mbhc->resmgr->reg_addr && mbhc->resmgr->reg_addr->micb_4_mbhc)
		snd_soc_update_bits(codec, mbhc->resmgr->reg_addr->micb_4_mbhc,
				    0x3, mbhc->mbhc_cfg->micbias);

	wcd9xxx_enable_irq(mbhc->resmgr->core_res, WCD9XXX_IRQ_MBHC_INSERTION);
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_INT_CTL, 0x1, 0x1);
	pr_debug("%s: leave\n", __func__);

	return 0;
}

/* called under codec_resource_lock acquisition */
static void wcd9xxx_find_plug_and_report(struct wcd9xxx_mbhc *mbhc,
					 enum wcd9xxx_mbhc_plug_type plug_type)
{
	pr_debug("%s: enter current_plug(%d) new_plug(%d)\n",
		 __func__, mbhc->current_plug, plug_type);

	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);

	if (plug_type == PLUG_TYPE_HEADPHONE &&
	    mbhc->current_plug == PLUG_TYPE_NONE) {
		/*
		 * Nothing was reported previously
		 * report a headphone or unsupported
		 */
		wcd9xxx_report_plug(mbhc, 1, SND_JACK_HEADPHONE);
		wcd9xxx_cleanup_hs_polling(mbhc);
	} else if (plug_type == PLUG_TYPE_GND_MIC_SWAP) {
		if (!mbhc->mbhc_cfg->detect_extn_cable) {
			if (mbhc->current_plug == PLUG_TYPE_HEADSET)
				wcd9xxx_report_plug(mbhc, 0,
							 SND_JACK_HEADSET);
			else if (mbhc->current_plug == PLUG_TYPE_HEADPHONE)
				wcd9xxx_report_plug(mbhc, 0,
							 SND_JACK_HEADPHONE);
		}
		wcd9xxx_report_plug(mbhc, 1, SND_JACK_UNSUPPORTED);
		wcd9xxx_cleanup_hs_polling(mbhc);
	} else if (plug_type == PLUG_TYPE_HEADSET) {
		/*
		 * If Headphone was reported previously, this will
		 * only report the mic line
		 */
		wcd9xxx_report_plug(mbhc, 1, SND_JACK_HEADSET);
		msleep(100);

		/* if PA is already on, switch micbias source to VDDIO */
		if (mbhc->event_state &
		    (1 << MBHC_EVENT_PA_HPHL | 1 << MBHC_EVENT_PA_HPHR))
			__wcd9xxx_switch_micbias(mbhc, 1, false, false);
		wcd9xxx_start_hs_polling(mbhc);
	} else if (plug_type == PLUG_TYPE_HIGH_HPH) {
		if (mbhc->mbhc_cfg->detect_extn_cable) {
			/* High impedance device found. Report as LINEOUT*/
			wcd9xxx_report_plug(mbhc, 1, SND_JACK_LINEOUT);
			wcd9xxx_cleanup_hs_polling(mbhc);
			pr_debug("%s: setup mic trigger for further detection\n",
				 __func__);
			mbhc->lpi_enabled = true;
			/*
			 * Do not enable HPHL trigger. If playback is active,
			 * it might lead to continuous false HPHL triggers
			 */
			wcd9xxx_enable_hs_detect(mbhc, 1, MBHC_USE_MB_TRIGGER,
						 false);
		} else {
			if (mbhc->current_plug == PLUG_TYPE_NONE)
				wcd9xxx_report_plug(mbhc, 1,
							 SND_JACK_HEADPHONE);
			wcd9xxx_cleanup_hs_polling(mbhc);
			pr_debug("setup mic trigger for further detection\n");
			mbhc->lpi_enabled = true;
			wcd9xxx_enable_hs_detect(mbhc, 1, MBHC_USE_MB_TRIGGER |
							  MBHC_USE_HPHL_TRIGGER,
						 false);
		}
	} else {
		WARN(1, "Unexpected current plug_type %d, plug_type %d\n",
		     mbhc->current_plug, plug_type);
	}
	pr_debug("%s: leave\n", __func__);
}

/* called under codec_resource_lock acquisition */
static void wcd9xxx_mbhc_decide_swch_plug(struct wcd9xxx_mbhc *mbhc)
{
	enum wcd9xxx_mbhc_plug_type plug_type;
	bool current_source_enable;

	pr_debug("%s: enter\n", __func__);

	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);
	current_source_enable = ((mbhc->mbhc_cfg->cs_enable_flags &
				  (1 << MBHC_CS_ENABLE_INSERTION)) != 0);

	if (current_source_enable) {
		wcd9xxx_turn_onoff_current_source(mbhc, true, false);
		plug_type = wcd9xxx_codec_cs_get_plug_type(mbhc, false);
		wcd9xxx_turn_onoff_current_source(mbhc, false, false);
	} else {
		wcd9xxx_turn_onoff_override(mbhc, true);
		plug_type = wcd9xxx_codec_get_plug_type(mbhc, true);
		wcd9xxx_turn_onoff_override(mbhc, false);
	}

	if (wcd9xxx_swch_level_remove(mbhc)) {
		pr_debug("%s: Switch level is low when determining plug\n",
			 __func__);
		return;
	}

	if (plug_type == PLUG_TYPE_INVALID ||
	    plug_type == PLUG_TYPE_GND_MIC_SWAP) {
		wcd9xxx_cleanup_hs_polling(mbhc);
		wcd9xxx_schedule_hs_detect_plug(mbhc,
						&mbhc->correct_plug_swch);
	} else if (plug_type == PLUG_TYPE_HEADPHONE) {
		wcd9xxx_report_plug(mbhc, 1, SND_JACK_HEADPHONE);
		wcd9xxx_cleanup_hs_polling(mbhc);
		wcd9xxx_schedule_hs_detect_plug(mbhc,
						&mbhc->correct_plug_swch);
	} else if (plug_type == PLUG_TYPE_HIGH_HPH) {
		wcd9xxx_cleanup_hs_polling(mbhc);
		wcd9xxx_schedule_hs_detect_plug(mbhc,
						&mbhc->correct_plug_swch);
	} else {
		pr_debug("%s: Valid plug found, determine plug type %d\n",
			 __func__, plug_type);
		wcd9xxx_find_plug_and_report(mbhc, plug_type);
	}
	pr_debug("%s: leave\n", __func__);
}

/* called under codec_resource_lock acquisition */
static void wcd9xxx_mbhc_detect_plug_type(struct wcd9xxx_mbhc *mbhc)
{
	pr_debug("%s: enter\n", __func__);
	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);

	if (wcd9xxx_swch_level_remove(mbhc))
		pr_debug("%s: Switch level low when determining plug\n",
			 __func__);
	else
		wcd9xxx_mbhc_decide_swch_plug(mbhc);
	pr_debug("%s: leave\n", __func__);
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void wcd9xxx_hs_insert_irq_swch(struct wcd9xxx_mbhc *mbhc,
				       bool is_removal)
{
	if (!is_removal) {
		pr_debug("%s: MIC trigger insertion interrupt\n", __func__);

		rmb();
		if (mbhc->lpi_enabled)
			msleep(100);

		rmb();
		if (!mbhc->lpi_enabled) {
			pr_debug("%s: lpi is disabled\n", __func__);
		} else if (!wcd9xxx_swch_level_remove(mbhc)) {
			pr_debug("%s: Valid insertion, detect plug type\n",
				 __func__);
			wcd9xxx_mbhc_decide_swch_plug(mbhc);
		} else {
			pr_debug("%s: Invalid insertion stop plug detection\n",
				 __func__);
		}
	} else if (mbhc->mbhc_cfg->detect_extn_cable) {
		pr_debug("%s: Removal\n", __func__);
		if (!wcd9xxx_swch_level_remove(mbhc)) {
			/*
			 * Switch indicates, something is still inserted.
			 * This could be extension cable i.e. headset is
			 * removed from extension cable.
			 */
			/* cancel detect plug */
			wcd9xxx_cancel_hs_detect_plug(mbhc,
						      &mbhc->correct_plug_swch);
			wcd9xxx_mbhc_decide_swch_plug(mbhc);
		}
	} else {
		pr_err("%s: Switch IRQ used, invalid MBHC Removal\n", __func__);
	}
}

static bool is_valid_mic_voltage(struct wcd9xxx_mbhc *mbhc, s32 mic_mv,
				 bool cs_enable)
{
	const struct wcd9xxx_mbhc_plug_type_cfg *plug_type =
	    WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(mbhc->mbhc_cfg->calibration);
	const s16 v_hs_max = wcd9xxx_get_current_v_hs_max(mbhc);

	if (cs_enable)
		return ((mic_mv > WCD9XXX_V_CS_NO_MIC) &&
			 (mic_mv < WCD9XXX_V_CS_HS_MAX)) ? true : false;
	else
		return (!(mic_mv > WCD9XXX_MEAS_INVALD_RANGE_LOW_MV &&
			  mic_mv < WCD9XXX_MEAS_INVALD_RANGE_HIGH_MV) &&
			(mic_mv > plug_type->v_no_mic) &&
			(mic_mv < v_hs_max)) ? true : false;
}

/*
 * called under codec_resource_lock acquisition
 * returns true if mic voltage range is back to normal insertion
 * returns false either if timedout or removed
 */
static bool wcd9xxx_hs_remove_settle(struct wcd9xxx_mbhc *mbhc)
{
	int i;
	bool timedout, settled = false;
	s32 mic_mv[NUM_DCE_PLUG_DETECT];
	short mb_v[NUM_DCE_PLUG_DETECT];
	unsigned long retry = 0, timeout;
	bool cs_enable;

	cs_enable = ((mbhc->mbhc_cfg->cs_enable_flags &
		      (1 << MBHC_CS_ENABLE_REMOVAL)) != 0);

	if (cs_enable)
		wcd9xxx_turn_onoff_current_source(mbhc, true, false);

	timeout = jiffies + msecs_to_jiffies(HS_DETECT_PLUG_TIME_MS);
	while (!(timedout = time_after(jiffies, timeout))) {
		retry++;
		if (wcd9xxx_swch_level_remove(mbhc)) {
			pr_debug("%s: Switch indicates removal\n", __func__);
			break;
		}

		if (retry > 1)
			msleep(250);
		else
			msleep(50);

		if (wcd9xxx_swch_level_remove(mbhc)) {
			pr_debug("%s: Switch indicates removal\n", __func__);
			break;
		}

		if (cs_enable) {
			for (i = 0; i < NUM_DCE_PLUG_DETECT; i++) {
				mb_v[i] = __wcd9xxx_codec_sta_dce(mbhc, 1,
								  true, true);
				mic_mv[i] = __wcd9xxx_codec_sta_dce_v(mbhc,
								      true,
								      mb_v[i],
						mbhc->mbhc_data.dce_nsc_cs_z,
						(u32)VDDIO_MICBIAS_MV);
				pr_debug("%s : DCE run %lu, mic_mv = %d(%x)\n",
					 __func__, retry, mic_mv[i], mb_v[i]);
			}
		} else {
			for (i = 0; i < NUM_DCE_PLUG_DETECT; i++) {
				mb_v[i] = wcd9xxx_codec_sta_dce(mbhc, 1,
								true);
				mic_mv[i] = wcd9xxx_codec_sta_dce_v(mbhc, 1,
								mb_v[i]);
				pr_debug("%s : DCE run %lu, mic_mv = %d(%x)\n",
					 __func__, retry, mic_mv[i],
								mb_v[i]);
			}
		}

		if (wcd9xxx_swch_level_remove(mbhc)) {
			pr_debug("%s: Switcn indicates removal\n", __func__);
			break;
		}

		if (mbhc->current_plug == PLUG_TYPE_NONE) {
			pr_debug("%s : headset/headphone is removed\n",
				 __func__);
			break;
		}

		for (i = 0; i < NUM_DCE_PLUG_DETECT; i++)
			if (!is_valid_mic_voltage(mbhc, mic_mv[i], cs_enable))
				break;

		if (i == NUM_DCE_PLUG_DETECT) {
			pr_debug("%s: MIC voltage settled\n", __func__);
			settled = true;
			msleep(200);
			break;
		}
	}

	if (cs_enable)
		wcd9xxx_turn_onoff_current_source(mbhc, false, false);

	if (timedout)
		pr_debug("%s: Microphone did not settle in %d seconds\n",
			 __func__, HS_DETECT_PLUG_TIME_MS);
	return settled;
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void wcd9xxx_hs_remove_irq_swch(struct wcd9xxx_mbhc *mbhc)
{
	pr_debug("%s: enter\n", __func__);
	if (wcd9xxx_hs_remove_settle(mbhc))
		wcd9xxx_start_hs_polling(mbhc);
	pr_debug("%s: leave\n", __func__);
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void wcd9xxx_hs_remove_irq_noswch(struct wcd9xxx_mbhc *mbhc)
{
	s16 dce, dcez;
	unsigned long timeout;
	bool removed = true;
	struct snd_soc_codec *codec = mbhc->codec;
	const struct wcd9xxx_mbhc_general_cfg *generic =
		WCD9XXX_MBHC_CAL_GENERAL_PTR(mbhc->mbhc_cfg->calibration);
	bool cs_enable;
	s16 cur_v_ins_h;
	u32 mb_mv;

	pr_debug("%s: enter\n", __func__);
	if (mbhc->current_plug != PLUG_TYPE_HEADSET) {
		pr_debug("%s(): Headset is not inserted, ignore removal\n",
			 __func__);
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL,
				0x08, 0x08);
		return;
	}

	usleep_range(generic->t_shutdown_plug_rem,
		     generic->t_shutdown_plug_rem);

	/* If micbias is enabled, don't enable current source */
	cs_enable = (((mbhc->mbhc_cfg->cs_enable_flags &
		      (1 << MBHC_CS_ENABLE_REMOVAL)) != 0) &&
		     (!(snd_soc_read(codec,
				     mbhc->mbhc_bias_regs.ctl_reg) & 0x80)));
	if (cs_enable)
		wcd9xxx_turn_onoff_current_source(mbhc, true, false);

	timeout = jiffies + msecs_to_jiffies(FAKE_REMOVAL_MIN_PERIOD_MS);
	do {
		if (cs_enable) {
			dce = __wcd9xxx_codec_sta_dce(mbhc, 1,  true, true);
			dcez = mbhc->mbhc_data.dce_nsc_cs_z;
			mb_mv = VDDIO_MICBIAS_MV;
		} else {
			dce = wcd9xxx_codec_sta_dce(mbhc, 1,  true);
			dcez = mbhc->mbhc_data.dce_z;
			mb_mv = mbhc->mbhc_data.micb_mv;
		}

		pr_debug("%s: DCE 0x%x,%d\n", __func__, dce,
			  __wcd9xxx_codec_sta_dce_v(mbhc, true, dce,
						    dcez, mb_mv));

		cur_v_ins_h = cs_enable ? (s16) mbhc->mbhc_data.v_cs_ins_h :
					  (wcd9xxx_get_current_v(mbhc,
					   WCD9XXX_CURRENT_V_INS_H));

		if (dce < cur_v_ins_h) {
			removed = false;
			break;
		}
	} while (!time_after(jiffies, timeout));
	pr_debug("%s: headset %sactually removed\n", __func__,
		  removed ? "" : "not ");

	if (cs_enable)
		wcd9xxx_turn_onoff_current_source(mbhc, false, false);

	if (removed) {
		if (mbhc->mbhc_cfg->detect_extn_cable) {
			if (!wcd9xxx_swch_level_remove(mbhc)) {
				/*
				 * extension cable is still plugged in
				 * report it as LINEOUT device
				 */
				wcd9xxx_report_plug(mbhc, 1, SND_JACK_LINEOUT);
				wcd9xxx_cleanup_hs_polling(mbhc);
				wcd9xxx_enable_hs_detect(mbhc, 1,
							 MBHC_USE_MB_TRIGGER,
							 false);
			}
		} else {
			/* Cancel possibly running hs_detect_work */
			wcd9xxx_cancel_hs_detect_plug(mbhc,
						    &mbhc->correct_plug_noswch);
			/*
			 * If this removal is not false, first check the micbias
			 * switch status and switch it to LDOH if it is already
			 * switched to VDDIO.
			 */
			wcd9xxx_switch_micbias(mbhc, 0);

			wcd9xxx_report_plug(mbhc, 0, SND_JACK_HEADSET);
			wcd9xxx_cleanup_hs_polling(mbhc);
			wcd9xxx_enable_hs_detect(mbhc, 1, MBHC_USE_MB_TRIGGER |
							  MBHC_USE_HPHL_TRIGGER,
						 true);
		}
	} else {
		wcd9xxx_start_hs_polling(mbhc);
	}
	pr_debug("%s: leave\n", __func__);
}

/* called only from interrupt which is under codec_resource_lock acquisition */
static void wcd9xxx_hs_insert_irq_extn(struct wcd9xxx_mbhc *mbhc,
				       bool is_mb_trigger)
{
	/* Cancel possibly running hs_detect_work */
	wcd9xxx_cancel_hs_detect_plug(mbhc, &mbhc->correct_plug_swch);

	if (is_mb_trigger) {
		pr_debug("%s: Waiting for Headphone left trigger\n", __func__);
		wcd9xxx_enable_hs_detect(mbhc, 1, MBHC_USE_HPHL_TRIGGER, false);
	} else  {
		pr_debug("%s: HPHL trigger received, detecting plug type\n",
			 __func__);
		wcd9xxx_mbhc_detect_plug_type(mbhc);
	}
}

static irqreturn_t wcd9xxx_hs_remove_irq(int irq, void *data)
{
	struct wcd9xxx_mbhc *mbhc = data;

	pr_debug("%s: enter, removal interrupt\n", __func__);
	WCD9XXX_BCL_LOCK(mbhc->resmgr);
	/*
	 * While we don't know whether MIC is there or not, let the resmgr know
	 * so micbias can be disabled temporarily
	 */
	if (mbhc->current_plug == PLUG_TYPE_HEADSET) {
		wcd9xxx_resmgr_cond_update_cond(mbhc->resmgr,
						WCD9XXX_COND_HPH_MIC, false);
		wcd9xxx_resmgr_cond_update_cond(mbhc->resmgr,
						WCD9XXX_COND_HPH, false);
	} else if (mbhc->current_plug == PLUG_TYPE_HEADPHONE) {
		wcd9xxx_resmgr_cond_update_cond(mbhc->resmgr,
						WCD9XXX_COND_HPH, false);
	}

	if (mbhc->mbhc_cfg->detect_extn_cable &&
	    !wcd9xxx_swch_level_remove(mbhc))
		wcd9xxx_hs_remove_irq_noswch(mbhc);
	else
		wcd9xxx_hs_remove_irq_swch(mbhc);

	if (mbhc->current_plug == PLUG_TYPE_HEADSET) {
		wcd9xxx_resmgr_cond_update_cond(mbhc->resmgr,
						WCD9XXX_COND_HPH, true);
		wcd9xxx_resmgr_cond_update_cond(mbhc->resmgr,
						WCD9XXX_COND_HPH_MIC, true);
	} else if (mbhc->current_plug == PLUG_TYPE_HEADPHONE) {
		wcd9xxx_resmgr_cond_update_cond(mbhc->resmgr,
						WCD9XXX_COND_HPH, true);
	}
	WCD9XXX_BCL_UNLOCK(mbhc->resmgr);

	return IRQ_HANDLED;
}

static irqreturn_t wcd9xxx_hs_insert_irq(int irq, void *data)
{
	bool is_mb_trigger, is_removal;
	struct wcd9xxx_mbhc *mbhc = data;
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);
	WCD9XXX_BCL_LOCK(mbhc->resmgr);
	wcd9xxx_disable_irq(mbhc->resmgr->core_res, WCD9XXX_IRQ_MBHC_INSERTION);

	is_mb_trigger = !!(snd_soc_read(codec, mbhc->mbhc_bias_regs.mbhc_reg) &
			   0x10);
	is_removal = !!(snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_INT_CTL) & 0x02);
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_INT_CTL, 0x03, 0x00);

	/* Turn off both HPH and MIC line schmitt triggers */
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);
	snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x13, 0x00);
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x01, 0x00);

	if (mbhc->mbhc_cfg->detect_extn_cable &&
	    mbhc->current_plug == PLUG_TYPE_HIGH_HPH)
		wcd9xxx_hs_insert_irq_extn(mbhc, is_mb_trigger);
	else
		wcd9xxx_hs_insert_irq_swch(mbhc, is_removal);

	WCD9XXX_BCL_UNLOCK(mbhc->resmgr);
	return IRQ_HANDLED;
}

static void wcd9xxx_btn_lpress_fn(struct work_struct *work)
{
	struct delayed_work *dwork;
	short bias_value;
	int dce_mv, sta_mv;
	struct wcd9xxx_mbhc *mbhc;

	pr_debug("%s:\n", __func__);

	dwork = to_delayed_work(work);
	mbhc = container_of(dwork, struct wcd9xxx_mbhc, mbhc_btn_dwork);

	bias_value = wcd9xxx_read_sta_result(mbhc->codec);
	sta_mv = wcd9xxx_codec_sta_dce_v(mbhc, 0, bias_value);

	bias_value = wcd9xxx_read_dce_result(mbhc->codec);
	dce_mv = wcd9xxx_codec_sta_dce_v(mbhc, 1, bias_value);
	pr_debug("%s: STA: %d, DCE: %d\n", __func__, sta_mv, dce_mv);

	pr_debug("%s: Reporting long button press event\n", __func__);
	wcd9xxx_jack_report(mbhc, &mbhc->button_jack, mbhc->buttons_pressed,
			    mbhc->buttons_pressed);

	pr_debug("%s: leave\n", __func__);
	wcd9xxx_unlock_sleep(mbhc->resmgr->core_res);
}

static void wcd9xxx_mbhc_insert_work(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct wcd9xxx_mbhc *mbhc;
	struct snd_soc_codec *codec;
	struct wcd9xxx_core_resource *core_res;

	dwork = to_delayed_work(work);
	mbhc = container_of(dwork, struct wcd9xxx_mbhc, mbhc_insert_dwork);
	codec = mbhc->codec;
	core_res = mbhc->resmgr->core_res;

	pr_debug("%s:\n", __func__);

	/* Turn off both HPH and MIC line schmitt triggers */
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg, 0x90, 0x00);
	snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x13, 0x00);
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x01, 0x00);
	wcd9xxx_disable_irq_sync(core_res, WCD9XXX_IRQ_MBHC_INSERTION);
	wcd9xxx_mbhc_detect_plug_type(mbhc);
	wcd9xxx_unlock_sleep(core_res);
}

static bool wcd9xxx_mbhc_fw_validate(const struct firmware *fw)
{
	u32 cfg_offset;
	struct wcd9xxx_mbhc_imped_detect_cfg *imped_cfg;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_cfg;

	if (fw->size < WCD9XXX_MBHC_CAL_MIN_SIZE)
		return false;

	/*
	 * Previous check guarantees that there is enough fw data up
	 * to num_btn
	 */
	btn_cfg = WCD9XXX_MBHC_CAL_BTN_DET_PTR(fw->data);
	cfg_offset = (u32) ((void *) btn_cfg - (void *) fw->data);
	if (fw->size < (cfg_offset + WCD9XXX_MBHC_CAL_BTN_SZ(btn_cfg)))
		return false;

	/*
	 * Previous check guarantees that there is enough fw data up
	 * to start of impedance detection configuration
	 */
	imped_cfg = WCD9XXX_MBHC_CAL_IMPED_DET_PTR(fw->data);
	cfg_offset = (u32) ((void *) imped_cfg - (void *) fw->data);

	if (fw->size < (cfg_offset + WCD9XXX_MBHC_CAL_IMPED_MIN_SZ))
		return false;

	if (fw->size < (cfg_offset + WCD9XXX_MBHC_CAL_IMPED_SZ(imped_cfg)))
		return false;

	return true;
}

static u16 wcd9xxx_codec_v_sta_dce(struct wcd9xxx_mbhc *mbhc,
				   enum meas_type dce, s16 vin_mv,
				   bool cs_enable)
{
	s16 diff, zero;
	u32 mb_mv, in;
	u16 value;
	s16 dce_z;

	mb_mv = mbhc->mbhc_data.micb_mv;
	dce_z = mbhc->mbhc_data.dce_z;

	if (mb_mv == 0) {
		pr_err("%s: Mic Bias voltage is set to zero\n", __func__);
		return -EINVAL;
	}
	if (cs_enable) {
		mb_mv = VDDIO_MICBIAS_MV;
		dce_z = mbhc->mbhc_data.dce_nsc_cs_z;
	}

	if (dce) {
		diff = (mbhc->mbhc_data.dce_mb) - (dce_z);
		zero = (dce_z);
	} else {
		diff = (mbhc->mbhc_data.sta_mb) - (mbhc->mbhc_data.sta_z);
		zero = (mbhc->mbhc_data.sta_z);
	}
	in = (u32) diff * vin_mv;

	value = (u16) (in / mb_mv) + zero;
	return value;
}

static void wcd9xxx_mbhc_calc_thres(struct wcd9xxx_mbhc *mbhc)
{
	struct snd_soc_codec *codec;
	s16 adj_v_hs_max;
	s16 btn_mv = 0, btn_mv_sta[MBHC_V_IDX_NUM], btn_mv_dce[MBHC_V_IDX_NUM];
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_det;
	struct wcd9xxx_mbhc_plug_type_cfg *plug_type;
	u16 *btn_high;
	int i;

	pr_debug("%s: enter\n", __func__);
	codec = mbhc->codec;
	btn_det = WCD9XXX_MBHC_CAL_BTN_DET_PTR(mbhc->mbhc_cfg->calibration);
	plug_type = WCD9XXX_MBHC_CAL_PLUG_TYPE_PTR(mbhc->mbhc_cfg->calibration);

	mbhc->mbhc_data.v_ins_hu[MBHC_V_IDX_CFILT] =
	    wcd9xxx_codec_v_sta_dce(mbhc, STA, plug_type->v_hs_max, false);
	mbhc->mbhc_data.v_ins_h[MBHC_V_IDX_CFILT] =
	    wcd9xxx_codec_v_sta_dce(mbhc, DCE, plug_type->v_hs_max, false);

	mbhc->mbhc_data.v_inval_ins_low = FAKE_INS_LOW;
	mbhc->mbhc_data.v_inval_ins_high = FAKE_INS_HIGH;

	if (mbhc->mbhc_data.micb_mv != VDDIO_MICBIAS_MV) {
		adj_v_hs_max = scale_v_micb_vddio(mbhc, plug_type->v_hs_max,
						  true);
		mbhc->mbhc_data.v_ins_hu[MBHC_V_IDX_VDDIO] =
		    wcd9xxx_codec_v_sta_dce(mbhc, STA, adj_v_hs_max, false);
		mbhc->mbhc_data.v_ins_h[MBHC_V_IDX_VDDIO] =
		    wcd9xxx_codec_v_sta_dce(mbhc, DCE, adj_v_hs_max, false);
		mbhc->mbhc_data.v_inval_ins_low =
		    scale_v_micb_vddio(mbhc, mbhc->mbhc_data.v_inval_ins_low,
				       false);
		mbhc->mbhc_data.v_inval_ins_high =
		    scale_v_micb_vddio(mbhc, mbhc->mbhc_data.v_inval_ins_high,
				       false);
	}
	mbhc->mbhc_data.v_cs_ins_h = wcd9xxx_codec_v_sta_dce(mbhc, DCE,
							WCD9XXX_V_CS_HS_MAX,
							true);
	pr_debug("%s: v_ins_h for current source: 0x%x\n", __func__,
		  mbhc->mbhc_data.v_cs_ins_h);

	btn_high = wcd9xxx_mbhc_cal_btn_det_mp(btn_det,
					       MBHC_BTN_DET_V_BTN_HIGH);
	for (i = 0; i < btn_det->num_btn; i++)
		btn_mv = btn_high[i] > btn_mv ? btn_high[i] : btn_mv;

	btn_mv_sta[MBHC_V_IDX_CFILT] = btn_mv + btn_det->v_btn_press_delta_sta;
	btn_mv_dce[MBHC_V_IDX_CFILT] = btn_mv + btn_det->v_btn_press_delta_cic;
	btn_mv_sta[MBHC_V_IDX_VDDIO] =
	    scale_v_micb_vddio(mbhc, btn_mv_sta[MBHC_V_IDX_CFILT], true);
	btn_mv_dce[MBHC_V_IDX_VDDIO] =
	    scale_v_micb_vddio(mbhc, btn_mv_dce[MBHC_V_IDX_CFILT], true);

	mbhc->mbhc_data.v_b1_hu[MBHC_V_IDX_CFILT] =
	    wcd9xxx_codec_v_sta_dce(mbhc, STA, btn_mv_sta[MBHC_V_IDX_CFILT],
				    false);
	mbhc->mbhc_data.v_b1_h[MBHC_V_IDX_CFILT] =
	    wcd9xxx_codec_v_sta_dce(mbhc, DCE, btn_mv_dce[MBHC_V_IDX_CFILT],
				    false);
	mbhc->mbhc_data.v_b1_hu[MBHC_V_IDX_VDDIO] =
	    wcd9xxx_codec_v_sta_dce(mbhc, STA, btn_mv_sta[MBHC_V_IDX_VDDIO],
				    false);
	mbhc->mbhc_data.v_b1_h[MBHC_V_IDX_VDDIO] =
	    wcd9xxx_codec_v_sta_dce(mbhc, DCE, btn_mv_dce[MBHC_V_IDX_VDDIO],
				    false);

	mbhc->mbhc_data.v_brh[MBHC_V_IDX_CFILT] =
	    mbhc->mbhc_data.v_b1_h[MBHC_V_IDX_CFILT];
	mbhc->mbhc_data.v_brh[MBHC_V_IDX_VDDIO] =
	    mbhc->mbhc_data.v_b1_h[MBHC_V_IDX_VDDIO];

	mbhc->mbhc_data.v_brl = BUTTON_MIN;

	mbhc->mbhc_data.v_no_mic =
	    wcd9xxx_codec_v_sta_dce(mbhc, STA, plug_type->v_no_mic, false);
	pr_debug("%s: leave\n", __func__);
}

static void wcd9xxx_onoff_ext_mclk(struct wcd9xxx_mbhc *mbhc, bool on)
{
	/*
	 * XXX: {codec}_mclk_enable holds WCD9XXX_BCL_LOCK,
	 * therefore wcd9xxx_onoff_ext_mclk caller SHOULDN'T hold
	 * WCD9XXX_BCL_LOCK when it calls wcd9xxx_onoff_ext_mclk()
	 */
	 mbhc->mbhc_cfg->mclk_cb_fn(mbhc->codec, on, false);
}

/*
 * Mic Bias Enable Decision
 * Return true if high_hph_cnt is a power of 2 (!= 2)
 * otherwise return false
 */
static bool wcd9xxx_mbhc_enable_mb_decision(int high_hph_cnt)
{
	return (high_hph_cnt > 2) && !(high_hph_cnt & (high_hph_cnt - 1));
}

static void wcd9xxx_correct_swch_plug(struct work_struct *work)
{
	struct wcd9xxx_mbhc *mbhc;
	struct snd_soc_codec *codec;
	enum wcd9xxx_mbhc_plug_type plug_type = PLUG_TYPE_INVALID;
	unsigned long timeout;
	int retry = 0, pt_gnd_mic_swap_cnt = 0;
	int highhph_cnt = 0;
	bool correction = false;
	bool current_source_enable;
	bool wrk_complete = true, highhph = false;

	pr_debug("%s: enter\n", __func__);

	mbhc = container_of(work, struct wcd9xxx_mbhc, correct_plug_swch);
	codec = mbhc->codec;
	current_source_enable = ((mbhc->mbhc_cfg->cs_enable_flags &
				  (1 << MBHC_CS_ENABLE_POLLING)) != 0);

	wcd9xxx_onoff_ext_mclk(mbhc, true);

	/*
	 * Keep override on during entire plug type correction work.
	 *
	 * This is okay under the assumption that any switch irqs which use
	 * MBHC block cancel and sync this work so override is off again
	 * prior to switch interrupt handler's MBHC block usage.
	 * Also while this correction work is running, we can guarantee
	 * DAPM doesn't use any MBHC block as this work only runs with
	 * headphone detection.
	 */
	if (current_source_enable)
		wcd9xxx_turn_onoff_current_source(mbhc, true,
						  false);
	else
		wcd9xxx_turn_onoff_override(mbhc, true);

	timeout = jiffies + msecs_to_jiffies(HS_DETECT_PLUG_TIME_MS);
	while (!time_after(jiffies, timeout)) {
		++retry;
		rmb();
		if (mbhc->hs_detect_work_stop) {
			wrk_complete = false;
			pr_debug("%s: stop requested\n", __func__);
			break;
		}

		msleep(HS_DETECT_PLUG_INERVAL_MS);
		if (wcd9xxx_swch_level_remove(mbhc)) {
			wrk_complete = false;
			pr_debug("%s: Switch level is low\n", __func__);
			break;
		}

		/* can race with removal interrupt */
		WCD9XXX_BCL_LOCK(mbhc->resmgr);
		if (current_source_enable)
			plug_type = wcd9xxx_codec_cs_get_plug_type(mbhc,
								   highhph);
		else
			plug_type = wcd9xxx_codec_get_plug_type(mbhc, true);
		WCD9XXX_BCL_UNLOCK(mbhc->resmgr);

		pr_debug("%s: attempt(%d) current_plug(%d) new_plug(%d)\n",
			 __func__, retry, mbhc->current_plug, plug_type);

		highhph_cnt = (plug_type == PLUG_TYPE_HIGH_HPH) ?
					(highhph_cnt + 1) :
					0;
		highhph = wcd9xxx_mbhc_enable_mb_decision(highhph_cnt);
		if (plug_type == PLUG_TYPE_INVALID) {
			pr_debug("Invalid plug in attempt # %d\n", retry);
			if (!mbhc->mbhc_cfg->detect_extn_cable &&
			    retry == NUM_ATTEMPTS_TO_REPORT &&
			    mbhc->current_plug == PLUG_TYPE_NONE) {
				wcd9xxx_report_plug(mbhc, 1,
						    SND_JACK_HEADPHONE);
			}
		} else if (plug_type == PLUG_TYPE_HEADPHONE) {
			pr_debug("Good headphone detected, continue polling\n");
			if (mbhc->mbhc_cfg->detect_extn_cable) {
				if (mbhc->current_plug != plug_type)
					wcd9xxx_report_plug(mbhc, 1,
							    SND_JACK_HEADPHONE);
			} else if (mbhc->current_plug == PLUG_TYPE_NONE) {
				wcd9xxx_report_plug(mbhc, 1,
						    SND_JACK_HEADPHONE);
			}
		} else if (plug_type == PLUG_TYPE_HIGH_HPH) {
			pr_debug("%s: High HPH detected, continue polling\n",
				  __func__);
		} else {
			if (plug_type == PLUG_TYPE_GND_MIC_SWAP) {
				pt_gnd_mic_swap_cnt++;
				if (pt_gnd_mic_swap_cnt <
				    GND_MIC_SWAP_THRESHOLD)
					continue;
				else if (pt_gnd_mic_swap_cnt >
					 GND_MIC_SWAP_THRESHOLD) {
					/*
					 * This is due to GND/MIC switch didn't
					 * work,  Report unsupported plug
					 */
				} else if (mbhc->mbhc_cfg->swap_gnd_mic) {
					/*
					 * if switch is toggled, check again,
					 * otherwise report unsupported plug
					 */
					if (mbhc->mbhc_cfg->swap_gnd_mic(codec))
						continue;
				}
			} else
				pt_gnd_mic_swap_cnt = 0;

			WCD9XXX_BCL_LOCK(mbhc->resmgr);
			/* Turn off override/current source */
			if (current_source_enable)
				wcd9xxx_turn_onoff_current_source(mbhc, false,
								  false);
			else
				wcd9xxx_turn_onoff_override(mbhc, false);
			/*
			 * The valid plug also includes PLUG_TYPE_GND_MIC_SWAP
			 */
			wcd9xxx_find_plug_and_report(mbhc, plug_type);
			WCD9XXX_BCL_UNLOCK(mbhc->resmgr);
			pr_debug("Attempt %d found correct plug %d\n", retry,
				 plug_type);
			correction = true;
			break;
		}
	}

	highhph = false;
	if (wrk_complete && plug_type == PLUG_TYPE_HIGH_HPH) {
		pr_debug("%s: polling is done, still HPH, so enabling MIC trigger\n",
			 __func__);
		WCD9XXX_BCL_LOCK(mbhc->resmgr);
		wcd9xxx_find_plug_and_report(mbhc, plug_type);
		highhph = true;
		WCD9XXX_BCL_UNLOCK(mbhc->resmgr);
	}

	if (!correction && current_source_enable)
		wcd9xxx_turn_onoff_current_source(mbhc, false, highhph);
	else if (!correction)
		wcd9xxx_turn_onoff_override(mbhc, false);

	wcd9xxx_onoff_ext_mclk(mbhc, false);

	if (mbhc->mbhc_cfg->detect_extn_cable) {
		WCD9XXX_BCL_LOCK(mbhc->resmgr);
		if ((mbhc->current_plug == PLUG_TYPE_HEADPHONE &&
		    wrk_complete) ||
		    mbhc->current_plug == PLUG_TYPE_GND_MIC_SWAP ||
		    mbhc->current_plug == PLUG_TYPE_INVALID ||
		    (plug_type == PLUG_TYPE_INVALID && wrk_complete)) {
			/* Enable removal detection */
			wcd9xxx_cleanup_hs_polling(mbhc);
			wcd9xxx_enable_hs_detect(mbhc, 0, 0, false);
		}
		WCD9XXX_BCL_UNLOCK(mbhc->resmgr);
	}
	pr_debug("%s: leave current_plug(%d)\n", __func__, mbhc->current_plug);
	/* unlock sleep */
	wcd9xxx_unlock_sleep(mbhc->resmgr->core_res);
}

static void wcd9xxx_swch_irq_handler(struct wcd9xxx_mbhc *mbhc)
{
	bool insert;
	bool is_removed = false;
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);

	mbhc->in_swch_irq_handler = true;
	/* Wait here for debounce time */
	usleep_range(SWCH_IRQ_DEBOUNCE_TIME_US, SWCH_IRQ_DEBOUNCE_TIME_US);

	WCD9XXX_BCL_LOCK(mbhc->resmgr);

	/* cancel pending button press */
	if (wcd9xxx_cancel_btn_work(mbhc))
		pr_debug("%s: button press is canceled\n", __func__);

	insert = !wcd9xxx_swch_level_remove(mbhc);
	pr_debug("%s: Current plug type %d, insert %d\n", __func__,
		 mbhc->current_plug, insert);
	if ((mbhc->current_plug == PLUG_TYPE_NONE) && insert) {
		mbhc->lpi_enabled = false;
		wmb();

		/* cancel detect plug */
		wcd9xxx_cancel_hs_detect_plug(mbhc,
					      &mbhc->correct_plug_swch);
		if ((mbhc->current_plug != PLUG_TYPE_NONE) &&
		    !(snd_soc_read(codec, WCD9XXX_A_MBHC_INSERT_DETECT) &
				   (1 << 1)))
			goto exit;

		/* Disable Mic Bias pull down and HPH Switch to GND */
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x01,
				    0x00);
		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x01, 0x00);
		wcd9xxx_mbhc_detect_plug_type(mbhc);
	} else if ((mbhc->current_plug != PLUG_TYPE_NONE) && !insert) {
		mbhc->lpi_enabled = false;
		wmb();

		/* cancel detect plug */
		wcd9xxx_cancel_hs_detect_plug(mbhc,
					      &mbhc->correct_plug_swch);

		if (mbhc->current_plug == PLUG_TYPE_HEADPHONE) {
			wcd9xxx_report_plug(mbhc, 0, SND_JACK_HEADPHONE);
			is_removed = true;
		} else if (mbhc->current_plug == PLUG_TYPE_GND_MIC_SWAP) {
			wcd9xxx_report_plug(mbhc, 0, SND_JACK_UNSUPPORTED);
			is_removed = true;
		} else if (mbhc->current_plug == PLUG_TYPE_HEADSET) {
			wcd9xxx_pause_hs_polling(mbhc);
			wcd9xxx_cleanup_hs_polling(mbhc);
			wcd9xxx_report_plug(mbhc, 0, SND_JACK_HEADSET);
			is_removed = true;
		} else if (mbhc->current_plug == PLUG_TYPE_HIGH_HPH) {
			wcd9xxx_report_plug(mbhc, 0, SND_JACK_LINEOUT);
			is_removed = true;
		}

		if (is_removed) {
			/* Enable Mic Bias pull down and HPH Switch to GND */
			snd_soc_update_bits(codec,
					mbhc->mbhc_bias_regs.ctl_reg, 0x01,
					0x01);
			snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x01,
					0x01);
			/* Make sure mic trigger is turned off */
			snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg,
					    0x01, 0x01);
			snd_soc_update_bits(codec,
					    mbhc->mbhc_bias_regs.mbhc_reg,
					    0x90, 0x00);
			/* Reset MBHC State Machine */
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL,
					    0x08, 0x08);
			snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL,
					    0x08, 0x00);
			/* Turn off override */
			wcd9xxx_turn_onoff_override(mbhc, false);
		}
	}
exit:
	mbhc->in_swch_irq_handler = false;
	WCD9XXX_BCL_UNLOCK(mbhc->resmgr);
	pr_debug("%s: leave\n", __func__);
}

static irqreturn_t wcd9xxx_mech_plug_detect_irq(int irq, void *data)
{
	int r = IRQ_HANDLED;
	struct wcd9xxx_mbhc *mbhc = data;

	pr_debug("%s: enter\n", __func__);
	if (unlikely(wcd9xxx_lock_sleep(mbhc->resmgr->core_res) == false)) {
		pr_warn("%s: failed to hold suspend\n", __func__);
		r = IRQ_NONE;
	} else {
		/* Call handler */
		wcd9xxx_swch_irq_handler(mbhc);
		wcd9xxx_unlock_sleep(mbhc->resmgr->core_res);
	}

	pr_debug("%s: leave %d\n", __func__, r);
	return r;
}

static int wcd9xxx_is_false_press(struct wcd9xxx_mbhc *mbhc)
{
	s16 mb_v;
	int i = 0;
	int r = 0;
	const s16 v_ins_hu =
	    wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_INS_HU);
	const s16 v_ins_h =
	    wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_INS_H);
	const s16 v_b1_hu =
	    wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_B1_HU);
	const s16 v_b1_h =
	    wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_B1_H);
	const unsigned long timeout =
	    jiffies + msecs_to_jiffies(BTN_RELEASE_DEBOUNCE_TIME_MS);

	while (time_before(jiffies, timeout)) {
		/*
		 * This function needs to run measurements just few times during
		 * release debounce time.  Make 1ms interval to avoid
		 * unnecessary excessive measurements.
		 */
		usleep_range(1000, 1000 + WCD9XXX_USLEEP_RANGE_MARGIN_US);
		if (i == 0) {
			mb_v = wcd9xxx_codec_sta_dce(mbhc, 0, true);
			pr_debug("%s: STA[0]: %d,%d\n", __func__, mb_v,
				 wcd9xxx_codec_sta_dce_v(mbhc, 0, mb_v));
			if (mb_v < v_b1_hu || mb_v > v_ins_hu) {
				r = 1;
				break;
			}
		} else {
			mb_v = wcd9xxx_codec_sta_dce(mbhc, 1, true);
			pr_debug("%s: DCE[%d]: %d,%d\n", __func__, i, mb_v,
				 wcd9xxx_codec_sta_dce_v(mbhc, 1, mb_v));
			if (mb_v < v_b1_h || mb_v > v_ins_h) {
				r = 1;
				break;
			}
		}
		i++;
	}

	return r;
}

/* called under codec_resource_lock acquisition */
static int wcd9xxx_determine_button(const struct wcd9xxx_mbhc *mbhc,
				  const s32 micmv)
{
	s16 *v_btn_low, *v_btn_high;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_det;
	int i, btn = -1;

	btn_det = WCD9XXX_MBHC_CAL_BTN_DET_PTR(mbhc->mbhc_cfg->calibration);
	v_btn_low = wcd9xxx_mbhc_cal_btn_det_mp(btn_det,
						MBHC_BTN_DET_V_BTN_LOW);
	v_btn_high = wcd9xxx_mbhc_cal_btn_det_mp(btn_det,
						 MBHC_BTN_DET_V_BTN_HIGH);

	for (i = 0; i < btn_det->num_btn; i++) {
		if ((v_btn_low[i] <= micmv) && (v_btn_high[i] >= micmv)) {
			btn = i;
			break;
		}
	}

	if (btn == -1)
		pr_debug("%s: couldn't find button number for mic mv %d\n",
			 __func__, micmv);

	return btn;
}

static int wcd9xxx_get_button_mask(const int btn)
{
	int mask = 0;
	switch (btn) {
	case 0:
		mask = SND_JACK_BTN_0;
		break;
	case 1:
		mask = SND_JACK_BTN_1;
		break;
	case 2:
		mask = SND_JACK_BTN_2;
		break;
	case 3:
		mask = SND_JACK_BTN_3;
		break;
	case 4:
		mask = SND_JACK_BTN_4;
		break;
	case 5:
		mask = SND_JACK_BTN_5;
		break;
	case 6:
		mask = SND_JACK_BTN_6;
		break;
	case 7:
		mask = SND_JACK_BTN_7;
		break;
	}
	return mask;
}

static void wcd9xxx_get_z(struct wcd9xxx_mbhc *mbhc, s16 *dce_z, s16 *sta_z)
{
	s16 reg0, reg1;
	int change;
	struct snd_soc_codec *codec = mbhc->codec;

	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);
	/* Pull down micbias to ground and disconnect vddio switch */
	reg0 = snd_soc_read(codec, mbhc->mbhc_bias_regs.ctl_reg);
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x81, 0x1);
	reg1 = snd_soc_read(codec, mbhc->mbhc_bias_regs.mbhc_reg);
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.mbhc_reg, 1 << 7, 0);

	/* Disconnect override from micbias */
	change = snd_soc_update_bits(codec, WCD9XXX_A_MAD_ANA_CTRL, 1 << 4,
				     1 << 0);
	usleep_range(1000, 1000 + 1000);
	if (sta_z) {
		*sta_z = wcd9xxx_codec_sta_dce(mbhc, 0, false);
		pr_debug("%s: sta_z 0x%x\n", __func__, *sta_z & 0xFFFF);
	}
	if (dce_z) {
		*dce_z = wcd9xxx_codec_sta_dce(mbhc, 1, false);
		pr_debug("%s: dce_z 0x%x\n", __func__, *dce_z & 0xFFFF);
	}

	/* Connect override from micbias */
	if (change)
		snd_soc_update_bits(codec, WCD9XXX_A_MAD_ANA_CTRL, 1 << 4,
				    1 << 4);
	/* Disable pull down micbias to ground */
	snd_soc_write(codec, mbhc->mbhc_bias_regs.mbhc_reg, reg1);
	snd_soc_write(codec, mbhc->mbhc_bias_regs.ctl_reg, reg0);
}

void wcd9xxx_update_z(struct wcd9xxx_mbhc *mbhc)
{
	const u16 sta_z = mbhc->mbhc_data.sta_z;
	const u16 dce_z = mbhc->mbhc_data.dce_z;

	wcd9xxx_get_z(mbhc, &mbhc->mbhc_data.dce_z, &mbhc->mbhc_data.sta_z);
	pr_debug("%s: sta_z 0x%x,dce_z 0x%x -> sta_z 0x%x,dce_z 0x%x\n",
		 __func__, sta_z & 0xFFFF, dce_z & 0xFFFF,
		 mbhc->mbhc_data.sta_z & 0xFFFF,
		 mbhc->mbhc_data.dce_z & 0xFFFF);

	wcd9xxx_mbhc_calc_thres(mbhc);
	wcd9xxx_calibrate_hs_polling(mbhc);
}

/*
 * wcd9xxx_update_rel_threshold : update mbhc release upper bound threshold
 *				  to ceilmv + buffer
 */
static int wcd9xxx_update_rel_threshold(struct wcd9xxx_mbhc *mbhc, int ceilmv)
{
	u16 v_brh, v_b1_hu;
	int mv;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_det;
	void *calibration = mbhc->mbhc_cfg->calibration;
	struct snd_soc_codec *codec = mbhc->codec;

	btn_det = WCD9XXX_MBHC_CAL_BTN_DET_PTR(calibration);
	mv = ceilmv + btn_det->v_btn_press_delta_cic;
	pr_debug("%s: reprogram vb1hu/vbrh to %dmv\n", __func__, mv);

	/* update LSB first so mbhc hardware block doesn't see too low value */
	v_b1_hu = wcd9xxx_codec_v_sta_dce(mbhc, STA, mv, false);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B3_CTL, v_b1_hu & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B4_CTL,
		      (v_b1_hu >> 8) & 0xFF);
	v_brh = wcd9xxx_codec_v_sta_dce(mbhc, DCE, mv, false);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B9_CTL, v_brh & 0xFF);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_VOLT_B10_CTL,
		      (v_brh >> 8) & 0xFF);
	return 0;
}

irqreturn_t wcd9xxx_dce_handler(int irq, void *data)
{
	int i, mask;
	bool vddio;
	u8 mbhc_status;
	s16 dce_z, sta_z;
	s32 stamv, stamv_s;
	s16 *v_btn_high;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_det;
	int btn = -1, meas = 0;
	struct wcd9xxx_mbhc *mbhc = data;
	const struct wcd9xxx_mbhc_btn_detect_cfg *d =
	    WCD9XXX_MBHC_CAL_BTN_DET_PTR(mbhc->mbhc_cfg->calibration);
	short btnmeas[d->n_btn_meas + 1];
	short dce[d->n_btn_meas + 1], sta;
	s32 mv[d->n_btn_meas + 1], mv_s[d->n_btn_meas + 1];
	struct snd_soc_codec *codec = mbhc->codec;
	struct wcd9xxx_core_resource *core_res = mbhc->resmgr->core_res;
	int n_btn_meas = d->n_btn_meas;
	void *calibration = mbhc->mbhc_cfg->calibration;

	pr_debug("%s: enter\n", __func__);

	WCD9XXX_BCL_LOCK(mbhc->resmgr);
	mbhc_status = snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B1_STATUS) & 0x3E;

	if (mbhc->mbhc_state == MBHC_STATE_POTENTIAL_RECOVERY) {
		pr_debug("%s: mbhc is being recovered, skip button press\n",
			 __func__);
		goto done;
	}

	mbhc->mbhc_state = MBHC_STATE_POTENTIAL;

	if (!mbhc->polling_active) {
		pr_warn("%s: mbhc polling is not active, skip button press\n",
			__func__);
		goto done;
	}

	/* If switch nterrupt already kicked in, ignore button press */
	if (mbhc->in_swch_irq_handler) {
		pr_debug("%s: Swtich level changed, ignore button press\n",
			 __func__);
		btn = -1;
		goto done;
	}

	/* Measure scaled HW DCE */
	vddio = (mbhc->mbhc_data.micb_mv != VDDIO_MICBIAS_MV &&
		 mbhc->mbhc_micbias_switched);

	dce_z = mbhc->mbhc_data.dce_z;
	sta_z = mbhc->mbhc_data.sta_z;

	/* Measure scaled HW STA */
	dce[0] = wcd9xxx_read_dce_result(codec);
	sta = wcd9xxx_read_sta_result(codec);
	if (mbhc_status != STATUS_REL_DETECTION) {
		if (mbhc->mbhc_last_resume &&
		    !time_after(jiffies, mbhc->mbhc_last_resume + HZ)) {
			pr_debug("%s: Button is released after resume\n",
				__func__);
			n_btn_meas = 0;
		} else {
			pr_debug("%s: Button is released without resume",
				 __func__);
			if (mbhc->update_z) {
				wcd9xxx_update_z(mbhc);
				mbhc->update_z = false;
			}
			stamv = __wcd9xxx_codec_sta_dce_v(mbhc, 0, sta, sta_z,
						mbhc->mbhc_data.micb_mv);
			if (vddio)
				stamv_s = scale_v_micb_vddio(mbhc, stamv,
							     false);
			else
				stamv_s = stamv;
			mv[0] = __wcd9xxx_codec_sta_dce_v(mbhc, 1, dce[0],
					  dce_z, mbhc->mbhc_data.micb_mv);
			mv_s[0] = vddio ? scale_v_micb_vddio(mbhc, mv[0],
							     false) : mv[0];
			btn = wcd9xxx_determine_button(mbhc, mv_s[0]);
			if (btn != wcd9xxx_determine_button(mbhc, stamv_s))
				btn = -1;
			goto done;
		}
	}

	for (meas = 1; ((d->n_btn_meas) && (meas < (d->n_btn_meas + 1)));
	     meas++)
		dce[meas] = wcd9xxx_codec_sta_dce(mbhc, 1, false);

	if (mbhc->update_z) {
		wcd9xxx_update_z(mbhc);
		mbhc->update_z = false;
	}

	stamv = __wcd9xxx_codec_sta_dce_v(mbhc, 0, sta, sta_z,
					  mbhc->mbhc_data.micb_mv);
	if (vddio)
		stamv_s = scale_v_micb_vddio(mbhc, stamv, false);
	else
		stamv_s = stamv;
	pr_debug("%s: Meas HW - STA 0x%x,%d,%d\n", __func__,
		 sta & 0xFFFF, stamv, stamv_s);

	/* determine pressed button */
	mv[0] = __wcd9xxx_codec_sta_dce_v(mbhc, 1, dce[0], dce_z,
					  mbhc->mbhc_data.micb_mv);
	mv_s[0] = vddio ? scale_v_micb_vddio(mbhc, mv[0], false) : mv[0];
	btnmeas[0] = wcd9xxx_determine_button(mbhc, mv_s[0]);
	pr_debug("%s: Meas HW - DCE 0x%x,%d,%d button %d\n", __func__,
		 dce[0] & 0xFFFF, mv[0], mv_s[0], btnmeas[0]);
	if (n_btn_meas == 0)
		btn = btnmeas[0];
	for (meas = 1; (n_btn_meas && d->n_btn_meas &&
			(meas < (d->n_btn_meas + 1))); meas++) {
		mv[meas] = __wcd9xxx_codec_sta_dce_v(mbhc, 1, dce[meas], dce_z,
						     mbhc->mbhc_data.micb_mv);
		mv_s[meas] = vddio ? scale_v_micb_vddio(mbhc, mv[meas], false) :
				     mv[meas];
		btnmeas[meas] = wcd9xxx_determine_button(mbhc, mv_s[meas]);
		pr_debug("%s: Meas %d - DCE 0x%x,%d,%d button %d\n",
			 __func__, meas, dce[meas] & 0xFFFF, mv[meas],
			 mv_s[meas], btnmeas[meas]);
		/*
		 * if large enough measurements are collected,
		 * start to check if last all n_btn_con measurements were
		 * in same button low/high range
		 */
		if (meas + 1 >= d->n_btn_con) {
			for (i = 0; i < d->n_btn_con; i++)
				if ((btnmeas[meas] < 0) ||
				    (btnmeas[meas] != btnmeas[meas - i]))
					break;
			if (i == d->n_btn_con) {
				/* button pressed */
				btn = btnmeas[meas];
				break;
			} else if ((n_btn_meas - meas) < (d->n_btn_con - 1)) {
				/*
				 * if left measurements are less than n_btn_con,
				 * it's impossible to find button number
				 */
				break;
			}
		}
	}

	if (btn >= 0) {
		if (mbhc->in_swch_irq_handler) {
			pr_debug(
			"%s: Switch irq triggered, ignore button press\n",
			__func__);
			goto done;
		}
		btn_det = WCD9XXX_MBHC_CAL_BTN_DET_PTR(calibration);
		v_btn_high = wcd9xxx_mbhc_cal_btn_det_mp(btn_det,
						       MBHC_BTN_DET_V_BTN_HIGH);
		WARN_ON(btn >= btn_det->num_btn);
		/* reprogram release threshold to catch voltage ramp up early */
		wcd9xxx_update_rel_threshold(mbhc, v_btn_high[btn]);

		mask = wcd9xxx_get_button_mask(btn);
		mbhc->buttons_pressed |= mask;
		wcd9xxx_lock_sleep(core_res);
		if (schedule_delayed_work(&mbhc->mbhc_btn_dwork,
					  msecs_to_jiffies(400)) == 0) {
			WARN(1, "Button pressed twice without release event\n");
			wcd9xxx_unlock_sleep(core_res);
		}
	} else {
		pr_debug("%s: bogus button press, too short press?\n",
			 __func__);
	}

 done:
	pr_debug("%s: leave\n", __func__);
	WCD9XXX_BCL_UNLOCK(mbhc->resmgr);
	return IRQ_HANDLED;
}

static irqreturn_t wcd9xxx_release_handler(int irq, void *data)
{
	int ret;
	bool waitdebounce = true;
	struct wcd9xxx_mbhc *mbhc = data;

	pr_debug("%s: enter\n", __func__);
	WCD9XXX_BCL_LOCK(mbhc->resmgr);
	mbhc->mbhc_state = MBHC_STATE_RELEASE;

	if (mbhc->buttons_pressed & WCD9XXX_JACK_BUTTON_MASK) {
		ret = wcd9xxx_cancel_btn_work(mbhc);
		if (ret == 0) {
			pr_debug("%s: Reporting long button release event\n",
				 __func__);
			wcd9xxx_jack_report(mbhc, &mbhc->button_jack, 0,
					    mbhc->buttons_pressed);
		} else {
			if (wcd9xxx_is_false_press(mbhc)) {
				pr_debug("%s: Fake button press interrupt\n",
					 __func__);
			} else {
				if (mbhc->in_swch_irq_handler) {
					pr_debug("%s: Switch irq kicked in, ignore\n",
						 __func__);
				} else {
					pr_debug("%s: Reporting btn press\n",
						 __func__);
					wcd9xxx_jack_report(mbhc,
							 &mbhc->button_jack,
							 mbhc->buttons_pressed,
							 mbhc->buttons_pressed);
					pr_debug("%s: Reporting btn release\n",
						 __func__);
					wcd9xxx_jack_report(mbhc,
						      &mbhc->button_jack,
						      0, mbhc->buttons_pressed);
					waitdebounce = false;
				}
			}
		}

		mbhc->buttons_pressed &= ~WCD9XXX_JACK_BUTTON_MASK;
	}

	wcd9xxx_calibrate_hs_polling(mbhc);

	if (waitdebounce)
		msleep(SWCH_REL_DEBOUNCE_TIME_MS);
	wcd9xxx_start_hs_polling(mbhc);

	pr_debug("%s: leave\n", __func__);
	WCD9XXX_BCL_UNLOCK(mbhc->resmgr);
	return IRQ_HANDLED;
}

static irqreturn_t wcd9xxx_hphl_ocp_irq(int irq, void *data)
{
	struct wcd9xxx_mbhc *mbhc = data;
	struct snd_soc_codec *codec;

	pr_info("%s: received HPHL OCP irq\n", __func__);

	if (mbhc) {
		codec = mbhc->codec;
		if ((mbhc->hphlocp_cnt < OCP_ATTEMPT) &&
		    (!mbhc->hphrocp_cnt)) {
			pr_info("%s: retry\n", __func__);
			mbhc->hphlocp_cnt++;
			snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_OCP_CTL,
					    0x10, 0x00);
			snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_OCP_CTL,
					    0x10, 0x10);
		} else {
			wcd9xxx_disable_irq(mbhc->resmgr->core_res,
					  WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
			mbhc->hph_status |= SND_JACK_OC_HPHL;
			wcd9xxx_jack_report(mbhc, &mbhc->headset_jack,
					    mbhc->hph_status,
					    WCD9XXX_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad wcd9xxx private data\n", __func__);
	}

	return IRQ_HANDLED;
}

static irqreturn_t wcd9xxx_hphr_ocp_irq(int irq, void *data)
{
	struct wcd9xxx_mbhc *mbhc = data;
	struct snd_soc_codec *codec;

	pr_info("%s: received HPHR OCP irq\n", __func__);
	codec = mbhc->codec;
	if ((mbhc->hphrocp_cnt < OCP_ATTEMPT) &&
	    (!mbhc->hphlocp_cnt)) {
		pr_info("%s: retry\n", __func__);
		mbhc->hphrocp_cnt++;
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_OCP_CTL, 0x10,
				    0x00);
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_OCP_CTL, 0x10,
				    0x10);
	} else {
		wcd9xxx_disable_irq(mbhc->resmgr->core_res,
				    WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
		mbhc->hph_status |= SND_JACK_OC_HPHR;
		wcd9xxx_jack_report(mbhc, &mbhc->headset_jack,
				    mbhc->hph_status, WCD9XXX_JACK_MASK);
	}

	return IRQ_HANDLED;
}

static int wcd9xxx_acdb_mclk_index(const int rate)
{
	if (rate == MCLK_RATE_12288KHZ)
		return 0;
	else if (rate == MCLK_RATE_9600KHZ)
		return 1;
	else {
		BUG_ON(1);
		return -EINVAL;
	}
}

static void wcd9xxx_update_mbhc_clk_rate(struct wcd9xxx_mbhc *mbhc, u32 rate)
{
	u32 dce_wait, sta_wait;
	u8 ncic, nmeas, navg;
	void *calibration;
	u8 *n_cic, *n_ready;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_det;
	u8 npoll = 4, nbounce_wait = 30;
	struct snd_soc_codec *codec = mbhc->codec;
	int idx = wcd9xxx_acdb_mclk_index(rate);
	int idxmclk = wcd9xxx_acdb_mclk_index(mbhc->mbhc_cfg->mclk_rate);

	pr_debug("%s: Updating clock rate dependents, rate = %u\n", __func__,
		 rate);
	calibration = mbhc->mbhc_cfg->calibration;

	/*
	 * First compute the DCE / STA wait times depending on tunable
	 * parameters. The value is computed in microseconds
	 */
	btn_det = WCD9XXX_MBHC_CAL_BTN_DET_PTR(calibration);
	n_ready = wcd9xxx_mbhc_cal_btn_det_mp(btn_det, MBHC_BTN_DET_N_READY);
	n_cic = wcd9xxx_mbhc_cal_btn_det_mp(btn_det, MBHC_BTN_DET_N_CIC);
	nmeas = WCD9XXX_MBHC_CAL_BTN_DET_PTR(calibration)->n_meas;
	navg = WCD9XXX_MBHC_CAL_GENERAL_PTR(calibration)->mbhc_navg;

	/* ncic stays with the same what we had during calibration */
	ncic = n_cic[idxmclk];
	dce_wait = (1000 * 512 * ncic * (nmeas + 1)) / (rate / 1000);
	sta_wait = (1000 * 128 * (navg + 1)) / (rate / 1000);
	mbhc->mbhc_data.t_dce = dce_wait;
	/* give extra margin to sta for safety */
	mbhc->mbhc_data.t_sta = sta_wait + 250;
	mbhc->mbhc_data.t_sta_dce = ((1000 * 256) / (rate / 1000) *
				     n_ready[idx]) + 10;

	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_TIMER_B1_CTL, n_ready[idx]);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_TIMER_B6_CTL, ncic);

	if (rate == MCLK_RATE_12288KHZ) {
		npoll = 4;
		nbounce_wait = 30;
	} else if (rate == MCLK_RATE_9600KHZ) {
		npoll = 3;
		nbounce_wait = 23;
	}

	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_TIMER_B2_CTL, npoll);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_TIMER_B3_CTL, nbounce_wait);
	pr_debug("%s: leave\n", __func__);
}

static void wcd9xxx_mbhc_cal(struct wcd9xxx_mbhc *mbhc)
{
	u8 cfilt_mode;
	u16 reg0, reg1, reg2;
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);
	wcd9xxx_disable_irq(mbhc->resmgr->core_res, WCD9XXX_IRQ_MBHC_POTENTIAL);
	wcd9xxx_turn_onoff_rel_detection(codec, false);

	/* t_dce and t_sta are updated by wcd9xxx_update_mbhc_clk_rate() */
	WARN_ON(!mbhc->mbhc_data.t_dce);
	WARN_ON(!mbhc->mbhc_data.t_sta);

	/*
	 * LDOH and CFILT are already configured during pdata handling.
	 * Only need to make sure CFILT and bandgap are in Fast mode.
	 * Need to restore defaults once calculation is done.
	 *
	 * In case when Micbias is powered by external source, request
	 * turn on the external voltage source for Calibration.
	 */
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mb_source)
		mbhc->mbhc_cb->enable_mb_source(codec, true);

	cfilt_mode = snd_soc_read(codec, mbhc->mbhc_bias_regs.cfilt_ctl);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->cfilt_fast_mode)
		mbhc->mbhc_cb->cfilt_fast_mode(codec, mbhc);
	else
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.cfilt_ctl,
				    0x40, 0x00);

	/*
	 * Micbias, CFILT, LDOH, MBHC MUX mode settings
	 * to perform ADC calibration
	 */
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->select_cfilt)
		mbhc->mbhc_cb->select_cfilt(codec, mbhc);
	else
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x60,
			    mbhc->mbhc_cfg->micbias << 5);
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x01, 0x00);
	snd_soc_update_bits(codec, WCD9XXX_A_LDO_H_MODE_1, 0x60, 0x60);
	snd_soc_write(codec, WCD9XXX_A_TX_7_MBHC_TEST_CTL, 0x78);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->codec_specific_cal)
		mbhc->mbhc_cb->codec_specific_cal(codec, mbhc);
	else
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL,
				    0x04, 0x04);

	/* Pull down micbias to ground */
	reg0 = snd_soc_read(codec, mbhc->mbhc_bias_regs.ctl_reg);
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 1, 1);
	/* Disconnect override from micbias */
	reg1 = snd_soc_read(codec, WCD9XXX_A_MAD_ANA_CTRL);
	snd_soc_update_bits(codec, WCD9XXX_A_MAD_ANA_CTRL, 1 << 4, 1 << 0);
	/* Connect the MUX to micbias */
	snd_soc_write(codec, WCD9XXX_A_MBHC_SCALING_MUX_1, 0x02);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mux_bias_block)
		mbhc->mbhc_cb->enable_mux_bias_block(codec);
	else
		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
				    0x80, 0x80);
	/*
	 * Hardware that has external cap can delay mic bias ramping down up
	 * to 50ms.
	 */
	msleep(WCD9XXX_MUX_SWITCH_READY_WAIT_MS);
	/* DCE measurement for 0 voltage */
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x02);
	mbhc->mbhc_data.dce_z = __wcd9xxx_codec_sta_dce(mbhc, 1, true, false);

	/* compute dce_z for current source */
	reg2 = snd_soc_read(codec, WCD9XXX_A_CDC_MBHC_B1_CTL);
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x78,
			    WCD9XXX_MBHC_NSC_CS << 3);

	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x02);
	mbhc->mbhc_data.dce_nsc_cs_z = __wcd9xxx_codec_sta_dce(mbhc, 1, true,
							       false);
	pr_debug("%s: dce_z with nsc cs: 0x%x\n", __func__,
						 mbhc->mbhc_data.dce_nsc_cs_z);

	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, reg2);

	/* STA measurement for 0 voltage */
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x02);
	mbhc->mbhc_data.sta_z = __wcd9xxx_codec_sta_dce(mbhc, 0, true, false);

	/* Restore registers */
	snd_soc_write(codec, mbhc->mbhc_bias_regs.ctl_reg, reg0);
	snd_soc_write(codec, WCD9XXX_A_MAD_ANA_CTRL, reg1);

	/* DCE measurment for MB voltage */
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, WCD9XXX_A_MBHC_SCALING_MUX_1, 0x02);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mux_bias_block)
		mbhc->mbhc_cb->enable_mux_bias_block(codec);
	else
		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
				    0x80, 0x80);
	/*
	 * Hardware that has external cap can delay mic bias ramping down up
	 * to 50ms.
	 */
	msleep(WCD9XXX_MUX_SWITCH_READY_WAIT_MS);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x04);
	usleep_range(mbhc->mbhc_data.t_dce, mbhc->mbhc_data.t_dce);
	mbhc->mbhc_data.dce_mb = wcd9xxx_read_dce_result(codec);

	/* STA Measurement for MB Voltage */
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x0A);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x02);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_CLK_CTL, 0x02);
	snd_soc_write(codec, WCD9XXX_A_MBHC_SCALING_MUX_1, 0x02);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mux_bias_block)
		mbhc->mbhc_cb->enable_mux_bias_block(codec);
	else
		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
				    0x80, 0x80);
	/*
	 * Hardware that has external cap can delay mic bias ramping down up
	 * to 50ms.
	 */
	msleep(WCD9XXX_MUX_SWITCH_READY_WAIT_MS);
	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_EN_CTL, 0x02);
	usleep_range(mbhc->mbhc_data.t_sta, mbhc->mbhc_data.t_sta);
	mbhc->mbhc_data.sta_mb = wcd9xxx_read_sta_result(codec);

	/* Restore default settings. */
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x04, 0x00);
	snd_soc_write(codec, mbhc->mbhc_bias_regs.cfilt_ctl, cfilt_mode);
	snd_soc_write(codec, WCD9XXX_A_MBHC_SCALING_MUX_1, 0x04);
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mux_bias_block)
		mbhc->mbhc_cb->enable_mux_bias_block(codec);
	else
		snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_1,
				    0x80, 0x80);
	usleep_range(100, 100);

	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mb_source)
		mbhc->mbhc_cb->enable_mb_source(codec, false);

	wcd9xxx_enable_irq(mbhc->resmgr->core_res, WCD9XXX_IRQ_MBHC_POTENTIAL);
	wcd9xxx_turn_onoff_rel_detection(codec, true);

	pr_debug("%s: leave\n", __func__);
}

static void wcd9xxx_mbhc_setup(struct wcd9xxx_mbhc *mbhc)
{
	int n;
	u8 *gain;
	struct wcd9xxx_mbhc_general_cfg *generic;
	struct wcd9xxx_mbhc_btn_detect_cfg *btn_det;
	struct snd_soc_codec *codec = mbhc->codec;
	const int idx = wcd9xxx_acdb_mclk_index(mbhc->mbhc_cfg->mclk_rate);

	pr_debug("%s: enter\n", __func__);
	generic = WCD9XXX_MBHC_CAL_GENERAL_PTR(mbhc->mbhc_cfg->calibration);
	btn_det = WCD9XXX_MBHC_CAL_BTN_DET_PTR(mbhc->mbhc_cfg->calibration);

	for (n = 0; n < 8; n++) {
		snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_FIR_B1_CFG,
				    0x07, n);
		snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_FIR_B2_CFG,
			      btn_det->c[n]);
	}

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B2_CTL, 0x07,
			    btn_det->nc);

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_TIMER_B4_CTL, 0x70,
			    generic->mbhc_nsa << 4);

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_TIMER_B4_CTL, 0x0F,
			    btn_det->n_meas);

	snd_soc_write(codec, WCD9XXX_A_CDC_MBHC_TIMER_B5_CTL,
		      generic->mbhc_navg);

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x80, 0x80);

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x78,
			    btn_det->mbhc_nsc << 3);

	if (mbhc->mbhc_cb &&
			mbhc->mbhc_cb->get_cdc_type() !=
					WCD9XXX_CDC_TYPE_HELICON) {
		if (mbhc->resmgr->reg_addr->micb_4_mbhc)
			snd_soc_update_bits(codec,
					mbhc->resmgr->reg_addr->micb_4_mbhc,
					0x03, MBHC_MICBIAS2);
	}

	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B1_CTL, 0x02, 0x02);

	snd_soc_update_bits(codec, WCD9XXX_A_MBHC_SCALING_MUX_2, 0xF0, 0xF0);

	gain = wcd9xxx_mbhc_cal_btn_det_mp(btn_det, MBHC_BTN_DET_GAIN);
	snd_soc_update_bits(codec, WCD9XXX_A_CDC_MBHC_B2_CTL, 0x78,
			    gain[idx] << 3);

	pr_debug("%s: leave\n", __func__);
}

static int wcd9xxx_setup_jack_detect_irq(struct wcd9xxx_mbhc *mbhc)
{
	int ret = 0;
	struct snd_soc_codec *codec = mbhc->codec;
	void *core_res = mbhc->resmgr->core_res;
	int jack_irq;

	if (mbhc->mbhc_cb && mbhc->mbhc_cb->jack_detect_irq)
		jack_irq = mbhc->mbhc_cb->jack_detect_irq(codec);
	else
		jack_irq = WCD9XXX_IRQ_MBHC_JACK_SWITCH_DEFAULT;

	if (mbhc->mbhc_cfg->gpio) {
		ret = request_threaded_irq(mbhc->mbhc_cfg->gpio_irq, NULL,
					   wcd9xxx_mech_plug_detect_irq,
					   (IRQF_TRIGGER_RISING |
					    IRQF_TRIGGER_FALLING |
					    IRQF_DISABLED),
					   "headset detect", mbhc);
		if (ret) {
			pr_err("%s: Failed to request gpio irq %d\n", __func__,
			       mbhc->mbhc_cfg->gpio_irq);
		} else {
			ret = enable_irq_wake(mbhc->mbhc_cfg->gpio_irq);
			if (ret)
				pr_err("%s: Failed to enable wake up irq %d\n",
				       __func__, mbhc->mbhc_cfg->gpio_irq);
		}
	} else if (mbhc->mbhc_cfg->insert_detect) {
		/* Enable HPHL_10K_SW */
		snd_soc_update_bits(mbhc->codec, WCD9XXX_A_RX_HPH_OCP_CTL,
				    1 << 1, 1 << 1);

		ret = wcd9xxx_request_irq(core_res, jack_irq,
					  wcd9xxx_mech_plug_detect_irq,
					  "Jack Detect",
					  mbhc);
		if (ret)
			pr_err("%s: Failed to request insert detect irq %d\n",
				__func__, jack_irq);
	}

	return ret;
}

static int wcd9xxx_init_and_calibrate(struct wcd9xxx_mbhc *mbhc)
{
	int ret = 0;
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);

	/* Enable MCLK during calibration */
	wcd9xxx_onoff_ext_mclk(mbhc, true);
	wcd9xxx_mbhc_setup(mbhc);
	wcd9xxx_mbhc_cal(mbhc);
	wcd9xxx_mbhc_calc_thres(mbhc);
	wcd9xxx_onoff_ext_mclk(mbhc, false);
	wcd9xxx_calibrate_hs_polling(mbhc);

	/* Enable Mic Bias pull down and HPH Switch to GND */
	snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.ctl_reg, 0x01, 0x01);
	snd_soc_update_bits(codec, WCD9XXX_A_MBHC_HPH, 0x01, 0x01);
	INIT_WORK(&mbhc->correct_plug_swch, wcd9xxx_correct_swch_plug);

	if (!IS_ERR_VALUE(ret)) {
		snd_soc_update_bits(codec, WCD9XXX_A_RX_HPH_OCP_CTL, 0x10,
				    0x10);
		wcd9xxx_enable_irq(mbhc->resmgr->core_res,
				   WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
		wcd9xxx_enable_irq(mbhc->resmgr->core_res,
				   WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);

		/* Initialize mechanical mbhc */
		ret = wcd9xxx_setup_jack_detect_irq(mbhc);

		if (!ret && mbhc->mbhc_cfg->gpio) {
			/* Requested with IRQF_DISABLED */
			enable_irq(mbhc->mbhc_cfg->gpio_irq);

			/* Bootup time detection */
			wcd9xxx_swch_irq_handler(mbhc);
		} else if (!ret && mbhc->mbhc_cfg->insert_detect) {
			pr_debug("%s: Setting up codec own insert detection\n",
				 __func__);
			/* Setup for insertion detection */
			wcd9xxx_insert_detect_setup(mbhc, true);
		}
	}

	pr_debug("%s: leave\n", __func__);

	return ret;
}

static void wcd9xxx_mbhc_fw_read(struct work_struct *work)
{
	struct delayed_work *dwork;
	struct wcd9xxx_mbhc *mbhc;
	struct snd_soc_codec *codec;
	const struct firmware *fw;
	int ret = -1, retry = 0;

	dwork = to_delayed_work(work);
	mbhc = container_of(dwork, struct wcd9xxx_mbhc, mbhc_firmware_dwork);
	codec = mbhc->codec;

	while (retry < FW_READ_ATTEMPTS) {
		retry++;
		pr_info("%s:Attempt %d to request MBHC firmware\n",
			__func__, retry);
		ret = request_firmware(&fw, "wcd9320/wcd9320_mbhc.bin",
				       codec->dev);

		if (ret != 0) {
			usleep_range(FW_READ_TIMEOUT, FW_READ_TIMEOUT);
		} else {
			pr_info("%s: MBHC Firmware read succesful\n", __func__);
			break;
		}
	}

	if (ret != 0) {
		pr_err("%s: Cannot load MBHC firmware use default cal\n",
		       __func__);
	} else if (wcd9xxx_mbhc_fw_validate(fw) == false) {
		pr_err("%s: Invalid MBHC cal data size use default cal\n",
		       __func__);
		release_firmware(fw);
	} else {
		mbhc->mbhc_cfg->calibration = (void *)fw->data;
		mbhc->mbhc_fw = fw;
	}

	(void) wcd9xxx_init_and_calibrate(mbhc);
}

#ifdef CONFIG_DEBUG_FS
ssize_t codec_mbhc_debug_read(struct file *file, char __user *buf,
			      size_t count, loff_t *pos)
{
	const int size = 768;
	char buffer[size];
	int n = 0;
	struct wcd9xxx_mbhc *mbhc = file->private_data;
	const struct mbhc_internal_cal_data *p = &mbhc->mbhc_data;
	const s16 v_ins_hu =
	    wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_INS_HU);
	const s16 v_ins_h =
	    wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_INS_H);
	const s16 v_b1_hu =
	    wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_B1_HU);
	const s16 v_b1_h =
	    wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_B1_H);
	const s16 v_br_h =
	    wcd9xxx_get_current_v(mbhc, WCD9XXX_CURRENT_V_BR_H);

	n = scnprintf(buffer, size - n, "dce_z = %x(%dmv)\n",
		      p->dce_z, wcd9xxx_codec_sta_dce_v(mbhc, 1, p->dce_z));
	n += scnprintf(buffer + n, size - n, "dce_mb = %x(%dmv)\n",
		       p->dce_mb, wcd9xxx_codec_sta_dce_v(mbhc, 1, p->dce_mb));
	n += scnprintf(buffer + n, size - n, "dce_nsc_cs_z = %x(%dmv)\n",
		      p->dce_nsc_cs_z,
		      __wcd9xxx_codec_sta_dce_v(mbhc, 1, p->dce_nsc_cs_z,
						p->dce_nsc_cs_z,
						VDDIO_MICBIAS_MV));
	n += scnprintf(buffer + n, size - n, "sta_z = %x(%dmv)\n",
		       p->sta_z, wcd9xxx_codec_sta_dce_v(mbhc, 0, p->sta_z));
	n += scnprintf(buffer + n, size - n, "sta_mb = %x(%dmv)\n",
		       p->sta_mb, wcd9xxx_codec_sta_dce_v(mbhc, 0, p->sta_mb));
	n += scnprintf(buffer + n, size - n, "t_dce = %d\n",  p->t_dce);
	n += scnprintf(buffer + n, size - n, "t_sta = %d\n",  p->t_sta);
	n += scnprintf(buffer + n, size - n, "micb_mv = %dmv\n", p->micb_mv);
	n += scnprintf(buffer + n, size - n, "v_ins_hu = %x(%dmv)\n",
		       v_ins_hu, wcd9xxx_codec_sta_dce_v(mbhc, 0, v_ins_hu));
	n += scnprintf(buffer + n, size - n, "v_ins_h = %x(%dmv)\n",
		       v_ins_h, wcd9xxx_codec_sta_dce_v(mbhc, 1, v_ins_h));
	n += scnprintf(buffer + n, size - n, "v_b1_hu = %x(%dmv)\n",
		       v_b1_hu, wcd9xxx_codec_sta_dce_v(mbhc, 0, v_b1_hu));
	n += scnprintf(buffer + n, size - n, "v_b1_h = %x(%dmv)\n",
		       v_b1_h, wcd9xxx_codec_sta_dce_v(mbhc, 1, v_b1_h));
	n += scnprintf(buffer + n, size - n, "v_brh = %x(%dmv)\n",
		       v_br_h, wcd9xxx_codec_sta_dce_v(mbhc, 1, v_br_h));
	n += scnprintf(buffer + n, size - n, "v_brl = %x(%dmv)\n",  p->v_brl,
		       wcd9xxx_codec_sta_dce_v(mbhc, 0, p->v_brl));
	n += scnprintf(buffer + n, size - n, "v_no_mic = %x(%dmv)\n",
		       p->v_no_mic,
		       wcd9xxx_codec_sta_dce_v(mbhc, 0, p->v_no_mic));
	n += scnprintf(buffer + n, size - n, "v_inval_ins_low = %d\n",
		       p->v_inval_ins_low);
	n += scnprintf(buffer + n, size - n, "v_inval_ins_high = %d\n",
		       p->v_inval_ins_high);
	n += scnprintf(buffer + n, size - n, "Insert detect insert = %d\n",
		       !wcd9xxx_swch_level_remove(mbhc));
	buffer[n] = 0;

	return simple_read_from_buffer(buf, count, pos, buffer, n);
}

static int codec_debug_open(struct inode *inode, struct file *file)
{
	file->private_data = inode->i_private;
	return 0;
}

static ssize_t codec_debug_write(struct file *filp,
				 const char __user *ubuf, size_t cnt,
				 loff_t *ppos)
{
	char lbuf[32];
	char *buf;
	int rc;
	struct wcd9xxx_mbhc *mbhc = filp->private_data;

	if (cnt > sizeof(lbuf) - 1)
		return -EINVAL;

	rc = copy_from_user(lbuf, ubuf, cnt);
	if (rc)
		return -EFAULT;

	lbuf[cnt] = '\0';
	buf = (char *)lbuf;
	mbhc->no_mic_headset_override = (*strsep(&buf, " ") == '0') ?
					     false : true;
	return rc;
}

static const struct file_operations mbhc_trrs_debug_ops = {
	.open = codec_debug_open,
	.write = codec_debug_write,
};

static const struct file_operations mbhc_debug_ops = {
	.open = codec_debug_open,
	.read = codec_mbhc_debug_read,
};

static void wcd9xxx_init_debugfs(struct wcd9xxx_mbhc *mbhc)
{
	mbhc->debugfs_poke =
	    debugfs_create_file("TRRS", S_IFREG | S_IRUGO, NULL, mbhc,
				&mbhc_trrs_debug_ops);
	mbhc->debugfs_mbhc =
	    debugfs_create_file("wcd9xxx_mbhc", S_IFREG | S_IRUGO,
				NULL, mbhc, &mbhc_debug_ops);
}

static void wcd9xxx_cleanup_debugfs(struct wcd9xxx_mbhc *mbhc)
{
	debugfs_remove(mbhc->debugfs_poke);
	debugfs_remove(mbhc->debugfs_mbhc);
}
#else
static void wcd9xxx_init_debugfs(struct wcd9xxx_mbhc *mbhc)
{
}

static void wcd9xxx_cleanup_debugfs(struct wcd9xxx_mbhc *mbhc)
{
}
#endif

int wcd9xxx_mbhc_start(struct wcd9xxx_mbhc *mbhc,
		       struct wcd9xxx_mbhc_config *mbhc_cfg)
{
	int rc = 0;
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);

	if (!codec) {
		pr_err("%s: no codec\n", __func__);
		return -EINVAL;
	}

	if (mbhc_cfg->mclk_rate != MCLK_RATE_12288KHZ &&
	    mbhc_cfg->mclk_rate != MCLK_RATE_9600KHZ) {
		pr_err("Error: unsupported clock rate %d\n",
		       mbhc_cfg->mclk_rate);
		return -EINVAL;
	}

	/* Save mbhc config */
	mbhc->mbhc_cfg = mbhc_cfg;

	/* Get HW specific mbhc registers' address */
	wcd9xxx_get_mbhc_micbias_regs(mbhc, &mbhc->mbhc_bias_regs);

	/* Put CFILT in fast mode by default */
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->cfilt_fast_mode)
		mbhc->mbhc_cb->cfilt_fast_mode(codec, mbhc);
	else
		snd_soc_update_bits(codec, mbhc->mbhc_bias_regs.cfilt_ctl,
		0x40, WCD9XXX_CFILT_FAST_MODE);

	/*
	 * setup internal micbias if codec uses internal micbias for
	 * headset detection
	 */
	if (mbhc->mbhc_cfg->use_int_rbias) {
		if (mbhc->mbhc_cb && mbhc->mbhc_cb->setup_int_rbias)
			mbhc->mbhc_cb->setup_int_rbias(codec, true);
		else
			pr_info("%s: internal bias is requested but codec did not provide callback\n",
				__func__);
	}

	/*
	 * If codec has specific clock gating for MBHC,
	 * remove the clock gate
	 */
	if (mbhc->mbhc_cb &&
			mbhc->mbhc_cb->enable_clock_gate)
		mbhc->mbhc_cb->enable_clock_gate(mbhc->codec, true);

	if (!mbhc->mbhc_cfg->read_fw_bin ||
	    (mbhc->mbhc_cfg->read_fw_bin && mbhc->mbhc_fw)) {
		rc = wcd9xxx_init_and_calibrate(mbhc);
	} else {
		if (!mbhc->mbhc_fw)
			schedule_delayed_work(&mbhc->mbhc_firmware_dwork,
					     usecs_to_jiffies(FW_READ_TIMEOUT));
		else
			pr_debug("%s: Skipping to read mbhc fw, 0x%p\n",
				 __func__, mbhc->mbhc_fw);
	}

	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}
EXPORT_SYMBOL(wcd9xxx_mbhc_start);

void wcd9xxx_mbhc_stop(struct wcd9xxx_mbhc *mbhc)
{
	if (mbhc->mbhc_fw) {
		cancel_delayed_work_sync(&mbhc->mbhc_firmware_dwork);
		release_firmware(mbhc->mbhc_fw);
		mbhc->mbhc_fw = NULL;
	}
}
EXPORT_SYMBOL(wcd9xxx_mbhc_stop);

static enum wcd9xxx_micbias_num
wcd9xxx_event_to_micbias(const enum wcd9xxx_notify_event event)
{
	enum wcd9xxx_micbias_num ret;
	switch (event) {
	case WCD9XXX_EVENT_PRE_MICBIAS_1_ON:
	case WCD9XXX_EVENT_PRE_MICBIAS_1_OFF:
	case WCD9XXX_EVENT_POST_MICBIAS_1_ON:
	case WCD9XXX_EVENT_POST_MICBIAS_1_OFF:
		ret = MBHC_MICBIAS1;
		break;
	case WCD9XXX_EVENT_PRE_MICBIAS_2_ON:
	case WCD9XXX_EVENT_PRE_MICBIAS_2_OFF:
	case WCD9XXX_EVENT_POST_MICBIAS_2_ON:
	case WCD9XXX_EVENT_POST_MICBIAS_2_OFF:
		ret = MBHC_MICBIAS2;
		break;
	case WCD9XXX_EVENT_PRE_MICBIAS_3_ON:
	case WCD9XXX_EVENT_PRE_MICBIAS_3_OFF:
	case WCD9XXX_EVENT_POST_MICBIAS_3_ON:
	case WCD9XXX_EVENT_POST_MICBIAS_3_OFF:
		ret = MBHC_MICBIAS3;
		break;
	case WCD9XXX_EVENT_PRE_MICBIAS_4_ON:
	case WCD9XXX_EVENT_PRE_MICBIAS_4_OFF:
	case WCD9XXX_EVENT_POST_MICBIAS_4_ON:
	case WCD9XXX_EVENT_POST_MICBIAS_4_OFF:
		ret = MBHC_MICBIAS4;
		break;
	default:
		WARN_ONCE(1, "Cannot convert event %d to micbias\n", event);
		ret = MBHC_MICBIAS_INVALID;
		break;
	}
	return ret;
}

static int wcd9xxx_event_to_cfilt(const enum wcd9xxx_notify_event event)
{
	int ret;
	switch (event) {
	case WCD9XXX_EVENT_PRE_CFILT_1_OFF:
	case WCD9XXX_EVENT_POST_CFILT_1_OFF:
	case WCD9XXX_EVENT_PRE_CFILT_1_ON:
	case WCD9XXX_EVENT_POST_CFILT_1_ON:
		ret = WCD9XXX_CFILT1_SEL;
		break;
	case WCD9XXX_EVENT_PRE_CFILT_2_OFF:
	case WCD9XXX_EVENT_POST_CFILT_2_OFF:
	case WCD9XXX_EVENT_PRE_CFILT_2_ON:
	case WCD9XXX_EVENT_POST_CFILT_2_ON:
		ret = WCD9XXX_CFILT2_SEL;
		break;
	case WCD9XXX_EVENT_PRE_CFILT_3_OFF:
	case WCD9XXX_EVENT_POST_CFILT_3_OFF:
	case WCD9XXX_EVENT_PRE_CFILT_3_ON:
	case WCD9XXX_EVENT_POST_CFILT_3_ON:
		ret = WCD9XXX_CFILT3_SEL;
		break;
	default:
		ret = -1;
	}
	return ret;
}

static int wcd9xxx_get_mbhc_cfilt_sel(struct wcd9xxx_mbhc *mbhc)
{
	int cfilt;
	const struct wcd9xxx_pdata *pdata = mbhc->resmgr->pdata;

	switch (mbhc->mbhc_cfg->micbias) {
	case MBHC_MICBIAS1:
		cfilt = pdata->micbias.bias1_cfilt_sel;
		break;
	case MBHC_MICBIAS2:
		cfilt = pdata->micbias.bias2_cfilt_sel;
		break;
	case MBHC_MICBIAS3:
		cfilt = pdata->micbias.bias3_cfilt_sel;
		break;
	case MBHC_MICBIAS4:
		cfilt = pdata->micbias.bias4_cfilt_sel;
		break;
	default:
		cfilt = MBHC_MICBIAS_INVALID;
		break;
	}
	return cfilt;
}

static void wcd9xxx_enable_mbhc_txfe(struct wcd9xxx_mbhc *mbhc, bool on)
{
	if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mbhc_txfe)
		mbhc->mbhc_cb->enable_mbhc_txfe(mbhc->codec, on);
	else
		snd_soc_update_bits(mbhc->codec, WCD9XXX_A_TX_7_MBHC_TEST_CTL,
				    0x40, on ? 0x40 : 0x00);
}

static int wcd9xxx_event_notify(struct notifier_block *self, unsigned long val,
				void *data)
{
	int ret = 0;
	struct wcd9xxx_mbhc *mbhc = ((struct wcd9xxx_resmgr *)data)->mbhc;
	struct snd_soc_codec *codec = mbhc->codec;
	enum wcd9xxx_notify_event event = (enum wcd9xxx_notify_event)val;

	pr_debug("%s: enter event %s(%d)\n", __func__,
		 wcd9xxx_get_event_string(event), event);

	switch (event) {
	/* MICBIAS usage change */
	case WCD9XXX_EVENT_PRE_MICBIAS_1_ON:
	case WCD9XXX_EVENT_PRE_MICBIAS_2_ON:
	case WCD9XXX_EVENT_PRE_MICBIAS_3_ON:
	case WCD9XXX_EVENT_PRE_MICBIAS_4_ON:
		if (mbhc->mbhc_cfg && mbhc->mbhc_cfg->micbias ==
		    wcd9xxx_event_to_micbias(event)) {
			wcd9xxx_switch_micbias(mbhc, 0);
			/*
			 * Enable MBHC TxFE whenever  micbias is
			 * turned ON and polling is active
			 */
			if (mbhc->polling_active)
				wcd9xxx_enable_mbhc_txfe(mbhc, true);
		}
		break;
	case WCD9XXX_EVENT_POST_MICBIAS_1_ON:
	case WCD9XXX_EVENT_POST_MICBIAS_2_ON:
	case WCD9XXX_EVENT_POST_MICBIAS_3_ON:
	case WCD9XXX_EVENT_POST_MICBIAS_4_ON:
		if (mbhc->mbhc_cfg && mbhc->mbhc_cfg->micbias ==
		    wcd9xxx_event_to_micbias(event) &&
		    wcd9xxx_mbhc_polling(mbhc)) {
			/* if polling is on, restart it */
			wcd9xxx_pause_hs_polling(mbhc);
			wcd9xxx_start_hs_polling(mbhc);
		}
		break;
	case WCD9XXX_EVENT_POST_MICBIAS_1_OFF:
	case WCD9XXX_EVENT_POST_MICBIAS_2_OFF:
	case WCD9XXX_EVENT_POST_MICBIAS_3_OFF:
	case WCD9XXX_EVENT_POST_MICBIAS_4_OFF:
		if (mbhc->mbhc_cfg && mbhc->mbhc_cfg->micbias ==
		    wcd9xxx_event_to_micbias(event)) {
			if (mbhc->event_state &
			   (1 << MBHC_EVENT_PA_HPHL | 1 << MBHC_EVENT_PA_HPHR))
				wcd9xxx_switch_micbias(mbhc, 1);
			/*
			 * Disable MBHC TxFE, in case it was enabled
			 * earlier when micbias was enabled.
			 */
			wcd9xxx_enable_mbhc_txfe(mbhc, false);
		}
		break;
	/* PA usage change */
	case WCD9XXX_EVENT_PRE_HPHL_PA_ON:
		set_bit(MBHC_EVENT_PA_HPHL, &mbhc->event_state);
		if (!(snd_soc_read(codec, mbhc->mbhc_bias_regs.ctl_reg) & 0x80))
			/* if micbias is not enabled, switch to vddio */
			wcd9xxx_switch_micbias(mbhc, 1);
		break;
	case WCD9XXX_EVENT_PRE_HPHR_PA_ON:
		set_bit(MBHC_EVENT_PA_HPHR, &mbhc->event_state);
		break;
	case WCD9XXX_EVENT_POST_HPHL_PA_OFF:
		clear_bit(MBHC_EVENT_PA_HPHL, &mbhc->event_state);
		/* if HPH PAs are off, report OCP and switch back to CFILT */
		clear_bit(WCD9XXX_HPHL_PA_OFF_ACK, &mbhc->hph_pa_dac_state);
		clear_bit(WCD9XXX_HPHL_DAC_OFF_ACK, &mbhc->hph_pa_dac_state);
		if (mbhc->hph_status & SND_JACK_OC_HPHL)
			hphlocp_off_report(mbhc, SND_JACK_OC_HPHL);
		if (!(mbhc->event_state &
		      (1 << MBHC_EVENT_PA_HPHL | 1 << MBHC_EVENT_PA_HPHR)))
			wcd9xxx_switch_micbias(mbhc, 0);
		break;
	case WCD9XXX_EVENT_POST_HPHR_PA_OFF:
		clear_bit(MBHC_EVENT_PA_HPHR, &mbhc->event_state);
		/* if HPH PAs are off, report OCP and switch back to CFILT */
		clear_bit(WCD9XXX_HPHR_PA_OFF_ACK, &mbhc->hph_pa_dac_state);
		clear_bit(WCD9XXX_HPHR_DAC_OFF_ACK, &mbhc->hph_pa_dac_state);
		if (mbhc->hph_status & SND_JACK_OC_HPHR)
			hphrocp_off_report(mbhc, SND_JACK_OC_HPHL);
		if (!(mbhc->event_state &
		      (1 << MBHC_EVENT_PA_HPHL | 1 << MBHC_EVENT_PA_HPHR)))
			wcd9xxx_switch_micbias(mbhc, 0);
		break;
	/* Clock usage change */
	case WCD9XXX_EVENT_PRE_MCLK_ON:
		break;
	case WCD9XXX_EVENT_POST_MCLK_ON:
		/* Change to lower TxAAF frequency */
		snd_soc_update_bits(codec, WCD9XXX_A_TX_COM_BIAS, 1 << 4,
				    1 << 4);
		/* Re-calibrate clock rate dependent values */
		wcd9xxx_update_mbhc_clk_rate(mbhc, mbhc->mbhc_cfg->mclk_rate);
		/* If clock source changes, stop and restart polling */
		if (wcd9xxx_mbhc_polling(mbhc)) {
			wcd9xxx_calibrate_hs_polling(mbhc);
			wcd9xxx_start_hs_polling(mbhc);
		}
		break;
	case WCD9XXX_EVENT_PRE_MCLK_OFF:
		/* If clock source changes, stop and restart polling */
		if (wcd9xxx_mbhc_polling(mbhc))
			wcd9xxx_pause_hs_polling(mbhc);
		break;
	case WCD9XXX_EVENT_POST_MCLK_OFF:
		break;
	case WCD9XXX_EVENT_PRE_RCO_ON:
		break;
	case WCD9XXX_EVENT_POST_RCO_ON:
		/* Change to higher TxAAF frequency */
		snd_soc_update_bits(codec, WCD9XXX_A_TX_COM_BIAS, 1 << 4,
				    0 << 4);
		/* Re-calibrate clock rate dependent values */
		wcd9xxx_update_mbhc_clk_rate(mbhc, mbhc->rco_clk_rate);
		/* If clock source changes, stop and restart polling */
		if (wcd9xxx_mbhc_polling(mbhc)) {
			wcd9xxx_calibrate_hs_polling(mbhc);
			wcd9xxx_start_hs_polling(mbhc);
		}
		break;
	case WCD9XXX_EVENT_PRE_RCO_OFF:
		/* If clock source changes, stop and restart polling */
		if (wcd9xxx_mbhc_polling(mbhc))
			wcd9xxx_pause_hs_polling(mbhc);
		break;
	case WCD9XXX_EVENT_POST_RCO_OFF:
		break;
	/* CFILT usage change */
	case WCD9XXX_EVENT_PRE_CFILT_1_ON:
	case WCD9XXX_EVENT_PRE_CFILT_2_ON:
	case WCD9XXX_EVENT_PRE_CFILT_3_ON:
		if (wcd9xxx_get_mbhc_cfilt_sel(mbhc) ==
		    wcd9xxx_event_to_cfilt(event))
			/*
			 * Switch CFILT to slow mode if MBHC CFILT is being
			 * used.
			 */
			wcd9xxx_codec_switch_cfilt_mode(mbhc, false);
		break;
	case WCD9XXX_EVENT_POST_CFILT_1_OFF:
	case WCD9XXX_EVENT_POST_CFILT_2_OFF:
	case WCD9XXX_EVENT_POST_CFILT_3_OFF:
		if (wcd9xxx_get_mbhc_cfilt_sel(mbhc) ==
		    wcd9xxx_event_to_cfilt(event))
			/*
			 * Switch CFILT to fast mode if MBHC CFILT is not
			 * used anymore.
			 */
			wcd9xxx_codec_switch_cfilt_mode(mbhc, true);
		break;
	/* System resume */
	case WCD9XXX_EVENT_POST_RESUME:
		mbhc->mbhc_last_resume = jiffies;
		break;
	/* BG mode chage */
	case WCD9XXX_EVENT_PRE_BG_OFF:
	case WCD9XXX_EVENT_POST_BG_OFF:
	case WCD9XXX_EVENT_PRE_BG_AUDIO_ON:
	case WCD9XXX_EVENT_POST_BG_AUDIO_ON:
	case WCD9XXX_EVENT_PRE_BG_MBHC_ON:
	case WCD9XXX_EVENT_POST_BG_MBHC_ON:
		/* Not used for now */
		break;
	default:
		WARN(1, "Unknown event %d\n", event);
		ret = -EINVAL;
	}

	pr_debug("%s: leave\n", __func__);

	return ret;
}

static int wcd9xxx_detect_impedance(struct wcd9xxx_mbhc *mbhc, uint32_t *zl,
				    uint32_t *zr)
{
	int i;
	int ret = 0;
	s16 l[3], r[3];
	s16 *z[] = {
		&l[0], &r[0], &r[1], &l[1], &l[2], &r[2],
	};
	struct snd_soc_codec *codec = mbhc->codec;
	const int mux_wait_us = 25;
	const struct wcd9xxx_reg_mask_val reg_set_mux[] = {
		/* Phase 1 */
		/* Set MBHC_MUX for HPHL without ical */
		{WCD9XXX_A_MBHC_SCALING_MUX_2, 0xFF, 0xF0},
		/* Set MBHC_MUX for HPHR without ical */
		{WCD9XXX_A_MBHC_SCALING_MUX_1, 0xFF, 0xA0},
		/* Set MBHC_MUX for HPHR with ical */
		{WCD9XXX_A_MBHC_SCALING_MUX_2, 0xFF, 0xF8},
		/* Set MBHC_MUX for HPHL with ical */
		{WCD9XXX_A_MBHC_SCALING_MUX_1, 0xFF, 0xC0},

		/* Phase 2 */
		{WCD9XXX_A_MBHC_SCALING_MUX_2, 0xFF, 0xF0},
		/* Set MBHC_MUX for HPHR without ical and wait for 25us */
		{WCD9XXX_A_MBHC_SCALING_MUX_1, 0xFF, 0xA0},
	};

	pr_debug("%s: enter\n", __func__);
	WCD9XXX_BCL_ASSERT_LOCKED(mbhc->resmgr);

	if (!mbhc->mbhc_cb || !mbhc->mbhc_cb->setup_zdet ||
	    !mbhc->mbhc_cb->compute_impedance || !zl ||
	    !zr)
		return -EINVAL;

	/*
	 * Impedance detection is an intrusive function as it mutes RX paths,
	 * enable PAs and etc.  Therefore codec drvier including ALSA
	 * shouldn't read and write hardware registers during detection.
	 */
	mutex_lock(&codec->mutex);

	WCD9XXX_BG_CLK_LOCK(mbhc->resmgr);
	/*
	 * Fast(mbhc) mode bandagap doesn't need to be enabled explicitly
	 * since fast mode is set by MBHC hardware when override is on.
	 * Enable bandgap mode to avoid unnecessary RCO disable and enable
	 * during clock source change.
	 */
	wcd9xxx_resmgr_get_bandgap(mbhc->resmgr, WCD9XXX_BANDGAP_AUDIO_MODE);
	wcd9xxx_resmgr_get_clk_block(mbhc->resmgr, WCD9XXX_CLK_RCO);
	WCD9XXX_BG_CLK_UNLOCK(mbhc->resmgr);

	wcd9xxx_turn_onoff_override(mbhc, true);
	pr_debug("%s: Setting impedance detection\n", __func__);

	/* Codec specific setup for L0, R0, L1 and R1 measurements */
	mbhc->mbhc_cb->setup_zdet(mbhc, PRE_MEAS);

	pr_debug("%s: Performing impedance detection\n", __func__);
	for (i = 0; i < ARRAY_SIZE(reg_set_mux) - 2; i++) {
		snd_soc_update_bits(codec, reg_set_mux[i].reg,
				    reg_set_mux[i].mask,
				    reg_set_mux[i].val);
		if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mux_bias_block)
			mbhc->mbhc_cb->enable_mux_bias_block(codec);
		else
			snd_soc_update_bits(codec,
					    WCD9XXX_A_MBHC_SCALING_MUX_1,
					    0x80, 0x80);
		/* 25us is required after mux change to settle down */
		usleep_range(mux_wait_us,
			     mux_wait_us + WCD9XXX_USLEEP_RANGE_MARGIN_US);
		*(z[i]) = __wcd9xxx_codec_sta_dce(mbhc, 0, true, false);
	}

	/* Codec specific setup for L2 and R2 measurements */
	mbhc->mbhc_cb->setup_zdet(mbhc, POST_MEAS);

	for (; i < ARRAY_SIZE(reg_set_mux); i++) {
		snd_soc_update_bits(codec, reg_set_mux[i].reg,
				    reg_set_mux[i].mask,
				    reg_set_mux[i].val);
		if (mbhc->mbhc_cb && mbhc->mbhc_cb->enable_mux_bias_block)
			mbhc->mbhc_cb->enable_mux_bias_block(codec);
		else
			snd_soc_update_bits(codec,
					    WCD9XXX_A_MBHC_SCALING_MUX_1,
					    0x80, 0x80);
		/* 25us is required after mux change to settle down */
		usleep_range(mux_wait_us,
			     mux_wait_us + WCD9XXX_USLEEP_RANGE_MARGIN_US);
		*(z[i]) = __wcd9xxx_codec_sta_dce(mbhc, 0, true, false);
	}

	mbhc->mbhc_cb->setup_zdet(mbhc, PA_DISABLE);

	mutex_unlock(&codec->mutex);

	WCD9XXX_BG_CLK_LOCK(mbhc->resmgr);
	wcd9xxx_resmgr_put_bandgap(mbhc->resmgr, WCD9XXX_BANDGAP_AUDIO_MODE);
	wcd9xxx_resmgr_put_clk_block(mbhc->resmgr, WCD9XXX_CLK_RCO);
	WCD9XXX_BG_CLK_UNLOCK(mbhc->resmgr);

	wcd9xxx_turn_onoff_override(mbhc, false);
	mbhc->mbhc_cb->compute_impedance(l, r, zl, zr);

	pr_debug("%s: L0: 0x%x(%d), L1: 0x%x(%d), L2: 0x%x(%d)\n",
		 __func__,
		 l[0] & 0xffff, l[0], l[1] & 0xffff, l[1], l[2] & 0xffff, l[2]);
	pr_debug("%s: R0: 0x%x(%d), R1: 0x%x(%d), R2: 0x%x(%d)\n",
		 __func__,
		 r[0] & 0xffff, r[0], r[1] & 0xffff, r[1], r[2] & 0xffff, r[2]);
	pr_debug("%s: RL %d milliohm, RR %d milliohm\n", __func__, *zl, *zr);
	pr_debug("%s: Impedance detection completed\n", __func__);

	return ret;
}

int wcd9xxx_mbhc_get_impedance(struct wcd9xxx_mbhc *mbhc, uint32_t *zl,
			       uint32_t *zr)
{
	WCD9XXX_BCL_LOCK(mbhc->resmgr);
	*zl = mbhc->zl;
	*zr = mbhc->zr;
	WCD9XXX_BCL_UNLOCK(mbhc->resmgr);

	if (*zl && *zr)
		return 0;
	else
		return -EINVAL;
}

/*
 * wcd9xxx_mbhc_init : initialize MBHC internal structures.
 *
 * NOTE: mbhc->mbhc_cfg is not YET configure so shouldn't be used
 */
int wcd9xxx_mbhc_init(struct wcd9xxx_mbhc *mbhc, struct wcd9xxx_resmgr *resmgr,
		      struct snd_soc_codec *codec,
		      int (*micbias_enable_cb) (struct snd_soc_codec*,  bool),
		      const struct wcd9xxx_mbhc_cb *mbhc_cb, int rco_clk_rate,
		      bool impedance_det_en)
{
	int ret;
	void *core_res;

	pr_debug("%s: enter\n", __func__);
	memset(&mbhc->mbhc_bias_regs, 0, sizeof(struct mbhc_micbias_regs));
	memset(&mbhc->mbhc_data, 0, sizeof(struct mbhc_internal_cal_data));

	mbhc->mbhc_data.t_sta_dce = DEFAULT_DCE_STA_WAIT;
	mbhc->mbhc_data.t_dce = DEFAULT_DCE_WAIT;
	mbhc->mbhc_data.t_sta = DEFAULT_STA_WAIT;
	mbhc->mbhc_micbias_switched = false;
	mbhc->polling_active = false;
	mbhc->mbhc_state = MBHC_STATE_NONE;
	mbhc->in_swch_irq_handler = false;
	mbhc->current_plug = PLUG_TYPE_NONE;
	mbhc->lpi_enabled = false;
	mbhc->no_mic_headset_override = false;
	mbhc->mbhc_last_resume = 0;
	mbhc->codec = codec;
	mbhc->resmgr = resmgr;
	mbhc->resmgr->mbhc = mbhc;
	mbhc->micbias_enable_cb = micbias_enable_cb;
	mbhc->rco_clk_rate = rco_clk_rate;
	mbhc->mbhc_cb = mbhc_cb;
	mbhc->impedance_detect = impedance_det_en;
	impedance_detect_en = impedance_det_en ? 1 : 0;

	if (mbhc->headset_jack.jack == NULL) {
		ret = snd_soc_jack_new(codec, "Headset Jack", WCD9XXX_JACK_MASK,
				       &mbhc->headset_jack);
		if (ret) {
			pr_err("%s: Failed to create new jack\n", __func__);
			return ret;
		}

		ret = snd_soc_jack_new(codec, "Button Jack",
				       WCD9XXX_JACK_BUTTON_MASK,
				       &mbhc->button_jack);
		if (ret) {
			pr_err("Failed to create new jack\n");
			return ret;
		}

		ret = snd_jack_set_key(mbhc->button_jack.jack,
				       SND_JACK_BTN_0,
				       KEY_MEDIA);
		if (ret) {
			pr_err("%s: Failed to set code for btn-0\n",
				__func__);
			return ret;
		}

		INIT_DELAYED_WORK(&mbhc->mbhc_firmware_dwork,
				  wcd9xxx_mbhc_fw_read);
		INIT_DELAYED_WORK(&mbhc->mbhc_btn_dwork, wcd9xxx_btn_lpress_fn);
		INIT_DELAYED_WORK(&mbhc->mbhc_insert_dwork,
				  wcd9xxx_mbhc_insert_work);
	}

	/* Register event notifier */
	mbhc->nblock.notifier_call = wcd9xxx_event_notify;
	ret = wcd9xxx_resmgr_register_notifier(mbhc->resmgr, &mbhc->nblock);
	if (ret) {
		pr_err("%s: Failed to register notifier %d\n", __func__, ret);
		return ret;
	}

	wcd9xxx_init_debugfs(mbhc);

	core_res = mbhc->resmgr->core_res;
	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION,
				  wcd9xxx_hs_insert_irq,
				  "Headset insert detect", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d, ret = %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_INSERTION, ret);
		goto err_insert_irq;
	}
	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION);

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_MBHC_REMOVAL,
				  wcd9xxx_hs_remove_irq,
				  "Headset remove detect", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			WCD9XXX_IRQ_MBHC_REMOVAL);
		goto err_remove_irq;
	}

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_MBHC_POTENTIAL,
				  wcd9xxx_dce_handler, "DC Estimation detect",
				  mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_MBHC_POTENTIAL);
		goto err_potential_irq;
	}

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_MBHC_RELEASE,
				  wcd9xxx_release_handler,
				  "Button Release detect", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			WCD9XXX_IRQ_MBHC_RELEASE);
		goto err_release_irq;
	}

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPL_FAULT,
				  wcd9xxx_hphl_ocp_irq, "HPH_L OCP detect",
				  mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);
		goto err_hphl_ocp_irq;
	}
	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPL_FAULT);

	ret = wcd9xxx_request_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPR_FAULT,
				  wcd9xxx_hphr_ocp_irq, "HPH_R OCP detect",
				  mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);
		goto err_hphr_ocp_irq;
	}
	wcd9xxx_disable_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPR_FAULT);

	wcd9xxx_regmgr_cond_register(resmgr, 1 << WCD9XXX_COND_HPH_MIC |
					     1 << WCD9XXX_COND_HPH);

	pr_debug("%s: leave ret %d\n", __func__, ret);
	return ret;

err_hphr_ocp_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPL_FAULT, mbhc);
err_hphl_ocp_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_RELEASE, mbhc);
err_release_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_POTENTIAL, mbhc);
err_potential_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_REMOVAL, mbhc);
err_remove_irq:
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION, mbhc);
err_insert_irq:
	wcd9xxx_resmgr_unregister_notifier(mbhc->resmgr, &mbhc->nblock);

	pr_debug("%s: leave ret %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(wcd9xxx_mbhc_init);

void wcd9xxx_mbhc_deinit(struct wcd9xxx_mbhc *mbhc)
{
	struct wcd9xxx_core_resource *core_res =
				mbhc->resmgr->core_res;

	wcd9xxx_regmgr_cond_deregister(mbhc->resmgr, 1 << WCD9XXX_COND_HPH_MIC |
						     1 << WCD9XXX_COND_HPH);

	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_RELEASE, mbhc);
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_POTENTIAL, mbhc);
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_REMOVAL, mbhc);
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_MBHC_INSERTION, mbhc);

	if (mbhc->mbhc_cb && mbhc->mbhc_cb->free_irq)
		mbhc->mbhc_cb->free_irq(mbhc);
	else
		wcd9xxx_free_irq(core_res, WCD9320_IRQ_MBHC_JACK_SWITCH,
				 mbhc);

	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPL_FAULT, mbhc);
	wcd9xxx_free_irq(core_res, WCD9XXX_IRQ_HPH_PA_OCPR_FAULT, mbhc);

	wcd9xxx_resmgr_unregister_notifier(mbhc->resmgr, &mbhc->nblock);

	wcd9xxx_cleanup_debugfs(mbhc);
}
EXPORT_SYMBOL(wcd9xxx_mbhc_deinit);

MODULE_DESCRIPTION("wcd9xxx MBHC module");
MODULE_LICENSE("GPL v2");
