/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef VCP_STATUS_H
#define VCP_STATUS_H

#include "vcp.h"

typedef phys_addr_t (*vcp_get_reserve_mem_phys_fp)(enum vcp_reserve_mem_id_t id);
typedef phys_addr_t (*vcp_get_reserve_mem_virt_fp)(enum vcp_reserve_mem_id_t id);
typedef void (*vcp_register_feature_fp)(enum feature_id id);
typedef void (*vcp_deregister_feature_fp)(enum feature_id id);
typedef unsigned int (*is_vcp_ready_fp)(enum vcp_core_id id);
typedef void (*vcp_A_register_notify_fp)(struct notifier_block *nb);
typedef void (*vcp_A_unregister_notify_fp)(struct notifier_block *nb);

struct vcp_status_fp {
	vcp_get_reserve_mem_phys_fp	vcp_get_reserve_mem_phys;
	vcp_get_reserve_mem_virt_fp	vcp_get_reserve_mem_virt;
	vcp_register_feature_fp		vcp_register_feature;
	vcp_deregister_feature_fp	vcp_deregister_feature;
	is_vcp_ready_fp				is_vcp_ready;
	vcp_A_register_notify_fp	vcp_A_register_notify;
	vcp_A_unregister_notify_fp	vcp_A_unregister_notify;
};

extern int pwclkcnt;
extern bool is_suspending;
int mmup_enable_count(void);
void vcp_set_fp(struct vcp_status_fp *fp);
phys_addr_t vcp_get_reserve_mem_phys_ex(enum vcp_reserve_mem_id_t id);
phys_addr_t vcp_get_reserve_mem_virt_ex(enum vcp_reserve_mem_id_t id);
void vcp_register_feature_ex(enum feature_id id);
void vcp_deregister_feature_ex(enum feature_id id);
unsigned int is_vcp_ready_ex(enum vcp_core_id id);
void vcp_A_register_notify_ex(struct notifier_block *nb);
void vcp_A_unregister_notify_ex(struct notifier_block *nb);

#endif
