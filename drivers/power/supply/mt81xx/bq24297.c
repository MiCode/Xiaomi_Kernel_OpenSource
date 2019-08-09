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
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/workqueue.h>

#include "bq24297.h"
#include "mt_charging.h"
#include <mt-plat/mtk_boot.h>
#include <mt-plat/upmu_common.h>
#include <mach/upmu_sw.h>

/**********************************************************
 *
 *    [Define]
 *
 **********************************************************/

#define STATUS_OK 0
#define STATUS_UNSUPPORTED -1
#define GETARRAYNUM(array) (ARRAY_SIZE(array))
#define bq24297_REG_NUM 11
#define bq24297_SLAVE_ADDR_WRITE 0xD6
#define bq24297_SLAVE_ADDR_READ 0xD7

/**********************************************************
 *
 *   [Global Variable]
 *
 *********************************************************/

static struct i2c_client *new_client;
static bool charging_type_det_done = true;
static u8 bq24297_reg[bq24297_REG_NUM] = {0};

static u32 part_num;
static u8 g_reg_value_bq24297;

static const u32 VBAT_CV_VTH[] = {
	3504000, 3520000, 3536000, 3552000, 3568000, 3584000, 3600000, 3616000,
	3632000, 3648000, 3664000, 3680000, 3696000, 3712000, 3728000, 3744000,
	3760000, 3776000, 3792000, 3808000, 3824000, 3840000, 3856000, 3872000,
	3888000, 3904000, 3920000, 3936000, 3952000, 3968000, 3984000, 4000000,
	4016000, 4032000, 4048000, 4064000, 4080000, 4096000, 4112000, 4128000,
	4144000, 4160000, 4176000, 4192000, 4208000, 4224000, 4240000, 4256000};

static const u32 CS_VTH[] = {51200,  57600,  64000,  70400,  76800,  83200,
			     89600,  96000,  102400, 108800, 115200, 121600,
			     128000, 134400, 140800, 147200, 153600, 160000,
			     166400, 172800, 179200, 185600, 192000, 198400,
			     204800, 211200, 217600, 224000};

static const u32 INPUT_CS_VTH[] = {
	CHARGE_CURRENT_100_00_MA,  CHARGE_CURRENT_150_00_MA,
	CHARGE_CURRENT_500_00_MA,  CHARGE_CURRENT_900_00_MA,
	CHARGE_CURRENT_1000_00_MA, CHARGE_CURRENT_1500_00_MA,
	CHARGE_CURRENT_1800_00_MA, CHARGE_CURRENT_1800_00_MA};

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

__attribute__((weak)) void Charger_Detect_Init(void)
{
	pr_debug("need usb porting!\n");
}

__attribute__((weak)) void Charger_Detect_Release(void)
{
	pr_debug("need usb porting!\n");
}

__attribute__((weak)) int slp_get_wake_reason(void)
{
	pr_debug("need spm porting!\n");
	return 0;
}
/**********************************************************
 *
 *   [I2C Function For Read/Write bq24297]
 *
 *********************************************************/

static int bq24297_i2c_alloc(void)
{
	u8 *i2c_buf;

	i2c_buf = kzalloc(2, GFP_KERNEL);
	if (!i2c_buf)
		return -1;

	i2c_set_clientdata(new_client, i2c_buf);
	return 0;
}

static void bq24297_i2c_release(void)
{
	kfree(i2c_get_clientdata(new_client));
	i2c_set_clientdata(new_client, NULL);
}

int bq24297_read_byte(u8 cmd, u8 *data)
{
	static u8 *i2c_addr, *i2c_buff;
	int ret;

	struct i2c_msg msg[2];

	if (!new_client) {
		pr_debug("error: access bq24297 before driver ready\n");
		return 0;
	}

	i2c_addr = i2c_get_clientdata(new_client);
	if (i2c_addr == NULL) {
		pr_debug("error: NO I2C buffer\n");
		return 0;
	}
	memcpy(i2c_addr, &cmd, 1);
	i2c_buff = i2c_addr + 1;

	msg[0].addr = new_client->addr;
	msg[0].buf = i2c_addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[1].addr = new_client->addr;
	msg[1].buf = i2c_buff;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;

	ret = i2c_transfer(new_client->adapter, msg, 2);

	if (ret != 2)
		pr_debug("%s: err=%d\n", __func__, ret);

	*data = *i2c_buff;

	return ret == 2 ? 1 : 0;
}

int bq24297_write_byte(u8 cmd, u8 data)
{
	static u8 *i2c_buff;
	int ret;

	if (!new_client) {
		pr_debug("error: access bq24297 before driver ready\n");
		return 0;
	}

	i2c_buff = i2c_get_clientdata(new_client);
	if (i2c_buff == NULL) {
		pr_debug("error: NO I2C buffer\n");
		return 0;
	}

	*i2c_buff = cmd;
	memcpy(i2c_buff + 1, &data, 1);

	ret = i2c_master_send(new_client, i2c_buff, 2);
	if (ret != 2)
		pr_debug("%s: err=%d\n", __func__, ret);

	return ret == 2 ? 1 : 0;
}

u32 bq24297_read_interface(u8 RegNum, u8 *val, u8 MASK, u8 SHIFT)
{
	u8 bq24297_reg = 0;
	int ret = 0;

	ret = bq24297_read_byte(RegNum, &bq24297_reg);

	bq24297_reg &= (MASK << SHIFT);
	*val = (bq24297_reg >> SHIFT);

	return ret;
}

u32 bq24297_config_interface(u8 RegNum, u8 val, u8 MASK, u8 SHIFT)
{
	u8 bq24297_reg = 0;
	int ret = 0;

	ret = bq24297_read_byte(RegNum, &bq24297_reg);

	bq24297_reg &= ~(MASK << SHIFT);
	bq24297_reg |= (val << SHIFT);

	ret = bq24297_write_byte(RegNum, bq24297_reg);
	return ret;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/
/* CON0---------------------------------------------------- */

void bq24297_set_en_hiz(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON0), (u8)(val),
				       (u8)(CON0_EN_HIZ_MASK),
				       (u8)(CON0_EN_HIZ_SHIFT));
}

void bq24297_set_vindpm(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON0), (u8)(val),
				       (u8)(CON0_VINDPM_MASK),
				       (u8)(CON0_VINDPM_SHIFT));
}

void bq24297_set_iinlim(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON0), (u8)(val),
				       (u8)(CON0_IINLIM_MASK),
				       (u8)(CON0_IINLIM_SHIFT));
}

u32 bq24297_get_iinlim(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24297_read_interface((u8)(bq24297_CON0), (&val),
				     (u8)(CON0_IINLIM_MASK),
				     (u8)(CON0_IINLIM_SHIFT));
	return val;
}

/* CON1---------------------------------------------------- */

void bq24297_set_reg_rst(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON1), (u8)(val),
				       (u8)(CON1_REG_RST_MASK),
				       (u8)(CON1_REG_RST_SHIFT));
}

void bq24297_set_wdt_rst(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON1), (u8)(val),
				       (u8)(CON1_WDT_RST_MASK),
				       (u8)(CON1_WDT_RST_SHIFT));
}

void bq24297_set_otg_config(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON1), (u8)(val),
				       (u8)(CON1_OTG_CONFIG_MASK),
				       (u8)(CON1_OTG_CONFIG_SHIFT));
}

void bq24297_set_chg_config(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON1), (u8)(val),
				       (u8)(CON1_CHG_CONFIG_MASK),
				       (u8)(CON1_CHG_CONFIG_SHIFT));
}

void bq24297_set_sys_min(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON1), (u8)(val),
				       (u8)(CON1_SYS_MIN_MASK),
				       (u8)(CON1_SYS_MIN_SHIFT));
}

void bq24297_set_boost_lim(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON1), (u8)(val),
				       (u8)(CON1_BOOST_LIM_MASK),
				       (u8)(CON1_BOOST_LIM_SHIFT));
}

/* CON2---------------------------------------------------- */

void bq24297_set_ichg(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON2), (u8)(val),
				       (u8)(CON2_ICHG_MASK),
				       (u8)(CON2_ICHG_SHIFT));
}

void bq24297_set_force_20pct(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON2), (u8)(val),
				       (u8)(CON2_FORCE_20PCT_MASK),
				       (u8)(CON2_FORCE_20PCT_SHIFT));
}

/* CON3---------------------------------------------------- */

void bq24297_set_iprechg(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON3), (u8)(val),
				       (u8)(CON3_IPRECHG_MASK),
				       (u8)(CON3_IPRECHG_SHIFT));
}

void bq24297_set_iterm(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON3), (u8)(val),
				       (u8)(CON3_ITERM_MASK),
				       (u8)(CON3_ITERM_SHIFT));
}

/* CON4---------------------------------------------------- */

void bq24297_set_vreg(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON4), (u8)(val),
				       (u8)(CON4_VREG_MASK),
				       (u8)(CON4_VREG_SHIFT));
}

void bq24297_set_batlowv(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON4), (u8)(val),
				       (u8)(CON4_BATLOWV_MASK),
				       (u8)(CON4_BATLOWV_SHIFT));
}

void bq24297_set_vrechg(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON4), (u8)(val),
				       (u8)(CON4_VRECHG_MASK),
				       (u8)(CON4_VRECHG_SHIFT));
}

/* CON5---------------------------------------------------- */

void bq24297_set_en_term(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON5), (u8)(val),
				       (u8)(CON5_EN_TERM_MASK),
				       (u8)(CON5_EN_TERM_SHIFT));
}

void bq24297_set_term_stat(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON5), (u8)(val),
				       (u8)(CON5_TERM_STAT_MASK),
				       (u8)(CON5_TERM_STAT_SHIFT));
}

void bq24297_set_watchdog(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON5), (u8)(val),
				       (u8)(CON5_WATCHDOG_MASK),
				       (u8)(CON5_WATCHDOG_SHIFT));
}

void bq24297_set_en_timer(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON5), (u8)(val),
				       (u8)(CON5_EN_TIMER_MASK),
				       (u8)(CON5_EN_TIMER_SHIFT));
}

void bq24297_set_chg_timer(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON5), (u8)(val),
				       (u8)(CON5_CHG_TIMER_MASK),
				       (u8)(CON5_CHG_TIMER_SHIFT));
}

/* CON6---------------------------------------------------- */

void bq24297_set_treg(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON6), (u8)(val),
				       (u8)(CON6_TREG_MASK),
				       (u8)(CON6_TREG_SHIFT));
}

/* CON7---------------------------------------------------- */
u32 bq24297_get_dpdm_status(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24297_read_interface((u8)(bq24297_CON7), (&val),
				     (u8)(CON7_DPDM_EN_MASK),
				     (u8)(CON7_DPDM_EN_SHIFT));
	return val;
}

void bq24297_set_dpdm_en(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON7), (u8)(val),
				       (u8)(CON7_DPDM_EN_MASK),
				       (u8)(CON7_DPDM_EN_SHIFT));
}

void bq24297_set_tmr2x_en(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON7), (u8)(val),
				       (u8)(CON7_TMR2X_EN_MASK),
				       (u8)(CON7_TMR2X_EN_SHIFT));
}

void bq24297_set_batfet_disable(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON7), (u8)(val),
				       (u8)(CON7_BATFET_Disable_MASK),
				       (u8)(CON7_BATFET_Disable_SHIFT));
}

void bq24297_set_int_mask(u32 val)
{
	u32 ret = 0;

	ret = bq24297_config_interface((u8)(bq24297_CON7), (u8)(val),
				       (u8)(CON7_INT_MASK_MASK),
				       (u8)(CON7_INT_MASK_SHIFT));
}

/* CON8---------------------------------------------------- */

u32 bq24297_get_system_status(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24297_read_interface((u8)(bq24297_CON8), (&val), (u8)(0xFF),
				     (u8)(0x0));
	return val;
}

u32 bq24297_get_vbus_stat(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24297_read_interface((u8)(bq24297_CON8), (&val),
				     (u8)(CON8_VBUS_STAT_MASK),
				     (u8)(CON8_VBUS_STAT_SHIFT));
	return val;
}

u32 bq24297_get_chrg_stat(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24297_read_interface((u8)(bq24297_CON8), (&val),
				     (u8)(CON8_CHRG_STAT_MASK),
				     (u8)(CON8_CHRG_STAT_SHIFT));
	return val;
}

u32 bq24297_get_pg_stat(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24297_read_interface((u8)(bq24297_CON8), (&val),
				     (u8)(CON8_PG_STAT_MASK),
				     (u8)(CON8_PG_STAT_SHIFT));
	return val;
}

u32 bq24297_get_vsys_stat(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24297_read_interface((u8)(bq24297_CON8), (&val),
				     (u8)(CON8_VSYS_STAT_MASK),
				     (u8)(CON8_VSYS_STAT_SHIFT));
	return val;
}

/* CON10---------------------------------------------------- */

u32 bq24297_get_pn(void)
{
	u32 ret = 0;
	u8 val = 0;

	ret = bq24297_read_interface((u8)(bq24297_CON10), (&val),
				     (u8)(CON10_PN_MASK), (u8)(CON10_PN_SHIFT));
	return val;
}

/**********************************************************
 *
 *   [Internal Function]
 *
 *********************************************************/

u32 charging_value_to_parameter(const u32 *parameter, const u32 array_size,
				const u32 val)
{
	if (val < array_size)
		return parameter[val];

	pr_debug("Can't find the parameter \r\n");
	return parameter[0];
}

u32 charging_parameter_to_value(const u32 *parameter, const u32 array_size,
				const u32 val)
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

	bq24297_set_en_hiz(0x0);
	bq24297_set_vindpm(0x9); /* VIN DPM check 4.60V */
	bq24297_set_reg_rst(0x0);
	bq24297_set_wdt_rst(0x1); /* Kick watchdog */
	bq24297_set_sys_min(0x5); /* Minimum system voltage 3.5V */
#if defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq24297_set_iprechg(0x1); /* Precharge current 256mA */
#else
	bq24297_set_iprechg(0x3); /* Precharge current 512mA */
#endif
	bq24297_set_iterm(0x0);     /* Termination current 128mA */
	bq24297_set_batlowv(0x1);   /* BATLOWV 3.0V */
	bq24297_set_vrechg(0x0);    /* VRECHG 0.1V (4.108V) */
	bq24297_set_en_term(0x1);   /* Enable termination */
	bq24297_set_term_stat(0x0); /* Match ITERM */
	bq24297_set_watchdog(0x1);  /* WDT 40s */
#if !defined(CONFIG_MTK_JEITA_STANDARD_SUPPORT)
	bq24297_set_en_timer(0x0); /* Disable charge timer */
#endif
	bq24297_set_int_mask(0x1); /* Disable CHRG fault interrupt */

	return status;
}

static u32 charging_dump_register(void *data)
{
	u32 status = STATUS_OK;

	pr_debug("charging_dump_register\r\n");

	bq24297_dump_register();

	return status;
}

static u32 charging_enable(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);

	if (true == enable) {
		bq24297_set_en_hiz(0x0);
		bq24297_set_chg_config(0x1);
	} else
		bq24297_set_chg_config(0x0);

	return status;
}

static u32 charging_set_cv_voltage(void *data)
{
	u32 status = STATUS_OK;
	u16 register_value;
	u32 cv_value = *(u32 *)(data);

	if (cv_value == BATTERY_VOLT_04_200000_V) {
		/* use nearest value */
		cv_value = 4208000;
	}
	register_value = charging_parameter_to_value(
		VBAT_CV_VTH, GETARRAYNUM(VBAT_CV_VTH), cv_value);
	bq24297_set_vreg(register_value);

	return status;
}

static u32 charging_get_current(void *data)
{
	u32 status = STATUS_OK;
	u32 data_val = 0;
	u8 ret_val = 0;
	u8 ret_force_20pct = 0;

	bq24297_read_interface(bq24297_CON2, &ret_val, CON2_ICHG_MASK,
			       CON2_ICHG_SHIFT);
	bq24297_read_interface(bq24297_CON2, &ret_force_20pct,
			       CON2_FORCE_20PCT_MASK, CON2_FORCE_20PCT_SHIFT);

	data_val = (ret_val * 64) + 512;

	if (ret_force_20pct == 0)
		*(u32 *)data = data_val;
	else
		*(u32 *)data = data_val / 5;

	return status;
}

static u32 charging_set_current(void *data)
{
	u32 status = STATUS_OK;
	u32 set_chr_current;
	u32 array_size;
	u32 register_value;
	u32 current_value = *(u32 *)data;

	if (current_value == 25600) {
		bq24297_set_force_20pct(0x1);
		bq24297_set_ichg(0xC);
		return status;
	}
	bq24297_set_force_20pct(0x0);

	array_size = GETARRAYNUM(CS_VTH);
	set_chr_current =
		bmt_find_closest_level(CS_VTH, array_size, current_value);
	register_value = charging_parameter_to_value(CS_VTH, array_size,
						     set_chr_current);
	bq24297_set_ichg(register_value);

	return status;
}

static u32 charging_set_input_current(void *data)
{
	u32 status = STATUS_OK;
	u32 set_chr_current;
	u32 array_size;
	u32 register_value;

	array_size = GETARRAYNUM(INPUT_CS_VTH);
	set_chr_current =
		bmt_find_closest_level(INPUT_CS_VTH, array_size, *(u32 *)data);
	register_value = charging_parameter_to_value(INPUT_CS_VTH, array_size,
						     set_chr_current);

	bq24297_set_iinlim(register_value);
	return status;
}

static u32 charging_get_input_current(void *data)
{
	u32 register_value;

	register_value = bq24297_get_iinlim();
	*(u32 *)data = INPUT_CS_VTH[register_value];
	return STATUS_OK;
}

static u32 charging_get_charging_status(void *data)
{
	u32 status = STATUS_OK;
	u32 ret_val;

	ret_val = bq24297_get_chrg_stat();

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
	bq24297_set_wdt_rst(0x1); /* Kick watchdog */
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
	int charger_type = CHARGER_UNKNOWN;
	u32 ret_val;
	u32 vbus_state;
	u8 reg_val = 0;
	u32 count = 0;

#if 0 /*defined(CONFIG_POWER_EXT) */
	*(int *)(data) = STANDARD_HOST;
#else

	charging_type_det_done = false;

	/* for BQ24296 */
	if (part_num == 0x1) {
		*(int *)(data) = hw_charger_type_detection();
		charging_type_det_done = true;
		return status;
	}

	pr_debug("use BQ24297 charger detection\r\n");

	Charger_Detect_Init();

	while (bq24297_get_pg_stat() == 0) {
		pr_debug("wait pg_state ready.\n");
		count++;
		msleep(20);
		if (count > 500) {
			pr_debug("wait BQ24297 pg_state ready timeout!\n");
			break;
		}

		if (!upmu_get_rgs_chrdet())
			break;
	}

	ret_val = bq24297_get_vbus_stat();

	/* if detection is not finished or non-standard charger detected. */
	if (ret_val == 0x0) {
		count = 0;
		bq24297_set_dpdm_en(1);
		while (bq24297_get_dpdm_status() == 1) {
			count++;
			mdelay(1);
			pr_debug("polling BQ24297 charger detection\r\n");
			if (count > 1000)
				break;
			if (!upmu_get_rgs_chrdet())
				break;
		}
	}

	vbus_state = bq24297_get_vbus_stat();

	/* We might not be able to switch on RG_USB20_BC11_SW_EN in time. */
	/* We detect again to confirm its type */
	if (upmu_get_rgs_chrdet()) {
		count = 0;
		bq24297_set_dpdm_en(1);
		while (bq24297_get_dpdm_status() == 1) {
			count++;
			mdelay(1);
			pr_debug("polling again BQ24297 charger detection\r\n");
			if (count > 1000)
				break;
			if (!upmu_get_rgs_chrdet())
				break;
		}
	}

	ret_val = bq24297_get_vbus_stat();

	if (ret_val != vbus_state)
		pr_debug("Update VBUS state from %d to %d!\n", vbus_state,
			ret_val);

	switch (ret_val) {
	case 0x1:
		charger_type = STANDARD_HOST;
		break;
	case 0x2:
		charger_type = STANDARD_CHARGER;
		break;
	default:
		charger_type = NONSTANDARD_CHARGER;
		break;
	}

	if (charger_type == STANDARD_CHARGER) {
		bq24297_read_interface(bq24297_CON0, &reg_val, CON0_IINLIM_MASK,
				       CON0_IINLIM_SHIFT);
		if (reg_val < 0x4) {
			pr_debug("Set to Non-standard charger due to 1A input limit.\r\n");
			charger_type = NONSTANDARD_CHARGER;
		} else if (reg_val ==
			   0x4) { /* APPLE_1_0A_CHARGER - 1A apple charger */
			pr_debug("Set to APPLE_1_0A_CHARGER.\r\n");
			charger_type = APPLE_1_0A_CHARGER;
		} else if (reg_val ==
			   0x6) { /* APPLE_2_1A_CHARGER,  2.1A apple charger */
			pr_debug("Set to APPLE_2_1A_CHARGER.\r\n");
			charger_type = APPLE_2_1A_CHARGER;
		}
	}

	Charger_Detect_Release();

	pr_debug("charging_get_charger_type = %d\n", charger_type);

	*(int *)(data) = charger_type;

	charging_type_det_done = true;
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

	*(bool *)(data) = false;
	return status;
}

static u32 charging_set_platform_reset(void *data)
{
	u32 status = STATUS_OK;

	pr_debug("charging_set_platform_reset\n");

	orderly_reboot();

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
		bq24297_set_en_hiz(0x0);
	else
		bq24297_set_en_hiz(0x1);

	return status;
}

static u32 charging_boost_enable(void *data)
{
	u32 status = STATUS_OK;
	u32 enable = *(u32 *)(data);

	if (true == enable) {
		bq24297_set_boost_lim(0x1); /* 1.5A on VBUS */
		bq24297_set_en_hiz(0x0);
		bq24297_set_chg_config(0);   /* Charge disabled */
		bq24297_set_otg_config(0x1); /* OTG */
#ifdef CONFIG_POWER_EXT
		bq24297_set_watchdog(0);
#endif
	} else {
		bq24297_set_otg_config(0x0); /* OTG & Charge disabled */
#ifdef CONFIG_POWER_EXT
		bq24297_set_watchdog(1);
#endif
	}

	return status;
}

static u32 (*charging_func[CHARGING_CMD_NUMBER])(void *data);

s32 bq24297_control_interface(int cmd, void *data)
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

void bq24297_dump_register(void)
{
	int i = 0;

	for (i = 0; i < bq24297_REG_NUM; i++) {
		bq24297_read_byte(i, &bq24297_reg[i]);
		pr_debug("[bq24297_dump_register] Reg[0x%X]=0x%X\n", i,
			    bq24297_reg[i]);
	}
}

u8 bq24297_get_reg9_fault_type(u8 reg9_fault)
{
	u8 ret = 0;
	/* if((reg9_fault & (CON9_WATCHDOG_FAULT_MASK <<
	 * CON9_WATCHDOG_FAULT_SHIFT)) !=0) {
	 *   ret = BQ_WATCHDOG_FAULT;
	 * } else
	 */
	if ((reg9_fault & (CON9_OTG_FAULT_MASK << CON9_OTG_FAULT_SHIFT)) != 0) {
		ret = BQ_OTG_FAULT;
	} else if ((reg9_fault &
		    (CON9_CHRG_FAULT_MASK << CON9_CHRG_FAULT_SHIFT)) != 0) {
		if ((reg9_fault & (CON9_CHRG_INPUT_FAULT_MASK
				   << CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_INPUT_FAULT;
		else if ((reg9_fault & (CON9_CHRG_THERMAL_SHUTDOWN_FAULT_MASK
					<< CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_THERMAL_FAULT;
		else if ((reg9_fault & (CON9_CHRG_TIMER_EXPIRATION_FAULT_MASK
					<< CON9_CHRG_FAULT_SHIFT)) != 0)
			ret = BQ_CHRG_TIMER_EXPIRATION_FAULT;
	} else if ((reg9_fault &
		    (CON9_BAT_FAULT_MASK << CON9_BAT_FAULT_SHIFT)) != 0)
		ret = BQ_BAT_FAULT;
	else if ((reg9_fault & (CON9_NTC_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) !=
		 0) {
		if ((reg9_fault &
		     (CON9_NTC_COLD_FAULT_MASK << CON9_NTC_FAULT_SHIFT)) != 0)
			ret = BQ_NTC_COLD_FAULT;
		else if ((reg9_fault & (CON9_NTC_HOT_FAULT_MASK
					<< CON9_NTC_FAULT_SHIFT)) != 0)
			ret = BQ_NTC_HOT_FAULT;
	}
	return ret;
}

void bq24297_polling_reg09(void)
{
	int i, i2;
	u8 reg1;

	for (i2 = i = 0; i < 4 && i2 < 10; i++, i2++) {
		bq24297_read_byte((u8)(bq24297_CON9),
				  &bq24297_reg[bq24297_CON9]);
		if ((bq24297_reg[bq24297_CON9] & 0x40) !=
		    0) { /* OTG_FAULT bit */
			/* Disable OTG */
			bq24297_read_byte(1, &reg1);
			reg1 &= ~0x20; /* 0 = OTG Disable */
			bq24297_write_byte(1, reg1);
			msleep(20);
			/* Enable OTG */
			reg1 |= 0x20; /* 1 = OTG Enable */
			bq24297_write_byte(1, reg1);
		}
		if (bq24297_reg[bq24297_CON9] != 0) {
			/* keep on polling if reg9 is not 0.
			 * This is to filter noises
			 */
			i = 0;
		}
		msleep(20);
	}
}

static irqreturn_t ops_bq24297_int_handler(int irq, void *dev_id)
{
	bq24297_polling_reg09();
	return IRQ_HANDLED;
}

static int bq24297_driver_suspend(struct device *pdev)
{
	if (!new_client) {
		pr_debug("[%s]error: driver not ready\n", __func__);
		return 0;
	}

	pr_debug("[bq24297_driver_suspend] client->irq(%d)\n", new_client->irq);
	if (new_client->irq > 0)
		disable_irq(new_client->irq);

	return 0;
}

static int bq24297_driver_resume(struct device *pdev)
{
	if (!new_client) {
		pr_debug("[%s]error: driver not ready\n", __func__);
		return 0;
	}

	pr_debug("[bq24297_driver_resume] client->irq(%d)\n", new_client->irq);
	if (new_client->irq > 0)
		enable_irq(new_client->irq);

	return 0;
}

static void bq24297_driver_shutdown(struct i2c_client *client)
{
	pr_debug("[bq24297_driver_shutdown] client->irq(%d)\n", client->irq);
	if (client->irq > 0)
		disable_irq(client->irq);
}

static int bq24297_driver_remove(struct i2c_client *client)
{
	bq24297_i2c_release();
	return 0;
}

static int bq24297_driver_probe(struct i2c_client *client,
				const struct i2c_device_id *id)
{
	int ret = 0;
	struct regulator *i2c_reg = devm_regulator_get(&client->dev, "reg-i2c");

	pr_debug("[bq24297_driver_probe]\n");

	new_client = client;

	ret = bq24297_i2c_alloc();
	if (ret != 0) {
		bq24297_i2c_release();
		return 0;
	}

	if (!IS_ERR(i2c_reg)) {

		ret = regulator_set_voltage(i2c_reg, 1800000, 1800000);
		if (ret != 0)
			dev_dbg(&client->dev,
				"Fail to set 1.8V to reg-i2c: %d\n", ret);

		ret = regulator_get_voltage(i2c_reg);
		pr_debug("bq24297 i2c voltage: %d\n", ret);

		ret = regulator_enable(i2c_reg);
		if (ret != 0)
			dev_dbg(&client->dev, "Fail to enable reg-i2c: %d\n",
				ret);
	}

	part_num = bq24297_get_pn();

	if (part_num == 0x3) {
		pr_debug("BQ24297 device is found. register charger control.\n");
		bat_charger_register(bq24297_control_interface);
	} else if (part_num == 0x1) {
		pr_debug("BQ24296 device is found. register charger control.\n");
		bat_charger_register(bq24297_control_interface);
	} else {
		pr_debug("No BQ24297 device part number is found.\n");
		return 0;
	}

	bq24297_dump_register();

	if (client->irq > 0) {

		pr_debug("[bq24297_driver_probe] enable interrupt: %d\n",
			 client->irq);
		/* make sure we clean REG9 before enable fault interrupt */
		bq24297_read_byte((u8)(bq24297_CON9), &bq24297_reg[9]);
		if (bq24297_reg[9] != 0)
			bq24297_polling_reg09();

		bq24297_set_int_mask(0x1); /* Disable CHRG fault interrupt */
		ret = request_threaded_irq(client->irq, NULL,
					   ops_bq24297_int_handler,
					   IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
					   "ops_bq24297_int_handler", NULL);
		if (ret)
			pr_debug(
				"[bq24297_driver_probe] fault interrupt registration failed err = %d\n",
				ret);
	}

	return 0;
}

static const struct i2c_device_id bq24297_i2c_id[] = {{"bq24297", 0}, {} };

#ifdef CONFIG_OF
static const struct of_device_id bq24297_id[] = {
	{.compatible = "ti,bq24297"}, {.compatible = "ti,bq24296"}, {},
};

MODULE_DEVICE_TABLE(of, bq24297_id);
#endif
#ifdef CONFIG_PM
static const struct dev_pm_ops bq24297_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(bq24297_driver_suspend, bq24297_driver_resume)
};
#endif
static struct i2c_driver bq24297_driver = {
	.driver = {

			.name = "bq24297",
#ifdef CONFIG_OF
			.of_match_table = of_match_ptr(bq24297_id),
#endif
#ifdef CONFIG_PM
			.pm   = &bq24297_pm_ops,
#endif
		},
	.probe = bq24297_driver_probe,
	.remove = bq24297_driver_remove,
	.shutdown = bq24297_driver_shutdown,
	.id_table = bq24297_i2c_id,
};

static ssize_t show_bq24297_access(struct device *dev,
				   struct device_attribute *attr, char *buf)
{
	pr_debug("[show_bq24297_access] 0x%x\n", g_reg_value_bq24297);
	return sprintf(buf, "0x%x\n", g_reg_value_bq24297);
}

static ssize_t store_bq24297_access(struct device *dev,
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
				"[store_bq24297_access] write bq24297 reg 0x%x with value 0x%x !\n",
				reg_address, reg_value);
			bq24297_config_interface(reg_address, reg_value, 0xFF,
						 0x0);
		} else {
			ret = kstrtouint(pvalue, 0, &reg_address);
			if (ret) {
				pr_debug("wrong format!\n");
				return size;
			}
			bq24297_read_interface(reg_address,
					       &g_reg_value_bq24297, 0xFF, 0x0);
			pr_debug(
				"[store_bq24297_access] read bq24297 reg 0x%x with value 0x%x !\n",
				reg_address, g_reg_value_bq24297);
			pr_debug(
				"[store_bq24297_access] Please use \"cat bq24297_access\" to get value\r\n");
		}
	}
	return size;
}

static DEVICE_ATTR(bq24297_access, 0644, show_bq24297_access,
		   store_bq24297_access);

static int bq24297_user_space_probe(struct platform_device *dev)
{
	int ret_device_file = 0;

	pr_debug("bq24297_user_space_probe!\n");
	ret_device_file =
		device_create_file(&(dev->dev), &dev_attr_bq24297_access);

	return 0;
}

struct platform_device bq24297_user_space_device = {
	.name = "bq24297-user", .id = -1,
};

static struct platform_driver bq24297_user_space_driver = {
	.probe = bq24297_user_space_probe,
	.driver = {

			.name = "bq24297-user",
		},
};

static int __init bq24297_init(void)
{
	int ret = 0;

	if (i2c_add_driver(&bq24297_driver) != 0)
		pr_debug(
			"[bq24297_init] failed to register bq24297 i2c driver.\n");
	else
		pr_debug(
			"[bq24297_init] Success to register bq24297 i2c driver.\n");

	/* bq24297 user space access interface */
	ret = platform_device_register(&bq24297_user_space_device);
	if (ret) {
		pr_debug("[bq24297_init] Unable to device register(%d)\n", ret);
		return ret;
	}
	ret = platform_driver_register(&bq24297_user_space_driver);
	if (ret) {
		pr_debug("[bq24297_init] Unable to register driver (%d)\n",
			 ret);
		return ret;
	}

	return 0;
}

static void __exit bq24297_exit(void)
{
	i2c_del_driver(&bq24297_driver);
}
module_init(bq24297_init);
module_exit(bq24297_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("I2C bq24297 Driver");
MODULE_AUTHOR("Tank Hung<tank.hung@mediatek.com>");
