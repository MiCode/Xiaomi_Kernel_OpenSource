/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2002,2007-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_RINGBUFFER_H
#define __ADRENO_RINGBUFFER_H

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

/* Specifies that the command should be run in protected mode */
#define F_NOTPROTECTED		BIT(0)
/* Indicates that the CP should wait for idle after executing the command */
#define F_WFI			BIT(1)
/* Indicates that the poweron fixup should be executed before the command */
#define F_PWRON_FIXUP		BIT(2)
/* Indicates that the submission should be secure */
#define F_SECURE		BIT(3)
/* Indicates that the IBs in the submission should be skipped */
#define F_SKIP			BIT(4)
/* Indicates that user always on timer profiling is enabled */
#define F_USER_PROFILE		BIT(5)
/* Indicates that kernel always on timer profiling is enabled */
#define F_KERNEL_PROFILE	BIT(6)
/* Indicates that the submission has a preamble */
#define F_PREAMBLE		BIT(7)

#define IS_NOTPROTECTED(flags) ((flags) & F_NOTPROTECTED)
#define IS_WFI(flags) ((flags) & F_WFI)
#define IS_PWRON_FIXUP(flags) ((flags) & F_PWRON_FIXUP)
#define IS_SECURE(flags) ((flags) & F_SECURE)
#define IS_SKIP(flags) ((flags) & F_SKIP)
#define IS_USER_PROFILE(flags) ((flags) & F_USER_PROFILE)
#define IS_KERNEL_PROFILE(flags) ((flags) & F_KERNEL_PROFILE)
#define IS_PREAMBLE(flags) ((flags) & F_PREAMBLE)

struct kgsl_device;
struct kgsl_device_private;

/**
 * struct adreno_submit_time - utility structure to store the wall clock / GPU
 * ticks at command submit time
 * @ticks: GPU ticks at submit time (from the 19.2Mhz timer)
 * @ktime: local clock time (in nanoseconds)
 * @utime: Wall clock time
 * @drawobj: the object that we want to profile
 */
struct adreno_submit_time {
	uint64_t ticks;
	u64 ktime;
	struct timespec64 utime;
	struct kgsl_drawobj *drawobj;
};

/**
 * This is to keep track whether the SET_PSEUDO_REGISTER packet needs to be submitted
 * or not
 */
#define ADRENO_RB_SET_PSEUDO_DONE 0

/**
 * struct adreno_ringbuffer - Definition for an adreno ringbuffer object
 * @flags: Internal control flags for the ringbuffer
 * @buffer_desc: Pointer to the ringbuffer memory descriptor
 * @_wptr: The next value of wptr to be written to the hardware on submit
 * @wptr: Local copy of the wptr offset last written to hardware
 * @last_wptr: offset of the last wptr that was written to CFF
 * @rb_ctx: The context that represents a ringbuffer
 * @id: Priority level of the ringbuffer, also used as an ID
 * @fault_detect_ts: The last retired global timestamp read during fault detect
 * @timestamp: The RB's global timestamp
 * @events: A kgsl_event_group for this context - contains the list of GPU
 * events
 * @drawctxt_active: The last pagetable that this ringbuffer is set to
 * @preemption_desc: The memory descriptor containing
 * preemption info written/read by CP
 * @secure_preemption_desc: The memory descriptor containing
 * preemption info written/read by CP for secure contexts
 * @perfcounter_save_restore_desc: Used by CP to save/restore the perfcounter
 * values across preemption
 * and the commands to switch pagetable on the RB
 * @dispatch_q: The dispatcher side queue for this ringbuffer
 * @ts_expire_waitq: Wait queue to wait for rb timestamp to expire
 * @ts_expire_waitq: Wait q to wait for rb timestamp to expire
 * @wptr_preempt_end: Used during preemption to check that preemption occurred
 * at the right rptr
 * @gpr11: The gpr11 value of this RB
 * @preempted_midway: Indicates that the RB was preempted before rptr = wptr
 * @preempt_lock: Lock to protect the wptr pointer while it is being updated
 * @skip_inline_wptr: Used during preemption to make sure wptr is updated in
 * hardware
 */
struct adreno_ringbuffer {
	unsigned long flags;
	struct kgsl_memdesc *buffer_desc;
	unsigned int _wptr;
	unsigned int wptr;
	unsigned int last_wptr;
	int id;
	unsigned int fault_detect_ts;
	unsigned int timestamp;
	struct kgsl_event_group events;
	struct adreno_context *drawctxt_active;
	struct kgsl_memdesc *preemption_desc;
	struct kgsl_memdesc *secure_preemption_desc;
	struct kgsl_memdesc *perfcounter_save_restore_desc;
	struct adreno_dispatcher_drawqueue dispatch_q;
	wait_queue_head_t ts_expire_waitq;
	unsigned int wptr_preempt_end;
	unsigned int gpr11;
	int preempted_midway;
	spinlock_t preempt_lock;
	bool skip_inline_wptr;
	/**
	 * @profile_desc: global memory to construct IB1s to do user side
	 * profiling
	 */
	struct kgsl_memdesc *profile_desc;
	/**
	 * @profile_index: Pointer to the next "slot" in profile_desc for a user
	 * profiling IB1.  This allows for PAGE_SIZE / 16 = 256 simultaneous
	 * commands per ringbuffer with user profiling enabled
	 * enough.
	 */
	u32 profile_index;
};

/* Returns the current ringbuffer */
#define ADRENO_CURRENT_RINGBUFFER(a)	((a)->cur_rb)

int adreno_ringbuffer_issueibcmds(struct kgsl_device_private *dev_priv,
				struct kgsl_context *context,
				struct kgsl_drawobj *drawobj,
				uint32_t *timestamp);

/**
 * adreno_ringbuffer_setup - Do generic set up on a ringbuffer
 * @adreno_dev: Pointer to an Adreno GPU handle
 * @rb: Pointer to the ringbuffer struct to set up
 * @id: Index of the ringbuffer
 *
 * Set up generic memory and other bits of a ringbuffer.
 * Return: 0 on success or negative on error.
 */
int adreno_ringbuffer_setup(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, int id);

int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj,
		struct adreno_submit_time *time);


void adreno_ringbuffer_stop(struct adreno_device *adreno_dev);

void adreno_ringbuffer_submit(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time);

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

/**
 * adreno_ringbuffer_set_constraint - Set a system constraint before submission
 * @device: A KGSL GPU device handle
 * @drawobj: Pointer to the drawobj being sbumitted
 *
 * Check the drawobj to see if a constraint is applied and apply it.
 */
void adreno_ringbuffer_set_constraint(struct kgsl_device *device,
		struct kgsl_drawobj *drawobj);

void adreno_get_submit_time(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time);

#endif  /* __ADRENO_RINGBUFFER_H */
