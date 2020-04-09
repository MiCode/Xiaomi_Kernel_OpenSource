/*
 * Copyright (C) 2020 MediaTek Inc.
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

#ifndef __VPU_TAG_H__
#define __VPU_TAG_H__

#include "vpu_ioctl.h"
#include "vpu_cfg.h"

#define STAGE_NAMELEN (32)

/* The tag entry of VPU */
struct vpu_tag {
	int type;
	int core;

	union vpu_tag_data {
		struct vpu_tag_cmd {
			int prio;
			char algo[ALGO_NAMELEN];
			uint64_t start_time;
			int boost;
			int cmd;
			int ret;
			int algo_ret;
			int result;
		} cmd;
		struct vpu_tag_dmp {
			char stage[STAGE_NAMELEN];
			uint32_t pc;
		} dmp;
		struct vpu_tag_wait {
			uint32_t donest;
			uint32_t info00;
			uint32_t info25;
			uint32_t pc;
		} wait;
	} d;
};

#ifdef CONFIG_MTK_APUSYS_VPU_DEBUG
int vpu_init_drv_tags(void);
void vpu_exit_drv_tags(void);
#else
static inline int vpu_init_drv_tags(void)
{
	return 0;
}

static inline void vpu_exit_drv_tags(void)
{
}
#endif

#endif

