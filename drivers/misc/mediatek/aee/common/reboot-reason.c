/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <asm/memory.h>
#ifdef CONFIG_MTK_WATCHDOG
#include <mtk_wd_api.h>
#endif
#ifdef CONFIG_MTK_RAM_CONSOLE
#include <mt-plat/mtk_ram_console.h>
#endif
#include <mt-plat/aee.h>
#include "aee-common.h"

#define RR_PROC_NAME "reboot-reason"

static struct proc_dir_entry *aee_rr_file;

#define WDT_NORMAL_BOOT 0
#define WDT_HW_REBOOT 1
#define WDT_SW_REBOOT 2

enum boot_reason_t {
	BR_POWER_KEY = 0,
	BR_USB,
	BR_RTC,
	BR_WDT,
	BR_WDT_BY_PASS_PWK,
	BR_TOOL_BY_PASS_PWK,
	BR_2SEC_REBOOT,
	BR_UNKNOWN,
	BR_KERNEL_PANIC,
	BR_WDT_SW,
	BR_WDT_HW
};

#define REBOOT_REASON_LEN	16
char boot_reason[][REBOOT_REASON_LEN] = { "keypad", "usb_chg", "rtc", "wdt",
	"reboot", "tool reboot", "smpl", "others", "kpanic", "wdt_sw",
	"wdt_hw" };

int __weak aee_rr_reboot_reason_show(struct seq_file *m, void *v)
{
	seq_puts(m, "mtk_ram_console not enabled.");
	return 0;
}

static int aee_rr_reboot_reason_proc_open(struct inode *inode,
		struct file *file)
{
	return single_open(file, aee_rr_reboot_reason_show, NULL);
}

static const struct file_operations aee_rr_reboot_reason_proc_fops = {
	.open = aee_rr_reboot_reason_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = single_release,
};


void aee_rr_proc_init(struct proc_dir_entry *aed_proc_dir)
{
	aee_rr_file = proc_create(RR_PROC_NAME, 0440, aed_proc_dir,
			&aee_rr_reboot_reason_proc_fops);
	if (aee_rr_file == NULL)
		pr_notice("%s: Can't create rr proc entry\n", __func__);
}
EXPORT_SYMBOL(aee_rr_proc_init);

void aee_rr_proc_done(struct proc_dir_entry *aed_proc_dir)
{
	remove_proc_entry(RR_PROC_NAME, aed_proc_dir);
}
EXPORT_SYMBOL(aee_rr_proc_done);

/* define /sys/bootinfo/powerup_reason */
static ssize_t powerup_reason_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	char boot_reason[64];
	char *br_ptr;
	char *br_ptr_e;

	memset(boot_reason, 0x0, 64);
	br_ptr = strstr(saved_command_line, "androidboot.bootreason=");
	if (br_ptr != 0) {
		br_ptr_e = strstr(br_ptr, " ");
		/* get boot reason */
		if (br_ptr_e != 0) {
			strncpy(boot_reason, br_ptr + 23,
					br_ptr_e - br_ptr - 23);
			boot_reason[br_ptr_e - br_ptr - 23] = '\0';
		}
#ifdef CONFIG_MTK_RAM_CONSOLE
		if (aee_rr_last_fiq_step() != 0)
			strncpy(boot_reason, "kpanic", 7);
#endif
		if (!strncmp(boot_reason, "2sec_reboot",
					strlen("2sec_reboot"))) {
			br_ptr = strstr(saved_command_line,
					"has_battery_removed=1");
			if (br_ptr == NULL)
				return snprintf(buf, sizeof(boot_reason),
						"%s_abnormal\n", boot_reason);
		}
		return snprintf(buf, sizeof(boot_reason), "%s\n", boot_reason);
	} else
		return 0;

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

	bootinfo_kobj = kobject_create_and_add("bootinfo", NULL);
	if (!bootinfo_kobj)
		return -ENOMEM;

	error = sysfs_create_group(bootinfo_kobj, &bootinfo_attr_group);
	if (error)
		kobject_put(bootinfo_kobj);

	return error;
}

void ksysfs_bootinfo_exit(void)
{
	kobject_put(bootinfo_kobj);
}

/* end sysfs bootinfo */
