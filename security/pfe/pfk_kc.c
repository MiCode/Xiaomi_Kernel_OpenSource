/*
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

/*
 * PFK Key Cache
 *
 * Key Cache used internally in PFK.
 * The purpose of the cache is to save access time to QSEE
 * when loading the keys.
 * Currently the cache is the same size as the total number of keys that can
 * be loaded to ICE. Since this number is relatively small, the alghoritms for
 * cache eviction are simple, linear and based on last usage timestamp, i.e
 * the node that will be evicted is the one with the oldest timestamp.
 * Empty entries always have the oldest timestamp.
 *
 */

#include <linux/mutex.h>
#include <linux/spinlock.h>
#include <crypto/ice.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/jiffies.h>
#include <linux/slab.h>
#include <linux/printk.h>

#include "pfk_kc.h"
#include "pfk_ice.h"


/** the first available index in ice engine */
#define PFK_KC_STARTING_INDEX 2

/** currently the only supported key and salt sizes */
#define PFK_KC_KEY_SIZE 32
#define PFK_KC_SALT_SIZE 32

/** Table size */
/* TODO replace by some constant from ice.h */
#define PFK_KC_TABLE_SIZE ((32) - (PFK_KC_STARTING_INDEX))

/** The maximum key and salt size */
#define PFK_MAX_KEY_SIZE PFK_KC_KEY_SIZE
#define PFK_MAX_SALT_SIZE PFK_KC_SALT_SIZE

static DEFINE_SPINLOCK(kc_lock);
static bool kc_ready;

struct kc_entry {
	 unsigned char key[PFK_MAX_KEY_SIZE];
	 size_t key_size;

	 unsigned char salt[PFK_MAX_SALT_SIZE];
	 size_t salt_size;

	 u64 time_stamp;
	 u32 key_index;
};

static struct kc_entry kc_table[PFK_KC_TABLE_SIZE] = {{{0}, 0, {0}, 0, 0, 0} };

/**
 * pfk_min_time_entry() - update min time and update min entry
 * @min_time: pointer to current min_time, might be updated with new value
 * @time: time to compare minimum with
 * @min_entry: ptr to ptr to current min_entry, might be updated with
 * ptr to new entry
 * @entry: will be the new min_entry if the time was updated
 *
 *
 * Calculates the minimum between min_time and time. Replaces the min_time
 * if time is less and replaces min_entry with entry
 *
 */
static inline void pfk_min_time_entry(u64 *min_time, u64 time,
	struct kc_entry **min_entry, struct kc_entry *entry)
{
	if (time_before64(time, *min_time)) {
		*min_time = time;
		*min_entry = entry;
	}
}

/**
 * kc_is_ready() - driver is initialized and ready.
 *
 * Return: true if the key cache is ready.
 */
static inline bool kc_is_ready(void)
{
	return kc_ready == true;
}

/**
 * kc_find_key_at_index() - find kc entry starting at specific index
 * @key: key to look for
 * @key_size: the key size
 * @salt: salt to look for
 * @salt_size: the salt size
 * @sarting_index: index to start search with, if entry found, updated with
 * index of that entry
 *
 * Return entry or NULL in case of error
 * Should be invoked under lock
 */
static struct kc_entry *kc_find_key_at_index(const unsigned char *key,
	size_t key_size, const unsigned char *salt, size_t salt_size,
	int *starting_index)
{
	struct kc_entry *entry = NULL;
	int i = 0;

	for (i = *starting_index; i < PFK_KC_TABLE_SIZE; i++) {
		entry = &(kc_table[i]);

		if (NULL != salt) {
			if (entry->salt_size != salt_size)
				continue;

			if (0 != memcmp(entry->salt, salt, salt_size))
				continue;
		}

		if (entry->key_size != key_size)
			continue;

		if (0 == memcmp(entry->key, key, key_size)) {
			*starting_index = i;
			return entry;
		}
	}

	return NULL;
}

/**
 * kc_find_key() - find kc entry
 * @key: key to look for
 * @key_size: the key size
 * @salt: salt to look for
 * @salt_size: the salt size
 *
 * Return entry or NULL in case of error
 * Should be invoked under lock
 */
static struct kc_entry *kc_find_key(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size)
{
	int index = 0;

	return kc_find_key_at_index(key, key_size, salt, salt_size, &index);
}

/**
 * kc_find_oldest_entry() - finds the entry with minimal timestamp
 *
 * Returns entry with minimal timestamp. Empty entries have timestamp
 * of 0, therefore they are returned first.
 * Should always succeed, the returned entry should never be NULL
 * Should be invoked under lock
 */
static struct kc_entry *kc_find_oldest_entry(void)
{
	struct kc_entry *curr_min_entry = NULL;
	struct kc_entry *entry = NULL;
	u64 min_time = 0;
	int i = 0;

	min_time = kc_table[0].time_stamp;
	curr_min_entry = &(kc_table[0]);
	for (i = 0; i < PFK_KC_TABLE_SIZE; i++) {
		entry = &(kc_table[i]);
		if (!entry->time_stamp)
			return entry;

		pfk_min_time_entry(&min_time, entry->time_stamp,
			&curr_min_entry, entry);
	}

	return curr_min_entry;
}

/**
 * kc_update_timestamp() - updates timestamp of entry to current
 *
 * @entry: entry to update
 *
 * If system time can't be retrieved, timestamp will not be updated
 * Should be invoked under lock
 */
static void kc_update_timestamp(struct kc_entry *entry)
{
	if (!entry)
		return;

	entry->time_stamp = get_jiffies_64();
}

/**
 * kc_clear_entry() - clear the key from entry and remove the key from ICE
 *
 * @entry: pointer to entry
 *
 * Securely wipe and release the key memory, remove the key from ICE
 * Should be invoked under lock
 */
static void kc_clear_entry(struct kc_entry *entry)
{
	if (!entry)
		return;

	memset(entry->key, 0, entry->key_size);
	memset(entry->salt, 0, entry->salt_size);

	entry->time_stamp = 0;
}

/**
 * kc_replace_entry() - replaces the key in given entry and
 *			loads the new key to ICE
 *
 * @entry: entry to replace key in
 * @key: key
 * @key_size: key_size
 * @salt: salt
 * @salt_size: salt_size
 *
 * The previous key is securely released and wiped, the new one is loaded
 * to ICE.
 * Should be invoked under lock
 */
static int kc_replace_entry(struct kc_entry *entry, const unsigned char *key,
	size_t key_size, const unsigned char *salt, size_t salt_size)
{
	int ret = 0;

	kc_clear_entry(entry);

	memcpy(entry->key, key, key_size);
	entry->key_size = key_size;

	memcpy(entry->salt, salt, salt_size);
	entry->salt_size = salt_size;

	ret = qti_pfk_ice_set_key(entry->key_index, (uint8_t *) key,
		(uint8_t *) salt);
	if (ret != 0) {
		ret = -EINVAL;
		goto err;
	}

	kc_update_timestamp(entry);

	return 0;

err:

	kc_clear_entry(entry);

	return ret;

}

/**
 * pfk_kc_init() - init function
 *
 * Return 0 in case of success, error otherwise
 */
int pfk_kc_init(void)
{
	int i = 0;

	spin_lock(&kc_lock);
	for (i = 0; i < PFK_KC_TABLE_SIZE; i++)
		kc_table[i].key_index = PFK_KC_STARTING_INDEX + i;

	spin_unlock(&kc_lock);

	kc_ready = true;

	return 0;
}


/**
 * pfk_kc_denit() - deinit function
 *
 * Return 0 in case of success, error otherwise
 */
int pfk_kc_deinit(void)
{
	pfk_kc_clear();
	kc_ready = false;

	return 0;
}

/**
 * pfk_kc_load_key() - retrieve the key from cache or add it if it's not there
 *                     return the ICE hw key index
 * @key: pointer to the key
 * @key_size: the size of the key
 * @salt: pointer to the salt
 * @salt_size: the size of the salt
 * @key_index: the pointer to key_index where the output will be stored
 *
 * If key is present in cache, than the key_index will be retrieved from cache.
 * If it is not present, the oldest entry from kc table will be evicted,
 * the key will be loaded to ICE via QSEE to the index that is the evicted
 * entry number and stored in cache
 *
 * Return 0 in case of success, error otherwise
 */
int pfk_kc_load_key(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size, u32 *key_index)
{
	int ret = 0;
	struct kc_entry *entry = NULL;

	if (!kc_is_ready())
		return -ENODEV;

	if (!key || !salt || !key_index)
		return -EPERM;

	if (key_size != PFK_KC_KEY_SIZE)
		return -EPERM;

	if (salt_size != PFK_KC_SALT_SIZE)
		return -EPERM;

	spin_lock(&kc_lock);
	entry = kc_find_key(key, key_size, salt, salt_size);
	if (!entry) {
		entry = kc_find_oldest_entry();
		if (!entry) {
			pr_err("internal error, there should always be an oldest entry\n");
			spin_unlock(&kc_lock);
			return -EINVAL;
		}

		pr_debug("didn't found key in cache, replacing entry with index %d\n",
				entry->key_index);

		ret = kc_replace_entry(entry, key, key_size, salt, salt_size);
		if (ret) {
			spin_unlock(&kc_lock);
			return -EINVAL;
		}

	} else {
		pr_debug("found key in cache, index %d\n", entry->key_index);
		kc_update_timestamp(entry);
	}

	*key_index = entry->key_index;
	spin_unlock(&kc_lock);

	return 0;
}

/**
 * pfk_kc_remove_key() - remove the key from cache and from ICE engine
 * @key: pointer to the key
 * @key_size: the size of the key
 * @salt: pointer to the key
 * @salt_size: the size of the key
 *
 * Return 0 in case of success, error otherwise (also in case of non
 * (existing key)
 */
int pfk_kc_remove_key_with_salt(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size)
{
	struct kc_entry *entry = NULL;

	if (!kc_is_ready())
		return -ENODEV;

	if (!key)
		return -EPERM;

	if (!salt)
		return -EPERM;

	if (key_size != PFK_KC_KEY_SIZE)
		return -EPERM;

	if (salt_size != PFK_KC_SALT_SIZE)
		return -EPERM;

	spin_lock(&kc_lock);
	entry = kc_find_key(key, key_size, salt, salt_size);
	if (!entry) {
		pr_err("key does not exist\n");
		spin_unlock(&kc_lock);
		return -EINVAL;
	}

	kc_clear_entry(entry);
	spin_unlock(&kc_lock);

	qti_pfk_ice_invalidate_key(entry->key_index);

	return 0;
}

/**
 * pfk_kc_remove_key() - remove the key from cache and from ICE engine
 * when no salt is available. Will only search key part, if there are several,
 * all will be removed
 *
 * @key: pointer to the key
 * @key_size: the size of the key
 *
 * Return 0 in case of success, error otherwise (also in case of non
 * (existing key)
 */
int pfk_kc_remove_key(const unsigned char *key, size_t key_size)
{
	struct kc_entry *entry = NULL;
	int index = 0;
	int temp_indexes[PFK_KC_TABLE_SIZE] = {0};
	int i = 0;

	if (!kc_is_ready())
		return -ENODEV;

	if (!key)
		return -EPERM;

	if (key_size != PFK_KC_KEY_SIZE)
		return -EPERM;

	memset(temp_indexes, -1, sizeof(temp_indexes));

	spin_lock(&kc_lock);

	entry = kc_find_key_at_index(key, key_size, NULL, 0, &index);
	if (!entry) {
		pr_debug("key does not exist\n");
		spin_unlock(&kc_lock);
		return -EINVAL;
	}

	temp_indexes[i++] = entry->key_index;
	kc_clear_entry(entry);

	/* let's clean additional entries with the same key if there are any */
	do {
		entry = kc_find_key_at_index(key, key_size, NULL, 0, &index);
		if (!entry)
			break;

		temp_indexes[i++] = entry->key_index;

		kc_clear_entry(entry);

	} while (true);

	spin_unlock(&kc_lock);

	for (i--; i >= 0 ; i--)
		qti_pfk_ice_invalidate_key(temp_indexes[i]);

	return 0;
}

/**
 * pfk_kc_clear() - clear the table and remove all keys from ICE
 *
 */
void pfk_kc_clear(void)
{
	struct kc_entry *entry = NULL;
	int i = 0;

	if (!kc_is_ready())
		return;

	spin_lock(&kc_lock);
	for (i = 0; i < PFK_KC_TABLE_SIZE; i++) {
		entry = &(kc_table[i]);
		kc_clear_entry(entry);
	}
	spin_unlock(&kc_lock);

	for (i = 0; i < PFK_KC_TABLE_SIZE; i++)
		qti_pfk_ice_invalidate_key(entry->key_index);

}

