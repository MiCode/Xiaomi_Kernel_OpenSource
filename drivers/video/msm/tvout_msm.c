/* Copyright (c) 2008-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/delay.h>

#include "msm_fb.h"
#include "tvenc.h"
#include "external_common.h"

#define TVOUT_HPD_DUTY_CYCLE 3000

#define TV_DIMENSION_MAX_WIDTH		720
#define TV_DIMENSION_MAX_HEIGHT		576

struct tvout_msm_state_type {
	struct external_common_state_type common;
	struct platform_device *pdev;
	struct timer_list hpd_state_timer;
	struct timer_list hpd_work_timer;
	struct work_struct hpd_work;
	uint32 hpd_int_status;
	uint32 prev_hpd_int_status;
	uint32 five_retry;
	int irq;
	uint16 y_res;
	boolean hpd_initialized;
	boolean disp_powered_up;
#ifdef CONFIG_SUSPEND
	boolean pm_suspended;
#endif

};

static struct tvout_msm_state_type *tvout_msm_state;
static DEFINE_MUTEX(tvout_msm_state_mutex);

static int tvout_off(struct platform_device *pdev);
static int tvout_on(struct platform_device *pdev);
static void tvout_check_status(void);

static void tvout_msm_turn_on(boolean power_on)
{
	uint32 reg_val = 0;
	reg_val = TV_IN(TV_ENC_CTL);
	if (power_on) {
		DEV_DBG("%s: TV Encoder turned on\n", __func__);
		reg_val |= TVENC_CTL_ENC_EN;
	} else {
		DEV_DBG("%s: TV Encoder turned off\n", __func__);
		reg_val = 0;
	}
	/* Enable TV Encoder*/
	TV_OUT(TV_ENC_CTL, reg_val);
}

static void tvout_check_status()
{
	tvout_msm_state->hpd_int_status &= 0x05;
	/* hpd_int_status could either be 0x05 or 0x04 for a cable
		plug-out event when cable detect is driven by polling. */
	if ((((tvout_msm_state->hpd_int_status == 0x05) ||
		(tvout_msm_state->hpd_int_status == 0x04)) &&
		(tvout_msm_state->prev_hpd_int_status == BIT(2))) ||
		((tvout_msm_state->hpd_int_status == 0x01) &&
		(tvout_msm_state->prev_hpd_int_status == BIT(0)))) {
		DEV_DBG("%s: cable event sent already!", __func__);
		return;
	}

	if (tvout_msm_state->hpd_int_status & BIT(2)) {
		DEV_DBG("%s: cable plug-out\n", __func__);
		mutex_lock(&external_common_state_hpd_mutex);
		external_common_state->hpd_state = FALSE;
		mutex_unlock(&external_common_state_hpd_mutex);
		kobject_uevent(external_common_state->uevent_kobj,
				KOBJ_OFFLINE);
		tvout_msm_state->prev_hpd_int_status = BIT(2);
	} else if (tvout_msm_state->hpd_int_status & BIT(0)) {
		DEV_DBG("%s: cable plug-in\n", __func__);
		mutex_lock(&external_common_state_hpd_mutex);
		external_common_state->hpd_state = TRUE;
		mutex_unlock(&external_common_state_hpd_mutex);
		kobject_uevent(external_common_state->uevent_kobj,
				KOBJ_ONLINE);
		tvout_msm_state->prev_hpd_int_status = BIT(0);
	}
}

/* ISR for TV out cable detect */
static irqreturn_t tvout_msm_isr(int irq, void *dev_id)
{
	tvout_msm_state->hpd_int_status = TV_IN(TV_INTR_STATUS);
	TV_OUT(TV_INTR_CLEAR, tvout_msm_state->hpd_int_status);
	DEV_DBG("%s: ISR: 0x%02x\n", __func__,
		tvout_msm_state->hpd_int_status & 0x05);

	if (tvenc_pdata->poll)
		if (!tvout_msm_state || !tvout_msm_state->disp_powered_up) {
			DEV_DBG("%s: ISR ignored, display not yet powered on\n",
				__func__);
			return IRQ_HANDLED;
		}
	if (tvout_msm_state->hpd_int_status & BIT(0) ||
		tvout_msm_state->hpd_int_status & BIT(2)) {
		/* Use .75sec to debounce the interrupt */
		mod_timer(&tvout_msm_state->hpd_state_timer, jiffies
			+ msecs_to_jiffies(750));
	}

	return IRQ_HANDLED;
}

/* Interrupt debounce timer */
static void tvout_msm_hpd_state_timer(unsigned long data)
{
#ifdef CONFIG_SUSPEND
	mutex_lock(&tvout_msm_state_mutex);
	if (tvout_msm_state->pm_suspended) {
		mutex_unlock(&tvout_msm_state_mutex);
		DEV_WARN("%s: ignored, pm_suspended\n", __func__);
		return;
	}
	mutex_unlock(&tvout_msm_state_mutex);
#endif

	if (tvenc_pdata->poll)
		if (!tvout_msm_state || !tvout_msm_state->disp_powered_up) {
			DEV_DBG("%s: ignored, display powered off\n", __func__);
			return;
		}

	/* TV_INTR_STATUS[0x204]
		When a TV_ENC interrupt occurs, then reading this register will
		indicate what caused the interrupt since that each bit indicates
		the source of the interrupt that had happened. If multiple
		interrupt sources had happened, then multiple bits of this
		register will be set
		Bit 0 : Load present on Video1
		Bit 1 : Load present on Video2
		Bit 2 : Load removed on Video1
		Bit 3 : Load removed on Video2
	*/

	/* Locking interrupt status is not required because
	last status read after debouncing is used */
	if ((tvout_msm_state->hpd_int_status & 0x05) == 0x05) {
		/* SW-workaround :If the status read after debouncing is
		0x05(indicating both load present & load removed- which can't
		happen in reality), force an update. If status remains 0x05
		after retry, it's a cable unplug event */
		if (++tvout_msm_state->five_retry < 2) {
			uint32 reg;
			DEV_DBG("tvout: Timer: 0x05\n");
			TV_OUT(TV_INTR_CLEAR, 0xf);
			reg = TV_IN(TV_DAC_INTF);
			TV_OUT(TV_DAC_INTF, reg & ~TVENC_LOAD_DETECT_EN);
			TV_OUT(TV_INTR_CLEAR, 0xf);
			reg = TV_IN(TV_DAC_INTF);
			TV_OUT(TV_DAC_INTF, reg | TVENC_LOAD_DETECT_EN);
			return;
		}
	}
	tvout_msm_state->five_retry = 0;
	tvout_check_status();
}

static void tvout_msm_hpd_work(struct work_struct *work)
{
	uint32 reg;

#ifdef CONFIG_SUSPEND
	mutex_lock(&tvout_msm_state_mutex);
	if (tvout_msm_state->pm_suspended) {
		mutex_unlock(&tvout_msm_state_mutex);
		DEV_WARN("%s: ignored, pm_suspended\n", __func__);
		return;
	}
	mutex_unlock(&tvout_msm_state_mutex);
#endif

	/* Enable power lines & clocks */
	tvenc_pdata->pm_vid_en(1);
	tvenc_set_clock(CLOCK_ON);

	/* Enable encoder to get a stable interrupt */
	reg = TV_IN(TV_ENC_CTL);
	TV_OUT(TV_ENC_CTL, reg | TVENC_CTL_ENC_EN);

	/* SW- workaround to update status register */
	reg = TV_IN(TV_DAC_INTF);
	TV_OUT(TV_DAC_INTF, reg & ~TVENC_LOAD_DETECT_EN);
	TV_OUT(TV_INTR_CLEAR, 0xf);
	reg = TV_IN(TV_DAC_INTF);
	TV_OUT(TV_DAC_INTF, reg | TVENC_LOAD_DETECT_EN);

	tvout_msm_state->hpd_int_status = TV_IN(TV_INTR_STATUS);

	/* Disable TV encoder */
	reg = TV_IN(TV_ENC_CTL);
	TV_OUT(TV_ENC_CTL, reg & ~TVENC_CTL_ENC_EN);

	/*Disable power lines & clocks */
	tvenc_set_clock(CLOCK_OFF);
	tvenc_pdata->pm_vid_en(0);

	DEV_DBG("%s: ISR: 0x%02x\n", __func__,
		tvout_msm_state->hpd_int_status & 0x05);

	mod_timer(&tvout_msm_state->hpd_work_timer, jiffies
		+ msecs_to_jiffies(TVOUT_HPD_DUTY_CYCLE));

	tvout_check_status();
}

static void tvout_msm_hpd_work_timer(unsigned long data)
{
	schedule_work(&tvout_msm_state->hpd_work);
}

static int tvout_on(struct platform_device *pdev)
{
	uint32 reg = 0;
	struct fb_var_screeninfo *var;
	struct msm_fb_data_type *mfd = platform_get_drvdata(pdev);

	if (!mfd)
		return -ENODEV;

	if (mfd->key != MFD_KEY)
		return -EINVAL;

#ifdef CONFIG_SUSPEND
	mutex_lock(&tvout_msm_state_mutex);
	if (tvout_msm_state->pm_suspended) {
		mutex_unlock(&tvout_msm_state_mutex);
		DEV_WARN("%s: ignored, pm_suspended\n", __func__);
		return -ENODEV;
	}
	mutex_unlock(&tvout_msm_state_mutex);
#endif

	var = &mfd->fbi->var;
	if (var->reserved[3] >= NTSC_M && var->reserved[3] <= PAL_N)
		external_common_state->video_resolution = var->reserved[3];

	tvout_msm_state->pdev = pdev;
	if (del_timer(&tvout_msm_state->hpd_work_timer))
		DEV_DBG("%s: work timer stopped\n", __func__);

	TV_OUT(TV_ENC_CTL, 0);	/* disable TV encoder */

	switch (external_common_state->video_resolution) {
	case NTSC_M:
	case NTSC_J:
		TV_OUT(TV_CGMS, 0x0);
		/*  NTSC Timing */
		TV_OUT(TV_SYNC_1, 0x0020009e);
		TV_OUT(TV_SYNC_2, 0x011306B4);
		TV_OUT(TV_SYNC_3, 0x0006000C);
		TV_OUT(TV_SYNC_4, 0x0028020D);
		TV_OUT(TV_SYNC_5, 0x005E02FB);
		TV_OUT(TV_SYNC_6, 0x0006000C);
		TV_OUT(TV_SYNC_7, 0x00000012);
		TV_OUT(TV_BURST_V1, 0x0013020D);
		TV_OUT(TV_BURST_V2, 0x0014020C);
		TV_OUT(TV_BURST_V3, 0x0013020D);
		TV_OUT(TV_BURST_V4, 0x0014020C);
		TV_OUT(TV_BURST_H, 0x00AE00F2);
		TV_OUT(TV_SOL_REQ_ODD, 0x00280208);
		TV_OUT(TV_SOL_REQ_EVEN, 0x00290209);

		reg |= TVENC_CTL_TV_MODE_NTSC_M_PAL60;

		if (external_common_state->video_resolution == NTSC_M) {
			/* Cr gain 11, Cb gain C6, y_gain 97 */
			TV_OUT(TV_GAIN, 0x0081B697);
		} else {
			/* Cr gain 11, Cb gain C6, y_gain 97 */
			TV_OUT(TV_GAIN, 0x008bc4a3);
			reg |= TVENC_CTL_NTSCJ_MODE;
		}

		var->yres = 480;
		break;
	case PAL_BDGHIN:
	case PAL_N:
		/*  PAL Timing */
		TV_OUT(TV_SYNC_1, 0x00180097);
		TV_OUT(TV_SYNC_3, 0x0005000a);
		TV_OUT(TV_SYNC_4, 0x00320271);
		TV_OUT(TV_SYNC_5, 0x005602f9);
		TV_OUT(TV_SYNC_6, 0x0005000a);
		TV_OUT(TV_SYNC_7, 0x0000000f);
		TV_OUT(TV_BURST_V1, 0x0012026e);
		TV_OUT(TV_BURST_V2, 0x0011026d);
		TV_OUT(TV_BURST_V3, 0x00100270);
		TV_OUT(TV_BURST_V4, 0x0013026f);
		TV_OUT(TV_SOL_REQ_ODD, 0x0030026e);
		TV_OUT(TV_SOL_REQ_EVEN, 0x0031026f);

		if (external_common_state->video_resolution == PAL_BDGHIN) {
			/* Cr gain 11, Cb gain C6, y_gain 97 */
			TV_OUT(TV_GAIN, 0x0088c1a0);
			TV_OUT(TV_CGMS, 0x00012345);
			TV_OUT(TV_SYNC_2, 0x011f06c0);
			TV_OUT(TV_BURST_H, 0x00af00ea);
			reg |= TVENC_CTL_TV_MODE_PAL_BDGHIN;
		} else {
			/* Cr gain 11, Cb gain C6, y_gain 97 */
			TV_OUT(TV_GAIN, 0x0081b697);
			TV_OUT(TV_CGMS, 0x000af317);
			TV_OUT(TV_SYNC_2, 0x12006c0);
			TV_OUT(TV_BURST_H, 0x00af00fa);
			reg |= TVENC_CTL_TV_MODE_PAL_N;
		}
		var->yres = 576;
		break;
	case PAL_M:
		/* Cr gain 11, Cb gain C6, y_gain 97 */
		TV_OUT(TV_GAIN, 0x0081b697);
		TV_OUT(TV_CGMS, 0x000af317);
		TV_OUT(TV_TEST_MUX, 0x000001c3);
		TV_OUT(TV_TEST_MODE, 0x00000002);
		/*  PAL Timing */
		TV_OUT(TV_SYNC_1, 0x0020009e);
		TV_OUT(TV_SYNC_2, 0x011306b4);
		TV_OUT(TV_SYNC_3, 0x0006000c);
		TV_OUT(TV_SYNC_4, 0x0028020D);
		TV_OUT(TV_SYNC_5, 0x005e02fb);
		TV_OUT(TV_SYNC_6, 0x0006000c);
		TV_OUT(TV_SYNC_7, 0x00000012);
		TV_OUT(TV_BURST_V1, 0x0012020b);
		TV_OUT(TV_BURST_V2, 0x0016020c);
		TV_OUT(TV_BURST_V3, 0x00150209);
		TV_OUT(TV_BURST_V4, 0x0013020c);
		TV_OUT(TV_BURST_H, 0x00bf010b);
		TV_OUT(TV_SOL_REQ_ODD, 0x00280208);
		TV_OUT(TV_SOL_REQ_EVEN, 0x00290209);

		reg |= TVENC_CTL_TV_MODE_PAL_M;
		var->yres = 480;
		break;
	default:
		return -ENODEV;
	}

	reg |= TVENC_CTL_Y_FILTER_EN | TVENC_CTL_CR_FILTER_EN |
		TVENC_CTL_CB_FILTER_EN | TVENC_CTL_SINX_FILTER_EN;

	/* DC offset to 0. */
	TV_OUT(TV_LEVEL, 0x00000000);
	TV_OUT(TV_OFFSET, 0x008080f0);

#ifdef CONFIG_FB_MSM_TVOUT_SVIDEO
	reg |= TVENC_CTL_S_VIDEO_EN;
#endif
#if defined(CONFIG_FB_MSM_MDP31)
	TV_OUT(TV_DAC_INTF, 0x29);
#endif
	TV_OUT(TV_ENC_CTL, reg);

	if (!tvout_msm_state->hpd_initialized) {
		tvout_msm_state->hpd_initialized = TRUE;
		/* Load detect enable */
		reg = TV_IN(TV_DAC_INTF);
		reg |= TVENC_LOAD_DETECT_EN;
		TV_OUT(TV_DAC_INTF, reg);
	}

	tvout_msm_state->disp_powered_up = TRUE;
	tvout_msm_turn_on(TRUE);

	if (tvenc_pdata->poll) {
		/* Enable Load present & removal interrupts for Video1 */
		TV_OUT(TV_INTR_ENABLE, 0x5);

		/* Enable interrupts when display is on */
		enable_irq(tvout_msm_state->irq);
	}
	return 0;
}

static int tvout_off(struct platform_device *pdev)
{
	/* Disable TV encoder irqs when display is off */
	if (tvenc_pdata->poll)
		disable_irq(tvout_msm_state->irq);
	tvout_msm_turn_on(FALSE);
	tvout_msm_state->hpd_initialized = FALSE;
	tvout_msm_state->disp_powered_up = FALSE;
	if (tvenc_pdata->poll) {
		mod_timer(&tvout_msm_state->hpd_work_timer, jiffies
			+ msecs_to_jiffies(TVOUT_HPD_DUTY_CYCLE));
	}
	return 0;
}

static int __devinit tvout_probe(struct platform_device *pdev)
{
	int rc = 0;
	uint32 reg;
	struct platform_device *fb_dev;

#ifdef CONFIG_FB_MSM_TVOUT_NTSC_M
	external_common_state->video_resolution = NTSC_M;
#elif defined CONFIG_FB_MSM_TVOUT_NTSC_J
	external_common_state->video_resolution = NTSC_J;
#elif defined CONFIG_FB_MSM_TVOUT_PAL_M
	external_common_state->video_resolution = PAL_M;
#elif defined CONFIG_FB_MSM_TVOUT_PAL_N
	external_common_state->video_resolution = PAL_N;
#elif defined CONFIG_FB_MSM_TVOUT_PAL_BDGHIN
	external_common_state->video_resolution = PAL_BDGHIN;
#endif
	external_common_state->dev = &pdev->dev;
	if (pdev->id == 0) {
		struct resource *res;

		#define GET_RES(name, mode) do {			\
			res = platform_get_resource_byname(pdev, mode, name); \
			if (!res) {					\
				DEV_DBG("'" name "' resource not found\n"); \
				rc = -ENODEV;				\
				goto error;				\
			}						\
		} while (0)

		#define GET_IRQ(var, name) do {				\
			GET_RES(name, IORESOURCE_IRQ);			\
			var = res->start;				\
		} while (0)

		GET_IRQ(tvout_msm_state->irq, "tvout_device_irq");
		#undef GET_IRQ
		#undef GET_RES
		return 0;
	}

	DEV_DBG("%s: tvout_msm_state->irq : %d",
			__func__, tvout_msm_state->irq);

	rc = request_irq(tvout_msm_state->irq, &tvout_msm_isr,
		IRQF_TRIGGER_HIGH, "tvout_msm_isr", NULL);

	if (rc) {
		DEV_DBG("Init FAILED: IRQ request, rc=%d\n", rc);
		goto error;
	}
	disable_irq(tvout_msm_state->irq);

	init_timer(&tvout_msm_state->hpd_state_timer);
	tvout_msm_state->hpd_state_timer.function =
		tvout_msm_hpd_state_timer;
	tvout_msm_state->hpd_state_timer.data = (uint32)NULL;
	tvout_msm_state->hpd_state_timer.expires = jiffies
						+ msecs_to_jiffies(1000);

	if (tvenc_pdata->poll) {
		init_timer(&tvout_msm_state->hpd_work_timer);
		tvout_msm_state->hpd_work_timer.function =
			tvout_msm_hpd_work_timer;
		tvout_msm_state->hpd_work_timer.data = (uint32)NULL;
		tvout_msm_state->hpd_work_timer.expires = jiffies
						+ msecs_to_jiffies(1000);
	}
	fb_dev = msm_fb_add_device(pdev);
	if (fb_dev) {
		rc = external_common_state_create(fb_dev);
		if (rc) {
			DEV_ERR("Init FAILED: tvout_msm_state_create, rc=%d\n",
				rc);
			goto error;
		}
		if (tvenc_pdata->poll) {
			/* Start polling timer to detect load */
			mod_timer(&tvout_msm_state->hpd_work_timer, jiffies
				+ msecs_to_jiffies(TVOUT_HPD_DUTY_CYCLE));
		} else {
			/* Enable interrupt to detect load */
			tvenc_set_encoder_clock(CLOCK_ON);
			reg = TV_IN(TV_DAC_INTF);
			reg |= TVENC_LOAD_DETECT_EN;
			TV_OUT(TV_DAC_INTF, reg);
			TV_OUT(TV_INTR_ENABLE, 0x5);
			enable_irq(tvout_msm_state->irq);
		}
	} else
		DEV_ERR("Init FAILED: failed to add fb device\n");
error:
	return 0;
}

static int __devexit tvout_remove(struct platform_device *pdev)
{
	external_common_state_remove();
	kfree(tvout_msm_state);
	tvout_msm_state = NULL;
	return 0;
}

#ifdef CONFIG_SUSPEND
static int tvout_device_pm_suspend(struct device *dev)
{
	mutex_lock(&tvout_msm_state_mutex);
	if (tvout_msm_state->pm_suspended) {
		mutex_unlock(&tvout_msm_state_mutex);
		return 0;
	}
	if (tvenc_pdata->poll) {
		if (del_timer(&tvout_msm_state->hpd_work_timer))
			DEV_DBG("%s: suspending cable detect timer\n",
				__func__);
	} else {
		disable_irq(tvout_msm_state->irq);
		tvenc_set_encoder_clock(CLOCK_OFF);
	}
	tvout_msm_state->pm_suspended = TRUE;
	mutex_unlock(&tvout_msm_state_mutex);
	return 0;
}

static int tvout_device_pm_resume(struct device *dev)
{
	mutex_lock(&tvout_msm_state_mutex);
	if (!tvout_msm_state->pm_suspended) {
		mutex_unlock(&tvout_msm_state_mutex);
		return 0;
	}

	if (tvenc_pdata->poll) {
		tvout_msm_state->pm_suspended = FALSE;
		mod_timer(&tvout_msm_state->hpd_work_timer, jiffies
				+ msecs_to_jiffies(TVOUT_HPD_DUTY_CYCLE));
		mutex_unlock(&tvout_msm_state_mutex);
		DEV_DBG("%s: resuming cable detect timer\n", __func__);
	} else {
		tvenc_set_encoder_clock(CLOCK_ON);
		tvout_msm_state->pm_suspended = FALSE;
		mutex_unlock(&tvout_msm_state_mutex);
		enable_irq(tvout_msm_state->irq);
		DEV_DBG("%s: enable cable detect interrupt\n", __func__);
	}
	return 0;
}
#else
#define tvout_device_pm_suspend	NULL
#define tvout_device_pm_resume		NULL
#endif


static const struct dev_pm_ops tvout_device_pm_ops = {
	.suspend = tvout_device_pm_suspend,
	.resume = tvout_device_pm_resume,
};

static struct platform_driver this_driver = {
	.probe  = tvout_probe,
	.remove = tvout_remove,
	.driver = {
		.name	= "tvout_device",
		.pm	= &tvout_device_pm_ops,
	},
};

static struct msm_fb_panel_data tvout_panel_data = {
	.panel_info.xres = TV_DIMENSION_MAX_WIDTH,
	.panel_info.yres = TV_DIMENSION_MAX_HEIGHT,
	.panel_info.type = TV_PANEL,
	.panel_info.pdest = DISPLAY_2,
	.panel_info.wait_cycle = 0,
#ifdef CONFIG_FB_MSM_MDP40
	.panel_info.bpp = 24,
#else
	.panel_info.bpp = 16,
#endif
	.panel_info.fb_num = 2,
	.on = tvout_on,
	.off = tvout_off,
};

static struct platform_device this_device = {
	.name   = "tvout_device",
	.id = 1,
	.dev	= {
		.platform_data = &tvout_panel_data,
	}
};

static int __init tvout_init(void)
{
	int ret;
	tvout_msm_state = kzalloc(sizeof(*tvout_msm_state), GFP_KERNEL);
	if (!tvout_msm_state) {
		DEV_ERR("tvout_msm_init FAILED: out of memory\n");
		ret = -ENOMEM;
		goto init_exit;
	}

	external_common_state = &tvout_msm_state->common;
	ret = platform_driver_register(&this_driver);
	if (ret) {
		DEV_ERR("tvout_device_init FAILED: platform_driver_register\
			rc=%d\n", ret);
		goto init_exit;
	}

	ret = platform_device_register(&this_device);
	if (ret) {
		DEV_ERR("tvout_device_init FAILED: platform_driver_register\
			rc=%d\n", ret);
		platform_driver_unregister(&this_driver);
		goto init_exit;
	}

	INIT_WORK(&tvout_msm_state->hpd_work, tvout_msm_hpd_work);
	return 0;

init_exit:
	kfree(tvout_msm_state);
	tvout_msm_state = NULL;
	return ret;
}

static void __exit tvout_exit(void)
{
	platform_device_unregister(&this_device);
	platform_driver_unregister(&this_driver);
}

module_init(tvout_init);
module_exit(tvout_exit);

MODULE_LICENSE("GPL v2");
MODULE_VERSION("1.0");
MODULE_AUTHOR("Qualcomm Innovation Center, Inc.");
MODULE_DESCRIPTION("TV out driver");
