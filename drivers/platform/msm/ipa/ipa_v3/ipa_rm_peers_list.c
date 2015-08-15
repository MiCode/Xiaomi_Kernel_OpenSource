/* Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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
#include "ipa_i.h"
#include "ipa_rm_i.h"

/**
 * ipa_rm_peers_list_get_resource_index() - resource name to index
 *	of this resource in corresponding peers list
 * @resource_name: [in] resource name
 *
 * Returns: resource index mapping, IPA_RM_INDEX_INVALID
 * in case provided resource name isn't contained in enum
 * ipa_rm_resource_name.
 *
 */
static int ipa_rm_peers_list_get_resource_index(
		enum ipa_rm_resource_name resource_name)
{
	int resource_index = IPA_RM_INDEX_INVALID;

	if (IPA_RM_RESORCE_IS_PROD(resource_name))
		resource_index = ipa_rm_prod_index(resource_name);
	else if (IPA_RM_RESORCE_IS_CONS(resource_name)) {
		resource_index = ipa_rm_cons_index(resource_name);
		if (resource_index != IPA_RM_INDEX_INVALID)
			resource_index =
				resource_index - IPA_RM_RESOURCE_PROD_MAX;
	}

	return resource_index;
}

static bool ipa_rm_peers_list_check_index(int index,
		struct ipa_rm_peers_list *peers_list)
{
	return !(index > peers_list->max_peers || index < 0);
}

/**
 * ipa_rm_peers_list_create() - creates the peers list
 *
 * @max_peers: maximum number of peers in new list
 * @peers_list: [out] newly created peers list
 *
 * Returns: 0 in case of SUCCESS, negative otherwise
 */
int ipa_rm_peers_list_create(int max_peers,
		struct ipa_rm_peers_list **peers_list)
{
	int result;

	*peers_list = kzalloc(sizeof(**peers_list), GFP_ATOMIC);
	if (!*peers_list) {
		IPA_RM_ERR("no mem\n");
		result = -ENOMEM;
		goto bail;
	}

	(*peers_list)->max_peers = max_peers;
	(*peers_list)->peers = kzalloc((*peers_list)->max_peers *
				sizeof(struct ipa_rm_resource *), GFP_ATOMIC);
	if (!((*peers_list)->peers)) {
		IPA_RM_ERR("no mem\n");
		result = -ENOMEM;
		goto list_alloc_fail;
	}

	return 0;

list_alloc_fail:
	kfree(*peers_list);
bail:
	return result;
}

/**
 * ipa_rm_peers_list_delete() - deletes the peers list
 *
 * @peers_list: peers list
 *
 */
void ipa_rm_peers_list_delete(struct ipa_rm_peers_list *peers_list)
{
	if (peers_list) {
		kfree(peers_list->peers);
		kfree(peers_list);
	}
}

/**
 * ipa_rm_peers_list_remove_peer() - removes peer from the list
 *
 * @peers_list: peers list
 * @resource_name: name of the resource to remove
 *
 */
void ipa_rm_peers_list_remove_peer(
		struct ipa_rm_peers_list *peers_list,
		enum ipa_rm_resource_name resource_name)
{
	if (!peers_list)
		return;

	peers_list->peers[ipa_rm_peers_list_get_resource_index(
			resource_name)] = NULL;
	peers_list->peers_count--;
}

/**
 * ipa_rm_peers_list_add_peer() - adds peer to the list
 *
 * @peers_list: peers list
 * @resource: resource to add
 *
 */
void ipa_rm_peers_list_add_peer(
		struct ipa_rm_peers_list *peers_list,
		struct ipa_rm_resource *resource)
{
	if (!peers_list || !resource)
		return;

	peers_list->peers[ipa_rm_peers_list_get_resource_index(
			resource->name)] =
			resource;
	peers_list->peers_count++;
}

/**
 * ipa_rm_peers_list_is_empty() - checks
 *	if resource peers list is empty
 *
 * @peers_list: peers list
 *
 * Returns: true if the list is empty, false otherwise
 */
bool ipa_rm_peers_list_is_empty(struct ipa_rm_peers_list *peers_list)
{
	bool result = true;

	if (!peers_list)
		goto bail;

	if (peers_list->peers_count > 0)
		result = false;
bail:
	return result;
}

/**
 * ipa_rm_peers_list_has_last_peer() - checks
 *	if resource peers list has exactly one peer
 *
 * @peers_list: peers list
 *
 * Returns: true if the list has exactly one peer, false otherwise
 */
bool ipa_rm_peers_list_has_last_peer(
		struct ipa_rm_peers_list *peers_list)
{
	bool result = false;

	if (!peers_list)
		goto bail;

	if (peers_list->peers_count == 1)
		result = true;
bail:
	return result;
}

/**
 * ipa_rm_peers_list_check_dependency() - check dependency
 *	between 2 peer lists
 * @resource_peers: first peers list
 * @resource_name: first peers list resource name
 * @depends_on_peers: second peers list
 * @depends_on_name: second peers list resource name
 *
 * Returns: true if there is dependency, false otherwise
 *
 */
bool ipa_rm_peers_list_check_dependency(
		struct ipa_rm_peers_list *resource_peers,
		enum ipa_rm_resource_name resource_name,
		struct ipa_rm_peers_list *depends_on_peers,
		enum ipa_rm_resource_name depends_on_name)
{
	bool result = false;

	if (!resource_peers || !depends_on_peers)
		return result;

	if (resource_peers->peers[ipa_rm_peers_list_get_resource_index(
			depends_on_name)] != NULL)
		result = true;

	if (depends_on_peers->peers[ipa_rm_peers_list_get_resource_index(
						resource_name)] != NULL)
		result = true;

	return result;
}

/**
 * ipa_rm_peers_list_get_resource() - get resource by
 *	resource index
 * @resource_index: resource index
 * @resource_peers: peers list
 *
 * Returns: the resource if found, NULL otherwise
 */
struct ipa_rm_resource *ipa_rm_peers_list_get_resource(int resource_index,
		struct ipa_rm_peers_list *resource_peers)
{
	struct ipa_rm_resource *result = NULL;

	if (!ipa_rm_peers_list_check_index(resource_index, resource_peers))
		goto bail;

	result = resource_peers->peers[resource_index];
bail:
	return result;
}

/**
 * ipa_rm_peers_list_get_size() - get peers list sise
 *
 * @peers_list: peers list
 *
 * Returns: the size of the peers list
 */
int ipa_rm_peers_list_get_size(struct ipa_rm_peers_list *peers_list)
{
	return peers_list->max_peers;
}
