// SPDX-License-Identifier: GPL-2.0

/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * GenieZone (hypervisor-based seucrity platform) enables hardware protected
 * and isolated security execution environment, includes
 * 1. GZ hypervisor
 * 2. Hypervisor-TEE OS (built-in Trusty OS)
 * 3. Drivers (ex: debug, communication and interrupt) for GZ and
 *    hypervisor-TEE OS
 * 4. GZ and hypervisor-TEE and GZ framework (supporting multiple TEE
 *    ecosystem, ex: M-TEE, Trusty, GlobalPlatform, ...)
 */


#include <gz-trusty/smcnr_table.h>
#include <linux/platform_device.h>

#define SMC_UNDEFINED 0xFFFFFFFF

/* gz_smcnr_table:
 * Select SMC number by looking up gz_smcnr_table.
 * This table is introduced to support more TEEs and drive them with the same
 * code.
 */
static const uint32_t gz_smcnr_table[SMCF_END][TEE_ID_END] = {
	[SMCF_FC_RESERVED] = {
		SMC_FC_GZ_RESERVED, SMC_FC_GZ_RESERVED },
	[SMCF_FC_FIQ_EXIT] = {
		SMC_FC_GZ_FIQ_EXIT, SMC_FC_GZ_FIQ_EXIT },
	[SMCF_FC_REQUEST_FIQ] = {
		SMC_FC_GZ_REQUEST_FIQ, SMC_FC_GZ_REQUEST_FIQ},
	[SMCF_FC_GET_NEXT_IRQ] = {
		SMC_FC_GZ_GET_NEXT_IRQ, SMC_FC_GZ_GET_NEXT_IRQ },
	[SMCF_FC_FIQ_ENTER] = {
		SMC_FC_GZ_FIQ_ENTER, SMC_FC_GZ_FIQ_ENTER },
	[SMCF_FC64_SET_FIQ_HANDLER] = {
		SMC_FC64_GZ_SET_FIQ_HANDLER, SMC_FC64_GZ_SET_FIQ_HANDLER },
	[SMCF_FC64_GET_FIQ_REGS] = {
		SMC_FC64_GZ_GET_FIQ_REGS, SMC_FC64_GZ_GET_FIQ_REGS },
	[SMCF_FC_CPU_SUSPEND] = {
		SMC_FC_GZ_CPU_SUSPEND, SMC_FC_GZ_CPU_SUSPEND },
	[SMCF_FC_CPU_RESUME] = {
		SMC_FC_GZ_CPU_RESUME, SMC_FC_GZ_CPU_RESUME },
	[SMCF_FC_AARCH_SWITCH] = {
		SMC_FC_GZ_AARCH_SWITCH, SMC_FC_GZ_AARCH_SWITCH },
	[SMCF_FC_GET_VERSION_STR] = {
		SMC_FC_GZ_GET_VERSION_STR, SMC_FC_GZ_GET_VERSION_STR },
	[SMCF_FC_API_VERSION] = {
		SMC_FC_GZ_API_VERSION, SMC_FC_GZ_API_VERSION },
	[SMCF_FC_GET_CMASK] = {
		SMC_FC_GZ_GET_CMASK, SMC_FC_GZ_GET_CMASK },
	[SMCF_SC_NS_RETURN] = {
		SMC_SC_GZ_NS_RETURN, SMC_SC_GZ_NS_RETURN },

	[MT_SMCF_SC_ADD] = {
		MT_SMC_SC_GZ_ADD, SMC_UNDEFINED },
	[MT_SMCF_SC_MDELAY] = {
		MT_SMC_SC_GZ_MDELAY, SMC_UNDEFINED },
	[MT_SMCF_SC_IRQ_LATENCY] = {
		MT_SMC_SC_GZ_IRQ_LATENCY, SMC_UNDEFINED },
	[MT_SMCF_SC_INTERCEPT_MMIO] = {
		MT_SMC_SC_GZ_INTERCEPT_MMIO, SMC_UNDEFINED },
	[MT_SMCF_FC_THREADS] = {
		MT_SMC_FC_GZ_THREADS, SMC_UNDEFINED },
	[MT_SMCF_FC_THREADSTATS] = {
		MT_SMC_FC_GZ_THREADSTATS, SMC_UNDEFINED },
	[MT_SMCF_FC_THREADLOAD] = {
		MT_SMC_FC_GZ_THREADLOAD, SMC_UNDEFINED },
	[MT_SMCF_FC_HEAP_DUMP] = {
		MT_SMC_FC_GZ_HEAP_DUMP, SMC_UNDEFINED },
	[MT_SMCF_FC_APPS] = {
		MT_SMC_FC_GZ_APPS, SMC_UNDEFINED },
	[MT_SMCF_FC_MEM_USAGE] = {
		MT_SMC_FC_GZ_MEM_USAGE, SMC_UNDEFINED },
	[MT_SMCF_FC_DEVAPC_VIO] = {
		MT_SMC_FC_GZ_DEVAPC_VIO, SMC_UNDEFINED },
	[MT_SMCF_SC_SET_RAMCONSOLE] = {
		MT_SMC_SC_GZ_SET_RAMCONSOLE, SMC_UNDEFINED },
	[MT_SMCF_SC_VPU] = {
		MT_SMC_SC_GZ_VPU, SMC_UNDEFINED },
	[SMCF_FC_TEST_ADD] = {
		SMC_UNDEFINED, SMC_FC_NBL_TEST_ADD },
	[SMCF_FC_TEST_MULTIPLY] = {
		SMC_UNDEFINED, SMC_FC_NBL_TEST_MULTIPLY },
	[SMCF_SC_TEST_ADD] = {
		SMC_UNDEFINED, SMC_SC_NBL_TEST_ADD },
	[SMCF_SC_TEST_MULTIPLY] = {
		SMC_UNDEFINED, SMC_SC_NBL_TEST_MULTIPLY },

	[SMCF_SC_RESTART_LAST] = {
		SMC_SC_TRU_RESTART_LAST, SMC_SC_NBL_RESTART_LAST },
	[SMCF_SC_LOCKED_NOP] = {
		SMC_SC_TRU_LOCKED_NOP, SMC_SC_NBL_LOCKED_NOP },
	[SMCF_SC_RESTART_FIQ] = {
		SMC_SC_TRU_RESTART_FIQ, SMC_SC_GZ_RESTART_FIQ },
	[SMCF_SC_NOP] = {
		SMC_SC_TRU_NOP, SMC_SC_NBL_NOP },

	[SMCF_SC_SHARED_LOG_VERSION] = {
		SMC_SC_GZ_SHARED_LOG_VERSION, SMC_SC_GZ_SHARED_LOG_VERSION },
	[SMCF_SC_SHARED_LOG_ADD] = {
		SMC_SC_GZ_SHARED_LOG_ADD, SMC_SC_GZ_SHARED_LOG_ADD },
	[SMCF_SC_SHARED_LOG_RM] = {
		SMC_SC_GZ_SHARED_LOG_RM, SMC_SC_GZ_SHARED_LOG_RM },

	[SMCF_SC_VIRTIO_GET_DESCR] = {
		SMC_SC_GZ_VIRTIO_GET_DESCR, SMC_SC_NBL_VIRTIO_GET_DESCR },
	[SMCF_SC_VIRTIO_START] = {
		SMC_SC_GZ_VIRTIO_START, SMC_SC_NBL_VIRTIO_START },
	[SMCF_SC_VIRTIO_STOP] = {
		SMC_SC_GZ_VIRTIO_STOP, SMC_SC_NBL_VIRTIO_STOP },
	[SMCF_SC_VDEV_RESET] = {
		SMC_SC_GZ_VDEV_RESET, SMC_SC_NBL_VDEV_RESET },
	[SMCF_SC_VDEV_KICK_VQ] = {
		SMC_SC_GZ_VDEV_KICK_VQ, SMC_SC_NBL_VDEV_KICK_VQ },
	[SMCF_NC_VDEV_KICK_VQ] = {
		SMC_NC_GZ_VDEV_KICK_VQ, SMC_NC_NBL_VDEV_KICK_VQ }
};

int init_smcnr_table(struct device *dev, enum tee_id_t tee_id)
{
	int ver = 0;

	if (!dev || !is_tee_id(tee_id)) {
		WARN(1, "Error parameters %p, %d\n", dev, tee_id);
		return -EINVAL;
	}

	ver = trusty_fast_call32(dev, SMC_FC_GZ_API_VERSION,
				 TRUSTY_API_VERSION_CURRENT, tee_id, 0);

	if (ver == SMC_UNDEFINED)
		ver = 0;

	if (ver < TRUSTY_API_VERSION_SMCNR_TABLE) {
		pr_info("GZ SMC version(%u) is not supported for MTEE %d, %s\n",
			ver, tee_id, "please update");
		return -ENODEV;
	}

	pr_info("New smcall table ver %d support for MTEE %d\n", ver, tee_id);

	return ver;
}

inline uint32_t get_smcnr_teeid(enum smc_functions fid, enum tee_id_t tee_id)
{
	uint32_t smcnr;

	if (unlikely(fid < 0 || fid >= SMCF_END || !is_tee_id(tee_id))) {
		WARN(1, "ERROR parameters %d, %d\n", fid, tee_id);
		return SMC_UNDEFINED;
	}

	smcnr = gz_smcnr_table[fid][tee_id];
	if (unlikely(smcnr == SMC_UNDEFINED))
		pr_info("ERROR smcnr retrieving failed %d, %d\n", fid, tee_id);

	return smcnr;
}

inline uint32_t get_smcnr_dev(enum smc_functions fid, struct device *dev)
{
	struct trusty_state *s = platform_get_drvdata(to_platform_device(dev));

	return get_smcnr_teeid(fid, s->tee_id);
}
EXPORT_SYMBOL(get_smcnr_dev);
