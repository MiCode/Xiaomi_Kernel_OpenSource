/* SPDX-License-Identifier: GPL-2.0 */

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


#ifndef __SMCALL_TABLE_H__
#define __SMCALL_TABLE_H__

#include <gz-trusty/smcall_mtee.h>
#include <gz-trusty/trusty.h>

/* enum smc_functions:
 * Define all SMC functions of GenieZone.
 * SMCF means SMC Function.
 * The value of this enum corresponds to the array index of gz_smcnr_table.
 * Steps of adding a new SMC number:
 *   1. Define SMC number in smcall_mtee.h.
 *   2. Create an element which should be named as SMCF_[SC/FC]_[function name]
 *      in this enum.
 *   2. Insert an element to gz_smcnr_table and set the SMC number for TEEs.
 *   3. Access the SMC number by MTEE_SMCNR_TID or MTEE_SMCNR.
 */
enum smc_functions {
	SMCF_NONE = 0,
	SMCF_FC_RESERVED,
	SMCF_FC_FIQ_EXIT,
	SMCF_FC_REQUEST_FIQ,
	SMCF_FC_GET_NEXT_IRQ,
	SMCF_FC_FIQ_ENTER,
	SMCF_FC64_SET_FIQ_HANDLER,
	SMCF_FC64_GET_FIQ_REGS,
	SMCF_FC_CPU_SUSPEND,
	SMCF_FC_CPU_RESUME,
	SMCF_FC_AARCH_SWITCH,
	SMCF_FC_GET_VERSION_STR,
	SMCF_FC_API_VERSION,
	SMCF_SC_NS_RETURN,

	MT_SMCF_SC_ADD,
	MT_SMCF_SC_MDELAY,
	MT_SMCF_SC_IRQ_LATENCY,
	MT_SMCF_SC_INTERCEPT_MMIO,
	MT_SMCF_FC_THREADS,
	MT_SMCF_FC_THREADSTATS,
	MT_SMCF_FC_THREADLOAD,
	MT_SMCF_FC_HEAP_DUMP,
	MT_SMCF_FC_APPS,
	MT_SMCF_FC_MEM_USAGE,
	MT_SMCF_FC_DEVAPC_VIO,
	MT_SMCF_SC_SET_RAMCONSOLE,
	MT_SMCF_SC_VPU,

	SMCF_FC_TEST_ADD,
	SMCF_FC_TEST_MULTIPLY,
	SMCF_SC_TEST_ADD,
	SMCF_SC_TEST_MULTIPLY,

	SMCF_SC_RESTART_LAST,
	SMCF_SC_LOCKED_NOP,
	SMCF_SC_RESTART_FIQ,
	SMCF_SC_NOP,

	SMCF_SC_SHARED_LOG_VERSION,
	SMCF_SC_SHARED_LOG_ADD,
	SMCF_SC_SHARED_LOG_RM,

	SMCF_SC_VIRTIO_GET_DESCR,
	SMCF_SC_VIRTIO_START,
	SMCF_SC_VIRTIO_STOP,
	SMCF_SC_VDEV_RESET,
	SMCF_SC_VDEV_KICK_VQ,
	SMCF_NC_VDEV_KICK_VQ,

	SMCF_END
};

/* get GZ SMC version for select smcnr table */
int init_smcnr_table(struct device *dev, enum tee_id_t tee_id);

/* Get SMC number, not suggest to directly use */
uint32_t get_smcnr_teeid(enum smc_functions fid, enum tee_id_t tee_id);
uint32_t get_smcnr_dev(enum smc_functions fid, struct device *dev);

/* Helper macro */
#define MTEE_SMCNR_TID(fid, tee_id) (get_smcnr_teeid((fid), (tee_id)))
#define MTEE_SMCNR(fid, dev) (get_smcnr_dev((fid), (dev)))

#endif /* __SMCALL_TABLE_H__ */
