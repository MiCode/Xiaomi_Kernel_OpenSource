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

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/switch.h>
#include <linux/workqueue.h>

#include "bq25890.h"
#include "mt_charging.h"
#include <mt-plat/mt_boot.h>
#include <mt-plat/mt_reboot.h>
#include <mt-plat/upmu_common.h>

/**********************************************************
 *
 *    [Define]
 *
 **********************************************************/

#define STATUS_OK 0
#define STATUS_UNSUPPORTED -1
#define GETARRAYNUM(array) (ARRAY_SIZE(array))
#define bq25890_REG_NUM 21

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/

static struct i2c_client *new_client;
static u8 bq25890_reg[bq25890_REG_NUM] = {0};

static u8 g_reg_value_bq25890;

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
 *   [I2C Function For Read/Write bq25890]
 *
 *********************************************************/
int bq25890_read_byte(u8 cmd, u8 *data)
{
	int ret;

	struct i2c_msg msg[2];

	if (!new_client) {
		pr_debug("error: access bq25890 before driver ready\n");
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

int bq25890_write_byte(u8 cmd, u8 data)
{
	char buf[2];
	int ret;

	if (!new_client) {
		pr_debug("error: access bq25890 before driver ready\n");
		return 0;
	}

	buf[0] = cmd;
	buf[1] = data;

	ret = i2c_master_send(new_client, buf, 2);

	if (ret != 2)
		pr_debug("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

u32 bq25890_read_interface(u8 RegNum, u8 *val, u8 MASK, u8 SHIFT)
{
	u8 bq25890_reg = 0;
	int ret = 0;

	ret = bq25890_read_byte(RegNum, &bq25890_reg);

	bq25890_reg &= (MASK << SHIFT);
	*val = (bq25890_reg >> SHIFT);

	return ret;
}

u32 bq25890_config_interface(u8 RegNum, u8 val, u8 MASK, u8 SHIFT)
{
	u8 bq25890_reg = 0;
	int ret = 0;

	ret = bq25890_read_byte(RegNum, &bq25890_reg);

	bq25890_reg &= ~(MASK << SHIFT);
	bq25890_reg |= (val << SHIFT);

	ret = bq25890_write_byte(RegNum, bq25890_reg);
	return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/

/* CON0---------------------------------------------------- */
void bq25890_set_en_hiz(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON0), (u8)(val),
				 (u8)(CON0_EN_HIZ_MASK),
				 (u8)(CON0_EN_HIZ_SHIFT));
}

void bq25890_set_iinlim(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON0), (u8)(val),
				 (u8)(CON0_IINLIM_MASK),
				 (u8)(CON0_IINLIM_SHIFT));
}

u32 bq25890_get_iinlim(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON0), (&val),
			       (u8)(CON0_IINLIM_MASK), (u8)(CON0_IINLIM_SHIFT));
	return val;
}

/* CON1---------------------------------------------------- */

/* CON2---------------------------------------------------- */
void bq25890_set_force_dpdm(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON2), (u8)(val),
				 (u8)(CON2_FORCE_DPDM_MASK),
				 (u8)(CON2_FORCE_DPDM_SHIFT));
}

void bq25890_set_auto_dpdm_en(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON2), (u8)(val),
				 (u8)(CON2_AUTO_DPDM_MASK),
				 (u8)(CON2_AUTO_DPDM_SHIFT));
}

u32 bq25890_get_dpdm_status(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON2), (&val),
			       (u8)(CON2_FORCE_DPDM_MASK),
			       (u8)(CON2_FORCE_DPDM_SHIFT));
	return val;
}

/* CON3---------------------------------------------------- */
void bq25890_set_wdt_rst(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON3), (u8)(val),
				 (u8)(CON3_WDT_RST_MASK),
				 (u8)(CON3_WDT_RST_SHIFT));
}

void bq25890_set_otg_config(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON3), (u8)(val),
				 (u8)(CON3_OTG_CONFIG_MASK),
				 (u8)(CON3_OTG_CONFIG_SHIFT));
}

void bq25890_set_chg_config(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON3), (u8)(val),
				 (u8)(CON3_CHG_CONFIG_MASK),
				 (u8)(CON3_CHG_CONFIG_SHIFT));
}

void bq25890_set_sys_min(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON3), (u8)(val),
				 (u8)(CON3_SYS_MIN_MASK),
				 (u8)(CON3_SYS_MIN_SHIFT));
}

/* CON4---------------------------------------------------- */
void bq25890_set_pumpx_en(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON4), (u8)(val),
				 (u8)(CON4_EN_PUMPX_MASK),
				 (u8)(CON4_EN_PUMPX_SHIFT));
}

u32 bq25890_get_pumpx_en(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON4), (&val),
			       (u8)(CON4_EN_PUMPX_MASK),
			       (u8)(CON4_EN_PUMPX_SHIFT));
	return val;
}

void bq25890_set_ichg(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON4), (u8)(val),
				 (u8)(CON4_ICHG_MASK), (u8)(CON4_ICHG_SHIFT));
}

u32 bq25890_get_ichg(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON4), (&val), (u8)(CON4_ICHG_MASK),
			       (u8)(CON4_ICHG_SHIFT));
	return val;
}

/* CON5---------------------------------------------------- */
void bq25890_set_iprechg(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON5), (u8)(val),
				 (u8)(CON5_IPRECHG_MASK),
				 (u8)(CON5_IPRECHG_SHIFT));
}

void bq25890_set_iterm(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON5), (u8)(val),
				 (u8)(CON5_ITERM_MASK), (u8)(CON5_ITERM_SHIFT));
}

/* CON6---------------------------------------------------- */
void bq25890_set_vreg(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON6), (u8)(val),
				 (u8)(CON6_VREG_MASK), (u8)(CON6_VREG_SHIFT));
}

void bq25890_set_batlowv(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON6), (u8)(val),
				 (u8)(CON6_BATLOWV_MASK),
				 (u8)(CON6_BATLOWV_SHIFT));
}

void bq25890_set_vrechg(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON6), (u8)(val),
				 (u8)(CON6_VRECHG_MASK),
				 (u8)(CON6_VRECHG_SHIFT));
}

/* CON7---------------------------------------------------- */
void bq25890_set_en_term(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON7), (u8)(val),
				 (u8)(CON7_EN_TERM_MASK),
				 (u8)(CON7_EN_TERM_SHIFT));
}

void bq25890_set_watchdog(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON7), (u8)(val),
				 (u8)(CON7_WATCHDOG_MASK),
				 (u8)(CON7_WATCHDOG_SHIFT));
}

void bq25890_set_en_timer(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON7), (u8)(val),
				 (u8)(CON7_EN_TIMER_MASK),
				 (u8)(CON7_EN_TIMER_SHIFT));
}

void bq25890_set_chg_timer(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON7), (u8)(val),
				 (u8)(CON7_CHG_TIMER_MASK),
				 (u8)(CON7_CHG_TIMER_SHIFT));
}

/* CON8---------------------------------------------------- */
void bq25890_set_treg(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON8), (u8)(val),
				 (u8)(CON8_TREG_MASK), (u8)(CON8_TREG_SHIFT));
}

/* CON9---------------------------------------------------- */
u32 bq25890_get_pumpx_up(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON9), (&val),
			       (u8)(CON9_PUMPX_UP_MASK),
			       (u8)(CON9_PUMPX_UP_SHIFT));
	return val;
}

void bq25890_set_pumpx_up(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON9), (u8)(val),
				 (u8)(CON9_PUMPX_UP_MASK),
				 (u8)(CON9_PUMPX_UP_SHIFT));
}

u32 bq25890_get_pumpx_dn(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON9), (&val),
			       (u8)(CON9_PUMPX_DN_MASK),
			       (u8)(CON9_PUMPX_DN_SHIFT));
	return val;
}

void bq25890_set_pumpx_dn(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON9), (u8)(val),
				 (u8)(CON9_PUMPX_DN_MASK),
				 (u8)(CON9_PUMPX_DN_SHIFT));
}

void bq25890_set_batfet_disable(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON9), (u8)(val),
				 (u8)(CON9_BATFET_DIS_MASK),
				 (u8)(CON9_BATFET_DIS_SHIFT));
}

/* CON10---------------------------------------------------- */
void bq25890_set_boost_lim(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON10), (u8)(val),
				 (u8)(CON10_BOOST_LIM_MASK),
				 (u8)(CON10_BOOST_LIM_SHIFT));
}

/* CON11---------------------------------------------------- */
u32 bq25890_get_vbus_stat(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON11), (&val),
			       (u8)(CON11_VBUS_STAT_MASK),
			       (u8)(CON11_VBUS_STAT_SHIFT));
	return val;
}

u32 bq25890_get_chrg_stat(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON11), (&val),
			       (u8)(CON11_CHRG_STAT_MASK),
			       (u8)(CON11_CHRG_STAT_SHIFT));
	return val;
}

u32 bq25890_get_pg_stat(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON11), (&val),
			       (u8)(CON11_PG_STAT_MASK),
			       (u8)(CON11_PG_STAT_SHIFT));
	return val;
}

u32 bq25890_get_vsys_stat(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON11), (&val),
			       (u8)(CON11_VSYS_STAT_MASK),
			       (u8)(CON11_VSYS_STAT_SHIFT));
	return val;
}

u32 bq25890_get_system_status(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON11), (&val), (u8)(0xFF),
			       (u8)(0x0));
	return val;
}

/* CON13---------------------------------------------------- */
void bq25890_set_force_vindpm(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON13), (u8)(val),
				 (u8)(CON13_FORCE_VINDPM_MASK),
				 (u8)(CON13_FORCE_VINDPM_SHIFT));
}

void bq25890_set_vindpm(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON13), (u8)(val),
				 (u8)(CON13_VINDPM_MASK),
				 (u8)(CON13_VINDPM_SHIFT));
}

/* CON19---------------------------------------------------- */
u32 bq25890_get_vdpm_stat(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON19), (&val),
			       (u8)(CON19_VDPM_STAT_MASK),
			       (u8)(CON19_VDPM_STAT_SHIFT));
	return val;
}

u32 bq25890_get_idpm_stat(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON19), (&val),
			       (u8)(CON19_IDPM_STAT_MASK),
			       (u8)(CON19_IDPM_STAT_SHIFT));
	return val;
}

u32 bq25890_get_current_iinlim(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON19), (&val),
			       (u8)(CON19_IDPM_LIM_MASK),
			       (u8)(CON19_IDPM_LIM_SHIFT));
	return val;
}

/* CON20---------------------------------------------------- */
void bq25890_set_reg_rst(u32 val)
{
	bq25890_config_interface((u8)(bq25890_CON20), (u8)(val),
				 (u8)(CON20_REG_RST_MASK),
				 (u8)(CON20_REG_RST_SHIFT));
}

u32 bq25890_get_pn(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON20), (&val), (u8)(CON20_PN_MASK),
			       (u8)(CON20_PN_SHIFT));
	return val;
}

u32 bq25890_get_rev(void)
{
	u8 val = 0;

	bq25890_read_interface((u8)(bq25890_CON20), (&val),
			       (u8)(CON20_REV_MASK), (u8)(CON20_REV_SHIFT));
	return val;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
static u32 charging_parameter_to_value(const u32 *parameter,
				       const u32 array_size, const u32 val)
{
	u32 i;

	for (i = 0; i < array_size; i++) {
		if (val == *(parameter + i))
			return i;
	}

	pr_debug("NO register value match. val=%d\r\n", val);
	/* TODO: ASSERT(0);      // not find the value */
	return 0;
}

static u32 bmt_find_closest_level(const u32 *pList, u32 number, u32 level)
{
	u32 i;
	u32 max_value_in_last_element;

	if (pList[0] < pList[1])
		max_value_in_last_element = true;
	else
		max_value_in_last_element = false;

	if (max_value_in_last_element == true) {
		for (i = (number - 1); i != 0;
		     i--) { /* max value in the last element */
			if (pList[i] <= level)
				return pList[i];
		}

		pr_debug("Can't find closest level, small value first \r\n");
		return pList[0];
	}

	for (i = 0; i < number; i++) { /* max value in the first element */
		if (pList[i] <= level)
			return pList[i];
	}

	pr_debug("Can't find closest level, large value first \r\n");
	return pList[number - 1];
}

static u32 charging_hw_init(void *data)
{
	u32 status = STATUS_OK;

	upmu_set_rg_bc11_bb_ctrl(1); /* BC11_BB_CTRL */
	upmu_set_rg_bc11_rst(1);     /* BC11_RST */

	/* ICO_EN default off */
	bq25890_config_interface(bq25890_CON2, 0x0, CON2_ICO_EN_MASK,
				 CON2_ICO_EN_SHIFT);
	/* disable MAXC_EN and HVDCP_EN */
	bq25890_config_interface(bq25890_CON2, 0x0, CON2_HVDCP_EN_MASK,
				 CON2_HVDCP_EN_SHIFT);
	bq25890_config_interface(bq25890_CON2, 0x0, CON2_MAXC_EN_MASK,
				 CON2_MAXC_EN_SHIFT);

	/* AUTO_DPDM_EN default on */
	bq25890_config_interface(bq25890_CON2, 0x1, CON2_AUTO_DPDM_MASK,
				 CON2_AUTO_DPDM_SHIFT);

	bq25890_set_en_hiz(0x0);
	bq25890_set_force_vindpm(0x1); /* Run absolute VINDPM */
	bq25890_set_vindpm(0x14);      /* VIN DPM check 4.60V */
	bq25890_set_reg_rst(0x0);
	bq25890_set_wdt_rst(0x1); /* Kick watchdog */
	bq25890_set_sys_min(0x5); /* Minimum system voltage 3.5V */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq25890_set_iprechg(0x3); /* Precharge current 256mA */
#else
	bq25890_set_iprechg(0x7); /* Precharge current 512mA */
#endif
	bq25890_set_iterm(0x1); /* Termination current 128mA */

#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq25890_set_vreg(0x17); /* VREG 4.208V */
#endif
	bq25890_set_batlowv(0x1);  /* BATLOWV 3.0V */
	bq25890_set_vrechg(0x0);   /* VRECHG 0.1V (4.108V) */
	bq25890_set_en_term(0x1);  /* Enable termination */
	bq25890_set_watchdog(0x1); /* WDT 40s */
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq25890_set_en_timer(0x0); /* Disable charge timer */
#endif

	return status;
}

static u32 charging_dump_register(void *data)
{
	u32 status = STATUS_OK;

	pr_debug("charging_dump_register\r\n");
	bq25890_dump_register();
	return status;
}

static u32 charging_enable(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);

	if (true == enable) {
		bq25890_set_en_hiz(0x0);
		bq25890_set_chg_config(0x1);
	} else
		bq25890_set_chg_config(0x0);

	return status;
}

static u32 charging_set_cv_voltage(void *data)
{
	u32 status = STATUS_OK;
	u16 register_value;
	u32 cv_value = *(u32 *)(data);

	if (cv_value == BATTERY_VOLT_04_200000_V)
		cv_value = 4208000; /* use nearest value */

	if (cv_value <= 3840000)
		cv_value = 3840000;

	register_value = (cv_value / 1000 - 3840) / 16;
	bq25890_set_vreg(register_value);

	return status;
}

static u32 charging_get_current(void *data)
{
	u32 status = STATUS_OK;
	u32 data_val = 0;

	/* to CHR_CURRENT_ENUM */
	data_val = bq25890_get_ichg() * 64 * 100;
	*(u32 *)data = data_val;

	return status;
}

static u32 charging_set_current(void *data)
{
	u32 status = STATUS_OK;
	u32 register_value;
	u32 current_value = (*(u32 *)data) / 100;

	register_value = current_value / 64;

	if (register_value > 0x4F)
		register_value = 0x4F;

	bq25890_set_ichg(register_value);
	return status;
}

static u32 charging_set_input_current(void *data)
{
	u32 status = STATUS_OK;
	u32 set_chr_current;
	u32 register_value;

	set_chr_current = (*(u32 *)data) / 100;
	register_value = (set_chr_current - 100) / 50;

	if (register_value > 0x3F)
		register_value = 0x3F;

	bq25890_set_iinlim(register_value);
	return status;
}

static u32 charging_get_input_current(void *data)
{
	u32 register_value;

	register_value = bq25890_get_iinlim();
	*(u32 *)data =
		(100 + 50 * register_value) * 100; /* to CHR_CURRENT_ENUM */
	return STATUS_OK;
}

static u32 charging_get_charging_status(void *data)
{
	u32 status = STATUS_OK;
	u32 ret_val;

	ret_val = bq25890_get_chrg_stat();

	if (ret_val == 0x3)
		*(u32 *)data = true;
	else
		*(u32 *)data = false;

	return status;
}

static u32 charging_reset_watch_dog_timer(void *data)
{
	u32 status = STATUS_OK;

	pr_debug("charging_reset_watch_dog_timer\r\n");
	bq25890_set_wdt_rst(0x1); /* Kick watchdog */
	return status;
}

static u32 charging_set_hv_threshold(void *data)
{
	u32 status = STATUS_OK;

	u32 set_hv_voltage;
	u32 array_size;
	u16 register_value;
	u32 voltage = *(u32 *)(data);

	array_size = GETARRAYNUM(VCDT_HV_VTH);
	set_hv_voltage =
		bmt_find_closest_level(VCDT_HV_VTH, array_size, voltage);
	register_value = charging_parameter_to_value(VCDT_HV_VTH, array_size,
						     set_hv_voltage);
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
	int __maybe_unused charger_type = CHARGER_UNKNOWN;
	u32 __maybe_unused ret_val;
	u32 __maybe_unused vbus_state;
	u8 __maybe_unused reg_val = 0;
	u32 __maybe_unused count = 0;

#if defined(CONFIG_MTK_BQ25896_SUPPORT)
	*(int *)(data) = hw_charger_type_detection();
#else

	pr_debug("use BQ25890 charger detection\r\n");

	Charger_Detect_Init();

	while (bq25890_get_pg_stat() == 0) {
		pr_debug("wait pg_state ready.\n");
		count++;
		msleep(20);
		if (count > 500) {
			pr_debug("wait BQ25890 pg_state ready timeout!\n");
			break;
		}

		if (!upmu_get_rgs_chrdet())
			break;
	}

	ret_val = bq25890_get_vbus_stat();

	/* if detection is not finished */
	if (ret_val == 0x0) {
		count = 0;
		bq25890_set_force_dpdm(1);
		while (bq25890_get_dpdm_status() == 1) {
			count++;
			mdelay(1);
			pr_debug("polling BQ25890 charger detection\r\n");
			if (count > 1000)
				break;
			if (!upmu_get_rgs_chrdet())
				break;
		}
	}

	vbus_state = bq25890_get_vbus_stat();

	/* We might not be able to switch on RG_USB20_BC11_SW_EN in time. */
	/* We detect again to confirm its type */
	if (upmu_get_rgs_chrdet()) {
		count = 0;
		bq25890_set_force_dpdm(1);
		while (bq25890_get_dpdm_status() == 1) {
			count++;
			mdelay(1);
			pr_debug("polling again BQ25890 charger detection\r\n");
			if (count > 1000)
				break;
			if (!upmu_get_rgs_chrdet())
				break;
		}
	}

	ret_val = bq25890_get_vbus_stat();

	if (ret_val != vbus_state)
		pr_debug("Update VBUS state from %d to %d!\n", vbus_state,
			ret_val);

	switch (ret_val) {
	case 0x0:
		charger_type = CHARGER_UNKNOWN;
		break;
	case 0x1:
		charger_type = STANDARD_HOST;
		break;
	case 0x2:
		charger_type = CHARGING_HOST;
		break;
	case 0x3:
		charger_type = STANDARD_CHARGER;
		break;
	case 0x4:
		charger_type = STANDARD_CHARGER; /* MaxCharge DCP */
		break;
	case 0x5:
		charger_type = NONSTANDARD_CHARGER;
		break;
	case 0x6:
		charger_type = NONSTANDARD_CHARGER;
		break;
	case 0x7:
		charger_type = CHARGER_UNKNOWN; /* OTG */
		break;

	default:
		charger_type = CHARGER_UNKNOWN;
		break;
	}

	if (charger_type == NONSTANDARD_CHARGER) {
		reg_val = bq25890_get_iinlim();
		if (reg_val < 0x12) {
			pr_debug("Set to Non-standard charger due to 1A input limit.\r\n");
			charger_type = NONSTANDARD_CHARGER;
		} else if (reg_val == 0x12) { /* 1A charger */
			pr_debug("Set to APPLE_1_0A_CHARGER.\r\n");
			charger_type = APPLE_1_0A_CHARGER;
		} else if (reg_val >= 0x26) { /* 2A/2.1A/2.4A charger */
			pr_debug("Set to APPLE_2_1A_CHARGER.\r\n");
			charger_type = APPLE_2_1A_CHARGER;
		}
	}

	Charger_Detect_Release();

	pr_debug("charging_get_charger_type = %d\n", charger_type);

	*(int *)(data) = charger_type;

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
		bq25890_set_en_hiz(0x0);
	else
		bq25890_set_en_hiz(0x1);

	return status;
}

static u32 charging_boost_enable(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);

	if (true == enable) {
		bq25890_set_boost_lim(0x2); /* 1.2A on VBUS */
		bq25890_set_en_hiz(0x0);
		bq25890_set_chg_config(0);   /* Charge disabled */
		bq25890_set_otg_config(0x1); /* OTG */
#ifdef CONFIG_POWER_EXT
		bq25890_set_watchdog(0);
#endif
	} else {
		bq25890_set_otg_config(0x0); /* OTG & Charge disabled */
#ifdef CONFIG_POWER_EXT
		bq25890_set_watchdog(1);
#endif
	}

	return status;
}

static u32 charging_set_ta_current_pattern(void *data)
{
	u32 increase = *(u32 *)(data);
	u32 count = 0;

	/* input current limit = 800 mA */
	bq25890_set_iinlim(0xE);
	/* cc mode current = 2048 mA */
	bq25890_set_ichg(0x20);

	bq25890_set_pumpx_en(1);

	if (increase == 1) {
		bq25890_set_pumpx_up(1);
		while (bq25890_get_pumpx_up() == 1) {
			count++;
			if (!upmu_get_rgs_chrdet() || count >= 20) {
				bq25890_set_pumpx_en(0);
				break;
			}
			msleep(200);
		}
	} else {
		bq25890_set_pumpx_dn(1);
		while (bq25890_get_pumpx_dn() == 1) {
			count++;
			if (!upmu_get_rgs_chrdet() || count >= 20) {
				bq25890_set_pumpx_en(0);
				break;
			}
			msleep(200);
		}
	}

	return STATUS_OK;
}

static u32 (*charging_func[CHARGING_CMD_NUMBER])(void *data);

s32 bq25890_control_interface(int cmd, void *data)
{
	s32 status;

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

void bq25890_dump_register(void)
{
	int i = 0;

	for (i = 0; i < bq25890_REG_NUM; i++) {
		bq25890_read_byte(i, &bq25890_reg[i]);
		pr_debug("[bq25890_dump_register] Reg[0x%X]=0x%X\n", i,
			    bq25890_reg[i]);
	}
}

static int bq25890_driver_suspend(struct i2c_client *client, pm_message_t mesg)
{
	return 0;
}

static int bq25890_driver_resume(struct i2c_client *client)
{
	return 0;
}

static void bq25890_driver_shutdown(struct i2c_client *client)
{
}

static int bq25890_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	pr_info("[bq25890_driver_probe]\n");

	new_client = client;

	if (bq25890_get_pn() == 0x2 || bq25890_get_pn() == 0x0) {
		pr_notice(
			"BQ25890/BQ25896 device is found. register charger control.\n");
		bat_charger_register(bq25890_control_interface);
	} else {
		pr_notice("No BQ25890/BQ25896 device part number is found.\n");
		return 0;
	}

	bq25890_dump_register();

	bat_charger_register(bq25890_control_interface);

	return 0;
}

static const struct i2c_device_id bq25890_i2c_id[] = {{"bq25890", 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id bq25890_id[] = {
	{.compatible = "ti,bq25890"}, {.compatible = "ti,bq25896"}, {},
};

MODULE_DEVICE_TABLE(of, bq25890_id);
#endif

static struct i2c_driver bq25890_driver = {
	.driver = {

			.name = "bq25890",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(bq25890_id),
#endif
		},
	.probe = bq25890_driver_probe,
	.shutdown = bq25890_driver_shutdown,
	.suspend = bq25890_driver_suspend,
	.resume = bq25890_driver_resume,
	.id_table = bq25890_i2c_id,
};

static ssize_t show_bq25890_access(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	pr_info("[show_bq25890_access] 0x%x\n", g_reg_value_bq25890);
	return sprintf(buf, "0x%x\n", g_reg_value_bq25890);
}

static ssize_t store_bq25890_access(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t size)
{
	int ret = 0;
	char *pvalue = NULL, temp_buf[16];
	unsigned int reg_value = 0;
	unsigned int reg_address = 0;

	pr_info("[store_bq25890_access]\n");

	if (buf != NULL && size != 0) {
		strncpy(temp_buf, buf, 15);
		temp_buf[15] = '\0';
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
				"[store_bq25890_access] write bq25890 reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			bq25890_config_interface(reg_address, reg_value, 0xFF,
						 0x0);
		} else {
			ret = kstrtouint(pvalue, 0, &reg_address);
			if (ret) {
				pr_debug("wrong format!\n");
				return size;
			}
			bq25890_read_interface(reg_address,
					       &g_reg_value_bq25890, 0xFF, 0x0);
			pr_debug(
				"[store_bq25890_access] read bq25890 reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value_bq25890);
			pr_debug(
				"[store_bq25890_access] Please use \"cat bq25890_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(bq25890_access, 0644, show_bq25890_access,
		   store_bq25890_access);

static int bq25890_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_info("bq25890_user_space_probe!\n");
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_bq25890_access);

	return 0;
}

struct platform_device bq25890_user_space_device = {
	.name = "bq25890-user", .id = -1,
};

static struct platform_driver bq25890_user_space_driver = {
	.probe = bq25890_user_space_probe,
	.driver = {

			.name = "bq25890-user",
		},
};

static int __init bq25890_init(void)
{
	int ret = 0;

	if (i2c_add_driver(&bq25890_driver) != 0)
		pr_debug("[bq25890_init] failed to register bq25890 i2c driver.\n");
	else
		pr_info("[bq25890_init] Success to register bq25890 i2c driver.\n");

	/* bq25890 user space access interface */
	ret = platform_device_register(&bq25890_user_space_device);
	if (ret) {
		pr_debug("[bq25890_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&bq25890_user_space_driver);
	if (ret) {
		pr_debug("[bq25890_init] Unable to register driver (%d)\n",
			 ret);
		return ret;
	}

	return 0;
}

static void __exit bq25890_exit(void)
{
	i2c_del_driver(&bq25890_driver);
}
module_init(bq25890_init);
module_exit(bq25890_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq25890 Driver");
MODULE_AUTHOR("MengHui Lin<menghui.lin@mediatek.com>");
