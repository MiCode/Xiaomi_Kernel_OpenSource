// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

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
#include <linux/soc/qcom/llcc-qcom.h>
#include <soc/qcom/cmd-db.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_hwsched.h"
#include "kgsl_bus.h"
#include "kgsl_device.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

struct gmu_iommu_context {
	const char *name;
	struct platform_device *pdev;
	struct iommu_domain *domain;
};

#define ARC_VOTE_GET_PRI(_v) ((_v) & 0xFF)
#define ARC_VOTE_GET_SEC(_v) (((_v) >> 8) & 0xFF)
#define ARC_VOTE_GET_VLVL(_v) (((_v) >> 16) & 0xFFFF)

#define ARC_VOTE_SET(pri, sec, vlvl) \
	((((vlvl) & 0xFFFF) << 16) | (((sec) & 0xFF) << 8) | ((pri) & 0xFF))

static struct gmu_vma_entry a6xx_gmu_vma_legacy[] = {
	[GMU_ITCM] = {
			.start = 0x00000,
			.size = SZ_16K
		},
	[GMU_ICACHE] = {
			.start = 0x04000,
			.size = (SZ_256K - SZ_16K),
			.next_va = 0x4000
		},
	[GMU_DTCM] = {
			.start = 0x40000,
			.size = SZ_16K
		},
	[GMU_DCACHE] = {
			.start = 0x44000,
			.size = (SZ_256K - SZ_16K),
			.next_va = 0x44000
		},
	[GMU_NONCACHED_KERNEL] = {
			.start = 0x60000000,
			.size = SZ_512M,
			.next_va = 0x60000000
		},
	[GMU_NONCACHED_USER] = {
			.start = 0x80000000,
			.size = SZ_1G,
			.next_va = 0x80000000
		},
};

static struct gmu_vma_entry a6xx_gmu_vma[] = {
	[GMU_ITCM] = {
			.start = 0x00000000,
			.size = SZ_16K
		},
	[GMU_CACHE] = {
			.start = SZ_16K,
			.size = (SZ_16M - SZ_16K),
			.next_va = SZ_16K
		},
	[GMU_DTCM] = {
			.start = SZ_256M + SZ_16K,
			.size = SZ_16K
		},
	[GMU_DCACHE] = {
			.start = 0x0,
			.size = 0x0
		},
	[GMU_NONCACHED_KERNEL] = {
			.start = 0x60000000,
			.size = SZ_512M,
			.next_va = 0x60000000
		},
	[GMU_NONCACHED_USER] = {
			.start = 0x80000000,
			.size = SZ_1G,
			.next_va = 0x80000000
		},
};

static struct gmu_iommu_context a6xx_gmu_ctx[] = {
	[GMU_CONTEXT_USER] = { .name = "gmu_user" },
	[GMU_CONTEXT_KERNEL] = { .name = "gmu_kernel" }
};

static int timed_poll_check_rscc(struct kgsl_device *device,
		unsigned int offset, unsigned int expected_ret,
		unsigned int timeout, unsigned int mask)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u32 value;

	if (!adreno_is_a650_family(adreno_dev))
		return timed_poll_check(device, offset + RSCC_OFFSET_LEGACY,
				expected_ret, timeout, mask);

	return readl_poll_timeout(gmu->rscc_virt + (offset << 2), value,
		(value & mask) == expected_ret, 100, timeout * 1000);
}

void gmu_fault_snapshot(struct kgsl_device *device)
{
	device->gmu_fault = true;
	kgsl_device_snapshot(device, NULL, true);
}

struct a6xx_gmu_device *to_a6xx_gmu(struct adreno_device *adreno_dev)
{
	struct a6xx_device *a6xx_dev = container_of(adreno_dev,
					struct a6xx_device, adreno_dev);

	return &a6xx_dev->gmu;
}

struct adreno_device *a6xx_gmu_to_adreno(struct a6xx_gmu_device *gmu)
{
	struct a6xx_device *a6xx_dev =
			container_of(gmu, struct a6xx_device, gmu);

	return &a6xx_dev->adreno_dev;
}

#define RSC_CMD_OFFSET 2
#define PDC_CMD_OFFSET 4

static void _regwrite(void __iomem *regbase,
		unsigned int offsetwords, unsigned int value)
{
	void __iomem *reg;

	reg = regbase + (offsetwords << 2);
	__raw_writel(value, reg);
}

void a6xx_load_rsc_ucode(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	void __iomem *rscc;

	if (adreno_is_a650_family(adreno_dev))
		rscc = gmu->rscc_virt;
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

int a6xx_load_pdc_ucode(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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
 * @adreno_dev: Pointer to adreno device
 */
static void a6xx_gmu_power_config(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	/* Configure registers for idle setting. The setting is cumulative */

	/* Disable GMU WB/RB buffer and caches at boot */
	gmu_core_regwrite(device, A6XX_GMU_SYS_BUS_CONFIG, 0x1);
	gmu_core_regwrite(device, A6XX_GMU_ICACHE_CONFIG, 0x1);
	gmu_core_regwrite(device, A6XX_GMU_DCACHE_CONFIG, 0x1);

	gmu_core_regwrite(device,
		A6XX_GMU_PWR_COL_INTER_FRAME_CTRL,  0x9C40400);

	switch (gmu->idle_level) {
	case GPU_HW_MIN_VOLT:
		gmu_core_regrmw(device, A6XX_GMU_RPMH_CTRL, MIN_BW_ENABLE_MASK,
				MIN_BW_ENABLE_MASK);
		gmu_core_regrmw(device, A6XX_GMU_RPMH_HYST_CTRL, 0xFFFF,
				MIN_BW_HYST);
		/* fall through */
	case GPU_HW_NAP:
		gmu_core_regrmw(device, A6XX_GMU_GPU_NAP_CTRL,
				HW_NAP_ENABLE_MASK, HW_NAP_ENABLE_MASK);
		/* fall through */
	case GPU_HW_IFPC:
		gmu_core_regwrite(device, A6XX_GMU_PWR_COL_INTER_FRAME_HYST,
				GMU_PWR_COL_HYST);
		gmu_core_regrmw(device, A6XX_GMU_PWR_COL_INTER_FRAME_CTRL,
				IFPC_ENABLE_MASK, IFPC_ENABLE_MASK);
		/* fall through */
	case GPU_HW_SPTP_PC:
		gmu_core_regwrite(device, A6XX_GMU_PWR_COL_SPTPRAC_HYST,
				GMU_PWR_COL_HYST);
		gmu_core_regrmw(device, A6XX_GMU_PWR_COL_INTER_FRAME_CTRL,
				SPTP_ENABLE_MASK, SPTP_ENABLE_MASK);
		/* fall through */
	default:
		break;
	}

	/* Enable RPMh GPU client */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_RPMH))
		gmu_core_regrmw(device, A6XX_GMU_RPMH_CTRL, RPMH_ENABLE_MASK,
				RPMH_ENABLE_MASK);
}

int a6xx_gmu_device_start(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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

		dev_err(&gmu->pdev->dev, "GMU doesn't boot\n");
		gmu_fault_snapshot(device);
		return -ETIMEDOUT;
	}

	return 0;
}

/*
 * a6xx_gmu_hfi_start() - Write registers and start HFI.
 * @device: Pointer to KGSL device
 */
int a6xx_gmu_hfi_start(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	gmu_core_regwrite(device, A6XX_GMU_HFI_CTRL_INIT, 1);

	if (timed_poll_check(device,
			A6XX_GMU_HFI_CTRL_STATUS,
			BIT(0),
			GMU_START_TIMEOUT,
			BIT(0))) {
		dev_err(&gmu->pdev->dev, "GMU HFI init failed\n");
		gmu_fault_snapshot(device);
		return -ETIMEDOUT;
	}

	return 0;
}

int a6xx_rscc_wakeup_sequence(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct device *dev = &gmu->pdev->dev;
	int val;

	/* Skip wakeup sequence if we didn't do the sleep sequence */
	if (!test_bit(GMU_PRIV_RSCC_SLEEP_DONE, &gmu->flags))
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
		return -ETIMEDOUT;
	}

	if (timed_poll_check_rscc(device,
			A6XX_RSCC_SEQ_BUSY_DRV0,
			0,
			GPU_START_TIMEOUT,
			0xFFFFFFFF))
		goto error_rsc;

	gmu_core_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, 0);

	clear_bit(GMU_PRIV_RSCC_SLEEP_DONE, &gmu->flags);

	return 0;

error_rsc:
	dev_err(dev, "GPU RSC sequence stuck in waking up GPU\n");
	return -ETIMEDOUT;
}

int a6xx_rscc_sleep_sequence(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	if (!test_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags))
		return 0;

	if (test_bit(GMU_PRIV_RSCC_SLEEP_DONE, &gmu->flags))
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

	set_bit(GMU_PRIV_RSCC_SLEEP_DONE, &gmu->flags);

	return 0;
}

static struct gmu_memdesc *find_gmu_memdesc(struct a6xx_gmu_device *gmu,
	u32 addr, u32 size)
{
	int i;

	for (i = 0; i < gmu->global_entries; i++) {
		struct gmu_memdesc *md = &gmu->gmu_globals[i];

		if ((addr >= md->gmuaddr) &&
		(((addr + size) <= (md->gmuaddr + md->size))))
			return md;
	}

	return NULL;
}

static int find_vma_block(struct a6xx_gmu_device *gmu, u32 addr, u32 size)
{
	int i;

	for (i = 0; i < GMU_MEM_TYPE_MAX; i++) {
		struct gmu_vma_entry *vma = &gmu->vma[i];

		if ((addr >= vma->start) &&
			((addr + size) <= (vma->start + vma->size)))
			return i;
	}

	return -ENOENT;
}

struct iommu_domain *a6xx_get_gmu_domain(struct a6xx_gmu_device *gmu,
	u32 gmuaddr, u32 size)
{
	u32 vma_id = find_vma_block(gmu, gmuaddr, size);

	if (vma_id == GMU_NONCACHED_USER)
		return a6xx_gmu_ctx[GMU_CONTEXT_USER].domain;

	return a6xx_gmu_ctx[GMU_CONTEXT_KERNEL].domain;
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

static void load_tcm(struct adreno_device *adreno_dev, const u8 *src,
	u32 tcm_start, u32 base, const struct gmu_block_header *blk)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 tcm_offset = tcm_start + ((blk->addr - base)/sizeof(u32));
	void __iomem *addr = device->gmu_core.reg_virt +
		((tcm_offset - device->gmu_core.gmu2gpu_offset) << 2);

	memcpy_toio(addr, src, blk->size);
}

int a6xx_gmu_load_fw(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	const u8 *fw = (const u8 *)gmu->fw_image->data;

	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev))
		return _load_legacy_gmu_fw(KGSL_DEVICE(adreno_dev), gmu);

	while (fw < gmu->fw_image->data + gmu->fw_image->size) {
		const struct gmu_block_header *blk  =
					(const struct gmu_block_header *)fw;
		int id;

		fw += sizeof(*blk);

		/* Don't deal with zero size blocks */
		if (blk->size == 0)
			continue;

		id = find_vma_block(gmu, blk->addr, blk->size);

		if (id < 0) {
			dev_err(&gmu->pdev->dev,
				"Unknown block in GMU FW addr:0x%x size:0x%x\n",
				blk->addr, blk->size);
			return -EINVAL;
		}

		if (id == GMU_ITCM) {
			load_tcm(adreno_dev, fw,
				A6XX_GMU_CM3_ITCM_START,
				gmu->vma[GMU_ITCM].start, blk);
		} else if (id == GMU_DTCM) {
			load_tcm(adreno_dev, fw,
				A6XX_GMU_CM3_DTCM_START,
				gmu->vma[GMU_DTCM].start, blk);
		} else {
			struct gmu_memdesc *md =
				find_gmu_memdesc(gmu, blk->addr, blk->size);

			if (!md) {
				dev_err(&gmu->pdev->dev,
					"No backing memory for GMU FW block addr:0x%x size:0x%x\n",
					blk->addr, blk->size);
				return -EINVAL;
			}

			memcpy(md->hostptr + (blk->addr - md->gmuaddr), fw,
				blk->size);
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

static void trigger_reset_recovery(struct adreno_device *adreno_dev,
	enum oob_request req)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/*
	 * Trigger recovery for perfcounter oob only since only
	 * perfcounter oob can happen alongside an actively rendering gpu.
	 */
	if (req != oob_perfcntr)
		return;

	if (test_bit(GMU_DISPATCH, &device->gmu_core.flags)) {
		adreno_get_gpu_halt(adreno_dev);

		adreno_hwsched_set_fault(adreno_dev);
	} else {
		adreno_set_gpu_fault(adreno_dev,
			ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
		adreno_dispatcher_schedule(device);
	}
}

int a6xx_gmu_oob_set(struct kgsl_device *device,
		enum oob_request req)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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
		gmu_fault_snapshot(device);
		ret = -ETIMEDOUT;
		WARN(1, "OOB request %s timed out\n", oob_to_str(req));
		trigger_reset_recovery(adreno_dev, req);
	}

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, check);

	trace_kgsl_gmu_oob_set(set);
	return ret;
}

void a6xx_gmu_oob_clear(struct kgsl_device *device,
		enum oob_request req)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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

void a6xx_gmu_irq_enable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hfi *hfi = &gmu->hfi;

	/* Clear pending IRQs and Unmask needed IRQs */
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, 0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_CLR, 0xffffffff);

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK,
		(unsigned int)~HFI_IRQ_MASK);
	gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_MASK,
		(unsigned int)~GMU_AO_INT_MASK);


	/* Enable all IRQs on host */
	enable_irq(hfi->irq);
	enable_irq(gmu->irq);
}

void a6xx_gmu_irq_disable(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct a6xx_hfi *hfi = &gmu->hfi;

	/* Disable all IRQs on host */
	disable_irq(gmu->irq);
	disable_irq(hfi->irq);

	/* Mask all IRQs and clear pending IRQs */
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK, 0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_MASK, 0xffffffff);

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, 0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_CLR, 0xffffffff);

}

static int a6xx_gmu_hfi_start_msg(struct adreno_device *adreno_dev)
{
	struct hfi_start_cmd req;

	/*
	 * This HFI was not supported in legacy firmware and this quirk
	 * serves as a better means to identify targets that depend on
	 * legacy firmware.
	 */
	if (!ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		CMD_MSG_HDR(req, H2F_MSG_START);

		return a6xx_hfi_send_generic_req(adreno_dev, &req);
	}

	return 0;

}

#define FREQ_VOTE(idx, ack) (((idx) & 0xFF) | (((ack) & 0xF) << 28))
#define BW_VOTE(idx) ((((idx) & 0xFFF) << 12) | ((idx) & 0xFFF))

#define CLKSET_OPTION_ATLEAST 3

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

static int a6xx_complete_rpmh_votes(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
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
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	if (adreno_is_a619_holi(adreno_dev)) {
		u32 val;
		void __iomem *addr = adreno_dev->gmu_wrapper_virt +
				(A6XX_GMU_SPTPRAC_PWR_CLK_STATUS << 2) -
				adreno_dev->gmu_wrapper_base;

		if (!adreno_dev->gmu_wrapper_virt) {
			dev_err(device->dev,
				"invalid gmu_wrapper addr, power on SPTPRAC fail\n");
			return -EINVAL;
		}

		adreno_write_gmu_wrapper(adreno_dev,
			A6XX_GMU_GX_SPTPRAC_POWER_CONTROL,
			SPTPRAC_POWERON_CTRL_MASK);

		if (readl_poll_timeout(addr, val,
			(val & SPTPRAC_POWERON_STATUS_MASK) ==
			SPTPRAC_POWERON_STATUS_MASK, 10,
			10 * 1000)) {
			dev_err(device->dev, "power on SPTPRAC fail\n");
			return -EINVAL;
		}
		return 0;
	}

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
		gmu_fault_snapshot(device);
		return -ETIMEDOUT;
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
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	if (adreno_is_a619_holi(adreno_dev)) {
		u32 val;
		void __iomem *addr = adreno_dev->gmu_wrapper_virt +
				(A6XX_GMU_SPTPRAC_PWR_CLK_STATUS << 2) -
				adreno_dev->gmu_wrapper_base;

		if (!adreno_dev->gmu_wrapper_virt) {
			dev_err(device->dev,
				"invalid gmu_wrapper addr, power off SPTPRAC fail\n");
			return;
		}

		/* Ensure that retention is on */
		adreno_read_gmu_wrapper(adreno_dev,
			A6XX_GPU_CC_GX_GDSCR, &val);
		adreno_write_gmu_wrapper(adreno_dev,
			A6XX_GPU_CC_GX_GDSCR,
			(val | A6XX_RETAIN_FF_ENABLE_ENABLE_MASK));
		adreno_write_gmu_wrapper(adreno_dev,
			A6XX_GMU_GX_SPTPRAC_POWER_CONTROL,
			SPTPRAC_POWEROFF_CTRL_MASK);
		if (readl_poll_timeout(addr, val,
			(val & SPTPRAC_POWEROFF_STATUS_MASK) ==
			SPTPRAC_POWEROFF_STATUS_MASK, 10,
			10 * 1000))
			dev_err(device->dev, "power off SPTPRAC fail\n");
		return;
	}

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

bool a6xx_gmu_gx_is_on(struct kgsl_device *device)
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

	if (gmu_core_isenabled(device))
		gmu_core_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS,
			&val);
	else if (adreno_is_a619_holi(adreno_dev))
		adreno_read_gmu_wrapper(adreno_dev,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &val);
	else
		return true;

	return !(val & (SPTPRAC_POWER_OFF | SP_CLK_OFF));
}

/*
 * a6xx_gmu_gfx_rail_on() - request GMU to power GPU at given OPP.
 * @device: Pointer to KGSL device
 *
 */
static int a6xx_gmu_gfx_rail_on(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u32 perf_idx = gmu->hfi.dcvs_table.gpu_level_num -
		pwr->default_pwrlevel - 1;
	u32 default_opp = gmu->hfi.dcvs_table.gx_votes[perf_idx].vote;

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

int a6xx_gmu_wait_for_lowest_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	unsigned int reg, reg1, reg2, reg3, reg4, reg5, reg6, reg7, reg8;
	unsigned long t;
	uint64_t ts1, ts2, ts3;

	ts1 = a6xx_read_alwayson(adreno_dev);

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

	ts2 = a6xx_read_alwayson(adreno_dev);
	/* Check one last time */

	gmu_core_regread(device, A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
	gmu_core_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg1);

	if (idle_trandition_complete(gmu->idle_level, reg, reg1))
		return 0;

	ts3 = a6xx_read_alwayson(adreno_dev);

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
	gmu_fault_snapshot(device);
	return -ETIMEDOUT;
}

/* Bitmask for GPU idle status check */
#define CXGXCPUBUSYIGNAHB	BIT(30)
int a6xx_gmu_wait_for_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	unsigned int status2;
	uint64_t ts1;

	ts1 = a6xx_read_alwayson(adreno_dev);
	if (timed_poll_check(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS,
			0, GMU_START_TIMEOUT, CXGXCPUBUSYIGNAHB)) {
		gmu_core_regread(device,
				A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS2, &status2);
		dev_err(&gmu->pdev->dev,
				"GMU not idling: status2=0x%x %llx %llx\n",
				status2, ts1,
				a6xx_read_alwayson(ADRENO_DEVICE(device)));
		gmu_fault_snapshot(device);
		return -ETIMEDOUT;
	}

	return 0;
}

/* A6xx GMU FENCE RANGE MASK */
#define GMU_FENCE_RANGE_MASK	((0x1 << 31) | ((0xA << 2) << 18) | (0x8A0))

void a6xx_gmu_version_info(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

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

int a6xx_gmu_itcm_shadow(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	u32 i, *dest;

	if (gmu->itcm_shadow)
		return 0;

	gmu->itcm_shadow = vzalloc(gmu->vma[GMU_ITCM].size);
	if (!gmu->itcm_shadow)
		return -ENOMEM;

	dest = (u32 *)gmu->itcm_shadow;

	for (i = 0; i < (gmu->vma[GMU_ITCM].size >> 2); i++)
		gmu_core_regread(KGSL_DEVICE(adreno_dev),
			A6XX_GMU_CM3_ITCM_START + i, dest++);

	return 0;
}

static void a6xx_gmu_enable_lm(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
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

void a6xx_gmu_register_config(struct adreno_device *adreno_dev)
{
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 gmu_log_info, chipid = 0;

	/* Vote veto for FAL10 feature if supported*/
	if (a6xx_core->veto_fal10) {
		gmu_core_regwrite(device, A6XX_GPU_GMU_CX_GMU_CX_FAL_INTF, 0x1);
		gmu_core_regwrite(device,
			A6XX_GPU_GMU_CX_GMU_CX_FALNEXT_INTF, 0x1);
	}

	/* Turn on TCM retention */
	gmu_core_regwrite(device, A6XX_GMU_GENERAL_7, 1);

	/* Clear init result to make sure we are getting fresh value */
	gmu_core_regwrite(device, A6XX_GMU_CM3_FW_INIT_RESULT, 0);
	gmu_core_regwrite(device, A6XX_GMU_CM3_BOOT_CONFIG, 0x2);

	gmu_core_regwrite(device, A6XX_GMU_HFI_QTBL_ADDR,
			gmu->hfi.hfi_mem->gmuaddr);
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
	a6xx_gmu_power_config(adreno_dev);

	a6xx_gmu_enable_lm(adreno_dev);
}

struct gmu_memdesc *reserve_gmu_kernel_block(struct a6xx_gmu_device *gmu,
	u32 addr, u32 size, u32 vma_id)
{
	int ret;
	struct gmu_memdesc *md;
	struct gmu_vma_entry *vma = &gmu->vma[vma_id];

	if (gmu->global_entries == ARRAY_SIZE(gmu->gmu_globals))
		return ERR_PTR(-ENOMEM);

	md = &gmu->gmu_globals[gmu->global_entries];

	md->size = PAGE_ALIGN(size);

	md->hostptr = dma_alloc_attrs(&gmu->pdev->dev, (size_t)md->size,
		&md->physaddr, GFP_KERNEL, 0);

	if (md->hostptr == NULL)
		return ERR_PTR(-ENOMEM);

	memset(md->hostptr, 0x0, size);

	if (!addr)
		addr = vma->next_va;

	md->gmuaddr = addr;

	ret = iommu_map(a6xx_get_gmu_domain(gmu, md->gmuaddr, md->size), addr,
		md->physaddr, md->size, IOMMU_READ | IOMMU_WRITE | IOMMU_PRIV);
	if (ret) {
		dev_err(&gmu->pdev->dev,
			"Unable to map GMU kernel block: addr:0x%08x size:0x%x :%d\n",
			md->gmuaddr, md->size, ret);
		dma_free_attrs(&gmu->pdev->dev, (size_t)size,
			(void *)md->hostptr, md->physaddr, 0);
		memset(md, 0, sizeof(*md));
		return ERR_PTR(ret);
	}

	vma->next_va = md->gmuaddr + md->size;

	gmu->global_entries++;

	return md;
}

static int reserve_entire_vma(struct a6xx_gmu_device *gmu, u32 vma_id)
{
	struct gmu_memdesc *md;
	u32 start = gmu->vma[vma_id].start, size = gmu->vma[vma_id].size;

	md = find_gmu_memdesc(gmu, start, size);
	if (md)
		return 0;

	md = reserve_gmu_kernel_block(gmu, start, size, vma_id);

	return PTR_ERR_OR_ZERO(md);
}

static int a6xx_gmu_cache_finalize(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct gmu_memdesc *md;
	int ret;

	/* Preallocations were made so no need to request all this memory */
	if (gmu->preallocations)
		return 0;

	ret = reserve_entire_vma(gmu, GMU_ICACHE);
	if (ret)
		return ret;

	if (!adreno_is_a650_family(adreno_dev)) {
		ret = reserve_entire_vma(gmu, GMU_DCACHE);
		if (ret)
			return ret;
	}

	md = reserve_gmu_kernel_block(gmu, 0, SZ_4K, GMU_NONCACHED_KERNEL);
	if (IS_ERR(md))
		return PTR_ERR(md);

	gmu->preallocations = true;

	return 0;
}

static int a6xx_gmu_process_prealloc(struct a6xx_gmu_device *gmu,
	struct gmu_block_header *blk)
{
	struct gmu_memdesc *md;

	int id = find_vma_block(gmu, blk->addr, blk->value);

	if (id < 0) {
		dev_err(&gmu->pdev->dev,
			"Invalid prealloc block addr: 0x%x value:%d\n",
			blk->addr, blk->value);
		return id;
	}

	/* Nothing to do for TCM blocks or user uncached */
	if (id == GMU_ITCM || id == GMU_DTCM || id == GMU_NONCACHED_USER)
		return 0;

	/* Check if the block is already allocated */
	md = find_gmu_memdesc(gmu, blk->addr, blk->value);
	if (md != NULL)
		return 0;

	md = reserve_gmu_kernel_block(gmu, blk->addr, blk->value, id);
	if (IS_ERR(md))
		return PTR_ERR(md);

	gmu->preallocations = true;

	return 0;
}

int a6xx_gmu_parse_fw(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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
			ret = a6xx_gmu_process_prealloc(gmu, blk);

		if (ret)
			return ret;
	}

	return 0;
}

int a6xx_gmu_memory_init(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	/* Allocates & maps GMU crash dump memory */
	if (adreno_is_a630(adreno_dev) || adreno_is_a615_family(adreno_dev)) {
		if (IS_ERR_OR_NULL(gmu->dump_mem))
			gmu->dump_mem = reserve_gmu_kernel_block(gmu, 0, SZ_16K,
					GMU_NONCACHED_KERNEL);
		if (IS_ERR(gmu->dump_mem))
			return PTR_ERR(gmu->dump_mem);
	}

	/* GMU master log */
	if (IS_ERR_OR_NULL(gmu->gmu_log))
		gmu->gmu_log = reserve_gmu_kernel_block(gmu, 0, SZ_4K,
				GMU_NONCACHED_KERNEL);

	return PTR_ERR_OR_ZERO(gmu->gmu_log);
}

static int a6xx_gmu_init(struct adreno_device *adreno_dev)
{
	int ret;

	ret = a6xx_gmu_parse_fw(adreno_dev);
	if (ret)
		return ret;

	 /* Request any other cache ranges that might be required */
	ret = a6xx_gmu_cache_finalize(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gmu_memory_init(adreno_dev);
	if (ret)
		return ret;

	return a6xx_hfi_init(adreno_dev);
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

static void a6xx_gmu_pwrctrl_suspend(struct adreno_device *adreno_dev)
{
	int ret = 0;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* If SPTP_RAC is on, turn off SPTP_RAC HS */
	a6xx_gmu_sptprac_disable(adreno_dev);

	/* Disconnect GPU from BUS is not needed if CX GDSC goes off later */

	/* Check no outstanding RPMh voting */
	a6xx_complete_rpmh_votes(adreno_dev);

	/* Clear the WRITEDROPPED fields and set fence to allow mode */
	gmu_core_regwrite(device, A6XX_GMU_AHB_FENCE_STATUS_CLR, 0x7);
	gmu_core_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);

	/* Make sure above writes are committed before we proceed to recovery */
	wmb();

	gmu_core_regwrite(device, A6XX_GMU_CM3_SYSRESET, 1);

	if (adreno_has_gbif(adreno_dev)) {
		/* Halt GX traffic */
		if (a6xx_gmu_gx_is_on(device))
			do_gbif_halt(device, A6XX_RBBM_GBIF_HALT,
				A6XX_RBBM_GBIF_HALT_ACK,
				A6XX_GBIF_GX_HALT_MASK,
				"GX");

		/* Halt CX traffic */
		do_gbif_halt(device, A6XX_GBIF_HALT, A6XX_GBIF_HALT_ACK,
			A6XX_GBIF_ARB_HALT_MASK, "CX");
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

			/*
			 * Toggle the loop_en bit, across disabling the gx gdsc,
			 * with a delay of 10 XO cycles before disabling gx
			 * gdsc. This is to prevent CPR measurements from
			 * failing.
			 */
			if (adreno_is_a660(adreno_dev)) {
				gmu_core_regrmw(device, A6XX_GPU_CPR_FSM_CTL,
					1, 0);
				ndelay(520);
			}

			ret = regulator_disable(gmu->gx_gdsc);
			if (ret)
				dev_err(&gmu->pdev->dev,
					"suspend fail: gx disable %d\n", ret);

			if (adreno_is_a660(adreno_dev))
				gmu_core_regrmw(device, A6XX_GPU_CPR_FSM_CTL,
					1, 1);

			if (a6xx_gmu_gx_is_on(device))
				dev_err(&gmu->pdev->dev,
					"gx is stuck on\n");
		}
	}
}

/*
 * a6xx_gmu_notify_slumber() - initiate request to GMU to prepare to slumber
 * @device: Pointer to KGSL device
 */
static int a6xx_gmu_notify_slumber(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int bus_level = pwr->pwrlevels[pwr->default_pwrlevel].bus_freq;
	int perf_idx = gmu->hfi.dcvs_table.gpu_level_num -
			pwr->default_pwrlevel - 1;
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

		CMD_MSG_HDR(req, H2F_MSG_PREPARE_SLUMBER);

		ret = a6xx_hfi_send_generic_req(adreno_dev, &req);
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
			ret = -ETIMEDOUT;
		}
	}

out:
	/* Make sure the fence is in ALLOW mode */
	gmu_core_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);
	return ret;
}

void a6xx_gmu_suspend(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	a6xx_gmu_irq_disable(adreno_dev);

	a6xx_gmu_pwrctrl_suspend(adreno_dev);

	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CX_GDSC))
		regulator_set_mode(gmu->cx_gdsc, REGULATOR_MODE_IDLE);

	if (!kgsl_regulator_disable_wait(gmu->cx_gdsc, 5000))
		dev_err(&gmu->pdev->dev, "GMU CX gdsc off timeout\n");

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_CX_GDSC))
		regulator_set_mode(gmu->cx_gdsc, REGULATOR_MODE_NORMAL);

	dev_err(&gmu->pdev->dev, "Suspended GMU\n");

	device->state = KGSL_STATE_NONE;
}

static int a6xx_gmu_dcvs_set(struct adreno_device *adreno_dev,
		int gpu_pwrlevel, int bus_level)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct hfi_dcvstable_cmd *table = &gmu->hfi.dcvs_table;
	struct hfi_gx_bw_perf_vote_cmd req = {
		.ack_type = DCVS_ACK_BLOCK,
		.freq = INVALID_DCVS_IDX,
		.bw = INVALID_DCVS_IDX,
	};
	int ret = 0;

	if (!test_bit(GMU_PRIV_HFI_STARTED, &gmu->flags))
		return 0;

	/* Do not set to XO and lower GPU clock vote from GMU */
	if ((gpu_pwrlevel != INVALID_DCVS_IDX) &&
			(gpu_pwrlevel >= table->gpu_level_num - 1))
		return -EINVAL;

	if (gpu_pwrlevel < table->gpu_level_num - 1)
		req.freq = table->gpu_level_num - gpu_pwrlevel - 1;

	if (bus_level < pwr->ddr_table_count && bus_level > 0)
		req.bw = bus_level;

	/* GMU will vote for slumber levels through the sleep sequence */
	if ((req.freq == INVALID_DCVS_IDX) &&
		(req.bw == INVALID_DCVS_IDX)) {
		return 0;
	}

	CMD_MSG_HDR(req, H2F_MSG_GX_BW_PERF_VOTE);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		ret = a6xx_gmu_dcvs_nohfi(device, req.freq, req.bw);
	else
		ret = a6xx_hfi_send_generic_req(adreno_dev, &req);

	if (ret) {
		dev_err_ratelimited(&gmu->pdev->dev,
			"Failed to set GPU perf idx %d, bw idx %d\n",
			req.freq, req.bw);

		/*
		 * If this was a dcvs request along side an active gpu, request
		 * dispatcher based reset and recovery.
		 */
		if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags)) {
			adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT |
				ADRENO_GMU_FAULT_SKIP_SNAPSHOT);
			adreno_dispatcher_schedule(device);
		}
	}

	return ret;
}

static int a6xx_gmu_clock_set(struct adreno_device *adreno_dev, u32 pwrlevel)
{
	return a6xx_gmu_dcvs_set(adreno_dev, pwrlevel, INVALID_DCVS_IDX);
}

static int a6xx_gmu_ifpc_store(struct kgsl_device *device,
		unsigned int val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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

	/* Power down the GPU before changing the idle level */
	return adreno_power_cycle_u32(adreno_dev, &gmu->idle_level,
		requested_idle_level);
}

static unsigned int a6xx_gmu_ifpc_show(struct kgsl_device *device)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(ADRENO_DEVICE(device));

	return gmu->idle_level >= GPU_HW_IFPC;
}

/* Send an NMI to the GMU */
static void a6xx_gmu_send_nmi(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	u32 val;

	/* Mask so there's no interrupt caused by NMI */
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK, 0xFFFFFFFF);

	/* Make sure the interrupt is masked before causing it */
	wmb();
	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		gmu_core_regwrite(device,
			A6XX_GMU_NMI_CONTROL_STATUS, 0);

	/* This will cause the GMU to save it's internal state to ddr */
	gmu_core_regread(device, A6XX_GMU_CM3_CFG, &val);
	val |=  BIT(9);
	gmu_core_regwrite(device, A6XX_GMU_CM3_CFG, val);

	/* Make sure the NMI is invoked before we proceed*/
	wmb();
}

static void a6xx_gmu_cooperative_reset(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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
	a6xx_gmu_send_nmi(adreno_dev);
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
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(ADRENO_DEVICE(device));

	if (!gmu_core_isenabled(device))
		return 0;

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

static irqreturn_t a6xx_gmu_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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
			a6xx_gmu_send_nmi(adreno_dev);

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

static void a6xx_gmu_nmi(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	/* No need to nmi if it was a gpu fault */
	if (!device->gmu_fault)
		return;

	/* make sure we're reading the latest cm3_fault */
	smp_rmb();

	/*
	 * We should not send NMI if there was a CM3 fault reported because we
	 * don't want to overwrite the critical CM3 state captured by gmu before
	 * it sent the CM3 fault interrupt.
	 */
	if (!atomic_read(&gmu->cm3_fault)) {
		a6xx_gmu_send_nmi(adreno_dev);

		/* Wait for the NMI to be handled */
		udelay(100);
	}
}

void a6xx_gmu_snapshot(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	a6xx_gmu_nmi(adreno_dev);

	a6xx_gmu_device_snapshot(device, snapshot);

	a6xx_snapshot(adreno_dev, snapshot);

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR,
		0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK,
		HFI_IRQ_MASK);

}

void a6xx_gmu_aop_send_acd_state(struct a6xx_gmu_device *gmu, bool flag)
{
	char msg_buf[33];
	int ret;
	struct {
		u32 len;
		void *msg;
	} msg;

	if (IS_ERR_OR_NULL(gmu->mailbox.channel))
		return;

	msg.len = scnprintf(msg_buf, sizeof(msg_buf),
			"{class: gpu, res: acd, value: %d}", flag);
	msg.msg = msg_buf;

	ret = mbox_send_message(gmu->mailbox.channel, &msg);

	if (ret < 0)
		dev_err(&gmu->pdev->dev,
			"AOP mbox send message failed: %d\n", ret);
}

int a6xx_gmu_enable_gdsc(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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

int a6xx_gmu_enable_clks(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	ret = a6xx_gmu_clk_set_rate(gmu, "gmu_clk", GMU_FREQUENCY);
	if (ret) {
		dev_err(&gmu->pdev->dev, "Unable to set the GMU clock\n");
		return ret;
	}

	ret = a6xx_gmu_clk_set_rate(gmu, "hub_clk", 150000000);
	if (ret && ret != -ENODEV) {
		dev_err(&gmu->pdev->dev, "Unable to set the HUB clock\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(gmu->num_clks, gmu->clks);
	if (ret) {
		dev_err(&gmu->pdev->dev, "Cannot enable GMU clocks\n");
		return ret;
	}

	device->state = KGSL_STATE_AWARE;

	return 0;
}

static int a6xx_gmu_first_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int level, ret;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_AWARE);

	a6xx_gmu_aop_send_acd_state(gmu, adreno_dev->acd_enabled);

	ret = a6xx_gmu_enable_gdsc(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gmu_enable_clks(adreno_dev);
	if (ret)
		goto gdsc_off;

	ret = a6xx_gmu_load_fw(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	ret = a6xx_gmu_itcm_shadow(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	a6xx_gmu_register_config(adreno_dev);

	a6xx_gmu_version_info(adreno_dev);

	a6xx_gmu_irq_enable(adreno_dev);

	/* Vote for minimal DDR BW for GMU to init */
	level = pwr->pwrlevels[pwr->default_pwrlevel].bus_min;
	icc_set_bw(pwr->icc_path, 0, kBps_to_icc(pwr->ddr_table[level]));

	ret = a6xx_gmu_device_start(adreno_dev);
	if (ret)
		goto err;

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		ret = a6xx_gmu_gfx_rail_on(adreno_dev);
		if (ret) {
			a6xx_gmu_oob_clear(device, oob_boot_slumber);
			goto err;
		}
	}

	if (gmu->idle_level < GPU_HW_SPTP_PC) {
		ret = a6xx_gmu_sptprac_enable(adreno_dev);
		if (ret)
			goto err;
	}

	if (!test_bit(GMU_PRIV_PDC_RSC_LOADED, &gmu->flags)) {
		ret = a6xx_load_pdc_ucode(adreno_dev);
		if (ret)
			goto err;

		a6xx_load_rsc_ucode(adreno_dev);
		set_bit(GMU_PRIV_PDC_RSC_LOADED, &gmu->flags);
	}

	ret = a6xx_gmu_hfi_start(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_start(adreno_dev);
	if (ret)
		goto err;

	icc_set_bw(pwr->icc_path, 0, 0);

	device->gmu_fault = false;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_AWARE);

	return 0;

err:
	if (device->gmu_fault) {
		a6xx_gmu_suspend(adreno_dev);
		return ret;
	}

clks_gdsc_off:
	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

gdsc_off:
	/* Pool to make sure that the CX is off */
	if (!kgsl_regulator_disable_wait(gmu->cx_gdsc, 5000))
		dev_err(&gmu->pdev->dev, "GMU CX gdsc off timeout\n");

	return ret;
}

static int a6xx_gmu_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret = 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_AWARE);

	ret = a6xx_gmu_enable_gdsc(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gmu_enable_clks(adreno_dev);
	if (ret)
		goto gdsc_off;

	ret = a6xx_rscc_wakeup_sequence(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	ret = a6xx_gmu_load_fw(adreno_dev);
	if (ret)
		goto clks_gdsc_off;

	a6xx_gmu_register_config(adreno_dev);

	a6xx_gmu_irq_enable(adreno_dev);

	ret = a6xx_gmu_device_start(adreno_dev);
	if (ret)
		goto err;

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		ret = a6xx_gmu_gfx_rail_on(adreno_dev);
		if (ret) {
			a6xx_gmu_oob_clear(device, oob_boot_slumber);
			goto err;
		}
	}

	if (gmu->idle_level < GPU_HW_SPTP_PC) {
		ret = a6xx_gmu_sptprac_enable(adreno_dev);
		if (ret)
			goto err;
	}

	ret = a6xx_gmu_hfi_start(adreno_dev);
	if (ret)
		goto err;

	ret = a6xx_hfi_start(adreno_dev);
	if (ret)
		goto err;

	device->gmu_fault = false;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_AWARE);

	return 0;

err:
	if (device->gmu_fault) {
		a6xx_gmu_suspend(adreno_dev);
		return ret;
	}

clks_gdsc_off:
	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

gdsc_off:
	/* Pool to make sure that the CX is off */
	if (!kgsl_regulator_disable_wait(gmu->cx_gdsc, 5000))
		dev_err(&gmu->pdev->dev, "GMU CX gdsc off timeout\n");

	return ret;
}

static void set_acd(struct adreno_device *adreno_dev, void *priv)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	adreno_dev->acd_enabled = *((bool *)priv);
	a6xx_gmu_aop_send_acd_state(gmu, adreno_dev->acd_enabled);
}

static int a6xx_gmu_acd_set(struct kgsl_device *device, bool val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	if (IS_ERR_OR_NULL(gmu->mailbox.channel))
		return -EINVAL;

	/* Don't do any unneeded work if ACD is already in the correct state */
	if (adreno_dev->acd_enabled == val)
		return 0;

	/* Power cycle the GPU for changes to take effect */
	return adreno_power_cycle(adreno_dev, set_acd, &val);
}

static struct gmu_dev_ops a6xx_gmudev = {
	.oob_set = a6xx_gmu_oob_set,
	.oob_clear = a6xx_gmu_oob_clear,
	.gx_is_on = a6xx_gmu_gx_is_on,
	.ifpc_store = a6xx_gmu_ifpc_store,
	.ifpc_show = a6xx_gmu_ifpc_show,
	.cooperative_reset = a6xx_gmu_cooperative_reset,
	.wait_for_active_transition = a6xx_gmu_wait_for_active_transition,
	.scales_bandwidth = a6xx_gmu_scales_bandwidth,
	.acd_set = a6xx_gmu_acd_set,
};

static int a6xx_gmu_bus_set(struct adreno_device *adreno_dev, int buslevel,
	u32 ab)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;
	int ret = 0;

	if (buslevel != pwr->cur_buslevel) {
		ret = a6xx_gmu_dcvs_set(adreno_dev, INVALID_DCVS_IDX, buslevel);
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

static void a6xx_gmu_iommu_cb_close(struct gmu_iommu_context *ctx)
{
	if (!ctx->domain)
		return;

	iommu_detach_device(ctx->domain, &ctx->pdev->dev);
	iommu_domain_free(ctx->domain);

	platform_device_put(ctx->pdev);
	ctx->domain = NULL;
}

static void a6xx_free_gmu_globals(struct a6xx_gmu_device *gmu)
{
	int i;

	for (i = 0; i < gmu->global_entries; i++) {
		struct gmu_memdesc *md = &gmu->gmu_globals[i];

		if (!md->gmuaddr)
			continue;

		iommu_unmap(a6xx_get_gmu_domain(gmu, md->gmuaddr, md->size),
			md->gmuaddr, md->size);

		dma_free_attrs(&gmu->pdev->dev, (size_t) md->size,
				(void *)md->hostptr, md->physaddr, 0);

		memset(md, 0, sizeof(*md));
	}

	a6xx_gmu_iommu_cb_close(&a6xx_gmu_ctx[GMU_CONTEXT_KERNEL]);
	a6xx_gmu_iommu_cb_close(&a6xx_gmu_ctx[GMU_CONTEXT_USER]);

	gmu->global_entries = 0;
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
	struct kgsl_pwrlevel *pwrlevel =
			&pwr->pwrlevels[pwr->num_pwrlevels - 1];
	struct hfi_acd_table_cmd *cmd = &gmu->hfi.acd_table;
	int ret, i, cmd_idx = 0;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_ACD))
		return;

	cmd->hdr = CREATE_MSG_HDR(H2F_MSG_ACD_TBL, sizeof(*cmd), HFI_MSG_CMD);

	cmd->version = 1;
	cmd->stride = 1;
	cmd->enable_by_level = 0;

	/*
	 * Iterate through each gpu power level and generate a mask for GMU
	 * firmware for ACD enabled levels and store the corresponding control
	 * register configurations to the acd_table structure.
	 */
	for (i = 0; i < pwr->num_pwrlevels; i++) {
		if (pwrlevel->acd_level) {
			cmd->enable_by_level |= (1 << (i + 1));
			cmd->data[cmd_idx++] = pwrlevel->acd_level;
		}
		pwrlevel--;
	}

	if (!cmd->enable_by_level)
		return;

	cmd->num_levels = cmd_idx;

	ret = a6xx_gmu_aop_mailbox_init(adreno_dev, gmu);
	if (ret)
		dev_err(&gmu->pdev->dev,
			"AOP mailbox init failed: %d\n", ret);
}

static int a6xx_gmu_reg_probe(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
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

void a6xx_gmu_remove(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	if (!IS_ERR_OR_NULL(gmu->mailbox.channel))
		mbox_free_channel(gmu->mailbox.channel);

	adreno_dev->acd_enabled = false;

	if (gmu->fw_image)
		release_firmware(gmu->fw_image);

	a6xx_free_gmu_globals(gmu);

	vfree(gmu->itcm_shadow);
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
		struct gmu_iommu_context *ctx, struct device_node *parent,
		iommu_fault_handler_t handler)
{
	struct device_node *node = of_get_child_by_name(parent, ctx->name);
	struct platform_device *pdev;
	int ret;
	int no_stall = 1;

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

	/*
	 * Disable stall on fault for the GMU context bank.
	 * This sets SCTLR.CFCFG = 0.
	 * Also note that, the smmu driver sets SCTLR.HUPCF = 0 by default.
	 */
	iommu_domain_set_attr(ctx->domain,
		DOMAIN_ATTR_FAULT_MODEL_NO_STALL, &no_stall);

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

static int a6xx_gmu_iommu_init(struct a6xx_gmu_device *gmu,
		struct device_node *node)
{
	int ret;

	devm_of_platform_populate(&gmu->pdev->dev);

	ret = a6xx_gmu_iommu_cb_probe(gmu, &a6xx_gmu_ctx[GMU_CONTEXT_USER],
			node, a6xx_gmu_user_fault_handler);
	if (ret)
		return ret;

	return a6xx_gmu_iommu_cb_probe(gmu, &a6xx_gmu_ctx[GMU_CONTEXT_KERNEL],
			node, a6xx_gmu_kernel_fault_handler);
}

int a6xx_gmu_probe(struct kgsl_device *device,
		struct platform_device *pdev)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct resource *res;
	int ret;

	gmu->pdev = pdev;

	dma_set_coherent_mask(&gmu->pdev->dev, DMA_BIT_MASK(64));
	gmu->pdev->dev.dma_mask = &gmu->pdev->dev.coherent_dma_mask;
	set_dma_ops(&gmu->pdev->dev, NULL);

	res = platform_get_resource_byname(device->pdev, IORESOURCE_MEM,
						"rscc");
	if (res) {
		gmu->rscc_virt = devm_ioremap(&device->pdev->dev, res->start,
						resource_size(res));
		if (gmu->rscc_virt == NULL) {
			dev_err(&gmu->pdev->dev, "rscc ioremap failed\n");
			return -ENOMEM;
		}
	}

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

	/* Map and reserve GMU CSRs registers */
	ret = a6xx_gmu_reg_probe(adreno_dev);
	if (ret)
		goto error;

	/* Populates RPMh configurations */
	ret = a6xx_build_rpmh_tables(adreno_dev);
	if (ret)
		goto error;

	/* Set up GMU idle state */
	if (ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		gmu->idle_level = GPU_HW_IFPC;
	else
		gmu->idle_level = GPU_HW_ACTIVE;

	if (ADRENO_FEATURE(adreno_dev, ADRENO_BCL))
		adreno_dev->bcl_enabled = true;

	a6xx_gmu_acd_probe(device, gmu, pdev->dev.of_node);

	set_bit(GMU_ENABLED, &device->gmu_core.flags);

	device->gmu_core.dev_ops = &a6xx_gmudev;

	gmu->irq = kgsl_request_irq(gmu->pdev, "kgsl_gmu_irq",
		a6xx_gmu_irq_handler, device);

	if (gmu->irq < 0) {
		ret = gmu->irq;
		goto error;
	}

	/* Don't enable GMU interrupts until GMU started */
	/* We cannot use irq_disable because it writes registers */
	disable_irq(gmu->irq);

	return 0;

error:
	a6xx_gmu_remove(device);
	return ret;
}

static void a6xx_gmu_active_count_put(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return;

	if (WARN(atomic_read(&device->active_cnt) == 0,
		"Unbalanced get/put calls to KGSL active count\n"))
		return;

	if (atomic_dec_and_test(&device->active_cnt)) {
		kgsl_pwrscale_update_stats(device);
		kgsl_pwrscale_update(device);
		mod_timer(&device->idle_timer,
			jiffies + device->pwrctrl.interval_timeout);
	}

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	wake_up(&device->active_cnt_wq);
}

int a6xx_halt_gbif(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/* Halt new client requests */
	kgsl_regwrite(device, A6XX_GBIF_HALT, A6XX_GBIF_CLIENT_HALT_MASK);
	ret = adreno_wait_for_halt_ack(device,
		ADRENO_REG_GBIF_HALT_ACK,
		A6XX_GBIF_CLIENT_HALT_MASK);

	/* Halt all AXI requests */
	kgsl_regwrite(device, A6XX_GBIF_HALT, A6XX_GBIF_ARB_HALT_MASK);
	ret = adreno_wait_for_halt_ack(device,
		ADRENO_REG_GBIF_HALT_ACK,
		A6XX_GBIF_ARB_HALT_MASK);

	/* De-assert the halts */
	kgsl_regwrite(device, A6XX_GBIF_HALT, 0x0);

	return ret;
}

static int a6xx_gmu_power_off(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = 0;

	if (device->gmu_fault)
		goto error;

	/* Wait for the lowest idle level we requested */
	ret = a6xx_gmu_wait_for_lowest_idle(adreno_dev);
	if (ret)
		goto error;

	ret = a6xx_gmu_notify_slumber(adreno_dev);
	if (ret)
		goto error;

	ret = a6xx_gmu_wait_for_idle(adreno_dev);
	if (ret)
		goto error;

	ret = a6xx_rscc_sleep_sequence(adreno_dev);

	/* Now that we are done with GMU and GPU, Clear the GBIF */
	if (!adreno_is_a630(adreno_dev))
		ret = a6xx_halt_gbif(adreno_dev);

	a6xx_gmu_irq_disable(adreno_dev);

	a6xx_hfi_stop(adreno_dev);

	clk_bulk_disable_unprepare(gmu->num_clks, gmu->clks);

	/* Pool to make sure that the CX is off */
	if (!kgsl_regulator_disable_wait(gmu->cx_gdsc, 5000))
		dev_err(&gmu->pdev->dev, "GMU CX gdsc off timeout\n");

	device->state = KGSL_STATE_NONE;

	return ret;

error:
	a6xx_hfi_stop(adreno_dev);
	a6xx_gmu_suspend(adreno_dev);

	return ret;
}

void a6xx_enable_gpu_irq(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_ON);

	adreno_irqctrl(adreno_dev, 1);
}

void a6xx_disable_gpu_irq(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);

	if (a6xx_gmu_gx_is_on(device))
		adreno_irqctrl(adreno_dev, 0);

}

static int a6xx_gpu_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/* Clear any GPU faults that might have been left over */
	adreno_clear_gpu_fault(adreno_dev);

	adreno_set_active_ctxs_null(adreno_dev);

	adreno_ringbuffer_set_global(adreno_dev, 0);

	ret = kgsl_mmu_start(device);
	if (ret)
		goto err;

	ret = a6xx_gmu_oob_set(device, oob_gpu);
	if (ret)
		goto oob_clear;

	ret = a6xx_gmu_hfi_start_msg(adreno_dev);
	if (ret)
		goto oob_clear;

	adreno_clear_dcvs_counters(adreno_dev);

	/* Restore performance counter registers with saved values */
	adreno_perfcounter_restore(adreno_dev);

	a6xx_start(adreno_dev);

	/* Re-initialize the coresight registers if applicable */
	adreno_coresight_start(adreno_dev);

	adreno_perfcounter_start(adreno_dev);

	/* Clear FSR here in case it is set from a previous pagefault */
	kgsl_mmu_clear_fsr(&device->mmu);

	a6xx_enable_gpu_irq(adreno_dev);

	ret = a6xx_rb_start(adreno_dev);
	if (ret) {
		a6xx_disable_gpu_irq(adreno_dev);
		goto oob_clear;
	}

	/* Start the dispatcher */
	adreno_dispatcher_start(device);

	device->reset_counter++;

	a6xx_gmu_oob_clear(device, oob_gpu);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		gmu_core_dev_oob_clear(device, oob_boot_slumber);

	return 0;

oob_clear:
	a6xx_gmu_oob_clear(device, oob_gpu);

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG))
		gmu_core_dev_oob_clear(device, oob_boot_slumber);

err:
	a6xx_gmu_power_off(adreno_dev);

	return ret;
}

static void gmu_idle_timer(struct timer_list *t)
{
	struct kgsl_device *device = container_of(t, struct kgsl_device,
					idle_timer);

	kgsl_schedule_work(&device->idle_check_ws);
}

static int a6xx_boot(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	WARN_ON(test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags));

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	ret = a6xx_gmu_boot(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gpu_boot(adreno_dev);
	if (ret)
		return ret;

	mod_timer(&device->idle_timer, jiffies +
			device->pwrctrl.interval_timeout);

	kgsl_pwrscale_wake(device);

	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);
	device->state = KGSL_STATE_ACTIVE;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_ACTIVE);

	return ret;
}

static int a6xx_first_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;
	unsigned long priv = 0;

	if (test_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags))
		return a6xx_boot(adreno_dev);

	ret = adreno_dispatcher_init(adreno_dev);
	if (ret)
		return ret;

	ret = adreno_ringbuffer_init(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_microcode_read(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_init(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gmu_init(adreno_dev);
	if (ret)
		return ret;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	ret = a6xx_gmu_first_boot(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gpu_boot(adreno_dev);
	if (ret)
		return ret;

	adreno_get_bus_counters(adreno_dev);

	adreno_dev->cooperative_reset = ADRENO_FEATURE(adreno_dev,
						 ADRENO_COOP_RESET);

	if (ADRENO_FEATURE(adreno_dev, ADRENO_APRIV))
		priv |= KGSL_MEMDESC_PRIVILEGED;

	adreno_dev->profile_buffer = kgsl_allocate_global(device, PAGE_SIZE, 0,
				priv, "alwayson");

	adreno_dev->profile_index = 0;

	if (!IS_ERR(adreno_dev->profile_buffer))
		set_bit(ADRENO_DEVICE_DRAWOBJ_PROFILE, &adreno_dev->priv);

	set_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags);
	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	device->state = KGSL_STATE_ACTIVE;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_ACTIVE);


	return 0;
}

static int a630_vbif_halt(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	kgsl_regwrite(device, A6XX_VBIF_XIN_HALT_CTRL0,
		A6XX_VBIF_XIN_HALT_CTRL0_MASK);
	ret = adreno_wait_for_halt_ack(device,
			ADRENO_REG_VBIF_XIN_HALT_CTRL1,
			A6XX_VBIF_XIN_HALT_CTRL0_MASK);
	kgsl_regwrite(device, A6XX_VBIF_XIN_HALT_CTRL0, 0);

	return ret;
}

static int a6xx_power_off(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	WARN_ON(!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags));

	trace_kgsl_pwr_request_state(device, KGSL_STATE_SLUMBER);

	adreno_suspend_context(device);

	ret = a6xx_gmu_oob_set(device, oob_gpu);
	if (ret) {
		a6xx_gmu_oob_clear(device, oob_gpu);
		goto no_gx_power;
	}

	kgsl_pwrscale_update_stats(device);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	/* Save physical performance counter values before GPU power down*/
	adreno_perfcounter_save(adreno_dev);

	/*
	 * Clear GX halt on non-gbif targets. For targets with GBIF,
	 * GX halt is handled by the GMU FW.
	 */
	if (adreno_is_a630(adreno_dev))
		a630_vbif_halt(adreno_dev);

	adreno_irqctrl(adreno_dev, 0);

	a6xx_gmu_oob_clear(device, oob_gpu);

no_gx_power:
	kgsl_pwrctrl_irq(device, KGSL_PWRFLAGS_OFF);

	a6xx_gmu_power_off(adreno_dev);

	adreno_set_active_ctxs_null(adreno_dev);

	adreno_dispatcher_stop(adreno_dev);

	adreno_ringbuffer_stop(adreno_dev);

	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpu_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpuhtw_llc_slice);

	clear_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	del_timer_sync(&device->idle_timer);

	kgsl_pwrscale_sleep(device);

	trace_kgsl_pwr_set_state(device, KGSL_STATE_SLUMBER);

	return ret;
}

static void gmu_idle_check(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work,
					struct kgsl_device, idle_check_ws);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	mutex_lock(&device->mutex);

	if (test_bit(GMU_DISABLE_SLUMBER, &device->gmu_core.flags))
		goto done;

	if (!atomic_read(&device->active_cnt)) {
		if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
			a6xx_power_off(adreno_dev);
	} else {
		kgsl_pwrscale_update(device);
		mod_timer(&device->idle_timer,
			jiffies + device->pwrctrl.interval_timeout);
	}

done:
	mutex_unlock(&device->mutex);
}

static int a6xx_gmu_first_open(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/*
	 * Do the one time settings that need to happen when we
	 * attempt to boot the gpu the very first time
	 */
	ret = a6xx_first_boot(adreno_dev);
	if (ret)
		return ret;

	/*
	 * A client that does a first_open but never closes the device
	 * may prevent us from going back to SLUMBER. So trigger the idle
	 * check by incrementing the active count and immediately releasing it.
	 */
	atomic_inc(&device->active_cnt);
	a6xx_gmu_active_count_put(adreno_dev);

	return 0;
}

static int a6xx_gmu_last_close(struct adreno_device *adreno_dev)
{
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		return a6xx_power_off(adreno_dev);

	return 0;
}

static int a6xx_gmu_active_count_get(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret = 0;

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return -EINVAL;

	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags))
		return -EINVAL;

	if ((atomic_read(&device->active_cnt) == 0) &&
		!test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		ret = a6xx_boot(adreno_dev);

	if (ret == 0)
		atomic_inc(&device->active_cnt);

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	return ret;
}

static int a6xx_gmu_pm_suspend(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags))
		return 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_SUSPEND);

	/* Halt any new submissions */
	reinit_completion(&device->halt_gate);

	/* wait for active count so device can be put in slumber */
	ret = kgsl_active_count_wait(device, 0, HZ);
	if (ret) {
		dev_err(device->dev,
			"Timed out waiting for the active count\n");
		goto err;
	}

	ret = adreno_idle(device);
	if (ret)
		goto err;

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		a6xx_power_off(adreno_dev);

	set_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags);

	adreno_dispatcher_halt(device);

	trace_kgsl_pwr_set_state(device, KGSL_STATE_SUSPEND);

	return 0;
err:
	adreno_dispatcher_start(device);
	return ret;
}

static void a6xx_gmu_pm_resume(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	if (WARN(!test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags),
		"resume invoked without a suspend\n"))
		return;

	adreno_dispatcher_unhalt(device);

	adreno_dispatcher_start(device);

	clear_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags);
}

static void a6xx_gmu_touch_wakeup(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);
	int ret;

	/*
	 * Do not wake up a suspended device or until the first boot sequence
	 * has been completed.
	 */
	if (test_bit(GMU_PRIV_PM_SUSPEND, &gmu->flags) ||
		!test_bit(GMU_PRIV_FIRST_BOOT_DONE, &gmu->flags))
		return;

	if (test_bit(GMU_PRIV_GPU_STARTED, &gmu->flags))
		goto done;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	ret = a6xx_gmu_boot(adreno_dev);
	if (ret)
		return;

	ret = a6xx_gpu_boot(adreno_dev);
	if (ret)
		return;

	kgsl_pwrscale_wake(device);

	set_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);
	device->state = KGSL_STATE_ACTIVE;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_ACTIVE);

done:
	/*
	 * When waking up from a touch event we want to stay active long enough
	 * for the user to send a draw command.  The default idle timer timeout
	 * is shorter than we want so go ahead and push the idle timer out
	 * further for this special case
	 */
	mod_timer(&device->idle_timer, jiffies +
			msecs_to_jiffies(adreno_wake_timeout));

}

const struct adreno_power_ops a6xx_gmu_power_ops = {
	.first_open = a6xx_gmu_first_open,
	.last_close = a6xx_gmu_last_close,
	.active_count_get = a6xx_gmu_active_count_get,
	.active_count_put = a6xx_gmu_active_count_put,
	.pm_suspend = a6xx_gmu_pm_suspend,
	.pm_resume = a6xx_gmu_pm_resume,
	.touch_wakeup = a6xx_gmu_touch_wakeup,
	.gpu_clock_set = a6xx_gmu_clock_set,
	.gpu_bus_set = a6xx_gmu_bus_set,
};

const struct adreno_power_ops a630_gmu_power_ops = {
	.first_open = a6xx_gmu_first_open,
	.last_close = a6xx_gmu_last_close,
	.active_count_get = a6xx_gmu_active_count_get,
	.active_count_put = a6xx_gmu_active_count_put,
	.pm_suspend = a6xx_gmu_pm_suspend,
	.pm_resume = a6xx_gmu_pm_resume,
	.touch_wakeup = a6xx_gmu_touch_wakeup,
	.gpu_clock_set = a6xx_gmu_clock_set,
};

int a6xx_gmu_device_probe(struct platform_device *pdev,
	u32 chipid, const struct adreno_gpu_core *gpucore)
{
	struct adreno_device *adreno_dev;
	struct kgsl_device *device;
	struct a6xx_device *a6xx_dev;
	int ret;

	a6xx_dev = devm_kzalloc(&pdev->dev, sizeof(*a6xx_dev),
			GFP_KERNEL);
	if (!a6xx_dev)
		return -ENOMEM;

	adreno_dev = &a6xx_dev->adreno_dev;

	ret = a6xx_probe_common(pdev, adreno_dev, chipid, gpucore);
	if (ret)
		return ret;

	device = KGSL_DEVICE(adreno_dev);

	INIT_WORK(&device->idle_check_ws, gmu_idle_check);

	timer_setup(&device->idle_timer, gmu_idle_timer, 0);

	adreno_dev->irq_mask = A6XX_INT_MASK;

	return 0;
}

int a6xx_gmu_restart(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(adreno_dev);

	a6xx_hfi_stop(adreno_dev);

	a6xx_disable_gpu_irq(adreno_dev);

	/* Hard reset the gmu and gpu */
	a6xx_gmu_suspend(adreno_dev);

	clear_bit(GMU_PRIV_GPU_STARTED, &gmu->flags);

	/* Attempt to reboot the gmu and gpu */
	return a6xx_boot(adreno_dev);
}

static int a6xx_gmu_bind(struct device *dev, struct device *master, void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);
	struct a6xx_gmu_device *gmu = to_a6xx_gmu(ADRENO_DEVICE(device));
	struct a6xx_hfi *hfi = &gmu->hfi;
	int ret;

	ret = a6xx_gmu_probe(device, to_platform_device(dev));
	if (ret)
		return ret;

	/*
	 * a6xx_gmu_probe() is also called by hwscheduling probe. However,
	 * since HFI interrupts are handled differently in hwscheduling, move
	 * out HFI interrupt setup from a6xx_gmu_probe().
	 */
	hfi->irq = kgsl_request_irq(gmu->pdev, "kgsl_hfi_irq",
		a6xx_hfi_irq_handler, device);
	if (hfi->irq < 0) {
		a6xx_gmu_remove(device);
		return hfi->irq;
	}

	disable_irq(gmu->hfi.irq);

	return 0;
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
