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

#ifndef _STP_DEBUG_COMBO_H_
#define _STP_DEBUG_COMBO_H_

#include <linux/time.h>
#include "osal.h"

typedef enum {
	COMBO_TASK_ID_GEN2 = 0,
	COMBO_TASK_ID_GEN3,
	COMBO_TASK_ID_INDX_MAX,
} COMBO_TAKS_ID_INDX_T;

typedef enum {
	COMBO_GEN2_TASK_WMT = 0,
	COMBO_GEN2_TASK_BT,
	COMBO_GEN2_TASK_WIFI,
	COMBO_GEN2_TASK_TST,
	COMBO_GEN2_TASK_FM,
	COMBO_GEN2_TASK_IDLE,
	COMBO_GEN2_TASK_DRVSTP,
	COMBO_GEN2_TASK_SDIO,
	COMBO_GEN2_TASK_NATBT,
	COMBO_GEN2_TASK_ID_MAX,
} COMBO_GEN2_TAKS_ID_T;

typedef enum {
	COMBO_GEN3_TASK_WMT = 0,
	COMBO_GEN3_TASK_BT,
	COMBO_GEN3_TASK_WIFI,
	COMBO_GEN3_TASK_TST,
	COMBO_GEN3_TASK_FM,
	COMBO_GEN3_TASK_GPS,
	COMBO_GEN3_TASK_FLP,
	COMBO_GEN3_TASK_BAL,
	COMBO_GEN3_TASK_IDLE,
	COMBO_GEN3_TASK_DRVSTP,
	COMBO_GEN3_TASK_SDIO,
	COMBO_GEN3_TASK_NATBT,
	COMBO_GEN3_TASK_ID_MAX,
} COMBO_GEN3_TAKS_ID_T;

INT32 stp_dbg_combo_core_dump(INT32 dump_sink);
PUINT8 stp_dbg_combo_id_to_task(UINT32 id);

#endif /* end of _STP_DEBUG_COMBO_H_ */
