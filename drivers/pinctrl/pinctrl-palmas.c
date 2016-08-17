/*
 * pinctrl-palmas.c -- TI PALMAS series pin control driver.
 *
 * Copyright (c) 2013, NVIDIA Corporation.
 *
 * Author: Laxman Dewangan <ldewangan@nvidia.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation version 2.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any kind,
 * whether express or implied; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/delay.h>
#include <linux/mfd/palmas.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/slab.h>

struct palmas_pinctrl {
	struct device	*dev;
	struct palmas	*palmas;
};

struct palmas_pins_pullup_dn_info {
	int pullup_dn_reg_base;
	int pullup_dn_reg_add;
	int pullup_dn_mask;
	int normal_val;
	int pull_up_val;
	int pull_dn_val;
};

struct palmas_pins_od_info {
	int od_reg_base;
	int od_reg_add;
	int od_mask;
	int od_enable;
	int od_disable;
};

struct palmas_pin_info {
	int mux_opt;
	struct palmas_pins_pullup_dn_info *pud_info;
	struct palmas_pins_od_info *od_info;
};

struct palmas_pinctrl_info {
	const char *pin_name;
	int pin_id;
	int pin_rg_base;
	int pin_reg_add;
	int pin_reg_mask;
	int pin_bit_shift;
	struct palmas_pin_info *opt1;
	struct palmas_pin_info *opt2;
	struct palmas_pin_info *opt3;
	struct palmas_pin_info *opt4;
};

#define PALMAS_NONE_BASE	-1

#define PULL_UP_DN(_name, _rbase, _add, _mask, _nv, _uv, _dv)	\
static struct palmas_pins_pullup_dn_info pu_pd_##_name##_info = {	\
	.pullup_dn_reg_base = PALMAS_##_rbase##_BASE,		\
	.pullup_dn_reg_add = _add,				\
	.pullup_dn_mask = _mask,				\
	.normal_val = _nv,					\
	.pull_up_val = _uv,					\
	.pull_dn_val = _dv,					\
}

PULL_UP_DN(NRESWARM, PU_PD_OD, 0x0, 0x2, 0x0, 0x2, -1);
PULL_UP_DN(PWRDOWN, PU_PD_OD, 0x0, 0x4, 0x0, -1, 0x4);
PULL_UP_DN(GPADC_START, PU_PD_OD, 0x0, 0x30, 0x0, 0x20, 0x10);
PULL_UP_DN(RESET_IN, PU_PD_OD, 0x0, 0x40, 0x0, -1, 0x40);
PULL_UP_DN(NSLEEP, PU_PD_OD, 0x1, 0x3, 0x0, 0x2, 0x1);
PULL_UP_DN(ENABLE1, PU_PD_OD, 0x1, 0xC, 0x0, 0x8, 0x4);
PULL_UP_DN(ENABLE2, PU_PD_OD, 0x1, 0x30, 0x0, 0x20, 0x10);
PULL_UP_DN(VACOK, PU_PD_OD, 0x2, 0x40, 0x0, -1, 0x40);
PULL_UP_DN(CHRG_DET, PU_PD_OD, 0x2, 0x10, 0x0, -1, 0x10);
PULL_UP_DN(PWRHOLD, PU_PD_OD, 0x2, 0x4, 0x0, -1, 0x4);
PULL_UP_DN(MSECURE, PU_PD_OD, 0x2, 0x1, 0x0, -1, 0x1);
PULL_UP_DN(GPIO0, GPIO, 0x06, 0x04, 0, -1, 1);
PULL_UP_DN(GPIO1, GPIO, 0x06, 0x0C, 0, 0x8, 0x4);
PULL_UP_DN(GPIO2, GPIO, 0x06, 0x30, 0x0, 0x20, 0x10);
PULL_UP_DN(GPIO3, GPIO, 0x06, 0x40, 0x0, -1, 0x40);
PULL_UP_DN(GPIO4, GPIO, 0x07, 0x03, 0x0, 0x2, 0x1);
PULL_UP_DN(GPIO5, GPIO, 0x07, 0x0c, 0x0, 0x8, 0x4);
PULL_UP_DN(GPIO6, GPIO, 0x07, 0x30, 0x0, 0x20, 0x10);
PULL_UP_DN(GPIO7, GPIO, 0x07, 0x40, 0x0, -1, 0x40);

#define OD_INFO(_name, _rbase, _add, _mask, _ev, _dv)		\
static struct palmas_pins_od_info od_##_name##_info = {		\
	.od_reg_base = PALMAS_##_rbase##_BASE,			\
	.od_reg_add = _add,					\
	.od_mask = _mask,					\
	.od_enable = _ev,					\
	.od_disable = _dv,					\
}

OD_INFO(GPIO1, GPIO, 0x8, 0x1, 0x1, 0x0);
OD_INFO(GPIO2, GPIO, 0x8, 0x2, 0x2, 0x0);
OD_INFO(GPIO5, GPIO, 0x8, 0x20, 0x20, 0x0);
OD_INFO(INT, PU_PD_OD, 0x4, 0x8, 0x8, 0x0);
OD_INFO(PWM1, PU_PD_OD, 0x4, 0x20, 0x20, 0x0);
OD_INFO(VBUS_DET, PU_PD_OD, 0x4, 0x40, 0x40, 0x0);
OD_INFO(PWM2, PU_PD_OD, 0x4, 0x80, 0x80, 0x0);

#define PIN_INFO(_name, _id, _pud_info, _od_info)		\
static struct palmas_pin_info pin_##_name##_info = {		\
	.mux_opt = PALMAS_PINMUX_##_id,				\
	.pud_info = _pud_info,					\
	.od_info = _od_info					\
}

PIN_INFO(GPIO0,		GPIO,		&pu_pd_GPIO0_info,	NULL);
PIN_INFO(GPIO1,		GPIO,		&pu_pd_GPIO1_info,	&od_GPIO1_info);
PIN_INFO(GPIO2,		GPIO,		&pu_pd_GPIO2_info,	&od_GPIO2_info);
PIN_INFO(GPIO3,		GPIO,		&pu_pd_GPIO3_info,	NULL);
PIN_INFO(GPIO4,		GPIO,		&pu_pd_GPIO4_info,	NULL);
PIN_INFO(GPIO5,		GPIO,		&pu_pd_GPIO5_info,	&od_GPIO5_info);
PIN_INFO(GPIO6,		GPIO,		&pu_pd_GPIO6_info,	NULL);
PIN_INFO(GPIO7,		GPIO,		&pu_pd_GPIO7_info,	NULL);
PIN_INFO(ID,		ID,		NULL,			NULL);
PIN_INFO(LED1,		LED,		NULL,			NULL);
PIN_INFO(LED2,		LED,		NULL,			NULL);
PIN_INFO(REGEN,		REGEN,		NULL,			NULL);
PIN_INFO(SYSEN1,	SYSEN,		NULL,			NULL);
PIN_INFO(SYSEN2,	SYSEN,		NULL,			NULL);
PIN_INFO(INT,		INT,		NULL,			&od_INT_info);
PIN_INFO(PWM1,		PWM,		NULL,			&od_PWM1_info);
PIN_INFO(PWM2,		PWM,		NULL,			&od_PWM2_info);
PIN_INFO(VACOK,		VACOK,		&pu_pd_VACOK_info,	NULL);
PIN_INFO(CHRG_DET,	CHRG_DET,	&pu_pd_CHRG_DET_info,	NULL);
PIN_INFO(PWRHOLD,	PWRHOLD,	&pu_pd_PWRHOLD_info,	NULL);
PIN_INFO(MSECURE,	MSECURE,	&pu_pd_MSECURE_info,	NULL);
PIN_INFO(NRESWARM,	RESVD,		&pu_pd_NRESWARM_info,	NULL);
PIN_INFO(PWRDOWN,	RESVD,		&pu_pd_PWRDOWN_info,	NULL);
PIN_INFO(GPADC_START,	RESVD,		&pu_pd_GPADC_START_info, NULL);
PIN_INFO(RESET_IN,	RESVD,		&pu_pd_RESET_IN_info,	NULL);
PIN_INFO(NSLEEP,	RESVD,		&pu_pd_NSLEEP_info,	NULL);
PIN_INFO(ENABLE1,	RESVD,		&pu_pd_ENABLE1_info,	NULL);
PIN_INFO(ENABLE2,	RESVD,		&pu_pd_ENABLE2_info,	NULL);
PIN_INFO(CLK32KGAUDIO,	CLK32KGAUDIO,	NULL,			NULL);
PIN_INFO(USB_PSEL,	USB_PSEL,	NULL,			NULL);
PIN_INFO(VAC,		VAC,		NULL,			NULL);
PIN_INFO(POWERGOOD,	POWERGOOD,	NULL,			NULL);
PIN_INFO(VBUS_DET,	VBUS_DET,	NULL,		&od_VBUS_DET_info);

#define PALMAS_PIN(_pin, _rbase, _add, _mask, _bshift, o1, o2, o3, o4)	\
[PALMAS_PIN_NAME_##_pin] = {					\
		.pin_name = "palmas_pin_"#_pin,			\
		.pin_id = PALMAS_PIN_NAME_##_pin,		\
		.pin_rg_base = PALMAS_##_rbase##_BASE,		\
		.pin_reg_add = _add,				\
		.pin_reg_mask = _mask,				\
		.pin_bit_shift = _bshift,			\
		.opt1 = o1,					\
		.opt2 = o2,					\
		.opt3 = o3,					\
		.opt4 = o4,					\
}

static struct palmas_pinctrl_info palmas_pinctrl_info[PALMAS_PIN_NAME_MAX] = {
	PALMAS_PIN(GPIO0, PU_PD_OD, 0x6, 0x04, 0x2,
		&pin_GPIO0_info, &pin_ID_info, NULL, NULL),
	PALMAS_PIN(GPIO1, PU_PD_OD, 0x6, 0x18, 0x3,
		&pin_GPIO1_info, &pin_VBUS_DET_info, &pin_LED1_info,
		&pin_PWM1_info),
	PALMAS_PIN(GPIO2, PU_PD_OD, 0x6, 0x60, 0x5,
		&pin_GPIO2_info, &pin_REGEN_info, &pin_LED2_info,
		&pin_PWM2_info),
	PALMAS_PIN(GPIO3, PU_PD_OD, 0x6, 0x80, 0x7,
		&pin_GPIO3_info, &pin_CHRG_DET_info, NULL, NULL),
	PALMAS_PIN(GPIO4, PU_PD_OD, 0x7, 0x01, 0x0,
		&pin_GPIO4_info, &pin_SYSEN1_info, NULL, NULL),
	PALMAS_PIN(GPIO5, PU_PD_OD, 0x7, 0x06, 0x1,
		&pin_GPIO5_info, &pin_CLK32KGAUDIO_info, &pin_USB_PSEL_info,
		NULL),
	PALMAS_PIN(GPIO6, PU_PD_OD, 0x7, 0x8, 3,
		 &pin_GPIO6_info, &pin_SYSEN2_info, NULL, NULL),
	PALMAS_PIN(GPIO7, PU_PD_OD, 0x7, 0x30, 4,
		&pin_GPIO7_info, &pin_MSECURE_info, &pin_PWRHOLD_info, NULL),
	PALMAS_PIN(VAC, PU_PD_OD, 0x6, 0x2, 1,
		&pin_VAC_info, &pin_VACOK_info, NULL, NULL),
	PALMAS_PIN(POWERGOOD, PU_PD_OD, 0x6, 0x1, 0,
		&pin_POWERGOOD_info, &pin_USB_PSEL_info, NULL, NULL),
	PALMAS_PIN(NRESWARM, NONE, 0, 0, 0,
		&pin_NRESWARM_info, NULL, NULL, NULL),
	PALMAS_PIN(PWRDOWN, NONE, 0, 0, 0,
		&pin_PWRDOWN_info, NULL, NULL, NULL),
	PALMAS_PIN(GPADC_START, NONE, 0, 0, 0,
		&pin_GPADC_START_info, NULL, NULL, NULL),
	PALMAS_PIN(RESET_IN, NONE, 0, 0, 0,
		&pin_RESET_IN_info, NULL, NULL, NULL),
	PALMAS_PIN(NSLEEP, NONE, 0, 0, 0,
		&pin_NSLEEP_info, NULL, NULL, NULL),
	PALMAS_PIN(ENABLE1, NONE, 0, 0, 0,
		&pin_ENABLE1_info, NULL, NULL, NULL),
	PALMAS_PIN(ENABLE2, NONE, 0, 0, 0,
		&pin_ENABLE2_info, NULL, NULL, NULL),
	PALMAS_PIN(INT, NONE, 0, 0, 0,
		&pin_INT_info, NULL, NULL, NULL),
};

static int palmas_set_single_pin_config(struct palmas_pinctrl *pinctrl,
	struct palmas_pinctrl_config *muxcfg)
{
	int pin_id = muxcfg->pin_name;
	int pin_option = muxcfg->pin_mux_option;
	int pull_up_dn = muxcfg->pin_pull_up_dn;
	int od_config = muxcfg->open_drain_state;
	struct palmas_pin_info *opt;
	struct palmas_pinctrl_info *pin_cfg;
	struct palmas_pins_pullup_dn_info *pud_info;
	struct palmas_pins_od_info *od_info;
	int opt_nr;
	int ret = 0;
	int val;

	if ((pin_id < 0) || (pin_id >= PALMAS_PIN_NAME_MAX)) {
		dev_err(pinctrl->dev, "Pin id %d is out of range\n", pin_id);
		return -EINVAL;
	}

	pin_cfg = &palmas_pinctrl_info[pin_id];
	if (pin_id != pin_cfg->pin_id) {
		dev_err(pinctrl->dev,
			"Pin Config table for pin %s is out of sync\n",
			pin_cfg->pin_name);
		return -EINVAL;
	}

	opt = NULL;
	opt_nr = -1;
	if (pin_cfg->opt1 && (pin_cfg->opt1->mux_opt == pin_option)) {
		opt = pin_cfg->opt1;
		opt_nr = 0;
	} else if (pin_cfg->opt2 && (pin_cfg->opt2->mux_opt == pin_option)) {
		opt = pin_cfg->opt2;
		opt_nr = 1;
	} else if (pin_cfg->opt3 && (pin_cfg->opt3->mux_opt == pin_option)) {
		opt = pin_cfg->opt3;
		opt_nr = 2;
	} else if (pin_cfg->opt4 && (pin_cfg->opt4->mux_opt == pin_option)) {
		opt = pin_cfg->opt4;
		opt_nr = 1;
	}

	if (!opt) {
		dev_err(pinctrl->dev, "Pin %s does not have pinmux option %d\n",
			pin_cfg->pin_name, pin_option);
		return -EINVAL;
	}

	if (pin_cfg->pin_rg_base == PALMAS_NONE_BASE)
		goto skip_pinmux_config;

	ret = palmas_update_bits(pinctrl->palmas, pin_cfg->pin_rg_base,
			pin_cfg->pin_reg_add, pin_cfg->pin_reg_mask,
			opt_nr << pin_cfg->pin_bit_shift);
	if (ret < 0) {
		dev_err(pinctrl->dev, "Reg 0x%03x update failed, %d\n",
			pin_cfg->pin_rg_base + pin_cfg->pin_reg_add, ret);
		return ret;
	}

skip_pinmux_config:
	pud_info = opt->pud_info;
	if (pull_up_dn == PALMAS_PIN_CONFIG_DEFAULT)
		goto skip_pud_config;

	if (pud_info->pullup_dn_reg_base == PALMAS_NONE_BASE) {
		dev_err(pinctrl->dev, "Pin %s up/dn option not available\n",
			pin_cfg->pin_name);
		return -EINVAL;
	}

	switch (pull_up_dn) {
	case PALMAS_PIN_CONFIG_NORMAL:
		val = pud_info->normal_val;
		break;
	case PALMAS_PIN_CONFIG_PULL_UP:
		val = pud_info->pull_up_val;
		break;
	case PALMAS_PIN_CONFIG_PULL_DOWN:
		val = pud_info->pull_dn_val;
		break;
	default:
		dev_err(pinctrl->dev, "Invalid option of pull-up-dn %d\n",
			pull_up_dn);
		return -EINVAL;
	}
	if (val == -1) {
		dev_err(pinctrl->dev,
			"Pull up/dn option %d is not supported\n", pull_up_dn);
		return -EINVAL;
	}

	ret = palmas_update_bits(pinctrl->palmas, pud_info->pullup_dn_reg_base,
			pud_info->pullup_dn_reg_add, pud_info->pullup_dn_mask,
			val);
	if (ret < 0) {
		dev_err(pinctrl->dev, "reg 0x%03x update failed %d\n",
			pud_info->pullup_dn_reg_base +
				pud_info->pullup_dn_reg_add, ret);
		return ret;
	}

skip_pud_config:
	od_info = opt->od_info;
	if (od_config == PALMAS_PIN_CONFIG_OD_DEFAULT)
		goto skip_od_config;

	switch (od_config) {
	case PALMAS_PIN_CONFIG_OD_ENABLE:
		val = opt->od_info->od_enable;
		break;
	case PALMAS_PIN_CONFIG_OD_DISABLE:
		val = opt->od_info->od_disable;
		break;
	default:
		dev_err(pinctrl->dev, "Invalid option of OD %d\n", od_config);
		return -EINVAL;
	}

	if (opt->od_info->od_reg_base == PALMAS_NONE_BASE) {
		dev_err(pinctrl->dev, "Pin %s OD option not available\n",
			pin_cfg->pin_name);
		return -EINVAL;
	}

	ret = palmas_update_bits(pinctrl->palmas, od_info->od_reg_base,
		od_info->od_reg_add, od_info->od_mask, val);
	if (ret < 0) {
		dev_err(pinctrl->dev, "Reg 0x%03x update failed. %d\n",
			od_info->od_reg_base + od_info->od_reg_add, ret);
		return ret;
	}

skip_od_config:
	return ret;
}

static int palmas_set_dvfs1(struct palmas_pinctrl *pinctrl, bool enable)
{
	int ret;
	int val;

	val = (enable) ? PALMAS_PRIMARY_SECONDARY_PAD3_DVFS1 : 0;
	ret = palmas_update_bits(pinctrl->palmas, PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD3,
			PALMAS_PRIMARY_SECONDARY_PAD3_DVFS1, val);
	if (ret < 0)
		dev_err(pinctrl->dev, "SECONDARY_PAD3 update failed %d\n", ret);
	return ret;
}

static int palmas_set_dvfs2(struct palmas_pinctrl *pinctrl, bool enable)
{
	int ret;
	int val;

	val = (enable) ? PALMAS_PRIMARY_SECONDARY_PAD3_DVFS2 : 0;
	ret = palmas_update_bits(pinctrl->palmas, PALMAS_PU_PD_OD_BASE,
			PALMAS_PRIMARY_SECONDARY_PAD3,
			PALMAS_PRIMARY_SECONDARY_PAD3_DVFS2, val);
	if (ret < 0)
		dev_err(pinctrl->dev, "SECONDARY_PAD3 update failed %d\n", ret);
	return ret;
}

static int __devinit palmas_pinctrl_probe(struct platform_device *pdev)
{
	struct palmas_platform_data *pdata;
	struct palmas_pinctrl_platform_data *pctrl_pdata;
	struct palmas_pinctrl *pinctrl;
	struct palmas *palmas = dev_get_drvdata(pdev->dev.parent);
	int i;
	int ret;
	unsigned int reg;

	pdata = dev_get_platdata(pdev->dev.parent);
	if (!pdata || !pdata->pinctrl_pdata) {
		dev_err(&pdev->dev, "No Platform data\n");
		return -EINVAL;
	}

	pinctrl = devm_kzalloc(&pdev->dev, sizeof(*pinctrl), GFP_KERNEL);
	if (!pinctrl) {
		dev_err(&pdev->dev, "Memory allocation for pinctrl failed\n");
		return -ENOMEM;
	}

	pinctrl->dev = &pdev->dev;
	pinctrl->palmas = palmas;
	pctrl_pdata = pdata->pinctrl_pdata;
	for (i = 0; i < pctrl_pdata->num_pinctrl; ++i) {
		struct palmas_pinctrl_config *pcfg =  &pctrl_pdata->pincfg[i];

		palmas_set_single_pin_config(pinctrl, pcfg);
	}

	palmas_set_dvfs1(pinctrl, pctrl_pdata->dvfs1_enable);
	palmas_set_dvfs2(pinctrl, pctrl_pdata->dvfs2_enable);

	/* PAD1 muxing */
	ret = palmas_read(pinctrl->palmas, PALMAS_PU_PD_OD_BASE,
				PALMAS_PRIMARY_SECONDARY_PAD1, &reg);
	if (ret < 0) {
		dev_err(&pdev->dev, "SECONDARY_PAD1 read failed %d\n", ret);
		return ret;
	}
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_0))
		palmas->gpio_muxed |= PALMAS_GPIO_0_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_1_MUXED;
	else if ((reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_MASK) ==
			(2 << PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_SHIFT))
		palmas->led_muxed |= PALMAS_LED1_MUXED;
	else if ((reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_MASK) ==
			(3 << PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_1_SHIFT))
		palmas->pwm_muxed |= PALMAS_PWM1_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_2_MUXED;
	else if ((reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_MASK) ==
			(2 << PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_SHIFT))
		palmas->led_muxed |= PALMAS_LED2_MUXED;
	else if ((reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_MASK) ==
			(3 << PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_2_SHIFT))
		palmas->pwm_muxed |= PALMAS_PWM2_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD1_GPIO_3))
		palmas->gpio_muxed |= PALMAS_GPIO_3_MUXED;

	/* PAD2 muxing */
	ret = palmas_read(pinctrl->palmas, PALMAS_PU_PD_OD_BASE,
				PALMAS_PRIMARY_SECONDARY_PAD2, &reg);
	if (ret < 0) {
		dev_err(&pdev->dev, "SECONDARY_PAD2 read failed %d\n", ret);
		return ret;
	}
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_4))
		palmas->gpio_muxed |= PALMAS_GPIO_4_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_5_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_5_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_6))
		palmas->gpio_muxed |= PALMAS_GPIO_6_MUXED;
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD2_GPIO_7_MASK))
		palmas->gpio_muxed |= PALMAS_GPIO_7_MUXED;

	/* PAD3 muxing */
	ret = palmas_read(pinctrl->palmas, PALMAS_PU_PD_OD_BASE,
				PALMAS_PRIMARY_SECONDARY_PAD3, &reg);
	if (ret < 0) {
		dev_err(&pdev->dev, "SECONDARY_PAD3 read failed %d\n", ret);
		return ret;
	}
	if (!(reg & PALMAS_PRIMARY_SECONDARY_PAD3_DVFS2))
		palmas->gpio_muxed |= PALMAS_GPIO_6_MUXED;

	dev_info(palmas->dev, "Muxing GPIO %x, PWM %x, LED %x\n",
		palmas->gpio_muxed, palmas->pwm_muxed, palmas->led_muxed);

	return 0;
}

static struct platform_driver palmas_pinctrl_driver = {
	.probe = palmas_pinctrl_probe,
	.driver = {
		.name = "palmas-pinctrl",
		.owner = THIS_MODULE,
	},
};

static int __init palmas_pinctrl_init(void)
{
	return platform_driver_register(&palmas_pinctrl_driver);
}
subsys_initcall(palmas_pinctrl_init);

static void __exit palmas_pinctrl_exit(void)
{
	platform_driver_unregister(&palmas_pinctrl_driver);
}
module_exit(palmas_pinctrl_exit);

MODULE_DESCRIPTION("palmas pin control driver");
MODULE_AUTHOR("Laxman Dewangan<ldewangan@nvidia.com>");
MODULE_ALIAS("platform:palmas-pinctrl");
MODULE_LICENSE("GPL v2");
