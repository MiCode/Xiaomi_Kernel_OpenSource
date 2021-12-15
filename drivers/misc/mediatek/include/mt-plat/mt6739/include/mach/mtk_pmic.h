/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
*/

#ifndef _CUST_PMIC_H_
#define _CUST_PMIC_H_

/*#define PMIC_VDVFS_CUST_ENABLE*/

#define LOW_POWER_LIMIT_LEVEL_1 15

#define PT_DLPT_BRINGUP 0

#if defined(CONFIG_FPGA_EARLY_PORTING) || PT_DLPT_BRINGUP
/* Define for disable low battery protect feature,
 * default no define for enable low battery protect.
 */
#define DISABLE_LOW_BATTERY_PROTECT

/*Define for disable battery OC protect*/
#define DISABLE_BATTERY_OC_PROTECT

/*Define for disable battery 15% protect*/
#define DISABLE_BATTERY_PERCENT_PROTECT

/*Define for DLPT*/
#define DISABLE_DLPT_FEATURE
#endif /* defined(CONFIG_FPGA_EARLY_PORTING) || PT_DLPT_BRINGUP */

#define POWER_UVLO_VOLT_LEVEL 2600
#define IMAX_MAX_VALUE 5500

#define POWER_INT0_VOLT 3400
#define POWER_INT1_VOLT 3250
#define POWER_INT2_VOLT 3100

#define POWER_BAT_OC_CURRENT_H    4670
#define POWER_BAT_OC_CURRENT_L    5500
#define POWER_BAT_OC_CURRENT_H_RE 4670
#define POWER_BAT_OC_CURRENT_L_RE 5500

#define DLPT_POWER_OFF_EN
#define POWEROFF_BAT_CURRENT 3000
#define DLPT_POWER_OFF_THD 100

#define DLPT_VOLT_MIN 3100

#define BATTERY_MODULE_INIT

/* ADC Channel Number, not uesed and removed */


extern const char *pmic_auxadc_channel_name[];

/* Legacy pmic auxadc interface */
enum {
	/* mt6357 */
	AUXADC_LIST_BATADC,
	AUXADC_LIST_START = AUXADC_LIST_BATADC,
	AUXADC_LIST_VCDT,
	AUXADC_LIST_BATTEMP,
	AUXADC_LIST_VBIF,
	AUXADC_LIST_MT6357_CHIP_TEMP,
	AUXADC_LIST_DCXO,
	AUXADC_LIST_ACCDET,
	AUXADC_LIST_TSX,
	AUXADC_LIST_HPOFS_CAL,
	AUXADC_LIST_ISENSE,
	AUXADC_LIST_MT6357_BUCK1_TEMP,
	AUXADC_LIST_MT6357_BUCK2_TEMP,
	AUXADC_LIST_END = AUXADC_LIST_MT6357_BUCK2_TEMP,
	AUXADC_LIST_MAX,
};


extern void pmic_auxadc_init(void);
extern void pmic_auxadc_dump_regs(char *buf);
extern int pmic_get_auxadc_channel_max(void);
extern int mt_get_auxadc_value(u8 channel);
extern void pmic_auxadc_lock(void);
extern void pmic_auxadc_unlock(void);
#endif /* _CUST_PMIC_H_ */
