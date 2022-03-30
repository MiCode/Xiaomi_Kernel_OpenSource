/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __MTK_APU_MDW_APTAG_H__
#define __MTK_APU_MDW_APTAG_H__

#include "mdw_rv.h"
#define MDW_TAGS_CNT (3000)

struct mdw_rv_tag {
	int type;

	union mdw_tag_data {
		struct mdw_tag_cmd {
			bool done;
			pid_t pid;
			pid_t tgid;
			uint64_t uid;
			uint64_t kid;
			uint64_t rvid;
			uint32_t num_subcmds;
			uint32_t num_cmdbufs;
			uint32_t priority;
			uint32_t softlimit;
			uint32_t pwr_dtime;
			uint64_t sc_rets;
		} cmd;
	} d;
};

#if IS_ENABLED(CONFIG_MTK_APUSYS_DEBUG)
int mdw_rv_tag_init(void);
void mdw_rv_tag_deinit(void);
void mdw_rv_tag_show(struct seq_file *s);
#else
static inline int mdw_rv_tag_init(void)
{
	return 0;
}

static inline void mdw_rv_tag_deinit(void)
{
}
static inline void mdw_rv_tag_show(struct seq_file *s)
{
}
#endif

#endif

