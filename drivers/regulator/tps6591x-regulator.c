/*
 * driver/regulator/tps6591x-regulator.c
 *
 * Regulator driver for TI TPS6591x PMIC family
 *
 * Copyright (C) 2011 NVIDIA Corporation
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

#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/slab.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/regulator/tps6591x-regulator.h>
#include <linux/mfd/tps6591x.h>
#include <linux/module.h>

/* supply control and voltage setting  */
#define TPS6591X_VIO_ADD		0x20
#define TPS6591X_VDD1_ADD		0x21
#define TPS6591X_VDD1_OP_ADD		0x22
#define TPS6591X_VDD1_SR_ADD		0x23
#define TPS6591X_VDD2_ADD		0x24
#define TPS6591X_VDD2_OP_ADD		0x25
#define TPS6591X_VDD2_SR_ADD		0x26
#define TPS6591X_VDDCTRL_ADD		0x27
#define TPS6591X_VDDCTRL_OP_ADD		0x28
#define TPS6591X_VDDCTRL_SR_ADD		0x29
#define TPS6591X_LDO1_ADD		0x30
#define TPS6591X_LDO2_ADD		0x31
#define TPS6591X_LDO3_ADD		0x37
#define TPS6591X_LDO4_ADD		0x36
#define TPS6591X_LDO5_ADD		0x32
#define TPS6591X_LDO6_ADD		0x35
#define TPS6591X_LDO7_ADD		0x34
#define TPS6591X_LDO8_ADD		0x33
#define TPS6591X_SLEEP_SET_LDO_OFF_ADD	0x43
#define TPS6591X_SLEEP_SET_RES_OFF_ADD	0x44
#define TPS6591X_EN1_LDO_ADD		0x45
#define TPS6591X_EN1_SMPS_ADD		0x46
#define TPS6591X_EN2_LDO_ADD		0x47
#define TPS6591X_EN2_SMPS_ADD		0x48
#define TPS6591X_DCDCCTRL_ADD		0x3E
#define TPS6591X_INVALID_ADD		0xFF

#define EN1_EN2_OFFSET			2
#define LDO4_TRACK_VAL			0x40

struct tps6591x_register_info {
	unsigned char addr;
	unsigned char nbits;
	unsigned char shift_bits;
	uint8_t cache_val;
};

enum {
	supply_type_none = 0x0,
	supply_type_single_reg,
	supply_type_sr_op_reg
};

struct tps6591x_regulator {
	struct regulator_desc desc;
	int supply_type;

	struct tps6591x_register_info supply_reg;
	struct tps6591x_register_info op_reg;
	struct tps6591x_register_info sr_reg;
	struct tps6591x_register_info en1_reg;
	struct tps6591x_register_info slp_off_reg;

	int *voltages;

	int enable_delay; /* delay in us for regulator to stabilize */
	enum tps6591x_ext_control ectrl;
	int current_volt_uv;

	/* Time (micro sec) taken for 1uV change */
	int voltage_change_uv_per_us;
	unsigned int config_flags;
};

static inline struct device *to_tps6591x_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent->parent;
}

static int tps6591x_regulator_enable_time(struct regulator_dev *rdev)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);

	return ri->enable_delay;
}

static int __tps6591x_ext_control_set(struct device *parent,
				      struct tps6591x_regulator *ri,
				      enum tps6591x_ext_control ectrl)
{
	int ret;
	uint8_t mask, reg_val, addr, offset;
	struct tps6591x_register_info *ext_reg;

	/* For regulator that has separate operational and sleep register make
	   sure that operational is used and clear sleep register to turn
	   regulator off when external control is inactive */
	if (ri->supply_type == supply_type_sr_op_reg) {
		reg_val = ri->op_reg.cache_val;
		if (reg_val & 0x80) {	/* boot has used sr - switch to op */
			reg_val = ri->sr_reg.cache_val;
			mask = ((1 << ri->sr_reg.nbits) - 1)
				<< ri->sr_reg.shift_bits;
			reg_val &= mask;
			ret = tps6591x_write(parent, ri->op_reg.addr, reg_val);
			if (ret)
				return ret;
			ri->op_reg.cache_val = reg_val;
		}
		ret = tps6591x_write(parent, ri->sr_reg.addr, 0);
		if (ret)
			return ret;
		ri->sr_reg.cache_val = 0;
	}

	offset = 0;
	switch (ectrl) {
	case EXT_CTRL_EN2:
		offset = EN1_EN2_OFFSET;
		/* fall through to EXT_CTRL_EN1 */
	case EXT_CTRL_EN1:
		ext_reg = &(ri->en1_reg);
		break;
	case EXT_CTRL_SLEEP_OFF:
		ext_reg = &(ri->slp_off_reg);
		break;
	default:
		return -EINVAL;
	}

	addr = ext_reg->addr + offset;
	mask = ((1 << ext_reg->nbits) - 1) << ext_reg->shift_bits;

	return tps6591x_update(parent, addr, mask, mask);
}

static void wait_for_voltage_change(struct tps6591x_regulator *ri, int uV)
{
	int change_uv;
	int change_us;

	change_uv = abs(uV - ri->current_volt_uv);
	change_us = change_uv/ri->voltage_change_uv_per_us + 1;
	if (change_us >= 1000) {
		mdelay(change_us/1000);
		change_us -= (change_us/1000);
	}
	if (change_us)
		udelay(change_us);
	ri->current_volt_uv = uV;
}

static int __tps6591x_vio_set_voltage(struct device *parent,
				      struct tps6591x_regulator *ri,
				      int min_uV, int max_uV,
				      unsigned *selector)
{
	int uV;
	uint8_t mask;
	uint8_t val;
	int ret;
	uint8_t reg_val;

	for (val = 0; val < ri->desc.n_voltages; val++) {
		uV = ri->voltages[val] * 1000;

		/* use the first in-range value */
		if (min_uV <= uV && uV <= max_uV) {
			if (selector)
				*selector = val;

			reg_val = ri->supply_reg.cache_val;
			val <<= ri->supply_reg.shift_bits;

			mask = ((1 << ri->supply_reg.nbits) - 1) <<
					ri->supply_reg.shift_bits;
			reg_val = (reg_val & ~mask) | (val & mask);

			ret = tps6591x_write(parent, ri->supply_reg.addr,
					reg_val);
			if (ret >= 0) {
				wait_for_voltage_change(ri, uV);
				ri->supply_reg.cache_val = reg_val;
			}
			return ret;
		}
	}

	return -EINVAL;
}

static int tps6591x_vio_set_voltage(struct regulator_dev *rdev,
				    int min_uV, int max_uV,
				    unsigned *selector)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps6591x_dev(rdev);

	return __tps6591x_vio_set_voltage(parent, ri, min_uV, max_uV,
					  selector);
}

static int tps6591x_vio_get_voltage(struct regulator_dev *rdev)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t val, mask;

	val = ri->supply_reg.cache_val;

	mask = ((1 << ri->supply_reg.nbits) - 1) << ri->supply_reg.shift_bits;
	val = (val & mask) >> ri->supply_reg.shift_bits;

	if (val >= ri->desc.n_voltages)
		BUG();

	return ri->voltages[val] * 1000;
}


static int tps6591x_ldo_list_voltage(struct regulator_dev *rdev,
				     unsigned selector)
{
	struct tps6591x_regulator *info = rdev_get_drvdata(rdev);

	return info->voltages[selector] * 1000;
}

static int __tps6591x_ldo1_set_voltage(struct device *parent,
				       struct tps6591x_regulator *ri,
				       int min_uV, int max_uV,
				       unsigned *selector)
{
	int val, uV;
	uint8_t mask;
	uint8_t reg_val;
	int ret;

	for (val = 0; val < ri->desc.n_voltages; val++) {
		uV = ri->voltages[val] * 1000;

		/* use the first in-range value */
		if (min_uV <= uV && uV <= max_uV) {
			if (selector)
				*selector = val;
			reg_val = ri->supply_reg.cache_val;
			val += 4;
			val <<= ri->supply_reg.shift_bits;
			mask = ((1 << ri->supply_reg.nbits) - 1) <<
					ri->supply_reg.shift_bits;

			reg_val = (reg_val & ~mask) | (val & mask);
			ret = tps6591x_write(parent, ri->supply_reg.addr,
					reg_val);
			if (ret >= 0) {
				wait_for_voltage_change(ri, uV);
				ri->supply_reg.cache_val = reg_val;
			}
			return ret;
		}
	}

	return -EINVAL;
}

static int tps6591x_ldo1_set_voltage(struct regulator_dev *rdev,
				     int min_uV, int max_uV,
				     unsigned *selector)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps6591x_dev(rdev);

	return __tps6591x_ldo1_set_voltage(parent, ri, min_uV, max_uV,
					   selector);
}

static int tps6591x_ldo1_get_voltage(struct regulator_dev *rdev)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t val, mask;

	val = ri->supply_reg.cache_val;
	mask = ((1 << ri->supply_reg.nbits) - 1) << ri->supply_reg.shift_bits;
	val = (val & mask) >> ri->supply_reg.shift_bits;

	if (val < 4)
		return 1000 * 1000;
	else if (val > 0x32)
		return 3300 * 1000;
	else
		val -= 4;
	if (val >= ri->desc.n_voltages)
		BUG();

	return ri->voltages[val] * 1000;
}

static int __tps6591x_ldo3_set_voltage(struct device *parent,
				       struct tps6591x_regulator *ri,
				       int min_uV, int max_uV,
				       unsigned *selector)
{
	int val, uV;
	uint8_t mask;
	int ret;
	uint8_t reg_val;

	for (val = 0; val < ri->desc.n_voltages; val++) {
		uV = ri->voltages[val] * 1000;

		/* use the first in-range value */
		if (min_uV <= uV && uV <= max_uV) {
			if (selector)
				*selector = val;
			reg_val = ri->supply_reg.cache_val;
			val += 2;
			val <<= ri->supply_reg.shift_bits;
			mask = ((1 << ri->supply_reg.nbits) - 1) <<
						ri->supply_reg.shift_bits;

			reg_val = (reg_val & ~mask) | (val & mask);

			ret = tps6591x_write(parent, ri->supply_reg.addr,
					reg_val);
			if (ret >= 0) {
				wait_for_voltage_change(ri, uV);
				ri->supply_reg.cache_val = reg_val;
			}
			return ret;
		}
	}

	return -EINVAL;
}

static int tps6591x_ldo3_set_voltage(struct regulator_dev *rdev,
				     int min_uV, int max_uV,
				     unsigned *selector)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps6591x_dev(rdev);

	return __tps6591x_ldo3_set_voltage(parent, ri, min_uV, max_uV,
					   selector);
}

static int tps6591x_ldo3_get_voltage(struct regulator_dev *rdev)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t val, mask;

	val = ri->supply_reg.cache_val;
	mask = ((1 << ri->supply_reg.nbits) - 1) << ri->supply_reg.shift_bits;
	val = (val & mask) >> ri->supply_reg.shift_bits;

	if (val < 2)
		return 1000 * 1000;
	else if (val > 0x19)
		return 3300 * 1000;
	else
		val -= 2;
	if (val >= ri->desc.n_voltages)
		BUG();

	return ri->voltages[val] * 1000;
}

static int __tps6591x_vdd_set_voltage(struct device *parent,
				      struct tps6591x_regulator *ri,
				      int min_uV, int max_uV,
				      unsigned *selector)
{
	int val, uV, ret;
	uint8_t mask;
	uint8_t op_reg_val;
	uint8_t sr_reg_val;

	for (val = 0; val < ri->desc.n_voltages; val++) {
		uV = ri->voltages[val] * 1000;

		/* use the first in-range value */
		if (min_uV <= uV && uV <= max_uV) {
			if (selector)
				*selector = val;
			op_reg_val = ri->op_reg.cache_val;
			val += 3;
			if (op_reg_val & 0x80) {
				sr_reg_val = ri->sr_reg.cache_val;
				val <<= ri->sr_reg.shift_bits;
				mask = ((1 << ri->sr_reg.nbits) - 1)
					<< ri->sr_reg.shift_bits;
				sr_reg_val = (sr_reg_val & ~mask) |
							(val & mask);
				ret = tps6591x_write(parent,
					ri->sr_reg.addr, sr_reg_val);
				if (!ret)
					ri->sr_reg.cache_val = sr_reg_val;
			} else {
				val <<= ri->op_reg.shift_bits;
				mask = ((1 << ri->op_reg.nbits) - 1)
					<< ri->op_reg.shift_bits;
				op_reg_val = (op_reg_val & ~mask) |
							(val & mask);
				ret = tps6591x_write(parent,
					ri->op_reg.addr, op_reg_val);
				if (!ret)
					ri->op_reg.cache_val = op_reg_val;
			}
			if (ret >= 0)
				wait_for_voltage_change(ri, uV);
			return ret;
		}
	}

	return -EINVAL;
}

static int tps6591x_vdd_set_voltage(struct regulator_dev *rdev,
				    int min_uV, int max_uV,
				    unsigned *selector)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps6591x_dev(rdev);

	return __tps6591x_vdd_set_voltage(parent, ri, min_uV, max_uV,
					  selector);
}

static int tps6591x_vdd_get_voltage(struct regulator_dev *rdev)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t op_val, sr_val, val;

	op_val = ri->op_reg.cache_val;
	sr_val = ri->sr_reg.cache_val;

	val = (op_val & 0x80) ? (sr_val & 0x7F) : (op_val & 0x7F);

	if (!val)
		return 0;
	else if (val < 0x3)
		return 600 * 1000;
	else if (val > 0x4B)
		return 1500 * 1000;
	else
		val -= 3;

	if (val >= ri->desc.n_voltages)
		BUG();

	return ri->voltages[val] * 1000;
}

static int tps6591x_regulator_enable(struct regulator_dev *rdev)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps6591x_dev(rdev);
	uint8_t reg_val;
	int ret;

	reg_val = ri->supply_reg.cache_val;
	reg_val |= 0x1;

	ret = tps6591x_write(parent, ri->supply_reg.addr, reg_val);
	if (!ret)
		ri->supply_reg.cache_val = reg_val;
	return ret;
}

static int tps6591x_regulator_disable(struct regulator_dev *rdev)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps6591x_dev(rdev);
	uint8_t reg_val;
	int ret;

	reg_val = ri->supply_reg.cache_val;
	reg_val &= ~0x1;
	ret = tps6591x_write(parent, ri->supply_reg.addr, reg_val);
	if (!ret)
		ri->supply_reg.cache_val = reg_val;
	return ret;
}

static int tps6591x_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t reg_val;

	reg_val = ri->supply_reg.cache_val;
	reg_val &= 0x1;
	return reg_val & 0x1;
}

static struct regulator_ops tps6591x_regulator_vio_ops = {
	.list_voltage = tps6591x_ldo_list_voltage,
	.get_voltage = tps6591x_vio_get_voltage,
	.set_voltage = tps6591x_vio_set_voltage,

	.enable_time = tps6591x_regulator_enable_time,
	.is_enabled = tps6591x_regulator_is_enabled,
	.enable = tps6591x_regulator_enable,
	.disable = tps6591x_regulator_disable,
};

static struct regulator_ops tps6591x_regulator_ldo1_ops = {
	.list_voltage = tps6591x_ldo_list_voltage,
	.get_voltage = tps6591x_ldo1_get_voltage,
	.set_voltage = tps6591x_ldo1_set_voltage,

	.enable_time = tps6591x_regulator_enable_time,
	.is_enabled = tps6591x_regulator_is_enabled,
	.enable = tps6591x_regulator_enable,
	.disable = tps6591x_regulator_disable,
};

static struct regulator_ops tps6591x_regulator_ldo3_ops = {
	.list_voltage = tps6591x_ldo_list_voltage,
	.get_voltage = tps6591x_ldo3_get_voltage,
	.set_voltage = tps6591x_ldo3_set_voltage,

	.enable_time = tps6591x_regulator_enable_time,
	.is_enabled = tps6591x_regulator_is_enabled,
	.enable = tps6591x_regulator_enable,
	.disable = tps6591x_regulator_disable,
};

static struct regulator_ops tps6591x_regulator_vdd_ops = {
	.list_voltage = tps6591x_ldo_list_voltage,
	.get_voltage = tps6591x_vdd_get_voltage,
	.set_voltage = tps6591x_vdd_set_voltage,

	.enable_time = tps6591x_regulator_enable_time,
	.is_enabled = tps6591x_regulator_is_enabled,
	.enable = tps6591x_regulator_enable,
	.disable = tps6591x_regulator_disable,
};

static int tps6591x_vio_voltages[] = {
	1500, 1800, 2500, 3300,
};

/* SEL[7:2]=000100:1000mV --> 110010:3300mV */
static int tps6591x_ldo124_voltages[] = {
	1000, 1050, 1100, 1150, 1200, 1250, 1300, 1350, 1400, 1450,
	1500, 1550, 1600, 1650, 1700, 1750, 1800, 1850, 1900, 1950,
	2000, 2050, 2100, 2150, 2200, 2250, 2300, 2350, 2400, 2450,
	2500, 2550, 2600, 2650, 2700, 2750, 2800, 2850, 2900, 2950,
	3000, 3050, 3100, 3150, 3200, 3250, 3300,
};

/* SEL[6:2]=00010:1000mv --> 11001:3300mV */
static int tps6591x_ldo35678_voltages[] = {
	1000, 1100, 1200, 1300, 1400, 1500, 1600, 1700, 1800, 1900,
	2000, 2100, 2200, 2300, 2400, 2500, 2600, 2700, 2800, 2900,
	3000, 3100, 3200, 3300,
};

static int tps6591x_vdd_voltages[] = {
	600, 612, 625, 637, 650, 662, 675, 687, 700, 712, 725, 737,
	750, 762, 775, 787, 800, 812, 825, 837, 850, 862, 875, 887,
	900, 912, 925, 937, 950, 962, 975, 987, 1000, 1012, 1025,
	1037, 1050, 1062, 1075, 1087, 1100, 1112, 1125, 1137, 1150,
	1162, 1175, 1187, 1200, 1212, 1225, 1237, 1250, 1262, 1275,
	1287, 1300, 1312, 1325, 1337, 1350, 1362, 1375, 1387, 1400,
	1412, 1425, 1437, 1450, 1462, 1475, 1487, 1500,
};

static int tps6591x_vddctrl_voltages[] = {
	600, 612, 625, 637, 650, 662, 675, 687, 700, 712, 725, 737,
	750, 762, 775, 787, 800, 812, 825, 837, 850, 862, 875, 887,
	900, 912, 925, 937, 950, 962, 975, 987, 1000, 1012, 1025,
	1037, 1050, 1062, 1075, 1087, 1100, 1112, 1125, 1137, 1150,
	1162, 1175, 1187, 1200, 1212, 1225, 1237, 1250, 1262, 1275,
	1287, 1300, 1312, 1325, 1337, 1350, 1362, 1375, 1387, 1400,
};

#define TPS6591X_REGULATOR(_id, vdata, _ops, s_addr, s_nbits, s_shift,	\
			s_type, op_addr, op_nbits, op_shift, sr_addr,	\
			sr_nbits, sr_shift, en1_addr, en1_shift,	\
			slp_off_addr, slp_off_shift, en_time,		\
			change_rate)					\
	.desc	= {							\
		.name	= tps6591x_rails(_id),				\
		.ops	= &tps6591x_regulator_##_ops,			\
		.type	= REGULATOR_VOLTAGE,				\
		.id	= TPS6591X_ID_##_id,				\
		.n_voltages = ARRAY_SIZE(tps6591x_##vdata##_voltages),	\
		.owner	= THIS_MODULE,					\
	},								\
	.supply_type	= supply_type_##s_type,				\
	.supply_reg	= {						\
		.addr	= TPS6591X_##s_addr##_ADD,			\
		.nbits	= s_nbits,					\
		.shift_bits = s_shift,					\
	},								\
	.op_reg		= {						\
		.addr	= TPS6591X_##op_addr##_ADD,			\
		.nbits	= op_nbits,					\
		.shift_bits = op_shift,					\
	},								\
	.sr_reg		= {						\
		.addr	= TPS6591X_##sr_addr##_ADD,			\
		.nbits	= sr_nbits,					\
		.shift_bits = sr_shift,					\
	},								\
	.en1_reg	= {						\
		.addr	= TPS6591X_##en1_addr##_ADD,			\
		.nbits	= 1,						\
		.shift_bits = en1_shift,				\
	},								\
	.slp_off_reg	= {						\
		.addr	= TPS6591X_SLEEP_SET_##slp_off_addr##_ADD,	\
		.nbits	= 1,						\
		.shift_bits = slp_off_shift,				\
	},								\
	.voltages	= tps6591x_##vdata##_voltages,			\
	.enable_delay		= en_time,				\
	.voltage_change_uv_per_us = change_rate,

#define TPS6591X_VIO(_id, vdata, s_addr, s_nbits, s_shift, s_type,	\
			en1_shift, slp_off_shift, en_time)		\
{									\
	TPS6591X_REGULATOR(_id, vdata, vio_ops, s_addr, s_nbits,	\
			s_shift, s_type, INVALID, 0, 0,	INVALID, 0, 0,	\
			EN1_SMPS, en1_shift, RES_OFF, slp_off_shift,	\
			en_time, 10000)					\
}

#define TPS6591X_LDO1(_id, vdata, s_addr, s_nbits, s_shift, s_type,	\
			en1_shift, slp_off_shift, en_time)		\
{									\
	TPS6591X_REGULATOR(_id, vdata, ldo1_ops, s_addr, s_nbits,	\
			s_shift, s_type, INVALID, 0, 0, INVALID, 0, 0,	\
			EN1_LDO, en1_shift, LDO_OFF, slp_off_shift,	\
			en_time, 6000)					\
}

#define TPS6591X_LDO3(_id, vdata, s_addr, s_nbits, s_shift, s_type,	\
			en1_shift, slp_off_shift, en_time)		\
{									\
	TPS6591X_REGULATOR(_id, vdata, ldo3_ops, s_addr, s_nbits,	\
			s_shift, s_type, INVALID, 0, 0, INVALID, 0, 0,	\
			EN1_LDO, en1_shift, LDO_OFF, slp_off_shift,	\
			en_time, 11000)					\
}

#define TPS6591X_VDD(_id, vdata, s_addr, s_nbits, s_shift, s_type,	\
			op_addr, op_nbits, op_shift, sr_addr, sr_nbits,	\
			sr_shift, en1_shift, slp_off_shift, en_time)	\
{									\
	TPS6591X_REGULATOR(_id, vdata, vdd_ops, s_addr, s_nbits,	\
			s_shift, s_type, op_addr, op_nbits, op_shift,	\
			sr_addr, sr_nbits, sr_shift, EN1_SMPS,		\
			en1_shift, RES_OFF, slp_off_shift, en_time,	\
			5000)						\
}

static struct tps6591x_regulator tps6591x_regulator[] = {
	TPS6591X_VIO(VIO, vio, VIO, 2, 2, single_reg, 0, 0, 350),
	TPS6591X_LDO1(LDO_1, ldo124, LDO1, 6, 2, single_reg, 1, 1, 420),
	TPS6591X_LDO1(LDO_2, ldo124, LDO2, 6, 2, single_reg, 2, 2, 420),
	TPS6591X_LDO3(LDO_3, ldo35678, LDO3, 5, 2, single_reg, 7, 7, 230),
	TPS6591X_LDO1(LDO_4, ldo124, LDO4, 6, 2, single_reg, 6, 6, 230),
	TPS6591X_LDO3(LDO_5, ldo35678, LDO5, 5, 2, single_reg, 3, 3, 230),
	TPS6591X_LDO3(LDO_6, ldo35678, LDO6, 5, 2, single_reg, 0, 0, 230),
	TPS6591X_LDO3(LDO_7, ldo35678, LDO7, 5, 2, single_reg, 5, 5, 230),
	TPS6591X_LDO3(LDO_8, ldo35678, LDO8, 5, 2, single_reg, 4, 4, 230),
	TPS6591X_VDD(VDD_1, vdd, VDD1, 2, 0, sr_op_reg, VDD1_OP,
		7, 0, VDD1_SR, 7, 0, 1, 1, 350),
	TPS6591X_VDD(VDD_2, vdd, VDD2, 2, 0, sr_op_reg, VDD2_OP,
		7, 0, VDD2_SR, 7, 0, 2, 2, 350),
	TPS6591X_VDD(VDDCTRL, vddctrl, VDDCTRL, 2, 0, sr_op_reg,
		VDDCTRL_OP, 7, 0, VDDCTRL_SR, 7, 0, 3, 3, 900),
};

static inline int tps6591x_regulator_preinit(struct device *parent,
		struct tps6591x_regulator *ri,
		struct tps6591x_regulator_platform_data *tps6591x_pdata)
{
	int ret;
	uint8_t reg_val;

	if (tps6591x_pdata->ectrl != EXT_CTRL_NONE) {
		ret = __tps6591x_ext_control_set(
			parent, ri, tps6591x_pdata->ectrl);
		if (ret < 0) {
			pr_err("Not able to configure external control %d"
			       " for rail %d err %d\n", tps6591x_pdata->ectrl,
			       ri->desc.id, ret);
			return ret;
		}
	}

	if (!tps6591x_pdata->init_apply)
		return 0;

	if (ri->desc.id == TPS6591X_ID_LDO_4 &&
		(tps6591x_pdata->flags & LDO4_TRACKING_EN)) {
		ret = tps6591x_read(parent, TPS6591X_DCDCCTRL_ADD,
						&reg_val);
		if (ret < 0)
			pr_err("Not able to configure tracking mode for"
			" %d err %d\n", ri->desc.id, ret);
		else
			tps6591x_write(parent, TPS6591X_DCDCCTRL_ADD,
					reg_val|LDO4_TRACK_VAL);
	}

	if (tps6591x_pdata->init_uV >= 0) {
		switch (ri->desc.id) {
		case TPS6591X_ID_VIO:
			ret = __tps6591x_vio_set_voltage(parent, ri,
					tps6591x_pdata->init_uV,
					tps6591x_pdata->init_uV, 0);
			break;

		case TPS6591X_ID_LDO_1:
		case TPS6591X_ID_LDO_2:
		case TPS6591X_ID_LDO_4:
			ret = __tps6591x_ldo1_set_voltage(parent, ri,
					tps6591x_pdata->init_uV,
					tps6591x_pdata->init_uV, 0);
			break;

		case TPS6591X_ID_LDO_3:
		case TPS6591X_ID_LDO_5:
		case TPS6591X_ID_LDO_6:
		case TPS6591X_ID_LDO_7:
		case TPS6591X_ID_LDO_8:
			ret = __tps6591x_ldo3_set_voltage(parent, ri,
					tps6591x_pdata->init_uV,
					tps6591x_pdata->init_uV, 0);
			break;

		case TPS6591X_ID_VDD_1:
		case TPS6591X_ID_VDD_2:
		case TPS6591X_ID_VDDCTRL:
			ret = __tps6591x_vdd_set_voltage(parent, ri,
					tps6591x_pdata->init_uV,
					tps6591x_pdata->init_uV, 0);
			break;

		default:
			ret = -EINVAL;
			break;
		}
		if (ret < 0) {
			pr_err("Not able to initialize voltage %d for rail "
				"%d err %d\n", tps6591x_pdata->init_uV,
				ri->desc.id, ret);
			return ret;
		}
	}

	reg_val = ri->supply_reg.cache_val;
	if (tps6591x_pdata->init_enable)
		reg_val |= 0x1;
	else
		reg_val &= ~0x1;
	ret = tps6591x_write(parent, ri->supply_reg.addr, reg_val);

	if (ret < 0)
		pr_err("Not able to %s rail %d err %d\n",
			(tps6591x_pdata->init_enable) ? "enable" : "disable",
			ri->desc.id, ret);
	else
		ri->supply_reg.cache_val = reg_val;
	return ret;
}

static inline int tps6591x_cache_regulator_register(struct device *parent,
		struct tps6591x_regulator *ri)
{
	int ret;
	ret = tps6591x_read(parent, ri->supply_reg.addr,
				&ri->supply_reg.cache_val);
	if (!ret && (ri->supply_type == supply_type_sr_op_reg)) {
		ret = tps6591x_read(parent, ri->op_reg.addr,
				&ri->op_reg.cache_val);
		if (!ret)
			ret = tps6591x_read(parent, ri->sr_reg.addr,
				&ri->sr_reg.cache_val);
	}
	return ret;
}

static inline struct tps6591x_regulator *find_regulator_info(int id)
{
	struct tps6591x_regulator *ri;
	int i;

	for (i = 0; i < ARRAY_SIZE(tps6591x_regulator); i++) {
		ri = &tps6591x_regulator[i];
		if (ri->desc.id == id)
			return ri;
	}
	return NULL;
}


static int __devinit tps6591x_regulator_probe(struct platform_device *pdev)
{
	struct tps6591x_regulator *ri = NULL;
	struct regulator_dev *rdev;
	struct tps6591x_regulator_platform_data *tps_pdata;
	int id = pdev->id;
	int err;

	dev_dbg(&pdev->dev, "Probing regulator %d\n", id);

	ri = find_regulator_info(id);
	if (ri == NULL) {
		dev_err(&pdev->dev, "invalid regulator ID specified\n");
		return -EINVAL;
	}
	tps_pdata = pdev->dev.platform_data;
	ri->ectrl = tps_pdata->ectrl;
	ri->config_flags = tps_pdata->flags;

	if (tps_pdata->slew_rate_uV_per_us)
		ri->voltage_change_uv_per_us = tps_pdata->slew_rate_uV_per_us;

	err = tps6591x_cache_regulator_register(pdev->dev.parent, ri);
	if (err) {
		dev_err(&pdev->dev, "Error in caching registers error %d\n",
							err);
		return err;
	}

	err = tps6591x_regulator_preinit(pdev->dev.parent, ri, tps_pdata);
	if (err) {
		dev_err(&pdev->dev, "Error in pre-initialization of regulator "
					"error %d\n", err);
		return err;
	}

	rdev = regulator_register(&ri->desc, &pdev->dev,
				&tps_pdata->regulator, ri, NULL);
	if (IS_ERR_OR_NULL(rdev)) {
		dev_err(&pdev->dev, "failed to register regulator %s\n",
				ri->desc.name);
		return PTR_ERR(rdev);
	}
	ri->current_volt_uv = ri->desc.ops->get_voltage(rdev);

	platform_set_drvdata(pdev, rdev);

	return 0;
}

static int __devexit tps6591x_regulator_remove(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);

	regulator_unregister(rdev);
	return 0;
}

static void tps6591x_regulator_shutdown(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps6591x_dev(rdev);
	int ret;

	if (ri->ectrl == EXT_CTRL_EN1) {
		ret = tps6591x_clr_bits(parent, ri->en1_reg.addr,
				(1 << ri->en1_reg.shift_bits));
		if (ret < 0)
			dev_err(&pdev->dev, "Error in clearing external control\n");
	}
}

static int tps6591x_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps6591x_dev(rdev);
	int ret = 0;
	uint8_t reg_val;

	if (ri->config_flags & LDO_LOW_POWER_ON_SUSPEND) {
		ret = tps6591x_clr_bits(parent, ri->en1_reg.addr,
				(1 << ri->en1_reg.shift_bits));
		reg_val = ri->supply_reg.cache_val;
		reg_val = (reg_val & ~0x3) | (0x3);
		ret = tps6591x_write(parent, ri->supply_reg.addr, reg_val);
		if (ret >= 0)
			ri->supply_reg.cache_val = reg_val;
		else
			dev_err(&pdev->dev, "Error in updating the supply state\n");
	}
	return ret;
}

static int tps6591x_resume(struct platform_device *pdev)
{
	struct regulator_dev *rdev = platform_get_drvdata(pdev);
	struct tps6591x_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps6591x_dev(rdev);
	int ret = 0;
	uint8_t reg_val;

	if (ri->config_flags & LDO_LOW_POWER_ON_SUSPEND) {
		ret = tps6591x_clr_bits(parent, ri->en1_reg.addr,
				(1 << ri->en1_reg.shift_bits));
		reg_val = ri->supply_reg.cache_val;
		reg_val = (reg_val & ~0x3) | (0x1);
		ret = tps6591x_write(parent, ri->supply_reg.addr, reg_val);
		if (ret >= 0)
			ri->supply_reg.cache_val = reg_val;
		else
			dev_err(&pdev->dev, "Error in updating the supply state\n");
	}
	return ret;
}

static struct platform_driver tps6591x_regulator_driver = {
	.driver	= {
		.name	= "tps6591x-regulator",
		.owner	= THIS_MODULE,
	},
	.probe		= tps6591x_regulator_probe,
	.remove		= __devexit_p(tps6591x_regulator_remove),
	.shutdown	= tps6591x_regulator_shutdown,
	.suspend	= tps6591x_suspend,
	.resume		= tps6591x_resume,
};

static int __init tps6591x_regulator_init(void)
{
	return platform_driver_register(&tps6591x_regulator_driver);
}
subsys_initcall(tps6591x_regulator_init);

static void __exit tps6591x_regulator_exit(void)
{
	platform_driver_unregister(&tps6591x_regulator_driver);
}
module_exit(tps6591x_regulator_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Regulator Driver for TI TPS6591X PMIC");
MODULE_ALIAS("platform:tps6591x-regulator");
