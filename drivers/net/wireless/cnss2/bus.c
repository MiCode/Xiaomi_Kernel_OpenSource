/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include "bus.h"
#include "debug.h"
#include "pci.h"

enum cnss_dev_bus_type cnss_get_dev_bus_type(struct device *dev)
{
	if (!dev)
		return CNSS_BUS_NONE;

	if (!dev->bus)
		return CNSS_BUS_NONE;

	if (memcmp(dev->bus->name, "pci", 3) == 0)
		return CNSS_BUS_PCI;
	else
		return CNSS_BUS_NONE;
}

enum cnss_dev_bus_type cnss_get_bus_type(unsigned long device_id)
{
	switch (device_id) {
	case QCA6174_DEVICE_ID:
	case QCA6290_EMULATION_DEVICE_ID:
	case QCA6290_DEVICE_ID:
		return CNSS_BUS_PCI;
	default:
		cnss_pr_err("Unknown device_id: 0x%lx\n", device_id);
		return CNSS_BUS_NONE;
	}
}

int cnss_bus_init(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_init(plat_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

void cnss_bus_deinit(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		cnss_pci_deinit(plat_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return;
	}
}

int cnss_bus_load_m3(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_load_m3(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_alloc_fw_mem(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_alloc_fw_mem(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

u32 cnss_bus_get_wake_irq(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_get_wake_msi(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

int cnss_bus_force_fw_assert_hdlr(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return -ENODEV;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_force_fw_assert_hdlr(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return -EINVAL;
	}
}

void cnss_bus_fw_boot_timeout_hdlr(unsigned long data)
{
	struct cnss_plat_data *plat_priv = (struct cnss_plat_data *)data;

	if (!plat_priv)
		return;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_fw_boot_timeout_hdlr(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return;
	}
}

void cnss_bus_collect_dump_info(struct cnss_plat_data *plat_priv)
{
	if (!plat_priv)
		return;

	switch (plat_priv->bus_type) {
	case CNSS_BUS_PCI:
		return cnss_pci_collect_dump_info(plat_priv->bus_priv);
	default:
		cnss_pr_err("Unsupported bus type: %d\n",
			    plat_priv->bus_type);
		return;
	}
}
