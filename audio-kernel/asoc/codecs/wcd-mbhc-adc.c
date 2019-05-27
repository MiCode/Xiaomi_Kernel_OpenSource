/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/printk.h>
#include <linux/ratelimit.h>
#include <linux/list.h>
#include <linux/bitops.h>
#include <linux/delay.h>
#include <linux/pm_runtime.h>
#include <linux/kernel.h>
#include <linux/input.h>
#include <linux/firmware.h>
#include <linux/completion.h>
#include <sound/soc.h>
#include <sound/jack.h>
#include "wcd-mbhc-adc.h"
#include "wcd-mbhc-v2.h"

#define WCD_MBHC_ADC_HS_THRESHOLD_MV    1700
#define WCD_MBHC_ADC_HPH_THRESHOLD_MV   75
#define WCD_MBHC_ADC_MICBIAS_MV         1800

static int wcd_mbhc_get_micbias(struct wcd_mbhc *mbhc)
{
	int micbias = 0;
	u8 vout_ctl = 0;

	/* Read MBHC Micbias (Mic Bias2) voltage */
	WCD_MBHC_REG_READ(WCD_MBHC_MICB2_VOUT, vout_ctl);

	/* Formula for getting micbias from vout
	 * micbias = 1.0V + VOUT_CTL * 50mV
	 */
	micbias = 1000 + (vout_ctl * 50);
	pr_debug("%s: vout_ctl: %d, micbias: %d\n",
		 __func__, vout_ctl, micbias);

	return micbias;
}

static int wcd_get_voltage_from_adc(u8 val, int micbias)
{
	/* Formula for calculating voltage from ADC
	 * Voltage = ADC_RESULT*12.5mV*V_MICBIAS/1.8
	 */
	return ((val * 125 * micbias)/(WCD_MBHC_ADC_MICBIAS_MV * 10));
}

static int wcd_measure_adc_continuous(struct wcd_mbhc *mbhc)
{
	u8 adc_result = 0;
	int output_mv = 0;
	int retry = 3;
	u8 adc_en = 0;

	pr_debug("%s: enter\n", __func__);

	/* Pre-requisites for ADC continuous measurement */
	/* Read legacy electircal detection and disable */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0x00);
	/* Set ADC to continuous measurement */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_MODE, 1);
	/* Read ADC Enable bit to restore after adc measurement */
	WCD_MBHC_REG_READ(WCD_MBHC_ADC_EN, adc_en);
	/* Disable ADC_ENABLE bit */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, 0);
	/* Disable MBHC FSM */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
	/* Set the MUX selection to IN2P */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MUX_CTL, MUX_CTL_IN2P);
	/* Enable MBHC FSM */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
	/* Enable ADC_ENABLE bit */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, 1);

	while (retry--) {
		/* wait for 3 msec before reading ADC result */
		usleep_range(3000, 3100);

		/* Read ADC result */
		WCD_MBHC_REG_READ(WCD_MBHC_ADC_RESULT, adc_result);
	}

	/* Restore ADC Enable */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, adc_en);
	/* Get voltage from ADC result */
	output_mv = wcd_get_voltage_from_adc(adc_result,
					     wcd_mbhc_get_micbias(mbhc));
	pr_debug("%s: adc_result: 0x%x, output_mv: %d\n",
		 __func__, adc_result, output_mv);

	return output_mv;
}

static int wcd_measure_adc_once(struct wcd_mbhc *mbhc, int mux_ctl)
{
	u8 adc_timeout = 0;
	u8 adc_complete = 0;
	u8 adc_result = 0;
	int retry = 6;
	int ret = 0;
	int output_mv = 0;
	u8 adc_en = 0;

	pr_debug("%s: enter\n", __func__);

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_MODE, 0);
	/* Read ADC Enable bit to restore after adc measurement */
	WCD_MBHC_REG_READ(WCD_MBHC_ADC_EN, adc_en);
	/* Trigger ADC one time measurement */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, 0);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
	/* Set the appropriate MUX selection */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MUX_CTL, mux_ctl);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, 1);

	while (retry--) {
		/* wait for 600usec to get adc results */
		usleep_range(600, 610);

		/* check for ADC Timeout */
		WCD_MBHC_REG_READ(WCD_MBHC_ADC_TIMEOUT, adc_timeout);
		if (adc_timeout)
			continue;

		/* Read ADC complete bit */
		WCD_MBHC_REG_READ(WCD_MBHC_ADC_COMPLETE, adc_complete);
		if (!adc_complete)
			continue;

		/* Read ADC result */
		WCD_MBHC_REG_READ(WCD_MBHC_ADC_RESULT, adc_result);

		pr_debug("%s: ADC result: 0x%x\n", __func__, adc_result);
		/* Get voltage from ADC result */
		output_mv = wcd_get_voltage_from_adc(adc_result,
						wcd_mbhc_get_micbias(mbhc));
		break;
	}

	/* Restore ADC Enable */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, adc_en);

	if (retry <= 0) {
		pr_err("%s: adc complete: %d, adc timeout: %d\n",
			__func__, adc_complete, adc_timeout);
		ret = -EINVAL;
	} else {
		pr_debug("%s: adc complete: %d, adc timeout: %d output_mV: %d\n",
			__func__, adc_complete, adc_timeout, output_mv);
		ret = output_mv;
	}

	pr_debug("%s: leave\n", __func__);

	return ret;
}

static bool wcd_mbhc_adc_detect_anc_plug_type(struct wcd_mbhc *mbhc)
{
	bool anc_mic_found = false;
	u16 fsm_en = 0;
	u8 det = 0;
	unsigned long retry = 0;
	int valid_plug_cnt = 0, invalid_plug_cnt = 0;
	int ret = 0;
	u8 elect_ctl = 0;
	u8 adc_mode = 0;
	u8 vref = 0;
	int vref_mv[] = {1650, 1500, 1600, 1700};

	if (mbhc->mbhc_cfg->anc_micbias < MIC_BIAS_1 ||
	    mbhc->mbhc_cfg->anc_micbias > MIC_BIAS_4)
		return false;

	if (!mbhc->mbhc_cb->mbhc_micbias_control)
		return false;

	/* Disable Detection done for ADC operation */
	WCD_MBHC_REG_READ(WCD_MBHC_DETECTION_DONE, det);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_DETECTION_DONE, 0);

	/* Mask ADC COMPLETE interrupt */
	wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS, false);

	WCD_MBHC_REG_READ(WCD_MBHC_FSM_EN, fsm_en);
	mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec,
					    mbhc->mbhc_cfg->anc_micbias,
					    MICB_ENABLE);

	/* Read legacy electircal detection and disable */
	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_SCHMT_ISRC, elect_ctl);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0x00);

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ANC_DET_EN, 1);
	WCD_MBHC_REG_READ(WCD_MBHC_ADC_MODE, adc_mode);

	/*
	 * wait for button debounce time 20ms. If 4-pole plug is inserted
	 * into 5-pole jack, then there will be a button press interrupt
	 * during anc plug detection. In that case though Hs_comp_res is 0,
	 * it should not be declared as ANC plug type
	 */
	usleep_range(20000, 20100);

	/*
	 * After enabling FSM, to handle slow insertion scenarios,
	 * check IN3 voltage is below the Vref
	 */
	WCD_MBHC_REG_READ(WCD_MBHC_HS_VREF, vref);

	do {
		if (wcd_swch_level_remove(mbhc)) {
			pr_debug("%s: Switch level is low\n", __func__);
			goto done;
		}
		pr_debug("%s: Retry attempt %lu\n", __func__, retry + 1);
		ret = wcd_measure_adc_once(mbhc, MUX_CTL_IN3P);
		/* TODO - check the logic */
		if (ret && (ret < vref_mv[vref]))
			valid_plug_cnt++;
		else
			invalid_plug_cnt++;
		retry++;
	} while (retry < ANC_DETECT_RETRY_CNT);

	pr_debug("%s: valid: %d, invalid: %d\n", __func__, valid_plug_cnt,
		 invalid_plug_cnt);

	/* decision logic */
	if (valid_plug_cnt > invalid_plug_cnt)
		anc_mic_found = true;
done:
	/* Restore ADC mode */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_MODE, adc_mode);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ANC_DET_EN, 0);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
	/* Set the MUX selection to AUTO */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MUX_CTL, MUX_CTL_AUTO);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, fsm_en);
	/* Restore detection done */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_DETECTION_DONE, det);

	/* Restore electrical detection */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, elect_ctl);

	mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec,
					    mbhc->mbhc_cfg->anc_micbias,
					    MICB_DISABLE);
	pr_debug("%s: anc mic %sfound\n", __func__,
		 anc_mic_found ? "" : "not ");

	return anc_mic_found;
}

/* To determine if cross connection occurred */
static int wcd_check_cross_conn(struct wcd_mbhc *mbhc)
{
	enum wcd_mbhc_plug_type plug_type = MBHC_PLUG_TYPE_NONE;
	int hphl_adc_res = 0, hphr_adc_res = 0;
	u8 fsm_en = 0;
	int ret = 0;
	u8 adc_mode = 0;
	u8 elect_ctl = 0;
	u8 adc_en = 0;

	pr_debug("%s: enter\n", __func__);
	/* Check for button press and plug detection */
	if (wcd_swch_level_remove(mbhc)) {
		pr_debug("%s: Switch level is low\n", __func__);
		return -EINVAL;
	}

	/* If PA is enabled, dont check for cross-connection */
	if (mbhc->mbhc_cb->hph_pa_on_status)
		if (mbhc->mbhc_cb->hph_pa_on_status(mbhc->codec))
			return -EINVAL;

	/* Read legacy electircal detection and disable */
	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_SCHMT_ISRC, elect_ctl);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0x00);

	/* Read and set ADC to single measurement */
	WCD_MBHC_REG_READ(WCD_MBHC_ADC_MODE, adc_mode);
	/* Read ADC Enable bit to restore after adc measurement */
	WCD_MBHC_REG_READ(WCD_MBHC_ADC_EN, adc_en);
	/* Read FSM status */
	WCD_MBHC_REG_READ(WCD_MBHC_FSM_EN, fsm_en);

	/* Get adc result for HPH L */
	hphl_adc_res = wcd_measure_adc_once(mbhc, MUX_CTL_HPH_L);
	if (hphl_adc_res < 0) {
		pr_err("%s: hphl_adc_res adc measurement failed\n", __func__);
		ret = hphl_adc_res;
		goto done;
	}

	/* Get adc result for HPH R in mV */
	hphr_adc_res = wcd_measure_adc_once(mbhc, MUX_CTL_HPH_R);
	if (hphr_adc_res < 0) {
		pr_err("%s: hphr_adc_res adc measurement failed\n", __func__);
		ret = hphr_adc_res;
		goto done;
	}

	if (hphl_adc_res > 100 && hphr_adc_res > 100) {
		plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
		pr_debug("%s: Cross connection identified\n", __func__);
	} else {
		pr_debug("%s: No Cross connection found\n", __func__);
	}

done:
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
	/* Set the MUX selection to Auto */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MUX_CTL, MUX_CTL_AUTO);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);

	/* Restore ADC Enable */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, adc_en);

	/* Restore ADC mode */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_MODE, adc_mode);

	/* Restore FSM state */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, fsm_en);

	/* Restore electrical detection */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, elect_ctl);

	pr_debug("%s: leave, plug type: %d\n", __func__,  plug_type);

	return (plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP) ? true : false;
}

static bool wcd_mbhc_adc_check_for_spl_headset(struct wcd_mbhc *mbhc,
					   int *spl_hs_cnt)
{
	bool spl_hs = false;
	int output_mv = 0;
	int adc_threshold = 0, adc_hph_threshold = 0;

	pr_debug("%s: enter\n", __func__);
	if (!mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic)
		goto exit;

	/* Bump up MB2 to 2.7V */
	mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
				mbhc->mbhc_cfg->mbhc_micbias, true);
	usleep_range(10000, 10100);

	/*
	 * Use ADC single mode to minimize the chance of missing out
	 * btn press/relesae for HEADSET type during correct work.
	 */
	output_mv = wcd_measure_adc_once(mbhc, MUX_CTL_IN2P);
	if (mbhc->hs_thr)
		adc_threshold = mbhc->hs_thr;
	else
		adc_threshold = ((WCD_MBHC_ADC_HS_THRESHOLD_MV *
			  wcd_mbhc_get_micbias(mbhc))/WCD_MBHC_ADC_MICBIAS_MV);

	if (mbhc->hph_thr)
		adc_hph_threshold = mbhc->hph_thr;
	else
		adc_hph_threshold = ((WCD_MBHC_ADC_HPH_THRESHOLD_MV *
				wcd_mbhc_get_micbias(mbhc))/
				WCD_MBHC_ADC_MICBIAS_MV);

	if (output_mv > adc_threshold || output_mv < adc_hph_threshold) {
		spl_hs = false;
	} else {
		spl_hs = true;
		if (spl_hs_cnt)
			*spl_hs_cnt += 1;
	}

	/* MB2 back to 1.8v if the type is not special headset */
	if (spl_hs_cnt && (*spl_hs_cnt != WCD_MBHC_SPL_HS_CNT)) {
		mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
				mbhc->mbhc_cfg->mbhc_micbias, false);
		/* Add 10ms delay for micbias to settle */
		usleep_range(10000, 10100);
	}

	if (spl_hs)
		pr_debug("%s: Detected special HS (%d)\n", __func__, spl_hs);

exit:
	pr_debug("%s: leave\n", __func__);
	return spl_hs;
}

static bool wcd_is_special_headset(struct wcd_mbhc *mbhc)
{
	int delay = 0;
	bool ret = false;
	bool is_spl_hs = false;
	int output_mv = 0;
	int adc_threshold = 0;

	/*
	 * Increase micbias to 2.7V to detect headsets with
	 * threshold on microphone
	 */
	if (mbhc->mbhc_cb->mbhc_micbias_control &&
	    !mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic) {
		pr_debug("%s: callback fn micb_ctrl_thr_mic not defined\n",
			 __func__);
		return false;
	} else if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic) {
		ret = mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
							MIC_BIAS_2, true);
		if (ret) {
			pr_err("%s: mbhc_micb_ctrl_thr_mic failed, ret: %d\n",
				__func__, ret);
			return false;
		}
	}
	if (mbhc->hs_thr)
		adc_threshold = mbhc->hs_thr;
	else
		adc_threshold = ((WCD_MBHC_ADC_HS_THRESHOLD_MV *
			  wcd_mbhc_get_micbias(mbhc)) /
			  WCD_MBHC_ADC_MICBIAS_MV);

	while (!is_spl_hs) {
		if (mbhc->hs_detect_work_stop) {
			pr_debug("%s: stop requested: %d\n", __func__,
					mbhc->hs_detect_work_stop);
			break;
		}
		delay += 50;
		/* Wait for 50ms for FSM to update result */
		msleep(50);
		output_mv = wcd_measure_adc_once(mbhc, MUX_CTL_IN2P);
		if (output_mv <= adc_threshold) {
			pr_debug("%s: Special headset detected in %d msecs\n",
					__func__, delay);
			is_spl_hs = true;
		}

		if (delay == SPECIAL_HS_DETECT_TIME_MS) {
			pr_debug("%s: Spl headset not found in 2 sec\n",
				 __func__);
			break;
		}
	}
	if (is_spl_hs) {
		pr_debug("%s: Headset with threshold found\n",  __func__);
		mbhc->micbias_enable = true;
		ret = true;
	}
	if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic &&
	    !mbhc->micbias_enable)
		mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec, MIC_BIAS_2,
						      false);
	pr_debug("%s: leave, micb_enable: %d\n", __func__,
		  mbhc->micbias_enable);

	return ret;
}

static void wcd_mbhc_adc_update_fsm_source(struct wcd_mbhc *mbhc,
				       enum wcd_mbhc_plug_type plug_type)
{
	bool micbias2;

	micbias2 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
							MIC_BIAS_2);
	switch (plug_type) {
	case MBHC_PLUG_TYPE_HEADPHONE:
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 3);
		break;
	case MBHC_PLUG_TYPE_HEADSET:
	case MBHC_PLUG_TYPE_ANC_HEADPHONE:
		if (!mbhc->is_hs_recording && !micbias2)
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 3);
		break;
	default:
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
		break;

	};
}

/* should be called under interrupt context that hold suspend */
static void wcd_schedule_hs_detect_plug(struct wcd_mbhc *mbhc,
					    struct work_struct *work)
{
	pr_debug("%s: scheduling correct_swch_plug\n", __func__);
	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);
	mbhc->hs_detect_work_stop = false;
	mbhc->mbhc_cb->lock_sleep(mbhc, true);
	schedule_work(work);
}

/* called under codec_resource_lock acquisition */
static void wcd_cancel_hs_detect_plug(struct wcd_mbhc *mbhc,
					 struct work_struct *work)
{
	pr_debug("%s: Canceling correct_plug_swch\n", __func__);
	mbhc->hs_detect_work_stop = true;
	WCD_MBHC_RSC_UNLOCK(mbhc);
	if (cancel_work_sync(work)) {
		pr_debug("%s: correct_plug_swch is canceled\n",
			 __func__);
		mbhc->mbhc_cb->lock_sleep(mbhc, false);
	}
	WCD_MBHC_RSC_LOCK(mbhc);
}

/* called under codec_resource_lock acquisition */
static void wcd_mbhc_adc_detect_plug_type(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);
	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	if (mbhc->mbhc_cb->hph_pull_down_ctrl)
		mbhc->mbhc_cb->hph_pull_down_ctrl(codec, false);

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_DETECTION_DONE, 0);

	if (mbhc->mbhc_cb->mbhc_micbias_control) {
		mbhc->mbhc_cb->mbhc_micbias_control(codec, MIC_BIAS_2,
						    MICB_ENABLE);
	} else {
		pr_err("%s: Mic Bias is not enabled\n", __func__);
		return;
	}

	/* Re-initialize button press completion object */
	reinit_completion(&mbhc->btn_press_compl);
	wcd_schedule_hs_detect_plug(mbhc, &mbhc->correct_plug_swch);
	pr_debug("%s: leave\n", __func__);
}

static void wcd_micbias_disable(struct wcd_mbhc *mbhc)
{
	if (mbhc->micbias_enable) {
		mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(
			mbhc->codec, MIC_BIAS_2, false);
		if (mbhc->mbhc_cb->set_micbias_value)
			mbhc->mbhc_cb->set_micbias_value(
					mbhc->codec);
		mbhc->micbias_enable = false;
	}
}

static int wcd_mbhc_get_plug_from_adc(struct wcd_mbhc *mbhc, int adc_result)

{
	enum wcd_mbhc_plug_type plug_type = MBHC_PLUG_TYPE_INVALID;
	u32 hph_thr = 0, hs_thr = 0;

	if (mbhc->hs_thr)
		hs_thr = mbhc->hs_thr;
	else
		hs_thr = WCD_MBHC_ADC_HS_THRESHOLD_MV;

	if (mbhc->hph_thr)
		hph_thr = mbhc->hph_thr;
	else
		hph_thr = WCD_MBHC_ADC_HPH_THRESHOLD_MV;

	if (adc_result < hph_thr)
		plug_type = MBHC_PLUG_TYPE_HEADPHONE;
	else if (adc_result > hs_thr)
		plug_type = MBHC_PLUG_TYPE_HIGH_HPH;
	else
		plug_type = MBHC_PLUG_TYPE_HEADSET;
	pr_debug("%s: plug type is %d found\n", __func__, plug_type);

	return plug_type;
}

static void wcd_correct_swch_plug(struct work_struct *work)
{
	struct wcd_mbhc *mbhc;
	struct snd_soc_codec *codec;
	enum wcd_mbhc_plug_type plug_type = MBHC_PLUG_TYPE_INVALID;
	unsigned long timeout;
	bool wrk_complete = false;
	int pt_gnd_mic_swap_cnt = 0;
	int no_gnd_mic_swap_cnt = 0;
	bool is_pa_on = false, spl_hs = false, spl_hs_reported = false;
	int ret = 0;
	int spl_hs_count = 0;
	int output_mv = 0;
	int cross_conn;
	int try = 0;

	pr_debug("%s: enter\n", __func__);

	mbhc = container_of(work, struct wcd_mbhc, correct_plug_swch);
	codec = mbhc->codec;

	WCD_MBHC_RSC_LOCK(mbhc);
	/* Mask ADC COMPLETE interrupt */
	wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS, false);
	WCD_MBHC_RSC_UNLOCK(mbhc);

	/* Check for cross connection */
	do {
		cross_conn = wcd_check_cross_conn(mbhc);
		try++;
	} while (try < mbhc->swap_thr);

	if (cross_conn > 0) {
		plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
		pr_debug("%s: cross connection found, Plug type %d\n",
			 __func__, plug_type);
		goto correct_plug_type;
	}
	/* Find plug type */
	output_mv = wcd_measure_adc_continuous(mbhc);
	plug_type = wcd_mbhc_get_plug_from_adc(mbhc, output_mv);

	/*
	 * Report plug type if it is either headset or headphone
	 * else start the 3 sec loop
	 */
	if ((plug_type == MBHC_PLUG_TYPE_HEADSET ||
	     plug_type == MBHC_PLUG_TYPE_HEADPHONE) &&
	    (!wcd_swch_level_remove(mbhc))) {
		WCD_MBHC_RSC_LOCK(mbhc);
		wcd_mbhc_find_plug_and_report(mbhc, plug_type);
		WCD_MBHC_RSC_UNLOCK(mbhc);
	}

	/*
	 * Set DETECTION_DONE bit for HEADSET and ANC_HEADPHONE,
	 * so that btn press/release interrupt can be generated.
	 */
	if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET ||
		mbhc->current_plug == MBHC_PLUG_TYPE_ANC_HEADPHONE) {
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_MODE, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_DETECTION_DONE, 1);
	}

correct_plug_type:
	timeout = jiffies + msecs_to_jiffies(HS_DETECT_PLUG_TIME_MS);
	while (!time_after(jiffies, timeout)) {
		if (mbhc->hs_detect_work_stop) {
			pr_debug("%s: stop requested: %d\n", __func__,
					mbhc->hs_detect_work_stop);
			wcd_micbias_disable(mbhc);
			goto exit;
		}

		/* allow sometime and re-check stop requested again */
		msleep(20);
		if (mbhc->hs_detect_work_stop) {
			pr_debug("%s: stop requested: %d\n", __func__,
					mbhc->hs_detect_work_stop);
			wcd_micbias_disable(mbhc);
			goto exit;
		}

		msleep(180);
		/*
		 * Use ADC single mode to minimize the chance of missing out
		 * btn press/release for HEADSET type during correct work.
		 */
		output_mv = wcd_measure_adc_once(mbhc, MUX_CTL_IN2P);

		/*
		 * instead of hogging system by contineous polling, wait for
		 * sometime and re-check stop request again.
		 */
		plug_type = wcd_mbhc_get_plug_from_adc(mbhc, output_mv);

		if ((output_mv > WCD_MBHC_ADC_HS_THRESHOLD_MV) &&
		    (spl_hs_count < WCD_MBHC_SPL_HS_CNT)) {
			spl_hs = wcd_mbhc_adc_check_for_spl_headset(mbhc,
								&spl_hs_count);

			if (spl_hs_count == WCD_MBHC_SPL_HS_CNT) {
				output_mv = WCD_MBHC_ADC_HS_THRESHOLD_MV;
				spl_hs = true;
				mbhc->micbias_enable = true;
			}
		}

		if (mbhc->mbhc_cb->hph_pa_on_status)
			is_pa_on = mbhc->mbhc_cb->hph_pa_on_status(mbhc->codec);

		if ((output_mv <= WCD_MBHC_ADC_HS_THRESHOLD_MV) &&
		    (!is_pa_on)) {
			/* Check for cross connection*/
			ret = wcd_check_cross_conn(mbhc);
			if (ret < 0)
				continue;
			else if (ret > 0) {
				pt_gnd_mic_swap_cnt++;
				no_gnd_mic_swap_cnt = 0;
				if (pt_gnd_mic_swap_cnt <
						mbhc->swap_thr) {
					continue;
				} else if (pt_gnd_mic_swap_cnt >
					   mbhc->swap_thr) {
					/*
					 * This is due to GND/MIC switch didn't
					 * work,  Report unsupported plug.
					 */
					pr_debug("%s: switch did not work\n",
						 __func__);
					plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
					goto report;
				} else {
					plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
				}
			} else {
				no_gnd_mic_swap_cnt++;
				pt_gnd_mic_swap_cnt = 0;
				plug_type = wcd_mbhc_get_plug_from_adc(
						mbhc, output_mv);
				if ((no_gnd_mic_swap_cnt <
				    mbhc->swap_thr) &&
				    (spl_hs_count != WCD_MBHC_SPL_HS_CNT)) {
					continue;
				} else {
					no_gnd_mic_swap_cnt = 0;
				}
			}
			if ((pt_gnd_mic_swap_cnt == mbhc->swap_thr) &&
				(plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP)) {
				/*
				 * if switch is toggled, check again,
				 * otherwise report unsupported plug
				 */
				if (mbhc->mbhc_cfg->swap_gnd_mic &&
					mbhc->mbhc_cfg->swap_gnd_mic(codec,
					true)) {
					pr_debug("%s: US_EU gpio present,flip switch\n"
						, __func__);
					continue;
				}
			}
		}

		if (output_mv > WCD_MBHC_ADC_HS_THRESHOLD_MV) {
			pr_debug("%s: cable is extension cable\n", __func__);
			plug_type = MBHC_PLUG_TYPE_HIGH_HPH;
			wrk_complete = true;
		} else {
			pr_debug("%s: cable might be headset: %d\n", __func__,
				 plug_type);
			if (plug_type != MBHC_PLUG_TYPE_GND_MIC_SWAP) {
				plug_type = wcd_mbhc_get_plug_from_adc(
						mbhc, output_mv);
				if (!spl_hs_reported &&
				    spl_hs_count == WCD_MBHC_SPL_HS_CNT) {
					spl_hs_reported = true;
					WCD_MBHC_RSC_LOCK(mbhc);
					wcd_mbhc_find_plug_and_report(mbhc,
								    plug_type);
					WCD_MBHC_RSC_UNLOCK(mbhc);
					continue;
				} else if (spl_hs_reported)
					continue;
				/*
				 * Report headset only if not already reported
				 * and if there is not button press without
				 * release
				 */
				if ((mbhc->current_plug !=
				      MBHC_PLUG_TYPE_HEADSET) &&
				     (mbhc->current_plug !=
				     MBHC_PLUG_TYPE_ANC_HEADPHONE) &&
				    !wcd_swch_level_remove(mbhc)) {
					pr_debug("%s: cable is %s headset\n",
						 __func__,
						((spl_hs_count ==
							WCD_MBHC_SPL_HS_CNT) ?
							"special ":""));
					goto report;
				}
			}
			wrk_complete = false;
		}
	}
	if (!wrk_complete) {
		/*
		 * If plug_tye is headset, we might have already reported either
		 * in detect_plug-type or in above while loop, no need to report
		 * again
		 */
		if ((plug_type == MBHC_PLUG_TYPE_HEADSET) ||
		    (plug_type == MBHC_PLUG_TYPE_ANC_HEADPHONE)) {
			pr_debug("%s: plug_type:0x%x already reported\n",
				 __func__, mbhc->current_plug);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_MODE, 0);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, 0);
			goto enable_supply;
		}
	}
	if (plug_type == MBHC_PLUG_TYPE_HIGH_HPH) {
		if (wcd_is_special_headset(mbhc)) {
			pr_debug("%s: Special headset found %d\n",
					__func__, plug_type);
			plug_type = MBHC_PLUG_TYPE_HEADSET;
		} else {
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_ISRC_EN, 1);
		}
	}

report:
	if (wcd_swch_level_remove(mbhc)) {
		pr_debug("%s: Switch level is low\n", __func__);
		goto exit;
	}

	pr_debug("%s: Valid plug found, plug type %d wrk_cmpt %d btn_intr %d\n",
			__func__, plug_type, wrk_complete,
			mbhc->btn_press_intr);

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_MODE, 0);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, 0);

	WCD_MBHC_RSC_LOCK(mbhc);
	wcd_mbhc_find_plug_and_report(mbhc, plug_type);
	WCD_MBHC_RSC_UNLOCK(mbhc);
enable_supply:
	/*
	 * Set DETECTION_DONE bit for HEADSET and ANC_HEADPHONE,
	 * so that btn press/release interrupt can be generated.
	 * For other plug type, clear the bit.
	 */
	if (plug_type == MBHC_PLUG_TYPE_HEADSET ||
	    plug_type == MBHC_PLUG_TYPE_ANC_HEADPHONE)
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_DETECTION_DONE, 1);
	else
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_DETECTION_DONE, 0);

	if (mbhc->mbhc_cb->mbhc_micbias_control)
		wcd_mbhc_adc_update_fsm_source(mbhc, plug_type);
exit:
	if (mbhc->mbhc_cb->mbhc_micbias_control &&
	    !mbhc->micbias_enable)
		mbhc->mbhc_cb->mbhc_micbias_control(codec, MIC_BIAS_2,
						    MICB_DISABLE);

	/*
	 * If plug type is corrected from special headset to headphone,
	 * clear the micbias enable flag, set micbias back to 1.8V and
	 * disable micbias.
	 */
	if (plug_type == MBHC_PLUG_TYPE_HEADPHONE &&
	    mbhc->micbias_enable) {
		if (mbhc->mbhc_cb->mbhc_micbias_control)
			mbhc->mbhc_cb->mbhc_micbias_control(
					codec, MIC_BIAS_2,
					MICB_DISABLE);
		if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic)
			mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(
					codec,
					MIC_BIAS_2, false);
		if (mbhc->mbhc_cb->set_micbias_value) {
			mbhc->mbhc_cb->set_micbias_value(codec);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MICB_CTRL, 0);
		}
		mbhc->micbias_enable = false;
	}

	if (mbhc->mbhc_cfg->detect_extn_cable &&
	    ((plug_type == MBHC_PLUG_TYPE_HEADPHONE) ||
	     (plug_type == MBHC_PLUG_TYPE_HEADSET)) &&
	    !mbhc->hs_detect_work_stop) {
		WCD_MBHC_RSC_LOCK(mbhc);
		wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_REM, true);
		WCD_MBHC_RSC_UNLOCK(mbhc);
	}

	/*
	 * Enable ADC COMPLETE interrupt for HEADPHONE.
	 * Btn release may happen after the correct work, ADC COMPLETE
	 * interrupt needs to be captured to correct plug type.
	 */
	if (plug_type == MBHC_PLUG_TYPE_HEADPHONE) {
		WCD_MBHC_RSC_LOCK(mbhc);
		wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS,
				     true);
		WCD_MBHC_RSC_UNLOCK(mbhc);
	}

	if (mbhc->mbhc_cb->hph_pull_down_ctrl)
		mbhc->mbhc_cb->hph_pull_down_ctrl(codec, true);

	mbhc->mbhc_cb->lock_sleep(mbhc, false);
	pr_debug("%s: leave\n", __func__);
}

static irqreturn_t wcd_mbhc_adc_hs_rem_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	unsigned long timeout;
	int adc_threshold, output_mv, retry = 0;
	bool hphpa_on = false;
	u8  moisture_status = 0;

	pr_debug("%s: enter\n", __func__);
	WCD_MBHC_RSC_LOCK(mbhc);

	timeout = jiffies +
		  msecs_to_jiffies(WCD_FAKE_REMOVAL_MIN_PERIOD_MS);
	adc_threshold = ((WCD_MBHC_ADC_HS_THRESHOLD_MV *
			  wcd_mbhc_get_micbias(mbhc)) /
			  WCD_MBHC_ADC_MICBIAS_MV);
	do {
		retry++;
		/*
		 * read output_mv every 10ms to look for
		 * any change in IN2_P
		 */
		usleep_range(10000, 10100);
		output_mv = wcd_measure_adc_once(mbhc, MUX_CTL_IN2P);

		pr_debug("%s: Check for fake removal: output_mv %d\n",
			 __func__, output_mv);
		if ((output_mv <= adc_threshold) &&
		    retry > FAKE_REM_RETRY_ATTEMPTS) {
			pr_debug("%s: headset is NOT actually removed\n",
				 __func__);
			goto exit;
		}
	} while (!time_after(jiffies, timeout));

	if (wcd_swch_level_remove(mbhc)) {
		pr_debug("%s: Switch level is low ", __func__);
		goto exit;
	}

	if (mbhc->mbhc_cfg->moisture_en) {
		if (mbhc->mbhc_cb->hph_pa_on_status)
			if (mbhc->mbhc_cb->hph_pa_on_status(mbhc->codec)) {
				hphpa_on = true;
				WCD_MBHC_REG_UPDATE_BITS(
					WCD_MBHC_HPHL_PA_EN, 0);
				WCD_MBHC_REG_UPDATE_BITS(
					WCD_MBHC_HPH_PA_EN, 0);
			}
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPHR_GND, 1);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPHL_GND, 1);
		/* wait for 50ms to get moisture status */
		usleep_range(50000, 50100);

		WCD_MBHC_REG_READ(WCD_MBHC_MOISTURE_STATUS, moisture_status);
	}

	if (mbhc->mbhc_cfg->moisture_en && !moisture_status) {
		pr_debug("%s: moisture present in jack\n", __func__);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_L_DET_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MECH_DETECTION_TYPE, 1);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_L_DET_EN, 1);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
		mbhc->btn_press_intr = false;
		mbhc->is_btn_press = false;
		if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET)
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_HEADSET);
		else if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADPHONE)
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_HEADPHONE);
		else if (mbhc->current_plug == MBHC_PLUG_TYPE_GND_MIC_SWAP)
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_UNSUPPORTED);
		else if (mbhc->current_plug == MBHC_PLUG_TYPE_HIGH_HPH)
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_LINEOUT);
	} else {
		/*
		 * ADC COMPLETE and ELEC_REM interrupts are both enabled for
		 * HEADPHONE, need to reject the ADC COMPLETE interrupt which
		 * follows ELEC_REM one when HEADPHONE is removed.
		 */
		if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADPHONE)
			mbhc->extn_cable_hph_rem = true;
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_DETECTION_DONE, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_MODE, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ADC_EN, 0);
		wcd_mbhc_elec_hs_report_unplug(mbhc);

		if (hphpa_on) {
			hphpa_on = false;
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPHL_PA_EN, 1);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPH_PA_EN, 1);
		}
	}
exit:
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t wcd_mbhc_adc_hs_ins_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;

	pr_debug("%s: enter\n", __func__);

	/*
	 * ADC COMPLETE and ELEC_REM interrupts are both enabled for HEADPHONE,
	 * need to reject the ADC COMPLETE interrupt which follows ELEC_REM one
	 * when HEADPHONE is removed.
	 */
	if (mbhc->extn_cable_hph_rem == true) {
		mbhc->extn_cable_hph_rem = false;
		pr_debug("%s: leave\n", __func__);
		return IRQ_HANDLED;
	}

	WCD_MBHC_RSC_LOCK(mbhc);
	/*
	 * If current plug is headphone then there is no chance to
	 * get ADC complete interrupt, so connected cable should be
	 * headset not headphone.
	 */
	if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADPHONE) {
		wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS, false);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_DETECTION_DONE, 1);
		wcd_mbhc_find_plug_and_report(mbhc, MBHC_PLUG_TYPE_HEADSET);
		WCD_MBHC_RSC_UNLOCK(mbhc);
		return IRQ_HANDLED;
	}

	if (!mbhc->mbhc_cfg->detect_extn_cable) {
		pr_debug("%s: Returning as Extension cable feature not enabled\n",
			__func__);
		WCD_MBHC_RSC_UNLOCK(mbhc);
		return IRQ_HANDLED;
	}

	pr_debug("%s: Disable electrical headset insertion interrupt\n",
		 __func__);
	wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS, false);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_ISRC_EN, 0);
	mbhc->is_extn_cable = true;
	mbhc->btn_press_intr = false;
	wcd_mbhc_adc_detect_plug_type(mbhc);
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;
}

static struct wcd_mbhc_fn mbhc_fn = {
	.wcd_mbhc_hs_ins_irq = wcd_mbhc_adc_hs_ins_irq,
	.wcd_mbhc_hs_rem_irq = wcd_mbhc_adc_hs_rem_irq,
	.wcd_mbhc_detect_plug_type = wcd_mbhc_adc_detect_plug_type,
	.wcd_mbhc_detect_anc_plug_type = wcd_mbhc_adc_detect_anc_plug_type,
	.wcd_cancel_hs_detect_plug = wcd_cancel_hs_detect_plug,
};

/* Function: wcd_mbhc_adc_init
 * @mbhc: MBHC function pointer
 * Description: Initialize MBHC ADC related function pointers to MBHC structure
 */
void wcd_mbhc_adc_init(struct wcd_mbhc *mbhc)
{
	if (!mbhc) {
		pr_err("%s: mbhc is NULL\n", __func__);
		return;
	}
	mbhc->mbhc_fn = &mbhc_fn;
	INIT_WORK(&mbhc->correct_plug_swch, wcd_correct_swch_plug);
}
EXPORT_SYMBOL(wcd_mbhc_adc_init);
