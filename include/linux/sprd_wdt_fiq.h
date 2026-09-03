// SPDX-License-Identifier: GPL-2.0-only
/*
 * sprd_wdt_fiq.h - Unisoc platform header 
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

#ifndef __SPRD_WDT_FIQ_H__
#define __SPRD_WDT_FIQ_H__

#include <linux/watchdog.h>

int sprd_wdt_fiq_get_dev(struct watchdog_device **wdd);

#endif
