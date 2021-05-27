// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef GBE2_H
#define GBE2_H

void fpsgo_comp2gbe_frame_update(int pid, unsigned long long bufID);
extern void gbe2_exit(void);

extern int gbe2_init(void);

#endif

