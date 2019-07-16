/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __MT6360_H__
#define __MT6360_H__

#include <linux/regmap.h>

enum {
	MT6360_SLAVE_PMU = 0,
	MT6360_SLAVE_PMIC,
	MT6360_SLAVE_LDO,
	MT6360_SLAVE_TCPC,
	MT6360_SLAVE_MAX,
};

#define MT6360_PMU_SLAVEID	(0x34)
#define MT6360_PMIC_SLAVEID	(0x1A)
#define MT6360_LDO_SLAVEID	(0x64)
#define MT6360_TCPC_SLAVEID	(0x4E)

struct mt6360_pmu_info {
	struct i2c_client *i2c[MT6360_SLAVE_MAX];
	struct device *dev;
	struct regmap *regmap;
	struct regmap_irq_chip_data *irq_data;
	struct regmap_irq_chip irq_chip;
	unsigned int chip_rev;
};

#endif /* __MT6360_H__ */
