/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT  */

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */


/*
 * Public interface for the Mali Resource Group Module.
 */

#ifndef _MALI_GPU_RESOURCE_GROUP_H_
#define _MALI_GPU_RESOURCE_GROUP_H_

/*
 * Mali resource group interface version
 *
 * This specifies the current version of the resource group interface.
 * Whenever the resource group interface change in its functionality,
 * so that integration effort is required, the version number will be increased.
 * Each module which interact with the resource group interface must make an
 * effort to check that it implements the correct version.
 *
 * Version history:
 * 1 - Added the Mali resource group interface.
 */
#define MALI_RESOURCE_GROUP_VERSION 1

/**
 * enum mali_gpu_slice_tiler_type - Identifiers for the different slice tiler
 *                                  types.
 * @MALI_GPU_TILER_COMPACT: Compact tiler
 * @MALI_GPU_TILER_HIGH_PERFORMANCE: High performance tiler
 */
enum mali_gpu_slice_tiler_type {
	MALI_GPU_TILER_COMPACT,
	MALI_GPU_TILER_HIGH_PERFORMANCE
};

/**
 * struct mali_ptm_rg_ops - Specific callbacks from the resource group
 * This struct contains specific resource group callbacks. It is used by other
 * modules to interface with the resource group module.
 * The resource group driver must set it as its device data on any
 * platform config devices using dev_set_drvdata() so that can be retrieved
 * by modules interfacing with the device using dev_get_drvdata().
 */
struct mali_ptm_rg_ops {
	/**
	 * @get_slice_tiler_type: Get the slice tiler type.
	 * @dev -   Pointer to the partition config module device.
	 * @slice - Slice index.
	 * @type -  Pointer to the result.
	 *
	 * Gets the tiler type of a given slice.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*get_slice_tiler_type)(struct device *dev, uint8_t slice,
		enum mali_gpu_slice_tiler_type *type);

	/**
	 * @get_slices_core_mask: Get a mask of the cores in all slices.
	 * @dev -              Pointer to the resource group device.
	 * @core_mask -        Pointer to variable to receive a mask of the
	 *                     cores for all slices in the PTM.
	 * @core_mask_stride - Pointer to variable to receive the number of bits
	 *                     allocated to each slice in the mask.
	 *
	 * Reads PTM_SLICE_CORES register which identifies the number of cores
	 * in each slice and  returns a mask with the cores allocated along with
	 * the number of bits allocated to each slice in this mask.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*get_slices_core_mask)(struct device *dev,
			uint64_t *core_mask, uint8_t *core_mask_stride);

	/**
	 * @get_slice_mask: Get the slice mask.
	 * @dev -  Pointer to the resource group module device.
	 * @mask - Pointer to variable to receive the mask.
	 *
	 * Gets a bitmap, of 1 bit per slice, of the slices assigned to this
	 * Resource Group.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*get_slice_mask)(struct device *dev, uint32_t *mask);

	/**
	 * @get_partition_mask: Get the partition mask.
	 * @dev -  Pointer to the resource group module device.
	 * @mask - Pointer to variable to receive the mask.
	 *
	 * Gets a bitmap, of 1 bit per partition, of the partitions assigned to
	 * this Resource Group.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*get_partition_mask)(struct device *dev, uint32_t *mask);

	/**
	 * @get_aw_mask: Get the Access Window mask.
	 * @dev -  Pointer to the resource group module device.
	 * @mask - Pointer to variable to receive the mask.
	 *
	 * Gets a bitmap, of 1 bit per Access Window, of the Access Windows
	 * assigned to this Resource Group.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*get_aw_mask)(struct device *dev, uint32_t *mask);

	/**
	 * @poweron_slices: Power on slices.
	 * @dev -  Pointer to the resource group module device.
	 * @mask - Mask of slices to be powered on.
	 *
	 * Power on the slices indicated by mask parameter.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*poweron_slices)(struct device *dev, uint32_t mask);

	/**
	 * @poweroff_slices: Power off slices.
	 * @dev -   Pointer to the resource group module device.
	 * @mask -  Mask of slices to be powered off.
	 *
	 * Power off the slices indicated by mask parameter.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*poweroff_slices)(struct device *dev, uint32_t mask);

	/**
	 * @get_powered_slices_mask: Get a mask of the powered on slices.
	 * @dev -  Pointer to the resource group module device.
	 * @mask - Pointer to variable to receive the mask.
	 *
	 * Gets a bitmap, of 1 bit per Slice, of the slices assigned to this
	 * Resource Group that are powered.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*get_powered_slices_mask)(struct device *dev, uint32_t *mask);

	/**
	 * @enable_slices: Release slices from reset.
	 * @dev -   Pointer to the resource group module device.
	 * @mask -  Mask of slices to be enabled.
	 *
	 * Release the slices indicated by mask parameter from reset.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*enable_slices)(struct device *dev, uint32_t mask);

	/**
	 * @reset_slices: Reset slices.
	 * @dev -   Pointer to the resource group module device.
	 * @mask -  Mask of slices to be reset.
	 *
	 * Put the slices indicated by mask parameter into reset.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*reset_slices)(struct device *dev, uint32_t mask);

	/**
	 * @get_enabled_slices_mask: Get a mask of the enabled slices.
	 * @dev -  Pointer to the resource group module device.
	 * @mask - Pointer to variable to receive the mask.
	 *
	 * Gets a bitmap, of 1 bit per Slice, of the slices assigned to this
	 * Resource Group that are enabled.
	 *
	 * Return: 0 if successful, otherwise a negative error code.
	 */
	int (*get_enabled_slices_mask)(struct device *dev, uint32_t *mask);
};

#endif /* _MALI_GPU_RESOURCE_GROUP_H_ */
