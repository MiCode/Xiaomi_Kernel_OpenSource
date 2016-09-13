/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
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

#ifndef _PFK_INTERNAL_H_
#define _PFK_INTERNAL_H_

#include <linux/types.h>
#include <crypto/ice.h>

struct pfk_key_info {
	const unsigned char *key;
	const unsigned char *salt;
	size_t key_size;
	size_t salt_size;
};

int pfk_key_size_to_key_type(size_t key_size,
	enum ice_crpto_key_size *key_size_type);

bool pfe_is_inode_filesystem_type(const struct inode *inode,
	const char *fs_type);

char *inode_to_filename(const struct inode *inode);

#endif /* _PFK_INTERNAL_H_ */
