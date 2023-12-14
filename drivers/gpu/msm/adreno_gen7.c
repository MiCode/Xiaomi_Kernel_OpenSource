// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/qcom/llcc-qcom.h>

#include "adreno.h"
#include "adreno_gen7.h"
#include "adreno_gen7_hwsched.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"
#include "kgsl_pwrscale.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

/* IFPC & Preemption static powerup restore list */
static const u32 gen7_pwrup_reglist[] = {
	GEN7_UCHE_TRAP_BASE_LO,
	GEN7_UCHE_TRAP_BASE_HI,
	GEN7_UCHE_WRITE_THRU_BASE_LO,
	GEN7_UCHE_WRITE_THRU_BASE_HI,
	GEN7_UCHE_GMEM_RANGE_MIN_LO,
	GEN7_UCHE_GMEM_RANGE_MIN_HI,
	GEN7_UCHE_GMEM_RANGE_MAX_LO,
	GEN7_UCHE_GMEM_RANGE_MAX_HI,
	GEN7_UCHE_CACHE_WAYS,
	GEN7_UCHE_MODE_CNTL,
	GEN7_RB_NC_MODE_CNTL,
	GEN7_RB_CMP_DBG_ECO_CNTL,
	GEN7_SP_NC_MODE_CNTL,
	GEN7_GRAS_NC_MODE_CNTL,
	GEN7_RB_CONTEXT_SWITCH_GMEM_SAVE_RESTORE,
	GEN7_UCHE_GBIF_GX_CONFIG,
	GEN7_UCHE_CLIENT_PF,
};

/* IFPC only static powerup restore list */
static const u32 gen7_ifpc_pwrup_reglist[] = {
	GEN7_TPL1_NC_MODE_CNTL,
	GEN7_CP_DBG_ECO_CNTL,
	GEN7_CP_PROTECT_CNTL,
	GEN7_CP_LPAC_PROTECT_CNTL,
	GEN7_CP_PROTECT_REG,
	GEN7_CP_PROTECT_REG+1,
	GEN7_CP_PROTECT_REG+2,
	GEN7_CP_PROTECT_REG+3,
	GEN7_CP_PROTECT_REG+4,
	GEN7_CP_PROTECT_REG+5,
	GEN7_CP_PROTECT_REG+6,
	GEN7_CP_PROTECT_REG+7,
	GEN7_CP_PROTECT_REG+8,
	GEN7_CP_PROTECT_REG+9,
	GEN7_CP_PROTECT_REG+10,
	GEN7_CP_PROTECT_REG+11,
	GEN7_CP_PROTECT_REG+12,
	GEN7_CP_PROTECT_REG+13,
	GEN7_CP_PROTECT_REG+14,
	GEN7_CP_PROTECT_REG+15,
	GEN7_CP_PROTECT_REG+16,
	GEN7_CP_PROTECT_REG+17,
	GEN7_CP_PROTECT_REG+18,
	GEN7_CP_PROTECT_REG+19,
	GEN7_CP_PROTECT_REG+20,
	GEN7_CP_PROTECT_REG+21,
	GEN7_CP_PROTECT_REG+22,
	GEN7_CP_PROTECT_REG+23,
	GEN7_CP_PROTECT_REG+24,
	GEN7_CP_PROTECT_REG+25,
	GEN7_CP_PROTECT_REG+26,
	GEN7_CP_PROTECT_REG+27,
	GEN7_CP_PROTECT_REG+28,
	GEN7_CP_PROTECT_REG+29,
	GEN7_CP_PROTECT_REG+30,
	GEN7_CP_PROTECT_REG+31,
	GEN7_CP_PROTECT_REG+32,
	GEN7_CP_PROTECT_REG+33,
	GEN7_CP_PROTECT_REG+34,
	GEN7_CP_PROTECT_REG+35,
	GEN7_CP_PROTECT_REG+36,
	GEN7_CP_PROTECT_REG+37,
	GEN7_CP_PROTECT_REG+38,
	GEN7_CP_PROTECT_REG+39,
	GEN7_CP_PROTECT_REG+40,
	GEN7_CP_PROTECT_REG+41,
	GEN7_CP_PROTECT_REG+42,
	GEN7_CP_PROTECT_REG+43,
	GEN7_CP_PROTECT_REG+44,
	GEN7_CP_PROTECT_REG+45,
	GEN7_CP_PROTECT_REG+46,
	GEN7_CP_PROTECT_REG+47,
	GEN7_CP_AHB_CNTL,
};

void gen7_cp_init_cmds(struct adreno_device *adreno_dev, u32 *cmds)
{
	u32 i = 0, mask = 0;

	/* Disable concurrent binning before sending CP init */
	cmds[i++] = cp_type7_packet(CP_THREAD_CONTROL, 1);
	cmds[i++] = BIT(27);

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

	/* By default DMS is enabled from CP side, disable it if not supported */
	if (!adreno_dev->dms_enabled)
		mask |= BIT(11);

	cmds[i++] = cp_type7_packet(CP_ME_INIT, 7);

	/* Enabled ordinal mask */
	cmds[i++] = mask;
	cmds[i++] = 0x00000003; /* Set number of HW contexts */
	cmds[i++] = 0x20000000; /* Enable error detection */
	cmds[i++] = 0x00000002; /* Operation mode mask */

	/* Register initialization list with spinlock */
	cmds[i++] = lower_32_bits(adreno_dev->pwrup_reglist->gpuaddr);
	cmds[i++] = upper_32_bits(adreno_dev->pwrup_reglist->gpuaddr);
	/*
	 * Gen7 targets with concurrent binning are expected to have a dynamic
	 * power up list with triplets which contains the pipe id in it.
	 * Bit 31 of POWER_UP_REGISTER_LIST_LENGTH is reused here to let CP
	 * know if the power up contains the triplets. If
	 * REGISTER_INIT_LIST_WITH_SPINLOCK is set and bit 31 below is set,
	 * CP expects a dynamic list with triplets.
	 */
	cmds[i++] = BIT(31);
}

int gen7_fenced_write(struct adreno_device *adreno_dev, u32 offset,
		u32 value, u32 mask)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status, i;
	u64 ts1, ts2;

	kgsl_regwrite(device, offset, value);
	ts1 = gen7_read_alwayson(adreno_dev);
	for (i = 0; i < GMU_CORE_LONG_WAKEUP_RETRY_LIMIT; i++) {
		/*
		 * Make sure the previous register write is posted before
		 * checking the fence status
		 */
		mb();

		gmu_core_regread(device, GEN7_GMU_AHB_FENCE_STATUS, &status);

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

	if (i == GMU_CORE_LONG_WAKEUP_RETRY_LIMIT) {
		ts2 = gen7_read_alwayson(adreno_dev);
		dev_err(device->dev,
				"Timed out waiting %d usecs to write fenced register 0x%x, timestamps: %llx %llx\n",
				i * GMU_CORE_WAKEUP_DELAY_US, offset, ts1, ts2);
		return -ETIMEDOUT;
	}

	dev_info(device->dev,
		"Waited %d usecs to write fenced register 0x%x\n",
		i * GMU_CORE_WAKEUP_DELAY_US, offset);

	return 0;
}

int gen7_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	u64 freq = gen7_core->gmu_hub_clk_freq;

	adreno_dev->highest_bank_bit = gen7_core->highest_bank_bit;
	adreno_dev->gmu_hub_clk_freq = freq ? freq : 150000000;
	adreno_dev->bcl_data = gen7_core->bcl_data;

	adreno_dev->cooperative_reset = ADRENO_FEATURE(adreno_dev,
			ADRENO_COOP_RESET);

	/* If the memory type is DDR 4, override the existing configuration */
	if (of_fdt_get_ddrtype() == 0x7)
		adreno_dev->highest_bank_bit = 14;

	gen7_crashdump_init(adreno_dev);

	return adreno_allocate_global(device, &adreno_dev->pwrup_reglist,
		PAGE_SIZE, 0, 0, KGSL_MEMDESC_PRIVILEGED,
		"powerup_register_list");
}

#define GEN7_PROTECT_DEFAULT (BIT(0) | BIT(1) | BIT(3))
static void gen7_protect_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	const struct gen7_protected_regs *regs = gen7_core->protected_regs;
	int i;

	/*
	 * Enable access protection to privileged registers, fault on an access
	 * protect violation and select the last span to protect from the start
	 * address all the way to the end of the register address space
	 */
	kgsl_regwrite(device, GEN7_CP_PROTECT_CNTL, GEN7_PROTECT_DEFAULT);

	if (adreno_dev->lpac_enabled)
		kgsl_regwrite(device, GEN7_CP_LPAC_PROTECT_CNTL, GEN7_PROTECT_DEFAULT);

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

#define RBBM_CLOCK_CNTL_ON 0x8aa8aa82
#define GMU_AO_CGC_MODE_CNTL 0x00020000
#define GEN7_6_0_GMU_AO_CGC_MODE_CNTL 0x00020202
#define GMU_AO_CGC_DELAY_CNTL 0x00010111
#define GMU_AO_CGC_HYST_CNTL 0x00005555

static void gen7_hwcg_set(struct adreno_device *adreno_dev, bool on)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	unsigned int value, cgc_mode_cntl;
	int i;

	if (!adreno_dev->hwcg_enabled)
		on = false;

	cgc_mode_cntl = adreno_is_gen7_6_0(adreno_dev) ?
			GEN7_6_0_GMU_AO_CGC_MODE_CNTL :
			GMU_AO_CGC_MODE_CNTL;
	gmu_core_regwrite(device, GEN7_GPU_GMU_AO_GMU_CGC_MODE_CNTL,
			on ? cgc_mode_cntl : 0);
	gmu_core_regwrite(device, GEN7_GPU_GMU_AO_GMU_CGC_DELAY_CNTL,
			on ? GMU_AO_CGC_DELAY_CNTL : 0);
	gmu_core_regwrite(device, GEN7_GPU_GMU_AO_GMU_CGC_HYST_CNTL,
			on ? GMU_AO_CGC_HYST_CNTL : 0);

	kgsl_regread(device, GEN7_RBBM_CLOCK_CNTL, &value);

	if (value == RBBM_CLOCK_CNTL_ON && on)
		return;

	if (value == 0 && !on)
		return;

	for (i = 0; i < gen7_core->hwcg_count; i++)
		kgsl_regwrite(device, gen7_core->hwcg[i].offset,
			on ? gen7_core->hwcg[i].val : 0);

	/* enable top level HWCG */
	kgsl_regwrite(device, GEN7_RBBM_CLOCK_CNTL,
		on ? RBBM_CLOCK_CNTL_ON : 0);
}

static void gen7_patch_pwrup_reglist(struct adreno_device *adreno_dev)
{
	struct adreno_reglist_list reglist[2];
	void *ptr = adreno_dev->pwrup_reglist->hostptr;
	struct cpu_gpu_lock *lock = ptr;
	int i, j;
	u32 *dest = ptr + sizeof(*lock);

	/* Static IFPC-only registers */
	reglist[0].regs = gen7_ifpc_pwrup_reglist;
	reglist[0].count = ARRAY_SIZE(gen7_ifpc_pwrup_reglist);
	lock->ifpc_list_len = reglist[0].count;

	/* Static IFPC + preemption registers */
	reglist[1].regs = gen7_pwrup_reglist;
	reglist[1].count = ARRAY_SIZE(gen7_pwrup_reglist);
	lock->preemption_list_len = reglist[1].count;

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
	}

	/* This needs to be at the end of the dynamic list */
	*dest++ = FIELD_PREP(GENMASK(13, 12), PIPE_NONE);
	*dest++ = GEN7_RBBM_PERFCTR_CNTL;
	*dest++ = 1;

	/*
	 * The overall register list is composed of
	 * 1. Static IFPC-only registers
	 * 2. Static IFPC + preemption registers
	 * 3. Dynamic IFPC + preemption registers (ex: perfcounter selects)
	 *
	 * The first two lists are static. Size of these lists are stored as
	 * number of pairs in ifpc_list_len and preemption_list_len
	 * respectively. With concurrent binning, Some of the perfcounter
	 * registers being virtualized, CP needs to know the pipe id to program
	 * the aperture inorder to restore the same. Thus, third list is a
	 * dynamic list with triplets as
	 * (<aperture, shifted 12 bits> <address> <data>), and the length is
	 * stored as number for triplets in dynamic_list_len.
	 */
	lock->dynamic_list_len = 1;
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
	kgsl_regwrite(device, GEN7_GBIF_SCACHE_CNTL1,
			FIELD_PREP(GENMASK(29, 25), gpu_scid) |
			FIELD_PREP(GENMASK(24, 20), gpu_scid) |
			FIELD_PREP(GENMASK(19, 15), gpu_scid) |
			FIELD_PREP(GENMASK(14, 10), gpu_scid) |
			FIELD_PREP(GENMASK(9, 5), gpu_scid) |
			FIELD_PREP(GENMASK(4, 0), gpu_scid));

	kgsl_regwrite(device, GEN7_GBIF_SCACHE_CNTL0,
			FIELD_PREP(GENMASK(14, 10), gpu_scid) | BIT(8));
}

static void _llc_gpuhtw_slice_activate(struct adreno_device *adreno_dev)
{
	if (IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice) ||
		!adreno_dev->gpuhtw_llc_slice_enable)
		return;

	llcc_slice_activate(adreno_dev->gpuhtw_llc_slice);
}

static void _set_secvid(struct kgsl_device *device)
{
	kgsl_regwrite(device, GEN7_RBBM_SECVID_TSB_CNTL, 0x0);
	kgsl_regwrite(device, GEN7_RBBM_SECVID_TSB_TRUSTED_BASE_LO,
		lower_32_bits(KGSL_IOMMU_SECURE_BASE32));
	kgsl_regwrite(device, GEN7_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
		upper_32_bits(KGSL_IOMMU_SECURE_BASE32));
	kgsl_regwrite(device, GEN7_RBBM_SECVID_TSB_TRUSTED_SIZE,
		FIELD_PREP(GENMASK(31, 12),
		(KGSL_IOMMU_SECURE_SIZE(&device->mmu) / SZ_4K)));
}

/*
 * All Gen7 targets support marking certain transactions as always privileged
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

#define GEN7_APRIV_DEFAULT (BIT(3) | BIT(2) | BIT(1) | BIT(0))
/* Add crashdumper permissions for the BR APRIV */
#define GEN7_BR_APRIV_DEFAULT (GEN7_APRIV_DEFAULT | BIT(6) | BIT(5))

int gen7_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	u32 bank_bit, mal;

	/* Set up GBIF registers from the GPU core definition */
	kgsl_regmap_multi_write(&device->regmap, gen7_core->gbif,
		gen7_core->gbif_count);

	kgsl_regwrite(device, GEN7_UCHE_GBIF_GX_CONFIG, 0x10240e0);

	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, GEN7_RBBM_PERFCTR_GPU_BUSY_MASKED, 0xffffffff);

	/*
	 * Set UCHE_WRITE_THRU_BASE to the UCHE_TRAP_BASE effectively
	 * disabling L2 bypass
	 */
	kgsl_regwrite(device, GEN7_UCHE_TRAP_BASE_LO, 0xfffff000);
	kgsl_regwrite(device, GEN7_UCHE_TRAP_BASE_HI, 0x0001ffff);
	kgsl_regwrite(device, GEN7_UCHE_WRITE_THRU_BASE_LO, 0xfffff000);
	kgsl_regwrite(device, GEN7_UCHE_WRITE_THRU_BASE_HI, 0x0001ffff);

	/*
	 * Some gen7 targets don't use a programmed UCHE GMEM base address,
	 * so skip programming the register for such targets.
	 */
	if (adreno_dev->uche_gmem_base) {
		kgsl_regwrite(device, GEN7_UCHE_GMEM_RANGE_MIN_LO,
				adreno_dev->uche_gmem_base);
		kgsl_regwrite(device, GEN7_UCHE_GMEM_RANGE_MIN_HI, 0x0);
		kgsl_regwrite(device, GEN7_UCHE_GMEM_RANGE_MAX_LO,
				adreno_dev->uche_gmem_base +
				adreno_dev->gpucore->gmem_size - 1);
		kgsl_regwrite(device, GEN7_UCHE_GMEM_RANGE_MAX_HI, 0x0);
	}

	kgsl_regwrite(device, GEN7_UCHE_CACHE_WAYS, 0x800000);

	kgsl_regwrite(device, GEN7_UCHE_CMDQ_CONFIG,
			FIELD_PREP(GENMASK(19, 16), 6) |
			FIELD_PREP(GENMASK(15, 12), 6) |
			FIELD_PREP(GENMASK(11, 8), 9) |
			BIT(3) | BIT(2) |
			FIELD_PREP(GENMASK(1, 0), 2));

	/* Set the AHB default slave response to "ERROR" */
	kgsl_regwrite(device, GEN7_CP_AHB_CNTL, 0x1);

	/* Turn on performance counters */
	kgsl_regwrite(device, GEN7_RBBM_PERFCTR_CNTL, 0x1);

	/* Turn on the IFPC counter (countable 4 on XOCLK4) */
	kgsl_regwrite(device, GEN7_GMU_CX_GMU_POWER_COUNTER_SELECT_1,
			FIELD_PREP(GENMASK(7, 0), 0x4));

	/* Turn on counter to count total time spent in BCL throttle */
	if (adreno_dev->bcl_enabled && adreno_is_gen7_6_0(adreno_dev))
		kgsl_regrmw(device, GEN7_GMU_CX_GMU_POWER_COUNTER_SELECT_1, GENMASK(15, 8),
				FIELD_PREP(GENMASK(15, 8), 0x26));

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,min-access-length", &mal))
		mal = 32;

	if (!WARN_ON(!adreno_dev->highest_bank_bit))
		bank_bit = (adreno_dev->highest_bank_bit - 13) & 3;
	else
		bank_bit = 1;

	kgsl_regwrite(device, GEN7_RB_NC_MODE_CNTL, BIT(11) | BIT(4) |
			((mal == 64) ? BIT(3) : 0) |
			FIELD_PREP(GENMASK(2, 1), bank_bit));
	kgsl_regwrite(device, GEN7_TPL1_NC_MODE_CNTL,
			((mal == 64) ? BIT(3) : 0) |
			FIELD_PREP(GENMASK(2, 1), bank_bit));
	kgsl_regwrite(device, GEN7_SP_NC_MODE_CNTL,
			((mal == 64) ? BIT(3) : 0) |
			FIELD_PREP(GENMASK(5, 4), 2) |
			FIELD_PREP(GENMASK(2, 1), bank_bit));
	kgsl_regwrite(device, GEN7_GRAS_NC_MODE_CNTL,
			FIELD_PREP(GENMASK(8, 5), bank_bit));

	kgsl_regwrite(device, GEN7_UCHE_MODE_CNTL,
			FIELD_PREP(GENMASK(22, 21), bank_bit));
	kgsl_regwrite(device, GEN7_RBBM_INTERFACE_HANG_INT_CNTL, BIT(30) |
			FIELD_PREP(GENMASK(27, 0),
				gen7_core->hang_detect_cycles));
	kgsl_regwrite(device, GEN7_UCHE_CLIENT_PF, BIT(7) |
			FIELD_PREP(GENMASK(3, 0), adreno_dev->uche_client_pf));

	/* Enable the GMEM save/restore feature for preemption */
	if (adreno_is_preemption_enabled(adreno_dev))
		kgsl_regwrite(device, GEN7_RB_CONTEXT_SWITCH_GMEM_SAVE_RESTORE,
			0x1);

	/* Enable GMU power counter 0 to count GPU busy */
	kgsl_regwrite(device, GEN7_GPU_GMU_AO_GPU_CX_BUSY_MASK, 0xff000000);
	kgsl_regrmw(device, GEN7_GMU_CX_GMU_POWER_COUNTER_SELECT_0,
			0xFF, 0x20);
	kgsl_regwrite(device, GEN7_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0x1);

	/* Disable non-ubwc read reqs from passing write reqs */
	kgsl_regrmw(device, GEN7_RB_CMP_DBG_ECO_CNTL, 0, (1 << 11));

	gen7_protect_init(adreno_dev);

	/* Configure LLCC */
	_llc_configure_gpu_scid(adreno_dev);
	_llc_gpuhtw_slice_activate(adreno_dev);

	kgsl_regwrite(device, GEN7_CP_APRIV_CNTL, GEN7_BR_APRIV_DEFAULT);
	if (!adreno_is_gen7_3_0(adreno_dev)) {
		kgsl_regwrite(device, GEN7_CP_BV_APRIV_CNTL, GEN7_APRIV_DEFAULT);
		kgsl_regwrite(device, GEN7_CP_LPAC_APRIV_CNTL, GEN7_APRIV_DEFAULT);
	}

	/*
	 * CP Icache prefetch brings no benefit on few gen7 variants because of
	 * the prefetch granularity size.
	 */
	if (adreno_is_gen7_0_0(adreno_dev) || adreno_is_gen7_0_1(adreno_dev) ||
		adreno_is_gen7_4_0(adreno_dev) || adreno_is_gen7_6_0(adreno_dev)) {
		kgsl_regwrite(device, GEN7_CP_CHICKEN_DBG, 0x1);
		kgsl_regwrite(device, GEN7_CP_BV_CHICKEN_DBG, 0x1);
		kgsl_regwrite(device, GEN7_CP_LPAC_CHICKEN_DBG, 0x1);
	}

	_set_secvid(device);

	/*
	 * Enable hardware clock gating here to prevent any register access
	 * issue due to internal clock gating.
	 */
	gen7_hwcg_set(adreno_dev, true);

	/*
	 * All registers must be written before this point so that we don't
	 * miss any register programming when we patch the power up register
	 * list.
	 */
	if (!adreno_dev->patch_reglist &&
		(adreno_dev->pwrup_reglist->gpuaddr != 0)) {
		gen7_patch_pwrup_reglist(adreno_dev);
		adreno_dev->patch_reglist = true;
	}

	return 0;
}

/* Offsets into the MX/CX mapped register regions */
#define GEN7_RDPM_MX_OFFSET 0xf00
#define GEN7_RDPM_CX_OFFSET 0xf14

void gen7_rdpm_mx_freq_update(struct gen7_gmu_device *gmu, u32 freq)
{
	if (gmu->rdpm_mx_virt) {
		writel_relaxed(freq/1000, (gmu->rdpm_mx_virt + GEN7_RDPM_MX_OFFSET));

		/*
		 * ensure previous writes post before this one,
		 * i.e. act like normal writel()
		 */
		wmb();
	}
}

void gen7_rdpm_cx_freq_update(struct gen7_gmu_device *gmu, u32 freq)
{
	if (gmu->rdpm_cx_virt) {
		writel_relaxed(freq/1000, (gmu->rdpm_cx_virt + GEN7_RDPM_CX_OFFSET));

		/*
		 * ensure previous writes post before this one,
		 * i.e. act like normal writel()
		 */
		wmb();
	}
}

void gen7_spin_idle_debug(struct adreno_device *adreno_dev,
				const char *str)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int rptr, wptr;
	unsigned int status, status3, intstatus;
	unsigned int hwfault;

	dev_err(device->dev, str);

	kgsl_regread(device, GEN7_CP_RB_RPTR, &rptr);
	kgsl_regread(device, GEN7_CP_RB_WPTR, &wptr);

	kgsl_regread(device, GEN7_RBBM_STATUS, &status);
	kgsl_regread(device, GEN7_RBBM_STATUS3, &status3);
	kgsl_regread(device, GEN7_RBBM_INT_0_STATUS, &intstatus);
	kgsl_regread(device, GEN7_CP_HW_FAULT, &hwfault);

	dev_err(device->dev,
		"rb=%d pos=%X/%X rbbm_status=%8.8X/%8.8X int_0_status=%8.8X\n",
		adreno_dev->cur_rb ? adreno_dev->cur_rb->id : -1, rptr, wptr,
		status, status3, intstatus);

	dev_err(device->dev, " hwfault=%8.8X\n", hwfault);

	kgsl_device_snapshot(device, NULL, NULL, false);
}

/*
 * gen7_send_cp_init() - Initialize ringbuffer
 * @adreno_dev: Pointer to adreno device
 * @rb: Pointer to the ringbuffer of device
 *
 * Submit commands for ME initialization,
 */
static int gen7_send_cp_init(struct adreno_device *adreno_dev,
			 struct adreno_ringbuffer *rb)
{
	unsigned int *cmds;
	int ret;

	cmds = adreno_ringbuffer_allocspace(rb, GEN7_CP_INIT_DWORDS);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	gen7_cp_init_cmds(adreno_dev, cmds);

	ret = gen7_ringbuffer_submit(rb, NULL);
	if (!ret) {
		ret = adreno_spin_idle(adreno_dev, 2000);
		if (ret) {
			gen7_spin_idle_debug(adreno_dev,
				"CP initialization failed to idle\n");
			rb->wptr = 0;
			rb->_wptr = 0;
		}
	}

	return ret;
}

static int gen7_post_start(struct adreno_device *adreno_dev)
{
	int ret;
	unsigned int *cmds;
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;
	struct adreno_preemption *preempt = &adreno_dev->preempt;
	u64 kmd_postamble_addr;

	if (!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	kmd_postamble_addr = SCRATCH_POSTAMBLE_ADDR(KGSL_DEVICE(adreno_dev));
	gen7_preemption_prepare_postamble(adreno_dev);

	cmds = adreno_ringbuffer_allocspace(rb, (preempt->postamble_len ? 16 : 12));
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	*cmds++ = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 6);
	*cmds++ = SET_PSEUDO_PRIV_NON_SECURE_SAVE_ADDR;
	*cmds++ = lower_32_bits(rb->preemption_desc->gpuaddr);
	*cmds++ = upper_32_bits(rb->preemption_desc->gpuaddr);

	*cmds++ = SET_PSEUDO_PRIV_SECURE_SAVE_ADDR;
	*cmds++ = lower_32_bits(rb->secure_preemption_desc->gpuaddr);
	*cmds++ = upper_32_bits(rb->secure_preemption_desc->gpuaddr);

	/* Postambles are not preserved across slumber */
	if (preempt->postamble_len) {
		*cmds++ = cp_type7_packet(CP_SET_AMBLE, 3);
		*cmds++ = lower_32_bits(kmd_postamble_addr);
		*cmds++ = upper_32_bits(kmd_postamble_addr);
		*cmds++ = FIELD_PREP(GENMASK(22, 20), CP_KMD_AMBLE_TYPE)
			| (FIELD_PREP(GENMASK(19, 0), adreno_dev->preempt.postamble_len));
	}

	*cmds++ = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	*cmds++ = 0;
	*cmds++ = 0;
	*cmds++ = 0;
	/* generate interrupt on preemption completion */
	*cmds++ = 0;

	ret = gen7_ringbuffer_submit(rb, NULL);
	if (!ret) {
		ret = adreno_spin_idle(adreno_dev, 2000);
		if (ret)
			gen7_spin_idle_debug(adreno_dev,
				"hw preemption initialization failed to idle\n");
	}

	return ret;
}

int gen7_rb_start(struct adreno_device *adreno_dev)
{
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_ringbuffer *rb;
	u64 addr;
	int ret, i;
	unsigned int *cmds;

	/* Clear all the ringbuffers */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		memset(rb->buffer_desc->hostptr, 0xaa, KGSL_RB_SIZE);
		kgsl_sharedmem_writel(device->scratch,
			SCRATCH_RB_OFFSET(rb->id, rptr), 0);
		kgsl_sharedmem_writel(device->scratch,
			SCRATCH_RB_OFFSET(rb->id, bv_rptr), 0);

		rb->wptr = 0;
		rb->_wptr = 0;
		rb->wptr_preempt_end = UINT_MAX;
	}

	gen7_preemption_start(adreno_dev);

	/* Set up the current ringbuffer */
	rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);

	addr = SCRATCH_RB_GPU_ADDR(device, rb->id, rptr);
	kgsl_regwrite(device, GEN7_CP_RB_RPTR_ADDR_LO, lower_32_bits(addr));
	kgsl_regwrite(device, GEN7_CP_RB_RPTR_ADDR_HI, upper_32_bits(addr));

	addr = SCRATCH_RB_GPU_ADDR(device, rb->id, bv_rptr);
	if (!adreno_is_gen7_3_0(adreno_dev)) {
		kgsl_regwrite(device, GEN7_CP_BV_RB_RPTR_ADDR_LO, lower_32_bits(addr));
		kgsl_regwrite(device, GEN7_CP_BV_RB_RPTR_ADDR_HI, upper_32_bits(addr));
	}

	kgsl_regwrite(device, GEN7_CP_RB_CNTL, GEN7_CP_RB_CNTL_DEFAULT);

	kgsl_regwrite(device, GEN7_CP_RB_BASE,
		lower_32_bits(rb->buffer_desc->gpuaddr));
	kgsl_regwrite(device, GEN7_CP_RB_BASE_HI,
		upper_32_bits(rb->buffer_desc->gpuaddr));

	/* Program the ucode base for CP */
	kgsl_regwrite(device, GEN7_CP_SQE_INSTR_BASE_LO,
		lower_32_bits(fw->memdesc->gpuaddr));
	kgsl_regwrite(device, GEN7_CP_SQE_INSTR_BASE_HI,
		upper_32_bits(fw->memdesc->gpuaddr));

	/* Clear the SQE_HALT to start the CP engine */
	kgsl_regwrite(device, GEN7_CP_SQE_CNTL, 1);

	ret = gen7_send_cp_init(adreno_dev, rb);
	if (ret)
		return ret;

	ret = adreno_zap_shader_load(adreno_dev, gen7_core->zap_name);
	if (ret)
		return ret;

	/*
	 * Take the GPU out of secure mode. Try the zap shader if it is loaded,
	 * otherwise just try to write directly to the secure control register
	 */
	if (!adreno_dev->zap_loaded)
		kgsl_regwrite(device, GEN7_RBBM_SECVID_TRUST_CNTL, 0);
	else {
		cmds = adreno_ringbuffer_allocspace(rb, 2);
		if (IS_ERR(cmds))
			return PTR_ERR(cmds);

		*cmds++ = cp_type7_packet(CP_SET_SECURE_MODE, 1);
		*cmds++ = 0;

		ret = gen7_ringbuffer_submit(rb, NULL);
		if (!ret) {
			ret = adreno_spin_idle(adreno_dev, 2000);
			if (ret) {
				gen7_spin_idle_debug(adreno_dev,
					"Switch to unsecure failed to idle\n");
				return ret;
			}
		}
	}

	return gen7_post_start(adreno_dev);
}

/*
 * gen7_gpu_keepalive() - GMU reg write to request GPU stays on
 * @adreno_dev: Pointer to the adreno device that has the GMU
 * @state: State to set: true is ON, false is OFF
 */
static void gen7_gpu_keepalive(struct adreno_device *adreno_dev,
		bool state)
{
	gmu_core_regwrite(KGSL_DEVICE(adreno_dev),
			GEN7_GMU_GMU_PWR_COL_KEEPALIVE, state);
}

bool gen7_hw_isidle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg;

	gmu_core_regread(device, GEN7_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &reg);

	/* Bit 23 is GPUBUSYIGNAHB */
	return (reg & BIT(23)) ? false : true;
}

int gen7_microcode_read(struct adreno_device *adreno_dev)
{
	struct adreno_firmware *sqe_fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
	const struct adreno_gen7_core *gen7_core = to_gen7_core(adreno_dev);

	return adreno_get_firmware(adreno_dev, gen7_core->sqefw_name, sqe_fw);
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

static void gen7_cp_hw_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status1, status2;
	struct device *dev = device->dev;
	unsigned int opcode;

	kgsl_regread(device, GEN7_CP_INTERRUPT_STATUS, &status1);

	if (status1 & BIT(CP_INT_OPCODEERROR)) {
		kgsl_regwrite(device, GEN7_CP_SQE_STAT_ADDR, 1);
		kgsl_regread(device, GEN7_CP_SQE_STAT_DATA, &opcode);
		dev_crit_ratelimited(dev,
			"CP opcode error interrupt | opcode=0x%8.8x\n", opcode);
	}

	if (status1 & BIT(CP_INT_UCODEERROR))
		dev_crit_ratelimited(dev, "CP ucode error interrupt\n");

	if (status1 & BIT(CP_INT_CPHWFAULT)) {
		kgsl_regread(device, GEN7_CP_HW_FAULT, &status2);
		dev_crit_ratelimited(dev,
			"CP | Ringbuffer HW fault | status=%x\n", status2);
	}

	if (status1 & BIT(CP_INT_REGISTERPROTECTION)) {
		kgsl_regread(device, GEN7_CP_PROTECT_STATUS, &status2);
		dev_crit_ratelimited(dev,
			"CP | Protected mode error | %s | addr=%x | status=%x\n",
			status2 & BIT(20) ? "READ" : "WRITE",
			status2 & 0x3ffff, status2);
	}

	if (status1 & BIT(CP_INT_VSDPARITYERROR))
		dev_crit_ratelimited(dev, "CP VSD decoder parity error\n");

	if (status1 & BIT(CP_INT_ILLEGALINSTRUCTION))
		dev_crit_ratelimited(dev, "CP Illegal instruction error\n");

	if (adreno_is_gen7_3_0(adreno_dev))
		return;

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

	if (status1 & BIT(CP_INT_OPCODEERRORBV)) {
		kgsl_regwrite(device, GEN7_CP_BV_SQE_STAT_ADDR, 1);
		kgsl_regread(device, GEN7_CP_BV_SQE_STAT_DATA, &opcode);
		dev_crit_ratelimited(dev, "CP opcode error BV | opcode=0x%8.8x\n", opcode);
	}

	if (status1 & BIT(CP_INT_UCODEERRORBV))
		dev_crit_ratelimited(dev, "CP ucode error BV\n");

	if (status1 & BIT(CP_INT_CPHWFAULTBV)) {
		kgsl_regread(device, GEN7_CP_BV_HW_FAULT, &status2);
		dev_crit_ratelimited(dev,
			"CP BV | Ringbuffer HW fault | status=%x\n", status2);
	}

	if (status1 & BIT(CP_INT_REGISTERPROTECTIONBV)) {
		kgsl_regread(device, GEN7_CP_BV_PROTECT_STATUS, &status2);
		dev_crit_ratelimited(dev,
			"CP BV | Protected mode error | %s | addr=%x | status=%x\n",
			status2 & BIT(20) ? "READ" : "WRITE",
			status2 & 0x3ffff, status2);
	}

	if (status1 & BIT(CP_INT_ILLEGALINSTRUCTIONBV))
		dev_crit_ratelimited(dev, "CP illegal instruction BV\n");
}

static void gen7_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct device *dev = device->dev;

	switch (bit) {
	case GEN7_INT_AHBERROR:
		{
		u32 err_details_0, err_details_1;

		kgsl_regread(device, GEN7_CP_RL_ERROR_DETAILS_0, &err_details_0);
		kgsl_regread(device, GEN7_CP_RL_ERROR_DETAILS_1, &err_details_1);
		dev_crit_ratelimited(dev,
			"CP: AHB bus error, CP_RL_ERROR_DETAILS_0:0x%x CP_RL_ERROR_DETAILS_1:0x%x\n",
			err_details_0, err_details_1);
		break;
		}
	case GEN7_INT_ATBASYNCFIFOOVERFLOW:
		dev_crit_ratelimited(dev, "RBBM: ATB ASYNC overflow\n");
		break;
	case GEN7_INT_ATBBUSOVERFLOW:
		dev_crit_ratelimited(dev, "RBBM: ATB bus overflow\n");
		break;
	case GEN7_INT_OUTOFBOUNDACCESS:
		dev_crit_ratelimited(dev, "UCHE: Out of bounds access\n");
		break;
	case GEN7_INT_UCHETRAPINTERRUPT:
		dev_crit_ratelimited(dev, "UCHE: Trap interrupt\n");
		break;
	case GEN7_INT_TSBWRITEERROR:
		{
		u32 lo, hi;

		kgsl_regread(device, GEN7_RBBM_SECVID_TSB_STATUS_LO, &lo);
		kgsl_regread(device, GEN7_RBBM_SECVID_TSB_STATUS_HI, &hi);

		dev_crit_ratelimited(dev, "TSB: Write error interrupt: Address: 0x%llx MID: %d\n",
			FIELD_GET(GENMASK(16, 0), hi) << 32 | lo,
			FIELD_GET(GENMASK(31, 23), hi));
		break;
		}
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

static const char *const uche_lpac_client[] = {
	"-", "SP_LPAC", "-", "-", "HLSQ_LPAC", "-", "-", "TP_LPAC"
};

#define SCOOBYDOO 0x5c00bd00

static const char *gen7_fault_block_uche(struct kgsl_device *device,
		char *str, int size, bool lpac)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int uche_client_id = adreno_dev->uche_client_pf;
	const char *uche_client_str, *fault_block;

	/*
	 * Smmu driver takes a vote on CX gdsc before calling the kgsl
	 * pagefault handler. If there is contention for device mutex in this
	 * path and the dispatcher fault handler is holding this lock, trying
	 * to turn off CX gdsc will fail during the reset. So to avoid blocking
	 * here, try to lock device mutex and return if it fails.
	 */
	if (!mutex_trylock(&device->mutex))
		goto regread_fail;

	if (!kgsl_state_is_awake(device)) {
		mutex_unlock(&device->mutex);
		goto regread_fail;
	}

	kgsl_regread(device, GEN7_UCHE_CLIENT_PF, &uche_client_id);
	mutex_unlock(&device->mutex);

	/* Ignore the value if the gpu is in IFPC */
	if (uche_client_id == SCOOBYDOO) {
		uche_client_id = adreno_dev->uche_client_pf;
		goto regread_fail;
	}

	/* UCHE client id mask is bits [6:0] */
	uche_client_id &= GENMASK(6, 0);

regread_fail:
	if (lpac) {
		fault_block = "UCHE_LPAC";
		if (uche_client_id >= ARRAY_SIZE(uche_lpac_client))
			goto fail;
		uche_client_str = uche_lpac_client[uche_client_id];
	} else {
		fault_block = "UCHE";
		if (uche_client_id >= ARRAY_SIZE(uche_client))
			goto fail;
		uche_client_str = uche_client[uche_client_id];
	}

	snprintf(str, size, "%s: %s", fault_block, uche_client_str);
	return str;

fail:
	snprintf(str, size, "%s: Unknown (client_id: %u)",
			fault_block, uche_client_id);
	return str;
}

static const char *gen7_iommu_fault_block(struct kgsl_device *device,
		unsigned int fsynr1)
{
	unsigned int mid = fsynr1 & 0xff;
	static char str[36];

	switch (mid) {
	case 0x0:
		return "CP";
	case 0x1:
		return "UCHE: Unknown";
	case 0x2:
		return "UCHE_LPAC: Unknown";
	case 0x3:
		return gen7_fault_block_uche(device, str, sizeof(str), false);
	case 0x4:
		return "CCU";
	case 0x5:
		return "Flag cache";
	case 0x6:
		return "PREFETCH";
	case 0x7:
		return "GMU";
	case 0x8:
		return gen7_fault_block_uche(device, str, sizeof(str), true);
	}

	snprintf(str, sizeof(str), "Unknown (mid: %u)", mid);
	return str;
}

static void gen7_cp_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_is_preemption_enabled(adreno_dev))
		gen7_preemption_trigger(adreno_dev, true);

	adreno_dispatcher_schedule(device);
}

/*
 * gen7_gpc_err_int_callback() - Isr for GPC error interrupts
 * @adreno_dev: Pointer to device
 * @bit: Interrupt bit
 */
static void gen7_gpc_err_int_callback(struct adreno_device *adreno_dev, int bit)
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
	adreno_dispatcher_fault(adreno_dev, ADRENO_SOFT_FAULT);
}

static const struct adreno_irq_funcs gen7_irq_funcs[32] = {
	ADRENO_IRQ_CALLBACK(NULL), /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(gen7_err_callback), /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 2 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 3 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 4 - CPIPCINT0 */
	ADRENO_IRQ_CALLBACK(NULL), /* 5 - CPIPCINT1 */
	ADRENO_IRQ_CALLBACK(gen7_err_callback), /* 6 - ATBASYNCOVERFLOW */
	ADRENO_IRQ_CALLBACK(gen7_gpc_err_int_callback), /* 7 - GPC_ERR */
	ADRENO_IRQ_CALLBACK(gen7_preemption_callback),/* 8 - CP_SW */
	ADRENO_IRQ_CALLBACK(gen7_cp_hw_err_callback), /* 9 - CP_HW_ERROR */
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
	ADRENO_IRQ_CALLBACK(gen7_cp_callback), /* 20 - CP_CACHE_FLUSH_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 21 - CP_CACHE_TS_LPAC */
	ADRENO_IRQ_CALLBACK(gen7_err_callback), /* 22 - RBBM_ATB_BUS_OVERFLOW */
	ADRENO_IRQ_CALLBACK(adreno_hang_int_callback), /* 23 - MISHANGDETECT */
	ADRENO_IRQ_CALLBACK(gen7_err_callback), /* 24 - UCHE_OOB_ACCESS */
	ADRENO_IRQ_CALLBACK(gen7_err_callback), /* 25 - UCHE_TRAP_INTR */
	ADRENO_IRQ_CALLBACK(NULL), /* 26 - DEBBUS_INTR_0 */
	ADRENO_IRQ_CALLBACK(NULL), /* 27 - DEBBUS_INTR_1 */
	ADRENO_IRQ_CALLBACK(gen7_err_callback), /* 28 - TSBWRITEERROR */
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
static int gen7_irq_poll_fence(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 status, fence, fence_retries = 0;
	u64 a, b, c;

	a = gen7_read_alwayson(adreno_dev);

	kgsl_regread(device, GEN7_GMU_AO_AHB_FENCE_CTRL, &fence);

	while (fence != 0) {
		b = gen7_read_alwayson(adreno_dev);

		/* Wait for small time before trying again */
		udelay(1);
		kgsl_regread(device, GEN7_GMU_AO_AHB_FENCE_CTRL, &fence);

		if (fence_retries == 100 && fence != 0) {
			c = gen7_read_alwayson(adreno_dev);

			kgsl_regread(device, GEN7_GMU_RBBM_INT_UNMASKED_STATUS,
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

static irqreturn_t gen7_irq_handler(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	irqreturn_t ret = IRQ_NONE;
	u32 status;

	/*
	 * GPU can power down once the INT_0_STATUS is read below.
	 * But there still might be some register reads required so
	 * force the GMU/GPU into KEEPALIVE mode until done with the ISR.
	 */
	gen7_gpu_keepalive(adreno_dev, true);

	if (gen7_irq_poll_fence(adreno_dev)) {
		adreno_dispatcher_fault(adreno_dev, ADRENO_GMU_FAULT);
		goto done;
	}

	kgsl_regread(device, GEN7_RBBM_INT_0_STATUS, &status);

	kgsl_regwrite(device, GEN7_RBBM_INT_CLEAR_CMD, status);

	ret = adreno_irq_callbacks(adreno_dev, gen7_irq_funcs, status);

	trace_kgsl_gen7_irq_status(adreno_dev, status);

done:
	/* If hard fault, then let snapshot turn off the keepalive */
	if (!(adreno_gpu_fault(adreno_dev) & ADRENO_HARD_FAULT))
		gen7_gpu_keepalive(adreno_dev, false);

	return ret;
}

int gen7_probe_common(struct platform_device *pdev,
	struct adreno_device *adreno_dev, u32 chipid,
	const struct adreno_gpu_core *gpucore)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_gpudev *gpudev = gpucore->gpudev;
	const struct adreno_gen7_core *gen7_core = container_of(gpucore,
			struct adreno_gen7_core, base);

	adreno_dev->gpucore = gpucore;
	adreno_dev->chipid = chipid;

	adreno_reg_offset_init(gpudev->reg_offsets);

	adreno_dev->hwcg_enabled = true;
	adreno_dev->uche_client_pf = 1;

	adreno_dev->preempt.preempt_level = 1;
	adreno_dev->preempt.skipsaverestore = true;
	adreno_dev->preempt.usesgmem = true;

	device->pwrctrl.rt_bus_hint = gen7_core->rt_bus_hint;
	kgsl_pwrscale_fast_bus_hint(gen7_core->fast_bus_hint);

	if (adreno_is_gen7_3_0(adreno_dev))
		adreno_drawobj_timeout = 4500;

	return adreno_device_probe(pdev, adreno_dev);
}

/* Register offset defines for Gen7, in order of enum adreno_regs */
static unsigned int gen7_register_offsets[ADRENO_REG_REGISTER_MAX] = {
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, GEN7_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE_HI, GEN7_CP_RB_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, GEN7_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, GEN7_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_CNTL, GEN7_CP_SQE_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE, GEN7_CP_IB1_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE_HI, GEN7_CP_IB1_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BUFSZ, GEN7_CP_IB1_REM_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE, GEN7_CP_IB2_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE_HI, GEN7_CP_IB2_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BUFSZ, GEN7_CP_IB2_REM_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, GEN7_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS3, GEN7_RBBM_STATUS3),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, GEN7_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, GEN7_RBBM_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
			GEN7_GMU_AO_HOST_INTERRUPT_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
			GEN7_GMU_GMU2HOST_INTR_MASK),
};

int gen7_perfcounter_update(struct adreno_device *adreno_dev,
	struct adreno_perfcount_register *reg, bool update_reg, u32 pipe)
{
	void *ptr = adreno_dev->pwrup_reglist->hostptr;
	struct cpu_gpu_lock *lock = ptr;
	u32 *data = ptr + sizeof(*lock);
	int i, offset = (lock->ifpc_list_len + lock->preemption_list_len) * 2;

	if (kgsl_hwlock(lock)) {
		kgsl_hwunlock(lock);
		return -EBUSY;
	}

	/*
	 * If the perfcounter select register is already present in reglist
	 * update it, otherwise append the <aperture, select register, value>
	 * triplet to the end of the list.
	 */
	for (i = 0; i < lock->dynamic_list_len; i++) {
		if ((data[offset + 1] == reg->select) && (data[offset] == pipe)) {
			data[offset + 2] = reg->countable;
			goto update;
		}

		if (data[offset + 1] == GEN7_RBBM_PERFCTR_CNTL)
			break;

		offset += 3;
	}

	/*
	 * For all targets GEN7_RBBM_PERFCTR_CNTL needs to be the last entry,
	 * so overwrite the existing GEN7_RBBM_PERFCNTL_CTRL and add it back to
	 * the end.
	 */
	data[offset++] = pipe;
	data[offset++] = reg->select;
	data[offset++] = reg->countable;

	data[offset++] = FIELD_PREP(GENMASK(13, 12), PIPE_NONE);
	data[offset++] = GEN7_RBBM_PERFCTR_CNTL;
	data[offset++] = 1;

	lock->dynamic_list_len++;

update:
	if (update_reg)
		kgsl_regwrite(KGSL_DEVICE(adreno_dev), reg->select,
			reg->countable);

	kgsl_hwunlock(lock);
	return 0;
}

u64 gen7_read_alwayson(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 lo = 0, hi = 0, tmp = 0;

	/* Always use the GMU AO counter when doing a AHB read */
	gmu_core_regread(device, GEN7_GMU_ALWAYS_ON_COUNTER_H, &hi);
	gmu_core_regread(device, GEN7_GMU_ALWAYS_ON_COUNTER_L, &lo);

	/* Check for overflow */
	gmu_core_regread(device, GEN7_GMU_ALWAYS_ON_COUNTER_H, &tmp);

	if (hi != tmp) {
		gmu_core_regread(device, GEN7_GMU_ALWAYS_ON_COUNTER_L,
				&lo);
		hi = tmp;
	}

	return (((u64) hi) << 32) | lo;
}

static void gen7_remove(struct adreno_device *adreno_dev)
{
	if (adreno_is_preemption_enabled(adreno_dev))
		del_timer(&adreno_dev->preempt.timer);
}

static void gen7_read_bus_stats(struct kgsl_device *device,
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

static void gen7_power_stats(struct adreno_device *adreno_dev,
		struct kgsl_power_stats *stats)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_busy_data *busy = &adreno_dev->busy_data;
	u64 gpu_busy;

	/* Set the GPU busy counter for frequency scaling */
	gpu_busy = counter_delta(device, GEN7_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_L,
		&busy->gpu_busy);

	stats->busy_time = gpu_busy * 10;
	do_div(stats->busy_time, 192);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_IFPC)) {
		u32 ifpc = counter_delta(device,
			GEN7_GMU_CX_GMU_POWER_COUNTER_XOCLK_4_L,
			&busy->num_ifpc);

		adreno_dev->ifpc_count += ifpc;
		if (ifpc > 0)
			trace_adreno_ifpc_count(adreno_dev->ifpc_count);
	}

	if (device->pwrctrl.bus_control)
		gen7_read_bus_stats(device, stats, busy);

	if (adreno_dev->bcl_enabled) {
		u32 a, b, c;

		a = counter_delta(device, GEN7_GMU_CX_GMU_POWER_COUNTER_XOCLK_1_L,
			&busy->throttle_cycles[0]);

		b = counter_delta(device, GEN7_GMU_CX_GMU_POWER_COUNTER_XOCLK_2_L,
			&busy->throttle_cycles[1]);

		c = counter_delta(device, GEN7_GMU_CX_GMU_POWER_COUNTER_XOCLK_3_L,
			&busy->throttle_cycles[2]);

		if (a || b || c)
			trace_kgsl_bcl_clock_throttling(a, b, c);

		if (adreno_is_gen7_6_0(adreno_dev)) {
			u32 bcl_throttle = counter_delta(device,
				GEN7_GMU_CX_GMU_POWER_COUNTER_XOCLK_5_L, &busy->bcl_throttle);
			/*
			 * This counts number of cycles throttled in XO cycles. Convert it to
			 * micro seconds by dividing by XO freq which is 19.2MHz.
			 */
			adreno_dev->bcl_throttle_time_us += ((bcl_throttle * 10) / 192);
		}
	}
}

static int gen7_setproperty(struct kgsl_device_private *dev_priv,
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

const struct gen7_gpudev adreno_gen7_hwsched_gpudev = {
	.base = {
		.reg_offsets = gen7_register_offsets,
		.probe = gen7_hwsched_probe,
		.snapshot = gen7_hwsched_snapshot,
		.irq_handler = gen7_irq_handler,
		.iommu_fault_block = gen7_iommu_fault_block,
		.preemption_context_init = gen7_preemption_context_init,
		.context_detach = gen7_hwsched_context_detach,
		.read_alwayson = gen7_read_alwayson,
		.reset = gen7_hwsched_reset,
		.power_ops = &gen7_hwsched_power_ops,
		.power_stats = gen7_power_stats,
		.setproperty = gen7_setproperty,
		.hw_isidle = gen7_hw_isidle,
		.add_to_va_minidump = gen7_hwsched_add_to_minidump,
		.gx_is_on = gen7_gmu_gx_is_on,
		.send_recurring_cmdobj = gen7_hwsched_send_recurring_cmdobj,
		.context_destroy = gen7_hwsched_context_destroy,
	},
	.hfi_probe = gen7_hwsched_hfi_probe,
	.hfi_remove = gen7_hwsched_hfi_remove,
	.handle_watchdog = gen7_hwsched_handle_watchdog,
};

const struct gen7_gpudev adreno_gen7_gmu_gpudev = {
	.base = {
		.reg_offsets = gen7_register_offsets,
		.probe = gen7_gmu_device_probe,
		.snapshot = gen7_gmu_snapshot,
		.irq_handler = gen7_irq_handler,
		.rb_start = gen7_rb_start,
		.gpu_keepalive = gen7_gpu_keepalive,
		.hw_isidle = gen7_hw_isidle,
		.iommu_fault_block = gen7_iommu_fault_block,
		.reset = gen7_gmu_reset,
		.preemption_schedule = gen7_preemption_schedule,
		.preemption_context_init = gen7_preemption_context_init,
		.read_alwayson = gen7_read_alwayson,
		.power_ops = &gen7_gmu_power_ops,
		.remove = gen7_remove,
		.ringbuffer_submitcmd = gen7_ringbuffer_submitcmd,
		.power_stats = gen7_power_stats,
		.setproperty = gen7_setproperty,
		.add_to_va_minidump = gen7_gmu_add_to_minidump,
		.gx_is_on = gen7_gmu_gx_is_on,
	},
	.hfi_probe = gen7_gmu_hfi_probe,
	.handle_watchdog = gen7_gmu_handle_watchdog,
};
