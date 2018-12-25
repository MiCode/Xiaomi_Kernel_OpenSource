/*
 * Copyright (C) 2015 MediaTek Inc.
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

#ifndef __SMI_DEBUG_H__
#define __SMI_DEBUG_H__

#include "smi_configuration.h"
#define SMI_DBG_LARB_SELECT(smi_dbg_larb, n) ((smi_dbg_larb) & (1<<n))

void smi_dumpCommonDebugMsg(int gce_buffer);
void smi_dumpLarbDebugMsg(unsigned int larb_indx, int gce_buffer);
void smi_dumpDebugMsg(void);
void smi_dump_larb_m4u_register(unsigned int larb_indx);
int smi_debug_bus_hanging_detect(unsigned short larbs, int show_dump,
	int gce_buffer, int enable_m4u_reg_dump);
#endif /* __SMI_DEBUG_H__ */
