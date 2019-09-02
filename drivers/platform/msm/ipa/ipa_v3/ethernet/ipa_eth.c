/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include <linux/printk.h>
#include <linux/debugfs.h>
#include <linux/module.h>
#include <linux/suspend.h>
#include <linux/timer.h>

#include <linux/msm_ipa.h>

#include "ipa_eth_i.h"

enum ipa_eth_states {
	IPA_ETH_ST_READY,
	IPA_ETH_ST_UC_READY,
	IPA_ETH_ST_IPA_READY,
	IPA_ETH_ST_MAX,
};

enum ipa_eth_dev_states {
	IPA_ETH_DEV_ST_UNPAIRING,
	IPA_ETH_DEV_ST_MAX,
};

static unsigned long ipa_eth_state;

static struct dentry *ipa_eth_debugfs;
static struct dentry *ipa_eth_drivers_debugfs;
static struct dentry *ipa_eth_devices_debugfs;

static LIST_HEAD(ipa_eth_devices);
static DEFINE_MUTEX(ipa_eth_devices_lock);

static bool ipa_eth_noauto = IPA_ETH_NOAUTO_DEFAULT;
module_param(ipa_eth_noauto, bool, 0444);
MODULE_PARM_DESC(ipa_eth_noauto,
	"Disable automatic offload initialization of interfaces");

static bool ipa_eth_ipc_logdbg = IPA_ETH_IPC_LOGDBG_DEFAULT;
module_param(ipa_eth_ipc_logdbg, bool, 0444);
MODULE_PARM_DESC(ipa_eth_ipc_logdbg, "Log debug IPC messages");

static struct workqueue_struct *ipa_eth_wq;

static inline bool ipa_eth_ready(void)
{
	return test_bit(IPA_ETH_ST_READY, &ipa_eth_state) &&
		test_bit(IPA_ETH_ST_UC_READY, &ipa_eth_state) &&
		test_bit(IPA_ETH_ST_IPA_READY, &ipa_eth_state);
}

static inline bool initable(struct ipa_eth_device *eth_dev)
{
	return !test_bit(IPA_ETH_DEV_ST_UNPAIRING, &eth_dev->state) &&
		eth_dev->init;
}

static inline bool startable(struct ipa_eth_device *eth_dev)
{
	return !test_bit(IPA_ETH_DEV_ST_UNPAIRING, &eth_dev->state) &&
		eth_dev->init &&
		eth_dev->start &&
		test_bit(IPA_ETH_IF_ST_LOWER_UP, &eth_dev->if_state);
}

static int ipa_eth_init_device(struct ipa_eth_device *eth_dev)
{
	int rc;

	if (eth_dev->of_state == IPA_ETH_OF_ST_INITED)
		return 0;

	if (eth_dev->of_state != IPA_ETH_OF_ST_DEINITED)
		return -EFAULT;

	rc = ipa_eth_ep_init_headers(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to init EP headers");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_pm_register(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register with IPA PM");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_offload_init(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to init offload");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_ep_register_interface(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register EP interface");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	ipa_eth_dev_log(eth_dev, "Initialized device");

	eth_dev->of_state = IPA_ETH_OF_ST_INITED;

	return 0;
}

static int ipa_eth_deinit_device(struct ipa_eth_device *eth_dev)
{
	int rc;

	if (eth_dev->of_state == IPA_ETH_OF_ST_DEINITED)
		return 0;

	if (eth_dev->of_state != IPA_ETH_OF_ST_INITED)
		return -EFAULT;

	rc = ipa_eth_ep_unregister_interface(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to unregister IPA interface");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_offload_deinit(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to deinit offload");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_pm_unregister(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to unregister with IPA PM");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	ipa_eth_dev_log(eth_dev, "Deinitialized device");

	eth_dev->of_state = IPA_ETH_OF_ST_DEINITED;

	return 0;
}

static void ipa_eth_free_msg(void *buff, u32 len, u32 type) {}

static int ipa_eth_start_device(struct ipa_eth_device *eth_dev)
{
	int rc;
	struct ipa_msg_meta msg_meta;
	struct ipa_ecm_msg ecm_msg;

	if (eth_dev->of_state == IPA_ETH_OF_ST_STARTED)
		return 0;

	if (eth_dev->of_state != IPA_ETH_OF_ST_INITED)
		return -EFAULT;

	rc = ipa_eth_pm_activate(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to activate device PM");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_offload_start(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to start offload");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_bus_disable_pc(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to disable bus power collapse");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	memset(&msg_meta, 0, sizeof(msg_meta));
	memset(&ecm_msg, 0, sizeof(ecm_msg));

	ecm_msg.ifindex = eth_dev->net_dev->ifindex;
	strlcpy(ecm_msg.name, eth_dev->net_dev->name, IPA_RESOURCE_NAME_MAX);

	msg_meta.msg_type = ECM_CONNECT;
	msg_meta.msg_len = sizeof(struct ipa_ecm_msg);
	(void) ipa_send_msg(&msg_meta, &ecm_msg, ipa_eth_free_msg);

	ipa_eth_dev_log(eth_dev, "Started device");

	eth_dev->of_state = IPA_ETH_OF_ST_STARTED;

	return 0;
}

static int ipa_eth_stop_device(struct ipa_eth_device *eth_dev)
{
	int rc;
	struct ipa_msg_meta msg_meta;
	struct ipa_ecm_msg ecm_msg;

	memset(&msg_meta, 0, sizeof(msg_meta));
	memset(&ecm_msg, 0, sizeof(ecm_msg));

	ecm_msg.ifindex = eth_dev->net_dev->ifindex;
	strlcpy(ecm_msg.name, eth_dev->net_dev->name, IPA_RESOURCE_NAME_MAX);

	msg_meta.msg_type = ECM_DISCONNECT;
	msg_meta.msg_len = sizeof(struct ipa_ecm_msg);
	(void) ipa_send_msg(&msg_meta, &ecm_msg, ipa_eth_free_msg);

	if (eth_dev->of_state == IPA_ETH_OF_ST_DEINITED)
		return 0;

	if (eth_dev->of_state != IPA_ETH_OF_ST_STARTED)
		return -EFAULT;

	rc = ipa_eth_bus_enable_pc(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev,
			"Failed to enable bus power collapse");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_offload_stop(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to stop offload");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	rc = ipa_eth_pm_deactivate(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to deactivate device PM");
		eth_dev->of_state = IPA_ETH_OF_ST_ERROR;
		return rc;
	}

	ipa_eth_dev_log(eth_dev, "Stopped device");

	eth_dev->of_state = IPA_ETH_OF_ST_INITED;

	return 0;
}

static void __ipa_eth_refresh_device(struct work_struct *work)
{
	struct ipa_eth_device *eth_dev = container_of(work,
				struct ipa_eth_device, refresh);

	ipa_eth_dev_log(eth_dev, "Refreshing offload state for device");

	if (!ipa_eth_offload_device_paired(eth_dev)) {
		ipa_eth_dev_log(eth_dev, "Device is not paired. Skipping.");
		return;
	}

	if (eth_dev->of_state == IPA_ETH_OF_ST_ERROR) {
		ipa_eth_dev_err(eth_dev,
				"Device in ERROR state, skipping refresh");
		return;
	}

	if (initable(eth_dev)) {
		if (eth_dev->of_state == IPA_ETH_OF_ST_DEINITED) {
			IPA_ACTIVE_CLIENTS_INC_SIMPLE();
			(void) ipa_eth_init_device(eth_dev);
			IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

			if (eth_dev->of_state != IPA_ETH_OF_ST_INITED) {
				ipa_eth_dev_err(eth_dev,
						"Failed to init device");
				return;
			}
		}
	}

	if (startable(eth_dev)) {
		IPA_ACTIVE_CLIENTS_INC_SIMPLE();
		(void) ipa_eth_start_device(eth_dev);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

		if (eth_dev->of_state != IPA_ETH_OF_ST_STARTED) {
			ipa_eth_dev_err(eth_dev, "Failed to start device");
			return;
		}

		if (ipa_eth_pm_vote_bw(eth_dev))
			ipa_eth_dev_err(eth_dev,
					"Failed to vote for required BW");
	} else {
		ipa_eth_dev_log(eth_dev, "Start is disallowed for the device");

		if (eth_dev->of_state == IPA_ETH_OF_ST_STARTED) {
			IPA_ACTIVE_CLIENTS_INC_SIMPLE();
			ipa_eth_stop_device(eth_dev);
			IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

			if (eth_dev->of_state != IPA_ETH_OF_ST_INITED) {
				ipa_eth_dev_err(eth_dev,
						"Failed to stop device");
				return;
			}
		}
	}

	if (!initable(eth_dev)) {
		ipa_eth_dev_log(eth_dev, "Init is disallowed for the device");

		IPA_ACTIVE_CLIENTS_INC_SIMPLE();
		ipa_eth_deinit_device(eth_dev);
		IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

		if (eth_dev->of_state != IPA_ETH_OF_ST_DEINITED) {
			ipa_eth_dev_err(eth_dev, "Failed to deinit device");
			return;
		}
	}
}

static void ipa_eth_refresh_device(struct ipa_eth_device *eth_dev)
{
	if (ipa_eth_ready())
		queue_work(ipa_eth_wq, &eth_dev->refresh);
}

static void __ipa_eth_refresh_devices(struct work_struct *work)
{
	struct ipa_eth_device *eth_dev;

	ipa_eth_log("Performing global refresh");

	mutex_lock(&ipa_eth_devices_lock);

	if (ipa_eth_ready()) {
		list_for_each_entry(eth_dev, &ipa_eth_devices, device_list) {
			ipa_eth_refresh_device(eth_dev);
		}
	}

	mutex_unlock(&ipa_eth_devices_lock);
}

static DECLARE_WORK(global_refresh, __ipa_eth_refresh_devices);

static void ipa_eth_refresh_devices(void)
{
	queue_work(ipa_eth_wq, &global_refresh);
}

static void ipa_eth_dev_start_timer_cb(unsigned long data)
{
	struct ipa_eth_device *eth_dev = (struct ipa_eth_device *)data;

	/* Do not start offload if user disabled start_on_timeout in between */
	if (eth_dev && eth_dev->start_on_timeout)
		eth_dev->start = true;

	ipa_eth_refresh_device(eth_dev);
}

static int __ipa_eth_netdev_event(struct ipa_eth_device *eth_dev)
{
	bool refresh_needed = netif_carrier_ok(eth_dev->net_dev) ?
		!test_and_set_bit(IPA_ETH_IF_ST_LOWER_UP, &eth_dev->if_state) :
		test_and_clear_bit(IPA_ETH_IF_ST_LOWER_UP, &eth_dev->if_state);

	if (refresh_needed)
		ipa_eth_refresh_device(eth_dev);

	return NOTIFY_DONE;
}

static int ipa_eth_netdev_event(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct net_device *net_dev = netdev_notifier_info_to_dev(ptr);

	struct ipa_eth_device *eth_dev = container_of(nb,
				struct ipa_eth_device, netdevice_nb);

	if (net_dev != eth_dev->net_dev)
		return NOTIFY_DONE;

	ipa_eth_dev_log(eth_dev, "Received netdev event 0x%04lx", event);

	return __ipa_eth_netdev_event(eth_dev);
}

static int ipa_eth_uc_ready_cb(struct notifier_block *nb,
	unsigned long action, void *data)
{
	ipa_eth_log("IPA uC is ready");

	set_bit(IPA_ETH_ST_UC_READY, &ipa_eth_state);

	ipa_eth_refresh_devices();

	return NOTIFY_OK;
}

static struct notifier_block uc_ready_cb = {
	.notifier_call = ipa_eth_uc_ready_cb,
};

static void ipa_eth_ipa_ready_cb(void *data)
{
	ipa_eth_log("IPA is ready");

	set_bit(IPA_ETH_ST_IPA_READY, &ipa_eth_state);

	ipa_eth_refresh_devices();
}

static ssize_t ipa_eth_dev_write_init(struct file *file,
				      const char __user *user_buf,
				      size_t count, loff_t *ppos)
{
	ssize_t ret = debugfs_write_file_bool(file, user_buf, count, ppos);
	struct ipa_eth_device *eth_dev = container_of(file->private_data,
						      struct ipa_eth_device,
						      init);

	ipa_eth_refresh_device(eth_dev);

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

	ipa_eth_refresh_device(eth_dev);

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

static int ipa_eth_device_debugfs_create(struct ipa_eth_device *eth_dev)
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

static void ipa_eth_device_debugfs_remove(struct ipa_eth_device *eth_dev)
{
	debugfs_remove_recursive(eth_dev->debugfs);
	eth_dev->debugfs = NULL;
}

static int __ipa_eth_pair_device(struct ipa_eth_device *eth_dev)
{
	int rc;

	if (ipa_eth_offload_device_paired(eth_dev)) {
		ipa_eth_dev_dbg(eth_dev, "Device already paired. Skipping.");
		return 0;
	}

	rc = ipa_eth_offload_pair_device(eth_dev);
	if (rc) {
		ipa_eth_dev_log(eth_dev, "Failed to pair device. Deferring.");
		return rc;
	}

	eth_dev->netdevice_nb.notifier_call = ipa_eth_netdev_event;
	rc = register_netdevice_notifier(&eth_dev->netdevice_nb);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to register netdev notifier");
		ipa_eth_offload_unpair_device(eth_dev);
		return rc;
	}

	(void) ipa_eth_device_debugfs_create(eth_dev);

	ipa_eth_dev_log(eth_dev, "Paired device with offload driver %s",
			eth_dev->od->name);

	return 0;
}

static void __ipa_eth_unpair_device(struct ipa_eth_device *eth_dev)
{
	if (!ipa_eth_offload_device_paired(eth_dev)) {
		ipa_eth_dev_dbg(eth_dev, "Device already unpaired. Skipping.");
		return;
	}

	ipa_eth_dev_log(eth_dev, "Unpairing device from offload driver %s",
			eth_dev->od->name);

	ipa_eth_device_debugfs_remove(eth_dev);

	eth_dev->start_on_wakeup = false;
	eth_dev->start_on_resume = false;
	eth_dev->start_on_timeout = 0;
	del_timer_sync(&eth_dev->start_timer);

	flush_work(&eth_dev->refresh);

	set_bit(IPA_ETH_DEV_ST_UNPAIRING, &eth_dev->state);

	ipa_eth_refresh_device(eth_dev);
	flush_work(&eth_dev->refresh);
	cancel_work_sync(&eth_dev->refresh);

	unregister_netdevice_notifier(&eth_dev->netdevice_nb);
	ipa_eth_offload_unpair_device(eth_dev);

	clear_bit(IPA_ETH_DEV_ST_UNPAIRING, &eth_dev->state);
}

static void ipa_eth_pair_devices(void)
{
	struct ipa_eth_device *eth_dev;

	mutex_lock(&ipa_eth_devices_lock);

	list_for_each_entry(eth_dev, &ipa_eth_devices, device_list)
		(void) __ipa_eth_pair_device(eth_dev);

	mutex_unlock(&ipa_eth_devices_lock);
}

static void ipa_eth_unpair_devices(struct ipa_eth_offload_driver *od)
{
	struct ipa_eth_device *eth_dev;

	mutex_lock(&ipa_eth_devices_lock);

	list_for_each_entry(eth_dev, &ipa_eth_devices, device_list) {
		if (eth_dev->od == od)
			__ipa_eth_unpair_device(eth_dev);
	}

	mutex_unlock(&ipa_eth_devices_lock);
}

int ipa_eth_register_device(struct ipa_eth_device *eth_dev)
{
	int rc;

	if (!eth_dev->dev) {
		ipa_eth_dev_err(eth_dev, "Device is NULL");
		return -EINVAL;
	}

	if (!eth_dev->nd) {
		ipa_eth_dev_err(eth_dev, "Network driver is NULL");
		return -EINVAL;
	}

	eth_dev->of_state = IPA_ETH_OF_ST_DEINITED;
	eth_dev->pm_handle = IPA_PM_MAX_CLIENTS;
	INIT_WORK(&eth_dev->refresh, __ipa_eth_refresh_device);

	INIT_LIST_HEAD(&eth_dev->rx_channels);
	INIT_LIST_HEAD(&eth_dev->tx_channels);

	init_timer(&eth_dev->start_timer);

	eth_dev->start_timer.function = ipa_eth_dev_start_timer_cb;
	eth_dev->start_timer.data = (unsigned long)eth_dev;

	eth_dev->init = eth_dev->start = !ipa_eth_noauto;

	rc = ipa_eth_net_open_device(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to open network device");
		return rc;
	}

	if (!eth_dev->net_dev) {
		ipa_eth_dev_err(eth_dev, "Netdev info is missing");
		ipa_eth_net_close_device(eth_dev);
		return -EFAULT;
	}

	mutex_lock(&ipa_eth_devices_lock);

	list_add(&eth_dev->device_list, &ipa_eth_devices);

	ipa_eth_dev_log(eth_dev, "Registered new device");

	(void) __ipa_eth_pair_device(eth_dev);

	mutex_unlock(&ipa_eth_devices_lock);

	return 0;
}

void ipa_eth_unregister_device(struct ipa_eth_device *eth_dev)
{
	mutex_lock(&ipa_eth_devices_lock);

	__ipa_eth_unpair_device(eth_dev);
	list_del(&eth_dev->device_list);
	ipa_eth_net_close_device(eth_dev);

	mutex_unlock(&ipa_eth_devices_lock);

	ipa_eth_dev_log(eth_dev, "Unregistered device");
}

static phys_addr_t ipa_eth_vmalloc_to_pa(void *vaddr)
{
	struct page *pg = vmalloc_to_page(vaddr);

	if (pg)
		return page_to_phys(pg);
	else
		return 0;
}

static phys_addr_t ipa_eth_va_to_pa(void *vaddr)
{
	return is_vmalloc_addr(vaddr) ?
			ipa_eth_vmalloc_to_pa(vaddr) :
			virt_to_phys(vaddr);
}

/**
 * ipa_eth_iommu_map() - Create IOMMU mapping from a given DMA address to a
 *                       physical/virtual address
 * @domain: IOMMU domain in which the mapping need to be created
 * @daddr: DMA address (IO Virtual Address) that need to be mapped
 * @addr: Physical or CPU Virtual Address of memory
 * @is_va: True if @addr is CPU Virtual Address
 * @size: Total size of the mapping
 * @prot: Flags for iommu_map() call
 * @split: If True, separate page sized mapping is created
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_iommu_map(struct iommu_domain *domain,
	dma_addr_t daddr, void *addr, bool is_va,
	size_t size, int prot, bool split)
{
	int rc;
	dma_addr_t daddr_r = rounddown(daddr, PAGE_SIZE);
	void *addr_r = (void *)rounddown((unsigned long)addr, PAGE_SIZE);
	size_t size_r = roundup(size + (daddr - daddr_r), PAGE_SIZE);
	const size_t MAP_SIZE = split ? PAGE_SIZE : size_r;

	if ((daddr - daddr_r) != (addr - addr_r)) {
		ipa_eth_err("Alignment mismatch between paddr and addr");
		return -EINVAL;
	}

	if (daddr != daddr_r)
		ipa_eth_dbg("DMA address %p realigned to %p", daddr, daddr_r);

	if (addr != addr_r)
		ipa_eth_dbg("PA/VA address %p realigned to %p", addr, addr_r);

	if (size != size_r)
		ipa_eth_dbg("DMA map size %zx realigned to %zx", size, size_r);

	for (size = 0; size < size_r;
	     size += MAP_SIZE, daddr_r += MAP_SIZE, addr_r += MAP_SIZE) {
		phys_addr_t paddr = is_va ?
					ipa_eth_va_to_pa(addr_r) :
					(phys_addr_t) (addr_r);
		phys_addr_t paddr_r = rounddown(paddr, PAGE_SIZE);

		if (!paddr_r) {
			rc = -EFAULT;
			ipa_eth_err("Failed to find paddr for vaddr %p", addr);
			goto failed_map;
		}

		if (paddr != paddr_r) {
			rc = -EFAULT;
			ipa_eth_err("paddr %p is not page aligned", paddr);
			goto failed_map;
		}

		rc = ipa3_iommu_map(domain, daddr_r, paddr_r, MAP_SIZE, prot);
		if (rc) {
			ipa_eth_err("Failed to map daddr %p to %s domain",
				    daddr, domain->name);
			goto failed_map;
		}

		ipa_eth_log(
			"Mapped %zu bytes of daddr %p to paddr %p in domain %s",
			MAP_SIZE, daddr_r, paddr_r, domain->name);
	}

	return 0;

failed_map:
	ipa_eth_iommu_unmap(domain, daddr_r - size, size, split);
	return rc;

}

/**
 * ipa_eth_iommu_unmap() - Remove an IOMMU mapping previously made using
 *                         ipa_eth_iommu_map()
 * @domain: IOMMU domain from which the mapping need to be removed
 * @daddr: DMA address (IO Virtual Address) that was mapped
 * @size: Total size of the mapping
 * @split: If True, separate page sized mappings were created
 *
 * Return: 0 on success, negative errno if at least one of the mappings could
 *         not be removed.
 */
int ipa_eth_iommu_unmap(struct iommu_domain *domain,
	dma_addr_t daddr, size_t size, bool split)
{
	int rc = 0;
	dma_addr_t daddr_r = rounddown(daddr, PAGE_SIZE);
	size_t size_r = roundup(size + (daddr - daddr_r), PAGE_SIZE);
	const size_t MAP_SIZE = split ? PAGE_SIZE : size_r;

	if (!size_r) {
		ipa_eth_dbg("Ignoring unmap request of size 0");
		return 0;
	}

	for (size = 0; size < size_r; size += MAP_SIZE) {
		if (iommu_unmap(domain, daddr_r + size, MAP_SIZE) != MAP_SIZE) {
			rc = -EFAULT;
			ipa_eth_err("Failed to unmap daddr %p", daddr_r + size);
		}

		ipa_eth_log(
			"Unmapped %zu bytes of daddr %p in domain %s",
			MAP_SIZE, daddr_r + size, domain->name);
	}

	return rc;
}

/**
 * ipa_eth_register_net_driver() - Register a network driver with the offload
 *                                 subsystem
 * @nd: Network driver to register
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_register_net_driver(struct ipa_eth_net_driver *nd)
{
	int rc;

	rc = ipa_eth_bus_register_driver(nd);
	if (rc)
		ipa_eth_err("Failed to register network driver %s", nd->name);
	else
		ipa_eth_log("Registered network driver %s", nd->name);

	return rc;
}
EXPORT_SYMBOL(ipa_eth_register_net_driver);

/**
 * ipa_eth_unregister_net_driver() - Unregister a network driver
 * @nd: Network driver to unregister
 */
void ipa_eth_unregister_net_driver(struct ipa_eth_net_driver *nd)
{
	ipa_eth_bus_unregister_driver(nd);
}
EXPORT_SYMBOL(ipa_eth_unregister_net_driver);

/**
 * ipa_eth_register_offload_driver - Register an offload driver with the offload
 *                                   subsystem
 * @nd: Offload driver to register
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_register_offload_driver(struct ipa_eth_offload_driver *od)
{
	int rc;

	rc = ipa_eth_offload_register_driver(od);
	if (rc) {
		ipa_eth_err("Failed to register offload driver %s", od->name);
		return rc;
	}

	ipa_eth_log("Registered offload driver %s", od->name);

	ipa_eth_pair_devices();
	ipa_eth_refresh_devices();

	return 0;
}
EXPORT_SYMBOL(ipa_eth_register_offload_driver);

/**
 * ipa_eth_unregister_offload_driver() - Unregister an offload driver
 * @nd: Offload driver to unregister
 */
void ipa_eth_unregister_offload_driver(struct ipa_eth_offload_driver *od)
{
	ipa_eth_unpair_devices(od);
	ipa_eth_offload_unregister_driver(od);

	ipa_eth_log("Unregistered offload driver %s", od->name);
}
EXPORT_SYMBOL(ipa_eth_unregister_offload_driver);

static void ipa_eth_debugfs_cleanup(void)
{
	debugfs_remove_recursive(ipa_eth_debugfs);
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
		ipa_eth_ready() ? "Ready" : "Not Ready");

	n = simple_read_from_buffer(user_buf, count, ppos, buf, n);

	kfree(buf);

	return n;
}

static const struct file_operations fops_eth_dev_ready = {
	.read = eth_dev_ready_read,
	.open = simple_open,
	.llseek = default_llseek,
};

static int ipa_eth_debugfs_init(void)
{
	int rc = 0;
	struct dentry *ipa_debugfs = ipa_debugfs_get_root();

	if (IS_ERR_OR_NULL(ipa_debugfs))
		return 0;

	ipa_eth_debugfs =
		debugfs_create_dir("ethernet", ipa_debugfs);
	if (IS_ERR_OR_NULL(ipa_eth_debugfs)) {
		ipa_eth_log("Unable to create debugfs root");
		rc = ipa_eth_debugfs ?
			PTR_ERR(ipa_eth_debugfs) : -EFAULT;
		goto err_exit;
	}

	ipa_eth_drivers_debugfs =
		debugfs_create_dir("drivers", ipa_eth_debugfs);
	if (IS_ERR_OR_NULL(ipa_eth_drivers_debugfs)) {
		ipa_eth_log("Unable to create debugfs root for drivers");

		rc = ipa_eth_drivers_debugfs ?
			PTR_ERR(ipa_eth_drivers_debugfs) : -EFAULT;
		goto err_exit;
	}

	ipa_eth_devices_debugfs =
		debugfs_create_dir("devices", ipa_eth_debugfs);
	if (IS_ERR_OR_NULL(ipa_eth_devices_debugfs)) {
		ipa_eth_log("Unable to create debugfs root for devices");

		rc = ipa_eth_devices_debugfs ?
			PTR_ERR(ipa_eth_devices_debugfs) : -EFAULT;
		goto err_exit;
	}

	(void) debugfs_create_file("ready", 0644, ipa_eth_debugfs, NULL,
			    &fops_eth_dev_ready);

	(void) debugfs_create_bool("no_auto", 0644,
				   ipa_eth_debugfs, &ipa_eth_noauto);

	(void) debugfs_create_bool("ipc_logdbg", 0644,
				   ipa_eth_debugfs, &ipa_eth_ipc_logdbg);

	ipa_eth_log("Debugfs root is initialized");

	return 0;

err_exit:
	ipa_eth_debugfs_cleanup();
	return rc;
}

static void *ipa_eth_ipc_logbuf;

void *ipa_eth_get_ipc_logbuf(void)
{
	return ipa_eth_ipc_logbuf;
}
EXPORT_SYMBOL(ipa_eth_get_ipc_logbuf);

void *ipa_eth_get_ipc_logbuf_dbg(void)
{
	return ipa_eth_ipc_logdbg ? ipa_eth_ipc_logbuf : NULL;
}
EXPORT_SYMBOL(ipa_eth_get_ipc_logbuf_dbg);

#define IPA_ETH_IPC_LOG_PAGES 128

static int ipa_eth_ipc_log_init(void)
{
	if (ipa_eth_ipc_logbuf)
		return 0;

	ipa_eth_ipc_logbuf = ipc_log_context_create(
				IPA_ETH_IPC_LOG_PAGES, IPA_ETH_SUBSYS, 0);

	return ipa_eth_ipc_logbuf ? 0 : -EFAULT;
}

static void ipa_eth_ipc_log_cleanup(void)
{
	if (ipa_eth_ipc_logbuf) {
		ipc_log_context_destroy(ipa_eth_ipc_logbuf);
		ipa_eth_ipc_logbuf = NULL;
	}
}

static int ipa_eth_panic_save_device(struct ipa_eth_device *eth_dev)
{
	ipa_eth_net_save_regs(eth_dev);
	ipa_eth_offload_save_regs(eth_dev);

	return 0;
}

static int ipa_eth_panic_notifier(struct notifier_block *nb,
	unsigned long event, void *ptr)
{
	struct ipa_eth_device *eth_dev;

	mutex_lock(&ipa_eth_devices_lock);

	list_for_each_entry(eth_dev, &ipa_eth_devices, device_list)
		ipa_eth_panic_save_device(eth_dev);

	mutex_unlock(&ipa_eth_devices_lock);

	return NOTIFY_DONE;
}

static struct notifier_block ipa_eth_panic_nb = {
	.notifier_call  = ipa_eth_panic_notifier,
};

static int ipa_eth_pm_notifier_cb(struct notifier_block *nb,
	unsigned long pm_event, void *unused)
{
	ipa_eth_log("PM notifier called for event %lu", pm_event);

	switch (pm_event) {
	case PM_POST_SUSPEND:
		ipa_eth_refresh_devices();
		break;
	}

	return NOTIFY_DONE;
}

static struct notifier_block pm_notifier = {
	.notifier_call = ipa_eth_pm_notifier_cb,
};

int ipa_eth_init(void)
{
	int rc;
	unsigned int wq_flags = WQ_UNBOUND | WQ_MEM_RECLAIM;

	(void) atomic_notifier_chain_register(
			&panic_notifier_list, &ipa_eth_panic_nb);

	rc = ipa_eth_ipc_log_init();
	if (rc) {
		ipa_eth_err("Failed to initialize IPC logging");
		goto err_ipclog;
	}

	ipa_eth_dbg("Initializing IPA Ethernet Offload Sub-System");

	rc = ipa_eth_debugfs_init();
	if (rc) {
		ipa_eth_err("Failed to initialize debugfs");
		goto err_dbgfs;
	}

	ipa_eth_wq = alloc_workqueue("ipa_eth", wq_flags, 0);
	if (!ipa_eth_wq) {
		ipa_eth_err("Failed to alloc workqueue");
		goto err_wq;
	}

	rc = ipa_eth_bus_modinit(ipa_eth_drivers_debugfs);
	if (rc) {
		ipa_eth_err("Failed to initialize bus");
		goto err_bus;
	}

	rc = ipa_eth_offload_modinit(ipa_eth_drivers_debugfs);
	if (rc) {
		ipa_eth_err("Failed to initialize offload");
		goto err_offload;
	}

	rc = register_pm_notifier(&pm_notifier);
	if (rc) {
		ipa_eth_err("Failed to register for PM notification");
		goto err_pm_notifier;
	}

	rc = ipa3_uc_register_ready_cb(&uc_ready_cb);
	if (rc) {
		ipa_eth_err("Failed to register for uC ready cb");
		goto err_uc;
	}

	rc = ipa_register_ipa_ready_cb(ipa_eth_ipa_ready_cb, NULL);
	if (rc == -EEXIST) {
		set_bit(IPA_ETH_ST_IPA_READY, &ipa_eth_state);
	} else if (rc) {
		ipa_eth_err("Failed to register for IPA ready cb");
		goto err_ipa;
	}

	set_bit(IPA_ETH_ST_READY, &ipa_eth_state);

	ipa_eth_log("Offload sub-system init is complete");

	ipa_eth_refresh_devices();

	return 0;

err_ipa:
	ipa3_uc_unregister_ready_cb(&uc_ready_cb);
err_uc:
	unregister_pm_notifier(&pm_notifier);
err_pm_notifier:
	ipa_eth_offload_modexit();
err_offload:
	ipa_eth_bus_modexit();
err_bus:
	destroy_workqueue(ipa_eth_wq);
	ipa_eth_wq = NULL;
err_wq:
	ipa_eth_debugfs_cleanup();
err_dbgfs:
	ipa_eth_ipc_log_cleanup();
err_ipclog:
	(void) atomic_notifier_chain_unregister(
			&panic_notifier_list, &ipa_eth_panic_nb);
	return rc;
}

void ipa_eth_exit(void)
{
	ipa_eth_dbg("De-initializing IPA Ethernet Offload Sub-System");

	clear_bit(IPA_ETH_ST_READY, &ipa_eth_state);

	// IPA ready CB can not be unregistered; just unregister uC ready CB
	ipa3_uc_unregister_ready_cb(&uc_ready_cb);

	unregister_pm_notifier(&pm_notifier);

	ipa_eth_offload_modexit();
	ipa_eth_bus_modexit();

	destroy_workqueue(ipa_eth_wq);
	ipa_eth_wq = NULL;

	ipa_eth_debugfs_cleanup();
	ipa_eth_ipc_log_cleanup();

	(void) atomic_notifier_chain_unregister(
			&panic_notifier_list, &ipa_eth_panic_nb);
}
