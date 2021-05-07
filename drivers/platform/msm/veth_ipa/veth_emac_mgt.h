/* SPDX-License-Identifier: GPL-2.0-only
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#ifndef _VETH_EMAC_MGT_H_
#define _VETH_EMAC_MGT_H_


#include "veth_ipa.h"

#define  MM_DATA_NETWORK_1_VM_HAB_MINOR_ID 10


int veth_emac_init(struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata,
					bool smmu_s2_enb);

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
int emac_ipa_hab_export_tx_buf_pool(int vc_id,
					struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata);
int emac_ipa_hab_export_rx_buf_pool(int vc_id,
					struct veth_emac_export_mem *veth_emac_mem,
					struct veth_ipa_dev *pdata);
int veth_emac_ipa_send_exp_id(int vc_id,
					struct veth_emac_export_mem *veth_emac_mem);




#endif /* _VETH_EMAC_MGT_H_ */
