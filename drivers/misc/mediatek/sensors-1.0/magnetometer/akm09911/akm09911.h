/*
 * Copyright (C) 2017 MediaTek Inc.
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

/* akm09911.c - akm09911 compass driver
 *
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

#ifndef AKM09911_H
#define AKM09911_H

#include <linux/ioctl.h>

#define AKM09911_I2C_NAME "akm09911"

#define AKM09911_I2C_ADDRESS 0x18
#define AKM09911_BUFSIZE 0x50

#define SENSOR_DATA_SIZE 9 /* Rx buffer size, i.e from ST1 to ST2 */
#define RWBUF_SIZE 16      /* Read/Write buffer size. */
#define CALIBRATION_DATA_SIZE 26

/* Device specific constant values */
#define AK09911_REG_WIA1 0x00
#define AK09911_REG_WIA2 0x01
#define AK09911_REG_INFO1 0x02
#define AK09911_REG_INFO2 0x03
#define AK09911_REG_ST1 0x10
#define AK09911_REG_HXL 0x11
#define AK09911_REG_HXH 0x12
#define AK09911_REG_HYL 0x13
#define AK09911_REG_HYH 0x14
#define AK09911_REG_HZL 0x15
#define AK09911_REG_HZH 0x16
#define AK09911_REG_TMPS 0x17
#define AK09911_REG_ST2 0x18
#define AK09911_REG_CNTL1 0x30
#define AK09911_REG_CNTL2 0x31
#define AK09911_REG_CNTL3 0x32

/* Defines a read-only address of the fuse ROM of the AK09911.*/
#define AK09911_FUSE_ASAX 0x60
#define AK09911_FUSE_ASAY 0x61
#define AK09911_FUSE_ASAZ 0x62

/* Defines an operation mode of the AK09911.*/
#define AK09911_MODE_SNG_MEASURE 0x01
#define AK09911_MODE_SELF_TEST 0x10
#define AK09911_MODE_FUSE_ACCESS 0x1F
#define AK09911_MODE_POWERDOWN 0x00
#define AK09911_RESET_DATA 0x01

#define AK09911_REGS_SIZE 13
#define AK09911_WIA1_VALUE 0x48
#define AK09911_WIA2_VALUE 0x05

/* Device specific constant values for AK8963*/
#define AK8963_REG_WIA 0x00
#define AK8963_REG_INFO 0x01
#define AK8963_REG_ST1 0x02
#define AK8963_REG_HXL 0x03
#define AK8963_REG_HXH 0x04
#define AK8963_REG_HYL 0x05
#define AK8963_REG_HYH 0x06
#define AK8963_REG_HZL 0x07
#define AK8963_REG_HZH 0x08
#define AK8963_REG_ST2 0x09
#define AK8963_REG_CNTL1 0x0A
#define AK8963_REG_CNTL2 0x0B
#define AK8963_REG_ASTC 0x0C
#define AK8963_REG_TS1 0x0D
#define AK8963_REG_TS2 0x0E
#define AK8963_REG_I2CDIS 0x0F

#define AK8963_FUSE_ASAX 0x10
#define AK8963_FUSE_ASAY 0x11
#define AK8963_FUSE_ASAZ 0x12

#define AK8963_MODE_SNG_MEASURE 0x01
#define AK8963_MODE_SELF_TEST 0x08
#define AK8963_MODE_FUSE_ACCESS 0x0F
#define AK8963_MODE_POWERDOWN 0x00
#define AK8963_RESET_DATA 0x01

#define AK8963_REGS_SIZE 13
#define AK8963_WIA_VALUE 0x48

/* To avoid device dependency, convert to general name */
#define AKM_I2C_NAME "akm09911"
#define AKM_MISCDEV_NAME "akm09911_dev"
#define AKM_SYSCLS_NAME "compass"
#define AKM_SYSDEV_NAME "akm09911"
#define AKM_REG_MODE AK09911_REG_CNTL2
#define AKM_REG_RESET AK09911_REG_CNTL3
#define AKM_REG_STATUS AK09911_REG_ST1
#define AKM_MEASURE_TIME_US 10000
#define AKM_DRDY_IS_HIGH(x) ((x)&0x01)
#define AKM_SENSOR_INFO_SIZE 2
#define AKM_SENSOR_CONF_SIZE 3
#define AKM_SENSOR_DATA_SIZE 9

#define AKM_YPR_DATA_SIZE 26
#define AKM_RWBUF_SIZE 16
#define AKM_REGS_SIZE AK09911_REGS_SIZE
#define AKM_REGS_1ST_ADDR AK09911_REG_WIA1
#define AKM_FUSE_1ST_ADDR AK09911_FUSE_ASAX

#define AKM_MODE_SNG_MEASURE AK09911_MODE_SNG_MEASURE
#define AKM_MODE_SELF_TEST AK09911_MODE_SELF_TEST
#define AKM_MODE_FUSE_ACCESS AK09911_MODE_FUSE_ACCESS
#define AKM_MODE_POWERDOWN AK09911_MODE_POWERDOWN
#define AKM_RESET_DATA AK09911_RESET_DATA

#define ACC_DATA_FLAG 0
#define MAG_DATA_FLAG 1
#define FUSION_DATA_FLAG 2
#define AKM_NUM_SENSORS 3

#define ACC_DATA_READY (1 << (ACC_DATA_FLAG))
#define MAG_DATA_READY (1 << (MAG_DATA_FLAG))
#define FUSION_DATA_READY (1 << (FUSION_DATA_FLAG))
/*! @}*/

/* conversion of magnetic data (for AK09911) to uT units */
/* #define CONVERT_M                   (1.0f*0.06f) */
/* conversion of orientation data to degree units */
/* #define CONVERT_O                   (1.0f/64.0f) */

#define CONVERT_M 6
#define CONVERT_M_DIV 100 /* 6/100 = CONVERT_M */

#define CSPEC_SPI_USE 0
#define DBG_LEVEL0 0x0001 /* Critical */
#define DBG_LEVEL1 0x0002 /* Notice */
#define DBG_LEVEL2 0x0003 /* Information */
#define DBG_LEVEL3 0x0004 /* Debug */
#define DBGFLAG DBG_LEVEL2

#define AKM099XX_AXIS_X 0
#define AKM099XX_AXIS_Y 1
#define AKM099XX_AXIS_Z 2

#define AKECS_ASA_CACULATE_AK09912(x) (((x - 128) / 256) + 1)
#define AKECS_ASA_CACULATE_AK09911(x) ((x / 128) + 1)

/* IOCTLs for Msensor misc. device library */
#define MSENSOR 0x83
/* IOCTLs for AKM library */
#define ECS_IOCTL_WRITE _IOW(MSENSOR, 0x0b, char *)
#define ECS_IOCTL_READ _IOWR(MSENSOR, 0x0c, char *)
#define ECS_IOCTL_RESET _IO(MSENSOR, 0x0d) /* NOT used in AK8975 */
#define ECS_IOCTL_SET_MODE _IOW(MSENSOR, 0x0e, short)
#define ECS_IOCTL_GETDATA _IOR(MSENSOR, 0x0f, char[SENSOR_DATA_SIZE])
#define ECS_IOCTL_SET_YPR _IOW(MSENSOR, 0x10, short[12])
#define ECS_IOCTL_GET_OPEN_STATUS _IOR(MSENSOR, 0x11, int)
#define ECS_IOCTL_GET_CLOSE_STATUS _IOR(MSENSOR, 0x12, int)
#define ECS_IOCTL_GET_OSENSOR_STATUS _IOR(MSENSOR, 0x13, int)
#define ECS_IOCTL_GET_DELAY _IOR(MSENSOR, 0x14, short)
#define ECS_IOCTL_GET_PROJECT_NAME _IOR(MSENSOR, 0x15, char[64])
#define ECS_IOCTL_GET_MATRIX _IOR(MSENSOR, 0x16, short[4][3][3])
#define ECS_IOCTL_GET_LAYOUT _IOR(MSENSOR, 0x17, int[3])

#define ECS_IOCTL_GET_OUTBIT _IOR(MSENSOR, 0x23, char)
#define ECS_IOCTL_GET_ACCEL _IOR(MSENSOR, 0x24, short[3])

#define ECS_IOCTL_GET_CONF                                                     \
	_IOR(MSENSOR, 0x28, unsigned char[AKM_SENSOR_CONF_SIZE])
#define ECS_IOCTL_SET_YPR_09911 _IOW(MSENSOR, 0x29, int[26])
#define ECS_IOCTL_GET_DELAY_09911 _IOR(MSENSOR, 0x30, int64_t[3])
#define ECS_IOCTL_GET_LAYOUT_09911 _IOR(MSENSOR, 0x31, char)

#ifndef DBGPRINT
#define DBGPRINT(level, format, ...)                                           \
	((((level) != 0) && ((level) <= DBGFLAG))                              \
		 ? (pr_debug((format), ##__VA_ARGS__))                         \
		 : (void)0)

#endif

struct akm09911_platform_data {
	char layout;
	char outbit;
	int gpio_DRDY;
	int gpio_RSTN;
};

/*** Limit of factory shipment test *******************************************/
#define TLIMIT_TN_REVISION_09911 ""
#define TLIMIT_NO_RST_WIA1_09911 "1-3"
#define TLIMIT_TN_RST_WIA1_09911 "RST_WIA1"
#define TLIMIT_LO_RST_WIA1_09911 0x48
#define TLIMIT_HI_RST_WIA1_09911 0x48
#define TLIMIT_NO_RST_WIA2_09911 "1-4"
#define TLIMIT_TN_RST_WIA2_09911 "RST_WIA2"
#define TLIMIT_LO_RST_WIA2_09911 0x05
#define TLIMIT_HI_RST_WIA2_09911 0x05

#define TLIMIT_NO_ASAX_09911 "1-7"
#define TLIMIT_TN_ASAX_09911 "ASAX"
#define TLIMIT_LO_ASAX_09911 1
#define TLIMIT_HI_ASAX_09911 254
#define TLIMIT_NO_ASAY_09911 "1-8"
#define TLIMIT_TN_ASAY_09911 "ASAY"
#define TLIMIT_LO_ASAY_09911 1
#define TLIMIT_HI_ASAY_09911 254
#define TLIMIT_NO_ASAZ_09911 "1-9"
#define TLIMIT_TN_ASAZ_09911 "ASAZ"
#define TLIMIT_LO_ASAZ_09911 1
#define TLIMIT_HI_ASAZ_09911 254

#define TLIMIT_NO_SNG_ST1_09911 "2-3"
#define TLIMIT_TN_SNG_ST1_09911 "SNG_ST1"
#define TLIMIT_LO_SNG_ST1_09911 1
#define TLIMIT_HI_SNG_ST1_09911 1

#define TLIMIT_NO_SNG_HX_09911 "2-4"
#define TLIMIT_TN_SNG_HX_09911 "SNG_HX"
#define TLIMIT_LO_SNG_HX_09911 -8189
#define TLIMIT_HI_SNG_HX_09911 8189

#define TLIMIT_NO_SNG_HY_09911 "2-6"
#define TLIMIT_TN_SNG_HY_09911 "SNG_HY"
#define TLIMIT_LO_SNG_HY_09911 -8189
#define TLIMIT_HI_SNG_HY_09911 8189

#define TLIMIT_NO_SNG_HZ_09911 "2-8"
#define TLIMIT_TN_SNG_HZ_09911 "SNG_HZ"
#define TLIMIT_LO_SNG_HZ_09911 -8189
#define TLIMIT_HI_SNG_HZ_09911 8189

#define TLIMIT_NO_SNG_ST2_09911 "2-10"
#define TLIMIT_TN_SNG_ST2_09911 "SNG_ST2"
#define TLIMIT_LO_SNG_ST2_09911 0
#define TLIMIT_HI_SNG_ST2_09911 0

#define TLIMIT_NO_SLF_ST1_09911 "2-13"
#define TLIMIT_TN_SLF_ST1_09911 "SLF_ST1"
#define TLIMIT_LO_SLF_ST1_09911 1
#define TLIMIT_HI_SLF_ST1_09911 1

#define TLIMIT_NO_SLF_RVHX_09911 "2-14"
#define TLIMIT_TN_SLF_RVHX_09911 "SLF_REVSHX"
#define TLIMIT_LO_SLF_RVHX_09911 -30
#define TLIMIT_HI_SLF_RVHX_09911 30

#define TLIMIT_NO_SLF_RVHY_09911 "2-16"
#define TLIMIT_TN_SLF_RVHY_09911 "SLF_REVSHY"
#define TLIMIT_LO_SLF_RVHY_09911 -30
#define TLIMIT_HI_SLF_RVHY_09911 30

#define TLIMIT_NO_SLF_RVHZ_09911 "2-18"
#define TLIMIT_TN_SLF_RVHZ_09911 "SLF_REVSHZ"
#define TLIMIT_LO_SLF_RVHZ_09911 -400
#define TLIMIT_HI_SLF_RVHZ_09911 -50

#define TLIMIT_NO_SLF_ST2_09911 "2-20"
#define TLIMIT_TN_SLF_ST2_09911 "SLF_ST2"
#define TLIMIT_LO_SLF_ST2_09911 0
#define TLIMIT_HI_SLF_ST2_09911 0

/*** Limit of factory shipment test *******************************************/

#define TLIMIT_TN_REVISION ""
#define TLIMIT_NO_RST_WIA "1-3"
#define TLIMIT_TN_RST_WIA "RST_WIA"
#define TLIMIT_LO_RST_WIA 0x48
#define TLIMIT_HI_RST_WIA 0x48
#define TLIMIT_NO_RST_INFO "1-4"
#define TLIMIT_TN_RST_INFO "RST_INFO"
#define TLIMIT_LO_RST_INFO 0
#define TLIMIT_HI_RST_INFO 255
#define TLIMIT_NO_RST_ST1 "1-5"
#define TLIMIT_TN_RST_ST1 "RST_ST1"
#define TLIMIT_LO_RST_ST1 0
#define TLIMIT_HI_RST_ST1 0
#define TLIMIT_NO_RST_HXL "1-6"
#define TLIMIT_TN_RST_HXL "RST_HXL"
#define TLIMIT_LO_RST_HXL 0
#define TLIMIT_HI_RST_HXL 0
#define TLIMIT_NO_RST_HXH "1-7"
#define TLIMIT_TN_RST_HXH "RST_HXH"
#define TLIMIT_LO_RST_HXH 0
#define TLIMIT_HI_RST_HXH 0
#define TLIMIT_NO_RST_HYL "1-8"
#define TLIMIT_TN_RST_HYL "RST_HYL"
#define TLIMIT_LO_RST_HYL 0
#define TLIMIT_HI_RST_HYL 0
#define TLIMIT_NO_RST_HYH "1-9"
#define TLIMIT_TN_RST_HYH "RST_HYH"
#define TLIMIT_LO_RST_HYH 0
#define TLIMIT_HI_RST_HYH 0
#define TLIMIT_NO_RST_HZL "1-10"
#define TLIMIT_TN_RST_HZL "RST_HZL"
#define TLIMIT_LO_RST_HZL 0
#define TLIMIT_HI_RST_HZL 0
#define TLIMIT_NO_RST_HZH "1-11"
#define TLIMIT_TN_RST_HZH "RST_HZH"
#define TLIMIT_LO_RST_HZH 0
#define TLIMIT_HI_RST_HZH 0
#define TLIMIT_NO_RST_ST2 "1-12"
#define TLIMIT_TN_RST_ST2 "RST_ST2"
#define TLIMIT_LO_RST_ST2 0
#define TLIMIT_HI_RST_ST2 0
#define TLIMIT_NO_RST_CNTL "1-13"
#define TLIMIT_TN_RST_CNTL "RST_CNTL"
#define TLIMIT_LO_RST_CNTL 0
#define TLIMIT_HI_RST_CNTL 0
#define TLIMIT_NO_RST_ASTC "1-14"
#define TLIMIT_TN_RST_ASTC "RST_ASTC"
#define TLIMIT_LO_RST_ASTC 0
#define TLIMIT_HI_RST_ASTC 0
#define TLIMIT_NO_RST_I2CDIS "1-15"
#define TLIMIT_TN_RST_I2CDIS "RST_I2CDIS"
#define TLIMIT_LO_RST_I2CDIS_USEI2C 0
#define TLIMIT_HI_RST_I2CDIS_USEI2C 0
#define TLIMIT_LO_RST_I2CDIS_USESPI 1
#define TLIMIT_HI_RST_I2CDIS_USESPI 1
#define TLIMIT_NO_ASAX "1-17"
#define TLIMIT_TN_ASAX "ASAX"
#define TLIMIT_LO_ASAX 1
#define TLIMIT_HI_ASAX 254
#define TLIMIT_NO_ASAY "1-18"
#define TLIMIT_TN_ASAY "ASAY"
#define TLIMIT_LO_ASAY 1
#define TLIMIT_HI_ASAY 254
#define TLIMIT_NO_ASAZ "1-19"
#define TLIMIT_TN_ASAZ "ASAZ"
#define TLIMIT_LO_ASAZ 1
#define TLIMIT_HI_ASAZ 254
#define TLIMIT_NO_WR_CNTL "1-20"
#define TLIMIT_TN_WR_CNTL "WR_CNTL"
#define TLIMIT_LO_WR_CNTL 0x0F
#define TLIMIT_HI_WR_CNTL 0x0F

#define TLIMIT_NO_SNG_ST1 "2-3"
#define TLIMIT_TN_SNG_ST1 "SNG_ST1"
#define TLIMIT_LO_SNG_ST1 1
#define TLIMIT_HI_SNG_ST1 1

#define TLIMIT_NO_SNG_HX "2-4"
#define TLIMIT_TN_SNG_HX "SNG_HX"
#define TLIMIT_LO_SNG_HX -32759
#define TLIMIT_HI_SNG_HX 32759

#define TLIMIT_NO_SNG_HY "2-6"
#define TLIMIT_TN_SNG_HY "SNG_HY"
#define TLIMIT_LO_SNG_HY -32759
#define TLIMIT_HI_SNG_HY 32759

#define TLIMIT_NO_SNG_HZ "2-8"
#define TLIMIT_TN_SNG_HZ "SNG_HZ"
#define TLIMIT_LO_SNG_HZ -32759
#define TLIMIT_HI_SNG_HZ 32759

#define TLIMIT_NO_SNG_ST2 "2-10"
#define TLIMIT_TN_SNG_ST2 "SNG_ST2"
#define TLIMIT_LO_SNG_ST2 0
#define TLIMIT_HI_SNG_ST2 0

#define TLIMIT_NO_SLF_ST1 "2-14"
#define TLIMIT_TN_SLF_ST1 "SLF_ST1"
#define TLIMIT_LO_SLF_ST1 1
#define TLIMIT_HI_SLF_ST1 1

#define TLIMIT_NO_SLF_RVHX "2-15"
#define TLIMIT_TN_SLF_RVHX "SLF_REVSHX"
#define TLIMIT_LO_SLF_RVHX -200
#define TLIMIT_HI_SLF_RVHX 200

#define TLIMIT_NO_SLF_RVHY "2-17"
#define TLIMIT_TN_SLF_RVHY "SLF_REVSHY"
#define TLIMIT_LO_SLF_RVHY -200
#define TLIMIT_HI_SLF_RVHY 200

#define TLIMIT_NO_SLF_RVHZ "2-19"
#define TLIMIT_TN_SLF_RVHZ "SLF_REVSHZ"
#define TLIMIT_LO_SLF_RVHZ -3200
#define TLIMIT_HI_SLF_RVHZ -800

#define TLIMIT_NO_SLF_ST2 "2-21"
#define TLIMIT_TN_SLF_ST2 "SLF_ST2"
#define TLIMIT_LO_SLF_ST2 0
#define TLIMIT_HI_SLF_ST2 0

#ifdef CONFIG_CUSTOM_KERNEL_MAGNETOMETER_MODULE
extern bool mag_success_Flag;
#endif

#endif
