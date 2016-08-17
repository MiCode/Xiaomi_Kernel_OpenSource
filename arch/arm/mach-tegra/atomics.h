/*
 * Copyright (c) 2013, NVIDIA CORPORATION. All rights reserved.
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

#ifndef __TEGRA_ATOMICS_H
#define __TEGRA_ATOMICS_H

#define ATOMICS_NUM_ENTRIES	128
#define AP0_TRIGGER		0x0
#define AP1_TRIGGER		0x4
#define AP0_V_BASE		0x400
#define AP0_C_BASE		0x800
#define AP0_RESULT_BASE		0xc00
#define AP1_V_BASE		0x1000
#define AP1_C_BASE		0x1400
#define AP1_RESULT_BASE		0x1800

#define ATOMIC_EXCHANGE		0
#define ATOMIC_COMP_EXCHANGE	1
#define ATOMIC_INCREMENT	2
#define ATOMIC_DECREMENT	3
#define ATOMIC_GET		4
#define ATOMIC_PUT		5
#define ATOMIC_TEST_AND_SET	6
#define ATOMIC_TEST_AND_CLEAR	7
#define ATOMIC_TEST_AND_INVERT	8

#ifndef __ASSEMBLY__
void tegra_atomics_init(void);

/*
 * tegra_atomics_swap
 *
 * Ideally, the atomics is just like, an atomic operation, it doesn't matter
 * which task or which CPU executes it. However, we have only two apertures,
 * so we can only arbitrate between two entities, for example, between two
 * different CPUs, or between one CPU and AVP. To arbitrate among more than
 * 2 entities, we need a chain of operations.
 *
 * For the time being, we first define operations for two entities only
 */

void tegra_atomic_spin_lock(u32 id, u32 index);
void tegra_atomic_spin_unlock(u32 id, u32 index);
int tegra_atomic_spin_lock_alloc(u32 *id);
int tegra_atomic_spin_lock_free(u32 id);
#endif
#endif
