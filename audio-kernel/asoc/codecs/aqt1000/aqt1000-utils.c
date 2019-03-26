/* Copyright (c) 2016-2018, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/sched.h>
#include "aqt1000.h"
#include "aqt1000-utils.h"

#define REG_BYTES 2
#define VAL_BYTES 1
/*
 * Page Register Address that APP Proc uses to
 * access codec registers is identified as 0x00
 */
#define PAGE_REG_ADDR 0x00

static int aqt_page_write(struct aqt1000 *aqt, unsigned short *reg)
{
	int ret = 0;
	unsigned short c_reg, reg_addr;
	u8 pg_num, prev_pg_num;

	c_reg = *reg;
	pg_num = c_reg >> 8;
	reg_addr = c_reg & 0xff;
	if (aqt->prev_pg_valid) {
		prev_pg_num = aqt->prev_pg;
		if (prev_pg_num != pg_num) {
			ret = aqt->write_dev(
					aqt, PAGE_REG_ADDR,
					(void *) &pg_num, 1);
			if (ret < 0)
				dev_err(aqt->dev,
					"%s: page write error, pg_num: 0x%x\n",
					__func__, pg_num);
			else {
				aqt->prev_pg = pg_num;
				dev_dbg(aqt->dev, "%s: Page 0x%x Write to 0x00\n",
					__func__, pg_num);
			}
		}
	} else {
		ret = aqt->write_dev(
				aqt, PAGE_REG_ADDR, (void *) &pg_num, 1);
		if (ret < 0)
			dev_err(aqt->dev,
				"%s: page write error, pg_num: 0x%x\n",
				__func__, pg_num);
		else {
			aqt->prev_pg = pg_num;
			aqt->prev_pg_valid = true;
			dev_dbg(aqt->dev, "%s: Page 0x%x Write to 0x00\n",
				__func__, pg_num);
		}
	}
	*reg = reg_addr;
	return ret;
}

static int regmap_bus_read(void *context, const void *reg, size_t reg_size,
			   void *val, size_t val_size)
{
	struct device *dev = context;
	struct aqt1000 *aqt = dev_get_drvdata(dev);
	unsigned short c_reg, rreg;
	int ret, i;

	if (!aqt) {
		dev_err(dev, "%s: aqt is NULL\n", __func__);
		return -EINVAL;
	}
	if (!reg || !val) {
		dev_err(dev, "%s: reg or val is NULL\n", __func__);
		return -EINVAL;
	}

	if (reg_size != REG_BYTES) {
		dev_err(dev, "%s: register size %zd bytes, not supported\n",
			__func__, reg_size);
		return -EINVAL;
	}

	mutex_lock(&aqt->io_lock);
	c_reg = *(u16 *)reg;
	rreg = c_reg;

	ret = aqt_page_write(aqt, &c_reg);
	if (ret)
		goto err;
	ret = aqt->read_dev(aqt, c_reg, val, val_size);
	if (ret < 0)
		dev_err(dev, "%s: Codec read failed (%d), reg: 0x%x, size:%zd\n",
			__func__, ret, rreg, val_size);
	else {
		for (i = 0; i < val_size; i++)
			dev_dbg(dev, "%s: Read 0x%02x from 0x%x\n",
				__func__, ((u8 *)val)[i], rreg + i);
	}
err:
	mutex_unlock(&aqt->io_lock);
	return ret;
}

static int regmap_bus_gather_write(void *context,
				   const void *reg, size_t reg_size,
				   const void *val, size_t val_size)
{
	struct device *dev = context;
	struct aqt1000 *aqt = dev_get_drvdata(dev);
	unsigned short c_reg, rreg;
	int ret, i;

	if (!aqt) {
		dev_err(dev, "%s: aqt is NULL\n", __func__);
		return -EINVAL;
	}
	if (!reg || !val) {
		dev_err(dev, "%s: reg or val is NULL\n", __func__);
		return -EINVAL;
	}
	if (reg_size != REG_BYTES) {
		dev_err(dev, "%s: register size %zd bytes, not supported\n",
			__func__, reg_size);
		return -EINVAL;
	}
	mutex_lock(&aqt->io_lock);
	c_reg = *(u16 *)reg;
	rreg = c_reg;

	ret = aqt_page_write(aqt, &c_reg);
	if (ret)
		goto err;

	for (i = 0; i < val_size; i++)
		dev_dbg(dev, "Write %02x to 0x%x\n", ((u8 *)val)[i],
			rreg + i);

	ret = aqt->write_dev(aqt, c_reg, (void *) val, val_size);
	if (ret < 0)
		dev_err(dev, "%s: Codec write failed (%d), reg:0x%x, size:%zd\n",
			__func__, ret, rreg, val_size);

err:
	mutex_unlock(&aqt->io_lock);
	return ret;
}

static int regmap_bus_write(void *context, const void *data, size_t count)
{
	struct device *dev = context;
	struct aqt1000 *aqt = dev_get_drvdata(dev);

	if (!aqt)
		return -EINVAL;

	WARN_ON(count < REG_BYTES);

	return regmap_bus_gather_write(context, data, REG_BYTES,
					       data + REG_BYTES,
					       count - REG_BYTES);
}

static struct regmap_bus regmap_bus_config = {
	.write = regmap_bus_write,
	.gather_write = regmap_bus_gather_write,
	.read = regmap_bus_read,
	.reg_format_endian_default = REGMAP_ENDIAN_NATIVE,
	.val_format_endian_default = REGMAP_ENDIAN_NATIVE,
};

/*
 * aqt1000_regmap_init:
 *	Initialize aqt1000 register map
 *
 * @dev: pointer to wcd device
 * @config: pointer to register map config
 *
 * Returns pointer to regmap structure for success
 * or NULL in case of failure.
 */
struct regmap *aqt1000_regmap_init(struct device *dev,
				   const struct regmap_config *config)
{
	return devm_regmap_init(dev, &regmap_bus_config, dev, config);
}
EXPORT_SYMBOL(aqt1000_regmap_init);
