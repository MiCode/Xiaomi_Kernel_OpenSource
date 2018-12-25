/*
 * Copyright (C) 2017 MediaTek Inc.
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

bool __attribute__((weak))
mcdi_is_buck_off(int cluster_idx)
{
	return false;
}

void __attribute__((weak))
mcdi_pause(unsigned int id, bool paused)
{
}
#endif /* __MTK_MCDI_COMMON_H__ */
