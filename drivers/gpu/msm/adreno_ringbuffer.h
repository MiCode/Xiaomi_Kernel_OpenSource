/* Copyright (c) 2002,2007-2013, The Linux Foundation. All rights reserved.
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

/*
 * Adreno ringbuffer sizes in bytes - these are converted to
 * the appropriate log2 values in the code
 */

#define KGSL_RB_SIZE (32 * 1024)
#define KGSL_RB_BLKSIZE 16

/* CP timestamp register */
#define	REG_CP_TIMESTAMP		 REG_SCRATCH_REG0


struct kgsl_device;
struct kgsl_device_private;

#define GSL_RB_MEMPTRS_SCRATCH_COUNT	 8
struct kgsl_rbmemptrs {
	int  rptr;
	int  wptr_poll;
};

#define GSL_RB_MEMPTRS_RPTR_OFFSET \
	(offsetof(struct kgsl_rbmemptrs, rptr))

#define GSL_RB_MEMPTRS_WPTRPOLL_OFFSET \
	(offsetof(struct kgsl_rbmemptrs, wptr_poll))

struct adreno_ringbuffer {
	struct kgsl_device *device;
	uint32_t flags;

	struct kgsl_memdesc buffer_desc;

	struct kgsl_memdesc memptrs_desc;
	struct kgsl_rbmemptrs *memptrs;

	/*ringbuffer size */
	unsigned int sizedwords;

	unsigned int wptr; /* write pointer offset in dwords from baseaddr */

	unsigned int global_ts;
};


#define GSL_RB_WRITE(device, ring, gpuaddr, data) \
	do { \
		*ring = data; \
		wmb(); \
		kgsl_cffdump_setmem(device, gpuaddr, data, 4); \
		ring++; \
		gpuaddr += sizeof(uint); \
	} while (0)

/* enable timestamp (...scratch0) memory shadowing */
#define GSL_RB_MEMPTRS_SCRATCH_MASK 0x1

/* mem rptr */
#define GSL_RB_CNTL_NO_UPDATE 0x0 /* enable */

/**
 * adreno_get_rptr - Get the current ringbuffer read pointer
 * @rb -  the ringbuffer
 *
 * Get the current read pointer, which is written by the GPU.
 */
static inline unsigned int
adreno_get_rptr(struct adreno_ringbuffer *rb)
{
	unsigned int result = rb->memptrs->rptr;
	rmb();
	return result;
}

#define GSL_RB_CNTL_POLL_EN 0x0 /* disable */

/*
 * protected mode error checking below register address 0x800
 * note: if CP_INTERRUPT packet is used then checking needs
 * to change to below register address 0x7C8
 */
#define GSL_RB_PROTECTED_MODE_CONTROL		0x200001F2

int adreno_ringbuffer_issueibcmds(struct kgsl_device_private *dev_priv,
				struct kgsl_context *context,
				struct kgsl_cmdbatch *cmdbatch,
				uint32_t *timestamp);

int adreno_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_cmdbatch *cmdbatch);

int adreno_ringbuffer_init(struct kgsl_device *device);

int adreno_ringbuffer_warm_start(struct adreno_ringbuffer *rb);

int adreno_ringbuffer_cold_start(struct adreno_ringbuffer *rb);

void adreno_ringbuffer_stop(struct adreno_ringbuffer *rb);

void adreno_ringbuffer_close(struct adreno_ringbuffer *rb);

unsigned int adreno_ringbuffer_issuecmds(struct kgsl_device *device,
					struct adreno_context *drawctxt,
					unsigned int flags,
					unsigned int *cmdaddr,
					int sizedwords);

void adreno_ringbuffer_submit(struct adreno_ringbuffer *rb);

void kgsl_cp_intrcallback(struct kgsl_device *device);

unsigned int *adreno_ringbuffer_allocspace(struct adreno_ringbuffer *rb,
						struct adreno_context *context,
						unsigned int numcmds);

int adreno_ringbuffer_read_pfp_ucode(struct kgsl_device *device);

int adreno_ringbuffer_read_pm4_ucode(struct kgsl_device *device);

static inline int adreno_ringbuffer_count(struct adreno_ringbuffer *rb,
	unsigned int rptr)
{
	if (rb->wptr >= rptr)
		return rb->wptr - rptr;
	return rb->wptr + rb->sizedwords - rptr;
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
