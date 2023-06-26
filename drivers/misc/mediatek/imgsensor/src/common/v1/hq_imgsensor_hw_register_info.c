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
#include "imgsensor_cfg_table.h"
#include "hq_imgsensor_hw_register_info.h"
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
#include <linux/hqsysfs.h>

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
#include "imgsensor_clk.h"
#include "imgsensor.h"


#define MAX_SENSOR_NAME_SIZE 30
#define MAX_HW_REG_NAME_SIZE 20
#define MAX_VENDOR_COUNT 2
#define MAX_IMGSENSOR_NUM 5
typedef struct HQ_IMGSENSOR_HQ_REGISTER_INFO {
	// IMGSENSOR_SENSOR_IDX psensor_idx;
	char psensor_name[MAX_SENSOR_NAME_SIZE];
	char hw_register_name[MAX_HW_REG_NAME_SIZE];
	enum hardware_id hw_register_id;
} HQ_IMGSENSOR_HQ_REGISTER_INFO;

/************************************************************************
 * Inceasing Sensor HW info map
 * Todo : front_main_camera_str_buff and rear_aux_0_camera_str_buff
 * should to be modified before factory P1 node. Need to add
 * < sunny_8856_i > < ofilm_8856_i >
 ************************************************************************/
#if defined(MERLIN_MSM_CAMERA_HW_INFO)
#define MAX_SENSOR_NAME_SIZE 30
#define MAX_HW_REG_NAME_SIZE 20
#undef  MAX_VENDOR_COUNT
#define MAX_VENDOR_COUNT 5
#define MAX_IMGSENSOR_NUM 5
HQ_IMGSENSOR_HQ_REGISTER_INFO cam_str_buff[MAX_IMGSENSOR_NUM][MAX_VENDOR_COUNT] = {
	{
		{"s5kgm1sp_mipi_raw", "ofilm_s5kgm1sp_i", HWID_SUB_CAM_2},
		{"s5kgm1sp_sunny_mipi_raw", "sunny_s5kgm1sp_ii", HWID_SUB_CAM_2},
	},
	{
		{"ov13b10_mipi_raw", "ofilm_ov13b10_i", HWID_SUB_CAM},
		{"ov13b10_sunny_mipi_raw", "sunny_ov13b10_ii", HWID_SUB_CAM},
	},
	{
		{"s5k4h7yx_mipi_raw", "ofilm_s5k4h7yx_i", HWID_MAIN_CAM},
		{"s5k4h7yx_sunny_mipi_raw", "sunny_s5k4h7yx_ii", HWID_MAIN_CAM},
	},
	{
		{"gc02m1_mipi_raw", "ofilm_gc02m1_i", HWID_MAIN_CAM_2},
		{"gc02m1_sunny_mipi_raw", "sunny_gc02m1_ii", HWID_MAIN_CAM_2},
		{"ov2180_mipi_raw", "ofilm_ov2180_i", HWID_MAIN_CAM_2},
		{"ov2180_sunny_mipi_raw", "sunny_ov2180_ii", HWID_MAIN_CAM_2},
	},
	{
		{"s5k5e9_mipi_raw", "ofilm_s5k5e9_i", HWID_MAIN_CAM_3},
		{"s5k5e9_sunny_mipi_raw", "sunny_s5k5e9_ii", HWID_MAIN_CAM_3},
		{"gc02m1_macro_mipi_raw", "ofilm_gc02m1_i", HWID_MAIN_CAM_3},
		{"gc02m1_macro_sunny_mipi_raw", "sunny_gc02m1_ii", HWID_MAIN_CAM_3},
		{"ov2680_sunny_mipi_raw", "sunny_ov2680_ii", HWID_MAIN_CAM_3},
	}
};

#elif defined(LANCELOT_MSM_CAMERA_HW_INFO) || defined(GALAHAD_MSM_CAMERA_HW_INFO) || defined(SELENE_MSM_CAMERA_HW_INFO)
#define MAX_SENSOR_NAME_SIZE 30
#define MAX_HW_REG_NAME_SIZE 20
#undef  MAX_VENDOR_COUNT
#define MAX_VENDOR_COUNT 3
#define MAX_IMGSENSOR_NUM 5
HQ_IMGSENSOR_HQ_REGISTER_INFO cam_str_buff[MAX_IMGSENSOR_NUM][MAX_VENDOR_COUNT] = {
	{
		{"ov13b10_ofilm_mipi_raw", "ofilm_ov13b10_i", HWID_SUB_CAM_2},
		{"ov13b10_qtech_mipi_raw", "qtech_ov13b10_ii", HWID_SUB_CAM_2},
		{"s5k3l6_qtech_mipi_raw", "qtech_s5k3l6_ii", HWID_SUB_CAM_2},
	},
	{
		{"s5k4h7yx_ofilm_front_mipi_raw", "ofilm_s5k4h7yx_i", HWID_SUB_CAM},
		{"ov8856_qtech_front_mipi_raw", "qtech_ov8856_ii", HWID_SUB_CAM},
	},
	{
		{"s5k4h7yx_ofilm_ultra_mipi_raw", "ofilm_s5k4h7yx_i", HWID_MAIN_CAM},
		{"ov8856_qtech_ultra_mipi_raw", "qtech_ov8856_ii", HWID_MAIN_CAM},
	},
	{
		{"ov2180_ofilm_mipi_raw", "ofilm_ov2180_i", HWID_MAIN_CAM_2},
		{"ov2180_qtech_mipi_raw", "qtech_ov2180_ii", HWID_MAIN_CAM_2},
	},
	{
		{"gc5035_ofilm_mipi_raw", "ofilm_gc5035_i", HWID_MAIN_CAM_3},
		{"gc5035_qtech_mipi_raw", "qtech_gc5035_ii", HWID_MAIN_CAM_3},
	}
};

#elif defined(SHIVA_MSM_CAMERA_HW_INFO) || defined(SHIVA_MSM_CAMERA_HW_INFO)
#define MAX_SENSOR_NAME_SIZE 30
#define MAX_HW_REG_NAME_SIZE 20
#undef  MAX_VENDOR_COUNT
#define MAX_VENDOR_COUNT 3
#define MAX_IMGSENSOR_NUM 5
HQ_IMGSENSOR_HQ_REGISTER_INFO cam_str_buff[MAX_IMGSENSOR_NUM][MAX_VENDOR_COUNT] = {
	{
		{"ov13b10_ofilm_mipi_raw", "ofilm_ov13b10_i", HWID_SUB_CAM_2},
		{"ov13b10_qtech_mipi_raw", "qtech_ov13b10_ii", HWID_SUB_CAM_2},
		{"s5k3l6_qtech_mipi_raw", "qtech_s5k3l6_ii", HWID_SUB_CAM_2},
	},
	{
		{"s5k4h7yx_ofilm_front_mipi_raw", "ofilm_s5k4h7yx_i", HWID_SUB_CAM},
		{"ov8856_qtech_front_mipi_raw", "qtech_ov8856_ii", HWID_SUB_CAM},
	},
	{
		{"s5k4h7yx_ofilm_ultra_mipi_raw", "ofilm_s5k4h7yx_i", HWID_MAIN_CAM},
		{"ov8856_qtech_ultra_mipi_raw", "qtech_ov8856_ii", HWID_MAIN_CAM},
	},
	{
		{"ov2180_ofilm_mipi_raw", "ofilm_ov2180_i", HWID_MAIN_CAM_2},
		{"ov2180_qtech_mipi_raw", "qtech_ov2180_ii", HWID_MAIN_CAM_2},
	},
	{
		{"gc5035_ofilm_mipi_raw", "ofilm_gc5035_i", HWID_MAIN_CAM_3},
		{"gc5035_qtech_mipi_raw", "qtech_gc5035_ii", HWID_MAIN_CAM_3},
	}
};

#endif

/************************************************************************
 * hq_imgsensor_sensor_hw_register
 * register camera sensor Name to /sys/class/huaqin/interface/hw_info/
 * Todo : check eeprome name if matched after timepoint < eeprome is done >
 ************************************************************************/
MINT8 hq_imgsensor_sensor_hw_register(struct IMGSENSOR_SENSOR *psensor, struct IMGSENSOR_SENSOR_INST *psensor_inst)
{
	static int search_index;
	MINT8 ret = 0;
	int j = psensor->inst.sensor_idx;

	for (search_index = 0; search_index < MAX_VENDOR_COUNT; search_index++) {
		pr_info("%s %s is %s", __func__, psensor_inst->psensor_name,
		cam_str_buff[j][search_index].psensor_name);
		if (strncmp(
				(char *)(psensor_inst->psensor_name),
				cam_str_buff[j][search_index].psensor_name,
				strlen(psensor_inst->psensor_name)) == 0) {
				ret = hq_regiser_hw_info(cam_str_buff[j][search_index].hw_register_id,
				cam_str_buff[j][search_index].hw_register_name);
				pr_info("%s j = %d  search_index = %d", __func__, j, search_index);
				break;
			}
	}
	pr_err("%s %s is %s", __func__,
	psensor_inst->psensor_name, cam_str_buff[j][search_index].psensor_name);
	return ret;
}
