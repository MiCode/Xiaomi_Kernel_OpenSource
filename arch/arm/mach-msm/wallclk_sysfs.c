/* Copyright (c) 2013, The Linux Foundation. All rights reserved.
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
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/semaphore.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/timer.h>
#include <linux/ctype.h>
#include <linux/uaccess.h>
#include <linux/errno.h>

#include "wallclk.h"

#define WALLCLK_SYSFS_MODULE_NAME	"wallclk_sysfs"

static struct kobject *wallclk_kobj;

static ssize_t sfn_show(struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf)
{
	int rc;

	rc = wallclk_get_sfn();
	if (rc < 0)
		return rc;
	return snprintf(buf, 10, "%d\n", rc);
}

static ssize_t sfn_store(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 const char *buf,
			 size_t count)
{
	u16 sfn;
	int rc;

	if (kstrtou16(buf, 0, &sfn)) {
		printk(KERN_ERR "%s: sfn input is not a valid u16 value\n",
		       WALLCLK_SYSFS_MODULE_NAME);
		rc = -EINVAL;
		goto out;
	}

	rc = wallclk_set_sfn(sfn);

	if (rc) {
		printk(KERN_ERR "%s: fail to set sfn\n",
		       WALLCLK_SYSFS_MODULE_NAME);
		goto out;
	}
	rc = count;

out:
	return rc;
}

static struct kobj_attribute sfn_attribute =
	__ATTR(sfn, 0666, sfn_show, sfn_store);

static ssize_t sfn_ref_show(struct kobject *kobj,
			    struct kobj_attribute *attr,
			    char *buf)
{
	int rc;

	rc = wallclk_get_sfn_ref();
	if (rc < 0)
		return rc;
	return snprintf(buf, 10, "%d\n", rc);
}

static ssize_t sfn_ref_store(struct kobject *kobj,
			     struct kobj_attribute *attr,
			     const char *buf,
			     size_t count)
{
	u16 sfn_ref;
	int rc;

	if (kstrtou16(buf, 0, &sfn_ref)) {
		printk(KERN_ERR "%s: sfn_ref input is not a valid u16 value\n",
		       WALLCLK_SYSFS_MODULE_NAME);
		rc = -EINVAL;
		goto out;
	}

	rc = wallclk_set_sfn_ref(sfn_ref);

	if (rc) {
		printk(KERN_ERR "%s: fail to set sfn_ref\n",
		       WALLCLK_SYSFS_MODULE_NAME);
		goto out;
	}
	rc = count;

out:
	return rc;
}

static struct kobj_attribute sfn_ref_attribute =
	__ATTR(sfn_ref, 0666, sfn_ref_show, sfn_ref_store);

static ssize_t reg_show(struct kobject *kobj,
			struct kobj_attribute *attr,
			char *buf,
			u32 offset)
{
	int rc;
	u32 val;

	rc = wallclk_reg_read(offset, &val);
	if (rc)
		return rc;

	return snprintf(buf, 20, "%08x\n", val);
}

static ssize_t reg_store(struct kobject *kobj,
			 struct kobj_attribute *attr,
			 const char *buf,
			 const size_t count,
			 u32 offset)
{
	u32 v;
	int rc;

	if (kstrtou32(buf, 0, &v)) {
		printk(KERN_ERR "%s: input is not a valid u32 value\n",
		       WALLCLK_SYSFS_MODULE_NAME);
		rc = -EINVAL;
		goto out;
	}

	rc = wallclk_reg_write(offset, v);

	if (rc) {
		printk(KERN_ERR "%s: fail to set register(offset=0x%x)\n",
		       WALLCLK_SYSFS_MODULE_NAME, offset);
		goto out;
	}
	rc = count;

out:
	return rc;
}

static ssize_t ctrl_reg_show(struct kobject *kobj,
			     struct kobj_attribute *attr,
			     char *buf)
{
	return reg_show(kobj, attr, buf, CTRL_REG_OFFSET);
}

static ssize_t ctrl_reg_store(struct kobject *kobj,
			      struct kobj_attribute *attr,
			      const char *buf,
			      const size_t count)
{
	return reg_store(kobj, attr, buf, count, CTRL_REG_OFFSET);
}

static struct kobj_attribute ctrl_reg_attribute =
	__ATTR(ctrl_reg, 0666, ctrl_reg_show, ctrl_reg_store);

static ssize_t basetime0_reg_show(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  char *buf)
{
	return reg_show(kobj, attr, buf, CLK_BASE_TIME0_OFFSET);
}

static ssize_t basetime0_reg_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf,
				   size_t count)
{
	return reg_store(kobj, attr, buf, count, CLK_BASE_TIME0_OFFSET);
}

static struct kobj_attribute basetime0_reg_attribute =
	__ATTR(base_time0_reg, 0666, basetime0_reg_show, basetime0_reg_store);

static ssize_t basetime1_reg_show(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  char *buf)
{
	return reg_show(kobj, attr, buf, CLK_BASE_TIME1_OFFSET);
}

static ssize_t basetime1_reg_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf,
				   size_t count)
{
	return reg_store(kobj, attr, buf, count, CLK_BASE_TIME1_OFFSET);
}

static struct kobj_attribute basetime1_reg_attribute =
	__ATTR(base_time1_reg, 0666, basetime1_reg_show, basetime1_reg_store);

static ssize_t pulse_cnt_reg_show(struct kobject *kobj,
				  struct kobj_attribute *attr,
				  char *buf)
{
	return reg_show(kobj, attr, buf, PULSE_CNT_REG_OFFSET);
}

static ssize_t pulse_cnt_reg_store(struct kobject *kobj,
				   struct kobj_attribute *attr,
				   const char *buf,
				   size_t count)
{
	return reg_store(kobj, attr, buf, count, PULSE_CNT_REG_OFFSET);
}

static struct kobj_attribute pulse_cnt_reg_attribute =
	__ATTR(pulse_cnt_reg, 0666, pulse_cnt_reg_show, pulse_cnt_reg_store);

static ssize_t clk_cnt_reg_show(struct kobject *kobj,
				struct kobj_attribute *attr,
				char *buf)
{
	return reg_show(kobj, attr, buf, CLK_CNT_REG_OFFSET);
}

static ssize_t clk_cnt_reg_store(struct kobject *kobj,
				 struct kobj_attribute *attr,
				 const char *buf,
				 size_t count)
{
	return reg_store(kobj, attr, buf, count, CLK_CNT_REG_OFFSET);
}

static struct kobj_attribute clk_cnt_reg_attribute =
	__ATTR(clock_cnt_reg, 0666, clk_cnt_reg_show, clk_cnt_reg_store);

static ssize_t clk_cnt_snapshot_reg_show(struct kobject *kobj,
					 struct kobj_attribute *attr,
					 char *buf)
{
	return reg_show(kobj, attr, buf, CLK_CNT_SNAPSHOT_REG_OFFSET);
}

static struct kobj_attribute clk_cnt_snapshot_reg_attribute =
	__ATTR(clock_cnt_snapshot_reg, 0444, clk_cnt_snapshot_reg_show, NULL);

static struct attribute *wallclk_attrs[] = {
	&sfn_attribute.attr,
	&sfn_ref_attribute.attr,
	&ctrl_reg_attribute.attr,
	&pulse_cnt_reg_attribute.attr,
	&clk_cnt_snapshot_reg_attribute.attr,
	&clk_cnt_reg_attribute.attr,
	&basetime0_reg_attribute.attr,
	&basetime1_reg_attribute.attr,
	NULL
};

static struct attribute_group wallclk_attr_group = {
	.attrs = wallclk_attrs,
};

static int __init wallclk_sysfs_init(void)
{
	int rc;

	wallclk_kobj = kobject_create_and_add("wallclk", kernel_kobj);
	if (!wallclk_kobj) {
		printk(KERN_ERR "%s: failed to create kobject\n",
		       WALLCLK_SYSFS_MODULE_NAME);
		rc = -ENOMEM;
		goto out;
	}

	rc = sysfs_create_group(wallclk_kobj, &wallclk_attr_group);
	if (rc) {
		kobject_put(wallclk_kobj);
		printk(KERN_ERR "%s: failed to create sysfs group\n",
		       WALLCLK_SYSFS_MODULE_NAME);
	}

out:
	return rc;
}

static void __exit wallclk_sysfs_exit(void)
{
	kobject_put(wallclk_kobj);
}

module_init(wallclk_sysfs_init);
module_exit(wallclk_sysfs_exit);

MODULE_DESCRIPTION("Wall clock SysFS");
MODULE_LICENSE("GPL v2");
