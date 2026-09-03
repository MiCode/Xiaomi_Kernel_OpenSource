//SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2020 UNISOC Communications Inc.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/regmap.h>
#include <linux/platform_device.h>
#include "../base/base.h"

#define SC2721_GLB_FREE_TIMER_LOW	0x328
#define SC2721_GLB_FREE_TIMER_HIGH	0x32c
#define SC2730_GLB_FREE_TIMER_LOW	0x408
#define SC2730_GLB_FREE_TIMER_HIGH	0x40c
#define UMP9620_GLB_FREE_TIMER_LOW	0x408
#define UMP9620_GLB_FREE_TIMER_HIGH	0x40c

struct sprd_pmic_ft_data {
	const char	*compatible;
	u32		freetimer_low;
	u32		freetimer_high;
};

struct pmic_glb {
	u32 reg;
	u32 base;
	struct kobject *kobj;
	struct regmap *regmap;
	struct device *dev;
	struct sprd_pmic_ft_data *ft_data;
	struct list_head list;
};

static DEFINE_SPINLOCK(sc27xx_head_slock);
static LIST_HEAD(sc27xx_head);
static struct platform_driver sprd_pmic_glb_driver;

static const struct sprd_pmic_ft_data ft_data[] = {
	{ .compatible = "sprd,sc2721",
	  .freetimer_low = SC2721_GLB_FREE_TIMER_LOW,
	  .freetimer_high = SC2721_GLB_FREE_TIMER_HIGH},
	{ .compatible = "sprd,sc2730",
	  .freetimer_low = SC2730_GLB_FREE_TIMER_LOW,
	  .freetimer_high = SC2730_GLB_FREE_TIMER_HIGH},
	{ .compatible = "sprd,ump9620",
	  .freetimer_low = UMP9620_GLB_FREE_TIMER_LOW,
	  .freetimer_high = UMP9620_GLB_FREE_TIMER_HIGH},
	{},
};

int sprd_pmic_power_boot_time(u32 *count)
{
	struct pmic_glb *sc27xx_glb;
	unsigned int val_low;
	unsigned int val_high;
	int ret = -EINVAL;
	bool find_ft = false;

	spin_lock(&sc27xx_head_slock);
	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		if (sc27xx_glb->ft_data == NULL)
			continue;
		if (sc27xx_glb->ft_data->compatible) {
			find_ft = true;
			break;
		}
	}
	spin_unlock(&sc27xx_head_slock);

	if (find_ft == true) {
		ret = regmap_read(sc27xx_glb->regmap,
				  sc27xx_glb->base + sc27xx_glb->ft_data->freetimer_low,
				  &val_low);
		if (ret == 0) {
			ret = regmap_read(sc27xx_glb->regmap,
					  sc27xx_glb->base + sc27xx_glb->ft_data->freetimer_high,
					  &val_high);
			if (ret)
				pr_err("%s: failed to get freetimer high\n", __func__);
			else
				*count = (val_high << 16) | (val_low & 0xffff);
		} else {
			pr_err("%s: failed to get freetimer low\n", __func__);
		}
	}

	return ret;
}
EXPORT_SYMBOL_GPL(sprd_pmic_power_boot_time);

static ssize_t pmic_freetimer_show(struct kobject *kobj, struct kobj_attribute *attr,
				   char *buf)
{
	u32 count;
	ssize_t ret = -EINVAL;

	ret = sprd_pmic_power_boot_time(&count);
	if (ret)
		ret = snprintf(buf, PAGE_SIZE, "%s power boot time error(%ld)\n", __func__, ret);
	else
		ret = snprintf(buf, PAGE_SIZE, "[power_boot_time][%d]s\n", count);

	return ret;
}

static ssize_t pmic_reg_show(struct kobject *kobj, struct kobj_attribute *attr,
			     char *buf)
{
	ssize_t ret = -EINVAL;
	struct pmic_glb *sc27xx_glb;

	spin_lock(&sc27xx_head_slock);
	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		if (sc27xx_glb->kobj == kobj) {
			ret = snprintf(buf, 10, "0x%x", sc27xx_glb->reg);
			break;
		}
	}
	spin_unlock(&sc27xx_head_slock);

	return ret;
}

static ssize_t pmic_reg_store(struct kobject *kobj, struct kobj_attribute *attr,
			      const char *buf, size_t count)
{
	int ret = -EINVAL;
	struct pmic_glb *sc27xx_glb;

	spin_lock(&sc27xx_head_slock);
	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		if (sc27xx_glb->kobj == kobj) {
			ret = kstrtouint(buf, 16, &sc27xx_glb->reg);
			if (!ret)
				ret = strnlen(buf, count);
			break;
		}
	}
	spin_unlock(&sc27xx_head_slock);

	return ret;
}

static ssize_t pmic_value_show(struct kobject *kobj, struct kobj_attribute
			       *attr, char *buf)
{
	int ret = -EINVAL;
	u32 value;
	struct pmic_glb *sc27xx_glb = NULL;

	spin_lock(&sc27xx_head_slock);
	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		if (sc27xx_glb->kobj == kobj) {
			break;
		}
	}
	spin_unlock(&sc27xx_head_slock);

	if (sc27xx_glb->kobj != NULL && sc27xx_glb->reg > sc27xx_glb->base) {
		ret = regmap_read(sc27xx_glb->regmap, sc27xx_glb->reg, &value);
		if (!ret)
			ret = snprintf(buf, 10, "0x%x", value);
	}

	return ret;
}

static ssize_t pmic_value_store(struct kobject *kobj, struct kobj_attribute
				*attr, const char *buf, size_t count)
{
	int ret = -EINVAL;
	u32 value;
	struct pmic_glb *sc27xx_glb = NULL;

	spin_lock(&sc27xx_head_slock);
	list_for_each_entry(sc27xx_glb, &sc27xx_head, list) {
		if (sc27xx_glb->kobj == kobj) {
			break;
		}
	}
	spin_unlock(&sc27xx_head_slock);

	if (sc27xx_glb->kobj != NULL && sc27xx_glb->reg > sc27xx_glb->base) {
		ret = kstrtouint(buf, 16, &value);
		if (!ret)
			ret = regmap_write(sc27xx_glb->regmap, sc27xx_glb->reg, value);
		if (!ret)
			ret = count;
	}

	return ret;
}

static struct kobj_attribute pmic_reg_attr =
__ATTR(pmic_reg, 0644, pmic_reg_show, pmic_reg_store);
static struct kobj_attribute pmic_value_attr =
__ATTR(pmic_value, 0644, pmic_value_show, pmic_value_store);
static struct kobj_attribute pmic_freetimer_attr =
__ATTR(pmic_freetimer, 0444, pmic_freetimer_show, NULL);

static struct attribute *pmic_syscon_attrs[] = {
	&pmic_reg_attr.attr,
	&pmic_value_attr.attr,
	&pmic_freetimer_attr.attr,
	NULL
};

static const struct attribute_group pmic_syscon_group = {
	.attrs = pmic_syscon_attrs,
};

static int sprd_pmic_glb_probe(struct platform_device *pdev)
{
	int i, ret;
	struct pmic_glb *sc27xx_glb;
	struct kobject *sprd_pmic_glb_kobj;
	struct device *dev = &pdev->dev;
	struct device_node *np = pdev->dev.of_node;
	struct device_driver *drv = &sprd_pmic_glb_driver.driver;
	const struct of_device_id *match;

	sc27xx_glb = devm_kzalloc(dev, sizeof(struct pmic_glb), GFP_KERNEL);
	if (!sc27xx_glb)
		return -ENOMEM;

	sc27xx_glb->regmap = dev_get_regmap(pdev->dev.parent, NULL);
	if (!sc27xx_glb->regmap) {
		dev_err(dev, "get regmap fail\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(np, "reg", 0, &sc27xx_glb->base);
	if (ret) {
		dev_err(dev, "get base register failed\n");
		return -EINVAL;
	}

	sc27xx_glb->dev = &pdev->dev;

	for (i = 0; i < sizeof(ft_data) / sizeof(struct sprd_pmic_ft_data); i++) {
		if (of_device_is_compatible(dev->parent->of_node, ft_data[i].compatible)) {
			sc27xx_glb->ft_data = (struct sprd_pmic_ft_data *)&ft_data[i];
			dev_info(dev, "pmic dev[%d] is (%s)\n", i, ft_data[i].compatible);
			break;
		}
	}

	match = of_match_device(pdev->dev.driver->of_match_table, dev);
	if (!match)
		return -EINVAL;

	sprd_pmic_glb_kobj = kobject_create_and_add(match->compatible, &drv->p->kobj);
	if (sprd_pmic_glb_kobj == NULL) {
		dev_err(dev, "failed to create sprd_pmic_glb_kobj.\n");
		return -ENOMEM;
	}
	ret = sysfs_create_group(sprd_pmic_glb_kobj, &pmic_syscon_group);
	if (ret) {
		dev_err(dev, "failed to create pmic_syscon attributes\n");
		kobject_put(sprd_pmic_glb_kobj);
		return ret;
	}

	sc27xx_glb->kobj = sprd_pmic_glb_kobj;

	spin_lock(&sc27xx_head_slock);
	list_add(&sc27xx_glb->list, &sc27xx_head);
	spin_unlock(&sc27xx_head_slock);

	dev_set_drvdata(dev, sc27xx_glb);

	return 0;
}

static int sprd_pmic_glb_remove(struct platform_device *pdev)
{
	struct pmic_glb *sc27xx_glb = dev_get_drvdata(&pdev->dev);

	spin_lock(&sc27xx_head_slock);
	list_del(&sc27xx_glb->list);
	spin_unlock(&sc27xx_head_slock);
	sysfs_remove_group(sc27xx_glb->kobj, &pmic_syscon_group);
	kobject_put(sc27xx_glb->kobj);

	return 0;
}
static const struct of_device_id sprd_pmic_glb_match[] = {
	{ .compatible = "sprd,sc27xx-syscon"},
	{ .compatible = "sprd,uip8520-syscon"},
	{ .compatible = "sprd,ump962x-syscon"},
	{ .compatible = "sprd,ump9621-syscon"},
	{ .compatible = "sprd,ump9622-syscon"},
	{},
};
MODULE_DEVICE_TABLE(of, sprd_pmic_glb_match);

static struct platform_driver sprd_pmic_glb_driver = {
	.probe = sprd_pmic_glb_probe,
	.remove = sprd_pmic_glb_remove,
	.driver = {
		.name = "sprd-pmic-glb",
		.of_match_table = sprd_pmic_glb_match,
	},
};

module_platform_driver(sprd_pmic_glb_driver);

MODULE_AUTHOR("Luyao Wu <luyao.wu@unisoc.com>");
MODULE_DESCRIPTION("UNISOC pmic glob driver");
MODULE_LICENSE("GPL v2");
