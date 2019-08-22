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

#include "ipa_eth_i.h"

static LIST_HEAD(ipa_eth_offload_drivers);
static DEFINE_MUTEX(ipa_eth_offload_drivers_lock);

static struct dentry *ipa_eth_offload_debugfs;

static void ipa_eth_offload_debugfs_cleanup(void)
{
	debugfs_remove_recursive(ipa_eth_offload_debugfs);
}

static int ipa_eth_offload_debugfs_init(struct dentry *dbgfs_root)
{
	if (!dbgfs_root)
		return 0;

	ipa_eth_offload_debugfs = debugfs_create_dir("offload", dbgfs_root);
	if (IS_ERR_OR_NULL(ipa_eth_offload_debugfs)) {
		int rc = ipa_eth_offload_debugfs ?
			PTR_ERR(ipa_eth_offload_debugfs) : -EFAULT;

		ipa_eth_offload_debugfs = NULL;
		return rc;
	}

	return 0;
}

int ipa_eth_offload_init(struct ipa_eth_device *eth_dev)
{
	int rc;

	rc = eth_dev->od->ops->init_tx(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to init offload for tx");
		return rc;
	}

	rc = eth_dev->od->ops->init_rx(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to init offload for rx");
		(void) eth_dev->od->ops->deinit_tx(eth_dev);
		return rc;
	}

	return 0;
}

int ipa_eth_offload_deinit(struct ipa_eth_device *eth_dev)
{
	int rc_rx, rc_tx;

	rc_rx = eth_dev->od->ops->deinit_rx(eth_dev);
	if (rc_rx)
		ipa_eth_dev_err(eth_dev, "Failed to deinit offload for rx");

	rc_tx = eth_dev->od->ops->deinit_tx(eth_dev);
	if (rc_tx)
		ipa_eth_dev_err(eth_dev, "Failed to deinit offload for tx");

	return rc_rx || rc_tx;
}

int ipa_eth_offload_start(struct ipa_eth_device *eth_dev)
{
	int rc;

	rc = eth_dev->od->ops->start_tx(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to start offload for tx");
		return rc;
	}

	rc = eth_dev->od->ops->start_rx(eth_dev);
	if (rc) {
		ipa_eth_dev_err(eth_dev, "Failed to start offload for rx");
		(void) eth_dev->od->ops->stop_tx(eth_dev);
		return rc;
	}

	return 0;
}

int ipa_eth_offload_stop(struct ipa_eth_device *eth_dev)
{
	int rc_rx, rc_tx;

	rc_rx = eth_dev->od->ops->stop_rx(eth_dev);
	if (rc_rx)
		ipa_eth_dev_err(eth_dev, "Failed to stop offload for rx");

	rc_tx = eth_dev->od->ops->stop_tx(eth_dev);
	if (rc_tx)
		ipa_eth_dev_err(eth_dev, "Failed to stop offload for tx");

	return rc_rx || rc_tx;
}

static int __try_pair_device(struct ipa_eth_device *eth_dev,
			   struct ipa_eth_offload_driver *od)
{
	int rc = od->ops->pair(eth_dev);

	if (rc) {
		ipa_eth_dev_dbg(eth_dev,
			"Offload driver %s passed up paring with %s",
			od->name, eth_dev->nd->name);
		return rc;
	}

	ipa_eth_dev_log(eth_dev,
		"Offload driver %s is paired with device from %s",
		od->name, eth_dev->nd->name);

	eth_dev->od = od;

	ipa_eth_dev_log(eth_dev,
		"Offload driver %s successfully paired with device from %s",
		od->name, eth_dev->nd->name);

	return 0;
}

static int try_pair_device(struct ipa_eth_device *eth_dev,
			   struct ipa_eth_offload_driver *od)
{
	if (od->bus != eth_dev->dev->bus) {
		ipa_eth_dev_dbg(eth_dev,
			"Offload driver %s is not a bus match for %s",
			od->name, eth_dev->nd->name);

		return -ENOTSUPP;
	}

	return __try_pair_device(eth_dev, od);
}

int ipa_eth_offload_pair_device(struct ipa_eth_device *eth_dev)
{
	struct ipa_eth_offload_driver *od;

	if (ipa_eth_offload_device_paired(eth_dev))
		return 0;

	list_for_each_entry(od, &ipa_eth_offload_drivers, driver_list) {
		if (!try_pair_device(eth_dev, od))
			return 0;
	}

	return -ENODEV;
}

void ipa_eth_offload_unpair_device(struct ipa_eth_device *eth_dev)
{
	struct ipa_eth_offload_driver *od = eth_dev->od;

	if (!ipa_eth_offload_device_paired(eth_dev))
		return;

	eth_dev->od = NULL;

	od->ops->unpair(eth_dev);
}

int ipa_eth_offload_register_driver(struct ipa_eth_offload_driver *od)
{
	if (!od->bus) {
		ipa_eth_err("Bus info missing for offload driver %s", od->name);
		return -EINVAL;
	}

	if (!od->ops || !od->ops->pair  || !od->ops->unpair) {
		ipa_eth_err("Pair ops missing for offload driver %s", od->name);
		return -EINVAL;
	}

	if (!od->debugfs && ipa_eth_offload_debugfs) {
		od->debugfs =
			debugfs_create_dir(od->name, ipa_eth_offload_debugfs);
		if (IS_ERR_OR_NULL(od->debugfs)) {
			int rc = od->debugfs ? PTR_ERR(od->debugfs) : -EFAULT;

			od->debugfs = NULL;
			return rc;
		}
	}

	mutex_lock(&ipa_eth_offload_drivers_lock);
	list_add(&od->driver_list, &ipa_eth_offload_drivers);
	mutex_unlock(&ipa_eth_offload_drivers_lock);

	return 0;
}

void ipa_eth_offload_unregister_driver(struct ipa_eth_offload_driver *od)
{
	debugfs_remove_recursive(od->debugfs);

	mutex_lock(&ipa_eth_offload_drivers_lock);
	list_del(&od->driver_list);
	mutex_unlock(&ipa_eth_offload_drivers_lock);
}

int ipa_eth_offload_save_regs(struct ipa_eth_device *eth_dev)
{
	struct ipa_eth_offload_driver *od = eth_dev->od;

	if (od && od->ops->save_regs)
		return eth_dev->od->ops->save_regs(eth_dev, NULL, NULL);

	return 0;
}

int ipa_eth_offload_modinit(struct dentry *dbgfs_root)
{
	int rc;

	rc = ipa_eth_offload_debugfs_init(dbgfs_root);
	if (rc) {
		ipa_eth_err("Failed to init offload module debugfs");
		return rc;
	}

	ipa_eth_log("Offload sub-system offload module init is complete");

	return 0;
}

void ipa_eth_offload_modexit(void)
{
	ipa_eth_log("De-initing offload sub-system offload module");
	ipa_eth_offload_debugfs_cleanup();
}
