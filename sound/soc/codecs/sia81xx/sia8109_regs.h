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


#ifndef _SIA8109_REGS_H
#define _SIA8109_REGS_H

#define SIA8109_REG_SYSCTRL				(0x00)
#define SIA8109_REG_AGCCTRL				(0x01)
#define SIA8109_REG_BOOST_CFG			(0x02)
#define SIA8109_REG_CLSD_CFG1			(0x03)
#define SIA8109_REG_CLSD_CFG2			(0x04)
#define SIA8109_REG_BSG_CFG				(0x05)
#define SIA8109_REG_SML_CFG1			(0x06)
#define SIA8109_REG_SML_CFG2			(0x07)
#define SIA8109_REG_SML_CFG3			(0x08)
#define SIA8109_REG_SML_CFG4			(0x09)
#define SIA8109_REG_Excur_CTRL_1		(0x0A)
#define SIA8109_REG_Excur_CTRL_2		(0x0B)
#define SIA8109_REG_Excur_CTRL_3		(0x0C)
#define SIA8109_REG_Excur_CTRL_4		(0x0D)
#define SIA8109_REG_CHIP_ID				(0x41)

#define SIA8109_CHIP_ID_V0_1			(0x09)

extern const struct regmap_config sia8109_regmap_config;
extern const struct sia81xx_reg_default_val sia8109_reg_default_val;
extern const struct sia81xx_opt_if sia8109_opt_if;

#endif /* _SIA8108_REGS_H */

