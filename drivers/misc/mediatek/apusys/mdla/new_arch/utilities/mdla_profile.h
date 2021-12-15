/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#ifndef __MDLA_PROFILE_H__
#define __MDLA_PROFILE_H__

#include <linux/types.h>


#define DBGFS_PROF_NAME_V1 "prof"
#define DBGFS_PROF_NAME_V2 "profile"

enum PROF_MODE {
	PROF_V1,
	PROF_V2,

	PROF_NONE = 0xff
};

enum PROF_TS {
	TS_CMD_START,
	TS_CMD_STOP_REQ,
	TS_CMD_STOPPED,
	TS_CMD_RESUME,
	TS_HW_FIRST_TRIGGER,
	TS_HW_TRIGGER,
	TS_HW_INTR,
	TS_HW_LAST_INTR,
	TS_CMD_FINISH,

	NR_PROF_TS
};

void mdla_prof_start(u32 core_id);
void mdla_prof_stop(u32 core_id, int wait);
void mdla_prof_iter(u32 core_id);

bool mdla_prof_pmu_timer_is_running(u32 core_id);
bool mdla_prof_use_dbgfs_pmu_event(u32 core_id);

void mdla_prof_set_ts(u32 core_id, int index, u64 time_ns);
u64 mdla_prof_get_ts(u32 core_id, int index);
void mdla_prof_ts(u32 core_id, int idx);

void mdla_prof_init(int mode);
void mdla_prof_deinit(void);

#endif /* __MDLA_PROFILE_H__ */

