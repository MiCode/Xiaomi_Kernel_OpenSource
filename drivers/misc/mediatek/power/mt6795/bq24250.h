/*****************************************************************************
*
* Filename:
* ---------
*   bq24250.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   bq24250 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _bq24250_SW_H_
#define _bq24250_SW_H_

/*Begin,Lenovo-sw chailu1 modify 2014-4-23, support hight battery voltage */
#ifdef LENOVO_HIGH_BATTERY_VOLTAGE_SUPPORT
#define HIGH_BATTERY_VOLTAGE_SUPPORT
#endif
/*End,Lenovo-sw chailu1 modify 2014-4-23, support hight battery voltage*/

#define bq24250_CON0      0x00
#define bq24250_CON1      0x01
#define bq24250_CON2      0x02
#define bq24250_CON3      0x03
#define bq24250_CON4      0x04
#define bq24250_CON5      0x05
#define bq24250_CON6      0x06

#define bq24250_REG_NUM 7

/**********************************************************
  *
  *   [MASK/SHIFT] 
  *
  *********************************************************/
//CON0
#define CON0_WD_FAULT_MASK   0x01
#define CON0_WD_FAULT_SHIFT  7

#define CON0_WD_EN_MASK   0x01
#define CON0_WD_EN_SHIFT  6

#define CON0_STATE_MASK   0x03
#define CON0_STATE_SHIFT  4

#define CON0_FAULT_MASK       0x0f
#define CON0_FAULT_SHIFT      0

//CON1
#define CON1_RST_MASK     0x01
#define CON1_RST_SHIFT    7

#define CON1_LIN_LIMIT_MASK     0x07
#define CON1_LIN_LIMIT_SHIFT    4

#define CON1_EN_STAT_MASK        0x01
#define CON1_EN_STAT_SHIFT       3

#define CON1_EN_TERM_MASK        0x01
#define CON1_EN_TERM_SHIFT       2

#define CON1_EN_CHG_MASK   0x01
#define CON1_EN_CHG_SHIFT  1

#define CON1_HZ_MODE_MASK   0x01
#define CON1_HZ_MODE_SHIFT  0

//CON2
#define CON2_VBAT_REG_MASK    0x3F
#define CON2_VBAT_REG_SHIFT   2

#define CON2_USB_DET_MASK    0x03
#define CON2_USB_DET_SHIFT   0

//CON3
#define CON3_ICHG_MASK   0x1F
#define CON3_ICHG_SHIFT  3

#define CON3_ITERM_MASK           0x07
#define CON3_ITERM_SHIFT          0

//CON4
#define CON4_LOOP_STATUS_MASK     0x03
#define CON4_LOOP_STATUS_SHIFT    6

#define CON4_LOW_CHG_MASK     0x01
#define CON4_LOW_CHG_SHIFT    5

#define CON4_DPDM_EN_MASK    0x01
#define CON4_DPDM_EN_SHIFT   4

#define CON4_CE_STATUS_MASK    0x01
#define CON4_CE_STATUS_SHIFT   3

#define CON4_VIN_DPM_MASK    0x07
#define CON4_VIN_DPM_SHIFT   0

//CON5
#define CON5_2XTMR_EN_MASK      0x01
#define CON5_2XTMR_EN_SHIFT     7

#define CON5_TMR_MASK      0x03
#define CON5_TMR_SHIFT     5

#define CON5_SYSOFF_MASK     0x01
#define CON5_SYSOFF_SHIFT    4

#define CON5_TS_EN_MASK      0x01
#define CON5_TS_EN_SHIFT     3

#define CON5_TS_STAT_MASK           0x07
#define CON5_TS_STAT_SHIFT          0

//CON6
#define CON6_VOVP_MASK     0x07
#define CON6_VOVP_SHIFT    5

#define CON6_CLR_VDP_MASK     0x01
#define CON6_CLR_VDP_SHIFT    4

#define CON6_FORCE_BATDET_MASK     0x01
#define CON6_FORCE_BATDET_SHIFT    3

#define CON6_FORCE_PTM_MASK     0x01
#define CON6_FORCE_PTM_SHIFT    2

/**********************************************************
  *
  *   [Extern Function] 
  *
  *********************************************************/
//CON0----------------------------------------------------
extern int bq24250_get_wdt_fault(void);
extern void bq24250_set_en_wdt(kal_uint32 val);
extern void bq24250_set_wdt_rst();
extern int bq24250_get_state(void);
extern int bq24250_get_fault(void);
//CON1----------------------------------------------------
extern void bq24250_set_reg_rst(kal_uint32 val);
extern void bq24250_set_iinlim(kal_uint32 val);
extern void bq24250_set_en_stat(kal_uint32 val);
extern void bq24250_set_en_term(kal_uint32 val);
extern void bq24250_set_en_chg(kal_uint32 val);
extern void bq24250_set_en_hiz(kal_uint32 val);
//CON2----------------------------------------------------
extern void bq24250_set_vreg(kal_uint32 val);
extern int bq24250_get_usb_det(void);
//CON3----------------------------------------------------
extern void bq24250_set_ichg(kal_uint32 val);
extern unsigned int bq24250_get_ichg(void);
extern void bq24250_set_iterm(kal_uint32 val);
//CON4----------------------------------------------------
extern int bq24250_get_loop_state(void);
extern void bq24250_set_low_chg(kal_uint32 val);
extern void bq24250_set_dpdm(kal_uint32 val);
extern int bq24250_get_ce_state(void);
extern void bq24250_set_vin_dpm(kal_uint32 val);
//CON5----------------------------------------------------
extern void bq24250_set_en_2xtimer(kal_uint32 val);
extern void bq24250_set_tmr(kal_uint32 val);
extern void bq24250_set_en_sysoff(kal_uint32 val);
extern void bq24250_set_en_ts(kal_uint32 val);
extern int bq24250_get_ts_state(void);
//CON6----------------------------------------------------
extern void bq24250_set_en_vovp(kal_uint32 val);
extern void bq24250_set_cls_vdp(kal_uint32 val);
extern void bq24250_set_force_batdet(kal_uint32 val);
extern void bq24250_set_force_ptm(kal_uint32 val);

extern void bq24250_dump_register(void);

#endif // _bq24250_SW_H_

