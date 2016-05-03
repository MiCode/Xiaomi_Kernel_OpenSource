/* Copyright (c) 2002,2007-2016, The Linux Foundation. All rights reserved.
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
#ifndef __ADRENO_RINGBUFFER_H
#define __ADRENO_RINGBUFFER_H

#include "kgsl_iommu.h"
#include "adreno_iommu.h"
#include "adreno_dispatch.h"

/* Given a ringbuffer, return the adreno device that owns it */

#define _RB_OFFSET(_id) (offsetof(struct adreno_device, ringbuffers) + \
		((_id) * sizeof(struct adreno_ringbuffer)))

#define ADRENO_RB_DEVICE(_rb) \
	((struct adreno_device *) (((void *) (_rb)) - _RB_OFFSET((_rb)->id)))

/* Adreno ringbuffer size in bytes */
#define KGSL_RB_SIZE (32 * 1024)

/*
 * A handy macro to convert the RB size to dwords since most ringbuffer
 * operations happen in dword increments
 */
#define KGSL_RB_DWORDS (KGSL_RB_SIZE >> 2)

struct kgsl_device;
struct kgsl_device_private;

/**
 * struct adreno_submit_time - utility structure to store the wall clock / GPU
 * ticks at command submit time
 * @ticks: GPU ticks at submit time (from the 19.2Mhz timer)
 * @ktime: local clock time (in nanoseconds)
 * @utime: Wall clock time
 */
struct adreno_submit_time {
	uint64_t ticks;
	u64 ktime;
	struct timespec utime;
};

/**
 * struct adreno_ringbuffer_pagetable_info - Contains fields used during a
 * pagetable switch.
 * @current_global_ptname: The current pagetable id being used by the GPU.
 * Only the ringbuffers[0] current_global_ptname is used to keep track of
 * the current pagetable id
 * @current_rb_ptname: The current pagetable active on the given RB
 * @incoming_ptname: Contains the incoming pagetable we are switching to. After
 * switching of pagetable this value equals current_rb_ptname.
 * @switch_pt_enable: Flag used during pagetable switch to check if pt
 * switch can be skipped
 * @ttbr0: value to program into TTBR0 during pagetable switch.
 * @contextidr: value to program into CONTEXTIDR during pagetable switch.
 */
struct adreno_ringbuffer_pagetable_info {
	int current_global_ptname;
	int current_rb_ptname;
	int incoming_ptname;
	int switch_pt_enable;
	uint64_t ttbr0;
	unsigned int contextidr;
};

/**
 * struct adreno_ringbuffer - Definition for an adreno ringbuffer object
 * @flags: Internal control flags for the ringbuffer
 * @buffer_desc: Pointer to the ringbuffer memory descriptor
 * @wptr: Local copy of the wptr offset
 * @last_wptr: offset of the last H/W committed wptr
 * @rb_ctx: The context that represents a ringbuffer
 * @id: Priority level of the ringbuffer, also used as an ID
 * @fault_detect_ts: The last retired global timestamp read during fault detect
 * @timestamp: The RB's global timestamp
 * @events: A kgsl_event_group for this context - contains the list of GPU
 * events
 * @drawctxt_active: The last pagetable that this ringbuffer is set to
 * @preemption_desc: The memory descriptor containing
 * preemption info written/read by CP
 * @pagetable_desc: Memory to hold information about the pagetables being used
 * and the commands to switch pagetable on the RB
 * @dispatch_q: The dispatcher side queue for this ringbuffer
 * @ts_expire_waitq: Wait queue to wait for rb timestamp to expire
 * @ts_expire_waitq: Wait q to wait for rb timestamp to expire
 * @wptr_preempt_end: Used during preemption to check that preemption occurred
 * at the right rptr
 * @gpr11: The gpr11 value of this RB
 * @preempted_midway: Indicates that the RB was preempted before rptr = wptr
 * @sched_timer: Timer that tracks how long RB has been waiting to be scheduled
 * or how long it has been scheduled for after preempting in
 * @starve_timer_state: Indicates the state of the wait.
 */
struct adreno_ringbuffer {
	uint32_t flags;
	struct kgsl_memdesc buffer_desc;
	unsigned int wptr;
	unsigned int last_wptr;
	int id;
	unsigned int fault_detect_ts;
	unsigned int timestamp;
	struct kgsl_event_group events;
	struct adreno_context *drawctxt_active;
	struct kgsl_memdesc preemption_desc;
	struct kgsl_memdesc pagetable_desc;
	struct adreno_dispatcher_cmdqueue dispatch_q;
	wait_queue_head_t ts_expire_waitq;
	unsigned int wptr_preempt_end;
	unsigned int gpr11;
	int preempted_midway;
	unsigned long sched_timer;
	enum adreno_dispatcher_starve_timer_states starve_timer_state;
};

/* Returns the current ringbuffer */
#define ADRENO_CURRENT_RINGBUFFER(a)	((a)->cur_rb)

int cp_secure_mode(struct adreno_device *adreno_dev, uint *cmds, int set);

int adreno_ringbuffer_issueibcmds(struct kgsl_device_private *dev_priv,
				struct kgsl_context *context,
				struct kgsl_cmdbatch *cmdbatch,
				uint32_t *timestamp);

int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_cmdbatch *cmdbatch,
		struct adreno_submit_time *time);

int adreno_ringbuffer_probe(struct adreno_device *adreno_dev, bool nopreempt);

int adreno_ringbuffer_start(struct adreno_device *adreno_dev,
		unsigned int start_type);

void adreno_ringbuffer_stop(struct adreno_device *adreno_dev);

void adreno_ringbuffer_close(struct adreno_device *adreno_dev);

int adreno_ringbuffer_issuecmds(struct adreno_ringbuffer *rb,
					unsigned int flags,
					unsigned int *cmdaddr,
					int sizedwords);

void adreno_ringbuffer_submit(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time);

int adreno_ringbuffer_submit_spin(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time, unsigned int timeout);

void kgsl_cp_intrcallback(struct kgsl_device *device);

unsigned int *adreno_ringbuffer_allocspace(struct adreno_ringbuffer *rb,
						unsigned int numcmds);

void adreno_ringbuffer_read_pfp_ucode(struct kgsl_device *device);

void adreno_ringbuffer_read_pm4_ucode(struct kgsl_device *device);

int adreno_ringbuffer_waittimestamp(struct adreno_ringbuffer *rb,
					unsigned int timestamp,
					unsigned int msecs);

int adreno_rb_readtimestamp(struct adreno_device *adreno_dev,
	void *priv, enum kgsl_timestamp_type type,
	unsigned int *timestamp);

static inline int adreno_ringbuffer_count(struct adreno_ringbuffer *rb,
	unsigned int rptr)
{
	if (rb->wptr >= rptr)
		return rb->wptr - rptr;
	return rb->wptr + KGSL_RB_DWORDS - rptr;
}

/* Increment a value by 4 bytes with wrap-around based on size */
static inline unsigned int adreno_ringbuffer_inc_wrapped(unsigned int val,
							unsigned int size)
{
	return (val + sizeof(unsigned int)) % size;
}

/* Decrement a value by 4 bytes with wrap-around based on size */
static inline unsigned int adreno_ringbuffer_dec_wrapped(unsigned int val,
							unsigned int size)
{
	return (val + size - sizeof(unsigned int)) % size;
}

static inline int adreno_ringbuffer_set_pt_ctx(struct adreno_ringbuffer *rb,
		struct kgsl_pagetable *pt, struct adreno_context *context)
{
	return adreno_iommu_set_pt_ctx(rb, pt, context);
}

#endif  /* __ADRENO_RINGBUFFER_H */
