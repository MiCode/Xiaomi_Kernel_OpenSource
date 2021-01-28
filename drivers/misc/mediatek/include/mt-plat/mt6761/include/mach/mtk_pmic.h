/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2017 MediaTek Inc.
 */

#ifndef _CUST_PMIC_H_
#define _CUST_PMIC_H_

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

#define IMAX_MAX_VALUE 5500
#define DLPT_POWER_OFF_EN
#define POWEROFF_BAT_CURRENT 3000
#define DLPT_POWER_OFF_THD 100

#define DLPT_VOLT_MIN 3100

#endif /* _CUST_PMIC_H_ */
