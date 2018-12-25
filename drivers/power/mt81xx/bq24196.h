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

/*****************************************************************************
 *
 * Filename:
 * ---------
 *   bq24196.h
 *
 * Project:
 * --------
 *   Android
 *
 * Description:
 * ------------
 *   bq24196 header file
 *
 * Author:
 * -------
 *
 ****************************************************************************/

#ifndef _bq24196_SW_H_
#define _bq24196_SW_H_

#define bq24196_CON0 0x00
#define bq24196_CON1 0x01
#define bq24196_CON2 0x02
#define bq24196_CON3 0x03
#define bq24196_CON4 0x04
#define bq24196_CON5 0x05
#define bq24196_CON6 0x06
#define bq24196_CON7 0x07
#define bq24196_CON8 0x08
#define bq24196_CON9 0x09
#define bq24196_CON10 0x0A

/********************************************************
 *
 *   [MASK/SHIFT]
 *
 ********************************************************
 */
/* CON0 */
#define CON0_EN_HIZ_MASK 0x01
#define CON0_EN_HIZ_SHIFT 7

#define CON0_VINDPM_MASK 0x0F
#define CON0_VINDPM_SHIFT 3

#define CON0_IINLIM_MASK 0x07
#define CON0_IINLIM_SHIFT 0

/* CON1 */
#define CON1_REG_RST_MASK 0x01
#define CON1_REG_RST_SHIFT 7

#define CON1_WDT_RST_MASK 0x01
#define CON1_WDT_RST_SHIFT 6

#define CON1_OTG_CONFIG_MASK 0x01
#define CON1_OTG_CONFIG_SHIFT 5

#define CON1_CHG_CONFIG_MASK 0x01
#define CON1_CHG_CONFIG_SHIFT 4

#define CON1_SYS_MIN_MASK 0x07
#define CON1_SYS_MIN_SHIFT 1

#define CON1_BOOST_LIM_MASK 0x01
#define CON1_BOOST_LIM_SHIFT 0

/* CON2 */
#define CON2_ICHG_MASK 0x3F
#define CON2_ICHG_SHIFT 2

#define CON2_FORCE_20PCT_MASK 0x1
#define CON2_FORCE_20PCT_SHIFT 0

/* CON3 */
#define CON3_IPRECHG_MASK 0x0F
#define CON3_IPRECHG_SHIFT 4

#define CON3_ITERM_MASK 0x0F
#define CON3_ITERM_SHIFT 0

/* CON4 */
#define CON4_VREG_MASK 0x3F
#define CON4_VREG_SHIFT 2

#define CON4_BATLOWV_MASK 0x01
#define CON4_BATLOWV_SHIFT 1

#define CON4_VRECHG_MASK 0x01
#define CON4_VRECHG_SHIFT 0

/* CON5 */
#define CON5_EN_TERM_MASK 0x01
#define CON5_EN_TERM_SHIFT 7

#define CON5_TERM_STAT_MASK 0x01
#define CON5_TERM_STAT_SHIFT 6

#define CON5_WATCHDOG_MASK 0x03
#define CON5_WATCHDOG_SHIFT 4

#define CON5_EN_TIMER_MASK 0x01
#define CON5_EN_TIMER_SHIFT 3

#define CON5_CHG_TIMER_MASK 0x03
#define CON5_CHG_TIMER_SHIFT 1

/* CON6 */
#define CON6_TREG_MASK 0x03
#define CON6_TREG_SHIFT 0

/* CON7 */
#define CON7_DPDM_EN_MASK 0x01
#define CON7_DPDM_EN_SHIFT 7

#define CON7_TMR2X_EN_MASK 0x01
#define CON7_TMR2X_EN_SHIFT 6

#define CON7_BATFET_Disable_MASK 0x01
#define CON7_BATFET_Disable_SHIFT 5

#define CON7_INT_MASK_MASK 0x03
#define CON7_INT_MASK_SHIFT 0

/* CON8 */
#define CON8_VBUS_STAT_MASK 0x03
#define CON8_VBUS_STAT_SHIFT 6

#define CON8_CHRG_STAT_MASK 0x03
#define CON8_CHRG_STAT_SHIFT 4

#define CON8_DPM_STAT_MASK 0x01
#define CON8_DPM_STAT_SHIFT 3

#define CON8_PG_STAT_MASK 0x01
#define CON8_PG_STAT_SHIFT 2

#define CON8_THERM_STAT_MASK 0x01
#define CON8_THERM_STAT_SHIFT 1

#define CON8_VSYS_STAT_MASK 0x01
#define CON8_VSYS_STAT_SHIFT 0

/* CON9 */
#define CON9_WATCHDOG_FAULT_MASK 0x01
#define CON9_WATCHDOG_FAULT_SHIFT 7

#define CON9_OTG_FAULT_MASK 0x01
#define CON9_OTG_FAULT_SHIFT 6

#define CON9_CHRG_FAULT_MASK 0x03
#define CON9_CHRG_FAULT_SHIFT 4

#define CON9_CHRG_INPUT_FAULT_MASK 0x01
#define CON9_CHRG_THERMAL_SHUTDOWN_FAULT_MASK 0x02
#define CON9_CHRG_TIMER_EXPIRATION_FAULT_MASK 0x03

#define CON9_BAT_FAULT_MASK 0x01
#define CON9_BAT_FAULT_SHIFT 3

#define CON9_NTC_FAULT_MASK 0x03
#define CON9_NTC_FAULT_SHIFT 0
#define CON9_NTC_COLD_FAULT_MASK 0x02
#define CON9_NTC_HOT_FAULT_MASK 0x01

/* CON10 */
#define CON10_PN_MASK 0x07
#define CON10_PN_SHIFT 3

#define CON10_Rev_MASK 0x07
#define CON10_Rev_SHIFT 0

/* REG09 status */
enum BQ_FAULT {
	BQ_NORMAL_FAULT = 0,
	BQ_WATCHDOG_FAULT,
	BQ_OTG_FAULT,
	BQ_CHRG_NORMAL_FAULT,
	BQ_CHRG_INPUT_FAULT,
	BQ_CHRG_THERMAL_FAULT,
	BQ_CHRG_TIMER_EXPIRATION_FAULT,
	BQ_BAT_FAULT,
	BQ_NTC_COLD_FAULT,
	BQ_NTC_HOT_FAULT,
	BQ_FAULT_MAX
};

/**********************************************************
 *
 *   [Extern Function]
 *
 *********************************************************/
/* CON0---------------------------------------------------- */
extern void bq24196_set_en_hiz(u32 val);
extern void bq24196_set_vindpm(u32 val);
extern void bq24196_set_iinlim(u32 val);
extern u32 bq24196_get_iinlim(void);
/* CON1---------------------------------------------------- */
extern void bq24196_set_reg_rst(u32 val);
extern void bq24196_set_wdt_rst(u32 val);
extern void bq24196_set_otg_config(u32 val);
extern void bq24196_set_chg_config(u32 val);
extern void bq24196_set_sys_min(u32 val);
extern void bq24196_set_boost_lim(u32 val);
/* CON2---------------------------------------------------- */
extern void bq24196_set_ichg(u32 val);
extern void bq24196_set_force_20pct(u32 val);
/* CON3---------------------------------------------------- */
extern void bq24196_set_iprechg(u32 val);
extern void bq24196_set_iterm(u32 val);
/* CON4---------------------------------------------------- */
extern void bq24196_set_vreg(u32 val);
extern void bq24196_set_batlowv(u32 val);
extern void bq24196_set_vrechg(u32 val);
/* CON5---------------------------------------------------- */
extern void bq24196_set_en_term(u32 val);
extern void bq24196_set_term_stat(u32 val);
extern void bq24196_set_watchdog(u32 val);
extern void bq24196_set_en_timer(u32 val);
extern void bq24196_set_chg_timer(u32 val);
/* CON6---------------------------------------------------- */
extern void bq24196_set_treg(u32 val);
/* CON7---------------------------------------------------- */
extern u32 bq24196_get_dpdm_status(void);
extern void bq24196_set_dpdm_en(u32 val);
extern void bq24196_set_tmr2x_en(u32 val);
extern void bq24196_set_batfet_disable(u32 val);
extern void bq24196_set_int_mask(u32 val);
/* CON8---------------------------------------------------- */
extern u32 bq24196_get_system_status(void);
extern u32 bq24196_get_vbus_stat(void);
extern u32 bq24196_get_chrg_stat(void);
extern u32 bq24196_get_pg_stat(void);
extern u32 bq24196_get_vsys_stat(void);
/* --------------------------------------------------------- */
extern void bq24196_dump_register(void);
extern u32 bq24196_read_interface(u8 RegNum, u8 *val, u8 MASK, u8 SHIFT);
int bq24196_control_interface(int cmd, void *data);

extern u32 upmu_get_reg_value(u32 reg);
extern bool mt_usb_is_device(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int hw_charger_type_detection(void);

/* spm utility */
extern int slp_get_wake_reason(void);

#endif /* _bq24196_SW_H_ */
