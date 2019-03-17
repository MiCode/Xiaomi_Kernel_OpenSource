/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
 */

#ifndef PFK_KC_H_
#define PFK_KC_H_

#include <linux/types.h>

int pfk_kc_init(void);
int pfk_kc_deinit(void);
int pfk_kc_load_key_start(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size, u32 *key_index,
		bool async, unsigned int data_unit);
void pfk_kc_load_key_end(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size);
int pfk_kc_remove_key_with_salt(const unsigned char *key, size_t key_size,
		const unsigned char *salt, size_t salt_size);
int pfk_kc_remove_key(const unsigned char *key, size_t key_size);
int pfk_kc_clear(void);
void pfk_kc_clear_on_reset(void);
const char *pfk_kc_get_storage_type(void);
extern char *saved_command_line;


#endif /* PFK_KC_H_ */
