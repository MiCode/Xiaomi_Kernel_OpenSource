/*
 * Copyright (C) 2010 Google, Inc.
 * Copyright (c) 2011-2013 NVIDIA CORPORATION. All rights reserved.
 *
 * Author:
 *	Colin Cross <ccross@google.com>
 *	Erik Gilling <konkers@google.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef MACH_TEGRA_HARDWARE_H
#define MACH_TEGRA_HARDWARE_H

#if defined(CONFIG_ARCH_TEGRA_2x_SOC)
#define pcibios_assign_all_busses()		1

#else

#define pcibios_assign_all_busses()		0
#endif

enum tegra_chipid {
	TEGRA_CHIPID_UNKNOWN = 0,
	TEGRA_CHIPID_TEGRA2 = 0x20,
	TEGRA_CHIPID_TEGRA3 = 0x30,
	TEGRA_CHIPID_TEGRA11 = 0x35,
};

enum tegra_revision {
	TEGRA_REVISION_UNKNOWN = 0,
	TEGRA_REVISION_A01,
	TEGRA_REVISION_A02,
	TEGRA_REVISION_A03,
	TEGRA_REVISION_A03p,
	TEGRA_REVISION_A04,
	TEGRA_REVISION_A04p,
	TEGRA_REVISION_QT,
	TEGRA_REVISION_MAX,
};

extern enum tegra_revision tegra_revision;
enum tegra_chipid tegra_get_chipid(void);
unsigned int tegra_get_minor_rev(void);

#ifndef CONFIG_TEGRA_SILICON_PLATFORM
void tegra_get_netlist_revision(u32 *netlist, u32* patchid);
#else
static inline void tegra_get_netlist_revision(u32 *netlist, u32* patchid)
{
	BUG();
}
#endif

#endif
