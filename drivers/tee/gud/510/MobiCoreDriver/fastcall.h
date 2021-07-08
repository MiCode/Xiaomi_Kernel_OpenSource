/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2019 TRUSTONIC LIMITED
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

#ifndef TBASE_FASTCALL_H
#define TBASE_FASTCALL_H

struct fc_s_yield {
	u32	resp;
	u32	ret;
	u32	code;
};

int fc_init(uintptr_t base_pa, ptrdiff_t off, size_t q_len, size_t buf_len);
int fc_info(u32 ext_info_id, u32 *state, u32 *ext_info);
int fc_trace_init(phys_addr_t buffer, u32 size);
int fc_trace_set_level(u32 level);
int fc_trace_deinit(void);
int fc_nsiq(u32 session_id, u32 payload);
int fc_yield(u32 session_id, u32 payload, struct fc_s_yield *resp);
int fc_cpu_off(void);

int mc_fastcall_debug_smclog(struct kasnprintf_buf *buf);

#endif /* TBASE_FASTCALL_H */
