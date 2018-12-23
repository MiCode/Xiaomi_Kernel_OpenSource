/* Copyright (c) 2010-2018, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/delay.h>

#include <asm/cacheflush.h>
#include <asm/compiler.h>

#include <soc/qcom/scm.h>

#define CREATE_TRACE_POINTS
#include <trace/events/scm.h>

#define SCM_ENOMEM		-5
#define SCM_EOPNOTSUPP		-4
#define SCM_EINVAL_ADDR		-3
#define SCM_EINVAL_ARG		-2
#define SCM_ERROR		-1
#define SCM_INTERRUPTED		1
#define SCM_EBUSY		-55
#define SCM_V2_EBUSY		-12

static DEFINE_MUTEX(scm_lock);

/*
 * MSM8996 V2 requires a lock to protect against
 * concurrent accesses between the limits management
 * driver and the clock controller
 */
DEFINE_MUTEX(scm_lmh_lock);

#define SCM_EBUSY_WAIT_MS 30
#define SCM_EBUSY_MAX_RETRY 67

#define N_EXT_SCM_ARGS 7
#define FIRST_EXT_ARG_IDX 3
#define SMC_ATOMIC_SYSCALL 31
#define N_REGISTER_ARGS (MAX_SCM_ARGS - N_EXT_SCM_ARGS + 1)
#define SMC64_MASK 0x40000000
#define SMC_ATOMIC_MASK 0x80000000
#define IS_CALL_AVAIL_CMD 1

#define SCM_BUF_LEN(__cmd_size, __resp_size) ({ \
	size_t x =  __cmd_size + __resp_size; \
	size_t y = sizeof(struct scm_command) + sizeof(struct scm_response); \
	size_t result; \
	if (x < __cmd_size || (x + y) < x) \
		result = 0; \
	else \
		result = x + y; \
	result; \
	})
/**
 * struct scm_command - one SCM command buffer
 * @len: total available memory for command and response
 * @buf_offset: start of command buffer
 * @resp_hdr_offset: start of response buffer
 * @id: command to be executed
 * @buf: buffer returned from scm_get_command_buffer()
 *
 * An SCM command is laid out in memory as follows:
 *
 *	------------------- <--- struct scm_command
 *	| command header  |
 *	------------------- <--- scm_get_command_buffer()
 *	| command buffer  |
 *	------------------- <--- struct scm_response and
 *	| response header |      scm_command_to_response()
 *	------------------- <--- scm_get_response_buffer()
 *	| response buffer |
 *	-------------------
 *
 * There can be arbitrary padding between the headers and buffers so
 * you should always use the appropriate scm_get_*_buffer() routines
 * to access the buffers in a safe manner.
 */
struct scm_command {
	u32	len;
	u32	buf_offset;
	u32	resp_hdr_offset;
	u32	id;
	u32	buf[0];
};

/**
 * struct scm_response - one SCM response buffer
 * @len: total available memory for response
 * @buf_offset: start of response data relative to start of scm_response
 * @is_complete: indicates if the command has finished processing
 */
struct scm_response {
	u32	len;
	u32	buf_offset;
	u32	is_complete;
};

#ifdef CONFIG_ARM64

#define R0_STR "x0"
#define R1_STR "x1"
#define R2_STR "x2"
#define R3_STR "x3"
#define R4_STR "x4"
#define R5_STR "x5"
#define R6_STR "x6"

/* Outer caches unsupported on ARM64 platforms */
#define outer_inv_range(x, y)
#define outer_flush_range(x, y)

#define __cpuc_flush_dcache_area __flush_dcache_area

#else

#define R0_STR "r0"
#define R1_STR "r1"
#define R2_STR "r2"
#define R3_STR "r3"
#define R4_STR "r4"
#define R5_STR "r5"
#define R6_STR "r6"

#endif

/**
 * scm_command_to_response() - Get a pointer to a scm_response
 * @cmd: command
 *
 * Returns a pointer to a response for a command.
 */
static inline struct scm_response *scm_command_to_response(
		const struct scm_command *cmd)
{
	return (void *)cmd + cmd->resp_hdr_offset;
}

/**
 * scm_get_command_buffer() - Get a pointer to a command buffer
 * @cmd: command
 *
 * Returns a pointer to the command buffer of a command.
 */
static inline void *scm_get_command_buffer(const struct scm_command *cmd)
{
	return (void *)cmd->buf;
}

/**
 * scm_get_response_buffer() - Get a pointer to a response buffer
 * @rsp: response
 *
 * Returns a pointer to a response buffer of a response.
 */
static inline void *scm_get_response_buffer(const struct scm_response *rsp)
{
	return (void *)rsp + rsp->buf_offset;
}

static int scm_remap_error(int err)
{
	switch (err) {
	case SCM_ERROR:
		return -EIO;
	case SCM_EINVAL_ADDR:
	case SCM_EINVAL_ARG:
		return -EINVAL;
	case SCM_EOPNOTSUPP:
		return -EOPNOTSUPP;
	case SCM_ENOMEM:
		return -ENOMEM;
	case SCM_EBUSY:
	case SCM_V2_EBUSY:
		return -EBUSY;
	}
	return -EINVAL;
}

static u32 smc(u32 cmd_addr)
{
	int context_id;
	register u32 r0 asm("r0") = 1;
	register u32 r1 asm("r1") = (uintptr_t)&context_id;
	register u32 r2 asm("r2") = cmd_addr;
	do {
		asm volatile(
			__asmeq("%0", R0_STR)
			__asmeq("%1", R0_STR)
			__asmeq("%2", R1_STR)
			__asmeq("%3", R2_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc	#0\n"
			: "=r" (r0)
			: "r" (r0), "r" (r1), "r" (r2)
			: "r3");
	} while (r0 == SCM_INTERRUPTED);

	return r0;
}

static int __scm_call(const struct scm_command *cmd)
{
	int ret;
	u32 cmd_addr = virt_to_phys(cmd);

	/*
	 * Flush the command buffer so that the secure world sees
	 * the correct data.
	 */
	__cpuc_flush_dcache_area((void *)cmd, cmd->len);
	outer_flush_range(cmd_addr, cmd_addr + cmd->len);

	ret = smc(cmd_addr);
	if (ret < 0) {
		if (ret != SCM_EBUSY)
			pr_err("scm_call failed with error code %d\n", ret);
		ret = scm_remap_error(ret);
	}
	return ret;
}

#ifndef CONFIG_ARM64
static void scm_inv_range(unsigned long start, unsigned long end)
{
	u32 cacheline_size, ctr;

	asm volatile("mrc p15, 0, %0, c0, c0, 1" : "=r" (ctr));
	cacheline_size = 4 << ((ctr >> 16) & 0xf);

	start = round_down(start, cacheline_size);
	end = round_up(end, cacheline_size);
	outer_inv_range(start, end);
	while (start < end) {
		asm ("mcr p15, 0, %0, c7, c6, 1" : : "r" (start)
		     : "memory");
		start += cacheline_size;
	}
	dsb();
	isb();
}
#else

static void scm_inv_range(unsigned long start, unsigned long end)
{
	dmac_inv_range((void *)start, (void *)end);
}
#endif

/**
 * scm_call_common() - Send an SCM command
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @cmd_buf: command buffer
 * @cmd_len: length of the command buffer
 * @resp_buf: response buffer
 * @resp_len: length of the response buffer
 * @scm_buf: internal scm structure used for passing data
 * @scm_buf_len: length of the internal scm structure
 *
 * Core function to scm call. Initializes the given cmd structure with
 * appropriate values and makes the actual scm call. Validation of cmd
 * pointer and length must occur in the calling function.
 *
 * Returns the appropriate error code from the scm call
 */

static int scm_call_common(u32 svc_id, u32 cmd_id, const void *cmd_buf,
				size_t cmd_len, void *resp_buf, size_t resp_len,
				struct scm_command *scm_buf,
				size_t scm_buf_length)
{
	int ret;
	struct scm_response *rsp;
	unsigned long start, end;

	scm_buf->len = scm_buf_length;
	scm_buf->buf_offset = offsetof(struct scm_command, buf);
	scm_buf->resp_hdr_offset = scm_buf->buf_offset + cmd_len;
	scm_buf->id = (svc_id << 10) | cmd_id;

	if (cmd_buf)
		memcpy(scm_get_command_buffer(scm_buf), cmd_buf, cmd_len);

	mutex_lock(&scm_lock);
	ret = __scm_call(scm_buf);
	mutex_unlock(&scm_lock);
	if (ret)
		return ret;

	rsp = scm_command_to_response(scm_buf);
	start = (unsigned long)rsp;

	do {
		scm_inv_range(start, start + sizeof(*rsp));
	} while (!rsp->is_complete);

	end = (unsigned long)scm_get_response_buffer(rsp) + resp_len;
	scm_inv_range(start, end);

	if (resp_buf)
		memcpy(resp_buf, scm_get_response_buffer(rsp), resp_len);

	return ret;
}

/*
 * Sometimes the secure world may be busy waiting for a particular resource.
 * In those situations, it is expected that the secure world returns a special
 * error code (SCM_EBUSY). Retry any scm_call that fails with this error code,
 * but with a timeout in place. Also, don't move this into scm_call_common,
 * since we want the first attempt to be the "fastpath".
 */
static int _scm_call_retry(u32 svc_id, u32 cmd_id, const void *cmd_buf,
				size_t cmd_len, void *resp_buf, size_t resp_len,
				struct scm_command *cmd,
				size_t len)
{
	int ret, retry_count = 0;

	do {
		ret = scm_call_common(svc_id, cmd_id, cmd_buf, cmd_len,
					resp_buf, resp_len, cmd, len);
		if (ret == -EBUSY)
			msleep(SCM_EBUSY_WAIT_MS);
		if (retry_count == 33)
			pr_warn("scm: secure world has been busy for 1 second!\n");
	} while (ret == -EBUSY && (retry_count++ < SCM_EBUSY_MAX_RETRY));

	if (ret == -EBUSY)
		pr_err("scm: secure world busy (rc = SCM_EBUSY)\n");

	return ret;
}

/**
 * scm_call_noalloc - Send an SCM command
 *
 * Same as scm_call except clients pass in a buffer (@scm_buf) to be used for
 * scm internal structures. The buffer should be allocated with
 * DEFINE_SCM_BUFFER to account for the proper alignment and size.
 */
int scm_call_noalloc(u32 svc_id, u32 cmd_id, const void *cmd_buf,
		size_t cmd_len, void *resp_buf, size_t resp_len,
		void *scm_buf, size_t scm_buf_len)
{
	int ret;
	size_t len = SCM_BUF_LEN(cmd_len, resp_len);

	if (len == 0)
		return -EINVAL;

	if (!IS_ALIGNED((unsigned long)scm_buf, PAGE_SIZE))
		return -EINVAL;

	memset(scm_buf, 0, scm_buf_len);

	ret = scm_call_common(svc_id, cmd_id, cmd_buf, cmd_len, resp_buf,
				resp_len, scm_buf, len);
	return ret;

}

#ifdef CONFIG_ARM64

static int __scm_call_armv8_64(u64 x0, u64 x1, u64 x2, u64 x3, u64 x4, u64 x5,
				u64 *ret1, u64 *ret2, u64 *ret3)
{
	register u64 r0 asm("r0") = x0;
	register u64 r1 asm("r1") = x1;
	register u64 r2 asm("r2") = x2;
	register u64 r3 asm("r3") = x3;
	register u64 r4 asm("r4") = x4;
	register u64 r5 asm("r5") = x5;
	register u64 r6 asm("r6") = 0;

	do {
		asm volatile(
			__asmeq("%0", R0_STR)
			__asmeq("%1", R1_STR)
			__asmeq("%2", R2_STR)
			__asmeq("%3", R3_STR)
			__asmeq("%4", R4_STR)
			__asmeq("%5", R5_STR)
			__asmeq("%6", R6_STR)
			__asmeq("%7", R0_STR)
			__asmeq("%8", R1_STR)
			__asmeq("%9", R2_STR)
			__asmeq("%10", R3_STR)
			__asmeq("%11", R4_STR)
			__asmeq("%12", R5_STR)
			__asmeq("%13", R6_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc	#0\n"
			: "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3),
			  "=r" (r4), "=r" (r5), "=r" (r6)
			: "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4),
			  "r" (r5), "r" (r6)
			: "x7", "x8", "x9", "x10", "x11", "x12", "x13",
			  "x14", "x15", "x16", "x17");
	} while (r0 == SCM_INTERRUPTED);

	if (ret1)
		*ret1 = r1;
	if (ret2)
		*ret2 = r2;
	if (ret3)
		*ret3 = r3;

	return r0;
}

static int __scm_call_armv8_32(u32 w0, u32 w1, u32 w2, u32 w3, u32 w4, u32 w5,
				u64 *ret1, u64 *ret2, u64 *ret3)
{
	register u32 r0 asm("r0") = w0;
	register u32 r1 asm("r1") = w1;
	register u32 r2 asm("r2") = w2;
	register u32 r3 asm("r3") = w3;
	register u32 r4 asm("r4") = w4;
	register u32 r5 asm("r5") = w5;
	register u32 r6 asm("r6") = 0;

	do {
		asm volatile(
			__asmeq("%0", R0_STR)
			__asmeq("%1", R1_STR)
			__asmeq("%2", R2_STR)
			__asmeq("%3", R3_STR)
			__asmeq("%4", R4_STR)
			__asmeq("%5", R5_STR)
			__asmeq("%6", R6_STR)
			__asmeq("%7", R0_STR)
			__asmeq("%8", R1_STR)
			__asmeq("%9", R2_STR)
			__asmeq("%10", R3_STR)
			__asmeq("%11", R4_STR)
			__asmeq("%12", R5_STR)
			__asmeq("%13", R6_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc	#0\n"
			: "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3),
			  "=r" (r4), "=r" (r5), "=r" (r6)
			: "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4),
			  "r" (r5), "r" (r6)
			: "x7", "x8", "x9", "x10", "x11", "x12", "x13",
			"x14", "x15", "x16", "x17");

	} while (r0 == SCM_INTERRUPTED);

	if (ret1)
		*ret1 = r1;
	if (ret2)
		*ret2 = r2;
	if (ret3)
		*ret3 = r3;

	return r0;
}

#else

static int __scm_call_armv8_32(u32 w0, u32 w1, u32 w2, u32 w3, u32 w4, u32 w5,
				u64 *ret1, u64 *ret2, u64 *ret3)
{
	register u32 r0 asm("r0") = w0;
	register u32 r1 asm("r1") = w1;
	register u32 r2 asm("r2") = w2;
	register u32 r3 asm("r3") = w3;
	register u32 r4 asm("r4") = w4;
	register u32 r5 asm("r5") = w5;
	register u32 r6 asm("r6") = 0;

	do {
		asm volatile(
			__asmeq("%0", R0_STR)
			__asmeq("%1", R1_STR)
			__asmeq("%2", R2_STR)
			__asmeq("%3", R3_STR)
			__asmeq("%4", R4_STR)
			__asmeq("%5", R5_STR)
			__asmeq("%6", R6_STR)
			__asmeq("%7", R0_STR)
			__asmeq("%8", R1_STR)
			__asmeq("%9", R2_STR)
			__asmeq("%10", R3_STR)
			__asmeq("%11", R4_STR)
			__asmeq("%12", R5_STR)
			__asmeq("%13", R6_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc	#0\n"
			: "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3),
			  "=r" (r4), "=r" (r5), "=r" (r6)
			: "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4),
			 "r" (r5), "r" (r6));

	} while (r0 == SCM_INTERRUPTED);

	if (ret1)
		*ret1 = r1;
	if (ret2)
		*ret2 = r2;
	if (ret3)
		*ret3 = r3;

	return r0;
}

static int __scm_call_armv8_64(u64 x0, u64 x1, u64 x2, u64 x3, u64 x4, u64 x5,
				u64 *ret1, u64 *ret2, u64 *ret3)
{
	return 0;
}
#endif

struct scm_extra_arg {
	union {
		u32 args32[N_EXT_SCM_ARGS];
		u64 args64[N_EXT_SCM_ARGS];
	};
};

static enum scm_interface_version {
	SCM_UNKNOWN,
	SCM_LEGACY,
	SCM_ARMV8_32,
	SCM_ARMV8_64,
} scm_version = SCM_UNKNOWN;

/* This will be set to specify SMC32 or SMC64 */
static u32 scm_version_mask;

bool is_scm_armv8(void)
{
	int ret;
	u64 ret1, x0;

	if (likely(scm_version != SCM_UNKNOWN))
		return (scm_version == SCM_ARMV8_32) ||
			(scm_version == SCM_ARMV8_64);
	/*
	 * This is a one time check that runs on the first ever
	 * invocation of is_scm_armv8. We might be called in atomic
	 * context so no mutexes etc. Also, we can't use the scm_call2
	 * or scm_call2_APIs directly since they depend on this init.
	 */

	/* First try a SMC64 call */
	scm_version = SCM_ARMV8_64;
	ret1 = 0;
	x0 = SCM_SIP_FNID(SCM_SVC_INFO, IS_CALL_AVAIL_CMD) | SMC_ATOMIC_MASK;
	ret = __scm_call_armv8_64(x0 | SMC64_MASK, SCM_ARGS(1), x0, 0, 0, 0,
				  &ret1, NULL, NULL);
	if (ret || !ret1) {
		/* Try SMC32 call */
		ret1 = 0;
		ret = __scm_call_armv8_32(x0, SCM_ARGS(1), x0, 0, 0, 0,
					  &ret1, NULL, NULL);
		if (ret || !ret1)
			scm_version = SCM_LEGACY;
		else
			scm_version = SCM_ARMV8_32;
	} else
		scm_version_mask = SMC64_MASK;

	pr_debug("scm_call: scm version is %x, mask is %x\n", scm_version,
		  scm_version_mask);

	return (scm_version == SCM_ARMV8_32) ||
			(scm_version == SCM_ARMV8_64);
}
EXPORT_SYMBOL(is_scm_armv8);

/*
 * If there are more than N_REGISTER_ARGS, allocate a buffer and place
 * the additional arguments in it. The extra argument buffer will be
 * pointed to by X5.
 */
static int allocate_extra_arg_buffer(struct scm_desc *desc, gfp_t flags)
{
	int i, j;
	struct scm_extra_arg *argbuf;
	int arglen = desc->arginfo & 0xf;
	size_t argbuflen = PAGE_ALIGN(sizeof(struct scm_extra_arg));

	desc->x5 = desc->args[FIRST_EXT_ARG_IDX];

	if (likely(arglen <= N_REGISTER_ARGS)) {
		desc->extra_arg_buf = NULL;
		return 0;
	}

	argbuf = kzalloc(argbuflen, flags);
	if (!argbuf) {
		pr_err("scm_call: failed to alloc mem for extended argument buffer\n");
		return -ENOMEM;
	}

	desc->extra_arg_buf = argbuf;

	j = FIRST_EXT_ARG_IDX;
	if (scm_version == SCM_ARMV8_64)
		for (i = 0; i < N_EXT_SCM_ARGS; i++)
			argbuf->args64[i] = desc->args[j++];
	else
		for (i = 0; i < N_EXT_SCM_ARGS; i++)
			argbuf->args32[i] = desc->args[j++];
	desc->x5 = virt_to_phys(argbuf);
	__cpuc_flush_dcache_area(argbuf, argbuflen);
	outer_flush_range(virt_to_phys(argbuf),
			  virt_to_phys(argbuf) + argbuflen);

	return 0;
}

static int __scm_call2(u32 fn_id, struct scm_desc *desc, bool retry)
{
	int arglen = desc->arginfo & 0xf;
	int ret, retry_count = 0;
	u64 x0;

	if (unlikely(!is_scm_armv8()))
		return -ENODEV;

	ret = allocate_extra_arg_buffer(desc, GFP_NOIO);
	if (ret)
		return ret;

	x0 = fn_id | scm_version_mask;
	do {
		mutex_lock(&scm_lock);

		if (SCM_SVC_ID(fn_id) == SCM_SVC_LMH)
			mutex_lock(&scm_lmh_lock);

		desc->ret[0] = desc->ret[1] = desc->ret[2] = 0;

		trace_scm_call_start(x0, desc);

		if (scm_version == SCM_ARMV8_64)
			ret = __scm_call_armv8_64(x0, desc->arginfo,
						  desc->args[0], desc->args[1],
						  desc->args[2], desc->x5,
						  &desc->ret[0], &desc->ret[1],
						  &desc->ret[2]);
		else
			ret = __scm_call_armv8_32(x0, desc->arginfo,
						  desc->args[0], desc->args[1],
						  desc->args[2], desc->x5,
						  &desc->ret[0], &desc->ret[1],
						  &desc->ret[2]);

		trace_scm_call_end(desc);

		if (SCM_SVC_ID(fn_id) == SCM_SVC_LMH)
			mutex_unlock(&scm_lmh_lock);

		mutex_unlock(&scm_lock);
		if (!retry)
			goto out;

		if (ret == SCM_V2_EBUSY)
			msleep(SCM_EBUSY_WAIT_MS);
		if (retry_count == 33)
			pr_warn("scm: secure world has been busy for 1 second!\n");
	} while (ret == SCM_V2_EBUSY && (retry_count++ < SCM_EBUSY_MAX_RETRY));
out:
	if (ret < 0)
		pr_err("scm_call failed: func id %#llx, ret: %d, syscall returns: %#llx, %#llx, %#llx\n",
			x0, ret, desc->ret[0], desc->ret[1], desc->ret[2]);

	if (arglen > N_REGISTER_ARGS)
		kfree(desc->extra_arg_buf);
	if (ret < 0)
		return scm_remap_error(ret);
	return 0;
}

/**
 * scm_call2() - Invoke a syscall in the secure world
 * @fn_id: The function ID for this syscall
 * @desc: Descriptor structure containing arguments and return values
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This should *only* be called in pre-emptible context.
 *
 * A note on cache maintenance:
 * Note that any buffers that are expected to be accessed by the secure world
 * must be flushed before invoking scm_call and invalidated in the cache
 * immediately after scm_call returns. An important point that must be noted
 * is that on ARMV8 architectures, invalidation actually also causes a dirty
 * cache line to be cleaned (flushed + unset-dirty-bit). Therefore it is of
 * paramount importance that the buffer be flushed before invoking scm_call2,
 * even if you don't care about the contents of that buffer.
 *
 * Note that cache maintenance on the argument buffer (desc->args) is taken care
 * of by scm_call2; however, callers are responsible for any other cached
 * buffers passed over to the secure world.
 */
int scm_call2(u32 fn_id, struct scm_desc *desc)
{
	return __scm_call2(fn_id, desc, true);
}
EXPORT_SYMBOL(scm_call2);

/**
 * scm_call2_noretry() - Invoke a syscall in the secure world
 *
 * Similar to scm_call2 except that there is no retry mechanism
 * implemented.
 */
int scm_call2_noretry(u32 fn_id, struct scm_desc *desc)
{
	return __scm_call2(fn_id, desc, false);
}
EXPORT_SYMBOL(scm_call2_noretry);

/**
 * scm_call2_atomic() - Invoke a syscall in the secure world
 *
 * Similar to scm_call2 except that this can be invoked in atomic context.
 * There is also no retry mechanism implemented. Please ensure that the
 * secure world syscall can be executed in such a context and can complete
 * in a timely manner.
 */
int scm_call2_atomic(u32 fn_id, struct scm_desc *desc)
{
	int arglen = desc->arginfo & 0xf;
	int ret;
	u64 x0;

	if (unlikely(!is_scm_armv8()))
		return -ENODEV;

	ret = allocate_extra_arg_buffer(desc, GFP_ATOMIC);
	if (ret)
		return ret;

	x0 = fn_id | BIT(SMC_ATOMIC_SYSCALL) | scm_version_mask;

	if (scm_version == SCM_ARMV8_64)
		ret = __scm_call_armv8_64(x0, desc->arginfo, desc->args[0],
					  desc->args[1], desc->args[2],
					  desc->x5, &desc->ret[0],
					  &desc->ret[1], &desc->ret[2]);
	else
		ret = __scm_call_armv8_32(x0, desc->arginfo, desc->args[0],
					  desc->args[1], desc->args[2],
					  desc->x5, &desc->ret[0],
					  &desc->ret[1], &desc->ret[2]);
	if (ret < 0)
		pr_err("scm_call failed: func id %#llx, ret: %d, syscall returns: %#llx, %#llx, %#llx\n",
			x0, ret, desc->ret[0],
			desc->ret[1], desc->ret[2]);

	if (arglen > N_REGISTER_ARGS)
		kfree(desc->extra_arg_buf);
	if (ret < 0)
		return scm_remap_error(ret);
	return ret;
}

/**
 * scm_call() - Send an SCM command
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @cmd_buf: command buffer
 * @cmd_len: length of the command buffer
 * @resp_buf: response buffer
 * @resp_len: length of the response buffer
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 *
 * A note on cache maintenance:
 * Note that any buffers that are expected to be accessed by the secure world
 * must be flushed before invoking scm_call and invalidated in the cache
 * immediately after scm_call returns. Cache maintenance on the command and
 * response buffers is taken care of by scm_call; however, callers are
 * responsible for any other cached buffers passed over to the secure world.
 */
int scm_call(u32 svc_id, u32 cmd_id, const void *cmd_buf, size_t cmd_len,
		void *resp_buf, size_t resp_len)
{
	struct scm_command *cmd;
	int ret;
	size_t len = SCM_BUF_LEN(cmd_len, resp_len);

	if (len == 0 || PAGE_ALIGN(len) < len)
		return -EINVAL;

	cmd = kzalloc(PAGE_ALIGN(len), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	ret = scm_call_common(svc_id, cmd_id, cmd_buf, cmd_len, resp_buf,
				resp_len, cmd, len);
	if (unlikely(ret == -EBUSY))
		ret = _scm_call_retry(svc_id, cmd_id, cmd_buf, cmd_len,
				      resp_buf, resp_len, cmd, PAGE_ALIGN(len));
	kfree(cmd);
	return ret;
}
EXPORT_SYMBOL(scm_call);

#define SCM_CLASS_REGISTER	(0x2 << 8)
#define SCM_MASK_IRQS		BIT(5)
#define SCM_ATOMIC(svc, cmd, n) (((((svc) << 10)|((cmd) & 0x3ff)) << 12) | \
				SCM_CLASS_REGISTER | \
				SCM_MASK_IRQS | \
				(n & 0xf))

/**
 * scm_call_atomic1() - Send an atomic SCM command with one argument
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @arg1: first argument
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
s32 scm_call_atomic1(u32 svc, u32 cmd, u32 arg1)
{
	int context_id;
	register u32 r0 asm("r0") = SCM_ATOMIC(svc, cmd, 1);
	register u32 r1 asm("r1") = (uintptr_t)&context_id;
	register u32 r2 asm("r2") = arg1;

	asm volatile(
		__asmeq("%0", R0_STR)
		__asmeq("%1", R0_STR)
		__asmeq("%2", R1_STR)
		__asmeq("%3", R2_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
		"smc	#0\n"
		: "=r" (r0)
		: "r" (r0), "r" (r1), "r" (r2)
		: "r3");
	return r0;
}
EXPORT_SYMBOL(scm_call_atomic1);

/**
 * scm_call_atomic1_1() - SCM command with one argument and one return value
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @arg1: first argument
 * @ret1: first return value
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
s32 scm_call_atomic1_1(u32 svc, u32 cmd, u32 arg1, u32 *ret1)
{
	int context_id;
	register u32 r0 asm("r0") = SCM_ATOMIC(svc, cmd, 1);
	register u32 r1 asm("r1") = (uintptr_t)&context_id;
	register u32 r2 asm("r2") = arg1;

	asm volatile(
		__asmeq("%0", R0_STR)
		__asmeq("%1", R1_STR)
		__asmeq("%2", R0_STR)
		__asmeq("%3", R1_STR)
		__asmeq("%4", R2_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
		"smc	#0\n"
		: "=r" (r0), "=r" (r1)
		: "r" (r0), "r" (r1), "r" (r2)
		: "r3");
	if (ret1)
		*ret1 = r1;
	return r0;
}
EXPORT_SYMBOL(scm_call_atomic1_1);

/**
 * scm_call_atomic2() - Send an atomic SCM command with two arguments
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @arg1: first argument
 * @arg2: second argument
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
s32 scm_call_atomic2(u32 svc, u32 cmd, u32 arg1, u32 arg2)
{
	int context_id;
	register u32 r0 asm("r0") = SCM_ATOMIC(svc, cmd, 2);
	register u32 r1 asm("r1") = (uintptr_t)&context_id;
	register u32 r2 asm("r2") = arg1;
	register u32 r3 asm("r3") = arg2;

	asm volatile(
		__asmeq("%0", R0_STR)
		__asmeq("%1", R0_STR)
		__asmeq("%2", R1_STR)
		__asmeq("%3", R2_STR)
		__asmeq("%4", R3_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
		"smc	#0\n"
		: "=r" (r0)
		: "r" (r0), "r" (r1), "r" (r2), "r" (r3));
	return r0;
}
EXPORT_SYMBOL(scm_call_atomic2);

/**
 * scm_call_atomic3() - Send an atomic SCM command with three arguments
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @arg1: first argument
 * @arg2: second argument
 * @arg3: third argument
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
s32 scm_call_atomic3(u32 svc, u32 cmd, u32 arg1, u32 arg2, u32 arg3)
{
	int context_id;
	register u32 r0 asm("r0") = SCM_ATOMIC(svc, cmd, 3);
	register u32 r1 asm("r1") = (uintptr_t)&context_id;
	register u32 r2 asm("r2") = arg1;
	register u32 r3 asm("r3") = arg2;
	register u32 r4 asm("r4") = arg3;

	asm volatile(
		__asmeq("%0", R0_STR)
		__asmeq("%1", R0_STR)
		__asmeq("%2", R1_STR)
		__asmeq("%3", R2_STR)
		__asmeq("%4", R3_STR)
		__asmeq("%5", R4_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
		"smc	#0\n"
		: "=r" (r0)
		: "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4));
	return r0;
}
EXPORT_SYMBOL(scm_call_atomic3);

s32 scm_call_atomic4_3(u32 svc, u32 cmd, u32 arg1, u32 arg2,
		u32 arg3, u32 arg4, u32 *ret1, u32 *ret2)
{
	int ret;
	int context_id;
	register u32 r0 asm("r0") = SCM_ATOMIC(svc, cmd, 4);
	register u32 r1 asm("r1") = (uintptr_t)&context_id;
	register u32 r2 asm("r2") = arg1;
	register u32 r3 asm("r3") = arg2;
	register u32 r4 asm("r4") = arg3;
	register u32 r5 asm("r5") = arg4;

	asm volatile(
		__asmeq("%0", R0_STR)
		__asmeq("%1", R1_STR)
		__asmeq("%2", R2_STR)
		__asmeq("%3", R0_STR)
		__asmeq("%4", R1_STR)
		__asmeq("%5", R2_STR)
		__asmeq("%6", R3_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
		"smc	#0\n"
		: "=r" (r0), "=r" (r1), "=r" (r2)
		: "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4), "r" (r5));
	ret = r0;
	if (ret1)
		*ret1 = r1;
	if (ret2)
		*ret2 = r2;
	return r0;
}
EXPORT_SYMBOL(scm_call_atomic4_3);

/**
 * scm_call_atomic5_3() - SCM command with five argument and three return value
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @arg1: first argument
 * @arg2: second argument
 * @arg3: third argument
 * @arg4: fourth argument
 * @arg5: fifth argument
 * @ret1: first return value
 * @ret2: second return value
 * @ret3: third return value
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
s32 scm_call_atomic5_3(u32 svc, u32 cmd, u32 arg1, u32 arg2,
	u32 arg3, u32 arg4, u32 arg5, u32 *ret1, u32 *ret2, u32 *ret3)
{
	int ret;
	int context_id;
	register u32 r0 asm("r0") = SCM_ATOMIC(svc, cmd, 5);
	register u32 r1 asm("r1") = (uintptr_t)&context_id;
	register u32 r2 asm("r2") = arg1;
	register u32 r3 asm("r3") = arg2;
	register u32 r4 asm("r4") = arg3;
	register u32 r5 asm("r5") = arg4;
	register u32 r6 asm("r6") = arg5;

	asm volatile(
		__asmeq("%0", R0_STR)
		__asmeq("%1", R1_STR)
		__asmeq("%2", R2_STR)
		__asmeq("%3", R3_STR)
		__asmeq("%4", R0_STR)
		__asmeq("%5", R1_STR)
		__asmeq("%6", R2_STR)
		__asmeq("%7", R3_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
		"smc	#0\n"
		: "=r" (r0), "=r" (r1), "=r" (r2), "=r" (r3)
		: "r" (r0), "r" (r1), "r" (r2), "r" (r3), "r" (r4), "r" (r5),
		 "r" (r6));
	ret = r0;

	if (ret1)
		*ret1 = r1;
	if (ret2)
		*ret2 = r2;
	if (ret3)
		*ret3 = r3;
	return r0;
}
EXPORT_SYMBOL(scm_call_atomic5_3);

u32 scm_get_version(void)
{
	int context_id;
	static u32 version = -1;
	register u32 r0 asm("r0");
	register u32 r1 asm("r1");

	if (version != -1)
		return version;

	mutex_lock(&scm_lock);

	r0 = 0x1 << 8;
	r1 = (uintptr_t)&context_id;
	do {
		asm volatile(
			__asmeq("%0", R0_STR)
			__asmeq("%1", R1_STR)
			__asmeq("%2", R0_STR)
			__asmeq("%3", R1_STR)
#ifdef REQUIRES_SEC
			".arch_extension sec\n"
#endif
			"smc	#0\n"
			: "=r" (r0), "=r" (r1)
			: "r" (r0), "r" (r1)
			: "r2", "r3");
	} while (r0 == SCM_INTERRUPTED);

	version = r1;
	mutex_unlock(&scm_lock);

	return version;
}
EXPORT_SYMBOL(scm_get_version);

#define SCM_IO_READ	0x1
#define SCM_IO_WRITE	0x2

u32 scm_io_read(phys_addr_t address)
{
	if (!is_scm_armv8()) {
		return scm_call_atomic1(SCM_SVC_IO, SCM_IO_READ, address);
	} else {
		struct scm_desc desc = {
			.args[0] = address,
			.arginfo = SCM_ARGS(1),
		};
		scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_IO, SCM_IO_READ), &desc);
		return desc.ret[0];
	}
}
EXPORT_SYMBOL(scm_io_read);

int scm_io_write(phys_addr_t address, u32 val)
{
	int ret;

	if (!is_scm_armv8()) {
		ret = scm_call_atomic2(SCM_SVC_IO, SCM_IO_WRITE, address, val);
	} else {
		struct scm_desc desc = {
			.args[0] = address,
			.args[1] = val,
			.arginfo = SCM_ARGS(2),
		};
		ret = scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_IO, SCM_IO_WRITE),
				       &desc);
	}
	return ret;
}
EXPORT_SYMBOL(scm_io_write);

int scm_is_call_available(u32 svc_id, u32 cmd_id)
{
	int ret;
	struct scm_desc desc = {0};

	if (!is_scm_armv8()) {
		u32 ret_val = 0;
		u32 svc_cmd = (svc_id << 10) | cmd_id;

		ret = scm_call(SCM_SVC_INFO, IS_CALL_AVAIL_CMD, &svc_cmd,
			sizeof(svc_cmd), &ret_val, sizeof(ret_val));
		if (!ret && ret_val)
			return 1;
		else
			return 0;

	}
	desc.arginfo = SCM_ARGS(1);
	desc.args[0] = SCM_SIP_FNID(svc_id, cmd_id);
	desc.ret[0] = 0;
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_INFO, IS_CALL_AVAIL_CMD), &desc);
	if (!ret && desc.ret[0])
		return 1;
	else
		return 0;

}
EXPORT_SYMBOL(scm_is_call_available);

#define GET_FEAT_VERSION_CMD	3
int scm_get_feat_version(u32 feat, u64 *scm_ret)
{
	struct scm_desc desc = {0};
	int ret;

	if (!is_scm_armv8()) {
		if (scm_is_call_available(SCM_SVC_INFO, GET_FEAT_VERSION_CMD)) {
			ret = scm_call(SCM_SVC_INFO, GET_FEAT_VERSION_CMD,
				&feat, sizeof(feat), scm_ret, sizeof(*scm_ret));
			return ret;
		}
	}

	ret = scm_is_call_available(SCM_SVC_INFO, GET_FEAT_VERSION_CMD);
	if (ret <= 0)
		return -EAGAIN;

	desc.args[0] = feat;
	desc.arginfo = SCM_ARGS(1);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_INFO, GET_FEAT_VERSION_CMD),
			&desc);

	*scm_ret = desc.ret[0];

	return ret;
}
EXPORT_SYMBOL(scm_get_feat_version);

#define RESTORE_SEC_CFG    2
int scm_restore_sec_cfg(u32 device_id, u32 spare, u64 *scm_ret)
{
	struct scm_desc desc = {0};
	int ret;
	struct restore_sec_cfg {
		u32 device_id;
		u32 spare;
	} cfg;

	cfg.device_id = device_id;
	cfg.spare = spare;

	if (IS_ERR_OR_NULL(scm_ret))
		return -EINVAL;

	if (!is_scm_armv8())
		return scm_call(SCM_SVC_MP, RESTORE_SEC_CFG, &cfg, sizeof(cfg),
				scm_ret, sizeof(*scm_ret));

	desc.args[0] = device_id;
	desc.args[1] = spare;
	desc.arginfo = SCM_ARGS(2);

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_MP, RESTORE_SEC_CFG), &desc);
	if (ret)
		return ret;

	*scm_ret = desc.ret[0];
	return 0;
}
EXPORT_SYMBOL(scm_restore_sec_cfg);

/*
 * SCM call command ID to check secure mode
 * Return zero for secure device.
 * Return one for non secure device or secure
 * device with debug enabled device.
 */
#define TZ_INFO_GET_SECURE_STATE	0x4
bool scm_is_secure_device(void)
{
	struct scm_desc desc = {0};
	int ret = 0, resp;

	desc.args[0] = 0;
	desc.arginfo = 0;
	if (!is_scm_armv8()) {
		ret = scm_call(SCM_SVC_INFO, TZ_INFO_GET_SECURE_STATE, NULL,
			0, &resp, sizeof(resp));
	} else {
		ret = scm_call2(SCM_SIP_FNID(SCM_SVC_INFO,
				TZ_INFO_GET_SECURE_STATE),
				&desc);
		resp = desc.ret[0];
	}

	if (ret) {
		pr_err("%s: SCM call failed\n", __func__);
		return false;
	}

	if ((resp & BIT(0)) || (resp & BIT(2)))
		return true;
	else
		return false;
}
EXPORT_SYMBOL(scm_is_secure_device);
