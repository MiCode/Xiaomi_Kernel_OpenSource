/*****************************************************************************
*
* Filename:
* ---------
*   bq24160.h
*
* Project:
* --------
*   Android
*
* Description:
* ------------
*   bq24160 header file
*
* Author:
* -------
*
****************************************************************************/

#ifndef _bq24160_SW_H_
#define _bq24160_SW_H_

#define bq24160_CON0      0x00
#define bq24160_CON1      0x01
#define bq24160_CON2      0x02
#define bq24160_CON3      0x03
#define bq24160_CON4      0x04
#define bq24160_CON5      0x05
#define bq24160_CON6      0x06
#define bq24160_CON7      0x07
#define bq24160_REG_NUM 8 


/**********************************************************
  *
  *   [MASK/SHIFT] 
  *
  *********************************************************/
//CON0
#define CON0_TMR_RST_MASK   0x1
#define CON0_TMR_RST_SHIFT  7

#define CON0_STAT_MASK   0x7
#define CON0_STAT_SHIFT  4

#define CON0_SUPPLY_SEL_MASK   0x1
#define CON0_SUPPLY_SEL_SHIFT  3

#define CON0_FAULT_MASK   0x7
#define CON0_FAULT_SHIFT  0

//CON1
#define CON1_INSTAT_MASK   0x3
#define CON1_INSTAT_SHIFT  6

#define CON1_USBSTAT_MASK   0x3
#define CON1_USBSTAT_SHIFT  4

#define CON1_OTG_LOCK_MASK   0x1
#define CON1_OTG_LOCK_SHIFT  3

#define CON1_BATSTAT_MASK   0x3
#define CON1_BATSTAT_SHIFT  1

#define CON1_EN_NOBATOP_MASK   0x1
#define CON1_EN_NOBATOP_SHIFT  0

//CON2
#define CON2_RESET_MASK   0x1
#define CON2_RESET_SHIFT  7

#define CON2_IUSB_LIMIT_MASK   0x7
#define CON2_IUSB_LIMIT_SHIFT  4

#define CON2_EN_STAT_MASK   0x1
#define CON2_EN_STAT_SHIFT  3

#define CON2_TE_MASK   0x1
#define CON2_TE_SHIFT  2

#define CON2_CE_MASK   0x1
#define CON2_CE_SHIFT  1

#define CON2_HZ_MODE_MASK   0x1
#define CON2_HZ_MODE_SHIFT  0

//CON3
#define CON3_VBREG_MASK   0x3F
#define CON3_VBREG_SHIFT  2

#define CON3_I_IN_LIMIT_MASK   0x1
#define CON3_I_IN_LIMIT_SHIFT  1

#define CON3_DPDM_EN_MASK   0x1
#define CON3_DPDM_EN_SHIFT  0

//CON4
#define CON4_VENDER_CODE_MASK   0x7
#define CON4_VENDER_CODE_SHIFT  5

#define CON4_PN_MASK   0x3
#define CON4_PN_SHIFT  3

#define CON4_REVISION_MASK   0x7
#define CON4_REVISION_SHIFT  0

//CON5
#define CON5_ICHRG_MASK   0x1F
#define CON5_ICHRG_SHIFT  3

#define CON5_ITERM_MASK   0x7
#define CON5_ITERM_SHIFT  0

//CON6
#define CON6_MINSYS_STATUS_MASK   0x1
#define CON6_MINSYS_STATUS_SHIFT  7

#define CON6_DPM_STATUS_MASK   0x1
#define CON6_DPM_STATUS_SHIFT  6

#define CON6_VINDPM_USB_MASK   0x7
#define CON6_VINDPM_USB_SHIFT  3

#define CON6_VINDPM_IN_MASK   0x7
#define CON6_VINDPM_IN_SHIFT  0

//CON7
#define CON7_2XTMR_EN_MASK   0x1
#define CON7_2XTMR_EN_SHIFT  7

#define CON7_TMR_MASK   0x3
#define CON7_TMR_SHIFT  5

#define CON7_TS_EN_MASK   0x1
#define CON7_TS_EN_SHIFT  3

#define CON7_TS_FAULT_MASK   0x3
#define CON7_TS_FAULT_SHIFT  1

#define CON7_LOW_CHG_MASK   0x1
#define CON7_LOW_CHG_SHIFT  0

/**********************************************************
  *
  *   [Extern Function] 
  *
  *********************************************************/
//CON0----------------------------------------------------
extern void bq24160_set_tmr_rst(kal_uint32 val);
extern kal_uint32 bq24160_get_stat(void);
extern void bq24160_set_supply_sel(kal_uint32 val);
extern kal_uint32 bq24160_get_fault(void);
//CON1----------------------------------------------------
extern kal_uint32 bq24160_get_instat(void);
extern kal_uint32 bq24160_get_usbstat(void);
extern void bq24160_set_otg_lock(kal_uint32 val);
extern kal_uint32 bq24160_get_batstat(void);
extern void bq24160_set_en_nobatop(kal_uint32 val);
//CON2----------------------------------------------------
extern void bq24160_set_reset(kal_uint32 val);
extern void bq24160_set_iusb_limit(kal_uint32 val);
extern void bq24160_set_en_stat(kal_uint32 val);
extern void bq24160_set_te(kal_uint32 val);
extern void bq24160_set_ce(kal_uint32 val);
extern void bq24160_set_hz_mode(kal_uint32 val);
//CON3----------------------------------------------------
extern void bq24160_set_vbreg(kal_uint32 val);
extern void bq24160_set_i_in_limit(kal_uint32 val);
extern void bq24160_set_dpdm_en(kal_uint32 val);
//CON4----------------------------------------------------
extern kal_uint32 bq24160_get_vender_code(void);
extern kal_uint32 bq24160_get_pn(void);
extern kal_uint32 bq24160_get_revision(void);
//CON5----------------------------------------------------
extern void bq24160_set_ichrg(kal_uint32 val);
extern void bq24160_set_iterm(kal_uint32 val);
//CON6----------------------------------------------------
extern kal_uint32 bq24160_get_minsys_status(void);
extern kal_uint32 bq24160_get_dpm_status(void);
extern void bq24160_set_vindpm_usb(kal_uint32 val);
extern void bq24160_set_vindpm_in(kal_uint32 val);
//CON7----------------------------------------------------
extern void bq24160_set_2xtmr_en(kal_uint32 val);
extern void bq24160_set_tmr(kal_uint32 val);
extern void bq24160_set_ts_en(kal_uint32 val);
extern kal_uint32 bq24160_get_ts_fault(void);
extern void bq24160_set_low_chg(kal_uint32 val);

//---------------------------------------------------------
extern void bq24160_dump_register(void);
extern kal_uint32 bq24160_reg_config_interface (kal_uint8 RegNum, kal_uint8 val);

extern kal_uint32 bq24160_read_interface (kal_uint8 RegNum, kal_uint8 *val, kal_uint8 MASK, kal_uint8 SHIFT);
extern kal_uint32 bq24160_config_interface (kal_uint8 RegNum, kal_uint8 val, kal_uint8 MASK, kal_uint8 SHIFT);

#endif // _bq24160_SW_H_

