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
#include "sipa_rm_peers_list.h"

/**
 * sipa_rm_peers_list_get_res_idx() - resource name to index
 *	of this resource in corresponding peers list
 * @resource_name: [in] resource name
 *
 * Returns: resource index mapping, SIPA_RM_INDEX_INVALID
 * in case provided resource name isn't contained in enum
 * sipa_rm_res_id.
 */
static int
sipa_rm_peers_list_get_res_idx(enum sipa_rm_res_id resource_name)
{
	int resource_index = SIPA_RM_INDEX_INVALID;

	if (SIPA_RM_RESORCE_IS_PROD(resource_name)) {
		resource_index = sipa_rm_prod_index(resource_name);
	} else if (SIPA_RM_RESORCE_IS_CONS(resource_name)) {
		resource_index = sipa_rm_cons_index(resource_name);
		if (resource_index != SIPA_RM_INDEX_INVALID)
			resource_index =
				resource_index - SIPA_RM_RES_PROD_MAX;
	}

	return resource_index;
}

static bool
sipa_rm_peers_list_check_index(int index,
			       struct sipa_rm_peers_list *peers_list)
{
	return !(index > peers_list->max_peers || index < 0);
}

/**
 * sipa_rm_peers_list_create() - creates the peers list
 *
 * @max_peers: maximum number of peers in new list
 * @peers_list: [out] newly created peers list
 *
 * Returns: 0 in case of SUCCESS, negative otherwise
 */
int sipa_rm_peers_list_create(int max_peers,
			      struct sipa_rm_peers_list **peers_list)
{
	*peers_list = kzalloc(sizeof(**peers_list), GFP_KERNEL);
	if (!*peers_list)
		return -ENOMEM;

	(*peers_list)->max_peers = max_peers;
	(*peers_list)->peers = kzalloc((*peers_list)->max_peers *
				       sizeof(*(*peers_list)->peers),
				       GFP_KERNEL);
	if (!(*peers_list)->peers) {
		kfree(*peers_list);
		return -ENOMEM;
	}

	return 0;
}

/**
 * sipa_rm_peers_list_delete() - deletes the peers list
 *
 * @peers_list: peers list
 */
void sipa_rm_peers_list_delete(struct sipa_rm_peers_list *peers_list)
{
	if (peers_list) {
		kfree(peers_list->peers);
		kfree(peers_list);
	}
}

/**
 * sipa_rm_peers_list_remove_peer() - removes peer from the list
 *
 * @peers_list: peers list
 * @resource_name: name of the resource to remove
 */
void sipa_rm_peers_list_remove_peer(struct sipa_rm_peers_list *peers_list,
				    enum sipa_rm_res_id resource_name)
{
	if (!peers_list)
		return;

	peers_list->peers[sipa_rm_peers_list_get_res_idx(resource_name)].resource = NULL;
	peers_list->peers[sipa_rm_peers_list_get_res_idx(resource_name)].userspace_dep = false;
	peers_list->peers_count--;
}

/**
 * sipa_rm_peers_list_add_peer() - adds peer to the list
 *
 * @peers_list: peers list
 * @resource: resource to add
 */
void sipa_rm_peers_list_add_peer(struct sipa_rm_peers_list *peers_list,
				 struct sipa_rm_resource *resource,
				 bool userspace_dep)
{
	if (!peers_list || !resource)
		return;

	peers_list->peers[sipa_rm_peers_list_get_res_idx(resource->name)].resource = resource;
	peers_list->peers[sipa_rm_peers_list_get_res_idx(resource->name)].userspace_dep =
	userspace_dep;
	peers_list->peers_count++;
}

/**
 * sipa_rm_peers_list_is_empty() - checks
 *	if resource peers list is empty
 *
 * @peers_list: peers list
 *
 * Returns: true if the list is empty, false otherwise
 */
bool sipa_rm_peers_list_is_empty(struct sipa_rm_peers_list *peers_list)
{
	if (!peers_list)
		return true;

	if (peers_list->peers_count > 0)
		return false;

	return true;
}

/**
 * sipa_rm_peers_list_has_last_peer() - checks
 *	if resource peers list has exactly one peer
 *
 * @peers_list: peers list
 *
 * Returns: true if the list has exactly one peer, false otherwise
 */
bool sipa_rm_peers_list_has_last_peer(struct sipa_rm_peers_list *peers_list)
{
	if (!peers_list)
		return false;

	if (peers_list->peers_count == 1)
		return true;

	return false;
}

/**
 * sipa_rm_peers_list_check_dependency() - check dependency
 *	between 2 peer lists
 * @resource_peers: first peers list
 * @resource_name: first peers list resource name
 * @depends_on_peers: second peers list
 * @depends_on_name: second peers list resource name
 * @userspace_dep: [out] dependency was created by userspace
 *
 * Returns: true if there is dependency, false otherwise
 */
bool
sipa_rm_peers_list_check_dependency(struct sipa_rm_peers_list *resource_peers,
				    enum sipa_rm_res_id resource_name,
				    struct sipa_rm_peers_list *depends_on_peers,
				    enum sipa_rm_res_id depends_on_name,
				    bool *userspace_dep)
{
	bool result = false;
	int res_idx;

	if (!resource_peers || !depends_on_peers || !userspace_dep)
		return result;

	res_idx = sipa_rm_peers_list_get_res_idx(depends_on_name);
	if (resource_peers->peers[res_idx].resource) {
		result = true;
		*userspace_dep = resource_peers->peers[res_idx]. userspace_dep;
	}

	res_idx = sipa_rm_peers_list_get_res_idx(resource_name);
	if (depends_on_peers->peers[res_idx].resource) {
		result = true;
		*userspace_dep = depends_on_peers->peers[res_idx].userspace_dep;
	}

	return result;
}

/**
 * sipa_rm_peers_list_get_res() - get resource by
 *	resource index
 * @resource_index: resource index
 * @resource_peers: peers list
 *
 * Returns: the resource if found, NULL otherwise
 */
struct sipa_rm_resource *
sipa_rm_peers_list_get_res(int resource_index,
			   struct sipa_rm_peers_list *resource_peers)
{
	if (!sipa_rm_peers_list_check_index(resource_index, resource_peers))
		return NULL;

	return resource_peers->peers[resource_index].resource;
}

/**
 * sipa_rm_peers_list_get_userspace_dep() - returns whether resource dependency
 * was added by userspace
 * @resource_index: resource index
 * @resource_peers: peers list
 *
 * Returns: true if dependency was added by userspace, false by kernel
 */
bool
sipa_rm_peers_list_get_userspace_dep(int resource_index,
				     struct sipa_rm_peers_list *resource_peers)
{
	if (!sipa_rm_peers_list_check_index(resource_index, resource_peers))
		return false;

	return resource_peers->peers[resource_index].userspace_dep;
}

/**
 * sipa_rm_peers_list_get_size() - get peers list sise
 *
 * @peers_list: peers list
 *
 * Returns: the size of the peers list
 */
int sipa_rm_peers_list_get_size(struct sipa_rm_peers_list *peers_list)
{
	return peers_list->max_peers;
}
