/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022-2023, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#ifndef __KGSL_GMU_CORE_H
#define __KGSL_GMU_CORE_H

#include <linux/rbtree.h>
#include <linux/mailbox_client.h>

/* GMU_DEVICE - Given an KGSL device return the GMU specific struct */
#define GMU_DEVICE_OPS(_a) ((_a)->gmu_core.dev_ops)

#define MAX_GX_LEVELS		16
#define MAX_CX_LEVELS		4
#define MAX_CNOC_LEVELS		2
#define MAX_CNOC_CMDS		6
#define MAX_BW_CMDS		8
#define INVALID_DCVS_IDX	0xFF

#if MAX_CNOC_LEVELS > MAX_GX_LEVELS
#error "CNOC levels cannot exceed GX levels"
#endif

/*
 * These are the different ways the GMU can boot. GMU_WARM_BOOT is waking up
 * from slumber. GMU_COLD_BOOT is booting for the first time. GMU_RESET
 * is a soft reset of the GMU.
 */
enum gmu_core_boot {
	GMU_WARM_BOOT = 0,
	GMU_COLD_BOOT = 1,
	GMU_RESET = 2
};

/* Bits for the flags field in the gmu structure */
enum gmu_core_flags {
	GMU_BOOT_INIT_DONE = 0,
	GMU_HFI_ON,
	GMU_FAULT,
	GMU_DCVS_REPLAY,
	GMU_ENABLED,
	GMU_RSCC_SLEEP_SEQ_DONE,
	GMU_DISABLE_SLUMBER,
};

/*
 * OOB requests values. These range from 0 to 7 and then
 * the BIT() offset into the actual value is calculated
 * later based on the request. This keeps the math clean
 * and easy to ensure not reaching over/under the range
 * of 8 bits.
 */
enum oob_request {
	oob_gpu = 0,
	oob_perfcntr = 1,
	oob_boot_slumber = 6, /* reserved special case */
	oob_dcvs = 7, /* reserved special case */
	oob_max,
};

enum gmu_pwrctrl_mode {
	GMU_FW_START,
	GMU_FW_STOP,
	GMU_SUSPEND,
	GMU_DCVS_NOHFI,
	GMU_NOTIFY_SLUMBER,
	INVALID_POWER_CTRL
};

#define GPU_HW_ACTIVE	0x00
#define GPU_HW_IFPC	0x03
#define GPU_HW_SLUMBER	0x0f

/*
 * Wait time before trying to write the register again.
 * Hopefully the GMU has finished waking up during this delay.
 * This delay must be less than the IFPC main hysteresis or
 * the GMU will start shutting down before we try again.
 */
#define GMU_CORE_WAKEUP_DELAY_US 10

/* Max amount of tries to wake up the GMU. The short retry
 * limit is half of the long retry limit. After the short
 * number of retries, we print an informational message to say
 * exiting IFPC is taking longer than expected. We continue
 * to retry after this until the long retry limit.
 */
#define GMU_CORE_SHORT_WAKEUP_RETRY_LIMIT 100
#define GMU_CORE_LONG_WAKEUP_RETRY_LIMIT 200

#define FENCE_STATUS_WRITEDROPPED0_MASK 0x1
#define FENCE_STATUS_WRITEDROPPED1_MASK 0x2

#define GMU_MAX_PWRLEVELS	2
#define GMU_FREQ_MIN   200000000
#define GMU_FREQ_MAX   500000000

#define GMU_VER_MAJOR(ver) (((ver) >> 28) & 0xF)
#define GMU_VER_MINOR(ver) (((ver) >> 16) & 0xFFF)
#define GMU_VER_STEP(ver) ((ver) & 0xFFFF)
#define GMU_VERSION(major, minor, step) \
	((((major) & 0xF) << 28) | (((minor) & 0xFFF) << 16) | ((step) & 0xFFFF))

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
	u32 addr;
	u32 size;
	u32 type;
	u32 value;
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
#define GMU_LOG_SIZE  SZ_16K

/* GMU memdesc entries */
#define GMU_KERNEL_ENTRIES		16

enum gmu_mem_type {
	GMU_ITCM = 0,
	GMU_ICACHE,
	GMU_CACHE = GMU_ICACHE,
	GMU_DTCM,
	GMU_DCACHE,
	GMU_NONCACHED_KERNEL, /* GMU VBIF3 uncached VA range: 0x60000000 - 0x7fffffff */
	GMU_NONCACHED_KERNEL_EXTENDED, /* GMU VBIF3 uncached VA range: 0xc0000000 - 0xdfffffff */
	GMU_NONCACHED_USER,
	GMU_MEM_TYPE_MAX,
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

struct kgsl_mailbox {
	struct mbox_client client;
	struct mbox_chan *channel;
};

struct icc_path;

struct gmu_vma_node {
	struct rb_node node;
	u32 va;
	u32 size;
};

struct gmu_vma_entry {
	/** @start: Starting virtual address of the vma */
	u32 start;
	/** @size: Size of this vma */
	u32 size;
	/** @next_va: Next available virtual address in this vma */
	u32 next_va;
	/** @lock: Spinlock for synchronization */
	spinlock_t lock;
	/** @vma_root: RB tree root that keeps track of dynamic allocations */
	struct rb_root vma_root;
};

enum {
	GMU_PRIV_FIRST_BOOT_DONE = 0,
	GMU_PRIV_GPU_STARTED,
	GMU_PRIV_HFI_STARTED,
	GMU_PRIV_RSCC_SLEEP_DONE,
	GMU_PRIV_PM_SUSPEND,
	GMU_PRIV_PDC_RSC_LOADED,
};

struct device_node;
struct kgsl_device;
struct kgsl_snapshot;

struct gmu_dev_ops {
	int (*oob_set)(struct kgsl_device *device, enum oob_request req);
	void (*oob_clear)(struct kgsl_device *device, enum oob_request req);
	int (*ifpc_store)(struct kgsl_device *device, unsigned int val);
	unsigned int (*ifpc_show)(struct kgsl_device *device);
	void (*cooperative_reset)(struct kgsl_device *device);
	int (*wait_for_active_transition)(struct kgsl_device *device);
	bool (*scales_bandwidth)(struct kgsl_device *device);
	int (*acd_set)(struct kgsl_device *device, bool val);
	int (*bcl_sid_set)(struct kgsl_device *device, u32 sid_id, u64 sid_val);
	u64 (*bcl_sid_get)(struct kgsl_device *device, u32 sid_id);
	void (*send_nmi)(struct kgsl_device *device, bool force);
};

/**
 * struct gmu_core_device - GMU Core device structure
 * @ptr: Pointer to GMU device structure
 * @dev_ops: Pointer to gmu device operations
 * @flags: GMU flags
 */
struct gmu_core_device {
	void *ptr;
	const struct gmu_dev_ops *dev_ops;
	unsigned long flags;
};

extern struct platform_driver a6xx_gmu_driver;
extern struct platform_driver a6xx_rgmu_driver;
extern struct platform_driver a6xx_hwsched_driver;
extern struct platform_driver gen7_gmu_driver;
extern struct platform_driver gen7_hwsched_driver;

/* GMU core functions */

void __init gmu_core_register(void);
void gmu_core_unregister(void);

bool gmu_core_gpmu_isenabled(struct kgsl_device *device);
bool gmu_core_scales_bandwidth(struct kgsl_device *device);
bool gmu_core_isenabled(struct kgsl_device *device);
int gmu_core_dev_acd_set(struct kgsl_device *device, bool val);
void gmu_core_regread(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int *value);
void gmu_core_regwrite(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int value);

/**
 * gmu_core_blkwrite - Do a bulk I/O write to GMU
 * @device: Pointer to the kgsl device
 * @offsetwords: Destination dword offset
 * @buffer: Pointer to the source buffer
 * @size: Number of bytes to copy
 *
 * Write a series of GMU registers quickly without bothering to spend time
 * logging the register writes. The logging of these writes causes extra
 * delays that could allow IRQs arrive and be serviced before finishing
 * all the writes.
 */
void gmu_core_blkwrite(struct kgsl_device *device, unsigned int offsetwords,
		const void *buffer, size_t size);
void gmu_core_regrmw(struct kgsl_device *device, unsigned int offsetwords,
		unsigned int mask, unsigned int bits);
int gmu_core_dev_oob_set(struct kgsl_device *device, enum oob_request req);
void gmu_core_dev_oob_clear(struct kgsl_device *device, enum oob_request req);
int gmu_core_dev_ifpc_show(struct kgsl_device *device);
int gmu_core_dev_ifpc_store(struct kgsl_device *device, unsigned int val);
int gmu_core_dev_wait_for_active_transition(struct kgsl_device *device);
void gmu_core_dev_cooperative_reset(struct kgsl_device *device);

/**
 * gmu_core_fault_snapshot - Set gmu fault and trigger snapshot
 * @device: Pointer to the kgsl device
 *
 * Set the gmu fault and take snapshot when we hit a gmu fault
 */
void gmu_core_fault_snapshot(struct kgsl_device *device);

/**
 * gmu_core_timed_poll_check() - polling *gmu* register at given offset until
 * its value changed to match expected value. The function times
 * out and returns after given duration if register is not updated
 * as expected.
 *
 * @device: Pointer to KGSL device
 * @offset: Register offset in dwords
 * @expected_ret: expected register value that stops polling
 * @timeout_ms: time in milliseconds to poll the register
 * @mask: bitmask to filter register value to match expected_ret
 */
int gmu_core_timed_poll_check(struct kgsl_device *device,
		unsigned int offset, unsigned int expected_ret,
		unsigned int timeout_ms, unsigned int mask);

struct kgsl_memdesc;
struct iommu_domain;

/**
 * gmu_core_map_memdesc - Map the memdesc into the GMU IOMMU domain
 * @domain: Domain to map the memory into
 * @memdesc: Memory descriptor to map
 * @gmuaddr: Virtual GMU address to map the memory into
 * @attrs: Attributes for the mapping
 *
 * Return: 0 on success or -ENOMEM on failure
 */
int gmu_core_map_memdesc(struct iommu_domain *domain, struct kgsl_memdesc *memdesc,
		u64 gmuaddr, int attrs);

#endif /* __KGSL_GMU_CORE_H */
