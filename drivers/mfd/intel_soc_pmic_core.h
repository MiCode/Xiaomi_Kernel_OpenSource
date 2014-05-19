/*
 * intel_soc_pmic_core.h - Intel SoC PMIC MFD Driver
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

#ifndef __INTEL_SOC_PMIC_CORE_H__
#define __INTEL_SOC_PMIC_CORE_H__

#define INTEL_PMIC_IRQ_MAX	128
#define INTEL_PMIC_REG_NULL	{-1,}

#define INTEL_PMIC_REG_INV	(1<<0) /*value revert*/
#define INTEL_PMIC_REG_WO	(1<<1) /*write only*/
#define INTEL_PMIC_REG_RO	(1<<2) /*read only*/
#define INTEL_PMIC_REG_W1C	(1<<3) /*write 1 clear*/
#define INTEL_PMIC_REG_RC	(1<<4) /*read clear*/
#define IS_PMIC_REG_INV(_map)	(_map->flags & INTEL_PMIC_REG_INV)
#define IS_PMIC_REG_WO(_map)	(_map->flags & INTEL_PMIC_REG_WO)
#define IS_PMIC_REG_RO(_map)	(_map->flags & INTEL_PMIC_REG_RO)
#define IS_PMIC_REG_W1C(_map)	(_map->flags & INTEL_PMIC_REG_W1C)
#define IS_PMIC_REG_RC(_map)	(_map->flags & INTEL_PMIC_REG_RC)
#define IS_PMIC_REG_VALID(_map) \
	((_map->mask != 0) && (_map->offset >= 0))

#define PMIC_IRQREG_MASK	0
#define PMIC_IRQREG_STATUS	1
#define PMIC_IRQREG_ACK		2

struct intel_pmic_regmap {
	int				offset;
	int				shift;
	int				mask;
	int				flags;
};

struct intel_pmic_irqregmap {
	struct intel_pmic_regmap	mask;
	struct intel_pmic_regmap	status;
	struct intel_pmic_regmap	ack;
};

struct intel_soc_pmic {
	const char			*label;
	struct device			*dev;
	struct mutex			io_lock; /* For registers */
	struct mutex			irq_lock; /* irq_bus_lock */
	int				irq_need_update;
	int				irq;
	unsigned long			irq_flags;
	int				irq_num;
	int				irq_base;
	unsigned long			irq_mask[INTEL_PMIC_IRQ_MAX/32];
	int				pmic_int_gpio;
	int				(*init)(void);
	int				(*readb)(int);
	int				(*writeb)(int, u8);
	struct intel_pmic_irqregmap	*irq_regmap;
	struct mfd_cell			*cell_dev;
};

int intel_pmic_add(struct intel_soc_pmic *chip);
int intel_pmic_remove(struct intel_soc_pmic *chip);

extern struct intel_soc_pmic crystal_cove_pmic;

#endif	/* __INTEL_SOC_PMIC_CORE_H__ */
