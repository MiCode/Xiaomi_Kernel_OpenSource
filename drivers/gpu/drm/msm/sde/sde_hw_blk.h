/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#ifndef _SDE_HW_BLK_H
#define _SDE_HW_BLK_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/atomic.h>

/**
 * struct sde_hw_blk_attachment - hardware block attachment
 * @list: list of attachment
 * @tag: search tag
 * @value: value associated with the given tag
 */
struct sde_hw_blk_attachment {
	struct list_head list;
	u32 tag;
	void *value;
};

/**
 * struct sde_hw_blk - definition of hardware block object
 * @list: list of hardware blocks
 * @type: hardware block type
 * @id: instance id
 * @refcount: reference/usage count
 * @attachment_list: list of attachment
 */
struct sde_hw_blk {
	struct list_head list;
	u32 type;
	int id;
	atomic_t refcount;
	struct list_head attach_list;
};

int sde_hw_blk_init(struct sde_hw_blk *hw_blk, u32 type, int id);
void sde_hw_blk_destroy(struct sde_hw_blk *hw_blk);

struct sde_hw_blk *sde_hw_blk_get(struct sde_hw_blk *hw_blk, u32 type, int id);
void sde_hw_blk_put(struct sde_hw_blk *hw_blk,
		void (*blk_free)(struct sde_hw_blk *));

struct sde_hw_blk *sde_hw_blk_lookup_blk(u32 tag, void *value, u32 type);
int sde_hw_blk_attach(struct sde_hw_blk *hw_blk, u32 tag, void *value);
void sde_hw_blk_detach(struct sde_hw_blk *hw_blk, u32 tag, void *value);

/**
 * sde_hw_blk_lookup_value - return value associated with the given tag
 * @hw_blk: Pointer to hardware block
 * @tag: tag to find
 * @idx: index if more than one value found, with 0 being first
 * return: value associated with the given tag
 */
static inline void *sde_hw_blk_lookup_value(struct sde_hw_blk *hw_blk,
		u32 tag, u32 idx)
{
	struct sde_hw_blk_attachment *attach;

	if (!hw_blk)
		return NULL;

	list_for_each_entry(attach, &hw_blk->attach_list, list) {
		if (attach->tag != tag)
			continue;

		if (idx == 0)
			return attach->value;

		idx--;
	}

	return NULL;
}

#endif /*_SDE_HW_BLK_H */
