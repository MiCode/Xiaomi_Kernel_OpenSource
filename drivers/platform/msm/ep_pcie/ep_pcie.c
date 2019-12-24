/* Copyright (c) 2015, 2017-2019, The Linux Foundation. All rights reserved.
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

/*
 * MSM PCIe endpoint service layer.
 */
#include <linux/types.h>
#include <linux/list.h>
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include "ep_pcie_com.h"

LIST_HEAD(head);

int ep_pcie_register_drv(struct ep_pcie_hw *handle)
{
	struct ep_pcie_hw *present;
	bool new = true;

	if (WARN_ON(!handle))
		return -EINVAL;

	list_for_each_entry(present, &head, node) {
		if (present->device_id == handle->device_id) {
			new = false;
			break;
		}
	}

	if (new) {
		list_add(&handle->node, &head);
		pr_debug("ep_pcie:%s: register a new driver for device 0x%x\n",
			__func__, handle->device_id);
		return 0;
	}
	pr_debug(
		"ep_pcie:%s: driver to register for device 0x%x has already existed\n",
		__func__, handle->device_id);
	return -EEXIST;
}
EXPORT_SYMBOL(ep_pcie_register_drv);

int ep_pcie_deregister_drv(struct ep_pcie_hw *handle)
{
	struct ep_pcie_hw *present;
	bool found = false;

	if (WARN_ON(!handle))
		return -EINVAL;

	list_for_each_entry(present, &head, node) {
		if (present->device_id == handle->device_id) {
			found = true;
			list_del(&handle->node);
			break;
		}
	}

	if (found) {
		pr_debug("ep_pcie:%s: deregistered driver for device 0x%x\n",
			__func__, handle->device_id);
		return 0;
	}
	pr_err("ep_pcie:%s: driver for device 0x%x does not exist\n",
		__func__, handle->device_id);
	return -EEXIST;
}
EXPORT_SYMBOL(ep_pcie_deregister_drv);

struct ep_pcie_hw *ep_pcie_get_phandle(u32 id)
{
	struct ep_pcie_hw *present;

	list_for_each_entry(present, &head, node) {
		if (present->device_id == id) {
			pr_debug("ep_pcie:%s: found driver for device 0x%x\n",
				__func__, id);
			return present;
		}
	}

	pr_debug("ep_pcie:%s: driver for device 0x%x does not exist\n",
			__func__, id);
	return NULL;
}
EXPORT_SYMBOL(ep_pcie_get_phandle);

int ep_pcie_configure_inactivity_timer(struct ep_pcie_hw *phandle,
					struct ep_pcie_inactivity *param)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->configure_inactivity_timer(param);
}
EXPORT_SYMBOL(ep_pcie_configure_inactivity_timer);

int ep_pcie_register_event(struct ep_pcie_hw *phandle,
			struct ep_pcie_register_event *reg)
{
	if (phandle)
		return phandle->register_event(reg);

	return ep_pcie_core_register_event(reg);
}
EXPORT_SYMBOL(ep_pcie_register_event);

int ep_pcie_deregister_event(struct ep_pcie_hw *phandle)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->deregister_event();
}
EXPORT_SYMBOL(ep_pcie_deregister_event);

enum ep_pcie_link_status ep_pcie_get_linkstatus(struct ep_pcie_hw *phandle)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->get_linkstatus();
}
EXPORT_SYMBOL(ep_pcie_get_linkstatus);

int ep_pcie_config_outbound_iatu(struct ep_pcie_hw *phandle,
				struct ep_pcie_iatu entries[],
				u32 num_entries)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->config_outbound_iatu(entries, num_entries);
}
EXPORT_SYMBOL(ep_pcie_config_outbound_iatu);

int ep_pcie_get_msi_config(struct ep_pcie_hw *phandle,
				struct ep_pcie_msi_config *cfg)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->get_msi_config(cfg);
}
EXPORT_SYMBOL(ep_pcie_get_msi_config);

int ep_pcie_trigger_msi(struct ep_pcie_hw *phandle, u32 idx)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->trigger_msi(idx);
}
EXPORT_SYMBOL(ep_pcie_trigger_msi);

int ep_pcie_wakeup_host(struct ep_pcie_hw *phandle,
			enum ep_pcie_event event)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->wakeup_host(event);
}
EXPORT_SYMBOL(ep_pcie_wakeup_host);

int ep_pcie_config_db_routing(struct ep_pcie_hw *phandle,
				struct ep_pcie_db_config chdb_cfg,
				struct ep_pcie_db_config erdb_cfg)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->config_db_routing(chdb_cfg, erdb_cfg);
}
EXPORT_SYMBOL(ep_pcie_config_db_routing);

int ep_pcie_enable_endpoint(struct ep_pcie_hw *phandle,
				enum ep_pcie_options opt)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->enable_endpoint(opt);
}
EXPORT_SYMBOL(ep_pcie_enable_endpoint);

int ep_pcie_disable_endpoint(struct ep_pcie_hw *phandle)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->disable_endpoint();
}
EXPORT_SYMBOL(ep_pcie_disable_endpoint);

int ep_pcie_mask_irq_event(struct ep_pcie_hw *phandle,
				enum ep_pcie_irq_event event,
				bool enable)
{
	if (WARN_ON(!phandle))
		return -EINVAL;

	return phandle->mask_irq_event(event, enable);
}
EXPORT_SYMBOL(ep_pcie_mask_irq_event);
