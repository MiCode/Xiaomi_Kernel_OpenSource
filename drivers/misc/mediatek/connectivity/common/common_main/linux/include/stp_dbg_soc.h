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

typedef enum {
	SOC_TASK_ID_GEN2 = 0,
	SOC_TASK_ID_GEN3,
	SOC_TASK_ID_INDX_MAX,
} SOC_TAKS_ID_INDX_T;

typedef enum {
	SOC_GEN2_TASK_WMT = 0,
	SOC_GEN2_TASK_BT,
	SOC_GEN2_TASK_WIFI,
	SOC_GEN2_TASK_TST,
	SOC_GEN2_TASK_FM,
	SOC_GEN2_TASK_IDLE,
	SOC_GEN2_TASK_DRVSTP,
	SOC_GEN2_TASK_BTIF,
	SOC_GEN2_TASK_NATBT,
	SOC_GEN2_TASK_ID_MAX,
} GEN2_TAKS_ID_T;

typedef enum {
	SOC_GEN3_TASK_WMT = 0,
	SOC_GEN3_TASK_BT,
	SOC_GEN3_TASK_WIFI,
	SOC_GEN3_TASK_TST,
	SOC_GEN3_TASK_FM,
	SOC_GEN3_TASK_GPS,
	SOC_GEN3_TASK_FLP,
	SOC_GEN3_TASK_NULL,
	SOC_GEN3_TASK_IDLE,
	SOC_GEN3_TASK_DRVSTP,
	SOC_GEN3_TASK_BTIF,
	SOC_GEN3_TASK_NATBT,
	SOC_GEN3_TASK_DRVWIFI,
	SOC_GEN3_TASK_ID_MAX,
} SOC_GEN3_TAKS_ID_T;

INT32 stp_dbg_soc_core_dump(INT32 dump_sink);
PUINT8 stp_dbg_soc_id_to_task(UINT32 id);
UINT32 stp_dbg_soc_read_debug_crs(ENUM_CONNSYS_DEBUG_CR cr);
INT32 stp_dbg_soc_poll_cpupcr(UINT32 times, UINT32 sleep, UINT32 cmd);

#endif /* end of _STP_DEBUG_SOC_H_ */
