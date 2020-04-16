// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018 MediaTek Inc.
 *
 * Author: Sean Wang <sean.wang@mediatek.com>
 *
 */

#include <linux/device.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>

#include <dt-bindings/pinctrl/mt65xx.h>

#include "mtk-eint.h"
#include "pinctrl-mtk-common-v2.h"

/* Some SOC provide more control register other than value register.
 * Generally, a value register need read-modify-write is at offset 0xXXXXXXXX0.
 * A corresponding SET register is at offset 0xXXXXXXX4. Write 1s' to some bits
 *  of SET register will set same bits in value register.
 * A corresponding CLR register is at offset 0xXXXXXXX8. Write 1s' to some bits
 *  of CLR register will clr same bits in value register.
 * For GPIO mode control, MWR register is provided at offset 0xXXXXXXXC.
 *  With MWR, the MSBit of GPIO mode contrl is for modification-enable, not for
 *  GPIO mode selection.
 */

#define SET_OFFSET 0x4
#define CLR_OFFSET 0x8
#define MWR_OFFSET 0xC

/**
 * struct mtk_drive_desc - the structure that holds the information
 *                          of the driving current
 * @min:        the minimum current of this group
 * @max:        the maximum current of this group
 * @step:       the step current of this group
 * @scal:       the weight factor
 *
 * formula: output = ((input) / step - 1) * scal
 */
struct mtk_drive_desc {
	u8 min;
	u8 max;
	u8 step;
	u8 scal;
};

/* The groups of drive strength */
static const struct mtk_drive_desc mtk_drive[] = {
	[DRV_GRP0] = { 4, 16, 4, 1 },
	[DRV_GRP1] = { 4, 16, 4, 2 },
	[DRV_GRP2] = { 2, 8, 2, 1 },
	[DRV_GRP3] = { 2, 8, 2, 2 },
	[DRV_GRP4] = { 2, 16, 2, 1 },
};

static void mtk_w32(struct mtk_pinctrl *pctl, u8 i, u32 reg, u32 val)
{
	writel_relaxed(val, pctl->base[i] + reg);
}

static u32 mtk_r32(struct mtk_pinctrl *pctl, u8 i, u32 reg)
{
	return readl_relaxed(pctl->base[i] + reg);
}

void mtk_rmw(struct mtk_pinctrl *pctl, u8 i, u32 reg, u32 mask, u32 set)
{
	u32 val;

	val = mtk_r32(pctl, i, reg);
	val &= ~mask;
	val |= set;
	mtk_w32(pctl, i, reg, val);
}

void mtk_hw_set_value_race_free(struct mtk_pinctrl *pctl,
		struct mtk_pin_field *pf, u32 value)
{
	unsigned int set, clr;

	set = value & pf->mask;
	clr = (~set) & pf->mask;

	if (set)
		mtk_w32(pctl, pf->index, pf->offset + SET_OFFSET,
			set << pf->bitpos);
	if (clr)
		mtk_w32(pctl, pf->index, pf->offset + CLR_OFFSET,
			clr << pf->bitpos);
}

void mtk_hw_set_mode_race_free(struct mtk_pinctrl *pctl,
		struct mtk_pin_field *pf, u32 value)
{
	unsigned int value_new;

	/* MSB of mask is modification-enable bit, set this bit */
	value_new = 0x8 | value;
	if (value_new == value)
		dev_notice(pctl->dev,
			"invalid mode 0x%x, use it by ignoring MSBit!\n",
			value);
	mtk_w32(pctl, pf->index, pf->offset + MWR_OFFSET,
		value_new << pf->bitpos);
}

static int mtk_hw_pin_field_lookup(struct mtk_pinctrl *hw,
				   const struct mtk_pin_desc *desc,
				   int field, struct mtk_pin_field *pfd)
{
	const struct mtk_pin_field_calc *c, *e;
	const struct mtk_pin_reg_calc *rc;
	u32 bits, found = 0;
	int start = 0, end, check;

	if (hw->soc->reg_cal && hw->soc->reg_cal[field].range) {
		rc = &hw->soc->reg_cal[field];
	} else {
		dev_dbg(hw->dev,
			"Not support field %d for pin %d (%s)\n",
			field, desc->number, desc->name);
		return -ENOTSUPP;
	}

	end = rc->nranges - 1;
	c = rc->range;
	e = c + rc->nranges;

	while (start <= end) {
		check = (start + end) >> 1;
		if (desc->number >= rc->range[check].s_pin
		 && desc->number <= rc->range[check].e_pin) {
			found = 1;
			break;
		} else if (start == end)
			break;
		else if (desc->number < rc->range[check].s_pin)
			end = check - 1;
		else
			start = check + 1;
	}

	if (!found) {
		dev_dbg(hw->dev, "Not support field %d for pin = %d (%s)\n",
			field, desc->number, desc->name);
		return -ENOTSUPP;
	}

	c = rc->range + check;

	if (c->i_base > hw->nbase - 1) {
		dev_err(hw->dev,
			"Invalid base for field %d for pin = %d (%s)\n",
			field, desc->number, desc->name);
		return -EINVAL;
	}

	/* Calculated bits as the overall offset the pin is located at,
	 * if c->fixed is held, that determines the all the pins in the
	 * range use the same field with the s_pin.
	 */
	bits = c->fixed ? c->s_bit : c->s_bit +
	       (desc->number - c->s_pin) * (c->x_bits);

	/* Fill pfd from bits. For example 32-bit register applied is assumed
	 * when c->sz_reg is equal to 32.
	 */
	pfd->index = c->i_base;
	pfd->offset = c->s_addr + c->x_addrs * (bits / c->sz_reg);
	pfd->bitpos = bits % c->sz_reg;
	pfd->mask = (1 << c->x_bits) - 1;

	/* pfd->next is used for indicating that bit wrapping-around happens
	 * which requires the manipulation for bit 0 starting in the next
	 * register to form the complete field read/write.
	 */
	pfd->next = pfd->bitpos + c->x_bits > c->sz_reg ? c->x_addrs : 0;

	return 0;
}

static int mtk_hw_pin_field_get(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				int field, struct mtk_pin_field *pfd)
{
	if (field < 0 || field >= PINCTRL_PIN_REG_MAX) {
		dev_err(hw->dev, "Invalid Field %d\n", field);
		return -EINVAL;
	}

	return mtk_hw_pin_field_lookup(hw, desc, field, pfd);
}

static void mtk_hw_bits_part(struct mtk_pin_field *pf, int *h, int *l)
{
	*l = 32 - pf->bitpos;
	*h = get_count_order(pf->mask) - *l;
}

static void mtk_hw_write_cross_field(struct mtk_pinctrl *hw,
				     struct mtk_pin_field *pf, int value)
{
	int nbits_l, nbits_h;

	mtk_hw_bits_part(pf, &nbits_h, &nbits_l);

	mtk_rmw(hw, pf->index, pf->offset, pf->mask << pf->bitpos,
		(value & pf->mask) << pf->bitpos);

	mtk_rmw(hw, pf->index, pf->offset + pf->next, BIT(nbits_h) - 1,
		(value & pf->mask) >> nbits_l);
}

static void mtk_hw_read_cross_field(struct mtk_pinctrl *hw,
				    struct mtk_pin_field *pf, int *value)
{
	int nbits_l, nbits_h, h, l;

	mtk_hw_bits_part(pf, &nbits_h, &nbits_l);

	l  = (mtk_r32(hw, pf->index, pf->offset)
	      >> pf->bitpos) & (BIT(nbits_l) - 1);
	h  = (mtk_r32(hw, pf->index, pf->offset + pf->next))
	      & (BIT(nbits_h) - 1);

	*value = (h << nbits_l) | l;
}

int mtk_hw_set_value(struct mtk_pinctrl *hw, const struct mtk_pin_desc *desc,
		     int field, int value)
{
	struct mtk_pin_field pf;
	int err;

	err = mtk_hw_pin_field_get(hw, desc, field, &pf);
	if (err)
		return err;

	if (value < 0 || value > pf.mask)
		return -EINVAL;

	if (!pf.next) {
		if (hw->soc->race_free_access) {
			if (field == PINCTRL_PIN_REG_MODE)
				mtk_hw_set_mode_race_free(hw, &pf, value);
			else
				mtk_hw_set_value_race_free(hw, &pf, value);
		} else
			mtk_rmw(hw, pf.index, pf.offset, pf.mask << pf.bitpos,
				(value & pf.mask) << pf.bitpos);
	} else
		mtk_hw_write_cross_field(hw, &pf, value);

	return 0;
}

void mtk_hw_set_value_no_lookup(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				int value, struct mtk_pin_field *pf)
{
	if (value < 0 || value > pf->mask)
		return;

	if (!pf->next)
		mtk_rmw(hw, pf->index, pf->offset, pf->mask << pf->bitpos,
			(value & pf->mask) << pf->bitpos);
	else
		mtk_hw_write_cross_field(hw, pf, value);
}

int mtk_hw_get_value(struct mtk_pinctrl *hw, const struct mtk_pin_desc *desc,
		     int field, int *value)
{
	struct mtk_pin_field pf;
	int err;

	err = mtk_hw_pin_field_get(hw, desc, field, &pf);
	if (err)
		return err;

	if (!pf.next)
		*value = (mtk_r32(hw, pf.index, pf.offset)
			  >> pf.bitpos) & pf.mask;
	else
		mtk_hw_read_cross_field(hw, &pf, value);

	return 0;
}

void mtk_hw_get_value_no_lookup(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				int *value, struct mtk_pin_field *pf)
{
	if (!pf->next)
		*value = (mtk_r32(hw, pf->index, pf->offset)
			  >> pf->bitpos) & pf->mask;
	else
		mtk_hw_read_cross_field(hw, pf, value);
}

void mtk_eh_ctrl(struct mtk_pinctrl *hw, const struct mtk_pin_desc *desc,
		 u16 mode)
{
	const struct mtk_eh_pin_pinmux *p = hw->soc->eh_pin_pinmux;
	u32 val = 0, on = 0;

	while (p->pin != 0xffff) {
		if (desc->number == p->pin) {
			if (mode == p->pinmux) {
				on = 1;
				break;
			} else if (desc->number != (p + 1)->pin) {
				/*
				 * If the target mode does not match
				 * the mode in current entry.
				 *
				 * Check the next entry if the pin
				 * number is the same.
				 * Yes: target pin have more than one
				 *    pinmux shall enable eh. Check the
				 *    next entry.
				 * No: target pin do not have other
				 *    pinmux shall enable eh. Just disable
				 *    the EH function.
				 */
				break;
			}
		}
		/* It is possible that one pin may have more than one pinmux
		 *   that shall enable eh.
		 * Besides, we assume that hw->soc->eh_pin_pinmux is sorted
		 *   according to field 'pin'.
		 * So when desc->number < p->pin, it mean no match will be
		 *   found and we can leave.
		 */
		if (desc->number < p->pin)
			return;

		p++;
	}

	/* If pin not found, just return */
	if (p->pin == 0xffff)
		return;

	(void)mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DRV_EH, &val);
	if (on)
		val |= on;
	else
		val &= 0xfffffffe;
	(void)mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DRV_EH, val);
}

static int mtk_xt_find_eint_num(struct mtk_pinctrl *hw, unsigned long eint_n,
				unsigned int *virt_gpio)
{
	const struct mtk_pin_desc *desc;
	int i = 0;

	desc = (const struct mtk_pin_desc *)hw->soc->pins;

	while (i < hw->soc->npins) {
		if (desc[i].eint.eint_n == eint_n) {
			if (desc[i].funcs[desc[i].eint.eint_m].name == 0)
				*virt_gpio = 1;
			return desc[i].number;
		}
		i++;
	}

	return EINT_NA;
}

static int mtk_xt_get_gpio_n(void *data, unsigned long eint_n,
			     unsigned int *gpio_n,
			     struct gpio_chip **gpio_chip)
{
	struct mtk_pinctrl *hw = (struct mtk_pinctrl *)data;
	const struct mtk_pin_desc *desc;
	unsigned int virt_gpio = 0;

	desc = (const struct mtk_pin_desc *)hw->soc->pins;
	*gpio_chip = &hw->chip;

	/* Be greedy to guess first gpio_n is equal to eint_n */
	if (desc[eint_n].eint.eint_n == eint_n)
		*gpio_n = eint_n;
	else
		*gpio_n = mtk_xt_find_eint_num(hw, eint_n, &virt_gpio);

	if (virt_gpio)
		return EINT_NO_GPIO;

	return *gpio_n == EINT_NA ? -EINVAL : 0;
}

static int mtk_xt_get_gpio_state(void *data, unsigned long eint_n)
{
	struct mtk_pinctrl *hw = (struct mtk_pinctrl *)data;
	const struct mtk_pin_desc *desc;
	struct gpio_chip *gpio_chip;
	unsigned int gpio_n;
	int value, err;

	err = mtk_xt_get_gpio_n(hw, eint_n, &gpio_n, &gpio_chip);
	if (err < 0)
		return err;

	if (err == EINT_NO_GPIO)
		return 0;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio_n];

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DI, &value);
	if (err)
		return err;

	return !!value;
}

static int mtk_xt_set_gpio_as_eint(void *data, unsigned long eint_n)
{
	struct mtk_pinctrl *hw = (struct mtk_pinctrl *)data;
	const struct mtk_pin_desc *desc;
	struct gpio_chip *gpio_chip;
	unsigned int gpio_n;
	int err;

	err = mtk_xt_get_gpio_n(hw, eint_n, &gpio_n, &gpio_chip);
	if (err < 0)
		return err;

	if (err == EINT_NO_GPIO)
		return 0;

	desc = (const struct mtk_pin_desc *)&hw->soc->pins[gpio_n];

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_MODE,
			       desc->eint.eint_m);
	if (err)
		return err;

	if (hw->soc->eh_pin_pinmux)
		mtk_eh_ctrl(hw, desc, desc->eint.eint_m);

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DIR, MTK_INPUT);
	if (err)
		return err;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_SMT, MTK_ENABLE);
	if (err)
		return err;

	return 0;
}

static const struct mtk_eint_xt mtk_eint_xt = {
	.get_gpio_n = mtk_xt_get_gpio_n,
	.get_gpio_state = mtk_xt_get_gpio_state,
	.set_gpio_as_eint = mtk_xt_set_gpio_as_eint,
};

static int mtk_eint_suspend(struct device *device)
{
	struct mtk_pinctrl *pctl = dev_get_drvdata(device);

	return mtk_eint_do_suspend(pctl->eint);
}

static int mtk_eint_resume(struct device *device)
{
	struct mtk_pinctrl *pctl = dev_get_drvdata(device);

	return mtk_eint_do_resume(pctl->eint);
}

const struct dev_pm_ops mtk_eint_pm_ops_v2 = {
	.suspend_noirq = mtk_eint_suspend,
	.resume_noirq = mtk_eint_resume,
};

int mtk_build_eint(struct mtk_pinctrl *hw, struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node, *node;
	struct resource *res;

	if (!IS_ENABLED(CONFIG_EINT_MTK))
		return 0;

	hw->eint = devm_kzalloc(hw->dev, sizeof(*hw->eint), GFP_KERNEL);
	if (!hw->eint)
		return -ENOMEM;

	node = of_parse_phandle(np, "reg_base_eint", 0);
	if (node) {
		hw->eint->base = of_iomap(node, 0);
	} else {
		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
			"eint");
		if (!res) {
			dev_err(&pdev->dev, "Unable to get eint resource\n");
			return -ENODEV;
		}
		hw->eint->base = devm_ioremap_resource(&pdev->dev, res);
	}

	if (IS_ERR(hw->eint->base))
		return PTR_ERR(hw->eint->base);

	hw->eint->irq = irq_of_parse_and_map(np, 0);
	if (!hw->eint->irq)
		return -EINVAL;

	if (!hw->soc->eint_hw)
		return -ENODEV;

	hw->eint->dev = &pdev->dev;
	hw->eint->hw = hw->soc->eint_hw;
	hw->eint->pctl = hw;
	hw->eint->gpio_xlate = &mtk_eint_xt;

	return mtk_eint_do_init(hw->eint);
}

/* Revision 0 */
int mtk_pinconf_bias_disable_set(struct mtk_pinctrl *hw,
				 const struct mtk_pin_desc *desc)
{
	int err;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_PU,
			       MTK_DISABLE);
	if (err)
		return err;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_PD,
			       MTK_DISABLE);
	if (err)
		return err;

	return 0;
}

int mtk_pinconf_bias_disable_get(struct mtk_pinctrl *hw,
				 const struct mtk_pin_desc *desc, int *res)
{
	int v, v2;
	int err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_PU, &v);
	if (err)
		return err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_PD, &v2);
	if (err)
		return err;

	if (v == MTK_ENABLE || v2 == MTK_ENABLE)
		return -EINVAL;

	*res = 1;

	return 0;
}

int mtk_pinconf_bias_set(struct mtk_pinctrl *hw,
			 const struct mtk_pin_desc *desc, bool pullup)
{
	int err, arg;

	arg = pullup ? 1 : 2;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_PU, arg & 1);
	if (err)
		return err;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_PD,
			       !!(arg & 2));
	if (err)
		return err;

	return 0;
}

int mtk_pinconf_bias_get(struct mtk_pinctrl *hw,
			 const struct mtk_pin_desc *desc, bool pullup, int *res)
{
	int reg, err, v;

	reg = pullup ? PINCTRL_PIN_REG_PU : PINCTRL_PIN_REG_PD;

	err = mtk_hw_get_value(hw, desc, reg, &v);
	if (err)
		return err;

	if (!v)
		return -EINVAL;

	*res = 1;

	return 0;
}

/* Revision 1 */
int mtk_pinconf_bias_disable_set_rev1(struct mtk_pinctrl *hw,
				      const struct mtk_pin_desc *desc)
{
	int err;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_PULLEN,
			       MTK_DISABLE);
	if (err)
		return err;

	return 0;
}

int mtk_pinconf_bias_disable_get_rev1(struct mtk_pinctrl *hw,
				      const struct mtk_pin_desc *desc, int *res)
{
	int v, err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_PULLEN, &v);
	if (err)
		return err;

	if (v == MTK_ENABLE)
		return -EINVAL;

	*res = 1;

	return 0;
}

int mtk_pinconf_bias_set_rev1(struct mtk_pinctrl *hw,
			      const struct mtk_pin_desc *desc, bool pullup)
{
	int err, arg;

	arg = pullup ? MTK_PULLUP : MTK_PULLDOWN;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_PULLEN,
			       MTK_ENABLE);
	if (err)
		return err;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_PULLSEL, arg);
	if (err)
		return err;

	return 0;
}

int mtk_pinconf_bias_get_rev1(struct mtk_pinctrl *hw,
			      const struct mtk_pin_desc *desc, bool pullup,
			      int *res)
{
	int err, v;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_PULLEN, &v);
	if (err)
		return err;

	if (v == MTK_DISABLE)
		return -EINVAL;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_PULLSEL, &v);
	if (err)
		return err;

	if (pullup ^ (v == MTK_PULLUP))
		return -EINVAL;

	*res = 1;

	return 0;
}

/* Combo for the following pull register type:
 * 1. PU + PD
 * 2. PULLSEL + PULLEN
 * 3. PUPD + R0 + R1
 */
int mtk_pinconf_bias_set_pu_pd(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				u32 pullup, u32 arg)
{
	struct mtk_pin_field pf;
	int err = -EINVAL;
	int pu, pd;

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PU, &pf);
	if (err)
		goto out;

	if (arg == MTK_DISABLE) {
		pu = 0;
		pd = 0;
	} else if ((arg == MTK_ENABLE) && pullup) {
		pu = 1;
		pd = 0;
	} else if ((arg == MTK_ENABLE) && !pullup) {
		pu = 0;
		pd = 1;
	} else {
		goto out;
	}

	mtk_hw_set_value_no_lookup(hw, desc, pu, &pf);

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PD, &pf);
	if (err)
		goto out;

	mtk_hw_set_value_no_lookup(hw, desc, pd, &pf);

out:
	return err;
}

int mtk_pinconf_bias_set_pullsel_pullen(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				u32 pullup, u32 arg)
{
	struct mtk_pin_field pf;
	int err = -EINVAL, enable;

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PULLEN, &pf);
	if (err)
		goto out;

	if (arg == MTK_DISABLE)
		enable = 0;
	else if (arg == MTK_ENABLE)
		enable = 1;
	else
		goto out;

	mtk_hw_set_value_no_lookup(hw, desc, enable, &pf);

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PULLSEL, &pf);
	if (err)
		goto out;
	mtk_hw_set_value_no_lookup(hw, desc, pullup, &pf);

out:
	return err;
}

int mtk_pinconf_bias_set_pupd_r1_r0(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				u32 pullup, u32 arg)
{
	struct mtk_pin_field pf;
	int err = -EINVAL;
	int r0, r1;

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PUPD, &pf);
	if (err)
		goto out;

	if ((arg == MTK_DISABLE) || (arg == MTK_PUPD_SET_R1R0_00)) {
		pullup = 0;
		r0 = 0;
		r1 = 0;
	} else if (arg == MTK_PUPD_SET_R1R0_01) {
		r0 = 1;
		r1 = 0;
	} else if (arg == MTK_PUPD_SET_R1R0_10) {
		r0 = 0;
		r1 = 1;
	} else if (arg == MTK_PUPD_SET_R1R0_11) {
		r0 = 1;
		r1 = 1;
	} else
		goto out;

	/* MTK HW PUPD bit: 1 for pull-down, 0 for pull-up */
	mtk_hw_set_value_no_lookup(hw, desc, !pullup, &pf);

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_R0, &pf);
	if (err)
		goto out;
	mtk_hw_set_value_no_lookup(hw, desc, r0, &pf);

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_R1, &pf);
	if (err)
		goto out;
	mtk_hw_set_value_no_lookup(hw, desc, r1, &pf);

out:
	return err;
}

int mtk_pinconf_bias_get_pu_pd(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				u32 *pullup, u32 *enable)
{
	struct mtk_pin_field pf;
	int err = -EINVAL;
	int pu, pd;

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PU, &pf);
	if (err)
		goto out;

	mtk_hw_get_value_no_lookup(hw, desc, &pu, &pf);

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PD, &pf);
	if (err)
		goto out;

	mtk_hw_get_value_no_lookup(hw, desc, &pd, &pf);

	if (pu == 0 && pd == 0) {
		*pullup = 0;
		*enable = MTK_DISABLE;
	} else if (pu == 1 && pd == 0) {
		*pullup = 1;
		*enable = MTK_ENABLE;
	} else if (pu == 0 && pd == 1) {
		*pullup = 0;
		*enable = MTK_ENABLE;
	} else {
		err = -EINVAL;
		goto out;
	}

out:
	return err;
}

int mtk_pinconf_bias_get_pullsel_pullen(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				u32 *pullup, u32 *enable)
{
	struct mtk_pin_field pf;
	int err = -EINVAL;

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PULLSEL, &pf);
	if (err)
		goto out;

	mtk_hw_get_value_no_lookup(hw, desc, pullup, &pf);

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PULLEN, &pf);
	if (err)
		goto out;

	mtk_hw_get_value_no_lookup(hw, desc, enable, &pf);

out:
	return err;
}

int mtk_pinconf_bias_get_pupd_r1_r0(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				u32 *pullup, u32 *enable)
{
	struct mtk_pin_field pf;
	int err = -EINVAL;
	int r0, r1;

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_PUPD, &pf);
	if (err)
		goto out;

	/* MTK HW PUPD bit: 1 for pull-down, 0 for pull-up */
	mtk_hw_get_value_no_lookup(hw, desc, pullup, &pf);
	*pullup = !(*pullup);

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_R0, &pf);
	if (err)
		goto out;
	mtk_hw_get_value_no_lookup(hw, desc, &r0, &pf);

	err = mtk_hw_pin_field_lookup(hw, desc, PINCTRL_PIN_REG_R1, &pf);
	if (err)
		goto out;
	mtk_hw_get_value_no_lookup(hw, desc, &r1, &pf);

	if ((r1 == 0) && (r0 == 0))
		*enable = MTK_PUPD_SET_R1R0_00;
	else if ((r1 == 0) && (r0 == 1))
		*enable = MTK_PUPD_SET_R1R0_01;
	else if ((r1 == 1) && (r0 == 0))
		*enable = MTK_PUPD_SET_R1R0_10;
	else if ((r1 == 1) && (r0 == 1))
		*enable = MTK_PUPD_SET_R1R0_11;
	else
		goto out;

out:
	return err;
}

int mtk_pinconf_bias_set_combo(struct mtk_pinctrl *hw,
				const struct mtk_pin_desc *desc,
				u32 pullup, u32 arg)
{
	int err;

	err = mtk_pinconf_bias_set_pu_pd(hw, desc, pullup, arg);
	if (!err)
		goto out;

	err = mtk_pinconf_bias_set_pullsel_pullen(hw, desc, pullup, arg);
	if (!err)
		goto out;

	err = mtk_pinconf_bias_set_pupd_r1_r0(hw, desc, pullup, arg);

out:
	return err;
}

int mtk_pinconf_bias_get_combo(struct mtk_pinctrl *hw,
			      const struct mtk_pin_desc *desc,
			      u32 *pullup, u32 *enable)
{
	int err;

	err = mtk_pinconf_bias_get_pu_pd(hw, desc, pullup, enable);
	if (!err)
		goto out;

	err = mtk_pinconf_bias_get_pullsel_pullen(hw, desc, pullup, enable);
	if (!err)
		goto out;

	err = mtk_pinconf_bias_get_pupd_r1_r0(hw, desc, pullup, enable);

out:
	return err;
}

/* Revision 0 */
int mtk_pinconf_drive_set(struct mtk_pinctrl *hw,
			  const struct mtk_pin_desc *desc, u32 arg)
{
	const struct mtk_drive_desc *tb;
	int err = -ENOTSUPP;

	tb = &mtk_drive[desc->drv_n];
	/* 4mA when (e8, e4) = (0, 0)
	 * 8mA when (e8, e4) = (0, 1)
	 * 12mA when (e8, e4) = (1, 0)
	 * 16mA when (e8, e4) = (1, 1)
	 */
	if ((arg >= tb->min && arg <= tb->max) && !(arg % tb->step)) {
		arg = (arg / tb->step - 1) * tb->scal;
		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_E4,
				       arg & 0x1);
		if (err)
			return err;

		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_E8,
				       (arg & 0x2) >> 1);
		if (err)
			return err;
	}

	return err;
}

int mtk_pinconf_drive_get(struct mtk_pinctrl *hw,
			  const struct mtk_pin_desc *desc, int *val)
{
	const struct mtk_drive_desc *tb;
	int err, val1, val2;

	tb = &mtk_drive[desc->drv_n];

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_E4, &val1);
	if (err)
		return err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_E8, &val2);
	if (err)
		return err;

	/* 4mA when (e8, e4) = (0, 0); 8mA when (e8, e4) = (0, 1)
	 * 12mA when (e8, e4) = (1, 0); 16mA when (e8, e4) = (1, 1)
	 */
	*val = (((val2 << 1) + val1) / tb->scal + 1) * tb->step;

	return 0;
}

/* Revision 1 */
int mtk_pinconf_drive_set_rev1(struct mtk_pinctrl *hw,
			       const struct mtk_pin_desc *desc, u32 arg)
{
	const struct mtk_drive_desc *tb;
	int err = -ENOTSUPP;

	tb = &mtk_drive[desc->drv_n];

	if ((arg >= tb->min && arg <= tb->max) && !(arg % tb->step)) {
		arg = (arg / tb->step - 1) * tb->scal;

		err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DRV,
				       arg);
		if (err)
			return err;
	}

	return err;
}

int mtk_pinconf_drive_get_rev1(struct mtk_pinctrl *hw,
			       const struct mtk_pin_desc *desc, int *val)
{
	const struct mtk_drive_desc *tb;
	int err, val1;

	tb = &mtk_drive[desc->drv_n];

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DRV, &val1);
	if (err)
		return err;

	*val = ((val1 & 0x7) / tb->scal + 1) * tb->step;

	return 0;
}

/* Revision direct value */
int mtk_pinconf_drive_set_direct_val(struct mtk_pinctrl *hw,
			       const struct mtk_pin_desc *desc, u32 arg)
{
	int err;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DRV, arg);

	return err;
}

int mtk_pinconf_drive_get_direct_val(struct mtk_pinctrl *hw,
			       const struct mtk_pin_desc *desc, int *val)
{
	int err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DRV, val);

	return err;
}

int mtk_pinconf_adv_pull_set(struct mtk_pinctrl *hw,
			     const struct mtk_pin_desc *desc, bool pullup,
			     u32 arg)
{
	int err;

	/* 10K off & 50K (75K) off, when (R0, R1) = (0, 0);
	 * 10K off & 50K (75K) on, when (R0, R1) = (0, 1);
	 * 10K on & 50K (75K) off, when (R0, R1) = (1, 0);
	 * 10K on & 50K (75K) on, when (R0, R1) = (1, 1)
	 */
	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_R0, arg & 1);
	if (err)
		return 0;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_R1,
			       !!(arg & 2));
	if (err)
		return 0;

	arg = pullup ? 0 : 1;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_PUPD, arg);

	/* If PUPD register is not supported for that pin, let's fallback to
	 * general bias control.
	 */
	if (err == -ENOTSUPP) {
		if (hw->soc->bias_set_combo) {
			err = hw->soc->bias_set_combo(hw,
				desc, pullup, MTK_ENABLE);
			if (err)
				return err;
		} else if (hw->soc->bias_set) {
			err = hw->soc->bias_set(hw, desc, pullup);
			if (err)
				return err;
		} else {
			return -ENOTSUPP;
		}
	}

	return err;
}

int mtk_pinconf_adv_pull_get(struct mtk_pinctrl *hw,
			     const struct mtk_pin_desc *desc, bool pullup,
			     u32 *val)
{
	u32 t, t2;
	int err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_PUPD, &t);

	/* If PUPD register is not supported for that pin, let's fallback to
	 * general bias control.
	 */
	if (err == -ENOTSUPP) {
		if (hw->soc->bias_get_combo) {
			/* do nothing */
		} else if (hw->soc->bias_get) {
			err = hw->soc->bias_get(hw, desc, pullup, val);
			if (err)
				return err;
		} else {
			return -ENOTSUPP;
		}
	} else {
		/* t == 0 supposes PULLUP for the customized PULL setup */
		if (err)
			return err;

		if (pullup ^ !t)
			return -EINVAL;
	}

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_R0, &t);
	if (err)
		return err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_R1, &t2);
	if (err)
		return err;

	*val = (t | t2 << 1) & 0x7;

	return 0;
}

int mtk_pinconf_adv_drive_set(struct mtk_pinctrl *hw,
			      const struct mtk_pin_desc *desc, u32 arg)
{
	int err;
	int en = arg & 1;
	int e0 = !!(arg & 2);
	int e1 = !!(arg & 4);

	/*
	 * Only one will be exist EH table or EN,E0,E1 table
	 * Check EH table first
	 */
	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DRV_EH, arg);
	if (!err)
		return 0;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DRV_EN, en);
	if (err)
		return err;

	if (!en)
		return err;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DRV_E0, e0);
	if (err)
		return err;

	err = mtk_hw_set_value(hw, desc, PINCTRL_PIN_REG_DRV_E1, e1);
	if (err)
		return err;

	return err;
}

int mtk_pinconf_adv_drive_get(struct mtk_pinctrl *hw,
			      const struct mtk_pin_desc *desc, u32 *val)
{
	u32 en, e0, e1;
	int err;

	/*
	 * Only one will be exist EH table or EN,E0,E1 table
	 * Check EH table first
	 */
	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DRV_EH, val);
	if (!err)
		return 0;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DRV_EN, &en);
	if (err)
		return err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DRV_E0, &e0);
	if (err)
		return err;

	err = mtk_hw_get_value(hw, desc, PINCTRL_PIN_REG_DRV_E1, &e1);
	if (err)
		return err;

	*val = (en | e0 << 1 | e1 << 2) & 0x7;

	return 0;
}
