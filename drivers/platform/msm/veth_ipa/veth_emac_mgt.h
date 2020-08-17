/* Copyright (c) 2020, The Linux Foundation. All rights reserved.
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

#ifndef _VETH_EMAC_MGT_H_
#define _VETH_EMAC_MGT_H_


#include "veth_ipa.h"

#define  MM_DATA_NETWORK_1_VM_HAB_MINOR_ID 10


int veth_emac_init(struct veth_emac_export_mem *veth_emac_mem,
				   struct veth_ipa_dev *pdata);

int veth_alloc_emac_export_mem(struct veth_emac_export_mem *veth_emac_mem,
	struct veth_ipa_dev *pdata);

int veth_emac_start_offload(struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata);

int veth_emac_setup_be(struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata);

int veth_emac_ipa_hab_init(int mmid);
int veth_alloc_emac_dealloc_mem(struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata);
int veth_emac_stop_offload(struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata);
int veth_emac_open_notify(struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata);
int veth_emac_ipa_setup_complete(struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata);


#endif /* _VETH_EMAC_MGT_H_ */
