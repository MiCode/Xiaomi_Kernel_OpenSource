// SPDX-License-Identifier: GPL-2.0
// Spreadtrum Generic power domain support.
//
// Copyright (C) 2018 Spreadtrum Communications Inc.
//
// This software is licensed under the terms of the GNU General Public
// License version 2, as published by the Free Software Foundation, and
// may be copied, distributed, and modified under those terms.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/regmap.h>
#include <linux/slab.h>
#include <linux/types.h>

#ifdef pr_fmt
#undef pr_fmt
#define pr_fmt(__fmt) "[disp-pm-domain][drm][%20s] "__fmt, __func__
#endif

struct disp_pm_domain {
	unsigned int ctrl_reg;
	unsigned int ctrl_mask;

	struct generic_pm_domain pd;
	struct regmap *regmap;
};

static int sprd_disp_power_on(struct generic_pm_domain *domain)
{
	struct disp_pm_domain *pd;

	pd = container_of(domain, struct disp_pm_domain, pd);

	regmap_update_bits(pd->regmap,
		    pd->ctrl_reg,
		    pd->ctrl_mask,
		    (unsigned int)~pd->ctrl_mask);

	mdelay(10);

	pr_info("disp power domain on\n");
	return 0;
}

static int sprd_disp_power_off(struct generic_pm_domain *domain)
{
	struct disp_pm_domain *pd;

	pd = container_of(domain, struct disp_pm_domain, pd);

	mdelay(10);

	regmap_update_bits(pd->regmap,
		    pd->ctrl_reg,
		    pd->ctrl_mask,
		    pd->ctrl_mask);

	pr_info("disp power domain off\n");
	return 0;
}

static bool cali_mode_check(const char *str)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	int rc;

	cmdline_node = of_find_node_by_path("/chosen");
	rc = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (rc)
		return false;

	if (!strstr(cmd_line, str))
		return false;

	return true;
}

static int sprd_disp_pm_domain_probe(struct platform_device *pdev)
{
	struct disp_pm_domain *pd;
	struct device_node *np = pdev->dev.of_node;
	unsigned int syscon_args[2];
	bool cali_mode;

	pd = devm_kzalloc(&pdev->dev, sizeof(*pd), GFP_KERNEL);
	if (!pd)
		return -ENOMEM;

	pd->regmap = syscon_regmap_lookup_by_phandle_args(np,
			"disp-power", 2, syscon_args);
	if (IS_ERR(pd->regmap)) {
		pr_err("failed to map glb reg\n");
		goto err;
	} else {
		pd->ctrl_reg = syscon_args[0];
		pd->ctrl_mask = syscon_args[1];
	}

	/* Workaround:
	 * When enter Cali mode, need to power off the disp manually.
	 */
	cali_mode = cali_mode_check("sprdboot.mode=cali");
	if (cali_mode) {
		regmap_update_bits(pd->regmap,
		    pd->ctrl_reg,
		    pd->ctrl_mask,
		    pd->ctrl_mask);

		pr_info("Calibration Mode! disp power domain off\n");
	}

	pd->pd.name = kstrdup(np->name, GFP_KERNEL);
	pd->pd.power_off = sprd_disp_power_off;
	pd->pd.power_on = sprd_disp_power_on;

	pm_genpd_init(&pd->pd, NULL, true);
	of_genpd_add_provider_simple(np, &pd->pd);

	pr_info("display power domain init ok!\n");

	return 0;
err:
	kfree(pd);
	return -EINVAL;
}

static int sprd_disp_pm_domain_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct of_device_id sprd_disp_pm_domain_of_match[] = {
	{ .compatible = "sprd,sharkl3-disp-domain", },
	{ },
};

static struct platform_driver disp_pm_domain_driver = {
	.probe = sprd_disp_pm_domain_probe,
	.remove = sprd_disp_pm_domain_remove,
	.driver = {
		.name   = "sprd-disp-pm-domain",
		.of_match_table = sprd_disp_pm_domain_of_match,
	},
};

module_platform_driver(disp_pm_domain_driver);

MODULE_LICENSE("GPL v2");
MODULE_AUTHOR("leon.he@spreadtrum.com");
MODULE_DESCRIPTION("sprd sharkl3 display pm generic domain");
