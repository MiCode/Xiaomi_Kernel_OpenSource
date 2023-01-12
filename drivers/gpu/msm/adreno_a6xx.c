// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/clk/qcom.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_device.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/qcom/llcc-qcom.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_a6xx_hwsched.h"
#include "adreno_pm4types.h"
#include "adreno_trace.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

/* IFPC & Preemption static powerup restore list */
static u32 a6xx_pwrup_reglist[] = {
	A6XX_VSC_ADDR_MODE_CNTL,
	A6XX_GRAS_ADDR_MODE_CNTL,
	A6XX_RB_ADDR_MODE_CNTL,
	A6XX_PC_ADDR_MODE_CNTL,
	A6XX_HLSQ_ADDR_MODE_CNTL,
	A6XX_VFD_ADDR_MODE_CNTL,
	A6XX_VPC_ADDR_MODE_CNTL,
	A6XX_UCHE_ADDR_MODE_CNTL,
	A6XX_SP_ADDR_MODE_CNTL,
	A6XX_TPL1_ADDR_MODE_CNTL,
	A6XX_UCHE_WRITE_RANGE_MAX_LO,
	A6XX_UCHE_WRITE_RANGE_MAX_HI,
	A6XX_UCHE_TRAP_BASE_LO,
	A6XX_UCHE_TRAP_BASE_HI,
	A6XX_UCHE_WRITE_THRU_BASE_LO,
	A6XX_UCHE_WRITE_THRU_BASE_HI,
	A6XX_UCHE_GMEM_RANGE_MIN_LO,
	A6XX_UCHE_GMEM_RANGE_MIN_HI,
	A6XX_UCHE_GMEM_RANGE_MAX_LO,
	A6XX_UCHE_GMEM_RANGE_MAX_HI,
	A6XX_UCHE_FILTER_CNTL,
	A6XX_UCHE_CACHE_WAYS,
	A6XX_UCHE_MODE_CNTL,
	A6XX_RB_NC_MODE_CNTL,
	A6XX_TPL1_NC_MODE_CNTL,
	A6XX_SP_NC_MODE_CNTL,
	A6XX_PC_DBG_ECO_CNTL,
	A6XX_RB_CONTEXT_SWITCH_GMEM_SAVE_RESTORE,
	A6XX_UCHE_GBIF_GX_CONFIG,
};

/* IFPC only static powerup restore list */
static u32 a6xx_ifpc_pwrup_reglist[] = {
	A6XX_CP_CHICKEN_DBG,
	A6XX_CP_DBG_ECO_CNTL,
	A6XX_CP_PROTECT_CNTL,
	A6XX_CP_PROTECT_REG,
	A6XX_CP_PROTECT_REG+1,
	A6XX_CP_PROTECT_REG+2,
	A6XX_CP_PROTECT_REG+3,
	A6XX_CP_PROTECT_REG+4,
	A6XX_CP_PROTECT_REG+5,
	A6XX_CP_PROTECT_REG+6,
	A6XX_CP_PROTECT_REG+7,
	A6XX_CP_PROTECT_REG+8,
	A6XX_CP_PROTECT_REG+9,
	A6XX_CP_PROTECT_REG+10,
	A6XX_CP_PROTECT_REG+11,
	A6XX_CP_PROTECT_REG+12,
	A6XX_CP_PROTECT_REG+13,
	A6XX_CP_PROTECT_REG+14,
	A6XX_CP_PROTECT_REG+15,
	A6XX_CP_PROTECT_REG+16,
	A6XX_CP_PROTECT_REG+17,
	A6XX_CP_PROTECT_REG+18,
	A6XX_CP_PROTECT_REG+19,
	A6XX_CP_PROTECT_REG+20,
	A6XX_CP_PROTECT_REG+21,
	A6XX_CP_PROTECT_REG+22,
	A6XX_CP_PROTECT_REG+23,
	A6XX_CP_PROTECT_REG+24,
	A6XX_CP_PROTECT_REG+25,
	A6XX_CP_PROTECT_REG+26,
	A6XX_CP_PROTECT_REG+27,
	A6XX_CP_PROTECT_REG+28,
	A6XX_CP_PROTECT_REG+29,
	A6XX_CP_PROTECT_REG+30,
	A6XX_CP_PROTECT_REG+31,
	A6XX_CP_AHB_CNTL,
};

/* Applicable to a620, a642, a642l, a650 and a660 */
static u32 a650_ifpc_pwrup_reglist[] = {
	A6XX_CP_PROTECT_REG+32,
	A6XX_CP_PROTECT_REG+33,
	A6XX_CP_PROTECT_REG+34,
	A6XX_CP_PROTECT_REG+35,
	A6XX_CP_PROTECT_REG+36,
	A6XX_CP_PROTECT_REG+37,
	A6XX_CP_PROTECT_REG+38,
	A6XX_CP_PROTECT_REG+39,
	A6XX_CP_PROTECT_REG+40,
	A6XX_CP_PROTECT_REG+41,
	A6XX_CP_PROTECT_REG+42,
	A6XX_CP_PROTECT_REG+43,
	A6XX_CP_PROTECT_REG+44,
	A6XX_CP_PROTECT_REG+45,
	A6XX_CP_PROTECT_REG+46,
	A6XX_CP_PROTECT_REG+47,
};

/* Applicable to a620, a635, a650 and a660 */
static u32 a650_pwrup_reglist[] = {
	A6XX_CP_PROTECT_REG + 47,          /* Programmed for infinite span */
	A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_0,
	A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_1,
	A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_2,
	A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_3,
	A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_4,
	A6XX_UCHE_CMDQ_CONFIG,
};

static u32 a615_pwrup_reglist[] = {
	A6XX_UCHE_GBIF_GX_CONFIG,
};

static int a6xx_get_cp_init_cmds(struct adreno_device *adreno_dev);

static int a6xx_gmu_wrapper_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct resource *res;

	if (adreno_dev->gmu_wrapper_virt)
		return 0;

	res = platform_get_resource_byname(device->pdev,
		IORESOURCE_MEM, "gmu_wrapper");
	if (!res) {
		dev_err(device->dev, "gmu_wrapper platform_get_resource_byname failed\n");
		return -ENODEV;
	}

	adreno_dev->gmu_wrapper_base = res->start - device->reg_phys;
	adreno_dev->gmu_wrapper_virt = devm_ioremap(&device->pdev->dev,
		res->start, resource_size(res));

	if (!adreno_dev->gmu_wrapper_virt) {
		dev_err(device->dev, "gmu_wrapper ioremap failed\n");
		return -ENODEV;
	}

	return 0;
}

static int match_name(struct device *dev, void *data)
{
	struct device *parent = data;

	return (!strcmp(dev_name(dev), dev_name(parent)));
}

static void find_ddr_qos_device(struct adreno_device *adreno_dev)
{
	struct device *devfreq_dev, *ddr_qos_dev;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (device->pwrctrl.ddr_qos_devfreq)
		return;

	ddr_qos_dev = bus_find_device_by_name(&platform_bus_type, NULL,
			"soc:qcom,kgsl-ddr-qos");

	if (!ddr_qos_dev)
		return;

	/* Devfreq device has the same name as its parent device */
	devfreq_dev = device_find_child(ddr_qos_dev, ddr_qos_dev, match_name);
	if (!devfreq_dev)
		return;

	device->pwrctrl.ddr_qos_devfreq = container_of(devfreq_dev,
					struct devfreq, dev);
}

int a6xx_init(struct adreno_device *adreno_dev)
{
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);

	adreno_dev->highest_bank_bit = a6xx_core->highest_bank_bit;

	/* If the memory type is DDR 4, override the existing configuration */
	if (of_fdt_get_ddrtype() == 0x7) {
		if (adreno_is_a642(adreno_dev) ||
			adreno_is_a642l(adreno_dev) ||
			adreno_is_a643(adreno_dev))
			adreno_dev->highest_bank_bit = 14;
		else if ((adreno_is_a650(adreno_dev) ||
				adreno_is_a660(adreno_dev)))
			adreno_dev->highest_bank_bit = 15;
	}

	a6xx_crashdump_init(adreno_dev);

	if (IS_ERR_OR_NULL(adreno_dev->pwrup_reglist)) {
		adreno_dev->pwrup_reglist =
				kgsl_allocate_global(KGSL_DEVICE(adreno_dev),
					PAGE_SIZE, 0, 0, KGSL_MEMDESC_PRIVILEGED,
					"powerup_register_list");
		if (IS_ERR(adreno_dev->pwrup_reglist))
			return PTR_ERR(adreno_dev->pwrup_reglist);
	}

	find_ddr_qos_device(adreno_dev);

	return a6xx_get_cp_init_cmds(adreno_dev);
}

static int a6xx_holi_init(struct adreno_device *adreno_dev)
{
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	int ret;

	ret = a6xx_gmu_wrapper_init(adreno_dev);
	if (ret)
		return ret;

	adreno_dev->highest_bank_bit = a6xx_core->highest_bank_bit;

	a6xx_crashdump_init(adreno_dev);

	if (IS_ERR_OR_NULL(adreno_dev->pwrup_reglist)) {
		adreno_dev->pwrup_reglist =
				kgsl_allocate_global(KGSL_DEVICE(adreno_dev),
					PAGE_SIZE, 0, 0, KGSL_MEMDESC_PRIVILEGED,
					"powerup_register_list");
		if (IS_ERR(adreno_dev->pwrup_reglist))
			return PTR_ERR(adreno_dev->pwrup_reglist);
	}

	find_ddr_qos_device(adreno_dev);

	return a6xx_get_cp_init_cmds(adreno_dev);
}


static void a6xx_protect_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	const struct a6xx_protected_regs *regs = a6xx_core->protected_regs;
	int i;

	/*
	 * Enable access protection to privileged registers, fault on an access
	 * protect violation and select the last span to protect from the start
	 * address all the way to the end of the register address space
	 */
	kgsl_regwrite(device, A6XX_CP_PROTECT_CNTL,
		(1 << 0) | (1 << 1) | (1 << 3));

	/* Program each register defined by the core definition */
	for (i = 0; regs[i].reg; i++) {
		u32 count;

		/*
		 * This is the offset of the end register as counted from the
		 * start, i.e. # of registers in the range - 1
		 */
		count = regs[i].end - regs[i].start;

		kgsl_regwrite(device, regs[i].reg,
			(regs[i].start & 0x3ffff) | ((count & 0x1fff) << 18) |
			(regs[i].noaccess << 31));
	}
}

static inline unsigned int
__get_rbbm_clock_cntl_on(struct adreno_device *adreno_dev)
{
	if (adreno_is_a630(adreno_dev))
		return 0x8AA8AA02;
	else if (adreno_is_a612(adreno_dev) || adreno_is_a610(adreno_dev))
		return 0xAAA8AA82;
	else if (adreno_is_a702(adreno_dev))
		return 0xAAAAAA82;
	else
		return 0x8AA8AA82;
}

static inline unsigned int
__get_gmu_ao_cgc_mode_cntl(struct adreno_device *adreno_dev)
{
	if (adreno_is_a612(adreno_dev))
		return 0x00000022;
	else if (adreno_is_a615_family(adreno_dev))
		return 0x00000222;
	else if (adreno_is_a660(adreno_dev))
		return 0x00020000;
	else
		return 0x00020202;
}

static inline unsigned int
__get_gmu_ao_cgc_delay_cntl(struct adreno_device *adreno_dev)
{
	if (adreno_is_a612(adreno_dev))
		return 0x00000011;
	else if (adreno_is_a615_family(adreno_dev))
		return 0x00000111;
	else
		return 0x00010111;
}

static inline unsigned int
__get_gmu_ao_cgc_hyst_cntl(struct adreno_device *adreno_dev)
{
	if (adreno_is_a612(adreno_dev))
		return 0x00000055;
	else if (adreno_is_a615_family(adreno_dev))
		return 0x00000555;
	else
		return 0x00005555;
}

static unsigned int __get_gmu_wfi_config(struct adreno_device *adreno_dev)
{
	if (adreno_is_a620(adreno_dev) || adreno_is_a640(adreno_dev) ||
		adreno_is_a650(adreno_dev))
		return 0x00000002;

	return 0x00000000;
}

static bool __disable_cx_regulator_wait(struct regulator *reg,
				struct kgsl_device *device, u32 timeout)
{
	ktime_t tout = ktime_add_us(ktime_get(), timeout * 1000);
	unsigned int val;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	regulator_disable(reg);

	if (IS_ENABLED(CONFIG_ARM_SMMU_POWER_ALWAYS_ON))
		return true;

	for (;;) {
		if (adreno_is_a619_holi(adreno_dev))
			adreno_read_gmu_wrapper(adreno_dev,
					A6XX_GPU_CC_CX_GDSCR, &val);
		else
			gmu_core_regread(device, A6XX_GPU_CC_CX_GDSCR, &val);

		if (!(val & BIT(31)))
			return true;

		if (ktime_compare(ktime_get(), tout) > 0) {
			if (adreno_is_a619_holi(adreno_dev))
				adreno_read_gmu_wrapper(adreno_dev,
						A6XX_GPU_CC_CX_GDSCR, &val);
			else
				gmu_core_regread(device, A6XX_GPU_CC_CX_GDSCR,
							&val);
			return (!(val & BIT(31)));
		}

		usleep_range((100 >> 2) + 1, 100);
	}
}

void a6xx_cx_regulator_disable_wait(struct regulator *reg,
				struct kgsl_device *device, u32 timeout)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (IS_ERR_OR_NULL(reg))
		return;

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CX_GDSC))
		regulator_set_mode(reg, REGULATOR_MODE_IDLE);

	if (!__disable_cx_regulator_wait(reg, device, timeout)) {
		dev_err(device->dev, "GPU CX wait timeout. Dumping CX votes:\n");
		/* Dump the cx regulator consumer list */
		qcom_clk_dump(NULL, reg, false);
	}

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CX_GDSC))
		regulator_set_mode(reg, REGULATOR_MODE_NORMAL);
}

static void set_holi_sptprac_clock(struct adreno_device *adreno_dev, bool enable)
{
	u32 val = 0;

	adreno_read_gmu_wrapper(adreno_dev,
			A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL, &val);
	val &= ~1;
	adreno_write_gmu_wrapper(adreno_dev,
			A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL,
			val | (enable ? 1 : 0));
}

static void a6xx_hwcg_set(struct adreno_device *adreno_dev, bool on)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	unsigned int value;
	int i;

	if (!adreno_dev->hwcg_enabled)
		on = false;

	if (gmu_core_isenabled(device)) {
		gmu_core_regwrite(device, A6XX_GPU_GMU_AO_GMU_CGC_MODE_CNTL,
			on ? __get_gmu_ao_cgc_mode_cntl(adreno_dev) : 0);
		gmu_core_regwrite(device, A6XX_GPU_GMU_AO_GMU_CGC_DELAY_CNTL,
			on ? __get_gmu_ao_cgc_delay_cntl(adreno_dev) : 0);
		gmu_core_regwrite(device, A6XX_GPU_GMU_AO_GMU_CGC_HYST_CNTL,
			on ? __get_gmu_ao_cgc_hyst_cntl(adreno_dev) : 0);
		gmu_core_regwrite(device, A6XX_GMU_CX_GMU_WFI_CONFIG,
			on ? __get_gmu_wfi_config(adreno_dev) : 0);
	}

	kgsl_regread(device, A6XX_RBBM_CLOCK_CNTL, &value);

	if (value == __get_rbbm_clock_cntl_on(adreno_dev) && on)
		return;

	if (value == 0 && !on)
		return;

	/*
	 * Disable SP clock before programming HWCG registers.
	 * A612 and A610 GPU is not having the GX power domain.
	 * Hence skip GMU_GX registers for A12 and A610.
	 */

	if (gmu_core_isenabled(device) && !adreno_is_a612(adreno_dev) &&
		!adreno_is_a610(adreno_dev) && !(adreno_is_a702(adreno_dev)))
		gmu_core_regrmw(device,
			A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL, 1, 0);
	else if (adreno_is_a619_holi(adreno_dev))
		set_holi_sptprac_clock(adreno_dev, false);

	for (i = 0; i < a6xx_core->hwcg_count; i++)
		kgsl_regwrite(device, a6xx_core->hwcg[i].offset,
			on ? a6xx_core->hwcg[i].value : 0);

	/* GBIF L2 CGC control is not part of the UCHE */
	kgsl_regrmw(device, A6XX_UCHE_GBIF_GX_CONFIG, 0x70000, on ? 2 : 0);

	/*
	 * Enable SP clock after programming HWCG registers.
	 * A612 and A610 GPU is not having the GX power domain.
	 * Hence skip GMU_GX registers for A612.
	 */
	if (gmu_core_isenabled(device) && !adreno_is_a612(adreno_dev) &&
		!adreno_is_a610(adreno_dev) && !adreno_is_a702(adreno_dev))
		gmu_core_regrmw(device,
			A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL, 0, 1);
	else if (adreno_is_a619_holi(adreno_dev))
		set_holi_sptprac_clock(adreno_dev, true);

	/* enable top level HWCG */
	kgsl_regwrite(device, A6XX_RBBM_CLOCK_CNTL,
		on ? __get_rbbm_clock_cntl_on(adreno_dev) : 0);
}

struct a6xx_reglist_list {
	u32 *regs;
	u32 count;
};

#define REGLIST(_a) \
	 (struct a6xx_reglist_list) { .regs = _a, .count = ARRAY_SIZE(_a), }

static void a6xx_patch_pwrup_reglist(struct adreno_device *adreno_dev)
{
	struct a6xx_reglist_list reglist[4];
	void *ptr = adreno_dev->pwrup_reglist->hostptr;
	struct cpu_gpu_lock *lock = ptr;
	int items = 0, i, j;
	u32 *dest = ptr + sizeof(*lock);
	u16 list_offset = 0;

	/* Static IFPC-only registers */
	reglist[items] = REGLIST(a6xx_ifpc_pwrup_reglist);
	list_offset += reglist[items++].count * 2;

	if (adreno_is_a650_family(adreno_dev)) {
		reglist[items] = REGLIST(a650_ifpc_pwrup_reglist);
		list_offset += reglist[items++].count * 2;
	}

	/* Static IFPC + preemption registers */
	reglist[items++] = REGLIST(a6xx_pwrup_reglist);

	/* Add target specific registers */
	if (adreno_is_a615_family(adreno_dev))
		reglist[items++] = REGLIST(a615_pwrup_reglist);
	else if (adreno_is_a650_family(adreno_dev))
		reglist[items++] = REGLIST(a650_pwrup_reglist);

	/*
	 * For each entry in each of the lists, write the offset and the current
	 * register value into the GPU buffer
	 */
	for (i = 0; i < items; i++) {
		u32 *r = reglist[i].regs;

		for (j = 0; j < reglist[i].count; j++) {
			*dest++ = r[j];
			kgsl_regread(KGSL_DEVICE(adreno_dev), r[j], dest++);
		}

		lock->list_length += reglist[i].count * 2;
	}

	if (adreno_is_a630(adreno_dev)) {
		*dest++ = A6XX_RBBM_VBIF_CLIENT_QOS_CNTL;
		kgsl_regread(KGSL_DEVICE(adreno_dev),
			A6XX_RBBM_VBIF_CLIENT_QOS_CNTL, dest++);
	} else {
		*dest++ = A6XX_RBBM_GBIF_CLIENT_QOS_CNTL;
		kgsl_regread(KGSL_DEVICE(adreno_dev),
			A6XX_RBBM_GBIF_CLIENT_QOS_CNTL, dest++);
	}

	lock->list_length += 2;

	*dest++ = A6XX_RBBM_PERFCTR_CNTL;
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
	lock->list_offset = list_offset;
}


static void a6xx_llc_configure_gpu_scid(struct adreno_device *adreno_dev);
static void a6xx_llc_configure_gpuhtw_scid(struct adreno_device *adreno_dev);
static void a6xx_llc_enable_overrides(struct adreno_device *adreno_dev);

static void a6xx_set_secvid(struct kgsl_device *device)
{
	static bool set;

	if (set || !device->mmu.secured)
		return;

	kgsl_regwrite(device, A6XX_RBBM_SECVID_TSB_CNTL, 0x0);
	kgsl_regwrite(device, A6XX_RBBM_SECVID_TSB_TRUSTED_BASE_LO,
		lower_32_bits(KGSL_IOMMU_SECURE_BASE(&device->mmu)));
	kgsl_regwrite(device, A6XX_RBBM_SECVID_TSB_TRUSTED_BASE_HI,
		upper_32_bits(KGSL_IOMMU_SECURE_BASE(&device->mmu)));
	kgsl_regwrite(device, A6XX_RBBM_SECVID_TSB_TRUSTED_SIZE,
		KGSL_IOMMU_SECURE_SIZE);

	if (ADRENO_QUIRK(ADRENO_DEVICE(device), ADRENO_QUIRK_SECVID_SET_ONCE))
		set = true;
}

static void a6xx_deassert_gbif_halt(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_regwrite(device, A6XX_GBIF_HALT, 0x0);

	if (adreno_is_a619_holi(adreno_dev))
		kgsl_regwrite(device, A6XX_RBBM_GPR0_CNTL, 0x0);
	else
		kgsl_regwrite(device, A6XX_RBBM_GBIF_HALT, 0x0);
}

/*
 * Some targets support marking certain transactions as always privileged which
 * allows us to mark more memory as privileged without having to explicitly set
 * the APRIV bit.  For those targets, choose the following transactions to be
 * privileged by default:
 * CDWRITE     [6:6] - Crashdumper writes
 * CDREAD      [5:5] - Crashdumper reads
 * RBRPWB      [3:3] - RPTR shadow writes
 * RBPRIVLEVEL [2:2] - Memory accesses from PM4 packets in the ringbuffer
 * RBFETCH     [1:1] - Ringbuffer reads
 */
#define A6XX_APRIV_DEFAULT \
	((1 << 6) | (1 << 5) | (1 << 3) | (1 << 2) | (1 << 1))

void a6xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	unsigned int mal, mode, hbb_hi = 0, hbb_lo = 0;
	unsigned int uavflagprd_inv;
	unsigned int amsbc = 0;
	unsigned int rgb565_predicator = 0;
	static bool patch_reglist;

	/* Enable 64 bit addressing */
	kgsl_regwrite(device, A6XX_CP_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_VSC_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_GRAS_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_RB_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_PC_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_HLSQ_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_VFD_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_VPC_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_UCHE_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_SP_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_TPL1_ADDR_MODE_CNTL, 0x1);
	kgsl_regwrite(device, A6XX_RBBM_SECVID_TSB_ADDR_MODE_CNTL, 0x1);

	/* Set up VBIF registers from the GPU core definition */
	adreno_reglist_write(adreno_dev, a6xx_core->vbif,
		a6xx_core->vbif_count);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_LIMIT_UCHE_GBIF_RW))
		kgsl_regwrite(device, A6XX_UCHE_GBIF_GX_CONFIG, 0x10200F9);

	/* Make all blocks contribute to the GPU BUSY perf counter */
	kgsl_regwrite(device, A6XX_RBBM_PERFCTR_GPU_BUSY_MASKED, 0xFFFFFFFF);

	/*
	 * Set UCHE_WRITE_THRU_BASE to the UCHE_TRAP_BASE effectively
	 * disabling L2 bypass
	 */
	kgsl_regwrite(device, A6XX_UCHE_WRITE_RANGE_MAX_LO, 0xffffffc0);
	kgsl_regwrite(device, A6XX_UCHE_WRITE_RANGE_MAX_HI, 0x0001ffff);
	kgsl_regwrite(device, A6XX_UCHE_TRAP_BASE_LO, 0xfffff000);
	kgsl_regwrite(device, A6XX_UCHE_TRAP_BASE_HI, 0x0001ffff);
	kgsl_regwrite(device, A6XX_UCHE_WRITE_THRU_BASE_LO, 0xfffff000);
	kgsl_regwrite(device, A6XX_UCHE_WRITE_THRU_BASE_HI, 0x0001ffff);

	/*
	 * Some A6xx targets no longer use a programmed UCHE GMEM base
	 * address so only write the registers if this address is
	 * non zero.
	 */
	if (adreno_dev->uche_gmem_base) {
		kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MIN_LO,
				adreno_dev->uche_gmem_base);
		kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MIN_HI, 0x0);
		kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MAX_LO,
				adreno_dev->uche_gmem_base +
				adreno_dev->gpucore->gmem_size - 1);
		kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MAX_HI, 0x0);
	}

	kgsl_regwrite(device, A6XX_UCHE_FILTER_CNTL, 0x804);
	kgsl_regwrite(device, A6XX_UCHE_CACHE_WAYS, 0x4);

	/* ROQ sizes are twice as big on a640/a680 than on a630 */
	if ((ADRENO_GPUREV(adreno_dev) >= ADRENO_REV_A640) &&
		!adreno_is_a702(adreno_dev)) {
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_2, 0x02000140);
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_1, 0x8040362C);
	} else if (adreno_is_a612(adreno_dev) || adreno_is_a610(adreno_dev) ||
			adreno_is_a702(adreno_dev)) {
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_2, 0x00800060);
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_1, 0x40201b16);
	} else {
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_2, 0x010000C0);
		kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_1, 0x8040362C);
	}

	if (adreno_is_a660(adreno_dev))
		kgsl_regwrite(device, A6XX_CP_LPAC_PROG_FIFO_SIZE, 0x00000020);

	if (adreno_is_a612(adreno_dev) || adreno_is_a610(adreno_dev)) {
		/* For A612 and A610 Mem pool size is reduced to 48 */
		kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, 48);
		kgsl_regwrite(device, A6XX_CP_MEM_POOL_DBG_ADDR, 47);
	} else if (adreno_is_a702(adreno_dev)) {
		kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, 64);
		kgsl_regwrite(device, A6XX_CP_MEM_POOL_DBG_ADDR, 63);
	} else {
		kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, 128);
	}

	/* Setting the primFifo thresholds values */
	kgsl_regwrite(device, A6XX_PC_DBG_ECO_CNTL,
		a6xx_core->prim_fifo_threshold);

	/* Set the AHB default slave response to "ERROR" */
	kgsl_regwrite(device, A6XX_CP_AHB_CNTL, 0x1);

	/* Turn on performance counters */
	kgsl_regwrite(device, A6XX_RBBM_PERFCTR_CNTL, 0x1);

	/* Turn on the IFPC counter (countable 4 on XOCLK4) */
	if (gmu_core_isenabled(device))
		gmu_core_regrmw(device, A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_1,
			0xff, 0x4);

	/* Turn on GX_MEM retention */
	if (gmu_core_isenabled(device) && adreno_is_a612(adreno_dev)) {
		kgsl_regwrite(device, A6XX_RBBM_BLOCK_GX_RETENTION_CNTL, 0x7FB);
		/* For CP IPC interrupt */
		kgsl_regwrite(device, A6XX_RBBM_INT_2_MASK, 0x00000010);
	}

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,min-access-length", &mal))
		mal = 32;

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,ubwc-mode", &mode))
		mode = 0;

	switch (mode) {
	case KGSL_UBWC_1_0:
		mode = 1;
		break;
	case KGSL_UBWC_2_0:
		mode = 0;
		break;
	case KGSL_UBWC_3_0:
		mode = 0;
		amsbc = 1; /* Only valid for A640 and A680 */
		break;
	case KGSL_UBWC_4_0:
		mode = 0;
		rgb565_predicator = 1;
		amsbc = 1;
		break;
	default:
		break;
	}

	/*
	 * For macrotiling change on a680,  will affect RB, SP and TP
	 * 0 means UBWC 3.0, 1 means UBWC 3.1
	 */
	if (adreno_is_a680(adreno_dev))
		kgsl_regwrite(device, A6XX_RBBM_NC_MODE_CNTL, 1);

	if (!WARN_ON(!adreno_dev->highest_bank_bit)) {
		hbb_lo = (adreno_dev->highest_bank_bit - 13) & 3;
		hbb_hi = ((adreno_dev->highest_bank_bit - 13) >> 2) & 1;
	}

	mal = (mal == 64) ? 1 : 0;

	uavflagprd_inv = (adreno_is_a650_family(adreno_dev)) ? 2 : 0;

	kgsl_regwrite(device, A6XX_RB_NC_MODE_CNTL, (rgb565_predicator << 11)|
				(hbb_hi << 10) | (amsbc << 4) | (mal << 3) |
				(hbb_lo << 1) | mode);

	kgsl_regwrite(device, A6XX_TPL1_NC_MODE_CNTL, (hbb_hi << 4) |
				(mal << 3) | (hbb_lo << 1) | mode);

	kgsl_regwrite(device, A6XX_SP_NC_MODE_CNTL, (hbb_hi << 10) |
				(mal << 3) | (uavflagprd_inv << 4) |
				(hbb_lo << 1) | mode);

	kgsl_regwrite(device, A6XX_UCHE_MODE_CNTL, (mal << 23) |
		(hbb_lo << 21));

	kgsl_regwrite(device, A6XX_RBBM_INTERFACE_HANG_INT_CNTL,
				(1 << 30) | a6xx_core->hang_detect_cycles);

	kgsl_regwrite(device, A6XX_UCHE_CLIENT_PF, 1);

	/* Set weights for bicubic filtering */
	if (adreno_is_a650_family(adreno_dev)) {
		kgsl_regwrite(device, A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_0, 0);
		kgsl_regwrite(device, A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_1,
			0x3FE05FF4);
		kgsl_regwrite(device, A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_2,
			0x3FA0EBEE);
		kgsl_regwrite(device, A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_3,
			0x3F5193ED);
		kgsl_regwrite(device, A6XX_TPL1_BICUBIC_WEIGHTS_TABLE_4,
			0x3F0243F0);
	}

	/* Set TWOPASSUSEWFI in A6XX_PC_DBG_ECO_CNTL if requested */
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_TWO_PASS_USE_WFI))
		kgsl_regrmw(device, A6XX_PC_DBG_ECO_CNTL, 0, (1 << 8));

	/* Set the bit vccCacheSkipDis=1 to get rid of TSEskip logic */
	if (a6xx_core->disable_tseskip)
		kgsl_regrmw(device, A6XX_PC_DBG_ECO_CNTL, 0, (1 << 9));

	/* Set the bit in HLSQ Cluster for A702 */
	if (adreno_is_a702(adreno_dev))
		kgsl_regwrite(device, A6XX_CP_CHICKEN_DBG, (1 << 24));

	/* Enable the GMEM save/restore feature for preemption */
	if (adreno_is_preemption_enabled(adreno_dev))
		kgsl_regwrite(device, A6XX_RB_CONTEXT_SWITCH_GMEM_SAVE_RESTORE,
			0x1);

	/*
	 * Enable GMU power counter 0 to count GPU busy. This is applicable to
	 * all a6xx targets
	 */
	if (adreno_is_a619_holi(adreno_dev)) {
		unsigned int val;

		adreno_write_gmu_wrapper(adreno_dev,
			A6XX_GPU_GMU_AO_GPU_CX_BUSY_MASK, 0xff000000);
		adreno_read_gmu_wrapper(adreno_dev,
			A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0, &val);
		adreno_write_gmu_wrapper(adreno_dev,
			A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0,
			(val & 0xff) | 0x20);
		adreno_write_gmu_wrapper(adreno_dev,
			A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0x1);

	} else {
		kgsl_regwrite(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_MASK,
			0xff000000);
		kgsl_regrmw(device, A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0,
			0xff, 0x20);
		kgsl_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE,
			0x1);
	}
	a6xx_protect_init(adreno_dev);
	/*
	 * We start LM here because we want all the following to be up
	 * 1. GX HS
	 * 2. SPTPRAC
	 * 3. HFI
	 * At this point, we are guaranteed all.
	 */

	/* Configure LLCC */
	a6xx_llc_configure_gpu_scid(adreno_dev);
	a6xx_llc_configure_gpuhtw_scid(adreno_dev);

	a6xx_llc_enable_overrides(adreno_dev);

	if (adreno_is_a660(adreno_dev)) {
		kgsl_regwrite(device, A6XX_CP_CHICKEN_DBG, 0x1);
		kgsl_regwrite(device, A6XX_RBBM_GBIF_CLIENT_QOS_CNTL, 0x0);

		/*
		 * Set dualQ + disable afull for A660, A642 GPU but
		 * not for A642L and A643
		 */
		if (!adreno_is_a642l(adreno_dev) ||
			!adreno_is_a643(adreno_dev))
			kgsl_regwrite(device, A6XX_UCHE_CMDQ_CONFIG, 0x66906);
	}

	if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		kgsl_regwrite(device, A6XX_CP_APRIV_CNTL, A6XX_APRIV_DEFAULT);

	a6xx_set_secvid(device);

	/*
	 * Enable hardware clock gating here to prevent any register access
	 * issue due to internal clock gating.
	 */
	a6xx_hwcg_set(adreno_dev, true);

	/*
	 * All registers must be written before this point so that we don't
	 * miss any register programming when we patch the power up register
	 * list.
	 */
	if (!patch_reglist && (adreno_dev->pwrup_reglist->gpuaddr != 0)) {
		a6xx_patch_pwrup_reglist(adreno_dev);
		patch_reglist = true;
	}

	/*
	 * During adreno_stop, GBIF halt is asserted to ensure
	 * no further transaction can go through GPU before GPU
	 * headswitch is turned off.
	 *
	 * This halt is deasserted once headswitch goes off but
	 * incase headswitch doesn't goes off clear GBIF halt
	 * here to ensure GPU wake-up doesn't fail because of
	 * halted GPU transactions.
	 */
	a6xx_deassert_gbif_halt(adreno_dev);

}

/* Offsets into the MX/CX mapped register regions */
#define RDPM_MX_OFFSET 0xf00
#define RDPM_CX_OFFSET 0xf18

void a6xx_rdpm_mx_freq_update(struct a6xx_gmu_device *gmu,
		u32 freq)
{
	if (gmu->rdpm_mx_virt) {
		writel_relaxed(freq/1000,
			(gmu->rdpm_mx_virt + RDPM_MX_OFFSET));

		/*
		 * ensure previous writes post before this one,
		 * i.e. act like normal writel()
		 */
		wmb();
	}
}

void a6xx_rdpm_cx_freq_update(struct a6xx_gmu_device *gmu,
		u32 freq)
{
	if (gmu->rdpm_cx_virt) {
		writel_relaxed(freq/1000,
			(gmu->rdpm_cx_virt + RDPM_CX_OFFSET));

		/*
		 * ensure previous writes post before this one,
		 * i.e. act like normal writel()
		 */
		wmb();
	}
}

void a6xx_unhalt_sqe(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
	uint64_t gpuaddr;

	gpuaddr = fw->memdesc->gpuaddr;

	/* Program the ucode base for CP */
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_LO,
				lower_32_bits(gpuaddr));
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_HI,
				upper_32_bits(gpuaddr));

	/* Clear the SQE_HALT to start the CP engine */
	kgsl_regwrite(device, A6XX_CP_SQE_CNTL, 1);
}


/*
 * CP_INIT_MAX_CONTEXT bit tells if the multiple hardware contexts can
 * be used at once of if they should be serialized
 */
#define CP_INIT_MAX_CONTEXT BIT(0)

/* Enables register protection mode */
#define CP_INIT_ERROR_DETECTION_CONTROL BIT(1)

/* Header dump information */
#define CP_INIT_HEADER_DUMP BIT(2) /* Reserved */

/* Default Reset states enabled for PFP and ME */
#define CP_INIT_DEFAULT_RESET_STATE BIT(3)

/* Drawcall filter range */
#define CP_INIT_DRAWCALL_FILTER_RANGE BIT(4)

/* Ucode workaround masks */
#define CP_INIT_UCODE_WORKAROUND_MASK BIT(5)

/*
 * Operation mode mask
 *
 * This ordinal provides the option to disable the
 * save/restore of performance counters across preemption.
 */
#define CP_INIT_OPERATION_MODE_MASK BIT(6)

/* Register initialization list */
#define CP_INIT_REGISTER_INIT_LIST BIT(7)

/* Register initialization list with spinlock */
#define CP_INIT_REGISTER_INIT_LIST_WITH_SPINLOCK BIT(8)

#define CP_INIT_MASK (CP_INIT_MAX_CONTEXT | \
		CP_INIT_ERROR_DETECTION_CONTROL | \
		CP_INIT_HEADER_DUMP | \
		CP_INIT_DEFAULT_RESET_STATE | \
		CP_INIT_UCODE_WORKAROUND_MASK | \
		CP_INIT_OPERATION_MODE_MASK | \
		CP_INIT_REGISTER_INIT_LIST_WITH_SPINLOCK)

static int a6xx_get_cp_init_cmds(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 *cmds, i = 0;

	if (adreno_dev->cp_init_cmds)
		return 0;

	adreno_dev->cp_init_cmds = devm_kzalloc(&device->pdev->dev,
			A6XX_CP_INIT_DWORDS << 2, GFP_KERNEL);
	if (!adreno_dev->cp_init_cmds)
		return -ENOMEM;

	cmds = (u32 *)adreno_dev->cp_init_cmds;

	cmds[i++] = cp_type7_packet(CP_ME_INIT, A6XX_CP_INIT_DWORDS - 1);

	/* Enabled ordinal mask */
	cmds[i++] = CP_INIT_MASK;

	if (CP_INIT_MASK & CP_INIT_MAX_CONTEXT)
		cmds[i++] = 0x00000003;

	if (CP_INIT_MASK & CP_INIT_ERROR_DETECTION_CONTROL)
		cmds[i++] = 0x20000000;

	if (CP_INIT_MASK & CP_INIT_HEADER_DUMP) {
		/* Header dump address */
		cmds[i++] = 0x00000000;
		/* Header dump enable and dump size */
		cmds[i++] = 0x00000000;
	}

	if (CP_INIT_MASK & CP_INIT_UCODE_WORKAROUND_MASK)
		cmds[i++] = 0x00000000;

	if (CP_INIT_MASK & CP_INIT_OPERATION_MODE_MASK)
		cmds[i++] = 0x00000002;

	if (CP_INIT_MASK & CP_INIT_REGISTER_INIT_LIST_WITH_SPINLOCK) {
		uint64_t gpuaddr = adreno_dev->pwrup_reglist->gpuaddr;

		cmds[i++] = lower_32_bits(gpuaddr);
		cmds[i++] = upper_32_bits(gpuaddr);
		cmds[i++] =  0;
	}

	return 0;
}

void a6xx_spin_idle_debug(struct adreno_device *adreno_dev,
				const char *str)
{
	struct kgsl_device *device = &adreno_dev->dev;
	unsigned int rptr, wptr;
	unsigned int status, status3, intstatus;
	unsigned int hwfault;

	dev_err(device->dev, str);

	kgsl_regread(device, A6XX_CP_RB_RPTR, &rptr);
	kgsl_regread(device, A6XX_CP_RB_WPTR, &wptr);

	kgsl_regread(device, A6XX_RBBM_STATUS, &status);
	kgsl_regread(device, A6XX_RBBM_STATUS3, &status3);
	kgsl_regread(device, A6XX_RBBM_INT_0_STATUS, &intstatus);
	kgsl_regread(device, A6XX_CP_HW_FAULT, &hwfault);


	dev_err(device->dev,
		"rb=%d pos=%X/%X rbbm_status=%8.8X/%8.8X int_0_status=%8.8X\n",
		adreno_dev->cur_rb->id, rptr, wptr, status, status3, intstatus);

	dev_err(device->dev, " hwfault=%8.8X\n", hwfault);

	kgsl_device_snapshot(device, NULL, false);

}

/*
 * a6xx_send_cp_init() - Initialize ringbuffer
 * @adreno_dev: Pointer to adreno device
 * @rb: Pointer to the ringbuffer of device
 *
 * Submit commands for ME initialization,
 */
static int a6xx_send_cp_init(struct adreno_device *adreno_dev,
			 struct adreno_ringbuffer *rb)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int *cmds;
	int ret;

	cmds = adreno_ringbuffer_allocspace(rb, 12);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	memcpy(cmds, adreno_dev->cp_init_cmds, 12 << 2);

	ret = adreno_ringbuffer_submit_spin(rb, NULL, 2000);
	if (ret) {
		a6xx_spin_idle_debug(adreno_dev,
				"CP initialization failed to idle\n");

		kgsl_sharedmem_writel(device->scratch,
			SCRATCH_RPTR_OFFSET(rb->id), 0);
		rb->wptr = 0;
		rb->_wptr = 0;
	}

	return ret;
}

/*
 * Follow the ME_INIT sequence with a preemption yield to allow the GPU to move
 * to a different ringbuffer, if desired
 */
static int _preemption_init(struct adreno_device *adreno_dev,
		struct adreno_ringbuffer *rb, unsigned int *cmds,
		struct kgsl_context *context)
{
	unsigned int *cmds_orig = cmds;

	/* Turn CP protection OFF on legacy targets */
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		cmds += cp_protected_mode(adreno_dev, cmds, 0);

	*cmds++ = cp_type7_packet(CP_SET_PSEUDO_REGISTER, 6);
	*cmds++ = 1;
	cmds += cp_gpuaddr(adreno_dev, cmds,
			rb->preemption_desc->gpuaddr);

	*cmds++ = 2;
	cmds += cp_gpuaddr(adreno_dev, cmds,
		rb->secure_preemption_desc->gpuaddr);

	/* Turn CP protection back ON */
	if (!ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		cmds += cp_protected_mode(adreno_dev, cmds, 1);

	*cmds++ = cp_type7_packet(CP_CONTEXT_SWITCH_YIELD, 4);
	cmds += cp_gpuaddr(adreno_dev, cmds, 0x0);
	*cmds++ = 0;
	/* generate interrupt on preemption completion */
	*cmds++ = 0;

	return cmds - cmds_orig;
}

static int a6xx_post_start(struct adreno_device *adreno_dev)
{
	int ret;
	unsigned int *cmds, *start;
	struct adreno_ringbuffer *rb = adreno_dev->cur_rb;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!adreno_is_preemption_enabled(adreno_dev))
		return 0;

	cmds = adreno_ringbuffer_allocspace(rb, 42);
	if (IS_ERR(cmds)) {
		dev_err(device->dev,
			     "error allocating preemption init cmds\n");
		return PTR_ERR(cmds);
	}
	start = cmds;

	cmds += _preemption_init(adreno_dev, rb, cmds, NULL);

	rb->_wptr = rb->_wptr - (42 - (cmds - start));

	ret = adreno_ringbuffer_submit_spin_nosync(rb, NULL, 2000);
	if (ret)
		a6xx_spin_idle_debug(adreno_dev,
			"hw preemption initialization failed to idle\n");

	return ret;
}

int a6xx_rb_start(struct adreno_device *adreno_dev)
{
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 cp_rb_cntl = A6XX_CP_RB_CNTL_DEFAULT |
		(ADRENO_FEATURE(adreno_dev, ADRENO_APRIV) ? 0 : (1 << 27));
	struct adreno_ringbuffer *rb;
	uint64_t addr;
	int ret, i;
	unsigned int *cmds;

	/* Clear all the ringbuffers */
	FOR_EACH_RINGBUFFER(adreno_dev, rb, i) {
		memset(rb->buffer_desc->hostptr, 0xaa, KGSL_RB_SIZE);
		kgsl_sharedmem_writel(device->scratch,
			SCRATCH_RPTR_OFFSET(rb->id), 0);

		rb->wptr = 0;
		rb->_wptr = 0;
		rb->wptr_preempt_end = ~0;
	}

	a6xx_preemption_start(adreno_dev);

	/* Set up the current ringbuffer */
	rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	addr = SCRATCH_RPTR_GPU_ADDR(device, rb->id);

	kgsl_regwrite(device, A6XX_CP_RB_RPTR_ADDR_LO, lower_32_bits(addr));
	kgsl_regwrite(device, A6XX_CP_RB_RPTR_ADDR_HI, upper_32_bits(addr));

	/*
	 * The size of the ringbuffer in the hardware is the log2
	 * representation of the size in quadwords (sizedwords / 2).
	 */
	kgsl_regwrite(device, A6XX_CP_RB_CNTL, cp_rb_cntl);

	kgsl_regwrite(device, A6XX_CP_RB_BASE,
		lower_32_bits(rb->buffer_desc->gpuaddr));

	kgsl_regwrite(device, A6XX_CP_RB_BASE_HI,
		upper_32_bits(rb->buffer_desc->gpuaddr));

	a6xx_unhalt_sqe(adreno_dev);

	ret = a6xx_send_cp_init(adreno_dev, rb);
	if (ret)
		return ret;

	ret = adreno_zap_shader_load(adreno_dev, a6xx_core->zap_name);
	if (ret)
		return ret;

	/*
	 * Take the GPU out of secure mode. Try the zap shader if it is loaded,
	 * otherwise just try to write directly to the secure control register
	 */
	if (!adreno_dev->zap_loaded)
		kgsl_regwrite(device, A6XX_RBBM_SECVID_TRUST_CNTL, 0);
	else {
		cmds = adreno_ringbuffer_allocspace(rb, 2);
		if (IS_ERR(cmds))
			return PTR_ERR(cmds);

		*cmds++ = cp_packet(adreno_dev, CP_SET_SECURE_MODE, 1);
		*cmds++ = 0;

		ret = adreno_ringbuffer_submit_spin(rb, NULL, 2000);
		if (ret) {
			a6xx_spin_idle_debug(adreno_dev,
				"Switch to unsecure failed to idle\n");
			return ret;
		}
	}

	return a6xx_post_start(adreno_dev);
}

unsigned int a6xx_set_marker(
		unsigned int *cmds, enum adreno_cp_marker_type type)
{
	unsigned int cmd = 0;

	*cmds++ = cp_type7_packet(CP_SET_MARKER, 1);

	/*
	 * Indicate the beginning and end of the IB1 list with a SET_MARKER.
	 * Among other things, this will implicitly enable and disable
	 * preemption respectively. IFPC can also be disabled and enabled
	 * with a SET_MARKER. Bit 8 tells the CP the marker is for IFPC.
	 */
	switch (type) {
	case IFPC_DISABLE:
		cmd = 0x101;
		break;
	case IFPC_ENABLE:
		cmd = 0x100;
		break;
	case IB1LIST_START:
		cmd = 0xD;
		break;
	case IB1LIST_END:
		cmd = 0xE;
		break;
	}

	*cmds++ = cmd;
	return 2;
}

/*
 * a6xx_gpu_keepalive() - GMU reg write to request GPU stays on
 * @adreno_dev: Pointer to the adreno device that has the GMU
 * @state: State to set: true is ON, false is OFF
 */
static void a6xx_gpu_keepalive(struct adreno_device *adreno_dev,
		bool state)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!gmu_core_isenabled(device))
		return;

	gmu_core_regwrite(device, A6XX_GMU_GMU_PWR_COL_KEEPALIVE, state);
}

static bool a619_holi_hw_isidle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg;

	kgsl_regread(device, A6XX_RBBM_STATUS, &reg);
	if (reg & 0xfffffffe)
		return false;

	return adreno_irq_pending(adreno_dev) ? false : true;
}

bool a6xx_hw_isidle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int reg;

	/* Non GMU devices monitor the RBBM status */
	if (!gmu_core_isenabled(device)) {
		kgsl_regread(device, A6XX_RBBM_STATUS, &reg);
		if (reg & 0xfffffffe)
			return false;

		return adreno_irq_pending(adreno_dev) ? false : true;
	}

	gmu_core_regread(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &reg);
	/* Bit 23 is GPUBUSYIGNAHB */
	return ((reg & BIT(23)) || adreno_irq_pending(adreno_dev)) ? false : true;
}

int a6xx_microcode_read(struct adreno_device *adreno_dev)
{
	struct adreno_firmware *sqe_fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);

	return adreno_get_firmware(adreno_dev, a6xx_core->sqefw_name, sqe_fw);
}

static int64_t a6xx_read_throttling_counters(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int64_t adj = -1;
	u32 a, b, c;
	struct adreno_busy_data *busy = &adreno_dev->busy_data;

	if (!adreno_dev->lm_enabled)
		return 0;

	/* The counters are selected in a6xx_gmu_enable_lm() */
	a = counter_delta(device, A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_1_L,
		&busy->throttle_cycles[0]);

	b = counter_delta(device, A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_2_L,
		&busy->throttle_cycles[1]);

	c = counter_delta(device, A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_3_L,
		&busy->throttle_cycles[2]);


	/*
	 * The adjustment is the number of cycles lost to throttling, which
	 * is calculated as a weighted average of the cycles throttled
	 * at different levels. The adjustment is negative because in A6XX,
	 * the busy count includes the throttled cycles. Therefore, we want
	 * to remove them to prevent appearing to be busier than
	 * we actually are.
	 */
	if (adreno_is_a620(adreno_dev) || adreno_is_a650(adreno_dev))
		/*
		 * With the newer generations, CRC throttle from SIDs of 0x14
		 * and above cannot be observed in power counters. Since 90%
		 * throttle uses SID 0x16 the adjustment calculation needs
		 * correction. The throttling is in increments of 4.2%, and the
		 * 91.7% counter does a weighted count by the value of sid used
		 * which are taken into consideration for the final formula.
		 */
		adj *= div_s64((a * 42) + (b * 500) +
			(div_s64((int64_t)c - a - b * 12, 22) * 917), 1000);
	else
		adj *= ((a * 5) + (b * 50) + (c * 90)) / 100;

	trace_kgsl_clock_throttling(0, b, c, a, adj);

	return adj;
}
#define GPU_CPR_FSM_CTL_OFFSET	 0x4
void a6xx_gx_cpr_toggle(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	static void __iomem *gx_cpr_virt;
	struct resource *res;
	u32 val = 0;

	if (!a6xx_core->gx_cpr_toggle)
		return;

	if (!gx_cpr_virt) {
		res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
				"gx_cpr");
		if (res == NULL)
			return;

		gx_cpr_virt = devm_ioremap_resource(&device->pdev->dev, res);
		if (!gx_cpr_virt) {
			dev_err(device->dev, "Failed to map GX CPR\n");
			return;
		}
	}

	/*
	 * Toggle(disable -> enable) closed loop functionality to recover
	 * CPR measurements stall happened under certain conditions.
	 */

	val = readl_relaxed(gx_cpr_virt + GPU_CPR_FSM_CTL_OFFSET);
	/* Make sure memory is updated before access */
	rmb();

	writel_relaxed(val & 0xfffffff0, gx_cpr_virt + GPU_CPR_FSM_CTL_OFFSET);
	/* make sure register write committed */
	wmb();

	/* Wait for small time before we enable GX CPR */
	udelay(5);

	writel_relaxed(val | 0x00000001, gx_cpr_virt + GPU_CPR_FSM_CTL_OFFSET);
	/* make sure register write committed */
	wmb();
}

/* This is only defined for non-GMU and non-RGMU targets */
static int a6xx_clear_pending_transactions(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (adreno_is_a619_holi(adreno_dev)) {
		kgsl_regwrite(device, A6XX_RBBM_GPR0_CNTL, 0x1e0);
		ret = adreno_wait_for_halt_ack(device,
			A6XX_RBBM_VBIF_GX_RESET_STATUS, 0xf0);
	} else {
		kgsl_regwrite(device, A6XX_RBBM_GBIF_HALT,
			A6XX_GBIF_GX_HALT_MASK);
		ret = adreno_wait_for_halt_ack(device, A6XX_RBBM_GBIF_HALT_ACK,
			A6XX_GBIF_GX_HALT_MASK);
	}

	if (ret)
		return ret;

	return a6xx_halt_gbif(adreno_dev);
}

/**
 * a6xx_reset() - Helper function to reset the GPU
 * @device: Pointer to the KGSL device structure for the GPU
 *
 * Try to reset the GPU to recover from a fault for targets without
 * a GMU.
 */
static int a6xx_reset(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret;
	unsigned long flags = device->pwrctrl.ctrl_flags;

	ret = a6xx_clear_pending_transactions(adreno_dev);
	if (ret)
		return ret;

	/* Clear ctrl_flags to ensure clocks and regulators are turned off */
	device->pwrctrl.ctrl_flags = 0;

	kgsl_pwrctrl_change_state(device, KGSL_STATE_INIT);

	/* since device is officially off now clear start bit */
	clear_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv);

	ret = adreno_start(device, 0);
	if (ret)
		return ret;

	kgsl_pwrctrl_change_state(device, KGSL_STATE_ACTIVE);

	device->pwrctrl.ctrl_flags = flags;

	/* Toggle GX CPR on demand */
	 a6xx_gx_cpr_toggle(device);

	/*
	 * If active_cnt is zero, there is no need to keep the GPU active. So,
	 * we should transition to SLUMBER.
	 */
	if (!atomic_read(&device->active_cnt))
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	return 0;
}

static void a6xx_cp_hw_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int status1, status2;

	kgsl_regread(device, A6XX_CP_INTERRUPT_STATUS, &status1);

	if (status1 & BIT(A6XX_CP_OPCODE_ERROR)) {
		unsigned int opcode;

		kgsl_regwrite(device, A6XX_CP_SQE_STAT_ADDR, 1);
		kgsl_regread(device, A6XX_CP_SQE_STAT_DATA, &opcode);
		dev_crit_ratelimited(device->dev,
			"CP opcode error interrupt | opcode=0x%8.8x\n", opcode);
	}
	if (status1 & BIT(A6XX_CP_UCODE_ERROR))
		dev_crit_ratelimited(device->dev, "CP ucode error interrupt\n");
	if (status1 & BIT(A6XX_CP_HW_FAULT_ERROR)) {
		kgsl_regread(device, A6XX_CP_HW_FAULT, &status2);
		dev_crit_ratelimited(device->dev,
			"CP | Ringbuffer HW fault | status=%x\n", status2);
	}
	if (status1 & BIT(A6XX_CP_REGISTER_PROTECTION_ERROR)) {
		kgsl_regread(device, A6XX_CP_PROTECT_STATUS, &status2);
		dev_crit_ratelimited(device->dev,
			"CP | Protected mode error | %s | addr=%x | status=%x\n",
			status2 & (1 << 20) ? "READ" : "WRITE",
			status2 & 0x3FFFF, status2);
	}
	if (status1 & BIT(A6XX_CP_AHB_ERROR))
		dev_crit_ratelimited(device->dev,
			"CP AHB error interrupt\n");
	if (status1 & BIT(A6XX_CP_VSD_PARITY_ERROR))
		dev_crit_ratelimited(device->dev,
			"CP VSD decoder parity error\n");
	if (status1 & BIT(A6XX_CP_ILLEGAL_INSTR_ERROR))
		dev_crit_ratelimited(device->dev,
			"CP Illegal instruction error\n");

}

static void a6xx_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	switch (bit) {
	case A6XX_INT_CP_AHB_ERROR:
		dev_crit_ratelimited(device->dev, "CP: AHB bus error\n");
		break;
	case A6XX_INT_ATB_ASYNCFIFO_OVERFLOW:
		dev_crit_ratelimited(device->dev,
					"RBBM: ATB ASYNC overflow\n");
		break;
	case A6XX_INT_RBBM_ATB_BUS_OVERFLOW:
		dev_crit_ratelimited(device->dev,
					"RBBM: ATB bus overflow\n");
		break;
	case A6XX_INT_UCHE_OOB_ACCESS:
		dev_crit_ratelimited(device->dev,
					"UCHE: Out of bounds access\n");
		break;
	case A6XX_INT_UCHE_TRAP_INTR:
		dev_crit_ratelimited(device->dev, "UCHE: Trap interrupt\n");
		break;
	case A6XX_INT_TSB_WRITE_ERROR:
		dev_crit_ratelimited(device->dev, "TSB: Write error interrupt\n");
		break;
	default:
		dev_crit_ratelimited(device->dev, "Unknown interrupt %d\n",
					bit);
	}
}

/*
 * a6xx_llc_configure_gpu_scid() - Program the sub-cache ID for all GPU blocks
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_configure_gpu_scid(struct adreno_device *adreno_dev)
{
	uint32_t gpu_scid;
	uint32_t gpu_cntl1_val = 0;
	int i;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_mmu *mmu = &device->mmu;

	if (IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice) ||
		!adreno_dev->gpu_llc_slice_enable)
		return;

	if (llcc_slice_activate(adreno_dev->gpu_llc_slice))
		return;

	gpu_scid = llcc_get_slice_id(adreno_dev->gpu_llc_slice);
	for (i = 0; i < A6XX_LLC_NUM_GPU_SCIDS; i++)
		gpu_cntl1_val = (gpu_cntl1_val << A6XX_GPU_LLC_SCID_NUM_BITS)
			| gpu_scid;

	if (mmu->subtype == KGSL_IOMMU_SMMU_V500)
		kgsl_regrmw(device, A6XX_GBIF_SCACHE_CNTL1,
			A6XX_GPU_LLC_SCID_MASK, gpu_cntl1_val);
	else
		adreno_cx_misc_regrmw(adreno_dev,
				A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1,
				A6XX_GPU_LLC_SCID_MASK, gpu_cntl1_val);

	/*
	 * On A660, the SCID programming for UCHE traffic is done in
	 * A6XX_GBIF_SCACHE_CNTL0[14:10]
	 * GFO ENABLE BIT(8) : LLC uses a 64 byte cache line size enabling
	 * GFO allows it allocate partial cache lines
	 */
	if (adreno_is_a660(adreno_dev))
		kgsl_regrmw(device, A6XX_GBIF_SCACHE_CNTL0, (0x1f << 10) |
				BIT(8), (gpu_scid << 10) | BIT(8));
}

/*
 * a6xx_llc_configure_gpuhtw_scid() - Program the SCID for GPU pagetables
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_configure_gpuhtw_scid(struct adreno_device *adreno_dev)
{
	uint32_t gpuhtw_scid;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_mmu *mmu = &device->mmu;

	if (IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice) ||
		!adreno_dev->gpuhtw_llc_slice_enable)
		return;

	if (llcc_slice_activate(adreno_dev->gpuhtw_llc_slice))
		return;

	/*
	 * On SMMU-v500, the GPUHTW SCID is configured via a NoC override in
	 * the XBL image.
	 */
	if (mmu->subtype == KGSL_IOMMU_SMMU_V500)
		return;

	gpuhtw_scid = llcc_get_slice_id(adreno_dev->gpuhtw_llc_slice);

	adreno_cx_misc_regrmw(adreno_dev,
			A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1,
			A6XX_GPUHTW_LLC_SCID_MASK,
			gpuhtw_scid << A6XX_GPUHTW_LLC_SCID_SHIFT);
}

/*
 * a6xx_llc_enable_overrides() - Override the page attributes
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_enable_overrides(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_mmu *mmu = &device->mmu;

	/*
	 * Attributes override through GBIF is not supported with MMU-500.
	 * Attributes are used as configured through SMMU pagetable entries.
	 */
	if (mmu->subtype == KGSL_IOMMU_SMMU_V500)
		return;

	/*
	 * 0x3: readnoallocoverrideen=0
	 *      read-no-alloc=0 - Allocate lines on read miss
	 *      writenoallocoverrideen=1
	 *      write-no-alloc=1 - Do not allocates lines on write miss
	 */
	adreno_cx_misc_regwrite(adreno_dev,
			A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_0, 0x3);
}

static const char *uche_client[7][3] = {
	{"SP | VSC | VPC | HLSQ | PC | LRZ", "TP", "VFD"},
	{"VSC | VPC | HLSQ | PC | LRZ", "TP | VFD", "SP"},
	{"SP | VPC | HLSQ | PC | LRZ", "TP | VFD", "VSC"},
	{"SP | VSC | HLSQ | PC | LRZ", "TP | VFD", "VPC"},
	{"SP | VSC | VPC | PC | LRZ", "TP | VFD", "HLSQ"},
	{"SP | VSC | VPC | HLSQ | LRZ", "TP | VFD", "PC"},
	{"SP | VSC | VPC | HLSQ | PC", "TP | VFD", "LRZ"},
};

static const char *const uche_client_a660[] = { "VFD", "SP", "VSC", "VPC",
						"HLSQ", "PC", "LRZ", "TP" };

#define SCOOBYDOO 0x5c00bd00

static const char *a6xx_fault_block_uche(struct kgsl_device *device,
		unsigned int mid)
{
	unsigned int uche_client_id = 0;
	static char str[40];

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

	kgsl_regread(device, A6XX_UCHE_CLIENT_PF, &uche_client_id);
	mutex_unlock(&device->mutex);

	/* Ignore the value if the gpu is in IFPC */
	if (uche_client_id == SCOOBYDOO)
		return "UCHE: unknown";

	if (adreno_is_a660(ADRENO_DEVICE(device))) {

		/* Mask is 7 bits for A660 */
		uche_client_id &= 0x7F;
		if (uche_client_id >= ARRAY_SIZE(uche_client_a660) ||
				(mid == 2))
			return "UCHE: Unknown";

		if (mid == 1)
			snprintf(str, sizeof(str), "UCHE: Not %s",
				uche_client_a660[uche_client_id]);
		else if (mid == 3)
			snprintf(str, sizeof(str), "UCHE: %s",
				uche_client_a660[uche_client_id]);
	} else {
		uche_client_id &= A6XX_UCHE_CLIENT_PF_CLIENT_ID_MASK;
		if (uche_client_id >= ARRAY_SIZE(uche_client))
			return "UCHE: Unknown";

		snprintf(str, sizeof(str), "UCHE: %s",
			uche_client[uche_client_id][mid - 1]);
	}

	return str;
}

static const char *a6xx_iommu_fault_block(struct kgsl_device *device,
		unsigned int fsynr1)
{
	unsigned int mid = fsynr1 & 0xff;

	switch (mid) {
	case 0:
		return "CP";
	case 1:
	case 2:
	case 3:
		return a6xx_fault_block_uche(device, mid);
	case 4:
		return "CCU";
	case 6:
		return "CDP Prefetch";
	case 7:
		return "GPMU";
	}

	return "Unknown";
}

static void a6xx_cp_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_is_preemption_enabled(adreno_dev))
		a6xx_preemption_trigger(adreno_dev);

	adreno_dispatcher_schedule(device);
}

/*
 * a6xx_gpc_err_int_callback() - Isr for GPC error interrupts
 * @adreno_dev: Pointer to device
 * @bit: Interrupt bit
 */
static void a6xx_gpc_err_int_callback(struct adreno_device *adreno_dev, int bit)
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

static const struct adreno_irq_funcs a6xx_irq_funcs[32] = {
	ADRENO_IRQ_CALLBACK(NULL),              /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 2 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 3 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 4 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 5 - UNUSED */
	/* 6 - RBBM_ATB_ASYNC_OVERFLOW */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback),
	ADRENO_IRQ_CALLBACK(a6xx_gpc_err_int_callback), /* 7 - GPC_ERR */
	ADRENO_IRQ_CALLBACK(a6xx_preemption_callback),/* 8 - CP_SW */
	ADRENO_IRQ_CALLBACK(a6xx_cp_hw_err_callback), /* 9 - CP_HW_ERROR */
	ADRENO_IRQ_CALLBACK(NULL),  /* 10 - CP_CCU_FLUSH_DEPTH_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 11 - CP_CCU_FLUSH_COLOR_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 12 - CP_CCU_RESOLVE_TS */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 13 - CP_IB2_INT */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 14 - CP_IB1_INT */
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 15 - CP_RB_INT */
	ADRENO_IRQ_CALLBACK(NULL), /* 16 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 17 - CP_RB_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 18 - CP_WT_DONE_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 19 - UNUSED */
	ADRENO_IRQ_CALLBACK(a6xx_cp_callback), /* 20 - CP_CACHE_FLUSH_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 21 - UNUSED */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 22 - RBBM_ATB_BUS_OVERFLOW */
	/* 23 - MISC_HANG_DETECT */
	ADRENO_IRQ_CALLBACK(adreno_hang_int_callback),
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 24 - UCHE_OOB_ACCESS */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 25 - UCHE_TRAP_INTR */
	ADRENO_IRQ_CALLBACK(NULL), /* 26 - DEBBUS_INTR_0 */
	ADRENO_IRQ_CALLBACK(NULL), /* 27 - DEBBUS_INTR_1 */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 28 - TSBWRITEERROR */
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
static int a6xx_irq_poll_fence(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 status, fence, fence_retries = 0;
	u64 a, b, c;

	if (!gmu_core_isenabled(device))
		return 0;

	a = a6xx_read_alwayson(adreno_dev);

	kgsl_regread(device, A6XX_GMU_AO_AHB_FENCE_CTRL, &fence);

	while (fence != 0) {
		b = a6xx_read_alwayson(adreno_dev);

		/* Wait for small time before trying again */
		udelay(1);
		kgsl_regread(device, A6XX_GMU_AO_AHB_FENCE_CTRL, &fence);

		if (fence_retries == 100 && fence != 0) {
			c = a6xx_read_alwayson(adreno_dev);

			kgsl_regread(device, A6XX_GMU_RBBM_INT_UNMASKED_STATUS,
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

static irqreturn_t a6xx_irq_handler(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	irqreturn_t ret = IRQ_NONE;
	u32 status;

	/*
	 * On A6xx, the GPU can power down once the INT_0_STATUS is read
	 * below. But there still might be some register reads required
	 * so force the GMU/GPU into KEEPALIVE mode until done with the ISR.
	 */
	a6xx_gpu_keepalive(adreno_dev, true);

	if (a6xx_irq_poll_fence(adreno_dev)) {
		adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
		adreno_dispatcher_schedule(device);
		goto done;
	}

	kgsl_regread(device, A6XX_RBBM_INT_0_STATUS, &status);

	kgsl_regwrite(device, A6XX_RBBM_INT_CLEAR_CMD, status);

	ret = adreno_irq_callbacks(adreno_dev, a6xx_irq_funcs, status);

	trace_kgsl_a5xx_irq_status(adreno_dev, status);

done:
	/* If hard fault, then let snapshot turn off the keepalive */
	if (!(adreno_gpu_fault(adreno_dev) & ADRENO_HARD_FAULT))
		a6xx_gpu_keepalive(adreno_dev, false);

	return ret;
}

#ifdef CONFIG_QCOM_KGSL_CORESIGHT
static struct adreno_coresight_register a6xx_coresight_regs[] = {
	{ A6XX_DBGC_CFG_DBGBUS_SEL_A },
	{ A6XX_DBGC_CFG_DBGBUS_SEL_B },
	{ A6XX_DBGC_CFG_DBGBUS_SEL_C },
	{ A6XX_DBGC_CFG_DBGBUS_SEL_D },
	{ A6XX_DBGC_CFG_DBGBUS_CNTLT },
	{ A6XX_DBGC_CFG_DBGBUS_CNTLM },
	{ A6XX_DBGC_CFG_DBGBUS_OPL },
	{ A6XX_DBGC_CFG_DBGBUS_OPE },
	{ A6XX_DBGC_CFG_DBGBUS_IVTL_0 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTL_1 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTL_2 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTL_3 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKL_0 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKL_1 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKL_2 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKL_3 },
	{ A6XX_DBGC_CFG_DBGBUS_BYTEL_0 },
	{ A6XX_DBGC_CFG_DBGBUS_BYTEL_1 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTE_0 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTE_1 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTE_2 },
	{ A6XX_DBGC_CFG_DBGBUS_IVTE_3 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKE_0 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKE_1 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKE_2 },
	{ A6XX_DBGC_CFG_DBGBUS_MASKE_3 },
	{ A6XX_DBGC_CFG_DBGBUS_NIBBLEE },
	{ A6XX_DBGC_CFG_DBGBUS_PTRC0 },
	{ A6XX_DBGC_CFG_DBGBUS_PTRC1 },
	{ A6XX_DBGC_CFG_DBGBUS_LOADREG },
	{ A6XX_DBGC_CFG_DBGBUS_IDX },
	{ A6XX_DBGC_CFG_DBGBUS_CLRC },
	{ A6XX_DBGC_CFG_DBGBUS_LOADIVT },
	{ A6XX_DBGC_VBIF_DBG_CNTL },
	{ A6XX_DBGC_DBG_LO_HI_GPIO },
	{ A6XX_DBGC_EXT_TRACE_BUS_CNTL },
	{ A6XX_DBGC_READ_AHB_THROUGH_DBG },
	{ A6XX_DBGC_CFG_DBGBUS_TRACE_BUF1 },
	{ A6XX_DBGC_CFG_DBGBUS_TRACE_BUF2 },
	{ A6XX_DBGC_EVT_CFG },
	{ A6XX_DBGC_EVT_INTF_SEL_0 },
	{ A6XX_DBGC_EVT_INTF_SEL_1 },
	{ A6XX_DBGC_PERF_ATB_CFG },
	{ A6XX_DBGC_PERF_ATB_COUNTER_SEL_0 },
	{ A6XX_DBGC_PERF_ATB_COUNTER_SEL_1 },
	{ A6XX_DBGC_PERF_ATB_COUNTER_SEL_2 },
	{ A6XX_DBGC_PERF_ATB_COUNTER_SEL_3 },
	{ A6XX_DBGC_PERF_ATB_TRIG_INTF_SEL_0 },
	{ A6XX_DBGC_PERF_ATB_TRIG_INTF_SEL_1 },
	{ A6XX_DBGC_PERF_ATB_DRAIN_CMD },
	{ A6XX_DBGC_ECO_CNTL },
	{ A6XX_DBGC_AHB_DBG_CNTL },
};

static struct adreno_coresight_register a6xx_coresight_regs_cx[] = {
	{ A6XX_CX_DBGC_CFG_DBGBUS_SEL_A },
	{ A6XX_CX_DBGC_CFG_DBGBUS_SEL_B },
	{ A6XX_CX_DBGC_CFG_DBGBUS_SEL_C },
	{ A6XX_CX_DBGC_CFG_DBGBUS_SEL_D },
	{ A6XX_CX_DBGC_CFG_DBGBUS_CNTLT },
	{ A6XX_CX_DBGC_CFG_DBGBUS_CNTLM },
	{ A6XX_CX_DBGC_CFG_DBGBUS_OPL },
	{ A6XX_CX_DBGC_CFG_DBGBUS_OPE },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTL_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTL_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTL_2 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTL_3 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKL_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKL_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKL_2 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKL_3 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_BYTEL_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_BYTEL_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTE_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTE_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTE_2 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IVTE_3 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKE_0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKE_1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKE_2 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_MASKE_3 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_NIBBLEE },
	{ A6XX_CX_DBGC_CFG_DBGBUS_PTRC0 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_PTRC1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_LOADREG },
	{ A6XX_CX_DBGC_CFG_DBGBUS_IDX },
	{ A6XX_CX_DBGC_CFG_DBGBUS_CLRC },
	{ A6XX_CX_DBGC_CFG_DBGBUS_LOADIVT },
	{ A6XX_CX_DBGC_VBIF_DBG_CNTL },
	{ A6XX_CX_DBGC_DBG_LO_HI_GPIO },
	{ A6XX_CX_DBGC_EXT_TRACE_BUS_CNTL },
	{ A6XX_CX_DBGC_READ_AHB_THROUGH_DBG },
	{ A6XX_CX_DBGC_CFG_DBGBUS_TRACE_BUF1 },
	{ A6XX_CX_DBGC_CFG_DBGBUS_TRACE_BUF2 },
	{ A6XX_CX_DBGC_EVT_CFG },
	{ A6XX_CX_DBGC_EVT_INTF_SEL_0 },
	{ A6XX_CX_DBGC_EVT_INTF_SEL_1 },
	{ A6XX_CX_DBGC_PERF_ATB_CFG },
	{ A6XX_CX_DBGC_PERF_ATB_COUNTER_SEL_0 },
	{ A6XX_CX_DBGC_PERF_ATB_COUNTER_SEL_1 },
	{ A6XX_CX_DBGC_PERF_ATB_COUNTER_SEL_2 },
	{ A6XX_CX_DBGC_PERF_ATB_COUNTER_SEL_3 },
	{ A6XX_CX_DBGC_PERF_ATB_TRIG_INTF_SEL_0 },
	{ A6XX_CX_DBGC_PERF_ATB_TRIG_INTF_SEL_1 },
	{ A6XX_CX_DBGC_PERF_ATB_DRAIN_CMD },
	{ A6XX_CX_DBGC_ECO_CNTL },
	{ A6XX_CX_DBGC_AHB_DBG_CNTL },
};

static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_a, &a6xx_coresight_regs[0]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_b, &a6xx_coresight_regs[1]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_c, &a6xx_coresight_regs[2]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_sel_d, &a6xx_coresight_regs[3]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlt, &a6xx_coresight_regs[4]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_cntlm, &a6xx_coresight_regs[5]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_opl, &a6xx_coresight_regs[6]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ope, &a6xx_coresight_regs[7]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_0, &a6xx_coresight_regs[8]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_1, &a6xx_coresight_regs[9]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_2, &a6xx_coresight_regs[10]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivtl_3, &a6xx_coresight_regs[11]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_0, &a6xx_coresight_regs[12]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_1, &a6xx_coresight_regs[13]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_2, &a6xx_coresight_regs[14]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maskl_3, &a6xx_coresight_regs[15]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_0, &a6xx_coresight_regs[16]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_bytel_1, &a6xx_coresight_regs[17]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_0, &a6xx_coresight_regs[18]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_1, &a6xx_coresight_regs[19]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_2, &a6xx_coresight_regs[20]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ivte_3, &a6xx_coresight_regs[21]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_0, &a6xx_coresight_regs[22]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_1, &a6xx_coresight_regs[23]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_2, &a6xx_coresight_regs[24]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_maske_3, &a6xx_coresight_regs[25]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_nibblee, &a6xx_coresight_regs[26]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc0, &a6xx_coresight_regs[27]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_ptrc1, &a6xx_coresight_regs[28]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadreg, &a6xx_coresight_regs[29]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_idx, &a6xx_coresight_regs[30]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_clrc, &a6xx_coresight_regs[31]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_loadivt, &a6xx_coresight_regs[32]);
static ADRENO_CORESIGHT_ATTR(vbif_dbg_cntl, &a6xx_coresight_regs[33]);
static ADRENO_CORESIGHT_ATTR(dbg_lo_hi_gpio, &a6xx_coresight_regs[34]);
static ADRENO_CORESIGHT_ATTR(ext_trace_bus_cntl, &a6xx_coresight_regs[35]);
static ADRENO_CORESIGHT_ATTR(read_ahb_through_dbg, &a6xx_coresight_regs[36]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf1, &a6xx_coresight_regs[37]);
static ADRENO_CORESIGHT_ATTR(cfg_dbgbus_trace_buf2, &a6xx_coresight_regs[38]);
static ADRENO_CORESIGHT_ATTR(evt_cfg, &a6xx_coresight_regs[39]);
static ADRENO_CORESIGHT_ATTR(evt_intf_sel_0, &a6xx_coresight_regs[40]);
static ADRENO_CORESIGHT_ATTR(evt_intf_sel_1, &a6xx_coresight_regs[41]);
static ADRENO_CORESIGHT_ATTR(perf_atb_cfg, &a6xx_coresight_regs[42]);
static ADRENO_CORESIGHT_ATTR(perf_atb_counter_sel_0, &a6xx_coresight_regs[43]);
static ADRENO_CORESIGHT_ATTR(perf_atb_counter_sel_1, &a6xx_coresight_regs[44]);
static ADRENO_CORESIGHT_ATTR(perf_atb_counter_sel_2, &a6xx_coresight_regs[45]);
static ADRENO_CORESIGHT_ATTR(perf_atb_counter_sel_3, &a6xx_coresight_regs[46]);
static ADRENO_CORESIGHT_ATTR(perf_atb_trig_intf_sel_0,
				&a6xx_coresight_regs[47]);
static ADRENO_CORESIGHT_ATTR(perf_atb_trig_intf_sel_1,
				&a6xx_coresight_regs[48]);
static ADRENO_CORESIGHT_ATTR(perf_atb_drain_cmd, &a6xx_coresight_regs[49]);
static ADRENO_CORESIGHT_ATTR(eco_cntl, &a6xx_coresight_regs[50]);
static ADRENO_CORESIGHT_ATTR(ahb_dbg_cntl, &a6xx_coresight_regs[51]);

/*CX debug registers*/
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_a,
				&a6xx_coresight_regs_cx[0]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_b,
				&a6xx_coresight_regs_cx[1]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_c,
				&a6xx_coresight_regs_cx[2]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_sel_d,
				&a6xx_coresight_regs_cx[3]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_cntlt,
				&a6xx_coresight_regs_cx[4]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_cntlm,
				&a6xx_coresight_regs_cx[5]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_opl,
				&a6xx_coresight_regs_cx[6]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ope,
				&a6xx_coresight_regs_cx[7]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_0,
				&a6xx_coresight_regs_cx[8]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_1,
				&a6xx_coresight_regs_cx[9]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_2,
				&a6xx_coresight_regs_cx[10]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivtl_3,
				&a6xx_coresight_regs_cx[11]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_0,
				&a6xx_coresight_regs_cx[12]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_1,
				&a6xx_coresight_regs_cx[13]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_2,
				&a6xx_coresight_regs_cx[14]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maskl_3,
				&a6xx_coresight_regs_cx[15]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_bytel_0,
				&a6xx_coresight_regs_cx[16]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_bytel_1,
				&a6xx_coresight_regs_cx[17]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_0,
				&a6xx_coresight_regs_cx[18]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_1,
				&a6xx_coresight_regs_cx[19]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_2,
				&a6xx_coresight_regs_cx[20]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ivte_3,
				&a6xx_coresight_regs_cx[21]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_0,
				&a6xx_coresight_regs_cx[22]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_1,
				&a6xx_coresight_regs_cx[23]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_2,
				&a6xx_coresight_regs_cx[24]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_maske_3,
				&a6xx_coresight_regs_cx[25]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_nibblee,
				&a6xx_coresight_regs_cx[26]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ptrc0,
				&a6xx_coresight_regs_cx[27]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_ptrc1,
				&a6xx_coresight_regs_cx[28]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_loadreg,
				&a6xx_coresight_regs_cx[29]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_idx,
				&a6xx_coresight_regs_cx[30]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_clrc,
				&a6xx_coresight_regs_cx[31]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_loadivt,
				&a6xx_coresight_regs_cx[32]);
static ADRENO_CORESIGHT_ATTR(cx_vbif_dbg_cntl,
				&a6xx_coresight_regs_cx[33]);
static ADRENO_CORESIGHT_ATTR(cx_dbg_lo_hi_gpio,
				&a6xx_coresight_regs_cx[34]);
static ADRENO_CORESIGHT_ATTR(cx_ext_trace_bus_cntl,
				&a6xx_coresight_regs_cx[35]);
static ADRENO_CORESIGHT_ATTR(cx_read_ahb_through_dbg,
				&a6xx_coresight_regs_cx[36]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_trace_buf1,
				&a6xx_coresight_regs_cx[37]);
static ADRENO_CORESIGHT_ATTR(cx_cfg_dbgbus_trace_buf2,
				&a6xx_coresight_regs_cx[38]);
static ADRENO_CORESIGHT_ATTR(cx_evt_cfg,
				&a6xx_coresight_regs_cx[39]);
static ADRENO_CORESIGHT_ATTR(cx_evt_intf_sel_0,
				&a6xx_coresight_regs_cx[40]);
static ADRENO_CORESIGHT_ATTR(cx_evt_intf_sel_1,
				&a6xx_coresight_regs_cx[41]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_cfg,
				&a6xx_coresight_regs_cx[42]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_counter_sel_0,
				&a6xx_coresight_regs_cx[43]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_counter_sel_1,
				&a6xx_coresight_regs_cx[44]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_counter_sel_2,
				&a6xx_coresight_regs_cx[45]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_counter_sel_3,
				&a6xx_coresight_regs_cx[46]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_trig_intf_sel_0,
				&a6xx_coresight_regs_cx[47]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_trig_intf_sel_1,
				&a6xx_coresight_regs_cx[48]);
static ADRENO_CORESIGHT_ATTR(cx_perf_atb_drain_cmd,
				&a6xx_coresight_regs_cx[49]);
static ADRENO_CORESIGHT_ATTR(cx_eco_cntl,
				&a6xx_coresight_regs_cx[50]);
static ADRENO_CORESIGHT_ATTR(cx_ahb_dbg_cntl,
				&a6xx_coresight_regs_cx[51]);

static struct attribute *a6xx_coresight_attrs[] = {
	&coresight_attr_cfg_dbgbus_sel_a.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_b.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_c.attr.attr,
	&coresight_attr_cfg_dbgbus_sel_d.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlt.attr.attr,
	&coresight_attr_cfg_dbgbus_cntlm.attr.attr,
	&coresight_attr_cfg_dbgbus_opl.attr.attr,
	&coresight_attr_cfg_dbgbus_ope.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivtl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maskl_3.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_0.attr.attr,
	&coresight_attr_cfg_dbgbus_bytel_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_0.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_1.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_2.attr.attr,
	&coresight_attr_cfg_dbgbus_ivte_3.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_0.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_1.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_2.attr.attr,
	&coresight_attr_cfg_dbgbus_maske_3.attr.attr,
	&coresight_attr_cfg_dbgbus_nibblee.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc0.attr.attr,
	&coresight_attr_cfg_dbgbus_ptrc1.attr.attr,
	&coresight_attr_cfg_dbgbus_loadreg.attr.attr,
	&coresight_attr_cfg_dbgbus_idx.attr.attr,
	&coresight_attr_cfg_dbgbus_clrc.attr.attr,
	&coresight_attr_cfg_dbgbus_loadivt.attr.attr,
	&coresight_attr_vbif_dbg_cntl.attr.attr,
	&coresight_attr_dbg_lo_hi_gpio.attr.attr,
	&coresight_attr_ext_trace_bus_cntl.attr.attr,
	&coresight_attr_read_ahb_through_dbg.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf1.attr.attr,
	&coresight_attr_cfg_dbgbus_trace_buf2.attr.attr,
	&coresight_attr_evt_cfg.attr.attr,
	&coresight_attr_evt_intf_sel_0.attr.attr,
	&coresight_attr_evt_intf_sel_1.attr.attr,
	&coresight_attr_perf_atb_cfg.attr.attr,
	&coresight_attr_perf_atb_counter_sel_0.attr.attr,
	&coresight_attr_perf_atb_counter_sel_1.attr.attr,
	&coresight_attr_perf_atb_counter_sel_2.attr.attr,
	&coresight_attr_perf_atb_counter_sel_3.attr.attr,
	&coresight_attr_perf_atb_trig_intf_sel_0.attr.attr,
	&coresight_attr_perf_atb_trig_intf_sel_1.attr.attr,
	&coresight_attr_perf_atb_drain_cmd.attr.attr,
	&coresight_attr_eco_cntl.attr.attr,
	&coresight_attr_ahb_dbg_cntl.attr.attr,
	NULL,
};

/*cx*/
static struct attribute *a6xx_coresight_attrs_cx[] = {
	&coresight_attr_cx_cfg_dbgbus_sel_a.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_sel_b.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_sel_c.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_sel_d.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_cntlt.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_cntlm.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_opl.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ope.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivtl_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maskl_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_bytel_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_bytel_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ivte_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_2.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_maske_3.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_nibblee.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ptrc0.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_ptrc1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_loadreg.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_idx.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_clrc.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_loadivt.attr.attr,
	&coresight_attr_cx_vbif_dbg_cntl.attr.attr,
	&coresight_attr_cx_dbg_lo_hi_gpio.attr.attr,
	&coresight_attr_cx_ext_trace_bus_cntl.attr.attr,
	&coresight_attr_cx_read_ahb_through_dbg.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_trace_buf1.attr.attr,
	&coresight_attr_cx_cfg_dbgbus_trace_buf2.attr.attr,
	&coresight_attr_cx_evt_cfg.attr.attr,
	&coresight_attr_cx_evt_intf_sel_0.attr.attr,
	&coresight_attr_cx_evt_intf_sel_1.attr.attr,
	&coresight_attr_cx_perf_atb_cfg.attr.attr,
	&coresight_attr_cx_perf_atb_counter_sel_0.attr.attr,
	&coresight_attr_cx_perf_atb_counter_sel_1.attr.attr,
	&coresight_attr_cx_perf_atb_counter_sel_2.attr.attr,
	&coresight_attr_cx_perf_atb_counter_sel_3.attr.attr,
	&coresight_attr_cx_perf_atb_trig_intf_sel_0.attr.attr,
	&coresight_attr_cx_perf_atb_trig_intf_sel_1.attr.attr,
	&coresight_attr_cx_perf_atb_drain_cmd.attr.attr,
	&coresight_attr_cx_eco_cntl.attr.attr,
	&coresight_attr_cx_ahb_dbg_cntl.attr.attr,
	NULL,
};

static const struct attribute_group a6xx_coresight_group = {
	.attrs = a6xx_coresight_attrs,
};

static const struct attribute_group *a6xx_coresight_groups[] = {
	&a6xx_coresight_group,
	NULL,
};

static const struct attribute_group a6xx_coresight_group_cx = {
	.attrs = a6xx_coresight_attrs_cx,
};

static const struct attribute_group *a6xx_coresight_groups_cx[] = {
	&a6xx_coresight_group_cx,
	NULL,
};

static struct adreno_coresight a6xx_coresight = {
	.registers = a6xx_coresight_regs,
	.count = ARRAY_SIZE(a6xx_coresight_regs),
	.groups = a6xx_coresight_groups,
};

static struct adreno_coresight a6xx_coresight_cx = {
	.registers = a6xx_coresight_regs_cx,
	.count = ARRAY_SIZE(a6xx_coresight_regs_cx),
	.groups = a6xx_coresight_groups_cx,
};
#endif

int a6xx_probe_common(struct platform_device *pdev,
	struct	adreno_device *adreno_dev, u32 chipid,
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

	/* Set the GPU busy counter for frequency scaling */
	adreno_dev->perfctr_pwr_lo = A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_0_L;

	/* Set the counter for IFPC */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		adreno_dev->perfctr_ifpc_lo =
			A6XX_GMU_CX_GMU_POWER_COUNTER_XOCLK_4_L;

	return adreno_device_probe(pdev, adreno_dev);
}

static int a6xx_probe(struct platform_device *pdev,
		u32 chipid, const struct adreno_gpu_core *gpucore)
{
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	int ret;

	adreno_dev = (struct adreno_device *)
		of_device_get_match_data(&pdev->dev);

	memset(adreno_dev, 0, sizeof(*adreno_dev));

	ret = a6xx_probe_common(pdev, adreno_dev, chipid, gpucore);
	if (ret)
		return ret;

	device = KGSL_DEVICE(adreno_dev);

	timer_setup(&device->idle_timer, kgsl_timer, 0);

	INIT_WORK(&device->idle_check_ws, kgsl_idle_check);

	adreno_dev->irq_mask = A6XX_INT_MASK;

	return 0;
}


static unsigned int a6xx_ccu_invalidate(struct adreno_device *adreno_dev,
	unsigned int *cmds)
{
	/* CCU_INVALIDATE_DEPTH */
	*cmds++ = cp_packet(adreno_dev, CP_EVENT_WRITE, 1);
	*cmds++ = 24;

	/* CCU_INVALIDATE_COLOR */
	*cmds++ = cp_packet(adreno_dev, CP_EVENT_WRITE, 1);
	*cmds++ = 25;

	return 4;
}

/* Register offset defines for A6XX, in order of enum adreno_regs */
static unsigned int a6xx_register_offsets[ADRENO_REG_REGISTER_MAX] = {

	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, A6XX_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE_HI, A6XX_CP_RB_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR_LO,
				A6XX_CP_RB_RPTR_ADDR_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR_HI,
				A6XX_CP_RB_RPTR_ADDR_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, A6XX_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, A6XX_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_CNTL, A6XX_CP_RB_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ME_CNTL, A6XX_CP_SQE_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE, A6XX_CP_IB1_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BASE_HI, A6XX_CP_IB1_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB1_BUFSZ, A6XX_CP_IB1_REM_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE, A6XX_CP_IB2_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BASE_HI, A6XX_CP_IB2_BASE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_IB2_BUFSZ, A6XX_CP_IB2_REM_SIZE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_ADDR, A6XX_CP_ROQ_DBG_ADDR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_ROQ_DATA, A6XX_CP_ROQ_DBG_DATA),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT, A6XX_CP_CONTEXT_SWITCH_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_LO,
			A6XX_CP_CONTEXT_SWITCH_SMMU_INFO_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_SMMU_INFO_HI,
			A6XX_CP_CONTEXT_SWITCH_SMMU_INFO_HI),
	ADRENO_REG_DEFINE(
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_LO,
			A6XX_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_LO),
	ADRENO_REG_DEFINE(
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_HI,
			A6XX_CP_CONTEXT_SWITCH_PRIV_NON_SECURE_RESTORE_ADDR_HI),
	ADRENO_REG_DEFINE(
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO,
			A6XX_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_LO),
	ADRENO_REG_DEFINE(
		ADRENO_REG_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI,
			A6XX_CP_CONTEXT_SWITCH_PRIV_SECURE_RESTORE_ADDR_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_LO,
			A6XX_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_HI,
			A6XX_CP_CONTEXT_SWITCH_NON_PRIV_RESTORE_ADDR_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_PREEMPT_LEVEL_STATUS,
			A6XX_CP_CONTEXT_SWITCH_LEVEL_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, A6XX_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS3, A6XX_RBBM_STATUS3),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD0,
					A6XX_RBBM_PERFCTR_LOAD_CMD0),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD1,
					A6XX_RBBM_PERFCTR_LOAD_CMD1),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD2,
					A6XX_RBBM_PERFCTR_LOAD_CMD2),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_CMD3,
					A6XX_RBBM_PERFCTR_LOAD_CMD3),

	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, A6XX_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_STATUS, A6XX_RBBM_INT_0_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_CLOCK_CTL, A6XX_RBBM_CLOCK_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, A6XX_RBBM_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_LO,
				A6XX_RBBM_PERFCTR_LOAD_VALUE_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_PERFCTR_LOAD_VALUE_HI,
				A6XX_RBBM_PERFCTR_LOAD_VALUE_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_VERSION, A6XX_VBIF_VERSION),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AO_HOST_INTERRUPT_MASK,
				A6XX_GMU_AO_HOST_INTERRUPT_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AHB_FENCE_STATUS,
				A6XX_GMU_AHB_FENCE_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_GMU2HOST_INTR_MASK,
				A6XX_GMU_GMU2HOST_INTR_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_GMU_AO_RBBM_INT_UNMASKED_STATUS,
				A6XX_GMU_RBBM_INT_UNMASKED_STATUS),
};

static int cpu_gpu_lock(struct cpu_gpu_lock *lock)
{
	unsigned long timeout = jiffies + msecs_to_jiffies(1000);

	/* Indicate that the CPU wants the lock */
	lock->flag_kmd = 1;

	/* post the request */
	wmb();

	/* Wait for our turn */
	lock->turn = 0;

	/* Finish all memory transactions before moving on */
	mb();

	/*
	 * Spin here while GPU ucode holds the lock, lock->flag_ucode will
	 * be set to 0 after GPU ucode releases the lock. Minimum wait time
	 * is 1 second and this should be enough for GPU to release the lock
	 */
	while (lock->flag_ucode == 1 && lock->turn == 0) {
		cpu_relax();
		/* Get the latest updates from GPU */
		rmb();

		if (time_after(jiffies, timeout))
			break;
	}

	if (lock->flag_ucode == 1 && lock->turn == 0)
		return -EBUSY;

	return 0;
}

static void cpu_gpu_unlock(struct cpu_gpu_lock *lock)
{
	/* Make sure all writes are done before releasing the lock */
	wmb();
	lock->flag_kmd = 0;
}

int a6xx_perfcounter_update(struct adreno_device *adreno_dev,
	struct adreno_perfcount_register *reg, bool update_reg)
{
	void *ptr = adreno_dev->pwrup_reglist->hostptr;
	struct cpu_gpu_lock *lock = ptr;
	u32 *data = ptr + sizeof(*lock);
	int i, offset = 0;

	if (cpu_gpu_lock(lock)) {
		cpu_gpu_unlock(lock);
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

		if (data[offset] == A6XX_RBBM_PERFCTR_CNTL)
			break;

		offset += 2;
	}

	/*
	 * For all targets A6XX_RBBM_PERFCTR_CNTL needs to be the last entry,
	 * so overwrite the existing A6XX_RBBM_PERFCNTL_CTRL and add it back to
	 * the end.
	 */

	data[offset] = reg->select;
	data[offset + 1] = reg->countable;
	data[offset + 2] = A6XX_RBBM_PERFCTR_CNTL,
	data[offset + 3] = 1;

	lock->list_length += 2;

update:
	if (update_reg)
		kgsl_regwrite(KGSL_DEVICE(adreno_dev), reg->select,
			reg->countable);

	cpu_gpu_unlock(lock);
	return 0;
}

#if IS_ENABLED(CONFIG_COMMON_CLK_QCOM)
static void a6xx_clk_set_options(struct adreno_device *adreno_dev,
	const char *name, struct clk *clk, bool on)
{
	/* Handle clock settings for GFX PSCBCs */
	if (on) {
		if (!strcmp(name, "mem_iface_clk")) {
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_PERIPH);
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_MEM);
		} else if (!strcmp(name, "core_clk")) {
			qcom_clk_set_flags(clk, CLKFLAG_RETAIN_PERIPH);
			qcom_clk_set_flags(clk, CLKFLAG_RETAIN_MEM);
		}
	} else {
		if (!strcmp(name, "core_clk")) {
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_PERIPH);
			qcom_clk_set_flags(clk, CLKFLAG_NORETAIN_MEM);
		}
	}
}
#endif

u64 a6xx_read_alwayson(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 lo = 0, hi = 0, tmp = 0;

	if (!gmu_core_isenabled(device)) {
		kgsl_regread(device, A6XX_CP_ALWAYS_ON_COUNTER_LO, &lo);
		kgsl_regread(device, A6XX_CP_ALWAYS_ON_COUNTER_HI, &hi);
	} else {
		/* Always use the GMU AO counter when doing a AHB read */
		gmu_core_regread(device, A6XX_GMU_ALWAYS_ON_COUNTER_H, &hi);
		gmu_core_regread(device, A6XX_GMU_ALWAYS_ON_COUNTER_L, &lo);

		/* Check for overflow */
		gmu_core_regread(device, A6XX_GMU_ALWAYS_ON_COUNTER_H, &tmp);

		if (hi != tmp) {
			gmu_core_regread(device, A6XX_GMU_ALWAYS_ON_COUNTER_L,
				&lo);
			hi = tmp;
		}
	}

	return (((u64) hi) << 32) | lo;
}

static void a619_holi_regulator_disable_poll(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	/* Set the parent in retention voltage to disable CPR interrupts */
	kgsl_regulator_set_voltage(device->dev, pwr->gx_gdsc_parent,
			pwr->gx_gdsc_parent_min_corner);

	if (!kgsl_regulator_disable_wait(pwr->gx_gdsc, 200))
		dev_err(device->dev, "Regulator vdd is stuck on\n");

	/* Remove the vote for the vdd parent supply */
	kgsl_regulator_set_voltage(device->dev, pwr->gx_gdsc_parent, 0);

	a6xx_cx_regulator_disable_wait(pwr->cx_gdsc, device, 200);

}

const struct adreno_gpudev adreno_a6xx_gpudev = {
	.reg_offsets = a6xx_register_offsets,
	.probe = a6xx_probe,
	.start = a6xx_start,
	.snapshot = a6xx_snapshot,
	.init = a6xx_init,
	.irq_handler = a6xx_irq_handler,
	.rb_start = a6xx_rb_start,
	.read_throttling_counters = a6xx_read_throttling_counters,
	.microcode_read = a6xx_microcode_read,
	.gpu_keepalive = a6xx_gpu_keepalive,
	.hw_isidle = a6xx_hw_isidle,
	.iommu_fault_block = a6xx_iommu_fault_block,
	.reset = a6xx_reset,
	.preemption_pre_ibsubmit = a6xx_preemption_pre_ibsubmit,
	.preemption_post_ibsubmit = a6xx_preemption_post_ibsubmit,
	.preemption_init = a6xx_preemption_init,
	.preemption_schedule = a6xx_preemption_schedule,
	.set_marker = a6xx_set_marker,
	.preemption_context_init = a6xx_preemption_context_init,
	.ccu_invalidate = a6xx_ccu_invalidate,
#ifdef CONFIG_QCOM_KGSL_CORESIGHT
	.coresight = {&a6xx_coresight, &a6xx_coresight_cx},
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_QCOM)
	.clk_set_options = a6xx_clk_set_options,
#endif
	.read_alwayson = a6xx_read_alwayson,
	.power_ops = &adreno_power_operations,
	.clear_pending_transactions = a6xx_clear_pending_transactions,
	.deassert_gbif_halt = a6xx_deassert_gbif_halt,
};

const struct adreno_gpudev adreno_a6xx_hwsched_gpudev = {
	.reg_offsets = a6xx_register_offsets,
	.probe = a6xx_hwsched_probe,
	.snapshot = a6xx_hwsched_snapshot,
	.irq_handler = a6xx_irq_handler,
	.read_throttling_counters = a6xx_read_throttling_counters,
	.iommu_fault_block = a6xx_iommu_fault_block,
	.preemption_context_init = a6xx_preemption_context_init,
	.context_detach = a6xx_hwsched_context_detach,
#ifdef CONFIG_QCOM_KGSL_CORESIGHT
	.coresight = {&a6xx_coresight, &a6xx_coresight_cx},
#endif
	.read_alwayson = a6xx_read_alwayson,
	.power_ops = &a6xx_hwsched_power_ops,
};

const struct adreno_gpudev adreno_a6xx_gmu_gpudev = {
	.reg_offsets = a6xx_register_offsets,
	.probe = a6xx_gmu_device_probe,
	.start = a6xx_start,
	.snapshot = a6xx_gmu_snapshot,
	.init = a6xx_init,
	.irq_handler = a6xx_irq_handler,
	.rb_start = a6xx_rb_start,
	.regulator_enable = a6xx_gmu_sptprac_enable,
	.regulator_disable = a6xx_gmu_sptprac_disable,
	.read_throttling_counters = a6xx_read_throttling_counters,
	.microcode_read = a6xx_microcode_read,
	.gpu_keepalive = a6xx_gpu_keepalive,
	.hw_isidle = a6xx_hw_isidle,
	.iommu_fault_block = a6xx_iommu_fault_block,
	.reset = a6xx_gmu_restart,
	.preemption_pre_ibsubmit = a6xx_preemption_pre_ibsubmit,
	.preemption_post_ibsubmit = a6xx_preemption_post_ibsubmit,
	.preemption_init = a6xx_preemption_init,
	.preemption_schedule = a6xx_preemption_schedule,
	.set_marker = a6xx_set_marker,
	.preemption_context_init = a6xx_preemption_context_init,
	.sptprac_is_on = a6xx_gmu_sptprac_is_on,
	.ccu_invalidate = a6xx_ccu_invalidate,
#ifdef CONFIG_QCOM_KGSL_CORESIGHT
	.coresight = {&a6xx_coresight, &a6xx_coresight_cx},
#endif
	.read_alwayson = a6xx_read_alwayson,
	.power_ops = &a6xx_gmu_power_ops,
};

const struct adreno_gpudev adreno_a6xx_rgmu_gpudev = {
	.reg_offsets = a6xx_register_offsets,
	.probe = a6xx_rgmu_device_probe,
	.start = a6xx_start,
	.snapshot = a6xx_rgmu_snapshot,
	.init = a6xx_init,
	.irq_handler = a6xx_irq_handler,
	.rb_start = a6xx_rb_start,
	.read_throttling_counters = a6xx_read_throttling_counters,
	.microcode_read = a6xx_microcode_read,
	.gpu_keepalive = a6xx_gpu_keepalive,
	.hw_isidle = a6xx_hw_isidle,
	.iommu_fault_block = a6xx_iommu_fault_block,
	.reset = a6xx_rgmu_restart,
	.preemption_pre_ibsubmit = a6xx_preemption_pre_ibsubmit,
	.preemption_post_ibsubmit = a6xx_preemption_post_ibsubmit,
	.preemption_init = a6xx_preemption_init,
	.preemption_schedule = a6xx_preemption_schedule,
	.set_marker = a6xx_set_marker,
	.preemption_context_init = a6xx_preemption_context_init,
	.ccu_invalidate = a6xx_ccu_invalidate,
#ifdef CONFIG_QCOM_KGSL_CORESIGHT
	.coresight = {&a6xx_coresight, &a6xx_coresight_cx},
#endif
	.read_alwayson = a6xx_read_alwayson,
	.power_ops = &a6xx_rgmu_power_ops,
};

const struct adreno_gpudev adreno_a619_holi_gpudev = {
	.reg_offsets = a6xx_register_offsets,
	.probe = a6xx_probe,
	.start = a6xx_start,
	.snapshot = a6xx_snapshot,
	.init = a6xx_holi_init,
	.irq_handler = a6xx_irq_handler,
	.rb_start = a6xx_rb_start,
	.regulator_enable = a6xx_holi_gmu_sptprac_enable,
	.regulator_disable = a6xx_holi_gmu_sptprac_disable,
	.read_throttling_counters = a6xx_read_throttling_counters,
	.microcode_read = a6xx_microcode_read,
	.gpu_keepalive = a6xx_gpu_keepalive,
	.hw_isidle = a619_holi_hw_isidle,
	.iommu_fault_block = a6xx_iommu_fault_block,
	.reset = a6xx_reset,
	.preemption_pre_ibsubmit = a6xx_preemption_pre_ibsubmit,
	.preemption_post_ibsubmit = a6xx_preemption_post_ibsubmit,
	.preemption_init = a6xx_preemption_init,
	.preemption_schedule = a6xx_preemption_schedule,
	.set_marker = a6xx_set_marker,
	.preemption_context_init = a6xx_preemption_context_init,
	.sptprac_is_on = a6xx_holi_gmu_sptprac_is_on,
	.ccu_invalidate = a6xx_ccu_invalidate,
#ifdef CONFIG_QCOM_KGSL_CORESIGHT
	.coresight = {&a6xx_coresight, &a6xx_coresight_cx},
#endif
#if IS_ENABLED(CONFIG_COMMON_CLK_QCOM)
	.clk_set_options = a6xx_clk_set_options,
#endif
	.read_alwayson = a6xx_read_alwayson,
	.power_ops = &adreno_power_operations,
	.clear_pending_transactions = a6xx_clear_pending_transactions,
	.deassert_gbif_halt = a6xx_deassert_gbif_halt,
	.regulator_disable_poll = a619_holi_regulator_disable_poll,
};

const struct adreno_gpudev adreno_a630_gpudev = {
	.reg_offsets = a6xx_register_offsets,
	.probe = a6xx_gmu_device_probe,
	.start = a6xx_start,
	.snapshot = a6xx_gmu_snapshot,
	.init = a6xx_init,
	.irq_handler = a6xx_irq_handler,
	.rb_start = a6xx_rb_start,
	.regulator_enable = a6xx_gmu_sptprac_enable,
	.regulator_disable = a6xx_gmu_sptprac_disable,
	.read_throttling_counters = a6xx_read_throttling_counters,
	.microcode_read = a6xx_microcode_read,
	.gpu_keepalive = a6xx_gpu_keepalive,
	.hw_isidle = a6xx_hw_isidle,
	.iommu_fault_block = a6xx_iommu_fault_block,
	.reset = a6xx_gmu_restart,
	.preemption_pre_ibsubmit = a6xx_preemption_pre_ibsubmit,
	.preemption_post_ibsubmit = a6xx_preemption_post_ibsubmit,
	.preemption_init = a6xx_preemption_init,
	.preemption_schedule = a6xx_preemption_schedule,
	.set_marker = a6xx_set_marker,
	.preemption_context_init = a6xx_preemption_context_init,
	.sptprac_is_on = a6xx_gmu_sptprac_is_on,
	.ccu_invalidate = a6xx_ccu_invalidate,
#ifdef CONFIG_QCOM_KGSL_CORESIGHT
	.coresight = {&a6xx_coresight, &a6xx_coresight_cx},
#endif
	.read_alwayson = a6xx_read_alwayson,
	.power_ops = &a630_gmu_power_ops,
};
