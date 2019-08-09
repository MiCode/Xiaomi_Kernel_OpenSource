/*
 *  drivers/misc/mediatek/pmic/mt6360/mt6360_ldo_regmap.c
 *  Driver for MT6360 LDO regmap
 *
 *  Copyright (C) 2018 Mediatek Technology Inc.
 *  cy_huang <cy_huang@richtek.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *  See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/device.h>

#include "../inc/mt6360_ldo.h"

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(MT6360_LDO_RST_LDO_PAS_CODE1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_RST_LDO_PAS_CODE2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_RST_LDO, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_RESV1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO3_EN_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO3_EN_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO3_CTRL0, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO3_CTRL1, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO3_CTRL2, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO3_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO5_EN_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO5_EN_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO5_CTRL0, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO5_CTRL1, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO5_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO5_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO2_EN_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO2_EN_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO2_CTRL0, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO2_CTRL1, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO2_CTRL2, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO2_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO1_EN_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO1_EN_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO1_CTRL0, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_LDO1_CTRL1, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO1_CTRL2, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_LDO1_CTRL3, 1, RT_NORMAL_WR_ONCE, {});
RT_REG_DECL(MT6360_LDO_RESV2, 1, RT_VOLATILE, {});
RT_REG_DECL(MT6360_LDO_SPARE, 1, RT_VOLATILE, {});

static const rt_register_map_t mt6360_ldo_regmap[] = {
	RT_REG(MT6360_LDO_RST_LDO_PAS_CODE1),
	RT_REG(MT6360_LDO_RST_LDO_PAS_CODE2),
	RT_REG(MT6360_LDO_RST_LDO),
	RT_REG(MT6360_LDO_RESV1),
	RT_REG(MT6360_LDO_LDO3_EN_CTRL1),
	RT_REG(MT6360_LDO_LDO3_EN_CTRL2),
	RT_REG(MT6360_LDO_LDO3_CTRL0),
	RT_REG(MT6360_LDO_LDO3_CTRL1),
	RT_REG(MT6360_LDO_LDO3_CTRL2),
	RT_REG(MT6360_LDO_LDO3_CTRL3),
	RT_REG(MT6360_LDO_LDO5_EN_CTRL1),
	RT_REG(MT6360_LDO_LDO5_EN_CTRL2),
	RT_REG(MT6360_LDO_LDO5_CTRL0),
	RT_REG(MT6360_LDO_LDO5_CTRL1),
	RT_REG(MT6360_LDO_LDO5_CTRL2),
	RT_REG(MT6360_LDO_LDO5_CTRL3),
	RT_REG(MT6360_LDO_LDO2_EN_CTRL1),
	RT_REG(MT6360_LDO_LDO2_EN_CTRL2),
	RT_REG(MT6360_LDO_LDO2_CTRL0),
	RT_REG(MT6360_LDO_LDO2_CTRL1),
	RT_REG(MT6360_LDO_LDO2_CTRL2),
	RT_REG(MT6360_LDO_LDO2_CTRL3),
	RT_REG(MT6360_LDO_LDO1_EN_CTRL1),
	RT_REG(MT6360_LDO_LDO1_EN_CTRL2),
	RT_REG(MT6360_LDO_LDO1_CTRL0),
	RT_REG(MT6360_LDO_LDO1_CTRL1),
	RT_REG(MT6360_LDO_LDO1_CTRL2),
	RT_REG(MT6360_LDO_LDO1_CTRL3),
	RT_REG(MT6360_LDO_RESV2),
	RT_REG(MT6360_LDO_SPARE),
};

static struct rt_regmap_properties mt6360_ldo_regmap_props = {
	.register_num = ARRAY_SIZE(mt6360_ldo_regmap),
	.rm = mt6360_ldo_regmap,
	.rt_regmap_mode = RT_MULTI_BYTE | RT_DBG_SPECIAL,
	.aliases = "mt6360_ldo",
};

int mt6360_ldo_regmap_register(struct mt6360_ldo_info *mli,
			       struct rt_regmap_fops *fops)
{
	mt6360_ldo_regmap_props.name = kasprintf(GFP_KERNEL,
						 "mt6360_ldo.%s",
						 dev_name(mli->dev));
	mli->regmap = rt_regmap_device_register(&mt6360_ldo_regmap_props, fops,
						mli->dev, mli->i2c, mli);
	return mli->regmap ? 0 : -EINVAL;
}
EXPORT_SYMBOL_GPL(mt6360_ldo_regmap_register);

void mt6360_ldo_regmap_unregister(struct mt6360_ldo_info *mli)
{
	rt_regmap_device_unregister(mli->regmap);
}
EXPORT_SYMBOL_GPL(mt6360_ldo_regmap_unregister);
#else
int mt6360_ldo_regmap_register(struct mt6360_ldo_info *mli,
			       struct rt_regmap_fops *fops)
{
	return 0;
}
EXPORT_SYMBOL_GPL(mt6360_ldo_regmap_register);

void mt6360_ldo_regmap_unregister(struct mt6360_ldo_info *mli)
{
}
EXPORT_SYMBOL_GPL(mt6360_ldo_regmap_unregister);
#endif /* CONFIG_RT_REGMAP */
