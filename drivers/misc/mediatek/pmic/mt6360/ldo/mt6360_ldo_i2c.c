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

#include "../inc/mt6360_ldo.h"

static bool dbg_log_en; /* module param to enable/disable debug log */
module_param(dbg_log_en, bool, 0644);

static const struct mt6360_ldo_platform_data def_platform_data = {
	.sdcard_det_en = true,
};

struct mt6360_regulator_desc {
	const struct regulator_desc desc;
	unsigned int enst_reg;
	unsigned int enst_mask;
	unsigned int mode_reg;
	unsigned int mode_mask;
	unsigned int moder_reg;
	unsigned int moder_mask;
};

static const u8 ldo_ctrl_mask[MT6360_LDO_CTRLS_NUM] = {
	0xff, 0x8f, 0xff, 0xff, 0xff
};

static int mt6360_ldo_read_device(void *client, u32 addr, int len, void *dst)
{
	struct i2c_client *i2c = client;
	struct mt6360_ldo_info *mli = i2c_get_clientdata(i2c);
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
	chunk[7] = crc8(mli->crc8_table, chunk, 2 + len, 0);
	if (chunk[2 + len] != chunk[7])
		return -EINVAL;
	memcpy(dst, chunk + 2, len);
	return 0;
}

static int mt6360_ldo_write_device(void *client, u32 addr,
				   int len, const void *src)
{
	struct i2c_client *i2c = client;
	struct mt6360_ldo_info *mli = i2c_get_clientdata(i2c);
	u8 chunk[8] = {0};

	if ((addr & 0xc0) != 0 || len > 4 || len <= 0) {
		dev_err(&i2c->dev,
			"not support addr [%x], len [%d]\n", addr, len);
		return -EINVAL;
	}
	chunk[0] = (i2c->addr & 0x7f) << 1;
	chunk[1] = (addr & 0x3f) | ((len - 1) << 6);
	memcpy(chunk + 2, src, len);
	chunk[2 + len] = crc8(mli->crc8_table, chunk, 2 + len, 0);
	return i2c_smbus_write_i2c_block_data(client, chunk[1],
					      len + 2, chunk + 2);
}

static struct rt_regmap_fops mt6360_ldo_regmap_fops = {
	.read_device = mt6360_ldo_read_device,
	.write_device = mt6360_ldo_write_device,
};

static int __maybe_unused mt6360_ldo_reg_read(struct mt6360_ldo_info *mli,
					      u8 addr)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret;

	mt_dbg(mli->dev, "%s: reg[%02x]\n", __func__, addr);
	mutex_lock(&mli->io_lock);
	ret = rt_regmap_reg_read(mli->regmap, &rrd, addr);
	mutex_unlock(&mli->io_lock);
	return (ret < 0 ? ret : rrd.rt_data.data_u8);
#else
	u8 data = 0;
	int ret;

	mt_dbg(mli->dev, "%s: reg[%02x]\n", __func__, addr);
	mutex_lock(&mli->io_lock);
	ret = mt6360_ldo_read_device(mli->i2c, addr, 1, &data);
	mutex_unlock(&mli->io_lock);
	return (ret < 0 ? ret : data);
#endif /* CONFIG_RT_REGMAP */
}

static int __maybe_unused mt6360_ldo_reg_write(struct mt6360_ldo_info *mli,
					       u8 addr, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret;

	mt_dbg(mli->dev, "%s reg[%02x] data [%02x]\n", __func__, addr, data);
	mutex_lock(&mli->io_lock);
	ret = rt_regmap_reg_write(mli->regmap, &rrd, addr, data);
	mutex_unlock(&mli->io_lock);
	return ret;
#else
	int ret;

	mt_dbg(mli->dev, "%s reg[%02x] data [%02x]\n", __func__, addr, data);
	mutex_lock(&mli->io_lock);
	ret = mt6360_ldo_write_device(mli->i2c, addr, 1, &data);
	mutex_unlock(&mli->io_lock);
	return ret;
#endif /* CONFIG_RT_REGMAP */
}

static int mt6360_ldo_reg_update_bits(struct mt6360_ldo_info *mli,
				      u8 addr, u8 mask, u8 data)
{
#ifdef CONFIG_RT_REGMAP
	struct rt_reg_data rrd = {0};
	int ret;

	mt_dbg(mli->dev,
		"%s reg[%02x], mask[%02x], data[%02x]\n",
		__func__, addr, mask, data);
	mutex_lock(&mli->io_lock);
	ret = rt_regmap_update_bits(mli->regmap, &rrd, addr, mask, data);
	mutex_unlock(&mli->io_lock);
	return ret;
#else
	u8 org = 0;
	int ret;

	mt_dbg(mli->dev,
		"%s reg[%02x], mask[%02x], data[%02x]\n",
		__func__, addr, mask, data);
	mutex_lock(&mli->io_lock);
	ret = mt6360_ldo_read_device(mli->i2c, addr, 1, &org);
	if (ret < 0)
		goto out_update_bits;
	org &= ~mask;
	org |= (data & mask);
	ret = mt6360_ldo_write_device(mli->i2c, addr, 1, &org);
out_update_bits:
	mutex_unlock(&mli->io_lock);
	return ret;
#endif /* CONFIG_RT_REGMAP */
}

static irqreturn_t mt6360_ldo_ldo1_oc_evt_handler(int irq, void *data)
{
	struct mt6360_ldo_info *mli = data;

	dev_warn(mli->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mli->rdev[MT6360_LDO_LDO1],
				      REGULATOR_EVENT_OVER_CURRENT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_ldo_ldo2_oc_evt_handler(int irq, void *data)
{
	struct mt6360_ldo_info *mli = data;

	dev_warn(mli->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mli->rdev[MT6360_LDO_LDO2],
				      REGULATOR_EVENT_OVER_CURRENT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_ldo_ldo3_oc_evt_handler(int irq, void *data)
{
	struct mt6360_ldo_info *mli = data;

	dev_warn(mli->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mli->rdev[MT6360_LDO_LDO3],
				      REGULATOR_EVENT_OVER_CURRENT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_ldo_ldo5_oc_evt_handler(int irq, void *data)
{
	struct mt6360_ldo_info *mli = data;

	dev_warn(mli->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mli->rdev[MT6360_LDO_LDO5],
				      REGULATOR_EVENT_OVER_CURRENT, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_ldo_ldo1_pgb_evt_handler(int irq, void *data)
{
	struct mt6360_ldo_info *mli = data;

	dev_warn(mli->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mli->rdev[MT6360_LDO_LDO1],
				      REGULATOR_EVENT_FAIL, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_ldo_ldo2_pgb_evt_handler(int irq, void *data)
{
	struct mt6360_ldo_info *mli = data;

	dev_warn(mli->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mli->rdev[MT6360_LDO_LDO2],
				      REGULATOR_EVENT_FAIL, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_ldo_ldo3_pgb_evt_handler(int irq, void *data)
{
	struct mt6360_ldo_info *mli = data;

	dev_warn(mli->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mli->rdev[MT6360_LDO_LDO3],
				      REGULATOR_EVENT_FAIL, NULL);
	return IRQ_HANDLED;
}

static irqreturn_t mt6360_ldo_ldo5_pgb_evt_handler(int irq, void *data)
{
	struct mt6360_ldo_info *mli = data;

	dev_warn(mli->dev, "%s\n", __func__);
	regulator_notifier_call_chain(mli->rdev[MT6360_LDO_LDO5],
				      REGULATOR_EVENT_FAIL, NULL);
	return IRQ_HANDLED;
}

static struct mt6360_ldo_irq_desc mt6360_ldo_irq_desc[] = {
	MT6360_LDO_IRQDESC(ldo1_oc_evt),
	MT6360_LDO_IRQDESC(ldo2_oc_evt),
	MT6360_LDO_IRQDESC(ldo3_oc_evt),
	MT6360_LDO_IRQDESC(ldo5_oc_evt),
	MT6360_LDO_IRQDESC(ldo1_pgb_evt),
	MT6360_LDO_IRQDESC(ldo2_pgb_evt),
	MT6360_LDO_IRQDESC(ldo3_pgb_evt),
	MT6360_LDO_IRQDESC(ldo5_pgb_evt),
};

static struct resource *mt6360_ldo_get_irq_byname(struct device *dev,
						  unsigned int type,
						  const char *name)
{
	struct mt6360_ldo_platform_data *pdata = dev_get_platdata(dev);
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

static void mt6360_ldo_irq_register(struct mt6360_ldo_info *mli)
{
	struct mt6360_ldo_irq_desc *irq_desc;
	struct resource *r;
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(mt6360_ldo_irq_desc); i++) {
		irq_desc = mt6360_ldo_irq_desc + i;
		if (unlikely(!irq_desc->name))
			continue;
		r = mt6360_ldo_get_irq_byname(mli->dev,
					      IORESOURCE_IRQ, irq_desc->name);
		if (!r)
			continue;
		irq_desc->irq = r->start;
		ret = devm_request_threaded_irq(mli->dev, irq_desc->irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						mli);
		if (ret < 0)
			dev_err(mli->dev,
				"request %s irq fail\n", irq_desc->name);
	}
}

static int mt6360_ldo_enable(struct regulator_dev *rdev)
{
	struct mt6360_ldo_info *mli = rdev_get_drvdata(rdev);
	struct mt6360_ldo_platform_data *pdata = dev_get_platdata(mli->dev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev), ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_ldo_reg_update_bits(mli, desc->enable_reg,
					 desc->enable_mask, 0xff);
	if (ret < 0) {
		dev_err(&rdev->dev, "%s: fail (%d)\n", __func__, ret);
		return ret;
	}
	/* when LDO5 enable, enable SDCARD_DET */
	if (id == MT6360_LDO_LDO5 && pdata->sdcard_det_en) {
		ret = mt6360_ldo_reg_update_bits(mli, MT6360_LDO_LDO5_CTRL0,
						 0x40, 0xff);
		if (ret < 0) {
			dev_err(&rdev->dev,
				"%s: en sdcard_det fail (%d)\n", __func__, ret);
			return ret;
		}
	}
	return 0;
}

static int mt6360_ldo_disable(struct regulator_dev *rdev)
{
	struct mt6360_ldo_info *mli = rdev_get_drvdata(rdev);
	struct mt6360_ldo_platform_data *pdata = dev_get_platdata(mli->dev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev), ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_ldo_reg_update_bits(mli, desc->enable_reg,
					 desc->enable_mask, 0);
	if (ret < 0) {
		dev_err(&rdev->dev, "%s: fail (%d)\n", __func__, ret);
		return ret;
	}
	/* when LDO5 disable, disable SDCARD_DET */
	if (id == MT6360_LDO_LDO5 && pdata->sdcard_det_en) {
		ret = mt6360_ldo_reg_update_bits(mli, MT6360_LDO_LDO5_CTRL0,
						 0x40, 0);
		if (ret < 0) {
			dev_err(&rdev->dev,
				"%s: di sdcard_det fail (%d)\n", __func__, ret);
			return ret;
		}
	}
	return 0;
}

static int mt6360_ldo_is_enabled(struct regulator_dev *rdev)
{
	struct mt6360_ldo_info *mli = rdev_get_drvdata(rdev);
	const struct mt6360_regulator_desc *desc =
			       (const struct mt6360_regulator_desc *)rdev->desc;
	int id = rdev_get_id(rdev);
	int ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_ldo_reg_read(mli, desc->enst_reg);
	if (ret < 0)
		return ret;
	return (ret & desc->enst_mask) ? 1 : 0;
}

static int mt6360_ldo_set_voltage_sel(struct regulator_dev *rdev,
				      unsigned int sel)
{
	struct mt6360_ldo_info *mli = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev);
	int shift = ffs(desc->vsel_mask) - 1, ret;

	mt_dbg(&rdev->dev, "%s, id = %d, sel %d\n", __func__, id, sel);
	ret = mt6360_ldo_reg_update_bits(mli, desc->vsel_reg,
					 desc->vsel_mask, sel << shift);
	if (ret < 0) {
		dev_err(&rdev->dev, "%s: fail (%d)\n", __func__, ret);
		return ret;
	}
	return 0;
}

static int mt6360_ldo_get_voltage_sel(struct regulator_dev *rdev)
{
	struct mt6360_ldo_info *mli = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev);
	int shift = ffs(desc->vsel_mask) - 1;
	int ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_ldo_reg_read(mli, desc->vsel_reg);
	if (ret < 0)
		return ret;
	ret &= (desc->vsel_mask);
	ret >>= shift;
	return ret;
}

static int mt6360_ldo_set_mode(struct regulator_dev *rdev, unsigned int mode)
{
	struct mt6360_ldo_info *mli = rdev_get_drvdata(rdev);
	const struct mt6360_regulator_desc *desc =
			       (const struct mt6360_regulator_desc *)rdev->desc;
	int id = rdev_get_id(rdev);
	int shift = ffs(desc->mode_mask) - 1, ret;
	u8 val;

	mt_dbg(&rdev->dev, "%s, id = %d, mode = %d\n", __func__, id, mode);
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
	ret = mt6360_ldo_reg_update_bits(mli, desc->mode_reg,
					 desc->mode_mask, val << shift);
	if (ret < 0) {
		dev_err(&rdev->dev, "%s: fail (%d)\n", __func__, ret);
		return ret;
	}
	return 0;
}

static unsigned int mt6360_ldo_get_mode(struct regulator_dev *rdev)
{
	struct mt6360_ldo_info *mli = rdev_get_drvdata(rdev);
	const struct mt6360_regulator_desc *desc =
			       (const struct mt6360_regulator_desc *)rdev->desc;
	int id = rdev_get_id(rdev);
	int shift = ffs(desc->moder_mask) - 1;
	int ret;

	mt_dbg(&rdev->dev, "%s, id = %d\n", __func__, id);
	ret = mt6360_ldo_reg_read(mli, desc->moder_reg);
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

static const struct regulator_ops mt6360_ldo_regulator_ops = {
	.list_voltage = regulator_list_voltage_linear_range,
	.enable = mt6360_ldo_enable,
	.disable = mt6360_ldo_disable,
	.is_enabled = mt6360_ldo_is_enabled,
	.set_voltage_sel = mt6360_ldo_set_voltage_sel,
	.get_voltage_sel = mt6360_ldo_get_voltage_sel,
	.set_mode = mt6360_ldo_set_mode,
	.get_mode = mt6360_ldo_get_mode,
};

static const struct regulator_linear_range ldo_volt_ranges1[] = {
	REGULATOR_LINEAR_RANGE(1200000, 0x00, 0x09, 10000),
	REGULATOR_LINEAR_RANGE(1300000, 0x0a, 0x10, 0),
	REGULATOR_LINEAR_RANGE(1310000, 0x11, 0x19, 10000),
	REGULATOR_LINEAR_RANGE(1400000, 0x1a, 0x1f, 0),
	REGULATOR_LINEAR_RANGE(1500000, 0x20, 0x29, 10000),
	REGULATOR_LINEAR_RANGE(1600000, 0x2a, 0x2f, 0),
	REGULATOR_LINEAR_RANGE(1700000, 0x30, 0x39, 10000),
	REGULATOR_LINEAR_RANGE(1800000, 0x3a, 0x40, 0),
	REGULATOR_LINEAR_RANGE(1810000, 0x41, 0x49, 10000),
	REGULATOR_LINEAR_RANGE(1900000, 0x4a, 0x4f, 0),
	REGULATOR_LINEAR_RANGE(2000000, 0x50, 0x59, 10000),
	REGULATOR_LINEAR_RANGE(2100000, 0x5a, 0x60, 0),
	REGULATOR_LINEAR_RANGE(2110000, 0x61, 0x69, 10000),
	REGULATOR_LINEAR_RANGE(2200000, 0x6a, 0x70, 0),
	REGULATOR_LINEAR_RANGE(2210000, 0x71, 0x79, 10000),
	REGULATOR_LINEAR_RANGE(2300000, 0x7a, 0x7f, 0),
	REGULATOR_LINEAR_RANGE(2700000, 0x80, 0x89, 10000),
	REGULATOR_LINEAR_RANGE(2800000, 0x8a, 0x90, 0),
	REGULATOR_LINEAR_RANGE(2810000, 0x91, 0x99, 10000),
	REGULATOR_LINEAR_RANGE(2900000, 0x9a, 0xa0, 0),
	REGULATOR_LINEAR_RANGE(2910000, 0xa1, 0xa9, 10000),
	REGULATOR_LINEAR_RANGE(3000000, 0xaa, 0xb0, 0),
	REGULATOR_LINEAR_RANGE(3010000, 0xb1, 0xb9, 10000),
	REGULATOR_LINEAR_RANGE(3100000, 0xba, 0xc0, 0),
	REGULATOR_LINEAR_RANGE(3110000, 0xc1, 0xc9, 10000),
	REGULATOR_LINEAR_RANGE(3200000, 0xca, 0xcf, 0),
	REGULATOR_LINEAR_RANGE(3300000, 0xd0, 0xd9, 10000),
	REGULATOR_LINEAR_RANGE(3400000, 0xda, 0xe0, 0),
	REGULATOR_LINEAR_RANGE(3410000, 0xe1, 0xe9, 10000),
	REGULATOR_LINEAR_RANGE(3500000, 0xea, 0xf0, 0),
	REGULATOR_LINEAR_RANGE(3510000, 0xf1, 0xf9, 10000),
	REGULATOR_LINEAR_RANGE(3600000, 0xfa, 0xff, 0),
};

static const struct regulator_linear_range ldo_volt_ranges2[] = {
	REGULATOR_LINEAR_RANGE(2700000, 0x00, 0x09, 10000),
	REGULATOR_LINEAR_RANGE(2800000, 0x0a, 0x10, 0),
	REGULATOR_LINEAR_RANGE(2810000, 0x11, 0x19, 10000),
	REGULATOR_LINEAR_RANGE(2900000, 0x1a, 0x20, 0),
	REGULATOR_LINEAR_RANGE(2910000, 0x21, 0x29, 10000),
	REGULATOR_LINEAR_RANGE(3000000, 0x2a, 0x30, 0),
	REGULATOR_LINEAR_RANGE(3010000, 0x31, 0x39, 10000),
	REGULATOR_LINEAR_RANGE(3100000, 0x3a, 0x40, 0),
	REGULATOR_LINEAR_RANGE(3110000, 0x41, 0x49, 10000),
	REGULATOR_LINEAR_RANGE(3200000, 0x4a, 0x4f, 0),
	REGULATOR_LINEAR_RANGE(3300000, 0x50, 0x59, 10000),
	REGULATOR_LINEAR_RANGE(3400000, 0x5a, 0x60, 0),
	REGULATOR_LINEAR_RANGE(3410000, 0x61, 0x69, 10000),
	REGULATOR_LINEAR_RANGE(3500000, 0x6a, 0x70, 0),
	REGULATOR_LINEAR_RANGE(3510000, 0x71, 0x79, 10000),
	REGULATOR_LINEAR_RANGE(3600000, 0x7a, 0x7f, 0),
};

#define LDO1_VOLT_RANGES	(ldo_volt_ranges1)
#define LDO1_N_VOLT_RANGES	(ARRAY_SIZE(ldo_volt_ranges1))
#define LDO1_VOUT_CNT		(256)
#define LDO2_VOLT_RANGES	(ldo_volt_ranges1)
#define LDO2_N_VOLT_RANGES	(ARRAY_SIZE(ldo_volt_ranges1))
#define LDO2_VOUT_CNT		(256)
#define LDO3_VOLT_RANGES	(ldo_volt_ranges1)
#define LDO3_N_VOLT_RANGES	(ARRAY_SIZE(ldo_volt_ranges1))
#define LDO3_VOUT_CNT		(256)
#define LDO5_VOLT_RANGES	(ldo_volt_ranges2)
#define LDO5_N_VOLT_RANGES	(ARRAY_SIZE(ldo_volt_ranges2))
#define LDO5_VOUT_CNT		(128)

#define MT6360_LDO_DESC(_name, vreg, vmask, enreg, enmask, enstreg,\
			enstmask, modereg, modemask, moderreg, modermask,\
			offon_delay) \
{\
	.desc = {\
		.name = #_name,					\
		.id =  MT6360_LDO_##_name,			\
		.owner = THIS_MODULE,				\
		.ops = &mt6360_ldo_regulator_ops,		\
		.of_match = of_match_ptr(#_name),		\
		.linear_ranges = _name##_VOLT_RANGES,		\
		.n_linear_ranges = _name##_N_VOLT_RANGES,	\
		.n_voltages = _name##_VOUT_CNT,			\
		.type = REGULATOR_VOLTAGE,			\
		.vsel_reg = vreg,				\
		.vsel_mask = vmask,				\
		.enable_reg = enreg,				\
		.enable_mask = enmask,				\
		.off_on_delay = offon_delay,			\
	},							\
	.enst_reg = enstreg,					\
	.enst_mask = enstmask,					\
	.mode_reg = modereg,					\
	.mode_mask = modemask,					\
	.moder_reg = moderreg,					\
	.moder_mask = modermask,				\
}

static const struct mt6360_regulator_desc mt6360_ldo_descs[] =  {
	MT6360_LDO_DESC(LDO1, 0x1b, 0xff, 0x17, 0x40,
			0x17, 0x04, 0x17, 0x30, 0x17, 0x03, 0),
	MT6360_LDO_DESC(LDO2, 0x15, 0xff, 0x11, 0x40,
			0x11, 0x04, 0x11, 0x30, 0x11, 0x03, 0),
	MT6360_LDO_DESC(LDO3, 0x09, 0xff, 0x05, 0x40,
			0x05, 0x04, 0x05, 0x30, 0x05, 0x03, 120),
	MT6360_LDO_DESC(LDO5, 0x0f, 0x7f, 0x0b, 0x40,
			0x0b, 0x04, 0x0b, 0x30, 0x0b, 0x03, 120),
};

static inline int mt6360_pdata_apply_helper(void *info, void *pdata,
					   const struct mt6360_pdata_prop *prop,
					   int prop_cnt)
{
	int i, ret;
	u32 val;

	for (i = 0; i < prop_cnt; i++) {
		val = *(u32 *)(pdata + prop[i].offset);
		if (prop[i].transform)
			val = prop[i].transform(val);
		val += prop[i].base;
		ret = mt6360_ldo_reg_update_bits(info,
			     prop[i].reg, prop[i].mask, val << prop[i].shift);
		if (ret < 0)
			return ret;
	}
	return 0;
}

static const struct mt6360_pdata_prop mt6360_pdata_props[] = {
};

static int mt6360_ldo_apply_pdata(struct mt6360_ldo_info *mli,
				  struct mt6360_ldo_platform_data *pdata)
{
	int ret;

	dev_dbg(mli->dev, "%s ++\n", __func__);
	ret = mt6360_pdata_apply_helper(mli, pdata, mt6360_pdata_props,
					ARRAY_SIZE(mt6360_pdata_props));
	if (ret < 0)
		return ret;
	dev_dbg(mli->dev, "%s --\n", __func__);
	return 0;
}

static const struct mt6360_val_prop mt6360_val_props[] = {
	MT6360_DT_VALPROP(sdcard_det_en, struct mt6360_ldo_platform_data),
};

static int mt6360_ldo_parse_dt_data(struct device *dev,
				    struct mt6360_ldo_platform_data *pdata)
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
bypass_irq_res:
	dev_dbg(dev, "%s --\n", __func__);
	return 0;
}

static inline int mt6360_pmic_chip_id_check(struct i2c_client *i2c)
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

static int mt6360_ldo_i2c_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	struct mt6360_ldo_platform_data *pdata = dev_get_platdata(&client->dev);
	struct mt6360_ldo_info *mli;
	bool use_dt = client->dev.of_node;
	struct regulator_config config = {};
	struct regulation_constraints *constraints;
	u8 chip_rev;
	int i, ret;

	dev_dbg(&client->dev, "%s\n", __func__);
	ret = mt6360_pmic_chip_id_check(client);
	if (ret < 0) {
		dev_err(&client->dev, "no device found\n");
		return ret;
	}
	chip_rev = (u8)ret;
	if (use_dt) {
		mt6360_config_of_node(&client->dev, "mt6360_ldo_dts");
		pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
		if (!pdata)
			return -ENOMEM;
		ret = mt6360_ldo_parse_dt_data(&client->dev, pdata);
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
	mli = devm_kzalloc(&client->dev, sizeof(*mli), GFP_KERNEL);
	if (!mli)
		return -ENOMEM;
	mli->i2c = client;
	mli->dev = &client->dev;
	mli->chip_rev = chip_rev;
	crc8_populate_msb(mli->crc8_table, 0x7);
	mutex_init(&mli->io_lock);
	i2c_set_clientdata(client, mli);
	dev_info(&client->dev, "chip_rev [%02x]\n", mli->chip_rev);

	/* regmap regiser */
	ret = mt6360_ldo_regmap_register(mli, &mt6360_ldo_regmap_fops);
	if (ret < 0) {
		dev_err(&client->dev, "regmap register fail\n");
		goto out_regmap;
	}
	/* after regmap register, apply platform data */
	ret = mt6360_ldo_apply_pdata(mli, pdata);
	if (ret < 0) {
		dev_err(&client->dev, "apply pdata fail\n");
		goto out_pdata;
	}
	/* regulator register */
	config.dev = &client->dev;
	config.driver_data = mli;
	for (i = 0; i < ARRAY_SIZE(mt6360_ldo_descs); i++) {
		config.init_data = pdata->init_data[i];
		mli->rdev[i] = devm_regulator_register(&client->dev,
						      &mt6360_ldo_descs[i].desc,
						      &config);
		if (IS_ERR(mli->rdev[i])) {
			dev_err(&client->dev,
				"fail to register  %d regulaotr\n", i);
			ret = PTR_ERR(mli->rdev[i]);
			goto out_pdata;
		}
		/* allow change mode */
		constraints = (mli->rdev[i])->constraints;
		constraints->valid_ops_mask |= REGULATOR_CHANGE_MODE;
		constraints->valid_modes_mask = REGULATOR_MODE_NORMAL |
						REGULATOR_MODE_IDLE |
						REGULATOR_MODE_STANDBY;
	}
	mt6360_ldo_irq_register(mli);
	dev_info(&client->dev, "%s: successfully probed\n", __func__);
	return 0;
out_pdata:
	mt6360_ldo_regmap_unregister(mli);
out_regmap:
	mutex_destroy(&mli->io_lock);
	return ret;
}

static int mt6360_ldo_i2c_remove(struct i2c_client *client)
{
	struct mt6360_ldo_info *mli = i2c_get_clientdata(client);

	dev_dbg(mli->dev, "%s\n", __func__);
	/* To-do: regulator unregister */
	mt6360_ldo_regmap_unregister(mli);
	mutex_destroy(&mli->io_lock);
	return 0;
}

static int __maybe_unused mt6360_ldo_i2c_suspend(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static int __maybe_unused mt6360_ldo_i2c_resume(struct device *dev)
{
	dev_dbg(dev, "%s\n", __func__);
	return 0;
}

static SIMPLE_DEV_PM_OPS(mt6360_ldo_pm_ops,
			 mt6360_ldo_i2c_suspend, mt6360_ldo_i2c_resume);

static const struct of_device_id __maybe_unused mt6360_ldo_of_id[] = {
	{ .compatible = "mediatek,mt6360_ldo", },
	{ .compatible = "mediatek,subpmic_ldo", },
	{},
};
MODULE_DEVICE_TABLE(of, mt6360_ldo_of_id);

static const struct i2c_device_id mt6360_ldo_i2c_id[] = {
	{ "mt6360_ldo", 0 },
	{},
};
MODULE_DEVICE_TABLE(i2c, mt6360_ldo_i2c_id);

static struct i2c_driver mt6360_ldo_i2c_driver = {
	.driver = {
		.name = "mt6360_ldo",
		.owner = THIS_MODULE,
		.pm = &mt6360_ldo_pm_ops,
		.of_match_table = of_match_ptr(mt6360_ldo_of_id),
	},
	.probe = mt6360_ldo_i2c_probe,
	.remove = mt6360_ldo_i2c_remove,
	.id_table = mt6360_ldo_i2c_id,
};
module_i2c_driver(mt6360_ldo_i2c_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6660 LDO Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
