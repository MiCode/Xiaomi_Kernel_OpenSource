/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef _STP_DEBUG_SOC_H_
#define _STP_DEBUG_SOC_H_

#include <linux/time.h>
#include "osal.h"
#include "wmt_plat.h"

#define STP_DBG_PAGED_DUMP_BUFFER_SIZE (32*1024*sizeof(char))

INT32 stp_dbg_soc_core_dump(INT32 dump_sink);
PUINT8 stp_dbg_soc_id_to_task(UINT32 id);
UINT32 stp_dbg_soc_read_debug_crs(ENUM_CONNSYS_DEBUG_CR cr);
INT32 stp_dbg_soc_poll_cpupcr(UINT32 times, UINT32 sleep, UINT32 cmd);

#endif /* end of _STP_DEBUG_SOC_H_ */
