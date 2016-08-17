/*
 * drivers/regulator/ricoh583-regulator.c
 *
 * Regulator driver for RICOH583 power management chip.
 *
 * Copyright (C) 2011 NVIDIA Corporation
 *
 * Copyright (C) 2011 RICOH COMPANY,LTD
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */

/*#define DEBUG			1*/
/*#define VERBOSE_DEBUG		1*/
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/mfd/ricoh583.h>
#include <linux/regulator/ricoh583-regulator.h>

struct ricoh583_regulator {
	int		id;
	int		deepsleep_id;
	/* Regulator register address.*/
	u8		reg_en_reg;
	u8		en_bit;
	u8		reg_disc_reg;
	u8		disc_bit;
	u8		vout_reg;
	u8		vout_mask;
	u8		vout_reg_cache;
	u8		deepsleep_reg;

	/* chip constraints on regulator behavior */
	int			min_uV;
	int			max_uV;
	int			step_uV;
	int			nsteps;

	/* regulator specific turn-on delay */
	u16			delay;

	/* used by regulator core */
	struct regulator_desc	desc;

	/* Device */
	struct device		*dev;
};


static inline struct device *to_ricoh583_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent->parent;
}

static int ricoh583_regulator_enable_time(struct regulator_dev *rdev)
{
	struct ricoh583_regulator *ri = rdev_get_drvdata(rdev);

	return ri->delay;
}

static int ricoh583_reg_is_enabled(struct regulator_dev *rdev)
{
	struct ricoh583_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh583_dev(rdev);
	uint8_t control;
	int ret;

	ret = ricoh583_read(parent, ri->reg_en_reg, &control);
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in reading the control register\n");
		return ret;
	}
	return (((control >> ri->en_bit) & 1) == 1);
}

static int ricoh583_reg_enable(struct regulator_dev *rdev)
{
	struct ricoh583_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh583_dev(rdev);
	int ret;

	ret = ricoh583_set_bits(parent, ri->reg_en_reg, (1 << ri->en_bit));
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in updating the STATE register\n");
		return ret;
	}
	udelay(ri->delay);
	return ret;
}

static int ricoh583_reg_disable(struct regulator_dev *rdev)
{
	struct ricoh583_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh583_dev(rdev);
	int ret;

	ret = ricoh583_clr_bits(parent, ri->reg_en_reg, (1 << ri->en_bit));
	if (ret < 0)
		dev_err(&rdev->dev, "Error in updating the STATE register\n");

	return ret;
}

static int ricoh583_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct ricoh583_regulator *ri = rdev_get_drvdata(rdev);

	return ri->min_uV + (ri->step_uV * index);
}

static int __ricoh583_set_ds_voltage(struct device *parent,
		struct ricoh583_regulator *ri, int min_uV, int max_uV)
{
	int vsel;
	int ret;

	if ((min_uV < ri->min_uV) || (max_uV > ri->max_uV))
		return -EDOM;

	vsel = (min_uV - ri->min_uV + ri->step_uV - 1)/ri->step_uV;
	if (vsel > ri->nsteps)
		return -EDOM;

	ret = ricoh583_update(parent, ri->deepsleep_reg, vsel, ri->vout_mask);
	if (ret < 0)
		dev_err(ri->dev, "Error in writing the deepsleep register\n");
	return ret;
}

static int __ricoh583_set_voltage(struct device *parent,
		struct ricoh583_regulator *ri, int min_uV, int max_uV,
		unsigned *selector)
{
	int vsel;
	int ret;
	uint8_t vout_val;

	if ((min_uV < ri->min_uV) || (max_uV > ri->max_uV))
		return -EDOM;

	vsel = (min_uV - ri->min_uV + ri->step_uV - 1)/ri->step_uV;
	if (vsel > ri->nsteps)
		return -EDOM;

	if (selector)
		*selector = vsel;

	vout_val = (ri->vout_reg_cache & ~ri->vout_mask) |
				(vsel & ri->vout_mask);
	ret = ricoh583_write(parent, ri->vout_reg, vout_val);
	if (ret < 0)
		dev_err(ri->dev, "Error in writing the Voltage register\n");
	else
		ri->vout_reg_cache = vout_val;

	return ret;
}

static int ricoh583_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct ricoh583_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_ricoh583_dev(rdev);

	return __ricoh583_set_voltage(parent, ri, min_uV, max_uV, selector);
}

static int ricoh583_get_voltage(struct regulator_dev *rdev)
{
	struct ricoh583_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t vsel;

	vsel = ri->vout_reg_cache & ri->vout_mask;
	return ri->min_uV + vsel * ri->step_uV;
}

static struct regulator_ops ricoh583_ops = {
	.list_voltage	= ricoh583_list_voltage,
	.set_voltage	= ricoh583_set_voltage,
	.get_voltage	= ricoh583_get_voltage,
	.enable		= ricoh583_reg_enable,
	.disable	= ricoh583_reg_disable,
	.is_enabled	= ricoh583_reg_is_enabled,
	.enable_time	= ricoh583_regulator_enable_time,
};

#define RICOH583_REG(_id, _en_reg, _en_bit, _disc_reg, _disc_bit, _vout_reg, \
		_vout_mask, _ds_reg, _min_mv, _max_mv, _step_uV, _nsteps,    \
		_ops, _delay)		\
{								\
	.reg_en_reg	= _en_reg,				\
	.en_bit		= _en_bit,				\
	.reg_disc_reg	= _disc_reg,				\
	.disc_bit	= _disc_bit,				\
	.vout_reg	= _vout_reg,				\
	.vout_mask	= _vout_mask,				\
	.deepsleep_reg	= _ds_reg,				\
	.min_uV		= _min_mv * 1000,			\
	.max_uV		= _max_mv * 1000,			\
	.step_uV	= _step_uV,				\
	.nsteps		= _nsteps,				\
	.delay		= _delay,				\
	.id		= RICOH583_ID_##_id,			\
	.deepsleep_id	= RICOH583_DS_##_id,			\
	.desc = {						\
		.name = ricoh583_rails(_id),			\
		.id = RICOH583_ID_##_id,			\
		.n_voltages = _nsteps,				\
		.ops = &_ops,					\
		.type = REGULATOR_VOLTAGE,			\
		.owner = THIS_MODULE,				\
	},							\
}

static struct ricoh583_regulator ricoh583_regulator[] = {
	RICOH583_REG(DC0, 0x30, 0, 0x30, 1, 0x31, 0x7F, 0x60,
			700, 1500, 12500, 0x41, ricoh583_ops, 500),
	RICOH583_REG(DC1, 0x34, 0, 0x34, 1, 0x35, 0x7F, 0x61,
			700, 1500, 12500, 0x41, ricoh583_ops, 500),
	RICOH583_REG(DC2, 0x38, 0, 0x38, 1, 0x39, 0x7F, 0x62,
			900, 2400, 12500, 0x79, ricoh583_ops, 500),
	RICOH583_REG(DC3, 0x3C, 0, 0x3C, 1, 0x3D, 0x7F, 0x63,
			900, 2400, 12500, 0x79, ricoh583_ops, 500),
	RICOH583_REG(LDO0, 0x51, 0, 0x53, 0, 0x54, 0x7F, 0x64,
			900, 3400, 25000, 0x65, ricoh583_ops, 500),
	RICOH583_REG(LDO1, 0x51, 1, 0x53, 1, 0x55, 0x7F, 0x65,
			900, 3400, 25000, 0x65, ricoh583_ops, 500),
	RICOH583_REG(LDO2, 0x51, 2, 0x53, 2, 0x56, 0x7F, 0x66,
			900, 3400, 25000, 0x65, ricoh583_ops, 500),
	RICOH583_REG(LDO3, 0x51, 3, 0x53, 3, 0x57, 0x7F, 0x67,
			900, 3400, 25000, 0x65, ricoh583_ops, 500),
	RICOH583_REG(LDO4, 0x51, 4, 0x53, 4, 0x58, 0x3F, 0x68,
			750, 1500, 12500, 0x3D, ricoh583_ops, 500),
	RICOH583_REG(LDO5, 0x51, 5, 0x53, 5, 0x59, 0x7F, 0x69,
			900, 3400, 25000, 0x65, ricoh583_ops, 500),
	RICOH583_REG(LDO6, 0x51, 6, 0x53, 6, 0x5A, 0x7F, 0x6A,
			900, 3400, 25000, 0x65, ricoh583_ops, 500),
	RICOH583_REG(LDO7, 0x51, 7, 0x53, 7, 0x5B, 0x7F, 0x6B,
			900, 3400, 25000, 0x65, ricoh583_ops, 500),
	RICOH583_REG(LDO8, 0x50, 0, 0x52, 0, 0x5C, 0x7F, 0x6C,
			900, 3400, 25000, 0x65, ricoh583_ops, 500),
	RICOH583_REG(LDO9, 0x50, 1, 0x52, 1, 0x5D, 0x7F, 0x6D,
			900, 3400, 25000, 0x65, ricoh583_ops, 500),
};
static inline struct ricoh583_regulator *find_regulator_info(int id)
{
	struct ricoh583_regulator *ri;
	int i;

	for (i = 0; i < ARRAY_SIZE(ricoh583_regulator); i++) {
		ri = &ricoh583_regulator[i];
		if (ri->desc.id == id)
			return ri;
	}
	return NULL;
}

static int ricoh583_regulator_preinit(struct device *parent,
		struct ricoh583_regulator *ri,
		struct ricoh583_regulator_platform_data *ricoh583_pdata)
{
	int ret = 0;

	if (ri->deepsleep_id != RICOH583_DS_NONE) {
		ret = ricoh583_ext_power_req_config(parent, ri->deepsleep_id,
			ricoh583_pdata->ext_pwr_req,
			ricoh583_pdata->deepsleep_slots);
		if (ret < 0)
			return ret;
	}

	if (!ricoh583_pdata->init_apply)
		return 0;

	if (ricoh583_pdata->deepsleep_uV) {
		ret = __ricoh583_set_ds_voltage(parent, ri,
				ricoh583_pdata->deepsleep_uV,
				ricoh583_pdata->deepsleep_uV);
		if (ret < 0) {
			dev_err(ri->dev, "Not able to initialize ds voltage %d"
				" for rail %d err %d\n",
				ricoh583_pdata->deepsleep_uV, ri->desc.id, ret);
			return ret;
		}
	}

	if (ricoh583_pdata->init_uV >= 0) {
		ret = __ricoh583_set_voltage(parent, ri,
				ricoh583_pdata->init_uV,
				ricoh583_pdata->init_uV, 0);
		if (ret < 0) {
			dev_err(ri->dev, "Not able to initialize voltage %d "
				"for rail %d err %d\n", ricoh583_pdata->init_uV,
				ri->desc.id, ret);
			return ret;
		}
	}

	if (ricoh583_pdata->init_enable)
		ret = ricoh583_set_bits(parent, ri->reg_en_reg,
							(1 << ri->en_bit));
	else
		ret = ricoh583_clr_bits(parent, ri->reg_en_reg,
							(1 << ri->en_bit));
	if (ret < 0)
		dev_err(ri->dev, "Not able to %s rail %d err %d\n",
			(ricoh583_pdata->init_enable) ? "enable" : "disable",
			ri->desc.id, ret);

	return ret;
}

static inline int ricoh583_cache_regulator_register(struct device *parent,
	struct ricoh583_regulator *ri)
{
	ri->vout_reg_cache = 0;
	return ricoh583_read(parent, ri->vout_reg, &ri->vout_reg_cache);
}

static int __devinit ricoh583_regulator_probe(struct platform_device *pdev)
{
	struct ricoh583_regulator *ri = NULL;
	struct regulator_dev *rdev;
	struct ricoh583_regulator_platform_data *tps_pdata;
	int id = pdev->id;
	int err;

	dev_dbg(&pdev->dev, "Probing reulator %d\n", id);

	ri = find_regulator_info(id);
	if (ri == NULL) {
		dev_err(&pdev->dev, "invalid regulator ID specified\n");
		return -EINVAL;
	}
	tps_pdata = pdev->dev.platform_data;
	ri->dev = &pdev->dev;

	err = ricoh583_cache_regulator_register(pdev->dev.parent, ri);
	if (err) {
		dev_err(&pdev->dev, "Fail in caching register\n");
		return err;
	}

	err = ricoh583_regulator_preinit(pdev->dev.parent, ri, tps_pdata);
	if (err) {
		dev_err(&pdev->dev, "Fail in pre-initialisation\n");
		return err;
	}
	rdev = regulator_register(&ri->desc, &pdev->dev,
				&tps_pdata->regulator, ri, NULL);
	if (IS_ERR_OR_NULL(rdev)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
				ri->desc.name);
		return PTR_ERR(rdev);
	}

	platform_set_drvdata(pdev, rdev);
	return 0;
}

static int __devexit ricoh583_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	regulator_unregister(rdev);
	return 0;
}

static struct platform_driver ricoh583_regulator_driver = {
	.driver	= {
		.name	= "ricoh583-regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= ricoh583_regulator_probe,
	.remove		= __devexit_p(ricoh583_regulator_remove),
};

static int __init ricoh583_regulator_init(void)
{
	return platform_driver_register(&ricoh583_regulator_driver);
}
subsys_initcall(ricoh583_regulator_init);

static void __exit ricoh583_regulator_exit(void)
{
	platform_driver_unregister(&ricoh583_regulator_driver);
}
module_exit(ricoh583_regulator_exit);

MODULE_DESCRIPTION("RICOH583 regulator driver");
MODULE_ALIAS("platform:ricoh583-regulator");
MODULE_LICENSE("GPL");
