/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */
#ifndef _GPS_DL_DMA_BUF_H
#define _GPS_DL_DMA_BUF_H

#include "gps_dl_config.h"

#if GPS_DL_ON_LINUX
#include "linux/semaphore.h"
#include "linux/dma-mapping.h"
#elif GPS_DL_ON_CTP
#include "kernel_to_ctp.h"
#include "gps_dl_ctp_osal.h"
#endif

#include "gps_dl_base.h"

enum gps_dl_dma_dir {
	GDL_DMA_A2D,
	GDL_DMA_D2A,
	GDL_DMA_DIR_NUM
};

/* for lock free structure */
struct gdl_dma_buf_idx {
	unsigned int rd_idx;
	unsigned int wr_idx;
};

#if GPS_DL_ON_LINUX
struct gdl_dma_lock {
	struct semaphore internal_lock;
};

enum GDL_RET_STATUS gdl_dma_lock_init(struct gdl_dma_lock *p_lock);
enum GDL_RET_STATUS gdl_dma_lock_take(struct gdl_dma_lock *p_lock);
enum GDL_RET_STATUS gdl_dma_lock_give(struct gdl_dma_lock *p_lock);
void gdl_dma_lock_deinit(struct gdl_dma_lock *p_lock);
#endif

struct gdl_dma_buf_entry {
	void *vir_addr;
#if GPS_DL_ON_LINUX
	dma_addr_t phy_addr;
#else
	unsigned int phy_addr;
#endif
	unsigned int read_index;
	unsigned int write_index;
	unsigned int buf_length;
	bool is_valid;
	bool is_nodata;
};

#if GPS_DL_ON_LINUX
/* if set to 2, it likes not use multi entry */
#define GPS_DL_DMA_BUF_ENTRY_MAX (2)
#else
#define GPS_DL_DMA_BUF_ENTRY_MAX (4)
#endif
struct gps_dl_dma_buf {
	int dev_index;
	enum gps_dl_dma_dir dir;
	unsigned int len;

	void *vir_addr;
#if GPS_DL_ON_LINUX
	dma_addr_t phy_addr;
#else
	unsigned int phy_addr;
#endif
	unsigned int read_index;
	unsigned int write_index;
	unsigned int transfer_max;
	bool writer_working;
	bool reader_working;

	/* TODO: better way is put it to LINK rather than dma_buf */
	bool has_pending_rx;

	struct gdl_dma_buf_entry dma_working_entry;
	struct gdl_dma_buf_entry data_entries[GPS_DL_DMA_BUF_ENTRY_MAX];
	unsigned int entry_r;
	unsigned int entry_w;

#if 0
	struct gdl_dma_buf_idx reader;
	struct gdl_dma_buf_idx writer;
	struct gdl_dma_lock lock;
#endif
};


struct gdl_hw_dma_transfer {
	unsigned int buf_start_addr;
	unsigned int transfer_start_addr;
	unsigned int len_to_wrap;
	unsigned int transfer_max_len;
};

int gps_dl_dma_buf_alloc(struct gps_dl_dma_buf *p_dma_buf, enum gps_dl_link_id_enum link_id,
	enum gps_dl_dma_dir dir, unsigned int len);
void gps_dma_buf_reset(struct gps_dl_dma_buf *p_dma);
void gps_dma_buf_show(struct gps_dl_dma_buf *p_dma, bool is_warning);
void gps_dma_buf_align_as_byte_mode(struct gps_dl_dma_buf *p_dma);
bool gps_dma_buf_is_empty(struct gps_dl_dma_buf *p_dma);

/* enum GDL_RET_STATUS gdl_dma_buf_init(struct gps_dl_dma_buf *p_dma); */
/* enum GDL_RET_STATUS gdl_dma_buf_deinit(struct gps_dl_dma_buf *p_dma); */

enum GDL_RET_STATUS gdl_dma_buf_put(struct gps_dl_dma_buf *p_dma,
	const unsigned char *p_buf, unsigned int buf_len);

enum GDL_RET_STATUS gdl_dma_buf_get(struct gps_dl_dma_buf *p_dma,
	unsigned char *p_buf, unsigned int buf_len, unsigned int *p_data_len,
	bool *p_is_nodata);


enum GDL_RET_STATUS gdl_dma_buf_get_data_entry(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry);

enum GDL_RET_STATUS gdl_dma_buf_set_data_entry(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry);

enum GDL_RET_STATUS gdl_dma_buf_get_free_entry(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry, bool nospace_set_pending_rx);

enum GDL_RET_STATUS gdl_dma_buf_set_free_entry(struct gps_dl_dma_buf *p_dma,
	struct gdl_dma_buf_entry *p_entry);


enum GDL_RET_STATUS gdl_dma_buf_entry_to_buf(const struct gdl_dma_buf_entry *p_entry,
	unsigned char *p_buf, unsigned int buf_len, unsigned int *p_data_len);

enum GDL_RET_STATUS gdl_dma_buf_buf_to_entry(const struct gdl_dma_buf_entry *p_entry,
	const unsigned char *p_buf, unsigned int data_len, unsigned int *p_write_index);

enum GDL_RET_STATUS gdl_dma_buf_entry_to_transfer(
	const struct gdl_dma_buf_entry *p_entry,
	struct gdl_hw_dma_transfer *p_transfer, bool is_tx);

enum GDL_RET_STATUS gdl_dma_buf_entry_transfer_left_to_write_index(
	const struct gdl_dma_buf_entry *p_entry,
	unsigned int left_len, unsigned int *p_write_index);

#endif /* _GPS_DL_DMA_BUF_H */

