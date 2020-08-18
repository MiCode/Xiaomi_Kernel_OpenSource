/*
 * Copyright (C) 2017 MediaTek Inc.
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

#include <linux/string.h>

#include "mmdvfs_plat.h"

inline void mmdvfs_update_qos_sram(struct mm_larb_request larb_req[],
	u32 larb_update)
{ return; }

inline void mmdvfs_update_limit_config(enum mmdvfs_limit_source source,
	u32 source_value, u32 *limit_value, u32 *limit_level)
{ return; }

bool mmdvfs_log_larb_mmp(s32 common_port_id, s32 larb_id)
{ return false; }

/* Return port number of CCU on SMI common */
inline u32 mmdvfs_get_ccu_smi_common_port(u32 master_id)
{ return 0; }

inline s32 get_ccu_hrt_bw(struct mm_larb_request larb_req[])
{ return 0; }

inline s32 get_md_hrt_bw(void)
{ return 0; }

inline s32 dram_write_weight(s32 val)
{ return val; }
