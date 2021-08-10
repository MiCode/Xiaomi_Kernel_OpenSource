// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2020, The Linux Foundation. All rights reserved.
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
	}
}

void msm_property_destroy(struct msm_property_info *info)
{
	if (!info)
		return;

	/* free state cache */
	while (info->state_cache_size > 0)
		kfree(info->state_cache[--(info->state_cache_size)]);

	mutex_destroy(&info->property_lock);
}

int msm_property_pop_dirty(struct msm_property_info *info,
		struct msm_property_state *property_state)
{
	struct list_head *item;
	int rc = 0;

	if (!info || !property_state || !property_state->values) {
		DRM_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}

	WARN_ON(!mutex_is_locked(&info->property_lock));

	if (list_empty(&property_state->dirty_list)) {
		rc = -EAGAIN;
	} else {
		item = property_state->dirty_list.next;
		list_del_init(item);
		rc = container_of(item, struct msm_property_value, dirty_node)
			- property_state->values;
		DRM_DEBUG_KMS("property %d dirty\n", rc);
	}

	return rc;
}

/**
 * _msm_property_set_dirty_no_lock - flag given property as being dirty
 *                                   This function doesn't mutex protect the
 *                                   dirty linked list.
 * @info: Pointer to property info container struct
 * @property_state: Pointer to property state container struct
 * @property_idx: Property index
 */
static void _msm_property_set_dirty_no_lock(
		struct msm_property_info *info,
		struct msm_property_state *property_state,
		uint32_t property_idx)
{
	if (!info || !property_state || !property_state->values ||
			property_idx >= info->property_count) {
		DRM_ERROR("invalid argument(s), idx %u\n", property_idx);
		return;
	}

	/* avoid re-inserting if already dirty */
	if (!list_empty(&property_state->values[property_idx].dirty_node)) {
		DRM_DEBUG_KMS("property %u already dirty\n", property_idx);
		return;
	}

	list_add_tail(&property_state->values[property_idx].dirty_node,
			&property_state->dirty_list);
}

bool msm_property_is_dirty(
		struct msm_property_info *info,
		struct msm_property_state *property_state,
		uint32_t property_idx)
{
	if (!info || !property_state || !property_state->values ||
			property_idx >= info->property_count) {
		DRM_ERROR("invalid argument(s), idx %u\n", property_idx);
		return false;
	}

	return !list_empty(&property_state->values[property_idx].dirty_node);
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
		if (!*prop) {
			*prop = drm_property_create_range(info->dev,
					flags, name, min, max);
			if (!*prop)
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

void msm_property_install_enum(struct msm_property_info *info,
		const char *name, int flags, int is_bitmask,
		const struct drm_prop_enum_list *values, int num_values,
		u32 init_idx, uint32_t property_idx)
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
		if (!*prop) {
			/* 'bitmask' is a special type of 'enum' */
			if (is_bitmask)
				*prop = drm_property_create_bitmask(info->dev,
						DRM_MODE_PROP_BITMASK | flags,
						name, values, num_values, -1);
			else
				*prop = drm_property_create_enum(info->dev,
						DRM_MODE_PROP_ENUM | flags,
						name, values, num_values);
			if (!*prop)
				DRM_ERROR("create %s property failed\n", name);
		}

		/* save init value for later */
		info->property_data[property_idx].default_value = 0;
		info->property_data[property_idx].force_dirty = false;

		/* initialize with the given idx if valid */
		if (!is_bitmask && init_idx && (init_idx < num_values))
			info->property_data[property_idx].default_value =
				values[init_idx].type;

		/* always attach property, if created */
		if (*prop) {
			drm_object_attach_property(info->base, *prop,
					info->property_data
					[property_idx].default_value);
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
		if (!*prop) {
			/* use 'create' for blob property place holder */
			*prop = drm_property_create(info->dev,
					DRM_MODE_PROP_BLOB | flags, name, 0);
			if (!*prop)
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

int msm_property_set_dirty(struct msm_property_info *info,
		struct msm_property_state *property_state,
		int property_idx)
{
	if (!info || !property_state || !property_state->values) {
		DRM_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}
	mutex_lock(&info->property_lock);
	_msm_property_set_dirty_no_lock(info, property_state, property_idx);
	mutex_unlock(&info->property_lock);
	return 0;
}

int msm_property_atomic_set(struct msm_property_info *info,
		struct msm_property_state *property_state,
		struct drm_property *property, uint64_t val)
{
	struct drm_property_blob *blob;
	int property_idx, rc = -EINVAL;

	if (!info || !property_state) {
		DRM_ERROR("invalid argument(s)\n");
		return -EINVAL;
	}

	property_idx = msm_property_index(info, property);
	if ((property_idx == -EINVAL) || !property_state->values) {
		DRM_ERROR("invalid argument(s)\n");
	} else {
		/* extra handling for incoming properties */
		mutex_lock(&info->property_lock);
		if ((property->flags & DRM_MODE_PROP_BLOB) &&
			(property_idx < info->blob_count)) {

			/* need to clear previous ref */
			if (property_state->values[property_idx].blob)
				drm_property_blob_put(
					property_state->values[
						property_idx].blob);

			/* DRM lookup also takes a reference */
			blob = drm_property_lookup_blob(info->dev,
				(uint32_t)val);
			if (val && !blob) {
				DRM_ERROR("prop %d blob id 0x%llx not found\n",
						property_idx, val);
				val = 0;
			} else {
				if (blob) {
					DBG("Blob %u saved", blob->base.id);
					val = blob->base.id;
				}

				/* save the new blob */
				property_state->values[property_idx].blob =
					blob;
			}
		}

		/* update value and flag as dirty */
		if (property_state->values[property_idx].value != val ||
				info->property_data[property_idx].force_dirty) {
			property_state->values[property_idx].value = val;
			_msm_property_set_dirty_no_lock(info, property_state,
					property_idx);

			DBG("%s - %lld", property->name, val);
		}
		mutex_unlock(&info->property_lock);
		rc = 0;
	}

	return rc;
}

int msm_property_atomic_get(struct msm_property_info *info,
		struct msm_property_state *property_state,
		struct drm_property *property, uint64_t *val)
{
	int property_idx, rc = -EINVAL;

	property_idx = msm_property_index(info, property);
	if (!info || (property_idx == -EINVAL) ||
			!property_state->values || !val) {
		DRM_DEBUG("Invalid argument(s)\n");
	} else {
		mutex_lock(&info->property_lock);
		*val = property_state->values[property_idx].value;
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
		struct msm_property_state *property_state,
		struct msm_property_value *property_values)
{
	uint32_t i;

	if (!info) {
		DRM_ERROR("invalid property info\n");
		return;
	}

	if (state)
		memset(state, 0, info->state_size);

	if (property_state) {
		property_state->property_count = info->property_count;
		property_state->values = property_values;
		INIT_LIST_HEAD(&property_state->dirty_list);
	}

	/*
	 * Assign default property values. This helper is mostly used
	 * to initialize newly created state objects.
	 */
	if (property_values)
		for (i = 0; i < info->property_count; ++i) {
			property_values[i].value =
				info->property_data[i].default_value;
			property_values[i].blob = NULL;
			INIT_LIST_HEAD(&property_values[i].dirty_node);
		}
}

void msm_property_duplicate_state(struct msm_property_info *info,
		void *old_state, void *state,
		struct msm_property_state *property_state,
		struct msm_property_value *property_values)
{
	uint32_t i;

	if (!info || !old_state || !state) {
		DRM_ERROR("invalid argument(s)\n");
		return;
	}

	memcpy(state, old_state, info->state_size);

	if (!property_state)
		return;

	INIT_LIST_HEAD(&property_state->dirty_list);
	property_state->values = property_values;

	if (property_state->values)
		/* add ref count for blobs and initialize dirty nodes */
		for (i = 0; i < info->property_count; ++i) {
			if (property_state->values[i].blob)
				drm_property_blob_get(
						property_state->values[i].blob);
			INIT_LIST_HEAD(&property_state->values[i].dirty_node);
		}
}

void msm_property_destroy_state(struct msm_property_info *info, void *state,
		struct msm_property_state *property_state)
{
	uint32_t i;

	if (!info || !state) {
		DRM_ERROR("invalid argument(s)\n");
		return;
	}
	if (property_state && property_state->values) {
		/* remove ref count for blobs */
		for (i = 0; i < info->property_count; ++i)
			if (property_state->values[i].blob) {
				drm_property_blob_put(
						property_state->values[i].blob);
				property_state->values[i].blob = NULL;
			}
	}

	_msm_property_free_state(info, state);
}

void *msm_property_get_blob(struct msm_property_info *info,
		struct msm_property_state *property_state,
		size_t *byte_len,
		uint32_t property_idx)
{
	struct drm_property_blob *blob;
	size_t len = 0;
	void *rc = 0;

	if (!info || !property_state || !property_state->values ||
			(property_idx >= info->blob_count)) {
		DRM_ERROR("invalid argument(s)\n");
	} else {
		blob = property_state->values[property_idx].blob;
		if (blob) {
			len = blob->length;
			rc = blob->data;
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
				drm_property_blob_put(blob);
			goto exit;
		}

		/* update local reference */
		if (*blob_reference)
			drm_property_blob_put(*blob_reference);
		*blob_reference = blob;
	}

exit:
	return rc;
}

int msm_property_set_property(struct msm_property_info *info,
		struct msm_property_state *property_state,
		uint32_t property_idx,
		uint64_t val)
{
	int rc = -EINVAL;

	if (!info || (property_idx >= info->property_count) ||
			property_idx < info->blob_count ||
			!property_state || !property_state->values) {
		DRM_ERROR("invalid argument(s)\n");
	} else {
		struct drm_property *drm_prop;

		mutex_lock(&info->property_lock);

		/* update cached value */
		property_state->values[property_idx].value = val;

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

