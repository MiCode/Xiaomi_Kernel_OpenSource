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

#ifndef __MT8173_SMI_DEBUG_H__
#define __MT8173_SMI_DEBUG_H__

#define SMI_DBG_DISPSYS (1<<0)
#define SMI_DBG_VDEC (1<<1)
#define SMI_DBG_IMGSYS (1<<2)
#define SMI_DBG_VENC (1<<3)
#define SMI_DBG_MJC (1<<4)

#define SMI_DGB_LARB_SELECT(smi_dbg_larb, n) ((smi_dbg_larb) & (1<<n))

#ifndef CONFIG_MTK_SMI_EXT
#define smi_debug_bus_hanging_detect(larbs, show_dump) {}
#define smi_debug_bus_hanging_detect_ext(larbs, show_dump, output_gce_buffer) {}
#else
int smi_debug_bus_hanging_detect(unsigned int larbs, int show_dump);
    /* output_gce_buffer = 1, pass log to CMDQ error dumping messages */
int smi_debug_bus_hanging_detect_ext(unsigned int larbs, int show_dump, int output_gce_buffer);

#endif


#endif				/* __MT6735_SMI_DEBUG_H__ */
