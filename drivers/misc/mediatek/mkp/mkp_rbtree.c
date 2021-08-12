// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Kuan-Ying Lee <Kuan-Ying.Lee@mediatek.com>
 */

#include "mkp_rbtree.h"
#include <linux/rwlock.h>

DEFINE_RWLOCK(mkp_rbtree_rwlock);
DEBUG_SET_LEVEL(DEBUG_LEVEL_ERR);

static struct mkp_rb_node fail_node = {
	.addr = 0,
	.size = 0,
	.handle = 0,
};

void traverse_rbtree(struct rb_root *root)
{
	struct rb_node *node;
	unsigned long flags;

	read_lock_irqsave(&mkp_rbtree_rwlock, flags);
	for (node = rb_first(root); node; node=rb_next(node)) {
		struct mkp_rb_node *data = rb_entry(node, struct mkp_rb_node, rb_node);

		MKP_DEBUG("%s: addr: 0x%pa, size: %pa\n", __func__, &data->addr, &data->size);
	}
	read_unlock_irqrestore(&mkp_rbtree_rwlock, flags);
	return;
}

struct mkp_rb_node *mkp_rbtree_search(struct rb_root *root, phys_addr_t addr)
{
	struct rb_node *n;
	unsigned long flags;

	read_lock_irqsave(&mkp_rbtree_rwlock, flags);
	n = root->rb_node;
	while (n) {
		struct mkp_rb_node *cur = container_of(n, struct mkp_rb_node, rb_node);

		if (addr > cur->addr && addr < cur->addr+cur->size) {
			MKP_WARN("%s: fail node\n", __func__);
			read_unlock_irqrestore(&mkp_rbtree_rwlock, flags);
			return &fail_node;
		}
		if (addr < cur->addr)
			n = n->rb_left;
		else if (addr > cur->addr)
			n = n->rb_right;
		else {
			read_unlock_irqrestore(&mkp_rbtree_rwlock, flags);
			return cur;
		}
	}
	read_unlock_irqrestore(&mkp_rbtree_rwlock, flags);
	return NULL;
}
int mkp_rbtree_insert(struct rb_root *root, struct mkp_rb_node *ins)
{
	struct rb_node **new, *parent = NULL;
	unsigned long flags;

	write_lock_irqsave(&mkp_rbtree_rwlock, flags);
	new = &(root->rb_node);
	while (*new) {
		struct mkp_rb_node *cur = container_of(*new, struct mkp_rb_node, rb_node); // also can use rb_entry

		parent = *new;
		if (ins->addr >= cur->addr + cur->size)
			new = &((*new)->rb_right);
		else if (ins->addr + ins->size <= cur->addr &&
			ins->addr < ins->addr + ins->size)
			new = &((*new)->rb_left);
		else {
			MKP_ERR("Cannot insert node (existing overlapp)\n");
			write_unlock_irqrestore(&mkp_rbtree_rwlock, flags);
			return -EEXIST;
		}
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&ins->rb_node, parent, new);
	rb_insert_color(&ins->rb_node, root);

	write_unlock_irqrestore(&mkp_rbtree_rwlock, flags);
	return 0;
}

int mkp_rbtree_erase(struct rb_root *root, phys_addr_t addr)
{
	struct mkp_rb_node *data = NULL;
	struct rb_node *n;
	unsigned long flags;

	write_lock_irqsave(&mkp_rbtree_rwlock, flags);
	n = root->rb_node;
	while (n) {
		struct mkp_rb_node *cur = container_of(n, struct mkp_rb_node, rb_node);

		if (addr > cur->addr && addr < cur->addr+cur->size) {
			MKP_WARN("%s: fail node\n", __func__);
			break;
		}
		if (addr < cur->addr)
			n = n->rb_left;
		else if (addr > cur->addr)
			n = n->rb_right;
		else {
			data = cur;
			break;
		}
	}

	if (data) {
		rb_erase(&data->rb_node, root);
		kfree(data);
		write_unlock_irqrestore(&mkp_rbtree_rwlock, flags);
		return 0;
	}
	write_unlock_irqrestore(&mkp_rbtree_rwlock, flags);
	return -1;
}
