/* SPDX-License-Identifier: GPL-2.0 */
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

#ifndef MC_VLX_COMMON_H
#define MC_VLX_COMMON_H

#include <linux/list.h>
#include <linux/workqueue.h>

#include <nk/nkern.h>		/* NkOsId*/

#include "public/mc_user.h"	/* many types */
#include "protocol_common.h"
#include "platform.h"

#ifdef MC_USE_VLX_PMEM
#define MC_VLX_PMEM_PAGES 128
#endif

struct tee_vlx_vm_info {
	NkOsId			no;
	const char		*id;
	/* VRPC channel names must be aligned with device tree, NULL for BE */
	const char		*call_channel_name;
	const char		*callback_channel_name;
};

static const struct tee_vlx_vm_info tee_vlx_vms_info[] = {
	{
		.no = 2,
		.id = "sys",
	},
	{
		.no = 3,
		.id = "ivi",
		.call_channel_name = "vtrustonic_ivi",
		.callback_channel_name = "vtrustonic_ivi cb",
	},
	{
		.no = 4,
		.id = "android",
		.call_channel_name = "vtrustonic_and",
		.callback_channel_name = "vtrustonic_and cb",
	},
	/* End of list */
	{
		.id = NULL,
	},
};

struct tee_vlx_fe {
	struct protocol_fe		pfe;
	struct kref			kref;
	struct list_head		list;
	/* VM information */
	const struct tee_vlx_vm_info	*vm_info;
	/* Channels BE FE */
	struct vrpc_t			*fe2be_vrpc;	/* vRPC link */
	struct vrpc_t			*be2fe_vrpc;	/* vRPC link */
	int				client_is_open;
#ifdef MC_USE_VLX_PMEM
	void				*pmem_vaddr;
#endif
};

const struct tee_vlx_vm_info *tee_vlx_find_vm_info(NkOsId vm_no);
struct tee_vlx_fe *tee_vlx_fe_create(const struct tee_vlx_vm_info *vm_info);
void tee_vlx_fe_put(struct tee_vlx_fe *vlx_fe);
void tee_vlx_fe_cleanup(struct tee_vlx_fe *vlx_fe);

#endif /* MC_VLX_COMMON_H */
