/* Copyright (c) 2009, 2011, 2013-2014 The Linux Foundation.
 * All rights reserved.
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

/*
 * Part of this this code is based on the standard ARM spinlock
 * implementation (asm/spinlock.h) found in the 2.6.29 kernel.
 */

#ifndef __ASM__ARCH_QC_REMOTE_SPINLOCK_H
#define __ASM__ARCH_QC_REMOTE_SPINLOCK_H

#include <linux/io.h>
#include <linux/types.h>

#define REMOTE_SPINLOCK_NUM_PID 128
#define REMOTE_SPINLOCK_TID_START REMOTE_SPINLOCK_NUM_PID

/* Remote spinlock definitions. */

struct dek_spinlock {
	volatile uint8_t self_lock;
	volatile uint8_t other_lock;
	volatile uint8_t next_yield;
	uint8_t pad;
};

typedef union {
	volatile uint32_t lock;
	struct dek_spinlock dek;
} raw_remote_spinlock_t;

typedef raw_remote_spinlock_t *_remote_spinlock_t;

#define remote_spinlock_id_t const char *

#if defined(CONFIG_MSM_SMD) || defined(CONFIG_MSM_REMOTE_SPINLOCK_SFPB)
int _remote_spin_lock_init(remote_spinlock_id_t, _remote_spinlock_t *lock);
void _remote_spin_release_all(uint32_t pid);
void _remote_spin_lock(_remote_spinlock_t *lock);
void _remote_spin_unlock(_remote_spinlock_t *lock);
int _remote_spin_trylock(_remote_spinlock_t *lock);
int _remote_spin_release(_remote_spinlock_t *lock, uint32_t pid);
int _remote_spin_owner(_remote_spinlock_t *lock);
void _remote_spin_lock_rlock_id(_remote_spinlock_t *lock, uint32_t tid);
void _remote_spin_unlock_rlock(_remote_spinlock_t *lock);
#else
static inline
int _remote_spin_lock_init(remote_spinlock_id_t id, _remote_spinlock_t *lock)
{
	return -EINVAL;
}
static inline void _remote_spin_release_all(uint32_t pid) {}
static inline void _remote_spin_lock(_remote_spinlock_t *lock) {}
static inline void _remote_spin_unlock(_remote_spinlock_t *lock) {}
static inline int _remote_spin_trylock(_remote_spinlock_t *lock)
{
	return -ENODEV;
}
static inline int _remote_spin_release(_remote_spinlock_t *lock, uint32_t pid)
{
	return -ENODEV;
}
static inline int _remote_spin_owner(_remote_spinlock_t *lock)
{
	return -ENODEV;
}
static inline void _remote_spin_lock_rlock_id(_remote_spinlock_t *lock,
					      uint32_t tid) {}
static inline void _remote_spin_unlock_rlock(_remote_spinlock_t *lock) {}
#endif


/* Remote mutex definitions. */

typedef struct {
	_remote_spinlock_t	r_spinlock;
	uint32_t		delay_us;
} _remote_mutex_t;

struct remote_mutex_id {
	remote_spinlock_id_t	r_spinlock_id;
	uint32_t		delay_us;
};

#ifdef CONFIG_MSM_SMD
int _remote_mutex_init(struct remote_mutex_id *id, _remote_mutex_t *lock);
void _remote_mutex_lock(_remote_mutex_t *lock);
void _remote_mutex_unlock(_remote_mutex_t *lock);
int _remote_mutex_trylock(_remote_mutex_t *lock);
#else
static inline
int _remote_mutex_init(struct remote_mutex_id *id, _remote_mutex_t *lock)
{
	return -EINVAL;
}
static inline void _remote_mutex_lock(_remote_mutex_t *lock) {}
static inline void _remote_mutex_unlock(_remote_mutex_t *lock) {}
static inline int _remote_mutex_trylock(_remote_mutex_t *lock)
{
	return 0;
}
#endif

#endif /* __ASM__ARCH_QC_REMOTE_SPINLOCK_H */
