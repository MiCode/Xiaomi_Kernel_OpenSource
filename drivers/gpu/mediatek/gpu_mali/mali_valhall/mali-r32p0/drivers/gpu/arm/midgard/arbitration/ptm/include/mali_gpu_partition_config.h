/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT */

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * Public interface for Mali Partition Config Module
 */

#ifndef _MALI_GPU_PARTITION_CONFIG_H_
#define _MALI_GPU_PARTITION_CONFIG_H_

/*
 * Mali partition config interface version
 *
 * This specifies the current version of the partition config interface.
 * Whenever the partition config interface change in its functionality,
 * so that integration effort is required, the version number will be increased.
 * Each module which interact with the partition config interface must make an
 * effort to check that it implements the correct version.
 *
 * Version history:
 * 1 - Added the Mali partition config interface.
 */
#define MALI_PARTITION_CONFIG_VERSION 1

/**
 * struct part_cfg_ops - Partition Config Callbacks
 * @assign_slices: Assign slices to the partition.
 *
 * This struct contains partition config callbacks. It is used by other
 * modules to interface with the partition config module.
 * The partition config driver must set it as its device data on any
 * platform config devices using dev_set_drvdata() so that can be retrieved
 * by modules interfacing with the device using dev_get_drvdata().
 */
struct part_cfg_ops {
	/**
	 * @assign_slices: Assign slices to the partition.
	 * @dev:      Pointer to the partition config module device.
	 * @slices:   Bit mask of slices to configure
	 *            (bit0=slice0, bit1=slice1,...)
	 *
	 * After this function completes, the specified slices will be assigned
	 * to the partition.
	 * The function call is blocking and should not be called concurrently
	 * for the same device from different threads
	 *
	 * Return:    0 if successful, otherwise a negative error code.
	 */
	int (*assign_slices)(struct device *dev, uint32_t slices);
};

#endif /* _MALI_GPU_PARTITION_CONFIG_H_ */
