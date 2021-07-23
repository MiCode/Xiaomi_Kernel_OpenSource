/*
 * Copyright (C) 2019 MediaTek Inc.
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

#ifndef _APU_POWER_RULE_CHECK_H_
#define _APU_POWER_RULE_CHECK_H_

#include "apu_power_api.h"

extern void apu_power_assert_check(struct apu_power_info *info);
extern void constraints_check_stress(int opp);
extern void voltage_constraint_check(void);

#endif // _APU_POWER_RULE_CHECK_H_
