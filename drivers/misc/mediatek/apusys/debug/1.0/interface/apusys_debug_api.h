/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef __APUSYS_DEBUG_API_H__
#define __APUSYS_DEBUG_API_H__


void apusys_reg_dump_skip_gals(int onoff);
void apusys_reg_dump(char *module_name, bool dump_vpu);

#endif
