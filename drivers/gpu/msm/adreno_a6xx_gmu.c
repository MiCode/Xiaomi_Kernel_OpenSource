// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

/* soc/qcom/cmd-db.h needs types.h */
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/regulator/consumer.h>
#include <soc/qcom/cmd-db.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_snapshot.h"
#include "kgsl_gmu.h"
#include "kgsl_trace.h"

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
	0x1F400, 0x1F40B, 0x1F410, 0x1F412, 0x1F500, 0x1F500, 0x1F507, 0x1F50A,
	0x1F800, 0x1F804, 0x1F807, 0x1F808, 0x1F80B, 0x1F80C, 0x1F80F, 0x1F81C,
	0x1F824, 0x1F82A, 0x1F82D, 0x1F830, 0x1F840, 0x1F853, 0x1F887, 0x1F889,
	0x1F8A0, 0x1F8A2, 0x1F8A4, 0x1F8AF, 0x1F8C0, 0x1F8C3, 0x1F8D0, 0x1F8D0,
	0x1F8E4, 0x1F8E4, 0x1F8E8, 0x1F8EC, 0x1F900, 0x1F903, 0x1F940, 0x1F940,
	0x1F942, 0x1F944, 0x1F94C, 0x1F94D, 0x1F94F, 0x1F951, 0x1F954, 0x1F954,
	0x1F957, 0x1F958, 0x1F95D, 0x1F95D, 0x1F962, 0x1F962, 0x1F964, 0x1F965,
	0x1F980, 0x1F986, 0x1F990, 0x1F99E, 0x1F9C0, 0x1F9C0, 0x1F9C5, 0x1F9CC,
	0x1F9E0, 0x1F9E2, 0x1F9F0, 0x1F9F0, 0x1FA00, 0x1FA01,
	/* GMU AO */
	0x23B00, 0x23B16, 0x23C00, 0x23C00,
	/* GPU CC */
	0x24000, 0x24012, 0x24040, 0x24052, 0x24400, 0x24404, 0x24407, 0x2440B,
	0x24415, 0x2441C, 0x2441E, 0x2442D, 0x2443C, 0x2443D, 0x2443F, 0x24440,
	0x24442, 0x24449, 0x24458, 0x2445A, 0x24540, 0x2455E, 0x24800, 0x24802,
	0x24C00, 0x24C02, 0x25400, 0x25402, 0x25800, 0x25802, 0x25C00, 0x25C02,
	0x26000, 0x26002,
	/* GPU CC ACD */
	0x26400, 0x26416, 0x26420, 0x26427,
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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

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

/*
 * a6xx_gmu_start() - Start GMU and wait until FW boot up.
 * @device: Pointer to KGSL device
 */
static int a6xx_gmu_start(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	u32 val = 0x00000100;
	u32 mask = 0x000001FF;

	/* Check for 0xBABEFACE on legacy targets */
	if (gmu->ver.core <= 0x20010004) {
		val = 0xBABEFACE;
		mask = 0xFFFFFFFF;
	}

	/**
	 * We may have asserted gbif halt as part of reset sequence which may
	 * not get cleared if the gdsc was not reset. So clear it before
	 * attempting GMU boot.
	 */
	if (adreno_has_gbif(ADRENO_DEVICE(device)))
		kgsl_regwrite(device, A6XX_GBIF_HALT, 0x0);

	/* Set the log wptr index */
	gmu_core_regwrite(device, A6XX_GPU_GMU_CX_GMU_PWR_COL_CP_RESP,
			gmu->log_wptr_retention);

	/* Bring GMU out of reset */
	gmu_core_regwrite(device, A6XX_GMU_CM3_SYSRESET, 0);
	/* Make sure the request completes before continuing */
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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct device *dev = &gmu->pdev->dev;
	int val;

	/* Only trigger wakeup sequence if sleep sequence was done earlier */
	if (!test_bit(GMU_RSCC_SLEEP_SEQ_DONE, &device->gmu_core.flags))
		return 0;

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

	/* Enable the power counter because it was disabled before slumber */
	gmu_core_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 1);

	return 0;
error_rsc:
	dev_err(dev, "GPU RSC sequence stuck in waking up GPU\n");
	return -EINVAL;
}

static int a6xx_rpmh_power_off_gpu(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
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

	if (ADRENO_FEATURE(adreno_dev, ADRENO_LM) &&
			test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		gmu_core_regwrite(device, A6XX_GMU_AO_SPARE_CNTL, 0);

	set_bit(GMU_RSCC_SLEEP_SEQ_DONE, &device->gmu_core.flags);
	return 0;
}

static int _load_legacy_gmu_fw(struct kgsl_device *device,
	struct gmu_device *gmu)
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

static int load_gmu_fw(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
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

		md = gmu_get_memdesc(gmu, blk->addr, blk->size);
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

/*
 * a6xx_gmu_oob_set() - Set OOB interrupt to GMU.
 * @device: Pointer to kgsl device
 * @req: Which of the OOB bits to request
 */
static int a6xx_gmu_oob_set(struct kgsl_device *device,
		enum oob_request req)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	int ret = 0;
	int set, check;

	if (!adreno_is_a630(adreno_dev) && !adreno_is_a615_family(adreno_dev)) {
		set = BIT(30 - req * 2);
		check = BIT(31 - req);

		if (req >= 6) {
			dev_err(&gmu->pdev->dev,
					"OOB_set(0x%x) invalid\n", set);
			return -EINVAL;
		}
	} else {
		set = BIT(req + 16);
		check = BIT(req + 24);
	}

	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, set);

	if (timed_poll_check(device,
			A6XX_GMU_GMU2HOST_INTR_INFO,
			check,
			GPU_START_TIMEOUT,
			check)) {
		ret = -ETIMEDOUT;
		dev_err(&gmu->pdev->dev,
			"OOB_set(0x%x) timed out\n", set);
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
static inline void a6xx_gmu_oob_clear(struct kgsl_device *device,
		enum oob_request req)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	int clear;

	if (!adreno_is_a630(adreno_dev) && !adreno_is_a615_family(adreno_dev)) {
		clear = BIT(31 - req * 2);
		if (req >= 6) {
			dev_err(&gmu->pdev->dev,
					"OOB_clear(0x%x) invalid\n", clear);
			return;
		}
	} else
		clear = BIT(req + 24);

	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, clear);
	trace_kgsl_gmu_oob_clear(clear);
}

static void a6xx_gmu_irq_enable(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct kgsl_hfi *hfi = &gmu->hfi;

	/* Clear pending IRQs and Unmask needed IRQs */
	adreno_gmu_clear_and_unmask_irqs(ADRENO_DEVICE(device));

	/* Enable all IRQs on host */
	enable_irq(hfi->hfi_interrupt_num);
	enable_irq(gmu->gmu_interrupt_num);
}

static void a6xx_gmu_irq_disable(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct kgsl_hfi *hfi = &gmu->hfi;

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
		return hfi_send_req(KGSL_GMU_DEVICE(device),
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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

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
 * a6xx_gmu_cx_is_on() - Check if CX is on using GPUCC register
 * @device - Pointer to KGSL device struct
 */
static bool a6xx_gmu_cx_is_on(struct kgsl_device *device)
{
	unsigned int val;

	gmu_core_regread(device, A6XX_GPU_CC_CX_GDSCR, &val);
	return (val & BIT(31));
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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	unsigned int perf_idx = pwr->num_pwrlevels - pwr->default_pwrlevel - 1;
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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	unsigned int reg, reg1, reg2, reg3, reg4, reg5, reg6, reg7, reg8;
	unsigned long t;
	uint64_t ts1, ts2, ts3;

	ts1 = a6xx_gmu_read_ao_counter(device);

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

	ts2 = a6xx_gmu_read_ao_counter(device);
	/* Check one last time */

	gmu_core_regread(device, A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
	gmu_core_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg1);

	if (idle_trandition_complete(gmu->idle_level, reg, reg1))
		return 0;

	ts3 = a6xx_gmu_read_ao_counter(device);

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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	unsigned int status2;
	uint64_t ts1;

	ts1 = a6xx_gmu_read_ao_counter(device);
	if (timed_poll_check(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS,
			0, GMU_START_TIMEOUT, CXGXCPUBUSYIGNAHB)) {
		gmu_core_regread(device,
				A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS2, &status2);
		dev_err(&gmu->pdev->dev,
				"GMU not idling: status2=0x%x %llx %llx\n",
				status2, ts1, a6xx_gmu_read_ao_counter(device));
		return -ETIMEDOUT;
	}

	return 0;
}

/* A6xx GMU FENCE RANGE MASK */
#define GMU_FENCE_RANGE_MASK	((0x1 << 31) | ((0xA << 2) << 18) | (0x8A0))

static void load_gmu_version_info(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

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

/*
 * a6xx_gmu_fw_start() - set up GMU and start FW
 * @device: Pointer to KGSL device
 * @boot_state: State of the GMU being started
 */
static int a6xx_gmu_fw_start(struct kgsl_device *device,
		unsigned int boot_state)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	uint32_t gmu_log_info;
	int ret;
	unsigned int chipid = 0;

	/* Vote veto for FAL10 feature if supported*/
	if (a6xx_core->veto_fal10)
		gmu_core_regwrite(device, A6XX_GPU_GMU_CX_GMU_CX_FAL_INTF, 0x1);

	switch (boot_state) {
	case GMU_COLD_BOOT:
		/* Turn on TCM retention */
		gmu_core_regwrite(device, A6XX_GMU_GENERAL_7, 1);

		if (!test_and_set_bit(GMU_BOOT_INIT_DONE,
			&device->gmu_core.flags))
			ret = _load_gmu_rpmh_ucode(device);
		else
			ret = a6xx_rpmh_power_on_gpu(device);
		if (ret)
			return ret;

		if (gmu->load_mode == TCM_BOOT) {
			/* Load GMU image via AHB bus */
			ret = load_gmu_fw(device);
			if (ret)
				return ret;
		} else {
			dev_err(&gmu->pdev->dev, "Unsupported GMU load mode %d\n",
					gmu->load_mode);
			return -EINVAL;
		}
		break;
	case GMU_WARM_BOOT:
		ret = a6xx_rpmh_power_on_gpu(device);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	/* Clear init result to make sure we are getting fresh value */
	gmu_core_regwrite(device, A6XX_GMU_CM3_FW_INIT_RESULT, 0);
	gmu_core_regwrite(device, A6XX_GMU_CM3_BOOT_CONFIG, gmu->load_mode);

	gmu_core_regwrite(device, A6XX_GMU_HFI_QTBL_ADDR,
			gmu->hfi_mem->gmuaddr);
	gmu_core_regwrite(device, A6XX_GMU_HFI_QTBL_INFO, 1);

	gmu_core_regwrite(device, A6XX_GMU_AHB_FENCE_RANGE_0,
			GMU_FENCE_RANGE_MASK);

	/*
	 * Make sure that CM3 state is at reset value. Snapshot is changing
	 * NMI bit and if we boot up GMU with NMI bit set.GMU will boot straight
	 * in to NMI handler without executing __main code
	 */
	gmu_core_regwrite(device, A6XX_GMU_CM3_CFG, 0x4052);

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

	/* Populate the GMU version info before GMU boots */
	load_gmu_version_info(device);

	ret = a6xx_gmu_start(device);
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

/*
 * a6xx_gmu_load_firmware() - Load the ucode into the GPMU RAM & PDC/RSC
 * @device: Pointer to KGSL device
 */
static int a6xx_gmu_load_firmware(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	struct gmu_block_header *blk;
	int ret, offset = 0;

	/* GMU fw already saved and verified so do nothing new */
	if (gmu->fw_image)
		return 0;

	if (a6xx_core->gmufw_name == NULL)
		return -EINVAL;

	ret = request_firmware(&gmu->fw_image, a6xx_core->gmufw_name,
			device->dev);
	if (ret) {
		dev_err(device->dev, "request_firmware (%s) failed: %d\n",
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
			ret = gmu_prealloc_req(device, blk);

		if (ret)
			return ret;
	}

	 /* Request any other cache ranges that might be required */
	return gmu_cache_finalize(device);
}

#define A6XX_VBIF_XIN_HALT_CTRL1_ACKS   (BIT(0) | BIT(1) | BIT(2) | BIT(3))

static int a6xx_gmu_suspend(struct kgsl_device *device)
{
	int ret = 0;
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
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

	if (adreno_has_gbif(adreno_dev))
		adreno_smmu_resume(adreno_dev);

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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
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

		ret = hfi_send_req(gmu, H2F_MSG_PREPARE_SLUMBER, &req);
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
 * a6xx_rpmh_gpu_pwrctrl() - GPU power control via RPMh/GMU interface
 * @adreno_dev: Pointer to adreno device
 * @mode: requested power mode
 * @arg1: first argument for mode control
 * @arg2: second argument for mode control
 */
static int a6xx_gmu_rpmh_gpu_pwrctrl(struct kgsl_device *device,
		unsigned int mode, unsigned int arg1, unsigned int arg2)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	int ret;

	switch (mode) {
	case GMU_FW_START:
		ret = a6xx_gmu_fw_start(device, arg1);
		break;
	case GMU_SUSPEND:
		ret = a6xx_gmu_suspend(device);
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

void a6xx_gmu_enable_lm(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	u32 val;

	memset(adreno_dev->busy_data.throttle_cycles, 0,
		sizeof(adreno_dev->busy_data.throttle_cycles));

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
			!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
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

static int a6xx_gmu_ifpc_store(struct kgsl_device *device,
		unsigned int val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

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
	mem_hdr->gmuaddr = gmu_get_memtype_base(KGSL_GMU_DEVICE(device),
			tcm->type);
	mem_hdr->gpuaddr = 0;

	for (i = tcm->start; i <= tcm->last; i++)
		kgsl_regread(device, i, data++);

	return bytes + sizeof(*mem_hdr);
}

static void a6xx_gmu_snapshot_memories(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
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
 * a6xx_gmu_snapshot() - A6XX GMU snapshot function
 * @device: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A6XX GMU specific bits and pieces are grabbed
 * into the snapshot memory
 */
static void a6xx_gmu_snapshot(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{
	unsigned int val;

	dev_err(device->dev, "GMU snapshot started at 0x%llx ticks\n",
			a6xx_gmu_read_ao_counter(device));
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

	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
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
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

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

/*
 * a6xx_gmu_read_ao_counter() - Returns the 64bit always on counter value
 *
 * @device: Pointer to KGSL device
 */
u64 a6xx_gmu_read_ao_counter(struct kgsl_device *device)
{
	unsigned int l, h, h1;

	gmu_core_regread(device, A6XX_GMU_CX_GMU_ALWAYS_ON_COUNTER_H, &h);
	gmu_core_regread(device, A6XX_GMU_CX_GMU_ALWAYS_ON_COUNTER_L, &l);
	gmu_core_regread(device, A6XX_GMU_CX_GMU_ALWAYS_ON_COUNTER_H, &h1);

	/*
	 * If there's no change in COUNTER_H we have no overflow so return,
	 * otherwise read COUNTER_L again
	 */

	if (h == h1)
		return (uint64_t) l | ((uint64_t) h << 32);

	gmu_core_regread(device, A6XX_GMU_CX_GMU_ALWAYS_ON_COUNTER_L, &l);
	return (uint64_t) l | ((uint64_t) h1 << 32);
}

struct gmu_dev_ops adreno_a6xx_gmudev = {
	.load_firmware = a6xx_gmu_load_firmware,
	.oob_set = a6xx_gmu_oob_set,
	.oob_clear = a6xx_gmu_oob_clear,
	.irq_enable = a6xx_gmu_irq_enable,
	.irq_disable = a6xx_gmu_irq_disable,
	.hfi_start_msg = a6xx_gmu_hfi_start_msg,
	.enable_lm = a6xx_gmu_enable_lm,
	.rpmh_gpu_pwrctrl = a6xx_gmu_rpmh_gpu_pwrctrl,
	.gx_is_on = a6xx_gmu_gx_is_on,
	.cx_is_on = a6xx_gmu_cx_is_on,
	.wait_for_lowest_idle = a6xx_gmu_wait_for_lowest_idle,
	.wait_for_gmu_idle = a6xx_gmu_wait_for_idle,
	.ifpc_store = a6xx_gmu_ifpc_store,
	.ifpc_show = a6xx_gmu_ifpc_show,
	.snapshot = a6xx_gmu_snapshot,
	.cooperative_reset = a6xx_gmu_cooperative_reset,
	.wait_for_active_transition = a6xx_gmu_wait_for_active_transition,
	.read_ao_counter = a6xx_gmu_read_ao_counter,
	.gmu2host_intr_mask = HFI_IRQ_MASK,
	.gmu_ao_intr_mask = GMU_AO_INT_MASK,
};
