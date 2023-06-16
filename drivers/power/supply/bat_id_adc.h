/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2020.
 */
#ifndef __BATTERY_ID_ADC__
#define __BATTERY_ID_ADC__
//static int bat_id_get_adc_info(struct device *dev);
int bat_id_get_adc_num(void);
signed int battery_get_bat_id_voltage(void);
#endif