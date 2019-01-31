/*
 * Copyright (C) 2015 MediaTek Inc.
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
#include <linux/slab.h>

#include "kd_camera_typedef.h"
//#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"


#define LOG_ERR(format,...) pr_err("s5k4h7yx "format,## __VA_ARGS__)
#define LOG_INFO(format,...) pr_info("s5k4h7yx "format,## __VA_ARGS__)

#define DUMP_OTP 0
#define USING_AWB_CALI_INSENSOR     0

#define S5K4H7YX_INFO_AWB_PAGE		21
#define S5K4H7YX_INFO_AWB_GROUP1_ADDR      0x0A04
#define S5K4H7YX_INFO_AWB_GROUP2_ADDR      0x0A1E
#define S5K4H7YX_INFO_AWB_GROUP3_ADDR      0x0A38

#define S5K4H7YX_LSC_PAGE		22
#define S5K4H7YX_LSC_GROUP1_ADDR      0x0A12
#define S5K4H7YX_LSC_GROUP2_ADDR      0x0A13

#define MAX_EEPROM_BYTE 0x1FFF
#define CHECKSUM_OK_FLAG_ADDR 0x1FFF-1
#define FRONT_DEVICE_ID 1
extern int g_read_flag[3];
extern char g_otp_buf[3][MAX_EEPROM_BYTE];
extern int do_checksum(unsigned char *buf,
    unsigned int first, unsigned int last, unsigned int checksum);

extern kal_uint16 otp_4h7_read_cmos_sensor(kal_uint32 addr);
extern void otp_4h7_write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para);
/*
    g_otp_buf[1][0]
        .
        .
        .
    info_awb
        26 bytes
        .
        .
        .
        
    g_otp_buf[1][25] checksum_info_awb 0 ~ 24

 
    g_otp_buf[1][26]   
        .
        .
       lsc
       360 bytes
        .
        .
    g_otp_buf[1][385]    
    g_otp_buf[1][386]checksum_LSC 26 ~ 385
*/

typedef struct {
    unsigned short info_awb_group;
    unsigned short lsc_group;
    unsigned short vender_id;
    unsigned short sensor_id;
    unsigned short product_id;
    unsigned int unit_r;
    unsigned int unit_gr;
    unsigned int unit_gb;
    unsigned int unit_b;
    unsigned int golden_r;
    unsigned int golden_gr;
    unsigned int golden_gb;
    unsigned int golden_b;
}OTP;

static OTP otp_data_info = {0};

static unsigned short selective_read_region(int pageidx,unsigned int addr)
{
	unsigned short get_byte = 0;
 
	otp_4h7_write_cmos_sensor_8(0x0A02,pageidx);
	otp_4h7_write_cmos_sensor_8(0x0A00,0x01);
	do
	{
		mdelay(1);
		get_byte = otp_4h7_read_cmos_sensor(0x0A01);
	}while((get_byte & 0x01) != 1);

  
	get_byte = otp_4h7_read_cmos_sensor(addr);
	otp_4h7_write_cmos_sensor_8(0x0A00,0x00);

	return get_byte;
}
#if DUMP_OTP
static int global_addr = 0;
#endif
/**********************************************************
 * get_4h7_page_data
 * get page data
 * return true or false
 * *******************************************************/
void get_4h7_page_data(int pageidx,int start,int end, unsigned char *pdata)
{
	unsigned short get_byte=0;
	unsigned int addr = 0x0A04;
    if(start == end){
        start = 0x0A04;
        end =   0x0A43;
    }
	otp_4h7_write_cmos_sensor_8(0x0A02,pageidx);
	otp_4h7_write_cmos_sensor_8(0x0A00,0x01);

	do
	{
		mdelay(1);
		get_byte = otp_4h7_read_cmos_sensor(0x0A01);
	}while((get_byte & 0x01) != 1);

	for(addr = start; addr <= end; addr++){
		*pdata = otp_4h7_read_cmos_sensor(addr);
#if DUMP_OTP
        LOG_INFO("s5k4h7yx page idx=%d,addr=0x%x,value=%d(0x%x)global_addr=0x%x(%d)",pageidx,addr,*pdata,*pdata,global_addr,global_addr);
        global_addr++;
#endif
        pdata++;
	}

	otp_4h7_write_cmos_sensor_8(0x0A00,0x00);
}

/*********************************************************
 * read_4h7_page
 * read_Page1~Page21 of data
 * return true or false
 ********************************************************/
int read_4h7_page(int page_start,int page_end,unsigned char *pdata)
{
	int bresult = 0, i=0;
	int st_page_start = page_start;
	if (page_start <= 0 || page_end > 21){
		bresult = -1;
		printk(" OTP page_end is large!");
		return bresult;
	}
	for(; st_page_start <= page_end; st_page_start++,i++){
		get_4h7_page_data(st_page_start,0,0, pdata+64*i);
	}
	return bresult;
}


/*****************************************************
 * cal_rgb_gain
 * **************************************************/
static void cal_rgb_gain(int* r_gain, int* g_gain, int* b_gain, unsigned int r_ration, unsigned int b_ration)
{
	int gain_default = 0x0100;
	if(r_ration >= 1){
		if(b_ration >= 1){
			*g_gain = gain_default;
			*r_gain = (int)((gain_default*1000 * r_ration + 500)/1000);
			*b_gain = (int)((gain_default*1000 * b_ration + 500)/1000);
		}
		else{
			*b_gain = gain_default;
			*g_gain = (int)((gain_default * 1000 / b_ration + 500)/1000);
			*r_gain = (int)((gain_default * r_ration *1000 / b_ration + 500)/1000);
		}
	}
	else{
		if(b_ration >= 1){
			*r_gain = gain_default;
			*g_gain = (int)((gain_default * 1000 / r_ration + 500)/1000);
			*b_gain = (int)((gain_default * b_ration*1000 / r_ration + 500) / 1000);
		}
		else{
			if(r_ration >= b_ration){
				*b_gain = gain_default;
				*g_gain = (int)((gain_default * 1000 / b_ration + 500) / 1000);
				*r_gain = (int)((gain_default * r_ration * 1000 / b_ration + 500) / 1000);
			}
			else{
				*r_gain = gain_default;
				*g_gain = (int)((gain_default * 1000 / r_ration + 500)/1000);
				*b_gain = (int)((gain_default * b_ration * 1000 / r_ration + 500) / 1000);
			}
		}
	}
}


void apply_s5k4h7yx_awb_otp(void)
{
    int r_gain,g_gain,b_gain,addr = 17;
    unsigned int unit_rg,unit_bg,unit_grgb,golden_rg,golden_bg,golden_grgb;
    unsigned int r_ratio,b_ratio;
    static char r_gain_h=0, r_gain_l=0, g_gain_h=0, g_gain_l=0, b_gain_h=0, b_gain_l=0;

    if(0x88 == g_otp_buf[FRONT_DEVICE_ID][CHECKSUM_OK_FLAG_ADDR]){

        otp_data_info.unit_r = g_otp_buf[FRONT_DEVICE_ID][addr++];
        otp_data_info.unit_gr = g_otp_buf[FRONT_DEVICE_ID][addr++];
        otp_data_info.unit_gb = g_otp_buf[FRONT_DEVICE_ID][addr++];
        otp_data_info.unit_b = g_otp_buf[FRONT_DEVICE_ID][addr++];

        otp_data_info.golden_r = g_otp_buf[FRONT_DEVICE_ID][addr++];
        otp_data_info.golden_gr = g_otp_buf[FRONT_DEVICE_ID][addr++];
        otp_data_info.golden_gb = g_otp_buf[FRONT_DEVICE_ID][addr++];
        otp_data_info.golden_b = g_otp_buf[FRONT_DEVICE_ID][addr++];

        LOG_INFO("UNIT: r=(0x%x)%d, gr=(0x%x)%d, gb=(0x%x)%d, b=(0x%x)%d",
                             otp_data_info.unit_r,otp_data_info.unit_r,
                             otp_data_info.unit_gr,otp_data_info.unit_gr,
                             otp_data_info.unit_gb,otp_data_info.unit_gb,
                             otp_data_info.unit_b,otp_data_info.unit_b);

        LOG_INFO("GOLD: r=(0x%x)%d, gr=(0x%x)%d, gb=(0x%x)%d, b=(0x%x)%d",
                             otp_data_info.golden_r,otp_data_info.golden_r,
                             otp_data_info.golden_gr,otp_data_info.golden_gr,
                             otp_data_info.golden_gb,otp_data_info.golden_gb,
                             otp_data_info.golden_b,otp_data_info.golden_b);

        unit_rg = (unsigned int)(otp_data_info.unit_r * 1000 / otp_data_info.unit_gr + 500);
        unit_grgb = (unsigned int)((otp_data_info.unit_gb+ otp_data_info.unit_gr)*1000/2 + 500);  	
	    unit_bg = (unsigned int)(otp_data_info.unit_b*1000 / otp_data_info.unit_gr + 500);

        golden_rg = (unsigned int)(otp_data_info.golden_r * 1000 / otp_data_info.golden_gr + 500);
        golden_grgb = (unsigned int)((otp_data_info.golden_gb+ otp_data_info.golden_gr)*1000/2 + 500);  	
	    golden_bg = (unsigned int)(otp_data_info.golden_b*1000 / otp_data_info.golden_gr + 500);

	    r_ratio = (unsigned int)((golden_rg * 1000 /unit_rg + 500)/1000);
	    b_ratio = (unsigned int)((golden_bg * 1000 /unit_bg + 500)/1000); 

        cal_rgb_gain(&r_gain, &g_gain, &b_gain, r_ratio, b_ratio);    

        LOG_INFO("r_ratio=%u b_ratio=%u r_g=(0x%x)%d gr_gb=(0x%x)%d b_g=(0x%x)%d",
                        r_ratio,b_ratio,r_gain,r_gain,g_gain,g_gain,b_gain,b_gain);

        r_gain_h = (r_gain >> 8) & 0xff;
	    r_gain_l = (r_gain >> 0) & 0xff;

    	g_gain_h = (g_gain >> 8) & 0xff;
    	g_gain_l = (g_gain >> 0) & 0xff;

    	b_gain_h = (b_gain >> 8) & 0xff;
    	b_gain_l = (b_gain >> 0) & 0xff;

        LOG_INFO("rh=(0x%x)%d,rl=(0x%x)%d,gh=(0x%x)%d,gl=(0x%x)%d,bh=(0x%x)%d,bl=(0x%x)%d",
                               r_gain_h,r_gain_h,r_gain_l,r_gain_l,g_gain_h,g_gain_h,
                               g_gain_l,g_gain_l,b_gain_h,b_gain_h,b_gain_l,b_gain_l);

        otp_4h7_write_cmos_sensor_8(0x0210,r_gain_h);
    	otp_4h7_write_cmos_sensor_8(0x0211,r_gain_l);

    	otp_4h7_write_cmos_sensor_8(0x020E,g_gain_h);
    	otp_4h7_write_cmos_sensor_8(0x020F,g_gain_l);

    	otp_4h7_write_cmos_sensor_8(0x0214,g_gain_h);
    	otp_4h7_write_cmos_sensor_8(0x0215,g_gain_l);

    	otp_4h7_write_cmos_sensor_8(0x0212,b_gain_h);
    	otp_4h7_write_cmos_sensor_8(0x0213,b_gain_l);

    }else{
        LOG_ERR("OTP DATA ERROR,PLEASE CHECK BOOT KERNEL LOG");
    }
}

int read_s5k4h7yx_otp(void)
{
    int ret = 0;
#if DUMP_OTP
    int i = 0;
#endif
    if(0x88 == g_otp_buf[FRONT_DEVICE_ID][CHECKSUM_OK_FLAG_ADDR]){
        LOG_INFO("OTP DATA IS GOOD ,RETURN ...");
        return ret;
    }
#if DUMP_OTP
    global_addr = 0;
#endif
    memset(&otp_data_info,0,sizeof(OTP));

	if(0x55 == selective_read_region(S5K4H7YX_INFO_AWB_PAGE,S5K4H7YX_INFO_AWB_GROUP1_ADDR)){

        otp_data_info.info_awb_group = 1;

    }else if(0x55 == selective_read_region(S5K4H7YX_INFO_AWB_PAGE,S5K4H7YX_INFO_AWB_GROUP2_ADDR)){
    
        otp_data_info.info_awb_group = 2;
        
    }else if(0x55 == selective_read_region(S5K4H7YX_INFO_AWB_PAGE,S5K4H7YX_INFO_AWB_GROUP3_ADDR)){

        otp_data_info.info_awb_group = 3;
        
    }else{

        LOG_ERR("read info_awb_group failed");
    }

    switch(otp_data_info.info_awb_group){

    case 1:
        get_4h7_page_data(21,0x0A04,0x0A1D,&g_otp_buf[FRONT_DEVICE_ID][0]);
        break;
    case 2:
        get_4h7_page_data(21,0x0A1E,0x0A37,&g_otp_buf[FRONT_DEVICE_ID][0]);
        break;
    case 3:
        get_4h7_page_data(21,0x0A38,0x0A43,&g_otp_buf[FRONT_DEVICE_ID][0]);
        get_4h7_page_data(22,0x0A04,0x0A11,&g_otp_buf[FRONT_DEVICE_ID][12]);
        break;
    default:

        LOG_ERR("ERROR INFO_AWB_GEOUP");
        ret = -1;

    }

    if(0x55 == selective_read_region(S5K4H7YX_LSC_PAGE,S5K4H7YX_LSC_GROUP1_ADDR)){

        otp_data_info.lsc_group = 1;

    }else if(0x55 == selective_read_region(S5K4H7YX_LSC_PAGE,S5K4H7YX_LSC_GROUP2_ADDR)){
    
        otp_data_info.lsc_group = 2;
        
    }else{

        LOG_ERR("read lsc_group failed");
        ret = -1;
    }

    switch(otp_data_info.lsc_group){

    case 1:
        read_4h7_page(1,5,&g_otp_buf[FRONT_DEVICE_ID][26]);
        get_4h7_page_data(6,0x0A04,0x0A2B,&g_otp_buf[FRONT_DEVICE_ID][346]);
        g_otp_buf[FRONT_DEVICE_ID][386] = selective_read_region(22,0x0A14);
        break;
    case 2:
        get_4h7_page_data(6,0x0A2C,0x0A43,&g_otp_buf[FRONT_DEVICE_ID][26]);
        read_4h7_page(7,11,&g_otp_buf[FRONT_DEVICE_ID][50]);
        get_4h7_page_data(12,0x0A04,0x0A13,&g_otp_buf[FRONT_DEVICE_ID][370]);
        g_otp_buf[FRONT_DEVICE_ID][386] = selective_read_region(22,0x0A15);
        break;
    default:

        LOG_ERR("ERROR LSC_GEOUP");
        ret = -1;
   }

   if(!ret){
        LOG_INFO("READ OTP DATA SUCCESS");
        g_read_flag[FRONT_DEVICE_ID] = 1;
   }

   if(do_checksum(&g_otp_buf[FRONT_DEVICE_ID][0],1,24,25)){

         LOG_ERR("ERROR AWB_INFO CheckSum ERROR");
         ret = -1;
   }else{

         LOG_INFO("checksum AWB INFO ok");

   }

   if(do_checksum(&g_otp_buf[FRONT_DEVICE_ID][0],26,385,386)){

         LOG_ERR("ERROR LSC CheckSum ERROR");
         ret = -1;
   }else{

         LOG_INFO("checksum LSC s ok");

   }
   if(!ret){
        LOG_INFO("GOOD! READ OTP DATA AND CHECKSUM OK");
        g_otp_buf[FRONT_DEVICE_ID][CHECKSUM_OK_FLAG_ADDR] = 0x88;//0x88 means good
   }
#if DUMP_OTP
   for(i=0; i < 400; i++){

        LOG_INFO("addr=(0x%x)%d value=(0x%x)%d",
                i,i,g_otp_buf[FRONT_DEVICE_ID][i],g_otp_buf[FRONT_DEVICE_ID][i]);
   }
#endif
    return ret;
}


int  apply_s5k4h7yx_otp(void)
{
	int result = 0;
    #if USING_AWB_CALI_INSENSOR
    apply_s5k4h7yx_awb_otp();
    #endif
    if(0x88 == g_otp_buf[FRONT_DEVICE_ID][CHECKSUM_OK_FLAG_ADDR]){
        otp_4h7_write_cmos_sensor_8(0x0B00,0x01);//lsc enable
    }
	return  result;
}

int get_s5k4h7yx_vendor_id(void)
{
   read_s5k4h7yx_otp();
   return (g_otp_buf[FRONT_DEVICE_ID][1] << 8) | g_otp_buf[FRONT_DEVICE_ID][2];
  
}
