/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __LLCC_QCOM__
#define __LLCC_QCOM__

/**
 * llcc_slice_desc - Cache slice descriptor
 * @llcc_slice_id: llcc slice id
 * @llcc_slice_size: Size allocated for the llcc slice
 * @dev: pointer to llcc device
 */
struct llcc_slice_desc {
	int llcc_slice_id;
	size_t llcc_slice_size;
	struct device *dev;
};

/**
 * llcc_slice_config - Data associated with the llcc slice
 * @name: name of the use case associated with the llcc slice
 * @usecase_id: usecase id for which the llcc slice is used
 * @slice_id: llcc slice id assigned to each slice
 * @max_cap: maximum capacity of the llcc slice
 * @priority: priority of the llcc slice
 * @fixed_size: whether the llcc slice can grow beyond its size
 * @bonus_ways: bonus ways associated with llcc slice
 * @res_ways: reserved ways associated with llcc slice
 * @cache_mode: mode of the llcce slice
 * @probe_target_ways: Probe only reserved and bonus ways on a cache miss
 * @dis_cap_alloc: Disable capacity based allocation
 * @retain_on_pc: Retain through power collapse
 * @activate_on_init: activate the slice on init
 */
struct llcc_slice_config {
	const char *name;
	int usecase_id;
	int slice_id;
	u32 max_cap;
	u32 priority;
	bool fixed_size;
	u32 bonus_ways;
	u32 res_ways;
	u32 cache_mode;
	u32 probe_target_ways;
	bool dis_cap_alloc;
	bool retain_on_pc;
	u32 activate_on_init;
};

#ifdef CONFIG_QCOM_LLCC
/**
 * llcc_slice_getd - get llcc slice descriptor
 * @dev: Device pointer of the client
 * @name: Name of the use case
 */
struct llcc_slice_desc *llcc_slice_getd(struct device *dev, const char *name);

/**
 * llcc_slice_putd - llcc slice descritpor
 * @desc: Pointer to llcc slice descriptor
 */
void llcc_slice_putd(struct llcc_slice_desc *desc);

/**
 * llcc_get_slice_id - get slice id
 * @desc: Pointer to llcc slice descriptor
 */
int llcc_get_slice_id(struct llcc_slice_desc *desc);

/**
 * llcc_get_slice_size - llcc slice size
 * @desc: Pointer to llcc slice descriptor
 */
size_t llcc_get_slice_size(struct llcc_slice_desc *desc);

/**
 * llcc_slice_activate - Activate the llcc slice
 * @desc: Pointer to llcc slice descriptor
 */
int llcc_slice_activate(struct llcc_slice_desc *desc);

/**
 * llcc_slice_deactivate - Deactivate the llcc slice
 * @desc: Pointer to llcc slice descriptor
 */
int llcc_slice_deactivate(struct llcc_slice_desc *desc);

/**
 * qcom_llcc_probe - program the sct table
 * @pdev: platform device pointer
 * @table: soc sct table
 */
int qcom_llcc_probe(struct platform_device *pdev,
		      const struct llcc_slice_config *table, u32 sz);
/**
 * qcom_llcc_remove - clean up llcc driver
 * @pdev: platform driver pointer
 */
int qcom_llcc_remove(struct platform_device *pdev);
#else
static inline struct llcc_slice_desc *llcc_slice_getd(struct device *dev,
			const char *name)
{
	return NULL;
}

static inline void llcc_slice_putd(struct llcc_slice_desc *desc)
{

};

static inline int llcc_get_slice_id(struct llcc_slice_desc *desc)
{
	return -EINVAL;
}

static inline size_t llcc_get_slice_size(struct llcc_slice_desc *desc)
{
	return 0;
}
static inline int llcc_slice_activate(struct llcc_slice_desc *desc)
{
	return -EINVAL;
}

static inline int llcc_slice_deactivate(struct llcc_slice_desc *desc)
{
	return -EINVAL;
}
static inline int qcom_llcc_probe(struct platform_device *pdev,
		      const struct llcc_slice_config *table, u32 sz)
{
	return -ENODEV;
}

static inline int qcom_llcc_remove(struct platform_device *pdev)
{
	return -ENODEV;
}
#endif

#endif
