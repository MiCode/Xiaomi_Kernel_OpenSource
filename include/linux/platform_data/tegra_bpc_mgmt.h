/*
 * Copyright (C) 2010-2011 NVIDIA Corporation
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __TEGRA_BPC_MGMT_H
#define __TEGRA_BPC_MGMT_H
#include <linux/cpumask.h>

struct tegra_bpc_mgmt_platform_data {
	int gpio_trigger;
	struct cpumask affinity_mask;
	int bpc_mgmt_timeout;
};

#endif /*__TEGRA_BPC_MGMT_H*/
