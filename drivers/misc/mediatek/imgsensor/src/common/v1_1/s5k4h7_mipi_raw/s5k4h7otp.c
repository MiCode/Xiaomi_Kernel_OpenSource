// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

/*
 * NOTE:
 * The modification is appended to initialization of image sensor.
 * After sensor initialization, use the function
 * bool otp_update_wb(unsigned char golden_rg, unsigned char golden_bg)
 * and
 * bool otp_update_lenc(void)
 * and then the calibration of AWB & LSC & BLC will be applied.
 * After finishing the OTP written, we will provide you the typical
 * value of golden sample.
 */

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>
#include <linux/slab.h>

//#ifndef VENDOR_EDIT
//#include "kd_camera_hw.h"
/*Caohua.Lin@Camera.Drv, 20180126 remove to adapt with mt6771*/
//#endif
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "kd_camera_typedef.h"

#include "s5k4h7mipiraw_Sensor.h"
#include "s5k4h7otp.h"


/******************Modify Following Strings for Debug*******************/
#define PFX "S5K4H7OTP"
#define LOG_1 SENSORDB("S5K4H7,MIPI CAM\n")
#define LOG_INF(format, args...) \
	pr_debug(PFX "[%s] " format, __func__, ##args)
/*********************   Modify end    *********************************/

#define USHORT        unsigned short
#define BYTE          unsigned char
#define I2C_ID          0x20

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
	kal_uint16 get_byte = 0;
	char pusendcmd[2] = { (char)(addr >> 8), (char)(addr & 0xFF) };

	iReadRegI2C(pusendcmd, 2, (u8 *) &get_byte, 1, I2C_ID);
	return get_byte;
}



static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
	char pusendcmd[4] = {
		(char)(addr >> 8),
		(char)(addr & 0xFF),
		(char)(para & 0xFF) };

	iWriteRegI2C(pusendcmd, 3, I2C_ID);
}

/*************************************************************************
 * Function    :  wb_gain_set
 * Description :  Set WB ratio to register gain setting  512x
 * Parameters  :  [int] r_ratio : R ratio data compared with golden module R
 *                b_ratio : B ratio data compared with golden module B
 * Return      :  [bool] 0 : set wb fail 1 : WB set success
 *************************************************************************/
bool wb_gain_set(kal_uint32 r_ratio, kal_uint32 b_ratio)
{
	kal_uint32 R_GAIN = 0;
	kal_uint32 B_GAIN = 0;
	kal_uint32 Gr_GAIN = 0;
	kal_uint32 Gb_GAIN = 0;
	kal_uint32 G_GAIN = 0;
	kal_uint32 GAIN_DEFAULT = 0x0100;

	if (!r_ratio || !b_ratio) {
		LOG_INF(" OTP WB ratio Data Err!");
		return 0;
	}
	if (r_ratio >= 512) {
		if (b_ratio >= 512) {
			R_GAIN = (USHORT) (GAIN_DEFAULT * r_ratio / 512);
			G_GAIN = GAIN_DEFAULT;
			B_GAIN = (USHORT) (GAIN_DEFAULT * b_ratio / 512);
		} else {
			R_GAIN = (USHORT) (GAIN_DEFAULT * r_ratio / b_ratio);
			G_GAIN = (USHORT) (GAIN_DEFAULT * 512 / b_ratio);
			B_GAIN = GAIN_DEFAULT;
		}
	} else {
		if (b_ratio >= 512) {
			R_GAIN = GAIN_DEFAULT;
			G_GAIN = (USHORT) (GAIN_DEFAULT * 512 / r_ratio);
			B_GAIN = (USHORT) (GAIN_DEFAULT * b_ratio / r_ratio);
		} else {
			Gr_GAIN = (USHORT) (GAIN_DEFAULT * 512 / r_ratio);
			Gb_GAIN = (USHORT) (GAIN_DEFAULT * 512 / b_ratio);
			if (Gr_GAIN >= Gb_GAIN) {
				R_GAIN = GAIN_DEFAULT;
				G_GAIN = (USHORT)
					(GAIN_DEFAULT * 512 / r_ratio);
				B_GAIN = (USHORT)
					(GAIN_DEFAULT * b_ratio / r_ratio);
			} else {
				R_GAIN = (USHORT)
					(GAIN_DEFAULT * r_ratio / b_ratio);
				G_GAIN = (USHORT)
					(GAIN_DEFAULT * 512 / b_ratio);
				B_GAIN = GAIN_DEFAULT;
			}
		}
	}

	write_cmos_sensor_8(0x3C0F, 0x01);
	if (R_GAIN > GAIN_DEFAULT) {
		write_cmos_sensor_8(0x0210, (R_GAIN >> 8) & 0x0F);
		write_cmos_sensor_8(0x0211, R_GAIN & 0xFF);
	}
	if (B_GAIN > GAIN_DEFAULT) {
		write_cmos_sensor_8(0x0212, (B_GAIN >> 8) & 0x0F);
		write_cmos_sensor_8(0x0213, B_GAIN & 0xFF);
	}
	if (G_GAIN > GAIN_DEFAULT) {
		write_cmos_sensor_8(0x020E, (G_GAIN >> 8) & 0x0F);
		write_cmos_sensor_8(0x020F, G_GAIN & 0xFF);
		write_cmos_sensor_8(0x0214, (G_GAIN >> 8) & 0x0F);
		write_cmos_sensor_8(0x0215, G_GAIN & 0xFF);
	}
	return 1;
}


bool S5K4H7_update_awb(unsigned char page)
{
	kal_uint32 r_ratio;
	kal_uint32 b_ratio;

	kal_uint32 golden_rg = 0;
	kal_uint32 golden_bg = 0;

	kal_uint32 unit_rg = 0;
	kal_uint32 unit_bg = 0;

	write_cmos_sensor_8(0x0A02, page);
	write_cmos_sensor_8(0x0A00, 0x01);

	golden_rg = read_cmos_sensor_8(0x0A1A)
		| read_cmos_sensor_8(0x0A1B) << 8;
	golden_bg = read_cmos_sensor_8(0x0A20)
		| read_cmos_sensor_8(0x0A21) << 8;

	unit_rg = read_cmos_sensor_8(0x0A12) | read_cmos_sensor_8(0x0A13) << 8;
	unit_bg = read_cmos_sensor_8(0x0A18) | read_cmos_sensor_8(0x0A19) << 8;

	LOG_INF(
		"updata wb golden_rg=0x%x golden_bg=0x%x unit_rg=0x%x unit_bg =0x%x\n",
		golden_rg, golden_bg, unit_rg, unit_bg);

	if (!golden_rg || !golden_bg || !unit_rg || !unit_bg) {
		LOG_INF("updata wb err");
		return 0;
	}
	r_ratio = 512 * (golden_rg) / (unit_rg);
	b_ratio = 512 * (golden_bg) / (unit_bg);
	wb_gain_set(r_ratio, b_ratio);
	return 1;
}


/***************************************************************************
 * Function    :  otp_update_wb
 * Description :  Update white balance settings from OTP
 * Parameters  :  [in] golden_rg : R/G of golden camera module
 [in] golden_bg : B/G of golden camera module
 * Return      :  1, success; 0, fail
 ***************************************************************************/
bool S5K4H7_otp_update(void)
{
	bool flag = 0;
	unsigned char page = 0;

	write_cmos_sensor_8(0x0136, 0x18);	// 24MHz
	write_cmos_sensor_8(0x0137, 0x00);
	write_cmos_sensor_8(0x0305, 0x06);	// PLL pre div
	write_cmos_sensor_8(0x0306, 0x00);	//PLL multiplier
	write_cmos_sensor_8(0x0307, 0x8C);

	write_cmos_sensor_8(0x030D, 0x06);	// second_pre_pll_clk_div

	write_cmos_sensor_8(0x030E, 0x00);	// second_pll_multiplier
	write_cmos_sensor_8(0x030F, 0xAF);	// second_pll_multiplier
	write_cmos_sensor_8(0x0301, 0x04);	// vt_pix_clk_div

	//Streaming ON
	write_cmos_sensor_8(0x0100, 0x01);	// Streaming ON
	mDELAY(10);

	write_cmos_sensor_8(0x0A02, 0x15);	// page 21
	write_cmos_sensor_8(0x0A00, 0x01);
	mDELAY(10);
	flag = read_cmos_sensor_8(0x0A10);
	if (flag) {
		page = 0x15;
	} else {
		LOG_INF("otp read page 21 failed\n");
		write_cmos_sensor_8(0x0A02, 0x17);	// page 21
		write_cmos_sensor_8(0x0A00, 0x01);
		flag = read_cmos_sensor_8(0x0A10);
		if (flag)
			page = 0x17;
		else
			LOG_INF("otp read page 23 failed\n");
	}
	if (!flag) {
		page = 0x0;
		LOG_INF("otp read failed\n");
		return 0;
	}
	flag = S5K4H7_update_awb(page);
	return flag;
}

unsigned char s5k4h7_get_module_id(void)
{
	unsigned char module_id = 0;

	write_cmos_sensor_8(0x0136, 0x18);	// 24MHz
	write_cmos_sensor_8(0x0137, 0x00);
	write_cmos_sensor_8(0x0305, 0x06);	// PLL pre div
	write_cmos_sensor_8(0x0306, 0x00);	//PLL multiplier
	write_cmos_sensor_8(0x0307, 0x8C);

	write_cmos_sensor_8(0x030D, 0x06);	// second_pre_pll_clk_div

	write_cmos_sensor_8(0x030E, 0x00);	// second_pll_multiplier
	write_cmos_sensor_8(0x030F, 0xAF);	// second_pll_multiplier
	write_cmos_sensor_8(0x0301, 0x04);	// vt_pix_clk_div

	//Streaming ON
	write_cmos_sensor_8(0x0100, 0x01);	// Streaming ON
	mDELAY(10);

	write_cmos_sensor_8(0x0A02, 0x15);	// page 21
	write_cmos_sensor_8(0x0A00, 0x01);
	mDELAY(10);

	module_id = read_cmos_sensor_8(0x0A04);
	LOG_INF("S5K4H7 module id = 0x%x\n", module_id);
	return module_id;
}
