// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_DEBUG_H__
#define __APUSYS_DEBUG_H__

extern bool apusys_dump_force;
extern bool apusys_dump_skip;

void apusys_dump_init(struct device *dev);
void apusys_reg_dump(void);
void apusys_dump_exit(struct device *dev);
int apusys_dump_show(struct seq_file *sfile, void *v);
void apusys_dump_reg_skip(int onoff);

#endif
