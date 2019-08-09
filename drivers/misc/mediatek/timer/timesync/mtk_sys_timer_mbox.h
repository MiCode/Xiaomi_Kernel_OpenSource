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

#ifndef __MTK_SYS_TIMER_MBOX_H__
#define __MTK_SYS_TIMER_MBOX_H__

#include <sspm_ipi_mbox_layout.h>

#define SYS_TIMER_MBOX                      SHAREMBOX_NO_MCDI
#define SYS_TIMER_MBOX_OFFSET_BASE          SHAREMBOX_OFFSET_TIMESTAMP

/*
 * Shared MBOX: AP write, SSPM read
 * Unit for each offset: 4 bytes
 */

#define SYS_TIMER_MBOX_TICK_H               (SYS_TIMER_MBOX_OFFSET_BASE + 0)
#define SYS_TIMER_MBOX_TICK_L               (SYS_TIMER_MBOX_OFFSET_BASE + 1)
#define SYS_TIMER_MBOX_TS_H                 (SYS_TIMER_MBOX_OFFSET_BASE + 2)
#define SYS_TIMER_MBOX_TS_L                 (SYS_TIMER_MBOX_OFFSET_BASE + 3)
#define SYS_TIMER_MBOX_DEBUG_TS_H           (SYS_TIMER_MBOX_OFFSET_BASE + 4)
#define SYS_TIMER_MBOX_DEBUG_TS_L           (SYS_TIMER_MBOX_OFFSET_BASE + 5)

#endif /* __MTK_SYS_TIMER_MBOX_H__ */

