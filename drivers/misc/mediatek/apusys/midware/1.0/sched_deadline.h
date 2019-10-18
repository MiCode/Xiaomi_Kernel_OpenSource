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

#ifndef MIDWARE_1_0_SCHED_DEADLINE_H_
#define MIDWARE_1_0_SCHED_DEADLINE_H_
#include "cmd_parser.h"
#include "cmd_format.h"

struct deadline_root {
	struct rb_root_cached root;
	uint64_t total_runtime;
	uint64_t total_period;
};

void deadline_node_init(struct deadline_root *root);
void deadline_node_insert(struct deadline_root *root,
		struct apusys_subcmd *node);
bool deadline_node_empty(struct deadline_root *root);
struct apusys_subcmd *deadline_node_pop_first(struct deadline_root *root);


#endif /* MIDWARE_1_0_SCHED_DEADLINE_H_ */
