// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 * Copyright (C) 2022 XiaoMi, Inc.
 */

#define PFX "CAM_CAL_XIAOMI"

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/of.h>
#include <linux/compat.h>

#include "kd_camera_feature.h"
#include "cam_cal_config.h"
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"

#define CAM_CAL_LOG_ERR(format, args...) pr_err(PFX "[%s] " format, __func__, ##args)
#define CAM_CAL_LOG_INF(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)
#define CAM_CAL_LOG_DBG(format, args...) pr_info(PFX "[%s] " format, __func__, ##args)


unsigned int xiaomi_do_module_version(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];
	unsigned int size_limit = 0x10;
	unsigned char cal_module_version[16] = {0};

	if (block_size > size_limit) {
		CAM_CAL_LOG_ERR("module version size can't larger than %u\n", size_limit);
		return err;
	}

	if (read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, block_size, cal_module_version) > 0)
		err = CAM_CAL_ERR_NO_ERR;
	else {
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	CAM_CAL_LOG_DBG("======================Module version==================\n");
	CAM_CAL_LOG_DBG("[VENDOR ID] = 0x%x\n", cal_module_version[0]);
	CAM_CAL_LOG_DBG("[YY/MM/DD] = %d/%d/%d \n", cal_module_version[1], cal_module_version[2], cal_module_version[3]);
	CAM_CAL_LOG_DBG("[HH/MM/SEC] = %d:%d:%d \n", cal_module_version[4], cal_module_version[5], cal_module_version[6]);
	CAM_CAL_LOG_DBG("[LENS_ID/VCMID/DRIVE ID] = 0x%x/0x%x/0x%x \n", cal_module_version[7], cal_module_version[8], cal_module_version[9]);
	CAM_CAL_LOG_DBG("[OIS_VER/SENSOR ID/Product ID] = 0x%x/0x%x/0x%x \n", cal_module_version[0xA], cal_module_version[0xB], cal_module_version[0xC]);
	CAM_CAL_LOG_DBG("======================Module version==================\n");

	return CAM_CAL_ERR_NO_ERR;
}

unsigned int xiaomi_do_part_number(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];
	unsigned int size_limit = sizeof(pCamCalData->PartNumber);
	unsigned char vendor_id = 0xFF;

	memset(&pCamCalData->PartNumber[0], 0, size_limit);

	if (block_size > size_limit) {
		CAM_CAL_LOG_ERR("part number size can't larger than %u\n", size_limit);
		return err;
	}

	if (read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, block_size, (unsigned char *)&pCamCalData->PartNumber[0]) > 0)
		err = CAM_CAL_ERR_NO_ERR;
	else {
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID, 0x01, 1, &vendor_id);
	pCamCalData->PartNumber[0] = vendor_id;
	pCamCalData->PartNumber[1] = 0;

	CAM_CAL_LOG_DBG("======================Part Number==================\n");
	CAM_CAL_LOG_DBG("[Part Number] = %x %x %x %x\n",
			pCamCalData->PartNumber[0], pCamCalData->PartNumber[1],
			pCamCalData->PartNumber[2], pCamCalData->PartNumber[3]);
	CAM_CAL_LOG_DBG("[Part Number] = %x %x %x %x\n",
			pCamCalData->PartNumber[4], pCamCalData->PartNumber[5],
			pCamCalData->PartNumber[6], pCamCalData->PartNumber[7]);
	CAM_CAL_LOG_DBG("[Part Number] = %x %x %x %x\n",
			pCamCalData->PartNumber[8], pCamCalData->PartNumber[9],
			pCamCalData->PartNumber[10], pCamCalData->PartNumber[11]);
	CAM_CAL_LOG_DBG("======================Part Number==================\n");

	return err;
}





#define AWB_FLAG         0x750
#ifdef XAGA_CAM
#define AWB_757_FLAG         0x757
#endif
#define AF_FLAG          0x27

#define AF_MAC_DIS_ADDR  0x2B
#define AF_MAC_DAC_ADDR  0x2D
#define AF_INF_DIS_ADDR  0x2F
#define AF_INF_DAC_ADDR  0x31

unsigned int xiaomi_do_2a_gain(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	int read_data_size;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];

    char cal_awb_gain[16] = {0};
    unsigned short AFInf = 0, AFMacro = 0;
    unsigned short AFInfDistance = 0, AFMacroDistance = 0;

	unsigned char AWBConfig;
	unsigned char AFConfig;

	int CalR = 1, CalGr = 1, CalGb = 1, CalB = 1;
	int FacR = 1, FacGr = 1, FacGb = 1, FacB = 1;


	CAM_CAL_LOG_DBG("block_size=%d sensor_id=%x\n", block_size, pCamCalData->sensorID);
	memset((void *)&pCamCalData->Single2A, 0, sizeof(struct STRUCT_CAM_CAL_SINGLE_2A_STRUCT));

	/* Check rule */
	if (pCamCalData->DataVer >= CAM_CAL_TYPE_NUM) {
		err = CAM_CAL_ERR_NO_DEVICE;
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;

	/* get AWB enable bit */
	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			AWB_FLAG, 1, (unsigned char *)&AWBConfig);
	if (read_data_size > 0) {
		err = CAM_CAL_ERR_NO_ERR;
	} else {
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	/* check AWB enable bit */
	if (0x1 == AWBConfig) {
		pCamCalData->Single2A.S2aVer = 0x01;
		pCamCalData->Single2A.S2aBitEn |= CAM_CAL_AWB_BITEN;

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				start_addr, block_size, cal_awb_gain);

		CalR  = (cal_awb_gain[0] << 8) | cal_awb_gain[1];
		CalGr = (cal_awb_gain[2] << 8) | cal_awb_gain[3];
		CalGb = (cal_awb_gain[4] << 8) | cal_awb_gain[5];
		CalB  = (cal_awb_gain[6] << 8) | cal_awb_gain[7];
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : CalR,CalGr,CalGb,CalB=0x%x,0x%x,0x%x,0x%x\n",
			CalR, CalGr, CalGb, CalB);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain: UnitR:0x%x, UnitGr:0x%x, UnitGb:0x%x, UnitB=0x%x",
			cal_awb_gain[1], cal_awb_gain[3], cal_awb_gain[5], cal_awb_gain[7]);


		pCamCalData->Single2A.S2aAwb.rUnitGainu4R = (unsigned int)(1023*CalGr/CalR);
		pCamCalData->Single2A.S2aAwb.rUnitGainu4G = 1023;
		pCamCalData->Single2A.S2aAwb.rUnitGainu4B = (unsigned int)(1023*CalGr/CalB);

		FacR  = (cal_awb_gain[8]  << 8) | cal_awb_gain[9];
		FacGr = (cal_awb_gain[10] << 8) | cal_awb_gain[11];
		FacGb = (cal_awb_gain[12] << 8) | cal_awb_gain[13];
		FacB  = (cal_awb_gain[14] << 8) | cal_awb_gain[15];

		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : FacR,FacGr,FacGb,FacB=0x%x,0x%x,0x%x,0x%x\n",
			FacR, FacGr, FacGb, FacB);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : GoldenR:0x%x, GoldenGr:0x%x, GoldenGb:0x%x, GoldenB=0x%x\n",
			cal_awb_gain[9], cal_awb_gain[11], cal_awb_gain[13], cal_awb_gain[15]);

		pCamCalData->Single2A.S2aAwb.rGoldGainu4R = (unsigned int)(1023*FacGr/FacR);
		pCamCalData->Single2A.S2aAwb.rGoldGainu4G = 1023;
		pCamCalData->Single2A.S2aAwb.rGoldGainu4B = (unsigned int)(1023*FacGr/FacB);

		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : ======================AWB CAM_CAL==================\n");
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rCalGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4R);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rCalGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4G);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rCalGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4B);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rFacGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4R);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rFacGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4G);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rFacGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4B);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : ======================AWB CAM_CAL==================\n");
	}


	/* get AF enable bit */
	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			AF_FLAG, 1, (unsigned char *)&AFConfig);
	if (read_data_size > 0) {
		err = CAM_CAL_ERR_NO_ERR;
	} else {
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	/* check AF enable bit */
	if (0x1 == AFConfig) {
		pCamCalData->Single2A.S2aVer = 0x01;
		pCamCalData->Single2A.S2aBitEn |= CAM_CAL_AF_BITEN;

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				AF_MAC_DIS_ADDR, 2, (unsigned char *)&AFMacroDistance);

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				AF_MAC_DAC_ADDR, 2, (unsigned char *)&AFMacro);

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				AF_INF_DIS_ADDR, 2, (unsigned char *)&AFInfDistance);

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				AF_INF_DAC_ADDR, 2, (unsigned char *)&AFInf);

		AFInf   = 0xffff & ((AFInf   << 8) | (AFInf   >> 8));
		AFMacro = 0xffff & ((AFMacro << 8) | (AFMacro >> 8));

		AFInfDistance   = 0xffff & ((AFInfDistance   << 8) | (AFInfDistance   >> 8));
		AFMacroDistance = 0xffff & ((AFMacroDistance << 8) | (AFMacroDistance >> 8));

		pCamCalData->Single2A.S2aAf[0] = AFInf;
		pCamCalData->Single2A.S2aAf[1] = AFMacro;
		pCamCalData->Single2A.S2aAF_t.AF_infinite_pattern_distance = AFInfDistance * 10;
		pCamCalData->Single2A.S2aAF_t.AF_Macro_pattern_distance    = AFMacroDistance * 10;

		CAM_CAL_LOG_DBG("======================AF CAM_CAL==================\n");
		CAM_CAL_LOG_DBG("[AFInf] = %d\n", AFInf);
		CAM_CAL_LOG_DBG("[AFMacro] = %d\n", AFMacro);
		CAM_CAL_LOG_DBG("[AFInfDistance] = %d\n", pCamCalData->Single2A.S2aAF_t.AF_infinite_pattern_distance);
		CAM_CAL_LOG_DBG("[AFMacroDistance] = %d\n", pCamCalData->Single2A.S2aAF_t.AF_Macro_pattern_distance);
		CAM_CAL_LOG_DBG("======================AF CAM_CAL==================\n");

	}

	CAM_CAL_LOG_DBG("S2aBitEn=0x%02x", pCamCalData->Single2A.S2aBitEn);

	return err;
}

#ifdef XAGA_CAM
unsigned int xiaomi_do_2a_gain_else(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	int read_data_size;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];

    char cal_awb_gain[16] = {0};
    unsigned short AFInf = 0, AFMacro = 0;
    unsigned short AFInfDistance = 0, AFMacroDistance = 0;

	unsigned char AWBConfig;
	unsigned char AFConfig;

	int CalR = 1, CalGr = 1, CalGb = 1, CalB = 1;
	int FacR = 1, FacGr = 1, FacGb = 1, FacB = 1;


	CAM_CAL_LOG_DBG("block_size=%d sensor_id=%x\n", block_size, pCamCalData->sensorID);
	memset((void *)&pCamCalData->Single2A, 0, sizeof(struct STRUCT_CAM_CAL_SINGLE_2A_STRUCT));

	/* Check rule */
	if (pCamCalData->DataVer >= CAM_CAL_TYPE_NUM) {
		err = CAM_CAL_ERR_NO_DEVICE;
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;

	/* get AWB enable bit */
	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			AWB_757_FLAG, 1, (unsigned char *)&AWBConfig);
	if (read_data_size > 0) {
		err = CAM_CAL_ERR_NO_ERR;
	} else {
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	/* check AWB enable bit */
	if (0x1 == AWBConfig) {
		pCamCalData->Single2A.S2aVer = 0x01;
		pCamCalData->Single2A.S2aBitEn |= CAM_CAL_AWB_BITEN;

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				start_addr, block_size, cal_awb_gain);

		CalR  = (cal_awb_gain[0] << 8) | cal_awb_gain[1];
		CalGr = (cal_awb_gain[2] << 8) | cal_awb_gain[3];
		CalGb = (cal_awb_gain[4] << 8) | cal_awb_gain[5];
		CalB  = (cal_awb_gain[6] << 8) | cal_awb_gain[7];
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : CalR,CalGr,CalGb,CalB=0x%x,0x%x,0x%x,0x%x\n",
			CalR, CalGr, CalGb, CalB);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain: UnitR:0x%x, UnitGr:0x%x, UnitGb:0x%x, UnitB=0x%x",
			cal_awb_gain[1], cal_awb_gain[3], cal_awb_gain[5], cal_awb_gain[7]);


		pCamCalData->Single2A.S2aAwb.rUnitGainu4R = (unsigned int)(1023*CalGr/CalR);
		pCamCalData->Single2A.S2aAwb.rUnitGainu4G = 1023;
		pCamCalData->Single2A.S2aAwb.rUnitGainu4B = (unsigned int)(1023*CalGr/CalB);

		FacR  = (cal_awb_gain[8]  << 8) | cal_awb_gain[9];
		FacGr = (cal_awb_gain[10] << 8) | cal_awb_gain[11];
		FacGb = (cal_awb_gain[12] << 8) | cal_awb_gain[13];
		FacB  = (cal_awb_gain[14] << 8) | cal_awb_gain[15];

		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : FacR,FacGr,FacGb,FacB=0x%x,0x%x,0x%x,0x%x\n",
			FacR, FacGr, FacGb, FacB);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : GoldenR:0x%x, GoldenGr:0x%x, GoldenGb:0x%x, GoldenB=0x%x\n",
			cal_awb_gain[9], cal_awb_gain[11], cal_awb_gain[13], cal_awb_gain[15]);

		pCamCalData->Single2A.S2aAwb.rGoldGainu4R = (unsigned int)(1023*FacGr/FacR);
		pCamCalData->Single2A.S2aAwb.rGoldGainu4G = 1023;
		pCamCalData->Single2A.S2aAwb.rGoldGainu4B = (unsigned int)(1023*FacGr/FacB);

		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : ======================AWB CAM_CAL==================\n");
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rCalGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4R);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rCalGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4G);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rCalGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4B);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rFacGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4R);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rFacGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4G);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : [rFacGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4B);
		CAM_CAL_LOG_DBG("XiaoMiCamCalAWBGain : ======================AWB CAM_CAL==================\n");
	}


	/* get AF enable bit */
	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			AF_FLAG, 1, (unsigned char *)&AFConfig);
	if (read_data_size > 0) {
		err = CAM_CAL_ERR_NO_ERR;
	} else {
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	/* check AF enable bit */
	if (0x1 == AFConfig) {
		pCamCalData->Single2A.S2aVer = 0x01;
		pCamCalData->Single2A.S2aBitEn |= CAM_CAL_AF_BITEN;

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				AF_MAC_DIS_ADDR, 2, (unsigned char *)&AFMacroDistance);

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				AF_MAC_DAC_ADDR, 2, (unsigned char *)&AFMacro);

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				AF_INF_DIS_ADDR, 2, (unsigned char *)&AFInfDistance);

		read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				AF_INF_DAC_ADDR, 2, (unsigned char *)&AFInf);

		AFInf   = 0xffff & ((AFInf   << 8) | (AFInf   >> 8));
		AFMacro = 0xffff & ((AFMacro << 8) | (AFMacro >> 8));

		AFInfDistance   = 0xffff & ((AFInfDistance   << 8) | (AFInfDistance   >> 8));
		AFMacroDistance = 0xffff & ((AFMacroDistance << 8) | (AFMacroDistance >> 8));

		pCamCalData->Single2A.S2aAf[0] = AFInf;
		pCamCalData->Single2A.S2aAf[1] = AFMacro;
		pCamCalData->Single2A.S2aAF_t.AF_infinite_pattern_distance = AFInfDistance * 10;
		pCamCalData->Single2A.S2aAF_t.AF_Macro_pattern_distance    = AFMacroDistance * 10;

		CAM_CAL_LOG_DBG("======================AF CAM_CAL==================\n");
		CAM_CAL_LOG_DBG("[AFInf] = %d\n", AFInf);
		CAM_CAL_LOG_DBG("[AFMacro] = %d\n", AFMacro);
		CAM_CAL_LOG_DBG("[AFInfDistance] = %d\n", pCamCalData->Single2A.S2aAF_t.AF_infinite_pattern_distance);
		CAM_CAL_LOG_DBG("[AFMacroDistance] = %d\n", pCamCalData->Single2A.S2aAF_t.AF_Macro_pattern_distance);
		CAM_CAL_LOG_DBG("======================AF CAM_CAL==================\n");

	}

	CAM_CAL_LOG_DBG("S2aBitEn=0x%02x", pCamCalData->Single2A.S2aBitEn);

	return err;
}
#endif


/***********************************************************************************
 * Function : To read LSC Table
 ***********************************************************************************/
unsigned int xiaomi_do_single_lsc(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	int read_data_size;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];
	unsigned short table_size = 1868;

	if (pCamCalData->DataVer >= CAM_CAL_TYPE_NUM) {
		err = CAM_CAL_ERR_NO_DEVICE;
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	if (block_size != CAM_CAL_SINGLE_LSC_SIZE)
		CAM_CAL_LOG_ERR("block_size(%d) is not match (%d)\n",
				block_size, CAM_CAL_SINGLE_LSC_SIZE);

	pCamCalData->SingleLsc.LscTable.MtkLcsData.MtkLscType = 2;//mtk type
	pCamCalData->SingleLsc.LscTable.MtkLcsData.PixId = 8;


	CAM_CAL_LOG_DBG("lsc table_size %d\n", table_size);
	pCamCalData->SingleLsc.LscTable.MtkLcsData.TableSize = table_size;
	if (table_size > 0) {
		pCamCalData->SingleLsc.TableRotation = 0;
#if defined(MATISSE_CAM) || defined(RUBENS_CAM)
	if (0xfad2 == pCamCalData->sensorID || 0x0582 == pCamCalData->sensorID)
		pCamCalData->SingleLsc.TableRotation = 1; //Because the module made the mirror and flip
#endif
		CAM_CAL_LOG_DBG("u4Offset=%d u4Length=%d", start_addr, table_size);
		CAM_CAL_LOG_DBG("sensorID : 0x%x TableRotation : %d\n", pCamCalData->sensorID,
			pCamCalData->SingleLsc.TableRotation);

		read_data_size = read_data(pdata,
			pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, table_size, (unsigned char *)
			&pCamCalData->SingleLsc.LscTable.MtkLcsData.SlimLscType);
		if (table_size == read_data_size)
			err = CAM_CAL_ERR_NO_ERR;
		else {
			CAM_CAL_LOG_ERR("Read Failed\n");
			err = CamCalReturnErr[pCamCalData->Command];
			show_cmd_error_log(pCamCalData->Command);
			return err;
		}
	}

	CAM_CAL_LOG_DBG("======================SingleLsc Data==================\n");
	CAM_CAL_LOG_DBG("[1st] = %x, %x, %x, %x\n",
		pCamCalData->SingleLsc.LscTable.Data[0],
		pCamCalData->SingleLsc.LscTable.Data[1],
		pCamCalData->SingleLsc.LscTable.Data[2],
		pCamCalData->SingleLsc.LscTable.Data[3]);
	CAM_CAL_LOG_DBG("[1st] = SensorLSC(1)?MTKLSC(2)?  %x\n",
		pCamCalData->SingleLsc.LscTable.MtkLcsData.MtkLscType);
	CAM_CAL_LOG_DBG("CapIspReg =0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[0],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[1],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[2],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[3],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[4]);
	CAM_CAL_LOG_DBG("RETURN = 0x%x\n", err);
	CAM_CAL_LOG_DBG("======================SingleLsc Data==================\n");

	return err;
}

unsigned int xiaomi_do_pdaf(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	int read_data_size;
	int err =  CamCalReturnErr[pCamCalData->Command];

    (void) start_addr;
    (void) block_size;

	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, 496, (unsigned char *)&pCamCalData->PDAF.Data[0]);
	if (read_data_size > 0) {
		pCamCalData->PDAF.Size_of_PDAF = 496;
		err = CAM_CAL_ERR_NO_ERR;
	} else {
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}

	read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			(start_addr + pCamCalData->PDAF.Size_of_PDAF + 2), 1004,
			(u8 *)&pCamCalData->PDAF.Data[496]);

	pCamCalData->PDAF.Size_of_PDAF = 1500;

	CAM_CAL_LOG_DBG("======================PDAF Data==================\n");
	CAM_CAL_LOG_DBG("First five %x, %x, %x, %x, %x\n",
		pCamCalData->PDAF.Data[0],
		pCamCalData->PDAF.Data[1],
		pCamCalData->PDAF.Data[2],
		pCamCalData->PDAF.Data[3],
		pCamCalData->PDAF.Data[4]);
	CAM_CAL_LOG_DBG("RETURN = 0x%x\n", err);
	CAM_CAL_LOG_DBG("======================PDAF Data==================\n");

	return err;

}

unsigned int xiaomi_do_stereo_data(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	int read_data_size;
	unsigned int err =  0;
	char Stereo_Data[1360];

	CAM_CAL_LOG_DBG("DoCamCal_Stereo_Data sensorID = %x\n", pCamCalData->sensorID);
	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, block_size, (unsigned char *)Stereo_Data);
	if (read_data_size > 0)
		err = CAM_CAL_ERR_NO_ERR;
	else {
		CAM_CAL_LOG_ERR("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
	}

	CAM_CAL_LOG_DBG("======================DoCamCal_Stereo_Data==================\n");
	CAM_CAL_LOG_DBG("======================DoCamCal_Stereo_Data==================\n");

	return err;
}

unsigned int xiaomi_do_dump_all(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	(void) pdata;
	(void) start_addr;
	(void) block_size;
	(void) pGetSensorCalData;

	return CAM_CAL_ERR_NO_ERR;
}

