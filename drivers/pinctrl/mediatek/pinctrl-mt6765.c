/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/pinctrl/pinctrl.h>
#include <linux/regmap.h>
#include <linux/irq.h>
#include <linux/irqdomain.h>
#include <dt-bindings/pinctrl/mt65xx.h>
#include "pinctrl-mtk-common.h"
#include "pinctrl-mtk-mt6765.h"


static int mtk_pinctrl_set_gpio_pupd_r1r0(struct mtk_pinctrl *pctl,
	unsigned int pin, bool enable, bool isup, unsigned int r1r0)
{
	unsigned int r0, r1, ret;

	ret = mtk_pinctrl_set_gpio_value(pctl, pin, !isup,
		pctl->devdata->n_pin_pupd, pctl->devdata->pin_pupd_grps);
	if (ret == 0) {
		r0 = r1r0 & 0x1;
		r1 = (r1r0 & 0x2) >> 1;
		mtk_pinctrl_set_gpio_value(pctl, pin, r0,
			pctl->devdata->n_pin_r0, pctl->devdata->pin_r0_grps);
		mtk_pinctrl_set_gpio_value(pctl, pin, r1,
			pctl->devdata->n_pin_r1, pctl->devdata->pin_r1_grps);
		ret = 0;
	}
	return ret;
}

static int mtk_pinctrl_get_gpio_pupd_r1r0(struct mtk_pinctrl *pctl,
	unsigned int pin)
{
	int bit_pupd, bit_r0, bit_r1;

	bit_pupd = mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_pupd, pctl->devdata->pin_pupd_grps);
	if (bit_pupd != -EPERM) {
		bit_r1 = mtk_pinctrl_get_gpio_value(pctl, pin,
			pctl->devdata->n_pin_r1, pctl->devdata->pin_r1_grps);
		bit_r0 = mtk_pinctrl_get_gpio_value(pctl, pin,
			pctl->devdata->n_pin_r0, pctl->devdata->pin_r0_grps);
		return (!bit_pupd)|(bit_r0<<1)|(bit_r1<<2)|(1<<3);
	}
	return -EPERM;
}

static int mtk_pinctrl_get_gpio_pu_pd(struct mtk_pinctrl *pctl,
	unsigned int pin)
{
	unsigned int bit_pu = 0, bit_pd = 0;

	bit_pu = mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_pu, pctl->devdata->pin_pu_grps);
	bit_pd = mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_pd, pctl->devdata->pin_pd_grps);
	if ((bit_pd != -EPERM) && (bit_pu != -EPERM))
		return (bit_pd)|(bit_pu<<1);
	else if ((bit_pd == -EPERM) && (bit_pu != -EPERM))
		return bit_pu<<1;
	else if ((bit_pd != -EPERM) && (bit_pu == -EPERM))
		return bit_pd;
	else
		return -EPERM;
}

static int mtk_pinctrl_set_gpio_pu_pd(struct mtk_pinctrl *pctl,
	unsigned int pin, bool enable, bool isup, unsigned int r1r0)
{
	mtk_pinctrl_set_gpio_value(pctl, pin, isup,
		pctl->devdata->n_pin_pu, pctl->devdata->pin_pu_grps);
	if (isup == 1)
		mtk_pinctrl_set_gpio_value(pctl, pin, !enable,
		pctl->devdata->n_pin_pd, pctl->devdata->pin_pd_grps);
	else
		mtk_pinctrl_set_gpio_value(pctl, pin, enable,
		pctl->devdata->n_pin_pd, pctl->devdata->pin_pd_grps);
	return 0;
}

static int mtk_pinctrl_get_gpio_pullen(struct mtk_pinctrl *pctl,
	unsigned int pin)
{
	unsigned int pull_en = 0;

	pull_en = mtk_pinctrl_get_gpio_pupd_r1r0(pctl, pin);
	if (pull_en == -EPERM) {
		pull_en = mtk_pinctrl_get_gpio_pu_pd(pctl, pin);
	/*pull_en = [pu,pd], 10,01 pull enabel, others pull disable*/
		if ((pull_en == 0x1) || (pull_en == 0x2))
			pull_en = GPIO_PULL_ENABLE;
		else if (pull_en == -EPERM)
			pull_en = GPIO_PULL_UNSUPPORTED;
		else
			pull_en = GPIO_PULL_DISABLE;
	} else {
	/*pull_en = [r1,r0,pupd], pull disabel 000,001, others enable*/
		if ((pull_en == 0x8) || (pull_en == 0x9))
			pull_en = GPIO_PULL_DISABLE;
		else
			pull_en = GPIO_PULL_ENABLE;
	}
	return pull_en;

}

static int mtk_pinctrl_get_gpio_pullsel(struct mtk_pinctrl *pctl,
	unsigned int pin)
{
	unsigned int pull_sel = 0;

	pull_sel = mtk_pinctrl_get_gpio_pupd_r1r0(pctl, pin);
	if (pull_sel == -EPERM) {
		pull_sel = mtk_pinctrl_get_gpio_pu_pd(pctl, pin);
		/*pull_sel = [pu,pd], 10 is pull up, 01 is pull down*/
		if (pull_sel == 0x02)
			pull_sel = GPIO_PULL_UP;
		else if (pull_sel == 0x01)
			pull_sel = GPIO_PULL_DOWN;
		else if (pull_sel == -EPERM)
			pull_sel = GPIO_PULL_UNSUPPORTED;
		else
			pull_sel = GPIO_NO_PULL;
	}
	return pull_sel;
}

static int mtk_pinctrl_set_gpio_pullsel(struct mtk_pinctrl *pctl,
		unsigned int pin, bool enable, bool isup, unsigned int arg)
{
	int ret = 0;
#ifndef GPIO_DEBUG
	ret = mtk_pinctrl_set_gpio_pupd_r1r0(pctl, pin, enable, isup, arg);
	if (ret != 0)
		mtk_pinctrl_set_gpio_pu_pd(pctl, pin, enable, isup, arg);
#else
	pr_debug("mtk_pinctrl_set_gpio_pull,pin=%d,enab=%d,sel=%d\n",
		pin, enable, isup);
	ret = mtk_pinctrl_set_gpio_pupd_r1r0(pctl, pin, enable, isup, arg);
	if (ret != 0)
		mtk_pinctrl_set_gpio_pu_pd(pctl, pin, enable, isup, arg);
	pr_debug("mtk_pinctrl_get_gpio_pull,pin=%d,enab=%d,sel=%d\n",
		pin, mtk_pinctrl_get_gpio_pullen(pctl, pin),
		mtk_pinctrl_get_gpio_pullsel(pctl, pin));
#endif
	return 0;
}

int mtk_pinctrl_get_gpio_mode_for_eint(int pin)
{
	return mtk_pinctrl_get_gpio_value(pctl, pin,
		pctl->devdata->n_pin_mode, pctl->devdata->pin_mode_grps);
}

static const unsigned int mt6765_debounce_data[] = {
	128, 256, 512, 1024, 16384,
	32768, 65536, 131072, 262144, 524288
};

static unsigned int mt6765_spec_debounce_select(unsigned int debounce)
{
	return mtk_gpio_debounce_select(mt6765_debounce_data,
		ARRAY_SIZE(mt6765_debounce_data), debounce);
}

int mtk_irq_domain_xlate_fourcell(struct irq_domain *d,
	struct device_node *ctrlr, const u32 *intspec, unsigned int intsize,
	irq_hw_number_t *out_hwirq, unsigned int *out_type)
{
	struct mtk_desc_eint *eint;
	int gpio, mode;

	if (WARN_ON(intsize < 4))
		return -EINVAL;
	*out_hwirq = intspec[0];
	*out_type = intspec[1] & IRQ_TYPE_SENSE_MASK;
	gpio = intspec[2];
	mode = intspec[3];

	eint = (struct mtk_desc_eint *)&pctl->devdata->pins[gpio].eint;
	eint->eintmux = mode;
	eint->eintnum = intspec[0];

	pr_debug("[pinctrl] mtk_pin[%d], eint=%d, mode=%d\n",
		gpio, eint->eintnum, eint->eintmux);
	return 0;
}
EXPORT_SYMBOL_GPL(mtk_irq_domain_xlate_fourcell);

const struct irq_domain_ops mtk_irq_domain_ops = {
	.xlate = mtk_irq_domain_xlate_fourcell,
};

static const struct mtk_pinctrl_devdata mt6765_pinctrl_data = {
	.pins = mtk_pins_mt6765,
	.npins = ARRAY_SIZE(mtk_pins_mt6765),
	.pin_mode_grps = mtk_pin_info_mode,
	.n_pin_mode = ARRAY_SIZE(mtk_pin_info_mode),
	.pin_drv_grps = mtk_pin_info_drv,
	.n_pin_drv = ARRAY_SIZE(mtk_pin_info_mode),
	.pin_smt_grps = mtk_pin_info_smt,
	.n_pin_smt = ARRAY_SIZE(mtk_pin_info_smt),
	.pin_ies_grps = mtk_pin_info_ies,
	.n_pin_ies = ARRAY_SIZE(mtk_pin_info_ies),
	.pin_pu_grps = mtk_pin_info_pu,
	.n_pin_pu = ARRAY_SIZE(mtk_pin_info_pu),
	.pin_pd_grps = mtk_pin_info_pd,
	.n_pin_pd = ARRAY_SIZE(mtk_pin_info_pd),
	.pin_pupd_grps = mtk_pin_info_pupd,
	.n_pin_pupd = ARRAY_SIZE(mtk_pin_info_pupd),
	.pin_r0_grps = mtk_pin_info_r0,
	.n_pin_r0 = ARRAY_SIZE(mtk_pin_info_r0),
	.pin_r1_grps = mtk_pin_info_r1,
	.n_pin_r1 = ARRAY_SIZE(mtk_pin_info_r1),
	.pin_dout_grps = mtk_pin_info_dataout,
	.n_pin_dout = ARRAY_SIZE(mtk_pin_info_dataout),
	.pin_din_grps = mtk_pin_info_datain,
	.n_pin_din = ARRAY_SIZE(mtk_pin_info_datain),
	.pin_dir_grps = mtk_pin_info_dir,
	.n_pin_dir = ARRAY_SIZE(mtk_pin_info_dir),
	.mtk_pctl_set_pull_sel = mtk_pinctrl_set_gpio_pullsel,
	.mtk_pctl_get_pull_sel = mtk_pinctrl_get_gpio_pullsel,
	.mtk_pctl_get_pull_en = mtk_pinctrl_get_gpio_pullen,
	.spec_debounce_select = mt6765_spec_debounce_select,
	.mtk_irq_domain_ops = &mtk_irq_domain_ops,
	.type1_start = 179,
	.type1_end = 179,
	.regmap_num = 8,
	.port_shf = 4,
	.port_mask = 0xf,
	.port_align = 4,
	.eint_offsets = {
		.name = "mt6765_eint",
		.stat      = 0x000,
		.ack       = 0x040,
		.mask      = 0x080,
		.mask_set  = 0x0c0,
		.mask_clr  = 0x100,
		.sens      = 0x140,
		.sens_set  = 0x180,
		.sens_clr  = 0x1c0,
		.soft      = 0x200,
		.soft_set  = 0x240,
		.soft_clr  = 0x280,
		.pol       = 0x300,
		.pol_set   = 0x340,
		.pol_clr   = 0x380,
		.dom_en    = 0x400,
		.dbnc_ctrl = 0x500,
		.dbnc_set  = 0x600,
		.dbnc_clr  = 0x700,
		.port_mask = 7,
		.ports     = 6,
	},
	.ap_num = 160,
	.db_cnt = 13,
};

static int mtk_pinctrl_probe(struct platform_device *pdev)
{
	int ret = 0;

	pr_info("[%04d][%s]: probe start..\n", __LINE__, __func__);
	ret = mtk_pctrl_init(pdev, &mt6765_pinctrl_data, NULL);
	pr_info("[%04d][%s]: probe end, ret:%d\n", __LINE__, __func__, ret);
	return ret;
}

static const struct of_device_id mtk_pctrl_match[] = {
	{
		.compatible = "mediatek,mt6765-pinctrl",
	}, {
	}
};
MODULE_DEVICE_TABLE(of, mt6765_pctrl_match);

static struct platform_driver mtk_pinctrl_driver = {
	.probe = mtk_pinctrl_probe,
	.driver = {
		.name = "mediatek-mt6765-pinctrl",
		.owner = THIS_MODULE,
		.of_match_table = mtk_pctrl_match,
		.pm = &mtk_eint_pm_ops,
	},
};

static int __init mtk_pinctrl_init(void)
{
	return platform_driver_register(&mtk_pinctrl_driver);
}

/* module_init(mtk_pinctrl_init); */

postcore_initcall(mtk_pinctrl_init);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("MediaTek Pinctrl Driver");
MODULE_AUTHOR("ZH Chen <zh.chen@mediatek.com>");
