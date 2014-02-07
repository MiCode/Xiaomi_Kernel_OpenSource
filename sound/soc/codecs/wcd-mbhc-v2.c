/* Copyright (c) 2014, The Linux Foundation. All rights reserved.
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
#include <linux/mfd/wcd9xxx/pdata.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>
#include <sound/tlv.h>
#include <sound/jack.h>
#include "wcd-mbhc-v2.h"
#include "msm8x16_wcd_registers.h"
#include "msm8916-wcd-irq.h"

#define WCD_MBHC_JACK_MASK (SND_JACK_HEADSET | SND_JACK_OC_HPHL | \
			   SND_JACK_OC_HPHR | SND_JACK_LINEOUT | \
			   SND_JACK_UNSUPPORTED)
#define WCD_MBHC_JACK_BUTTON_MASK (SND_JACK_BTN_0 | SND_JACK_BTN_1 | \
				  SND_JACK_BTN_2 | SND_JACK_BTN_3 | \
				  SND_JACK_BTN_4)
#define OCP_ATTEMPT 1

#define WCD_MBHC_RSC_LOCK(mbhc)			\
{							\
	pr_debug("%s: Acquiring BCL\n", __func__);	\
	mutex_lock(&mbhc->codec_resource_lock);		\
	pr_debug("%s: Acquiring BCL done\n", __func__);	\
}

#define WCD_MBHC_RSC_UNLOCK(mbhc)			\
{							\
	pr_debug("%s: Release BCL\n", __func__);	\
	mutex_unlock(&mbhc->codec_resource_lock);	\
}

#define WCD_MBHC_RSC_ASSERT_LOCKED(mbhc)		\
{							\
	WARN_ONCE(!mutex_is_locked(&mbhc->codec_resource_lock), \
		  "%s: BCL should have acquired\n", __func__); \
}

static void wcd_mbhc_jack_report(struct wcd_mbhc *mbhc,
				struct snd_soc_jack *jack, int status, int mask)
{
	snd_soc_jack_report_no_dapm(jack, status, mask);
}

static void __hphocp_off_report(struct wcd_mbhc *mbhc, u32 jack_status,
				int irq)
{
	struct snd_soc_codec *codec;

	pr_debug("%s: clear ocp status %x\n", __func__, jack_status);
	codec = mbhc->codec;
	if (mbhc->hph_status & jack_status) {
		mbhc->hph_status &= ~jack_status;
		wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
				    mbhc->hph_status, WCD_MBHC_JACK_MASK);
		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL,
				0x10, 0x00);

		snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL,
				0x10, 0x10);
		/*
		 * reset retry counter as PA is turned off signifying
		 * start of new OCP detection session
		 */
		if (mbhc->intr_ids->hph_left_ocp)
			mbhc->hphlocp_cnt = 0;
		else
			mbhc->hphrocp_cnt = 0;
		wcd9xxx_enable_irq(irq);
	}
}

static void hphrocp_off_report(struct wcd_mbhc *mbhc, u32 jack_status)
{
	__hphocp_off_report(mbhc, SND_JACK_OC_HPHR,
			    mbhc->intr_ids->hph_right_ocp);
}

static void hphlocp_off_report(struct wcd_mbhc *mbhc, u32 jack_status)
{
	__hphocp_off_report(mbhc, SND_JACK_OC_HPHL,
			    mbhc->intr_ids->hph_left_ocp);
}

static void wcd_mbhc_calc_impedance(struct wcd_mbhc *mbhc, uint32_t *zl,
	uint32_t *zr)
{
	struct snd_soc_codec *codec = mbhc->codec;
	u16 impedance_l, impedance_r;
	u16 impedance_l_fixed;

	pr_debug("%s: enter\n", __func__);
	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);
	/* Enable ZDET_L_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x08, 0x08);
	usleep_range(2000, 2100);
	/* Read Left impedance value from Result1 */
	impedance_l = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
	/* Enable ZDET_R_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x0C, 0x04);
	usleep_range(2000, 2100);
	/* Read Right impedance value from Result1 */
	impedance_r = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x04, 0x00);

	if (impedance_l)
		goto exit;

	/*
	 * As the result is 0 impedance is < 200 use
	 * RAMP to measure impedance further.
	 */

	/* Enable RAMP_L , RAMP_R & ZDET_CHG*/
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x03, 0x03);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x02, 0x02);
	usleep_range(50000, 50100);
	/* Enable ZDET_DISCHG_CAP_CTL  to add extra capacitance */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x01, 0x01);
	usleep_range(5000, 5100);

	/* Enable ZDET_L_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x08, 0x08);
	usleep_range(2000, 2100);
	/* Read Left impedance value from Result1 */
	impedance_l = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
	/* Enable ZDET_R_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x0C, 0x04);
	usleep_range(2000, 2100);
	/* Read Right impedance value from Result1 */
	impedance_r = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x04, 0x00);

	if (!mbhc->mbhc_cfg->mono_stero_detection) {
		/* Set ZDET_CHG to 0  to discharge ramp */
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
				0x02, 0x00);
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
				0x03, 0x00);
		usleep_range(40000, 40100);
		goto exit;
	}

	/* Disable Set ZDET_CONN_RAMP_L and enable ZDET_CONN_FIXED_L */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x02, 0x00);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN1_ZDETM_CTL,
			0x02, 0x02);
	/* Set ZDET_CHG to 0  */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x02, 0x00);
	usleep_range(40000, 40100);

	/* Set ZDET_CONN_RAMP_R to 0  */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x01, 0x00);
	/* Enable ZDET_L_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x08, 0x08);
	usleep_range(2000, 2100);
	/* Read Left impedance value from Result1 */
	impedance_l_fixed = snd_soc_read(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
	/* Disable ZDET_L_MEAS_EN */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x08, 0x00);
	/*
	 * impedance_l is equal to impedance_l_fixed then headset is stereo
	 * otherwise headset is mono
	 */

	/* Enable ZDET_CHG  */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x02, 0x02);
	usleep_range(10000, 10100);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x02, 0x02);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN1_ZDETM_CTL,
			0x02, 0x00);
	/* Set ZDET_CHG to 0  to discharge HPHL */
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
			0x02, 0x00);
	usleep_range(40000, 40100);
	snd_soc_update_bits(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_BTN0_ZDETL_CTL,
			0x02, 0x00);

exit:
	*zl = impedance_l;
	*zr = impedance_r;
	pr_debug("%s: RL %d milliohm, RR %d milliohm\n", __func__, *zl, *zr);
	pr_debug("%s: Impedance detection completed\n", __func__);
}

static void wcd_mbhc_report_plug(struct wcd_mbhc *mbhc, int insertion,
				enum snd_jack_types jack_type)
{
	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	pr_debug("%s: enter insertion %d hph_status %x\n",
		 __func__, insertion, mbhc->hph_status);
	if (!insertion) {
		/* Report removal */
		mbhc->hph_status &= ~jack_type;
		if (mbhc->buttons_pressed) {
			pr_debug("%s: release of button press%d\n",
				 __func__, jack_type);
			wcd_mbhc_jack_report(mbhc, &mbhc->button_jack, 0,
					    mbhc->buttons_pressed);
			mbhc->buttons_pressed &=
				~WCD_MBHC_JACK_BUTTON_MASK;
		}

		mbhc->zl = mbhc->zr = 0;
		pr_debug("%s: Reporting removal %d(%x)\n", __func__,
			 jack_type, mbhc->hph_status);
		wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
				mbhc->hph_status, WCD_MBHC_JACK_MASK);
		hphrocp_off_report(mbhc, SND_JACK_OC_HPHR);
		hphlocp_off_report(mbhc, SND_JACK_OC_HPHL);
		mbhc->current_plug = PLUG_TYPE_NONE;
	} else {
		/*
		 * Report removal of current jack type.
		 * Headphone to headset shouldn't report headphone
		 * removal.
		 */
		if (mbhc->mbhc_cfg->detect_extn_cable &&
		    (mbhc->hph_status && mbhc->hph_status != jack_type)) {

			mbhc->zl = mbhc->zr = 0;
			pr_debug("%s: Reporting removal (%x)\n",
				 __func__, mbhc->hph_status);
			wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
					    0, WCD_MBHC_JACK_MASK);
			mbhc->hph_status &= ~(SND_JACK_HEADSET |
						SND_JACK_LINEOUT);
		}
		/* Report insertion */
		mbhc->hph_status |= jack_type;

		if (jack_type == SND_JACK_HEADPHONE)
			mbhc->current_plug = PLUG_TYPE_HEADPHONE;
		else if (jack_type == SND_JACK_UNSUPPORTED)
			mbhc->current_plug = PLUG_TYPE_GND_MIC_SWAP;
		else if (jack_type == SND_JACK_HEADSET)
			mbhc->current_plug = PLUG_TYPE_HEADSET;
		else if (jack_type == SND_JACK_LINEOUT)
			mbhc->current_plug = PLUG_TYPE_HIGH_HPH;

		if (mbhc->impedance_detect)
			wcd_mbhc_calc_impedance(mbhc,
					&mbhc->zl, &mbhc->zr);

		pr_debug("%s: Reporting insertion %d(%x)\n", __func__,
			 jack_type, mbhc->hph_status);
		wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
				    mbhc->hph_status, WCD_MBHC_JACK_MASK);
	}
	pr_debug("%s: leave hph_status %x\n", __func__, mbhc->hph_status);
}

static void wcd_mbhc_find_plug_and_report(struct wcd_mbhc *mbhc,
					 enum wcd_mbhc_plug_type plug_type)
{
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter current_plug(%d) new_plug(%d)\n",
		 __func__, mbhc->current_plug, plug_type);

	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);

	if (plug_type == PLUG_TYPE_HEADPHONE &&
	    mbhc->current_plug == PLUG_TYPE_NONE) {
		/*
		 * Nothing was reported previously
		 * report a headphone or unsupported
		 */
		wcd_mbhc_report_plug(mbhc, 1, SND_JACK_HEADPHONE);
	} else if (plug_type == PLUG_TYPE_GND_MIC_SWAP) {
		wcd_mbhc_report_plug(mbhc, 1, SND_JACK_UNSUPPORTED);
	} else if (plug_type == PLUG_TYPE_HEADSET) {
		/*
		 * If Headphone was reported previously, this will
		 * only report the mic line
		 */
		wcd_mbhc_report_plug(mbhc, 1, SND_JACK_HEADSET);
	} else if (plug_type == PLUG_TYPE_HIGH_HPH) {
		if (mbhc->mbhc_cfg->detect_extn_cable) {
			/* High impedance device found. Report as LINEOUT */
			wcd_mbhc_report_plug(mbhc, 1, SND_JACK_LINEOUT);
			pr_debug("%s: setup mic trigger for further detection\n",
				 __func__);
			/*
			 * Enable HPHL trigger and MIC Schmitt triggers.
			 * Setup for insertion detection.
			 */
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2,
					0x06, 0x06);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1,
					0x01, 0x01);
			wcd9xxx_enable_irq(
					mbhc->intr_ids->mbhc_hs_ins_rem_intr);
		} else {
			wcd_mbhc_report_plug(mbhc, 1, SND_JACK_UNSUPPORTED);
		}
	} else {
		WARN(1, "Unexpected current plug_type %d, plug_type %d\n",
		     mbhc->current_plug, plug_type);
	}
	pr_debug("%s: leave\n", __func__);
}

/* called under codec_resource_lock acquisition */
static void wcd_mbhc_detect_plug_type(struct wcd_mbhc *mbhc)
{
	struct snd_soc_codec *codec = mbhc->codec;
	long timeout = msecs_to_jiffies(20);   /* 20ms */
	enum wcd_mbhc_plug_type plug_type;
	int timeout_result;
	u16 result1, result2;

	pr_debug("%s: enter\n", __func__);
	WCD_MBHC_RSC_ASSERT_LOCKED(mbhc);
	/*
	 * Wait for 20msec for FSM to complete its task.
	 * wakeup if btn pres intr occurs
	 */
	mbhc->is_btn_press = false;
	WCD_MBHC_RSC_UNLOCK(mbhc);
	timeout_result = wait_event_interruptible_timeout(mbhc->wait_btn_press,
			mbhc->is_btn_press, timeout);

	WCD_MBHC_RSC_LOCK(mbhc);
	result1 = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
	result2 = snd_soc_read(codec,
			MSM8X16_WCD_A_ANALOG_MBHC_ZDET_ELECT_RESULT);

	if (!timeout_result) {
		pr_debug("%s No btn press interrupt\n", __func__);
		if (!result1 && !result2)
			plug_type = PLUG_TYPE_HEADSET;
		else if (!result1 && (result2 & 0x01))
			plug_type = PLUG_TYPE_HIGH_HPH;
		else if (!result1 && (result2 & 0x04))
			plug_type = PLUG_TYPE_GND_MIC_SWAP;
		else {
			plug_type = PLUG_TYPE_INVALID;
			goto exit;
		}
	} else {
		if (!result1 && !result2)
			plug_type = PLUG_TYPE_HEADPHONE;
		else {
			plug_type = PLUG_TYPE_INVALID;
			goto exit;
		}
	}
	pr_debug("%s: Valid plug found, plug type is %d\n",
			 __func__, plug_type);
	wcd_mbhc_find_plug_and_report(mbhc, plug_type);
exit:
	pr_debug("%s: leave\n", __func__);
}

static void wcd_mbhc_swch_irq_handler(struct wcd_mbhc *mbhc)
{
	bool detection_type;
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);

	WCD_MBHC_RSC_LOCK(mbhc);

	mbhc->in_swch_irq_handler = true;

	detection_type = (snd_soc_read(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1)) & 0x20;

	/* Set the detection type appropriately */
	snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1,
			0x20, (!detection_type << 5));

	if ((mbhc->current_plug == PLUG_TYPE_NONE) && detection_type) {
		/* Enable HW FSM */
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
				0x80, 0x80);
		wcd_mbhc_detect_plug_type(mbhc);
	} else if ((mbhc->current_plug != PLUG_TYPE_NONE) && !detection_type) {
		/* Disable HW FSM */
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_FSM_CTL,
				0x80, 0x00);
		if (mbhc->current_plug == PLUG_TYPE_HEADPHONE) {
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_HEADPHONE);
		} else if (mbhc->current_plug == PLUG_TYPE_GND_MIC_SWAP) {
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_UNSUPPORTED);
		} else if (mbhc->current_plug == PLUG_TYPE_HEADSET) {
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_HEADSET);
		} else if (mbhc->current_plug == PLUG_TYPE_HIGH_HPH) {
			wcd_mbhc_report_plug(mbhc, 0, SND_JACK_LINEOUT);
			/*
			 * Disable HPHL trigger and MIC Schmitt triggers.
			 * Setup for insertion detection.
			 */
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2,
					0x06, 0x00);
			snd_soc_update_bits(codec,
					MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1,
					0x01, 0x00);
			wcd9xxx_disable_irq(
					mbhc->intr_ids->mbhc_hs_ins_rem_intr);
		}

	}
	mbhc->in_swch_irq_handler = false;
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
}

static irqreturn_t wcd_mbhc_mech_plug_detect_irq(int irq, void *data)
{
	int r = IRQ_HANDLED;
	struct wcd_mbhc *mbhc = data;

	pr_debug("%s: enter\n", __func__);
	if (unlikely(wcd9xxx_lock_sleep() == false)) {
		pr_warn("%s: failed to hold suspend\n", __func__);
		r = IRQ_NONE;
	} else {
		/* Call handler */
		wcd_mbhc_swch_irq_handler(mbhc);
		wcd9xxx_unlock_sleep();
	}

	pr_debug("%s: leave %d\n", __func__, r);
	return r;
}

static int wcd_mbhc_get_button_mask(u16 btn)
{
	int mask = 0;

	switch (btn) {
	case 0:
		mask = SND_JACK_BTN_0;
		break;
	case 1:
		mask = SND_JACK_BTN_1;
		break;
	case 3:
		mask = SND_JACK_BTN_2;
		break;
	case 7:
		mask = SND_JACK_BTN_3;
		break;
	case 15:
		mask = SND_JACK_BTN_4;
		break;
	default:
		break;
	}
	return mask;
}

static irqreturn_t wcd_mbhc_hs_ins_rem_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	struct snd_soc_codec *codec = mbhc->codec;
	u16 result2;
	bool detection_type;

	pr_debug("%s: enter\n", __func__);
	if (!mbhc->mbhc_cfg->detect_extn_cable) {
		pr_debug("%s: Returning as Extension cable feature not enabled\n",
			__func__);
		return IRQ_HANDLED;
	}
	WCD_MBHC_RSC_LOCK(mbhc);

	detection_type = (snd_soc_read(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1)) & 0x01;

	/* Set the detection type appropriately */
	snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1,
			0x01, (!detection_type));

	/* chech if both Left and MIC Schmitt triggers are triggered */
	result2 = (snd_soc_read(codec,
				MSM8X16_WCD_A_ANALOG_MBHC_ZDET_ELECT_RESULT)
				^ 0x0A);
	if (detection_type) {
		if (result2)
			/*  Go for plug type determination */
			wcd_mbhc_detect_plug_type(mbhc);
	} else if (!result2) {
		/*
		 * extension cable is still plugged in
		 * report it as LINEOUT device
		 */
		wcd_mbhc_report_plug(mbhc, 1, SND_JACK_LINEOUT);
	} else {
		pr_debug("%s: detection_type %d, result2 %d\n", __func__,
				detection_type, result2);
	}
	WCD_MBHC_RSC_UNLOCK(mbhc);
	pr_debug("%s: leave\n", __func__);
	return IRQ_HANDLED;
}

irqreturn_t wcd_mbhc_btn_press_handler(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	struct snd_soc_codec *codec = mbhc->codec;
	u16 result1;
	u16 mask;

	pr_debug("%s: enter\n", __func__);
	WCD_MBHC_RSC_LOCK(mbhc);
	/* send event to sw intr handler*/
	mbhc->is_btn_press = true;
	wake_up_interruptible(&mbhc->wait_btn_press);
	/* If switch interrupt already kicked in, ignore button press */
	if (mbhc->in_swch_irq_handler) {
		pr_debug("%s: Swtich level changed, ignore button press\n",
			 __func__);
		goto done;
	}

	if (mbhc->current_plug != PLUG_TYPE_HEADSET) {
		pr_debug("%s: Plug type is not headset, ignore button press\n",
			 __func__);
		goto done;
	}

	result1 = snd_soc_read(codec, MSM8X16_WCD_A_ANALOG_MBHC_BTN_RESULT);
	mask = wcd_mbhc_get_button_mask(result1);
	mbhc->buttons_pressed |= mask;
	pr_debug("%s: Reporting btn press\n", __func__);
	wcd_mbhc_jack_report(mbhc, &mbhc->button_jack,
			mbhc->buttons_pressed, mbhc->buttons_pressed);
done:
	pr_debug("%s: leave\n", __func__);
	WCD_MBHC_RSC_UNLOCK(mbhc);
	return IRQ_HANDLED;
}

static irqreturn_t wcd_mbhc_release_handler(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;

	pr_debug("%s: enter\n", __func__);
	WCD_MBHC_RSC_LOCK(mbhc);

	/*
	 * If current plug is headphone then there is no chance to
	 * get btn release interrupt, so connected cable should be
	 * headset not headphone.
	 */
	if (mbhc->current_plug == PLUG_TYPE_HEADPHONE) {
		wcd_mbhc_report_plug(mbhc, 1, SND_JACK_HEADSET);
		goto exit;

	}
	if (mbhc->buttons_pressed & WCD_MBHC_JACK_BUTTON_MASK) {
		if (mbhc->in_swch_irq_handler) {
			pr_debug("%s: Switch irq kicked in, ignore\n",
					__func__);
		} else {
			pr_debug("%s: Reporting btn release\n", __func__);
			wcd_mbhc_jack_report(mbhc, &mbhc->button_jack,
					0, mbhc->buttons_pressed);
		}

		mbhc->buttons_pressed &= ~WCD_MBHC_JACK_BUTTON_MASK;
	}
exit:
	pr_debug("%s: leave\n", __func__);
	WCD_MBHC_RSC_UNLOCK(mbhc);
	return IRQ_HANDLED;
}

static irqreturn_t wcd_mbhc_hphl_ocp_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	struct snd_soc_codec *codec;

	pr_debug("%s: received HPHL OCP irq\n", __func__);
	if (mbhc) {
		codec = mbhc->codec;
		if ((mbhc->hphlocp_cnt < OCP_ATTEMPT) &&
		    (!mbhc->hphrocp_cnt)) {
			pr_debug("%s: retry\n", __func__);
			mbhc->hphlocp_cnt++;
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL,
				0x10, 0x00);
			snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL,
				0x10, 0x10);
		} else {
			wcd9xxx_disable_irq(mbhc->intr_ids->hph_left_ocp);
			mbhc->hph_status |= SND_JACK_OC_HPHL;
			wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
					    mbhc->hph_status,
					    WCD_MBHC_JACK_MASK);
		}
	} else {
		pr_err("%s: Bad wcd9xxx private data\n", __func__);
	}
	return IRQ_HANDLED;
}

static irqreturn_t wcd_mbhc_hphr_ocp_irq(int irq, void *data)
{
	struct wcd_mbhc *mbhc = data;
	struct snd_soc_codec *codec;

	pr_debug("%s: received HPHR OCP irq\n", __func__);
	codec = mbhc->codec;
	if ((mbhc->hphrocp_cnt < OCP_ATTEMPT) &&
	    (!mbhc->hphlocp_cnt)) {
		pr_debug("%s: retry\n", __func__);
		mbhc->hphrocp_cnt++;
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL,
				0x10, 0x00);
		snd_soc_update_bits(codec,
				MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL,
				0x10, 0x10);
	} else {
		wcd9xxx_disable_irq(mbhc->intr_ids->hph_right_ocp);
		mbhc->hph_status |= SND_JACK_OC_HPHR;
		wcd_mbhc_jack_report(mbhc, &mbhc->headset_jack,
				    mbhc->hph_status, WCD_MBHC_JACK_MASK);
	}
	return IRQ_HANDLED;
}

static int wcd_mbhc_initialise(struct wcd_mbhc *mbhc)
{
	int ret = 0;
	struct snd_soc_codec *codec = mbhc->codec;

	pr_debug("%s: enter\n", __func__);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_1, 0xF4);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_MBHC_DET_CTL_2, 0x08);
	snd_soc_write(codec, MSM8X16_WCD_A_ANALOG_MBHC_DBNC_TIMER, 0x98);

	snd_soc_update_bits(codec, MSM8X16_WCD_A_ANALOG_RX_COM_OCP_CTL,
			0x10, 0x00);
	wcd9xxx_enable_irq(mbhc->intr_ids->hph_left_ocp);
	wcd9xxx_enable_irq(mbhc->intr_ids->hph_right_ocp);
	pr_debug("%s: leave\n", __func__);
	return ret;
}

int wcd_mbhc_start(struct wcd_mbhc *mbhc,
		       struct wcd_mbhc_config *mbhc_cfg)
{
	int rc = 0;

	pr_debug("%s: enter\n", __func__);
	rc = wcd_mbhc_initialise(mbhc);
	pr_debug("%s: leave %d\n", __func__, rc);
	return rc;
}
EXPORT_SYMBOL(wcd_mbhc_start);

void wcd_mbhc_stop(struct wcd_mbhc *mbhc)
{
	pr_debug("%s: enter\n", __func__);
	wcd9xxx_disable_irq(mbhc->intr_ids->hph_left_ocp);
	wcd9xxx_disable_irq(mbhc->intr_ids->hph_right_ocp);
	pr_debug("%s: leave\n", __func__);
}
EXPORT_SYMBOL(wcd_mbhc_stop);

/*
 * wcd_mbhc_init : initialize MBHC internal structures.
 *
 * NOTE: mbhc->mbhc_cfg is not YET configure so shouldn't be used
 */
int wcd_mbhc_init(struct wcd_mbhc *mbhc, struct snd_soc_codec *codec,
		      const struct wcd_mbhc_intr *mbhc_cdc_intr_ids,
		      bool impedance_det_en)
{
	int ret;

	pr_debug("%s: enter\n", __func__);

	mbhc->in_swch_irq_handler = false;
	mbhc->current_plug = PLUG_TYPE_NONE;
	mbhc->is_btn_press = false;
	mbhc->codec = codec;
	mbhc->intr_ids = mbhc_cdc_intr_ids;
	mbhc->impedance_detect = impedance_det_en;

	if (mbhc->intr_ids == NULL) {
		pr_err("%s: Interrupt mapping not provided\n", __func__);
		return -EINVAL;
	}

	if (mbhc->headset_jack.jack == NULL) {
		ret = snd_soc_jack_new(codec, "Headset Jack",
				WCD_MBHC_JACK_MASK, &mbhc->headset_jack);
		if (ret) {
			pr_err("%s: Failed to create new jack\n", __func__);
			return ret;
		}

		ret = snd_soc_jack_new(codec, "Button Jack",
				       WCD_MBHC_JACK_BUTTON_MASK,
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

	}

	init_waitqueue_head(&mbhc->wait_btn_press);
	mutex_init(&mbhc->codec_resource_lock);

	ret = wcd9xxx_request_irq(mbhc->intr_ids->mbhc_sw_intr,
				  wcd_mbhc_mech_plug_detect_irq,
				  "mbhc sw intr", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d, ret = %d\n", __func__,
		       mbhc->intr_ids->mbhc_sw_intr, ret);
		goto err_mbhc_sw_irq;
	}

	ret = wcd9xxx_request_irq(mbhc->intr_ids->mbhc_btn_press_intr,
				  wcd_mbhc_btn_press_handler,
				  "Button Press detect",
				  mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       mbhc->intr_ids->mbhc_btn_press_intr);
		goto err_btn_press_irq;
	}

	ret = wcd9xxx_request_irq(mbhc->intr_ids->mbhc_btn_release_intr,
				  wcd_mbhc_release_handler,
				  "Button Release detect", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
			mbhc->intr_ids->mbhc_btn_release_intr);
		goto err_btn_release_irq;
	}

	ret = wcd9xxx_request_irq(mbhc->intr_ids->mbhc_hs_ins_rem_intr,
				  wcd_mbhc_hs_ins_rem_irq,
				  "Elect Insertion/Remove", mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       mbhc->intr_ids->mbhc_hs_ins_rem_intr);
		goto err_mbhc_hs_ins_rem_irq;
	}
	wcd9xxx_disable_irq(mbhc->intr_ids->mbhc_hs_ins_rem_intr);

	ret = wcd9xxx_request_irq(mbhc->intr_ids->hph_left_ocp,
				  wcd_mbhc_hphl_ocp_irq, "HPH_L OCP detect",
				  mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       mbhc->intr_ids->hph_left_ocp);
		goto err_hphl_ocp_irq;
	}
	wcd9xxx_disable_irq(mbhc->intr_ids->hph_left_ocp);

	ret = wcd9xxx_request_irq(mbhc->intr_ids->hph_right_ocp,
				  wcd_mbhc_hphr_ocp_irq, "HPH_R OCP detect",
				  mbhc);
	if (ret) {
		pr_err("%s: Failed to request irq %d\n", __func__,
		       mbhc->intr_ids->hph_right_ocp);
		goto err_hphr_ocp_irq;
	}
	wcd9xxx_disable_irq(mbhc->intr_ids->hph_right_ocp);

	pr_debug("%s: leave ret %d\n", __func__, ret);
	return ret;

err_hphr_ocp_irq:
	wcd9xxx_free_irq(mbhc->intr_ids->hph_left_ocp, mbhc);
err_hphl_ocp_irq:
	wcd9xxx_free_irq(mbhc->intr_ids->mbhc_hs_ins_rem_intr, mbhc);
err_mbhc_hs_ins_rem_irq:
	wcd9xxx_free_irq(mbhc->intr_ids->mbhc_btn_release_intr, mbhc);
err_btn_release_irq:
	wcd9xxx_free_irq(mbhc->intr_ids->mbhc_btn_press_intr, mbhc);
err_btn_press_irq:
	wcd9xxx_free_irq(mbhc->intr_ids->mbhc_sw_intr, mbhc);
err_mbhc_sw_irq:
	mutex_destroy(&mbhc->codec_resource_lock);
	pr_debug("%s: leave ret %d\n", __func__, ret);
	return ret;
}
EXPORT_SYMBOL(wcd_mbhc_init);

void wcd_mbhc_deinit(struct wcd_mbhc *mbhc)
{

	wcd9xxx_free_irq(mbhc->intr_ids->mbhc_sw_intr, mbhc);
	wcd9xxx_free_irq(mbhc->intr_ids->mbhc_btn_press_intr, mbhc);
	wcd9xxx_free_irq(mbhc->intr_ids->mbhc_btn_release_intr, mbhc);
	wcd9xxx_free_irq(mbhc->intr_ids->mbhc_hs_ins_rem_intr, mbhc);
	wcd9xxx_free_irq(mbhc->intr_ids->hph_left_ocp, mbhc);
	wcd9xxx_free_irq(mbhc->intr_ids->hph_right_ocp, mbhc);
	mutex_destroy(&mbhc->codec_resource_lock);
}
EXPORT_SYMBOL(wcd_mbhc_deinit);

MODULE_DESCRIPTION("wcd MBHC v2 module");
MODULE_LICENSE("GPL v2");
