/*
 * Copyright (c) 2014, The Linux Foundation. All rights reserved.
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

#define pr_fmt(fmt) "msmclock: %s: " fmt, __func__
#define dt_err(np, fmt, ...) \
	pr_err("%s: " fmt, np->name, ##__VA_ARGS__)
#define dt_prop_err(np, str, fmt, ...) \
	dt_err(np, "%s: " fmt, str, ##__VA_ARGS__)

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/clkdev.h>
#include <linux/io.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/of.h>
#include <linux/hashtable.h>

#include <linux/clk/msm-clk-provider.h>
#include <soc/qcom/msm-clock-controller.h>
#include <soc/qcom/clock-rpm.h>

/* Protects list operations */
static DEFINE_MUTEX(msmclk_lock);
static LIST_HEAD(msmclk_parser_list);
static u32 msmclk_debug;

struct hitem {
	struct hlist_node list;
	phandle key;
	void *ptr;
};

int of_property_count_phandles(struct device_node *np, char *propname)
{
	const __be32 *phandle;
	int size;

	phandle = of_get_property(np, propname, &size);
	return phandle ? (size / sizeof(*phandle)) : -EINVAL;
}
EXPORT_SYMBOL(of_property_count_phandles);

int of_property_read_phandle_index(struct device_node *np, char *propname,
					int index, phandle *p)
{
	const __be32 *phandle;
	int size;

	phandle = of_get_property(np, propname, &size);
	if ((!phandle) || (size < sizeof(*phandle) * (index + 1)))
		return -EINVAL;

	*p = be32_to_cpup(phandle + index);
	return 0;
}
EXPORT_SYMBOL(of_property_read_phandle_index);

static int generic_vdd_parse_regulators(struct device *dev,
		struct clk_vdd_class *vdd, struct device_node *np)
{
	int num_regulators, i, rc;
	char *name = "qcom,regulators";

	num_regulators = of_property_count_phandles(np, name);
	if (num_regulators <= 0) {
		dt_prop_err(np, name, "missing dt property\n");
		return -EINVAL;
	}

	vdd->regulator = devm_kzalloc(dev,
				sizeof(*vdd->regulator) * num_regulators,
				GFP_KERNEL);
	if (!vdd->regulator) {
		dt_err(np, "memory alloc failure\n");
		return -ENOMEM;
	}

	for (i = 0; i < num_regulators; i++) {
		phandle p;
		rc = of_property_read_phandle_index(np, name, i, &p);
		if (rc) {
			dt_prop_err(np, name, "unable to read phandle\n");
			return rc;
		}

		vdd->regulator[i] = msmclk_parse_phandle(dev, p);
		if (IS_ERR(vdd->regulator[i])) {
			dt_prop_err(np, name, "hashtable lookup failed\n");
			return PTR_ERR(vdd->regulator[i]);
		}
	}

	vdd->num_regulators = num_regulators;
	return 0;
}

static int generic_vdd_parse_levels(struct device *dev,
		struct clk_vdd_class *vdd, struct device_node *np)
{
	int len, rc;
	char *name = "qcom,uV-levels";

	if (!of_find_property(np, name, &len)) {
		dt_prop_err(np, name, "missing dt property\n");
		return -EINVAL;
	}

	len /= sizeof(u32);
	if (len % vdd->num_regulators) {
		dt_err(np, "mismatch beween qcom,uV-levels and qcom,regulators dt properties\n");
		return -EINVAL;
	}

	vdd->num_levels = len / vdd->num_regulators;
	vdd->vdd_uv = devm_kzalloc(dev, len * sizeof(*vdd->vdd_uv),
						GFP_KERNEL);
	vdd->level_votes = devm_kzalloc(dev,
				vdd->num_levels * sizeof(*vdd->level_votes),
				GFP_KERNEL);

	if (!vdd->vdd_uv || !vdd->level_votes) {
		dt_err(np, "memory alloc failure\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(np, name, vdd->vdd_uv,
					vdd->num_levels * vdd->num_regulators);
	if (rc) {
		dt_prop_err(np, name, "unable to read u32 array\n");
		return -EINVAL;
	}

	/* Optional Property */
	name = "qcom,uA-levels";
	if (!of_find_property(np, name, &len))
		return 0;

	len /= sizeof(u32);
	if (len / vdd->num_regulators != vdd->num_levels) {
		dt_err(np, "size of qcom,uA-levels and qcom,uV-levels must match\n");
		return -EINVAL;
	}

	vdd->vdd_ua = devm_kzalloc(dev, len * sizeof(*vdd->vdd_ua),
						GFP_KERNEL);
	if (!vdd->vdd_ua) {
		dt_err(np, "memory alloc failure\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(np, name, vdd->vdd_ua,
					vdd->num_levels * vdd->num_regulators);
	if (rc) {
		dt_prop_err(np, name, "unable to read u32 array\n");
		return -EINVAL;
	}

	return 0;
}

static void *simple_vdd_class_dt_parser(struct device *dev,
			struct device_node *np)
{
	struct clk_vdd_class *vdd;
	int rc = 0;

	vdd = devm_kzalloc(dev, sizeof(*vdd), GFP_KERNEL);
	if (!vdd) {
		dev_err(dev, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}

	mutex_init(&vdd->lock);
	vdd->class_name = np->name;

	rc = generic_vdd_parse_regulators(dev, vdd, np);
	rc |= generic_vdd_parse_levels(dev, vdd, np);
	if (rc) {
		dt_err(np, "unable to read vdd_class\n");
		return ERR_PTR(rc);
	}

	return vdd;
}
MSMCLK_PARSER(simple_vdd_class_dt_parser, "qcom,simple-vdd-class", 0);

static int generic_clk_parse_parents(struct device *dev, struct clk *c,
					struct device_node *np)
{
	int rc;
	phandle p;
	char *name = "qcom,parent";

	/* This property is optional */
	if (!of_property_read_bool(np, name))
		return 0;

	rc = of_property_read_phandle_index(np, name, 0, &p);
	if (rc) {
		dt_prop_err(np, name, "unable to read phandle\n");
		return rc;
	}

	c->parent = msmclk_parse_phandle(dev, p);
	if (IS_ERR(c->parent)) {
		dt_prop_err(np, name, "hashtable lookup failed\n");
		return PTR_ERR(c->parent);
	}

	return 0;
}

static int generic_clk_parse_vdd(struct device *dev, struct clk *c,
					struct device_node *np)
{
	phandle p;
	int rc;
	char *name = "qcom,supply-group";

	/* This property is optional */
	if (!of_property_read_bool(np, name))
		return 0;

	rc = of_property_read_phandle_index(np, name, 0, &p);
	if (rc) {
		dt_prop_err(np, name, "unable to read phandle\n");
		return rc;
	}

	c->vdd_class = msmclk_parse_phandle(dev, p);
	if (IS_ERR(c->vdd_class)) {
		dt_prop_err(np, name, "hashtable lookup failed\n");
		return PTR_ERR(c->vdd_class);
	}

	return 0;
}

static int generic_clk_parse_flags(struct device *dev, struct clk *c,
						struct device_node *np)
{
	int rc;
	char *name = "qcom,clk-flags";

	/* This property is optional */
	if (!of_property_read_bool(np, name))
		return 0;

	rc = of_property_read_u32(np, name, &c->flags);
	if (rc) {
		dt_prop_err(np, name, "unable to read u32\n");
		return rc;
	}

	return 0;
}

static int generic_clk_parse_fmax(struct device *dev, struct clk *c,
					struct device_node *np)
{
	u32 prop_len, i;
	int rc;
	char *name = "qcom,clk-fmax";

	/* This property is optional */
	if (!of_find_property(np, name, &prop_len))
		return 0;

	if (!c->vdd_class) {
		dt_err(np, "both qcom,clk-fmax and qcom,supply-group must be defined\n");
		return -EINVAL;
	}

	prop_len /= sizeof(u32);
	if (prop_len % 2) {
		dt_prop_err(np, name, "bad length\n");
		return -EINVAL;
	}

	/* Value at proplen - 2 is the index of the  last entry in fmax array */
	rc = of_property_read_u32_index(np, name, prop_len - 2, &c->num_fmax);
	c->num_fmax += 1;
	if (rc) {
		dt_prop_err(np, name, "unable to read u32\n");
		return rc;
	}

	c->fmax = devm_kzalloc(dev, sizeof(*c->fmax) * c->num_fmax, GFP_KERNEL);
	if (!c->fmax) {
		dev_err(dev, "memory alloc failure\n");
		return -ENOMEM;
	}

	for (i = 0; i < prop_len; i += 2) {
		u32 level, value;
		rc = of_property_read_u32_index(np, name, i, &level);
		if (rc) {
			dt_prop_err(np, name, "unable to read u32\n");
			return rc;
		}

		rc = of_property_read_u32_index(np, name, i + 1, &value);
		if (rc) {
			dt_prop_err(np, name, "unable to read u32\n");
			return rc;
		}

		if (level >= c->num_fmax) {
			dt_prop_err(np, name, "must be sorted\n");
			return -EINVAL;
		}
		c->fmax[level] = value;
	}

	return 0;
}

static int generic_clk_add_lookup_tbl_entry(struct device *dev, struct clk *c)
{
	struct msmclk_data *drv = dev_get_drvdata(dev);
	struct clk_lookup *cl;

	if (drv->clk_tbl_size >= drv->max_clk_tbl_size) {
		dev_err(dev, "child node count should be > clock_count?\n");
		return -EINVAL;
	}

	cl = drv->clk_tbl + drv->clk_tbl_size;
	cl->clk = c;
	drv->clk_tbl_size++;
	return 0;
}

static int generic_clk_parse_depends(struct device *dev, struct clk *c,
						struct device_node *np)
{
	phandle p;
	int rc;
	char *name = "qcom,depends";

	/* This property is optional */
	if (!of_property_read_bool(np, name))
		return 0;

	rc = of_property_read_phandle_index(np, name, 0, &p);
	if (rc) {
		dt_prop_err(np, name, "unable to read phandle\n");
		return rc;
	}

	c->depends = msmclk_parse_phandle(dev, p);
	if (IS_ERR(c->depends)) {
		dt_prop_err(np, name, "hashtable lookup failed\n");
		return PTR_ERR(c->depends);
	}

	return 0;
}

static int generic_clk_parse_init_config(struct device *dev, struct clk *c,
						struct device_node *np)
{
	int rc;
	u32 temp;
	char *name = "qcom,config-rate";

	/* This property is optional */
	if (!of_property_read_bool(np, name))
		return 0;

	rc = of_property_read_u32(np, name, &temp);
	if (rc) {
		dt_prop_err(np, name, "unable to read u32\n");
		return rc;
	}
	c->init_rate = temp;

	name = "qcom,always-on";
	c->always_on = of_property_read_bool(np, name);
	return rc;
}

void *msmclk_generic_clk_init(struct device *dev, struct device_node *np,
				struct clk *c)
{
	int rc;

	/* CLK_INIT macro */
	spin_lock_init(&c->lock);
	mutex_init(&c->prepare_lock);
	INIT_LIST_HEAD(&c->children);
	INIT_LIST_HEAD(&c->siblings);
	INIT_LIST_HEAD(&c->list);
	c->dbg_name = np->name;

	rc = generic_clk_add_lookup_tbl_entry(dev, c);
	rc |= generic_clk_parse_flags(dev, c, np);
	rc |= generic_clk_parse_parents(dev, c, np);
	rc |= generic_clk_parse_vdd(dev, c, np);
	rc |= generic_clk_parse_fmax(dev, c, np);
	rc |= generic_clk_parse_depends(dev, c, np);
	rc |= generic_clk_parse_init_config(dev, c, np);

	if (rc) {
		dt_err(np, "unable to read clk\n");
		return ERR_PTR(-EINVAL);
	}

	return c;
}

static struct msmclk_parser *msmclk_parser_lookup(struct device_node *np)
{
	struct msmclk_parser *item;
	list_for_each_entry(item, &msmclk_parser_list, list) {
		if (of_device_is_compatible(np, item->compatible))
			return item;
	}
	return NULL;
}
void msmclk_parser_register(struct msmclk_parser *item)
{
	mutex_lock(&msmclk_lock);
	list_add(&item->list, &msmclk_parser_list);
	mutex_unlock(&msmclk_lock);
}

static int msmclk_htable_add(struct device *dev, void *result, phandle key);

void *msmclk_parse_dt_node(struct device *dev, struct device_node *np)
{
	struct msmclk_parser *parser;
	phandle key;
	void *result;
	int rc;

	key = np->phandle;
	result = msmclk_lookup_phandle(dev, key);
	if (!result)
		return ERR_PTR(-EINVAL);

	if (!of_device_is_available(np)) {
		dt_err(np, "node is disabled\n");
		return ERR_PTR(-EINVAL);
	}

	parser = msmclk_parser_lookup(np);
	if (IS_ERR(parser)) {
		dt_err(np, "no parser found\n");
		return ERR_PTR(-EINVAL);
	}

	/* This may return -EPROBE_DEFER */
	result = parser->parsedt(dev, np);
	if (IS_ERR(result)) {
		dt_err(np, "parsedt failed");
		return result;
	}

	rc = msmclk_htable_add(dev, result, key);
	if (rc)
		return ERR_PTR(rc);

	return result;
}

void *msmclk_parse_phandle(struct device *dev, phandle key)
{
	struct hitem *item;
	struct device_node *np;
	struct msmclk_data *drv = dev_get_drvdata(dev);

	/*
	 * the default phandle value is 0. Since hashtable keys must
	 * be unique, reject the default value.
	 */
	if (!key)
		return ERR_PTR(-EINVAL);

	hash_for_each_possible(drv->htable, item, list, key) {
		if (item->key == key)
			return item->ptr;
	}

	np = of_find_node_by_phandle(key);
	if (!np)
		return ERR_PTR(-EINVAL);

	return msmclk_parse_dt_node(dev, np);
}
EXPORT_SYMBOL(msmclk_parse_phandle);

void *msmclk_lookup_phandle(struct device *dev, phandle key)
{
	struct hitem *item;
	struct msmclk_data *drv = dev_get_drvdata(dev);

	hash_for_each_possible(drv->htable, item, list, key) {
		if (item->key == key)
			return item->ptr;
	}

	return ERR_PTR(-EINVAL);
}
EXPORT_SYMBOL(msmclk_lookup_phandle);

static int msmclk_htable_add(struct device *dev, void *data, phandle key)
{
	struct hitem *item;
	struct msmclk_data *drv = dev_get_drvdata(dev);

	/*
	 * If there are no phandle references to a node, key == 0. However, if
	 * there is a second node like this, both will have key == 0. This
	 * violates the requirement that hashtable keys be unique. Skip it.
	 */
	if (!key)
		return 0;

	if (!IS_ERR(msmclk_lookup_phandle(dev, key))) {
		struct device_node *np = of_find_node_by_phandle(key);
		dev_err(dev, "attempt to add duplicate entry for %s\n",
				np ? np->name : "NULL");
		return -EINVAL;
	}

	item = devm_kzalloc(dev, sizeof(*item), GFP_KERNEL);
	if (!item) {
		dev_err(dev, "memory alloc failure\n");
		return -ENOMEM;
	}

	INIT_HLIST_NODE(&item->list);
	item->key = key;
	item->ptr = data;

	hash_add(drv->htable, &item->list, key);
	return 0;
}

/*
 * Currently, regulators are the only elements capable of probe deferral.
 * Check them first to handle probe deferal efficiently.
*/
static int get_ext_regulators(struct device *dev)
{
	int num_strings, i, rc;
	struct device_node *np;
	void *item;
	char *name = "qcom,regulator-names";

	np = dev->of_node;
	/* This property is optional */
	num_strings = of_property_count_strings(np, name);
	if (num_strings <= 0)
		return 0;

	for (i = 0; i < num_strings; i++) {
		const char *str;
		char buf[50];
		phandle key;

		rc = of_property_read_string_index(np, name, i, &str);
		if (rc) {
			dt_prop_err(np, name, "unable to read string\n");
			return rc;
		}

		item = devm_regulator_get(dev, str);
		if (IS_ERR(item)) {
			dev_err(dev, "Failed to get regulator: %s\n", str);
			return PTR_ERR(item);
		}

		snprintf(buf, ARRAY_SIZE(buf), "%s-supply", str);
		rc = of_property_read_phandle_index(np, buf, 0, &key);
		if (rc) {
			dt_prop_err(np, buf, "unable to read phandle\n");
			return rc;
		}

		rc = msmclk_htable_add(dev, item, key);
		if (rc)
			return rc;
	}
	return 0;
}

static struct clk *msmclk_clk_get(struct of_phandle_args *clkspec, void *data)
{
	phandle key;
	struct clk *c = ERR_PTR(-ENOENT);

	key = clkspec->args[0];
	c = msmclk_lookup_phandle(data, key);

	if (!IS_ERR(c) && !(c->flags & CLKFLAG_INIT_DONE))
		return ERR_PTR(-EPROBE_DEFER);

	return c;
}

static void *regulator_dt_parser(struct device *dev, struct device_node *np)
{
	dt_err(np, "regulators should be handled in probe()");
	return ERR_PTR(-EINVAL);
}
MSMCLK_PARSER(regulator_dt_parser, "qcom,rpm-smd-regulator", 0);

static void *msmclk_dt_parser(struct device *dev, struct device_node *np)
{
	dt_err(np, "calling into other clock controllers isn't allowed");
	return ERR_PTR(-EINVAL);
}
MSMCLK_PARSER(msmclk_dt_parser, "qcom,msm-clock-controller", 0);

static struct msmclk_data *msmclk_drv_init(struct device *dev)
{
	struct msmclk_data *drv;
	size_t size;

	drv = devm_kzalloc(dev, sizeof(*drv), GFP_KERNEL);
	if (!drv) {
		dev_err(dev, "memory alloc failure\n");
		return ERR_PTR(-ENOMEM);
	}
	dev_set_drvdata(dev, drv);

	drv->dev = dev;
	INIT_LIST_HEAD(&drv->list);

	/* This overestimates size */
	drv->max_clk_tbl_size = of_get_child_count(dev->of_node);
	size = sizeof(*drv->clk_tbl) * drv->max_clk_tbl_size;
	drv->clk_tbl = devm_kzalloc(dev, size, GFP_KERNEL);
	if (!drv->clk_tbl) {
		dev_err(dev, "memory alloc failure clock table size %zu\n",
				size);
		return ERR_PTR(-ENOMEM);
	}

	hash_init(drv->htable);
	return drv;
}

static int msmclk_probe(struct platform_device *pdev)
{
	struct resource *res;
	struct device *dev;
	struct msmclk_data *drv;
	struct device_node *child;
	void *result;
	int rc = 0;

	dev = &pdev->dev;
	drv = msmclk_drv_init(dev);
	if (IS_ERR(drv))
		return PTR_ERR(drv);

	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "cc-base");
	if (!res) {
		dt_err(dev->of_node, "missing cc-base\n");
		return -EINVAL;
	}
	drv->base = devm_ioremap(dev, res->start, resource_size(res));
	if (!drv->base) {
		dev_err(dev, "ioremap failed for drv->base\n");
		return -ENOMEM;
	}
	rc = msmclk_htable_add(dev, drv, dev->of_node->phandle);
	if (rc)
		return rc;

	rc = enable_rpm_scaling();
	if (rc)
		return rc;

	rc = get_ext_regulators(dev);
	if (rc)
		return rc;

	/*
	 * Returning -EPROBE_DEFER here is inefficient due to
	 * destroying work 'unnecessarily'
	 */
	for_each_available_child_of_node(dev->of_node, child) {
		result = msmclk_parse_dt_node(dev, child);
		if (!IS_ERR(result))
			continue;
		if (!msmclk_debug)
			return PTR_ERR(result);
		/*
		 * Parse and report all errors instead of immediately
		 * exiting. Return the first error code.
		 */
		if (!rc)
			rc = PTR_ERR(result);
	}
	if (rc)
		return rc;

	rc = of_clk_add_provider(dev->of_node, msmclk_clk_get, dev);
	if (rc) {
		dev_err(dev, "of_clk_add_provider failed\n");
		return rc;
	}

	/*
	 * can't fail after registering clocks, because users may have
	 * gotten clock references. Failing would delete the memory.
	 */
	WARN_ON(msm_clock_register(drv->clk_tbl, drv->clk_tbl_size));
	dev_info(dev, "registered clocks\n");

	return 0;
}

static struct of_device_id msmclk_match_table[] = {
	{.compatible = "qcom,msm-clock-controller"},
	{}
};

static struct platform_driver msmclk_driver = {
	.probe = msmclk_probe,
	.driver = {
		.name =  "msm-clock-controller",
		.of_match_table = msmclk_match_table,
		.owner = THIS_MODULE,
	},
};

static bool initialized;
int __init msmclk_init(void)
{
	int rc;
	if (initialized)
		return 0;

	rc = platform_driver_register(&msmclk_driver);
	if (rc)
		return rc;
	initialized = true;
	return rc;
}
arch_initcall(msmclk_init);
