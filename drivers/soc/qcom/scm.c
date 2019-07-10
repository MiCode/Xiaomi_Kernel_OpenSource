// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2010-2019, The Linux Foundation. All rights reserved.
 */

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>

#include <soc/qcom/scm.h>

#define CREATE_TRACE_POINTS
#include <trace/events/scm.h>

#ifdef CONFIG_ARM64
/*
 * This is used to ensure the compiler did actually allocate the register we
 * asked it for some inline assembly sequences.  Apparently we can't trust the
 * compiler from one version to another so a bit of paranoia won't hurt.  This
 * string is meant to be concatenated with the inline asm string and will
 * cause compilation to stop on mismatch.  (for details, see gcc PR 15089)
 */
#define __asmeq(x, y)  ".ifnc " x "," y " ; .err ; .endif\n\t"

#else

#include <asm/compiler.h>
#include <asm/cacheflush.h>

#endif

#define SCM_ENOMEM		-9
#define SCM_EINVAL_ADDR		-3
#define SCM_EINVAL_ARG		-2
#define SCM_ERROR		-1
#define SCM_INTERRUPTED		1
#define SCM_V2_EBUSY		-12

static DEFINE_MUTEX(scm_lock);

#define SCM_EBUSY_WAIT_MS 30
#define SCM_EBUSY_MAX_RETRY 67

#define N_EXT_SCM_ARGS 7
#define FIRST_EXT_ARG_IDX 3
#define SMC_ATOMIC_SYSCALL 31
#define N_REGISTER_ARGS (MAX_SCM_ARGS - N_EXT_SCM_ARGS + 1)
#define SMC64_MASK 0x40000000
#define SMC_ATOMIC_MASK 0x80000000
#define IS_CALL_AVAIL_CMD 1

#ifdef CONFIG_ARM64

#define R0_STR "x0"
#define R1_STR "x1"
#define R2_STR "x2"
#define R3_STR "x3"
#define R4_STR "x4"
#define R5_STR "x5"
#define R6_STR "x6"

/* Outer caches unsupported on ARM64 platforms */
#define outer_flush_range(x, y)

#else

#define R0_STR "r0"
#define R1_STR "r1"
#define R2_STR "r2"
#define R3_STR "r3"
#define R4_STR "r4"
#define R5_STR "r5"
#define R6_STR "r6"

#endif

static struct device *qcom_scm_dev;

struct scm_dma_buf {
	size_t size;
	dma_addr_t extra_arg_buf_phy;
};

static int scm_remap_error(int err)
{
	switch (err) {
	case SCM_ERROR:
		return -EOPNOTSUPP;
	case SCM_EINVAL_ADDR:
	case SCM_EINVAL_ARG:
		return -EINVAL;
	case SCM_ENOMEM:
		return -ENOMEM;
	case SCM_V2_EBUSY:
		return -EBUSY;
	}
	return -EINVAL;
}

#ifdef CONFIG_ARM64

static int __scm_call_armv8_64(u64 x0, u64 x1, u64 x2, u64 x3, u64 x4, u64 x5,
				u64 *ret1, u64 *ret2, u64 *ret3)
{
	register u64 r0 asm("x0") = x0;
	register u64 r1 asm("x1") = x1;
	register u64 r2 asm("x2") = x2;
	register u64 r3 asm("x3") = x3;
	register u64 r4 asm("x4") = x4;
	register u64 r5 asm("x5") = x5;
	register u64 r6 asm("x6") = 0;

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
	register u32 r0 asm("w0") = w0;
	register u32 r1 asm("w1") = w1;
	register u32 r2 asm("w2") = w2;
	register u32 r3 asm("w3") = w3;
	register u32 r4 asm("w4") = w4;
	register u32 r5 asm("w5") = w5;
	register u32 r6 asm("w6") = 0;

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

static noinline bool is_scm_armv8(void)
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
			return -ENODEV;

		scm_version = SCM_ARMV8_32;
	} else
		scm_version_mask = SMC64_MASK;

	pr_debug("scm_call: scm version is %x, mask is %x\n", scm_version,
		  scm_version_mask);

	return (scm_version == SCM_ARMV8_32) ||
			(scm_version == SCM_ARMV8_64);
}

/*
 * If there are more than N_REGISTER_ARGS, allocate a buffer and place
 * the additional arguments in it. The extra argument buffer will be
 * pointed to by X5.
 */
static int allocate_extra_arg_buffer(struct scm_desc *desc,
				struct scm_dma_buf *scm_dma_buf, gfp_t flags)
{
	int i, j;
	struct scm_extra_arg *argbuf;
	dma_addr_t argbuf_phy;
	int arglen = desc->arginfo & 0xf;
	size_t argbuflen = PAGE_ALIGN(sizeof(struct scm_extra_arg));

	desc->x5 = desc->args[FIRST_EXT_ARG_IDX];

	if (likely(arglen <= N_REGISTER_ARGS)) {
		desc->extra_arg_buf = NULL;
		return 0;
	}

	if (!qcom_scm_dev)
		return -EPROBE_DEFER;

	argbuf = kzalloc(argbuflen, flags);
	if (!argbuf)
		return -ENOMEM;

	desc->extra_arg_buf = argbuf;

	j = FIRST_EXT_ARG_IDX;
	if (scm_version == SCM_ARMV8_64)
		for (i = 0; i < N_EXT_SCM_ARGS; i++)
			argbuf->args64[i] = desc->args[j++];
	else
		for (i = 0; i < N_EXT_SCM_ARGS; i++)
			argbuf->args32[i] = desc->args[j++];
	desc->x5 = virt_to_phys(argbuf);

	argbuf_phy = dma_map_single(qcom_scm_dev, argbuf,
					argbuflen, DMA_TO_DEVICE);
	if (dma_mapping_error(qcom_scm_dev, argbuf_phy)) {
		kfree(argbuf);
		return -ENOMEM;
	}

	scm_dma_buf->extra_arg_buf_phy = argbuf_phy;
	scm_dma_buf->size = argbuflen;

	outer_flush_range(virt_to_phys(argbuf),
			  virt_to_phys(argbuf) + argbuflen);

	return 0;
}

static int __scm_call2(u32 fn_id, struct scm_desc *desc, bool retry)
{
	int arglen = desc->arginfo & 0xf;
	int ret, retry_count = 0;
	u64 x0;
	struct scm_dma_buf scm_dma_buf = {0};

	ret = allocate_extra_arg_buffer(desc, &scm_dma_buf, GFP_NOIO);
	if (ret)
		return ret;

	x0 = fn_id | scm_version_mask;

	trace_scm_call_start(x0, desc);
	do {
		mutex_lock(&scm_lock);

		desc->ret[0] = desc->ret[1] = desc->ret[2] = 0;

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

		mutex_unlock(&scm_lock);
		if (!retry)
			goto out;

		if (ret == SCM_V2_EBUSY)
			msleep(SCM_EBUSY_WAIT_MS);
		if (retry_count == 33)
			pr_warn("scm: secure world has been busy for 1 second!\n");
	} while (ret == SCM_V2_EBUSY && (retry_count++ < SCM_EBUSY_MAX_RETRY));
out:
	trace_scm_call_end(desc);
	if (ret < 0)
		pr_err("scm_call failed: func id %#llx, ret: %d, syscall returns: %#llx, %#llx, %#llx\n",
			x0, ret, desc->ret[0], desc->ret[1], desc->ret[2]);

	if (arglen > N_REGISTER_ARGS) {
		dma_unmap_single(qcom_scm_dev, scm_dma_buf.extra_arg_buf_phy,
					scm_dma_buf.size, DMA_TO_DEVICE);
		kfree(desc->extra_arg_buf);
	}
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
	struct scm_dma_buf scm_dma_buf = {0};

	ret = allocate_extra_arg_buffer(desc, &scm_dma_buf, GFP_ATOMIC);
	if (ret)
		return ret;

	x0 = fn_id | BIT(SMC_ATOMIC_SYSCALL) | scm_version_mask;

	trace_scm_call_start(x0, desc);
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
	trace_scm_call_end(desc);
	if (ret < 0)
		pr_err("scm_call failed: func id %#llx, ret: %d, syscall returns: %#llx, %#llx, %#llx\n",
			x0, ret, desc->ret[0],
			desc->ret[1], desc->ret[2]);

	if (arglen > N_REGISTER_ARGS) {
		dma_unmap_single(qcom_scm_dev, scm_dma_buf.extra_arg_buf_phy,
					scm_dma_buf.size, DMA_TO_DEVICE);
		kfree(desc->extra_arg_buf);
	}
	if (ret < 0)
		return scm_remap_error(ret);
	return ret;
}
EXPORT_SYMBOL(scm_call2_atomic);

#define SCM_IO_READ	0x1
#define SCM_IO_WRITE	0x2

u32 scm_io_read(phys_addr_t address)
{
	struct scm_desc desc = {
		.args[0] = address,
		.arginfo = SCM_ARGS(1),
	};

	scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_IO, SCM_IO_READ), &desc);
	return desc.ret[0];
}
EXPORT_SYMBOL(scm_io_read);

int scm_io_write(phys_addr_t address, u32 val)
{
	int ret;
	struct scm_desc desc = {
		.args[0] = address,
		.args[1] = val,
		.arginfo = SCM_ARGS(2),
	};

	ret = scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_IO, SCM_IO_WRITE),
					&desc);
	return ret;
}
EXPORT_SYMBOL(scm_io_write);

int scm_is_call_available(u32 svc_id, u32 cmd_id)
{
	int ret;
	struct scm_desc desc = {0};

	desc.arginfo = SCM_ARGS(1);
	desc.args[0] = SCM_SIP_FNID(svc_id, cmd_id);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_INFO, IS_CALL_AVAIL_CMD), &desc);
	if (ret)
		return ret;

	return desc.ret[0];
}
EXPORT_SYMBOL(scm_is_call_available);

#define GET_FEAT_VERSION_CMD	3
int scm_get_feat_version(u32 feat)
{
	struct scm_desc desc = {0};
	int ret;

	ret = scm_is_call_available(SCM_SVC_INFO, GET_FEAT_VERSION_CMD);
	if (ret <= 0)
		return 0;

	desc.args[0] = feat;
	desc.arginfo = SCM_ARGS(1);
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_INFO, GET_FEAT_VERSION_CMD),
			&desc);
	if (!ret)
		return desc.ret[0];

	return 0;
}
EXPORT_SYMBOL(scm_get_feat_version);

static int qcom_scm_probe(struct platform_device *pdev)
{
	int ret = 0;

	if (unlikely(!is_scm_armv8())) {
		dev_err(&pdev->dev, "SCM ARMv8 not supported\n");
		return -ENODEV;
	}

	qcom_scm_dev = &pdev->dev;

#ifdef CONFIG_ARM64
	ret = dma_set_mask(&pdev->dev, DMA_BIT_MASK(64));
#endif

	return ret;
}

static const struct of_device_id qcom_scm_of_match[] = {
	{ .compatible = "qcom,secure-chan-manager"},
	{}
};
MODULE_DEVICE_TABLE(of, qcom_scm_of_match);

static struct platform_driver qcom_scm_driver = {
	.probe = qcom_scm_probe,
	.driver = {
		.name = "qcom_secure_chan_manager",
		.of_match_table = qcom_scm_of_match,
	},
};

module_platform_driver(qcom_scm_driver);

MODULE_LICENSE("GPL v2");
