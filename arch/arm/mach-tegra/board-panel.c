/*
 * arch/arm/mach-tegra/board-panel.c
 *
 * Copyright (c) 2012-2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/platform_device.h>
#include <linux/of.h>

#include "board.h"

#ifdef CONFIG_TEGRA_DC
/**
 * tegra_init_hdmi - initialize and add HDMI device if not disabled by DT
 */
int tegra_init_hdmi(struct platform_device *pdev,
		     struct platform_device *phost1x)
{
	struct resource __maybe_unused *res;
	bool enabled = true;
	int err;
#ifdef CONFIG_OF
	struct device_node *hdmi_node = NULL;

	hdmi_node = of_find_node_by_path("/host1x/hdmi");
	/* disable HDMI if explicitly set that way in the device tree */
	enabled = !hdmi_node || of_device_is_available(hdmi_node);
#endif

	if (enabled) {
		/*
		 * If the bootloader fb2 is valid, copy it to the fb2, or else
		 * clear fb2 to avoid garbage on dispaly2.
		 */
		if (tegra_bootloader_fb2_size)
			tegra_move_framebuffer(tegra_fb2_start,
				tegra_bootloader_fb2_start,
				min(tegra_fb2_size, tegra_bootloader_fb2_size));
		else
			tegra_clear_framebuffer(tegra_fb2_start,
						tegra_fb2_size);

		res = platform_get_resource_byname(pdev, IORESOURCE_MEM,
						   "fbmem");
		res->start = tegra_fb2_start;
		res->end = tegra_fb2_start + tegra_fb2_size - 1;

		pdev->dev.parent = &phost1x->dev;
		err = platform_device_register(pdev);
		if (err) {
			dev_err(&pdev->dev, "device registration failed\n");
			return err;
		}
	}

	return 0;
}
#else
int tegra_init_hdmi(struct platform_device *pdev,
		     struct platform_device *phost1x)
{
	return 0;
}
#endif
