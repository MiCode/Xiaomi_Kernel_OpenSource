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
 *   bq24261.h
 *
 * Project:
 * --------
 *   Android
 *
 * Description:
 * ------------
 *   bq24261 header file
 *
 * Author:
 * -------
 *
 ****************************************************************************/

#ifndef _bq24261_SW_H_
#define _bq24261_SW_H_

#define bq24261_CON0 0x00
#define bq24261_CON1 0x01
#define bq24261_CON2 0x02
#define bq24261_CON3 0x03
#define bq24261_CON4 0x04
#define bq24261_CON5 0x05
#define bq24261_CON6 0x06
#define bq24261_REG_NUM 7

/**********************************************************
 *
 *   [MASK/SHIFT]
 *
 *********************************************************/
/* CON0 */
#define CON0_TMR_RST_MASK 0x1
#define CON0_TMR_RST_SHIFT 7

#define CON0_EN_BOOST_MASK 0x1
#define CON0_EN_BOOST_SHIFT 6

#define CON0_STAT_MASK 0x3
#define CON0_STAT_SHIFT 4

#define CON0_EN_SHIPMODE_MASK 0x1
#define CON0_EN_SHIPMODE_SHIFT 3

#define CON0_FAULT_MASK 0x7
#define CON0_FAULT_SHIFT 0

/* CON1 */
#define CON1_RESET_MASK 0x1
#define CON1_RESET_SHIFT 7

#define CON1_IN_LIMIT_MASK 0x7
#define CON1_IN_LIMIT_SHIFT 4

#define CON1_EN_STAT_MASK 0x1
#define CON1_EN_STAT_SHIFT 3

#define CON1_TE_MASK 0x1
#define CON1_TE_SHIFT 2

#define CON1_DIS_CE_MASK 0x1
#define CON1_DIS_CE_SHIFT 1

#define CON1_HZ_MODE_MASK 0x1
#define CON1_HZ_MODE_SHIFT 0

/* CON2 */
#define CON2_VBREG_MASK 0x3F
#define CON2_VBREG_SHIFT 2

#define CON2_MOD_FREQ_MASK 0x3
#define CON2_MOD_FREQ_SHIFT 0

/* CON3 */
#define CON3_VENDER_CODE_MASK 0x7
#define CON3_VENDER_CODE_SHIFT 5

#define CON3_PN_MASK 0x3
#define CON3_PN_SHIFT 3

/* CON4 */
#define CON4_ICHRG_MASK 0x1F
#define CON4_ICHRG_SHIFT 3

#define CON4_ITERM_MASK 0x7
#define CON4_ITERM_SHIFT 0

/* CON5 */
#define CON5_MINSYS_STATUS_MASK 0x1
#define CON5_MINSYS_STATUS_SHIFT 7

#define CON5_VINDPM_STATUS_MASK 0x1
#define CON5_VINDPM_STATUS_SHIFT 6

#define CON5_LOW_CHG_MASK 0x1
#define CON5_LOW_CHG_SHIFT 5

#define CON5_DPDM_EN_MASK 0x1
#define CON5_DPDM_EN_SHIFT 4

#define CON5_CD_STATUS_MASK 0x1
#define CON5_CD_STATUS_SHIFT 3

#define CON5_VINDPM_MASK 0x7
#define CON5_VINDPM_SHIFT 0

/* CON6 */
#define CON6_2XTMR_EN_MASK 0x1
#define CON6_2XTMR_EN_SHIFT 7

#define CON6_TMR_MASK 0x3
#define CON6_TMR_SHIFT 5

#define CON6_BOOST_ILIM_MASK 0x1
#define CON6_BOOST_ILIM_SHIFT 4

#define CON6_TS_EN_MASK 0x1
#define CON6_TS_EN_SHIFT 3

#define CON6_TS_FAULT_MASK 0x3
#define CON6_TS_FAULT_SHIFT 1

#define CON6_VINDPM_OFF_MASK 0x1
#define CON6_VINDPM_OFF_SHIFT 0

/**********************************************************
 *
 *   [Extern Function]
 *
 *********************************************************/
/* CON0---------------------------------------------------- */
extern void bq24261_set_tmr_rst(u32 val);
extern void bq24261_set_en_boost(u32 val);
extern u32 bq24261_get_stat(void);
extern void bq24261_set_en_shipmode(u32 val);
extern u32 bq24261_get_fault(void);
/* CON1---------------------------------------------------- */
extern void bq24261_set_reset(u32 val);
extern u32 bq24261_get_in_limit(void);
extern void bq24261_set_in_limit(u32 val);
extern void bq24261_set_en_stat(u32 val);
extern void bq24261_set_te(u32 val);
extern void bq24261_set_dis_ce(u32 val);
extern void bq24261_set_hz_mode(u32 val);
/* CON2---------------------------------------------------- */
extern void bq24261_set_vbreg(u32 val);
extern void bq24261_set_mod_freq(u32 val);
/* CON3---------------------------------------------------- */
extern u32 bq24261_get_vender_code(void);
extern u32 bq24261_get_pn(void);
/* CON4---------------------------------------------------- */
extern u32 bq24261_get_ichg(void);
extern void bq24261_set_ichg(u32 val);
extern void bq24261_set_iterm(u32 val);
/* CON5---------------------------------------------------- */
extern u32 bq24261_get_minsys_status(void);
extern u32 bq24261_get_vindpm_status(void);
extern u32 bq24261_get_low_chg(void);
extern void bq24261_set_low_chg(u32 val);
extern void bq24261_set_dpdm_en(u32 val);
extern u32 bq24261_get_cd_status(void);
extern void bq24261_set_vindpm(u32 val);
/* CON6---------------------------------------------------- */
extern void bq24261_set_2xtmr_en(u32 val);
extern void bq24261_set_tmr(u32 val);
extern void bq24261_set_boost_ilim(u32 val);
extern void bq24261_set_ts_en(u32 val);
extern u32 bq24261_get_ts_fault(void);
extern void bq24261_set_vindpm_off(u32 val);

/* --------------------------------------------------------- */
extern void bq24261_dump_register(void);
extern u32 bq24261_reg_config_interface(u8 RegNum, u8 val);

extern u32 bq24261_read_interface(u8 RegNum, u8 *val, u8 MASK, u8 SHIFT);
extern u32 bq24261_config_interface(u8 RegNum, u8 val, u8 MASK, u8 SHIFT);

extern u32 upmu_get_reg_value(u32 reg);
extern bool mt_usb_is_device(void);
extern void Charger_Detect_Init(void);
extern void Charger_Detect_Release(void);
extern int hw_charger_type_detection(void);

/* spm utility */
extern int slp_get_wake_reason(void);

#endif /* _bq24261_SW_H_ */
