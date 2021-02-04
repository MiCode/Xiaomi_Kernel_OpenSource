/*
 *  Copyright (C) 2016 Richtek Technology Corp.
 *  jeff_chang <jeff_chang@richtek.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include "../../flashlight/richtek/rtfled.h"

#include "inc/rt5081_pmu.h"
#include "inc/rt5081_pmu_fled.h"

#define RT5081_PMU_FLED_DRV_VERSION	"1.0.2_MTK"

static u8 rt5081_fled_inited;
static u8 rt5081_global_mode = FLASHLIGHT_MODE_OFF;

static u8 rt5081_fled_on;

enum {
	RT5081_FLED1 = 0,
	RT5081_FLED2 = 1,
};

struct rt5081_pmu_fled_data {
	struct rt_fled_dev base;
	struct rt5081_pmu_chip *chip;
	struct device *dev;
	struct platform_device *rt_flash_dev;
	int id;
	unsigned char suspend:1;
	unsigned char fled_ctrl:2; /* fled1, fled2, both */
	unsigned char fled_ctrl_reg;
	unsigned char fled_tor_cur_reg;
	unsigned char fled_strb_cur_reg;
	unsigned char fled_strb_to_reg;
	unsigned char fled_cs_mask;
};

static irqreturn_t rt5081_pmu_fled_strbpin_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled_torpin_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled_tx_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled_lvf_irq_handler(int irq, void *data)
{
	struct rt5081_pmu_fled_data *info = data;

	dev_dbg(info->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled2_short_irq_handler(int irq, void *data)
{
	struct rt5081_pmu_fled_data *info = data;

	dev_dbg(info->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled1_short_irq_handler(int irq, void *data)
{
	struct rt5081_pmu_fled_data *info = data;

	dev_dbg(info->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled2_strb_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled1_strb_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled2_strb_to_irq_handler(int irq, void *data)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)data;

	dev_info(fi->dev, "%s occurred\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled1_strb_to_irq_handler(int irq, void *data)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)data;

	dev_info(fi->dev, "%s occurred\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled2_tor_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t rt5081_pmu_fled1_tor_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static struct rt5081_pmu_irq_desc rt5081_fled_irq_desc[] = {
	RT5081_PMU_IRQDESC(fled_strbpin),
	RT5081_PMU_IRQDESC(fled_torpin),
	RT5081_PMU_IRQDESC(fled_tx),
	RT5081_PMU_IRQDESC(fled_lvf),
	RT5081_PMU_IRQDESC(fled2_short),
	RT5081_PMU_IRQDESC(fled1_short),
	RT5081_PMU_IRQDESC(fled2_strb),
	RT5081_PMU_IRQDESC(fled1_strb),
	RT5081_PMU_IRQDESC(fled2_strb_to),
	RT5081_PMU_IRQDESC(fled1_strb_to),
	RT5081_PMU_IRQDESC(fled2_tor),
	RT5081_PMU_IRQDESC(fled1_tor),
};

static void rt5081_pmu_fled_irq_register(struct platform_device *pdev)
{
	struct resource *res;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(rt5081_fled_irq_desc); i++) {
		if (!rt5081_fled_irq_desc[i].name)
			continue;
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				rt5081_fled_irq_desc[i].name);
		if (!res)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, res->start, NULL,
				rt5081_fled_irq_desc[i].irq_handler,
				IRQF_TRIGGER_FALLING,
				rt5081_fled_irq_desc[i].name,
				platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_dbg(&pdev->dev, "request %s irq fail\n", res->name);
			continue;
		}
		rt5081_fled_irq_desc[i].irq = res->start;
	}
}

static inline int rt5081_fled_parse_dt(struct device *dev,
				struct rt5081_pmu_fled_data *fi)
{
	struct device_node *np = dev->of_node;
	int ret = 0;
	u32 val = 0;
	unsigned char regval;

	pr_info("%s start\n", __func__);
	if (!np) {
		pr_debug("%s cannot rt5081 fled dts node\n", __func__);
		return -ENODEV;
	}

#if 0
	ret = of_property_read_u32(np, "fled_enable", &val);
	if (ret < 0) {
		pr_debug("%s default enable fled%d\n", __func__, fi->id+1);
	} else {
		if (val) {
			pr_debug("%s enable fled%d\n", __func__, fi->id+1);
			rt5081_pmu_reg_set_bit(fi->chip,
				RT5081_PMU_REG_FLEDEN, fi->fled_cs_mask);
		} else {
			pr_debug("%s disable fled%d\n", __func__, fi->id+1);
			rt5081_pmu_reg_clr_bit(fi->chip,
				RT5081_PMU_REG_FLEDEN, fi->fled_cs_mask);
		}
	}
#endif

	ret = of_property_read_u32(np, "torch_cur", &val);
	if (ret < 0)
		pr_debug("%s use default torch cur\n", __func__);
	else {
		pr_info("%s use torch cur %d\n", __func__, val);
		regval = (val > 400000) ? 30 : (val - 25000)/12500;
		rt5081_pmu_reg_update_bits(fi->chip,
				fi->fled_tor_cur_reg,
				RT5081_FLED_TORCHCUR_MASK,
				regval << RT5081_FLED_TORCHCUR_SHIFT);
	}

	ret = of_property_read_u32(np, "strobe_cur", &val);
	if (ret < 0)
		pr_debug("%s use default strobe cur\n", __func__);
	else {
		pr_info("%s use strobe cur %d\n", __func__, val);
		regval = (val > 1500000) ? 112 : (val - 100000)/12500;
		rt5081_pmu_reg_update_bits(fi->chip,
				fi->fled_strb_cur_reg,
				RT5081_FLED_STROBECUR_MASK,
				regval << RT5081_FLED_STROBECUR_SHIFT);
	}

	ret = of_property_read_u32(np, "strobe_timeout", &val);
	if (ret < 0)
		pr_debug("%s use default strobe timeout\n", __func__);
	else {
		pr_info("%s use strobe timeout %d\n", __func__, val);
		regval = (val > 2432) ? 74 : (val - 64)/32;
		rt5081_pmu_reg_update_bits(fi->chip,
				RT5081_PMU_REG_FLEDSTRBCTRL,
				RT5081_FLED_STROBE_TIMEOUT_MASK,
				regval << RT5081_FLED_STROBE_TIMEOUT_SHIFT);
	}
	return 0;
}

static struct flashlight_properties rt5081_fled_props = {
	.type = FLASHLIGHT_TYPE_LED,
	.torch_brightness = 0,
	.torch_max_brightness = 31, /* 0000 ~ 1110 */
	.strobe_brightness = 0,
	.strobe_max_brightness = 256, /* 0000000 ~ 1110000 */
	.strobe_delay = 0,
	.strobe_timeout = 0,
	.alias_name = "rt5081-fled",
};

static int rt5081_fled_reg_init(struct rt5081_pmu_fled_data *info)
{
	/* TBD */
	return 0;
}

static int rt5081_fled_init(struct rt_fled_dev *info)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret = 0;

	ret = rt5081_fled_reg_init(fi);
	if (ret < 0)
		dev_dbg(fi->dev, "init rt5081 fled register fail\n");
	return ret;
}

static int rt5081_fled_suspend(struct rt_fled_dev *info, pm_message_t state)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;

	fi->suspend = 1;
	return 0;
}

static int rt5081_fled_resume(struct rt_fled_dev *info)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;

	fi->suspend = 0;
	return 0;
}

static int rt5081_fled_set_mode(struct rt_fled_dev *info,
					enum flashlight_mode mode)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret = 0;

	switch (mode) {
	case FLASHLIGHT_MODE_TORCH:
		if (rt5081_global_mode == FLASHLIGHT_MODE_FLASH)
			break;
		ret |= rt5081_pmu_reg_clr_bit(fi->chip,
			RT5081_PMU_REG_FLEDEN, RT5081_STROBE_EN_MASK);
		ret |= rt5081_pmu_reg_set_bit(fi->chip,
				RT5081_PMU_REG_FLEDEN, fi->id == RT5081_FLED1 ? 0x02 : 0x01);
		ret |= rt5081_pmu_reg_set_bit(fi->chip,
				RT5081_PMU_REG_FLEDEN, RT5081_TORCH_EN_MASK);
		dev_info(fi->dev, "set to torch mode\n");
		rt5081_global_mode = mode;
		if (fi->id == RT5081_FLED1)
			rt5081_fled_on |= 1 << RT5081_FLED1;
		if (fi->id == RT5081_FLED2)
			rt5081_fled_on |= 1 << RT5081_FLED2;
		break;
	case FLASHLIGHT_MODE_FLASH:
		ret = rt5081_pmu_reg_clr_bit(fi->chip,
			RT5081_PMU_REG_FLEDEN, RT5081_STROBE_EN_MASK);
		udelay(400);
		ret |= rt5081_pmu_reg_set_bit(fi->chip,
			RT5081_PMU_REG_FLEDEN, fi->id == RT5081_FLED1 ? 0x02 : 0x01);
		ret |= rt5081_pmu_reg_set_bit(fi->chip,
			RT5081_PMU_REG_FLEDEN, RT5081_STROBE_EN_MASK);
		udelay(400);
		dev_info(fi->dev, "set to flash mode\n");
		rt5081_global_mode = mode;
		if (fi->id == RT5081_FLED1)
			rt5081_fled_on |= 1 << RT5081_FLED1;
		if (fi->id == RT5081_FLED2)
			rt5081_fled_on |= 1 << RT5081_FLED2;
		break;
	case FLASHLIGHT_MODE_OFF:
		ret = rt5081_pmu_reg_clr_bit(fi->chip,
				RT5081_PMU_REG_FLEDEN,
				fi->id == RT5081_FLED1 ? 0x02 : 0x01);
		dev_info(fi->dev, "set to off mode\n");
		if (fi->id == RT5081_FLED1)
			rt5081_fled_on &= ~(1 << RT5081_FLED1);
		if (fi->id == RT5081_FLED2)
			rt5081_fled_on &= ~(1 << RT5081_FLED2);
		if (rt5081_fled_on == 0)
			rt5081_global_mode = mode;
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static int rt5081_fled_get_mode(struct rt_fled_dev *info)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret;

	if (fi->id == RT5081_FLED2) {
		pr_debug("%s FLED2 not support get mode\n", __func__);
		return 0;
	}

	ret = rt5081_pmu_reg_read(fi->chip, RT5081_PMU_REG_FLEDEN);
	if (ret < 0)
		return -EINVAL;

	if (ret & RT5081_STROBE_EN_MASK)
		return FLASHLIGHT_MODE_FLASH;
	else if (ret & RT5081_TORCH_EN_MASK)
		return FLASHLIGHT_MODE_TORCH;
	else
		return FLASHLIGHT_MODE_OFF;
}

static int rt5081_fled_strobe(struct rt_fled_dev *info)
{
	return rt5081_fled_set_mode(info, FLASHLIGHT_MODE_FLASH);
}

static int rt5081_fled_torch_current_list(
			struct rt_fled_dev *info, int selector)
{

	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;

	return (selector > fi->base.init_props->torch_max_brightness) ?
		-EINVAL : 25000 + selector * 12500;
}

static int rt5081_fled_strobe_current_list(struct rt_fled_dev *info,
							int selector)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;

	return (selector > fi->base.init_props->strobe_max_brightness) ?
		-EINVAL : 100000 + selector * 12500;
}

static unsigned int rt5081_timeout_level_list[] = {
	50000, 75000, 100000, 125000, 150000, 175000, 200000, 200000,
};

static int rt5081_fled_timeout_level_list(struct rt_fled_dev *info,
							int selector)
{
	return (selector >= ARRAY_SIZE(rt5081_timeout_level_list)) ?
		-EINVAL : rt5081_timeout_level_list[selector];
}

static int rt5081_fled_strobe_timeout_list(struct rt_fled_dev *info,
							int selector)
{
	if (selector > 74)
		return -EINVAL;
	return 64 + selector * 32;
}

static int rt5081_fled_set_torch_current_sel(struct rt_fled_dev *info,
								int selector)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret;

	ret = rt5081_pmu_reg_update_bits(fi->chip, fi->fled_tor_cur_reg,
			RT5081_FLED_TORCHCUR_MASK,
			selector << RT5081_FLED_TORCHCUR_SHIFT);

	return ret;
}

static int rt5081_fled_set_strobe_current_sel(struct rt_fled_dev *info,
								int selector)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret;

	if (selector >= 256)
		return -EINVAL;
	if (selector >= 128)
		rt5081_pmu_reg_set_bit(fi->chip, fi->fled_strb_cur_reg, 0x80);
	else
		rt5081_pmu_reg_clr_bit(fi->chip, fi->fled_strb_cur_reg, 0x80);

	if (selector >= 128)
		selector -= 128;
	ret = rt5081_pmu_reg_update_bits(fi->chip, fi->fled_strb_cur_reg,
		RT5081_FLED_STROBECUR_MASK,
		selector << RT5081_FLED_STROBECUR_SHIFT);
	return ret;
}

static int rt5081_fled_set_timeout_level_sel(struct rt_fled_dev *info,
								int selector)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret;

	if (fi->id == RT5081_FLED1) {
		ret = rt5081_pmu_reg_update_bits(fi->chip,
			RT5081_PMU_REG_FLED1CTRL,
			RT5081_FLED_TIMEOUT_LEVEL_MASK,
			selector << RT5081_TIMEOUT_LEVEL_SHIFT);
	} else {
		ret = rt5081_pmu_reg_update_bits(fi->chip,
			RT5081_PMU_REG_FLED2CTRL,
			RT5081_FLED_TIMEOUT_LEVEL_MASK,
			selector << RT5081_TIMEOUT_LEVEL_SHIFT);
	}

	return ret;
}

static int rt5081_fled_set_strobe_timeout_sel(struct rt_fled_dev *info,
							int selector)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret = 0;

	if (fi->id == RT5081_FLED2) {
		pr_debug("%s not support set strobe timeout\n", __func__);
		return -EINVAL;
	}
	ret = rt5081_pmu_reg_update_bits(fi->chip, fi->fled_strb_to_reg,
			RT5081_FLED_STROBE_TIMEOUT_MASK,
			selector << RT5081_FLED_STROBE_TIMEOUT_SHIFT);
	return ret;
}

static int rt5081_fled_get_torch_current_sel(struct rt_fled_dev *info)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret = 0;

	ret = rt5081_pmu_reg_read(fi->chip, fi->fled_tor_cur_reg);
	if (ret < 0) {
		pr_debug("%s get fled tor current sel fail\n", __func__);
		return ret;
	}

	ret &= RT5081_FLED_TORCHCUR_MASK;
	ret >>= RT5081_FLED_TORCHCUR_SHIFT;
	return ret;
}

static int rt5081_fled_get_strobe_current_sel(struct rt_fled_dev *info)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret = 0;

	ret = rt5081_pmu_reg_read(fi->chip, fi->fled_strb_cur_reg);
	if (ret < 0) {
		pr_debug("%s get fled strobe curr sel fail\n", __func__);
		return ret;
	}

	ret &= RT5081_FLED_STROBECUR_MASK;
	ret >>= RT5081_FLED_STROBECUR_SHIFT;
	return ret;
}

static int rt5081_fled_get_timeout_level_sel(struct rt_fled_dev *info)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret = 0;

	if (fi->id == RT5081_FLED1)
		ret = rt5081_pmu_reg_read(fi->chip, RT5081_PMU_REG_FLED1CTRL);
	else
		ret = rt5081_pmu_reg_read(fi->chip, RT5081_PMU_REG_FLED2CTRL);
	if (ret < 0) {
		pr_debug("%s get fled timeout level fail\n", __func__);
		return ret;
	}

	ret &= RT5081_FLED_TIMEOUT_LEVEL_MASK;
	ret >>= RT5081_TIMEOUT_LEVEL_SHIFT;
	return ret;
}

static int rt5081_fled_get_strobe_timeout_sel(struct rt_fled_dev *info)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret = 0;

	ret = rt5081_pmu_reg_read(fi->chip, fi->fled_strb_to_reg);
	if (ret < 0) {
		pr_debug("%s get fled timeout level fail\n", __func__);
		return ret;
	}

	ret &= RT5081_FLED_STROBE_TIMEOUT_MASK;
	ret >>= RT5081_FLED_STROBE_TIMEOUT_SHIFT;
	return ret;
}

static void rt5081_fled_shutdown(struct rt_fled_dev *info)
{
	rt5081_fled_set_mode(info, FLASHLIGHT_MODE_OFF);
}

static int rt5081_fled_is_ready(struct rt_fled_dev *info)
{
	struct rt5081_pmu_fled_data *fi = (struct rt5081_pmu_fled_data *)info;
	int ret;

	ret = rt5081_pmu_reg_read(fi->chip, RT5081_PMU_REG_CHGSTAT2);
	if (ret < 0) {
		pr_debug("%s read flash ready bit fail\n", __func__);
		return 0;
	}

	/* CHG_STAT2 bit[3] CHG_VINOVP,
	 * if OVP = 0 --> V < 5.3, if OVP = 1, V > 5.6
	 */
	return  ret & 0x08 ? 0 : 1;
}

static struct rt_fled_hal rt5081_fled_hal = {
	.rt_hal_fled_init = rt5081_fled_init,
	.rt_hal_fled_suspend = rt5081_fled_suspend,
	.rt_hal_fled_resume = rt5081_fled_resume,
	.rt_hal_fled_set_mode = rt5081_fled_set_mode,
	.rt_hal_fled_get_mode = rt5081_fled_get_mode,
	.rt_hal_fled_strobe = rt5081_fled_strobe,
	.rt_hal_fled_get_is_ready = rt5081_fled_is_ready,
	.rt_hal_fled_torch_current_list = rt5081_fled_torch_current_list,
	.rt_hal_fled_strobe_current_list = rt5081_fled_strobe_current_list,
	.rt_hal_fled_timeout_level_list = rt5081_fled_timeout_level_list,
	/* .rt_hal_fled_lv_protection_list = rt5081_fled_lv_protection_list, */
	.rt_hal_fled_strobe_timeout_list = rt5081_fled_strobe_timeout_list,
	/* method to set */
	.rt_hal_fled_set_torch_current_sel =
					rt5081_fled_set_torch_current_sel,
	.rt_hal_fled_set_strobe_current_sel =
					rt5081_fled_set_strobe_current_sel,
	.rt_hal_fled_set_timeout_level_sel =
					rt5081_fled_set_timeout_level_sel,
	.rt_hal_fled_set_strobe_timeout_sel =
					rt5081_fled_set_strobe_timeout_sel,

	/* method to get */
	.rt_hal_fled_get_torch_current_sel =
					rt5081_fled_get_torch_current_sel,
	.rt_hal_fled_get_strobe_current_sel =
					rt5081_fled_get_strobe_current_sel,
	.rt_hal_fled_get_timeout_level_sel =
					rt5081_fled_get_timeout_level_sel,
	.rt_hal_fled_get_strobe_timeout_sel =
					rt5081_fled_get_strobe_timeout_sel,
	/* PM shutdown, optional */
	.rt_hal_fled_shutdown = rt5081_fled_shutdown,
};

#define RT5081_FLED_TOR_CUR0	RT5081_PMU_REG_FLED1TORCTRL
#define RT5081_FLED_TOR_CUR1	RT5081_PMU_REG_FLED2TORCTRL
#define RT5081_FLED_STRB_CUR0	RT5081_PMU_REG_FLED1STRBCTRL
#define RT5081_FLED_STRB_CUR1	RT5081_PMU_REG_FLED2STRBCTRL2
#define RT5081_FLED_CS_MASK0	0x02
#define RT5081_FLED_CS_MASK1	0x01


#define RT5081_FLED_DEVICE(_id)			\
{						\
	.id = _id,				\
	.fled_ctrl_reg = RT5081_PMU_REG_FLEDEN,	\
	.fled_tor_cur_reg = RT5081_FLED_TOR_CUR##_id,	\
	.fled_strb_cur_reg = RT5081_FLED_STRB_CUR##_id,	\
	.fled_strb_to_reg = RT5081_PMU_REG_FLEDSTRBCTRL,	\
	.fled_cs_mask = RT5081_FLED_CS_MASK##_id,		\
}

static struct rt5081_pmu_fled_data rt5081_pmu_fleds[] = {
	RT5081_FLED_DEVICE(0),
	RT5081_FLED_DEVICE(1),
};

static struct rt5081_pmu_fled_data *rt5081_find_info(int id)
{
	struct rt5081_pmu_fled_data *fi;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(rt5081_pmu_fleds); i++) {
		fi = &rt5081_pmu_fleds[i];
		if (fi->id == id)
			return fi;
	}
	return NULL;
}

static int rt5081_pmu_fled_probe(struct platform_device *pdev)
{
	struct rt5081_pmu_fled_data *fled_data;
	bool use_dt = pdev->dev.of_node;
	int ret;

	pr_info("%s (%s) id = %d\n", __func__, RT5081_PMU_FLED_DRV_VERSION, pdev->id);
	fled_data = rt5081_find_info(pdev->id);
	if (fled_data == NULL) {
		dev_dbg(&pdev->dev, "invalid fled ID Specified\n");
		return -EINVAL;
	}

	fled_data->chip = dev_get_drvdata(pdev->dev.parent);
	fled_data->dev = &pdev->dev;

	if (use_dt)
		rt5081_fled_parse_dt(&pdev->dev, fled_data);
	platform_set_drvdata(pdev, fled_data);

	fled_data->base.init_props = &rt5081_fled_props;
	fled_data->base.hal = &rt5081_fled_hal;
	if (pdev->id == 0)
		fled_data->base.name = "rt-flash-led1";
	else
		fled_data->base.name = "rt-flash-led2";
	fled_data->base.chip_name = "rt5081_pmu_fled";
	pr_info("%s flash name = %s\n", __func__, fled_data->base.name);
	fled_data->rt_flash_dev = platform_device_register_resndata(
			fled_data->dev, "rt-flash-led",
			fled_data->id, NULL, 0, NULL, 0);

	if (!rt5081_fled_inited) {
		ret = rt5081_pmu_reg_clr_bit(fled_data->chip,
			RT5081_PMU_REG_FLEDVMIDTRKCTRL1,
			RT5081_FLED_FIXED_MODE_MASK);
		if (ret < 0) {
			pr_debug("%s set fled fixed mode fail\n", __func__);
			return -EINVAL;
		}

		rt5081_pmu_fled_irq_register(pdev);
		rt5081_fled_inited = 1;
		dev_info(&pdev->dev, "rt5081 fled inited\n");
	}
	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;
}

static int rt5081_pmu_fled_remove(struct platform_device *pdev)
{
	struct rt5081_pmu_fled_data *fled_data = platform_get_drvdata(pdev);

	platform_device_unregister(fled_data->rt_flash_dev);
	dev_info(fled_data->dev, "%s successfully\n", __func__);
	return 0;
}

static const struct of_device_id rt_ofid_table[] = {
	{ .compatible = "richtek,rt5081_pmu_fled1", },
	{ .compatible = "richtek,rt5081_pmu_fled2", },
	{ },
};
MODULE_DEVICE_TABLE(of, rt_ofid_table);

#if 0
static const struct platform_device_id rt_id_table[] = {
	{ "rt5081_pmu_fled1", 0},
	{ "rt5081_pmu_fled2", 0},
	{ },
};
MODULE_DEVICE_TABLE(platform, rt_id_table);
#endif

static struct platform_driver rt5081_pmu_fled = {
	.driver = {
		.name = "rt5081_pmu_fled",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt_ofid_table),
	},
	.probe = rt5081_pmu_fled_probe,
	.remove = rt5081_pmu_fled_remove,
	/*.id_table = rt_id_table, */
};
module_platform_driver(rt5081_pmu_fled);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("jeff_chang <jeff_chang@richtek.com>");
MODULE_DESCRIPTION("Richtek RT5081 PMU Fled");
MODULE_VERSION(RT5081_PMU_FLED_DRV_VERSION);

/*
 * Version Note
 * 1.0.2_MTK
 * (1) Add delay for strobe on/off
 *
 * 1.0.1_MTK
 * (1) Remove typedef
 *
 * 1.0.0_MTK
 * (1) Initial Release
 */
