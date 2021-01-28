// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/pm.h>
#include <linux/delay.h>

#include "../inc/mt6360_pmu.h"
#include "../inc/mt6360_pmu_fled.h"
#include "../inc/mt6360_pmu_chg.h"
#include "../../../flashlight/richtek/rtfled.h"

#define MT6360_PMU_FLED_DRV_VERSION	"1.0.1_MTK"

static DEFINE_MUTEX(fled_lock);

static bool mt6360_fled_inited;
static enum flashlight_mode mt6360_global_mode = FLASHLIGHT_MODE_OFF;
static u8 mt6360_fled_on;

enum mt6360_fled_dev {
	MT6360_FLED1 = 0,
	MT6360_FLED2,
	MT6360_FLED_NUM,
};

struct mt6360_pmu_fled_info {
	struct rt_fled_dev base;
	struct device *dev;
	struct mt6360_pmu_info *mpi;
	int id;
	struct platform_device *fled_dev[MT6360_FLED_NUM];
	struct platform_device *mt_flash_dev;
};

static const struct mt6360_fled_platform_data def_platform_data = {
	.fled_vmid_track = 0,	/* 0: fixed mode, 1: tracking mode */
	.fled_strb_tout = 1248,	/* 64~2432ms, 32ms/step */
	.fled1_tor_cur = 37500,	/* 25000~400000uA, 12500uA/step */
	/*
	 * 25000~750000uA, 6250uA/step
	 * 750000~1500000uA, 12500uA/step
	 */
	.fled1_strb_cur = 800000,
	.fled2_tor_cur = 37500,	/* 25000~400000uA, 12500uA/step */
	/*
	 * 25000~750000uA, 6250uA/step
	 * 750000~1500000uA, 12500uA/step
	 */
	.fled2_strb_cur = 800000,
};

static inline struct mt6360_pmu_fled_info *
to_fled_info(struct rt_fled_dev *fled)
{
	return (struct mt6360_pmu_fled_info *)fled;
}

static const char *flashlight_mode_str[FLASHLIGHT_MODE_MAX] = {
	"off", "torch", "flash", "mixed",
	"dual flash", "dual torch", "dual off",
};

static irqreturn_t mt6360_pmu_fled_strbpin_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled_torpin_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled_tx_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled_lvf_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_err(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled2_short_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_err(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled1_short_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_err(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled2_strb_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled1_strb_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled2_strb_to_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled1_strb_to_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled2_tor_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled1_tor_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmu_fled_chg_vinovp_evt_handler(int irq, void *data)
{
	struct mt6360_pmu_fled_info *mpfi = data;

	dev_warn(mpfi->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static struct mt6360_pmu_irq_desc mt6360_pmu_fled_irq_desc[] = {
	MT6360_PMU_IRQDESC(fled_strbpin_evt),
	MT6360_PMU_IRQDESC(fled_torpin_evt),
	MT6360_PMU_IRQDESC(fled_tx_evt),
	MT6360_PMU_IRQDESC(fled_lvf_evt),
	MT6360_PMU_IRQDESC(fled2_short_evt),
	MT6360_PMU_IRQDESC(fled1_short_evt),
	MT6360_PMU_IRQDESC(fled2_strb_evt),
	MT6360_PMU_IRQDESC(fled1_strb_evt),
	MT6360_PMU_IRQDESC(fled2_strb_to_evt),
	MT6360_PMU_IRQDESC(fled1_strb_to_evt),
	MT6360_PMU_IRQDESC(fled2_tor_evt),
	MT6360_PMU_IRQDESC(fled1_tor_evt),
	MT6360_PMU_IRQDESC(fled_chg_vinovp_evt),
};

static void mt6360_pmu_fled_irq_register(struct platform_device *pdev)
{
	struct mt6360_pmu_irq_desc *irq_desc;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6360_pmu_fled_irq_desc); i++) {
		irq_desc = mt6360_pmu_fled_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		ret = platform_get_irq_byname(pdev, irq_desc->name);
		if (ret < 0)
			continue;
		irq_desc->irq = ret;
		ret = devm_request_threaded_irq(&pdev->dev, irq_desc->irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						platform_get_drvdata(pdev));
		if (ret < 0)
			dev_err(&pdev->dev,
				"request %s irq fail\n", irq_desc->name);
	}
}

static inline u32 mt6360_closest_reg(u32 min, u32 max, u32 step, u32 target)
{
	if (target < min)
		return 0;
	if (target >= max)
		return (max - min) / step;
	return (target - min) / step;
}

static u32 mt6360_transform_tor_cur(u32 val)
{
	return mt6360_closest_reg(25000, 400000, 12500, val);
}

static u32 mt6360_transform_strb_cur(u32 val)
{
	if (val <= 750000) {
		pr_err("%s %d\n", __func__, val);
		val = mt6360_closest_reg(25000, 750000, 6250, val);
		val |= 0x80; /* UTRAL_ISTRB */
		pr_err("%s 0x%02X\n", __func__, val);
		return val;
	}
	return mt6360_closest_reg(50000, 1500000, 12500, val);
}

static u32 mt6360_transform_strb_tout(u32 val)
{
	return mt6360_closest_reg(64, 2432, 32, val);
}

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
	MT6360_PDATA_VALPROP(fled_vmid_track, struct mt6360_fled_platform_data,
			     MT6360_PMU_FLED_VMIDTRK_CTRL1, 6, 0x40, NULL, 0),
	MT6360_PDATA_VALPROP(fled_strb_tout, struct mt6360_fled_platform_data,
			     MT6360_PMU_FLED_STRB_CTRL, 0, 0x7F,
			     mt6360_transform_strb_tout, 0),
	MT6360_PDATA_VALPROP(fled1_tor_cur, struct mt6360_fled_platform_data,
			     MT6360_PMU_FLED1_TOR_CTRL, 0, 0x1F,
			     mt6360_transform_tor_cur, 0),
	MT6360_PDATA_VALPROP(fled1_strb_cur, struct mt6360_fled_platform_data,
			     MT6360_PMU_FLED1_STRB_CTRL2, 0, 0x7F,
			     mt6360_transform_strb_cur, 0),
	MT6360_PDATA_VALPROP(fled2_tor_cur, struct mt6360_fled_platform_data,
			     MT6360_PMU_FLED2_TOR_CTRL, 0, 0x1F,
			     mt6360_transform_tor_cur, 0),
	MT6360_PDATA_VALPROP(fled2_strb_cur, struct mt6360_fled_platform_data,
			     MT6360_PMU_FLED2_STRB_CTRL2, 0, 0x7F,
			     mt6360_transform_strb_cur, 0),
};

static int mt6360_fled_apply_pdata(struct mt6360_pmu_fled_info *mpfi,
				   struct mt6360_fled_platform_data *pdata)
{
	int ret;

	dev_dbg(mpfi->dev, "%s ++\n", __func__);
	ret = mt6360_pdata_apply_helper(mpfi->mpi, pdata, mt6360_pdata_props,
					ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0)
		return ret;
	dev_dbg(mpfi->dev, "%s --\n", __func__);
	return 0;
}

static const struct mt6360_val_prop mt6360_val_props[] = {
	MT6360_DT_VALPROP(fled_vmid_track, struct mt6360_fled_platform_data),
	MT6360_DT_VALPROP(fled_strb_tout, struct mt6360_fled_platform_data),
	MT6360_DT_VALPROP(fled1_tor_cur, struct mt6360_fled_platform_data),
	MT6360_DT_VALPROP(fled1_strb_cur, struct mt6360_fled_platform_data),
	MT6360_DT_VALPROP(fled2_tor_cur, struct mt6360_fled_platform_data),
	MT6360_DT_VALPROP(fled2_strb_cur, struct mt6360_fled_platform_data),
};

static int mt6360_fled_parse_dt_data(struct device *dev,
				     struct mt6360_fled_platform_data *pdata)
{
	struct device_node *np = dev->of_node;

	dev_dbg(dev, "%s ++\n", __func__);
	memcpy(pdata, &def_platform_data, sizeof(*pdata));
	mt6360_dt_parser_helper(np, (void *)pdata,
				mt6360_val_props, ARRAY_SIZE(mt6360_val_props));
	dev_dbg(dev, "%s --\n", __func__);
	return 0;
}

static struct flashlight_properties mt6360_fled_props = {
	.type = FLASHLIGHT_TYPE_LED,
	.torch_brightness = 0,
	.torch_max_brightness = 31,	/* 00000 ~ 11110 */
	.strobe_brightness = 0,
	/*
	 * 25 ~ 750mA, step = 6.25mA, 117 steps
	 * 762.5 ~ 1500mA, step = 12.5mA, 60 steps
	 * 117 + 60 = 177
	 */
	.strobe_max_brightness = 177,
	.strobe_delay = 0,
	.strobe_timeout = 0,
	.alias_name = "mt6360-fled",
};

static int mt6360_fled_init(struct rt_fled_dev *fled)
{
	return 0;
}

static int mt6360_fled_suspend(struct rt_fled_dev *fled, pm_message_t state)
{
	return 0;
}

static int mt6360_fled_resume(struct rt_fled_dev *fled)
{
	return 0;
}

static inline int mt6360_pmu_reg_test_bit(struct mt6360_pmu_info *mpi,
					  u8 addr, u8 shift, bool *is_one)
{
	int ret = 0;
	u8 data = 0;

	ret = mt6360_pmu_reg_read(mpi, addr);
	if (ret < 0) {
		*is_one = false;
		return ret;
	}

	data = ret & (1 << shift);
	*is_one = (data == 0 ? false : true);

	return 0;
}

static int mt6360_fled_set_mode(struct rt_fled_dev *fled,
				enum flashlight_mode mode)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);
	u8 val, mask;
	int ret = 0;
	u8 en_bit = (fi->id == MT6360_FLED1) ? MT6360_FLCS1_EN_MASK :
		    MT6360_FLCS2_EN_MASK;
	bool hz_en = false, cfo_en = true;

	switch (mode) {
	case FLASHLIGHT_MODE_FLASH:
	case FLASHLIGHT_MODE_DUAL_FLASH:
		ret = mt6360_pmu_reg_test_bit(fi->mpi, MT6360_PMU_CHG_CTRL1,
					      MT6360_SHFT_HZ_EN, &hz_en);
		if (ret >= 0 && hz_en) {
			dev_err(fi->dev, "%s WARNING\n", __func__);
			dev_err(fi->dev, "%s set %s mode with HZ=1\n",
					 __func__, flashlight_mode_str[mode]);
		}

		ret = mt6360_pmu_reg_test_bit(fi->mpi, MT6360_PMU_CHG_CTRL2,
					      MT6360_SHFT_CFO_EN, &cfo_en);
		if (ret >= 0 && !cfo_en) {
			dev_err(fi->dev, "%s WARNING\n", __func__);
			dev_err(fi->dev, "%s set %s mode with CFO=0\n",
					 __func__, flashlight_mode_str[mode]);
		}
		break;
	default:
		break;
	}

	dev_info(fi->dev, "%s set %s mutex_lock +\n", __func__,
		flashlight_mode_str[mode]);
	mutex_lock(&fled_lock);
	dev_info(fi->dev, "%s set %s mutex_lock -\n", __func__,
		flashlight_mode_str[mode]);
	switch (mode) {
	case FLASHLIGHT_MODE_TORCH:
		if (mt6360_global_mode == FLASHLIGHT_MODE_FLASH ||
		    mt6360_global_mode == FLASHLIGHT_MODE_DUAL_FLASH)
			goto out;
		/* Fled en/Strobe off/Torch on */
		ret = mt6360_pmu_reg_clr_bits(fi->mpi, MT6360_PMU_FLED_EN,
					      MT6360_FL_STROBE_MASK);
		if (ret < 0)
			break;
		udelay(500);
		val = en_bit | MT6360_FL_TORCH_MASK;
		ret = mt6360_pmu_reg_set_bits(fi->mpi, MT6360_PMU_FLED_EN, val);
		if (ret < 0)
			break;
		udelay(500);
		mt6360_global_mode = mode;
		mt6360_fled_on |= 1 << fi->id;
		break;
	case FLASHLIGHT_MODE_FLASH:
		/* strobe off */
		ret = mt6360_pmu_reg_clr_bits(fi->mpi, MT6360_PMU_FLED_EN,
					     MT6360_FL_STROBE_MASK);
		if (ret < 0)
			break;
		udelay(400);
		/* fled en/strobe on */
		val = en_bit | MT6360_FL_STROBE_MASK;
		mask = val;
		ret = mt6360_pmu_reg_update_bits(fi->mpi, MT6360_PMU_FLED_EN,
						 mask, val);
		if (ret < 0)
			break;
		mdelay(5);
		mt6360_global_mode = mode;
		mt6360_fled_on |= 1 << fi->id;
		break;
	case FLASHLIGHT_MODE_OFF:
		ret = mt6360_pmu_reg_clr_bits(fi->mpi, MT6360_PMU_FLED_EN,
					     en_bit);
		if (ret < 0)
			break;
		mt6360_fled_on &= ~(1 << fi->id);
		if (mt6360_fled_on == 0)
			mt6360_global_mode = mode;
		break;
	case FLASHLIGHT_MODE_DUAL_FLASH:
		if (fi->id == MT6360_FLED2)
			goto out;
		/* strobe off */
		ret = mt6360_pmu_reg_clr_bits(fi->mpi, MT6360_PMU_FLED_EN,
					     MT6360_FL_STROBE_MASK);
		if (ret < 0)
			break;
		udelay(400);
		/* fled en/strobe on */
		val = en_bit | MT6360_FLCS2_EN_MASK | MT6360_FL_STROBE_MASK;
		mask = val;
		ret = mt6360_pmu_reg_update_bits(fi->mpi, MT6360_PMU_FLED_EN,
						 mask, val);
		if (ret < 0)
			break;
		mt6360_global_mode = mode;
		mt6360_fled_on |= (BIT(MT6360_FLED1) | BIT(MT6360_FLED2));
		break;
	case FLASHLIGHT_MODE_DUAL_TORCH:
		if (fi->id == MT6360_FLED2)
			goto out;
		if (mt6360_global_mode == FLASHLIGHT_MODE_FLASH ||
		    mt6360_global_mode == FLASHLIGHT_MODE_DUAL_FLASH)
			goto out;
		/* Fled en/Strobe off/Torch on */
		ret = mt6360_pmu_reg_clr_bits(fi->mpi, MT6360_PMU_FLED_EN,
					      MT6360_FL_STROBE_MASK);
		if (ret < 0)
			break;
		udelay(500);
		val = en_bit | MT6360_FLCS2_EN_MASK | MT6360_FL_TORCH_MASK;
		ret = mt6360_pmu_reg_set_bits(fi->mpi, MT6360_PMU_FLED_EN, val);
		if (ret < 0)
			break;
		udelay(500);
		mt6360_global_mode = mode;
		mt6360_fled_on |= (BIT(MT6360_FLED1) | BIT(MT6360_FLED2));
		break;
	case FLASHLIGHT_MODE_DUAL_OFF:
		if (fi->id == MT6360_FLED2)
			goto out;
		ret = mt6360_pmu_reg_clr_bits(fi->mpi, MT6360_PMU_FLED_EN,
					      en_bit | MT6360_FLCS2_EN_MASK);
		if (ret < 0)
			break;
		mt6360_fled_on = 0;
		mt6360_global_mode = FLASHLIGHT_MODE_OFF;
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

static int mt6360_fled_get_mode(struct rt_fled_dev *fled)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);
	int ret;

	if (fi->id == MT6360_FLED2) {
		dev_err(fi->dev, "%s FLED2 not support get mode\n", __func__);
		return 0;
	}

	ret = mt6360_pmu_reg_read(fi->mpi, MT6360_PMU_FLED_EN);
	if (ret < 0)
		return -EINVAL;

	if (ret & MT6360_FL_STROBE_MASK)
		return FLASHLIGHT_MODE_FLASH;
	if (ret & MT6360_FL_TORCH_MASK)
		return FLASHLIGHT_MODE_TORCH;
	return FLASHLIGHT_MODE_OFF;
}

static int mt6360_fled_strobe(struct rt_fled_dev *fled)
{
	return mt6360_fled_set_mode(fled, FLASHLIGHT_MODE_FLASH);
}

static int mt6360_fled_torch_current_list(struct rt_fled_dev *fled,
					  int selector)
{
	if (selector < 0 || selector > fled->init_props->torch_max_brightness)
		return -EINVAL;
	return 25000 + selector * 12500;
}

static int mt6360_fled_strobe_current_list(struct rt_fled_dev *fled,
					   int selector)
{
	if (selector < 0 || selector > fled->init_props->strobe_max_brightness)
		return -EINVAL;
	if (selector < 117)
		return 25000 + selector * 6250;
	/* make the base 762.5 mA */
	selector -= 60;
	return 750000 + selector * 12500;
}

#define MT6360_TCL_MAX	8
static int mt6360_fled_timeout_level_list(struct rt_fled_dev *fled,
					  int selector)
{
	if (selector < 0 || selector > MT6360_TCL_MAX)
		return -EINVAL;
	return 25000 + selector * 25000;
}

static int mt6360_fled_strobe_timeout_list(struct rt_fled_dev *fled,
					   int selector)
{
	if (selector < 0 || selector > 74)
		return -EINVAL;
	return 64 + selector * 32;
}

static int mt6360_fled_set_torch_current_sel(struct rt_fled_dev *fled,
					     int selector)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);

	return mt6360_pmu_reg_update_bits(fi->mpi, fi->id == MT6360_FLED1 ?
					  MT6360_PMU_FLED1_TOR_CTRL :
					  MT6360_PMU_FLED2_TOR_CTRL,
					  MT6360_ITOR_MASK,
					  selector << MT6360_ITOR_SHIFT);
}

static int mt6360_fled_set_strobe_current_sel(struct rt_fled_dev *fled,
					      int selector)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);
	int ret;
	u8 reg_strb_cur = (fi->id == MT6360_FLED1 ?
			   MT6360_PMU_FLED1_STRB_CTRL2 :
			   MT6360_PMU_FLED2_STRB_CTRL2);

	if (selector > 177 || selector < 0)
		return -EINVAL;

	if (selector < 117) /* 25 ~ 750mA */
		ret = mt6360_pmu_reg_set_bits(fi->mpi, reg_strb_cur,
					     MT6360_UTRAL_ISTRB_MASK);
	else { /* 762.5 ~ 1500mA */
		ret = mt6360_pmu_reg_clr_bits(fi->mpi, reg_strb_cur,
					     MT6360_UTRAL_ISTRB_MASK);
		selector -= 60; /* make the base 762.5 mA */
	}

	return mt6360_pmu_reg_update_bits(fi->mpi, reg_strb_cur,
					  MT6360_ISTRB_MASK,
					  selector << MT6360_ISTRB_SHIFT);
}

static int mt6360_fled_set_timeout_level_sel(struct rt_fled_dev *fled,
					     int selector)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);

	return mt6360_pmu_reg_update_bits(fi->mpi, fi->id == MT6360_FLED1 ?
					  MT6360_PMU_FLED1_CTRL :
					  MT6360_PMU_FLED2_CTRL,
					  MT6360_TCL_MASK,
					  selector << MT6360_TCL_SHIFT);
}

static int mt6360_fled_set_strobe_timeout_sel(struct rt_fled_dev *fled,
					      int selector)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);

	if (fi->id == MT6360_FLED2) {
		dev_err(fi->dev, "%s FLED2 not support set strobe timeout\n",
			__func__);
		return -EINVAL;
	}
	return mt6360_pmu_reg_update_bits(fi->mpi, MT6360_PMU_FLED_STRB_CTRL,
					  MT6360_STRB_TO_MASK,
					  selector << MT6360_STRB_TO_SHIFT);
}

static int mt6360_fled_get_torch_current_sel(struct rt_fled_dev *fled)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);
	int ret;

	ret = mt6360_pmu_reg_read(fi->mpi, fi->id == MT6360_FLED1 ?
				  MT6360_PMU_FLED1_TOR_CTRL :
				  MT6360_PMU_FLED2_TOR_CTRL);
	if (ret < 0) {
		dev_err(fi->dev, "%s fail\n", __func__);
		return ret;
	}

	return (ret & MT6360_ITOR_MASK) >> MT6360_ITOR_SHIFT;
}

static int mt6360_fled_get_strobe_current_sel(struct rt_fled_dev *fled)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);
	int ret;

	ret = mt6360_pmu_reg_read(fi->mpi, fi->id == MT6360_FLED1 ?
				  MT6360_PMU_FLED1_STRB_CTRL2 :
				  MT6360_PMU_FLED2_STRB_CTRL2);
	if (ret < 0) {
		dev_err(fi->dev, "%s fail\n", __func__);
		return ret;
	}

	return (ret & MT6360_ISTRB_MASK) >> MT6360_ISTRB_SHIFT;
}

static int mt6360_fled_get_timeout_level_sel(struct rt_fled_dev *fled)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);
	int ret;

	ret = mt6360_pmu_reg_read(fi->mpi, fi->id == MT6360_FLED1 ?
				  MT6360_PMU_FLED1_CTRL :
				  MT6360_PMU_FLED2_CTRL);
	if (ret < 0) {
		dev_err(fi->dev, "%s fail\n", __func__);
		return ret;
	}

	return (ret & MT6360_TCL_MASK) >> MT6360_TCL_SHIFT;
}

static int mt6360_fled_get_strobe_timeout_sel(struct rt_fled_dev *fled)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);
	int ret;

	ret = mt6360_pmu_reg_read(fi->mpi, MT6360_PMU_FLED_STRB_CTRL);
	if (ret < 0) {
		dev_err(fi->dev, "%s fail\n", __func__);
		return ret;
	}

	return (ret & MT6360_STRB_TO_MASK) >> MT6360_STRB_TO_SHIFT;
}

static void mt6360_fled_shutdown(struct rt_fled_dev *fled)
{
	mt6360_fled_set_mode(fled, FLASHLIGHT_MODE_OFF);
}

static int mt6360_fled_is_ready(struct rt_fled_dev *fled)
{
	struct mt6360_pmu_fled_info *fi = to_fled_info(fled);
	int ret;

	ret = mt6360_pmu_reg_read(fi->mpi, MT6360_PMU_CHG_STAT2);
	if (ret < 0) {
		dev_err(fi->dev, "%s fail\n", __func__);
		return ret;
	}

	/*
	 * CHG_STAT2 bit[3] FLED_CHG_VINOVP,
	 * if OVP = 0 --> V < 5.3, if OVP = 1, V > 5.6
	 */
	return ret & MT6360_FLED_CHG_VINOVP ? 0 : 1;
}

static struct rt_fled_hal mt6360_fled_hal = {
	.rt_hal_fled_init = mt6360_fled_init,
	.rt_hal_fled_suspend = mt6360_fled_suspend,
	.rt_hal_fled_resume = mt6360_fled_resume,
	.rt_hal_fled_set_mode = mt6360_fled_set_mode,
	.rt_hal_fled_get_mode = mt6360_fled_get_mode,
	.rt_hal_fled_strobe = mt6360_fled_strobe,
	.rt_hal_fled_get_is_ready = mt6360_fled_is_ready,
	.rt_hal_fled_torch_current_list = mt6360_fled_torch_current_list,
	.rt_hal_fled_strobe_current_list = mt6360_fled_strobe_current_list,
	.rt_hal_fled_timeout_level_list = mt6360_fled_timeout_level_list,
	/* .fled_lv_protection_list = mt6360_fled_lv_protection_list, */
	.rt_hal_fled_strobe_timeout_list = mt6360_fled_strobe_timeout_list,
	/* method to set */
	.rt_hal_fled_set_torch_current_sel = mt6360_fled_set_torch_current_sel,
	.rt_hal_fled_set_strobe_current_sel =
		mt6360_fled_set_strobe_current_sel,
	.rt_hal_fled_set_timeout_level_sel = mt6360_fled_set_timeout_level_sel,

	.rt_hal_fled_set_strobe_timeout_sel =
		mt6360_fled_set_strobe_timeout_sel,

	/* method to get */
	.rt_hal_fled_get_torch_current_sel = mt6360_fled_get_torch_current_sel,
	.rt_hal_fled_get_strobe_current_sel =
		mt6360_fled_get_strobe_current_sel,
	.rt_hal_fled_get_timeout_level_sel = mt6360_fled_get_timeout_level_sel,
	.rt_hal_fled_get_strobe_timeout_sel =
		mt6360_fled_get_strobe_timeout_sel,
	/* PM shutdown, optional */
	.rt_hal_fled_shutdown = mt6360_fled_shutdown,
};

static int mt6360_pmu_fled_probe(struct platform_device *pdev)
{
	struct mt6360_fled_platform_data *pdata = dev_get_platdata(&pdev->dev);
	struct mt6360_pmu_fled_info *mpfi;
	bool use_dt = pdev->dev.of_node;
	int ret, i;

	pr_info("%s (%s) id = %d\n", __func__, MT6360_PMU_FLED_DRV_VERSION,
					       pdev->id);
	if (!mt6360_fled_inited) {
		if (use_dt) {
			pdata = devm_kzalloc(&pdev->dev, sizeof(*pdata),
					     GFP_KERNEL);
			if (!pdata)
				return -ENOMEM;
			ret = mt6360_fled_parse_dt_data(&pdev->dev, pdata);
			if (ret < 0) {
				dev_err(&pdev->dev, "parse dt fail\n");
				return ret;
			}
			pdev->dev.platform_data = pdata;
		}
		if (!pdata) {
			dev_err(&pdev->dev, "no platform data specified\n");
			return -EINVAL;
		}
	}
	mpfi = devm_kzalloc(&pdev->dev, sizeof(*mpfi), GFP_KERNEL);
	if (!mpfi)
		return -ENOMEM;
	mpfi->dev = &pdev->dev;
	mpfi->mpi = dev_get_drvdata(mt6360_fled_inited ?
				    (pdev->dev.parent)->parent :
				    pdev->dev.parent);
	platform_set_drvdata(pdev, mpfi);

	if (!mt6360_fled_inited) {
		/* apply platform data */
		ret = mt6360_fled_apply_pdata(mpfi, pdata);
		if (ret < 0) {
			dev_err(&pdev->dev, "apply pdata fail\n");
			return ret;
		}

		/* irq register */
		mt6360_pmu_fled_irq_register(pdev);
		mt6360_fled_inited = true;

		for (i = 0; i < MT6360_FLED_NUM; i++) {
			mpfi->fled_dev[i] =
				platform_device_register_resndata(mpfi->dev,
					"mt6360_pmu_fled", i, NULL, 0, NULL, 0);

		}
	} else {
		if (pdev->id >= MT6360_FLED_NUM) {
			dev_err(mpfi->dev, "%s incorrect dev id\n", __func__);
			goto out;
		}
		mpfi->id = pdev->id;
		mpfi->base.init_props = &mt6360_fled_props;
		mpfi->base.hal = &mt6360_fled_hal;
		if (pdev->id == MT6360_FLED1)
			mpfi->base.name = "mt-flash-led1";
		else
			mpfi->base.name = "mt-flash-led2";
		mpfi->base.chip_name = "mt6360_pmu_fled";
		dev_err(mpfi->dev, "%s flash name %s\n", __func__,
			mpfi->base.name);
		mpfi->mt_flash_dev =
			platform_device_register_resndata(mpfi->dev,
							  "rt-flash-led",
							  mpfi->id, NULL, 0,
							  NULL, 0);
	}
	dev_info(&pdev->dev, "%s: successfully probed\n", __func__);
out:
	return 0;
}

static int mt6360_pmu_fled_remove(struct platform_device *pdev)
{
	struct mt6360_pmu_fled_info *mpfi = platform_get_drvdata(pdev);
	int i;

	dev_dbg(mpfi->dev, "%s\n", __func__);
	if (mpfi) {
		for (i = 0; i < MT6360_FLED_NUM; i++)
			platform_device_unregister(mpfi->fled_dev[i]);
		platform_device_unregister(mpfi->mt_flash_dev);
	}
	return 0;
}

static int __maybe_unused mt6360_pmu_fled_suspend(struct device *dev)
{
	return 0;
}

static int __maybe_unused mt6360_pmu_fled_resume(struct device *dev)
{
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_pmu_fled_pm_ops,
			 mt6360_pmu_fled_suspend, mt6360_pmu_fled_resume);

static const struct of_device_id __maybe_unused mt6360_pmu_fled_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmu_fled", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmu_fled_of_id);

static const struct platform_device_id mt6360_pmu_fled_id[] = {
	{ "mt6360_pmu_fled", 0 },
	{},
};
MODULE_DEVICE_TABLE(platform, mt6360_pmu_fled_id);

static struct platform_driver mt6360_pmu_fled_driver = {
	.driver = {
		.name = "mt6360_pmu_fled",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmu_fled_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmu_fled_of_id),
	},
	.probe = mt6360_pmu_fled_probe,
	.remove = mt6360_pmu_fled_remove,
	.id_table = mt6360_pmu_fled_id,
};
module_platform_driver(mt6360_pmu_fled_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6360 PMU FLED Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION(MT6360_PMU_FLED_DRV_VERSION);

/*
 * Version Note
 * 1.0.1_MTK
 * (1) Print warnings when strobe mode with HZ=1 or CFO=0
 *
 * 1.0.0_MTK
 * (1) Initial Release
 */
