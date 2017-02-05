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
 * return: 0 if success; error code otherwise
 */
int sde_hw_blk_init(struct sde_hw_blk *hw_blk, u32 type, int id)
{
	if (!hw_blk) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	INIT_LIST_HEAD(&hw_blk->list);
	hw_blk->type = type;
	hw_blk->id = id;
	atomic_set(&hw_blk->refcount, 0);
	INIT_LIST_HEAD(&hw_blk->attach_list);

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
	struct sde_hw_blk_attachment *curr, *next;

	if (!hw_blk) {
		pr_err("invalid parameters\n");
		return;
	}

	if (atomic_read(&hw_blk->refcount))
		pr_err("hw_blk:%d.%d invalid refcount\n", hw_blk->type,
				hw_blk->id);

	list_for_each_entry_safe(curr, next, &hw_blk->attach_list, list) {
		pr_err("hw_blk:%d.%d tag:0x%x/0x%llx still attached\n",
				hw_blk->type, hw_blk->id,
				curr->tag, (u64) curr->value);
		list_del_init(&curr->list);
		kfree(curr);
	}

	mutex_lock(&sde_hw_blk_lock);
	list_del(&hw_blk->list);
	mutex_unlock(&sde_hw_blk_lock);
}

/**
 * sde_hw_blk_get - get hw_blk from free pool
 * @hw_blk: if specified, increment reference count only
 * @type: if hw_blk is not specified, allocate the next available of this type
 * @id: if specified (>= 0), allocate the given instance of the above type
 * return: pointer to hw block object
 */
struct sde_hw_blk *sde_hw_blk_get(struct sde_hw_blk *hw_blk, u32 type, int id)
{
	struct sde_hw_blk *curr;

	if (!hw_blk) {
		mutex_lock(&sde_hw_blk_lock);
		list_for_each_entry(curr, &sde_hw_blk_list, list) {
			if ((curr->type != type) ||
					(id >= 0 && curr->id != id) ||
					(id < 0 &&
						atomic_read(&curr->refcount)))
				continue;

			hw_blk = curr;
			break;
		}
		mutex_unlock(&sde_hw_blk_lock);
	}

	if (hw_blk) {
		int refcount = atomic_inc_return(&hw_blk->refcount);

		pr_debug("hw_blk:%d.%d refcount:%d\n", hw_blk->type,
				hw_blk->id, refcount);
	} else {
		pr_err("no hw_blk:%d\n", type);
	}

	return hw_blk;
}

/**
 * sde_hw_blk_put - put hw_blk to free pool if decremented refcount is zero
 * @hw_blk: hw block to be freed
 * @free_blk: function to be called when reference count goes to zero
 */
void sde_hw_blk_put(struct sde_hw_blk *hw_blk,
		void (*free_blk)(struct sde_hw_blk *))
{
	struct sde_hw_blk_attachment *curr, *next;

	if (!hw_blk) {
		pr_err("invalid parameters\n");
		return;
	}

	pr_debug("hw_blk:%d.%d refcount:%d\n", hw_blk->type, hw_blk->id,
			atomic_read(&hw_blk->refcount));

	if (!atomic_read(&hw_blk->refcount)) {
		pr_err("hw_blk:%d.%d invalid put\n", hw_blk->type, hw_blk->id);
		return;
	}

	if (atomic_dec_return(&hw_blk->refcount))
		return;

	if (free_blk)
		free_blk(hw_blk);

	/* report any residual attachments */
	list_for_each_entry_safe(curr, next, &hw_blk->attach_list, list) {
		pr_err("hw_blk:%d.%d tag:0x%x/0x%llx still attached\n",
				hw_blk->type, hw_blk->id,
				curr->tag, (u64) curr->value);
		list_del_init(&curr->list);
		kfree(curr);
	}
}

/**
 * sde_hw_blk_lookup_blk - lookup hardware block that matches tag/value/type
 *	tuple and increment reference count
 * @tag: search tag
 * @value: value associated with search tag
 * @type: hardware block type
 * return: Pointer to hardware block
 */
struct sde_hw_blk *sde_hw_blk_lookup_blk(u32 tag, void *value, u32 type)
{
	struct sde_hw_blk *hw_blk = NULL, *curr;
	struct sde_hw_blk_attachment *attach;

	pr_debug("hw_blk:%d tag:0x%x/0x%llx\n", type, tag, (u64) value);

	mutex_lock(&sde_hw_blk_lock);
	list_for_each_entry(curr, &sde_hw_blk_list, list) {
		if ((curr->type != type) || !atomic_read(&curr->refcount))
			continue;

		list_for_each_entry(attach, &curr->attach_list, list) {
			if ((attach->tag != tag) || (attach->value != value))
				continue;

			hw_blk = curr;
			break;
		}

		if (hw_blk)
			break;
	}
	mutex_unlock(&sde_hw_blk_lock);

	if (hw_blk)
		sde_hw_blk_get(hw_blk, 0, -1);

	return hw_blk;
}

/**
 * sde_hw_blk_attach - attach given tag/value pair to hardware block
 *	and increment reference count
 * @hw_blk: Pointer hardware block
 * @tag: search tag
 * @value: value associated with search tag
 * return: 0 if success; error code otherwise
 */
int sde_hw_blk_attach(struct sde_hw_blk *hw_blk, u32 tag, void *value)
{
	struct sde_hw_blk_attachment *attach;

	if (!hw_blk) {
		pr_err("invalid parameters\n");
		return -EINVAL;
	}

	pr_debug("hw_blk:%d.%d tag:0x%x/0x%llx\n", hw_blk->type, hw_blk->id,
			tag, (u64) value);

	attach = kzalloc(sizeof(struct sde_hw_blk_attachment), GFP_KERNEL);
	if (!attach)
		return -ENOMEM;

	INIT_LIST_HEAD(&attach->list);
	attach->tag = tag;
	attach->value = value;
	/* always add to the front so latest shows up first in search */
	list_add(&attach->list, &hw_blk->attach_list);
	sde_hw_blk_get(hw_blk, 0, -1);

	return 0;
}

/**
 * sde_hw_blk_detach - detach given tag/value pair from hardware block
 *	and decrement reference count
 * @hw_blk: Pointer hardware block
 * @tag: search tag
 * @value: value associated with search tag
 * return: none
 */
void sde_hw_blk_detach(struct sde_hw_blk *hw_blk, u32 tag, void *value)
{
	struct sde_hw_blk_attachment *curr, *next;

	if (!hw_blk) {
		pr_err("invalid parameters\n");
		return;
	}

	pr_debug("hw_blk:%d.%d tag:0x%x/0x%llx\n", hw_blk->type, hw_blk->id,
			tag, (u64) value);

	list_for_each_entry_safe(curr, next, &hw_blk->attach_list, list) {
		if ((curr->tag != tag) || (curr->value != value))
			continue;

		list_del_init(&curr->list);
		kfree(curr);
		sde_hw_blk_put(hw_blk, NULL);
		return;
	}

	pr_err("hw_blk:%d.%d tag:0x%x/0x%llx not found\n", hw_blk->type,
			hw_blk->id, tag, (u64) value);
}
