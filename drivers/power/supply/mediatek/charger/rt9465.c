/*
 * Copyright (C) 2016 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/of_device.h>
#include <linux/of_irq.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/mutex.h>
#include <linux/interrupt.h>
#include <linux/irq.h>

#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif

#include "mtk_charger_intf.h"
#include "rt9465.h"
#define I2C_ACCESS_MAX_RETRY	5
#define RT9465_DRV_VERSION	"1.0.11_MTK"

/* ======================= */
/* RT9465 Parameter        */
/* ======================= */

enum rt9465_charging_status {
	RT9465_CHG_STATUS_READY = 0,
	RT9465_CHG_STATUS_PROGRESS,
	RT9465_CHG_STATUS_DONE,
	RT9465_CHG_STATUS_FAULT,
	RT9465_CHG_STATUS_MAX,
};

/* Charging status name */
static const char *rt9465_chg_status_name[RT9465_CHG_STATUS_MAX] = {
	"ready", "progress", "done", "fault",
};

static const u8 rt9465_hidden_mode_val[] = {
	0x30, 0x26, 0xC7, 0xA4, 0x33, 0x06, 0x5F, 0xE1,
};

static const u32 rt9465_safety_timer[] = {
	4, 6, 8, 10, 12, 14, 16, 20,
};

enum rt9465_irq_idx {
	RT9465_IRQIDX_STATC = 0,
	RT9465_IRQIDX_FAULT,
	RT9465_IRQSTAT_MAX,
	RT9465_IRQIDX_IRQ1 = RT9465_IRQSTAT_MAX,
	RT9465_IRQIDX_IRQ2,
	RT9465_IRQIDX_MAX,
};

static u8 rt9465_irqmask[RT9465_IRQIDX_MAX] = {
	0x00, 0xE0, 0xFF, 0xFF
};

static const u8 rt9465_irq_maskall[RT9465_IRQIDX_MAX] = {
	0xE0, 0xE0, 0xFF, 0xFF
};

struct rt9465_desc {
	u32 ichg;		/* uA */
	u32 mivr;		/* uV */
	u32 cv;			/* uV */
	u32 ieoc;		/* uA */
	u32 safety_timer;	/* hour */
	bool en_te;
	bool en_wdt;
	bool en_st;
	int regmap_represent_slave_addr;
	const char *regmap_name;
	const char *chg_dev_name;
	const char *alias_name;
};

/* These default values will be used if there's no property in dts */
static struct rt9465_desc rt9465_default_desc = {
	.ichg = 2000000,	/* uA */
	.mivr = 4400000,	/* uV */
	.cv = 4350000,		/* uV */
	.ieoc = 250000,		/* uA */
	.safety_timer = 12,	/* hour */
	.en_te = true,
	.en_wdt = true,
	.en_st = true,
	.regmap_represent_slave_addr = RT9465_SLAVE_ADDR,
	.regmap_name = "rt9465",
	.chg_dev_name = "secondary_chg",
	.alias_name = "rt9465",
};

struct rt9465_info {
	struct i2c_client *i2c;
	struct rt9465_desc *desc;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct device *dev;
	struct mutex i2c_access_lock;
	struct mutex adc_access_lock;
	struct mutex gpio_access_lock;
	struct mutex irq_access_lock;
	struct mutex hidden_mode_lock;
	u32 intr_gpio;
	u32 en_gpio;
	int irq;
	u8 chip_rev;
	u32 hidden_mode_cnt;
	atomic_t is_chip_en;
	u8 irq_flag[RT9465_IRQIDX_MAX];
	u8 irq_stat[RT9465_IRQSTAT_MAX];
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap_dev;
	struct rt_regmap_properties *regmap_prop;
#endif /* CONFIG_RT_REGMAP */
};

static int rt9465_kick_wdt(struct charger_device *chg_dev);
static int rt9465_reserved_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_treg_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_mivr_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_pwr_rdy_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_vbatsuv_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_vbatov_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_vbusov_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_faulti_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_statci_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_temp_l_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_temp_h_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_tmri_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_adpbadi_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_otpi_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_wdtmri_handler(struct rt9465_info *info)
{
	int ret = 0;

	chr_info("%s\n", __func__);
	ret = rt9465_kick_wdt(info->chg_dev);
	if (ret < 0)
		chr_err("%s: kick wdt fail\n", __func__);

	return ret;
}

static int rt9465_ssfinishi_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_termi_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

static int rt9465_chg_ieoci_handler(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return 0;
}

struct irq_mapping_tbl {
	const char *name;
	int (*hdlr)(struct rt9465_info *info);
};

#define RT9465_IRQ_MAPPING(_name) \
	{.name = #_name, .hdlr = rt9465_ ## _name ## _handler}

static const struct irq_mapping_tbl rt9465_irq_mapping_tbl[] = {
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(chg_treg),
	RT9465_IRQ_MAPPING(chg_mivr),
	RT9465_IRQ_MAPPING(pwr_rdy),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(chg_vbatsuv),
	RT9465_IRQ_MAPPING(chg_vbatov),
	RT9465_IRQ_MAPPING(chg_vbusov),
	RT9465_IRQ_MAPPING(chg_faulti),
	RT9465_IRQ_MAPPING(chg_statci),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(temp_l),
	RT9465_IRQ_MAPPING(temp_h),
	RT9465_IRQ_MAPPING(chg_tmri),
	RT9465_IRQ_MAPPING(chg_adpbadi),
	RT9465_IRQ_MAPPING(chg_otpi),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(reserved),
	RT9465_IRQ_MAPPING(wdtmri),
	RT9465_IRQ_MAPPING(ssfinishi),
	RT9465_IRQ_MAPPING(chg_termi),
	RT9465_IRQ_MAPPING(chg_ieoci),
};

/* ======================= */
/* Address & Default value */
/* ======================= */

static const unsigned char rt9465_reg_addr[] = {
	RT9465_REG_CHG_CTRL0,
	RT9465_REG_CHG_CTRL1,
	RT9465_REG_CHG_CTRL2,
	RT9465_REG_CHG_CTRL3,
	RT9465_REG_CHG_CTRL4,
	RT9465_REG_CHG_CTRL5,
	RT9465_REG_CHG_CTRL6,
	RT9465_REG_CHG_CTRL7,
	RT9465_REG_CHG_CTRL8,
	RT9465_REG_CHG_CTRL9,
	RT9465_REG_CHG_CTRL10,
	RT9465_REG_CHG_CTRL12,
	RT9465_REG_CHG_CTRL13,
	RT9465_REG_HIDDEN_CTRL2,
	RT9465_REG_HIDDEN_CTRL6,
	RT9465_REG_SYSTEM1,
	RT9465_REG_CHG_STATC,
	RT9465_REG_CHG_FAULT,
	RT9465_REG_CHG_IRQ1,
	RT9465_REG_CHG_IRQ2,
	RT9465_REG_CHG_STATC_MASK,
	RT9465_REG_CHG_FAULT_MASK,
	RT9465_REG_CHG_IRQ1_MASK,
	RT9465_REG_CHG_IRQ2_MASK,
};

/* ========= */
/* RT Regmap */
/* ========= */

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT9465_REG_CHG_CTRL0, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL4, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL5, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL6, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL7, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL8, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL9, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL10, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL12, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_CTRL13, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_HIDDEN_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_HIDDEN_CTRL6, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_SYSTEM1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_STATC, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_FAULT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_IRQ1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_IRQ2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_STATC_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_FAULT_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_IRQ1_MASK, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9465_REG_CHG_IRQ2_MASK, 1, RT_VOLATILE, {});

static const rt_register_map_t rt9465_regmap_map[] = {
	RT_REG(RT9465_REG_CHG_CTRL0),
	RT_REG(RT9465_REG_CHG_CTRL1),
	RT_REG(RT9465_REG_CHG_CTRL2),
	RT_REG(RT9465_REG_CHG_CTRL3),
	RT_REG(RT9465_REG_CHG_CTRL4),
	RT_REG(RT9465_REG_CHG_CTRL5),
	RT_REG(RT9465_REG_CHG_CTRL6),
	RT_REG(RT9465_REG_CHG_CTRL7),
	RT_REG(RT9465_REG_CHG_CTRL8),
	RT_REG(RT9465_REG_CHG_CTRL9),
	RT_REG(RT9465_REG_CHG_CTRL10),
	RT_REG(RT9465_REG_CHG_CTRL12),
	RT_REG(RT9465_REG_CHG_CTRL13),
	RT_REG(RT9465_REG_HIDDEN_CTRL2),
	RT_REG(RT9465_REG_HIDDEN_CTRL6),
	RT_REG(RT9465_REG_SYSTEM1),
	RT_REG(RT9465_REG_CHG_STATC),
	RT_REG(RT9465_REG_CHG_FAULT),
	RT_REG(RT9465_REG_CHG_IRQ1),
	RT_REG(RT9465_REG_CHG_IRQ2),
	RT_REG(RT9465_REG_CHG_STATC_MASK),
	RT_REG(RT9465_REG_CHG_FAULT_MASK),
	RT_REG(RT9465_REG_CHG_IRQ1_MASK),
	RT_REG(RT9465_REG_CHG_IRQ2_MASK),
};
#endif /* CONFIG_RT_REGMAP */

/* ========================= */
/* I2C operations            */
/* ========================= */

static inline bool __rt9465_is_chip_en(struct rt9465_info *info)
{
	int en = 0;

	en = gpio_get_value(info->en_gpio);
	if ((en && !atomic_read(&info->is_chip_en)) ||
		(!en && atomic_read(&info->is_chip_en)))
		chr_err("%s: en not sync(%d, %d)\n", __func__, en,
			atomic_read(&info->is_chip_en));

	return en;
}

static int rt9465_device_read(void *client, u32 addr, int leng, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_read_i2c_block_data(i2c, addr, leng, dst);
}

static int rt9465_device_write(void *client, u32 addr, int leng,
	const void *src)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_write_i2c_block_data(i2c, addr, leng, src);
}

#ifdef CONFIG_RT_REGMAP
static struct rt_regmap_fops rt9465_regmap_fops = {
	.read_device = rt9465_device_read,
	.write_device = rt9465_device_write,
};

static int rt9465_register_rt_regmap(struct rt9465_info *info)
{
	int ret = 0;
	struct i2c_client *i2c = info->i2c;
	struct rt_regmap_properties *prop = NULL;

	chr_info("%s\n", __func__);

	prop = devm_kzalloc(&i2c->dev, sizeof(struct rt_regmap_properties),
		GFP_KERNEL);
	if (!prop)
		return -ENOMEM;

	prop->name = info->desc->regmap_name;
	prop->aliases = info->desc->regmap_name;
	prop->register_num = ARRAY_SIZE(rt9465_regmap_map);
	prop->rm = rt9465_regmap_map;
	prop->rt_regmap_mode = RT_SINGLE_BYTE | RT_CACHE_DISABLE |
		RT_IO_PASS_THROUGH;
	prop->io_log_en = 0;

	info->regmap_prop = prop;
	info->regmap_dev = rt_regmap_device_register_ex(info->regmap_prop,
		&rt9465_regmap_fops, &i2c->dev, i2c,
		info->desc->regmap_represent_slave_addr, info);

	if (!info->regmap_dev) {
		chr_err("register regmap device failed\n");
		return -EIO;
	}

	return ret;
}
#endif /* CONFIG_RT_REGMAP */

static inline int __rt9465_i2c_write_byte(struct rt9465_info *info, u8 cmd,
	u8 data)
{
	int ret = 0, retry = 0;

	do {
#ifdef CONFIG_RT_REGMAP
		ret = rt_regmap_block_write(info->regmap_dev, cmd, 1, &data);
#else
		ret = rt9465_device_write(info->i2c, cmd, 1, &data);
#endif
		retry++;
		if (ret < 0)
			udelay(10);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);

	if (ret < 0)
		chr_err("%s: I2CW[0x%02X] = 0x%02X failed\n",
			__func__, cmd, data);
	else
		dev_dbg_ratelimited(info->dev, "%s: I2CW[0x%02X] = 0x%02X\n",
			__func__, cmd, data);

	return ret;
}

static int rt9465_i2c_write_byte(struct rt9465_info *info, u8 cmd, u8 data)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	mutex_lock(&info->gpio_access_lock);
	if (__rt9465_is_chip_en(info))
		ret = __rt9465_i2c_write_byte(info, cmd, data);
	else
		ret = -EINVAL;
	mutex_unlock(&info->gpio_access_lock);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static inline int __rt9465_i2c_read_byte(struct rt9465_info *info, u8 cmd)
{
	int ret = 0, ret_val = 0, retry = 0;

	do {
#ifdef CONFIG_RT_REGMAP
		ret = rt_regmap_block_read(info->regmap_dev, cmd, 1, &ret_val);
#else
		ret = rt9465_device_read(info->i2c, cmd, 1, &ret_val);
#endif
		retry++;
		if (ret < 0)
			udelay(10);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);

	if (ret < 0) {
		chr_err("%s: I2CR[0x%02X] failed\n", __func__, cmd);
		return ret;
	}

	ret_val = ret_val & 0xFF;

	dev_dbg_ratelimited(info->dev, "%s: I2CR[0x%02X] = 0x%02X\n", __func__,
		cmd, ret_val);

	return ret_val;
}

static int rt9465_i2c_read_byte(struct rt9465_info *info, u8 cmd)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	mutex_lock(&info->gpio_access_lock);
	if (__rt9465_is_chip_en(info))
		ret = __rt9465_i2c_read_byte(info, cmd);
	else
		ret = -EINVAL;
	mutex_unlock(&info->gpio_access_lock);
	mutex_unlock(&info->i2c_access_lock);

	if (ret < 0)
		return ret;

	return (ret & 0xFF);
}

static inline int __rt9465_i2c_block_write(struct rt9465_info *info, u8 cmd,
	u32 leng, const u8 *data)
{
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(info->regmap_dev, cmd, leng, data);
#else
	ret = rt9465_device_write(info->i2c, cmd, leng, data);
#endif

	return ret;
}

static int rt9465_i2c_block_write(struct rt9465_info *info, u8 cmd, u32 leng,
	const u8 *data)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	mutex_lock(&info->gpio_access_lock);
	if (__rt9465_is_chip_en(info))
		ret = __rt9465_i2c_block_write(info, cmd, leng, data);
	else
		ret = -EINVAL;
	mutex_unlock(&info->gpio_access_lock);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static inline int __rt9465_i2c_block_read(struct rt9465_info *info, u8 cmd,
	u32 leng, u8 *data)
{
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(info->regmap_dev, cmd, leng, data);
#else
	ret = rt9465_device_read(info->i2c, cmd, leng, data);
#endif

	return ret;
}

static int rt9465_i2c_block_read(struct rt9465_info *info, u8 cmd, u32 leng,
	u8 *data)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	mutex_lock(&info->gpio_access_lock);
	if (__rt9465_is_chip_en(info))
		ret = __rt9465_i2c_block_read(info, cmd, leng, data);
	else
		ret = -EINVAL;
	mutex_unlock(&info->gpio_access_lock);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static int rt9465_i2c_test_bit(struct rt9465_info *info, u8 cmd, u8 shift,
	bool *is_one)
{
	int ret = 0;
	u8 data = 0;

	ret = rt9465_i2c_read_byte(info, cmd);
	if (ret < 0) {
		*is_one = false;
		return ret;
	}

	data = ret & (1 << shift);
	*is_one = (data == 0 ? false : true);

	return ret;
}

static int rt9465_i2c_update_bits(struct rt9465_info *info, u8 cmd, u8 data,
	u8 mask)
{
	int ret = 0;
	u8 reg_data = 0;

	mutex_lock(&info->i2c_access_lock);
	mutex_lock(&info->gpio_access_lock);
	if (__rt9465_is_chip_en(info)) {
		ret = __rt9465_i2c_read_byte(info, cmd);
		if (ret < 0)
			goto out;

		reg_data = ret & 0xFF;
		reg_data &= ~mask;
		reg_data |= (data & mask);

		ret = __rt9465_i2c_write_byte(info, cmd, reg_data);
	} else
		ret = -EINVAL;

out:
	mutex_unlock(&info->gpio_access_lock);
	mutex_unlock(&info->i2c_access_lock);
	return ret;
}

static inline int rt9465_set_bit(struct rt9465_info *info, u8 reg, u8 mask)
{
	return rt9465_i2c_update_bits(info, reg, mask, mask);
}

static inline int rt9465_clr_bit(struct rt9465_info *info, u8 reg, u8 mask)
{
	return rt9465_i2c_update_bits(info, reg, 0x00, mask);
}

/* ================== */
/* Internal Functions */
/* ================== */

/* The following APIs will be reference in internal functions */
static int rt9465_get_ichg(struct charger_device *chg_dev, u32 *uA);
static int rt9465_dump_register(struct charger_device *chg_dev);

static inline u8 rt9465_closest_reg(u32 min, u32 max, u32 step, u32 target)
{
	/* Smaller than minimum supported value, use minimum one */
	if (target < min)
		return 0;

	/* Greater than maximum supported value, use maximum one */
	if (target >= max)
		return (max - min) / step;

	return (target - min) / step;
}

static inline u32 rt9465_closest_value(u32 min, u32 max, u32 step, u8 reg)
{
	u32 ret_val = 0;

	ret_val = min + reg * step;

	return ret_val > max ? max : ret_val;
}

static inline u8 rt9465_closest_reg_via_tbl(const u32 *tbl, u32 tbl_size,
	u32 target)
{
	u32 i = 0;

	/* Smaller than minimum supported value, use minimum one */
	if (target < tbl[0])
		return 0;

	for (i = 0; i < tbl_size - 1; i++) {
		if (target >= tbl[i] && target < tbl[i + 1])
			return i;
	}

	/* Greater than maximum supported value, use maximum one */
	return tbl_size - 1;
}

static inline void rt9465_irq_set_flag(struct rt9465_info *info, u8 *irq,
	u8 mask)
{
	mutex_lock(&info->irq_access_lock);
	*irq |= mask;
	mutex_unlock(&info->irq_access_lock);
}

static inline void rt9465_irq_clr_flag(struct rt9465_info *info, u8 *irq,
	u8 mask)
{
	mutex_lock(&info->irq_access_lock);
	*irq &= ~mask;
	mutex_unlock(&info->irq_access_lock);
}

static inline const char *rt9465_get_irq_name(struct rt9465_info *info,
	int irqnum)
{
	if (irqnum >= 0 && irqnum < ARRAY_SIZE(rt9465_irq_mapping_tbl))
		return rt9465_irq_mapping_tbl[irqnum].name;
	return "not found";
}

static inline void rt9465_irq_mask(struct rt9465_info *info, int irqnum)
{
	dev_dbg(info->dev, "%s: irq = %d, %s\n", __func__, irqnum,
		rt9465_get_irq_name(info, irqnum));
	rt9465_irqmask[irqnum / 8] |= (1 << (irqnum % 8));
}

static inline void rt9465_irq_unmask(struct rt9465_info *info, int irqnum)
{
	dev_dbg(info->dev, "%s: irq = %d, %s\n", __func__, irqnum,
		rt9465_get_irq_name(info, irqnum));
	rt9465_irqmask[irqnum / 8] &= ~(1 << (irqnum % 8));
}

static int rt9465_enable_hidden_mode(struct rt9465_info *info, bool en)
{
	int ret = 0;

	mutex_lock(&info->hidden_mode_lock);

	if (en) {
		if (info->hidden_mode_cnt == 0) {
			ret = rt9465_i2c_block_write(info, 0x50,
				ARRAY_SIZE(rt9465_hidden_mode_val),
				rt9465_hidden_mode_val);
			if (ret < 0)
				goto err;
		}
		info->hidden_mode_cnt++;
	} else {
		if (info->hidden_mode_cnt == 1) /* last one */
			ret = rt9465_i2c_write_byte(info, 0x50, 0x00);
		info->hidden_mode_cnt--;
		if (ret < 0)
			goto err;
	}
	chr_info("%s: en = %d\n", __func__, en);
	goto out;

err:
	chr_err("%s: en = %d fail(%d)\n", __func__, en, ret);
out:
	mutex_unlock(&info->hidden_mode_lock);
	return ret;
}

static int rt9465_sw_workaround(struct rt9465_info *info)
{
	int ret = 0;

	chr_info("%s\n", __func__);

	/* Enter hidden mode */
	rt9465_enable_hidden_mode(info, true);

	/* For chip rev == E2, set Hidden code to make ICHG accurate */
	if (info->chip_rev == RT9465_VERSION_E2) {
		ret = rt9465_i2c_write_byte(info, RT9465_REG_HIDDEN_CTRL2,
			0x68);
		if (ret < 0)
			goto out;
		ret = rt9465_i2c_write_byte(info, RT9465_REG_HIDDEN_CTRL6,
			0x3A);
		if (ret < 0)
			goto out;
	}

out:
	rt9465_enable_hidden_mode(info, false);
	return ret;
}

static irqreturn_t rt9465_irq_handler(int irq, void *data)
{
	int ret = 0, i = 0, j = 0;
	u8 evt[RT9465_IRQIDX_MAX] = {0};
	u8 mask[RT9465_IRQIDX_MAX] = {0};
	u8 stat[RT9465_IRQSTAT_MAX] = {0};
	struct rt9465_info *info = (struct rt9465_info *)data;

	chr_info("%s\n", __func__);

	/* read event */
	ret = rt9465_i2c_block_read(info, RT9465_REG_CHG_STATC, ARRAY_SIZE(evt),
		evt);
	if (ret < 0) {
		chr_err("%s: read evt fail\n", __func__);
		goto err;
	}

	/* read mask */
	ret = rt9465_i2c_block_read(info, RT9465_REG_CHG_STATC_MASK,
		ARRAY_SIZE(mask), mask);
	if (ret < 0) {
		chr_err("%s: read mask fail\n", __func__);
		goto err;
	}

	/* Store stat */
	memcpy(stat, info->irq_stat, RT9465_IRQSTAT_MAX);

	for (i = 0; i < RT9465_IRQIDX_MAX; i++) {
		evt[i] &= ~mask[i];
		if (i < RT9465_IRQSTAT_MAX) {
			info->irq_stat[i] = evt[i];
			evt[i] ^= stat[i];
		}
		for (j = 0; j < 8; j++) {
			if (!(evt[i] & (1 << j)))
				continue;
			if (rt9465_irq_mapping_tbl[i * 8 + j].hdlr)
				(rt9465_irq_mapping_tbl[i * 8 + j].hdlr)(info);
		}
	}

err:
	return IRQ_HANDLED;
}

static int rt9465_register_irq(struct rt9465_info *info)
{
	int ret = 0, len = 0;
	char *name = NULL;

	/* request gpio */
	len = strlen(info->desc->chg_dev_name);
	name = devm_kzalloc(info->dev, len + 10, GFP_KERNEL);
	snprintf(name,  len + 10, "%s_irq_gpio", info->desc->chg_dev_name);
	ret = devm_gpio_request_one(info->dev, info->intr_gpio, GPIOF_IN, name);
	if (ret < 0) {
		chr_err("%s: gpio request fail\n", __func__);
		goto err;
	}

	ret = gpio_to_irq(info->intr_gpio);
	if (ret < 0) {
		chr_err("%s: irq mapping fail\n", __func__);
		goto err;
	}
	info->irq = ret;
	chr_info("%s: irq = %d\n", __func__, info->irq);

	/* Request threaded IRQ */
	ret = devm_request_threaded_irq(info->dev, info->irq, NULL,
		rt9465_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, name,
		info);
	if (ret < 0) {
		chr_err("%s: request thread irq failed\n", __func__);
		goto err;
	}
	device_init_wakeup(info->dev, true);

	return 0;

err:
	return ret;
}

static int rt9465_maskall_irq(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return rt9465_i2c_block_write(info, RT9465_REG_CHG_STATC_MASK,
		ARRAY_SIZE(rt9465_irq_maskall), rt9465_irq_maskall);
}

static int rt9465_init_irq(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return rt9465_i2c_block_write(info, RT9465_REG_CHG_STATC,
		ARRAY_SIZE(rt9465_irqmask), rt9465_irqmask);
}

static bool rt9465_is_hw_exist(struct rt9465_info *info)
{
	int ret = 0;
	u8 version = 0;

	ret = i2c_smbus_read_byte_data(info->i2c, RT9465_REG_SYSTEM1);
	version = (ret & RT9465_MASK_VERSION) >> RT9465_SHIFT_VERSION;
	chr_info("%s: E%d(0x%02X)\n", __func__, version + 1,
		version);

	if (version < RT9465_VERSION_E5) {
		chr_err("%s: chip version is incorrect\n", __func__);
		return false;
	}

	info->chip_rev = version;
	return true;
}

static int rt9465_set_safety_timer(struct rt9465_info *info, u32 hr)
{
	u8 reg_st = 0;

	reg_st = rt9465_closest_reg_via_tbl(rt9465_safety_timer,
		ARRAY_SIZE(rt9465_safety_timer), hr);

	chr_info("%s: st = %d(0x%02X)\n", __func__, hr, reg_st);

	return rt9465_i2c_update_bits(info, RT9465_REG_CHG_CTRL9,
		reg_st << RT9465_SHIFT_WT_FC, RT9465_MASK_WT_FC);
}

static inline int rt9465_enable_wdt(struct rt9465_info *info, bool en)
{
	chr_info("%s: en = %d\n", __func__, en);
	return (en ? rt9465_set_bit : rt9465_clr_bit)
		(info, RT9465_REG_CHG_CTRL10, RT9465_MASK_WDT_EN);
}

static inline int rt9465_reset_chip(struct rt9465_info *info)
{
	chr_info("%s\n", __func__);
	return rt9465_i2c_write_byte(info, RT9465_REG_CHG_CTRL0, 0x80);
}

static inline int rt9465_enable_te(struct rt9465_info *info, bool en)
{
	chr_info("%s: en = %d\n", __func__, en);
	return (en ? rt9465_set_bit : rt9465_clr_bit)
		(info, RT9465_REG_CHG_CTRL8, RT9465_MASK_TE_EN);
}

static int rt9465_set_ieoc(struct rt9465_info *info, u32 ieoc)
{
	u8 reg_ieoc = 0;

	/* Workaround for E1, IEOC must >= 700mA */
	if (ieoc < 700000 && info->chip_rev == RT9465_VERSION_E1)
		ieoc = 700000;

	/* Find corresponding reg value */
	reg_ieoc = rt9465_closest_reg(RT9465_IEOC_MIN, RT9465_IEOC_MAX,
		RT9465_IEOC_STEP, ieoc);

	/* ieoc starts from 600mA and its register value is 0x05 */
	reg_ieoc += 0x05;

	chr_info("%s: ieoc = %d(0x%02X)\n", __func__, ieoc,
		reg_ieoc);

	return rt9465_i2c_update_bits(info, RT9465_REG_CHG_CTRL7,
		reg_ieoc << RT9465_SHIFT_IEOC, RT9465_MASK_IEOC);
}

static int __rt9465_get_mivr(struct rt9465_info *info, u32 *mivr)
{
	int ret = 0;
	u8 reg_mivr = 0;

	ret = rt9465_i2c_read_byte(info, RT9465_REG_CHG_CTRL5);
	if (ret < 0)
		return ret;

	reg_mivr = ((ret & RT9465_MASK_MIVR) >> RT9465_SHIFT_MIVR) & 0xFF;
	*mivr = rt9465_closest_value(RT9465_MIVR_MIN, RT9465_MIVR_MAX,
		RT9465_MIVR_STEP, reg_mivr);

	return ret;
}

static int rt9465_get_charging_status(struct rt9465_info *info,
	enum rt9465_charging_status *chg_stat)
{
	int ret = 0;

	ret = rt9465_i2c_read_byte(info, RT9465_REG_SYSTEM1);
	if (ret < 0)
		return ret;

	*chg_stat = (ret & RT9465_MASK_CHG_STAT) >> RT9465_SHIFT_CHG_STAT;

	return ret;
}

static int rt9465_get_ieoc(struct rt9465_info *info, u32 *ieoc)
{
	int ret = 0;
	u8 reg_ieoc = 0;

	ret = rt9465_i2c_read_byte(info, RT9465_REG_CHG_CTRL7);
	if (ret < 0)
		return ret;

	reg_ieoc = (ret & RT9465_MASK_IEOC) >> RT9465_SHIFT_IEOC;

	/* ieoc starts from 600mA and its register value is 0x05 */
	reg_ieoc -= 0x05;
	*ieoc = rt9465_closest_value(RT9465_IEOC_MIN, RT9465_IEOC_MAX,
		RT9465_IEOC_STEP, reg_ieoc);

	return ret;
}

static inline int rt9465_get_irq_number(struct rt9465_info *info,
	const char *name)
{
	int i = 0;

	if (!name) {
		chr_err("%s: null name\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(rt9465_irq_mapping_tbl); i++) {
		if (!strcmp(name, rt9465_irq_mapping_tbl[i].name))
			return i;
	}

	return -EINVAL;
}

static int rt9465_parse_dt(struct rt9465_info *info, struct device *dev)
{
	int ret = 0, irqcnt = 0, irqnum = 0;
	struct rt9465_desc *desc = NULL;
	struct device_node *np = dev->of_node;
	const char *name = NULL;
	int len = 0;
	char *en_name = NULL;

	chr_info("%s\n", __func__);

	if (!np) {
		chr_err("%s: no device node\n", __func__);
		return -EINVAL;
	}

	info->desc = &rt9465_default_desc;

	desc = devm_kzalloc(dev, sizeof(struct rt9465_desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	memcpy(desc, &rt9465_default_desc, sizeof(struct rt9465_desc));

	/*
	 * For dual charger, one is primary_chg;
	 * another one will be secondary_chg
	 */
	if (of_property_read_string(np, "charger_name",
		&desc->chg_dev_name) < 0)
		chr_err("%s: no charger name\n", __func__);

	if (of_property_read_u32(np, "regmap_represent_slave_addr",
		&(desc->regmap_represent_slave_addr)) < 0)
		chr_err("%s: no regmap represent slave addr\n",
			__func__);

	if (of_property_read_string(np, "regmap_name", &desc->regmap_name) < 0)
		chr_err("%s: no regmap name\n", __func__);

	if (of_property_read_string(np, "alias_name", &desc->alias_name) < 0)
		chr_err("%s: no alias name\n", __func__);

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "rt,en_gpio", 0);
	if (ret < 0)
		return ret;
	info->en_gpio = ret;
#else
	ret = of_property_read_u32(np, "rt,en_gpio_num", &info->en_gpio);
	if (ret < 0)
		return ret;
#endif /* !CONFIG_MTK_GPIO || CONFIG_MTK_GPIOLIB_STAND */

	chr_info("%s: intr/en gpio = %d, %d\n", __func__,
		info->intr_gpio, info->en_gpio);

	/* request en gpio */
	len = strlen(desc->chg_dev_name);
	en_name = devm_kzalloc(info->dev, len + 9, GFP_KERNEL);
	snprintf(en_name, len + 9, "%s_en_gpio", desc->chg_dev_name);
	ret = devm_gpio_request_one(info->dev, info->en_gpio, GPIOF_DIR_OUT,
		en_name);
	if (ret < 0) {
		chr_err("%s: en gpio request fail\n", __func__);
		return ret;
	}

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(np, "rt,intr_gpio", 0);
	if (ret < 0)
		return ret;
	info->intr_gpio = ret;
#else
	ret = of_property_read_u32(np, "rt,intr_gpio_num", &info->intr_gpio);
	if (ret < 0)
		return ret;
#endif
	chr_info("%s: intr/en gpio = %d, %d\n", __func__,
		info->intr_gpio, info->en_gpio);

	if (of_property_read_u32(np, "ichg", &desc->ichg) < 0)
		chr_err("%s: no ichg\n", __func__);

	if (of_property_read_u32(np, "mivr", &desc->mivr) < 0)
		chr_err("%s: no mivr\n", __func__);

	if (of_property_read_u32(np, "cv", &desc->cv) < 0)
		chr_err("%s: no cv\n", __func__);

	if (of_property_read_u32(np, "ieoc", &desc->ieoc) < 0)
		chr_err("%s: no ieoc\n", __func__);

	if (of_property_read_u32(np, "safety_timer", &desc->safety_timer) < 0)
		chr_err("%s: no safety timer\n", __func__);

	desc->en_te = of_property_read_bool(np, "en_te");
	desc->en_wdt = of_property_read_bool(np, "en_wdt");
	desc->en_st = of_property_read_bool(np, "en_st");

	while (true) {
		ret = of_property_read_string_index(np, "interrupt-names",
			irqcnt, &name);
		if (ret < 0)
			break;
		irqcnt++;
		irqnum = rt9465_get_irq_number(info, name);
		if (irqnum >= 0)
			rt9465_irq_unmask(info, irqnum);
	}

	info->desc = desc;
	info->chg_props.alias_name = info->desc->alias_name;
	chr_info("%s: chg_name:%s alias:%s\n", __func__,
		info->desc->chg_dev_name, info->chg_props.alias_name);

	return 0;
}

static int __rt9465_enable_chip(struct rt9465_info *info, bool en)
{
	bool is_chip_en = false;

	chr_info("%s: en = %d\n", __func__, en);

	mutex_lock(&info->gpio_access_lock);
	is_chip_en = __rt9465_is_chip_en(info);
	if (en && !is_chip_en) {
		gpio_set_value(info->en_gpio, 1);
		chr_info("%s: set gpio high\n", __func__);
	} else if (!en && is_chip_en) {
		gpio_set_value(info->en_gpio, 0);
		chr_info("%s: set gpio low\n", __func__);
	}

	/* Wait for chip's enable/disable */
	mdelay(1);
	atomic_set(&info->is_chip_en, en);
	mutex_unlock(&info->gpio_access_lock);
	return 0;
}

static int __rt9465_set_ichg(struct rt9465_info *info, u32 uA)
{
	u8 reg_ichg = 0;

	/* Workaround for E1, Ichg must >= 1000mA */
	if (uA < 1000000 && info->chip_rev == RT9465_VERSION_E1)
		uA = 1000000;

	/* Find corresponding reg value */
	reg_ichg = rt9465_closest_reg(RT9465_ICHG_MIN, RT9465_ICHG_MAX,
		RT9465_ICHG_STEP, uA);

	/* ichg starts from 600mA and its register value is 0x06 */
	reg_ichg += 0x06;

	chr_info("%s: ichg = %d(0x%02X)\n", __func__, uA, reg_ichg);

	return rt9465_i2c_update_bits(info, RT9465_REG_CHG_CTRL6,
		reg_ichg << RT9465_SHIFT_ICHG, RT9465_MASK_ICHG);
}

static int __rt9465_set_mivr(struct rt9465_info *info, u32 uV)
{
	u8 reg_mivr = 0;

	/* Find corresponding reg value */
	reg_mivr = rt9465_closest_reg(RT9465_MIVR_MIN, RT9465_MIVR_MAX,
		RT9465_MIVR_STEP, uV);

	chr_info("%s: mivr = %d(0x%02X)\n", __func__, uV, reg_mivr);

	return rt9465_i2c_update_bits(info, RT9465_REG_CHG_CTRL5,
		reg_mivr << RT9465_SHIFT_MIVR, RT9465_MASK_MIVR);
}

static int __rt9465_set_cv(struct rt9465_info *info, u32 uV)
{
	u8 reg_cv = 0;

	reg_cv = rt9465_closest_reg(RT9465_BAT_VOREG_MIN, RT9465_BAT_VOREG_MAX,
		RT9465_BAT_VOREG_STEP, uV);

	chr_info("%s: cv = %d(0x%02X)\n", __func__, uV, reg_cv);

	return rt9465_i2c_update_bits(info, RT9465_REG_CHG_CTRL3,
		reg_cv << RT9465_SHIFT_BAT_VOREG, RT9465_MASK_BAT_VOREG);
}

static int __rt9465_enable_safety_timer(struct rt9465_info *info, bool en)
{
	chr_info("%s: en = %d\n", __func__, en);
	return (en ? rt9465_set_bit : rt9465_clr_bit)
		(info, RT9465_REG_CHG_CTRL9, RT9465_MASK_TMR_EN);
}

static int rt9465_init_setting(struct rt9465_info *info)
{
	int ret = 0;
	u8 evt[RT9465_IRQIDX_MAX] = {0};
	struct rt9465_desc *desc = info->desc;

	chr_info("%s\n", __func__);

	ret = rt9465_maskall_irq(info);
	if (ret < 0) {
		chr_err("%s: mask all irq fail\n", __func__);
		goto err;
	}

	/* clear evt */
	ret = rt9465_i2c_block_read(info, RT9465_REG_CHG_STATC, ARRAY_SIZE(evt),
		evt);
	if (ret < 0) {
		chr_err("%s: read evt fail\n", __func__);
		goto err;
	}

	ret = __rt9465_set_ichg(info, desc->ichg);
	if (ret < 0)
		chr_err("%s: set ichg fail\n", __func__);

	ret = __rt9465_set_mivr(info, desc->mivr);
	if (ret < 0)
		chr_err("%s: set mivr fail\n", __func__);

	ret = rt9465_set_ieoc(info, desc->ieoc);
	if (ret < 0)
		chr_err("%s: set ieoc fail\n", __func__);

	ret = __rt9465_set_cv(info, desc->cv);
	if (ret < 0)
		chr_err("%s: set cv fail\n", __func__);

	ret = rt9465_enable_te(info, desc->en_te);
	if (ret < 0)
		chr_err("%s: set te fail\n", __func__);

	ret = rt9465_set_safety_timer(info, desc->safety_timer);
	if (ret < 0)
		chr_err("%s: set safety timer fail\n", __func__);

	ret = __rt9465_enable_safety_timer(info, desc->en_st);
	if (ret < 0)
		chr_err("%s: enable charger timer fail\n", __func__);

	ret = rt9465_enable_wdt(info, desc->en_wdt);
	if (ret < 0)
		chr_err("%s: enable watchdog fail\n", __func__);

err:
	return ret;
}

/* =========================================================== */
/* The following is released interfaces                        */
/* =========================================================== */

static int rt9465_enable_chip(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = __rt9465_enable_chip(info, en);
	if (ret < 0)
		return ret;

	if (!en)
		return 0;

	/* Do the following flow for enabling chip */
	if (!rt9465_is_hw_exist(info)) {
		chr_info("%s: no rt9465 exists\n", __func__);
		return -ENODEV;
	}

	ret = rt9465_init_setting(info);
	if (ret < 0)
		chr_info("%s: init fail(%d)\n", __func__, ret);

	ret = rt9465_sw_workaround(info);
	if (ret < 0)
		chr_info("%s: sw wkard fail(%d)\n", __func__, ret);

	ret = rt9465_init_irq(info);
	if (ret < 0)
		chr_info("%s: init irq fail(%d)\n", __func__, ret);

	rt9465_dump_register(info->chg_dev);

	return ret;
}

static int rt9465_is_chip_enabled(struct charger_device *chg_dev, bool *en)
{
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->gpio_access_lock);
	*en = __rt9465_is_chip_en(info);
	mutex_unlock(&info->gpio_access_lock);

	return 0;
}

static int rt9465_is_charging_enabled(struct charger_device *chg_dev, bool *en)
{
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	return rt9465_i2c_test_bit(info, RT9465_REG_CHG_CTRL1,
		RT9465_SHIFT_CHG_EN, en);
}

static int rt9465_dump_register(struct charger_device *chg_dev)
{
	int i = 0, ret = 0;
	int ichg = 0;
	u32 mivr = 0, ieoc = 0;
	bool chg_enable = 0;
	enum rt9465_charging_status chg_status = RT9465_CHG_STATUS_READY;
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);
	u8 chg_stat = 0;

	ret = rt9465_get_ichg(chg_dev, &ichg);
	ret = __rt9465_get_mivr(info, &mivr);
	ret = rt9465_is_charging_enabled(chg_dev, &chg_enable);
	ret = rt9465_get_ieoc(info, &ieoc);
	ret = rt9465_get_charging_status(info, &chg_status);
	chg_stat = rt9465_i2c_read_byte(info, RT9465_REG_CHG_STATC);

	/* Dump register if in fault status */
	if (chg_status == RT9465_CHG_STATUS_FAULT) {
		for (i = 0; i < ARRAY_SIZE(rt9465_reg_addr); i++)
			ret = rt9465_i2c_read_byte(info, rt9465_reg_addr[i]);
	}

	chr_info("%s: ICHG = %dmA, MIVR = %dmV, IEOC = %dmA\n",
		__func__, ichg / 1000, mivr / 1000, ieoc / 1000);

	chr_info("%s: CHG_EN = %d, CHG_STATUS = %s, CHG_STAT = 0x%02X\n",
		__func__, chg_enable, rt9465_chg_status_name[chg_status],
		chg_stat);

	return ret;
}

static int rt9465_enable_charging(struct charger_device *chg_dev, bool en)
{
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	return (en ? rt9465_set_bit : rt9465_clr_bit)
		(info, RT9465_REG_CHG_CTRL1, RT9465_MASK_CHG_EN);
}

static int rt9465_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9465_enable_safety_timer(info, en);
}

static int rt9465_set_ichg(struct charger_device *chg_dev, u32 uA)
{
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9465_set_ichg(info, uA);
}

static int rt9465_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9465_set_mivr(info, uV);
}

static int rt9465_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	return rt9465_i2c_test_bit(info, RT9465_REG_CHG_STATC,
				   RT9465_SHIFT_CHG_MIVR, in_loop);
}

static int rt9465_set_cv(struct charger_device *chg_dev, u32 uV)
{
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9465_set_cv(info, uV);
}

static int rt9465_get_ichg(struct charger_device *chg_dev, u32 *uA)
{
	int ret = 0;
	u8 reg_ichg = 0;
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9465_i2c_read_byte(info, RT9465_REG_CHG_CTRL6);
	if (ret < 0)
		return ret;

	reg_ichg = (ret & RT9465_MASK_ICHG) >> RT9465_SHIFT_ICHG;
	reg_ichg -= 0x06;
	*uA = rt9465_closest_value(RT9465_ICHG_MIN, RT9465_ICHG_MAX,
		RT9465_ICHG_STEP, reg_ichg);

	return ret;
}

static int rt9465_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	*uA = rt9465_closest_value(RT9465_ICHG_MIN, RT9465_ICHG_MAX,
		RT9465_ICHG_STEP, 0);
	return 0;
}

static int rt9465_get_mivr(struct charger_device *chg_dev, u32 *uV)
{
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9465_get_mivr(info, uV);
}

static int rt9465_get_tchg(struct charger_device *chg_dev,
	int *tchg_min, int *tchg_max)
{
	int ret = 0, reg_adc_temp = 0, adc_temp = 0;
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->adc_access_lock);

	/* Get value from ADC */
	ret = rt9465_i2c_read_byte(info, RT9465_REG_CHG_CTRL12);
	if (ret < 0)
		goto out;

	reg_adc_temp = (ret & RT9465_MASK_ADC_RPT) >> RT9465_SHIFT_ADC_RPT;
	if (reg_adc_temp == 0x00) {
		*tchg_min = 0;
		*tchg_max = 60;
	} else {
		reg_adc_temp -= 0x01;
		adc_temp = rt9465_closest_value(RT9465_ADC_RPT_MIN,
			RT9465_ADC_RPT_MAX, RT9465_ADC_RPT_STEP, reg_adc_temp);
		*tchg_min = adc_temp + 1;
		*tchg_max = adc_temp + RT9465_ADC_RPT_STEP;
	}

	chr_info("%s: %d < temperature <= %d\n", __func__, *tchg_min,
		*tchg_max);

out:
	mutex_unlock(&info->adc_access_lock);
	return ret;
}

static int rt9465_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	int ret = 0;
	enum rt9465_charging_status chg_stat = RT9465_CHG_STATUS_READY;
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9465_get_charging_status(info, &chg_stat);

	/* Return is charging done or not */
	switch (chg_stat) {
	case RT9465_CHG_STATUS_READY:
	case RT9465_CHG_STATUS_PROGRESS:
	case RT9465_CHG_STATUS_FAULT:
		*done = false;
		break;
	case RT9465_CHG_STATUS_DONE:
		*done = true;
		break;
	default:
		*done = false;
		break;
	}

	return ret;
}


static int rt9465_kick_wdt(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9465_info *info = dev_get_drvdata(&chg_dev->dev);
	enum rt9465_charging_status chg_status = RT9465_CHG_STATUS_READY;

	/* Workaround: enable/disable watchdog to kick it */
	if (info->chip_rev <= RT9465_VERSION_E2) {
		ret = rt9465_enable_wdt(info, false);
		if (ret < 0)
			chr_err("%s: disable wdt failed\n",
				__func__);

		ret = rt9465_enable_wdt(info, true);
		if (ret < 0)
			chr_err("%s: enable wdt failed\n", __func__);

		return ret;
	}

	/* Any I2C communication can kick wdt */
	return rt9465_get_charging_status(info, &chg_status);
}

static struct charger_ops rt9465_chg_ops = {
	.enable = rt9465_enable_charging,
	.is_enabled = rt9465_is_charging_enabled,
	.is_chip_enabled = rt9465_is_chip_enabled,
	.enable_safety_timer = rt9465_enable_safety_timer,
	.enable_chip = rt9465_enable_chip,
	.dump_registers = rt9465_dump_register,
	.is_charging_done = rt9465_is_charging_done,
	.get_charging_current = rt9465_get_ichg,
	.set_charging_current = rt9465_set_ichg,
	.get_min_charging_current = rt9465_get_min_ichg,
	.set_constant_voltage = rt9465_set_cv,
	.kick_wdt = rt9465_kick_wdt,
	.get_tchg_adc = rt9465_get_tchg,
	.get_mivr = rt9465_get_mivr,
	.set_mivr = rt9465_set_mivr,
	.get_mivr_state = rt9465_get_mivr_state,
};

/* ========================= */
/* I2C driver function       */
/* ========================= */

static int rt9465_probe(struct i2c_client *i2c,
	const struct i2c_device_id *dev_id)
{
	int ret = 0;
	struct rt9465_info *info = NULL;

	pr_info("%s (%s)\n", __func__, RT9465_DRV_VERSION);

	info = devm_kzalloc(&i2c->dev, sizeof(struct rt9465_info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	info->i2c = i2c;
	info->dev = &i2c->dev;
	mutex_init(&info->i2c_access_lock);
	mutex_init(&info->adc_access_lock);
	mutex_init(&info->gpio_access_lock);
	mutex_init(&info->irq_access_lock);
	mutex_init(&info->hidden_mode_lock);
	atomic_set(&info->is_chip_en, 0);

	/* Must parse en gpio */
	ret = rt9465_parse_dt(info, &i2c->dev);
	if (ret < 0) {
		chr_err("%s: parse dt failed\n", __func__);
		goto err_parse_dt;
	}
	i2c_set_clientdata(i2c, info);

#ifdef CONFIG_RT_REGMAP
	ret = rt9465_register_rt_regmap(info);
	if (ret < 0)
		goto err_register_regmap;
#endif

	/* Register charger device */
	info->chg_dev = charger_device_register(info->desc->chg_dev_name,
		&i2c->dev, info, &rt9465_chg_ops, &info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev)) {
		ret = PTR_ERR(info->chg_dev);
		goto err_register_chg_dev;
	}

	ret = rt9465_register_irq(info);
	if (ret < 0) {
		chr_err("%s: reg irq fail(%d)\n", __func__, ret);
		goto err_register_irq;
	}

	chr_info("%s: successfully\n", __func__);

	return ret;

err_register_irq:
err_register_chg_dev:
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(info->regmap_dev);
err_register_regmap:
#endif
err_parse_dt:
	mutex_destroy(&info->adc_access_lock);
	mutex_destroy(&info->i2c_access_lock);
	mutex_destroy(&info->gpio_access_lock);
	mutex_destroy(&info->irq_access_lock);
	mutex_destroy(&info->hidden_mode_lock);
	return ret;
}

static int rt9465_remove(struct i2c_client *i2c)
{
	int ret = 0;
	struct rt9465_info *info = i2c_get_clientdata(i2c);

	pr_info("%s\n", __func__);

	if (info) {
		if (info->chg_dev)
			charger_device_unregister(info->chg_dev);
#ifdef CONFIG_RT_REGMAP
		rt_regmap_device_unregister(info->regmap_dev);
#endif
		mutex_destroy(&info->adc_access_lock);
		mutex_destroy(&info->i2c_access_lock);
		mutex_destroy(&info->gpio_access_lock);
		mutex_destroy(&info->irq_access_lock);
		mutex_destroy(&info->hidden_mode_lock);
	}
	return ret;
}

static void rt9465_shutdown(struct i2c_client *i2c)
{
	int ret = 0;
	struct rt9465_info *info = i2c_get_clientdata(i2c);

	pr_info("%s\n", __func__);

	if (info) {
		ret = rt9465_reset_chip(info);
		if (ret < 0)
			chr_err("%s: sw reset failed\n", __func__);
	}
}

static int rt9465_suspend(struct device *dev)
{
	struct rt9465_info *info = dev_get_drvdata(dev);

	chr_info("%s\n", __func__);
	if (device_may_wakeup(dev))
		enable_irq_wake(info->irq);

	return 0;
}

static int rt9465_resume(struct device *dev)
{
	struct rt9465_info *info = dev_get_drvdata(dev);

	chr_info("%s\n", __func__);
	if (device_may_wakeup(dev))
		disable_irq_wake(info->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rt9465_pm_ops, rt9465_suspend, rt9465_resume);

static const struct i2c_device_id rt9465_i2c_id[] = {
	{"rt9465", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt9465_i2c_id);

static const struct of_device_id rt9465_of_match[] = {
	{ .compatible = "richtek,rt9465", },
	{},
};
MODULE_DEVICE_TABLE(of, rt9465_of_match);

#ifndef CONFIG_OF
#define RT9465_BUSNUM 1

static struct i2c_board_info rt9465_i2c_board_info __initdata = {
	I2C_BOARD_INFO("rt9465", RT9465_SALVE_ADDR)
};
#endif /* CONFIG_OF */


static struct i2c_driver rt9465_i2c_driver = {
	.driver = {
		.name = "rt9465",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt9465_of_match),
		.pm = &rt9465_pm_ops,
	},
	.probe = rt9465_probe,
	.remove = rt9465_remove,
	.shutdown = rt9465_shutdown,
	.id_table = rt9465_i2c_id,
};

static int __init rt9465_init(void)
{
	int ret = 0;

#ifdef CONFIG_OF
	pr_info("%s: with dts\n", __func__);
#else
	pr_info("%s: without dts\n", __func__);
	i2c_register_board_info(RT9465_BUSNUM, &rt9465_i2c_board_info, 1);
#endif

	ret = i2c_add_driver(&rt9465_i2c_driver);
	if (ret < 0)
		chr_err("%s: register i2c driver fail\n", __func__);

	return ret;
}
module_init(rt9465_init);


static void __exit rt9465_exit(void)
{
	i2c_del_driver(&rt9465_i2c_driver);
}
module_exit(rt9465_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("ShuFanLee <shufan_lee@richtek.com>");
MODULE_DESCRIPTION("RT9465 Charger Driver");
MODULE_VERSION(RT9465_DRV_VERSION);


/*
 * Version Note
 * 1.0.11
 * (1) Remove mt6336 and mt6306 related code
 *
 * 1.0.10
 * (1) Remove retries for chip id check
 * (2) Move chip check from probe to enale_chip
 *
 * 1.0.9
 * (1) Add more retries for chip id check
 *
 * 1.0.8
 * (1) Use standard GPIO API instead of pinctrl
 * (2) Remove EN pin pull high, lock i2c adapter workaround
 * (3) Add mt6306 gpio expander control
 *
 * 1.0.7
 * (1) Modify init sequence in probe function
 *
 * 1.0.6
 * (1) Modify the way to kick WDT and the name of enable_watchdog_timer to
 *     enable_wdt
 * (2) Change pr_xxx to dev_xxx
 *
 * 1.0.5
 * (1) Modify charger name to secondary_chg
 *
 * 1.0.4
 * (1) Modify some pr_debug to pr_debug_ratelimited
 * (2) Modify the way to parse dt
 *
 * 1.0.3
 * (1) Modify rt9465_is_hw_exist to support all version
 * (2) Correct chip version
 * (3) Release rt9465_is_charging_enabled/rt9465_get_min_ichg
 *
 * 1.0.2
 * (1) Add ICHG accuracy workaround for E2
 * (2) Support E3 chip
 * (3) Add config to separate EN pin from MT6336 or AP
 *
 * 1.0.1
 * (1) Remove registering power supply class
 *
 * 1.0.0
 * Initial Release
 */
