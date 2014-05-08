/* Copyright (c) 2002,2007-2014, The Linux Foundation. All rights reserved.
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
 * struct adreno_ringbuffer - Definition for an adreno ringbuffer object
 * @device: KGSL device that owns the ringbuffer object
 * @flags: Internal control flags for the ringbuffer
 * @buffer_desc: Pointer to the ringbuffer memory descriptor
 * @wptr: Local copy of the wptr offset
 * @rptr: Read pointer offset in dwords from baseaddr
 * @last_wptr: offset of the last H/W committed wptr
 * @rb_ctx: The context that represents a ringbuffer
 * @id: Priority level of the ringbuffer, also used as an ID
 * @fault_detect_ts: The last retired global timestamp read during fault detect
 * @timestamp: The RB's global timestamp
 * @events: A kgsl_event_group for this context - contains the list of GPU
 * events
 * @mmu_events: A kgsl_event_group for this context - contains the list of mmu
 * events
 */
struct adreno_ringbuffer {
	struct kgsl_device *device;
	uint32_t flags;
	struct kgsl_memdesc buffer_desc;
	unsigned int sizedwords;
	unsigned int wptr;
	unsigned int rptr;
	unsigned int last_wptr;
	int id;
	unsigned int fault_detect_ts;
	unsigned int timestamp;
	struct kgsl_event_group events;
	struct kgsl_event_group mmu_events;
};

/**
 * struct adreno_ringbuffer_mmu_disable_clk_param - Parameter struct for
 * disble clk event
 * @rb: The rb pointer
 * @unit: The MMU unit whose clock is to be turned off
 * @ts: Timestamp on which clock is to be disabled
 */
struct adreno_ringbuffer_mmu_disable_clk_param {
	struct adreno_ringbuffer *rb;
	int unit;
	unsigned int ts;
};

/* enable timestamp (...scratch0) memory shadowing */
#define GSL_RB_MEMPTRS_SCRATCH_MASK 0x1

/*
 * protected mode error checking below register address 0x800
 * note: if CP_INTERRUPT packet is used then checking needs
 * to change to below register address 0x7C8
 */
#define GSL_RB_PROTECTED_MODE_CONTROL		0x200001F2

/* Returns the current ringbuffer */
#define ADRENO_CURRENT_RINGBUFFER(a)	((a)->cur_rb)

#define KGSL_MEMSTORE_RB_OFFSET(rb, field)	\
	KGSL_MEMSTORE_OFFSET((rb->id + KGSL_MEMSTORE_MAX), field)

int adreno_ringbuffer_issueibcmds(struct kgsl_device_private *dev_priv,
				struct kgsl_context *context,
				struct kgsl_cmdbatch *cmdbatch,
				uint32_t *timestamp);

int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_cmdbatch *cmdbatch);

int adreno_ringbuffer_init(struct kgsl_device *device);

int adreno_ringbuffer_warm_start(struct adreno_device *adreno_dev);

int adreno_ringbuffer_cold_start(struct adreno_device *adreno_dev);

void adreno_ringbuffer_stop(struct adreno_device *adreno_dev);

void adreno_ringbuffer_close(struct adreno_device *adreno_dev);

unsigned int adreno_ringbuffer_issuecmds(struct kgsl_device *device,
					struct adreno_context *drawctxt,
					unsigned int flags,
					unsigned int *cmdaddr,
					int sizedwords);

void adreno_ringbuffer_submit(struct adreno_ringbuffer *rb);

void kgsl_cp_intrcallback(struct kgsl_device *device);

unsigned int *adreno_ringbuffer_allocspace(struct adreno_ringbuffer *rb,
						unsigned int numcmds);

void adreno_ringbuffer_read_pfp_ucode(struct kgsl_device *device);

void adreno_ringbuffer_read_pm4_ucode(struct kgsl_device *device);

void adreno_ringbuffer_mmu_disable_clk_on_ts(struct kgsl_device *device,
			struct adreno_ringbuffer *rb, int unit);

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

#endif  /* __ADRENO_RINGBUFFER_H */
