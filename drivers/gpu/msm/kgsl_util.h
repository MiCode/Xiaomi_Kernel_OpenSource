/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019-2021, The Linux Foundation. All rights reserved.
 */

#ifndef _KGSL_UTIL_H_
#define _KGSL_UTIL_H_

#define KGSL_DRIVER "kgsl_driver"
#define KGSL_ADRENO_DEVICE "kgsl_adreno_device"
#define KGSL_A6XX_DEVICE "kgsl_a6xx_device"
#define KGSL_GEN7_DEVICE "kgsl_gen7_device"
#define KGSL_HWSCHED_DEVICE "kgsl_hwsched_device"

#define KGSL_SCRATCH_ENTRY "kgsl_scratch"
#define KGSL_MEMSTORE_ENTRY "kgsl_memstore"
#define KGSL_GMU_LOG_ENTRY "kgsl_gmu_log"
#define KGSL_HFIMEM_ENTRY "kgsl_hfi_mem"
#define KGSL_GMU_DUMPMEM_ENTRY "kgsl_gmu_dump_mem"

struct regulator;
struct clk_bulk_data;

/**
 * struct cpu_gpu_lock - CP spinlock structure for power up list
 * @gpu_req: flag value set by CP
 * @cpu_req: flag value set by KMD
 * @turn: turn variable set by both CP and KMD
 * @list_length: this tells CP the last dword in the list:
 * 16 + (4 * (List_Length - 1))
 * @list_offset: this tells CP the start of preemption only list:
 * 16 + (4 * List_Offset)
 */
struct cpu_gpu_lock {
	u32 gpu_req;
	u32 cpu_req;
	u32 turn;
	u16 list_length;
	u16 list_offset;
};

/**
 * kgsl_hwlock - Try to get the spinlock
 * @lock: cpu_gpu_lock structure
 *
 * Spin while the GPU has the lock.
 *
 * Return: 0 if lock is successful, -EBUSY if timed out waiting for lock
 */
int kgsl_hwlock(struct cpu_gpu_lock *lock);

/**
 * kgsl_hwunlock - Release a previously grabbed lock
 * @lock: cpu_gpu_lock structure
 */
void kgsl_hwunlock(struct cpu_gpu_lock *lock);

/**
 * kgsl_regulator_disable_wait - Disable a regulator and wait for it
 * @reg: A &struct regulator handle
 * @timeout: Time to wait (in milliseconds)
 *
 * Disable the regulator and wait @timeout milliseconds for it to enter the
 * disabled state.
 *
 * Return: True if the regulator was disabled or false if it timed out
 */
bool kgsl_regulator_disable_wait(struct regulator *reg, u32 timeout);

/**
 * kgsl_of_clk_by_name - Return a clock device for a given name
 * @clks: Pointer to an array of bulk clk data
 * @count: Number of entries in the array
 * @id: Name of the clock to search for
 *
 * Returns: A pointer to the clock device for the given name or NULL if not
 * found
 */
struct clk *kgsl_of_clk_by_name(struct clk_bulk_data *clks, int count,
		const char *id);
/**
 * kgsl_regulator_set_voltage - Set voltage level for regulator
 * @dev: A &struct device pointer
 * @reg: A &struct regulator handle
 * @voltage: Voltage value to set regulator
 *
 * Return: 0 on success and negative error on failure.
 */
int kgsl_regulator_set_voltage(struct device *dev,
		struct regulator *reg, u32 voltage);

/**
 * kgsl_clk_set_rate - Set a clock to a given rate
 * @clks: Pointer to an array of bulk clk data
 * @count: Number of entries in the array
 * @id: Name of the clock to search for
 * @rate: Rate to st the clock to
 *
 * Return: 0 on success or negative error on failure
 */
int kgsl_clk_set_rate(struct clk_bulk_data *clks, int num_clks,
		const char *id, unsigned long rate);

/**
 * kgsl_zap_shader_load - Load a zap shader
 * @dev: Pointer to the struct device for the GPU platform device
 * @name: Basename of the zap shader to load (without the postfix)
 *
 * Load and install the zap shader named @name. Name should be specified without
 * the extension for example "a660_zap" instead of "a660_zap.mdt".
 *
 * Return: 0 on success or negative on failure
 */
int kgsl_zap_shader_load(struct device *dev, const char *name);

/**
 * kgsl_add_to_minidump - Add a physically contiguous section to minidump
 * @name: Name of the section
 * @virt_addr: Virtual address of the section
 * @phy_addr: Physical address of the section
 * @size: Size of the section
 */
void kgsl_add_to_minidump(char *name, u64 virt_addr, u64 phy_addr, size_t size);

/**
 * kgsl_remove_from_minidump - Remove a contiguous section from minidump
 * @name: Name of the section
 * @virt_addr: Virtual address of the section
 * @phy_addr: Physical address of the section
 * @size: Size of the section
 */
void kgsl_remove_from_minidump(char *name, u64 virt_addr, u64 phy_addr, size_t size);

/**
 * kgsl_add_va_to_minidump - Add a physically non-contiguous section to minidump
 * @dev: Pointer to the struct device for the GPU platform device
 * @name: Name of the section
 * @ptr: Virtual address of the section
 * @size: Size of the section
 */
int kgsl_add_va_to_minidump(struct device *dev, const char *name, void *ptr,
		size_t size);

/**
 * kgsl_qcom_va_md_register - Register driver with va-minidump
 * @device: Pointer to kgsl device
 */
void kgsl_qcom_va_md_register(struct kgsl_device *device);

#endif
