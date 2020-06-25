/* Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
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

#include <linux/debugfs.h>

#include "ipa_eth_debugfs.h"

static struct dentry *ipa_eth_debugfs;

static struct dentry *ipa_eth_devices_debugfs;
static struct dentry *ipa_eth_drivers_debugfs;

static struct dentry *ipa_eth_bus_debugfs;
static struct dentry *ipa_eth_pci_debugfs;
static struct dentry *ipa_eth_offload_debugfs;

static ssize_t ipa_eth_dev_write_init(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret = debugfs_write_file_bool(file, user_buf, count, ppos);
	struct ipa_eth_device *eth_dev = container_of(file->private_data,
						      struct ipa_eth_device,
						      init);

	ipa_eth_device_refresh_sync(eth_dev);

	return ret;
}

static const struct file_operations fops_eth_dev_init = {
	.read = debugfs_read_file_bool,
	.write = ipa_eth_dev_write_init,
	.open = simple_open,
	.llseek = default_llseek,
};

static ssize_t ipa_eth_dev_write_start(struct file *file,
				       const char __user *user_buf,
				       size_t count, loff_t *ppos)
{
	ssize_t ret = debugfs_write_file_bool(file, user_buf, count, ppos);
	struct ipa_eth_device *eth_dev = container_of(file->private_data,
						      struct ipa_eth_device,
						      start);

	/* Set/reset timer to automatically start offload after the timeout
	 * specified in eth_dev->start_on_timeout (milliseconds) expires.
	 */
	if (!eth_dev->start && eth_dev->start_on_timeout)
		mod_timer(&eth_dev->start_timer,
			jiffies + msecs_to_jiffies(eth_dev->start_on_timeout));

	ipa_eth_device_refresh_sync(eth_dev);

	return ret;
}

static const struct file_operations fops_eth_dev_start = {
	.read = debugfs_read_file_bool,
	.write = ipa_eth_dev_write_start,
	.open = simple_open,
	.llseek = default_llseek,
};

static ssize_t eth_dev_stats_print_one(char *buf, const size_t size,
				   const char *dir, const char *link,
				   struct ipa_eth_offload_link_stats *stats)
{
	return scnprintf(buf, size,
			 "%10s%10s%10s%10llu%10llu%10llu%10llu\n",
			 dir, link, (stats->valid ? "yes" : "no"),
			 stats->events, stats->frames,
			 stats->packets, stats->octets);
}

static ssize_t eth_dev_stats_print(char *buf, const size_t size,
			       struct ipa_eth_device *eth_dev)
{
	ssize_t n = 0;
	struct ipa_eth_offload_stats stats;

	if (!eth_dev->od->ops->get_stats)
		return scnprintf(buf, size - n, "Not supported\n");

	memset(&stats, 0, sizeof(stats));

	if (eth_dev->od->ops->get_stats(eth_dev, &stats))
		return scnprintf(buf, size - n, "Operation failed\n");

	n += scnprintf(&buf[n], size - n,
		       "%10s%10s%10s%10s%10s%10s%10s\n",
		       "Dir", "Link", "Valid",
		       "Events", "Frames", "Packets", "Octets");

	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "rx", "ndev", &stats.rx.ndev);
	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "rx", "host", &stats.rx.host);
	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "rx", "uc", &stats.rx.uc);
	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "rx", "gsi", &stats.rx.gsi);
	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "rx", "ipa", &stats.rx.ipa);

	n += scnprintf(&buf[n], size - n, "\n");

	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "tx", "ndev", &stats.tx.ndev);
	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "tx", "host", &stats.tx.host);
	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "tx", "uc", &stats.tx.uc);
	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "tx", "gsi", &stats.tx.gsi);
	n += eth_dev_stats_print_one(&buf[n], size - n,
				     "tx", "ipa", &stats.tx.ipa);

	return n;
}

static ssize_t eth_dev_stats_read(struct file *file, char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	ssize_t n = 0, size = 2048;
	char *buf = NULL;
	struct ipa_eth_device *eth_dev = file->private_data;

	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL)
		return 0;

	n = eth_dev_stats_print(buf, size, eth_dev);
	n = simple_read_from_buffer(user_buf, count, ppos, buf, n);

	kfree(buf);

	return n;
}

static const struct file_operations fops_eth_dev_stats = {
	.read = eth_dev_stats_read,
	.open = simple_open,
	.llseek = default_llseek,
};

int ipa_eth_debugfs_add_device(struct ipa_eth_device *eth_dev)
{
	eth_dev->debugfs = debugfs_create_dir(eth_dev->net_dev->name,
					      ipa_eth_devices_debugfs);
	if (IS_ERR_OR_NULL(eth_dev->debugfs)) {
		ipa_eth_dev_err(eth_dev, "Failed to create debugfs root");
		return -EFAULT;
	}

	debugfs_create_file("init", 0644, eth_dev->debugfs, &eth_dev->init,
			    &fops_eth_dev_init);

	debugfs_create_file("start", 0644, eth_dev->debugfs, &eth_dev->start,
			    &fops_eth_dev_start);

	debugfs_create_bool("start_on_wakeup", 0644,
			    eth_dev->debugfs, &eth_dev->start_on_wakeup);

	debugfs_create_bool("start_on_resume", 0644,
			    eth_dev->debugfs, &eth_dev->start_on_resume);

	debugfs_create_u32("start_on_timeout", 0644, eth_dev->debugfs,
			    &eth_dev->start_on_timeout);

	debugfs_create_file("stats", 0644, eth_dev->debugfs, eth_dev,
			    &fops_eth_dev_stats);

	return 0;
}

void ipa_eth_debugfs_remove_device(struct ipa_eth_device *eth_dev)
{
	debugfs_remove_recursive(eth_dev->debugfs);
	eth_dev->debugfs = NULL;
}

int ipa_eth_debugfs_add_offload_driver(struct ipa_eth_offload_driver *od)
{
	if (!od->debugfs && ipa_eth_offload_debugfs) {
		od->debugfs =
			debugfs_create_dir(od->name, ipa_eth_offload_debugfs);
		if (!IS_ERR_OR_NULL(od->debugfs))
			return 0;
	}

	return -EFAULT;
}

void ipa_eth_debugfs_remove_offload_driver(struct ipa_eth_offload_driver *od)
{
	debugfs_remove_recursive(od->debugfs);
	od->debugfs = NULL;
}

static ssize_t eth_dev_ready_read(struct file *file, char __user *user_buf,
				  size_t count, loff_t *ppos)
{
	char *buf;
	ssize_t n = 0, size = 128;

	buf = kzalloc(size, GFP_KERNEL);
	if (buf == NULL)
		return 0;

	n += scnprintf(&buf[n], size - n, "Offload Sub-system: %s\n",
		test_bit(IPA_ETH_ST_READY, &ipa_eth_state) ?
			"Ready" : "Not Ready");

	n += scnprintf(&buf[n], size - n, "uC: %s\n",
		test_bit(IPA_ETH_ST_UC_READY, &ipa_eth_state) ?
			"Ready" : "Not Ready");

	n += scnprintf(&buf[n], size - n, "IPA: %s\n",
		test_bit(IPA_ETH_ST_IPA_READY, &ipa_eth_state) ?
			"Ready" : "Not Ready");

	n += scnprintf(&buf[n], size - n, "ALL: %s\n",
		ipa_eth_all_ready() ? "Ready" : "Not Ready");

	n = simple_read_from_buffer(user_buf, count, ppos, buf, n);

	kfree(buf);

	return n;
}

static const struct file_operations fops_eth_dev_ready = {
	.read = eth_dev_ready_read,
	.open = simple_open,
	.llseek = default_llseek,
};

int ipa_eth_debugfs_init(void)
{
	struct dentry *ipa_debugfs = ipa_debugfs_get_root();

	if (IS_ERR_OR_NULL(ipa_debugfs))
		return -EFAULT;

	ipa_eth_debugfs =
		debugfs_create_dir("ethernet", ipa_debugfs);
	if (IS_ERR_OR_NULL(ipa_eth_debugfs)) {
		ipa_eth_log("Unable to create debugfs root");
		goto err_exit;
	}

	(void) debugfs_create_file("ready", 0644, ipa_eth_debugfs, NULL,
			    &fops_eth_dev_ready);

	(void) debugfs_create_bool("no_auto", 0644,
				   ipa_eth_debugfs, &ipa_eth_noauto);

	(void) debugfs_create_bool("ipc_logdbg", 0644,
				   ipa_eth_debugfs, &ipa_eth_ipc_logdbg);

	ipa_eth_devices_debugfs =
		debugfs_create_dir("devices", ipa_eth_debugfs);
	if (IS_ERR_OR_NULL(ipa_eth_devices_debugfs)) {
		ipa_eth_log("Unable to create debugfs root for devices");
		goto err_exit;
	}

	ipa_eth_drivers_debugfs =
		debugfs_create_dir("drivers", ipa_eth_debugfs);
	if (IS_ERR_OR_NULL(ipa_eth_drivers_debugfs)) {
		ipa_eth_log("Unable to create debugfs root for drivers");
		goto err_exit;
	}

	ipa_eth_bus_debugfs =
		debugfs_create_dir("bus", ipa_eth_drivers_debugfs);
	if (IS_ERR_OR_NULL(ipa_eth_bus_debugfs)) {
		ipa_eth_log("Unable to create debugfs root for bus");
		goto err_exit;
	}

	ipa_eth_pci_debugfs = debugfs_create_dir("pci", ipa_eth_bus_debugfs);
	if (IS_ERR_OR_NULL(ipa_eth_pci_debugfs)) {
		ipa_eth_log("Unable to create debugfs root for pci");
		goto err_exit;
	}

	ipa_eth_offload_debugfs =
		debugfs_create_dir("offload", ipa_eth_drivers_debugfs);
	if (IS_ERR_OR_NULL(ipa_eth_offload_debugfs)) {
		ipa_eth_log("Unable to create debugfs root for offload");
		goto err_exit;
	}

	ipa_eth_log("Debugfs root is initialized");

	return 0;

err_exit:
	ipa_eth_debugfs_cleanup();
	return -EFAULT;
}

void ipa_eth_debugfs_cleanup(void)
{
	debugfs_remove_recursive(ipa_eth_debugfs);
}
