/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __APUSYS_CORE_H__
#define __APUSYS_CORE_H__
struct apusys_core_info {
	struct dentry *dbg_root;
};
/* declare init/exit func at other module */
int apusys_mdw_init(struct apusys_core_info *info);
void apusys_mdw_exit(void);
int sample_init(struct apusys_core_info *info);
void sample_exit(void);
int edma_init(struct apusys_core_info *info);
void edma_exit(void);
int mnoc_init(struct apusys_core_info *info);
void mnoc_exit(void);
/*
 * init function at other modulses
 * call init function in order at apusys.ko init stage
 */
static int (*apusys_init_func[])(struct apusys_core_info *) = {
	apusys_mdw_init,
	sample_init,
	edma_init,
	mnoc_init,
};

/*
 * exit function at other modulses
 * call exit function in order at apusys.ko exit stage
 */
static void (*apusys_exit_func[])(void) = {
	mnoc_exit,
	edma_exit,
	sample_exit,
	apusys_mdw_exit,
};
#endif
