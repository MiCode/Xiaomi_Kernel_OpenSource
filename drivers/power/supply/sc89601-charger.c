// SPDX-License-Identifier: GPL-2.0:
// Copyright (c) 2024 unisoc.
/*
 * Driver for the sc sc89601 charger.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/alarmtimer.h>
#include <linux/gpio/consumer.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/power/charger-manager.h>
#include <linux/power/sprd_battery_info.h>
#include <linux/regmap.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/pm_wakeup.h>
#include "lc_charger_class.h"
#include <linux/delay.h>

#define SC89601_BATTERY_NAME			"sc27xx-fgu"
#define SC89601_I2C_ADDR                0x6B
#define SC89601_DEV_ID				0x03

#define SC89601_REG_0				0x0
#define SC89601_REG_1				0x1
#define SC89601_REG_2				0x2
#define SC89601_REG_3				0x3
#define SC89601_REG_4				0x4
#define SC89601_REG_5				0x5
#define SC89601_REG_6				0x6
#define SC89601_REG_7				0x7
#define SC89601_REG_8				0x8
#define SC89601_REG_9				0x9
#define SC89601_REG_A				0xa
#define SC89601_REG_B				0xb
#define SC89601_REG_C				0xc
#define SC89601_REG_D				0xd
#define SC89601_REG_E				0xe
#define SC89601_REG_NUM			15
#define SC89601_REG_7F				0x7F
#define REG93_VAL				0x2A
#define REG7F_KEY1                  		0x5A
#define REG7F_KEY2                  		0x68
#define REG7F_KEY3                  		0x65
#define REG7F_KEY4                  		0x6E
#define REG7F_KEY5                  		0x67
#define REG7F_KEY6                  		0x4C
#define REG7F_KEY7                  		0x69
#define REG7F_KEY8                  		0x6E
#define THERMAL_LOOP_DIS			0x80
#define THERMAL_LOOP_DIS_MASK			GENMASK(6,6)
#define THERMAL_LOOP_DIS_SHIFT			6
#define IBUS_LOOP_DIS				0x90
#define IBUS_LOOP_DIS_MASK			GENMASK(6,6)
#define IBUS_LOOP_DIS_SHIFT			6
#define BIT_DP_DM_BC_ENB			BIT(0)
#define SC89601_OTG_VALID_MS			500
#define SC89601_FEED_WATCHDOG_VALID_MS		50
#define SC89601_OTG_ALARM_TIMER_S		15
#define SC89601_REG_IINLIM_BASE		100
#define SC89601_REG_ICHG_LSB			60
#define SC89601_REG_ICHG_MASK			GENMASK(5, 0)
#define SC89601_REG_ICHG_SHIFT			0
#define SC89601_REG_CHG_MASK			GENMASK(4, 4)
#define SC89601_REG_CHG_SHIFT			4
#define SC89601_REG_CHG_DISABLE		0
#define SC89601_REG_CHG_ENABLE			1
#define SC89601_REG_WD_RST_MASK		GENMASK(6, 6)
#define SC89601_REG_WD_RST_SHIFT		6
#define SC89601_REG_WD_RST				1
#define SC89601_REG_OTG_MASK			GENMASK(5, 5)
#define SC89601_REG_OTG_SHIFT			5
#define SC89601_REG_OTG_ENABLE			1
#define SC89601_REG_OTG_DISABLE		0
#define SC89601_REG_BOOST_FAULT_MASK		GENMASK(6, 6)
#define SC89601_REG_BOOST_LIMIT_MASK	GENMASK(7,7)
#define SC89601_REG_BOOST_LIMIT_SHIFT	7
#define SC89601_REG_LIMIT_SEL_MASK		GENMASK(6,6)
#define SC89601_REG_LIMIT_SEL_SHIFT		6
#define SC89601_REG_WATCHDOG_MASK		GENMASK(6, 6)
#define SC89601_REG_WATCHDOG_TIMER_MASK	GENMASK(5, 4)
#define SC89601_REG_WATCHDOG_TIMER_SHIFT	4
#define SC89601_REG_EN_TIMER_MASK		GENMASK(3, 3)
#define SC89601_REG_EN_TIMER_SHIFT		3
#define SC89601_WDT_DISABLE			0
#define SC89601_WDT_40S			1
#define SC89601_REG_TERMINAL_VOLTAGE_MASK	GENMASK(7, 3)
#define SC89601_REG_TERMINAL_VOLTAGE_SHIFT	3
#define SC89601_REG_TERMINAL_VOLTAGE_BASE	3847
#define SC89601_REG_TERMINAL_VOLTAGE_MAX	4615
#define SC89601_REG_TERMINAL_VOLTAGE_LSB	32
#define SC89601_REG_IPRECHG_CUR_MASK		GENMASK(7, 4)
#define SC89601_REG_TERMINAL_CUR_MASK		GENMASK(3, 0)
#define SC89601_REG_VINDPM_VOLTAGE_MASK	GENMASK(3, 0)
#define SC89601_REG_VINDPM_MIN_3P9V	3900
#define SC89601_REG_VINDPM_MAX_5P1V	5100
#define SC89601_REG_VINDPM_MAX_8P0V	8000
#define SC89601_REG_VINDPM_MAX_8P2V	8200
#define SC89601_REG_VINDPM_MAX_8P4V	8400
#define SC89601_REG_VINDPM_OFFSET_3P9V	3900
#define SC89601_REG_VINDPM_LSB		100
#define SC89601_REG_OVP_MASK			GENMASK(7, 6)
#define SC89601_REG_OVP_SHIFT			6
#define SC89601_REG_OVP_5P8V			0
#define SC89601_REG_OVP_6P4V			1
#define SC89601_REG_OVP_11V			2
#define SC89601_REG_OVP_14P2V			3
#define SC89601_REG_EN_HIZ_MASK		GENMASK(7, 7)
#define SC89601_REG_EN_HIZ_SHIFT		7
#define SC89601_PN_MASK			0x78
#define SC89601_PN_SHIFT			3
#define SC89601_VINDPM_STATE_MASK		0x40 //GENMASK(6, 6)
#define SC89601_VINDPM_STATE_SHIFT		6


#define SC89601_REG7_BATFET_DLY_SHIFT	3
#define SC89601_REG7_BATFET_DLY_MASK	GENMASK(3, 3)
#define SC89601_REG7_BATFET_DIS_SHIFT	5
#define SC89601_REG7_BATFET_DIS_MASK	GENMASK(5, 5)
#define SC89601_REG7_IINDET_EN_SHIFT	7
#define SC89601_REG7_IINDET_EN_MASK	GENMASK(7, 7)
#define SC89601_REG7_IINDET_EN_ENABLE		1
#define SC89601_REG7_IINDET_EN_DISABLE		0

#define SC89601_REGA_VBUS_GD_MASK		0x80
#define SC89601_REGA_VBUS_GD_SHIFT		7

#define SC89601_DISABLE_BATFET_RST_MASK        BIT(2)
#define SC89601_DISABLE_BATFET_RST_SHIFT       2
#define SC89601_REG_LIMIT_CURRENT_MASK		GENMASK(4, 0)

#define SC89601_DISABLE_PIN_MASK		BIT(0)
#define SC89601_DISABLE_PIN_MASK_2730		BIT(0)
#define SC89601_DISABLE_PIN_MASK_2721		BIT(15)
#define SC89601_DISABLE_PIN_MASK_2720		BIT(0)

#define SC89601_OTG_RETRY_TIMES		10
#define SC89601_LIMIT_CURRENT_MAX		3200000
#define SC89601_LIMIT_CURRENT_OFFSET		100000
#define SC89601_REG_IINDPM_LSB			100
#define SC89601_SHUTDOWN_LIMIT			1500000
#define SC89601_SHUTDOWN_CURRENT		500000

#define SC89601_ROLE_MASTER_DEFAULT		1
#define SC89601_ROLE_SLAVE			2
#define SC89601_FCHG_OVP_6V			6000
#define SC89601_FCHG_OVP_9V			9000
#define SC89601_FCHG_OVP_14V			14000
#define SC89601_FAST_CHARGER_VOLTAGE_MAX	10500000
#define SC89601_NORMAL_CHARGER_VOLTAGE_MAX	6500000
#define SC89601_WAKE_UP_MS			1000
#define SC89601_CURRENT_WORK_MS		100
#define SC89601_WAIT_WL_VBUS_STABLE_CUR_THR	200000
#define SC89601_PROBE_TIMEOUT			msecs_to_jiffies(500)
#define SC89601_WATCH_DOG_TIME_OUT_MS		20000

#define SC89601_REG4_VRECHG_200mV		1
#define SC89601_REG4_VRECHG_MASK		0x01

#define SC89601_REG6_BOOSTV_5P3V		0x30
#define SC89601_REG6_BOOSTV_MASK		0x30

#define SC89601_REG3_IPRECHG_OFFSET		60
#define SC89601_REG3_IPRECHG_MIN		60
#define SC89601_REG3_IPRECHG_MAX		780
#define SC89601_REG3_IPRECHG_STEP		60

#define SC89601_REG3_ITERM_OFFSET		60
#define SC89601_REG3_ITERM_MIN			60
#define SC89601_REG3_ITERM_MAX			960
#define SC89601_REG3_ITERM_STEP		60

#define SC89601_REG2_ICHG_MIN			0
#define SC89601_REG2_ICHG_MAX			3000
#define SC89601_REG2_ICHG_STEP			60

#define SC89601_REG8_VBUS_STAT_MASK		0xE0
#define SC89601_REG8_VBUS_STAT_SHIFT		5
#define SC89601_REG8_PG_STAT_MASK		0x04
#define SC89601_REG8_PG_STAT_SHIFT		2

#define SC89601_VBUS_TYPE_NONE			0
#define SC89601_VBUS_TYPE_USB			1
#define SC89601_VBUS_TYPE_CDP			2
#define SC89601_VBUS_TYPE_ADAPTER		3
#define SC89601_VBUS_TYPE_UNKNOWN_ADAPTER	5
#define SC89601_VBUS_TYPE_NON_STANDARD_ADAPTER	6
#define SC89601_VBUS_TYPE_OTG			7
#define SC89601_REGE_INPUT_DET_DONE_MASK	0x40
#define SC89601_REGE_INPUT_DET_DONE_SHIFT	6
#define SC89601_REGE_INPUT_DET_DONE		1

#define SC89601_REGE_EN_AUTO_MASK		GENMASK(5, 5)
#define SC89601_REGE_EN_AUTO_SHIFT		5

#define SC89601_REGA_DPM_INT_MASK		GENMASK(1, 0)
#define SC89601_REGA_DPM_VALUE			3

#define SC89601_REG0D_VBAT_REG_FT_MASK		0xC0
#define SC89601_REG0D_VBAT_REG_FT_SHIFT		6
#define SC89601_REG0D_VREG_FT_DEFAULT		0
#define SC89601_REG0D_VREG_FT_INC8MV		1
#define SC89601_REG0D_VREG_FT_INC16MV		2
#define SC89601_REG0D_VREG_FT_INC24MV		3
#define SC89601_REG0D_VREG_FT_INC_BASE		0
#define SC89601_REG0D_VREG_FT_INC_LSB		8

#define SC89601_REG01_PFM_DIS_MASK		0x80
#define SC89601_REG01_PFM_DIS_SHIFT		7
#define SC89601_REG01_PFM_ENABLE		0
#define SC89601_REG01_PFM_DISABLE		1

#define SC89601_REG08_CHRG_STAT_MASK		0x18
#define SC89601_REG08_CHRG_STAT_SHIFT		3
#define SC89601_REG08_CHRG_STAT_IDLE		0
#define SC89601_REG08_CHRG_STAT_PRECHG		1
#define SC89601_REG08_CHRG_STAT_FASTCHG		2
#define SC89601_REG08_CHRG_STAT_CHGDONE		3

#define BC1P2_CHECK_WORK			0
#define DEFAULT_BC12_TIME_EXPIRE    900

static bool boot_calibration = false;

struct sc89601_charger_sysfs {
	char *name;
	struct attribute_group attr_g;
	struct device_attribute attr_sc89601_dump_reg;
	struct device_attribute attr_sc89601_lookup_reg;
	struct device_attribute attr_sc89601_sel_reg_id;
	struct device_attribute attr_sc89601_reg_val;
	struct attribute *attrs[5];
	struct sc89601_charger_info *info;
};

struct sc89601_charge_current {
	int sdp_limit;
	int sdp_cur;
	int dcp_limit;
	int dcp_cur;
	int cdp_limit;
	int cdp_cur;
	int unknown_limit;
	int unknown_cur;
	int fchg_limit;
	int fchg_cur;
};

enum sc89601_bc1p2_status {
	SC_BC1P2_UNKNOWN = 0,
	SC_BC1P2_FORCE,
	SC_BC1P2_COMPLETE,
	SC_BC1P2_TIMEOUT,
};

struct sc89601_charger_info {
	struct i2c_client *client;
	struct device *dev;
	struct power_supply *psy_usb;
	struct sc89601_charge_current cur;
	struct mutex lock;
	struct mutex input_limit_cur_lock;
	struct delayed_work otg_work;
	struct delayed_work wdt_work;
	struct delayed_work cur_work;
	struct regmap *pmic;
	struct gpio_desc *gpiod;
	struct extcon_dev *typec_extcon;
	struct alarm otg_timer;
	struct sc89601_charger_sysfs *sysfs;
	struct completion probe_init;
	struct charger_dev *sc_charger;
	u32 charger_detect;
	u32 charger_pd;
	u32 charger_pd_mask;
	u32 new_charge_limit_cur;
	u32 current_charge_limit_cur;
	u32 new_input_limit_cur;
	u32 current_input_limit_cur;
	u32 last_limit_cur;
	u32 actual_limit_cur;
	u32 role;
	u64 last_wdt_time;
	bool charging;
	bool need_disable_Q1;
	int termination_cur;
	int voltage_max_microvolt;
	int iprechg_cur;
	bool disable_wdg;
	bool otg_enable;
	unsigned int irq_gpio;
	bool is_wireless_charge;
	bool is_charger_online;
	int reg_id;
	bool disable_power_path;
	bool probe_initialized;
	bool use_typec_extcon;
	bool shutdown_flag;
	bool vbus_gd;
	bool otg_2a_enable;

	bool true_online;
	bool vbus_good;
	int force_detect_count;

	enum sc89601_bc1p2_status bc1p2_status;
	bool bc12_detect;
	struct mutex bc_detect_lock;

#if BC1P2_CHECK_WORK
	struct delayed_work bc12_timeout_dwork;
#endif

	struct delayed_work force_detect_dwork;
	bool bc12_recovery;
	int power_good;

	int chg_type;
	int irq_cnt;
	bool force_dpdm;
	bool first_plugin;
};

struct sc89601_charger_reg_tab {
	int id;
	u32 addr;
	char *name;
};

static struct sc89601_charger_reg_tab reg_tab[SC89601_REG_NUM + 1] = {
	{0, SC89601_REG_0, "EN_HIZ/EN_ICHG_MON/IINDPM"},
	{1, SC89601_REG_1, "PFM _DIS/WD_RST/OTG_CONFIG/CHG_CONFIG/SYS_Min/Min_VBAT_SEL"},
	{2, SC89601_REG_2, "BOOST_LIM/Q1_FULLON/ICHG"},
	{3, SC89601_REG_3, "IPRECHG/ITERM"},
	{4, SC89601_REG_4, "VREG/TOPOFF_TIMER/VRECHG"},
	{5, SC89601_REG_5, "EN_TERM/WATCHDOG/EN_TIMER/CHG_TIMER/TREG/JEITA_ISET"},
	{6, SC89601_REG_6, "OVP/BOOSTV/VINDPM"},
	{7, SC89601_REG_7, "IINDET_EN/TMR2X_EN/BATFET_DIS/JEITA_VSET/BATFET_DLY/BATFET_RST_EN/VDPM_BAT_TRACK"},
	{8, SC89601_REG_8, "VBUS_STAT/CHRG_STAT/PG_STAT/THERM_STAT/VSYS_STAT"},
	{9, SC89601_REG_9, "WATCHDOG_FAULT/BOOST_FAULT/CHRG_FAULT/BAT_FAULT/NTC_FAULT"},
	{10, SC89601_REG_A, "VBUS_GD/VINDPM_STAT/IINDPM_STAT/TOPOFF_ACTIVE/ACOV_STAT/VINDPM_INT_ MASK/IINDPM_INT_ MASK"},
	{11, SC89601_REG_B, "REG_RST/PN/SCPART/DEV_REV"},
	{12, SC89601_REG_C, "JEITA_VSET_L/JEITA_ISET_L_EN/JEITA_ISET_H/JEITA_VT2/JEITA_VT3"},
	{13, SC89601_REG_D, "EN_PUMPX/PUMPX_UP/PUMPX_DN/DP_VSET/DM_VSET"},
	{14, SC89601_REG_E, "VTC/INPUT_DET_DONE/AUTO_DPDM_EN"},
	{15, 0, "null"},
};

static bool enable_dump_stack;
module_param(enable_dump_stack, bool, 0644);
static int sc89601_read(struct sc89601_charger_info *info, u8 reg, u8 *data);
static int sc89601_write(struct sc89601_charger_info *info, u8 reg, u8 data);
static int sc89601_update_bits(struct sc89601_charger_info *info, u8 reg, u8 mask, u8 data);
static int sc89601_charger_set_power_path_status(struct sc89601_charger_info *info, bool enable);
static int sc89601_charger_set_vindpm(struct sc89601_charger_info *info, u32 vol);
static int sc89601_enable_charger(struct sc89601_charger_info *info, bool enable);
static int sc89601_get_bc1p2_result(struct sc89601_charger_info *info, bool flag);
static void sc89601_dump_register(struct sc89601_charger_info *info);
static int sc89601_get_auto_bc1p2_done(struct sc89601_charger_info *info);
static int sc89601_charger_feed_watchdog(struct sc89601_charger_info *info);

static int sc8960x_enter_test_mode(struct sc89601_charger_info *sc, bool en)
{
    int ret = 0;
    u8 val = 0;
    do {
        ret = sc89601_read(sc, 0x7f, &val);
        if (ret < 0) {
            dev_info(sc->dev, "i2c read failed\n");
            break;
        }
        dev_info(sc->dev, "%s %d\n", __func__, val);
        if (val == 0x00 && !en) {
            dev_info(sc->dev, "not in test mode\n");
            break;
        }

        if (val == 0x01 && en) {
            dev_info(sc->dev, "in test mode\n");
            break;
        }

        ret = sc89601_write(sc, 0x7F, 0X5A);
        ret |= sc89601_write(sc, 0x7F, 0X68);
        ret |= sc89601_write(sc, 0x7F, 0X65);
        ret |= sc89601_write(sc, 0x7F, 0X6E);
        ret = sc89601_write(sc, 0x7F, 0X67);
        ret |= sc89601_write(sc, 0x7F, 0X4C);
        ret |= sc89601_write(sc, 0x7F, 0X69);
        ret |= sc89601_write(sc, 0x7F, 0X6E);
        if (ret < 0) {
            dev_info(sc->dev, "i2c write failed\n");
            break;
        }
    } while(true);
    return 0;
}

static int sc8960x_force_dpdm(struct sc89601_charger_info *sc)
{
	int ret = 0;

	dev_info(sc->dev, "%s\n", __func__);
	/*
	ret = sc89601_charger_set_vindpm(sc, 4800);
	if (ret)
		dev_err(sc->dev, "%s set sc89601 vindpm vol 4800 failed\n", __func__);
	else
		dev_err(sc->dev, "%s set sc89601 vindpm vol 4800 success\n", __func__);
	*/
	mutex_lock(&sc->bc_detect_lock);
	if (sc->bc12_detect) {
		dev_err(sc->dev, "bc12_detect is true, return!\n");
		mutex_unlock(&sc->bc_detect_lock);
		return -EBUSY;
	}
	sc->bc12_detect = true;
	sc->bc1p2_status = SC_BC1P2_FORCE;
	mutex_unlock(&sc->bc_detect_lock);
	/*
	ret = sc89601_charger_set_vindpm(sc, 0);
	if (ret)
		dev_err(sc->dev, "%s set sc89601 vindpm vol 0 failed\n", __func__);
	else
		dev_err(sc->dev, "%s set sc89601 vindpm vol 0 success\n", __func__);
	*/

	if (sc->first_plugin) {
		sc->first_plugin = false;
		sc->force_dpdm = true;
		sc8960x_enter_test_mode(sc, true);
		sc->irq_cnt = 0;
		sc89601_write(sc, 0xD3, 0X85);
		sc89601_write(sc, 0xD4, 0X6A);
		sc89601_write(sc, 0xD3, 0X95);
		mdelay(10);
		if (sc->irq_cnt < 20) {
			sc89601_write(sc, 0xD3, 0x05);
			sc89601_write(sc, 0xD3, 0x2D);
			dev_err(sc->dev, "adapter is not unknown\n");
		} else {
			sc89601_write(sc, 0xD3, 0x1D);
			dev_err(sc->dev, "adapter is unknown\n");
		}
		sc89601_write(sc, 0xD3, 0X00);
		sc8960x_enter_test_mode(sc, false);
		sc->force_dpdm = false;
		sc->chg_type = sc89601_get_bc1p2_result(sc, false);
	} else {
		ret = sc89601_update_bits(sc, SC89601_REG_7, SC89601_REG7_IINDET_EN_MASK,
				SC89601_REG7_IINDET_EN_ENABLE << SC89601_REG7_IINDET_EN_SHIFT);
		if(ret)
			dev_err(sc->dev, "%s, Set FORCE_BC1P2 1 fail, ret = %d\n", __func__, ret);
		else
			dev_err(sc->dev, "%s, Set FORCE_BC1P2 1 succ, ret = %d\n", __func__, ret);
#if BC1P2_CHECK_WORK
		schedule_delayed_work(&sc->bc12_timeout_dwork,
							 msecs_to_jiffies(DEFAULT_BC12_TIME_EXPIRE));
#else
		ret = sc89601_get_auto_bc1p2_done(sc);
		if(ret)
			dev_err(sc->dev, "%s, FORCE_BC1P2 process succ\n", __func__);
		else
			dev_err(sc->dev, "%s, FORCE_BC1P2 process fail\n", __func__);
		sc89601_charger_feed_watchdog(sc);
#endif
	}
	sc->power_good = 0;

	return ret;
}

static void sc8960x_force_detection_dwork_handler(struct work_struct *work)
{
    int ret;
    struct sc89601_charger_info *sc = container_of(work, struct sc89601_charger_info, force_detect_dwork.work);

    //Charger_Detect_Init();

    ret = sc8960x_force_dpdm(sc);
    if (ret) {
        dev_err(sc->dev, "%s: force dpdm failed(%d)\n", __func__, ret);
        return;
    }

    sc->force_detect_count++;
}

#if BC1P2_CHECK_WORK
static void sc8960x_bc12_timeout_dwork_handler(struct work_struct *work) {
    int ret;
    u8 vbus_stat = 0;
    u8 force_dpdm_stat = 0;
    u8 pg_state = 0;
    struct sc89601_charger_info *sc = container_of(work,
                                        struct sc89601_charger_info,
                                        bc12_timeout_dwork.work);


    ret = sc89601_read(sc, SC89601_REG_8, &vbus_stat);
	vbus_stat = (vbus_stat & SC89601_REG8_VBUS_STAT_MASK) >> SC89601_REG8_VBUS_STAT_SHIFT;
    dev_info(sc->dev, "%s: vbus stat = %d\n", __func__, vbus_stat);

    ret = sc89601_read(sc, SC89601_REG_8, &pg_state);
	pg_state = (pg_state & 0x04) >> 2;
    dev_info(sc->dev, "%s: pg stat = %d\n", __func__, pg_state);

    ret = sc89601_read(sc, SC89601_REG_7, &force_dpdm_stat);
    force_dpdm_stat &= 0x80;
    dev_info(sc->dev, "%s: force_dpdm = %d\n", __func__, force_dpdm_stat);

    mutex_lock(&sc->bc_detect_lock);
    sc->bc12_detect = false;
    mutex_unlock(&sc->bc_detect_lock);
    if (force_dpdm_stat && !vbus_stat && !pg_state && !sc->bc12_recovery) {
        dev_info(sc->dev, "BC1.2 timeout\n");
        sc8960x_force_dpdm(sc);
        sc->bc12_recovery = true;
		sc->bc1p2_status = SC_BC1P2_TIMEOUT;
    }
	else
	{
		sc->bc1p2_status = SC_BC1P2_COMPLETE;
		dev_info(sc->dev, "%s: BC1.2 COMPLETE\n", __func__);
	}
}
#endif

static void sc8960x_removed_irq(struct sc89601_charger_info *sc)
{
    dev_info(sc->dev, "%s: adapter/usb removed\n", __func__);
    cancel_delayed_work_sync(&sc->force_detect_dwork);
    mutex_lock(&sc->bc_detect_lock);
    sc->bc12_detect = false;
    sc->bc12_recovery = false;
	sc->bc1p2_status = SC_BC1P2_UNKNOWN;
    mutex_unlock(&sc->bc_detect_lock);

#if BC1P2_CHECK_WORK
	cancel_delayed_work_sync(&sc->bc12_timeout_dwork);
#endif
	/*
	sc8960x_request_dpdm(sc, false);
    sc8960x_set_dpdm_hiz(sc);
    sc8960x_set_iindpm(sc, 500);
    sc8960x_set_ichg(sc, 500);
	*/
}

static irqreturn_t sc8960x_irq_pre_handler(int irq, void *data)
{
    struct sc89601_charger_info *sc = (struct sc89601_charger_info *)data;
    if (!sc->force_dpdm) {
        dev_info(sc->dev, "%s normal irq\n", __func__);
        return IRQ_WAKE_THREAD;
    }
    sc->irq_cnt++;
    return IRQ_HANDLED;
}

static irqreturn_t sc8960x_irq_handler(int irq, void *data)
{
    int ret;
    u8 reg_val = 0, true_power_good = 0;
    bool prev_vbus_gd;
    int tmp_pg = 0;
    struct sc89601_charger_info *sc = (struct sc89601_charger_info *)data;

    dev_info(sc->dev, "%s: sc8960x_irq_handler\n", __func__);
    /* delay400ms 过滤适配器vbus跳变电压的情况 */
    // mdelay(400);

    ret = sc89601_read(sc, SC89601_REG_A, &reg_val);
	if(ret)
		dev_err(sc->dev, "%s: reg_a operate fail\n", __func__);
	else
		dev_err(sc->dev, "%s: reg_a operate succ\n", __func__);
	reg_val = (reg_val & 0x80) >> 7;

    ret = sc89601_read(sc, SC89601_REG_8, &true_power_good);
	if(ret)
		dev_err(sc->dev, "%s: reg_8 operate 1 fail\n", __func__);
	else
		dev_err(sc->dev, "%s: reg_8 operate 1 succ\n", __func__);
	true_power_good = (true_power_good & 0x04) >> 2;

    if (ret) {
        return IRQ_HANDLED;
    }
    dev_info(sc->dev, "sc->vbus_good:%d VBUS_GD:%d\n", sc->vbus_good, reg_val);
    prev_vbus_gd = sc->vbus_good;
    sc->vbus_good = !!reg_val;

    if (!prev_vbus_gd && sc->vbus_good) {
	sc->first_plugin = true;
	sc->force_detect_count = 0;
        dev_info(sc->dev, "%s: adapter/usb inserted\n", __func__);
        sc->true_online = sc->vbus_good;
	/*
	ret = sc89601_update_bits(sc, SC89601_REG_7, 0x03, 0x03);
	if (ret)
		dev_err(sc->dev, "set sc89601 vdpm track failed\n");
	*/

    } else if (prev_vbus_gd && !sc->vbus_good) {
	sc->first_plugin = false;
        dev_info(sc->dev, "%s: adapter/usb removed\n", __func__);
        sc8960x_removed_irq(sc);
        sc->chg_type = sc89601_get_bc1p2_result(sc, false);
        sc->true_online = sc->vbus_good;
    }
    tmp_pg = sc->power_good;
	ret = sc89601_read(sc, SC89601_REG_8, &true_power_good);
	if(ret)
		dev_err(sc->dev, "%s: reg_8 operate 2 fail\n", __func__);
	else
		dev_err(sc->dev, "%s: reg_8 operate 2 succ\n", __func__);
	true_power_good = (true_power_good & 0x04) >> 2;

    dev_info(sc->dev, "qwe pre sc->power_good :%d PG_STAT: %d tmp_pg: %d\n", sc->power_good, true_power_good, tmp_pg);
#if BC1P2_CHECK_WORK
    if (!tmp_pg && true_power_good && delayed_work_pending(&sc->bc12_timeout_dwork)) {
        cancel_delayed_work_sync(&sc->bc12_timeout_dwork);
#else
	if (!tmp_pg && true_power_good && sc->bc1p2_status == SC_BC1P2_FORCE) {
#endif
        mutex_lock(&sc->bc_detect_lock);
        sc->bc12_detect = false;
        sc->bc12_recovery = false;
        mutex_unlock(&sc->bc_detect_lock);
        sc->chg_type = sc89601_get_bc1p2_result(sc, false);
		sc->bc1p2_status = SC_BC1P2_COMPLETE;
		sc89601_charger_feed_watchdog(sc);
		dev_info(sc->dev, "%s: first plugin bc1p2 end\n", __func__);
    }
    sc->power_good = true_power_good;
    if (!prev_vbus_gd && sc->vbus_good) {
        dev_err(sc->dev, "%s schedule force detect work create\n", __func__);
        schedule_delayed_work(&sc->force_detect_dwork, msecs_to_jiffies(80));
    }
    //power_supply_changed(sc->psy);

	sc89601_dump_register(sc);
    return IRQ_HANDLED;
}


static void sc89601_charger_dump_stack(void)
{
	if (enable_dump_stack)
		dump_stack();
}

static void power_path_control(struct sc89601_charger_info *info)
{
	struct device_node *cmdline_node;
	const char *cmd_line;
	char *match;
	char result[5] = {0};
	int ret;

	cmdline_node = of_find_node_by_path("/chosen");
	ret = of_property_read_string(cmdline_node, "bootargs", &cmd_line);
	if (ret) {
		info->disable_power_path = false;
		return;
	}

	if (strncmp(cmd_line, "charger", strlen("charger")) == 0)
		info->disable_power_path = true;

	match = strstr(cmd_line, "sprdboot.mode=");
	if (match) {
		memcpy(result, (match + strlen("sprdboot.mode=")), sizeof(result) - 1);
		if ((!strcmp(result, "cali")) || (!strcmp(result, "auto")))
			info->disable_power_path = true;

		if (!strcmp(result, "cali"))
			boot_calibration = true;
	}

	dev_info(info->dev, "disable_power_path=%d\n", info->disable_power_path);
}

static bool sc89601_charger_is_bat_present(struct sc89601_charger_info *info)
{
	struct power_supply *psy;
	union power_supply_propval val;
	bool present = false;
	int ret;

	psy = power_supply_get_by_name(SC89601_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to get psy of sc27xx_fgu\n");
		return present;
	}

	val.intval = 0;
	ret = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
					&val);
	if (ret == 0 && val.intval)
		present = true;
	power_supply_put(psy);

	if (ret)
		dev_err(info->dev,
			"Failed to get property of present:%d\n", ret);

	return present;
}

static int sc89601_charger_is_fgu_present(struct sc89601_charger_info *info)
{
	struct power_supply *psy;

	psy = power_supply_get_by_name(SC89601_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "Failed to find psy of sc27xx_fgu\n");
		return -ENODEV;
	}
	power_supply_put(psy);

	return 0;
}

static int sc89601_read(struct sc89601_charger_info *info, u8 reg, u8 *data)
{
	int ret;

	ret = i2c_smbus_read_byte_data(info->client, reg);
	if (ret < 0)
		return ret;

	*data = ret;
	return 0;
}

static int sc89601_write(struct sc89601_charger_info *info, u8 reg, u8 data)
{
	return i2c_smbus_write_byte_data(info->client, reg, data);
}

static int sc89601_update_bits(struct sc89601_charger_info *info, u8 reg, u8 mask, u8 data)
{
	u8 v;
	int ret;

	ret = sc89601_read(info, reg, &v);
	if (ret < 0)
		return ret;

	v &= ~mask;
	v |= (data & mask);

	return sc89601_write(info, reg, v);
}

static u32 sc89601_charger_get_limit_current(struct sc89601_charger_info *info, u32 *limit_cur)
{
	u8 reg_val;
	int ret;
	ret = sc89601_read(info, SC89601_REG_0, &reg_val);
	if (ret < 0)
		return ret;
	reg_val &= SC89601_REG_LIMIT_CURRENT_MASK;
	*limit_cur = reg_val * SC89601_REG_IINLIM_BASE * 1000;
	*limit_cur += SC89601_LIMIT_CURRENT_OFFSET;
	if (*limit_cur >= SC89601_LIMIT_CURRENT_MAX)
		*limit_cur = SC89601_LIMIT_CURRENT_MAX;
	return 0;
}

static int sc89601_charger_set_limit_current(struct sc89601_charger_info *info,
					     u32 limit_cur, bool enable)
{
	u8 reg_val;
	int ret = 0;
	static bool enable_flag = false;
	mutex_lock(&info->input_limit_cur_lock);
	if (enable) {
		ret = sc89601_charger_get_limit_current(info, &limit_cur);
		if (ret) {
			dev_err(info->dev, "get limit cur failed\n");
			goto out;
		}
		dev_info(info->dev, "%s, act_limit:%d, limit:%d \n", __func__, limit_cur, info->actual_limit_cur);
		if (limit_cur == info->actual_limit_cur)
			goto out;
		limit_cur = info->actual_limit_cur;
		dev_info(info->dev, "set limit current limit_cur = %d\n", limit_cur);
	}
	if (limit_cur >= SC89601_LIMIT_CURRENT_MAX)
		limit_cur = SC89601_LIMIT_CURRENT_MAX;
	else if (limit_cur < SC89601_LIMIT_CURRENT_OFFSET)
		limit_cur = SC89601_LIMIT_CURRENT_OFFSET;
	info->last_limit_cur = limit_cur;
	limit_cur = limit_cur / 1000;
	reg_val = (limit_cur - SC89601_REG_IINLIM_BASE) / SC89601_REG_IINLIM_BASE;
	info->actual_limit_cur = reg_val * SC89601_REG_IINLIM_BASE * 1000;
	info->actual_limit_cur += SC89601_LIMIT_CURRENT_OFFSET;
	ret = sc89601_update_bits(info, SC89601_REG_0,
				  SC89601_REG_LIMIT_CURRENT_MASK,
				  reg_val);
	if (ret)
		dev_err(info->dev, "set sc89601 limit cur failed\n");

	if (limit_cur <= SC89601_REG_IINLIM_BASE) {
		ret = 	sc89601_charger_set_vindpm(info, 8400);
		if (ret)
			dev_err(info->dev, "%s set sc89601 vindpm vol 8400 failed\n", __func__);
		else
			dev_err(info->dev, "%s set sc89601 vindpm vol 8400 success\n", __func__);
		if (true == sc27xx_get_try_sink_flag()) {
			enable_flag = false;
			ret =  sc89601_enable_charger(info, false);
			if (ret)
				dev_err(info->dev, "%s sc89601_disable_charger failed\n", __func__);
			else
				dev_err(info->dev, "%s sc89601_disable_charger success\n", __func__);
		}
	} else {
		ret = sc89601_charger_set_vindpm(info, info->voltage_max_microvolt);
		if (ret)
			dev_err(info->dev, "%s set sc89601 vindpm vol about %d failed\n", __func__, info->voltage_max_microvolt);
		else
			dev_err(info->dev, "%s set sc89601 vindpm vol about %d success\n", __func__, info->voltage_max_microvolt);
		if (true == sc27xx_get_try_sink_flag()) {
			if (false == enable_flag) {
				enable_flag = true;
				ret =  sc89601_enable_charger(info, true);
				if (ret)
					dev_err(info->dev, "%s sc89601_enable_charger failed\n", __func__);
				else
					dev_err(info->dev, "%s sc89601_enable_charger success\n", __func__);
			}
		}
	}

	dev_info(info->dev, "set limit_cur = %d, reg_val = %#x, actual_limit_cur = %d\n",
		 limit_cur, reg_val, info->actual_limit_cur);

out:
	mutex_unlock(&info->input_limit_cur_lock);

	return ret;
}

static int sc89601_charger_set_ovp(struct sc89601_charger_info *info, u32 vol)
{
	u8 reg_val;

	dev_info(info->dev, "%s:line%d: set ovp vol = %d\n", __func__, __LINE__, vol);
	if (vol < 5800)
		reg_val = SC89601_REG_OVP_5P8V;
	else if (vol > 5800 && vol < 6400)
		reg_val = SC89601_REG_OVP_6P4V;
	else if (vol > 6400 && vol < 11000)
		reg_val = SC89601_REG_OVP_11V;
	else
		reg_val = SC89601_REG_OVP_14P2V;
	return sc89601_update_bits(info, SC89601_REG_6,
				   SC89601_REG_OVP_MASK,
				   reg_val << SC89601_REG_OVP_SHIFT);
}

static int sc89601_charger_set_dpm(struct sc89601_charger_info *info)
{
	return sc89601_update_bits(info, SC89601_REG_A,
				   SC89601_REGA_DPM_INT_MASK,
				   SC89601_REGA_DPM_VALUE);
}

static int sc89601_enable_charger(struct sc89601_charger_info *info, bool enable)
{
	u8 val = SC89601_REG_CHG_DISABLE;

	if (enable)
		val = SC89601_REG_CHG_ENABLE;
	return sc89601_update_bits(info, SC89601_REG_1, SC89601_REG_CHG_MASK,
				   val << SC89601_REG_CHG_SHIFT);
}

static int sc89601_check_charge_full(struct sc89601_charger_info *info)
{
	int ret = 0;
	u8 reg_val = 0;
	ret = sc89601_read(info, SC89601_REG_8, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= SC89601_REG08_CHRG_STAT_MASK;
	reg_val = reg_val >> SC89601_REG08_CHRG_STAT_SHIFT;
	if (reg_val == SC89601_REG08_CHRG_STAT_CHGDONE){
		pr_err("%s:charge is done, recharge in probe!\n",__func__);
		ret = sc89601_enable_charger(info, false);
		if (ret) {
			dev_err(info->dev, "%s, failed to disable charger, ret = %d\n", __func__, ret);
			return ret;
		}
		msleep(10);
		ret = sc89601_enable_charger(info, true);
		if (ret) {
			dev_err(info->dev, "%s, failed to enable charger, ret = %d\n", __func__, ret);
			return ret;
		}
	}
	return ret;
}

static int sc89601_enable_pfm(struct sc89601_charger_info *info, bool enable)
{
	u8 val = SC89601_REG01_PFM_ENABLE;

	if (!enable)
		val = SC89601_REG01_PFM_DISABLE;
	return sc89601_update_bits(info, SC89601_REG_1, SC89601_REG01_PFM_DIS_MASK,
				   val << SC89601_REG01_PFM_DIS_SHIFT);
}

static int sc89601_charger_set_shipmode(struct sc89601_charger_info *info, bool en)
{
	int ret = 0;

  	pr_info("%s:shipmode en=%d\n", __func__, en);
  	if (en) {
  		ret = sc89601_update_bits(info, SC89601_REG_7, SC89601_REG7_BATFET_DLY_MASK, 0);
  		ret |= sc89601_update_bits(info, SC89601_REG_7, SC89601_REG7_BATFET_DIS_MASK, 1 << SC89601_REG7_BATFET_DIS_SHIFT);
  	} else {
  		ret |= sc89601_update_bits(info, SC89601_REG_7, SC89601_REG7_BATFET_DIS_MASK, 0);
  	}
  	return ret;
}

static int sc89601_charger_set_vindpm(struct sc89601_charger_info *info, u32 vol)
{
	u8 reg_val;

	if (vol >= SC89601_REG_VINDPM_MAX_8P4V) {
		reg_val = 0x0f;
		goto vdpm_end;
	} else if (vol >= SC89601_REG_VINDPM_MAX_8P2V) {
		reg_val = 0x0e;
		goto vdpm_end;
	} else if (vol >= SC89601_REG_VINDPM_MAX_8P0V) {
		reg_val = 0x0d;
		goto vdpm_end;
	} else if (vol >= SC89601_REG_VINDPM_MAX_5P1V) {
		vol = SC89601_REG_VINDPM_MAX_5P1V;
	} else if (vol <= SC89601_REG_VINDPM_MIN_3P9V) {
		vol = SC89601_REG_VINDPM_MIN_3P9V;
	}

	reg_val = (vol - SC89601_REG_VINDPM_OFFSET_3P9V) / SC89601_REG_VINDPM_LSB;

vdpm_end:
	return sc89601_update_bits(info, SC89601_REG_6,
				   SC89601_REG_VINDPM_VOLTAGE_MASK, reg_val);
}

static int sc89601_get_charger_vindpm_state(struct sc89601_charger_info *info, bool *vindpm_stat)
{
	u8 reg_val = 0;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	ret = sc89601_read(info, SC89601_REG_A, &reg_val);
	if (ret < 0)
		return ret;

	*vindpm_stat = (reg_val & SC89601_VINDPM_STATE_MASK) >> SC89601_VINDPM_STATE_SHIFT;
	if(*vindpm_stat == false) {
		*vindpm_stat = !info->vbus_gd;
		dev_info(info->dev,  "%s: reg0A = 0x%x, vbus_gd: %d \n", __func__, reg_val, info->vbus_gd);
	}

	return ret;
}

static int sc89601_charger_set_termina_vol(struct sc89601_charger_info *info, u32 vol)
{
	u8 reg_val, vreg_ft;
	int ret = 0;

	dev_info(info->dev, "%s:line%d: set termina vol = %d\n", __func__, __LINE__, vol);
	if (vol < SC89601_REG_TERMINAL_VOLTAGE_BASE)
		vol = SC89601_REG_TERMINAL_VOLTAGE_BASE;
	else if (vol > SC89601_REG_TERMINAL_VOLTAGE_MAX)
		vol = SC89601_REG_TERMINAL_VOLTAGE_MAX;

	/*
	 * Reason for rounding up: Avoid insufficiency problem.
	 *
	 * Example: Assume that the battery charging limit voltage
	 *          is 4.43V. If it is rounded down, the actual
	 *          charging limit voltage will be 4.40V, and the
	 *          problem of insufficient charging will occur.
	 */

	if (((vol - SC89601_REG_TERMINAL_VOLTAGE_BASE) % SC89601_REG_TERMINAL_VOLTAGE_LSB) / 8 == 1) {
		vol -= 8;
		vreg_ft = SC89601_REG0D_VREG_FT_INC8MV;
	}
	else if (((vol - SC89601_REG_TERMINAL_VOLTAGE_BASE) % SC89601_REG_TERMINAL_VOLTAGE_LSB) / 8 == 2) {
		vol -= 16;
		vreg_ft = SC89601_REG0D_VREG_FT_INC16MV;
	}
	else if (((vol - SC89601_REG_TERMINAL_VOLTAGE_BASE) % SC89601_REG_TERMINAL_VOLTAGE_LSB) / 8 == 3) {
		vol -= 24;
		vreg_ft = SC89601_REG0D_VREG_FT_INC24MV;
	}
	else
		vreg_ft = SC89601_REG0D_VREG_FT_DEFAULT;

	reg_val = (vol - SC89601_REG_TERMINAL_VOLTAGE_BASE) / SC89601_REG_TERMINAL_VOLTAGE_LSB;
	reg_val <<= SC89601_REG_TERMINAL_VOLTAGE_SHIFT;

	ret = sc89601_update_bits(info, SC89601_REG_4, SC89601_REG_TERMINAL_VOLTAGE_MASK, reg_val);
	if (ret)
		dev_err(info->dev, "set sc89601 termina_vol failed\n");
	ret = sc89601_update_bits(info, SC89601_REG_D, SC89601_REG0D_VBAT_REG_FT_MASK,
								vreg_ft << SC89601_REG0D_VBAT_REG_FT_SHIFT);
	if (ret)
		dev_err(info->dev, "set sc89601 termina_vol SC89601_REG_D failed\n");
	else
		dev_err(info->dev, "set SC89601_REG_D bit[7:6]: 0x%x\n", vreg_ft);

	return ret;
}

static int sc89601_charger_set_termina_cur(struct sc89601_charger_info *info, u32 cur)
{
	u8 reg_val;

	dev_info(info->dev, "%s:line%d: set termina cur = %d\n", __func__, __LINE__, cur);

	if (cur <= 60)
		reg_val = 0x0;
	else if (cur >= 960)
		reg_val = 0xf;
	else
		reg_val = (cur - 60) / 60;

	return sc89601_update_bits(info, SC89601_REG_3,
				   SC89601_REG_TERMINAL_CUR_MASK,
				   reg_val);
}

static int sc89601_charger_set_iprechg_cur(struct sc89601_charger_info *info, u32 cur)
{
	u8 reg_val;

	dev_info(info->dev, "%s:line%d: set iprechg cur = %d\n", __func__, __LINE__, cur);

	if (cur <= 60)
		reg_val = 0x0;
	else if (cur >= 480)
		reg_val = 0x8;
	else
		reg_val = (cur - 60) / 60;

	return sc89601_update_bits(info, SC89601_REG_3,
				   SC89601_REG_IPRECHG_CUR_MASK,
				   reg_val);
}

static int sc89601_charger_hw_init(struct sc89601_charger_info *info)
{
	struct sprd_battery_info bat_info = {};
	int termination_cur, iprechg_cur;
	int ret;

	pr_info("%s:line%d +++\n", __func__, __LINE__);
	ret = sprd_battery_get_battery_info(info->psy_usb, &bat_info, 0);
	if (ret) {
		dev_warn(info->dev, "no battery information is supplied\n");
		info->cur.sdp_limit = 500000;
		info->cur.sdp_cur = 500000;
		info->cur.dcp_limit = 2000000;
		info->cur.dcp_cur = 2000000;
		info->cur.cdp_limit = 900000;
		info->cur.cdp_cur = 900000;
		info->cur.unknown_limit = 1000000;
		info->cur.unknown_cur = 1000000;
		/*
		 * If no battery information is supplied, we should set
		 * default charge termination current to 120 mA, and default
		 * charge termination voltage to 4.44V.
		 */
		info->voltage_max_microvolt = 4440;
		termination_cur = 120;
		info->termination_cur = termination_cur;
		iprechg_cur = 240;
		info->iprechg_cur = iprechg_cur;
	} else {
		info->cur.sdp_limit = bat_info.cur.sdp_limit;
		info->cur.sdp_cur = bat_info.cur.sdp_cur;
		info->cur.dcp_limit = bat_info.cur.dcp_limit;
		info->cur.dcp_cur = bat_info.cur.dcp_cur;
		info->cur.cdp_limit = bat_info.cur.cdp_limit;
		info->cur.cdp_cur = bat_info.cur.cdp_cur;
		info->cur.unknown_limit = bat_info.cur.unknown_limit;
		info->cur.unknown_cur = bat_info.cur.unknown_cur;
		info->cur.fchg_limit = bat_info.cur.fchg_limit;
		info->cur.fchg_cur = bat_info.cur.fchg_cur;
		info->voltage_max_microvolt = bat_info.constant_charge_voltage_max_uv / 1000;
		iprechg_cur = bat_info.precharge_current_ua / 1000;
		info->iprechg_cur = iprechg_cur;
		termination_cur = bat_info.charge_term_current_ua / 1000;
		info->termination_cur = termination_cur;
		sprd_battery_put_battery_info(info->psy_usb, &bat_info);
	}

	ret =sc89601_charger_set_dpm(info);
	if (ret) {
		dev_err(info->dev, "set sc89601 vindpm&&iindpm failed\n");
		return ret;
	}

	if (info->role == SC89601_ROLE_MASTER_DEFAULT) {
		ret = sc89601_charger_set_ovp(info, SC89601_FCHG_OVP_6V);
		if (ret) {
			dev_err(info->dev, "set sc89601 ovp failed\n");
			return ret;
		}
	} else if (info->role == SC89601_ROLE_SLAVE) {
		ret = sc89601_charger_set_ovp(info, SC89601_FCHG_OVP_9V);
		if (ret) {
			dev_err(info->dev, "set sc89601 slave ovp failed\n");
			return ret;
		}
	}
	ret = sc89601_charger_set_vindpm(info, info->voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sc89601 vindpm vol failed\n");
		return ret;
	}
	ret = sc89601_charger_set_termina_vol(info, info->voltage_max_microvolt);
	if (ret) {
		dev_err(info->dev, "set sc89601 terminal vol failed\n");
		return ret;
	}
	ret = sc89601_charger_set_termina_cur(info, termination_cur);
	if (ret) {
		dev_err(info->dev, "set sc89601 terminal cur failed\n");
		return ret;
	}
	ret = sc89601_charger_set_iprechg_cur(info, iprechg_cur);
	if (ret) {
		dev_err(info->dev, "set sc89601 iprechg cur failed\n");
		return ret;
	}
	ret = sc89601_charger_set_limit_current(info, info->cur.unknown_cur, false);
	if (ret)
		dev_err(info->dev, "set sc89601 limit current failed\n");

	ret = sc89601_update_bits(info, SC89601_REG_7,
				  SC89601_DISABLE_BATFET_RST_MASK,
				  0x0 << SC89601_DISABLE_BATFET_RST_SHIFT);
	if (ret)
		dev_err(info->dev, "disable batfet_rst_en failed\n");
	ret = sc89601_update_bits(info, SC89601_REG_5, SC89601_REG_EN_TIMER_MASK,
				0 << SC89601_REG_EN_TIMER_SHIFT);
	if (ret)
		dev_err(info->dev, "fail to set SC89601_REG_EN_TIMER_MASK, ret = %d\n", ret);
	ret = sc89601_update_bits(info, SC89601_REG_4,
				SC89601_REG4_VRECHG_MASK,
				SC89601_REG4_VRECHG_200mV);

	if (ret)
		dev_err(info->dev, "set vrechg failed\n");

	ret = sc89601_update_bits(info, SC89601_REG_6,
				SC89601_REG6_BOOSTV_MASK,
				SC89601_REG6_BOOSTV_5P3V);
	if (ret)
		dev_err(info->dev, "set boostv failed\n");

	info->current_charge_limit_cur = SC89601_REG_ICHG_LSB * 1000;
	info->current_input_limit_cur = SC89601_REG_IINDPM_LSB * 1000;

	pr_info("%s:line%d ---\n", __func__, __LINE__);
	return ret;
}

static int sc89601_charger_get_charge_voltage(struct sc89601_charger_info *info, u32 *charge_vol)
{
	struct power_supply *psy;
	union power_supply_propval val;
	int ret;

	psy = power_supply_get_by_name(SC89601_BATTERY_NAME);
	if (!psy) {
		dev_err(info->dev, "failed to get SC89601_BATTERY_NAME\n");
		return -ENODEV;
	}

	val.intval = 0;
	ret = power_supply_get_property(psy,
					POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE,
					&val);
	power_supply_put(psy);
	if (ret) {
		dev_err(info->dev, "failed to get CONSTANT_CHARGE_VOLTAGE\n");
		return ret;
	}

	*charge_vol = val.intval;

	return 0;
}

static void sc89601_dump_register(struct sc89601_charger_info *info)
{
	int i, ret, len, idx = 0;
	u8 reg_val;
	char buf[256];

	memset(buf, '\0', sizeof(buf));
	for (i = 0; i < SC89601_REG_NUM; i++) {
		if(i == 0x0e)
                  	continue;
		ret = sc89601_read(info,  reg_tab[i].addr, &reg_val);
		if (ret == 0) {
			if(i == SC89601_REG_A)
				info->vbus_gd = reg_val & 0x80;
			len = snprintf(buf + idx, sizeof(buf) - idx,
				       "[REG_0x%.2x]=0x%.2x  ",
				       reg_tab[i].addr, reg_val);
			idx += len;
		}
	}
	dev_info(info->dev, "%s: %s", __func__, buf);
}

static int sc89601_charger_enable_wdg(struct sc89601_charger_info *info, bool en)
{
	u8 val = SC89601_WDT_DISABLE;

	if (en)
		val = SC89601_WDT_40S;
	return sc89601_update_bits(info, SC89601_REG_5,
				   SC89601_REG_WATCHDOG_TIMER_MASK,
				   val << SC89601_REG_WATCHDOG_TIMER_SHIFT);
}

static int sc89601_charger_start_charge(struct sc89601_charger_info *info)
{
	int ret = 0;

	ret = sc89601_update_bits(info, SC89601_REG_0,
				  SC89601_REG_EN_HIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed\n");

	ret = sc89601_charger_enable_wdg(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	if (info->role == SC89601_ROLE_MASTER_DEFAULT) {
		ret = regmap_update_bits(info->pmic, info->charger_pd,
					 info->charger_pd_mask, 0);
		if (ret) {
			dev_err(info->dev, "enable sc89601 charge failed\n");
			return ret;
		}
	} else if (info->role == SC89601_ROLE_SLAVE) {
		gpiod_set_value_cansleep(info->gpiod, 0);
	}

	ret = sc89601_enable_charger(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable charger, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sc89601_charger_set_limit_current(info, info->last_limit_cur, false);
	if (ret) {
		dev_err(info->dev, "failed to set limit current\n");
		return ret;
	}

	ret = sc89601_charger_set_termina_cur(info, info->termination_cur);
	if (ret)
		dev_err(info->dev, "set sc89601 terminal cur failed\n");

	ret = sc89601_update_bits(info, SC89601_REG_5, SC89601_REG_EN_TIMER_MASK,
				0 << SC89601_REG_EN_TIMER_SHIFT);
	if (ret)
		dev_err(info->dev, "fail to set SC89601_REG_EN_TIMER_MASK, ret = %d\n", ret);
	ret = sc89601_update_bits(info, SC89601_REG_4,
				SC89601_REG4_VRECHG_MASK,
				SC89601_REG4_VRECHG_200mV);
	if (ret)
		dev_err(info->dev, "set vrechg failed\n");

	return ret;
}

static void sc89601_charger_stop_charge(struct sc89601_charger_info *info, bool present)
{
	int ret;

	dev_info(info->dev, "%s:line%d: stop charge\n", __func__, __LINE__);

	ret = sc89601_enable_charger(info, false);
	if (ret)
		dev_err(info->dev, "%s, disable charger failed, ret = %d\n", __func__, ret);
	if (info->role == SC89601_ROLE_MASTER_DEFAULT) {
		if ((!present || info->need_disable_Q1) && boot_calibration) {
			ret = sc89601_update_bits(info, SC89601_REG_0,
						  SC89601_REG_EN_HIZ_MASK,
						  0x01 << SC89601_REG_EN_HIZ_SHIFT);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");
			info->need_disable_Q1 = false;
		}
		ret = regmap_update_bits(info->pmic, info->charger_pd, info->charger_pd_mask,
					 info->charger_pd_mask);
		if (ret)
			dev_err(info->dev, "disable sc89601 charge_pd failed\n");
	} else if (info->role == SC89601_ROLE_SLAVE) {
		if (!present && boot_calibration) {
			ret = sc89601_update_bits(info, SC89601_REG_0, SC89601_REG_EN_HIZ_MASK,
						0x01 << SC89601_REG_EN_HIZ_SHIFT);
			if (ret)
				dev_err(info->dev, "enable HIZ mode failed\n");
		}
		gpiod_set_value_cansleep(info->gpiod, 1);
	}

	if (info->disable_power_path) {
		ret = sc89601_update_bits(info, SC89601_REG_0,
					  SC89601_REG_EN_HIZ_MASK,
					  0x01 << SC89601_REG_EN_HIZ_SHIFT);
		if (ret)
			dev_err(info->dev, "Failed to disable power path\n");
	}

	ret = sc89601_charger_enable_wdg(info, false);
	if (ret)
		dev_err(info->dev, "%s, failed to disable watchdog, ret = %d\n", __func__, ret);
}

static int sc89601_charger_set_current(struct sc89601_charger_info *info, u32 cur)
{
	u8 reg_val;

	dev_info(info->dev, "%s:line%d: set ibat cur = %d\n", __func__, __LINE__, cur);

	cur = cur / 1000;
	if (cur > 3000) {
		reg_val = 0x30;
	} else {
		reg_val = cur / SC89601_REG_ICHG_LSB;
		reg_val &= SC89601_REG_ICHG_MASK;
	}

	return sc89601_update_bits(info, SC89601_REG_2,
				   SC89601_REG_ICHG_MASK,
				   reg_val);
}

static int sc89601_charger_get_current(struct sc89601_charger_info *info, u32 *cur)
{
	u8 reg_val;
	int ret;

	ret = sc89601_read(info, SC89601_REG_2, &reg_val);
	if (ret < 0)
		return ret;

	reg_val &= SC89601_REG_ICHG_MASK;
	*cur = reg_val * SC89601_REG_ICHG_LSB * 1000;

	return 0;
}

static int sc89601_charger_get_health(struct sc89601_charger_info *info, u32 *health)
{
	*health = POWER_SUPPLY_HEALTH_GOOD;

	return 0;
}

static int sc89601_charger_feed_watchdog(struct sc89601_charger_info *info)
{
	int ret = 0;
	u8 reg_val = SC89601_REG_WD_RST << SC89601_REG_WD_RST_SHIFT;
	u64 duration, curr = ktime_to_ms(ktime_get());

	dev_info(info->dev, "%s, start\n", __func__);

	ret = sc89601_update_bits(info, SC89601_REG_1,
				  SC89601_REG_WD_RST_MASK,
				  reg_val);
	if (ret) {
		dev_err(info->dev, "reset sc89601 failed\n");
		return ret;
	}

	duration = curr - info->last_wdt_time;
	if (duration >= SC89601_WATCH_DOG_TIME_OUT_MS) {
		dev_err(info->dev, "charger wdg maybe time out:%lld ms\n", duration);
		sc89601_dump_register(info);
	}

	info->last_wdt_time = curr;

	if (info->otg_enable)
		return ret;

	ret = sc89601_charger_set_limit_current(info, info->actual_limit_cur, true);
	if (ret)
		dev_err(info->dev, "set limit cur failed\n");

	return ret;
}

/*
static irqreturn_t sc89601_int_handler(int irq, void *dev_id)
{
	struct sc89601_charger_info *info = dev_id;
	int ret = 0;
	u8 reg_val = 0;
	static bool last_vbus_gd = false;
 	//bool dpdm_det_done = false;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return IRQ_HANDLED;
	}
	ret = sc89601_read(info, SC89601_REG_A, &reg_val);
	info->vbus_gd = reg_val & 0x80;
	if (last_vbus_gd == 0 && info->vbus_gd == 1) {
		sprd_hsphy_set_high_impedance_state();
		dev_info(info->dev, "vbus rise, swtich dpdm to charge\n");
	}
#if 0
	ret = sc89601_read(info, SC89601_REG_8, &reg_val);
	dpdm_det_done = reg_val & 0x04;
	if (dpdm_det_done) {
		sprd_hsphy_cancel_high_impedance_state();
		dev_info(info->dev, "bc12 detect done, swtich dpdm to usb\n");
	}
#endif
	last_vbus_gd = info->vbus_gd;
	sc89601_dump_register(info);

	return IRQ_HANDLED;
}
*/

static int sc89601_charger_get_status(struct sc89601_charger_info *info)
{
	if (info->charging)
		return POWER_SUPPLY_STATUS_CHARGING;
	else
		return POWER_SUPPLY_STATUS_NOT_CHARGING;
}

static bool sc89601_charger_get_power_path_status(struct sc89601_charger_info *info)
{
	u8 value;
	int ret;
	bool power_path_enabled = true;
	ret = sc89601_read(info, SC89601_REG_0, &value);
	if (ret < 0) {
		dev_err(info->dev, "Fail to get power path status, ret = %d\n", ret);
		return power_path_enabled;
	}
	if (value & SC89601_REG_EN_HIZ_MASK)
		power_path_enabled = false;
        dev_info(info->dev, "get HIZ mode: %s\n", power_path_enabled ? "Disable" : "Enable");
	return power_path_enabled;
}

static int sc89601_charger_set_power_path_status(struct sc89601_charger_info *info, bool enable)
{
	int ret = 0;
	u8 value = 0x1;
	dev_info(info->dev, "set HIZ mode: %s\n", enable ? "Disable" : "Enable");

	if (enable)
		value = 0;
	ret = sc89601_update_bits(info, SC89601_REG_0,
				  SC89601_REG_EN_HIZ_MASK,
				  value << SC89601_REG_EN_HIZ_SHIFT);
	if (ret)
		dev_err(info->dev, "%s HIZ mode failed, ret = %d\n",
			enable ? "Enable" : "Disable", ret);
	return ret;
}

static int sc89601_charger_check_power_path_status(struct sc89601_charger_info *info)
{
	int ret = 0;
	if (info->disable_power_path)
		return 0;
	if (sc89601_charger_get_power_path_status(info))
		return 0;
	dev_info(info->dev, "%s:line%d, disable HIZ\n", __func__, __LINE__);
	ret = sc89601_update_bits(info, SC89601_REG_0,
				  SC89601_REG_EN_HIZ_MASK, 0);
	if (ret)
		dev_err(info->dev, "disable HIZ mode failed, ret = %d\n", ret);
	return ret;
}

static void sc89601_check_wireless_charge(struct sc89601_charger_info *info, bool enable)
{
	int ret;
	if (!enable)
		cancel_delayed_work_sync(&info->cur_work);
	if (info->is_wireless_charge && enable) {
		cancel_delayed_work_sync(&info->cur_work);
		ret = sc89601_charger_set_current(info, info->current_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		ret = sc89601_charger_set_current(info, info->current_input_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);

		pm_wakeup_event(info->dev, SC89601_WAKE_UP_MS);
		schedule_delayed_work(&info->cur_work, msecs_to_jiffies(SC89601_CURRENT_WORK_MS));
	} else if (info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = info->current_charge_limit_cur;
		info->current_charge_limit_cur = SC89601_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = info->current_input_limit_cur;
		info->current_input_limit_cur = SC89601_REG_IINDPM_LSB * 1000;
	} else if (!info->is_wireless_charge && !enable) {
		info->new_charge_limit_cur = SC89601_REG_ICHG_LSB * 1000;
		info->current_charge_limit_cur = SC89601_REG_ICHG_LSB * 1000;
		info->new_input_limit_cur = SC89601_REG_IINDPM_LSB * 1000;
		info->current_input_limit_cur = SC89601_REG_IINDPM_LSB * 1000;
	}
}

static int sc89601_charger_set_status(struct sc89601_charger_info *info,
				      int val, u32 input_vol, bool bat_present)
{
	int ret = 0;

	if (val == CM_BUCK_MAX_TERMINA_VOL) {
		ret = sc89601_charger_set_termina_vol(info, SC89601_REG_TERMINAL_VOLTAGE_MAX);
		if (ret) {
			dev_err(info->dev, "failed to set terminate max voltage\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_MAX_OVP_ENABLE_CMD) {
		ret = sc89601_charger_set_ovp(info, SC89601_FCHG_OVP_14V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge max ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_OVP_ENABLE_CMD) {
		ret = sc89601_charger_set_ovp(info, SC89601_FCHG_OVP_9V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 9V ovp\n");
			return ret;
		}
	} else if (val == CM_FAST_CHARGE_OVP_DISABLE_CMD) {
		ret = sc89601_charger_set_ovp(info, SC89601_FCHG_OVP_6V);
		if (ret) {
			dev_err(info->dev, "failed to set fast charge 5V ovp\n");
			return ret;
		}
		if (info->role == SC89601_ROLE_MASTER_DEFAULT) {
			if (input_vol > SC89601_FAST_CHARGER_VOLTAGE_MAX)
				info->need_disable_Q1 = true;
		}
	} else if ((val == false) && (info->role == SC89601_ROLE_MASTER_DEFAULT)) {
		if (input_vol > SC89601_NORMAL_CHARGER_VOLTAGE_MAX)
			info->need_disable_Q1 = true;
	}

	if (val > CM_FAST_CHARGE_NORMAL_CMD)
		return 0;

	if (!val && info->charging) {
		sc89601_check_wireless_charge(info, false);
		sc89601_charger_stop_charge(info, bat_present);
		info->charging = false;
	} else if (val && !info->charging) {
		sc89601_check_wireless_charge(info, true);
		ret = sc89601_charger_start_charge(info);
		if (ret)
			dev_err(info->dev, "start charge failed\n");
		else
			info->charging = true;
	}

	return ret;
}

static void sc89601_current_work(struct work_struct *data)
{
	struct delayed_work *dwork = to_delayed_work(data);
	struct sc89601_charger_info *info =
		container_of(dwork, struct sc89601_charger_info, cur_work);
	int ret = 0, delay_work_ms = 10 * SC89601_CURRENT_WORK_MS;
	bool need_return = false;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (info->shutdown_flag)
		return;

	if (info->current_charge_limit_cur > info->new_charge_limit_cur) {
		ret = sc89601_charger_set_current(info, info->new_charge_limit_cur);
		if (ret < 0)
			dev_err(info->dev, "%s: set charge limit cur failed\n", __func__);
		return;
	}

	if (info->current_input_limit_cur > info->new_input_limit_cur) {
		ret = sc89601_charger_set_limit_current(info, info->new_input_limit_cur, false);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);
		return;
	}

	if (info->current_charge_limit_cur + SC89601_REG_ICHG_LSB * 1000 <=
	    info->new_charge_limit_cur)
		info->current_charge_limit_cur += SC89601_REG_ICHG_LSB * 1000;
	else
		need_return = true;

	if (info->current_input_limit_cur + SC89601_REG_IINDPM_LSB * 1000 <=
	    info->new_input_limit_cur)
		info->current_input_limit_cur += SC89601_REG_IINDPM_LSB * 1000;
	else if (need_return)
		return;

	ret = sc89601_charger_set_current(info, info->current_charge_limit_cur);
	if (ret < 0) {
		dev_err(info->dev, "set charge limit current failed\n");
		return;
	}

	ret = sc89601_charger_set_limit_current(info, info->current_input_limit_cur, false);
	if (ret < 0) {
		dev_err(info->dev, "set input limit current failed\n");
		return;
	}

	dev_info(info->dev, "set charge_limit_cur %duA, input_limit_curr %duA\n",
		 info->current_charge_limit_cur, info->current_input_limit_cur);

	if (info->current_charge_limit_cur < SC89601_WAIT_WL_VBUS_STABLE_CUR_THR)
		delay_work_ms = SC89601_CURRENT_WORK_MS * 50;

	schedule_delayed_work(&info->cur_work, msecs_to_jiffies(delay_work_ms));
}

static bool sc89601_probe_is_ready(struct sc89601_charger_info *info)
{
	unsigned long timeout;

	if (unlikely(!info->probe_initialized)) {
		timeout = wait_for_completion_timeout(&info->probe_init, SC89601_PROBE_TIMEOUT);
		if (!timeout) {
			dev_err(info->dev, "%s wait probe timeout\n", __func__);
			return false;
		}
	}

	return true;
}

static int sc89601_get_usb_online(struct sc89601_charger_info *info)
{
	int ret;
	bool vbus_gd = false;
	u8 reg_val = 0;
	int i;

	/* timeout: 20*5 = 100ms, if vbus is not good, wait 20ms retry */
	for (i = 0; i < 5; i++) {
		ret = sc89601_read(info, SC89601_REG_A, &reg_val);
		if (ret < 0) {
			dev_err(info->dev, "%s, read SC89601_REG_A failed, ret = %d\n", __func__, ret);
			return 0;
		}

		vbus_gd = (reg_val & SC89601_REGA_VBUS_GD_MASK) >> SC89601_REGA_VBUS_GD_SHIFT;
		if (vbus_gd) {
			dev_info(info->dev, "%s: vbus is good\n", __func__);
			return 1;
		}
		msleep(20);
	}

	dev_info(info->dev,  "%s: vbus is not good\n", __func__);
	return 0;
}

static int sc89601_get_first_bc1p2_done(struct sc89601_charger_info *info)
{
	u8 i;
	bool bc12_done = false;
	for (i = 0; i < 10; i++) {
		msleep(100);
		if (!sc89601_get_usb_online(info) && i > 3)
			return bc12_done;

		if(info->bc1p2_status == SC_BC1P2_COMPLETE)
		{
			bc12_done = true;
			break;
		}
	}
	return bc12_done;
}

static int sc89601_get_auto_bc1p2_done(struct sc89601_charger_info *info)
{
	int ret;
	u8 i;
	u8 force_dpdm_stat = 0;
	u8 pg_state = 0;
	bool bc12_done = false;

	dev_info(info->dev, "%s: bc1p2_status = %d\n", __func__, info->bc1p2_status);
	if(info->bc1p2_status == SC_BC1P2_COMPLETE)
		return true;
	else if(info->bc1p2_status == SC_BC1P2_UNKNOWN)
		return bc12_done;

	/* timeout: 100*10 = 1000ms, if bc1.2 is not done, wait 100ms retry */
	for (i = 0; i < 10; i++) {
		msleep(100);
		if (!sc89601_get_usb_online(info) && i > 3)
			return bc12_done;

		ret = sc89601_read(info, SC89601_REG_8, &pg_state);
		pg_state = (pg_state & 0x04) >> 2;
		dev_info(info->dev, "%s: pg stat = %d\n", __func__, pg_state);

		ret = sc89601_read(info, SC89601_REG_7, &force_dpdm_stat);
		force_dpdm_stat &= 0x80;
		dev_info(info->dev, "%s: force_dpdm = %d\n", __func__, force_dpdm_stat);

		if (!force_dpdm_stat && pg_state)
		{
			bc12_done = true;
			dev_info(info->dev, "%s: BC1.2 complete\n", __func__);
			goto bc1p2_end;
		}
	}
	dev_info(info->dev, "%s, auto bc1.2 is timeout\n", __func__);
	//sc8960x_force_dpdm(info);
	//info->bc12_recovery = true;

bc1p2_end:
	if(info->bc1p2_status == SC_BC1P2_FORCE)
	{
		mutex_lock(&info->bc_detect_lock);
		info->bc12_detect = false;
		info->bc12_recovery = false;
		info->bc1p2_status = SC_BC1P2_COMPLETE;
		mutex_unlock(&info->bc_detect_lock);
	}
	dev_info(info->dev, "%s: auto bc1.2 is done\n", __func__);
	return bc12_done;
}

/*
static int __attribute__((unused)) sc89601_get_force_bc1p2_done(struct sc89601_charger_info *info)
{
	int ret;
	bool bc12_done = false;

	bc12_done = sc89601_get_auto_bc1p2_done(info);

	ret = sc89601_update_bits(info, SC89601_REG_7, SC89601_REG7_IINDET_EN_MASK,
		SC89601_REG7_IINDET_EN_ENABLE << SC89601_REG7_IINDET_EN_SHIFT);
	if (ret < 0)
		dev_err(info->dev, "%s, Set IINDET_EN 1 failed, ret = %d\n", __func__, ret);

	msleep(100);
	bc12_done = sc89601_get_auto_bc1p2_done(info);

	return bc12_done;
}
*/

static int sc89601_get_bc1p2_result(struct sc89601_charger_info *info, bool flag)
{
	int ret;
	u8 reg_val = 0;
	int vbus_stat;
	int chg_type;

	ret = sc89601_read(info, SC89601_REG_8, &reg_val);
	if (ret < 0)
		dev_err(info->dev, "%s, read SC89601_REG_8 failed, ret = %d\n", __func__, ret);

	vbus_stat = (reg_val & SC89601_REG8_VBUS_STAT_MASK) >> SC89601_REG8_VBUS_STAT_SHIFT;
	switch (vbus_stat) {
	case SC89601_VBUS_TYPE_USB:
		chg_type = POWER_SUPPLY_USB_TYPE_SDP;
		break;
	case SC89601_VBUS_TYPE_CDP:
		chg_type = POWER_SUPPLY_USB_TYPE_CDP;
		break;
	case SC89601_VBUS_TYPE_ADAPTER:
		chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		break;
	case SC89601_VBUS_TYPE_UNKNOWN_ADAPTER:
		if (flag)
			chg_type = POWER_SUPPLY_USB_TYPE_C;
		else
			chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	case SC89601_VBUS_TYPE_NON_STANDARD_ADAPTER:
		if (flag)
			chg_type = POWER_SUPPLY_USB_TYPE_DCP;
		else
			chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	default:
		chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;
		break;
	}

	dev_info(info->dev, "%s: bool=%d, vbus_stat=%d, chg_type=%d\n", __func__, flag, vbus_stat, chg_type);
	return chg_type;
}

static int sc89601_charger_is_present(struct sc89601_charger_info *info)
{
	if (!info) {
		pr_err("%s: info is null\n", __func__);
		return 0;
	}
	return sc89601_get_usb_online(info);
}

static int sc89601_charger_chg_type_det(struct sc89601_charger_info *info, bool force_dpdm)
{
	int ret;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (!info) {
		pr_err("%s: info is null\n", __func__);
		return chg_type;
	}

	if (force_dpdm) {
		sprd_hsphy_set_high_impedance_state();
		msleep(100);
		sc8960x_force_dpdm(info);
		sprd_hsphy_cancel_high_impedance_state();
	} else {
		ret = sc89601_get_auto_bc1p2_done(info);
	}
	chg_type = sc89601_get_bc1p2_result(info, false);

	return chg_type;
}

static int sc89601_charger_get_vbus_stat(struct sc89601_charger_info *info)
{
	int ret;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (!info) {
		pr_err("%s: info is null\n", __func__);
		return chg_type;
	}

	if (!sc89601_get_usb_online(info))
		return chg_type;

	ret = sc89601_get_auto_bc1p2_done(info);
	chg_type = sc89601_get_bc1p2_result(info, true);

	return chg_type;
}

static int sc89601_charger_first_bc1p2(struct sc89601_charger_info *info, int *bc1p2_result)
{
	int ret = 0;
	int chg_type = POWER_SUPPLY_USB_TYPE_UNKNOWN;

	if (!info) {
		pr_err("%s: info is null\n", __func__);
		goto first_end;
	}
	dev_info(info->dev, "%s: charge irq is waitting\n", __func__);
	ret = sc89601_get_first_bc1p2_done(info);
	if (!ret) {
		dev_info(info->dev, "%s: bc1p2 process is timeout\n", __func__);
	}
	chg_type = sc89601_get_bc1p2_result(info, false);
first_end:
	*bc1p2_result = chg_type;
	return 0;
}

static int sc89601_charger_retry_bc1p2(struct sc89601_charger_info *info, int *bc1p2_result)
{
	*bc1p2_result = sc89601_charger_chg_type_det(info, true);
	return 0;
}

static int sc89601_charger_usb_get_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    union power_supply_propval *val)
{
	struct sc89601_charger_info *info = power_supply_get_drvdata(psy);
	u32 cur = 0, health, enabled = 0;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (unlikely(!psy->initialized && atomic_read(&psy->use_cnt) > 0)) {
		dev_err(info->dev, "%s psy is not ready\n", __func__);
		return -ENODEV;
	}

	if (!sc89601_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);
	switch (psp) {
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD ||
		    val->intval == CM_POWER_PATH_DISABLE_CMD) {
			val->intval = sc89601_charger_get_power_path_status(info);
			break;
		} else if (val->intval == CM_BUCK_MAX_TERMINA_VOL) {
			val->intval = SC89601_REG_TERMINAL_VOLTAGE_MAX * 1000;
			break;
		}

		val->intval = sc89601_charger_get_status(info);
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sc89601_charger_get_current(info, &cur);
			if (ret)
				goto out;
			val->intval = cur;
		}
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (!info->charging) {
			val->intval = 0;
		} else {
			ret = sc89601_charger_get_limit_current(info, &cur);
			if (ret)
				goto out;
			val->intval = cur;
		}
		break;
	case POWER_SUPPLY_PROP_HEALTH:
		if (info->charging) {
			val->intval = 0;
		} else {
			ret = sc89601_charger_get_health(info, &health);
			if (ret)
				goto out;

			val->intval = health;
		}
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		if (info->role == SC89601_ROLE_MASTER_DEFAULT) {
			ret = regmap_read(info->pmic, info->charger_pd, &enabled);
			if (ret) {
				dev_err(info->dev, "get sc89601 charge status failed\n");
				goto out;
			}
			val->intval = !(enabled & info->charger_pd_mask);
		} else if (info->role == SC89601_ROLE_SLAVE) {
			enabled = gpiod_get_value_cansleep(info->gpiod);
			val->intval = !enabled;
		}
		break;
	case POWER_SUPPLY_PROP_USB_TYPE:
		val->intval = sc89601_charger_chg_type_det(info, false);
		break;
	case POWER_SUPPLY_PROP_MANUFACTURER:
		val->strval = "sc89601";
		break;
	default:
		ret = -EINVAL;
	}

out:
	mutex_unlock(&info->lock);
	return ret;
}

static int sc89601_charger_usb_set_property(struct power_supply *psy,
					    enum power_supply_property psp,
					    const union power_supply_propval *val)
{
	struct sc89601_charger_info *info = power_supply_get_drvdata(psy);
	int ret = 0;
	u32 input_vol;
	bool bat_present;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (unlikely(!psy->initialized && atomic_read(&psy->use_cnt) > 0)) {
		dev_err(info->dev, "%s psy is not ready\n", __func__);
		return -ENODEV;
	}

	/*
	 * input_vol and bat_present should be assigned a value, only if psp is
	 * POWER_SUPPLY_PROP_STATUS and POWER_SUPPLY_PROP_CALIBRATE.
	 */
	if (psp == POWER_SUPPLY_PROP_STATUS || psp == POWER_SUPPLY_PROP_CALIBRATE) {
		bat_present = sc89601_charger_is_bat_present(info);
		ret = sc89601_charger_get_charge_voltage(info, &input_vol);
		if (ret) {
			input_vol = 0;
			dev_err(info->dev, "failed to get charge voltage! ret = %d\n", ret);
		}
	}

	if (!sc89601_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}

	mutex_lock(&info->lock);

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_charge_limit_cur = val->intval;
			pm_wakeup_event(info->dev, SC89601_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work,
					      msecs_to_jiffies(SC89601_CURRENT_WORK_MS * 2));
			break;
		}
		ret = sc89601_charger_set_current(info, val->intval);
		if (ret < 0)
			dev_err(info->dev, "set charge current failed\n");
		break;
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
		if (info->is_wireless_charge) {
			cancel_delayed_work_sync(&info->cur_work);
			info->new_input_limit_cur = val->intval;
			pm_wakeup_event(info->dev, SC89601_WAKE_UP_MS);
			schedule_delayed_work(&info->cur_work,
					      msecs_to_jiffies(SC89601_CURRENT_WORK_MS * 2));
			break;
		}
		ret = sc89601_charger_set_limit_current(info, val->intval, false);
		if (ret < 0)
			dev_err(info->dev, "set input current limit failed\n");
		break;
	case POWER_SUPPLY_PROP_STATUS:
		if (val->intval == CM_POWER_PATH_ENABLE_CMD) {
			ret = sc89601_charger_set_power_path_status(info, true);
			break;
		} else if (val->intval == CM_POWER_PATH_DISABLE_CMD) {
			ret = sc89601_charger_set_power_path_status(info, false);
			break;
		}
		ret = sc89601_charger_set_status(info, val->intval, input_vol, bat_present);
		if (ret < 0)
			dev_err(info->dev, "set charge status failed\n");
		break;
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_VOLTAGE_MAX:
		ret = sc89601_charger_set_termina_vol(info, val->intval / 1000);
		if (ret < 0)
			dev_err(info->dev, "failed to set terminate voltage\n");
		break;
	case POWER_SUPPLY_PROP_CALIBRATE:
		dev_info(info->dev, "POWER_SUPPLY_PROP_CHARGE_ENABLED = %d\n", val->intval);
		if (val->intval == true) {
			sc89601_check_wireless_charge(info, true);
			ret = sc89601_charger_start_charge(info);
			if (ret)
				dev_err(info->dev, "start charge failed\n");
		} else if (val->intval == false) {
			sc89601_check_wireless_charge(info, false);
			sc89601_charger_stop_charge(info, bat_present);
		}
		break;
	case POWER_SUPPLY_PROP_TYPE:
		if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_UNKNOWN) {
			info->is_wireless_charge = true;
			ret = sc89601_charger_set_ovp(info, SC89601_FCHG_OVP_6V);
		} else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_BPP) {
			info->is_wireless_charge = true;
			ret = sc89601_charger_set_ovp(info, SC89601_FCHG_OVP_6V);
		} else if (val->intval == POWER_SUPPLY_WIRELESS_CHARGER_TYPE_EPP) {
			info->is_wireless_charge = true;
			ret = sc89601_charger_set_ovp(info, SC89601_FCHG_OVP_14V);
		} else {
			info->is_wireless_charge = false;
			ret = sc89601_charger_set_ovp(info, SC89601_FCHG_OVP_6V);
		}
		if (ret)
			dev_err(info->dev, "failed to set fast charge ovp\n");
		break;
	case POWER_SUPPLY_PROP_PRESENT:
		info->is_charger_online = val->intval;
		if (val->intval == true) {
			info->last_wdt_time = ktime_to_ms(ktime_get());
			schedule_delayed_work(&info->wdt_work, 0);
		} else {
			info->actual_limit_cur = 0;
			cancel_delayed_work_sync(&info->wdt_work);
		}
		break;
	default:
		ret = -EINVAL;
	}

	mutex_unlock(&info->lock);
	return ret;
}

static int sc89601_charger_property_is_writeable(struct power_supply *psy,
						 enum power_supply_property psp)
{
	int ret;

	switch (psp) {
	case POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT:
	case POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT:
	case POWER_SUPPLY_PROP_CALIBRATE:
	case POWER_SUPPLY_PROP_TYPE:
	case POWER_SUPPLY_PROP_STATUS:
	case POWER_SUPPLY_PROP_PRESENT:
		ret = 1;
		break;
	default:
		ret = 0;
	}

	return ret;
}

static enum power_supply_property sc89601_usb_props[] = {
	POWER_SUPPLY_PROP_STATUS,
	POWER_SUPPLY_PROP_CONSTANT_CHARGE_CURRENT,
	POWER_SUPPLY_PROP_INPUT_CURRENT_LIMIT,
	POWER_SUPPLY_PROP_HEALTH,
	POWER_SUPPLY_PROP_CALIBRATE,
	POWER_SUPPLY_PROP_TYPE,
	POWER_SUPPLY_PROP_MANUFACTURER,
};

static const struct power_supply_desc sc89601_charger_desc = {
	.name			= "sc89601_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sc89601_usb_props,
	.num_properties		= ARRAY_SIZE(sc89601_usb_props),
	.get_property		= sc89601_charger_usb_get_property,
	.set_property		= sc89601_charger_usb_set_property,
	.property_is_writeable	= sc89601_charger_property_is_writeable,
};

static const struct power_supply_desc sc89601_slave_charger_desc = {
	.name			= "sc89601_slave_charger",
	.type			= POWER_SUPPLY_TYPE_UNKNOWN,
	.properties		= sc89601_usb_props,
	.num_properties		= ARRAY_SIZE(sc89601_usb_props),
	.get_property		= sc89601_charger_usb_get_property,
	.set_property		= sc89601_charger_usb_set_property,
	.property_is_writeable	= sc89601_charger_property_is_writeable,
};

static ssize_t sc89601_register_value_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sc89601_charger_sysfs *sc89601_sysfs =
		container_of(attr, struct sc89601_charger_sysfs,
			     attr_sc89601_reg_val);
	struct  sc89601_charger_info *info =  sc89601_sysfs->info;
	u8 val;
	int ret;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s  sc89601_sysfs->info is null\n", __func__);

	ret = sc89601_read(info, reg_tab[info->reg_id].addr, &val);
	if (ret) {
		dev_err(info->dev, "fail to get  SC89601_REG_0x%.2x value, ret = %d\n",
			reg_tab[info->reg_id].addr, ret);
		return snprintf(buf, PAGE_SIZE, "fail to get  SC89601_REG_0x%.2x value\n",
			       reg_tab[info->reg_id].addr);
	}

	return snprintf(buf, PAGE_SIZE, "SC89601_REG_0x%.2x = 0x%.2x\n",
			reg_tab[info->reg_id].addr, val);
}

static ssize_t sc89601_register_value_store(struct device *dev,
					    struct device_attribute *attr,
					    const char *buf, size_t count)
{
	struct sc89601_charger_sysfs *sc89601_sysfs =
		container_of(attr, struct sc89601_charger_sysfs,
			     attr_sc89601_reg_val);
	struct sc89601_charger_info *info = sc89601_sysfs->info;
	u8 val;
	int ret;

	if (!info) {
		dev_err(dev, "%s sc89601_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtou8(buf, 16, &val);
	if (ret) {
		dev_err(info->dev, "fail to get addr, ret = %d\n", ret);
		return count;
	}

	ret = sc89601_write(info, reg_tab[info->reg_id].addr, val);
	if (ret) {
		dev_err(info->dev, "fail to wite 0x%.2x to REG_0x%.2x, ret = %d\n",
				val, reg_tab[info->reg_id].addr, ret);
		return count;
	}

	dev_info(info->dev, "wite 0x%.2x to REG_0x%.2x success\n", val, reg_tab[info->reg_id].addr);
	return count;
}

static ssize_t sc89601_register_id_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t count)
{
	struct sc89601_charger_sysfs *sc89601_sysfs =
		container_of(attr, struct sc89601_charger_sysfs,
			     attr_sc89601_sel_reg_id);
	struct sc89601_charger_info *info = sc89601_sysfs->info;
	int ret, id;

	if (!info) {
		dev_err(dev, "%s sc89601_sysfs->info is null\n", __func__);
		return count;
	}

	ret =  kstrtoint(buf, 10, &id);
	if (ret) {
		dev_err(info->dev, "%s store register id fail\n", sc89601_sysfs->name);
		return count;
	}

	if (id < 0 || id >= SC89601_REG_NUM) {
		dev_err(info->dev, "%s store register id fail, id = %d is out of range\n",
			sc89601_sysfs->name, id);
		return count;
	}

	info->reg_id = id;
	dev_info(info->dev, "%s store register id = %d success\n", sc89601_sysfs->name, id);

	return count;
}

static ssize_t sc89601_register_id_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct sc89601_charger_sysfs *sc89601_sysfs =
		container_of(attr, struct sc89601_charger_sysfs,
			     attr_sc89601_sel_reg_id);
	struct sc89601_charger_info *info = sc89601_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sc89601_sysfs->info is null\n", __func__);

	return snprintf(buf, PAGE_SIZE, "Curent register id = %d\n", info->reg_id);
}

static ssize_t sc89601_register_table_show(struct device *dev,
					   struct device_attribute *attr,
					   char *buf)
{
	struct sc89601_charger_sysfs *sc89601_sysfs =
		container_of(attr, struct sc89601_charger_sysfs,
			     attr_sc89601_lookup_reg);
	struct sc89601_charger_info *info = sc89601_sysfs->info;
	int i, len, idx = 0;
	char reg_tab_buf[1024];

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sc89601_sysfs->info is null\n", __func__);

	memset(reg_tab_buf, '\0', sizeof(reg_tab_buf));
	len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
		       "Format: [id] [addr] [desc]\n");
	idx += len;
	for (i = 0; i < SC89601_REG_NUM; i++) {
		len = snprintf(reg_tab_buf + idx, sizeof(reg_tab_buf) - idx,
			       "[%d] [REG_0x%.2x] [%s];\n",
			       reg_tab[i].id, reg_tab[i].addr, reg_tab[i].name);
		idx += len;
	}

	return snprintf(buf, PAGE_SIZE, "%s\n", reg_tab_buf);
}

static ssize_t sc89601_dump_register_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	struct sc89601_charger_sysfs *sc89601_sysfs =
		container_of(attr, struct sc89601_charger_sysfs,
			     attr_sc89601_dump_reg);
	struct sc89601_charger_info *info = sc89601_sysfs->info;

	if (!info)
		return snprintf(buf, PAGE_SIZE, "%s sc89601_sysfs->info is null\n", __func__);

	sc89601_dump_register(info);
	return snprintf(buf, PAGE_SIZE, "%s\n", sc89601_sysfs->name);
}

static int sc89601_register_sysfs(struct sc89601_charger_info *info)
{
	struct sc89601_charger_sysfs *sc89601_sysfs;
	int ret;

	sc89601_sysfs = devm_kzalloc(info->dev, sizeof(*sc89601_sysfs), GFP_KERNEL);
	if (!sc89601_sysfs)
		return -ENOMEM;
	info->sysfs = sc89601_sysfs;
	sc89601_sysfs->name = "sc89601_sysfs";
	sc89601_sysfs->info = info;
	sc89601_sysfs->attrs[0] = &sc89601_sysfs->attr_sc89601_dump_reg.attr;
	sc89601_sysfs->attrs[1] = &sc89601_sysfs->attr_sc89601_lookup_reg.attr;
	sc89601_sysfs->attrs[2] = &sc89601_sysfs->attr_sc89601_sel_reg_id.attr;
	sc89601_sysfs->attrs[3] = &sc89601_sysfs->attr_sc89601_reg_val.attr;
	sc89601_sysfs->attrs[4] = NULL;
	sc89601_sysfs->attr_g.name = "debug";
	sc89601_sysfs->attr_g.attrs = sc89601_sysfs->attrs;
	sysfs_attr_init(&sc89601_sysfs->attr_sc89601_dump_reg.attr);
	sc89601_sysfs->attr_sc89601_dump_reg.attr.name = "sc89601_dump_reg";
	sc89601_sysfs->attr_sc89601_dump_reg.attr.mode = 0444;
	sc89601_sysfs->attr_sc89601_dump_reg.show = sc89601_dump_register_show;
	sysfs_attr_init(&sc89601_sysfs->attr_sc89601_lookup_reg.attr);
	sc89601_sysfs->attr_sc89601_lookup_reg.attr.name = "sc89601_lookup_reg";
	sc89601_sysfs->attr_sc89601_lookup_reg.attr.mode = 0444;
	sc89601_sysfs->attr_sc89601_lookup_reg.show = sc89601_register_table_show;
	sysfs_attr_init(&sc89601_sysfs->attr_sc89601_sel_reg_id.attr);
	sc89601_sysfs->attr_sc89601_sel_reg_id.attr.name = "sc89601_sel_reg_id";
	sc89601_sysfs->attr_sc89601_sel_reg_id.attr.mode = 0644;
	sc89601_sysfs->attr_sc89601_sel_reg_id.show = sc89601_register_id_show;
	sc89601_sysfs->attr_sc89601_sel_reg_id.store = sc89601_register_id_store;
	sysfs_attr_init(&sc89601_sysfs->attr_sc89601_reg_val.attr);
	sc89601_sysfs->attr_sc89601_reg_val.attr.name = "sc89601_reg_val";
	sc89601_sysfs->attr_sc89601_reg_val.attr.mode = 0644;
	sc89601_sysfs->attr_sc89601_reg_val.show = sc89601_register_value_show;
	sc89601_sysfs->attr_sc89601_reg_val.store = sc89601_register_value_store;
	ret = sysfs_create_group(&info->psy_usb->dev.kobj, &sc89601_sysfs->attr_g);
	if (ret < 0)
		dev_err(info->dev, "Cannot create sysfs , ret = %d\n", ret);

	return ret;
}

static void sc89601_charger_feed_watchdog_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc89601_charger_info *info = container_of(dwork,
							 struct sc89601_charger_info,
							 wdt_work);
	int ret;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	ret = sc89601_charger_feed_watchdog(info);
	if (ret)
		schedule_delayed_work(&info->wdt_work, HZ * 1);
	else
		schedule_delayed_work(&info->wdt_work, HZ * 15);
}

#if IS_ENABLED(CONFIG_REGULATOR)
static bool sc89601_charger_check_otg_valid(struct sc89601_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = false;

	ret = sc89601_read(info, SC89601_REG_1, &value);
	if (ret) {
		dev_err(info->dev, "get sc89601 charger otg valid status failed\n");
		return status;
	}

	if (value & SC89601_REG_OTG_MASK)
		status = true;
	else
		dev_err(info->dev, "otg is not valid, REG_1 = 0x%x\n", value);
	return status;
}

static bool sc89601_charger_check_otg_fault(struct sc89601_charger_info *info)
{
	int ret;
	u8 value = 0;
	bool status = true;
	ret = sc89601_read(info, SC89601_REG_9, &value);
	if (ret) {
		dev_err(info->dev, "get sc89601 charger otg fault status failed\n");
		return status;
	}
	if (!(value & SC89601_REG_BOOST_FAULT_MASK))
		status = false;
	else
		dev_err(info->dev, "boost fault occurs, REG_9 = 0x%x\n", value);

	return status;
}

static void sc89601_charger_otg_work(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct sc89601_charger_info *info = container_of(dwork,
			struct sc89601_charger_info, otg_work);
	bool otg_valid = sc89601_charger_check_otg_valid(info);
	bool otg_fault;
	int ret, retry = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (otg_valid)
		goto out;

	do {
		otg_fault = sc89601_charger_check_otg_fault(info);
		if (!otg_fault) {
			dev_info(info->dev, "%s:line%d:restart charger otg\n", __func__, __LINE__);
			ret = sc89601_update_bits(info, SC89601_REG_1,
						  SC89601_REG_OTG_MASK,
						  SC89601_REG_OTG_ENABLE << SC89601_REG_OTG_SHIFT
						  );

			if (ret)
				dev_err(info->dev, "restart sc89601 charger otg failed\n");
		}
		otg_valid = sc89601_charger_check_otg_valid(info);
	} while (!otg_valid && retry++ < SC89601_OTG_RETRY_TIMES);

	if (retry >= SC89601_OTG_RETRY_TIMES) {
		dev_err(info->dev, "Restart OTG failed\n");
		return;
	}

out:
	dev_info(info->dev, "%s:line%d:schedule_work\n", __func__, __LINE__);
	schedule_delayed_work(&info->otg_work, msecs_to_jiffies(1500));
}

static int sc89601_get_key_status(struct sc89601_charger_info *info) {
	u8 reg_val;
	int ret;
	ret = sc89601_read(info,SC89601_REG_7F, &reg_val);
	if(ret < 0){
		dev_err(info->dev, "%s: sc89601 read reg 0x7f fail: %d\n", __func__, ret);
        return ret;
	}
	return reg_val;
}

static int sc89601_set_key(struct sc89601_charger_info *info) {
	int ret;
	ret = sc89601_write(info, SC89601_REG_7F, REG7F_KEY1);
	ret = sc89601_write(info, SC89601_REG_7F, REG7F_KEY2);
	ret = sc89601_write(info, SC89601_REG_7F, REG7F_KEY3);
	ret = sc89601_write(info, SC89601_REG_7F, REG7F_KEY4);
	ret = sc89601_write(info, SC89601_REG_7F, REG7F_KEY5);
	ret = sc89601_write(info, SC89601_REG_7F, REG7F_KEY6);
	ret = sc89601_write(info, SC89601_REG_7F, REG7F_KEY7);
	return sc89601_write(info, SC89601_REG_7F, REG7F_KEY8);
}
static int sc89601_set_otg_2A(struct sc89601_charger_info *info, bool enable) {
	int ret;
	int reg_val = enable?1:0;
	if(enable) {
		ret = sc89601_get_key_status(info);
		if(!ret) {
			sc89601_set_key(info);
		}
		ret = sc89601_update_bits(info, THERMAL_LOOP_DIS,
							THERMAL_LOOP_DIS_MASK,
							1<<THERMAL_LOOP_DIS_SHIFT);
		if(ret) {
			dev_info(info->dev, "THERMAL_LOOP_DIS set 1 failed\n");
		}
		ret = sc89601_update_bits(info, IBUS_LOOP_DIS,
							IBUS_LOOP_DIS_MASK,
							1<<IBUS_LOOP_DIS_SHIFT);
		if(ret) {
			dev_info(info->dev, "close ibus loop failed\n");
		}
		sc89601_write(info, SC89601_REG_7F, REG93_VAL);
		ret = sc89601_set_key(info);
		mdelay(10);
		ret |= sc89601_update_bits(info, SC89601_REG_1,
						SC89601_REG_OTG_MASK,
						reg_val<<SC89601_REG_OTG_SHIFT);
		if(!ret)
			info->otg_2a_enable = true;

	}else{
		sc89601_update_bits(info,SC89601_REG_1,
					SC89601_REG_OTG_MASK,
					reg_val<<SC89601_REG_OTG_SHIFT);
		ret = sc89601_get_key_status(info);
		if(!ret) {
			sc89601_set_key(info);
		}
		ret = sc89601_update_bits(info, THERMAL_LOOP_DIS, THERMAL_LOOP_DIS_MASK, 0);
		if(ret) {
			dev_info(info->dev, "THERMAL_LOOP_DIS set 0 failed\n");
		}
		ret = sc89601_update_bits(info, IBUS_LOOP_DIS, IBUS_LOOP_DIS_MASK, 0);
		if(ret) {
			dev_info(info->dev, "close ibus loop 0 failed\n");
		}
		ret = sc89601_write(info, SC89601_REG_7F, REG93_VAL);
		ret |= sc89601_set_key(info);
		if(!ret)
			info->otg_2a_enable = false;
		dev_info(info->dev, "%s OTG iboost 2A  %s\n", enable ? "enable" : "disable",!ret ? "successfully" : "failed");
	}
	return ret;
}

static int sc89601_charger_enable_otg(struct regulator_dev *dev)
{
	int temp,capacity;
	struct power_supply *batt_psy=NULL;
	union power_supply_propval val;
	struct sc89601_charger_info *info = rdev_get_drvdata(dev);
	struct charger_manager *cm = NULL;
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->shutdown_flag)
		return ret;

	sc89601_charger_dump_stack();

	if (!sc89601_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}
	/*
	 * Disable charger detection function in case
	 * affecting the OTG timing sequence.
	 */
	if (!info->use_typec_extcon) {
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, BIT_DP_DM_BC_ENB);
		if (ret) {
			dev_err(info->dev, "failed to disable bc1.2 detect function.\n");
			return ret;
		}
	}
	batt_psy=power_supply_get_by_name("battery");
	if(!batt_psy)
		goto err;
	ret = power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_TEMP, &val);
	temp = val.intval;
	ret |= power_supply_get_property(batt_psy, POWER_SUPPLY_PROP_CAPACITY, &val);
	capacity = val.intval;
	if(ret)
		goto err;
	if(temp > 0 && capacity > 15 && capacity <= 40) {
		ret |= sc89601_update_bits(info,SC89601_REG_2,
						SC89601_REG_BOOST_LIMIT_MASK,
						1<<SC89601_REG_BOOST_LIMIT_SHIFT);
	}else if(temp > 0 && capacity > 40) {
		//ret = sc89601_update_bits(info, 0x0e, GENMASK(3,3), 1<<3);
		ret = sc89601_set_otg_2A(info, true);
	}else {
		ret |= sc89601_update_bits(info, SC89601_REG_2,
							SC89601_REG_BOOST_LIMIT_MASK,0);
	}

	cm = power_supply_get_drvdata(batt_psy);

	if (IS_ERR_OR_NULL(cm)) {
		pr_err("sc89601 couldn't get cm of battery!\n");
		goto err;
	}

	if(cm->otg_debug){
		ret = sc89601_set_otg_2A(info, true);
	}

	if(ret) {
		dev_err(info->dev,"Set boost limit failed\n");
		goto err;
	}
err:
	ret = sc89601_update_bits(info, SC89601_REG_1,
				  SC89601_REG_OTG_MASK,
				  SC89601_REG_OTG_ENABLE << SC89601_REG_OTG_SHIFT);
	if (ret) {
		dev_err(info->dev, "enable sc89601 otg failed\n");
		regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
		return ret;
	}

	ret = sc89601_charger_enable_wdg(info, true);
	if (ret) {
		dev_err(info->dev, "%s, failed to enable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	ret = sc89601_charger_feed_watchdog(info);
	if (ret) {
		dev_err(info->dev, "%s, failed to feed watchdog, ret = %d\n", __func__, ret);
		return ret;
	}
	ret = sc89601_charger_set_power_path_status(info, true);
	if (ret)
		dev_err(info->dev, "Failed to enable power path\n");

	info->otg_enable = true;
	info->last_wdt_time = ktime_to_ms(ktime_get());
	schedule_delayed_work(&info->wdt_work,
			      msecs_to_jiffies(SC89601_FEED_WATCHDOG_VALID_MS));
	schedule_delayed_work(&info->otg_work,
			      msecs_to_jiffies(SC89601_OTG_VALID_MS));
	dev_info(info->dev, "%s:line%d:enable_otg\n", __func__, __LINE__);

	return ret;
}

static int sc89601_charger_disable_otg(struct regulator_dev *dev)
{
	struct sc89601_charger_info *info = rdev_get_drvdata(dev);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	sc89601_charger_dump_stack();

	if (!sc89601_probe_is_ready(info)) {
		dev_err(info->dev, "%s wait probe timeout\n", __func__);
		return -EINVAL;
	}
	if(info->otg_2a_enable) {
		ret = sc89601_set_otg_2A(info, false);
	}

	info->otg_enable = false;
	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);
	ret = sc89601_update_bits(info, SC89601_REG_1,
				  SC89601_REG_OTG_MASK,
				  SC89601_REG_OTG_DISABLE << SC89601_REG_OTG_SHIFT);
	if (ret) {
		dev_err(info->dev, "disable sc89601 otg failed\n");
		return ret;
	}

	ret = sc89601_charger_enable_wdg(info, false);
	if (ret) {
		dev_err(info->dev, "%s, failed to disable watchdog, ret = %d\n", __func__, ret);
		return ret;
	}

	/* Enable charger detection function to identify the charger type */
	if (!info->use_typec_extcon) {
		ret = regmap_update_bits(info->pmic, info->charger_detect, BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev, "enable BC1.2 failed\n");
	}
	dev_info(info->dev, "%s:line%d:disable_otg\n", __func__, __LINE__);

	return ret;
}

static int sc89601_charger_vbus_is_enabled(struct regulator_dev *dev)
{
	struct sc89601_charger_info *info = rdev_get_drvdata(dev);
	int ret;
	u8 val;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}
	ret = sc89601_read(info, SC89601_REG_1, &val);
	if (ret) {
		dev_err(info->dev, "failed to get sc89601 otg status\n");
		return ret;
	}
	val &= SC89601_REG_OTG_MASK;
	val = (val >> SC89601_REG_OTG_SHIFT) & 0x01;
	dev_info(info->dev, "%s:line%d:vbus_is_enabled\n", __func__, __LINE__);
	return val;
}

static const struct regulator_ops sc89601_charger_vbus_ops = {
	.enable = sc89601_charger_enable_otg,
	.disable = sc89601_charger_disable_otg,
	.is_enabled = sc89601_charger_vbus_is_enabled,
};

static const struct regulator_desc sc89601_charger_vbus_desc = {
	.name = "otg-vbus",
	.of_match = "otg-vbus",
	.type = REGULATOR_VOLTAGE,
	.owner = THIS_MODULE,
	.ops = &sc89601_charger_vbus_ops,
	.fixed_uV = 5000000,
	.n_voltages = 1,
};

static void sc89601_charger_check_otg_status(struct sc89601_charger_info *info)
{
	int ret;
	u8 val;
	ret = sc89601_read(info, SC89601_REG_1, &val);
	if (ret < 0) {
		dev_err(info->dev, "%s:line%d, failed to get reg1(%d)\n", __func__, __LINE__, ret);
		return;
	}
	if (val & SC89601_REG_OTG_MASK) {
		dev_info(info->dev, "%s:line%d, exit otg mode\n", __func__, __LINE__);

		ret = sc89601_update_bits(info, SC89601_REG_1,
					SC89601_REG_OTG_MASK,
					SC89601_REG_OTG_DISABLE << SC89601_REG_OTG_SHIFT);
		if (ret)
			dev_err(info->dev, "disable sc89601 otg failed\n");
	}
}

static int sc89601_charger_register_vbus_regulator(struct sc89601_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;

	/*
	 * only master to support otg
	 */
	if (info->role != SC89601_ROLE_MASTER_DEFAULT)
		return 0;

	sc89601_charger_check_otg_status(info);

	cfg.dev = info->dev;
	cfg.driver_data = info;
	reg = devm_regulator_register(info->dev, &sc89601_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_err(info->dev, "Can't register regulator:%d\n", ret);
	}

	return ret;
}

static int sc89601_charger_register_external_vbus_regulator(struct sc89601_charger_info *info)
{
	struct regulator_config cfg = { };
	struct regulator_dev *reg;
	int ret = 0;
	struct device_node *otg_nd;
	struct device_node *otg_parent_nd;
	struct platform_device *otg_parent_nd_pdev;

	/*
	 * only master to support otg
	 */
	if (info->role != SC89601_ROLE_MASTER_DEFAULT)
		return 0;

	otg_nd = of_find_node_by_name(NULL, "otg-vbus");
	if (!otg_nd) {
		dev_warn(info->dev, "%s, unable to get otg node\n", __func__);
		return -EPROBE_DEFER;
	}

	otg_parent_nd = of_get_parent(otg_nd);
	of_node_put(otg_nd);
	if (!otg_parent_nd) {
		dev_warn(info->dev, "%s, unable to get otg parent node\n", __func__);
		return -EPROBE_DEFER;
	}

	otg_parent_nd_pdev = of_find_device_by_node(otg_parent_nd);
	of_node_put(otg_parent_nd);
	if (!otg_parent_nd_pdev) {
		dev_warn(info->dev, "%s, unable to get otg parent node device\n", __func__);
		return -EPROBE_DEFER;
	}

	cfg.dev = &otg_parent_nd_pdev->dev;
	platform_device_put(otg_parent_nd_pdev);
	cfg.driver_data = info;
	reg = devm_regulator_register(cfg.dev, &sc89601_charger_vbus_desc, &cfg);
	if (IS_ERR(reg)) {
		ret = PTR_ERR(reg);
		dev_warn(info->dev, "%s, failed to register vddvbus regulator:%d\n",
			 __func__, ret);
	}

	return ret;
}

#else
static int sc89601_charger_register_vbus_regulator(struct sc89601_charger_info *info)
{
	return 0;
}

static int sc89601_charger_register_external_vbus_regulator(struct sc89601_charger_info *info)
{
	return 0;
}
#endif

static int sc89601_charger_detect_device(struct sc89601_charger_info *info)
{
	int ret, part_id;
	u8 reg_val;
	ret = sc89601_read(info, SC89601_REG_B, &reg_val);
	if (ret < 0) {
		dev_err(info->dev, "%s, failed to get device id, ret = %d\n", __func__, ret);
		return ret;
	}

	part_id = (reg_val & SC89601_PN_MASK) >> SC89601_PN_SHIFT;
	if (part_id != SC89601_DEV_ID) {
		dev_err(info->dev, "%s, the device id is 0x%x\n", __func__, part_id);
		return -EINVAL;
	}

	return ret;
}

static int sc_chg_is_present(struct charger_dev *charger, int *present)
{
	struct sc89601_charger_info *sc = charger_get_private(charger);
	*present = sc89601_charger_is_present(sc);
	return 0;
}

static int sc_chg_get_vindpm_state(struct charger_dev *charger, bool *state)
{
	struct sc89601_charger_info *sc = charger_get_private(charger);
	return sc89601_get_charger_vindpm_state(sc, state);
}
static int sc_chg_set_shipmode(struct charger_dev *charger, bool en)
{
	struct sc89601_charger_info *sc = charger_get_private(charger);
	return sc89601_charger_set_shipmode(sc, en);
}
static int sc_chg_first_bc1p2(struct charger_dev *charger, int *bc1p2_result)
{
	struct sc89601_charger_info *sc = charger_get_private(charger);

	return sc89601_charger_first_bc1p2(sc, bc1p2_result);
}
static int sc_chg_retry_bc1p2(struct charger_dev *charger, int *bc1p2_result)
{
	struct sc89601_charger_info *sc = charger_get_private(charger);

	return sc89601_charger_retry_bc1p2(sc, bc1p2_result);
}
static int sc_chg_get_vbus_type(struct charger_dev *charger, int *type)
{
	struct sc89601_charger_info *sc = charger_get_private(charger);
	*type = sc89601_charger_get_vbus_stat(sc);
	return 0;
}
static int sc_chg_set_vindpm(struct charger_dev *charger, int vindpm)
{
	struct sc89601_charger_info *sc = charger_get_private(charger);
	sc->voltage_max_microvolt = vindpm;
	return sc89601_charger_set_vindpm(sc, vindpm);
}

static int sc_chg_get_iindpm(struct charger_dev *charger, int *iindpm)
{
	struct sc89601_charger_info *sc = charger_get_private(charger);

	return sc89601_charger_get_limit_current(sc, iindpm);
}

static int sc_chg_set_iterm(struct charger_dev *charger, int iterm)
{
	struct sc89601_charger_info *sc = charger_get_private(charger);
	int ret;

	ret = sc89601_charger_set_termina_cur(sc, iterm);
	if (!ret)
		sc->termination_cur = iterm;

	return ret;
}

static int sc_chg_set_pfm(struct charger_dev *charger, bool en)
{
	struct sc89601_charger_info *sgm = charger_get_private(charger);

	return sc89601_enable_pfm(sgm, en);
}

/*****************************charger ops**************************************/
static struct charger_ops charger_ops = {
	.is_present = sc_chg_is_present,
	.get_vindpm_state = sc_chg_get_vindpm_state,
	.set_shipmode = sc_chg_set_shipmode,
	.first_bc1p2 = sc_chg_first_bc1p2,
	.retry_bc1p2 = sc_chg_retry_bc1p2,
	.get_vbus_type = sc_chg_get_vbus_type,
	.set_vindpm = sc_chg_set_vindpm,
	.get_iindpm = sc_chg_get_iindpm,
	.set_iterm = sc_chg_set_iterm,
	.set_pfm = sc_chg_set_pfm,
};

static int sc8960x_irq_register(struct sc89601_charger_info *info)
{
    int ret = 1;

    info->irq_gpio = of_get_named_gpio(info->dev->of_node, "irq-gpio", 0);
	if (gpio_is_valid(info->irq_gpio)) {
		ret = devm_gpio_request_one(info->dev, info->irq_gpio,
					    GPIOF_DIR_IN, "sc89601_int");
		if (!ret)
			info->client->irq = gpio_to_irq(info->irq_gpio);
		else
			dev_err(info->dev, "int request failed, ret = %d\n", ret);
		if (info->client->irq < 0) {
			dev_err(info->dev, "failed to get irq no\n");
			gpio_free(info->irq_gpio);
		} else {
			ret = devm_request_threaded_irq(&info->client->dev, info->client->irq,
							sc8960x_irq_pre_handler, sc8960x_irq_handler,
							IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
							"sc89601 interrupt", info);
			if (ret)
				dev_err(info->dev, "Failed irq = %d ret = %d\n",
					info->client->irq, ret);
			else
				enable_irq_wake(info->client->irq);
		}
	} else {
		dev_err(info->dev, "failed to get irq gpio\n");
	}

    return ret;
}

static int sc89601_charger_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct device *dev = &client->dev;
	struct power_supply_config charger_cfg = { };
	struct sc89601_charger_info *info;
	struct device_node *regmap_np;
	struct platform_device *regmap_pdev;
	int ret;

	pr_info("%s:line%d: probe start!\n", __func__, __LINE__);
	client->addr = SC89601_I2C_ADDR;
	if (!adapter) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA)) {
		dev_err(dev, "No support for SMBUS_BYTE_DATA\n");
		return -ENODEV;
	}

	info = devm_kzalloc(dev, sizeof(*info), GFP_KERNEL);
	if (!info)
		return -ENOMEM;
	info->client = client;
	info->dev = dev;
	info->otg_2a_enable = false;

	i2c_set_clientdata(client, info);

	ret = sc89601_charger_detect_device(info);
	if (ret) {
		dev_err(dev, "%s, failed to detect device, ret = %d\n", __func__, ret);
		return -ENODEV;
	}

	INIT_DELAYED_WORK(&info->force_detect_dwork, sc8960x_force_detection_dwork_handler);
#if BC1P2_CHECK_WORK
	INIT_DELAYED_WORK(&info->bc12_timeout_dwork, sc8960x_bc12_timeout_dwork_handler);
#endif
	mutex_init(&info->bc_detect_lock);
	info->bc12_detect = false;
	info->bc12_recovery = false;
	info->power_good = 1;
	info->bc1p2_status = SC_BC1P2_UNKNOWN;

	power_path_control(info);

	ret = sc89601_charger_is_fgu_present(info);
	if (ret) {
		dev_err(dev, "sc27xx_fgu not ready.\n");
		return -EPROBE_DEFER;
	}

	info->use_typec_extcon = device_property_read_bool(dev, "use-typec-extcon");
	info->disable_wdg = device_property_read_bool(dev, "disable-otg-wdg-in-sleep");

	ret = device_property_read_bool(dev, "role-slave");
	if (ret)
		info->role = SC89601_ROLE_SLAVE;
	else
		info->role = SC89601_ROLE_MASTER_DEFAULT;
	if (info->role == SC89601_ROLE_SLAVE) {
		info->gpiod = devm_gpiod_get(dev, "enable", GPIOD_OUT_HIGH);
		if (IS_ERR(info->gpiod)) {
			dev_err(dev, "failed to get enable gpio\n");
			return PTR_ERR(info->gpiod);
		}
	}

	regmap_np = of_find_compatible_node(NULL, NULL, "sprd,sc27xx-syscon");
	if (!regmap_np)
		regmap_np = of_find_compatible_node(NULL, NULL, "sprd,ump962x-syscon");

	if (regmap_np) {
		if (of_device_is_compatible(regmap_np->parent, "sprd,sc2730"))
			info->charger_pd_mask = SC89601_DISABLE_PIN_MASK_2730;
		else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2721"))
			info->charger_pd_mask = SC89601_DISABLE_PIN_MASK_2721;
		else if (of_device_is_compatible(regmap_np->parent, "sprd,sc2720"))
			info->charger_pd_mask = SC89601_DISABLE_PIN_MASK_2720;
		else if (of_device_is_compatible(regmap_np, "sprd,ump962x-syscon"))
			info->charger_pd_mask = SC89601_DISABLE_PIN_MASK;
		else {
			dev_err(dev, "failed to get charger_pd mask\n");
			info->charger_pd_mask = SC89601_DISABLE_PIN_MASK;
		}
	} else {
		dev_err(dev, "unable to get syscon node\n");
		return -ENODEV;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 1,
					 &info->charger_detect);
	if (ret) {
		dev_err(dev, "failed to get charger_detect\n");
		return -EINVAL;
	}

	ret = of_property_read_u32_index(regmap_np, "reg", 2,
					 &info->charger_pd);
	if (ret) {
		dev_err(dev, "failed to get charger_pd reg\n");
		return ret;
	}

	regmap_pdev = of_find_device_by_node(regmap_np);
	if (!regmap_pdev) {
		of_node_put(regmap_np);
		dev_err(dev, "unable to get syscon device\n");
		return -ENODEV;
	}

	of_node_put(regmap_np);
	info->pmic = dev_get_regmap(regmap_pdev->dev.parent, NULL);
	if (!info->pmic) {
		dev_err(dev, "unable to get pmic regmap device\n");
		return -ENODEV;
	}

	mutex_init(&info->lock);
	mutex_init(&info->input_limit_cur_lock);
	init_completion(&info->probe_init);

	charger_cfg.drv_data = info;
	charger_cfg.of_node = dev->of_node;
	if (info->role == SC89601_ROLE_MASTER_DEFAULT) {
		info->psy_usb = devm_power_supply_register(dev,
							   &sc89601_charger_desc,
							   &charger_cfg);
	} else if (info->role == SC89601_ROLE_SLAVE) {
		info->psy_usb = devm_power_supply_register(dev,
							   &sc89601_slave_charger_desc,
							   &charger_cfg);
	}

	if (IS_ERR(info->psy_usb)) {
		dev_err(dev, "failed to register power supply\n");
		ret = PTR_ERR(info->psy_usb);
		goto err_regmap_exit;
	}

	ret = sc89601_charger_hw_init(info);
	if (ret) {
		dev_err(dev, "failed to sc89601_charger_hw_init\n");
		goto err_psy_usb;
	}

	sc89601_charger_check_power_path_status(info);

	device_init_wakeup(info->dev, true);

	alarm_init(&info->otg_timer, ALARM_BOOTTIME, NULL);
	INIT_DELAYED_WORK(&info->otg_work, sc89601_charger_otg_work);
	INIT_DELAYED_WORK(&info->wdt_work, sc89601_charger_feed_watchdog_work);
	INIT_DELAYED_WORK(&info->cur_work, sc89601_current_work);

	if (device_property_read_bool(dev, "otg-vbus-node-external"))
		ret = sc89601_charger_register_external_vbus_regulator(info);
	else
		ret = sc89601_charger_register_vbus_regulator(info);

	if (ret) {
		dev_err(dev, "failed to register vbus regulator.\n");
		goto err_psy_usb;
	}

	ret = sc89601_register_sysfs(info);
	if (ret) {
		dev_err(info->dev, "register sysfs fail, ret = %d\n", ret);
		goto error_sysfs;
	}

	info->sc_charger = charger_register("master_chg", info->dev, &charger_ops, info);
	if (!info->sc_charger) {
		ret = PTR_ERR(info->sc_charger);
		goto error_sc_charger;
	}

	ret = sc89601_update_bits(info, SC89601_REG_E, SC89601_REGE_EN_AUTO_MASK, 0 < SC89601_REGE_EN_AUTO_SHIFT);
	if (ret) {
        dev_err(info->dev, "%s disable auto bc1p2 failed(%d)\n", __func__, ret);
    }

	ret = sc8960x_irq_register(info);
	if (ret) {
        dev_err(info->dev, "%s irq register failed(%d)\n", __func__, ret);
    }

	info->probe_initialized = true;
	complete_all(&info->probe_init);

	sc89601_check_charge_full(info);
	sc89601_dump_register(info);
	dev_info(dev, "use_typec_extcon = %d\n", info->use_typec_extcon);

	return 0;
error_sc_charger:
	charger_unregister(info->sc_charger);
error_sysfs:
	sysfs_remove_group(&info->psy_usb->dev.kobj, &info->sysfs->attr_g);
err_psy_usb:
	if (info->irq_gpio)
		gpio_free(info->irq_gpio);
err_regmap_exit:
	mutex_destroy(&info->input_limit_cur_lock);
	mutex_destroy(&info->lock);
	return ret;
}

static void sc89601_charger_shutdown(struct i2c_client *client)
{
	struct sc89601_charger_info *info = i2c_get_clientdata(client);
	int ret = 0;

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return;
	}

	if (info->is_wireless_charge)
		cancel_delayed_work_sync(&info->cur_work);

	cancel_delayed_work_sync(&info->wdt_work);

	if (info->otg_enable) {
		info->otg_enable = false;
		cancel_delayed_work_sync(&info->otg_work);
		ret = sc89601_update_bits(info, SC89601_REG_1,
					  SC89601_REG_OTG_MASK,
					  0);
		if (ret)
			dev_err(info->dev, "disable sc89601 otg failed ret = %d\n", ret);

		ret = sc89601_set_otg_2A(info, false);
		dev_info(info->dev, "%s: dis otg 2A: %d\n", __func__, ret);
		ret = sc89601_charger_set_power_path_status(info, false);
		if (ret)
			dev_err(info->dev, "Failed to disable power path\n");

		/* Enable charger detection function to identify the charger type */
		ret = regmap_update_bits(info->pmic, info->charger_detect,
					 BIT_DP_DM_BC_ENB, 0);
		if (ret)
			dev_err(info->dev,
				"enable charger detection function failed ret = %d\n", ret);
	}

	info->shutdown_flag = true;
//	if (info->charging) {
		ret = sc89601_charger_set_limit_current(info, SC89601_SHUTDOWN_LIMIT, false);
		if (ret < 0)
			dev_err(info->dev, "%s: set input limit cur failed\n", __func__);

		ret = sc89601_charger_set_current(info, SC89601_SHUTDOWN_CURRENT);
		if (ret < 0)
			dev_err(info->dev, "%s:set charge current failed\n", __func__);
//	}
}

static int sc89601_charger_remove(struct i2c_client *client)
{
	struct sc89601_charger_info *info = i2c_get_clientdata(client);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	cancel_delayed_work_sync(&info->wdt_work);
	cancel_delayed_work_sync(&info->otg_work);

	mutex_destroy(&info->input_limit_cur_lock);
	mutex_destroy(&info->lock);

	return 0;
}

#if IS_ENABLED(CONFIG_PM_SLEEP)
static int sc89601_charger_suspend(struct device *dev)
{
	ktime_t now, add;
	struct sc89601_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online) {
		if (sc89601_charger_feed_watchdog(info))
			dev_err(info->dev, "%s, failed to feed watchdog\n", __func__);

		cancel_delayed_work_sync(&info->wdt_work);
	}

	if (!info->otg_enable)
		return 0;

	if (info->is_wireless_charge)
		cancel_delayed_work_sync(&info->cur_work);

	if (info->disable_wdg) {
		if (sc89601_charger_enable_wdg(info, false)) {
			dev_err(info->dev, "%s, failed to disable watchdog\n", __func__);
			return -EBUSY;
		}
	} else {
		now = ktime_get_boottime();
		add = ktime_set(SC89601_OTG_ALARM_TIMER_S, 0);
		alarm_start(&info->otg_timer, ktime_add(now, add));
		pr_info("sc89601_charger set alarm, triggered at [%lld]ms\n",
			ktime_to_ms(ktime_add(now, add)));
	}

	return 0;
}

static int sc89601_charger_resume(struct device *dev)
{
	struct sc89601_charger_info *info = dev_get_drvdata(dev);

	if (!info) {
		pr_err("%s:line%d: NULL pointer!!!\n", __func__, __LINE__);
		return -EINVAL;
	}

	if (info->otg_enable || info->is_charger_online) {
		if (sc89601_charger_feed_watchdog(info))
			dev_err(info->dev, "%s, failed to feed watchdog\n", __func__);

		schedule_delayed_work(&info->wdt_work, HZ * 15);
	}

	if (!info->otg_enable)
		return 0;

	if (info->disable_wdg) {
		if (sc89601_charger_enable_wdg(info, true)) {
			dev_err(info->dev, "%s, failed to enable watchdog\n", __func__);
			return -EBUSY;
		}
	} else {
		alarm_cancel(&info->otg_timer);
	}

	if (info->is_wireless_charge)
		schedule_delayed_work(&info->cur_work, 0);

	return 0;
}
#endif

static const struct dev_pm_ops sc89601_charger_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(sc89601_charger_suspend,
				sc89601_charger_resume)
};

static const struct i2c_device_id sc89601_i2c_id[] = {
	{"sc89601_chg", 0},
	{"sc89601_slave_chg", 0},
	{}
};

static const struct of_device_id sc89601_charger_of_match[] = {
	{ .compatible = "sc,sc89601_chg", },
	{ .compatible = "sc,sc89601_slave_chg", },
	{ }
};

MODULE_DEVICE_TABLE(of, sc89601_charger_of_match);

static struct i2c_driver sc89601_charger_driver = {
	.driver = {
		.name = "sc89601_chg",
		.of_match_table = sc89601_charger_of_match,
		.pm = &sc89601_charger_pm_ops,
	},
	.probe = sc89601_charger_probe,
	.shutdown = sc89601_charger_shutdown,
	.remove = sc89601_charger_remove,
	.id_table = sc89601_i2c_id,
};

module_i2c_driver(sc89601_charger_driver);
MODULE_DESCRIPTION("SC89601 Charger Driver");
MODULE_LICENSE("GPL v2");
