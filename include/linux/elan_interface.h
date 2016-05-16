/*
 * This file is part of the EPL2182 sensor driver.
 * Chip is combined proximity and ambient light sensor.
 *
 * Copyright (C) 2011-2014 ELAN Corporation.
 * Copyright (C) 2016 XiaoMi, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __ELAN_EPL2182__
#define __ELAN_EPL2182__

/* POWER SUPPLY VOLTAGE RANGE */
#define EPL2182_VDD_MIN_UV  2000000
#define EPL2182_VDD_MAX_UV  3300000
#define EPL2182_VIO_MIN_UV  1750000
#define EPL2182_VIO_MAX_UV  1950000

#define ELAN_LS_2182 "elan-epl2182"
#define ELAN_LS_6801 "elan-epl6801"
#define ELAN_LS_6802 "elan-epl6802"
#define ELAN_LS_6803 "elan-epl6803"
#define ELAN_LS_6804 "elan-epl6804"
#define ELAN_LS_6814 "elan-epl6814"
#define ELAN_LS_2182 "elan-epl2182"
#define ELAN_LS_8852 "elan-epl8852"

#define ELAN_IOCTL_MAGIC 'c'
#define ELAN_EPL6801_IOCTL_GET_PFLAG _IOR(ELAN_IOCTL_MAGIC, 1, int *)
#define ELAN_EPL6801_IOCTL_GET_LFLAG _IOR(ELAN_IOCTL_MAGIC, 2, int *)
#define ELAN_EPL6801_IOCTL_ENABLE_PFLAG _IOW(ELAN_IOCTL_MAGIC, 3, int *)
#define ELAN_EPL6801_IOCTL_ENABLE_LFLAG _IOW(ELAN_IOCTL_MAGIC, 4, int *)
#define ELAN_EPL6801_IOCTL_GETDATA _IOR(ELAN_IOCTL_MAGIC, 5, int *)

#define ELAN_EPL6800_IOCTL_GET_PFLAG _IOR(ELAN_IOCTL_MAGIC, 1, int *)
#define ELAN_EPL6800_IOCTL_GET_LFLAG _IOR(ELAN_IOCTL_MAGIC, 2, int *)
#define ELAN_EPL6800_IOCTL_ENABLE_PFLAG _IOW(ELAN_IOCTL_MAGIC, 3, int *)
#define ELAN_EPL6800_IOCTL_ENABLE_LFLAG _IOW(ELAN_IOCTL_MAGIC, 4, int *)
#define ELAN_EPL6800_IOCTL_GETDATA _IOR(ELAN_IOCTL_MAGIC, 5, int *)
#define LOW_OFFSET 300
#define HIGH_OFFSET 600

/*for epl6801 command */
#define REG_0			0X00
#define REG_1			0X01
#define REG_2			0X02
#define REG_3			0X03
#define REG_4			0X04
#define REG_5			0X05
#define REG_6			0X06
#define REG_7			0X07
#define REG_8			0X08
#define REG_9			0X09
#define REG_10			0X0A
#define REG_11			0X0B
#define REG_12			0X0C
#define REG_13			0X0D
#define REG_14			0X0E
#define REG_15			0X0F
#define REG_16			0X10
#define REG_17			0X11
#define REG_18			0X12
#define REG_19			0X13
#define REG_20			0X14
#define REG_21			0X15

#define W_SINGLE_BYTE		0X00
#define W_TWO_BYTE		0X01
#define W_THREE_BYTE		0X02
#define W_FOUR_BYTE		0X03
#define W_FIVE_BYTE		0X04
#define W_SIX_BYTE		0X05
#define W_SEVEN_BYTE		0X06
#define W_EIGHT_BYTE		0X07

#define R_SINGLE_BYTE		0X00
#define R_TWO_BYTE		0X01
#define R_THREE_BYTE		0X02
#define R_FOUR_BYTE		0X03
#define R_FIVE_BYTE		0X04
#define R_SIX_BYTE		0X05
#define R_SEVEN_BYTE		0X06
#define R_EIGHT_BYTE		0X07

#define EPL_SENSING_1_TIME	(0 << 5)
#define EPL_SENSING_2_TIME	(1 << 5)
#define EPL_SENSING_4_TIME	(2 << 5)
#define EPL_SENSING_8_TIME	(3 << 5)
#define EPL_SENSING_16_TIME	(4 << 5)
#define EPL_SENSING_32_TIME	(5 << 5)
#define EPL_SENSING_64_TIME	(6 << 5)
#define EPL_SENSING_128_TIME	(7 << 5)
#define EPL_C_SENSING_MODE	(0 << 4)
#define EPL_S_SENSING_MODE	(1 << 4)
#define EPL_ALS_MODE		(0 << 2)
#define EPL_PS_MODE		(1 << 2)
#define EPL_TEMP_MODE		(2 << 2)
#define EPL_H_GAIN		(0)
#define EPL_M_GAIN		(1)
#define EPL_L_GAIN		(3)
#define EPL_AUTO_GAIN		(2)

#define EPL_8BIT_ADC		0
#define EPL_10BIT_ADC		1
#define EPL_12BIT_ADC		2
#define EPL_14BIT_ADC		3

#define EPL_C_RESET				0x00
#define EPL_C_START_RUN		0x04
#define EPL_C_P_UP				0x04
#define EPL_C_P_DOWN			0x06
#define EPL_DATA_LOCK_ONLY		0x01
#define EPL_DATA_LOCK			0x05
#define EPL_DATA_UNLOCK		0x04

#define EPL_GO_MID				0x3E
#define EPL_GO_LOW				0x3E

#define EPL_INT_BINARY			0
#define EPL_INT_DISABLE			2
#define EPL_INT_ACTIVE_LOW		3
#define EPL_INT_FRAME_ENABLE		4

#define EPL_PST_1_TIME		(0 << 2)
#define EPL_PST_4_TIME		(1 << 2)
#define EPL_PST_8_TIME		(2 << 2)
#define EPL_PST_16_TIME		(3 << 2)


#define EPL_ALS_INTT_8	 (1)
#define EPL_ALS_INTT_16	(2)
#define EPL_ALS_INTT_32	(3)
#define EPL_ALS_INTT_64	(4)
#define EPL_ALS_INTT_128   (5)
#define EPL_ALS_INTT_256   (6)
#define EPL_ALS_INTT_512   (7)
#define EPL_ALS_INTT_640   (8)
#define EPL_ALS_INTT_768   (9)
#define EPL_ALS_INTT_1024  (10)
#define EPL_ALS_INTT_2048  (11)
#define EPL_ALS_INTT_4096  (12)
#define EPL_ALS_INTT_6144  (13)
#define EPL_ALS_INTT_8192  (14)
#define EPL_ALS_INTT_10240 (15)

struct elan_epl_platform_data {
	int intr_gpio;
	int (*power) (int on);
};

#endif
