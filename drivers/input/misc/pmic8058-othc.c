/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/interrupt.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/switch.h>
#include <linux/pm.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/hrtimer.h>
#include <linux/delay.h>
#include <linux/regulator/consumer.h>

#include <linux/mfd/pm8xxx/core.h>
#include <linux/pmic8058-othc.h>
#include <linux/msm_adc.h>

#define PM8058_OTHC_LOW_CURR_MASK	0xF0
#define PM8058_OTHC_HIGH_CURR_MASK	0x0F
#define PM8058_OTHC_EN_SIG_MASK		0x3F
#define PM8058_OTHC_HYST_PREDIV_MASK	0xC7
#define PM8058_OTHC_CLK_PREDIV_MASK	0xF8
#define PM8058_OTHC_HYST_CLK_MASK	0x0F
#define PM8058_OTHC_PERIOD_CLK_MASK	0xF0

#define PM8058_OTHC_LOW_CURR_SHIFT	0x4
#define PM8058_OTHC_EN_SIG_SHIFT	0x6
#define PM8058_OTHC_HYST_PREDIV_SHIFT	0x3
#define PM8058_OTHC_HYST_CLK_SHIFT	0x4

#define OTHC_GPIO_MAX_LEN		25

struct pm8058_othc {
	bool othc_sw_state;
	bool switch_reject;
	bool othc_support_n_switch;
	bool accessory_support;
	bool accessories_adc_support;
	int othc_base;
	int othc_irq_sw;
	int othc_irq_ir;
	int othc_ir_state;
	int num_accessories;
	int curr_accessory_code;
	int curr_accessory;
	int video_out_gpio;
	u32 sw_key_code;
	u32 accessories_adc_channel;
	int ir_gpio;
	unsigned long switch_debounce_ms;
	unsigned long detection_delay_ms;
	void *adc_handle;
	void *accessory_adc_handle;
	spinlock_t lock;
	struct device *dev;
	struct regulator *othc_vreg;
	struct input_dev *othc_ipd;
	struct switch_dev othc_sdev;
	struct pmic8058_othc_config_pdata *othc_pdata;
	struct othc_accessory_info *accessory_info;
	struct hrtimer timer;
	struct othc_n_switch_config *switch_config;
	struct work_struct switch_work;
	struct delayed_work detect_work;
	struct delayed_work hs_work;
};

static struct pm8058_othc *config[OTHC_MICBIAS_MAX];

static void hs_worker(struct work_struct *work)
{
	int rc;
	struct pm8058_othc *dd =
		container_of(work, struct pm8058_othc, hs_work.work);

	rc = gpio_get_value_cansleep(dd->ir_gpio);
	if (rc < 0) {
		pr_err("Unable to read IR GPIO\n");
		enable_irq(dd->othc_irq_ir);
		return;
	}

	dd->othc_ir_state = !rc;
	schedule_delayed_work(&dd->detect_work,
				msecs_to_jiffies(dd->detection_delay_ms));
}

static irqreturn_t ir_gpio_irq(int irq, void *dev_id)
{
	unsigned long flags;
	struct pm8058_othc *dd = dev_id;

	spin_lock_irqsave(&dd->lock, flags);
	/* Enable the switch reject flag */
	dd->switch_reject = true;
	spin_unlock_irqrestore(&dd->lock, flags);

	/* Start the HR timer if one is not active */
	if (hrtimer_active(&dd->timer))
		hrtimer_cancel(&dd->timer);

	hrtimer_start(&dd->timer,
		ktime_set((dd->switch_debounce_ms / 1000),
		(dd->switch_debounce_ms % 1000) * 1000000), HRTIMER_MODE_REL);

	/* disable irq, this gets enabled in the workqueue */
	disable_irq_nosync(dd->othc_irq_ir);
	schedule_delayed_work(&dd->hs_work, 0);

	return IRQ_HANDLED;
}
/*
 * The API pm8058_micbias_enable() allows to configure
 * the MIC_BIAS. Only the lines which are not used for
 * headset detection can be configured using this API.
 * The API returns an error code if it fails to configure
 * the specified MIC_BIAS line, else it returns 0.
 */
int pm8058_micbias_enable(enum othc_micbias micbias,
		enum othc_micbias_enable enable)
{
	int rc;
	u8 reg;
	struct pm8058_othc *dd = config[micbias];

	if (dd == NULL) {
		pr_err("MIC_BIAS not registered, cannot enable\n");
		return -ENODEV;
	}

	if (dd->othc_pdata->micbias_capability != OTHC_MICBIAS) {
		pr_err("MIC_BIAS enable capability not supported\n");
		return -EINVAL;
	}

	rc = pm8xxx_readb(dd->dev->parent, dd->othc_base + 1, &reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	reg &= PM8058_OTHC_EN_SIG_MASK;
	reg |= (enable << PM8058_OTHC_EN_SIG_SHIFT);

	rc = pm8xxx_writeb(dd->dev->parent, dd->othc_base + 1, reg);
	if (rc < 0) {
		pr_err("PM8058 write failed\n");
		return rc;
	}

	return rc;
}
EXPORT_SYMBOL(pm8058_micbias_enable);

int pm8058_othc_svideo_enable(enum othc_micbias micbias, bool enable)
{
	struct pm8058_othc *dd = config[micbias];

	if (dd == NULL) {
		pr_err("MIC_BIAS not registered, cannot enable\n");
		return -ENODEV;
	}

	if (dd->othc_pdata->micbias_capability != OTHC_MICBIAS_HSED) {
		pr_err("MIC_BIAS enable capability not supported\n");
		return -EINVAL;
	}

	if (dd->accessories_adc_support) {
		/* GPIO state for MIC_IN = 0, SVIDEO = 1 */
		gpio_set_value_cansleep(dd->video_out_gpio, !!enable);
		if (enable) {
			pr_debug("Enable the video path\n");
			switch_set_state(&dd->othc_sdev, dd->curr_accessory);
			input_report_switch(dd->othc_ipd,
						dd->curr_accessory_code, 1);
			input_sync(dd->othc_ipd);
		} else {
			pr_debug("Disable the video path\n");
			switch_set_state(&dd->othc_sdev, 0);
			input_report_switch(dd->othc_ipd,
					dd->curr_accessory_code, 0);
			input_sync(dd->othc_ipd);
		}
	}

	return 0;
}
EXPORT_SYMBOL(pm8058_othc_svideo_enable);

#ifdef CONFIG_PM
static int pm8058_othc_suspend(struct device *dev)
{
	int rc = 0;
	struct pm8058_othc *dd = dev_get_drvdata(dev);

	if (dd->othc_pdata->micbias_capability == OTHC_MICBIAS_HSED) {
		if (device_may_wakeup(dev)) {
			enable_irq_wake(dd->othc_irq_sw);
			enable_irq_wake(dd->othc_irq_ir);
		}
	}

	if (!device_may_wakeup(dev)) {
		rc = regulator_disable(dd->othc_vreg);
		if (rc)
			pr_err("othc micbais power off failed\n");
	}

	return rc;
}

static int pm8058_othc_resume(struct device *dev)
{
	int rc = 0;
	struct pm8058_othc *dd = dev_get_drvdata(dev);

	if (dd->othc_pdata->micbias_capability == OTHC_MICBIAS_HSED) {
		if (device_may_wakeup(dev)) {
			disable_irq_wake(dd->othc_irq_sw);
			disable_irq_wake(dd->othc_irq_ir);
		}
	}

	if (!device_may_wakeup(dev)) {
		rc = regulator_enable(dd->othc_vreg);
		if (rc)
			pr_err("othc micbais power on failed\n");
	}

	return rc;
}

static struct dev_pm_ops pm8058_othc_pm_ops = {
	.suspend = pm8058_othc_suspend,
	.resume = pm8058_othc_resume,
};
#endif

static int __devexit pm8058_othc_remove(struct platform_device *pd)
{
	struct pm8058_othc *dd = platform_get_drvdata(pd);

	pm_runtime_set_suspended(&pd->dev);
	pm_runtime_disable(&pd->dev);

	if (dd->othc_pdata->micbias_capability == OTHC_MICBIAS_HSED) {
		device_init_wakeup(&pd->dev, 0);
		if (dd->othc_support_n_switch == true) {
			adc_channel_close(dd->adc_handle);
			cancel_work_sync(&dd->switch_work);
		}

		if (dd->accessory_support == true) {
			int i;
			for (i = 0; i < dd->num_accessories; i++) {
				if (dd->accessory_info[i].detect_flags &
							OTHC_GPIO_DETECT)
					gpio_free(dd->accessory_info[i].gpio);
			}
		}
		cancel_delayed_work_sync(&dd->detect_work);
		cancel_delayed_work_sync(&dd->hs_work);
		free_irq(dd->othc_irq_sw, dd);
		free_irq(dd->othc_irq_ir, dd);
		if (dd->ir_gpio != -1)
			gpio_free(dd->ir_gpio);
		input_unregister_device(dd->othc_ipd);
	}
	regulator_disable(dd->othc_vreg);
	regulator_put(dd->othc_vreg);

	kfree(dd);

	return 0;
}

static enum hrtimer_restart pm8058_othc_timer(struct hrtimer *timer)
{
	unsigned long flags;
	struct pm8058_othc *dd = container_of(timer,
					struct pm8058_othc, timer);

	spin_lock_irqsave(&dd->lock, flags);
	dd->switch_reject = false;
	spin_unlock_irqrestore(&dd->lock, flags);

	return HRTIMER_NORESTART;
}

static void othc_report_switch(struct pm8058_othc *dd, u32 res)
{
	u8 i;
	struct othc_switch_info *sw_info = dd->switch_config->switch_info;

	for (i = 0; i < dd->switch_config->num_keys; i++) {
		if (res >= sw_info[i].min_adc_threshold &&
				res <= sw_info[i].max_adc_threshold) {
			dd->othc_sw_state = true;
			dd->sw_key_code = sw_info[i].key_code;
			input_report_key(dd->othc_ipd, sw_info[i].key_code, 1);
			input_sync(dd->othc_ipd);
			return;
		}
	}

	/*
	 * If the switch is not present in a specified ADC range
	 * report a default switch press.
	 */
	if (dd->switch_config->default_sw_en) {
		dd->othc_sw_state = true;
		dd->sw_key_code =
			sw_info[dd->switch_config->default_sw_idx].key_code;
		input_report_key(dd->othc_ipd, dd->sw_key_code, 1);
		input_sync(dd->othc_ipd);
	}
}

static void switch_work_f(struct work_struct *work)
{
	int rc, i;
	u32 res = 0;
	struct adc_chan_result adc_result;
	struct pm8058_othc *dd =
		container_of(work, struct pm8058_othc, switch_work);
	DECLARE_COMPLETION_ONSTACK(adc_wait);
	u8 num_adc_samples = dd->switch_config->num_adc_samples;

	/* sleep for settling time */
	msleep(dd->switch_config->voltage_settling_time_ms);

	for (i = 0; i < num_adc_samples; i++) {
		rc = adc_channel_request_conv(dd->adc_handle, &adc_wait);
		if (rc) {
			pr_err("adc_channel_request_conv failed\n");
			goto bail_out;
		}
		rc = wait_for_completion_interruptible(&adc_wait);
		if (rc) {
			pr_err("wait_for_completion_interruptible failed\n");
			goto bail_out;
		}
		rc = adc_channel_read_result(dd->adc_handle, &adc_result);
		if (rc) {
			pr_err("adc_channel_read_result failed\n");
			goto bail_out;
		}
		res += adc_result.physical;
	}
bail_out:
	if (i == num_adc_samples && num_adc_samples != 0) {
		res /= num_adc_samples;
		othc_report_switch(dd, res);
	} else
		pr_err("Insufficient ADC samples\n");

	enable_irq(dd->othc_irq_sw);
}

static int accessory_adc_detect(struct pm8058_othc *dd, int accessory)
{
	int rc;
	u32 res;
	struct adc_chan_result accessory_adc_result;
	DECLARE_COMPLETION_ONSTACK(accessory_adc_wait);

	rc = adc_channel_request_conv(dd->accessory_adc_handle,
						&accessory_adc_wait);
	if (rc) {
		pr_err("adc_channel_request_conv failed\n");
		goto adc_failed;
	}
	rc = wait_for_completion_interruptible(&accessory_adc_wait);
	if (rc) {
		pr_err("wait_for_completion_interruptible failed\n");
		goto adc_failed;
	}
	rc = adc_channel_read_result(dd->accessory_adc_handle,
						&accessory_adc_result);
	if (rc) {
		pr_err("adc_channel_read_result failed\n");
		goto adc_failed;
	}

	res = accessory_adc_result.physical;

	if (res >= dd->accessory_info[accessory].adc_thres.min_threshold &&
		res <= dd->accessory_info[accessory].adc_thres.max_threshold) {
		pr_debug("Accessory on ADC detected!, ADC Value = %u\n", res);
		return 1;
	}

adc_failed:
	return 0;
}


static int pm8058_accessory_report(struct pm8058_othc *dd, int status)
{
	int i, rc, detected = 0;
	u8 micbias_status, switch_status;

	if (dd->accessory_support == false) {
		/* Report default headset */
		switch_set_state(&dd->othc_sdev, !!status);
		input_report_switch(dd->othc_ipd, SW_HEADPHONE_INSERT,
							!!status);
		input_sync(dd->othc_ipd);
		return 0;
	}

	/* For accessory */
	if (dd->accessory_support == true && status == 0) {
		/* Report removal of the accessory. */

		/*
		 * If the current accessory is video cable, reject the removal
		 * interrupt.
		 */
		pr_info("Accessory [%d] removed\n", dd->curr_accessory);
		if (dd->curr_accessory == OTHC_SVIDEO_OUT)
			return 0;

		switch_set_state(&dd->othc_sdev, 0);
		input_report_switch(dd->othc_ipd, dd->curr_accessory_code, 0);
		input_sync(dd->othc_ipd);
		return 0;
	}

	if (dd->ir_gpio < 0) {
		/* Check the MIC_BIAS status */
		rc = pm8xxx_read_irq_stat(dd->dev->parent, dd->othc_irq_ir);
		if (rc < 0) {
			pr_err("Unable to read IR status from PMIC\n");
			goto fail_ir_accessory;
		}
		micbias_status = !!rc;
	} else {
		rc = gpio_get_value_cansleep(dd->ir_gpio);
		if (rc < 0) {
			pr_err("Unable to read IR status from GPIO\n");
			goto fail_ir_accessory;
		}
		micbias_status = !rc;
	}

	/* Check the switch status */
	rc = pm8xxx_read_irq_stat(dd->dev->parent, dd->othc_irq_sw);
	if (rc < 0) {
		pr_err("Unable to read SWITCH status\n");
		goto fail_ir_accessory;
	}
	switch_status = !!rc;

	/* Loop through to check which accessory is connected */
	for (i = 0; i < dd->num_accessories; i++) {
		detected = 0;
		if (dd->accessory_info[i].enabled == false)
			continue;

		if (dd->accessory_info[i].detect_flags & OTHC_MICBIAS_DETECT) {
			if (micbias_status)
				detected = 1;
			else
				continue;
		}
		if (dd->accessory_info[i].detect_flags & OTHC_SWITCH_DETECT) {
			if (switch_status)
				detected = 1;
			else
				continue;
		}
		if (dd->accessory_info[i].detect_flags & OTHC_GPIO_DETECT) {
			rc = gpio_get_value_cansleep(
						dd->accessory_info[i].gpio);
			if (rc < 0)
				continue;

			if (rc ^ dd->accessory_info[i].active_low)
				detected = 1;
			else
				continue;
		}
		if (dd->accessory_info[i].detect_flags & OTHC_ADC_DETECT)
			detected = accessory_adc_detect(dd, i);

		if (detected)
			break;
	}

	if (detected) {
		dd->curr_accessory = dd->accessory_info[i].accessory;
		dd->curr_accessory_code = dd->accessory_info[i].key_code;

		/* if Video out cable detected enable the video path*/
		if (dd->curr_accessory == OTHC_SVIDEO_OUT) {
			pm8058_othc_svideo_enable(
					dd->othc_pdata->micbias_select, true);

		} else {
			switch_set_state(&dd->othc_sdev, dd->curr_accessory);
			input_report_switch(dd->othc_ipd,
						dd->curr_accessory_code, 1);
			input_sync(dd->othc_ipd);
		}
		pr_info("Accessory [%d] inserted\n", dd->curr_accessory);
	} else
		pr_info("Unable to detect accessory. False interrupt!\n");

	return 0;

fail_ir_accessory:
	return rc;
}

static void detect_work_f(struct work_struct *work)
{
	int rc;
	struct pm8058_othc *dd =
		container_of(work, struct pm8058_othc, detect_work.work);

	/* Accessory has been inserted */
	rc = pm8058_accessory_report(dd, 1);
	if (rc)
		pr_err("Accessory insertion could not be detected\n");

	enable_irq(dd->othc_irq_ir);
}

/*
 * The pm8058_no_sw detects the switch press and release operation.
 * The odd number call is press and even number call is release.
 * The current state of the button is maintained in othc_sw_state variable.
 * This isr gets called only for NO type headsets.
 */
static irqreturn_t pm8058_no_sw(int irq, void *dev_id)
{
	int level;
	struct pm8058_othc *dd = dev_id;
	unsigned long flags;

	/* Check if headset has been inserted, else return */
	if (!dd->othc_ir_state)
		return IRQ_HANDLED;

	spin_lock_irqsave(&dd->lock, flags);
	if (dd->switch_reject == true) {
		pr_debug("Rejected switch interrupt\n");
		spin_unlock_irqrestore(&dd->lock, flags);
		return IRQ_HANDLED;
	}
	spin_unlock_irqrestore(&dd->lock, flags);

	level = pm8xxx_read_irq_stat(dd->dev->parent, dd->othc_irq_sw);
	if (level < 0) {
		pr_err("Unable to read IRQ status register\n");
		return IRQ_HANDLED;
	}

	if (dd->othc_support_n_switch == true) {
		if (level == 0) {
			dd->othc_sw_state = false;
			input_report_key(dd->othc_ipd, dd->sw_key_code, 0);
			input_sync(dd->othc_ipd);
		} else {
			disable_irq_nosync(dd->othc_irq_sw);
			schedule_work(&dd->switch_work);
		}
		return IRQ_HANDLED;
	}
	/*
	 * It is necessary to check the software state and the hardware state
	 * to make sure that the residual interrupt after the debounce time does
	 * not disturb the software state machine.
	 */
	if (level == 1 && dd->othc_sw_state == false) {
		/*  Switch has been pressed */
		dd->othc_sw_state = true;
		input_report_key(dd->othc_ipd, KEY_MEDIA, 1);
	} else if (level == 0 && dd->othc_sw_state == true) {
		/* Switch has been released */
		dd->othc_sw_state = false;
		input_report_key(dd->othc_ipd, KEY_MEDIA, 0);
	}
	input_sync(dd->othc_ipd);

	return IRQ_HANDLED;
}

/*
 * The pm8058_nc_ir detects insert / remove of the headset (for NO),
 * The current state of the headset is maintained in othc_ir_state variable.
 * Due to a hardware bug, false switch interrupts are seen during headset
 * insert. This is handled in the software by rejecting the switch interrupts
 * for a small period of time after the headset has been inserted.
 */
static irqreturn_t pm8058_nc_ir(int irq, void *dev_id)
{
	unsigned long flags, rc;
	struct pm8058_othc *dd = dev_id;

	spin_lock_irqsave(&dd->lock, flags);
	/* Enable the switch reject flag */
	dd->switch_reject = true;
	spin_unlock_irqrestore(&dd->lock, flags);

	/* Start the HR timer if one is not active */
	if (hrtimer_active(&dd->timer))
		hrtimer_cancel(&dd->timer);

	hrtimer_start(&dd->timer,
		ktime_set((dd->switch_debounce_ms / 1000),
		(dd->switch_debounce_ms % 1000) * 1000000), HRTIMER_MODE_REL);


	/* Check the MIC_BIAS status, to check if inserted or removed */
	rc = pm8xxx_read_irq_stat(dd->dev->parent, dd->othc_irq_ir);
	if (rc < 0) {
		pr_err("Unable to read IR status\n");
		goto fail_ir;
	}

	dd->othc_ir_state = rc;
	if (dd->othc_ir_state) {
		/* disable irq, this gets enabled in the workqueue */
		disable_irq_nosync(dd->othc_irq_ir);
		/* Accessory has been inserted, report with detection delay */
		schedule_delayed_work(&dd->detect_work,
				msecs_to_jiffies(dd->detection_delay_ms));
	} else {
		/* Accessory has been removed, report removal immediately */
		rc = pm8058_accessory_report(dd, 0);
		if (rc)
			pr_err("Accessory removal could not be detected\n");
		/* Clear existing switch state */
		dd->othc_sw_state = false;
	}

fail_ir:
	return IRQ_HANDLED;
}

static int pm8058_configure_micbias(struct pm8058_othc *dd)
{
	int rc;
	u8 reg, value;
	u32 value1;
	u16 base_addr = dd->othc_base;
	struct hsed_bias_config *hsed_config =
			dd->othc_pdata->hsed_config->hsed_bias_config;

	/* Intialize the OTHC module */
	/* Control Register 1*/
	rc = pm8xxx_readb(dd->dev->parent, base_addr, &reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	/* set iDAC high current threshold */
	value = (hsed_config->othc_highcurr_thresh_uA / 100) - 2;
	reg =  (reg & PM8058_OTHC_HIGH_CURR_MASK) | value;

	rc = pm8xxx_writeb(dd->dev->parent, base_addr, reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	/* Control register 2*/
	rc = pm8xxx_readb(dd->dev->parent, base_addr + 1, &reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	value = dd->othc_pdata->micbias_enable;
	reg &= PM8058_OTHC_EN_SIG_MASK;
	reg |= (value << PM8058_OTHC_EN_SIG_SHIFT);

	value = 0;
	value1 = (hsed_config->othc_hyst_prediv_us << 10) / USEC_PER_SEC;
	while (value1 != 0) {
		value1 = value1 >> 1;
		value++;
	}
	if (value > 7) {
		pr_err("Invalid input argument - othc_hyst_prediv_us\n");
		return -EINVAL;
	}
	reg &= PM8058_OTHC_HYST_PREDIV_MASK;
	reg |= (value << PM8058_OTHC_HYST_PREDIV_SHIFT);

	value = 0;
	value1 = (hsed_config->othc_period_clkdiv_us << 10) / USEC_PER_SEC;
	while (value1 != 1) {
		value1 = value1 >> 1;
		value++;
	}
	if (value > 8) {
		pr_err("Invalid input argument - othc_period_clkdiv_us\n");
		return -EINVAL;
	}
	reg = (reg &  PM8058_OTHC_CLK_PREDIV_MASK) | (value - 1);

	rc = pm8xxx_writeb(dd->dev->parent, base_addr + 1, reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	/* Control register 3 */
	rc = pm8xxx_readb(dd->dev->parent, base_addr + 2 , &reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	value = hsed_config->othc_hyst_clk_us /
					hsed_config->othc_hyst_prediv_us;
	if (value > 15) {
		pr_err("Invalid input argument - othc_hyst_prediv_us\n");
		return -EINVAL;
	}
	reg &= PM8058_OTHC_HYST_CLK_MASK;
	reg |= value << PM8058_OTHC_HYST_CLK_SHIFT;

	value = hsed_config->othc_period_clk_us /
					hsed_config->othc_period_clkdiv_us;
	if (value > 15) {
		pr_err("Invalid input argument - othc_hyst_prediv_us\n");
		return -EINVAL;
	}
	reg = (reg & PM8058_OTHC_PERIOD_CLK_MASK) | value;

	rc = pm8xxx_writeb(dd->dev->parent, base_addr + 2, reg);
	if (rc < 0) {
		pr_err("PM8058 read failed\n");
		return rc;
	}

	return 0;
}

static ssize_t othc_headset_print_name(struct switch_dev *sdev, char *buf)
{
	switch (switch_get_state(sdev)) {
	case OTHC_NO_DEVICE:
		return sprintf(buf, "No Device\n");
	case OTHC_HEADSET:
	case OTHC_HEADPHONE:
	case OTHC_MICROPHONE:
	case OTHC_ANC_HEADSET:
	case OTHC_ANC_HEADPHONE:
	case OTHC_ANC_MICROPHONE:
		return sprintf(buf, "Headset\n");
	}
	return -EINVAL;
}

static int pm8058_configure_switch(struct pm8058_othc *dd)
{
	int rc, i;

	if (dd->othc_support_n_switch == true) {
		/* n-switch support */
		rc = adc_channel_open(dd->switch_config->adc_channel,
							&dd->adc_handle);
		if (rc) {
			pr_err("Unable to open ADC channel\n");
			return -ENODEV;
		}

		for (i = 0; i < dd->switch_config->num_keys; i++) {
			input_set_capability(dd->othc_ipd, EV_KEY,
				dd->switch_config->switch_info[i].key_code);
		}
	} else /* Only single switch supported */
		input_set_capability(dd->othc_ipd, EV_KEY, KEY_MEDIA);

	return 0;
}

static int
pm8058_configure_accessory(struct pm8058_othc *dd)
{
	int i, rc;
	char name[OTHC_GPIO_MAX_LEN];

	/*
	 * Not bailing out if the gpio_* configure calls fail. This is required
	 * as multiple accessories are detected by the same gpio.
	 */
	for (i = 0; i < dd->num_accessories; i++) {
		if (dd->accessory_info[i].enabled == false)
			continue;
		if (dd->accessory_info[i].detect_flags & OTHC_GPIO_DETECT) {
			snprintf(name, OTHC_GPIO_MAX_LEN, "%s%d",
							"othc_acc_gpio_", i);
			rc = gpio_request(dd->accessory_info[i].gpio, name);
			if (rc) {
				pr_debug("Unable to request GPIO [%d]\n",
						dd->accessory_info[i].gpio);
				continue;
			}
			rc = gpio_direction_input(dd->accessory_info[i].gpio);
			if (rc) {
				pr_debug("Unable to set-direction GPIO [%d]\n",
						dd->accessory_info[i].gpio);
				gpio_free(dd->accessory_info[i].gpio);
				continue;
			}
		}
		input_set_capability(dd->othc_ipd, EV_SW,
					dd->accessory_info[i].key_code);
	}

	if (dd->accessories_adc_support) {
		/*
		 * Check if 3 switch is supported. If both are using the same
		 * ADC channel, the same handle can be used.
		 */
		if (dd->othc_support_n_switch) {
			if (dd->adc_handle != NULL &&
				(dd->accessories_adc_channel ==
				 dd->switch_config->adc_channel))
				dd->accessory_adc_handle = dd->adc_handle;
		} else {
			rc = adc_channel_open(dd->accessories_adc_channel,
						&dd->accessory_adc_handle);
			if (rc) {
				pr_err("Unable to open ADC channel\n");
				rc = -ENODEV;
				goto accessory_adc_fail;
			}
		}
		if (dd->video_out_gpio != 0) {
			rc = gpio_request(dd->video_out_gpio, "vout_enable");
			if (rc < 0) {
				pr_err("request VOUT gpio failed (%d)\n", rc);
				goto accessory_adc_fail;
			}
			rc = gpio_direction_output(dd->video_out_gpio, 0);
			if (rc < 0) {
				pr_err("direction_out failed (%d)\n", rc);
				goto accessory_adc_fail;
			}
		}

	}

	return 0;

accessory_adc_fail:
	for (i = 0; i < dd->num_accessories; i++) {
		if (dd->accessory_info[i].enabled == false)
			continue;
		gpio_free(dd->accessory_info[i].gpio);
	}
	return rc;
}

static int
othc_configure_hsed(struct pm8058_othc *dd, struct platform_device *pd)
{
	int rc;
	struct input_dev *ipd;
	struct pmic8058_othc_config_pdata *pdata = pd->dev.platform_data;
	struct othc_hsed_config *hsed_config = pdata->hsed_config;

	dd->othc_sdev.name = "h2w";
	dd->othc_sdev.print_name = othc_headset_print_name;

	rc = switch_dev_register(&dd->othc_sdev);
	if (rc) {
		pr_err("Unable to register switch device\n");
		return rc;
	}

	ipd = input_allocate_device();
	if (ipd == NULL) {
		pr_err("Unable to allocate memory\n");
		rc = -ENOMEM;
		goto fail_input_alloc;
	}

	/* Get the IRQ for Headset Insert-remove and Switch-press */
	dd->othc_irq_sw = platform_get_irq(pd, 0);
	dd->othc_irq_ir = platform_get_irq(pd, 1);
	if (dd->othc_irq_ir < 0 || dd->othc_irq_sw < 0) {
		pr_err("othc resource:IRQs absent\n");
		rc = -ENXIO;
		goto fail_micbias_config;
	}

	if (pdata->hsed_name != NULL)
		ipd->name = pdata->hsed_name;
	else
		ipd->name = "pmic8058_othc";

	ipd->phys = "pmic8058_othc/input0";
	ipd->dev.parent = &pd->dev;

	dd->othc_ipd = ipd;
	dd->ir_gpio = hsed_config->ir_gpio;
	dd->othc_sw_state = false;
	dd->switch_debounce_ms = hsed_config->switch_debounce_ms;
	dd->othc_support_n_switch = hsed_config->othc_support_n_switch;
	dd->accessory_support = pdata->hsed_config->accessories_support;
	dd->detection_delay_ms = pdata->hsed_config->detection_delay_ms;

	if (dd->othc_support_n_switch == true)
		dd->switch_config = hsed_config->switch_config;

	if (dd->accessory_support == true) {
		dd->accessory_info = pdata->hsed_config->accessories;
		dd->num_accessories = pdata->hsed_config->othc_num_accessories;
		dd->accessories_adc_support =
				pdata->hsed_config->accessories_adc_support;
		dd->accessories_adc_channel =
				pdata->hsed_config->accessories_adc_channel;
		dd->video_out_gpio = pdata->hsed_config->video_out_gpio;
	}

	/* Configure the MIC_BIAS line for headset detection */
	rc = pm8058_configure_micbias(dd);
	if (rc < 0)
		goto fail_micbias_config;

	/* Configure for the switch events */
	rc = pm8058_configure_switch(dd);
	if (rc < 0)
		goto fail_micbias_config;

	/* Configure the accessory */
	if (dd->accessory_support == true) {
		rc = pm8058_configure_accessory(dd);
		if (rc < 0)
			goto fail_micbias_config;
	}

	input_set_drvdata(ipd, dd);
	spin_lock_init(&dd->lock);

	rc = input_register_device(ipd);
	if (rc) {
		pr_err("Unable to register OTHC device\n");
		goto fail_micbias_config;
	}

	hrtimer_init(&dd->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	dd->timer.function = pm8058_othc_timer;

	/* Request the HEADSET IR interrupt */
	if (dd->ir_gpio < 0) {
		rc = request_threaded_irq(dd->othc_irq_ir, NULL, pm8058_nc_ir,
		IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_DISABLED,
					"pm8058_othc_ir", dd);
		if (rc < 0) {
			pr_err("Unable to request pm8058_othc_ir IRQ\n");
			goto fail_ir_irq;
		}
	} else {
		rc = gpio_request(dd->ir_gpio, "othc_ir_gpio");
		if (rc) {
			pr_err("Unable to request IR GPIO\n");
			goto fail_ir_gpio_req;
		}
		rc = gpio_direction_input(dd->ir_gpio);
		if (rc) {
			pr_err("GPIO %d set_direction failed\n", dd->ir_gpio);
			goto fail_ir_irq;
		}
		dd->othc_irq_ir = gpio_to_irq(dd->ir_gpio);
		rc = request_any_context_irq(dd->othc_irq_ir, ir_gpio_irq,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING,
				"othc_gpio_ir_irq", dd);
		if (rc < 0) {
			pr_err("could not request hs irq err=%d\n", rc);
			goto fail_ir_irq;
		}
	}
	/* Request the  SWITCH press/release interrupt */
	rc = request_threaded_irq(dd->othc_irq_sw, NULL, pm8058_no_sw,
	IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING | IRQF_DISABLED,
			"pm8058_othc_sw", dd);
	if (rc < 0) {
		pr_err("Unable to request pm8058_othc_sw IRQ\n");
		goto fail_sw_irq;
	}

	/* Check if the accessory is already inserted during boot up */
	if (dd->ir_gpio < 0) {
		rc = pm8xxx_read_irq_stat(dd->dev->parent, dd->othc_irq_ir);
		if (rc < 0) {
			pr_err("Unable to get accessory status at boot\n");
			goto fail_ir_status;
		}
	} else {
		rc = gpio_get_value_cansleep(dd->ir_gpio);
		if (rc < 0) {
			pr_err("Unable to get accessory status at boot\n");
			goto fail_ir_status;
		}
		rc = !rc;
	}
	if (rc) {
		pr_debug("Accessory inserted during boot up\n");
		/* process the data and report the inserted accessory */
		rc = pm8058_accessory_report(dd, 1);
		if (rc)
			pr_debug("Unabele to detect accessory at boot up\n");
	}

	device_init_wakeup(&pd->dev,
			hsed_config->hsed_bias_config->othc_wakeup);

	INIT_DELAYED_WORK(&dd->detect_work, detect_work_f);

	INIT_DELAYED_WORK(&dd->hs_work, hs_worker);

	if (dd->othc_support_n_switch == true)
		INIT_WORK(&dd->switch_work, switch_work_f);


	return 0;

fail_ir_status:
	free_irq(dd->othc_irq_sw, dd);
fail_sw_irq:
	free_irq(dd->othc_irq_ir, dd);
fail_ir_irq:
	if (dd->ir_gpio != -1)
		gpio_free(dd->ir_gpio);
fail_ir_gpio_req:
	input_unregister_device(ipd);
	dd->othc_ipd = NULL;
fail_micbias_config:
	input_free_device(ipd);
fail_input_alloc:
	switch_dev_unregister(&dd->othc_sdev);
	return rc;
}

static int __devinit pm8058_othc_probe(struct platform_device *pd)
{
	int rc;
	struct pm8058_othc *dd;
	struct resource *res;
	struct pmic8058_othc_config_pdata *pdata = pd->dev.platform_data;

	if (pdata == NULL) {
		pr_err("Platform data not present\n");
		return -EINVAL;
	}

	dd = kzalloc(sizeof(*dd), GFP_KERNEL);
	if (dd == NULL) {
		pr_err("Unable to allocate memory\n");
		return -ENOMEM;
	}

	/* Enable runtime PM ops, start in ACTIVE mode */
	rc = pm_runtime_set_active(&pd->dev);
	if (rc < 0)
		dev_dbg(&pd->dev, "unable to set runtime pm state\n");
	pm_runtime_enable(&pd->dev);

	res = platform_get_resource_byname(pd, IORESOURCE_IO, "othc_base");
	if (res == NULL) {
		pr_err("othc resource:Base address absent\n");
		rc = -ENXIO;
		goto fail_get_res;
	}

	dd->dev = &pd->dev;
	dd->othc_pdata = pdata;
	dd->othc_base = res->start;
	if (pdata->micbias_regulator == NULL) {
		pr_err("OTHC regulator not specified\n");
		goto fail_get_res;
	}

	dd->othc_vreg = regulator_get(NULL,
				pdata->micbias_regulator->regulator);
	if (IS_ERR(dd->othc_vreg)) {
		pr_err("regulator get failed\n");
		rc = PTR_ERR(dd->othc_vreg);
		goto fail_get_res;
	}

	rc = regulator_set_voltage(dd->othc_vreg,
				pdata->micbias_regulator->min_uV,
				pdata->micbias_regulator->max_uV);
	if (rc) {
		pr_err("othc regulator set voltage failed\n");
		goto fail_reg_enable;
	}

	rc = regulator_enable(dd->othc_vreg);
	if (rc) {
		pr_err("othc regulator enable failed\n");
		goto fail_reg_enable;
	}

	platform_set_drvdata(pd, dd);

	if (pdata->micbias_capability == OTHC_MICBIAS_HSED) {
		/* HSED to be supported on this MICBIAS line */
		if (pdata->hsed_config != NULL) {
			rc = othc_configure_hsed(dd, pd);
			if (rc < 0)
				goto fail_othc_hsed;
		} else {
			pr_err("HSED config data not present\n");
			rc = -EINVAL;
			goto fail_othc_hsed;
		}
	}

	/* Store the local driver data structure */
	if (dd->othc_pdata->micbias_select < OTHC_MICBIAS_MAX)
		config[dd->othc_pdata->micbias_select] = dd;

	pr_debug("Device %s:%d successfully registered\n",
			pd->name, pd->id);
	return 0;

fail_othc_hsed:
	regulator_disable(dd->othc_vreg);
fail_reg_enable:
	regulator_put(dd->othc_vreg);
fail_get_res:
	pm_runtime_set_suspended(&pd->dev);
	pm_runtime_disable(&pd->dev);

	kfree(dd);
	return rc;
}

static struct platform_driver pm8058_othc_driver = {
	.driver = {
		.name = "pm8058-othc",
		.owner = THIS_MODULE,
#ifdef CONFIG_PM
		.pm = &pm8058_othc_pm_ops,
#endif
	},
	.probe = pm8058_othc_probe,
	.remove = __devexit_p(pm8058_othc_remove),
};

static int __init pm8058_othc_init(void)
{
	return platform_driver_register(&pm8058_othc_driver);
}

static void __exit pm8058_othc_exit(void)
{
	platform_driver_unregister(&pm8058_othc_driver);
}
/*
 * Move to late_initcall, to make sure that the ADC driver registration is
 * completed before we open a ADC channel.
 */
late_initcall(pm8058_othc_init);
module_exit(pm8058_othc_exit);

MODULE_ALIAS("platform:pmic8058_othc");
MODULE_DESCRIPTION("PMIC 8058 OTHC");
MODULE_LICENSE("GPL v2");
