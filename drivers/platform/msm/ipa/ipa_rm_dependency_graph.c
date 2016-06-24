/* Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#include <linux/slab.h>
#include "ipa_rm_dependency_graph.h"
#include "ipa_rm_i.h"

static int ipa_rm_dep_get_index(enum ipa_rm_resource_name resource_name)
{
	int resource_index = IPA_RM_INDEX_INVALID;

	if (IPA_RM_RESORCE_IS_PROD(resource_name))
		resource_index = ipa_rm_prod_index(resource_name);
	else if (IPA_RM_RESORCE_IS_CONS(resource_name))
		resource_index = ipa_rm_cons_index(resource_name);

	return resource_index;
}

/**
 * ipa_rm_dep_graph_create() - creates graph
 * @dep_graph: [out] created dependency graph
 *
 * Returns: dependency graph on success, NULL on failure
 */
int  ipa_rm_dep_graph_create(struct ipa_rm_dep_graph **dep_graph)
{
	int result = 0;

	*dep_graph = kzalloc(sizeof(**dep_graph), GFP_KERNEL);
	if (!*dep_graph) {
		IPA_RM_ERR("no mem\n");
		result = -ENOMEM;
		goto bail;
	}
bail:
	return result;
}

/**
 * ipa_rm_dep_graph_delete() - destroyes the graph
 * @graph: [in] dependency graph
 *
 * Frees all resources.
 */
void ipa_rm_dep_graph_delete(struct ipa_rm_dep_graph *graph)
{
	int resource_index;

	if (!graph) {
		IPA_RM_ERR("invalid params\n");
		return;
	}
	for (resource_index = 0;
			resource_index < IPA_RM_RESOURCE_MAX;
			resource_index++)
		kfree(graph->resource_table[resource_index]);
	memset(graph->resource_table, 0, sizeof(graph->resource_table));
}

/**
 * ipa_rm_dep_graph_get_resource() - provides a resource by name
 * @graph: [in] dependency graph
 * @name: [in] name of the resource
 * @resource: [out] resource in case of success
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_dep_graph_get_resource(
				struct ipa_rm_dep_graph *graph,
				enum ipa_rm_resource_name resource_name,
				struct ipa_rm_resource **resource)
{
	int result;
	int resource_index;

	if (!graph) {
		result = -EINVAL;
		goto bail;
	}
	resource_index = ipa_rm_dep_get_index(resource_name);
	if (resource_index == IPA_RM_INDEX_INVALID) {
		result = -EINVAL;
		goto bail;
	}
	*resource = graph->resource_table[resource_index];
	if (!*resource) {
		result = -EINVAL;
		goto bail;
	}
	result = 0;
bail:
	return result;
}

/**
 * ipa_rm_dep_graph_add() - adds resource to graph
 * @graph: [in] dependency graph
 * @resource: [in] resource to add
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_dep_graph_add(struct ipa_rm_dep_graph *graph,
			 struct ipa_rm_resource *resource)
{
	int result = 0;
	int resource_index;

	if (!graph || !resource) {
		result = -EINVAL;
		goto bail;
	}
	resource_index = ipa_rm_dep_get_index(resource->name);
	if (resource_index == IPA_RM_INDEX_INVALID) {
		result = -EINVAL;
		goto bail;
	}
	graph->resource_table[resource_index] = resource;
bail:
	return result;
}

/**
 * ipa_rm_dep_graph_remove() - removes resource from graph
 * @graph: [in] dependency graph
 * @resource: [in] resource to add
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_dep_graph_remove(struct ipa_rm_dep_graph *graph,
		enum ipa_rm_resource_name resource_name)
{
	if (!graph)
		return -EINVAL;
	graph->resource_table[resource_name] = NULL;

	return 0;
}

/**
 * ipa_rm_dep_graph_add_dependency() - adds dependency between
 *				two nodes in graph
 * @graph: [in] dependency graph
 * @resource_name: [in] resource to add
 * @depends_on_name: [in] resource to add
 * @userspace_dep: [in] operation requested by userspace ?
 *
 * Returns: 0 on success, negative on failure
 */
int ipa_rm_dep_graph_add_dependency(struct ipa_rm_dep_graph *graph,
				    enum ipa_rm_resource_name resource_name,
				    enum ipa_rm_resource_name depends_on_name,
				    bool userspace_dep)
{
	struct ipa_rm_resource *dependent = NULL;
	struct ipa_rm_resource *dependency = NULL;
	int result;

	if (!graph ||
		!IPA_RM_RESORCE_IS_PROD(resource_name) ||
		!IPA_RM_RESORCE_IS_CONS(depends_on_name)) {
		IPA_RM_ERR("invalid params\n");
		result = -EINVAL;
		goto bail;
	}
	if (ipa_rm_dep_graph_get_resource(graph,
					  resource_name,
					  &dependent)) {
		IPA_RM_ERR("%s does not exist\n",
					ipa_rm_resource_str(resource_name));
		result = -EINVAL;
		goto bail;
	}
	if (ipa_rm_dep_graph_get_resource(graph,
					depends_on_name,
					  &dependency)) {
		IPA_RM_ERR("%s does not exist\n",
					ipa_rm_resource_str(depends_on_name));
		result = -EINVAL;
		goto bail;
	}
	result = ipa_rm_resource_add_dependency(dependent, dependency,
		userspace_dep);
bail:
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}

/**
 * ipa_rm_dep_graph_delete_dependency() - deleted dependency between
 *				two nodes in graph
 * @graph: [in] dependency graph
 * @resource_name: [in] resource to delete
 * @depends_on_name: [in] resource to delete
 * @userspace_dep: [in] operation requested by userspace ?
 *
 * Returns: 0 on success, negative on failure
 *
 */
int ipa_rm_dep_graph_delete_dependency(struct ipa_rm_dep_graph *graph,
				enum ipa_rm_resource_name resource_name,
				enum ipa_rm_resource_name depends_on_name,
				bool userspace_dep)
{
	struct ipa_rm_resource *dependent = NULL;
	struct ipa_rm_resource *dependency = NULL;
	int result;

	if (!graph ||
		!IPA_RM_RESORCE_IS_PROD(resource_name) ||
		!IPA_RM_RESORCE_IS_CONS(depends_on_name)) {
		IPA_RM_ERR("invalid params\n");
		result = -EINVAL;
		goto bail;
	}

	if (ipa_rm_dep_graph_get_resource(graph,
					  resource_name,
					  &dependent)) {
		IPA_RM_ERR("%s does not exist\n",
					ipa_rm_resource_str(resource_name));
		result = -EINVAL;
		goto bail;
	}

	if (ipa_rm_dep_graph_get_resource(graph,
					  depends_on_name,
					  &dependency)) {
		IPA_RM_ERR("%s does not exist\n",
					ipa_rm_resource_str(depends_on_name));
		result = -EINVAL;
		goto bail;
	}

	result = ipa_rm_resource_delete_dependency(dependent, dependency,
		userspace_dep);
bail:
	IPA_RM_DBG("EXIT with %d\n", result);

	return result;
}
