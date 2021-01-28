// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <asm/memory.h>
#include <asm/setup.h>

#include <linux/init.h>
#include <linux/kobject.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/uaccess.h>

#if IS_ENABLED(CONFIG_MTK_WATCHDOG)
#include <mtk_wd_api.h>
#endif
#include <mt-plat/aee.h>
#include <mt-plat/mboot_params.h>
#include <mt-plat/mrdump.h>
#include "aed.h"

#define RR_PROC_NAME "reboot-reason"

static struct proc_dir_entry *aee_rr_file;

static char aee_cmdline[COMMAND_LINE_SIZE];

static const char *mrdump_get_cmd(void)
{
	struct file *fd;
	mm_segment_t fs;
	loff_t pos = 0;

	if (aee_cmdline[0] != 0)
		return aee_cmdline;

	fs = get_fs();
	set_fs(KERNEL_DS);
	fd = filp_open("/proc/cmdline", O_RDONLY, 0);
	if (IS_ERR(fd)) {
		pr_info("kedump: Unable to open /proc/cmdline (%ld)",
			PTR_ERR(fd));
		set_fs(fs);
		return aee_cmdline;
	}
	vfs_read(fd, (void *)aee_cmdline, COMMAND_LINE_SIZE, &pos);
	filp_close(fd, NULL);
	fd = NULL;
	set_fs(fs);
	return aee_cmdline;
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
	if (!aee_rr_file)
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
	br_ptr = strstr(mrdump_get_cmd(), "androidboot.bootreason=");
	if (br_ptr) {
		br_ptr_e = strstr(br_ptr, " ");
		/* get boot reason */
		if (br_ptr_e) {
			strncpy(boot_reason, br_ptr + 23,
					br_ptr_e - br_ptr - 23);
			boot_reason[br_ptr_e - br_ptr - 23] = '\0';
		}
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
		if (aee_rr_last_fiq_step())
			strncpy(boot_reason, "kpanic", 7);
#endif
		if (!strncmp(boot_reason, "2sec_reboot",
					strlen("2sec_reboot"))) {
			br_ptr = strstr(mrdump_get_cmd(),
					"has_battery_removed=1");
			if (!br_ptr)
				return snprintf(buf, sizeof(boot_reason),
						"%s_abnormal\n", boot_reason);
		}
		return snprintf(buf, sizeof(boot_reason), "%s\n", boot_reason);
	} else {
		return 0;
	}
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
