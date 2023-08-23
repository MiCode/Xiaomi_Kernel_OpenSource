/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (c) 2018-2019, Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version
 * 2 of the License, or (at your option) any later version.
 */

#ifndef __AKM09970_H__
#define __AKM09970_H__

#include <linux/types.h>
#include <linux/ioctl.h>

//#define pr_fmt(fmt) "akm09970: %s: %d " fmt, __func__, __LINE__

#define AKM09970_DRV_NAME       "akm09970"
#define AKM09970_CLASS_NAME     "akm"

/* AKM09970 Driver Magic Number */
#define AKM_IOC_MAGIC           'M'

#define AKM_PRIVATE             109

/* Device specific constant values */
#define AK09970_REG_WIA                 0x00
#define AK09970_REG_ST_XYZ              0x17
#define AK09970_REG_CNTL1               0x20
#define AK09970_REG_CNTL2               0x21
#define AK09970_REG_RESET               0x30

#define AK09970_RESET_DATA              0x01

#define AK09970_WIA1_VALUE              0x48
#define AK09970_WIA2_VALUE              0xC0

#define AK09970_MODE_POWERDOWN          0x00
#define AK09970_MODE_CONTINUOUS_10HZ    0x08 /* 10Hz */
#define AK09970_MODE_CONTINUOUS_20HZ    0x0A /* 20Hz */
#define AK09970_MODE_CONTINUOUS_50HZ    0x0C /* 50Hz */
#define AK09970_MODE_CONTINUOUS_100HZ   0x0E /* 100Hz */

#define AKM_SENSOR_INFO_SIZE        2
#define AKM_SENSOR_CONF_SIZE        3
#define AKM_SENSOR_DATA_SIZE        8

#define AK09970_MODE_POS        0
#define AK09970_MODE_MSK        0x0F
#define AK09970_MODE_REG        AK09970_REG_CNTL2

#define AK09970_SDR_MODE_POS    4
#define AK09970_SDR_MODE_MSK    0x10
#define AK09970_SDR_MODE_REG    AK09970_REG_CNTL2

#define AK09970_SMR_MODE_POS    5
#define AK09970_SMR_MODE_MSK    0x20
#define AK09970_SMR_MODE_REG    AK09970_REG_CNTL2

#define AKM_DRDY_IS_HIGH(x)         ((x) & 0x01)
#define AKM_DOR_IS_HIGH(x)          ((x) & 0x02)
#define AKM_ERRADC_IS_HIGH(x)       ((x) & 0x01)
#define AKM_ERRXY_IS_HIGH(x)        ((x) & 0x80)

#define AK09970_SENS_Q16    ((int32_t)(72090))  /* 1.1uT in Q16 format */

#define AKM_DRDY_TIMEOUT_MS     100
#define AKM_DEFAULT_MEASURE_HZ  10

/* POWER SUPPLY VOLTAGE RANGE */
#define AKM09970_VDD_MIN_UV 1800000
#define AKM09970_VDD_MAX_UV 1800000

#define PWM_PERIOD_DEFAULT_NS 1000000

struct akm09970_platform_data {
	uint8_t  sensor_smr;
	uint8_t  sensor_mode;
	uint8_t  sensor_state;
	uint8_t data[AKM_SENSOR_DATA_SIZE];
};

/* IOC CMD */
#define AKM_IOC_SET_ACTIVE \
	_IOW(AKM_IOC_MAGIC, AKM_PRIVATE + 1, struct akm09970_platform_data)

#define AKM_IOC_SET_MODE \
	_IOW(AKM_IOC_MAGIC, AKM_PRIVATE + 2, struct akm09970_platform_data)

#define AKM_IOC_GET_SENSEDATA \
	_IOR(AKM_IOC_MAGIC, AKM_PRIVATE + 4, struct akm09970_platform_data)

#define AKM_IOC_GET_SENSSMR \
	_IOR(AKM_IOC_MAGIC, AKM_PRIVATE + 5, struct akm09970_platform_data)


#endif /* __AKM09970_H__ */
