// SPDX-License-Identifier: GPL-2.0-only
/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/regmap.h>
#include "bolero-cdc.h"
#include "internal.h"

#define REG_BYTES 2
#define VAL_BYTES 1

const u16 macro_id_base_offset[MAX_MACRO] = {
	TX_START_OFFSET,
	RX_START_OFFSET,
	WSA_START_OFFSET,
	VA_START_OFFSET,
};

int bolero_get_macro_id(bool va_no_dec_flag, u16 reg)
{
	if (reg >= TX_START_OFFSET
		&& reg <= TX_MAX_OFFSET)
		return TX_MACRO;
	if (reg >= RX_START_OFFSET
		&& reg <= RX_MAX_OFFSET)
		return RX_MACRO;
	if (reg >= WSA_START_OFFSET
		&& reg <= WSA_MAX_OFFSET)
		return WSA_MACRO;
	if (!va_no_dec_flag &&
		(reg >= VA_START_OFFSET &&
		reg <= VA_MAX_OFFSET))
		return VA_MACRO;
	if (va_no_dec_flag &&
		(reg >= VA_START_OFFSET &&
		reg <= VA_TOP_MAX_OFFSET))
		return VA_MACRO;

	return -EINVAL;
}

static int regmap_bus_read(void *context, const void *reg, size_t reg_size,
			   void *val, size_t val_size)
{
	struct device *dev = context;
	struct bolero_priv *priv = dev_get_drvdata(dev);
	u16 *reg_p;
	u16 __reg;
	int macro_id, i;
	u8 temp = 0;
	int ret = -EINVAL;

	if (!priv) {
		dev_err(dev, "%s: priv is NULL\n", __func__);
		return ret;
	}
	if (!reg || !val) {
		dev_err(dev, "%s: reg or val is NULL\n", __func__);
		return ret;
	}
	if (reg_size != REG_BYTES) {
		dev_err(dev, "%s: register size %zd bytes, not supported\n",
			__func__, reg_size);
		return ret;
	}

	reg_p = (u16 *)reg;
	macro_id = bolero_get_macro_id(priv->va_without_decimation,
					   reg_p[0]);
	if (macro_id < 0 || !priv->macros_supported[macro_id])
		return 0;

	mutex_lock(&priv->io_lock);
	for (i = 0; i < val_size; i++) {
		__reg = (reg_p[0] + i * 4) - macro_id_base_offset[macro_id];
		ret = priv->read_dev(priv, macro_id, __reg, &temp);
		if (ret < 0) {
			dev_err_ratelimited(dev,
			"%s: Codec read failed (%d), reg: 0x%x, size:%zd\n",
			__func__, ret, reg_p[0] + i * 4, val_size);
			break;
		}
		((u8 *)val)[i] = temp;
		dev_dbg(dev, "%s: Read 0x%02x from reg 0x%x\n",
			__func__, temp, reg_p[0] + i * 4);
	}
	mutex_unlock(&priv->io_lock);

	return ret;
}

static int regmap_bus_gather_write(void *context,
				   const void *reg, size_t reg_size,
				   const void *val, size_t val_size)
{
	struct device *dev = context;
	struct bolero_priv *priv = dev_get_drvdata(dev);
	u16 *reg_p;
	u16 __reg;
	int macro_id, i;
	int ret = -EINVAL;

	if (!priv) {
		dev_err(dev, "%s: priv is NULL\n", __func__);
		return ret;
	}
	if (!reg || !val) {
		dev_err(dev, "%s: reg or val is NULL\n", __func__);
		return ret;
	}
	if (reg_size != REG_BYTES) {
		dev_err(dev, "%s: register size %zd bytes, not supported\n",
			__func__, reg_size);
		return ret;
	}

	reg_p = (u16 *)reg;
	macro_id = bolero_get_macro_id(priv->va_without_decimation,
					reg_p[0]);
	if (macro_id < 0 || !priv->macros_supported[macro_id])
		return 0;

	mutex_lock(&priv->io_lock);
	for (i = 0; i < val_size; i++) {
		__reg = (reg_p[0] + i * 4) - macro_id_base_offset[macro_id];
		ret = priv->write_dev(priv, macro_id, __reg, ((u8 *)val)[i]);
		if (ret < 0) {
			dev_err_ratelimited(dev,
			"%s: Codec write failed (%d), reg:0x%x, size:%zd\n",
			__func__, ret, reg_p[0] + i * 4, val_size);
			break;
		}
		dev_dbg(dev, "Write %02x to reg 0x%x\n", ((u8 *)val)[i],
			reg_p[0] + i * 4);
	}
	mutex_unlock(&priv->io_lock);
	return ret;
}

static int regmap_bus_write(void *context, const void *data, size_t count)
{
	struct device *dev = context;
	struct bolero_priv *priv = dev_get_drvdata(dev);

	if (!priv)
		return -EINVAL;

	if (count < REG_BYTES) {
		dev_err(dev, "%s: count %zd bytes < %d, not supported\n",
			__func__, count, REG_BYTES);
		return -EINVAL;
	}

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

struct regmap *bolero_regmap_init(struct device *dev,
				      const struct regmap_config *config)
{
	return devm_regmap_init(dev, &regmap_bus_config, dev, config);
}
