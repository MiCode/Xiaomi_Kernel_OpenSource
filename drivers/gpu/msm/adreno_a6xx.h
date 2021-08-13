/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_A6XX_H_
#define _ADRENO_A6XX_H_

#include <linux/delay.h>
#include <linux/iopoll.h>

#include "a6xx_reg.h"
#include "adreno_a6xx_gmu.h"
#include "adreno_a6xx_rgmu.h"

/* Snapshot section size of each CP preemption record for A6XX  */
#define A6XX_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES (64 * 1024)

extern const struct adreno_power_ops a6xx_gmu_power_ops;
extern const struct adreno_power_ops a6xx_rgmu_power_ops;
extern const struct adreno_power_ops a630_gmu_power_ops;
extern const struct adreno_power_ops a6xx_hwsched_power_ops;

/**
 * struct a6xx_device - Container for the a6xx_device
 */
struct a6xx_device {
	/** @gmu: Container for the a6xx GMU device */
	struct a6xx_gmu_device gmu;
	/** @rgmu: Container for the a6xx rGMU device */
	struct a6xx_rgmu_device rgmu;
	/** @adreno_dev: Container for the generic adreno device */
	struct adreno_device adreno_dev;
};

/**
 * struct a6xx_protected_regs - container for a protect register span
 */
struct a6xx_protected_regs {
	/** @reg: Physical protected mode register to write to */
	u32 reg;
	/** @start: Dword offset of the starting register in the range */
	u32 start;
	/**
	 * @end: Dword offset of the ending register in the range
	 * (inclusive)
	 */
	u32 end;
	/**
	 * @noaccess: 1 if the register should not be accessible from
	 * userspace, 0 if it can be read (but not written)
	 */
	u32 noaccess;
};

/**
 * struct adreno_a6xx_core - a6xx specific GPU core definitions
 */
struct adreno_a6xx_core {
	/** @base: Container for the generic GPU definitions */
	struct adreno_gpu_core base;
	/** @gmu_major: The maximum GMU version supported by the core */
	u32 gmu_major;
	/** @gmu_minor: The minimum GMU version supported by the core */
	u32 gmu_minor;
	/** @prim_fifo_threshold: target specific value for PC_DBG_ECO_CNTL */
	unsigned int prim_fifo_threshold;
	/** @sqefw_name: Name of the SQE microcode file */
	const char *sqefw_name;
	/** @gmufw_name: Name of the GMU firmware file */
	const char *gmufw_name;
	/** @zap_name: Name of the CPZ zap file */
	const char *zap_name;
	/** @hwcg: List of registers and values to write for HWCG */
	const struct adreno_reglist *hwcg;
	/** @hwcg_count: Number of registers in @hwcg */
	u32 hwcg_count;
	/** @vbif: List of registers and values to write for VBIF */
	const struct adreno_reglist *vbif;
	/** @vbif_count: Number of registers in @vbif */
	u32 vbif_count;
	/** @veto_fal10: veto status for fal10 feature */
	bool veto_fal10;
	/** @pdc_in_aop: True if PDC programmed in AOP */
	bool pdc_in_aop;
	/** @hang_detect_cycles: Hang detect counter timeout value */
	u32 hang_detect_cycles;
	/** @protected_regs: Array of protected registers for the target */
	const struct a6xx_protected_regs *protected_regs;
	/** @disable_tseskip: True if TSESkip logic is disabled */
	bool disable_tseskip;
	/** @gx_cpr_toggle: True to toggle GX CPR FSM to avoid CPR stalls */
	bool gx_cpr_toggle;
	/** @highest_bank_bit: The bit of the highest DDR bank */
	u32 highest_bank_bit;
	/** @ctxt_record_size: Size of the preemption record in bytes */
	u64 ctxt_record_size;
};

#define CP_CLUSTER_FE		0x0
#define CP_CLUSTER_SP_VS	0x1
#define CP_CLUSTER_PC_VS	0x2
#define CP_CLUSTER_GRAS		0x3
#define CP_CLUSTER_SP_PS	0x4
#define CP_CLUSTER_PS		0x5
#define CP_CLUSTER_VPC_PS	0x6

/**
 * struct a6xx_cp_preemption_record - CP context record for
 * preemption.
 * @magic: (00) Value at this offset must be equal to
 * A6XX_CP_CTXRECORD_MAGIC_REF.
 * @info: (04) Type of record. Written non-zero (usually) by CP.
 * we must set to zero for all ringbuffers.
 * @errno: (08) Error code. Initialize this to A6XX_CP_CTXRECORD_ERROR_NONE.
 * CP will update to another value if a preemption error occurs.
 * @data: (12) DATA field in YIELD and SET_MARKER packets.
 * Written by CP when switching out. Not used on switch-in. Initialized to 0.
 * @cntl: (16) RB_CNTL, saved and restored by CP. We must initialize this.
 * @rptr: (20) RB_RPTR, saved and restored by CP. We must initialize this.
 * @wptr: (24) RB_WPTR, saved and restored by CP. We must initialize this.
 * @_pad28: (28) Reserved/padding.
 * @rptr_addr: (32) RB_RPTR_ADDR_LO|HI saved and restored. We must initialize.
 * rbase: (40) RB_BASE_LO|HI saved and restored.
 * counter: (48) Pointer to preemption counter.
 */
struct a6xx_cp_preemption_record {
	uint32_t  magic;
	uint32_t  info;
	uint32_t  errno;
	uint32_t  data;
	uint32_t  cntl;
	uint32_t  rptr;
	uint32_t  wptr;
	uint32_t  _pad28;
	uint64_t  rptr_addr;
	uint64_t  rbase;
	uint64_t  counter;
};

/**
 * struct a6xx_cp_smmu_info - CP preemption SMMU info.
 * @magic: (00) The value at this offset must be equal to
 * A6XX_CP_SMMU_INFO_MAGIC_REF.
 * @_pad4: (04) Reserved/padding
 * @ttbr0: (08) Base address of the page table for the
 * incoming context.
 * @context_idr: (16) Context Identification Register value.
 */
struct a6xx_cp_smmu_info {
	uint32_t  magic;
	uint32_t  _pad4;
	uint64_t  ttbr0;
	uint32_t  asid;
	uint32_t  context_idr;
};

#define A6XX_CP_SMMU_INFO_MAGIC_REF     0x241350D5UL

/**
 * struct cpu_gpu_spinlock - CP spinlock structure for power up list
 * @flag_ucode: flag value set by CP
 * @flag_kmd: flag value set by KMD
 * @turn: turn variable set by both CP and KMD
 * @list_length: this tells CP the last dword in the list:
 * 16 + (4 * (List_Length - 1))
 * @list_offset: this tells CP the start of preemption only list:
 * 16 + (4 * List_Offset)
 */
struct cpu_gpu_lock {
	uint32_t flag_ucode;
	uint32_t flag_kmd;
	uint32_t turn;
	uint16_t list_length;
	uint16_t list_offset;
};

#define A6XX_CP_CTXRECORD_MAGIC_REF     0xAE399D6EUL
/* Size of each CP preemption record */
#define A6XX_CP_CTXRECORD_SIZE_IN_BYTES     (2112 * 1024)
/* Size of the user context record block (in bytes) */
#define A6XX_CP_CTXRECORD_USER_RESTORE_SIZE (192 * 1024)
/* Size of the performance counter save/restore block (in bytes) */
#define A6XX_CP_PERFCOUNTER_SAVE_RESTORE_SIZE   (4 * 1024)

#define A6XX_CP_RB_CNTL_DEFAULT (((ilog2(4) << 8) & 0x1F00) | \
		(ilog2(KGSL_RB_DWORDS >> 1) & 0x3F))

/* Size of the CP_INIT pm4 stream in dwords */
#define A6XX_CP_INIT_DWORDS 12

#define A6XX_INT_MASK \
	((1 << A6XX_INT_CP_AHB_ERROR) |			\
	 (1 << A6XX_INT_ATB_ASYNCFIFO_OVERFLOW) |	\
	 (1 << A6XX_INT_RBBM_GPC_ERROR) |		\
	 (1 << A6XX_INT_CP_SW) |			\
	 (1 << A6XX_INT_CP_HW_ERROR) |			\
	 (1 << A6XX_INT_CP_IB2) |			\
	 (1 << A6XX_INT_CP_IB1) |			\
	 (1 << A6XX_INT_CP_RB) |			\
	 (1 << A6XX_INT_CP_CACHE_FLUSH_TS) |		\
	 (1 << A6XX_INT_RBBM_ATB_BUS_OVERFLOW) |	\
	 (1 << A6XX_INT_RBBM_HANG_DETECT) |		\
	 (1 << A6XX_INT_UCHE_OOB_ACCESS) |		\
	 (1 << A6XX_INT_UCHE_TRAP_INTR) |		\
	 (1 << A6XX_INT_TSB_WRITE_ERROR))

#define A6XX_HWSCHED_INT_MASK \
	((1 << A6XX_INT_CP_AHB_ERROR) |			\
	 (1 << A6XX_INT_ATB_ASYNCFIFO_OVERFLOW) |	\
	 (1 << A6XX_INT_RBBM_GPC_ERROR) |		\
	 (1 << A6XX_INT_RBBM_ATB_BUS_OVERFLOW) |	\
	 (1 << A6XX_INT_UCHE_OOB_ACCESS) |		\
	 (1 << A6XX_INT_UCHE_TRAP_INTR) |		\
	 (1 << A6XX_INT_TSB_WRITE_ERROR))

/**
 * to_a6xx_core - return the a6xx specific GPU core struct
 * @adreno_dev: An Adreno GPU device handle
 *
 * Returns:
 * A pointer to the a6xx specific GPU core struct
 */
static inline const struct adreno_a6xx_core *
to_a6xx_core(struct adreno_device *adreno_dev)
{
	const struct adreno_gpu_core *core = adreno_dev->gpucore;

	return container_of(core, struct adreno_a6xx_core, base);
}

/**
 * timed_poll_check() - polling *gmu* register at given offset until
 * its value changed to match expected value. The function times
 * out and returns after given duration if register is not updated
 * as expected.
 *
 * @device: Pointer to KGSL device
 * @offset: Register offset in dwords
 * @expected_ret: expected register value that stops polling
 * @timeout_ms: time in milliseconds to poll the register
 * @mask: bitmask to filter register value to match expected_ret
 */
static inline int timed_poll_check(struct kgsl_device *device,
		unsigned int offset, unsigned int expected_ret,
		unsigned int timeout_ms, unsigned int mask)
{
	u32 val;
	void __iomem *addr = device->gmu_core.reg_virt +
		((offset - device->gmu_core.gmu2gpu_offset) << 2);

	if (WARN(!gmu_core_is_register_offset(device, offset),
		"Out of bounds register read: 0x%x\n", offset))
		return -EINVAL;

	if (readl_poll_timeout(addr, val, (val & mask) == expected_ret, 100,
		timeout_ms * 1000))
		return -ETIMEDOUT;

	return 0;
}

/**
 * a6xx_is_smmu_stalled() - Check whether smmu is stalled or not
 * @device: Pointer to KGSL device
 *
 * Return - True if smmu is stalled or false otherwise
 */
static inline bool a6xx_is_smmu_stalled(struct kgsl_device *device)
{
	u32 val;

	kgsl_regread(device, A6XX_RBBM_STATUS3, &val);

	return val & BIT(24);
}

/**
 * a6xx_cx_regulator_disable_wait - Disable a cx regulator and wait for it
 * @reg: A &struct regulator handle
 * @device: kgsl device struct
 * @timeout: Time to wait (in milliseconds)
 *
 * Disable the regulator and wait @timeout milliseconds for it to enter the
 * disabled state.
 */
void a6xx_cx_regulator_disable_wait(struct regulator *reg,
				struct kgsl_device *device, u32 timeout);

/* Preemption functions */
void a6xx_preemption_trigger(struct adreno_device *adreno_dev);
void a6xx_preemption_schedule(struct adreno_device *adreno_dev);
void a6xx_preemption_start(struct adreno_device *adreno_dev);
int a6xx_preemption_init(struct adreno_device *adreno_dev);

unsigned int a6xx_preemption_post_ibsubmit(struct adreno_device *adreno_dev,
		unsigned int *cmds);
unsigned int a6xx_preemption_pre_ibsubmit(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb,
		unsigned int *cmds, struct kgsl_context *context);

unsigned int a6xx_set_marker(unsigned int *cmds,
		enum adreno_cp_marker_type type);

void a6xx_preemption_callback(struct adreno_device *adreno_dev, int bit);

int a6xx_preemption_context_init(struct kgsl_context *context);

void a6xx_preemption_context_destroy(struct kgsl_context *context);

void a6xx_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);
void a6xx_crashdump_init(struct adreno_device *adreno_dev);
int a6xx_gmu_sptprac_enable(struct adreno_device *adreno_dev);
void a6xx_gmu_sptprac_disable(struct adreno_device *adreno_dev);
int a6xx_holi_gmu_sptprac_enable(struct adreno_device *adreno_dev);
void a6xx_holi_gmu_sptprac_disable(struct adreno_device *adreno_dev);

bool a6xx_gmu_sptprac_is_on(struct adreno_device *adreno_dev);
bool a6xx_holi_gmu_sptprac_is_on(struct adreno_device *adreno_dev);


/**
 * a6xx_read_alwayson - Read the current always on clock value
 * @adreno_dev: An Adreno GPU handle
 *
 * Return: The current value of the GMU always on counter
 */
u64 a6xx_read_alwayson(struct adreno_device *adreno_dev);

/**
 * a6xx_start - Program a6xx registers
 * @adreno_dev: An Adreno GPU handle
 *
 * This function does all a6xx register programming every
 * time we boot the gpu
 */
void a6xx_start(struct adreno_device *adreno_dev);

/**
 * a6xx_sqe_unhalt - Unhalt the SQE engine
 * @adreno_dev: An Adreno GPU handle
 *
 * Points the hardware to the microcode location in memory and then
 * unhalts the SQE so that it can fetch instructions from DDR
 */
void a6xx_unhalt_sqe(struct adreno_device *adreno_dev);

/**
 * a6xx_init - Initialize a6xx resources
 * @adreno_dev: An Adreno GPU handle
 *
 * This function does a6xx specific one time initialization
 * and is invoked when the very first client opens a
 * kgsl instance
 *
 * Return: Zero on success and negative error on failure
 */
int a6xx_init(struct adreno_device *adreno_dev);

/**
 * a6xx_rb_start - A6xx specific ringbuffer setup
 * @adreno_dev: An Adreno GPU handle
 *
 * This function does a6xx specific ringbuffer setup and
 * attempts to submit CP INIT and bring GPU out of secure mode
 *
 * Return: Zero on success and negative error on failure
 */
int a6xx_rb_start(struct adreno_device *adreno_dev);

/**
 * a6xx_microcode_read - Get the cp microcode from the filesystem
 * @adreno_dev: An Adreno GPU handle
 *
 * This function gets the firmware from filesystem and sets up
 * the micorocode global buffer
 *
 * Return: Zero on success and negative error on failure
 */
int a6xx_microcode_read(struct adreno_device *adreno_dev);

/**
 * a6xx_probe_common - Probe common a6xx resources
 * @pdev: Pointer to the platform device
 * @adreno_dev: Pointer to the adreno device
 * @chipid: Chipid of the target
 * @gpucore: Pointer to the gpucore strucure
 *
 * This function sets up the a6xx resources common across all
 * a6xx targets
 */
int a6xx_probe_common(struct platform_device *pdev,
	struct  adreno_device *adreno_dev, u32 chipid,
	const struct adreno_gpu_core *gpucore);

/**
 * a6xx_hw_isidle - Check whether a6xx gpu is idle or not
 * @adreno_dev: An Adreno GPU handle
 *
 * Return: True if gpu is idle, otherwise false
 */
bool a6xx_hw_isidle(struct adreno_device *adreno_dev);

/**
 * a6xx_spin_idle_debug - Debug logging used when gpu fails to idle
 * @adreno_dev: An Adreno GPU handle
 *
 * This function logs interesting registers and triggers a snapshot
 */
void a6xx_spin_idle_debug(struct adreno_device *adreno_dev,
	const char *str);

/**
 * a6xx_perfcounter_update - Update the IFPC perfcounter list
 * @adreno_dev: An Adreno GPU handle
 * @reg: Perfcounter reg struct to add/remove to the list
 * @update_reg: true if the perfcounter needs to be programmed by the CPU
 *
 * Return: 0 on success or -EBUSY if the lock couldn't be taken
 */
int a6xx_perfcounter_update(struct adreno_device *adreno_dev,
	struct adreno_perfcount_register *reg, bool update_reg);

extern const struct adreno_perfcounters adreno_a630_perfcounters;
extern const struct adreno_perfcounters adreno_a6xx_perfcounters;
extern const struct adreno_perfcounters adreno_a6xx_legacy_perfcounters;

/**
 * a6xx_rdpm_mx_freq_update - Update the mx frequency
 * @gmu: An Adreno GMU handle
 * @freq: Frequency in KHz
 *
 * This function communicates GPU mx frequency(in Mhz) changes to rdpm.
 */
void a6xx_rdpm_mx_freq_update(struct a6xx_gmu_device *gmu, u32 freq);

/**
 * a6xx_rdpm_cx_freq_update - Update the cx frequency
 * @gmu: An Adreno GMU handle
 * @freq: Frequency in KHz
 *
 * This function communicates GPU cx frequency(in Mhz) changes to rdpm.
 */
void a6xx_rdpm_cx_freq_update(struct a6xx_gmu_device *gmu, u32 freq);
#endif
