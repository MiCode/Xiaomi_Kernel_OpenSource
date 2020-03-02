/*
 * Copyright (C) 2018 MediaTek Inc.
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

#ifndef __TZCPU_INITCFG_H__
#define __TZCPU_INITCFG_H__

#define TZCPU_SET_INIT_CFG			(1)

#define TZCPU_INITCFG_INTERVAL			(40)
#define TZCPU_INITCFG_NUM_TRIPS			(4)

#define TZCPU_INITCFG_TRIP_0_TEMP		(117000)
#define TZCPU_INITCFG_TRIP_0_COOLER		"mtktscpu-sysrst"

#define TZCPU_INITCFG_TRIP_1_TEMP		(95000)
#define TZCPU_INITCFG_TRIP_1_COOLER		"cpu00"

#define TZCPU_INITCFG_TRIP_2_TEMP		(85000)
#define TZCPU_INITCFG_TRIP_2_COOLER		"cpu03"

#define TZCPU_INITCFG_TRIP_3_TEMP		(65000)
#define TZCPU_INITCFG_TRIP_3_COOLER		"cpu_adaptive_0"

#define TZCPU_INITCFG_TRIP_4_TEMP		(63000)
#define TZCPU_INITCFG_TRIP_4_COOLER		""

#define TZCPU_INITCFG_TRIP_5_TEMP		(60000)
#define TZCPU_INITCFG_TRIP_5_COOLER		""

#define TZCPU_INITCFG_TRIP_6_TEMP		(55000)
#define TZCPU_INITCFG_TRIP_6_COOLER		""

#define TZCPU_INITCFG_TRIP_7_TEMP		(50000)
#define TZCPU_INITCFG_TRIP_7_COOLER		""

#define TZCPU_INITCFG_TRIP_8_TEMP		(45000)
#define TZCPU_INITCFG_TRIP_8_COOLER		""

#define TZCPU_INITCFG_TRIP_9_TEMP		(40000)
#define TZCPU_INITCFG_TRIP_9_COOLER		""


#endif	/* __TZCPU_INITCFG_H__ */


