/*
 * arch/arm/mach-tegra/atomics.c
 *
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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

#include <linux/init.h>
#include <linux/compiler.h>
#include <linux/bug.h>
#include <linux/bitops.h>
#include <linux/bitmap.h>
#include <linux/errno.h>
#include <linux/syscore_ops.h>
#include <linux/preempt.h>

#include <asm/io.h>

#include <mach/iomap.h>
#include <mach/io.h>

#include "atomics.h"

#define __atomics_writel(val, reg)	__raw_writel(val, atomics_base+reg)
#define __atomics_readl(reg)		__raw_readl(atomics_base+reg)

static void __iomem *atomics_base = IO_ADDRESS(TEGRA_ATOMICS_BASE);

static u32 atomic_trigger[2] = {AP0_TRIGGER, AP1_TRIGGER};
static u32 atomic_setup_v[2] = {AP0_V_BASE, AP1_V_BASE};
static u32 atomic_result[2] = {AP0_RESULT_BASE, AP1_RESULT_BASE};

static DECLARE_BITMAP(atomics_bitmap, ATOMICS_NUM_ENTRIES);

static inline void tegra_atomics_reset(void)
{
	int i;

	for (i = 0; i < ATOMICS_NUM_ENTRIES; i++) {
		__atomics_writel(0, AP0_V_BASE+(4*i));
		__atomics_writel(0, AP1_V_BASE+(4*i));
		__atomics_writel(0, AP0_C_BASE+(4*i));
		__atomics_writel(0, AP1_C_BASE+(4*i));
		__atomics_writel((i<<16) | ATOMIC_PUT, AP0_TRIGGER);
		__atomics_writel((i<<16) | ATOMIC_PUT, AP1_TRIGGER);
	}
}

static void tegra_atomics_resume(void)
{
	tegra_atomics_reset();
}

static int tegra_atomics_suspend(void)
{
	return 0;
}

static struct syscore_ops tegra_atomics_ops = {
	.suspend = tegra_atomics_suspend,
	.resume = tegra_atomics_resume,
};

/*
 * tegra_atomics_init
 *
 * clears target registers of both apertures
 */
void __init tegra_atomics_init(void)
{
	tegra_atomics_reset();

#ifdef CONFIG_PM_SLEEP
	register_syscore_ops(&tegra_atomics_ops);
#endif
}

/*
 * tegra_atomic_spin_lock
 *
 * The synchronization achieved by calling this function is applicable
 * only to 2 CPUs. If we need synchronization among all 4 CPU cores, we
 * need a chained tegra_atomic_spin_lock/unlock calls on 3 ids.
 *
 * id:    specifies which register we are operating upon
 * index: specifies which aperture we are operating upon
 */
void notrace tegra_atomic_spin_lock(u32 id, u32 index)
{
	BUG_ON(index > 1);
	BUG_ON(id >= ATOMICS_NUM_ENTRIES);

	preempt_disable();
	do {
		__atomics_writel(1, atomic_setup_v[index] * 4*id);
		__atomics_writel((id << 16) | ATOMIC_EXCHANGE,
				atomic_trigger[index]);
	} while (__atomics_readl(atomic_result[index] + 4*id) == 1);
}

/*
 * tegra_atomic_spin_unlock
 *
 * The synchronization achieved by calling this function is applicable
 * only to 2 CPUs. If we need synchronization among all 4 CPU cores, we
 * need a chained tegra_atomic_spin_lock/unlock calls on 3 ids.
 *
 * id:    specifies which register we are operating upon
 * index: specifies which aperture we are operating upon
 */
void notrace tegra_atomic_spin_unlock(u32 id, u32 index)
{
	BUG_ON(index > 1);
	BUG_ON(id >= ATOMICS_NUM_ENTRIES);

	/* blindly clears target register */
	__atomics_writel(0, atomic_setup_v[index] + 4*id);
	__atomics_writel((id << 16) | ATOMIC_EXCHANGE, atomic_trigger[index]);
	preempt_enable();
}

/*
 * tegra_atomic_spin_lock_alloc
 *
 * id: the allocated entry
 * return: 0 if success, errno if failure
 */
int tegra_atomic_spin_lock_alloc(u32 *id)
{
	u32 entry;
	entry = find_first_zero_bit(atomics_bitmap, ATOMICS_NUM_ENTRIES);
	if (entry >= ATOMICS_NUM_ENTRIES)
		return -EBUSY;
	*id = entry;
	bitmap_set(atomics_bitmap, entry, 1);
	return 0;
}

/*
 * tegra_atomic_spin_lock_free
 *
 * id: the allocated entry
 * return: 0 if success, errno if failure
 */
int tegra_atomic_spin_lock_free(u32 id)
{
	u32 index = UINT_MAX;

	if (!test_bit(id, atomics_bitmap))
		return -EINVAL;

	/*
	 * if an atomics semaphore has been released, at least one setup_v
	 * register will be 0.
	 */
	if (__atomics_readl(atomic_setup_v[0]) == 0)
		index = 0;
	if (__atomics_readl(atomic_setup_v[1]) == 0)
		index = 1;

	if (index > 1)
		return -EBUSY;

	__atomics_writel((id << 16) | ATOMIC_GET, atomic_trigger[index]);
	if (__atomics_readl(atomic_result[index] + 4*id) == 1)
		return -EBUSY;

	clear_bit(id, atomics_bitmap);
	return 0;
}
