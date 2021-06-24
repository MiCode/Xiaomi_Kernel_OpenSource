/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT */

/*
 * (C) COPYRIGHT 2020-2021 Arm Limited or its affiliates. All rights reserved.
 */

/*
 * SysFS handling for the Mali reference arbiter
 */

#ifndef _MALI_ARBITER_SYSFS_H_
#define _MALI_ARBITER_SYSFS_H_

struct mali_arb;
struct mali_arb_sysfs;

/**
 * mali_arb_sysfs_create_root() - Creates the root for an Arbiter SysFS
 *                                directory
 * @arb: Arbiter data
 * @dev: Owning Resource Group device
 * @get_slice_assignment: Callback to return the slice assignment per partition
 * @set_slice_assignment: Callback to set the slice assignment for a partititon
 * @get_aw_assignment: Callback to return the AW assignment for a partition
 * @set_aw_assignment: Callback to set the AW assignment for a partititon
 *
 * Return: Pointer to the created SysFS entry, or NULL if creation failed
 */
struct mali_arb_sysfs *mali_arb_sysfs_create_root(struct mali_arb *arb,
	struct device *dev,
	int (*get_slice_assignment)(struct mali_arb *arb,
		uint8_t partition_index, uint32_t *buf),
	int (*set_slice_assignment)(struct mali_arb *arb,
		uint8_t partition_index, uint32_t slices, uint32_t *old_slices),
	int (*get_aw_assignment)(struct mali_arb *arb,
		uint8_t partition_index, uint32_t *buf),
	int (*set_aw_assignment)(struct mali_arb *arb,
		uint8_t partition_index, uint32_t access_windows,
		uint32_t *old_access_windows));

/**
 * mali_arb_sysfs_destroy_root() - Destroy the root for an Arbiter SysFS
 * @root: arbiter SysFS root to destroy
 *
 */
void mali_arb_sysfs_destroy_root(struct mali_arb_sysfs *root);

/**
 * mali_arb_sysfs_add_slice() - Adds a slice entry to an Arbiter SysFS root
 * @data: Arbiter SysFS data
 * @index: Slice index
 * @dev: Resource Group device
 *
 * Return: Error code
 */
int mali_arb_sysfs_add_slice(struct mali_arb_sysfs *data, uint8_t index,
	struct device *dev);

/**
 * mali_arb_sysfs_add_partition() - Adds a partition entry to an Arbiter SysFS
 *                                  root
 * @data: Arbiter SysFS data
 * @index: Partition index
 * @config_dev: Partition Config device
 *
 * Return: Error code
 */
int mali_arb_sysfs_add_partition(struct mali_arb_sysfs *data, uint8_t index,
	struct device *config_dev);

/**
 * mali_arb_sysfs_free() - Free the SysFS entries
 * @data: Arbiter SysFS data
 *
 * Return: Error code
 *
 * This should be called when the arbiter module is unloaded.
 */
void mali_arb_sysfs_free(struct mali_arb_sysfs *data);

#endif /* _MALI_ARBITER_SYSFS_H_ */
