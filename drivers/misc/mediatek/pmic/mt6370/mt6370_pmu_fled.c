// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/delay.h>
#include "../../flashlight/richtek/rtfled.h"

#include "inc/mt6370_pmu.h"
#include "inc/mt6370_pmu_fled.h"
#include "inc/mt6370_pmu_charger.h"

#define MT6370_PMU_FLED_DRV_VERSION	"1.0.3_MTK"

static DEFINE_MUTEX(fled_lock);

static u8 mt6370_fled_inited;
static u8 mt6370_global_mode = FLASHLIGHT_MODE_OFF;

static u8 mt6370_fled_on;

enum {
	MT6370_FLED1 = 0,
	MT6370_FLED2 = 1,
};

struct mt6370_pmu_fled_data {
	struct rt_fled_dev base;
	struct mt6370_pmu_chip *chip;
	struct device *dev;
	struct platform_device *mt_flash_dev;
	int id;
	unsigned char suspend:1;
	unsigned char fled_ctrl:2; /* fled1, fled2, both */
	unsigned char fled_ctrl_reg;
	unsigned char fled_tor_cur_reg;
	unsigned char fled_strb_cur_reg;
	unsigned char fled_strb_to_reg;
	unsigned char fled_cs_mask;
};

static const char *flashlight_mode_str[FLASHLIGHT_MODE_MAX] = {
	"off", "torch", "flash", "mixed",
	"dual flash", "dual torch", "dual off",
};

static irqreturn_t mt6370_pmu_fled_strbpin_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled_torpin_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled_tx_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled_lvf_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_fled_data *info = data;

	dev_notice(info->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled2_short_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_fled_data *info = data;

	dev_notice(info->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled1_short_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_fled_data *info = data;

	dev_notice(info->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled2_strb_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled1_strb_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled2_strb_to_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)data;

	dev_info(fi->dev, "%s occurred\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled1_strb_to_irq_handler(int irq, void *data)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)data;

	dev_info(fi->dev, "%s occurred\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled2_tor_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static irqreturn_t mt6370_pmu_fled1_tor_irq_handler(int irq, void *data)
{
	return IRQ_HANDLED;
}

static struct mt6370_pmu_irq_desc mt6370_fled_irq_desc[] = {
	MT6370_PMU_IRQDESC(fled_strbpin),
	MT6370_PMU_IRQDESC(fled_torpin),
	MT6370_PMU_IRQDESC(fled_tx),
	MT6370_PMU_IRQDESC(fled_lvf),
	MT6370_PMU_IRQDESC(fled2_short),
	MT6370_PMU_IRQDESC(fled1_short),
	MT6370_PMU_IRQDESC(fled2_strb),
	MT6370_PMU_IRQDESC(fled1_strb),
	MT6370_PMU_IRQDESC(fled2_strb_to),
	MT6370_PMU_IRQDESC(fled1_strb_to),
	MT6370_PMU_IRQDESC(fled2_tor),
	MT6370_PMU_IRQDESC(fled1_tor),
};

static void mt6370_pmu_fled_irq_register(struct platform_device *pdev)
{
	struct resource *res;
	int i, ret = 0;

	for (i = 0; i < ARRAY_SIZE(mt6370_fled_irq_desc); i++) {
		if (!mt6370_fled_irq_desc[i].name)
			continue;
		res = platform_get_resource_byname(pdev, IORESOURCE_IRQ,
				mt6370_fled_irq_desc[i].name);
		if (!res)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, res->start, NULL,
				mt6370_fled_irq_desc[i].irq_handler,
				IRQF_TRIGGER_FALLING,
				mt6370_fled_irq_desc[i].name,
				platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_err(&pdev->dev, "request %s irq fail\n", res->name);
			continue;
		}
		mt6370_fled_irq_desc[i].irq = res->start;
	}
}

static inline int mt6370_fled_parse_dt(struct device *dev,
				struct mt6370_pmu_fled_data *fi)
{
	struct device_node *np = dev->of_node;
	int ret = 0;
	u32 val = 0;
	unsigned char regval;

	pr_info("%s start\n", __func__);
	if (!np) {
		pr_err("%s cannot mt6370 fled dts node\n", __func__);
		return -ENODEV;
	}

	ret = of_property_read_u32(np, "torch_cur", &val);
	if (ret < 0)
		pr_err("%s use default torch cur\n", __func__);
	else {
		pr_info("%s use torch cur %d\n", __func__, val);
		regval = (val > 400000) ? 30 : (val - 25000)/12500;
		mt6370_pmu_reg_update_bits(fi->chip,
				fi->fled_tor_cur_reg,
				MT6370_FLED_TORCHCUR_MASK,
				regval << MT6370_FLED_TORCHCUR_SHIFT);
	}

	ret = of_property_read_u32(np, "strobe_cur", &val);
	if (ret < 0)
		pr_err("%s use default strobe cur\n", __func__);
	else {
		pr_info("%s use strobe cur %d\n", __func__, val);
		regval = (val > 1500000) ? 112 : (val - 100000)/12500;
		mt6370_pmu_reg_update_bits(fi->chip,
				fi->fled_strb_cur_reg,
				MT6370_FLED_STROBECUR_MASK,
				regval << MT6370_FLED_STROBECUR_SHIFT);
	}

	ret = of_property_read_u32(np, "strobe_timeout", &val);
	if (ret < 0)
		pr_err("%s use default strobe timeout\n", __func__);
	else {
		pr_info("%s use strobe timeout %d\n", __func__, val);
		regval = (val > 2432) ? 74 : (val - 64)/32;
		mt6370_pmu_reg_update_bits(fi->chip,
				MT6370_PMU_REG_FLEDSTRBCTRL,
				MT6370_FLED_STROBE_TIMEOUT_MASK,
				regval << MT6370_FLED_STROBE_TIMEOUT_SHIFT);
	}
	return 0;
}

static struct flashlight_properties mt6370_fled_props = {
	.type = FLASHLIGHT_TYPE_LED,
	.torch_brightness = 0,
	.torch_max_brightness = 30, /* 00000 ~ 11110 */
	.strobe_brightness = 0,
	.strobe_max_brightness = 255, /* 0000000 ~ 1111111 */
	.strobe_delay = 0,
	.strobe_timeout = 0,
	.alias_name = "mt6370-fled",
};

static int mt6370_fled_reg_init(struct mt6370_pmu_fled_data *info)
{
	/* TBD */
	return 0;
}

static int mt6370_fled_init(struct rt_fled_dev *info)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret = 0;

	ret = mt6370_fled_reg_init(fi);
	if (ret < 0)
		dev_err(fi->dev, "init mt6370 fled register fail\n");
	return ret;
}

static int mt6370_fled_suspend(struct rt_fled_dev *info, pm_message_t state)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;

	fi->suspend = 1;
	return 0;
}

static int mt6370_fled_resume(struct rt_fled_dev *info)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;

	fi->suspend = 0;
	return 0;
}

static inline int mt6370_pmu_reg_test_bit(
	struct mt6370_pmu_chip *chip, u8 cmd, u8 shift, bool *is_one)
{
	int ret = 0;
	u8 data = 0;

	ret = mt6370_pmu_reg_read(chip, cmd);
	if (ret < 0) {
		*is_one = false;
		return ret;
	}

	data = ret & (1 << shift);
	*is_one = (data == 0 ? false : true);

	return 0;
}

static int mt6370_fled_set_mode(struct rt_fled_dev *info,
					enum flashlight_mode mode)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret = 0;
	u8 val, mask;
	bool hz_en = false, cfo_en = true;

	switch (mode) {
	case FLASHLIGHT_MODE_FLASH:
	case FLASHLIGHT_MODE_DUAL_FLASH:
		ret = mt6370_pmu_reg_test_bit(fi->chip, MT6370_PMU_REG_CHGCTRL1,
				MT6370_SHIFT_HZ_EN, &hz_en);
		if (ret >= 0 && hz_en) {
			dev_err(fi->dev, "%s WARNING\n", __func__);
			dev_err(fi->dev, "%s set %s mode with HZ=1\n",
					 __func__, flashlight_mode_str[mode]);
		}

		ret = mt6370_pmu_reg_test_bit(fi->chip, MT6370_PMU_REG_CHGCTRL2,
				MT6370_SHIFT_CFO_EN, &cfo_en);
		if (ret >= 0 && !cfo_en) {
			dev_err(fi->dev, "%s WARNING\n", __func__);
			dev_err(fi->dev, "%s set %s mode with CFO=0\n",
					 __func__, flashlight_mode_str[mode]);
		}
		break;
	default:
		break;
	}

	mutex_lock(&fled_lock);
	switch (mode) {
	case FLASHLIGHT_MODE_TORCH:
		if (mt6370_global_mode == FLASHLIGHT_MODE_FLASH)
			break;
		ret |= mt6370_pmu_reg_clr_bit(fi->chip,
			MT6370_PMU_REG_FLEDEN, MT6370_STROBE_EN_MASK);
		udelay(500);
		ret |= mt6370_pmu_reg_set_bit(fi->chip, MT6370_PMU_REG_FLEDEN,
				fi->id == MT6370_FLED1 ? 0x02 : 0x01);
		ret |= mt6370_pmu_reg_set_bit(fi->chip,
				MT6370_PMU_REG_FLEDEN, MT6370_TORCH_EN_MASK);
		udelay(500);
		dev_info(fi->dev, "set to torch mode with 500 us delay\n");
		mt6370_global_mode = mode;
		if (fi->id == MT6370_FLED1)
			mt6370_fled_on |= 1 << MT6370_FLED1;
		if (fi->id == MT6370_FLED2)
			mt6370_fled_on |= 1 << MT6370_FLED2;
		break;
	case FLASHLIGHT_MODE_FLASH:
		ret = mt6370_pmu_reg_clr_bit(fi->chip,
			MT6370_PMU_REG_FLEDEN, MT6370_STROBE_EN_MASK);
		udelay(400);
		ret |= mt6370_pmu_reg_set_bit(fi->chip, MT6370_PMU_REG_FLEDEN,
			fi->id == MT6370_FLED1 ? 0x02 : 0x01);
		ret |= mt6370_pmu_reg_set_bit(fi->chip,
			MT6370_PMU_REG_FLEDEN, MT6370_STROBE_EN_MASK);
		mdelay(5);
		dev_info(fi->dev, "set to flash mode with 400/4500 us delay\n");
		mt6370_global_mode = mode;
		if (fi->id == MT6370_FLED1)
			mt6370_fled_on |= 1 << MT6370_FLED1;
		if (fi->id == MT6370_FLED2)
			mt6370_fled_on |= 1 << MT6370_FLED2;
		break;
	case FLASHLIGHT_MODE_OFF:
		ret = mt6370_pmu_reg_clr_bit(fi->chip,
				MT6370_PMU_REG_FLEDEN,
				fi->id == MT6370_FLED1 ? 0x02 : 0x01);
		dev_info(fi->dev, "set to off mode\n");
		if (fi->id == MT6370_FLED1)
			mt6370_fled_on &= ~(1 << MT6370_FLED1);
		if (fi->id == MT6370_FLED2)
			mt6370_fled_on &= ~(1 << MT6370_FLED2);
		if (mt6370_fled_on == 0)
			mt6370_global_mode = mode;
		break;
	case FLASHLIGHT_MODE_DUAL_FLASH:
		if (fi->id == MT6370_FLED2)
			goto out;
		/* strobe off */
		ret = mt6370_pmu_reg_clr_bit(fi->chip, MT6370_PMU_REG_FLEDEN,
					     MT6370_STROBE_EN_MASK);
		if (ret < 0)
			break;
		udelay(400);
		/* fled en/strobe on */
		val = BIT(MT6370_FLED1) | BIT(MT6370_FLED2) |
			MT6370_STROBE_EN_MASK;
		mask = val;
		ret = mt6370_pmu_reg_update_bits(fi->chip,
						 MT6370_PMU_REG_FLEDEN,
						 mask, val);
		if (ret < 0)
			break;
		mt6370_global_mode = mode;
		mt6370_fled_on |= (BIT(MT6370_FLED1) | BIT(MT6370_FLED2));
		break;
	case FLASHLIGHT_MODE_DUAL_TORCH:
		if (fi->id == MT6370_FLED2)
			goto out;
		if (mt6370_global_mode == FLASHLIGHT_MODE_FLASH ||
		    mt6370_global_mode == FLASHLIGHT_MODE_DUAL_FLASH)
			goto out;
		/* Fled en/Strobe off/Torch on */
		ret = mt6370_pmu_reg_clr_bit(fi->chip, MT6370_PMU_REG_FLEDEN,
					     MT6370_STROBE_EN_MASK);
		if (ret < 0)
			break;
		udelay(500);
		val = BIT(MT6370_FLED1) | BIT(MT6370_FLED2) |
			MT6370_TORCH_EN_MASK;
		ret = mt6370_pmu_reg_set_bit(fi->chip,
					     MT6370_PMU_REG_FLEDEN, val);
		if (ret < 0)
			break;
		udelay(500);
		mt6370_global_mode = mode;
		mt6370_fled_on |= (BIT(MT6370_FLED1) | BIT(MT6370_FLED2));
		break;
	case FLASHLIGHT_MODE_DUAL_OFF:
		if (fi->id == MT6370_FLED2)
			goto out;
		ret = mt6370_pmu_reg_clr_bit(fi->chip, MT6370_PMU_REG_FLEDEN,
					 BIT(MT6370_FLED1) | BIT(MT6370_FLED2));
		if (ret < 0)
			break;
		mt6370_fled_on = 0;
		mt6370_global_mode = FLASHLIGHT_MODE_OFF;
		break;
	default:
		mutex_unlock(&fled_lock);
		return -EINVAL;
	}
	if (ret < 0)
		dev_info(fi->dev, "%s set %s mode fail\n", __func__,
			 flashlight_mode_str[mode]);
	else
		dev_info(fi->dev, "%s set %s\n", __func__,
			 flashlight_mode_str[mode]);
out:
	mutex_unlock(&fled_lock);
	return ret;
}

static int mt6370_fled_get_mode(struct rt_fled_dev *info)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret;

	if (fi->id == MT6370_FLED2) {
		pr_err("%s FLED2 not support get mode\n", __func__);
		return 0;
	}

	ret = mt6370_pmu_reg_read(fi->chip, MT6370_PMU_REG_FLEDEN);
	if (ret < 0)
		return -EINVAL;

	if (ret & MT6370_STROBE_EN_MASK)
		return FLASHLIGHT_MODE_FLASH;
	else if (ret & MT6370_TORCH_EN_MASK)
		return FLASHLIGHT_MODE_TORCH;
	else
		return FLASHLIGHT_MODE_OFF;
}

static int mt6370_fled_strobe(struct rt_fled_dev *info)
{
	return mt6370_fled_set_mode(info, FLASHLIGHT_MODE_FLASH);
}

static int mt6370_fled_torch_current_list(
			struct rt_fled_dev *info, int selector)
{

	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;

	return (selector > fi->base.init_props->torch_max_brightness) ?
		-EINVAL : 25000 + selector * 12500;
}

static int mt6370_fled_strobe_current_list(struct rt_fled_dev *info,
							int selector)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;

	if (selector > fi->base.init_props->strobe_max_brightness)
		return -EINVAL;
	if (selector < 128)
		return 50000 + selector * 12500;
	else
		return 25000 + (selector - 128) * 6250;
}

static unsigned int mt6370_timeout_level_list[] = {
	50000, 75000, 100000, 125000, 150000, 175000, 200000, 200000,
};

static int mt6370_fled_timeout_level_list(struct rt_fled_dev *info,
							int selector)
{
	return (selector >= ARRAY_SIZE(mt6370_timeout_level_list)) ?
		-EINVAL : mt6370_timeout_level_list[selector];
}

static int mt6370_fled_strobe_timeout_list(struct rt_fled_dev *info,
							int selector)
{
	if (selector > 74)
		return -EINVAL;
	return 64 + selector * 32;
}

static int mt6370_fled_set_torch_current_sel(struct rt_fled_dev *info,
								int selector)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret;

	ret = mt6370_pmu_reg_update_bits(fi->chip, fi->fled_tor_cur_reg,
			MT6370_FLED_TORCHCUR_MASK,
			selector << MT6370_FLED_TORCHCUR_SHIFT);

	return ret;
}

static int mt6370_fled_set_strobe_current_sel(struct rt_fled_dev *info,
								int selector)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret;

	if (selector > fi->base.init_props->strobe_max_brightness)
		return -EINVAL;
	if (selector < 128)
		mt6370_pmu_reg_clr_bit(fi->chip, fi->fled_strb_cur_reg, 0x80);
	else
		mt6370_pmu_reg_set_bit(fi->chip, fi->fled_strb_cur_reg, 0x80);

	if (selector >= 128)
		selector -= 128;
	ret = mt6370_pmu_reg_update_bits(fi->chip,
					 fi->fled_strb_cur_reg,
					 MT6370_FLED_STROBECUR_MASK,
				       selector << MT6370_FLED_STROBECUR_SHIFT);
	return ret;
}

static int mt6370_fled_set_timeout_level_sel(struct rt_fled_dev *info,
								int selector)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret;

	if (fi->id == MT6370_FLED1) {
		ret = mt6370_pmu_reg_update_bits(fi->chip,
			MT6370_PMU_REG_FLED1CTRL,
			MT6370_FLED_TIMEOUT_LEVEL_MASK,
			selector << MT6370_TIMEOUT_LEVEL_SHIFT);
	} else {
		ret = mt6370_pmu_reg_update_bits(fi->chip,
			MT6370_PMU_REG_FLED2CTRL,
			MT6370_FLED_TIMEOUT_LEVEL_MASK,
			selector << MT6370_TIMEOUT_LEVEL_SHIFT);
	}

	return ret;
}

static int mt6370_fled_set_strobe_timeout_sel(struct rt_fled_dev *info,
							int selector)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret = 0;

	if (fi->id == MT6370_FLED2) {
		pr_err("%s not support set strobe timeout\n", __func__);
		return -EINVAL;
	}
	ret = mt6370_pmu_reg_update_bits(fi->chip, fi->fled_strb_to_reg,
			MT6370_FLED_STROBE_TIMEOUT_MASK,
			selector << MT6370_FLED_STROBE_TIMEOUT_SHIFT);
	return ret;
}

static int mt6370_fled_get_torch_current_sel(struct rt_fled_dev *info)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret = 0;

	ret = mt6370_pmu_reg_read(fi->chip, fi->fled_tor_cur_reg);
	if (ret < 0) {
		pr_err("%s get fled tor current sel fail\n", __func__);
		return ret;
	}

	ret &= MT6370_FLED_TORCHCUR_MASK;
	ret >>= MT6370_FLED_TORCHCUR_SHIFT;
	return ret;
}

static int mt6370_fled_get_strobe_current_sel(struct rt_fled_dev *info)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret = 0;

	ret = mt6370_pmu_reg_read(fi->chip, fi->fled_strb_cur_reg);
	if (ret < 0) {
		pr_err("%s get fled strobe curr sel fail\n", __func__);
		return ret;
	}

	ret &= MT6370_FLED_STROBECUR_MASK;
	ret >>= MT6370_FLED_STROBECUR_SHIFT;
	return ret;
}

static int mt6370_fled_get_timeout_level_sel(struct rt_fled_dev *info)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret = 0;

	if (fi->id == MT6370_FLED1)
		ret = mt6370_pmu_reg_read(fi->chip, MT6370_PMU_REG_FLED1CTRL);
	else
		ret = mt6370_pmu_reg_read(fi->chip, MT6370_PMU_REG_FLED2CTRL);
	if (ret < 0) {
		pr_err("%s get fled timeout level fail\n", __func__);
		return ret;
	}

	ret &= MT6370_FLED_TIMEOUT_LEVEL_MASK;
	ret >>= MT6370_TIMEOUT_LEVEL_SHIFT;
	return ret;
}

static int mt6370_fled_get_strobe_timeout_sel(struct rt_fled_dev *info)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret = 0;

	ret = mt6370_pmu_reg_read(fi->chip, fi->fled_strb_to_reg);
	if (ret < 0) {
		pr_err("%s get fled timeout level fail\n", __func__);
		return ret;
	}

	ret &= MT6370_FLED_STROBE_TIMEOUT_MASK;
	ret >>= MT6370_FLED_STROBE_TIMEOUT_SHIFT;
	return ret;
}

static void mt6370_fled_shutdown(struct rt_fled_dev *info)
{
	mt6370_fled_set_mode(info, FLASHLIGHT_MODE_OFF);
}

static int mt6370_fled_is_ready(struct rt_fled_dev *info)
{
	struct mt6370_pmu_fled_data *fi = (struct mt6370_pmu_fled_data *)info;
	int ret;

	ret = mt6370_pmu_reg_read(fi->chip, MT6370_PMU_REG_CHGSTAT2);
	if (ret < 0) {
		pr_err("%s read flash ready bit fail\n", __func__);
		return 0;
	}

	/* CHG_STAT2 bit[3] CHG_VINOVP,
	 * if OVP = 0 --> V < 5.3, if OVP = 1, V > 5.6
	 */
	return  ret & 0x08 ? 0 : 1;
}

static struct rt_fled_hal mt6370_fled_hal = {
	.rt_hal_fled_init = mt6370_fled_init,
	.rt_hal_fled_suspend = mt6370_fled_suspend,
	.rt_hal_fled_resume = mt6370_fled_resume,
	.rt_hal_fled_set_mode = mt6370_fled_set_mode,
	.rt_hal_fled_get_mode = mt6370_fled_get_mode,
	.rt_hal_fled_strobe = mt6370_fled_strobe,
	.rt_hal_fled_get_is_ready = mt6370_fled_is_ready,
	.rt_hal_fled_torch_current_list = mt6370_fled_torch_current_list,
	.rt_hal_fled_strobe_current_list = mt6370_fled_strobe_current_list,
	.rt_hal_fled_timeout_level_list = mt6370_fled_timeout_level_list,
	/* .fled_lv_protection_list = mt6370_fled_lv_protection_list, */
	.rt_hal_fled_strobe_timeout_list = mt6370_fled_strobe_timeout_list,
	/* method to set */
	.rt_hal_fled_set_torch_current_sel = mt6370_fled_set_torch_current_sel,
	.rt_hal_fled_set_strobe_current_sel =
					mt6370_fled_set_strobe_current_sel,
	.rt_hal_fled_set_timeout_level_sel = mt6370_fled_set_timeout_level_sel,

	.rt_hal_fled_set_strobe_timeout_sel =
					mt6370_fled_set_strobe_timeout_sel,

	/* method to get */
	.rt_hal_fled_get_torch_current_sel = mt6370_fled_get_torch_current_sel,
	.rt_hal_fled_get_strobe_current_sel =
					mt6370_fled_get_strobe_current_sel,
	.rt_hal_fled_get_timeout_level_sel = mt6370_fled_get_timeout_level_sel,
	.rt_hal_fled_get_strobe_timeout_sel =
					mt6370_fled_get_strobe_timeout_sel,
	/* PM shutdown, optional */
	.rt_hal_fled_shutdown = mt6370_fled_shutdown,
};

#define MT6370_FLED_TOR_CUR0	MT6370_PMU_REG_FLED1TORCTRL
#define MT6370_FLED_TOR_CUR1	MT6370_PMU_REG_FLED2TORCTRL
#define MT6370_FLED_STRB_CUR0	MT6370_PMU_REG_FLED1STRBCTRL
#define MT6370_FLED_STRB_CUR1	MT6370_PMU_REG_FLED2STRBCTRL2
#define MT6370_FLED_CS_MASK0	0x02
#define MT6370_FLED_CS_MASK1	0x01


#define MT6370_FLED_DEVICE(_id)			\
{						\
	.id = _id,				\
	.fled_ctrl_reg = MT6370_PMU_REG_FLEDEN,	\
	.fled_tor_cur_reg = MT6370_FLED_TOR_CUR##_id,	\
	.fled_strb_cur_reg = MT6370_FLED_STRB_CUR##_id,	\
	.fled_strb_to_reg = MT6370_PMU_REG_FLEDSTRBCTRL,	\
	.fled_cs_mask = MT6370_FLED_CS_MASK##_id,		\
}

static struct mt6370_pmu_fled_data mt6370_pmu_fleds[] = {
	MT6370_FLED_DEVICE(0),
	MT6370_FLED_DEVICE(1),
};

static struct mt6370_pmu_fled_data *mt6370_find_info(int id)
{
	struct mt6370_pmu_fled_data *fi;
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(mt6370_pmu_fleds); i++) {
		fi = &mt6370_pmu_fleds[i];
		if (fi->id == id)
			return fi;
	}
	return NULL;
}

static int mt6370_pmu_fled_probe(struct platform_device *pdev)
{
	struct mt6370_pmu_fled_data *fled_data;
	bool use_dt = pdev->dev.of_node;
	int ret;

	pr_info("%s: (%s) id = %d\n", __func__, MT6370_PMU_FLED_DRV_VERSION,
						pdev->id);
	fled_data = mt6370_find_info(pdev->id);
	if (fled_data == NULL) {
		dev_err(&pdev->dev, "invalid fled ID Specified\n");
		return -EINVAL;
	}

	fled_data->chip = dev_get_drvdata(pdev->dev.parent);
	fled_data->dev = &pdev->dev;

	if (use_dt)
		mt6370_fled_parse_dt(&pdev->dev, fled_data);
	platform_set_drvdata(pdev, fled_data);

	fled_data->base.init_props = &mt6370_fled_props;
	fled_data->base.hal = &mt6370_fled_hal;
	if (pdev->id == 0)
		fled_data->base.name = "mt-flash-led1";
	else
		fled_data->base.name = "mt-flash-led2";
	fled_data->base.chip_name = "mt6370_pmu_fled";
	pr_info("%s flash name = %s\n", __func__, fled_data->base.name);
	fled_data->mt_flash_dev = platform_device_register_resndata(
			fled_data->dev, "rt-flash-led",
			fled_data->id, NULL, 0, NULL, 0);

	if (!mt6370_fled_inited) {
		ret = mt6370_pmu_reg_clr_bit(fled_data->chip,
			MT6370_PMU_REG_FLEDVMIDTRKCTRL1,
			MT6370_FLED_FIXED_MODE_MASK);
		if (ret < 0) {
			pr_err("%s set fled fixed mode fail\n", __func__);
			return -EINVAL;
		}

		mt6370_pmu_fled_irq_register(pdev);
		mt6370_fled_inited = 1;
		dev_info(&pdev->dev, "mt6370 fled inited\n");
	}
	dev_info(&pdev->dev, "%s successfully\n", __func__);
	return 0;
}

static int mt6370_pmu_fled_remove(struct platform_device *pdev)
{
	struct mt6370_pmu_fled_data *fled_data = platform_get_drvdata(pdev);

	platform_device_unregister(fled_data->mt_flash_dev);
	dev_info(fled_data->dev, "%s successfully\n", __func__);
	return 0;
}

static const struct of_device_id mt_ofid_table[] = {
	{ .compatible = "mediatek,mt6370_pmu_fled1", },
	{ .compatible = "mediatek,mt6370_pmu_fled2", },
	{ },
};
MODULE_DEVICE_TABLE(of, mt_ofid_table);

static struct platform_driver mt6370_pmu_fled = {
	.driver = {
		.name = "mt6370_pmu_fled",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(mt_ofid_table),
	},
	.probe = mt6370_pmu_fled_probe,
	.remove = mt6370_pmu_fled_remove,
	/*.id_table = mt_id_table, */
};
module_platform_driver(mt6370_pmu_fled);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MediaTek MT6370 PMU Fled");
MODULE_VERSION(MT6370_PMU_FLED_DRV_VERSION);

/*
 * Release Note
 * 1.0.3_MTK
 * (1) Print warnings when strobe mode with HZ=1 or CFO=0
 *
 * 1.0.2_MTK
 * (1) Add delay for strobe on/off
 *
 * 1.0.1_MTK
 * (1) Remove typedef
 *
 * 1.0.0_MTK
 * (1) Initial Release
 */
