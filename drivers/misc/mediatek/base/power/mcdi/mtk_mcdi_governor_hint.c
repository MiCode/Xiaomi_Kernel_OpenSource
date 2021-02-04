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

#include <linux/kernel.h>
#include <linux/spinlock.h>

#include "mtk_mcdi_governor_hint.h"
#include "mtk_mcdi_api.h"

static DEFINE_SPINLOCK(system_idle_hint_spin_lock);

static unsigned int system_idle_hint;

unsigned int system_idle_hint_result_raw(void)
{
	return system_idle_hint;
}

bool system_idle_hint_result(void)
{
	return (system_idle_hint_result_raw() != 0);
}

bool _system_idle_hint_request(unsigned int id, bool value)
{
	unsigned long flags = 0;

	if (!(id >= 0 && id < NF_SYSTEM_IDLE_HINT))
		return false;

	spin_lock_irqsave(&system_idle_hint_spin_lock, flags);

	if (value == true)
		system_idle_hint |= (1 << id);
	else
		system_idle_hint &= ~(1 << id);

	spin_unlock_irqrestore(&system_idle_hint_spin_lock, flags);

	return true;
}

