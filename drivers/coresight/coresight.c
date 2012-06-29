/* Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/err.h>
#include <linux/export.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/coresight.h>

#include "coresight-priv.h"

#define MAX_STR_LEN	(65535)


static LIST_HEAD(coresight_orph_conns);
static DEFINE_MUTEX(coresight_conns_mutex);
static LIST_HEAD(coresight_devs);
static DEFINE_MUTEX(coresight_devs_mutex);


int coresight_enable(struct coresight_device *csdev, int port)
{
	int i;
	int ret;
	struct coresight_connection *conn;

	mutex_lock(&csdev->mutex);
	if (csdev->refcnt[port] == 0) {
		for (i = 0; i < csdev->nr_conns; i++) {
			conn = &csdev->conns[i];
			ret = coresight_enable(conn->child_dev,
					       conn->child_port);
			if (ret)
				goto err_enable_child;
		}
		if (csdev->ops->enable)
			ret = csdev->ops->enable(csdev, port);
		if (ret)
			goto err_enable;
	}
	csdev->refcnt[port]++;
	mutex_unlock(&csdev->mutex);
	return 0;
err_enable_child:
	while (i) {
		conn = &csdev->conns[--i];
		coresight_disable(conn->child_dev, conn->child_port);
	}
err_enable:
	mutex_unlock(&csdev->mutex);
	return ret;
}
EXPORT_SYMBOL(coresight_enable);

void coresight_disable(struct coresight_device *csdev, int port)
{
	int i;
	struct coresight_connection *conn;

	mutex_lock(&csdev->mutex);
	if (csdev->refcnt[port] == 1) {
		if (csdev->ops->disable)
			csdev->ops->disable(csdev, port);
		for (i = 0; i < csdev->nr_conns; i++) {
			conn = &csdev->conns[i];
			coresight_disable(conn->child_dev, conn->child_port);
		}
	}
	csdev->refcnt[port]--;
	mutex_unlock(&csdev->mutex);
}
EXPORT_SYMBOL(coresight_disable);

static ssize_t coresight_show_type(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", dev->type->name);
}

static struct device_attribute coresight_dev_attrs[] = {
	__ATTR(type, S_IRUGO, coresight_show_type, NULL),
	{ },
};

struct bus_type coresight_bus_type = {
	.name		= "coresight",
	.dev_attrs	= coresight_dev_attrs,
};

static ssize_t coresight_show_enable(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	struct coresight_device *csdev = to_coresight_device(dev);

	return scnprintf(buf, PAGE_SIZE, "%u\n", (unsigned)csdev->enable);
}

static ssize_t coresight_store_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	int ret = 0;
	unsigned long val;
	struct coresight_device *csdev = to_coresight_device(dev);

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	if (val)
		ret = coresight_enable(csdev, 0);
	else
		coresight_disable(csdev, 0);

	if (ret)
		return ret;
	return size;
}
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR, coresight_show_enable,
		   coresight_store_enable);

static struct attribute *coresight_attrs[] = {
	&dev_attr_enable.attr,
	NULL,
};

static struct attribute_group coresight_attr_grp = {
	.attrs = coresight_attrs,
};

static const struct attribute_group *coresight_attr_grps[] = {
	&coresight_attr_grp,
	NULL,
};

static struct device_type coresight_dev_type[CORESIGHT_DEV_TYPE_MAX] = {
	{
		.name = "source",
		.groups = coresight_attr_grps,
	},
	{
		.name = "link",
	},
	{
		.name = "sink",
		.groups = coresight_attr_grps,
	},
};

static void coresight_device_release(struct device *dev)
{
	struct coresight_device *csdev = to_coresight_device(dev);
	mutex_destroy(&csdev->mutex);
	kfree(csdev);
}

static void coresight_fixup_orphan_conns(struct coresight_device *csdev)
{
	struct coresight_connection *conn, *temp;

	mutex_lock(&coresight_conns_mutex);
	list_for_each_entry_safe(conn, temp, &coresight_orph_conns, link) {
		if (conn->child_id == csdev->id) {
			conn->child_dev = csdev;
			list_del(&conn->link);
		}
	}
	mutex_unlock(&coresight_conns_mutex);
}

static void coresight_fixup_device_conns(struct coresight_device *csdev)
{
	int i;
	struct coresight_device *cd;
	bool found;

	for (i = 0; i < csdev->nr_conns; i++) {
		found = false;
		mutex_lock(&coresight_devs_mutex);
		list_for_each_entry(cd, &coresight_devs, link) {
			if (csdev->conns[i].child_id == cd->id) {
				csdev->conns[i].child_dev = cd;
				found = true;
				break;
			}
		}
		mutex_unlock(&coresight_devs_mutex);
		if (!found) {
			mutex_lock(&coresight_conns_mutex);
			list_add_tail(&csdev->conns[i].link,
				      &coresight_orph_conns);
			mutex_unlock(&coresight_conns_mutex);
		}
	}
}

struct coresight_device *coresight_register(struct coresight_desc *desc)
{
	int i;
	int ret;
	int *refcnt;
	struct coresight_device *csdev;
	struct coresight_connection *conns;

	csdev = kzalloc(sizeof(*csdev), GFP_KERNEL);
	if (!csdev) {
		ret = -ENOMEM;
		goto err_kzalloc_csdev;
	}

	mutex_init(&csdev->mutex);
	csdev->id = desc->pdata->id;

	refcnt = kzalloc(sizeof(*refcnt) * desc->pdata->nr_ports, GFP_KERNEL);
	if (!refcnt) {
		ret = -ENOMEM;
		goto err_kzalloc_refcnt;
	}
	csdev->refcnt = refcnt;

	csdev->nr_conns = desc->pdata->nr_children;
	conns = kzalloc(sizeof(*conns) * csdev->nr_conns, GFP_KERNEL);
	if (!conns) {
		ret = -ENOMEM;
		goto err_kzalloc_conns;
	}
	for (i = 0; i < csdev->nr_conns; i++) {
		conns[i].child_id = desc->pdata->child_ids[i];
		conns[i].child_port = desc->pdata->child_ports[i];
	}
	csdev->conns = conns;

	csdev->ops = desc->ops;
	csdev->owner = desc->owner;

	csdev->dev.type = &coresight_dev_type[desc->type];
	csdev->dev.groups = desc->groups;
	csdev->dev.parent = desc->dev;
	csdev->dev.bus = &coresight_bus_type;
	csdev->dev.release = coresight_device_release;
	dev_set_name(&csdev->dev, "%s", desc->pdata->name);

	coresight_fixup_device_conns(csdev);
	ret = device_register(&csdev->dev);
	if (ret)
		goto err_dev_reg;
	coresight_fixup_orphan_conns(csdev);

	mutex_lock(&coresight_devs_mutex);
	list_add_tail(&csdev->link, &coresight_devs);
	mutex_unlock(&coresight_devs_mutex);

	return csdev;
err_dev_reg:
	put_device(&csdev->dev);
	kfree(conns);
err_kzalloc_conns:
	kfree(refcnt);
err_kzalloc_refcnt:
	mutex_destroy(&csdev->mutex);
	kfree(csdev);
err_kzalloc_csdev:
	return ERR_PTR(ret);
}
EXPORT_SYMBOL(coresight_register);

void coresight_unregister(struct coresight_device *csdev)
{
	if (IS_ERR_OR_NULL(csdev))
		return;

	if (get_device(&csdev->dev)) {
		mutex_lock(&csdev->mutex);
		device_unregister(&csdev->dev);
		mutex_unlock(&csdev->mutex);
		put_device(&csdev->dev);
	}
}
EXPORT_SYMBOL(coresight_unregister);

static int __init coresight_init(void)
{
	return bus_register(&coresight_bus_type);
}
subsys_initcall(coresight_init);

static void __exit coresight_exit(void)
{
	bus_unregister(&coresight_bus_type);
}
module_exit(coresight_exit);

MODULE_LICENSE("GPL v2");
/*
 * Exclusion rules for structure fields.
 *
 * S: qdss.sources_mutex protected.
 * I: qdss.sink_mutex protected.
 * C: qdss.clk_mutex protected.
 */
struct qdss_ctx {
	struct kobject			*modulekobj;
	uint8_t				afamily;
	struct list_head		sources;	/* S: sources list */
	struct mutex			sources_mutex;
	uint8_t				sink_count;	/* I: sink count */
	struct mutex			sink_mutex;
	uint8_t				max_clk;
	struct clk			*clk;
};

static struct qdss_ctx qdss;

/**
 * qdss_get - get the qdss source handle
 * @name: name of the qdss source
 *
 * Searches the sources list to get the qdss source handle for this source.
 *
 * CONTEXT:
 * Typically called from init or probe functions
 *
 * RETURNS:
 * pointer to struct qdss_source on success, %NULL on failure
 */
struct qdss_source *qdss_get(const char *name)
{
	struct qdss_source *src, *source = NULL;

	mutex_lock(&qdss.sources_mutex);
	list_for_each_entry(src, &qdss.sources, link) {
		if (src->name) {
			if (strncmp(src->name, name, MAX_STR_LEN))
				continue;
			source = src;
			break;
		}
	}
	mutex_unlock(&qdss.sources_mutex);

	return source ? source : ERR_PTR(-ENOENT);
}
EXPORT_SYMBOL(qdss_get);

/**
 * qdss_put - release the qdss source handle
 * @name: name of the qdss source
 *
 * CONTEXT:
 * Typically called from driver remove or exit functions
 */
void qdss_put(struct qdss_source *src)
{
}
EXPORT_SYMBOL(qdss_put);

/**
 * qdss_enable - enable qdss for the source
 * @src: handle for the source making the call
 *
 * Enables qdss block (relevant funnel ports and sink) if not already
 * enabled, otherwise increments the reference count
 *
 * CONTEXT:
 * Might sleep. Uses a mutex lock. Should be called from a non-atomic context.
 *
 * RETURNS:
 * 0 on success, non-zero on failure
 */
int qdss_enable(struct qdss_source *src)
{
	if (!src)
		return -EINVAL;

	if (qdss.afamily) {
		mutex_lock(&qdss.sink_mutex);
		if (qdss.sink_count == 0) {
			tpiu_disable();
			/* enable ETB first to avoid losing any trace data */
			etb_enable();
		}
		qdss.sink_count++;
		mutex_unlock(&qdss.sink_mutex);
	}

	funnel_enable(0x0, src->fport_mask);
	return 0;
}
EXPORT_SYMBOL(qdss_enable);

/**
 * qdss_disable - disable qdss for the source
 * @src: handle for the source making the call
 *
 * Disables qdss block (relevant funnel ports and sink) if the reference count
 * is one, otherwise decrements the reference count
 *
 * CONTEXT:
 * Might sleep. Uses a mutex lock. Should be called from a non-atomic context.
 */
void qdss_disable(struct qdss_source *src)
{
	if (!src)
		return;

	if (qdss.afamily) {
		mutex_lock(&qdss.sink_mutex);
		if (WARN(qdss.sink_count == 0, "qdss is unbalanced\n"))
			goto out;
		if (qdss.sink_count == 1) {
			etb_dump();
			etb_disable();
		}
		qdss.sink_count--;
		mutex_unlock(&qdss.sink_mutex);
	}

	funnel_disable(0x0, src->fport_mask);
	return;
out:
	mutex_unlock(&qdss.sink_mutex);
}
EXPORT_SYMBOL(qdss_disable);

/**
 * qdss_disable_sink - force disable the current qdss sink(s)
 *
 * Force disable the current qdss sink(s) to stop the sink from accepting any
 * trace generated subsequent to this call. This function should only be used
 * as a way to stop the sink from getting polluted with trace data that is
 * uninteresting after an event of interest has occured.
 *
 * CONTEXT:
 * Can be called from atomic or non-atomic context.
 */
void qdss_disable_sink(void)
{
	if (qdss.afamily) {
		etb_dump();
		etb_disable();
	}
}
EXPORT_SYMBOL(qdss_disable_sink);

struct kobject *qdss_get_modulekobj(void)
{
	return qdss.modulekobj;
}

#define QDSS_ATTR(name)						\
static struct kobj_attribute name##_attr =				\
		__ATTR(name, S_IRUGO | S_IWUSR, name##_show, name##_store)

static ssize_t max_clk_store(struct kobject *kobj,
			struct kobj_attribute *attr,
			const char *buf, size_t n)
{
	unsigned long val;

	if (sscanf(buf, "%lx", &val) != 1)
		return -EINVAL;

	qdss.max_clk = val;
	return n;
}
static ssize_t max_clk_show(struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf)
{
	unsigned long val = qdss.max_clk;
	return scnprintf(buf, PAGE_SIZE, "%#lx\n", val);
}
QDSS_ATTR(max_clk);

static void __devinit qdss_add_sources(struct qdss_source *srcs, size_t num)
{
	mutex_lock(&qdss.sources_mutex);
	while (num--) {
		list_add_tail(&srcs->link, &qdss.sources);
		srcs++;
	}
	mutex_unlock(&qdss.sources_mutex);
}

static int __init qdss_sysfs_init(void)
{
	int ret;

	qdss.modulekobj = kset_find_obj(module_kset, KBUILD_MODNAME);
	if (!qdss.modulekobj) {
		pr_err("failed to find QDSS sysfs module kobject\n");
		ret = -ENOENT;
		goto err;
	}

	ret = sysfs_create_file(qdss.modulekobj, &max_clk_attr.attr);
	if (ret) {
		pr_err("failed to create QDSS sysfs max_clk attribute\n");
		goto err;
	}

	return 0;
err:
	return ret;
}

static void __devexit qdss_sysfs_exit(void)
{
	sysfs_remove_file(qdss.modulekobj, &max_clk_attr.attr);
}

static int __devinit qdss_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct msm_qdss_platform_data *pdata;

	mutex_init(&qdss.sources_mutex);
	mutex_init(&qdss.sink_mutex);

	INIT_LIST_HEAD(&qdss.sources);

	pdata = pdev->dev.platform_data;
	if (!pdata)
		goto err_pdata;

	qdss.afamily = pdata->afamily;
	qdss_add_sources(pdata->src_table, pdata->size);

	pr_info("QDSS arch initialized\n");
	return 0;
err_pdata:
	mutex_destroy(&qdss.sink_mutex);
	mutex_destroy(&qdss.sources_mutex);
	pr_err("QDSS init failed\n");
	return ret;
}

static int __devexit qdss_remove(struct platform_device *pdev)
{
	qdss_sysfs_exit();
	mutex_destroy(&qdss.sink_mutex);
	mutex_destroy(&qdss.sources_mutex);

	return 0;
}

static struct of_device_id qdss_match[] = {
	{.compatible = "qcom,msm-qdss"},
	{}
};

static struct platform_driver qdss_driver = {
	.probe          = qdss_probe,
	.remove         = __devexit_p(qdss_remove),
	.driver         = {
		.name   = "msm_qdss",
		.owner	= THIS_MODULE,
		.of_match_table = qdss_match,
	},
};

static int __init qdss_init(void)
{
	return platform_driver_register(&qdss_driver);
}
arch_initcall(qdss_init);

static int __init qdss_module_init(void)
{
	int ret;

	ret = qdss_sysfs_init();
	if (ret)
		goto err_sysfs;

	pr_info("QDSS module initialized\n");
	return 0;
err_sysfs:
	return ret;
}
module_init(qdss_module_init);

static void __exit qdss_exit(void)
{
	platform_driver_unregister(&qdss_driver);
}
module_exit(qdss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Debug SubSystem Driver");
