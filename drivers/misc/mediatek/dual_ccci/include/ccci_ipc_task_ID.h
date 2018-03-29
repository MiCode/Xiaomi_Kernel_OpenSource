/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#ifndef __CCCI_IPC_TASK_ID_H__
#define __CCCI_IPC_TASK_ID_H__
/*  Priority   -->   Local module ID -->     External ID     --> Max sent message */
/*         X_IPC_MODULE_CONF(1, M_SSDBG1, 0, 1)      TASK_ID_1 */
/*         X_IPC_MODULE_CONF(1, AP_SSDBG2, 1, 1)     TASK_ID_2 */
#ifdef __IPC_ID_TABLE
#define X_IPC_MODULE_CONF(a, b, c, d) {c, b},
#else
#define X_IPC_MODULE_CONF(a, b, c, d)
#endif
#define AP_UNIFY_ID_FLAG (1<<31)
#define MD_UNIFY_ID_FLAG (0<<31)
/* ---------------------------------------------------------- */
#define AGPS_MD_MOD_L4C 0
#define AGPS_MD_MOD_L4C_2 1
#define AGPS_MD_MOD_L4C_3 2
#define AGPS_MD_MOD_L4C_4 3
/* agps MD begin task_id */
/* Wait to add */
#define AGPS_AP_MOD_MMI 0
/* agps AP begin task_id */
/* Wait to add     */
#define GPS_AP_MOD 2
/* -------------------------------------------------------------------------- */
X_IPC_MODULE_CONF(1, AGPS_MD_MOD_L4C, MD_UNIFY_ID_FLAG | 0, 1)
X_IPC_MODULE_CONF(1, AGPS_MD_MOD_L4C_2, MD_UNIFY_ID_FLAG | 1, 1)
X_IPC_MODULE_CONF(1, AGPS_MD_MOD_L4C_3, MD_UNIFY_ID_FLAG | 2, 1)
X_IPC_MODULE_CONF(1, AGPS_MD_MOD_L4C_4, MD_UNIFY_ID_FLAG | 3, 1)
/* Wait to add  */
/* -------------------------------------------------------------------------- */
X_IPC_MODULE_CONF(1, AGPS_AP_MOD_MMI, AP_UNIFY_ID_FLAG | 0, 1)
X_IPC_MODULE_CONF(1, GPS_AP_MOD, AP_UNIFY_ID_FLAG | 2, 1)
/* Wait to add  */
/* ------------------------------------------------------------------------- */
#endif
