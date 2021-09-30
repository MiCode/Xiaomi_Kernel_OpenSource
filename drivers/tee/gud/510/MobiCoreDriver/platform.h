/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2013-2017,2019 TRUSTONIC LIMITED
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#ifndef MC_DRV_PLATFORM_H
#define MC_DRV_PLATFORM_H

/* Needed for MTK6779 */
#define MC_INTR_SSIQ_SWD       121 /* 89(From Linux DTS) + 32(SPI+PPI) */

/* Ensure consistency for Fastcall ID between NWd and TEE*/
#define MC_AARCH32_FC

/* Enable Fastcall worker thread */
#define MC_FASTCALL_WORKER_THREAD

/* For retrieving SSIQ from dts */
#define MC_DEVICE_PROPNAME	"trustonic,mobicore"

#define MC_DISABLE_IRQ_WAKEUP /* Failing on this platform */

#define MC_BIG_CORE	0x7

#endif /* MC_DRV_PLATFORM_H */
