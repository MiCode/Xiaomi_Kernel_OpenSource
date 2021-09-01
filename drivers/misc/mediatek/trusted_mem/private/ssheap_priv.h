/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2021 MediaTek Inc.
 */

#ifndef SSHEAP_PRIV_H
#define SSHEAP_PRIV_H

/* prefer_align: 0 (use default alignment) */
struct ssheap_buf_info *ssheap_alloc_non_contig(u32 req_size, u32 prefer_align, u8 mem_type);
int ssheap_free_non_contig(struct ssheap_buf_info *info);

unsigned long mtee_assign_buffer(struct ssheap_buf_info *info, uint8_t mem_type);
unsigned long mtee_unassign_buffer(struct ssheap_buf_info *info, uint8_t mem_type);

void ssheap_set_cma_region(phys_addr_t base, phys_addr_t size);
void ssheap_set_dev(struct device *dev);

long long ssheap_get_used_size(void);
void ssheap_dump_mem_info(void);
void ssheap_enable_buddy_system(bool enable);

#if IS_ENABLED(CONFIG_TEST_MTK_TRUSTED_MEMORY)
void create_ssheap_ut_device(void);
#endif

#endif
