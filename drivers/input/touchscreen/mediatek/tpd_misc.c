/*
* Copyright (C) 2016 MediaTek Inc.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
* See http://www.gnu.org/licenses/gpl-2.0.html for more details.
*/

#include "tpd.h"

int tpd_calibrate_en = 0;
module_param(tpd_calibrate_en, int, 0664);

int tpd_show_version = 0;
module_param(tpd_show_version, int, 0664);

/* switch touch panel into single scan mode for decreasing interference */
void tpd_switch_single_mode(void)
{
#ifdef HAVE_SINGLE_MULTIPLE_SCAN_MODE
	_tpd_switch_single_mode();
#endif
}
EXPORT_SYMBOL(tpd_switch_single_mode);

/* switch touch panel into multiple scan mode for better performance */
void tpd_switch_multiple_mode(void)
{
#ifdef HAVE_SINGLE_MULTIPLE_SCAN_MODE
	_tpd_switch_multiple_mode();
#endif
}
EXPORT_SYMBOL(tpd_switch_multiple_mode);

/* switch touch panel into deep sleep mode */
void tpd_switch_sleep_mode(void)
{
#ifdef HAVE_SLEEP_NORMAL_MODE
	_tpd_switch_sleep_mode();
#endif
}
EXPORT_SYMBOL(tpd_switch_sleep_mode);

/* switch touch panel back to normal mode */
void tpd_switch_normal_mode(void)
{
#ifdef HAVE_SLEEP_NORMAL_MODE
	_tpd_switch_normal_mode();
#endif
}
EXPORT_SYMBOL(tpd_switch_normal_mode);
