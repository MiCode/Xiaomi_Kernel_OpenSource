/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __LINUX_REGULATOR_MT6315_MISC_H
#define __LINUX_REGULATOR_MT6315_MISC_H

#define HW_OFF	0
#define HW_LP   1

struct mt6315_misc {
	uint8_t master_idx;
	uint8_t slave_addr;
	struct regmap *regmap;
};

enum MT6315_BUCK_EN_USER {
	MT6315_SRCLKEN0,
	MT6315_SRCLKEN1,
	MT6315_SRCLKEN2,
	MT6315_SRCLKEN3,
	MT6315_SRCLKEN4,
	MT6315_SRCLKEN5,
	MT6315_SRCLKEN6,
	MT6315_SRCLKEN7,
	MT6315_SW,
};

extern void mt6315_vmd1_pmic_setting_on(void);
extern int is_mt6315_exist(void);
extern void mt6315_misc_init(u32 sid, struct regmap *regmap);

#endif /* __LINUX_REGULATOR_MT6315_MISC_H */
