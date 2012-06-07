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
#include <mach/rpm.h>

#include "rpm_resources.h"
#include "qdss-priv.h"

#define MAX_STR_LEN	(65535)

enum {
	QDSS_CLK_OFF,
	QDSS_CLK_ON_DBG,
	QDSS_CLK_ON_HSDBG,
};

/*
 * Exclusion rules for structure fields.
 *
 * S: qdss.sources_mutex protected.
 * I: qdss.sink_mutex protected.
 * C: qdss.clk_mutex protected.
 */
struct qdss_ctx {
	struct kobject			*modulekobj;
	struct msm_qdss_platform_data	*pdata;
	struct list_head		sources;	/* S: sources list */
	struct mutex			sources_mutex;
	uint8_t				sink_count;	/* I: sink count */
	struct mutex			sink_mutex;
	uint8_t				max_clk;
	uint8_t				clk_count;	/* C: clk count */
	struct mutex			clk_mutex;
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
	int ret;

	if (!src)
		return -EINVAL;

	ret = qdss_clk_enable();
	if (ret)
		goto err;

	if ((qdss.pdata)->afamily) {
		mutex_lock(&qdss.sink_mutex);
		if (qdss.sink_count == 0) {
			etb_disable();
			tpiu_disable();
			/* enable ETB first to avoid losing any trace data */
			etb_enable();
		}
		qdss.sink_count++;
		mutex_unlock(&qdss.sink_mutex);
	}

	funnel_enable(0x0, src->fport_mask);
	return 0;
err:
	return ret;
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

	if ((qdss.pdata)->afamily) {
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
	qdss_clk_disable();
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
	if ((qdss.pdata)->afamily) {
		etb_dump();
		etb_disable();
	}
}
EXPORT_SYMBOL(qdss_disable_sink);

/**
 * qdss_clk_enable - enable qdss clocks
 *
 * Enables qdss clocks via RPM if they aren't already enabled, otherwise
 * increments the reference count.
 *
 * CONTEXT:
 * Might sleep. Uses a mutex lock. Should be called from a non-atomic context.
 *
 * RETURNS:
 * 0 on success, non-zero on failure
 */
int qdss_clk_enable(void)
{
	int ret;
	struct msm_rpm_iv_pair iv;

	mutex_lock(&qdss.clk_mutex);
	if (qdss.clk_count == 0) {
		iv.id = MSM_RPM_ID_QDSS_CLK;
		if (qdss.max_clk)
			iv.value = QDSS_CLK_ON_HSDBG;
		else
			iv.value = QDSS_CLK_ON_DBG;
		ret = msm_rpmrs_set(MSM_RPM_CTX_SET_0, &iv, 1);
		if (WARN(ret, "qdss clks not enabled (%d)\n", ret))
			goto err_clk;
	}
	qdss.clk_count++;
	mutex_unlock(&qdss.clk_mutex);
	return 0;
err_clk:
	mutex_unlock(&qdss.clk_mutex);
	return ret;
}
EXPORT_SYMBOL(qdss_clk_enable);

/**
 * qdss_clk_disable - disable qdss clocks
 *
 * Disables qdss clocks via RPM if the reference count is one, otherwise
 * decrements the reference count.
 *
 * CONTEXT:
 * Might sleep. Uses a mutex lock. Should be called from a non-atomic context.
 */
void qdss_clk_disable(void)
{
	int ret;
	struct msm_rpm_iv_pair iv;

	mutex_lock(&qdss.clk_mutex);
	if (WARN(qdss.clk_count == 0, "qdss clks are unbalanced\n"))
		goto out;
	if (qdss.clk_count == 1) {
		iv.id = MSM_RPM_ID_QDSS_CLK;
		iv.value = QDSS_CLK_OFF;
		ret = msm_rpmrs_set(MSM_RPM_CTX_SET_0, &iv, 1);
		WARN(ret, "qdss clks not disabled (%d)\n", ret);
	}
	qdss.clk_count--;
out:
	mutex_unlock(&qdss.clk_mutex);
}
EXPORT_SYMBOL(qdss_clk_disable);

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
	int ret;
	struct qdss_source *src_table;
	size_t num_srcs;

	mutex_init(&qdss.sources_mutex);
	mutex_init(&qdss.clk_mutex);
	mutex_init(&qdss.sink_mutex);

	if (pdev->dev.platform_data == NULL) {
		pr_err("%s: platform data is NULL\n", __func__);
		ret = -ENODEV;
		goto err_pdata;
	}
	qdss.pdata = pdev->dev.platform_data;

	INIT_LIST_HEAD(&qdss.sources);
	src_table = (qdss.pdata)->src_table;
	num_srcs = (qdss.pdata)->size;
	qdss_add_sources(src_table, num_srcs);

	pr_info("QDSS arch initialized\n");
	return 0;
err_pdata:
	mutex_destroy(&qdss.sink_mutex);
	mutex_destroy(&qdss.clk_mutex);
	mutex_destroy(&qdss.sources_mutex);
	pr_err("QDSS init failed\n");
	return ret;
}

static int __devexit qdss_remove(struct platform_device *pdev)
{
	qdss_sysfs_exit();
	mutex_destroy(&qdss.sink_mutex);
	mutex_destroy(&qdss.clk_mutex);
	mutex_destroy(&qdss.sources_mutex);

	return 0;
}

static struct platform_driver qdss_driver = {
	.probe          = qdss_probe,
	.remove         = __devexit_p(qdss_remove),
	.driver         = {
		.name   = "msm_qdss",
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
