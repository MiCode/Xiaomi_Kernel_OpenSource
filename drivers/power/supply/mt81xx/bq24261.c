/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif
#include <linux/reboot.h>

#include "bq24261.h"
#include "mt_charging.h"
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_reboot.h>
#include <mt-plat/upmu_common.h>

#define STATUS_OK 0
#define STATUS_UNSUPPORTED -1

/**********************************************************
 *
 *   [I2C Slave Setting]
 *
 *********************************************************/
#define bq24261_SLAVE_ADDR_WRITE 0xD6
#define bq24261_SLAVE_ADDR_Read 0xD7

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/
static u8 bq24261_reg[bq24261_REG_NUM] = {0};

static int g_bq24261_hw_exist;
static struct i2c_client *new_client;
static DEFINE_MUTEX(bq24261_i2c_access);

static const u32 INPUT_CS_VTH[] = {
	CHARGE_CURRENT_100_00_MA,  CHARGE_CURRENT_150_00_MA,
	CHARGE_CURRENT_500_00_MA,  CHARGE_CURRENT_900_00_MA,
	CHARGE_CURRENT_1500_00_MA, CHARGE_CURRENT_1950_00_MA,
	CHARGE_CURRENT_2500_00_MA, CHARGE_CURRENT_2000_00_MA,
	CHARGE_CURRENT_MAX};

/* for MT6391 */
static const u32 VCDT_HV_VTH[] = {
	BATTERY_VOLT_04_000000_V, BATTERY_VOLT_04_100000_V,
	BATTERY_VOLT_04_150000_V, BATTERY_VOLT_04_200000_V,
	BATTERY_VOLT_04_250000_V, BATTERY_VOLT_04_300000_V,
	BATTERY_VOLT_04_350000_V, BATTERY_VOLT_04_400000_V,
	BATTERY_VOLT_04_450000_V, BATTERY_VOLT_04_500000_V,
	BATTERY_VOLT_04_550000_V, BATTERY_VOLT_04_600000_V,
	BATTERY_VOLT_07_000000_V, BATTERY_VOLT_07_500000_V,
	BATTERY_VOLT_08_500000_V, BATTERY_VOLT_10_500000_V};

/**********************************************************
 *
 *   [I2C Function For Read/Write bq24261]
 *
 *********************************************************/
int bq24261_read_byte(u8 cmd, u8 *data)
{
	int ret;

	struct i2c_msg msg[2];

	if (!new_client) {
		pr_debug("error: access bq24261 before driver ready\n");
		return 0;
	}

	msg[0].addr = new_client->addr;
	msg[0].buf = &cmd;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[1].addr = new_client->addr;
	msg[1].buf = data;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;

	ret = i2c_transfer(new_client->adapter, msg, 2);

	if (ret != 2)
		pr_debug("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

int bq24261_write_byte(u8 cmd, u8 data)
{
	char buf[2];
	int ret;

	if (!new_client) {
		pr_debug("error: access bq24261 before driver ready\n");
		return 0;
	}

	buf[0] = cmd;
	buf[1] = data;

	ret = i2c_master_send(new_client, buf, 2);

	if (ret != 2)
		pr_debug("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

u32 bq24261_read_interface(u8 RegNum, u8 *val, u8 MASK, u8 SHIFT)
{
	u8 bq24261_reg = 0;
	u32 ret = 0;

	ret = bq24261_read_byte(RegNum, &bq24261_reg);

	bq24261_reg &= (MASK << SHIFT);
	*val = (bq24261_reg >> SHIFT);

	return ret;
}

u32 bq24261_config_interface(u8 RegNum, u8 val, u8 MASK, u8 SHIFT)
{
	u8 bq24261_reg = 0;
	u32 ret = 0;

	ret = bq24261_read_byte(RegNum, &bq24261_reg);

	bq24261_reg &= ~(MASK << SHIFT);
	bq24261_reg |= (val << SHIFT);

	if (RegNum == bq24261_CON1 && val == 1 && MASK == CON1_RESET_MASK &&
	    SHIFT == CON1_RESET_SHIFT) {
		/* read RESET bit */
	} else if (RegNum == bq24261_CON1) {
		/* RESET bit read always return 1, need clear it */
		bq24261_reg &= ~0x80;
	}

	ret = bq24261_write_byte(RegNum, bq24261_reg);

	return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
/* CON0---------------------------------------------------- */

void bq24261_set_tmr_rst(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON0), (u8)(val),
				       (u8)(CON0_TMR_RST_MASK),
				       (u8)(CON0_TMR_RST_SHIFT));
}

void bq24261_set_en_boost(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON0), (u8)(val),
				       (u8)(CON0_EN_BOOST_MASK),
				       (u8)(CON0_EN_BOOST_SHIFT));
}

u32 bq24261_get_stat(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON0), (&val),
				     (u8)(CON0_STAT_MASK),
				     (u8)(CON0_STAT_SHIFT));
	return val;
}

void bq24261_set_en_shipmode(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON0), (u8)(val),
				       (u8)(CON0_EN_SHIPMODE_MASK),
				       (u8)(CON0_EN_SHIPMODE_SHIFT));
}

u32 bq24261_get_fault(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON0), (&val),
				     (u8)(CON0_FAULT_MASK),
				     (u8)(CON0_FAULT_SHIFT));
	return val;
}

/* CON1---------------------------------------------------- */

void bq24261_set_reset(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON1), (u8)(val),
				       (u8)(CON1_RESET_MASK),
				       (u8)(CON1_RESET_SHIFT));
}

u32 bq24261_get_in_limit(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON1), (&val),
				     (u8)(CON1_IN_LIMIT_MASK),
				     (u8)(CON1_IN_LIMIT_SHIFT));
	return val;
}

void bq24261_set_in_limit(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON1), (u8)(val),
				       (u8)(CON1_IN_LIMIT_MASK),
				       (u8)(CON1_IN_LIMIT_SHIFT));
}

void bq24261_set_en_stat(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON1), (u8)(val),
				       (u8)(CON1_EN_STAT_MASK),
				       (u8)(CON1_EN_STAT_SHIFT));
}

void bq24261_set_te(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON1), (u8)(val),
				       (u8)(CON1_TE_MASK), (u8)(CON1_TE_SHIFT));
}

void bq24261_set_dis_ce(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON1), (u8)(val),
				       (u8)(CON1_DIS_CE_MASK),
				       (u8)(CON1_DIS_CE_SHIFT));
}

void bq24261_set_hz_mode(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON1), (u8)(val),
				       (u8)(CON1_HZ_MODE_MASK),
				       (u8)(CON1_HZ_MODE_SHIFT));
}

/* CON2---------------------------------------------------- */

void bq24261_set_vbreg(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON2), (u8)(val),
				       (u8)(CON2_VBREG_MASK),
				       (u8)(CON2_VBREG_SHIFT));
}

void bq24261_set_mod_freq(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON2), (u8)(val),
				       (u8)(CON2_MOD_FREQ_MASK),
				       (u8)(CON2_MOD_FREQ_SHIFT));
}

/* CON3---------------------------------------------------- */

u32 bq24261_get_vender_code(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON3), (&val),
				     (u8)(CON3_VENDER_CODE_MASK),
				     (u8)(CON3_VENDER_CODE_SHIFT));
	return val;
}

u32 bq24261_get_pn(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON3), (&val),
				     (u8)(CON3_PN_MASK), (u8)(CON3_PN_SHIFT));
	return val;
}

/* CON4---------------------------------------------------- */

u32 bq24261_get_ichg(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON4), (&val),
				     (u8)(CON4_ICHRG_MASK),
				     (u8)(CON4_ICHRG_SHIFT));
	return val;
}

void bq24261_set_ichg(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON4), (u8)(val),
				       (u8)(CON4_ICHRG_MASK),
				       (u8)(CON4_ICHRG_SHIFT));
}

void bq24261_set_iterm(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON4), (u8)(val),
				       (u8)(CON4_ITERM_MASK),
				       (u8)(CON4_ITERM_SHIFT));
}

/* CON5---------------------------------------------------- */

u32 bq24261_get_minsys_status(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON5), (&val),
				     (u8)(CON5_MINSYS_STATUS_MASK),
				     (u8)(CON5_MINSYS_STATUS_SHIFT));
	return val;
}

u32 bq24261_get_vindpm_status(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON5), (&val),
				     (u8)(CON5_VINDPM_STATUS_MASK),
				     (u8)(CON5_VINDPM_STATUS_SHIFT));
	return val;
}

u32 bq24261_get_low_chg(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON5), (&val),
				     (u8)(CON5_LOW_CHG_MASK),
				     (u8)(CON5_LOW_CHG_SHIFT));
	return val;
}

void bq24261_set_low_chg(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON5), (u8)(val),
				       (u8)(CON5_LOW_CHG_MASK),
				       (u8)(CON5_LOW_CHG_SHIFT));
}

void bq24261_set_dpdm_en(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON5), (u8)(val),
				       (u8)(CON5_DPDM_EN_MASK),
				       (u8)(CON5_DPDM_EN_SHIFT));
}

u32 bq24261_get_cd_status(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON5), (&val),
				     (u8)(CON5_CD_STATUS_MASK),
				     (u8)(CON5_CD_STATUS_SHIFT));
	return val;
}

void bq24261_set_vindpm(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON5), (u8)(val),
				       (u8)(CON5_VINDPM_MASK),
				       (u8)(CON5_VINDPM_SHIFT));
}

/* CON6---------------------------------------------------- */

void bq24261_set_2xtmr_en(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON6), (u8)(val),
				       (u8)(CON6_2XTMR_EN_MASK),
				       (u8)(CON6_2XTMR_EN_SHIFT));
}

void bq24261_set_tmr(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON6), (u8)(val),
				       (u8)(CON6_TMR_MASK),
				       (u8)(CON6_TMR_SHIFT));
}

void bq24261_set_boost_ilim(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON6), (u8)(val),
				       (u8)(CON6_BOOST_ILIM_MASK),
				       (u8)(CON6_BOOST_ILIM_SHIFT));
}

void bq24261_set_ts_en(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON6), (u8)(val),
				       (u8)(CON6_TS_EN_MASK),
				       (u8)(CON6_TS_EN_SHIFT));
}

u32 bq24261_get_ts_fault(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface((u8)(bq24261_CON6), (&val),
				     (u8)(CON6_TS_FAULT_MASK),
				     (u8)(CON6_TS_FAULT_SHIFT));
	return val;
}

void bq24261_set_vindpm_off(u32 val)
{
	u32 ret = 0;

	ret = bq24261_config_interface((u8)(bq24261_CON6), (u8)(val),
				       (u8)(CON6_VINDPM_OFF_MASK),
				       (u8)(CON6_VINDPM_OFF_SHIFT));
}

void bq24261_dump_register(void)
{
	u8 i = 0;

	for (i = 0; i < bq24261_REG_NUM; i++) {
		bq24261_read_byte(i, &bq24261_reg[i]);
		pr_debug("[bq24261_dump_register] Reg[0x%X]=0x%X\n", i,
			    bq24261_reg[i]);
	}
}

static u32 charging_hw_init(void *data)
{
	u32 status = STATUS_OK;

	bq24261_set_tmr_rst(1);  /* wdt reset */
	bq24261_set_en_boost(0); /* OTG boost */
	bq24261_set_tmr(0x3);    /* default disable safety timer */
	bq24261_set_iterm(0x3);  /* iterm 200mA */

	bq24261_set_vindpm_off(0); /* 4.2V offset */
	bq24261_set_vindpm(0x3);   /* 4.452 VINDPM */
	bq24261_set_ts_en(0);      /* disable TS function */

	return status;
}

static u32 charging_dump_register(void *data)
{
	u32 status = STATUS_OK;

	pr_debug("charging_dump_register\r\n");

	bq24261_dump_register();

	return status;
}

static u32 charging_enable(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);

	if (true == enable) {
		bq24261_set_hz_mode(0x0);
		bq24261_set_dis_ce(0);
	} else
		bq24261_set_dis_ce(0x1);

	return status;
}

static u32 charging_set_cv_voltage(void *data)
{
	u32 status = STATUS_OK;
	u16 register_value;
	u32 cv_value = *(u32 *)(data);

	register_value = ((cv_value / 1000) - 3500) / 20;
	bq24261_set_vbreg(register_value);

	return status;
}

static u32 charging_get_current(void *data)
{
	u32 status = STATUS_OK;
	u32 data_val = 0;
	u8 ret_val = 0;

	ret_val = bq24261_get_ichg();
	data_val = 500 + (ret_val * 100);
	ret_val = bq24261_get_low_chg();

	/* value in uA */
	if (ret_val == 0)
		*(u32 *)data = data_val * 1000;
	else
		*(u32 *)data = 300000;

	return status;
}

static u32 charging_set_current(void *data)
{
	u32 status = STATUS_OK;
	u32 register_value;
	u32 current_value = *(u32 *)data;

	current_value = current_value / 100;

	if (current_value <= 500)
		register_value = 0;
	else
		register_value = (current_value - 500) / 100;

	bq24261_set_ichg(register_value);

	return status;
}

static u32 charging_set_input_current(void *data)
{
	u32 status = STATUS_OK;
	u32 array_size;
	u32 current_value = *(u32 *)data;
	u32 register_value;

	if (current_value >= CHARGE_CURRENT_2500_00_MA)
		register_value = 0x6;
	else if (current_value == CHARGE_CURRENT_2000_00_MA)
		register_value = 0x7;
	else {
		array_size = ARRAY_SIZE(INPUT_CS_VTH);
		for (register_value = 0; register_value < array_size;
		     register_value++)
			if (INPUT_CS_VTH[register_value] >= current_value)
				break;
	}

	bq24261_set_in_limit(register_value);

	return status;
}

static u32 charging_get_input_current(void *data)
{
	u32 register_value;

	register_value = bq24261_get_in_limit();
	*(u32 *)data = INPUT_CS_VTH[register_value];
	return STATUS_OK;
}

static u32 charging_get_charging_status(void *data)
{
	u32 status = STATUS_OK;
	u32 ret_val;

	ret_val = bq24261_get_stat();

	if (ret_val == 0x2) /* charge done */
		*(u32 *)data = true;
	else
		*(u32 *)data = false;

	return status;
}

static u32 charging_reset_watch_dog_timer(void *data)
{
	u32 status = STATUS_OK;

	pr_debug("charging_reset_watch_dog_timer\r\n");
	bq24261_set_tmr_rst(1);
	return status;
}

static u32 charging_set_hv_threshold(void *data)
{
	u32 status = STATUS_OK;
	u32 array_size;
	u32 current_value = *(u32 *)data;
	u32 register_value;

	array_size = ARRAY_SIZE(VCDT_HV_VTH);
	for (register_value = 0; register_value < array_size;
	     register_value++) {
		if (VCDT_HV_VTH[register_value] >= current_value)
			break;
	}

	upmu_set_rg_vcdt_hv_vth(register_value);

	return status;
}

static u32 charging_get_hv_status(void *data)
{
	u32 status = STATUS_OK;

	*(bool *)(data) = upmu_get_rgs_vcdt_hv_det();
	return status;
}

static u32 charging_get_battery_status(void *data)
{
	u32 status = STATUS_OK;

	/* upmu_set_baton_tdet_en(1); */
	upmu_set_rg_baton_en(1);
	*(bool *)(data) = upmu_get_rgs_baton_undet();

	return status;
}

static u32 charging_get_charger_det_status(void *data)
{
	u32 status = STATUS_OK;

	*(bool *)(data) = upmu_get_rgs_chrdet();

	return status;
}

static u32 charging_get_charger_type(void *data)
{
	u32 status = STATUS_OK;

#if 0 /*defined(CONFIG_POWER_EXT) */
	*(CHARGER_TYPE *) (data) = STANDARD_HOST;
#else
	*(int *)(data) = hw_charger_type_detection();
#endif
	return status;
}

static u32 charging_get_is_pcm_timer_trigger(void *data)
{
	u32 status = STATUS_OK;

	if (slp_get_wake_reason() == 3)
		*(bool *)(data) = true;
	else
		*(bool *)(data) = false;

	pr_debug("slp_get_wake_reason=%d\n",
		    slp_get_wake_reason());

	return status;
}

static u32 charging_set_platform_reset(void *data)
{
	u32 status = STATUS_OK;

	pr_debug("charging_set_platform_reset\n");

	if (system_state == SYSTEM_BOOTING)
		arch_reset(0, NULL);
	else
		orderly_reboot(true);

	return status;
}

static u32 charging_get_platform_boot_mode(void *data)
{
	u32 status = STATUS_OK;

	*(u32 *)(data) = get_boot_mode();

	pr_debug("get_boot_mode=%d\n", get_boot_mode());

	return status;
}

static u32 charging_enable_powerpath(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);

	if (true == enable)
		bq24261_set_hz_mode(0x0);
	else
		bq24261_set_hz_mode(0x1);

	return status;
}

static u32 charging_boost_enable(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);

	if (true == enable) {
		bq24261_set_boost_ilim(0x1); /* 1A on VBUS */
		bq24261_set_hz_mode(0x0);
		bq24261_set_dis_ce(0x1); /* charge disabled */
		bq24261_set_en_boost(0x1);
	} else
		bq24261_set_en_boost(0x0);

	return status;
}

static u32 charging_set_ta_current_pattern(void *data)
{
	u32 increase = *(u32 *)(data);
	u32 charging_status = false;
#if defined(HIGH_BATTERY_VOLTAGE_SUPPORT)
	u32 cv_voltage = BATTERY_VOLT_04_340000_V;
#else
	u32 cv_voltage = BATTERY_VOLT_04_200000_V;
#endif

	charging_get_charging_status(&charging_status);
	if (false == charging_status) {
		charging_set_cv_voltage(&cv_voltage); /* Set CV 4.2V */
		bq24261_set_ichg(0x0);   /* Set charging current 500ma */
		bq24261_set_dis_ce(0x0); /* Enable Charging */
	}

	if (increase == true) {
		bq24261_set_in_limit(0x0); /* 100mA */
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 1");
		msleep(85);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 1");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 2");
		msleep(85);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 2");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 3");
		msleep(281);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 3");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 4");
		msleep(281);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 4");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 5");
		msleep(281);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 5");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_increase() on 6");
		msleep(485);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_increase() off 6");
		msleep(50);

		pr_debug("mtk_ta_increase() end\n");

		bq24261_set_in_limit(0x2); /* 500mA */
		msleep(200);
	} else {
		bq24261_set_in_limit(0x0); /* 100mA */
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 1");
		msleep(281);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 1");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 2");
		msleep(281);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 2");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 3");
		msleep(281);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 3");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 4");
		msleep(85);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 4");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 5");
		msleep(85);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 5");
		msleep(85);

		bq24261_set_in_limit(0x2); /* 500mA */
		pr_debug("mtk_ta_decrease() on 6");
		msleep(485);

		bq24261_set_in_limit(0x0); /* 100mA */
		pr_debug("mtk_ta_decrease() off 6");
		msleep(50);

		pr_debug("mtk_ta_decrease() end\n");

		bq24261_set_in_limit(0x2); /* 500mA */
	}

	return STATUS_OK;
}

static u32 (*charging_func[CHARGING_CMD_NUMBER])(void *data);

int bq24261_control_interface(int cmd, void *data)
{
	int status;
	static bool is_init;

	if (is_init == false) {
		charging_func[CHARGING_CMD_INIT] = charging_hw_init;
		charging_func[CHARGING_CMD_DUMP_REGISTER] =
			charging_dump_register;
		charging_func[CHARGING_CMD_ENABLE] = charging_enable;
		charging_func[CHARGING_CMD_SET_CV_VOLTAGE] =
			charging_set_cv_voltage;
		charging_func[CHARGING_CMD_GET_CURRENT] = charging_get_current;
		charging_func[CHARGING_CMD_SET_CURRENT] = charging_set_current;
		charging_func[CHARGING_CMD_GET_INPUT_CURRENT] =
			charging_get_input_current;
		charging_func[CHARGING_CMD_SET_INPUT_CURRENT] =
			charging_set_input_current;
		charging_func[CHARGING_CMD_GET_CHARGING_STATUS] =
			charging_get_charging_status;
		charging_func[CHARGING_CMD_RESET_WATCH_DOG_TIMER] =
			charging_reset_watch_dog_timer;
		charging_func[CHARGING_CMD_SET_HV_THRESHOLD] =
			charging_set_hv_threshold;
		charging_func[CHARGING_CMD_GET_HV_STATUS] =
			charging_get_hv_status;
		charging_func[CHARGING_CMD_GET_BATTERY_STATUS] =
			charging_get_battery_status;
		charging_func[CHARGING_CMD_GET_CHARGER_DET_STATUS] =
			charging_get_charger_det_status;
		charging_func[CHARGING_CMD_GET_CHARGER_TYPE] =
			charging_get_charger_type;
		charging_func[CHARGING_CMD_GET_IS_PCM_TIMER_TRIGGER] =
			charging_get_is_pcm_timer_trigger;
		charging_func[CHARGING_CMD_SET_PLATFORM_RESET] =
			charging_set_platform_reset;
		charging_func[CHARGING_CMD_GET_PLATFORM_BOOT_MODE] =
			charging_get_platform_boot_mode;
		charging_func[CHARGING_CMD_ENABLE_POWERPATH] =
			charging_enable_powerpath;
		charging_func[CHARGING_CMD_BOOST_ENABLE] =
			charging_boost_enable;
		charging_func[CHARGING_CMD_SET_TA_CURRENT_PATTERN] =
			charging_set_ta_current_pattern;
		is_init = true;
	}

	if (cmd < CHARGING_CMD_NUMBER && charging_func[cmd])
		status = charging_func[cmd](data);
	else {
		pr_debug("Unsupported charging command:%d!\n", cmd);
		return STATUS_UNSUPPORTED;
	}

	return status;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
void bq24261_hw_component_detect(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24261_read_interface(0x03, &val, 0xFF, 0x0);

	if (val == 0)
		g_bq24261_hw_exist = 0;
	else
		g_bq24261_hw_exist = 1;

	pr_debug("[bq24261_hw_component_detect] exist=%d, Reg[0x03]=0x%x\n",
		g_bq24261_hw_exist, val);

	if (g_bq24261_hw_exist)
		bat_charger_register(bq24261_control_interface);
}

int is_bq24261_exist(void)
{
	pr_debug("[is_bq24261_exist] g_bq24261_hw_exist=%d\n",
		 g_bq24261_hw_exist);

	return g_bq24261_hw_exist;
}

static int bq24261_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	pr_debug("[bq24261_driver_probe]\n");

	new_client = client;

	bq24261_hw_component_detect();
	bq24261_dump_register();

	return 0;
}

static const struct i2c_device_id bq24261_i2c_id[] = {{"bq24261", 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id bq24261_of_match[] = {
	{
		.compatible = "ti,bq24261",
	},
	{},
};

MODULE_DEVICE_TABLE(of, bq24261_of_match);
#endif

static struct i2c_driver bq24261_driver = {
	.driver = {

			.name = "bq24261",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(bq24261_of_match),
#endif
		},
	.probe = bq24261_driver_probe,
	.id_table = bq24261_i2c_id,

};

/**********************************************************
 *
 *   [platform_driver API]
 *
 *********************************************************/
u8 g_reg_value_bq24261;
static ssize_t show_bq24261_access(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	pr_debug("[show_bq24261_access] 0x%x\n", g_reg_value_bq24261);
	return sprintf(buf, "%u\n", g_reg_value_bq24261);
}

static ssize_t store_bq24261_access(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue, temp_buf[16];
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	if (buf != NULL && size != 0) {

		strncpy(temp_buf, buf, sizeof(temp_buf));
		temp_buf[sizeof(temp_buf) - 1] = 0;
		pvalue = temp_buf;
		if (size > 4) {
			ret = kstrtouint(strsep(&pvalue, " "), 0, &reg_address);
			if (ret) {
				pr_debug("wrong format!\n");
				return size;
			}
			ret = kstrtouint(pvalue, 0, &reg_value);
			if (ret) {
				pr_debug("wrong format!\n");
				return size;
			}
			pr_debug(
				"[store_bq24261_access] write bq24261 reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			bq24261_config_interface(reg_address, reg_value, 0xFF,
						 0x0);
		} else {
			ret = kstrtouint(pvalue, 0, &reg_address);
			if (ret) {
				pr_debug("wrong format!\n");
				return size;
			}
			bq24261_read_interface(reg_address,
					       &g_reg_value_bq24261, 0xFF, 0x0);
			pr_debug(
				"[store_bq24261_access] read bq24261 reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value_bq24261);
			pr_debug(
				"[store_bq24261_access] Please use \"cat bq24261_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(bq24261_access, 0644, show_bq24261_access,
		   store_bq24261_access);

static int bq24261_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_bq24261_access);

	return 0;
}

struct platform_device bq24261_user_space_device = {
	.name = "bq24261-user", .id = -1,
};

static struct platform_driver bq24261_user_space_driver = {
	.probe = bq24261_user_space_probe,
	.driver = {

			.name = "bq24261-user",
		},
};

static int __init bq24261_init(void)
{
	int ret = 0;

	if (i2c_add_driver(&bq24261_driver) != 0)
		pr_debug("[bq24261_init] failed to register bq24261 i2c driver.\n");
	else
		pr_debug("[bq24261_init] Success to register bq24261 i2c driver.\n");

	ret = platform_device_register(&bq24261_user_space_device);
	if (ret) {
		pr_debug("****[bq24261_init] Unable to device register(%d)\n",
			ret);
		return ret;
	}
	ret = platform_driver_register(&bq24261_user_space_driver);
	if (ret) {
		pr_debug("****[bq24261_init] Unable to register driver (%d)\n",
			ret);
		return ret;
	}

	return 0;
}

static void __exit bq24261_exit(void)
{
	i2c_del_driver(&bq24261_driver);
}
module_init(bq24261_init);
module_exit(bq24261_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq24261 Driver");
MODULE_AUTHOR("James Lo<james.lo@mediatek.com>");
