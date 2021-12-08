// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */

#include <linux/clk-provider.h>
#include <linux/component.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/regulator/consumer.h>
#include <linux/soc/qcom/llcc-qcom.h>

#include "adreno.h"
#include "adreno_a6xx.h"
#include "adreno_a6xx_rgmu.h"
#include "adreno_snapshot.h"
#include "kgsl_bus.h"
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

static struct a6xx_rgmu_device *to_a6xx_rgmu(struct adreno_device *adreno_dev)
{
	struct a6xx_device *a6xx_dev = container_of(adreno_dev,
					struct a6xx_device, adreno_dev);

	return &a6xx_dev->rgmu;
}

static void a6xx_rgmu_active_count_put(struct adreno_device *adreno_dev)
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
		kgsl_start_idle_timer(device);
	}

	trace_kgsl_active_count(device,
		(unsigned long) __builtin_return_address(0));

	wake_up(&device->active_cnt_wq);
}

static irqreturn_t a6xx_rgmu_irq_handler(int irq, void *data)
{
	struct kgsl_device *device = data;
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(ADRENO_DEVICE(device));
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
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	unsigned int status = 0;

	gmu_core_regread(device, A6XX_GMU_GMU2HOST_INTR_INFO, &status);

	if (status & RGMU_OOB_IRQ_ERR_MSG) {
		gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, status);

		dev_err_ratelimited(&rgmu->pdev->dev,
				"RGMU oob irq error\n");
		adreno_dispatcher_fault(adreno_dev, ADRENO_GMU_FAULT);
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
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(ADRENO_DEVICE(device));
	int ret, set, check;

	if (req == oob_perfcntr && rgmu->num_oob_perfcntr++)
		return 0;

	set = BIT(req + 16);
	check = BIT(req + 16);

	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, set);

	ret = gmu_core_timed_poll_check(device,
			A6XX_GMU_GMU2HOST_INTR_INFO,
			check,
			GPU_START_TIMEOUT,
			check);

	if (ret) {
		unsigned int status;

		if (req == oob_perfcntr)
			rgmu->num_oob_perfcntr--;
		gmu_core_regread(device, A6XX_RGMU_CX_PCC_DEBUG, &status);
		dev_err(&rgmu->pdev->dev,
				"Timed out while setting OOB req:%s status:0x%x\n",
				oob_to_str(req), status);
		gmu_core_fault_snapshot(device);
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
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(ADRENO_DEVICE(device));

	if (req == oob_perfcntr && --rgmu->num_oob_perfcntr)
		return;

	gmu_core_regwrite(device, A6XX_GMU_HOST2GMU_INTR_SET, BIT(req + 24));
	trace_kgsl_gmu_oob_clear(BIT(req + 24));
}

static void a6xx_rgmu_bcl_config(struct kgsl_device *device, bool on)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(ADRENO_DEVICE(device));

	if (on) {
		/* Enable BCL CRC HW i/f */
		gmu_core_regwrite(device,
				A6XX_GMU_AO_RGMU_GLM_HW_CRC_DISABLE, 0);
	} else {
		/* Disable CRC HW i/f */
		gmu_core_regwrite(device,
				A6XX_GMU_AO_RGMU_GLM_HW_CRC_DISABLE, 1);

		/* Wait for HW CRC disable ACK */
		if (gmu_core_timed_poll_check(device,
				A6XX_GMU_AO_RGMU_GLM_SLEEP_STATUS,
				BIT(1), GLM_SLEEP_TIMEOUT, BIT(1)))
			dev_err_ratelimited(&rgmu->pdev->dev,
				"Timed out waiting for HW CRC disable acknowledgment\n");

		/* Pull down the valid RGMU_GLM_SLEEP_CTRL[7] to 0 */
		gmu_core_regrmw(device, A6XX_GMU_AO_RGMU_GLM_SLEEP_CTRL,
				BIT(7), 0);

	}
}

static void a6xx_rgmu_irq_enable(struct adreno_device *adreno_dev)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Clear pending IRQs and Unmask needed IRQs */
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, 0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_CLR, 0xffffffff);

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK,
		~((unsigned int)RGMU_OOB_IRQ_MASK));
	gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_MASK,
		(unsigned int)~RGMU_AO_IRQ_MASK);

	/* Enable all IRQs on host */
	enable_irq(rgmu->oob_interrupt_num);
	enable_irq(rgmu->rgmu_interrupt_num);
}

static void a6xx_rgmu_irq_disable(struct adreno_device *adreno_dev)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Disable all IRQs on host */
	disable_irq(rgmu->rgmu_interrupt_num);
	disable_irq(rgmu->oob_interrupt_num);

	/* Mask all IRQs and clear pending IRQs */
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK, 0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_MASK, 0xffffffff);

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, 0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_AO_HOST_INTERRUPT_CLR, 0xffffffff);
}

static int a6xx_rgmu_ifpc_store(struct kgsl_device *device,
		unsigned int val)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	unsigned int requested_idle_level;

	if (!ADRENO_FEATURE(adreno_dev, ADRENO_IFPC))
		return -EINVAL;

	if (val)
		requested_idle_level = GPU_HW_IFPC;
	else
		requested_idle_level = GPU_HW_ACTIVE;

	if (requested_idle_level == rgmu->idle_level)
		return 0;

	/* Power cycle the GPU for changes to take effect */
	return adreno_power_cycle_u32(adreno_dev, &rgmu->idle_level,
		requested_idle_level);
}

static unsigned int a6xx_rgmu_ifpc_show(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(ADRENO_DEVICE(device));

	return rgmu->idle_level == GPU_HW_IFPC;
}


static void a6xx_rgmu_prepare_stop(struct kgsl_device *device)
{
	/* Turn off GX_MEM retention */
	kgsl_regwrite(device, A6XX_RBBM_BLOCK_GX_RETENTION_CNTL, 0);
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

static int a6xx_rgmu_wait_for_lowest_idle(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	unsigned int reg[10] = {0};
	unsigned long t;
	uint64_t ts1, ts2, ts3;

	if (rgmu->idle_level != GPU_HW_IFPC)
		return 0;

	ts1 = a6xx_read_alwayson(adreno_dev);

	/* FIXME: readl_poll_timeout? */
	t = jiffies + msecs_to_jiffies(RGMU_IDLE_TIMEOUT);
	do {
		gmu_core_regread(device,
			A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg[0]);

		if (reg[0] & GX_GDSC_POWER_OFF)
			return 0;

		/* Wait 10us to reduce unnecessary AHB bus traffic */
		usleep_range(10, 100);
	} while (!time_after(jiffies, t));

	ts2 = a6xx_read_alwayson(adreno_dev);

	/* Do one last read incase it succeeds */
	gmu_core_regread(device,
		A6XX_GMU_SPTPRAC_PWR_CLK_STATUS, &reg[0]);

	if (reg[0] & GX_GDSC_POWER_OFF)
		return 0;

	ts3 = a6xx_read_alwayson(adreno_dev);

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
	gmu_core_fault_snapshot(device);
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

static int a6xx_rgmu_fw_start(struct adreno_device *adreno_dev,
		unsigned int boot_state)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
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

	if (gmu_core_timed_poll_check(device, A6XX_RGMU_CX_PCC_INIT_RESULT,
			BIT(0), RGMU_START_TIMEOUT, BIT(0))) {
		gmu_core_regread(device, A6XX_RGMU_CX_PCC_DEBUG, &status);
		dev_err(&rgmu->pdev->dev,
				"rgmu boot Failed. status:%08x\n", status);
		gmu_core_fault_snapshot(device);
		return -ETIMEDOUT;
	}

	/* Read the RGMU firmware version from registers */
	gmu_core_regread(device, A6XX_GMU_GENERAL_0, &rgmu->ver);

	return 0;
}

static void a6xx_rgmu_notify_slumber(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Disable the power counter so that the RGMU is not busy */
	gmu_core_regwrite(device, A6XX_GMU_CX_GMU_POWER_COUNTER_ENABLE, 0);

	/* BCL OFF Sequence */
	a6xx_rgmu_bcl_config(device, false);
}

static void a6xx_rgmu_disable_clks(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
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
}

static int a6xx_rgmu_disable_gdsc(struct adreno_device *adreno_dev)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Wait up to 5 seconds for the regulator to go off */
	if (kgsl_regulator_disable_wait(rgmu->cx_gdsc, 5000))
		return 0;

	dev_err(&rgmu->pdev->dev, "RGMU CX gdsc off timeout\n");

	device->state = KGSL_STATE_NONE;

	return -ETIMEDOUT;
}


static void a6xx_rgmu_halt_execution(struct kgsl_device *device);

void a6xx_rgmu_snapshot(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);

	/*
	 * Halt RGMU execution so that GX will not
	 * be collapsed while dumping snapshot.
	 */
	a6xx_rgmu_halt_execution(device);

	adreno_snapshot_registers(device, snapshot, a6xx_rgmu_registers,
			ARRAY_SIZE(a6xx_rgmu_registers) / 2);

	a6xx_snapshot(adreno_dev, snapshot);

	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_CLR, 0xffffffff);
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK,
		RGMU_OOB_IRQ_MASK);

	if (device->gmu_fault)
		rgmu->fault_count++;
}

static void a6xx_rgmu_suspend(struct adreno_device *adreno_dev)
{
	a6xx_rgmu_irq_disable(adreno_dev);

	a6xx_rgmu_disable_clks(adreno_dev);
	a6xx_rgmu_disable_gdsc(adreno_dev);
}

static int a6xx_rgmu_enable_clks(struct adreno_device *adreno_dev)
{
	int ret;
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
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

	device->state = KGSL_STATE_AWARE;

	return 0;
}

static int a6xx_rgmu_enable_gdsc(struct adreno_device *adreno_dev)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	int ret;

	if (IS_ERR_OR_NULL(rgmu->cx_gdsc))
		return 0;

	ret = regulator_enable(rgmu->cx_gdsc);
	if (ret)
		dev_err(&rgmu->pdev->dev,
			"Fail to enable CX gdsc:%d\n", ret);

	return ret;
}

/*
 * a6xx_rgmu_load_firmware() - Load the ucode into the RGMU TCM
 * @adreno_dev: Pointer to adreno device
 */
static int a6xx_rgmu_load_firmware(struct adreno_device *adreno_dev)
{
	const struct firmware *fw = NULL;
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
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

/* Halt RGMU execution */
static void a6xx_rgmu_halt_execution(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(ADRENO_DEVICE(device));
	unsigned int index, status, fence;

	if (!device->gmu_fault)
		return;

	/* Mask so there's no interrupt caused by NMI */
	gmu_core_regwrite(device, A6XX_GMU_GMU2HOST_INTR_MASK, 0xFFFFFFFF);

	/* Make sure the interrupt is masked */
	wmb();

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

static void halt_gbif_arb(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);

	/* Halt all AXI requests */
	kgsl_regwrite(device, A6XX_GBIF_HALT, A6XX_GBIF_ARB_HALT_MASK);
	adreno_wait_for_halt_ack(device, A6XX_GBIF_HALT_ACK,
		A6XX_GBIF_ARB_HALT_MASK);

	/* De-assert the halts */
	kgsl_regwrite(device, A6XX_GBIF_HALT, 0x0);
}


/* Caller shall ensure GPU is ready for SLUMBER */
static void a6xx_rgmu_power_off(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	kgsl_pwrctrl_axi(device, false);

	if (device->gmu_fault)
		return a6xx_rgmu_suspend(adreno_dev);

	/* Wait for the lowest idle level we requested */
	ret = a6xx_rgmu_wait_for_lowest_idle(adreno_dev);
	if (ret)
		return a6xx_rgmu_suspend(adreno_dev);

	a6xx_rgmu_notify_slumber(adreno_dev);

	/* Halt CX traffic and de-assert halt */
	halt_gbif_arb(adreno_dev);

	a6xx_rgmu_irq_disable(adreno_dev);
	a6xx_rgmu_disable_clks(adreno_dev);
	a6xx_rgmu_disable_gdsc(adreno_dev);

	kgsl_pwrctrl_clear_l3_vote(device);
}

static int a6xx_rgmu_clock_set(struct adreno_device *adreno_dev,
		u32 pwrlevel)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
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

static int a6xx_gpu_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	/* Clear any GPU faults that might have been left over */
	adreno_clear_gpu_fault(adreno_dev);

	adreno_set_active_ctxs_null(adreno_dev);

	ret = kgsl_mmu_start(device);
	if (ret)
		goto err;

	ret = a6xx_rgmu_oob_set(device, oob_gpu);
	if (ret)
		goto err_oob_clear;

	/* Clear the busy_data stats - we're starting over from scratch */
	memset(&adreno_dev->busy_data, 0, sizeof(adreno_dev->busy_data));

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
		goto err_oob_clear;
	}

	/* Start the dispatcher */
	adreno_dispatcher_start(device);

	device->reset_counter++;

	a6xx_rgmu_oob_clear(device, oob_gpu);

	return 0;

err_oob_clear:
	a6xx_rgmu_oob_clear(device, oob_gpu);

err:
	a6xx_rgmu_power_off(adreno_dev);

	return ret;
}

static int a6xx_rgmu_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_AWARE);

	ret = a6xx_rgmu_enable_gdsc(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_rgmu_enable_clks(adreno_dev);
	if (ret) {
		a6xx_rgmu_disable_gdsc(adreno_dev);
		return ret;
	}

	a6xx_rgmu_irq_enable(adreno_dev);

	ret = a6xx_rgmu_fw_start(adreno_dev, GMU_COLD_BOOT);
	if (ret)
		goto err;

	/* Request default DCVS level */
	ret = kgsl_pwrctrl_set_default_gpu_pwrlevel(device);
	if (ret)
		goto err;

	ret = kgsl_pwrctrl_axi(device, true);
	if (ret)
		goto err;

	device->gmu_fault = false;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_AWARE);

	return 0;

err:
	a6xx_rgmu_power_off(adreno_dev);

	return ret;
}

static int a6xx_power_off(struct adreno_device *adreno_dev);

static void rgmu_idle_check(struct work_struct *work)
{
	struct kgsl_device *device = container_of(work,
					struct kgsl_device, idle_check_ws);
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);

	mutex_lock(&device->mutex);

	if (test_bit(GMU_DISABLE_SLUMBER, &device->gmu_core.flags))
		goto done;

	if (!atomic_read(&device->active_cnt)) {
		a6xx_power_off(adreno_dev);
	} else {
		kgsl_pwrscale_update(device);
		kgsl_start_idle_timer(device);
	}

done:
	mutex_unlock(&device->mutex);
}

static void rgmu_idle_timer(struct timer_list *t)
{
	struct kgsl_device *device = container_of(t, struct kgsl_device,
					idle_timer);

	kgsl_schedule_work(&device->idle_check_ws);
}

static int a6xx_boot(struct adreno_device *adreno_dev)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	if (test_bit(RGMU_PRIV_GPU_STARTED, &rgmu->flags))
		return 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	ret = a6xx_rgmu_boot(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gpu_boot(adreno_dev);
	if (ret)
		return ret;

	kgsl_start_idle_timer(device);

	kgsl_pwrscale_wake(device);

	set_bit(RGMU_PRIV_GPU_STARTED, &rgmu->flags);

	device->pwrctrl.last_stat_updated = ktime_get();
	device->state = KGSL_STATE_ACTIVE;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_ACTIVE);

	return 0;
}

static void a6xx_rgmu_touch_wakeup(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	int ret;

	/*
	 * Do not wake up a suspended device or until the first boot sequence
	 * has been completed.
	 */
	if (test_bit(RGMU_PRIV_PM_SUSPEND, &rgmu->flags) ||
		!test_bit(RGMU_PRIV_FIRST_BOOT_DONE, &rgmu->flags))
		return;

	if (test_bit(RGMU_PRIV_GPU_STARTED, &rgmu->flags))
		goto done;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	ret = a6xx_rgmu_boot(adreno_dev);
	if (ret)
		return;

	ret = a6xx_gpu_boot(adreno_dev);
	if (ret)
		return;

	kgsl_pwrscale_wake(device);

	set_bit(RGMU_PRIV_GPU_STARTED, &rgmu->flags);

	device->pwrctrl.last_stat_updated = ktime_get();
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

static int a6xx_first_boot(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	int ret;

	if (test_bit(RGMU_PRIV_FIRST_BOOT_DONE, &rgmu->flags))
		return a6xx_boot(adreno_dev);

	ret = a6xx_ringbuffer_init(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_microcode_read(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_init(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_rgmu_load_firmware(adreno_dev);
	if (ret)
		return ret;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_ACTIVE);

	ret = a6xx_rgmu_boot(adreno_dev);
	if (ret)
		return ret;

	ret = a6xx_gpu_boot(adreno_dev);
	if (ret)
		return ret;

	adreno_get_bus_counters(adreno_dev);

	adreno_create_profile_buffer(adreno_dev);

	set_bit(RGMU_PRIV_FIRST_BOOT_DONE, &rgmu->flags);
	set_bit(RGMU_PRIV_GPU_STARTED, &rgmu->flags);

	/*
	 * There is a possible deadlock scenario during kgsl firmware reading
	 * (request_firmware) and devfreq update calls. During first boot, kgsl
	 * device mutex is held and then request_firmware is called for reading
	 * firmware. request_firmware internally takes dev_pm_qos_mtx lock.
	 * Whereas in case of devfreq update calls triggered by thermal/bcl or
	 * devfreq sysfs, it first takes the same dev_pm_qos_mtx lock and then
	 * tries to take kgsl device mutex as part of get_dev_status/target
	 * calls. This results in deadlock when both thread are unable to acquire
	 * the mutex held by other thread. Enable devfreq updates now as we are
	 * done reading all firmware files.
	 */
	device->pwrscale.devfreq_enabled = true;

	device->pwrctrl.last_stat_updated = ktime_get();
	device->state = KGSL_STATE_ACTIVE;

	trace_kgsl_pwr_set_state(device, KGSL_STATE_ACTIVE);

	return 0;
}

static int a6xx_rgmu_first_open(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	int ret;

	ret = a6xx_first_boot(adreno_dev);
	if (ret)
		return ret;

	/*
	 * A client that does a first_open but never closes the device
	 * may prevent us from going back to SLUMBER. So trigger the idle
	 * check by incrementing the active count and immediately releasing it.
	 */
	atomic_inc(&device->active_cnt);
	a6xx_rgmu_active_count_put(adreno_dev);

	return 0;
}

static int a6xx_power_off(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	int ret;

	adreno_suspend_context(device);

	/*
	 * adreno_suspend_context() unlocks the device mutex, which
	 * could allow a concurrent thread to attempt SLUMBER sequence.
	 * Hence, check the flags before proceeding with SLUMBER.
	 */
	if (!test_bit(RGMU_PRIV_GPU_STARTED, &rgmu->flags))
		return 0;

	trace_kgsl_pwr_request_state(device, KGSL_STATE_SLUMBER);

	ret = a6xx_rgmu_oob_set(device, oob_gpu);
	if (ret) {
		a6xx_rgmu_oob_clear(device, oob_gpu);
		goto no_gx_power;
	}

	kgsl_pwrscale_update_stats(device);

	/* Save active coresight registers if applicable */
	adreno_coresight_stop(adreno_dev);

	/* Save physical performance counter values before GPU power down*/
	adreno_perfcounter_save(adreno_dev);

	adreno_irqctrl(adreno_dev, 0);

	a6xx_rgmu_prepare_stop(device);

	a6xx_rgmu_oob_clear(device, oob_gpu);

no_gx_power:
	/* Halt all gx traffic */
	kgsl_regwrite(device, A6XX_GBIF_HALT, A6XX_GBIF_CLIENT_HALT_MASK);

	adreno_wait_for_halt_ack(device, A6XX_GBIF_HALT_ACK,
		A6XX_GBIF_CLIENT_HALT_MASK);

	kgsl_pwrctrl_irq(device, false);

	a6xx_rgmu_power_off(adreno_dev);

	adreno_set_active_ctxs_null(adreno_dev);

	adreno_dispatcher_stop(adreno_dev);

	adreno_ringbuffer_stop(adreno_dev);

	if (!IS_ERR_OR_NULL(adreno_dev->gpu_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpu_llc_slice);

	if (!IS_ERR_OR_NULL(adreno_dev->gpuhtw_llc_slice))
		llcc_slice_deactivate(adreno_dev->gpuhtw_llc_slice);

	clear_bit(RGMU_PRIV_GPU_STARTED, &rgmu->flags);

	del_timer_sync(&device->idle_timer);

	kgsl_pwrscale_sleep(device);

	trace_kgsl_pwr_set_state(device, KGSL_STATE_SLUMBER);

	return ret;
}

int a6xx_rgmu_reset(struct adreno_device *adreno_dev)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);

	a6xx_disable_gpu_irq(adreno_dev);

	/* Hard reset the rgmu and gpu */
	a6xx_rgmu_suspend(adreno_dev);

	a6xx_reset_preempt_records(adreno_dev);

	clear_bit(RGMU_PRIV_GPU_STARTED, &rgmu->flags);

	/* Attempt rebooting the rgmu and gpu */
	return a6xx_boot(adreno_dev);
}

static int a6xx_rgmu_active_count_get(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	int ret = 0;

	if (WARN_ON(!mutex_is_locked(&device->mutex)))
		return -EINVAL;

	if (test_bit(RGMU_PRIV_PM_SUSPEND, &rgmu->flags))
		return -EINVAL;

	if (atomic_read(&device->active_cnt) == 0)
		ret = a6xx_boot(adreno_dev);

	if (ret == 0) {
		atomic_inc(&device->active_cnt);
		trace_kgsl_active_count(device,
			(unsigned long) __builtin_return_address(0));
	}

	return ret;
}

static int a6xx_rgmu_pm_suspend(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	int ret;

	if (test_bit(RGMU_PRIV_PM_SUSPEND, &rgmu->flags))
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

	a6xx_power_off(adreno_dev);

	set_bit(RGMU_PRIV_PM_SUSPEND, &rgmu->flags);

	adreno_get_gpu_halt(adreno_dev);

	trace_kgsl_pwr_set_state(device, KGSL_STATE_SUSPEND);

	return 0;
err:
	adreno_dispatcher_start(device);
	return ret;
}

static void a6xx_rgmu_pm_resume(struct adreno_device *adreno_dev)
{
	struct kgsl_device *device = KGSL_DEVICE(adreno_dev);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);

	if (WARN(!test_bit(GMU_PRIV_PM_SUSPEND, &rgmu->flags),
		"resume invoked without a suspend\n"))
		return;

	adreno_put_gpu_halt(adreno_dev);

	clear_bit(RGMU_PRIV_PM_SUSPEND, &rgmu->flags);

	adreno_dispatcher_start(device);
}

static const struct gmu_dev_ops a6xx_rgmudev = {
	.oob_set = a6xx_rgmu_oob_set,
	.oob_clear = a6xx_rgmu_oob_clear,
	.gx_is_on = a6xx_rgmu_gx_is_on,
	.ifpc_store = a6xx_rgmu_ifpc_store,
	.ifpc_show = a6xx_rgmu_ifpc_show,
};

static int a6xx_rgmu_irq_probe(struct kgsl_device *device)
{
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(ADRENO_DEVICE(device));
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
	int ret, i;

	ret = devm_clk_bulk_get_all(&rgmu->pdev->dev, &rgmu->clks);
	if (ret < 0)
		return ret;
	/*
	 * Voting for apb_pclk will enable power and clocks required for
	 * QDSS path to function. However, if QCOM_KGSL_QDSS_STM is not enabled,
	 * QDSS is essentially unusable. Hence, if QDSS cannot be used,
	 * don't vote for this clock.
	 */
	if (!IS_ENABLED(CONFIG_QCOM_KGSL_QDSS_STM)) {
		for (i = 0; i < ret; i++) {
			if (!strcmp(rgmu->clks[i].id, "apb_pclk")) {
				rgmu->clks[i].clk = NULL;
				break;
			}
		}
	}
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

const struct adreno_power_ops a6xx_rgmu_power_ops = {
	.first_open = a6xx_rgmu_first_open,
	.last_close = a6xx_power_off,
	.active_count_get = a6xx_rgmu_active_count_get,
	.active_count_put = a6xx_rgmu_active_count_put,
	.pm_suspend = a6xx_rgmu_pm_suspend,
	.pm_resume = a6xx_rgmu_pm_resume,
	.touch_wakeup = a6xx_rgmu_touch_wakeup,
	.gpu_clock_set = a6xx_rgmu_clock_set,
};

int a6xx_rgmu_device_probe(struct platform_device *pdev,
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

	ret = adreno_dispatcher_init(adreno_dev);
	if (ret)
		return ret;

	device = KGSL_DEVICE(adreno_dev);

	INIT_WORK(&device->idle_check_ws, rgmu_idle_check);

	timer_setup(&device->idle_timer, rgmu_idle_timer, 0);

	adreno_dev->irq_mask = A6XX_INT_MASK;

	return 0;
}

int a6xx_rgmu_add_to_minidump(struct adreno_device *adreno_dev)
{
	struct a6xx_device *a6xx_dev = container_of(adreno_dev,
					struct a6xx_device, adreno_dev);

	return kgsl_add_va_to_minidump(adreno_dev->dev.dev, KGSL_A6XX_DEVICE,
				(void *)(a6xx_dev), sizeof(struct a6xx_device));
}

/* Do not access any RGMU registers in RGMU probe function */
static int a6xx_rgmu_probe(struct kgsl_device *device,
		struct platform_device *pdev)
{
	struct adreno_device *adreno_dev = ADRENO_DEVICE(device);
	struct a6xx_rgmu_device *rgmu = to_a6xx_rgmu(adreno_dev);
	int ret;

	rgmu->pdev = pdev;

	/* Set up RGMU regulators */
	ret = a6xx_rgmu_regulators_probe(rgmu);
	if (ret)
		return ret;

	/* Set up RGMU clocks */
	ret = a6xx_rgmu_clocks_probe(rgmu, pdev->dev.of_node);
	if (ret)
		return ret;

	ret = kgsl_regmap_add_region(&device->regmap, pdev,
		"kgsl_rgmu", NULL, NULL);
	if (ret) {
		dev_err(&pdev->dev, "Unable to map the RGMU registers\n");
		return ret;
	}

	/* Initialize OOB and RGMU interrupts */
	ret = a6xx_rgmu_irq_probe(device);
	if (ret)
		return ret;

	/* Set up RGMU idle states */
	if (ADRENO_FEATURE(ADRENO_DEVICE(device), ADRENO_IFPC))
		rgmu->idle_level = GPU_HW_IFPC;
	else
		rgmu->idle_level = GPU_HW_ACTIVE;

	set_bit(GMU_ENABLED, &device->gmu_core.flags);
	device->gmu_core.dev_ops = &a6xx_rgmudev;

	return 0;
}

static void a6xx_rgmu_remove(struct kgsl_device *device)
{

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
