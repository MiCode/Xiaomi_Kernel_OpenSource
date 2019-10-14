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
	int rc;

	mutex_lock(&od->mutex);

	if (od->bus != eth_dev->dev->bus) {
		rc = -ENOTSUPP;
		ipa_eth_dev_dbg(eth_dev,
			"Offload driver %s is not a bus match for %s",
			od->name, eth_dev->nd->name);
	} else {
		rc = __try_pair_device(eth_dev, od);
	}

	mutex_unlock(&od->mutex);

	return rc;
}

int ipa_eth_offload_pair_device(struct ipa_eth_device *eth_dev)
{
	int rc = -ENODEV;

	mutex_lock(&ipa_eth_offload_drivers_lock);

	/* Check if device is already paired while holding the offload_drivers
	 * mutex to ensure this API is re-entrant safe.
	 */
	if (!eth_dev->od) {
		struct ipa_eth_offload_driver *od;

		list_for_each_entry(od, &ipa_eth_offload_drivers, driver_list) {
			if (!try_pair_device(eth_dev, od)) {
				rc = 0;
				break;
			}
		}
	}

	mutex_unlock(&ipa_eth_offload_drivers_lock);

	return rc;
}

void ipa_eth_offload_unpair_device(struct ipa_eth_device *eth_dev)
{
	mutex_lock(&ipa_eth_offload_drivers_lock);

	/* Check if device is already unpaired while holding the offload_drivers
	 * mutex to ensure this API is re-entrant safe.
	 */
	if (eth_dev->od) {
		struct ipa_eth_offload_driver *od = eth_dev->od;

		mutex_lock(&od->mutex);
		od->ops->unpair(eth_dev);
		eth_dev->od = NULL;
		mutex_unlock(&od->mutex);
	}

	mutex_unlock(&ipa_eth_offload_drivers_lock);
}

static bool ipa_eth_offload_check_ops(struct ipa_eth_offload_ops *ops)
{
	/* Following call backs are optional:
	 *  - get_stats()
	 *  - clear_stats()
	 *  - save_regs()
	 *  - prepare_reset()
	 *  - complete_reset()
	 */
	return
		ops &&
		ops->pair &&
		ops->unpair &&
		ops->init_tx &&
		ops->start_tx &&
		ops->stop_tx &&
		ops->deinit_tx &&
		ops->init_rx &&
		ops->start_rx &&
		ops->stop_rx &&
		ops->deinit_rx;
}

static bool ipa_eth_offload_check_driver(struct ipa_eth_offload_driver *od)
{
	return
		od &&
		od->bus &&
		od->name &&
		ipa_eth_offload_check_ops(od->ops);
}

int ipa_eth_offload_register_driver(struct ipa_eth_offload_driver *od)
{
	if (!ipa_eth_offload_check_driver(od)) {
		ipa_eth_err("Offload driver validation failed");
		return -EINVAL;
	}

	mutex_init(&od->mutex);

	mutex_lock(&ipa_eth_offload_drivers_lock);
	list_add(&od->driver_list, &ipa_eth_offload_drivers);
	mutex_unlock(&ipa_eth_offload_drivers_lock);

	(void) ipa_eth_debugfs_add_offload_driver(od);

	return 0;
}

void ipa_eth_offload_unregister_driver(struct ipa_eth_offload_driver *od)
{
	ipa_eth_debugfs_remove_offload_driver(od);

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

int ipa_eth_offload_prepare_reset(struct ipa_eth_device *eth_dev, void *data)
{
	struct ipa_eth_offload_driver *od = eth_dev->od;

	if (od && od->ops->prepare_reset)
		return eth_dev->od->ops->prepare_reset(eth_dev, data);

	return 0;
}

int ipa_eth_offload_complete_reset(struct ipa_eth_device *eth_dev, void *data)
{
	struct ipa_eth_offload_driver *od = eth_dev->od;

	if (od && od->ops->complete_reset)
		return eth_dev->od->ops->complete_reset(eth_dev, data);

	return 0;
}
