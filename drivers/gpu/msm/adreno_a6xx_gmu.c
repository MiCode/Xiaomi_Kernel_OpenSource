// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <dt-bindings/regulator/qcom,rpmh-regulator-levels.h>
#include <linux/clk.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/interconnect.h>
#include <linux/io.h>
#include <linux/iommu.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <soc/qcom/cmd-db.h>
#include <soc/qcom/tcs.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_a6xx_gmu.h"
#include "adreno_snapshot.h"
#include "kgsl_bus.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

struct gmu_iommu_context {
	const char *name;
	struct platform_device *pdev;
	struct iommu_domain *domain;
};

struct gmu_vma_entry {
	unsigned int start;
	unsigned int size;
};

static const struct gmu_vma_entry a6xx_gmu_vma_legacy[] = {
	[GMU_ITCM] = { .start = 0x00000, .size = SZ_16K },
	[GMU_ICACHE] = { .start = 0x04000, .size = (SZ_256K - SZ_16K) },
	[GMU_DTCM] = { .start = 0x40000, .size = SZ_16K },
	[GMU_DCACHE] = { .start = 0x44000, .size = (SZ_256K - SZ_16K) },
	[GMU_NONCACHED_KERNEL] = { .start = 0x60000000, .size = SZ_512M },
	[GMU_NONCACHED_USER] = { .start = 0x80000000, .size = SZ_1G },
	[GMU_MEM_TYPE_MAX] = { .start = 0x0, .size = 0x0 },
};

static const struct gmu_vma_entry a6xx_gmu_vma[] = {
	[GMU_ITCM] = { .start = 0x00000000, .size = SZ_16K },
	[GMU_CACHE] = { .start = SZ_16K, .size = (SZ_16M - SZ_16K) },
	[GMU_DTCM] = { .start = SZ_256M + SZ_16K, .size = SZ_16K },
	[GMU_DCACHE] = { .start = 0x0, .size = 0x0 },
	[GMU_NONCACHED_KERNEL] = { .start = 0x60000000, .size = SZ_512M },
	[GMU_NONCACHED_USER] = { .start = 0x80000000, .size = SZ_1G },
	[GMU_MEM_TYPE_MAX] = { .start = 0x0, .size = 0x0 },
};

static struct gmu_iommu_context a6xx_gmu_ctx[] = {
	[GMU_CONTEXT_USER] = { .name = "gmu_user" },
	[GMU_CONTEXT_KERNEL] = { .name = "gmu_kernel" }
};

static const unsigned int a6xx_gmu_gx_registers[] = {
	/* GMU GX */
	0x1A800, 0x1A800, 0x1A810, 0x1A813, 0x1A816, 0x1A816, 0x1A818, 0x1A81B,
	0x1A81E, 0x1A81E, 0x1A820, 0x1A823, 0x1A826, 0x1A826, 0x1A828, 0x1A82B,
	0x1A82E, 0x1A82E, 0x1A830, 0x1A833, 0x1A836, 0x1A836, 0x1A838, 0x1A83B,
	0x1A83E, 0x1A83E, 0x1A840, 0x1A843, 0x1A846, 0x1A846, 0x1A880, 0x1A884,
	0x1A900, 0x1A92B, 0x1A940, 0x1A940,
};

static const unsigned int a6xx_gmu_tcm_registers[] = {
	/* ITCM */
	0x1B400, 0x1C3FF,
	/* DTCM */
	0x1C400, 0x1D3FF,
};

static const unsigned int a6xx_gmu_registers[] = {
	/* GMU CX */
	0x1F400, 0x1F407, 0x1F410, 0x1F412, 0x1F500, 0x1F500, 0x1F507, 0x1F50A,
	0x1F800, 0x1F804, 0x1F807, 0x1F808, 0x1F80B, 0x1F80C, 0x1F80F, 0x1F81C,
	0x1F824, 0x1F82A, 0x1F82D, 0x1F830, 0x1F840, 0x1F853, 0x1F887, 0x1F889,
	0x1F8A0, 0x1F8A2, 0x1F8A4, 0x1F8AF, 0x1F8C0, 0x1F8C3, 0x1F8D0, 0x1F8D0,
	0x1F8E4, 0x1F8E4, 0x1F8E8, 0x1F8EC, 0x1F900, 0x1F903, 0x1F940, 0x1F940,
	0x1F942, 0x1F944, 0x1F94C, 0x1F94D, 0x1F94F, 0x1F951, 0x1F954, 0x1F954,
	0x1F957, 0x1F958, 0x1F95D, 0x1F95D, 0x1F962, 0x1F962, 0x1F964, 0x1F965,
	0x1F980, 0x1F986, 0x1F990, 0x1F99E, 0x1F9C0, 0x1F9C0, 0x1F9C5, 0x1F9CC,
	0x1F9E0, 0x1F9E2, 0x1F9F0, 0x1F9F0, 0x1FA00, 0x1FA01,
	/* GMU AO */
	0x23B00, 0x23B16,
	/* GPU CC */
	0x24000, 0x24012, 0x24040, 0x24052, 0x24400, 0x24404, 0x24407, 0x2440B,
	0x24415, 0x2441C, 0x2441E, 0x2442D, 0x2443C, 0x2443D, 0x2443F, 0x24440,
	0x24442, 0x24449, 0x24458, 0x2445A, 0x24540, 0x2455E, 0x24800, 0x24802,
	0x24C00, 0x24C02, 0x25400, 0x25402, 0x25800, 0x25802, 0x25C00, 0x25C02,
	0x26000, 0x26002,
	/* GPU CC ACD */
	0x26400, 0x26416, 0x26420, 0x26427,
};

static const unsigned int a660_gmu_registers[] = {
	/* GMU CX */
	0x1F408, 0x1F40D, 0x1F40F, 0x1F40F, 0x1F50B, 0x1F50B, 0x1F860, 0x1F860,
	0x1F870, 0x1F877, 0x1F8C4, 0x1F8C4, 0x1F8F0, 0x1F8F1, 0x1F948, 0x1F94A,
	0x1F966, 0x1F96B, 0x1F970, 0x1F970, 0x1F972, 0x1F979, 0x1F9CD, 0x1F9D4,
	0x1FA02, 0x1FA03, 0x20000, 0x20001, 0x20004, 0x20004, 0x20008, 0x20012,
	0x20018, 0x20018,
};

#define RSC_CMD_OFFSET 2
#define PDC_CMD_OFFSET 4

static void _regwrite(void __iomem *regbase,
		unsigned int offsetwords, unsigned int value)
{
	void __iomem *reg;

	reg = regbase + (offsetwords << 2);
	__raw_writel(value, reg);
}

static void a6xx_load_rsc_ucode(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	void __iomem *rscc;

	if (adreno_is_a650_family(adreno_dev))
		rscc = adreno_dev->rscc_virt;
	else
		rscc = device->gmu_core.reg_virt + 0x23000;

	/* Disable SDE clock gating */
	_regwrite(rscc, A6XX_GPU_RSCC_RSC_STATUS0_DRV0, BIT(24));

	/* Setup RSC PDC handshake for sleep and wakeup */
	_regwrite(rscc, A6XX_RSCC_PDC_SLAVE_ID_DRV0, 1);
	_regwrite(rscc, A6XX_RSCC_HIDDEN_TCS_CMD0_DATA, 0);
	_regwrite(rscc, A6XX_RSCC_HIDDEN_TCS_CMD0_ADDR, 0);
	_regwrite(rscc, A6XX_RSCC_HIDDEN_TCS_CMD0_DATA + RSC_CMD_OFFSET, 0);
	_regwrite(rscc, A6XX_RSCC_HIDDEN_TCS_CMD0_ADDR + RSC_CMD_OFFSET, 0);
	_regwrite(rscc, A6XX_RSCC_HIDDEN_TCS_CMD0_DATA + RSC_CMD_OFFSET * 2,
			0x80000000);
	_regwrite(rscc, A6XX_RSCC_HIDDEN_TCS_CMD0_ADDR + RSC_CMD_OFFSET * 2,
			0);
	_regwrite(rscc, A6XX_RSCC_OVERRIDE_START_ADDR, 0);
	_regwrite(rscc, A6XX_RSCC_PDC_SEQ_START_ADDR, 0x4520);
	_regwrite(rscc, A6XX_RSCC_PDC_MATCH_VALUE_LO, 0x4510);
	_regwrite(rscc, A6XX_RSCC_PDC_MATCH_VALUE_HI, 0x4514);

	/* Load RSC sequencer uCode for sleep and wakeup */
	if (adreno_is_a650_family(adreno_dev)) {
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0, 0xEAAAE5A0);
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0 + 1, 0xE1A1EBAB);
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0 + 2, 0xA2E0A581);
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0 + 3, 0xECAC82E2);
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0 + 4, 0x0020EDAD);
	} else {
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0, 0xA7A506A0);
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0 + 1, 0xA1E6A6E7);
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0 + 2, 0xA2E081E1);
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0 + 3, 0xE9A982E2);
		_regwrite(rscc, A6XX_RSCC_SEQ_MEM_0_DRV0 + 4, 0x0020E8A8);
	}
}

static int a6xx_load_pdc_ucode(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct resource *res_pdc, *res_cfg, *res_seq;
	unsigned int cfg_offset, seq_offset;
	void __iomem *cfg = NULL, *seq = NULL;
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	u32 vrm_resource_addr = cmd_db_read_addr("vrm.soc");
	u32 xo_resource_addr = cmd_db_read_addr("xo.lvl");
	u32 cx_res_addr = cmd_db_read_addr("cx.lvl");
	u32 mx_res_addr = cmd_db_read_addr("mx.lvl");

	if (!xo_resource_addr) {
		dev_err(&gmu->pdev->dev,
				"Failed to get 'xo.lvl' addr from cmd_db\n");
		return -ENOENT;
	}

	if (!cx_res_addr) {
		dev_err(&gmu->pdev->dev,
				"Failed to get 'cx.lvl' addr from cmd_db\n");
		return -ENOENT;
	}

	if (!mx_res_addr) {
		dev_err(&gmu->pdev->dev,
				"Failed to get 'mx.lvl' addr from cmd_db\n");
		return -ENOENT;
	}
	/*
	 * Older A6x platforms specified PDC registers in the DT using a
	 * single base pointer that encompassed the entire PDC range. Current
	 * targets specify the individual GPU-owned PDC register blocks
	 * (sequence and config).
	 *
	 * This code handles both possibilities and generates individual
	 * pointers to the GPU PDC blocks, either as offsets from the single
	 * base, or as directly specified ranges.
	 *
	 * PDC programming has moved to AOP for newer A6x platforms.
	 * However registers to enable GPU PDC and set the sequence start
	 * address still need to be programmed.
	 */

	/* Offsets from the base PDC (if no PDC subsections in the DTSI) */
	if (adreno_is_a640v2(adreno_dev)) {
		cfg_offset = 0x90000;
		seq_offset = 0x290000;
	} else {
		cfg_offset = 0x80000;
		seq_offset = 0x280000;
	}

	/*
	 * Map the starting address for pdc_cfg programming. If the pdc_cfg
	 * resource is not available use an offset from the base PDC resource.
	 */
	res_pdc = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM,
			"kgsl_gmu_pdc_reg");
	res_cfg = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM,
			"kgsl_gmu_pdc_cfg");
	if (res_cfg)
		cfg = ioremap(res_cfg->start, resource_size(res_cfg));
	else if (res_pdc)
		cfg = ioremap(res_pdc->start + cfg_offset, 0x10000);

	if (!cfg) {
		dev_err(&gmu->pdev->dev, "Failed to map PDC CFG\n");
		return -ENODEV;
	}

	/* PDC is programmed in AOP for newer platforms */
	if (a6xx_core->pdc_in_aop)
		goto done;

	/*
	 * Map the starting address for pdc_seq programming. If the pdc_seq
	 * resource is not available use an offset from the base PDC resource.
	 */
	res_seq = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM,
			"kgsl_gmu_pdc_seq");
	if (res_seq)
		seq = ioremap(res_seq->start, resource_size(res_seq));
	else if (res_pdc)
		seq = ioremap(res_pdc->start + seq_offset, 0x10000);

	if (!seq) {
		dev_err(&gmu->pdev->dev, "Failed to map PDC SEQ\n");
		iounmap(cfg);
		return -ENODEV;
	}

	/* Load PDC sequencer uCode for power up and power down sequence */
	_regwrite(seq, PDC_GPU_SEQ_MEM_0, 0xFEBEA1E1);
	_regwrite(seq, PDC_GPU_SEQ_MEM_0 + 1, 0xA5A4A3A2);
	_regwrite(seq, PDC_GPU_SEQ_MEM_0 + 2, 0x8382A6E0);
	_regwrite(seq, PDC_GPU_SEQ_MEM_0 + 3, 0xBCE3E284);
	_regwrite(seq, PDC_GPU_SEQ_MEM_0 + 4, 0x002081FC);

	iounmap(seq);

	/* Set TCS commands used by PDC sequence for low power modes */
	_regwrite(cfg, PDC_GPU_TCS1_CMD_ENABLE_BANK, 7);
	_regwrite(cfg, PDC_GPU_TCS1_CMD_WAIT_FOR_CMPL_BANK, 0);
	_regwrite(cfg, PDC_GPU_TCS1_CONTROL, 0);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_MSGID, 0x10108);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_ADDR, mx_res_addr);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_DATA, 1);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_MSGID + PDC_CMD_OFFSET, 0x10108);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_ADDR + PDC_CMD_OFFSET, cx_res_addr);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_DATA + PDC_CMD_OFFSET, 0x0);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_MSGID + PDC_CMD_OFFSET * 2, 0x10108);

	_regwrite(cfg, PDC_GPU_TCS1_CMD0_ADDR + PDC_CMD_OFFSET * 2,
			xo_resource_addr);

	_regwrite(cfg, PDC_GPU_TCS1_CMD0_DATA + PDC_CMD_OFFSET * 2, 0x0);

	if (vrm_resource_addr && adreno_is_a620(adreno_dev)) {
		_regwrite(cfg, PDC_GPU_TCS1_CMD0_MSGID + PDC_CMD_OFFSET * 3,
				0x10108);
		_regwrite(cfg, PDC_GPU_TCS1_CMD0_ADDR + PDC_CMD_OFFSET * 3,
				vrm_resource_addr + 0x4);
		_regwrite(cfg, PDC_GPU_TCS1_CMD0_DATA + PDC_CMD_OFFSET * 3,
				0x0);
	}

	_regwrite(cfg, PDC_GPU_TCS3_CMD_ENABLE_BANK, 7);
	_regwrite(cfg, PDC_GPU_TCS3_CMD_WAIT_FOR_CMPL_BANK, 0);
	_regwrite(cfg, PDC_GPU_TCS3_CONTROL, 0);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_MSGID, 0x10108);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_ADDR, mx_res_addr);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_DATA, 2);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_MSGID + PDC_CMD_OFFSET, 0x10108);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_ADDR + PDC_CMD_OFFSET, cx_res_addr);

	if (adreno_is_a618(adreno_dev) || adreno_is_a619(adreno_dev) ||
			adreno_is_a650_family(adreno_dev))
		_regwrite(cfg, PDC_GPU_TCS3_CMD0_DATA + PDC_CMD_OFFSET, 0x2);
	else
		_regwrite(cfg, PDC_GPU_TCS3_CMD0_DATA + PDC_CMD_OFFSET, 0x3);

	_regwrite(cfg, PDC_GPU_TCS3_CMD0_MSGID + PDC_CMD_OFFSET * 2, 0x10108);

	_regwrite(cfg, PDC_GPU_TCS3_CMD0_ADDR + PDC_CMD_OFFSET * 2,
			xo_resource_addr);

	_regwrite(cfg, PDC_GPU_TCS3_CMD0_DATA + PDC_CMD_OFFSET * 2, 0x3);

	if (vrm_resource_addr && adreno_is_a620(adreno_dev)) {
		_regwrite(cfg, PDC_GPU_TCS3_CMD0_MSGID + PDC_CMD_OFFSET * 3,
				0x10108);
		_regwrite(cfg, PDC_GPU_TCS3_CMD0_ADDR + PDC_CMD_OFFSET * 3,
				vrm_resource_addr + 0x4);
		_regwrite(cfg, PDC_GPU_TCS3_CMD0_DATA + PDC_CMD_OFFSET * 3,
				0x1);
	}

done:
	/* Setup GPU PDC */
	_regwrite(cfg, PDC_GPU_SEQ_START_ADDR, 0);
	_regwrite(cfg, PDC_GPU_ENABLE_PDC, 0x80000001);

	/* ensure no writes happen before the uCode is fully written */
	wmb();
	iounmap(cfg);
	return 0;
}

static int _load_gmu_rpmh_ucode(struct kgsl_device *device)
{
	a6xx_load_rsc_ucode(device);
	return a6xx_load_pdc_ucode(device);
}

/* GMU timeouts */
#define GMU_IDLE_TIMEOUT	100	/* ms */
#define GMU_START_TIMEOUT	100	/* ms */
#define GPU_START_TIMEOUT	100	/* ms */
#define GPU_RESET_TIMEOUT	1	/* ms */
#define GPU_RESET_TIMEOUT_US	10	/* us */

/*
 * The lowest 16 bits of this value are the number of XO clock cycles
 * for main hysteresis. This is the first hysteresis. Here we set it
 * to 0x1680 cycles, or 300 us. The highest 16 bits of this value are
 * the number of XO clock cycles for short hysteresis. This happens
 * after main hysteresis. Here we set it to 0xA cycles, or 0.5 us.
 */
#define GMU_PWR_COL_HYST 0x000A1680

/*
 * a6xx_gmu_power_config() - Configure and enable GMU's low power mode
 * setting based on ADRENO feature flags.
 * @device: Pointer to KGSL device
 */
static void a6xx_gmu_power_config(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	/* Configure registers for idle setting. The setting is cumulative */

	/* Disable GMU WB/RB buffer and caches at boot */
	gmu_core_regwrite(device, A6XX_GMU_SYS_BUS_CONFIG, 0x1);
	gmu_core_regwrite(device, A6XX_GMU_ICACHE_CONFIG, 0x1);
	gmu_core_regwrite(device, A6XX_GMU_DCACHE_CONFIG, 0x1);

	gmu_core_regwrite(device,
		A6XX_GMU_PWR_COL_INTER_FRAME_CTRL,  0x9C40400);

	switch (gmu->idle_level) {
	case GPU_HW_MIN_VOLT:
		gmu_core_regrmw(device, A6XX_GMU_RPMH_CTRL, 0,
				MIN_BW_ENABLE_MASK);
		gmu_core_regrmw(device, A6XX_GMU_RPMH_HYST_CTRL, 0,
				MIN_BW_HYST);
		/* fall through */
	case GPU_HW_NAP:
		gmu_core_regrmw(device, A6XX_GMU_GPU_NAP_CTRL, 0,
				HW_NAP_ENABLE_MASK);
		/* fall through */
	case GPU_HW_IFPC:
		gmu_core_regwrite(device, A6XX_GMU_PWR_COL_INTER_FRAME_HYST,
				GMU_PWR_COL_HYST);
		gmu_core_regrmw(device, A6XX_GMU_PWR_COL_INTER_FRAME_CTRL, 0,
				IFPC_ENABLE_MASK);
		/* fall through */
	case GPU_HW_SPTP_PC:
		gmu_core_regwrite(device, A6XX_GMU_PWR_COL_SPTPRAC_HYST,
				GMU_PWR_COL_HYST);
		gmu_core_regrmw(device, A6XX_GMU_PWR_COL_INTER_FRAME_CTRL, 0,
				SPTP_ENABLE_MASK);
		/* fall through */
	default:
		break;
	}

	/* Enable RPMh GPU client */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_RPMH))
		gmu_core_regrmw(device, A6XX_GMU_RPMH_CTRL, 0,
				RPMH_ENABLE_MASK);
}

static int a6xx_gmu_device_start(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	u32 val = 0x00000100;
	u32 mask = 0x000001FF;

	/* Check for 0xBABEFACE on legacy targets */
	if (gmu->ver.core <= 0x20010004) {
		val = 0xBABEFACE;
		mask = 0xFFFFFFFF;
	}

	/* Bring GMU out of reset */
	gmu_core_regwrite(device, A6XX_GMU_CM3_SYSRESET, 0);

	/* Make sure the write is posted before moving ahead */
	wmb();

	if (timed_poll_check(device,
			A6XX_GMU_CM3_FW_INIT_RESULT,
			val, GMU_START_TIMEOUT, mask)) {
		u32 val;

		/*
		 * The breadcrumb is written to a gmu virtual mapping
		 * which points to dtcm byte offset 0x3fdc.
		 */
		gmu_core_regread(device,
			A6XX_GMU_CM3_DTCM_START + (0x3fdc >> 2), &val);
		dev_err(&gmu->pdev->dev, "GMU doesn't boot: 0x%x\n", val);

		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * a6xx_gmu_hfi_start() - Write registers and start HFI.
 * @device: Pointer to KGSL device
 */
static int a6xx_gmu_hfi_start(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	gmu_core_regwrite(device, A6XX_GMU_HFI_CTRL_INIT, 1);

	if (timed_poll_check(device,
			A6XX_GMU_HFI_CTRL_STATUS,
			BIT(0),
			GMU_START_TIMEOUT,
			BIT(0))) {
		dev_err(&gmu->pdev->dev, "GMU HFI init failed\n");
		return -ETIMEDOUT;
	}

	return 0;
}

static int a6xx_rpmh_power_on_gpu(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct device *dev = &gmu->pdev->dev;
	int val;

	/* Only trigger wakeup sequence if sleep sequence was done earlier */
	if (!test_bit(GMU_RSCC_SLEEP_SEQ_DONE, &device->gmu_core.flags))
		return 0;

	 /* A660 has a replacement register */
	if (adreno_is_a660(ADRENO_DEVICE(device)))
		gmu_core_regread(device, A6XX_GPU_CC_GX_DOMAIN_MISC3, &val);
	else
		gmu_core_regread(device, A6XX_GPU_CC_GX_DOMAIN_MISC, &val);

	if (!(val & 0x1))
		dev_err_ratelimited(&gmu->pdev->dev,
			"GMEM CLAMP IO not set while GFX rail off\n");

	/* RSC wake sequence */
	gmu_core_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, BIT(1));

	/* Write request before polling */
	wmb();

	if (timed_poll_check(device,
			A6XX_GMU_RSCC_CONTROL_ACK,
			BIT(1),
			GPU_START_TIMEOUT,
			BIT(1))) {
		dev_err(dev, "Failed to do GPU RSC power on\n");
		return -EINVAL;
	}

	if (timed_poll_check_rscc(device,
			A6XX_RSCC_SEQ_BUSY_DRV0,
			0,
			GPU_START_TIMEOUT,
			0xFFFFFFFF))
		goto error_rsc;

	gmu_core_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, 0);

	/* Clear sleep sequence flag as wakeup sequence is successful */
	clear_bit(GMU_RSCC_SLEEP_SEQ_DONE, &device->gmu_core.flags);

	return 0;
error_rsc:
	dev_err(dev, "GPU RSC sequence stuck in waking up GPU\n");
	return -EINVAL;
}

static int a6xx_rpmh_power_off_gpu(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret;

	if (test_bit(GMU_RSCC_SLEEP_SEQ_DONE, &device->gmu_core.flags))
		return 0;

	gmu_core_regwrite(device, A6XX_GMU_CM3_SYSRESET, 1);
	/* Make sure M3 is in reset before going on */
	wmb();

	gmu_core_regread(device, A6XX_GPU_GMU_CX_GMU_PWR_COL_CP_RESP,
			&gmu->log_wptr_retention);

	gmu_core_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, 1);
	/* Make sure the request completes before continuing */
	wmb();

	ret = timed_poll_check_rscc(device,
			A6XX_GPU_RSCC_RSC_STATUS0_DRV0,
			BIT(16),
			GPU_START_TIMEOUT,
			BIT(16));

	if (ret) {
		dev_err(&gmu->pdev->dev, "GPU RSC power off fail\n");
		return -ETIMEDOUT;
	}

	gmu_core_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, 0);

	if (adreno_dev->lm_enabled)
		gmu_core_regwrite(device, A6XX_GMU_AO_SPARE_CNTL, 0);

	set_bit(GMU_RSCC_SLEEP_SEQ_DONE, &device->gmu_core.flags);
	return 0;
}

static int _load_legacy_gmu_fw(struct kgsl_device *device,
	struct a6xx_gmu_device *gmu)
{
	const struct firmware *fw = gmu->fw_image;

	if (fw->size > MAX_GMUFW_SIZE)
		return -EINVAL;

	gmu_core_blkwrite(device, A6XX_GMU_CM3_ITCM_START, fw->data,
			fw->size);

	/* Proceed only after the FW is written */
	wmb();
	return 0;
}

static struct gmu_memdesc *a6xx_gmu_get_memdesc(struct a6xx_gmu_device *gmu,
		unsigned int addr, unsigned int size)
{
	int i;
	struct gmu_memdesc *mem;

	for (i = 0; i < GMU_KERNEL_ENTRIES; i++) {
		if (!test_bit(i, &gmu->kmem_bitmap))
			continue;

		mem = &gmu->kmem_entries[i];

		if (addr >= mem->gmuaddr &&
				(addr + size <= mem->gmuaddr + mem->size))
			return mem;
	}

	return NULL;
}

static int load_gmu_fw(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	uint8_t *fw = (uint8_t *)gmu->fw_image->data;
	int tcm_addr;
	struct gmu_block_header *blk;
	struct gmu_memdesc *md;

	if (adreno_is_a630(ADRENO_DEVICE(device)) ||
		adreno_is_a615_family(ADRENO_DEVICE(device)))
		return _load_legacy_gmu_fw(device, gmu);

	while (fw < (uint8_t *)gmu->fw_image->data + gmu->fw_image->size) {
		blk = (struct gmu_block_header *)fw;
		fw += sizeof(*blk);

		/* Don't deal with zero size blocks */
		if (blk->size == 0)
			continue;

		md = a6xx_gmu_get_memdesc(gmu, blk->addr, blk->size);
		if (md == NULL) {
			dev_err(&gmu->pdev->dev,
					"No backing memory for 0x%8.8X\n",
					blk->addr);
			return -EINVAL;
		}

		if (md->mem_type == GMU_ITCM || md->mem_type == GMU_DTCM) {
			tcm_addr = (blk->addr - (uint32_t)md->gmuaddr) /
				sizeof(uint32_t);

			if (md->mem_type == GMU_ITCM)
				tcm_addr += A6XX_GMU_CM3_ITCM_START;
			else
				tcm_addr += A6XX_GMU_CM3_DTCM_START;

			gmu_core_blkwrite(device, tcm_addr, fw, blk->size);
		} else {
			uint32_t offset = blk->addr - (uint32_t)md->gmuaddr;

			/* Copy the memory directly */
			memcpy(md->hostptr + offset, fw, blk->size);
		}

		fw += blk->size;
	}

	/* Proceed only after the FW is written */
	wmb();
	return 0;
}

static const char *oob_to_str(enum oob_request req)
{
	if (req == oob_gpu)
		return "oob_gpu";
	else if (req == oob_perfcntr)
		return "oob_perfcntr";
	else if (req == oob_boot_slumber)
		return "oob_boot_slumber";
	else if (req == oob_dcvs)
		return "oob_dcvs";
	return "unknown";
}

/*
 * a6xx_gmu_oob_set() - Set OOB interrupt to GMU.
 * @device: Pointer to kgsl device
 * @req: Which of the OOB bits to request
 */
static int a6xx_gmu_oob_set(struct kgsl_device *device,
		enum oob_request req)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int ret = 0;
	int set, check;

	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) {
		set = BIT(req + 16);
		check = BIT(req + 24);
	} else {
		/*
		 * The legacy targets have special bits that aren't supported on
		 * newer implementations
		 */
		if (req >= oob_boot_slumber) {
			dev_err(&gmu->pdev->dev,
				"Unsupported OOB request %s\n",
				oob_to_str(req));
			return -EINVAL;
		}

		set = BIT(30 - req * 2);
		check = BIT(31 - req);
	}

	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, set);

	if (timed_poll_check(device, A6XX_GMU_GMU2HOST_INTR_INFO, check,
		GPU_START_TIMEOUT, check)) {
		ret = -ETIMEDOUT;
		WARN(1, "OOB request %s timed out\n", oob_to_str(req));
	}

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, check);

	trace_kgsl_gmu_oob_set(set);
	return ret;
}

/*
 * a6xx_gmu_oob_clear() - Clear a previously set  OOB request.
 * @device: Pointer to the kgsl device that has the GMU
 * @req: Which of the OOB bits to clear
 */
static void a6xx_gmu_oob_clear(struct kgsl_device *device,
		enum oob_request req)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int clear;

	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) {
		clear = BIT(req + 24);
	} else {
		clear = BIT(31 - req * 2);
		if (req >= oob_boot_slumber) {
			dev_err(&gmu->pdev->dev, "Unsupported OOB clear %s\n",
				oob_to_str(req));
			return;
		}
	}

	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, clear);
	trace_kgsl_gmu_oob_clear(clear);
}

static void a6xx_gmu_irq_enable(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct a6xx_hfi *hfi = &gmu->hfi;

	/* Clear pending IRQs and Unmask needed IRQs */
	adreno_gmu_clear_and_unmask_irqs(ADRENO_DEVICE(device));

	/* Enable all IRQs on host */
	enable_irq(hfi->hfi_interrupt_num);
	enable_irq(gmu->gmu_interrupt_num);
}

static void a6xx_gmu_irq_disable(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct a6xx_hfi *hfi = &gmu->hfi;

	/* Disable all IRQs on host */
	disable_irq(gmu->gmu_interrupt_num);
	disable_irq(hfi->hfi_interrupt_num);

	/* Mask all IRQs and clear pending IRQs */
	adreno_gmu_mask_and_clear_irqs(ADRENO_DEVICE(device));
}

static int a6xx_gmu_hfi_start_msg(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct hfi_start_cmd req;

	/*
	 * This HFI was not supported in legacy firmware and this quirk
	 * serves as a better means to identify targets that depend on
	 * legacy firmware.
	 */
	if (!ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		return a6xx_hfi_send_req(A6XX_GMU_DEVICE(device),
					 H2F_MSG_START, &req);

	return 0;

}

#define FREQ_VOTE(idx, ack) (((idx) & 0xFF) | (((ack) & 0xF) << 28))
#define BW_VOTE(idx) ((((idx) & 0xFFF) << 12) | ((idx) & 0xFFF))

/*
 * a6xx_gmu_dcvs_nohfi() - request GMU to do DCVS without using HFI
 * @device: Pointer to KGSL device
 * @perf_idx: Index into GPU performance level table defined in
 *	HFI DCVS table message
 * @bw_idx: Index into GPU b/w table defined in HFI b/w table message
 *
 */
static int a6xx_gmu_dcvs_nohfi(struct kgsl_device *device,
		unsigned int perf_idx, unsigned int bw_idx)
{
	int ret;

	gmu_core_regwrite(device, A6XX_GMU_DCVS_ACK_OPTION, DCVS_ACK_NONBLOCK);

	gmu_core_regwrite(device, A6XX_GMU_DCVS_PERF_SETTING,
			FREQ_VOTE(perf_idx, CLKSET_OPTION_ATLEAST));

	gmu_core_regwrite(device, A6XX_GMU_DCVS_BW_SETTING, BW_VOTE(bw_idx));

	ret = a6xx_gmu_oob_set(device, oob_dcvs);
	if (ret == 0)
		gmu_core_regread(device, A6XX_GMU_DCVS_RETURN, &ret);

	a6xx_gmu_oob_clear(device, oob_dcvs);

	return ret;
}
static int a6xx_complete_rpmh_votes(struct kgsl_device *device)
{
	int ret = 0;

	ret |= timed_poll_check_rscc(device, A6XX_RSCC_TCS0_DRV0_STATUS,
			BIT(0), GPU_RESET_TIMEOUT, BIT(0));
	ret |= timed_poll_check_rscc(device, A6XX_RSCC_TCS1_DRV0_STATUS,
			BIT(0), GPU_RESET_TIMEOUT, BIT(0));
	ret |= timed_poll_check_rscc(device, A6XX_RSCC_TCS2_DRV0_STATUS,
			BIT(0), GPU_RESET_TIMEOUT, BIT(0));
	ret |= timed_poll_check_rscc(device, A6XX_RSCC_TCS3_DRV0_STATUS,
			BIT(0), GPU_RESET_TIMEOUT, BIT(0));

	return ret;
}

#define SPTPRAC_POWERON_CTRL_MASK	0x00778000
#define SPTPRAC_POWEROFF_CTRL_MASK	0x00778001
#define SPTPRAC_POWEROFF_STATUS_MASK	BIT(2)
#define SPTPRAC_POWERON_STATUS_MASK	BIT(3)
#define SPTPRAC_CTRL_TIMEOUT		10 /* ms */
#define A6XX_RETAIN_FF_ENABLE_ENABLE_MASK BIT(11)

/*
 * a6xx_gmu_sptprac_enable() - Power on SPTPRAC
 * @adreno_dev: Pointer to Adreno device
 */
int a6xx_gmu_sptprac_enable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	if (!gmu_core_gpmu_isenabled(device) ||
			!adreno_has_sptprac_gdsc(adreno_dev))
		return 0;

	gmu_core_regwrite(device, A6XX_GMU_GX_SPTPRAC_POWER_CONTROL,
			SPTPRAC_POWERON_CTRL_MASK);

	if (timed_poll_check(device,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS,
			SPTPRAC_POWERON_STATUS_MASK,
			SPTPRAC_CTRL_TIMEOUT,
			SPTPRAC_POWERON_STATUS_MASK)) {
		dev_err(&gmu->pdev->dev, "power on SPTPRAC fail\n");
		return -EINVAL;
	}

	return 0;
}

/*
 * a6xx_gmu_sptprac_disable() - Power of SPTPRAC
 * @adreno_dev: Pointer to Adreno device
 */
void a6xx_gmu_sptprac_disable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	if (!gmu_core_gpmu_isenabled(device) ||
			!adreno_has_sptprac_gdsc(adreno_dev))
		return;

	/* Ensure that retention is on */
	gmu_core_regrmw(device, A6XX_GPU_CC_GX_GDSCR, 0,
			A6XX_RETAIN_FF_ENABLE_ENABLE_MASK);

	gmu_core_regwrite(device, A6XX_GMU_GX_SPTPRAC_POWER_CONTROL,
			SPTPRAC_POWEROFF_CTRL_MASK);

	if (timed_poll_check(device,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS,
			SPTPRAC_POWEROFF_STATUS_MASK,
			SPTPRAC_CTRL_TIMEOUT,
			SPTPRAC_POWEROFF_STATUS_MASK))
		dev_err(&gmu->pdev->dev, "power off SPTPRAC fail\n");
}

#define SPTPRAC_POWER_OFF	BIT(2)
#define SP_CLK_OFF		BIT(4)
#define GX_GDSC_POWER_OFF	BIT(6)
#define GX_CLK_OFF		BIT(7)
#define is_on(val)		(!(val & (GX_GDSC_POWER_OFF | GX_CLK_OFF)))
/*
 * a6xx_gmu_gx_is_on() - Check if GX is on using pwr status register
 * @device - Pointer to KGSLxi _device
 * This check should only be performed if the keepalive bit is set or it
 * can be guaranteed that the power state of the GPU will remain unchanged
 */
static bool a6xx_gmu_gx_is_on(struct kgsl_device *device)
{
	unsigned int val;

	gmu_core_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &val);
	return is_on(val);
}

/*
 * a6xx_gmu_sptprac_is_on() - Check if SPTP is on using pwr status register
 * @adreno_dev - Pointer to adreno_device
 * This check should only be performed if the keepalive bit is set or it
 * can be guaranteed that the power state of the GPU will remain unchanged
 */
bool a6xx_gmu_sptprac_is_on(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int val;

	if (!gmu_core_isenabled(device))
		return true;

	gmu_core_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &val);
	return !(val & (SPTPRAC_POWER_OFF | SP_CLK_OFF));
}

/*
 * a6xx_gmu_gfx_rail_on() - request GMU to power GPU at given OPP.
 * @device: Pointer to KGSL device
 *
 */
static int a6xx_gmu_gfx_rail_on(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	unsigned int perf_idx = gmu->num_gpupwrlevels -
		pwr->default_pwrlevel - 1;
	uint32_t default_opp = gmu->rpmh_votes.gx_votes[perf_idx];

	gmu_core_regwrite(device, A6XX_GMU_BOOT_SLUMBER_OPTION,
			OOB_BOOT_OPTION);
	gmu_core_regwrite(device, A6XX_GMU_GX_VOTE_IDX,
			ARC_VOTE_GET_PRI(default_opp));
	gmu_core_regwrite(device, A6XX_GMU_MX_VOTE_IDX,
			ARC_VOTE_GET_SEC(default_opp));

	return a6xx_gmu_oob_set(device, oob_boot_slumber);
}

static bool idle_trandition_complete(unsigned int idle_level,
	unsigned int gmu_power_reg,
	unsigned int sptprac_clk_reg)
{
	if (idle_level != gmu_power_reg)
		return false;

	switch (idle_level) {
	case GPU_HW_IFPC:
		if (is_on(sptprac_clk_reg))
			return false;
		break;
	/* other GMU idle levels can be added here */
	case GPU_HW_ACTIVE:
	default:
		break;
	}
	return true;
}

static const char *idle_level_name(int level)
{
	if (level == GPU_HW_ACTIVE)
		return "GPU_HW_ACTIVE";
	else if (level == GPU_HW_SPTP_PC)
		return "GPU_HW_SPTP_PC";
	else if (level == GPU_HW_IFPC)
		return "GPU_HW_IFPC";
	else if (level == GPU_HW_NAP)
		return "GPU_HW_NAP";
	else if (level == GPU_HW_MIN_VOLT)
		return "GPU_HW_MIN_VOLT";

	return "";
}

static int a6xx_gmu_wait_for_lowest_idle(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	unsigned int reg, reg1, reg2, reg3, reg4, reg5, reg6, reg7, reg8;
	unsigned long t;
	uint64_t ts1, ts2, ts3;

	ts1 = a6xx_read_alwayson(ADRENO_DEVICE(device));

	t = jiffies + msecs_to_jiffies(GMU_IDLE_TIMEOUT);
	do {
		gmu_core_regread(device,
			A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
		gmu_core_regread(device,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg1);

		if (idle_trandition_complete(gmu->idle_level, reg, reg1))
			return 0;
		/* Wait 100us to reduce unnecessary AHB bus traffic */
		usleep_range(10, 100);
	} while (!time_after(jiffies, t));

	ts2 = a6xx_read_alwayson(ADRENO_DEVICE(device));
	/* Check one last time */

	gmu_core_regread(device, A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
	gmu_core_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg1);

	if (idle_trandition_complete(gmu->idle_level, reg, reg1))
		return 0;

	ts3 = a6xx_read_alwayson(ADRENO_DEVICE(device));

	/* Collect abort data to help with debugging */
	gmu_core_regread(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &reg2);
	gmu_core_regread(device, A6XX_GMU_RBBM_INT_UNMASKED_STATUS, &reg3);
	gmu_core_regread(device, A6XX_GMU_GMU_PWR_COL_KEEPALIVE, &reg4);
	gmu_core_regread(device, A6XX_GMU_AO_SPARE_CNTL, &reg5);

	dev_err(&gmu->pdev->dev,
		"----------------------[ GMU error ]----------------------\n");
	dev_err(&gmu->pdev->dev,
		"Timeout waiting for lowest idle level %s\n",
		idle_level_name(gmu->idle_level));
	dev_err(&gmu->pdev->dev, "Start: %llx (absolute ticks)\n", ts1);
	dev_err(&gmu->pdev->dev, "Poll: %llx (ticks relative to start)\n",
		ts2-ts1);
	dev_err(&gmu->pdev->dev, "Retry: %llx (ticks relative to poll)\n",
		ts3-ts2);
	dev_err(&gmu->pdev->dev,
		"RPMH_POWER_STATE=%x SPTPRAC_PWR_CLK_STATUS=%x\n", reg, reg1);
	dev_err(&gmu->pdev->dev, "CX_BUSY_STATUS=%x\n", reg2);
	dev_err(&gmu->pdev->dev,
		"RBBM_INT_UNMASKED_STATUS=%x PWR_COL_KEEPALIVE=%x\n",
		reg3, reg4);
	dev_err(&gmu->pdev->dev, "A6XX_GMU_AO_SPARE_CNTL=%x\n", reg5);

	/* Access GX registers only when GX is ON */
	if (is_on(reg1)) {
		kgsl_regread(device, A6XX_CP_STATUS_1, &reg6);
		kgsl_regread(device, A6XX_CP_CP2GMU_STATUS, &reg7);
		kgsl_regread(device, A6XX_CP_CONTEXT_SWITCH_CNTL, &reg8);

		dev_err(&gmu->pdev->dev, "A6XX_CP_STATUS_1=%x\n", reg6);
		dev_err(&gmu->pdev->dev,
			"CP2GMU_STATUS=%x CONTEXT_SWITCH_CNTL=%x\n",
			reg7, reg8);
	}

	WARN_ON(1);
	return -ETIMEDOUT;
}

/* Bitmask for GPU idle status check */
#define CXGXCPUBUSYIGNAHB	BIT(30)
static int a6xx_gmu_wait_for_idle(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	unsigned int status2;
	uint64_t ts1;

	ts1 = a6xx_read_alwayson(ADRENO_DEVICE(device));
	if (timed_poll_check(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS,
			0, GMU_START_TIMEOUT, CXGXCPUBUSYIGNAHB)) {
		gmu_core_regread(device,
				A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS2, &status2);
		dev_err(&gmu->pdev->dev,
				"GMU not idling: status2=0x%x %llx %llx\n",
				status2, ts1,
				a6xx_read_alwayson(ADRENO_DEVICE(device)));
		return -ETIMEDOUT;
	}

	return 0;
}

/* A6xx GMU FENCE RANGE MASK */
#define GMU_FENCE_RANGE_MASK	((0x1 << 31) | ((0xA << 2) << 18) | (0x8A0))

static void load_gmu_version_info(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	/* GMU version info is at a fixed offset in the DTCM */
	gmu_core_regread(device, A6XX_GMU_CM3_DTCM_START + 0xFF8,
				&gmu->ver.core);
	gmu_core_regread(device, A6XX_GMU_CM3_DTCM_START + 0xFF9,
				&gmu->ver.core_dev);
	gmu_core_regread(device, A6XX_GMU_CM3_DTCM_START + 0xFFA,
				&gmu->ver.pwr);
	gmu_core_regread(device, A6XX_GMU_CM3_DTCM_START + 0xFFB,
				&gmu->ver.pwr_dev);
	gmu_core_regread(device, A6XX_GMU_CM3_DTCM_START + 0xFFC,
				&gmu->ver.hfi);
}

static void a6xx_gmu_mem_free(struct a6xx_gmu_device *gmu,
		struct gmu_memdesc *md)
{
	/* Free GMU image memory */
	if (md->hostptr)
		dma_free_attrs(&gmu->pdev->dev, (size_t) md->size,
				(void *)md->hostptr, md->physaddr, 0);
	memset(md, 0, sizeof(*md));
}

static void a6xx_gmu_enable_lm(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u32 val;

	memset(adreno_dev->busy_data.throttle_cycles, 0,
		sizeof(adreno_dev->busy_data.throttle_cycles));

	if (!adreno_dev->lm_enabled)
		return;

	/*
	 * For throttling, use the following counters for throttled cycles:
	 * XOCLK1: countable: 0x10
	 * XOCLK2: countable: 0x16 for newer hardware / 0x15 for others
	 * XOCLK3: countable: 0xf for newer hardware / 0x19 for others
	 *
	 * POWER_CONTROL_SELECT_0 controls counters 0 - 3, each selector
	 * is 8 bits wide.
	 */

	if (adreno_is_a620(adreno_dev) || adreno_is_a650(adreno_dev))
		val = (0x10 << 8) | (0x16 << 16) | (0x0f << 24);
	else
		val = (0x10 << 8) | (0x15 << 16) | (0x19 << 24);


	/* Make sure not to write over XOCLK0 */
	gmu_core_regrmw(device, A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0,
		0xffffff00, val);

	gmu_core_regwrite(device, A6XX_GMU_AO_SPARE_CNTL, 1);
}

static void a6xx_gmu_register_config(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	u32 gmu_log_info, chipid = 0;

	/* Vote veto for FAL10 feature if supported*/
	if (a6xx_core->veto_fal10)
		gmu_core_regwrite(device, A6XX_GPU_GMU_CX_GMU_CX_FAL_INTF, 0x1);

	/* Turn on TCM retention */
	gmu_core_regwrite(device, A6XX_GMU_GENERAL_7, 1);

	/* Clear init result to make sure we are getting fresh value */
	gmu_core_regwrite(device, A6XX_GMU_CM3_FW_INIT_RESULT, 0);
	gmu_core_regwrite(device, A6XX_GMU_CM3_BOOT_CONFIG, 0x2);

	gmu_core_regwrite(device, A6XX_GMU_HFI_QTBL_ADDR,
			gmu->hfi_mem->gmuaddr);
	gmu_core_regwrite(device, A6XX_GMU_HFI_QTBL_INFO, 1);

	gmu_core_regwrite(device, A6XX_GMU_AHB_FENCE_RANGE_0,
			GMU_FENCE_RANGE_MASK);

	/*
	 * Make sure that CM3 state is at reset value. Snapshot is changing
	 * NMI bit and if we boot up GMU with NMI bit set GMU will boot
	 * straight in to NMI handler without executing __main code
	 */
	gmu_core_regwrite(device, A6XX_GMU_CM3_CFG, 0x4052);

	/**
	 * We may have asserted gbif halt as part of reset sequence which may
	 * not get cleared if the gdsc was not reset. So clear it before
	 * attempting GMU boot.
	 */
	if (!adreno_is_a630(adreno_dev))
		kgsl_regwrite(device, A6XX_GBIF_HALT, 0x0);

	/* Set the log wptr index */
	gmu_core_regwrite(device, A6XX_GPU_GMU_CX_GMU_PWR_COL_CP_RESP,
			gmu->log_wptr_retention);

	/* Pass chipid to GMU FW, must happen before starting GMU */

	/* Keep Core and Major bitfields unchanged */
	chipid = adreno_dev->chipid & 0xFFFF0000;

	/*
	 * Compress minor and patch version into 8 bits
	 * Bit 15-12: minor version
	 * Bit 11-8: patch version
	 */
	chipid = chipid | (ADRENO_CHIPID_MINOR(adreno_dev->chipid) << 12)
			| (ADRENO_CHIPID_PATCH(adreno_dev->chipid) << 8);

	gmu_core_regwrite(device, A6XX_GMU_HFI_SFR_ADDR, chipid);

	/* Log size is encoded in (number of 4K units - 1) */
	gmu_log_info = (gmu->gmu_log->gmuaddr & 0xFFFFF000) |
		((LOGMEM_SIZE/SZ_4K - 1) & 0xFF);
	gmu_core_regwrite(device, A6XX_GPU_GMU_CX_GMU_PWR_COL_CP_MSG,
			gmu_log_info);

	/* Configure power control and bring the GMU out of reset */
	a6xx_gmu_power_config(device);

	a6xx_gmu_enable_lm(device);
}

/*
 * a6xx_gmu_fw_start() - set up GMU and start FW
 * @device: Pointer to KGSL device
 */
static int a6xx_gmu_fw_start(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int ret;

	/* Do the necessary register programming */
	a6xx_gmu_register_config(device);

	if (!test_and_set_bit(GMU_BOOT_INIT_DONE,
		&device->gmu_core.flags))
		ret = _load_gmu_rpmh_ucode(device);
	else
		ret = a6xx_rpmh_power_on_gpu(device);
	if (ret)
		return ret;

	/* Load GMU image via AHB bus */
	ret = load_gmu_fw(device);
	if (ret)
		return ret;

	/* Populate the GMU version info before GMU boots */
	load_gmu_version_info(device);

	/* Clear any previously set cm3 fault */
	atomic_set(&gmu->cm3_fault, 0);

	ret = a6xx_gmu_device_start(device);
	if (ret)
		return ret;

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		ret = a6xx_gmu_gfx_rail_on(device);
		if (ret) {
			a6xx_gmu_oob_clear(device, oob_boot_slumber);
			return ret;
		}
	}

	if (gmu->idle_level < GPU_HW_SPTP_PC) {
		ret = a6xx_gmu_sptprac_enable(adreno_dev);
		if (ret)
			return ret;
	}

	ret = a6xx_gmu_hfi_start(device);
	if (ret)
		return ret;

	/* Make sure the write to start HFI happens before sending a message */
	wmb();
	return ret;
}

static int a6xx_gmu_alloc_and_map(struct a6xx_gmu_device *gmu,
		struct gmu_memdesc *md, unsigned int attrs)
{
	struct iommu_domain *domain = a6xx_gmu_ctx[md->ctx_idx].domain;
	int ret;

	md->hostptr = dma_alloc_attrs(&gmu->pdev->dev, (size_t) md->size,
		&md->physaddr, GFP_KERNEL, 0);

	if (md->hostptr == NULL)
		return -ENOMEM;

	ret = iommu_map(domain, md->gmuaddr, md->physaddr, md->size,
		attrs);

	if (ret) {
		dev_err(&gmu->pdev->dev,
				"gmu map err: gaddr=0x%016llX, paddr=0x%pa\n",
				md->gmuaddr, &(md->physaddr));
		a6xx_gmu_mem_free(gmu, md);
	}

	return ret;
}

/*
 * There are a few static memory buffers that are allocated and mapped at boot
 * time for GMU to function. The buffers are permanent (not freed) after
 * GPU boot. The size of the buffers are constant and not expected to change.
 *
 * We define an array and a simple allocator to keep track of the currently
 * active SMMU entries of GMU kernel mode context. Each entry is assigned
 * a unique address inside GMU kernel mode address range.
 */
static struct gmu_memdesc *a6xx_gmu_kmem_allocate(struct a6xx_gmu_device *gmu,
		enum gmu_mem_type mem_type, unsigned int addr,
		unsigned int size, unsigned int attrs)
{
	static unsigned int next_uncached_kernel_alloc;
	static unsigned int next_uncached_user_alloc;

	struct gmu_memdesc *md;
	int ret;
	int entry_idx = find_first_zero_bit(
			&gmu->kmem_bitmap, GMU_KERNEL_ENTRIES);

	if (entry_idx >= GMU_KERNEL_ENTRIES) {
		dev_err(&gmu->pdev->dev,
				"Ran out of GMU kernel mempool slots\n");
		return ERR_PTR(-EINVAL);
	}

	/* Non-TCM requests have page alignment requirement */
	if ((mem_type != GMU_ITCM) && (mem_type != GMU_DTCM) &&
			addr & (PAGE_SIZE - 1)) {
		dev_err(&gmu->pdev->dev,
				"Invalid alignment request 0x%X\n",
				addr);
		return ERR_PTR(-EINVAL);
	}

	md = &gmu->kmem_entries[entry_idx];
	set_bit(entry_idx, &gmu->kmem_bitmap);

	memset(md, 0, sizeof(*md));

	switch (mem_type) {
	case GMU_ITCM:
	case GMU_DTCM:
		/* Assign values and return without mapping */
		md->size = size;
		md->mem_type = mem_type;
		md->gmuaddr = addr;
		return md;

	case GMU_DCACHE:
	case GMU_ICACHE:
		md->ctx_idx = GMU_CONTEXT_KERNEL;
		size = PAGE_ALIGN(size);
		break;

	case GMU_NONCACHED_KERNEL:
		/* Set start address for first uncached kernel alloc */
		if (next_uncached_kernel_alloc == 0)
			next_uncached_kernel_alloc = gmu->vma[mem_type].start;

		if (addr == 0)
			addr = next_uncached_kernel_alloc;

		md->ctx_idx = GMU_CONTEXT_KERNEL;
		size = PAGE_ALIGN(size);
		break;
	case GMU_NONCACHED_USER:
		/* Set start address for first uncached kernel alloc */
		if (next_uncached_user_alloc == 0)
			next_uncached_user_alloc = gmu->vma[mem_type].start;

		if (addr == 0)
			addr = next_uncached_user_alloc;

		md->ctx_idx = GMU_CONTEXT_USER;
		size = PAGE_ALIGN(size);
		break;

	default:
		dev_err(&gmu->pdev->dev,
				"Invalid memory type (%d) requested\n",
				mem_type);
		clear_bit(entry_idx, &gmu->kmem_bitmap);
		return ERR_PTR(-EINVAL);
	}

	md->size = size;
	md->mem_type = mem_type;
	md->gmuaddr = addr;

	ret = a6xx_gmu_alloc_and_map(gmu, md, attrs);
	if (ret) {
		clear_bit(entry_idx, &gmu->kmem_bitmap);
		return ERR_PTR(ret);
	}

	if (mem_type == GMU_NONCACHED_KERNEL)
		next_uncached_kernel_alloc = PAGE_ALIGN(md->gmuaddr + md->size);
	if (mem_type == GMU_NONCACHED_USER)
		next_uncached_user_alloc = PAGE_ALIGN(md->gmuaddr + md->size);

	return md;
}


static int a6xx_gmu_cache_finalize(struct adreno_device *adreno_dev,
		struct a6xx_gmu_device *gmu)
{
	struct gmu_memdesc *md;

	/* Preallocations were made so no need to request all this memory */
	if (gmu->preallocations)
		return 0;

	md = a6xx_gmu_kmem_allocate(gmu, GMU_ICACHE,
			gmu->vma[GMU_ICACHE].start, gmu->vma[GMU_ICACHE].size,
			(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(md))
		return PTR_ERR(md);

	if (!adreno_is_a650_family(adreno_dev)) {
		md = a6xx_gmu_kmem_allocate(gmu, GMU_DCACHE,
				gmu->vma[GMU_DCACHE].start,
				gmu->vma[GMU_DCACHE].size,
				(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
		if (IS_ERR(md))
			return PTR_ERR(md);
	}

	md = a6xx_gmu_kmem_allocate(gmu, GMU_NONCACHED_KERNEL,
			0, SZ_4K, (IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(md))
		return PTR_ERR(md);

	gmu->preallocations = true;

	return 0;
}

static enum gmu_mem_type a6xx_gmu_get_blk_memtype(struct a6xx_gmu_device *gmu,
		struct gmu_block_header *blk)
{
	int i;

	for (i = 0; i < GMU_MEM_TYPE_MAX; i++) {
		if (blk->addr >= gmu->vma[i].start &&
				blk->addr + blk->value <=
				gmu->vma[i].start + gmu->vma[i].size)
			return (enum gmu_mem_type)i;
	}

	return GMU_MEM_TYPE_MAX;
}

static int a6xx_gmu_prealloc_req(struct a6xx_gmu_device *gmu,
		struct gmu_block_header *blk)
{
	enum gmu_mem_type type;
	struct gmu_memdesc *md;

	/* Check to see if this memdesc is already around */
	md = a6xx_gmu_get_memdesc(gmu, blk->addr, blk->value);
	if (md)
		return 0;

	type = a6xx_gmu_get_blk_memtype(gmu, blk);
	if (type >= GMU_MEM_TYPE_MAX)
		return -EINVAL;

	md = a6xx_gmu_kmem_allocate(gmu, type, blk->addr, blk->value,
			(IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	if (IS_ERR(md))
		return PTR_ERR(md);

	gmu->preallocations = true;

	return 0;
}

/*
 * a6xx_gmu_load_firmware() - Load the ucode into the GPMU RAM & PDC/RSC
 * @device: Pointer to KGSL device
 */
static int a6xx_gmu_load_firmware(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	struct gmu_block_header *blk;
	int ret, offset = 0;

	/* GMU fw already saved and verified so do nothing new */
	if (gmu->fw_image)
		return 0;

	if (a6xx_core->gmufw_name == NULL)
		return -EINVAL;

	ret = request_firmware(&gmu->fw_image, a6xx_core->gmufw_name,
			&gmu->pdev->dev);
	if (ret) {
		dev_err(&gmu->pdev->dev, "request_firmware (%s) failed: %d\n",
				a6xx_core->gmufw_name, ret);
		return ret;
	}

	/*
	 * Zero payload fw blocks contain meta data and are
	 * guaranteed to precede fw load data. Parse the
	 * meta data blocks.
	 */
	while (offset < gmu->fw_image->size) {
		blk = (struct gmu_block_header *)&gmu->fw_image->data[offset];

		if (offset + sizeof(*blk) > gmu->fw_image->size) {
			dev_err(&gmu->pdev->dev, "Invalid FW Block\n");
			return -EINVAL;
		}

		/* Done with zero length blocks so return */
		if (blk->size)
			break;

		offset += sizeof(*blk);

		if (blk->type == GMU_BLK_TYPE_PREALLOC_REQ ||
				blk->type == GMU_BLK_TYPE_PREALLOC_PERSIST_REQ)
			ret = a6xx_gmu_prealloc_req(gmu, blk);

		if (ret)
			return ret;
	}

	 /* Request any other cache ranges that might be required */
	return a6xx_gmu_cache_finalize(adreno_dev, gmu);
}

static int a6xx_gmu_memory_probe(struct adreno_device *adreno_dev,
		struct a6xx_gmu_device *gmu)
{
	/* Allocates & maps memory for HFI */
	if (IS_ERR_OR_NULL(gmu->hfi_mem))
		gmu->hfi_mem = a6xx_gmu_kmem_allocate(gmu,
				GMU_NONCACHED_KERNEL, 0,
				HFIMEM_SIZE, (IOMMU_READ | IOMMU_WRITE));
	if (IS_ERR(gmu->hfi_mem))
		return PTR_ERR(gmu->hfi_mem);

	/* Allocates & maps GMU crash dump memory */
	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) {
		if (IS_ERR_OR_NULL(gmu->dump_mem))
			gmu->dump_mem = a6xx_gmu_kmem_allocate(gmu,
					GMU_NONCACHED_KERNEL, 0, SZ_16K,
					(IOMMU_READ | IOMMU_WRITE));
		if (IS_ERR(gmu->dump_mem))
			return PTR_ERR(gmu->dump_mem);
	}

	/* GMU master log */
	if (IS_ERR_OR_NULL(gmu->gmu_log))
		gmu->gmu_log = a6xx_gmu_kmem_allocate(gmu,
				GMU_NONCACHED_KERNEL, 0,
				SZ_4K, (IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV));
	return PTR_ERR_OR_ZERO(gmu->gmu_log);
}

static int a6xx_gmu_init(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int ret;

	ret = a6xx_gmu_load_firmware(device);
	if (ret)
		return ret;

	ret = a6xx_gmu_memory_probe(ADRENO_DEVICE(device), gmu);
	if (ret)
		return ret;

	a6xx_hfi_init(gmu);

	return 0;
}

#define A6XX_VBIF_XIN_HALT_CTRL1_ACKS   (BIT(0) | BIT(1) | BIT(2) | BIT(3))

static void do_gbif_halt(struct kgsl_device *device, u32 reg, u32 ack_reg,
	u32 mask, const char *client)
{
	u32 ack;
	unsigned long t;

	kgsl_regwrite(device, reg, mask);

	t = jiffies + msecs_to_jiffies(100);
	do {
		kgsl_regread(device, ack_reg, &ack);
		if ((ack & mask) == mask)
			return;

		/*
		 * If we are attempting recovery in case of stall-on-fault
		 * then the halt sequence will not complete as long as SMMU
		 * is stalled.
		 */
		kgsl_mmu_pagefault_resume(&device->mmu);

		usleep_range(10, 100);
	} while (!time_after(jiffies, t));

	/* Check one last time */
	kgsl_mmu_pagefault_resume(&device->mmu);

	kgsl_regread(device, ack_reg, &ack);
	if ((ack & mask) == mask)
		return;

	dev_err(device->dev, "%s GBIF halt timed out\n", client);
}

static int a6xx_gmu_pwrctrl_suspend(struct kgsl_device *device)
{
	int ret = 0;
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* If SPTP_RAC is on, turn off SPTP_RAC HS */
	a6xx_gmu_sptprac_disable(adreno_dev);

	/* Disconnect GPU from BUS is not needed if CX GDSC goes off later */

	/* Check no outstanding RPMh voting */
	a6xx_complete_rpmh_votes(device);

	/* Clear the WRITEDROPPED fields and set fence to allow mode */
	gmu_core_regwrite(device, A6XX_GMU_AHB_FENCE_STATUS_CLR, 0x7);
	gmu_core_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);

	/* Make sure above writes are committed before we proceed to recovery */
	wmb();

	gmu_core_regwrite(device, A6XX_GMU_CM3_SYSRESET, 1);

	if (adreno_has_gbif(adreno_dev)) {
		struct adreno_gpudev *gpudev =
			ADRENO_GPU_DEVICE(adreno_dev);

		/* Halt GX traffic */
		if (a6xx_gmu_gx_is_on(device))
			do_gbif_halt(device, A6XX_RBBM_GBIF_HALT,
				A6XX_RBBM_GBIF_HALT_ACK,
				gpudev->gbif_gx_halt_mask,
				"GX");

		/* Halt CX traffic */
		do_gbif_halt(device, A6XX_GBIF_HALT, A6XX_GBIF_HALT_ACK,
			gpudev->gbif_arb_halt_mask, "CX");
	}

	if (a6xx_gmu_gx_is_on(device))
		kgsl_regwrite(device, A6XX_RBBM_SW_RESET_CMD, 0x1);

	/* Allow the software reset to complete */
	udelay(100);

	/*
	 * This is based on the assumption that GMU is the only one controlling
	 * the GX HS. This code path is the only client voting for GX through
	 * the regulator interface.
	 */
	if (gmu->gx_gdsc) {
		if (a6xx_gmu_gx_is_on(device)) {
			/* Switch gx gdsc control from GMU to CPU
			 * force non-zero reference count in clk driver
			 * so next disable call will turn
			 * off the GDSC
			 */
			ret = regulator_enable(gmu->gx_gdsc);
			if (ret)
				dev_err(&gmu->pdev->dev,
					"suspend fail: gx enable %d\n", ret);

			ret = regulator_disable(gmu->gx_gdsc);
			if (ret)
				dev_err(&gmu->pdev->dev,
					"suspend fail: gx disable %d\n", ret);

			if (a6xx_gmu_gx_is_on(device))
				dev_err(&gmu->pdev->dev,
					"gx is stuck on\n");
		}
	}

	return ret;
}

/*
 * a6xx_gmu_notify_slumber() - initiate request to GMU to prepare to slumber
 * @device: Pointer to KGSL device
 */
static int a6xx_gmu_notify_slumber(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int bus_level = pwr->pwrlevels[pwr->default_pwrlevel].bus_freq;
	int perf_idx = gmu->num_gpupwrlevels - pwr->default_pwrlevel - 1;
	int ret, state;

	/* Disable the power counter so that the GMU is not busy */
	gmu_core_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0);

	/* Turn off SPTPRAC if we own it */
	if (gmu->idle_level < GPU_HW_SPTP_PC)
		a6xx_gmu_sptprac_disable(adreno_dev);

	if (!ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		struct hfi_prep_slumber_cmd req = {
			.freq = perf_idx,
			.bw = bus_level,
		};

		ret = a6xx_hfi_send_req(gmu, H2F_MSG_PREPARE_SLUMBER, &req);
		goto out;
	}

	gmu_core_regwrite(device, A6XX_GMU_BOOT_SLUMBER_OPTION,
			OOB_SLUMBER_OPTION);
	gmu_core_regwrite(device, A6XX_GMU_GX_VOTE_IDX, perf_idx);
	gmu_core_regwrite(device, A6XX_GMU_MX_VOTE_IDX, bus_level);

	ret = a6xx_gmu_oob_set(device, oob_boot_slumber);
	a6xx_gmu_oob_clear(device, oob_boot_slumber);

	if (!ret) {
		gmu_core_regread(device,
			A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &state);
		if (state != GPU_HW_SLUMBER) {
			dev_err(&gmu->pdev->dev,
					"Failed to prepare for slumber: 0x%x\n",
					state);
			ret = -EINVAL;
		}
	}

out:
	/* Make sure the fence is in ALLOW mode */
	gmu_core_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);
	return ret;
}

/*
 * a6xx_gmu_rpmh_gpu_pwrctrl() - GPU power control via RPMh/GMU interface
 * @adreno_dev: Pointer to adreno device
 * @mode: requested power mode
 * @arg1: first argument for mode control
 * @arg2: second argument for mode control
 */
static int a6xx_gmu_rpmh_gpu_pwrctrl(struct kgsl_device *device,
		unsigned int mode, unsigned int arg1, unsigned int arg2)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int ret;

	switch (mode) {
	case GMU_FW_START:
		ret = a6xx_gmu_fw_start(device);
		break;
	case GMU_SUSPEND:
		ret = a6xx_gmu_pwrctrl_suspend(device);
		break;
	case GMU_FW_STOP:
		if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
			a6xx_gmu_oob_clear(device, oob_boot_slumber);
		ret = a6xx_rpmh_power_off_gpu(device);
		break;
	case GMU_DCVS_NOHFI:
		ret = a6xx_gmu_dcvs_nohfi(device, arg1, arg2);
		break;
	case GMU_NOTIFY_SLUMBER:
		ret = a6xx_gmu_notify_slumber(device);
		break;
	default:
		dev_err(&gmu->pdev->dev,
				"unsupported GMU power ctrl mode:%d\n", mode);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int a6xx_gmu_suspend(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return 0;

	/* Pending message in all queues are abandoned */
	a6xx_gmu_irq_disable(device);
	a6xx_hfi_stop(gmu);

	if (a6xx_gmu_rpmh_gpu_pwrctrl(device, GMU_SUSPEND, 0, 0))
		return -EINVAL;

	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);
	clear_bit(GMU_CLK_ON, &device->gmu_core.flags);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CX_GDSC))
		regulator_set_mode(gmu->cx_gdsc, REGULATOR_MODE_IDLE);

	if (!kgsl_regulator_disable_wait(gmu->cx_gdsc, 5000))
		dev_err(&gmu->pdev->dev, "GMU CX gdsc off timeout\n");

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CX_GDSC))
		regulator_set_mode(gmu->cx_gdsc, REGULATOR_MODE_NORMAL);

	dev_err(&gmu->pdev->dev, "Suspended GMU\n");

	clear_bit(GMU_FAULT, &device->gmu_core.flags);

	return 0;
}

static void a6xx_gmu_snapshot(struct kgsl_device *device);

static int a6xx_gmu_dcvs_set(struct kgsl_device *device,
		int gpu_pwrlevel, int bus_level)
{
	int ret = 0;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct hfi_gx_bw_perf_vote_cmd req = {
		.ack_type = DCVS_ACK_BLOCK,
		.freq = INVALID_DCVS_IDX,
		.bw = INVALID_DCVS_IDX,
	};

	/* If GMU has not been started, save it */
	if (!test_bit(GMU_HFI_ON, &device->gmu_core.flags)) {
		/* store clock change request */
		set_bit(GMU_DCVS_REPLAY, &device->gmu_core.flags);
		return 0;
	}

	/* Do not set to XO and lower GPU clock vote from GMU */
	if ((gpu_pwrlevel != INVALID_DCVS_IDX) &&
			(gpu_pwrlevel >= gmu->num_gpupwrlevels - 1))
		return -EINVAL;

	if (gpu_pwrlevel < gmu->num_gpupwrlevels - 1)
		req.freq = gmu->num_gpupwrlevels - gpu_pwrlevel - 1;

	if (bus_level < pwr->ddr_table_count && bus_level > 0)
		req.bw = bus_level;

	/* GMU will vote for slumber levels through the sleep sequence */
	if ((req.freq == INVALID_DCVS_IDX) &&
		(req.bw == INVALID_DCVS_IDX)) {
		clear_bit(GMU_DCVS_REPLAY, &device->gmu_core.flags);
		return 0;
	}

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		ret = a6xx_gmu_rpmh_gpu_pwrctrl(device,
			GMU_DCVS_NOHFI, req.freq, req.bw);
	else if (test_bit(GMU_HFI_ON, &device->gmu_core.flags))
		ret = a6xx_hfi_send_req(gmu, H2F_MSG_GX_BW_PERF_VOTE, &req);

	if (ret) {
		dev_err_ratelimited(&gmu->pdev->dev,
			"Failed to set GPU perf idx %d, bw idx %d\n",
			req.freq, req.bw);

		/*
		 * We can be here in two situations. First, we send a dcvs
		 * hfi so gmu knows at what level it must bring up the gpu.
		 * If that fails, it is already being handled as part of
		 * gmu boot failures. The other reason why we are here is
		 * because we are trying to scale an active gpu. For this,
		 * we need to do inline snapshot and dispatcher based
		 * recovery.
		 */
		if (test_bit(ADRENO_DEVICE_STARTED, &adreno_dev->priv)) {
			a6xx_gmu_snapshot(device);
			adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT |
				ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
			adreno_dispatcher_schedule(device);
		}
	}

	/* indicate actual clock change */
	clear_bit(GMU_DCVS_REPLAY, &device->gmu_core.flags);
	return ret;
}

static int a6xx_gmu_ifpc_store(struct kgsl_device *device,
		unsigned int val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	unsigned int requested_idle_level;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		return -EINVAL;

	if ((val && gmu->idle_level >= GPU_HW_IFPC) ||
			(!val && gmu->idle_level < GPU_HW_IFPC))
		return 0;

	if (val)
		requested_idle_level = GPU_HW_IFPC;
	else {
		if (ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC))
			requested_idle_level = GPU_HW_SPTP_PC;
		else
			requested_idle_level = GPU_HW_ACTIVE;
	}

	mutex_lock(&device->mutex);

	/* Power down the GPU before changing the idle level */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	gmu->idle_level = requested_idle_level;
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);

	return 0;
}

static unsigned int a6xx_gmu_ifpc_show(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	return gmu->idle_level >= GPU_HW_IFPC;
}

struct gmu_mem_type_desc {
	struct gmu_memdesc *memdesc;
	uint32_t type;
};

static size_t a6xx_snapshot_gmu_mem(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
		(struct kgsl_snapshot_gmu_mem *)buf;
	unsigned int *data = (unsigned int *)
		(buf + sizeof(*mem_hdr));
	struct gmu_mem_type_desc *desc = priv;

	if (priv == NULL)
		return 0;

	if (remain < desc->memdesc->size + sizeof(*mem_hdr)) {
		dev_err(device->dev,
			"snapshot: Not enough memory for the gmu section %d\n",
			desc->type);
		return 0;
	}

	memset(mem_hdr, 0, sizeof(*mem_hdr));
	mem_hdr->type = desc->type;
	mem_hdr->hostaddr = (uintptr_t)desc->memdesc->hostptr;
	mem_hdr->gmuaddr = desc->memdesc->gmuaddr;
	mem_hdr->gpuaddr = 0;

	/* Just copy the ringbuffer, there are no active IBs */
	memcpy(data, desc->memdesc->hostptr, desc->memdesc->size);

	return desc->memdesc->size + sizeof(*mem_hdr);
}

struct a6xx_tcm_data {
	enum gmu_mem_type type;
	u32 start;
	u32 last;
};

static size_t a6xx_snapshot_gmu_tcm(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct kgsl_snapshot_gmu_mem *mem_hdr =
		(struct kgsl_snapshot_gmu_mem *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*mem_hdr));
	unsigned int i, bytes;
	struct a6xx_tcm_data *tcm = priv;

	bytes = (tcm->last - tcm->start + 1) << 2;

	if (remain < bytes + sizeof(*mem_hdr)) {
		SNAPSHOT_ERR_NOMEM(device, "GMU Memory");
		return 0;
	}

	mem_hdr->type = SNAPSHOT_GMU_MEM_BIN_BLOCK;
	mem_hdr->hostaddr = 0;
	mem_hdr->gmuaddr = gmu->vma[tcm->type].start;
	mem_hdr->gpuaddr = 0;

	for (i = tcm->start; i <= tcm->last; i++)
		kgsl_regread(device, i, data++);

	return bytes + sizeof(*mem_hdr);
}

static void a6xx_gmu_snapshot_memories(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct gmu_mem_type_desc desc;
	struct gmu_memdesc *md;
	int i;

	for (i = 0; i < ARRAY_SIZE(gmu->kmem_entries); i++) {
		if (!test_bit(i, &gmu->kmem_bitmap))
			continue;

		md = &gmu->kmem_entries[i];
		if (!md->size)
			continue;

		desc.memdesc = md;
		if (md == gmu->hfi_mem)
			desc.type = SNAPSHOT_GMU_MEM_HFI;
		else if (md == gmu->gmu_log)
			desc.type = SNAPSHOT_GMU_MEM_LOG;
		else if (md == gmu->dump_mem)
			desc.type = SNAPSHOT_GMU_MEM_DEBUG;
		else
			desc.type = SNAPSHOT_GMU_MEM_BIN_BLOCK;

		if (md->mem_type == GMU_ITCM) {
			struct a6xx_tcm_data tcm = {
				.type = md->mem_type,
				.start = a6xx_gmu_tcm_registers[0],
				.last = a6xx_gmu_tcm_registers[1],
			};

			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
				snapshot, a6xx_snapshot_gmu_tcm, &tcm);
		} else if (md->mem_type == GMU_DTCM) {
			struct a6xx_tcm_data tcm = {
				.type = md->mem_type,
				.start = a6xx_gmu_tcm_registers[2],
				.last = a6xx_gmu_tcm_registers[3],
			};

			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
				snapshot, a6xx_snapshot_gmu_tcm, &tcm);
		} else {
			kgsl_snapshot_add_section(device,
				KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
				snapshot, a6xx_snapshot_gmu_mem, &desc);
		}
	}
}

struct kgsl_snapshot_gmu_version {
	uint32_t type;
	uint32_t value;
};

static size_t a6xx_snapshot_gmu_version(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_debug *header = (struct kgsl_snapshot_debug *)buf;
	uint32_t *data = (uint32_t *) (buf + sizeof(*header));
	struct kgsl_snapshot_gmu_version *ver = priv;

	if (remain < DEBUG_SECTION_SZ(1)) {
		SNAPSHOT_ERR_NOMEM(device, "GMU Version");
		return 0;
	}

	header->type = ver->type;
	header->size = 1;

	*data = ver->value;

	return DEBUG_SECTION_SZ(1);
}

static void a6xx_gmu_snapshot_versions(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	int i;
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct kgsl_snapshot_gmu_version gmu_vers[] = {
		{ .type = SNAPSHOT_DEBUG_GMU_CORE_VERSION,
			.value = gmu->ver.core, },
		{ .type = SNAPSHOT_DEBUG_GMU_CORE_DEV_VERSION,
			.value = gmu->ver.core_dev, },
		{ .type = SNAPSHOT_DEBUG_GMU_PWR_VERSION,
			.value = gmu->ver.pwr, },
		{ .type = SNAPSHOT_DEBUG_GMU_PWR_DEV_VERSION,
			.value = gmu->ver.pwr_dev, },
		{ .type = SNAPSHOT_DEBUG_GMU_HFI_VERSION,
			.value = gmu->ver.hfi, },
	};

	for (i = 0; i < ARRAY_SIZE(gmu_vers); i++)
		kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_DEBUG,
				snapshot, a6xx_snapshot_gmu_version,
				&gmu_vers[i]);
}

/*
 * a6xx_gmu_device_snapshot() - A6XX GMU snapshot function
 * @device: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A6XX GMU specific bits and pieces are grabbed
 * into the snapshot memory
 */
static void a6xx_gmu_device_snapshot(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	unsigned int val;

	a6xx_gmu_snapshot_versions(device, snapshot);

	a6xx_gmu_snapshot_memories(device, snapshot);

	/* Snapshot tcms as registers for legacy targets */
	if (adreno_is_a630(ADRENO_DEVICE(device)) ||
			adreno_is_a615_family(ADRENO_DEVICE(device)))
		adreno_snapshot_registers(device, snapshot,
				a6xx_gmu_tcm_registers,
				ARRAY_SIZE(a6xx_gmu_tcm_registers) / 2);

	adreno_snapshot_registers(device, snapshot, a6xx_gmu_registers,
					ARRAY_SIZE(a6xx_gmu_registers) / 2);

	/* Snapshot A660 specific GMU registers */
	if (adreno_is_a660(ADRENO_DEVICE(device)))
		adreno_snapshot_registers(device, snapshot, a660_gmu_registers,
					ARRAY_SIZE(a660_gmu_registers) / 2);

	if (a6xx_gmu_gx_is_on(device)) {
		/* Set fence to ALLOW mode so registers can be read */
		kgsl_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);
		/* Make sure the previous write posted before reading */
		wmb();
		kgsl_regread(device, A6XX_GMU_AO_AHB_FENCE_CTRL, &val);

		dev_err(device->dev, "set FENCE to ALLOW mode:%x\n", val);
		adreno_snapshot_registers(device, snapshot,
				a6xx_gmu_gx_registers,
				ARRAY_SIZE(a6xx_gmu_gx_registers) / 2);
	}
}

static void a6xx_gmu_cooperative_reset(struct kgsl_device *device)
{

	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	unsigned int result;

	gmu_core_regwrite(device, A6XX_GMU_CX_GMU_WDOG_CTRL, 0);
	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, BIT(17));

	/*
	 * After triggering graceful death wait for snapshot ready
	 * indication from GMU.
	 */
	if (!timed_poll_check(device, A6XX_GMU_CM3_FW_INIT_RESULT,
				0x800, 2, 0x800))
		return;

	gmu_core_regread(device, A6XX_GMU_CM3_FW_INIT_RESULT, &result);
	dev_err(&gmu->pdev->dev,
		"GMU cooperative reset timed out 0x%x\n", result);
	/*
	 * If we dont get a snapshot ready from GMU, trigger NMI
	 * and if we still timeout then we just continue with reset.
	 */
	adreno_gmu_send_nmi(ADRENO_DEVICE(device));
	udelay(200);
	gmu_core_regread(device, A6XX_GMU_CM3_FW_INIT_RESULT, &result);
	if ((result & 0x800) != 0x800)
		dev_err(&gmu->pdev->dev,
			"GMU cooperative reset NMI timed out 0x%x\n", result);
}

static int a6xx_gmu_wait_for_active_transition(
	struct kgsl_device *device)
{
	unsigned int reg, num_retries;
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	gmu_core_regread(device,
		A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);

	for (num_retries = 0; reg != GPU_HW_ACTIVE && num_retries < 100;
		num_retries++) {
		/* Wait for small time before trying again */
		udelay(5);
		gmu_core_regread(device,
			A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
	}

	if (reg == GPU_HW_ACTIVE)
		return 0;

	dev_err(&gmu->pdev->dev,
		"GMU failed to move to ACTIVE state, Current state: 0x%x\n",
		reg);

	return -ETIMEDOUT;
}

static bool a6xx_gmu_scales_bandwidth(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	return (ADRENO_GPUREV(adreno_dev) >= ADRENO_REV_A640);
}

static u64 a6xx_gmu_read_alwayson(struct kgsl_device *device)
{
	return a6xx_read_alwayson(ADRENO_DEVICE(device));
}

static irqreturn_t a6xx_gmu_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int mask, status = 0;

	gmu_core_regread(device, A6XX_GMU_AO_HOST_INTERRUPT_STATUS, &status);
	gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_CLR, status);

	/* Ignore GMU_INT_RSCC_COMP and GMU_INT_DBD WAKEUP interrupts */
	if (status & GMU_INT_WDOG_BITE) {
		/* Temporarily mask the watchdog interrupt to prevent a storm */
		gmu_core_regread(device, A6XX_GMU_AO_HOST_INTERRUPT_MASK,
			&mask);
		gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_MASK,
				(mask | GMU_INT_WDOG_BITE));

		/* make sure we're reading the latest cm3_fault */
		smp_rmb();

		/*
		 * We should not send NMI if there was a CM3 fault reported
		 * because we don't want to overwrite the critical CM3 state
		 * captured by gmu before it sent the CM3 fault interrupt.
		 */
		if (!atomic_read(&gmu->cm3_fault))
			adreno_gmu_send_nmi(adreno_dev);

		/*
		 * There is sufficient delay for the GMU to have finished
		 * handling the NMI before snapshot is taken, as the fault
		 * worker is scheduled below.
		 */

		dev_err_ratelimited(&gmu->pdev->dev,
				"GMU watchdog expired interrupt received\n");
	}
	if (status & GMU_INT_HOST_AHB_BUS_ERR)
		dev_err_ratelimited(&gmu->pdev->dev,
				"AHB bus error interrupt received\n");
	if (status & GMU_INT_FENCE_ERR) {
		unsigned int fence_status;

		gmu_core_regread(device, A6XX_GMU_AHB_FENCE_STATUS,
			&fence_status);
		dev_err_ratelimited(&gmu->pdev->dev,
			"FENCE error interrupt received %x\n", fence_status);
	}

	if (status & ~GMU_AO_INT_MASK)
		dev_err_ratelimited(&gmu->pdev->dev,
				"Unhandled GMU interrupts 0x%lx\n",
				status & ~GMU_AO_INT_MASK);

	return IRQ_HANDLED;
}


static void a6xx_gmu_snapshot(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	/* Abstain from sending another nmi or over-writing snapshot */
	if (test_and_set_bit(GMU_FAULT, &device->gmu_core.flags))
		return;

	/* make sure we're reading the latest cm3_fault */
	smp_rmb();

	/*
	 * We should not send NMI if there was a CM3 fault reported because we
	 * don't want to overwrite the critical CM3 state captured by gmu before
	 * it sent the CM3 fault interrupt.
	 */
	if (!atomic_read(&gmu->cm3_fault)) {
		adreno_gmu_send_nmi(adreno_dev);

		/* Wait for the NMI to be handled */
		udelay(100);
	}

	kgsl_device_snapshot(device, NULL, true);

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR,
		0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK,
		HFI_IRQ_MASK);

	gmu->fault_count++;
}

/* Caller shall ensure GPU is ready for SLUMBER */
static void a6xx_gmu_stop(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret = 0;

	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return;

	/* Force suspend if gmu is already in fault */
	if (test_bit(GMU_FAULT, &device->gmu_core.flags)) {
		a6xx_gmu_suspend(device);
		return;
	}

	/* Wait for the lowest idle level we requested */
	if (a6xx_gmu_wait_for_lowest_idle(device))
		goto error;

	ret = a6xx_gmu_rpmh_gpu_pwrctrl(device,
			GMU_NOTIFY_SLUMBER, 0, 0);
	if (ret)
		goto error;

	if (a6xx_gmu_wait_for_idle(device))
		goto error;

	/* Pending message in all queues are abandoned */
	a6xx_gmu_irq_disable(device);
	a6xx_hfi_stop(gmu);

	a6xx_gmu_rpmh_gpu_pwrctrl(device, GMU_FW_STOP, 0, 0);

	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);
	clear_bit(GMU_CLK_ON, &device->gmu_core.flags);

	/* Pool to make sure that the CX is off */
	if (!kgsl_regulator_disable_wait(gmu->cx_gdsc, 5000))
		dev_err(&gmu->pdev->dev, "GMU CX gdsc off timeout\n");

	icc_set_bw(pwr->icc_path, 0, 0);
	return;

error:
	dev_err(&gmu->pdev->dev, "Failed to stop GMU\n");
	a6xx_gmu_snapshot(device);
	/*
	 * We failed to stop the gmu successfully. Force a suspend
	 * to set things up for a fresh start.
	 */
	a6xx_gmu_suspend(device);
}

static int a6xx_gmu_aop_send_acd_state(struct mbox_chan *channel, bool flag)
{
	char msg_buf[33];
	struct {
		u32 len;
		void *msg;
	} msg;

	if (IS_ERR_OR_NULL(channel))
		return 0;

	msg.len = scnprintf(msg_buf, sizeof(msg_buf),
			"{class: gpu, res: acd, value: %d}", flag);
	msg.msg = msg_buf;

	return mbox_send_message(channel, &msg);
}

static int a6xx_gmu_enable_gdsc(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int ret;

	ret = regulator_enable(gmu->cx_gdsc);
	if (ret)
		dev_err(&gmu->pdev->dev,
			"Failed to enable GMU CX gdsc, error %d\n", ret);

	return ret;
}

static int a6xx_gmu_clk_set_rate(struct a6xx_gmu_device *gmu, const char *id,
	unsigned long rate)
{
	struct clk *clk;

	clk = kgsl_of_clk_by_name(gmu->clks, gmu->num_clks, id);
	if (!clk)
		return -ENODEV;

	return clk_set_rate(clk, rate);
}

static int a6xx_gmu_enable_clks(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int ret;

	ret = a6xx_gmu_clk_set_rate(gmu, "gmu_clk", GMU_FREQUENCY);
	if (ret) {
		dev_err(&gmu->pdev->dev, "Unable to set the GMU clock\n");
		return ret;
	}

	ret = a6xx_gmu_clk_set_rate(gmu, "hub_clk", 150000000);
	if (ret && ret != ENODEV) {
		dev_err(&gmu->pdev->dev, "Unable to set the HUB clock\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(gmu->num_clks, gmu->clks);
	if (ret) {
		dev_err(&gmu->pdev->dev, "Cannot enable GMU clocks\n");
		return ret;
	}

	set_bit(GMU_CLK_ON, &device->gmu_core.flags);
	return 0;
}



static int a6xx_gmu_start_from_init(struct kgsl_device *device)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int level, ret;

	if (device->state == KGSL_STATE_INIT) {
		int ret = a6xx_gmu_aop_send_acd_state(gmu->mailbox.channel,
			adreno_dev->acd_enabled);
		if (ret)
			dev_err(&gmu->pdev->dev,
				"AOP mbox send message failed: %d\n", ret);
	}

	WARN_ON(test_bit(GMU_CLK_ON, &device->gmu_core.flags));

	a6xx_gmu_enable_gdsc(device);
	a6xx_gmu_enable_clks(device);
	a6xx_gmu_irq_enable(device);

	/* Vote for minimal DDR BW for GMU to init */
	level = pwr->pwrlevels[pwr->default_pwrlevel].bus_min;
	icc_set_bw(pwr->icc_path, 0, kBps_to_icc(pwr->ddr_table[level]));

	ret = a6xx_gmu_rpmh_gpu_pwrctrl(device, GMU_FW_START,
				GMU_COLD_BOOT, 0);
	if (ret)
		return ret;

	ret = a6xx_hfi_start(device, gmu, GMU_COLD_BOOT);
	if (ret)
		return ret;

	/* Request default DCVS level */
	return kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
}

static int a6xx_gmu_start_from_slumber(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int ret;

	WARN_ON(test_bit(GMU_CLK_ON, &device->gmu_core.flags));

	a6xx_gmu_enable_gdsc(device);
	a6xx_gmu_enable_clks(device);
	a6xx_gmu_irq_enable(device);

	ret = a6xx_gmu_rpmh_gpu_pwrctrl(device, GMU_FW_START,
				GMU_COLD_BOOT, 0);
	if (ret)
		return ret;

	ret = a6xx_hfi_start(device, gmu, GMU_COLD_BOOT);
	if (ret)
		return ret;

	return kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
}

static int a6xx_gmu_start_from_reset(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int ret;

	a6xx_gmu_suspend(device);

	a6xx_gmu_enable_gdsc(device);
	a6xx_gmu_enable_clks(device);
	a6xx_gmu_irq_enable(device);

	ret = a6xx_gmu_rpmh_gpu_pwrctrl(device, GMU_FW_START, GMU_COLD_BOOT, 0);
	if (ret)
		return ret;

	ret = a6xx_hfi_start(device, gmu, GMU_COLD_BOOT);
	if (ret)
		return ret;

	/* Send DCVS level prior to reset*/
	return kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
}

/* To be called to power on both GPU and GMU */
static int a6xx_gmu_start(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret = 0;

	switch (device->state) {
	case KGSL_STATE_INIT:
	case KGSL_STATE_SUSPEND:
		ret = a6xx_gmu_start_from_init(device);
		break;

	case KGSL_STATE_SLUMBER:
		ret = a6xx_gmu_start_from_slumber(device);
		break;
	case KGSL_STATE_RESET:
		ret = a6xx_gmu_start_from_reset(device);
		break;
	}

	if (ret) {
		if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
			a6xx_gmu_oob_clear(device, oob_boot_slumber);

		a6xx_gmu_snapshot(device);
	}

	return ret;
}

static int a6xx_gmu_acd_set(struct kgsl_device *device, bool val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	int ret;

	if (IS_ERR_OR_NULL(gmu->mailbox.channel))
		return -EINVAL;

	/* Don't do any unneeded work if ACD is already in the correct state */
	if (adreno_dev->acd_enabled == val)
		return 0;

	mutex_lock(&device->mutex);

	/* Power down the GPU before enabling or disabling ACD */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);

	adreno_dev->acd_enabled = val;
	ret = a6xx_gmu_aop_send_acd_state(gmu->mailbox.channel, val);
	if (ret)
		dev_err(&gmu->pdev->dev,
				"AOP mbox send message failed: %d\n", ret);

	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);
	return 0;
}

static struct gmu_dev_ops a6xx_gmudev = {
	.load_firmware = a6xx_gmu_load_firmware,
	.oob_set = a6xx_gmu_oob_set,
	.oob_clear = a6xx_gmu_oob_clear,
	.irq_enable = a6xx_gmu_irq_enable,
	.irq_disable = a6xx_gmu_irq_disable,
	.hfi_start_msg = a6xx_gmu_hfi_start_msg,
	.rpmh_gpu_pwrctrl = a6xx_gmu_rpmh_gpu_pwrctrl,
	.gx_is_on = a6xx_gmu_gx_is_on,
	.wait_for_lowest_idle = a6xx_gmu_wait_for_lowest_idle,
	.wait_for_gmu_idle = a6xx_gmu_wait_for_idle,
	.ifpc_store = a6xx_gmu_ifpc_store,
	.ifpc_show = a6xx_gmu_ifpc_show,
	.snapshot = a6xx_gmu_device_snapshot,
	.cooperative_reset = a6xx_gmu_cooperative_reset,
	.wait_for_active_transition = a6xx_gmu_wait_for_active_transition,
	.read_alwayson = a6xx_gmu_read_alwayson,
	.gmu2host_intr_mask = HFI_IRQ_MASK,
	.gmu_ao_intr_mask = GMU_AO_INT_MASK,
	.scales_bandwidth = a6xx_gmu_scales_bandwidth,
};

static struct gmu_core_ops a6xx_gmu_ops = {
	.init = a6xx_gmu_init,
	.start = a6xx_gmu_start,
	.stop = a6xx_gmu_stop,
	.dcvs_set = a6xx_gmu_dcvs_set,
	.snapshot = a6xx_gmu_snapshot,
	.suspend = a6xx_gmu_suspend,
	.acd_set = a6xx_gmu_acd_set,
};

static int a6xx_gmu_bus_set(struct kgsl_device *device, int buslevel,
	u32 ab)
{
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret = 0;

	if (buslevel != pwr->cur_buslevel) {
		ret = a6xx_gmu_dcvs_set(device, INVALID_DCVS_IDX, buslevel);
		if (ret)
			return ret;

		pwr->cur_buslevel = buslevel;

		trace_kgsl_buslevel(device, pwr->active_pwrlevel, buslevel);
	}

	if (ab != pwr->cur_ab) {
		icc_set_bw(pwr->icc_path, MBps_to_icc(ab), 0);
		pwr->cur_ab = ab;
	}

	return ret;
}

static void a6xx_gmu_iommu_cb_close(struct gmu_iommu_context *ctx);

static void a6xx_gmu_memory_close(struct a6xx_gmu_device *gmu)
{
	int i;
	struct gmu_memdesc *md;
	struct gmu_iommu_context *ctx;

	gmu->hfi_mem = NULL;
	gmu->dump_mem = NULL;
	gmu->gmu_log = NULL;
	gmu->preallocations = false;

	/* Unmap and free all memories in GMU kernel memory pool */
	for (i = 0; i < GMU_KERNEL_ENTRIES; i++) {
		if (!test_bit(i, &gmu->kmem_bitmap))
			continue;

		md = &gmu->kmem_entries[i];
		ctx = &a6xx_gmu_ctx[md->ctx_idx];

		if (md->gmuaddr && md->mem_type != GMU_ITCM &&
				md->mem_type != GMU_DTCM)
			iommu_unmap(ctx->domain, md->gmuaddr, md->size);

		a6xx_gmu_mem_free(gmu, md);

		clear_bit(i, &gmu->kmem_bitmap);
	}

	a6xx_gmu_iommu_cb_close(&a6xx_gmu_ctx[GMU_CONTEXT_KERNEL]);
	a6xx_gmu_iommu_cb_close(&a6xx_gmu_ctx[GMU_CONTEXT_USER]);
}

static int a6xx_gmu_aop_mailbox_init(struct adreno_device *adreno_dev,
		struct a6xx_gmu_device *gmu)
{
	struct kgsl_mailbox *mailbox = &gmu->mailbox;

	mailbox->client.dev = &gmu->pdev->dev;
	mailbox->client.tx_block = true;
	mailbox->client.tx_tout = 1000;
	mailbox->client.knows_txdone = false;

	mailbox->channel = mbox_request_channel(&mailbox->client, 0);
	if (IS_ERR(mailbox->channel))
		return PTR_ERR(mailbox->channel);

	adreno_dev->acd_enabled = true;
	return 0;
}

static void a6xx_gmu_acd_probe(struct kgsl_device *device,
		struct a6xx_gmu_device *gmu, struct device_node *node)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct hfi_acd_table_cmd *cmd = &gmu->hfi.acd_tbl_cmd;
	u32 acd_level, cmd_idx, numlvl = pwr->num_pwrlevels;
	int ret, i;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_ACD))
		return;

	cmd->hdr = 0xFFFFFFFF;
	cmd->version = HFI_ACD_INIT_VERSION;
	cmd->stride = 1;
	cmd->enable_by_level = 0;

	for (i = 0, cmd_idx = 0; i < numlvl; i++) {
		acd_level = pwr->pwrlevels[numlvl - i].acd_level;
		if (acd_level) {
			cmd->enable_by_level |= (1 << i);
			cmd->data[cmd_idx++] = acd_level;
		}
	}

	if (!cmd->enable_by_level)
		return;

	cmd->num_levels = cmd_idx;

	ret = a6xx_gmu_aop_mailbox_init(adreno_dev, gmu);
	if (ret)
		dev_err(&gmu->pdev->dev,
			"AOP mailbox init failed: %d\n", ret);
}

struct rpmh_arc_vals {
	unsigned int num;
	const u16 *val;
};

enum rpmh_vote_type {
	GPU_ARC_VOTE = 0,
	GMU_ARC_VOTE,
	INVALID_ARC_VOTE,
};

/*
 * rpmh_arc_cmds() - query RPMh command database for GX/CX/MX rail
 * VLVL tables. The index of table will be used by GMU to vote rail
 * voltage.
 *
 * @gmu: Pointer to GMU device
 * @arc: Pointer to RPMh rail controller (ARC) voltage table
 * @res_id: Pointer to 8 char array that contains rail name
 */
static int rpmh_arc_cmds(struct a6xx_gmu_device *gmu,
		struct rpmh_arc_vals *arc, const char *res_id)
{
	size_t len = 0;

	arc->val = cmd_db_read_aux_data(res_id, &len);

	/*
	 * cmd_db_read_aux_data() gives us a zero-padded table of
	 * size len that contains the arc values. To determine the
	 * number of arc values, we loop through the table and count
	 * them until we get to the end of the buffer or hit the
	 * zero padding.
	 */
	for (arc->num = 1; arc->num < (len >> 1); arc->num++) {
		if (arc->val[arc->num - 1] != 0 &&  arc->val[arc->num] == 0)
			break;
	}

	return 0;
}

/*
 * setup_volt_dependency_tbl() - set up GX->MX or CX->MX rail voltage
 * dependencies. Second rail voltage shall be equal to or higher than
 * primary rail voltage. VLVL table index was used by RPMh for PMIC
 * voltage setting.
 * @votes: Pointer to a ARC vote descriptor
 * @pri_rail: Pointer to primary power rail VLVL table
 * @sec_rail: Pointer to second/dependent power rail VLVL table
 * @vlvl: Pointer to VLVL table being used by GPU or GMU driver, a subset
 *	of pri_rail VLVL table
 * @num_entries: Valid number of entries in table pointed by "vlvl" parameter
 */
static int setup_volt_dependency_tbl(uint32_t *votes,
		struct rpmh_arc_vals *pri_rail, struct rpmh_arc_vals *sec_rail,
		u16 *vlvl, unsigned int num_entries)
{
	int i, j, k;
	uint16_t cur_vlvl;
	bool found_match;

	/* i tracks current KGSL GPU frequency table entry
	 * j tracks secondary rail voltage table entry
	 * k tracks primary rail voltage table entry
	 */
	for (i = 0; i < num_entries; i++) {
		found_match = false;

		/* Look for a primary rail voltage that matches a VLVL level */
		for (k = 0; k < pri_rail->num; k++) {
			if (pri_rail->val[k] >= vlvl[i]) {
				cur_vlvl = pri_rail->val[k];
				found_match = true;
				break;
			}
		}

		/* If we did not find a matching VLVL level then abort */
		if (!found_match)
			return -EINVAL;

		/*
		 * Look for a secondary rail index whose VLVL value
		 * is greater than or equal to the VLVL value of the
		 * corresponding index of the primary rail
		 */
		for (j = 0; j < sec_rail->num; j++) {
			if (sec_rail->val[j] >= cur_vlvl ||
					j + 1 == sec_rail->num)
				break;
		}

		if (j == sec_rail->num)
			j = 0;

		votes[i] = ARC_VOTE_SET(k, j, cur_vlvl);
	}

	return 0;
}

static int rpmh_gmu_arc_votes_init(struct a6xx_gmu_device *gmu,
		struct rpmh_arc_vals *pri_rail, struct rpmh_arc_vals *sec_rail)
{
	/* Hardcoded values of GMU CX voltage levels */
	u16 gmu_cx_vlvl[] = { 0, RPMH_REGULATOR_LEVEL_MIN_SVS };

	return setup_volt_dependency_tbl(gmu->rpmh_votes.cx_votes, pri_rail,
						sec_rail, gmu_cx_vlvl, 2);
}

/*
 * rpmh_arc_votes_init() - initialized GX RPMh votes needed for rails
 * voltage scaling by GMU.
 * @device: Pointer to KGSL device
 * @gmu: Pointer to GMU device
 * @pri_rail: Pointer to primary power rail VLVL table
 * @sec_rail: Pointer to second/dependent power rail VLVL table
 *	of pri_rail VLVL table
 * @type: the type of the primary rail, GPU or GMU
 */
static int rpmh_arc_votes_init(struct kgsl_device *device,
		struct a6xx_gmu_device *gmu, struct rpmh_arc_vals *pri_rail,
		struct rpmh_arc_vals *sec_rail, unsigned int type)
{
	unsigned int num_freqs;
	u16 vlvl_tbl[MAX_GX_LEVELS];
	int i;

	if (type == GMU_ARC_VOTE)
		return rpmh_gmu_arc_votes_init(gmu, pri_rail, sec_rail);

	num_freqs = gmu->num_gpupwrlevels;

	if (num_freqs > pri_rail->num || num_freqs > ARRAY_SIZE(vlvl_tbl)) {
		dev_err(&gmu->pdev->dev,
			"Defined more GPU DCVS levels than RPMh can support\n");
		return -EINVAL;
	}

	memset(vlvl_tbl, 0, sizeof(vlvl_tbl));
	for (i = 0; i < num_freqs; i++)
		vlvl_tbl[i] = gmu->pwrlevels[i].level;

	return setup_volt_dependency_tbl(gmu->rpmh_votes.gx_votes, pri_rail,
						sec_rail, vlvl_tbl, num_freqs);
}

struct bcm {
	const char *name;
	u32 buswidth;
	u32 channels;
	u32 unit;
	u16 width;
	u8 vcd;
	bool fixed;
};

/*
 * List of Bus Control Modules (BCMs) that need to be configured for the GPU
 * to access DDR. For each bus level we will generate a vote each BC
 */
static struct bcm a660_ddr_bcms[] = {
	{ .name = "SH0", .buswidth = 16 },
	{ .name = "MC0", .buswidth = 4 },
	{ .name = "ACV", .fixed = true },
};

/* Same as above, but for the CNOC BCMs */
static struct bcm a660_cnoc_bcms[] = {
	{ .name = "CN0", .buswidth = 4 },
};

/* Generate a set of bandwidth votes for the list of BCMs */
static void tcs_cmd_data(struct bcm *bcms, int count, u32 ab, u32 ib,
		u32 *data)
{
	int i;

	for (i = 0; i < count; i++) {
		bool valid = true;
		bool commit = false;
		u64 avg, peak, x, y;

		if (i == count - 1 || bcms[i].vcd != bcms[i + 1].vcd)
			commit = true;

		/*
		 * On a660, the "ACV" y vote should be 0x08 if there is a valid
		 * vote and 0x00 if not. This is kind of hacky and a660 specific
		 * but we can clean it up when we add a new target
		 */
		if (bcms[i].fixed) {
			if (!ab && !ib)
				data[i] = BCM_TCS_CMD(commit, false, 0x0, 0x0);
			else
				data[i] = BCM_TCS_CMD(commit, true, 0x0, 0x8);
			continue;
		}

		/* Multiple the bandwidth by the width of the connection */
		avg = ((u64) ab) * bcms[i].width;

		/* And then divide by the total width across channels */
		do_div(avg, bcms[i].buswidth * bcms[i].channels);

		peak = ((u64) ib) * bcms[i].width;
		do_div(peak, bcms[i].buswidth);

		/* Input bandwidth value is in KBps */
		x = avg * 1000ULL;
		do_div(x, bcms[i].unit);

		/* Input bandwidth value is in KBps */
		y = peak * 1000ULL;
		do_div(y, bcms[i].unit);

		/*
		 * If a bandwidth value was specified but the calculation ends
		 * rounding down to zero, set a minimum level
		 */
		if (ab && x == 0)
			x = 1;

		if (ib && y == 0)
			y = 1;

		x = min_t(u64, x, BCM_TCS_CMD_VOTE_MASK);
		y = min_t(u64, y, BCM_TCS_CMD_VOTE_MASK);

		if (!x && !y)
			valid = false;

		data[i] = BCM_TCS_CMD(commit, valid, x, y);
	}
}

struct bcm_data {
	__le32 unit;
	__le16 width;
	u8 vcd;
	u8 reserved;
};

struct rpmh_bw_votes {
	u32 wait_bitmask;
	u32 num_cmds;
	u32 *addrs;
	u32 num_levels;
	u32 **cmds;
};

static void free_rpmh_bw_votes(struct rpmh_bw_votes *votes)
{
	int i;

	if (!votes)
		return;

	for (i = 0; votes->cmds && i < votes->num_levels; i++)
		kfree(votes->cmds[i]);

	kfree(votes->cmds);
	kfree(votes->addrs);
	kfree(votes);
}

/* Build the votes table from the specified bandwidth levels */
static struct rpmh_bw_votes *build_rpmh_bw_votes(struct bcm *bcms,
		int bcm_count, u32 *levels, int levels_count)
{
	struct rpmh_bw_votes *votes;
	int i;

	votes = kzalloc(sizeof(*votes), GFP_KERNEL);
	if (!votes)
		return ERR_PTR(-ENOMEM);

	votes->addrs = kcalloc(bcm_count, sizeof(*votes->cmds), GFP_KERNEL);
	if (!votes->addrs) {
		free_rpmh_bw_votes(votes);
		return ERR_PTR(-ENOMEM);
	}

	votes->cmds = kcalloc(levels_count, sizeof(*votes->cmds), GFP_KERNEL);
	if (!votes->cmds) {
		free_rpmh_bw_votes(votes);
		return ERR_PTR(-ENOMEM);
	}

	votes->num_cmds = bcm_count;
	votes->num_levels = levels_count;

	/* Get the cmd-db information for each BCM */
	for (i = 0; i < bcm_count; i++) {
		size_t l;
		const struct bcm_data *data;

		data = cmd_db_read_aux_data(bcms[i].name, &l);

		votes->addrs[i] = cmd_db_read_addr(bcms[i].name);

		bcms[i].unit = le32_to_cpu(data->unit);
		bcms[i].width = le16_to_cpu(data->width);
		bcms[i].vcd = data->vcd;
	}

	for (i = 0; i < bcm_count; i++) {
		if (i == (bcm_count - 1) || bcms[i].vcd != bcms[i + 1].vcd)
			votes->wait_bitmask |= (1 << i);
	}

	for (i = 0; i < levels_count; i++) {
		votes->cmds[i] = kcalloc(bcm_count, sizeof(u32), GFP_KERNEL);
		if (!votes->cmds[i]) {
			free_rpmh_bw_votes(votes);
			return ERR_PTR(-ENOMEM);
		}

		tcs_cmd_data(bcms, bcm_count, 0, levels[i], votes->cmds[i]);
	}

	return votes;
}

static void build_bwtable_cmd_cache(struct hfi_bwtable_cmd *cmd,
		struct rpmh_bw_votes *ddr, struct rpmh_bw_votes *cnoc)
{
	unsigned int i, j;

	cmd->hdr = 0xFFFFFFFF;
	cmd->bw_level_num = ddr->num_levels;
	cmd->ddr_cmds_num = ddr->num_cmds;
	cmd->ddr_wait_bitmask = ddr->wait_bitmask;

	for (i = 0; i < ddr->num_cmds; i++)
		cmd->ddr_cmd_addrs[i] = ddr->addrs[i];

	for (i = 0; i < ddr->num_levels; i++)
		for (j = 0; j < ddr->num_cmds; j++)
			cmd->ddr_cmd_data[i][j] = (u32) ddr->cmds[i][j];

	if (!cnoc)
		return;

	cmd->cnoc_cmds_num = cnoc->num_cmds;
	cmd->cnoc_wait_bitmask = cnoc->wait_bitmask;

	for (i = 0; i < cnoc->num_cmds; i++)
		cmd->cnoc_cmd_addrs[i] = cnoc->addrs[i];

	for (i = 0; i < cnoc->num_levels; i++)
		for (j = 0; j < cnoc->num_cmds; j++)
			cmd->cnoc_cmd_data[i][j] = (u32) cnoc->cmds[i][j];
}

static int a6xx_gmu_bus_vote_init(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct rpmh_bw_votes *ddr, *cnoc = NULL;
	u32 *cnoc_table;
	u32 count;

	/* Build the DDR votes */
	ddr = build_rpmh_bw_votes(a660_ddr_bcms, ARRAY_SIZE(a660_ddr_bcms),
		pwr->ddr_table, pwr->ddr_table_count);
	if (IS_ERR(ddr))
		return PTR_ERR(ddr);

	/* Get the CNOC table */
	cnoc_table = kgsl_bus_get_table(device->pdev, "qcom,bus-table-cnoc",
		&count);

	/* And build the votes for that, if it exists */
	if (count > 0)
		cnoc = build_rpmh_bw_votes(a660_cnoc_bcms,
			ARRAY_SIZE(a660_cnoc_bcms), cnoc_table, count);
	kfree(cnoc_table);

	if (IS_ERR(cnoc)) {
		free_rpmh_bw_votes(ddr);
		return PTR_ERR(cnoc);
	}

	/* Build the HFI command once */
	build_bwtable_cmd_cache(&gmu->hfi.bwtbl_cmd, ddr, cnoc);

	free_rpmh_bw_votes(ddr);
	free_rpmh_bw_votes(cnoc);

	return 0;
}

static int a6xx_gmu_rpmh_init(struct kgsl_device *device,
		struct a6xx_gmu_device *gmu)
{
	struct rpmh_arc_vals gfx_arc, cx_arc, mx_arc;
	int ret;

	/* Initialize BW tables */
	ret = a6xx_gmu_bus_vote_init(device);
	if (ret)
		return ret;

	/* Populate GPU and GMU frequency vote table */
	ret = rpmh_arc_cmds(gmu, &gfx_arc, "gfx.lvl");
	if (ret)
		return ret;

	ret = rpmh_arc_cmds(gmu, &cx_arc, "cx.lvl");
	if (ret)
		return ret;

	ret = rpmh_arc_cmds(gmu, &mx_arc, "mx.lvl");
	if (ret)
		return ret;

	ret = rpmh_arc_votes_init(device, gmu, &gfx_arc, &mx_arc, GPU_ARC_VOTE);
	if (ret)
		return ret;

	return rpmh_arc_votes_init(device, gmu, &cx_arc, &mx_arc, GMU_ARC_VOTE);
}


static int a6xx_gmu_reg_probe(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);
	struct resource *res;

	res = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM,
			"kgsl_gmu_reg");
	if (!res) {
		dev_err(&gmu->pdev->dev, "The GMU register region isn't defined\n");
		return -ENODEV;
	}

	device->gmu_core.gmu2gpu_offset = (res->start - device->reg_phys) >> 2;
	device->gmu_core.reg_len = resource_size(res);

	/*
	 * We can't use devm_ioremap_resource here because we purposely double
	 * map the gpu_cc registers for debugging purposes
	 */
	device->gmu_core.reg_virt = devm_ioremap(&gmu->pdev->dev, res->start,
		resource_size(res));

	if (!device->gmu_core.reg_virt) {
		dev_err(&gmu->pdev->dev, "Unable to map the GMU registers\n");
		return -ENOMEM;
	}

	return 0;
}

static int a6xx_gmu_tcm_init(struct a6xx_gmu_device *gmu)
{
	struct gmu_memdesc *md;

	/* Reserve a memdesc for ITCM. No actually memory allocated */
	md = a6xx_gmu_kmem_allocate(gmu, GMU_ITCM, gmu->vma[GMU_ITCM].start,
			gmu->vma[GMU_ITCM].size, 0);
	if (IS_ERR(md))
		return PTR_ERR(md);

	/* Reserve a memdesc for DTCM. No actually memory allocated */
	md = a6xx_gmu_kmem_allocate(gmu, GMU_DTCM, gmu->vma[GMU_DTCM].start,
			gmu->vma[GMU_DTCM].size, 0);

	return PTR_ERR_OR_ZERO(md);
}


static int a6xx_gmu_iommu_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long addr, int flags, void *token,
		const char *name)
{
	char *fault_type = "unknown";

	if (flags & IOMMU_FAULT_TRANSLATION)
		fault_type = "translation";
	else if (flags & IOMMU_FAULT_PERMISSION)
		fault_type = "permission";
	else if (flags & IOMMU_FAULT_EXTERNAL)
		fault_type = "external";
	else if (flags & IOMMU_FAULT_TRANSACTION_STALLED)
		fault_type = "transaction stalled";

	dev_err(dev, "GMU fault addr = %lX, context=%s (%s %s fault)\n",
			addr, name,
			(flags & IOMMU_FAULT_WRITE) ? "write" : "read",
			fault_type);

	return 0;
}

static int a6xx_gmu_kernel_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long addr, int flags, void *token)
{
	return a6xx_gmu_iommu_fault_handler(domain, dev, addr, flags, token,
		"gmu_kernel");
}

static int a6xx_gmu_user_fault_handler(struct iommu_domain *domain,
		struct device *dev, unsigned long addr, int flags, void *token)
{
	return a6xx_gmu_iommu_fault_handler(domain, dev, addr, flags, token,
		"gmu_user");
}
static int a6xx_gmu_iommu_cb_probe(struct a6xx_gmu_device *gmu,
		const char *name, struct gmu_iommu_context *ctx,
		struct device_node *parent, iommu_fault_handler_t handler)
{
	struct device_node *node = of_get_child_by_name(parent, name);
	struct platform_device *pdev;
	int ret;

	if (!node)
		return -ENODEV;

	pdev = of_find_device_by_node(node);
	ret = of_dma_configure(&pdev->dev, node, true);
	of_node_put(node);

	if (ret) {
		platform_device_put(pdev);
		return ret;
	}

	ctx->pdev = pdev;
	ctx->domain = iommu_domain_alloc(&platform_bus_type);
	if (ctx->domain == NULL) {
		dev_err(&gmu->pdev->dev, "gmu iommu fail to alloc %s domain\n",
			ctx->name);
		platform_device_put(pdev);
		return -ENODEV;
	}

	ret = iommu_attach_device(ctx->domain, &pdev->dev);
	if (!ret) {
		iommu_set_fault_handler(ctx->domain, handler, ctx);
		return 0;
	}

	dev_err(&gmu->pdev->dev,
		"gmu iommu fail to attach %s device\n", ctx->name);
	iommu_domain_free(ctx->domain);
	ctx->domain = NULL;
	platform_device_put(pdev);

	return ret;
}

static void a6xx_gmu_iommu_cb_close(struct gmu_iommu_context *ctx)
{
	if (!ctx->domain)
		return;

	iommu_detach_device(ctx->domain, &ctx->pdev->dev);
	iommu_domain_free(ctx->domain);

	platform_device_put(ctx->pdev);
	ctx->domain = NULL;
}

static int a6xx_gmu_iommu_init(struct a6xx_gmu_device *gmu,
		struct device_node *node)
{
	int ret;

	devm_of_platform_populate(&gmu->pdev->dev);

	ret = a6xx_gmu_iommu_cb_probe(gmu, "gmu_user",
		&a6xx_gmu_ctx[GMU_CONTEXT_USER], node,
			a6xx_gmu_user_fault_handler);
	if (ret)
		return ret;

	return a6xx_gmu_iommu_cb_probe(gmu, "gmu_kernel",
		&a6xx_gmu_ctx[GMU_CONTEXT_KERNEL], node,
			a6xx_gmu_kernel_fault_handler);
}

static int a6xx_gmu_regulators_probe(struct a6xx_gmu_device *gmu,
		struct platform_device *pdev)
{
	gmu->cx_gdsc = devm_regulator_get(&pdev->dev, "vddcx");
	if (IS_ERR(gmu->cx_gdsc)) {
		if (PTR_ERR(gmu->cx_gdsc) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Couldn't get the vddcx gdsc\n");
		return PTR_ERR(gmu->cx_gdsc);
	}

	gmu->gx_gdsc = devm_regulator_get(&pdev->dev, "vdd");
	if (IS_ERR(gmu->gx_gdsc)) {
		if (PTR_ERR(gmu->gx_gdsc) != -EPROBE_DEFER)
			dev_err(&pdev->dev, "Couldn't get the vdd gdsc\n");
		return PTR_ERR(gmu->gx_gdsc);
	}

	return 0;
}

static void a6xx_gmu_remove(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = A6XX_GMU_DEVICE(device);

	tasklet_kill(&gmu->hfi.tasklet);

	a6xx_gmu_stop(device);

	if (!IS_ERR_OR_NULL(gmu->mailbox.channel))
		mbox_free_channel(gmu->mailbox.channel);

	adreno_dev->acd_enabled = false;

	if (gmu->fw_image)
		release_firmware(gmu->fw_image);

	a6xx_gmu_memory_close(gmu);

	memset(&device->gmu_core, 0, sizeof(device->gmu_core));
}

static int a6xx_gmu_probe(struct kgsl_device *device,
		struct platform_device *pdev)
{
	struct a6xx_gmu_device *gmu;
	struct a6xx_hfi *hfi;
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int i = 0, ret = -ENXIO, index;

	gmu = devm_kzalloc(&pdev->dev, sizeof(*gmu), GFP_KERNEL);
	if (!gmu)
		return -ENOMEM;

	gmu->pdev = pdev;

	device->gmu_core.ptr = gmu;
	hfi = &gmu->hfi;

	dma_set_coherent_mask(&gmu->pdev->dev, DMA_BIT_MASK(64));
	gmu->pdev->dev.dma_mask = &gmu->pdev->dev.coherent_dma_mask;
	set_dma_ops(&gmu->pdev->dev, NULL);

	/* Set up GMU regulators */
	ret = a6xx_gmu_regulators_probe(gmu, pdev);
	if (ret)
		return ret;

	ret = devm_clk_bulk_get_all(&pdev->dev, &gmu->clks);
	if (ret < 0)
		return ret;

	gmu->num_clks = ret;

	/* Set up GMU IOMMU and shared memory with GMU */
	ret = a6xx_gmu_iommu_init(gmu, pdev->dev.of_node);
	if (ret)
		goto error;

	if (adreno_is_a650_family(adreno_dev))
		gmu->vma = a6xx_gmu_vma;
	else
		gmu->vma = a6xx_gmu_vma_legacy;

	ret = a6xx_gmu_tcm_init(gmu);
	if (ret)
		goto error;

	/* Map and reserve GMU CSRs registers */
	ret = a6xx_gmu_reg_probe(device);
	if (ret)
		goto error;

	/* Initialize HFI and GMU interrupts */
	ret = kgsl_request_irq(gmu->pdev, "kgsl_hfi_irq",
		a6xx_hfi_irq_handler, device);
	if (ret < 0)
		goto error;

	hfi->hfi_interrupt_num = ret;

	ret = kgsl_request_irq(gmu->pdev, "kgsl_gmu_irq", a6xx_gmu_irq_handler,
		device);
	if (ret < 0)
		goto error;

	gmu->gmu_interrupt_num = ret;

	/* Don't enable GMU interrupts until GMU started */
	/* We cannot use irq_disable because it writes registers */
	disable_irq(gmu->gmu_interrupt_num);
	disable_irq(hfi->hfi_interrupt_num);

	tasklet_init(&hfi->tasklet, a6xx_hfi_receiver, (unsigned long) gmu);
	hfi->kgsldev = device;

	if (WARN(pwr->num_pwrlevels + 1 > ARRAY_SIZE(gmu->pwrlevels),
		"Too many GPU powerlevels for the GMU HFI\n")) {
		ret = -EINVAL;
		goto error;
	}

	/* Add a dummy level for "off" because the GMU expects it */
	gmu->pwrlevels[0].freq = 0;
	gmu->pwrlevels[0].level = 0;

	/* GMU power levels are in ascending order */
	for (index = 1, i = pwr->num_pwrlevels - 1; i >= 0; i--, index++) {
		gmu->pwrlevels[index].freq = pwr->pwrlevels[i].gpu_freq;
		gmu->pwrlevels[index].level = pwr->pwrlevels[i].voltage_level;
	}

	gmu->num_gpupwrlevels = pwr->num_pwrlevels + 1;

	/* Populates RPMh configurations */
	ret = a6xx_gmu_rpmh_init(device, gmu);
	if (ret)
		goto error;

	/* Set up GMU idle states */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_MIN_VOLT))
		gmu->idle_level = GPU_HW_MIN_VOLT;
	else if (ADRENO_FEATURE(adreno_dev, ADRENO_HW_NAP))
		gmu->idle_level = GPU_HW_NAP;
	else if (ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		gmu->idle_level = GPU_HW_IFPC;
	else if (ADRENO_FEATURE(adreno_dev, ADRENO_SPTP_PC))
		gmu->idle_level = GPU_HW_SPTP_PC;
	else
		gmu->idle_level = GPU_HW_ACTIVE;

	a6xx_gmu_acd_probe(device, gmu, pdev->dev.of_node);


	if (a6xx_gmu_scales_bandwidth(device))
		pwr->bus_set = a6xx_gmu_bus_set;

	set_bit(GMU_ENABLED, &device->gmu_core.flags);

	device->gmu_core.core_ops = &a6xx_gmu_ops;
	device->gmu_core.dev_ops = &a6xx_gmudev;

	return 0;

error:
	a6xx_gmu_remove(device);
	return ret;
}



static int a6xx_gmu_bind(struct device *dev, struct device *master, void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);

	return a6xx_gmu_probe(device, to_platform_device(dev));
}

static void a6xx_gmu_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);

	a6xx_gmu_remove(device);
}

static const struct component_ops a6xx_gmu_component_ops = {
	.bind = a6xx_gmu_bind,
	.unbind = a6xx_gmu_unbind,
};

static int a6xx_gmu_probe_dev(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &a6xx_gmu_component_ops);
}

static int a6xx_gmu_remove_dev(struct platform_device *pdev)
{
	component_del(&pdev->dev, &a6xx_gmu_component_ops);
	return 0;
}

static const struct of_device_id a6xx_gmu_match_table[] = {
	{ .compatible = "qcom,gpu-gmu" },
	{ },
};

struct platform_driver a6xx_gmu_driver = {
	.probe = a6xx_gmu_probe_dev,
	.remove = a6xx_gmu_remove_dev,
	.driver = {
		.name = "adreno-a6xx-gmu",
		.of_match_table = a6xx_gmu_match_table,
	},
};
