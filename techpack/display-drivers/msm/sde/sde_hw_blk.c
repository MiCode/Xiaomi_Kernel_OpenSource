// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2019, 2021, The Linux Foundation. All rights reserved.
 */

#define pr_fmt(fmt)	"[drm:%s:%d] " fmt, __func__, __LINE__

#include <linux/mutex.h>
#include <linux/errno.h>
#include <linux/slab.h>

#include "sde_hw_mdss.h"
#include "sde_hw_blk.h"

/* Serialization lock for sde_hw_blk_list */
static DEFINE_MUTEX(sde_hw_blk_lock);

/* List of all hw block objects */
static LIST_HEAD(sde_hw_blk_list);

/**
 * sde_hw_blk_init - initialize hw block object
 * @type: hw block type - enum sde_hw_blk_type
 * @id: instance id of the hw block
 * @ops: Pointer to block operations
 * return: 0 if success; error code otherwise
 */
int sde_hw_blk_init(struct sde_hw_blk *hw_blk, u32 type, int id,
		struct sde_hw_blk_ops *ops)
{
	if (!hw_blk) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	INIT_LIST_HEAD(&hw_blk->list);
	hw_blk->type = type;
	hw_blk->id = id;
	atomic_set(&hw_blk->refcount, 0);

	if (ops)
		hw_blk->ops = *ops;

	mutex_lock(&sde_hw_blk_lock);
	list_add(&hw_blk->list, &sde_hw_blk_list);
	mutex_unlock(&sde_hw_blk_lock);

	return 0;
}

/**
 * sde_hw_blk_destroy - destroy hw block object.
 * @hw_blk:  pointer to hw block object
 * return: none
 */
void sde_hw_blk_destroy(struct sde_hw_blk *hw_blk)
{
	if (!hw_blk) {
		pr_err("invalid parameters\n");
		return;
	}

	if (atomic_read(&hw_blk->refcount))
		pr_err("hw_blk:%d.%d invalid refcount\n", hw_blk->type,
				hw_blk->id);

	mutex_lock(&sde_hw_blk_lock);
	list_del(&hw_blk->list);
	mutex_unlock(&sde_hw_blk_lock);
}

