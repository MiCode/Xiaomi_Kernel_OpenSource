/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2017-2019, 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _SDE_HW_BLK_H
#define _SDE_HW_BLK_H

#include <linux/types.h>
#include <linux/list.h>
#include <linux/atomic.h>

struct sde_hw_blk;

/**
 * struct sde_hw_blk_ops - common hardware block operations
 * @start: start operation on first get
 * @stop: stop operation on last put
 */
struct sde_hw_blk_ops {
	int (*start)(struct sde_hw_blk *hw_blk);
	void (*stop)(struct sde_hw_blk *hw_blk);
};

/**
 * struct sde_hw_blk - definition of hardware block object
 * @list: list of hardware blocks
 * @type: hardware block type
 * @id: instance id
 * @refcount: reference/usage count
 */
struct sde_hw_blk {
	struct list_head list;
	u32 type;
	int id;
	atomic_t refcount;
	struct sde_hw_blk_ops ops;
};

int sde_hw_blk_init(struct sde_hw_blk *hw_blk, u32 type, int id,
		struct sde_hw_blk_ops *ops);
void sde_hw_blk_destroy(struct sde_hw_blk *hw_blk);

#endif /*_SDE_HW_BLK_H */
