// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2018-2021 Unisoc Inc.
 */

#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/sc27xx/sprd,sc2730-regs.h>
#include <linux/mfd/sc27xx/sprd,sc2730-mask.h>
#include <linux/sched.h>
#include <linux/delay.h>

/*
 * SC2730 regulator base address
 */
#define SC2730_REGULATOR_BASE		0x1800

/*
 * SC2730 regulator lock register
 */
#define SC2730_WR_UNLOCK_VALUE		0x6e7f
#define SC2730_PWR_WR_PROT		(SC2730_REGULATOR_BASE + 0x3d0)

/*
 * SC2730 enable register
 */
#define SC2730_POWER_PD_SW		(SC2730_REGULATOR_BASE + 0x01c)
#define SC2730_LDO_VDDRF18_PD		(SC2730_REGULATOR_BASE + 0x10c)
#define SC2730_LDO_VDDCAMIO_PD		(SC2730_REGULATOR_BASE + 0x118)
#define SC2730_LDO_VDDWCN_PD		(SC2730_REGULATOR_BASE + 0x11c)
#define SC2730_LDO_VDDCAMD1_PD		(SC2730_REGULATOR_BASE + 0x128)
#define SC2730_LDO_VDDCAMD0_PD		(SC2730_REGULATOR_BASE + 0x134)
#define SC2730_LDO_VDDRF1V25_PD		(SC2730_REGULATOR_BASE + 0x140)
#define SC2730_LDO_AVDD12_PD		(SC2730_REGULATOR_BASE + 0x14c)
#define SC2730_LDO_VDDCAMA0_PD		(SC2730_REGULATOR_BASE + 0x158)
#define SC2730_LDO_VDDCAMA1_PD		(SC2730_REGULATOR_BASE + 0x164)
#define SC2730_LDO_VDDCAMMOT_PD		(SC2730_REGULATOR_BASE + 0x170)
#define SC2730_LDO_VDDSIM2_PD		(SC2730_REGULATOR_BASE + 0x194)
#define SC2730_LDO_VDDEMMCCORE_PD	(SC2730_REGULATOR_BASE + 0x1a0)
#define SC2730_LDO_VDDSDCORE_PD		(SC2730_REGULATOR_BASE + 0x1ac)
#define SC2730_LDO_VDDSDIO_PD		(SC2730_REGULATOR_BASE + 0x1b8)
#define SC2730_LDO_VDDWIFIPA_PD		(SC2730_REGULATOR_BASE + 0x1d0)
#define SC2730_LDO_VDDUSB33_PD		(SC2730_REGULATOR_BASE + 0x1e8)
#define SC2730_LDO_VDDLDO0_PD		(SC2730_REGULATOR_BASE + 0x1f4)
#define SC2730_LDO_VDDLDO1_PD		(SC2730_REGULATOR_BASE + 0x200)
#define SC2730_LDO_VDDLDO2_PD		(SC2730_REGULATOR_BASE + 0x20c)
#define SC2730_LDO_VDDKPLED_PD		(SC2730_REGULATOR_BASE + 0x38c)

/*
 * SC2730 enable mask
 */
#define SC2730_DCDC_CPU_PD_MASK		BIT(4)
#define SC2730_DCDC_GPU_PD_MASK		BIT(3)
#define SC2730_DCDC_CORE_PD_MASK	BIT(5)
#define SC2730_DCDC_MODEM_PD_MASK	BIT(11)
#define SC2730_DCDC_MEM_PD_MASK		BIT(6)
#define SC2730_DCDC_MEMQ_PD_MASK	BIT(12)
#define SC2730_DCDC_GEN0_PD_MASK	BIT(8)
#define SC2730_DCDC_GEN1_PD_MASK	BIT(7)
#define SC2730_DCDC_SRAM_PD_MASK	BIT(13)
#define SC2730_LDO_AVDD18_PD_MASK	BIT(2)
#define SC2730_LDO_VDDRF18_PD_MASK	BIT(0)
#define SC2730_LDO_VDDCAMIO_PD_MASK	BIT(0)
#define SC2730_LDO_VDDWCN_PD_MASK	BIT(0)
#define SC2730_LDO_VDDCAMD1_PD_MASK	BIT(0)
#define SC2730_LDO_VDDCAMD0_PD_MASK	BIT(0)
#define SC2730_LDO_VDDRF1V25_PD_MASK	BIT(0)
#define SC2730_LDO_AVDD12_PD_MASK	BIT(0)
#define SC2730_LDO_VDDCAMA0_PD_MASK	BIT(0)
#define SC2730_LDO_VDDCAMA1_PD_MASK	BIT(0)
#define SC2730_LDO_VDDCAMMOT_PD_MASK	BIT(0)
#define SC2730_LDO_VDDSIM2_PD_MASK	BIT(0)
#define SC2730_LDO_VDDEMMCCORE_PD_MASK	BIT(0)
#define SC2730_LDO_VDDSDCORE_PD_MASK	BIT(0)
#define SC2730_LDO_VDDSDIO_PD_MASK	BIT(0)
#define SC2730_LDO_VDD28_PD_MASK	BIT(1)
#define SC2730_LDO_VDDWIFIPA_PD_MASK	BIT(0)
#define SC2730_LDO_VDD18_DCXO_PD_MASK	BIT(10)
#define SC2730_LDO_VDDUSB33_PD_MASK	BIT(0)
#define SC2730_LDO_VDDLDO0_PD_MASK	BIT(0)
#define SC2730_LDO_VDDLDO1_PD_MASK	BIT(0)
#define SC2730_LDO_VDDLDO2_PD_MASK	BIT(0)
#define SC2730_LDO_VDDKPLED_PD_MASK	BIT(15)

/*
 * SC2730 vsel register
 */
#define SC2730_DCDC_CPU_VOL		(SC2730_REGULATOR_BASE + 0x44)
#define SC2730_DCDC_GPU_VOL		(SC2730_REGULATOR_BASE + 0x54)
#define SC2730_DCDC_CORE_VOL		(SC2730_REGULATOR_BASE + 0x64)
#define SC2730_DCDC_MODEM_VOL		(SC2730_REGULATOR_BASE + 0x74)
#define SC2730_DCDC_MEM_VOL		(SC2730_REGULATOR_BASE + 0x84)
#define SC2730_DCDC_MEMQ_VOL		(SC2730_REGULATOR_BASE + 0x94)
#define SC2730_DCDC_GEN0_VOL		(SC2730_REGULATOR_BASE + 0xa4)
#define SC2730_DCDC_GEN1_VOL		(SC2730_REGULATOR_BASE + 0xb4)
#define SC2730_DCDC_SRAM_VOL		(SC2730_REGULATOR_BASE + 0xdc)
#define SC2730_LDO_AVDD18_VOL		(SC2730_REGULATOR_BASE + 0x104)
#define SC2730_LDO_VDDRF18_VOL		(SC2730_REGULATOR_BASE + 0x110)
#define SC2730_LDO_VDDCAMIO_VOL		(SC2730_REGULATOR_BASE + 0x28)
#define SC2730_LDO_VDDWCN_VOL		(SC2730_REGULATOR_BASE + 0x120)
#define SC2730_LDO_VDDCAMD1_VOL		(SC2730_REGULATOR_BASE + 0x12c)
#define SC2730_LDO_VDDCAMD0_VOL		(SC2730_REGULATOR_BASE + 0x138)
#define SC2730_LDO_VDDRF1V25_VOL	(SC2730_REGULATOR_BASE + 0x144)
#define SC2730_LDO_AVDD12_VOL		(SC2730_REGULATOR_BASE + 0x150)
#define SC2730_LDO_VDDCAMA0_VOL		(SC2730_REGULATOR_BASE + 0x15c)
#define SC2730_LDO_VDDCAMA1_VOL		(SC2730_REGULATOR_BASE + 0x168)
#define SC2730_LDO_VDDCAMMOT_VOL	(SC2730_REGULATOR_BASE + 0x174)
#define SC2730_LDO_VDDSIM2_VOL		(SC2730_REGULATOR_BASE + 0x198)
#define SC2730_LDO_VDDEMMCCORE_VOL	(SC2730_REGULATOR_BASE + 0x1a4)
#define SC2730_LDO_VDDSDCORE_VOL	(SC2730_REGULATOR_BASE + 0x1b0)
#define SC2730_LDO_VDDSDIO_VOL		(SC2730_REGULATOR_BASE + 0x1bc)
#define SC2730_LDO_VDD28_VOL		(SC2730_REGULATOR_BASE + 0x1c8)
#define SC2730_LDO_VDDWIFIPA_VOL	(SC2730_REGULATOR_BASE + 0x1d4)
#define SC2730_LDO_VDD18_DCXO_VOL	(SC2730_REGULATOR_BASE + 0x1e0)
#define SC2730_LDO_VDDUSB33_VOL		(SC2730_REGULATOR_BASE + 0x1ec)
#define SC2730_LDO_VDDLDO0_VOL		(SC2730_REGULATOR_BASE + 0x1f8)
#define SC2730_LDO_VDDLDO1_VOL		(SC2730_REGULATOR_BASE + 0x204)
#define SC2730_LDO_VDDLDO2_VOL		(SC2730_REGULATOR_BASE + 0x210)
#define SC2730_LDO_VDDKPLED_VOL		(SC2730_REGULATOR_BASE + 0x38c)

/*
 * SC2730 vsel register mask
 */
#define SC2730_DCDC_CPU_VOL_MASK	GENMASK(8, 0)
#define SC2730_DCDC_GPU_VOL_MASK	GENMASK(8, 0)
#define SC2730_DCDC_CORE_VOL_MASK	GENMASK(8, 0)
#define SC2730_DCDC_MODEM_VOL_MASK	GENMASK(8, 0)
#define SC2730_DCDC_MEM_VOL_MASK	GENMASK(7, 0)
#define SC2730_DCDC_MEMQ_VOL_MASK	GENMASK(8, 0)
#define SC2730_DCDC_GEN0_VOL_MASK	GENMASK(7, 0)
#define SC2730_DCDC_GEN1_VOL_MASK	GENMASK(7, 0)
#define SC2730_DCDC_SRAM_VOL_MASK	GENMASK(8, 0)
#define SC2730_LDO_AVDD18_VOL_MASK	GENMASK(5, 0)
#define SC2730_LDO_VDDRF18_VOL_MASK	GENMASK(5, 0)
#define SC2730_LDO_VDDCAMIO_VOL_MASK	GENMASK(5, 0)
#define SC2730_LDO_VDDWCN_VOL_MASK	GENMASK(5, 0)
#define SC2730_LDO_VDDCAMD1_VOL_MASK	GENMASK(4, 0)
#define SC2730_LDO_VDDCAMD0_VOL_MASK	GENMASK(4, 0)
#define SC2730_LDO_VDDRF1V25_VOL_MASK	GENMASK(4, 0)
#define SC2730_LDO_AVDD12_VOL_MASK	GENMASK(4, 0)
#define SC2730_LDO_VDDCAMA0_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDCAMA1_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDCAMMOT_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDSIM2_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDEMMCCORE_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDSDCORE_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDSDIO_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDD28_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDWIFIPA_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDD18_DCXO_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDUSB33_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDLDO0_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDLDO1_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDLDO2_VOL_MASK	GENMASK(7, 0)
#define SC2730_LDO_VDDKPLED_VOL_MASK	GENMASK(14, 7)

#define SPRD_MIN_VOLTAGE_UV	300000

enum sc2730_regulator_id {
	SC2730_DCDC_CPU,
	SC2730_DCDC_GPU,
	SC2730_DCDC_CORE,
	SC2730_DCDC_MODEM,
	SC2730_DCDC_MEM,
	SC2730_DCDC_MEMQ,
	SC2730_DCDC_GEN0,
	SC2730_DCDC_GEN1,
	SC2730_DCDC_SRAM,
	SC2730_LDO_AVDD18,
	SC2730_LDO_VDDRF18,
	SC2730_LDO_VDDCAMIO,
	SC2730_LDO_VDDWCN,
	SC2730_LDO_VDDCAMD1,
	SC2730_LDO_VDDCAMD0,
	SC2730_LDO_VDDRF1V25,
	SC2730_LDO_AVDD12,
	SC2730_LDO_VDDCAMA0,
	SC2730_LDO_VDDCAMA1,
	SC2730_LDO_VDDCAMMOT,
	SC2730_LDO_VDDSIM0,
	SC2730_LDO_VDDSIM1,
	SC2730_LDO_VDDSIM2,
	SC2730_LDO_VDDEMMCCORE,
	SC2730_LDO_VDDSDCORE,
	SC2730_LDO_VDDSDIO,
	SC2730_LDO_VDD28,
	SC2730_LDO_VDDWIFIPA,
	SC2730_LDO_VDD18_DCXO,
	SC2730_LDO_VDDUSB33,
	SC2730_LDO_VDDLDO0,
	SC2730_LDO_VDDLDO1,
	SC2730_LDO_VDDLDO2,
	SC2730_LDO_VDDKPLED,
	SC2730_DCDC_CORE_SLP,		//SLP_LP, DROP_EN
	SC2730_DCDC_GPU_SLP,		//SLP_LP, EMMC SLP_PD / UFS NOT SLP_PD
	SC2730_DCDC_MODEM_SLP,		//SLP_LP, EMMC SLP_PD / UFS NOT SLP_PD
	SC2730_LDO_AVDD18_SLP,		//SLP_LP, EMMC SLP_PD / UFS NOT SLP_PD
	SC2730_LDO_AVDD12_SLP,		//SLP_LP, EMMC SLP_PD / UFS NOT SLP_PD
	SC2730_DCDC_GEN1_SLP,		//SLP_LP, SLP_PD MAYBE
};

static bool sc2730_is_ufs_boot;
static bool sc2730_is_gen1_slppd_boot;

static struct dentry *debugfs_root;

static int regulator_set_voltage_sel_sprd(struct regulator_dev *rdev, unsigned int sel)
{
	if (!rdev->desc->min_uV) {
		if (sel < (SPRD_MIN_VOLTAGE_UV / rdev->desc->uV_step)) {
			char kevent[] = "kevent_begin:{\"event_id\":\"107000001\",\"event_time\"";
			char *pr_str[2];

			pr_str[0] = kasprintf(GFP_KERNEL,
					      "%s:%ld,\"task\":%s,\"PID\":%d,\"%s[0x%04x]\":0x%04x}:kevent_end\n",
					      kevent, jiffies, current->comm, current->pid,
					      rdev->desc->name, rdev->desc->vsel_reg, sel);
			pr_str[1] = NULL;
			kobject_uevent_env(&rdev->dev.kobj, KOBJ_CHANGE, pr_str);
			kfree(pr_str[0]);
		}
	}
	return regulator_set_voltage_sel_regmap(rdev, sel);
};

/* only for sc27xx Project */
static int sprd_regulator_enable_regmap(struct regulator_dev *rdev)
{
	int id = 0, ret = 0;

	id = rdev_get_id(rdev);
	switch (id) {
	case SC2730_DCDC_GPU_SLP:
	case SC2730_DCDC_MODEM_SLP:
	case SC2730_LDO_AVDD18_SLP:
	case SC2730_LDO_AVDD12_SLP:
		if (sc2730_is_ufs_boot == true)
			return ret;
		break;
	case SC2730_DCDC_GEN1_SLP:
		if (sc2730_is_gen1_slppd_boot == false)
			return ret;
		break;
	default:
		break;
	}

	return regulator_enable_regmap(rdev);
};

static int sprd_regulator_disable_regmap(struct regulator_dev *rdev)
{
	int id = 0, ret = 0;

	id = rdev_get_id(rdev);
	if (id == SC2730_LDO_VDDSDCORE || id == SC2730_LDO_VDDSDIO) {
		ret = regulator_disable_regmap(rdev);
		usleep_range(10 * 1000, 10 * 1250);
		return ret;
	}

	switch (id) {
	case SC2730_DCDC_GPU_SLP:
	case SC2730_DCDC_MODEM_SLP:
	case SC2730_LDO_AVDD18_SLP:
	case SC2730_LDO_AVDD12_SLP:
		if (sc2730_is_ufs_boot == true)
			return ret;
		break;
	case SC2730_DCDC_GEN1_SLP:
		if (sc2730_is_gen1_slppd_boot == false)
			return ret;
		break;
	default:
		break;
	}

	return regulator_disable_regmap(rdev);
};

static int sprd_regulator_is_enabled_regmap(struct regulator_dev *rdev)
{
	int id = 0, ret = 1;

	id = rdev_get_id(rdev);
	switch (id) {
	case SC2730_DCDC_GPU_SLP:
	case SC2730_DCDC_MODEM_SLP:
	case SC2730_LDO_AVDD18_SLP:
	case SC2730_LDO_AVDD12_SLP:
		if (sc2730_is_ufs_boot == true)
			return ret;
		break;
	case SC2730_DCDC_GEN1_SLP:
		if (sc2730_is_gen1_slppd_boot == false)
			return ret;
		break;
	default:
		break;
	}

	return regulator_is_enabled_regmap(rdev);
}

static const struct regulator_ops sc2730_regu_linear_ops = {
	.enable = sprd_regulator_enable_regmap,
	.disable = sprd_regulator_disable_regmap,
	.is_enabled = sprd_regulator_is_enabled_regmap,
	.list_voltage = regulator_list_voltage_linear,
	.get_voltage_sel = regulator_get_voltage_sel_regmap,
	.set_voltage_sel = regulator_set_voltage_sel_sprd,
};

#define SC2730_REGU_LINEAR(_id, en_reg, en_mask, vreg, vmask,	\
			  vstep, vmin, vmax, min_sel) {		\
	.name			= #_id,				\
	.of_match		= of_match_ptr(#_id),		\
	.ops			= &sc2730_regu_linear_ops,	\
	.type			= REGULATOR_VOLTAGE,		\
	.id			= SC2730_##_id,			\
	.owner			= THIS_MODULE,			\
	.min_uV			= vmin,				\
	.n_voltages		= ((vmax) - (vmin)) / (vstep) + 1,	\
	.uV_step		= vstep,			\
	.enable_is_inverted	= true,				\
	.enable_val		= 0,				\
	.enable_reg		= en_reg,			\
	.enable_mask		= en_mask,			\
	.vsel_reg		= vreg,				\
	.vsel_mask		= vmask,			\
	.linear_min_sel		= min_sel,			\
}

static struct regulator_desc regulators[] = {
	SC2730_REGU_LINEAR(DCDC_CPU, SC2730_POWER_PD_SW,
			   SC2730_DCDC_CPU_PD_MASK, SC2730_DCDC_CPU_VOL,
			   SC2730_DCDC_CPU_VOL_MASK, 3125, 0, 1596875,
			   0),
	SC2730_REGU_LINEAR(DCDC_GPU, SC2730_POWER_PD_SW,
			   SC2730_DCDC_GPU_PD_MASK, SC2730_DCDC_GPU_VOL,
			   SC2730_DCDC_GPU_VOL_MASK, 3125, 0, 1596875,
			   0),
	SC2730_REGU_LINEAR(DCDC_CORE, SC2730_POWER_PD_SW,
			   SC2730_DCDC_CORE_PD_MASK, SC2730_DCDC_CORE_VOL,
			   SC2730_DCDC_CORE_VOL_MASK, 3125, 0, 1596875,
			   0),
	SC2730_REGU_LINEAR(DCDC_MODEM, SC2730_POWER_PD_SW,
			   SC2730_DCDC_MODEM_PD_MASK, SC2730_DCDC_MODEM_VOL,
			   SC2730_DCDC_MODEM_VOL_MASK, 3125, 0, 1596875,
			   0),
	SC2730_REGU_LINEAR(DCDC_MEM, SC2730_POWER_PD_SW,
			   SC2730_DCDC_MEM_PD_MASK, SC2730_DCDC_MEM_VOL,
			   SC2730_DCDC_MEM_VOL_MASK, 6250, 0, 1593750,
			   0),
	SC2730_REGU_LINEAR(DCDC_MEMQ, SC2730_POWER_PD_SW,
			   SC2730_DCDC_MEMQ_PD_MASK, SC2730_DCDC_MEMQ_VOL,
			   SC2730_DCDC_MEMQ_VOL_MASK, 3125, 0, 1596875,
			   0),
	SC2730_REGU_LINEAR(DCDC_GEN0, SC2730_POWER_PD_SW,
			   SC2730_DCDC_GEN0_PD_MASK, SC2730_DCDC_GEN0_VOL,
			   SC2730_DCDC_GEN0_VOL_MASK, 9375, 20000, 2410625,
			   0),
	SC2730_REGU_LINEAR(DCDC_GEN1, SC2730_POWER_PD_SW,
			   SC2730_DCDC_GEN1_PD_MASK, SC2730_DCDC_GEN1_VOL,
			   SC2730_DCDC_GEN1_VOL_MASK, 6250, 50000, 1643750,
			   0),
	SC2730_REGU_LINEAR(DCDC_SRAM, SC2730_POWER_PD_SW,
			   SC2730_DCDC_SRAM_PD_MASK, SC2730_DCDC_SRAM_VOL,
			   SC2730_DCDC_SRAM_VOL_MASK, 3125, 0, 1596875,
			   0),
	SC2730_REGU_LINEAR(LDO_AVDD18, SC2730_POWER_PD_SW,
			   SC2730_LDO_AVDD18_PD_MASK, SC2730_LDO_AVDD18_VOL,
			   SC2730_LDO_AVDD18_VOL_MASK, 10000, 1175000, 1805000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDDRF18, SC2730_LDO_VDDRF18_PD,
			   SC2730_LDO_VDDRF18_PD_MASK, SC2730_LDO_VDDRF18_VOL,
			   SC2730_LDO_VDDRF18_VOL_MASK, 10000, 1175000, 1805000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDDCAMIO, SC2730_LDO_VDDCAMIO_PD,
			   SC2730_LDO_VDDCAMIO_PD_MASK, SC2730_LDO_VDDCAMIO_VOL,
			   SC2730_LDO_VDDCAMIO_VOL_MASK, 10000, 1200000,
			   1830000, 0),
	SC2730_REGU_LINEAR(LDO_VDDWCN, SC2730_LDO_VDDWCN_PD,
			   SC2730_LDO_VDDWCN_PD_MASK, SC2730_LDO_VDDWCN_VOL,
			   SC2730_LDO_VDDWCN_VOL_MASK, 15000, 900000, 1845000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDDCAMD1, SC2730_LDO_VDDCAMD1_PD,
			   SC2730_LDO_VDDCAMD1_PD_MASK, SC2730_LDO_VDDCAMD1_VOL,
			   SC2730_LDO_VDDCAMD1_VOL_MASK, 15000, 900000,
			   1365000, 0),
	SC2730_REGU_LINEAR(LDO_VDDCAMD0, SC2730_LDO_VDDCAMD0_PD,
			   SC2730_LDO_VDDCAMD0_PD_MASK, SC2730_LDO_VDDCAMD0_VOL,
			   SC2730_LDO_VDDCAMD0_VOL_MASK, 15000, 900000,
			   1365000, 0),
	SC2730_REGU_LINEAR(LDO_VDDRF1V25, SC2730_LDO_VDDRF1V25_PD,
			   SC2730_LDO_VDDRF1V25_PD_MASK,
			   SC2730_LDO_VDDRF1V25_VOL,
			   SC2730_LDO_VDDRF1V25_VOL_MASK, 15000, 900000,
			   1365000, 0),
	SC2730_REGU_LINEAR(LDO_AVDD12, SC2730_LDO_AVDD12_PD,
			   SC2730_LDO_AVDD12_PD_MASK, SC2730_LDO_AVDD12_VOL,
			   SC2730_LDO_AVDD12_VOL_MASK, 15000, 900000, 1365000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDDCAMA0, SC2730_LDO_VDDCAMA0_PD,
			   SC2730_LDO_VDDCAMA0_PD_MASK, SC2730_LDO_VDDCAMA0_VOL,
			   SC2730_LDO_VDDCAMA0_VOL_MASK, 10000, 1200000,
			   3750000, 0),
	SC2730_REGU_LINEAR(LDO_VDDCAMA1, SC2730_LDO_VDDCAMA1_PD,
			   SC2730_LDO_VDDCAMA1_PD_MASK, SC2730_LDO_VDDCAMA1_VOL,
			   SC2730_LDO_VDDCAMA1_VOL_MASK, 10000, 1200000,
			   3750000, 0),
	SC2730_REGU_LINEAR(LDO_VDDCAMMOT, SC2730_LDO_VDDCAMMOT_PD,
			   SC2730_LDO_VDDCAMMOT_PD_MASK,
			   SC2730_LDO_VDDCAMMOT_VOL,
			   SC2730_LDO_VDDCAMMOT_VOL_MASK, 10000, 1200000,
			   3750000, 0),
	SC2730_REGU_LINEAR(LDO_VDDSIM2, SC2730_LDO_VDDSIM2_PD,
			   SC2730_LDO_VDDSIM2_PD_MASK, SC2730_LDO_VDDSIM2_VOL,
			   SC2730_LDO_VDDSIM2_VOL_MASK, 10000, 1200000, 3750000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDDEMMCCORE, SC2730_LDO_VDDEMMCCORE_PD,
			   SC2730_LDO_VDDEMMCCORE_PD_MASK,
			   SC2730_LDO_VDDEMMCCORE_VOL,
			   SC2730_LDO_VDDEMMCCORE_VOL_MASK, 10000, 1200000,
			   3750000, 0),
	SC2730_REGU_LINEAR(LDO_VDDSDCORE, SC2730_LDO_VDDSDCORE_PD,
			   SC2730_LDO_VDDSDCORE_PD_MASK,
			   SC2730_LDO_VDDSDCORE_VOL,
			   SC2730_LDO_VDDSDCORE_VOL_MASK, 10000, 1200000,
			   3750000, 0),
	SC2730_REGU_LINEAR(LDO_VDDSDIO, SC2730_LDO_VDDSDIO_PD,
			   SC2730_LDO_VDDSDIO_PD_MASK, SC2730_LDO_VDDSDIO_VOL,
			   SC2730_LDO_VDDSDIO_VOL_MASK, 10000, 1200000, 3750000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDD28, SC2730_POWER_PD_SW,
			   SC2730_LDO_VDD28_PD_MASK, SC2730_LDO_VDD28_VOL,
			   SC2730_LDO_VDD28_VOL_MASK, 10000, 1200000, 3750000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDDWIFIPA, SC2730_LDO_VDDWIFIPA_PD,
			   SC2730_LDO_VDDWIFIPA_PD_MASK,
			   SC2730_LDO_VDDWIFIPA_VOL,
			   SC2730_LDO_VDDWIFIPA_VOL_MASK, 10000, 1200000,
			   3750000, 0),
	SC2730_REGU_LINEAR(LDO_VDD18_DCXO, SC2730_POWER_PD_SW,
			   SC2730_LDO_VDD18_DCXO_PD_MASK,
			   SC2730_LDO_VDD18_DCXO_VOL,
			   SC2730_LDO_VDD18_DCXO_VOL_MASK, 10000, 1200000,
			   3750000, 0),
	SC2730_REGU_LINEAR(LDO_VDDUSB33, SC2730_LDO_VDDUSB33_PD,
			   SC2730_LDO_VDDUSB33_PD_MASK, SC2730_LDO_VDDUSB33_VOL,
			   SC2730_LDO_VDDUSB33_VOL_MASK, 10000, 1200000,
			   3750000, 0),
	SC2730_REGU_LINEAR(LDO_VDDLDO0, SC2730_LDO_VDDLDO0_PD,
			   SC2730_LDO_VDDLDO0_PD_MASK, SC2730_LDO_VDDLDO0_VOL,
			   SC2730_LDO_VDDLDO0_VOL_MASK, 10000, 1200000, 3750000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDDLDO1, SC2730_LDO_VDDLDO1_PD,
			   SC2730_LDO_VDDLDO1_PD_MASK, SC2730_LDO_VDDLDO1_VOL,
			   SC2730_LDO_VDDLDO1_VOL_MASK, 10000, 1200000, 3750000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDDLDO2, SC2730_LDO_VDDLDO2_PD,
			   SC2730_LDO_VDDLDO2_PD_MASK, SC2730_LDO_VDDLDO2_VOL,
			   SC2730_LDO_VDDLDO2_VOL_MASK, 10000, 1200000, 3750000,
			   0),
	SC2730_REGU_LINEAR(LDO_VDDKPLED, SC2730_LDO_VDDKPLED_PD,
			   SC2730_LDO_VDDKPLED_PD_MASK, SC2730_LDO_VDDKPLED_VOL,
			   SC2730_LDO_VDDKPLED_VOL_MASK, 10000, 1200000,
			   3750000, 0),
	SC2730_REGU_LINEAR(DCDC_CORE_SLP, REG_ANA_SLP_DCDC_PD_CTRL,
			   MASK_ANA_SLP_DCDCCORE_DROP_EN, SC2730_DCDC_CORE_VOL,
			   SC2730_DCDC_CORE_VOL_MASK, 3125, 0, 1596875,
			   0),
	SC2730_REGU_LINEAR(DCDC_GPU_SLP, REG_ANA_SLP_DCDC_PD_CTRL,
			   MASK_ANA_SLP_DCDCGPU_PD_EN, SC2730_DCDC_GPU_VOL,
			   SC2730_DCDC_GPU_VOL_MASK, 3125, 0, 1596875,
			   0),
	SC2730_REGU_LINEAR(DCDC_MODEM_SLP, REG_ANA_SLP_DCDC_PD_CTRL,
			   MASK_ANA_SLP_DCDCMODEM_PD_EN, SC2730_DCDC_MODEM_VOL,
			   SC2730_DCDC_MODEM_VOL_MASK, 3125, 0, 1596875,
			   0),
	SC2730_REGU_LINEAR(LDO_AVDD18_SLP, REG_ANA_SLP_LDO_PD_CTRL1,
			   MASK_ANA_SLP_LDO_AVDD18_PD_EN, SC2730_LDO_AVDD18_VOL,
			   SC2730_LDO_AVDD18_VOL_MASK, 10000, 1175000, 1805000,
			   0),
	SC2730_REGU_LINEAR(LDO_AVDD12_SLP, REG_ANA_SLP_LDO_PD_CTRL1,
			   MASK_ANA_SLP_LDO_AVDD12_PD_EN, SC2730_LDO_AVDD12_VOL,
			   SC2730_LDO_AVDD12_VOL_MASK, 15000, 900000, 1365000,
			   0),
	SC2730_REGU_LINEAR(DCDC_GEN1_SLP, REG_ANA_SLP_DCDC_PD_CTRL,
			   MASK_ANA_SLP_DCDCGEN1_PD_EN, SC2730_DCDC_GEN1_VOL,
			   SC2730_DCDC_GEN1_VOL_MASK, 6250, 50000, 1643750,
			   0),
};

static int debugfs_enable_get(void *data, u64 *val)
{
	struct regulator_dev *rdev = data;

	if (rdev && rdev->desc->ops->is_enabled)
		*val = rdev->desc->ops->is_enabled(rdev);
	else
		*val = ~0ULL;
	return 0;
}

static int debugfs_enable_set(void *data, u64 val)
{
	struct regulator_dev *rdev = data;

	if (rdev && rdev->desc->ops->enable && rdev->desc->ops->disable) {
		if (val)
			rdev->desc->ops->enable(rdev);
		else
			rdev->desc->ops->disable(rdev);
	}

	return 0;
}

static int debugfs_voltage_get(void *data, u64 *val)
{
	int sel, ret;
	struct regulator_dev *rdev = data;

	if (!rdev || !rdev->desc->ops->get_voltage_sel || !rdev->desc->ops->list_voltage)
		return -EINVAL;

	sel = rdev->desc->ops->get_voltage_sel(rdev);
	if (sel < 0)
		return sel;

	ret = rdev->desc->ops->list_voltage(rdev, sel);
	*val = ret / 1000;

	return 0;
}

static int debugfs_voltage_set(void *data, u64 val)
{
	int selector;
	struct regulator_dev *rdev = data;

	if (!rdev || !rdev->desc->ops->set_voltage_sel)
		return -EINVAL;

	val = val * 1000;
	if (val < rdev->desc->min_uV)
		return -EINVAL;

	selector = regulator_map_voltage_linear(rdev,
						val - rdev->desc->uV_step / 2,
						val + rdev->desc->uV_step / 2);

	if (selector < 0)
		return selector;

	return rdev->desc->ops->set_voltage_sel(rdev, selector);
}

DEFINE_SIMPLE_ATTRIBUTE(fops_enable,
			debugfs_enable_get, debugfs_enable_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(fops_ldo,
			debugfs_voltage_get, debugfs_voltage_set, "%llu\n");

static void sc2730_regulator_debugfs_init(struct regulator_dev *rdev)
{
	struct dentry *debugfs;

	if (IS_ERR_OR_NULL(debugfs_root)) {
		debugfs_root = debugfs_create_dir("sprd_regulator", NULL);
		if (IS_ERR_OR_NULL(debugfs_root)) {
			dev_warn(&rdev->dev, "Failed to recreate debugfs directory\n");
			return;
		}
	}

	debugfs = debugfs_create_dir(rdev->desc->name, debugfs_root);
	if (IS_ERR_OR_NULL(debugfs)) {
		dev_warn(&rdev->dev, "Failed to create %s directory\n", rdev->desc->name);
		return;
	}

	debugfs_create_file("enable", 0644, debugfs, rdev, &fops_enable);
	debugfs_create_file("voltage", 0644, debugfs, rdev, &fops_ldo);
}

static int sc2730_regulator_unlock(struct regmap *regmap)
{
	return regmap_write(regmap, SC2730_PWR_WR_PROT, SC2730_WR_UNLOCK_VALUE);
}

static int sc2730_get_ufs_device_info(struct regmap *regmap)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int ret;
	u32 tmp;

	sc2730_is_ufs_boot = 0;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret) {
		pr_err("Sc2730 Can not get ufs bootargs!\n");

		ret = regmap_read(regmap, REG_ANA_SLP_LDO_PD_CTRL1, &tmp);
		if (ret) {
			pr_err("Sc2730 Can't get slp_pd regmap! default ufs.\n");
			sc2730_is_ufs_boot = 1;
			return ret;
		}

		tmp &= MASK_ANA_SLP_LDO_AVDD18_PD_EN;
		if (!tmp) {
			sc2730_is_ufs_boot = 1;
			pr_info("Sc2730 regmap boot from ufs!\n");
			return 0;
		}

		pr_info("Sc2730 regmap boot from emmc!\n");
		return 0;
	}

	if (strstr(cmd_line, "ufs")) {
		sc2730_is_ufs_boot = 1;
		pr_info("Sc2730 bootargs boot from ufs!\n");
		return 0;
	}

	pr_info("Sc2730 bootargs boot from emmc!\n");

	return 0;
}

static int sc2730_get_gen1_slppd_info(struct regmap *regmap)
{
	int ret;
	u32 tmp;

	sc2730_is_gen1_slppd_boot = 0;

	ret = regmap_read(regmap, REG_ANA_SLP_DCDC_PD_CTRL, &tmp);
	if (ret) {
		pr_err("Sc2730 Can't get dcdcpd ctrl regmap! default slplp.\n");
		return ret;
	}

	tmp &= MASK_ANA_SLP_DCDCGEN1_PD_EN;
	if (tmp) {
		sc2730_is_gen1_slppd_boot = 1;
		pr_info("Sc2730 regmap gen1 bound slp [%d]!\n", tmp);
		return 0;
	}

	pr_info("Sc2730 regmap gen1 unbound slp [%d]!\n", tmp);

	return 0;
}

static int sc2730_regulator_probe(struct platform_device *pdev)
{
	int i, ret;
	struct regmap *regmap;
	struct regulator_config config = { };
	struct regulator_dev *rdev;

	regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!regmap) {
		dev_err(&pdev->dev, "failed to get regmap.\n");
		return -ENODEV;
	}

	ret = sc2730_regulator_unlock(regmap);
	if (ret) {
		dev_err(&pdev->dev, "failed to release regulator lock\n");
		return ret;
	}

	ret = sc2730_get_ufs_device_info(regmap);
	if (ret)
		dev_err(&pdev->dev, "failed to get ufs device info\n");

	ret = sc2730_get_gen1_slppd_info(regmap);
	if (ret)
		dev_err(&pdev->dev, "failed to get gen1 slppd info\n");

	config.dev = &pdev->dev;
	config.regmap = regmap;

	debugfs_root = debugfs_create_dir("sprd_regulator", NULL);
	if (IS_ERR(debugfs_root))
		dev_warn(&pdev->dev, "Failed to create debugfs directory\n");

	for (i = 0; i < ARRAY_SIZE(regulators); i++) {
		rdev = devm_regulator_register(&pdev->dev, &regulators[i],
					       &config);
		if (IS_ERR(rdev)) {
			dev_err(&pdev->dev, "failed to register regulator %s\n",
				regulators[i].name);
			continue;
		}

		sc2730_regulator_debugfs_init(rdev);
	}

	return 0;
}

static int sc2730_regulator_remove(struct platform_device *pdev)
{
	debugfs_remove_recursive(debugfs_root);
	return 0;
}

static const struct of_device_id sc2730_regulator_match[] = {
	{ .compatible = "sprd,sc2730-regulator" },
	{},
};
MODULE_DEVICE_TABLE(of, sc2730_regulator_match);

static struct platform_driver sc2730_regulator_driver = {
	.driver = {
		.name = "sc2730-regulator",
		.of_match_table = sc2730_regulator_match,
	},
	.probe = sc2730_regulator_probe,
	.remove = sc2730_regulator_remove,
};

module_platform_driver(sc2730_regulator_driver);

MODULE_AUTHOR("Chen Junhui <erick.chen@spreadtrum.com>");
MODULE_DESCRIPTION("Spreadtrum SC2730 regulator driver");
MODULE_LICENSE("GPL v2");
