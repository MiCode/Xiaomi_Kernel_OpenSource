/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2018-2020, The Linux Foundation. All rights reserved.
 */
#ifndef __KGSL_GMU_CORE_H
#define __KGSL_GMU_CORE_H

/* GMU_DEVICE - Given an KGSL device return the GMU specific struct */
#define GMU_DEVICE_OPS(_a) ((_a)->gmu_core.dev_ops)
#define GMU_CORE_OPS(_a) ((_a)->gmu_core.core_ops)

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

#define MAX_GMU_CLKS 7

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
	GMU_CLK_ON,
	GMU_HFI_ON,
	GMU_FAULT,
	GMU_DCVS_REPLAY,
	GMU_GPMU,
	GMU_ENABLED,
	GMU_RSCC_SLEEP_SEQ_DONE,
};

/* GMU Types */
enum gmu_coretype {
	GMU_CORE_TYPE_CM3 = 1, /* Cortex M3 core */
	GMU_CORE_TYPE_PCC = 2, /* Power collapsible controller */
	GMU_CORE_TYPE_NONE, /* No GMU */
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

struct gmu_core_ops {
	int (*probe)(struct kgsl_device *device, struct device_node *node);
	void (*remove)(struct kgsl_device *device);
	int (*dcvs_set)(struct kgsl_device *device,
			unsigned int gpu_pwrlevel, unsigned int bus_level);
	int (*init)(struct kgsl_device *device);
	int (*start)(struct kgsl_device *device);
	void (*stop)(struct kgsl_device *device);
	void (*snapshot)(struct kgsl_device *device);
	bool (*regulator_isenabled)(struct kgsl_device *device);
	int (*suspend)(struct kgsl_device *device);
	int (*acd_set)(struct kgsl_device *device, unsigned int val);
};

struct gmu_dev_ops {
	int (*load_firmware)(struct kgsl_device *device);
	int (*oob_set)(struct kgsl_device *device, enum oob_request req);
	void (*oob_clear)(struct kgsl_device *device, enum oob_request req);
	void (*bcl_config)(struct kgsl_device *device, bool on);
	void (*irq_enable)(struct kgsl_device *device);
	void (*irq_disable)(struct kgsl_device *device);
	int (*hfi_start_msg)(struct kgsl_device *device);
	void (*enable_lm)(struct kgsl_device *device);
	int (*rpmh_gpu_pwrctrl)(struct kgsl_device *device, unsigned int ops,
			unsigned int arg1, unsigned int arg2);
	int (*wait_for_lowest_idle)(struct kgsl_device *device);
	int (*wait_for_gmu_idle)(struct kgsl_device *device);
	bool (*gx_is_on)(struct kgsl_device *device);
	bool (*cx_is_on)(struct kgsl_device *device);
	void (*prepare_stop)(struct kgsl_device *device);
	int (*ifpc_store)(struct kgsl_device *device, unsigned int val);
	unsigned int (*ifpc_show)(struct kgsl_device *device);
	void (*snapshot)(struct kgsl_device *device,
		struct kgsl_snapshot *shapshot);
	void (*cooperative_reset)(struct kgsl_device *device);
	void (*halt_execution)(struct kgsl_device *device);
	int (*wait_for_active_transition)(struct kgsl_device *device);
	u64 (*read_ao_counter)(struct kgsl_device *device);
	const unsigned int gmu2host_intr_mask;
	const unsigned int gmu_ao_intr_mask;
};

/**
 * struct gmu_core_device - GMU Core device structure
 * @ptr: Pointer to GMU device structure
 * @gmu2gpu_offset: address difference between GMU register set
 *	and GPU register set, the offset will be used when accessing
 *	gmu registers using offset defined in GPU register space.
 * @reg_len: GMU registers length
 * @reg_virt: GMU CSR virtual address
 * @core_ops: Pointer to gmu core operations
 * @dev_ops: Pointer to gmu device operations
 * @flags: GMU flags
 */
struct gmu_core_device {
	void *ptr;
	unsigned int gmu2gpu_offset;
	unsigned int reg_len;
	void __iomem *reg_virt;
	struct gmu_core_ops *core_ops;
	struct gmu_dev_ops *dev_ops;
	unsigned long flags;
	enum gmu_coretype type;
};

extern struct gmu_core_ops gmu_ops;
extern struct gmu_core_ops rgmu_ops;

/* GMU core functions */
int gmu_core_probe(struct kgsl_device *device);
void gmu_core_remove(struct kgsl_device *device);
int gmu_core_init(struct kgsl_device *device);
int gmu_core_start(struct kgsl_device *device);
void gmu_core_stop(struct kgsl_device *device);
int gmu_core_suspend(struct kgsl_device *device);
void gmu_core_snapshot(struct kgsl_device *device);
bool gmu_core_gpmu_isenabled(struct kgsl_device *device);
bool gmu_core_scales_bandwidth(struct kgsl_device *device);
bool gmu_core_isenabled(struct kgsl_device *device);
int gmu_core_dcvs_set(struct kgsl_device *device, unsigned int gpu_pwrlevel,
		unsigned int bus_level);
int gmu_core_acd_set(struct kgsl_device *device, unsigned int val);
bool gmu_core_regulator_isenabled(struct kgsl_device *device);
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
const char *gmu_core_oob_type_str(enum oob_request req);
int gmu_core_dev_oob_set(struct kgsl_device *device, enum oob_request req);
void gmu_core_dev_oob_clear(struct kgsl_device *device, enum oob_request req);
int gmu_core_dev_hfi_start_msg(struct kgsl_device *device);
int gmu_core_dev_wait_for_lowest_idle(struct kgsl_device *device);
void gmu_core_dev_enable_lm(struct kgsl_device *device);
void gmu_core_dev_snapshot(struct kgsl_device *device,
		struct kgsl_snapshot *snapshot);
bool gmu_core_dev_gx_is_on(struct kgsl_device *device);
bool gmu_core_dev_cx_is_on(struct kgsl_device *device);
int gmu_core_dev_ifpc_show(struct kgsl_device *device);
int gmu_core_dev_ifpc_store(struct kgsl_device *device, unsigned int val);
void gmu_core_dev_prepare_stop(struct kgsl_device *device);
int gmu_core_dev_wait_for_active_transition(struct kgsl_device *device);
void gmu_core_dev_cooperative_reset(struct kgsl_device *device);
u64 gmu_core_dev_read_ao_counter(struct kgsl_device *device);

#endif /* __KGSL_GMU_CORE_H */
