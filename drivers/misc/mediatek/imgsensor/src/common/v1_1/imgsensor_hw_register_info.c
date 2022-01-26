/*
 * Copyright (C) 2017 Hoperun Inc.
 * Copyright (C) 2021 XiaoMi, Inc.
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
#include "imgsensor_cfg_table.h"
#include "imgsensor_hw_register_info.h"
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/workqueue.h>
#include <linux/init.h>
#include <linux/types.h>

#undef CONFIG_MTK_SMI_EXT
#ifdef CONFIG_MTK_SMI_EXT
#include "mmdvfs_mgr.h"
#endif
#ifdef CONFIG_OF
/* device tree */
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#ifdef CONFIG_COMPAT
#include <linux/compat.h>
#endif

#ifdef CONFIG_MTK_CCU
#include "ccu_inc.h"
#endif

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_camera_feature.h"
#include "kd_imgsensor_errcode.h"
#include "imgsensor_sensor_list.h"
#include "imgsensor_hw.h"
#include "imgsensor_i2c.h"
#include "imgsensor_proc.h"
#include "imgsensor.h"

#include <linux/hardware_info.h>  //客制化代码

//后摄
extern char main_cam_name[HARDWARE_MAX_ITEM_LONGTH];
extern char main_cam_moduleid[HARDWARE_MAX_ITEM_LONGTH];
extern char main_cam_sensorid[HARDWARE_MAX_ITEM_LONGTH];
extern char main_cam_fuseid[HARDWARE_MAX_ITEM_LONGTH];
//前摄
extern char sub_cam_name[HARDWARE_MAX_ITEM_LONGTH];
extern char sub_cam_moduleid[HARDWARE_MAX_ITEM_LONGTH];
extern char sub_cam_sensorid[HARDWARE_MAX_ITEM_LONGTH];
extern char sub_cam_fuseid[HARDWARE_MAX_ITEM_LONGTH];
//广角
extern char wide_cam_name[HARDWARE_MAX_ITEM_LONGTH];
extern char wide_cam_moduleid[HARDWARE_MAX_ITEM_LONGTH];
extern char wide_cam_sensorid[HARDWARE_MAX_ITEM_LONGTH];
extern char wide_cam_fuseid[HARDWARE_MAX_ITEM_LONGTH];
//微距
extern char macro_cam_name[HARDWARE_MAX_ITEM_LONGTH];
extern char macro_cam_moduleid[HARDWARE_MAX_ITEM_LONGTH];
extern char macro_cam_sensorid[HARDWARE_MAX_ITEM_LONGTH];
extern char macro_cam_fuseid[HARDWARE_MAX_ITEM_LONGTH];

#define MAX_VENDOR_COUNT 2
#define MAX_IMGSENSOR_NUM 5
#define HARDWARE_MODULEID_LONGTH 5
char main_hw_info[19]; //这几个数组大小根据项目会不同
char sub_hw_info[19];
char wide_hw_info[14];
char macro_hw_info[19];
typedef struct IMGSENSOR_HW_REGISTER_INFO {
	// IMGSENSOR_SENSOR_IDX psensor_idx;
	 char psensor_name[HARDWARE_MAX_ITEM_LONGTH];
	 u16 nameNumber;
	 char moduleid[HARDWARE_MODULEID_LONGTH];
	 MUINT32 sensorID;
	 char *hw_info_data;
	 u16 i2cId;
	imgsensor_hwinfo_get_func read_hwinfo;
} IMGSENSOR_HQ_REGISTER_INFO;

/************************************************************************
 * Inceasing camera HW info map
 ************************************************************************/
IMGSENSOR_HQ_REGISTER_INFO cam_hwinfo[MAX_IMGSENSOR_NUM][MAX_VENDOR_COUNT] = {
	{
	    {"s5kjn1sunny_mipi_raw_i", 22, "SUNNY", S5KJN1SUNNY_SENSOR_ID, main_hw_info, 0xA2, s5kjn1sunny_get_otpdata},
	    {"ov50c40ofilm_mipi_raw_ii", 24, "OFILM", OV50C40OFILM_SENSOR_ID, main_hw_info, 0xA2, ov50c40ofilm_get_otpdata},
	},
	{
	    {"ov16a1qofilm_mipi_raw_i", 22, "OFILM", OV16A1QOFILM_SENSOR_ID, sub_hw_info, 0xA2, ov16a1qofilm_get_otpdata},
	    {"ov16a1qqtech_mipi_raw_ii", 24, "QTECH", OV16A1QQTECH_SENSOR_ID, sub_hw_info, 0xA2, ov16a1qqtech_get_otpdata},
	},
	{
	    {"imx355sunny_mipi_raw_i", 22, "SUNNY", IMX355SUNNY_SENSOR_ID, wide_hw_info, 0xA0, imx355sunny_get_otpdata},
	    {"imx355ofilm_mipi_raw_ii", 23, "OFILM", IMX355OFILM_SENSOR_ID, wide_hw_info, 0xA0, imx355ofilm_get_otpdata},
	}
};

/************************************************************************
 * imgsensor_sensor_hw_register
 * register cam_info by sensorID
 * Todo :get moduleid  sensorid and fuseid by sensorID
 ************************************************************************/
MINT8 imgsensor_sensor_hw_register(struct IMGSENSOR_SENSOR *psensor, MUINT32 sensorID)
{
	static int search_index;
	MINT8 ret = 0;
	MINT8 fuseid_index = 0;
	int j = psensor->inst.sensor_idx;

	for (search_index = 0; search_index < MAX_VENDOR_COUNT; search_index++) {
		pr_info("%s read sensor %x's otpinfo ", __func__, sensorID);
		if (sensorID == cam_hwinfo[j][search_index].sensorID) {
				ret = cam_hwinfo[j][search_index].read_hwinfo(
				cam_hwinfo[j][search_index].hw_info_data, cam_hwinfo[j][search_index].i2cId);
				pr_info("%s j = %d  search_index = %d", __func__, j, search_index);
				break;
			}
	}
	switch(j) {
	case 0: {
		strncpy(main_cam_name,cam_hwinfo[j][search_index].psensor_name,cam_hwinfo[j][search_index].nameNumber+1);
		strncpy(main_cam_moduleid,cam_hwinfo[j][search_index].moduleid,5);
		main_cam_sensorid[0] = main_hw_info[0];
		main_cam_sensorid[1] = '\0';
		//main_cam_moduleid[0] = main_hw_info[1];
		//main_cam_moduleid[1] = main_hw_info[2];
		//main_cam_moduleid[2] = '\0';
		for(fuseid_index = 0 ; fuseid_index <= (ret-3) ; fuseid_index++ )
			main_cam_fuseid[fuseid_index] = main_hw_info[fuseid_index+3];
		main_cam_fuseid[ret-2] = '\0';
	}
		break;
	case 1: {
		strncpy(sub_cam_name,cam_hwinfo[j][search_index].psensor_name,cam_hwinfo[j][search_index].nameNumber+1);
		strncpy(sub_cam_moduleid,cam_hwinfo[j][search_index].moduleid,5);
		sub_cam_sensorid[0] = sub_hw_info[0];
		sub_cam_sensorid[1] = '\0';
		//sub_cam_moduleid[0] = sub_hw_info[1];
		//sub_cam_moduleid[1] = sub_hw_info[2];
		//sub_cam_moduleid[2] = '\0';
		for(fuseid_index = 0 ; fuseid_index <= (ret-3) ; fuseid_index++ )
			sub_cam_fuseid[fuseid_index] = sub_hw_info[fuseid_index+3];
		sub_cam_fuseid[ret-2] = '\0';
	}
		break;
	case 2: {
		strncpy(wide_cam_name,cam_hwinfo[j][search_index].psensor_name,cam_hwinfo[j][search_index].nameNumber+1);
		strncpy(wide_cam_moduleid,cam_hwinfo[j][search_index].moduleid,5);
		wide_cam_sensorid[0] = wide_hw_info[0];
		wide_cam_sensorid[1] = '\0';
		//wide_cam_moduleid[0] = wide_hw_info[1];
		//wide_cam_moduleid[1] = wide_hw_info[2];
		//wide_cam_moduleid[2] = '\0';
		for(fuseid_index = 0 ; fuseid_index <= (ret-3) ; fuseid_index++ )
			wide_cam_fuseid[fuseid_index] = wide_hw_info[fuseid_index+3];
		wide_cam_fuseid[ret-2] = '\0';
	}
		break;
	case 4: {
		strncpy(macro_cam_name,cam_hwinfo[j][search_index].psensor_name,cam_hwinfo[j][search_index].nameNumber+1);
		strncpy(macro_cam_moduleid,cam_hwinfo[j][search_index].moduleid,5);
		macro_cam_sensorid[0] = macro_hw_info[0];
		macro_cam_sensorid[1] = '\0';
		//macro_cam_moduleid[0] = macro_hw_info[1];
		//macro_cam_moduleid[1] = macro_hw_info[2];
		//macro_cam_moduleid[2] = '\0';
		for(fuseid_index = 0 ; fuseid_index <= (ret-3) ; fuseid_index++ )
			macro_cam_fuseid[fuseid_index] = macro_hw_info[fuseid_index+3];
		macro_cam_fuseid[ret-2] = '\0';
	}
		break;
	default:
		pr_info("%s sensor number too much", __func__);
		break;
	}

	return ret;
}
