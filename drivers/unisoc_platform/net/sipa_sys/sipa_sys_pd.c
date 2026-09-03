// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/clk.h>
#include <linux/debugfs.h>
#include <linux/device.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/kernel.h>
#include <linux/mfd/syscon.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/pm_domain.h>
#include <linux/power_supply.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/sipa.h>

#include "sipa_sys_pd.h"
#include "sipa_sys_phy/sipa_sys_phy_v3.h"
#include "sipa_sys_phy/sipa_sys_phy_v1.h"

#define DRV_NAME "sprd-sipa-sys"

static struct sipa_sys_pd_drv *ipa_sys_drv;

static int sipa_sys_clk_init(struct sipa_sys_pd_drv *drv)
{
	drv->ipa_core_clk = devm_clk_get(drv->dev, "ipa_core");
	if (IS_ERR(drv->ipa_core_clk)) {
		dev_warn(drv->dev, "sipa_sys can't get the IPA core clock\n");
		return PTR_ERR(drv->ipa_core_clk);
	}

	drv->ipa_core_parent = devm_clk_get(drv->dev, "ipa_core_source");
	if (IS_ERR(drv->ipa_core_parent)) {
		dev_warn(drv->dev, "sipa_sys can't get the ipa core source\n");
		return PTR_ERR(drv->ipa_core_parent);
	}

	drv->ipa_core_default = devm_clk_get(drv->dev, "ipa_core_default");
	if (IS_ERR(drv->ipa_core_default)) {
		dev_err(drv->dev, "sipa_sys can't get the ipa core default\n");
		return PTR_ERR(drv->ipa_core_default);
	}

	return 0;
}

static int sipa_sys_update_power_state(struct sipa_sys_pd_drv *drv)
{
	if (drv->pd_on)
		drv->do_power_on_cb(drv->cb_priv);
	else
		drv->do_power_off_cb(drv->cb_priv);

	return 0;
}

static int sipa_sys_pd_power_on(struct generic_pm_domain *domain)
{
	struct sipa_sys_pd_drv *drv;

	drv = container_of(domain, struct sipa_sys_pd_drv, gpd);

	drv->pd_on = true;
	sipa_sys_update_power_state(drv);
	dev_dbg(drv->dev, "pd power on\n");
	return 0;
}

static int sipa_sys_pd_power_off(struct generic_pm_domain *domain)
{
	struct sipa_sys_pd_drv *drv;

	drv = container_of(domain, struct sipa_sys_pd_drv, gpd);

	drv->pd_on = false;
	sipa_sys_update_power_state(drv);
	dev_dbg(drv->dev, "pd power off\n");
	return 0;
}

static int sipa_sys_register_power_domain(struct sipa_sys_pd_drv *drv)
{
	int ret;

	drv->gpd.name = kstrdup(drv->dev->of_node->name, GFP_KERNEL);
	if (!drv->gpd.name)
		return -ENOMEM;

	drv->gpd.power_off = sipa_sys_pd_power_off;
	drv->gpd.power_on = sipa_sys_pd_power_on;

	ret = pm_genpd_init(&drv->gpd, NULL, true);
	if (ret) {
		dev_warn(drv->dev, "sipa sys pm_genpd_init fail!");
		kfree(drv->gpd.name);
		return ret;
	}

	ret = of_genpd_add_provider_simple(drv->dev->of_node, &drv->gpd);
	if (ret) {
		dev_warn(drv->dev, "sipa sys genpd_add_provider fail!");
		pm_genpd_remove(&drv->gpd);
		kfree(drv->gpd.name);
	}

	return ret;
}

#ifdef CONFIG_DEBUG_FS
static int sipa_sys_debug_show(struct seq_file *s, void *unused)
{
	struct pm_domain_data *pdd;
	struct sipa_sys_pd_drv *drv = (struct sipa_sys_pd_drv *)s->private;

	seq_puts(s, "status:\n");
	seq_puts(s, "\t0: active 1: power off\n\n");
	seq_puts(s, "power status:\n");
	seq_puts(s, "\t0: active 1: resuming 2: suspended 3: suspending\n\n");

	seq_printf(s, "pd_on = %d sd_count = %d status = %d\n",
		   drv->pd_on,
		   atomic_read(&drv->gpd.sd_count),
		   drv->gpd.status);

	seq_printf(s, "device_count = %d suspended_count = %d prepared_count = %d\n",
		   drv->gpd.device_count, drv->gpd.suspended_count,
		   drv->gpd.prepared_count);

	list_for_each_entry(pdd, &drv->gpd.dev_list, list_node)
		seq_printf(s,
			   "%s power status = %d, disable_depth = %d usage_cnt = %d\n",
			   dev_name(pdd->dev), pdd->dev->power.runtime_status,
			   pdd->dev->power.disable_depth,
			   atomic_read(&pdd->dev->power.usage_count));

	debugfs_create_symlink("sprd-sipa-sys", NULL,
			       "/sys/kernel/debug/sprd-sipa-sys/power_domain");

	return 0;
}

static int sipa_sys_debug_open(struct inode *inode, struct file *file)
{
	return single_open(file, sipa_sys_debug_show, inode->i_private);
}

static const struct file_operations sipa_sys_debug_fops = {
	.open = sipa_sys_debug_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};

static int sipa_sys_init_debugfs(struct platform_device *pdev)
{
	struct dentry *root;
	struct sipa_sys_pd_drv *drv = platform_get_drvdata(pdev);

	root = debugfs_create_dir(DRV_NAME, NULL);
	if (!root)
		return -ENOMEM;

	debugfs_create_file("power_domain", 0444, root, drv,
			    &sipa_sys_debug_fops);

	return 0;
}
#endif

static void sipa_sys_register_cb(struct sipa_sys_pd_drv *drv,
				 sipa_sys_parse_dts_cb parse_dts_cb,
				 sipa_sys_init_cb init_cb,
				 sipa_sys_do_power_on_cb do_power_on_cb,
				 sipa_sys_do_power_off_cb do_power_off_cb,
				 sipa_sys_clk_enable_cb clk_enable_cb,
				 void *priv)
{
	drv->parse_dts_cb = parse_dts_cb;
	drv->init_cb = init_cb;
	drv->do_power_on_cb = do_power_on_cb;
	drv->do_power_off_cb = do_power_off_cb;
	drv->clk_enable_cb = clk_enable_cb;
	drv->cb_priv = priv;
}

static int sipa_sys_drv_probe(struct platform_device *pdev_p)
{
	int ret = 0;
	u32 num = 0;
	struct sipa_sys_pd_drv *drv;
	const struct sipa_sys_data *data;

	data = of_device_get_match_data(&pdev_p->dev);
	ret = of_property_read_u32(pdev_p->dev.of_node, "reg-size", &num);
	if (ret)
		return ret;

	drv = devm_kzalloc(&pdev_p->dev,
			   sizeof(*drv) +
			   sizeof(struct sipa_sys_register) * num,
			   GFP_KERNEL);
	if (!drv)
		return -ENOMEM;
	ipa_sys_drv = drv;

	platform_set_drvdata(pdev_p, drv);
	drv->dev = &pdev_p->dev;

	if (data->version == 1) {
		sipa_sys_register_cb(drv,
				     sipa_sys_parse_dts_cb_v1,
				     sipa_sys_init_cb_v1,
				     sipa_sys_do_power_on_cb_v1,
				     sipa_sys_do_power_off_cb_v1,
				     sipa_sys_clk_enable_cb_v1,
				     (void *)drv);
	} else if (data->version == 3) {
		sipa_sys_register_cb(drv,
				     sipa_sys_parse_dts_cb_v3,
				     sipa_sys_init_cb_v3,
				     sipa_sys_do_power_on_cb_v3,
				     sipa_sys_do_power_off_cb_v3,
				     sipa_sys_clk_enable_cb_v3,
				     (void *)drv);
	}

	ret = sipa_sys_clk_init(drv);
	if (ret)
		return ret;

	ret = drv->clk_enable_cb(drv->cb_priv);
	if (ret)
		return ret;

	ret = drv->parse_dts_cb(drv->cb_priv);
	if (ret)
		return ret;

	ret = sipa_sys_register_power_domain(drv);
	if (ret)
		dev_warn(drv->dev, "sipa sys reg power domain fail\n");

#ifdef CONFIG_DEBUG_FS
	ret = sipa_sys_init_debugfs(pdev_p);
	if (ret)
		dev_warn(drv->dev, "sipa_sys init debug fs fail\n");
#endif
	drv->init_cb(drv->cb_priv);

	return 0;
}

static struct sipa_sys_data sipa_sys_n6pro_data = {
	.version = 3,
};

static struct sipa_sys_data sipa_sys_roc1_data = {
	.version = 1,
};

static const struct of_device_id sipa_sys_drv_match[] = {
	{ .compatible = "sprd,roc1-ipa-sys-power-domain",
		.data = &sipa_sys_roc1_data },
	{ .compatible = "sprd,qogirn6pro-ipa-sys-power-domain",
		.data = &sipa_sys_n6pro_data },
	{}
};

static struct platform_driver sipa_sys_drv = {
	.probe = sipa_sys_drv_probe,
	.driver = {
		.name = DRV_NAME,
		.of_match_table = sipa_sys_drv_match,
	},
};

module_platform_driver(sipa_sys_drv);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Spreadtrum sipa sys power domain device driver");
