// SPDX-License-Identifier: GPL-2.0-only
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

#ifdef pr_fmt
#undef pr_fmt
#endif
#define pr_fmt(fmt) "sipa_rm: " fmt

#include <linux/slab.h>

#include "sipa_rm.h"
#include "sipa_rm_dep_graph.h"

static int sipa_rm_dep_get_index(enum sipa_rm_res_id resource_name)
{
	int resource_index = SIPA_RM_INDEX_INVALID;

	if (SIPA_RM_RESORCE_IS_PROD(resource_name))
		resource_index = sipa_rm_prod_index(resource_name);
	else if (SIPA_RM_RESORCE_IS_CONS(resource_name))
		resource_index = sipa_rm_cons_index(resource_name);

	return resource_index;
}

/**
 * sipa_rm_dep_graph_create() - creates graph
 * @dep_graph: [out] created dependency graph
 *
 * Returns: dependency graph on success, NULL on failure
 */
int sipa_rm_dep_graph_create(struct sipa_rm_dep_graph **dep_graph)
{
	*dep_graph = kzalloc(sizeof(**dep_graph), GFP_KERNEL);
	if (!*dep_graph)
		return -ENOMEM;

	return 0;
}

/**
 * sipa_rm_dep_graph_delete() - destroyes the graph
 * @graph: [in] dependency graph
 *
 * Frees all resources.
 */
void sipa_rm_dep_graph_delete(struct sipa_rm_dep_graph *graph)
{
	int resource_index;

	if (!graph) {
		pr_err("invalid params\n");
		return;
	}
	for (resource_index = 0;
	     resource_index < SIPA_RM_RES_MAX;
	     resource_index++)
		kfree(graph->resource_table[resource_index]);
	memset(graph->resource_table, 0, sizeof(graph->resource_table));
}

/**
 * sipa_rm_dep_graph_get_resource() - provides a resource by name
 * @graph: [in] dependency graph
 * @name: [in] name of the resource
 * @resource: [out] resource in case of success
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_dep_graph_get_resource(struct sipa_rm_dep_graph *graph,
				   enum sipa_rm_res_id resource_name,
				   struct sipa_rm_resource **resource)
{
	int resource_index;

	if (!graph)
		return -EINVAL;

	resource_index = sipa_rm_dep_get_index(resource_name);
	if (resource_index == SIPA_RM_INDEX_INVALID)
		return -EINVAL;

	*resource = graph->resource_table[resource_index];
	if (!*resource)
		return -EINVAL;

	return 0;
}

/**
 * sipa_rm_dep_graph_add() - adds resource to graph
 * @graph: [in] dependency graph
 * @resource: [in] resource to add
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_dep_graph_add(struct sipa_rm_dep_graph *graph,
			  struct sipa_rm_resource *resource)
{
	int resource_index;

	if (!graph || !resource)
		return -EINVAL;

	resource_index = sipa_rm_dep_get_index(resource->name);
	if (resource_index == SIPA_RM_INDEX_INVALID)
		return -EINVAL;

	graph->resource_table[resource_index] = resource;

	return 0;
}

/**
 * sipa_rm_dep_graph_remove() - removes resource from graph
 * @graph: [in] dependency graph
 * @resource: [in] resource to add
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_dep_graph_remove(struct sipa_rm_dep_graph *graph,
			     enum sipa_rm_res_id resource_name)
{
	if (!graph)
		return -EINVAL;
	graph->resource_table[resource_name] = NULL;

	return 0;
}

int sipa_rm_dep_graph_add_dependency(struct sipa_rm_dep_graph *graph,
				     enum sipa_rm_res_id cons,
				     enum sipa_rm_res_id prod,
				     bool userspace_dep)
{
	struct sipa_rm_resource *cons_res = NULL;
	struct sipa_rm_resource *prod_res = NULL;

	if (!graph ||
	    !SIPA_RM_RESORCE_IS_CONS(cons) ||
	    !SIPA_RM_RESORCE_IS_PROD(prod)) {
		pr_err("invalid params\n");
		return -EINVAL;
	}
	if (sipa_rm_dep_graph_get_resource(graph,
					   cons,
					   &cons_res)) {
		pr_err("%s does not exist\n",
		       sipa_rm_res_str(cons));
		return -EINVAL;
	}
	if (sipa_rm_dep_graph_get_resource(graph,
					   prod,
					   &prod_res)) {
		pr_err("%s does not exist\n",
		       sipa_rm_res_str(prod));
		return -EINVAL;
	}

	return sipa_rm_resource_add_dependency(cons_res, prod_res,
					       userspace_dep);
}

/**
 * sipa_rm_dep_graph_delete_dependency() - deleted dependency between
 *				two nodes in graph
 * @graph: [in] dependency graph
 * @resource_name: [in] resource to delete
 * @depends_on_name: [in] resource to delete
 * @userspace_dep: [in] operation requested by userspace ?
 *
 * Returns: 0 on success, negative on failure
 */
int sipa_rm_dep_graph_delete_dependency(struct sipa_rm_dep_graph *graph,
					enum sipa_rm_res_id cons,
					enum sipa_rm_res_id pord,
					bool userspace_dep)
{
	struct sipa_rm_resource *cons_res = NULL;
	struct sipa_rm_resource *pord_res = NULL;

	if (!graph ||
	    !SIPA_RM_RESORCE_IS_PROD(pord) ||
	    !SIPA_RM_RESORCE_IS_CONS(cons)) {
		pr_err("invalid params\n");
		return -EINVAL;
	}

	if (sipa_rm_dep_graph_get_resource(graph,
					   cons,
					   &cons_res)) {
		pr_info("%s does not exist\n",
			sipa_rm_res_str(cons));
		return -EINVAL;
	}

	if (sipa_rm_dep_graph_get_resource(graph,
					   pord,
					   &pord_res)) {
		pr_info("%s does not exist\n",
			sipa_rm_res_str(pord));
		return -EINVAL;
	}

	return sipa_rm_resource_delete_dependency(cons_res, pord_res,
			userspace_dep);
}
