/* SPDX-License-Identifier: GPL-2.0
 * Copyright (C) 2021 Spreadtrum Communications Inc.
 */

#include <linux/sysfs.h>

#define UMP96XX_XTL_WAIT_CTRL0		0x20e0
#define UMP96XX_XTL_EN			BIT(8)

#define UMP96XX_TSEN_CTRL0		0x0
#define UMP96XX_TSEN_CLK_SRC_SEL	BIT(4)

#define UMP96XX_TSEN_CTRL1		0x4
#define UMP96XX_TSEN_26M_EN		BIT(0)
#define UMP96XX_TSEN_SDADC_EN		BIT(4)

#define UMP96XX_TSEN_CTRL3		0xc
#define UMP96XX_TSEN_SEL_CH		BIT(3)
#define UMP96XX_TSEN_EN			BIT(4)
#define UMP96XX_TSEN_UGBUF_EN		BIT(8)
#define UMP96XX_TSEN_ADCLDO_EN		BIT(12)

#define UMP96XX_TSEN_CTRL4		0x10
#define UMP96XX_TSEN_CTRL5		0x14
#define UMP96XX_TSEN_CTRL6		0x18
#define UMP96XX_TSEN_SEL_EN		(BIT(6) | BIT(7))

#if IS_ENABLED(CONFIG_SPRD_UMP96XX_TSENSOR)
extern int gnss_tsen_control(struct regmap *regmap, unsigned int base, bool en);
#else
extern int gnss_tsen_control(struct regmap *regmap, unsigned int base, bool en)
{
	return 0;
}
#endif

