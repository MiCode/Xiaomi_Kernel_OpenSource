/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_A6XX_RGMU_H
#define __ADRENO_A6XX_RGMU_H

#define RGMU_AO_IRQ_FENCE_ERR		BIT(3)
#define RGMU_AO_IRQ_MASK			RGMU_AO_IRQ_FENCE_ERR

#define RGMU_OOB_IRQ_ERR_MSG		BIT(24)
#define RGMU_OOB_IRQ_ACK_MASK		GENMASK(23, 16)
#define RGMU_OOB_IRQ_ERR_MSG_MASK	GENMASK(31, 24)
#define RGMU_OOB_IRQ_MASK		RGMU_OOB_IRQ_ERR_MSG_MASK

#define MAX_RGMU_CLKS  8

enum {
	/* @RGMU_PRIV_FIRST_BOOT_DONE: The very first ggpu boot is done */
	RGMU_PRIV_FIRST_BOOT_DONE,
	/* @RGMU_PRIV_GPU_STARTED: GPU has been started */
	RGMU_PRIV_GPU_STARTED,
	/* @RGMU_PRIV_PM_SUSPEND: The rgmu driver is suspended */
	RGMU_PRIV_PM_SUSPEND,
};

/**
 * struct a6xx_rgmu_device - rGMU device structure
 * @ver: RGMU firmware version
 * @rgmu_interrupt_num: RGMU interrupt number
 * @oob_interrupt_num: number of RGMU asserted OOB interrupt
 * @fw_hostptr: Buffer which holds the RGMU firmware
 * @fw_size: Size of RGMU firmware buffer
 * @cx_gdsc: CX headswitch that controls power of RGMU and
		subsystem peripherals
 * @clks: RGMU clocks including the GPU
 * @gpu_clk: Pointer to GPU core clock
 * @rgmu_clk: Pointer to rgmu clock
 * @flags: RGMU flags
 * @idle_level: Minimal GPU idle power level
 * @fault_count: RGMU fault count
 */
struct a6xx_rgmu_device {
	u32 ver;
	struct platform_device *pdev;
	unsigned int rgmu_interrupt_num;
	unsigned int oob_interrupt_num;
	unsigned int *fw_hostptr;
	uint32_t fw_size;
	struct regulator *cx_gdsc;
	struct regulator *gx_gdsc;
	struct clk_bulk_data *clks;
	/** @num_clks: Number of clocks in @clks */
	int num_clks;
	struct clk *gpu_clk;
	struct clk *rgmu_clk;
	unsigned int idle_level;
	unsigned int fault_count;
	/** @flags: rgmu internal flags */
	unsigned long flags;
	/** @num_oob_perfcntr: Number of active oob_perfcntr requests */
	u32 num_oob_perfcntr;
};

/**
 * a6xx_rgmu_device_probe - Probe a6xx rgmu resources
 * @pdev: Pointer to the platform device
 * @chipid: Chipid of the target
 * @gpucore: Pointer to the gpucore
 *
 * The target specific probe function for rgmu based a6xx targets.
 */
int a6xx_rgmu_device_probe(struct platform_device *pdev,
	u32 chipid, const struct adreno_gpu_core *gpucore);

/**
 * a6xx_rgmu_restart - Reset and restart the rgmu
 * @device: Pointer to the kgsl device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_rgmu_restart(struct kgsl_device *device);

/**
 * a6xx_rgmu_snapshot - Take snapshot for rgmu based targets
 * @adreno_dev: Pointer to the adreno device
 * @snapshot: Pointer to the snapshot
 *
 * This function halts rgmu execution if we hit a rgmu
 * fault. And then, it takes rgmu and gpu snapshot.
 */
void a6xx_rgmu_snapshot(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot);
#endif
