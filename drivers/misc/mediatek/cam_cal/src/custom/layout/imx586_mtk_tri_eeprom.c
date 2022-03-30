// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#define PFX "CAM_CAL"
#define pr_fmt(fmt) PFX "[%s] " fmt, __func__

#include <linux/kernel.h>
#include "cam_cal_list.h"
#include "eeprom_i2c_common_driver.h"
#include "eeprom_i2c_custom_driver.h"
#include "cam_cal_config.h"

static unsigned int do_2a_gain_default(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData);

static struct STRUCT_CALIBRATION_LAYOUT_STRUCT cal_layout_table = {
	0x00000001, 0x010b00ff, CAM_CAL_SINGLE_EEPROM_DATA,
	{
		{0x00000000, 0x00000000, 0x00000000, do_module_version},
		{0x00000000, 0x00000005, 0x00000002, do_part_number},
		{0x00000000, 0x00000017, 0x0000074C, do_single_lsc},
		{0x00000001, 0x00000007, 0x0000000E, do_2a_gain_default},
		{0x00000000, 0x00000763, 0x00000800, do_pdaf},
		{0x00000000, 0x00000FAE, 0x00000550, do_stereo_data},
		{0x00000000, 0x00000000, 0x00001600, do_dump_all},
		{0x00000000, 0x00000F80, 0x0000000A, do_lens_id}
	}
};

struct STRUCT_CAM_CAL_CONFIG_STRUCT imx586_mtk_tri_eeprom = {
	.name = "imx586_mtk_tri_eeprom",
	.check_layout_function = layout_no_ck,
	.read_function = Common_read_region,
	.layout = &cal_layout_table,
	.sensor_id = IMX586_SENSOR_ID,
	.i2c_write_id = 0xA0,
	.max_size = 0x4000,
	.enable_preload = 0,
	.preload_size = 0x0000,
	.has_stored_data = 0,
};

static unsigned int do_2a_gain_default(struct EEPROM_DRV_FD_DATA *pdata,
		unsigned int start_addr, unsigned int block_size, unsigned int *pGetSensorCalData)
{
	struct STRUCT_CAM_CAL_DATA_STRUCT *pCamCalData =
			(struct STRUCT_CAM_CAL_DATA_STRUCT *)pGetSensorCalData;

	(void) pdata;
	(void) start_addr;
	(void) block_size;

	pCamCalData->Single2A.S2aVer = 0x01;
	pCamCalData->Single2A.S2aBitEn = 0x02; // AF enable
	pCamCalData->Single2A.S2aAf[0] = 456; //AFInf
	pCamCalData->Single2A.S2aAf[1] = 856; //AFMacro
	pCamCalData->Single2A.S2aAF_t.AF_infinite_pattern_distance = 5000;
	pCamCalData->Single2A.S2aAF_t.AF_Macro_pattern_distance = 100;
	pCamCalData->Single2A.S2aAF_t.AF_Middle_calibration = 577;

	must_log("Load default cal data\n");

	return CAM_CAL_ERR_NO_ERR;
}
