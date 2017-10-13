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
#ifndef __KGSL_GMU_H
#define __KGSL_GMU_H

#include "kgsl_hfi.h"

#define FW_VER_MAJOR(ver)		(((ver)>>28) & 0xFF)
#define FW_VER_MINOR(ver)		(((ver)>>16) & 0xFFF)
#define FW_VERSION(major, minor)	\
		(((major) << 28) | (((minor) & 0xFFF) << 16))

#define GMU_INT_WDOG_BITE		BIT(0)
#define GMU_INT_RSCC_COMP		BIT(1)
#define GMU_INT_FENCE_ERR		BIT(3)
#define GMU_INT_DBD_WAKEUP		BIT(4)
#define GMU_INT_HOST_AHB_BUS_ERR	BIT(5)
#define GMU_AO_INT_MASK		\
		(GMU_INT_WDOG_BITE |	\
		GMU_INT_HOST_AHB_BUS_ERR |	\
		GMU_INT_FENCE_ERR)

#define MAX_GMUFW_SIZE	0x2000	/* in dwords */
#define FENCE_RANGE_MASK	((0x1 << 31) | (0x0A << 18) | (0x8A0))

/* Bitmask for GPU low power mode enabling and hysterisis*/
#define SPTP_ENABLE_MASK (BIT(2) | BIT(0))
#define IFPC_ENABLE_MASK (BIT(1) | BIT(0))
#define HW_NAP_ENABLE_MASK	BIT(0)
#define MIN_BW_ENABLE_MASK	BIT(12)
#define MIN_BW_HYST		0xFA0

/* Bitmask for RPMH capability enabling */
#define RPMH_INTERFACE_ENABLE	BIT(0)
#define LLC_VOTE_ENABLE			BIT(4)
#define DDR_VOTE_ENABLE			BIT(8)
#define MX_VOTE_ENABLE			BIT(9)
#define CX_VOTE_ENABLE			BIT(10)
#define GFX_VOTE_ENABLE			BIT(11)
#define RPMH_ENABLE_MASK	(RPMH_INTERFACE_ENABLE	| \
				LLC_VOTE_ENABLE		| \
				DDR_VOTE_ENABLE		| \
				MX_VOTE_ENABLE		| \
				CX_VOTE_ENABLE		| \
				GFX_VOTE_ENABLE)

/* Bitmask for GPU idle status check */
#define GPUBUSYIGNAHB		BIT(23)
#define CXGXCPUBUSYIGNAHB	BIT(30)

/* GMU timeouts */
#define GMU_IDLE_TIMEOUT        10 /* ms */

/* Constants for GMU OOBs */
#define OOB_BOOT_OPTION         0
#define OOB_SLUMBER_OPTION      1

/* Bitmasks for GMU OOBs */
#define OOB_BOOT_SLUMBER_SET_MASK	BIT(22)
#define OOB_BOOT_SLUMBER_CHECK_MASK	BIT(30)
#define OOB_BOOT_SLUMBER_CLEAR_MASK	BIT(30)
#define OOB_DCVS_SET_MASK		BIT(23)
#define OOB_DCVS_CHECK_MASK		BIT(31)
#define OOB_DCVS_CLEAR_MASK		BIT(31)
#define OOB_GPU_SET_MASK		BIT(16)
#define OOB_GPU_CHECK_MASK		BIT(24)
#define OOB_GPU_CLEAR_MASK		BIT(24)
#define OOB_PERFCNTR_SET_MASK		BIT(17)
#define OOB_PERFCNTR_CHECK_MASK		BIT(25)
#define OOB_PERFCNTR_CLEAR_MASK		BIT(25)

/* Bits for the flags field in the gmu structure */
enum gmu_flags {
	GMU_BOOT_INIT_DONE = 0,
	GMU_CLK_ON = 1,
	GMU_HFI_ON = 2,
	GMU_FAULT = 3,
	GMU_DCVS_REPLAY = 4,
};

/**
 * struct gmu_memdesc - Gmu shared memory object descriptor
 * @hostptr: Kernel virtual address
 * @gmuaddr: GPU virtual address
 * @physaddr: Physical address of the memory object
 * @size: Size of the memory object
 * @attr: memory attributes for this memory
 */
struct gmu_memdesc {
	void *hostptr;
	uint64_t gmuaddr;
	phys_addr_t physaddr;
	uint64_t size;
	uint32_t attr;
};

struct gmu_bw_votes {
	uint32_t cmds_wait_bitmask;
	uint32_t cmds_per_bw_vote;
	uint32_t cmd_addrs[MAX_BW_CMDS];
	uint32_t cmd_data[MAX_GX_LEVELS][MAX_BW_CMDS];
};

struct rpmh_votes_t {
	struct arc_vote_desc gx_votes[MAX_GX_LEVELS];
	struct arc_vote_desc cx_votes[MAX_CX_LEVELS];
	struct gmu_bw_votes ddr_votes;
	struct gmu_bw_votes cnoc_votes;
};

#define MAX_GMU_CLKS 6
#define DEFAULT_GMU_FREQ_IDX 1

/*
 * These are the different ways the GMU can boot. GMU_WARM_BOOT is waking up
 * from slumber. GMU_COLD_BOOT is booting for the first time. GMU_RESET
 * is a soft reset of the GMU.
 */
enum gmu_boot {
	GMU_WARM_BOOT = 0,
	GMU_COLD_BOOT = 1,
	GMU_RESET = 2
};

enum gmu_load_mode {
	CACHED_LOAD_BOOT,
	CACHED_BOOT,
	TCM_BOOT,
	TCM_LOAD_BOOT,
	INVALID_LOAD
};

enum gmu_pwrctrl_mode {
	GMU_FW_START,
	GMU_FW_STOP,
	GMU_SUSPEND,
	GMU_DCVS_NOHFI,
	GMU_NOTIFY_SLUMBER,
	INVALID_POWER_CTRL
};

enum gpu_idle_level {
	GPU_HW_ACTIVE = 0x0,
	GPU_HW_SPTP_PC = 0x2,
	GPU_HW_IFPC = 0x3,
	GPU_HW_NAP = 0x4,
	GPU_HW_MIN_VOLT = 0x5,
	GPU_HW_MIN_DDR = 0x6,
	GPU_HW_SLUMBER = 0xF
};

/**
 * struct gmu_device - GMU device structure
 * @ver: GMU FW version, read from GMU
 * @reg_phys: GMU CSR physical address
 * @reg_virt: GMU CSR virtual address
 * @reg_len: GMU CSR range
 * @gmu2gpu_offset: address difference between GMU register set
 *	and GPU register set, the offset will be used when accessing
 *	gmu registers using offset defined in GPU register space.
 * @pdc_reg_virt: starting kernel virtual address for RPMh PDC registers
 * @gmu_interrupt_num: GMU interrupt number
 * @fw_image: descriptor of GMU memory that has GMU image in it
 * @hfi_mem: pointer to HFI shared memory
 * @dump_mem: pointer to GMU debug dump memory
 * @hfi: HFI controller
 * @lm_config: GPU LM configuration data
 * @lm_dcvs_level: Minimal DCVS level that enable LM. LM disable in
 *		lower levels
 * @bcl_config: Battery Current Limit configuration data
 * @gmu_freqs: GMU frequency table with lowest freq at index 0
 * @gpu_freqs: GPU frequency table with lowest freq at index 0
 * @num_gmupwrlevels: number GMU frequencies in GMU freq table
 * @num_gpupwrlevels: number GPU frequencies in GPU freq table
 * @num_bwlevel: number of GPU BW levels
 * @num_cnocbwlevel: number CNOC BW levels
 * @rpmh_votes: RPMh TCS command set for GPU, GMU voltage and bw scaling
 * @cx_gdsc: CX headswitch that controls power of GMU and
		subsystem peripherals
 * @gx_gdsc: GX headswitch that controls power of GPU subsystem
 * @clks: GPU subsystem clocks required for GMU functionality
 * @load_mode: GMU FW load/boot mode
 * @flags: GMU power control flags
 * @wakeup_pwrlevel: GPU wake up power/DCVS level in case different
 *		than default power level
 * @pcl: GPU BW scaling client
 * @ccl: CNOC BW scaling client
 * @idle_level: Minimal GPU idle power level
 * @fault_count: GMU fault count
 */
struct gmu_device {
	unsigned int ver;
	struct platform_device *pdev;
	unsigned long reg_phys;
	void __iomem *reg_virt;
	unsigned int reg_len;
	unsigned int gmu2gpu_offset;
	void __iomem *pdc_reg_virt;
	unsigned int gmu_interrupt_num;
	struct gmu_memdesc fw_image;
	struct gmu_memdesc *hfi_mem;
	struct gmu_memdesc *dump_mem;
	struct kgsl_hfi hfi;
	struct limits_config lm_config;
	unsigned int lm_dcvs_level;
	unsigned int bcl_config;
	unsigned int gmu_freqs[MAX_CX_LEVELS];
	unsigned int gpu_freqs[MAX_GX_LEVELS];
	unsigned int num_gmupwrlevels;
	unsigned int num_gpupwrlevels;
	unsigned int num_bwlevels;
	unsigned int num_cnocbwlevels;
	struct rpmh_votes_t rpmh_votes;
	struct regulator *cx_gdsc;
	struct regulator *gx_gdsc;
	struct clk *clks[MAX_GMU_CLKS];
	enum gmu_load_mode load_mode;
	unsigned long flags;
	unsigned int wakeup_pwrlevel;
	unsigned int pcl;
	unsigned int ccl;
	unsigned int idle_level;
	unsigned int fault_count;
};

bool kgsl_gmu_isenabled(struct kgsl_device *device);
int gmu_probe(struct kgsl_device *device);
void gmu_remove(struct kgsl_device *device);
int allocate_gmu_image(struct gmu_device *gmu, unsigned int size);
int gmu_start(struct kgsl_device *device);
void gmu_stop(struct kgsl_device *device);
int gmu_dcvs_set(struct gmu_device *gmu, unsigned int gpu_pwrlevel,
		unsigned int bus_level);
#endif /* __KGSL_GMU_H */
