/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef __CAM_CAL_LAYOUT_H
#define __CAM_CAL_LAYOUT_H

#include "eeprom_driver.h"
#include "kd_imgsensor.h"
#include "cam_cal_format.h"
#include "eeprom_utils.h"

/*****************************************************************************
 * Structures  custom
 *****************************************************************************/
#define CUSTOM_PART_NUMBER_SIZE  10
#define CUSTOM_SN_SIZE           16
#define CUSTOM_AF_DATA_SIZE      11
#define CUSTOM_AWB_DATA_SIZE     31
#define CUSTOM_AF_CAL_COEFF      64
#define CUSTOM_PDAF_PROC1_SIZE   496
#define CUSTOM_PDAF_PROC2_SIZE   1004
#define CUSTOM_PDAF_DATA_SIZE    (CUSTOM_PDAF_PROC1_SIZE + CUSTOM_PDAF_PROC2_SIZE)
#define CUSTOM_LSC_DATA_SIZE     1868

#define ENABLE_CHECK_SUM         1

struct STRUCT_CAM_CAL_MODULE_INFO {
    unsigned char flag;
    unsigned char part_number[CUSTOM_PART_NUMBER_SIZE];
    unsigned char supplier_id;
    unsigned char sensor_id;
    unsigned char lens_id;
    unsigned char vcm_id;
    unsigned char driverIC_id;
    unsigned char phase;
    unsigned char mirror_flip_status;
    unsigned char year;
    unsigned char month;
    unsigned char day;
    unsigned char SN[CUSTOM_SN_SIZE];
    unsigned char check_sum_h;
    unsigned char check_sum_l;
};

struct STRUCT_CAM_CAL_AWB_INFO {
    unsigned char flag;
    unsigned char golden_awb_r_h;
    unsigned char golden_awb_r_l;
    unsigned char golden_awb_gr_h;
    unsigned char golden_awb_gr_l;
    unsigned char golden_awb_gb_h;
    unsigned char golden_awb_gb_l;
    unsigned char golden_awb_b_h;
    unsigned char golden_awb_b_l;
    unsigned char golden_awb_rg_h;
    unsigned char golden_awb_rg_l;
    unsigned char golden_awb_bg_h;
    unsigned char golden_awb_bg_l;
    unsigned char golden_awb_grgb_h;
    unsigned char golden_awb_grgb_l;
    unsigned char awb_r_h;
    unsigned char awb_r_l;
    unsigned char awb_gr_h;
    unsigned char awb_gr_l;
    unsigned char awb_gb_h;
    unsigned char awb_gb_l;
    unsigned char awb_b_h;
    unsigned char awb_b_l;
    unsigned char awb_rg_h;
    unsigned char awb_rg_l;
    unsigned char awb_bg_h;
    unsigned char awb_bg_l;
    unsigned char awb_grgb_h;
    unsigned char awb_grgb_l;
    unsigned char check_sum_h;
    unsigned char check_sum_l;
};

struct STRUCT_CAM_CAL_AF_INFO {
    unsigned char flag;
    unsigned char af_hor_inf_h;
    unsigned char af_hor_inf_l;
    unsigned char af_hor_macro_h;
    unsigned char af_hor_macro_l;
    unsigned char af_scan_inf_h;
    unsigned char af_scan_inf_l;
    unsigned char af_scan_macro_h;
    unsigned char af_scan_macro_l;
    unsigned char check_sum_h;
    unsigned char check_sum_l;
};

struct STRUCT_CAM_CAL_PDAF_INFO {
    unsigned char flag;
    unsigned char pdaf_proc1[CUSTOM_PDAF_PROC1_SIZE];
    unsigned char pdaf_proc2[CUSTOM_PDAF_PROC2_SIZE];
    unsigned char check_proc1_sum_h;
    unsigned char check_proc1_sum_l;
    unsigned char check_proc2_sum_h;
    unsigned char check_proc2_sum_l;
};

struct STRUCT_CAM_CAL_LSC_INFO {
    unsigned char flag;
    unsigned char lsc_data[CUSTOM_LSC_DATA_SIZE];
    unsigned char check_sum_h;
    unsigned char check_sum_l;
};

/*****************************************************************************
 * Structures
 *****************************************************************************/

struct STRUCT_CALIBRATION_ITEM_STRUCT {
	unsigned short Include; //calibration layout include this item?
	unsigned int start_addr; // item Start Address
	unsigned int block_size;   //block_size
	unsigned int (*GetCalDataProcess)(struct EEPROM_DRV_FD_DATA *pdata,
			unsigned int start_addr, unsigned int block_size,
			unsigned int *pGetSensorCalData);
};

struct STRUCT_CALIBRATION_LAYOUT_STRUCT {
	unsigned int header_addr; //Header Address
	unsigned int header_id;   //Header ID
	unsigned int data_ver;
	struct STRUCT_CALIBRATION_ITEM_STRUCT cal_layout_tbl[CAMERA_CAM_CAL_DATA_LIST];
};

struct STRUCT_CAM_CAL_CONFIG_STRUCT {
	const char *name;
	unsigned int (*check_layout_function)(struct EEPROM_DRV_FD_DATA *pdata,
				unsigned int sensorID);
	unsigned int (*read_function)(struct i2c_client *client, unsigned int addr,
				unsigned char *data, unsigned int size);
	struct STRUCT_CALIBRATION_LAYOUT_STRUCT *layout;
	unsigned int sensor_id;
	unsigned int i2c_write_id;
	unsigned int max_size;
	unsigned int enable_preload;
	unsigned int preload_size;
	unsigned int has_stored_data;
};

unsigned int show_cmd_error_log(enum ENUM_CAMERA_CAM_CAL_TYPE_ENUM cmd);
int get_mtk_format_version(struct EEPROM_DRV_FD_DATA *pdata, unsigned int *pGetSensorCalData);

unsigned int layout_check(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int sensorID);
unsigned int layout_no_ck(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int sensorID);
unsigned int do_module_version(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int do_part_number(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int do_single_lsc(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int do_2a_gain(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int do_pdaf(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int do_stereo_data(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int do_dump_all(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int do_lens_id_base(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int do_lens_id(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);

unsigned int get_is_need_power_on(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int *pGetNeedPowerOn);
unsigned int get_cal_data(struct EEPROM_DRV_FD_DATA *pdata, unsigned int *pGetSensorCalData);
int read_data(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int sensor_id, unsigned int device_id,
		unsigned int offset, unsigned int length, unsigned char *data);
unsigned int read_data_region(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned char *buf, unsigned int offset, unsigned int size);

unsigned int custom_do_module_info(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int custom_do_2a_gain(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int custom_do_awb_gain(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int custom_do_pdaf(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int custom_do_single_lsc(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);
unsigned int custom_layout_check(struct EEPROM_DRV_FD_DATA *pdata,
				unsigned int sensorID);
#endif /* __CAM_CAL_LAYOUT_H */
