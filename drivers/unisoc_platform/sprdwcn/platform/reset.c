// SPDX-License-Identifier: GPL-2.0-only
/*
 * reset.c - Unisoc platform driver
 *
 * Copyright 2022 Unisoc(Shanghai) Technologies Co.Ltd
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

/*
 * This function include:
 * 1. register reset callback
 * 2. notify BT FM WIFI GNSS CP2 Assert
 */

#include <linux/debug_locks.h>
#include <linux/sched/debug.h>
#include <linux/interrupt.h>
#include <linux/notifier.h>
#include <linux/vt_kern.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/ratelimit.h>

#include "wcn_glb.h"

ATOMIC_NOTIFIER_HEAD(wcn_reset_notifier_list);
EXPORT_SYMBOL_GPL(wcn_reset_notifier_list);

void wcn_reset_cp2(void)
{
	wcn_chip_power_off();
	atomic_notifier_call_chain(&wcn_reset_notifier_list, 0, NULL);
}
EXPORT_SYMBOL_GPL(wcn_reset_cp2);
