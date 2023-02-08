/*
 * Copyright (C) 2017 MediaTek Inc.
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

/* *****************************************************************************
 *
 * Filename:
 * ---------
 *     s5k5e9mipi_Sensor.c
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     Source code of Sensor driver
 *
 *
 *------------------------------------------------------------------------------
 * Upper this line, this part is controlled by CC/CQ. DO NOT MODIFY!!
 *============================================================================
 ****************************************************************************/

#include <linux/videodev2.h>
#include <linux/i2c.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/atomic.h>
#include <linux/types.h>

#include "kd_camera_typedef.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"
#include "s5k5e9_sunny_front_mipi_raw.h"

/****************************Modify following Strings for debug****************************/
#define PFX "s5k5e9_sunny_front_camera_sensor"
#define LOG_1 LOG_INF("s5k5e9,MIPI 2LANE\n")
#define LOG_2 \
LOG_INF("preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n")
/****************************   Modify end    *******************************************/
#define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __func__, ##args)

//#define S5K5E9_OTP
// extern unsigned char fusion_id_front[96];
// extern unsigned char sn_front[96];

#ifdef S5K5E9_OTP
#define GAIN_DEFAULT       0x0100
#define GAIN_GREEN1_ADDR   0x020E
#define GAIN_BLUE_ADDR     0x0212
#define GAIN_RED_ADDR      0x0210
#define GAIN_GREEN2_ADDR   0x0214
#define CAM_DISABLE_OTP 1
#if CAM_DISABLE_OTP
//extern int cqcq_disable_otp;   //change by caodeng
#endif

//int module_integrator_id;
//int lens_id;
int rg_ratio;
int bg_ratio;
int golden_rg_ratio;
int golden_bg_ratio;

//struct otp_struct current_otp; // add by caodeng
static unsigned short R_GAIN = 0;
static unsigned short B_GAIN = 0;
static unsigned short Gr_GAIN = 0;
static unsigned short Gb_GAIN = 0;
static unsigned short G_GAIN = 0;

int cqcq_disable_otp=0;
#endif

static DEFINE_SPINLOCK(imgsensor_drv_lock);

static MUINT32 g_sync_mode = SENSOR_NO_SYNC_MODE;
static int first_flag = 0;
static  imgsensor_info_struct imgsensor_info = {
    .sensor_id = S5K5E9_SUNNY_FRONT_SENSOR_ID,
//  .checksum_value = 0x2ae69154,
    .checksum_value = 0x42d95d37,
    .pre = {
		.pclk = 190000000,              //record different mode's pclk
        .linelength = 3112,    /* record different mode's linelength */
        .framelength = 2030,         //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2592,    /* record different mode's width of grabwindow */
        .grabwindow_height = 1944,    /* record different mode's height of grabwindow */
        /*     following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario    */
        .mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 175200000,
        /*     following for GetDefaultFramerateByScenario()    */
        .max_framerate = 300,
    },
    .cap = {
        .pclk =190000000,
        .linelength = 3112,
        .framelength = 2030, //2035: 30.0019
        .startx =0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 85,
		.mipi_pixel_rate = 175200000,
        .max_framerate = 300,
    },
    .normal_video = {
        .pclk =190000000,
        .linelength = 3112,
        .framelength = 2030,
        .startx =0,
        .starty = 0,
        .grabwindow_width = 2592,
        .grabwindow_height = 1944,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
		.mipi_pixel_rate = 175200000,
    },

/* code at 2022/08/29 start */
    .margin = 5,
    .min_shutter = 2,
    .min_gain = 64,
    .max_gain = 1024,
    .min_gain_iso = 50,
    .gain_step = 2,
    .gain_type = 2,
/* code at 2022/08/29 end */
    .max_frame_length = 0xffff,
    .ae_shut_delay_frame = 0,
/* code at 2022/09/1 start */
    .ae_sensor_gain_delay_frame = 0,
/* code at 2022/09/1 end */
    .ae_ispGain_delay_frame = 2,
    .ihdr_support = 0,    /* 1, support; 0,not support */
    .ihdr_le_firstline = 0,    /* 1,le first ; 0, se first */
    .sensor_mode_num = 7,    /* support sensor mode num */

    .cap_delay_frame = 3,
    .pre_delay_frame = 3,
    .video_delay_frame = 3,
    .hs_video_delay_frame = 3,
    .slim_video_delay_frame = 3,

    .isp_driving_current = ISP_DRIVING_6MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2,    /* 0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 */
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
    .mclk = 24,
    .mipi_lane_num = SENSOR_MIPI_2_LANE,
    .i2c_addr_table = {0x20,0x5A,0xff},
	.i2c_speed = 400,
};


static  imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,    /* mirrorflip information */
    .sensor_mode = IMGSENSOR_MODE_INIT,    /* IMGSENSOR_MODE enum value,record current sensor mode,
                         * such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
                         */
    .shutter = 0x3D0,    /* current shutter */
    .gain = 0x100,        /* current gain */
    .dummy_pixel = 0,    /* current dummypixel */
    .dummy_line = 0,    /* current dummyline */
    .current_fps = 300,    /* full size current fps : 24fps for PIP, 30fps for Normal or ZSD */
    .autoflicker_en = KAL_FALSE,    /* auto flicker enable: KAL_FALSE for disable auto flicker,
                     * KAL_TRUE for enable auto flicker
                     */
    .test_pattern = KAL_FALSE,    /* test pattern mode or not.
                     * KAL_FALSE for in test pattern mode,
                     * KAL_TRUE for normal output
                     */
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,    /* current scenario id */
    .ihdr_en = 0,        /* sensor need support LE, SE with HDR feature */
    .i2c_write_id = 0x20,
};


/* Sensor output window information */
static struct SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] = {
    {2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944},    /* Preview */
    {2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0000, 0000, 2592, 1944, 0, 0, 2592, 1944},    /* capture */
	{2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0, 0, 2592, 1944, 0, 0, 2592, 1944 },   /* video */
	{2592, 1944, 0, 0, 2592, 1944, 2592, 1944, 0, 0, 2592, 1944, 0, 0, 2592, 1944 },    /* high speed video */
	{2592, 1944, 16, 252, 2560, 1440, 1280,  720, 0, 0, 1280,  720, 0, 0, 1280,  720 }    /*  slim video */

};


static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
	char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };

	iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);
	return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    char pu_send_cmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
    char pusendcmd[3] = { (char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF) };

    iWriteRegI2C(pusendcmd, 3, imgsensor.i2c_write_id);
}


static void set_dummy(void)
{
    LOG_INF("dummyline = %d, dummypixels = %d\n", imgsensor.dummy_line, imgsensor.dummy_pixel);

    write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
    write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
    write_cmos_sensor_8(0x0342, imgsensor.line_length >> 8);
    write_cmos_sensor_8(0x0343, imgsensor.line_length & 0xFF);

}                /*      set_dummy  */

static kal_uint32 return_sensor_id(void)
{
    return ((read_cmos_sensor(0x0000) << 8) | read_cmos_sensor(0x0001));
}


static void set_max_framerate(UINT16 framerate, kal_bool min_framelength_en)
{
    /* kal_int16 dummy_line; */
    kal_uint32 frame_length = imgsensor.frame_length;
    /* unsigned long flags; */

    LOG_INF("framerate = %d, min framelength should enable? %d\n", framerate,
        min_framelength_en);

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    imgsensor.frame_length = (frame_length > imgsensor.min_frame_length)
        ? frame_length : imgsensor.min_frame_length;
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    /* dummy_line = frame_length - imgsensor.min_frame_length; */
    /* if (dummy_line < 0) */
    /* imgsensor.dummy_line = 0; */
    /* else */
    /* imgsensor.dummy_line = dummy_line; */
    /* imgsensor.frame_length = frame_length + imgsensor.dummy_line; */
    if (imgsensor.frame_length > imgsensor_info.max_frame_length) {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if (min_framelength_en)
        imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);
    set_dummy();
}                /*      set_max_framerate  */


static void write_shutter(kal_uint16 shutter)
{
    kal_uint16 realtime_fps = 0;
    /* kal_uint32 frame_length = 0; */


    /* if shutter bigger than frame_length, should extend frame length first */
    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin))
        ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if (realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296, 0);
        else if (realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146, 0);
        else {
            /* Extend frame length */
            write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
            write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
        }
    } else {
        /* Extend frame length */
        write_cmos_sensor_8(0x0340, imgsensor.frame_length >> 8);
        write_cmos_sensor_8(0x0341, imgsensor.frame_length & 0xFF);
    }

    /* Update Shutter */
    /* write_cmos_sensor_8(0x0104, 0x01);   //group hold */
    write_cmos_sensor_8(0x0202, shutter >> 8);
    write_cmos_sensor_8(0x0203, shutter & 0xFF);
    /* write_cmos_sensor_8(0x0104, 0x00);   //group hold */

    LOG_INF("shutter =%d, framelength =%d\n", shutter, imgsensor.frame_length);

    /* LOG_INF("frame_length = %d ", frame_length); */

}                /*      write_shutter  */



/*************************************************************************
 * FUNCTION
 *    set_shutter
 *
 * DESCRIPTION
 *    This function set e-shutter of sensor to change exposure time.
 *
 * PARAMETERS
 *    iShutter : exposured lines
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static void set_shutter(kal_uint16 shutter)
{
    unsigned long flags;

    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    imgsensor.shutter = shutter;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);

    write_shutter(shutter);
}                /*      set_shutter */


#if 0
static kal_uint16 gain2reg(const kal_uint16 gain)
{
    return gain >> 1;
}
#endif

/*************************************************************************
 * FUNCTION
 *    set_gain
 *
 * DESCRIPTION
 *    This function is to set global gain to sensor.
 *
 * PARAMETERS
 *    iGain : sensor global gain(base: 0x40)
 *
 * RETURNS
 *    the actually gain set to sensor.
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;

    /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
    /* [0:3] = N meams N /16 X      */
    /* [4:9] = M meams M X           */
    /* Total gain = M + N /16 X   */

    /*  */


    reg_gain = gain >> 1;
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    /* write_cmos_sensor_8(0x0104, 0x01);   //group hold */
    write_cmos_sensor_8(0x0204, reg_gain >> 8);
    write_cmos_sensor_8(0x0205, reg_gain & 0xFF);
    /* write_cmos_sensor_8(0x0104, 0x00); */

    return gain;
}                /*      set_gain  */



/* defined but not used */
static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
    LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n", le, se, gain);
    if (imgsensor.ihdr_en) {

        spin_lock(&imgsensor_drv_lock);
        if (le > imgsensor.min_frame_length - imgsensor_info.margin)
            imgsensor.frame_length = le + imgsensor_info.margin;
        else
            imgsensor.frame_length = imgsensor.min_frame_length;
        if (imgsensor.frame_length > imgsensor_info.max_frame_length)
            imgsensor.frame_length = imgsensor_info.max_frame_length;
        spin_unlock(&imgsensor_drv_lock);
        if (le < imgsensor_info.min_shutter)
            le = imgsensor_info.min_shutter;
        if (se < imgsensor_info.min_shutter)
            se = imgsensor_info.min_shutter;


        /* Extend frame length first */
        write_cmos_sensor_8(0x380e, imgsensor.frame_length >> 8);
        write_cmos_sensor_8(0x380f, imgsensor.frame_length & 0xFF);

        write_cmos_sensor_8(0x3502, (le << 4) & 0xFF);
        write_cmos_sensor_8(0x3501, (le >> 4) & 0xFF);
        write_cmos_sensor_8(0x3500, (le >> 12) & 0x0F);

        write_cmos_sensor_8(0x3508, (se << 4) & 0xFF);
        write_cmos_sensor_8(0x3507, (se >> 4) & 0xFF);
        write_cmos_sensor_8(0x3506, (se >> 12) & 0x0F);

        set_gain(gain);
    }

}



static void set_mirror_flip(kal_uint8 image_mirror)
{
    LOG_INF("image_mirror = %d\n", image_mirror);

    /********************************************************
     *
     *   0x3820[2] ISP Vertical flip
     *   0x3820[1] Sensor Vertical flip
     *
     *   0x3821[2] ISP Horizontal mirror
     *   0x3821[1] Sensor Horizontal mirror
     *
     *   ISP and Sensor flip or mirror register bit should be the same!!
     *
     ********************************************************/

    switch (image_mirror) {
    case IMAGE_NORMAL:
        write_cmos_sensor_8(0x0101, 0x00);
        break;
    case IMAGE_H_MIRROR:
        write_cmos_sensor_8(0x0101, 0x01);
        break;
    case IMAGE_V_MIRROR:
        write_cmos_sensor_8(0x0101, 0x02);
        break;
    case IMAGE_HV_MIRROR:
        write_cmos_sensor_8(0x0101, 0x03);
        break;
    default:
        LOG_INF("Error image_mirror setting\n");
    }

}

/*************************************************************************
 * FUNCTION
 *    night_mode
 *
 * DESCRIPTION
 *    This function night mode of sensor.
 *
 * PARAMETERS
 *    bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
#if 0
static void night_mode(kal_bool enable)
{
    /*No Need to implement this function*/
}                /*      night_mode      */
#endif

static void sensor_set_sync_mode(void)
{
    LOG_INF("E\n");

    /* streaming off */
    write_cmos_sensor_8(0x0100, 0x00);

    if (g_sync_mode == SENSOR_MASTER_SYNC_MODE) {
        LOG_INF("set to master mode\n");
        /* master mode */
        write_cmos_sensor_8(0x3C02, 0x0F);
    } else {
        LOG_INF("set to no sync mode\n");
    }

    /* streaming on */
    write_cmos_sensor_8(0x0100, 0x01);
}
static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    //imgsensor.i2c_write_id = 0x20;
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte,1,imgsensor.i2c_write_id);
    return get_byte;
}

static void check_streamoff(void)
{
    unsigned int i = 0;
    int timeout = (10000 / imgsensor.current_fps) + 1;

    mdelay(3);
    for (i = 0; i < timeout; i++) {
        if (read_cmos_sensor_8(0x0005) != 0xFF)
            mdelay(1);
        else
            break;
    }
    pr_debug("%s exit!\n", __func__);
}

static kal_uint32 streaming_control(kal_bool enable)
{
    pr_debug("streaming_enable(0=Sw Standby,1=streaming): %d\n",
         enable);

    if (enable) {
        //write_cmos_sensor_8(0x6214, 0x7970);
        write_cmos_sensor_8(0x0100, 0x01);
    } else {
        //write_cmos_sensor_8(0x6028, 0x4000);
        write_cmos_sensor_8(0x0100, 0x00);
        check_streamoff();
    }
    return ERROR_NONE;
}


#ifdef S5K5E9_OTP


static int check_otp(void)
{
    int awbgroupflag=0;
    int flag = 0;
    BYTE zone = 0x11;
    while(flag == 0){
        write_cmos_sensor_8(0x0A02, zone);//Select the page to write by writing to 0xD0000A02 0x01~0x0C
        write_cmos_sensor_8(0x0A00, 0x01);//Enter read mode by writing 01h to 0xD0000A00
         awbgroupflag=read_cmos_sensor_8(0x0A04);// read th awb group flag
         flag=(awbgroupflag>>6)&0x03;  // read 0x0a04  7:6 bit
         printk("[s5k5e9] Start read awbgroupflag = %x flag = %x\n",awbgroupflag,flag);
         zone++;
        if(zone == 0x14){
            printk("[s5k5e9] Start read group is error!!!!");
            return 0;
        }
    }
    return 1;

}

static bool read_otp(void)
{

    rg_ratio=((read_cmos_sensor_8(0x0A11)<<2)|(read_cmos_sensor_8(0x0A13)>>6));
    bg_ratio=((read_cmos_sensor_8(0x0A12)<<2)|((read_cmos_sensor_8(0x0A13)>>4)&0x3));
    golden_rg_ratio=((read_cmos_sensor_8(0x0A14)<<2)|(read_cmos_sensor_8(0x0A16)>>6));
    golden_bg_ratio=((read_cmos_sensor_8(0x0A15)<<2)|((read_cmos_sensor_8(0x0A16)>>4)&0x3));

    printk("[s5k5e9] rg_ratio=%d bg_ratio= %d golden_rg_ratio=%d golden_bg_ratio=%d\n",rg_ratio,bg_ratio,golden_rg_ratio,golden_bg_ratio);
    write_cmos_sensor_8(0x0A00, 0x00);   //Reset the NVM interface by writing 00h to 0xD0000A00
      return 1;
}

static bool update_otp(void)
{
    int temp = 0;
    int rg = 0,bg = 0,golden_rg = 0,golden_bg = 0;
    kal_uint32 r_ratio = 0;
    kal_uint32 b_ratio = 0;

    printk("[s5k5e9] otp wb R_GAIN = %d, B_GAIN = %d, G_GAIN = %d\n",R_GAIN,B_GAIN,G_GAIN);
    if((R_GAIN==0)&&(B_GAIN==0)&&(G_GAIN==0))
    {
    write_cmos_sensor_8(0x0136,0x18);// write pll 24mHZ
    write_cmos_sensor_8(0x0100,0x01);     //stream on
       mdelay(50);    // delay 50 ms  add by caodeng

        temp = check_otp();
        if (temp ==0)
        {
           return 0;
        }
    printk("[s5k5e9] zhongyuqi10otp temp= %d\n",temp);
    //LOG_INF("otp i = %d\n",i);

    if(!(read_otp()))
    {
        return false;
    }
    printk("[s5k5e9] zhongyuqi11otp we get herr\n");
    // no light source information in OTP
    rg =rg_ratio;
    // no light source information in OTP
    bg =bg_ratio;
    golden_rg =golden_rg_ratio;
    golden_bg =golden_bg_ratio;

    r_ratio=((512*golden_rg)/rg);
    b_ratio=((512*golden_bg)/bg);
    printk("[s5k5e9] otp wb r_ratio = %d, b_ratio = %d\n",r_ratio,b_ratio);

    if(r_ratio >= 512 )
    {
        if(b_ratio>=512)
        {
            R_GAIN = (unsigned short)((GAIN_DEFAULT*r_ratio)/512);
            G_GAIN = GAIN_DEFAULT;
            B_GAIN = (unsigned short)((GAIN_DEFAULT*b_ratio)/512);
        }
        else
        {
            R_GAIN = (unsigned short)((GAIN_DEFAULT*r_ratio)/b_ratio);
            G_GAIN = (unsigned short)((GAIN_DEFAULT*512)/b_ratio);
            B_GAIN = GAIN_DEFAULT;
        }
    }
    else
    {
        if(b_ratio >= 512)
        {
            R_GAIN = GAIN_DEFAULT;
            G_GAIN =(unsigned short)((GAIN_DEFAULT*512)/r_ratio);
            B_GAIN =(unsigned short)((GAIN_DEFAULT*b_ratio)/r_ratio);

        }
        else
        {
            Gr_GAIN = (unsigned short)((GAIN_DEFAULT*512)/r_ratio);
            Gb_GAIN = (unsigned short)((GAIN_DEFAULT*512)/b_ratio);
            if(Gr_GAIN >= Gb_GAIN)
             {
                    R_GAIN = GAIN_DEFAULT;
                    G_GAIN = (unsigned short)((GAIN_DEFAULT*512)/r_ratio);
                    B_GAIN = (unsigned short)((GAIN_DEFAULT*b_ratio)/r_ratio);
             }
                else
                {
                     R_GAIN =  (unsigned short)((GAIN_DEFAULT*r_ratio)/b_ratio);
                     G_GAIN = (unsigned short)((GAIN_DEFAULT*512)/b_ratio);
                     B_GAIN = GAIN_DEFAULT;
            }
        }
    }

    printk("[s5k5e9]wb R_GAIN = %d, B_GAIN = %d, G_GAIN = %d\n",R_GAIN,B_GAIN,G_GAIN);
    return 1;    // add this to stop write data  when we have read out the data in power up the phone
    }

    if((R_GAIN!=0)&&(B_GAIN!=0)&&(G_GAIN!=0))
    {
    write_cmos_sensor_8(0x3c0f,0x00);

    write_cmos_sensor_8(0x0210, R_GAIN>>8);  //  change by me to let the preivew is very red!!!!!  caodeng
    write_cmos_sensor_8(0x0211, R_GAIN&0xff);
    write_cmos_sensor_8(0x0212, B_GAIN>>8);
    write_cmos_sensor_8(0x0213, B_GAIN&0xff);
    write_cmos_sensor_8(0x020E, G_GAIN>>8);
    write_cmos_sensor_8(0x020F, G_GAIN&0xff);
    write_cmos_sensor_8(0x0214, G_GAIN>>8);
    write_cmos_sensor_8(0x0215, G_GAIN&0xff);
    }

    printk("[s5k5e9] otp wb R_GAIN = %d, B_GAIN = %d, G_GAIN = %d\n",R_GAIN,B_GAIN,G_GAIN);

    return 1;
}
#endif

static void sensor_init(void)
{
    LOG_INF("E\n");
    write_cmos_sensor_8(0x0100, 0x00);  //gobal
    write_cmos_sensor_8(0x3B45, 0x01);
    write_cmos_sensor_8(0x0B05, 0x01);
    write_cmos_sensor_8(0x3931, 0x02);  //
    write_cmos_sensor_8(0x392F, 0x01);  //
    write_cmos_sensor_8(0x3930, 0x80);  //0x00
    write_cmos_sensor_8(0x3932, 0x88);  //80
    write_cmos_sensor_8(0x3924, 0x7F);
    write_cmos_sensor_8(0x3925, 0xFD);
    write_cmos_sensor_8(0x3C08, 0xFF);
    write_cmos_sensor_8(0x3C09, 0xFF);
    write_cmos_sensor_8(0x3C31, 0xFF);
    write_cmos_sensor_8(0x3C32, 0xFF);
    write_cmos_sensor_8(0x3290, 0x10);
    write_cmos_sensor_8(0x3200, 0x01);
    write_cmos_sensor_8(0x3074, 0x06);
    write_cmos_sensor_8(0x3075, 0x2F);
    write_cmos_sensor_8(0x308A, 0x20);
    write_cmos_sensor_8(0x308B, 0x08);
    write_cmos_sensor_8(0x308C, 0x0B);
    write_cmos_sensor_8(0x3081, 0x07);
    write_cmos_sensor_8(0x307B, 0x85);
    write_cmos_sensor_8(0x307A, 0x0A);
    write_cmos_sensor_8(0x3079, 0x0A);
    write_cmos_sensor_8(0x306E, 0x71);
    write_cmos_sensor_8(0x306F, 0x28);
    write_cmos_sensor_8(0x301F, 0x20);
    write_cmos_sensor_8(0x3012, 0x4E);
    write_cmos_sensor_8(0x306B, 0x9A);
    write_cmos_sensor_8(0x3091, 0x16);
    write_cmos_sensor_8(0x30C4, 0x06);
    write_cmos_sensor_8(0x306A, 0x79);
    write_cmos_sensor_8(0x30B0, 0xFF);
    write_cmos_sensor_8(0x306D, 0x08);
    write_cmos_sensor_8(0x3084, 0x16);
    write_cmos_sensor_8(0x3070, 0x0F);
    write_cmos_sensor_8(0x30C2, 0x05);
    write_cmos_sensor_8(0x3069, 0x87);
    write_cmos_sensor_8(0x3C0F, 0x00);
    write_cmos_sensor_8(0x3083, 0x14);
    write_cmos_sensor_8(0x3080, 0x08);
    write_cmos_sensor_8(0x3C34, 0xEA);
    write_cmos_sensor_8(0x3C35, 0x5C);
    write_cmos_sensor_8(0x3400, 0x01);    //lsc off00
    write_cmos_sensor_8(0x0B00, 0x00);    //lsc off01
/* code at 2022/09/09 start */
    write_cmos_sensor_8(0x3941, 0x0E);
    write_cmos_sensor_8(0x3942, 0x11);
    write_cmos_sensor_8(0x3C0A, 0x00);
/* code at 2022/09/09 end */

    //add for hw sync
    //write_cmos_sensor_8(0x30C2, 0x01);
    //write_cmos_sensor_8(0x3C05, 0x1D);
    //write_cmos_sensor_8(0x3500, 0x03);

}    /*      sensor_init  */


static void preview_setting(void)
{
    LOG_INF("s5k5e9 %s:%d E", __func__, __LINE__);
    // preview 30.01fps
    LOG_INF("E\n");
    write_cmos_sensor_8(0x0100, 0x00);
    //check_output_stream_off();
    write_cmos_sensor_8(0x0136, 0x18);
    write_cmos_sensor_8(0x0137, 0x00);
    write_cmos_sensor_8(0x0305, 0x04);
    write_cmos_sensor_8(0x0306, 0x00);
    write_cmos_sensor_8(0x0307, 0x5F);
    write_cmos_sensor_8(0x030D, 0x04);
    write_cmos_sensor_8(0x030E, 0x00);
    write_cmos_sensor_8(0x030F, 0x92);
    write_cmos_sensor_8(0x3C1F, 0x00);
    write_cmos_sensor_8(0x3C17, 0x00);
    write_cmos_sensor_8(0x0112, 0x0A);
    write_cmos_sensor_8(0x0113, 0x0A);
    write_cmos_sensor_8(0x0114, 0x01);
    write_cmos_sensor_8(0x0820, 0x03);
    write_cmos_sensor_8(0x0821, 0x6C);
    write_cmos_sensor_8(0x0822, 0x00);
    write_cmos_sensor_8(0x0823, 0x00);
    write_cmos_sensor_8(0x3929, 0x0F);
    write_cmos_sensor_8(0x0344, 0x00);
    write_cmos_sensor_8(0x0345, 0x08);
    write_cmos_sensor_8(0x0346, 0x00);
    write_cmos_sensor_8(0x0347, 0x08);
    write_cmos_sensor_8(0x0348, 0x0A);
    write_cmos_sensor_8(0x0349, 0x27);
    write_cmos_sensor_8(0x034A, 0x07);
    write_cmos_sensor_8(0x034B, 0x9f);
    write_cmos_sensor_8(0x034C, 0x0A);
    write_cmos_sensor_8(0x034D, 0x20);
    write_cmos_sensor_8(0x034E, 0x07);
    write_cmos_sensor_8(0x034F, 0x98);
    write_cmos_sensor_8(0x0900, 0x00);
    write_cmos_sensor_8(0x0901, 0x00);
    write_cmos_sensor_8(0x0381, 0x01);
    write_cmos_sensor_8(0x0383, 0x01);
    write_cmos_sensor_8(0x0385, 0x01);
    write_cmos_sensor_8(0x0387, 0x01);
    write_cmos_sensor_8(0x0101, 0x00);
    write_cmos_sensor_8(0x0340, 0x07);
    write_cmos_sensor_8(0x0341, 0xF4);
    write_cmos_sensor_8(0x0342, 0x0C);
    write_cmos_sensor_8(0x0343, 0x28);
    write_cmos_sensor_8(0x0200, 0x0B);
    write_cmos_sensor_8(0x0201, 0x9C);
    write_cmos_sensor_8(0x0202, 0x00);
    write_cmos_sensor_8(0x0203, 0x02);
    write_cmos_sensor_8(0x30B8, 0x2E);
    write_cmos_sensor_8(0x30BA, 0x36);
    LOG_INF("s5k5e9 %s:%d X", __func__, __LINE__);

#ifdef S5K5E9_OTP
    if(update_otp())
        printk("5e9 update_otp !!!!!!!!!!!!\n ");
#endif

}    /*    preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
    write_cmos_sensor_8(0x0100, 0x00);
    //check_output_stream_off();
    write_cmos_sensor_8(0x0136, 0x18);
    write_cmos_sensor_8(0x0137, 0x00);
    write_cmos_sensor_8(0x0305, 0x04);
    write_cmos_sensor_8(0x0306, 0x00);
    write_cmos_sensor_8(0x0307, 0x5F);
    write_cmos_sensor_8(0x030D, 0x04);
    write_cmos_sensor_8(0x030E, 0x00);
    write_cmos_sensor_8(0x030F, 0x92);
    write_cmos_sensor_8(0x3C1F, 0x00);
    write_cmos_sensor_8(0x3C17, 0x00);
    write_cmos_sensor_8(0x0112, 0x0A);
    write_cmos_sensor_8(0x0113, 0x0A);
    write_cmos_sensor_8(0x0114, 0x01);
    write_cmos_sensor_8(0x0820, 0x03);
    write_cmos_sensor_8(0x0821, 0x6C);
    write_cmos_sensor_8(0x0822, 0x00);
    write_cmos_sensor_8(0x0823, 0x00);
    write_cmos_sensor_8(0x3929, 0x0F);
    write_cmos_sensor_8(0x0344, 0x00);
    write_cmos_sensor_8(0x0345, 0x08);
    write_cmos_sensor_8(0x0346, 0x00);
    write_cmos_sensor_8(0x0347, 0x08);
    write_cmos_sensor_8(0x0348, 0x0A);
    write_cmos_sensor_8(0x0349, 0x27);
    write_cmos_sensor_8(0x034A, 0x07);
    write_cmos_sensor_8(0x034B, 0x9f);
    write_cmos_sensor_8(0x034C, 0x0A);
    write_cmos_sensor_8(0x034D, 0x20);
    write_cmos_sensor_8(0x034E, 0x07);
    write_cmos_sensor_8(0x034F, 0x98);
    write_cmos_sensor_8(0x0900, 0x00);
    write_cmos_sensor_8(0x0901, 0x00);
    write_cmos_sensor_8(0x0381, 0x01);
    write_cmos_sensor_8(0x0383, 0x01);
    write_cmos_sensor_8(0x0385, 0x01);
    write_cmos_sensor_8(0x0387, 0x01);
    write_cmos_sensor_8(0x0101, 0x00);
    write_cmos_sensor_8(0x0340, 0x07);
    write_cmos_sensor_8(0x0341, 0xF4);
    write_cmos_sensor_8(0x0342, 0x0C);
    write_cmos_sensor_8(0x0343, 0x28);
    write_cmos_sensor_8(0x0200, 0x0B);
    write_cmos_sensor_8(0x0201, 0x9C);
    write_cmos_sensor_8(0x0202, 0x00);
    write_cmos_sensor_8(0x0203, 0x02);
    write_cmos_sensor_8(0x30B8, 0x2E);
    write_cmos_sensor_8(0x30BA, 0x36);

#ifdef S5K5E9_OTP
    if(update_otp())
        printk("5e9 update_otp !!!!!!!!!!!!\n ");
#endif
}

/* code at 2022/07/26 start */
static void normal_video_setting(kal_uint16 currefps)
{
    LOG_INF("E! currefps:%d\n",currefps);

    write_cmos_sensor_8(0x0100, 0x00);
    //check_output_stream_off();
    write_cmos_sensor_8(0x0136, 0x18);
    write_cmos_sensor_8(0x0137, 0x00);
    write_cmos_sensor_8(0x0305, 0x04);
    write_cmos_sensor_8(0x0306, 0x00);
    write_cmos_sensor_8(0x0307, 0x5F);
    write_cmos_sensor_8(0x030D, 0x04);
    write_cmos_sensor_8(0x030E, 0x00);
    write_cmos_sensor_8(0x030F, 0x92);
    write_cmos_sensor_8(0x3C1F, 0x00);
    write_cmos_sensor_8(0x3C17, 0x00);
    write_cmos_sensor_8(0x0112, 0x0A);
    write_cmos_sensor_8(0x0113, 0x0A);
    write_cmos_sensor_8(0x0114, 0x01);
    write_cmos_sensor_8(0x0820, 0x03);
    write_cmos_sensor_8(0x0821, 0x6C);
    write_cmos_sensor_8(0x0822, 0x00);
    write_cmos_sensor_8(0x0823, 0x00);
    write_cmos_sensor_8(0x3929, 0x0F);
    write_cmos_sensor_8(0x0344, 0x00);
    write_cmos_sensor_8(0x0345, 0x08);
    write_cmos_sensor_8(0x0346, 0x00);
    write_cmos_sensor_8(0x0347, 0x08);
    write_cmos_sensor_8(0x0348, 0x0A);
    write_cmos_sensor_8(0x0349, 0x27);
    write_cmos_sensor_8(0x034A, 0x07);
    write_cmos_sensor_8(0x034B, 0x9f);
    write_cmos_sensor_8(0x034C, 0x0A);
    write_cmos_sensor_8(0x034D, 0x20);
    write_cmos_sensor_8(0x034E, 0x07);
    write_cmos_sensor_8(0x034F, 0x98);
    write_cmos_sensor_8(0x0900, 0x00);
    write_cmos_sensor_8(0x0901, 0x00);
    write_cmos_sensor_8(0x0381, 0x01);
    write_cmos_sensor_8(0x0383, 0x01);
    write_cmos_sensor_8(0x0385, 0x01);
    write_cmos_sensor_8(0x0387, 0x01);
    write_cmos_sensor_8(0x0101, 0x00);
    write_cmos_sensor_8(0x0340, 0x07);
    write_cmos_sensor_8(0x0341, 0xF4);
    write_cmos_sensor_8(0x0342, 0x0C);
    write_cmos_sensor_8(0x0343, 0x28);
    write_cmos_sensor_8(0x0200, 0x0B);
    write_cmos_sensor_8(0x0201, 0x9C);
    write_cmos_sensor_8(0x0202, 0x00);
    write_cmos_sensor_8(0x0203, 0x02);
    write_cmos_sensor_8(0x30B8, 0x2E);
    write_cmos_sensor_8(0x30BA, 0x36);
/* code at 2022/07/26 end */
#ifdef S5K5E9_OTP
    if(update_otp())
        printk("5e9 update_otp !!!!!!!!!!!!\n ");
#endif
}
static void hs_video_setting(void)
{
    LOG_INF("E! VGA 120fps\n");
    write_cmos_sensor_8(0x0100, 0x00);
    write_cmos_sensor_8(0x0136, 0x18);
    write_cmos_sensor_8(0x0137, 0x00);
    write_cmos_sensor_8(0x0305, 0x04);
    write_cmos_sensor_8(0x0306, 0x00);
    write_cmos_sensor_8(0x0307, 0x5F);
    write_cmos_sensor_8(0x030D, 0x04);
    write_cmos_sensor_8(0x030E, 0x00);
    write_cmos_sensor_8(0x030F, 0x96);
    write_cmos_sensor_8(0x3C1F, 0x00);
    write_cmos_sensor_8(0x3C17, 0x00);
    write_cmos_sensor_8(0x0112, 0x0A);
    write_cmos_sensor_8(0x0113, 0x0A);
    write_cmos_sensor_8(0x0114, 0x01);
    write_cmos_sensor_8(0x0820, 0x03);
    write_cmos_sensor_8(0x0821, 0x84);
    write_cmos_sensor_8(0x0822, 0x00);
    write_cmos_sensor_8(0x0823, 0x00);
    write_cmos_sensor_8(0x3929, 0x0F);
    write_cmos_sensor_8(0x3941, 0x0E);
    write_cmos_sensor_8(0x3942, 0x11);
    write_cmos_sensor_8(0x3C0A, 0x00);
    write_cmos_sensor_8(0x0344, 0x00);
    write_cmos_sensor_8(0x0345, 0x18);
    write_cmos_sensor_8(0x0346, 0x01);
    write_cmos_sensor_8(0x0347, 0x04);
    write_cmos_sensor_8(0x0348, 0x0A);
    write_cmos_sensor_8(0x0349, 0x17);
    write_cmos_sensor_8(0x034A, 0x06);
    write_cmos_sensor_8(0x034B, 0xA3);
    write_cmos_sensor_8(0x034C, 0x05);
    write_cmos_sensor_8(0x034D, 0x00);
    write_cmos_sensor_8(0x034E, 0x02);
    write_cmos_sensor_8(0x034F, 0xD0);
    write_cmos_sensor_8(0x0900, 0x01);
    write_cmos_sensor_8(0x0901, 0x22);
    write_cmos_sensor_8(0x0381, 0x01);
    write_cmos_sensor_8(0x0383, 0x01);
    write_cmos_sensor_8(0x0385, 0x01);
    write_cmos_sensor_8(0x0387, 0x03);
    write_cmos_sensor_8(0x0101, 0x00);
    write_cmos_sensor_8(0x0340, 0x07);
    write_cmos_sensor_8(0x0341, 0xF2);
    write_cmos_sensor_8(0x0342, 0x0C);
    write_cmos_sensor_8(0x0343, 0x28);
    write_cmos_sensor_8(0x0200, 0x0B);
    write_cmos_sensor_8(0x0201, 0x9C);
    write_cmos_sensor_8(0x0202, 0x00);
    write_cmos_sensor_8(0x0203, 0x02);
    write_cmos_sensor_8(0x30B8, 0x2A);
    write_cmos_sensor_8(0x30BA, 0x2E);

#ifdef S5K5E9_OTP
    if(update_otp())
        printk("5e9 update_otp !!!!!!!!!!!!\n ");
#endif

}

static void slim_video_setting(void)
{
    LOG_INF("E! HD 30fps\n");
    write_cmos_sensor_8(0x0100, 0x00);
    //check_output_stream_off();
    write_cmos_sensor_8(0x0136, 0x18);
    write_cmos_sensor_8(0x0137, 0x00);
    write_cmos_sensor_8(0x0305, 0x04);
    write_cmos_sensor_8(0x0306, 0x00);
    write_cmos_sensor_8(0x0307, 0x5F);
    write_cmos_sensor_8(0x030D, 0x04);
    write_cmos_sensor_8(0x030E, 0x00);
    write_cmos_sensor_8(0x030F, 0x92);
    write_cmos_sensor_8(0x3C1F, 0x00);
    write_cmos_sensor_8(0x3C17, 0x00);
    write_cmos_sensor_8(0x0112, 0x0A);
    write_cmos_sensor_8(0x0113, 0x0A);
    write_cmos_sensor_8(0x0114, 0x01);
    write_cmos_sensor_8(0x0820, 0x03);
    write_cmos_sensor_8(0x0821, 0x6C);
    write_cmos_sensor_8(0x0822, 0x00);
    write_cmos_sensor_8(0x0823, 0x00);
    write_cmos_sensor_8(0x3929, 0x0F);
    write_cmos_sensor_8(0x0344, 0x00);
    write_cmos_sensor_8(0x0345, 0x18);
    write_cmos_sensor_8(0x0346, 0x01);
    write_cmos_sensor_8(0x0347, 0x04);
    write_cmos_sensor_8(0x0348, 0x0A);
    write_cmos_sensor_8(0x0349, 0x17);
    write_cmos_sensor_8(0x034A, 0x06);
    write_cmos_sensor_8(0x034B, 0xA3);
    write_cmos_sensor_8(0x034C, 0x05);
    write_cmos_sensor_8(0x034D, 0x00);
    write_cmos_sensor_8(0x034E, 0x02);
    write_cmos_sensor_8(0x034F, 0xD0);
    write_cmos_sensor_8(0x0900, 0x01);
    write_cmos_sensor_8(0x0901, 0x22);
    write_cmos_sensor_8(0x0381, 0x01);
    write_cmos_sensor_8(0x0383, 0x01);
    write_cmos_sensor_8(0x0385, 0x01);
    write_cmos_sensor_8(0x0387, 0x03);
    write_cmos_sensor_8(0x0101, 0x00);
    write_cmos_sensor_8(0x0340, 0x07);
    write_cmos_sensor_8(0x0341, 0xF2);
    write_cmos_sensor_8(0x0342, 0x0C);
    write_cmos_sensor_8(0x0343, 0x28);
    write_cmos_sensor_8(0x0200, 0x0B);
    write_cmos_sensor_8(0x0201, 0x9C);
    write_cmos_sensor_8(0x0202, 0x00);
    write_cmos_sensor_8(0x0203, 0x02);
    write_cmos_sensor_8(0x30B8, 0x2A);
    write_cmos_sensor_8(0x30BA, 0x2E);

#ifdef S5K5E9_OTP
        if(update_otp())
    printk("5e9 update_otp !!!!!!!!!!!!\n ");
#endif
}

/*
static kal_uint16 read_cmos_sensor_s5k5e9(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
	iReadRegI2C(pu_send_cmd, 2, (u8 *)&get_byte, 1, 0xA2);
	return get_byte;
}

static void s5k5e9_fusion_id_read(void)
{
	int i;
	for (i=0; i<43; i++) {
		fusion_id_front[i] = read_cmos_sensor_s5k5e9(0x10+i);
		//pr_err("%s %d wpc fusion_id_front[%d]=0x%2x\n",__func__, __LINE__, i, fusion_id_front[i]);
	}
}

static void s5k5e9_sn_read(void)
{
	int i;
	for (i=0; i<14; i++) {
		sn_front[i] = read_cmos_sensor_s5k5e9(0x1F92+i);
		//pr_err("%s %d wpc fusion_id_front[%d]=0x%2x\n",__func__, __LINE__, i, sn_front[i]);
	}
}

static UINT8 get_sensor_module_id(void)
{
    int i;
    u8 calmoduleversion[16] = {0};
    for (i=0; i<15; i++) {
        calmoduleversion[i] = read_cmos_sensor_s5k5e9(0x01+i);
        //pr_err("%s %d s5k5e9_iii module_id[%d]=0x%2x\n",__func__, __LINE__, i, calmoduleversion[i]);
    }
    pr_err("======================module version==================\n");
    pr_err("[vendor id] = 0x%x\n", calmoduleversion[0]);
    pr_err("[yy/mm/dd] = %d/%d/%d \n", calmoduleversion[1], calmoduleversion[2], calmoduleversion[3]);
    pr_err("[hh/mm/sec] = %d:%d:%d \n", calmoduleversion[4], calmoduleversion[5], calmoduleversion[6]);
    pr_err("[lens_id/vcmid/drive id] = 0x%x/0x%x/0x%x \n", calmoduleversion[7], calmoduleversion[8], calmoduleversion[9]);
    pr_err("======================module version==================\n");

    return calmoduleversion[0];
}
*/

/*************************************************************************
 * FUNCTION
 *    get_imgsensor_id
 *
 * DESCRIPTION
 *    This function get the sensor ID
 *
 * PARAMETERS
 *    *sensorID : return the sensor ID
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/

static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    //UINT8 module_id;

    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = return_sensor_id();
            if (*sensor_id == imgsensor_info.sensor_id) {
                printk(" debug i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, *sensor_id);
				// s5k5e9_sn_read();
				// s5k5e9_fusion_id_read();
                // module_id = get_sensor_module_id();
                // if (module_id != 0x6) {
                //         pr_err("Read qtech module id failed,module id = 0x%x\n", module_id);
                //         *sensor_id = 0xFFFFFFFF;
                // }

                return ERROR_NONE;
            }
            LOG_INF(" debug Read sensor id fail, id: 0x%x, sensor id: 0x%x\n",
                imgsensor.i2c_write_id, *sensor_id);

            retry--;
        } while(retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        /* if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF */
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}
/*************************************************************************
 * FUNCTION
 *    open
 *
 * DESCRIPTION
 *    This function initialize the registers of CMOS sensor
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 open(void)
{
    /* const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2}; */
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint32 sensor_id = 0;                
    // UINT8 module_id;
                                             
    LOG_1;                                   
    LOG_2;                                   
    if (first_flag == 1) {
        LOG_INF("modify device I2C address to 0x5a\n");
        write_cmos_sensor_8(0x0107, 0x5a);
        first_flag = 0;
    }
    //sensor have two i2c address 0x5a  we should detect the module used i2c address
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_INF(" debug i2c write id: 0x%x, sensor id: 0x%x\n",
                    imgsensor.i2c_write_id, sensor_id);

                // module_id = get_sensor_module_id();
                // if (module_id != 0x6) {
                //         pr_err("Read qtech module id failed,module id = 0x%x\n", module_id);
                //         //sensor_id = 0xFFFFFFFF;
                // }

                break;
            }
            LOG_INF("Read sensor id fail, id: 0x%x, sensor id: 0x%x\n",
                imgsensor.i2c_write_id, sensor_id);

            retry--;
        } while (retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;

    /* initail sequence write in  */
    sensor_init();
#ifdef S5K5E9_OTP
        if(update_otp())
    printk("5e9 update_otp !!!!!!!!!!!!\n ");
#endif

    spin_lock(&imgsensor_drv_lock);
    imgsensor.autoflicker_en= KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.shutter = 0x3D0;
    imgsensor.gain = 0x100;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_en = 0;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}    /*    open  */



/*************************************************************************
 * FUNCTION
 *    close
 *
 * DESCRIPTION
 *
 *
 * PARAMETERS
 *    None
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 close(void)
{
    LOG_INF("E\n");

    /*No Need to implement this function*/
    streaming_control(0);
    return ERROR_NONE;
}    /*    close  */


/*************************************************************************
 * FUNCTION
 * preview
 *
 * DESCRIPTION
 *    This function start the sensor preview.
 *
 * PARAMETERS
 *    *image_window : address pointer of pixel numbers in one period of HSYNC
 *  *sensor_config_data : address pointer of line numbers in one period of VSYNC
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
        MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_PREVIEW;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    set_mirror_flip(imgsensor.mirror);
    return ERROR_NONE;
}    /*    preview   */

/*************************************************************************
 * FUNCTION
 *    capture
 *
 * DESCRIPTION
 *    This function setup the CMOS sensor in capture MY_OUTPUT mode
 *
 * PARAMETERS
 *
 * RETURNS
 *    None
 *
 * GLOBALS AFFECTED
 *
 *************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    /* imgsensor.current_fps = 240; */
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
    if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) {
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    } else if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {    /* PIP capture:15fps */
        imgsensor.pclk = imgsensor_info.cap1.pclk;
        imgsensor.line_length = imgsensor_info.cap1.linelength;
        imgsensor.frame_length = imgsensor_info.cap1.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    } else {        /* PIP capture: 24fps */
        imgsensor.pclk = imgsensor_info.cap2.pclk;
        imgsensor.line_length = imgsensor_info.cap2.linelength;
        imgsensor.frame_length = imgsensor_info.cap2.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap2.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);

    capture_setting(imgsensor.current_fps);
    set_mirror_flip(imgsensor.mirror);


    return ERROR_NONE;
}                /* capture() */

static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                   MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_VIDEO;
    imgsensor.pclk = imgsensor_info.normal_video.pclk;
    imgsensor.line_length = imgsensor_info.normal_video.linelength;
    imgsensor.frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.normal_video.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    normal_video_setting(imgsensor.current_fps);
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}                /*      normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
               MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{

    LOG_INF("E\n");


    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    /* imgsensor.video_mode = KAL_TRUE; */
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}    /*    hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                 MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
    imgsensor.pclk = imgsensor_info.slim_video.pclk;
    imgsensor.line_length = imgsensor_info.slim_video.linelength;
    imgsensor.frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    slim_video_setting();
    set_mirror_flip(imgsensor.mirror);

    return ERROR_NONE;
}                /*      slim_video       */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    LOG_INF("E\n");
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


    sensor_resolution->SensorHighSpeedVideoWidth = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight = imgsensor_info.slim_video.grabwindow_height;
    return ERROR_NONE;
}                /*      get_resolution  */

static kal_uint32 get_info(enum MSDK_SCENARIO_ID_ENUM scenario_id,
               MSDK_SENSOR_INFO_STRUCT *sensor_info,
               MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);


    /* sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; // not use */
    /* sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; // not use */
    /* imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; // not use */

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW;    /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;    /* inverse with datasheet */
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 4;    /* not use */
    sensor_info->SensorResetActiveHigh = FALSE;    /* not use */
    sensor_info->SensorResetDelayCount = 5;    /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0;    /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    /* The frame of setting shutter default 0 for TG int */
    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;
    /* The frame of setting sensor gain */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 3; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */

    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
    sensor_info->SensorHightSampling = 0;    // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

        sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

        break;
    default:
        sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
        sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

        sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount =
            imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
        break;
    }

    return ERROR_NONE;
}                /*      get_info  */


static kal_uint32 control(enum MSDK_SCENARIO_ID_ENUM scenario_id,
              MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
              MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.current_scenario_id = scenario_id;
    spin_unlock(&imgsensor_drv_lock);
    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        preview(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        capture(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        normal_video(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        hs_video(image_window, sensor_config_data);
        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        slim_video(image_window, sensor_config_data);
        break;
    default:
        LOG_INF("Error ScenarioId setting");
        preview(image_window, sensor_config_data);
        return ERROR_INVALID_SCENARIO_ID;
    }

    sensor_set_sync_mode();

    return ERROR_NONE;
}                /* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
    LOG_INF("framerate = %d\n ", framerate);
    /* SetVideoMode Function should fix framerate */
    if (framerate == 0)
        /* Dynamic frame rate */
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 146;
    else
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps, 1);

    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d\n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable)        /* enable auto flicker */
        imgsensor.autoflicker_en = KAL_TRUE;
    else            /* Cancel Auto flick */
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                        MUINT32 framerate)
{
    kal_uint32 frame_length;

    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
            ? (frame_length - imgsensor_info.pre.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        /* set_dummy(); */
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        if (framerate == 0)
            return ERROR_NONE;
        frame_length = imgsensor_info.normal_video.pclk / framerate * 10 /
            imgsensor_info.normal_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength)
            ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        /* set_dummy(); */
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
            frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength)
                ? (frame_length - imgsensor_info.cap1.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
        } else if (imgsensor.current_fps == imgsensor_info.cap2.max_framerate) {
            frame_length = imgsensor_info.cap2.pclk / framerate * 10 / imgsensor_info.cap2.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.cap2.framelength)
                ? (frame_length - imgsensor_info.cap2.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.cap2.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
        } else {
            if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",
                    framerate, imgsensor_info.cap.max_framerate / 10);
            frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength)
                ? (frame_length - imgsensor_info.cap.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
        }
        /* set_dummy(); */
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength)
            ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        /* set_dummy(); */
        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength)
            ? (frame_length - imgsensor_info.slim_video.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        /* set_dummy(); */
        break;
    default:        /* coding with  preview scenario by default */
        frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
        spin_lock(&imgsensor_drv_lock);
        imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength)
            ? (frame_length - imgsensor_info.pre.framelength) : 0;
        imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
        imgsensor.min_frame_length = imgsensor.frame_length;
        spin_unlock(&imgsensor_drv_lock);
        /* set_dummy(); */
        LOG_INF("error scenario_id = %d, we use preview scenario\n", scenario_id);
        break;
    }
    return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(enum MSDK_SCENARIO_ID_ENUM scenario_id,
                            MUINT32 *framerate)
{
    LOG_INF("scenario_id = %d\n", scenario_id);
    if (framerate == NULL)
        return ERROR_NONE;

    switch (scenario_id) {
    case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
        *framerate = imgsensor_info.pre.max_framerate;
        break;
    case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
        *framerate = imgsensor_info.normal_video.max_framerate;
        break;
    case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
        *framerate = imgsensor_info.cap.max_framerate;
        break;
    case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
        *framerate = imgsensor_info.hs_video.max_framerate;
        break;
    case MSDK_SCENARIO_ID_SLIM_VIDEO:
        *framerate = imgsensor_info.slim_video.max_framerate;
        break;
    default:
        break;
    }

    return ERROR_NONE;
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);

    if (enable) {
        /* code at 2022/08/31 start */
        /* 0x0601[2:0]; 0=no pattern,1=solid colour,2 = 100% colour bar ,3 = Fade to gray' colour bar */
        write_cmos_sensor_8(0x0602, 0x00);
        write_cmos_sensor_8(0x0603, 0x00);
        write_cmos_sensor_8(0x0604, 0x00);
        write_cmos_sensor_8(0x0605, 0x00);
        write_cmos_sensor_8(0x0606, 0x00);
        write_cmos_sensor_8(0x0607, 0x00);
        write_cmos_sensor_8(0x0608, 0x00);
        write_cmos_sensor_8(0x0609, 0x00);
        write_cmos_sensor_8(0x0601, 0x01);
    } else {
        write_cmos_sensor_8(0x0602, 0x02);
        write_cmos_sensor_8(0x0603, 0x00);
        write_cmos_sensor_8(0x0604, 0x02);
        write_cmos_sensor_8(0x0605, 0x00);
        write_cmos_sensor_8(0x0606, 0x02);
        write_cmos_sensor_8(0x0607, 0x00);
        write_cmos_sensor_8(0x0608, 0x02);
        write_cmos_sensor_8(0x0609, 0x00);
        write_cmos_sensor_8(0x0601, 0x00);
        /* code at 2022/08/31 end */
    }
    write_cmos_sensor_8(0x3200, 0x00);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                  UINT8 *feature_para, UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16 = (UINT16 *) feature_para;
    UINT16 *feature_data_16 = (UINT16 *) feature_para;
    UINT32 *feature_return_para_32 = (UINT32 *) feature_para;
    UINT32 *feature_data_32 = (UINT32 *) feature_para;
    unsigned long long *feature_data = (unsigned long long *)feature_para;
    /* unsigned long long *feature_return_para=(unsigned long long *) feature_para; */

    struct SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data = (MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    LOG_INF("feature_id = %d", feature_id);
    switch (feature_id) {
/* code at 2022/08/29 start */
	case SENSOR_FEATURE_GET_GAIN_RANGE_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_gain;
		*(feature_data + 2) = imgsensor_info.max_gain;
		break;
	case SENSOR_FEATURE_GET_BASE_GAIN_ISO_AND_STEP:
		*(feature_data + 0) = imgsensor_info.min_gain_iso;
		*(feature_data + 1) = imgsensor_info.gain_step;
		*(feature_data + 2) = imgsensor_info.gain_type;
		break;
	case SENSOR_FEATURE_GET_MIN_SHUTTER_BY_SCENARIO:
		*(feature_data + 1) = imgsensor_info.min_shutter;
		*(feature_data + 2) = imgsensor_info.exp_step;
		break;
/* code at 2022/08/29 end */
    case SENSOR_FEATURE_GET_PERIOD:
        *feature_return_para_16++ = imgsensor.line_length;
        *feature_return_para_16 = imgsensor.frame_length;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
        //    LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n", imgsensor.pclk,imgsensor.current_fps);
        *feature_return_para_32 = imgsensor.pclk;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_ESHUTTER:
		/*code at 2022/10/02 start */
        set_shutter((UINT16) *feature_data);
        break;
    case SENSOR_FEATURE_SET_NIGHTMODE:
        break;
    case SENSOR_FEATURE_SET_GAIN:
            set_gain((UINT16) *feature_data);
        break;
    case SENSOR_FEATURE_SET_FLASHLIGHT:
        break;
    case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
        break;
    case SENSOR_FEATURE_SET_REGISTER:
           // if((sensor_reg_data->RegData>>8)>0)
                write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
          //  else
            //    write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
        break;
    case SENSOR_FEATURE_GET_REGISTER:
        sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
        break;
    case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
        /* get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE */
        /* if EEPROM does not exist in camera module. */
        *feature_return_para_32 = LENS_DRIVER_ID_DO_NOT_CARE;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode((UINT16) *feature_data_16);
        break;
    case SENSOR_FEATURE_CHECK_SENSOR_ID:
        get_imgsensor_id(feature_return_para_32);
        break;
    case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            set_auto_flicker_mode((BOOL)*feature_data_16, (UINT16) *(feature_data_16+1));
            break;
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, (UINT32) *(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((enum MSDK_SCENARIO_ID_ENUM)*feature_data, (MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((BOOL)*feature_data);
        break;
    case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE:    /* for factory mode auto testing */
        *feature_return_para_32 = imgsensor_info.checksum_value;
        *feature_para_len = 4;
        break;
    case SENSOR_FEATURE_SET_FRAMERATE:
        LOG_INF("current fps :%d\n", (UINT16)*feature_data_32);
        spin_lock(&imgsensor_drv_lock);
        imgsensor.current_fps = (UINT16) *feature_data_32;
        spin_unlock(&imgsensor_drv_lock);
        break;
    case SENSOR_FEATURE_SET_HDR:
        /* LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_16); */
        LOG_INF("Warning! Not Support IHDR Feature");
        spin_lock(&imgsensor_drv_lock);
            imgsensor.ihdr_en = (UINT8) *feature_data_32;
            spin_unlock(&imgsensor_drv_lock);
            break;
    case SENSOR_FEATURE_GET_CROP_INFO:
        LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d \n", (UINT32) *feature_data_32);
            wininfo = (struct SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

        switch (*feature_data_32) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(struct SENSOR_WINSIZE_INFO_STRUCT));
            break;
            }
            break;
    case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
           // LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",*feature_data,*(feature_data+1),*(feature_data+2));
            ihdr_write_shutter_gain((UINT16)(*feature_data), (UINT16)(*(feature_data + 1)), (BOOL) (*(feature_data + 2)));
            break;
	case SENSOR_FEATURE_SET_STREAMING_SUSPEND:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_SUSPEND\n");
		streaming_control(KAL_FALSE);
		break;
	case SENSOR_FEATURE_SET_STREAMING_RESUME:
		LOG_INF("SENSOR_FEATURE_SET_STREAMING_RESUME, shutter:%d\n",
			(UINT16)(*feature_data));
		if (*feature_data != 0)
			set_shutter((UINT16)(*feature_data));
		/*code at 2022/10/02 end */
		streaming_control(KAL_TRUE);
		break;
	case SENSOR_FEATURE_GET_PIXEL_RATE:
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.cap.pclk /
			(imgsensor_info.cap.linelength - 80))*
			imgsensor_info.cap.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.normal_video.pclk /
			(imgsensor_info.normal_video.linelength - 80))*
			imgsensor_info.normal_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.hs_video.pclk /
			(imgsensor_info.hs_video.linelength - 80))*
			imgsensor_info.hs_video.grabwindow_width;

			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.slim_video.pclk /
			(imgsensor_info.slim_video.linelength - 80))*
			imgsensor_info.slim_video.grabwindow_width;

			break;

		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
			(imgsensor_info.pre.pclk /
			(imgsensor_info.pre.linelength - 80))*
			imgsensor_info.pre.grabwindow_width;
			break;
		}
		break;
	case SENSOR_FEATURE_GET_MIPI_PIXEL_RATE:
	{
		switch (*feature_data) {
		case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.cap.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.normal_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.hs_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_SLIM_VIDEO:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1)) =
				imgsensor_info.slim_video.mipi_pixel_rate;
			break;
		case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
		default:
			*(MUINT32 *)(uintptr_t)(*(feature_data + 1))
				= imgsensor_info.pre.mipi_pixel_rate;
			break;
		}
	}
	break;
        default:
            break;
    }

    return ERROR_NONE;
}    /*    feature_control()  */

static struct SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 S5K5E9_SUNNY_FRONT_MIPI_RAW_SensorInit(struct SENSOR_FUNCTION_STRUCT **pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&sensor_func;
    return ERROR_NONE;
}    /*    S5K5E9_MIPI_RAW_SensorInit    */
