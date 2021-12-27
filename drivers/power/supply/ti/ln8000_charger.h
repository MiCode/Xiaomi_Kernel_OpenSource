/*
 * ln8000-charger.h - Charger driver for LIONSEMI LN8000
 *
 * Copyright (C) 2021 Lion Semiconductor Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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

#ifndef __LN8000_CHARGER_H__
#define __LN8000_CHARGER_H__

//#define LN8000_DUAL_CONFIG	/* uncomment to enable DUAL chip operation */
//#define LN8000_DEBUG_SUPPORT

/**
 * ln8000 device descripion definition 
 */
#define ASSIGNED_BITS(_end, _start) ((BIT(_end) - BIT(_start)) + BIT(_end))

/* register map description */
enum ln8000_int1_desc {
    LN8000_MASK_FAULT_INT           = BIT(7),
    LN8000_MASK_NTC_PROT_INT	    = BIT(6),
    LN8000_MASK_CHARGE_PHASE_INT    = BIT(5),
    LN8000_MASK_MODE_INT            = BIT(4),
    LN8000_MASK_REV_CURR_INT        = BIT(3),
    LN8000_MASK_TEMP_INT            = BIT(2),
    LN8000_MASK_ADC_DONE_INT        = BIT(1),
    LN8000_MASK_TIMER_INT           = BIT(0),
};

enum ln8000_sys_sts_desc {
    LN8000_MASK_IIN_LOOP_STS        = BIT(7),
    LN8000_MASK_VFLOAT_LOOP_STS     = BIT(6),
    LN8000_MASK_BYPASS_ENABLED      = BIT(3),
    LN8000_MASK_SWITCHING_ENABLED   = BIT(2),
    LN8000_MASK_STANDBY_STS         = BIT(1),
    LN8000_MASK_SHUTDOWN_STS        = BIT(0),
};

enum ln8000_safety_sts_desc {
    LN8000_MASK_TEMP_MAX_STS        = BIT(6),
    LN8000_MASK_TEMP_REGULATION_STS = BIT(5),
    LN8000_MASK_NTC_ALARM_STS       = BIT(4),
    LN8000_MASK_NTC_SHUTDOWN_STS    = BIT(3),
    LN8000_MASK_REV_IIN_STS         = BIT(2),
};

enum ln8000_fault1_sts_desc {
    LN8000_MASK_WATCHDOG_TIMER_STS  = BIT(7),
    LN8000_MASK_VBAT_OV_STS         = BIT(6),
    LN8000_MASK_VAC_UNPLUG_STS      = BIT(4),
    LN8000_MASK_VAC_OV_STS          = BIT(3),
    LN8000_MASK_VIN_OV_STS          = BIT(1),
    LN8000_MASK_VFAULTS	            = ASSIGNED_BITS(6,0),
};

enum ln8000_fault2_sts_desc {
    LN8000_MASK_IIN_OC_DETECTED     = BIT(7),
};

enum ln8000_ldo_sts_desc {
    LN8000_MASK_VBAT_MIN_OK_STS	    = BIT(7),
    LN8000_MASK_CHARGE_TERM_STS	    = BIT(5),
    LN8000_MASK_RECHARGE_STS        = BIT(4),
};

enum ln8000_regulation_ctrl_desc {
    LN8000_BIT_ENABLE_VFLOAT_LOOP_INT   = 7,
    LN8000_BIT_ENABLE_IIN_LOOP_INT      = 6,
    LN8000_BIT_DISABLE_VFLOAT_LOOP      = 5,
    LN8000_BIT_DISABLE_IIN_LOOP         = 4,
    LN8000_BIT_TEMP_MAX_EN              = 2,
    LN8000_BIT_TEMP_REG_EN              = 1,
};

enum ln8000_sys_ctrl_desc {
    LN8000_BIT_STANDBY_EN               = 3,
    LN8000_BIT_REV_IIN_DET              = 2,
    LN8000_BIT_EN_1TO1                  = 0,
};

enum ln8000_fault_ctrl_desc {
    LN8000_BIT_DISABLE_IIN_OCP          = 6,
    LN8000_BIT_DISABLE_VBAT_OV          = 5,
    LN8000_BIT_DISABLE_VAC_OV           = 4,
    LN8000_BIT_DISABLE_VAC_UV           = 3,
    LN8000_BIT_DISABLE_VIN_OV           = 2,
};

enum ln8000_bc_op1_desc {
    LN8000_BIT_DUAL_FUNCTION_EN         = 2,
    LN8000_BIT_DUAL_CFG	                = 1,
    LN8000_BIT_DUAL_LOCKOUT_EN          = 0,
};

enum ln8000_reg_addr {
    LN8000_REG_DEVICE_ID        = 0x00,
    LN8000_REG_INT1             = 0x01,
    LN8000_REG_INT1_MSK         = 0x02,
    LN8000_REG_SYS_STS          = 0x03,
    LN8000_REG_SAFETY_STS       = 0x04,
    LN8000_REG_FAULT1_STS       = 0x05,
    LN8000_REG_FAULT2_STS       = 0x06,
    LN8000_REG_CURR1_STS        = 0x07,
    LN8000_REG_LDO_STS          = 0x08,
    LN8000_REG_ADC01_STS        = 0x09,
    LN8000_REG_ADC02_STS        = 0x0A,
    LN8000_REG_ADC03_STS        = 0x0B,
    LN8000_REG_ADC04_STS        = 0x0C,
    LN8000_REG_ADC05_STS        = 0x0D,
    LN8000_REG_ADC06_STS        = 0x0E,
    LN8000_REG_ADC07_STS        = 0x0F,
    LN8000_REG_ADC08_STS        = 0x10,
    LN8000_REG_ADC09_STS        = 0x11,
    LN8000_REG_ADC10_STS        = 0x12,
    LN8000_REG_IIN_CTRL         = 0x1B,
    LN8000_REG_REGULATION_CTRL  = 0x1C,
    LN8000_REG_PWR_CTRL         = 0x1D,
    LN8000_REG_SYS_CTRL         = 0x1E,
    LN8000_REG_LDO_CTRL         = 0x1F,
    LN8000_REG_GLITCH_CTRL      = 0x20,
    LN8000_REG_FAULT_CTRL       = 0x21,
    LN8000_REG_NTC_CTRL         = 0x22,
    LN8000_REG_ADC_CTRL         = 0x23,
    LN8000_REG_ADC_CFG          = 0x24,
    LN8000_REG_RECOVERY_CTRL    = 0x25,
    LN8000_REG_TIMER_CTRL       = 0x26,
    LN8000_REG_THRESHOLD_CTRL   = 0x27,
    LN8000_REG_V_FLOAT_CTRL     = 0x28,
    LN8000_REG_CHARGE_CTRL      = 0x29,
    LN8000_REG_LION_CTRL        = 0x30,
    LN8000_REG_PRODUCT_ID	= 0x31,
    LN8000_REG_BC_OP_1          = 0x41,
    LN8000_REG_BC_OP_2          = 0x42,
    LN8000_REG_BC_STS_A         = 0x49,
    LN8000_REG_BC_STS_B         = 0x4A,
    LN8000_REG_BC_STS_C         = 0x4B,
    LN8000_REG_BC_STS_D         = 0x4C,
    LN8000_REG_BC_STS_E         = 0x4D,
    LN8000_REG_MAX,
};

/* device feature configuration desc */
#define LN8000_DEVICE_ID    0x42

enum ln8000_role {
	LN_ROLE_STANDALONE      = 0x0,
	LN_ROLE_MASTER          = 0x1,
	LN_ROLE_SLAVE           = 0x2,
};

enum ln8000_opmode_{
    LN8000_OPMODE_UNKNOWN   = 0x0,
    LN8000_OPMODE_STANDBY   = 0x1,
    LN8000_OPMODE_BYPASS    = 0x2,
    LN8000_OPMODE_SWITCHING = 0x3,
};

enum ln8000_vac_ov_cfg_desc {
    LN8000_VAC_OVP_6P5V     = 0x0,
    LN8000_VAC_OVP_11V      = 0x1,
    LN8000_VAC_OVP_12V      = 0x2,
    LN8000_VAC_OVP_13V      = 0x3,
};

enum ln8000_watchdpg_cfg_desc {
    LN8000_WATCHDOG_5SEC    = 0x0,
    LN8000_WATCHDOG_10SEC   = 0x1,
    LN8000_WATCHDOG_20SEC   = 0x2,
    LN8000_WATCHDOG_40SEC   = 0x3,
    LN8000_WATCHDOG_MAX
};

enum ln8000_adc_channel_index {
    LN8000_ADC_CH_VOUT      = 1,
    LN8000_ADC_CH_VIN,
    LN8000_ADC_CH_VBAT,
    LN8000_ADC_CH_VAC,
    LN8000_ADC_CH_IIN,
    LN8000_ADC_CH_DIETEMP,
    LN8000_ADC_CH_TSBAT,
    LN8000_ADC_CH_TSBUS,
    LN8000_ADC_CH_ALL
};

enum ln8000_adc_mode_desc {     /* used FORCE_ADC_MODE + ADC_SHUTDOWN_CFG */
    ADC_AUTO_HIB_MODE       = 0x0,
    ADC_AUTO_SHD_MODE       = 0x1,
    ADC_SHUTDOWN_MODE       = 0x2,
    ADC_HIBERNATE_MODE      = 0x4,
    ADC_NORMAL_MODE         = 0x6,
};

enum ln8000_adc_hibernate_delay_desc {
    ADC_HIBERNATE_500MS     = 0x0,
    ADC_HIBERNATE_1S        = 0x1,
    ADC_HIBERNATE_2S        = 0x2,
    ADC_HIBERNATE_4S        = 0x3,
};

/* electrical numeric calculation unit description */
#define LN8000_VBAT_FLOAT_MIN           3725000     /* unit = uV */
#define LN8000_VBAT_FLOAT_MAX           5000000
#define LN8000_VBAT_FLOAT_LSB           5000
#define LN8000_ADC_VOUT_STEP	        5000        /* 5mV= 5000uV LSB	(0V ~ 5.115V) */
#define LN8000_ADC_VIN_STEP             16000       /* 16mV=16000uV LSB	(0V ~ 16.386V) */
#define LN8000_ADC_VBAT_STEP	        5000        /* 5mV= 5000uV LSB	(0V ~ 5.115V) */
#define LN8000_ADC_VBAT_MIN             1000000     /* 1V */
#define LN8000_ADC_VAC_STEP             16000       /* 16mV=16000uV LSB	(0V ~ 16.386V) */
#define LN8000_ADC_VAC_OS               5	        
#define LN8000_ADC_IIN_STEP             4890        /* 4.89mA=4890uA LSB	(0A ~ 5A) */
#define LN8000_ADC_DIETEMP_STEP	        4350        /* 0.435C LSB = 4350dC/1000 (-25C ~ 160C) */
#define LN8000_ADC_DIETEMP_DENOM        1000        /* 1000 */
#define LN8000_ADC_DIETEMP_MIN	        (-250)      /* -25C = -250dC */
#define LN8000_ADC_DIETEMP_MAX	        1600        /* 160C = 1600dC */
#define LN8000_ADC_NTCV_STEP	        2933        /* 2.933mV=2933uV LSB	(0V ~ 3V) */
#define LN8000_IIN_CFG_MIN              500000      /* 500mA=500,000uA */
#define LN8000_IIN_CFG_LSB              50000       /* 50mA=50,000uA */

/* device default values */
#define LN8000_BAT_OVP_DEFAULT          4440000
#define LN8000_BUS_OVP_DEFAULT          9500000
#define LN8000_BUS_OCP_DEFAULT          2000000

#define LN8000_NTC_ALARM_CFG_DEFAULT    226         /* NTC alarm threshold (~40C) */
#define LN8000_NTC_SHUTDOWN_CFG         2           /* NTC shutdown config (-16LSB ~ 4.3C) */
#define LN8000_DEFAULT_FSW_CFG          8           /* 8=440kHz, switching freq */
#define LN8000_IIN_CFG_DEFAULT          2000000     /* 2A=2,000,000uA, input current limit */

/* bus protection values for QC */
#define BUS_OVP_FOR_QC                  13000000 /* ln8000 didn't used 10V, (support tot 6.5V, 11V, 12V, 13V) */
#define BUS_OVP_ALARM_FOR_QC			9500000
#define BUS_OCP_FOR_QC_CLASS_A			3250000
#define BUS_OCP_ALARM_FOR_QC_CLASS_A    2000000
#define BUS_OCP_FOR_QC_CLASS_B			3750000
#define BUS_OCP_ALARM_FOR_QC_CLASS_B	2800000
#define BUS_OCP_FOR_QC3P5_CLASS_A		3000000
#define BUS_OCP_ALARM_FOR_QC3P5_CLASS_A	2500000
#define BUS_OCP_FOR_QC3P5_CLASS_B		3500000
#define BUS_OCP_ALARM_FOR_QC3P5_CLASS_B	3200000

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
    struct power_supply	*psy_chg;

    struct mutex data_lock;
    struct mutex i2c_lock;
    struct mutex irq_lock;
    struct regmap *regmap;
    struct power_supply_config psy_cfg;
    struct power_supply_desc psy_desc;

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
    bool vac_unplug;            /* vac unplugged */
    bool iin_rc;                /* iin reverse current detected */
    bool volt_qual;             /* all voltages are qualified */
    bool usb_present;           /* usb plugged (present) */
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

    /* VAC_OV control for QC3 */
    struct delayed_work vac_ov_work;
    bool vac_ov_work_on;

    /* for restore reg_init_val */
    u32 regulation_ctrl;
    u32 adc_ctrl;
    u32 v_float_ctrl;
    u32 charge_ctrl;

#ifdef LN8000_ROLE_MASTER
    bool ibat_term;             /* battery current below termination threshold */
    bool vbat_rechg;            /* battery voltage below recharge threshold */
    bool vbat_min;              /* battery voltage above min. threshold */
#endif

	bool standalone_mode_master;
	bool standalone_mode_slave;

    /* debugfs */
    struct dentry *debug_root;
    u32 debug_address;
};

#endif  /* __LN8000_CHARGER_H__ */
