/*
 * Copyright (C) 2015 MediaTek Inc.
 * Copyright (C) 2018 XiaoMi, Inc.
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

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <asm/atomic.h>
/*#include <linux/xlog.h>*/


#include "mt_cam.h"

#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"

/* temp workaround */
/* belows are used to build pass , it should move to board-{PROJECT}.c */
/* Camera Custom Configs */
const int camera_i2c_bus_num1 = 3;
const int camera_i2c_bus_num2 = 3;


PowerUp PowerOnList = {
	{
		{SENSOR_DRVNAME_S5K3L8_MIPI_RAW,
			{
				{RST, Vol_Low, 1},
				{AVDD, Vol_2800, 1},
				{DOVDD, Vol_1800, 1},
				{DVDD, Vol_1200, 10},
				{AFVDD, Vol_2800, 10},
				{RST, Vol_High, 5},
				{SensorMCLK, Vol_High, 1},
			},
		},
		{SENSOR_DRVNAME_S5K5E8YX_MIPI_RAW,
			{
				{PDN, Vol_Low, 1},
				{RST, Vol_Low, 1},
				{AVDD, Vol_2800, 1},
				{DOVDD, Vol_1800, 1},
				{DVDD, Vol_1200, 1},
				{PDN, Vol_High, 1},
				{RST, Vol_High, 1},
				{SensorMCLK, Vol_High, 1},
			},
		},
		/* add new sensor before this line */
		{NULL,},
	}
};
/* Camera Custom Configs */

/******************************************************************************
 * Debug configuration
******************************************************************************/
#define PFX "[mt_cam]"
#define PK_DBG_NONE(fmt, arg...)    do {} while (0)
#define PK_DBG_FUNC(fmt, arg...)    pr_debug(PFX "[%s]" fmt, __func__, ##arg)

#define DEBUG_CAMERA_HW_K
#ifdef DEBUG_CAMERA_HW_K
#define PK_DBG PK_DBG_FUNC
#define PK_INF(fmt, arg...)         pr_info(PFX "[%s]" fmt, __func__, ##arg)
#define PK_ERR(fmt, arg...)         pr_err(PFX "[%s]" fmt, __func__, ##arg)
#else
#define PK_DBG(a, ...)
#define PK_ERR(fmt, arg...)
#define PK_ERR(a, ...)
#endif


u32 pinSetIdx = 0;		/* default main sensor */

BOOL hwpoweron(PowerInformation pwInfo, char *mode_name)
{
	switch (pwInfo.PowerType) {
	/* Power pins */
	case AVDD:
	case DVDD:
	case DOVDD:
	case AFVDD:
	{
		PK_INF("Power Pin#:%d (%dV)\n", pwInfo.PowerType, pwInfo.Voltage);
		if (TRUE != CAMERA_Regulator_poweron(pinSetIdx, pwInfo.PowerType, pwInfo.Voltage)) {
			PK_DBG("[CAMERA SENSOR] Fail to enable cam(%d) power(%d) to %d\n",
					pinSetIdx, pwInfo.PowerType, pwInfo.Voltage);
				return FALSE;
		}
	}
	break;
	/* GPIO pins*/
	case RST:
	case PDN:
	case LDO:
	{
		PK_DBG("Set GPIO pin(%d): %d\n", pwInfo.PowerType, pwInfo.Voltage);
		mtkcam_gpio_set(pinSetIdx, pwInfo.PowerType, pwInfo.Voltage);
	}
	break;

	/* MCLK */
	case SensorMCLK:
	{
		if (pinSetIdx == 0) {
			PK_DBG("Sensor MCLK1 On");
			ISP_MCLK1_EN(TRUE);
		} else if (pinSetIdx == 1)	{
			PK_DBG("Sensor MCLK2 On");
			ISP_MCLK2_EN(TRUE);
		}
	}
	break;
	default:
		pr_err("Error: invalid Power type (%d)\n", pwInfo.PowerType);
	break;
	};

	if (pwInfo.Delay > 0)
		mdelay(pwInfo.Delay);

	return TRUE;
}



BOOL hwpowerdown(PowerInformation pwInfo, char *mode_name)
{
	switch (pwInfo.PowerType) {
	/* Power pins */
	case AVDD:
	case DVDD:
	case DOVDD:
	case AFVDD:
	{
		PK_DBG("Power Pin#:%d (%dV)\n", pwInfo.PowerType, pwInfo.Voltage);
		if (TRUE != CAMERA_Regulator_powerdown(pinSetIdx, pwInfo.PowerType)) {
			PK_DBG("[CAMERA SENSOR] Fail to disable cam(%d) power(%d) to %d\n",
					pinSetIdx, pwInfo.PowerType, pwInfo.Voltage);
				return FALSE;
		}
	}
	break;
	/* GPIO pins*/
	case RST:
	case PDN:
	case LDO:
	{
		PK_DBG("Set GPIO pin(%d): %d\n", pwInfo.PowerType, pwInfo.Voltage);
		mtkcam_gpio_set(pinSetIdx, pwInfo.PowerType, Vol_Low);
	}
	break;

	/* MCLK */
	case SensorMCLK:
	{
		if (pinSetIdx == 0) {
			PK_DBG("Sensor MCLK1 OFF");
			ISP_MCLK1_EN(FALSE);
		} else if (pinSetIdx == 1)	{
			PK_DBG("Sensor MCLK2 OFF");
			ISP_MCLK2_EN(FALSE);
		}
	}
	break;
	default:
		pr_err("Error: invalid Power type (%d)\n", pwInfo.PowerType);
	break;
	};

	return TRUE;
}


int kdCISModulePowerOn(CAMERA_DUAL_CAMERA_SENSOR_ENUM SensorIdx,
						char *currSensorName, BOOL On, char *mode_name)
{
	int pwListIdx, pwIdx;

	if ((DUAL_CAMERA_MAIN_SENSOR == SensorIdx) && currSensorName
	    && (0 == strcmp(PowerOnList.PowerSeq[0].SensorName, currSensorName))) {
		pinSetIdx = 0;
	} else if (DUAL_CAMERA_SUB_SENSOR == SensorIdx && currSensorName
		   && (0 == strcmp(PowerOnList.PowerSeq[1].SensorName, currSensorName))) {
		pinSetIdx = 1;
	} else {
		PK_DBG("Not Match ! Bypass:  SensorIdx = %d (1:Main , 2:Sub), SensorName=%s\n",
		       SensorIdx, currSensorName);
		return -ENODEV;
	}

	/* power ON */
	if (On) {
		printk(KERN_ERR "kdCISModulePowerOn -on:currSensorName=%s\n", currSensorName);
		printk(KERN_ERR "kdCISModulePowerOn -on:pinSetIdx=%d\n", pinSetIdx);

		for (pwListIdx = 0; pwListIdx < 16; pwListIdx++) {
			if (currSensorName && (PowerOnList.PowerSeq[pwListIdx].SensorName != NULL)
			    && (0 == strcmp(PowerOnList.PowerSeq[pwListIdx].SensorName, currSensorName))) {
				printk(KERN_ERR "kdCISModulePowerOn get in---\n");
				printk(KERN_ERR "sensorIdx:%d\n", SensorIdx);

				for (pwIdx = 0; pwIdx < 10; pwIdx++) {
					if (PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx].PowerType != VDD_None) {
						if (hwpoweron(PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx], mode_name) == FALSE) {
							printk(KERN_ERR "Power ON Fail\n");
							goto _kdCISModulePowerOn_exit_;
						}
					} else {
						PK_DBG("pwIdx=%d\n", pwIdx);
						break;
					}
				}
				break;
			} else if (PowerOnList.PowerSeq[pwListIdx].SensorName == NULL) {
				break;
			} else {
			}
		}
	} else {		/* power OFF */
		PK_DBG("kdCISModulePowerOn -off:currSensorName=%s\n",
			currSensorName);
		for (pwListIdx = 0; pwListIdx < 16; pwListIdx++) {
			if (currSensorName && (PowerOnList.PowerSeq[pwListIdx].SensorName != NULL)
			    && (0 == strcmp(PowerOnList.PowerSeq[pwListIdx].SensorName, currSensorName))) {
				PK_DBG("kdCISModulePowerOn get in---\n");
				PK_DBG("sensorIdx:%d\n", SensorIdx);

				for (pwIdx = 9; pwIdx >= 0; pwIdx--) {
					if (PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx].PowerType != VDD_None) {
						if (hwpowerdown(PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx], mode_name) == FALSE)
							goto _kdCISModulePowerOn_exit_;
						if (pwIdx > 0) {
							if (PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx - 1].Delay > 0)
								mdelay(PowerOnList.PowerSeq[pwListIdx].PowerInfo[pwIdx - 1].Delay);
						}
					} else {
						PK_DBG("pwIdx=%d\n", pwIdx);
					}
				}
			} else if (PowerOnList.PowerSeq[pwListIdx].SensorName == NULL) {
				break;
			} else {
			}
		}
	}			/*  */

	return 0;

 _kdCISModulePowerOn_exit_:
	return -EIO;
}
EXPORT_SYMBOL(kdCISModulePowerOn);


/* !-- */
/*  */

