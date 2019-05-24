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

static bool ipa_eth_bus_is_ready;

static struct dentry *ipa_eth_bus_debugfs;

struct ipa_eth_bus_map {
	struct bus_type *bus;
	struct ipa_eth_bus *eth_bus;
	int (*modinit)(struct dentry *);
	void (*modexit)(void);
} bus_map[] = {
	{
		&pci_bus_type,
		&ipa_eth_pci_bus,
		&ipa_eth_pci_modinit,
		&ipa_eth_pci_modexit
	},
	{},
};

static int ipa_eth_bus_debugfs_init(struct dentry *dbgfs_root)
{
	if (!dbgfs_root)
		return 0;

	ipa_eth_bus_debugfs = debugfs_create_dir("bus", dbgfs_root);
	if (IS_ERR_OR_NULL(ipa_eth_bus_debugfs)) {
		int rc = ipa_eth_bus_debugfs ?
			PTR_ERR(ipa_eth_bus_debugfs) : -EFAULT;

		ipa_eth_bus_debugfs = NULL;
		return rc;
	}

	return 0;
}

static void ipa_eth_bus_debugfs_cleanup(void)
{
	debugfs_remove_recursive(ipa_eth_bus_debugfs);
}

static struct ipa_eth_bus *lookup_eth_bus(struct bus_type *bus)
{
	struct ipa_eth_bus_map *map;

	for (map = bus_map; map->bus != NULL; map++) {
		if (map->bus == bus)
			return map->eth_bus;
	}

	return NULL;
}

int ipa_eth_bus_register_driver(struct ipa_eth_net_driver *nd)
{
	struct ipa_eth_bus *eth_bus;

	if (!nd->bus) {
		ipa_eth_err("Missing bus info in net driver");
		return -EINVAL;
	}

	eth_bus = lookup_eth_bus(nd->bus);
	if (!eth_bus) {
		ipa_eth_err("Unsupported bus %s", nd->bus->name);
		return -ENOTSUPP;
	}

	return eth_bus->register_net_driver(nd);
}

void ipa_eth_bus_unregister_driver(struct ipa_eth_net_driver *nd)
{
	struct ipa_eth_bus *eth_bus = lookup_eth_bus(nd->bus);

	if (!eth_bus) {
		ipa_eth_bug("Failed to lookup eth_bus for %s", nd->bus->name);
		return;
	}

	eth_bus->unregister_net_driver(nd);
}

int ipa_eth_bus_enable_pc(struct ipa_eth_device *eth_dev)
{
	struct ipa_eth_net_driver *nd = eth_dev->nd;
	struct ipa_eth_bus *eth_bus = lookup_eth_bus(nd->bus);

	if (!eth_bus) {
		ipa_eth_dev_bug(eth_dev, "Failed to lookup eth_bus");
		return -EFAULT;
	}

	if (eth_bus->enable_pc)
		return eth_bus->enable_pc(eth_dev);

	return -EFAULT;
}

int ipa_eth_bus_disable_pc(struct ipa_eth_device *eth_dev)
{
	struct ipa_eth_net_driver *nd = eth_dev->nd;
	struct ipa_eth_bus *eth_bus = lookup_eth_bus(nd->bus);

	if (!eth_bus) {
		ipa_eth_dev_bug(eth_dev, "Failed to lookup eth_bus");
		return -EFAULT;
	}

	if (eth_bus->disable_pc)
		return eth_bus->disable_pc(eth_dev);

	return -EFAULT;
}

int ipa_eth_bus_modinit(struct dentry *dbgfs_root)
{
	int rc;
	struct ipa_eth_bus_map *map;

	rc = ipa_eth_bus_debugfs_init(dbgfs_root);
	if (rc) {
		ipa_eth_err("Unable to create debugfs root for bus");
		return rc;
	}

	/* initialize all registered busses */
	for (rc = 0, map = bus_map; map->bus != NULL; map++)
		rc |= map->modinit(ipa_eth_bus_debugfs);

	if (rc) {
		ipa_eth_err("Failed to initialize one or more busses");
		goto err_init;
	}

	ipa_eth_bus_is_ready = true;

	ipa_eth_log("Offload sub-system bus module init is complete");

	return 0;

err_init:
	for (map = bus_map; map->bus != NULL; map++)
		map->modexit();

	ipa_eth_bus_debugfs_cleanup();

	return rc;
}

void ipa_eth_bus_modexit(void)
{
	struct ipa_eth_bus_map *map;

	ipa_eth_log("De-initing offload sub-system bus module");

	if (!ipa_eth_bus_is_ready)
		return;

	ipa_eth_bus_is_ready = false;

	for (map = bus_map; map->bus != NULL; map++)
		map->modexit();

	ipa_eth_bus_debugfs_cleanup();
}
