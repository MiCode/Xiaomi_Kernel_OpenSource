/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
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


#ifndef _SIA8101_REGS_H
#define _SIA8101_REGS_H

#define SIA8101_REG_CHIP_ID				(0x00)
#define SIA8101_REG_MOD_CFG				(0x01)
#define SIA8101_REG_SYS_EN				(0x02)
#define SIA8101_REG_X_DATA_A			(0x03)
#define SIA8101_REG_X_DATA_B			(0x04)
#define SIA8101_REG_X_DATA_C			(0x05)
#define SIA8101_REG_TEST_CFG			(0x06)
#define SIA8101_REG_STATE_FLAG1			(0x07)
#define SIA8101_REG_STATE_FLAG2			(0x08)

#define SIA8101_CHIP_ID_VAL				(0x81)
#define SIA8101_CHIP_ID_MASK			(0xF0)


extern const struct regmap_config sia8101_regmap_config;
extern const struct sia81xx_reg_default_val sia8101_reg_default_val;
extern const struct sia81xx_opt_if sia8101_opt_if;


#endif /* _SIA8101_REGS_H */

