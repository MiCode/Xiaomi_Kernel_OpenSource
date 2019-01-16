/*
 * Copyright (c) 2013 TRUSTONIC LIMITED
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
#ifndef _MC_OPS_H_
#define _MC_OPS_H_

#include <linux/workqueue.h>
#include "fastcall.h"

int mc_yield(void);
int mc_nsiq(void);
int _nsiq(void);
uint32_t mc_get_version(void);

int mc_info(uint32_t ext_info_id, uint32_t *state, uint32_t *ext_info);
int mc_init(phys_addr_t base, uint32_t  nq_length, uint32_t mcp_offset,
		uint32_t  mcp_length);
#ifdef TBASE_CORE_SWITCHER
int mc_switch_core(uint32_t core_num);
#endif

bool mc_fastcall(void *data);

int mc_fastcall_init(struct mc_context *context);
void mc_fastcall_destroy(void);

#endif /* _MC_OPS_H_ */
