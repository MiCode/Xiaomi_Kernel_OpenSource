/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/firmware.h>
#include <linux/pm_opp.h>

#include "adreno.h"
#include "a6xx_reg.h"
#include "adreno_cp_parser.h"
#include "adreno_trace.h"
#include "adreno_pm4types.h"
#include "adreno_perfcounter.h"
#include "adreno_ringbuffer.h"
#include "adreno_llc.h"
#include "kgsl_sharedmem.h"
#include "kgsl_log.h"
#include "kgsl.h"
#include <linux/msm_kgsl.h>

#define A6XX_CP_RB_CNTL_DEFAULT (((ilog2(4) << 8) & 0x1F00) | \
		(ilog2(KGSL_RB_DWORDS >> 1) & 0x3F))

#define MIN_HBB		13

#define A6XX_LLC_NUM_GPU_SCIDS		5
#define A6XX_GPU_LLC_SCID_NUM_BITS	5
#define A6XX_GPU_LLC_SCID_MASK \
	((1 << (A6XX_LLC_NUM_GPU_SCIDS * A6XX_GPU_LLC_SCID_NUM_BITS)) - 1)
#define A6XX_GPUHTW_LLC_SCID_SHIFT	25
#define A6XX_GPUHTW_LLC_SCID_MASK \
	(((1 << A6XX_GPU_LLC_SCID_NUM_BITS) - 1) << A6XX_GPUHTW_LLC_SCID_SHIFT)

#define A6XX_GPU_CX_REG_BASE		0x509E000
#define A6XX_GPU_CX_REG_SIZE		0x1000

static const struct adreno_vbif_data a630_vbif[] = {
	{A6XX_VBIF_GATE_OFF_WRREQ_EN, 0x00000009},
	{A6XX_RBBM_VBIF_CLIENT_QOS_CNTL, 0x3},
	{0, 0},
};

static const struct adreno_vbif_platform a6xx_vbif_platforms[] = {
	{ adreno_is_a630, a630_vbif },
};

static struct a6xx_protected_regs {
	unsigned int base;
	unsigned int count;
	int read_protect;
} a6xx_protected_regs_group[] = {
	{ 0x600, 0x51, 0 },
	{ 0xAE50, 0x2, 1 },
	{ 0x9624, 0x13, 1 },
	{ 0x8630, 0x8, 1 },
	{ 0x9E70, 0x1, 1 },
	{ 0x9E78, 0x187, 1 },
	{ 0xF000, 0x810, 1 },
	{ 0xFC00, 0x3, 0 },
	{ 0x50E, 0x0, 1 },
	{ 0x50F, 0x0, 0 },
	{ 0x510, 0x0, 1 },
	{ 0x0, 0x4F9, 0 },
	{ 0x501, 0xA, 0 },
	{ 0x511, 0x44, 0 },
	{ 0xE00, 0xE, 1 },
	{ 0x8E00, 0x0, 1 },
	{ 0x8E50, 0xF, 1 },
	{ 0xBE02, 0x0, 1 },
	{ 0xBE20, 0x11F3, 1 },
	{ 0x800, 0x82, 1 },
	{ 0x8A0, 0x8, 1 },
	{ 0x8AB, 0x19, 1 },
	{ 0x900, 0x4D, 1 },
	{ 0x98D, 0x76, 1 },
	{ 0x8D0, 0x23, 0 },
	{ 0x980, 0x4, 0 },
	{ 0xA630, 0x0, 1 },
};

/* Print some key registers if a spin-for-idle times out */
static void spin_idle_debug(struct kgsl_device *device,
		const char *str)
{
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
		" rb=%X/%X rbbm_status=%8.8X/%8.8X int_0_status=%8.8X\n",
		rptr, wptr, status, status3, intstatus);
	dev_err(device->dev, " hwfault=%8.8X\n", hwfault);
}

static void a6xx_platform_setup(struct adreno_device *adreno_dev)
{
	uint64_t addr;

	/* Calculate SP local and private mem addresses */
	addr = ALIGN(ADRENO_UCHE_GMEM_BASE + adreno_dev->gmem_size, SZ_64K);
	adreno_dev->sp_local_gpuaddr = addr;
	adreno_dev->sp_pvt_gpuaddr = addr + SZ_64K;
}

/**
 * a6xx_protect_init() - Initializes register protection on a6xx
 * @device: Pointer to the device structure
 * Performs register writes to enable protected access to sensitive
 * registers
 */
static void a6xx_protect_init(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int i;

	/* enable access protection to privileged registers */
	kgsl_regwrite(device, A6XX_CP_PROTECT_CNTL, 0x00000007);

	if (ARRAY_SIZE(a6xx_protected_regs_group) >
			adreno_dev->gpucore->num_protected_regs)
		WARN(1, "Size exceeds the num of protection regs available\n");

	for (i = 0; i < ARRAY_SIZE(a6xx_protected_regs_group); i++) {
		struct a6xx_protected_regs *regs =
					&a6xx_protected_regs_group[i];

		kgsl_regwrite(device, A6XX_CP_PROTECT_REG + i,
				regs->base | (regs->count << 18) |
				(regs->read_protect << 31));
	}

}

static void a6xx_enable_64bit(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

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
}

/*
 * a6xx_start() - Device start
 * @adreno_dev: Pointer to adreno device
 *
 * a6xx device start
 */
static void a6xx_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int bit, mal, mode, glbl_inv;
	unsigned int amsbc = 0;

	adreno_vbif_start(adreno_dev, a6xx_vbif_platforms,
			ARRAY_SIZE(a6xx_vbif_platforms));
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

	/* Program the GMEM VA range for the UCHE path */
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MIN_LO,
				ADRENO_UCHE_GMEM_BASE);
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MIN_HI, 0x0);
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MAX_LO,
				ADRENO_UCHE_GMEM_BASE +
				adreno_dev->gmem_size - 1);
	kgsl_regwrite(device, A6XX_UCHE_GMEM_RANGE_MAX_HI, 0x0);

	kgsl_regwrite(device, A6XX_UCHE_FILTER_CNTL, 0x804);
	kgsl_regwrite(device, A6XX_UCHE_CACHE_WAYS, 0x4);

	kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_2, 0x010000C0);
	kgsl_regwrite(device, A6XX_CP_ROQ_THRESHOLDS_1, 0x8040362C);

	/* Setting the mem pool size */
	kgsl_regwrite(device, A6XX_CP_MEM_POOL_SIZE, 128);

	/* Setting the primFifo thresholds default values */
	kgsl_regwrite(device, A6XX_PC_DBG_ECO_CNTL, (0x300 << 11));

	/* Disable secured mode */
	kgsl_regwrite(device, A6XX_RBBM_SECVID_TRUST_CNTL, 0x0);

	/* Set the AHB default slave response to "ERROR" */
	kgsl_regwrite(device, A6XX_CP_AHB_CNTL, 0x1);

	if (of_property_read_u32(device->pdev->dev.of_node,
		"qcom,highest-bank-bit", &bit))
		bit = MIN_HBB;

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
	default:
		break;
	}

	if (bit >= 13 && bit <= 16)
		bit = (bit - 13) & 0x03;
	else
		bit = 0;

	mal = (mal == 64) ? 1 : 0;

	/* (1 << 29)globalInvFlushFilterDis bit needs to be set for A630 V1 */
	glbl_inv = (adreno_is_a630v1(adreno_dev)) ? 1 : 0;

	kgsl_regwrite(device, A6XX_RB_NC_MODE_CNTL, (amsbc << 4) | (mal << 3) |
							(bit << 1) | mode);
	kgsl_regwrite(device, A6XX_TPL1_NC_MODE_CNTL, (mal << 3) |
							(bit << 1) | mode);
	kgsl_regwrite(device, A6XX_SP_NC_MODE_CNTL, (mal << 3) | (bit << 1) |
								mode);

	kgsl_regwrite(device, A6XX_UCHE_MODE_CNTL, (glbl_inv << 29) |
						(mal << 23) | (bit << 21));

	kgsl_regwrite(device, A6XX_RBBM_INTERFACE_HANG_INT_CNTL,
					  (1 << 30) | 0x4000);

	/* Set TWOPASSUSEWFI in A6XX_PC_DBG_ECO_CNTL if requested */
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_TWO_PASS_USE_WFI))
		kgsl_regrmw(device, A6XX_PC_DBG_ECO_CNTL, 0, (1 << 8));

	a6xx_protect_init(adreno_dev);
}

/*
 * a6xx_microcode_load() - Load microcode
 * @adreno_dev: Pointer to adreno device
 */
static int a6xx_microcode_load(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct adreno_firmware *fw = ADRENO_FW(adreno_dev, ADRENO_FW_SQE);
	uint64_t gpuaddr;

	gpuaddr = fw->memdesc.gpuaddr;
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_LO,
				lower_32_bits(gpuaddr));
	kgsl_regwrite(device, A6XX_CP_SQE_INSTR_BASE_HI,
				upper_32_bits(gpuaddr));

	return 0;
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

#define CP_INIT_MASK (CP_INIT_MAX_CONTEXT | \
		CP_INIT_ERROR_DETECTION_CONTROL | \
		CP_INIT_HEADER_DUMP | \
		CP_INIT_DEFAULT_RESET_STATE | \
		CP_INIT_UCODE_WORKAROUND_MASK)

static void _set_ordinals(struct adreno_device *adreno_dev,
		unsigned int *cmds, unsigned int count)
{
	unsigned int *start = cmds;

	/* Enabled ordinal mask */
	*cmds++ = CP_INIT_MASK;

	if (CP_INIT_MASK & CP_INIT_MAX_CONTEXT)
		*cmds++ = 0x00000003;

	if (CP_INIT_MASK & CP_INIT_ERROR_DETECTION_CONTROL)
		*cmds++ = 0x20000000;

	if (CP_INIT_MASK & CP_INIT_HEADER_DUMP) {
		/* Header dump address */
		*cmds++ = 0x00000000;
		/* Header dump enable and dump size */
		*cmds++ = 0x00000000;
	}

	if (CP_INIT_MASK & CP_INIT_DRAWCALL_FILTER_RANGE) {
		/* Start range */
		*cmds++ = 0x00000000;
		/* End range (inclusive) */
		*cmds++ = 0x00000000;
	}

	if (CP_INIT_MASK & CP_INIT_UCODE_WORKAROUND_MASK)
		*cmds++ = 0x00000000;

	/* Pad rest of the cmds with 0's */
	while ((unsigned int)(cmds - start) < count)
		*cmds++ = 0x0;
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
	unsigned int *cmds;
	int ret;

	cmds = adreno_ringbuffer_allocspace(rb, 9);
	if (IS_ERR(cmds))
		return PTR_ERR(cmds);

	*cmds++ = cp_type7_packet(CP_ME_INIT, 8);

	_set_ordinals(adreno_dev, cmds, 8);

	ret = adreno_ringbuffer_submit_spin(rb, NULL, 2000);
	if (ret)
		spin_idle_debug(KGSL_DEVICE(adreno_dev),
				"CP initialization failed to idle\n");

	return ret;
}

/*
 * a6xx_rb_start() - Start the ringbuffer
 * @adreno_dev: Pointer to adreno device
 * @start_type: Warm or cold start
 */
static int a6xx_rb_start(struct adreno_device *adreno_dev,
			 unsigned int start_type)
{
	struct adreno_ringbuffer *rb = ADRENO_CURRENT_RINGBUFFER(adreno_dev);
	struct kgsl_device *device = &adreno_dev->dev;
	uint64_t addr;
	int ret;

	addr = SCRATCH_RPTR_GPU_ADDR(device, rb->id);

	adreno_writereg64(adreno_dev, ADRENO_REG_CP_RB_RPTR_ADDR_LO,
				ADRENO_REG_CP_RB_RPTR_ADDR_HI, addr);

	/*
	 * The size of the ringbuffer in the hardware is the log2
	 * representation of the size in quadwords (sizedwords / 2).
	 */
	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_CNTL,
					A6XX_CP_RB_CNTL_DEFAULT);

	adreno_writereg(adreno_dev, ADRENO_REG_CP_RB_BASE,
			rb->buffer_desc.gpuaddr);

	ret = a6xx_microcode_load(adreno_dev);
	if (ret)
		return ret;

	/* Clear the SQE_HALT to start the CP engine */
	kgsl_regwrite(device, A6XX_CP_SQE_CNTL, 1);

	return a6xx_send_cp_init(adreno_dev, rb);
}

static int _load_firmware(struct kgsl_device *device, const char *fwfile,
			  struct adreno_firmware *firmware)
{
	const struct firmware *fw = NULL;
	int ret;

	ret = request_firmware(&fw, fwfile, device->dev);

	if (ret) {
		KGSL_DRV_ERR(device, "request_firmware(%s) failed: %d\n",
				fwfile, ret);
		return ret;
	}

	ret = kgsl_allocate_global(device, &firmware->memdesc, fw->size - 4,
				KGSL_MEMFLAGS_GPUREADONLY, 0, "ucode");

	if (!ret) {
		memcpy(firmware->memdesc.hostptr, &fw->data[4], fw->size - 4);
		firmware->size = (fw->size - 4) / sizeof(uint32_t);
		firmware->version = *(unsigned int *)&fw->data[4];
	}

	release_firmware(fw);

	return ret;
}

#define SPTPRAC_POWER_CONTROL_OFFSET	0x204
#define SPTPRAC_PWR_CLK_STATUS_OFFSET	0x14340
#define SPTPRAC_POWERON_CTRL_MASK	0x00778000
#define SPTPRAC_POWEROFF_CTRL_MASK	0x00778001
#define SPTPRAC_POWERON_STATUS_MASK	BIT(3)
#define SPTPRAC_CTRL_TIMEOUT		10 /* ms */

static int a6xx_sptprac_enable(struct adreno_device *adreno_dev)
{
	void __iomem *gmu_reg;
	unsigned long t;
	unsigned int val;
	int ret;

	gmu_reg = ioremap(0x506a000, 0x26000);

	__raw_writel(SPTPRAC_POWERON_CTRL_MASK,
			gmu_reg + SPTPRAC_POWER_CONTROL_OFFSET);

	/* Make sure the above write is observed before the reads below */
	wmb();

	t = jiffies + msecs_to_jiffies(SPTPRAC_CTRL_TIMEOUT);

	ret = -EINVAL;
	while (!time_after(jiffies, t)) {
		val = __raw_readl(gmu_reg + SPTPRAC_PWR_CLK_STATUS_OFFSET);
		/*
		 * Make sure the above read completes before polling the
		 * register again
		 */
		rmb();

		if ((val & SPTPRAC_POWERON_STATUS_MASK) ==
			SPTPRAC_POWERON_STATUS_MASK) {
			ret = 0;
			break;
		}
		cpu_relax();
	}

	iounmap(gmu_reg);

	return ret;
}

static void a6xx_sptprac_disable(struct adreno_device *adreno_dev)
{
	void __iomem *gmu_reg;

	gmu_reg = ioremap(0x506a000, 0x26000);

	__raw_writel(SPTPRAC_POWEROFF_CTRL_MASK,
			gmu_reg + SPTPRAC_POWER_CONTROL_OFFSET);
	/* Make sure the above write posts before moving on */
	wmb();

	iounmap(gmu_reg);
}

/*
 * a6xx_microcode_read() - Read microcode
 * @adreno_dev: Pointer to adreno device
 */
static int a6xx_microcode_read(struct adreno_device *adreno_dev)
{
	return _load_firmware(KGSL_DEVICE(adreno_dev),
			adreno_dev->gpucore->sqefw_name,
			ADRENO_FW(adreno_dev, ADRENO_FW_SQE));
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
		KGSL_DRV_CRIT_RATELIMIT(device,
		"CP opcode error interrupt | possible opcode=0x%8.8x\n");
	}
	if (status1 & BIT(A6XX_CP_UCODE_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device, "CP ucode error interrupt\n");
	if (status1 & BIT(A6XX_CP_HW_FAULT_ERROR)) {
		kgsl_regread(device, A6XX_CP_HW_FAULT, &status2);
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP | Ringbuffer HW fault | status=%x\n",
			status2);
	}
	if (status1 & BIT(A6XX_CP_REGISTER_PROTECTION_ERROR)) {
		kgsl_regread(device, A6XX_CP_PROTECT_STATUS, &status2);
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP | Protected mode error | %s | addr=%x | status=%x\n",
			status2 & (1 << 20) ? "READ" : "WRITE",
			status2 & 0x3FFFF, status2);
	}
	if (status1 & BIT(A6XX_CP_AHB_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP AHB error interrupt\n");
	if (status1 & BIT(A6XX_CP_VSD_PARITY_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP VSD decoder parity error\n");
	if (status1 & BIT(A6XX_CP_ILLEGAL_INSTR_ERROR))
		KGSL_DRV_CRIT_RATELIMIT(device,
			"CP Illegal instruction error\n");

}

static void a6xx_err_callback(struct adreno_device *adreno_dev, int bit)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	switch (bit) {
	case A6XX_INT_CP_AHB_ERROR:
		KGSL_DRV_CRIT_RATELIMIT(device, "CP: AHB bus error\n");
		break;
	case A6XX_INT_ATB_ASYNCFIFO_OVERFLOW:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: ATB ASYNC overflow\n");
		break;
	case A6XX_INT_RBBM_ATB_BUS_OVERFLOW:
		KGSL_DRV_CRIT_RATELIMIT(device, "RBBM: ATB bus overflow\n");
		break;
	case A6XX_INT_UCHE_OOB_ACCESS:
		KGSL_DRV_CRIT_RATELIMIT(device, "UCHE: Out of bounds access\n");
		break;
	case A6XX_INT_UCHE_TRAP_INTR:
		KGSL_DRV_CRIT_RATELIMIT(device, "UCHE: Trap interrupt\n");
		break;
	default:
		KGSL_DRV_CRIT_RATELIMIT(device, "Unknown interrupt %d\n", bit);
	}
}

/* GPU System Cache control registers */
#define A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_0   0x4
#define A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1   0x8

static inline void _reg_rmw(void __iomem *regaddr,
	unsigned int mask, unsigned int bits)
{
	unsigned int val = 0;

	val = __raw_readl(regaddr);
	/* Make sure the above read completes before we proceed  */
	rmb();
	val &= ~mask;
	__raw_writel(val | bits, regaddr);
	/* Make sure the above write posts before we proceed*/
	wmb();
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
	void __iomem *gpu_cx_reg;

	gpu_scid = adreno_llc_get_scid(adreno_dev->gpu_llc_slice);
	for (i = 0; i < A6XX_LLC_NUM_GPU_SCIDS; i++)
		gpu_cntl1_val = (gpu_cntl1_val << A6XX_GPU_LLC_SCID_NUM_BITS)
			| gpu_scid;

	gpu_cx_reg = ioremap(A6XX_GPU_CX_REG_BASE, A6XX_GPU_CX_REG_SIZE);
	_reg_rmw(gpu_cx_reg + A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1,
			A6XX_GPU_LLC_SCID_MASK, gpu_cntl1_val);
	iounmap(gpu_cx_reg);
}

/*
 * a6xx_llc_configure_gpuhtw_scid() - Program the SCID for GPU pagetables
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_configure_gpuhtw_scid(struct adreno_device *adreno_dev)
{
	uint32_t gpuhtw_scid;
	void __iomem *gpu_cx_reg;

	gpuhtw_scid = adreno_llc_get_scid(adreno_dev->gpuhtw_llc_slice);

	gpu_cx_reg = ioremap(A6XX_GPU_CX_REG_BASE, A6XX_GPU_CX_REG_SIZE);
	extregrmw(gpu_cx_reg + A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_1,
			A6XX_GPUHTW_LLC_SCID_MASK,
			gpuhtw_scid << A6XX_GPUHTW_LLC_SCID_SHIFT);
	iounmap(gpu_cx_reg);
}

/*
 * a6xx_llc_enable_overrides() - Override the page attributes
 * @adreno_dev: The adreno device pointer
 */
static void a6xx_llc_enable_overrides(struct adreno_device *adreno_dev)
{
	void __iomem *gpu_cx_reg;

	/*
	 * 0x3: readnoallocoverrideen=0
	 *      read-no-alloc=0 - Allocate lines on read miss
	 *      writenoallocoverrideen=1
	 *      write-no-alloc=1 - Do not allocates lines on write miss
	 */
	gpu_cx_reg = ioremap(A6XX_GPU_CX_REG_BASE, A6XX_GPU_CX_REG_SIZE);
	__raw_writel(0x3, gpu_cx_reg + A6XX_GPU_CX_MISC_SYSTEM_CACHE_CNTL_0);
	/* Make sure the above write posts before we proceed*/
	wmb();
	iounmap(gpu_cx_reg);
}

#define A6XX_INT_MASK \
	((1 << A6XX_INT_CP_AHB_ERROR) |		\
	 (1 << A6XX_INT_ATB_ASYNCFIFO_OVERFLOW) |	\
	 (1 << A6XX_INT_RBBM_GPC_ERROR) |	\
	 (1 << A6XX_INT_CP_SW) |		\
	 (1 << A6XX_INT_CP_HW_ERROR) |		\
	 (1 << A6XX_INT_CP_IB2) |		\
	 (1 << A6XX_INT_CP_IB1) |		\
	 (1 << A6XX_INT_CP_RB) |		\
	 (1 << A6XX_INT_CP_CACHE_FLUSH_TS) |	\
	 (1 << A6XX_INT_RBBM_ATB_BUS_OVERFLOW) |	\
	 (1 << A6XX_INT_RBBM_HANG_DETECT) |	\
	 (1 << A6XX_INT_UCHE_OOB_ACCESS) |	\
	 (1 << A6XX_INT_UCHE_TRAP_INTR))

static struct adreno_irq_funcs a6xx_irq_funcs[32] = {
	ADRENO_IRQ_CALLBACK(NULL),              /* 0 - RBBM_GPU_IDLE */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 1 - RBBM_AHB_ERROR */
	ADRENO_IRQ_CALLBACK(NULL), /* 2 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 3 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 4 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 5 - UNUSED */
	/* 6 - RBBM_ATB_ASYNC_OVERFLOW */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback),
	ADRENO_IRQ_CALLBACK(NULL), /* 7 - GPC_ERR */
	ADRENO_IRQ_CALLBACK(NULL),/* 8 - CP_SW */
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
	ADRENO_IRQ_CALLBACK(adreno_cp_callback), /* 20 - CP_CACHE_FLUSH_TS */
	ADRENO_IRQ_CALLBACK(NULL), /* 21 - UNUSED */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 22 - RBBM_ATB_BUS_OVERFLOW */
	/* 23 - MISC_HANG_DETECT */
	ADRENO_IRQ_CALLBACK(adreno_hang_int_callback),
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 24 - UCHE_OOB_ACCESS */
	ADRENO_IRQ_CALLBACK(a6xx_err_callback), /* 25 - UCHE_TRAP_INTR */
	ADRENO_IRQ_CALLBACK(NULL), /* 26 - DEBBUS_INTR_0 */
	ADRENO_IRQ_CALLBACK(NULL), /* 27 - DEBBUS_INTR_1 */
	ADRENO_IRQ_CALLBACK(NULL), /* 28 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 29 - UNUSED */
	ADRENO_IRQ_CALLBACK(NULL), /* 30 - ISDB_CPU_IRQ */
	ADRENO_IRQ_CALLBACK(NULL), /* 31 - ISDB_UNDER_DEBUG */
};

static struct adreno_irq a6xx_irq = {
	.funcs = a6xx_irq_funcs,
	.mask = A6XX_INT_MASK,
};

/* Register offset defines for A6XX, in order of enum adreno_regs */
static unsigned int a6xx_register_offsets[ADRENO_REG_REGISTER_MAX] = {

	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_BASE, A6XX_CP_RB_BASE),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR_LO,
				A6XX_CP_RB_RPTR_ADDR_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR_ADDR_HI,
				A6XX_CP_RB_RPTR_ADDR_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_RPTR, A6XX_CP_RB_RPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_WPTR, A6XX_CP_RB_WPTR),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_RB_CNTL, A6XX_CP_RB_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_CP_CNTL, A6XX_CP_MISC_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS, A6XX_RBBM_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_STATUS3, A6XX_RBBM_STATUS3),

	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_MASK, A6XX_RBBM_INT_0_MASK),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_0_STATUS, A6XX_RBBM_INT_0_STATUS),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_CLOCK_CTL, A6XX_RBBM_CLOCK_CNTL),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_INT_CLEAR_CMD,
				A6XX_RBBM_INT_CLEAR_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_SW_RESET_CMD, A6XX_RBBM_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD,
					  A6XX_RBBM_BLOCK_SW_RESET_CMD),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_BLOCK_SW_RESET_CMD2,
					  A6XX_RBBM_BLOCK_SW_RESET_CMD2),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_ALWAYSON_COUNTER_LO,
				A6XX_CP_ALWAYS_ON_COUNTER_LO),
	ADRENO_REG_DEFINE(ADRENO_REG_RBBM_ALWAYSON_COUNTER_HI,
				A6XX_CP_ALWAYS_ON_COUNTER_HI),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_VERSION, A6XX_VBIF_VERSION),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_XIN_HALT_CTRL0,
				A6XX_VBIF_XIN_HALT_CTRL0),
	ADRENO_REG_DEFINE(ADRENO_REG_VBIF_XIN_HALT_CTRL1,
				A6XX_VBIF_XIN_HALT_CTRL1),

};

static const struct adreno_reg_offsets a6xx_reg_offsets = {
	.offsets = a6xx_register_offsets,
	.offset_0 = ADRENO_REG_REGISTER_MAX,
};

struct adreno_gpudev adreno_a6xx_gpudev = {
	.reg_offsets = &a6xx_reg_offsets,
	.start = a6xx_start,
	.irq = &a6xx_irq,
	.irq_trace = trace_kgsl_a5xx_irq_status,
	.num_prio_levels = KGSL_PRIORITY_MAX_RB_LEVELS,
	.platform_setup = a6xx_platform_setup,
	.rb_start = a6xx_rb_start,
	.regulator_enable = a6xx_sptprac_enable,
	.regulator_disable = a6xx_sptprac_disable,
	.microcode_read = a6xx_microcode_read,
	.enable_64bit = a6xx_enable_64bit,
	.llc_configure_gpu_scid = a6xx_llc_configure_gpu_scid,
	.llc_configure_gpuhtw_scid = a6xx_llc_configure_gpuhtw_scid,
	.llc_enable_overrides = a6xx_llc_enable_overrides
};
