// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2015,2020-2021 The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/qcom_scm.h>
#include <linux/arm-smccc.h>
#include <linux/dma-mapping.h>

#include <asm/cacheflush.h>

#include <linux/qtee_shmbridge.h>
#include <soc/qcom/qseecom_scm.h>
#include <soc/qcom/qseecomi.h>

#include "qcom_scm.h"

#define CREATE_TRACE_POINTS
#include <trace/events/scm.h>

#include <linux/habmm.h>

#define MAX_QCOM_SCM_ARGS 10
#define MAX_QCOM_SCM_RETS 3

#define QCOM_SCM_ATOMIC		BIT(0)
#define QCOM_SCM_NORETRY	BIT(1)

enum qcom_scm_arg_types {
	QCOM_SCM_VAL,
	QCOM_SCM_RO,
	QCOM_SCM_RW,
	QCOM_SCM_BUFVAL,
};

#define QCOM_SCM_ARGS_IMPL(num, a, b, c, d, e, f, g, h, i, j, ...) (\
			   (((a) & 0x3) << 4) | \
			   (((b) & 0x3) << 6) | \
			   (((c) & 0x3) << 8) | \
			   (((d) & 0x3) << 10) | \
			   (((e) & 0x3) << 12) | \
			   (((f) & 0x3) << 14) | \
			   (((g) & 0x3) << 16) | \
			   (((h) & 0x3) << 18) | \
			   (((i) & 0x3) << 20) | \
			   (((j) & 0x3) << 22) | \
			   ((num) & 0xf))

#define QCOM_SCM_ARGS(...) QCOM_SCM_ARGS_IMPL(__VA_ARGS__, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)

/**
 * struct qcom_scm_desc
 * @arginfo:	Metadata describing the arguments in args[]
 * @args:	The array of arguments for the secure syscall
 * @res:	The values returned by the secure syscall
 */
struct qcom_scm_desc {
	u32 svc;
	u32 cmd;
	u32 arginfo;
	u64 args[MAX_QCOM_SCM_ARGS];
	u64 res[MAX_QCOM_SCM_RETS];
	u32 owner;
};

struct arm_smccc_args {
	unsigned long a[8];
};

enum qcom_smc_convention {
	SMC_CONVENTION_UNKNOWN,
	SMC_CONVENTION_LEGACY,
	SMC_CONVENTION_ARM_32,
	SMC_CONVENTION_ARM_64,
};

static enum qcom_smc_convention qcom_smc_convention = SMC_CONVENTION_UNKNOWN;
static DEFINE_MUTEX(qcom_scm_lock);
static bool has_queried;
static DEFINE_SPINLOCK(query_lock);

#define QCOM_SCM_EBUSY_WAIT_MS 30
#define QCOM_SCM_EBUSY_MAX_RETRY 20

#define SMCCC_FUNCNUM(s, c)	((((s) & 0xFF) << 8) | ((c) & 0xFF))
#define SMCCC_N_REG_ARGS	4
#define SMCCC_FIRST_EXT_IDX	(SMCCC_N_REG_ARGS - 1)
#define SMCCC_N_EXT_ARGS	(MAX_QCOM_SCM_ARGS - SMCCC_N_REG_ARGS + 1)
#define SMCCC_FIRST_REG_IDX	2
#define SMCCC_LAST_REG_IDX	(SMCCC_FIRST_REG_IDX + SMCCC_N_REG_ARGS - 1)

#define LEGACY_FUNCNUM(s, c)  (((s) << 10) | ((c) & 0x3ff))

/**
 * struct legacy_command - one SCM command buffer
 * @len: total available memory for command and response
 * @buf_offset: start of command buffer
 * @resp_hdr_offset: start of response buffer
 * @id: command to be executed
 * @buf: buffer returned from legacy_get_command_buffer()
 *
 * An SCM command is laid out in memory as follows:
 *
 *	------------------- <--- struct legacy_command
 *	| command header  |
 *	------------------- <--- legacy_get_command_buffer()
 *	| command buffer  |
 *	------------------- <--- struct legacy_response and
 *	| response header |      legacy_command_to_response()
 *	------------------- <--- legacy_get_response_buffer()
 *	| response buffer |
 *	-------------------
 *
 * There can be arbitrary padding between the headers and buffers so
 * you should always use the appropriate qcom_scm_get_*_buffer() routines
 * to access the buffers in a safe manner.
 */
struct legacy_command {
	__le32 len;
	__le32 buf_offset;
	__le32 resp_hdr_offset;
	__le32 id;
	__le32 buf[0];
};

/**
 * struct legacy_response - one SCM response buffer
 * @len: total available memory for response
 * @buf_offset: start of response data relative to start of legacy_response
 * @is_complete: indicates if the command has finished processing
 */
struct legacy_response {
	__le32 len;
	__le32 buf_offset;
	__le32 is_complete;
};

#define LEGACY_ATOMIC_N_REG_ARGS	5
#define LEGACY_ATOMIC_FIRST_REG_IDX	2
#define LEGACY_CLASS_REGISTER	(0x2 << 8)
#define LEGACY_MASK_IRQS		BIT(5)
#define LEGACY_ATOMIC(svc, cmd, n) ((LEGACY_FUNCNUM(svc, cmd) << 12) | \
				    LEGACY_CLASS_REGISTER | \
				    LEGACY_MASK_IRQS | \
				    (n & 0xf))

#define QCOM_SCM_FLAG_COLDBOOT_CPU0	0x00
#define QCOM_SCM_FLAG_COLDBOOT_CPU1	0x01
#define QCOM_SCM_FLAG_COLDBOOT_CPU2	0x08
#define QCOM_SCM_FLAG_COLDBOOT_CPU3	0x20

#define QCOM_SCM_FLAG_WARMBOOT_CPU0	0x04
#define QCOM_SCM_FLAG_WARMBOOT_CPU1	0x02
#define QCOM_SCM_FLAG_WARMBOOT_CPU2	0x10
#define QCOM_SCM_FLAG_WARMBOOT_CPU3	0x40

struct qcom_scm_entry {
	int flag;
	void *entry;
};

static struct qcom_scm_entry qcom_scm_wb[] = {
	{ .flag = QCOM_SCM_FLAG_WARMBOOT_CPU0 },
	{ .flag = QCOM_SCM_FLAG_WARMBOOT_CPU1 },
	{ .flag = QCOM_SCM_FLAG_WARMBOOT_CPU2 },
	{ .flag = QCOM_SCM_FLAG_WARMBOOT_CPU3 },
};


#if IS_ENABLED(CONFIG_QCOM_SCM_QCPE)

#ifdef CONFIG_GHS_VMM
struct scm_extra_arg {
	union {
		u32 args32[N_EXT_SCM_ARGS];
		u64 args64[N_EXT_SCM_ARGS];
	};
};
#endif

struct smc_params_s {
	uint64_t fn_id;
	uint64_t arginfo;
	uint64_t args[MAX_SCM_ARGS];
} __packed;

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


static int scm_call_qcpe(const struct arm_smccc_args *smc,
			 struct arm_smccc_res *res, const bool atomic)
{
	u32 size_bytes;
	struct smc_params_s smc_params = {0,};
	int ret;
#ifdef CONFIG_GHS_VMM
	int i;
	uint64_t arglen = smc->a[1] & 0xf;
	struct ion_handle *ihandle = NULL;
#endif

	pr_info("SCM IN [QCPE]: 0x%x, 0x%x, 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
		smc->a[0], smc->a[1], smc->a[2], smc->a[3], smc->a[4], smc->a[5],
		smc->a[5]);

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

	smc_params.fn_id   = smc->a[0];
	smc_params.arginfo = smc->a[1];
	smc_params.args[0] = smc->a[2];
	smc_params.args[1] = smc->a[3];
	smc_params.args[2] = smc->a[4];

#ifdef CONFIG_GHS_VMM
	if (arglen <= N_REGISTER_ARGS) {
		smc_params.args[FIRST_EXT_ARG_IDX] = smc->a[5];
	} else {
		struct scm_extra_arg *argbuf =
				(struct scm_extra_arg *)desc->extra_arg_buf;
		int j = 0;

		if (scm_version == SMC_CONVENTION_ARM_64)
			for (i = FIRST_EXT_ARG_IDX; i < MAX_QCOM_SCM_ARGS; i++)
				smc_params.args[i] = argbuf->args64[j++];
		else
			for (i = FIRST_EXT_ARG_IDX; i < MAX_QCOM_SCM_ARGS; i++)
				smc_params.args[i] = argbuf->args32[j++];
	}

	ret = ionize_buffers(smc->a[0] & (~SMC64_MASK), &smc_params, &ihandle);
	if (ret)
		return ret;
#else
	smc_params.args[3] = smc->a[5];
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
		ret = QCOM_SCM_ERROR;
		goto err_ret;
	}

	res->a1 = smc_params.args[1];
	res->a2 = smc_params.args[2];
	res->a3 = smc_params.args[3];
	res->a0 = smc_params.args[0];
	pr_info("SCM OUT [QCPE]: 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
		res->a0, res->a1, res->a2, res->a3);
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
	return res->a0;
}

#endif /* CONFIG_QCOM_SCM_QCPE */

static void __qcom_scm_call_do_quirk(const struct arm_smccc_args *smc,
				     struct arm_smccc_res *res,
				     const bool atomic)
{
	ktime_t time;
	const bool trace = trace_scm_call_enabled();
#if !(IS_ENABLED(CONFIG_QCOM_SCM_QCPE))
	unsigned long a0 = smc->a[0];
	struct arm_smccc_quirk quirk = { .id = ARM_SMCCC_QUIRK_QCOM_A6 };

	quirk.state.a6 = 0;
#endif
	if (trace)
		time = ktime_get();

#if IS_ENABLED(CONFIG_QCOM_SCM_QCPE)
	scm_call_qcpe(smc, res, atomic);
#else
	do {
		arm_smccc_smc_quirk(a0, smc->a[1], smc->a[2], smc->a[3],
				    smc->a[4], smc->a[5], quirk.state.a6,
				    smc->a[7], res, &quirk);

		if (res->a0 == QCOM_SCM_INTERRUPTED)
			a0 = res->a0;

	} while (res->a0 == QCOM_SCM_INTERRUPTED);
#endif
	if (trace)
		trace_scm_call(smc->a, res, ktime_us_delta(ktime_get(), time));
}

static int qcom_scm_call_smccc(struct device *dev,
				  struct qcom_scm_desc *desc, const u32 options)
{
	int arglen = desc->arginfo & 0xf;
	int i, ret;
	size_t alloc_len;
	const bool atomic = options & QCOM_SCM_ATOMIC;
	gfp_t flag = atomic ? GFP_ATOMIC : GFP_NOIO;
	u32 smccc_call_type = atomic ? ARM_SMCCC_FAST_CALL : ARM_SMCCC_STD_CALL;
	u32 qcom_smccc_convention =
		(qcom_smc_convention == SMC_CONVENTION_ARM_32) ?
		ARM_SMCCC_SMC_32 : ARM_SMCCC_SMC_64;
	struct arm_smccc_res res;
	struct arm_smccc_args smc = {{0}};
	struct qtee_shm shm = {0};
	bool use_qtee_shmbridge;

	smc.a[0] = ARM_SMCCC_CALL_VAL(
		smccc_call_type,
		qcom_smccc_convention,
		desc->owner,
		SMCCC_FUNCNUM(desc->svc, desc->cmd));
	smc.a[1] = desc->arginfo;
	for (i = 0; i < SMCCC_N_REG_ARGS; i++)
		smc.a[i + SMCCC_FIRST_REG_IDX] = desc->args[i];

	if (unlikely(arglen > SMCCC_N_REG_ARGS)) {
		if (!dev)
			return -EPROBE_DEFER;

		alloc_len = SMCCC_N_EXT_ARGS * sizeof(u64);
		use_qtee_shmbridge = qtee_shmbridge_is_enabled();
		if (use_qtee_shmbridge) {
			ret = qtee_shmbridge_allocate_shm(alloc_len, &shm);
			if (ret)
				return ret;
		} else {
			shm.vaddr = kzalloc(alloc_len, flag);
			if (!shm.vaddr)
				return -ENOMEM;
		}

		if (qcom_smc_convention == SMC_CONVENTION_ARM_32) {
			__le32 *args = shm.vaddr;

			for (i = 0; i < SMCCC_N_EXT_ARGS; i++)
				args[i] = cpu_to_le32(desc->args[i +
						      SMCCC_FIRST_EXT_IDX]);
		} else {
			__le64 *args = shm.vaddr;

			for (i = 0; i < SMCCC_N_EXT_ARGS; i++)
				args[i] = cpu_to_le64(desc->args[i +
						      SMCCC_FIRST_EXT_IDX]);
		}

		shm.paddr = dma_map_single(dev, shm.vaddr, alloc_len,
						DMA_TO_DEVICE);

		if (dma_mapping_error(dev, shm.paddr)) {
			if (use_qtee_shmbridge)
				qtee_shmbridge_free_shm(&shm);
			else
				kfree(shm.vaddr);
			return -ENOMEM;
		}

		smc.a[SMCCC_LAST_REG_IDX] = shm.paddr;
	}

	if (atomic) {
		__qcom_scm_call_do_quirk(&smc, &res, true);
	} else {
		int retry_count = 0;

		do {
			mutex_lock(&qcom_scm_lock);
			__qcom_scm_call_do_quirk(&smc, &res, false);
			mutex_unlock(&qcom_scm_lock);

			if (res.a0 == QCOM_SCM_V2_EBUSY) {
				if (retry_count++ > QCOM_SCM_EBUSY_MAX_RETRY ||
				    (options & QCOM_SCM_NORETRY))
					break;
				msleep(QCOM_SCM_EBUSY_WAIT_MS);
			}
		} while (res.a0 == QCOM_SCM_V2_EBUSY);
	}

	if (unlikely(arglen > SMCCC_N_REG_ARGS)) {
		dma_unmap_single(dev, shm.paddr, alloc_len,
					DMA_TO_DEVICE);
		if (use_qtee_shmbridge)
			qtee_shmbridge_free_shm(&shm);
		else
			kfree(shm.vaddr);
	}

	desc->res[0] = res.a1;
	desc->res[1] = res.a2;
	desc->res[2] = res.a3;

	return res.a0 ? qcom_scm_remap_error(res.a0) : 0;
}

/**
 * legacy_command_to_response() - Get a pointer to a legacy_response
 * @cmd: command
 *
 * Returns a pointer to a response for a command.
 */
static inline struct legacy_response *legacy_command_to_response(
		const struct legacy_command *cmd)
{
	return (void *)cmd + le32_to_cpu(cmd->resp_hdr_offset);
}

/**
 * legacy_get_command_buffer() - Get a pointer to a command buffer
 * @cmd: command
 *
 * Returns a pointer to the command buffer of a command.
 */
static inline void *legacy_get_command_buffer(const struct legacy_command *cmd)
{
	return (void *)cmd->buf;
}

/**
 * legacy_get_response_buffer() - Get a pointer to a response buffer
 * @rsp: response
 *
 * Returns a pointer to a response buffer of a response.
 */
static inline void *legacy_get_response_buffer(
		const struct legacy_response *rsp)
{
	return (void *)rsp + le32_to_cpu(rsp->buf_offset);
}

static void __qcom_scm_call_do(const struct arm_smccc_args *smc,
			      struct arm_smccc_res *res)
{
	ktime_t time;
	const bool trace = trace_scm_call_enabled();

	if (trace)
		time = ktime_get();

	do {
		arm_smccc_smc(smc->a[0], smc->a[1], smc->a[2], smc->a[3],
			      smc->a[4], smc->a[5], smc->a[6], smc->a[7], res);
	} while (res->a0 == QCOM_SCM_INTERRUPTED);

	if (trace)
		trace_scm_call(smc->a, res, ktime_us_delta(ktime_get(), time));
}

/**
 * qcom_scm_call_legacy() - Send an SCM command
 * @dev: struct device
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
 * must be flushed before invoking qcom_scm_call and invalidated in the cache
 * immediately after qcom_scm_call returns. Cache maintenance on the command
 * and response buffers is taken care of by qcom_scm_call; however, callers are
 * responsible for any other cached buffers passed over to the secure world.
 */
static int qcom_scm_call_legacy(struct device *dev, struct qcom_scm_desc *desc)
{
	int arglen = desc->arginfo & 0xf;
	int ret = 0, context_id;
	size_t i;
	struct legacy_command *cmd;
	struct legacy_response *rsp;
	struct arm_smccc_args smc = {{0}};
	struct arm_smccc_res res;
	const size_t cmd_len = arglen * sizeof(__le32);
	const size_t resp_len = MAX_QCOM_SCM_RETS * sizeof(__le32);
	size_t alloc_len = sizeof(*cmd) + cmd_len + sizeof(*rsp) + resp_len;
	dma_addr_t cmd_phys;
	__le32 *arg_buf;
	__le32 *res_buf;

	if (!dev)
		return -EPROBE_DEFER;

	cmd = kzalloc(PAGE_ALIGN(alloc_len), GFP_KERNEL);
	if (!cmd)
		return -ENOMEM;

	cmd->len = cpu_to_le32(alloc_len);
	cmd->buf_offset = cpu_to_le32(sizeof(*cmd));
	cmd->resp_hdr_offset = cpu_to_le32(sizeof(*cmd) + cmd_len);
	cmd->id = cpu_to_le32(LEGACY_FUNCNUM(desc->svc, desc->cmd));

	arg_buf = legacy_get_command_buffer(cmd);
	for (i = 0; i < arglen; i++)
		arg_buf[i] = cpu_to_le32(desc->args[i]);

	rsp = legacy_command_to_response(cmd);

	cmd_phys = dma_map_single(dev, cmd, alloc_len, DMA_TO_DEVICE);
	if (dma_mapping_error(dev, cmd_phys)) {
		kfree(cmd);
		return -ENOMEM;
	}

	smc.a[0] = 1;
	smc.a[1] = (unsigned long)&context_id;
	smc.a[2] = cmd_phys;

	mutex_lock(&qcom_scm_lock);
	__qcom_scm_call_do(&smc, &res);
	if (res.a0 < 0)
		ret = qcom_scm_remap_error(res.a0);
	mutex_unlock(&qcom_scm_lock);
	if (ret)
		goto out;

	do {
		dma_sync_single_for_cpu(dev, cmd_phys + sizeof(*cmd) + cmd_len,
					sizeof(*rsp), DMA_FROM_DEVICE);
	} while (!rsp->is_complete);

	dma_sync_single_for_cpu(dev, cmd_phys + sizeof(*cmd) + cmd_len +
				le32_to_cpu(rsp->buf_offset),
				resp_len, DMA_FROM_DEVICE);

	res_buf = legacy_get_response_buffer(rsp);
	for (i = 0; i < MAX_QCOM_SCM_RETS; i++)
		desc->res[i] = le32_to_cpu(res_buf[i]);
out:
	dma_unmap_single(dev, cmd_phys, alloc_len, DMA_TO_DEVICE);
	kfree(cmd);
	return ret;
}

/**
 * qcom_scm_call_atomic_legacy() - Send an atomic SCM command with up to
 * 5 arguments and 3 return values
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
static int qcom_scm_call_atomic_legacy(struct device *dev,
				       struct qcom_scm_desc *desc)
{
	int context_id;
	struct arm_smccc_args smc = {{0}};
	struct arm_smccc_res res;
	size_t i, arglen = desc->arginfo & 0xf;
	const bool trace = trace_scm_call_enabled();
	ktime_t time;

	BUG_ON(arglen > LEGACY_ATOMIC_N_REG_ARGS);

	smc.a[0] = LEGACY_ATOMIC(desc->svc, desc->cmd, arglen);
	smc.a[1] = (unsigned long)&context_id;

	for (i = 0; i < arglen; i++)
		smc.a[i + LEGACY_ATOMIC_FIRST_REG_IDX] = desc->args[i];

	if (trace)
		time = ktime_get();
	arm_smccc_smc(smc.a[0], smc.a[1], smc.a[2], smc.a[3],
		      smc.a[4], smc.a[5], smc.a[6], smc.a[7], &res);

	if (trace)
		trace_scm_call(smc.a, &res, ktime_us_delta(ktime_get(), time));

	desc->res[0] = res.a1;
	desc->res[1] = res.a2;
	desc->res[2] = res.a3;

	return res.a0;
}

static void __query_convention(void)
{
	unsigned long flags;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_IS_CALL_AVAIL,
		.args[0] = SMCCC_FUNCNUM(QCOM_SCM_SVC_INFO,
					 QCOM_SCM_INFO_IS_CALL_AVAIL) |
			   (ARM_SMCCC_OWNER_SIP << ARM_SMCCC_OWNER_SHIFT),
		.arginfo = QCOM_SCM_ARGS(1),
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	spin_lock_irqsave(&query_lock, flags);
	if (has_queried)
		goto out;

	qcom_smc_convention = SMC_CONVENTION_ARM_64;
	ret = qcom_scm_call_smccc(NULL, &desc, true);
	if (!ret && desc.res[0] == 1)
		goto out;

	qcom_smc_convention = SMC_CONVENTION_ARM_32;
	ret = qcom_scm_call_smccc(NULL, &desc, true);
	if (!ret && desc.res[0] == 1)
		goto out;

	qcom_smc_convention = SMC_CONVENTION_LEGACY;
out:
	has_queried = true;
	spin_unlock_irqrestore(&query_lock, flags);
	pr_debug("QCOM SCM SMC Convention: %d\n", qcom_smc_convention);
}

static inline enum qcom_smc_convention __get_convention(void)
{
	if (unlikely(!has_queried))
		__query_convention();
	return qcom_smc_convention;
}

/**
 * qcom_scm_call() - Invoke a syscall in the secure world
 * @dev:	device
 * @svc_id:	service identifier
 * @cmd_id:	command identifier
 * @desc:	Descriptor structure containing arguments and return values
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This should *only* be called in pre-emptible context.
 */
static int qcom_scm_call(struct device *dev, struct qcom_scm_desc *desc)
{
	might_sleep();
	switch (__get_convention()) {
	case SMC_CONVENTION_ARM_32:
	case SMC_CONVENTION_ARM_64:
		return qcom_scm_call_smccc(dev, desc, false);
	case SMC_CONVENTION_LEGACY:
		return qcom_scm_call_legacy(dev, desc);
	default:
		pr_err("Unknown current SCM calling convention.\n");
		return -EINVAL;
	}
}

/**
 * qcom_scm_call_atomic() - atomic variation of qcom_scm_call()
 * @dev:	device
 * @svc_id:	service identifier
 * @cmd_id:	command identifier
 * @desc:	Descriptor structure containing arguments and return values
 * @res:	Structure containing results from SMC/HVC call
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This can be called in atomic context.
 */
static int qcom_scm_call_atomic(struct device *dev, struct qcom_scm_desc *desc)
{
	switch (__get_convention()) {
	case SMC_CONVENTION_ARM_32:
	case SMC_CONVENTION_ARM_64:
		return qcom_scm_call_smccc(dev, desc, true);
	case SMC_CONVENTION_LEGACY:
		return qcom_scm_call_atomic_legacy(dev, desc);
	default:
		pr_err("Unknown current SCM calling convention.\n");
		return -EINVAL;
	}
}

/**
 * qcom_scm_call_noretry() - Invoke a syscall in the secure world
 * @dev:	device
 * @svc_id:	service identifier
 * @cmd_id:	command identifier
 * @desc:	Descriptor structure containing arguments and return values
 *
 * Sends a command to the SCM and waits for the command to finish processing.
 * This should *only* be called in pre-emptible context.
 */
static int qcom_scm_call_noretry(struct device *dev, struct qcom_scm_desc *desc)
{
	might_sleep();
	switch (__get_convention()) {
	case SMC_CONVENTION_ARM_32:
	case SMC_CONVENTION_ARM_64:
		return qcom_scm_call_smccc(dev, desc, QCOM_SCM_NORETRY);
	case SMC_CONVENTION_LEGACY:
		return qcom_scm_call_legacy(dev, desc);
	default:
		pr_err("Unknown current SCM calling convention.\n");
		return -EINVAL;
	}
}

/**
 * qcom_scm_set_cold_boot_addr() - Set the cold boot address for cpus
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the cold boot address of the cpus. Any cpu outside the supported
 * range would be removed from the cpu present mask.
 */
int __qcom_scm_set_cold_boot_addr(struct device *dev, void *entry,
				  const cpumask_t *cpus)
{
	int flags = 0;
	int cpu;
	int scm_cb_flags[] = {
		QCOM_SCM_FLAG_COLDBOOT_CPU0,
		QCOM_SCM_FLAG_COLDBOOT_CPU1,
		QCOM_SCM_FLAG_COLDBOOT_CPU2,
		QCOM_SCM_FLAG_COLDBOOT_CPU3,
	};
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_ADDR,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	if (!cpus || (cpus && cpumask_empty(cpus)))
		return -EINVAL;

	for_each_cpu(cpu, cpus) {
		if (cpu < ARRAY_SIZE(scm_cb_flags))
			flags |= scm_cb_flags[cpu];
		else
			set_cpu_present(cpu, false);
	}

	desc.args[0] = flags;
	desc.args[1] = virt_to_phys(entry);
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call_atomic(dev, &desc);
}

/**
 * scm_set_boot_addr_mc - Set entry physical address for cpus
 * @addr: 32bit physical address
 * @aff0: Collective bitmask of the affinity-level-0 of the mpidr
 *	  1<<aff0_CPU0| 1<<aff0_CPU1....... | 1<<aff0_CPU32
 *	  Supports maximum 32 cpus under any affinity level.
 * @aff1:  Collective bitmask of the affinity-level-1 of the mpidr
 * @aff2:  Collective bitmask of the affinity-level-2 of the mpidr
 * @flags: Flag to differentiate between coldboot vs warmboot
 */
int __qcom_scm_set_warm_boot_addr_mc(struct device *dev, void *entry, u32 aff0,
				     u32 aff1, u32 aff2, u32 flags)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_ADDR_MC,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = virt_to_phys(entry);
	desc.args[1] = aff0;
	desc.args[2] = aff1;
	desc.args[3] = aff2;
	desc.args[4] = ~0ULL;
	desc.args[5] = flags;
	desc.arginfo = QCOM_SCM_ARGS(6);
	ret = qcom_scm_call(dev, &desc);

	return ret;
}

/**
 * qcom_scm_set_warm_boot_addr() - Set the warm boot address for cpus
 * @dev: Device pointer
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the Linux entry point for the SCM to transfer control to when coming
 * out of a power down. CPU power down may be executed on cpuidle or hotplug.
 */
int __qcom_scm_set_warm_boot_addr(struct device *dev, void *entry,
				  const cpumask_t *cpus)
{
	int ret;
	int flags = 0;
	int cpu;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_ADDR,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	/*
	 * Reassign only if we are switching from hotplug entry point
	 * to cpuidle entry point or vice versa.
	 */
	for_each_cpu(cpu, cpus) {
		if (entry == qcom_scm_wb[cpu].entry)
			continue;
		flags |= qcom_scm_wb[cpu].flag;
	}

	/* No change in entry function */
	if (!flags)
		return 0;

	desc.args[0] = virt_to_phys(entry);
	desc.args[1] = flags;
	desc.arginfo = QCOM_SCM_ARGS(2);
	ret = qcom_scm_call(dev, &desc);
	if (!ret) {
		for_each_cpu(cpu, cpus)
			qcom_scm_wb[cpu].entry = entry;
	}

	return ret;
}

void __qcom_scm_cpu_hp(struct device *dev, u32 flags)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_TERMINATE_PC,
		.args[0] = flags,
		.arginfo = QCOM_SCM_ARGS(1),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	qcom_scm_call_atomic(dev, &desc);
}

/**
 * qcom_scm_cpu_power_down() - Power down the cpu
 * @flags - Flags to flush cache
 *
 * This is an end point to power down cpu. If there was a pending interrupt,
 * the control would return from this function, otherwise, the cpu jumps to the
 * warm boot entry point set for this cpu upon reset.
 */
void __qcom_scm_cpu_power_down(struct device *dev, u32 flags)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_TERMINATE_PC,
		.args[0] = flags & QCOM_SCM_FLUSH_FLAG_MASK,
		.arginfo = QCOM_SCM_ARGS(1),
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	qcom_scm_call_atomic(dev, &desc);
}

int __qcom_scm_sec_wdog_deactivate(struct device *dev)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SEC_WDOG_DIS,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = 1;
	desc.arginfo = QCOM_SCM_ARGS(1);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_sec_wdog_trigger(struct device *dev)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SEC_WDOG_TRIGGER,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = 0;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

#ifdef CONFIG_TLB_CONF_HANDLER
int __qcom_scm_tlb_conf_handler(struct device *dev, unsigned long addr)
{
	int ret;

#define SCM_TLB_CONFLICT_CMD	0x1F
	struct qcom_scm_desc desc = {
	.svc = QCOM_SCM_SVC_MP,
	.cmd = SCM_TLB_CONFLICT_CMD,
	.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = addr;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call_atomic(dev, &desc);

	return ret ? : desc.res[0];
}
#endif

void __qcom_scm_disable_sdi(struct device *dev)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_WDOG_DEBUG_PART,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = 1;
	desc.args[1] = 0;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call_atomic(dev, &desc);
	if (ret)
		pr_err("Failed to disable secure wdog debug: %d\n", ret);
}

int __qcom_scm_set_remote_state(struct device *dev, u32 state, u32 id)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_REMOTE_STATE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = state;
	desc.args[1] = id;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_spin_cpu(struct device *dev)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SPIN_CPU,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = 0;
	desc.arginfo = QCOM_SCM_ARGS(1);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_set_dload_mode(struct device *dev, enum qcom_download_mode mode)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_SET_DLOAD_MODE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = mode;
	desc.args[1] = 0;
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call_atomic(dev, &desc);
}

int __qcom_scm_config_cpu_errata(struct device *dev)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_BOOT_CONFIG_CPU_ERRATA,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.arginfo = 0xffffffff;

	return qcom_scm_call(dev, &desc);
}

void __qcom_scm_phy_update_scm_level_shifter(struct device *dev, u32 val)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_BOOT,
		.cmd = QCOM_SCM_QUSB2PHY_LVL_SHIFTER_CMD_ID,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = val;
	desc.args[1] = 0;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, &desc);
	if (ret)
		pr_err("Failed to update scm level shifter=0x%x\n", ret);
}

bool __qcom_scm_pas_supported(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_IS_SUPPORTED,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	return ret ? false : !!desc.res[0];
}

int __qcom_scm_pas_init_image(struct device *dev, u32 peripheral,
			      dma_addr_t metadata_phys)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_INIT_IMAGE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.args[1] = metadata_phys;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_RW);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_pas_mem_setup(struct device *dev, u32 peripheral,
			      phys_addr_t addr, phys_addr_t size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_MEM_SETUP,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.args[1] = addr;
	desc.args[2] = size;
	desc.arginfo = QCOM_SCM_ARGS(3);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_pas_auth_and_reset(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_AUTH_AND_RESET,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_pas_shutdown(struct device *dev, u32 peripheral)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_SHUTDOWN,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = peripheral;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_pas_mss_reset(struct device *dev, bool reset)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_MSS_RESET,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = reset;
	desc.args[1] = 0;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_get_sec_dump_state(struct device *dev, u32 *dump_state)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_UTIL,
		.cmd = QCOM_SCM_UTIL_GET_SEC_DUMP_STATE,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	ret = qcom_scm_call(dev, &desc);

	if (dump_state)
		*dump_state = desc.res[0];

	return ret;
}

int __qcom_scm_tz_blsp_modify_owner(struct device *dev, int food, u64 subsystem,
				    int *out)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_TZ,
		.cmd = QOCM_SCM_TZ_BLSP_MODIFY_OWNER,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = subsystem;
	desc.args[1] = food;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, &desc);

	if (out)
		*out = desc.res[0];

	return ret;
}

int __qcom_scm_io_readl(struct device *dev, phys_addr_t addr,
			unsigned int *val)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_IO,
		.cmd = QCOM_SCM_IO_READ,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = addr;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call_atomic(dev, &desc);
	if (ret >= 0)
		*val = desc.res[0];

	return ret < 0 ? ret : 0;
}

int __qcom_scm_io_writel(struct device *dev, phys_addr_t addr, unsigned int val)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_IO,
		.cmd = QCOM_SCM_IO_WRITE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = addr;
	desc.args[1] = val;
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call_atomic(dev, &desc);
}

int __qcom_scm_io_reset(struct device *dev)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_IO,
		.cmd = QCOM_SCM_IO_RESET,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call_atomic(dev, &desc);
}

int __qcom_scm_is_call_available(struct device *dev, u32 svc_id, u32 cmd_id)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_IS_CALL_AVAIL,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.arginfo = QCOM_SCM_ARGS(1);
	switch (__get_convention()) {
	case SMC_CONVENTION_ARM_32:
	case SMC_CONVENTION_ARM_64:
		desc.args[0] = SMCCC_FUNCNUM(svc_id, cmd_id) |
				(ARM_SMCCC_OWNER_SIP << ARM_SMCCC_OWNER_SHIFT);
		break;
	case SMC_CONVENTION_LEGACY:
		desc.args[0] = LEGACY_FUNCNUM(svc_id, cmd_id);
		break;
	default:
		pr_err("Unknown SMC convention being used\n");
		return -EINVAL;
	}

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_get_feat_version(struct device *dev, u64 feat_id, u64 *version)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_GET_FEAT_VERSION_CMD,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = feat_id;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	if (version)
		*version = desc.res[0];

	return ret;
}

void __qcom_scm_halt_spmi_pmic_arbiter(struct device *dev)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PWR,
		.cmd = QCOM_SCM_PWR_IO_DISABLE_PMIC_ARBITER,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = 0;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call_atomic(dev, &desc);
	if (ret)
		pr_err("Failed to halt_spmi_pmic_arbiter=0x%x\n", ret);
}

void __qcom_scm_deassert_ps_hold(struct device *dev)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PWR,
		.cmd = QCOM_SCM_PWR_IO_DEASSERT_PS_HOLD,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = 0;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call_atomic(dev, &desc);
	if (ret)
		pr_err("Failed to deassert_ps_hold=0x%x\n", ret);
}

void __qcom_scm_mmu_sync(struct device *dev, bool sync)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PWR,
		.cmd = QCOM_SCM_PWR_MMU_SYNC,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = sync;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call_atomic(dev, &desc);

	if (ret)
		pr_err("MMU sync with Hypervisor off %x\n", ret);
}

int __qcom_scm_restore_sec_cfg(struct device *dev, u32 device_id, u32 spare)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_RESTORE_SEC_CFG,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = device_id;
	desc.args[1] = spare;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_iommu_secure_ptbl_size(struct device *dev, u32 spare,
				      size_t *size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_IOMMU_SECURE_PTBL_SIZE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = spare;
	desc.arginfo = QCOM_SCM_ARGS(1);

	ret = qcom_scm_call(dev, &desc);

	if (size)
		*size = desc.res[0];

	return ret ? : desc.res[1];
}

int __qcom_scm_iommu_secure_ptbl_init(struct device *dev, u64 addr, u32 size,
				      u32 spare)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_IOMMU_SECURE_PTBL_INIT,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = addr;
	desc.args[1] = size;
	desc.args[2] = spare;
	desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_RW, QCOM_SCM_VAL,
				     QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	/* the pg table has been initialized already, ignore the error */
	if (ret == -EPERM)
		ret = 0;

	return ret;
}

int __qcom_scm_mem_protect_video(struct device *dev,
				u32 cp_start, u32 cp_size,
				u32 cp_nonpixel_start, u32 cp_nonpixel_size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_MEM_PROTECT_VIDEO,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = cp_start;
	desc.args[1] = cp_size;
	desc.args[2] = cp_nonpixel_start;
	desc.args[3] = cp_nonpixel_size;
	desc.arginfo = QCOM_SCM_ARGS(4);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_mem_protect_region_id(struct device *dev, phys_addr_t paddr,
					size_t size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_MEM_PROTECT_REGION_ID,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = paddr;
	desc.args[1] = size;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RO, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	return ret;
}

int __qcom_scm_mem_protect_lock_id2_flat(struct device *dev,
				phys_addr_t list_addr, size_t list_size,
				size_t chunk_size, size_t memory_usage,
				int lock)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_MEM_PROTECT_LOCK_ID2_FLAT,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = list_addr;
	desc.args[1] = list_size;
	desc.args[2] = chunk_size;
	desc.args[3] = memory_usage;
	desc.args[4] = lock;
	desc.args[5] = 0;

	desc.arginfo = QCOM_SCM_ARGS(6, QCOM_SCM_RW, QCOM_SCM_VAL, QCOM_SCM_VAL,
				QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	return ret;
}

int __qcom_scm_iommu_secure_map(struct device *dev, phys_addr_t sg_list_addr,
			size_t num_sg, size_t sg_block_size, u64 sec_id,
			int cbndx, unsigned long iova, size_t total_len)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_IOMMU_SECURE_MAP2_FLAT,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = sg_list_addr;
	desc.args[1] = num_sg;
	desc.args[2] = sg_block_size;
	desc.args[3] = sec_id;
	desc.args[4] = cbndx;
	desc.args[5] = iova;
	desc.args[6] = total_len;
	desc.args[7] = 0;


	desc.arginfo = QCOM_SCM_ARGS(8, QCOM_SCM_RW, QCOM_SCM_VAL, QCOM_SCM_VAL,
			QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL,
			QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_iommu_secure_unmap(struct device *dev, u64 sec_id, int cbndx,
			unsigned long iova, size_t total_len)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_IOMMU_SECURE_UNMAP2_FLAT,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = sec_id;
	desc.args[1] = cbndx;
	desc.args[2] = iova;
	desc.args[3] = total_len;
	desc.args[4] = QCOM_SCM_IOMMU_TLBINVAL_FLAG;
	desc.arginfo = QCOM_SCM_ARGS(5);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_assign_mem(struct device *dev, phys_addr_t mem_region,
			  size_t mem_sz, phys_addr_t src, size_t src_sz,
			  phys_addr_t dest, size_t dest_sz)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_ASSIGN,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = mem_region;
	desc.args[1] = mem_sz;
	desc.args[2] = src;
	desc.args[3] = src_sz;
	desc.args[4] = dest;
	desc.args[5] = dest_sz;
	desc.args[6] = 0;

	desc.arginfo = QCOM_SCM_ARGS(7, QCOM_SCM_RO, QCOM_SCM_VAL,
				     QCOM_SCM_RO, QCOM_SCM_VAL, QCOM_SCM_RO,
				     QCOM_SCM_VAL, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_mem_protect_sd_ctrl(struct device *dev, u32 devid,
				phys_addr_t mem_addr, u64 mem_size, u32 vmid)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_CMD_SD_CTRL,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = devid;
	desc.args[1] = mem_addr;
	desc.args[2] = mem_size;
	desc.args[3] = vmid;
	desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_VAL,
				     QCOM_SCM_RW, QCOM_SCM_VAL, QCOM_SCM_VAL);
	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_kgsl_set_smmu_aperture(struct device *dev,
					unsigned int num_context_bank)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_CP_SMMU_APERTURE_ID,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = 0xffff0000 | ((QCOM_SCM_CP_APERTURE_REG & 0xff) << 8) |
			(num_context_bank & 0xff);
	desc.args[1] = 0xffffffff;
	desc.args[2] = 0xffffffff;
	desc.args[3] = 0xffffffff;
	desc.arginfo = QCOM_SCM_ARGS(4);

	ret = qcom_scm_call(dev, &desc);

	return ret;
}

/**
 * The following shmbridge functions should be called before the SCM driver
 * has been initialized. If not, there could be errors that might cause the
 * system to crash.
 */
int __qcom_scm_enable_shm_bridge(struct device *dev)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MEMP_SHM_BRIDGE_ENABLE,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_delete_shm_bridge(struct device *dev, u64 handle)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MEMP_SHM_BRIDGE_DELETE,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = handle;
	desc.arginfo = QCOM_SCM_ARGS(1, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	return ret;
}

int __qcom_scm_create_shm_bridge(struct device *dev, u64 pfn_and_ns_perm_flags,
			u64 ipfn_and_s_perm_flags, u64 size_and_flags,
			u64 ns_vmids, u64 *handle)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MEMP_SHM_BRDIGE_CREATE,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = pfn_and_ns_perm_flags;
	desc.args[1] = ipfn_and_s_perm_flags;
	desc.args[2] = size_and_flags;
	desc.args[3] = ns_vmids;

	desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_VAL, QCOM_SCM_VAL,
					QCOM_SCM_VAL, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	if (handle)
		*handle = desc.res[1];

	return ret ? : desc.res[0];
}

int __qcom_scm_smmu_prepare_atos_id(struct device *dev, u64 dev_id, int cb_num,
					int operation)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_SMMU_PREPARE_ATOS_ID,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = dev_id;
	desc.args[1] = cb_num;
	desc.args[2] = operation;

	desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_VAL,
					QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	return ret;
}

int __qcom_mdf_assign_memory_to_subsys(struct device *dev, u64 start_addr,
	u64 end_addr, phys_addr_t paddr, u64 size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_MP,
		.cmd = QCOM_SCM_MP_MPU_LOCK_NS_REGION,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = start_addr;
	desc.args[1] = end_addr;
	desc.args[2] = paddr;
	desc.args[3] = size;
	desc.arginfo = QCOM_SCM_ARGS(4);
	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

bool __qcom_scm_dcvs_core_available(struct device *dev)
{
	return __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_INIT) &&
	       __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_UPDATE) &&
	       __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_RESET);
}

bool __qcom_scm_dcvs_ca_available(struct device *dev)
{
	return __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_INIT_CA_V2) &&
	       __qcom_scm_is_call_available(dev, QCOM_SCM_SVC_DCVS,
					    QCOM_SCM_DCVS_UPDATE_CA_V2);
}

int __qcom_scm_dcvs_reset(struct device *dev)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_RESET,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_dcvs_init_v2(struct device *dev, phys_addr_t addr, size_t size,
			    int *version)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_INIT_V2,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = addr;
	desc.args[1] = size;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	if (ret >= 0)
		*version = desc.res[0];
	return ret;
}

int __qcom_scm_dcvs_init_ca_v2(struct device *dev, phys_addr_t addr,
			       size_t size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_INIT_CA_V2,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = addr;
	desc.args[1] = size;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_dcvs_update(struct device *dev, int level, s64 total_time,
			   s64 busy_time)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_UPDATE,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = level;
	desc.args[1] = total_time;
	desc.args[2] = busy_time;
	desc.arginfo = QCOM_SCM_ARGS(3);

	ret = qcom_scm_call_atomic(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_dcvs_update_v2(struct device *dev, int level, s64 total_time,
			     s64 busy_time)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_UPDATE_V2,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = level;
	desc.args[1] = total_time;
	desc.args[2] = busy_time;
	desc.arginfo = QCOM_SCM_ARGS(3);

	ret = qcom_scm_call(dev, &desc);
	return ret ? : desc.res[0];
}

int __qcom_scm_dcvs_update_ca_v2(struct device *dev, int level, s64 total_time,
				 s64 busy_time, int context_count)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_DCVS,
		.cmd = QCOM_SCM_DCVS_UPDATE_CA_V2,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = level;
	desc.args[1] = total_time;
	desc.args[2] = busy_time;
	desc.args[3] = context_count;
	desc.arginfo = QCOM_SCM_ARGS(4);

	ret = qcom_scm_call(dev, &desc);
	return ret ? : desc.res[0];
}

int __qcom_scm_config_set_ice_key(struct device *dev, uint32_t index,
				  phys_addr_t paddr, size_t size,
				  uint32_t cipher, unsigned int data_unit,
				  unsigned int food)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_ES,
		.cmd = QCOM_SCM_ES_CONFIG_SET_ICE_KEY,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = index;
	desc.args[1] = paddr;
	desc.args[2] = size;
	desc.args[3] = cipher;
	desc.args[4] = data_unit;
	desc.args[5] = food;
	desc.arginfo = QCOM_SCM_ARGS(6, QCOM_SCM_VAL, QCOM_SCM_RW, QCOM_SCM_VAL,
				     QCOM_SCM_VAL, QCOM_SCM_VAL, QCOM_SCM_VAL);

	return qcom_scm_call_noretry(dev, &desc);
}

int __qcom_scm_clear_ice_key(struct device *dev, uint32_t index,
			     unsigned int food)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_ES,
		.cmd = QCOM_SCM_ES_CLEAR_ICE_KEY,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = index;
	desc.args[1] = food;
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call_noretry(dev, &desc);
}

int __qcom_scm_hdcp_req(struct device *dev, struct qcom_scm_hdcp_req *req,
			u32 req_cnt, u32 *resp)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_HDCP,
		.cmd = QCOM_SCM_HDCP_INVOKE,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	if (req_cnt > QCOM_SCM_HDCP_MAX_REQ_CNT)
		return -ERANGE;

	desc.args[0] = req[0].addr;
	desc.args[1] = req[0].val;
	desc.args[2] = req[1].addr;
	desc.args[3] = req[1].val;
	desc.args[4] = req[2].addr;
	desc.args[5] = req[2].val;
	desc.args[6] = req[3].addr;
	desc.args[7] = req[3].val;
	desc.args[8] = req[4].addr;
	desc.args[9] = req[4].val;
	desc.arginfo = QCOM_SCM_ARGS(10);

	ret = qcom_scm_call(dev, &desc);
	*resp = desc.res[0];

	return ret;
}

int __qcom_scm_lmh_read_buf_size(struct device *dev, int *size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_LMH,
		.cmd = QCOM_SCM_LMH_DEBUG_READ_BUF_SIZE,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.arginfo = QCOM_SCM_ARGS(0);

	ret = qcom_scm_call(dev, &desc);

	if (size)
		*size = desc.res[0];

	return ret;
}

int __qcom_scm_lmh_limit_dcvsh(struct device *dev, phys_addr_t payload,
			uint32_t payload_size, u64 limit_node, uint32_t node_id,
			u64 version)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_LMH,
		.cmd = QCOM_SCM_LMH_LIMIT_DCVSH,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = payload;
	desc.args[1] = payload_size;
	desc.args[2] = limit_node;
	desc.args[3] = node_id;
	desc.args[4] = version;
	desc.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_RO, QCOM_SCM_VAL, QCOM_SCM_VAL,
					QCOM_SCM_VAL, QCOM_SCM_VAL);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_lmh_debug_read(struct device *dev, phys_addr_t payload,
				uint32_t size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_LMH,
		.cmd = QCOM_SCM_LMH_DEBUG_READ,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = payload;
	desc.args[1] = size;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_lmh_debug_config_write(struct device *dev, u64 cmd_id,
			phys_addr_t payload, int payload_size, uint32_t *buf,
			int buf_size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_LMH,
		.cmd = cmd_id,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	if (buf_size < 3)
		return -EINVAL;

	desc.args[0] = payload;
	desc.args[1] = payload_size;
	desc.args[2] = buf[0];
	desc.args[3] = buf[1];
	desc.args[4] = buf[2];
	desc.arginfo = QCOM_SCM_ARGS(5, QCOM_SCM_RO, QCOM_SCM_VAL, QCOM_SCM_VAL,
					QCOM_SCM_VAL, QCOM_SCM_VAL);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_lmh_get_type(struct device *dev, phys_addr_t payload,
			u64 payload_size, u64 debug_type, uint32_t get_from,
			uint32_t *size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_LMH,
		.cmd = QCOM_SCM_LMH_DEBUG_GET_TYPE,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = payload;
	desc.args[1] = payload_size;
	desc.args[2] = debug_type;
	desc.args[3] = get_from;
	desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL, QCOM_SCM_VAL,
					QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	if (size)
		*size = desc.res[0];

	return ret;
}

int __qcom_scm_lmh_fetch_data(struct device *dev,
		u32 node_id, u32 debug_type, uint32_t *peak, uint32_t *avg)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_LMH,
		.cmd = QCOM_SCM_LMH_DEBUG_FETCH_DATA,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = node_id;
	desc.args[1] = debug_type;
	desc.arginfo = SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	if (peak)
		*peak = desc.res[0];
	if (avg)
		*avg = desc.res[1];

	return ret;
}

int __qcom_scm_smmu_change_pgtbl_format(struct device *dev, u64 dev_id,
					int cbndx)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = QCOM_SCM_SMMU_CHANGE_PGTBL_FORMAT,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = dev_id;
	desc.args[1] = cbndx;
	desc.args[2] = 1;	/* Enable */

	desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_VAL, QCOM_SCM_VAL,
					QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	return ret;
}

int __qcom_scm_qsmmu500_wait_safe_toggle(struct device *dev, bool en)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = QCOM_SCM_SMMU_SECURE_LUT,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = QCOM_SCM_SMMU_CONFIG_ERRATA1_CLIENT_ALL;
	desc.args[1] = en;
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call_atomic(dev, &desc);
}

int __qcom_scm_smmu_notify_secure_lut(struct device *dev, u64 dev_id,
				      bool secure)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = QCOM_SCM_SMMU_SECURE_LUT,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = dev_id;
	desc.args[1] = secure;
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_qdss_invoke(struct device *dev, phys_addr_t addr, size_t size,
			   u64 *out)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_QDSS,
		.cmd = QCOM_SCM_QDSS_INVOKE,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = addr;
	desc.args[1] = size;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RO, QCOM_SCM_VAL);

	ret = qcom_scm_call(dev, &desc);

	if (out)
		*out = desc.res[1];

	return ret ? : desc.res[0];
}

int __qcom_scm_camera_protect_all(struct device *dev, uint32_t protect,
				  uint32_t param)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_CAMERA,
		.cmd = QCOM_SCM_CAMERA_PROTECT_ALL,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = protect;
	desc.args[1] = param;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_VAL, QCOM_SCM_VAL);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_camera_protect_phy_lanes(struct device *dev, bool protect,
					 u64 regmask)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_CAMERA,
		.cmd = QCOM_SCM_CAMERA_PROTECT_PHY_LANES,
		.owner = ARM_SMCCC_OWNER_SIP
	};

	desc.args[0] = protect;
	desc.args[1] = regmask;
	desc.arginfo = QCOM_SCM_ARGS(2);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_tsens_reinit(struct device *dev, int *tsens_ret)
{
	unsigned int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_TSENS,
		.cmd = QCOM_SCM_TSENS_INIT_ID,
	};

	ret = qcom_scm_call(dev, &desc);
	if (tsens_ret)
		*tsens_ret = desc.res[0];

	return ret;
}

int __qcom_scm_reboot(struct device *dev)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_OEM_POWER,
		.cmd = QCOM_SCM_OEM_POWER_REBOOT,
		.owner = ARM_SMCCC_OWNER_OEM,
	};

	desc.arginfo = QCOM_SCM_ARGS(0);

	return qcom_scm_call_atomic(dev, &desc);
}

int __qcom_scm_ice_restore_cfg(struct device *dev)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_KEYSTORE,
		.cmd = QCOM_SCM_ICE_RESTORE_KEY_ID,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS
	};

	desc.arginfo = QCOM_SCM_ARGS(0);

	return qcom_scm_call(dev, &desc);
}

int __qcom_scm_register_qsee_log_buf(struct device *dev, phys_addr_t buf,
				     size_t len)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_QSEELOG,
		.cmd = QCOM_SCM_QSEELOG_REGISTER,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS
	};


	desc.args[0] = buf;
	desc.args[1] = len;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_query_encrypted_log_feature(struct device *dev, u64 *enabled)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_QSEELOG,
		.cmd = QCOM_SCM_QUERY_ENCR_LOG_FEAT_ID,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS
	};

	desc.arginfo = QCOM_SCM_ARGS(0);

	ret = qcom_scm_call(dev, &desc);
	if (enabled)
		*enabled = desc.res[0];

	return ret;
}

int __qcom_scm_request_encrypted_log(struct device *dev, phys_addr_t buf,
				     size_t len, uint32_t log_id)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_QSEELOG,
		.cmd = QCOM_SCM_REQUEST_ENCR_LOG_ID,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS
	};

	desc.args[0] = buf;
	desc.args[1] = len;
	desc.args[2] = log_id;
	desc.arginfo = QCOM_SCM_ARGS(3, QCOM_SCM_RW);

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_invoke_smc_legacy(struct device *dev, phys_addr_t in_buf,
        size_t in_buf_size, phys_addr_t out_buf, size_t out_buf_size,
        int32_t *result, u64 *response_type, unsigned int *data)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMCINVOKE,
		.cmd = QCOM_SCM_SMCINVOKE_INVOKE_LEGACY,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS
	};

	desc.args[0] = in_buf;
	desc.args[1] = in_buf_size;
	desc.args[2] = out_buf;
	desc.args[3] = out_buf_size;
	desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL, QCOM_SCM_RW,
					QCOM_SCM_VAL);

	ret = qcom_scm_call_noretry(dev, &desc);

	if (result)
		*result = desc.res[1];

	if (response_type)
		*response_type = desc.res[0];

	if (data)
		*data = desc.res[2];

	return ret;
}

int __qcom_scm_invoke_smc(struct device *dev, phys_addr_t in_buf,
	size_t in_buf_size, phys_addr_t out_buf, size_t out_buf_size,
	int32_t *result, u64 *response_type, unsigned int *data)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMCINVOKE,
		.cmd = QCOM_SCM_SMCINVOKE_INVOKE,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS
	};

	desc.args[0] = in_buf;
	desc.args[1] = in_buf_size;
	desc.args[2] = out_buf;
	desc.args[3] = out_buf_size;
	desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL, QCOM_SCM_RW,
					QCOM_SCM_VAL);

	ret = qcom_scm_call_noretry(dev, &desc);

	if (result)
		*result = desc.res[1];

	if (response_type)
		*response_type = desc.res[0];

	if (data)
		*data = desc.res[2];

	return ret;
}

int __qcom_scm_invoke_callback_response(struct device *dev, phys_addr_t out_buf,
	size_t out_buf_size, int32_t *result, u64 *response_type,
	unsigned int *data)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMCINVOKE,
		.cmd = QCOM_SCM_SMCINVOKE_CB_RSP,
		.owner = ARM_SMCCC_OWNER_TRUSTED_OS
	};

	desc.args[0] = out_buf;
	desc.args[1] = out_buf_size;
	desc.arginfo = QCOM_SCM_ARGS(2, QCOM_SCM_RW, QCOM_SCM_VAL);

	ret = qcom_scm_call_noretry(dev, &desc);

	if (result)
		*result = desc.res[1];

	if (response_type)
		*response_type = desc.res[0];

	if (data)
		*data = desc.res[2];

	return ret;
}


#define TZ_SVC_MEMORY_PROTECTION 12 /* Memory protection service. */

#define TZ_MPU_LOCK_AUDIO_BUFFER  \
	TZ_SYSCALL_CREATE_SMC_ID(TZ_OWNER_SIP, TZ_SVC_MEMORY_PROTECTION, 0x06)

#define TZ_MPU_LOCK_AUDIO_BUFFER_PARAM_ID \
	TZ_SYSCALL_CREATE_PARAM_ID_2( \
	TZ_SYSCALL_PARAM_TYPE_VAL, TZ_SYSCALL_PARAM_TYPE_VAL)
int __qcom_scm_mem_protect_audio(struct device *dev, phys_addr_t paddr,
					size_t size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = TZ_SVC_MEMORY_PROTECTION,
		.cmd = 0x6,
		.owner = TZ_OWNER_SIP,
	};

	desc.args[0] = paddr;
	desc.args[1] = size;
	desc.arginfo = QCOM_SCM_ARGS(2);

	ret = qcom_scm_call(dev, &desc);

	return ret;
}

int __qcom_scm_qseecom_do(struct device *dev, u32 cmd_id, struct scm_desc *desc,
			  bool retry)
{
	int _ret;
	struct qcom_scm_desc _desc;

	memcpy(&_desc.args, desc->args, sizeof(_desc.args));
	_desc.owner = (cmd_id & 0x3f000000) >> 24;
	_desc.svc = (cmd_id & 0xff00) >> 8;
	_desc.cmd = (cmd_id & 0xff);
	_desc.arginfo = desc->arginfo;

	if (retry)
		_ret = qcom_scm_call(dev, &_desc);
	else
		_ret = qcom_scm_call_noretry(dev, &_desc);

	memcpy(desc->ret, &_desc.res, sizeof(_desc.res));

	return _ret;
}

int __qcom_scm_paravirt_smmu_attach(struct device *dev, u64 sid,
				    u64 asid, u64 ste_pa, u64 ste_size,
				    u64 cd_pa, u64 cd_size)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = ARM_SMMU_PARAVIRT_CMD,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = SMMU_PARAVIRT_OP_ATTACH;
	desc.args[1] = sid;
	desc.args[2] = asid;
	desc.args[3] = 0;
	desc.args[4] = ste_pa;
	desc.args[5] = ste_size;
	desc.args[6] = cd_pa;
	desc.args[7] = cd_size;
	desc.arginfo = ARM_SMMU_PARAVIRT_DESCARG;
	ret = qcom_scm_call(dev, &desc);
	return ret ? : desc.res[0];
}

int __qcom_scm_paravirt_tlb_inv(struct device *dev, u64 asid)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = ARM_SMMU_PARAVIRT_CMD,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = SMMU_PARAVIRT_OP_INVAL_ASID;
	desc.args[1] = 0;
	desc.args[2] = asid;
	desc.args[3] = 0;
	desc.args[4] = 0;
	desc.args[5] = 0;
	desc.args[6] = 0;
	desc.args[7] = 0;
	desc.arginfo = ARM_SMMU_PARAVIRT_DESCARG;
	ret = qcom_scm_call_atomic(dev, &desc);
	return ret ? : desc.res[0];
}

int __qcom_scm_paravirt_smmu_detach(struct device *dev, u64 sid)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_SMMU_PROGRAM,
		.cmd = ARM_SMMU_PARAVIRT_CMD,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

	desc.args[0] = SMMU_PARAVIRT_OP_DETACH;
	desc.args[1] = sid;
	desc.args[2] = 0;
	desc.args[3] = 0;
	desc.args[4] = 0;
	desc.args[5] = 0;
	desc.args[6] = 0;
	desc.args[7] = 0;
	desc.arginfo = ARM_SMMU_PARAVIRT_DESCARG;
	ret = qcom_scm_call(dev, &desc);
	return ret ? : desc.res[0];
}

#ifdef CONFIG_QCOM_RTIC

#define TZ_RTIC_ENABLE_MEM_PROTECTION	0x4
int  __init scm_mem_protection_init_do(struct device *dev)
{
	int ret = 0, resp;
	struct qcom_scm_desc desc = {
		.svc = SCM_SVC_RTIC,
		.cmd = TZ_RTIC_ENABLE_MEM_PROTECTION,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = 0;
	desc.arginfo = 0;

	ret = qcom_scm_call(dev, &desc);
	resp = desc.res[0];

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
#endif

int __qcom_scm_ddrbw_profiler(struct device *dev, phys_addr_t in_buf,
	size_t in_buf_size, phys_addr_t out_buf, size_t out_buf_size)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = TZ_SVC_BW_PROF_ID,
		.owner = ARM_SMCCC_OWNER_SIP,
	};

	desc.args[0] = in_buf;
	desc.args[1] = in_buf_size;
	desc.args[2] = out_buf;
	desc.args[3] = out_buf_size;
	desc.arginfo = QCOM_SCM_ARGS(4, QCOM_SCM_RW, QCOM_SCM_VAL, QCOM_SCM_RW,
								 QCOM_SCM_VAL);
	ret = qcom_scm_call(dev, &desc);

	return ret;
}

void __qcom_scm_init(void)
{

#if IS_ENABLED(CONFIG_QCOM_SCM_QCPE)
/**
 * The HAB connection should be opened before first SMC call.
 * If not, there could be errors that might cause the
 * system to crash.
 */
	scm_qcpe_hab_open();
#endif
	__query_convention();
}

#if IS_ENABLED(CONFIG_QCOM_SCM_QCPE)
void __qcom_scm_qcpe_exit(void)
{
	scm_qcpe_hab_close();
}
#endif
