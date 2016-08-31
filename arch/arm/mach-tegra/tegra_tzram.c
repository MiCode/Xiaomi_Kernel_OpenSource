/*
 * Mapping Secure os carveout as 1 to 1 mapping.
 *
 * Copyright (c) 2013 NVIDIA Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include "common.h"

#if defined(CONFIG_PLATFORM_ENABLE_IOMMU)
static struct platform_device tegra_tzram_device = {
	.name   = "tegra-tzram",
	.id     = -1,
};
#endif

static int __init tegra_tzram_carveout_init(void)
{
	int err = 0;

#if defined(CONFIG_PLATFORM_ENABLE_IOMMU)
	DEFINE_DMA_ATTRS(attrs);

	if (!tegra_tzram_start || !tegra_tzram_size)
		return -EINVAL;

	err = platform_device_register(&tegra_tzram_device);
	if (err) {
		pr_err("tegra device registration failed\n");
		return err;
	}

	dma_set_attr(DMA_ATTR_SKIP_CPU_SYNC, &attrs);
	dma_map_linear_attrs(&tegra_tzram_device.dev,
		tegra_tzram_start, tegra_tzram_size, DMA_TO_DEVICE, &attrs);
#endif

	return err;
}
late_initcall(tegra_tzram_carveout_init);
