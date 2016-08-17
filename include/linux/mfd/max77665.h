/*
 * Core driver interface for MAXIM77665
 *
 * Copyright (c) 2012, NVIDIA CORPORATION.  All rights reserved.

 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.

 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __LINUX_MFD_MAX77665_H
#define __LINUX_MFD_MAX77665_H

#include <linux/irq.h>
#include <linux/regmap.h>

/* Max77665 irq register */
#define MAX77665_REG_INTSRC		0x22
#define MAX77665_REG_INTSRC_MASK	0x23
#define MAX77665_INTSRC_CHGR_MASK	BIT(0)
#define MAX77665_INTSRC_TOP_MASK	BIT(1)
#define MAX77665_INTSRC_FLASH_MASK	BIT(2)
#define MAX77665_INTSRC_MUIC_MASK	BIT(3)

#define MAX77665_REG_TOP_SYS_INT_STS	0x24
#define MAX77665_REG_TOP_SYS_INT_MASK	0x26
#define MAX77665_TOP_SYS_INT_120C	BIT(0)
#define MAX77665_TOP_SYS_INT_140C	BIT(1)
#define MAX77665_TOP_SYS_INT_LOWSYS	BIT(3)

/* MAX77665 Interrups */
enum {
	MAX77665_IRQ_CHARGER,
	MAX77665_IRQ_TOP_SYS,
	MAX77665_IRQ_FLASH,
	MAX77665_IRQ_MUIC,
	MAX77665_NUM_IRQ,
};

enum {
	MAX77665_I2C_SLAVE_PMIC,
	MAX77665_I2C_SLAVE_MUIC,
	MAX77665_I2C_SLAVE_HAPTIC,
	MAX77665_I2C_SLAVE_MAX,
};

enum {
	MAX77665_CELL_CHARGER,
	MAX77665_CELL_FLASH,
	MAX77665_CELL_MUIC,
	MAX77665_CELL_HAPTIC,
	MAX77665_CELL_MAX,
};

struct max77665 {
	struct device		*dev;
	struct i2c_client	*client[MAX77665_I2C_SLAVE_MAX];
	struct regmap		*regmap[MAX77665_I2C_SLAVE_MAX];
	struct regmap_irq_chip_data *regmap_irq_data;
	int			irq_base;
	int			top_sys_irq;
};

struct max77665_cell_data {
	void *pdata;
	size_t size;
};

struct max77665_system_interrupt {
	bool enable_thermal_interrupt;
	bool enable_low_sys_interrupt;
};

struct max77665_platform_data {
	int irq_base;
	unsigned long irq_flag;
	struct max77665_system_interrupt *system_interrupt;
	struct max77665_cell_data charger_platform_data;
	struct max77665_cell_data flash_platform_data;
	struct max77665_cell_data muic_platform_data;
	struct max77665_cell_data haptic_platform_data;
};

static inline int max77665_write(struct device *dev, int slv_id,
		int reg, uint8_t val)
{
	struct max77665 *maxim = dev_get_drvdata(dev);

	return regmap_write(maxim->regmap[slv_id], reg, val);
}

static inline int max77665_read(struct device *dev, int slv_id,
		int reg, uint8_t *val)
{
	struct max77665 *maxim = dev_get_drvdata(dev);
	unsigned int temp_val;
	int ret;

	ret = regmap_read(maxim->regmap[slv_id], reg, &temp_val);
	if (!ret)
		*val = temp_val;
	return ret;
}

static inline int max77665_bulk_read(struct device *dev, int slv_id,
		int reg, int count, void *val)
{
	struct max77665 *maxim = dev_get_drvdata(dev);

	return regmap_bulk_read(maxim->regmap[slv_id], reg, val, count);
}

static inline int max77665_update_bits(struct device *dev, int slv_id,
		int reg, unsigned int mask, unsigned int  val)
{
	struct max77665 *maxim = dev_get_drvdata(dev);

	return regmap_update_bits(maxim->regmap[slv_id], reg, mask, val);
}

static inline int max77665_set_bits(struct device *dev, int slv_id,
		int reg, uint8_t bit_num)
{
	struct max77665 *maxim = dev_get_drvdata(dev);

	return regmap_update_bits(maxim->regmap[slv_id],
				reg, BIT(bit_num), ~0u);
}

static inline int max77665_clr_bits(struct device *dev, int slv_id,
		int reg, uint8_t bit_num)
{
	struct max77665 *maxim = dev_get_drvdata(dev);

	return regmap_update_bits(maxim->regmap[slv_id],
				reg, BIT(bit_num), 0u);
}

#endif /*__LINUX_MFD_MAX77665_H */
