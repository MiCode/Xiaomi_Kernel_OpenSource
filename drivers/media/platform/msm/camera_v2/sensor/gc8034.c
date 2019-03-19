/* Copyright (c) 2013-2014, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include "msm_sensor.h"
#include "msm_cci.h"
#include "msm_camera_io_util.h"
#include "msm_camera_i2c_mux.h"
#include<linux/kernel.h>

#define GC8034_SENSOR_NAME "gc8034"
DEFINE_MSM_MUTEX(gc8034_mut);

#undef CDBG
#ifdef CONFIG_MSMB_CAMERA_DEBUG
#define CDBG(fmt, args...) pr_err(fmt, ##args)
#else
#define CDBG(fmt, args...) do { } while (0)
#endif
#define DD_WIDTH  3284
#define DD_HEIGH  2464
/* Keep consistent with "vendor\...\gc8034_lib.h" !!!*/
#define GC8034_MIRROR_NORMAL




#if defined(GC8034_MIRROR_NORMAL)
#define MIRROR	0xc0
#elif defined(GC8034_MIRROR_H)
#define MIRROR	0xc1
#elif defined(GC8034_MIRROR_V)
#define MIRROR	0xc2
#elif defined(GC8034_MIRROR_HV)
#define MIRROR	0xc3
#else
#define MIRROR	0xc0
#endif

typedef struct {
	uint16_t x;
	uint16_t y;
	uint16_t t;
} gc8034_dd_t;

struct gc8034_otp_t {
	uint8_t  dd_cnt;
	uint8_t  dd_flag;
	gc8034_dd_t dd_param[160];
	uint8_t  reg_flag;
	uint8_t  reg_num;
	uint8_t  reg_page[10];
	uint8_t  reg_addr[10];
	uint8_t  reg_value[10];
};

static struct gc8034_otp_t gc8034_otp_info = {0};


typedef enum{
	otp_close=0,
	otp_open,
}otp_state;

static uint16_t gc8034_Sensor_ReadReg(
	struct msm_sensor_ctrl_t *s_ctrl, uint8_t reg_addr)
{
	uint16_t reg_value = 0;
	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_read(
				s_ctrl->sensor_i2c_client,
				reg_addr,
				&reg_value, MSM_CAMERA_I2C_BYTE_DATA);
	return reg_value ;
}

static void gc8034_Sensor_WriteReg(
	struct msm_sensor_ctrl_t *s_ctrl, uint8_t reg_addr, uint8_t reg_value)
{

	s_ctrl->sensor_i2c_client->i2c_func_tbl->i2c_write(
		s_ctrl->sensor_i2c_client, reg_addr, reg_value, MSM_CAMERA_I2C_BYTE_DATA);
}

static uint8_t gc8034_read_otp(struct msm_sensor_ctrl_t *s_ctrl, uint8_t page, uint8_t addr)
{
	uint8_t value;

	gc8034_Sensor_WriteReg(s_ctrl, 0xfe, 0x00);
	gc8034_Sensor_WriteReg(s_ctrl, 0xd4, ((page << 2) & 0x3c) + ((addr >> 5) & 0x03));
	gc8034_Sensor_WriteReg(s_ctrl, 0xd5, (addr << 3) & 0xff);
	mdelay(1);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf3, 0x20);
	value = gc8034_Sensor_ReadReg(s_ctrl, 0xd7);

	return value;
}

static void gc8034_read_otp_kgroup(struct msm_sensor_ctrl_t *s_ctrl, uint8_t page, uint8_t addr, uint8_t *buff, int size)
{
	uint8_t i;
	uint8_t regf4 = gc8034_Sensor_ReadReg(s_ctrl, 0xf4);
	gc8034_Sensor_WriteReg(s_ctrl, 0xd4, ((page << 2) & 0x3c) + ((addr >> 5) & 0x03));
	gc8034_Sensor_WriteReg(s_ctrl, 0xd5, (addr << 3) & 0xff);
	mdelay(1);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf3, 0x20);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf4, regf4 | 0x02);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf3, 0x80);
	for (i = 0; i < size; i++) {
		if (((addr + i) % 0x80) == 0) {
			gc8034_Sensor_WriteReg(s_ctrl, 0xf3, 0x00);
			gc8034_Sensor_WriteReg(s_ctrl, 0xf4, regf4 & 0xfd);
			gc8034_Sensor_WriteReg(s_ctrl, 0xd4, ((++page << 2) & 0x3c));
			gc8034_Sensor_WriteReg(s_ctrl, 0xd5, 0x00);
			mdelay(1);
			gc8034_Sensor_WriteReg(s_ctrl, 0xf3, 0x20);
			gc8034_Sensor_WriteReg(s_ctrl, 0xf4, regf4 | 0x02);
			gc8034_Sensor_WriteReg(s_ctrl, 0xf3, 0x80);
		}
		buff[i] = gc8034_Sensor_ReadReg(s_ctrl, 0xd7);
	}
	gc8034_Sensor_WriteReg(s_ctrl, 0xf3, 0x00);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf4, regf4 & 0xfd);
}

static void gc8034_gcore_read_otp_info(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint8_t  flagdd = 0;
	uint8_t i = 0, j = 0;
	uint8_t  temp = 0;
	uint8_t  total_number = 0, cnt = 0;
	uint8_t  ddtempbuff[4 * 80] = { 0 };

	memset(&gc8034_otp_info, 0, sizeof(struct gc8034_otp_t));

	/* Static Defective Pixel */
	flagdd = gc8034_read_otp(s_ctrl, 0, 0x0b);
	CDBG("GC8034_OTP_DD flag_dd=0x%x\n", flagdd);

	switch (flagdd & 0x03) {
	case 0x00:
		CDBG("GC8034_OTP_DD is Empty !!\n");
		gc8034_otp_info.dd_flag = 0x00;
		break;
	case 0x01:
		CDBG("GC8034_OTP_DD is Valid!!\n");
		total_number = gc8034_read_otp(s_ctrl, 0, 0x0c) + gc8034_read_otp(s_ctrl, 0, 0x0d);
		gc8034_otp_info.dd_flag = 0x01;
		gc8034_read_otp_kgroup(s_ctrl, 0, 0x0e, &ddtempbuff[0], 4 * total_number);
		for (i = 0; i < total_number; i++) {
			if ((ddtempbuff[4 * i + 3] & 0x80) == 0x80) {
				if ((ddtempbuff[4 * i + 3] & 0x03) == 0x03) {
					gc8034_otp_info.dd_param[cnt].x = (((uint8_t)ddtempbuff[4 * i + 1] & 0x0f) << 8) + ddtempbuff[4 * i];
					gc8034_otp_info.dd_param[cnt].y = ((uint8_t)ddtempbuff[4 * i + 2] << 4) + ((ddtempbuff[4 * i + 1] & 0xf0) >> 4);
					gc8034_otp_info.dd_param[cnt].t = 2;
					CDBG("%s:%d DD[%d].x =0x%x, DD[%d].y = 0x%x, DD[%d].t = %d\n", __func__, __LINE__, cnt, gc8034_otp_info.dd_param[cnt].x, cnt, gc8034_otp_info.dd_param[cnt].y, cnt, gc8034_otp_info.dd_param[cnt].t);
					cnt ++;
					gc8034_otp_info.dd_param[cnt].x = (((uint8_t)ddtempbuff[4 * i + 1] & 0x0f) << 8) + ddtempbuff[4 * i];
					gc8034_otp_info.dd_param[cnt].y = ((uint8_t)ddtempbuff[4 * i + 2] << 4) + ((ddtempbuff[4 * i + 1] & 0xf0) >> 4) + 1;
					gc8034_otp_info.dd_param[cnt].t = 2;
					CDBG("%s:%d DD[%d].x =0x%x DD[%d].y =0x%x DD[%d].t = %d\n", __func__, __LINE__, cnt, gc8034_otp_info.dd_param[cnt].x, cnt, gc8034_otp_info.dd_param[cnt].y, cnt, gc8034_otp_info.dd_param[cnt].t);
					cnt ++;
				}
				else {
					gc8034_otp_info.dd_param[cnt].x = (((uint8_t)ddtempbuff[4 * i + 1] & 0x0f) << 8) + ddtempbuff[4 * i];
					gc8034_otp_info.dd_param[cnt].y = ((uint8_t)ddtempbuff[4 * i + 2] << 4) + ((ddtempbuff[4 * i + 1] & 0xf0) >> 4);
					gc8034_otp_info.dd_param[cnt].t = ddtempbuff[4 * i + 3] & 0x03;
					CDBG("%s:%d DD[%d].x =0x%x, DD[%d].y = 0x%x, DD[%d].t = %d\n", __func__, __LINE__, cnt, gc8034_otp_info.dd_param[cnt].x, cnt, gc8034_otp_info.dd_param[cnt].y, cnt, gc8034_otp_info.dd_param[cnt].t);
					cnt ++;
				}
			}
		}
		gc8034_otp_info.dd_cnt = cnt;
		CDBG("GC8034_OTP : total_number = %d\n", gc8034_otp_info.dd_cnt);
		break;
	case 0x02:
	case 0x03:
		CDBG("GC8034_OTP_DD is Invalid !!\n");
		gc8034_otp_info.dd_flag = 0x02;
		break;
	default:
		break;
	}

	/* chip regs */
	gc8034_otp_info.reg_flag = gc8034_read_otp(s_ctrl, 2, 0x4e);

	if (gc8034_otp_info.reg_flag == 1)
		for (i = 0; i < 5; i++) {
			temp = gc8034_read_otp(s_ctrl, 2, 0x4f + 5 * i);
			for (j = 0; j < 2; j++)
				if (((temp >> (4 * j + 3)) & 0x01) == 0x01) {
					gc8034_otp_info.reg_page[gc8034_otp_info.reg_num] = (temp >> (4 * j)) & 0x03;
					gc8034_otp_info.reg_addr[gc8034_otp_info.reg_num] =
						gc8034_read_otp(s_ctrl, 2, 0x50 + 5 * i + 2 * j);
					gc8034_otp_info.reg_value[gc8034_otp_info.reg_num] =
						gc8034_read_otp(s_ctrl, 2, 0x50 + 5 * i + 2 * j + 1);
					gc8034_otp_info.reg_num++;
				}
		}
}

static void gc8034_gcore_check_prsel(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint8_t product_level = 0;

	product_level = gc8034_read_otp(s_ctrl, 2, 0x68) & 0x07;

	if ((product_level == 0x00) || (product_level == 0x01)) {
		gc8034_Sensor_WriteReg(s_ctrl, 0xfe, 0x00);
	    gc8034_Sensor_WriteReg(s_ctrl, 0xd2, 0xcb);
	} else {
		gc8034_Sensor_WriteReg(s_ctrl, 0xfe, 0x00);
	    gc8034_Sensor_WriteReg(s_ctrl, 0xd2, 0xc3);
	}
}

static void gc8034_gcore_update_dd(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint8_t i = 0, j = 0;
	uint8_t temp_val0 = 0, temp_val1 = 0, temp_val2 = 0;
	gc8034_dd_t dd_temp = {0, 0, 0};
	if (gc8034_otp_info.dd_flag == 0x01) {
		printk("GC8034_OTP_AUTO_DD start !\n");
		for(i = 0; i < gc8034_otp_info.dd_cnt; i++) {
#if defined(GC8034_MIRROR_H) || defined(GC8034_MIRROR_HV)
			switch(gc8034_otp_info.dd_param[i].t) {
			case 0:
				gc8034_otp_info.dd_param[i].x = DD_WIDTH - gc8034_otp_info.dd_param[i].x + 1;
				break;
			case 1:
				gc8034_otp_info.dd_param[i].x = DD_WIDTH - gc8034_otp_info.dd_param[i].x - 1;
				break;
			default:
				gc8034_otp_info.dd_param[i].x = DD_WIDTH - gc8034_otp_info.dd_param[i].x;
				break;
			}
#endif
#if defined(GC8034_MIRROR_V) || defined(GC8034_MIRROR_HV)
			gc8034_otp_info.dd_param[i].y = DD_HEIGHT -gc8034_otp_info. dd_param[i].y + 1;
#endif
		}
		for(i = 0; i < gc8034_otp_info.dd_cnt - 1; i++) {
			for(j = i + 1; j < gc8034_otp_info.dd_cnt; j++) {
				if(gc8034_otp_info.dd_param[i].y * DD_WIDTH + gc8034_otp_info.dd_param[i].x
				 > gc8034_otp_info.dd_param[j].y * DD_WIDTH + gc8034_otp_info.dd_param[j].x) {
					dd_temp.x = gc8034_otp_info.dd_param[i].x;
					dd_temp.y = gc8034_otp_info.dd_param[i].y;
					dd_temp.t = gc8034_otp_info.dd_param[i].t;
					gc8034_otp_info.dd_param[i].x = gc8034_otp_info.dd_param[j].x;
					gc8034_otp_info.dd_param[i].y = gc8034_otp_info.dd_param[j].y;
					gc8034_otp_info.dd_param[i].t = gc8034_otp_info.dd_param[j].t;
					gc8034_otp_info.dd_param[j].x = dd_temp.x;
					gc8034_otp_info.dd_param[j].y = dd_temp.y;
					gc8034_otp_info.dd_param[j].t = dd_temp.t;
				}
			}
		}
		gc8034_Sensor_WriteReg(s_ctrl, 0xfe, 0x01);
		gc8034_Sensor_WriteReg(s_ctrl, 0xbe, 0x00);
		gc8034_Sensor_WriteReg(s_ctrl, 0xa9, 0x01);
		for (i = 0; i < gc8034_otp_info.dd_cnt; i++) {
			temp_val0 = gc8034_otp_info.dd_param[i].x & 0x00ff;
			temp_val1 = ((gc8034_otp_info.dd_param[i].y & 0x000f) << 4) + ((gc8034_otp_info.dd_param[i].x & 0x0f00)>>8);
			temp_val2 = (gc8034_otp_info.dd_param[i].y & 0x0ff0) >> 4;
			gc8034_Sensor_WriteReg(s_ctrl, 0xaa, i);
			gc8034_Sensor_WriteReg(s_ctrl, 0xac, temp_val0);
			gc8034_Sensor_WriteReg(s_ctrl, 0xac, temp_val1);
			gc8034_Sensor_WriteReg(s_ctrl, 0xac, temp_val2);
			gc8034_Sensor_WriteReg(s_ctrl, 0xac, gc8034_otp_info.dd_param[i].t);

			printk("GC8034_OTP_GC val0 = 0x%x , val1 = 0x%x , val2 = 0x%x \n", temp_val0, temp_val1, temp_val2);
			printk("GC8034_OTP_GC x = %d , y = %d \n", ((temp_val1&0x0f)<<8) + temp_val0, (temp_val2<<4) + ((temp_val1&0xf0)>>4));
		}

		gc8034_Sensor_WriteReg(s_ctrl, 0xbe, 0x01);
		gc8034_Sensor_WriteReg(s_ctrl, 0xfe, 0x00);
	}
}

static void gc8034_gcore_update_chipversion(struct msm_sensor_ctrl_t *s_ctrl)
{
	uint8_t i = 0;

	printk("GC8034_OTP_UPDATE_CHIPVERSION:reg_num = %d\n", gc8034_otp_info.reg_num);

	if (gc8034_otp_info.reg_flag)
		for (i = 0; i < gc8034_otp_info.reg_num; i++) {
			gc8034_Sensor_WriteReg(s_ctrl, 0xfe, gc8034_otp_info.reg_page[i]);
			gc8034_Sensor_WriteReg(s_ctrl, gc8034_otp_info.reg_addr[i], gc8034_otp_info.reg_value[i]);
			printk("GC8034_OTP_UPDATE_CHIP_VERSION: P%d:0x%x -> 0x%x\n",
			gc8034_otp_info.reg_page[i], gc8034_otp_info.reg_addr[i], gc8034_otp_info.reg_value[i]);
		}
}

static void gc8034_gcore_enable_otp(struct msm_sensor_ctrl_t *s_ctrl, bool otp_state)
{
	uint8_t otp_clk = 0, otp_en = 0;

	otp_clk = gc8034_Sensor_ReadReg(s_ctrl, 0xf2);
	otp_en = gc8034_Sensor_ReadReg(s_ctrl, 0xf4);
	if (otp_state) {
		otp_clk = otp_clk | 0x01;
		otp_en = otp_en | 0x08;
		gc8034_Sensor_WriteReg(s_ctrl, 0xf2, otp_clk);
		gc8034_Sensor_WriteReg(s_ctrl, 0xf4, otp_en);
		printk("GC8034_OTP: Enable OTP!\n");
	} else {
		otp_en = otp_en & 0xf7;
		otp_clk = otp_clk & 0xfe;
		gc8034_Sensor_WriteReg(s_ctrl, 0xf4, otp_en);
		gc8034_Sensor_WriteReg(s_ctrl, 0xf2, otp_clk);
		printk("GC8034_OTP: Disable OTP!\n");
	}
}

void gc8034_gcore_identify_otp(struct msm_sensor_ctrl_t *s_ctrl)
{
	gc8034_Sensor_WriteReg(s_ctrl, 0xfe, 0x00);
	gc8034_Sensor_WriteReg(s_ctrl, 0xfe, 0x00);
	gc8034_Sensor_WriteReg(s_ctrl, 0xfe, 0x00);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf2, 0x01);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf4, 0x88);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf5, 0x19);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf6, 0x44);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf7, 0x97);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf8, 0x63);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf9, 0x00);
	gc8034_Sensor_WriteReg(s_ctrl, 0xfa, 0x45);
	gc8034_Sensor_WriteReg(s_ctrl, 0xfc, 0xee);

	mdelay(100);
	gc8034_gcore_enable_otp(s_ctrl, otp_open);
	gc8034_gcore_read_otp_info(s_ctrl);
	gc8034_gcore_update_dd(s_ctrl);
	gc8034_gcore_check_prsel(s_ctrl);
	gc8034_gcore_update_chipversion(s_ctrl);
	gc8034_gcore_enable_otp(s_ctrl, otp_close);
	gc8034_Sensor_WriteReg(s_ctrl, 0xfc, 0x00);
	gc8034_Sensor_WriteReg(s_ctrl, 0xf7, 0x95);
	gc8034_Sensor_WriteReg(s_ctrl, 0xfc, 0x00);
	gc8034_Sensor_WriteReg(s_ctrl, 0xfc, 0x00);
	gc8034_Sensor_WriteReg(s_ctrl, 0xfc, 0xee);
}
/* otp end */
