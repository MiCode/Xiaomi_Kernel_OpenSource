// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2021 MediaTek Inc.
 * Author: Chia-Mao Hung <chia-mao.hung@mediatek.com>
 */
#include <linux/module.h>
#include "vcp_status.h"
#include "vcp.h"

struct vcp_status_fp *vcp_fp;

int pwclkcnt;
EXPORT_SYMBOL_GPL(pwclkcnt);
bool is_suspending;
EXPORT_SYMBOL_GPL(is_suspending);

static int __init mtk_vcp_status_init(void)
{
	pwclkcnt = 0;
	return 0;
}

int mmup_enable_count(void)
{
	return ((is_suspending) ? 0 : pwclkcnt);
}
EXPORT_SYMBOL_GPL(mmup_enable_count);

void vcp_set_fp(struct vcp_status_fp *fp)
{
	if (!fp)
		return;
	vcp_fp = fp;
}
EXPORT_SYMBOL_GPL(vcp_set_fp);

phys_addr_t vcp_get_reserve_mem_phys_ex(enum vcp_reserve_mem_id_t id)
{
	if (!vcp_fp || !vcp_fp->vcp_get_reserve_mem_phys)
		return 0;
	return vcp_fp->vcp_get_reserve_mem_phys(id);
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_phys_ex);

phys_addr_t vcp_get_reserve_mem_virt_ex(enum vcp_reserve_mem_id_t id)
{
	if (!vcp_fp || !vcp_fp->vcp_get_reserve_mem_virt)
		return 0;
	return vcp_fp->vcp_get_reserve_mem_virt(id);
}
EXPORT_SYMBOL_GPL(vcp_get_reserve_mem_virt_ex);

void vcp_register_feature_ex(enum feature_id id)
{
	if (!vcp_fp || !vcp_fp->vcp_register_feature)
		return;
	vcp_fp->vcp_register_feature(id);
}
EXPORT_SYMBOL_GPL(vcp_register_feature_ex);

void vcp_deregister_feature_ex(enum feature_id id)
{
	if (!vcp_fp || !vcp_fp->vcp_deregister_feature)
		return;
	vcp_fp->vcp_deregister_feature(id);
}
EXPORT_SYMBOL_GPL(vcp_deregister_feature_ex);

unsigned int is_vcp_ready_ex(enum vcp_core_id id)
{
	if (!vcp_fp || !vcp_fp->is_vcp_ready)
		return 0;
	return vcp_fp->is_vcp_ready(id);
}
EXPORT_SYMBOL_GPL(is_vcp_ready_ex);

void vcp_A_register_notify_ex(struct notifier_block *nb)
{
	if (!vcp_fp || !vcp_fp->vcp_A_register_notify)
		return;
	vcp_fp->vcp_A_register_notify(nb);
}
EXPORT_SYMBOL_GPL(vcp_A_register_notify_ex);

void vcp_A_unregister_notify_ex(struct notifier_block *nb)
{
	if (!vcp_fp || !vcp_fp->vcp_A_unregister_notify)
		return;
	vcp_fp->vcp_A_unregister_notify(nb);
}
EXPORT_SYMBOL_GPL(vcp_A_unregister_notify_ex);

static void __exit mtk_vcp_status_exit(void)
{
}
module_init(mtk_vcp_status_init);
module_exit(mtk_vcp_status_exit);
MODULE_LICENSE("GPL v2");
