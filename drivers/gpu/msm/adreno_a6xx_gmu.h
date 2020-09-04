/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __ADRENO_A6XX_GMU_H
#define __ADRENO_A6XX_GMU_H

#include <linux/mailbox_client.h>

#include "adreno_a6xx_hfi.h"
#include "kgsl_gmu_core.h"

#define GMU_FREQUENCY   200000000

#define GMU_PWR_LEVELS  2
#define MAX_GMUFW_SIZE	0x8000	/* in bytes */

#define GMU_VER_MAJOR(ver) (((ver) >> 28) & 0xF)
#define GMU_VER_MINOR(ver) (((ver) >> 16) & 0xFFF)
#define GMU_VER_STEP(ver) ((ver) & 0xFFFF)
#define GMU_VERSION(major, minor) \
	((((major) & 0xF) << 28) | (((minor) & 0xFFF) << 16))

#define GMU_INT_WDOG_BITE		BIT(0)
#define GMU_INT_RSCC_COMP		BIT(1)
#define GMU_INT_FENCE_ERR		BIT(3)
#define GMU_INT_DBD_WAKEUP		BIT(4)
#define GMU_INT_HOST_AHB_BUS_ERR	BIT(5)
#define GMU_AO_INT_MASK		\
		(GMU_INT_WDOG_BITE |	\
		GMU_INT_FENCE_ERR |	\
		GMU_INT_HOST_AHB_BUS_ERR)

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

/* Constants for GMU OOBs */
#define OOB_BOOT_OPTION         0
#define OOB_SLUMBER_OPTION      1

/* Gmu FW block header format */
struct gmu_block_header {
	uint32_t addr;
	uint32_t size;
	uint32_t type;
	uint32_t value;
};

/* GMU Block types */
#define GMU_BLK_TYPE_DATA 0
#define GMU_BLK_TYPE_PREALLOC_REQ 1
#define GMU_BLK_TYPE_CORE_VER 2
#define GMU_BLK_TYPE_CORE_DEV_VER 3
#define GMU_BLK_TYPE_PWR_VER 4
#define GMU_BLK_TYPE_PWR_DEV_VER 5
#define GMU_BLK_TYPE_HFI_VER 6
#define GMU_BLK_TYPE_PREALLOC_PERSIST_REQ 7

/* For GMU Logs*/
#define LOGMEM_SIZE  SZ_4K

/* GMU memdesc entries */
#define GMU_KERNEL_ENTRIES		16

enum gmu_mem_type {
	GMU_ITCM = 0,
	GMU_ICACHE,
	GMU_CACHE = GMU_ICACHE,
	GMU_DTCM,
	GMU_DCACHE,
	GMU_NONCACHED_KERNEL,
	GMU_NONCACHED_USER,
	GMU_MEM_TYPE_MAX,
};

enum gmu_context_index {
	GMU_CONTEXT_USER,
	GMU_CONTEXT_KERNEL,
};

/**
 * struct gmu_memdesc - Gmu shared memory object descriptor
 * @hostptr: Kernel virtual address
 * @gmuaddr: GPU virtual address
 * @physaddr: Physical address of the memory object
 * @size: Size of the memory object
 */
struct gmu_memdesc {
	void *hostptr;
	u32 gmuaddr;
	phys_addr_t physaddr;
	u32 size;
};

struct rpmh_votes_t {
	uint32_t gx_votes[MAX_GX_LEVELS];
	uint32_t cx_votes[MAX_CX_LEVELS];
};

struct kgsl_mailbox {
	struct mbox_client client;
	struct mbox_chan *channel;
};

struct icc_path;

struct gmu_vma_entry {
	/** @start: Starting virtual address of the vma */
	u32 start;
	/** @size: Size of this vma */
	u32 size;
	/** @next_va: Next available virtual address in this vma */
	u32 next_va;
};

enum {
	GMU_PRIV_FIRST_BOOT_DONE = 0,
	GMU_PRIV_GPU_STARTED,
	GMU_PRIV_HFI_STARTED,
	GMU_PRIV_RSCC_SLEEP_DONE,
	GMU_PRIV_PM_SUSPEND,
	GMU_PRIV_PDC_RSC_LOADED,
};

/**
 * struct a6xx_gmu_device - GMU device structure
 * @ver: GMU Version information
 * @irq: GMU interrupt number
 * @fw_image: GMU FW image
 * @hfi_mem: pointer to HFI shared memory
 * @dump_mem: pointer to GMU debug dump memory
 * @gmu_log: gmu event log memory
 * @hfi: HFI controller
 * @num_gpupwrlevels: number GPU frequencies in GPU freq table
 * @num_bwlevel: number of GPU BW levels
 * @num_cnocbwlevel: number CNOC BW levels
 * @rpmh_votes: RPMh TCS command set for GPU, GMU voltage and bw scaling
 * @cx_gdsc: CX headswitch that controls power of GMU and
		subsystem peripherals
 * @gx_gdsc: GX headswitch that controls power of GPU subsystem
 * @clks: GPU subsystem clocks required for GMU functionality
 * @wakeup_pwrlevel: GPU wake up power/DCVS level in case different
 *		than default power level
 * @idle_level: Minimal GPU idle power level
 * @fault_count: GMU fault count
 * @mailbox: Messages to AOP for ACD enable/disable go through this
 * @log_wptr_retention: Store the log wptr offset on slumber
 */
struct a6xx_gmu_device {
	struct {
		u32 core;
		u32 core_dev;
		u32 pwr;
		u32 pwr_dev;
		u32 hfi;
	} ver;
	struct platform_device *pdev;
	int irq;
	const struct firmware *fw_image;
	struct gmu_memdesc *dump_mem;
	struct gmu_memdesc *gmu_log;
	struct a6xx_hfi hfi;
	/** @pwrlevels: Array of GMU power levels */
	struct regulator *cx_gdsc;
	struct regulator *gx_gdsc;
	struct clk_bulk_data *clks;
	/** @num_clks: Number of entries in the @clks array */
	int num_clks;
	unsigned int idle_level;
	struct kgsl_mailbox mailbox;
	bool preallocations;
	/** @gmu_globals: Array to store gmu global buffers */
	struct gmu_memdesc gmu_globals[GMU_KERNEL_ENTRIES];
	/** @global_entries: To keep track of number of gmu buffers */
	u32 global_entries;
	struct gmu_vma_entry *vma;
	unsigned int log_wptr_retention;
	/** @cm3_fault: whether gmu received a cm3 fault interrupt */
	atomic_t cm3_fault;
	/**
	 * @itcm_shadow: Copy of the itcm block in firmware binary used for
	 * snapshot
	 */
	void *itcm_shadow;
	/** @flags: Internal gmu flags */
	unsigned long flags;
	/** @rscc_virt: Pointer where RSCC block is mapped */
	void __iomem *rscc_virt;
};

/* Helper function to get to a6xx gmu device from adreno device */
struct a6xx_gmu_device *to_a6xx_gmu(struct adreno_device *adreno_dev);

/* Helper function to get to adreno device from a6xx gmu device */
struct adreno_device *a6xx_gmu_to_adreno(struct a6xx_gmu_device *gmu);

/**
 * reserve_gmu_kernel_block() - Allocate a gmu buffer
 * @gmu: Pointer to the a6xx gmu device
 * @addr: Desired gmu virtual address
 * @size: Size of the buffer in bytes
 * @vma_id: Target gmu vma where this bufer should be mapped
 *
 * This function allocates a buffer and maps it in
 * the desired gmu vma
 *
 * Return: Pointer to the memory descriptor or error pointer on failure
 */
struct gmu_memdesc *reserve_gmu_kernel_block(struct a6xx_gmu_device *gmu,
	u32 addr, u32 size, u32 vma_id);

/**
 * a6xx_build_rpmh_tables - Build the rpmh tables
 * @adreno_dev: Pointer to the adreno device
 *
 * This function creates the gpu dcvs and bw tables
 *
 * Return: 0 on success and negative error on failure
 */
int a6xx_build_rpmh_tables(struct adreno_device *adreno_dev);

/**
 * a6xx_gmu_gx_is_on - Check if GX is on
 * @device: Pointer to KGSL device
 *
 * This function reads pwr status registers to check if GX
 * is on or off
 */
bool a6xx_gmu_gx_is_on(struct kgsl_device *device);

/**
 * a6xx_gmu_device_snapshot - A6XX GMU snapshot function
 * @device: Device being snapshotted
 * @snapshot: Pointer to the snapshot instance
 *
 * This is where all of the A6XX GMU specific bits and pieces are grabbed
 * into the snapshot memory
 */
void a6xx_gmu_device_snapshot(struct kgsl_device *device,
	struct kgsl_snapshot *snapshot);

/**
 * a6xx_gmu_device_probe - A6XX GMU snapshot function
 * @pdev: Pointer to the platform device
 * @chipid: Chipid of the target
 * @gpucore: Pointer to the gpucore
 *
 * The target specific probe function for gmu based a6xx targets.
 */
int a6xx_gmu_device_probe(struct platform_device *pdev,
	u32 chipid, const struct adreno_gpu_core *gpucore);

/**
 * a6xx_gmu_restart - Reset and restart the gmu
 * @device: Pointer to the kgsl device
 *
 * Return: 0 on success or negative error on failure
 */
int a6xx_gmu_restart(struct kgsl_device *device);

/**
 * a6xx_enable_gpu_irq - Enable gpu interrupt
 * @adreno_dev: Pointer to the adreno device
 */
void a6xx_enable_gpu_irq(struct adreno_device *adreno_dev);

/**
 * a6xx_disable_gpu_irq - Disable gpu interrupt
 * @adreno_dev: Pointer to the adreno device
 */
void a6xx_disable_gpu_irq(struct adreno_device *adreno_dev);

/**
 * a6xx_gmu_snapshot- Take snapshot for gmu targets
 * @adreno_dev: Pointer to the adreno device
 * @snapshot: Pointer to the snapshot structure
 *
 * Send an NMI to gmu if we hit a gmu fault. Then take gmu
 * snapshot and carry on with rest of the a6xx snapshot
 */
void a6xx_gmu_snapshot(struct adreno_device *adreno_dev,
	struct kgsl_snapshot *snapshot);
#endif
