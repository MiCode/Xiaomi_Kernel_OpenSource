/*
 * drivers/video/tegra/host/gr2d/gr2d_common.c
 *
 * Tegra Graphics 2D common parts
 *
 * Copyright (c) 2012, NVIDIA Corporation.
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

#include <linux/nvhost.h>
#include <linux/io.h>
#include "host1x/host1x.h"

static void gr2d_reset(struct platform_device *dev)
{
	void __iomem *sync_aperture = nvhost_get_host(dev)->sync_aperture;

	writel(host1x_sync_mod_teardown_epp_teardown_f(1)
			+ host1x_sync_mod_teardown_gr2d_teardown_f(1),
			sync_aperture + host1x_sync_mod_teardown_r());
}
