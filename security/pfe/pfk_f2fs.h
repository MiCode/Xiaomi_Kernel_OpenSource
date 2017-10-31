/* Copyright (c) 2015-2016, The Linux Foundation. All rights reserved.
 * Copyright (C) 2017 XiaoMi, Inc.
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

#ifndef _PFK_F2FS_H_
#define _PFK_F2FS_H_

#include <linux/types.h>
#include <linux/fs.h>
#include <crypto/ice.h>
#include "pfk_internal.h"

bool pfk_is_f2fs_type(const struct inode *inode);

int pfk_f2fs_parse_inode(const struct bio *bio,
	const struct inode *inode,
	struct pfk_key_info *key_info,
	enum ice_cryto_algo_mode *algo,
	bool *is_pfe);

bool pfk_f2fs_allow_merge_bio(const struct bio *bio1,
	const struct bio *bio2, const struct inode *inode1,
	const struct inode *inode2);

int __init pfk_f2fs_init(void);

void __exit pfk_f2fs_deinit(void);

#endif /* _PFK_F2FS_H_ */
