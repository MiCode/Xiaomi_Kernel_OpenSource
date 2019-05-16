/* Copyright (c) 2017-2019, The Linux Foundation. All rights reserved.
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

#include <linux/mailbox_client.h>
#include "kgsl_gmu_core.h"
#include <linux/firmware.h>
#include "kgsl_hfi.h"

#define MAX_GMUFW_SIZE	0x8000	/* in bytes */

#define BWMEM_SIZE	(12 + (4 * NUM_BW_LEVELS))	/*in bytes*/

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

/* For GMU Logs*/
#define LOGMEM_SIZE  SZ_4K

extern struct gmu_dev_ops adreno_a6xx_gmudev;
#define KGSL_GMU_DEVICE(_a)  ((struct gmu_device *)((_a)->gmu_core.ptr))

enum gmu_mem_type {
	GMU_ITCM = 0,
	GMU_ICACHE,
	GMU_DTCM,
	GMU_DCACHE,
	GMU_NONCACHED_KERNEL,
	GMU_NONCACHED_USER,
	GMU_MEM_TYPE_MAX,
};

/**
 * struct gmu_memdesc - Gmu shared memory object descriptor
 * @hostptr: Kernel virtual address
 * @gmuaddr: GPU virtual address
 * @physaddr: Physical address of the memory object
 * @size: Size of the memory object
 * @mem_type: memory type for this memory
 */
struct gmu_memdesc {
	void *hostptr;
	uint64_t gmuaddr;
	phys_addr_t physaddr;
	uint64_t size;
	enum gmu_mem_type mem_type;
};

struct gmu_bw_votes {
	uint32_t cmds_wait_bitmask;
	uint32_t cmds_per_bw_vote;
	uint32_t cmd_addrs[MAX_BW_CMDS];
	uint32_t cmd_data[MAX_GX_LEVELS][MAX_BW_CMDS];
};

struct rpmh_votes_t {
	uint32_t gx_votes[MAX_GX_LEVELS];
	uint32_t cx_votes[MAX_CX_LEVELS];
	struct gmu_bw_votes ddr_votes;
	struct gmu_bw_votes cnoc_votes;
};

enum gmu_load_mode {
	CACHED_LOAD_BOOT,
	CACHED_BOOT,
	TCM_BOOT,
	TCM_LOAD_BOOT,
	INVALID_LOAD
};

struct kgsl_mailbox {
	struct mbox_client *client;
	struct mbox_chan *channel;
};

/**
 * struct gmu_device - GMU device structure
 * @ver: GMU FW version, read from GMU
 * @reg_phys: GMU CSR physical address
 * @reg_len: GMU CSR range
 * @gmu_interrupt_num: GMU interrupt number
 * @fw_image: GMU FW image
 * @hfi_mem: pointer to HFI shared memory
 * @icache_mem: pointer to GMU icache memory
 * @dcache_mem: pointer to GMU dcache memory
 * @persist_mem: pointer to GMU persistent memory
 * @dump_mem: pointer to GMU debug dump memory
 * @gmu_log: gmu event log memory
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
 * @wakeup_pwrlevel: GPU wake up power/DCVS level in case different
 *		than default power level
 * @pcl: GPU BW scaling client
 * @ccl: CNOC BW scaling client
 * @idle_level: Minimal GPU idle power level
 * @fault_count: GMU fault count
 * @mailbox: Messages to AOP for ACD enable/disable go through this
 */
struct gmu_device {
	unsigned int ver;
	struct platform_device *pdev;
	unsigned long reg_phys;
	unsigned int reg_len;
	unsigned int gmu_interrupt_num;
	const struct firmware *fw_image;
	struct gmu_memdesc *hfi_mem;
	struct gmu_memdesc *icache_mem;
	struct gmu_memdesc *dcache_mem;
	struct gmu_memdesc *persist_mem;
	struct gmu_memdesc *dump_mem;
	struct gmu_memdesc *gmu_log;
	struct kgsl_hfi hfi;
	unsigned int lm_config;
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
	unsigned int wakeup_pwrlevel;
	unsigned int pcl;
	unsigned int ccl;
	unsigned int idle_level;
	unsigned int fault_count;
	struct kgsl_mailbox mailbox;
};

struct gmu_memdesc *gmu_get_memdesc(unsigned int addr, unsigned int size);
unsigned int gmu_get_memtype_base(struct gmu_device *gmu,
		enum gmu_mem_type type);

#endif /* __KGSL_GMU_H */
