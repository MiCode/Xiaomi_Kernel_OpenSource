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

#ifndef _MTK_DEVICE_APC_H
#define _MTK_DEVICE_APC_H

extern int mt_devapc_emi_initial(void);
extern int mt_devapc_check_emi_violation(void);
extern int mt_devapc_check_emi_mpu_violation(void);
extern int mt_devapc_clear_emi_violation(void);
extern int mt_devapc_clear_emi_mpu_violation(void);
#endif

