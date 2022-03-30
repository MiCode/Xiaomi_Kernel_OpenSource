/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
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
	INFRA_SUBSYS_GZ,
	DEVAPC_SUBSYS_CLKMGR,
	DEVAPC_SUBSYS_TEST,
	DEVAPC_SUBSYS_RESERVED,
};

enum devapc_type {
	DEVAPC_TYPE_INFRA = 0,
	DEVAPC_TYPE_INFRA1,
	DEVAPC_TYPE_PERI_PAR,
	DEVAPC_TYPE_VLP,
	DEVAPC_TYPE_ADSP,
	DEVAPC_TYPE_MMINFRA,
	DEVAPC_TYPE_MMUP,
};

enum devapc_cb_status {
	DEVAPC_OK = 0,
	DEVAPC_NOT_KE,
};

struct devapc_vio_callbacks {
	struct list_head list;
	enum infra_subsys_id id;
	void (*debug_dump)(void);
	enum devapc_cb_status (*debug_dump_adv)(uint32_t vio_addr);
};

struct devapc_power_callbacks {
	struct list_head list;
	enum devapc_type type;
	bool (*query_power)(void);
};

uint32_t devapc_vio_check(void);
void dump_dbg_info(void);
void register_devapc_vio_callback(struct devapc_vio_callbacks *viocb);
void register_devapc_power_callback(struct devapc_power_callbacks *powercb);
void devapc_catch_illegal_range(phys_addr_t phys_addr, size_t size);

#endif  /* __DEVAPC_PUBLIC_H__ */

