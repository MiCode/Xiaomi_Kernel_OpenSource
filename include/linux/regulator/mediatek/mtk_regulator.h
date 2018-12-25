/*
 * mtk_regulator.h -- MTK Regulator driver support.
 * Copyright (C) 2017 MediaTek Inc.
 * Author: Patrick Chang <patrick_chang@richtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */


#ifndef __LINUX_REGULATOR_MTK_REGULATOR_H_
#define __LINUX_REGULATOR_MTK_REGULATOR_H_

#include <linux/regulator/consumer.h>
#include <linux/regulator/mediatek/mtk_regulator_class.h>

struct mtk_regulator {
	struct regulator *consumer;
	struct mtk_simple_regulator_device *mreg_dev;
};

extern int mtk_regulator_get(struct device *dev, const char *id,
	struct mtk_regulator *mreg);
extern int mtk_regulator_get_exclusive(struct device *dev, const char *id,
	struct mtk_regulator *mreg);
extern int devm_mtk_regulator_get(struct device *dev, const char *id,
	struct mtk_regulator *mreg);
extern void mtk_regulator_put(struct mtk_regulator *mreg);
extern void devm_mtk_regulator_put(struct mtk_regulator *mreg);

extern int mtk_regulator_enable(struct mtk_regulator *mreg, bool enable);
extern int mtk_regulator_force_disable(struct mtk_regulator *mreg);
extern int mtk_regulator_is_enabled(struct mtk_regulator *mreg);

extern int mtk_regulator_set_mode(struct mtk_regulator *mreg,
	unsigned int mode);
extern unsigned int mtk_regulator_get_mode(struct mtk_regulator *mreg);
extern int mtk_regulator_set_voltage(struct mtk_regulator *mreg,
	int min_uv, int max_uv);
extern int mtk_regulator_get_voltage(struct mtk_regulator *mreg);
extern int mtk_regulator_set_current_limit(struct mtk_regulator *mreg,
	int min_uA, int max_uA);
extern int mtk_regulator_get_current_limit(struct mtk_regulator *mreg);

/* Advanced ops */
extern int mtk_regulator_set_property(struct mtk_regulator *mreg,
	enum mtk_simple_regulator_property prop,
	union mtk_simple_regulator_propval *val);
extern int mtk_regulator_get_property(struct mtk_regulator *mreg,
	enum mtk_simple_regulator_property prop,
	union mtk_simple_regulator_propval *val);

#endif /* __LINUX_REGULATOR_MTK_REGULATOR_H_ */
