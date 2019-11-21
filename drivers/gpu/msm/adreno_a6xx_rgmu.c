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

#include "kgsl_gmu_core.h"
#include "kgsl_rgmu.h"
#include "kgsl_trace.h"

#include "adreno.h"
#include "a6xx_reg.h"
#include "adreno_a6xx.h"
#include "adreno_trace.h"
#include "adreno_snapshot.h"

/* RGMU timeouts */
#define RGMU_IDLE_TIMEOUT		100	/* ms */
#define RGMU_START_TIMEOUT		100	/* ms */
#define GPU_START_TIMEOUT		100	/* ms */
#define GLM_SLEEP_TIMEOUT		10	/* ms */

static const unsigned int a6xx_rgmu_registers[] = {
	/*GPUCX_TCM */
	0x1B400, 0x1B7FF,
	/* GMU CX */
	0x1F80F, 0x1F83D, 0x1F840, 0x1F8D8, 0x1F990, 0x1F99E, 0x1F9C0, 0x1F9CC,
	/* GMU AO */
	0x23B03, 0x23B16, 0x23B80, 0x23B82,
	/* GPU CC */
	0x24000, 0x24012, 0x24040, 0x24052, 0x24400, 0x24404, 0x24407, 0x2440B,
	0x24415, 0x2441C, 0x2441E, 0x2442D, 0x2443C, 0x2443D, 0x2443F, 0x24440,
	0x24442, 0x24449, 0x24458, 0x2445A, 0x24540, 0x2455E, 0x24800, 0x24802,
	0x24C00, 0x24C02, 0x25400, 0x25402, 0x25800, 0x25802, 0x25C00, 0x25C02,
	0x26000, 0x26002,
};

irqreturn_t rgmu_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int status = 0;

	adreno_read_gmureg(adreno_dev,
			ADRENO_REG_GMU_AO_HOST_INTERRUPT_STATUS, &status);

	if (status & RGMU_AO_IRQ_FENCE_ERR) {
		unsigned int fence_status;

		adreno_read_gmureg(adreno_dev,
				ADRENO_REG_GMU_AHB_FENCE_STATUS, &fence_status);
		adreno_write_gmureg(adreno_dev,
				ADRENO_REG_GMU_AO_HOST_INTERRUPT_CLR, status);

		dev_err_ratelimited(&rgmu->pdev->dev,
			"FENCE error interrupt received %x\n", fence_status);
	}

	if (status & ~RGMU_AO_IRQ_MASK)
		dev_err_ratelimited(&rgmu->pdev->dev,
				"Unhandled RGMU interrupts 0x%lx\n",
				status & ~RGMU_AO_IRQ_MASK);

	return IRQ_HANDLED;
}

irqreturn_t oob_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int status = 0;

	adreno_read_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_INFO, &status);

	if (status & RGMU_OOB_IRQ_ERR_MSG) {
		adreno_write_gmureg(adreno_dev,
				ADRENO_REG_GMU_GMU2HOST_INTR_CLR, status);

		dev_err_ratelimited(&rgmu->pdev->dev,
				"RGMU oob irq error\n");
		adreno_set_gpu_fault(adreno_dev, ADRENO_GMU_FAULT);
		adreno_dispatcher_schedule(device);
	}
	if (status & ~RGMU_OOB_IRQ_MASK)
		dev_err_ratelimited(&rgmu->pdev->dev,
				"Unhandled OOB interrupts 0x%lx\n",
				status & ~RGMU_OOB_IRQ_MASK);

	return IRQ_HANDLED;
}

/*
 * a6xx_rgmu_oob_set() - Set OOB interrupt to RGMU
 * @adreno_dev: Pointer to adreno device
 * @req: Which of the OOB bits to request
 */
static int a6xx_rgmu_oob_set(struct adreno_device *adreno_dev,
		enum oob_request req)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	int ret, set, check;

	if (!gmu_core_gpmu_isenabled(device))
		return 0;

	set = BIT(req + 16);
	check = BIT(req + 16);

	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, set);

	ret = timed_poll_check(device,
			A6XX_GMU_GMU2HOST_INTR_INFO,
			check,
			GPU_START_TIMEOUT,
			check);

	if (ret) {
		unsigned int status;

		gmu_core_regread(device, A6XX_RGMU_CX_PCC_DEBUG, &status);
		dev_err(&rgmu->pdev->dev,
				"Timed out while setting OOB req:%s status:0x%x\n",
				gmu_core_oob_type_str(req), status);
		return ret;
	}

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, check);
	trace_kgsl_gmu_oob_set(set);
	return 0;
}

/*
 * a6xx_rgmu_oob_clear() - Clear a previously set OOB request.
 * @adreno_dev: Pointer to the adreno device that has the RGMU
 * @req: Which of the OOB bits to clear
 */
static inline void a6xx_rgmu_oob_clear(struct adreno_device *adreno_dev,
		enum oob_request req)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	if (!gmu_core_gpmu_isenabled(device))
		return;

	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, BIT(req + 24));
	trace_kgsl_gmu_oob_clear(BIT(req + 24));
}

static void a6xx_rgmu_bcl_config(struct kgsl_device *device, bool on)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	if (on) {
		/* Enable BCL CRC HW i/f */
		gmu_core_regwrite(device,
				A6XX_GMU_AO_RGMU_GLM_HW_CRC_DISABLE, 0);
	} else {
		/* Disable CRC HW i/f */
		gmu_core_regwrite(device,
				A6XX_GMU_AO_RGMU_GLM_HW_CRC_DISABLE, 1);

		/* Wait for HW CRC disable ACK */
		if (timed_poll_check(device,
				A6XX_GMU_AO_RGMU_GLM_SLEEP_STATUS,
				BIT(1), GLM_SLEEP_TIMEOUT, BIT(1)))
			dev_err_ratelimited(&rgmu->pdev->dev,
				"Timed out waiting for HW CRC disable acknowledgment\n");

		/* Pull down the valid RGMU_GLM_SLEEP_CTRL[7] to 0 */
		gmu_core_regrmw(device, A6XX_GMU_AO_RGMU_GLM_SLEEP_CTRL,
				BIT(7), 0);

	}
}

static void a6xx_rgmu_irq_enable(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	/* Clear pending IRQs and Unmask needed IRQs */
	adreno_gmu_clear_and_unmask_irqs(ADRENO_DEVICE(device));

	/* Enable all IRQs on host */
	enable_irq(rgmu->oob_interrupt_num);
	enable_irq(rgmu->rgmu_interrupt_num);
}

static void a6xx_rgmu_irq_disable(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	/* Disable all IRQs on host */
	disable_irq(rgmu->rgmu_interrupt_num);
	disable_irq(rgmu->oob_interrupt_num);

	/* Mask all IRQs and clear pending IRQs */
	adreno_gmu_mask_and_clear_irqs(ADRENO_DEVICE(device));
}

static int a6xx_rgmu_ifpc_store(struct adreno_device *adreno_dev,
		unsigned int val)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	unsigned int requested_idle_level;
	int ret;

	if (!gmu_core_gpmu_isenabled(device) ||
		!ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		return -EINVAL;

	if (val)
		requested_idle_level = GPU_HW_IFPC;
	else
		requested_idle_level = GPU_HW_ACTIVE;

	if (requested_idle_level == rgmu->idle_level)
		return 0;

	mutex_lock(&device->mutex);

	/* Power down the GPU before changing the idle level */
	ret = kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	if (!ret) {
		rgmu->idle_level = requested_idle_level;
		kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);
	}

	mutex_unlock(&device->mutex);

	return ret;
}

static unsigned int a6xx_rgmu_ifpc_show(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);

	return gmu_core_gpmu_isenabled(device) &&
			rgmu->idle_level == GPU_HW_IFPC;
}


static void a6xx_rgmu_prepare_stop(struct adreno_device *adreno_dev)
{
	/* Turn off GX_MEM retention */
	kgsl_regwrite(KGSL_DEVICE(adreno_dev),
			A6XX_RBBM_BLOCK_GX_RETENTION_CNTL, 0);
}

#define GX_GDSC_POWER_OFF	BIT(6)
/*
 * a6xx_rgmu_gx_is_on() - Check if GX is on using pwr status register
 * @adreno_dev - Pointer to adreno_device
 * This check should only be performed if the keepalive bit is set or it
 * can be guaranteed that the power state of the GPU will remain unchanged
 */
static bool a6xx_rgmu_gx_is_on(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	unsigned int val;

	gmu_core_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &val);
	return !(val & GX_GDSC_POWER_OFF);
}

static int a6xx_rgmu_wait_for_lowest_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	unsigned int reg[10] = {0};
	unsigned long t;
	uint64_t ts1, ts2, ts3;

	if (!gmu_core_gpmu_isenabled(device) ||
			rgmu->idle_level != GPU_HW_IFPC)
		return 0;

	ts1 = a6xx_gmu_read_ao_counter(device);

	t = jiffies + msecs_to_jiffies(RGMU_IDLE_TIMEOUT);
	do {
		gmu_core_regread(device,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg[0]);

		if (reg[0] & GX_GDSC_POWER_OFF)
			return 0;

		/* Wait 10us to reduce unnecessary AHB bus traffic */
		usleep_range(10, 100);
	} while (!time_after(jiffies, t));

	ts2 = a6xx_gmu_read_ao_counter(device);

	/* Do one last read incase it succeeds */
	gmu_core_regread(device,
		A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg[0]);

	if (reg[0] & GX_GDSC_POWER_OFF)
		return 0;

	ts3 = a6xx_gmu_read_ao_counter(device);

	/* Collect abort data to help with debugging */
	gmu_core_regread(device, A6XX_RGMU_CX_PCC_DEBUG, &reg[1]);
	gmu_core_regread(device, A6XX_RGMU_CX_PCC_STATUS, &reg[2]);
	gmu_core_regread(device, A6XX_GPU_GMU_AO_GPU_CX_BUSY_STATUS, &reg[3]);
	kgsl_regread(device, A6XX_CP_STATUS_1, &reg[4]);
	gmu_core_regread(device, A6XX_GMU_RBBM_INT_UNMASKED_STATUS, &reg[5]);
	gmu_core_regread(device, A6XX_GMU_GMU_PWR_COL_KEEPALIVE, &reg[6]);
	kgsl_regread(device, A6XX_CP_CP2GMU_STATUS, &reg[7]);
	kgsl_regread(device, A6XX_CP_CONTEXT_SWITCH_CNTL, &reg[8]);
	gmu_core_regread(device, A6XX_GMU_AO_SPARE_CNTL, &reg[9]);

	dev_err(&rgmu->pdev->dev,
		"----------------------[ RGMU error ]----------------------\n");
	dev_err(&rgmu->pdev->dev, "Timeout waiting for lowest idle level\n");
	dev_err(&rgmu->pdev->dev,
			"Timestamps: %llx %llx %llx\n", ts1, ts2, ts3);
	dev_err(&rgmu->pdev->dev,
			"SPTPRAC_PWR_CLK_STATUS=%x PCC_DEBUG=%x PCC_STATUS=%x\n",
			reg[0], reg[1], reg[2]);
	dev_err(&rgmu->pdev->dev,
			"CX_BUSY_STATUS=%x CP_STATUS_1=%x\n", reg[3], reg[4]);
	dev_err(&rgmu->pdev->dev,
			"RBBM_INT_UNMASKED_STATUS=%x PWR_COL_KEEPALIVE=%x\n",
			reg[5], reg[6]);
	dev_err(&rgmu->pdev->dev,
			"CP2GMU_STATUS=%x CONTEXT_SWITCH_CNTL=%x AO_SPARE_CNTL=%x\n",
			reg[7], reg[8], reg[9]);

	WARN_ON(1);
	return -ETIMEDOUT;
}

/*
 * The lowest 16 bits of this value are the number of XO clock cycles
 * for main hysteresis. This is the first hysteresis. Here we set it
 * to 0x1680 cycles, or 300 us. The highest 16 bits of this value are
 * the number of XO clock cycles for short hysteresis. This happens
 * after main hysteresis. Here we set it to 0xA cycles, or 0.5 us.
 */
#define RGMU_PWR_COL_HYST 0x000A1680

/* HOSTTOGMU and TIMER0/1 interrupt mask: 0x20060 */
#define RGMU_INTR_EN_MASK  (BIT(5) | BIT(6) | BIT(17))

/* RGMU FENCE RANGE MASK */
#define RGMU_FENCE_RANGE_MASK	((0x1 << 31) | ((0xA << 2) << 18) | (0x8A0))

/*
 * a6xx_rgmu_fw_start() - set up GMU and start FW
 * @device: Pointer to KGSL device
 * @boot_state: State of the rgmu being started
 */
static int a6xx_rgmu_fw_start(struct kgsl_device *device,
		unsigned int boot_state)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	const struct firmware *fw = rgmu->fw_image;
	unsigned int status;

	switch (boot_state) {
	case GMU_COLD_BOOT:
	case GMU_WARM_BOOT:
		/* Turn on TCM retention */
		gmu_core_regwrite(device, A6XX_GMU_GENERAL_7, 1);

		/* Load RGMU FW image via AHB bus */
		gmu_core_blkwrite(device, A6XX_GMU_CM3_ITCM_START, fw->data,
				fw->size);
		/*
		 * Enable power counter because it was disabled before
		 * slumber.
		 */
		gmu_core_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE,
				1);
		break;
	}

	/* IFPC Feature Enable */
	if (rgmu->idle_level == GPU_HW_IFPC) {
		gmu_core_regwrite(device, A6XX_GMU_PWR_COL_INTER_FRAME_HYST,
				RGMU_PWR_COL_HYST);
		gmu_core_regwrite(device, A6XX_GMU_PWR_COL_INTER_FRAME_CTRL,
				BIT(0));
	}

	/* For RGMU CX interrupt */
	gmu_core_regwrite(device, A6XX_RGMU_CX_INTR_GEN_EN, RGMU_INTR_EN_MASK);

	/* Enable GMU AO to host interrupt */
	gmu_core_regwrite(device, A6XX_GMU_AO_INTERRUPT_EN, RGMU_AO_IRQ_MASK);

	/* For OOB */
	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_EN_2, 0x00FF0000);
	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_EN_3, 0xFF000000);

	/* Fence Address range configuration */
	gmu_core_regwrite(device, A6XX_GMU_AHB_FENCE_RANGE_0,
			RGMU_FENCE_RANGE_MASK);

	/* During IFPC RGMU will put fence in drop mode so we would
	 * need to put fence allow mode during slumber out sequence.
	 */
	gmu_core_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);

	/* BCL ON Sequence */
	a6xx_rgmu_bcl_config(device, true);

	/* Write 0 first to make sure that rgmu is reset */
	gmu_core_regwrite(device, A6XX_RGMU_CX_PCC_CTRL, 0);

	/* Make sure putting in reset doesn't happen after writing 1 */
	wmb();

	/* Bring rgmu out of reset */
	gmu_core_regwrite(device, A6XX_RGMU_CX_PCC_CTRL, 1);

	if (timed_poll_check(device, A6XX_RGMU_CX_PCC_INIT_RESULT,
			BIT(0), RGMU_START_TIMEOUT, BIT(0))) {
		gmu_core_regread(device, A6XX_RGMU_CX_PCC_DEBUG, &status);
		dev_err(&rgmu->pdev->dev,
				"rgmu boot Failed. status:%08x\n", status);
		return -ETIMEDOUT;
	}

	/* Read the RGMU firmware version from registers */
	gmu_core_regread(device, A6XX_GMU_GENERAL_0, &rgmu->ver);

	return 0;
}

static int a6xx_rgmu_suspend(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	int ret = 0;

	/* Check GX GDSC is status */
	if (a6xx_rgmu_gx_is_on(adreno_dev)) {

		/* Switch gx gdsc control from RGMU to CPU
		 * force non-zero reference count in clk driver
		 * so next disable call will turn
		 * off the GDSC
		 */
		ret = regulator_enable(rgmu->gx_gdsc);
		if (ret)
			dev_err(&rgmu->pdev->dev,
				"Fail to enable gx gdsc, error:%d\n", ret);

		ret = regulator_disable(rgmu->gx_gdsc);
		if (ret)
			dev_err(&rgmu->pdev->dev,
				"Fail to disable gx gdsc, error:%d\n", ret);

		if (a6xx_rgmu_gx_is_on(adreno_dev))
			dev_err(&rgmu->pdev->dev, "gx is stuck on\n");
	}

	return ret;
}

/*
 * a6xx_rgmu_gpu_pwrctrl() - GPU power control via rgmu interface
 * @adreno_dev: Pointer to adreno device
 * @mode: requested power mode
 * @arg1: first argument for mode control
 * @arg2: second argument for mode control
 */
static int a6xx_rgmu_gpu_pwrctrl(struct adreno_device *adreno_dev,
		unsigned int mode, unsigned int arg1, unsigned int arg2)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret = 0;

	if (!gmu_core_gpmu_isenabled(device))
		return 0;

	switch (mode) {
	case GMU_FW_START:
		ret = a6xx_rgmu_fw_start(device, arg1);
		break;
	case GMU_SUSPEND:
		ret = a6xx_rgmu_suspend(device);
		break;
	case GMU_NOTIFY_SLUMBER:

		/* Disable the power counter so that the RGMU is not busy */
		gmu_core_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE,
				0);

		/* BCL OFF Sequence */
		a6xx_rgmu_bcl_config(device, false);

		break;
	default:
		ret = -EINVAL;
		break;
	}

	return ret;
}

/*
 * a6xx_rgmu_load_firmware() - Load the ucode into the RGMU TCM
 * @device: Pointer to KGSL device
 */
static int a6xx_rgmu_load_firmware(struct kgsl_device *device)
{
	const struct firmware *fw = NULL;
	const struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	const struct adreno_gpu_core *gpucore = adreno_dev->gpucore;
	int ret;

	/* RGMU fw already saved and verified so do nothing new */
	if (rgmu->fw_image)
		return 0;

	ret = request_firmware(&fw, gpucore->gpmufw_name, device->dev);
	if (ret < 0) {
		KGSL_CORE_ERR("request_firmware (%s) failed: %d\n",
				gpucore->gpmufw_name, ret);
		return ret;
	}

	rgmu->fw_image = fw;
	return rgmu->fw_image ? 0 : -ENOMEM;
}

/* Halt RGMU execution */
static void a6xx_rgmu_halt_execution(struct kgsl_device *device)
{
	struct rgmu_device *rgmu = KGSL_RGMU_DEVICE(device);
	unsigned int index, status, fence;

	gmu_core_regread(device, A6XX_RGMU_CX_PCC_DEBUG, &index);
	gmu_core_regread(device, A6XX_RGMU_CX_PCC_STATUS, &status);
	gmu_core_regread(device, A6XX_GMU_AO_AHB_FENCE_CTRL, &fence);

	dev_err(&rgmu->pdev->dev,
			"RGMU Fault PCC_DEBUG:0x%x PCC_STATUS:0x%x FENCE_CTRL:0x%x\n",
			index, status, fence);

	/*
	 * Write 0 to halt RGMU execution. We halt it in GMU/GPU fault and
	 * re start PCC execution in recovery path.
	 */
	gmu_core_regwrite(device, A6XX_RGMU_CX_PCC_CTRL, 0);

	/*
	 * Ensure that fence is in allow mode after halting RGMU.
	 * After halting RGMU we dump snapshot.
	 */
	gmu_core_regwrite(device, A6XX_GMU_AO_AHB_FENCE_CTRL, 0);

}

/*
 * a6xx_rgmu_snapshot() - A6XX GMU snapshot function
 * @adreno_dev: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A6XX GMU specific bits and pieces are grabbed
 * into the snapshot memory
 */
static void a6xx_rgmu_snapshot(struct adreno_device *adreno_dev,
		struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	adreno_snapshot_registers(device, snapshot, a6xx_rgmu_registers,
					ARRAY_SIZE(a6xx_rgmu_registers) / 2);
}

struct gmu_dev_ops adreno_a6xx_rgmudev = {
	.load_firmware = a6xx_rgmu_load_firmware,
	.oob_set = a6xx_rgmu_oob_set,
	.oob_clear = a6xx_rgmu_oob_clear,
	.irq_enable = a6xx_rgmu_irq_enable,
	.irq_disable = a6xx_rgmu_irq_disable,
	.rpmh_gpu_pwrctrl = a6xx_rgmu_gpu_pwrctrl,
	.gx_is_on = a6xx_rgmu_gx_is_on,
	.prepare_stop = a6xx_rgmu_prepare_stop,
	.wait_for_lowest_idle = a6xx_rgmu_wait_for_lowest_idle,
	.ifpc_store = a6xx_rgmu_ifpc_store,
	.ifpc_show = a6xx_rgmu_ifpc_show,
	.snapshot = a6xx_rgmu_snapshot,
	.halt_execution = a6xx_rgmu_halt_execution,
	.read_ao_counter = a6xx_gmu_read_ao_counter,
	.gmu2host_intr_mask = RGMU_OOB_IRQ_MASK,
	.gmu_ao_intr_mask = RGMU_AO_IRQ_MASK,
};
