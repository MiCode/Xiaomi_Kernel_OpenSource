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
	events_ptr->get_salt_key_size_cb = ops->get_salt_key_size_cb;

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
 * returns appropriate salt size.
 *
 * !!! crypt_stat cipher name and mode must be initialized
 */
size_t ecryptfs_get_salt_size(void *data)
{
	struct ecryptfs_crypt_stat *stat = NULL;
	unsigned char final[2*ECRYPTFS_MAX_CIPHER_NAME_SIZE+1];

	if (!data) {
		ecryptfs_printk(KERN_ERR,
				"ecryptfs_get_salt_size: invalid data parameter\n");
		return 0;
	}

	stat = (struct ecryptfs_crypt_stat *)data;
	return ecryptfs_get_salt_size_for_cipher(
			ecryptfs_get_full_cipher(stat->cipher,
						 stat->cipher_mode,
						 final, sizeof(final)));

}

/**
 * Given ecryptfs data, the function
 * returns appropriate cipher.
 */
const unsigned char *ecryptfs_get_cipher(void *data)
{
	unsigned char final[2*ECRYPTFS_MAX_CIPHER_NAME_SIZE+1];
	struct ecryptfs_crypt_stat *stat = NULL;

	if (!data) {
		ecryptfs_printk(KERN_ERR,
			"ecryptfs_get_cipher: invalid data parameter\n");
		return NULL;
	}
	stat = (struct ecryptfs_crypt_stat *)data;
	return ecryptfs_get_full_cipher(stat->cipher, stat->cipher_mode,
			final, sizeof(final));
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
 * Given ecryptfs data, the function
 * returns file encryption salt.
 */
const unsigned char *ecryptfs_get_salt(void *data)
{
	struct ecryptfs_crypt_stat *stat = NULL;

	if (!data) {
		ecryptfs_printk(KERN_ERR,
			"ecryptfs_get_salt: invalid data parameter\n");
		return NULL;
	}
	stat = (struct ecryptfs_crypt_stat *)data;
	return stat->key + ecryptfs_get_salt_size(data);
}

/**
 * Returns ecryptfs events pointer
 */
inline struct ecryptfs_events *get_events(void)
{
	return events_ptr;
}

/**
 * If external crypto module requires salt in addition to key,
 * we store it as part of key array (if there is enough space)
 * Checks whether a salt key can fit into array allocated for
 * regular key
 */
bool ecryptfs_check_space_for_salt(const size_t key_size,
		const size_t salt_size)
{
	if ((salt_size + key_size) > ECRYPTFS_MAX_KEY_BYTES)
		return false;

	return true;
}

/*
 * If there is salt that is used by external crypto module, it is stored
 * in the same array where regular key is. Salt is going to be used by
 * external crypto module only, so for all internal crypto operations salt
 * should be ignored.
 *
 * Get key size in cases where it is going to be used for data encryption
 * or for all other general purposes
 */
size_t ecryptfs_get_key_size_to_enc_data(
		struct ecryptfs_crypt_stat *crypt_stat)
{
	if (!crypt_stat)
		return 0;

	return crypt_stat->key_size;
}

/*
 * If there is salt that is used by external crypto module, it is stored
 * in the same array where regular key is. Salt is going to be used by
 * external crypto module only, but we still need to save and restore it
 * (in encrypted form) as part of ecryptfs header along with the regular
 * key.
 *
 * Get key size in cases where it is going to be stored persistently
 *
 * !!! crypt_stat cipher name and mode must be initialized
 */
size_t ecryptfs_get_key_size_to_store_key(
		struct ecryptfs_crypt_stat *crypt_stat)
{
	size_t salt_size = 0;

	if (!crypt_stat)
		return 0;

	salt_size = ecryptfs_get_salt_size(crypt_stat);

	if (!ecryptfs_check_space_for_salt(crypt_stat->key_size, salt_size)) {
		ecryptfs_printk(KERN_WARNING,
			"ecryptfs_get_key_size_to_store_key: not enough space for salt\n");
		return crypt_stat->key_size;
	}

	return crypt_stat->key_size + salt_size;
}

/*
 * If there is salt that is used by external crypto module, it is stored
 * in the same array where regular key is. Salt is going to be used by
 * external crypto module only, but we still need to save and restore it
 * (in encrypted form) as part of ecryptfs header along with the regular
 * key.
 *
 * Get key size in cases where it is going to be restored from storage
 *
 * !!! crypt_stat cipher name and mode must be initialized
 */
size_t ecryptfs_get_key_size_to_restore_key(size_t stored_key_size,
		const char *cipher)
{
	size_t salt_size = 0;

	if (!cipher)
		return 0;

	salt_size = ecryptfs_get_salt_size_for_cipher(cipher);

	if (salt_size >= stored_key_size) {
		ecryptfs_printk(KERN_WARNING,
			"ecryptfs_get_key_size_to_restore_key: salt %zu >= stred size %zu\n",
			salt_size, stored_key_size);

		return stored_key_size;
	}

	return stored_key_size - salt_size;
}

/**
 * Given cipher, the function returns appropriate salt size.
 */
size_t ecryptfs_get_salt_size_for_cipher(const char *cipher)
{
	if (!get_events() || !(get_events()->get_salt_key_size_cb))
		return 0;

	return get_events()->get_salt_key_size_cb(cipher);
}
