/* Copyright (c) 2008-2009, 2011, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>

#include <asm/system.h>

#include <mach/msm_iomap.h>
#include <mach/remote_spinlock.h>
#include <mach/dal.h>
#include "smd_private.h"
#include <linux/module.h>

static void remote_spin_release_all_locks(uint32_t pid, int count);

#if defined(CONFIG_MSM_REMOTE_SPINLOCK_SFPB)
#define SFPB_SPINLOCK_COUNT 8
#define MSM_SFPB_MUTEX_REG_BASE 0x01200600
#define MSM_SFPB_MUTEX_REG_SIZE	(33 * 4)

static void *hw_mutex_reg_base;
static DEFINE_MUTEX(hw_map_init_lock);

static int remote_spinlock_init_address(int id, _remote_spinlock_t *lock)
{
	if (id >= SFPB_SPINLOCK_COUNT)
		return -EINVAL;

	if (!hw_mutex_reg_base) {
		mutex_lock(&hw_map_init_lock);
		if (!hw_mutex_reg_base)
			hw_mutex_reg_base = ioremap(MSM_SFPB_MUTEX_REG_BASE,
				   MSM_SFPB_MUTEX_REG_SIZE);
		mutex_unlock(&hw_map_init_lock);
		BUG_ON(hw_mutex_reg_base == NULL);
	}

	*lock = hw_mutex_reg_base + 0x4 + id * 4;
	return 0;
}

void _remote_spin_release_all(uint32_t pid)
{
	remote_spin_release_all_locks(pid, SFPB_SPINLOCK_COUNT);
}

#else
#define SMEM_SPINLOCK_COUNT 8
#define SMEM_SPINLOCK_ARRAY_SIZE (SMEM_SPINLOCK_COUNT * sizeof(uint32_t))

static int remote_spinlock_init_address(int id, _remote_spinlock_t *lock)
{
	_remote_spinlock_t spinlock_start;

	if (id >= SMEM_SPINLOCK_COUNT)
		return -EINVAL;

	spinlock_start = smem_alloc(SMEM_SPINLOCK_ARRAY,
				    SMEM_SPINLOCK_ARRAY_SIZE);
	if (spinlock_start == NULL)
		return -ENXIO;

	*lock = spinlock_start + id;

	return 0;
}

void _remote_spin_release_all(uint32_t pid)
{
	remote_spin_release_all_locks(pid, SMEM_SPINLOCK_COUNT);
}

#endif

/**
 * Release all spinlocks owned by @pid.
 *
 * This is only to be used for situations where the processor owning
 * spinlocks has crashed and the spinlocks must be released.
 *
 * @pid - processor ID of processor to release
 */
static void remote_spin_release_all_locks(uint32_t pid, int count)
{
	int n;
	 _remote_spinlock_t lock;

	for (n = 0; n < count; ++n) {
		if (remote_spinlock_init_address(n, &lock) == 0)
			_remote_spin_release(&lock, pid);
	}
}

static int
remote_spinlock_dal_init(const char *chunk_name, _remote_spinlock_t *lock)
{
	void *dal_smem_start, *dal_smem_end;
	uint32_t dal_smem_size;
	struct dal_chunk_header *cur_header;

	if (!chunk_name)
		return -EINVAL;

	dal_smem_start = smem_get_entry(SMEM_DAL_AREA, &dal_smem_size);
	if (!dal_smem_start)
		return -ENXIO;

	dal_smem_end = dal_smem_start + dal_smem_size;

	/* Find first chunk header */
	cur_header = (struct dal_chunk_header *)
			(((uint32_t)dal_smem_start + (4095)) & ~4095);
	*lock = NULL;
	while (cur_header->size != 0
		&& ((uint32_t)(cur_header + 1) < (uint32_t)dal_smem_end)) {

		/* Check if chunk name matches */
		if (!strncmp(cur_header->name, chunk_name,
						DAL_CHUNK_NAME_LENGTH)) {
			*lock = (_remote_spinlock_t)&cur_header->lock;
			return 0;
		}
		cur_header = (void *)cur_header + cur_header->size;
	}

	pr_err("%s: DAL remote lock \"%s\" not found.\n", __func__,
		chunk_name);
	return -EINVAL;
}

int _remote_spin_lock_init(remote_spinlock_id_t id, _remote_spinlock_t *lock)
{
	BUG_ON(id == NULL);

	if (id[0] == 'D' && id[1] == ':') {
		/* DAL chunk name starts after "D:" */
		return remote_spinlock_dal_init(&id[2], lock);
	} else if (id[0] == 'S' && id[1] == ':') {
		/* Single-digit lock ID follows "S:" */
		BUG_ON(id[3] != '\0');

		return remote_spinlock_init_address((((uint8_t)id[2])-'0'),
				lock);
	} else {
		return -EINVAL;
	}
}

int _remote_mutex_init(struct remote_mutex_id *id, _remote_mutex_t *lock)
{
	BUG_ON(id == NULL);

	lock->delay_us = id->delay_us;
	return _remote_spin_lock_init(id->r_spinlock_id, &(lock->r_spinlock));
}
EXPORT_SYMBOL(_remote_mutex_init);

void _remote_mutex_lock(_remote_mutex_t *lock)
{
	while (!_remote_spin_trylock(&(lock->r_spinlock))) {
		if (lock->delay_us >= 1000)
			msleep(lock->delay_us/1000);
		else
			udelay(lock->delay_us);
	}
}
EXPORT_SYMBOL(_remote_mutex_lock);

void _remote_mutex_unlock(_remote_mutex_t *lock)
{
	_remote_spin_unlock(&(lock->r_spinlock));
}
EXPORT_SYMBOL(_remote_mutex_unlock);

int _remote_mutex_trylock(_remote_mutex_t *lock)
{
	return _remote_spin_trylock(&(lock->r_spinlock));
}
EXPORT_SYMBOL(_remote_mutex_trylock);
