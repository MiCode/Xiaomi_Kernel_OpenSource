/*
 * drivers/video/tegra/host/gr2d/gr2d_t114.c
 *
 * Tegra Graphics 2D Tegra11 specific parts
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
#include "host1x/hw_host1x02_sync.h"

#include "gr2d_common.c"

int nvhost_gr2d_t114_finalize_poweron(struct platform_device *dev)
{
	gr2d_reset(dev);
	return 0;
}
