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
#include <stdbool.h>
#include <mtk_mcdi.h>
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_governor_hint.h>

void mcdi_cpu_iso_mask(unsigned int iso_mask)
{
	_mcdi_cpu_iso_mask(iso_mask);
}

bool mcdi_task_pause(bool paused)
{
	return _mcdi_task_pause(paused);
}

bool system_idle_hint_request(unsigned int id, bool value)
{
	return _system_idle_hint_request(id, value);
}

bool mcdi_is_buck_off(int cluster_idx)
{
	return _mcdi_is_buck_off(cluster_idx);
}

void mcdi_pause(unsigned int id, bool paused)
{
	__mcdi_pause(id, paused);
}

