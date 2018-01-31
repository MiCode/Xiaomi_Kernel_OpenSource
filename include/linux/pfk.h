/* Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
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

#ifndef PFK_H_
#define PFK_H_

#include <linux/bio.h>

struct ice_crypto_setting;

#ifdef CONFIG_PFK

int pfk_load_key_start(const struct bio *bio,
		struct ice_crypto_setting *ice_setting, bool *is_pfe, bool);
int pfk_load_key_end(const struct bio *bio, bool *is_pfe);
int pfk_remove_key(const unsigned char *key, size_t key_size);
bool pfk_allow_merge_bio(const struct bio *bio1, const struct bio *bio2);
void pfk_clear_on_reset(void);

#else
static inline int pfk_load_key_start(const struct bio *bio,
	struct ice_crypto_setting *ice_setting, bool *is_pfe, bool async)
{
	return -ENODEV;
}

static inline int pfk_load_key_end(const struct bio *bio, bool *is_pfe)
{
	return -ENODEV;
}

static inline int pfk_remove_key(const unsigned char *key, size_t key_size)
{
	return -ENODEV;
}

static inline bool pfk_allow_merge_bio(const struct bio *bio1,
		const struct bio *bio2)
{
	return true;
}

static inline void pfk_clear_on_reset(void)
{}

#endif /* CONFIG_PFK */

#endif /* PFK_H */
