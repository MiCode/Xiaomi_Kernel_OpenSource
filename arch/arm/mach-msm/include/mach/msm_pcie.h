/* Copyright (c) 2012-2014, The Linux Foundation. All rights reserved.
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

#ifndef __ASM_ARCH_MSM_PCIE_H
#define __ASM_ARCH_MSM_PCIE_H

#include <linux/types.h>
#include <linux/pci.h>

enum msm_pcie_config {
	MSM_PCIE_CONFIG_INVALID = 0,
	MSM_PCIE_CONFIG_NO_CFG_RESTORE = 0x1,
	MSM_PCIE_CONFIG_LINKDOWN = 0x2,
	MSM_PCIE_CONFIG_NO_RECOVERY = 0x4,
};

enum msm_pcie_pm_opt {
	MSM_PCIE_SUSPEND,
	MSM_PCIE_RESUME,
	MSM_PCIE_REQ_EXIT_L1,
};

enum msm_pcie_event {
	MSM_PCIE_EVENT_INVALID = 0,
	MSM_PCIE_EVENT_LINKDOWN = 0x1,
	MSM_PCIE_EVENT_LINKUP = 0x2,
	MSM_PCIE_EVENT_WAKEUP = 0x4,
	MSM_PCIE_EVENT_WAKE_RECOVERY = 0x8,
	MSM_PCIE_EVENT_NO_ACCESS = 0x10,
};

enum msm_pcie_trigger {
	MSM_PCIE_TRIGGER_CALLBACK,
	MSM_PCIE_TRIGGER_COMPLETION,
};

struct msm_pcie_notify {
	enum msm_pcie_event event;
	void *user;
	void *data;
	u32 options;
};

struct msm_pcie_register_event {
	u32 events;
	void *user;
	enum msm_pcie_trigger mode;
	void (*callback)(struct msm_pcie_notify *notify);
	struct msm_pcie_notify notify;
	struct completion *completion;
	u32 options;
};

/**
 * msm_pcie_pm_control - control the power state of a PCIe link.
 * @pm_opt:	power management operation
 * @busnr:	bus number of PCIe endpoint
 * @user:	handle of the caller
 * @data:	private data from the caller
 * @options:	options for pm control
 *
 * This function gives PCIe endpoint device drivers the control to change
 * the power state of a PCIe link for their device.
 *
 * Return: 0 on success, negative value on error
 */
int msm_pcie_pm_control(enum msm_pcie_pm_opt pm_opt, u32 busnr, void *user,
			void *data, u32 options);

/**
 * msm_pcie_register_event - register an event with PCIe bus driver.
 * @reg:	event structure
 *
 * This function gives PCIe endpoint device drivers an option to register
 * events with PCIe bus driver.
 *
 * Return: 0 on success, negative value on error
 */
int msm_pcie_register_event(struct msm_pcie_register_event *reg);

/**
 * msm_pcie_deregister_event - deregister an event with PCIe bus driver.
 * @reg:	event structure
 *
 * This function gives PCIe endpoint device drivers an option to deregister
 * events with PCIe bus driver.
 *
 * Return: 0 on success, negative value on error
 */
int msm_pcie_deregister_event(struct msm_pcie_register_event *reg);

/**
 * msm_pcie_recover_config - recover config space.
 * @dev:	pci device structure
 *
 * This function recovers the config space of both RC and Endpoint.
 *
 * Return: 0 on success, negative value on error
 */
int msm_pcie_recover_config(struct pci_dev *dev);

/**
 * msm_pcie_shadow_control - control the shadowing of PCIe config space.
 * @dev:	pci device structure
 * @enable:	shadowing should be enabled or disabled
 *
 * This function gives PCIe endpoint device drivers the control to enable
 * or disable the shadowing of PCIe config space.
 *
 * Return: 0 on success, negative value on error
 */
int msm_pcie_shadow_control(struct pci_dev *dev, bool enable);

/*
 * msm_pcie_access_control - access control to PCIe address range.
 * @dev:	pci device structure
 * @enable:	enable or disable the access
 *
 * This function gives PCIe endpoint device drivers the control to enable
 * or disable the access to PCIe address range.
 *
 * Return: 0 on success, negative value on error
 */
int msm_pcie_access_control(struct pci_dev *dev, bool enable);
#endif
