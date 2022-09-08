// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/crc8.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <linux/mfd/mt6370/mt6370.h>
#include <linux/mfd/mt6370/mt6370-private.h>

#define LDO	0
#define SHUTDOWN 0
#define MT6370_PMIC_REGMAX		(0xB5)

enum {
	MT6370_PMIC_DSVP = 0,
	MT6370_PMIC_DSVN,
	MT6370_PMIC_MAX,
};

//#define MT6370_MAX_REGULATOR		(2)

/* mask and shift definition */
#define MT6360_MASK_SDCARD_DET_EN	BIT(6)

#define MT6360_OPMODE_LP		(2)
#define MT6360_OPMODE_ULP		(3)
#define MT6360_OPMODE_NORMAL		(0)

struct mt6370_pmu_dsv_platform_data {
	union {
		uint8_t raw;
		struct {
			uint8_t db_ext_en:1;
			uint8_t reserved:3;
			uint8_t db_periodic_fix:1;
			uint8_t db_single_pin:1;
			uint8_t db_freq_pm:1;
			uint8_t db_periodic_mode:1;
		};
	} db_ctrl1;

	union {
		uint8_t raw;
		struct {
			uint8_t db_startup:1;
			uint8_t db_vneg_20ms:1;
			uint8_t db_vneg_disc:1;
			uint8_t reserved:1;
			uint8_t db_vpos_20ms:1;
			uint8_t db_vpos_disc:1;
		};
	} db_ctrl2;

	union {
		uint8_t raw;
		struct {
			uint8_t vbst:6;
			uint8_t delay:2;
		} bitfield;
	} db_vbst;

	uint8_t db_vpos_slew;
	uint8_t db_vneg_slew;
};

struct dbctrl_bitfield_desc {
	const char *name;
	uint8_t shift;
};

static const struct dbctrl_bitfield_desc dbctrl1_desc[] = {
	{ "db_ext_en", 0 },
	{ "db_periodic_fix", 4 },
	{ "db_single_pin", 5 },
	{ "db_freq_pm", 6 },
	{ "db_periodic_mode", 7 },
};

static const struct dbctrl_bitfield_desc dbctrl2_desc[] = {
	{ "db_startup", 0 },
	{ "db_vneg_20ms", 1 },
	{ "db_vneg_disc", 2 },
	{ "db_vpos_20ms", 4 },
	{ "db_vpos_disc", 5 },
};

struct mt6370_regulator_desc {//mt6360_regulator_desc
	const struct regulator_desc desc;
	//unsigned int control_reg;
	//unsigned int mode_set_mask;
	//unsigned int mode_get_mask;
};

struct mt6370_regulator_devdata {
	int i2c_idx;
	const struct regmap_config *regmap_config;
	const struct mt6370_regulator_desc *reg_descs;
	int num_reg_descs;
	const struct mt6370_pmu_irq_desc *irq_descs;
	int num_irq_descs;
};

struct mt6370_regulator_info {
	struct i2c_client *i2c;
	struct device *dev;
	struct regulator_dev *rdev[MT6370_PMIC_MAX];
	struct regmap *regmap;
	unsigned int chip_rev;
	u8 crc8_table[CRC8_TABLE_SIZE];
};

static inline int mt_parse_dt(struct device *dev,
		struct mt6370_pmu_dsv_platform_data *pdata,
		struct mt6370_pmu_dsv_platform_data *mask)
{
	struct device_node *np = dev->of_node;
	int i;
	uint32_t val = 0;

	for (i = 0; i < ARRAY_SIZE(dbctrl1_desc); i++) {
		if (of_property_read_u32(np, dbctrl1_desc[i].name, &val) == 0) {
			mask->db_ctrl1.raw |= (1 << dbctrl1_desc[i].shift);
			pdata->db_ctrl1.raw |= (val << dbctrl1_desc[i].shift);
		}
	}

	for (i = 0; i < ARRAY_SIZE(dbctrl2_desc); i++) {
		if (of_property_read_u32(np, dbctrl2_desc[i].name, &val) == 0) {
			mask->db_ctrl2.raw |= (1 << dbctrl2_desc[i].shift);
			pdata->db_ctrl2.raw |= (val << dbctrl2_desc[i].shift);
		}
	}

	if (of_property_read_u32(np, "db_delay", &val) == 0) {
		mask->db_vbst.bitfield.delay = 0x3;
		pdata->db_vbst.bitfield.delay = val;
	}

	if (of_property_read_u32(np, "db_vbst", &val) == 0) {
		if (val >= 4000 && val <= 6200) {
			mask->db_vbst.bitfield.vbst = 0x3F;
			pdata->db_vbst.bitfield.vbst = (val - 4000) / 50;
		}
	}

	if (of_property_read_u32(np, "db_vpos_slew", &val) == 0) {
		mask->db_vpos_slew = 0x3  <<  6;
		pdata->db_vpos_slew = val  <<  6;
	}

	if (of_property_read_u32(np, "db_vneg_slew", &val) == 0) {
		mask->db_vneg_slew = 0x3  <<  6;
		pdata->db_vneg_slew = val  <<  6;
	}
	return 0;
}

static int dsv_apply_dts(struct regmap *map,
		struct mt6370_pmu_dsv_platform_data *pdata,
		struct mt6370_pmu_dsv_platform_data *mask)
{
	int ret = 0;

	if (mask->db_ctrl1.raw)
		ret = regmap_update_bits(map, MT6370_PMU_REG_DBCTRL1,
				mask->db_ctrl1.raw, pdata->db_ctrl1.raw);
	if (ret < 0)
		return ret;
	if (mask->db_ctrl2.raw)
		ret = regmap_update_bits(map, MT6370_PMU_REG_DBCTRL2,
				mask->db_ctrl2.raw, pdata->db_ctrl2.raw);
	if (ret < 0)
		return ret;
	if (mask->db_vbst.raw)
		ret = regmap_update_bits(map, MT6370_PMU_REG_DBVBST,
				mask->db_vbst.raw, pdata->db_vbst.raw);
	if (ret < 0)
		return ret;
	if (mask->db_vpos_slew)
		ret = regmap_update_bits(map, MT6370_PMU_REG_DBVPOS,
				mask->db_vpos_slew, pdata->db_vpos_slew);
	if (ret < 0)
		return ret;
	if (mask->db_vneg_slew)
		ret = regmap_update_bits(map, MT6370_PMU_REG_DBVNEG,
				mask->db_vneg_slew, pdata->db_vneg_slew);
	if (ret < 0)
		return ret;

	return ret;
}

#define MT6370_REGU_IRQH(_name, _rid, _event) \
static irqreturn_t mt6370_pmu_##_name##_handler(int irq, void *data) \
{ \
	struct mt6370_regulator_info *mri = data; \
	dev_warn(mri->dev, "%s\n", __func__); \
	regulator_notifier_call_chain(mri->rdev[_rid], _event, NULL); \
	return IRQ_HANDLED; \
}

#define MT6370_REGU_IRQ(_name) { #_name, mt6370_pmu_##_name##_handler }

/* PMIC irqs */
MT6370_REGU_IRQH(dsv_vpos_ocp, MT6370_PMIC_DSVP, REGULATOR_EVENT_OVER_CURRENT)
MT6370_REGU_IRQH(dsv_vneg_ocp, MT6370_PMIC_DSVN, REGULATOR_EVENT_OVER_CURRENT)
//MT6370_REGU_IRQH(dsv_bst_ocp, 2, REGULATOR_EVENT_OVER_CURRENT)

static const struct mt6370_pmu_irq_desc mt6370_pmic_irq_descs[] = {
	MT6370_REGU_IRQ(dsv_vneg_ocp),
	MT6370_REGU_IRQ(dsv_vpos_ocp),
	//MT6370_REGU_IRQ(dsv_bst_ocp),
};

static int mt6370_regulator_irq_register(struct platform_device *pdev,
				       struct mt6370_regulator_devdata *devdata)
{
	const struct mt6370_pmu_irq_desc *irq_desc;
	int i, irq, ret;

	for (i = 0; i < devdata->num_irq_descs; i++) {
		irq_desc = devdata->irq_descs + i;
		if (unlikely(!irq_desc->name))
			continue;
		irq = platform_get_irq_byname(pdev, irq_desc->name);
		if (irq < 0)
			continue;
		ret = devm_request_threaded_irq(&pdev->dev, irq, NULL,
						irq_desc->irq_handler,
						IRQF_TRIGGER_FALLING,
						irq_desc->name,
						platform_get_drvdata(pdev));
		if (ret < 0) {
			dev_err(&pdev->dev,
				"request %s irq fail\n", irq_desc->name);
			return ret;
		}
	}
	return 0;
}

#if LDO
static unsigned int mt6360_regulator_of_map_mode(unsigned int hw_mode)
{
	unsigned int trans_mode = 0;

	switch (hw_mode) {
	case MT6360_OPMODE_NORMAL:
		trans_mode = REGULATOR_MODE_NORMAL;
		break;
	case MT6360_OPMODE_LP:
		trans_mode = REGULATOR_MODE_IDLE;
		break;
	case MT6360_OPMODE_ULP:
		trans_mode = REGULATOR_MODE_STANDBY;
		break;
	}
	return trans_mode;
}
#endif

static const struct regulator_ops mt6370_pmic_regulator_ops = {//mt6360_pmic_regulator_ops
	.list_voltage = regulator_list_voltage_linear,
	.enable = regulator_enable_regmap,
	.disable = regulator_disable_regmap,
	.is_enabled = regulator_is_enabled_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_regmap,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	//.set_mode = mt6360_regulator_set_mode,
	//.get_mode = mt6360_regulator_get_mode,
};

//DEL: _ctrlreg, _modesmask, _modegmask
#define MT6370_PMIC_DESC(_name, _min, _stp, _cnt, _vreg, _vmask, \
			 _enreg, _enmask) \
{									\
	.desc = {							\
		.name = #_name,						\
		.id =  MT6370_PMIC_##_name,				\
		.owner = THIS_MODULE,					\
		.ops = &mt6370_pmic_regulator_ops,			\
		.of_match = of_match_ptr(#_name),			\
		.min_uV = _min,						\
		.uV_step = _stp,					\
		.n_voltages = _cnt,					\
		.type = REGULATOR_VOLTAGE,				\
		.vsel_reg = _vreg,					\
		.vsel_mask = _vmask,					\
		.enable_reg = _enreg,					\
		.enable_mask = _enmask,					\
	},								\
}

#if LDO
static int mt6360_ldo_enable(struct regulator_dev *rdev)
{
	struct mt6360_regulator_info *mri = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev), ret;

	ret = regmap_update_bits(mri->regmap, desc->enable_reg,
				 desc->enable_mask, 0xff);
	if (ret < 0) {
		dev_notice(mri->dev, "%s: fail\n", __func__);
		return ret;
	}

	/* when LDO5 enable, enable SDCARD_DET */
	if (id == MT6360_LDO_LDO5)
		ret = regmap_update_bits(mri->regmap, MT6360_LDO_LDO5_CTRL0,
					 MT6360_MASK_SDCARD_DET_EN, 0xff);
	return ret;
}

static int mt6360_ldo_disable(struct regulator_dev *rdev)
{
	struct mt6360_regulator_info *mri = rdev_get_drvdata(rdev);
	const struct regulator_desc *desc = rdev->desc;
	int id = rdev_get_id(rdev), ret;

	ret = regmap_update_bits(mri->regmap, desc->enable_reg,
				 desc->enable_mask, 0);
	if (ret < 0) {
		dev_notice(mri->dev, "%s: fail\n", __func__);
		return ret;
	}

	/* when LDO5 disable, disable SDCARD_DET */
	if (id == MT6360_LDO_LDO5)
		ret = regmap_update_bits(mri->regmap, MT6360_LDO_LDO5_CTRL0,
					 MT6360_MASK_SDCARD_DET_EN, 0);
	return ret;
}
#endif


static const struct mt6370_regulator_desc mt6370_pmic_descs[] =  {
	MT6370_PMIC_DESC(DSVP, 4000000, 50000, 41, MT6370_PMU_REG_DBVBST,
			 0x3f, MT6370_PMU_REG_DBCTRL2, 0x40),
	MT6370_PMIC_DESC(DSVN, 4000000, 50000, 41, MT6370_PMU_REG_DBVBST,
			 0x3f, MT6370_PMU_REG_DBCTRL2, 0x8),
};


static int mt6370_regulator_reg_write(void *context,
				      unsigned int reg, unsigned int val)
{
	struct mt6370_regulator_info *mri = context;
	u8 chunk[5] = {0};

	/* chunk 0 ->i2c addr, 1 -> reg_addr, 2 -> reg_val 3-> crc8 */
	chunk[0] = (mri->i2c->addr & 0x7f) << 1;
	chunk[1] = reg & 0x3f;
	chunk[2] = (u8)val;
	chunk[3] = crc8(mri->crc8_table, chunk, 3, 0);
	/* also dummy one byte */
	return i2c_smbus_write_i2c_block_data(mri->i2c, chunk[1], 3, chunk + 2);
}

static int mt6370_regulator_reg_read(void *context,
				     unsigned int reg, unsigned int *val)
{
	struct mt6370_regulator_info *mri = context;
	u8 chunk[5] = {0};
	int ret;

	/* chunk 0->i2c addr, 1->reg_addr, 2->reg_val, 3->crc8, 4->crck */
	chunk[0] = ((mri->i2c->addr & 0x7f) << 1) + 1;
	chunk[1] = reg & 0x3f;
	ret =  i2c_smbus_read_i2c_block_data(mri->i2c, chunk[1], 2, chunk + 2);
	if (ret < 0)
		return ret;
	chunk[4] = crc8(mri->crc8_table, chunk, 3, 0);
	if (chunk[3] != chunk[4])
		return -EINVAL;
	*val = chunk[2];
	return 0;
}

static const struct regmap_config mt6370_pmic_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.reg_read = mt6370_regulator_reg_read,
	.reg_write = mt6370_regulator_reg_write,
	.max_register = MT6370_PMIC_REGMAX,
	.use_single_read = true,
	.use_single_write = true,
};
#if LDO
static const struct regmap_config mt6360_ldo_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.reg_read = mt6360_regulator_reg_read,
	.reg_write = mt6360_regulator_reg_write,
	.max_register = MT6370_LDO_REGMAX,
	.use_single_read = true,
	.use_single_write = true,
};
#endif

static const struct mt6370_regulator_devdata mt6370_pmic_devdata = {
	.i2c_idx = MT6370_SLAVE_PMU,
	.regmap_config = &mt6370_pmic_regmap_config,
	.reg_descs = mt6370_pmic_descs,
	.num_reg_descs = ARRAY_SIZE(mt6370_pmic_descs),
	.irq_descs = mt6370_pmic_irq_descs,
	.num_irq_descs = ARRAY_SIZE(mt6370_pmic_irq_descs),
};

#if LDO
static const struct mt6360_regulator_devdata mt6360_ldo_devdata = {
	.i2c_idx = MT6360_SLAVE_LDO,
	.regmap_config = &mt6360_ldo_regmap_config,
	.reg_descs = mt6360_ldo_descs,
	.num_reg_descs = ARRAY_SIZE(mt6360_ldo_descs),
	.irq_descs = mt6360_ldo_irq_descs,
	.num_irq_descs = ARRAY_SIZE(mt6360_ldo_irq_descs),
};
#endif

static const struct of_device_id __maybe_unused mt6370_regulator_of_id[] = {
	{
		.compatible = "mediatek,mt6370_pmu_dsv",
		.data = (void *)&mt6370_pmic_devdata,
	},
	{},
};
MODULE_DEVICE_TABLE(of, mt6370_regulator_of_id);

static int mt6370_regulator_probe(struct platform_device *pdev)
{
	struct mt6370_pmu_data *pmu_info = dev_get_drvdata(pdev->dev.parent);
	struct mt6370_regulator_devdata *devdata;
	struct mt6370_regulator_info *mri;
	struct regulator_config config = {};
	const struct of_device_id *match;
	const struct platform_device_id *id;
	struct mt6370_pmu_dsv_platform_data pdata, mask;
	int i, ret;

	mri = devm_kzalloc(&pdev->dev, sizeof(*mri), GFP_KERNEL);
	if (!mri)
		return -ENOMEM;

	if (pdev->dev.of_node) {
		match = of_match_device(
			      of_match_ptr(mt6370_regulator_of_id), &pdev->dev);
		if (!match) {
			dev_err(&pdev->dev, "no match device id\n");
			return -EINVAL;
		}
		devdata = (struct mt6370_regulator_devdata *)match->data;
	} else {
		id = platform_get_device_id(pdev);
		devdata = (struct mt6370_regulator_devdata *)id->driver_data;
	}
	mri->i2c = pmu_info->i2c[devdata->i2c_idx];
	mri->dev = &pdev->dev;
	mri->chip_rev = pmu_info->chip_rev;
	crc8_populate_msb(mri->crc8_table, 0x7);
	platform_set_drvdata(pdev, mri);

	/* regmap regiser */
	mri->regmap = devm_regmap_init(&(mri->i2c->dev),
				       NULL, mri, devdata->regmap_config);
	if (IS_ERR(mri->regmap)) {
		dev_err(&pdev->dev, "Fail to register regmap\n");
		return PTR_ERR(mri->regmap);
	}
	/* regulator register */
	config.dev = &pdev->dev;
	config.driver_data = mri;
	config.regmap = mri->regmap;
	mt_parse_dt(&pdev->dev, &pdata, &mask);
	for (i = 0; i < devdata->num_reg_descs; i++) {
		mri->rdev[i] = devm_regulator_register(&pdev->dev,
					&(devdata->reg_descs[i].desc), &config);
		if (IS_ERR(mri->rdev[i])) {
			dev_err(&pdev->dev,
				"fail to register  %d regulaotr\n", i);
			return PTR_ERR(mri->rdev[i]);
		}
	}
	ret = dsv_apply_dts(config.regmap, &pdata, &mask);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to apply dsv dts\n");
		return ret;
	}
	ret = mt6370_regulator_irq_register(pdev, devdata);
	if (ret < 0) {
		dev_err(&pdev->dev, "Fail to register irqs\n");
		return ret;
	}
	dev_err(&pdev->dev, "Successfully probed\n");
	return 0;
}
#if SHUTDOWN
static void mt6370_regulator_shutdown(struct platform_device *pdev)
{
	struct mt6370_regulator_info *mri = platform_get_drvdata(pdev);
	int i, ret;
	//u8 pwr_off_seq[MT6360_PMIC_MAX] = { 0x06, 0x04, 0x00, 0x02 }; //TODO

	dev_dbg(mri->dev, "%s\n", __func__);
	if (mri == NULL)
		return;
	if (mri->i2c->addr == MT6370_PMIC_SLAVEID) {
		for (i = 0; i < MT6370_PMIC_MAX; i++) {
			ret = regmap_write(mri->regmap,
					   MT6370_PMIC_BUCK1_SEQOFFDLY + i,
					   pwr_off_seq[i]);
			if (ret < 0)
				dev_notice(mri->dev,
					   "%s: set pwr off seq buck(%d) fail\n",
					   __func__, i);
		}
	}
}
#endif

static const struct platform_device_id mt6370_regulator_id[] = {
	{ "mt6370_pmic", (kernel_ulong_t)&mt6370_pmic_devdata },
	{ },
};
MODULE_DEVICE_TABLE(platform, mt6370_regulator_id);

static struct platform_driver mt6370_regulator_driver = {
	.driver = {
		.name = "mt6370_regulator",
		.of_match_table = of_match_ptr(mt6370_regulator_of_id),
	},
	.probe = mt6370_regulator_probe,
	/*.shutdown = mt6370_regulator_shutdown,*/
	.id_table = mt6370_regulator_id,
};
module_platform_driver(mt6370_regulator_driver);

MODULE_AUTHOR("CY_Huang <cy_huang@richtek.com>");
MODULE_DESCRIPTION("MT6370 Regulator Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.0.0");
