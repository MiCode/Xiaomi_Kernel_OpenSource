// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/i2c.h>
#include <linux/of_irq.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/delay.h>
#ifdef FIXME
//#if defined(CONFIG_MACH_MT6779) || defined(CONFIG_MACH_MT6785)
#include <mach/mtk_pmic_wrap.h>
#include <mt-plat/upmu_common.h>
#endif
#include "../inc/mt6360_pmic.h"
#include <charger_class.h>

static bool dbg_log_en; /* module param to enable/disable debug log */
module_param(dbg_log_en, bool, 0644);

struct mt6360_regulator_desc {
	const struct regulator_desc desc;
	unsigned int enst_reg;
	unsigned int enst_mask;
	unsigned int mode_reg;
	unsigned int mode_mask;
	unsigned int moder_reg;
	unsigned int moder_mask;
};

static const struct mt6360_pmic_platform_data def_platform_data = {
	.pwr_off_seq = { 0x06, 0x04, 0x00, 0x02 },
};

static const u8 sys_ctrl_mask[MT6360_SYS_CTRLS_NUM] = {
	0xfe, 0xc0, 0xff
};

static const u8 buck_ctrl_mask[MT6360_BUCK_CTRLS_NUM] = {
	0xff, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xff, 0xff
};

static const u8 ldo_ctrl_mask[MT6360_LDO_CTRLS_NUM] = {
	0xff, 0x8f, 0x3f, 0xfe, 0xff
};

static int mt6360_pmic_read_device(void *client, u32 addr, int len, void *dst)
{
	struct i2c_client *i2c = client;
	struct mt6360_pmic_info *mpi = i2c_get_clientdata(i2c);
	u8 chunk[8] = {0};
	int ret;

	if ((addr & 0xc0) != 0 || len > 4 || len <= 0) {
		dev_err(&i2c->dev,
			"not support addr [%x], len [%d]\n", addr, len);
		return -EINVAL;
	}
	chunk[0] = ((i2c->addr & 0x7f) << 1) + 1;
	chunk[1] = (addr & 0x3f) | ((len - 1) << 6);
	ret =  i2c_smbus_read_i2c_block_data(client, chunk[1],
					     len + 1, chunk + 2);
	if (ret < 0)
		return ret;
	chunk[7] = crc8(mpi->crc8_table, chunk, 2 + len, 0);
	if (chunk[2 + len] != chunk[7])
		return -EINVAL;
	memcpy(dst, chunk + 2, len);
	return 0;
}

static int mt6360_pmic_write_device(void *client, u32 addr,
				   int len, const void *src)
{
	struct i2c_client *i2c = client;
	struct mt6360_pmic_info *mpi = i2c_get_clientdata(i2c);
	u8 chunk[8] = {0};

	if ((addr & 0xc0) != 0 || len > 4 || len <= 0) {
		dev_err(&i2c->dev,
			"not support addr [%x], len [%d]\n", addr, len);
		return -EINVAL;
	}
	chunk[0] = (i2c->addr & 0x7f) << 1;
	chunk[1] = (addr & 0x3f) | ((len - 1) << 6);
	memcpy(chunk + 2, src, len);
	chunk[2 + len] = crc8(mpi->crc8_table, chunk, 2 + len, 0);
	return i2c_smbus_write_i2c_block_data(client, chunk[1],
					      len + 2, chunk + 2);
}

static struct rt_regmap_fops mt6360_pmic_regmap_fops = {
	.read_device = mt6360_pmic_read_device,
	.write_device = mt6360_pmic_write_device,
};

static int __maybe_unused mt6360_pmic_reg_read(struct mt6360_pmic_info *mpi,
					       u8 addr)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret;

	mt_dbg(mpi->dev, "%s: reg[%02x]\n", __func__, addr);
	mutex_lock(&mpi->io_lock);
	ret = rt_regmap_reg_read(mpi->regmap, &rrd, addr);
	mutex_unlock(&mpi->io_lock);
	return (ret < 0 ? ret : rrd.rt_data.data_u8);
#else
	u8 data = 0;
	int ret;

	mt_dbg(mpi->dev, "%s: reg[%02x]\n", __func__, addr);
	mutex_lock(&mpi->io_lock);
	ret = mt6360_pmic_read_device(mpi->i2c, addr, 1, &data);
	mutex_unlock(&mpi->io_lock);
	return (ret < 0 ? ret : data);
#endif /* CONFIG_RT_REGMAP */
}

static int __maybe_unused mt6360_pmic_reg_write(struct mt6360_pmic_info *mpi,
						u8 addr, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret;

	mt_dbg(mpi->dev, "%s reg[%02x] data [%02x]\n", __func__, addr, data);
	mutex_lock(&mpi->io_lock);
	ret = rt_regmap_reg_write(mpi->regmap, &rrd, addr, data);
	mutex_unlock(&mpi->io_lock);
	return ret;
#else
	int ret;

	mt_dbg(mpi->dev, "%s reg[%02x] data [%02x]\n", __func__, addr, data);
	mutex_lock(&mpi->io_lock);
	ret = mt6360_pmic_write_device(mpi->i2c, addr, 1, &data);
	mutex_unlock(&mpi->io_lock);
	return ret;
#endif /* CONFIG_RT_REGMAP */
}

static int __maybe_unused mt6360_pmic_reg_update_bits(
			struct mt6360_pmic_info *mpi, u8 addr, u8 mask, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret;

	mt_dbg(mpi->dev,
		"%s reg[%02x], mask[%02x], data[%02x]\n",
		__func__, addr, mask, data);
	mutex_lock(&mpi->io_lock);
	ret = rt_regmap_update_bits(mpi->regmap, &rrd, addr, mask, data);
	mutex_unlock(&mpi->io_lock);
	return ret;
#else
	u8 org = 0;
	int ret;

	mt_dbg(mpi->dev,
		"%s reg[%02x], mask[%02x], data[%02x]\n",
		__func__, addr, mask, data);
	mutex_lock(&mpi->io_lock);
	ret = mt6360_pmic_read_device(mpi->i2c, addr, 1, &org);
	if (ret < 0)
		goto out_update_bits;
	org &= ~mask;
	org |= (data & mask);
	ret = mt6360_pmic_write_device(mpi->i2c, addr, 1, &org);
out_update_bits:
	mutex_unlock(&mpi->io_lock);
	return ret;
#endif /* CONFIG_RT_REGMAP */
}

static irqreturn_t mt6360_pmic_buck1_pgb_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_BUCK1],
				      REGULATOR_EVENT_FAIL, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_buck1_oc_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_BUCK1],
				      REGULATOR_EVENT_OVER_CURRENT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_buck1_ov_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_BUCK1],
				      REGULATOR_EVENT_REGULATION_OUT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_buck1_uv_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_BUCK1],
				      REGULATOR_EVENT_UNDER_VOLTAGE, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_buck2_pgb_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_BUCK2],
				      REGULATOR_EVENT_FAIL, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_buck2_oc_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_BUCK2],
				      REGULATOR_EVENT_OVER_CURRENT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_buck2_ov_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_BUCK2],
				      REGULATOR_EVENT_REGULATION_OUT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_buck2_uv_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_BUCK2],
				      REGULATOR_EVENT_UNDER_VOLTAGE, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_ldo6_oc_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_LDO6],
				      REGULATOR_EVENT_OVER_CURRENT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_ldo7_oc_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_LDO7],
				      REGULATOR_EVENT_OVER_CURRENT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_ldo6_pgb_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_LDO6],
				      REGULATOR_EVENT_FAIL, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_pmic_ldo7_pgb_evt_handler(int irq, void *data)
{
	struct mt6360_pmic_info *mpi = data;

	dev_warn(mpi->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mpi->rdev[MT6360_PMIC_LDO7],
				      REGULATOR_EVENT_FAIL, NULL);
	return IRQ_HANDLED;
}

static struct mt6360_pmic_irq_desc mt6360_pmic_irq_desc[] = {
	MT6360_PMIC_IRQDESC(buck1_pgb_evt),
	MT6360_PMIC_IRQDESC(buck1_oc_evt),
	MT6360_PMIC_IRQDESC(buck1_ov_evt),
	MT6360_PMIC_IRQDESC(buck1_uv_evt),
	MT6360_PMIC_IRQDESC(buck2_pgb_evt),
	MT6360_PMIC_IRQDESC(buck2_oc_evt),
	MT6360_PMIC_IRQDESC(buck2_ov_evt),
	MT6360_PMIC_IRQDESC(buck2_uv_evt),
	MT6360_PMIC_IRQDESC(ldo6_oc_evt),
	MT6360_PMIC_IRQDESC(ldo7_oc_evt),
	MT6360_PMIC_IRQDESC(ldo6_pgb_evt),
	MT6360_PMIC_IRQDESC(ldo7_pgb_evt),
};

static struct resource *mt6360_pmic_get_irq_byname(struct device *dev,
						   unsigned int type,
						   const char *name)
{
	struct mt6360_pmic_platform_data *pdata = dev_get_platdata(dev);
	int i;

	for (i = 0; i < pdata->irq_res_cnt; i++) {
		struct resource *r = pdata->irq_res + i;

		if (unlikely(!r->name))
			continue;

		if (type == resource_type(r) && !strcmp(r->name, name))
			return r;
	}
	return NULL;
}

static void mt6360_pmic_irq_register(struct mt6360_pmic_info *mpi)
{
	struct mt6360_pmic_irq_desc *irq_desc;
	struct resource *r;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6360_pmic_irq_desc); i++) {
		irq_desc = mt6360_pmic_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		r = mt6360_pmic_get_irq_byname(mpi->dev,
					       IORESOURCE_IRQ, irq_desc->name);
		if (!r)
			continue;
		irq_desc->irq = r->start;
		ret = devm_request_threaded_irq(mpi->dev, irq_desc->irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						mpi);
		if (ret < 0)
			dev_err(mpi->dev,
				"request %s irq fail\n", irq_desc->name);
	}
}

static int mt6360_pmic_enable(struct regulator_dev *rdev)
{
	struct mt6360_pmic_info *mpi = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev), ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_pmic_reg_update_bits(mpi, desc->enable_reg,
					  desc->enable_mask, 0xff);
	if (ret < 0) {
		dev_err(&rdev->dev, "%s: fail (%d)\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int mt6360_pmic_disable(struct regulator_dev *rdev)
{
	struct mt6360_pmic_info *mpi = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev), ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_pmic_reg_update_bits(mpi, desc->enable_reg,
					  desc->enable_mask, 0);
	if (ret < 0) {
		dev_err(&rdev->dev, "%s: fail (%d)\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int mt6360_pmic_is_enabled(struct regulator_dev *rdev)
{
	struct mt6360_pmic_info *mpi = rdev_get_drvdata(rdev);
	const struct mt6360_regulator_desc *desc =
			       (const struct mt6360_regulator_desc *)rdev->desc;
	int id = rdev_get_id(rdev);
	int ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_pmic_reg_read(mpi, desc->enst_reg);
	if (ret < 0)
		return ret;
	return (ret & desc->enst_mask) ? 1 : 0;
}

static int mt6360_enable_fpwm_usm(struct mt6360_pmic_info *mpi, bool en)
{
	int ret = 0;

	dev_dbg(mpi->dev, "%s, en = %d\n", __func__, en);
	/* Enable ultra sonic mode */
	ret = mt6360_pmic_reg_update_bits(mpi, MT6360_PMIC_BUCK1_CTRL2,
					  0x08, en ? 0xff : 0);
	if (ret < 0) {
		dev_err(mpi->dev,
			"%s: enable ultra sonic mode fail\n", __func__);
		return ret;
	}
#if defined(CONFIG_MTK_CHARGER)
	ret = charger_dev_enable_hidden_mode(mpi->chg_dev, true);
	if (ret < 0) {
		dev_err(mpi->dev, "%s: enable hidden mode fail\n", __func__);
		return ret;
	}
#endif
	/* Enable FPWM mode */
	ret = mt6360_pmic_reg_update_bits(mpi, MT6360_PMIC_BUCK1_Hidden1,
					  0x02, en ? 0xff : 0);
	if (ret < 0) {
		dev_err(mpi->dev, "%s: enable FPWM fail\n", __func__);
		goto out;
	}
out:
#if defined(CONFIG_MTK_CHARGER)
	charger_dev_enable_hidden_mode(mpi->chg_dev, false);
#endif
	return ret;
}

static int mt6360_pmic_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6360_pmic_info *mpi = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev);
	int shift = ffs(desc->vsel_mask) - 1;
	int ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_pmic_reg_read(mpi, desc->vsel_reg);
	if (ret < 0)
		return ret;
	ret &= (desc->vsel_mask);
	ret >>= shift;
	/* for LDO6/7 vocal add */
	if (id > MT6360_PMIC_BUCK2) {
		/* just use to prevent vocal over range */
		if ((ret & 0x0f) > 0x0a)
			ret = (ret & 0xf0) | 0x0a;
		ret = ((ret & 0xf0) >> 4) * 10 + (ret & 0x0f);
	}
	return ret;
}

static int mt6360_pmic_set_voltage_sel(struct regulator_dev *rdev,
				       unsigned int sel)
{
	struct mt6360_pmic_info *mpi = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev);
	int shift = ffs(desc->vsel_mask) - 1, ret;
	u8 dvfs_down = 0;

	mt_dbg(&rdev->dev, "%s, id = %d, sel 0x%02x\n", __func__, id, sel);
	if ((id == MT6360_PMIC_BUCK1 || id == MT6360_PMIC_BUCK2) &&
	    mpi->chip_rev <= 0x02) {
		ret = mt6360_pmic_get_voltage_sel(rdev);
		if (ret < 0)
			return ret;
		dvfs_down = (ret > sel) ? true : false;
		if (dvfs_down) {
			/* Enable FPWM Mode */
			ret = mt6360_enable_fpwm_usm(mpi, true);
			if (ret < 0)
				return ret;
			mdelay(1);
		}
	}
	/* for LDO6/7 vocal add */
	if (id > MT6360_PMIC_BUCK2)
		sel = (sel >= 160) ? 0xfa : (((sel / 10) << 4) + sel % 10);
#ifdef FIXME
//#if defined(CONFIG_MACH_MT6779) || defined(CONFIG_MACH_MT6785)
	if (id == MT6360_PMIC_BUCK1)
		pwrap_write(MT6359_RG_SPI_CON12, sel);
#endif
	ret = mt6360_pmic_reg_update_bits(mpi, desc->vsel_reg,
					  desc->vsel_mask, sel << shift);
	if (ret < 0)
		dev_err(&rdev->dev, "%s: fail(%d)\n", __func__, ret);
	if ((id == MT6360_PMIC_BUCK1 || id == MT6360_PMIC_BUCK2) &&
	    mpi->chip_rev <= 0x02) {
		if (dvfs_down) {
			udelay(200);
			/* Disable FPWM Mode */
			ret = mt6360_enable_fpwm_usm(mpi, false);
			if (ret < 0)
				dev_err(&rdev->dev,
					"%s: disable fpwm fail\n", __func__);
		}
	}
	return ret;
}

static int mt6360_pmic_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct mt6360_pmic_info *mpi = rdev_get_drvdata(rdev);
	const struct mt6360_regulator_desc *desc =
			       (const struct mt6360_regulator_desc *)rdev->desc;
	int id = rdev_get_id(rdev);
	int shift = ffs(desc->mode_mask) - 1, ret;
	u8 val;

	dev_dbg(&rdev->dev, "%s, id = %d, mode = %d\n", __func__, id, mode);
	if (mpi->chip_rev <= 0x02)
		return -ENOTSUPP;
	if (!mode)
		return -EINVAL;
	switch (1 << (ffs(mode) - 1)) {
	case REGULATOR_MODE_NORMAL:
		val = 0;
		break;
	case REGULATOR_MODE_IDLE:
		val = 2;
		break;
	case REGULATOR_MODE_STANDBY:
		val = 1;
		break;
	default:
		return -ENOTSUPP;
	}
	ret = mt6360_pmic_reg_update_bits(mpi, desc->mode_reg,
					  desc->mode_mask, val << shift);
	if (ret < 0) {
		dev_err(&rdev->dev, "%s: fail (%d)\n", __func__, ret);
		return ret;
	}
	return 0;
}

static unsigned int mt6360_pmic_get_mode(struct regulator_dev *rdev)
{
	struct mt6360_pmic_info *mpi = rdev_get_drvdata(rdev);
	const struct mt6360_regulator_desc *desc =
			       (const struct mt6360_regulator_desc *)rdev->desc;
	int id = rdev_get_id(rdev);
	int shift = ffs(desc->moder_mask) - 1;
	int ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_pmic_reg_read(mpi, desc->moder_reg);
	if (ret < 0)
		return ret;
	ret &= desc->moder_mask;
	ret >>= shift;
	switch (ret) {
	case 0:
		ret = REGULATOR_MODE_NORMAL;
		break;
	case 2:
		ret = REGULATOR_MODE_IDLE;
		break;
	case 3:
		ret = REGULATOR_MODE_STANDBY;
		break;
	default:
		return -EINVAL;
	}
	return ret;
}

static const struct regulator_ops mt6360_pmic_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear,
	.enable = mt6360_pmic_enable,
	.disable = mt6360_pmic_disable,
	.is_enabled = mt6360_pmic_is_enabled,
	.set_voltage_sel = mt6360_pmic_set_voltage_sel,
	.get_voltage_sel = mt6360_pmic_get_voltage_sel,
	.set_mode = mt6360_pmic_set_mode,
	.get_mode = mt6360_pmic_get_mode,
};

#define BUCK1_VOUT_CNT	(0xc9)
#define BUCK2_VOUT_CNT	(0xc9)
#define LDO6_VOUT_CNT	(161)
#define LDO7_VOUT_CNT	(161)

#define MT6360_PMIC_DESC(_name, _min, _stp, vreg, vmask, enreg, enmask,\
			 enstreg, enstmask, modereg, modemask,\
			 moderreg, modermask) \
{\
	.desc = {\
		.name = #_name,					\
		.id =  MT6360_PMIC_##_name,			\
		.owner = THIS_MODULE,				\
		.ops = &mt6360_pmic_regulator_ops,		\
		.of_match = of_match_ptr(#_name),		\
		.min_uV = _min,					\
		.uV_step = _stp,				\
		.n_voltages = _name##_VOUT_CNT,			\
		.type = REGULATOR_VOLTAGE,			\
		.vsel_reg = vreg,				\
		.vsel_mask = vmask,				\
		.enable_reg = enreg,				\
		.enable_mask = enmask,				\
	},							\
	.enst_reg = enstreg,					\
	.enst_mask = enstmask,					\
	.mode_reg = modereg,					\
	.mode_mask = modemask,					\
	.moder_reg = moderreg,					\
	.moder_mask = modermask,				\
}

static const struct mt6360_regulator_desc mt6360_pmic_descs[] =  {
	MT6360_PMIC_DESC(BUCK1, 300000, 5000, 0x10, 0xff,
			 0x17, 0x40, 0x17, 0x04, 0x17, 0x30, 0x17, 0x03),
	MT6360_PMIC_DESC(BUCK2, 300000, 5000, 0x20, 0xff,
			 0x27, 0x40, 0x27, 0x04, 0x27, 0x30, 0x27, 0x03),
	MT6360_PMIC_DESC(LDO6, 500000, 10000, 0x3b, 0xff,
			 0x37, 0x40, 0x37, 0x04, 0x37, 0x30, 0x37, 0x03),
	MT6360_PMIC_DESC(LDO7, 500000, 10000, 0x35, 0xff,
			 0x31, 0x40, 0x31, 0x04, 0x31, 0x30, 0x31, 0x03),
};

static u32 buck_vol_to_sel(u32 val)
{
	return DIV_ROUND_UP(val - 300000, 5000);
}

static bool buck_vol_rewrite_needed(u32 val)
{
	return val ? true : false;
}

static inline int mt6360_pdata_apply_helper(void *info, void *pdata,
					   const struct mt6360_pdata_prop *prop,
					   int prop_cnt)
{
	int i, ret;
	u32 val;

	for (i = 0; i < prop_cnt; i++) {
		val = *(u32 *)(pdata + prop[i].offset);
		if (prop[i].rewrite_needed && !prop[i].rewrite_needed(val))
			continue;
		if (prop[i].transform)
			val = prop[i].transform(val);
		val += prop[i].base;
		ret = mt6360_pmic_reg_update_bits(info,
			     prop[i].reg, prop[i].mask, val << prop[i].shift);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
	MT6360_PDATA_VALPROP(buck1_lp_vout, struct mt6360_pmic_platform_data,
			     MT6360_PMIC_BUCK1_LP_VOSEL, 0, 0xff,
			     buck_vol_to_sel, 0, buck_vol_rewrite_needed),
	MT6360_PDATA_VALPROP(buck2_lp_vout, struct mt6360_pmic_platform_data,
			     MT6360_PMIC_BUCK2_LP_VOSEL, 0, 0xff,
			     buck_vol_to_sel, 0, buck_vol_rewrite_needed),
};

static int mt6360_pmic_apply_pdata(struct mt6360_pmic_info *mpi,
				   struct mt6360_pmic_platform_data *pdata)
{
	int ret;

	dev_dbg(mpi->dev, "%s ++\n", __func__);
	ret = mt6360_pdata_apply_helper(mpi, pdata, mt6360_pdata_props,
					ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0)
		return ret;
	dev_dbg(mpi->dev, "%s --\n", __func__);
	return 0;
}

static const struct mt6360_val_prop mt6360_val_props[] = {
	MT6360_DT_VALPROP(buck1_lp_vout, struct mt6360_pmic_platform_data),
	MT6360_DT_VALPROP(buck2_lp_vout, struct mt6360_pmic_platform_data),
};

static int mt6360_pmic_parse_dt_data(struct device *dev,
				     struct mt6360_pmic_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	struct resource *res;
	int res_cnt, ret;

	dev_dbg(dev, "%s ++\n", __func__);
	memcpy(pdata, &def_platform_data, sizeof(*pdata));
	mt6360_dt_parser_helper(np, (void *)pdata,
				mt6360_val_props, ARRAY_SIZE(mt6360_val_props));
	res_cnt = of_irq_count(np);
	if (!res_cnt) {
		dev_info(dev, "no irqs specified\n");
		goto bypass_irq_res;
	}
	res = devm_kzalloc(dev, res_cnt * sizeof(*res), GFP_KERNEL);
	if (!res)
		return -ENOMEM;
	ret = of_irq_to_resource_table(np, res, res_cnt);
	pdata->irq_res = res;
	pdata->irq_res_cnt = ret;
	of_property_read_u8_array(np, "pwr_off_seq",
				  pdata->pwr_off_seq, MT6360_PMIC_MAX);
bypass_irq_res:
	dev_dbg(dev, "%s --\n", __func__);
	return 0;
}

static inline int mt6360_ldo_chip_id_check(struct i2c_client *i2c)
{
	struct i2c_client pmu_client;
	int ret;

	memcpy(&pmu_client, i2c, sizeof(*i2c));
	pmu_client.addr = 0x34;
	ret = i2c_smbus_read_byte_data(&pmu_client, 0x00);
	if (ret < 0)
		return ret;
	if ((ret & 0xf0) != 0x50)
		return -ENODEV;
	return (ret & 0x0f);
}

static inline void mt6360_config_of_node(struct device *dev, const char *name)
{
	struct device_node *np = NULL;

	if (unlikely(!dev) || unlikely(!name))
		return;
	np = of_find_node_by_name(NULL, name);
	if (np) {
		dev_info(dev, "find %s node\n", name);
		dev->of_node = np;
	}
}

static int mt6360_pmic_init_setting(struct mt6360_pmic_info *mpi)
{
	int ret = 0;
#if defined(CONFIG_MTK_CHARGER)
	ret = charger_dev_enable_hidden_mode(mpi->chg_dev, true);
	if (ret < 0) {
		dev_err(mpi->dev, "%s: enable hidden mode fail\n", __func__);
		return ret;
	}
#endif
	/* Set USM Load Selection to 10mA */
	ret = mt6360_pmic_reg_update_bits(mpi, MT6360_PMIC_BUCK1_Hidden1,
					  0x1c, 0);
	if (ret < 0) {
		dev_err(mpi->dev, "%s: enable FPWM fail\n", __func__);
		goto out;
	}
out:
#if defined(CONFIG_MTK_CHARGER)
	charger_dev_enable_hidden_mode(mpi->chg_dev, false);
#endif
	return ret;
}

static int mt6360_pmic_i2c_probe(struct i2c_client *client,
				 const struct i2c_device_id *id)
{
	struct mt6360_pmic_platform_data *pdata =
					dev_get_platdata(&client->dev);
	struct mt6360_pmic_info *mpi;
	bool use_dt = client->dev.of_node;
	struct regulator_config config = {};
	struct regulation_constraints *constraints;
	u8 chip_rev;
	int i, ret;

	dev_dbg(&client->dev, "%s\n", __func__);
	ret = mt6360_ldo_chip_id_check(client);
	if (ret < 0) {
		dev_err(&client->dev, "no device found\n");
		return ret;
	}
	chip_rev = (u8)ret;
	if (use_dt) {
		mt6360_config_of_node(&client->dev, "mt6360_pmic_dts");
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = mt6360_pmic_parse_dt_data(&client->dev, pdata);
		if (ret < 0) {
			dev_err(&client->dev, "parse dt fail\n");
			return ret;
		}
		client->dev.platform_data = pdata;
	}
	if (!pdata) {
		dev_err(&client->dev, "no platform data specified\n");
		return -EINVAL;
	}
	mpi = devm_kzalloc(&client->dev, sizeof(*mpi), GFP_KERNEL);
	if (!mpi)
		return -ENOMEM;
	mpi->i2c = client;
	mpi->dev = &client->dev;
	mpi->chip_rev = chip_rev;
	crc8_populate_msb(mpi->crc8_table, 0x7);
	mutex_init(&mpi->io_lock);
	i2c_set_clientdata(client, mpi);
	dev_info(&client->dev, "chip_rev [%02x]\n", mpi->chip_rev);

	/* regmap regiser */
	ret = mt6360_pmic_regmap_register(mpi, &mt6360_pmic_regmap_fops);
	if (ret < 0) {
		dev_err(&client->dev, "regmap register fail\n");
		goto out_regmap;
	}
	/* after regmap register, apply platform data */
	ret = mt6360_pmic_apply_pdata(mpi, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "apply pdata fail\n");
		goto out_pdata;
	}
#if defined(CONFIG_MTK_CHARGER)
	/* get charger device for dvfs in FPWM mode */
	mpi->chg_dev = get_charger_by_name("primary_chg");
	if (!mpi->chg_dev) {
		dev_err(&client->dev, "%s: get charger device fail\n",
			__func__);
		goto out_pdata;
	}
#endif
	ret = mt6360_pmic_init_setting(mpi);
	if (ret < 0) {
		dev_err(&client->dev, "%s: init setting fail\n", __func__);
		goto out_pdata;
	}

	/* regulator register */
	config.dev = &client->dev;
	config.driver_data = mpi;
	for (i = 0; i < ARRAY_SIZE(mt6360_pmic_descs); i++) {
		config.init_data = pdata->init_data[i];
		mpi->rdev[i] = devm_regulator_register(&client->dev,
						     &mt6360_pmic_descs[i].desc,
						     &config);
		if (IS_ERR(mpi->rdev[i])) {
			dev_err(&client->dev,
				"fail to register  %d regulaotr\n", i);
			ret = PTR_ERR(mpi->rdev[i]);
			goto out_pdata;
		}
		/* allow change mode */
		constraints = (mpi->rdev[i])->constraints;
		constraints->valid_ops_mask |= REGULATOR_CHANGE_MODE;
		constraints->valid_modes_mask = REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_IDLE |
						REGULATOR_MODE_STANDBY;
	}
	mt6360_pmic_irq_register(mpi);
	dev_info(&client->dev, "%s: successfully probed\n", __func__);
#ifdef FIXME
//#if defined(CONFIG_MACH_MT6779) || defined(CONFIG_MACH_MT6785)
	/* MT6359 record VMDLA vosel */
	ret = mt6360_pmic_reg_read(mpi,
		mt6360_pmic_descs[MT6360_PMIC_BUCK1].desc.vsel_reg);
	if (ret < 0)
		return ret;
	pwrap_write(MT6359_RG_SPI_CON12, ret);
#endif

	return 0;
out_pdata:
	mt6360_pmic_regmap_unregister(mpi);
out_regmap:
	mutex_destroy(&mpi->io_lock);
	return ret;
}

static int mt6360_pmic_i2c_remove(struct i2c_client *client)
{
	struct mt6360_pmic_info *mpi = i2c_get_clientdata(client);

	dev_dbg(mpi->dev, "%s\n", __func__);
	/* To-do: regulator unregister */
	mt6360_pmic_regmap_unregister(mpi);
	mutex_destroy(&mpi->io_lock);
	return 0;
}

static int mt6360_pmic_enable_poweroff_sequence(struct mt6360_pmic_info *mpi,
						bool en)
{
	struct mt6360_pmic_platform_data *pdata = dev_get_platdata(mpi->dev);
	int i, ret = 0;

	dev_dbg(mpi->dev, "%s: en = %d\n", __func__, en);
	for (i = 0; i < 4; i++) {
		ret = mt6360_pmic_reg_write(mpi,
					    MT6360_PMIC_BUCK1_SEQOFFDLY + i,
					    en ? pdata->pwr_off_seq[i] : 0);
		if (ret < 0) {
			dev_notice(mpi->dev, "%s: set buck(%d) fail\n",
				__func__, i);
			return ret;
		}
	}
	return ret;
}

static void mt6360_pmic_shutdown(struct i2c_client *client)
{
	struct mt6360_pmic_info *mpi = i2c_get_clientdata(client);
	int ret = 0;

	dev_dbg(mpi->dev, "%s\n", __func__);
	if (mpi == NULL)
		return;
	ret = mt6360_pmic_enable_poweroff_sequence(mpi, true);
	if (ret < 0) {
		dev_notice(mpi->dev, "%s: enable power off sequence fail\n",
			__func__);
		return;
	}
}

static int __maybe_unused mt6360_pmic_i2c_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int __maybe_unused mt6360_pmic_i2c_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_pmic_pm_ops,
			 mt6360_pmic_i2c_suspend, mt6360_pmic_i2c_resume);

static const struct of_device_id __maybe_unused mt6360_pmic_of_id[] = {
	{ .compatible = "mediatek,mt6360_pmic", },
	{ .compatible = "mediatek,subpmic_pmic", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_pmic_of_id);

static const struct i2c_device_id mt6360_pmic_i2c_id[] = {
	{ "mt6360_pmic", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6360_pmic_i2c_id);

static struct i2c_driver mt6360_pmic_i2c_driver = {
	.driver = {
		.name = "mt6360_pmic",
		.owner = THIS_MODULE,
		.pm = &mt6360_pmic_pm_ops,
		.of_match_table = of_match_ptr(mt6360_pmic_of_id),
	},
	.probe = mt6360_pmic_i2c_probe,
	.remove = mt6360_pmic_i2c_remove,
	.shutdown = mt6360_pmic_shutdown,
	.id_table = mt6360_pmic_i2c_id,
};
module_i2c_driver(mt6360_pmic_i2c_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6660 PMIC Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
