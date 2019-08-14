/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_RGMU_H
#define __KGSL_RGMU_H

#define RGMU_AO_IRQ_FENCE_ERR		BIT(3)
#define RGMU_AO_IRQ_MASK			RGMU_AO_IRQ_FENCE_ERR

#define RGMU_OOB_IRQ_ERR_MSG		BIT(24)
#define RGMU_OOB_IRQ_ACK_MASK		GENMASK(23, 16)
#define RGMU_OOB_IRQ_ERR_MSG_MASK	GENMASK(31, 24)
#define RGMU_OOB_IRQ_MASK		RGMU_OOB_IRQ_ERR_MSG_MASK

#define MAX_RGMU_CLKS  8

/**
 * struct rgmu_device - rGMU device structure
 * @ver: RGMU firmware version
 * @reg_phys: RGMU CSR physical address
 * @reg_virt: RGMU CSR virtual address
 * @reg_len: RGMU CSR range
 * @rgmu_interrupt_num: RGMU interrupt number
 * @oob_interrupt_num: number of RGMU asserted OOB interrupt
 * @fw_hostptr: Buffer which holds the RGMU firmware
 * @fw_size: Size of RGMU firmware buffer
 * @cx_gdsc: CX headswitch that controls power of RGMU and
		subsystem peripherals
 * @clks: RGMU clocks including the GPU
 * @gpu_clk: Pointer to GPU core clock
 * @rgmu_clk: Pointer to rgmu clock
 * @gpu_freqs: GPU frequency table with lowest freq at index 0
 * @num_gpupwrlevels: number GPU frequencies in GPU freq table
 * @flags: RGMU flags
 * @idle_level: Minimal GPU idle power level
 * @fault_count: RGMU fault count
 */
struct rgmu_device {
	u32 ver;
	struct platform_device *pdev;
	unsigned long reg_phys;
	unsigned int reg_len;
	unsigned int rgmu_interrupt_num;
	unsigned int oob_interrupt_num;
	unsigned int *fw_hostptr;
	uint32_t fw_size;
	struct regulator *cx_gdsc;
	struct regulator *gx_gdsc;
	struct clk *clks[MAX_RGMU_CLKS];
	struct clk *gpu_clk;
	struct clk *rgmu_clk;
	unsigned int gpu_freqs[MAX_GX_LEVELS];
	unsigned int num_gpupwrlevels;
	unsigned int idle_level;
	unsigned int fault_count;
};

extern struct gmu_dev_ops adreno_a6xx_rgmudev;
#define KGSL_RGMU_DEVICE(_a)  ((struct rgmu_device *)((_a)->gmu_core.ptr))

irqreturn_t rgmu_irq_handler(int irq, void *data);
irqreturn_t oob_irq_handler(int irq, void *data);
#endif /* __KGSL_RGMU_H */
