/* Copyright (c) 2010-2019, The Linux Foundation. All rights reserved.
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

#include <linux/habmm.h>

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

struct scm_extra_arg {
	union {
		u32 args32[N_EXT_SCM_ARGS];
		u64 args64[N_EXT_SCM_ARGS];
	};
};

struct smc_params_s {
	uint64_t fn_id;
	uint64_t arginfo;
	uint64_t args[MAX_SCM_ARGS];
} __packed;

static enum scm_interface_version {
	SCM_UNKNOWN,
	SCM_LEGACY,
	SCM_ARMV8_32,
	SCM_ARMV8_64,
} scm_version = SCM_UNKNOWN;

/* This will be set to specify SMC32 or SMC64 */
static u32 scm_version_mask;
static u32 handle;
static bool opened;

static int scm_qcpe_hab_open(void)
{
	int ret;

	if (!opened) {
		ret = habmm_socket_open(&handle, MM_QCPE_VM1, 0, 0);
		if (ret) {
			pr_err("habmm_socket_open failed with ret = %d\n", ret);
			return ret;
		}
		opened = true;
	}

	return 0;
}

static void scm_qcpe_hab_close(void)
{
	if (opened) {
		habmm_socket_close(handle);
		opened = false;
		handle = 0;
	}
}

/* Send SMC over HAB, receive the response. Both operations are blocking. */
/* This is meant to be called from non-atomic context. */
static int scm_qcpe_hab_send_receive(struct smc_params_s *smc_params,
	u32 *size_bytes)
{
	int ret;

	ret = habmm_socket_send(handle, smc_params, sizeof(*smc_params), 0);
	if (ret) {
		pr_err("habmm_socket_send failed, ret= 0x%x\n", ret);
		return ret;
	}

	memset(smc_params, 0x0, sizeof(*smc_params));

	do {
		*size_bytes = sizeof(*smc_params);
		ret = habmm_socket_recv(handle, smc_params, size_bytes, 0,
			HABMM_SOCKET_RECV_FLAGS_UNINTERRUPTIBLE);
	} while (-EINTR == ret);

	if (ret) {
		pr_err("habmm_socket_recv failed, ret= 0x%x\n", ret);
		return ret;
	}

	return 0;
}

/* Send SMC over HAB, receive the response, in non-blocking mode. */
/* This is meant to be called from atomic context. */
static int scm_qcpe_hab_send_receive_atomic(struct smc_params_s *smc_params,
	u32 *size_bytes)
{
	int ret;
	unsigned long delay;

	delay = jiffies + (HZ); /* 1 second delay for send */

	do {
		ret = habmm_socket_send(handle,
			smc_params, sizeof(*smc_params),
			HABMM_SOCKET_SEND_FLAGS_NON_BLOCKING);
	} while ((-EAGAIN == ret) && time_before(jiffies, delay));

	if (ret) {
		pr_err("HAB send failed, non-blocking, ret= 0x%x\n", ret);
		return ret;
	}

	memset(smc_params, 0x0, sizeof(*smc_params));

	delay = jiffies + (HZ); /* 1 second delay for receive */

	do {
		*size_bytes = sizeof(*smc_params);
		ret = habmm_socket_recv(handle, smc_params, size_bytes, 0,
			HABMM_SOCKET_RECV_FLAGS_NON_BLOCKING);
	} while ((-EAGAIN == ret) && time_before(jiffies, delay) &&
		(*size_bytes == 0));

	if (ret) {
		pr_err("HAB recv failed, non-blocking, ret= 0x%x\n", ret);
		return ret;
	}

	return 0;
}

static int scm_call_qcpe(u32 fn_id, struct scm_desc *desc, bool atomic)
{
	u32 size_bytes;
	struct smc_params_s smc_params = {0,};
	int ret;
#ifdef CONFIG_GHS_VMM
	int i;
	uint64_t arglen = desc->arginfo & 0xf;
	struct ion_handle *ihandle = NULL;
#endif

	pr_info("SCM IN [QCPE]: 0x%x, 0x%x, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
			fn_id, desc->arginfo, desc->args[0], desc->args[1],
			desc->args[2], desc->args[3], desc->x5);

	if (!opened) {
		if (!atomic) {
			if (scm_qcpe_hab_open()) {
				pr_err("HAB channel re-open failed\n");
				return -ENODEV;
			}
		} else {
			pr_err("HAB channel is not opened\n");
			return -ENODEV;
		}
	}

	smc_params.fn_id   = fn_id | scm_version_mask;
	smc_params.arginfo = desc->arginfo;
	smc_params.args[0] = desc->args[0];
	smc_params.args[1] = desc->args[1];
	smc_params.args[2] = desc->args[2];

#ifdef CONFIG_GHS_VMM
	if (arglen <= N_REGISTER_ARGS) {
		smc_params.args[FIRST_EXT_ARG_IDX] = desc->x5;
	} else {
		struct scm_extra_arg *argbuf =
				(struct scm_extra_arg *)desc->extra_arg_buf;
		int j = 0;

		if (scm_version == SCM_ARMV8_64)
			for (i = FIRST_EXT_ARG_IDX; i < MAX_SCM_ARGS; i++)
				smc_params.args[i] = argbuf->args64[j++];
		else
			for (i = FIRST_EXT_ARG_IDX; i < MAX_SCM_ARGS; i++)
				smc_params.args[i] = argbuf->args32[j++];
	}

	ret = ionize_buffers(fn_id & (~SMC64_MASK), &smc_params, &ihandle);
	if (ret)
		return ret;
#else
	smc_params.args[3] = desc->x5;
	smc_params.args[4] = 0;
#endif

	if (!atomic) {
		ret = scm_qcpe_hab_send_receive(&smc_params, &size_bytes);
		if (ret) {
			pr_err("send/receive failed, non-atomic, ret= 0x%x\n",
				ret);
			goto err_ret;
		}
	} else {
		ret = scm_qcpe_hab_send_receive_atomic(&smc_params,
			&size_bytes);
		if (ret) {
			pr_err("send/receive failed, ret= 0x%x\n", ret);
			goto err_ret;
		}
	}

	if (size_bytes != sizeof(smc_params)) {
		pr_err("habmm_socket_recv expected size: %lu, actual=%u\n",
				sizeof(smc_params),
				size_bytes);
		ret = SCM_ERROR;
		goto err_ret;
	}

	desc->ret[0] = smc_params.args[1];
	desc->ret[1] = smc_params.args[2];
	desc->ret[2] = smc_params.args[3];
	ret = smc_params.args[0];
	pr_info("SCM OUT [QCPE]: 0x%llx, 0x%llx, 0x%llx, 0x%llx",
		smc_params.args[0], desc->ret[0], desc->ret[1], desc->ret[2]);
	goto no_err;

err_ret:
	if (!atomic) {
		/* In case of an error, try to recover the hab connection
		 * for next time. This can only be done if called in
		 * non-atomic context.
		 */
		scm_qcpe_hab_close();
		if (scm_qcpe_hab_open())
			pr_err("scm_qcpe_hab_open failed\n");
		}

no_err:
#ifdef CONFIG_GHS_VMM
	if (ihandle)
		free_ion_buffers(ihandle);
#endif
	return ret;
}

bool is_scm_armv8(void)
{
	int ret;
	u64 ret1, x0;
	bool ret_scm_version;
	bool save_scm_version;

	struct scm_desc desc = {0};

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

	desc.arginfo = SCM_ARGS(1);
	desc.args[0] = x0;

	ret = scm_call_qcpe(x0 | SMC64_MASK, &desc, true);
	save_scm_version = (ret == -ENODEV) ? false : true;

	ret1 = desc.ret[0];

	if (ret || !ret1) {
		/* Try SMC32 call */
		ret1 = 0;

		desc.arginfo = SCM_ARGS(1);
		desc.args[0] = x0;

		ret = scm_call_qcpe(x0, &desc, true);
		save_scm_version = (ret == -ENODEV) ? false : true;

		if (ret || !ret1)
			scm_version = SCM_LEGACY;
		else
			scm_version = SCM_ARMV8_32;
	} else
		scm_version_mask = SMC64_MASK;

	pr_debug("scm_call: scm version is %x, mask is %x\n", scm_version,
			scm_version_mask);

	ret_scm_version = (scm_version == SCM_ARMV8_32) ||
			(scm_version == SCM_ARMV8_64);

	/* Don't cache the scm_version in case error is due to hab issues. */
	/* In this case, allow a later retry. */
	if (!save_scm_version)
		scm_version = SCM_UNKNOWN;

	return ret_scm_version;
}

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
	__cpuc_flush_dcache_area(argbuf, argbuflen);
	outer_flush_range(virt_to_phys(argbuf),
			  virt_to_phys(argbuf) + argbuflen);

	return 0;
}

static int __scm_call2(u32 fn_id, struct scm_desc *desc, bool retry)
{
	int arglen = desc->arginfo & 0xf;
	int ret;
	u64 x0;

	if (unlikely(!is_scm_armv8()))
		return -ENODEV;

	ret = allocate_extra_arg_buffer(desc, GFP_NOIO);
	if (ret)
		return ret;

	x0 = fn_id | scm_version_mask;

	mutex_lock(&scm_lock);

	if (SCM_SVC_ID(fn_id) == SCM_SVC_LMH)
		mutex_lock(&scm_lmh_lock);

	desc->ret[0] = desc->ret[1] = desc->ret[2] = 0;

	trace_scm_call_start(x0, desc);

	ret = scm_call_qcpe(x0, desc, false);

	trace_scm_call_end(desc);

	if (SCM_SVC_ID(fn_id) == SCM_SVC_LMH)
		mutex_unlock(&scm_lmh_lock);

	mutex_unlock(&scm_lock);

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

	pr_debug("scm_call: func id %#llx, args: %#x, %#llx, %#llx, %#llx, %#llx\n",
			x0, desc->arginfo, desc->args[0], desc->args[1],
			desc->args[2], desc->x5);

	ret = scm_call_qcpe(x0, desc, true);

	if (ret < 0)
		pr_err("scm_call failed: func id %#llx, arginfo: %#x, args: %#llx, %#llx, %#llx, %#llx, ret: %d, syscall returns: %#llx, %#llx, %#llx\n",
				x0, desc->arginfo, desc->args[0], desc->args[1],
				desc->args[2], desc->x5, ret, desc->ret[0],
				desc->ret[1], desc->ret[2]);

	if (arglen > N_REGISTER_ARGS)
		kfree(desc->extra_arg_buf);
	if (ret < 0)
		return scm_remap_error(ret);
	return ret;
}
EXPORT_SYMBOL(scm_call2_atomic);

u32 scm_get_version(void)
{
	int context_id;
	static u32 version = -1;
	int ret;
	uint64_t x0;
	struct scm_desc desc = {0};

	register u32 r0;
	register u32 r1;

	if (version != -1)
		return version;

	mutex_lock(&scm_lock);

	r0 = 0x1 << 8;
	r1 = (uintptr_t)&context_id;

	x0 = r0;
	desc.arginfo = r1;

	ret = scm_call_qcpe(x0, &desc, false);

	version = desc.ret[0];

	mutex_unlock(&scm_lock);

	if (ret < 0)
		return scm_remap_error(ret);

	return version;
}
EXPORT_SYMBOL(scm_get_version);

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

#define RESTORE_SEC_CFG    2
int scm_restore_sec_cfg(u32 device_id, u32 spare, int *scm_ret)
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
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_INFO,
			TZ_INFO_GET_SECURE_STATE),
			&desc);
	resp = desc.ret[0];

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

/*
 * SCM call command ID to protect kernel memory
 * in Hyp Stage 2 page tables.
 * Return zero for success.
 * Return non-zero for failure.
 */
#define TZ_RTIC_ENABLE_MEM_PROTECTION	0x4
#if IS_ENABLED(CONFIG_QCOM_QHEE_ENABLE_MEM_PROTECTION)
int scm_enable_mem_protection(void)
{
	struct scm_desc desc = {0};
	int ret = 0, resp;

	desc.args[0] = 0;
	desc.arginfo = 0;
	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_RTIC,
			TZ_RTIC_ENABLE_MEM_PROTECTION),
			&desc);
	resp = desc.ret[0];

	if (ret == -1) {
		pr_err("%s: SCM call not supported\n", __func__);
		return ret;
	} else if (ret || resp) {
		pr_err("%s: SCM call failed\n", __func__);
		if (ret)
			return ret;
		else
			return resp;
	}

	return resp;
}
#else
inline int scm_enable_mem_protection(void)
{
	return 0;
}
#endif

EXPORT_SYMBOL(scm_enable_mem_protection);

static int __init scm_qcpe_init(void)
{
	return scm_qcpe_hab_open();
}
/* Subsys sync is for init after HAB (subsys) and before kernel clients. */
subsys_initcall_sync(scm_qcpe_init);

static void __exit scm_qcpe_exit(void)
{
	scm_qcpe_hab_close();
}
module_exit(scm_qcpe_exit);

MODULE_DESCRIPTION("Support for SCM calls over HAB to QCPE module");
MODULE_LICENSE("GPL v2");
