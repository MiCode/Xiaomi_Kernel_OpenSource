/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2016-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _MSM_PROP_H_
#define _MSM_PROP_H_

#include <linux/list.h>
#include "msm_drv.h"

#define MSM_PROP_STATE_CACHE_SIZE	2

/**
 * struct msm_property_data - opaque structure for tracking per
 *                            drm-object per property stuff
 * @default_value: Default property value for this drm object
 * @force_dirty: Always dirty property on incoming sets, rather than checking
 *               for modified values
 */
struct msm_property_data {
	uint64_t default_value;
	bool force_dirty;
};

/**
 * struct msm_property_value - opaque structure for tracking per
 *                             drm-object per property stuff
 * @value: Current property value for this drm object
 * @blob: Pointer to associated blob data, if available
 * @dirty_node: Linked list node to track if property is dirty or not
 */
struct msm_property_value {
	uint64_t value;
	struct drm_property_blob *blob;
	struct list_head dirty_node;
};

/**
 * struct msm_property_info: Structure for property/state helper functions
 * @base: Pointer to base drm object (plane/crtc/etc.)
 * @dev: Pointer to drm device object
 * @property_array: Pointer to array for storing created property objects
 * @property_data: Pointer to array for storing private property data
 * @property_count: Total number of properties
 * @blob_count: Total number of blob properties, should be <= count
 * @install_request: Total number of property 'install' requests
 * @install_count: Total number of successful 'install' requests
 * @recent_idx: Index of property most recently accessed by set/get
 * @is_active: Whether or not drm component properties are 'active'
 * @state_cache: Cache of local states, to prevent alloc/free thrashing
 * @state_size: Size of local state structures
 * @state_cache_size: Number of state structures currently stored in state_cache
 * @property_lock: Mutex to protect local variables
 */
struct msm_property_info {
	struct drm_mode_object *base;
	struct drm_device *dev;

	struct drm_property **property_array;
	struct msm_property_data *property_data;
	uint32_t property_count;
	uint32_t blob_count;
	uint32_t install_request;
	uint32_t install_count;

	int32_t recent_idx;

	bool is_active;

	void *state_cache[MSM_PROP_STATE_CACHE_SIZE];
	uint32_t state_size;
	int32_t state_cache_size;
	struct mutex property_lock;
};

/**
 * struct msm_property_state - Structure for local property state information
 * @property_count: Total number of properties
 * @values: Pointer to array of msm_property_value objects
 * @dirty_list: List of all properties that have been 'atomic_set' but not
 *              yet cleared with 'msm_property_pop_dirty'
 */
struct msm_property_state {
	uint32_t property_count;
	struct msm_property_value *values;
	struct list_head dirty_list;
};

/**
 * msm_property_index_to_drm_property - get drm property struct from prop index
 * @info: Pointer to property info container struct
 * @property_idx: Property index
 * Returns: drm_property pointer associated with property index
 */
static inline
struct drm_property *msm_property_index_to_drm_property(
		struct msm_property_info *info, uint32_t property_idx)
{
	if (!info || property_idx >= info->property_count)
		return NULL;

	return info->property_array[property_idx];
}

/**
 * msm_property_get_default - query default value of a property
 * @info: Pointer to property info container struct
 * @property_idx: Property index
 * Returns: Default value for specified property
 */
static inline
uint64_t msm_property_get_default(struct msm_property_info *info,
		uint32_t property_idx)
{
	uint64_t rc = 0;

	if (!info)
		return 0;

	mutex_lock(&info->property_lock);
	if (property_idx < info->property_count)
		rc = info->property_data[property_idx].default_value;
	mutex_unlock(&info->property_lock);

	return rc;
}

/**
 * msm_property_set_is_active - set overall 'active' status for all properties
 * @info: Pointer to property info container struct
 * @is_active: New 'is active' status
 */
static inline
void msm_property_set_is_active(struct msm_property_info *info, bool is_active)
{
	if (info) {
		mutex_lock(&info->property_lock);
		info->is_active = is_active;
		mutex_unlock(&info->property_lock);
	}
}

/**
 * msm_property_get_is_active - query property 'is active' status
 * @info: Pointer to property info container struct
 * Returns: Current 'is active's status
 */
static inline
bool msm_property_get_is_active(struct msm_property_info *info)
{
	bool rc = false;

	if (info) {
		mutex_lock(&info->property_lock);
		rc = info->is_active;
		mutex_unlock(&info->property_lock);
	}

	return rc;
}

/**
 * msm_property_pop_dirty - determine next dirty property and clear
 *                          its dirty flag. Caller needs to acquire property
 *			  lock before calling this function and release
 *			  the lock when finished.
 * @info: Pointer to property info container struct
 * @property_state: Pointer to property state container struct
 * Returns: Valid msm property index on success,
 *          -EAGAIN if no dirty properties are available
 *          Property indicies returned from this function are similar
 *          to those returned by the msm_property_index function.
 */
int msm_property_pop_dirty(struct msm_property_info *info,
		struct msm_property_state *property_state);

/**
 * msm_property_init - initialize property info structure
 * @info: Pointer to property info container struct
 * @base: Pointer to base drm object (plane/crtc/etc.)
 * @dev: Pointer to drm device object
 * @property_array: Pointer to array for storing created property objects
 * @property_data: Pointer to array for storing private property data
 * @property_count: Total number of properties
 * @blob_count: Total number of blob properties, should be <= count
 * @state_size: Size of local state object
 */
void msm_property_init(struct msm_property_info *info,
		struct drm_mode_object *base,
		struct drm_device *dev,
		struct drm_property **property_array,
		struct msm_property_data *property_data,
		uint32_t property_count,
		uint32_t blob_count,
		uint32_t state_size);

/**
 * msm_property_destroy - destroy helper info structure
 *
 * @info: Pointer to property info container struct
 */
void msm_property_destroy(struct msm_property_info *info);

/**
 * msm_property_install_range - install standard drm range property
 * @info: Pointer to property info container struct
 * @name: Property name
 * @flags: Other property type flags, e.g. DRM_MODE_PROP_IMMUTABLE
 * @min: Min property value
 * @max: Max property value
 * @init: Default Property value
 * @property_idx: Property index
 */
void msm_property_install_range(struct msm_property_info *info,
		const char *name,
		int flags,
		uint64_t min,
		uint64_t max,
		uint64_t init,
		uint32_t property_idx);

/**
 * msm_property_install_volatile_range - install drm range property
 *	This function is similar to msm_property_install_range, but assumes
 *	that the property is meant for holding user pointers or descriptors
 *	that may reference volatile data without having an updated value.
 * @info: Pointer to property info container struct
 * @name: Property name
 * @flags: Other property type flags, e.g. DRM_MODE_PROP_IMMUTABLE
 * @min: Min property value
 * @max: Max property value
 * @init: Default Property value
 * @property_idx: Property index
 */
void msm_property_install_volatile_range(struct msm_property_info *info,
		const char *name,
		int flags,
		uint64_t min,
		uint64_t max,
		uint64_t init,
		uint32_t property_idx);

/**
 * msm_property_install_enum - install standard drm enum/bitmask property
 * @info: Pointer to property info container struct
 * @name: Property name
 * @flags: Other property type flags, e.g. DRM_MODE_PROP_IMMUTABLE
 * @is_bitmask: Set to non-zero to create a bitmask property, rather than an
 *              enumeration one
 * @values: Array of allowable enumeration/bitmask values
 * @num_values: Size of values array
 * @property_idx: Property index
 */
void msm_property_install_enum(struct msm_property_info *info,
		const char *name,
		int flags,
		int is_bitmask,
		const struct drm_prop_enum_list *values,
		int num_values,
		uint32_t property_idx);

/**
 * msm_property_install_blob - install standard drm blob property
 * @info: Pointer to property info container struct
 * @name: Property name
 * @flags: Extra flags for property creation
 * @property_idx: Property index
 */
void msm_property_install_blob(struct msm_property_info *info,
		const char *name,
		int flags,
		uint32_t property_idx);

/**
 * msm_property_install_get_status - query overal status of property additions
 * @info: Pointer to property info container struct
 * Returns: Zero if previous property install calls were all successful
 */
int msm_property_install_get_status(struct msm_property_info *info);

/**
 * msm_property_index - determine property index from drm_property ptr
 * @info: Pointer to property info container struct
 * @property: Incoming property pointer
 * Returns: Valid property index, or -EINVAL on error
 */
int msm_property_index(struct msm_property_info *info,
		struct drm_property *property);

/**
 * msm_property_set_dirty - forcibly flag a property as dirty
 * @info: Pointer to property info container struct
 * @property_state: Pointer to property state container struct
 * @property_idx: Property index
 * Returns: Zero on success
 */
int msm_property_set_dirty(struct msm_property_info *info,
		struct msm_property_state *property_state,
		int property_idx);

/**
 * msm_property_is_dirty - check whether a property is dirty
 *	Note: Intended for use during atomic_check before pop_dirty usage
 * @info: Pointer to property info container struct
 * @property_state: Pointer to property state container struct
 * @property_idx: Property index
 * Returns: true if dirty, false otherwise
 */
bool msm_property_is_dirty(
		struct msm_property_info *info,
		struct msm_property_state *property_state,
		uint32_t property_idx);

/**
 * msm_property_atomic_set - helper function for atomic property set callback
 * @info: Pointer to property info container struct
 * @property_state: Pointer to local state structure
 * @property: Incoming property pointer
 * @val: Incoming property value
 * Returns: Zero on success
 */
int msm_property_atomic_set(struct msm_property_info *info,
		struct msm_property_state *property_state,
		struct drm_property *property,
		uint64_t val);

/**
 * msm_property_atomic_get - helper function for atomic property get callback
 * @info: Pointer to property info container struct
 * @property_state: Pointer to local state structure
 * @property: Incoming property pointer
 * @val: Pointer to variable for receiving property value
 * Returns: Zero on success
 */
int msm_property_atomic_get(struct msm_property_info *info,
		struct msm_property_state *property_state,
		struct drm_property *property,
		uint64_t *val);

/**
 * msm_property_alloc_state - helper function for allocating local state objects
 * @info: Pointer to property info container struct
 */
void *msm_property_alloc_state(struct msm_property_info *info);

/**
 * msm_property_reset_state - helper function for state reset callback
 * @info: Pointer to property info container struct
 * @state: Pointer to local state structure
 * @property_state: Pointer to property state container struct
 * @property_values: Pointer to property values cache array
 */
void msm_property_reset_state(struct msm_property_info *info, void *state,
		struct msm_property_state *property_state,
		struct msm_property_value *property_values);

/**
 * msm_property_duplicate_state - helper function for duplicate state cb
 * @info: Pointer to property info container struct
 * @old_state: Pointer to original state structure
 * @state: Pointer to newly created state structure
 * @property_state: Pointer to destination property state container struct
 * @property_values: Pointer to property values cache array
 */
void msm_property_duplicate_state(struct msm_property_info *info,
		void *old_state,
		void *state,
		struct msm_property_state *property_state,
		struct msm_property_value *property_values);

/**
 * msm_property_destroy_state - helper function for destroy state cb
 * @info: Pointer to property info container struct
 * @state: Pointer to local state structure
 * @property_state: Pointer to property state container struct
 */
void msm_property_destroy_state(struct msm_property_info *info,
		void *state,
		struct msm_property_state *property_state);

/**
 * msm_property_get_blob - obtain cached data pointer for drm blob property
 * @info: Pointer to property info container struct
 * @property_state: Pointer to property state container struct
 * @byte_len: Optional pointer to variable for accepting blob size
 * @property_idx: Property index
 * Returns: Pointer to blob data
 */
void *msm_property_get_blob(struct msm_property_info *info,
		struct msm_property_state *property_state,
		size_t *byte_len,
		uint32_t property_idx);

/**
 * msm_property_set_blob - update blob property on a drm object
 * This function updates the blob property value of the given drm object. Its
 * intended use is to update blob properties that have been created with the
 * DRM_MODE_PROP_IMMUTABLE flag set.
 * @info: Pointer to property info container struct
 * @blob_reference: Reference to a pointer that holds the created data blob
 * @blob_data: Pointer to blob data
 * @byte_len: Length of blob data, in bytes
 * @property_idx: Property index
 * Returns: Zero on success
 */
int msm_property_set_blob(struct msm_property_info *info,
		struct drm_property_blob **blob_reference,
		void *blob_data,
		size_t byte_len,
		uint32_t property_idx);

/**
 * msm_property_set_property - update property on a drm object
 * This function updates the property value of the given drm object. Its
 * intended use is to update properties that have been created with the
 * DRM_MODE_PROP_IMMUTABLE flag set.
 * Note: This function cannot be called on a blob.
 * @info: Pointer to property info container struct
 * @property_state: Pointer to property state container struct
 * @property_idx: Property index
 * @val: value of the property to set
 * Returns: Zero on success
 */
int msm_property_set_property(struct msm_property_info *info,
		struct msm_property_state *property_state,
		uint32_t property_idx,
		uint64_t val);

#endif /* _MSM_PROP_H_ */

