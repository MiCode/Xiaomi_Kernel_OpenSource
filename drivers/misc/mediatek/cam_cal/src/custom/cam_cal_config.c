// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

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

#define IDX_MAX_CAM_NUMBER 7 // refer to IHalsensor.h
#define MAX_EEPROM_LIST_NUMBER 32

#undef E
#define E(__x__) (__x__)
extern struct STRUCT_CAM_CAL_CONFIG_STRUCT CAM_CAL_CONFIG_LIST;
#undef E
#define E(__x__) (&__x__)

/****************************************************************
 * Global variable
 ****************************************************************/

static struct STRUCT_CAM_CAL_CONFIG_STRUCT *cam_cal_config_list[] = {CAM_CAL_CONFIG_LIST};
static struct STRUCT_CAM_CAL_CONFIG_STRUCT *cam_cal_config;
static unsigned int last_sensor_id = 0xFFFFFFFF;
static unsigned short cam_cal_index = 0xFFFF;
static unsigned short cam_cal_number =
		sizeof(cam_cal_config_list)/sizeof(struct STRUCT_CAM_CAL_CONFIG_STRUCT *);

static unsigned char *mp_eeprom_preload[IDX_MAX_CAM_NUMBER];
static unsigned char *mp_layout_preload[IDX_MAX_CAM_NUMBER];

unsigned int show_cmd_error_log(enum ENUM_CAMERA_CAM_CAL_TYPE_ENUM cmd)
{
	error_log("Return %s\n", CamCalErrString[cmd]);
	return 0;
}

int get_mtk_format_version(struct EEPROM_DRV_FD_DATA *pdata, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	int ret = 0;

	if (read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				0xFA3, 1, (unsigned char *) &ret) > 0)
		debug_log("MTK format version = 0x%02x\n", ret);
	else {
		error_log("Read Failed\n");
		ret = -1;
	}

	return ret;
}

unsigned int layout_check(struct EEPROM_DRV_FD_DATA *pdata,
				unsigned int sensorID)
{
	unsigned int header_offset = cam_cal_config->layout->header_addr;
	unsigned int check_id = 0x00000000;
	unsigned int result = CAM_CAL_ERR_NO_DEVICE;

	if (cam_cal_config->sensor_id == sensorID)
		debug_log("%s sensor_id matched\n", cam_cal_config->name);
	else {
		debug_log("%s sensor_id not matched\n", cam_cal_config->name);
		return result;
	}

	if (read_data_region(pdata, (u8 *)&check_id, header_offset, 4) != 4) {
		debug_log("header_id read failed\n");
		return result;
	}

	if (check_id == cam_cal_config->layout->header_id) {
		debug_log("header_id matched 0x%08x 0x%08x\n",
			check_id, cam_cal_config->layout->header_id);
		result = CAM_CAL_ERR_NO_ERR;
	} else
		debug_log("header_id not matched 0x%08x 0x%08x\n",
			check_id, cam_cal_config->layout->header_id);

	return result;
}

unsigned int layout_no_ck(struct EEPROM_DRV_FD_DATA *pdata,
				unsigned int sensorID)
{
	unsigned int header_offset = cam_cal_config->layout->header_addr;
	unsigned int check_id = 0x00000000;
	unsigned int result = CAM_CAL_ERR_NO_DEVICE;

	if (cam_cal_config->sensor_id == sensorID)
		debug_log("%s sensor_id matched\n", cam_cal_config->name);
	else {
		debug_log("%s sensor_id not matched\n", cam_cal_config->name);
		return result;
	}

	if (read_data_region(pdata, (u8 *)&check_id, header_offset, 4) != 4) {
		debug_log("header_id read failed 0x%08x 0x%08x (forced in)\n",
			check_id, cam_cal_config->layout->header_id);
		return result;
	}

	debug_log("header_id = 0x%08x\n", check_id);

	if (check_id == cam_cal_config->layout->header_id) {
		debug_log("header_id matched 0x%08x 0x%08x (skipped out)\n",
			check_id, cam_cal_config->layout->header_id);
	} else {
		debug_log("header_id not matched 0x%08x 0x%08x (forced in)\n",
			check_id, cam_cal_config->layout->header_id);
		result = CAM_CAL_ERR_NO_ERR;
	}

	result = CAM_CAL_ERR_NO_ERR;
	return result;
}

unsigned int do_module_version(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	(void) pdata;
	(void) start_addr;
	(void) block_size;
	(void) pGetSensorCalData;

	return CAM_CAL_ERR_NO_ERR;
}

unsigned int do_part_number(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];
	unsigned int size_limit = sizeof(pCamCalData->PartNumber);

	memset(&pCamCalData->PartNumber[0], 0, size_limit);

	if (block_size > size_limit) {
		error_log("part number size can't larger than %u\n", size_limit);
		return err;
	}

	if (read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, block_size, (unsigned char *)&pCamCalData->PartNumber[0]) > 0)
		err = CAM_CAL_ERR_NO_ERR;
	else {
		error_log("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
	}

	debug_log("======================Part Number==================\n");
	debug_log("[Part Number] = %x %x %x %x\n",
			pCamCalData->PartNumber[0], pCamCalData->PartNumber[1],
			pCamCalData->PartNumber[2], pCamCalData->PartNumber[3]);
	debug_log("[Part Number] = %x %x %x %x\n",
			pCamCalData->PartNumber[4], pCamCalData->PartNumber[5],
			pCamCalData->PartNumber[6], pCamCalData->PartNumber[7]);
	debug_log("[Part Number] = %x %x %x %x\n",
			pCamCalData->PartNumber[8], pCamCalData->PartNumber[9],
			pCamCalData->PartNumber[10], pCamCalData->PartNumber[11]);
	debug_log("======================Part Number==================\n");

	return err;
}


/***********************************************************************************
 * Function : To read 2A information. Please put your AWB+AF data function, here.
 ***********************************************************************************/

unsigned int do_2a_gain(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	int read_data_size;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];

	unsigned int CalGain = 0, FacGain = 0;
	unsigned char AWBAFConfig = 0;

	unsigned short AFInf = 0, AFMacro = 0;
	int tempMax = 0;
	int CalR = 1, CalGr = 1, CalGb = 1, CalG = 1, CalB = 1;
	int FacR = 1, FacGr = 1, FacGb = 1, FacG = 1, FacB = 1;

	debug_log("block_size=%d sensor_id=%x\n", block_size, pCamCalData->sensorID);
	memset((void *)&pCamCalData->Single2A, 0, sizeof(struct STRUCT_CAM_CAL_SINGLE_2A_STRUCT));
	/* Check rule */
	if (pCamCalData->DataVer >= CAM_CAL_TYPE_NUM) {
		err = CAM_CAL_ERR_NO_DEVICE;
		error_log("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}
	if (block_size != 14) {
		error_log("block_size(%d) is not correct (%d)\n", block_size, 14);
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}
	/* Check AWB & AF enable bit */
	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr + 1, 1, (unsigned char *)&AWBAFConfig);
	if (read_data_size > 0)
		err = CAM_CAL_ERR_NO_ERR;
	else {
		pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
		error_log("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
	}
	pCamCalData->Single2A.S2aVer = 0x01;
	pCamCalData->Single2A.S2aBitEn = (0x03 & AWBAFConfig);
	debug_log("S2aBitEn=0x%02x", pCamCalData->Single2A.S2aBitEn);
	if (get_mtk_format_version(pdata, pGetSensorCalData) >= 0x18)
		if (0x2 & AWBAFConfig)
			pCamCalData->Single2A.S2aAfBitflagEn = 0x0C;
		else
			pCamCalData->Single2A.S2aAfBitflagEn = 0x00;
	else
		pCamCalData->Single2A.S2aAfBitflagEn = (0x0C & AWBAFConfig);
	/* AWB Calibration Data*/
	if (0x1 & AWBAFConfig) {
		/* AWB Unit Gain (5100K) */
		debug_log("5100K AWB\n");
		pCamCalData->Single2A.S2aAwb.rGainSetNum = 0;
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				start_addr + 2, 4, (unsigned char *)&CalGain);
		if (read_data_size > 0)	{
			debug_log("Read CalGain OK %x\n", read_data_size);
			CalR  = CalGain & 0xFF;
			CalGr = (CalGain >> 8) & 0xFF;
			CalGb = (CalGain >> 16) & 0xFF;
			CalG  = ((CalGr + CalGb) + 1) >> 1;
			CalB  = (CalGain >> 24) & 0xFF;
			if (CalR > CalG)
				/* R > G */
				if (CalR > CalB)
					tempMax = CalR;
				else
					tempMax = CalB;
			else
				/* G > R */
				if (CalG > CalB)
					tempMax = CalG;
				else
					tempMax = CalB;
			debug_log(
				"UnitR:%d, UnitG:%d, UnitB:%d, New Unit Max=%d",
				CalR, CalG, CalB, tempMax);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read CalGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		if (CalGain != 0x00000000 &&
			CalGain != 0xFFFFFFFF &&
			CalR    != 0x00000000 &&
			CalG    != 0x00000000 &&
			CalB    != 0x00000000) {
			pCamCalData->Single2A.S2aAwb.rGainSetNum++;
			pCamCalData->Single2A.S2aAwb.rUnitGainu4R =
					(unsigned int)((tempMax * 512 + (CalR >> 1)) / CalR);
			pCamCalData->Single2A.S2aAwb.rUnitGainu4G =
					(unsigned int)((tempMax * 512 + (CalG >> 1)) / CalG);
			pCamCalData->Single2A.S2aAwb.rUnitGainu4B =
					(unsigned int)((tempMax * 512 + (CalB >> 1)) / CalB);
		} else
			error_log(
			"There are something wrong on EEPROM, plz contact module vendor!!\n");
		/* AWB Golden Gain (5100K) */
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				start_addr + 6, 4, (unsigned char *)&FacGain);
		if (read_data_size > 0)	{
			debug_log("Read FacGain OK\n");
			FacR  = FacGain & 0xFF;
			FacGr = (FacGain >> 8) & 0xFF;
			FacGb = (FacGain >> 16) & 0xFF;
			FacG  = ((FacGr + FacGb) + 1) >> 1;
			FacB  = (FacGain >> 24) & 0xFF;
			if (FacR > FacG)
				if (FacR > FacB)
					tempMax = FacR;
				else
					tempMax = FacB;
			else
				if (FacG > FacB)
					tempMax = FacG;
				else
					tempMax = FacB;
			debug_log(
				"GoldenR:%d, GoldenG:%d, GoldenB:%d, New Golden Max=%d",
				FacR, FacG, FacB, tempMax);
			err = CAM_CAL_ERR_NO_ERR;
		} else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read FacGain Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		if (FacGain != 0x00000000 &&
			FacGain != 0xFFFFFFFF &&
			FacR    != 0x00000000 &&
			FacG    != 0x00000000 &&
			FacB    != 0x00000000)	{
			pCamCalData->Single2A.S2aAwb.rGoldGainu4R =
					(unsigned int)((tempMax * 512 + (FacR >> 1)) / FacR);
			pCamCalData->Single2A.S2aAwb.rGoldGainu4G =
					(unsigned int)((tempMax * 512 + (FacG >> 1)) / FacG);
			pCamCalData->Single2A.S2aAwb.rGoldGainu4B =
					(unsigned int)((tempMax * 512 + (FacB >> 1)) / FacB);
		} else
			error_log(
			"There are something wrong on EEPROM, plz contact module vendor!!\n");
		/* Set AWB to 3A Layer */
		pCamCalData->Single2A.S2aAwb.rValueR   = CalR;
		pCamCalData->Single2A.S2aAwb.rValueGr  = CalGr;
		pCamCalData->Single2A.S2aAwb.rValueGb  = CalGb;
		pCamCalData->Single2A.S2aAwb.rValueB   = CalB;
		pCamCalData->Single2A.S2aAwb.rGoldenR  = FacR;
		pCamCalData->Single2A.S2aAwb.rGoldenGr = FacGr;
		pCamCalData->Single2A.S2aAwb.rGoldenGb = FacGb;
		pCamCalData->Single2A.S2aAwb.rGoldenB  = FacB;

		debug_log("======================AWB CAM_CAL==================\n");
		debug_log("[CalGain] = 0x%x\n", CalGain);
		debug_log("[FacGain] = 0x%x\n", FacGain);
		debug_log("[rCalGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4R);
		debug_log("[rCalGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4G);
		debug_log("[rCalGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rUnitGainu4B);
		debug_log("[rFacGain.u4R] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4R);
		debug_log("[rFacGain.u4G] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4G);
		debug_log("[rFacGain.u4B] = %d\n", pCamCalData->Single2A.S2aAwb.rGoldGainu4B);
		debug_log("======================AWB CAM_CAL==================\n");

		if (get_mtk_format_version(pdata, pGetSensorCalData) >= 0x22) {
			/* AWB Unit Gain (3100K) */
			debug_log("3100K AWB\n");
			CalR = CalGr = CalGb = CalG = CalB = 0;
			tempMax = 0;
			read_data_size = read_data(pdata,
					pCamCalData->sensorID, pCamCalData->deviceID,
					0x14FE, 4, (unsigned char *)&CalGain);
			if (read_data_size > 0)	{
				debug_log("Read CalGain OK %x\n", read_data_size);
				CalR  = CalGain & 0xFF;
				CalGr = (CalGain >> 8) & 0xFF;
				CalGb = (CalGain >> 16) & 0xFF;
				CalG  = ((CalGr + CalGb) + 1) >> 1;
				CalB  = (CalGain >> 24) & 0xFF;
				if (CalR > CalG)
					/* R > G */
					if (CalR > CalB)
						tempMax = CalR;
					else
						tempMax = CalB;
				else
					/* G > R */
					if (CalG > CalB)
						tempMax = CalG;
					else
						tempMax = CalB;
				debug_log(
					"UnitR:%d, UnitG:%d, UnitB:%d, New Unit Max=%d",
					CalR, CalG, CalB, tempMax);
				err = CAM_CAL_ERR_NO_ERR;
			} else {
				pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
				error_log("Read CalGain Failed\n");
				show_cmd_error_log(pCamCalData->Command);
			}
			if (CalGain != 0x00000000 &&
				CalGain != 0xFFFFFFFF &&
				CalR    != 0x00000000 &&
				CalG    != 0x00000000 &&
				CalB    != 0x00000000) {
				pCamCalData->Single2A.S2aAwb.rGainSetNum++;
				pCamCalData->Single2A.S2aAwb.rUnitGainu4R_low =
					(unsigned int)((tempMax * 512 + (CalR >> 1)) / CalR);
				pCamCalData->Single2A.S2aAwb.rUnitGainu4G_low =
					(unsigned int)((tempMax * 512 + (CalG >> 1)) / CalG);
				pCamCalData->Single2A.S2aAwb.rUnitGainu4B_low =
					(unsigned int)((tempMax * 512 + (CalB >> 1)) / CalB);
			} else
				error_log(
			"There are something wrong on EEPROM, plz contact module vendor!!\n");
			/* AWB Golden Gain (3100K) */
			FacR = FacGr = FacGb = FacG = FacB = 0;
			tempMax = 0;
			read_data_size = read_data(pdata,
					pCamCalData->sensorID, pCamCalData->deviceID,
					0x1502, 4, (unsigned char *)&FacGain);
			if (read_data_size > 0)	{
				debug_log("Read FacGain OK\n");
				FacR  = FacGain & 0xFF;
				FacGr = (FacGain >> 8) & 0xFF;
				FacGb = (FacGain >> 16) & 0xFF;
				FacG  = ((FacGr + FacGb) + 1) >> 1;
				FacB  = (FacGain >> 24) & 0xFF;
				if (FacR > FacG)
					if (FacR > FacB)
						tempMax = FacR;
					else
						tempMax = FacB;
				else
					if (FacG > FacB)
						tempMax = FacG;
					else
						tempMax = FacB;
				debug_log(
					"GoldenR:%d, GoldenG:%d, GoldenB:%d, New Golden Max=%d",
					FacR, FacG, FacB, tempMax);
				err = CAM_CAL_ERR_NO_ERR;
			} else {
				pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
				error_log("Read FacGain Failed\n");
				show_cmd_error_log(pCamCalData->Command);
			}
			if (FacGain != 0x00000000 &&
				FacGain != 0xFFFFFFFF &&
				FacR    != 0x00000000 &&
				FacG    != 0x00000000 &&
				FacB    != 0x00000000)	{
				pCamCalData->Single2A.S2aAwb.rGoldGainu4R_low =
					(unsigned int)((tempMax * 512 + (FacR >> 1)) / FacR);
				pCamCalData->Single2A.S2aAwb.rGoldGainu4G_low =
					(unsigned int)((tempMax * 512 + (FacG >> 1)) / FacG);
				pCamCalData->Single2A.S2aAwb.rGoldGainu4B_low =
					(unsigned int)((tempMax * 512 + (FacB >> 1)) / FacB);
			} else
				error_log(
			"There are something wrong on EEPROM, plz contact module vendor!!\n");
			/* AWB Unit Gain (4000K) */
			debug_log("4000K AWB\n");
			CalR = CalGr = CalGb = CalG = CalB = 0;
			tempMax = 0;
			read_data_size = read_data(pdata,
					pCamCalData->sensorID, pCamCalData->deviceID,
					0x1506, 4, (unsigned char *)&CalGain);
			if (read_data_size > 0)	{
				debug_log("Read CalGain OK %x\n", read_data_size);
				CalR  = CalGain & 0xFF;
				CalGr = (CalGain >> 8) & 0xFF;
				CalGb = (CalGain >> 16) & 0xFF;
				CalG  = ((CalGr + CalGb) + 1) >> 1;
				CalB  = (CalGain >> 24) & 0xFF;
				if (CalR > CalG)
					/* R > G */
					if (CalR > CalB)
						tempMax = CalR;
					else
						tempMax = CalB;
				else
					/* G > R */
					if (CalG > CalB)
						tempMax = CalG;
					else
						tempMax = CalB;
				debug_log(
					"UnitR:%d, UnitG:%d, UnitB:%d, New Unit Max=%d",
					CalR, CalG, CalB, tempMax);
				err = CAM_CAL_ERR_NO_ERR;
			} else {
				pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
				error_log("Read CalGain Failed\n");
				show_cmd_error_log(pCamCalData->Command);
			}
			if (CalGain != 0x00000000 &&
				CalGain != 0xFFFFFFFF &&
				CalR    != 0x00000000 &&
				CalG    != 0x00000000 &&
				CalB    != 0x00000000) {
				pCamCalData->Single2A.S2aAwb.rGainSetNum++;
				pCamCalData->Single2A.S2aAwb.rUnitGainu4R_mid =
					(unsigned int)((tempMax * 512 + (CalR >> 1)) / CalR);
				pCamCalData->Single2A.S2aAwb.rUnitGainu4G_mid =
					(unsigned int)((tempMax * 512 + (CalG >> 1)) / CalG);
				pCamCalData->Single2A.S2aAwb.rUnitGainu4B_mid =
					(unsigned int)((tempMax * 512 + (CalB >> 1)) / CalB);
			} else
				error_log(
			"There are something wrong on EEPROM, plz contact module vendor!!\n");
			/* AWB Golden Gain (4000K) */
			FacR = FacGr = FacGb = FacG = FacB = 0;
			tempMax = 0;
			read_data_size = read_data(pdata,
					pCamCalData->sensorID, pCamCalData->deviceID,
					0x150A, 4, (unsigned char *)&FacGain);
			if (read_data_size > 0)	{
				debug_log("Read FacGain OK\n");
				FacR  = FacGain & 0xFF;
				FacGr = (FacGain >> 8) & 0xFF;
				FacGb = (FacGain >> 16) & 0xFF;
				FacG  = ((FacGr + FacGb) + 1) >> 1;
				FacB  = (FacGain >> 24) & 0xFF;
				if (FacR > FacG)
					if (FacR > FacB)
						tempMax = FacR;
					else
						tempMax = FacB;
				else
					if (FacG > FacB)
						tempMax = FacG;
					else
						tempMax = FacB;
				debug_log(
					"GoldenR:%d, GoldenG:%d, GoldenB:%d, New Golden Max=%d",
					FacR, FacG, FacB, tempMax);
				err = CAM_CAL_ERR_NO_ERR;
			} else {
				pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
				error_log("Read FacGain Failed\n");
				show_cmd_error_log(pCamCalData->Command);
			}
			if (FacGain != 0x00000000 &&
				FacGain != 0xFFFFFFFF &&
				FacR    != 0x00000000 &&
				FacG    != 0x00000000 &&
				FacB    != 0x00000000)	{
				pCamCalData->Single2A.S2aAwb.rGoldGainu4R_mid =
					(unsigned int)((tempMax * 512 + (FacR >> 1)) / FacR);
				pCamCalData->Single2A.S2aAwb.rGoldGainu4G_mid =
					(unsigned int)((tempMax * 512 + (FacG >> 1)) / FacG);
				pCamCalData->Single2A.S2aAwb.rGoldGainu4B_mid =
					(unsigned int)((tempMax * 512 + (FacB >> 1)) / FacB);
			} else
				error_log(
			"There are something wrong on EEPROM, plz contact module vendor!!\n");
		}
	}
	/* AF Calibration Data*/
	if (0x2 & AWBAFConfig) {
		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				start_addr + 10, 2, (unsigned char *)&AFInf);
		if (read_data_size > 0)
			err = CAM_CAL_ERR_NO_ERR;
		else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}

		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				start_addr + 12, 2, (unsigned char *)&AFMacro);
		if (read_data_size > 0)
			err = CAM_CAL_ERR_NO_ERR;
		else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}

		pCamCalData->Single2A.S2aAf[0] = AFInf;
		pCamCalData->Single2A.S2aAf[1] = AFMacro;

		////Only AF Gathering <////
		debug_log("======================AF CAM_CAL==================\n");
		debug_log("[AFInf] = %d\n", AFInf);
		debug_log("[AFMacro] = %d\n", AFMacro);
		debug_log("======================AF CAM_CAL==================\n");
	}
	/* AF Closed-Loop Calibration Data*/
	if ((get_mtk_format_version(pdata, pGetSensorCalData) < 0x18)
		? (0x4 & AWBAFConfig) : (0x2 & AWBAFConfig)) {
		//load AF addition info
		unsigned char AF_INFO[64];
		unsigned int af_info_offset = 0xf63;

		memset(AF_INFO, 0, 64);
		debug_log("af_info_offset = %d\n", af_info_offset);

		read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
				af_info_offset, 64, (unsigned char *) AF_INFO);
		if (read_data_size > 0)
			err = CAM_CAL_ERR_NO_ERR;
		else {
			pCamCalData->Single2A.S2aBitEn = CAM_CAL_NONE_BITEN;
			error_log("Read Failed\n");
			show_cmd_error_log(pCamCalData->Command);
		}
		debug_log("AF Test = %x %x %x %x\n",
				AF_INFO[6], AF_INFO[7], AF_INFO[8], AF_INFO[9]);

		pCamCalData->Single2A.S2aAF_t.Close_Loop_AF_Min_Position =
			AF_INFO[0] | (AF_INFO[1] << 8);
		pCamCalData->Single2A.S2aAF_t.Close_Loop_AF_Max_Position =
			AF_INFO[2] | (AF_INFO[3] << 8);
		pCamCalData->Single2A.S2aAF_t.Close_Loop_AF_Hall_AMP_Offset =
			AF_INFO[4];
		pCamCalData->Single2A.S2aAF_t.Close_Loop_AF_Hall_AMP_Gain =
			AF_INFO[5];
		pCamCalData->Single2A.S2aAF_t.AF_infinite_pattern_distance =
			AF_INFO[6] | (AF_INFO[7] << 8);
		pCamCalData->Single2A.S2aAF_t.AF_Macro_pattern_distance =
			AF_INFO[8] | (AF_INFO[9] << 8);
		pCamCalData->Single2A.S2aAF_t.AF_infinite_calibration_temperature =
			AF_INFO[10];

		if (get_mtk_format_version(pdata, pGetSensorCalData) >= 0x22) {
			pCamCalData->Single2A.S2aAF_t.AF_macro_calibration_temperature =
				0;
			pCamCalData->Single2A.S2aAF_t.AF_dac_code_bit_depth =
				AF_INFO[11];
			pCamCalData->Single2A.S2aAF_t.AF_Middle_calibration_temperature =
				0;
		} else {
			pCamCalData->Single2A.S2aAF_t.AF_macro_calibration_temperature =
				AF_INFO[11];
			pCamCalData->Single2A.S2aAF_t.AF_dac_code_bit_depth =
				0;
			pCamCalData->Single2A.S2aAF_t.AF_Middle_calibration_temperature =
				AF_INFO[20];
		}

		if (get_mtk_format_version(pdata, pGetSensorCalData) >= 0x18) {
			pCamCalData->Single2A.S2aAF_t.Posture_AF_infinite_calibration =
				AF_INFO[12] | (AF_INFO[13] << 8);
			pCamCalData->Single2A.S2aAF_t.Posture_AF_macro_calibration =
				AF_INFO[14] | (AF_INFO[15] << 8);
		}

		pCamCalData->Single2A.S2aAF_t.AF_Middle_calibration =
			AF_INFO[18] | (AF_INFO[19] << 8);

		if (get_mtk_format_version(pdata, pGetSensorCalData) >= 0x22) {
			memset(AF_INFO, 0, 64);
			read_data_size = read_data(pdata,
					pCamCalData->sensorID, pCamCalData->deviceID,
					0x154F, 41, (unsigned char *) AF_INFO);
			if (read_data_size >= 0) {
				pCamCalData->Single2A.S2aAF_t.Optical_zoom_cali_num = AF_INFO[0];
				memcpy(pCamCalData->Single2A.S2aAF_t.Optical_zoom_AF_cali,
					&AF_INFO[1], 40);
			}
		}

		debug_log("======================AF addition CAM_CAL==================\n");
		debug_log("[AF_infinite_pattern_distance] = %dmm\n",
				pCamCalData->Single2A.S2aAF_t.AF_infinite_pattern_distance);
		debug_log("[AF_Macro_pattern_distance] = %dmm\n",
				pCamCalData->Single2A.S2aAF_t.AF_Macro_pattern_distance);
		debug_log("[AF_Middle_calibration] = %d\n",
				pCamCalData->Single2A.S2aAF_t.AF_Middle_calibration);
		debug_log("======================AF addition CAM_CAL==================\n");
	}
	return err;
}


/***********************************************************************************
 * Function : To read LSC Table
 ***********************************************************************************/
unsigned int do_single_lsc(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	int read_data_size;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];
	unsigned short table_size = 0;

	if (pCamCalData->DataVer >= CAM_CAL_TYPE_NUM) {
		err = CAM_CAL_ERR_NO_DEVICE;
		error_log("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
		return err;
	}
	if (block_size != CAM_CAL_SINGLE_LSC_SIZE)
		error_log("block_size(%d) is not match (%d)\n",
				block_size, CAM_CAL_SINGLE_LSC_SIZE);

	pCamCalData->SingleLsc.LscTable.MtkLcsData.MtkLscType = 2;//mtk type
	pCamCalData->SingleLsc.LscTable.MtkLcsData.PixId = 8;

	debug_log("u4Offset=%d u4Length=%lu", start_addr - 2, sizeof(table_size));
	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr - 2, sizeof(table_size), (unsigned char *)&table_size);
	if (read_data_size <= 0)
		err = CAM_CAL_ERR_NO_SHADING;

	debug_log("lsc table_size %d\n", table_size);
	pCamCalData->SingleLsc.LscTable.MtkLcsData.TableSize = table_size;
	if (table_size > 0) {
		pCamCalData->SingleLsc.TableRotation = 0;
		debug_log("u4Offset=%d u4Length=%d", start_addr, table_size);
		read_data_size = read_data(pdata,
			pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, table_size, (unsigned char *)
			&pCamCalData->SingleLsc.LscTable.MtkLcsData.SlimLscType);
		if (table_size == read_data_size)
			err = CAM_CAL_ERR_NO_ERR;
		else {
			error_log("Read Failed\n");
			err = CamCalReturnErr[pCamCalData->Command];
			show_cmd_error_log(pCamCalData->Command);
		}
	}

	debug_log("======================SingleLsc Data==================\n");
	debug_log("[1st] = %x, %x, %x, %x\n",
		pCamCalData->SingleLsc.LscTable.Data[0],
		pCamCalData->SingleLsc.LscTable.Data[1],
		pCamCalData->SingleLsc.LscTable.Data[2],
		pCamCalData->SingleLsc.LscTable.Data[3]);
	debug_log("[1st] = SensorLSC(1)?MTKLSC(2)?  %x\n",
		pCamCalData->SingleLsc.LscTable.MtkLcsData.MtkLscType);
	debug_log("CapIspReg =0x%x, 0x%x, 0x%x, 0x%x, 0x%x",
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[0],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[1],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[2],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[3],
		pCamCalData->SingleLsc.LscTable.MtkLcsData.CapIspReg[4]);
	debug_log("RETURN = 0x%x\n", err);
	debug_log("======================SingleLsc Data==================\n");

	return err;
}

unsigned int do_pdaf(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	int read_data_size;
	int err =  CamCalReturnErr[pCamCalData->Command];

	pCamCalData->PDAF.Size_of_PDAF = block_size;
	debug_log("PDAF start_addr =%x table_size=%d\n", start_addr, block_size);

	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, block_size, (unsigned char *)&pCamCalData->PDAF.Data[0]);
	if (read_data_size > 0)
		err = CAM_CAL_ERR_NO_ERR;

	debug_log("======================PDAF Data==================\n");
	debug_log("First five %x, %x, %x, %x, %x\n",
		pCamCalData->PDAF.Data[0],
		pCamCalData->PDAF.Data[1],
		pCamCalData->PDAF.Data[2],
		pCamCalData->PDAF.Data[3],
		pCamCalData->PDAF.Data[4]);
	debug_log("RETURN = 0x%x\n", err);
	debug_log("======================PDAF Data==================\n");

	return err;

}

/******************************************************************************
 * This function will add after sensor support FOV data
 ******************************************************************************/
unsigned int do_stereo_data(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	int read_data_size;
	unsigned int err =  0;
	char Stereo_Data[1360];

	debug_log("DoCamCal_Stereo_Data sensorID = %x\n", pCamCalData->sensorID);
	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, block_size, (unsigned char *)Stereo_Data);
	if (read_data_size > 0)
		err = CAM_CAL_ERR_NO_ERR;
	else {
		error_log("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
	}

	debug_log("======================DoCamCal_Stereo_Data==================\n");
	debug_log("======================DoCamCal_Stereo_Data==================\n");

	return err;
}

unsigned int do_dump_all(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	(void) pdata;
	(void) start_addr;
	(void) block_size;
	(void) pGetSensorCalData;

	return CAM_CAL_ERR_NO_ERR;
}

unsigned int do_lens_id_base(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;
	int read_data_size;
	unsigned int err = CamCalReturnErr[pCamCalData->Command];
	unsigned int size_limit = sizeof(pCamCalData->LensDrvId);

	memset(&pCamCalData->LensDrvId[0], 0, size_limit);

	if (block_size > size_limit) {
		error_log("lens id size can't larger than %u\n", size_limit);
		return err;
	}

	read_data_size = read_data(pdata, pCamCalData->sensorID, pCamCalData->deviceID,
			start_addr, block_size, (unsigned char *)&pCamCalData->LensDrvId[0]);
	if (read_data_size > 0)
		err = CAM_CAL_ERR_NO_ERR;
	else {
		error_log("Read Failed\n");
		show_cmd_error_log(pCamCalData->Command);
	}

	debug_log("======================Lens Id==================\n");
	debug_log("[Lens Id] = %x %x %x %x %x\n",
			pCamCalData->LensDrvId[0], pCamCalData->LensDrvId[1],
			pCamCalData->LensDrvId[2], pCamCalData->LensDrvId[3],
			pCamCalData->LensDrvId[4]);
	debug_log("[Lens Id] = %x %x %x %x %x\n",
			pCamCalData->LensDrvId[5], pCamCalData->LensDrvId[6],
			pCamCalData->LensDrvId[7], pCamCalData->LensDrvId[8],
			pCamCalData->LensDrvId[9]);
	debug_log("======================Lens Id==================\n");

	return err;
}

unsigned int do_lens_id(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	unsigned int err = CamCalReturnErr[pCamCalData->Command];

	if (get_mtk_format_version(pdata, pGetSensorCalData) >= 0x18) {
		debug_log("No lens id data\n");
		return err;
	}

	return do_lens_id_base(pdata, start_addr, block_size, pGetSensorCalData);
}

unsigned int get_is_need_power_on(struct EEPROM_DRV_FD_DATA *pdata, unsigned int *pGetNeedPowerOn)
{
	struct STRUCT_CAM_CAL_NEED_POWER_ON *pCamCalNeedPowerOn =
				(struct STRUCT_CAM_CAL_NEED_POWER_ON *)pGetNeedPowerOn;

	enum ENUM_CAMERA_CAM_CAL_TYPE_ENUM lsCommand = pCamCalNeedPowerOn->Command;
	unsigned int uint_lsCommand = (unsigned int)lsCommand;
	unsigned int result = CAM_CAL_ERR_NO_DEVICE;
	int preloadLayoutIndex = IMGSENSOR_SENSOR_DUAL2IDX(pCamCalNeedPowerOn->deviceID);

	if (lsCommand >= CAMERA_CAM_CAL_DATA_LIST) {
		error_log("Invalid Command = 0x%x\n", lsCommand);
		return CAM_CAL_ERR_NO_CMD;
	}

	if (preloadLayoutIndex < 0 || preloadLayoutIndex >= IDX_MAX_CAM_NUMBER) {
		error_log("Invalid DeviceID: 0x%x", pCamCalNeedPowerOn->deviceID);
		return result;
	}

	if (last_sensor_id != pCamCalNeedPowerOn->sensorID) {
		last_sensor_id = pCamCalNeedPowerOn->sensorID;
		if (mp_layout_preload[preloadLayoutIndex] == NULL) {
			debug_log("Preloading layout type");
			debug_log("search %u layouts", cam_cal_number);
			for (cam_cal_index = 0; cam_cal_index < cam_cal_number; cam_cal_index++) {
				cam_cal_config = cam_cal_config_list[cam_cal_index];
				if ((cam_cal_config->check_layout_function != NULL) &&
				(cam_cal_config->check_layout_function(pdata,
				pCamCalNeedPowerOn->sensorID) == CAM_CAL_ERR_NO_ERR))
					break;
			}
			if (cam_cal_index < cam_cal_number) {
				mp_layout_preload[preloadLayoutIndex] = kmalloc(2, GFP_KERNEL);
				memcpy(mp_layout_preload[preloadLayoutIndex], &cam_cal_index, 2);
			}
		} else {
			debug_log("Read layout type from memory[%d]", preloadLayoutIndex);
			memcpy(&cam_cal_index, mp_layout_preload[preloadLayoutIndex], 2);
		}
	}

	if (cam_cal_index < cam_cal_number) {
		cam_cal_config = cam_cal_config_list[cam_cal_index];
		must_log(
		"device_id = %u last_sensor_id = 0x%x current_sensor_id = 0x%x layout type %s found",
		pCamCalNeedPowerOn->deviceID, last_sensor_id, pCamCalNeedPowerOn->sensorID,
		cam_cal_config->name);
		pCamCalNeedPowerOn->needPowerOn = cam_cal_config->has_stored_data &&
			cam_cal_config->layout->cal_layout_tbl[uint_lsCommand].Include;
		result = CAM_CAL_ERR_NO_ERR;
		return result;
	}
	must_log(
		"device_id = %u last_sensor_id = 0x%x current_sensor_id = 0x%x layout type not found",
		pCamCalNeedPowerOn->deviceID, last_sensor_id, pCamCalNeedPowerOn->sensorID);

	result = CamCalReturnErr[uint_lsCommand];
	show_cmd_error_log(lsCommand);
	return result;
}

unsigned int get_cal_data(struct EEPROM_DRV_FD_DATA *pdata, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
				(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	enum ENUM_CAMERA_CAM_CAL_TYPE_ENUM lsCommand = pCamCalData->Command;
	unsigned int uint_lsCommand = (unsigned int)lsCommand;
	unsigned int result = CAM_CAL_ERR_NO_DEVICE;
	int preloadLayoutIndex = IMGSENSOR_SENSOR_DUAL2IDX(pCamCalData->deviceID);

	if (lsCommand >= CAMERA_CAM_CAL_DATA_LIST) {
		error_log("Invalid Command = 0x%x\n", lsCommand);
		return CAM_CAL_ERR_NO_CMD;
	}

	if (preloadLayoutIndex < 0 || preloadLayoutIndex >= IDX_MAX_CAM_NUMBER) {
		error_log("Invalid DeviceID: 0x%x", pCamCalData->deviceID);
		return result;
	}

	if (last_sensor_id != pCamCalData->sensorID
			|| pCamCalData->DataVer == CAM_CAL_TYPE_NUM
			|| cam_cal_index == cam_cal_number) {
		last_sensor_id = pCamCalData->sensorID;
		if (mp_layout_preload[preloadLayoutIndex] == NULL) {
			debug_log("Preloading layout type");
			debug_log("search %u layouts", cam_cal_number);
			for (cam_cal_index = 0; cam_cal_index < cam_cal_number; cam_cal_index++) {
				cam_cal_config = cam_cal_config_list[cam_cal_index];
				if ((cam_cal_config->check_layout_function != NULL) &&
				(cam_cal_config->check_layout_function(pdata,
				pCamCalData->sensorID) == CAM_CAL_ERR_NO_ERR))
					break;
			}
			if (cam_cal_index < cam_cal_number) {
				mp_layout_preload[preloadLayoutIndex] = kmalloc(2, GFP_KERNEL);
				memcpy(mp_layout_preload[preloadLayoutIndex], &cam_cal_index, 2);
			}
		} else {
			debug_log("Read layout type from memory[%d]", preloadLayoutIndex);
			memcpy(&cam_cal_index, mp_layout_preload[preloadLayoutIndex], 2);
		}
	}

	if (cam_cal_index < cam_cal_number) {
		cam_cal_config = cam_cal_config_list[cam_cal_index];
		must_log(
		"device_id = %u last_sensor_id = 0x%x current_sensor_id = 0x%x layout type %s found",
		pCamCalData->deviceID, last_sensor_id, pCamCalData->sensorID, cam_cal_config->name);
		pCamCalData->DataVer =
			(enum ENUM_CAM_CAL_DATA_VER_ENUM)cam_cal_config->layout->data_ver;
		if ((cam_cal_config->layout->cal_layout_tbl[uint_lsCommand].Include != 0) &&
			(cam_cal_config->layout->cal_layout_tbl[uint_lsCommand].GetCalDataProcess
			!= NULL)) {
			result =
			cam_cal_config->layout->cal_layout_tbl[uint_lsCommand].GetCalDataProcess(
			pdata,
			cam_cal_config->layout->cal_layout_tbl[uint_lsCommand].start_addr,
			cam_cal_config->layout->cal_layout_tbl[uint_lsCommand].block_size,
			pGetSensorCalData);
			return result;
		}
	} else
		must_log(
		"device_id = %u last_sensor_id = 0x%x current_sensor_id = 0x%x layout type not found",
		pCamCalData->deviceID, last_sensor_id, pCamCalData->sensorID);

	result = CamCalReturnErr[uint_lsCommand];
	show_cmd_error_log(lsCommand);
	return result;
}

int read_data(struct EEPROM_DRV_FD_DATA *pdata, unsigned int sensor_id, unsigned int device_id,
		unsigned int offset, unsigned int length, unsigned char *data)
{
	int preloadIndex = IMGSENSOR_SENSOR_DUAL2IDX(device_id);
	unsigned int bufSize = (cam_cal_config->preload_size > cam_cal_config->max_size)
		? cam_cal_config->max_size : cam_cal_config->preload_size;

	(void) sensor_id;

	if (preloadIndex < 0 || preloadIndex >= IDX_MAX_CAM_NUMBER) {
		error_log("Invalid DeviceID: 0x%x", device_id);
		return -1;
	}
	if (cam_cal_config->enable_preload && bufSize > 0) {
		// Preloading to memory and read from memory
		if (mp_eeprom_preload[preloadIndex] == NULL) {
			mp_eeprom_preload[preloadIndex] = kmalloc(bufSize, GFP_KERNEL);
			must_log("Preloading data %u bytes", bufSize);
			if (read_data_region(pdata, mp_eeprom_preload[preloadIndex], 0, bufSize)
					!= bufSize) {
				error_log("Preload data failed");
				kfree(mp_eeprom_preload[preloadIndex]);
				mp_eeprom_preload[preloadIndex] = NULL;
			}
		}
		if ((mp_eeprom_preload[preloadIndex] != NULL) &&
				(offset + length <= bufSize)) {
			debug_log("Read data from memory[%d]", preloadIndex);
			memcpy(data, mp_eeprom_preload[preloadIndex] + offset, length);
			return length;
		}
	}
	// Read data from EEPROM
	must_log("Read data from EEPROM");
	return read_data_region(pdata, data, offset, length);
}

unsigned int read_data_region(struct EEPROM_DRV_FD_DATA *pdata,
			unsigned char *buf,
			unsigned int offset, unsigned int size)
{
	unsigned int ret;
	unsigned short dts_addr;
	unsigned int size_limit = (cam_cal_config->max_size > 0)
		? cam_cal_config->max_size : DEFAULT_MAX_EEPROM_SIZE_8K;

	if (offset + size > size_limit) {
		error_log("Error! Not support address >= 0x%x!!\n", size_limit);
		return 0;
	}
	if (cam_cal_config->read_function) {
		debug_log("i2c read 0x%02x %d %d\n",
					cam_cal_config->i2c_write_id, offset, size);
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		dts_addr = pdata->pdrv->pi2c_client->addr;
		pdata->pdrv->pi2c_client->addr = (cam_cal_config->i2c_write_id >> 1);
		ret = cam_cal_config->read_function(pdata->pdrv->pi2c_client,
					offset, buf, size);
		pdata->pdrv->pi2c_client->addr = dts_addr;
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
	} else {
		debug_log("no customized\n");
		debug_log("i2c read 0x%02x %d %d\n",
				(pdata->pdrv->pi2c_client->addr << 1),
				offset, size);
		mutex_lock(&pdata->pdrv->eeprom_mutex);
		ret = Common_read_region(pdata->pdrv->pi2c_client,
					offset, buf, size);
		mutex_unlock(&pdata->pdrv->eeprom_mutex);
	}
	return ret;
}
