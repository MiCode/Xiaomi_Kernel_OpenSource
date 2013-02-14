/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>

#include <mach/msm_iomap.h>

/* Address for Perf Level Registor */
#define VDD_APC_PLEVEL_BASE (MSM_CLK_CTL_BASE + 0x0298)
#define VDD_APC_PLEVEL(n) (VDD_APC_PLEVEL_BASE + 4 * n)

/* Address for SYS_P_Level register */
#define VDD_SVS_PLEVEL_ADDR (MSM_CSR_BASE + 0x124)

#define MV_TO_UV(mv) ((mv)*1000)
#define UV_TO_MV(uv) (((uv)+999)/1000)

#define MSM_VP_REGULATOR_DEV_NAME  "vp-regulator"

/**
 * Convert Voltage to PLEVEL register value
 * Here x is required voltage in minivolt
 * e.g. if Required voltage is 1200mV then
 * required value to be programmed into the
 * Plevel register is 0x32. This equation is
 * based on H/W logic being used in SVS controller.
 *
 * Here we are taking the minimum voltage step
 * to be 12.5mV as per H/W logic and adding 0x20
 * is for selecting the reference voltage.
 * 750mV is minimum voltage of MSMC2 smps.
 */
#define VOLT_TO_BIT(x) (((x-750)/(12500/1000)) + 0x20)
#define VREG_VREF_SEL		(1 << 5)
#define VREG_VREF_SEL_SHIFT	(0x5)
#define VREG_PD_EN		(1 << 6)
#define VREG_PD_EN_SHIFT	(0x6)
#define VREG_LVL_M		(0x1F)

/**
 * struct msm_vp -  Structure for VP
 * @regulator_dev: structure for regulator device
 * @current_voltage: current voltage value
 */
struct msm_vp {
	struct device		*dev;
	struct regulator_dev	*rdev;
	int current_voltage;
};

/* Function to change the Vdd Level */
static int vp_reg_set_voltage(struct regulator_dev *rdev, int min_uV,
						int max_uV, unsigned *sel)
{
	struct msm_vp *vp = rdev_get_drvdata(rdev);
	uint32_t reg_val, perf_level, plevel, cur_plevel, fine_step_volt;

	reg_val = readl_relaxed(VDD_SVS_PLEVEL_ADDR);
	perf_level = reg_val & 0x07;

	plevel = (min_uV - 750000) / 25000;
	fine_step_volt = (min_uV - 750000) % 25000;

	/**
	 * Program the new voltage level for the current perf_level
	 * in corresponding PLEVEL register.
	 */
	cur_plevel = readl_relaxed(VDD_APC_PLEVEL(perf_level));
	/* clear lower 7 bits */
	cur_plevel &= ~(0x7F);
	cur_plevel |= (plevel | VREG_VREF_SEL);
	if (fine_step_volt >= 12500)
		cur_plevel |= VREG_PD_EN;
	writel_relaxed(cur_plevel, VDD_APC_PLEVEL(perf_level));

	/* Clear the current perf level */
	reg_val &= 0xF8;
	writel_relaxed(reg_val, VDD_SVS_PLEVEL_ADDR);

	/* Initiate the PMIC SSBI request to change the voltage */
	reg_val |= (BIT(7) | perf_level << 3);
	writel_relaxed(reg_val, VDD_SVS_PLEVEL_ADDR);
	mb();
	udelay(62);

	if ((readl_relaxed(VDD_SVS_PLEVEL_ADDR) & 0x07) != perf_level) {
		pr_err("Vdd Set Failed\n");
		return -EIO;
	}

	vp->current_voltage = (min_uV / 1000);
	return 0;
}

static int vp_reg_get_voltage(struct regulator_dev *rdev)
{
	uint32_t reg_val, perf_level, vlevel, cur_plevel;
	uint32_t vref_sel, pd_en;
	uint32_t cur_voltage;

	reg_val = readl_relaxed(VDD_SVS_PLEVEL_ADDR);
	perf_level = reg_val & 0x07;

	cur_plevel = readl_relaxed(VDD_APC_PLEVEL(perf_level));
	vref_sel = (cur_plevel >> VREG_VREF_SEL_SHIFT) & 0x1;
	pd_en = (cur_plevel >> VREG_PD_EN_SHIFT) & 0x1;
	vlevel = cur_plevel & VREG_LVL_M;

	cur_voltage = (750000 + (pd_en * 12500) +
				(vlevel * 25000)) * (2 - vref_sel);
	return cur_voltage;
}

static int vp_reg_enable(struct regulator_dev *rdev)
{
	return 0;
}

static int vp_reg_disable(struct regulator_dev *rdev)
{
	return 0;
}

/* Regulator registration specific data */
/* FIXME: should move to board-xx-regulator.c file */
static struct regulator_consumer_supply vp_consumer =
	REGULATOR_SUPPLY("vddx_cx", "msm-cpr");

static struct regulator_init_data vp_reg_data = {
	.constraints	= {
		.name		= "vddx_c2",
		.min_uV		= 750000,
		.max_uV		= 1500000,
		.valid_ops_mask	= REGULATOR_CHANGE_VOLTAGE |
					REGULATOR_CHANGE_STATUS,
		.valid_modes_mask = REGULATOR_MODE_NORMAL,
		.boot_on	= 1,
		.input_uV	= 0,
		.always_on	= 1,
	},
	.num_consumer_supplies = 1,
	.consumer_supplies = &vp_consumer,
};

/* Regulator specific ops */
static struct regulator_ops vp_reg_ops = {
	.enable			= vp_reg_enable,
	.disable		= vp_reg_disable,
	.get_voltage		= vp_reg_get_voltage,
	.set_voltage		= vp_reg_set_voltage,
};

/* Regulator Description */
static struct regulator_desc vp_reg = {
	.name = "vddcx",
	.id = -1,
	.ops = &vp_reg_ops,
	.type = REGULATOR_VOLTAGE,
};

static int __devinit msm_vp_reg_probe(struct platform_device *pdev)
{
	struct msm_vp *vp;
	int rc;

	vp = kzalloc(sizeof(struct msm_vp), GFP_KERNEL);
	if (!vp) {
		pr_err("Could not allocate memory for VP\n");
		return -ENOMEM;
	}

	vp->rdev = regulator_register(&vp_reg, NULL, &vp_reg_data, vp, NULL);
	if (IS_ERR(vp->rdev)) {
		rc = PTR_ERR(vp->rdev);
		pr_err("Failed to register regulator: %d\n", rc);
		goto error;
	}

	platform_set_drvdata(pdev, vp);

	return 0;
error:
	kfree(vp);
	return rc;
}

static int __devexit msm_vp_reg_remove(struct platform_device *pdev)
{
	struct msm_vp *vp = platform_get_drvdata(pdev);

	regulator_unregister(vp->rdev);
	platform_set_drvdata(pdev, NULL);
	kfree(vp);

	return 0;
}

static struct platform_driver msm_vp_reg_driver = {
	.probe	= msm_vp_reg_probe,
	.remove = __devexit_p(msm_vp_reg_remove),
	.driver = {
		.name	= MSM_VP_REGULATOR_DEV_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init msm_vp_reg_init(void)
{
	return platform_driver_register(&msm_vp_reg_driver);
}
postcore_initcall(msm_vp_reg_init);

static void __exit msm_vp_reg_exit(void)
{
	platform_driver_unregister(&msm_vp_reg_driver);
}
module_exit(msm_vp_reg_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MSM VP regulator driver");
MODULE_VERSION("1.0");
MODULE_ALIAS("platform:" MSM_VP_REGULATOR_DEV_NAME);
