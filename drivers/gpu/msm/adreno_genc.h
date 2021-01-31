/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_GENC_H_
#define _ADRENO_GENC_H_

#include <linux/delay.h>

#include "genc_reg.h"
#include "adreno_genc_gmu.h"

/* Snapshot section size of each CP preemption record for GENC */
#define GENC_SNAPSHOT_CP_CTXRECORD_SIZE_IN_BYTES (64 * 1024)

extern const struct adreno_power_ops genc_gmu_power_ops;
extern const struct adreno_power_ops genc_hwsched_power_ops;
extern const struct adreno_perfcounters adreno_genc_perfcounters;

/**
 * struct genc_device - Container for the genc_device
 */
struct genc_device {
	/** @gmu: Container for the genc GMU device */
	struct genc_gmu_device gmu;
	/** @adreno_dev: Container for the generic adreno device */
	struct adreno_device adreno_dev;
};

/**
 * struct genc_protected_regs - container for a protect register span
 */
struct genc_protected_regs {
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
 * struct adreno_genc_core - genc specific GPU core definitions
 */
struct adreno_genc_core {
	/** @base: Container for the generic GPU definitions */
	struct adreno_gpu_core base;
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
	/** @gbif: List of registers and values to write for GBIF */
	const struct adreno_reglist *gbif;
	/** @gbif_count: Number of registers in @gbif */
	u32 gbif_count;
	/** @hang_detect_cycles: Hang detect counter timeout value */
	u32 hang_detect_cycles;
	/** @protected_regs: Array of protected registers for the target */
	const struct genc_protected_regs *protected_regs;
	/** @ctxt_record_size: Size of the preemption record in bytes */
	u64 ctxt_record_size;
	/** @highest_bank_bit: Highest bank bit value */
	u32 highest_bank_bit;
};

/**
 * struct genc_cp_preemption_record - CP context record for
 * preemption.
 * @magic: (00) Value at this offset must be equal to
 * GENC_CP_CTXRECORD_MAGIC_REF.
 * @info: (04) Type of record. Written non-zero (usually) by CP.
 * we must set to zero for all ringbuffers.
 * @errno: (08) Error code. Initialize this to GENC_CP_CTXRECORD_ERROR_NONE.
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
struct genc_cp_preemption_record {
	u32 magic;
	u32 info;
	u32 errno;
	u32 data;
	u32 cntl;
	u32 rptr;
	u32 wptr;
	u32 _pad28;
	u64 rptr_addr;
	u64 rbase;
	u64 counter;
};

/**
 * struct genc_cp_smmu_info - CP preemption SMMU info.
 * @magic: (00) The value at this offset must be equal to
 * GENC_CP_SMMU_INFO_MAGIC_REF
 * @_pad4: (04) Reserved/padding
 * @ttbr0: (08) Base address of the page table for the * incoming context
 * @asid: (16) Address Space IDentifier (ASID) of the incoming context
 * @context_idr: (20) Context Identification Register value
 * @context_bank: (24) Which Context Bank in SMMU to update
 */
struct genc_cp_smmu_info {
	u32 magic;
	u32 _pad4;
	u64 ttbr0;
	u32 asid;
	u32 context_idr;
	u32 context_bank;
};

#define GENC_CP_SMMU_INFO_MAGIC_REF		0x241350d5UL

#define GENC_CP_CTXRECORD_MAGIC_REF		0xae399d6eUL
/* Size of each CP preemption record */
#define GENC_CP_CTXRECORD_SIZE_IN_BYTES		(2112 * 1024)
/* Size of the user context record block (in bytes) */
#define GENC_CP_CTXRECORD_USER_RESTORE_SIZE	(192 * 1024)
/* Size of the performance counter save/restore block (in bytes) */
#define GENC_CP_PERFCOUNTER_SAVE_RESTORE_SIZE	(4 * 1024)

#define GENC_CP_RB_CNTL_DEFAULT \
	(FIELD_PREP(GENMASK(7, 0), ilog2(KGSL_RB_DWORDS >> 1)) | \
	 FIELD_PREP(GENMASK(12, 8), ilog2(4)))

/* Size of the CP_INIT pm4 stream in dwords */
#define GENC_CP_INIT_DWORDS 8

#define GENC_INT_MASK \
	((1 << GENC_INT_AHBERROR) |			\
	 (1 << GENC_INT_ATBASYNCFIFOOVERFLOW) |		\
	 (1 << GENC_INT_GPCERROR) |			\
	 (1 << GENC_INT_SWINTERRUPT) |			\
	 (1 << GENC_INT_HWERROR) |			\
	 (1 << GENC_INT_PM4CPINTERRUPT) |		\
	 (1 << GENC_INT_RB_DONE_TS) |			\
	 (1 << GENC_INT_CACHE_CLEAN_TS) |		\
	 (1 << GENC_INT_ATBBUSOVERFLOW) |		\
	 (1 << GENC_INT_HANGDETECTINTERRUPT) |		\
	 (1 << GENC_INT_OUTOFBOUNDACCESS) |		\
	 (1 << GENC_INT_UCHETRAPINTERRUPT) |		\
	 (1 << GENC_INT_TSBWRITEERROR))

#define GENC_HWSCHED_INT_MASK \
	((1 << GENC_INT_AHBERROR) |			\
	 (1 << GENC_INT_ATBASYNCFIFOOVERFLOW) |		\
	 (1 << GENC_INT_GPCERROR) |			\
	 (1 << GENC_INT_ATBBUSOVERFLOW) |		\
	 (1 << GENC_INT_OUTOFBOUNDACCESS) |		\
	 (1 << GENC_INT_UCHETRAPINTERRUPT))

/**
 * to_genc_core - return the genc specific GPU core struct
 * @adreno_dev: An Adreno GPU device handle
 *
 * Returns:
 * A pointer to the genc specific GPU core struct
 */
static inline const struct adreno_genc_core *
to_genc_core(struct adreno_device *adreno_dev)
{
	const struct adreno_gpu_core *core = adreno_dev->gpucore;

	return container_of(core, struct adreno_genc_core, base);
}

/**
 * genc_is_smmu_stalled() - Check whether smmu is stalled or not
 * @device: Pointer to KGSL device
 *
 * Return - True if smmu is stalled or false otherwise
 */
static inline bool genc_is_smmu_stalled(struct kgsl_device *device)
{
	u32 val;

	kgsl_regread(device, GENC_RBBM_STATUS3, &val);

	return val & BIT(24);
}

/**
 * genc_cx_regulator_disable_wait - Disable a cx regulator and wait for it
 * @reg: A &struct regulator handle
 * @device: kgsl device struct
 * @timeout: Time to wait (in milliseconds)
 *
 * Disable the regulator and wait @timeout milliseconds for it to enter the
 * disabled state.
 *
 * Return: True if the regulator was disabled or false if it timed out
 */
bool genc_cx_regulator_disable_wait(struct regulator *reg,
		struct kgsl_device *device, u32 timeout);

/* Preemption functions */
void genc_preemption_trigger(struct adreno_device *adreno_dev);
void genc_preemption_schedule(struct adreno_device *adreno_dev);
void genc_preemption_start(struct adreno_device *adreno_dev);
int genc_preemption_init(struct adreno_device *adreno_dev);

u32 genc_preemption_post_ibsubmit(struct adreno_device *adreno_dev,
		unsigned int *cmds);
u32 genc_preemption_pre_ibsubmit(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, struct adreno_context *drawctxt,
		u32 *cmds);

unsigned int genc_set_marker(unsigned int *cmds,
		enum adreno_cp_marker_type type);

void genc_preemption_callback(struct adreno_device *adreno_dev, int bit);

int genc_preemption_context_init(struct kgsl_context *context);

void genc_preemption_context_destroy(struct kgsl_context *context);

void genc_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot);
void genc_crashdump_init(struct adreno_device *adreno_dev);

/**
 * genc_read_alwayson - Read the current always on clock value
 * @adreno_dev: An Adreno GPU handle
 *
 * Return: The current value of the GMU always on counter
 */
u64 genc_read_alwayson(struct adreno_device *adreno_dev);

/**
 * genc_start - Program genc registers
 * @adreno_dev: An Adreno GPU handle
 *
 * This function does all genc register programming every
 * time we boot the gpu
 *
 * Return: 0 on success or negative on failure
 */
int genc_start(struct adreno_device *adreno_dev);

/**
 * genc_sqe_unhalt - Unhalt the SQE engine
 * @adreno_dev: An Adreno GPU handle
 *
 * Points the hardware to the microcode location in memory and then
 * unhalts the SQE so that it can fetch instructions from DDR
 */
void genc_unhalt_sqe(struct adreno_device *adreno_dev);

/**
 * genc_init - Initialize genc resources
 * @adreno_dev: An Adreno GPU handle
 *
 * This function does genc specific one time initialization
 * and is invoked when the very first client opens a
 * kgsl instance
 *
 * Return: Zero on success and negative error on failure
 */
int genc_init(struct adreno_device *adreno_dev);

/**
 * genc_rb_start - GenC specific ringbuffer setup
 * @adreno_dev: An Adreno GPU handle
 *
 * This function does genc specific ringbuffer setup and
 * attempts to submit CP INIT and bring GPU out of secure mode
 *
 * Return: Zero on success and negative error on failure
 */
int genc_rb_start(struct adreno_device *adreno_dev);

/**
 * genc_microcode_read - Get the cp microcode from the filesystem
 * @adreno_dev: An Adreno GPU handle
 *
 * This function gets the firmware from filesystem and sets up
 * the micorocode global buffer
 *
 * Return: Zero on success and negative error on failure
 */
int genc_microcode_read(struct adreno_device *adreno_dev);

/**
 * genc_probe_common - Probe common genc resources
 * @pdev: Pointer to the platform device
 * @adreno_dev: Pointer to the adreno device
 * @chipid: Chipid of the target
 * @gpucore: Pointer to the gpucore strucure
 *
 * This function sets up the genc resources common across all
 * genc targets
 */
int genc_probe_common(struct platform_device *pdev,
	struct adreno_device *adreno_dev, u32 chipid,
	const struct adreno_gpu_core *gpucore);

/**
 * genc_hw_isidle - Check whether genc gpu is idle or not
 * @adreno_dev: An Adreno GPU handle
 *
 * Return: True if gpu is idle, otherwise false
 */
bool genc_hw_isidle(struct adreno_device *adreno_dev);

/**
 * genc_spin_idle_debug - Debug logging used when gpu fails to idle
 * @adreno_dev: An Adreno GPU handle
 *
 * This function logs interesting registers and triggers a snapshot
 */
void genc_spin_idle_debug(struct adreno_device *adreno_dev,
	const char *str);

/**
 * genc_perfcounter_update - Update the IFPC perfcounter list
 * @adreno_dev: An Adreno GPU handle
 * @reg: Perfcounter reg struct to add/remove to the list
 * @update_reg: true if the perfcounter needs to be programmed by the CPU
 *
 * Return: 0 on success or -EBUSY if the lock couldn't be taken
 */
int genc_perfcounter_update(struct adreno_device *adreno_dev,
	struct adreno_perfcount_register *reg, bool update_reg);

/*
 * genc_ringbuffer_init - Initialize the ringbuffers
 * @adreno_dev: An Adreno GPU handle
 *
 * Initialize the ringbuffer(s) for a5xx.
 * Return: 0 on success or negative on failure
 */
int genc_ringbuffer_init(struct adreno_device *adreno_dev);

/**
 * genc_ringbuffer_submitcmd - Submit a user command to the ringbuffer
 * @adreno_dev: An Adreno GPU handle
 * @cmdobj: Pointer to a user command object
 * @flags: Internal submit flags
 * @time: Optional pointer to a adreno_submit_time container
 *
 * Return: 0 on success or negative on failure
 */
int genc_ringbuffer_submitcmd(struct adreno_device *adreno_dev,
		struct kgsl_drawobj_cmd *cmdobj, u32 flags,
		struct adreno_submit_time *time);

/**
 * genc_ringbuffer_submit - Submit a command to the ringbuffer
 * @rb: Ringbuffer pointer
 * @time: Optional pointer to a adreno_submit_time container
 *
 * Return: 0 on success or negative on failure
 */
int genc_ringbuffer_submit(struct adreno_ringbuffer *rb,
		struct adreno_submit_time *time);

/**
 * genc_fenced_write - Write to a fenced register
 * @adreno_dev: An Adreno GPU handle
 * @offset: Register offset
 * @value: Value to write
 * @mask: Expected FENCE_STATUS for successful write
 *
 * Return: 0 on success or negative on failure
 */
int genc_fenced_write(struct adreno_device *adreno_dev, u32 offset,
		u32 value, u32 mask);

/**
 * genc_ringbuffer_addcmds - Wrap and submit commands to the ringbuffer
 * @adreno_dev: An Adreno GPU handle
 * @rb: Ringbuffer pointer
 * @drawctxt: Draw context submitting the commands
 * @flags: Submission flags
 * @in: Input buffer to write to ringbuffer
 * @dwords: Dword length of @in
 * @timestamp: Draw context timestamp for the submission
 * @time: Optional pointer to a adreno_submit_time container
 *
 * Return: 0 on success or negative on failure
 */
int genc_ringbuffer_addcmds(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, struct adreno_context *drawctxt,
		u32 flags, u32 *in, u32 dwords, u32 timestamp,
		struct adreno_submit_time *time);

/**
 * genc_cp_init_cmds - Create the CP_INIT commands
 * @adreno_dev: An Adreno GPU handle
 * @cmd: Buffer to write the CP_INIT commands into
 */
void genc_cp_init_cmds(struct adreno_device *adreno_dev, u32 *cmds);

#endif
