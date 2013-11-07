/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

enum msm_pcie_pm_opt {
	MSM_PCIE_SUSPEND,
	MSM_PCIE_RESUME
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

#endif
