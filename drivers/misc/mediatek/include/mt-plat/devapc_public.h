/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __DEVAPC_PUBLIC_H__
#define __DEVAPC_PUBLIC_H__

#include <linux/types.h>

enum infra_subsys_id {
	INFRA_SUBSYS_MD = 0,
	INFRA_SUBSYS_CONN,
	INFRA_SUBSYS_ADSP,
	INFRA_SUBSYS_GCE,
	INFRA_SUBSYS_APMCU,
	DEVAPC_SUBSYS_CLKMGR,
	DEVAPC_SUBSYS_TEST,
	DEVAPC_SUBSYS_RESERVED,
};

struct devapc_vio_callbacks {
	struct list_head list;
	enum infra_subsys_id id;
	void (*debug_dump)(void);
};

uint32_t devapc_vio_check(void);
void dump_dbg_info(void);
void register_devapc_vio_callback(struct devapc_vio_callbacks *viocb);

#endif  /* __DEVAPC_PUBLIC_H__ */

