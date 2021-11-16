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

#ifndef __APUSYS_MDW_TAG_H__
#define __APUSYS_MDW_TAG_H__

#include "mdw_rsc.h"
#define MDW_TAGS_CNT (3000)

/* The tag entry of VPU */
struct mdw_tag {
	int type;

	union mdw_tag_data {
		struct mdw_tag_cmd {
			uint32_t done;
			pid_t pid;
			pid_t tgid;
			uint64_t uid;
			uint64_t cmd_id;
			int sc_idx;
			uint32_t num_sc;
			int type;
			char dev_name[MDW_DEV_NAME_SIZE];
			int dev_idx;
			uint32_t pack_id;
			uint32_t multicore_idx;
			uint32_t exec_core_num;
			uint64_t exec_core_bitmap;
			unsigned char priority;
			uint32_t soft_limit;
			uint32_t hard_limit;
			uint32_t exec_time;
			uint32_t suggest_time;
			unsigned char power_save;
			uint32_t ctx_id;
			unsigned char tcm_force;
			uint32_t tcm_usage;
			uint32_t tcm_real_usage;
			uint32_t boost;
			uint32_t ip_time;
			int ret;
		} cmd;
	} d;
};

#ifdef CONFIG_MTK_APUSYS_DEBUG
int mdw_tag_init(void);
void mdw_tag_exit(void);
void mdw_tag_show(struct seq_file *s);
#else
static inline int mdw_tag_init(void)
{
	return 0;
}

static inline void mdw_tag_exit(void)
{
}
static inline void mdw_tag_show(struct seq_file *s)
{
}
#endif

#endif

