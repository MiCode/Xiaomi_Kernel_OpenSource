/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020 Unisoc Inc.
 */

#ifndef _VIDEO_GSP_CFG_H
#define _VIDEO_GSP_CFG_H

#include <linux/dma-buf.h>
#include <linux/dma-direction.h>
#include <linux/ioctl.h>
#include <linux/list.h>
#include <uapi/drm/gsp_cfg.h>

#define GSP_MAX_NUM 2

struct gsp_buf {
	size_t size;
	struct dma_buf *dmabuf;
	int is_iova;
};

struct gsp_buf_map {
	struct dma_buf_attachment *attachment;
	struct sg_table *table;
	enum dma_data_direction dir;
};

struct gsp_mem_data {
	int share_fd;
	u32 uv_offset;
	u32 v_offset;
	struct gsp_buf buf;
	struct gsp_buf_map map;
};

struct gsp_layer {
	int type;
	int enable;
	struct list_head list;
	int wait_fd;
	int sig_fd;

	int filled;
	struct gsp_addr_data src_addr;
	struct gsp_mem_data mem_data;
};

struct gsp_cfg {
	int layer_num;
	int init;
	int tag;
	struct list_head layers;
	struct gsp_kcfg *kcfg;
	unsigned long frame_cnt;
};

struct coef_entry {
	struct list_head list;
	u16 in_w;
	u16 in_h;
	u16 out_w;
	u16 out_h;
	u16 hor_tap;
	u16 ver_tap;
	u32 coef[64 + 64];
};

#endif
