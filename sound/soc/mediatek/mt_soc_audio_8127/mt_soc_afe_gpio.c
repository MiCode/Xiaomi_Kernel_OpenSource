/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

/*****************************************************************************
 *                     C O M P I L E R   F L A G S
 *****************************************************************************/


/*****************************************************************************
 *                E X T E R N A L   R E F E R E N C E S
 *****************************************************************************/

#if !defined(CONFIG_MTK_LEGACY)
#include <linux/gpio.h>
#include <linux/pinctrl/consumer.h>
#else
#include <mt-plat/mt_gpio.h>
#endif
#include "mt_soc_afe_gpio.h"

#if !defined(CONFIG_MTK_LEGACY)
struct pinctrl *pinctrlaud;

enum audio_system_gpio_type {
	GPIO_DEFAULT = 0,
	GPIO_PMIC_MODE0,
	GPIO_PMIC_MODE1,
	GPIO_I2S_MODE0,
	GPIO_I2S_MODE1,
	GPIO_EXTAMP_HIGH,
	GPIO_EXTAMP_LOW,
	GPIO_NUM
};


struct audio_gpio_attr {
	const char *name;
	bool gpio_prepare;
	struct pinctrl_state *gpioctrl;
};

static struct audio_gpio_attr aud_gpios[GPIO_NUM] = {
	[GPIO_DEFAULT] = {"default", false, NULL},
	[GPIO_PMIC_MODE0] = {"audpmicclk-mode0", false, NULL},
	[GPIO_PMIC_MODE1] = {"audpmicclk-mode1", false, NULL},
	[GPIO_I2S_MODE0] = {"audi2s1-mode0", false, NULL},
	[GPIO_I2S_MODE1] = {"audi2s1-mode1", false, NULL},
	[GPIO_EXTAMP_HIGH] = {"extamp-pullhigh", false, NULL},
	[GPIO_EXTAMP_LOW] = {"extamp-pulllow", false, NULL},
};


void AudDrv_GPIO_probe(void *dev)
{
	/*int ret;
	int i = 0;*/

	pr_debug("%s\n", __func__);
#if 0

	pinctrlaud = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrlaud)) {
		ret = PTR_ERR(pinctrlaud);
		pr_err("Cannot find pinctrlaud!\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(aud_gpios); i++) {
		aud_gpios[i].gpioctrl = pinctrl_lookup_state(pinctrlaud, aud_gpios[i].name);
		if (IS_ERR(aud_gpios[i].gpioctrl)) {
			ret = PTR_ERR(aud_gpios[i].gpioctrl);
			pr_err("%s pinctrl_lookup_state %s fail %d\n", __func__, aud_gpios[i].name,
			       ret);
		} else {
			aud_gpios[i].gpio_prepare = true;
			pr_err("%s pinctrl_lookup_state %s success!\n", __func__, aud_gpios[i].name);
		}
	}

	pins_default = pinctrl_lookup_state(pinctrlaud, "default");
	if (IS_ERR(pins_default)) {
		ret = PTR_ERR(pins_default);
		dev_err(&pdev->dev, "Cannot find aud pinctrl default!\n");
		return;
	}

	audpmic_mode0 = pinctrl_lookup_state(pinctrlaud, "audpmicclk-mode0");
	if (IS_ERR(audpmic_mode0)) {
		ret = PTR_ERR(audpmic_mode0);
		dev_err(&pdev->dev, "Cannot find pinctrl audpmic_mode0!\n");
		return;
	}

	audpmic_mode1 = pinctrl_lookup_state(pinctrlaud, "audpmicclk-mode1");
	if (IS_ERR(audpmic_mode1)) {
		ret = PTR_ERR(audpmic_mode1);
		dev_err(&pdev->dev, "Cannot find pinctrl audpmic_mode1!\n");
		return;
	}

	audi2s1_mode0 = pinctrl_lookup_state(pinctrlaud, "audi2s1-mode0");
	if (IS_ERR(audi2s1_mode0)) {
		ret = PTR_ERR(audi2s1_mode0);
		dev_err(&pdev->dev, "Cannot find pinctrl audi2s1_mode0!\n");
		return;
	}

	audi2s1_mode1 = pinctrl_lookup_state(pinctrlaud, "audi2s1-mode1");
	if (IS_ERR(audi2s1_mode1)) {
		ret = PTR_ERR(audi2s1_mode1);
		dev_err(&pdev->dev, "Cannot find pinctrl audi2s1_mode1!\n");
		return;
	}


	audextamp_high = pinctrl_lookup_state(pinctrlaud, "extamp-pullhigh");
	if (IS_ERR(audextamp_high)) {
		ret = PTR_ERR(audextamp_high);
		dev_err(&pdev->dev, "Cannot find pinctrl audextamp_high!\n");
		return;
	}


	audextamp_low = pinctrl_lookup_state(pinctrlaud, "extamp-pulllow");
	if (IS_ERR(audextamp_low)) {
		ret = PTR_ERR(audextamp_low);
		dev_err(&pdev->dev, "Cannot find pinctrl audextamp_low!\n");
		return;
	}

	audextamp2_high = pinctrl_lookup_state(pinctrlaud, "extamp2-pullhigh");
	if (IS_ERR(audextamp2_high)) {
		ret = PTR_ERR(audextamp2_high);
		dev_err(&pdev->dev, "Cannot find pinctrl audextamp2_high!\n");
		return;
	}

	audextamp2_low = pinctrl_lookup_state(pinctrlaud, "extamp2-pulllow");
	if (IS_ERR(audextamp2_low)) {
		ret = PTR_ERR(audextamp2_low);
		dev_err(&pdev->dev, "Cannot find pinctrl audextamp2_low!\n");
		return;
	}

	audcvspk_high = pinctrl_lookup_state(pinctrlaud, "rcvspk-pullhigh");
	if (IS_ERR(audcvspk_high)) {
		ret = PTR_ERR(audcvspk_high);
		dev_err(&pdev->dev, "Cannot find pinctrl audcvspk_high!\n");
		return;
	}

	audcvspk_low = pinctrl_lookup_state(pinctrlaud, "rcvspk-pulllow");
	if (IS_ERR(audcvspk_low)) {
		ret = PTR_ERR(audcvspk_low);
		dev_err(&pdev->dev, "Cannot find pinctrl audcvspk_low!\n");
		return;
	}
#endif

}

int AudDrv_GPIO_PMIC_Select(int bEnable)
{
	int retval = 0;

	pr_debug("%s\n", __func__);

	if (bEnable == 1) {
		if (aud_gpios[GPIO_PMIC_MODE1].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[GPIO_PMIC_MODE1].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_PMIC_MODE1] pins\n");
		}
	} else {
		if (aud_gpios[GPIO_PMIC_MODE0].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[GPIO_PMIC_MODE0].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_PMIC_MODE0] pins\n");
		}

	}
	return retval;
}

int AudDrv_GPIO_I2S_Select(int bEnable)
{
	int retval = 0;

	pr_debug("%s\n", __func__);
#if 0
	if (bEnable == 1) {
		if (aud_gpios[GPIO_I2S_MODE1].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[GPIO_I2S_MODE1].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_I2S_MODE1] pins\n");
		}
	} else {
		if (aud_gpios[GPIO_I2S_MODE0].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[GPIO_I2S_MODE0].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_I2S_MODE0] pins\n");
		}

	}
#endif
	return retval;
}

int AudDrv_GPIO_EXTAMP_Select(int bEnable)
{
	int retval = 0;

	pr_debug("%s\n", __func__);
#if 0
	if (bEnable == 1) {
		if (aud_gpios[GPIO_EXTAMP_HIGH].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[GPIO_EXTAMP_HIGH].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_EXTAMP_HIGH] pins\n");
		}
	} else {
		if (aud_gpios[GPIO_EXTAMP_LOW].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud, aud_gpios[GPIO_EXTAMP_LOW].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_EXTAMP_LOW] pins\n");
		}

	}
#endif
	return retval;
}


#endif
