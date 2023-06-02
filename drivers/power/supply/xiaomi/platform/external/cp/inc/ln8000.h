
#ifndef __LN8000_H__
#define __LN8000_H__

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
#include <linux/battmngr/battmngr_notifier.h>

static const char *ln8000_dev_name[] = {
        "ln8000-standalone",
        "ln8000-master",
        "ln8000-slave",
};

#define LN8000_ROLE_STDALONE   	0
#define LN8000_ROLE_SLAVE		1
#define LN8000_ROLE_MASTER		2

enum {
	LN8000_STDALONE,
	LN8000_SLAVE,
	LN8000_MASTER,
};

#define PROBE_CNT_MAX	50

static int ln8000_mode_data[] = {
	[LN8000_STDALONE] = LN8000_ROLE_STDALONE,
	[LN8000_MASTER] = LN8000_ROLE_SLAVE,
	[LN8000_SLAVE] = LN8000_ROLE_MASTER,
};

#define ln_err(fmt, ...)                        \
do {                                            \
        if (info->dev_role == LN_ROLE_STANDALONE)   \
                printk(KERN_ERR "ln8000-standalone: %s: " fmt, __func__, ##__VA_ARGS__);   \
        else if (info->dev_role == LN_ROLE_MASTER)                              \
                printk(KERN_ERR "ln8000-master: %s: " fmt, __func__, ##__VA_ARGS__);   \
        else                                                                    \
                printk(KERN_ERR "ln8000-slave: %s: " fmt, __func__, ##__VA_ARGS__);   \
} while (0);

#define ln_info(fmt, ...)                       \
do {                                            \
        if (info->dev_role == LN_ROLE_STANDALONE)   \
                printk(KERN_INFO "ln8000-standalone: %s: " fmt, __func__, ##__VA_ARGS__);  \
        else if (info->dev_role == LN_ROLE_MASTER)                              \
                printk(KERN_INFO "ln8000-master: %s: " fmt, __func__, ##__VA_ARGS__);  \
        else                                                                    \
                printk(KERN_INFO "ln8000-slave: %s: " fmt, __func__, ##__VA_ARGS__);  \
} while (0);

#define ln_dbg(fmt, ...)                        \
do {                                            \
        if (info->dev_role == LN_ROLE_STANDALONE)   \
                printk(KERN_DEBUG "ln8000-standalone: %s: " fmt, __func__, ##__VA_ARGS__); \
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
	struct mutex irq_complete;

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

        /* for restore reg_init_val */
        u8 regulation_ctrl;
        u8 adc_ctrl;
        u8 v_float_ctrl;
        u8 charge_ctrl;

        /* debugfs */
        struct dentry *debug_root;
        u32 debug_address;
        struct iio_dev          *indio_dev;
        struct iio_chan_spec    *iio_chan;
        struct iio_channel	*int_iio_chans;
        struct delayed_work dump_regs_work;
};

int ln8000_set_sw_freq(struct ln8000_info *info, u8 fsw_cfg);
int ln8000_set_vac_ovp(struct ln8000_info *info, unsigned int ovp_th);
int ln8000_set_vbat_float(struct ln8000_info *info, unsigned int cfg);
int ln8000_set_iin_limit(struct ln8000_info *info, unsigned int cfg);
int ln8000_set_ntc_alarm(struct ln8000_info *info, unsigned int cfg);
int ln8000_enable_vbat_ovp(struct ln8000_info *info, bool enable);
int ln8000_enable_vbat_regulation(struct ln8000_info *info, bool enable);
int ln8000_enable_vbat_loop_int(struct ln8000_info *info, bool enable);
int ln8000_enable_iin_ocp(struct ln8000_info *info, bool enable);
int ln8000_enable_iin_regulation(struct ln8000_info *info, bool enable);
int ln8000_enable_iin_loop_int(struct ln8000_info *info, bool enable);
int ln8000_enable_tdie_prot(struct ln8000_info *info, bool enable);
int ln8000_enable_tdie_regulation(struct ln8000_info *info, bool enable);
int ln8000_enable_tbus_monitor(struct ln8000_info *info, bool enable);
int ln8000_enable_tbat_monitor(struct ln8000_info *info, bool enable);
int ln8000_enable_wdt(struct ln8000_info *info, bool enable);
int ln8000_enable_rcp(struct ln8000_info *info, bool enable);
int ln8000_enable_auto_recovery(struct ln8000_info *info, bool enable);
int ln8000_enable_rcp_auto_recovery(struct ln8000_info *info, bool enable);
int ln8000_set_adc_mode(struct ln8000_info *info, unsigned int cfg);
int ln8000_set_adc_hib_delay(struct ln8000_info *info, unsigned int cfg);
int ln8000_check_status(struct ln8000_info *info);
void ln8000_soft_reset(struct ln8000_info *info);
void ln8000_update_opmode(struct ln8000_info *info);
int ln8000_change_opmode(struct ln8000_info *info, unsigned int target_mode);
int ln8000_get_adc_data(struct ln8000_info *info, unsigned int ch, int *result);
int psy_chg_get_charging_enabled(struct ln8000_info *info);
int psy_chg_get_ti_alarm_status(struct ln8000_info *info);
int psy_chg_get_it_bus_error_status(struct ln8000_info *info);
int psy_chg_get_ti_fault_status(struct ln8000_info *info);
int psy_chg_set_charging_enable(struct ln8000_info *info, int val);
int psy_chg_set_present(struct ln8000_info *info, int val);

#endif /* __LN8000_H__ */

