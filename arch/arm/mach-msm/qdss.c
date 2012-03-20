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
#include <mach/rpm.h>

#include "rpm_resources.h"
#include "qdss.h"

enum {
	QDSS_CLK_OFF,
	QDSS_CLK_ON_DBG,
	QDSS_CLK_ON_HSDBG,
};

struct qdss_ctx {
	struct kobject	*modulekobj;
	uint8_t		max_clk;
};

static struct qdss_ctx qdss;


struct kobject *qdss_get_modulekobj(void)
{
	return qdss.modulekobj;
}

int qdss_clk_enable(void)
{
	int ret;

	struct msm_rpm_iv_pair iv;
	iv.id = MSM_RPM_ID_QDSS_CLK;
	if (qdss.max_clk)
		iv.value = QDSS_CLK_ON_HSDBG;
	else
		iv.value = QDSS_CLK_ON_DBG;
	ret = msm_rpmrs_set(MSM_RPM_CTX_SET_0, &iv, 1);
	if (WARN(ret, "qdss clks not enabled (%d)\n", ret))
		goto err_clk;

	return 0;
err_clk:
	return ret;
}

void qdss_clk_disable(void)
{
	int ret;
	struct msm_rpm_iv_pair iv;

	iv.id = MSM_RPM_ID_QDSS_CLK;
	iv.value = QDSS_CLK_OFF;
	ret = msm_rpmrs_set(MSM_RPM_CTX_SET_0, &iv, 1);
	WARN(ret, "qdss clks not disabled (%d)\n", ret);
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

static void qdss_sysfs_exit(void)
{
	sysfs_remove_file(qdss.modulekobj, &max_clk_attr.attr);
}

static int __init qdss_init(void)
{
	int ret;

	ret = qdss_sysfs_init();
	if (ret)
		goto err_sysfs;
	ret = etb_init();
	if (ret)
		goto err_etb;
	ret = tpiu_init();
	if (ret)
		goto err_tpiu;
	ret = funnel_init();
	if (ret)
		goto err_funnel;
	ret = etm_init();
	if (ret)
		goto err_etm;

	pr_info("QDSS initialized\n");
	return 0;
err_etm:
	funnel_exit();
err_funnel:
	tpiu_exit();
err_tpiu:
	etb_exit();
err_etb:
	qdss_sysfs_exit();
err_sysfs:
	pr_err("QDSS init failed\n");
	return ret;
}
module_init(qdss_init);

static void __exit qdss_exit(void)
{
	qdss_sysfs_exit();
	etm_exit();
	funnel_exit();
	tpiu_exit();
	etb_exit();
}
module_exit(qdss_exit);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Qualcomm Debug SubSystem Driver");
