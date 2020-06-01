/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_CORESIGHT_H_
#define _ADRENO_CORESIGHT_H_

#include <linux/device.h>

struct adreno_device;

/**
 * struct adreno_coresight_register - Definition for a coresight (tracebus)
 * debug register
 */
struct adreno_coresight_register {
	/** @offset: Offset of the debug register in the KGSL register space */
	unsigned int offset;
	/** @initial: Default value to write when coresight is enabled */
	unsigned int initial;
	/**
	 * @value: Current shadow value of the register (to be reprogrammed
	 * after power collapse)
	 */
	unsigned int value;
};

/**
 * struct adreno_coresight_attr - Local attribute struct for coresight sysfs
 *
 * files
 */
struct adreno_coresight_attr {
	/** @attr: Base device attribute */
	struct device_attribute attr;
	/**
	 * @reg: Pointer to the &struct adreno_coresight_register definition
	 * for this register
	 */
	struct adreno_coresight_register *reg;
};

/**
 * adreno_coresight_show_register - Callback function for sysfs show
 * @device: Pointer to a device handle
 * @attr: Pointer to the device attribute
 * @buf: Contains the output buffer for sysfs
 *
 * Callback function to write the value of the register into the sysfs node.
 * Return: The size of the data written to the buffer or negative on error.
 */
ssize_t adreno_coresight_show_register(struct device *device,
		struct device_attribute *attr, char *buf);

/**
 * adreno_coresight_show_register - Callback function for sysfs store
 * @device: Pointer to a device handle
 * @attr: Pointer to the device attribute
 * @buf: Contains the input buffer for sysfs
 * @size: Size of the data stored in buf
 *
 * Callback function to read the value of a register from a sysfs node.
 * Return: The size of the data consumed or negative on error.
 */
ssize_t adreno_coresight_store_register(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size);

#define ADRENO_CORESIGHT_ATTR(_attrname, _reg) \
	struct adreno_coresight_attr coresight_attr_##_attrname  = { \
		__ATTR(_attrname, 0644, \
		adreno_coresight_show_register, \
		adreno_coresight_store_register), \
		(_reg), }

/**
 * struct adreno_coresight - GPU specific coresight definition
 */
struct adreno_coresight {
	/**
	 * @registers: Array of GPU specific registers to configure trace
	 * bus output
	 */
	struct adreno_coresight_register *registers;
	/** @count: Number of registers in the array */
	unsigned int count;
	/** @groups: Pointer to an attribute list of control files */
	const struct attribute_group **groups;
	/** @atid: The unique ATID value of the coresight device */
	unsigned int atid;
};

#ifdef CONFIG_QCOM_KGSL_CORESIGHT
/**
 * adreno_coresight_init - Initialize coresight for the GPU device
 * @adreno_dev: An Adreno GPU device handle
 *
 * Initialize devices for the GPU target.
 */
void adreno_coresight_init(struct adreno_device *adreno_dev);

/**
 * adreno_coresight_start - Reprogram coresight registers after power collapse
 * @adreno_dev: An Adreno GPU device handle
 *
 * Reprogram the cached values to the coresight registers on power up.
 */
void adreno_coresight_start(struct adreno_device *adreno_dev);

/**
 * adreno_coresight_stop - Reprogram coresight registers after power collapse
 * @adreno_dev: An Adreno GPU device handle
 *
 * Cache the current coresight register values so they can be restored after
 * power collapse.
 */
void adreno_coresight_stop(struct adreno_device *adreno_dev);

/**
 * adreno_coresight_remove - Destroy active coresight devices
 * @adreno_dev: An Adreno GPU device handle
 *
 * Destroy any active coresight devices.
 */
void adreno_coresight_remove(struct adreno_device *adreno_dev);
#else
static inline void adreno_coresight_init(struct adreno_device *adreno_dev) { }
static inline void adreno_coresight_start(struct adreno_device *adreno_dev) { }
static inline void adreno_coresight_stop(struct adreno_device *adreno_dev) { }
static inline void adreno_coresight_remove(struct adreno_device *adreno_dev) { }
#endif
#endif
