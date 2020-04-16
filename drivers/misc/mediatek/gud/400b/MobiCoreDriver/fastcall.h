/*
 * Copyright (c) 2013-2015 TRUSTONIC LIMITED
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

#ifndef _TBASE_FASTCALL_H_
#define _TBASE_FASTCALL_H_

/* Use the arch_extension sec pseudo op before switching to secure world */
#if defined(__GNUC__) && \
	defined(__GNUC_MINOR__) && \
	defined(__GNUC_PATCHLEVEL__) && \
	((__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__)) \
	>= 40502
#ifndef CONFIG_ARM64
#define MC_ARCH_EXTENSION_SEC
#endif
#endif

int mc_fc_init(uintptr_t base_pa, ptrdiff_t off, size_t q_len, size_t buf_len);
int mc_fc_info(u32 ext_info_id, u32 *state, u32 *ext_info);
int mc_fc_mem_trace(phys_addr_t buffer, u32 size);
int mc_fc_nsiq(void);
int mc_fc_yield(void);

int mc_fastcall_init(void);
void mc_fastcall_exit(void);

int mc_fastcall_debug_smclog(struct kasnprintf_buf *buf);

#endif /* _TBASE_FASTCALL_H_ */
