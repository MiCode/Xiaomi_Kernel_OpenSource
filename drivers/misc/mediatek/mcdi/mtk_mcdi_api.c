// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2017 MediaTek Inc.
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

