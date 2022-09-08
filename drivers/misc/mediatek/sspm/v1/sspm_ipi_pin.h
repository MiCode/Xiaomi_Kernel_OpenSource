/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
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
#define IPI_ID_SMI          5
/* use bit 8 - 15 of IN_IRQ of mailbox 1 */
#define IPI_ID_MCDI         6  /* the following will use mbox 1 */
#define IPI_ID_SPM_SUSPEND  7
#define IPI_ID_PMIC         8
#define IPI_ID_PPM          9
#define IPI_ID_THERMAL      10
#define IPI_ID_UPOWER       11
#define IPI_ID_CM           12
#define IPI_ID_TOTAL        13

#define IPI_OPT_REDEF_MASK      0x1
#define IPI_OPT_LOCK_MASK       0x2
#define IPI_OPT_POLLING_MASK    0x4

#endif /* __SSPM_IPI_PIN_H__ */
