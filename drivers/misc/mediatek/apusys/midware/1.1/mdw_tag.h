// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
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
			uint64_t cmd_id;
			uint64_t sc_info;
			char dev_name[MDW_DEV_NAME_SIZE];
			uint64_t multi_info;
			uint64_t exec_info;
			uint64_t tcm_info;
			uint32_t boost;
			uint32_t ip_time;
			int ret;
		} cmd;
	} d;
};

#if IS_ENABLED(CONFIG_MTK_APUSYS_DEBUG)
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

