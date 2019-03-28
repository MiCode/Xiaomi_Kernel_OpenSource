/* Copyright (c) 2017 The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __STEP_CHG_H__
#define __STEP_CHG_H__
int qcom_step_chg_init(bool, bool);
int qcom_soft_jeita_fcc_init(int critical_low_fcc, int cool_fcc, int normal_cool_fcc, int normal_fcc, int warm_fcc);
void qcom_step_chg_deinit(void);
#endif /* __STEP_CHG_H__ */
