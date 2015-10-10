/* Copyright (c) 2015, The Linux Foundation. All rights reserved.
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
#include <linux/msm_ep_pcie.h>

LIST_HEAD(head);

int ep_pcie_register_drv(struct ep_pcie_hw *handle)
{
	struct ep_pcie_hw *present;
	bool new = true;

	if (!handle) {
		pr_err("ep_pcie:%s: the input handle is NULL.",
			__func__);
		return -EINVAL;
	}

	list_for_each_entry(present, &head, node) {
		if (present->device_id == handle->device_id) {
			new = false;
			break;
		}
	}

	if (new) {
		list_add(&handle->node, &head);
		pr_debug("ep_pcie:%s: register a new driver for device 0x%x.",
			__func__, handle->device_id);
		return 0;
	} else {
		pr_debug(
			"ep_pcie:%s: driver to register for device 0x%x has already existed.",
			__func__, handle->device_id);
		return -EEXIST;
	}
}
EXPORT_SYMBOL(ep_pcie_register_drv);

int ep_pcie_deregister_drv(struct ep_pcie_hw *handle)
{
	struct ep_pcie_hw *present;
	bool found = false;

	if (!handle) {
		pr_err("ep_pcie:%s: the input handle is NULL.",
			__func__);
		return -EINVAL;
	}

	list_for_each_entry(present, &head, node) {
		if (present->device_id == handle->device_id) {
			found = true;
			list_del(&handle->node);
			break;
		}
	}

	if (found) {
		pr_debug("ep_pcie:%s: deregistered driver for device 0x%x.",
			__func__, handle->device_id);
		return 0;
	} else {
		pr_err("ep_pcie:%s: driver for device 0x%x does not exist.",
			__func__, handle->device_id);
		return -EEXIST;
	}
}
EXPORT_SYMBOL(ep_pcie_deregister_drv);

struct ep_pcie_hw *ep_pcie_get_phandle(u32 id)
{
	struct ep_pcie_hw *present;

	list_for_each_entry(present, &head, node) {
		if (present->device_id == id) {
			pr_debug("ep_pcie:%s: found driver for device 0x%x.",
				__func__, id);
			return present;
		}
	}

	pr_debug("ep_pcie:%s: driver for device 0x%x does not exist.",
			__func__, id);
	return NULL;
}
EXPORT_SYMBOL(ep_pcie_get_phandle);

int ep_pcie_register_event(struct ep_pcie_hw *phandle,
			struct ep_pcie_register_event *reg)
{
	if (phandle) {
		return phandle->register_event(reg);
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_register_event);

int ep_pcie_deregister_event(struct ep_pcie_hw *phandle)
{
	if (phandle) {
		return phandle->deregister_event();
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_deregister_event);

enum ep_pcie_link_status ep_pcie_get_linkstatus(struct ep_pcie_hw *phandle)
{
	if (phandle) {
		return phandle->get_linkstatus();
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_get_linkstatus);

int ep_pcie_config_outbound_iatu(struct ep_pcie_hw *phandle,
				struct ep_pcie_iatu entries[],
				u32 num_entries)
{
	if (phandle) {
		return phandle->config_outbound_iatu(entries, num_entries);
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_config_outbound_iatu);

int ep_pcie_get_msi_config(struct ep_pcie_hw *phandle,
				struct ep_pcie_msi_config *cfg)
{
	if (phandle) {
		return phandle->get_msi_config(cfg);
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_get_msi_config);

int ep_pcie_trigger_msi(struct ep_pcie_hw *phandle, u32 idx)
{
	if (phandle) {
		return phandle->trigger_msi(idx);
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_trigger_msi);

int ep_pcie_wakeup_host(struct ep_pcie_hw *phandle)
{
	if (phandle) {
		return phandle->wakeup_host();
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_wakeup_host);

int ep_pcie_config_db_routing(struct ep_pcie_hw *phandle,
				struct ep_pcie_db_config chdb_cfg,
				struct ep_pcie_db_config erdb_cfg)
{
	if (phandle) {
		return phandle->config_db_routing(chdb_cfg, erdb_cfg);
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_config_db_routing);

int ep_pcie_enable_endpoint(struct ep_pcie_hw *phandle,
				enum ep_pcie_options opt)
{
	if (phandle) {
		return phandle->enable_endpoint(opt);
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_enable_endpoint);

int ep_pcie_disable_endpoint(struct ep_pcie_hw *phandle)
{
	if (phandle) {
		return phandle->disable_endpoint();
	} else {
		pr_err("ep_pcie:%s: the input driver handle is NULL.",
			__func__);
		return -EINVAL;
	}
}
EXPORT_SYMBOL(ep_pcie_disable_endpoint);

int ep_pcie_mask_irq_event(struct ep_pcie_hw *phandle,
				enum ep_pcie_irq_event event,
				bool enable)
{
	if (phandle)
		return phandle->mask_irq_event(event, enable);

	pr_err("ep_pcie:%s: the input driver handle is NULL.", __func__);
	return -EINVAL;
}
EXPORT_SYMBOL(ep_pcie_mask_irq_event);
