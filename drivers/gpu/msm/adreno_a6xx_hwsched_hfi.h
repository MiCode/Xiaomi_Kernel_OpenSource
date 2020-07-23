/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_A6XX_HWSCHED_HFI_H_
#define _ADRENO_A6XX_HWSCHED_HFI_H_

enum mem_kind {
	/** @MEMKIND_GENERIC: Used for requesting generic memory */
	MEMKIND_GENERIC = 0,
	/** @MEMKIND_RB: Used for requesting ringbuffer memory */
	MEMKIND_RB,
	/** @MEMKIND_SCRATCH: Used for requesting scratch memory */
	MEMKIND_SCRATCH,
	/**
	 * @MEMKIND_CSW_SMMU_INFO: Used for requesting SMMU record for
	 * preemption context switching
	 */
	MEMKIND_CSW_SMMU_INFO,
	/**
	 * @MEMKIND_CSW_PRIV_NON_SECURE: Used for requesting privileged non
	 * secure preemption records
	 */
	MEMKIND_CSW_PRIV_NON_SECURE,
	/**
	 * @MEMKIND_CSW_PRIV_SECURE: Used for requesting privileged secure
	 * preemption records
	 */
	MEMKIND_CSW_PRIV_SECURE,
	/**
	 * @MEMKIND_CSW_NON_PRIV: Used for requesting non privileged per context
	 * preemption buffer
	 */
	MEMKIND_CSW_NON_PRIV,
	/**
	 * @MEMKIND_CSW_COUNTER: Used for requesting preemption performance
	 * counter save/restore buffer
	 */
	MEMKIND_CSW_COUNTER,
	/**
	 * @MEMKIND_CTXTREC_PREEMPT_CNTR: Used for requesting preemption
	 * counter buffer
	 */
	MEMKIND_CTXTREC_PREEMPT_CNTR,
	/** @MEMKIND_SYSLOG: Used for requesting system log memory */
	MEMKIND_SYS_LOG,
	/** @MEMKIND_CRASH_DUMP: Used for requesting carsh dumper memory */
	MEMKIND_CRASH_DUMP,
	/**
	 * @MEMKIND_MMIO_DPU: Used for requesting Display processing unit's
	 * register space
	 */
	MEMKIND_MMIO_DPU,
	/**
	 * @MEMKIND_MMIO_TCSR: Used for requesting Top CSR(contains SoC
	 * doorbells) register space
	 */
	MEMKIND_MMIO_TCSR,
	/**
	 * @MEMKIND_MMIO_QDSS_STM: Used for requesting QDSS STM register space
	 */
	MEMKIND_MMIO_QDSS_STM,
	/** @MEMKIND_PROFILE: Used for kernel profiling */
	MEMKIND_PROFILE,
	/** @MEMKIND_USER_PROFILING_IBS: Used for user profiling */
	MEMKIND_USER_PROFILE_IBS,
	NUM_HFI_MEMKINDS,
	MEMKIND_NONE = 0x7fffffff,
};

/* CP/GFX pipeline can access */
#define MEMFLAG_GFX_ACC         BIT(0)

/* Buffer has APRIV protection in GFX PTEs */
#define MEMFLAG_GFX_PRIV        BIT(1)

/* Buffer is read-write for GFX PTEs. A 0 indicates read-only */
#define MEMFLAG_GFX_WRITEABLE   BIT(2)

/* GMU can access */
#define MEMFLAG_GMU_ACC         BIT(3)

/* Buffer has APRIV protection in GMU PTEs */
#define MEMFLAG_GMU_PRIV        BIT(4)

/* Buffer is read-write for GMU PTEs. A 0 indicates read-only */
#define MEMFLAG_GMU_WRITEABLE   BIT(5)

/* Buffer is located in GMU's non-cached bufferable VA range */
#define MEMFLAG_GMU_BUFFERABLE  BIT(6)

/* Buffer is located in GMU's cacheable VA range */
#define MEMFLAG_GMU_CACHEABLE   BIT(7)

/* Host can access */
#define MEMFLAG_HOST_ACC        BIT(8)

/* Host initializes the buffer */
#define MEMFLAG_HOST_INIT       BIT(9)

struct mem_alloc_entry {
	struct hfi_mem_alloc_desc desc;
	struct kgsl_memdesc *gpu_md;
};

struct a6xx_hwsched_hfi {
	struct mem_alloc_entry mem_alloc_table[32];
	u32 mem_alloc_entries;
	/** @pending_ack: To track un-ack'd hfi packet */
	struct pending_cmd pending_ack;
	/** @irq_mask: Store the hfi interrupt mask */
	u32 irq_mask;
};

struct kgsl_drawobj_cmd;

/**
 * a6xx_hwsched_hfi_probe - Probe hwsched hfi resources
 * @adreno_dev: Pointer to adreno device structure
 *
 * Return: 0 on success and negative error on failure.
 */
int a6xx_hwsched_hfi_probe(struct adreno_device *adreno_dev);

/**
 * a6xx_hwsched_hfi_init - Initialize hfi resources
 * @adreno_dev: Pointer to adreno device structure
 *
 * This function is used to initialize hfi resources
 * once before the very first gmu boot
 *
 * Return: 0 on success and negative error on failure.
 */
int a6xx_hwsched_hfi_init(struct adreno_device *adreno_dev);

/**
 * a6xx_hwsched_hfi_start - Start hfi resources
 * @adreno_dev: Pointer to adreno device structure
 *
 * Send the various hfi packets before booting the gpu
 *
 * Return: 0 on success and negative error on failure.
 */
int a6xx_hwsched_hfi_start(struct adreno_device *adreno_dev);

/**
 * a6xx_hwsched_hfi_stop - Stop the hfi resources
 * @adreno_dev: Pointer to the adreno device
 *
 * This function does the hfi cleanup when powering down the gmu
 */
void a6xx_hwsched_hfi_stop(struct adreno_device *adreno_dev);

/**
 * a6xx_hwched_cp_init - Send CP_INIT via HFI
 * @adreno_dev: Pointer to adreno device structure
 *
 * This function is used to send CP INIT packet and bring
 * GPU out of secure mode using hfi raw packets.
 *
 * Return: 0 on success and negative error on failure.
 */
int a6xx_hwsched_cp_init(struct adreno_device *adreno_dev);

/**
 * a6xx_hfi_send_cmd_async - Send an hfi packet
 * @adreno_dev: Pointer to adreno device structure
 * @data: Data to be sent in the hfi packet
 *
 * Send data in the form of an HFI packet to gmu and wait for
 * it's ack asynchronously
 *
 * Return: 0 on success and negative error on failure.
 */
int a6xx_hfi_send_cmd_async(struct adreno_device *adreno_dev, void *data);

/**
 * a6xx_hwsched_submit_cmdobj - Dispatch IBs to dispatch queues
 * @adreno_dev: Pointer to adreno device structure
 * @flags: Flags associated with the submission
 * @cmdobj: The command object which needs to be submitted
 *
 * This function is used to register the context if needed and submit
 * IBs to the hfi dispatch queues.

 * Return: 0 on success and negative error on failure
 */
int a6xx_hwsched_submit_cmdobj(struct adreno_device *adreno_dev, u32 flags,
	struct kgsl_drawobj_cmd *cmdobj);
#endif
