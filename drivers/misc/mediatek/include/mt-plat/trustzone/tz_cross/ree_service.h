/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


/** Commands and value for REE service call */
/* This is used by TEE internal. Other TA please don't include this */

#ifndef __REE_SERVICE__
#define __REE_SERVICE__

#define REE_SERVICE_BUFFER_SIZE    128

enum ReeServiceCommand {
	REE_SERV_NONE = 0,
	REE_SERV_PUTS,		/* Print buffer. */
	REE_SERV_USLEEP,	/* Sleep us */
	REE_SERV_MUTEX_CREATE,
	REE_SERV_MUTEX_DESTROY,
	REE_SERV_MUTEX_LOCK,
	REE_SERV_MUTEX_UNLOCK,
	REE_SERV_MUTEX_TRYLOCK,
	REE_SERV_MUTEX_ISLOCK,
	REE_SERV_SEMAPHORE_CREATE,
	REE_SERV_SEMAPHORE_DESTROY,
	REE_SERV_SEMAPHORE_DOWN,
	REE_SERV_SEMAPHORE_DWNTO,	/* down with time-out */
	REE_SERV_SEMAPHORE_TRYDWN,
	REE_SERV_SEMAPHORE_UP,
#if 0
	REE_SERV_WAITQ_CREATE,
	REE_SERV_WAITQ_DESTROY,
	REE_SERV_WAITQ_WAIT,
	REE_SERV_WAITQ_WAITTO,	/* wait with time-out */
	REE_SERV_WAITQ_WAKEUP,
#endif
	REE_SERV_REQUEST_IRQ,
	REE_SERV_ENABLE_IRQ,
	REE_SERV_ENABLE_CLOCK,
	REE_SERV_DISABLE_CLOCK,
	REE_SERV_THREAD_CREATE,

	REE_SERV_SEMAPHORE_DOWNINT,     /* interruptible down */
	REE_SERV_GET_CHUNK_MEMPOOL,
	REE_SERV_REL_CHUNK_MEMPOOL,

	REE_SERV_PM_GET,		/* pm runtime support */
	REE_SERV_PM_PUT,
};

/* //////// Param structure for commands */
struct ree_service_usleep {
	unsigned int ustime;
};

#define MTEE_THREAD_NAME_NUM 32

struct REE_THREAD_INFO {
	uint32_t handle;	/* trhread handle */
	char name[MTEE_THREAD_NAME_NUM];	/* kree side, trhread name */
};

#ifndef MTIRQF_NORMAL
/* / Must match the one in tz_private/system.h */
#define MTIRQF_SHARED          (1<<0)	/* / Share with other handlers */
#define MTIRQF_TRIGGER_LOW     (1<<1)	/* / IRQ is triggered by low signal */
#define MTIRQF_TRIGGER_HIGH    (1<<2)	/* / IRQ is triggered by high signal */
#define MTIRQF_TRIGGER_RISING  (1<<3)	/* / IRQ is triggered by rising edge */
#define MTIRQF_TRIGGER_FALLING (1<<4)	/* / IRQ is triggered by falling edge */
#endif

struct ree_service_irq {
	uint64_t token64;
	unsigned int irq;
	int enable;
	unsigned int flags;
};

struct ree_service_clock {
	unsigned int clk_id;
	char clk_name[112];
};

struct ree_service_chunk_mem {
	uint64_t chunkmem_pa;
	unsigned int size;
};
#endif	/* __REE_SERVICE__ */
