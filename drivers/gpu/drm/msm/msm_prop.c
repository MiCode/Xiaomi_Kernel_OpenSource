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

#include "msm_prop.h"

void msm_property_init(struct msm_property_info *info,
		struct drm_mode_object *base,
		struct drm_device *dev,
		struct drm_property **property_array,
		struct msm_property_data *property_data,
		uint32_t property_count,
		uint32_t blob_count,
		uint32_t state_size)
{
	int i;

	/* prevent access if any of these are NULL */
	if (!base || !dev || !property_array || !property_data) {
		property_count = 0;
		blob_count = 0;

		DRM_ERROR("invalid arguments, forcing zero properties\n");
		return;
	}

	/* can't have more blob properties than total properties */
	if (blob_count > property_count) {
		blob_count = property_count;

		DBG("Capping number of blob properties to %d", blob_count);
	}

	if (!info) {
		DRM_ERROR("info pointer is NULL\n");
	} else {
		info->base = base;
		info->dev = dev;
		info->property_array = property_array;
		info->property_data = property_data;
		info->property_count = property_count;
		info->blob_count = blob_count;
		info->install_request = 0;
		info->install_count = 0;
		info->recent_idx = 0;
		info->is_active = false;
		info->state_size = state_size;
		info->state_cache_size = 0;
		mutex_init(&info->property_lock);

		memset(property_data,
				0,
				sizeof(struct msm_property_data) *
				property_count);
		INIT_LIST_HEAD(&info->dirty_list);

		for (i = 0; i < property_count; ++i)
			INIT_LIST_HEAD(&property_data[i].dirty_node);
	}
}

void msm_property_destroy(struct msm_property_info *info)
{
	if (!info)
		return;

	/* reset dirty list */
	INIT_LIST_HEAD(&info->dirty_list);

	/* free state cache */
	while (info->state_cache_size > 0)
		kfree(info->state_cache[--(info->state_cache_size)]);

	mutex_destroy(&info->property_lock);
}

int msm_property_pop_dirty(struct msm_property_info *info)
{
	struct list_head *item;
	int rc = 0;

	if (!info) {
		DRM_ERROR("invalid info\n");
		return -EINVAL;
	}

	mutex_lock(&info->property_lock);
	if (list_empty(&info->dirty_list)) {
		rc = -EAGAIN;
	} else {
		item = info->dirty_list.next;
		list_del_init(item);
		rc = container_of(item, struct msm_property_data, dirty_node)
			- info->property_data;
		DRM_DEBUG_KMS("property %d dirty\n", rc);
	}
	mutex_unlock(&info->property_lock);

	return rc;
}

/**
 * _msm_property_set_dirty_no_lock - flag given property as being dirty
 *                                   This function doesn't mutex protect the
 *                                   dirty linked list.
 * @info: Pointer to property info container struct
 * @property_idx: Property index
 */
static void _msm_property_set_dirty_no_lock(
		struct msm_property_info *info,
		uint32_t property_idx)
{
	if (!info || property_idx >= info->property_count) {
		DRM_ERROR("invalid argument(s), info %pK, idx %u\n",
				info, property_idx);
		return;
	}

	/* avoid re-inserting if already dirty */
	if (!list_empty(&info->property_data[property_idx].dirty_node)) {
		DRM_DEBUG_KMS("property %u already dirty\n", property_idx);
		return;
	}

	list_add_tail(&info->property_data[property_idx].dirty_node,
			&info->dirty_list);
}

/**
 * _msm_property_install_integer - install standard drm range property
 * @info: Pointer to property info container struct
 * @name: Property name
 * @flags: Other property type flags, e.g. DRM_MODE_PROP_IMMUTABLE
 * @min: Min property value
 * @max: Max property value
 * @init: Default Property value
 * @property_idx: Property index
 * @force_dirty: Whether or not to filter 'dirty' status on unchanged values
 */
static void _msm_property_install_integer(struct msm_property_info *info,
		const char *name, int flags, uint64_t min, uint64_t max,
		uint64_t init, uint32_t property_idx, bool force_dirty)
{
	struct drm_property **prop;

	if (!info)
		return;

	++info->install_request;

	if (!name || (property_idx >= info->property_count)) {
		DRM_ERROR("invalid argument(s), %s\n", name ? name : "null");
	} else {
		prop = &info->property_array[property_idx];
		/*
		 * Properties need to be attached to each drm object that
		 * uses them, but only need to be created once
		 */
		if (*prop == 0) {
			*prop = drm_property_create_range(info->dev,
					flags, name, min, max);
			if (*prop == 0)
				DRM_ERROR("create %s property failed\n", name);
		}

		/* save init value for later */
		info->property_data[property_idx].default_value = init;
		info->property_data[property_idx].force_dirty = force_dirty;

		/* always attach property, if created */
		if (*prop) {
			drm_object_attach_property(info->base, *prop, init);
			++info->install_count;
		}
	}
}

void msm_property_install_range(struct msm_property_info *info,
		const char *name, int flags, uint64_t min, uint64_t max,
		uint64_t init, uint32_t property_idx)
{
	_msm_property_install_integer(info, name, flags,
			min, max, init, property_idx, false);
}

void msm_property_install_volatile_range(struct msm_property_info *info,
		const char *name, int flags, uint64_t min, uint64_t max,
		uint64_t init, uint32_t property_idx)
{
	_msm_property_install_integer(info, name, flags,
			min, max, init, property_idx, true);
}

void msm_property_install_rotation(struct msm_property_info *info,
		unsigned int supported_rotations, uint32_t property_idx)
{
	struct drm_property **prop;

	if (!info)
		return;

	++info->install_request;

	if (property_idx >= info->property_count) {
		DRM_ERROR("invalid property index %d\n", property_idx);
	} else {
		prop = &info->property_array[property_idx];
		/*
		 * Properties need to be attached to each drm object that
		 * uses them, but only need to be created once
		 */
		if (*prop == 0) {
			*prop = drm_mode_create_rotation_property(info->dev,
					supported_rotations);
			if (*prop == 0)
				DRM_ERROR("create rotation property failed\n");
		}

		/* save init value for later */
		info->property_data[property_idx].default_value = 0;
		info->property_data[property_idx].force_dirty = false;

		/* always attach property, if created */
		if (*prop) {
			drm_object_attach_property(info->base, *prop, 0);
			++info->install_count;
		}
	}
}

void msm_property_install_enum(struct msm_property_info *info,
		const char *name, int flags, int is_bitmask,
		const struct drm_prop_enum_list *values, int num_values,
		uint32_t property_idx)
{
	struct drm_property **prop;

	if (!info)
		return;

	++info->install_request;

	if (!name || !values || !num_values ||
			(property_idx >= info->property_count)) {
		DRM_ERROR("invalid argument(s), %s\n", name ? name : "null");
	} else {
		prop = &info->property_array[property_idx];
		/*
		 * Properties need to be attached to each drm object that
		 * uses them, but only need to be created once
		 */
		if (*prop == 0) {
			/* 'bitmask' is a special type of 'enum' */
			if (is_bitmask)
				*prop = drm_property_create_bitmask(info->dev,
						DRM_MODE_PROP_BITMASK | flags,
						name, values, num_values, -1);
			else
				*prop = drm_property_create_enum(info->dev,
						DRM_MODE_PROP_ENUM | flags,
						name, values, num_values);
			if (*prop == 0)
				DRM_ERROR("create %s property failed\n", name);
		}

		/* save init value for later */
		info->property_data[property_idx].default_value = 0;
		info->property_data[property_idx].force_dirty = false;

		/* always attach property, if created */
		if (*prop) {
			drm_object_attach_property(info->base, *prop, 0);
			++info->install_count;
		}
	}
}

void msm_property_install_blob(struct msm_property_info *info,
		const char *name, int flags, uint32_t property_idx)
{
	struct drm_property **prop;

	if (!info)
		return;

	++info->install_request;

	if (!name || (property_idx >= info->blob_count)) {
		DRM_ERROR("invalid argument(s), %s\n", name ? name : "null");
	} else {
		prop = &info->property_array[property_idx];
		/*
		 * Properties need to be attached to each drm object that
		 * uses them, but only need to be created once
		 */
		if (*prop == 0) {
			/* use 'create' for blob property place holder */
			*prop = drm_property_create(info->dev,
					DRM_MODE_PROP_BLOB | flags, name, 0);
			if (*prop == 0)
				DRM_ERROR("create %s property failed\n", name);
		}

		/* save init value for later */
		info->property_data[property_idx].default_value = 0;
		info->property_data[property_idx].force_dirty = true;

		/* always attach property, if created */
		if (*prop) {
			drm_object_attach_property(info->base, *prop, -1);
			++info->install_count;
		}
	}
}

int msm_property_install_get_status(struct msm_property_info *info)
{
	int rc = -ENOMEM;

	if (info && (info->install_request == info->install_count))
		rc = 0;

	return rc;
}

int msm_property_index(struct msm_property_info *info,
		struct drm_property *property)
{
	uint32_t count;
	int32_t idx;
	int rc = -EINVAL;

	if (!info || !property) {
		DRM_ERROR("invalid argument(s)\n");
	} else {
		/*
		 * Linear search, but start from last found index. This will
		 * help if any single property is accessed multiple times in a
		 * row. Ideally, we could keep a list of properties sorted in
		 * the order of most recent access, but that may be overkill
		 * for now.
		 */
		mutex_lock(&info->property_lock);
		idx = info->recent_idx;
		count = info->property_count;
		while (count) {
			--count;

			/* stop searching on match */
			if (info->property_array[idx] == property) {
				info->recent_idx = idx;
				rc = idx;
				break;
			}

			/* move to next valid index */
			if (--idx < 0)
				idx = info->property_count - 1;
		}
		mutex_unlock(&info->property_lock);
	}

	return rc;
}

int msm_property_atomic_set(struct msm_property_info *info,
		uint64_t *property_values,
		struct drm_property_blob **property_blobs,
		struct drm_property *property, uint64_t val)
{
	struct drm_property_blob *blob;
	int property_idx, rc = -EINVAL;

	property_idx = msm_property_index(info, property);
	if (!info || (property_idx == -EINVAL) || !property_values) {
		DRM_DEBUG("Invalid argument(s)\n");
	} else {
		/* extra handling for incoming properties */
		mutex_lock(&info->property_lock);
		if ((property->flags & DRM_MODE_PROP_BLOB) &&
			(property_idx < info->blob_count) &&
			property_blobs) {
			/* DRM lookup also takes a reference */
			blob = drm_property_lookup_blob(info->dev,
				(uint32_t)val);
			if (!blob) {
				DRM_ERROR("blob not found\n");
				val = 0;
			} else {
				DBG("Blob %u saved", blob->base.id);
				val = blob->base.id;

				/* save blob - need to clear previous ref */
				if (property_blobs[property_idx])
					drm_property_unreference_blob(
						property_blobs[property_idx]);
				property_blobs[property_idx] = blob;
			}
		}

		/* update value and flag as dirty */
		if (property_values[property_idx] != val ||
				info->property_data[property_idx].force_dirty) {
			property_values[property_idx] = val;
			_msm_property_set_dirty_no_lock(info, property_idx);

			DBG("%s - %lld", property->name, val);
		}
		mutex_unlock(&info->property_lock);
		rc = 0;
	}

	return rc;
}

int msm_property_atomic_get(struct msm_property_info *info,
		uint64_t *property_values,
		struct drm_property_blob **property_blobs,
		struct drm_property *property, uint64_t *val)
{
	int property_idx, rc = -EINVAL;

	property_idx = msm_property_index(info, property);
	if (!info || (property_idx == -EINVAL) || !property_values || !val) {
		DRM_DEBUG("Invalid argument(s)\n");
	} else {
		mutex_lock(&info->property_lock);
		*val = property_values[property_idx];
		mutex_unlock(&info->property_lock);
		rc = 0;
	}

	return rc;
}

void *msm_property_alloc_state(struct msm_property_info *info)
{
	void *state = NULL;

	if (!info) {
		DRM_ERROR("invalid property info\n");
		return NULL;
	}

	mutex_lock(&info->property_lock);
	if (info->state_cache_size)
		state = info->state_cache[--(info->state_cache_size)];
	mutex_unlock(&info->property_lock);

	if (!state && info->state_size)
		state = kmalloc(info->state_size, GFP_KERNEL);

	if (!state)
		DRM_ERROR("failed to allocate state\n");

	return state;
}

/**
 * _msm_property_free_state - helper function for freeing local state objects
 * @info: Pointer to property info container struct
 * @st: Pointer to state object
 */
static void _msm_property_free_state(struct msm_property_info *info, void *st)
{
	if (!info || !st)
		return;

	mutex_lock(&info->property_lock);
	if (info->state_cache_size < MSM_PROP_STATE_CACHE_SIZE)
		info->state_cache[(info->state_cache_size)++] = st;
	else
		kfree(st);
	mutex_unlock(&info->property_lock);
}

void msm_property_reset_state(struct msm_property_info *info, void *state,
		uint64_t *property_values,
		struct drm_property_blob **property_blobs)
{
	uint32_t i;

	if (!info) {
		DRM_ERROR("invalid property info\n");
		return;
	}

	if (state)
		memset(state, 0, info->state_size);

	/*
	 * Assign default property values. This helper is mostly used
	 * to initialize newly created state objects.
	 */
	if (property_values)
		for (i = 0; i < info->property_count; ++i)
			property_values[i] =
				info->property_data[i].default_value;

	if (property_blobs)
		for (i = 0; i < info->blob_count; ++i)
			property_blobs[i] = 0;
}

void msm_property_duplicate_state(struct msm_property_info *info,
		void *old_state, void *state,
		uint64_t *property_values,
		struct drm_property_blob **property_blobs)
{
	uint32_t i;

	if (!info || !old_state || !state) {
		DRM_ERROR("invalid argument(s)\n");
		return;
	}

	memcpy(state, old_state, info->state_size);

	if (property_blobs) {
		/* add ref count for blobs */
		for (i = 0; i < info->blob_count; ++i)
			if (property_blobs[i])
				drm_property_reference_blob(property_blobs[i]);
	}
}

void msm_property_destroy_state(struct msm_property_info *info, void *state,
		uint64_t *property_values,
		struct drm_property_blob **property_blobs)
{
	uint32_t i;

	if (!info || !state) {
		DRM_ERROR("invalid argument(s)\n");
		return;
	}
	if (property_blobs) {
		/* remove ref count for blobs */
		for (i = 0; i < info->blob_count; ++i)
			if (property_blobs[i])
				drm_property_unreference_blob(
						property_blobs[i]);
	}

	_msm_property_free_state(info, state);
}

void *msm_property_get_blob(struct msm_property_info *info,
		struct drm_property_blob **property_blobs,
		size_t *byte_len,
		uint32_t property_idx)
{
	struct drm_property_blob *blob;
	size_t len = 0;
	void *rc = 0;

	if (!info || !property_blobs || (property_idx >= info->blob_count)) {
		DRM_ERROR("invalid argument(s)\n");
	} else {
		blob = property_blobs[property_idx];
		if (blob) {
			len = blob->length;
			rc = &blob->data;
		}
	}

	if (byte_len)
		*byte_len = len;

	return rc;
}

int msm_property_set_blob(struct msm_property_info *info,
		struct drm_property_blob **blob_reference,
		void *blob_data,
		size_t byte_len,
		uint32_t property_idx)
{
	struct drm_property_blob *blob = NULL;
	int rc = -EINVAL;

	if (!info || !blob_reference || (property_idx >= info->blob_count)) {
		DRM_ERROR("invalid argument(s)\n");
	} else {
		/* create blob */
		if (blob_data && byte_len) {
			blob = drm_property_create_blob(info->dev,
					byte_len,
					blob_data);
			if (IS_ERR_OR_NULL(blob)) {
				rc = PTR_ERR(blob);
				DRM_ERROR("failed to create blob, %d\n", rc);
				goto exit;
			}
		}

		/* update drm object */
		rc = drm_object_property_set_value(info->base,
				info->property_array[property_idx],
				blob ? blob->base.id : 0);
		if (rc) {
			DRM_ERROR("failed to set blob to property\n");
			if (blob)
				drm_property_unreference_blob(blob);
			goto exit;
		}

		/* update local reference */
		if (*blob_reference)
			drm_property_unreference_blob(*blob_reference);
		*blob_reference = blob;
	}

exit:
	return rc;
}

int msm_property_set_property(struct msm_property_info *info,
		uint64_t *property_values,
		uint32_t property_idx,
		uint64_t val)
{
	int rc = -EINVAL;

	if (!info || (property_idx >= info->property_count) ||
			property_idx < info->blob_count || !property_values) {
		DRM_ERROR("invalid argument(s)\n");
	} else {
		struct drm_property *drm_prop;

		mutex_lock(&info->property_lock);

		/* update cached value */
		if (property_values)
			property_values[property_idx] = val;

		/* update the new default value for immutables */
		drm_prop = info->property_array[property_idx];
		if (drm_prop->flags & DRM_MODE_PROP_IMMUTABLE)
			info->property_data[property_idx].default_value = val;

		mutex_unlock(&info->property_lock);

		/* update drm object */
		rc = drm_object_property_set_value(info->base, drm_prop, val);
		if (rc)
			DRM_ERROR("failed set property value, idx %d rc %d\n",
					property_idx, rc);

	}

	return rc;
}

