/* SPDX-License-Identifier: GPL-2.0-only */
/*
 * Copyright (c) 2013-2019, The Linux Foundation. All rights reserved.
 */

#ifndef _IPA_RM_DEPENDENCY_GRAPH_H_
#define _IPA_RM_DEPENDENCY_GRAPH_H_

#include <linux/list.h>
#include <linux/ipa.h>
#include "ipa_rm_resource.h"

struct ipa_rm_dep_graph {
	struct ipa_rm_resource *resource_table[IPA_RM_RESOURCE_MAX];
};

int ipa_rm_dep_graph_get_resource(
				struct ipa_rm_dep_graph *graph,
				enum ipa_rm_resource_name name,
				struct ipa_rm_resource **resource);

int ipa_rm_dep_graph_create(struct ipa_rm_dep_graph **dep_graph);

void ipa_rm_dep_graph_delete(struct ipa_rm_dep_graph *graph);

int ipa_rm_dep_graph_add(struct ipa_rm_dep_graph *graph,
			 struct ipa_rm_resource *resource);

int ipa_rm_dep_graph_remove(struct ipa_rm_dep_graph *graph,
				enum ipa_rm_resource_name resource_name);

int ipa_rm_dep_graph_add_dependency(struct ipa_rm_dep_graph *graph,
				enum ipa_rm_resource_name resource_name,
				enum ipa_rm_resource_name depends_on_name,
				bool userspsace_dep);

int ipa_rm_dep_graph_delete_dependency(struct ipa_rm_dep_graph *graph,
				enum ipa_rm_resource_name resource_name,
				enum ipa_rm_resource_name depends_on_name,
				bool userspsace_dep);

#endif /* _IPA_RM_DEPENDENCY_GRAPH_H_ */
