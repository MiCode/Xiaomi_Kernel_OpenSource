/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/rbtree.h>
#include <apusys_device.h>
#include "sched_deadline.h"

void deadline_node_init(struct deadline_root *root)
{
	root->root = RB_ROOT_CACHED;
}

void deadline_node_insert(struct deadline_root *root,
		struct apusys_subcmd *node)
{
	struct rb_node **link = &root->root.rb_root.rb_node;
	struct rb_node *parent = NULL;
	struct apusys_subcmd *entry;
	bool leftmost = true;

	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct apusys_subcmd, node);
		if (entry->deadline > node->deadline)
			link = &parent->rb_left;
		else {
			link = &parent->rb_right;
			leftmost = false;
		}
	}
	rb_link_node(&node->node, parent, link);
	rb_insert_color_cached(&node->node, &root->root, leftmost);
}

bool deadline_node_empty(struct deadline_root *root)
{
	if (rb_first_cached(&root->root))
		return false;
	else
		return true;

}

struct apusys_subcmd *deadline_node_pop_first(struct deadline_root *root)
{
	struct rb_node *node;
	struct apusys_subcmd *entry;

	node = rb_first_cached(&root->root);
	if (node) {
		entry = rb_entry(node, struct apusys_subcmd, node);
		rb_erase_cached(node, &root->root);
		return entry;
	} else
		return NULL;

}

