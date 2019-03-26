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
#include "wcd-mbhc-legacy.h"
#include "wcd-mbhc-v2.h"

static int det_extn_cable_en;
module_param(det_extn_cable_en, int, 0664);
MODULE_PARM_DESC(det_extn_cable_en, "enable/disable extn cable detect");

static bool wcd_mbhc_detect_anc_plug_type(struct wcd_mbhc *mbhc)
{
	bool anc_mic_found = false;
	u16 val, hs_comp_res, btn_status = 0;
	unsigned long retry = 0;
	int valid_plug_cnt = 0, invalid_plug_cnt = 0;
	int btn_status_cnt = 0;
	bool is_check_btn_press = false;


	if (mbhc->mbhc_cfg->anc_micbias < MIC_BIAS_1 ||
	    mbhc->mbhc_cfg->anc_micbias > MIC_BIAS_4)
		return false;

	if (!mbhc->mbhc_cb->mbhc_micbias_control)
		return false;

	WCD_MBHC_REG_READ(WCD_MBHC_FSM_EN, val);

	if (val)
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);

	mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec,
					    mbhc->mbhc_cfg->anc_micbias,
					    MICB_ENABLE);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MUX_CTL, 0x2);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ANC_DET_EN, 1);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
	/*
	 * wait for button debounce time 20ms. If 4-pole plug is inserted
	 * into 5-pole jack, then there will be a button press interrupt
	 * during anc plug detection. In that case though Hs_comp_res is 0,
	 * it should not be declared as ANC plug type
	 */
	usleep_range(20000, 20100);

	/*
	 * After enabling FSM, to handle slow insertion scenarios,
	 * check hs_comp_result for few times to see if the IN3 voltage
	 * is below the Vref
	 */
	do {
		if (wcd_swch_level_remove(mbhc)) {
			pr_debug("%s: Switch level is low\n", __func__);
			goto exit;
		}
		pr_debug("%s: Retry attempt %lu\n", __func__, retry + 1);
		WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);

		if (!hs_comp_res) {
			valid_plug_cnt++;
			is_check_btn_press = true;
		} else
			invalid_plug_cnt++;
		/* Wait 1ms before taking another reading */
		usleep_range(1000, 1100);

		WCD_MBHC_REG_READ(WCD_MBHC_FSM_STATUS, btn_status);
		if (btn_status)
			btn_status_cnt++;

		retry++;
	} while (retry < ANC_DETECT_RETRY_CNT);

	pr_debug("%s: valid: %d, invalid: %d, btn_status_cnt: %d\n",
		 __func__, valid_plug_cnt, invalid_plug_cnt, btn_status_cnt);

	/* decision logic */
	if ((valid_plug_cnt > invalid_plug_cnt) && is_check_btn_press &&
	    (btn_status_cnt == 0))
		anc_mic_found = true;
exit:
	if (!val)
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ANC_DET_EN, 0);

	mbhc->mbhc_cb->mbhc_micbias_control(mbhc->codec,
					    mbhc->mbhc_cfg->anc_micbias,
					    MICB_DISABLE);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_MUX_CTL, 0x0);
	pr_debug("%s: anc mic %sfound\n", __func__,
		 anc_mic_found ? "" : "not ");
	return anc_mic_found;
}

/* To determine if cross connection occurred */
static int wcd_check_cross_conn(struct wcd_mbhc *mbhc)
{
	u16 swap_res = 0;
	enum wcd_mbhc_plug_type plug_type = MBHC_PLUG_TYPE_NONE;
	s16 reg1 = 0;
	bool hphl_sch_res = 0, hphr_sch_res = 0;

	if (wcd_swch_level_remove(mbhc)) {
		pr_debug("%s: Switch level is low\n", __func__);
		return -EINVAL;
	}

	/* If PA is enabled, dont check for cross-connection */
	if (mbhc->mbhc_cb->hph_pa_on_status)
		if (mbhc->mbhc_cb->hph_pa_on_status(mbhc->codec))
			return false;

	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_SCHMT_ISRC, reg1);
	/*
	 * Check if there is any cross connection,
	 * Micbias and schmitt trigger (HPHL-HPHR)
	 * needs to be enabled. For some codecs like wcd9335,
	 * pull-up will already be enabled when this function
	 * is called for cross-connection identification. No
	 * need to enable micbias in that case.
	 */
	wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 2);

	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_RESULT, swap_res);
	pr_debug("%s: swap_res%x\n", __func__, swap_res);

	/*
	 * Read reg hphl and hphr schmitt result with cross connection
	 * bit. These bits will both be "0" in case of cross connection
	 * otherwise, they stay at 1
	 */
	WCD_MBHC_REG_READ(WCD_MBHC_HPHL_SCHMT_RESULT, hphl_sch_res);
	WCD_MBHC_REG_READ(WCD_MBHC_HPHR_SCHMT_RESULT, hphr_sch_res);
	if (!(hphl_sch_res || hphr_sch_res)) {
		plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
		pr_debug("%s: Cross connection identified\n", __func__);
	} else {
		pr_debug("%s: No Cross connection found\n", __func__);
	}

	/* Disable schmitt trigger and restore micbias */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, reg1);
	pr_debug("%s: leave, plug type: %d\n", __func__,  plug_type);

	return (plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP) ? true : false;
}

static bool wcd_is_special_headset(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;
	int delay = 0, rc;
	bool ret = false;
	u16 hs_comp_res;
	bool is_spl_hs = false;

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
		rc = mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(codec,
							MIC_BIAS_2, true);
		if (rc) {
			pr_err("%s: Micbias control for thr mic failed, rc: %d\n",
				__func__, rc);
			return false;
		}
	}

	wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);

	pr_debug("%s: special headset, start register writes\n", __func__);

	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);
	while (!is_spl_hs)  {
		if (mbhc->hs_detect_work_stop) {
			pr_debug("%s: stop requested: %d\n", __func__,
					mbhc->hs_detect_work_stop);
			break;
		}
		delay = delay + 50;
		if (mbhc->mbhc_cb->mbhc_common_micb_ctrl) {
			mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
					MBHC_COMMON_MICB_PRECHARGE,
					true);
			mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
					MBHC_COMMON_MICB_SET_VAL,
					true);
		}
		/* Wait for 50msec for MICBIAS to settle down */
		msleep(50);
		if (mbhc->mbhc_cb->set_auto_zeroing)
			mbhc->mbhc_cb->set_auto_zeroing(codec, true);
		/* Wait for 50msec for FSM to update result values */
		msleep(50);
		WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);
		if (!(hs_comp_res)) {
			pr_debug("%s: Special headset detected in %d msecs\n",
					__func__, (delay * 2));
			is_spl_hs = true;
		}
		if (delay == SPECIAL_HS_DETECT_TIME_MS) {
			pr_debug("%s: Spl headset didn't get detect in 4 sec\n",
					__func__);
			break;
		}
	}
	if (is_spl_hs) {
		pr_debug("%s: Headset with threshold found\n",  __func__);
		mbhc->micbias_enable = true;
		ret = true;
	}
	if (mbhc->mbhc_cb->mbhc_common_micb_ctrl)
		mbhc->mbhc_cb->mbhc_common_micb_ctrl(codec,
				MBHC_COMMON_MICB_PRECHARGE,
				false);
	if (mbhc->mbhc_cb->set_micbias_value && !mbhc->micbias_enable)
		mbhc->mbhc_cb->set_micbias_value(codec);
	if (mbhc->mbhc_cb->set_auto_zeroing)
		mbhc->mbhc_cb->set_auto_zeroing(codec, false);

	if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic &&
	    !mbhc->micbias_enable)
		mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(codec, MIC_BIAS_2,
						      false);

	pr_debug("%s: leave, micb_enable: %d\n", __func__,
		  mbhc->micbias_enable);
	return ret;
}

static void wcd_mbhc_update_fsm_source(struct wcd_mbhc *mbhc,
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

static void wcd_enable_mbhc_supply(struct wcd_mbhc *mbhc,
			enum wcd_mbhc_plug_type plug_type)
{

	struct snd_soc_codec *codec = mbhc->codec;

	/*
	 * Do not disable micbias if recording is going on or
	 * headset is inserted on the other side of the extn
	 * cable. If headset has been detected current source
	 * needs to be kept enabled for button detection to work.
	 * If the accessory type is invalid or unsupported, we
	 * dont need to enable either of them.
	 */
	if (det_extn_cable_en && mbhc->is_extn_cable &&
		mbhc->mbhc_cb && mbhc->mbhc_cb->extn_use_mb &&
		mbhc->mbhc_cb->extn_use_mb(codec)) {
		if (plug_type == MBHC_PLUG_TYPE_HEADPHONE ||
		    plug_type == MBHC_PLUG_TYPE_HEADSET)
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
	} else {
		if (plug_type == MBHC_PLUG_TYPE_HEADSET) {
			if (mbhc->is_hs_recording || mbhc->micbias_enable) {
				wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);
			} else if ((test_bit(WCD_MBHC_EVENT_PA_HPHL,
					     &mbhc->event_state)) ||
				   (test_bit(WCD_MBHC_EVENT_PA_HPHR,
					     &mbhc->event_state))) {
				wcd_enable_curr_micbias(mbhc,
						WCD_MBHC_EN_PULLUP);
			} else {
				wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_CS);
			}
		} else if (plug_type == MBHC_PLUG_TYPE_HEADPHONE) {
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_CS);
		} else {
			wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_NONE);
		}
	}
}

static bool wcd_mbhc_check_for_spl_headset(struct wcd_mbhc *mbhc,
					   int *spl_hs_cnt)
{
	u16 hs_comp_res_1_8v = 0, hs_comp_res_2_7v = 0;
	bool spl_hs = false;

	if (!mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic)
		goto done;

	if (!spl_hs_cnt) {
		pr_err("%s: spl_hs_cnt is NULL\n", __func__);
		goto done;
	}
	/* Read back hs_comp_res @ 1.8v Micbias */
	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res_1_8v);
	if (!hs_comp_res_1_8v) {
		spl_hs = false;
		goto done;
	}

	/* Bump up MB2 to 2.7v */
	mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
				mbhc->mbhc_cfg->mbhc_micbias, true);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
	usleep_range(10000, 10100);

	/* Read back HS_COMP_RESULT */
	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res_2_7v);
	if (!hs_comp_res_2_7v && hs_comp_res_1_8v)
		spl_hs = true;

	if (spl_hs)
		*spl_hs_cnt += 1;

	/* MB2 back to 1.8v */
	if (*spl_hs_cnt != WCD_MBHC_SPL_HS_CNT) {
		mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(mbhc->codec,
				mbhc->mbhc_cfg->mbhc_micbias, false);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
		usleep_range(10000, 10100);
	}

	if (spl_hs)
		pr_debug("%s: Detected special HS (%d)\n", __func__, spl_hs);

done:
	return spl_hs;
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
static void wcd_mbhc_detect_plug_type(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;
	bool micbias1 = false;

	pr_debug("%s: enter\n", __func__);
	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	if (mbhc->mbhc_cb->hph_pull_down_ctrl)
		mbhc->mbhc_cb->hph_pull_down_ctrl(codec, false);

	if (mbhc->mbhc_cb->micbias_enable_status)
		micbias1 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
								MIC_BIAS_1);

	if (mbhc->mbhc_cb->set_cap_mode)
		mbhc->mbhc_cb->set_cap_mode(codec, micbias1, true);

	if (mbhc->mbhc_cb->mbhc_micbias_control)
		mbhc->mbhc_cb->mbhc_micbias_control(codec, MIC_BIAS_2,
						    MICB_ENABLE);
	else
		wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);

	/* Re-initialize button press completion object */
	reinit_completion(&mbhc->btn_press_compl);
	wcd_schedule_hs_detect_plug(mbhc, &mbhc->correct_plug_swch);
	pr_debug("%s: leave\n", __func__);
}

static void wcd_correct_swch_plug(struct work_struct *work)
{
	struct wcd_mbhc *mbhc;
	struct snd_soc_codec *codec;
	enum wcd_mbhc_plug_type plug_type = MBHC_PLUG_TYPE_INVALID;
	unsigned long timeout;
	u16 hs_comp_res = 0, hphl_sch = 0, mic_sch = 0, btn_result = 0;
	bool wrk_complete = false;
	int pt_gnd_mic_swap_cnt = 0;
	int no_gnd_mic_swap_cnt = 0;
	bool is_pa_on = false, spl_hs = false, spl_hs_reported = false;
	bool micbias2 = false;
	bool micbias1 = false;
	int ret = 0;
	int rc, spl_hs_count = 0;
	int cross_conn;
	int try = 0;

	pr_debug("%s: enter\n", __func__);

	mbhc = container_of(work, struct wcd_mbhc, correct_plug_swch);
	codec = mbhc->codec;

	/*
	 * Enable micbias/pullup for detection in correct work.
	 * This work will get scheduled from detect_plug_type which
	 * will already request for pullup/micbias. If the pullup/micbias
	 * is handled with ref-counts by individual codec drivers, there is
	 * no need to enabale micbias/pullup here
	 */

	wcd_enable_curr_micbias(mbhc, WCD_MBHC_EN_MB);

	/* Enable HW FSM */
	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);
	/*
	 * Check for any button press interrupts before starting 3-sec
	 * loop.
	 */
	rc = wait_for_completion_timeout(&mbhc->btn_press_compl,
			msecs_to_jiffies(WCD_MBHC_BTN_PRESS_COMPL_TIMEOUT_MS));

	WCD_MBHC_REG_READ(WCD_MBHC_BTN_RESULT, btn_result);
	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);

	if (!rc) {
		pr_debug("%s No btn press interrupt\n", __func__);
		if (!btn_result && !hs_comp_res)
			plug_type = MBHC_PLUG_TYPE_HEADSET;
		else if (!btn_result && hs_comp_res)
			plug_type = MBHC_PLUG_TYPE_HIGH_HPH;
		else
			plug_type = MBHC_PLUG_TYPE_INVALID;
	} else {
		if (!btn_result && !hs_comp_res)
			plug_type = MBHC_PLUG_TYPE_HEADPHONE;
		else
			plug_type = MBHC_PLUG_TYPE_INVALID;
	}

	do {
		cross_conn = wcd_check_cross_conn(mbhc);
		try++;
	} while (try < mbhc->swap_thr);

	/*
	 * Check for cross connection 4 times.
	 * Consider the result of the fourth iteration.
	 */
	if (cross_conn > 0) {
		pr_debug("%s: cross con found, start polling\n",
			 __func__);
		plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
		pr_debug("%s: Plug found, plug type is %d\n",
			 __func__, plug_type);
		goto correct_plug_type;
	}

	if ((plug_type == MBHC_PLUG_TYPE_HEADSET ||
	     plug_type == MBHC_PLUG_TYPE_HEADPHONE) &&
	    (!wcd_swch_level_remove(mbhc))) {
		WCD_MBHC_RSC_LOCK(mbhc);
		if (mbhc->current_plug ==  MBHC_PLUG_TYPE_HIGH_HPH)
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_DETECTION_TYPE,
						 0);
		wcd_mbhc_find_plug_and_report(mbhc, plug_type);
		WCD_MBHC_RSC_UNLOCK(mbhc);
	}

correct_plug_type:

	timeout = jiffies + msecs_to_jiffies(HS_DETECT_PLUG_TIME_MS);
	while (!time_after(jiffies, timeout)) {
		if (mbhc->hs_detect_work_stop) {
			pr_debug("%s: stop requested: %d\n", __func__,
					mbhc->hs_detect_work_stop);
			wcd_enable_curr_micbias(mbhc,
						WCD_MBHC_EN_NONE);
			if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic &&
				mbhc->micbias_enable) {
				mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(
					mbhc->codec, MIC_BIAS_2, false);
				if (mbhc->mbhc_cb->set_micbias_value)
					mbhc->mbhc_cb->set_micbias_value(
							mbhc->codec);
				mbhc->micbias_enable = false;
			}
			goto exit;
		}
		if (mbhc->btn_press_intr) {
			wcd_cancel_btn_work(mbhc);
			mbhc->btn_press_intr = false;
		}
		/* Toggle FSM */
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 1);

		/* allow sometime and re-check stop requested again */
		msleep(20);
		if (mbhc->hs_detect_work_stop) {
			pr_debug("%s: stop requested: %d\n", __func__,
					mbhc->hs_detect_work_stop);
			wcd_enable_curr_micbias(mbhc,
						WCD_MBHC_EN_NONE);
			if (mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic &&
				mbhc->micbias_enable) {
				mbhc->mbhc_cb->mbhc_micb_ctrl_thr_mic(
					mbhc->codec, MIC_BIAS_2, false);
				if (mbhc->mbhc_cb->set_micbias_value)
					mbhc->mbhc_cb->set_micbias_value(
							mbhc->codec);
				mbhc->micbias_enable = false;
			}
			goto exit;
		}
		WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_res);

		pr_debug("%s: hs_comp_res: %x\n", __func__, hs_comp_res);
		if (mbhc->mbhc_cb->hph_pa_on_status)
			is_pa_on = mbhc->mbhc_cb->hph_pa_on_status(codec);

		/*
		 * instead of hogging system by contineous polling, wait for
		 * sometime and re-check stop request again.
		 */
		msleep(180);
		if (hs_comp_res && (spl_hs_count < WCD_MBHC_SPL_HS_CNT)) {
			spl_hs = wcd_mbhc_check_for_spl_headset(mbhc,
								&spl_hs_count);

			if (spl_hs_count == WCD_MBHC_SPL_HS_CNT) {
				hs_comp_res = 0;
				spl_hs = true;
				mbhc->micbias_enable = true;
			}
		}

		if ((!hs_comp_res) && (!is_pa_on)) {
			/* Check for cross connection*/
			ret = wcd_check_cross_conn(mbhc);
			if (ret < 0) {
				continue;
			} else if (ret > 0) {
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
					pr_debug("%s: switch didn't work\n",
						  __func__);
					plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
					goto report;
				} else {
					plug_type = MBHC_PLUG_TYPE_GND_MIC_SWAP;
				}
			} else {
				no_gnd_mic_swap_cnt++;
				pt_gnd_mic_swap_cnt = 0;
				plug_type = MBHC_PLUG_TYPE_HEADSET;
				if ((no_gnd_mic_swap_cnt <
				    GND_MIC_SWAP_THRESHOLD) &&
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

		WCD_MBHC_REG_READ(WCD_MBHC_HPHL_SCHMT_RESULT, hphl_sch);
		WCD_MBHC_REG_READ(WCD_MBHC_MIC_SCHMT_RESULT, mic_sch);
		if (hs_comp_res && !(hphl_sch || mic_sch)) {
			pr_debug("%s: cable is extension cable\n", __func__);
			plug_type = MBHC_PLUG_TYPE_HIGH_HPH;
			wrk_complete = true;
		} else {
			pr_debug("%s: cable might be headset: %d\n", __func__,
					plug_type);
			if (!(plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP)) {
				plug_type = MBHC_PLUG_TYPE_HEADSET;
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
				if (((mbhc->current_plug !=
				      MBHC_PLUG_TYPE_HEADSET) &&
				     (mbhc->current_plug !=
				      MBHC_PLUG_TYPE_ANC_HEADPHONE)) &&
				    !wcd_swch_level_remove(mbhc) &&
				    !mbhc->btn_press_intr) {
					pr_debug("%s: cable is %sheadset\n",
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
	if (!wrk_complete && mbhc->btn_press_intr) {
		pr_debug("%s: Can be slow insertion of headphone\n", __func__);
		wcd_cancel_btn_work(mbhc);
		/* Report as headphone only if previously
		 * not reported as lineout
		 */
		if (!mbhc->force_linein)
			plug_type = MBHC_PLUG_TYPE_HEADPHONE;
	}
	/*
	 * If plug_tye is headset, we might have already reported either in
	 * detect_plug-type or in above while loop, no need to report again
	 */
	if (!wrk_complete && ((plug_type == MBHC_PLUG_TYPE_HEADSET) ||
	    (plug_type == MBHC_PLUG_TYPE_ANC_HEADPHONE))) {
		pr_debug("%s: plug_type:0x%x already reported\n",
			 __func__, mbhc->current_plug);
		goto enable_supply;
	}

	if (plug_type == MBHC_PLUG_TYPE_HIGH_HPH &&
		(!det_extn_cable_en)) {
		if (wcd_is_special_headset(mbhc)) {
			pr_debug("%s: Special headset found %d\n",
					__func__, plug_type);
			plug_type = MBHC_PLUG_TYPE_HEADSET;
			goto report;
		}
	}

report:
	if (wcd_swch_level_remove(mbhc)) {
		pr_debug("%s: Switch level is low\n", __func__);
		goto exit;
	}
	if (plug_type == MBHC_PLUG_TYPE_GND_MIC_SWAP && mbhc->btn_press_intr) {
		pr_debug("%s: insertion of headphone with swap\n", __func__);
		wcd_cancel_btn_work(mbhc);
		plug_type = MBHC_PLUG_TYPE_HEADPHONE;
	}
	pr_debug("%s: Valid plug found, plug type %d wrk_cmpt %d btn_intr %d\n",
			__func__, plug_type, wrk_complete,
			mbhc->btn_press_intr);
	WCD_MBHC_RSC_LOCK(mbhc);
	wcd_mbhc_find_plug_and_report(mbhc, plug_type);
	WCD_MBHC_RSC_UNLOCK(mbhc);
enable_supply:
	if (mbhc->mbhc_cb->mbhc_micbias_control)
		wcd_mbhc_update_fsm_source(mbhc, plug_type);
	else
		wcd_enable_mbhc_supply(mbhc, plug_type);
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

	if (mbhc->mbhc_cb->micbias_enable_status) {
		micbias1 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
								MIC_BIAS_1);
		micbias2 = mbhc->mbhc_cb->micbias_enable_status(mbhc,
								MIC_BIAS_2);
	}

	if (mbhc->mbhc_cfg->detect_extn_cable &&
	    ((plug_type == MBHC_PLUG_TYPE_HEADPHONE) ||
	     (plug_type == MBHC_PLUG_TYPE_HEADSET)) &&
	    !mbhc->hs_detect_work_stop) {
		WCD_MBHC_RSC_LOCK(mbhc);
		wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_REM, true);
		WCD_MBHC_RSC_UNLOCK(mbhc);
	}
	if (mbhc->mbhc_cb->set_cap_mode)
		mbhc->mbhc_cb->set_cap_mode(codec, micbias1, micbias2);

	if (mbhc->mbhc_cb->hph_pull_down_ctrl)
		mbhc->mbhc_cb->hph_pull_down_ctrl(codec, true);

	mbhc->mbhc_cb->lock_sleep(mbhc, false);
	pr_debug("%s: leave\n", __func__);
}

static irqreturn_t wcd_mbhc_hs_rem_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	u8 hs_comp_result = 0, hphl_sch = 0, mic_sch = 0;
	static u16 hphl_trigerred;
	static u16 mic_trigerred;
	unsigned long timeout;
	bool removed = true;
	int retry = 0;
	bool hphpa_on = false;
	u8 moisture_status = 0;

	pr_debug("%s: enter\n", __func__);

	WCD_MBHC_RSC_LOCK(mbhc);

	timeout = jiffies +
		  msecs_to_jiffies(WCD_FAKE_REMOVAL_MIN_PERIOD_MS);
	do {
		retry++;
		/*
		 * read the result register every 10ms to look for
		 * any change in HS_COMP_RESULT bit
		 */
		usleep_range(10000, 10100);
		WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_result);
		pr_debug("%s: Check result reg for fake removal: hs_comp_res %x\n",
			 __func__, hs_comp_result);
		if ((!hs_comp_result) &&
		    retry > FAKE_REM_RETRY_ATTEMPTS) {
			removed = false;
			break;
		}
	} while (!time_after(jiffies, timeout));

	if (wcd_swch_level_remove(mbhc)) {
		pr_debug("%s: Switch level is low ", __func__);
		goto exit;
	}
	pr_debug("%s: headset %s actually removed\n", __func__,
		removed ? "" : "not ");

	WCD_MBHC_REG_READ(WCD_MBHC_HPHL_SCHMT_RESULT, hphl_sch);
	WCD_MBHC_REG_READ(WCD_MBHC_MIC_SCHMT_RESULT, mic_sch);
	WCD_MBHC_REG_READ(WCD_MBHC_HS_COMP_RESULT, hs_comp_result);

	if (removed) {
		if (mbhc->mbhc_cfg->moisture_en) {
			if (mbhc->mbhc_cb->hph_pa_on_status)
				if (
				mbhc->mbhc_cb->hph_pa_on_status(mbhc->codec)) {
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

			WCD_MBHC_REG_READ(
				WCD_MBHC_MOISTURE_STATUS, moisture_status);
		}

		if (mbhc->mbhc_cfg->moisture_en && !moisture_status) {
			pr_debug("%s: moisture present in jack\n", __func__);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_L_DET_EN, 0);
			WCD_MBHC_REG_UPDATE_BITS(
				WCD_MBHC_MECH_DETECTION_TYPE, 1);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_L_DET_EN, 1);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_FSM_EN, 0);
			WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_BTN_ISRC_CTL, 0);
			mbhc->btn_press_intr = false;
			mbhc->is_btn_press = false;
			if (mbhc->current_plug == MBHC_PLUG_TYPE_HEADSET)
				wcd_mbhc_report_plug(
					mbhc, 0, SND_JACK_HEADSET);
			else if (mbhc->current_plug ==
					MBHC_PLUG_TYPE_HEADPHONE)
				wcd_mbhc_report_plug(
					mbhc, 0, SND_JACK_HEADPHONE);
			else if (mbhc->current_plug ==
					MBHC_PLUG_TYPE_GND_MIC_SWAP)
				wcd_mbhc_report_plug(
					mbhc, 0, SND_JACK_UNSUPPORTED);
			else if (mbhc->current_plug ==
					MBHC_PLUG_TYPE_HIGH_HPH)
				wcd_mbhc_report_plug(
					mbhc, 0, SND_JACK_LINEOUT);
		} else {
			if (!(hphl_sch && mic_sch && hs_comp_result)) {
				/*
				 * extension cable is still plugged in
				 * report it as LINEOUT device
				 */
				goto report_unplug;
			} else {
				if (!mic_sch) {
					mic_trigerred++;
					pr_debug(
					"%s: Removal MIC trigerred %d\n",
					__func__, mic_trigerred);
				}
				if (!hphl_sch) {
					hphl_trigerred++;
					pr_debug(
					"%s: Removal HPHL trigerred %d\n",
					 __func__, hphl_trigerred);
				}
				if (mic_trigerred && hphl_trigerred) {
					/*
					 * extension cable is still plugged in
					 * report it as LINEOUT device
					 */
					goto report_unplug;
				}
			}
		}
	}
exit:
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;

report_unplug:
	wcd_mbhc_elec_hs_report_unplug(mbhc);
	if (hphpa_on) {
		hphpa_on = false;
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPHL_PA_EN, 1);
		WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_HPH_PA_EN, 1);
	}
	hphl_trigerred = 0;
	mic_trigerred = 0;
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t wcd_mbhc_hs_ins_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	bool detection_type = 0, hphl_sch = 0, mic_sch = 0;
	u16 elect_result = 0;
	static u16 hphl_trigerred;
	static u16 mic_trigerred;

	pr_debug("%s: enter\n", __func__);
	if (!mbhc->mbhc_cfg->detect_extn_cable) {
		pr_debug("%s: Returning as Extension cable feature not enabled\n",
			__func__);
		return IRQ_HANDLED;
	}
	WCD_MBHC_RSC_LOCK(mbhc);

	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_DETECTION_TYPE, detection_type);
	WCD_MBHC_REG_READ(WCD_MBHC_ELECT_RESULT, elect_result);

	pr_debug("%s: detection_type %d, elect_result %x\n", __func__,
				detection_type, elect_result);
	if (detection_type) {
		/* check if both Left and MIC Schmitt triggers are triggered */
		WCD_MBHC_REG_READ(WCD_MBHC_HPHL_SCHMT_RESULT, hphl_sch);
		WCD_MBHC_REG_READ(WCD_MBHC_MIC_SCHMT_RESULT, mic_sch);
		if (hphl_sch && mic_sch) {
			/* Go for plug type determination */
			pr_debug("%s: Go for plug type determination\n",
				  __func__);
			goto determine_plug;

		} else {
			if (mic_sch) {
				mic_trigerred++;
				pr_debug("%s: Insertion MIC trigerred %d\n",
					 __func__, mic_trigerred);
				WCD_MBHC_REG_UPDATE_BITS(
						WCD_MBHC_ELECT_SCHMT_ISRC,
						0);
				msleep(20);
				WCD_MBHC_REG_UPDATE_BITS(
						WCD_MBHC_ELECT_SCHMT_ISRC,
						1);
			}
			if (hphl_sch) {
				hphl_trigerred++;
				pr_debug("%s: Insertion HPHL trigerred %d\n",
					 __func__, hphl_trigerred);
			}
			if (mic_trigerred && hphl_trigerred) {
				/* Go for plug type determination */
				pr_debug("%s: Go for plug type determination\n",
					 __func__);
				goto determine_plug;
			}
		}
	}
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;

determine_plug:
	/*
	 * Disable HPHL trigger and MIC Schmitt triggers.
	 * Setup for insertion detection.
	 */
	pr_debug("%s: Disable insertion interrupt\n", __func__);
	wcd_mbhc_hs_elec_irq(mbhc, WCD_MBHC_ELEC_HS_INS,
			     false);

	WCD_MBHC_REG_UPDATE_BITS(WCD_MBHC_ELECT_SCHMT_ISRC, 0);
	hphl_trigerred = 0;
	mic_trigerred = 0;
	mbhc->is_extn_cable = true;
	mbhc->btn_press_intr = false;
	wcd_mbhc_detect_plug_type(mbhc);
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;
}

static struct wcd_mbhc_fn mbhc_fn = {
	.wcd_mbhc_hs_ins_irq = wcd_mbhc_hs_ins_irq,
	.wcd_mbhc_hs_rem_irq = wcd_mbhc_hs_rem_irq,
	.wcd_mbhc_detect_plug_type = wcd_mbhc_detect_plug_type,
	.wcd_mbhc_detect_anc_plug_type = wcd_mbhc_detect_anc_plug_type,
	.wcd_cancel_hs_detect_plug = wcd_cancel_hs_detect_plug,
};

/* Function: wcd_mbhc_legacy_init
 * @mbhc: MBHC function pointer
 * Description: Initialize MBHC legacy based function pointers to MBHC structure
 */
void wcd_mbhc_legacy_init(struct wcd_mbhc *mbhc)
{
	if (!mbhc) {
		pr_err("%s: mbhc is NULL\n", __func__);
		return;
	}
	mbhc->mbhc_fn = &mbhc_fn;
	INIT_WORK(&mbhc->correct_plug_swch, wcd_correct_swch_plug);
}
EXPORT_SYMBOL(wcd_mbhc_legacy_init);
