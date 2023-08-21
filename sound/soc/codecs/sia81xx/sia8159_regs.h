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


#ifndef _SIA8159_REGS_H
#define _SIA8159_REGS_H

#define SIA8159_REG_CHIP_ID				(0x00)
#define SIA8159_REG_SYSCTRL				(0x01)
#define SIA8159_REG_ALGO_EN				(0x02)
#define SIA8159_REG_BST_CFG				(0x03)
#define SIA8159_REG_CLSD_CFG			(0x04)
#define SIA8159_REG_ALGO_CFG1			(0x05)
#define SIA8159_REG_ALGO_CFG2			(0x06)
#define SIA8159_REG_ALGO_CFG3			(0x07)
#define SIA8159_REG_ALGO_CFG4			(0x08)
#define SIA8159_REG_ALGO_CFG5			(0x09)
#define SIA8159_REG_CLSD_OCPCFG			(0x0A)
#define SIA8159_REG_STAT_REG1			(0x10)
#define SIA8159_REG_STAT_REG2			(0x11)
#define SIA8159_REG_TEST_CFG			(0x12)

#define SIA8159_REG_TRIMMING_BEGIN		(0x20)
#define SIA8159_REG_TRIMMING_END		(0x22)

extern const struct regmap_config sia8159_regmap_config;
extern const struct sia81xx_reg_default_val sia8159_reg_default_val;
extern const struct sia81xx_opt_if sia8159_opt_if;

#endif /* _SIA8159_REGS_H */

