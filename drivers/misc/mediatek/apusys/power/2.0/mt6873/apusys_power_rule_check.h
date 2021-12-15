// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2020 MediaTek Inc.
 */

#ifndef _APU_POWER_RULE_CHECK_H_
#define _APU_POWER_RULE_CHECK_H_

#include "apu_power_api.h"

extern void apu_power_assert_check(struct apu_power_info *info);
extern void constraints_check_stress(int opp);
extern void voltage_constraint_check(void);

#endif // _APU_POWER_RULE_CHECK_H_
