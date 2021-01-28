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
#ifndef __MT_OTP_PMIC_CONFIG_H__
#define __MT_OTP_PMIC_CONFIG_H__
#include <linux/types.h>

//EXTERN FUNCTION
extern u32 otp_pmic_fsource_set(void);
extern u32 otp_pmic_fsource_release(void);
extern u32 otp_pmic_is_fsource_enabled(void);
extern u32 otp_pmic_high_vcore_init(void);
extern u32 otp_pmic_high_vcore_set(void);
extern u32 otp_pmic_high_vcore_release(void);
#endif /* __MT_OTP_PMIC_CONFIG_H__ */
