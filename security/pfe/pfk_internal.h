/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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
