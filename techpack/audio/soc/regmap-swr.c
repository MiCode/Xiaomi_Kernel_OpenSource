// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2015-2017, The Linux Foundation. All rights reserved.
 */

#include <linux/device.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/regmap.h>
#include <linux/module.h>
#include <linux/init.h>
#include <soc/soundwire.h>
#include <soc/internal.h>


static int regmap_swr_gather_write(void *context,
				const void *reg, size_t reg_size,
				const void *val, size_t val_len)
{
	struct device *dev = context;
	struct swr_device *swr = to_swr_device(dev);
	struct regmap *map = dev_get_regmap(dev, NULL);
	size_t addr_bytes;
	size_t val_bytes;
	int i, ret = 0;
	u16 reg_addr = 0;
	u8 *value;

	if (map == NULL) {
		dev_err(dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}
	addr_bytes = map->format.reg_bytes;
	if (swr == NULL) {
		dev_err(dev, "%s: swr device is NULL\n", __func__);
		return -EINVAL;
	}
	if (reg_size != addr_bytes) {
		dev_err(dev, "%s: reg size %zd bytes not supported\n",
			__func__, reg_size);
		return -EINVAL;
	}
	reg_addr = *(u16 *)reg;
	val_bytes = map->format.val_bytes;
	/* val_len = val_bytes * val_count */
	for (i = 0; i < (val_len / val_bytes); i++) {
		value = (u8 *)val + (val_bytes * i);
		ret = swr_write(swr, swr->dev_num, (reg_addr + i), value);
		if (ret < 0) {
			dev_err(dev, "%s: write reg 0x%x failed, err %d\n",
				__func__, (reg_addr + i), ret);
			break;
		}
	}
	return ret;
}

static int regmap_swr_raw_multi_reg_write(void *context, const void *data,
					  size_t count)
{
	struct device *dev = context;
	struct swr_device *swr = to_swr_device(dev);
	struct regmap *map = dev_get_regmap(dev, NULL);
	size_t addr_bytes;
	size_t val_bytes;
	size_t pad_bytes;
	size_t num_regs;
	int i = 0;
	int ret = 0;
	u16 *reg;
	u8 *val;
	u8 *buf;

	if (swr == NULL) {
		dev_err(dev, "%s: swr device is NULL\n", __func__);
		return -EINVAL;
	}

	if (map == NULL) {
		dev_err(dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}

	addr_bytes = map->format.reg_bytes;
	val_bytes = map->format.val_bytes;
	pad_bytes = map->format.pad_bytes;

	if (addr_bytes + val_bytes + pad_bytes == 0) {
		dev_err(dev, "%s: sum of addr, value and pad is 0\n", __func__);
		return -EINVAL;
	}
	num_regs = count / (addr_bytes + val_bytes + pad_bytes);

	reg = kcalloc(num_regs, sizeof(u16), GFP_KERNEL);
	if (!reg)
		return -ENOMEM;

	val = kcalloc(num_regs, sizeof(u8), GFP_KERNEL);
	if (!val) {
		ret = -ENOMEM;
		goto mem_fail;
	}

	buf = (u8 *)data;
	for (i = 0; i < num_regs; i++) {
		reg[i] = *(u16 *)buf;
		buf += (map->format.reg_bytes + map->format.pad_bytes);
		val[i] = *buf;
		buf += map->format.val_bytes;
	}
	ret = swr_bulk_write(swr, swr->dev_num, reg, val, num_regs);
	if (ret)
		dev_err(dev, "%s: multi reg write failed\n", __func__);

	kfree(val);
mem_fail:
	kfree(reg);
	return ret;
}

static int regmap_swr_write(void *context, const void *data, size_t count)
{
	struct device *dev = context;
	struct regmap *map = dev_get_regmap(dev, NULL);
	size_t addr_bytes;
	size_t val_bytes;
	size_t pad_bytes;

	if (map == NULL) {
		dev_err(dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}
	addr_bytes = map->format.reg_bytes;
	val_bytes = map->format.val_bytes;
	pad_bytes = map->format.pad_bytes;

	WARN_ON(count < addr_bytes);

	if (count > (addr_bytes + val_bytes + pad_bytes))
		return regmap_swr_raw_multi_reg_write(context, data, count);
	else
		return regmap_swr_gather_write(context, data, addr_bytes,
					       (data + addr_bytes),
					       (count - addr_bytes));
}

static int regmap_swr_read(void *context,
			const void *reg, size_t reg_size,
			void *val, size_t val_size)
{
	struct device *dev = context;
	struct swr_device *swr = to_swr_device(dev);
	struct regmap *map = dev_get_regmap(dev, NULL);
	size_t addr_bytes;
	int ret = 0;
	u16 reg_addr = 0;

	if (map == NULL) {
		dev_err(dev, "%s: regmap is NULL\n", __func__);
		return -EINVAL;
	}
	addr_bytes = map->format.reg_bytes;
	if (swr == NULL) {
		dev_err(dev, "%s: swr is NULL\n", __func__);
		return -EINVAL;
	}
	if (reg_size != addr_bytes) {
		dev_err(dev, "%s: register size %zd bytes not supported\n",
			__func__, reg_size);
		return -EINVAL;
	}
	reg_addr = *(u16 *)reg;
	ret = swr_read(swr, swr->dev_num, reg_addr, val, val_size);
	if (ret < 0)
		dev_err(dev, "%s: codec reg 0x%x read failed %d\n",
			__func__, reg_addr, ret);
	return ret;
}

static struct regmap_bus regmap_swr = {
	.write = regmap_swr_write,
	.gather_write = regmap_swr_gather_write,
	.read = regmap_swr_read,
	.reg_format_endian_default = REGMAP_ENDIAN_NATIVE,
	.val_format_endian_default = REGMAP_ENDIAN_NATIVE,
};

struct regmap *__regmap_init_swr(struct swr_device *swr,
				 const struct regmap_config *config,
				 struct lock_class_key *lock_key,
				 const char *lock_name)
{
	return __regmap_init(&swr->dev, &regmap_swr, &swr->dev, config,
			   lock_key, lock_name);
}
EXPORT_SYMBOL(__regmap_init_swr);

struct regmap *__devm_regmap_init_swr(struct swr_device *swr,
				      const struct regmap_config *config,
				      struct lock_class_key *lock_key,
				      const char *lock_name)
{
	return __devm_regmap_init(&swr->dev, &regmap_swr, &swr->dev, config,
				lock_key, lock_name);
}
EXPORT_SYMBOL(__devm_regmap_init_swr);

MODULE_LICENSE("GPL v2");
