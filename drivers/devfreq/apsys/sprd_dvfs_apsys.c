// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt

#include <linux/devfreq.h>
#include <linux/devfreq-event.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sprd_soc_id.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#include "../governor.h"
#include "sprd_dvfs_apsys.h"

#define to_apsys(DEV)	container_of((DEV), struct apsys_dev, dev)

LIST_HEAD(apsys_dvfs_head);

bool get_display_power_status(void)
{
	struct device_node *dpu_node;
	struct platform_device *dpu_pdev;
	struct sprd_dpu_crtc *sprd_dpu;

	dpu_node = of_find_node_by_name(NULL, "dpu");
	if (!dpu_node) {
		pr_err("failed to get dpu node\n");
		return false;
	}

	dpu_pdev = of_find_device_by_node(dpu_node);
	if (!dpu_pdev) {
		pr_err("failed to get dpu platform device\n");
		of_node_put(dpu_node);
		return false;
	}

	sprd_dpu = dev_get_drvdata(&dpu_pdev->dev);
	if (!sprd_dpu->crtc->state->active) {
		pr_err("dpu is stopped, reject get dpu dvfs status\n");
		of_node_put(dpu_node);
		platform_device_put(dpu_pdev);
		return false;
	}

	of_node_put(dpu_node);
	platform_device_put(dpu_pdev);

	return true;
}

struct class *dvfs_class;
struct regmap *regmap_aon_base;
bool n6pro_AA_flag;

int dpu_vsp_dvfs_check_clkeb(void)
{
	u32  dpu_vsp_dvfs_eb_reg = 0;

	regmap_read(regmap_aon_base, 0x0, &dpu_vsp_dvfs_eb_reg);

	if (dpu_vsp_dvfs_eb_reg&BIT(21))
		return 0;
	else
		return -1;
}

int n6pro_soc_ver_id_check(void)
{
	int ret;
	u32 ver_id;

	ret = sprd_get_soc_id(AON_VER_ID, &ver_id, 1);
	if (ret) {
		pr_err("fail to get soc id\n");
		return 0;
	}
	if (ver_id == 0)
		pr_info("n6pro soc is AA\n");
	else if (ver_id == 1)
		pr_info("n6pro soc is AB\n");
	else {
		pr_info("unknowned soc\n");
		ver_id = 0;
	}

	return ver_id;
}

struct apsys_dev *find_apsys_device_by_name(char *name)
{
	struct device_node *np = NULL;
	struct platform_device *pdev = NULL;
	struct apsys_dev *apsys = NULL;

	np = of_find_node_by_name(NULL, name);
	if (np) {
		pdev = of_find_device_by_node(np);

		if (pdev)
			apsys = platform_get_drvdata(pdev);
		else
			pr_err("cannot find platform device by node with name :%s\n", name);
	} else {
		pr_err("cannot find node by name :%s\n", name);
		return NULL;
	}

	if (apsys) {
		pr_info("find platform device by node with name :%s, address:%lx\n",
					name, apsys->apsys_base);
	} else {
		pr_err("find apsys device failed, apsys is NULL!\n");
		dump_stack();
	}

	return apsys;
}

static ssize_t top_cur_volt_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct apsys_dev *apsys = to_apsys(dev);
	int ret, cur_volt;

	if (!get_display_power_status())
		return -EINVAL;

	if (apsys->dvfs_ops && apsys->dvfs_ops->top_cur_volt) {
		cur_volt = apsys->dvfs_ops->top_cur_volt(apsys);
	} else {
		pr_info("%s: apsys ops null\n", __func__);
		cur_volt = -EINVAL;
	}

	if (cur_volt == 0)
		ret = sprintf(buf, "0.7v\n");
	else if (cur_volt == 1)
		ret = sprintf(buf, "0.75v\n");
	else if (cur_volt == 2)
		ret = sprintf(buf, "0.8v\n");
	else
		ret = sprintf(buf, "undefined\n");

	return ret;
}

static ssize_t apsys_hold_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct apsys_dev *apsys = to_apsys(dev);
	int ret, hold_en;

	if (!get_display_power_status())
		return -EINVAL;

	ret = sscanf(buf, "%d\n", &hold_en);
	if (ret == 0)
		return -EINVAL;

	if (apsys->dvfs_ops && apsys->dvfs_ops->apsys_hold_en)
		apsys->dvfs_ops->apsys_hold_en(apsys, hold_en);
	else
		pr_info("%s: apsys ops null\n", __func__);

	return count;
}

static ssize_t apsys_force_en_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct apsys_dev *apsys = to_apsys(dev);
	int ret, force_en;

	if (!get_display_power_status())
		return -EINVAL;

	ret = sscanf(buf, "%d\n", &force_en);
	if (ret == 0)
		return -EINVAL;

	if (apsys->dvfs_ops && apsys->dvfs_ops->apsys_force_en)
		apsys->dvfs_ops->apsys_force_en(apsys, force_en);
	else
		pr_info("%s: apsys ops null\n", __func__);

	return count;
}

static ssize_t apsys_auto_gate_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct apsys_dev *apsys = to_apsys(dev);
	int ret, gate_sel;

	if (!get_display_power_status())
		return -EINVAL;

	ret = sscanf(buf, "%d\n", &gate_sel);
	if (ret == 0)
		return -EINVAL;

	if (apsys->dvfs_ops && apsys->dvfs_ops->apsys_auto_gate)
		apsys->dvfs_ops->apsys_auto_gate(apsys, gate_sel);
	else
		pr_info("%s: apsys ops null\n", __func__);

	return count;
}

static ssize_t apsys_wait_window_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct apsys_dev *apsys = to_apsys(dev);
	int ret, wait_window;

	if (!get_display_power_status())
		return -EINVAL;

	ret = sscanf(buf, "%d\n", &wait_window);
	if (ret == 0)
		return -EINVAL;

	if (apsys->dvfs_ops && apsys->dvfs_ops->apsys_wait_window)
		apsys->dvfs_ops->apsys_wait_window(apsys, wait_window);
	else
		pr_info("%s: apsys ops null\n", __func__);

	return count;
}

static ssize_t apsys_min_volt_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct apsys_dev *apsys = to_apsys(dev);
	int ret, min_volt;

	if (!get_display_power_status())
		return -EINVAL;

	ret = sscanf(buf, "%d\n", &min_volt);
	if (ret == 0)
		return -EINVAL;

	if (apsys->dvfs_ops && apsys->dvfs_ops->apsys_min_volt)
		apsys->dvfs_ops->apsys_min_volt(apsys, min_volt);
	else
		pr_info("%s: apsys ops null\n", __func__);

	return count;
}

static DEVICE_ATTR(cur_volt, 0444, top_cur_volt_show, NULL);
static DEVICE_ATTR(hold_en, 0200, NULL, apsys_hold_en_store);
static DEVICE_ATTR(force_en, 0200, NULL, apsys_force_en_store);
static DEVICE_ATTR(auto_gate, 0200, NULL, apsys_auto_gate_store);
static DEVICE_ATTR(wait_window, 0200, NULL, apsys_wait_window_store);
static DEVICE_ATTR(min_volt, 0200, NULL, apsys_min_volt_store);

static struct attribute *apsys_attrs[] = {
	&dev_attr_cur_volt.attr,
	&dev_attr_hold_en.attr,
	&dev_attr_force_en.attr,
	&dev_attr_auto_gate.attr,
	&dev_attr_wait_window.attr,
	&dev_attr_min_volt.attr,
	NULL,
};

static const struct attribute_group apsys_group = {
	.attrs = apsys_attrs,
};

static __maybe_unused int apsys_dvfs_suspend(struct device *dev)
{
	pr_info("%s()\n", __func__);

	return 0;
}

static __maybe_unused int apsys_dvfs_resume(struct device *dev)
{
	struct apsys_dev *apsys = dev_get_drvdata(dev);

	pr_info("%s()\n", __func__);

	if (apsys->dvfs_ops && apsys->dvfs_ops->dvfs_init)
		apsys->dvfs_ops->dvfs_init(apsys);

	return 0;
}

static SIMPLE_DEV_PM_OPS(apsys_dvfs_pm, apsys_dvfs_suspend,
			 apsys_dvfs_resume);

static int apsys_dvfs_class_init(void)
{
	pr_info("apsys dvfs class init\n");

	dvfs_class = class_create(THIS_MODULE, "dvfs");
	if (IS_ERR(dvfs_class)) {
		pr_err("Unable to create apsys dvfs class\n");
		return PTR_ERR(dvfs_class);
	}

	return 0;
}

static int apsys_dvfs_device_create(struct apsys_dev *apsys,
				struct device *parent)
{
	int ret;

	apsys->dev.class = dvfs_class;
	apsys->dev.parent = parent;
	apsys->dev.of_node = parent->of_node;
	dev_set_name(&apsys->dev, "apsys");
	dev_set_drvdata(&apsys->dev, apsys);

	ret = device_register(&apsys->dev);
	if (ret)
		pr_err("apsys dvfs device register failed\n");

	return ret;
}

static int apsys_dvfs_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	const struct sprd_apsys_dvfs_ops *pdata;
	struct apsys_dev *apsys;
	void __iomem *base;
	struct resource r;
	int ret;

	apsys = devm_kzalloc(dev, sizeof(*apsys), GFP_KERNEL);
	if (!apsys)
		return -ENOMEM;

	pdata = of_device_get_match_data(&pdev->dev);
	if (pdata) {
		apsys->dvfs_ops = pdata->apsys_ops;
	} else {
		pr_err("No matching driver data found\n");
		return -EINVAL;
	}

	if (!strcmp("qogirn6pro", pdata->version)) {
		if (!n6pro_soc_ver_id_check()) {
			pr_err("apsys : %s soc is AA,bypass\n", pdata->version);
			n6pro_AA_flag = true;
			return -EINVAL;
		}
	}

	if (of_address_to_resource(np, 0, &r)) {
		pr_err("parse apsys base address failed\n");
		return -ENODEV;
	}

	base = ioremap(r.start, resource_size(&r));
	if (IS_ERR(base)) {
		pr_err("ioremap apsys dvfs address failed\n");
		return -EFAULT;
	}
	apsys->apsys_base = (unsigned long)base;

	apsys_dvfs_class_init();
	apsys_dvfs_device_create(apsys, dev);

	ret = sysfs_create_group(&(apsys->dev.kobj), &apsys_group);
	if (ret) {
		dev_err(dev, "apsys create sysfs class failed, ret=%d\n", ret);
		return ret;
	}

	platform_set_drvdata(pdev, apsys);

	if (apsys->dvfs_ops && apsys->dvfs_ops->parse_dt)
		apsys->dvfs_ops->parse_dt(apsys, np);

	if (apsys->dvfs_ops && apsys->dvfs_ops->dvfs_init)
		apsys->dvfs_ops->dvfs_init(apsys);

	if (apsys->dvfs_ops && apsys->dvfs_ops->top_dvfs_init)
		apsys->dvfs_ops->top_dvfs_init(apsys);
	pr_info("apsys module registered\n");

	return 0;
}
static int apsys_dvfs_remove(struct platform_device *pdev)
{
	return 0;
}

static const struct sprd_apsys_dvfs_ops qogirl6_apsys_ops = {
	.apsys_ops = &qogirl6_apsys_dvfs_ops,
	.version = "qogirl6",
};

static const struct sprd_apsys_dvfs_ops roc1_apsys_ops = {
	.apsys_ops = &roc1_apsys_dvfs_ops,
	.version = "roc1",
};

static const struct sprd_apsys_dvfs_ops sharkl5pro_apsys_ops = {
	.apsys_ops = &sharkl5pro_apsys_dvfs_ops,
	.version = "sharkl5pro",
};

static const struct sprd_apsys_dvfs_ops sharkl5_apsys_ops = {
	.apsys_ops = &sharkl5_apsys_dvfs_ops,
	.version = "sharkl5",
};

static const struct sprd_apsys_dvfs_ops qogirn6pro_apsys_ops = {
	.apsys_ops = &qogirn6pro_apsys_dvfs_ops,
	.version = "qogirn6pro",
};

static const struct sprd_apsys_dvfs_ops qogirn6lite_apsys_ops = {
	.apsys_ops = &qogirn6lite_apsys_dvfs_ops,
	.version = "qogirn6lite",
};
/*
static const struct sprd_apsys_dvfs_ops sharkl5pro_apsys_ops = {
	.apsys_ops = &sharkl5pro_apsys_dvfs_ops,
};
*/

static const struct of_device_id apsys_dvfs_of_match[] = {
	{ .compatible = "sprd,hwdvfs-apsys-sharkl5",
	  .data = &sharkl5_apsys_ops },
	{ .compatible = "sprd,hwdvfs-apsys-roc1",
	  .data = &roc1_apsys_ops },
	{ .compatible = "sprd,hwdvfs-apsys-sharkl5pro",
	  .data = &sharkl5pro_apsys_ops },
	{ .compatible = "sprd,hwdvfs-apsys-qogirl6",
	  .data = &qogirl6_apsys_ops },
	{ .compatible = "sprd,hwdvfs-dpuvsp-qogirn6pro",
	  .data = &qogirn6pro_apsys_ops },
	{ .compatible = "sprd,hwdvfs-dpuvsp-qogirn6lite",
	  .data = &qogirn6lite_apsys_ops },
	{ },
};

MODULE_DEVICE_TABLE(of, apsys_dvfs_of_match);

static struct platform_driver apsys_dvfs_driver = {
	.probe	= apsys_dvfs_probe,
	.remove	= apsys_dvfs_remove,
	.driver = {
		.name = "apsys-dvfs",
		.pm	= &apsys_dvfs_pm,
		.of_match_table = apsys_dvfs_of_match,
	},
};

static struct platform_driver *sprd_apsys_dvfs_drivers[]  = {
	&dpu_dvfs_driver,
#ifdef CONFIG_DRM_SPRD_GSP_DVFS
	&gsp_dvfs_driver,
#endif
#ifdef CONFIG_DVFS_APSYS_VDSP
	&vdsp_dvfs_driver,
#endif
	&vsp_dvfs_driver,
};

static struct devfreq_governor *sprd_apsys_dvfs_governors[]  = {
	&dpu_devfreq_gov,
#ifdef CONFIG_DRM_SPRD_GSP_DVFS
	&gsp_devfreq_gov,
#endif
#ifdef CONFIG_DVFS_APSYS_VDSP
	&vdsp_devfreq_gov,
#endif
	&vsp_devfreq_gov,
};

static int __init apsys_dvfs_register(void)
{
	int i, ret;

	ret = platform_driver_register(&apsys_dvfs_driver);
	if (n6pro_AA_flag) {
		pr_warn("%s: n6pro aa does not need dvfs, skip other probe\n", __func__);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(sprd_apsys_dvfs_drivers); i++) {
		ret = devfreq_add_governor(sprd_apsys_dvfs_governors[i]);
		if (ret) {
			pr_err("%s: failed to add governor: %d\n", __func__, ret);
			return ret;
		}

		ret = platform_driver_register(sprd_apsys_dvfs_drivers[i]);
		if (ret) {
			devfreq_remove_governor(sprd_apsys_dvfs_governors[i]);
			pr_err("%s: remove governor:%d\n", __func__, i);
		}
	}

	return ret;
}

static void __exit apsys_dvfs_unregister(void)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(sprd_apsys_dvfs_drivers); i++) {
		platform_driver_unregister(sprd_apsys_dvfs_drivers[i]);

		devfreq_remove_governor(sprd_apsys_dvfs_governors[i]);
	}

	platform_driver_unregister(&apsys_dvfs_driver);
}

subsys_initcall(apsys_dvfs_register);
module_exit(apsys_dvfs_unregister);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("sprd apsys dvfs driver");
MODULE_AUTHOR("Kevin Tang <kevin.tang@unisoc.com>");
