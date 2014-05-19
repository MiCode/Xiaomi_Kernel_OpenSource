/*
 * intel_soc_pmic.h - Intel SoC PMIC Driver
 *
 * Copyright (C) 2012-2014 Intel Corporation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * Author: Yang, Bin <bin.yang@intel.com>
 */

#ifndef __INTEL_SOC_PMIC_H__
#define __INTEL_SOC_PMIC_H__

#define	INTEL_PMIC_IRQBASE	456

int intel_soc_pmic_readb(int reg);
int intel_soc_pmic_writeb(int reg, u8 val);
int intel_soc_pmic_setb(int reg, u8 mask);
int intel_soc_pmic_clearb(int reg, u8 mask);
int intel_soc_pmic_update(int reg, u8 val, u8 mask);
int intel_soc_pmic_set_pdata(const char *name, void *data, int len);
struct device *intel_soc_pmic_dev(void);

#endif	/* __INTEL_SOC_PMIC_H__ */
