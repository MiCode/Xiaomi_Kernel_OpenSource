/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#ifndef _ADRENO_SYSFS_H_
#define _ADRENO_SYSFS_H_

/*
 * struct adreno_sysfs_attribute_u32 - Container for accessing and modifying
 * integers in kgsl via sysfs
 */
struct adreno_sysfs_attribute_u32 {
	/** #attr: The device attribute corresponding to the sysfs node */
	struct device_attribute attr;
	/**  @show: Function to show the value of the integer */
	u32 (*show)(struct adreno_device *adreno_dev);
	/**  @store: Function to store the value of the integer */
	int (*store)(struct adreno_device *adreno_dev, u32 val);
};

/*
 * struct adreno_sysfs_attribute_bool - Container for accessing and modifying
 * booleans in kgsl via sysfs
 */
struct adreno_sysfs_attribute_bool {
	/** #attr: The device attribute corresponding to the sysfs node */
	struct device_attribute attr;
	/**  @show: Function to show the value of the boolean */
	bool (*show)(struct adreno_device *adreno_dev);
	/**  @store: Function to store the value of the boolean */
	int (*store)(struct adreno_device *adreno_dev, bool val);
};

/* Helper function to modify an integer in kgsl */
ssize_t adreno_sysfs_store_u32(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

/* Helper function to read an integer in kgsl */
ssize_t adreno_sysfs_show_u32(struct device *dev,
	struct device_attribute *attr, char *buf);

/* Helper function to modify a boolean in kgsl */
ssize_t adreno_sysfs_store_bool(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);

/* Helper function to read a boolean in kgsl */
ssize_t adreno_sysfs_show_bool(struct device *dev,
	struct device_attribute *attr, char *buf);

#define ADRENO_SYSFS_BOOL(_name) \
const struct adreno_sysfs_attribute_bool adreno_attr_##_name = { \
	.attr = __ATTR(_name, 0644, adreno_sysfs_show_bool, \
			adreno_sysfs_store_bool), \
	.show = _ ## _name ## _show, \
	.store = _ ## _name ## _store, \
}

#define ADRENO_SYSFS_RO_BOOL(_name) \
const struct adreno_sysfs_attribute_bool adreno_attr_##_name = { \
	.attr = __ATTR(_name, 0444, adreno_sysfs_show_bool, NULL), \
	.show = _ ## _name ## _show, \
}

#define ADRENO_SYSFS_U32(_name) \
const struct adreno_sysfs_attribute_u32 adreno_attr_##_name = { \
	.attr = __ATTR(_name, 0644, adreno_sysfs_show_u32, \
			adreno_sysfs_store_u32), \
	.show = _ ## _name ## _show, \
	.store = _ ## _name ## _store, \
}

#define ADRENO_SYSFS_RO_U32(_name) \
const struct adreno_sysfs_attribute_u32 adreno_attr_##_name = { \
	.attr = __ATTR(_name, 0444, adreno_sysfs_show_u32, NULL), \
	.show = _ ## _name ## _show, \
}
#endif
