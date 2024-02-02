/* Copyright (c) 2013-2017, The Linux Foundation. All rights reserved.
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

#ifndef _IPA_RM_PEERS_LIST_H_
#define _IPA_RM_PEERS_LIST_H_

#include "ipa_rm_resource.h"

struct ipa_rm_resource_peer {
	struct ipa_rm_resource *resource;
	bool userspace_dep;
};

/**
 * struct ipa_rm_peers_list - IPA RM resource peers list
 * @peers: the list of references to resources dependent on this resource
 *          in case of producer or list of dependencies in case of consumer
 * @max_peers: maximum number of peers for this resource
 * @peers_count: actual number of peers for this resource
 */
struct ipa_rm_peers_list {
	struct ipa_rm_resource_peer	*peers;
	int				max_peers;
	int				peers_count;
};

int ipa_rm_peers_list_create(int max_peers,
		struct ipa_rm_peers_list **peers_list);
void ipa_rm_peers_list_delete(struct ipa_rm_peers_list *peers_list);
void ipa_rm_peers_list_remove_peer(
		struct ipa_rm_peers_list *peers_list,
		enum ipa_rm_resource_name resource_name);
void ipa_rm_peers_list_add_peer(
		struct ipa_rm_peers_list *peers_list,
		struct ipa_rm_resource *resource,
		bool userspace_dep);
bool ipa_rm_peers_list_check_dependency(
		struct ipa_rm_peers_list *resource_peers,
		enum ipa_rm_resource_name resource_name,
		struct ipa_rm_peers_list *depends_on_peers,
		enum ipa_rm_resource_name depends_on_name,
		bool *userspace_dep);
struct ipa_rm_resource *ipa_rm_peers_list_get_resource(int resource_index,
		struct ipa_rm_peers_list *peers_list);
bool ipa_rm_peers_list_get_userspace_dep(int resource_index,
		struct ipa_rm_peers_list *resource_peers);
int ipa_rm_peers_list_get_size(struct ipa_rm_peers_list *peers_list);
bool ipa_rm_peers_list_is_empty(struct ipa_rm_peers_list *peers_list);
bool ipa_rm_peers_list_has_last_peer(
		struct ipa_rm_peers_list *peers_list);


#endif /* _IPA_RM_PEERS_LIST_H_ */
