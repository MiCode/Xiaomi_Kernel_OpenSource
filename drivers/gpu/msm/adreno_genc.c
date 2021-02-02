// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/qcom/llcc-qcom.h>

#include "adreno.h"
#include "adreno_genc.h"
#include "adreno_genc_hwsched.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

/* IFPC & Preemption static powerup restore list */
static const u32 genc_pwrup_reglist[] = {
	GENC_UCHE_TRAP_BASE_LO,
	GENC_UCHE_TRAP_BASE_HI,
	GENC_UCHE_WRITE_THRU_BASE_LO,
	GENC_UCHE_WRITE_THRU_BASE_HI,
	GENC_UCHE_GMEM_RANGE_MIN_LO,
	GENC_UCHE_GMEM_RANGE_MIN_HI,
	GENC_UCHE_GMEM_RANGE_MAX_LO,
	GENC_UCHE_GMEM_RANGE_MAX_HI,
	GENC_UCHE_CACHE_WAYS,
	GENC_UCHE_MODE_CNTL,
	GENC_RB_NC_MODE_CNTL,
	GENC_TPL1_NC_MODE_CNTL,
	GENC_SP_NC_MODE_CNTL,
	GENC_GRAS_NC_MODE_CNTL,
	GENC_RB_CONTEXT_SWITCH_GMEM_SAVE_RESTORE,
	GENC_TPL1_BICUBIC_WEIGHTS_TABLE_0,
	GENC_TPL1_BICUBIC_WEIGHTS_TABLE_1,
	GENC_TPL1_BICUBIC_WEIGHTS_TABLE_2,
	GENC_TPL1_BICUBIC_WEIGHTS_TABLE_3,
	GENC_TPL1_BICUBIC_WEIGHTS_TABLE_4,
	GENC_UCHE_GBIF_GX_CONFIG,
	GENC_RBBM_GBIF_CLIENT_QOS_CNTL,
};

/* IFPC only static powerup restore list */
static const u32 genc_ifpc_pwrup_reglist[] = {
	GENC_CP_CHICKEN_DBG,
	GENC_CP_DBG_ECO_CNTL,
	GENC_CP_PROTECT_CNTL,
	GENC_CP_PROTECT_REG,
	GENC_CP_PROTECT_REG+1,
	GENC_CP_PROTECT_REG+2,
	GENC_CP_PROTECT_REG+3,
	GENC_CP_PROTECT_REG+4,
	GENC_CP_PROTECT_REG+5,
	GENC_CP_PROTECT_REG+6,
	GENC_CP_PROTECT_REG+7,
	GENC_CP_PROTECT_REG+8,
	GENC_CP_PROTECT_REG+9,
	GENC_CP_PROTECT_REG+10,
	GENC_CP_PROTECT_REG+11,
	GENC_CP_PROTECT_REG+12,
	GENC_CP_PROTECT_REG+13,
	GENC_CP_PROTECT_REG+14,
	GENC_CP_PROTECT_REG+15,
	GENC_CP_PROTECT_REG+16,
	GENC_CP_PROTECT_REG+17,
	GENC_CP_PROTECT_REG+18,
	GENC_CP_PROTECT_REG+19,
	GENC_CP_PROTECT_REG+20,
	GENC_CP_PROTECT_REG+21,
	GENC_CP_PROTECT_REG+22,
	GENC_CP_PROTECT_REG+23,
	GENC_CP_PROTECT_REG+24,
	GENC_CP_PROTECT_REG+25,
	GENC_CP_PROTECT_REG+26,
	GENC_CP_PROTECT_REG+27,
	GENC_CP_PROTECT_REG+28,
	GENC_CP_PROTECT_REG+29,
	GENC_CP_PROTECT_REG+30,
	GENC_CP_PROTECT_REG+31,
	GENC_CP_PROTECT_REG+32,
	GENC_CP_PROTECT_REG+33,
	GENC_CP_PROTECT_REG+47,
	GENC_CP_AHB_CNTL,
};

void genc_cp_init_cmds(struct adreno_device *adreno_dev, u32 *cmds)
{
	u32 i = 0, mask = 0;

	/* Use multiple HW contexts */
	mask |= BIT(0);

	/* Enable error detection */
	mask |= BIT(1);

	/* Set default reset state */
	mask |= BIT(3);

	/* Disable save/restore of performance counters across preemption */
	mask |= BIT(6);

	/* Enable the register init list with the spinlock */
	mask |= BIT(8);

	cmds[i++] = cp_type7_packet(CP_ME_INIT, 7);

	/* Enabled ordinal mask */
	cmds[i++] = mask;
	cmds[i++] = 0x00000003; /* Set number of HW contexts */
	cmds[i++] = 0x20000000; /* Enable error detection */
	cmds[i++] = 0x00000002; /* Operation mode mask */

	/* Register initialization list with spinlock */
	cmds[i++] = lower_32_bits(adreno_dev->pwrup_reglist->gpuaddr);
	cmds[i++] = upper_32_bits(adreno_dev->pwrup_reglist->gpuaddr);
	cmds[i++] = 0;
}

int genc_fenced_write(struct adreno_device *adreno_dev, u32 offset,
		u32 value, u32 mask)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status, i;

	kgsl_regwrite(device, offset, value);

	for (i = 0; i < GMU_CORE_LONG_WAKEUP_RETRY_LIMIT; i++) {
		/*
		 * Make sure the previous register write is posted before
		 * checking the fence status
		 */
		mb();

		gmu_core_regread(device, GENC_GMU_AHB_FENCE_STATUS, &status);

		/*
		 * If !writedropped0/1, then the write to fenced register
		 * was successful
		 */
		if (!(status & mask))
			break;

		/* Wait a small amount of time before trying again */
		udelay(GMU_CORE_WAKEUP_DELAY_US);

		/* Try to write the fenced register again */
		kgsl_regwrite(device, offset, value);
	}

	if (i < GMU_CORE_SHORT_WAKEUP_RETRY_LIMIT)
		return 0;

	dev_err(adreno_dev->dev.dev,
			"Timed out waiting %d usecs to write fenced register 0x%x\n",
			i * GMU_CORE_WAKEUP_DELAY_US, offset);
	return -ETIMEDOUT;
}

int genc_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_genc_core *genc_core = to_genc_core(adreno_dev);
	int ret;

	adreno_dev->highest_bank_bit = genc_core->highest_bank_bit;
	adreno_dev->cooperative_reset = ADRENO_FEATURE(adreno_dev,
			ADRENO_COOP_RESET);

	genc_crashdump_init(adreno_dev);

	ret = adreno_allocate_global(device, &adreno_dev->pwrup_reglist,
		PAGE_SIZE, 0, 0, KGSL_MEMDESC_PRIVILEGED,
		"powerup_register_list");
	if (ret)
		return ret;

	adreno_create_profile_buffer(adreno_dev);

	return 0;
}

static void genc_protect_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_genc_core *genc_core = to_genc_core(adreno_dev);
	const struct genc_protected_regs *regs = genc_core->protected_regs;
	int i;

	/*
	 * Enable access protection to privileged registers, fault on an access
	 * protect violation and select the last span to protect from the start
	 * address all the way to the end of the register address space
	 */
	kgsl_regwrite(device, GENC_CP_PROTECT_CNTL,
		BIT(0) | BIT(1) | BIT(3));

	/* Program each register defined by the core definition */
	for (i = 0; regs[i].reg; i++) {
		u32 count;

		/*
		 * This is the offset of the end register as counted from the
		 * start, i.e. # of registers in the range - 1
		 */
		count = regs[i].end - regs[i].start;

		kgsl_regwrite(device, regs[i].reg,
				FIELD_PREP(GENMASK(17, 0), regs[i].start) |
				FIELD_PREP(GENMASK(30, 18), count) |
				FIELD_PREP(BIT(31), regs[i].noaccess));
	}
}

bool genc_cx_regulator_disable_wait(struct regulator *reg,
				struct kgsl_device *device, u32 timeout)
{
	ktime_t tout = ktime_add_us(ktime_get(), timeout * 1000);
	unsigned int val;

	if (IS_ERR_OR_NULL(reg))
		return true;

	regulator_disable(reg);

	for (;;) {
		gmu_core_regread(device, GENC_GPU_CC_CX_GDSCR, &val);

		if (!(val & BIT(31)))
			return true;

		if (ktime_compare(ktime_get(), tout) > 0) {
			gmu_core_regread(device, GENC_GPU_CC_CX_GDSCR, &val);
			return (!(val & BIT(31)));
		}

		usleep_range(26, 100);
	}
}

#define RBBM_CLOCK_CNTL_ON 0x8aa8aa82
#define GMU_AO_CGC_MODE_CNTL 0x00020000
#define GMU_AO_CGC_DELAY_CNTL 0x00010111
#define GMU_AO_CGC_HYST_CNTL 0x00005555

static void genc_hwcg_set(struct adreno_device *adreno_dev, bool on)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_genc_core *genc_core = to_genc_core(adreno_dev);
	unsigned int value;
	int i;

	if (!adreno_dev->hwcg_enabled)
		on = false;

	gmu_core_regwrite(device, GENC_GPU_GMU_AO_GMU_CGC_MODE_CNTL,
			on ? GMU_AO_CGC_MODE_CNTL : 0);
	gmu_core_regwrite(device, GENC_GPU_GMU_AO_GMU_CGC_DELAY_CNTL,
			on ? GMU_AO_CGC_DELAY_CNTL : 0);
	gmu_core_regwrite(device, GENC_GPU_GMU_AO_GMU_CGC_HYST_CNTL,
			on ? GMU_AO_CGC_HYST_CNTL : 0);

	kgsl_regread(device, GENC_RBBM_CLOCK_CNTL, &value);

	if (value == RBBM_CLOCK_CNTL_ON && on)
		return;

	if (value == 0 && !on)
		return;

	for (i = 0; i < genc_core->hwcg_count; i++)
		kgsl_regwrite(device, genc_core->hwcg[i].offset,
			on ? genc_core->hwcg[i].value : 0);

	/* enable top level HWCG */
	kgsl_regwrite(device, GENC_RBBM_CLOCK_CNTL,
		on ? RBBM_CLOCK_CNTL_ON : 0);
}

static void genc_patch_pwrup_reglist(struct adreno_device *adreno_dev)
{
	struct adreno_reglist_list reglist[2];
	void *ptr = adreno_dev->pwrup_reglist->hostptr;
	struct cpu_gpu_lock *lock = ptr;
	int i, j;
	u32 *dest = ptr + sizeof(*lock);

	/* Static IFPC-only registers */
	reglist[0].regs = genc_ifpc_pwrup_reglist;
	reglist[0].count = ARRAY_SIZE(genc_ifpc_pwrup_reglist);

	/* Static IFPC + preemption registers */
	reglist[1].regs = genc_pwrup_reglist;
	reglist[1].count = ARRAY_SIZE(genc_pwrup_reglist);

	/*
	 * For each entry in each of the lists, write the offset and the current
	 * register value into the GPU buffer
	 */
	for (i = 0; i < 2; i++) {
		const u32 *r = reglist[i].regs;

		for (j = 0; j < reglist[i].count; j++) {
			*dest++ = r[j];
			kgsl_regread(KGSL_DEVICE(adreno_dev), r[j], dest++);
		}

		lock->list_length += reglist[i].count * 2;
	}

	/* This needs to be at the end of the list */
	*dest++ = GENC_RBBM_PERFCTR_CNTL;
	*dest++ = 1;
	lock->list_length += 2;

	/*
	 * The overall register list is composed of
	 * 1. Static IFPC-only registers
	 * 2. Static IFPC + preemption registers
	 * 3. Dynamic IFPC + preemption registers (ex: perfcounter selects)
	 *
	 * The CP views the second and third entries as one dynamic list
	 * starting from list_offset. list_length should be the total dwords in
	 * all the lists and list_offset should be specified as the size in
	 * dwords of the first entry in the list.
	 */
	lock->list_offset = reglist[0].count * 2;
}

/* _llc_configure_gpu_scid() - Program the sub-cache ID for all GPU blocks */
static void _llc_configure_gpu_scid(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 gpu_scid;

	if (IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice) ||
		!adreno_dev->gpu_llc_slice_enable)
		return;

	if (llcc_slice_activate(adreno_dev->gpu_llc_slice))
		return;

	gpu_scid = llcc_get_slice_id(adreno_dev->gpu_llc_slice);

	/* 6 blocks at 5 bits per block */
	kgsl_regwrite(device, GENC_GBIF_SCACHE_CNTL1,
			FIELD_PREP(GENMASK(29, 25), gpu_scid) |
			FIELD_PREP(GENMASK(24, 20), gpu_scid) |
			FIELD_PREP(GENMASK(19, 15), gpu_scid) |
			FIELD_PREP(GENMASK(14, 10), gpu_scid) |
			FIELD_PREP(GENMASK(9, 5), gpu_scid) |
			FIELD_PREP(GENMASK(4, 0), gpu_scid));

	kgsl_regwrite(device, GENC_GBIF_SCACHE_CNTL0,
			FIELD_PREP(GENMASK(14, 10), gpu_scid));
}

static void _set_secvid(struct kgsl_device *device)
{
	kgsl_regwrite(device, GENC_RBBM_SECVID_TSB_CNTL, 0x0);
	kgsl_regwrite(device, GENC_RBBM_SECVID_TSB_TRUSTED_BASE_LO,
		lower_32_bits(KGSL_IOMMU_SECURE_BASE(&device->mmu)));
	kgsl_regwrite(device, GENC_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
		upper_32_bits(KGSL_IOMMU_SECURE_BASE(&device->mmu)));
	kgsl_regwrite(device, GENC_RBBM_SECVID_TSB_TRUSTED_SIZE,
		KGSL_IOMMU_SECURE_SIZE);
}

/*
 * All GenC targets support marking certain transactions as always privileged
 * which allows us to mark more memory as privileged without having to
 * explicitly set the APRIV bit. Choose the following transactions to be
 * privileged by default:
 * CDWRITE     [6:6] - Crashdumper writes
 * CDREAD      [5:5] - Crashdumper reads
 * RBRPWB      [3:3] - RPTR shadow writes
 * RBPRIVLEVEL [2:2] - Memory accesses from PM4 packets in the ringbuffer
 * RBFETCH     [1:1] - Ringbuffer reads
 * ICACHE      [0:0] - Instruction cache fetches
 */

#define GENC_APRIV_DEFAULT (BIT(3) | BIT(2) | BIT(1) | BIT(0))
/* Add crashdumper permissions for the BR APRIV */
#define GENC_BR_APRIV_DEFAULT (GENC_APRIV_DEFAULT | BIT(6) | BIT(5))

int genc_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_genc_core *genc_core = to_genc_core(adreno_dev);
	static bool patch_reglist;

	/* Set up GBIF registers from the GPU core definition */
	adreno_reglist_write(adreno_dev, genc_core->gbif,
		genc_core->gbif_count);

	kgsl_regwrite(device, GENC_UCHE_GBIF_GX_CONFIG, 0x10240e0);

	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, GENC_RBBM_PERFCTR_GPU_BUSY_MASKED, 0xffffffff);

	/*
	 * Set UCHE_WRITE_THRU_BASE to the UCHE_TRAP_BASE effectively
	 * disabling L2 bypass
	 */
	kgsl_regwrite(device, GENC_UCHE_TRAP_BASE_LO, 0xfffff000);
	kgsl_regwrite(device, GENC_UCHE_TRAP_BASE_HI, 0x0001ffff);
	kgsl_regwrite(device, GENC_UCHE_WRITE_THRU_BASE_LO, 0xfffff000);
	kgsl_regwrite(device, GENC_UCHE_WRITE_THRU_BASE_HI, 0x0001ffff);

	kgsl_regwrite(device, GENC_UCHE_CACHE_WAYS, 0x800000);

	kgsl_regwrite(device, GENC_UCHE_CMDQ_CONFIG,
			FIELD_PREP(GENMASK(19, 16), 6) |
			FIELD_PREP(GENMASK(15, 12), 6) |
			FIELD_PREP(GENMASK(11, 8), 9) |
			BIT(3) | BIT(2) |
			FIELD_PREP(GENMASK(1, 0), 2));

	/* Set the AHB default slave response to "ERROR" */
	kgsl_regwrite(device, GENC_CP_AHB_CNTL, 0x1);

	/* Turn on performance counters */
	kgsl_regwrite(device, GENC_RBBM_PERFCTR_CNTL, 0x1);

	/* Turn on the IFPC counter (countable 4 on XOCLK4) */
	kgsl_regwrite(device, GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_1,
			FIELD_PREP(GENMASK(7, 0), 0x4));

	kgsl_regwrite(device, GENC_RB_NC_MODE_CNTL, BIT(11) | BIT(4) |
			FIELD_PREP(GENMASK(2, 1), 3));
	kgsl_regwrite(device, GENC_TPL1_NC_MODE_CNTL,
			FIELD_PREP(GENMASK(2, 1), 3));
	kgsl_regwrite(device, GENC_SP_NC_MODE_CNTL,
			FIELD_PREP(GENMASK(5, 4), 2) |
			FIELD_PREP(GENMASK(2, 1), 3));
	kgsl_regwrite(device, GENC_GRAS_NC_MODE_CNTL,
			FIELD_PREP(GENMASK(8, 5), 3));

	kgsl_regwrite(device, GENC_UCHE_MODE_CNTL,
			FIELD_PREP(GENMASK(22, 21), 3));
	kgsl_regwrite(device, GENC_RBBM_INTERFACE_HANG_INT_CNTL, BIT(30) |
			FIELD_PREP(GENMASK(27, 0),
				genc_core->hang_detect_cycles));
	kgsl_regwrite(device, GENC_UCHE_CLIENT_PF, BIT(0));

	/* Set weights for bicubic filtering */
	kgsl_regwrite(device, GENC_TPL1_BICUBIC_WEIGHTS_TABLE_0, 0x0);
	kgsl_regwrite(device, GENC_TPL1_BICUBIC_WEIGHTS_TABLE_1, 0x3fe05ff4);
	kgsl_regwrite(device, GENC_TPL1_BICUBIC_WEIGHTS_TABLE_2, 0x3fa0ebee);
	kgsl_regwrite(device, GENC_TPL1_BICUBIC_WEIGHTS_TABLE_3, 0x3f5193ed);
	kgsl_regwrite(device, GENC_TPL1_BICUBIC_WEIGHTS_TABLE_4, 0x3f0243f0);

	/* Enable the GMEM save/restore feature for preemption */
	if (adreno_is_preemption_enabled(adreno_dev))
		kgsl_regwrite(device, GENC_RB_CONTEXT_SWITCH_GMEM_SAVE_RESTORE,
			0x1);

	/* Enable GMU power counter 0 to count GPU busy */
	kgsl_regwrite(device, GENC_GPU_GMU_AO_GPU_CX_BUSY_MASK, 0xff000000);
	kgsl_regwrite(device, GENC_GMU_CX_GMU_POWER_COUNTER_SELECT_0, 0x20);
	kgsl_regwrite(device, GENC_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0x1);

	genc_protect_init(adreno_dev);

	/* Configure LLCC */
	_llc_configure_gpu_scid(adreno_dev);

	kgsl_regwrite(device, GENC_CP_APRIV_CNTL, GENC_BR_APRIV_DEFAULT);
	kgsl_regwrite(device, GENC_CP_BV_APRIV_CNTL, GENC_APRIV_DEFAULT);
	kgsl_regwrite(device, GENC_CP_LPAC_APRIV_CNTL, GENC_APRIV_DEFAULT);

	_set_secvid(device);

	/*
	 * Enable hardware clock gating here to prevent any register access
	 * issue due to internal clock gating.
	 */
	genc_hwcg_set(adreno_dev, true);

	/*
	 * All registers must be written before this point so that we don't
	 * miss any register programming when we patch the power up register
	 * list.
	 */
	if (!patch_reglist && (adreno_dev->pwrup_reglist->gpuaddr != 0)) {
		genc_patch_pwrup_reglist(adreno_dev);
		patch_reglist = true;
	}

	return 0;
}

void genc_unhalt_sqe(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);

	/* Program the ucode base for CP */
	kgsl_regwrite(device, GENC_CP_SQE_INSTR_BASE_LO,
				lower_32_bits(fw->memdesc->gpuaddr));
	kgsl_regwrite(device, GENC_CP_SQE_INSTR_BASE_HI,
				upper_32_bits(fw->memdesc->gpuaddr));

	/* Clear the SQE_HALT to start the CP engine */
	kgsl_regwrite(device, GENC_CP_SQE_CNTL, 1);
}

void genc_spin_idle_debug(struct adreno_device *adreno_dev,
				const char *str)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int rptr, wptr;
	unsigned int status, status3, intstatus;
	unsigned int hwfault;

	dev_err(device->dev, str);

	kgsl_regread(device, GENC_CP_RB_RPTR, &rptr);
	kgsl_regread(device, GENC_CP_RB_WPTR, &wptr);

	kgsl_regread(device, GENC_RBBM_STATUS, &status);
	kgsl_regread(device, GENC_RBBM_STATUS3, &status3);
	kgsl_regread(device, GENC_RBBM_INT_0_STATUS, &intstatus);
	kgsl_regread(device, GENC_CP_HW_FAULT, &hwfault);

	dev_err(device->dev,
		"rb=%d pos=%X/%X rbbm_status=%8.8X/%8.8X int_0_status=%8.8X\n",
		adreno_dev->cur_rb->id, rptr, wptr, status, status3, intstatus);

	dev_err(device->dev, " hwfault=%8.8X\n", hwfault);

	kgsl_device_snapshot(device, NULL, false);
}

/*
 * genc_send_cp_init() - Initialize ringbuffer
 * @adreno_dev: Pointer to adreno device
 * @rb: Pointer to the ringbuffer of device
 *
 * Submit commands for ME initialization,
 */
static int genc_send_cp_init(struct adreno_device *adreno_dev,
			 struct adreno_ringbuffer *rb)
{
	unsigned int *cmds;
	int ret;

	cmds = adreno_ringbuffer_allocspace(rb, GENC_CP_INIT_DWORDS);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	genc_cp_init_cmds(adreno_dev, cmds);

	ret = genc_ringbuffer_submit(rb, NULL);
	if (!ret) {
		ret = adreno_spin_idle(adreno_dev, 2000);
		if (ret) {
			genc_spin_idle_debug(adreno_dev,
				"CP initialization failed to idle\n");
			rb->wptr = 0;
			rb->_wptr = 0;
		}
	}

	return ret;
}

static int genc_post_start(struct adreno_device *adreno_dev)
{
	int ret;
	unsigned int *cmds;
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	cmds = adreno_ringbuffer_allocspace(rb, 12);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	cmds[0] = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 6);
	cmds[1] = 1;
	cmds[2] = lower_32_bits(rb->preemption_desc->gpuaddr);
	cmds[3] = upper_32_bits(rb->preemption_desc->gpuaddr);

	cmds[4] = 2;
	cmds[5] = lower_32_bits(rb->secure_preemption_desc->gpuaddr);
	cmds[6] = upper_32_bits(rb->secure_preemption_desc->gpuaddr);

	cmds[7] = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	cmds[8] = 0;
	cmds[9] = 0;
	cmds[10] = 0;
	/* generate interrupt on preemption completion */
	cmds[11] = 0;

	ret = genc_ringbuffer_submit(rb, NULL);
	if (!ret) {
		ret = adreno_spin_idle(adreno_dev, 2000);
		if (ret)
			genc_spin_idle_debug(adreno_dev,
				"hw preemption initialization failed to idle\n");
	}

	return ret;
}

int genc_rb_start(struct adreno_device *adreno_dev)
{
	const struct adreno_genc_core *genc_core = to_genc_core(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	u64 addr;
	int ret, i;
	unsigned int *cmds;

	/* Clear all the ringbuffers */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		memset(rb->buffer_desc->hostptr, 0xaa, KGSL_RB_SIZE);
		kgsl_sharedmem_writel(device->scratch,
			SCRATCH_RPTR_OFFSET(rb->id), 0);
		kgsl_sharedmem_writel(device->scratch,
			SCRATCH_BV_RPTR_OFFSET(rb->id), 0);

		rb->wptr = 0;
		rb->_wptr = 0;
		rb->wptr_preempt_end = UINT_MAX;
	}

	genc_preemption_start(adreno_dev);

	/* Set up the current ringbuffer */
	rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);

	addr = SCRATCH_RPTR_GPU_ADDR(device, rb->id);
	kgsl_regwrite(device, GENC_CP_RB_RPTR_ADDR_LO, lower_32_bits(addr));
	kgsl_regwrite(device, GENC_CP_RB_RPTR_ADDR_HI, upper_32_bits(addr));

	addr = SCRATCH_BV_RPTR_GPU_ADDR(device, rb->id);
	kgsl_regwrite(device, GENC_CP_BV_RB_RPTR_ADDR_LO, lower_32_bits(addr));
	kgsl_regwrite(device, GENC_CP_BV_RB_RPTR_ADDR_HI, upper_32_bits(addr));

	kgsl_regwrite(device, GENC_CP_RB_CNTL, GENC_CP_RB_CNTL_DEFAULT);

	kgsl_regwrite(device, GENC_CP_RB_BASE,
		lower_32_bits(rb->buffer_desc->gpuaddr));
	kgsl_regwrite(device, GENC_CP_RB_BASE_HI,
		upper_32_bits(rb->buffer_desc->gpuaddr));

	genc_unhalt_sqe(adreno_dev);

	ret = genc_send_cp_init(adreno_dev, rb);
	if (ret)
		return ret;

	ret = adreno_zap_shader_load(adreno_dev, genc_core->zap_name);
	if (ret)
		return ret;

	/*
	 * Take the GPU out of secure mode. Try the zap shader if it is loaded,
	 * otherwise just try to write directly to the secure control register
	 */
	if (!adreno_dev->zap_loaded)
		kgsl_regwrite(device, GENC_RBBM_SECVID_TRUST_CNTL, 0);
	else {
		cmds = adreno_ringbuffer_allocspace(rb, 2);
		if (IS_ERR(cmds))
			return PTR_ERR(cmds);

		*cmds++ = cp_type7_packet(CP_SET_SECURE_MODE, 1);
		*cmds++ = 0;

		ret = genc_ringbuffer_submit(rb, NULL);
		if (!ret) {
			ret = adreno_spin_idle(adreno_dev, 2000);
			if (ret) {
				genc_spin_idle_debug(adreno_dev,
					"Switch to unsecure failed to idle\n");
				return ret;
			}
		}
	}

	return genc_post_start(adreno_dev);
}

/*
 * genc_gpu_keepalive() - GMU reg write to request GPU stays on
 * @adreno_dev: Pointer to the adreno device that has the GMU
 * @state: State to set: true is ON, false is OFF
 */
static void genc_gpu_keepalive(struct adreno_device *adreno_dev,
		bool state)
{
	gmu_core_regwrite(KGSL_DEVICE(adreno_dev),
			GENC_GMU_GMU_PWR_COL_KEEPALIVE, state);
}

bool genc_hw_isidle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg;

	gmu_core_regread(device, GENC_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &reg);

	/* Bit 23 is GPUBUSYIGNAHB */
	return (reg & BIT(23)) ? false : true;
}

int genc_microcode_read(struct adreno_device *adreno_dev)
{
	struct adreno_firmware *sqe_fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
	const struct adreno_genc_core *genc_core = to_genc_core(adreno_dev);

	return adreno_get_firmware(adreno_dev, genc_core->sqefw_name, sqe_fw);
}

/* CP Interrupt bits */
#define CP_INT_OPCODEERROR 0
#define CP_INT_UCODEERROR 1
#define CP_INT_CPHWFAULT 2
#define CP_INT_REGISTERPROTECTION 4
#define CP_INT_VSDPARITYERROR 6
#define CP_INT_ILLEGALINSTRUCTION 7
#define CP_INT_OPCODEERRORLPAC 8
#define CP_INT_UCODEERRORLPAC 9
#define CP_INT_CPHWFAULTLPAC 10
#define CP_INT_REGISTERPROTECTIONLPAC 11
#define CP_INT_ILLEGALINSTRUCTIONLPAC 12
#define CP_INT_OPCODEERRORBV 13
#define CP_INT_UCODEERRORBV 14
#define CP_INT_CPHWFAULTBV 15
#define CP_INT_REGISTERPROTECTIONBV 16
#define CP_INT_ILLEGALINSTRUCTIONBV 17

static void genc_cp_hw_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status1, status2;
	struct device *dev = device->dev;

	kgsl_regread(device, GENC_CP_INTERRUPT_STATUS, &status1);

	if (status1 & BIT(CP_INT_OPCODEERROR)) {
		unsigned int opcode;

		kgsl_regwrite(device, GENC_CP_SQE_STAT_ADDR, 1);
		kgsl_regread(device, GENC_CP_SQE_STAT_DATA, &opcode);
		dev_crit_ratelimited(dev,
			"CP opcode error interrupt | opcode=0x%8.8x\n", opcode);
	}

	if (status1 & BIT(CP_INT_UCODEERROR))
		dev_crit_ratelimited(dev, "CP ucode error interrupt\n");

	if (status1 & BIT(CP_INT_CPHWFAULT)) {
		kgsl_regread(device, GENC_CP_HW_FAULT, &status2);
		dev_crit_ratelimited(dev,
			"CP | Ringbuffer HW fault | status=%x\n", status2);
	}

	if (status1 & BIT(CP_INT_REGISTERPROTECTION)) {
		kgsl_regread(device, GENC_CP_PROTECT_STATUS, &status2);
		dev_crit_ratelimited(dev,
			"CP | Protected mode error | %s | addr=%x | status=%x\n",
			status2 & BIT(20) ? "READ" : "WRITE",
			status2 & 0x3ffff, status2);
	}

	if (status1 & BIT(CP_INT_VSDPARITYERROR))
		dev_crit_ratelimited(dev, "CP VSD decoder parity error\n");

	if (status1 & BIT(CP_INT_ILLEGALINSTRUCTION))
		dev_crit_ratelimited(dev, "CP Illegal instruction error\n");

	if (status1 & BIT(CP_INT_OPCODEERRORLPAC))
		dev_crit_ratelimited(dev, "CP Opcode error LPAC\n");

	if (status1 & BIT(CP_INT_UCODEERRORLPAC))
		dev_crit_ratelimited(dev, "CP ucode error LPAC\n");

	if (status1 & BIT(CP_INT_CPHWFAULTLPAC))
		dev_crit_ratelimited(dev, "CP hw fault LPAC\n");

	if (status1 & BIT(CP_INT_REGISTERPROTECTIONLPAC))
		dev_crit_ratelimited(dev, "CP register protection LPAC\n");

	if (status1 & BIT(CP_INT_ILLEGALINSTRUCTIONLPAC))
		dev_crit_ratelimited(dev, "CP illegal instruction LPAC\n");

	if (status1 & BIT(CP_INT_OPCODEERRORBV))
		dev_crit_ratelimited(dev, "CP opcode error BV\n");

	if (status1 & BIT(CP_INT_UCODEERRORBV))
		dev_crit_ratelimited(dev, "CP ucode error BV\n");

	if (status1 & BIT(CP_INT_CPHWFAULTBV))
		dev_crit_ratelimited(dev, "CP hw fault BV\n");

	if (status1 & BIT(CP_INT_REGISTERPROTECTIONBV))
		dev_crit_ratelimited(dev, "CP register protection BV\n");

	if (status1 & BIT(CP_INT_ILLEGALINSTRUCTIONBV))
		dev_crit_ratelimited(dev, "CP illegal instruction BV\n");
}

static void genc_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct device *dev = device->dev;

	switch (bit) {
	case GENC_INT_AHBERROR:
		dev_crit_ratelimited(dev, "CP: AHB bus error\n");
		break;
	case GENC_INT_ATBASYNCFIFOOVERFLOW:
		dev_crit_ratelimited(dev, "RBBM: ATB ASYNC overflow\n");
		break;
	case GENC_INT_ATBBUSOVERFLOW:
		dev_crit_ratelimited(dev, "RBBM: ATB bus overflow\n");
		break;
	case GENC_INT_OUTOFBOUNDACCESS:
		dev_crit_ratelimited(dev, "UCHE: Out of bounds access\n");
		break;
	case GENC_INT_UCHETRAPINTERRUPT:
		dev_crit_ratelimited(dev, "UCHE: Trap interrupt\n");
		break;
	case GENC_INT_TSBWRITEERROR:
		dev_crit_ratelimited(dev, "TSB: Write error interrupt\n");
		break;
	default:
		dev_crit_ratelimited(dev, "Unknown interrupt %d\n", bit);
	}
}

static const char *const uche_client[] = {
	"BR_VFD", "BR_SP", "BR_VSC", "BR_VPC",
	"BR_HLSQ", "BR_PC", "BR_LRZ", "BR_TP",
	"BV_VFD", "BV_SP", "BV_VSC", "BV_VPC",
	"BV_HLSQ", "BV_PC", "BV_LRZ", "BV_TP"
};

#define SCOOBYDOO 0x5c00bd00

static const char *genc_fault_block_uche(struct kgsl_device *device,
		unsigned int mid)
{
	unsigned int uche_client_id;
	static char str[20];

	/*
	 * Smmu driver takes a vote on CX gdsc before calling the kgsl
	 * pagefault handler. If there is contention for device mutex in this
	 * path and the dispatcher fault handler is holding this lock, trying
	 * to turn off CX gdsc will fail during the reset. So to avoid blocking
	 * here, try to lock device mutex and return if it fails.
	 */
	if (!mutex_trylock(&device->mutex))
		return "UCHE: unknown";

	if (!kgsl_state_is_awake(device)) {
		mutex_unlock(&device->mutex);
		return "UCHE: unknown";
	}

	kgsl_regread(device, GENC_UCHE_CLIENT_PF, &uche_client_id);
	mutex_unlock(&device->mutex);

	/* Ignore the value if the gpu is in IFPC */
	if (uche_client_id == SCOOBYDOO)
		return "UCHE: unknown";

	/* UCHE client id mask is bits [6:0] */
	uche_client_id &= GENMASK(6, 0);
	if (uche_client_id >= ARRAY_SIZE(uche_client))
		return "UCHE: Unknown";

	snprintf(str, sizeof(str), "UCHE: %s",
			uche_client[uche_client_id]);

	return str;
}

static const char *genc_iommu_fault_block(struct kgsl_device *device,
		unsigned int fsynr1)
{
	unsigned int mid = fsynr1 & 0xff;

	switch (mid) {
	case 0x0:
		return "CP";
	case 0x2:
		return "UCHE_LPAC";
	case 0x3:
		return genc_fault_block_uche(device, mid);
	case 0x4:
		return "CCU";
	case 0x6:
		return "Flag cache";
	case 0x7:
		return "GMU";
	}

	return "Unknown";
}

static void genc_cp_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_is_preemption_enabled(adreno_dev))
		genc_preemption_trigger(adreno_dev);

	adreno_dispatcher_schedule(device);
}

/*
 * genc_gpc_err_int_callback() - Isr for GPC error interrupts
 * @adreno_dev: Pointer to device
 * @bit: Interrupt bit
 */
static void genc_gpc_err_int_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/*
	 * GPC error is typically the result of mistake SW programming.
	 * Force GPU fault for this interrupt so that we can debug it
	 * with help of register dump.
	 */

	dev_crit(device->dev, "RBBM: GPC error\n");
	adreno_irqctrl(adreno_dev, 0);

	/* Trigger a fault in the dispatcher - this will effect a restart */
	adreno_set_gpu_fault(adreno_dev, ADRENO_SOFT_FAULT);
	adreno_dispatcher_schedule(device);
}

static const struct adreno_irq_funcs genc_irq_funcs[32] = {
	ADRENO_IRQ_CALLBACK(NULL), /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(genc_err_callback), /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 2 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 3 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 4 - CPIPCINT0 */
	ADRENO_IRQ_CALLBACK(NULL), /* 5 - CPIPCINT1 */
	ADRENO_IRQ_CALLBACK(genc_err_callback), /* 6 - ATBASYNCOVERFLOW */
	ADRENO_IRQ_CALLBACK(genc_gpc_err_int_callback), /* 7 - GPC_ERR */
	ADRENO_IRQ_CALLBACK(genc_preemption_callback),/* 8 - CP_SW */
	ADRENO_IRQ_CALLBACK(genc_cp_hw_err_callback), /* 9 - CP_HW_ERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 10 - CP_CCU_FLUSH_DEPTH_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 11 - CP_CCU_FLUSH_COLOR_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 12 - CP_CCU_RESOLVE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 13 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 14 - UNUSED */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 15 - CP_RB_INT */
	ADRENO_IRQ_CALLBACK(NULL), /* 16 - CP_RB_INT_LPAC*/
	ADRENO_IRQ_CALLBACK(NULL), /* 17 - CP_RB_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 18 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 19 - UNUSED */
	ADRENO_IRQ_CALLBACK(genc_cp_callback), /* 20 - CP_CACHE_FLUSH_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 21 - CP_CACHE_TS_LPAC */
	ADRENO_IRQ_CALLBACK(genc_err_callback), /* 22 - RBBM_ATB_BUS_OVERFLOW */
	ADRENO_IRQ_CALLBACK(adreno_hang_int_callback), /* 23 - MISHANGDETECT */
	ADRENO_IRQ_CALLBACK(genc_err_callback), /* 24 - UCHE_OOB_ACCESS */
	ADRENO_IRQ_CALLBACK(genc_err_callback), /* 25 - UCHE_TRAP_INTR */
	ADRENO_IRQ_CALLBACK(NULL), /* 26 - DEBBUS_INTR_0 */
	ADRENO_IRQ_CALLBACK(NULL), /* 27 - DEBBUS_INTR_1 */
	ADRENO_IRQ_CALLBACK(genc_err_callback), /* 28 - TSBWRITEERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 29 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 30 - ISDB_CPU_IRQ */
	ADRENO_IRQ_CALLBACK(NULL), /* 31 - ISDB_UNDER_DEBUG */
};

/*
 * If the AHB fence is not in ALLOW mode when we receive an RBBM
 * interrupt, something went wrong. This means that we cannot proceed
 * since the IRQ status and clear registers are not accessible.
 * This is usually harmless because the GMU will abort power collapse
 * and change the fence back to ALLOW. Poll so that this can happen.
 */
static int genc_irq_poll_fence(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 status, fence, fence_retries = 0;
	u64 a, b, c;

	a = genc_read_alwayson(adreno_dev);

	kgsl_regread(device, GENC_GMU_AO_AHB_FENCE_CTRL, &fence);

	while (fence != 0) {
		b = genc_read_alwayson(adreno_dev);

		/* Wait for small time before trying again */
		udelay(1);
		kgsl_regread(device, GENC_GMU_AO_AHB_FENCE_CTRL, &fence);

		if (fence_retries == 100 && fence != 0) {
			c = genc_read_alwayson(adreno_dev);

			kgsl_regread(device, GENC_GMU_RBBM_INT_UNMASKED_STATUS,
				&status);

			dev_crit_ratelimited(device->dev,
				"status=0x%x Unmasked status=0x%x Mask=0x%x timestamps: %llx %llx %llx\n",
					status & adreno_dev->irq_mask, status,
					adreno_dev->irq_mask, a, b, c);
				return -ETIMEDOUT;
		}

		fence_retries++;
	}

	return 0;
}

static irqreturn_t genc_irq_handler(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	irqreturn_t ret = IRQ_NONE;
	u32 status;

	/*
	 * GPU can power down once the INT_0_STATUS is read below.
	 * But there still might be some register reads required so
	 * force the GMU/GPU into KEEPALIVE mode until done with the ISR.
	 */
	genc_gpu_keepalive(adreno_dev, true);

	if (genc_irq_poll_fence(adreno_dev)) {
		adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
		adreno_dispatcher_schedule(device);
		goto done;
	}

	kgsl_regread(device, GENC_RBBM_INT_0_STATUS, &status);

	kgsl_regwrite(device, GENC_RBBM_INT_CLEAR_CMD, status);

	ret = adreno_irq_callbacks(adreno_dev, genc_irq_funcs, status);

	trace_kgsl_genc_irq_status(adreno_dev, status);

done:
	/* If hard fault, then let snapshot turn off the keepalive */
	if (!(adreno_gpu_fault(adreno_dev) & ADRENO_HARD_FAULT))
		genc_gpu_keepalive(adreno_dev, false);

	return ret;
}

int genc_probe_common(struct platform_device *pdev,
	struct adreno_device *adreno_dev, u32 chipid,
	const struct adreno_gpu_core *gpucore)
{
	const struct adreno_gpudev *gpudev = gpucore->gpudev;

	adreno_dev->gpucore = gpucore;
	adreno_dev->chipid = chipid;

	adreno_reg_offset_init(gpudev->reg_offsets);

	adreno_dev->hwcg_enabled = true;

	adreno_dev->preempt.preempt_level = 1;
	adreno_dev->preempt.skipsaverestore = true;
	adreno_dev->preempt.usesgmem = true;

	adreno_dev->gpu_llc_slice_enable = true;
	adreno_dev->gpuhtw_llc_slice_enable = true;

	return adreno_device_probe(pdev, adreno_dev);
}

/* Register offset defines for GenC, in order of enum adreno_regs */
static unsigned int genc_register_offsets[ADRENO_REG_REGISTER_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, GENC_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE_HI, GENC_CP_RB_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, GENC_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, GENC_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_CNTL, GENC_CP_SQE_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE, GENC_CP_IB1_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE_HI, GENC_CP_IB1_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BUFSZ, GENC_CP_IB1_REM_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE, GENC_CP_IB2_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE_HI, GENC_CP_IB2_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BUFSZ, GENC_CP_IB2_REM_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, GENC_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS3, GENC_RBBM_STATUS3),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, GENC_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, GENC_RBBM_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
			GENC_GMU_AO_HOST_INTERRUPT_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			GENC_GMU_GMU2HOST_INTR_MASK),
};

int genc_perfcounter_update(struct adreno_device *adreno_dev,
	struct adreno_perfcount_register *reg, bool update_reg)
{
	void *ptr = adreno_dev->pwrup_reglist->hostptr;
	struct cpu_gpu_lock *lock = ptr;
	u32 *data = ptr + sizeof(*lock);
	int i, offset = 0;

	if (kgsl_hwlock(lock)) {
		kgsl_hwunlock(lock);
		return -EBUSY;
	}

	/*
	 * If the perfcounter select register is already present in reglist
	 * update it, otherwise append the <select register, value> pair to
	 * the end of the list.
	 */
	for (i = 0; i < lock->list_length >> 1; i++) {
		if (data[offset] == reg->select) {
			data[offset + 1] = reg->countable;
			goto update;
		}

		if (data[offset] == GENC_RBBM_PERFCTR_CNTL)
			break;

		offset += 2;
	}

	/*
	 * For all targets GENC_RBBM_PERFCTR_CNTL needs to be the last entry,
	 * so overwrite the existing GENC_RBBM_PERFCNTL_CTRL and add it back to
	 * the end.
	 */
	data[offset] = reg->select;
	data[offset + 1] = reg->countable;
	data[offset + 2] = GENC_RBBM_PERFCTR_CNTL;
	data[offset + 3] = 1;

	lock->list_length += 2;

update:
	if (update_reg)
		kgsl_regwrite(KGSL_DEVICE(adreno_dev), reg->select,
			reg->countable);

	kgsl_hwunlock(lock);
	return 0;
}

u64 genc_read_alwayson(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 lo = 0, hi = 0, tmp = 0;

	/* Always use the GMU AO counter when doing a AHB read */
	gmu_core_regread(device, GENC_GMU_ALWAYS_ON_COUNTER_H, &hi);
	gmu_core_regread(device, GENC_GMU_ALWAYS_ON_COUNTER_L, &lo);

	/* Check for overflow */
	gmu_core_regread(device, GENC_GMU_ALWAYS_ON_COUNTER_H, &tmp);

	if (hi != tmp) {
		gmu_core_regread(device, GENC_GMU_ALWAYS_ON_COUNTER_L,
				&lo);
		hi = tmp;
	}

	return (((u64) hi) << 32) | lo;
}

static void genc_remove(struct adreno_device *adreno_dev)
{
	if (ADRENO_FEATURE(adreno_dev, ADRENO_PREEMPTION))
		del_timer(&adreno_dev->preempt.timer);
}

static void genc_read_bus_stats(struct kgsl_device *device,
		struct kgsl_power_stats *stats,
		struct adreno_busy_data *busy)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u64 ram_cycles, starved_ram;

	ram_cycles = counter_delta(device, adreno_dev->ram_cycles_lo,
		&busy->bif_ram_cycles);

	starved_ram = counter_delta(device, adreno_dev->starved_ram_lo,
		&busy->bif_starved_ram);

	ram_cycles += counter_delta(device,
		adreno_dev->ram_cycles_lo_ch1_read,
		&busy->bif_ram_cycles_read_ch1);

	ram_cycles += counter_delta(device,
		adreno_dev->ram_cycles_lo_ch0_write,
		&busy->bif_ram_cycles_write_ch0);

	ram_cycles += counter_delta(device,
		adreno_dev->ram_cycles_lo_ch1_write,
		&busy->bif_ram_cycles_write_ch1);

	starved_ram += counter_delta(device,
		adreno_dev->starved_ram_lo_ch1,
		&busy->bif_starved_ram_ch1);

	stats->ram_time = ram_cycles;
	stats->ram_wait = starved_ram;
}

static void genc_power_stats(struct adreno_device *adreno_dev,
		struct kgsl_power_stats *stats)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_busy_data *busy = &adreno_dev->busy_data;
	u64 gpu_busy;

	/* Set the GPU busy counter for frequency scaling */
	gpu_busy = counter_delta(device, GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_L,
		&busy->gpu_busy);

	stats->busy_time = gpu_busy * 10;
	do_div(stats->busy_time, 192);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_IFPC)) {
		u32 ifpc = counter_delta(device,
			GENC_GMU_CX_GMU_POWER_COUNTER_XOCLK_4_L,
			&busy->num_ifpc);

		adreno_dev->ifpc_count += ifpc;
		if (ifpc > 0)
			trace_adreno_ifpc_count(adreno_dev->ifpc_count);
	}

	if (device->pwrctrl.bus_control)
		genc_read_bus_stats(device, stats, busy);
}

static int genc_setproperty(struct kgsl_device_private *dev_priv,
		u32 type, void __user *value, u32 sizebytes)
{
	struct kgsl_device *device = dev_priv->device;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u32 enable;

	if (type != KGSL_PROP_PWRCTRL)
		return -ENODEV;

	if (sizebytes != sizeof(enable))
		return -EINVAL;

	if (copy_from_user(&enable, value, sizeof(enable)))
		return -EFAULT;

	mutex_lock(&device->mutex);

	if (enable) {
		clear_bit(GMU_DISABLE_SLUMBER, &device->gmu_core.flags);

		kgsl_pwrscale_enable(device);
	} else {
		set_bit(GMU_DISABLE_SLUMBER, &device->gmu_core.flags);

		if (!adreno_active_count_get(adreno_dev))
			adreno_active_count_put(adreno_dev);

		kgsl_pwrscale_disable(device, true);
	}

	mutex_unlock(&device->mutex);

	return 0;
}

static void genc_gpu_model(struct adreno_device *adreno_dev,
		char *str, size_t bufsz)
{
	int ver = 0;

	/*
	 * GenC does not include a 'chipid' in the device tree. It also does
	 * not follow the old school naming conventions. Force the gpu model
	 * to the agreed upon Cxxxvx naming convention.
	 */
	if (adreno_is_c500(adreno_dev))
		ver = 500;

	snprintf(str, bufsz, "AdrenoC%dv%ld", ver,
			ADRENO_CHIPID_PATCH(adreno_dev->chipid) + 1);
}

const struct adreno_gpudev adreno_genc_hwsched_gpudev = {
	.reg_offsets = genc_register_offsets,
	.probe = genc_hwsched_probe,
	.snapshot = genc_hwsched_snapshot,
	.irq_handler = genc_irq_handler,
	.iommu_fault_block = genc_iommu_fault_block,
	.preemption_context_init = genc_preemption_context_init,
	.context_detach = genc_hwsched_context_detach,
	.read_alwayson = genc_read_alwayson,
	.power_ops = &genc_hwsched_power_ops,
	.power_stats = genc_power_stats,
	.setproperty = genc_setproperty,
	.gpu_model = genc_gpu_model,
};

const struct adreno_gpudev adreno_genc_gmu_gpudev = {
	.reg_offsets = genc_register_offsets,
	.probe = genc_gmu_device_probe,
	.snapshot = genc_gmu_snapshot,
	.irq_handler = genc_irq_handler,
	.rb_start = genc_rb_start,
	.gpu_keepalive = genc_gpu_keepalive,
	.hw_isidle = genc_hw_isidle,
	.iommu_fault_block = genc_iommu_fault_block,
	.reset = genc_gmu_restart,
	.preemption_schedule = genc_preemption_schedule,
	.preemption_context_init = genc_preemption_context_init,
	.read_alwayson = genc_read_alwayson,
	.power_ops = &genc_gmu_power_ops,
	.remove = genc_remove,
	.ringbuffer_submitcmd = genc_ringbuffer_submitcmd,
	.power_stats = genc_power_stats,
	.setproperty = genc_setproperty,
	.gpu_model = genc_gpu_model,
};
