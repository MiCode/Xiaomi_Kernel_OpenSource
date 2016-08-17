/*
 *  arch/arm/mach-tegra/include/mach/pci.h
 *
 *  Header file containing constants for the tegra PCIe driver.
 *
 * Copyright (c) 2011, NVIDIA Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __MACH_PCI_H
#define __MACH_PCI_H

#include <linux/pci.h>

#ifdef CONFIG_ARCH_TEGRA_2x_SOC
	#define MAX_PCIE_SUPPORTED_PORTS 2
#else
	#define MAX_PCIE_SUPPORTED_PORTS 3
#endif

struct tegra_pci_platform_data {
	int port_status[MAX_PCIE_SUPPORTED_PORTS];
	bool use_dock_detect;
	int gpio;
};
#endif
