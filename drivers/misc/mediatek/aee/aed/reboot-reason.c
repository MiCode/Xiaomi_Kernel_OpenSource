// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <asm/memory.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/sysfs.h>

#include <mt-plat/aee.h>
#include <mt-plat/mboot_params.h>
#include <mt-plat/mrdump.h>
#include "aed.h"

#include <asm/setup.h>

#define RR_PROC_NAME "reboot-reason"

static struct proc_dir_entry *aee_rr_file;

/* define /sys/bootinfo/powerup_reason */
static char boot_reason[COMMAND_LINE_SIZE];

static ssize_t powerup_reason_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, sizeof(boot_reason), "%s\n", boot_reason);
}

static struct kobj_attribute powerup_reason_attr = __ATTR_RO(powerup_reason);

struct kobject *bootinfo_kobj;
EXPORT_SYMBOL(bootinfo_kobj);

static struct attribute *bootinfo_attrs[] = {
	&powerup_reason_attr.attr,
	NULL
};

static struct attribute_group bootinfo_attr_group = {
	.attrs = bootinfo_attrs,
};

int ksysfs_bootinfo_init(void)
{
	int error;

	pr_notice("ksysfs_bootinfo_init boot_reason:%s\n", boot_reason);

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (!bootinfo_kobj){
		pr_notice("ksysfs_bootinfo_init kobject_create_and_add error\n");
		return -ENOMEM;
	}

	error = sysfs_create_group(bootinfo_kobj, &bootinfo_attr_group);
	if (error){
		pr_notice("ksysfs_bootinfo_init sysfs_create_group error\n");
		kobject_put(bootinfo_kobj);
	}

	return error;
}

void ksysfs_bootinfo_exit(void)
{
	kobject_put(bootinfo_kobj);
}

module_param_string(bootreason, boot_reason, sizeof(boot_reason), 0644);
/* end sysfs bootinfo */

static int aee_rr_reboot_reason_proc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, aee_rr_reboot_reason_show, NULL);
}

static const struct proc_ops aee_rr_reboot_reason_proc_fops = {
	.proc_open = aee_rr_reboot_reason_proc_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};


void aee_rr_proc_init(struct proc_dir_entry *aed_proc_dir)
{
	ksysfs_bootinfo_init();
	aee_rr_file = proc_create(RR_PROC_NAME, 0440, aed_proc_dir,
			&aee_rr_reboot_reason_proc_fops);
	if (!aee_rr_file)
		pr_notice("%s: Can't create rr proc entry\n", __func__);
}

void aee_rr_proc_done(struct proc_dir_entry *aed_proc_dir)
{
	ksysfs_bootinfo_exit();
	remove_proc_entry(RR_PROC_NAME, aed_proc_dir);
}
