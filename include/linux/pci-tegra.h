/*
 *  Header file contains constants and structures for tegra PCIe driver.
 *
 * Copyright (c) 2011-2013, NVIDIA CORPORATION.  All rights reserved.
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

#define MAX_PCIE_SUPPORTED_PORTS 2

struct tegra_pci_platform_data {
	int port_status[MAX_PCIE_SUPPORTED_PORTS];
	/* used to identify if current platofrm supports CLKREQ# */
	bool has_clkreq;
	int gpio_hot_plug;
	int gpio_wake;
	int gpio_x1_slot;
};
#endif
