/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/of_gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif /* #ifdef CONFIG_RT_REGMAP */
#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
#include <mt-plat/charger_class.h>
#include "mtk_charger_intf.h"
#endif /* #if (CONFIG_MTK_GAUGE_VERSION == 30) */

#include "rt9458.h"

static bool dbg_log_en;
module_param(dbg_log_en, bool, 0644);

struct rt9458_info {
	struct device *dev;
	struct i2c_client *i2c;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap_dev;
	struct rt_regmap_properties *regmap_props;
#endif /* #ifdef CONFIG_RT_REGMAP */
#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
	struct charger_device *chg_dev;
#endif /* #if (CONFIG_MTK_GAUGE_VERSION == 30) */
	struct mutex io_lock;
	int irq;
	u8 irq_mask[RT9458_IRQ_REGNUM];
	u8 chip_rev;
	u8 bst_sel;
};

static const struct rt9458_platform_data rt9458_def_platform_data = {
	.chg_name = "primary_chg",
	.ichg = 1550000, /* unit: uA */
	.aicr = 500000, /* unit: uA */
	.mivr = 4500000, /* unit: uV */
	.ieoc = 150000, /* unit: uA */
	.voreg = 4350000, /* unit : uV */
	.vmreg = 4350000, /* unit : uV */
	.intr_gpio = -1,
};

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT9458_REG_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_DEVID, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_CTRL4, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_CTRL5, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_CTRL6, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_CTRL7, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_IRQ1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_IRQ2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_IRQ3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_MASK1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_MASK2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_MASK3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9458_REG_CTRL8, 1, RT_VOLATILE, {});

static const rt_register_map_t rt9458_register_map[] = {
	RT_REG(RT9458_REG_CTRL1),
	RT_REG(RT9458_REG_CTRL2),
	RT_REG(RT9458_REG_CTRL3),
	RT_REG(RT9458_REG_DEVID),
	RT_REG(RT9458_REG_CTRL4),
	RT_REG(RT9458_REG_CTRL5),
	RT_REG(RT9458_REG_CTRL6),
	RT_REG(RT9458_REG_CTRL7),
	RT_REG(RT9458_REG_IRQ1),
	RT_REG(RT9458_REG_IRQ2),
	RT_REG(RT9458_REG_IRQ3),
	RT_REG(RT9458_REG_MASK1),
	RT_REG(RT9458_REG_MASK2),
	RT_REG(RT9458_REG_MASK3),
	RT_REG(RT9458_REG_CTRL8),
};

#define RT9458_REGISTER_NUM ARRAY_SIZE(rt9458_register_map)

static const struct rt_regmap_properties rt9458_regmap_props = {
	.aliases = "rt9458",
	.register_num = RT9458_REGISTER_NUM,
	.rm = rt9458_register_map,
	.rt_regmap_mode = RT_SINGLE_BYTE,
};

static int rt9458_io_write(void *i2c, u32 addr, int len, const void *src)
{
	return i2c_smbus_write_i2c_block_data(i2c, addr, len, src);
}

static int rt9458_io_read(void *i2c, u32 addr, int len, void *dst)
{
	return i2c_smbus_read_i2c_block_data(i2c, addr, len, dst);
}

static struct rt_regmap_fops rt9458_regmap_fops = {
	.read_device = rt9458_io_read,
	.write_device = rt9458_io_write,
};
#else
#define rt9458_io_write(i2c, addr, len, src) \
	i2c_smbus_write_i2c_block_data(i2c, addr, len, src)
#define rt9458_io_read(i2c, addr, len, dst) \
	i2c_smbus_read_i2c_block_data(i2c, addr, len, dst)
#endif /* #ifdef CONFIG_RT_REGMAP */

/* common i2c transfer function ++ */
#if 0
static int rt9458_reg_write(struct rt9458_info *ri, u8 reg, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};

	return rt_regmap_reg_write(ri->regmap_dev, &rrd, reg, data);
#else
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9458_io_write(ri->i2c, reg, 1, &data);
	mutex_unlock(&ri->io_lock);
	return 0;
#endif /* #ifdef CONFIG_RT_REGMAP */
}
#endif

static int rt9458_reg_block_write(struct rt9458_info *ri, u8 reg,
				  int len, const void *src)
{
#ifdef CONFIG_RT_REGMAP
	return rt_regmap_block_write(ri->regmap_dev, reg, len, src);
#else
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9458_io_write(ri->i2c, reg, len, src);
	mutex_unlock(&ri->io_lock);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt9458_reg_read(struct rt9458_info *ri, u8 reg)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret = 0;

	ret = rt_regmap_reg_read(ri->regmap_dev, &rrd, reg);
	return (ret < 0 ? ret : rrd.rt_data.data_u32);
#else
	u8 data = 0;
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9458_io_read(ri->i2c, reg, 1, &data);
	mutex_unlock(&ri->io_lock);
	return (ret < 0) ? ret : data;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt9458_reg_block_read(struct rt9458_info *ri, u8 reg,
				 int len, void *dst)
{
#ifdef CONFIG_RT_REGMAP
	return rt_regmap_block_read(ri->regmap_dev, reg, len, dst);
#else
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9458_io_read(ri->i2c, reg, len, dst);
	mutex_unlock(&ri->io_lock);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt9458_reg_assign_bits(struct rt9458_info *ri,
				  u8 reg, u8 mask, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};

	return rt_regmap_update_bits(ri->regmap_dev, &rrd, reg, mask, data);
#else
	u8 orig_data = 0;
	int ret = 0;

	mutex_lock(&ri->io_lock);
	ret = rt9458_io_read(ri->i2c, reg, 1, &orig_data);
	if (ret < 0)
		goto out_unlock;
	orig_data &= (~mask);
	orig_data |= (data & mask);
	ret = rt9458_io_write(ri->i2c, reg, 1, &orig_data);
out_unlock:
	mutex_unlock(&ri->io_lock);
	return ret;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

#define rt9458_reg_set_bits(ri, reg, mask) \
	rt9458_reg_assign_bits(ri, reg, mask, mask)
#define rt9458_reg_clr_bits(ri, reg, mask) \
	rt9458_reg_assign_bits(ri, reg, mask, 0)
/* common i2c transfer function -- */


/* ================== */
/* Internal Functions */
/* ================== */
static const unsigned long rt9458_support_iaicr[] = { 100, 500, 700, 1000};
static inline int rt9458_set_aicr(struct rt9458_info *ri, unsigned long uA)
{
	u8 iaicr_sel, iaicr;
	unsigned long mA = uA / 1000;
	int i, ret = 0;
	/* change by sw control */

	for (i = 0; i < ARRAY_SIZE(rt9458_support_iaicr); i++) {
		if (mA < rt9458_support_iaicr[i])
			break;
	}
	if (i == ARRAY_SIZE(rt9458_support_iaicr))
		dev_dbg(ri->dev, "will config to no limit\n");
	else if (i > 0)
		i--;
	switch (i) {
	case 0:
		iaicr_sel = 0;
		iaicr = 0;
		break;
	case 1:
		iaicr_sel = 0;
		iaicr = 1;
		break;
	case 2:
		iaicr_sel = 1;
		iaicr = 1;
		break;
	case 3:
		iaicr_sel = 0;
		iaicr = 2;
		break;
	case 4:
		iaicr_sel = 0;
		iaicr = 3;
		break;
	default:
		dev_warn(ri->dev, "illegal selection\n");
		return -EINVAL;
	}
	ret = rt9458_reg_assign_bits(ri, RT9458_REG_CTRL2, RT9458_IAICR_MASK,
				     iaicr << RT9458_IAICR_SHFT);
	if (ret < 0) {
		dev_info(ri->dev, "config iaicr fail\n");
		return ret;
	}
	ret = rt9458_reg_assign_bits(ri, RT9458_REG_CTRL6,
				     RT9458_IAICRSEL_MASK,
				     (iaicr_sel ? 0xff : 0));
	if (ret < 0) {
		dev_info(ri->dev, "config iaicr_sel fail\n");
		return ret;
	}
	/* config aicr to internal aicr register */
	ret = rt9458_reg_set_bits(ri, RT9458_REG_CTRL2, RT9458_IAICRINT_MASK);
	if (ret < 0) {
		dev_info(ri->dev, "config iaicr_int  fail\n");
		return ret;
	}
	return 0;
}

static inline int rt9458_set_mivr(struct rt9458_info *ri, unsigned long uV)
{
	u8 data = 0;

	if (uV >= 4100000) {
		data = DIV_ROUND_UP(uV - 4100000, 100000);
		if (data > RT9458_MIVR_MAXVAL)
			data = RT9458_MIVR_MAXVAL;
	} else {
		dev_warn(ri->dev, "value is too small, disable mivr\n");
		data = 7;
	}
	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL8, RT9458_MIVR_MASK,
				      data << RT9458_MIVR_SHFT);
}

static inline int rt9458_set_ichg(struct rt9458_info *ri, unsigned long uA)
{
	u8 data = 0;

	if (uA >= 500000) {
		data = (uA - 500000) / 150000;
		if (data > RT9458_ICHG_MAXVAL)
			data = RT9458_ICHG_MAXVAL;
	}
	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL6, RT9458_ICHG_MASK,
				      data << RT9458_ICHG_SHFT);
}

static inline int rt9458_set_ieoc(struct rt9458_info *ri, unsigned long uA)
{
	u8 data = 0;

	if (uA >= 50000) {
		data = DIV_ROUND_UP(uA - 50000, 50000);
		if (data > RT9458_IEOC_MAXVAL)
			data = RT9458_IEOC_MAXVAL;
	}
	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL5, RT9458_IEOC_MASK,
				      data << RT9458_IEOC_SHFT);
}

static inline int rt9458_set_voreg(struct rt9458_info *ri, unsigned long uV)
{
	u8 data = 0;

	if (uV >= 4330000) {
		data = DIV_ROUND_UP(uV - 4330000, 20000);
		data += 0x29;
		if (data > RT9458_VOREG_MAXVAL)
			data = RT9458_VOREG_MAXVAL;
	} else if (uV > 4300000)
		data = 0x29;
	else if (uV >= 3500000)
		data = DIV_ROUND_UP(uV - 3500000, 20000);

	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL3, RT9458_VOREG_MASK,
				      data << RT9458_VOREG_SHFT);
}

static inline int rt9458_set_vmreg(struct rt9458_info *ri, unsigned long uV)
{
	u8 data = 0;

	if (uV >= 4200000) {
		data = DIV_ROUND_UP(uV - 4200000, 20000);
		if (data > RT9458_VMREG_MAXVAL)
			data = RT9458_VOREG_MAXVAL;
	}
	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL7, RT9458_VMREG_MASK,
				      data << RT9458_VMREG_SHFT);
}

static inline int rt9458_set_eoc_shdn(struct rt9458_info *ri, bool en)
{
	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL2, RT9458_TESHDN_MASK,
				      en ? 0xff : 0);
}

static inline int rt9458_set_chg_enable(struct rt9458_info *ri, bool en)
{
	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL7, RT9458_CHGEN_MASK,
				      en ? 0xff : 0);
}

#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
static int rt9458_charger_is_charging_done(struct charger_device *chg_dev,
					   bool *done)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	ret = rt9458_reg_read(ri, RT9458_REG_CTRL1);
	if (ret < 0)
		return ret;
	ret = (ret & RT9458_STAT_MASK) >> RT9458_STAT_SHFT;
	if (ret == 0x02)
		*done = true;
	else
		*done = false;
	return 0;
}

static int rt9458_charger_get_charging_stat(struct charger_device *chg_dev,
					enum rt9458_chg_stat *chg_stat)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	ret = rt9458_reg_read(ri, RT9458_REG_CTRL1);
	if (ret < 0)
		return ret;

	*chg_stat = (ret & RT9458_STAT_MASK) >> RT9458_STAT_SHFT;

	return ret;
}

static int rt9458_charger_enable_otg(struct charger_device *chg_dev, bool en)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	/* clear HZ mode */
	dev_info(ri->dev, "%s: clear HZ mode\n", __func__);
	rt9458_reg_clr_bits(ri, RT9458_REG_CTRL2, RT9458_HZ_MASK);

	if (en) {
		/* store VOREG selector, and switch Vbst to 5V */
		ret = rt9458_reg_read(ri, RT9458_REG_CTRL3);
		ri->bst_sel = (ret & RT9458_VOREG_MASK) >> RT9458_VOREG_SHFT;
		ret = rt9458_reg_assign_bits(ri,
					     RT9458_REG_CTRL3,
					     RT9458_VOREG_MASK,
					     0x17 << RT9458_VOREG_SHFT);
	} else	/* recover original VOREG */
		ret = rt9458_reg_assign_bits(ri,
					     RT9458_REG_CTRL3,
					     RT9458_VOREG_MASK,
					     ri->bst_sel << RT9458_VOREG_SHFT);

	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL2, RT9458_OPA_MASK,
				      en ? 0xff : 0x00);
}

static int rt9458_charger_enable_te(struct charger_device *chg_dev, bool en)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	struct rt9458_platform_data *pdata = dev_get_platdata(ri->dev);

	/* if not specified to enable te function, bypass it */
	if (!pdata->enable_te)
		return 0;
	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL2, RT9458_TERM_MASK,
				      en ? 0xff : 0x00);
}

static int rt9458_charger_enable_timer(struct charger_device *chg_dev, bool en)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);

	return rt9458_reg_assign_bits(ri, RT9458_REG_CTRL5, RT9458_TMREN_MASK,
				      en ? 0xff : 0);
}

static int rt9458_charger_is_timer_enabled(struct charger_device *chg_dev,
					   bool *en)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	ret = rt9458_reg_read(ri, RT9458_REG_CTRL5);
	if (ret < 0)
		return ret;
	*en = (ret & RT9458_TMREN_MASK) ? true : false;
	return 0;
}

static int rt9458_charger_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);

	return rt9458_set_mivr(ri, uV);
}

static int rt9458_charger_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 100000;
	return 0;
}

static int rt9458_charger_set_aicr(struct charger_device *chg_dev, u32 uA)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);

	return rt9458_set_aicr(ri, uA);
}

static int rt9458_charger_get_aicr(struct charger_device *chg_dev, u32 *uA)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	u8 iaicr = 0;
	u8 iaicr_sel = 0;
	int ret = 0;

	ret = rt9458_reg_read(ri, RT9458_REG_CTRL2);
	if (ret < 0)
		return ret;
	iaicr = (ret & RT9458_IAICR_MASK) >> RT9458_IAICR_SHFT;
	ret = rt9458_reg_read(ri, RT9458_REG_CTRL6);
	if (ret < 0)
		return ret;
	iaicr_sel = ret & RT9458_IAICRSEL_MASK;
	switch (iaicr) {
	case 0:
		*uA = 100000;
		break;
	case 1:
		if (iaicr_sel)
			*uA = 700000;
		else
			*uA = 500000;
		break;
	case 2:
		if (iaicr_sel)
			*uA = 700000;
		else
			*uA = 1000000;
		break;
	case 3:
		/* no limit */
		*uA = U32_MAX;
		break;
	default:
		return -EINVAL;
	}
	return 0;
}

static int rt9458_charger_get_cv(struct charger_device *chg_dev, u32 *uV)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	ret = rt9458_reg_read(ri, RT9458_REG_CTRL3);
	if (ret < 0)
		return ret;
	ret = (ret & RT9458_VOREG_MASK) >> RT9458_VOREG_SHFT;
	if (ret > RT9458_VOREG_MAXVAL)
		ret = RT9458_VOREG_MAXVAL;
	if (ret > 0x28)
		*uV = 4330000 + 20000 * (ret - 0x29);
	else
		*uV = 3500000 + 20000 * ret;
	return 0;
}

static int rt9458_charger_set_cv(struct charger_device *chg_dev, u32 uV)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);

	return rt9458_set_voreg(ri, uV);
}
static int rt9458_charger_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	*uA = 500000;
	return 0;
}
static int rt9458_charger_set_ichg(struct charger_device *chg_dev, u32 uA)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);

	return rt9458_set_ichg(ri, uA);
}

static int rt9458_charger_get_ichg(struct charger_device *chg_dev, u32 *uA)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	ret = rt9458_reg_read(ri, RT9458_REG_CTRL6);
	if (ret < 0)
		return ret;
	ret = (ret & RT9458_ICHG_MASK) >> RT9458_ICHG_SHFT;
	if (ret > RT9458_ICHG_MAXVAL)
		ret = RT9458_ICHG_MAXVAL;
	*uA = 500000 + (ret * 150000);
	return 0;
}

static int rt9458_charger_is_enabled(struct charger_device *chg_dev, bool *en)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	int ret = 0;

	ret = rt9458_reg_read(ri, RT9458_REG_CTRL7);
	if (ret < 0)
		return ret;
	*en = (ret & RT9458_CHGEN_MASK) ? true : false;
	return 0;
}

static int rt9458_charger_enable(struct charger_device *chg_dev, bool en)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);

	return rt9458_set_chg_enable(ri, en);
}

static int rt9458_charger_plug_out(struct charger_device *chg_dev)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	struct rt9458_platform_data *pdata = dev_get_platdata(ri->dev);

	return pdata->enable_te ? rt9458_charger_enable_te(chg_dev, 0) : 0;
}

static int rt9458_charger_plug_in(struct charger_device *chg_dev)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	struct rt9458_platform_data *pdata = dev_get_platdata(ri->dev);

	return pdata->enable_te ? rt9458_charger_enable_te(chg_dev, 1) : 0;
}

static inline int rt9458_get_mivr(struct rt9458_info *ri, u32 *uV)
{
	u8 reg_mivr;
	int ret = 0;

	ret = rt9458_reg_read(ri, RT9458_REG_CTRL8);
	if (ret < 0)
		return ret;
	reg_mivr = (ret & RT9458_MIVR_MASK) >> RT9458_MIVR_SHFT;
	/* disable mivr */
	if (reg_mivr == 0x07)
		*uV = 0;
	else
		*uV = 4100000 + 100000 * reg_mivr;
	return 0;
}

static inline int rt9458_get_ieoc(struct rt9458_info *ri, u32 *uA)
{
	int ret = 0;
	u8 reg_ieoc = 0;

	ret = rt9458_reg_read(ri, RT9458_REG_CTRL5);
	if (ret < 0)
		return ret;
	reg_ieoc = (ret & RT9458_IEOC_MASK) >> RT9458_IEOC_SHFT;
	*uA = 50000 + 50000 * reg_ieoc;
	return 0;
}

static int rt9458_charger_dump_registers(struct charger_device *chg_dev)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);
	struct rt9458_platform_data *pdata = dev_get_platdata(ri->dev);
	int i = 0, ret = 0;
	u32 ichg = 0, aicr = 0, mivr = 0, ieoc = 0, voreg = 0;
	enum rt9458_chg_stat chg_stat = RT9458_CHG_STAT_READY;
	bool chg_en = 0;

	ret = rt9458_charger_get_ichg(chg_dev, &ichg);
	if (ret < 0)
		dev_info(ri->dev, "get ichg fail\n");
	ret = rt9458_charger_get_aicr(chg_dev, &aicr);
	if (ret < 0)
		dev_info(ri->dev, "get aicr fail\n");
	ret = rt9458_get_mivr(ri, &mivr);
	if (ret < 0)
		dev_info(ri->dev, "get mivr fail\n");
	ret = rt9458_charger_is_enabled(chg_dev, &chg_en);
	if (ret < 0)
		dev_info(ri->dev, "get charger enabled fail\n");
	ret = rt9458_get_ieoc(ri, &ieoc);
	if (ret < 0)
		dev_info(ri->dev, "get ieoc fail\n");
	ret = rt9458_charger_get_cv(chg_dev, &voreg);
	if (ret < 0)
		dev_info(ri->dev, "get cv fail\n");
	ret = rt9458_charger_get_charging_stat(chg_dev, &chg_stat);
	if (ret < 0)
		dev_info(ri->dev, "get charger status fail\n");

	if (dbg_log_en) {
		for (i = RT9458_REG_CTRL1; i <= RT9458_REG_CTRL8; i++) {
			/* byapss ire event register */
			if (i >= RT9458_REG_IRQ1 && i <= RT9458_REG_IRQ3)
				continue;
			/* bypass not existed register */
			if (i > RT9458_REG_MASK3 && i < RT9458_REG_CTRL8)
				continue;
			ret = rt9458_reg_read(ri, i);
			if (ret < 0)
				continue;
			dev_info(ri->dev, "[0x%02X] : 0x%02x\n", i, ret);
		}
	}
	dev_info(ri->dev,
		"%s: ICHG = %dmA, AICR = %dmA, MIVR = %dmV, IEOC = %dmA\n",
		__func__, ichg / 1000, aicr / 1000, mivr / 1000, ieoc / 1000);

	dev_info(ri->dev,
		"%s: CV = %dmV, vmreg = %dmV, CHG_EN = %d, CHG_STATUS = %s\n",
		__func__, voreg / 1000, pdata->vmreg / 1000, chg_en,
		rt9458_chg_stat_name[chg_stat]);
	return 0;
}

static int rt9458_charger_do_event(struct charger_device *chg_dev, u32 event,
				   u32 args)
{
	struct rt9458_info *ri = charger_get_data(chg_dev);

	switch (event) {
	case EVENT_EOC:
		dev_info(ri->dev, "do eoc event\n");
		charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		dev_info(ri->dev, "do recharge event\n");
		charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}
	return 0;
}

static const struct charger_ops rt9458_chg_ops = {
	/* cable plug in/out */
	.plug_in = rt9458_charger_plug_in,
	.plug_out = rt9458_charger_plug_out,
	/* enable */
	.enable = rt9458_charger_enable,
	.is_enabled = rt9458_charger_is_enabled,
	/* charging current */
	.get_charging_current = rt9458_charger_get_ichg,
	.set_charging_current = rt9458_charger_set_ichg,
	.get_min_charging_current = rt9458_charger_get_min_ichg,
	/* charging voltage */
	.set_constant_voltage = rt9458_charger_set_cv,
	.get_constant_voltage = rt9458_charger_get_cv,
	/* charging input current */
	.get_input_current = rt9458_charger_get_aicr,
	.set_input_current = rt9458_charger_set_aicr,
	.get_min_input_current = rt9458_charger_get_min_aicr,
	/* charging mivr */
	.set_mivr = rt9458_charger_set_mivr,
	/* safety timer */
	.is_safety_timer_enabled = rt9458_charger_is_timer_enabled,
	.enable_safety_timer = rt9458_charger_enable_timer,
	/* charing termination */
	.enable_termination = rt9458_charger_enable_te,
	/* OTG */
	.enable_otg = rt9458_charger_enable_otg,
	/* misc */
	.is_charging_done = rt9458_charger_is_charging_done,
	.dump_registers = rt9458_charger_dump_registers,
	/* event */
	.event = rt9458_charger_do_event,
};

static const struct charger_properties rt9458_chg_props = {
	.alias_name = "rt9458",
};
#endif /* #if (CONFIG_MTK_GAUGE_VERSION == 30) */

static irqreturn_t rt9458_irq_BATAB_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_VINOVPI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
	ri->chg_dev->noti.vbusov_stat = true;
	charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);
#endif /* #if (CONFIG_MTK_GAUGE_VERSION == 30) */
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_TSDI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_CHMIVRI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_CHTREGI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_dbg(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_CH32MI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_dbg(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_CHRCHGI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
	charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_RECHG);
#endif /* #if (CONFIG_MTK_GAUGE_VERSION == 30) */
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_CHTERMI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
	charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_EOC);
#endif /* #if (CONFIG_MTK_GAUGE_VERSION == 30) */
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_CHBATOVI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
	charger_dev_notify(ri->chg_dev, CHARGER_DEV_NOTIFY_BAT_OVP);
#endif /* #if (CONFIG_MTK_GAUGE_VERSION == 30) */
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_CHRVPI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_BST32SI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_info(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_BSTLOWVI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_dbg(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_BSTOLI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_dbg(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static irqreturn_t rt9458_irq_BSTVINOVI_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;

	dev_dbg(ri->dev, "%s\n", __func__);
	return IRQ_HANDLED;
}

static const irq_handler_t rt9458_irq_desc[RT9458_IRQ_MAX] = {
	[RT9458_IRQ_BATAB]	= rt9458_irq_BATAB_handler,
	[RT9458_IRQ_VINOVPI]	= rt9458_irq_VINOVPI_handler,
	[RT9458_IRQ_TSDI]	= rt9458_irq_TSDI_handler,
	[RT9458_IRQ_CHMIVRI]	= rt9458_irq_CHMIVRI_handler,
	[RT9458_IRQ_CHTREGI]	= rt9458_irq_CHTREGI_handler,
	[RT9458_IRQ_CH32MI]	= rt9458_irq_CH32MI_handler,
	[RT9458_IRQ_CHRCHGI]	= rt9458_irq_CHRCHGI_handler,
	[RT9458_IRQ_CHTERMI]	= rt9458_irq_CHTERMI_handler,
	[RT9458_IRQ_CHBATOVI]	= rt9458_irq_CHBATOVI_handler,
	[RT9458_IRQ_CHRVPI]	= rt9458_irq_CHRVPI_handler,
	[RT9458_IRQ_BST32SI]	= rt9458_irq_BST32SI_handler,
	[RT9458_IRQ_BSTLOWVI]	= rt9458_irq_BSTLOWVI_handler,
	[RT9458_IRQ_BSTOLI]	= rt9458_irq_BSTOLI_handler,
	[RT9458_IRQ_BSTVINOVI]	= rt9458_irq_BSTVINOVI_handler,
};

static irqreturn_t rt9458_intr_handler(int irq, void *dev_id)
{
	struct rt9458_info *ri = (struct rt9458_info *)dev_id;
	unsigned char event[RT9458_IRQ_REGNUM] = {0};
	int i, j, id, ret = 0;

	dev_dbg(ri->dev, "%s triggered\n", __func__);
	ret = rt9458_reg_block_read(ri, RT9458_REG_IRQ1,
				    RT9458_IRQ_REGNUM, event);
	if (ret < 0) {
		dev_info(ri->dev, "read irq event fail\n");
		goto out_intr_handler;
	}
	for (i = 0; i < RT9458_IRQ_REGNUM; i++) {
		event[i] &=  ~(ri->irq_mask[i]);
		if (!event[i])
			continue;
		for (j = 0; j < 8; j++) {
			if (!(event[i] & (1 << j)))
				continue;
			id = i * 8 + j;
			if (!rt9458_irq_desc[id]) {
				dev_warn(ri->dev, "no %d irq_handler", id);
				continue;
			}
			rt9458_irq_desc[id](id, ri);
		}
	}
out_intr_handler:
	return IRQ_HANDLED;
}

static int rt9458_chip_irq_init(struct rt9458_info *ri)
{
	struct rt9458_platform_data *pdata = dev_get_platdata(ri->dev);
	int ret = 0;

	ret = devm_gpio_request_one(ri->dev, pdata->intr_gpio, GPIOF_IN,
				    "rt9458_intr_gpio");
	if (ret < 0) {
		dev_info(ri->dev, "request gpio fail\n");
		return ret;
	}
	ri->irq = gpio_to_irq(pdata->intr_gpio);
	ret = devm_request_threaded_irq(ri->dev, ri->irq, NULL,
					rt9458_intr_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					dev_name(ri->dev), ri);
	if (ret < 0) {
		dev_info(ri->dev, "request interrupt fail\n");
		return ret;
	}
	device_init_wakeup(ri->dev, true);
	ri->irq_mask[0] = 0x00;
	ri->irq_mask[1] = 0x19;
	ri->irq_mask[2] = 0x00;
	return rt9458_reg_block_write(ri, RT9458_REG_MASK1, 3, ri->irq_mask);
}

static int rt9458_register_rt_regmap(struct rt9458_info *ri)
{
#ifdef CONFIG_RT_REGMAP
	ri->regmap_props = devm_kzalloc(ri->dev, sizeof(*ri->regmap_props),
					GFP_KERNEL);
	if (!ri->regmap_props)
		return -ENOMEM;
	memcpy(ri->regmap_props, &rt9458_regmap_props,
	       sizeof(*ri->regmap_props));
	ri->regmap_props->name = kasprintf(GFP_KERNEL, "rt9458.%s",
					  dev_name(ri->dev));
	ri->regmap_dev = rt_regmap_device_register(ri->regmap_props,
				&rt9458_regmap_fops, ri->dev, ri->i2c, ri);
	if (!ri->regmap_dev)
		return -EINVAL;
	return 0;
#else
	return 0;
#endif /* #ifdef CONFIG_RT_REGMAP */
}

static int rt9458_chip_pdata_init(struct rt9458_info *ri)
{
	struct rt9458_platform_data *pdata = dev_get_platdata(ri->dev);
	int ret = 0;

	ret = rt9458_set_mivr(ri, pdata->mivr);
	if (ret < 0) {
		dev_info(ri->dev, "set mivr fail\n");
		return ret;
	}
	ret = rt9458_set_aicr(ri, pdata->aicr);
	if (ret < 0) {
		dev_info(ri->dev, "set aicr fail\n");
		return ret;
	}
	ret = rt9458_set_vmreg(ri, pdata->vmreg);
	if (ret < 0) {
		dev_info(ri->dev, "set vmreg fail\n");
		return ret;
	}
	ret = rt9458_set_voreg(ri, pdata->voreg);
	if (ret < 0) {
		dev_info(ri->dev, "set voreg fail\n");
		return ret;
	}
	ret = rt9458_set_ichg(ri, pdata->ichg);
	if (ret < 0) {
		dev_info(ri->dev, "set ichg fail\n");
		return ret;
	}
	ret = rt9458_set_ieoc(ri, pdata->ieoc);
	if (ret < 0) {
		dev_info(ri->dev, "set ieoc fail\n");
		return ret;
	}
	ret = rt9458_set_eoc_shdn(ri, pdata->enable_eoc_shdn);
	if (ret < 0)
		dev_info(ri->dev, "set eoc shutdown fail\n");
	if (!strcmp(pdata->chg_name, "secondary_chg"))
		rt9458_set_chg_enable(ri, false);
	return ret;
}

static int rt9458_chip_reset(struct i2c_client *i2c)
{
	unsigned char tmp[RT9458_IRQ_REGNUM] = {0};
	int ret = 0;

	ret = i2c_smbus_write_byte_data(i2c, RT9458_REG_CTRL4, 0x80);
	if (ret < 0) {
		dev_info(&i2c->dev, "chip reset fail\n");
		return ret;
	}
	msleep(20);
	/* default disable safety timer */
	ret = i2c_smbus_write_byte_data(i2c, RT9458_REG_CTRL5, 0x02);
	if (ret < 0) {
		dev_info(&i2c->dev, "default disable timer fail\n");
		return ret;
	}
	memset(tmp, 0xff, RT9458_IRQ_REGNUM);
	ret = i2c_smbus_write_i2c_block_data(i2c, RT9458_REG_MASK1,
					     RT9458_IRQ_REGNUM, tmp);
	if (ret < 0) {
		dev_info(&i2c->dev, "set all masked fail\n");
		return ret;
	}
	ret = i2c_smbus_read_i2c_block_data(i2c, RT9458_REG_IRQ1,
					    RT9458_IRQ_REGNUM, tmp);
	if (ret < 0) {
		dev_info(&i2c->dev, "read all irqevents fail\n");
		return ret;
	}
	return 0;
}

static inline int rt9458_i2c_detect_devid(struct i2c_client *i2c)
{
	int ret = 0;

	ret = i2c_smbus_read_byte_data(i2c, RT9458_REG_DEVID);
	if (ret < 0) {
		dev_info(&i2c->dev, "%s: chip io may bail\n", __func__);
		return ret;
	}
	dev_dbg(&i2c->dev, "%s: dev_id 0x%02x\n", __func__, ret);
	if ((ret & 0xf0) != 0x00) {
		dev_info(&i2c->dev, "%s: vendor id not correct\n", __func__);
		return -ENODEV;
	}
	/* finally return the rev id */
	return (ret & 0x0f);
}

static void rt_parse_dt(struct device *dev, struct rt9458_platform_data *pdata)
{
	/* just used to prevent the null parameter */
	if (!dev || !pdata)
		return;
	if (of_property_read_string(dev->of_node, "charger_name",
				    &pdata->chg_name) < 0)
		dev_warn(dev, "not specified chg_name\n");
	if (of_property_read_u32(dev->of_node, "ichg", &pdata->ichg) < 0)
		dev_warn(dev, "not specified ichg value\n");
	if (of_property_read_u32(dev->of_node, "aicr", &pdata->aicr) < 0)
		dev_warn(dev, "not specified aicr value\n");
	if (of_property_read_u32(dev->of_node, "mivr", &pdata->mivr) < 0)
		dev_warn(dev, "not specified mivr value\n");
	if (of_property_read_u32(dev->of_node, "ieoc", &pdata->ieoc) < 0)
		dev_warn(dev, "not specified ieoc_value\n");
	if (of_property_read_u32(dev->of_node, "cv", &pdata->voreg) < 0)
		dev_warn(dev, "not specified cv value\n");
	if (of_property_read_u32(dev->of_node, "vmreg", &pdata->vmreg) < 0)
		dev_warn(dev, "not specified vmreg value\n");
	pdata->enable_te = of_property_read_bool(dev->of_node, "enable_te");
	pdata->enable_eoc_shdn = of_property_read_bool(dev->of_node,
						       "enable_eoc_shdn");
#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	pdata->intr_gpio = of_get_named_gpio(dev->of_node, "rt,intr_gpio", 0);
#else
	if (of_property_read_u32(np, "rt,intr_gpio_num", &pdata->intr_gpio) < 0)
		dev_warn(dev, "not specified irq gpio number\n");
#endif /* #if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND) */
}

static int rt9458_i2c_probe(struct i2c_client *i2c,
			    const struct i2c_device_id *id)
{
	struct rt9458_platform_data *pdata = dev_get_platdata(&i2c->dev);
	struct rt9458_info *ri = NULL;
	u8 rev_id = 0;
	bool use_dt = i2c->dev.of_node;
	int ret = 0;

	dev_info(&i2c->dev, "%s start\n", __func__);
	ret = rt9458_i2c_detect_devid(i2c);
	if (ret < 0)
		return ret;
	/* if success, return value is revision id */
	rev_id = (u8)ret;
	/* driver data */
	ri = devm_kzalloc(&i2c->dev, sizeof(*ri), GFP_KERNEL);
	if (!ri)
		return -ENOMEM;
	/* platform data */
	if (use_dt) {
		pdata = devm_kzalloc(&i2c->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		memcpy(pdata, &rt9458_def_platform_data, sizeof(*pdata));
		i2c->dev.platform_data = pdata;
		rt_parse_dt(&i2c->dev, pdata);
	} else {
		if (!pdata) {
			dev_info(&i2c->dev, "no pdata specify\n");
			return -EINVAL;
		}
	}
	ri->dev = &i2c->dev;
	ri->i2c = i2c;
	mutex_init(&ri->io_lock);
	ri->chip_rev = rev_id;
	memset(ri->irq_mask, 0xff, RT9458_IRQ_REGNUM);
	i2c_set_clientdata(i2c, ri);
	/* do whole chip reset */
	ret = rt9458_chip_reset(i2c);
	if (ret < 0)
		return ret;
	/* rt-regmap register */
	ret = rt9458_register_rt_regmap(ri);
	if (ret < 0)
		return ret;
	ret = rt9458_chip_pdata_init(ri);
	if (ret < 0)
		return ret;
#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
	/* charger class register */
	ri->chg_dev = charger_device_register(pdata->chg_name, ri->dev, ri,
					      &rt9458_chg_ops,
					      &rt9458_chg_props);
	if (IS_ERR(ri->chg_dev)) {
		dev_info(ri->dev, "charger device register fail\n");
		return PTR_ERR(ri->chg_dev);
	}
#endif /* #if (CONFIG_MTK_GAUGE_VERSION == 30) */
	ret = rt9458_chip_irq_init(ri);
	if (ret < 0)
		return ret;
	dev_info(ri->dev, "%s end\n", __func__);
	return 0;
}

static int rt9458_i2c_remove(struct i2c_client *i2c)
{
	struct rt9458_info *ri = i2c_get_clientdata(i2c);

	dev_info(ri->dev, "%s start\n", __func__);
#if defined(CONFIG_MTK_GAUGE_VERSION) && (CONFIG_MTK_GAUGE_VERSION == 30)
	charger_device_unregister(ri->chg_dev);
#endif /* #if (CONFIG_MTK_GAUGE_VERSION == 30) */
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(ri->regmap_dev);
#endif /* #ifdef CONFIG_RT_REGMAP */
	dev_info(ri->dev, "%s end\n", __func__);
	return 0;
}

static void rt9458_i2c_shutdown(struct i2c_client *i2c)
{
	struct rt9458_info *ri = i2c_get_clientdata(i2c);

	disable_irq(ri->irq);
	rt9458_chip_reset(i2c);
}

static int rt9458_i2c_suspend(struct device *dev)
{
	struct rt9458_info *ri = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		enable_irq_wake(ri->irq);
	return 0;
}

static int rt9458_i2c_resume(struct device *dev)
{
	struct rt9458_info *ri = dev_get_drvdata(dev);

	if (device_may_wakeup(dev))
		disable_irq_wake(ri->irq);
	return 0;
}

static SIMPLE_DEV_PM_OPS(rt9458_pm_ops, rt9458_i2c_suspend, rt9458_i2c_resume);

static const struct of_device_id of_id_table[] = {
	{ .compatible = "richtek,rt9458"},
	{},
};
MODULE_DEVICE_TABLE(of, of_id_table);

static const struct i2c_device_id i2c_id_table[] = {
	{ "rt9458", 0},
	{},
};
MODULE_DEVICE_TABLE(i2c, i2c_id_table);

static struct i2c_driver rt9458_i2c_driver = {
	.driver = {
		.name = "rt9458",
		.owner = THIS_MODULE,
		.pm = &rt9458_pm_ops,
		.of_match_table = of_match_ptr(of_id_table),
	},
	.probe = rt9458_i2c_probe,
	.remove = rt9458_i2c_remove,
	.shutdown = rt9458_i2c_shutdown,
	.id_table = i2c_id_table,
};
module_i2c_driver(rt9458_i2c_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Richtek RT9458 Charger driver");
MODULE_AUTHOR("CY Huang <cy_huang@richtek.com>");
MODULE_VERSION("1.0.0");
