/**
 * eCryptfs: Linux filesystem encryption layer
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
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

#include <linux/string.h>
#include <linux/ecryptfs.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/pagemap.h>
#include <linux/random.h>
#include "ecryptfs_kernel.h"

static DEFINE_MUTEX(events_mutex);
struct ecryptfs_events *events_ptr = NULL;
static int handle;

void ecryptfs_free_events(void)
{
	mutex_lock(&events_mutex);
	if (events_ptr != NULL) {
		kfree(events_ptr);
		events_ptr = NULL;
	}

	mutex_unlock(&events_mutex);
}

/**
 * Register to ecryptfs events, by passing callback
 * functions to be called upon events occurence.
 * The function returns a handle to be passed
 * to unregister function.
 */
int ecryptfs_register_to_events(struct ecryptfs_events *ops)
{
	int ret_value = 0;

	if (!ops)
		return -EINVAL;

	mutex_lock(&events_mutex);

	if (events_ptr != NULL) {
		ecryptfs_printk(KERN_ERR,
			"already registered!\n");
		ret_value = -EPERM;
		goto out;
	}
	events_ptr =
		kzalloc(sizeof(struct ecryptfs_events), GFP_KERNEL);

	if (!events_ptr) {
		ecryptfs_printk(KERN_ERR, "malloc failure\n");
		ret_value = -ENOMEM;
		goto out;
	}
	/* copy the callbacks */
	events_ptr->open_cb = ops->open_cb;
	events_ptr->release_cb = ops->release_cb;
	events_ptr->encrypt_cb = ops->encrypt_cb;
	events_ptr->decrypt_cb = ops->decrypt_cb;
	events_ptr->is_cipher_supported_cb =
		ops->is_cipher_supported_cb;
	events_ptr->is_hw_crypt_cb = ops->is_hw_crypt_cb;

	get_random_bytes(&handle, sizeof(handle));
	ret_value = handle;

out:
	mutex_unlock(&events_mutex);
	return ret_value;
}

/**
 * Unregister from ecryptfs events.
 */
int ecryptfs_unregister_from_events(int user_handle)
{
	int ret_value = 0;

	mutex_lock(&events_mutex);

	if (!events_ptr) {
		ret_value = -EINVAL;
		goto out;
	}
	if (user_handle != handle) {
		ret_value = ECRYPTFS_INVALID_EVENTS_HANDLE;
		goto out;
	}

	kfree(events_ptr);
	events_ptr = NULL;

out:
	mutex_unlock(&events_mutex);
	return ret_value;
}

/**
 * This function decides whether the passed file offset
 * belongs to ecryptfs metadata or not.
 * The caller must pass ecryptfs data, which was received in one
 * of the callback invocations.
 */
bool ecryptfs_is_page_in_metadata(void *data, pgoff_t offset)
{

	struct ecryptfs_crypt_stat *stat = NULL;
	bool ret = true;

	if (!data) {
		ecryptfs_printk(KERN_ERR, "ecryptfs_is_page_in_metadata: invalid data parameter\n");
		ret = false;
		goto end;
	}
	stat = (struct ecryptfs_crypt_stat *)data;

	if (stat->flags & ECRYPTFS_METADATA_IN_XATTR) {
		ret = false;
		goto end;
	}

	if (offset >= (stat->metadata_size/PAGE_CACHE_SIZE)) {
		ret = false;
		goto end;
	}
end:
	return ret;
}

/**
 * Given two ecryptfs data, the function
 * decides whether they are equal.
 */
inline bool ecryptfs_is_data_equal(void *data1, void *data2)
{
	/* pointer comparison*/
	return data1 == data2;
}

/**
 * Given ecryptfs data, the function
 * returns appropriate key size.
 */
size_t ecryptfs_get_key_size(void *data)
{

	struct ecryptfs_crypt_stat *stat = NULL;

	if (!data)
		return 0;

	stat = (struct ecryptfs_crypt_stat *)data;
	return stat->key_size;
}

/**
 * Given ecryptfs data, the function
 * returns appropriate cipher.
 */
const unsigned char *ecryptfs_get_cipher(void *data)
{

	struct ecryptfs_crypt_stat *stat = NULL;

	if (!data) {
		ecryptfs_printk(KERN_ERR,
			"ecryptfs_get_cipher: invalid data parameter\n");
		return NULL;
	}
	stat = (struct ecryptfs_crypt_stat *)data;
	return ecryptfs_get_full_cipher(stat->cipher, stat->cipher_mode);
}

/**
 * Given ecryptfs data, the function
 * returns file encryption key.
 */
const unsigned char *ecryptfs_get_key(void *data)
{

	struct ecryptfs_crypt_stat *stat = NULL;

	if (!data) {
		ecryptfs_printk(KERN_ERR,
			"ecryptfs_get_key: invalid data parameter\n");
		return NULL;
	}
	stat = (struct ecryptfs_crypt_stat *)data;
	return stat->key;
}

/**
 * Returns ecryptfs events pointer
 */
inline struct ecryptfs_events *get_events(void)
{
	return events_ptr;
}

