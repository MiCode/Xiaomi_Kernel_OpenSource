/*
 * Copyright (C) 2016 MediaTek Inc.
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
#ifndef __SSPM_IPI_PIN_H__
#define __SSPM_IPI_PIN_H__

/* define module id here ... */
/* use bit 0 - 7 of IN_IRQ of mailbox 0 */
#define IPI_ID_PLATFORM     0  /* the following will use mbox 0 */
#define IPI_ID_CPU_DVFS     1
#define IPI_ID_QOS          2
#define IPI_ID_TST1         3
#define IPI_ID_FHCTL        4
/* use bit 8 - 15 of IN_IRQ of mailbox 1 */
#define IPI_ID_MCDI         5  /* the following will use mbox 1 */
#define IPI_ID_SPM_SUSPEND  6
#define IPI_ID_PMIC         7
#define IPI_ID_PPM          8
#define IPI_ID_THERMAL      9
#define IPI_ID_UPOWER       10
#define IPI_ID_TOTAL        11

#define IPI_OPT_REDEF_MASK      0x1
#define IPI_OPT_LOCK_MASK       0x2
#define IPI_OPT_POLLING_MASK    0x4

#endif /* __SSPM_IPI_PIN_H__ */
