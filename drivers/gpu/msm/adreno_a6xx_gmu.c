/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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
#include <linux/jiffies.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/of_platform.h>

#include "kgsl_gmu_core.h"
#include "kgsl_gmu.h"
#include "kgsl_trace.h"

#include "adreno.h"
#include "a6xx_reg.h"
#include "adreno_a6xx.h"
#include "adreno_snapshot.h"
#include "adreno_trace.h"

static const unsigned int a6xx_gmu_gx_registers[] = {
	/* GMU GX */
	0x1A800, 0x1A800, 0x1A810, 0x1A813, 0x1A816, 0x1A816, 0x1A818, 0x1A81B,
	0x1A81E, 0x1A81E, 0x1A820, 0x1A823, 0x1A826, 0x1A826, 0x1A828, 0x1A82B,
	0x1A82E, 0x1A82E, 0x1A830, 0x1A833, 0x1A836, 0x1A836, 0x1A838, 0x1A83B,
	0x1A83E, 0x1A83E, 0x1A840, 0x1A843, 0x1A846, 0x1A846, 0x1A880, 0x1A884,
	0x1A900, 0x1A92B, 0x1A940, 0x1A940,
};

static const unsigned int a6xx_gmu_itcm_registers[] = {
	/* GMU ITCM */
	0x1B400, 0x1C3FF,
};

static const unsigned int a6xx_gmu_dtcm_registers[] = {
	/* GMU DTCM */
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

/*
 * _load_gmu_rpmh_ucode() - Load the ucode into the GPU PDC/RSC blocks
 * PDC and RSC execute GPU power on/off RPMh sequence
 * @device: Pointer to KGSL device
 */
static int _load_gmu_rpmh_ucode(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct resource *res_pdc, *res_cfg, *res_seq;
	void __iomem *cfg = NULL, *seq = NULL;
	unsigned int cfg_offset, seq_offset;

	/* Offsets from the base PDC (if no PDC subsections in the DTSI) */
	if ((adreno_is_a640v2(adreno_dev)) || (adreno_is_a680v2(adreno_dev))) {
		cfg_offset = 0x90000;
		seq_offset = 0x290000;
	} else {
		cfg_offset = 0x80000;
		seq_offset = 0x280000;
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
	 */

	/* Get pointers to each of the possible PDC resources */
	res_pdc = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM,
			"kgsl_gmu_pdc_reg");
	res_cfg = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM,
			"kgsl_gmu_pdc_cfg");
	res_seq = platform_get_resource_byname(gmu->pdev, IORESOURCE_MEM,
			"kgsl_gmu_pdc_seq");

	/*
	 * Map the starting address for pdc_cfg programming. If the pdc_cfg
	 * resource is not available use an offset from the base PDC resource.
	 */
	if (res_cfg)
		cfg = ioremap(res_cfg->start, resource_size(res_cfg));
	else if (res_pdc)
		cfg = ioremap(res_pdc->start + cfg_offset, 0x10000);

	if (!cfg) {
		dev_err(&gmu->pdev->dev, "Failed to map PDC CFG\n");
		return -ENODEV;
	}

	/*
	 * Map the starting address for pdc_seq programming. If the pdc_seq
	 * resource is not available use an offset from the base PDC resource.
	 */
	if (res_seq)
		seq = ioremap(res_seq->start, resource_size(res_seq));
	else if (res_pdc)
		seq = ioremap(res_pdc->start + seq_offset, 0x10000);

	if (!seq) {
		dev_err(&gmu->pdev->dev, "Failed to map PDC SEQ\n");
		iounmap(cfg);
		return -ENODEV;
	}

	/* Disable SDE clock gating */
	gmu_core_regwrite(device, A6XX_GPU_RSCC_RSC_STATUS0_DRV0, BIT(24));

	/* Setup RSC PDC handshake for sleep and wakeup */
	gmu_core_regwrite(device, A6XX_RSCC_PDC_SLAVE_ID_DRV0, 1);
	gmu_core_regwrite(device, A6XX_RSCC_HIDDEN_TCS_CMD0_DATA, 0);
	gmu_core_regwrite(device, A6XX_RSCC_HIDDEN_TCS_CMD0_ADDR, 0);
	gmu_core_regwrite(device,
			A6XX_RSCC_HIDDEN_TCS_CMD0_DATA + RSC_CMD_OFFSET, 0);
	gmu_core_regwrite(device,
			A6XX_RSCC_HIDDEN_TCS_CMD0_ADDR + RSC_CMD_OFFSET, 0);
	gmu_core_regwrite(device,
			A6XX_RSCC_HIDDEN_TCS_CMD0_DATA + RSC_CMD_OFFSET * 2,
			0x80000000);
	gmu_core_regwrite(device,
			A6XX_RSCC_HIDDEN_TCS_CMD0_ADDR + RSC_CMD_OFFSET * 2,
			0);
	gmu_core_regwrite(device, A6XX_RSCC_OVERRIDE_START_ADDR, 0);
	gmu_core_regwrite(device, A6XX_RSCC_PDC_SEQ_START_ADDR, 0x4520);
	gmu_core_regwrite(device, A6XX_RSCC_PDC_MATCH_VALUE_LO, 0x4510);
	gmu_core_regwrite(device, A6XX_RSCC_PDC_MATCH_VALUE_HI, 0x4514);

	/* Enable timestamp event for v1 only */
	if (adreno_is_a630v1(adreno_dev))
		gmu_core_regwrite(device, A6XX_RSCC_TIMESTAMP_UNIT1_EN_DRV0, 1);

	/* Load RSC sequencer uCode for sleep and wakeup */
	gmu_core_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0, 0xA7A506A0);
	gmu_core_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0 + 1, 0xA1E6A6E7);
	gmu_core_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0 + 2, 0xA2E081E1);
	gmu_core_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0 + 3, 0xE9A982E2);
	gmu_core_regwrite(device, A6XX_RSCC_SEQ_MEM_0_DRV0 + 4, 0x0020E8A8);

	/* Load PDC sequencer uCode for power up and power down sequence */
	_regwrite(seq, PDC_GPU_SEQ_MEM_0, 0xFEBEA1E1);
	_regwrite(seq, PDC_GPU_SEQ_MEM_0 + 1, 0xA5A4A3A2);
	_regwrite(seq, PDC_GPU_SEQ_MEM_0 + 2, 0x8382A6E0);
	_regwrite(seq, PDC_GPU_SEQ_MEM_0 + 3, 0xBCE3E284);
	_regwrite(seq, PDC_GPU_SEQ_MEM_0 + 4, 0x002081FC);

	/* Set TCS commands used by PDC sequence for low power modes */
	_regwrite(cfg, PDC_GPU_TCS1_CMD_ENABLE_BANK, 7);
	_regwrite(cfg, PDC_GPU_TCS1_CMD_WAIT_FOR_CMPL_BANK, 0);
	_regwrite(cfg, PDC_GPU_TCS1_CONTROL, 0);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_MSGID, 0x10108);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_ADDR, 0x30010);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_DATA, 1);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_MSGID + PDC_CMD_OFFSET, 0x10108);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_ADDR + PDC_CMD_OFFSET, 0x30000);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_DATA + PDC_CMD_OFFSET, 0x0);
	_regwrite(cfg, PDC_GPU_TCS1_CMD0_MSGID + PDC_CMD_OFFSET * 2, 0x10108);

	if (adreno_is_a640(adreno_dev) || adreno_is_a680(adreno_dev) ||
		adreno_is_a618(adreno_dev))
		_regwrite(cfg, PDC_GPU_TCS1_CMD0_ADDR + PDC_CMD_OFFSET * 2,
				0x30090);
	else
		_regwrite(cfg, PDC_GPU_TCS1_CMD0_ADDR + PDC_CMD_OFFSET * 2,
				0x30080);

	_regwrite(cfg, PDC_GPU_TCS1_CMD0_DATA + PDC_CMD_OFFSET * 2, 0x0);
	_regwrite(cfg, PDC_GPU_TCS3_CMD_ENABLE_BANK, 7);
	_regwrite(cfg, PDC_GPU_TCS3_CMD_WAIT_FOR_CMPL_BANK, 0);
	_regwrite(cfg, PDC_GPU_TCS3_CONTROL, 0);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_MSGID, 0x10108);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_ADDR, 0x30010);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_DATA, 2);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_MSGID + PDC_CMD_OFFSET, 0x10108);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_ADDR + PDC_CMD_OFFSET, 0x30000);

	if (adreno_is_a618(adreno_dev))
		_regwrite(cfg, PDC_GPU_TCS3_CMD0_DATA + PDC_CMD_OFFSET, 0x2);
	else
		_regwrite(cfg, PDC_GPU_TCS3_CMD0_DATA + PDC_CMD_OFFSET, 0x3);

	_regwrite(cfg, PDC_GPU_TCS3_CMD0_MSGID + PDC_CMD_OFFSET * 2, 0x10108);

	if (adreno_is_a640(adreno_dev) || adreno_is_a680(adreno_dev) ||
		adreno_is_a618(adreno_dev))
		_regwrite(cfg, PDC_GPU_TCS3_CMD0_ADDR + PDC_CMD_OFFSET * 2,
				0x30090);
	else
		_regwrite(cfg, PDC_GPU_TCS3_CMD0_ADDR + PDC_CMD_OFFSET * 2,
				0x30080);
	_regwrite(cfg, PDC_GPU_TCS3_CMD0_DATA + PDC_CMD_OFFSET * 2, 0x3);

	/* Setup GPU PDC */
	_regwrite(cfg, PDC_GPU_SEQ_START_ADDR, 0);
	_regwrite(cfg, PDC_GPU_ENABLE_PDC, 0x80000001);

	/* ensure no writes happen before the uCode is fully written */
	wmb();

	iounmap(seq);
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

	kgsl_regwrite(device, A6XX_GMU_CX_GMU_WFI_CONFIG, 0x0);

	/* Bring GMU out of reset */
	gmu_core_regwrite(device, A6XX_GMU_CM3_SYSRESET, 0);
	if (timed_poll_check(device,
			A6XX_GMU_CM3_FW_INIT_RESULT,
			0xBABEFACE,
			GMU_START_TIMEOUT,
			0xFFFFFFFF)) {
		dev_err(&gmu->pdev->dev, "GMU doesn't boot\n");
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

	if (timed_poll_check(device,
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

	/* RSC sleep sequence is different on v1 */
	if (adreno_is_a630v1(adreno_dev))
		gmu_core_regwrite(device, A6XX_RSCC_TIMESTAMP_UNIT1_EN_DRV0, 1);

	gmu_core_regwrite(device, A6XX_GMU_RSCC_CONTROL_REQ, 1);
	/* Make sure the request completes before continuing */
	wmb();

	if (adreno_is_a630v1(adreno_dev))
		ret = timed_poll_check(device,
				A6XX_RSCC_TIMESTAMP_UNIT1_OUTPUT_DRV0,
				BIT(0),
				GPU_START_TIMEOUT,
				BIT(0));
	else
		ret = timed_poll_check(device,
				A6XX_GPU_RSCC_RSC_STATUS0_DRV0,
				BIT(16),
				GPU_START_TIMEOUT,
				BIT(16));

	if (ret) {
		dev_err(&gmu->pdev->dev, "GPU RSC power off fail\n");
		return -ETIMEDOUT;
	}

	/* Read to clear the timestamp valid signal. Don't care what we read. */
	if (adreno_is_a630v1(adreno_dev)) {
		gmu_core_regread(device,
				A6XX_RSCC_TIMESTAMP_UNIT0_TIMESTAMP_L_DRV0,
				&ret);
		gmu_core_regread(device,
				A6XX_RSCC_TIMESTAMP_UNIT0_TIMESTAMP_H_DRV0,
				&ret);
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
	u32 *fwptr = (u32 *)fw->data;
	int i;

	if (fw->size > MAX_GMUFW_SIZE)
		return -EINVAL;

	for (i = 0; i < (fw->size >> 2); i++)
		gmu_core_regwrite(device,
			A6XX_GMU_CM3_ITCM_START + i, fwptr[i]);

	/* Proceed only after the FW is written */
	wmb();
	return 0;
}

static int load_gmu_fw(struct kgsl_device *device)
{
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	uint8_t *fw = (uint8_t *)gmu->fw_image->data;
	int j;
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

		md = gmu_get_memdesc(blk->addr, blk->size);
		if (md == NULL) {
			dev_err(&gmu->pdev->dev,
					"No backing memory for 0x%8.8X\n",
					blk->addr);
			return -EINVAL;
		}

		if (md->mem_type == GMU_ITCM || md->mem_type == GMU_DTCM) {
			uint32_t *fwptr = (uint32_t *)fw;

			tcm_addr = (blk->addr - (uint32_t)md->gmuaddr) /
				sizeof(uint32_t);

			if (md->mem_type == GMU_ITCM)
				tcm_addr += A6XX_GMU_CM3_ITCM_START;
			else
				tcm_addr += A6XX_GMU_CM3_DTCM_START;

			for (j = 0; j < blk->size / sizeof(uint32_t); j++)
				gmu_core_regwrite(device, tcm_addr + j,
					fwptr[j]);
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
 * @adreno_dev: Pointer to adreno device
 * @req: Which of the OOB bits to request
 */
static int a6xx_gmu_oob_set(struct adreno_device *adreno_dev,
		enum oob_request req)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	int ret = 0;
	int set, check;

	if (!gmu_core_gpmu_isenabled(device))
		return 0;

	if (!adreno_is_a630(adreno_dev) && !adreno_is_a615_family(adreno_dev)) {
		set = BIT(30 - req * 2);
		check = BIT(31 - req);

		if ((gmu->hfi.version & 0x1F) == 0) {
			/* LEGACY for intermediate oobs */
			set = BIT(req + 16);
			check = BIT(req + 16);
		}

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
 * @adreno_dev: Pointer to the adreno device that has the GMU
 * @req: Which of the OOB bits to clear
 */
static inline void a6xx_gmu_oob_clear(struct adreno_device *adreno_dev,
		enum oob_request req)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	int clear;

	if (!gmu_core_gpmu_isenabled(device))
		return;

	if (!adreno_is_a630(adreno_dev) && !adreno_is_a615_family(adreno_dev)) {
		clear = BIT(31 - req * 2);
		if (req >= 6) {
			dev_err(&gmu->pdev->dev,
					"OOB_clear(0x%x) invalid\n", clear);
			return;
		}
		/* LEGACY for intermediate oobs */
		if ((gmu->hfi.version & 0x1F) == 0)
			clear = BIT(req + 24);
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

static int a6xx_gmu_hfi_start_msg(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct hfi_start_cmd req;

	if (!gmu_core_gpmu_isenabled(device))
		return 0;

	if (adreno_is_a640(adreno_dev) || adreno_is_a680(adreno_dev))
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
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret;

	gmu_core_regwrite(device, A6XX_GMU_DCVS_ACK_OPTION, DCVS_ACK_NONBLOCK);

	gmu_core_regwrite(device, A6XX_GMU_DCVS_PERF_SETTING,
			FREQ_VOTE(perf_idx, CLKSET_OPTION_ATLEAST));

	gmu_core_regwrite(device, A6XX_GMU_DCVS_BW_SETTING, BW_VOTE(bw_idx));

	ret = a6xx_gmu_oob_set(adreno_dev, oob_dcvs);
	if (ret == 0)
		gmu_core_regread(device, A6XX_GMU_DCVS_RETURN, &ret);

	a6xx_gmu_oob_clear(adreno_dev, oob_dcvs);

	return ret;
}
static int a6xx_complete_rpmh_votes(struct kgsl_device *device)
{
	int ret = 0;

	if (!gmu_core_gpmu_isenabled(device))
		return ret;

	ret |= timed_poll_check(device, A6XX_RSCC_TCS0_DRV0_STATUS, BIT(0),
			GPU_RESET_TIMEOUT, BIT(0));
	ret |= timed_poll_check(device, A6XX_RSCC_TCS1_DRV0_STATUS, BIT(0),
			GPU_RESET_TIMEOUT, BIT(0));
	ret |= timed_poll_check(device, A6XX_RSCC_TCS2_DRV0_STATUS, BIT(0),
			GPU_RESET_TIMEOUT, BIT(0));
	ret |= timed_poll_check(device, A6XX_RSCC_TCS3_DRV0_STATUS, BIT(0),
			GPU_RESET_TIMEOUT, BIT(0));

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

	if (!gmu_core_isenabled(device) ||
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

	if (!gmu_core_isenabled(device) ||
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
 * @adreno_dev - Pointer to adreno_device
 * This check should only be performed if the keepalive bit is set or it
 * can be guaranteed that the power state of the GPU will remain unchanged
 */
static bool a6xx_gmu_gx_is_on(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int val;

	if (!gmu_core_isenabled(device))
		return true;

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
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
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

	return a6xx_gmu_oob_set(adreno_dev, oob_boot_slumber);
}

static bool idle_transition_complete(unsigned int idle_level,
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

static int a6xx_gmu_wait_for_lowest_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	unsigned int reg, reg1, reg2, reg3, reg4, reg5, reg6, reg7, reg8;
	unsigned long t;
	uint64_t ts1, ts2, ts3;

	if (!gmu_core_gpmu_isenabled(device))
		return 0;

	ts1 = read_AO_counter(device);

	t = jiffies + msecs_to_jiffies(GMU_IDLE_TIMEOUT);
	do {
		gmu_core_regread(device,
			A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
		gmu_core_regread(device,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg1);

		if (idle_transition_complete(gmu->idle_level, reg, reg1))
			return 0;
		/* Wait 100us to reduce unnecessary AHB bus traffic */
		usleep_range(10, 100);
	} while (!time_after(jiffies, t));

	ts2 = read_AO_counter(device);
	/* Check one last time */

	gmu_core_regread(device, A6XX_GPU_GMU_CX_GMU_RPMH_POWER_STATE, &reg);
	gmu_core_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg1);

	if (idle_transition_complete(gmu->idle_level, reg, reg1))
		return 0;

	ts3 = read_AO_counter(device);

	/* Collect abort data to help with debugging */
	gmu_core_regread(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &reg2);
	kgsl_regread(device, A6XX_CP_STATUS_1, &reg3);
	gmu_core_regread(device, A6XX_GMU_RBBM_INT_UNMASKED_STATUS, &reg4);
	gmu_core_regread(device, A6XX_GMU_GMU_PWR_COL_KEEPALIVE, &reg5);
	kgsl_regread(device, A6XX_CP_CP2GMU_STATUS, &reg6);
	kgsl_regread(device, A6XX_CP_CONTEXT_SWITCH_CNTL, &reg7);
	gmu_core_regread(device, A6XX_GMU_AO_SPARE_CNTL, &reg8);

	dev_err(&gmu->pdev->dev,
		"----------------------[ GMU error ]----------------------\n");
	dev_err(&gmu->pdev->dev,
		"Timeout waiting for lowest idle level %d\n", gmu->idle_level);
	dev_err(&gmu->pdev->dev,
		"Timestamps: %llx %llx %llx\n", ts1, ts2, ts3);
	dev_err(&gmu->pdev->dev,
		"RPMH_POWER_STATE=%x SPTPRAC_PWR_CLK_STATUS=%x\n", reg, reg1);
	dev_err(&gmu->pdev->dev,
		"CX_BUSY_STATUS=%x CP_STATUS_1=%x\n", reg2, reg3);
	dev_err(&gmu->pdev->dev,
		"RBBM_INT_UNMASKED_STATUS=%x PWR_COL_KEEPALIVE=%x\n",
		reg4, reg5);
	dev_err(&gmu->pdev->dev,
		"CP2GMU_STATUS=%x CONTEXT_SWITCH_CNTL=%x AO_SPARE_CNTL=%x\n",
		reg6, reg7, reg8);

	WARN_ON(1);
	return -ETIMEDOUT;
}

/* Bitmask for GPU idle status check */
#define CXGXCPUBUSYIGNAHB	BIT(30)
static int a6xx_gmu_wait_for_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	unsigned int status2;
	uint64_t ts1;

	ts1 = read_AO_counter(device);
	if (timed_poll_check(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS,
			0, GMU_START_TIMEOUT, CXGXCPUBUSYIGNAHB)) {
		gmu_core_regread(device,
				A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS2, &status2);
		dev_err(&gmu->pdev->dev,
				"GMU not idling: status2=0x%x %llx %llx\n",
				status2, ts1, read_AO_counter(device));
		return -ETIMEDOUT;
	}

	return 0;
}

/* A6xx GMU FENCE RANGE MASK */
#define GMU_FENCE_RANGE_MASK	((0x1 << 31) | ((0xA << 2) << 18) | (0x8A0))

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
	struct gmu_memdesc *mem_addr = gmu->hfi_mem;
	uint32_t gmu_log_info;
	int ret;
	unsigned int chipid = 0;

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
			mem_addr->gmuaddr);
	gmu_core_regwrite(device, A6XX_GMU_HFI_QTBL_INFO, 1);

	gmu_core_regwrite(device, A6XX_GMU_AHB_FENCE_RANGE_0,
			GMU_FENCE_RANGE_MASK);

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
	ret = a6xx_gmu_start(device);
	if (ret)
		return ret;

	if (ADRENO_QUIRK(adreno_dev, ADRENO_QUIRK_HFI_USE_REG)) {
		ret = a6xx_gmu_gfx_rail_on(device);
		if (ret) {
			a6xx_gmu_oob_clear(adreno_dev, oob_boot_slumber);
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
	const struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	const struct adreno_gpu_core *gpucore = adreno_dev->gpucore;
	int ret =  -EINVAL;

	/* GMU fw already saved and verified so do nothing new */
	if (gmu->fw_image)
		return 0;

	if (gpucore->gpmufw_name == NULL)
		return -EINVAL;

	ret = request_firmware(&gmu->fw_image, gpucore->gpmufw_name,
			device->dev);
	if (ret || gmu->fw_image == NULL)
		KGSL_CORE_ERR("request_firmware (%s) failed: %d\n",
				gpucore->gpmufw_name, ret);

	return ret;
}

#define A6XX_VBIF_XIN_HALT_CTRL1_ACKS   (BIT(0) | BIT(1) | BIT(2) | BIT(3))

static void a6xx_llm_glm_handshake(struct kgsl_device *device)
{
	unsigned int val;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
			!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	if (adreno_is_a640(adreno_dev))
		return;

	gmu_core_regread(device, A6XX_GMU_LLM_GLM_SLEEP_CTRL, &val);
	if (val & (BIT(4) | BIT(5)))
		return;

	gmu_core_regrmw(device, A6XX_GMU_LLM_GLM_SLEEP_CTRL, 0, BIT(4));
	gmu_core_regrmw(device, A6XX_GMU_LLM_GLM_SLEEP_CTRL, 0, BIT(0));

	if (timed_poll_check(device, A6XX_GMU_LLM_GLM_SLEEP_STATUS,
		BIT(0), GPU_RESET_TIMEOUT, BIT(0)))
		dev_err(&gmu->pdev->dev, "LLM-GLM handshake failed\n");
}

static void a6xx_isense_disable(struct kgsl_device *device)
{
	unsigned int val;
	const struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
		!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	gmu_core_regread(device, A6XX_GPU_CS_ENABLE_REG, &val);
	if (val) {
		gmu_core_regwrite(device, A6XX_GPU_CS_ENABLE_REG, 0);
		gmu_core_regwrite(device, A6XX_GMU_ISENSE_CTRL, 0);
	}
}

static int a6xx_gmu_suspend(struct kgsl_device *device)
{
	int ret = 0;
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	/* do it only if LM feature is enabled */
	/* Disable ISENSE if it's on */
	a6xx_isense_disable(device);

	/* LLM-GLM handshake sequence */
	a6xx_llm_glm_handshake(device);

	/* If SPTP_RAC is on, turn off SPTP_RAC HS */
	a6xx_gmu_sptprac_disable(adreno_dev);

	/* Disconnect GPU from BUS is not needed if CX GDSC goes off later */

	/* Check no outstanding RPMh voting */
	a6xx_complete_rpmh_votes(device);

	/*
	 * This is based on the assumption that GMU is the only one controlling
	 * the GX HS. This code path is the only client voting for GX through
	 * the regulator interface.
	 */
	if (gmu->gx_gdsc) {
		if (a6xx_gmu_gx_is_on(adreno_dev)) {
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

			if (a6xx_gmu_gx_is_on(adreno_dev))
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

	ret = a6xx_gmu_oob_set(adreno_dev, oob_boot_slumber);
	a6xx_gmu_oob_clear(adreno_dev, oob_boot_slumber);

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
static int a6xx_gmu_rpmh_gpu_pwrctrl(struct adreno_device *adreno_dev,
		unsigned int mode, unsigned int arg1, unsigned int arg2)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
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
			a6xx_gmu_oob_clear(adreno_dev, oob_boot_slumber);
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

#define LM_DEFAULT_LIMIT	6000
#define GPU_LIMIT_THRESHOLD_ENABLE	BIT(31)

static uint32_t lm_limit(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (adreno_dev->lm_limit)
		return adreno_dev->lm_limit;

	if (of_property_read_u32(device->pdev->dev.of_node, "qcom,lm-limit",
		&adreno_dev->lm_limit))
		adreno_dev->lm_limit = LM_DEFAULT_LIMIT;

	return adreno_dev->lm_limit;
}

static int a640_throttling_counters[ADRENO_GPMU_THROTTLE_COUNTERS] = {
	0x11, 0x15, 0x19
};

static void _setup_throttling_counters(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	int i, ret;

	for (i = 0; i < ARRAY_SIZE(a640_throttling_counters); i++) {
		adreno_dev->busy_data.throttle_cycles[i] = 0;

		if (!a640_throttling_counters[i])
			continue;
		if (adreno_dev->gpmu_throttle_counters[i])
			continue;

		ret = adreno_perfcounter_get(adreno_dev,
				KGSL_PERFCOUNTER_GROUP_GPMU_PWR,
				a640_throttling_counters[i],
				&adreno_dev->gpmu_throttle_counters[i],
				NULL,
				PERFCOUNTER_FLAG_KERNEL);
		if (ret)
			dev_err_once(&gmu->pdev->dev,
				"Unable to get counter for LM: GPMU_PWR %d\n",
				a640_throttling_counters[i]);
	}
}

#define LIMITS_CONFIG(t, s, c, i, a) ( \
		(t & 0xF) | \
		((s & 0xF) << 4) | \
		((c & 0xF) << 8) | \
		((i & 0xF) << 12) | \
		((a & 0xF) << 16))

void a6xx_gmu_enable_lm(struct kgsl_device *device)
{
	int result;
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct device *dev = &gmu->pdev->dev;
	struct hfi_lmconfig_cmd cmd;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_LM) ||
			!test_bit(ADRENO_LM_CTRL, &adreno_dev->pwrctrl_flag))
		return;

	/* a640 only needs to set up throttling counters for DCVS */
	if (adreno_is_a640(adreno_dev)) {
		_setup_throttling_counters(adreno_dev);
		return;
	}

	gmu_core_regwrite(device, A6XX_GPU_GMU_CX_GMU_PWR_THRESHOLD,
		GPU_LIMIT_THRESHOLD_ENABLE | lm_limit(adreno_dev));
	gmu_core_regwrite(device, A6XX_GMU_AO_SPARE_CNTL, 1);
	gmu_core_regwrite(device, A6XX_GPU_GMU_CX_GMU_ISENSE_CTRL, 0x1);

	gmu->lm_config = LIMITS_CONFIG(1, 1, 1, 0, 0);
	gmu->bcl_config = 0;
	gmu->lm_dcvs_level = 0;

	cmd.limit_conf = gmu->lm_config;
	cmd.bcl_conf = gmu->bcl_config;
	cmd.lm_enable_bitmask = 0;

	if (gmu->lm_dcvs_level <= MAX_GX_LEVELS)
		cmd.lm_enable_bitmask =
			(1 << (gmu->lm_dcvs_level + 1)) - 1;

	result = hfi_send_req(gmu, H2F_MSG_LM_CFG, &cmd);
	if (result)
		dev_err(dev, "Failure enabling limits management:%d\n", result);
}

static int a6xx_gmu_ifpc_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	unsigned int requested_idle_level;

	if (!gmu_core_gpmu_isenabled(device) ||
			!ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
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

static unsigned int a6xx_gmu_ifpc_show(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

	return gmu_core_gpmu_isenabled(device) &&
			gmu->idle_level  >= GPU_HW_IFPC;
}

static size_t a6xx_snapshot_gmu_tcm(struct kgsl_device *device,
		u8 *buf, size_t remain, void *priv)
{
	struct kgsl_snapshot_gmu_mem *mem_hdr =
		(struct kgsl_snapshot_gmu_mem *)buf;
	unsigned int *data = (unsigned int *)(buf + sizeof(*mem_hdr));
	unsigned int i, bytes;
	unsigned int *type = priv;
	const unsigned int *regs;

	if (*type == GMU_ITCM)
		regs = a6xx_gmu_itcm_registers;
	else
		regs = a6xx_gmu_dtcm_registers;

	bytes = (regs[1] - regs[0] + 1) << 2;

	if (remain < bytes + sizeof(*mem_hdr)) {
		SNAPSHOT_ERR_NOMEM(device, "GMU Memory");
		return 0;
	}

	mem_hdr->type = SNAPSHOT_GMU_MEM_BIN_BLOCK;
	mem_hdr->hostaddr = 0;
	mem_hdr->gmuaddr = gmu_get_memtype_base(KGSL_GMU_DEVICE(device), *type);
	mem_hdr->gpuaddr = 0;

	for (i = regs[0]; i <= regs[1]; i++)
		kgsl_regread(device, i, data++);

	return bytes + sizeof(*mem_hdr);
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
	struct gmu_mem_type_desc *desc = priv;
	unsigned int *data = (unsigned int *)(buf + sizeof(*mem_hdr));

	if (priv == NULL)
		return 0;

	if (remain < desc->memdesc->size + sizeof(*mem_hdr)) {
		KGSL_CORE_ERR(
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

/*
 * a6xx_gmu_snapshot() - A6XX GMU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A6XX GMU specific bits and pieces are grabbed
 * into the snapshot memory
 */
static void a6xx_gmu_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);
	bool gx_on;
	struct gmu_mem_type_desc desc[] = {
		{gmu->hfi_mem, SNAPSHOT_GMU_MEM_HFI},
		{gmu->persist_mem, SNAPSHOT_GMU_MEM_BIN_BLOCK},
		{gmu->icache_mem, SNAPSHOT_GMU_MEM_BIN_BLOCK},
		{gmu->dcache_mem, SNAPSHOT_GMU_MEM_BIN_BLOCK},
		{gmu->gmu_log, SNAPSHOT_GMU_MEM_LOG},
		{gmu->dump_mem, SNAPSHOT_GMU_MEM_BIN_BLOCK} };
	unsigned int val, i;
	enum gmu_mem_type type;

	if (!gmu_core_isenabled(device))
		return;

	for (i = 0; i < ARRAY_SIZE(desc); i++) {
		if (desc[i].memdesc)
			kgsl_snapshot_add_section(device,
					KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
					snapshot, a6xx_snapshot_gmu_mem,
					&desc[i]);
	}

	type = GMU_ITCM;
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
			snapshot, a6xx_snapshot_gmu_tcm, &type);
	type = GMU_DTCM;
	kgsl_snapshot_add_section(device, KGSL_SNAPSHOT_SECTION_GMU_MEMORY,
			snapshot, a6xx_snapshot_gmu_tcm, &type);

	adreno_snapshot_registers(device, snapshot, a6xx_gmu_registers,
			ARRAY_SIZE(a6xx_gmu_registers) / 2);

	gx_on = a6xx_gmu_gx_is_on(adreno_dev);

	if (gx_on) {
		/* Set fence to ALLOW mode so registers can be read */
		kgsl_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);
		kgsl_regread(device, A6XX_GMU_AO_AHB_FENCE_CTRL, &val);

		KGSL_DRV_ERR(device, "set FENCE to ALLOW mode:%x\n", val);
		adreno_snapshot_registers(device, snapshot,
				a6xx_gmu_gx_registers,
				ARRAY_SIZE(a6xx_gmu_gx_registers) / 2);
	}
}

static int a6xx_gmu_wait_for_active_transition(
	struct adreno_device *adreno_dev)
{
	unsigned int reg, num_retries;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct gmu_device *gmu = KGSL_GMU_DEVICE(device);

	if (!gmu_core_gpmu_isenabled(device))
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
	.wait_for_lowest_idle = a6xx_gmu_wait_for_lowest_idle,
	.wait_for_gmu_idle = a6xx_gmu_wait_for_idle,
	.ifpc_store = a6xx_gmu_ifpc_store,
	.ifpc_show = a6xx_gmu_ifpc_show,
	.snapshot = a6xx_gmu_snapshot,
	.wait_for_active_transition = a6xx_gmu_wait_for_active_transition,
	.gmu2host_intr_mask = HFI_IRQ_MASK,
	.gmu_ao_intr_mask = GMU_AO_INT_MASK,
};
