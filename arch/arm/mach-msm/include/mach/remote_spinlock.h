/* Copyright (c) 2009, 2011 Code Aurora Forum. All rights reserved.
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
#define SMEM_SPINLOCK_PID_APPS 1

static inline void __raw_remote_ex_spin_lock(raw_remote_spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"1:	ldrex	%0, [%1]\n"
"	teq	%0, #0\n"
"	strexeq	%0, %2, [%1]\n"
"	teqeq	%0, #0\n"
"	bne	1b"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (1)
	: "cc");

	smp_mb();
}

static inline int __raw_remote_ex_spin_trylock(raw_remote_spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"	ldrex	%0, [%1]\n"
"	teq	%0, #0\n"
"	strexeq	%0, %2, [%1]\n"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (1)
	: "cc");

	if (tmp == 0) {
		smp_mb();
		return 1;
	}
	return 0;
}

static inline void __raw_remote_ex_spin_unlock(raw_remote_spinlock_t *lock)
{
	smp_mb();

	__asm__ __volatile__(
"	str	%1, [%0]\n"
	:
	: "r" (&lock->lock), "r" (0)
	: "cc");
}

static inline void __raw_remote_swp_spin_lock(raw_remote_spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"1:	swp	%0, %2, [%1]\n"
"	teq	%0, #0\n"
"	bne	1b"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (1)
	: "cc");

	smp_mb();
}

static inline int __raw_remote_swp_spin_trylock(raw_remote_spinlock_t *lock)
{
	unsigned long tmp;

	__asm__ __volatile__(
"	swp	%0, %2, [%1]\n"
	: "=&r" (tmp)
	: "r" (&lock->lock), "r" (1)
	: "cc");

	if (tmp == 0) {
		smp_mb();
		return 1;
	}
	return 0;
}

static inline void __raw_remote_swp_spin_unlock(raw_remote_spinlock_t *lock)
{
	smp_mb();

	__asm__ __volatile__(
"	str	%1, [%0]"
	:
	: "r" (&lock->lock), "r" (0)
	: "cc");
}

#define DEK_LOCK_REQUEST		1
#define DEK_LOCK_YIELD			(!DEK_LOCK_REQUEST)
#define DEK_YIELD_TURN_SELF		0
static inline void __raw_remote_dek_spin_lock(raw_remote_spinlock_t *lock)
{
	lock->dek.self_lock = DEK_LOCK_REQUEST;

	while (lock->dek.other_lock) {

		if (lock->dek.next_yield == DEK_YIELD_TURN_SELF)
			lock->dek.self_lock = DEK_LOCK_YIELD;

		while (lock->dek.other_lock)
			;

		lock->dek.self_lock = DEK_LOCK_REQUEST;
	}
	lock->dek.next_yield = DEK_YIELD_TURN_SELF;

	smp_mb();
}

static inline int __raw_remote_dek_spin_trylock(raw_remote_spinlock_t *lock)
{
	lock->dek.self_lock = DEK_LOCK_REQUEST;

	if (lock->dek.other_lock) {
		lock->dek.self_lock = DEK_LOCK_YIELD;
		return 0;
	}

	lock->dek.next_yield = DEK_YIELD_TURN_SELF;

	smp_mb();
	return 1;
}

static inline void __raw_remote_dek_spin_unlock(raw_remote_spinlock_t *lock)
{
	smp_mb();

	lock->dek.self_lock = DEK_LOCK_YIELD;
}

static inline int __raw_remote_dek_spin_release(raw_remote_spinlock_t *lock,
		uint32_t pid)
{
	return -EINVAL;
}

static inline void __raw_remote_sfpb_spin_lock(raw_remote_spinlock_t *lock)
{
	do {
		writel_relaxed(SMEM_SPINLOCK_PID_APPS, lock);
		smp_mb();
	} while (readl_relaxed(lock) != SMEM_SPINLOCK_PID_APPS);
}

static inline int __raw_remote_sfpb_spin_trylock(raw_remote_spinlock_t *lock)
{
	return 1;
}

static inline void __raw_remote_sfpb_spin_unlock(raw_remote_spinlock_t *lock)
{
	writel_relaxed(0, lock);
	smp_mb();
}

/**
 * Release spinlock if it is owned by @pid.
 *
 * This is only to be used for situations where the processor owning
 * the spinlock has crashed and the spinlock must be released.
 *
 * @lock - lock structure
 * @pid - processor ID of processor to release
 */
static inline int __raw_remote_gen_spin_release(raw_remote_spinlock_t *lock,
		uint32_t pid)
{
	int ret = 1;

	if (readl_relaxed(&lock->lock) == pid) {
		writel_relaxed(0, &lock->lock);
		wmb();
		ret = 0;
	}
	return ret;
}

#if defined(CONFIG_MSM_SMD) || defined(CONFIG_MSM_REMOTE_SPINLOCK_SFPB)
int _remote_spin_lock_init(remote_spinlock_id_t, _remote_spinlock_t *lock);
void _remote_spin_release_all(uint32_t pid);
#else
static inline
int _remote_spin_lock_init(remote_spinlock_id_t id, _remote_spinlock_t *lock)
{
	return -EINVAL;
}
static inline void _remote_spin_release_all(uint32_t pid) {}
#endif

#if defined(CONFIG_MSM_REMOTE_SPINLOCK_DEKKERS)
/* Use Dekker's algorithm when LDREX/STREX and SWP are unavailable for
 * shared memory */
#define _remote_spin_lock(lock)		__raw_remote_dek_spin_lock(*lock)
#define _remote_spin_unlock(lock)	__raw_remote_dek_spin_unlock(*lock)
#define _remote_spin_trylock(lock)	__raw_remote_dek_spin_trylock(*lock)
#define _remote_spin_release(lock, pid)	__raw_remote_dek_spin_release(*lock,\
		pid)
#elif defined(CONFIG_MSM_REMOTE_SPINLOCK_SWP)
/* Use SWP-based locks when LDREX/STREX are unavailable for shared memory. */
#define _remote_spin_lock(lock)		__raw_remote_swp_spin_lock(*lock)
#define _remote_spin_unlock(lock)	__raw_remote_swp_spin_unlock(*lock)
#define _remote_spin_trylock(lock)	__raw_remote_swp_spin_trylock(*lock)
#define _remote_spin_release(lock, pid)	__raw_remote_gen_spin_release(*lock,\
		pid)
#elif defined(CONFIG_MSM_REMOTE_SPINLOCK_SFPB)
/* Use SFPB Hardware Mutex Registers */
#define _remote_spin_lock(lock)		__raw_remote_sfpb_spin_lock(*lock)
#define _remote_spin_unlock(lock)	__raw_remote_sfpb_spin_unlock(*lock)
#define _remote_spin_trylock(lock)	__raw_remote_sfpb_spin_trylock(*lock)
#define _remote_spin_release(lock, pid)	__raw_remote_gen_spin_release(*lock,\
		pid)
#else
/* Use LDREX/STREX for shared memory locking, when available */
#define _remote_spin_lock(lock)		__raw_remote_ex_spin_lock(*lock)
#define _remote_spin_unlock(lock)	__raw_remote_ex_spin_unlock(*lock)
#define _remote_spin_trylock(lock)	__raw_remote_ex_spin_trylock(*lock)
#define _remote_spin_release(lock, pid)	__raw_remote_gen_spin_release(*lock, \
		pid)
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
