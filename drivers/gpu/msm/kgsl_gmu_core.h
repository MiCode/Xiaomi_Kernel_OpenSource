/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_GMU_CORE_H
#define __KGSL_GMU_CORE_H

/* GMU_DEVICE - Given an KGSL device return the GMU specific struct */
#define GMU_DEVICE_OPS(_a) ((_a)->gmu_core.dev_ops)

#define NUM_BW_LEVELS		100
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
	GMU_DISPATCH,
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

struct device_node;
struct kgsl_device;
struct kgsl_snapshot;

struct gmu_dev_ops {
	int (*oob_set)(struct kgsl_device *device, enum oob_request req);
	void (*oob_clear)(struct kgsl_device *device, enum oob_request req);
	bool (*gx_is_on)(struct kgsl_device *device);
	int (*ifpc_store)(struct kgsl_device *device, unsigned int val);
	unsigned int (*ifpc_show)(struct kgsl_device *device);
	void (*cooperative_reset)(struct kgsl_device *device);
	void (*halt_execution)(struct kgsl_device *device);
	int (*wait_for_active_transition)(struct kgsl_device *device);
	bool (*scales_bandwidth)(struct kgsl_device *device);
	int (*acd_set)(struct kgsl_device *device, bool val);
};

/**
 * struct gmu_core_device - GMU Core device structure
 * @ptr: Pointer to GMU device structure
 * @gmu2gpu_offset: address difference between GMU register set
 *	and GPU register set, the offset will be used when accessing
 *	gmu registers using offset defined in GPU register space.
 * @reg_len: GMU registers length
 * @reg_virt: GMU CSR virtual address
 * @dev_ops: Pointer to gmu device operations
 * @flags: GMU flags
 */
struct gmu_core_device {
	void *ptr;
	unsigned int gmu2gpu_offset;
	unsigned int reg_len;
	void __iomem *reg_virt;
	const struct gmu_dev_ops *dev_ops;
	unsigned long flags;
};

extern struct platform_driver a6xx_gmu_driver;
extern struct platform_driver a6xx_rgmu_driver;
extern struct platform_driver a6xx_hwsched_driver;

/* GMU core functions */

void __init gmu_core_register(void);
void __exit gmu_core_unregister(void);

bool gmu_core_gpmu_isenabled(struct kgsl_device *device);
bool gmu_core_scales_bandwidth(struct kgsl_device *device);
bool gmu_core_isenabled(struct kgsl_device *device);
int gmu_core_dev_acd_set(struct kgsl_device *device, bool val);
bool gmu_core_is_register_offset(struct kgsl_device *device,
				unsigned int offsetwords);
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
bool gmu_core_dev_gx_is_on(struct kgsl_device *device);
int gmu_core_dev_ifpc_show(struct kgsl_device *device);
int gmu_core_dev_ifpc_store(struct kgsl_device *device, unsigned int val);
int gmu_core_dev_wait_for_active_transition(struct kgsl_device *device);
void gmu_core_dev_cooperative_reset(struct kgsl_device *device);

#endif /* __KGSL_GMU_CORE_H */
