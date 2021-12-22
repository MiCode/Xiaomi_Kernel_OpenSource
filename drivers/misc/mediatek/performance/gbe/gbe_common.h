/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2021 MediaTek Inc.
 */

#ifndef GBE_H
#define GBE_H
enum GBE_KICKER {
	KIR_GBE1,
	KIR_GBE2,
	KIR_NUM,
};
void gbe_boost(enum GBE_KICKER kicker, int boost);
void gbe_trace_printk(int pid, char *module, char *string);
void gbe_trace_count(int tid, unsigned long long bufID,
	int val, const char *fmt, ...);
extern void (*gbe_get_cmd_fp)(int *cmd, int *value1, int *value2);

extern struct dentry *gbe_debugfs_dir;
#endif

