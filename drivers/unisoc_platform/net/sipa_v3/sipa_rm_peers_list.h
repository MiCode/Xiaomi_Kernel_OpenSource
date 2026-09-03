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

#ifndef _SIPA_RM_PEERS_LIST_H_
#define _SIPA_RM_PEERS_LIST_H_

#include "sipa_rm_res.h"

struct sipa_rm_res_peer {
	struct sipa_rm_resource *resource;
	bool userspace_dep;
};

/**
 * struct sipa_rm_peers_list - SIPA RM resource peers list
 * @peers: the list of references to resources dependent on this resource
 *          in case of producer or list of dependencies in case of consumer
 * @max_peers: maximum number of peers for this resource
 * @peers_count: actual number of peers for this resource
 */
struct sipa_rm_peers_list {
	struct sipa_rm_res_peer	*peers;
	int max_peers;
	int peers_count;
};

int sipa_rm_peers_list_create(int max_peers,
			      struct sipa_rm_peers_list **peers_list);
void sipa_rm_peers_list_delete(struct sipa_rm_peers_list *peers_list);
void sipa_rm_peers_list_remove_peer(struct sipa_rm_peers_list *peers_list,
				    enum sipa_rm_res_id resource_name);
void sipa_rm_peers_list_add_peer(struct sipa_rm_peers_list *peers_list,
				 struct sipa_rm_resource *resource,
				 bool userspace_dep);
bool sipa_rm_peers_list_check_dependency(struct sipa_rm_peers_list *resource_peers,
					 enum sipa_rm_res_id resource_name,
					 struct sipa_rm_peers_list *depends_on_peers,
					 enum sipa_rm_res_id depends_on_name,
					 bool *userspace_dep);
struct sipa_rm_resource *sipa_rm_peers_list_get_res(int resource_index,
						    struct sipa_rm_peers_list
						    *peers_list);
bool sipa_rm_peers_list_get_userspace_dep(int resource_index,
					  struct sipa_rm_peers_list *
					  resource_peers);
int sipa_rm_peers_list_get_size(struct sipa_rm_peers_list *peers_list);
bool sipa_rm_peers_list_is_empty(struct sipa_rm_peers_list *peers_list);
bool sipa_rm_peers_list_has_last_peer(struct sipa_rm_peers_list *peers_list);

#endif /* ! _SIPA_RM_PEERS_LIST_H_ */
