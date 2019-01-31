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

#ifndef __SMI_COMMON_H__
#define __SMI_COMMON_H__

#define SMI_CLIENT_DISP 0
#define SMI_CLIENT_WFD 1
#define SMI_EVENT_DIRECT_LINK  (0x1 << 0)
#define SMI_EVENT_DECOUPLE     (0x1 << 1)
#define SMI_EVENT_OVL_CASCADE  (0x1 << 2)
#define SMI_EVENT_OVL1_EXTERNAL  (0x1 << 3)

extern char *smi_port_name[][21];
/* for slow motion force 30 fps */
extern int primary_display_force_set_vsync_fps(unsigned int fps);
extern unsigned int primary_display_get_fps(void);
extern void smi_dumpDebugMsg(void);
extern void smi_client_status_change_notify(int module, int mode);
extern void SMI_DBG_Init(void);
void register_base_dump(void);
#endif
