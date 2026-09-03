/* SPDX-License-Identifier: GPL-2.0-only */
/* Copyright (C) 2019 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef _SIPA_RM_DEP_GRAPH_H_
#define _SIPA_RM_DEP_GRAPH_H_

#include <linux/list.h>
#include <linux/sipa.h>
#include "sipa_rm_res.h"

struct sipa_rm_dep_graph {
	struct sipa_rm_resource *resource_table[SIPA_RM_RES_MAX];
};

int sipa_rm_dep_graph_get_resource(struct sipa_rm_dep_graph *graph,
				   enum sipa_rm_res_id name,
				   struct sipa_rm_resource **resource);

int sipa_rm_dep_graph_create(struct sipa_rm_dep_graph **dep_graph);

void sipa_rm_dep_graph_delete(struct sipa_rm_dep_graph *graph);

int sipa_rm_dep_graph_add(struct sipa_rm_dep_graph *graph,
			  struct sipa_rm_resource *resource);

int sipa_rm_dep_graph_remove(struct sipa_rm_dep_graph *graph,
			     enum sipa_rm_res_id resource_name);

int sipa_rm_dep_graph_add_dependency(struct sipa_rm_dep_graph *graph,
				     enum sipa_rm_res_id resource_name,
				     enum sipa_rm_res_id depends_on_name,
				     bool userspsace_dep);

int sipa_rm_dep_graph_delete_dependency(struct sipa_rm_dep_graph *graph,
					enum sipa_rm_res_id resource_name,
					enum sipa_rm_res_id depends_on_name,
					bool userspsace_dep);

#endif /* _SIPA_RM_DEP_GRAPH_H_ */
