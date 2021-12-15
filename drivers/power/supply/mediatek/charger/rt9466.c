/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Author Andrew-sh.Cheng <andrew-sh.cheng@mediatek.com>
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
#include <linux/workqueue.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/pm_runtime.h>

#include <mt-plat/mtk_boot.h>
#ifdef CONFIG_RT_REGMAP
#include <mt-plat/rt-regmap.h>
#endif

#include "mtk_charger_intf.h"
#include "rt9466.h"
#define I2C_ACCESS_MAX_RETRY	5
#define RT9466_DRV_VERSION	"1.0.12_MTK"

#define RTLOG_LEVEL 0

#define rt_info(fmt, args...)   \
do {\
	if (RTLOG_LEVEL >= LOGLEVEL_INFO) {\
		pr_info(fmt, ##args); \
	} \
} while (0)

#define rt_debug(fmt, args...)   \
do {\
	if (RTLOG_LEVEL >= LOGLEVEL_DEBUG) {\
		pr_debug(fmt, ##args); \
	} \
} while (0)

/* ======================= */
/* RT9466 Parameter        */
/* ======================= */

static const u32 rt9466_boost_oc_threshold[] = {
	500000, 700000, 1100000, 1300000, 1800000, 2100000, 2400000, 3000000,
}; /* uA */

static const u32 rt9466_safety_timer[] = {
	4, 6, 8, 10, 12, 14, 16, 20,
}; /* hour */

enum rt9466_irq_idx {
	RT9466_IRQIDX_CHG_STATC = 0,
	RT9466_IRQIDX_CHG_FAULT,
	RT9466_IRQIDX_TS_STATC,
	RT9466_IRQIDX_CHG_IRQ1,
	RT9466_IRQIDX_CHG_IRQ2,
	RT9466_IRQIDX_CHG_IRQ3,
	RT9466_IRQIDX_MAX,
};

enum rt9466_irq_stat {
	RT9466_IRQSTAT_CHG_STATC = 0,
	RT9466_IRQSTAT_CHG_FAULT,
	RT9466_IRQSTAT_TS_STATC,
	RT9466_IRQSTAT_MAX,
};

static const u8 rt9466_irq_maskall[RT9466_IRQIDX_MAX] = {
	0xF0, 0xF0, 0xFF, 0xFF, 0xFF, 0xFF
};

struct irq_mapping_tbl {
	const char *name;
	const int id;
};

#define RT9466_IRQ_MAPPING(_name, _id) {.name = #_name, .id = _id}
static const struct irq_mapping_tbl rt9466_irq_mapping_tbl[] = {
	RT9466_IRQ_MAPPING(chg_treg, 4),
	RT9466_IRQ_MAPPING(chg_aicr, 5),
	RT9466_IRQ_MAPPING(chg_mivr, 6),
	RT9466_IRQ_MAPPING(pwr_rdy, 7),
	RT9466_IRQ_MAPPING(chg_vsysuv, 12),
	RT9466_IRQ_MAPPING(chg_vsysov, 13),
	RT9466_IRQ_MAPPING(chg_vbatov, 14),
	RT9466_IRQ_MAPPING(chg_vbusov, 15),
	RT9466_IRQ_MAPPING(ts_batcold, 20),
	RT9466_IRQ_MAPPING(ts_batcool, 21),
	RT9466_IRQ_MAPPING(ts_batwarm, 22),
	RT9466_IRQ_MAPPING(ts_bathot, 23),
	RT9466_IRQ_MAPPING(ts_statci, 24),
	RT9466_IRQ_MAPPING(chg_faulti, 25),
	RT9466_IRQ_MAPPING(chg_statci, 26),
	RT9466_IRQ_MAPPING(chg_tmri, 27),
	RT9466_IRQ_MAPPING(chg_batabsi, 28),
	RT9466_IRQ_MAPPING(chg_adpbadi, 29),
	RT9466_IRQ_MAPPING(chg_rvpi, 30),
	RT9466_IRQ_MAPPING(otpi, 31),
	RT9466_IRQ_MAPPING(chg_aiclmeasi, 32),
	RT9466_IRQ_MAPPING(chg_ichgmeasi, 33),
	RT9466_IRQ_MAPPING(wdtmri, 35),
	RT9466_IRQ_MAPPING(ssfinishi, 36),
	RT9466_IRQ_MAPPING(chg_rechgi, 37),
	RT9466_IRQ_MAPPING(chg_termi, 38),
	RT9466_IRQ_MAPPING(chg_ieoci, 39),
	RT9466_IRQ_MAPPING(adc_donei, 40),
	RT9466_IRQ_MAPPING(pumpx_donei, 41),
	RT9466_IRQ_MAPPING(bst_batuvi, 45),
	RT9466_IRQ_MAPPING(bst_midovi, 46),
	RT9466_IRQ_MAPPING(bst_olpi, 47),
};

enum rt9466_charging_status {
	RT9466_CHG_STATUS_READY = 0,
	RT9466_CHG_STATUS_PROGRESS,
	RT9466_CHG_STATUS_DONE,
	RT9466_CHG_STATUS_FAULT,
	RT9466_CHG_STATUS_MAX,
};

static const char *rt9466_chg_status_name[RT9466_CHG_STATUS_MAX] = {
	"ready", "progress", "done", "fault",
};

static const u8 rt9466_val_en_hidden_mode[] = {
	0x49, 0x32, 0xB6, 0x27, 0x48, 0x18, 0x03, 0xE2,
};

enum rt9466_iin_limit_sel {
	RT9466_IIMLMTSEL_PSEL_OTG,
	RT9466_IINLMTSEL_AICR = 2,
	RT9466_IINLMTSEL_LOWER_LEVEL, /* lower of above two */
};

enum rt9466_adc_sel {
	RT9466_ADC_VBUS_DIV5 = 1,
	RT9466_ADC_VBUS_DIV2,
	RT9466_ADC_VSYS,
	RT9466_ADC_VBAT,
	RT9466_ADC_TS_BAT = 6,
	RT9466_ADC_IBUS = 8,
	RT9466_ADC_IBAT,
	RT9466_ADC_REGN = 11,
	RT9466_ADC_TEMP_JC,
	RT9466_ADC_MAX,
};

/*
 * Unit for each ADC parameter
 * 0 stands for reserved
 * For TS_BAT, the real unit is 0.25.
 * Here we use 25, please remember to divide 100 while showing the value
 */
static const int rt9466_adc_unit[RT9466_ADC_MAX] = {
	0,
	RT9466_ADC_UNIT_VBUS_DIV5,
	RT9466_ADC_UNIT_VBUS_DIV2,
	RT9466_ADC_UNIT_VSYS,
	RT9466_ADC_UNIT_VBAT,
	0,
	RT9466_ADC_UNIT_TS_BAT,
	0,
	RT9466_ADC_UNIT_IBUS,
	RT9466_ADC_UNIT_IBAT,
	0,
	RT9466_ADC_UNIT_REGN,
	RT9466_ADC_UNIT_TEMP_JC,
};

static const int rt9466_adc_offset[RT9466_ADC_MAX] = {
	0,
	RT9466_ADC_OFFSET_VBUS_DIV5,
	RT9466_ADC_OFFSET_VBUS_DIV2,
	RT9466_ADC_OFFSET_VSYS,
	RT9466_ADC_OFFSET_VBAT,
	0,
	RT9466_ADC_OFFSET_TS_BAT,
	0,
	RT9466_ADC_OFFSET_IBUS,
	RT9466_ADC_OFFSET_IBAT,
	0,
	RT9466_ADC_OFFSET_REGN,
	RT9466_ADC_OFFSET_TEMP_JC,
};

struct rt9466_desc {
	u32 ichg;	/* uA */
	u32 aicr;	/* uA */
	u32 mivr;	/* uV */
	u32 cv;		/* uV */
	u32 ieoc;	/* uA */
	u32 safety_timer;	/* hour */
	u32 ircmp_resistor;	/* uohm */
	u32 ircmp_vclamp;	/* uV */
	bool en_te;
	bool en_wdt;
	bool en_irq_pulse;
	bool en_jeita;
	int regmap_represent_slave_addr;
	const char *regmap_name;
	const char *chg_dev_name;
	bool ceb_invert;
};

/* These default values will be applied if there's no property in dts */
static struct rt9466_desc rt9466_default_desc = {
	.ichg = 2000000,	/* uA */
	.aicr = 500000,		/* uA */
	.mivr = 4400000,	/* uV */
	.cv = 4350000,		/* uA */
	.ieoc = 250000,		/* uA */
	.safety_timer = 12,
#ifdef CONFIG_MTK_BIF_SUPPORT
	.ircmp_resistor = 0,		/* uohm */
	.ircmp_vclamp = 0,		/* uV */
#else
	.ircmp_resistor = 25000,	/* uohm */
	.ircmp_vclamp = 32000,		/* uV */
#endif /* CONFIG_MTK_BIF_SUPPORT */
	.en_te = true,
	.en_wdt = true,
	.en_irq_pulse = false,
	.en_jeita = false,
	.regmap_represent_slave_addr = RT9466_SLAVE_ADDR,
	.regmap_name = "rt9466",
	.chg_dev_name = "primary_chg",
	.ceb_invert = false,
};

struct rt9466_info {
	struct i2c_client *client;
	struct mutex i2c_access_lock;
	struct mutex adc_access_lock;
	struct mutex irq_access_lock;
	struct mutex aicr_access_lock;
	struct mutex ichg_access_lock;
	struct mutex pe_access_lock;
	struct mutex hidden_mode_lock;
	struct mutex ieoc_lock;
	struct device *dev;
	struct charger_device *chg_dev;
	struct charger_properties chg_props;
	struct rt9466_desc *desc;
	wait_queue_head_t wait_queue;
	int irq;
	int aicr_limit;
	u32 intr_gpio;
	u32 ceb_gpio;
	u8 chip_rev;
	u8 irq_flag[RT9466_IRQIDX_MAX];
	u8 irq_stat[RT9466_IRQSTAT_MAX];
	u8 irq_mask[RT9466_IRQIDX_MAX];
	u32 hidden_mode_cnt;
	u32 ieoc;
	bool ieoc_wkard;
	struct work_struct init_work;
#ifdef CONFIG_RT_REGMAP
	struct rt_regmap_device *regmap_dev;
	struct rt_regmap_properties *regmap_prop;
#endif /* CONFIG_RT_REGMAP */
};

/* ======================= */
/* Register Address        */
/* ======================= */

static const unsigned char rt9466_reg_addr[] = {
	RT9466_REG_CORE_CTRL0,
	RT9466_REG_CHG_CTRL1,
	RT9466_REG_CHG_CTRL2,
	RT9466_REG_CHG_CTRL3,
	RT9466_REG_CHG_CTRL4,
	RT9466_REG_CHG_CTRL5,
	RT9466_REG_CHG_CTRL6,
	RT9466_REG_CHG_CTRL7,
	RT9466_REG_CHG_CTRL8,
	RT9466_REG_CHG_CTRL9,
	RT9466_REG_CHG_CTRL10,
	RT9466_REG_CHG_CTRL11,
	RT9466_REG_CHG_CTRL12,
	RT9466_REG_CHG_CTRL13,
	RT9466_REG_CHG_CTRL14,
	RT9466_REG_CHG_CTRL15,
	RT9466_REG_CHG_CTRL16,
	RT9466_REG_CHG_ADC,
	RT9466_REG_CHG_CTRL19,
	RT9466_REG_CHG_CTRL17,
	RT9466_REG_CHG_CTRL18,
	RT9466_REG_DEVICE_ID,
	RT9466_REG_CHG_STAT,
	RT9466_REG_CHG_NTC,
	RT9466_REG_ADC_DATA_H,
	RT9466_REG_ADC_DATA_L,
	RT9466_REG_CHG_STATC,
	RT9466_REG_CHG_FAULT,
	RT9466_REG_TS_STATC,
	/* Skip IRQ evt to prevent reading clear while dumping registers */
	RT9466_REG_CHG_STATC_CTRL,
	RT9466_REG_CHG_FAULT_CTRL,
	RT9466_REG_TS_STATC_CTRL,
	RT9466_REG_CHG_IRQ1_CTRL,
	RT9466_REG_CHG_IRQ2_CTRL,
	RT9466_REG_CHG_IRQ3_CTRL,
};

/* ========= */
/* RT Regmap */
/* ========= */

#ifdef CONFIG_RT_REGMAP
RT_REG_DECL(RT9466_REG_CORE_CTRL0, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL4, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL5, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL6, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL7, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL8, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL9, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL10, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL11, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL12, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL13, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL14, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL15, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL16, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_ADC, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL19, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL17, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_CTRL18, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_HIDDEN_CTRL1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_HIDDEN_CTRL2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_HIDDEN_CTRL4, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_HIDDEN_CTRL6, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_HIDDEN_CTRL7, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_HIDDEN_CTRL8, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_HIDDEN_CTRL9, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_HIDDEN_CTRL15, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_DEVICE_ID, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_STAT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_NTC, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_ADC_DATA_H, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_ADC_DATA_L, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_STATC, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_FAULT, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_TS_STATC, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_IRQ1, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_IRQ2, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_IRQ3, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_STATC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_FAULT_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_TS_STATC_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_IRQ1_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_IRQ2_CTRL, 1, RT_VOLATILE, {});
RT_REG_DECL(RT9466_REG_CHG_IRQ3_CTRL, 1, RT_VOLATILE, {});

static const rt_register_map_t rt9466_regmap_map[] = {
	RT_REG(RT9466_REG_CORE_CTRL0),
	RT_REG(RT9466_REG_CHG_CTRL1),
	RT_REG(RT9466_REG_CHG_CTRL2),
	RT_REG(RT9466_REG_CHG_CTRL3),
	RT_REG(RT9466_REG_CHG_CTRL4),
	RT_REG(RT9466_REG_CHG_CTRL5),
	RT_REG(RT9466_REG_CHG_CTRL6),
	RT_REG(RT9466_REG_CHG_CTRL7),
	RT_REG(RT9466_REG_CHG_CTRL8),
	RT_REG(RT9466_REG_CHG_CTRL9),
	RT_REG(RT9466_REG_CHG_CTRL10),
	RT_REG(RT9466_REG_CHG_CTRL11),
	RT_REG(RT9466_REG_CHG_CTRL12),
	RT_REG(RT9466_REG_CHG_CTRL13),
	RT_REG(RT9466_REG_CHG_CTRL14),
	RT_REG(RT9466_REG_CHG_CTRL15),
	RT_REG(RT9466_REG_CHG_CTRL16),
	RT_REG(RT9466_REG_CHG_ADC),
	RT_REG(RT9466_REG_CHG_CTRL19),
	RT_REG(RT9466_REG_CHG_CTRL17),
	RT_REG(RT9466_REG_CHG_CTRL18),
	RT_REG(RT9466_REG_CHG_HIDDEN_CTRL1),
	RT_REG(RT9466_REG_CHG_HIDDEN_CTRL2),
	RT_REG(RT9466_REG_CHG_HIDDEN_CTRL4),
	RT_REG(RT9466_REG_CHG_HIDDEN_CTRL6),
	RT_REG(RT9466_REG_CHG_HIDDEN_CTRL7),
	RT_REG(RT9466_REG_CHG_HIDDEN_CTRL8),
	RT_REG(RT9466_REG_CHG_HIDDEN_CTRL9),
	RT_REG(RT9466_REG_CHG_HIDDEN_CTRL15),
	RT_REG(RT9466_REG_DEVICE_ID),
	RT_REG(RT9466_REG_CHG_STAT),
	RT_REG(RT9466_REG_CHG_NTC),
	RT_REG(RT9466_REG_ADC_DATA_H),
	RT_REG(RT9466_REG_ADC_DATA_L),
	RT_REG(RT9466_REG_CHG_STATC),
	RT_REG(RT9466_REG_CHG_FAULT),
	RT_REG(RT9466_REG_TS_STATC),
	RT_REG(RT9466_REG_CHG_IRQ1),
	RT_REG(RT9466_REG_CHG_IRQ2),
	RT_REG(RT9466_REG_CHG_IRQ3),
	RT_REG(RT9466_REG_CHG_STATC_CTRL),
	RT_REG(RT9466_REG_CHG_FAULT_CTRL),
	RT_REG(RT9466_REG_TS_STATC_CTRL),
	RT_REG(RT9466_REG_CHG_IRQ1_CTRL),
	RT_REG(RT9466_REG_CHG_IRQ2_CTRL),
	RT_REG(RT9466_REG_CHG_IRQ3_CTRL),
};
#endif /* CONFIG_RT_REGMAP */

/* ========================= */
/* I2C operations            */
/* ========================= */

static int rt9466_device_read(void *client, u32 addr, int leng, void *dst)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_read_i2c_block_data(i2c, addr, leng, dst);
}

static int rt9466_device_write(void *client, u32 addr, int leng,
	const void *src)
{
	struct i2c_client *i2c = (struct i2c_client *)client;

	return i2c_smbus_write_i2c_block_data(i2c, addr, leng, src);
}

#ifdef CONFIG_RT_REGMAP
static struct rt_regmap_fops rt9466_regmap_fops = {
	.read_device = rt9466_device_read,
	.write_device = rt9466_device_write,
};

static int rt9466_register_rt_regmap(struct rt9466_info *info)
{
	int ret = 0;
	struct i2c_client *client = info->client;
	struct rt_regmap_properties *prop = NULL;

	dev_info("%s\n", __func__);

	prop = devm_kzalloc(&client->dev, sizeof(struct rt_regmap_properties),
		GFP_KERNEL);
	if (!prop)
		return -ENOMEM;

	prop->name = info->desc->regmap_name;
	prop->aliases = info->desc->regmap_name;
	prop->register_num = ARRAY_SIZE(rt9466_regmap_map);
	prop->rm = rt9466_regmap_map;
	prop->rt_regmap_mode = RT_SINGLE_BYTE | RT_CACHE_DISABLE |
		RT_IO_PASS_THROUGH;
	prop->io_log_en = 0;

	info->regmap_prop = prop;
	info->regmap_dev = rt_regmap_device_register_ex(info->regmap_prop,
		&rt9466_regmap_fops, &client->dev, client,
		info->desc->regmap_represent_slave_addr, info);
	if (!info->regmap_dev) {
		rt_debug("%s: register regmap dev fail\n", __func__);
		return -EIO;
	}

	return ret;
}
#endif /* CONFIG_RT_REGMAP */

static inline int __rt9466_i2c_write_byte(struct rt9466_info *info, u8 cmd,
	u8 data)
{
	int ret = 0, retry = 0;

	do {
#ifdef CONFIG_RT_REGMAP
		ret = rt_regmap_block_write(info->regmap_dev, cmd, 1, &data);
#else
		ret = rt9466_device_write(info->client, cmd, 1, &data);
#endif
		retry++;
		if (ret < 0)
			udelay(10);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);

	if (ret < 0)
		rt_debug("%s: I2CW[0x%02X] = 0x%02X fail\n",
			__func__, cmd, data);
	#if 0
	else
		rt_debug("%s: I2CW[0x%02X] = 0x%02X\n", __func__,
			cmd, data);
	#endif

	return ret;
}

static int rt9466_i2c_write_byte(struct rt9466_info *info, u8 cmd, u8 data)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9466_i2c_write_byte(info, cmd, data);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static inline int __rt9466_i2c_read_byte(struct rt9466_info *info, u8 cmd)
{
	int ret = 0, ret_val = 0, retry = 0;

	do {
#ifdef CONFIG_RT_REGMAP
		ret = rt_regmap_block_read(info->regmap_dev, cmd, 1, &ret_val);
#else
		ret = rt9466_device_read(info->client, cmd, 1, &ret_val);
#endif
		retry++;
		if (ret < 0)
			udelay(10);
	} while (ret < 0 && retry < I2C_ACCESS_MAX_RETRY);

	if (ret < 0) {
		rt_debug("%s: I2CR[0x%02X] fail\n", __func__, cmd);
		return ret;
	}

	ret_val = ret_val & 0xFF;

	#if 0
	rt_debug("%s: I2CR[0x%02X] = 0x%02X\n", __func__, cmd,
		ret_val);
	#endif

	return ret_val;
}

static int rt9466_i2c_read_byte(struct rt9466_info *info, u8 cmd)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9466_i2c_read_byte(info, cmd);
	mutex_unlock(&info->i2c_access_lock);

	if (ret < 0)
		return ret;

	return (ret & 0xFF);
}

static inline int __rt9466_i2c_block_write(struct rt9466_info *info, u8 cmd,
	u32 leng, const u8 *data)
{
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_write(info->regmap_dev, cmd, leng, data);
#else
	ret = rt9466_device_write(info->client, cmd, leng, data);
#endif

	return ret;
}


static int rt9466_i2c_block_write(struct rt9466_info *info, u8 cmd, u32 leng,
	const u8 *data)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9466_i2c_block_write(info, cmd, leng, data);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static inline int __rt9466_i2c_block_read(struct rt9466_info *info, u8 cmd,
	u32 leng, u8 *data)
{
	int ret = 0;

#ifdef CONFIG_RT_REGMAP
	ret = rt_regmap_block_read(info->regmap_dev, cmd, leng, data);
#else
	ret = rt9466_device_read(info->client, cmd, leng, data);
#endif

	return ret;
}


static int rt9466_i2c_block_read(struct rt9466_info *info, u8 cmd, u32 leng,
	u8 *data)
{
	int ret = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9466_i2c_block_read(info, cmd, leng, data);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}


static int rt9466_i2c_test_bit(struct rt9466_info *info, u8 cmd, u8 shift,
	bool *is_one)
{
	int ret = 0;
	u8 data = 0;

	ret = rt9466_i2c_read_byte(info, cmd);
	if (ret < 0) {
		*is_one = false;
		return ret;
	}

	data = ret & (1 << shift);
	*is_one = (data == 0 ? false : true);

	return ret;
}

static int rt9466_i2c_update_bits(struct rt9466_info *info, u8 cmd, u8 data,
	u8 mask)
{
	int ret = 0;
	u8 reg_data = 0;

	mutex_lock(&info->i2c_access_lock);
	ret = __rt9466_i2c_read_byte(info, cmd);
	if (ret < 0) {
		mutex_unlock(&info->i2c_access_lock);
		return ret;
	}

	reg_data = ret & 0xFF;
	reg_data &= ~mask;
	reg_data |= (data & mask);

	ret = __rt9466_i2c_write_byte(info, cmd, reg_data);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static inline int rt9466_set_bit(struct rt9466_info *info, u8 reg, u8 mask)
{
	return rt9466_i2c_update_bits(info, reg, mask, mask);
}

static inline int rt9466_clr_bit(struct rt9466_info *info, u8 reg, u8 mask)
{
	return rt9466_i2c_update_bits(info, reg, 0x00, mask);
}

/* ================== */
/* Internal Functions */
/* ================== */
static int rt9466_get_mivr(struct rt9466_info *info, u32 *mivr);
static int rt9466_get_aicr(struct charger_device *chg_dev, u32 *aicr);
static int rt9466_get_ichg(struct charger_device *chg_dev, u32 *ichg);
static int rt9466_set_aicr(struct charger_device *chg_dev, u32 aicr);
static int rt9466_set_ichg(struct charger_device *chg_dev, u32 aicr);
static int rt9466_kick_wdt(struct charger_device *chg_dev);
static int rt9466_enable_charging(struct charger_device *chg_dev, bool en);
static int rt9466_get_ieoc(struct rt9466_info *info, u32 *ieoc);

static inline void rt9466_irq_set_flag(struct rt9466_info *info, u8 *irq,
	u8 mask)
{
	mutex_lock(&info->irq_access_lock);
	*irq |= mask;
	mutex_unlock(&info->irq_access_lock);
}

static inline void rt9466_irq_clr_flag(struct rt9466_info *info, u8 *irq,
	u8 mask)
{
	mutex_lock(&info->irq_access_lock);
	*irq &= ~mask;
	mutex_unlock(&info->irq_access_lock);
}

static inline const char *rt9466_get_irq_name(struct rt9466_info *info,
	int irqnum)
{
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(rt9466_irq_mapping_tbl); i++) {
		if (rt9466_irq_mapping_tbl[i].id == irqnum)
			return rt9466_irq_mapping_tbl[i].name;
	}

	return "not found";
}

static inline void rt9466_irq_mask(struct rt9466_info *info, int irqnum)
{
	rt_debug("%s: irq = %d, %s\n", __func__, irqnum,
		rt9466_get_irq_name(info, irqnum));
	info->irq_mask[irqnum / 8] |= (1 << (irqnum % 8));
}

static inline void rt9466_irq_unmask(struct rt9466_info *info, int irqnum)
{
	rt_debug("%s: irq = %d, %s\n", __func__, irqnum,
		rt9466_get_irq_name(info, irqnum));
	info->irq_mask[irqnum / 8] &= ~(1 << (irqnum % 8));
}

static inline u8 rt9466_closest_reg(u32 min, u32 max, u32 step, u32 target)
{
	/* Smaller than minimum supported value, use minimum one */
	if (target < min)
		return 0;

	/* Greater than maximum supported value, use maximum one */
	if (target >= max)
		return (max - min) / step;

	return (target - min) / step;
}

static inline u8 rt9466_closest_reg_via_tbl(const u32 *tbl, u32 tbl_size,
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

static inline u32 rt9466_closest_value(u32 min, u32 max, u32 step, u8 reg_val)
{
	u32 ret_val = 0;

	ret_val = min + reg_val * step;
	if (ret_val > max)
		ret_val = max;

	return ret_val;
}

static int rt9466_get_adc(struct rt9466_info *info,
	enum rt9466_adc_sel adc_sel, int *adc_val)
{
	int ret = 0, i = 0;
	const int max_wait_times = 6;
	u8 adc_data[2] = {0, 0};
	u32 aicr = 0, ichg = 0;
	bool adc_start = false;

	mutex_lock(&info->adc_access_lock);

	/* Select ADC to desired channel */
	ret = rt9466_i2c_update_bits(info, RT9466_REG_CHG_ADC,
		adc_sel << RT9466_SHIFT_ADC_IN_SEL, RT9466_MASK_ADC_IN_SEL);
	if (ret < 0) {
		rt_debug("%s: select ch to %d fail(%d)\n", __func__,
			adc_sel, ret);
		goto out;
	}

	/* Workaround for IBUS & IBAT */
	if (adc_sel == RT9466_ADC_IBUS) {
		mutex_lock(&info->aicr_access_lock);
		ret = rt9466_get_aicr(info->chg_dev, &aicr);
		if (ret < 0) {
			rt_debug("%s: get aicr fail\n", __func__);
			goto out_unlock_all;
		}
	} else if (adc_sel == RT9466_ADC_IBAT) {
		mutex_lock(&info->ichg_access_lock);
		ret = rt9466_get_ichg(info->chg_dev, &ichg);
		if (ret < 0) {
			rt_debug("%s: get ichg fail\n", __func__);
			goto out_unlock_all;
		}
	}

	/* Start ADC conversation */
	ret = rt9466_set_bit(info, RT9466_REG_CHG_ADC, RT9466_MASK_ADC_START);
	if (ret < 0) {
		rt_debug("%s: start con fail(%d), sel = %d\n",
			__func__, ret, adc_sel);
		goto out_unlock_all;
	}

	for (i = 0; i < max_wait_times; i++) {
		msleep(35);
		ret = rt9466_i2c_test_bit(info, RT9466_REG_CHG_ADC,
			RT9466_SHIFT_ADC_START, &adc_start);
		if (ret >= 0 && !adc_start)
			break;
	}
	if (i == max_wait_times) {
		rt_debug("%s: wait con fail(%d), sel = %d\n",
			__func__, ret, adc_sel);
		ret = -EINVAL;
		goto out_unlock_all;
	}

	mdelay(1);

	/* Read ADC data high/low byte */
	ret = rt9466_i2c_block_read(info, RT9466_REG_ADC_DATA_H, 2, adc_data);
	if (ret < 0) {
		rt_debug("%s: read ADC data fail\n", __func__);
		goto out_unlock_all;
	}

	/* Calculate ADC value */
	*adc_val = ((adc_data[0] << 8) + adc_data[1]) * rt9466_adc_unit[adc_sel]
		+ rt9466_adc_offset[adc_sel];

	rt_debug("%s: adc_sel = %d, adc_h = 0x%02X, adc_l = 0x%02X, val = %d\n",
		__func__, adc_sel, adc_data[0], adc_data[1], *adc_val);

	ret = 0;

out_unlock_all:
	/* Coefficient of IBUS & IBAT */
	if (adc_sel == RT9466_ADC_IBUS) {
		if (aicr < 400000) /* 400mA */
			*adc_val = *adc_val * 67 / 100;
		mutex_unlock(&info->aicr_access_lock);
	} else if (adc_sel == RT9466_ADC_IBAT) {
		if (ichg >= 100000 && ichg <= 450000) /* 100~450mA */
			*adc_val = *adc_val * 57 / 100;
		else if (ichg >= 500000 && ichg <= 850000) /* 500~850mA */
			*adc_val = *adc_val * 63 / 100;
		mutex_unlock(&info->ichg_access_lock);
	}

out:
	mutex_unlock(&info->adc_access_lock);
	return ret;
}

static inline int rt9466_enable_ilim(struct rt9466_info *info, bool en)
{

	rt_info("%s: en = %d\n", __func__, en);

	return (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL3, RT9466_MASK_ILIM_EN);
}


static int rt9466_set_aicl_vth(struct rt9466_info *info, u32 aicl_vth)
{
	u8 reg_aicl_vth = 0;

	reg_aicl_vth = rt9466_closest_reg(RT9466_AICL_VTH_MIN,
		RT9466_AICL_VTH_MAX, RT9466_AICL_VTH_STEP, aicl_vth);

	rt_info("%s: vth = %d(0x%02X)\n", __func__, aicl_vth,
		reg_aicl_vth);

	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL14,
		reg_aicl_vth << RT9466_SHIFT_AICL_VTH, RT9466_MASK_AICL_VTH);
}

static int __rt9466_set_aicr(struct rt9466_info *info, u32 aicr)
{
	u8 reg_aicr = 0;

	reg_aicr = rt9466_closest_reg(RT9466_AICR_MIN, RT9466_AICR_MAX,
		RT9466_AICR_STEP, aicr);

	rt_info("%s: aicr = %d(0x%02X)\n", __func__, aicr,
		reg_aicr);

	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL3,
		reg_aicr << RT9466_SHIFT_AICR, RT9466_MASK_AICR);
}

static int __rt9466_run_aicl(struct rt9466_info *info)
{
	int ret = 0;
	u32 mivr = 0, aicl_vth = 0, aicr = 0;
	bool mivr_act = false;

	/* Check whether MIVR loop is active */
	ret = rt9466_i2c_test_bit(info, RT9466_REG_CHG_STATC,
		RT9466_SHIFT_CHG_MIVR, &mivr_act);
	if (ret < 0) {
		rt_debug("%s: read mivr stat fail\n", __func__);
		goto out;
	}

	if (!mivr_act) {
		rt_info("%s: mivr loop is not active\n", __func__);
		goto out;
	}

	ret = rt9466_get_mivr(info, &mivr);
	if (ret < 0)
		goto out;

	/* Check if there's a suitable AICL_VTH */
	aicl_vth = mivr + 200000;
	if (aicl_vth > RT9466_AICL_VTH_MAX) {
		rt_debug("%s: no suitable vth, vth = %d\n", __func__,
			aicl_vth);
		ret = -EINVAL;
		goto out;
	}

	ret = rt9466_set_aicl_vth(info, aicl_vth);
	if (ret < 0)
		goto out;

	/* Clear AICL measurement IRQ */
	rt9466_irq_clr_flag(info, &info->irq_flag[RT9466_IRQIDX_CHG_IRQ2],
		RT9466_MASK_CHG_AICLMEASI);

	mutex_lock(&info->pe_access_lock);
	mutex_lock(&info->aicr_access_lock);

	ret = rt9466_set_bit(info, RT9466_REG_CHG_CTRL14,
		RT9466_MASK_AICL_MEAS);
	if (ret < 0)
		goto unlock_out;

	ret = wait_event_interruptible_timeout(info->wait_queue,
		info->irq_flag[RT9466_IRQIDX_CHG_IRQ2]
		& RT9466_MASK_CHG_AICLMEASI,
		msecs_to_jiffies(3500));
	if (ret <= 0) {
		rt_debug("%s: wait AICL time out\n", __func__);
		ret = -EIO;
		goto unlock_out;
	}

	ret = rt9466_get_aicr(info->chg_dev, &aicr);
	if (ret < 0)
		goto unlock_out;

	info->aicr_limit = aicr;
	rt_debug("%s: OK, aicr upper bound = %dmA\n", __func__,
		aicr / 1000);

unlock_out:
	mutex_unlock(&info->aicr_access_lock);
	mutex_unlock(&info->pe_access_lock);
out:
	return ret;
}

/* Prevent back boost */
static int rt9466_toggle_cfo(struct rt9466_info *info)
{
	int ret = 0;
	u8 data = 0;

	rt_info("%s\n", __func__);
	mutex_lock(&info->i2c_access_lock);
	ret = rt9466_device_read(info->client, RT9466_REG_CHG_CTRL2, 1, &data);
	if (ret < 0) {
		rt_debug("%s read cfo fail(%d)\n", __func__, ret);
		goto out;
	}

	/* CFO off */
	data &= ~RT9466_MASK_CFO_EN;
	ret = rt9466_device_write(info->client, RT9466_REG_CHG_CTRL2, 1, &data);
	if (ret < 0) {
		rt_debug("%s cfo off fail(%d)\n", __func__, ret);
		goto out;
	}

	/* CFO on */
	data |= RT9466_MASK_CFO_EN;
	ret = rt9466_device_write(info->client, RT9466_REG_CHG_CTRL2, 1, &data);
	if (ret < 0)
		rt_debug("%s cfo on fail(%d)\n", __func__, ret);

out:
	mutex_unlock(&info->i2c_access_lock);
	return ret;
}

/* IRQ handlers */
static int rt9466_pwr_rdy_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_mivr_irq_handler(struct rt9466_info *info)
{
	int ret = 0;
	bool mivr_act = false;
	int adc_ibus = 0;

	rt_debug("%s\n", __func__);

	/* Check whether MIVR loop is active */
	ret = rt9466_i2c_test_bit(info, RT9466_REG_CHG_STATC,
		RT9466_SHIFT_CHG_MIVR, &mivr_act);
	if (ret < 0) {
		rt_debug("%s: read mivr stat failed\n", __func__);
		goto out;
	}

	if (!mivr_act) {
		rt_info("%s: mivr loop is not active\n", __func__);
		goto out;
	}

	if (strcmp(info->desc->chg_dev_name, "primary_chg") == 0) {
		/* Check IBUS ADC */
		ret = rt9466_get_adc(info, RT9466_ADC_IBUS, &adc_ibus);
		if (ret < 0) {
			rt_debug("%s: get ibus fail\n", __func__);
			return ret;
		}
		if (adc_ibus < 100000) { /* 100mA */
			ret = rt9466_toggle_cfo(info);
			return ret;
		}
	}
out:
	return 0;
}

static int rt9466_chg_aicr_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_treg_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_vsysuv_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_vsysov_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_vbatov_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_vbusov_irq_handler(struct rt9466_info *info)
{
	int ret = 0;
	bool vbusov = false;
	struct chgdev_notify *noti = &(info->chg_dev->noti);

	rt_debug("%s\n", __func__);
	ret = rt9466_i2c_test_bit(info, RT9466_REG_CHG_FAULT,
		RT9466_SHIFT_VBUSOV, &vbusov);
	if (ret < 0)
		return ret;

	noti->vbusov_stat = vbusov;
	rt_info("%s: vbusov = %d\n", __func__, vbusov);
	charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_VBUS_OVP);

	return 0;
}

static int rt9466_ts_bat_cold_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_ts_bat_cool_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_ts_bat_warm_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_ts_bat_hot_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_ts_statci_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_faulti_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_statci_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_tmri_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_SAFETY_TIMEOUT);
	return 0;
}

static int rt9466_chg_batabsi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_adpbadi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_rvpi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_otpi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_aiclmeasi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	rt9466_irq_set_flag(info, &info->irq_flag[RT9466_IRQIDX_CHG_IRQ2],
		RT9466_MASK_CHG_AICLMEASI);
	wake_up_interruptible(&info->wait_queue);
	return 0;
}

static int rt9466_chg_ichgmeasi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_wdtmri_irq_handler(struct rt9466_info *info)
{
	int ret = 0;

	rt_debug("%s\n", __func__);
	ret = rt9466_kick_wdt(info->chg_dev);
	if (ret < 0)
		rt_debug("%s: kick wdt fail\n", __func__);

	return ret;
}

static int rt9466_ssfinishi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_rechgi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_RECHG);
	return 0;
}

static int rt9466_chg_termi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_chg_ieoci_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	charger_dev_notify(info->chg_dev, CHARGER_DEV_NOTIFY_EOC);
	return 0;
}

static int rt9466_adc_donei_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_pumpx_donei_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_bst_batuvi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_bst_midovi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

static int rt9466_bst_olpi_irq_handler(struct rt9466_info *info)
{
	rt_debug("%s\n", __func__);
	return 0;
}

typedef int (*rt9466_irq_fptr)(struct rt9466_info *);
static rt9466_irq_fptr rt9466_irq_handler_tbl[48] = {
	NULL,
	NULL,
	NULL,
	NULL,
	rt9466_chg_treg_irq_handler,
	rt9466_chg_aicr_irq_handler,
	rt9466_chg_mivr_irq_handler,
	rt9466_pwr_rdy_irq_handler,
	NULL,
	NULL,
	NULL,
	NULL,
	rt9466_chg_vsysuv_irq_handler,
	rt9466_chg_vsysov_irq_handler,
	rt9466_chg_vbatov_irq_handler,
	rt9466_chg_vbusov_irq_handler,
	NULL,
	NULL,
	NULL,
	NULL,
	rt9466_ts_bat_cold_irq_handler,
	rt9466_ts_bat_cool_irq_handler,
	rt9466_ts_bat_warm_irq_handler,
	rt9466_ts_bat_hot_irq_handler,
	rt9466_ts_statci_irq_handler,
	rt9466_chg_faulti_irq_handler,
	rt9466_chg_statci_irq_handler,
	rt9466_chg_tmri_irq_handler,
	rt9466_chg_batabsi_irq_handler,
	rt9466_chg_adpbadi_irq_handler,
	rt9466_chg_rvpi_irq_handler,
	rt9466_chg_otpi_irq_handler,
	rt9466_chg_aiclmeasi_irq_handler,
	rt9466_chg_ichgmeasi_irq_handler,
	NULL,
	rt9466_wdtmri_irq_handler,
	rt9466_ssfinishi_irq_handler,
	rt9466_chg_rechgi_irq_handler,
	rt9466_chg_termi_irq_handler,
	rt9466_chg_ieoci_irq_handler,
	rt9466_adc_donei_irq_handler,
	rt9466_pumpx_donei_irq_handler,
	NULL,
	NULL,
	NULL,
	rt9466_bst_batuvi_irq_handler,
	rt9466_bst_midovi_irq_handler,
	rt9466_bst_olpi_irq_handler,
};

static inline int rt9466_enable_irqrez(struct rt9466_info *info, bool en)
{
	rt_info("%s: en = %d\n", __func__, en);
	return (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL13, RT9466_MASK_IRQ_REZ);
}

static int __rt9466_irq_handler(struct rt9466_info *info)
{
	int ret = 0, i = 0, j = 0;
	u8 evt[RT9466_IRQIDX_MAX] = {0};
	u8 mask[RT9466_IRQIDX_MAX] = {0};
	u8 stat[RT9466_IRQSTAT_MAX] = {0};

	rt_info("%s\n", __func__);

	/* Read event and skip CHG_IRQ3 */
	ret = rt9466_i2c_block_read(info, RT9466_REG_CHG_IRQ1, 2, &evt[3]);
	if (ret < 0) {
		rt_debug("%s: read evt fail(%d)\n", __func__, ret);
		goto err_read_irq;
	}

	ret = rt9466_i2c_block_read(info, RT9466_REG_CHG_STATC, 3, evt);
	if (ret < 0) {
		rt_debug("%s: read stat fail(%d)\n", __func__, ret);
		goto err_read_irq;
	}

	/* Read mask */
	ret = rt9466_i2c_block_read(info, RT9466_REG_CHG_STATC_CTRL,
		ARRAY_SIZE(mask), mask);
	if (ret < 0) {
		rt_debug("%s: read mask fail(%d)\n", __func__, ret);
		goto err_read_irq;
	}

	/* Store/Update stat */
	memcpy(stat, info->irq_stat, RT9466_IRQSTAT_MAX);

	for (i = 0; i < RT9466_IRQIDX_MAX; i++) {
		evt[i] &= ~mask[i];
		if (i <= RT9466_IRQIDX_TS_STATC) {
			info->irq_stat[i] = evt[i];
			evt[i] ^= stat[i];
		}
		for (j = 0; j < 8; j++) {
			if (!(evt[i] & (1 << j)))
				continue;
			if (rt9466_irq_handler_tbl[i * 8 + j])
				rt9466_irq_handler_tbl[i * 8 + j](info);
		}
	}

err_read_irq:
	return ret;
}

static irqreturn_t rt9466_irq_handler(int irq, void *data)
{
	int ret = 0;
	struct rt9466_info *info = (struct rt9466_info *)data;

	rt_info("%s\n", __func__);

	ret = __rt9466_irq_handler(info);
	ret = rt9466_enable_irqrez(info, true);
	if (ret < 0)
		rt_debug("%s: en irqrez fail\n", __func__);

	return IRQ_HANDLED;
}

static int rt9466_irq_register(struct rt9466_info *info)
{
	int ret = 0, len = 0;
	char *name = NULL;

	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0)
		return 0;

	rt_info("%s\n", __func__);

	/* request gpio */
	len = strlen(info->desc->chg_dev_name);
	name = devm_kzalloc(info->dev, len + 10, GFP_KERNEL);
	snprintf(name,  len + 10, "%s_irq_gpio", info->desc->chg_dev_name);

	rt_info("%s: irq = %d\n", __func__, info->irq);

	/* Request threaded IRQ */
	name = devm_kzalloc(info->dev, len + 5, GFP_KERNEL);
	snprintf(name, len + 5, "%s_irq", info->desc->chg_dev_name);
	ret = devm_request_threaded_irq(info->dev, info->irq, NULL,
		rt9466_irq_handler, IRQF_TRIGGER_FALLING | IRQF_ONESHOT, name,
		info);
	if (ret < 0) {
		rt_debug("%s: request thread irq fail\n", __func__);
		return ret;
	}
	device_init_wakeup(info->dev, true);

	return 0;
}

static inline int rt9466_maskall_irq(struct rt9466_info *info)
{
	rt_info("%s\n", __func__);
	return rt9466_i2c_block_write(info, RT9466_REG_CHG_STATC_CTRL,
		ARRAY_SIZE(rt9466_irq_maskall), rt9466_irq_maskall);
}

static inline int rt9466_irq_init(struct rt9466_info *info)
{
	rt_info("%s\n", __func__);
	return rt9466_i2c_block_write(info, RT9466_REG_CHG_STATC_CTRL,
		ARRAY_SIZE(info->irq_mask), info->irq_mask);
}

static bool rt9466_is_hw_exist(struct rt9466_info *info)
{
	int ret = 0;
	u8 vendor_id = 0, chip_rev = 0;

	ret = i2c_smbus_read_byte_data(info->client, RT9466_REG_DEVICE_ID);
	if (ret < 0)
		return false;

	vendor_id = ret & 0xF0;
	chip_rev = ret & 0x0F;
	if (vendor_id != RT9466_VENDOR_ID) {
		rt_debug("%s: vendor id is incorrect (0x%02X)\n",
			__func__, vendor_id);
		return false;
	}

	rt_info("%s: 0x%02X\n", __func__, chip_rev);
	info->chip_rev = chip_rev;

	return true;
}

static int rt9466_set_safety_timer(struct rt9466_info *info, u32 hr)
{
	u8 reg_st = 0;

	reg_st = rt9466_closest_reg_via_tbl(rt9466_safety_timer,
		ARRAY_SIZE(rt9466_safety_timer), hr);

	rt_info("%s: time = %d(0x%02X)\n", __func__, hr, reg_st);

	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL12,
		reg_st << RT9466_SHIFT_WT_FC, RT9466_MASK_WT_FC);
}

static int rt9466_enable_wdt(struct rt9466_info *info, bool en)
{
	rt_info("%s: en = %d\n", __func__, en);
	return (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL13, RT9466_MASK_WDT_EN);
}

static int rt9466_select_input_current_limit(struct rt9466_info *info,
	enum rt9466_iin_limit_sel sel)
{
	rt_info("%s: sel = %d\n", __func__, sel);
	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL2,
		sel << RT9466_SHIFT_IINLMTSEL, RT9466_MASK_IINLMTSEL);
}

static int rt9466_enable_hidden_mode(struct rt9466_info *info, bool en)
{
	int ret = 0;

	mutex_lock(&info->hidden_mode_lock);

	if (en) {
		if (info->hidden_mode_cnt == 0) {
			ret = rt9466_i2c_block_write(info, 0x70,
				ARRAY_SIZE(rt9466_val_en_hidden_mode),
				rt9466_val_en_hidden_mode);
			if (ret < 0)
				goto err;
		}
		info->hidden_mode_cnt++;
	} else {
		if (info->hidden_mode_cnt == 1) /* last one */
			ret = rt9466_i2c_write_byte(info, 0x70, 0x00);
		info->hidden_mode_cnt--;
		if (ret < 0)
			goto err;
	}
	rt_debug("%s: en = %d\n", __func__, en);
	goto out;

err:
	rt_debug("%s: en = %d fail(%d)\n", __func__, en, ret);
out:
	mutex_unlock(&info->hidden_mode_lock);
	return ret;
}

static int rt9466_set_iprec(struct rt9466_info *info, u32 iprec)
{
	u8 reg_iprec = 0;

	reg_iprec = rt9466_closest_reg(RT9466_IPREC_MIN, RT9466_IPREC_MAX,
		RT9466_IPREC_STEP, iprec);

	rt_info("%s: iprec = %d(0x%02X)\n", __func__, iprec,
		reg_iprec);

	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL8,
		reg_iprec << RT9466_SHIFT_IPREC, RT9466_MASK_IPREC);
}

static int rt9466_sw_workaround(struct rt9466_info *info)
{
	int ret = 0;

	rt_info("%s\n", __func__);

	rt9466_enable_hidden_mode(info, true);

	/* Disable TS auto sensing */
	ret = rt9466_clr_bit(info, RT9466_REG_CHG_HIDDEN_CTRL15, 0x01);
	if (ret < 0)
		goto out;

	/* Set precharge current to 850mA, only do this in normal boot */
	if (info->chip_rev <= RT9466_CHIP_REV_E3) {
		/* Worst case delay: wait auto sensing */
		msleep(200);
// workaround for mt6768
//		if (get_boot_mode() == NORMAL_BOOT) {
//			ret = rt9466_set_iprec(info, 850000);
//			if (ret < 0)
//				goto out;
//
//			/* Increase Isys drop threshold to 2.5A */
//			ret = rt9466_i2c_write_byte(info,
//				RT9466_REG_CHG_HIDDEN_CTRL7, 0x1c);
//			if (ret < 0)
//				goto out;
//		}
	}

	/* Only revision <= E1 needs the following workaround */
	if (info->chip_rev > RT9466_CHIP_REV_E1)
		goto out;

	/* ICC: modify sensing node, make it more accurate */
	ret = rt9466_i2c_write_byte(info, RT9466_REG_CHG_HIDDEN_CTRL8, 0x00);
	if (ret < 0)
		goto out;

	/* DIMIN level */
	ret = rt9466_i2c_write_byte(info, RT9466_REG_CHG_HIDDEN_CTRL9, 0x86);

out:
	rt9466_enable_hidden_mode(info, false);
	return ret;
}

static int rt9466_enable_hz(struct rt9466_info *info, bool en)
{
	rt_info("%s: en = %d\n", __func__, en);
	return (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL1, RT9466_MASK_HZ_EN);
}

/* Reset all registers' value to default */
static int rt9466_reset_chip(struct rt9466_info *info)
{
	int ret = 0;

	rt_info("%s\n", __func__);

	/* disable hz before reset chip */
	ret = rt9466_enable_hz(info, false);
	if (ret < 0) {
		rt_debug("%s: disable hz fail\n", __func__);
		return ret;
	}

	return rt9466_set_bit(info, RT9466_REG_CORE_CTRL0, RT9466_MASK_RST);
}

static inline int __rt9466_enable_te(struct rt9466_info *info, bool en)
{
	rt_info("%s: en = %d\n", __func__, en);
	return (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL2, RT9466_MASK_TE_EN);
}

static inline int __rt9466_enable_safety_timer(struct rt9466_info *info,
	bool en)
{
	rt_info("%s: en = %d\n", __func__, en);
	return (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL12, RT9466_MASK_TMR_EN);
}

static int __rt9466_set_ieoc(struct rt9466_info *info, u32 ieoc)
{
	int ret = 0;
	u8 reg_ieoc = 0;

	/* IEOC workaround */
	if (info->ieoc_wkard)
		ieoc += 100000; /* 100mA */

	reg_ieoc = rt9466_closest_reg(RT9466_IEOC_MIN, RT9466_IEOC_MAX,
		RT9466_IEOC_STEP, ieoc);

	rt_info("%s: ieoc = %d(0x%02X)\n", __func__, ieoc,
		reg_ieoc);

	ret = rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL9,
		reg_ieoc << RT9466_SHIFT_IEOC, RT9466_MASK_IEOC);

	/* Store IEOC */
	return rt9466_get_ieoc(info, &info->ieoc);
}

static int rt9466_get_mivr(struct rt9466_info *info, u32 *mivr)
{
	int ret = 0;
	u8 reg_mivr = 0;

	ret = rt9466_i2c_read_byte(info, RT9466_REG_CHG_CTRL6);
	if (ret < 0)
		return ret;
	reg_mivr = ((ret & RT9466_MASK_MIVR) >> RT9466_SHIFT_MIVR) & 0xFF;

	*mivr = rt9466_closest_value(RT9466_MIVR_MIN, RT9466_MIVR_MAX,
		RT9466_MIVR_STEP, reg_mivr);

	return ret;
}

static int __rt9466_set_mivr(struct rt9466_info *info, u32 mivr)
{
	u8 reg_mivr = 0;

	reg_mivr = rt9466_closest_reg(RT9466_MIVR_MIN, RT9466_MIVR_MAX,
		RT9466_MIVR_STEP, mivr);

	rt_info("%s: mivr = %d(0x%02X)\n", __func__, mivr,
		reg_mivr);

	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL6,
		reg_mivr << RT9466_SHIFT_MIVR, RT9466_MASK_MIVR);
}

static int rt9466_enable_jeita(struct rt9466_info *info, bool en)
{
	rt_info("%s: en = %d\n", __func__, en);
	return (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL16, RT9466_MASK_JEITA_EN);
}


static int rt9466_get_charging_status(struct rt9466_info *info,
	enum rt9466_charging_status *chg_stat)
{
	int ret = 0;

	ret = rt9466_i2c_read_byte(info, RT9466_REG_CHG_STAT);
	if (ret < 0)
		return ret;

	*chg_stat = (ret & RT9466_MASK_CHG_STAT) >> RT9466_SHIFT_CHG_STAT;

	return ret;
}

static int rt9466_get_ieoc(struct rt9466_info *info, u32 *ieoc)
{
	int ret = 0;
	u8 reg_ieoc = 0;

	ret = rt9466_i2c_read_byte(info, RT9466_REG_CHG_CTRL9);
	if (ret < 0)
		return ret;

	reg_ieoc = (ret & RT9466_MASK_IEOC) >> RT9466_SHIFT_IEOC;
	*ieoc = rt9466_closest_value(RT9466_IEOC_MIN, RT9466_IEOC_MAX,
		RT9466_IEOC_STEP, reg_ieoc);

	return ret;
}

static inline int __rt9466_is_charging_enable(struct rt9466_info *info,
	bool *en)
{
	return rt9466_i2c_test_bit(info, RT9466_REG_CHG_CTRL2,
		RT9466_SHIFT_CHG_EN, en);
}

static int __rt9466_set_ichg(struct rt9466_info *info, u32 ichg)
{
	int ret = 0;
	u8 reg_ichg = 0;

	/* Workaround to keep ichg >= 900mA */
	if (strcmp(info->desc->chg_dev_name, "primary_chg") == 0)
		ichg = (ichg < 900000) ? 900000 : ichg;

	reg_ichg = rt9466_closest_reg(RT9466_ICHG_MIN, RT9466_ICHG_MAX,
		RT9466_ICHG_STEP, ichg);

	rt_info("%s: ichg = %d(0x%02X)\n", __func__, ichg,
		reg_ichg);

	ret = rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL7,
		reg_ichg << RT9466_SHIFT_ICHG, RT9466_MASK_ICHG);

	/* Workaround to make IEOC accurate */
	if (ichg < 900000 && !info->ieoc_wkard) { /* 900mA */
		ret = __rt9466_set_ieoc(info, info->ieoc + 100000);
		info->ieoc_wkard = true;
	} else if (ichg >= 900000 && info->ieoc_wkard) {
		info->ieoc_wkard = false;
		ret = __rt9466_set_ieoc(info, info->ieoc - 100000);
	}

	return ret;
}

static int __rt9466_set_cv(struct rt9466_info *info, u32 cv)
{
	u8 reg_cv = 0;

	reg_cv = rt9466_closest_reg(RT9466_CV_MIN, RT9466_CV_MAX,
		RT9466_CV_STEP, cv);

	rt_info("%s: cv = %d(0x%02X)\n", __func__, cv, reg_cv);

	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL4,
		reg_cv << RT9466_SHIFT_CV, RT9466_MASK_CV);
}

static int rt9466_set_ircmp_resistor(struct rt9466_info *info, u32 uohm)
{
	u8 reg_resistor = 0;

	reg_resistor = rt9466_closest_reg(RT9466_IRCMP_RES_MIN,
		RT9466_IRCMP_RES_MAX, RT9466_IRCMP_RES_STEP, uohm);

	rt_info("%s: resistor = %d(0x%02X)\n", __func__, uohm,
		reg_resistor);

	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL18,
		reg_resistor << RT9466_SHIFT_IRCMP_RES, RT9466_MASK_IRCMP_RES);
}

static int rt9466_set_ircmp_vclamp(struct rt9466_info *info, u32 uV)
{
	u8 reg_vclamp = 0;

	reg_vclamp = rt9466_closest_reg(RT9466_IRCMP_VCLAMP_MIN,
		RT9466_IRCMP_VCLAMP_MAX, RT9466_IRCMP_VCLAMP_STEP, uV);

	rt_info("%s: vclamp = %d(0x%02X)\n", __func__, uV,
		reg_vclamp);

	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL18,
		reg_vclamp << RT9466_SHIFT_IRCMP_VCLAMP,
		RT9466_MASK_IRCMP_VCLAMP);
}

static int rt9466_enable_pump_express(struct rt9466_info *info, bool en)
{
	int ret = 0, i = 0;
	bool pumpx_en = false;
	const int max_wait_times = 3;

	rt_info("%s: en = %d\n", __func__, en);

	ret = rt9466_set_aicr(info->chg_dev, 800000);
	if (ret < 0)
		return ret;

	ret = rt9466_set_ichg(info->chg_dev, 2000000);
	if (ret < 0)
		return ret;

	ret = rt9466_enable_charging(info->chg_dev, true);
	if (ret < 0)
		return ret;

	rt9466_enable_hidden_mode(info, true);

	ret = rt9466_clr_bit(info, RT9466_REG_CHG_HIDDEN_CTRL9, 0x80);
	if (ret < 0)
		rt_debug("%s: disable skip mode fail\n", __func__);

	ret = (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL17, RT9466_MASK_PUMPX_EN);
	if (ret < 0)
		goto out;

	for (i = 0; i < max_wait_times; i++) {
		msleep(2500);
		ret = rt9466_i2c_test_bit(info, RT9466_REG_CHG_CTRL17,
			RT9466_SHIFT_PUMPX_EN, &pumpx_en);
		if (ret >= 0 && !pumpx_en)
			break;
	}
	if (i == max_wait_times) {
		rt_debug("%s: pumpx done fail(%d)\n", __func__, ret);
		ret = -EIO;
	} else
		ret = 0;

out:
	rt9466_set_bit(info, RT9466_REG_CHG_HIDDEN_CTRL9, 0x80);
	rt9466_enable_hidden_mode(info, false);
	return ret;
}

static int rt9466_enable_irq_pulse(struct rt9466_info *info, bool en)
{
	rt_info("%s: en = %d\n", __func__, en);
	return (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL1, RT9466_MASK_IRQ_PULSE);
}

static int rt9466_disable_charger_reset(struct rt9466_info *info, bool disable)
{
	rt_info("%s: en = %d\n", __func__, disable);
	return (disable ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL19, RT9466_MASK_RESET_DISABLE);
}

static inline int rt9466_get_irq_number(struct rt9466_info *info,
	const char *name)
{
	int i = 0;

	if (!name) {
		rt_debug("%s: null name\n", __func__);
		return -EINVAL;
	}

	for (i = 0; i < ARRAY_SIZE(rt9466_irq_mapping_tbl); i++) {
		if (!strcmp(name, rt9466_irq_mapping_tbl[i].name))
			return rt9466_irq_mapping_tbl[i].id;
	}

	return -EINVAL;
}

static int rt9466_parse_dt(struct rt9466_info *info, struct device *dev)
{
	int ret = 0, irq_cnt = 0;
	struct rt9466_desc *desc = NULL;
	struct device_node *np = dev->of_node;
	const char *name = NULL;
	int irqnum = 0;

	rt_info("%s\n", __func__);

	if (!np) {
		rt_debug("%s: no device node\n", __func__);
		return -EINVAL;
	}

	info->desc = &rt9466_default_desc;

	desc = devm_kzalloc(dev, sizeof(struct rt9466_desc), GFP_KERNEL);
	if (!desc)
		return -ENOMEM;
	memcpy(desc, &rt9466_default_desc, sizeof(struct rt9466_desc));

	if (of_property_read_string(np, "charger_name",
		&desc->chg_dev_name) < 0)
		rt_debug("%s: no charger name\n", __func__);

	info->irq = irq_of_parse_and_map(np, 0);

	/* request ceb gpio for secondary charger */
	if (strcmp(desc->chg_dev_name, "secondary_chg") == 0) {
		ret = devm_gpio_request_one(info->dev, info->ceb_gpio,
			GPIOF_DIR_OUT, "rt9466_sec_ceb_gpio");
		if (ret < 0) {
			rt_debug("%s: ceb gpio request fail\n",
				__func__);
			return ret;
		}
	}

	if (of_property_read_u32(np, "regmap_represent_slave_addr",
		&desc->regmap_represent_slave_addr) < 0)
		rt_debug("%s: no regmap slave addr\n", __func__);

	if (of_property_read_string(np, "regmap_name",
		&(desc->regmap_name)) < 0)
		rt_debug("%s: no regmap name\n", __func__);

	if (of_property_read_u32(np, "ichg", &desc->ichg) < 0)
		rt_debug("%s: no ichg\n", __func__);

	if (of_property_read_u32(np, "aicr", &desc->aicr) < 0)
		rt_debug("%s: no aicr\n", __func__);

	if (of_property_read_u32(np, "mivr", &desc->mivr) < 0)
		rt_debug("%s: no mivr\n", __func__);

	if (of_property_read_u32(np, "cv", &desc->cv) < 0)
		rt_debug("%s: no cv\n", __func__);

	if (of_property_read_u32(np, "ieoc", &desc->ieoc) < 0)
		rt_debug("%s: no ieoc\n", __func__);

	if (of_property_read_u32(np, "safety_timer", &desc->safety_timer) < 0)
		rt_debug("%s: no safety timer\n", __func__);

	if (of_property_read_u32(np, "ircmp_resistor",
		&desc->ircmp_resistor) < 0)
		rt_debug("%s: no ircmp resistor\n", __func__);

	if (of_property_read_u32(np, "ircmp_vclamp", &desc->ircmp_vclamp) < 0)
		rt_debug("%s: no ircmp vclamp\n", __func__);

	desc->en_te = of_property_read_bool(np, "en_te");
	desc->en_wdt = of_property_read_bool(np, "en_wdt");
	desc->en_irq_pulse = of_property_read_bool(np, "en_irq_pulse");
	desc->en_jeita = of_property_read_bool(np, "en_jeita");
	desc->ceb_invert = of_property_read_bool(np, "ceb_invert");

	while (true) {
		ret = of_property_read_string_index(np, "rt-interrupt-names",
			irq_cnt, &name);
		if (ret < 0)
			break;
		irq_cnt++;
		irqnum = rt9466_get_irq_number(info, name);
		if (irqnum >= 0)
			rt9466_irq_unmask(info, irqnum);
	}

	info->desc = desc;

	return 0;
}


/* =========================================================== */
/* Released interfaces                                         */
/* =========================================================== */

static int rt9466_enable_charging(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	rt_info("%s: en = %d\n", __func__, en);

	/* set hz/ceb pin for secondary charger */
	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0) {
		ret = rt9466_enable_hz(info, !en);
		if (ret < 0) {
			rt_debug("%s: set hz of sec chg fail\n",
				__func__);
			return ret;
		}
		if (info->desc->ceb_invert)
			gpio_set_value(info->ceb_gpio, en);
		else
			gpio_set_value(info->ceb_gpio, !en);
	}

	return (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL2, RT9466_MASK_CHG_EN);
}

static int rt9466_enable_safety_timer(struct charger_device *chg_dev, bool en)
{
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9466_enable_safety_timer(info, en);
}

static int rt9466_set_boost_current_limit(struct charger_device *chg_dev,
	u32 current_limit)
{
	u8 reg_ilimit = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	reg_ilimit = rt9466_closest_reg_via_tbl(rt9466_boost_oc_threshold,
		ARRAY_SIZE(rt9466_boost_oc_threshold), current_limit);

	rt_info("%s: boost ilimit = %d(0x%02X)\n", __func__,
		current_limit, reg_ilimit);

	return rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL10,
		reg_ilimit << RT9466_SHIFT_BOOST_OC, RT9466_MASK_BOOST_OC);
}

static int rt9466_enable_otg(struct charger_device *chg_dev, bool en)
{
	int ret = 0;
	bool en_otg = false;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);
	u8 hidden_val = en ? 0x00 : 0x0F;
	u8 lg_slew_rate = en ? 0x7c : 0x73;

	rt_info("%s: en = %d\n", __func__, en);

	rt9466_enable_hidden_mode(info, true);

	/* Set OTG_OC to 500mA */
	ret = rt9466_set_boost_current_limit(chg_dev, 2100000);
	if (ret < 0) {
		rt_debug("%s: set current limit fail\n", __func__);
		return ret;
	}

	/*
	 * Woraround : slow Low side mos Gate driver slew rate
	 * for decline VBUS noise
	 * reg[0x23] = 0x7c after entering OTG mode
	 * reg[0x23] = 0x73 after leaving OTG mode
	 */
	ret = rt9466_i2c_write_byte(info, RT9466_REG_CHG_HIDDEN_CTRL4,
		lg_slew_rate);
	if (ret < 0) {
		rt_debug("%s: set Low side mos Gate drive speed fail(%d)\n",
			__func__, ret);
		goto out;
	}

	/* Enable WDT */
	if (en && info->desc->en_wdt) {
		ret = rt9466_enable_wdt(info, true);
		if (ret < 0) {
			rt_debug("%s: en wdt fail\n", __func__);
			goto err_en_otg;
		}
	}

	/* Switch OPA mode */
	ret = (en ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL1, RT9466_MASK_OPA_MODE);

	msleep(20);

	if (en) {
		ret = rt9466_i2c_test_bit(info, RT9466_REG_CHG_CTRL1,
			RT9466_SHIFT_OPA_MODE, &en_otg);
		if (ret < 0 || !en_otg) {
			rt_debug("%s: otg fail(%d)\n", __func__,
				 ret);
			goto err_en_otg;
		}
	}

	/*
	 * Woraround reg[0x25] = 0x00 after entering OTG mode
	 * reg[0x25] = 0x0F after leaving OTG mode
	 */
	ret = rt9466_i2c_write_byte(info, RT9466_REG_CHG_HIDDEN_CTRL6,
		hidden_val);
	if (ret < 0)
		rt_debug("%s: workaroud fail(%d)\n", __func__, ret);

	/* Disable WDT */
	if (!en) {
		ret = rt9466_enable_wdt(info, false);
		if (ret < 0)
			rt_debug("%s: disable wdt fail\n", __func__);
	}
	goto out;

err_en_otg:
	/* Disable WDT */
	ret = rt9466_enable_wdt(info, false);
	if (ret < 0)
		rt_debug("%s: disable wdt fail\n", __func__);

	/* Recover Low side mos Gate slew rate */
	ret = rt9466_i2c_write_byte(info, RT9466_REG_CHG_HIDDEN_CTRL4, 0x73);
	if (ret < 0)
		rt_debug("%s: recover Low side mos Gate drive speed fail(%d)\n",
			__func__, ret);
	ret = -EIO;
out:
	rt9466_enable_hidden_mode(info, false);
	return ret;
}

static int rt9466_enable_discharge(struct charger_device *chg_dev, bool en)
{
	int ret = 0, i = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);
	const int check_dischg_max = 3;
	bool is_dischg = true;

	rt_info("%s: en = %d\n", __func__, en);

	ret = rt9466_enable_hidden_mode(info, true);
	if (ret < 0)
		return ret;

	/* Set bit2 of reg[0x21] to 1 to enable discharging */
	ret = (en ? rt9466_set_bit : rt9466_clr_bit)(info,
		RT9466_REG_CHG_HIDDEN_CTRL2, 0x04);
	if (ret < 0) {
		rt_debug("%s: en = %d, failed\n", __func__, en);
		return ret;
	}

	if (!en) {
		for (i = 0; i < check_dischg_max; i++) {
			ret = rt9466_i2c_test_bit(info,
				RT9466_REG_CHG_HIDDEN_CTRL2, 2, &is_dischg);
			if (ret >= 0 && !is_dischg)
				break;
			/* Disable discharging */
			ret = rt9466_clr_bit(info, RT9466_REG_CHG_HIDDEN_CTRL2,
				0x04);
		}
		if (i == check_dischg_max)
			rt_debug("%s: disable dischg fail(%d)\n",
				__func__, ret);
	}

	rt9466_enable_hidden_mode(info, false);
	return ret;
}

static int rt9466_enable_power_path(struct charger_device *chg_dev, bool en)
{
	u32 mivr = (en ? 4500000 : RT9466_MIVR_MAX);
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	rt_info("%s: en = %d\n", __func__, en);
	return __rt9466_set_mivr(info, mivr);
}

static int rt9466_is_power_path_enable(struct charger_device *chg_dev, bool *en)
{
	int ret = 0;
	u32 mivr = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9466_get_mivr(info, &mivr);
	if (ret < 0)
		return ret;

	*en = ((mivr == RT9466_MIVR_MAX) ? false : true);

	return ret;
}

static int rt9466_set_ichg(struct charger_device *chg_dev, u32 ichg)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->ichg_access_lock);
	mutex_lock(&info->ieoc_lock);
	ret = __rt9466_set_ichg(info, ichg);
	mutex_unlock(&info->ieoc_lock);
	mutex_unlock(&info->ichg_access_lock);

	return ret;
}

static int rt9466_set_ieoc(struct charger_device *chg_dev, u32 ieoc)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->ichg_access_lock);
	mutex_lock(&info->ieoc_lock);
	ret = __rt9466_set_ieoc(info, ieoc);
	mutex_unlock(&info->ieoc_lock);
	mutex_unlock(&info->ichg_access_lock);

	return ret;
}

static int rt9466_set_aicr(struct charger_device *chg_dev, u32 aicr)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->aicr_access_lock);
	ret = __rt9466_set_aicr(info, aicr);
	mutex_unlock(&info->aicr_access_lock);

	return ret;
}

static int rt9466_set_mivr(struct charger_device *chg_dev, u32 mivr)
{
	int ret = 0;
	bool en = true;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9466_is_power_path_enable(chg_dev, &en);
	if (ret < 0) {
		rt_debug("%s: get power path en fail\n", __func__);
		return ret;
	}

	if (!en) {
		rt_info("%s: power path is disabled, op is not allowed\n",
			__func__);
		return -EINVAL;
	}

	return __rt9466_set_mivr(info, mivr);
}

static int rt9466_set_cv(struct charger_device *chg_dev, u32 cv)
{
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9466_set_cv(info, cv);
}

static int rt9466_set_pep_current_pattern(struct charger_device *chg_dev,
	bool is_increase)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	rt_info("%s: pump_up = %d\n", __func__, is_increase);

	mutex_lock(&info->pe_access_lock);

	/* Set to PE1.0 */
	ret = rt9466_clr_bit(info, RT9466_REG_CHG_CTRL17,
		RT9466_MASK_PUMPX_20_10);

	/* Set Pump Up/Down */
	ret = (is_increase ? rt9466_set_bit : rt9466_clr_bit)
		(info, RT9466_REG_CHG_CTRL17, RT9466_MASK_PUMPX_UP_DN);

	/* Enable PumpX */
	ret = rt9466_enable_pump_express(info, true);
	mutex_unlock(&info->pe_access_lock);

	return ret;
}

static int rt9466_set_pep20_reset(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->pe_access_lock);
	ret = rt9466_set_mivr(chg_dev, 4500000);
	if (ret < 0)
		goto out;

	/* Disable PSK mode */
	rt9466_enable_hidden_mode(info, true);
	ret = rt9466_clr_bit(info, RT9466_REG_CHG_HIDDEN_CTRL9, 0x80);
	if (ret < 0)
		rt_debug("%s: disable skip mode fail\n", __func__);

	ret = rt9466_set_aicr(chg_dev, 100000);
	if (ret < 0)
		goto psk_out;

	msleep(250);

	ret = rt9466_set_aicr(chg_dev, 700000);

psk_out:
	rt9466_set_bit(info, RT9466_REG_CHG_HIDDEN_CTRL9, 0x80);
	rt9466_enable_hidden_mode(info, false);
out:
	mutex_unlock(&info->pe_access_lock);
	return ret;
}

static int rt9466_set_pep20_current_pattern(struct charger_device *chg_dev,
	u32 uV)
{
	int ret = 0;
	u8 reg_volt = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	mutex_lock(&info->pe_access_lock);

	reg_volt = rt9466_closest_reg(RT9466_PEP20_VOLT_MIN,
		RT9466_PEP20_VOLT_MAX, RT9466_PEP20_VOLT_STEP, uV);

	rt_info("%s: volt = %d(0x%02X)\n", __func__, uV, reg_volt);

	/* Set to PEP2.0 */
	ret = rt9466_set_bit(info, RT9466_REG_CHG_CTRL17,
		RT9466_MASK_PUMPX_20_10);
	if (ret < 0)
		goto out;

	/* Set Voltage */
	ret = rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL17,
		reg_volt << RT9466_SHIFT_PUMPX_DEC, RT9466_MASK_PUMPX_DEC);
	if (ret < 0)
		goto out;

	/* Enable PumpX */
	ret = rt9466_enable_pump_express(info, true);

out:
	mutex_unlock(&info->pe_access_lock);
	return ret;
}

static int rt9466_enable_cable_drop_comp(struct charger_device *chg_dev,
	bool en)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	rt_info("%s: en = %d\n", __func__, en);

	mutex_lock(&info->pe_access_lock);

	/* Set to PEP2.0 */
	ret = rt9466_set_bit(info, RT9466_REG_CHG_CTRL17,
		RT9466_MASK_PUMPX_20_10);
	if (ret < 0)
		goto out;

	/* Set Voltage */
	ret = rt9466_i2c_update_bits(info, RT9466_REG_CHG_CTRL17,
		0x1F << RT9466_SHIFT_PUMPX_DEC, RT9466_MASK_PUMPX_DEC);
	if (ret < 0)
		goto out;

	/* Enable PumpX */
	ret = rt9466_enable_pump_express(info, true);

out:
	mutex_unlock(&info->pe_access_lock);
	return ret;
}

static int rt9466_get_ichg(struct charger_device *chg_dev, u32 *ichg)
{
	int ret = 0;
	u8 reg_ichg = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9466_i2c_read_byte(info, RT9466_REG_CHG_CTRL7);
	if (ret < 0)
		return ret;

	reg_ichg = (ret & RT9466_MASK_ICHG) >> RT9466_SHIFT_ICHG;
	*ichg = rt9466_closest_value(RT9466_ICHG_MIN, RT9466_ICHG_MAX,
		RT9466_ICHG_STEP, reg_ichg);

	return ret;
}

static int rt9466_get_aicr(struct charger_device *chg_dev, u32 *aicr)
{
	int ret = 0;
	u8 reg_aicr = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9466_i2c_read_byte(info, RT9466_REG_CHG_CTRL3);
	if (ret < 0)
		return ret;

	reg_aicr = (ret & RT9466_MASK_AICR) >> RT9466_SHIFT_AICR;
	*aicr = rt9466_closest_value(RT9466_AICR_MIN, RT9466_AICR_MAX,
		RT9466_AICR_STEP, reg_aicr);

	return ret;
}

static int rt9466_get_cv(struct charger_device *chg_dev, u32 *cv)
{
	int ret = 0;
	u8 reg_cv = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9466_i2c_read_byte(info, RT9466_REG_CHG_CTRL4);
	if (ret < 0)
		return ret;

	reg_cv = (ret & RT9466_MASK_CV) >> RT9466_SHIFT_CV;
	*cv = rt9466_closest_value(RT9466_CV_MIN, RT9466_CV_MAX,
		RT9466_CV_STEP, reg_cv);

	return ret;
}

static int rt9466_get_tchg(struct charger_device *chg_dev, int *tchg_min,
	int *tchg_max)
{
	int ret = 0, adc_temp = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);
	bool hz_en = false;

	/* Check HZ mode */
	ret = rt9466_i2c_test_bit(info, RT9466_REG_CHG_CTRL1,
		RT9466_SHIFT_HZ_EN, &hz_en);
	if (ret < 0)
		return ret;

	if (hz_en) {
		*tchg_min = 25;
		*tchg_max = 25;
		return -EINVAL;
	}

	/* Get value from ADC */
	ret = rt9466_get_adc(info, RT9466_ADC_TEMP_JC, &adc_temp);
	if (ret < 0)
		return ret;

	*tchg_min = adc_temp;
	*tchg_max = adc_temp;

	rt_info("%s: temperature = %d\n", __func__, adc_temp);
	return ret;
}

#if 0 /* Uncomment if you need this API */
static int rt9466_get_vbus(struct charger_device *chg_dev, u32 *vbus)
{
	int ret = 0, adc_vbus = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	/* Get value from ADC */
	ret = rt9466_get_adc(info, RT9466_ADC_VBUS_DIV2, &adc_vbus);
	if (ret < 0)
		return ret;

	*vbus = adc_vbus;

	dev_info(info->dev, "%s: vbus = %dmA\n", __func__, adc_vbus);
	return ret;
}
#endif

static int rt9466_is_charging_done(struct charger_device *chg_dev, bool *done)
{
	int ret = 0;
	enum rt9466_charging_status chg_stat = RT9466_CHG_STATUS_READY;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = rt9466_get_charging_status(info, &chg_stat);

	/* Return is charging done or not */
	switch (chg_stat) {
	case RT9466_CHG_STATUS_READY:
	case RT9466_CHG_STATUS_PROGRESS:
	case RT9466_CHG_STATUS_FAULT:
		*done = false;
		break;
	case RT9466_CHG_STATUS_DONE:
		*done = true;
		break;
	default:
		*done = false;
		break;
	}

	return ret;
}

static int rt9466_is_safety_timer_enable(struct charger_device *chg_dev,
	bool *en)
{
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	return rt9466_i2c_test_bit(info, RT9466_REG_CHG_CTRL12,
		RT9466_SHIFT_TMR_EN, en);
}

static int rt9466_kick_wdt(struct charger_device *chg_dev)
{
	enum rt9466_charging_status chg_status = RT9466_CHG_STATUS_READY;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	/* Any I2C communication can reset watchdog timer */
	return rt9466_get_charging_status(info, &chg_status);
}

static int rt9466_set_pep20_efficiency_table(struct charger_device *chg_dev)
{
	int ret = 0;
	struct charger_manager *chg_mgr = NULL;

	chg_mgr = charger_dev_get_drvdata(chg_dev);
	if (!chg_mgr)
		return -EINVAL;

	chg_mgr->pe2.profile[0].vchr = 8000000;
	chg_mgr->pe2.profile[1].vchr = 8000000;
	chg_mgr->pe2.profile[2].vchr = 8000000;
	chg_mgr->pe2.profile[3].vchr = 8500000;
	chg_mgr->pe2.profile[4].vchr = 8500000;
	chg_mgr->pe2.profile[5].vchr = 8500000;
	chg_mgr->pe2.profile[6].vchr = 9000000;
	chg_mgr->pe2.profile[7].vchr = 9000000;
	chg_mgr->pe2.profile[8].vchr = 9500000;
	chg_mgr->pe2.profile[9].vchr = 9500000;

	return ret;
}

static int __rt9466_enable_auto_sensing(struct rt9466_info *info, bool en)
{
	int ret = 0;
	u8 auto_sense = 0;
	u8 exit_hid = 0x00;

	/* enter hidden mode */
	ret = rt9466_device_write(info->client, 0x70,
		ARRAY_SIZE(rt9466_val_en_hidden_mode),
		rt9466_val_en_hidden_mode);
	if (ret < 0)
		return ret;

	ret = rt9466_device_read(info->client, RT9466_REG_CHG_HIDDEN_CTRL15, 1,
		&auto_sense);
	if (ret < 0) {
		rt_debug("%s: read auto sense fail\n", __func__);
		goto out;
	}

	if (en)
		auto_sense &= 0xFE; /* clear bit0 */
	else
		auto_sense |= 0x01; /* set bit0 */
	ret = rt9466_device_write(info->client, RT9466_REG_CHG_HIDDEN_CTRL15, 1,
		&auto_sense);
	if (ret < 0)
		rt_debug("%s: en = %d fail\n", __func__, en);

out:
	return rt9466_device_write(info->client, 0x70, 1, &exit_hid);
}

/*
 * This function is used in shutdown function
 * Use i2c smbus directly
 */
static int rt9466_sw_reset(struct rt9466_info *info)
{
	int ret = 0;
	u8 evt[RT9466_IRQIDX_MAX] = {0};

	/* Register 0x01 ~ 0x10 */
	u8 reg_data[] = {
		0x10, 0x03, 0x23, 0x3C, 0x67, 0x0B, 0x4C, 0xA1,
		0x3C, 0x58, 0x2C, 0x02, 0x52, 0x05, 0x00, 0x10
	};

	rt_info("%s\n", __func__);

	/* Disable auto sensing/Enable HZ,ship mode of secondary charger */
	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0) {
		mutex_lock(&info->hidden_mode_lock);
		mutex_lock(&info->i2c_access_lock);
		__rt9466_enable_auto_sensing(info, false);
		mutex_unlock(&info->i2c_access_lock);
		mutex_unlock(&info->hidden_mode_lock);

		reg_data[0] = 0x14; /* HZ */
		reg_data[1] = 0x83; /* Shipping mode */
	}

	/* Mask all irq */
	mutex_lock(&info->i2c_access_lock);
	ret = rt9466_device_write(info->client, RT9466_REG_CHG_STATC_CTRL,
		ARRAY_SIZE(rt9466_irq_maskall), rt9466_irq_maskall);
	if (ret < 0)
		rt_debug("%s: mask all irq fail\n", __func__);

	/* Read all irq */
	ret = rt9466_device_read(info->client, RT9466_REG_CHG_STATC, 5, evt);
	if (ret < 0)
		rt_debug("%s: read evt fail\n", __func__);

	/* Reset necessary registers */
	ret = rt9466_device_write(info->client, RT9466_REG_CHG_CTRL1,
		ARRAY_SIZE(reg_data), reg_data);
	if (ret < 0)
		rt_debug("%s: reset registers fail\n", __func__);
	mutex_unlock(&info->i2c_access_lock);

	return ret;
}

static int rt9466_init_setting(struct rt9466_info *info)
{
	int ret = 0;
	struct rt9466_desc *desc = info->desc;
	u8 evt[RT9466_IRQIDX_MAX] = {0};

	rt_info("%s\n", __func__);

	/* mask all irq */
	ret = rt9466_maskall_irq(info);
	if (ret < 0) {
		rt_debug("%s: mask all irq fail\n", __func__);
		goto err;
	}

	/* clear event */
	ret = rt9466_i2c_block_read(info, RT9466_REG_CHG_STATC, ARRAY_SIZE(evt),
		evt);
	if (ret < 0) {
		rt_debug("%s: clr evt fail(%d)\n", __func__, ret);
		goto err;
	}


	ret = __rt9466_set_ichg(info, desc->ichg);
	if (ret < 0)
		rt_debug("%s: set ichg fail\n", __func__);

	ret = __rt9466_set_aicr(info, desc->aicr);
	if (ret < 0)
		rt_debug("%s: set aicr fail\n", __func__);

	ret = __rt9466_set_mivr(info, desc->mivr);
	if (ret < 0)
		rt_debug("%s: set mivr fail\n", __func__);

	ret = __rt9466_set_cv(info, desc->cv);
	if (ret < 0)
		rt_debug("%s: set cv fail\n", __func__);

	ret = __rt9466_set_ieoc(info, desc->ieoc);
	if (ret < 0)
		rt_debug("%s: set ieoc fail\n", __func__);

	ret = __rt9466_enable_te(info, desc->en_te);
	if (ret < 0)
		rt_debug("%s: set te fail\n", __func__);

	ret = rt9466_set_safety_timer(info, desc->safety_timer);
	if (ret < 0)
		rt_debug("%s: set fast timer fail\n", __func__);

	ret = __rt9466_enable_safety_timer(info, true);
	if (ret < 0)
		rt_debug("%s: enable chg timer fail\n", __func__);

	ret = rt9466_enable_wdt(info, desc->en_wdt);
	if (ret < 0)
		rt_debug("%s: set wdt fail\n", __func__);

	ret = rt9466_enable_jeita(info, desc->en_jeita);
	if (ret < 0)
		rt_debug("%s: disable jeita fail\n", __func__);

	ret = rt9466_enable_irq_pulse(info, desc->en_irq_pulse);
	if (ret < 0)
		rt_debug("%s: set irq pulse fail\n", __func__);

	/* Set ircomp according to BIF */
	ret = rt9466_set_ircmp_resistor(info, desc->ircmp_resistor);
	if (ret < 0)
		rt_debug("%s: set ircmp resistor fail\n", __func__);

	ret = rt9466_set_ircmp_vclamp(info, desc->ircmp_vclamp);
	if (ret < 0)
		rt_debug("%s: set ircmp clamp fail\n", __func__);

	ret = rt9466_disable_charger_reset(info, true);
	if (ret < 0)
		rt_debug("%s: disable charger reset fail\n", __func__);

	ret = rt9466_sw_workaround(info);
	if (ret < 0) {
		rt_debug("%s: workaround fail\n", __func__);
		return ret;
	}

	/* Enable HZ mode of secondary charger */
	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0) {
		ret = rt9466_enable_hz(info, true);
		if (ret < 0)
			rt_debug("%s: hz sec chg fail\n", __func__);
	}
err:
	return ret;
}

static int rt9466_plug_in(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	rt_info("%s\n", __func__);

	/* Enable WDT */
	if (info->desc->en_wdt) {
		ret = rt9466_enable_wdt(info, true);
		if (ret < 0)
			rt_debug("%s: en wdt failed\n", __func__);
	}

	/* Enable charging */
	if (strcmp(info->desc->chg_dev_name, "primary_chg") == 0) {
		ret = rt9466_enable_charging(chg_dev, true);
		if (ret < 0)
			rt_debug("%s: en chg fail\n", __func__);
	}

	return ret;
}

static int rt9466_plug_out(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	rt_info("%s\n", __func__);

	/* Reset AICR limit */
	info->aicr_limit = -1;

	/* Disable charging */
	ret = rt9466_enable_charging(chg_dev, false);
	if (ret < 0) {
		rt_debug("%s: disable chg fail\n", __func__);
		return ret;
	}

	/* Disable WDT */
	ret = rt9466_enable_wdt(info, false);
	if (ret < 0)
		rt_debug("%s: disable wdt fail\n", __func__);

	/* enable HZ mode of secondary charger */
	if (strcmp(info->desc->chg_dev_name, "secondary_chg") == 0) {
		ret = rt9466_enable_hz(info, true);
		if (ret < 0)
			rt_debug("%s: en hz of sec chg fail\n",
				__func__);
	}

	return ret;
}

static int rt9466_is_charging_enable(struct charger_device *chg_dev, bool *en)
{
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9466_is_charging_enable(info, en);
}

static int rt9466_get_min_ichg(struct charger_device *chg_dev, u32 *uA)
{
	int ret = 0;

	*uA = rt9466_closest_value(RT9466_ICHG_MIN, RT9466_ICHG_MAX,
		RT9466_ICHG_STEP, 0);

	return ret;
}

static int rt9466_run_aicl(struct charger_device *chg_dev, u32 *uA)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	ret = __rt9466_run_aicl(info);
	if (ret >= 0)
		*uA = info->aicr_limit;

	return ret;
}

static int rt9466_dump_register(struct charger_device *chg_dev)
{
	int i = 0, ret = 0;
	u32 ichg = 0, aicr = 0, mivr = 0, ieoc = 0;
	bool chg_en = 0;
	int adc_vsys = 0, adc_vbat = 0, adc_ibat = 0, adc_ibus = 0;
	enum rt9466_charging_status chg_status = RT9466_CHG_STATUS_READY;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);
	u8 chg_stat = 0, chg_ctrl[2] = {0};

	ret = rt9466_get_ichg(chg_dev, &ichg);
	ret = rt9466_get_aicr(chg_dev, &aicr);
	ret = rt9466_get_mivr(info, &mivr);
	ret = __rt9466_is_charging_enable(info, &chg_en);
	ret = rt9466_get_ieoc(info, &ieoc);
	ret = rt9466_get_charging_status(info, &chg_status);
	ret = rt9466_get_adc(info, RT9466_ADC_VSYS, &adc_vsys);
	ret = rt9466_get_adc(info, RT9466_ADC_VBAT, &adc_vbat);
	ret = rt9466_get_adc(info, RT9466_ADC_IBAT, &adc_ibat);
	ret = rt9466_get_adc(info, RT9466_ADC_IBUS, &adc_ibus);
	chg_stat = rt9466_i2c_read_byte(info, RT9466_REG_CHG_STATC);
	ret = rt9466_i2c_block_read(info, RT9466_REG_CHG_CTRL1, 2, chg_ctrl);

	/* Charging fault, dump all registers' value */
	if (chg_status == RT9466_CHG_STATUS_FAULT) {
		for (i = 0; i < ARRAY_SIZE(rt9466_reg_addr); i++)
			ret = rt9466_i2c_read_byte(info, rt9466_reg_addr[i]);
	}

	rt_info("%s: ICHG = %dmA, AICR = %dmA, MIVR = %dmV, IEOC = %dmA\n",
		__func__, ichg / 1000, aicr / 1000, mivr / 1000, ieoc / 1000);

	rt_info("%s: VSYS = %dmV, VBAT = %dmV, IBAT = %dmA, IBUS = %dmA\n",
		__func__, adc_vsys / 1000, adc_vbat / 1000, adc_ibat / 1000,
		adc_ibus / 1000);

	rt_info("%s: CHG_EN = %d, CHG_STATUS = %s, CHG_STAT = 0x%02X\n",
		__func__, chg_en, rt9466_chg_status_name[chg_status], chg_stat);

	rt_info("%s: CHG_CTRL1 = 0x%02X, CHG_CTRL2 = 0x%02X\n",
		__func__, chg_ctrl[0], chg_ctrl[1]);

	return ret;
}

static int rt9466_enable_te(struct charger_device *chg_dev, bool en)
{
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	return __rt9466_enable_te(info, en);
}

static int rt9466_reset_eoc_state(struct charger_device *chg_dev)
{
	int ret = 0;
	struct rt9466_info *info = dev_get_drvdata(&chg_dev->dev);

	/* Toggle EOC_RST */
	rt9466_enable_hidden_mode(info, true);
	ret = rt9466_set_bit(info, RT9466_REG_CHG_HIDDEN_CTRL1, 0x80);
	if (ret < 0) {
		rt_debug("%s: set eoc rst fail\n", __func__);
		goto out;
	}

	ret = rt9466_clr_bit(info, RT9466_REG_CHG_HIDDEN_CTRL1, 0x80);
	if (ret < 0)
		rt_debug("%s: clr eoc rst fail\n", __func__);
out:
	rt9466_enable_hidden_mode(info, false);
	return ret;
}

static int rt9466_do_event(struct charger_device *chg_dev, u32 event, u32 args)
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

static int rt9466_is_chip_enabled(struct charger_device *chg_dev, bool *en)
{
	*en = true;
	return 0;
}

static struct charger_ops rt9466_chg_ops = {
	/* Normal charging */
	.plug_in = rt9466_plug_in,
	.plug_out = rt9466_plug_out,
	.dump_registers = rt9466_dump_register,
	.enable = rt9466_enable_charging,
	.is_enabled = rt9466_is_charging_enable,
	.get_charging_current = rt9466_get_ichg,
	.set_charging_current = rt9466_set_ichg,
	.get_input_current = rt9466_get_aicr,
	.set_input_current = rt9466_set_aicr,
	.get_constant_voltage = rt9466_get_cv,
	.set_constant_voltage = rt9466_set_cv,
	.kick_wdt = rt9466_kick_wdt,
	.set_mivr = rt9466_set_mivr,
	.is_charging_done = rt9466_is_charging_done,
	.get_min_charging_current = rt9466_get_min_ichg,
	.set_eoc_current = rt9466_set_ieoc,
	.enable_termination = rt9466_enable_te,
	.run_aicl = rt9466_run_aicl,
	.reset_eoc_state = rt9466_reset_eoc_state,
	.is_chip_enabled = rt9466_is_chip_enabled,

	/* Safety timer */
	.enable_safety_timer = rt9466_enable_safety_timer,
	.is_safety_timer_enabled = rt9466_is_safety_timer_enable,

	/* Power path */
	.enable_powerpath = rt9466_enable_power_path,
	.is_powerpath_enabled = rt9466_is_power_path_enable,

	/* OTG */
	.enable_otg = rt9466_enable_otg,
	.set_boost_current_limit = rt9466_set_boost_current_limit,
	.enable_discharge = rt9466_enable_discharge,

	/* PE+/PE+20 */
	.send_ta_current_pattern = rt9466_set_pep_current_pattern,
	.set_pe20_efficiency_table = rt9466_set_pep20_efficiency_table,
	.send_ta20_current_pattern = rt9466_set_pep20_current_pattern,
	/* .set_ta20_reset = rt9466_set_pep20_reset
	 * rename function pointer in new version to reset_ta
	 */
	.reset_ta = rt9466_set_pep20_reset,
	.enable_cable_drop_comp = rt9466_enable_cable_drop_comp,

	/* ADC */
	.get_tchg_adc = rt9466_get_tchg,

	/* Event */
	.event = rt9466_do_event,
};

static void rt9466_init_setting_work_handler(struct work_struct *work)
{
	int ret = 0, retry_cnt = 0;
	struct rt9466_info *info = (struct rt9466_info *)container_of(work,
		struct rt9466_info, init_work);

	do {
		/* Select IINLMTSEL to use AICR */
		ret = rt9466_select_input_current_limit(info,
			RT9466_IINLMTSEL_AICR);
		if (ret < 0) {
			rt_debug("%s: sel ilmtsel fail\n", __func__);
			retry_cnt++;
		}
	} while (retry_cnt < 5 && ret < 0);

	msleep(150);

	retry_cnt = 0;
	do {
		/* Disable hardware ILIM */
		ret = rt9466_enable_ilim(info, false);
		if (ret < 0) {
			rt_debug("%s: disable ilim fail\n",
				 __func__);
			retry_cnt++;
		}
	} while (retry_cnt < 5 && ret < 0);

	rt9466_dump_register(info->chg_dev);
}

/* ========================= */
/* I2C driver function       */
/* ========================= */

static int rt9466_probe(struct i2c_client *client,
	const struct i2c_device_id *dev_id)
{
	int ret = 0;
	struct rt9466_info *info = NULL;

	rt_info("%s(%s)\n", __func__, RT9466_DRV_VERSION);

	info = devm_kzalloc(&client->dev, sizeof(struct rt9466_info),
		GFP_KERNEL);
	if (!info)
		return -ENOMEM;

	mutex_init(&info->i2c_access_lock);
	mutex_init(&info->adc_access_lock);
	mutex_init(&info->irq_access_lock);
	mutex_init(&info->aicr_access_lock);
	mutex_init(&info->ichg_access_lock);
	mutex_init(&info->hidden_mode_lock);
	mutex_init(&info->pe_access_lock);
	mutex_init(&info->ieoc_lock);
	info->client = client;
	info->dev = &client->dev;
	info->aicr_limit = -1;
	info->hidden_mode_cnt = 0;
	info->ieoc_wkard = false;
	info->ieoc = 250000; /* register default value 250mA */
	memcpy(info->irq_mask, rt9466_irq_maskall, RT9466_IRQIDX_MAX);

	/* Init wait queue head */
	init_waitqueue_head(&info->wait_queue);

	INIT_WORK(&info->init_work, rt9466_init_setting_work_handler);

	/* Is HW exist */
	if (!rt9466_is_hw_exist(info)) {
		rt_debug("%s: no rt9466 exists\n", __func__);
		ret = -ENODEV;
		goto err_no_dev;
	}
	i2c_set_clientdata(client, info);

	ret = rt9466_parse_dt(info, &client->dev);
	if (ret < 0) {
		rt_debug("%s: parse dt fail\n", __func__);
		goto err_parse_dt;
	}

#ifdef CONFIG_RT_REGMAP
	ret = rt9466_register_rt_regmap(info);
	if (ret < 0)
		goto err_register_regmap;
#endif

	ret = rt9466_reset_chip(info);
	if (ret < 0) {
		rt_debug("%s: reset chip fail\n", __func__);
		goto err_reset_chip;
	}

	ret = rt9466_init_setting(info);
	if (ret < 0) {
		rt_debug("%s: init setting fail\n", __func__);
		goto err_init_setting;
	}

	/* Register charger device */
	info->chg_dev = charger_device_register(
		info->desc->chg_dev_name, info->dev, info, &rt9466_chg_ops,
		&info->chg_props);
	if (IS_ERR_OR_NULL(info->chg_dev)) {
		ret = PTR_ERR(info->chg_dev);
		goto err_register_chg_dev;
	}

	ret = rt9466_irq_register(info);
	if (ret < 0) {
		rt_debug("%s: irq register fail\n", __func__);
		goto err_irq_register;
	}

	ret = rt9466_irq_init(info);
	if (ret < 0) {
		rt_debug("%s: irq init fail\n", __func__);
		goto err_irq_init;
	}

	schedule_work(&info->init_work);
	rt_info("%s: successfully\n", __func__);
	return ret;

err_irq_init:
err_irq_register:
	charger_device_unregister(info->chg_dev);
err_register_chg_dev:
err_init_setting:
err_reset_chip:
#ifdef CONFIG_RT_REGMAP
	rt_regmap_device_unregister(info->regmap_dev);
err_register_regmap:
#endif
err_parse_dt:
err_no_dev:
	mutex_destroy(&info->i2c_access_lock);
	mutex_destroy(&info->adc_access_lock);
	mutex_destroy(&info->irq_access_lock);
	mutex_destroy(&info->aicr_access_lock);
	mutex_destroy(&info->ichg_access_lock);
	mutex_destroy(&info->hidden_mode_lock);
	mutex_destroy(&info->pe_access_lock);
	mutex_destroy(&info->ieoc_lock);
	return ret;
}


static int rt9466_remove(struct i2c_client *client)
{
	int ret = 0;
	struct rt9466_info *info = i2c_get_clientdata(client);

	rt_info("%s\n", __func__);

	if (info) {
#ifdef CONFIG_RT_REGMAP
		rt_regmap_device_unregister(info->regmap_dev);
#endif
		mutex_destroy(&info->i2c_access_lock);
		mutex_destroy(&info->adc_access_lock);
		mutex_destroy(&info->irq_access_lock);
		mutex_destroy(&info->aicr_access_lock);
		mutex_destroy(&info->ichg_access_lock);
		mutex_destroy(&info->hidden_mode_lock);
		mutex_destroy(&info->pe_access_lock);
		mutex_destroy(&info->ieoc_lock);
	}

	return ret;
}

static void rt9466_shutdown(struct i2c_client *client)
{
	int ret = 0;
	struct rt9466_info *info = i2c_get_clientdata(client);

	rt_info("%s\n", __func__);
	if (info) {
		ret = rt9466_sw_reset(info);
		if (ret < 0)
			rt_debug("%s: sw reset fail\n", __func__);
	}
}

static int rt9466_suspend(struct device *dev)
{
	struct rt9466_info *info = dev_get_drvdata(dev);

	rt_info("%s\n", __func__);
	if (device_may_wakeup(dev))
		enable_irq_wake(info->irq);

	return 0;
}

static int rt9466_resume(struct device *dev)
{
	struct rt9466_info *info = dev_get_drvdata(dev);

	rt_info("%s\n", __func__);
	if (device_may_wakeup(dev))
		disable_irq_wake(info->irq);

	return 0;
}

static SIMPLE_DEV_PM_OPS(rt9466_pm_ops, rt9466_suspend, rt9466_resume);

static const struct i2c_device_id rt9466_i2c_id[] = {
	{"rt9466", 0},
	{}
};
MODULE_DEVICE_TABLE(i2c, rt9466_i2c_id);

static const struct of_device_id rt9466_of_match[] = {
	{ .compatible = "richtek,rt9466", },
	{},
};
MODULE_DEVICE_TABLE(of, rt9466_of_match);

#ifndef CONFIG_OF
#define RT9466_BUSNUM 1

static struct i2c_board_info rt9466_i2c_board_info __initdata = {
	I2C_BOARD_INFO("rt9466", RT9466_SALVE_ADDR)
};
#endif /* CONFIG_OF */


static struct i2c_driver rt9466_i2c_driver = {
	.driver = {
		.name = "rt9466",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(rt9466_of_match),
		.pm = &rt9466_pm_ops,
	},
	.probe = rt9466_probe,
	.remove = rt9466_remove,
	.shutdown = rt9466_shutdown,
	.id_table = rt9466_i2c_id,
};

static int __init rt9466_init(void)
{
	int ret = 0;

#ifdef CONFIG_OF
	rt_info("%s: with dts\n", __func__);
#else
	rt_info("%s: without dts\n", __func__);
	i2c_register_board_info(RT9466_BUSNUM, &rt9466_i2c_board_info, 1);
#endif

	ret = i2c_add_driver(&rt9466_i2c_driver);
	if (ret < 0)
		rt_debug("%s: register i2c driver fail\n", __func__);

	return ret;
}
module_init(rt9466_init);


static void __exit rt9466_exit(void)
{
	i2c_del_driver(&rt9466_i2c_driver);
}
module_exit(rt9466_exit);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("ShuFanLee <shufan_lee@richtek.com>");
MODULE_DESCRIPTION("RT9466 Charger Driver");
MODULE_VERSION(RT9466_DRV_VERSION);

/*
 * Release Note
 * 1.0.12
 * (1) Fix type error of enable_auto_sensing in sw_reset
 * (2) Check HZ mode for get_tchg
 * (3) Add is_chip_enabled ops
 *
 * 1.0.11
 * (1) Add do event interface for polling mode
 * (2) Enable IRQ_RZE at the end of irq handler
 * (3) Remove IRQ related registers from reg_addr
 * (4) Do ilim select in WQ and register charger class in probe
 * (5) Move irq_mask to info structure
 *
 * 1.0.10
 * (1) Filter out not changed irq state
 * (2) Not to use CHG_IRQ3
 * (3) Set irq to wake up system
 * (4) Add IEOC inaccuracy workaround
 * (5) Move init setting & sw workaround to work queue
 * (6) Add en_irq_pulse & en_jeita property in dtsi
 * (7) Dump ctrl1&2 in dump register
 * (8) Remove set ichg to 512mA in reset_pep20
 * (9) Release set_ieoc/enable_te interface
 * (10) Disable spk mode in pep20_reest
 * (11) For secondary chg, enter shipping mode before shdn
 * (12) Workaround to keep Ichg >= 900mA
 * (13) Add workaround to adjust slow rate for OTG
 *
 * 1.0.9
 * (1) Prevent backboot
 * (2) Set secondary chg to HZ in plug out callback
 * (3) Add CEB pin control for secondary charger
 * (4) After PE pattern -> Enable skip mode
 *     Disable skip mode -> Start PE pattern
 *
 * 1.0.8
 * (1) Modify init sequence for init_irq
 * (2) Add enable_cable_drop_com, is_charging_enabled, get_min_ichg OPS
 * (3) Set secondary chg to HZ if it is not in charging mode
 *
 * 1.0.7
 * (1) Modify IBAT/IBUS ADC's coefficient
 * (2) Add interrupt-names in dts property to represent those evts that need to
 *     be unmasked
 * (3) Add IRQ handlers to handle each irq event separately
 * (4) Use dev_xxx instead of pr_xxx
 * (5) Modify some naming(bat_voreg -> cv, iin_vth -> aicl_vth) and remove some
 *     unnecessary code
 *
 * 1.0.6
 * (1) Adapt to GM30
 * (2) Modify irq init value
 *
 * 1.0.5
 * (1) Disable all irq in irq_init
 * (2) Modify rt9466_is_hw_exist, check vendor id and revision id separately
 *
 * 1.0.4
 * (1) Not to unmask the plug-in/out related IRQs in irq_init
 * PWR_RDYM/CHG_MIVRM/CHG_AICRM
 * VBUSOVM
 * TS_BAT_HOTM/TS_BAT_WARMM/TS_BAT_COOLM/TS_BAT_COLDM
 * CHG_OTPM/CHG_RVPM/CHG_ADPBADM/CHG_STATCM/CHG_FAULTM/TS_STATCM
 * IEOCM/TERMM/SSFINISHM/SSFINISHM/AICLMeasM
 * BST_BATUVM/PUMPX_DONEM/ADC_DONEM
 *
 * 1.0.3
 * (1) Copy default dts value before parsing dts
 * (2) Release rt_charger_sw_reset interface
 * (3) Add chip revision E4 (0x84)
 */
