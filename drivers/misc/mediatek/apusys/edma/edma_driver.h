/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author: JB Tsai <jb.tsai@mediatek.com>
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

#ifndef __EDMA_DRIVER_H__
#define __EDMA_DRIVER_H__

#include "apusys_device.h"

#define DEBUG

#if 0
#define edma_debug(mask, ...) do { if (edma_klog & mask) \
		pr_debug(__VA_ARGS__); \
	} while (0)

#endif

#define EDMA_SUB_NUM 2
#define EDMA_SUB_NAME_SIZE 20
#define CMD_WAIT_TIME_MS	(3 * 1000)

struct edma_device;

enum edma_sub_state {
	EDMA_UNPREPARE,
	EDMA_IDLE,
	EDMA_ACTIVE,
	EDMA_SLEEP,
};

enum edma_power_state {
	EDMA_POWER_OFF,
	EDMA_POWER_ON,
};

struct edma_sub {
	struct device *dev;
	struct apusys_device adev;
	u32 sub;
	struct edma_device *edma_device;

	void __iomem *base_addr;

	/* edma irq operation info */
	struct mutex cmd_mutex;
	wait_queue_head_t cmd_wait;
	bool is_cmd_done;

	enum edma_sub_state state;
	enum edma_power_state power_state;

	struct task_struct *enque_task;
	u8 sub_name[EDMA_SUB_NAME_SIZE];
	uint32_t ip_time;
};

struct edma_device {
	struct device *dev;
	struct edma_sub *edma_sub[EDMA_SUB_NUM];
	struct timer_list power_timer;

	unsigned int edma_sub_num;

	/* notify enque thread */
	wait_queue_head_t req_wait;

	/* to check user list must have mutex protection */
	struct mutex user_mutex;
	struct mutex power_mutex;
	struct list_head user_list;
	int edma_num_users;
	enum edma_power_state power_state;
	struct work_struct power_off_work;

	dev_t edma_devt;
	struct cdev edma_chardev;
	struct dentry *debug_root;

	unsigned int dbgfs_reg_core;
	unsigned int dbg_cfg;
};

struct edma_user {
	pid_t open_pid;
	pid_t open_tgid;
	u32 id;
	struct device *dev;
	struct mutex data_mutex;
	bool running;
	bool flush;
	struct list_head enque_list;
	struct list_head deque_list;
	wait_queue_head_t deque_wait;
};

struct descriptor {
	u32 src_tile_channel;
	u32 src_tile_width;
	u32 src_tile_height;
	u32 src_channel_stride;
	u32 src_uv_channel_stride;
	u32 src_width_stride;
	u32 src_uv_width_stride;
	u32 dst_tile_channel;
	u32 dst_tile_width;
	u32 dst_channel_stride;
	u32 dst_uv_channel_stride;
	u32 dst_width_stride;
	u32 dst_uv_width_stride;
	u32 src_addr;
	u32 src_uv_addr;
	u32 dst_addr;
	u32 dst_uv_addr;
	u32 range_scale;
	u32 min_fp32;
	u32 param_a;
	u32 param_m;
	u32 cmprs_src_pxl;
	u32 cmprs_dst_pxl;
	u32 src_c_stride_pxl;
	u32 src_w_stride_pxl;
	u32 src_c_offset_m1;
	u32 src_w_offset_m1;
	u32 dst_c_stride_pxl;
	u32 dst_w_stride_pxl;
	u32 dst_c_offset_m1;
	u32 dst_w_offset_m1;
	u8  in_format;
	u8  out_format;
	u8  yuv2rgb_mat_bypass;
	u8  rgb2yuv_mat_bypass;
	u8  yuv2rgb_mat_select;
	u8  rgb2yuv_mat_select;
	u8  plane_num;
	u8  unpack_shift;
	u8  bit_num;
};

struct edma_request {
	u64 handle;
	u32 cmd;
	u32 sub;
	struct descriptor desp;
	u32 fill_value;
	u32 ext_reg_addr;
	u32 ext_count;
	u8  buf_iommu_en;
	u8  desp_iommu_en;
	s32 cmd_result;
	u32 cmd_status;
};

long edma_ioctl(struct file *flip, unsigned int cmd, unsigned long arg);
int edma_initialize(struct edma_device *edma_device);

#endif /* __EDMA_DRIVER_H__ */
