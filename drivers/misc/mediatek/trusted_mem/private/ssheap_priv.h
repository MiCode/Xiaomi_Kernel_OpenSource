/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef SSHEAP_PRIV_H
#define SSHEAP_PRIV_H

/* prefer_align: 0 (use default alignment) */
struct ssheap_buf_info *ssheap_alloc_non_contig(u32 req_size, u32 prefer_align, u8 mem_type);

int ssheap_free_non_contig(struct ssheap_buf_info *info);

uint64_t ssheap_get_used_size(void);

unsigned long mtee_assign_buffer(struct ssheap_buf_info *info, uint8_t mem_type);
unsigned long mtee_unassign_buffer(struct ssheap_buf_info *info, uint8_t mem_type);

#endif
