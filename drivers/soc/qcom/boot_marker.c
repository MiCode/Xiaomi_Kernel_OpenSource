/* Copyright (c) 2016, 2019, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <linux/io.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/list.h>
#include <linux/export.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/mm.h>
#include <soc/qcom/boot_stats.h>

#define MAX_STRING_LEN 256
#define BOOT_MARKER_MAX_LEN 40

struct boot_marker {
	char marker_name[BOOT_MARKER_MAX_LEN];
	unsigned long long int timer_value;
	struct list_head list;
	spinlock_t slock;
};

static struct dentry *dent_bkpi, *dent_bkpi_status, *dent_mpm_timer;
static struct boot_marker boot_marker_list;

static void _create_boot_marker(const char *name,
		unsigned long long int timer_value)
{
	struct boot_marker *new_boot_marker;

	pr_debug("%-41s:%llu.%03llu seconds\n", name,
			timer_value/TIMER_KHZ,
			((timer_value % TIMER_KHZ)
			 * 1000) / TIMER_KHZ);

	new_boot_marker = kmalloc(sizeof(*new_boot_marker), GFP_ATOMIC);
	if (!new_boot_marker)
		return;

	strlcpy(new_boot_marker->marker_name, name,
			sizeof(new_boot_marker->marker_name));
	new_boot_marker->timer_value = timer_value;

	spin_lock(&boot_marker_list.slock);
	list_add_tail(&(new_boot_marker->list), &(boot_marker_list.list));
	spin_unlock(&boot_marker_list.slock);
}

static void set_bootloader_stats(void)
{
	_create_boot_marker("M - APPSBL Start - ",
		readl_relaxed(&boot_stats->bootloader_start));
	_create_boot_marker("M - APPSBL Display Init - ",
		readl_relaxed(&boot_stats->bootloader_display));
	_create_boot_marker("M - APPSBL Early-Domain Start - ",
		readl_relaxed(&boot_stats->bootloader_early_domain_start));
	_create_boot_marker("D - APPSBL Kernel Load Time - ",
		readl_relaxed(&boot_stats->bootloader_load_kernel));
	_create_boot_marker("D - APPSBL Kernel Auth Time - ",
		readl_relaxed(&boot_stats->bootloader_checksum));
	_create_boot_marker("M - APPSBL End - ",
		readl_relaxed(&boot_stats->bootloader_end));
}

void place_marker(const char *name)
{
	_create_boot_marker((char *) name, msm_timer_get_sclk_ticks());
}
EXPORT_SYMBOL(place_marker);

static ssize_t bootkpi_reader(struct file *fp, char __user *user_buffer,
		size_t count, loff_t *position)
{
	int rc = 0;
	char *buf;
	int temp = 0;
	struct boot_marker *marker;

	buf = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	spin_lock(&boot_marker_list.slock);
	list_for_each_entry(marker, &boot_marker_list.list, list) {
		temp += scnprintf(buf + temp, PAGE_SIZE - temp,
				"%-41s:%llu.%03llu seconds\n",
				marker->marker_name,
				marker->timer_value/TIMER_KHZ,
				(((marker->timer_value % TIMER_KHZ)
				  * 1000) / TIMER_KHZ));
	}
	spin_unlock(&boot_marker_list.slock);
	rc = simple_read_from_buffer(user_buffer, count, position, buf, temp);
	kfree(buf);
	return rc;
}

static ssize_t bootkpi_writer(struct file *fp, const char __user *user_buffer,
		size_t count, loff_t *position)
{
	int rc = 0;
	char buf[MAX_STRING_LEN];

	if (count > MAX_STRING_LEN)
		return -EINVAL;
	rc = simple_write_to_buffer(buf,
			sizeof(buf) - 1, position, user_buffer, count);
	if (rc < 0)
		return rc;
	buf[rc] = '\0';
	place_marker(buf);
	return rc;
}

static int bootkpi_open(struct inode *inode, struct file *file)
{
	return 0;
}

static const struct file_operations fops_bkpi = {
	.owner = THIS_MODULE,
	.open  = bootkpi_open,
	.read  = bootkpi_reader,
	.write = bootkpi_writer,
};

static ssize_t mpm_timer_read(struct file *fp, char __user *user_buffer,
		size_t count, loff_t *position)
{
	unsigned long long int timer_value;
	int rc = 0;
	char buf[100];
	int temp = 0;

	timer_value = msm_timer_get_sclk_ticks();

	temp = scnprintf(buf, sizeof(buf), "%llu.%03llu seconds\n",
			timer_value/TIMER_KHZ,
			(((timer_value % TIMER_KHZ) * 1000) / TIMER_KHZ));

	rc = simple_read_from_buffer(user_buffer, count, position, buf, temp);

	return rc;
}

static int mpm_timer_open(struct inode *inode, struct file *file)
{
	return 0;
}

static int mpm_timer_mmap(struct file *file, struct vm_area_struct *vma)
{
	phys_addr_t addr = msm_timer_get_pa();

	if (vma->vm_flags & VM_WRITE)
		return -EPERM;

	vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);
	return vm_iomap_memory(vma, addr, PAGE_SIZE);
}

static const struct file_operations fops_mpm_timer = {
	.owner = THIS_MODULE,
	.open  = mpm_timer_open,
	.read  = mpm_timer_read,
	.mmap = mpm_timer_mmap,
};

static int __init init_bootkpi(void)
{
	dent_bkpi = debugfs_create_dir("bootkpi", NULL);
	if (IS_ERR_OR_NULL(dent_bkpi))
		return -ENODEV;

	dent_bkpi_status = debugfs_create_file_unsafe("kpi_values",
			0666, dent_bkpi, NULL, &fops_bkpi);
	if (IS_ERR_OR_NULL(dent_bkpi_status)) {
		debugfs_remove(dent_bkpi);
		dent_bkpi = NULL;
		pr_err("boot_marker: Could not create 'kpi_values' debugfs file\n");
		return -ENODEV;
	}

	dent_mpm_timer = debugfs_create_file("mpm_timer",
			0444, dent_bkpi, NULL, &fops_mpm_timer);
	if (IS_ERR_OR_NULL(dent_mpm_timer)) {
		debugfs_remove(dent_bkpi_status);
		dent_bkpi_status = NULL;
		debugfs_remove(dent_bkpi);
		dent_bkpi = NULL;
		pr_err("boot_marker: Could not create 'mpm_timer' debugfs file\n");
		return -ENODEV;
	}

	INIT_LIST_HEAD(&boot_marker_list.list);
	spin_lock_init(&boot_marker_list.slock);
	set_bootloader_stats();
	return 0;
}
subsys_initcall(init_bootkpi);

static void __exit exit_bootkpi(void)
{
	struct boot_marker *marker;
	struct boot_marker *temp_addr;

	debugfs_remove_recursive(dent_bkpi);
	spin_lock(&boot_marker_list.slock);
	list_for_each_entry_safe(marker, temp_addr, &boot_marker_list.list,
			list) {
		list_del(&marker->list);
		kfree(marker);
	}
	spin_unlock(&boot_marker_list.slock);
	boot_stats_exit();
}
module_exit(exit_bootkpi);

MODULE_DESCRIPTION("MSM boot key performance indicators");
MODULE_LICENSE("GPL v2");
