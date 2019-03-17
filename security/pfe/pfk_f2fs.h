/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2015-2019, The Linux Foundation. All rights reserved.
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

void pfk_f2fs_deinit(void);

#endif /* _PFK_F2FS_H_ */
