/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/mutex.h>
#include <soc/qcom/boot_stats.h>

#define MAX_STRING_LEN 256
#define BOOT_MARKER_MAX_LEN 21
static struct dentry *dent_bkpi, *dent_bkpi_status;
static struct boot_marker boot_marker_list;

struct boot_marker {
	char marker_name[BOOT_MARKER_MAX_LEN];
	unsigned long long int timer_value;
	struct list_head list;
	struct mutex lock;
};

static void _create_boot_marker(const char *name,
					unsigned long long int timer_value)
{
	struct boot_marker *new_boot_marker;

	pr_debug("%-22s:%llu.%03llu seconds\n", name,
		timer_value/TIMER_KHZ,
		((timer_value % TIMER_KHZ)
		* 1000) / TIMER_KHZ);

	new_boot_marker = kmalloc(sizeof(*new_boot_marker), GFP_KERNEL);
	if (!new_boot_marker)
		return;

	strlcpy(new_boot_marker->marker_name, name,
		sizeof(new_boot_marker->marker_name));
	new_boot_marker->timer_value = timer_value;

	mutex_lock(&boot_marker_list.lock);
	list_add_tail(&(new_boot_marker->list), &(boot_marker_list.list));
	mutex_unlock(&boot_marker_list.lock);
}

static void set_bootloader_stats(void)
{
	_create_boot_marker("Lk Start - ",
			readl_relaxed(&boot_stats->bootloader_start));
	_create_boot_marker("Lk End - ",
			readl_relaxed(&boot_stats->bootloader_end));
	_create_boot_marker("Lk Display - ",
			readl_relaxed(&boot_stats->bootloader_display));
	_create_boot_marker("Lk Load Kernel - ",
			readl_relaxed(&boot_stats->bootloader_load_kernel));
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

	mutex_lock(&boot_marker_list.lock);
	list_for_each_entry(marker, &boot_marker_list.list, list) {
		temp += scnprintf(buf + temp, PAGE_SIZE - temp,
			"%-22s:%llu.%03llu seconds\n",
			marker->marker_name,
			marker->timer_value/TIMER_KHZ,
			(((marker->timer_value % TIMER_KHZ)
			* 1000) / TIMER_KHZ));
	}
	mutex_unlock(&boot_marker_list.lock);
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

static int __init init_bootkpi(void)
{
	dent_bkpi = debugfs_create_dir("bootkpi", NULL);
	if (IS_ERR_OR_NULL(dent_bkpi))
		return -ENODEV;

	dent_bkpi_status = debugfs_create_file("kpi_values",
		(S_IRUGO|S_IWUGO), dent_bkpi, 0, &fops_bkpi);
	if (IS_ERR_OR_NULL(dent_bkpi_status)) {
		debugfs_remove(dent_bkpi);
		dent_bkpi = NULL;
		pr_err("boot_marker: Could not create 'kpi_values' debugfs file\n");
		return -ENODEV;
	}

	INIT_LIST_HEAD(&boot_marker_list.list);
	mutex_init(&boot_marker_list.lock);
	set_bootloader_stats();
	return 0;
}
subsys_initcall(init_bootkpi);

static void __exit exit_bootkpi(void)
{
	struct boot_marker *marker;
	struct boot_marker *temp_addr;

	debugfs_remove_recursive(dent_bkpi);
	mutex_lock(&boot_marker_list.lock);
	list_for_each_entry_safe(marker, temp_addr, &boot_marker_list.list,
					list) {
		list_del(&marker->list);
		kfree(marker);
	}
	mutex_unlock(&boot_marker_list.lock);
	boot_stats_exit();
}
module_exit(exit_bootkpi);

MODULE_DESCRIPTION("MSM boot key performance indicators");
MODULE_LICENSE("GPL v2");
