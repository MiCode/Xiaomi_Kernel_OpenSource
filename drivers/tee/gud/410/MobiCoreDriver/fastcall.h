/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 TRUSTONIC LIMITED
 */

#ifndef _TBASE_FASTCALL_H_
#define _TBASE_FASTCALL_H_

int fc_init(uintptr_t base_pa, ptrdiff_t off, size_t q_len, size_t buf_len);
int fc_info(u32 ext_info_id, u32 *state, u32 *ext_info);
int fc_trace_init(phys_addr_t buffer, u32 size);
int fc_trace_deinit(void);
int fc_nsiq(u32 session_id, u32 payload);
int fc_yield(u32 timeslice);

int mc_fastcall_debug_smclog(struct kasnprintf_buf *buf);

#endif /* _TBASE_FASTCALL_H_ */
