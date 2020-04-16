/*
 * Copyright (c) 2018 MediaTek Inc.
 * Author: Cheng-Jung Ho <cheng-jung.ho@mediatek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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
