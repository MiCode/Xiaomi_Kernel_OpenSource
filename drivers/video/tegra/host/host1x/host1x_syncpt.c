/*
 * drivers/video/tegra/host/host1x/host1x_syncpt.c
 *
 * Tegra Graphics Host Syncpoints for HOST1X
 *
 * Copyright (c) 2010-2013, NVIDIA Corporation.
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

#include <linux/nvhost_ioctl.h>
#include <linux/io.h>
#include <trace/events/nvhost.h>
#include "nvhost_syncpt.h"
#include "nvhost_acm.h"
#include "host1x.h"
#include "chip_support.h"

/**
 * Write the current syncpoint value back to hw.
 */
static void t20_syncpt_reset(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	int min = nvhost_syncpt_read_min(sp, id);
	writel(min, dev->sync_aperture + (host1x_sync_syncpt_0_r() + id * 4));
}

/**
 * Write the current waitbase value back to hw.
 */
static void t20_syncpt_reset_wait_base(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	writel(sp->base_val[id],
		dev->sync_aperture + (host1x_sync_syncpt_base_0_r() + id * 4));
}

/**
 * Read waitbase value from hw.
 */
static void t20_syncpt_read_wait_base(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	sp->base_val[id] = readl(dev->sync_aperture +
				(host1x_sync_syncpt_base_0_r() + id * 4));
}

/**
 * Updates the last value read from hardware.
 * (was nvhost_syncpt_update_min)
 */
static u32 t20_syncpt_update_min(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	void __iomem *sync_regs = dev->sync_aperture;
	u32 old, live;

	do {
		old = nvhost_syncpt_read_min(sp, id);
		live = readl(sync_regs + (host1x_sync_syncpt_0_r() + id * 4));
	} while ((u32)atomic_cmpxchg(&sp->min_val[id], old, live) != old);

	return live;
}

/**
 * Write a cpu syncpoint increment to the hardware, without touching
 * the cache. Caller is responsible for host being powered.
 */
static void t20_syncpt_cpu_incr(struct nvhost_syncpt *sp, u32 id)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	u32 reg_offset = id / 32;

	if (!nvhost_syncpt_client_managed(sp, id)
			&& nvhost_syncpt_min_eq_max(sp, id)) {
		dev_err(&syncpt_to_dev(sp)->dev->dev,
			"Trying to increment syncpoint id %d beyond max\n",
			id);
		nvhost_debug_dump(syncpt_to_dev(sp));
		return;
	}
	writel(bit_mask(id), dev->sync_aperture +
			host1x_sync_syncpt_cpu_incr_r() + reg_offset * 4);
}

/* remove a wait pointed to by patch_addr */
static int host1x_syncpt_patch_wait(struct nvhost_syncpt *sp,
		void *patch_addr)
{
	u32 override = nvhost_class_host_wait_syncpt(
			NVSYNCPT_GRAPHICS_HOST, 0);
	__raw_writel(override, patch_addr);
	return 0;
}


static const char *t20_syncpt_name(struct nvhost_syncpt *sp, u32 id)
{
	struct host1x_device_info *info = &syncpt_to_dev(sp)->info;
	const char *name = NULL;

	if (id < info->nb_pts)
		name = info->syncpt_names[id];

	return name ? name : "";
}

static void t20_syncpt_debug(struct nvhost_syncpt *sp)
{
	u32 i;
	for (i = 0; i < nvhost_syncpt_nb_pts(sp); i++) {
		u32 max = nvhost_syncpt_read_max(sp, i);
		u32 min = nvhost_syncpt_update_min(sp, i);
		if (!max && !min)
			continue;
		dev_info(&syncpt_to_dev(sp)->dev->dev,
			"id %d (%s) min %d max %d\n",
			i, syncpt_op().name(sp, i),
			min, max);

	}

	for (i = 0; i < nvhost_syncpt_nb_bases(sp); i++) {
		u32 base_val;
		t20_syncpt_read_wait_base(sp, i);
		base_val = sp->base_val[i];
		if (base_val)
			dev_info(&syncpt_to_dev(sp)->dev->dev,
					"waitbase id %d val %d\n",
					i, base_val);

	}
}

static int syncpt_mutex_try_lock(struct nvhost_syncpt *sp,
		unsigned int idx)
{
	void __iomem *sync_regs = syncpt_to_dev(sp)->sync_aperture;
	/* mlock registers returns 0 when the lock is aquired.
	 * writing 0 clears the lock. */
	return !!readl(sync_regs + (host1x_sync_mlock_0_r() + idx * 4));
}

static void syncpt_mutex_unlock(struct nvhost_syncpt *sp,
	       unsigned int idx)
{
	void __iomem *sync_regs = syncpt_to_dev(sp)->sync_aperture;

	writel(0, sync_regs + (host1x_sync_mlock_0_r() + idx * 4));
}

static void syncpt_mutex_owner(struct nvhost_syncpt *sp,
				unsigned int idx,
				bool *cpu, bool *ch,
				unsigned int *chid)
{
	struct nvhost_master *dev = syncpt_to_dev(sp);
	u32 __iomem *mlo_regs = dev->sync_aperture +
		host1x_sync_mlock_owner_0_r();
	u32 owner = readl(mlo_regs + idx);

	*chid = host1x_sync_mlock_owner_0_mlock_owner_chid_0_v(owner);
	*cpu = host1x_sync_mlock_owner_0_mlock_cpu_owns_0_v(owner);
	*ch = host1x_sync_mlock_owner_0_mlock_ch_owns_0_v(owner);
}

static const struct nvhost_syncpt_ops host1x_syncpt_ops = {
	.reset = t20_syncpt_reset,
	.reset_wait_base = t20_syncpt_reset_wait_base,
	.read_wait_base = t20_syncpt_read_wait_base,
	.update_min = t20_syncpt_update_min,
	.cpu_incr = t20_syncpt_cpu_incr,
	.patch_wait = host1x_syncpt_patch_wait,
	.debug = t20_syncpt_debug,
	.name = t20_syncpt_name,
	.mutex_try_lock = syncpt_mutex_try_lock,
	.mutex_unlock = syncpt_mutex_unlock,
	.mutex_owner = syncpt_mutex_owner,
};
