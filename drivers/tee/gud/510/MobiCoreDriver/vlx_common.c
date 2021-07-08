// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019-2020 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifdef CONFIG_VLX_HYP

#include "vrpc.h"
#include "main.h"
#include "client.h"
#include "vlx_common.h"

const struct tee_vlx_vm_info *tee_vlx_find_vm_info(NkOsId vm_no)
{
	const struct tee_vlx_vm_info *vm_info = &tee_vlx_vms_info[0];

	while (vm_info->id) {
		if (vm_info->no == vm_no)
			return vm_info;

		vm_info++;
	}

	return NULL;
}

struct tee_vlx_fe *tee_vlx_fe_create(const struct tee_vlx_vm_info *vm_info)
{
	struct tee_vlx_fe *vlx_fe;
	int ret;

	vlx_fe = kzalloc(sizeof(*vlx_fe), GFP_KERNEL);
	if (!vlx_fe)
		return ERR_PTR(-ENOMEM);

	ret = protocol_fe_init(&vlx_fe->pfe);
	if (ret) {
		kfree(vlx_fe);
		mc_dev_err(ret, "failed to init vlx_fe");
		return ERR_PTR(ret);
	}

	atomic_inc(&g_ctx.c_vm_fes);

	kref_init(&vlx_fe->kref);
	INIT_LIST_HEAD(&vlx_fe->list);
	vlx_fe->vm_info = vm_info;
	return vlx_fe;
}

static void tee_vlx_fe_release(struct kref *kref)
{
	struct tee_vlx_fe *vlx_fe = container_of(kref, struct tee_vlx_fe, kref);
	struct vrpc_t *vrpc_be = vlx_fe->fe2be_vrpc;

	tee_vlx_fe_cleanup(vlx_fe);

	if (vrpc_be) {
		vrpc_close(vrpc_be);
		vrpc_release(vrpc_be);
	}

	kfree(vlx_fe);
	atomic_dec(&g_ctx.c_vm_fes);
}

void tee_vlx_fe_put(struct tee_vlx_fe *vlx_fe)
{
	kref_put(&vlx_fe->kref, tee_vlx_fe_release);
}

void tee_vlx_fe_cleanup(struct tee_vlx_fe *vlx_fe)
{
	if (vlx_fe->pfe.client) {
		client_close(vlx_fe->pfe.client);
		vlx_fe->pfe.client = NULL;
	}

	if (vlx_fe->be2fe_vrpc) {
		vrpc_close(vlx_fe->be2fe_vrpc);
		vrpc_release(vlx_fe->be2fe_vrpc);
		vlx_fe->be2fe_vrpc = NULL;
	}
}

#endif /* CONFIG_VLX_HYP */
