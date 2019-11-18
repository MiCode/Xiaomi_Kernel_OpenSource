// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2010,2015,2019 The Linux Foundation. All rights reserved.
 * Copyright (C) 2015 Linaro Ltd.
 */

#include <linux/slab.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/qcom_scm.h>
#include <linux/arm-smccc.h>
#include <linux/dma-mapping.h>

#include "qcom_scm.h"

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

static DEFINE_MUTEX(qcom_scm_lock);

#define MAX_QCOM_SCM_ARGS 10
#define MAX_QCOM_SCM_RETS 3

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

#define LEGACY_FUNCNUM(s, c)	(((s) << 10) | ((c) & 0x3ff))

/**
 * struct qcom_scm_legacy_command - one SCM command buffer
 * @len: total available memory for command and response
 * @buf_offset: start of command buffer
 * @resp_hdr_offset: start of response buffer
 * @id: command to be executed
 * @buf: buffer returned from legacy_get_command_buffer()
 *
 * An SCM command is laid out in memory as follows:
 *
 *	------------------- <--- struct qcom_scm_legacy_command
 *	| command header  |
 *	------------------- <--- legacy_get_command_buffer()
 *	| command buffer  |
 *	------------------- <--- struct qcom_scm_legacy_response and
 *	| response header |      legacy_command_to_response()
 *	------------------- <--- legacy_get_response_buffer()
 *	| response buffer |
 *	-------------------
 *
 * There can be arbitrary padding between the headers and buffers so
 * you should always use the appropriate qcom_scm_get_*_buffer() routines
 * to access the buffers in a safe manner.
 */
struct qcom_scm_legacy_command {
	__le32 len;
	__le32 buf_offset;
	__le32 resp_hdr_offset;
	__le32 id;
	__le32 buf[0];
};

/**
 * struct qcom_scm_legacy_response - one SCM response buffer
 * @len: total available memory for response
 * @buf_offset: start of response data relative to start of
 *              qcom_scm_legacy_response
 * @is_complete: indicates if the command has finished processing
 */
struct qcom_scm_legacy_response {
	__le32 len;
	__le32 buf_offset;
	__le32 is_complete;
};

/**
 * legacy_command_to_response() - Get a pointer to a qcom_scm_legacy_response
 * @cmd: command
 *
 * Returns a pointer to a response for a command.
 */
static inline struct qcom_scm_legacy_response *legacy_command_to_response(
		const struct qcom_scm_legacy_command *cmd)
{
	return (void *)cmd + le32_to_cpu(cmd->resp_hdr_offset);
}

/**
 * legacy_get_command_buffer() - Get a pointer to a command buffer
 * @cmd: command
 *
 * Returns a pointer to the command buffer of a command.
 */
static inline void *legacy_get_command_buffer(
		const struct qcom_scm_legacy_command *cmd)
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
		const struct qcom_scm_legacy_response *rsp)
{
	return (void *)rsp + le32_to_cpu(rsp->buf_offset);
}

static void __qcom_scm_call_do(const struct arm_smccc_args *smc,
			      struct arm_smccc_res *res)
{
	do {
		arm_smccc_smc(smc->a[0], smc->a[1], smc->a[2], smc->a[3],
			      smc->a[4], smc->a[5], smc->a[6], smc->a[7], res);
	} while (res->a0 == QCOM_SCM_INTERRUPTED);
}

/**
 * qcom_scm_call() - Send an SCM command
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
static int qcom_scm_call(struct device *dev, struct qcom_scm_desc *desc)
{
	int arglen = desc->arginfo & 0xf;
	int ret = 0, context_id;
	size_t i;
	struct qcom_scm_legacy_command *cmd;
	struct qcom_scm_legacy_response *rsp;
	struct arm_smccc_args smc = {{0}};
	struct arm_smccc_res res;
	const size_t cmd_len = arglen * sizeof(__le32);
	const size_t resp_len = MAX_QCOM_SCM_RETS * sizeof(__le32);
	size_t alloc_len = sizeof(*cmd) + cmd_len + sizeof(*rsp) + resp_len;
	dma_addr_t cmd_phys;
	__le32 *arg_buf;
	__le32 *res_buf;

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
	if (res.a0)
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

#define LEGACY_CLASS_REGISTER		(0x2 << 8)
#define LEGACY_MASK_IRQS		BIT(5)
#define LEGACY_ATOMIC_ID(svc, cmd, n) \
				((LEGACY_FUNCNUM(svc, cmd) << 12) | \
				LEGACY_CLASS_REGISTER | \
				LEGACY_MASK_IRQS | \
				(n & 0xf))

/**
 * qcom_scm_call_atomic1() - Send an atomic SCM command with one argument
 * @svc_id: service identifier
 * @cmd_id: command identifier
 * @arg1: first argument
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
static s32 qcom_scm_call_atomic1(u32 svc, u32 cmd, u32 arg1)
{
	int context_id;
	struct arm_smccc_args smc = {0};
	struct arm_smccc_res res;

	smc.a[0] = LEGACY_ATOMIC_ID(svc, cmd, 1);
	smc.a[1] = (unsigned long)&context_id;
	smc.a[2] = arg1;
	arm_smccc_smc(smc.a[0], smc.a[1], smc.a[2], smc.a[3],
		      smc.a[4], smc.a[5], smc.a[6], smc.a[7], &res);

	return res.a0;
}

/**
 * qcom_scm_call_atomic2() - Send an atomic SCM command with two arguments
 * @svc_id:	service identifier
 * @cmd_id:	command identifier
 * @arg1:	first argument
 * @arg2:	second argument
 *
 * This shall only be used with commands that are guaranteed to be
 * uninterruptable, atomic and SMP safe.
 */
static s32 qcom_scm_call_atomic2(u32 svc, u32 cmd, u32 arg1, u32 arg2)
{
	int context_id;
	struct arm_smccc_args smc = {0};
	struct arm_smccc_res res;

	smc.a[0] = LEGACY_ATOMIC_ID(svc, cmd, 2);
	smc.a[1] = (unsigned long)&context_id;
	smc.a[2] = arg1;
	smc.a[3] = arg2;
	arm_smccc_smc(smc.a[0], smc.a[1], smc.a[2], smc.a[3],
		      smc.a[4], smc.a[5], smc.a[6], smc.a[7], &res);

	return res.a0;
}

/**
 * qcom_scm_set_cold_boot_addr() - Set the cold boot address for cpus
 * @entry: Entry point function for the cpus
 * @cpus: The cpumask of cpus that will use the entry point
 *
 * Set the cold boot address of the cpus. Any cpu outside the supported
 * range would be removed from the cpu present mask.
 */
int __qcom_scm_set_cold_boot_addr(void *entry, const cpumask_t *cpus)
{
	int flags = 0;
	int cpu;
	int scm_cb_flags[] = {
		QCOM_SCM_FLAG_COLDBOOT_CPU0,
		QCOM_SCM_FLAG_COLDBOOT_CPU1,
		QCOM_SCM_FLAG_COLDBOOT_CPU2,
		QCOM_SCM_FLAG_COLDBOOT_CPU3,
	};

	if (!cpus || (cpus && cpumask_empty(cpus)))
		return -EINVAL;

	for_each_cpu(cpu, cpus) {
		if (cpu < ARRAY_SIZE(scm_cb_flags))
			flags |= scm_cb_flags[cpu];
		else
			set_cpu_present(cpu, false);
	}

	return qcom_scm_call_atomic2(QCOM_SCM_SVC_BOOT, QCOM_SCM_BOOT_SET_ADDR,
				    flags, virt_to_phys(entry));
}

/**
 * qcom_scm_set_warm_boot_addr() - Set the warm boot address for cpus
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
		.arginfo = QCOM_SCM_ARGS(2),
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

	desc.args[0] = flags;
	desc.args[1] = virt_to_phys(entry);
	ret = qcom_scm_call(dev, &desc);
	if (!ret) {
		for_each_cpu(cpu, cpus)
			qcom_scm_wb[cpu].entry = entry;
	}

	return ret;
}

/**
 * qcom_scm_cpu_power_down() - Power down the cpu
 * @flags - Flags to flush cache
 *
 * This is an end point to power down cpu. If there was a pending interrupt,
 * the control would return from this function, otherwise, the cpu jumps to the
 * warm boot entry point set for this cpu upon reset.
 */
void __qcom_scm_cpu_power_down(u32 flags)
{
	qcom_scm_call_atomic1(QCOM_SCM_SVC_BOOT, QCOM_SCM_BOOT_TERMINATE_PC,
			flags & QCOM_SCM_FLUSH_FLAG_MASK);
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

int __qcom_scm_set_dload_mode(struct device *dev, bool enable)
{
	return qcom_scm_call_atomic2(QCOM_SCM_SVC_BOOT,
				     QCOM_SCM_BOOT_SET_DLOAD_MODE,
				     enable ? QCOM_SCM_BOOT_SET_DLOAD_MODE : 0,
				     0);
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

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_pas_shutdown(struct device *dev, u32 peripheral)
{
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_PIL,
		.cmd = QCOM_SCM_PIL_PAS_SHUTDOWN,
		.owner = ARM_SMCCC_OWNER_SIP,
	};
	int ret;

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

int __qcom_scm_io_readl(struct device *dev, phys_addr_t addr,
			unsigned int *val)
{
	int ret;

	ret = qcom_scm_call_atomic1(QCOM_SCM_SVC_IO, QCOM_SCM_IO_READ, addr);
	if (ret >= 0)
		*val = ret;

	return ret < 0 ? ret : 0;
}

int __qcom_scm_io_writel(struct device *dev, phys_addr_t addr, unsigned int val)
{
	return qcom_scm_call_atomic2(QCOM_SCM_SVC_IO, QCOM_SCM_IO_WRITE,
				     addr, val);
}

int __qcom_scm_is_call_available(struct device *dev, u32 svc_id, u32 cmd_id)
{
	int ret;
	struct qcom_scm_desc desc = {
		.svc = QCOM_SCM_SVC_INFO,
		.cmd = QCOM_SCM_INFO_IS_CALL_AVAIL,
		.args[0] = (svc_id << 10) | cmd_id,
		.arginfo = QCOM_SCM_ARGS(1),
	};

	ret = qcom_scm_call(dev, &desc);

	return ret ? : desc.res[0];
}

int __qcom_scm_restore_sec_cfg(struct device *dev, u32 device_id,
			       u32 spare)
{
	return -ENODEV;
}

int __qcom_scm_iommu_secure_ptbl_size(struct device *dev, u32 spare,
				      size_t *size)
{
	return -ENODEV;
}

int __qcom_scm_iommu_secure_ptbl_init(struct device *dev, u64 addr, u32 size,
				      u32 spare)
{
	return -ENODEV;
}

int __qcom_scm_assign_mem(struct device *dev, phys_addr_t mem_region,
			  size_t mem_sz, phys_addr_t src, size_t src_sz,
			  phys_addr_t dest, size_t dest_sz)
{
	return -ENODEV;
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
	desc.arginfo = QCOM_SCM_ARGS(req_cnt * 2);

	ret = qcom_scm_call(dev, &desc);
	*resp = desc.res[0];

	return ret;
}

int __qcom_scm_qsmmu500_wait_safe_toggle(struct device *dev, bool enable)
{
	return -ENODEV;
}

void __qcom_scm_init(void)
{
}
