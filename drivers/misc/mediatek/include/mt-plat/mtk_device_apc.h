/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _MTK_DEVICE_APC_H
#define _MTK_DEVICE_APC_H

extern int mt_devapc_emi_initial(void);
extern int mt_devapc_check_emi_violation(void);
extern int mt_devapc_check_emi_mpu_violation(void);
extern int mt_devapc_clear_emi_violation(void);
extern int mt_devapc_clear_emi_mpu_violation(void);
#endif

