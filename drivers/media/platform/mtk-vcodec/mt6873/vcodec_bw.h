// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __VCODEC_BW_H__
#define __VCODEC_BW_H__

#include <linux/types.h>
#include <linux/slab.h>

#define TYPE_CNT 3

struct vcodec_bw {
	int id;
	unsigned int smi_bw_mon[TYPE_CNT];
	struct vcodec_bw *next;
};

bool validate_bw(struct vcodec_bw *bw, int type);
struct vcodec_bw *find_bw_by_id(struct vcodec_bw *bw_list, int id);
struct vcodec_bw *add_bw_by_id(struct vcodec_bw **bw_list, int id);
struct vcodec_bw *remove_bw_by_id(struct vcodec_bw **bw_list, int id);
void free_all_bw(struct vcodec_bw **bw_list);
#endif
