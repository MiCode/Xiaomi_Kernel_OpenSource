/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/err.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/bitops.h>
#include <linux/slab.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/cpr-regulator.h>

struct cpr_regulator {
	struct regulator_desc		rdesc;
	struct regulator_dev		*rdev;
	bool				enabled;
	int				corner;

	/* Process voltage parameters */
	phys_addr_t	efuse_phys;
	u32		num_efuse_bits;
	u32		efuse_bit_pos[CPR_PVS_EFUSE_BITS_MAX];
	u32		pvs_bin_process[CPR_PVS_EFUSE_BINS_MAX];
	u32		pvs_corner_ceiling[NUM_APC_PVS][CPR_CORNER_MAX];
	/* Process voltage variables */
	u32		pvs_bin;
	u32		pvs_process;
	u32		*corner_ceiling;

	/* APC voltage regulator */
	struct regulator	*vdd_apc;

	/* Dependency parameters */
	struct regulator	*vdd_mx;
	int			vdd_mx_vmax;
	int			vdd_mx_vmin_method;
	int			vdd_mx_vmin;
};

static int cpr_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);

	return cpr_vreg->enabled;
}

static int cpr_regulator_enable(struct regulator_dev *rdev)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);
	int rc = 0;

	/* Enable dependency power before vdd_apc */
	if (cpr_vreg->vdd_mx) {
		rc = regulator_enable(cpr_vreg->vdd_mx);
		if (rc) {
			pr_err("regulator_enable: vdd_mx: rc=%d\n", rc);
			return rc;
		}
	}

	rc = regulator_enable(cpr_vreg->vdd_apc);
	if (!rc)
		cpr_vreg->enabled = true;
	else
		pr_err("regulator_enable: vdd_apc: rc=%d\n", rc);

	return rc;
}

static int cpr_regulator_disable(struct regulator_dev *rdev)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);
	int rc;

	rc = regulator_disable(cpr_vreg->vdd_apc);
	if (!rc) {
		if (cpr_vreg->vdd_mx)
			rc = regulator_disable(cpr_vreg->vdd_mx);

		if (rc)
			pr_err("regulator_disable: vdd_mx: rc=%d\n", rc);
		else
			cpr_vreg->enabled = false;
	} else {
		pr_err("regulator_disable: vdd_apc: rc=%d\n", rc);
	}

	return rc;
}

static int cpr_regulator_set_voltage(struct regulator_dev *rdev,
		int min_uV, int max_uV, unsigned *selector)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);
	int rc;
	int vdd_apc_min, vdd_apc_max, vdd_mx_vmin = 0;
	int change_dir = 0;

	if (cpr_vreg->vdd_mx) {
		if (min_uV > cpr_vreg->corner)
			change_dir = 1;
		else if (min_uV < cpr_vreg->corner)
			change_dir = -1;
	}

	vdd_apc_min = cpr_vreg->corner_ceiling[min_uV];
	vdd_apc_max = cpr_vreg->corner_ceiling[CPR_CORNER_SUPER_TURBO];

	if (change_dir) {
		/* Determine the vdd_mx voltage */
		switch (cpr_vreg->vdd_mx_vmin_method) {
		case VDD_MX_VMIN_APC:
			vdd_mx_vmin = vdd_apc_min;
			break;
		case VDD_MX_VMIN_APC_CORNER_CEILING:
			vdd_mx_vmin = vdd_apc_min;
			break;
		case VDD_MX_VMIN_APC_SLOW_CORNER_CEILING:
			vdd_mx_vmin = cpr_vreg->pvs_corner_ceiling
					[APC_PVS_SLOW][min_uV];
			break;
		case VDD_MX_VMIN_MX_VMAX:
		default:
			vdd_mx_vmin = cpr_vreg->vdd_mx_vmax;
			break;
		}
	}

	if (change_dir > 0) {
		if (vdd_mx_vmin < cpr_vreg->vdd_mx_vmin) {
			/* Check and report the value in case */
			pr_err("Up: but new %d < old %d uV\n", vdd_mx_vmin,
					cpr_vreg->vdd_mx_vmin);
		}

		rc = regulator_set_voltage(cpr_vreg->vdd_mx, vdd_mx_vmin,
					   cpr_vreg->vdd_mx_vmax);
		if (!rc) {
			cpr_vreg->vdd_mx_vmin = vdd_mx_vmin;
		} else {
			pr_err("set: vdd_mx [%d] = %d uV: rc=%d\n",
			       min_uV, vdd_mx_vmin, rc);
			return rc;
		}
	}

	rc = regulator_set_voltage(cpr_vreg->vdd_apc,
				   vdd_apc_min, vdd_apc_max);
	if (!rc) {
		cpr_vreg->corner = min_uV;
	} else {
		pr_err("set: vdd_apc [%d] = %d uV: rc=%d\n",
		       min_uV, vdd_apc_min, rc);
		return rc;
	}

	if (change_dir < 0) {
		if (vdd_mx_vmin > cpr_vreg->vdd_mx_vmin) {
			/* Check and report the value in case */
			pr_err("Down: but new %d >= old %d uV\n", vdd_mx_vmin,
			       cpr_vreg->vdd_mx_vmin);
		}

		rc = regulator_set_voltage(cpr_vreg->vdd_mx, vdd_mx_vmin,
					   cpr_vreg->vdd_mx_vmax);
		if (!rc) {
			cpr_vreg->vdd_mx_vmin = vdd_mx_vmin;
		} else {
			pr_err("set: vdd_mx [%d] = %d uV: rc=%d\n",
			       min_uV, vdd_mx_vmin, rc);
			return rc;
		}
	}

	pr_debug("set [corner:%d] = %d uV: rc=%d\n", min_uV, vdd_apc_min, rc);
	return rc;
}

static int cpr_regulator_get_voltage(struct regulator_dev *rdev)
{
	struct cpr_regulator *cpr_vreg = rdev_get_drvdata(rdev);

	return cpr_vreg->corner;
}

static struct regulator_ops cpr_corner_ops = {
	.enable			= cpr_regulator_enable,
	.disable		= cpr_regulator_disable,
	.is_enabled		= cpr_regulator_is_enabled,
	.set_voltage		= cpr_regulator_set_voltage,
	.get_voltage		= cpr_regulator_get_voltage,
};

static int __init cpr_regulator_pvs_init(struct cpr_regulator *cpr_vreg)
{
	void __iomem *efuse_base;
	u32 efuse_bits;
	int i, bit_pos;
	u32 vmax;

	efuse_base = ioremap(cpr_vreg->efuse_phys, 4);
	if (!efuse_base) {
		pr_err("Unable to map efuse_phys 0x%x\n",
				cpr_vreg->efuse_phys);
		return -EINVAL;
	}

	efuse_bits = readl_relaxed(efuse_base);

	/* Construct PVS process # from the efuse bits */
	for (i = 0; i < cpr_vreg->num_efuse_bits; i++) {
		bit_pos = cpr_vreg->efuse_bit_pos[i];
		cpr_vreg->pvs_bin |= (efuse_bits & BIT(bit_pos)) ? BIT(i) : 0;
	}

	cpr_vreg->pvs_process = cpr_vreg->pvs_bin_process[cpr_vreg->pvs_bin];
	if (cpr_vreg->pvs_process >= NUM_APC_PVS)
		cpr_vreg->pvs_process = APC_PVS_NO;

	/* Use ceiling voltage of Turbo@Slow for all corners of APC_PVS_NO
	   but use SuperTurbo@Slow for its SuperTurbo */
	vmax = cpr_vreg->pvs_corner_ceiling[APC_PVS_SLOW][CPR_CORNER_TURBO];
	for (i = CPR_CORNER_SVS; i <= CPR_CORNER_TURBO; i++)
		cpr_vreg->pvs_corner_ceiling[APC_PVS_NO][i] = vmax;
	cpr_vreg->pvs_corner_ceiling[APC_PVS_NO][CPR_CORNER_SUPER_TURBO]
		= cpr_vreg->pvs_corner_ceiling[APC_PVS_SLOW]
					[CPR_CORNER_SUPER_TURBO];

	cpr_vreg->corner_ceiling =
		cpr_vreg->pvs_corner_ceiling[cpr_vreg->pvs_process];

	iounmap(efuse_base);

	pr_info("PVS Info: efuse_phys=0x%08X, n_bits=%d\n",
		cpr_vreg->efuse_phys, cpr_vreg->num_efuse_bits);
	pr_info("PVS Info: efuse=0x%08X, bin=%d, process=%d\n",
		efuse_bits, cpr_vreg->pvs_bin, cpr_vreg->pvs_process);

	return 0;
}

static int __init cpr_regulator_apc_init(struct platform_device *pdev,
					 struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	int rc;

	cpr_vreg->vdd_apc = devm_regulator_get(&pdev->dev, "vdd-apc");
	if (IS_ERR_OR_NULL(cpr_vreg->vdd_apc)) {
		rc = PTR_RET(cpr_vreg->vdd_apc);
		if (rc != -EPROBE_DEFER)
			pr_err("devm_regulator_get: rc=%d\n", rc);
		return rc;
	}

	/* Check dependencies */
	if (of_property_read_bool(of_node, "vdd-mx-supply")) {
		cpr_vreg->vdd_mx = devm_regulator_get(&pdev->dev, "vdd-mx");
		if (IS_ERR_OR_NULL(cpr_vreg->vdd_mx)) {
			rc = PTR_RET(cpr_vreg->vdd_mx);
			if (rc != -EPROBE_DEFER)
				pr_err("devm_regulator_get: vdd-mx: rc=%d\n",
				       rc);
			return rc;
		}
	}

	/* Parse dependency parameters */
	if (cpr_vreg->vdd_mx) {
		rc = of_property_read_u32(of_node, "qcom,vdd-mx-vmax",
				 &cpr_vreg->vdd_mx_vmax);
		if (rc < 0) {
			pr_err("vdd-mx-vmax missing: rc=%d\n", rc);
			return rc;
		}

		rc = of_property_read_u32(of_node, "qcom,vdd-mx-vmin-method",
				 &cpr_vreg->vdd_mx_vmin_method);
		if (rc < 0) {
			pr_err("vdd-mx-vmin-method missing: rc=%d\n", rc);
			return rc;
		}
		if (cpr_vreg->vdd_mx_vmin_method > VDD_MX_VMIN_MX_VMAX) {
			pr_err("Invalid vdd-mx-vmin-method(%d)\n",
				cpr_vreg->vdd_mx_vmin_method);
			return -EINVAL;
		}
	}

	return 0;
}

static void cpr_regulator_apc_exit(struct cpr_regulator *cpr_vreg)
{
	if (cpr_vreg->enabled) {
		regulator_disable(cpr_vreg->vdd_apc);

		if (cpr_vreg->vdd_mx)
			regulator_disable(cpr_vreg->vdd_mx);
	}
}

static int __init cpr_regulator_parse_dt(struct platform_device *pdev,
					 struct cpr_regulator *cpr_vreg)
{
	struct device_node *of_node = pdev->dev.of_node;
	struct resource *res;
	int rc;
	size_t pvs_bins;

	/* Parse process voltage parameters */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
					   "efuse_phys");
	if (!res || !res->start) {
		pr_err("efuse_phys missing: res=%p\n", res);
		return -EINVAL;
	}
	cpr_vreg->efuse_phys = res->start;

	rc = of_property_read_u32(of_node, "qcom,num-efuse-bits",
				&cpr_vreg->num_efuse_bits);
	if (rc < 0) {
		pr_err("num-efuse-bits missing: rc=%d\n", rc);
		return rc;
	}

	if (cpr_vreg->num_efuse_bits == 0 ||
	    cpr_vreg->num_efuse_bits > CPR_PVS_EFUSE_BITS_MAX) {
		pr_err("invalid num-efuse-bits : %d\n",
		       cpr_vreg->num_efuse_bits);
		return -EINVAL;
	}

	rc = of_property_read_u32_array(of_node, "qcom,efuse-bit-pos",
					cpr_vreg->efuse_bit_pos,
					cpr_vreg->num_efuse_bits);
	if (rc < 0) {
		pr_err("efuse-bit-pos missing: rc=%d\n", rc);
		return rc;
	}

	pvs_bins = 1 << cpr_vreg->num_efuse_bits;
	rc = of_property_read_u32_array(of_node, "qcom,pvs-bin-process",
					cpr_vreg->pvs_bin_process,
					pvs_bins);
	if (rc < 0) {
		pr_err("pvs-bin-process missing: rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,pvs-corner-ceiling-slow",
		&cpr_vreg->pvs_corner_ceiling[APC_PVS_SLOW][CPR_CORNER_SVS],
		CPR_CORNER_MAX - CPR_CORNER_SVS);
	if (rc < 0) {
		pr_err("pvs-corner-ceiling-slow missing: rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,pvs-corner-ceiling-nom",
		&cpr_vreg->pvs_corner_ceiling[APC_PVS_NOM][CPR_CORNER_SVS],
		CPR_CORNER_MAX - CPR_CORNER_SVS);
	if (rc < 0) {
		pr_err("pvs-corner-ceiling-norm missing: rc=%d\n", rc);
		return rc;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,pvs-corner-ceiling-fast",
		&cpr_vreg->pvs_corner_ceiling[APC_PVS_FAST][CPR_CORNER_SVS],
		CPR_CORNER_MAX - CPR_CORNER_SVS);
	if (rc < 0) {
		pr_err("pvs-corner-ceiling-fast missing: rc=%d\n", rc);
		return rc;
	}

	return 0;
}

static int __devinit cpr_regulator_probe(struct platform_device *pdev)
{
	struct cpr_regulator *cpr_vreg;
	struct regulator_desc *rdesc;
	struct regulator_init_data *init_data = pdev->dev.platform_data;
	int rc;

	if (!pdev->dev.of_node) {
		pr_err("Device tree node is missing\n");
		return -EINVAL;
	}

	init_data = of_get_regulator_init_data(&pdev->dev, pdev->dev.of_node);
	if (!init_data) {
		pr_err("regulator init data is missing\n");
		return -EINVAL;
	} else {
		init_data->constraints.input_uV
			= init_data->constraints.max_uV;
		init_data->constraints.valid_ops_mask
			|= REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS;
	}

	cpr_vreg = devm_kzalloc(&pdev->dev, sizeof(struct cpr_regulator),
				GFP_KERNEL);
	if (!cpr_vreg) {
		pr_err("Can't allocate cpr_regulator memory\n");
		return -ENOMEM;
	}

	rc = cpr_regulator_parse_dt(pdev, cpr_vreg);
	if (rc) {
		pr_err("Wrong DT parameter specified: rc=%d\n", rc);
		return rc;
	}

	rc = cpr_regulator_pvs_init(cpr_vreg);
	if (rc) {
		pr_err("Initialize PVS wrong: rc=%d\n", rc);
		return rc;
	}

	rc = cpr_regulator_apc_init(pdev, cpr_vreg);
	if (rc) {
		if (rc != -EPROBE_DEFER)
			pr_err("Initialize APC wrong: rc=%d\n", rc);
		return rc;
	}

	rdesc			= &cpr_vreg->rdesc;
	rdesc->owner		= THIS_MODULE;
	rdesc->type		= REGULATOR_VOLTAGE;
	rdesc->ops		= &cpr_corner_ops;
	rdesc->name		= init_data->constraints.name;

	cpr_vreg->rdev = regulator_register(rdesc, &pdev->dev, init_data,
					    cpr_vreg, pdev->dev.of_node);
	if (IS_ERR(cpr_vreg->rdev)) {
		rc = PTR_ERR(cpr_vreg->rdev);
		pr_err("regulator_register failed: rc=%d\n", rc);

		cpr_regulator_apc_exit(cpr_vreg);
		return rc;
	}

	platform_set_drvdata(pdev, cpr_vreg);

	pr_info("PVS [%d %d %d %d] uV\n",
		cpr_vreg->corner_ceiling[CPR_CORNER_SVS],
		cpr_vreg->corner_ceiling[CPR_CORNER_NORMAL],
		cpr_vreg->corner_ceiling[CPR_CORNER_TURBO],
		cpr_vreg->corner_ceiling[CPR_CORNER_SUPER_TURBO]);

	return 0;
}

static int __devexit cpr_regulator_remove(struct platform_device *pdev)
{
	struct cpr_regulator *cpr_vreg;

	cpr_vreg = platform_get_drvdata(pdev);
	if (cpr_vreg) {
		cpr_regulator_apc_exit(cpr_vreg);
		regulator_unregister(cpr_vreg->rdev);
	}

	return 0;
}

static struct of_device_id cpr_regulator_match_table[] = {
	{ .compatible = CPR_REGULATOR_DRIVER_NAME, },
	{}
};

static struct platform_driver cpr_regulator_driver = {
	.driver		= {
		.name	= CPR_REGULATOR_DRIVER_NAME,
		.of_match_table = cpr_regulator_match_table,
		.owner = THIS_MODULE,
	},
	.probe		= cpr_regulator_probe,
	.remove		= __devexit_p(cpr_regulator_remove),
};

/**
 * cpr_regulator_init() - register cpr-regulator driver
 *
 * This initialization function should be called in systems in which driver
 * registration ordering must be controlled precisely.
 */
int __init cpr_regulator_init(void)
{
	static bool initialized;

	if (initialized)
		return 0;
	else
		initialized = true;

	return platform_driver_register(&cpr_regulator_driver);
}
EXPORT_SYMBOL(cpr_regulator_init);

static void __exit cpr_regulator_exit(void)
{
	platform_driver_unregister(&cpr_regulator_driver);
}

MODULE_DESCRIPTION("CPR regulator driver");
MODULE_LICENSE("GPL v2");

arch_initcall(cpr_regulator_init);
module_exit(cpr_regulator_exit);
