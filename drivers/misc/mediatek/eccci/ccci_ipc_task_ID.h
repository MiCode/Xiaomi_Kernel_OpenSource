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
/* Priority --> Local module ID --> External ID --> Max sent message */
/* X_IPC_MODULE_CONF(1,M_SSDBG1,0,1)     //TASK_ID_1 */
/* X_IPC_MODULE_CONF(1,AP_SSDBG2,1,1)     //TASK_ID_2 */
#ifdef __IPC_ID_TABLE
#define X_IPC_MODULE_CONF(a, b, c, d) {c, b},
#else
#define X_IPC_MODULE_CONF(a, b, c, d)
#endif

#define AP_UNIFY_ID_FLAG (1<<31)
#define MD_UNIFY_ID_FLAG (0<<31)

/* ---------------------------------------------------------- */
#define    MD_MOD_L4C    0
#define    MD_MOD_L4C_1  1
#define    MD_MOD_L4C_2  2
#define    MD_MOD_L4C_3  3
#define    MD_MOD_AOMGR  4
#define    MD_MOD_EL1    5
#define    MD_MOD_MISCTA 6
#define    MD_MOD_CCCIIPC 7
#define    MD_MOD_IPCORE  8
#define    MD_MOD_MDT     9
#define    MD_MOD_UFPM    10
#define    MD_MOD_USBCLASS 11
#define    MD_MOD_WAAL     12

#define    AP_IPC_AGPS   0
#define    AP_IPC_DHCP   1
#define    AP_IPC_GPS    2
#define    AP_IPC_WMT    3
#define    AP_IPC_MISCTA 4
#define    AP_IPC_CCCIIPC 5
#define    AP_IPC_GF      6
#define    AP_IPC_PKTTRC  7
#define    AP_IPC_USB     8
#define    AP_IPC_LWAPROXY 9

#define    AP_MOD_AGPS   (AP_IPC_AGPS | AP_UNIFY_ID_FLAG)
#define    AP_MOD_DHCP   (AP_IPC_DHCP | AP_UNIFY_ID_FLAG)
#define    AP_MOD_GPS    (AP_IPC_GPS | AP_UNIFY_ID_FLAG)
#define    AP_MOD_WMT    (AP_IPC_WMT | AP_UNIFY_ID_FLAG)
#define    AP_MOD_MISCTA (AP_IPC_MISCTA | AP_UNIFY_ID_FLAG)
#define    AP_MOD_CCCIIPC (AP_IPC_CCCIIPC | AP_UNIFY_ID_FLAG)
#define    AP_MOD_GF      (AP_IPC_GF | AP_UNIFY_ID_FLAG)
#define    AP_MOD_PKTTRC  (AP_IPC_PKTTRC | AP_UNIFY_ID_FLAG)
#define    AP_MOD_USB     (AP_IPC_USB | AP_UNIFY_ID_FLAG)
#define    AP_MOD_LWAPROXY  (AP_IPC_LWAPROXY | AP_UNIFY_ID_FLAG)

/* -------------------------------------------------------------------------- */
X_IPC_MODULE_CONF(1, MD_MOD_L4C, MD_UNIFY_ID_FLAG | MD_MOD_L4C, 1)
X_IPC_MODULE_CONF(1, MD_MOD_L4C_1, MD_UNIFY_ID_FLAG | MD_MOD_L4C_1, 1)
X_IPC_MODULE_CONF(1, MD_MOD_L4C_2, MD_UNIFY_ID_FLAG | MD_MOD_L4C_2, 1)
X_IPC_MODULE_CONF(1, MD_MOD_L4C_3, MD_UNIFY_ID_FLAG | MD_MOD_L4C_3, 1)
X_IPC_MODULE_CONF(1, MD_MOD_AOMGR, MD_UNIFY_ID_FLAG | MD_MOD_AOMGR, 1)
X_IPC_MODULE_CONF(1, MD_MOD_EL1, MD_UNIFY_ID_FLAG | MD_MOD_EL1, 1)
X_IPC_MODULE_CONF(1, MD_MOD_MISCTA, MD_UNIFY_ID_FLAG | MD_MOD_MISCTA, 1)
X_IPC_MODULE_CONF(1, MD_MOD_CCCIIPC, MD_UNIFY_ID_FLAG | MD_MOD_CCCIIPC, 1)
X_IPC_MODULE_CONF(1, MD_MOD_IPCORE, MD_UNIFY_ID_FLAG | MD_MOD_IPCORE, 1)
X_IPC_MODULE_CONF(1, MD_MOD_MDT, MD_UNIFY_ID_FLAG | MD_MOD_MDT, 1)
X_IPC_MODULE_CONF(1, MD_MOD_UFPM, MD_UNIFY_ID_FLAG | MD_MOD_UFPM, 1)
X_IPC_MODULE_CONF(1, MD_MOD_WAAL, MD_UNIFY_ID_FLAG | MD_MOD_WAAL, 1)

/* -------------------------------------------------------------------------- */
X_IPC_MODULE_CONF(1, AP_IPC_AGPS, AP_UNIFY_ID_FLAG | AP_IPC_AGPS, 1)
X_IPC_MODULE_CONF(1, AP_IPC_DHCP, AP_UNIFY_ID_FLAG | AP_IPC_DHCP, 1)
X_IPC_MODULE_CONF(1, AP_IPC_GPS, AP_UNIFY_ID_FLAG | AP_IPC_GPS, 1)
X_IPC_MODULE_CONF(1, AP_IPC_WMT, AP_UNIFY_ID_FLAG | AP_IPC_WMT, 1)
X_IPC_MODULE_CONF(1, AP_IPC_MISCTA, AP_UNIFY_ID_FLAG | AP_IPC_MISCTA, 1)
X_IPC_MODULE_CONF(1, AP_IPC_CCCIIPC, AP_UNIFY_ID_FLAG | AP_IPC_CCCIIPC, 1)
X_IPC_MODULE_CONF(1, AP_IPC_GF, AP_UNIFY_ID_FLAG | AP_IPC_GF, 1)
X_IPC_MODULE_CONF(1, AP_IPC_PKTTRC, AP_UNIFY_ID_FLAG | AP_IPC_PKTTRC, 1)
X_IPC_MODULE_CONF(1, AP_IPC_USB, AP_UNIFY_ID_FLAG | AP_IPC_USB, 1)
X_IPC_MODULE_CONF(1, AP_IPC_LWAPROXY, AP_UNIFY_ID_FLAG | AP_IPC_LWAPROXY, 1)
/* ------------------------------------------------------------------------- */
#endif
