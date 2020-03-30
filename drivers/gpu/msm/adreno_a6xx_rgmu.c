// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_a6xx_rgmu.h"
#include "adreno_snapshot.h"
#include "kgsl_trace.h"
#include "kgsl_util.h"

#define RGMU_CLK_FREQ 200000000

/* RGMU timeouts */
#define RGMU_IDLE_TIMEOUT		100	/* ms */
#define RGMU_START_TIMEOUT		100	/* ms */
#define GPU_START_TIMEOUT		100	/* ms */
#define GLM_SLEEP_TIMEOUT		10	/* ms */

static const unsigned int a6xx_rgmu_registers[] = {
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

static irqreturn_t a6xx_rgmu_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	unsigned int status = 0;

	gmu_core_regread(device, A6XX_GMU_AO_HOST_INTERRUPT_STATUS, &status);

	if (status & RGMU_AO_IRQ_FENCE_ERR) {
		unsigned int fence_status;

		gmu_core_regread(device, A6XX_GMU_AHB_FENCE_STATUS,
			&fence_status);
		gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_CLR,
			status);

		dev_err_ratelimited(&rgmu->pdev->dev,
			"FENCE error interrupt received %x\n", fence_status);
	}

	if (status & ~RGMU_AO_IRQ_MASK)
		dev_err_ratelimited(&rgmu->pdev->dev,
				"Unhandled RGMU interrupts 0x%lx\n",
				status & ~RGMU_AO_IRQ_MASK);

	return IRQ_HANDLED;
}

static irqreturn_t a6xx_oob_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	unsigned int status = 0;

	gmu_core_regread(device, A6XX_GMU_GMU2HOST_INTR_INFO, &status);

	if (status & RGMU_OOB_IRQ_ERR_MSG) {
		gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, status);

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

static const char *oob_to_str(enum oob_request req)
{
	if (req == oob_gpu)
		return "oob_gpu";
	else if (req == oob_perfcntr)
		return "oob_perfcntr";
	return "unknown";
}

/*
 * a6xx_rgmu_oob_set() - Set OOB interrupt to RGMU
 * @adreno_dev: Pointer to adreno device
 * @req: Which of the OOB bits to request
 */
static int a6xx_rgmu_oob_set(struct kgsl_device *device,
		enum oob_request req)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	int ret, set, check;

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
				oob_to_str(req), status);
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
static void a6xx_rgmu_oob_clear(struct kgsl_device *device,
		enum oob_request req)
{
	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, BIT(req + 24));
	trace_kgsl_gmu_oob_clear(BIT(req + 24));
}

static void a6xx_rgmu_bcl_config(struct kgsl_device *device, bool on)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);

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
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);

	/* Clear pending IRQs and Unmask needed IRQs */
	adreno_gmu_clear_and_unmask_irqs(ADRENO_DEVICE(device));

	/* Enable all IRQs on host */
	enable_irq(rgmu->oob_interrupt_num);
	enable_irq(rgmu->rgmu_interrupt_num);
}

static void a6xx_rgmu_irq_disable(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);

	/* Disable all IRQs on host */
	disable_irq(rgmu->rgmu_interrupt_num);
	disable_irq(rgmu->oob_interrupt_num);

	/* Mask all IRQs and clear pending IRQs */
	adreno_gmu_mask_and_clear_irqs(ADRENO_DEVICE(device));
}

static int a6xx_rgmu_ifpc_store(struct kgsl_device *device,
		unsigned int val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	unsigned int requested_idle_level;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		return -EINVAL;

	if (val)
		requested_idle_level = GPU_HW_IFPC;
	else
		requested_idle_level = GPU_HW_ACTIVE;

	if (requested_idle_level == rgmu->idle_level)
		return 0;

	mutex_lock(&device->mutex);

	/* Power down the GPU before changing the idle level */
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SUSPEND);
	rgmu->idle_level = requested_idle_level;
	kgsl_pwrctrl_change_state(device, KGSL_STATE_SLUMBER);

	mutex_unlock(&device->mutex);

	return 0;
}

static unsigned int a6xx_rgmu_ifpc_show(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);

	return rgmu->idle_level == GPU_HW_IFPC;
}


static void a6xx_rgmu_prepare_stop(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
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
static bool a6xx_rgmu_gx_is_on(struct kgsl_device *device)
{
	unsigned int val;

	gmu_core_regread(device, A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &val);
	return !(val & GX_GDSC_POWER_OFF);
}

static int a6xx_rgmu_wait_for_lowest_idle(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	unsigned int reg[10] = {0};
	unsigned long t;
	uint64_t ts1, ts2, ts3;

	if (rgmu->idle_level != GPU_HW_IFPC)
		return 0;

	ts1 = a6xx_read_alwayson(ADRENO_DEVICE(device));

	t = jiffies + msecs_to_jiffies(RGMU_IDLE_TIMEOUT);
	do {
		gmu_core_regread(device,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg[0]);

		if (reg[0] & GX_GDSC_POWER_OFF)
			return 0;

		/* Wait 10us to reduce unnecessary AHB bus traffic */
		usleep_range(10, 100);
	} while (!time_after(jiffies, t));

	ts2 = a6xx_read_alwayson(ADRENO_DEVICE(device));

	/* Do one last read incase it succeeds */
	gmu_core_regread(device,
		A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg[0]);

	if (reg[0] & GX_GDSC_POWER_OFF)
		return 0;

	ts3 = a6xx_read_alwayson(ADRENO_DEVICE(device));

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
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	unsigned int status;
	int i;

	switch (boot_state) {
	case GMU_COLD_BOOT:
	case GMU_WARM_BOOT:
		/* Turn on TCM retention */
		gmu_core_regwrite(device, A6XX_GMU_GENERAL_7, 1);

		/* Load RGMU FW image via AHB bus */
		for (i = 0; i < rgmu->fw_size; i++)
			gmu_core_regwrite(device, A6XX_GMU_CM3_ITCM_START + i,
					rgmu->fw_hostptr[i]);
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

static int a6xx_rgmu_suspend_device(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	int ret = 0;

	/* Check GX GDSC is status */
	if (a6xx_rgmu_gx_is_on(device)) {

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

		if (a6xx_rgmu_gx_is_on(device))
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
static int a6xx_rgmu_gpu_pwrctrl(struct kgsl_device *device,
		unsigned int mode, unsigned int arg1, unsigned int arg2)
{
	int ret = 0;

	switch (mode) {
	case GMU_FW_START:
		ret = a6xx_rgmu_fw_start(device, arg1);
		break;
	case GMU_SUSPEND:
		ret = a6xx_rgmu_suspend_device(device);
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

static void a6xx_rgmu_disable_clks(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	int  ret;

	/* Check GX GDSC is status */
	if (a6xx_rgmu_gx_is_on(device)) {

		if (IS_ERR_OR_NULL(rgmu->gx_gdsc))
			return;

		/*
		 * Switch gx gdsc control from RGMU to CPU. Force non-zero
		 * reference count in clk driver so next disable call will
		 * turn off the GDSC.
		 */
		ret = regulator_enable(rgmu->gx_gdsc);
		if (ret)
			dev_err(&rgmu->pdev->dev,
					"Fail to enable gx gdsc:%d\n", ret);

		ret = regulator_disable(rgmu->gx_gdsc);
		if (ret)
			dev_err(&rgmu->pdev->dev,
					"Fail to disable gx gdsc:%d\n", ret);

		if (a6xx_rgmu_gx_is_on(device))
			dev_err(&rgmu->pdev->dev, "gx is stuck on\n");
	}

	clk_bulk_disable_unprepare(rgmu->num_clks, rgmu->clks);

	clear_bit(GMU_CLK_ON, &device->gmu_core.flags);
}

static int a6xx_rgmu_disable_gdsc(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);

	/* Wait up to 5 seconds for the regulator to go off */
	if (kgsl_regulator_disable_wait(rgmu->cx_gdsc, 5000))
		return 0;

	dev_err(&rgmu->pdev->dev, "RGMU CX gdsc off timeout\n");
	return -ETIMEDOUT;
}


static void a6xx_rgmu_halt_execution(struct kgsl_device *device);

static void a6xx_rgmu_snapshot(struct kgsl_device *device)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);

	/* Mask so there's no interrupt caused by NMI */
	adreno_write_gmureg(adreno_dev,
			ADRENO_REG_GMU_GMU2HOST_INTR_MASK, 0xFFFFFFFF);

	/* Make sure the interrupt is masked */
	wmb();

	/*
	 * Halt RGMU execution so that GX will not
	 * be collapsed while dumping snapshot.
	 */
	a6xx_rgmu_halt_execution(device);

	kgsl_device_snapshot(device, NULL, true);

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, 0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK,
		RGMU_OOB_IRQ_MASK);

	rgmu->fault_count++;
}

static int a6xx_rgmu_suspend(struct kgsl_device *device)
{
	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return 0;

	a6xx_rgmu_irq_disable(device);

	if (a6xx_rgmu_gpu_pwrctrl(device, GMU_SUSPEND, 0, 0))
		return -EINVAL;

	a6xx_rgmu_disable_clks(device);
	a6xx_rgmu_disable_gdsc(device);
	return 0;
}

static int a6xx_rgmu_enable_clks(struct kgsl_device *device)
{
	int ret;
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	struct kgsl_pwrctrl *pwr = &device->pwrctrl;

	ret = clk_set_rate(rgmu->rgmu_clk, RGMU_CLK_FREQ);
	if (ret) {
		dev_err(&rgmu->pdev->dev, "Couldn't set the RGMU clock\n");
		return ret;
	}

	ret = clk_set_rate(rgmu->gpu_clk,
		pwr->pwrlevels[pwr->default_pwrlevel].gpu_freq);
	if (ret) {
		dev_err(&rgmu->pdev->dev, "Couldn't set the GPU clock\n");
		return ret;
	}

	ret = clk_bulk_prepare_enable(rgmu->num_clks, rgmu->clks);
	if (ret) {
		dev_err(&rgmu->pdev->dev, "Failed to enable RGMU clocks\n");
		return ret;
	}

	set_bit(GMU_CLK_ON, &device->gmu_core.flags);
	return 0;
}

static int a6xx_rgmu_enable_gdsc(struct a6xx_rgmu_device *rgmu)
{
	int ret;

	if (IS_ERR_OR_NULL(rgmu->cx_gdsc))
		return 0;

	ret = regulator_enable(rgmu->cx_gdsc);
	if (ret)
		dev_err(&rgmu->pdev->dev,
			"Fail to enable CX gdsc:%d\n", ret);

	return ret;
}

/* To be called to power on both GPU and RGMU */
static int a6xx_rgmu_start(struct kgsl_device *device)
{
	int ret = 0;
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);

	switch (device->state) {
	case KGSL_STATE_RESET:
		ret = a6xx_rgmu_suspend(device);
		if (ret)
			goto error_rgmu;
		/* Fall-through */
	case KGSL_STATE_INIT:
	case KGSL_STATE_SUSPEND:
	case KGSL_STATE_SLUMBER:
		a6xx_rgmu_enable_gdsc(rgmu);
		a6xx_rgmu_enable_clks(device);
		a6xx_rgmu_irq_enable(device);
		ret = a6xx_rgmu_gpu_pwrctrl(device,
				GMU_FW_START, GMU_COLD_BOOT, 0);
		if (ret)
			goto error_rgmu;
		break;
	}
	/* Request default DCVS level */
	kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
	return 0;

error_rgmu:
	set_bit(GMU_FAULT, &device->gmu_core.flags);
	a6xx_rgmu_snapshot(device);
	return ret;
}



/*
 * a6xx_rgmu_load_firmware() - Load the ucode into the RGMU TCM
 * @device: Pointer to KGSL device
 */
static int a6xx_rgmu_load_firmware(struct kgsl_device *device)
{
	const struct firmware *fw = NULL;
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	const struct adreno_a6xx_core *a6xx_core = to_a6xx_core(adreno_dev);
	int ret;

	/* RGMU fw already saved and verified so do nothing new */
	if (rgmu->fw_hostptr)
		return 0;

	ret = request_firmware(&fw, a6xx_core->gmufw_name, &rgmu->pdev->dev);
	if (ret < 0) {
		dev_err(&rgmu->pdev->dev, "request_firmware (%s) failed: %d\n",
				a6xx_core->gmufw_name, ret);
		return ret;
	}

	rgmu->fw_hostptr = devm_kmemdup(&rgmu->pdev->dev, fw->data,
					fw->size, GFP_KERNEL);

	if (rgmu->fw_hostptr)
		rgmu->fw_size = (fw->size / sizeof(u32));

	release_firmware(fw);
	return rgmu->fw_hostptr ? 0 : -ENOMEM;
}

static int a6xx_rgmu_init(struct kgsl_device *device)
{
	return a6xx_rgmu_load_firmware(device);
}

/* Halt RGMU execution */
static void a6xx_rgmu_halt_execution(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
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
static void a6xx_rgmu_device_snapshot(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot)
{

	adreno_snapshot_registers(device, snapshot, a6xx_rgmu_registers,
					ARRAY_SIZE(a6xx_rgmu_registers) / 2);
}

static u64 a6xx_rgmu_read_alwayson(struct kgsl_device *device)
{
	return a6xx_read_alwayson(ADRENO_DEVICE(device));
}

/* Caller shall ensure GPU is ready for SLUMBER */
static void a6xx_rgmu_stop(struct kgsl_device *device)
{
	if (!test_bit(GMU_CLK_ON, &device->gmu_core.flags))
		return;

	/* Wait for the lowest idle level we requested */
	if (a6xx_rgmu_wait_for_lowest_idle(device))
		goto error;

	a6xx_rgmu_gpu_pwrctrl(device,
			GMU_NOTIFY_SLUMBER, 0, 0);

	a6xx_rgmu_irq_disable(device);
	a6xx_rgmu_disable_clks(device);
	a6xx_rgmu_disable_gdsc(device);
	return;

error:

	/*
	 * The power controller will change state to SLUMBER anyway
	 * Set GMU_FAULT flag to indicate to power contrller
	 * that hang recovery is needed to power on GPU
	 */
	set_bit(GMU_FAULT, &device->gmu_core.flags);
	a6xx_rgmu_snapshot(device);
}

/*
 * a6xx_rgmu_dcvs_set() - Change GPU frequency and/or bandwidth.
 * @rgmu: Pointer to RGMU device
 * @pwrlevel: index to GPU DCVS table used by KGSL
 * @bus_level: index to GPU bus table used by KGSL
 *
 * The function converts GPU power level and bus level index used by KGSL
 * to index being used by GMU/RPMh.
 */
static int a6xx_rgmu_dcvs_set(struct kgsl_device *device,
		int pwrlevel, int bus_level)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	int ret;
	unsigned long rate;

	if (pwrlevel == INVALID_DCVS_IDX)
		return -EINVAL;

	rate = device->pwrctrl.pwrlevels[pwrlevel].gpu_freq;

	ret = clk_set_rate(rgmu->gpu_clk, rate);
	if (ret)
		dev_err(&rgmu->pdev->dev, "Couldn't set the GPU clock\n");

	return ret;
}

static struct gmu_dev_ops a6xx_rgmudev = {
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
	.snapshot = a6xx_rgmu_device_snapshot,
	.halt_execution = a6xx_rgmu_halt_execution,
	.read_alwayson = a6xx_rgmu_read_alwayson,
	.gmu2host_intr_mask = RGMU_OOB_IRQ_MASK,
	.gmu_ao_intr_mask = RGMU_AO_IRQ_MASK,
};

static struct gmu_core_ops a6xx_rgmu_ops = {
	.init = a6xx_rgmu_init,
	.start = a6xx_rgmu_start,
	.stop = a6xx_rgmu_stop,
	.dcvs_set = a6xx_rgmu_dcvs_set,
	.snapshot = a6xx_rgmu_snapshot,
	.suspend = a6xx_rgmu_suspend,
};

static int a6xx_rgmu_irq_probe(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = A6XX_RGMU_DEVICE(device);
	int ret;

	ret = kgsl_request_irq(rgmu->pdev, "kgsl_oob",
			a6xx_oob_irq_handler, device);
	if (ret < 0)
		return ret;

	rgmu->oob_interrupt_num  = ret;

	ret = kgsl_request_irq(rgmu->pdev,
		"kgsl_rgmu", a6xx_rgmu_irq_handler, device);
	if (ret < 0)
		return ret;

	rgmu->rgmu_interrupt_num = ret;
	return 0;
}

static int a6xx_rgmu_regulators_probe(struct a6xx_rgmu_device *rgmu)
{
	int ret = 0;

	rgmu->cx_gdsc = devm_regulator_get(&rgmu->pdev->dev, "vddcx");
	if (IS_ERR(rgmu->cx_gdsc)) {
		ret = PTR_ERR(rgmu->cx_gdsc);
		if (ret != -EPROBE_DEFER)
			dev_err(&rgmu->pdev->dev,
				"Couldn't get CX gdsc error:%d\n", ret);
		return ret;
	}

	rgmu->gx_gdsc = devm_regulator_get(&rgmu->pdev->dev, "vdd");
	if (IS_ERR(rgmu->gx_gdsc)) {
		ret = PTR_ERR(rgmu->gx_gdsc);
		if (ret != -EPROBE_DEFER)
			dev_err(&rgmu->pdev->dev,
				"Couldn't get GX gdsc error:%d\n", ret);
	}

	return ret;
}

static int a6xx_rgmu_clocks_probe(struct a6xx_rgmu_device *rgmu,
		struct device_node *node)
{
	int ret;

	ret = devm_clk_bulk_get_all(&rgmu->pdev->dev, &rgmu->clks);
	if (ret < 0)
		return ret;

	rgmu->num_clks = ret;

	rgmu->gpu_clk = kgsl_of_clk_by_name(rgmu->clks, ret, "core");
	if (!rgmu->gpu_clk) {
		dev_err(&rgmu->pdev->dev, "The GPU clock isn't defined\n");
		return -ENODEV;
	}

	rgmu->rgmu_clk = kgsl_of_clk_by_name(rgmu->clks, ret, "gmu");
	if (!rgmu->rgmu_clk) {
		dev_err(&rgmu->pdev->dev, "The RGMU clock isn't defined\n");
		return -ENODEV;
	}

	return 0;
}

/* Do not access any RGMU registers in RGMU probe function */
static int a6xx_rgmu_probe(struct kgsl_device *device,
		struct platform_device *pdev)
{
	struct a6xx_rgmu_device *rgmu;
	struct resource *res;
	int ret = -ENXIO;

	rgmu = devm_kzalloc(&pdev->dev, sizeof(*rgmu), GFP_KERNEL);

	if (rgmu == NULL)
		return -ENOMEM;

	rgmu->pdev = pdev;

	/* Set up RGMU regulators */
	ret = a6xx_rgmu_regulators_probe(rgmu);
	if (ret)
		return ret;

	/* Set up RGMU clocks */
	ret = a6xx_rgmu_clocks_probe(rgmu, pdev->dev.of_node);
	if (ret)
		return ret;

	/* Map and reserve RGMU CSRs registers */
	res = platform_get_resource_byname(pdev, IORESOURCE_MEM, "kgsl_rgmu");
	if (!res) {
		dev_err(&pdev->dev, "The RGMU register region isn't defined\n");
		return -ENODEV;
	}

	device->gmu_core.gmu2gpu_offset = (res->start - device->reg_phys) >> 2;
	device->gmu_core.reg_len = resource_size(res);
	device->gmu_core.reg_virt = devm_ioremap_resource(&pdev->dev, res);

	if (IS_ERR(device->gmu_core.reg_virt)) {
		dev_err(&pdev->dev, "Unable to map the RGMU registers\n");
		return PTR_ERR(device->gmu_core.reg_virt);
	}

	device->gmu_core.ptr = (void *)rgmu;

	/* Initialize OOB and RGMU interrupts */
	ret = a6xx_rgmu_irq_probe(device);
	if (ret)
		return ret;

	/* Don't enable RGMU interrupts until RGMU started */
	/* We cannot use rgmu_irq_disable because it writes registers */
	disable_irq(rgmu->rgmu_interrupt_num);
	disable_irq(rgmu->oob_interrupt_num);

	/* Set up RGMU idle states */
	if (ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_IFPC))
		rgmu->idle_level = GPU_HW_IFPC;
	else
		rgmu->idle_level = GPU_HW_ACTIVE;

	set_bit(GMU_ENABLED, &device->gmu_core.flags);
	device->gmu_core.core_ops = &a6xx_rgmu_ops;
	device->gmu_core.dev_ops = &a6xx_rgmudev;

	return 0;
}

static void a6xx_rgmu_remove(struct kgsl_device *device)
{
	a6xx_rgmu_stop(device);

	memset(&device->gmu_core, 0, sizeof(device->gmu_core));
}

static int a6xx_rgmu_bind(struct device *dev, struct device *master, void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);

	return a6xx_rgmu_probe(device, to_platform_device(dev));
}

static void a6xx_rgmu_unbind(struct device *dev, struct device *master,
		void *data)
{
	struct kgsl_device *device = dev_get_drvdata(master);

	a6xx_rgmu_remove(device);
}

static const struct component_ops a6xx_rgmu_component_ops = {
	.bind = a6xx_rgmu_bind,
	.unbind = a6xx_rgmu_unbind,
};

static int a6xx_rgmu_probe_dev(struct platform_device *pdev)
{
	return component_add(&pdev->dev, &a6xx_rgmu_component_ops);
}

static int a6xx_rgmu_remove_dev(struct platform_device *pdev)
{
	component_del(&pdev->dev, &a6xx_rgmu_component_ops);
	return 0;
}

static const struct of_device_id a6xx_rgmu_match_table[] = {
	{ .compatible = "qcom,gpu-rgmu" },
	{ },
};

struct platform_driver a6xx_rgmu_driver = {
	.probe = a6xx_rgmu_probe_dev,
	.remove = a6xx_rgmu_remove_dev,
	.driver = {
		.name = "kgsl-rgmu",
		.of_match_table = a6xx_rgmu_match_table,
	},
};
