/*
 *  intel_mid_vibra_acpi.c - Intel vibrator driver
 *
 *  Copyright (C) 2013 Intel Corp
 *  Author: B, Jayachandran <jayachandran.b@intel.com>
 *  Author: Omair Md Abdullah <omair.m.abdullah@intel.com>
 *
 *  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; version 2 of the License.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/pm_runtime.h>
#include <linux/acpi.h>
#include <linux/mfd/intel_soc_pmic.h>
#include <linux/input/intel_mid_vibra.h>
#include "mid_vibra.h"
#include <linux/pwm.h>
#include <linux/io.h>
#include <linux/dmi.h>

#define CRYSTALCOVE_PMIC_PWM_EN_GPIO_REG        0x2F
/* PWM enable gpio register settings: drive type = CMOS; pull disabled */
#define CRYSTALCOVE_PMIC_PWM_EN_GPIO_VALUE	0x22
#define CRYSTALCOVE_PMIC_PWM1_CLKDIV_REG	0x4C
#define CRYSTALCOVE_PMIC_PWM1_DUTYCYC_REG	0x4F
#define CRYSTALCOVE_PMIC_PWM_ENABLE		0x80

#define CRYSTALCOVE_PMIC_VIBRA_MAX_BASEUNIT	0x7F

/* for CHT CR SOC controlled vibra */
#define SE_BASE_ADDRESS 0xFED98000
#define CFG0_ADDR 0x4408
#define CFG1_ADDR 0x440C
#define PERIOD_NS 2000
#define DUTY_NS_OFF 1000   /* 50 % */
#define DUTY_NS_ON 500     /* 25 % */

struct mid_vibra_pdata vibra_pdata = {
	.time_divisor	= 0x7f, /* for 50% duty cycle */
	.base_unit	= 0x0,
	.gpio_pwm	= -1,
	.name		= "VIBR22A8",
	.use_gpio_en    = false,
		/* CHT vibra doesnt use gpio enable control */
};

static int vibra_pwm_configure(struct vibra_info *info, bool enable)
{
	struct pwm_chip *chip = find_pwm_dev(1);
	struct pwm_device *pwm;
	int pwm_id;

	if (!chip) {
		pr_err("%s: Could not get pwm chip", __func__);
		return 0;
	}
	pwm_id = chip->pwms[0].pwm;
	pwm = pwm_request(pwm_id, "byt-pwm");
	if (!pwm) {
		pr_err("%s: Could not get pwm device", __func__);
		return -ENODEV;
	}

	if (enable) {
		pr_info("%s: Config and enable vibra  devi\n", __func__);
		chip->ops->config(chip,  pwm,  DUTY_NS_ON,  PERIOD_NS);
		chip->ops->enable(chip,  pwm);
	} else {
		pr_info("%s: disable  vibra device\n", __func__);
		chip->ops->config(chip,  pwm,  DUTY_NS_OFF,  PERIOD_NS);
		chip->ops->enable(chip,  pwm);
	}
	return 0;
}

static int vibra_pmic_pwm_configure(struct vibra_info *info, bool enable)
{
	u8 clk_div;
	u8 duty_cyc;

	if (enable) {
		/* disable PWM before updating clock div*/
		intel_soc_pmic_writeb(CRYSTALCOVE_PMIC_PWM1_CLKDIV_REG, 0);

		/* validate the input values */
		if (*info->base_unit > info->max_base_unit) {
			*info->base_unit = info->max_base_unit;
			pr_err("%s:base_unit i/p is greater than max using max",
								__func__);
		}
		if (*info->duty_cycle > info->max_duty_cycle) {
			*info->duty_cycle = info->max_duty_cycle;
			pr_err("%s:duty_cycle i/p greater than max", __func__);
		}

		clk_div = *info->base_unit;
		duty_cyc = *info->duty_cycle;

		clk_div = clk_div | CRYSTALCOVE_PMIC_PWM_ENABLE;
		intel_soc_pmic_writeb(CRYSTALCOVE_PMIC_PWM1_DUTYCYC_REG,
						duty_cyc);
		intel_soc_pmic_writeb(CRYSTALCOVE_PMIC_PWM1_CLKDIV_REG,
						clk_div);
	} else {
		/*disable PWM block */
		clk_div =  intel_soc_pmic_readb(
					CRYSTALCOVE_PMIC_PWM1_CLKDIV_REG);
		intel_soc_pmic_writeb(CRYSTALCOVE_PMIC_PWM1_CLKDIV_REG,
				      (clk_div & ~CRYSTALCOVE_PMIC_PWM_ENABLE));
	}
	clk_div =  intel_soc_pmic_readb(CRYSTALCOVE_PMIC_PWM1_CLKDIV_REG);
	duty_cyc =  intel_soc_pmic_readb(CRYSTALCOVE_PMIC_PWM1_DUTYCYC_REG);
	pr_debug("%s: clk_div_reg = %#x, duty_cycle_reg = %#x\n",
						__func__, clk_div, duty_cyc);
	return 0;
}

#if IS_ENABLED(CONFIG_ACPI)

int intel_mid_plat_vibra_probe(struct platform_device *pdev)
{
	struct vibra_info *info;
	struct device *dev = &pdev->dev;
	acpi_handle handle = ACPI_HANDLE(dev);
	struct acpi_device *device;
	const char *hid;
	struct mid_vibra_pdata *data;
	int ret;
	const char *board_name;

	ret = acpi_bus_get_device(handle, &device);
	if (ret) {
		pr_err("%s: could not get acpi device - %d\n", __func__, ret);
		return -ENODEV;
	}
	hid = acpi_device_hid(device);
	pr_debug("%s for %s", __func__, hid);

	data = mid_vibra_acpi_get_drvdata(hid);
	if (!data) {
		pr_err("Invalid driver data\n");
		return -ENODEV;
	}
	board_name = dmi_get_system_info(DMI_BOARD_NAME);
	if (strcmp(board_name, "Cherry Trail CR") == 0) {
		pr_info("Cherry Trail CR: intel_mid_plat_vibra_probe\n");
		/* make sure that the pad is set to native mode */
		void __iomem *cfg = ioremap_nocache(SE_BASE_ADDRESS, 0x8000);
		iowrite32(0x10000, cfg + CFG0_ADDR);
		iounmap(cfg);
	}

	if (data->use_gpio_en) {
		if (data->gpio_en < 0) {
			pr_err("Invalid gpio number from acpi\n");
			return -ENODEV;
		}
	}

	info = mid_vibra_setup(dev, data);
	if (!info)
		return -ENODEV;

	if (strcmp(board_name, "Cherry Trail CR") == 0) {
		info->pwm_configure = vibra_pwm_configure;
		/* WA: Until BIOS set 50% duty cycle on boot */
		vibra_pwm_configure(info, false);
	} else {
		info->pwm_configure = vibra_pmic_pwm_configure;
		info->max_base_unit = CRYSTALCOVE_PMIC_VIBRA_MAX_BASEUNIT;
		info->max_duty_cycle = INTEL_VIBRA_MAX_TIMEDIVISOR;
	}
	if (data->use_gpio_en) {
		pr_debug("%s: using gpio_en: %d", __func__, info->gpio_en);
		ret = gpio_request_one(info->gpio_en, GPIOF_DIR_OUT,
				"VIBRA ENABLE");
		if (ret != 0) {
			pr_err("gpio_request(%d) fails:%d\n",
					info->gpio_en, ret);
			return ret;
		}
		/* Re configure the PWM EN GPIO to have drive type as CMOS
		 * and pull disable
		 */
		intel_soc_pmic_writeb(CRYSTALCOVE_PMIC_PWM_EN_GPIO_REG,
				CRYSTALCOVE_PMIC_PWM_EN_GPIO_VALUE);
	}

	ret = sysfs_create_group(&dev->kobj, info->vibra_attr_group);
	if (ret) {
		pr_err("could not register sysfs files\n");
		vibra_gpio_free(info);
		return ret;
	}

	platform_set_drvdata(pdev, info);
	pm_runtime_allow(&pdev->dev);
	pm_runtime_put_noidle(&pdev->dev);
	pr_info("%s: vibra probe success\n", __func__);
	return ret;
}

int intel_mid_plat_vibra_remove(struct platform_device *pdev)
{
	struct vibra_info *info = platform_get_drvdata(pdev);
	vibra_gpio_free(info);
	sysfs_remove_group(&info->dev->kobj, info->vibra_attr_group);
	platform_set_drvdata(pdev, NULL);
	return 0;
}

#else
int intel_mid_plat_vibra_probe(struct platform_device *pdev)
{
	return -EINVAL;
}

int intel_mid_plat_vibra_remove(struct platform_device *pdev)
{
	return -EINVAL;
}
#endif

MODULE_ALIAS("platform:intel_mid_vibra");
MODULE_DESCRIPTION("Intel(R) MID ACPI Vibra driver");
MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("Jayachandran.B <jayachandran.b@intel.com>");
MODULE_AUTHOR("Omair Md Abdullah <omair.m.abdullah@intel.com>");
