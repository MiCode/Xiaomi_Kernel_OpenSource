/*
 * ln8000-charger.c - Charger driver for LIONSEMI LN8000
 *
 * Copyright (C) 2021 Lion Semiconductor Inc.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 */

#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/power_supply.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/err.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/regulator/machine.h>
#include <linux/debugfs.h>
#include <linux/bitops.h>
#include <linux/math64.h>
#include <linux/version.h>
#include <linux/iio/iio.h>
#include <dt-bindings/iio/qti_power_supply_iio.h>
#include <linux/iio/consumer.h>
#include "ln8000_reg.h"
#include "ln8000_iio.h"
#include <linux/hardware_info.h>

static const char *ln8000_dev_name[] = {
        "ln8000-standalone-2",
        "ln8000-master",
        "ln8000-slave",
};

#define ln_err(fmt, ...)                        \
do {                                            \
        if (info->dev_role == LN_ROLE_STANDALONE)   \
                printk(KERN_ERR "ln8000-standalone-2: %s: " fmt, __func__, ##__VA_ARGS__);   \
        else if (info->dev_role == LN_ROLE_MASTER)                              \
                printk(KERN_ERR "ln8000-master: %s: " fmt, __func__, ##__VA_ARGS__);   \
        else                                                                    \
                printk(KERN_ERR "ln8000-slave: %s: " fmt, __func__, ##__VA_ARGS__);   \
} while (0);

#define ln_info(fmt, ...)                       \
do {                                            \
        if (info->dev_role == LN_ROLE_STANDALONE)   \
                printk(KERN_INFO "ln8000-standalone-2: %s: " fmt, __func__, ##__VA_ARGS__);  \
        else if (info->dev_role == LN_ROLE_MASTER)                              \
                printk(KERN_INFO "ln8000-master: %s: " fmt, __func__, ##__VA_ARGS__);  \
        else                                                                    \
                printk(KERN_INFO "ln8000-slave: %s: " fmt, __func__, ##__VA_ARGS__);  \
} while (0);

#define ln_dbg(fmt, ...)                        \
do {                                            \
        if (info->dev_role == LN_ROLE_STANDALONE)   \
                printk(KERN_DEBUG "ln8000-standalone-2: %s: " fmt, __func__, ##__VA_ARGS__); \
        else if (info->dev_role == LN_ROLE_MASTER)                              \
                printk(KERN_DEBUG "ln8000-master: %s: " fmt, __func__, ##__VA_ARGS__); \
        else                                                                    \
                printk(KERN_DEBUG "ln8000-slave: %s: " fmt, __func__, ##__VA_ARGS__); \
} while (0);

#define LN8000_REG_PRINT(reg_addr, val)         \
do {                                            \
        ln_info("  --> [%-20s]   0x%02X   :   0x%02X\n",\
                #reg_addr, LN8000_REG_##reg_addr, (val) & 0xFF);    \
} while (0);

#define LN8000_PARSE_PROP(ret, pdata, field, prop, default_prop)\
do {                                                            \
        if (ret) {                                                          \
            ln_info("%s = %d (set to default)\n", #field, default_prop);    \
            pdata->field = default_prop;                                    \
        } else {                                                            \
            ln_info("%s = %d\n", #field, prop);                             \
            pdata->field = prop;                                            \
        }                                                                   \
} while (0);

#define LN8000_BIT_CHECK(val, idx, desc) if(val & (1<<idx)) ln_info("-> %s\n", desc)
#define LN8000_USE_GPIO(pdata) ((pdata != NULL) && (!IS_ERR_OR_NULL(pdata->irq_gpio)))
#define LN8000_STATUS(val, mask) ((val & mask) ? true : false)

enum {
	VBUS_ERROR_NONE,
	VBUS_ERROR_LOW,
	VBUS_ERROR_HIGHT,
};

/**
 * driver instance structure definition
 */
struct ln8000_platform_data {
        struct gpio_desc *irq_gpio;     /* GPIO pin for (generic/power-on) interrupt  */

        /* feature configuration */
        unsigned int bat_ovp_th;        /* battery ovp threshold (mV) */
        unsigned int bat_ovp_alarm_th;  /* battery ovp alarm threshold (mV) */
        unsigned int bus_ovp_th;        /* IIN ovp threshold (mV) */
        unsigned int bus_ovp_alarm_th;  /* IIN ovp alarm threshold (mV) */
        unsigned int bus_ocp_th;        /* IIN ocp threshold (mA) */
        unsigned int bus_ocp_alarm_th;  /* IIN ocp alarm threshold */
        unsigned int ntc_alarm_cfg;     /* input/battery NTC voltage threshold code: 0~1023 */

        /* protection enable/disable */
        bool vbat_ovp_disable;          /* disable battery voltage OVP */
        bool vbat_reg_disable;          /* disable battery voltage (float) regulation */
        bool iin_ocp_disable;           /* disable input current OCP */
        bool iin_reg_disable;           /* disable input current regulation */
        bool tbus_mon_disable;          /* disable BUS temperature monitor (prot/alarm) */
        bool tbat_mon_disable;          /* disable BAT temperature monitor (prot/alarm) */
        bool tdie_prot_disable;         /* disable die temperature protection */
        bool tdie_reg_disable;          /* disable die temperature regulation */
        bool revcurr_prot_disable;      /* disable reverse current protection */
};

struct ln8000_info {
        struct device *dev;
        struct i2c_client *client;
        struct ln8000_platform_data *pdata;
#ifndef CONFIG_WT_QGKI
		struct mutex irq_complete;
#endif

        struct mutex data_lock;
        struct mutex i2c_lock;
        struct mutex irq_lock;

        unsigned int op_mode;       /* target operation mode */
        unsigned int pwr_status;    /* current device status */
        unsigned int dev_role;      /* device role */

        /* system/device status */
        bool vbat_regulated;        /* vbat loop is active (+ OV alarm) */
        bool iin_regulated;         /* iin loop is active (+ OC alarm) */
        bool tdie_fault;            /* die temperature fault */
        bool tbus_tbat_fault;       /* BUS/BAT temperature fault */
        bool tdie_alarm;            /* die temperature alarm (regulated) */
        bool tbus_tbat_alarm;       /* BUS/BAT temperature alarm */
        bool wdt_fault;             /* watchdog timer expiration */
        bool vbat_ov;               /* vbat OV fault */
        bool vac_ov;                /* vac OV fault */
        bool vbus_ov;               /* vbus OV fault */
        bool iin_oc;                /* iin OC fault */
        bool vac_unplug;            /* vac unplugged */ //vbus_present
        bool iin_rc;                /* iin reverse current detected */
        bool volt_qual;             /* all voltages are qualified */
        bool usb_present;           /* usb plugged (present) */
        bool batt_present;
        bool chg_en;                /* charging enavbled */
        bool rcp_en;                /* reverse current protection enabled */
        int vbat_ovp_alarm_th;      /* vbat ovp alarm threshold */
        int vin_ovp_alarm_th;       /* vin ovp alarm threshold */
        int iin_ocp_alarm_th;       /* iin ocp alarm threshold */

        /* ADC readings */
        int	tbat_uV;                /* BAT temperature (NTC, uV) */
        int	tbus_uV;                /* BUS temperature (NTC, uV) */
        int	tdie_dC;                /* die temperature (deci-Celsius) */
        int	vbat_uV;                /* battery voltage (uV) */
        int	vbus_uV;                /* input voltage (uV) */
        int	iin_uA;                 /* input current (uV) */

        /* debugfs */
        struct dentry *debug_root;
        u32 debug_address;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
        struct iio_dev          *indio_dev;
        struct iio_chan_spec    *iio_chan;
        struct iio_channel	*int_iio_chans;
#else
        struct power_supply_desc psy_desc;
        struct power_supply_config psy_cfg;
        struct power_supply	*psy_chg;
#endif
};

/*static int hv_charge_enable = 1;
static int bq_charge_done;
*/


/**
 * I2C control functions : when occurred I2C tranfer fault, we 
 * will retry to it. (default count:3) 
 */
#define I2C_RETRY_CNT   3
static int ln8000_read_reg(struct ln8000_info *info, u8 addr, u8 *data)
{
        int i, ret = 0;

        mutex_lock(&info->i2c_lock);
        for (i=0; i < I2C_RETRY_CNT; ++i) {
                ret = i2c_smbus_read_byte_data(info->client, addr);
                if (IS_ERR_VALUE((unsigned long)ret)) {
                        ln_info("failed-read, reg(0x%02X), ret(%d)\n", addr, ret);
                } else {
                        *data = (u8)ret;
                }
        }
        mutex_unlock(&info->i2c_lock);
        return ret;
}

static int ln8000_bulk_read_reg(struct ln8000_info *info, u8 addr, void *data, int count)
{
        int i, ret = 0;
        mutex_lock(&info->i2c_lock);
        for (i=0; i < I2C_RETRY_CNT; ++i) {
                ret = i2c_smbus_read_i2c_block_data(info->client, addr, count, data);
                if (IS_ERR_VALUE((unsigned long)ret)) {
                        ln_info("failed-bulk-read, reg(0x%02X, %d bytes), ret(%d)\n", addr, count, ret);
                } else {
                        break;
                }
        }
        mutex_unlock(&info->i2c_lock);
        return ret;
}

static int ln8000_write_reg(struct ln8000_info *info, u8 addr, u8 data)
{
        int i, ret = 0;

        mutex_lock(&info->i2c_lock);
        for (i=0; i < I2C_RETRY_CNT; ++i) {
                ret = i2c_smbus_write_byte_data(info->client, addr, data);
                if (IS_ERR_VALUE((unsigned long)ret)) {
                        ln_info("failed-write, reg(0x%02X), ret(%d)\n", addr, ret);
                } else {
                        break;
                }
        }
        mutex_unlock(&info->i2c_lock);
        return ret;
}

static int ln8000_update_reg(struct ln8000_info *info, u8 addr, u8 mask, u8 data)
{
        int i, ret;
        u8 old_val, new_val;

        mutex_lock(&info->i2c_lock);
        for (i=0; i < I2C_RETRY_CNT; ++i) {
                ret = i2c_smbus_read_byte_data(info->client, addr);
                if (ret < 0) {
                        ln_err("failed-update, reg(0x%02X), ret(%d)\n", addr, ret);
                } else {
                        old_val = ret & 0xff;
                        new_val = (data & mask) | (old_val & ~(mask));
                        ret = i2c_smbus_write_byte_data(info->client, addr,
                                                        new_val);
                        if (ret < 0) {
                                ln_err("failed-update, reg(0x%02X), ret(%d)\n",
                                       addr, ret);
                        } else {
                                break;
                        }
                }
        }
        mutex_unlock(&info->i2c_lock);

        return ret;
}

/**
 * Register control functions
 */
static int ln8000_set_sw_freq(struct ln8000_info *info, u8 fsw_cfg)
{
    	return ln8000_update_reg(info, LN8000_REG_SYS_CTRL, 0xF << 4, fsw_cfg << 4);
}

static int ln8000_set_vac_ovp(struct ln8000_info *info, unsigned int ovp_th)
{
        u8 cfg;

        if (ovp_th <= 6500000) {
                cfg = LN8000_VAC_OVP_6P5V;
        } else if (ovp_th <= 11000000) {
                cfg = LN8000_VAC_OVP_11V;
        } else if (ovp_th <= 12000000) {
                cfg = LN8000_VAC_OVP_12V;
        } else {
                cfg = LN8000_VAC_OVP_13V;
        }

        return ln8000_update_reg(info, LN8000_REG_GLITCH_CTRL, 0x3 << 2, cfg << 2);
}

/* battery float voltage */
static int ln8000_set_vbat_float(struct ln8000_info *info, unsigned int cfg)
{
        u8 val;
        unsigned int adj_cfg = cfg - 30000;     /* adjust v_float bg offset (-30mV) */

        ln_info("ori_cfg=%d, adj_cfg=%d\n", cfg, adj_cfg);

        if (adj_cfg < LN8000_VBAT_FLOAT_MIN)
                val = 0x00;
        else if (adj_cfg > LN8000_VBAT_FLOAT_MAX)
                val = 0xFF;
        else
                val = (adj_cfg - LN8000_VBAT_FLOAT_MIN) / LN8000_VBAT_FLOAT_LSB;

        return ln8000_write_reg(info, LN8000_REG_V_FLOAT_CTRL, val);
}

static int ln8000_set_iin_limit(struct ln8000_info *info, unsigned int cfg)
{
        u8 val = cfg / LN8000_IIN_CFG_LSB;

        ln_info("iin_limit=%dmV(iin_ctrl=0x%x)\n", cfg / 1000, val);

        return ln8000_update_reg(info, LN8000_REG_IIN_CTRL, 0x7F, val);
}

static int ln8000_set_ntc_alarm(struct ln8000_info *info, unsigned int cfg)
{
        int ret;

        /* update lower bits */
        ret = ln8000_write_reg(info, LN8000_REG_NTC_CTRL, (cfg & 0xFF));
        if (ret < 0)
                return ret;

        /* update upper bits */
        ret = ln8000_update_reg(info, LN8000_REG_ADC_CTRL, 0x3, (cfg >> 8));
        return ret;
}

/* battery voltage OV protection */
static int ln8000_enable_vbat_ovp(struct ln8000_info *info, bool enable)
{
        u8 val;

        val = (enable) ? 0 : 1;//disable
        val <<= LN8000_BIT_DISABLE_VBAT_OV;

        return ln8000_update_reg(info, LN8000_REG_FAULT_CTRL, BIT(LN8000_BIT_DISABLE_VBAT_OV), val);
}

static int ln8000_enable_vbat_regulation(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_DISABLE_VFLOAT_LOOP,
                                !(enable) << LN8000_BIT_DISABLE_VFLOAT_LOOP);
}

static int ln8000_enable_vbat_loop_int(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_ENABLE_VFLOAT_LOOP_INT,
                                enable << LN8000_BIT_ENABLE_VFLOAT_LOOP_INT);
}

/* input current OC protection */
static int ln8000_enable_iin_ocp(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_FAULT_CTRL,
                                0x1 << LN8000_BIT_DISABLE_IIN_OCP,
                                !(enable) << LN8000_BIT_DISABLE_IIN_OCP);
}

static int ln8000_enable_iin_regulation(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_DISABLE_IIN_LOOP,
                                !(enable) << LN8000_BIT_DISABLE_IIN_LOOP);
}

static int ln8000_enable_iin_loop_int(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_ENABLE_IIN_LOOP_INT,
                                enable << LN8000_BIT_ENABLE_IIN_LOOP_INT);
}

static int ln8000_enable_tdie_prot(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_TEMP_MAX_EN, enable << LN8000_BIT_TEMP_MAX_EN);
}

static int ln8000_enable_tdie_regulation(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL,
                                0x1 << LN8000_BIT_TEMP_REG_EN, enable << LN8000_BIT_TEMP_REG_EN);
}

/* ADC channel enable */
static int ln8000_set_adc_ch(struct ln8000_info *info, unsigned int ch, bool enable)
{
        u8 mask;
        u8 val;
        int ret;

        if ((ch > LN8000_ADC_CH_ALL) || (ch < 1))
                return -EINVAL;

        if (ch == LN8000_ADC_CH_ALL) {
                // update all channels
                val  = (enable) ? 0x3E : 0x00;
                ret  = ln8000_write_reg(info, LN8000_REG_ADC_CFG, val);
        } else {
                // update selected channel
                mask = 1<<(ch-1);
                val  = (enable) ? 1 : 0;
                val <<= (ch-1);
                ret  = ln8000_update_reg(info, LN8000_REG_ADC_CFG, mask, val);
        }

        return ret;
}

/* BUS temperature monitoring (protection+alarm) */
static int ln8000_enable_tbus_monitor(struct ln8000_info *info, bool enable)
{
        int ret;

        /* enable BUS monitoring */
        ret = ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0x1 << 1, enable << 1);
        if (ret < 0)
                return ret;

        /* enable BUS ADC channel */
        if (enable) {
                ret = ln8000_set_adc_ch(info, LN8000_ADC_CH_TSBUS, true);
        }
        return ret;
}

/* BAT temperature monitoring (protection+alarm) */
static int ln8000_enable_tbat_monitor(struct ln8000_info *info, bool enable)
{
        int ret;

        /* enable BAT monitoring */
        ret = ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0x1 << 0, enable << 0);
        if (ret < 0)
                return ret;

        /* enable BAT ADC channel */
        if (enable) {
                ret = ln8000_set_adc_ch(info, LN8000_ADC_CH_TSBAT, true);
        }
        return ret;
}

/* watchdog timer */
static int ln8000_enable_wdt(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 7, enable << 7);
}

#if 0
static int ln8000_set_wdt(struct ln8000_info *info, unsigned int cfg)
{
        if (cfg >= LN8000_WATCHDOG_MAX) {
                cfg = LN8000_WATCHDOG_40SEC;
        }

        return ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x3 << 5, cfg << 5);
}
#endif

/* unplug / reverse-current detection */
static int ln8000_enable_rcp(struct ln8000_info *info, bool enable)
{
        info->rcp_en = enable;

        return ln8000_update_reg(info, LN8000_REG_SYS_CTRL,
                                BIT(LN8000_BIT_REV_IIN_DET),
                                enable << LN8000_BIT_REV_IIN_DET);
}

/* auto-recovery */
static int ln8000_enable_auto_recovery(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0xF << 4, ((0xF << 4) * enable));
}

static int ln8000_enable_rcp_auto_recovery(struct ln8000_info *info, bool enable)
{
        return ln8000_update_reg(info, LN8000_REG_RECOVERY_CTRL, 0x1 << 6, enable << 6);
}

static int ln8000_set_adc_mode(struct ln8000_info *info, unsigned int cfg)
{
        return ln8000_update_reg(info, LN8000_REG_ADC_CTRL, 0x7 << 5, cfg << 5);
}

static int ln8000_set_adc_hib_delay(struct ln8000_info *info, unsigned int cfg)
{
        return ln8000_update_reg(info, LN8000_REG_ADC_CTRL, 0x3 << 3, cfg << 3);
}

/* enable/disable STANDBY */
static inline void ln8000_sw_standby(struct ln8000_info *info, bool standby)
{
        u8 val = (standby) ? BIT(LN8000_BIT_STANDBY_EN) : 0x00;
        ln8000_update_reg(info, LN8000_REG_SYS_CTRL, BIT(LN8000_BIT_STANDBY_EN), val);
}

/* Convert Raw ADC Code */
static void ln8000_convert_adc_code(struct ln8000_info *info, unsigned int ch, u8 *sts, int *result)
{
        int adc_raw;	// raw ADC value
        int adc_final;	// final (converted) ADC value

        switch (ch) {
        case LN8000_ADC_CH_VOUT:
                adc_raw   = ((sts[1] & 0xFF)<<2) | ((sts[0] & 0xC0)>>6);
                adc_final = adc_raw * LN8000_ADC_VOUT_STEP;//uV
                break;
        case LN8000_ADC_CH_VIN:
                adc_raw   = ((sts[1] & 0x3F)<<4) | ((sts[0] & 0xF0)>>4);
                adc_final = adc_raw * LN8000_ADC_VIN_STEP;//uV
                break;
        case LN8000_ADC_CH_VBAT:
                adc_raw   = ((sts[1] & 0x03)<<8) | (sts[0] & 0xFF);
                adc_final = adc_raw * LN8000_ADC_VBAT_STEP;//uV
                break;
        case LN8000_ADC_CH_VAC:
                adc_raw   = (((sts[1] & 0x0F)<<6) | ((sts[0] & 0xFC)>>2)) + LN8000_ADC_VAC_OS;
                adc_final = adc_raw * LN8000_ADC_VAC_STEP;//uV
                break;
        case LN8000_ADC_CH_IIN:
                adc_raw   = ((sts[1] & 0x03)<<8) | (sts[0] & 0xFF);
                adc_final = adc_raw * LN8000_ADC_IIN_STEP;//uA
                break;
        case LN8000_ADC_CH_DIETEMP:
                adc_raw   = ((sts[1] & 0x0F)<<6) | ((sts[0] & 0xFC)>>2);
                adc_final = (935 - adc_raw) * LN8000_ADC_DIETEMP_STEP / LN8000_ADC_DIETEMP_DENOM;//dC
                if (adc_final > LN8000_ADC_DIETEMP_MAX)
                        adc_final = LN8000_ADC_DIETEMP_MAX;
                else if (adc_final < LN8000_ADC_DIETEMP_MIN)
                        adc_final = LN8000_ADC_DIETEMP_MIN;
                break;
        case LN8000_ADC_CH_TSBAT:
                adc_raw   = ((sts[1] & 0x3F)<<4) | ((sts[0] & 0xF0)>>4);
                adc_final = adc_raw * LN8000_ADC_NTCV_STEP;//(NTC) uV
                break;
        case LN8000_ADC_CH_TSBUS:
                adc_raw   = ((sts[1] & 0xFF)<<2) | ((sts[0] & 0xC0)>>6);
                adc_final = adc_raw * LN8000_ADC_NTCV_STEP;//(NTC) uV
                break;
        default:
                adc_raw   = -EINVAL;
                adc_final = -EINVAL;
                break;
        }

        *result = adc_final;
        return;
}

static void ln8000_print_regmap(struct ln8000_info *info)
{
        const u8 print_reg_num = (LN8000_REG_CHARGE_CTRL - LN8000_REG_INT1_MSK) + 1;
        u8 regs[64] = {0x0, };
        char temp_buf[128] = {0,};
        int i, ret;

        for (i = 0; i < print_reg_num; ++i) {
                ret = ln8000_read_reg(info, LN8000_REG_INT1_MSK + i, &regs[i]);
                if (IS_ERR_VALUE((unsigned long)ret)) {
                        ln_err("fail to read reg for print_regmap[%d]\n", i);
                        regs[i] = 0xFF;
                }
                sprintf(temp_buf + strlen(temp_buf), "0x%02X[0x%02X],", LN8000_REG_INT1_MSK + i, regs[i]);
                if (((i+1) % 10 == 0) || ((i+1) == print_reg_num)) {
                        ln_info("%s\n", temp_buf);
                        memset(temp_buf, 0x0, sizeof(temp_buf));
                }
        }
#if defined(CONFIG_LION_BENCH_TEST)
        ln8000_read_reg(info, 0x41, &regs[0]);
        ln8000_read_reg(info, 0x31, &regs[1]);
        ln8000_read_reg(info, 0x4A, &regs[2]);
        ln_info("dual-config:BC_OP_1[0x41]=0x%x, PRODUCT_ID[0x31]=0x%x, [0x4A0=0x%x\n", regs[0], regs[1], regs[2]);
#endif
}

/**
 * LN8000 device driver control routines
 */
static int ln8000_check_status(struct ln8000_info *info)
{
        u8 val[4];

        if (ln8000_bulk_read_reg(info, LN8000_REG_SYS_STS, val, 4) < 0) {
                return -EINVAL;
        }

        mutex_lock(&info->data_lock);

        info->vbat_regulated  = LN8000_STATUS(val[0], LN8000_MASK_VFLOAT_LOOP_STS);
        info->iin_regulated   = LN8000_STATUS(val[0], LN8000_MASK_IIN_LOOP_STS);
        info->pwr_status      = val[0] & (LN8000_MASK_BYPASS_ENABLED | LN8000_MASK_SWITCHING_ENABLED | \
                                      LN8000_MASK_STANDBY_STS | LN8000_MASK_SHUTDOWN_STS);
        info->tdie_fault      = LN8000_STATUS(val[1], LN8000_MASK_TEMP_MAX_STS);
        info->tdie_alarm      = LN8000_STATUS(val[1], LN8000_MASK_TEMP_REGULATION_STS);
        if (!info->pdata->tbat_mon_disable || !info->pdata->tbus_mon_disable) {
                info->tbus_tbat_fault = LN8000_STATUS(val[1], LN8000_MASK_NTC_SHUTDOWN_STS); //tbus or tbat
                info->tbus_tbat_alarm = LN8000_STATUS(val[1], LN8000_MASK_NTC_ALARM_STS);//tbus or tbat
        }
        info->iin_rc          = LN8000_STATUS(val[1], LN8000_MASK_REV_IIN_STS);

        info->wdt_fault  = LN8000_STATUS(val[2], LN8000_MASK_WATCHDOG_TIMER_STS);
        info->vbat_ov    = LN8000_STATUS(val[2], LN8000_MASK_VBAT_OV_STS);
        info->vac_unplug = LN8000_STATUS(val[2], LN8000_MASK_VAC_UNPLUG_STS);
        info->vac_ov     = LN8000_STATUS(val[2], LN8000_MASK_VAC_OV_STS);
        info->vbus_ov    = LN8000_STATUS(val[2], LN8000_MASK_VIN_OV_STS);
        info->volt_qual  = !(LN8000_STATUS(val[2], 0x7F));
        if (info->volt_qual == 1 && info->chg_en == 1) {
                info->volt_qual = !(LN8000_STATUS(val[3], 1 << 5));
                if (info->volt_qual == 0) {
                        ln_info("volt_fault_detected (volt_qual=%d\n", info->volt_qual);
                }
        }
        info->iin_oc     = LN8000_STATUS(val[3], LN8000_MASK_IIN_OC_DETECTED);

        mutex_unlock(&info->data_lock);

        return 0;
}

static void ln8000_irq_sleep(struct ln8000_info *info, int suspend)
{
        if (info->client->irq <= 0)
                return;

        if (suspend) {
                ln_info("disable/suspend IRQ\n");
                disable_irq(info->client->irq);
        } else {
                ln_info("enable/resume IRQ\n");
                enable_irq(info->client->irq);
        }
}

static void ln8000_soft_reset(struct ln8000_info *info)
{
        ln8000_write_reg(info, LN8000_REG_LION_CTRL, 0xC6);

        ln8000_irq_sleep(info, 1);

        ln_info("Trigger soft-reset\n");
        ln8000_update_reg(info, LN8000_REG_BC_OP_2, 0x1 << 0, 0x1 << 0);
        msleep(5 * 2);  /* ln8000 min wait time 5ms (after POR) */

        ln8000_irq_sleep(info, 0);
}

static void ln8000_update_opmode(struct ln8000_info *info)
{
        unsigned int op_mode;
        u8 val;

        /* chack mode status */
        ln8000_read_reg(info, LN8000_REG_SYS_STS, &val);

        if (val & LN8000_MASK_SHUTDOWN_STS) {
                op_mode = LN8000_OPMODE_STANDBY;
        } else if (val & LN8000_MASK_STANDBY_STS) {
                op_mode = LN8000_OPMODE_STANDBY;
        } else if (val & LN8000_MASK_SWITCHING_ENABLED) {
                op_mode = LN8000_OPMODE_SWITCHING;
        } else if (val & LN8000_MASK_BYPASS_ENABLED) {
                op_mode = LN8000_OPMODE_BYPASS;
        } else {
                op_mode = LN8000_OPMODE_UNKNOWN;
        }

        if (op_mode != info->op_mode) {
                /* IC already has been entered standby_mode, need to trigger standbt_en bit */
                if (op_mode == LN8000_OPMODE_STANDBY) {
                        ln8000_update_reg(info, LN8000_REG_SYS_CTRL, 1 << LN8000_BIT_STANDBY_EN, 1 << LN8000_BIT_STANDBY_EN);
                        ln_info("forced trigger standby_en\n");
                }
                ln_info("op_mode has been changed [%d]->[%d] (sys_st=0x%x)\n", info->op_mode, op_mode, val);
                info->op_mode = op_mode;
        }

        return;
}

static int ln8000_change_opmode(struct ln8000_info *info, unsigned int target_mode)
{
        int ret = 0;
        u8 val, msk = (0x1 << LN8000_BIT_STANDBY_EN | 0x1 << LN8000_BIT_EN_1TO1);

        switch(target_mode) {
        case LN8000_OPMODE_STANDBY:
                val = (1 << LN8000_BIT_STANDBY_EN);
                break;
        case LN8000_OPMODE_BYPASS:
                val = (0 << LN8000_BIT_STANDBY_EN) | (1 << LN8000_BIT_EN_1TO1);
                break;
        case LN8000_OPMODE_SWITCHING:
                val = (0 << LN8000_BIT_STANDBY_EN) | (0 << LN8000_BIT_EN_1TO1);
                break;
        default:
                ln_err("invalid index (target_mode=%d)\n", target_mode);
                return -EINVAL;
        }
        ret = ln8000_update_reg(info, LN8000_REG_SYS_CTRL, msk, val);
        if (IS_ERR_VALUE((unsigned long)ret)) {
                return -EINVAL;
        }
        ln_info("changed opmode [%d] -> [%d]\n", info->op_mode, target_mode);
        info->op_mode = target_mode;

        return 0;
}

static int ln8000_init_device(struct ln8000_info *info)
{
        unsigned int vbat_float;

        /* config default charging paramter by dt */
        vbat_float = info->pdata->bat_ovp_th * 100 / 102;   /* ovp thershold = v_float x 1.02 */
        vbat_float = (vbat_float /1000) * 1000;
        ln_info("bat_ovp_th=%d, vbat_float=%d\n", info->pdata->bat_ovp_th, vbat_float);
        ln8000_set_vbat_float(info, vbat_float);
        info->vbat_ovp_alarm_th = info->pdata->bat_ovp_alarm_th;
        ln8000_set_vac_ovp(info, info->pdata->bus_ovp_th);
        info->vin_ovp_alarm_th = info->pdata->bus_ovp_alarm_th;
        ln8000_set_iin_limit(info, info->pdata->bus_ocp_th - 700000);
        info->iin_ocp_alarm_th = info->pdata->bus_ocp_alarm_th;
        ln8000_set_ntc_alarm(info, info->pdata->ntc_alarm_cfg);

        ln8000_update_reg(info, LN8000_REG_REGULATION_CTRL, 0x3 << 2, LN8000_NTC_SHUTDOWN_CFG);
        ln8000_enable_auto_recovery(info, 0);

        /* config charging protection */
        ln8000_enable_vbat_ovp(info, !info->pdata->vbat_ovp_disable);
        ln8000_enable_vbat_regulation(info, !info->pdata->vbat_reg_disable);
        ln8000_enable_vbat_loop_int(info, !info->pdata->vbat_reg_disable);
        ln8000_enable_iin_ocp(info, !info->pdata->iin_ocp_disable);
        ln8000_enable_iin_regulation(info, !info->pdata->iin_reg_disable);
        ln8000_enable_iin_loop_int(info, !info->pdata->iin_reg_disable);
        ln8000_enable_tdie_regulation(info, !info->pdata->tdie_reg_disable);
        ln8000_enable_tdie_prot(info, info->pdata->tdie_prot_disable);
    	ln8000_enable_rcp(info, !info->pdata->revcurr_prot_disable);
        ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);

        /* wdt : disable, adc : shutdown mode */
        ln8000_enable_wdt(info, false);
        ln8000_set_adc_mode(info, ADC_SHUTDOWN_MODE);//disable before updating
        ln8000_set_adc_hib_delay(info, ADC_HIBERNATE_4S);
        ln8000_set_adc_ch(info, LN8000_ADC_CH_ALL, true);
        ln8000_enable_tbus_monitor(info, !info->pdata->tbus_mon_disable);//+enables ADC ch
        ln8000_enable_tbat_monitor(info, !info->pdata->tbat_mon_disable);//+enables ADC ch
        ln8000_set_adc_mode(info, ADC_AUTO_HIB_MODE);

        /* mark sw initialized (used CHARGE_CTRL bit:7) */
        ln8000_update_reg(info, LN8000_REG_CHARGE_CTRL, 0x1 << 7, 0x1 << 7);
        ln8000_write_reg(info, LN8000_REG_THRESHOLD_CTRL, 0x02);

        /* dual mode initialized */
        if (info->dev_role == LN_ROLE_MASTER) {
                ln8000_write_reg(info, LN8000_REG_LION_CTRL, 0xAA);
                // dual_function_en=1, dual_cfg=0, dual_lockout_en=1
                ln8000_update_reg(info, 0x41, 0x7, 0x5);
                // OVPFETDR_HIGH_IMP=0
                ln8000_update_reg(info, 0x31, 0x1 << 5, 0x0 << 5);
                ln8000_write_reg(info, LN8000_REG_LION_CTRL, 0x00);
        } else if (info->dev_role == LN_ROLE_SLAVE) {
                ln8000_write_reg(info, LN8000_REG_LION_CTRL, 0xAA);
                // dual_function_en=1, dual_cfg=1, dual_lockout_en=1
                ln8000_update_reg(info, 0x41, 0x7, 0x7);
                // OVPFETDR_HIGH_IMP=1
                ln8000_update_reg(info, 0x31, 0x1 << 5, 0x1 << 5);
                ln8000_write_reg(info, LN8000_REG_LION_CTRL, 0x00);
        }

	    ln8000_set_sw_freq(info, 0x9);  /* default switching frequency 890kHz */

        ln8000_print_regmap(info);

        ln_info(" done.\n");

        return 0;
}

static int ln8000_get_adc_data(struct ln8000_info *info, unsigned int ch, int *result)
{
        int ret;
        u8 sts[2];

        /* pause adc update */
        ret  = ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 1, 0x1 << 1);
        if (ret < 0) {
                ln_err("fail to update bit PAUSE_ADC_UPDATE:1 (ret=%d)\n", ret);
                return ret;
        }

        switch (ch) {
        case LN8000_ADC_CH_VOUT:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC04_STS, sts, 2);
                break;
        case LN8000_ADC_CH_VIN:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC03_STS, sts, 2);
                break;
        case LN8000_ADC_CH_VBAT:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC06_STS, sts, 2);
                break;
        case LN8000_ADC_CH_VAC:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC02_STS, sts, 2);
                break;
        case LN8000_ADC_CH_IIN:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC01_STS, sts, 2);
                break;
        case LN8000_ADC_CH_DIETEMP:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC07_STS, sts, 2);
                break;
        case LN8000_ADC_CH_TSBAT:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC08_STS, sts, 2);
                break;
        case LN8000_ADC_CH_TSBUS:
                ret = ln8000_bulk_read_reg(info, LN8000_REG_ADC09_STS, sts, 2);
                break;
        default:
                ln_err("invalid ch(%d)\n", ch);
                ret = -EINVAL;
                break;
        }

        /* resume adc update */
        ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1 << 1, 0x0 << 1);

        if (IS_ERR_VALUE((unsigned long)ret) == false) {
                ln8000_convert_adc_code(info, ch, sts, result);
        }

        return ret;
}

static int psy_chg_get_charging_enabled(struct ln8000_info *info)
{
        int enabled = 0;    /* disabled */

        if (info->chg_en) {
                ln8000_update_opmode(info);
                if (info->op_mode >= LN8000_OPMODE_BYPASS) {
                        enabled = 1;
                }
        }

        return enabled;
}

static int psy_chg_get_ti_alarm_status(struct ln8000_info *info)
{
        int alarm;
        unsigned int v_offset;
        bool bus_ovp, bus_ocp, bat_ovp;
        u8 val[4];

        ln8000_check_status(info);
        ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
        ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
        ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);

        bus_ovp = (info->vbus_uV > info->vin_ovp_alarm_th) ? 1 : 0;
        bus_ocp = (info->iin_uA > info->iin_ocp_alarm_th) ? 1 : 0;
        bat_ovp = (info->vbat_uV > info->vbat_ovp_alarm_th) ? 1 : 0;

        /* BAT alarm status not support (ovp/ocp/ucp) */
        alarm = ((bus_ovp << BUS_OVP_ALARM_SHIFT) |
                (bus_ocp << BUS_OCP_ALARM_SHIFT) |
                (bat_ovp << BAT_OVP_ALARM_SHIFT) |
                (info->tbus_tbat_alarm << BAT_THERM_ALARM_SHIFT) |
                (info->tbus_tbat_alarm << BUS_THERM_ALARM_SHIFT) |
                (info->tdie_alarm << DIE_THERM_ALARM_SHIFT));

        if (info->vbus_uV < (info->vbat_uV * 2)) {
                v_offset = 0;
        } else {
                v_offset = info->vbus_uV - (info->vbat_uV * 2);
        }

#if defined(LN8000_RCP_PATCH)
				/* after charging-enabled, When the input current rises above 400mA, it activates rcp. */
				if (info->chg_en && !(info->rcp_en)) {
					if (info->iin_uA > 200000 && v_offset > 300000) {
						ln8000_enable_rcp(info, 1);
						ln8000_enable_rcp_auto_recovery(info, 1);
						ln_info("enabled rcp\n");
					}
				}

				/* If an unplug event occurs when vbus voltage lower then iin(70mA) and v_offset(100mV), switch to standby mode. */
				if (info->chg_en && !(info->rcp_en)) {
					if (info->iin_uA < 70000 && v_offset < 100000) {
						ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);
						ln_info("forced change standby_mode for prevent reverse current\n");
						ln8000_enable_rcp(info, 1);
						ln8000_enable_rcp_auto_recovery(info, 1);
						info->chg_en = 0;
					}
				}
#endif

        ln8000_bulk_read_reg(info, LN8000_REG_SYS_STS, val, 4);
        ln_info("st:0x%x:0x%x:0x%x:0x%x alarm=0x%x, adc_vin=%d, adc_iin=%d, adc_vbat=%d, v_offset=%d\n",
                val[0], val[1], val[2], val[3], alarm, info->vbus_uV/1000, info->iin_uA/1000,
                info->vbat_uV/1000, v_offset/1000);

        return alarm;
}

static int psy_chg_get_it_bus_error_status(struct ln8000_info *info)
{
        ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);

        if (info->vbus_uV < 6000000) {
                return VBUS_ERROR_LOW;
	    } else if (info->vbus_uV > 13000000) {
                return VBUS_ERROR_HIGHT;
        }
        return VBUS_ERROR_NONE;
}

static int psy_chg_get_ti_fault_status(struct ln8000_info *info)
{
        int fault;

        ln8000_check_status(info);

        /* BAT ocp fault status not suppport */
        fault = ((info->vbat_ov << BAT_OVP_FAULT_SHIFT) |
                (info->vbus_ov << BUS_OVP_FAULT_SHIFT) |
                (info->iin_oc << BUS_OCP_FAULT_SHIFT) |
                (info->tbus_tbat_fault << BAT_THERM_FAULT_SHIFT) |
                (info->tbus_tbat_fault << BUS_THERM_FAULT_SHIFT) |
                (info->tdie_fault << DIE_THERM_FAULT_SHIFT));

        if (info->volt_qual == 0) {
                fault  |= ((1 << BAT_OVP_FAULT_SHIFT) |
                (1 << BUS_OVP_FAULT_SHIFT));
        }

        if (fault) {
                ln_info("fault=0x%x\n", fault);
        }

        return fault;
}

static int psy_chg_set_charging_enable(struct ln8000_info *info, int val)
{
        int op_mode;

	/* ignore duplicate request command*/
    ln_info("info->chg_en=%d,val=%d\n",info->chg_en,val);
	if (info->chg_en == val) 
		return 0;

        if (val) {
                ln_info("start charging\n");
                op_mode = LN8000_OPMODE_SWITCHING;
#if defined(LN8000_RCP_PATCH)
                /* when the start-up to charging, we need to disabled rcp. */
                ln8000_enable_rcp(info, 0);
                ln8000_enable_rcp_auto_recovery(info, 0);
#endif
        } else {
                ln_info("stop charging\n");
                op_mode = LN8000_OPMODE_STANDBY;
#if defined(LN8000_RCP_PATCH)
                ln8000_enable_rcp(info, 1);
                ln8000_enable_rcp_auto_recovery(info, 1);
#endif
        }

        ln8000_change_opmode(info, op_mode);
        msleep(10);
        ln8000_update_opmode(info);

        info->chg_en = val;
	if (val) {
		ln8000_print_regmap(info);
	}

        ln_info("op_mode=%d\n", info->op_mode);

        return 0;
}

static int psy_chg_set_present(struct ln8000_info *info, int val)
{
        bool usb_present = (bool)val;

  //      if (usb_present != info->usb_present) {
                ln_info("changed usb_present [%d] -> [%d]\n", info->usb_present, usb_present);
                if (usb_present) {
                        ln8000_init_device(info);
                }
                info->usb_present = usb_present;
   //     }

        return 0;
}

#if 0
static int psy_chg_set_bus_protection_for_qc3(struct ln8000_info *info, int hvdcp3_type)
{
        ln_info("hvdcp3_type: %d\n", hvdcp3_type);

        if (hvdcp3_type == HVDCP3_CLASSA_18W) {
                ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC);
                info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC;
                ln8000_set_iin_limit(info, BUS_OCP_FOR_QC_CLASS_A - 700000);
                info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC_CLASS_A;
        } else if (hvdcp3_type == HVDCP3_CLASSB_27W) {
                ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC);
                info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC;
                ln8000_set_iin_limit(info, BUS_OCP_FOR_QC_CLASS_B - 700000);
                info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC_CLASS_B;
        } else if (hvdcp3_type == HVDCP3_P_CLASSA_18W) {
                ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC35);
                info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC35;
                ln8000_set_iin_limit(info, BUS_OCP_FOR_QC35_CLASS_A_P - 700000);
                info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC35_CLASS_A_P;
        } else if (hvdcp3_type == HVDCP3_P_CLASSB_27W) {
                ln8000_set_vac_ovp(info, BUS_OVP_FOR_QC35);
                info->vin_ovp_alarm_th = BUS_OVP_ALARM_FOR_QC35;
                ln8000_set_iin_limit(info, BUS_OCP_FOR_QC35_CLASS_A_P - 700000);
                info->iin_ocp_alarm_th = BUS_OCP_ALARM_FOR_QC35_CLASS_A_P;
        } else {
                ln8000_set_vac_ovp(info, info->pdata->bus_ovp_th);
                info->vin_ovp_alarm_th = info->pdata->bus_ovp_alarm_th;
                ln8000_set_iin_limit(info, info->pdata->bus_ocp_th - 700000);
                info->iin_ocp_alarm_th = info->pdata->bus_ocp_alarm_th;
        }

        ln8000_print_regmap(info);

        return 0;
}
#endif

/**
 * Support power_supply platform for charger block.
 * propertis are compatible by Xiaomi platform
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
static int ln_iio_write_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int val1,
		int val2, long mask)
{
	struct ln8000_info *info = iio_priv(indio_dev);
	int rc = 0;

	switch (chan->channel) {
	case PSY_IIO_CHARGING_ENABLED:
		ln_info("POWER_SUPPLY_PROP_CHARGING_ENABLED: %s\n",
				val1 ? "enable" : "disable");
		rc = psy_chg_set_charging_enable(info, val1);
		break;
	case PSY_IIO_PRESENT:
        rc = psy_chg_set_present(info, !!val1);
		break;
	default:
		pr_debug("Unsupported LN8000 IIO chan %d\n", chan->channel);
		rc = -EINVAL;
		break;
	}

	if (rc < 0)
		pr_err("Couldn't write IIO channel %d, rc = %d\n",
			chan->channel, rc);

	return rc;
}

static int ln_iio_read_raw(struct iio_dev *indio_dev,
		struct iio_chan_spec const *chan, int *val1,
		int *val2, long mask)
{
	struct ln8000_info *info = iio_priv(indio_dev);
	int ret = 0;
	*val1 = 0;

	switch (chan->channel) {
	case PSY_IIO_CHARGING_ENABLED:
		*val1 = psy_chg_get_charging_enabled(info);
		break;
	case PSY_IIO_STATUS:
		*val1 = 0;
		break;
	case PSY_IIO_PRESENT:
		*val1 = info->usb_present;
		break;
	case PSY_IIO_SC_BATTERY_PRESENT:
        ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
        if (info->vbat_uV > LN8000_ADC_VBAT_MIN) {
                info->batt_present = 1;    /* detected battery */
        } else {
                info->batt_present = 0;    /* non-detected battery */
        }
		*val1 = info->batt_present;
		break;
	case PSY_IIO_SC_VBUS_PRESENT:
        ret = ln8000_check_status(info);
       	*val1 = !(info->vac_unplug);
		break;
	case PSY_IIO_SC_BATTERY_VOLTAGE:
        ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
        *val1 = info->vbat_uV/1000;
		break;
	case PSY_IIO_SC_BATTERY_CURRENT:
        ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
        *val1 = (info->iin_uA * 2)/1000;         /* return to IBUS_ADC x 2 */
		break;
	case PSY_IIO_SC_BATTERY_TEMPERATURE:
        if (info->pdata->tbat_mon_disable) {
                *val1 = 0;
        } else {
                ln8000_get_adc_data(info, LN8000_ADC_CH_TSBAT, &info->tbat_uV);
                *val1 = info->tbat_uV;
                ln_info("ti_battery_temperature: adc_tbat=%d\n", *val1);
        }
		break;
	case PSY_IIO_SC_BUS_VOLTAGE:
        ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
        *val1 = info->vbus_uV/1000;
		break;
	case PSY_IIO_SC_BUS_CURRENT:
        ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
        *val1 = info->iin_uA/1000;
		break;
	case PSY_IIO_SC_BUS_TEMPERATURE:
        if (info->pdata->tbus_mon_disable) {
                *val1 = 0;
        } else {
                ln8000_get_adc_data(info, LN8000_ADC_CH_TSBUS, &info->tbus_uV);
                *val1 = info->tbus_uV;
                ln_info("ti_bus_temperature: adc_tbus=%d\n", *val1);
        }
		break;
	case PSY_IIO_SC_DIE_TEMPERATURE:
        ln8000_get_adc_data(info, LN8000_ADC_CH_DIETEMP, &info->tdie_dC);
        *val1 = info->tdie_dC;
        ln_info("ti_die_temperature: adc_tdie=%d\n", *val1);
		break;

	case PSY_IIO_SC_ALARM_STATUS:
        *val1 = psy_chg_get_ti_alarm_status(info);
		break;
	case PSY_IIO_SC_FAULT_STATUS:
        *val1 = psy_chg_get_ti_fault_status(info);
		break;
	case PSY_IIO_SC_VBUS_ERROR_STATUS:
        *val1 = psy_chg_get_it_bus_error_status(info);
		break;
	default:
		pr_debug("Unsupported QG IIO chan %d\n", chan->channel);
		ret = -EINVAL;
		break;
	}

	if (ret < 0) {
		pr_err("Couldn't read IIO channel %d, ret = %d\n",
			chan->channel, ret);
		return ret;
	}

	return IIO_VAL_INT;
}

static int ln_iio_of_xlate(struct iio_dev *indio_dev,
				const struct of_phandle_args *iiospec)
{
	struct ln8000_info *chip = iio_priv(indio_dev);
	struct iio_chan_spec *iio_chan = chip->iio_chan;
	int i;

	for (i = 0; i < ARRAY_SIZE(ln8000_iio_psy_channels);
					i++, iio_chan++)
		if (iio_chan->channel == iiospec->args[0])
			return i;

	return -EINVAL;
}

static const struct iio_info ln_iio_info = {
	.read_raw	= ln_iio_read_raw,
	.write_raw	= ln_iio_write_raw,
	.of_xlate	= ln_iio_of_xlate,
};

static int ln_init_iio_psy(struct ln8000_info *chip)
{
	struct iio_dev *indio_dev = chip->indio_dev;
	struct iio_chan_spec *chan;
	int num_iio_channels = ARRAY_SIZE(ln8000_iio_psy_channels);
	int rc, i;

	pr_err("LN8000 ln_init_iio_psy start\n");
	chip->iio_chan = devm_kcalloc(chip->dev, num_iio_channels,
				sizeof(*chip->iio_chan), GFP_KERNEL);
	if (!chip->iio_chan)
		return -ENOMEM;

	chip->int_iio_chans = devm_kcalloc(chip->dev,
				num_iio_channels,
				sizeof(*chip->int_iio_chans),
				GFP_KERNEL);
	if (!chip->int_iio_chans)
		return -ENOMEM;

	indio_dev->info = &ln_iio_info;
	indio_dev->dev.parent = chip->dev;
	indio_dev->dev.of_node = chip->dev->of_node;
	indio_dev->modes = INDIO_DIRECT_MODE;
	indio_dev->channels = chip->iio_chan;
	indio_dev->num_channels = num_iio_channels;
	if (chip->dev_role == LN_ROLE_MASTER) {
		indio_dev->name = "ln8000-master";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = ln8000_iio_psy_channels[i].channel_num;
			chan->type = ln8000_iio_psy_channels[i].type;
			chan->datasheet_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				ln8000_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				ln8000_iio_psy_channels[i].info_mask;
		}
	} else if (chip->dev_role == LN_ROLE_SLAVE) {
		indio_dev->name = "ln8000-slave";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = ln8000_slave_iio_psy_channels[i].channel_num;
			chan->type = ln8000_slave_iio_psy_channels[i].type;
			chan->datasheet_name =
				ln8000_slave_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				ln8000_slave_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				ln8000_slave_iio_psy_channels[i].info_mask;
		}
	} else {
		indio_dev->name = "ln8000-standalone-2";
		for (i = 0; i < num_iio_channels; i++) {
			chip->int_iio_chans[i].indio_dev = indio_dev;
			chan = &chip->iio_chan[i];
			chip->int_iio_chans[i].channel = chan;
			chan->address = i;
			chan->channel = ln8000_slave_iio_psy_channels[i].channel_num;
			chan->type = ln8000_slave_iio_psy_channels[i].type;
			chan->datasheet_name =
				ln8000_slave_iio_psy_channels[i].datasheet_name;
			chan->extend_name =
				ln8000_slave_iio_psy_channels[i].datasheet_name;
			chan->info_mask_separate =
				ln8000_slave_iio_psy_channels[i].info_mask;
		}
	}

	rc = devm_iio_device_register(chip->dev, indio_dev);
	if (rc)
		pr_err("Failed to register LN8000 IIO device, rc=%d\n", rc);

	pr_err("LN8000 IIO device, rc=%d\n", rc);
	return rc;
}
#else
/* grab programmed battery float voltage (uV) */
static int ln8000_get_vbat_float(struct ln8000_info *info)
{
        int ret;
        u8 val;

        ret = ln8000_read_reg(info, LN8000_REG_V_FLOAT_CTRL, &val);
        if (ret < 0)
                return ret;

        return ((val & 0xFF) * LN8000_VBAT_FLOAT_LSB  + LN8000_VBAT_FLOAT_MIN);//uV
}

/* grab programmed input current limit (uA) */
static int ln8000_get_iin_limit(struct ln8000_info *info)
{
        int ret, iin;
        u8 val;

        ret = ln8000_read_reg(info, LN8000_REG_IIN_CTRL, &val);
        if (ret < 0)
                return ret;

        iin = ((val & 0x7F) * LN8000_IIN_CFG_LSB);

        if (iin < LN8000_IIN_CFG_MIN) {
                iin = LN8000_IIN_CFG_MIN;
        }

        return iin;
}

static int ln8000_charger_get_property(struct power_supply *psy,
                                       enum power_supply_property prop,
                                       union power_supply_propval *val)
{
        struct ln8000_info *info = power_supply_get_drvdata(psy);

        switch (prop) {
        case POWER_SUPPLY_PROP_CHARGING_ENABLED:
                val->intval = psy_chg_get_charging_enabled(info);
                break;
        case POWER_SUPPLY_PROP_STATUS:
                val->intval = 0;
                break;
        case POWER_SUPPLY_PROP_PRESENT:
                val->intval = info->usb_present;
                break;
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
                val->intval = ln8000_get_vbat_float(info);
                break;
        case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
                val->intval = ln8000_get_iin_limit(info);
                break;
        case POWER_SUPPLY_PROP_SC_BATTERY_PRESENT:
                ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
                if (info->vbat_uV > LN8000_ADC_VBAT_MIN) {
                        val->intval = 1;    /* detected battery */
                } else {
                        val->intval = 0;    /* non-detected battery */
                }
                break;
        case POWER_SUPPLY_PROP_SC_VBUS_PRESENT:
                ln8000_check_status(info);
                val->intval = !(info->vac_unplug);
                break;
        case POWER_SUPPLY_PROP_SC_BATTERY_VOLTAGE:
                ln8000_get_adc_data(info, LN8000_ADC_CH_VBAT, &info->vbat_uV);
                val->intval = info->vbat_uV/1000;
                break;
        case POWER_SUPPLY_PROP_SC_BATTERY_CURRENT:  /* ln8000 not support IBAT_ADC */
                ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
                val->intval = (info->iin_uA * 2)/1000;         /* return to IBUS_ADC x 2 */
                break;
        case POWER_SUPPLY_PROP_SC_BATTERY_TEMPERATURE:
                if (info->pdata->tbat_mon_disable) {
                        val->intval = 0;
                } else {
                        ln8000_get_adc_data(info, LN8000_ADC_CH_TSBAT, &info->tbat_uV);
                        val->intval = info->tbat_uV;
                        ln_info("ti_battery_temperature: adc_tbat=%d\n", val->intval);
                }
                break;
        case POWER_SUPPLY_PROP_SC_BUS_VOLTAGE:
                ln8000_get_adc_data(info, LN8000_ADC_CH_VIN, &info->vbus_uV);
                val->intval = info->vbus_uV/1000;
                break;
        case POWER_SUPPLY_PROP_SC_BUS_CURRENT:
                ln8000_get_adc_data(info, LN8000_ADC_CH_IIN, &info->iin_uA);
                val->intval = info->iin_uA/1000;
                break;
        case POWER_SUPPLY_PROP_SC_BUS_TEMPERATURE:
                if (info->pdata->tbus_mon_disable) {
                        val->intval = 0;
                } else {
                        ln8000_get_adc_data(info, LN8000_ADC_CH_TSBUS, &info->tbus_uV);
                        val->intval = info->tbus_uV;
                        ln_info("ti_bus_temperature: adc_tbus=%d\n", val->intval);
                }
                break;
        case POWER_SUPPLY_PROP_SC_DIE_TEMPERATURE:
                ln8000_get_adc_data(info, LN8000_ADC_CH_DIETEMP, &info->tdie_dC);
                val->intval = info->tdie_dC;
                ln_info("ti_die_temperature: adc_tdie=%d\n", val->intval);
                break;
        case POWER_SUPPLY_PROP_SC_ALARM_STATUS:
                val->intval = psy_chg_get_ti_alarm_status(info);
                break;
        case POWER_SUPPLY_PROP_SC_FAULT_STATUS:
                val->intval = psy_chg_get_ti_fault_status(info);
                break;
#if 0
        case POWER_SUPPLY_PROP_SC_REG_STATUS:
                ln8000_check_status(info);
                val->intval = ((info->vbat_regulated << VBAT_REG_STATUS_SHIFT) |
                /* ln8000 not support ibat_reg, we are can be ibus_reg */
                (info->iin_regulated << IBAT_REG_STATUS_SHIFT));
                if (val->intval) {
                        ln_info("ti_reg_status: intavl=0x%x\n", val->intval);
                }
                break;
        case POWER_SUPPLY_PROP_BQ_CHARGE_DONE:
                val->intval = bq_charge_done;
                break;
        case POWER_SUPPLY_PROP_HV_CHARGE_ENABLED:
                val->intval = hv_charge_enable;
                break;
#endif
        case POWER_SUPPLY_PROP_SC_VBUS_ERROR_STATUS:
                val->intval = psy_chg_get_it_bus_error_status(info);
                break;
#if 0
        case POWER_SUPPLY_PROP_SC_CHARGE_MODE:
                if (info->op_mode == LN8000_OPMODE_SWITCHING) {
                        val->intval = 0; /* DIV2 */
                } else {
                        val->intval = -1; /* NOT_SUPPORT */
                }
                break;

        case POWER_SUPPLY_PROP_SC_BYPASS_MODE_ENABLED:
                val->intval = 0;
                break;
#endif
        case POWER_SUPPLY_PROP_MODEL_NAME:
                val->strval = ln8000_dev_name[info->dev_role];
                break;
#if defined(CONFIG_LION_BENCH_TEST)
        case POWER_SUPPLY_PROP_LION_ADC_VALUES:
                val->intval = psy_chg_get_ti_alarm_status(info);
                break;
        case POWER_SUPPLY_PROP_LION_CHG_STATUS:
                psy_chg_get_ti_alarm_status(info);
                ln8000_update_opmode(info);
                if (info->op_mode == LN8000_OPMODE_STANDBY) {
                        val->intval = 0;
                } else {
                        val->intval = 1;
                }
                break;
        case POWER_SUPPLY_PROP_LION_REG_DUMP:
                ln8000_print_regmap(info);
                break;
#endif
        default:
                return -EINVAL;
        }

        return 0;
}

static int ln8000_charger_set_property(struct power_supply *psy,
                                       enum power_supply_property prop,
                                       const union power_supply_propval *val)
{
        struct ln8000_info *info = power_supply_get_drvdata(psy);
        int ret = 0;

        switch (prop) {
        case POWER_SUPPLY_PROP_CHARGING_ENABLED:
                ret = psy_chg_set_charging_enable(info, val->intval);
                break;
        case POWER_SUPPLY_PROP_PRESENT:
                ret = psy_chg_set_present(info, val->intval);
                break;
#if 0				
        case POWER_SUPPLY_PROP_SC_SET_BUS_PROTECTION_FOR_QC3:
                ret = psy_chg_set_bus_protection_for_qc3(info, val->intval);
                break;
        case POWER_SUPPLY_PROP_BQ_CHARGE_DONE:
                bq_charge_done = val->intval;
                break;
        case POWER_SUPPLY_PROP_HV_CHARGE_ENABLED:
                hv_charge_enable = val->intval;
                break;
        case POWER_SUPPLY_PROP_SC_CHARGE_MODE:
                ln_info("not support ti_charge_mode yet\n");
                break;
        case POWER_SUPPLY_PROP_SC_BYPASS_MODE_ENABLED:
                ln_info("not support ti_bypass_mode_enabled yet\n");
                break; 
#endif
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
                ret = ln8000_set_vbat_float(info, val->intval);
                break;
        case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
                ret = ln8000_set_iin_limit(info, val->intval);
                break;
        default:
                ret = -EINVAL;
                break;
        }

        return ret;
}

static int ln8000_charger_is_writeable(struct power_supply *psy,
                                       enum power_supply_property prop) {
        int ret;

        switch (prop) {
        case POWER_SUPPLY_PROP_PRESENT:
        case POWER_SUPPLY_PROP_CHARGING_ENABLED:
#if 0
        case POWER_SUPPLY_PROP_HV_CHARGE_ENABLED:
        case POWER_SUPPLY_PROP_BQ_CHARGE_DONE:
        case POWER_SUPPLY_PROP_SC_CHARGE_MODE:
        case POWER_SUPPLY_PROP_SC_BYPASS_MODE_ENABLED:
#endif
        case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE:
        case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
        //case POWER_SUPPLY_PROP_SC_SET_BUS_PROTECTION_FOR_QC3:
                ret = 1;
                break;
        default:
                ret = 0;
                break;
        }

        return ret;
}

static enum power_supply_property ln8000_charger_props[] = {
        POWER_SUPPLY_PROP_PRESENT,
        POWER_SUPPLY_PROP_CHARGING_ENABLED,
        POWER_SUPPLY_PROP_STATUS,
        POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
        POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
        /* support TI extended propertis */
	    POWER_SUPPLY_PROP_SC_VBUS_ERROR_STATUS,
#if 0
	    POWER_SUPPLY_PROP_SC_CHARGE_MODE,
	    POWER_SUPPLY_PROP_SC_BYPASS_MODE_ENABLED,
#endif
        POWER_SUPPLY_PROP_SC_BATTERY_PRESENT,
        POWER_SUPPLY_PROP_SC_VBUS_PRESENT,
        POWER_SUPPLY_PROP_SC_BATTERY_VOLTAGE,
        POWER_SUPPLY_PROP_SC_BATTERY_CURRENT,
        POWER_SUPPLY_PROP_SC_BATTERY_TEMPERATURE,
        POWER_SUPPLY_PROP_SC_BUS_VOLTAGE,
        POWER_SUPPLY_PROP_SC_BUS_CURRENT,
        POWER_SUPPLY_PROP_SC_BUS_TEMPERATURE,
        POWER_SUPPLY_PROP_SC_DIE_TEMPERATURE,
        POWER_SUPPLY_PROP_SC_ALARM_STATUS,
        POWER_SUPPLY_PROP_SC_FAULT_STATUS,
#if 0
		POWER_SUPPLY_PROP_SC_REG_STATUS,
        POWER_SUPPLY_PROP_SC_SET_BUS_PROTECTION_FOR_QC3,
        POWER_SUPPLY_PROP_BQ_CHARGE_DONE,
        POWER_SUPPLY_PROP_HV_CHARGE_ENABLED,
#endif
        POWER_SUPPLY_PROP_MODEL_NAME,
#if defined(CONFIG_LION_BENCH_TEST)
        POWER_SUPPLY_PROP_LION_ADC_VALUES,
        POWER_SUPPLY_PROP_LION_CHG_STATUS,
        POWER_SUPPLY_PROP_LION_REG_DUMP,
#endif
};

static int ln8000_psy_register(struct ln8000_info *info)
{
        info->psy_cfg.drv_data = info;
        info->psy_cfg.of_node = info->client->dev.of_node;
        info->psy_desc.name = ln8000_dev_name[info->dev_role];
        info->psy_desc.type = POWER_SUPPLY_TYPE_MAINS;
        info->psy_desc.properties = ln8000_charger_props;
        info->psy_desc.num_properties = ARRAY_SIZE(ln8000_charger_props);
        info->psy_desc.get_property = ln8000_charger_get_property;
        info->psy_desc.set_property = ln8000_charger_set_property;
        info->psy_desc.property_is_writeable = ln8000_charger_is_writeable;
        info->psy_chg = power_supply_register(&info->client->dev, &info->psy_desc, &info->psy_cfg);
        if (IS_ERR(info->psy_chg)) {
                ln_err("failed to register power supply\n");
                return PTR_ERR(info->psy_chg);
        }

        ln_info("successfully registered power supply(psy->desc->name=%s)\n", info->psy_chg->desc->name);

        return 0;
}

#endif

static int read_reg(void *data, u64 *val)
{
        struct ln8000_info *info = data;
        int ret;
        u8 temp;

        ret = ln8000_read_reg(info, info->debug_address, &temp);
        if (ret) {
                ln_err("Unable to read reg(0x%02X), ret=%d\n", info->debug_address, ret);
                return -EAGAIN;
        }
        *val = (u64)temp;
        return 0;
}

static int write_reg(void *data, u64 val)
{
        struct ln8000_info *info = data;
        int ret;
        u8 temp = (u8) val;

        ret = ln8000_write_reg(info, info->debug_address, temp);
        if (ret) {
                ln_err("Unable to write reg(0x%02X), data(0x%02X), ret=%d\n",
                info->debug_address, temp, ret);
                return -EAGAIN;
        }
        return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(register_debug_ops, read_reg, write_reg, "0x%02llX\n");

static int ln8000_create_debugfs_entries(struct ln8000_info *info)
{
        struct dentry *ent;

        info->debug_root = debugfs_create_dir(ln8000_dev_name[info->dev_role], NULL);
        if (!info->debug_root) {
                ln_err("unable to create debug dir\n");
                return -ENOENT;
        } else {
                ent = debugfs_create_x32("address", S_IFREG | S_IWUSR | S_IRUGO, info->debug_root, &(info->debug_address));
                if (!ent) {
                        ln_err("unable to create address debug file\n");
                        return -ENOENT;
                }
                ent = debugfs_create_file("data", S_IFREG | S_IWUSR | S_IRUGO, info->debug_root, info, &register_debug_ops);
                if (!ent) {
                        ln_err("unable to create data debug file\n");
                        return -ENOENT;
                }
        }

        return 0;
}

/**
 * Support IRQ interface
 */
#ifdef CONFIG_WT_QGKI
static int ln8000_read_int_value(struct ln8000_info *info, u8 *reg_val)
{
        int ret;

        /* pause INT updates */
        ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1, 0x1);
        mdelay(1);

        ret = ln8000_read_reg(info, LN8000_REG_INT1, reg_val);

        /* resume INT updates */
        ln8000_update_reg(info, LN8000_REG_TIMER_CTRL, 0x1, 0x0);

        return ret;
}
#endif

static irqreturn_t ln8000_interrupt_handler(int irq, void *data)
{
#ifdef CONFIG_WT_QGKI
        struct ln8000_info *info = data;
        u8 int_reg, int_msk;
        u8 masked_int;
        int ret;
        ln_err("ln8000_interrupt_handler enter!\n");
        ret = ln8000_read_int_value(info, &int_reg);
        if (IS_ERR_VALUE((unsigned long)ret)) {
                ln_err("fail to read INT reg (ret=%d)\n", ret);
                return IRQ_NONE;
        }
        ln8000_read_reg(info, LN8000_REG_INT1_MSK, &int_msk);
        masked_int = int_reg & ~int_msk;

        ln_info("int_reg=0x%x, int_msk=0x%x, masked_int=0x%x\n", int_reg, int_msk, masked_int);

        ln8000_print_regmap(info);
        LN8000_BIT_CHECK(masked_int, 7, "(INT) FAULT_INT");
        LN8000_BIT_CHECK(masked_int, 6, "(INT) NTC_PROT_INT");
        LN8000_BIT_CHECK(masked_int, 5, "(INT) CHARGE_PHASE_INT");
        LN8000_BIT_CHECK(masked_int, 4, "(INT) MODE_INT");
        LN8000_BIT_CHECK(masked_int, 3, "(INT) REV_CURR_INT");
        LN8000_BIT_CHECK(masked_int, 2, "(INT) TEMP_INT");
        LN8000_BIT_CHECK(masked_int, 1, "(INT) ADC_DONE_INT");
        LN8000_BIT_CHECK(masked_int, 0, "(INT) TIMER_INT");
        ln8000_check_status(info);

        if (masked_int & LN8000_MASK_FAULT_INT) { /* FAULT_INT */
                if (info->volt_qual) {
                        ln_info("connected to power_supplier\n");
                } else {
                        ln_info("FAULT_INT has occurred\n");
                }
        }
        if (masked_int & LN8000_MASK_NTC_PROT_INT) { /* NTC_PROT_INT */
                ln_info("NTC_PROT_INT has occurred(ntc_fault=%d, ntc_alarm=%d)\n",
                info->tbus_tbat_fault, info->tbus_tbat_alarm);
        }
        if (masked_int & LN8000_MASK_CHARGE_PHASE_INT) { /* CHARGE_PHASE_INT */
                if (info->vbat_regulated) {
                        ln_info("CHARGE_PHASE_INT: VFLOAT regulated\n");
                } else if (info->iin_regulated) {
                        ln_info("CHARGE_PHASE_INT: IIN regulated\n");
                }
        }
        if (masked_int & LN8000_MASK_MODE_INT) { /* MODE_INT */
                switch(info->pwr_status) {
                case LN8000_MASK_BYPASS_ENABLED:
                        ln_info("MODE_INT: device in BYPASS mode\n");
                        break;
                case LN8000_MASK_SWITCHING_ENABLED:
                        ln_info("MODE_INT: device in SWITCHING mode\n");
                        break;
                case LN8000_MASK_STANDBY_STS:
                        ln_info("MODE_INT: device in STANDBY mode\n");
                        break;
                case LN8000_MASK_SHUTDOWN_STS:
                        ln_info("MODE_INT: device in SHUTDOWN mode\n");
                        break;
                default:
                        ln_info("MODE_INT: device in  unknown mode\n");
                        break;
                }
        }
        if (masked_int & LN8000_MASK_TEMP_INT) { /* TEMP_INT */
                ln_info("TEMP_INT has occurred(tdie_fault=%d, tdie_alarm=%d)\n",
                info->tdie_fault, info->tdie_alarm);
        }
                if (masked_int & LN8000_MASK_TIMER_INT) { /* TIMER_INT */
                ln_info("Watchdog timer has expired(wdt_fault=%d)\n", info->wdt_fault);
        }
#endif
        return IRQ_HANDLED;
}

static int ln8000_irq_init(struct ln8000_info *info)
{
#ifdef CONFIG_WT_QGKI
        const struct ln8000_platform_data *pdata = info->pdata;
        int ret;
        u8 mask, int_reg;

        if (info->pdata->irq_gpio) {
                info->client->irq = gpiod_to_irq(pdata->irq_gpio);
                if (info->client->irq < 0) {
                        ln_err("fail to get irq from gpio(irq_gpio=%p)\n", pdata->irq_gpio)
                        info->client->irq = 0;
                        return -EINVAL;
                }
                ln_info("mapped GPIO to irq (%d)\n", info->client->irq);
        }

        /* interrupt mask setting */
        mask = LN8000_MASK_ADC_DONE_INT | LN8000_MASK_TIMER_INT | LN8000_MASK_MODE_INT | LN8000_MASK_REV_CURR_INT;
        if (info->pdata->tdie_prot_disable && info->pdata->tdie_reg_disable)
                mask |= LN8000_MASK_TEMP_INT;
        if (info->pdata->iin_reg_disable && info->pdata->vbat_reg_disable)
                mask |= LN8000_MASK_CHARGE_PHASE_INT;
        if (info->pdata->tbat_mon_disable && info->pdata->tbus_mon_disable)
                mask |= LN8000_MASK_NTC_PROT_INT;
        ln8000_write_reg(info, LN8000_REG_INT1_MSK, mask);
        /* read clear int_reg */
        ret = ln8000_read_int_value(info, &int_reg);
        if (IS_ERR_VALUE((unsigned long)ret)) {
                ln_err("fail to read INT reg (ret=%d)\n", ret);
                return IRQ_NONE;
        }
        ln_info("int1_msk=0x%x\n", mask);
#endif
        return 0;
}

static void determine_initial_status(struct ln8000_info *info)
{
        if (info->client->irq)
                ln8000_interrupt_handler(info->client->irq, info);
}


static const struct of_device_id ln8000_dt_match[] = {
        {
                .compatible = "lionsemi,ln8000_2",
                .data = (void *) LN_ROLE_STANDALONE
        },
        {
                .compatible = "lionsemi,ln8000-master",
                .data = (void *) LN_ROLE_MASTER
        },
        {
                .compatible = "lionsemi,ln8000-slave",
                .data = (void *) LN_ROLE_SLAVE
        },
        { },
};
MODULE_DEVICE_TABLE(of, ln8000_dt_match);

static const struct i2c_device_id ln8000_id[] = {
        { "ln8000-standalone-2", LN_ROLE_STANDALONE },
        { "ln8000-master", LN_ROLE_MASTER },
        { "ln8000-slave", LN_ROLE_SLAVE },
        { }
};

static int ln8000_get_dev_role(struct i2c_client *client)
{
        const struct of_device_id *of_id;

        of_id = of_match_device(of_match_ptr(ln8000_dt_match), &client->dev);
        if (of_id == NULL) {
                dev_err(&client->dev,"%s: fail to matched of_device_id\n", __func__);
                return -EINVAL;
        }

        dev_info(&client->dev,"%s: matched to %s\n", __func__, of_id->compatible);

        return (int)of_id->data;
}

static int ln8000_parse_dt(struct ln8000_info *info)
{
        struct device *dev = info->dev;
        struct ln8000_platform_data *pdata = info->pdata;
        struct device_node *np = dev->of_node;
        u32 prop;
        int ret;

        if (np == NULL)
                return -EINVAL;

#ifdef CONFIG_WT_QGKI
        /* first we try to used client->irq */
        if (info->client->irq == 0) {
                /* sencond we try to used irq_index info */
                ret = of_property_read_u32(np, "ln8000_charger,irq_index", &prop);
                if (ret < 0) {
                        ln_err("can't get irq_index(ret=%d)\n", ret);
                        prop = 0;
                }
                info->client->irq = prop;
        }
        ln_info("info->client->irq=%d\n", info->client->irq);

        if (!info->client->irq) {
                pdata->irq_gpio = gpiod_get(dev, "irqb", GPIOD_IN);
                if (LN8000_USE_GPIO(pdata)) {
                        ln_info("[gpio] found valid GPIO/IRQ delnriptor\n");
                } else {
                        ln_info("[gpio] unspecified or invalid GPIO descriptor\n");
                        pdata->irq_gpio = 0;
                }
        }
#endif

        /* device configuration */
        ret = of_property_read_u32(np, "ln8000_charger,bat-ovp-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bat_ovp_th, (prop*1000/*uV*/), LN8000_BAT_OVP_DEFAULT);
        ret = of_property_read_u32(np, "ln8000_charger,bat-ovp-alarm-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bat_ovp_alarm_th, (prop*1000/*uV*/), 0);
        ret = of_property_read_u32(np, "ln8000_charger,bus-ovp-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bus_ovp_th, (prop*1000/*uV*/), LN8000_BUS_OVP_DEFAULT);
        ret = of_property_read_u32(np, "ln8000_charger,bus-ovp-alarm-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bus_ovp_alarm_th, (prop*1000/*uA*/), 0);
        ret = of_property_read_u32(np, "ln8000_charger,bus-ocp-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bus_ocp_th, (prop*1000/*uA*/), LN8000_BUS_OCP_DEFAULT);
        ret = of_property_read_u32(np, "ln8000_charger,bus-ocp-alarm-threshold", &prop);
        LN8000_PARSE_PROP(ret, pdata, bus_ocp_alarm_th, (prop*1000/*uA*/), 0);
        ret = of_property_read_u32(np, "ln8000_charger,ntc-alarm-cfg", &prop);
        LN8000_PARSE_PROP(ret, pdata, ntc_alarm_cfg, prop, LN8000_NTC_ALARM_CFG_DEFAULT);

        /* protection/alarm disable (defaults to enable) */
        pdata->vbat_ovp_disable     = of_property_read_bool(np, "ln8000_charger,vbat-ovp-disable");
        pdata->vbat_reg_disable     = of_property_read_bool(np, "ln8000_charger,vbat-reg-disable");
        pdata->iin_ocp_disable      = of_property_read_bool(np, "ln8000_charger,iin-ocp-disable");
        pdata->iin_reg_disable      = of_property_read_bool(np, "ln8000_charger,iin-reg-disable");
        pdata->tbus_mon_disable     = of_property_read_bool(np, "ln8000_charger,tbus-mon-disable");
        pdata->tbat_mon_disable     = of_property_read_bool(np, "ln8000_charger,tbat-mon-disable");
        pdata->tdie_prot_disable    = of_property_read_bool(np, "ln8000_charger,tdie-prot-disable");
        pdata->tdie_reg_disable     = of_property_read_bool(np, "ln8000_charger,tdie-reg-disable");
        pdata->revcurr_prot_disable = of_property_read_bool(np, "ln8000_charger,revcurr-prot-disable");

        /* override device tree */
        if (info->dev_role == LN_ROLE_MASTER) {
                ln_info("disable TS_BAT monitor for primary device on dual-mode\n");
                pdata->tbat_mon_disable = true;
        } else if (info->dev_role == LN_ROLE_SLAVE) {
                ln_info("disable VBAT_OVP and TS_BAT for secondary device on dual-mode\n");
                pdata->vbat_ovp_disable   = true;
                pdata->tbat_mon_disable   = true;
        }

        ln_info("vbat_ovp_disable = %d\n", pdata->vbat_ovp_disable);
        ln_info("vbat_reg_disable = %d\n", pdata->vbat_reg_disable);
        ln_info("iin_ocp_disable = %d\n", pdata->iin_ocp_disable);
        ln_info("iin_reg_disable = %d\n", pdata->iin_reg_disable);
        ln_info("tbus_mon_disable = %d\n", pdata->tbus_mon_disable);
        ln_info("tbat_mon_disable = %d\n", pdata->tbat_mon_disable);
        ln_info("tdie_prot_disable = %d\n", pdata->tdie_prot_disable);
        ln_info("tdie_reg_disable = %d\n", pdata->tdie_reg_disable);
        ln_info("revcurr_prot_disable = %d\n", pdata->revcurr_prot_disable);

        return 0;
}

static int ln8000_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
        struct ln8000_info *info;
        int ret = 0;
#if 1
		struct iio_dev *indio_dev;

		indio_dev = devm_iio_device_alloc(&client->dev, sizeof(*info));
		info = iio_priv(indio_dev);
		info->indio_dev = indio_dev;
#else

        info = devm_kzalloc(&client->dev, sizeof(struct ln8000_info), GFP_KERNEL);
        if (info == NULL) {
                dev_err(&client->dev, "%s: fail to alloc devm for ln8000_info\n", __func__);
                return -ENOMEM;
        }
#endif
        /* detect device on connected i2c bus */
        ret = i2c_smbus_read_byte_data(client, LN8000_REG_DEVICE_ID);
        if (IS_ERR_VALUE((unsigned long)ret)) {
                dev_err(&client->dev, "fail to detect ln8000 on i2c_bus(addr=0x%x)\n", client->addr);
                return -ENODEV;
        }
        dev_info(&client->dev, "device id=0x%x\n", ret);

        info->dev_role = ln8000_get_dev_role(client);
        if (IS_ERR_VALUE((unsigned long)info->dev_role)) {
                kfree(info);
                return -EINVAL;
        }

        info->pdata = devm_kzalloc(&client->dev, sizeof(struct ln8000_platform_data), GFP_KERNEL);
        if (info->pdata == NULL) {
                ln_err("fail to alloc devm for ln8000_platform_data\n");
                kfree(info);
                return -ENOMEM;
        }
        info->dev = &client->dev;
        info->client = client;
        ret = ln8000_parse_dt(info);
        if (IS_ERR_VALUE((unsigned long)ret)) {
                ln_err("fail to parsed dt\n");
                goto err_devmem;
        }

        mutex_init(&info->data_lock);
        mutex_init(&info->i2c_lock);
        mutex_init(&info->irq_lock);
#ifndef CONFIG_WT_QGKI
		mutex_init(&info->irq_complete);
#endif
        i2c_set_clientdata(client, info);

        ln8000_soft_reset(info);
        ln8000_init_device(info);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
		ret = ln_init_iio_psy(info);
		if (ret)
			goto err_cleanup;
		ret = ln8000_irq_init(info);
		if (ret < 0) {
		        return ret;
		}
#else
        ret = ln8000_psy_register(info);
        if (ret) {
                goto err_cleanup;
        }
        ret = ln8000_irq_init(info);
        if (ret < 0) {
                goto err_psy;
        }
#endif

        if (client->irq) {
                ret = devm_request_threaded_irq(&client->dev, client->irq,
                                                NULL, ln8000_interrupt_handler,
                                                IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
                                                "ln8000-charger-irq", info);
                if (ret < 0) {
                        ln_err("request irq for irq=%d failed, ret =%d\n",
                        client->irq, ret);
                        goto err_wakeup;
                }
                enable_irq_wake(client->irq);
        } else {
                ln_info("don't support isr(irq=%d)\n", info->client->irq);
        }

        device_init_wakeup(info->dev, 1);

        ret = ln8000_create_debugfs_entries(info);
        if (IS_ERR_VALUE((unsigned long)ret)) {
                goto err_wakeup;
        }

        determine_initial_status(info);

#ifdef CONFIG_WT_QGKI
        if (info->dev_role == LN_ROLE_STANDALONE)
            hardwareinfo_set_prop(HARDWARE_SUB_CHARGER_SLAVE, "LN8000_CHARGER_SLAVE");
#endif
        dev_info(&client->dev, "%s: End!\n", __func__);
        return 0;

err_wakeup:
#ifdef CONFIG_WT_QGKI
        if (client->irq) {
                free_irq(client->irq, info);
                if (info->pdata->irq_gpio) {
                        gpiod_put(info->pdata->irq_gpio);
                }
        }
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#else
err_psy:
        power_supply_unregister(info->psy_chg);
#endif
err_cleanup:
        i2c_set_clientdata(client, NULL);
        mutex_destroy(&info->data_lock);
        mutex_destroy(&info->i2c_lock);
        mutex_destroy(&info->irq_lock);
err_devmem:
        kfree(info->pdata);
        kfree(info);

        return ret;
}

static int ln8000_remove(struct i2c_client *client)
{
        struct ln8000_info *info = i2c_get_clientdata(client);

        ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);

        debugfs_remove_recursive(info->debug_root);
		
#ifdef CONFIG_WT_QGKI
        if (client->irq) {
                free_irq(client->irq, info);
                if (info->pdata->irq_gpio) {
                        gpiod_put(info->pdata->irq_gpio);
                }
        }
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#else
        power_supply_unregister(info->psy_chg);
#endif
        i2c_set_clientdata(info->client, NULL);

        mutex_destroy(&info->data_lock);
        mutex_destroy(&info->i2c_lock);
        mutex_destroy(&info->irq_lock);

        kfree(info->pdata);
        kfree(info);

        return 0;
}

static void ln8000_shutdown(struct i2c_client *client)
{
        struct ln8000_info *info = i2c_get_clientdata(client);

        ln8000_change_opmode(info, LN8000_OPMODE_STANDBY);
}

#if defined(CONFIG_PM)
static int ln8000_suspend(struct device *dev)
{
        struct ln8000_info *info = dev_get_drvdata(dev);
#ifdef CONFIG_WT_QGKI
        if (device_may_wakeup(dev) && info->client->irq)
                enable_irq_wake(info->client->irq);

        ln8000_irq_sleep(info, 1);
#else
	mutex_lock(&info->irq_complete);
	//sc->resume_completed = false;
	mutex_unlock(&info->irq_complete);
#endif
        return 0;
}

static int ln8000_resume(struct device *dev)
{
        struct ln8000_info *info = dev_get_drvdata(dev);
#ifdef CONFIG_WT_QGKI
        if (device_may_wakeup(dev) && info->client->irq)
                disable_irq_wake(info->client->irq);

        ln8000_irq_sleep(info, 0);
#else
		mutex_lock(&info->irq_complete);
		//sc->resume_completed = false;
		mutex_unlock(&info->irq_complete);
#endif
        return 0;
}

static const struct dev_pm_ops ln8000_pm_ops = {
        .suspend = ln8000_suspend,
        .resume = ln8000_resume,
};
#endif

static struct i2c_driver ln8000_2_driver = {
        .driver   = {
                .name = "ln8000_charger_2",
                .owner = THIS_MODULE,
                .of_match_table = of_match_ptr(ln8000_dt_match),
#if defined(CONFIG_PM)
                .pm   = &ln8000_pm_ops,
#endif
        },
        .probe    = ln8000_probe,
        .remove   = ln8000_remove,
        .shutdown = ln8000_shutdown,
        .id_table = ln8000_id,
};
module_i2c_driver(ln8000_2_driver);

MODULE_AUTHOR("sungdae choi<sungdae@lionsemi.com>");
MODULE_DESCRIPTION("LIONSEMI LN8000 charger driver");
MODULE_LICENSE("GPL v2");
MODULE_VERSION("0.3.0");
