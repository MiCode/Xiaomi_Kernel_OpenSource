/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef _GPS_DL_LINUX_RESERVED_MEM_H
#define _GPS_DL_LINUX_RESERVED_MEM_H

#include "gps_dl_config.h"

#ifdef GPS_DL_HAS_PLAT_DRV
#include "gps_dl_dma_buf.h"

extern phys_addr_t gGpsRsvMemPhyBase;
extern unsigned long long gGpsRsvMemSize;

void gps_dl_reserved_mem_init(void);
void gps_dl_reserved_mem_deinit(void);
bool gps_dl_reserved_mem_is_ready(void);
void gps_dl_reserved_mem_get_range(unsigned int *p_min, unsigned int *p_max);
void gps_dl_reserved_mem_show_info(void);

void gps_dl_reserved_mem_dma_buf_init(struct gps_dl_dma_buf *p_dma_buf,
	enum gps_dl_link_id_enum link_id, enum gps_dl_dma_dir dir, unsigned int len);
void gps_dl_reserved_mem_dma_buf_deinit(struct gps_dl_dma_buf *p_dma_buf);
void *gps_dl_reserved_mem_icap_buf_get_vir_addr(void);
#endif

#endif /* _GPS_DL_LINUX_RESERVED_MEM_H */

