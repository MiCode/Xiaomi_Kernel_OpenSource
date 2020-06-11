/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2019 MediaTek Inc.
 */

#ifndef __CCCI_MD_AUXADC__
#define __CCCI_MD_AUXADC__

#ifdef CONFIG_MEDIATEK_MT6577_AUXADC
int ccci_get_adc_num(void);
int ccci_get_adc_val(void);
#endif

signed int battery_get_bat_voltage(void);

#endif
