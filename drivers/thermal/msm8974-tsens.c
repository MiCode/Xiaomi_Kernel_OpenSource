/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/thermal.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/msm_tsens.h>
#include <linux/err.h>
#include <linux/of.h>

#define CREATE_TRACE_POINTS
#include <trace/trace_thermal.h>

#define TSENS_DRIVER_NAME		"msm-tsens"
/* TSENS register info */
#define TSENS_UPPER_LOWER_INTERRUPT_CTRL(n)		((n) + 0x1000)
#define TSENS_INTERRUPT_EN		BIT(0)

#define TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(n)	((n) + 0x1004)
#define TSENS_UPPER_STATUS_CLR		BIT(21)
#define TSENS_LOWER_STATUS_CLR		BIT(20)
#define TSENS_UPPER_THRESHOLD_MASK	0xffc00
#define TSENS_LOWER_THRESHOLD_MASK	0x3ff
#define TSENS_UPPER_THRESHOLD_SHIFT	10

#define TSENS_S0_STATUS_ADDR(n)		((n) + 0x1030)
#define TSENS_SN_ADDR_OFFSET		0x4
#define TSENS_SN_STATUS_TEMP_MASK	0x3ff
#define TSENS_SN_STATUS_LOWER_STATUS	BIT(11)
#define TSENS_SN_STATUS_UPPER_STATUS	BIT(12)
#define TSENS_STATUS_ADDR_OFFSET			2

#define TSENS_TRDY_ADDR(n)		((n) + 0x105c)
#define TSENS_TRDY_MASK			BIT(0)

#define TSENS2_SN_STATUS_ADDR(n)	((n) + 0x1044)
#define TSENS2_SN_STATUS_VALID		BIT(14)
#define TSENS2_SN_STATUS_VALID_MASK	0x4000
#define TSENS2_TRDY_ADDR(n)		((n) + 0x84)

#define TSENS_CTRL_ADDR(n)		(n)
#define TSENS_EN			BIT(0)
#define TSENS_SW_RST			BIT(1)
#define TSENS_ADC_CLK_SEL		BIT(2)
#define TSENS_SENSOR0_SHIFT		3
#define TSENS_62_5_MS_MEAS_PERIOD	1
#define TSENS_312_5_MS_MEAS_PERIOD	2
#define TSENS_MEAS_PERIOD_SHIFT		18

#define TSENS_GLOBAL_CONFIG(n)		((n) + 0x34)
#define TSENS_S0_MAIN_CONFIG(n)		((n) + 0x38)
#define TSENS_SN_REMOTE_CONFIG(n)	((n) + 0x3c)

#define TSENS_EEPROM(n)			((n) + 0xd0)
#define TSENS_EEPROM_REDUNDANCY_SEL(n)	((n) + 0x444)
#define TSENS_EEPROM_BACKUP_REGION(n)	((n) + 0x440)

#define TSENS_MAIN_CALIB_ADDR_RANGE	6
#define TSENS_BACKUP_CALIB_ADDR_RANGE	4

#define TSENS_EEPROM_8X26_1(n)		((n) + 0x1c0)
#define TSENS_EEPROM_8X26_2(n)		((n) + 0x444)
#define TSENS_8X26_MAIN_CALIB_ADDR_RANGE	4

#define TSENS_EEPROM_8X10_1(n)		((n) + 0x1a4)
#define TSENS_EEPROM_8X10_1_OFFSET	8
#define TSENS_EEPROM_8X10_2(n)		((n) + 0x1a8)
#define TSENS_EEPROM_8X10_SPARE_1(n)	((n) + 0xd8)
#define TSENS_EEPROM_8X10_SPARE_2(n)	((n) + 0xdc)

#define TSENS_9900_EEPROM(n)			((n) + 0xd0)
#define TSENS_9900_EEPROM_REDUNDANCY_SEL(n)	((n) + 0x1c4)
#define TSENS_9900_EEPROM_BACKUP_REGION(n)	((n) + 0x450)
#define TSENS_9900_CALIB_ADDR_RANGE	4

#define TSENS_8939_EEPROM(n)			((n) + 0xa0)

#define TSENS_8994_EEPROM(n)			((n) + 0xd0)
#define TSENS_8994_EEPROM_REDUN_SEL(n)		((n) + 0x464)
#define TSENS_REDUN_REGION1_EEPROM(n)		((n) + 0x1c0)
#define TSENS_REDUN_REGION2_EEPROM(n)		((n) + 0x1c4)
#define TSENS_REDUN_REGION3_EEPROM(n)		((n) + 0x1cc)
#define TSENS_REDUN_REGION4_EEPROM(n)		((n) + 0x440)
#define TSENS_REDUN_REGION5_EEPROM(n)		((n) + 0x444)

/* TSENS calibration Mask data */
#define TSENS_BASE1_MASK		0xff
#define TSENS0_POINT1_MASK		0x3f00
#define TSENS1_POINT1_MASK		0xfc000
#define TSENS2_POINT1_MASK		0x3f00000
#define TSENS3_POINT1_MASK		0xfc000000
#define TSENS4_POINT1_MASK		0x3f
#define TSENS5_POINT1_MASK		0xfc0
#define TSENS6_POINT1_MASK		0x3f000
#define TSENS7_POINT1_MASK		0xfc0000
#define TSENS8_POINT1_MASK		0x3f000000
#define TSENS8_POINT1_MASK_BACKUP	0x3f
#define TSENS9_POINT1_MASK		0x3f
#define TSENS9_POINT1_MASK_BACKUP	0xfc0
#define TSENS10_POINT1_MASK		0xfc0
#define TSENS10_POINT1_MASK_BACKUP	0x3f000
#define TSENS_CAL_SEL_0_1		0xc0000000
#define TSENS_CAL_SEL_2			0x40000000
#define TSENS_CAL_SEL_SHIFT		30
#define TSENS_CAL_SEL_SHIFT_2		28
#define TSENS_ONE_POINT_CALIB		0x1
#define TSENS_ONE_POINT_CALIB_OPTION_2	0x2
#define TSENS_TWO_POINT_CALIB		0x3

#define TSENS0_POINT1_SHIFT		8
#define TSENS1_POINT1_SHIFT		14
#define TSENS2_POINT1_SHIFT		20
#define TSENS3_POINT1_SHIFT		26
#define TSENS5_POINT1_SHIFT		6
#define TSENS6_POINT1_SHIFT		12
#define TSENS7_POINT1_SHIFT		18
#define TSENS8_POINT1_SHIFT		24
#define TSENS9_POINT1_BACKUP_SHIFT	6
#define TSENS10_POINT1_SHIFT		6
#define TSENS10_POINT1_BACKUP_SHIFT	12

#define TSENS_POINT2_BASE_SHIFT		12
#define TSENS_POINT2_BASE_BACKUP_SHIFT	18
#define TSENS0_POINT2_SHIFT		20
#define TSENS0_POINT2_BACKUP_SHIFT	26
#define TSENS1_POINT2_SHIFT		26
#define TSENS2_POINT2_BACKUP_SHIFT	6
#define TSENS3_POINT2_SHIFT		6
#define TSENS3_POINT2_BACKUP_SHIFT	12
#define TSENS4_POINT2_SHIFT		12
#define TSENS4_POINT2_BACKUP_SHIFT	18
#define TSENS5_POINT2_SHIFT		18
#define TSENS5_POINT2_BACKUP_SHIFT	24
#define TSENS6_POINT2_SHIFT		24
#define TSENS7_POINT2_BACKUP_SHIFT	6
#define TSENS8_POINT2_SHIFT		6
#define TSENS8_POINT2_BACKUP_SHIFT	12
#define TSENS9_POINT2_SHIFT		12
#define TSENS9_POINT2_BACKUP_SHIFT	18
#define TSENS10_POINT2_SHIFT		18
#define TSENS10_POINT2_BACKUP_SHIFT	24

#define TSENS_BASE2_MASK		0xff000
#define TSENS_BASE2_BACKUP_MASK		0xfc0000
#define TSENS0_POINT2_MASK		0x3f00000
#define TSENS0_POINT2_BACKUP_MASK	0xfc000000
#define TSENS1_POINT2_MASK		0xfc000000
#define TSENS1_POINT2_BACKUP_MASK	0x3f
#define TSENS2_POINT2_MASK		0x3f
#define TSENS2_POINT2_BACKUP_MASK	0xfc0
#define TSENS3_POINT2_MASK		0xfc0
#define TSENS3_POINT2_BACKUP_MASK	0x3f000
#define TSENS4_POINT2_MASK		0x3f000
#define TSENS4_POINT2_BACKUP_MASK	0xfc0000
#define TSENS5_POINT2_MASK		0xfc0000
#define TSENS5_POINT2_BACKUP_MASK	0x3f000000
#define TSENS6_POINT2_MASK		0x3f000000
#define TSENS6_POINT2_BACKUP_MASK	0x3f
#define TSENS7_POINT2_MASK		0x3f
#define TSENS7_POINT2_BACKUP_MASK	0xfc0
#define TSENS8_POINT2_MASK		0xfc0
#define TSENS8_POINT2_BACKUP_MASK	0x3f000
#define TSENS9_POINT2_MASK		0x3f000
#define TSENS9_POINT2_BACKUP_MASK	0xfc0000
#define TSENS10_POINT2_MASK		0xfc0000
#define TSENS10_POINT2_BACKUP_MASK	0x3f000000

#define TSENS_8X26_BASE0_MASK		0x1fe000
#define TSENS0_8X26_POINT1_MASK		0x7e00000
#define TSENS1_8X26_POINT1_MASK		0x3f
#define TSENS2_8X26_POINT1_MASK		0xfc0
#define TSENS3_8X26_POINT1_MASK		0x3f000
#define TSENS4_8X26_POINT1_MASK		0xfc0000
#define TSENS5_8X26_POINT1_MASK		0x3f000000
#define TSENS6_8X26_POINT1_MASK		0x3f00000
#define TSENS_8X26_TSENS_CAL_SEL	0xe0000000
#define TSENS_8X26_BASE1_MASK		0xff
#define TSENS0_8X26_POINT2_MASK		0x3f00
#define TSENS1_8X26_POINT2_MASK		0xfc000
#define TSENS2_8X26_POINT2_MASK		0x3f00000
#define TSENS3_8X26_POINT2_MASK		0xfc000000
#define TSENS4_8X26_POINT2_MASK		0x3f00000
#define TSENS5_8X26_POINT2_MASK		0xfc000000
#define TSENS6_8X26_POINT2_MASK		0x7e0000

#define TSENS_8X26_CAL_SEL_SHIFT	29
#define TSENS_8X26_BASE0_SHIFT		13
#define TSENS0_8X26_POINT1_SHIFT	21
#define TSENS2_8X26_POINT1_SHIFT	6
#define TSENS3_8X26_POINT1_SHIFT	12
#define TSENS4_8X26_POINT1_SHIFT	18
#define TSENS5_8X26_POINT1_SHIFT	24
#define TSENS6_8X26_POINT1_SHIFT	20

#define TSENS0_8X26_POINT2_SHIFT	8
#define TSENS1_8X26_POINT2_SHIFT	14
#define TSENS2_8X26_POINT2_SHIFT	20
#define TSENS3_8X26_POINT2_SHIFT	26
#define TSENS4_8X26_POINT2_SHIFT	20
#define TSENS5_8X26_POINT2_SHIFT	26
#define TSENS6_8X26_POINT2_SHIFT	17

#define TSENS_8X10_CAL_SEL_SHIFT	28
#define TSENS_8X10_BASE1_SHIFT		8
#define TSENS0_8X10_POINT1_SHIFT	16
#define TSENS0_8X10_POINT2_SHIFT	22
#define TSENS1_8X10_POINT2_SHIFT	6
#define TSENS_8X10_BASE0_MASK		0xff
#define TSENS_8X10_BASE1_MASK		0xff00
#define TSENS0_8X10_POINT1_MASK		0x3f0000
#define TSENS0_8X10_POINT2_MASK		0xfc00000
#define TSENS_8X10_TSENS_CAL_SEL	0x70000000
#define TSENS1_8X10_POINT1_MASK		0x3f
#define TSENS1_8X10_POINT2_MASK		0xfc0
#define TSENS_8X10_REDUN_SEL_MASK	0x6000000
#define TSENS_8X10_REDUN_SEL_SHIFT	25

#define TSENS0_9900_POINT1_SHIFT	19
#define TSENS2_9900_POINT1_SHIFT	12
#define TSENS3_9900_POINT1_SHIFT	24
#define TSENS4_9900_POINT1_SHIFT	6
#define TSENS5_9900_POINT1_SHIFT	18
#define TSENS_9900_BASE1_MASK		0xff
#define TSENS0_9900_POINT1_MASK		0x1f80000
#define TSENS1_9900_POINT1_MASK		0x3f
#define TSENS2_9900_POINT1_MASK		0x3f000
#define TSENS3_9900_POINT1_MASK		0x3f000000
#define TSENS4_9900_POINT1_MASK		0xfc0
#define TSENS5_9900_POINT1_MASK		0xfc0000
#define TSENS6_9900_POINT1_MASK		0x3f

#define TSENS_9900_BASE2_SHIFT		8
#define TSENS0_9900_POINT2_SHIFT	25
#define TSENS1_9900_POINT2_SHIFT	6
#define TSENS2_9900_POINT2_SHIFT	18
#define TSENS4_9900_POINT2_SHIFT	12
#define TSENS5_9900_POINT2_SHIFT	24
#define TSENS6_9900_POINT2_SHIFT	6
#define TSENS_9900_BASE2_MASK		0xff00
#define TSENS0_9900_POINT2_MASK		0x7e000000
#define TSENS1_9900_POINT2_MASK		0xfc0
#define TSENS2_9900_POINT2_MASK		0xfc0000
#define TSENS3_9900_POINT2_MASK		0x3f
#define TSENS4_9900_POINT2_MASK		0x3f000
#define TSENS5_9900_POINT2_MASK		0x3f000000
#define TSENS6_9900_POINT2_MASK		0xfc0

#define TSENS_9900_CAL_SEL_SHIFT	16
#define TSENS_9900_TSENS_CAL_SEL	0x00070000

#define TSENS_BIT_APPEND		0x3
#define TSENS_CAL_DEGC_POINT1		30
#define TSENS_CAL_DEGC_POINT2		120
#define TSENS_SLOPE_FACTOR		1000

/* TSENS register data */
#define TSENS_TRDY_RDY_MIN_TIME		2000
#define TSENS_TRDY_RDY_MAX_TIME		2100
#define TSENS_THRESHOLD_MAX_CODE	0x3ff
#define TSENS_THRESHOLD_MIN_CODE	0x0

#define TSENS_GLOBAL_INIT_DATA		0x302f16c
#define TSENS_S0_MAIN_CFG_INIT_DATA	0x1c3
#define TSENS_SN_REMOTE_CFG_DATA	0x11c3

#define TSENS_QFPROM_BACKUP_SEL		0x3
#define TSENS_QFPROM_BACKUP_REDUN_SEL	0xe0000000
#define TSENS_QFPROM_BACKUP_REDUN_SHIFT	29

#define TSENS_QFPROM_BACKUP_9900_REDUN_SEL	0x07000000
#define TSENS_QFPROM_BACKUP_9900_REDUN_SHIFT	24

#define TSENS_TORINO_BASE0		0x3ff
#define TSENS_TORINO_BASE1		0xffc00
#define TSENS_TORINO_POINT0		0xf00000
#define TSENS_TORINO_POINT1		0xf0000000
#define TSENS_TORINO_POINT2		0xf0
#define TSENS_TORINO_POINT3		0xf000
#define TSENS_TORINO_POINT4		0xf00000
#define TSENS_TORINO_CALIB_PT		0x70000000

#define TSENS_TORINO_BASE1_SHIFT	10
#define TSENS_TORINO_POINT0_SHIFT	20
#define TSENS_TORINO_POINT1_SHIFT	28
#define TSENS_TORINO_POINT2_SHIFT	4
#define TSENS_TORINO_POINT3_SHIFT	12
#define TSENS_TORINO_POINT4_SHIFT	20
#define TSENS_TORINO_CALIB_SHIFT	28

#define TSENS_TYPE0		0
#define TSENS_TYPE2		2

#define TSENS_8916_BASE0_MASK		0x0000007f
#define TSENS_8916_BASE1_MASK		0xfe000000

#define TSENS0_8916_POINT1_MASK		0x00000f80
#define TSENS1_8916_POINT1_MASK		0x003e0000
#define TSENS2_8916_POINT1_MASK		0xf8000000
#define TSENS3_8916_POINT1_MASK		0x000003e0
#define TSENS4_8916_POINT1_MASK		0x000f8000

#define TSENS0_8916_POINT2_MASK		0x0001f000
#define TSENS1_8916_POINT2_MASK		0x07c00000
#define TSENS2_8916_POINT2_MASK		0x0000001f
#define TSENS3_8916_POINT2_MASK		0x00007c00
#define TSENS4_8916_POINT2_MASK		0x01f00000

#define TSENS_8916_TSENS_CAL_SEL	0xe0000000

#define TSENS_8916_CAL_SEL_SHIFT	29
#define TSENS_8916_BASE1_SHIFT		25

#define TSENS0_8916_POINT1_SHIFT	7
#define TSENS1_8916_POINT1_SHIFT	17
#define TSENS2_8916_POINT1_SHIFT	27
#define TSENS3_8916_POINT1_SHIFT	5
#define TSENS4_8916_POINT1_SHIFT	15

#define TSENS0_8916_POINT2_SHIFT	12
#define TSENS1_8916_POINT2_SHIFT	22
#define TSENS3_8916_POINT2_SHIFT	10
#define TSENS4_8916_POINT2_SHIFT	20
#define TSENS_VALID_CNT_2	2

#define TSENS_8939_BASE0_MASK		0x000000ff
#define TSENS_8939_BASE1_MASK		0xff000000

#define TSENS0_8939_POINT1_MASK		0x000001f8
#define TSENS1_8939_POINT1_MASK		0x001f8000
#define TSENS2_8939_POINT1_MASK_0_4	0xf8000000
#define TSENS2_8939_POINT1_MASK_5	0x00000001
#define TSENS3_8939_POINT1_MASK		0x00001f80
#define TSENS4_8939_POINT1_MASK		0x01f80000
#define TSENS5_8939_POINT1_MASK		0x00003f00
#define TSENS6_8939_POINT1_MASK		0x03f00000
#define TSENS7_8939_POINT1_MASK		0x0000003f
#define TSENS8_8939_POINT1_MASK		0x0003f000
#define TSENS9_8939_POINT1_MASK		0x07e00000

#define TSENS0_8939_POINT2_MASK		0x00007e00
#define TSENS1_8939_POINT2_MASK		0x07e00000
#define TSENS2_8939_POINT2_MASK		0x0000007e
#define TSENS3_8939_POINT2_MASK		0x0007e000
#define TSENS4_8939_POINT2_MASK		0x7e000000
#define TSENS5_8939_POINT2_MASK		0x000fc000
#define TSENS6_8939_POINT2_MASK		0xfc000000
#define TSENS7_8939_POINT2_MASK		0x00000fc0
#define TSENS8_8939_POINT2_MASK		0x00fc0000
#define TSENS9_8939_POINT2_MASK_0_4	0xf8000000
#define TSENS9_8939_POINT2_MASK_5	0x00002000

#define TSENS_8939_TSENS_CAL_SEL	0x7
#define TSENS_8939_CAL_SEL_SHIFT	0
#define TSENS_8939_BASE1_SHIFT		24

#define TSENS0_8939_POINT1_SHIFT	3
#define TSENS1_8939_POINT1_SHIFT	15
#define TSENS2_8939_POINT1_SHIFT_0_4	27
#define TSENS2_8939_POINT1_SHIFT_5	5
#define TSENS3_8939_POINT1_SHIFT	7
#define TSENS4_8939_POINT1_SHIFT	19
#define TSENS5_8939_POINT1_SHIFT	8
#define TSENS6_8939_POINT1_SHIFT	20
#define TSENS8_8939_POINT1_SHIFT	12
#define TSENS9_8939_POINT1_SHIFT	21

#define TSENS0_8939_POINT2_SHIFT	9
#define TSENS1_8939_POINT2_SHIFT	21
#define TSENS2_8939_POINT2_SHIFT	1
#define TSENS3_8939_POINT2_SHIFT	13
#define TSENS4_8939_POINT2_SHIFT	25
#define TSENS5_8939_POINT2_SHIFT	14
#define TSENS6_8939_POINT2_SHIFT	26
#define TSENS7_8939_POINT2_SHIFT	6
#define TSENS8_8939_POINT2_SHIFT	18
#define TSENS9_8939_POINT2_SHIFT_0_4	27
#define TSENS9_8939_POINT2_SHIFT_5	8
#define TSENS_NUM_SENSORS_V3		10

#define TSENS_BASE0_8994_MASK		0x3ff
#define TSENS_BASE1_8994_MASK		0xffc00
#define TSENS_BASE1_8994_SHIFT		10
#define TSENS0_OFFSET_8994_MASK		0xf00000
#define TSENS0_OFFSET_8994_SHIFT	20
#define TSENS1_OFFSET_8994_MASK		0xf000000
#define TSENS1_OFFSET_8994_SHIFT	24
#define TSENS2_OFFSET_8994_MASK		0xf0000000
#define TSENS2_OFFSET_8994_SHIFT	28
#define TSENS3_OFFSET_8994_MASK		0xf
#define TSENS4_OFFSET_8994_MASK		0xf0
#define TSENS4_OFFSET_8994_SHIFT	4
#define TSENS5_OFFSET_8994_MASK		0xf00
#define TSENS5_OFFSET_8994_SHIFT	8
#define TSENS6_OFFSET_8994_MASK		0xf000
#define TSENS6_OFFSET_8994_SHIFT	12
#define TSENS7_OFFSET_8994_MASK		0xf0000
#define TSENS7_OFFSET_8994_SHIFT	16
#define TSENS8_OFFSET_8994_MASK		0xf00000
#define TSENS8_OFFSET_8994_SHIFT	20
#define TSENS9_OFFSET_8994_MASK		0xf000000
#define TSENS9_OFFSET_8994_SHIFT	24
#define TSENS10_OFFSET_8994_MASK	0xf0000000
#define TSENS10_OFFSET_8994_SHIFT	28
#define TSENS11_OFFSET_8994_MASK	0xf
#define TSENS12_OFFSET_8994_MASK	0xf0
#define TSENS12_OFFSET_8994_SHIFT	4
#define TSENS13_OFFSET_8994_MASK	0xf00
#define TSENS13_OFFSET_8994_SHIFT	8
#define TSENS14_OFFSET_8994_MASK	0xf000
#define TSENS14_OFFSET_8994_SHIFT	12
#define TSENS15_OFFSET_8994_MASK	0xf0000
#define TSENS15_OFFSET_8994_SHIFT	16
#define TSENS_8994_CAL_SEL_MASK		0x700000
#define TSENS_8994_CAL_SEL_SHIFT	20

#define TSENS_BASE0_8994_REDUN_MASK		0x7fe00000
#define TSENS_BASE0_8994_REDUN_MASK_SHIFT	21
#define TSENS_BASE1_BIT0_8994_REDUN_MASK	0x80000000
#define TSENS_BASE1_BIT0_SHIFT_COMPUTE		31
#define TSENS_BASE1_BIT1_9_8994_REDUN_MASK	0x1ff
#define TSENS0_OFFSET_8994_REDUN_MASK		0x1e00
#define TSENS0_OFFSET_8994_REDUN_SHIFT		9
#define TSENS1_OFFSET_8994_REDUN_MASK		0x1e000
#define TSENS1_OFFSET_8994_REDUN_SHIFT		13
#define TSENS2_OFFSET_8994_REDUN_MASK		0x1e0000
#define TSENS2_OFFSET_8994_REDUN_SHIFT		17
#define TSENS3_OFFSET_8994_REDUN_MASK		0x1e00000
#define TSENS3_OFFSET_8994_REDUN_SHIFT		21
#define TSENS4_OFFSET_8994_REDUN_MASK		0x1e000000
#define TSENS4_OFFSET_8994_REDUN_SHIFT		25
#define TSENS5_OFFSET_8994_REDUN_MASK_BIT0_2	0xe0000000
#define TSENS5_OFFSET_8994_REDUN_SHIFT_BIT0_2	29
#define TSENS5_OFFSET_8994_REDUN_MASK_BIT3	0x800000
#define TSENS5_OFFSET_8994_REDUN_SHIFT_BIT3	23
#define TSENS6_OFFSET_8994_REDUN_MASK		0xf000000
#define TSENS6_OFFSET_8994_REDUN_SHIFT		24
#define TSENS7_OFFSET_8994_REDUN_MASK		0xf0000000
#define TSENS7_OFFSET_8994_REDUN_SHIFT		28
#define TSENS8_OFFSET_8994_REDUN_MASK		0xf
#define TSENS9_OFFSET_8994_REDUN_MASK		0xf0
#define TSENS9_OFFSET_8994_REDUN_SHIFT		4
#define TSENS10_OFFSET_8994_REDUN_MASK		0xf00
#define TSENS10_OFFSET_8994_REDUN_SHIFT		8
#define TSENS11_OFFSET_8994_REDUN_MASK		0xf000
#define TSENS11_OFFSET_8994_REDUN_SHIFT		12
#define TSENS12_OFFSET_8994_REDUN_MASK		0xf0000
#define TSENS12_OFFSET_8994_REDUN_SHIFT		16
#define TSENS13_OFFSET_8994_REDUN_MASK		0xf00000
#define TSENS13_OFFSET_8994_REDUN_SHIFT		20
#define TSENS14_OFFSET_8994_REDUN_MASK		0xf000000
#define TSENS14_OFFSET_8994_REDUN_SHIFT		24
#define TSENS15_OFFSET_8994_REDUN_MASK		0xf0000000
#define TSENS15_OFFSET_8994_REDUN_SHIFT		28
#define TSENS_8994_REDUN_SEL_MASK		0x7
#define TSENS_8994_CAL_SEL_REDUN_MASK		0xe0000000
#define TSENS_8994_CAL_SEL_REDUN_SHIFT		29

#define TSENS_MSM8909_BASE0_MASK		0x000000ff
#define TSENS_MSM8909_BASE1_MASK		0x0000ff00

#define TSENS0_MSM8909_POINT1_MASK		0x0000003f
#define TSENS1_MSM8909_POINT1_MASK		0x0003f000
#define TSENS2_MSM8909_POINT1_MASK		0x3f000000
#define TSENS3_MSM8909_POINT1_MASK		0x000003f0
#define TSENS4_MSM8909_POINT1_MASK		0x003f0000

#define TSENS0_MSM8909_POINT2_MASK		0x00000fc0
#define TSENS1_MSM8909_POINT2_MASK		0x00fc0000
#define TSENS2_MSM8909_POINT2_MASK_0_1	0xc0000000
#define TSENS2_MSM8909_POINT2_MASK_2_5	0x0000000f
#define TSENS3_MSM8909_POINT2_MASK		0x0000fc00
#define TSENS4_MSM8909_POINT2_MASK		0x0fc00000

#define TSENS_MSM8909_TSENS_CAL_SEL	0x00070000
#define TSENS_MSM8909_CAL_SEL_SHIFT	16
#define TSENS_MSM8909_BASE1_SHIFT	8

#define TSENS1_MSM8909_POINT1_SHIFT	12
#define TSENS2_MSM8909_POINT1_SHIFT	24
#define TSENS3_MSM8909_POINT1_SHIFT	4
#define TSENS4_MSM8909_POINT1_SHIFT	16

#define TSENS0_MSM8909_POINT2_SHIFT	6
#define TSENS1_MSM8909_POINT2_SHIFT	18
#define TSENS2_MSM8909_POINT2_SHIFT_0_1	30
#define TSENS2_MSM8909_POINT2_SHIFT_2_5	2
#define TSENS3_MSM8909_POINT2_SHIFT	10
#define TSENS4_MSM8909_POINT2_SHIFT	22

#define TSENS_MSM8909_D30_WA_S1   10
#define TSENS_MSM8909_D30_WA_S3   9
#define TSENS_MSM8909_D30_WA_S4   8
#define TSENS_MSM8909_D120_WA_S1  6
#define TSENS_MSM8909_D120_WA_S3  9
#define TSENS_MSM8909_D120_WA_S4  10

#define TSENS_ZIRC_CAL_SEL		0x700
#define TSENS_ZIRC_CAL_SEL_SHIFT	8
#define TSENS_BASE0_ZIRC_MASK		0x3ff
#define TSENS_BASE1_ZIRC_MASK		0xffc00
#define TSENS_BASE1_ZIRC_SHIFT		10
#define TSENS0_OFFSET_ZIRC_MASK		0xf00000
#define TSENS0_OFFSET_ZIRC_SHIFT	20
#define TSENS1_OFFSET_ZIRC_MASK		0xf000000
#define TSENS1_OFFSET_ZIRC_SHIFT	24
#define TSENS2_OFFSET_ZIRC_MASK		0xf0000000
#define TSENS2_OFFSET_ZIRC_SHIFT	28
#define TSENS3_OFFSET_ZIRC_MASK		0xf
#define TSENS4_OFFSET_ZIRC_MASK		0xf0
#define TSENS4_OFFSET_ZIRC_SHIFT	4

enum tsens_calib_fuse_map_type {
	TSENS_CALIB_FUSE_MAP_8974 = 0,
	TSENS_CALIB_FUSE_MAP_8X26,
	TSENS_CALIB_FUSE_MAP_8X10,
	TSENS_CALIB_FUSE_MAP_9900,
	TSENS_CALIB_FUSE_MAP_9630,
	TSENS_CALIB_FUSE_MAP_8916,
	TSENS_CALIB_FUSE_MAP_8939,
	TSENS_CALIB_FUSE_MAP_8994,
	TSENS_CALIB_FUSE_MAP_MSM8909,
	TSENS_CALIB_FUSE_MAP_MSMZIRC,
	TSENS_CALIB_FUSE_MAP_NUM,
};

/* Trips: warm and cool */
enum tsens_trip_type {
	TSENS_TRIP_WARM = 0,
	TSENS_TRIP_COOL,
	TSENS_TRIP_NUM,
};

#define TSENS_WRITABLE_TRIPS_MASK ((1 << TSENS_TRIP_NUM) - 1)

struct tsens_tm_device_sensor {
	struct thermal_zone_device	*tz_dev;
	enum thermal_device_mode	mode;
	/* Physical HW sensor number */
	unsigned int			sensor_hw_num;
	/* Software index. This is keep track of the HW/SW
	 * sensor_ID mapping */
	unsigned int			sensor_sw_id;
	struct work_struct		work;
	int				offset;
	int				calib_data_point1;
	int				calib_data_point2;
	uint32_t			slope_mul_tsens_factor;
};

struct tsens_tm_device {
	struct platform_device		*pdev;
	struct workqueue_struct		*tsens_wq;
	bool				is_ready;
	bool				prev_reading_avail;
	bool				calibration_less_mode;
	bool				tsens_local_init;
	int				tsens_factor;
	uint32_t			tsens_num_sensor;
	int				tsens_irq;
	void				*tsens_addr;
	void				*tsens_calib_addr;
	int				tsens_len;
	int				calib_len;
	struct resource			*res_tsens_mem;
	struct resource			*res_calib_mem;
	struct work_struct		tsens_work;
	uint32_t			calib_mode;
	uint32_t			tsens_type;
	bool				tsens_valid_status_check;
	struct tsens_tm_device_sensor	sensor[0];
};

struct tsens_tm_device *tmdev;


int tsens_is_ready()
{
	if (!tmdev)
		return -EPROBE_DEFER;
	else
		return tmdev->is_ready;
}
EXPORT_SYMBOL(tsens_is_ready);

int tsens_get_sw_id_mapping(int sensor_hw_num, int *sensor_sw_idx)
{
	int i = 0;
	bool id_found = false;

	if (tsens_is_ready() <= 0) {
		pr_debug("TSENS early init not done\n");
		return -EPROBE_DEFER;
	}

	while (i < tmdev->tsens_num_sensor && !id_found) {
		if (sensor_hw_num == tmdev->sensor[i].sensor_hw_num) {
			*sensor_sw_idx = tmdev->sensor[i].sensor_sw_id;
			id_found = true;
		}
		i++;
	}

	if (!id_found)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(tsens_get_sw_id_mapping);

int tsens_get_hw_id_mapping(int sensor_sw_id, int *sensor_hw_num)
{
	int i = 0;
	bool id_found = false;

	if (tsens_is_ready() <= 0) {
		pr_debug("TSENS early init not done\n");
		return -EPROBE_DEFER;
	}

	while (i < tmdev->tsens_num_sensor && !id_found) {
		if (sensor_sw_id == tmdev->sensor[i].sensor_sw_id) {
			*sensor_hw_num = tmdev->sensor[i].sensor_hw_num;
			id_found = true;
		}
		i++;
	}

	if (!id_found)
		return -EINVAL;

	return 0;
}
EXPORT_SYMBOL(tsens_get_hw_id_mapping);

static int tsens_tz_code_to_degc(int adc_code, int sensor_sw_id)
{
	int degc, num, den, idx;

	idx = sensor_sw_id;
	num = ((adc_code * tmdev->tsens_factor) -
				tmdev->sensor[idx].offset);
	den = (int) tmdev->sensor[idx].slope_mul_tsens_factor;

	if (num > 0)
		degc = ((num + (den/2))/den);
	else if (num < 0)
		degc = ((num - (den/2))/den);
	else
		degc = num/den;

	pr_debug("raw_code:0x%x, sensor_num:%d, degc:%d, offset:%d\n",
			adc_code, idx, degc, tmdev->sensor[idx].offset);

	return degc;
}

static int tsens_tz_degc_to_code(int degc, int idx)
{
	int code = ((degc * tmdev->sensor[idx].slope_mul_tsens_factor)
		+ tmdev->sensor[idx].offset)/tmdev->tsens_factor;

	if (code > TSENS_THRESHOLD_MAX_CODE)
		code = TSENS_THRESHOLD_MAX_CODE;
	else if (code < TSENS_THRESHOLD_MIN_CODE)
		code = TSENS_THRESHOLD_MIN_CODE;
	pr_debug("raw_code:0x%x, sensor_num:%d, degc:%d\n",
			code, idx, degc);
	return code;
}

static void msm_tsens_get_temp(int sensor_hw_num, unsigned long *temp)
{
	unsigned int code;
	void __iomem *sensor_addr;
	void __iomem *trdy_addr;
	int sensor_sw_id = -EINVAL, rc = 0, last_temp = 0, last_temp2 = 0;
	int last_temp3 = 0;
	bool last_temp_valid = false, last_temp2_valid = false;
	bool last_temp3_valid = false;

	if (tmdev->tsens_type == TSENS_TYPE2) {
		trdy_addr = TSENS2_TRDY_ADDR(tmdev->tsens_addr);
		sensor_addr = TSENS2_SN_STATUS_ADDR(tmdev->tsens_addr);
	} else {
		trdy_addr = TSENS_TRDY_ADDR(tmdev->tsens_addr);
		sensor_addr = TSENS_S0_STATUS_ADDR(tmdev->tsens_addr);
	}

	if ((!tmdev->prev_reading_avail) && !tmdev->tsens_valid_status_check) {
		while (!((readl_relaxed(trdy_addr)) & TSENS_TRDY_MASK))
			usleep_range(TSENS_TRDY_RDY_MIN_TIME,
				TSENS_TRDY_RDY_MAX_TIME);
		tmdev->prev_reading_avail = true;
	}

	code = readl_relaxed(sensor_addr +
			(sensor_hw_num << TSENS_STATUS_ADDR_OFFSET));
	last_temp = code & TSENS_SN_STATUS_TEMP_MASK;

	if (tmdev->tsens_valid_status_check) {
		if (code & TSENS2_SN_STATUS_VALID_MASK)
			last_temp_valid = true;
		else {
			code = readl_relaxed(sensor_addr +
				(sensor_hw_num << TSENS_STATUS_ADDR_OFFSET));
			last_temp2 = code & TSENS_SN_STATUS_TEMP_MASK;
			if (code & TSENS2_SN_STATUS_VALID_MASK) {
				last_temp = last_temp2;
				last_temp2_valid = true;
			} else {
				code = readl_relaxed(sensor_addr +
					(sensor_hw_num <<
					TSENS_STATUS_ADDR_OFFSET));
				last_temp3 = code & TSENS_SN_STATUS_TEMP_MASK;
				if (code & TSENS2_SN_STATUS_VALID_MASK) {
					last_temp = last_temp3;
					last_temp3_valid = true;
				}
			}
		}
	}

	if ((tmdev->tsens_valid_status_check) &&
		(!last_temp_valid && !last_temp2_valid && !last_temp3_valid)) {
		if (last_temp == last_temp2)
			last_temp = last_temp2;
		else if (last_temp2 == last_temp3)
			last_temp = last_temp3;
	}

	/* Obtain SW index to map the corresponding thermal zone's
	 * offset and slope for code to degc conversion. */
	rc = tsens_get_sw_id_mapping(sensor_hw_num, &sensor_sw_id);
	if (rc < 0) {
		pr_err("tsens mapping index not found\n");
		return;
	}

	*temp = tsens_tz_code_to_degc(last_temp, sensor_sw_id);
	trace_tsens_read(*temp, sensor_hw_num);
}

static int tsens_tz_get_temp(struct thermal_zone_device *thermal,
			     unsigned long *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || tm_sensor->mode != THERMAL_DEVICE_ENABLED || !temp)
		return -EINVAL;

	msm_tsens_get_temp(tm_sensor->sensor_hw_num, temp);

	return 0;
}

int tsens_get_temp(struct tsens_device *device, unsigned long *temp)
{
	if (tsens_is_ready() <= 0) {
		pr_debug("TSENS early init not done\n");
		return -EPROBE_DEFER;
	}

	msm_tsens_get_temp(device->sensor_num, temp);

	return 0;
}
EXPORT_SYMBOL(tsens_get_temp);

int tsens_get_max_sensor_num(uint32_t *tsens_num_sensors)
{
	if (tsens_is_ready() <= 0) {
		pr_debug("TSENS early init not done\n");
		return -EPROBE_DEFER;
	}

	*tsens_num_sensors = tmdev->tsens_num_sensor;

	return 0;
}
EXPORT_SYMBOL(tsens_get_max_sensor_num);

static int tsens_tz_get_mode(struct thermal_zone_device *thermal,
			      enum thermal_device_mode *mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || !mode)
		return -EINVAL;

	*mode = tm_sensor->mode;

	return 0;
}

static int tsens_tz_get_trip_type(struct thermal_zone_device *thermal,
				   int trip, enum thermal_trip_type *type)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;

	if (!tm_sensor || trip < 0 || !type)
		return -EINVAL;

	switch (trip) {
	case TSENS_TRIP_WARM:
		*type = THERMAL_TRIP_CONFIGURABLE_HI;
		break;
	case TSENS_TRIP_COOL:
		*type = THERMAL_TRIP_CONFIGURABLE_LOW;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int tsens_tz_activate_trip_type(struct thermal_zone_device *thermal,
			int trip, enum thermal_trip_activation_mode mode)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_cntl, code, hi_code, lo_code, mask;

	if (!tm_sensor || trip < 0)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	reg_cntl = readl_relaxed((TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_addr) +
					(tm_sensor->sensor_hw_num *
					TSENS_SN_ADDR_OFFSET)));
	switch (trip) {
	case TSENS_TRIP_WARM:
		code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		mask = TSENS_UPPER_STATUS_CLR;

		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		break;
	case TSENS_TRIP_COOL:
		code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		mask = TSENS_LOWER_STATUS_CLR;

		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	if (mode == THERMAL_TRIP_ACTIVATION_DISABLED)
		writel_relaxed(reg_cntl | mask,
			(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
						(tmdev->tsens_addr) +
			(tm_sensor->sensor_hw_num * TSENS_SN_ADDR_OFFSET)));
	else {
		if (code < lo_code || code > hi_code) {
			pr_err("%s with invalid code %x\n", __func__, code);
			return -EINVAL;
		}
		writel_relaxed(reg_cntl & ~mask,
		(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(tmdev->tsens_addr) +
		(tm_sensor->sensor_hw_num * TSENS_SN_ADDR_OFFSET)));
	}
	mb();
	return 0;
}

static int tsens_tz_get_trip_temp(struct thermal_zone_device *thermal,
				   int trip, unsigned long *temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg;
	int sensor_sw_id = -EINVAL, rc = 0;

	if (!tm_sensor || trip < 0 || !temp)
		return -EINVAL;

	reg = readl_relaxed(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
						(tmdev->tsens_addr) +
			(tm_sensor->sensor_hw_num * TSENS_SN_ADDR_OFFSET));
	switch (trip) {
	case TSENS_TRIP_WARM:
		reg = (reg & TSENS_UPPER_THRESHOLD_MASK) >>
				TSENS_UPPER_THRESHOLD_SHIFT;
		break;
	case TSENS_TRIP_COOL:
		reg = (reg & TSENS_LOWER_THRESHOLD_MASK);
		break;
	default:
		return -EINVAL;
	}

	rc = tsens_get_sw_id_mapping(tm_sensor->sensor_hw_num, &sensor_sw_id);
	if (rc < 0) {
		pr_err("tsens mapping index not found\n");
		return rc;
	}
	*temp = tsens_tz_code_to_degc(reg, sensor_sw_id);

	return 0;
}

static int tsens_tz_notify(struct thermal_zone_device *thermal,
				int count, enum thermal_trip_type type)
{
	/* Critical temperature threshold are enabled and will
	 * shutdown the device once critical thresholds are crossed. */
	pr_debug("%s debug\n", __func__);
	return 1;
}

static int tsens_tz_set_trip_temp(struct thermal_zone_device *thermal,
				   int trip, unsigned long temp)
{
	struct tsens_tm_device_sensor *tm_sensor = thermal->devdata;
	unsigned int reg_cntl;
	int code, hi_code, lo_code, code_err_chk, sensor_sw_id = 0, rc = 0;

	if (!tm_sensor || trip < 0)
		return -EINVAL;

	rc = tsens_get_sw_id_mapping(tm_sensor->sensor_hw_num, &sensor_sw_id);
	if (rc < 0) {
		pr_err("tsens mapping index not found\n");
		return rc;
	}
	code_err_chk = code = tsens_tz_degc_to_code(temp, sensor_sw_id);
	if (!tm_sensor || trip < 0)
		return -EINVAL;

	lo_code = TSENS_THRESHOLD_MIN_CODE;
	hi_code = TSENS_THRESHOLD_MAX_CODE;

	reg_cntl = readl_relaxed(TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
			(tmdev->tsens_addr) + (tm_sensor->sensor_hw_num *
					TSENS_SN_ADDR_OFFSET));
	switch (trip) {
	case TSENS_TRIP_WARM:
		code <<= TSENS_UPPER_THRESHOLD_SHIFT;
		reg_cntl &= ~TSENS_UPPER_THRESHOLD_MASK;
		if (!(reg_cntl & TSENS_LOWER_STATUS_CLR))
			lo_code = (reg_cntl & TSENS_LOWER_THRESHOLD_MASK);
		break;
	case TSENS_TRIP_COOL:
		reg_cntl &= ~TSENS_LOWER_THRESHOLD_MASK;
		if (!(reg_cntl & TSENS_UPPER_STATUS_CLR))
			hi_code = (reg_cntl & TSENS_UPPER_THRESHOLD_MASK)
					>> TSENS_UPPER_THRESHOLD_SHIFT;
		break;
	default:
		return -EINVAL;
	}

	if (code_err_chk < lo_code || code_err_chk > hi_code)
		return -EINVAL;

	writel_relaxed(reg_cntl | code, (TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR
					(tmdev->tsens_addr) +
					(tm_sensor->sensor_hw_num *
					TSENS_SN_ADDR_OFFSET)));
	mb();
	return 0;
}

static struct thermal_zone_device_ops tsens_thermal_zone_ops = {
	.get_temp = tsens_tz_get_temp,
	.get_mode = tsens_tz_get_mode,
	.get_trip_type = tsens_tz_get_trip_type,
	.activate_trip_type = tsens_tz_activate_trip_type,
	.get_trip_temp = tsens_tz_get_trip_temp,
	.set_trip_temp = tsens_tz_set_trip_temp,
	.notify = tsens_tz_notify,
};

static void notify_uspace_tsens_fn(struct work_struct *work)
{
	struct tsens_tm_device_sensor *tm = container_of(work,
		struct tsens_tm_device_sensor, work);

	sysfs_notify(&tm->tz_dev->device.kobj,
					NULL, "type");
}

static void tsens_scheduler_fn(struct work_struct *work)
{
	struct tsens_tm_device *tm = container_of(work, struct tsens_tm_device,
						tsens_work);
	unsigned int i, status, threshold;
	void __iomem *sensor_status_addr;
	void __iomem *sensor_status_ctrl_addr;
	int sensor_sw_id = -EINVAL, rc = 0;

	if (tmdev->tsens_type == TSENS_TYPE2)
		sensor_status_addr = TSENS2_SN_STATUS_ADDR(tmdev->tsens_addr);
	else
		sensor_status_addr = TSENS_S0_STATUS_ADDR(tmdev->tsens_addr);

	sensor_status_ctrl_addr =
		TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(tmdev->tsens_addr);
	for (i = 0; i < tm->tsens_num_sensor; i++) {
		bool upper_thr = false, lower_thr = false;
		uint32_t addr_offset;

		addr_offset = tm->sensor[i].sensor_hw_num *
						TSENS_SN_ADDR_OFFSET;
		status = readl_relaxed(sensor_status_addr + addr_offset);
		threshold = readl_relaxed(sensor_status_ctrl_addr +
								addr_offset);
		if (status & TSENS_SN_STATUS_UPPER_STATUS) {
			writel_relaxed(threshold | TSENS_UPPER_STATUS_CLR,
				TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(
					tmdev->tsens_addr + addr_offset));
			upper_thr = true;
		}
		if (status & TSENS_SN_STATUS_LOWER_STATUS) {
			writel_relaxed(threshold | TSENS_LOWER_STATUS_CLR,
				TSENS_S0_UPPER_LOWER_STATUS_CTRL_ADDR(
					tmdev->tsens_addr + addr_offset));
			lower_thr = true;
		}
		if (upper_thr || lower_thr) {
			unsigned long temp;
			enum thermal_trip_type trip =
					THERMAL_TRIP_CONFIGURABLE_LOW;

			if (upper_thr)
				trip = THERMAL_TRIP_CONFIGURABLE_HI;
			tsens_tz_get_temp(tm->sensor[i].tz_dev, &temp);
			thermal_sensor_trip(tm->sensor[i].tz_dev, trip, temp);

			/* Notify user space */
			queue_work(tm->tsens_wq, &tm->sensor[i].work);
			rc = tsens_get_sw_id_mapping(
					tm->sensor[i].sensor_hw_num,
					&sensor_sw_id);
			if (rc < 0)
				pr_err("tsens mapping index not found\n");
			pr_debug("sensor:%d trigger temp (%d degC)\n",
				tm->sensor[i].sensor_hw_num,
				tsens_tz_code_to_degc((status &
				TSENS_SN_STATUS_TEMP_MASK),
				sensor_sw_id));
			if (upper_thr)
				trace_tsens_threshold_hit(
					tsens_tz_code_to_degc((threshold &
					TSENS_UPPER_THRESHOLD_MASK) >>
					TSENS_UPPER_THRESHOLD_SHIFT,
					sensor_sw_id),
					tm->sensor[i].sensor_hw_num);
			else
				trace_tsens_threshold_clear(
					tsens_tz_code_to_degc((threshold &
					TSENS_LOWER_THRESHOLD_MASK),
					sensor_sw_id),
					tm->sensor[i].sensor_hw_num);
		}
	}
	mb();

	enable_irq(tmdev->tsens_irq);
}

static irqreturn_t tsens_isr(int irq, void *data)
{
	disable_irq_nosync(tmdev->tsens_irq);

	queue_work(tmdev->tsens_wq, &tmdev->tsens_work);

	return IRQ_HANDLED;
}

static void tsens_hw_init(void)
{
	unsigned int reg_cntl = 0, sensor_en = 0;
	unsigned int i;

	if (tmdev->tsens_local_init && tmdev->tsens_type == TSENS_TYPE0) {
		writel_relaxed(reg_cntl, TSENS_CTRL_ADDR(tmdev->tsens_addr));
		writel_relaxed(reg_cntl | TSENS_SW_RST,
			TSENS_CTRL_ADDR(tmdev->tsens_addr));
		reg_cntl |= (TSENS_62_5_MS_MEAS_PERIOD <<
		TSENS_MEAS_PERIOD_SHIFT);
		for (i = 0; i < tmdev->tsens_num_sensor; i++)
			sensor_en |= (1 << tmdev->sensor[i].sensor_hw_num);
		sensor_en <<= TSENS_SENSOR0_SHIFT;
		reg_cntl |= (sensor_en | TSENS_EN);
		writel_relaxed(reg_cntl, TSENS_CTRL_ADDR(tmdev->tsens_addr));
		writel_relaxed(TSENS_GLOBAL_INIT_DATA,
			TSENS_GLOBAL_CONFIG(tmdev->tsens_addr));
		writel_relaxed(TSENS_S0_MAIN_CFG_INIT_DATA,
			TSENS_S0_MAIN_CONFIG(tmdev->tsens_addr));
		for (i = 0; i < tmdev->tsens_num_sensor; i++) {
			writel_relaxed(TSENS_SN_REMOTE_CFG_DATA,
			TSENS_SN_REMOTE_CONFIG(tmdev->tsens_addr)
				+ (tmdev->sensor[i].sensor_hw_num *
						TSENS_SN_ADDR_OFFSET));
		}
		pr_debug("Local TSENS control initialization\n");
	}
	writel_relaxed(TSENS_INTERRUPT_EN,
		TSENS_UPPER_LOWER_INTERRUPT_CTRL(tmdev->tsens_addr));
}

static int tsens_calib_msm8909_sensors(void)
{
	int i, tsens_base0_data = 0, tsens_base1_data = 0;
	int tsens0_point1 = 0, tsens0_point2 = 0;
	int tsens1_point1 = 0, tsens1_point2 = 0;
	int tsens2_point1 = 0, tsens2_point2 = 0;
	int tsens3_point1 = 0, tsens3_point2 = 0;
	int tsens4_point1 = 0, tsens4_point2 = 0;
	int tsens_calibration_mode = 0, temp = 0;
	uint32_t calib_data[3] = {0, 0, 0};
	uint32_t calib_tsens_point1_data[5], calib_tsens_point2_data[5];

	if (!tmdev->calibration_less_mode) {

		calib_data[0] = readl_relaxed(
			TSENS_8939_EEPROM(tmdev->tsens_calib_addr));
		calib_data[1] = readl_relaxed(
			(TSENS_8939_EEPROM(tmdev->tsens_calib_addr) + 0x4));
		calib_data[2] = readl_relaxed(
			(TSENS_8939_EEPROM(tmdev->tsens_calib_addr) + 0x3c));

		tsens_calibration_mode =
			(calib_data[2] & TSENS_MSM8909_TSENS_CAL_SEL) >>
				TSENS_MSM8909_CAL_SEL_SHIFT;

		pr_debug("calib mode is %d\n", tsens_calibration_mode);
	}

	if ((tsens_calibration_mode == TSENS_TWO_POINT_CALIB) ||
		(tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2)) {
		tsens_base0_data = (calib_data[2] & TSENS_MSM8909_BASE0_MASK);
		tsens0_point1 = (calib_data[0] & TSENS0_MSM8909_POINT1_MASK);
		tsens1_point1 = (calib_data[0] & TSENS1_MSM8909_POINT1_MASK)
			>> TSENS1_MSM8909_POINT1_SHIFT;
		tsens2_point1 = (calib_data[0] & TSENS2_MSM8909_POINT1_MASK)
			>> TSENS2_MSM8909_POINT1_SHIFT;
		tsens3_point1 = (calib_data[1] & TSENS3_MSM8909_POINT1_MASK)
			>> TSENS3_MSM8909_POINT1_SHIFT;
		tsens4_point1 = (calib_data[1] & TSENS4_MSM8909_POINT1_MASK)
			>> TSENS4_MSM8909_POINT1_SHIFT;
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		tsens_base1_data = (calib_data[2] & TSENS_MSM8909_BASE1_MASK)
			>> TSENS_MSM8909_BASE1_SHIFT;
		tsens0_point2 = (calib_data[0] & TSENS0_MSM8909_POINT2_MASK)
			>> TSENS0_MSM8909_POINT2_SHIFT;
		tsens1_point2 = (calib_data[0] & TSENS1_MSM8909_POINT2_MASK)
			>> TSENS1_MSM8909_POINT2_SHIFT;
		tsens2_point2 =
			(calib_data[0] & TSENS2_MSM8909_POINT2_MASK_0_1)
				>> TSENS2_MSM8909_POINT2_SHIFT_0_1;
		temp = (calib_data[1] & TSENS2_MSM8909_POINT2_MASK_2_5) <<
			TSENS2_MSM8909_POINT2_SHIFT_2_5;
		tsens2_point2 |= temp;
		tsens3_point2 = (calib_data[1] & TSENS3_MSM8909_POINT2_MASK)
			>> TSENS3_MSM8909_POINT2_SHIFT;
		tsens4_point2 = (calib_data[1] & TSENS4_MSM8909_POINT2_MASK)
			>> TSENS4_MSM8909_POINT2_SHIFT;
	}

	if (tsens_calibration_mode == 0) {
		pr_debug("TSENS is calibrationless mode\n");
		for (i = 0; i < tmdev->tsens_num_sensor; i++)
			calib_tsens_point2_data[i] = 780;
		calib_tsens_point1_data[0] = 500;
		calib_tsens_point1_data[1] = 500;
		calib_tsens_point1_data[2] = 500;
		calib_tsens_point1_data[3] = 500;
		calib_tsens_point1_data[4] = 500;
	}

	if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		calib_tsens_point1_data[0] =
			(((tsens_base0_data) + tsens0_point1) << 2);
		calib_tsens_point1_data[1] =
			(((tsens_base0_data) + tsens1_point1) << 2);
		calib_tsens_point1_data[1] = calib_tsens_point1_data[1] -
						TSENS_MSM8909_D30_WA_S1;
		calib_tsens_point1_data[2] =
			(((tsens_base0_data) + tsens2_point1) << 2);
		calib_tsens_point1_data[3] =
			(((tsens_base0_data) + tsens3_point1) << 2);
		calib_tsens_point1_data[3] = calib_tsens_point1_data[3] -
						TSENS_MSM8909_D30_WA_S3;
		calib_tsens_point1_data[4] =
			(((tsens_base0_data) + tsens4_point1) << 2);
		calib_tsens_point1_data[4] = calib_tsens_point1_data[4] -
						TSENS_MSM8909_D30_WA_S4;
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		pr_debug("two point calibration calculation\n");
		calib_tsens_point2_data[0] =
			((tsens_base1_data + tsens0_point2) << 2);
		calib_tsens_point2_data[1] =
			((tsens_base1_data + tsens1_point2) << 2);
		calib_tsens_point2_data[1] = calib_tsens_point2_data[1] -
						TSENS_MSM8909_D120_WA_S1;
		calib_tsens_point2_data[2] =
			((tsens_base1_data + tsens2_point2) << 2);
		calib_tsens_point2_data[3] =
			((tsens_base1_data + tsens3_point2) << 2);
		calib_tsens_point2_data[3] = calib_tsens_point2_data[3] -
						TSENS_MSM8909_D120_WA_S3;
		calib_tsens_point2_data[4] =
			((tsens_base1_data + tsens4_point2) << 2);
		calib_tsens_point2_data[4] = calib_tsens_point2_data[4] -
						TSENS_MSM8909_D120_WA_S4;
	}

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0;
		tmdev->sensor[i].calib_data_point2 = calib_tsens_point2_data[i];
		tmdev->sensor[i].calib_data_point1 = calib_tsens_point1_data[i];
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
				i, tmdev->sensor[i].calib_data_point1,
				tmdev->sensor[i].calib_data_point2);
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
			 * temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
				tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		tmdev->sensor[i].offset = (tmdev->sensor[i].calib_data_point1 *
				tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		pr_debug("offset:%d and slope:%d\n", tmdev->sensor[i].offset,
				tmdev->sensor[i].slope_mul_tsens_factor);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int tsens_calib_8939_sensors(void)
{
	int i, tsens_base0_data = 0, tsens_base1_data = 0, v3_enable = 0;
	int tsens0_point1 = 0, tsens0_point2 = 0;
	int tsens1_point1 = 0, tsens1_point2 = 0;
	int tsens2_point1 = 0, tsens2_point2 = 0;
	int tsens3_point1 = 0, tsens3_point2 = 0;
	int tsens4_point1 = 0, tsens4_point2 = 0;
	int tsens5_point1 = 0, tsens5_point2 = 0;
	int tsens6_point1 = 0, tsens6_point2 = 0;
	int tsens7_point1 = 0, tsens7_point2 = 0;
	int tsens8_point1 = 0, tsens8_point2 = 0;
	int tsens9_point1 = 0, tsens9_point2 = 0;
	int tsens_calibration_mode = 0, temp = 0;
	uint32_t calib_data[6] = {0, 0, 0, 0, 0, 0};
	uint32_t calib_tsens_point1_data[10], calib_tsens_point2_data[10];

	if (tmdev->tsens_num_sensor == TSENS_NUM_SENSORS_V3)
		v3_enable = 1;

	if (!tmdev->calibration_less_mode) {

		calib_data[0] = readl_relaxed(
			TSENS_8939_EEPROM(tmdev->tsens_calib_addr) + 0x30);
		calib_data[1] = readl_relaxed(
			(TSENS_8939_EEPROM(tmdev->tsens_calib_addr) + 0x34));
		calib_data[2] = readl_relaxed(
			(TSENS_8939_EEPROM(tmdev->tsens_calib_addr)));
		calib_data[3] = readl_relaxed(
			(TSENS_8939_EEPROM(tmdev->tsens_calib_addr) + 0x4));
		if (v3_enable) {
			calib_data[4] = readl_relaxed(
				(TSENS_8939_EEPROM(tmdev->tsens_calib_addr) -
					0x58));
			calib_data[5] = readl_relaxed(
				(TSENS_8939_EEPROM(tmdev->tsens_calib_addr) -
					0x54));
		}

		tsens_calibration_mode =
			(calib_data[0] & TSENS_8939_TSENS_CAL_SEL);

		pr_debug("calib mode is %d\n", tsens_calibration_mode);
	}

	if ((tsens_calibration_mode == TSENS_TWO_POINT_CALIB) ||
		(tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2)) {
		tsens_base0_data = (calib_data[2] & TSENS_8939_BASE0_MASK);
		tsens0_point1 = (calib_data[0] & TSENS0_8939_POINT1_MASK) >>
			TSENS0_8939_POINT1_SHIFT;
		tsens1_point1 = (calib_data[0] & TSENS1_8939_POINT1_MASK) >>
			TSENS1_8939_POINT1_SHIFT;
		tsens2_point1 = (calib_data[0] & TSENS2_8939_POINT1_MASK_0_4)
			>> TSENS2_8939_POINT1_SHIFT_0_4;
		temp = (calib_data[1] & TSENS2_8939_POINT1_MASK_5) <<
			TSENS2_8939_POINT1_SHIFT_5;
		tsens2_point1 |= temp;
		tsens3_point1 = (calib_data[1] & TSENS3_8939_POINT1_MASK) >>
			TSENS3_8939_POINT1_SHIFT;
		tsens4_point1 = (calib_data[1] & TSENS4_8939_POINT1_MASK) >>
			TSENS4_8939_POINT1_SHIFT;
		tsens5_point1 = (calib_data[2] & TSENS5_8939_POINT1_MASK) >>
			TSENS5_8939_POINT1_SHIFT;
		tsens6_point1 = (calib_data[2] & TSENS6_8939_POINT1_MASK) >>
			TSENS6_8939_POINT1_SHIFT;
		tsens7_point1 = (calib_data[3] & TSENS7_8939_POINT1_MASK);
		tsens8_point1 = (calib_data[3] & TSENS8_8939_POINT1_MASK) >>
			TSENS8_8939_POINT1_SHIFT;
		if (v3_enable)
			tsens9_point1 = (calib_data[4] &
				TSENS9_8939_POINT1_MASK) >>
					TSENS9_8939_POINT1_SHIFT;
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		tsens_base1_data = (calib_data[3] & TSENS_8939_BASE1_MASK) >>
			TSENS_8939_BASE1_SHIFT;
		tsens0_point2 = (calib_data[0] & TSENS0_8939_POINT2_MASK) >>
			TSENS0_8939_POINT2_SHIFT;
		tsens1_point2 = (calib_data[0] & TSENS1_8939_POINT2_MASK) >>
			TSENS1_8939_POINT2_SHIFT;
		tsens2_point2 = (calib_data[1] & TSENS2_8939_POINT2_MASK) >>
			TSENS2_8939_POINT2_SHIFT;
		tsens3_point2 = (calib_data[1] & TSENS3_8939_POINT2_MASK) >>
			TSENS3_8939_POINT2_SHIFT;
		tsens4_point2 = (calib_data[1] & TSENS4_8939_POINT2_MASK) >>
			TSENS4_8939_POINT2_SHIFT;
		tsens5_point2 = (calib_data[2] & TSENS5_8939_POINT2_MASK) >>
			TSENS5_8939_POINT2_SHIFT;
		tsens6_point2 = (calib_data[2] & TSENS6_8939_POINT2_MASK) >>
			TSENS6_8939_POINT2_SHIFT;
		tsens7_point2 = (calib_data[3] & TSENS7_8939_POINT2_MASK) >>
			TSENS7_8939_POINT2_SHIFT;
		tsens8_point2 = (calib_data[3] & TSENS8_8939_POINT2_MASK) >>
			TSENS8_8939_POINT2_SHIFT;
		if (v3_enable) {
			tsens9_point2 = (calib_data[4] &
				TSENS9_8939_POINT2_MASK_0_4) >>
					TSENS9_8939_POINT2_SHIFT_0_4;
			temp = (calib_data[5] & TSENS9_8939_POINT2_MASK_5) >>
				TSENS9_8939_POINT2_SHIFT_5;
			tsens9_point2 |= temp;
		}
	}

	if (tsens_calibration_mode == 0) {
		pr_debug("TSENS is calibrationless mode\n");
		for (i = 0; i < tmdev->tsens_num_sensor; i++) {
			calib_tsens_point2_data[i] = 780;
			calib_tsens_point1_data[i] = 500;
		}
	}

	if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		calib_tsens_point1_data[0] =
			(((tsens_base0_data) + tsens0_point1) << 2);
		calib_tsens_point1_data[1] =
			(((tsens_base0_data) + tsens1_point1) << 2);
		calib_tsens_point1_data[2] =
			(((tsens_base0_data) + tsens2_point1) << 2);
		calib_tsens_point1_data[3] =
			(((tsens_base0_data) + tsens3_point1) << 2);
		calib_tsens_point1_data[4] =
			(((tsens_base0_data) + tsens4_point1) << 2);
		calib_tsens_point1_data[5] =
			(((tsens_base0_data) + tsens5_point1) << 2);
		calib_tsens_point1_data[6] =
			(((tsens_base0_data) + tsens6_point1) << 2);
		calib_tsens_point1_data[7] =
			(((tsens_base0_data) + tsens7_point1) << 2);
		calib_tsens_point1_data[8] =
			(((tsens_base0_data) + tsens8_point1) << 2);
		if (v3_enable)
			calib_tsens_point1_data[9] =
				(((tsens_base0_data) + tsens9_point1) << 2);
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		pr_debug("two point calibration calculation\n");
		calib_tsens_point2_data[0] =
			((tsens_base1_data + tsens0_point2) << 2);
		calib_tsens_point2_data[1] =
			((tsens_base1_data + tsens1_point2) << 2);
		calib_tsens_point2_data[2] =
			((tsens_base1_data + tsens2_point2) << 2);
		calib_tsens_point2_data[3] =
			((tsens_base1_data + tsens3_point2) << 2);
		calib_tsens_point2_data[4] =
			((tsens_base1_data + tsens4_point2) << 2);
		calib_tsens_point2_data[5] =
			((tsens_base1_data + tsens5_point2) << 2);
		calib_tsens_point2_data[6] =
			((tsens_base1_data + tsens6_point2) << 2);
		calib_tsens_point2_data[7] =
			((tsens_base1_data + tsens7_point2) << 2);
		calib_tsens_point2_data[8] =
			((tsens_base1_data + tsens8_point2) << 2);
		if (v3_enable)
			calib_tsens_point2_data[9] =
				((tsens_base1_data + tsens9_point2) << 2);
	}

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0;
		tmdev->sensor[i].calib_data_point2 = calib_tsens_point2_data[i];
		tmdev->sensor[i].calib_data_point1 = calib_tsens_point1_data[i];
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
				i, tmdev->sensor[i].calib_data_point1,
				tmdev->sensor[i].calib_data_point2);
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
			 * temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
				tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		tmdev->sensor[i].offset = (tmdev->sensor[i].calib_data_point1 *
				tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		pr_debug("offset:%d and slope:%d\n", tmdev->sensor[i].offset,
				tmdev->sensor[i].slope_mul_tsens_factor);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int tsens_calib_8916_sensors(void)
{
	int i, tsens_base0_data = 0, tsens_base1_data = 0;
	int tsens0_point1 = 0, tsens0_point2 = 0;
	int tsens1_point1 = 0, tsens1_point2 = 0;
	int tsens2_point1 = 0, tsens2_point2 = 0;
	int tsens3_point1 = 0, tsens3_point2 = 0;
	int tsens4_point1 = 0, tsens4_point2 = 0;
	int tsens_calibration_mode = 0;
	uint32_t calib_data[3] = {0, 0, 0};
	uint32_t calib_tsens_point1_data[5], calib_tsens_point2_data[5];

	if (!tmdev->calibration_less_mode) {

		calib_data[0] = readl_relaxed(
			TSENS_EEPROM(tmdev->tsens_calib_addr));
		calib_data[1] = readl_relaxed(
			(TSENS_EEPROM(tmdev->tsens_calib_addr) + 0x4));
		calib_data[2] = readl_relaxed(
			(TSENS_EEPROM(tmdev->tsens_calib_addr) + 0x1c));

		tsens_calibration_mode =
			(calib_data[2] & TSENS_8916_TSENS_CAL_SEL) >>
				TSENS_8916_CAL_SEL_SHIFT;

		pr_debug("calib mode is %d\n", tsens_calibration_mode);
	}

	if ((tsens_calibration_mode == TSENS_TWO_POINT_CALIB) ||
		(tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2)) {
		tsens_base0_data = (calib_data[0] & TSENS_8916_BASE0_MASK);
		tsens0_point1 = (calib_data[0] & TSENS0_8916_POINT1_MASK) >>
			TSENS0_8916_POINT1_SHIFT;
		tsens1_point1 = (calib_data[0] & TSENS1_8916_POINT1_MASK) >>
			TSENS1_8916_POINT1_SHIFT;
		tsens2_point1 = (calib_data[0] & TSENS2_8916_POINT1_MASK) >>
			TSENS2_8916_POINT1_SHIFT;
		tsens3_point1 = (calib_data[1] & TSENS3_8916_POINT1_MASK) >>
			TSENS3_8916_POINT1_SHIFT;
		tsens4_point1 = (calib_data[1] & TSENS4_8916_POINT1_MASK) >>
			TSENS4_8916_POINT1_SHIFT;
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		tsens_base1_data = (calib_data[1] & TSENS_8916_BASE1_MASK) >>
			TSENS_8916_BASE1_SHIFT;
		tsens0_point2 = (calib_data[0] & TSENS0_8916_POINT2_MASK) >>
			TSENS0_8916_POINT2_SHIFT;
		tsens1_point2 = (calib_data[0] & TSENS1_8916_POINT2_MASK) >>
			TSENS1_8916_POINT2_SHIFT;
		tsens2_point2 = (calib_data[1] & TSENS2_8916_POINT2_MASK);
		tsens3_point2 = (calib_data[1] & TSENS3_8916_POINT2_MASK) >>
			TSENS3_8916_POINT2_SHIFT;
		tsens4_point2 = (calib_data[1] & TSENS4_8916_POINT2_MASK) >>
			TSENS4_8916_POINT2_SHIFT;
	}

	if (tsens_calibration_mode == 0) {
		pr_debug("TSENS is calibrationless mode\n");
		for (i = 0; i < tmdev->tsens_num_sensor; i++)
			calib_tsens_point2_data[i] = 780;
		calib_tsens_point1_data[0] = 500;
		calib_tsens_point1_data[1] = 500;
		calib_tsens_point1_data[2] = 500;
		calib_tsens_point1_data[3] = 500;
		calib_tsens_point1_data[4] = 500;
	}

	if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		calib_tsens_point1_data[0] =
			(((tsens_base0_data) + tsens0_point1) << 3);
		calib_tsens_point1_data[1] =
			(((tsens_base0_data) + tsens1_point1) << 3);
		calib_tsens_point1_data[2] =
			(((tsens_base0_data) + tsens2_point1) << 3);
		calib_tsens_point1_data[3] =
			(((tsens_base0_data) + tsens3_point1) << 3);
		calib_tsens_point1_data[4] =
			(((tsens_base0_data) + tsens4_point1) << 3);
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		pr_debug("two point calibration calculation\n");
		calib_tsens_point2_data[0] =
			((tsens_base1_data + tsens0_point2) << 3);
		calib_tsens_point2_data[1] =
			((tsens_base1_data + tsens1_point2) << 3);
		calib_tsens_point2_data[2] =
			((tsens_base1_data + tsens2_point2) << 3);
		calib_tsens_point2_data[3] =
			((tsens_base1_data + tsens3_point2) << 3);
		calib_tsens_point2_data[4] =
			((tsens_base1_data + tsens4_point2) << 3);
	}

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0;
		tmdev->sensor[i].calib_data_point2 = calib_tsens_point2_data[i];
		tmdev->sensor[i].calib_data_point1 = calib_tsens_point1_data[i];
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
				i, tmdev->sensor[i].calib_data_point1,
				tmdev->sensor[i].calib_data_point2);
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
			 * temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
				tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		tmdev->sensor[i].offset = (tmdev->sensor[i].calib_data_point1 *
				tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		pr_debug("offset:%d and slope:%d\n", tmdev->sensor[i].offset,
				tmdev->sensor[i].slope_mul_tsens_factor);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int tsens_calib_9630_sensors(void)
{
	int i, tsens_base0_data = 0, tsens0_point = 0, tsens1_point = 0;
	int tsens2_point = 0, tsens3_point = 0, tsens4_point = 0;
	int tsens_base1_data = 0, tsens_calibration_mode = 0, calib_mode = 0;
	uint32_t calib_data[2], calib_tsens_point_data[5];

	if (tmdev->calibration_less_mode)
		goto calibration_less_mode;

	calib_data[0] = readl_relaxed(
			TSENS_EEPROM(tmdev->tsens_calib_addr));
	calib_data[1] = readl_relaxed(
			(TSENS_EEPROM(tmdev->tsens_calib_addr) + 0x4));

	calib_mode = (calib_data[1] & TSENS_TORINO_CALIB_PT) >>
				TSENS_TORINO_CALIB_SHIFT;
	pr_debug("calib mode is %d\n", calib_mode);
	if (calib_mode == TSENS_TWO_POINT_CALIB) {
		tsens_base0_data = (calib_data[0] & TSENS_TORINO_BASE0);
		tsens_base1_data = (calib_data[0] & TSENS_TORINO_BASE1) >>
				TSENS_TORINO_BASE1_SHIFT;
		tsens0_point = (calib_data[0] & TSENS_TORINO_POINT0) >>
				TSENS_TORINO_POINT0_SHIFT;
		tsens1_point = (calib_data[0] & TSENS_TORINO_POINT1) >>
				TSENS_TORINO_POINT1_SHIFT;
		tsens2_point = (calib_data[0] & TSENS_TORINO_POINT2) >>
				TSENS_TORINO_POINT2_SHIFT;
		tsens3_point = (calib_data[0] & TSENS_TORINO_POINT3) >>
				TSENS_TORINO_POINT3_SHIFT;
		tsens4_point = (calib_data[0] & TSENS_TORINO_POINT4) >>
				TSENS_TORINO_POINT4_SHIFT;
		calib_tsens_point_data[0] = tsens0_point;
		calib_tsens_point_data[1] = tsens1_point;
		calib_tsens_point_data[2] = tsens2_point;
		calib_tsens_point_data[3] = tsens3_point;
		calib_tsens_point_data[4] = tsens4_point;
	}

	if (calib_mode == 0) {
calibration_less_mode:
		pr_debug("TSENS is calibrationless mode\n");
		calib_tsens_point_data[0] = 532;
		calib_tsens_point_data[1] = 532;
		calib_tsens_point_data[2] = 532;
		calib_tsens_point_data[3] = 532;
		calib_tsens_point_data[4] = 532;
		goto compute_intercept_slope;
	}

compute_intercept_slope:
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0, adc_code_of_tempx = 0;
		tmdev->sensor[i].calib_data_point2 = tsens_base1_data;
		tmdev->sensor[i].calib_data_point1 = tsens_base0_data;
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
			i, tmdev->sensor[i].calib_data_point1,
			tmdev->sensor[i].calib_data_point2);
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
				temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
					tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		adc_code_of_tempx =
				tsens_base0_data + calib_tsens_point_data[i];
		pr_debug("offset_adc_code_of_tempx:0x%x\n",
						adc_code_of_tempx);
		tmdev->sensor[i].offset = (adc_code_of_tempx *
			tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		pr_debug("offset:%d and slope:%d\n", tmdev->sensor[i].offset,
				tmdev->sensor[i].slope_mul_tsens_factor);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int tsens_calib_8994_sensors(void)
{
	int i, tsens_base0_data = 0, tsens0_point = 0, tsens1_point = 0;
	int tsens2_point = 0, tsens3_point = 0, tsens4_point = 0;
	int tsens5_point = 0, tsens6_point = 0, tsens7_point = 0;
	int tsens8_point = 0, tsens9_point = 0, tsens10_point = 0;
	int tsens11_point = 0, tsens12_point = 0, tsens13_point = 0;
	int tsens14_point = 0, tsens15_point = 0;
	int tsens_base1_data = 0, calib_mode = 0;
	uint32_t calib_data[6], calib_tsens_point_data[16], calib_redun_sel;

	if (tmdev->calibration_less_mode)
		goto calibration_less_mode;

	calib_redun_sel = readl_relaxed(
			TSENS_8994_EEPROM_REDUN_SEL(tmdev->tsens_calib_addr));
	calib_redun_sel = calib_redun_sel & TSENS_8994_CAL_SEL_REDUN_MASK;
	calib_redun_sel >>= TSENS_8994_CAL_SEL_REDUN_SHIFT;

	if (calib_redun_sel == TSENS_QFPROM_BACKUP_SEL) {
		calib_data[0] = readl_relaxed(
			TSENS_REDUN_REGION1_EEPROM(tmdev->tsens_calib_addr));
		calib_data[1] = readl_relaxed(
			TSENS_REDUN_REGION2_EEPROM(tmdev->tsens_calib_addr));
		calib_data[2] = readl_relaxed(
			TSENS_REDUN_REGION3_EEPROM(tmdev->tsens_calib_addr));
		calib_data[3] = readl_relaxed(
			TSENS_REDUN_REGION4_EEPROM(tmdev->tsens_calib_addr));
		calib_data[4] = readl_relaxed(
			TSENS_REDUN_REGION5_EEPROM(tmdev->tsens_calib_addr));

		calib_mode = (calib_data[4] & TSENS_8994_REDUN_SEL_MASK);
		pr_debug("calib mode is %d\n", calib_mode);
		if (calib_mode == TSENS_TWO_POINT_CALIB) {
			tsens_base0_data = (calib_data[0] &
				TSENS_BASE0_8994_REDUN_MASK) >>
				TSENS_BASE0_8994_REDUN_MASK_SHIFT;
			tsens_base1_data = (calib_data[0] &
				TSENS_BASE1_BIT0_8994_REDUN_MASK) >>
				TSENS_BASE1_BIT0_SHIFT_COMPUTE;
			tsens_base1_data |= (calib_data[1] &
				TSENS_BASE1_BIT1_9_8994_REDUN_MASK);
			tsens0_point = (calib_data[1] &
				TSENS0_OFFSET_8994_REDUN_MASK) >>
				TSENS0_OFFSET_8994_REDUN_SHIFT;
			tsens1_point = (calib_data[1] &
				TSENS1_OFFSET_8994_REDUN_MASK) >>
				TSENS1_OFFSET_8994_REDUN_SHIFT;
			tsens2_point = (calib_data[1] &
				TSENS2_OFFSET_8994_REDUN_MASK) >>
				TSENS2_OFFSET_8994_REDUN_SHIFT;
			tsens3_point = (calib_data[1] &
				TSENS3_OFFSET_8994_REDUN_MASK) >>
				TSENS3_OFFSET_8994_REDUN_SHIFT;
			tsens4_point = (calib_data[1] &
				TSENS4_OFFSET_8994_REDUN_MASK) >>
				TSENS4_OFFSET_8994_REDUN_SHIFT;
			tsens5_point = (calib_data[1] &
				TSENS5_OFFSET_8994_REDUN_MASK_BIT0_2) >>
				TSENS5_OFFSET_8994_REDUN_SHIFT_BIT0_2;
			tsens5_point |= ((calib_data[2] &
				TSENS5_OFFSET_8994_REDUN_MASK_BIT3) >>
				TSENS5_OFFSET_8994_REDUN_SHIFT_BIT3);
			tsens6_point = (calib_data[2] &
				TSENS6_OFFSET_8994_REDUN_MASK) >>
				TSENS6_OFFSET_8994_REDUN_SHIFT;
			tsens7_point = (calib_data[2] &
				TSENS7_OFFSET_8994_REDUN_MASK) >>
				TSENS7_OFFSET_8994_REDUN_SHIFT;
			tsens8_point = (calib_data[3] &
				TSENS8_OFFSET_8994_REDUN_MASK);
			tsens9_point = (calib_data[3] &
				TSENS9_OFFSET_8994_REDUN_MASK) >>
				TSENS9_OFFSET_8994_REDUN_SHIFT;
			tsens10_point = (calib_data[3] &
				TSENS10_OFFSET_8994_REDUN_MASK) >>
				TSENS10_OFFSET_8994_REDUN_SHIFT;
			tsens11_point = (calib_data[3] &
				TSENS11_OFFSET_8994_REDUN_MASK) >>
				TSENS11_OFFSET_8994_REDUN_SHIFT;
			tsens12_point = (calib_data[3] &
				TSENS12_OFFSET_8994_REDUN_MASK) >>
				TSENS12_OFFSET_8994_REDUN_SHIFT;
			tsens13_point = (calib_data[3] &
				TSENS13_OFFSET_8994_REDUN_MASK) >>
				TSENS13_OFFSET_8994_REDUN_SHIFT;
			tsens14_point = (calib_data[3] &
				TSENS14_OFFSET_8994_REDUN_MASK) >>
				TSENS14_OFFSET_8994_REDUN_SHIFT;
			tsens15_point = (calib_data[3] &
				TSENS15_OFFSET_8994_REDUN_MASK) >>
				TSENS15_OFFSET_8994_REDUN_SHIFT;
			calib_tsens_point_data[0] = tsens0_point;
			calib_tsens_point_data[1] = tsens1_point;
			calib_tsens_point_data[2] = tsens2_point;
			calib_tsens_point_data[3] = tsens3_point;
			calib_tsens_point_data[4] = tsens4_point;
			calib_tsens_point_data[5] = tsens5_point;
			calib_tsens_point_data[6] = tsens6_point;
			calib_tsens_point_data[7] = tsens7_point;
			calib_tsens_point_data[8] = tsens8_point;
			calib_tsens_point_data[9] = tsens9_point;
			calib_tsens_point_data[10] = tsens10_point;
			calib_tsens_point_data[11] = tsens11_point;
			calib_tsens_point_data[12] = tsens12_point;
			calib_tsens_point_data[13] = tsens13_point;
			calib_tsens_point_data[14] = tsens14_point;
			calib_tsens_point_data[15] = tsens15_point;
		} else
			goto calibration_less_mode;
	} else {
		calib_data[0] = readl_relaxed(
			TSENS_8994_EEPROM(tmdev->tsens_calib_addr));
		calib_data[1] = readl_relaxed(
			(TSENS_8994_EEPROM(tmdev->tsens_calib_addr) + 0x4));
		calib_data[2] = readl_relaxed(
			(TSENS_8994_EEPROM(tmdev->tsens_calib_addr) + 0x8));

		calib_mode = (calib_data[2] & TSENS_8994_CAL_SEL_MASK) >>
				TSENS_8994_CAL_SEL_SHIFT;
		pr_debug("calib mode is %d\n", calib_mode);
		if (calib_mode == TSENS_TWO_POINT_CALIB) {
			tsens_base0_data = (calib_data[0] &
				TSENS_BASE0_8994_MASK);
			tsens_base1_data = (calib_data[0] &
				TSENS_BASE1_8994_MASK) >>
				TSENS_BASE1_8994_SHIFT;
			tsens0_point = (calib_data[0] &
				TSENS0_OFFSET_8994_MASK) >>
				TSENS0_OFFSET_8994_SHIFT;
			tsens1_point = (calib_data[0] &
				TSENS1_OFFSET_8994_MASK) >>
				TSENS1_OFFSET_8994_SHIFT;
			tsens2_point = (calib_data[0] &
				TSENS2_OFFSET_8994_MASK) >>
				TSENS2_OFFSET_8994_SHIFT;
			tsens3_point = (calib_data[1] &
				TSENS3_OFFSET_8994_MASK);
			tsens4_point = (calib_data[1] &
				TSENS4_OFFSET_8994_MASK) >>
				TSENS4_OFFSET_8994_SHIFT;
			tsens5_point = (calib_data[1] &
				TSENS5_OFFSET_8994_MASK) >>
				TSENS5_OFFSET_8994_SHIFT;
			tsens6_point = (calib_data[1] &
				TSENS6_OFFSET_8994_MASK) >>
				TSENS6_OFFSET_8994_SHIFT;
			tsens7_point = (calib_data[1] &
				TSENS7_OFFSET_8994_MASK) >>
				TSENS7_OFFSET_8994_SHIFT;
			tsens8_point = (calib_data[1] &
				TSENS8_OFFSET_8994_MASK) >>
				TSENS8_OFFSET_8994_SHIFT;
			tsens9_point = (calib_data[1] &
				TSENS9_OFFSET_8994_MASK) >>
				TSENS9_OFFSET_8994_SHIFT;
			tsens10_point = (calib_data[1] &
				TSENS10_OFFSET_8994_MASK) >>
				TSENS10_OFFSET_8994_SHIFT;
			tsens11_point = (calib_data[2] &
				TSENS11_OFFSET_8994_MASK);
			tsens12_point = (calib_data[2] &
				TSENS12_OFFSET_8994_MASK) >>
				TSENS12_OFFSET_8994_SHIFT;
			tsens13_point = (calib_data[2] &
				TSENS13_OFFSET_8994_MASK) >>
				TSENS13_OFFSET_8994_SHIFT;
			tsens14_point = (calib_data[2] &
				TSENS14_OFFSET_8994_MASK) >>
				TSENS14_OFFSET_8994_SHIFT;
			tsens15_point = (calib_data[2] &
				TSENS15_OFFSET_8994_MASK) >>
				TSENS15_OFFSET_8994_SHIFT;
			calib_tsens_point_data[0] = tsens0_point;
			calib_tsens_point_data[1] = tsens1_point;
			calib_tsens_point_data[2] = tsens2_point;
			calib_tsens_point_data[3] = tsens3_point;
			calib_tsens_point_data[4] = tsens4_point;
			calib_tsens_point_data[5] = tsens5_point;
			calib_tsens_point_data[6] = tsens6_point;
			calib_tsens_point_data[7] = tsens7_point;
			calib_tsens_point_data[8] = tsens8_point;
			calib_tsens_point_data[9] = tsens9_point;
			calib_tsens_point_data[10] = tsens10_point;
			calib_tsens_point_data[11] = tsens11_point;
			calib_tsens_point_data[12] = tsens12_point;
			calib_tsens_point_data[13] = tsens13_point;
			calib_tsens_point_data[14] = tsens14_point;
			calib_tsens_point_data[15] = tsens15_point;
		} else {
calibration_less_mode:
			pr_debug("TSENS is calibrationless mode\n");
			calib_tsens_point_data[0] = 532;
			calib_tsens_point_data[1] = 532;
			calib_tsens_point_data[2] = 532;
			calib_tsens_point_data[3] = 532;
			calib_tsens_point_data[4] = 532;
			calib_tsens_point_data[5] = 532;
			calib_tsens_point_data[6] = 532;
			calib_tsens_point_data[7] = 532;
			calib_tsens_point_data[8] = 532;
			calib_tsens_point_data[9] = 532;
			calib_tsens_point_data[10] = 532;
			calib_tsens_point_data[11] = 532;
			calib_tsens_point_data[12] = 532;
			calib_tsens_point_data[13] = 532;
			calib_tsens_point_data[14] = 532;
			calib_tsens_point_data[15] = 532;
		}
	}

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0, adc_code_of_tempx = 0;
		tmdev->sensor[i].calib_data_point2 = tsens_base1_data;
		tmdev->sensor[i].calib_data_point1 = tsens_base0_data;
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
			i, tmdev->sensor[i].calib_data_point1,
			tmdev->sensor[i].calib_data_point2);
		if (calib_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
				temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
					tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		adc_code_of_tempx =
				tsens_base0_data + calib_tsens_point_data[i];
		pr_debug("offset_adc_code_of_tempx:0x%x\n",
						adc_code_of_tempx);
		tmdev->sensor[i].offset = (adc_code_of_tempx *
			tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		pr_debug("offset:%d and slope:%d\n", tmdev->sensor[i].offset,
				tmdev->sensor[i].slope_mul_tsens_factor);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int tsens_calib_8x10_sensors(void)
{
	int i, tsens_base0_data = 0, tsens0_point1 = 0, tsens1_point1 = 0;
	int tsens0_point2 = 0, tsens1_point2 = 0;
	int tsens_base1_data = 0, tsens_calibration_mode = 0;
	uint32_t calib_data[2], calib_redun_sel;
	uint32_t calib_tsens_point1_data[2], calib_tsens_point2_data[2];

	if (tmdev->calibration_less_mode)
		goto calibration_less_mode;

	calib_redun_sel = readl_relaxed(
			TSENS_EEPROM_8X10_2(tmdev->tsens_calib_addr));
	calib_redun_sel = calib_redun_sel & TSENS_8X10_REDUN_SEL_MASK;
	calib_redun_sel >>= TSENS_8X10_REDUN_SEL_SHIFT;
	pr_debug("calib_redun_sel:%x\n", calib_redun_sel);

	if (calib_redun_sel == TSENS_QFPROM_BACKUP_SEL) {
		calib_data[0] = readl_relaxed(
			TSENS_EEPROM_8X10_SPARE_1(tmdev->tsens_calib_addr));
		calib_data[1] = readl_relaxed(
			TSENS_EEPROM_8X10_SPARE_2(tmdev->tsens_calib_addr));
	} else {
		calib_data[0] = readl_relaxed(
			TSENS_EEPROM_8X10_1(tmdev->tsens_calib_addr));
		calib_data[1] = readl_relaxed(
			(TSENS_EEPROM_8X10_1(tmdev->tsens_calib_addr) +
					TSENS_EEPROM_8X10_1_OFFSET));
	}

	tsens_calibration_mode = (calib_data[0] & TSENS_8X10_TSENS_CAL_SEL)
			>> TSENS_8X10_CAL_SEL_SHIFT;
	pr_debug("calib mode scheme:%x\n", tsens_calibration_mode);

	if ((tsens_calibration_mode == TSENS_TWO_POINT_CALIB) ||
		(tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2)) {
		tsens_base0_data = (calib_data[0] & TSENS_8X10_BASE0_MASK);
		tsens0_point1 = (calib_data[0] & TSENS0_8X10_POINT1_MASK) >>
				TSENS0_8X10_POINT1_SHIFT;
		tsens1_point1 = calib_data[1] & TSENS1_8X10_POINT1_MASK;
	} else
		goto calibration_less_mode;

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		tsens_base1_data = (calib_data[0] & TSENS_8X10_BASE1_MASK) >>
				TSENS_8X10_BASE1_SHIFT;
		tsens0_point2 = (calib_data[0] & TSENS0_8X10_POINT2_MASK) >>
				TSENS0_8X10_POINT2_SHIFT;
		tsens1_point2 = (calib_data[1] & TSENS1_8X10_POINT2_MASK) >>
				TSENS1_8X10_POINT2_SHIFT;
	}

	if (tsens_calibration_mode == 0) {
calibration_less_mode:
		pr_debug("TSENS is calibrationless mode\n");
		for (i = 0; i < tmdev->tsens_num_sensor; i++)
			calib_tsens_point2_data[i] = 780;
		calib_tsens_point1_data[0] = 595;
		calib_tsens_point1_data[1] = 629;
		goto compute_intercept_slope;
	}

	if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		calib_tsens_point1_data[0] =
			((((tsens_base0_data) + tsens0_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[1] =
			((((tsens_base0_data) + tsens1_point1) << 2) |
						TSENS_BIT_APPEND);
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		pr_debug("two point calibration calculation\n");
		calib_tsens_point2_data[0] =
			(((tsens_base1_data + tsens0_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[1] =
			(((tsens_base1_data + tsens1_point2) << 2) |
					TSENS_BIT_APPEND);
	}

compute_intercept_slope:
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0;
		tmdev->sensor[i].calib_data_point2 = calib_tsens_point2_data[i];
		tmdev->sensor[i].calib_data_point1 = calib_tsens_point1_data[i];
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
			i, tmdev->sensor[i].calib_data_point1,
			tmdev->sensor[i].calib_data_point2);
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
				temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
					tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		tmdev->sensor[i].offset = (tmdev->sensor[i].calib_data_point1 *
			tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int tsens_calib_8x26_sensors(void)
{
	int i, tsens_base0_data = 0, tsens0_point1 = 0, tsens1_point1 = 0;
	int tsens2_point1 = 0, tsens3_point1 = 0, tsens4_point1 = 0;
	int tsens5_point1 = 0, tsens6_point1 = 0, tsens6_point2 = 0;
	int tsens0_point2 = 0, tsens1_point2 = 0, tsens2_point2 = 0;
	int tsens3_point2 = 0, tsens4_point2 = 0, tsens5_point2 = 0;
	int tsens_base1_data = 0, tsens_calibration_mode = 0;
	uint32_t calib_data[6];
	uint32_t calib_tsens_point1_data[7], calib_tsens_point2_data[7];

	if (tmdev->calibration_less_mode)
		goto calibration_less_mode;

	for (i = 0; i < TSENS_8X26_MAIN_CALIB_ADDR_RANGE; i++)
		calib_data[i] = readl_relaxed(
			(TSENS_EEPROM_8X26_1(tmdev->tsens_calib_addr))
					+ (i * TSENS_SN_ADDR_OFFSET));
	calib_data[4] = readl_relaxed(
			(TSENS_EEPROM_8X26_2(tmdev->tsens_calib_addr)));
	calib_data[5] = readl_relaxed(
			(TSENS_EEPROM_8X26_2(tmdev->tsens_calib_addr)) + 0x8);

	tsens_calibration_mode = (calib_data[5] & TSENS_8X26_TSENS_CAL_SEL)
			>> TSENS_8X26_CAL_SEL_SHIFT;
	pr_debug("calib mode scheme:%x\n", tsens_calibration_mode);

	if ((tsens_calibration_mode == TSENS_TWO_POINT_CALIB) ||
		(tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2)) {
		tsens_base0_data = (calib_data[0] & TSENS_8X26_BASE0_MASK)
				>> TSENS_8X26_BASE0_SHIFT;
		tsens0_point1 = (calib_data[0] & TSENS0_8X26_POINT1_MASK) >>
				TSENS0_8X26_POINT1_SHIFT;
		tsens1_point1 = calib_data[1] & TSENS1_8X26_POINT1_MASK;
		tsens2_point1 = (calib_data[1] & TSENS2_8X26_POINT1_MASK) >>
				TSENS2_8X26_POINT1_SHIFT;
		tsens3_point1 = (calib_data[1] & TSENS3_8X26_POINT1_MASK) >>
				TSENS3_8X26_POINT1_SHIFT;
		tsens4_point1 = (calib_data[1] & TSENS4_8X26_POINT1_MASK) >>
				TSENS4_8X26_POINT1_SHIFT;
		tsens5_point1 = (calib_data[1] & TSENS5_8X26_POINT1_MASK) >>
				TSENS5_8X26_POINT1_SHIFT;
		tsens6_point1 = (calib_data[2] & TSENS6_8X26_POINT1_MASK) >>
				TSENS6_8X26_POINT1_SHIFT;
	} else
		goto calibration_less_mode;

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		tsens_base1_data = (calib_data[3] & TSENS_8X26_BASE1_MASK);
		tsens0_point2 = (calib_data[3] & TSENS0_8X26_POINT2_MASK) >>
				TSENS0_8X26_POINT2_SHIFT;
		tsens1_point2 = (calib_data[3] & TSENS1_8X26_POINT2_MASK) >>
				TSENS1_8X26_POINT2_SHIFT;
		tsens2_point2 = (calib_data[3] & TSENS2_8X26_POINT2_MASK) >>
				TSENS2_8X26_POINT2_SHIFT;
		tsens3_point2 = (calib_data[3] & TSENS3_8X26_POINT2_MASK) >>
				TSENS3_8X26_POINT2_SHIFT;
		tsens4_point2 = (calib_data[4] & TSENS4_8X26_POINT2_MASK) >>
				TSENS4_8X26_POINT2_SHIFT;
		tsens5_point2 = (calib_data[4] & TSENS5_8X26_POINT2_MASK) >>
				TSENS5_8X26_POINT2_SHIFT;
		tsens6_point2 = (calib_data[5] & TSENS6_8X26_POINT2_MASK) >>
				TSENS6_8X26_POINT2_SHIFT;
	}

	if (tsens_calibration_mode == 0) {
calibration_less_mode:
		pr_debug("TSENS is calibrationless mode\n");
		for (i = 0; i < tmdev->tsens_num_sensor; i++)
			calib_tsens_point2_data[i] = 780;
		calib_tsens_point1_data[0] = 595;
		calib_tsens_point1_data[1] = 625;
		calib_tsens_point1_data[2] = 553;
		calib_tsens_point1_data[3] = 578;
		calib_tsens_point1_data[4] = 505;
		calib_tsens_point1_data[5] = 509;
		calib_tsens_point1_data[6] = 507;
		goto compute_intercept_slope;
	}

	if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		calib_tsens_point1_data[0] =
			((((tsens_base0_data) + tsens0_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[1] =
			((((tsens_base0_data) + tsens1_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[2] =
			((((tsens_base0_data) + tsens2_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[3] =
			((((tsens_base0_data) + tsens3_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[4] =
			((((tsens_base0_data) + tsens4_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[5] =
			((((tsens_base0_data) + tsens5_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[6] =
			((((tsens_base0_data) + tsens6_point1) << 2) |
						TSENS_BIT_APPEND);
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		pr_debug("two point calibration calculation\n");
		calib_tsens_point2_data[0] =
			(((tsens_base1_data + tsens0_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[1] =
			(((tsens_base1_data + tsens1_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[2] =
			(((tsens_base1_data + tsens2_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[3] =
			(((tsens_base1_data + tsens3_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[4] =
			(((tsens_base1_data + tsens4_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[5] =
			(((tsens_base1_data + tsens5_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[6] =
			(((tsens_base1_data + tsens6_point2) << 2) |
					TSENS_BIT_APPEND);
	}

compute_intercept_slope:
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0;
		tmdev->sensor[i].calib_data_point2 = calib_tsens_point2_data[i];
		tmdev->sensor[i].calib_data_point1 = calib_tsens_point1_data[i];
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
			i, tmdev->sensor[i].calib_data_point1,
			tmdev->sensor[i].calib_data_point2);
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
				temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
					tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		tmdev->sensor[i].offset = (tmdev->sensor[i].calib_data_point1 *
			tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int tsens_calib_8974_sensors(void)
{
	int i, tsens_base1_data = 0, tsens0_point1 = 0, tsens1_point1 = 0;
	int tsens2_point1 = 0, tsens3_point1 = 0, tsens4_point1 = 0;
	int tsens5_point1 = 0, tsens6_point1 = 0, tsens7_point1 = 0;
	int tsens8_point1 = 0, tsens9_point1 = 0, tsens10_point1 = 0;
	int tsens0_point2 = 0, tsens1_point2 = 0, tsens2_point2 = 0;
	int tsens3_point2 = 0, tsens4_point2 = 0, tsens5_point2 = 0;
	int tsens6_point2 = 0, tsens7_point2 = 0, tsens8_point2 = 0;
	int tsens9_point2 = 0, tsens10_point2 = 0;
	int tsens_base2_data = 0, tsens_calibration_mode = 0, temp = 0;
	uint32_t calib_data[6], calib_redun_sel, calib_data_backup[4];
	uint32_t calib_tsens_point1_data[11], calib_tsens_point2_data[11];

	if (tmdev->calibration_less_mode)
		goto calibration_less_mode;

	calib_redun_sel = readl_relaxed(
			TSENS_EEPROM_REDUNDANCY_SEL(tmdev->tsens_calib_addr));
	calib_redun_sel = calib_redun_sel & TSENS_QFPROM_BACKUP_REDUN_SEL;
	calib_redun_sel >>= TSENS_QFPROM_BACKUP_REDUN_SHIFT;
	pr_debug("calib_redun_sel:%x\n", calib_redun_sel);

	for (i = 0; i < TSENS_MAIN_CALIB_ADDR_RANGE; i++) {
		calib_data[i] = readl_relaxed(
			(TSENS_EEPROM(tmdev->tsens_calib_addr))
					+ (i * TSENS_SN_ADDR_OFFSET));
		pr_debug("calib raw data row%d:0x%x\n", i, calib_data[i]);
	}

	if (calib_redun_sel == TSENS_QFPROM_BACKUP_SEL) {
		tsens_calibration_mode = (calib_data[4] & TSENS_CAL_SEL_0_1)
				>> TSENS_CAL_SEL_SHIFT;
		temp = (calib_data[5] & TSENS_CAL_SEL_2)
				>> TSENS_CAL_SEL_SHIFT_2;
		tsens_calibration_mode |= temp;
		pr_debug("backup calib mode:%x\n", calib_redun_sel);

		for (i = 0; i < TSENS_BACKUP_CALIB_ADDR_RANGE; i++)
			calib_data_backup[i] = readl_relaxed(
				(TSENS_EEPROM_BACKUP_REGION(
					tmdev->tsens_calib_addr))
				+ (i * TSENS_SN_ADDR_OFFSET));

		if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB)
				|| (tsens_calibration_mode ==
				TSENS_TWO_POINT_CALIB) ||
				(tsens_calibration_mode ==
				TSENS_ONE_POINT_CALIB_OPTION_2)) {
			tsens_base1_data = (calib_data_backup[0] &
						TSENS_BASE1_MASK);
			tsens0_point1 = (calib_data_backup[0] &
						TSENS0_POINT1_MASK) >>
						TSENS0_POINT1_SHIFT;
			tsens1_point1 = (calib_data_backup[0] &
				TSENS1_POINT1_MASK) >> TSENS1_POINT1_SHIFT;
			tsens2_point1 = (calib_data_backup[0] &
				TSENS2_POINT1_MASK) >> TSENS2_POINT1_SHIFT;
			tsens3_point1 = (calib_data_backup[0] &
				TSENS3_POINT1_MASK) >> TSENS3_POINT1_SHIFT;
			tsens4_point1 = (calib_data_backup[1] &
				TSENS4_POINT1_MASK);
			tsens5_point1 = (calib_data_backup[1] &
				TSENS5_POINT1_MASK) >> TSENS5_POINT1_SHIFT;
			tsens6_point1 = (calib_data_backup[1] &
				TSENS6_POINT1_MASK) >> TSENS6_POINT1_SHIFT;
			tsens7_point1 = (calib_data_backup[1] &
				TSENS7_POINT1_MASK) >> TSENS7_POINT1_SHIFT;
			tsens8_point1 = (calib_data_backup[2] &
						TSENS8_POINT1_MASK_BACKUP) >>
						TSENS8_POINT1_SHIFT;
			tsens9_point1 = (calib_data_backup[2] &
					TSENS9_POINT1_MASK_BACKUP) >>
					TSENS9_POINT1_BACKUP_SHIFT;
			tsens10_point1 = (calib_data_backup[2] &
				TSENS10_POINT1_MASK_BACKUP) >>
				TSENS10_POINT1_BACKUP_SHIFT;
		} else
			goto calibration_less_mode;

		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			tsens_base2_data = (calib_data_backup[2] &
				TSENS_BASE2_BACKUP_MASK) >>
				TSENS_POINT2_BASE_BACKUP_SHIFT;
			tsens0_point2 = (calib_data_backup[2] &
					TSENS0_POINT2_BACKUP_MASK) >>
					TSENS0_POINT2_BACKUP_SHIFT;
			tsens1_point2 = (calib_data_backup[3] &
					TSENS1_POINT2_BACKUP_MASK);
			tsens2_point2 = (calib_data_backup[3] &
					TSENS2_POINT2_BACKUP_MASK) >>
					TSENS2_POINT2_BACKUP_SHIFT;
			tsens3_point2 = (calib_data_backup[3] &
					TSENS3_POINT2_BACKUP_MASK) >>
					TSENS3_POINT2_BACKUP_SHIFT;
			tsens4_point2 = (calib_data_backup[3] &
					TSENS4_POINT2_BACKUP_MASK) >>
					TSENS4_POINT2_BACKUP_SHIFT;
			tsens5_point2 = (calib_data[4] &
					TSENS5_POINT2_BACKUP_MASK) >>
					TSENS5_POINT2_BACKUP_SHIFT;
			tsens6_point2 = (calib_data[5] &
					TSENS6_POINT2_BACKUP_MASK);
			tsens7_point2 = (calib_data[5] &
					TSENS7_POINT2_BACKUP_MASK) >>
					TSENS7_POINT2_BACKUP_SHIFT;
			tsens8_point2 = (calib_data[5] &
					TSENS8_POINT2_BACKUP_MASK) >>
					TSENS8_POINT2_BACKUP_SHIFT;
			tsens9_point2 = (calib_data[5] &
					TSENS9_POINT2_BACKUP_MASK) >>
					TSENS9_POINT2_BACKUP_SHIFT;
			tsens10_point2 = (calib_data[5] &
					TSENS10_POINT2_BACKUP_MASK)
					>> TSENS10_POINT2_BACKUP_SHIFT;
		}
	} else {
		tsens_calibration_mode = (calib_data[1] & TSENS_CAL_SEL_0_1)
			>> TSENS_CAL_SEL_SHIFT;
		temp = (calib_data[3] & TSENS_CAL_SEL_2)
			>> TSENS_CAL_SEL_SHIFT_2;
		tsens_calibration_mode |= temp;
		pr_debug("calib mode scheme:%x\n", tsens_calibration_mode);
		if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB) ||
			(tsens_calibration_mode ==
					TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
			tsens_base1_data = (calib_data[0] & TSENS_BASE1_MASK);
			tsens0_point1 = (calib_data[0] & TSENS0_POINT1_MASK) >>
							TSENS0_POINT1_SHIFT;
			tsens1_point1 = (calib_data[0] & TSENS1_POINT1_MASK) >>
							TSENS1_POINT1_SHIFT;
			tsens2_point1 = (calib_data[0] & TSENS2_POINT1_MASK) >>
							TSENS2_POINT1_SHIFT;
			tsens3_point1 = (calib_data[0] & TSENS3_POINT1_MASK) >>
							TSENS3_POINT1_SHIFT;
			tsens4_point1 = (calib_data[1] & TSENS4_POINT1_MASK);
			tsens5_point1 = (calib_data[1] & TSENS5_POINT1_MASK) >>
							TSENS5_POINT1_SHIFT;
			tsens6_point1 = (calib_data[1] & TSENS6_POINT1_MASK) >>
							TSENS6_POINT1_SHIFT;
			tsens7_point1 = (calib_data[1] & TSENS7_POINT1_MASK) >>
							TSENS7_POINT1_SHIFT;
			tsens8_point1 = (calib_data[1] & TSENS8_POINT1_MASK) >>
							TSENS8_POINT1_SHIFT;
			tsens9_point1 = (calib_data[2] & TSENS9_POINT1_MASK);
			tsens10_point1 = (calib_data[2] & TSENS10_POINT1_MASK)
							>> TSENS10_POINT1_SHIFT;
		} else
			goto calibration_less_mode;

		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			tsens_base2_data = (calib_data[2] & TSENS_BASE2_MASK) >>
						TSENS_POINT2_BASE_SHIFT;
			tsens0_point2 = (calib_data[2] & TSENS0_POINT2_MASK) >>
						TSENS0_POINT2_SHIFT;
			tsens1_point2 = (calib_data[2] & TSENS1_POINT2_MASK) >>
						TSENS1_POINT2_SHIFT;
			tsens2_point2 = (calib_data[3] & TSENS2_POINT2_MASK);
			tsens3_point2 = (calib_data[3] & TSENS3_POINT2_MASK) >>
						TSENS3_POINT2_SHIFT;
			tsens4_point2 = (calib_data[3] & TSENS4_POINT2_MASK) >>
						TSENS4_POINT2_SHIFT;
			tsens5_point2 = (calib_data[3] & TSENS5_POINT2_MASK) >>
						TSENS5_POINT2_SHIFT;
			tsens6_point2 = (calib_data[3] & TSENS6_POINT2_MASK) >>
						TSENS6_POINT2_SHIFT;
			tsens7_point2 = (calib_data[4] & TSENS7_POINT2_MASK);
			tsens8_point2 = (calib_data[4] & TSENS8_POINT2_MASK) >>
						TSENS8_POINT2_SHIFT;
			tsens9_point2 = (calib_data[4] & TSENS9_POINT2_MASK) >>
						TSENS9_POINT2_SHIFT;
			tsens10_point2 = (calib_data[4] & TSENS10_POINT2_MASK)
						>> TSENS10_POINT2_SHIFT;
		}

		if (tsens_calibration_mode == 0) {
calibration_less_mode:
			pr_debug("TSENS is calibrationless mode\n");
			for (i = 0; i < tmdev->tsens_num_sensor; i++)
				calib_tsens_point2_data[i] = 780;
			calib_tsens_point1_data[0] = 502;
			calib_tsens_point1_data[1] = 509;
			calib_tsens_point1_data[2] = 503;
			calib_tsens_point1_data[3] = 509;
			calib_tsens_point1_data[4] = 505;
			calib_tsens_point1_data[5] = 509;
			calib_tsens_point1_data[6] = 507;
			calib_tsens_point1_data[7] = 510;
			calib_tsens_point1_data[8] = 508;
			calib_tsens_point1_data[9] = 509;
			calib_tsens_point1_data[10] = 508;
			goto compute_intercept_slope;
		}
	}

	if (tsens_calibration_mode == TSENS_ONE_POINT_CALIB) {
		calib_tsens_point1_data[0] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens0_point1;
		calib_tsens_point1_data[1] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens1_point1;
		calib_tsens_point1_data[2] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens2_point1;
		calib_tsens_point1_data[3] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens3_point1;
		calib_tsens_point1_data[4] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens4_point1;
		calib_tsens_point1_data[5] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens5_point1;
		calib_tsens_point1_data[6] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens6_point1;
		calib_tsens_point1_data[7] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens7_point1;
		calib_tsens_point1_data[8] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens8_point1;
		calib_tsens_point1_data[9] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens9_point1;
		calib_tsens_point1_data[10] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens10_point1;
	}

	if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		pr_debug("one point calibration calculation\n");
		calib_tsens_point1_data[0] =
			((((tsens_base1_data) + tsens0_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[1] =
			((((tsens_base1_data) + tsens1_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[2] =
			((((tsens_base1_data) + tsens2_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[3] =
			((((tsens_base1_data) + tsens3_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[4] =
			((((tsens_base1_data) + tsens4_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[5] =
			((((tsens_base1_data) + tsens5_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[6] =
			((((tsens_base1_data) + tsens6_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[7] =
			((((tsens_base1_data) + tsens7_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[8] =
			((((tsens_base1_data) + tsens8_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[9] =
			((((tsens_base1_data) + tsens9_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[10] =
			((((tsens_base1_data) + tsens10_point1) << 2) |
						TSENS_BIT_APPEND);
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		pr_debug("two point calibration calculation\n");
		calib_tsens_point2_data[0] =
			(((tsens_base2_data + tsens0_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[1] =
			(((tsens_base2_data + tsens1_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[2] =
			(((tsens_base2_data + tsens2_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[3] =
			(((tsens_base2_data + tsens3_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[4] =
			(((tsens_base2_data + tsens4_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[5] =
			(((tsens_base2_data + tsens5_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[6] =
			(((tsens_base2_data + tsens6_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[7] =
			(((tsens_base2_data + tsens7_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[8] =
			(((tsens_base2_data + tsens8_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[9] =
			(((tsens_base2_data + tsens9_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[10] =
			(((tsens_base2_data + tsens10_point2) << 2) |
					TSENS_BIT_APPEND);
	}

compute_intercept_slope:
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0;
		tmdev->sensor[i].calib_data_point2 = calib_tsens_point2_data[i];
		tmdev->sensor[i].calib_data_point1 = calib_tsens_point1_data[i];
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
			i, tmdev->sensor[i].calib_data_point1,
			tmdev->sensor[i].calib_data_point2);
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
				temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
					tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		tmdev->sensor[i].offset = (tmdev->sensor[i].calib_data_point1 *
			tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		pr_debug("offset:%d\n", tmdev->sensor[i].offset);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int tsens_calib_9900_sensors(void)
{
	int i, tsens_base1_data = 0, tsens0_point1 = 0, tsens1_point1 = 0;
	int tsens2_point1 = 0, tsens3_point1 = 0, tsens4_point1 = 0;
	int tsens5_point1 = 0, tsens6_point1 = 0, tsens0_point2 = 0;
	int tsens1_point2 = 0, tsens2_point2 = 0, tsens3_point2 = 0;
	int tsens4_point2 = 0, tsens5_point2 = 0, tsens6_point2 = 0;
	int tsens_base2_data = 0, tsens_calibration_mode = 0;
	uint32_t calib_data[4], calib_redun_sel, calib_data_backup[4];
	uint32_t calib_tsens_point1_data[7], calib_tsens_point2_data[7];

	if (tmdev->calibration_less_mode)
		goto calibration_less_mode;

	calib_redun_sel = readl_relaxed(
		TSENS_9900_EEPROM_REDUNDANCY_SEL(tmdev->tsens_calib_addr));
	calib_redun_sel = calib_redun_sel & TSENS_QFPROM_BACKUP_9900_REDUN_SEL;
	calib_redun_sel >>= TSENS_QFPROM_BACKUP_9900_REDUN_SHIFT;
	pr_debug("calib_redun_sel:%x\n", calib_redun_sel);

	if (calib_redun_sel == TSENS_QFPROM_BACKUP_SEL) {
		for (i = 0; i < TSENS_9900_CALIB_ADDR_RANGE; i++) {
			calib_data_backup[i] = readl_relaxed(
				(TSENS_9900_EEPROM_BACKUP_REGION(
					tmdev->tsens_calib_addr))
				+ (i * TSENS_SN_ADDR_OFFSET));
		pr_debug("backup calib raw data row%d:0x%x\n",
			i, calib_data_backup[i]);
		}

		tsens_calibration_mode = (calib_data_backup[0] &
			TSENS_9900_TSENS_CAL_SEL) >> TSENS_9900_CAL_SEL_SHIFT;
		pr_debug("backup calib mode:%x\n", tsens_calibration_mode);

		if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB)
				|| (tsens_calibration_mode ==
				TSENS_TWO_POINT_CALIB) ||
				(tsens_calibration_mode ==
				TSENS_ONE_POINT_CALIB_OPTION_2)) {
			tsens_base1_data = (calib_data_backup[0] &
				TSENS_9900_BASE1_MASK);
			tsens0_point1 = (calib_data_backup[0] &
				TSENS0_9900_POINT1_MASK) >>
				TSENS0_9900_POINT1_SHIFT;
			tsens1_point1 = (calib_data_backup[1] &
				TSENS1_9900_POINT1_MASK);
			tsens2_point1 = (calib_data_backup[1] &
				TSENS2_9900_POINT1_MASK) >>
				TSENS2_9900_POINT1_SHIFT;
			tsens3_point1 = (calib_data_backup[1] &
				TSENS3_9900_POINT1_MASK) >>
				TSENS3_9900_POINT1_SHIFT;
			tsens4_point1 = (calib_data_backup[2] &
				TSENS4_9900_POINT1_MASK) >>
				TSENS4_9900_POINT1_SHIFT;
			tsens5_point1 = (calib_data_backup[2] &
				TSENS5_9900_POINT1_MASK) >>
				TSENS5_9900_POINT1_SHIFT;
			tsens6_point1 = (calib_data_backup[3] &
				TSENS6_9900_POINT1_MASK);
		} else
			goto calibration_less_mode;

		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			tsens_base2_data = (calib_data_backup[0] &
				TSENS_9900_BASE2_MASK) >>
				TSENS_9900_BASE2_SHIFT;
			tsens0_point2 = (calib_data_backup[0] &
				TSENS0_9900_POINT2_MASK) >>
				TSENS0_9900_POINT2_SHIFT;
			tsens1_point2 = (calib_data_backup[1] &
				TSENS1_9900_POINT2_MASK)>>
				TSENS1_9900_POINT2_SHIFT;
			tsens2_point2 = (calib_data_backup[1] &
				TSENS2_9900_POINT2_MASK) >>
				TSENS2_9900_POINT2_SHIFT;
			tsens3_point2 = (calib_data_backup[2] &
				TSENS3_9900_POINT2_MASK);
			tsens4_point2 = (calib_data_backup[2] &
				TSENS4_9900_POINT2_MASK) >>
				TSENS4_9900_POINT2_SHIFT;
			tsens5_point2 = (calib_data_backup[2] &
				TSENS5_9900_POINT2_MASK) >>
				TSENS5_9900_POINT2_SHIFT;
			tsens6_point2 = (calib_data_backup[3] &
				TSENS6_9900_POINT2_MASK) >>
				TSENS6_9900_POINT2_SHIFT;
		}
	} else {
		for (i = 0; i < TSENS_9900_CALIB_ADDR_RANGE; i++) {
			calib_data[i] = readl_relaxed(
				(TSENS_9900_EEPROM(tmdev->tsens_calib_addr))
					+ (i * TSENS_SN_ADDR_OFFSET));
		pr_debug("calib raw data row%d:0x%x\n", i , calib_data[i]);
		}

		tsens_calibration_mode = (calib_data[0] &
			TSENS_9900_TSENS_CAL_SEL) >> TSENS_9900_CAL_SEL_SHIFT;
		pr_debug("calib mode:%x\n", tsens_calibration_mode);

		if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB) ||
			(tsens_calibration_mode ==
					TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
			tsens_base1_data = (calib_data[0] &
				TSENS_9900_BASE1_MASK);
			tsens0_point1 = (calib_data[0] &
				TSENS0_9900_POINT1_MASK) >>
				TSENS0_9900_POINT1_SHIFT;
			tsens1_point1 = (calib_data[1] &
				TSENS1_9900_POINT1_MASK);
			tsens2_point1 = (calib_data[1] &
				TSENS2_9900_POINT1_MASK) >>
				TSENS2_9900_POINT1_SHIFT;
			tsens3_point1 = (calib_data[1] &
				TSENS3_9900_POINT1_MASK) >>
				TSENS3_9900_POINT1_SHIFT;
			tsens4_point1 = (calib_data[2] &
				TSENS4_9900_POINT1_MASK) >>
				TSENS4_9900_POINT1_SHIFT;
			tsens5_point1 = (calib_data[2] &
				TSENS5_9900_POINT1_MASK) >>
				TSENS5_9900_POINT1_SHIFT;
			tsens6_point1 = (calib_data[3] &
				TSENS6_9900_POINT1_MASK);
		} else
			goto calibration_less_mode;

		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			tsens_base2_data = (calib_data[0] &
				TSENS_9900_BASE2_MASK) >>
				TSENS_9900_BASE2_SHIFT;
			tsens0_point2 = (calib_data[0] &
				TSENS0_9900_POINT2_MASK) >>
				TSENS0_9900_POINT2_SHIFT;
			tsens1_point2 = (calib_data[1] &
				TSENS1_9900_POINT2_MASK) >>
				TSENS1_9900_POINT2_SHIFT;
			tsens2_point2 = (calib_data[1] &
				TSENS2_9900_POINT2_MASK)>>
				TSENS2_9900_POINT2_SHIFT;
			tsens3_point2 = (calib_data[2] &
				TSENS3_9900_POINT2_MASK);
			tsens4_point2 = (calib_data[2] &
				TSENS4_9900_POINT2_MASK) >>
				TSENS4_9900_POINT2_SHIFT;
			tsens5_point2 = (calib_data[2] &
				TSENS5_9900_POINT2_MASK) >>
				TSENS5_9900_POINT2_SHIFT;
			tsens6_point2 = (calib_data[3] &
				TSENS6_9900_POINT2_MASK) >>
				TSENS6_9900_POINT2_SHIFT;
		}

		if (tsens_calibration_mode == 0) {
calibration_less_mode:
			pr_debug("TSENS is calibrationless mode\n");
			for (i = 0; i < tmdev->tsens_num_sensor; i++)
				calib_tsens_point2_data[i] = 780;
			calib_tsens_point1_data[0] = 502;
			calib_tsens_point1_data[1] = 509;
			calib_tsens_point1_data[2] = 503;
			calib_tsens_point1_data[3] = 509;
			calib_tsens_point1_data[4] = 505;
			calib_tsens_point1_data[5] = 509;
			calib_tsens_point1_data[6] = 507;
			goto compute_intercept_slope;
		}
	}

	if (tsens_calibration_mode == TSENS_ONE_POINT_CALIB) {
		calib_tsens_point1_data[0] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens0_point1;
		calib_tsens_point1_data[1] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens1_point1;
		calib_tsens_point1_data[2] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens2_point1;
		calib_tsens_point1_data[3] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens3_point1;
		calib_tsens_point1_data[4] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens4_point1;
		calib_tsens_point1_data[5] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens5_point1;
		calib_tsens_point1_data[6] =
			(((tsens_base1_data) << 2) | TSENS_BIT_APPEND)
							+ tsens6_point1;
	}

	if ((tsens_calibration_mode == TSENS_ONE_POINT_CALIB_OPTION_2) ||
			(tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		pr_debug("one point calibration calculation\n");
		calib_tsens_point1_data[0] =
			((((tsens_base1_data) + tsens0_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[1] =
			((((tsens_base1_data) + tsens1_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[2] =
			((((tsens_base1_data) + tsens2_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[3] =
			((((tsens_base1_data) + tsens3_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[4] =
			((((tsens_base1_data) + tsens4_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[5] =
			((((tsens_base1_data) + tsens5_point1) << 2) |
						TSENS_BIT_APPEND);
		calib_tsens_point1_data[6] =
			((((tsens_base1_data) + tsens6_point1) << 2) |
						TSENS_BIT_APPEND);
	}

	if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
		pr_debug("two point calibration calculation\n");
		calib_tsens_point2_data[0] =
			(((tsens_base2_data + tsens0_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[1] =
			(((tsens_base2_data + tsens1_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[2] =
			(((tsens_base2_data + tsens2_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[3] =
			(((tsens_base2_data + tsens3_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[4] =
			(((tsens_base2_data + tsens4_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[5] =
			(((tsens_base2_data + tsens5_point2) << 2) |
					TSENS_BIT_APPEND);
		calib_tsens_point2_data[6] =
			(((tsens_base2_data + tsens6_point2) << 2) |
					TSENS_BIT_APPEND);
	}

compute_intercept_slope:
	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0;
		tmdev->sensor[i].calib_data_point2 = calib_tsens_point2_data[i];
		tmdev->sensor[i].calib_data_point1 = calib_tsens_point1_data[i];
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
			i, tmdev->sensor[i].calib_data_point1,
			tmdev->sensor[i].calib_data_point2);
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
				temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
					tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		tmdev->sensor[i].offset = (tmdev->sensor[i].calib_data_point1 *
			tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		pr_debug("offset:%d\n", tmdev->sensor[i].offset);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}

static int tsens_calib_msmzirc_sensors(void)
{
	int i = 0, tsens_base0_data = 0, tsens_base1_data = 0;
	int tsens0_point = 0, tsens1_point = 0, tsens2_point = 0;
	int tsens3_point = 0, tsens4_point = 0;
	int tsens_calibration_mode = 0;
	uint32_t calib_data[2] = {0, 0};
	uint32_t calib_tsens_point_data[5];

	if (!tmdev->calibration_less_mode) {
		calib_data[0] = readl_relaxed(
			TSENS_EEPROM(tmdev->tsens_calib_addr));
		calib_data[1] = readl_relaxed(
			(TSENS_EEPROM(tmdev->tsens_calib_addr) + 0x4));

		tsens_calibration_mode =
			(calib_data[1] & TSENS_ZIRC_CAL_SEL) >>
				TSENS_ZIRC_CAL_SEL_SHIFT;
		pr_debug("calib mode is %d\n", tsens_calibration_mode);
	}

	if ((tsens_calibration_mode == TSENS_TWO_POINT_CALIB)) {
		tsens_base0_data = (calib_data[0] & TSENS_BASE0_ZIRC_MASK);
		tsens_base1_data = (calib_data[0] & TSENS_BASE1_ZIRC_MASK) >>
						TSENS_BASE1_ZIRC_SHIFT;
		tsens0_point = (calib_data[0] & TSENS0_OFFSET_ZIRC_MASK) >>
						TSENS0_OFFSET_ZIRC_SHIFT;
		tsens1_point = (calib_data[0] & TSENS1_OFFSET_ZIRC_MASK) >>
						TSENS1_OFFSET_ZIRC_SHIFT;
		tsens2_point = (calib_data[0] & TSENS2_OFFSET_ZIRC_MASK) >>
						TSENS2_OFFSET_ZIRC_SHIFT;
		tsens3_point = (calib_data[1] & TSENS3_OFFSET_ZIRC_MASK);
		tsens4_point = (calib_data[1] & TSENS4_OFFSET_ZIRC_MASK) >>
						TSENS4_OFFSET_ZIRC_SHIFT;
		calib_tsens_point_data[0] = tsens0_point;
		calib_tsens_point_data[1] = tsens1_point;
		calib_tsens_point_data[2] = tsens2_point;
		calib_tsens_point_data[3] = tsens3_point;
		calib_tsens_point_data[4] = tsens4_point;
	} else {
		if (tsens_calibration_mode == 0) {
			pr_debug("TSENS is calibrationless mode\n");
			calib_tsens_point_data[0] = 532;
			calib_tsens_point_data[1] = 532;
			calib_tsens_point_data[2] = 532;
			calib_tsens_point_data[3] = 532;
			calib_tsens_point_data[4] = 532;
		}
	}

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		int32_t num = 0, den = 0, adc_code_of_tempx = 0;
		tmdev->sensor[i].calib_data_point2 = tsens_base1_data;
		tmdev->sensor[i].calib_data_point1 = tsens_base0_data;
		pr_debug("sensor:%d - calib_data_point1:0x%x, calib_data_point2:0x%x\n",
				i, tmdev->sensor[i].calib_data_point1,
				tmdev->sensor[i].calib_data_point2);
		if (tsens_calibration_mode == TSENS_TWO_POINT_CALIB) {
			/* slope (m) = adc_code2 - adc_code1 (y2 - y1)/
			 * temp_120_degc - temp_30_degc (x2 - x1) */
			num = tmdev->sensor[i].calib_data_point2 -
				tmdev->sensor[i].calib_data_point1;
			num *= tmdev->tsens_factor;
			den = TSENS_CAL_DEGC_POINT2 - TSENS_CAL_DEGC_POINT1;
			tmdev->sensor[i].slope_mul_tsens_factor = num/den;
		}
		adc_code_of_tempx =
				tsens_base0_data + calib_tsens_point_data[i];
		pr_debug("offset_adc_code_of_tempx:0x%x\n",
						adc_code_of_tempx);
		tmdev->sensor[i].offset = (adc_code_of_tempx *
			tmdev->tsens_factor) - (TSENS_CAL_DEGC_POINT1 *
				tmdev->sensor[i].slope_mul_tsens_factor);
		pr_debug("offset:%d and slope:%d\n", tmdev->sensor[i].offset,
				tmdev->sensor[i].slope_mul_tsens_factor);
		INIT_WORK(&tmdev->sensor[i].work, notify_uspace_tsens_fn);
		tmdev->prev_reading_avail = false;
	}

	return 0;
}
static int tsens_calib_sensors(void)
{
	int rc = 0;

	if (!tmdev)
		return -ENODEV;

	if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_8974)
		rc = tsens_calib_8974_sensors();
	else if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_8X26)
		rc = tsens_calib_8x26_sensors();
	else if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_8X10)
		rc = tsens_calib_8x10_sensors();
	else if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_9900)
		rc = tsens_calib_9900_sensors();
	else if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_9630)
		rc = tsens_calib_9630_sensors();
	else if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_8916)
		rc = tsens_calib_8916_sensors();
	else if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_8939)
		rc = tsens_calib_8939_sensors();
	else if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_8994)
		rc = tsens_calib_8994_sensors();
	else if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_MSM8909)
		rc = tsens_calib_msm8909_sensors();
	else if (tmdev->calib_mode == TSENS_CALIB_FUSE_MAP_MSMZIRC)
		rc = tsens_calib_msmzirc_sensors();
	else {
		pr_err("TSENS Calib fuse not found\n");
		rc = -ENODEV;
	}

	return rc;
}

static struct of_device_id tsens_match[] = {
	{	.compatible = "qcom,msm-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_8974,
	},
	{	.compatible = "qcom,msm8x26-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_8X26,
	},
	{	.compatible = "qcom,msm8x10-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_8X10,
	},
	{	.compatible = "qcom,fsm9900-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_9900,
	},
	{	.compatible = "qcom,mdm9630-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_9630,
	},
	{	.compatible = "qcom,msm8916-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_8916,
	},
	{	.compatible = "qcom,msm8939-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_8939,
	},
	{	.compatible = "qcom,msm8994-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_8994,
	},
	{	.compatible = "qcom,msm8909-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_MSM8909,
	},
	{	.compatible = "qcom,msmzirc-tsens",
		.data = (void *)TSENS_CALIB_FUSE_MAP_MSMZIRC,
	},
	{}
};

static int get_device_tree_data(struct platform_device *pdev)
{
	struct device_node *of_node = pdev->dev.of_node;
	struct resource *res_mem = NULL;
	u32 *tsens_slope_data, *sensor_id;
	u32 rc = 0, i, tsens_num_sensors;
	const struct of_device_id *id;

	rc = of_property_read_u32(of_node,
			"qcom,sensors", &tsens_num_sensors);
	if (rc) {
		dev_err(&pdev->dev, "missing sensor number\n");
		return -ENODEV;
	}

	tsens_slope_data = devm_kzalloc(&pdev->dev,
			tsens_num_sensors * sizeof(u32), GFP_KERNEL);
	if (!tsens_slope_data) {
		dev_err(&pdev->dev, "can not allocate slope data\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,slope", tsens_slope_data, tsens_num_sensors);
	if (rc) {
		dev_err(&pdev->dev, "invalid or missing property: tsens-slope\n");
		return rc;
	};

	if (!of_match_node(tsens_match, of_node)) {
		pr_err("Need to read SoC specific fuse map\n");
		return -ENODEV;
	} else {
		id = of_match_node(tsens_match, of_node);
		if (id == NULL) {
			pr_err("can not find tsens_match of_node\n");
			return -ENODEV;
		}
	}

	tmdev = devm_kzalloc(&pdev->dev,
			sizeof(struct tsens_tm_device) +
			tsens_num_sensors *
			sizeof(struct tsens_tm_device_sensor),
			GFP_KERNEL);
	if (tmdev == NULL) {
		pr_err("%s: kzalloc() failed.\n", __func__);
		return -ENOMEM;
	}

	for (i = 0; i < tsens_num_sensors; i++)
		tmdev->sensor[i].slope_mul_tsens_factor = tsens_slope_data[i];
	tmdev->tsens_factor = TSENS_SLOPE_FACTOR;
	tmdev->tsens_num_sensor = tsens_num_sensors;
	tmdev->calibration_less_mode = of_property_read_bool(of_node,
				"qcom,calibration-less-mode");
	tmdev->tsens_local_init = of_property_read_bool(of_node,
				"qcom,tsens-local-init");
	tmdev->calib_mode = (u32)(uintptr_t) id->data;

	sensor_id = devm_kzalloc(&pdev->dev,
		tsens_num_sensors * sizeof(u32), GFP_KERNEL);
	if (!sensor_id) {
		dev_err(&pdev->dev, "can not allocate sensor id\n");
		return -ENOMEM;
	}

	rc = of_property_read_u32_array(of_node,
		"qcom,sensor-id", sensor_id, tsens_num_sensors);
	if (rc) {
		pr_debug("Default sensor id mapping\n");
		for (i = 0; i < tsens_num_sensors; i++) {
			tmdev->sensor[i].sensor_hw_num = i;
			tmdev->sensor[i].sensor_sw_id = i;
		}
	} else {
		pr_debug("Use specified sensor id mapping\n");
		for (i = 0; i < tsens_num_sensors; i++) {
			tmdev->sensor[i].sensor_hw_num = sensor_id[i];
			tmdev->sensor[i].sensor_sw_id = i;
		}
	}

	if (!strcmp(id->compatible, "qcom,mdm9630-tsens") ||
		(!strcmp(id->compatible, "qcom,msmzirc-tsens")) ||
		(!strcmp(id->compatible, "qcom,msm8994-tsens")))
		tmdev->tsens_type = TSENS_TYPE2;
	else
		tmdev->tsens_type = TSENS_TYPE0;

	if (!strcmp(id->compatible, "qcom,msm8994-tsens") ||
		(!strcmp(id->compatible, "qcom,msmzirc-tsens")))
		tmdev->tsens_valid_status_check = true;
	else
		tmdev->tsens_valid_status_check = false;

	tmdev->tsens_irq = platform_get_irq_byname(pdev,
					"tsens-upper-lower");
	if (tmdev->tsens_irq < 0) {
		pr_err("Invalid Upper/Lower irq\n");
		rc = tmdev->tsens_irq;
		goto fail_tmdev;
	}

	/* TSENS register region */
	tmdev->res_tsens_mem = platform_get_resource_byname(pdev,
					IORESOURCE_MEM, "tsens_physical");
	if (!tmdev->res_tsens_mem) {
		pr_err("Could not get tsens physical address resource\n");
		rc = -EINVAL;
		goto fail_tmdev;
	}

	tmdev->tsens_len = tmdev->res_tsens_mem->end -
					tmdev->res_tsens_mem->start + 1;

	res_mem = request_mem_region(tmdev->res_tsens_mem->start,
				tmdev->tsens_len, tmdev->res_tsens_mem->name);
	if (!res_mem) {
		pr_err("Request tsens physical memory region failed\n");
		rc = -EINVAL;
		goto fail_tmdev;
	}

	tmdev->tsens_addr = ioremap(res_mem->start, tmdev->tsens_len);
	if (!tmdev->tsens_addr) {
		pr_err("Failed to IO map TSENS registers.\n");
		rc = -EINVAL;
		goto fail_unmap_tsens_region;
	}

	/* TSENS calibration region */
	tmdev->res_calib_mem = platform_get_resource_byname(pdev,
				IORESOURCE_MEM, "tsens_eeprom_physical");
	if (!tmdev->res_calib_mem) {
		pr_err("Could not get qfprom physical address resource\n");
		rc = -EINVAL;
		goto fail_unmap_tsens;
	}

	tmdev->calib_len = tmdev->res_calib_mem->end -
					tmdev->res_calib_mem->start + 1;

	res_mem = request_mem_region(tmdev->res_calib_mem->start,
				tmdev->calib_len, tmdev->res_calib_mem->name);
	if (!res_mem) {
		pr_err("Request calibration memory region failed\n");
		rc = -EINVAL;
		goto fail_unmap_tsens;
	}

	tmdev->tsens_calib_addr = ioremap(res_mem->start,
						tmdev->calib_len);
	if (!tmdev->tsens_calib_addr) {
		pr_err("Failed to IO map EEPROM registers.\n");
		rc = -EINVAL;
		goto fail_unmap_calib_region;
	}

	return 0;

fail_unmap_calib_region:
	if (tmdev->res_calib_mem)
		release_mem_region(tmdev->res_calib_mem->start,
					tmdev->calib_len);
fail_unmap_tsens:
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
fail_unmap_tsens_region:
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
					tmdev->tsens_len);
fail_tmdev:
	tmdev = NULL;
	return rc;
}

static int tsens_tm_probe(struct platform_device *pdev)
{
	int rc;

	if (tmdev) {
		pr_err("TSENS device already in use\n");
		return -EBUSY;
	}

	if (pdev->dev.of_node) {
		rc = get_device_tree_data(pdev);
		if (rc) {
			pr_err("Error reading TSENS DT\n");
			return rc;
		}
	} else
		return -ENODEV;

	tmdev->pdev = pdev;
	tmdev->tsens_wq = alloc_workqueue("tsens_wq", WQ_HIGHPRI, 0);
	if (!tmdev->tsens_wq) {
		rc = -ENOMEM;
		goto fail;
	}

	rc = tsens_calib_sensors();
	if (rc < 0) {
		pr_err("Calibration failed\n");
		goto fail;
	}

	tsens_hw_init();

	tmdev->prev_reading_avail = true;

	tmdev->is_ready = true;

	platform_set_drvdata(pdev, tmdev);

	return 0;
fail:
	if (tmdev->tsens_wq)
		destroy_workqueue(tmdev->tsens_wq);
	if (tmdev->tsens_calib_addr)
		iounmap(tmdev->tsens_calib_addr);
	if (tmdev->res_calib_mem)
		release_mem_region(tmdev->res_calib_mem->start,
					tmdev->calib_len);
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
			tmdev->tsens_len);
	tmdev = NULL;

	return rc;
}

static int _tsens_register_thermal(void)
{
	struct platform_device *pdev;
	int rc, i;

	if (tsens_is_ready() <= 0) {
		pr_err("%s: TSENS early init not done\n", __func__);
		return -ENODEV;
	}

	pdev = tmdev->pdev;

	for (i = 0; i < tmdev->tsens_num_sensor; i++) {
		char name[18];
		snprintf(name, sizeof(name), "tsens_tz_sensor%d",
					tmdev->sensor[i].sensor_hw_num);
		tmdev->sensor[i].mode = THERMAL_DEVICE_ENABLED;
		tmdev->sensor[i].tz_dev =
			thermal_zone_device_register(name, TSENS_TRIP_NUM,
						     TSENS_WRITABLE_TRIPS_MASK,
						     &tmdev->sensor[i],
						     &tsens_thermal_zone_ops,
						     NULL, 0, 0);
		if (IS_ERR(tmdev->sensor[i].tz_dev)) {
			pr_err("%s: thermal_zone_device_register() failed.\n",
			__func__);
			rc = -ENODEV;
			goto fail;
		}
	}

	rc = request_irq(tmdev->tsens_irq, tsens_isr,
		IRQF_TRIGGER_HIGH, "tsens_interrupt", tmdev);
	if (rc < 0) {
		pr_err("%s: request_irq FAIL: %d\n", __func__, rc);
		for (i = 0; i < tmdev->tsens_num_sensor; i++)
			thermal_zone_device_unregister(tmdev->sensor[i].tz_dev);
		goto fail;
	} else {
		enable_irq_wake(tmdev->tsens_irq);
	}
	platform_set_drvdata(pdev, tmdev);

	INIT_WORK(&tmdev->tsens_work, tsens_scheduler_fn);

	return 0;
fail:
	if (tmdev->tsens_calib_addr)
		iounmap(tmdev->tsens_calib_addr);
	if (tmdev->res_calib_mem)
		release_mem_region(tmdev->res_calib_mem->start,
					tmdev->calib_len);
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
			tmdev->tsens_len);
	return rc;
}

static int tsens_tm_remove(struct platform_device *pdev)
{
	struct tsens_tm_device *tmdev = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i < tmdev->tsens_num_sensor; i++)
		thermal_zone_device_unregister(tmdev->sensor[i].tz_dev);
	if (tmdev->tsens_calib_addr)
		iounmap(tmdev->tsens_calib_addr);
	if (tmdev->res_calib_mem)
		release_mem_region(tmdev->res_calib_mem->start,
					tmdev->calib_len);
	if (tmdev->tsens_addr)
		iounmap(tmdev->tsens_addr);
	if (tmdev->res_tsens_mem)
		release_mem_region(tmdev->res_tsens_mem->start,
			tmdev->tsens_len);
	free_irq(tmdev->tsens_irq, tmdev);
	destroy_workqueue(tmdev->tsens_wq);
	platform_set_drvdata(pdev, NULL);

	return 0;
}

static struct platform_driver tsens_tm_driver = {
	.probe = tsens_tm_probe,
	.remove = tsens_tm_remove,
	.driver = {
		.name = "msm-tsens",
		.owner = THIS_MODULE,
		.of_match_table = tsens_match,
	},
};

int __init tsens_tm_init_driver(void)
{
	return platform_driver_register(&tsens_tm_driver);
}
arch_initcall(tsens_tm_init_driver);

static int __init tsens_thermal_register(void)
{
	return _tsens_register_thermal();
}
module_init(tsens_thermal_register);

static void __exit _tsens_tm_remove(void)
{
	platform_driver_unregister(&tsens_tm_driver);
}
module_exit(_tsens_tm_remove);

MODULE_ALIAS("platform:" TSENS_DRIVER_NAME);
MODULE_LICENSE("GPL v2");
