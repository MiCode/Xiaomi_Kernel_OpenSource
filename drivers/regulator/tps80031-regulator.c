/*
 * driver/regulator/tps80031-regulator.c
 *
 * Regulator driver for TI TPS80031
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
#include <linux/regulator/tps80031-regulator.h>
#include <linux/mfd/tps80031.h>
#include <linux/module.h>

/* Flags for DCDC Voltage reading */
#define DCDC_OFFSET_EN		BIT(0)
#define DCDC_EXTENDED_EN	BIT(1)
#define TRACK_MODE_ENABLE	BIT(2)

#define SMPS_MULTOFFSET_VIO	BIT(1)
#define SMPS_MULTOFFSET_SMPS1	BIT(3)
#define SMPS_MULTOFFSET_SMPS2	BIT(4)
#define SMPS_MULTOFFSET_SMPS3	BIT(6)
#define SMPS_MULTOFFSET_SMPS4	BIT(0)

#define PMC_SMPS_OFFSET_ADD	0xE0
#define PMC_SMPS_MULT_ADD	0xE3

#define STATE_OFF		0x00
#define STATE_ON		0x01
#define STATE_MASK		0x03

#define TRANS_SLEEP_OFF		0x00
#define TRANS_SLEEP_ON		0x04
#define TRANS_SLEEP_MASK	0x0C

#define SMPS_CMD_MASK		0xC0
#define SMPS_VSEL_MASK		0x3F
#define LDO_VSEL_MASK		0x1F

#define TPS80031_MISC2_ADD	0xE5
#define MISC2_LDOUSB_IN_VSYS	0x10
#define MISC2_LDOUSB_IN_PMID	0x08
#define MISC2_LDOUSB_IN_MASK	0x18

#define MISC2_LDO3_SEL_VIB_VAL	BIT(0)
#define MISC2_LDO3_SEL_VIB_MASK	0x1

#define CHARGERUSB_CTRL3_ADD	0xEA
#define BOOST_HW_PWR_EN		BIT(5)
#define BOOST_HW_PWR_EN_MASK	BIT(5)

#define CHARGERUSB_CTRL1_ADD	0xE8
#define OPA_MODE_EN		BIT(6)
#define OPA_MODE_EN_MASK	BIT(6)

#define USB_VBUS_CTRL_SET	0x04
#define USB_VBUS_CTRL_CLR	0x05
#define VBUS_DISCHRG		0x20

#define EXT_PWR_REQ (PWR_REQ_INPUT_PREQ1 | PWR_REQ_INPUT_PREQ2 | \
			PWR_REQ_INPUT_PREQ3)

struct tps80031_regulator_info {
	/* Regulator register address.*/
	u8		trans_reg;
	u8		state_reg;
	u8		force_reg;
	u8		volt_reg;
	u8		volt_id;

	/* chip constraints on regulator behavior */
	u16			min_mV;
	u16			max_mV;

	/* regulator specific turn-on delay as per datasheet*/
	int			delay;

	/* used by regulator core */
	struct regulator_desc	desc;

	/*Power request bits */
	int preq_bit;
};

struct tps80031_regulator {
	struct device			*dev;
	struct regulator_dev		*rdev;
	struct tps80031_regulator_info	*rinfo;
	unsigned int			tolerance_uv;

	/* Regulator specific turn-on delay if board file provided */
	int				delay;

	u8				flags;
	unsigned int			platform_flags;
	unsigned int			ext_ctrl_flag;

	/* Cached register */
	uint8_t		trans_reg_cache;
	uint8_t		state_reg_cache;
	uint8_t		force_reg_cache;
	uint8_t		volt_reg_cache;
};

static inline struct device *to_tps80031_dev(struct regulator_dev *rdev)
{
	return rdev_get_dev(rdev)->parent->parent;
}

static int tps80031_regulator_enable_time(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);

	return ri->delay;
}

static u8 tps80031_get_smps_offset(struct device *parent)
{
	u8 value;
	int ret;

	ret = tps80031_read(parent, SLAVE_ID1, PMC_SMPS_OFFSET_ADD, &value);
	if (ret < 0) {
		dev_err(parent, "Error in reading smps offset register\n");
		return 0;
	}
	return value;
}

static u8 tps80031_get_smps_mult(struct device *parent)
{
	u8 value;
	int ret;

	ret = tps80031_read(parent, SLAVE_ID1, PMC_SMPS_MULT_ADD, &value);
	if (ret < 0) {
		dev_err(parent, "Error in reading smps mult register\n");
		return 0;
	}
	return value;
}

static int tps80031_reg_is_enabled(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);

	if (ri->ext_ctrl_flag & EXT_PWR_REQ)
		return true;
	return ((ri->state_reg_cache & STATE_MASK) == STATE_ON);
}

static int tps80031_reg_enable(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps80031_dev(rdev);
	int ret;
	uint8_t reg_val;

	if (ri->ext_ctrl_flag & EXT_PWR_REQ)
		return 0;

	reg_val = (ri->state_reg_cache & ~STATE_MASK) |
					(STATE_ON & STATE_MASK);
	ret = tps80031_write(parent, SLAVE_ID1, ri->rinfo->state_reg, reg_val);
	if (ret < 0) {
		dev_err(&rdev->dev, "Error in writing the STATE register\n");
		return ret;
	}
	ri->state_reg_cache = reg_val;
	udelay(ri->delay);
	return ret;
}

static int tps80031_reg_disable(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps80031_dev(rdev);
	int ret;
	uint8_t reg_val;

	if (ri->ext_ctrl_flag & EXT_PWR_REQ)
		return 0;

	reg_val = (ri->state_reg_cache & ~STATE_MASK) |
					(STATE_OFF & STATE_MASK);
	ret = tps80031_write(parent, SLAVE_ID1, ri->rinfo->state_reg, reg_val);
	if (ret < 0)
		dev_err(&rdev->dev, "Error in writing the STATE register\n");
	else
		ri->state_reg_cache = reg_val;
	return ret;
}

/*
 * DCDC status and control
 */
static int tps80031dcdc_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	int voltage = 0;

	switch (ri->flags) {
	case 0:
		if (index == 0)
			voltage = 0;
		else if (index < 58)
			voltage = (607700 + (12660 * (index - 1)));
		else if (index == 58)
			voltage = 1350 * 1000;
		else if (index == 59)
			voltage = 1500 * 1000;
		else if (index == 60)
			voltage = 1800 * 1000;
		else if (index == 61)
			voltage = 1900 * 1000;
		else if (index == 62)
			voltage = 2100 * 1000;
		break;

	case DCDC_OFFSET_EN:
		if (index == 0)
			voltage = 0;
		else if (index < 58)
			voltage = (700000 + (12500 * (index - 1)));
		else if (index == 58)
			voltage = 1350 * 1000;
		else if (index == 59)
			voltage = 1500 * 1000;
		else if (index == 60)
			voltage = 1800 * 1000;
		else if (index == 61)
			voltage = 1900 * 1000;
		else if (index == 62)
			voltage = 2100 * 1000;
		break;

	case DCDC_EXTENDED_EN:
		if (index == 0)
			voltage = 0;
		else if (index < 58)
			voltage = (1852000 + (38600 * (index - 1)));
		else if (index == 58)
			voltage = 2084 * 1000;
		else if (index == 59)
			voltage = 2315 * 1000;
		else if (index == 60)
			voltage = 2778 * 1000;
		else if (index == 61)
			voltage = 2932 * 1000;
		else if (index == 62)
			voltage = 3241 * 1000;
		break;

	case DCDC_OFFSET_EN|DCDC_EXTENDED_EN:
		if (index == 0)
			voltage = 0;
		else if (index < 58)
			voltage = (2161000 + (38600 * (index - 1)));
		else if (index == 58)
			voltage = 4167 * 1000;
		else if (index == 59)
			voltage = 2315 * 1000;
		else if (index == 60)
			voltage = 2778 * 1000;
		else if (index == 61)
			voltage = 2932 * 1000;
		else if (index == 62)
			voltage = 3241 * 1000;
		break;
	}

	return voltage;
}

static int __tps80031_dcdc_set_voltage(struct device *parent,
				       struct tps80031_regulator *ri,
				       int min_uV, int max_uV,
				       unsigned *selector)
{
	int vsel = 0;
	int ret;

	switch (ri->flags) {
	case 0:
		if (min_uV >= (607700 + ri->tolerance_uv))
			min_uV = min_uV - ri->tolerance_uv;

		if (min_uV == 0)
			vsel = 0;
		else if ((min_uV >= 607700) && (min_uV <= 1300000)) {
			int cal_volt;
			vsel = (10 * (min_uV - 607700)) / 1266;
			if (vsel % 100)
				vsel += 100;
			vsel /= 100;
			vsel++;
			cal_volt = (607700 + (12660 * (vsel - 1)));
			if (cal_volt > max_uV)
				return -EINVAL;
		} else if ((min_uV > 1900000) && (max_uV >= 2100000))
			vsel = 62;
		else if ((min_uV > 1800000) && (max_uV >= 1900000))
			vsel = 61;
		else if ((min_uV > 1500000) && (max_uV >= 1800000))
			vsel = 60;
		else if ((min_uV > 1350000) && (max_uV >= 1500000))
			vsel = 59;
		else if ((min_uV > 1300000) && (max_uV >= 1350000))
			vsel = 58;
		else
			return -EINVAL;
		break;

	case DCDC_OFFSET_EN:
		if (min_uV >= (700000 + ri->tolerance_uv))
			min_uV = min_uV - ri->tolerance_uv;
		if (min_uV == 0)
			vsel = 0;
		else if ((min_uV >= 700000) && (min_uV <= 1420000)) {
			int cal_volt;
			vsel = (min_uV - 700000) / 125;
			if (vsel % 100)
				vsel += 100;
			vsel /= 100;
			vsel++;
			cal_volt = (700000 + (12500 * (vsel - 1)));
			if (cal_volt > max_uV)
				return -EINVAL;
		} else if ((min_uV > 1900000) && (max_uV >= 2100000))
			vsel = 62;
		else if ((min_uV > 1800000) && (max_uV >= 1900000))
			vsel = 61;
		else if ((min_uV > 1500000) && (max_uV >= 1800000))
			vsel = 60;
		else if ((min_uV > 1350000) && (max_uV >= 1500000))
			vsel = 59;
		else if ((min_uV > 1300000) && (max_uV >= 1350000))
			vsel = 58;
		else
			return -EINVAL;
		break;

	case DCDC_EXTENDED_EN:
		if (min_uV >= (1852000 + ri->tolerance_uv))
			min_uV = min_uV - ri->tolerance_uv;
		if (min_uV == 0)
			vsel = 0;
		else if ((min_uV >= 1852000) && (max_uV <= 4013600)) {
			vsel = (min_uV - 1852000) / 386;
			if (vsel % 100)
				vsel += 100;
			vsel++;
		}
		break;

	case DCDC_OFFSET_EN|DCDC_EXTENDED_EN:
		if (min_uV >= (2161000 + ri->tolerance_uv))
			min_uV = min_uV - ri->tolerance_uv;
		if (min_uV == 0)
			vsel = 0;
		else if ((min_uV >= 2161000) && (max_uV <= 4321000)) {
			vsel = (min_uV - 2161000) / 386;
			if (vsel % 100)
				vsel += 100;
			vsel /= 100;
			vsel++;
		}
		break;
	}

	if (selector)
		*selector = vsel;

	if (ri->rinfo->force_reg) {
		if (((ri->force_reg_cache >> 6) & 0x3) == 0) {
			ret = tps80031_write(parent, ri->rinfo->volt_id,
						ri->rinfo->force_reg, vsel);
			if (ret < 0)
				dev_err(ri->dev, "Error in writing the "
						"force register\n");
			else
				ri->force_reg_cache = vsel;
			return ret;
		}
	}
	ret = tps80031_write(parent, ri->rinfo->volt_id,
					ri->rinfo->volt_reg, vsel);
	if (ret < 0)
		dev_err(ri->dev, "Error in writing the Voltage register\n");
	else
		ri->volt_reg_cache = vsel;
	return ret;
}

static int tps80031dcdc_set_voltage(struct regulator_dev *rdev,
			int min_uV, int max_uV, unsigned *selector)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps80031_dev(rdev);
	return __tps80031_dcdc_set_voltage(parent, ri, min_uV, max_uV,
					   selector);
}

static int tps80031dcdc_get_voltage(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t vsel = 0;
	int voltage = 0;

	if (ri->rinfo->force_reg) {
		vsel  = ri->force_reg_cache;
		if ((vsel & SMPS_CMD_MASK) == 0)
			goto decode;
	}

	vsel =  ri->volt_reg_cache;

decode:
	vsel &= SMPS_VSEL_MASK;

	switch (ri->flags) {
	case 0:
		if (vsel == 0)
			voltage = 0;
		else if (vsel < 58)
			voltage = (607700 + (12660 * (vsel - 1)));
		else if (vsel == 58)
			voltage = 1350 * 1000;
		else if (vsel == 59)
			voltage = 1500 * 1000;
		else if (vsel == 60)
			voltage = 1800 * 1000;
		else if (vsel == 61)
			voltage = 1900 * 1000;
		else if (vsel == 62)
			voltage = 2100 * 1000;
		break;

	case DCDC_OFFSET_EN:
		if (vsel == 0)
			voltage = 0;
		else if (vsel < 58)
			voltage = (700000 + (12500 * (vsel - 1)));
		else if (vsel == 58)
			voltage = 1350 * 1000;
		else if (vsel == 59)
			voltage = 1500 * 1000;
		else if (vsel == 60)
			voltage = 1800 * 1000;
		else if (vsel == 61)
			voltage = 1900 * 1000;
		else if (vsel == 62)
			voltage = 2100 * 1000;
		break;

	case DCDC_EXTENDED_EN:
		if (vsel == 0)
			voltage = 0;
		else if (vsel < 58)
			voltage = (1852000 + (38600 * (vsel - 1)));
		else if (vsel == 58)
			voltage = 2084 * 1000;
		else if (vsel == 59)
			voltage = 2315 * 1000;
		else if (vsel == 60)
			voltage = 2778 * 1000;
		else if (vsel == 61)
			voltage = 2932 * 1000;
		else if (vsel == 62)
			voltage = 3241 * 1000;
		break;

	case DCDC_EXTENDED_EN|DCDC_OFFSET_EN:
		if (vsel == 0)
			voltage = 0;
		else if (vsel < 58)
			voltage = (2161000 + (38600 * (vsel - 1)));
		else if (vsel == 58)
			voltage = 4167 * 1000;
		else if (vsel == 59)
			voltage = 2315 * 1000;
		else if (vsel == 60)
			voltage = 2778 * 1000;
		else if (vsel == 61)
			voltage = 2932 * 1000;
		else if (vsel == 62)
			voltage = 3241 * 1000;
		break;
	}

	return voltage;
}

static int tps80031ldo_list_voltage(struct regulator_dev *rdev, unsigned index)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);

	if (index == 0)
		return 0;

	if ((ri->rinfo->desc.id == TPS80031_REGULATOR_LDO2) &&
			(ri->flags &  TRACK_MODE_ENABLE))
		return (ri->rinfo->min_mV + (((index - 1) * 125))/10) * 1000;

	return (ri->rinfo->min_mV + ((index - 1) * 100)) * 1000;
}

static int __tps80031_ldo2_set_voltage_track_mode(struct device *parent,
		struct tps80031_regulator *ri, int min_uV, int max_uV)
{
	int vsel = 0;
	int ret;
	int nvsel;

	if (min_uV < 600000) {
		vsel = 0;
	} else if ((min_uV >= 600000) && (max_uV <= 1300000)) {
		vsel = (min_uV - 600000) / 125;
		if (vsel % 100)
			vsel += 100;
		vsel /= 100;
		vsel++;
	} else {
		return -EINVAL;
	}

	/* Check for valid setting for TPS80031 or TPS80032-ES1.0 */
	if ((tps80031_get_chip_info(parent) == TPS80031) ||
		((tps80031_get_chip_info(parent) == TPS80032) &&
		(tps80031_get_pmu_version(parent) == 0x0))) {
		nvsel = vsel & 0x1F;
		if ((nvsel == 0x0) || (nvsel >= 0x19 && nvsel <= 0x1F)) {
			dev_err(ri->dev, "Invalid value for track mode LDO2 "
				"configuration for TPS8003x PMU\n");
			return -EINVAL;
		}
	}

	ret = tps80031_write(parent, ri->rinfo->volt_id,
				ri->rinfo->volt_reg, vsel);
	if (ret < 0)
		dev_err(ri->dev, "Error in writing the Voltage register\n");
	else
		ri->volt_reg_cache = vsel;
	return ret;
}


static int __tps80031_ldo_set_voltage(struct device *parent,
				      struct tps80031_regulator *ri,
				      int min_uV, int max_uV,
				      unsigned *selector)
{
	int vsel;
	int ret;

	if ((min_uV/1000 < ri->rinfo->min_mV) ||
			(max_uV/1000 > ri->rinfo->max_mV))
		return -EDOM;

	if ((ri->rinfo->desc.id == TPS80031_REGULATOR_LDO2) &&
			(ri->flags &  TRACK_MODE_ENABLE))
		return __tps80031_ldo2_set_voltage_track_mode(parent, ri,
				min_uV, max_uV);

	/*
	 * Use the below formula to calculate vsel
	 * mV = 1000mv + 100mv * (vsel - 1)
	 */
	vsel = (min_uV/1000 - 1000)/100 + 1;
	if (selector)
		*selector = vsel;
	ret = tps80031_write(parent, ri->rinfo->volt_id,
				ri->rinfo->volt_reg, vsel);
	if (ret < 0)
		dev_err(ri->dev, "Error in writing the Voltage register\n");
	else
		ri->volt_reg_cache = vsel;
	return ret;
}

static int tps80031ldo_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps80031_dev(rdev);

	return __tps80031_ldo_set_voltage(parent, ri, min_uV, max_uV,
					  selector);
}

static int tps80031ldo_get_voltage(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	uint8_t vsel;


	if ((ri->rinfo->desc.id == TPS80031_REGULATOR_LDO2) &&
			(ri->flags &  TRACK_MODE_ENABLE)) {
		vsel = ri->volt_reg_cache & 0x3F;
		return (ri->rinfo->min_mV + (((vsel - 1) * 125))/10) * 1000;
	}

	vsel = ri->volt_reg_cache & LDO_VSEL_MASK;
	/*
	 * Use the below formula to calculate vsel
	 * mV = 1000mv + 100mv * (vsel - 1)
	 */
	return (1000 + (100 * (vsel - 1))) * 1000;
}

/* VBUS */
static int tps80031_vbus_enable_time(struct regulator_dev *rdev)
{
	/* Enable and settling time for vbus is 3ms */
	return 3000;
}
static int tps80031_vbus_is_enabled(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps80031_dev(rdev);
	uint8_t ctrl1, ctrl3;
	int ret;

	if (ri->platform_flags & VBUS_SW_ONLY) {
		ret = tps80031_read(parent, SLAVE_ID2,
				CHARGERUSB_CTRL1_ADD, &ctrl1);
		if (!ret)
			ret = tps80031_read(parent, SLAVE_ID2,
					CHARGERUSB_CTRL3_ADD, &ctrl3);
		if (ret < 0) {
			dev_err(&rdev->dev, "Error in reading control reg\n");
			return ret;
		}
		if ((ctrl1 & OPA_MODE_EN) && (ctrl3 & BOOST_HW_PWR_EN))
			return 1;
		return 0;
	} else {
		return -EIO;
	}
}

static int tps80031_vbus_enable(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps80031_dev(rdev);
	int ret;

	if (ri->platform_flags & VBUS_SW_ONLY) {
		ret = tps80031_set_bits(parent, SLAVE_ID2,
				CHARGERUSB_CTRL1_ADD,  OPA_MODE_EN);
		if (!ret)
			ret = tps80031_set_bits(parent, SLAVE_ID2,
					CHARGERUSB_CTRL3_ADD, BOOST_HW_PWR_EN);
		if (ret < 0) {
			dev_err(&rdev->dev, "Error in reading control reg\n");
			return ret;
		}
		udelay(ri->delay);
		return ret;
	}
	dev_err(&rdev->dev, "%s() is not supported with flag 0x%08x\n",
		 __func__, ri->platform_flags);
	return -EIO;
}

static int tps80031_vbus_disable(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	struct device *parent = to_tps80031_dev(rdev);
	int ret = 0;

	if (ri->platform_flags & VBUS_SW_ONLY) {

		if (ri->platform_flags & VBUS_DISCHRG_EN_PDN)
			ret = tps80031_write(parent, SLAVE_ID2,
				USB_VBUS_CTRL_SET, VBUS_DISCHRG);
		if (!ret)
			ret = tps80031_clr_bits(parent, SLAVE_ID2,
				CHARGERUSB_CTRL1_ADD,  OPA_MODE_EN);
		if (!ret)
			ret = tps80031_clr_bits(parent, SLAVE_ID2,
					CHARGERUSB_CTRL3_ADD, BOOST_HW_PWR_EN);
		if (!ret)
			mdelay((ri->delay + 999)/1000);

		if (ri->platform_flags & VBUS_DISCHRG_EN_PDN)
			tps80031_write(parent, SLAVE_ID2,
				USB_VBUS_CTRL_CLR, VBUS_DISCHRG);

		if (ret < 0)
			dev_err(&rdev->dev, "Error in reading control reg\n");
		return ret;
	}
	dev_err(&rdev->dev, "%s() is not supported with flag 0x%08x\n",
		 __func__, ri->platform_flags);
	return -EIO;
}

static int tps80031vbus_get_voltage(struct regulator_dev *rdev)
{
	int ret;
	ret = tps80031_vbus_is_enabled(rdev);
	if (ret > 0)
		return 5000000;
	return ret;
}

static int tps80031_extreg_enable_time(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	return ri->delay;
}

static int tps80031_extreg_get_voltage(struct regulator_dev *rdev)
{
	struct tps80031_regulator *ri = rdev_get_drvdata(rdev);
	int ret;
	ret = tps80031_reg_is_enabled(rdev);
	if (ret > 0)
		return ri->rinfo->max_mV * 1000;
	return 0;
}

static struct regulator_ops tps80031dcdc_ops = {
	.list_voltage	= tps80031dcdc_list_voltage,
	.set_voltage	= tps80031dcdc_set_voltage,
	.get_voltage	= tps80031dcdc_get_voltage,
	.enable		= tps80031_reg_enable,
	.disable	= tps80031_reg_disable,
	.is_enabled	= tps80031_reg_is_enabled,
	.enable_time	= tps80031_regulator_enable_time,
};

static struct regulator_ops tps80031ldo_ops = {
	.list_voltage	= tps80031ldo_list_voltage,
	.set_voltage	= tps80031ldo_set_voltage,
	.get_voltage	= tps80031ldo_get_voltage,
	.enable		= tps80031_reg_enable,
	.disable	= tps80031_reg_disable,
	.is_enabled	= tps80031_reg_is_enabled,
	.enable_time	= tps80031_regulator_enable_time,
};

static struct regulator_ops tps80031vbus_ops = {
	.get_voltage	= tps80031vbus_get_voltage,
	.enable		= tps80031_vbus_enable,
	.disable	= tps80031_vbus_disable,
	.is_enabled	= tps80031_vbus_is_enabled,
	.enable_time	= tps80031_vbus_enable_time,
};

static struct regulator_ops tps80031_ext_reg_ops = {
	.enable		= tps80031_reg_enable,
	.disable	= tps80031_reg_disable,
	.is_enabled	= tps80031_reg_is_enabled,
	.enable_time	= tps80031_extreg_enable_time,
	.get_voltage	= tps80031_extreg_get_voltage,
};



#define TPS80031_REG(_id, _trans_reg, _state_reg, _force_reg, _volt_reg, \
		_volt_id, min_mVolts, max_mVolts, _ops, _n_volt, _delay, \
		_preq_bit)						 \
{								\
	.trans_reg = _trans_reg,				\
	.state_reg = _state_reg,				\
	.force_reg = _force_reg,				\
	.volt_reg = _volt_reg,					\
	.volt_id = _volt_id,					\
	.min_mV = min_mVolts,					\
	.max_mV = max_mVolts,					\
	.desc = {						\
		.name = tps80031_rails(_id),			\
		.id = TPS80031_REGULATOR_##_id,			\
		.n_voltages = _n_volt,				\
		.ops = &_ops,					\
		.type = REGULATOR_VOLTAGE,			\
		.owner = THIS_MODULE,				\
	},							\
	.delay = _delay,					\
	.preq_bit = _preq_bit,					\
}

static struct tps80031_regulator_info tps80031_regulator_info[] = {
	TPS80031_REG(VIO,   0x47, 0x48, 0x49, 0x4A, SLAVE_ID0, 600, 2100,
				tps80031dcdc_ops, 63, 500, 4),
	TPS80031_REG(SMPS1, 0x53, 0x54, 0x55, 0x56, SLAVE_ID0, 600, 2100,
				tps80031dcdc_ops, 63, 500, 0),
	TPS80031_REG(SMPS2, 0x59, 0x5A, 0x5B, 0x5C, SLAVE_ID0, 600, 2100,
				tps80031dcdc_ops, 63, 500, 1),
	TPS80031_REG(SMPS3, 0x65, 0x66, 0x00, 0x68, SLAVE_ID1, 600, 2100,
				tps80031dcdc_ops, 63, 500, 2),
	TPS80031_REG(SMPS4, 0x41, 0x42, 0x00, 0x44, SLAVE_ID1, 600, 2100,
				tps80031dcdc_ops, 63, 500, 3),

	TPS80031_REG(LDO1,   0x9D, 0x9E, 0x00, 0x9F, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, 8),
	TPS80031_REG(LDO2,   0x85, 0x86, 0x00, 0x87, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, 9),
	TPS80031_REG(LDO3,   0x8D, 0x8E, 0x00, 0x8F, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, 10),
	TPS80031_REG(LDO4,   0x89, 0x8A, 0x00, 0x8B, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, 11),
	TPS80031_REG(LDO5,   0x99, 0x9A, 0x00, 0x9B, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, 12),
	TPS80031_REG(LDO6,   0x91, 0x92, 0x00, 0x93, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, 13),
	TPS80031_REG(LDO7,   0xA5, 0xA6, 0x00, 0xA7, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, 14),
	TPS80031_REG(LDOUSB, 0xA1, 0xA2, 0x00, 0xA3, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, 5),
	TPS80031_REG(LDOLN,  0x95, 0x96, 0x00, 0x97, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, 15),
	TPS80031_REG(VANA,   0x81, 0x82, 0x00, 0x83, SLAVE_ID1, 1000, 3300,
				tps80031ldo_ops, 25, 500, -1),
	TPS80031_REG(VBUS,   0x0,  0x0,  0x00, 0x0,  SLAVE_ID1, 0,    5000,
				tps80031vbus_ops, 2, 200000, -1),
	TPS80031_REG(REGEN1, 0xAE,  0xAF,  0x00, 0x0,  SLAVE_ID1, 0,    3300,
				tps80031_ext_reg_ops, 2, 500, 16),
	TPS80031_REG(REGEN2, 0xB1,  0xB2,  0x00, 0x0,  SLAVE_ID1, 0,    3300,
				tps80031_ext_reg_ops, 2, 500, 17),
	TPS80031_REG(SYSEN,  0xB4,  0xB5,  0x00, 0x0,  SLAVE_ID1, 0,    3300,
				tps80031_ext_reg_ops, 2, 500, 18),
};

static int tps80031_power_req_config(struct device *parent,
		struct tps80031_regulator *ri,
		struct tps80031_regulator_platform_data *tps80031_pdata)
{
	int ret = 0;
	uint8_t reg_val;

	if (ri->rinfo->preq_bit < 0)
		goto skip_pwr_req_config;

	ret = tps80031_ext_power_req_config(parent, ri->ext_ctrl_flag,
			ri->rinfo->preq_bit, ri->rinfo->state_reg,
			ri->rinfo->trans_reg);
	if (!ret)
		ret = tps80031_read(parent, SLAVE_ID1, ri->rinfo->trans_reg,
			&ri->trans_reg_cache);

	if (!ret && ri->rinfo->state_reg)
		ret = tps80031_read(parent, SLAVE_ID1, ri->rinfo->state_reg,
			&ri->state_reg_cache);
	if (ret < 0) {
		dev_err(ri->dev, "%s() fails\n", __func__);
		return ret;
	}

skip_pwr_req_config:
	if (tps80031_pdata->ext_ctrl_flag &
			(PWR_OFF_ON_SLEEP | PWR_ON_ON_SLEEP)) {
		reg_val = (ri->trans_reg_cache & ~0xC);
		if (tps80031_pdata->ext_ctrl_flag & PWR_ON_ON_SLEEP)
			reg_val |= 0x4;

		ret = tps80031_write(parent, SLAVE_ID1, ri->rinfo->trans_reg,
			reg_val);
		if (ret < 0)
			dev_err(ri->dev, "Not able to write reg 0x%02x\n",
				ri->rinfo->trans_reg);
		else
			ri->trans_reg_cache = reg_val;
	}
	return ret;
}

static int tps80031_regulator_preinit(struct device *parent,
		struct tps80031_regulator *ri,
		struct tps80031_regulator_platform_data *tps80031_pdata)
{
	int ret = 0;
	uint8_t reg_val;

	if (ri->rinfo->desc.id == TPS80031_REGULATOR_LDOUSB) {
		if (ri->platform_flags & USBLDO_INPUT_VSYS)
			ret = tps80031_update(parent, SLAVE_ID1,
				TPS80031_MISC2_ADD,
				MISC2_LDOUSB_IN_VSYS, MISC2_LDOUSB_IN_MASK);
		if (ri->platform_flags & USBLDO_INPUT_PMID)
			ret = tps80031_update(parent, SLAVE_ID1,
				TPS80031_MISC2_ADD,
				MISC2_LDOUSB_IN_PMID, MISC2_LDOUSB_IN_MASK);
		if (ret < 0) {
			dev_err(ri->dev, "Not able to configure the rail "
				"LDOUSB as per platform data error %d\n", ret);
			return ret;
		}
	}

	if (ri->rinfo->desc.id == TPS80031_REGULATOR_LDO3) {
		if (ri->platform_flags & LDO3_OUTPUT_VIB)
			ret = tps80031_update(parent, SLAVE_ID1,
				TPS80031_MISC2_ADD,
				MISC2_LDO3_SEL_VIB_VAL,
				MISC2_LDO3_SEL_VIB_MASK);
		if (ret < 0) {
			dev_err(ri->dev, "Not able to configure the rail "
				"LDO3 as per platform data error %d\n", ret);
			return ret;
		}
	}

	switch (ri->rinfo->desc.id) {
	case TPS80031_REGULATOR_REGEN1:
	case TPS80031_REGULATOR_REGEN2:
	case TPS80031_REGULATOR_SYSEN:
		if (tps80031_pdata->reg_init_data->constraints.always_on ||
			tps80031_pdata->reg_init_data->constraints.boot_on)
			ret = tps80031_update(parent, SLAVE_ID1,
				ri->rinfo->state_reg, STATE_ON, STATE_MASK);
		else
			ret = tps80031_update(parent, SLAVE_ID1,
				ri->rinfo->state_reg, STATE_OFF, STATE_MASK);
		if (ret < 0) {
			dev_err(ri->dev,
				"state reg update failed, e %d\n", ret);
			return ret;
		}
		ret = tps80031_update(parent, SLAVE_ID1,
					ri->rinfo->trans_reg, 1, 0x3);
		if (ret < 0) {
			dev_err(ri->dev,
				"trans reg update failed, e %d\n", ret);
			return ret;
		}
		break;
	default:
		break;
	}

	if (!tps80031_pdata->init_apply)
		return 0;

	if (tps80031_pdata->init_uV >= 0) {
		switch (ri->rinfo->desc.id) {
		case TPS80031_REGULATOR_VIO:
		case TPS80031_REGULATOR_SMPS1:
		case TPS80031_REGULATOR_SMPS2:
		case TPS80031_REGULATOR_SMPS3:
		case TPS80031_REGULATOR_SMPS4:
			ret = __tps80031_dcdc_set_voltage(parent, ri,
					tps80031_pdata->init_uV,
					tps80031_pdata->init_uV, 0);
			break;

		case TPS80031_REGULATOR_LDO1:
		case TPS80031_REGULATOR_LDO2:
		case TPS80031_REGULATOR_LDO3:
		case TPS80031_REGULATOR_LDO4:
		case TPS80031_REGULATOR_LDO5:
		case TPS80031_REGULATOR_LDO6:
		case TPS80031_REGULATOR_LDO7:
		case TPS80031_REGULATOR_LDOUSB:
		case TPS80031_REGULATOR_LDOLN:
		case TPS80031_REGULATOR_VANA:
			ret = __tps80031_ldo_set_voltage(parent, ri,
					tps80031_pdata->init_uV,
					tps80031_pdata->init_uV, 0);
			break;
		default:
			ret = -EINVAL;
			break;
		}

		if (ret < 0) {
			dev_err(ri->dev, "Not able to initialize voltage %d "
				"for rail %d err %d\n", tps80031_pdata->init_uV,
				ri->rinfo->desc.id, ret);
			return ret;
		}
	}

	if (tps80031_pdata->init_enable)
		reg_val = (ri->state_reg_cache & ~STATE_MASK) |
				(STATE_ON & STATE_MASK);
	else
		reg_val = (ri->state_reg_cache & ~STATE_MASK) |
				(STATE_OFF & STATE_MASK);

	ret = tps80031_write(parent, SLAVE_ID1, ri->rinfo->state_reg, reg_val);
	if (ret < 0)
		dev_err(ri->dev, "Not able to %s rail %d err %d\n",
			(tps80031_pdata->init_enable) ? "enable" : "disable",
			ri->rinfo->desc.id, ret);
	else
		ri->state_reg_cache = reg_val;
	return ret;
}

static inline struct tps80031_regulator_info *find_regulator_info(int id)
{
	struct tps80031_regulator_info *rinfo;
	int i;

	for (i = 0; i < ARRAY_SIZE(tps80031_regulator_info); i++) {
		rinfo = &tps80031_regulator_info[i];
		if (rinfo->desc.id == id)
			return rinfo;
	}
	return NULL;
}
static void check_smps_mode_mult(struct device *parent,
	struct tps80031_regulator *ri)
{
	int mult_offset;
	switch (ri->rinfo->desc.id) {
	case TPS80031_REGULATOR_VIO:
		mult_offset = SMPS_MULTOFFSET_VIO;
		break;
	case TPS80031_REGULATOR_SMPS1:
		mult_offset = SMPS_MULTOFFSET_SMPS1;
		break;
	case TPS80031_REGULATOR_SMPS2:
		mult_offset = SMPS_MULTOFFSET_SMPS2;
		break;
	case TPS80031_REGULATOR_SMPS3:
		mult_offset = SMPS_MULTOFFSET_SMPS3;
		break;
	case TPS80031_REGULATOR_SMPS4:
		mult_offset = SMPS_MULTOFFSET_SMPS4;
		break;
	case TPS80031_REGULATOR_LDO2:
		ri->flags = (tps80031_get_smps_mult(parent) & (1 << 5)) ?
						TRACK_MODE_ENABLE : 0;
		/* TRACK mode the ldo2 varies from 600mV to 1300mV */
		if (ri->flags & TRACK_MODE_ENABLE) {
			ri->rinfo->min_mV = 600;
			ri->rinfo->max_mV = 1300;
			ri->rinfo->desc.n_voltages = 57;
		}
		return;
	default:
		return;
	}

	ri->flags = (tps80031_get_smps_offset(parent) & mult_offset) ?
						DCDC_OFFSET_EN : 0;
	ri->flags |= (tps80031_get_smps_mult(parent) & mult_offset) ?
						DCDC_EXTENDED_EN : 0;
	return;
}

static inline int tps80031_cache_regulator_register(struct device *parent,
	struct tps80031_regulator *ri)
{
	int ret;

	ret = tps80031_read(parent, SLAVE_ID1, ri->rinfo->trans_reg,
			&ri->trans_reg_cache);
	if (!ret && ri->rinfo->state_reg)
		ret = tps80031_read(parent, SLAVE_ID1, ri->rinfo->state_reg,
			&ri->state_reg_cache);
	if (!ret && ri->rinfo->force_reg)
		ret = tps80031_read(parent, ri->rinfo->volt_id,
				ri->rinfo->force_reg, &ri->force_reg_cache);
	if (!ret && ri->rinfo->volt_reg)
		ret = tps80031_read(parent, ri->rinfo->volt_id,
				ri->rinfo->volt_reg, &ri->volt_reg_cache);
	return ret;
}

static int __devinit tps80031_regulator_probe(struct platform_device *pdev)
{
	struct tps80031_platform_data *pdata = dev_get_platdata(pdev->dev.parent);
	struct tps80031_regulator_platform_data *tps_pdata;
	struct tps80031_regulator_info *rinfo;
	struct tps80031_regulator *ri;
	struct tps80031_regulator *pmic;
	struct regulator_dev *rdev;
	int id;
	int ret;
	int num;

	if (!pdata || !pdata->num_regulator_pdata) {
		dev_err(&pdev->dev, "Number of regulator is 0\n");
		return -EINVAL;
	}

	pmic = devm_kzalloc(&pdev->dev,
			pdata->num_regulator_pdata * sizeof(*pmic), GFP_KERNEL);
	if (!pmic) {
		dev_err(&pdev->dev, "mem alloc for pmic failed\n");
		return -ENOMEM;
	}

	for (num = 0; num < pdata->num_regulator_pdata; ++num) {
		tps_pdata = pdata->regulator_pdata[num];
		if (!tps_pdata->reg_init_data) {
			dev_err(&pdev->dev,
				"No regulator init data for index %d\n", num);
			ret = -EINVAL;
			goto fail;
		}

		id = tps_pdata->regulator_id;
		rinfo = find_regulator_info(id);
		if (!rinfo) {
			dev_err(&pdev->dev, "invalid regulator ID specified\n");
			ret = -EINVAL;
			goto fail;
		}

		ri = &pmic[num];
		ri->rinfo = rinfo;
		ri->dev = &pdev->dev;
		if (tps_pdata->delay_us)
			ri->delay = tps_pdata->delay_us;
		else
			ri->delay = rinfo->delay;
		ri->tolerance_uv = tps_pdata->tolerance_uv;

		check_smps_mode_mult(pdev->dev.parent, ri);
		ri->platform_flags = tps_pdata->flags;
		ri->ext_ctrl_flag = tps_pdata->ext_ctrl_flag;

		ret = tps80031_cache_regulator_register(pdev->dev.parent, ri);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"Register cache failed, err %d\n", ret);
			goto fail;
		}
		ret = tps80031_regulator_preinit(pdev->dev.parent, ri, tps_pdata);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"regulator preinit failed, err %d\n", ret);
			goto fail;
		}

		ret = tps80031_power_req_config(pdev->dev.parent, ri, tps_pdata);
		if (ret < 0) {
			dev_err(&pdev->dev,
				"power req config failed, err %d\n", ret);
			goto fail;
		}

		rdev = regulator_register(&ri->rinfo->desc, &pdev->dev,
				tps_pdata->reg_init_data, ri, NULL);
		if (IS_ERR_OR_NULL(rdev)) {
			dev_err(&pdev->dev,
				"register regulator failed %s\n",
					ri->rinfo->desc.name);
			ret = PTR_ERR(rdev);
			goto fail;
		}
		ri->rdev = rdev;
	}

	platform_set_drvdata(pdev, pmic);
	return 0;
fail:
	while(--num >= 0) {
		ri = &pmic[num];
		regulator_unregister(ri->rdev);
	}
	return ret;
}

static int __devexit tps80031_regulator_remove(struct platform_device *pdev)
{
	struct tps80031_platform_data *pdata = pdev->dev.parent->platform_data;
	struct tps80031_regulator *pmic = platform_get_drvdata(pdev);
	struct tps80031_regulator *ri = NULL;
	int num;

	if (!pdata || !pdata->num_regulator_pdata)
		return 0;

	for (num = 0; num < pdata->num_regulator_pdata; ++num) {
		ri = &pmic[num];
		regulator_unregister(ri->rdev);
	}
	return 0;
}

static struct platform_driver tps80031_regulator_driver = {
	.driver	= {
		.name	= "tps80031-regulators",
		.owner	= THIS_MODULE,
	},
	.probe		= tps80031_regulator_probe,
	.remove		= __devexit_p(tps80031_regulator_remove),
};

static int __init tps80031_regulator_init(void)
{
	return platform_driver_register(&tps80031_regulator_driver);
}
subsys_initcall(tps80031_regulator_init);

static void __exit tps80031_regulator_exit(void)
{
	platform_driver_unregister(&tps80031_regulator_driver);
}
module_exit(tps80031_regulator_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Regulator Driver for TI TPS80031 PMIC");
MODULE_ALIAS("platform:tps80031-regulator");
