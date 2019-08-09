/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
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
#include "AudDrv_Gpio.h"

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
	GPIO_EXTAMP_GAIN0,
	GPIO_EXTAMP_GAIN1,
	GPIO_EXTAMP_GAIN2,
	GPIO_EXTAMP_GAIN3,
	GPIO_HPSPK_SWITCH_HIGH,
	GPIO_HPSPK_SWITCH_LOW,
	GPIO_HPAMP_HIGH,
	GPIO_HPAMP_LOW,
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
	[GPIO_EXTAMP_GAIN0] = {"extamp-gain0", false, NULL},
	[GPIO_EXTAMP_GAIN1] = {"extamp-gain1", false, NULL},
	[GPIO_EXTAMP_GAIN2] = {"extamp-gain2", false, NULL},
	[GPIO_EXTAMP_GAIN3] = {"extamp-gain3", false, NULL},
	[GPIO_HPSPK_SWITCH_HIGH] = {"hpspk-pullhigh", false, NULL},
	[GPIO_HPSPK_SWITCH_LOW] = {"hpspk-pulllow", false, NULL},
	[GPIO_HPAMP_HIGH] = {"hpamp-pullhigh", false, NULL},
	[GPIO_HPAMP_LOW] = {"hpamp-pulllow", false, NULL},
};


void AudDrv_GPIO_probe(void *dev)
{
	int ret;
	int i = 0;

	pr_debug("%s\n", __func__);

	pinctrlaud = devm_pinctrl_get(dev);
	if (IS_ERR(pinctrlaud)) {
		ret = PTR_ERR(pinctrlaud);
		pr_err("Cannot find pinctrlaud!\n");
		return;
	}

	for (i = 0; i < ARRAY_SIZE(aud_gpios); i++) {
		aud_gpios[i].gpioctrl = pinctrl_lookup_state(pinctrlaud,
			aud_gpios[i].name);
		if (IS_ERR(aud_gpios[i].gpioctrl)) {
			ret = PTR_ERR(aud_gpios[i].gpioctrl);
			pr_err("%s pinctrl_lookup_state %s fail %d\n",
				__func__, aud_gpios[i].name, ret);
		} else {
			aud_gpios[i].gpio_prepare = true;
			pr_debug("%s pinctrl_lookup_state %s success!\n",
				__func__, aud_gpios[i].name);
		}
	}
}

int AudDrv_GPIO_PMIC_Select(int bEnable)
{
	int retval = 0;

	pr_debug("%s\n", __func__);

	if (bEnable == 1) {
		if (aud_gpios[GPIO_PMIC_MODE1].gpio_prepare) {
			retval =
				pinctrl_select_state(pinctrlaud,
				aud_gpios[GPIO_PMIC_MODE1].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_PMIC_MODE1] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_PMIC_MODE1] pins are not prepared!\n");
			retval = -1;
		}
	} else {
		if (aud_gpios[GPIO_PMIC_MODE0].gpio_prepare) {
			retval =
				pinctrl_select_state(pinctrlaud,
					aud_gpios[GPIO_PMIC_MODE0].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_PMIC_MODE0] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_PMIC_MODE0] pins are not prepared!\n");
			retval = -1;
		}
	}
	return retval;
}

int AudDrv_GPIO_I2S_Select(int bEnable)
{
	int retval = 0;

	pr_debug("%s\n", __func__);

	if (bEnable == 1) {
		if (aud_gpios[GPIO_I2S_MODE1].gpio_prepare) {
			retval = pinctrl_select_state(pinctrlaud,
				aud_gpios[GPIO_I2S_MODE1].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_I2S_MODE1] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_I2S_MODE1] pins are not prepared!\n");
			retval = -1;
		}
	} else {
		if (aud_gpios[GPIO_I2S_MODE0].gpio_prepare) {
			retval =
			pinctrl_select_state(pinctrlaud,
				aud_gpios[GPIO_I2S_MODE0].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_I2S_MODE0] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_I2S_MODE0] pins are not prepared!\n");
			retval = -1;
		}
	}
	return retval;
}

int AudDrv_GPIO_EXTAMP_Select(int bEnable)
{
	int retval = 0;

	pr_debug("%s\n", __func__);

	if (bEnable == 1) {
		if (aud_gpios[GPIO_EXTAMP_HIGH].gpio_prepare) {
			retval =
			pinctrl_select_state(pinctrlaud,
				aud_gpios[GPIO_EXTAMP_HIGH].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_EXTAMP_HIGH] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_EXTAMP_HIGH] pins are not prepared!\n");
			retval = -1;
		}
	} else {
		if (aud_gpios[GPIO_EXTAMP_LOW].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud,
				aud_gpios[GPIO_EXTAMP_LOW].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_EXTAMP_LOW] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_EXTAMP_LOW] pins are not prepared!\n");
			retval = -1;
		}
	}
	return retval;
}

int AudDrv_GPIO_EXTAMP_Gain_Set(int value)
{
	int retval = 0;

	pr_debug("%s value = %d\n", __func__, value);

	switch (value) {
	case 3:
		if (aud_gpios[GPIO_EXTAMP_GAIN3].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud,
			    aud_gpios[GPIO_EXTAMP_GAIN3].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_EXTAMP_GAIN3] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_EXTAMP_GAIN3] pins are not prepared!\n");
			retval = -1;
		}
		break;
	case 2:
		if (aud_gpios[GPIO_EXTAMP_GAIN2].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud,
			    aud_gpios[GPIO_EXTAMP_GAIN2].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_EXTAMP_GAIN2] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_EXTAMP_GAIN2] pins are not prepared!\n");
			retval = -1;
		}
		break;
	case 1:
		if (aud_gpios[GPIO_EXTAMP_GAIN1].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud,
			    aud_gpios[GPIO_EXTAMP_GAIN1].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_EXTAMP_GAIN1] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_EXTAMP_GAIN1] pins are not prepared!\n");
			retval = -1;
		}
		break;
	case 0:
		if (aud_gpios[GPIO_EXTAMP_GAIN0].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud,
			    aud_gpios[GPIO_EXTAMP_GAIN0].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_EXTAMP_GAIN0] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_EXTAMP_GAIN0] pins are not prepared!\n");
			retval = -1;
		}
		break;
	default:
		pr_err("unsupported extamp gain mode!!!\n");
		return -1;
	}
	return retval;
}

int AudDrv_GPIO_HP_SPK_Switch_Select(int bEnable)
{
	int retval = 0;

	pr_debug("%s bEnable = %d\n", __func__, bEnable);

	if (bEnable == 1) {
		if (aud_gpios[GPIO_HPSPK_SWITCH_HIGH].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud,
			    aud_gpios[GPIO_HPSPK_SWITCH_HIGH].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_HPSPK_SWITCH_HIGH] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_HPSPK_SWITCH_HIGH] pins are not prepared!\n");
			retval = -1;
		}
	} else {
		if (aud_gpios[GPIO_HPSPK_SWITCH_LOW].gpio_prepare) {
			retval =
			    pinctrl_select_state(pinctrlaud,
			    aud_gpios[GPIO_HPSPK_SWITCH_LOW].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_HPSPK_SWITCH_LOW] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_HPSPK_SWITCH_LOW] pins are not prepared!\n");
			retval = -1;
		}
	}
	return retval;
}

int AudDrv_GPIO_EXTHPAMP_Select(int bEnable)
{
	int retval = 0;

	pr_debug("%s bEnable = %d\n", __func__, bEnable);

	if (bEnable == 1) {
		if (aud_gpios[GPIO_HPAMP_HIGH].gpio_prepare) {
			retval =
			pinctrl_select_state(pinctrlaud,
				aud_gpios[GPIO_HPAMP_HIGH].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_HPAMP_HIGH] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_HPAMP_HIGH] pins are not prepared!\n");
			retval = -1;
		}
	} else {
		if (aud_gpios[GPIO_HPAMP_LOW].gpio_prepare) {
			retval =
			pinctrl_select_state(pinctrlaud,
				aud_gpios[GPIO_HPAMP_LOW].gpioctrl);
			if (retval)
				pr_err("could not set aud_gpios[GPIO_HPAMP_LOW] pins\n");
		} else {
			pr_err("aud_gpios[GPIO_HPAMP_LOW] pins are not prepared!\n");
			retval = -1;
		}
	}
	return retval;
}


#endif
