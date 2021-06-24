// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2021 MediaTek Inc.
 * Author: Kuan-Ying Lee <Kuan-Ying.Lee@mediatek.com>
 */

#include "mkp_rbtree.h"

static struct mkp_rb_node fail_node = {
	.addr = 0,
	.size = 0,
	.handle = 0,
};

#ifdef DEBUG_MKP
void traverse_rbtree(struct rb_root *root)
{
	struct rb_node *node;

	for (node = rb_first(root); node; node=rb_next(node)) {
		struct mkp_rb_node *data = rb_entry(node, struct mkp_rb_node, rb_node);

		pr_info("%s: addr: 0x%pa, size: %pa\n", __func__, &data->addr, &data->size);
	}
	return;
}
#endif

struct mkp_rb_node *mkp_rbtree_search(struct rb_root *root, phys_addr_t addr)
{
	struct rb_node *n = root->rb_node;

	while (n) {
		struct mkp_rb_node *cur = container_of(n, struct mkp_rb_node, rb_node);

		if (addr > cur->addr && addr < cur->addr+cur->size) {
//			pr_info("%s: fail node\n", __func__);
			return &fail_node;
		}
		if (addr < cur->addr)
			n = n->rb_left;
		else if (addr > cur->addr)
			n = n->rb_right;
		else
			return cur;
	}
	return NULL;
}
int mkp_rbtree_insert(struct rb_root *root, struct mkp_rb_node *ins)
{
	struct rb_node **new = &(root->rb_node), *parent = NULL;
	while (*new) {
		struct mkp_rb_node *cur = container_of(*new, struct mkp_rb_node, rb_node); // also can use rb_entry

		parent = *new;
		if (cur->addr < ins->addr)
			new = &((*new)->rb_right);
		else if (cur->addr > ins->addr)
			new = &((*new)->rb_left);
		else
			return -EEXIST;
	}

	/* Add new node and rebalance tree. */
	rb_link_node(&ins->rb_node, parent, new);
	rb_insert_color(&ins->rb_node, root);

	return 0;
}
