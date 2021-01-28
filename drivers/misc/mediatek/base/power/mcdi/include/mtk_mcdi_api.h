/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef __MTK_MCDI_COMMON_H__
#define __MTK_MCDI_COMMON_H__

enum {
	SYSTEM_IDLE_HINT_USER_MCDI_TEST = 0,
	SYSTEM_IDLE_HINT_USER_BLUE_TOOTH,
	SYSTEM_IDLE_HINT_USER_AUDIO,
	NF_SYSTEM_IDLE_HINT
};

enum {
	MCDI_PAUSE_BY_HOTPLUG = 1,
	MCDI_PAUSE_BY_EEM,
	MCDI_PAUSE_BY_GZ,

	NF_MCDI_PAUSE
};

void __attribute__((weak))
mcdi_cpu_iso_mask(unsigned int iso_mask)
{

}

bool __attribute__((weak))
mcdi_task_pause(bool paused)
{
	return true;
}

bool __attribute__((weak))
system_idle_hint_request(unsigned int id, bool value)
{
	return false;
}

void __attribute__((weak))
mcdi_pause(unsigned int id, bool paused)
{
}
#endif /* __MTK_MCDI_COMMON_H__ */
