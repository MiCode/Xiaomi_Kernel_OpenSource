/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */
#ifndef __APU_TOP_ENTRY_H__
#define __APU_TOP_ENTRY_H__

#include "apusys_core.h"

extern int g_apupw_drv_ver;

/* for init/exit apusys power 2.5 */
int apu_power_init(void);
void apu_power_exit(void);
int apupw_dbg_init(struct apusys_core_info *info);
void apupw_dbg_exit(void);

/* for init/exit apusys power 3.0 */
int apu_top_3_init(void);
void apu_top_3_exit(void);
int aputop_dbg_init(struct apusys_core_info *info);
void aputop_dbg_exit(void);

#endif
