/*
 * drivers/video/tegra/host/t124/cpuaccess_t124.c
 *
 * Tegra Graphics Host Cpu Register Access
 *
 * Copyright (c) 2011-2012, NVIDIA CORPORATION.  All rights reserved.
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
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include "../nvhost_cpuaccess.h"
#include "../dev.h"

#include "t124.h"
#include "hardware_t124.h"

static int t124_cpuaccess_mutex_try_lock(struct nvhost_cpuaccess *ctx,
					unsigned int idx)
{
	struct nvhost_master *dev = cpuaccess_to_dev(ctx);
	void __iomem *sync_regs = dev->sync_aperture;

	nvhost_dbg_fn("");
	/* mlock registers returns 0 when the lock is aquired.
	 * writing 0 clears the lock. */
	return !!readl(sync_regs + (host1x_sync_mlock_0_0_r() + idx * 4));
}

static void t124_cpuaccess_mutex_unlock(struct nvhost_cpuaccess *ctx,
				       unsigned int idx)
{
	struct nvhost_master *dev = cpuaccess_to_dev(ctx);
	void __iomem *sync_regs = dev->sync_aperture;

	nvhost_dbg_fn("");
	writel(0, sync_regs + (host1x_sync_mlock_0_0_r() + idx * 4));
}

int nvhost_init_t124_cpuaccess_support(struct nvhost_master *host,
				      struct nvhost_chip_support *op)
{
	host->nb_modules = NVHOST_MODULE_NUM;

	op->cpuaccess.mutex_try_lock = t124_cpuaccess_mutex_try_lock;
	op->cpuaccess.mutex_unlock = t124_cpuaccess_mutex_unlock;

	return 0;
}
