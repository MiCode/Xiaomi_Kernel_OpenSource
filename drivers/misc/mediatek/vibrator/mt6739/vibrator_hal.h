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

/*********************************************
* HAL API
**********************************************/

void vibr_Enable_HW(void);
void vibr_Disable_HW(void);
void vibr_power_set(void);
struct vibrator_hw *mt_get_cust_vibrator_hw(void);
