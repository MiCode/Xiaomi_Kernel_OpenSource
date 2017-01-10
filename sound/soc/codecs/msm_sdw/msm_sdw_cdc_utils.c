/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/regmap.h>
#include "msm_sdw.h"

#define REG_BYTES 2
#define VAL_BYTES 1
/*
 * Page Register Address that APP Proc uses to
 * access WCD9335 Codec registers is identified
 * as 0x00
 */
#define PAGE_REG_ADDR 0x00

/*
 * msm_sdw_page_write:
 * Retrieve page number from register and
 * write that page number to the page address.
 * Called under io_lock acquisition.
 *
 * @msm_sdw: pointer to msm_sdw
 * @reg: Register address from which page number is retrieved
 *
 * Returns 0 for success and negative error code for failure.
 */
int msm_sdw_page_write(struct msm_sdw_priv *msm_sdw, unsigned short reg)
{
	int ret = 0;
	u8 pg_num, prev_pg_num;

	pg_num = msm_sdw_page_map[reg];
	if (msm_sdw->prev_pg_valid) {
		prev_pg_num = msm_sdw->prev_pg;
		if (prev_pg_num != pg_num) {
			ret = msm_sdw->write_dev(msm_sdw, PAGE_REG_ADDR, 1,
						 (void *) &pg_num);
			if (ret < 0) {
				dev_err(msm_sdw->dev,
					"page write error, pg_num: 0x%x\n",
					pg_num);
			} else {
				msm_sdw->prev_pg = pg_num;
				dev_dbg(msm_sdw->dev,
					"%s: Page 0x%x Write to 0x00\n",
					__func__, pg_num);
			}
		}
	} else {
		ret = msm_sdw->write_dev(msm_sdw, PAGE_REG_ADDR, 1,
					 (void *) &pg_num);
		if (ret < 0) {
			dev_err(msm_sdw->dev,
				"page write error, pg_num: 0x%x\n", pg_num);
		} else {
			msm_sdw->prev_pg = pg_num;
			msm_sdw->prev_pg_valid = true;
			dev_dbg(msm_sdw->dev, "%s: Page 0x%x Write to 0x00\n",
				__func__, pg_num);
		}
	}
	return ret;
}
EXPORT_SYMBOL(msm_sdw_page_write);

static int regmap_bus_read(void *context, const void *reg, size_t reg_size,
			   void *val, size_t val_size)
{
	struct device *dev = context;
	struct msm_sdw_priv *msm_sdw = dev_get_drvdata(dev);
	unsigned short c_reg;
	int ret, i;

	if (!msm_sdw) {
		dev_err(dev, "%s: msm_sdw is NULL\n", __func__);
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
	if (!msm_sdw->dev_up) {
		dev_dbg_ratelimited(dev, "%s: No read allowed. dev_up = %d\n",
				    __func__, msm_sdw->dev_up);
		return 0;
	}

	mutex_lock(&msm_sdw->io_lock);
	c_reg = *(u16 *)reg;
	ret = msm_sdw_page_write(msm_sdw, c_reg);
	if (ret)
		goto err;
	ret = msm_sdw->read_dev(msm_sdw, c_reg, val_size, val);
	if (ret < 0)
		dev_err(dev, "%s: Codec read failed (%d), reg: 0x%x, size:%zd\n",
			__func__, ret, c_reg, val_size);
	else {
		for (i = 0; i < val_size; i++)
			dev_dbg(dev, "%s: Read 0x%02x from 0x%x\n",
				__func__, ((u8 *)val)[i], c_reg + i);
	}
err:
	mutex_unlock(&msm_sdw->io_lock);

	return ret;
}

static int regmap_bus_gather_write(void *context,
				   const void *reg, size_t reg_size,
				   const void *val, size_t val_size)
{
	struct device *dev = context;
	struct msm_sdw_priv *msm_sdw = dev_get_drvdata(dev);
	unsigned short c_reg;
	int ret, i;

	if (!msm_sdw) {
		dev_err(dev, "%s: msm_sdw is NULL\n", __func__);
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
	if (!msm_sdw->dev_up) {
		dev_dbg_ratelimited(dev, "%s: No write allowed. dev_up = %d\n",
				    __func__, msm_sdw->dev_up);
		return 0;
	}

	mutex_lock(&msm_sdw->io_lock);
	c_reg = *(u16 *)reg;
	ret = msm_sdw_page_write(msm_sdw, c_reg);
	if (ret)
		goto err;

	for (i = 0; i < val_size; i++)
		dev_dbg(dev, "Write %02x to 0x%x\n", ((u8 *)val)[i],
			c_reg + i*4);

	ret = msm_sdw->write_dev(msm_sdw, c_reg, val_size, (void *) val);
	if (ret < 0)
		dev_err(dev,
			"%s: Codec write failed (%d), reg:0x%x, size:%zd\n",
			__func__, ret, c_reg, val_size);

err:
	mutex_unlock(&msm_sdw->io_lock);
	return ret;
}

static int regmap_bus_write(void *context, const void *data, size_t count)
{
	struct device *dev = context;
	struct msm_sdw_priv *msm_sdw = dev_get_drvdata(dev);

	if (!msm_sdw)
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
 * msm_sdw_regmap_init:
 * Initialize msm_sdw register map
 *
 * @dev: pointer to wcd device
 * @config: pointer to register map config
 *
 * Returns pointer to regmap structure for success
 * or NULL in case of failure.
 */
struct regmap *msm_sdw_regmap_init(struct device *dev,
				   const struct regmap_config *config)
{
	return devm_regmap_init(dev, &regmap_bus_config, dev, config);
}
EXPORT_SYMBOL(msm_sdw_regmap_init);
