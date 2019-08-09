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
#include <linux/types.h>

#include <mtk_mcdi.h>
#include <mtk_mcdi_util.h>
#include <mtk_mcdi_governor.h>
#include <mtk_mcdi_governor_hint.h>

void mcdi_cpu_iso_mask(unsigned int iso_mask)
{
	if (mcdi_is_cpc_mode())
		mcdi_set_cpu_iso_smc(iso_mask);
	else
		mcdi_set_cpu_iso_mbox(iso_mask);
}

bool mcdi_task_pause(bool paused)
{
	if (mcdi_is_cpc_mode())
		return true;

	return _mcdi_task_pause(paused);
}

bool system_idle_hint_request(unsigned int id, bool value)
{
	return _system_idle_hint_request(id, value);
}

void mcdi_pause(unsigned int id, bool paused)
{
	__mcdi_pause(id, paused);
}

