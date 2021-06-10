/*
 * Copyright (C) 2019 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __VPU_DEBUG_H__
#define __VPU_DEBUG_H__

#include <linux/printk.h>
#include "vpu_cmn.h"

enum VPU_DEBUG_MASK {
	VPU_DBG_DRV = 0x01,
	VPU_DBG_MEM = 0x02,
	VPU_DBG_ALG = 0x04,
	VPU_DBG_CMD = 0x08,
	VPU_DBG_PWR = 0x10,
	VPU_DBG_PEF = 0x20,
	VPU_DBG_MET = 0x40,
};

enum message_level {
	VPU_DBG_MSG_LEVEL_NONE,
	VPU_DBG_MSG_LEVEL_CTRL,
	VPU_DBG_MSG_LEVEL_CTX,
	VPU_DBG_MSG_LEVEL_INFO,
	VPU_DBG_MSG_LEVEL_DEBUG,
	VPU_DBG_MSG_LEVEL_TOTAL,
};

struct vpu_message_ctrl {
	unsigned int mutex;
	int head;
	int tail;
	int buf_size;
	unsigned int level_mask;
	unsigned int data;
};

static struct vpu_message_ctrl *vpu_mesg(struct vpu_device *vd)
{
	if (!vd || !vd->iova_work.m.va)
		return NULL;

	return (struct vpu_message_ctrl *)(vd->iova_work.m.va +
		VPU_LOG_OFFSET + VPU_LOG_HEADER_SIZE);
}

static inline
void vpu_mesg_init(struct vpu_device *vd)
{
	struct vpu_message_ctrl *msg = vpu_mesg(vd);

	if (!msg)
		return;
	memset(msg, 0, vd->wb_log_data);
	msg->level_mask = (1 << VPU_DBG_MSG_LEVEL_CTRL);
	vpu_iova_sync_for_device(vd->dev, &vd->iova_work);
}

extern u32 vpu_klog;

static inline
int vpu_debug_on(int mask)
{
	return (vpu_klog & mask);
}

#define vpu_debug(mask, ...) do { if (vpu_debug_on(mask)) \
		pr_info(__VA_ARGS__); \
	} while (0)

int vpu_init_debug(void);
void vpu_exit_debug(void);
int vpu_init_dev_debug(struct platform_device *pdev, struct vpu_device *vd);
void vpu_exit_dev_debug(struct platform_device *pdev, struct vpu_device *vd);
int vpu_mesg_seq(struct seq_file *s, struct vpu_device *vd);
int vpu_debug_state_seq(struct seq_file *s, uint32_t vs, uint32_t ds, int b);
int vpu_debug_cmd_seq(struct seq_file *s, struct vpu_device *vd, int prio,
	int prio_max, int active, struct vpu_cmd_ctl *c, uint64_t timeout);
const char *vpu_debug_cmd_str(int cmd);
void vpu_seq_time(struct seq_file *s, uint64_t t);
void vpu_seq_boost(struct seq_file *s, int boost);

#define vpu_drv_debug(...) vpu_debug(VPU_DBG_DRV, __VA_ARGS__)
#define vpu_mem_debug(...) vpu_debug(VPU_DBG_MEM, __VA_ARGS__)
#define vpu_cmd_debug(...) vpu_debug(VPU_DBG_CMD, __VA_ARGS__)
#define vpu_alg_debug(...) vpu_debug(VPU_DBG_ALG, __VA_ARGS__)
#define vpu_pwr_debug(...) vpu_debug(VPU_DBG_PWR, __VA_ARGS__)
#define vpu_pef_debug(...) vpu_debug(VPU_DBG_PEF, __VA_ARGS__)
#define vpu_met_debug(...) vpu_debug(VPU_DBG_MET, __VA_ARGS__)

#endif

