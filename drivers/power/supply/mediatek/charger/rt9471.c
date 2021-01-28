// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/pm_runtime.h>
#include <linux/i2c.h>
#include <linux/of_device.h>
#include <linux/mutex.h>
#include <linux/power_supply.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include <mt-plat/upmu_common.h>
#include <mt-plat/charger_class.h>
#include <mt-plat/charger_type.h>
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif

#include "mtk_charger_intf.h"
#include "rt9471.h"
#define RT9471_DRV_VERSION	"1.0.5_MTK"

enum rt9471_stat_idx {
	RT9471_STATIDX_STAT0 = 0,
	RT9471_STATIDX_STAT1,
	RT9471_STATIDX_STAT2,
	RT9471_STATIDX_STAT3,
	RT9471_STATIDX_MAX,
};

enum rt9471_irq_idx {
	RT9471_IRQIDX_IRQ0 = 0,
	RT9471_IRQIDX_IRQ1,
	RT9471_IRQIDX_IRQ2,
	RT9471_IRQIDX_IRQ3,
	RT9471_IRQIDX_MAX,
};

enum rt9471_ic_stat {
	RT9471_ICSTAT_SLEEP = 0,
	RT9471_ICSTAT_VBUSRDY,
	RT9471_ICSTAT_TRICKLECHG,
	RT9471_ICSTAT_PRECHG,
	RT9471_ICSTAT_FASTCHG,
	RT9471_ICSTAT_IEOC,
	RT9471_ICSTAT_BGCHG,
	RT9471_ICSTAT_CHGDONE,
	RT9471_ICSTAT_CHGFAULT,
	RT9471_ICSTAT_OTG = 15,
	RT9471_ICSTAT_MAX,
};

static const char *rt9471_ic_stat_name[RT9471_ICSTAT_MAX] = {
	"hz/sleep", "ready", "trickle-charge", "pre-charge",
	"fast-charge", "ieoc-charge", "background-charge",
	"done", "fault", "RESERVED", "RESERVED", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED", "OTG",
};

enum rt9471_mivr_track {
	RT9471_MIVRTRACK_REG = 0,
	RT9471_MIVRTRACK_VBAT_200MV,
	RT9471_MIVRTRACK_VBAT_250MV,
	RT9471_MIVRTRACK_VBAT_300MV,
	RT9471_MIVRTRACK_MAX,
};

enum rt9471_port_stat {
	RT9471_PORTSTAT_NOINFO = 0,
	RT9471_PORTSTAT_APPLE_10W = 8,
	RT9471_PORTSTAT_SAMSUNG_10W,
	RT9471_PORTSTAT_APPLE_5W,
	RT9471_PORTSTAT_APPLE_12W,
	RT9471_PORTSTAT_NSDP,
	RT9471_PORTSTAT_SDP,
	RT9471_PORTSTAT_CDP,
	RT9471_PORTSTAT_DCP,
	RT9471_PORTSTAT_MAX,
};

enum rt9471_usbsw_state {
	RT9471_USBSW_CHG = 0,
	RT9471_USBSW_USB,
};

struct rt9471_desc {
	const char *rm_name;
	u8 rm_slave_addr;
	u32 ichg;
	u32 aicr;
	u32 mivr;
	u32 cv;
	u32 ieoc;
	u32 safe_tmr;
	u32 wdt;
	u32 mivr_track;
	bool en_safe_tmr;
	bool en_te;
	bool en_jeita;
	bool ceb_invert;
	bool dis_i2c_tout;
	bool en_qon_rst;
	bool auto_aicr;
	const char *chg_name;
};

/* These default values will be applied if there's no property in dts */
static struct rt9471_desc rt9471_default_desc = {
	.rm_name = "rt9471",
	.rm_slave_addr = RT9471_SLAVE_ADDR,
	.ichg = 2000000,
	.aicr = 500000,
	.mivr = 4500000,
	.cv = 4200000,
	.ieoc = 200000,
	.safe_tmr = 10,
	.wdt = 40,
	.mivr_track = RT9471_MIVRTRACK_REG,
	.en_safe_tmr = true,
	.en_te = true,
	.en_jeita = true,
	.ceb_invert = false,
	.dis_i2c_tout = false,
	.en_qon_rst = true,
	.auto_aicr = true,
	.chg_name = "primary_chg",
};

static const u8 rt9471_irq_maskall[RT9471_IRQIDX_MAX] = {
	0xFF, 0xFE, 0xF3, 0xE7,
};

static const u32 rt9471_wdt[] = {
	0, 40, 80, 160,
};

static u32 rt9471_otgcc[] = {
	500000, 1200000,
};

static const u8 rt9471_val_en_hidden_mode[] = {
	0x69, 0x96,
};

static const char *rt9471_port_name[RT9471_PORTSTAT_MAX] = {
	"NOINFO",
	"RESERVED", "RESERVED", "RESERVED", "RESERVED",
	"RESERVED", "RESERVED", "RESERVED",
	"APPLE_10W",
	"SAMSUNG_10W",
	"APPLE_5W",
	"APPLE_12W",
	"NSDP",
	"SDP",
	"CDP",
	"DCP",
};

struct rt9471_chip {
	struct i2c_client *client;
	struct device *dev;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct semaphore suspend_lock;
	struct mutex io_lock;
	struct mutex bc12_lock;
	struct mutex bc12_en_lock;
	struct mutex hidden_mode_lock;
	int hidden_mode_cnt;
	u8 dev_id;
	u8 dev_rev;
	u8 chip_rev;
	struct rt9471_desc *desc;
	u32 intr_gpio;
	u32 ceb_gpio;
	int irq;
	u8 irq_mask[RT9471_IRQIDX_MAX];
#ifndef CONFIG_TCPC_CLASS
	struct work_struct init_work;
#endif
	atomic_t vbus_gd;
	bool attach;
	enum rt9471_port_stat port;
	enum charger_type chg_type;
	struct power_supply *psy;
	struct task_struct *bc12_en_kthread;
	int bc12_en_buf[2];
	int bc12_en_buf_idx;
	struct completion bc12_en_req;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *rm_dev;
	struct rt_regmap_properties *rm_prop;
#endif /* CONFIG_RT_REGMAP */
	u32 mivr;
};

static const u8 rt9471_reg_addr[] = {
	RT9471_REG_OTGCFG,
	RT9471_REG_TOP,
	RT9471_REG_FUNCTION,
	RT9471_REG_IBUS,
	RT9471_REG_VBUS,
	RT9471_REG_PRECHG,
	RT9471_REG_REGU,
	RT9471_REG_VCHG,
	RT9471_REG_ICHG,
	RT9471_REG_CHGTIMER,
	RT9471_REG_EOC,
	RT9471_REG_INFO,
	RT9471_REG_JEITA,
	RT9471_REG_DPDMDET,
	RT9471_REG_STATUS,
	RT9471_REG_STAT0,
	RT9471_REG_STAT1,
	RT9471_REG_STAT2,
	RT9471_REG_STAT3,
	/* Skip IRQs to prevent reading clear while dumping registers */
	RT9471_REG_MASK0,
	RT9471_REG_MASK1,
	RT9471_REG_MASK2,
	RT9471_REG_MASK3,
};

static int rt9471_read_device(void *client, u32 addr, int len, void *dst)
{
	int ret = 0;
	struct rt9471_chip *chip = i2c_get_clientdata(client);

	pm_stay_awake(chip->dev);
	down(&chip->suspend_lock);
	ret = i2c_smbus_read_i2c_block_data(client, addr, len, dst);
	up(&chip->suspend_lock);
	pm_relax(chip->dev);

	return ret;
}

static int rt9471_write_device(void *client, u32 addr, int len,
			       const void *src)
{
	int ret = 0;
	struct rt9471_chip *chip = i2c_get_clientdata(client);

	pm_stay_awake(chip->dev);
	down(&chip->suspend_lock);
	ret = i2c_smbus_write_i2c_block_data(client, addr, len, src);
	up(&chip->suspend_lock);
	pm_relax(chip->dev);

	return ret;
}

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT9471_REG_OTGCFG, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_TOP, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_FUNCTION, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_IBUS, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_VBUS, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_PRECHG, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_REGU, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_VCHG, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_ICHG, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_CHGTIMER, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_EOC, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_INFO, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_JEITA, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_DPDMDET, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_STATUS, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_STAT0, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_STAT1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_STAT2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_STAT3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_IRQ0, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_IRQ1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_IRQ2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_IRQ3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_MASK0, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_MASK1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_MASK2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9471_REG_MASK3, 1, RT_VOLATILE, {});

static const rt_register_map_t rt9471_rm_map[] = {
	RT_REG(RT9471_REG_OTGCFG),
	RT_REG(RT9471_REG_TOP),
	RT_REG(RT9471_REG_FUNCTION),
	RT_REG(RT9471_REG_IBUS),
	RT_REG(RT9471_REG_VBUS),
	RT_REG(RT9471_REG_PRECHG),
	RT_REG(RT9471_REG_REGU),
	RT_REG(RT9471_REG_VCHG),
	RT_REG(RT9471_REG_ICHG),
	RT_REG(RT9471_REG_CHGTIMER),
	RT_REG(RT9471_REG_EOC),
	RT_REG(RT9471_REG_INFO),
	RT_REG(RT9471_REG_JEITA),
	RT_REG(RT9471_REG_DPDMDET),
	RT_REG(RT9471_REG_STATUS),
	RT_REG(RT9471_REG_STAT0),
	RT_REG(RT9471_REG_STAT1),
	RT_REG(RT9471_REG_STAT2),
	RT_REG(RT9471_REG_STAT3),
	RT_REG(RT9471_REG_IRQ0),
	RT_REG(RT9471_REG_IRQ1),
	RT_REG(RT9471_REG_IRQ2),
	RT_REG(RT9471_REG_IRQ3),
	RT_REG(RT9471_REG_MASK0),
	RT_REG(RT9471_REG_MASK1),
	RT_REG(RT9471_REG_MASK2),
	RT_REG(RT9471_REG_MASK3),
};

static struct rt_regmap_fops rt9471_rm_fops = {
	.read_device = rt9471_read_device,
	.write_device = rt9471_write_device,
};

static int rt9471_register_rt_regmap(struct rt9471_chip *chip)
{
	struct rt_regmap_properties *prop = NULL;

	dev_info(chip->dev, "%s\n", __func__);

	prop = devm_kzalloc(chip->dev, sizeof(*prop), GFP_KERNEL);
	if (!prop)
		return -ENOMEM;

	prop->name = chip->desc->rm_name;
	prop->aliases = chip->desc->rm_name;
	prop->register_num = ARRAY_SIZE(rt9471_rm_map);
	prop->rm = rt9471_rm_map;
	prop->rt_regmap_mode = RT_SINGLE_BYTE | RT_CACHE_DISABLE |
			       RT_IO_PASS_THROUGH | RT_DBG_SPECIAL;
	prop->io_log_en = 0;

	chip->rm_prop = prop;
	chip->rm_dev = rt_regmap_device_register_ex(chip->rm_prop,
						    &rt9471_rm_fops, chip->dev,
						    chip->client,
						    chip->desc->rm_slave_addr,
						    chip);
	if (!chip->rm_dev) {
		dev_notice(chip->dev, "%s fail\n", __func__);
		return -EIO;
	}

	return 0;
}
#endif /* CONFIG_RT_REGMAP */

static inline int __rt9471_i2c_write_byte(struct rt9471_chip *chip, u8 cmd,
					  u8 data)
{
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->rm_dev, cmd, 1, &data);
#else
	ret = rt9471_write_device(chip->client, cmd, 1, &data);
#endif

	if (ret < 0)
		dev_notice(chip->dev, "%s reg0x%02X = 0x%02X fail(%d)\n",
				      __func__, cmd, data, ret);
	else
		dev_dbg(chip->dev, "%s reg0x%02X = 0x%02X\n", __func__, cmd,
			data);

	return ret;
}

static int rt9471_i2c_write_byte(struct rt9471_chip *chip, u8 cmd, u8 data)
{
	int ret = 0;

	mutex_lock(&chip->io_lock);
	ret = __rt9471_i2c_write_byte(chip, cmd, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static inline int __rt9471_i2c_read_byte(struct rt9471_chip *chip, u8 cmd,
					 u8 *data)
{
	int ret = 0;
	u8 regval = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->rm_dev, cmd, 1, &regval);
#else
	ret = rt9471_read_device(chip->client, cmd, 1, &regval);
#endif

	if (ret < 0) {
		dev_notice(chip->dev, "%s reg0x%02X fail(%d)\n",
				      __func__, cmd, ret);
		return ret;
	}

	dev_dbg(chip->dev, "%s reg0x%02X = 0x%02X\n", __func__, cmd, regval);
	*data = regval & 0xFF;
	return 0;
}

static int rt9471_i2c_read_byte(struct rt9471_chip *chip, u8 cmd, u8 *data)
{
	int ret = 0;

	mutex_lock(&chip->io_lock);
	ret = __rt9471_i2c_read_byte(chip, cmd, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static inline int __rt9471_i2c_block_write(struct rt9471_chip *chip, u8 cmd,
					   u32 len, const u8 *data)
{
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(chip->rm_dev, cmd, len, data);
#else
	ret = rt9471_write_device(chip->client, cmd, len, data);
#endif

	return ret;
}

static int rt9471_i2c_block_write(struct rt9471_chip *chip, u8 cmd, u32 len,
				  const u8 *data)
{
	int ret = 0;

	mutex_lock(&chip->io_lock);
	ret = __rt9471_i2c_block_write(chip, cmd, len, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static inline int __rt9471_i2c_block_read(struct rt9471_chip *chip, u8 cmd,
					  u32 len, u8 *data)
{
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(chip->rm_dev, cmd, len, data);
#else
	ret = rt9471_read_device(chip->client, cmd, len, data);
#endif

	return ret;
}

static int rt9471_i2c_block_read(struct rt9471_chip *chip, u8 cmd, u32 len,
				 u8 *data)
{
	int ret = 0;

	mutex_lock(&chip->io_lock);
	ret = __rt9471_i2c_block_read(chip, cmd, len, data);
	mutex_unlock(&chip->io_lock);

	return ret;
}

static int rt9471_i2c_test_bit(struct rt9471_chip *chip, u8 cmd, u8 shift,
			       bool *is_one)
{
	int ret = 0;
	u8 regval = 0;

	ret = rt9471_i2c_read_byte(chip, cmd, &regval);
	if (ret < 0) {
		*is_one = false;
		return ret;
	}

	regval &= 1 << shift;
	*is_one = (regval ? true : false);

	return ret;
}

static int rt9471_i2c_update_bits(struct rt9471_chip *chip, u8 cmd, u8 data,
				  u8 mask)
{
	int ret = 0;
	u8 regval = 0;

	mutex_lock(&chip->io_lock);
	ret = __rt9471_i2c_read_byte(chip, cmd, &regval);
	if (ret < 0)
		goto out;

	regval &= ~mask;
	regval |= (data & mask);

	ret = __rt9471_i2c_write_byte(chip, cmd, regval);
out:
	mutex_unlock(&chip->io_lock);
	return ret;
}

static inline int rt9471_set_bit(struct rt9471_chip *chip, u8 cmd, u8 mask)
{
	return rt9471_i2c_update_bits(chip, cmd, mask, mask);
}

static inline int rt9471_clr_bit(struct rt9471_chip *chip, u8 cmd, u8 mask)
{
	return rt9471_i2c_update_bits(chip, cmd, 0x00, mask);
}

static inline u8 rt9471_closest_reg(u32 min, u32 max, u32 step, u32 target)
{
	if (target < min)
		return 0;

	if (target >= max)
		return (max - min) / step;

	return (target - min) / step;
}

static inline u8 rt9471_closest_reg_via_tbl(const u32 *tbl, u32 tbl_size,
					    u32 target)
{
	u32 i = 0;

	if (target < tbl[0])
		return 0;

	for (i = 0; i < tbl_size - 1; i++) {
		if (target >= tbl[i] && target < tbl[i + 1])
			return i;
	}

	return tbl_size - 1;
}

static inline u32 rt9471_closest_value(u32 min, u32 max, u32 step, u8 regval)
{
	u32 val = 0;

	val = min + regval * step;
	if (val > max)
		val = max;

	return val;
}

#ifndef CONFIG_TCPC_CLASS
static bool rt9471_is_vbusgd(struct rt9471_chip *chip)
{
	int ret = 0;
	bool vbus_gd = false;

	ret = rt9471_i2c_test_bit(chip, RT9471_REG_STAT0,
				  RT9471_ST_VBUSGD_SHIFT, &vbus_gd);
	if (ret < 0)
		dev_notice(chip->dev, "%s check stat fail(%d)\n",
				      __func__, ret);
	dev_dbg(chip->dev, "%s vbus_gd = %d\n", __func__, vbus_gd);

	return vbus_gd;
}
#endif

static void rt9471_set_usbsw_state(struct rt9471_chip *chip, int state)
{
	dev_info(chip->dev, "%s state = %d\n", __func__, state);

	if (state == RT9471_USBSW_CHG)
		Charger_Detect_Init();
	else
		Charger_Detect_Release();
}

static int rt9471_bc12_en_kthread(void *data)
{
	int ret = 0, i = 0, en = 0;
	struct rt9471_chip *chip = data;
	const int max_wait_cnt = 200;

	dev_info(chip->dev, "%s\n", __func__);
wait:
	wait_for_completion(&chip->bc12_en_req);

	mutex_lock(&chip->bc12_en_lock);
	en = chip->bc12_en_buf[chip->bc12_en_buf_idx];
	chip->bc12_en_buf[chip->bc12_en_buf_idx] = -1;
	if (en == -1) {
		chip->bc12_en_buf_idx = 1 - chip->bc12_en_buf_idx;
		en = chip->bc12_en_buf[chip->bc12_en_buf_idx];
		chip->bc12_en_buf[chip->bc12_en_buf_idx] = -1;
	}
	mutex_unlock(&chip->bc12_en_lock);

	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	if (en == -1)
		goto wait;

	if (en) {
		/* Workaround for CDP port */
		for (i = 0; i < max_wait_cnt; i++) {
			if (is_usb_rdy())
				break;
			dev_dbg(chip->dev, "%s CDP block\n", __func__);
			if (!atomic_read(&chip->vbus_gd)) {
				dev_info(chip->dev, "%s plug out\n", __func__);
				goto wait;
			}
			msleep(100);
		}
		if (i == max_wait_cnt)
			dev_notice(chip->dev, "%s CDP timeout\n", __func__);
		else
			dev_info(chip->dev, "%s CDP free\n", __func__);
	}
	rt9471_set_usbsw_state(chip, en ? RT9471_USBSW_CHG : RT9471_USBSW_USB);
	ret = (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_DPDMDET, RT9471_BC12_EN_MASK);
	if (ret < 0)
		dev_notice(chip->dev, "%s en = %d fail(%d)\n",
				      __func__, en, ret);
	goto wait;

	return 0;
}

static void rt9471_enable_bc12(struct rt9471_chip *chip, bool en)
{
	if (chip->dev_id != RT9470D_DEVID && chip->dev_id != RT9471D_DEVID)
		return;

	dev_info(chip->dev, "%s en = %d\n", __func__, en);

	mutex_lock(&chip->bc12_en_lock);
	chip->bc12_en_buf[chip->bc12_en_buf_idx] = en;
	chip->bc12_en_buf_idx = 1 - chip->bc12_en_buf_idx;
	mutex_unlock(&chip->bc12_en_lock);
	complete(&chip->bc12_en_req);
}


static int rt9471_enable_hidden_mode(struct rt9471_chip *chip, bool en)
{
	int ret = 0;

	mutex_lock(&chip->hidden_mode_lock);

	if (en) {
		if (chip->hidden_mode_cnt == 0) {
			ret = rt9471_i2c_block_write(chip, 0xA0,
				ARRAY_SIZE(rt9471_val_en_hidden_mode),
				rt9471_val_en_hidden_mode);
			if (ret < 0)
				goto err;
		}
		chip->hidden_mode_cnt++;
	} else {
		if (chip->hidden_mode_cnt == 1) /* last one */
			ret = rt9471_i2c_write_byte(chip, 0xA0, 0x00);
		chip->hidden_mode_cnt--;
		if (ret < 0)
			goto err;
	}
	dev_dbg(chip->dev, "%s en = %d, cnt = %d\n", __func__,
			   en, chip->hidden_mode_cnt);
	goto out;

err:
	dev_notice(chip->dev, "%s en = %d fail(%d)\n", __func__, en, ret);
out:
	mutex_unlock(&chip->hidden_mode_lock);
	return ret;
}

static int __rt9471_get_ic_stat(struct rt9471_chip *chip,
				enum rt9471_ic_stat *stat)
{
	int ret = 0;
	u8 regval = 0;

	ret = rt9471_i2c_read_byte(chip, RT9471_REG_STATUS, &regval);
	if (ret < 0)
		return ret;
	*stat = (regval & RT9471_ICSTAT_MASK) >> RT9471_ICSTAT_SHIFT;

	return ret;
}

static int __rt9471_get_mivr(struct rt9471_chip *chip, u32 *mivr)
{
	int ret = 0;
	u8 regval = 0;

	ret = rt9471_i2c_read_byte(chip, RT9471_REG_VBUS, &regval);
	if (ret < 0)
		return ret;

	regval = (regval & RT9471_MIVR_MASK) >> RT9471_MIVR_SHIFT;
	*mivr = rt9471_closest_value(RT9471_MIVR_MIN, RT9471_MIVR_MAX,
				     RT9471_MIVR_STEP, regval);

	return ret;
}

static int __rt9471_get_ichg(struct rt9471_chip *chip, u32 *ichg)
{
	int ret = 0;
	u8 regval = 0;

	ret = rt9471_i2c_read_byte(chip, RT9471_REG_ICHG, &regval);
	if (ret < 0)
		return ret;

	regval = (regval & RT9471_ICHG_MASK) >> RT9471_ICHG_SHIFT;
	*ichg = rt9471_closest_value(RT9471_ICHG_MIN, RT9471_ICHG_MAX,
				     RT9471_ICHG_STEP, regval);

	return ret;
}

static int __rt9471_get_aicr(struct rt9471_chip *chip, u32 *aicr)
{
	int ret = 0;
	u8 regval = 0;

	ret = rt9471_i2c_read_byte(chip, RT9471_REG_IBUS, &regval);
	if (ret < 0)
		return ret;

	regval = (regval & RT9471_AICR_MASK) >> RT9471_AICR_SHIFT;
	*aicr = rt9471_closest_value(RT9471_AICR_MIN, RT9471_AICR_MAX,
				     RT9471_AICR_STEP, regval);
	if (*aicr > RT9471_AICR_MIN && *aicr < RT9471_AICR_MAX)
		*aicr -= RT9471_AICR_STEP;

	return ret;
}

static int __rt9471_get_cv(struct rt9471_chip *chip, u32 *cv)
{
	int ret = 0;
	u8 regval = 0;

	ret = rt9471_i2c_read_byte(chip, RT9471_REG_VCHG, &regval);
	if (ret < 0)
		return ret;

	regval = (regval & RT9471_CV_MASK) >> RT9471_CV_SHIFT;
	*cv = rt9471_closest_value(RT9471_CV_MIN, RT9471_CV_MAX, RT9471_CV_STEP,
				   regval);

	return ret;
}

static int __rt9471_get_ieoc(struct rt9471_chip *chip, u32 *ieoc)
{
	int ret = 0;
	u8 regval = 0;

	ret = rt9471_i2c_read_byte(chip, RT9471_REG_EOC, &regval);
	if (ret < 0)
		return ret;

	regval = (regval & RT9471_IEOC_MASK) >> RT9471_IEOC_SHIFT;
	*ieoc = rt9471_closest_value(RT9471_IEOC_MIN, RT9471_IEOC_MAX,
				     RT9471_IEOC_STEP, regval);

	return ret;
}

static int __rt9471_is_chg_enabled(struct rt9471_chip *chip, bool *en)
{
	return rt9471_i2c_test_bit(chip, RT9471_REG_FUNCTION,
				   RT9471_CHG_EN_SHIFT, en);
}

static int __rt9471_enable_shipmode(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_FUNCTION, RT9471_BATFETDIS_MASK);
}

static int __rt9471_enable_safe_tmr(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_CHGTIMER, RT9471_SAFETMR_EN_MASK);
}

static int __rt9471_enable_te(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_EOC, RT9471_TE_MASK);
}

static int __rt9471_enable_jeita(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_JEITA, RT9471_JEITA_EN_MASK);
}

static int __rt9471_disable_i2c_tout(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_TOP, RT9471_DISI2CTO_MASK);
}

static int __rt9471_enable_qon_rst(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_TOP, RT9471_QONRST_MASK);
}

static int __rt9471_enable_autoaicr(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_IBUS, RT9471_AUTOAICR_MASK);
}

static int __rt9471_enable_hz(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_FUNCTION, RT9471_HZ_MASK);
}

static int __rt9471_enable_otg(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_FUNCTION, RT9471_OTG_EN_MASK);
}

static int __rt9471_enable_chg(struct rt9471_chip *chip, bool en)
{
	dev_info(chip->dev, "%s en = %d\n", __func__, en);
	return (en ? rt9471_set_bit : rt9471_clr_bit)
		(chip, RT9471_REG_FUNCTION, RT9471_CHG_EN_MASK);
}

static int __rt9471_set_wdt(struct rt9471_chip *chip, u32 sec)
{
	u8 regval = 0;

	/* 40s is the minimum, set to 40 except sec == 0 */
	if (sec <= 40 && sec > 0)
		sec = 40;
	regval = rt9471_closest_reg_via_tbl(rt9471_wdt, ARRAY_SIZE(rt9471_wdt),
					    sec);

	dev_info(chip->dev, "%s time = %d(0x%02X)\n", __func__, sec, regval);

	return rt9471_i2c_update_bits(chip, RT9471_REG_TOP,
				      regval << RT9471_WDT_SHIFT,
				      RT9471_WDT_MASK);
}

static int __rt9471_set_otgcc(struct rt9471_chip *chip, u32 cc)
{
	dev_info(chip->dev, "%s cc = %d\n", __func__, cc);
	return (cc <= rt9471_otgcc[0] ? rt9471_clr_bit : rt9471_set_bit)
		(chip, RT9471_REG_OTGCFG, RT9471_OTGCC_MASK);
}

static int __rt9471_set_ichg(struct rt9471_chip *chip, u32 ichg)
{
	u8 regval = 0;

	regval = rt9471_closest_reg(RT9471_ICHG_MIN, RT9471_ICHG_MAX,
				    RT9471_ICHG_STEP, ichg);

	dev_info(chip->dev, "%s ichg = %d(0x%02X)\n", __func__, ichg, regval);

	return rt9471_i2c_update_bits(chip, RT9471_REG_ICHG,
				      regval << RT9471_ICHG_SHIFT,
				      RT9471_ICHG_MASK);
}

static int __rt9471_set_aicr(struct rt9471_chip *chip, u32 aicr)
{
	u8 regval = 0;

	regval = rt9471_closest_reg(RT9471_AICR_MIN, RT9471_AICR_MAX,
				    RT9471_AICR_STEP, aicr);
	/* 0 & 1 are both 50mA */
	if (aicr < RT9471_AICR_MAX)
		regval += 1;

	dev_info(chip->dev, "%s aicr = %d(0x%02X)\n", __func__, aicr, regval);

	return rt9471_i2c_update_bits(chip, RT9471_REG_IBUS,
				      regval << RT9471_AICR_SHIFT,
				      RT9471_AICR_MASK);
}

static int __rt9471_set_mivr(struct rt9471_chip *chip, u32 mivr)
{
	u8 regval = 0;

	regval = rt9471_closest_reg(RT9471_MIVR_MIN, RT9471_MIVR_MAX,
				    RT9471_MIVR_STEP, mivr);

	dev_info(chip->dev, "%s mivr = %d(0x%02X)\n", __func__, mivr, regval);

	return rt9471_i2c_update_bits(chip, RT9471_REG_VBUS,
				      regval << RT9471_MIVR_SHIFT,
				      RT9471_MIVR_MASK);
}

static int __rt9471_set_cv(struct rt9471_chip *chip, u32 cv)
{
	u8 regval = 0;

	regval = rt9471_closest_reg(RT9471_CV_MIN, RT9471_CV_MAX,
				    RT9471_CV_STEP, cv);

	dev_info(chip->dev, "%s cv = %d(0x%02X)\n", __func__, cv, regval);

	return rt9471_i2c_update_bits(chip, RT9471_REG_VCHG,
				      regval << RT9471_CV_SHIFT,
				      RT9471_CV_MASK);
}

static int __rt9471_set_ieoc(struct rt9471_chip *chip, u32 ieoc)
{
	u8 regval = 0;

	regval = rt9471_closest_reg(RT9471_IEOC_MIN, RT9471_IEOC_MAX,
				    RT9471_IEOC_STEP, ieoc);

	dev_info(chip->dev, "%s ieoc = %d(0x%02X)\n", __func__, ieoc, regval);

	return rt9471_i2c_update_bits(chip, RT9471_REG_EOC,
				      regval << RT9471_IEOC_SHIFT,
				      RT9471_IEOC_MASK);
}

static int __rt9471_set_safe_tmr(struct rt9471_chip *chip, u32 hr)
{
	u8 regval = 0;

	regval = rt9471_closest_reg(RT9471_SAFETMR_MIN, RT9471_SAFETMR_MAX,
				    RT9471_SAFETMR_STEP, hr);

	dev_info(chip->dev, "%s time = %d(0x%02X)\n", __func__, hr, regval);

	return rt9471_i2c_update_bits(chip, RT9471_REG_CHGTIMER,
				      regval << RT9471_SAFETMR_SHIFT,
				      RT9471_SAFETMR_MASK);
}

static int __rt9471_set_mivrtrack(struct rt9471_chip *chip, u32 mivr_track)
{
	if (mivr_track >= RT9471_MIVRTRACK_MAX)
		mivr_track = RT9471_MIVRTRACK_VBAT_300MV;

	dev_info(chip->dev, "%s mivrtrack = %d\n", __func__, mivr_track);

	return rt9471_i2c_update_bits(chip, RT9471_REG_VBUS,
				      mivr_track << RT9471_MIVRTRACK_SHIFT,
				      RT9471_MIVRTRACK_MASK);
}

static int __rt9471_kick_wdt(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return rt9471_set_bit(chip, RT9471_REG_TOP, RT9471_WDTCNTRST_MASK);
}

static int rt9471_bc12_preprocess(struct rt9471_chip *chip)
{
	if (chip->dev_id != RT9470D_DEVID && chip->dev_id != RT9471D_DEVID)
		return 0;

	if (atomic_read(&chip->vbus_gd)) {
		rt9471_enable_bc12(chip, false);
		rt9471_enable_bc12(chip, true);
	}

	return 0;
}

static int rt9471_inform_psy_changed(struct rt9471_chip *chip)
{
	int ret = 0;
	union power_supply_propval propval;
	bool vbus_gd = atomic_read(&chip->vbus_gd);

	dev_info(chip->dev, "%s vbus_gd = %d, type = %d\n", __func__,
			    vbus_gd, chip->chg_type);

	/* Get chg type det power supply */
	chip->psy = power_supply_get_by_name("charger");
	if (!chip->psy) {
		dev_notice(chip->dev, "%s get power supply fail\n", __func__);
		return -EINVAL;
	}

	/* inform chg det power supply */
	propval.intval = vbus_gd;
	ret = power_supply_set_property(chip->psy, POWER_SUPPLY_PROP_ONLINE,
					&propval);
	if (ret < 0)
		dev_notice(chip->dev, "%s psy online fail(%d)\n",
				      __func__, ret);

	propval.intval = chip->chg_type;
	ret = power_supply_set_property(chip->psy,
			POWER_SUPPLY_PROP_CHARGE_TYPE, &propval);
	if (ret < 0)
		dev_notice(chip->dev, "%s psy type fail(%d)\n", __func__, ret);

	return ret;
}

static int rt9471_bc12_postprocess(struct rt9471_chip *chip)
{
	int ret = 0;
	bool attach = false, inform_psy = true;
	u8 port = RT9471_PORTSTAT_NOINFO;

	if (chip->dev_id != RT9470D_DEVID && chip->dev_id != RT9471D_DEVID)
		return 0;

	attach = atomic_read(&chip->vbus_gd);
	if (chip->attach == attach) {
		dev_info(chip->dev, "%s attach(%d) is the same\n",
				    __func__, attach);
		inform_psy = false;
		goto out;
	}
	chip->attach = attach;
	dev_info(chip->dev, "%s attach = %d\n", __func__, attach);

	if (!attach) {
		chip->port = RT9471_PORTSTAT_NOINFO;
		chip->chg_type = CHARGER_UNKNOWN;
		goto out;
	}

	ret = rt9471_i2c_read_byte(chip, RT9471_REG_STATUS, &port);
	if (ret < 0)
		chip->port = RT9471_PORTSTAT_NOINFO;
	else
		chip->port = (port & RT9471_PORTSTAT_MASK) >>
				     RT9471_PORTSTAT_SHIFT;

	switch (chip->port) {
	case RT9471_PORTSTAT_NOINFO:
		chip->chg_type = CHARGER_UNKNOWN;
		break;
	case RT9471_PORTSTAT_SDP:
		chip->chg_type = STANDARD_HOST;
		break;
	case RT9471_PORTSTAT_CDP:
		chip->chg_type = CHARGING_HOST;
		break;
	case RT9471_PORTSTAT_SAMSUNG_10W:
	case RT9471_PORTSTAT_APPLE_12W:
	case RT9471_PORTSTAT_DCP:
		chip->chg_type = STANDARD_CHARGER;
		break;
	case RT9471_PORTSTAT_APPLE_10W:
		chip->chg_type = APPLE_2_1A_CHARGER;
		break;
	case RT9471_PORTSTAT_APPLE_5W:
		chip->chg_type = APPLE_1_0A_CHARGER;
		break;
	case RT9471_PORTSTAT_NSDP:
	default:
		chip->chg_type = NONSTANDARD_CHARGER;
		break;
	}
out:
	if (chip->chg_type != STANDARD_CHARGER)
		rt9471_enable_bc12(chip, false);
	if (inform_psy)
		rt9471_inform_psy_changed(chip);

	return 0;
}

static int rt9471_detach_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
#ifndef CONFIG_TCPC_CLASS
	mutex_lock(&chip->bc12_lock);
	atomic_set(&chip->vbus_gd, rt9471_is_vbusgd(chip));
	rt9471_bc12_postprocess(chip);
	mutex_unlock(&chip->bc12_lock);
#endif
	return 0;
}

static int rt9471_rechg_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static void rt9471_bc12_done_handler(struct rt9471_chip *chip)
{
	int ret = 0;
	u8 regval = 0;
	bool bc12_done = false, chg_rdy = false;

	if (chip->dev_id != RT9470D_DEVID && chip->dev_id != RT9471D_DEVID)
		return;

	dev_info(chip->dev, "%s\n", __func__);

	ret = rt9471_i2c_read_byte(chip, RT9471_REG_STAT0, &regval);
	if (ret < 0)
		dev_notice(chip->dev, "%s check stat fail(%d)\n",
				      __func__, ret);
	bc12_done = (regval & RT9471_ST_BC12_DONE_MASK ? true : false);
	chg_rdy = (regval & RT9471_ST_CHGRDY_MASK ? true : false);
	dev_info(chip->dev, "%s bc12_done = %d, chg_rdy = %d\n",
			    __func__, bc12_done, chg_rdy);
	if (bc12_done) {
		if (chip->chip_rev <= 3 && !chg_rdy) {
			/* Workaround waiting for chg_rdy */
			dev_info(chip->dev, "%s wait chg_rdy\n", __func__);
			return;
		}
		mutex_lock(&chip->bc12_lock);
		ret = rt9471_bc12_postprocess(chip);
		dev_info(chip->dev, "%s %d %s\n", __func__, chip->port,
				    rt9471_port_name[chip->port]);
		mutex_unlock(&chip->bc12_lock);
	}
}

static int rt9471_bc12_done_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	rt9471_bc12_done_handler(chip);
	return 0;
}

static int rt9471_chg_done_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_bg_chg_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_ieoc_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_chg_rdy_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	if (chip->chip_rev <= 3)
		rt9471_bc12_done_handler(chip);
	return 0;
}

static int rt9471_vbus_gd_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
#ifndef CONFIG_TCPC_CLASS
	mutex_lock(&chip->bc12_lock);
	atomic_set(&chip->vbus_gd, rt9471_is_vbusgd(chip));
	rt9471_bc12_preprocess(chip);
	mutex_unlock(&chip->bc12_lock);
#endif
	return 0;
}

static int rt9471_chg_batov_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_chg_sysov_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_chg_tout_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_chg_busuv_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_chg_threg_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_chg_aicr_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_chg_mivr_irq_handler(struct rt9471_chip *chip)
{
	int ret = 0;
	bool mivr = false;

	ret = rt9471_i2c_test_bit(chip, RT9471_REG_STAT1, RT9471_ST_MIVR_SHIFT,
				  &mivr);
	if (ret < 0) {
		dev_notice(chip->dev, "%s check stat fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	dev_info(chip->dev, "%s mivr = %d\n", __func__, mivr);

	return 0;
}

static int rt9471_sys_short_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_sys_min_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_jeita_cold_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_jeita_cool_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_jeita_warm_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_jeita_hot_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_otg_fault_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_otg_lbp_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_otg_cc_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

static int rt9471_wdt_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return __rt9471_kick_wdt(chip);
}

static int rt9471_vac_ov_irq_handler(struct rt9471_chip *chip)
{
	int ret = 0;
	bool vacov = false;

	ret = rt9471_i2c_test_bit(chip, RT9471_REG_STAT3, RT9471_ST_VACOV_SHIFT,
				  &vacov);
	if (ret < 0) {
		dev_notice(chip->dev, "%s check stat fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	dev_info(chip->dev, "%s vacov = %d\n", __func__, vacov);

	return 0;
}

static int rt9471_otp_irq_handler(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return 0;
}

struct irq_mapping_tbl {
	const char *name;
	int (*hdlr)(struct rt9471_chip *chip);
	int num;
};

#define RT9471_IRQ_MAPPING(_name, _num) \
	{.name = #_name, .hdlr = rt9471_##_name##_irq_handler, .num = _num}

static const struct irq_mapping_tbl rt9471_irq_mapping_tbl[] = {
	RT9471_IRQ_MAPPING(wdt, 29),
	RT9471_IRQ_MAPPING(vbus_gd, 7),
	RT9471_IRQ_MAPPING(chg_rdy, 6),
	RT9471_IRQ_MAPPING(bc12_done, 0),
	RT9471_IRQ_MAPPING(detach, 1),
	RT9471_IRQ_MAPPING(rechg, 2),
	RT9471_IRQ_MAPPING(chg_done, 3),
	RT9471_IRQ_MAPPING(bg_chg, 4),
	RT9471_IRQ_MAPPING(ieoc, 5),
	RT9471_IRQ_MAPPING(chg_batov, 9),
	RT9471_IRQ_MAPPING(chg_sysov, 10),
	RT9471_IRQ_MAPPING(chg_tout, 11),
	RT9471_IRQ_MAPPING(chg_busuv, 12),
	RT9471_IRQ_MAPPING(chg_threg, 13),
	RT9471_IRQ_MAPPING(chg_aicr, 14),
	RT9471_IRQ_MAPPING(chg_mivr, 15),
	RT9471_IRQ_MAPPING(sys_short, 16),
	RT9471_IRQ_MAPPING(sys_min, 17),
	RT9471_IRQ_MAPPING(jeita_cold, 20),
	RT9471_IRQ_MAPPING(jeita_cool, 21),
	RT9471_IRQ_MAPPING(jeita_warm, 22),
	RT9471_IRQ_MAPPING(jeita_hot, 23),
	RT9471_IRQ_MAPPING(otg_fault, 24),
	RT9471_IRQ_MAPPING(otg_lbp, 25),
	RT9471_IRQ_MAPPING(otg_cc, 26),
	RT9471_IRQ_MAPPING(vac_ov, 30),
	RT9471_IRQ_MAPPING(otp, 31),
};

static irqreturn_t rt9471_irq_handler(int irq, void *data)
{
	int ret = 0, i = 0, irqnum = 0, irqbit = 0;
	u8 evt[RT9471_IRQIDX_MAX] = {0};
	u8 mask[RT9471_IRQIDX_MAX] = {0};
	struct rt9471_chip *chip = (struct rt9471_chip *)data;

	dev_info(chip->dev, "%s\n", __func__);

	ret = rt9471_i2c_block_read(chip, RT9471_REG_IRQ0, RT9471_IRQIDX_MAX,
				    evt);
	if (ret < 0) {
		dev_notice(chip->dev, "%s read evt fail(%d)\n", __func__, ret);
		goto out;
	}

	ret = rt9471_i2c_block_read(chip, RT9471_REG_MASK0, RT9471_IRQIDX_MAX,
				    mask);
	if (ret < 0) {
		dev_notice(chip->dev, "%s read mask fail(%d)\n", __func__, ret);
		goto out;
	}

	for (i = 0; i < RT9471_IRQIDX_MAX; i++)
		evt[i] &= ~mask[i];
	for (i = 0; i < ARRAY_SIZE(rt9471_irq_mapping_tbl); i++) {
		irqnum = rt9471_irq_mapping_tbl[i].num / 8;
		if (irqnum >= RT9471_IRQIDX_MAX)
			continue;
		irqbit = rt9471_irq_mapping_tbl[i].num % 8;
		if (evt[irqnum] & (1 << irqbit))
			rt9471_irq_mapping_tbl[i].hdlr(chip);
	}
out:
	return IRQ_HANDLED;
}

static int rt9471_register_irq(struct rt9471_chip *chip)
{
	int ret = 0, len = 0;
	char *name = NULL;

	dev_info(chip->dev, "%s\n", __func__);

	len = strlen(chip->desc->chg_name);
	name = devm_kzalloc(chip->dev, len + 10, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	snprintf(name,  len + 10, "%s-irq-gpio", chip->desc->chg_name);
	ret = devm_gpio_request_one(chip->dev, chip->intr_gpio, GPIOF_IN, name);
	if (ret < 0) {
		dev_notice(chip->dev, "%s gpio request fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	chip->irq = gpio_to_irq(chip->intr_gpio);
	if (chip->irq < 0) {
		dev_notice(chip->dev, "%s gpio2irq fail(%d)\n", __func__,
				      chip->irq);
		return chip->irq;
	}
	dev_info(chip->dev, "%s irq = %d\n", __func__, chip->irq);

	/* Request threaded IRQ */
	len = strlen(chip->desc->chg_name);
	name = devm_kzalloc(chip->dev, len + 5, GFP_KERNEL);
	if (!name)
		return -ENOMEM;
	snprintf(name, len + 5, "%s-irq", chip->desc->chg_name);
	ret = devm_request_threaded_irq(chip->dev, chip->irq, NULL,
					rt9471_irq_handler,
					IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					name, chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s request threaded irq fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	device_init_wakeup(chip->dev, true);

	return 0;
}

static int rt9471_init_irq(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return rt9471_i2c_block_write(chip, RT9471_REG_MASK0,
				      ARRAY_SIZE(chip->irq_mask),
				      chip->irq_mask);
}

static inline int rt9471_get_irq_number(struct rt9471_chip *chip,
					const char *name)
{
	int i = 0;

	if (!name) {
		dev_notice(chip->dev, "%s null name\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(rt9471_irq_mapping_tbl); i++) {
		if (!strcmp(name, rt9471_irq_mapping_tbl[i].name))
			return rt9471_irq_mapping_tbl[i].num;
	}

	return -EINVAL;
}

static inline const char *rt9471_get_irq_name(int irqnum)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(rt9471_irq_mapping_tbl); i++) {
		if (rt9471_irq_mapping_tbl[i].num == irqnum)
			return rt9471_irq_mapping_tbl[i].name;
	}

	return "not found";
}

static inline void rt9471_irq_mask(struct rt9471_chip *chip, int irqnum)
{
	dev_dbg(chip->dev, "%s irq(%d, %s)\n", __func__, irqnum,
		rt9471_get_irq_name(irqnum));
	chip->irq_mask[irqnum / 8] |= (1 << (irqnum % 8));
}

static inline void rt9471_irq_unmask(struct rt9471_chip *chip, int irqnum)
{
	dev_info(chip->dev, "%s irq(%d, %s)\n", __func__, irqnum,
		 rt9471_get_irq_name(irqnum));
	chip->irq_mask[irqnum / 8] &= ~(1 << (irqnum % 8));
}

static int rt9471_parse_dt(struct rt9471_chip *chip)
{
	int ret = 0, len = 0, irqcnt = 0, irqnum = 0;
	struct device_node *parent_np = chip->dev->of_node, *np = NULL;
	struct rt9471_desc *desc = NULL;
	const char *name = NULL;
	char *ceb_name = NULL;

	dev_info(chip->dev, "%s\n", __func__);

	chip->desc = &rt9471_default_desc;

	if (!parent_np) {
		dev_notice(chip->dev, "%s no device node\n", __func__);
		return -EINVAL;
	}
	np = of_get_child_by_name(parent_np, "rt9471");
	if (!np) {
		dev_notice(chip->dev, "%s no rt9471 device node\n", __func__);
		return -EINVAL;
	}

	desc = devm_kzalloc(chip->dev, sizeof(*desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	memcpy(desc, &rt9471_default_desc, sizeof(*desc));

	if (of_property_read_string(np, "charger_name", &desc->chg_name) < 0)
		dev_notice(chip->dev, "%s no charger name\n", __func__);

	if (of_property_read_string(np, "chg_alias_name",
				    &chip->chg_props.alias_name) < 0) {
		dev_notice(chip->dev, "%s no chg alias name\n", __func__);
		chip->chg_props.alias_name = "rt9471_chg";
	}
	dev_info(chip->dev, "%s name %s, alias name %s\n", __func__,
			    desc->chg_name, chip->chg_props.alias_name);

#if (!defined(CONFIG_MTK_GPIO) || defined(CONFIG_MTK_GPIOLIB_STAND))
	ret = of_get_named_gpio(parent_np, "rt,intr_gpio", 0);
	if (ret < 0)
		return ret;
	chip->intr_gpio = ret;
	if (strcmp(desc->chg_name, "secondary_chg") == 0) {
		ret = of_get_named_gpio(parent_np, "rt,ceb_gpio", 0);
		if (ret < 0)
			return ret;
		chip->ceb_gpio = ret;
	}
#else
	ret = of_property_read_u32(parent_np,
				   "rt,intr_gpio_num", &chip->intr_gpio);
	if (ret < 0)
		return ret;
	if (strcmp(desc->chg_name, "secondary_chg") == 0) {
		ret = of_property_read_u32(parent_np, "rt,ceb_gpio_num",
					   &chip->ceb_gpio);
		if (ret < 0)
			return ret;
	}
#endif
	dev_info(chip->dev, "%s intr_gpio %u\n", __func__, chip->intr_gpio);

	/* ceb gpio */
	if (strcmp(desc->chg_name, "secondary_chg") == 0) {
		len = strlen(desc->chg_name);
		ceb_name = devm_kzalloc(chip->dev, len + 10, GFP_KERNEL);
		if (!ceb_name)
			return -ENOMEM;
		snprintf(ceb_name,  len + 10, "%s-ceb-gpio", desc->chg_name);
		ret = devm_gpio_request_one(chip->dev, chip->ceb_gpio,
					    GPIOF_DIR_OUT, ceb_name);
		if (ret < 0) {
			dev_notice(chip->dev, "%s gpio request fail(%d)\n",
					      __func__, ret);
			return ret;
		}
	}
	/* Register map */
	if (of_property_read_u8(np, "rm-slave-addr", &desc->rm_slave_addr) < 0)
		dev_info(chip->dev, "%s no regmap slave addr\n", __func__);
	if (of_property_read_string(np, "rm-name", &desc->rm_name) < 0)
		dev_info(chip->dev, "%s no regmap name\n", __func__);

	/* Charger parameter */
	if (of_property_read_u32(np, "ichg", &desc->ichg) < 0)
		dev_info(chip->dev, "%s no ichg\n", __func__);

	if (of_property_read_u32(np, "aicr", &desc->aicr) < 0)
		dev_info(chip->dev, "%s no aicr\n", __func__);

	if (of_property_read_u32(np, "mivr", &desc->mivr) < 0)
		dev_info(chip->dev, "%s no mivr\n", __func__);

	if (of_property_read_u32(np, "cv", &desc->cv) < 0)
		dev_info(chip->dev, "%s no cv\n", __func__);

	if (of_property_read_u32(np, "ieoc", &desc->ieoc) < 0)
		dev_info(chip->dev, "%s no ieoc\n", __func__);

	if (of_property_read_u32(np, "safe-tmr", &desc->safe_tmr) < 0)
		dev_info(chip->dev, "%s no safety timer\n", __func__);

	if (of_property_read_u32(np, "wdt", &desc->wdt) < 0)
		dev_info(chip->dev, "%s no wdt\n", __func__);

	if (of_property_read_u32(np, "mivr-track", &desc->mivr_track) < 0)
		dev_info(chip->dev, "%s no mivr track\n", __func__);
	if (desc->mivr_track >= RT9471_MIVRTRACK_MAX)
		desc->mivr_track = RT9471_MIVRTRACK_VBAT_300MV;

	desc->en_safe_tmr = of_property_read_bool(np, "en-safe-tmr");
	desc->en_te = of_property_read_bool(np, "en-te");
	desc->en_jeita = of_property_read_bool(np, "en-jeita");
	desc->ceb_invert = of_property_read_bool(np, "ceb-invert");
	desc->dis_i2c_tout = of_property_read_bool(np, "dis-i2c-tout");
	desc->en_qon_rst = of_property_read_bool(np, "en-qon-rst");
	desc->auto_aicr = of_property_read_bool(np, "auto-aicr");

	chip->desc = desc;

	memcpy(chip->irq_mask, rt9471_irq_maskall, RT9471_IRQIDX_MAX);
	while (true) {
		ret = of_property_read_string_index(np, "interrupt-names",
						    irqcnt, &name);
		if (ret < 0)
			break;
		irqcnt++;
		irqnum = rt9471_get_irq_number(chip, name);
		if (irqnum >= 0)
			rt9471_irq_unmask(chip, irqnum);
	}

	return 0;
}

static int rt9471_sw_workaround(struct rt9471_chip *chip)
{
	int ret = 0;
	u8 regval = 0;

	dev_info(chip->dev, "%s\n", __func__);

	ret = rt9471_enable_hidden_mode(chip, true);
	if (ret < 0)
		return ret;

	ret = rt9471_i2c_read_byte(chip, RT9471_REG_HIDDEN_0, &regval);
	if (ret < 0) {
		dev_notice(chip->dev, "%s read HIDDEN_0 fail(%d)\n",
				      __func__, ret);
		goto out;
	}
	chip->chip_rev = (regval & RT9471_CHIP_REV_MASK) >>
			 RT9471_CHIP_REV_SHIFT;
	dev_info(chip->dev, "%s chip_rev = %d\n", __func__, chip->chip_rev);

	/* OTG load transient improvement */
	if (chip->chip_rev <= 3)
		ret = rt9471_i2c_update_bits(chip, RT9471_REG_OTG_HDEN2, 0x10,
					     RT9471_REG_OTG_RES_COMP_MASK);

out:
	rt9471_enable_hidden_mode(chip, false);
	return ret;
}

static int rt9471_init_setting(struct rt9471_chip *chip)
{
	int ret = 0;
	struct rt9471_desc *desc = chip->desc;
	u8 evt[RT9471_IRQIDX_MAX] = {0};

	dev_info(chip->dev, "%s\n", __func__);

	/* Disable WDT during IRQ masked period */
	ret = __rt9471_set_wdt(chip, 0);
	if (ret < 0)
		dev_notice(chip->dev, "%s set wdt fail(%d)\n", __func__, ret);

	/* Mask all IRQs */
	ret = rt9471_i2c_block_write(chip, RT9471_REG_MASK0,
				     ARRAY_SIZE(rt9471_irq_maskall),
				     rt9471_irq_maskall);
	if (ret < 0)
		dev_notice(chip->dev, "%s mask irq fail(%d)\n", __func__, ret);

	/* Clear all IRQs */
	ret = rt9471_i2c_block_read(chip, RT9471_REG_IRQ0, RT9471_IRQIDX_MAX,
				    evt);
	if (ret < 0)
		dev_notice(chip->dev, "%s clear irq fail(%d)\n", __func__, ret);

	ret = __rt9471_set_ichg(chip, desc->ichg);
	if (ret < 0)
		dev_notice(chip->dev, "%s set ichg fail(%d)\n", __func__, ret);

	ret = __rt9471_set_aicr(chip, desc->aicr);
	if (ret < 0)
		dev_notice(chip->dev, "%s set aicr fail(%d)\n", __func__, ret);

	ret = __rt9471_set_mivr(chip, desc->mivr);
	if (ret < 0)
		dev_notice(chip->dev, "%s set mivr fail(%d)\n", __func__, ret);

	ret = __rt9471_set_cv(chip, desc->cv);
	if (ret < 0)
		dev_notice(chip->dev, "%s set cv fail(%d)\n", __func__, ret);

	ret = __rt9471_set_ieoc(chip, desc->ieoc);
	if (ret < 0)
		dev_notice(chip->dev, "%s set ieoc fail(%d)\n", __func__, ret);

	ret = __rt9471_set_safe_tmr(chip, desc->safe_tmr);
	if (ret < 0)
		dev_notice(chip->dev, "%s set safe tmr fail(%d)\n",
				      __func__, ret);

	ret = __rt9471_set_mivrtrack(chip, desc->mivr_track);
	if (ret < 0)
		dev_notice(chip->dev, "%s set mivrtrack fail(%d)\n",
				      __func__, ret);

	ret = __rt9471_enable_safe_tmr(chip, desc->en_safe_tmr);
	if (ret < 0)
		dev_notice(chip->dev, "%s en safe tmr fail(%d)\n",
				      __func__, ret);

	ret = __rt9471_enable_te(chip, desc->en_te);
	if (ret < 0)
		dev_notice(chip->dev, "%s en te fail(%d)\n", __func__, ret);

	ret = __rt9471_enable_jeita(chip, desc->en_jeita);
	if (ret < 0)
		dev_notice(chip->dev, "%s en jeita fail(%d)\n", __func__, ret);

	ret = __rt9471_disable_i2c_tout(chip, desc->dis_i2c_tout);
	if (ret < 0)
		dev_notice(chip->dev, "%s dis i2c tout fail(%d)\n",
				      __func__, ret);

	ret = __rt9471_enable_qon_rst(chip, desc->en_qon_rst);
	if (ret < 0)
		dev_notice(chip->dev, "%s en qon rst fail(%d)\n",
				      __func__, ret);

	ret = __rt9471_enable_autoaicr(chip, desc->auto_aicr);
	if (ret < 0)
		dev_notice(chip->dev, "%s en autoaicr fail(%d)\n",
				      __func__, ret);

	rt9471_enable_bc12(chip, false);

	ret = rt9471_sw_workaround(chip);
	if (ret < 0)
		dev_notice(chip->dev, "%s set sw workaround fail(%d)\n",
				      __func__, ret);

	return 0;
}

static int rt9471_reset_register(struct rt9471_chip *chip)
{
	dev_info(chip->dev, "%s\n", __func__);
	return rt9471_set_bit(chip, RT9471_REG_INFO, RT9471_REGRST_MASK);
}

static bool rt9471_check_devinfo(struct rt9471_chip *chip)
{
	int ret = 0;

	ret = i2c_smbus_read_byte_data(chip->client, RT9471_REG_INFO);
	if (ret < 0) {
		dev_notice(chip->dev, "%s get devinfo fail(%d)\n",
				      __func__, ret);
		return false;
	}
	chip->dev_id = (ret & RT9471_DEVID_MASK) >> RT9471_DEVID_SHIFT;
	if (chip->dev_id != RT9470_DEVID && chip->dev_id != RT9470D_DEVID &&
		chip->dev_id != RT9471_DEVID && chip->dev_id != RT9471D_DEVID) {
		dev_notice(chip->dev, "%s incorrect devid 0x%02X\n",
				      __func__, chip->dev_id);
		return false;
	}
	chip->dev_rev = (ret & RT9471_DEVREV_MASK) >> RT9471_DEVREV_SHIFT;
	dev_info(chip->dev, "%s id = 0x%02X, rev = 0x%02X\n", __func__,
		 chip->dev_id, chip->dev_rev);

	return true;
}

static int __rt9471_dump_registers(struct rt9471_chip *chip)
{
	int i = 0, ret = 0;
	u32 ichg = 0, aicr = 0, mivr = 0, ieoc = 0, cv = 0;
	bool chg_en = 0;
	enum rt9471_ic_stat ic_stat = RT9471_ICSTAT_SLEEP;
	u8 stats[RT9471_STATIDX_MAX] = {0}, regval = 0;

	ret = __rt9471_kick_wdt(chip);

	ret = __rt9471_get_ichg(chip, &ichg);
	ret = __rt9471_get_aicr(chip, &aicr);
	ret = __rt9471_get_mivr(chip, &mivr);
	ret = __rt9471_get_ieoc(chip, &ieoc);
	ret = __rt9471_get_cv(chip, &cv);
	ret = __rt9471_is_chg_enabled(chip, &chg_en);
	ret = __rt9471_get_ic_stat(chip, &ic_stat);
	ret = rt9471_i2c_block_read(chip, RT9471_REG_STAT0, RT9471_STATIDX_MAX,
				    stats);

	if (ic_stat == RT9471_ICSTAT_CHGFAULT) {
		for (i = 0; i < ARRAY_SIZE(rt9471_reg_addr); i++) {
			ret = rt9471_i2c_read_byte(chip, rt9471_reg_addr[i],
						   &regval);
			if (ret < 0)
				continue;
			dev_info(chip->dev, "%s reg0x%02X = 0x%02X\n", __func__,
					    rt9471_reg_addr[i], regval);
		}
	}

	dev_info(chip->dev,
		 "%s ICHG = %dmA, AICR = %dmA, MIVR = %dmV\n",
		 __func__, ichg / 1000, aicr / 1000, mivr / 1000);

	dev_info(chip->dev, "%s IEOC = %dmA, CV = %dmV\n",
		 __func__, ieoc / 1000, cv / 1000);

	dev_info(chip->dev, "%s CHG_EN = %d, IC_STAT = %s\n",
		 __func__, chg_en, rt9471_ic_stat_name[ic_stat]);

	dev_info(chip->dev,
		 "%s STAT0 = 0x%02X, STAT1 = 0x%02X\n", __func__,
		 stats[RT9471_STATIDX_STAT0], stats[RT9471_STATIDX_STAT1]);

	dev_info(chip->dev,
		 "%s STAT2 = 0x%02X, STAT3 = 0x%02X\n", __func__,
		 stats[RT9471_STATIDX_STAT2], stats[RT9471_STATIDX_STAT3]);

	return 0;
}

#ifndef CONFIG_TCPC_CLASS
static void rt9471_init_work_handler(struct work_struct *work)
{
	struct rt9471_chip *chip = container_of(work, struct rt9471_chip,
						init_work);

	mutex_lock(&chip->bc12_lock);
	atomic_set(&chip->vbus_gd, rt9471_is_vbusgd(chip));
	rt9471_bc12_preprocess(chip);
	mutex_unlock(&chip->bc12_lock);
	__rt9471_dump_registers(chip);
}
#endif

static int rt9471_enable_charging(struct charger_device *chg_dev, bool en);
static int rt9471_plug_in(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	dev_info(chip->dev, "%s\n", __func__);

	if (strcmp(chip->desc->chg_name, "primary_chg")) {
		dev_info(chip->dev, "%s not primary_chg\n", __func__);
		return ret;
	}

	/* Enable WDT */
	ret = __rt9471_set_wdt(chip, chip->desc->wdt);
	if (ret < 0) {
		dev_notice(chip->dev, "%s set wdt fail(%d)\n", __func__, ret);
		return ret;
	}

	/* Enable charging */
	ret = rt9471_enable_charging(chg_dev, true);
	if (ret < 0)
		dev_notice(chip->dev, "%s en fail(%d)\n", __func__, ret);

	return ret;
}

static int rt9471_plug_out(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	dev_info(chip->dev, "%s\n", __func__);

	/* Disable charging */
	ret = rt9471_enable_charging(chg_dev, false);
	if (ret < 0) {
		dev_notice(chip->dev, "%s en fail(%d)\n", __func__, ret);
		return ret;
	}

	/* Disable WDT */
	ret = __rt9471_set_wdt(chip, 0);
	if (ret < 0)
		dev_notice(chip->dev, "%s set wdt fail(%d)\n", __func__, ret);

	/* Enable HZ mode of secondary charger */
	if (strcmp(chip->desc->chg_name, "secondary_chg") == 0) {
		ret = __rt9471_enable_hz(chip, true);
		if (ret < 0)
			dev_notice(chip->dev, "%s en hz of sec chg fail(%d)\n",
					      __func__, ret);
	}

	return ret;
}

static int rt9471_enable_charging(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	/* Set hz/ceb pin for secondary charger */
	if (strcmp(chip->desc->chg_name, "secondary_chg") == 0) {
		ret = __rt9471_enable_hz(chip, !en);
		if (ret < 0) {
			dev_notice(chip->dev, "%s set hz of sec chg fail(%d)\n",
					      __func__, ret);
			return ret;
		}
		if (chip->desc->ceb_invert)
			gpio_set_value(chip->ceb_gpio, en);
		else
			gpio_set_value(chip->ceb_gpio, !en);
	}

	return __rt9471_enable_chg(chip, en);
}

static int rt9471_is_charging_enabled(struct charger_device *chg_dev, bool *en)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_is_chg_enabled(chip, en);
}

static int rt9471_get_ichg(struct charger_device *chg_dev, u32 *uA)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_get_ichg(chip, uA);
}

static int rt9471_set_ichg(struct charger_device *chg_dev, u32 uA)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_set_ichg(chip, uA);
}

static int rt9471_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	*uA = rt9471_closest_value(RT9471_ICHG_MIN, RT9471_ICHG_MAX,
				   RT9471_ICHG_STEP, 0);
	return 0;
}

static int rt9471_get_cv(struct charger_device *chg_dev, u32 *uV)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_get_cv(chip, uV);
}

static int rt9471_set_cv(struct charger_device *chg_dev, u32 uV)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_set_cv(chip, uV);
}

static int rt9471_get_aicr(struct charger_device *chg_dev, u32 *uA)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_get_aicr(chip, uA);
}

static int rt9471_set_aicr(struct charger_device *chg_dev, u32 uA)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_set_aicr(chip, uA);
}

static int rt9471_get_min_aicr(struct charger_device *chg_dev, u32 *uA)
{
	*uA = rt9471_closest_value(RT9471_AICR_MIN, RT9471_AICR_MAX,
				   RT9471_AICR_STEP, 0);
	return 0;
}

static int rt9471_get_ieoc(struct charger_device *chg_dev, u32 *uA)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_get_ieoc(chip, uA);
}

static int rt9471_set_ieoc(struct charger_device *chg_dev, u32 uA)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_set_ieoc(chip, uA);
}

static int rt9471_kick_wdt(struct charger_device *chg_dev)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_kick_wdt(chip);
}

static int rt9471_event(struct charger_device *chg_dev, u32 event, u32 args)
{
	switch (event) {
	case EVENT_EOC:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_EOC);
		break;
	case EVENT_RECHARGE:
		charger_dev_notify(chg_dev, CHARGER_DEV_NOTIFY_RECHG);
		break;
	default:
		break;
	}

	return 0;
}

static int rt9471_set_mivr(struct charger_device *chg_dev, u32 uV)
{
	int ret = 0;
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = __rt9471_set_mivr(chip, uV);
	if (ret >= 0)
		chip->mivr = uV;

	return ret;
}

static int rt9471_get_mivr_state(struct charger_device *chg_dev, bool *in_loop)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return rt9471_i2c_test_bit(chip, RT9471_REG_STAT1,
				   RT9471_ST_MIVR_SHIFT, in_loop);
}

static int rt9471_enable_powerpath(struct charger_device *chg_dev, bool en)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);
	u32 mivr = (en ? chip->mivr : RT9471_MIVR_MAX);

	dev_info(chip->dev, "%s en = %d\n", __func__, en);

	return __rt9471_set_mivr(chip, mivr);
}

static int rt9471_is_powerpath_enabled(struct charger_device *chg_dev, bool *en)
{
	int ret = 0;
	u32 mivr = 0;
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	ret = __rt9471_get_mivr(chip, &mivr);
	if (ret < 0)
		return ret;
	*en = (mivr == RT9471_MIVR_MAX ? false : true);

	return ret;
}

static int rt9471_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_enable_safe_tmr(chip, en);
}

static int rt9471_is_safety_timer_enabled(struct charger_device *chg_dev,
					  bool *en)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return rt9471_i2c_test_bit(chip, RT9471_REG_CHGTIMER,
				   RT9471_SAFETMR_EN_SHIFT, en);
}

static int rt9471_enable_te(struct charger_device *chg_dev, bool en)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_enable_te(chip, en);
}

static int rt9471_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	if (en) {
		ret = __rt9471_set_wdt(chip, chip->desc->wdt);
		if (ret < 0) {
			dev_notice(chip->dev, "%s set wdt fail(%d)\n",
					      __func__, ret);
			return ret;
		}
	}

	ret = __rt9471_enable_otg(chip, en);
	if (ret < 0) {
		dev_notice(chip->dev, "%s en otg fail(%d)\n", __func__, ret);
		return ret;
	}

	if (!en) {
		ret = __rt9471_set_wdt(chip, 0);
		if (ret < 0)
			dev_notice(chip->dev, "%s set wdt fail(%d)\n",
					      __func__, ret);
	}

	return ret;
}

static int rt9471_enable_discharge(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	dev_info(chip->dev, "%s en = %d\n", __func__, en);

	ret = rt9471_enable_hidden_mode(chip, true);
	if (ret < 0)
		return ret;

	ret = (en ? rt9471_set_bit : rt9471_clr_bit)(chip,
		RT9471_REG_TOP_HDEN, RT9471_FORCE_EN_VBUS_SINK_MASK);
	if (ret < 0)
		dev_notice(chip->dev, "%s en = %d fail(%d)\n",
				      __func__, en, ret);

	rt9471_enable_hidden_mode(chip, false);

	return ret;
}

static int rt9471_set_boost_current_limit(struct charger_device *chg_dev,
					  u32 uA)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_set_otgcc(chip, uA);
}

static int rt9471_enable_chg_type_det(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
#ifdef CONFIG_TCPC_CLASS
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	dev_info(chip->dev, "%s en = %d\n", __func__, en);

	mutex_lock(&chip->bc12_lock);
	atomic_set(&chip->vbus_gd, en);
	ret = (en ? rt9471_bc12_preprocess : rt9471_bc12_postprocess)(chip);
	mutex_unlock(&chip->bc12_lock);
	if (ret < 0)
		dev_notice(chip->dev, "%s en bc12 fail(%d)\n", __func__, ret);
#endif /* CONFIG_TCPC_CLASS */
	return ret;
}

static int rt9471_reset_eoc_state(struct charger_device *chg_dev)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return rt9471_set_bit(chip, RT9471_REG_EOC, RT9471_EOC_RST_MASK);
}

static int rt9471_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	int ret = 0;
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);
	enum rt9471_ic_stat ic_stat = RT9471_ICSTAT_SLEEP;

	ret = __rt9471_get_ic_stat(chip, &ic_stat);
	if (ret < 0)
		return ret;
	*done = (ic_stat == RT9471_ICSTAT_CHGDONE);

	return ret;
}

static int rt9471_dump_registers(struct charger_device *chg_dev)
{
	struct rt9471_chip *chip = dev_get_drvdata(&chg_dev->dev);

	return __rt9471_dump_registers(chip);
}

static struct charger_ops rt9471_chg_ops = {
	/* cable plug in/out */
	.plug_in = rt9471_plug_in,
	.plug_out = rt9471_plug_out,

	/* enable/disable charger */
	.enable = rt9471_enable_charging,
	.is_enabled = rt9471_is_charging_enabled,

	/* get/set charging current*/
	.get_charging_current = rt9471_get_ichg,
	.set_charging_current = rt9471_set_ichg,
	.get_min_charging_current = rt9471_get_min_ichg,

	/* set cv */
	.get_constant_voltage = rt9471_get_cv,
	.set_constant_voltage = rt9471_set_cv,

	/* set input_current */
	.get_input_current = rt9471_get_aicr,
	.set_input_current = rt9471_set_aicr,
	.get_min_input_current = rt9471_get_min_aicr,

	/* set termination current */
	.get_eoc_current = rt9471_get_ieoc,
	.set_eoc_current = rt9471_set_ieoc,

	/* kick wdt */
	.kick_wdt = rt9471_kick_wdt,

	.event = rt9471_event,

	.set_mivr = rt9471_set_mivr,
	.get_mivr_state = rt9471_get_mivr_state,

	/* enable/disable powerpath */
	.enable_powerpath = rt9471_enable_powerpath,
	.is_powerpath_enabled = rt9471_is_powerpath_enabled,

	/* enable/disable charging safety timer */
	.enable_safety_timer = rt9471_enable_safety_timer,
	.is_safety_timer_enabled = rt9471_is_safety_timer_enabled,

	/* enable term */
	.enable_termination = rt9471_enable_te,

	/* OTG */
	.enable_otg = rt9471_enable_otg,
	.enable_discharge = rt9471_enable_discharge,
	.set_boost_current_limit = rt9471_set_boost_current_limit,

	/* charger type detection */
	.enable_chg_type_det = rt9471_enable_chg_type_det,

	/* reset EOC state */
	.reset_eoc_state = rt9471_reset_eoc_state,

	.is_charging_done = rt9471_is_charging_done,
	.dump_registers = rt9471_dump_registers,
};

static ssize_t shipping_mode_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0, tmp = 0;
	struct rt9471_chip *chip = dev_get_drvdata(dev);

	ret = kstrtoint(buf, 10, &tmp);
	if (ret < 0) {
		dev_notice(dev, "%s parsing number fail(%d)\n", __func__, ret);
		return -EINVAL;
	}
	if (tmp != 5526789)
		return -EINVAL;
	ret = rt9471_reset_register(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s reset register fail(%d)\n",
				      __func__, ret);
		return ret;
	}
	ret = __rt9471_enable_shipmode(chip, true);
	if (ret < 0) {
		dev_notice(dev, "%s enter shipping mode fail(%d)\n",
				__func__, ret);
		return ret;
	}

	return count;
}

static const DEVICE_ATTR_WO(shipping_mode);

static int rt9471_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret = 0;
	struct rt9471_chip *chip = NULL;

	dev_info(&client->dev, "%s (%s)\n", __func__, RT9471_DRV_VERSION);

	chip = devm_kzalloc(&client->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;
	chip->client = client;
	chip->dev = &client->dev;
	sema_init(&chip->suspend_lock, 1);
	mutex_init(&chip->io_lock);
	mutex_init(&chip->bc12_lock);
	mutex_init(&chip->bc12_en_lock);
	mutex_init(&chip->hidden_mode_lock);
	chip->hidden_mode_cnt = 0;
#ifndef CONFIG_TCPC_CLASS
	INIT_WORK(&chip->init_work, rt9471_init_work_handler);
#endif
	atomic_set(&chip->vbus_gd, 0);
	chip->attach = false;
	chip->port = RT9471_PORTSTAT_NOINFO;
	chip->chg_type = CHARGER_UNKNOWN;
	i2c_set_clientdata(client, chip);

	if (!rt9471_check_devinfo(chip)) {
		ret = -ENODEV;
		goto err_nodev;
	}

	if (chip->dev_id == RT9470D_DEVID || chip->dev_id == RT9471D_DEVID) {
		chip->bc12_en_kthread =
			kthread_run(rt9471_bc12_en_kthread, chip,
				    "rt9471.%s", dev_name(chip->dev));
		if (IS_ERR_OR_NULL(chip->bc12_en_kthread)) {
			ret = PTR_ERR(chip->bc12_en_kthread);
			dev_notice(chip->dev, "%s kthread run fail(%d)\n",
					      __func__, ret);
			goto err_kthread_run;
		}
		chip->bc12_en_buf[0] = chip->bc12_en_buf[1] = -1;
		chip->bc12_en_buf_idx = 0;
		init_completion(&chip->bc12_en_req);
	}

	ret = rt9471_parse_dt(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s parse dt fail(%d)\n", __func__, ret);
		goto err_parse_dt;
	}
	chip->mivr = chip->desc->mivr;

#ifdef CONFIG_RT_REGMAP
	ret = rt9471_register_rt_regmap(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s register rt regmap fail(%d)\n",
				      __func__, ret);
		goto err_register_rm;
	}
#endif

	ret = rt9471_reset_register(chip);
	if (ret < 0)
		dev_notice(chip->dev, "%s reset register fail(%d)\n",
				      __func__, ret);

	ret = rt9471_init_setting(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s init fail(%d)\n", __func__, ret);
		goto err_init;
	}

	/* Register charger device */
	chip->chg_dev = charger_device_register(chip->desc->chg_name,
			chip->dev, chip, &rt9471_chg_ops, &chip->chg_props);
	if (IS_ERR_OR_NULL(chip->chg_dev)) {
		ret = PTR_ERR(chip->chg_dev);
		dev_notice(chip->dev, "%s register chg dev fail(%d)\n",
				      __func__, ret);
		goto err_register_chg_dev;
	}

	ret = rt9471_register_irq(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s register irq fail(%d)\n",
				      __func__, ret);
		goto err_register_irq;
	}

	ret = rt9471_init_irq(chip);
	if (ret < 0) {
		dev_notice(chip->dev, "%s init irq fail(%d)\n", __func__, ret);
		goto err_init_irq;
	}

	ret = device_create_file(chip->dev, &dev_attr_shipping_mode);
	if (ret < 0) {
		dev_notice(chip->dev, "%s create file fail(%d)\n",
				      __func__, ret);
		goto err_create_file;
	}

#ifndef CONFIG_TCPC_CLASS
	if (strcmp(chip->desc->chg_name, "primary_chg") == 0)
		schedule_work(&chip->init_work);
#else
	__rt9471_dump_registers(chip);
#endif
	dev_info(chip->dev, "%s successfully\n", __func__);
	return 0;

err_create_file:
err_init_irq:
err_register_irq:
	charger_device_unregister(chip->chg_dev);
err_register_chg_dev:
	power_supply_put(chip->psy);
err_init:
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(chip->rm_dev);
err_register_rm:
#endif /* CONFIG_RT_REGMAP */
err_parse_dt:
err_kthread_run:
err_nodev:
	mutex_destroy(&chip->io_lock);
	mutex_destroy(&chip->bc12_lock);
	mutex_destroy(&chip->bc12_en_lock);
	mutex_destroy(&chip->hidden_mode_lock);
	devm_kfree(chip->dev, chip);
	return ret;
}

static void rt9471_shutdown(struct i2c_client *client)
{
	struct rt9471_chip *chip = i2c_get_clientdata(client);

	pr_info("%s\n", __func__);
	if (chip)
		rt9471_reset_register(chip);
}

static int rt9471_remove(struct i2c_client *client)
{
	struct rt9471_chip *chip = i2c_get_clientdata(client);

	if (chip) {
		dev_info(chip->dev, "%s\n", __func__);
		device_remove_file(chip->dev, &dev_attr_shipping_mode);
		charger_device_unregister(chip->chg_dev);
		power_supply_put(chip->psy);
#ifdef CONFIG_RT_REGMAP
		rt_regmap_device_unregister(chip->rm_dev);
#endif /* CONFIG_RT_REGMAP */
		mutex_destroy(&chip->io_lock);
		mutex_destroy(&chip->bc12_lock);
		mutex_destroy(&chip->bc12_en_lock);
		mutex_destroy(&chip->hidden_mode_lock);
	}

	return 0;
}

static int rt9471_suspend(struct device *dev)
{
	struct rt9471_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	if (device_may_wakeup(dev))
		enable_irq_wake(chip->irq);
	disable_irq(chip->irq);
	down(&chip->suspend_lock);

	return 0;
}

static int rt9471_resume(struct device *dev)
{
	struct rt9471_chip *chip = dev_get_drvdata(dev);

	dev_info(dev, "%s\n", __func__);
	up(&chip->suspend_lock);
	enable_irq(chip->irq);
	if (device_may_wakeup(dev))
		disable_irq_wake(chip->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rt9471_pm_ops, rt9471_suspend, rt9471_resume);

static const struct of_device_id rt9471_of_device_id[] = {
	{ .compatible = "richtek,rt9471", },
	{ .compatible = "richtek,swchg", },
	{ },
};
MODULE_DEVICE_TABLE(of, rt9471_of_device_id);

static const struct i2c_device_id rt9471_i2c_device_id[] = {
	{ "rt9471", 0 },
	{ },
};
MODULE_DEVICE_TABLE(i2c, rt9471_i2c_device_id);

static struct i2c_driver rt9471_i2c_driver = {
	.driver = {
		.name = "rt9471",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt9471_of_device_id),
		.pm = &rt9471_pm_ops,
	},
	.probe = rt9471_probe,
	.shutdown = rt9471_shutdown,
	.remove = rt9471_remove,
	.id_table = rt9471_i2c_device_id,
};
module_i2c_driver(rt9471_i2c_driver);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("ShuFanLee <shufan_lee@richtek.com>");
MODULE_DESCRIPTION("RT9471 Charger Driver");
MODULE_VERSION(RT9471_DRV_VERSION);
/*
 * Release Note
 * 1.0.5
 * (1) Add suspend_lock
 * (2) Add support for RT9470/RT9470D
 * (3) Sync with LK Driver
 * (4) Use IRQ to wait chg_rdy
 * (5) disable_irq()/enable_irq() in suspend()/resume()
 * (6) bc12_en in the kthread
 *
 * 1.0.4
 * (1) Use type u8 for regval in __rt9471_i2c_read_byte()
 *
 * 1.0.3
 * (1) Add shipping mode sys node
 * (2) Keep D+ at 0.6V after DCP got detected
 *
 * 1.0.2
 * (1) Kick WDT in __rt9471_dump_registers()
 *
 * 1.0.1
 * (1) Keep mivr via chg_ops
 *
 * 1.0.0
 * (1) Initial released
 */
