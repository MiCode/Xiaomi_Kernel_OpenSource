/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX135mipi_Sensor.c
 *
 * Project:
 * --------
 *	 ALPS
 *
 * Description:
 * ------------
 *	 Source code of Sensor driver
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
#include <asm/atomic.h>
//#include <asm/system.h>
#include <linux/xlog.h>

#include "kd_camera_hw.h"
#include "kd_imgsensor.h"
#include "kd_imgsensor_define.h"
#include "kd_imgsensor_errcode.h"

#include "imx135mipiraw_Sensor.h"



/****************************Modify following Strings for debug****************************/
#define PFX "IMX135_camera_sensor"

#define LOG_1 LOG_INF("IMX135,MIPI 4LANE\n")
#define LOG_2 LOG_INF("preview 2096*1554@30fps,864Mbps/lane; video 4196*3108@30fps,864Mbps/lane; capture 13M@30fps,864Mbps/lane\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)    xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define MIPI_SETTLEDELAY_AUTO     0
#define MIPI_SETTLEDELAY_MANNUAL  1
#define IHDR_USED
static imgsensor_info_struct imgsensor_info = {
    .sensor_id = IMX135_SENSOR_ID,

    //.checksum_value = 0x215125a0,
    .checksum_value = 0x4ff3b7e6,

    .pre = {
        .pclk = 231270000,              //record different mode's pclk
        .linelength = 4572,             //record different mode's linelength
        .framelength = 1640,            //record different mode's framelength
        .startx = 2,                    //record different mode's startx of grabwindow
        .starty = 2,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2088,//2096,        //record different mode's width of grabwindow
        .grabwindow_height = 1544,//1552,       //record different mode's height of grabwindow
        /*   following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
        .mipi_data_lp2hs_settle_dc = 30,
        /*   following for GetDefaultFramerateByScenario()  */
        .max_framerate = 300,
    },
    .cap = {   // 30  fps  capture
        .pclk = 432000000,
        .linelength = 4572,
        .framelength = 3146,
        .startx = 4,
        .starty = 4,
        .grabwindow_width = 4176,//4192,
        .grabwindow_height = 3088,//3104,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 300,
    },
    .cap1 = {    // 24 fps  capture
        .pclk = 348000000,
        .linelength = 4572,
        .framelength = 3146,
        .startx = 4,
        .starty = 4,
        .grabwindow_width = 4176,//4192,
        .grabwindow_height = 3088,//3104,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 240,
    },
    #ifdef IHDR_USED
    .normal_video = {
        .pclk = 432000000,
        .linelength = 4572,
        .framelength = 3144,
        .startx = 2,
        .starty = 2,
        .grabwindow_width = 4192,//4192,
        .grabwindow_height = 3104,//3104,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 300,
    },
    #else
        .normal_video = {
        .pclk = 432000000,
        .linelength = 4572,
        .framelength = 3144,
        .startx = 2,
        .starty = 2,
        .grabwindow_width = 4176,//4192,
        .grabwindow_height = 3088,//3104,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 300,
    },
    #endif
    .hs_video = {     // 120 fps
        .pclk = 432000000,
        .linelength = 4572,
        .framelength = 786,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1280,
        .grabwindow_height = 720,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 1200,
    },
    .slim_video = {
        .pclk = 184000000,//231270000,
        .linelength = 4572,
        .framelength = 1312,//1640,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1280,
        .grabwindow_height = 720,
        .mipi_data_lp2hs_settle_dc = 30,
        .max_framerate = 300,
    },
    .custom1 = {
        .pclk = 231270000,              //record different mode's pclk
        .linelength = 4572,             //record different mode's linelength
        .framelength = 1640,            //record different mode's framelength
        .startx = 2,                    //record different mode's startx of grabwindow
        .starty = 2,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2088,//2096,        //record different mode's width of grabwindow
        .grabwindow_height = 1544,//1552,       //record different mode's height of grabwindow
        /*   following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
        .mipi_data_lp2hs_settle_dc = 30,
        /*   following for GetDefaultFramerateByScenario()  */
        .max_framerate = 300,

    },
    .custom2 = {
        .pclk = 231270000,              //record different mode's pclk
        .linelength = 4572,             //record different mode's linelength
        .framelength = 1640,            //record different mode's framelength
        .startx = 2,                    //record different mode's startx of grabwindow
        .starty = 2,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2088,//2096,        //record different mode's width of grabwindow
        .grabwindow_height = 1544,//1552,       //record different mode's height of grabwindow
        /*   following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
        .mipi_data_lp2hs_settle_dc = 30,
        /*   following for GetDefaultFramerateByScenario()  */
        .max_framerate = 300,

    },
    .custom3 = {
        .pclk = 231270000,              //record different mode's pclk
        .linelength = 4572,             //record different mode's linelength
        .framelength = 1640,            //record different mode's framelength
        .startx = 2,                    //record different mode's startx of grabwindow
        .starty = 2,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2088,//2096,        //record different mode's width of grabwindow
        .grabwindow_height = 1544,//1552,       //record different mode's height of grabwindow
        /*   following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
        .mipi_data_lp2hs_settle_dc = 30,
        /*   following for GetDefaultFramerateByScenario()  */
        .max_framerate = 300,

    },
    .custom4 = {
       .pclk = 231270000,              //record different mode's pclk
        .linelength = 4572,             //record different mode's linelength
        .framelength = 1640,            //record different mode's framelength
        .startx = 2,                    //record different mode's startx of grabwindow
        .starty = 2,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2088,//2096,        //record different mode's width of grabwindow
        .grabwindow_height = 1544,//1552,       //record different mode's height of grabwindow
        /*   following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
        .mipi_data_lp2hs_settle_dc = 30,
        /*   following for GetDefaultFramerateByScenario()  */
        .max_framerate = 300,

    },
    .custom5 = {
        .pclk = 231270000,              //record different mode's pclk
        .linelength = 4572,             //record different mode's linelength
        .framelength = 1640,            //record different mode's framelength
        .startx = 2,                    //record different mode's startx of grabwindow
        .starty = 2,                    //record different mode's starty of grabwindow
        .grabwindow_width = 2088,//2096,        //record different mode's width of grabwindow
        .grabwindow_height = 1544,//1552,       //record different mode's height of grabwindow
        /*   following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
        .mipi_data_lp2hs_settle_dc = 30,
        /*   following for GetDefaultFramerateByScenario()  */
        .max_framerate = 300,

    },
    .margin = 10,
    .min_shutter = 1,
    .max_frame_length = 0xffff,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,
    .ihdr_support = 1,    //1, support; 0,not support
    .ihdr_le_firstline = 0,  //1,le first ; 0, se first
    .sensor_mode_num = 10,   //support sensor mode num

    .cap_delay_frame = 2,
    .pre_delay_frame = 2,
    .video_delay_frame = 5,
    .hs_video_delay_frame = 5,
    .slim_video_delay_frame = 5,
    .custom1_delay_frame = 2,
    .custom2_delay_frame = 2,
    .custom3_delay_frame = 2,
    .custom4_delay_frame = 2,
    .custom5_delay_frame = 2,

    .isp_driving_current = ISP_DRIVING_2MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_MANNUAL,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
    .sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_R,
    .mclk = 24,
    .mipi_lane_num = SENSOR_MIPI_4_LANE,
    .i2c_addr_table = {0x20, 0x40, 0xff},
    .i2c_speed = 300, // i2c read/write speed
};


static imgsensor_struct imgsensor = {
    .mirror = IMAGE_NORMAL,             //mirrorflip information
    .sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
    .shutter = 0x3D0,                   //current shutter   // Danbo ??
    .gain = 0x100,                      //current gain     // Danbo ??
    .dummy_pixel = 0,                   //current dummypixel
    .dummy_line = 0,                    //current dummyline
    .current_fps = 300,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
    .autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
    .test_pattern = KAL_FALSE,      //test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
    .current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
    .ihdr_en = 0, //sensor need support LE, SE with HDR feature
    .i2c_write_id = 0x20,
    .update_sensor_otp_awb = 0,
    .update_sensor_otp_lsc = 0,
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] =
{{ 4208, 3120, 0000, 0000, 4208, 3120, 2104, 1560, 0000, 0000, 2104, 1560, 0007, 0007, 2088, 1544}, // Preview 2112*1558
 { 4208, 3120, 0000, 0000, 4208, 3120, 4208, 3120, 0000, 0000, 4208, 3120, 0015, 0015, 4176, 3088}, // capture 4206*3128
 { 4208, 3120, 0000, 0000, 4208, 3120, 4208, 3120, 0000, 0000, 4208, 3120, 0015, 0015, 4176, 3088}, // video
 { 4208, 3120,  824,  840, 2560, 1440, 1280,  720, 0000, 0000, 1280,  720, 0000, 0000, 1280,  720}, //hight speed video
 { 4208, 3120,  824,  840, 2560, 1440, 1280,  720, 0000, 0000, 1280,  720, 0000, 0000, 1280,  720},// slim video
 { 4208, 3120, 0000, 0000, 4208, 3120, 2104, 1560, 0000, 0000, 2104, 1560, 0007, 0007, 2088, 1544}, // Custom1 (defaultuse preview)
 { 4208, 3120, 0000, 0000, 4208, 3120, 2104, 1560, 0000, 0000, 2104, 1560, 0007, 0007, 2088, 1544}, // Custom2
 { 4208, 3120, 0000, 0000, 4208, 3120, 2104, 1560, 0000, 0000, 2104, 1560, 0007, 0007, 2088, 1544}, // Custom3
 { 4208, 3120, 0000, 0000, 4208, 3120, 2104, 1560, 0000, 0000, 2104, 1560, 0007, 0007, 2088, 1544}, // Custom4
 { 4208, 3120, 0000, 0000, 4208, 3120, 2104, 1560, 0000, 0000, 2104, 1560, 0007, 0007, 2088, 1544}, // Custom5
 };// slim video
//#define IMX135_OTP_Enable 1

// Gain Index
#define MaxGainIndex (71)
static kal_uint16 sensorGainMapping[MaxGainIndex][2] ={
    {71  ,25 },
    {76  ,42 },
    {83  ,59 },
    {89  ,73 },
    {96  ,85 },
    {102 ,96 },
    {108 ,105},
    {115 ,114},
    {121 ,121},
    {128 ,128},
    {134 ,134},
    {140 ,140},
    {147 ,145},
    {153 ,149},
    {160 ,154},
    {166 ,158},
    {172 ,161},
    {179 ,164},
    {185 ,168},
    {192 ,171},
    {200 ,174},
    {208 ,177},
    {216 ,180},
    {224 ,183},
    {232 ,185},
    {240 ,188},
    {248 ,190},
    {256 ,192},
    {264 ,194},
    {272 ,196},
    {280 ,197},
    {288 ,199},
    {296 ,201},
    {304 ,202},
    {312 ,203},
    {320 ,205},
    {328 ,206},
    {336 ,207},
    {344 ,208},
    {352 ,209},
    {360 ,210},
    {368 ,211},
    {376 ,212},
    {384 ,213},
    {390 ,214},
    {399 ,215},
    {409 ,216},
    {419 ,217},
    {431 ,218},
    {442 ,219},
    {455 ,220},
    {467 ,221},
    {481 ,222},
    {496 ,223},
    {512 ,224},
    {528 ,225},
    {545 ,226},
    {565 ,227},
    {584 ,228},
    {606 ,229},
    {630 ,230},
    {655 ,231},
    {682 ,232},
    {712 ,233},
    {744 ,234},
    {780 ,235},
    {819 ,236},
    {862 ,237},
    {910 ,238},
    {963 ,239},
    {1024,240}
};

extern bool otp_update(BYTE update_sensor_otp_awb, BYTE update_sensor_otp_lsc);
extern void otp_clear_flag(void);

//#if IMX135_OTP_Enable
void write_cmos_sensor_16(kal_uint16 addr, kal_uint16 para)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor

    char pusendcmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para >> 8),(char)(para & 0xFF)};
    iWriteRegI2C(pusendcmd , 4, imgsensor.i2c_write_id);
}
//#endif

kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor

    kal_uint16 get_byte=0;

    char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

    return get_byte;
}

void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor

    char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor

    kal_uint16 get_byte=0;
    char pu_send_cmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(pu_send_cmd , 2, (u8*)&get_byte,1,imgsensor.i2c_write_id);
    return get_byte;
}


void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor

    char pu_send_cmd[4] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

static void set_dummy()
{
    LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);

    write_cmos_sensor_8(0x0104, 0x01);
    write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
    write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
    write_cmos_sensor(0x0342, (imgsensor.line_length >> 8) & 0xFF);
    write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
    write_cmos_sensor_8(0x0104, 0x00);

}   /*  set_dummy  */

kal_uint32 return_sensor_id()
{
    return ((read_cmos_sensor_8(0x0016) << 8) | read_cmos_sensor_8(0x0017));
}

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
    kal_int16 dummy_line;
    kal_uint32 frame_length = imgsensor.frame_length;
    //unsigned long flags;

    LOG_INF("framerate = %d, min framelength should enable = %d\n", framerate,min_framelength_en);

    frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
    spin_lock(&imgsensor_drv_lock);
    imgsensor.frame_length = (frame_length > imgsensor.min_frame_length) ? frame_length : imgsensor.min_frame_length;
    imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    //dummy_line = frame_length - imgsensor.min_frame_length;
    //if (dummy_line < 0)
        //imgsensor.dummy_line = 0;
    //else
        //imgsensor.dummy_line = dummy_line;
    //imgsensor.frame_length = frame_length + imgsensor.dummy_line;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
    {
        imgsensor.frame_length = imgsensor_info.max_frame_length;
        imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
    }
    if (min_framelength_en)
        imgsensor.min_frame_length = imgsensor.frame_length;
    spin_unlock(&imgsensor_drv_lock);
    //set_dummy();
}   /*  set_max_framerate  */


static void write_shutter(kal_uint16 shutter)
{
    kal_uint16 realtime_fps = 0;
    kal_uint32 frame_length = 0;

    /* 0x3500, 0x3501, 0x3502 will increase VBLANK to get exposure larger than frame exposure */
    /* AE doesn't update sensor gain at capture mode, thus extra exposure lines must be updated here. */

    // OV Recommend Solution
    // if shutter bigger than frame_length, should extend frame length first
    spin_lock(&imgsensor_drv_lock);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
    shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;

    // Framelength should be an even number
    shutter = (shutter >> 1) << 1;
    imgsensor.frame_length = (imgsensor.frame_length >> 1) << 1;

    if (imgsensor.autoflicker_en) {
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305)
        {
            set_max_framerate(296,0);
            write_cmos_sensor_8(0x0104, 0x01);
            write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
            write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
            write_cmos_sensor(0x0342, (imgsensor.line_length >> 8) & 0xFF);
            write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
            write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
            write_cmos_sensor(0x0203, shutter  & 0xFF);
            write_cmos_sensor_8(0x0104, 0x00);
            LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
            //set_dummy();
        }
        else if(realtime_fps >= 147 && realtime_fps <= 150)
        {
            set_max_framerate(146,0);
            write_cmos_sensor_8(0x0104, 0x01);
            write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
            write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
            write_cmos_sensor(0x0342, (imgsensor.line_length >> 8) & 0xFF);
            write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
            write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
            write_cmos_sensor(0x0203, shutter  & 0xFF);
            write_cmos_sensor_8(0x0104, 0x00);
            LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
            //set_dummy();
        }
        else {
            // Extend frame length
            write_cmos_sensor_8(0x0104, 0x01);
            write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
            write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
            write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
            write_cmos_sensor(0x0203, shutter  & 0xFF);
            write_cmos_sensor_8(0x0104, 0x00);
        }
    } else {
        // Extend frame length
        write_cmos_sensor_8(0x0104, 0x01);
        write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
        write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
        write_cmos_sensor(0x0203, shutter  & 0xFF);
        write_cmos_sensor_8(0x0104, 0x00);
    }

    // Update Shutter
    //write_cmos_sensor_8(0x0104, 0x01);
    //write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    //write_cmos_sensor(0x0203, shutter  & 0xFF);
    //write_cmos_sensor_8(0x0104, 0x00);
    LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
    //LOG_INF("frame_length = %d ", frame_length);

}   /*  write_shutter  */


/*************************************************************************
* FUNCTION
*   set_shutter
*
* DESCRIPTION
*   This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*   iShutter : exposured lines
*
* RETURNS
*   None
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
}   /*  set_shutter */



static kal_uint16 gain2reg(const kal_uint16 gain)
{
    kal_uint8 iI;

    for (iI = 0; iI < (MaxGainIndex-1); iI++) {
        if(gain <= sensorGainMapping[iI][0]){
            break;
        }
    }
/*
    if(gain != sensorGainMapping[iI][0])
    {
         //SENSORDB("Gain mapping don't correctly:%d %d \n", gain, sensorGainMapping[iI][0]);
         return sensorGainMapping[iI][1];
    }
    else return (kal_uint16)gain;
*/
	return sensorGainMapping[iI][1];

}

/*************************************************************************
* FUNCTION
*   set_gain
*
* DESCRIPTION
*   This function is to set global gain to sensor.
*
* PARAMETERS
*   iGain : sensor global gain(base: 0x40)
*
* RETURNS
*   the actually gain set to sensor.
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint16 set_gain(kal_uint16 gain)
{
    kal_uint16 reg_gain;

    /* 0x350A[0:1], 0x350B[0:7] AGC real gain */
    /* [0:3] = N meams N /16 X  */
    /* [4:9] = M meams M X       */
    /* Total gain = M + N /16 X   */

    //
    if (gain < BASEGAIN || gain > 32 * BASEGAIN) {
        LOG_INF("Error gain setting");

        if (gain < BASEGAIN)
            gain = BASEGAIN;
        else if (gain > 32 * BASEGAIN)
            gain = 32 * BASEGAIN;
    }

    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain;
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("gain = %d , reg_gain = 0x%x\n ", gain, reg_gain);

    write_cmos_sensor_8(0x0104, 0x01);
    write_cmos_sensor(0x0204, (reg_gain>>8)& 0xFF);
    write_cmos_sensor(0x0205, reg_gain & 0xFF);
    write_cmos_sensor_8(0x0104, 0x00);

    return gain;
}   /*  set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
    kal_uint8 iRation;
    kal_uint8 iReg;

    LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
    if (imgsensor.ihdr_en) {

            spin_lock(&imgsensor_drv_lock);
            if (le > imgsensor.min_frame_length - imgsensor_info.margin)
                imgsensor.frame_length = le + imgsensor_info.margin;
            else
                imgsensor.frame_length = imgsensor.min_frame_length;
            if (imgsensor.frame_length > imgsensor_info.max_frame_length)
                imgsensor.frame_length = imgsensor_info.max_frame_length;
            spin_unlock(&imgsensor_drv_lock);
            if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;
            if (se < imgsensor_info.min_shutter) se = imgsensor_info.min_shutter;


        // Extend frame length
        write_cmos_sensor_8(0x0104, 0x01);
        write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8)& 0xFF);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
        //long exporsure
        write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
        write_cmos_sensor(0x0203, le & 0xFF);
        //short exporsure
        write_cmos_sensor(0x0230, (se >> 8) & 0xFF);
        write_cmos_sensor(0x0231, se & 0xFF);

        iReg = gain2reg(gain);

        //  LE Gain
        write_cmos_sensor(0x0205, (kal_uint8)iReg);
        // SE  Gain
        write_cmos_sensor(0x0233, (kal_uint8)iReg);


        //SET LE/SE ration
        //iRation = (((LE + SE/2)/SE) >> 1 ) << 1 ;
        iRation = ((10 * le / se) + 5) / 10;
        if(iRation < 2)
            iRation = 0;
        else if(iRation < 4)
            iRation = 1;
        else if(iRation < 8)
            iRation = 2;
        else if(iRation < 16)
            iRation = 4;
        else if(iRation < 32)
            iRation = 8;
        else
            iRation = 0;
        write_cmos_sensor(0x0239,iRation);//   exposure ratio --> 2 : 1/4
        write_cmos_sensor_8(0x0104, 0x00);
        LOG_INF("[IMX135MIPI_IHDR_write_shutter_gain ] iRation:%d\n", iRation);

    }


}



static void set_mirror_flip(kal_uint8 image_mirror)
{
    LOG_INF("image_mirror = %d\n", image_mirror);

    /********************************************************
       *
       *   0x0101 Sensor mirror flip
       *
       *   ISP and Sensor flip or mirror register bit should be the same!!
       *
       ********************************************************/
    kal_uint8  iTemp;

    iTemp = read_cmos_sensor(0x0101);
    iTemp&= ~0x03; //Clear the mirror and flip bits.

    switch (image_mirror) {
        case IMAGE_NORMAL:
            write_cmos_sensor_8(0x0101, iTemp);    //Set normal
            break;
        case IMAGE_H_MIRROR:
            write_cmos_sensor_8(0x0101, iTemp | 0x01); //Set mirror
            break;
        case IMAGE_V_MIRROR:
            write_cmos_sensor_8(0x0101, iTemp | 0x02); //Set flip
            break;
        case IMAGE_HV_MIRROR:
            write_cmos_sensor_8(0x0101, iTemp | 0x03); //Set mirror and flip
            break;
        default:
            LOG_INF("Error image_mirror setting\n");
    }

}


/*************************************************************************
* FUNCTION
*   night_mode
*
* DESCRIPTION
*   This function night mode of sensor.
*
* PARAMETERS
*   bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/
}   /*  night_mode  */

static void sensor_init(void)
{
    LOG_INF("E\n");

   /*****************************************************************************
    0x3098[0:1] pll3_prediv
    pll3_prediv_map[] = {2, 3, 4, 6}

    0x3099[0:4] pll3_multiplier
    pll3_multiplier

    0x309C[0] pll3_rdiv
    pll3_rdiv + 1

    0x309A[0:3] pll3_sys_div
    pll3_sys_div + 1

    0x309B[0:1] pll3_div
    pll3_div[] = {2, 2, 4, 5}

    VCO = XVCLK * 2 / pll3_prediv * pll3_multiplier * pll3_rdiv
    sysclk = VCO * 2 * 2 / pll3_sys_div / pll3_div

    XVCLK = 24 MHZ
    0x3098, 0x03
    0x3099, 0x1e
    0x309a, 0x02
    0x309b, 0x01
    0x309c, 0x00


    VCO = 24 * 2 / 6 * 31 * 1
    sysclk = VCO * 2  * 2 / 3 / 2
    sysclk = 160 MHZ
    */

    write_cmos_sensor(0x0103, 0x01);//
    write_cmos_sensor(0x0101, 0x00);//
    write_cmos_sensor(0x0105, 0x01);//
    write_cmos_sensor(0x0110, 0x00);//
    write_cmos_sensor(0x0220, 0x01);//
    write_cmos_sensor(0x3302, 0x11);//
    write_cmos_sensor(0x3833, 0x20);//
    write_cmos_sensor(0x3893, 0x00);//
    write_cmos_sensor(0x3906, 0x08);//
    write_cmos_sensor(0x3907, 0x01);//
    write_cmos_sensor(0x391B, 0x01);//
    write_cmos_sensor(0x3C09, 0x01);//
    write_cmos_sensor(0x600A, 0x00);//
    write_cmos_sensor(0x3008, 0xB0);//
    write_cmos_sensor(0x320A, 0x01);//
    write_cmos_sensor(0x320D, 0x10);//
    write_cmos_sensor(0x3216, 0x2E);//
    write_cmos_sensor(0x322C, 0x02);//
    write_cmos_sensor(0x3409, 0x0C);//
    write_cmos_sensor(0x340C, 0x2D);//
    write_cmos_sensor(0x3411, 0x39);//
    write_cmos_sensor(0x3414, 0x1E);//
    write_cmos_sensor(0x3427, 0x04);//
    write_cmos_sensor(0x3480, 0x1E);//
    write_cmos_sensor(0x3484, 0x1E);//
    write_cmos_sensor(0x3488, 0x1E);//
    write_cmos_sensor(0x348C, 0x1E);//
    write_cmos_sensor(0x3490, 0x1E);//
    write_cmos_sensor(0x3494, 0x1E);//
    write_cmos_sensor(0x3511, 0x8F);//
    write_cmos_sensor(0x364F, 0x2D);//

    //quality

    //defect forrection recommended setting

    write_cmos_sensor(0x380A, 0x00);//
    write_cmos_sensor(0x380B, 0x00);//
    write_cmos_sensor(0x4103, 0x00);//

    //color artifact recommended setting

    write_cmos_sensor(0x4243, 0x9A);//
    write_cmos_sensor(0x4330, 0x01);//
    write_cmos_sensor(0x4331, 0x90);//
    write_cmos_sensor(0x4332, 0x02);//
    write_cmos_sensor(0x4333, 0x58);//
    write_cmos_sensor(0x4334, 0x03);//
    write_cmos_sensor(0x4335, 0x20);//
    write_cmos_sensor(0x4336, 0x03);//
    write_cmos_sensor(0x4337, 0x84);//
    write_cmos_sensor(0x433C, 0x01);//
    write_cmos_sensor(0x4340, 0x02);//
    write_cmos_sensor(0x4341, 0x58);//
    write_cmos_sensor(0x4342, 0x03);//
    write_cmos_sensor(0x4343, 0x52);//


    /////Moire reduction parameter setting

    write_cmos_sensor(0x4364, 0x0B);//
    write_cmos_sensor(0x4368, 0x00);//
    write_cmos_sensor(0x4369, 0x0F);//
    write_cmos_sensor(0x436A, 0x03);//
    write_cmos_sensor(0x436B, 0xA8);//
    write_cmos_sensor(0x436C, 0x00);//
    write_cmos_sensor(0x436D, 0x00);//
    write_cmos_sensor(0x436E, 0x00);//
    write_cmos_sensor(0x436F, 0x06);//

    //CNR parameter setting

    write_cmos_sensor(0x4281, 0x21);//
    write_cmos_sensor(0x4282, 0x18);//
    write_cmos_sensor(0x4283, 0x04);//
    write_cmos_sensor(0x4284, 0x08);//
    write_cmos_sensor(0x4287, 0x7F);//
    write_cmos_sensor(0x4288, 0x08);//
    write_cmos_sensor(0x428B, 0x7F);//
    write_cmos_sensor(0x428C, 0x08);//
    write_cmos_sensor(0x428F, 0x7F);//
    write_cmos_sensor(0x4297, 0x00);//
    write_cmos_sensor(0x4298, 0x7E);//
    write_cmos_sensor(0x4299, 0x7E);//
    write_cmos_sensor(0x429A, 0x7E);//
    write_cmos_sensor(0x42A4, 0xFB);//
    write_cmos_sensor(0x42A5, 0x7E);//
    write_cmos_sensor(0x42A6, 0xDF);//
    write_cmos_sensor(0x42A7, 0xB7);//
    write_cmos_sensor(0x42AF, 0x03);//

    // ARNR Parameter setting
    write_cmos_sensor(0x4207, 0x03);//
    write_cmos_sensor(0x4216, 0x08);//
    write_cmos_sensor(0x4217, 0x08);//

    //DLC Parammeter setting
    write_cmos_sensor(0x4218, 0x00);//
    write_cmos_sensor(0x421B, 0x20);//
    write_cmos_sensor(0x421F, 0x04);//
    write_cmos_sensor(0x4222, 0x02);//
    write_cmos_sensor(0x4223, 0x22);//
    write_cmos_sensor(0x422E, 0x54);//
    write_cmos_sensor(0x422F, 0xFB);//
    write_cmos_sensor(0x4230, 0xFF);//
    write_cmos_sensor(0x4231, 0xFE);//
    write_cmos_sensor(0x4232, 0xFF);//
    write_cmos_sensor(0x4235, 0x58);//
    write_cmos_sensor(0x4236, 0xF7);//
    write_cmos_sensor(0x4237, 0xFD);//
    write_cmos_sensor(0x4239, 0x4E);//
    write_cmos_sensor(0x423A, 0xFC);//
    write_cmos_sensor(0x423B, 0xFD);//

    //HDR


    //LSC setting
    write_cmos_sensor(0x452A, 0x02);//


    //white balance setting
    write_cmos_sensor(0x0712, 0x01);//
    write_cmos_sensor(0x0713, 0x00);//
    write_cmos_sensor(0x0714, 0x01);//
    write_cmos_sensor(0x0715, 0x00);//
    write_cmos_sensor(0x0716, 0x01);//
    write_cmos_sensor(0x0717, 0x00);//
    write_cmos_sensor(0x0718, 0x01);//
    write_cmos_sensor(0x0719, 0x00);//

    //shading setting
    write_cmos_sensor(0x4500, 0x1F);//
    //Disable  AE statistics
    write_cmos_sensor(0x33B3, 0x00);
    //Disable Embeded lines
    write_cmos_sensor(0x3314, 0x00);
#if 0
	#if IMX135_OTP_Enable
	otp_update();
	#endif
#endif

    //otp_update();
    imgsensor.update_sensor_otp_awb = 0; // Init to 0
    imgsensor.update_sensor_otp_lsc = 0; // Init to 0


}   /*  sensor_init  */



static void preview_setting(void)   //PreviewSetting
{
    //5.1.2 FQPreview 1296x972 30fps 24M MCLK 2lane 864Mbps/lane
    //PLL setting
    write_cmos_sensor(0x0100,0x00);// STREAM STop

    //PLL setting
    write_cmos_sensor(0x011E,0x18);//
    write_cmos_sensor(0x011F,0x00);//
    write_cmos_sensor(0x0301,0x05);//
    write_cmos_sensor(0x0303,0x01);//
    write_cmos_sensor(0x0305,0x0B);//
    write_cmos_sensor(0x0309,0x05);//
    write_cmos_sensor(0x030B,0x01);//
    write_cmos_sensor(0x030C,0x01);//
    write_cmos_sensor(0x030D,0x09);//
    write_cmos_sensor(0x030E,0x01);//
    write_cmos_sensor(0x3A06,0x11);//

    //Mode setting
    mdelay(10);
    write_cmos_sensor(0x0108,0x03);//
    write_cmos_sensor(0x0112,0x0A);//
    write_cmos_sensor(0x0113,0x0A);//
    write_cmos_sensor(0x0381,0x01);//
    write_cmos_sensor(0x0383,0x01);//
    write_cmos_sensor(0x0385,0x01);//
    write_cmos_sensor(0x0387,0x01);//
    write_cmos_sensor(0x0390,0x01);//
    write_cmos_sensor(0x0391,0x22);//
    write_cmos_sensor(0x0392,0x00);//
    write_cmos_sensor(0x0401,0x00);//
    write_cmos_sensor(0x0404,0x00);//
    write_cmos_sensor(0x0405,0x10);//
    write_cmos_sensor(0x4082,0x01);//
    write_cmos_sensor(0x4083,0x01);//
    write_cmos_sensor(0x7006,0x04);//

    //Optionnal function setting
if(imgsensor.update_sensor_otp_lsc != 0){}
else {
    write_cmos_sensor(0x0700,0x00);//
    write_cmos_sensor(0x3A63,0x00);//
}
    write_cmos_sensor(0x4100,0xF8);//
    write_cmos_sensor(0x4203,0xFF);//
    write_cmos_sensor(0x4344,0x00);//
    write_cmos_sensor(0x441C,0x01);//

    //Size setting
    write_cmos_sensor(0x0340,(imgsensor_info.pre.framelength>>8)&0xFF);//
    write_cmos_sensor(0x0341,(imgsensor_info.pre.framelength>>0)&0xFF);//
    write_cmos_sensor(0x0342,(imgsensor_info.pre.linelength>>8)&0xFF);//
    write_cmos_sensor(0x0343,(imgsensor_info.pre.linelength>>0)&0xFF);//
    write_cmos_sensor(0x0344,0x00);//
    write_cmos_sensor(0x0345,0x00);//
    write_cmos_sensor(0x0346,0x00);//
    write_cmos_sensor(0x0347,0x00);//
    write_cmos_sensor(0x0348,0x10);//
    write_cmos_sensor(0x0349,0x6F);//
    write_cmos_sensor(0x034A,0x0C);//
    write_cmos_sensor(0x034B,0x2F);//
    write_cmos_sensor(0x034C,0x08);//
    write_cmos_sensor(0x034D,0x38);//
    write_cmos_sensor(0x034E,0x06);//
    write_cmos_sensor(0x034F,0x18);//
    write_cmos_sensor(0x0350,0x00);//
    write_cmos_sensor(0x0351,0x00);//
    write_cmos_sensor(0x0352,0x00);//
    write_cmos_sensor(0x0353,0x00);//
    write_cmos_sensor(0x0354,0x08);//
    write_cmos_sensor(0x0355,0x38);//
    write_cmos_sensor(0x0356,0x06);//
    write_cmos_sensor(0x0357,0x18);//
    write_cmos_sensor(0x301D,0x30);//
    write_cmos_sensor(0x3310,0x08);//
    write_cmos_sensor(0x3311,0x38);//
    write_cmos_sensor(0x3312,0x06);//
    write_cmos_sensor(0x3313,0x18);//
    write_cmos_sensor(0x331C,0x04);//
    write_cmos_sensor(0x331D,0xAB);//
    write_cmos_sensor(0x4084,0x00);//
    write_cmos_sensor(0x4085,0x00);//
    write_cmos_sensor(0x4086,0x00);//
    write_cmos_sensor(0x4087,0x00);//
    write_cmos_sensor(0x4400,0x00);//

    //global timing setting
    write_cmos_sensor(0x0830,0x6F);//
    write_cmos_sensor(0x0831,0x27);//
    write_cmos_sensor(0x0832,0x4F);//
    write_cmos_sensor(0x0833,0x2F);//
    write_cmos_sensor(0x0834,0x2F);//
    write_cmos_sensor(0x0835,0x2F);//
    write_cmos_sensor(0x0836,0x9F);//
    write_cmos_sensor(0x0837,0x37);//
    write_cmos_sensor(0x0839,0x1F);//
    write_cmos_sensor(0x083A,0x17);//
    write_cmos_sensor(0x083B,0x02);//

    // integration time setting
    write_cmos_sensor(0x0202,0x06);//
    write_cmos_sensor(0x0203,0x64);//

    //gain setting
if(imgsensor.update_sensor_otp_awb != 0) {}
else {
    write_cmos_sensor(0x0205,0xe0);//
    write_cmos_sensor(0x020E,0x01);//
    write_cmos_sensor(0x020F,0x00);//
    write_cmos_sensor(0x0210,0x01);//
    write_cmos_sensor(0x0211,0x00);//
    write_cmos_sensor(0x0212,0x01);//
    write_cmos_sensor(0x0213,0x00);//
    write_cmos_sensor(0x0214,0x01);//
    write_cmos_sensor(0x0215,0x00);//
}
    if(imgsensor.ihdr_en ){
        //hdr setting
        write_cmos_sensor(0x0230,0x00);//
        write_cmos_sensor(0x0231,0x00);//
        //write_cmos_sensor(0x0233,0x00);//
        write_cmos_sensor(0x0234,0x00);//
        write_cmos_sensor(0x0235,0x40);//
        write_cmos_sensor(0x0236,0x00);//
        write_cmos_sensor(0x0238,0x00);//   0: auto mode  1: direct mode
        write_cmos_sensor(0x0239,0x04);//  exposure ratio --> 2 : 1/4
        write_cmos_sensor(0x023B,0x03);//
        write_cmos_sensor(0x023C,0x01);//
        write_cmos_sensor(0x33B0,0x08);//
        write_cmos_sensor(0x33B1,0x38);//
        //write_cmos_sensor(0x33B3,0x01);// don't send AE statistics
        write_cmos_sensor(0x33B4,0x01);//
        write_cmos_sensor(0x3873,0x00);//
        write_cmos_sensor(0x3800,0x00);//
        write_cmos_sensor(0x391b,0x01);//
        write_cmos_sensor(0x446c,0x00);//
    }

    write_cmos_sensor(0x3A43,0x01);//
    write_cmos_sensor(0x0100,0x01);// STREAM START

}   /*  preview_setting  */

static void capture_setting(kal_uint16 currefps)  // IMX135MIPI_set_13M
{
LOG_INF("E! currefps:%d\n",currefps);
    write_cmos_sensor(0x0100,0x00);// STREAM STop
    //ClockSetting
    if(currefps == 300) // default 30.0 fps
    {
        write_cmos_sensor(0x011E,0x18);
        write_cmos_sensor(0x011F,0x00);
        write_cmos_sensor(0x0301,0x05);
        write_cmos_sensor(0x0303,0x01);
        write_cmos_sensor(0x0305,0x0C);
        write_cmos_sensor(0x0309,0x05);
        write_cmos_sensor(0x030B,0x01);
        write_cmos_sensor(0x030C,0x02); // 1
        write_cmos_sensor(0x030D,0x1c); // b3
        write_cmos_sensor(0x030E,0x01);
        write_cmos_sensor(0x3A06,0x11);
    }
    else
    {
        write_cmos_sensor(0x011E,0x18);
        write_cmos_sensor(0x011F,0x00);
        write_cmos_sensor(0x0301,0x05);
        write_cmos_sensor(0x0303,0x01);
        write_cmos_sensor(0x0305,0x0C);
        write_cmos_sensor(0x0309,0x05);
        write_cmos_sensor(0x030B,0x01);
        write_cmos_sensor(0x030C,0x01);
        write_cmos_sensor(0x030D,0xb3);
        write_cmos_sensor(0x030E,0x01); // 1
        write_cmos_sensor(0x3A06,0x11); // 11
    }

    mdelay(10);
    //Modesetting
    write_cmos_sensor(0x0108,0x03);
    write_cmos_sensor(0x0112,0x0A);
    write_cmos_sensor(0x0113,0x0A);
    write_cmos_sensor(0x0381,0x01);
    write_cmos_sensor(0x0383,0x01);
    write_cmos_sensor(0x0385,0x01);
    write_cmos_sensor(0x0387,0x01);
    write_cmos_sensor(0x0390,0x00);
    write_cmos_sensor(0x0391,0x11);
    write_cmos_sensor(0x0392,0x00);
    write_cmos_sensor(0x0401,0x00);
    write_cmos_sensor(0x0404,0x00);
    write_cmos_sensor(0x0405,0x10);
    write_cmos_sensor(0x4082,0x01);
    write_cmos_sensor(0x4083,0x01);
    write_cmos_sensor(0x7006,0x04);

    //OptionnalFunctionsetting
if(imgsensor.update_sensor_otp_lsc != 0){}
else {
    write_cmos_sensor(0x0700,0x00);//
    write_cmos_sensor(0x3A63,0x00);//
}
    write_cmos_sensor(0x4100,0xF8);
    write_cmos_sensor(0x4203,0xFF);
    write_cmos_sensor(0x4344,0x00);
    write_cmos_sensor(0x441C,0x01);

    //Sizesetting
    write_cmos_sensor(0x0340,(imgsensor_info.cap.framelength>>8)&0xFF);//
    write_cmos_sensor(0x0341,(imgsensor_info.cap.framelength>>0)&0xFF);//
    write_cmos_sensor(0x0342,(imgsensor_info.cap.linelength>>8)&0xFF);//
    write_cmos_sensor(0x0343,(imgsensor_info.cap.linelength>>0)&0xFF);//
    write_cmos_sensor(0x0344,0x00);
    write_cmos_sensor(0x0345,0x00);
    write_cmos_sensor(0x0346,0x00);
    write_cmos_sensor(0x0347,0x00);
    write_cmos_sensor(0x0348,0x10);
    write_cmos_sensor(0x0349,0x6F);
    write_cmos_sensor(0x034A,0x0C);
    write_cmos_sensor(0x034B,0x2F);
    write_cmos_sensor(0x034C,0x10);
    write_cmos_sensor(0x034D,0x70);
    write_cmos_sensor(0x034E,0x0C);
    write_cmos_sensor(0x034F,0x30);
    write_cmos_sensor(0x0350,0x00);
    write_cmos_sensor(0x0351,0x00);
    write_cmos_sensor(0x0352,0x00);
    write_cmos_sensor(0x0353,0x00);
    write_cmos_sensor(0x0354,0x10);
    write_cmos_sensor(0x0355,0x70);
    write_cmos_sensor(0x0356,0x0C);
    write_cmos_sensor(0x0357,0x30);
    write_cmos_sensor(0x301D,0x30);
    write_cmos_sensor(0x3310,0x10);
    write_cmos_sensor(0x3311,0x70);
    write_cmos_sensor(0x3312,0x0C);
    write_cmos_sensor(0x3313,0x30);
    write_cmos_sensor(0x331C,0x01);
    write_cmos_sensor(0x331D,0x68);
    write_cmos_sensor(0x4084,0x00);
    write_cmos_sensor(0x4085,0x00);
    write_cmos_sensor(0x4086,0x00);
    write_cmos_sensor(0x4087,0x00);
    write_cmos_sensor(0x4400,0x00);
    if(currefps == 300)
    {
        write_cmos_sensor(0x0830,0x8f); // 7f
        write_cmos_sensor(0x0831,0x47); // 37
        write_cmos_sensor(0x0832,0x7f); // 67
        write_cmos_sensor(0x0833,0x4F); // 3f
        write_cmos_sensor(0x0834,0x47); // 3f
        write_cmos_sensor(0x0835,0x5f); // 47
        write_cmos_sensor(0x0836,0xff); // df
        write_cmos_sensor(0x0837,0x4f); // 47
        write_cmos_sensor(0x0839,0x1F); // 1f
        write_cmos_sensor(0x083A,0x17); // 17
        write_cmos_sensor(0x083B,0x02); // 2
        write_cmos_sensor(0x0202,0x0C);
        write_cmos_sensor(0x0203,0x42);// 46
    }
    else    // for PIP
    {
        //GlobalTimingSetting
        write_cmos_sensor(0x0830,0x7f);
        write_cmos_sensor(0x0831,0x37);
        write_cmos_sensor(0x0832,0x67);
        write_cmos_sensor(0x0833,0x3f);
        write_cmos_sensor(0x0834,0x3f);
        write_cmos_sensor(0x0835,0x47);
        write_cmos_sensor(0x0836,0xdf);
        write_cmos_sensor(0x0837,0x47);
        write_cmos_sensor(0x0839,0x1F);
        write_cmos_sensor(0x083A,0x17);
        write_cmos_sensor(0x083B,0x02);
        //IntegrationTimeSetting
        write_cmos_sensor(0x0202,0x0C);
        write_cmos_sensor(0x0203,0x46);

        //GlobalTimingSetting
        //IntegrationTimeSetting
    }
    //GainSetting
if(imgsensor.update_sensor_otp_awb != 0){}
else {
    write_cmos_sensor(0x0205,0x00); // e0
    write_cmos_sensor(0x020E,0x01);
    write_cmos_sensor(0x020F,0x00);
    write_cmos_sensor(0x0210,0x01);
    write_cmos_sensor(0x0211,0x00);
    write_cmos_sensor(0x0212,0x01);
    write_cmos_sensor(0x0213,0x00);
    write_cmos_sensor(0x0214,0x01);
    write_cmos_sensor(0x0215,0x00);
}
    //HDRSetting
#if 0
    write_cmos_sensor(0x0230,0x00);
    write_cmos_sensor(0x0231,0x00);
    write_cmos_sensor(0x0233,0x00);
    write_cmos_sensor(0x0234,0x00);
    write_cmos_sensor(0x0235,0x40);
    write_cmos_sensor(0x0238,0x00);
    write_cmos_sensor(0x0239,0x04);
    write_cmos_sensor(0x023B,0x00);
    write_cmos_sensor(0x023C,0x01);
    write_cmos_sensor(0x33B0,0x04);
    write_cmos_sensor(0x33B1,0x00);
    write_cmos_sensor(0x33B3,0x00);
    write_cmos_sensor(0x33B4,0x01);
    write_cmos_sensor(0x3800,0x00);
#endif//HDRSetting

    write_cmos_sensor(0x3A43,0x01);//
    write_cmos_sensor(0x0100,0x01);//STREAM START

}

static void normal_video_setting(kal_uint16 currefps)    // VideoFullSizeSetting
{
    LOG_INF("E! currefps:%d\n",currefps);

    write_cmos_sensor(0x0100,0x00);// STREAM STop
    //ClockSetting
    write_cmos_sensor(0x011E,0x18);
    write_cmos_sensor(0x011F,0x00);
    write_cmos_sensor(0x0301,0x05);
    write_cmos_sensor(0x0303,0x01);
    write_cmos_sensor(0x0305,0x0C);
    write_cmos_sensor(0x0309,0x05);
    write_cmos_sensor(0x030B,0x01);
    write_cmos_sensor(0x030C,0x02); // 1
    write_cmos_sensor(0x030D,0x1c); // b3
    write_cmos_sensor(0x030E,0x01); // 1
    write_cmos_sensor(0x3A06,0x11); // 11
    //Modesetting
    mdelay(10);
    write_cmos_sensor(0x0108,0x03);
    write_cmos_sensor(0x0112,0x0A);
    write_cmos_sensor(0x0113,0x0A);
    write_cmos_sensor(0x0381,0x01);
    write_cmos_sensor(0x0383,0x01);
    write_cmos_sensor(0x0385,0x01);
    write_cmos_sensor(0x0387,0x01);
    write_cmos_sensor(0x0390,0x00);
    write_cmos_sensor(0x0391,0x11);
    write_cmos_sensor(0x0392,0x00);
    write_cmos_sensor(0x0401,0x00);
    write_cmos_sensor(0x0404,0x00);
    write_cmos_sensor(0x0405,0x10);
    write_cmos_sensor(0x4082,0x01);
    write_cmos_sensor(0x4083,0x01);
    write_cmos_sensor(0x7006,0x04);

    //OptionnalFunctionsetting
if(imgsensor.update_sensor_otp_lsc != 0){}
else {
    write_cmos_sensor(0x0700,0x00);//
    write_cmos_sensor(0x3A63,0x00);//
}
    write_cmos_sensor(0x4100,0xF8);
    write_cmos_sensor(0x4203,0xFF);
    write_cmos_sensor(0x4344,0x00);
    write_cmos_sensor(0x441C,0x01);

    //Size setting
    write_cmos_sensor(0x0340,(imgsensor_info.normal_video.framelength>>8)&0xFF);//
    write_cmos_sensor(0x0341,(imgsensor_info.normal_video.framelength>>0)&0xFF);//
    write_cmos_sensor(0x0342,(imgsensor_info.normal_video.linelength>>8)&0xFF);//
    write_cmos_sensor(0x0343,(imgsensor_info.normal_video.linelength>>0)&0xFF);//
    write_cmos_sensor(0x0344,0x00);
    write_cmos_sensor(0x0345,0x00);
    write_cmos_sensor(0x0346,0x00);
    write_cmos_sensor(0x0347,0x00);
    write_cmos_sensor(0x0348,0x10);
    write_cmos_sensor(0x0349,0x6F);
    write_cmos_sensor(0x034A,0x0C);
    write_cmos_sensor(0x034B,0x2F);
    write_cmos_sensor(0x034C,0x10);
    write_cmos_sensor(0x034D,0x70);
    write_cmos_sensor(0x034E,0x0C);
    write_cmos_sensor(0x034F,0x30);
    write_cmos_sensor(0x0350,0x00);
    write_cmos_sensor(0x0351,0x00);
    write_cmos_sensor(0x0352,0x00);
    write_cmos_sensor(0x0353,0x00);
    write_cmos_sensor(0x0354,0x10);
    write_cmos_sensor(0x0355,0x70);
    write_cmos_sensor(0x0356,0x0C);
    write_cmos_sensor(0x0357,0x30);
    write_cmos_sensor(0x301D,0x30);
    write_cmos_sensor(0x3310,0x10);
    write_cmos_sensor(0x3311,0x70);
    write_cmos_sensor(0x3312,0x0C);
    write_cmos_sensor(0x3313,0x30);
    write_cmos_sensor(0x331C,0x01);
    write_cmos_sensor(0x331D,0x68);
    write_cmos_sensor(0x4084,0x00);
    write_cmos_sensor(0x4085,0x00);
    write_cmos_sensor(0x4086,0x00);
    write_cmos_sensor(0x4087,0x00);
    write_cmos_sensor(0x4400,0x00);

    //GlobalTimingSetting
    write_cmos_sensor(0x0830,0x8f); // 7f
    write_cmos_sensor(0x0831,0x47); // 37
    write_cmos_sensor(0x0832,0x7f); // 67
    write_cmos_sensor(0x0833,0x4F); // 3f
    write_cmos_sensor(0x0834,0x47); // 3f
    write_cmos_sensor(0x0835,0x5f); // 47
    write_cmos_sensor(0x0836,0xff); // df
    write_cmos_sensor(0x0837,0x4f); // 47
    write_cmos_sensor(0x0839,0x1F); // 1f
    write_cmos_sensor(0x083A,0x17); // 17
    write_cmos_sensor(0x083B,0x02); // 2
    //IntegrationTimeSetting
    write_cmos_sensor(0x0202,0x0C);
    write_cmos_sensor(0x0203,0x42);// 46

    //GainSetting
if(imgsensor.update_sensor_otp_awb != 0){}
else {
    write_cmos_sensor(0x0205,0x00); // e0
    write_cmos_sensor(0x020E,0x01);
    write_cmos_sensor(0x020F,0x00);
    write_cmos_sensor(0x0210,0x01);
    write_cmos_sensor(0x0211,0x00);
    write_cmos_sensor(0x0212,0x01);
    write_cmos_sensor(0x0213,0x00);
    write_cmos_sensor(0x0214,0x01);
    write_cmos_sensor(0x0215,0x00);
}
    //HDRSetting
#if 0
    write_cmos_sensor(0x0230,0x00);
    write_cmos_sensor(0x0231,0x00);
    write_cmos_sensor(0x0233,0x00);
    write_cmos_sensor(0x0234,0x00);
    write_cmos_sensor(0x0235,0x40);
    write_cmos_sensor(0x0238,0x00);
    write_cmos_sensor(0x0239,0x04);
    write_cmos_sensor(0x023B,0x00);
    write_cmos_sensor(0x023C,0x01);
    write_cmos_sensor(0x33B0,0x04);
    write_cmos_sensor(0x33B1,0x00);
    write_cmos_sensor(0x33B3,0x00);
    write_cmos_sensor(0x33B4,0x01);
    write_cmos_sensor(0x3800,0x00);
  #endif

    write_cmos_sensor(0x3A43,0x01);//
    write_cmos_sensor(0x0100,0x01);// STREAM START

}

void IMX135MIPI_set_Video_IHDR(kal_uint16 IHDR_En)
{

    write_cmos_sensor(0x0100,0x00);// STREAM STop
    //PLL setting
    write_cmos_sensor(0x011E,0x18);//
    write_cmos_sensor(0x011F,0x00);//
    write_cmos_sensor(0x0301,0x05);//
    write_cmos_sensor(0x0303,0x01);//
    write_cmos_sensor(0x0305,0x0B);//
    write_cmos_sensor(0x0309,0x05);//
    write_cmos_sensor(0x030B,0x01);//
    write_cmos_sensor(0x030C,0x01);//
    write_cmos_sensor(0x030D,0xEF);//
    write_cmos_sensor(0x030E,0x01);//
    write_cmos_sensor(0x3A06,0x11);//

    //Mode setting
    mdelay(10);
    write_cmos_sensor(0x0108,0x03);//
    write_cmos_sensor(0x0112,0x0E);//
    write_cmos_sensor(0x0113,0x0A);//
    write_cmos_sensor(0x0381,0x01);//
    write_cmos_sensor(0x0383,0x01);//
    write_cmos_sensor(0x0385,0x01);//
    write_cmos_sensor(0x0387,0x01);//
    write_cmos_sensor(0x0390,0x00);//
    write_cmos_sensor(0x0391,0x11);//
    write_cmos_sensor(0x0392,0x00);//
    write_cmos_sensor(0x0401,0x00);//
    write_cmos_sensor(0x0404,0x00);//
    write_cmos_sensor(0x0405,0x10);//
    write_cmos_sensor(0x4082,0x01);//
    write_cmos_sensor(0x4083,0x01);//
    write_cmos_sensor(0x7006,0x04);//
    //Optionnal function setting
    write_cmos_sensor(0x0700,0x00);//
    write_cmos_sensor(0x3A63,0x00);//
    write_cmos_sensor(0x4100,0xF8);//
    write_cmos_sensor(0x4203,0xFF);//
    write_cmos_sensor(0x4344,0x00);//
    write_cmos_sensor(0x441C,0x01);//
    //Size setting
    write_cmos_sensor(0x0340,(imgsensor_info.normal_video.framelength>>8)&0xFF);//
    write_cmos_sensor(0x0341,(imgsensor_info.normal_video.framelength>>0)&0xFF);//
    write_cmos_sensor(0x0342,(imgsensor_info.normal_video.linelength>>8)&0xFF);//
    write_cmos_sensor(0x0343,(imgsensor_info.normal_video.linelength>>0)&0xFF);//
    write_cmos_sensor(0x0344,0x00);//
    write_cmos_sensor(0x0345,0x00);//
    write_cmos_sensor(0x0346,0x00);//
    write_cmos_sensor(0x0347,0x00);//
    write_cmos_sensor(0x0348,0x10);//
    write_cmos_sensor(0x0349,0x6F);//
    write_cmos_sensor(0x034A,0x0C);//
    write_cmos_sensor(0x034B,0x2F);//
    write_cmos_sensor(0x034C,0x10);//
    write_cmos_sensor(0x034D,0x70);//
    write_cmos_sensor(0x034E,0x0C);//
    write_cmos_sensor(0x034F,0x30);//
    write_cmos_sensor(0x0350,0x00);//
    write_cmos_sensor(0x0351,0x00);//
    write_cmos_sensor(0x0352,0x00);//
    write_cmos_sensor(0x0353,0x00);//
    write_cmos_sensor(0x0354,0x10);//
    write_cmos_sensor(0x0355,0x70);//
    write_cmos_sensor(0x0356,0x0C);//
    write_cmos_sensor(0x0357,0x30);//
    write_cmos_sensor(0x301D,0x30);//
    write_cmos_sensor(0x3310,0x10);//
    write_cmos_sensor(0x3311,0x70);//
    write_cmos_sensor(0x3312,0x0C);//
    write_cmos_sensor(0x3313,0x30);//
    write_cmos_sensor(0x331C,0x01);//
    write_cmos_sensor(0x331D,0x68);//
    write_cmos_sensor(0x4084,0x00);//
    write_cmos_sensor(0x4085,0x00);//
    write_cmos_sensor(0x4086,0x00);//
    write_cmos_sensor(0x4087,0x00);//
    write_cmos_sensor(0x4400,0x00);//
    //global timing setting
    write_cmos_sensor(0x0830,0x8F);//
    write_cmos_sensor(0x0831,0x47);//
    write_cmos_sensor(0x0832,0x7F);//
    write_cmos_sensor(0x0833,0x4F);//
    write_cmos_sensor(0x0834,0x47);//
    write_cmos_sensor(0x0835,0x5F);//
    write_cmos_sensor(0x0836,0xFF);//
    write_cmos_sensor(0x0837,0x4F);//
    write_cmos_sensor(0x0839,0x1F);//
    write_cmos_sensor(0x083A,0x17);//
    write_cmos_sensor(0x083B,0x02);//

    // integration time setting
    write_cmos_sensor(0x0202,0x0C);//
    write_cmos_sensor(0x0203,0x44);//
    //gain setting
    write_cmos_sensor(0x0205,0x00);//
    write_cmos_sensor(0x020E,0x01);//
    write_cmos_sensor(0x020F,0x00);//
    write_cmos_sensor(0x0210,0x01);//
    write_cmos_sensor(0x0211,0x00);//
    write_cmos_sensor(0x0212,0x01);//
    write_cmos_sensor(0x0213,0x00);//
    write_cmos_sensor(0x0214,0x01);//
    write_cmos_sensor(0x0215,0x00);//

    if(IHDR_En)
    {
        LOG_INF("VIDEO IHDR\n");
        //hdr setting
        write_cmos_sensor(0x0230,0x00);//
        write_cmos_sensor(0x0231,0x00);//
        //write_cmos_sensor(0x0233,0x00);//
        write_cmos_sensor(0x0234,0x00);//
        write_cmos_sensor(0x0235,0x40);//
        write_cmos_sensor(0x0238,0x01);//   0: auto mode  1: direct mode
        write_cmos_sensor(0x0239,0x00);//  exposure ratio --> 1:1/2 , 2 : 1/4, 4:1/8
        write_cmos_sensor(0x023B,0x00);//
        write_cmos_sensor(0x023C,0x01);//
        write_cmos_sensor(0x33B0,0x10);//
        write_cmos_sensor(0x33B1,0x70);//
        //write_cmos_sensor(0x33B3,0x01);// don't send AE statistics
        write_cmos_sensor(0x33B4,0x01);//
        write_cmos_sensor(0x3873,0x03);//
        write_cmos_sensor(0x3800,0x00);//
        write_cmos_sensor(0x391b,0x00);//
        write_cmos_sensor(0x446c,0x01);//
        //IMX135MIPI_IHDR_write_shutter_gain(1100,128);
    }
    else
    {
        //Raw Mode
        write_cmos_sensor(0x0112,0x0a);//
        write_cmos_sensor(0x0113,0x0a);//
        write_cmos_sensor(0x4100,0xF8);//
        write_cmos_sensor(0x4101,0x00);//
        write_cmos_sensor(0x3873,0x00);//
        write_cmos_sensor(0x446c,0x00);//
    }

    write_cmos_sensor(0x3A43,0x01);//
    write_cmos_sensor(0x0100,0x01);// STREAM START
}


static void hs_video_setting()  // VideoHDSetting_120fps
{
    LOG_INF("E\n  Video  120fps ");
    //PLL setting
    write_cmos_sensor(0x0100,0x00);// STREAM STop

    //PLL setting
    write_cmos_sensor(0x011E,0x18);//
    write_cmos_sensor(0x011F,0x00);//
    write_cmos_sensor(0x0301,0x05);//
    write_cmos_sensor(0x0303,0x01);//
    write_cmos_sensor(0x0305,0x0C);//
    write_cmos_sensor(0x0309,0x05);//
    write_cmos_sensor(0x030B,0x01);//
    write_cmos_sensor(0x030C,0x02);//
    write_cmos_sensor(0x030D,0x1C);//
    write_cmos_sensor(0x030E,0x01);//
    write_cmos_sensor(0x3A06,0x11);//

    //Mode setting
    mdelay(10);
    write_cmos_sensor(0x0108,0x03);//
    write_cmos_sensor(0x0112,0x0A);//
    write_cmos_sensor(0x0113,0x0A);//
    write_cmos_sensor(0x0381,0x01);//
    write_cmos_sensor(0x0383,0x01);//
    write_cmos_sensor(0x0385,0x01);//
    write_cmos_sensor(0x0387,0x01);//
    write_cmos_sensor(0x0390,0x01);//
    write_cmos_sensor(0x0391,0x22);//
    write_cmos_sensor(0x0392,0x00);//
    write_cmos_sensor(0x0401,0x00);//
    write_cmos_sensor(0x0404,0x00);//
    write_cmos_sensor(0x0405,0x10);//
    write_cmos_sensor(0x4082,0x01);//
    write_cmos_sensor(0x4083,0x01);//
    write_cmos_sensor(0x7006,0x04);//

    //Optionnal function setting
if(imgsensor.update_sensor_otp_lsc != 0){}
else {
    write_cmos_sensor(0x0700,0x00);//
    write_cmos_sensor(0x3A63,0x00);//
}
    write_cmos_sensor(0x4100,0xF8);//
    write_cmos_sensor(0x4203,0xFF);//
    write_cmos_sensor(0x4344,0x00);//
    write_cmos_sensor(0x441C,0x01);//

    //Size setting
    write_cmos_sensor(0x0340,(imgsensor_info.hs_video.framelength>>8)&0xFF);//
    write_cmos_sensor(0x0341,(imgsensor_info.hs_video.framelength>>0)&0xFF);//
    write_cmos_sensor(0x0342,(imgsensor_info.hs_video.linelength>>8)&0xFF);//
    write_cmos_sensor(0x0343,(imgsensor_info.hs_video.linelength>>0)&0xFF);//
    write_cmos_sensor(0x0344,0x03);//
    write_cmos_sensor(0x0345,0x38);//
    write_cmos_sensor(0x0346,0x03);//
    write_cmos_sensor(0x0347,0x48);//
    write_cmos_sensor(0x0348,0x0D);//
    write_cmos_sensor(0x0349,0x37);//
    write_cmos_sensor(0x034A,0x08);//
    write_cmos_sensor(0x034B,0xE7);//
    write_cmos_sensor(0x034C,0x05);//
    write_cmos_sensor(0x034D,0x00);//
    write_cmos_sensor(0x034E,0x02);//
    write_cmos_sensor(0x034F,0xD0);//
    write_cmos_sensor(0x0350,0x00);//
    write_cmos_sensor(0x0351,0x00);//
    write_cmos_sensor(0x0352,0x00);//
    write_cmos_sensor(0x0353,0x00);//
    write_cmos_sensor(0x0354,0x05);//
    write_cmos_sensor(0x0355,0x00);//
    write_cmos_sensor(0x0356,0x02);//
    write_cmos_sensor(0x0357,0xD0);//
    write_cmos_sensor(0x301D,0x30);//
    write_cmos_sensor(0x3310,0x05);//
    write_cmos_sensor(0x3311,0x00);//
    write_cmos_sensor(0x3312,0x02);//
    write_cmos_sensor(0x3313,0xD0);//
    write_cmos_sensor(0x331C,0x03);//
    write_cmos_sensor(0x331D,0xE8);//
    write_cmos_sensor(0x4084,0x00);//
    write_cmos_sensor(0x4085,0x00);//
    write_cmos_sensor(0x4086,0x00);//
    write_cmos_sensor(0x4087,0x00);//
    write_cmos_sensor(0x4400,0x00);//

    //global timing setting
    write_cmos_sensor(0x0830,0x6F);//
    write_cmos_sensor(0x0831,0x27);//
    write_cmos_sensor(0x0832,0x47);//
    write_cmos_sensor(0x0833,0x27);//
    write_cmos_sensor(0x0834,0x27);//
    write_cmos_sensor(0x0835,0x27);//
    write_cmos_sensor(0x0836,0x8F);//
    write_cmos_sensor(0x0837,0x37);//
    write_cmos_sensor(0x0839,0x1F);//
    write_cmos_sensor(0x083A,0x17);//
    write_cmos_sensor(0x083B,0x02);//

    // integration time setting
    write_cmos_sensor(0x0202,0x03);//
    write_cmos_sensor(0x0203,0x0E);//

    //gain setting
if(imgsensor.update_sensor_otp_awb != 0){}
else {
    write_cmos_sensor(0x0205,0x00);//
    write_cmos_sensor(0x020E,0x01);//
    write_cmos_sensor(0x020F,0x00);//
    write_cmos_sensor(0x0210,0x01);//
    write_cmos_sensor(0x0211,0x00);//
    write_cmos_sensor(0x0212,0x01);//
    write_cmos_sensor(0x0213,0x00);//
    write_cmos_sensor(0x0214,0x01);//
    write_cmos_sensor(0x0215,0x00);//
}
#if 0
    //hdr setting
    write_cmos_sensor(0x0230,0x00);//
    write_cmos_sensor(0x0231,0x00);//
    write_cmos_sensor(0x0233,0x00);//
    write_cmos_sensor(0x0234,0x00);//
    write_cmos_sensor(0x0235,0x40);//
    write_cmos_sensor(0x0236,0x00);//
    write_cmos_sensor(0x0238,0x00);//
    write_cmos_sensor(0x0239,0x04);//
    write_cmos_sensor(0x023B,0x00);//
    write_cmos_sensor(0x023C,0x00);//
    write_cmos_sensor(0x33B0,0x04);//
    write_cmos_sensor(0x33B1,0x00);//
    write_cmos_sensor(0x33B3,0X00);//
    write_cmos_sensor(0x33B4,0X01);//
    write_cmos_sensor(0x3800,0X00);//
#endif

    write_cmos_sensor(0x3A43,0x01);//
    write_cmos_sensor(0x0100,0x01);// STREAM START

}

static void slim_video_setting()  // VideoHDSetting
{
    LOG_INF("E\n  Video  120fps ");
    //PLL setting
    write_cmos_sensor(0x0100,0x00);// STREAM STop

    //PLL setting
    write_cmos_sensor(0x011E,0x18);//
    write_cmos_sensor(0x011F,0x00);//
    write_cmos_sensor(0x0301,0x05);//
    write_cmos_sensor(0x0303,0x01);//
    write_cmos_sensor(0x0305,0x0C);//
    write_cmos_sensor(0x0309,0x05);//
    write_cmos_sensor(0x030B,0x01);//
    write_cmos_sensor(0x030C,0x00);//
    write_cmos_sensor(0x030D,0xE6);//
    write_cmos_sensor(0x030E,0x01);//
    write_cmos_sensor(0x3A06,0x11);//

    //Mode setting
    mdelay(10);
    write_cmos_sensor(0x0108,0x03);//
    write_cmos_sensor(0x0112,0x0A);//
    write_cmos_sensor(0x0113,0x0A);//
    write_cmos_sensor(0x0381,0x01);//
    write_cmos_sensor(0x0383,0x01);//
    write_cmos_sensor(0x0385,0x01);//
    write_cmos_sensor(0x0387,0x01);//
    write_cmos_sensor(0x0390,0x01);//
    write_cmos_sensor(0x0391,0x22);//
    write_cmos_sensor(0x0392,0x00);//
    write_cmos_sensor(0x0401,0x02);//
    write_cmos_sensor(0x0404,0x00);//
    write_cmos_sensor(0x0405,0x1A);//
    write_cmos_sensor(0x4082,0x00);//
    write_cmos_sensor(0x4083,0x00);//
    write_cmos_sensor(0x7006,0x04);//
if(imgsensor.update_sensor_otp_lsc != 0){}
else {
    //Optionnal function setting
    write_cmos_sensor(0x0700,0x00);//
    write_cmos_sensor(0x3A63,0x00);//
}
    write_cmos_sensor(0x4100,0xF8);//
    write_cmos_sensor(0x4203,0xFF);//
    write_cmos_sensor(0x4344,0x00);//
    write_cmos_sensor(0x441C,0x01);//

    //Size setting
    write_cmos_sensor(0x0340,(imgsensor_info.slim_video.framelength>>8)&0xFF);//
    write_cmos_sensor(0x0341,(imgsensor_info.slim_video.framelength>>0)&0xFF);//
    write_cmos_sensor(0x0342,(imgsensor_info.slim_video.linelength>>8)&0xFF);//
    write_cmos_sensor(0x0343,(imgsensor_info.slim_video.linelength>>0)&0xFF);//
    write_cmos_sensor(0x0344,0x00);//
    write_cmos_sensor(0x0345,0x18);//
    write_cmos_sensor(0x0346,0x01);//
    write_cmos_sensor(0x0347,0x88);//
    write_cmos_sensor(0x0348,0x10);//
    write_cmos_sensor(0x0349,0x57);//
    write_cmos_sensor(0x034A,0x0A);//
    write_cmos_sensor(0x034B,0xAB);//
    write_cmos_sensor(0x034C,0x05);//
    write_cmos_sensor(0x034D,0x00);//
    write_cmos_sensor(0x034E,0x02);//
    write_cmos_sensor(0x034F,0xD0);//
    write_cmos_sensor(0x0350,0x00);//
    write_cmos_sensor(0x0351,0x00);//
    write_cmos_sensor(0x0352,0x00);//
    write_cmos_sensor(0x0353,0x00);//
    write_cmos_sensor(0x0354,0x08);//
    write_cmos_sensor(0x0355,0x20);//
    write_cmos_sensor(0x0356,0x04);//
    write_cmos_sensor(0x0357,0x92);//
    write_cmos_sensor(0x301D,0x30);//
    write_cmos_sensor(0x3310,0x05);//
    write_cmos_sensor(0x3311,0x00);//
    write_cmos_sensor(0x3312,0x02);//
    write_cmos_sensor(0x3313,0xD0);//
    write_cmos_sensor(0x331C,0x03);//
    write_cmos_sensor(0x331D,0xE8);//
    write_cmos_sensor(0x4084,0x05);//
    write_cmos_sensor(0x4085,0x00);//
    write_cmos_sensor(0x4086,0x02);//
    write_cmos_sensor(0x4087,0xD0);//
    write_cmos_sensor(0x4400,0x00);//

    //global timing setting
    write_cmos_sensor(0x0830,0x67);//
    write_cmos_sensor(0x0831,0x27);//
    write_cmos_sensor(0x0832,0x47);//
    write_cmos_sensor(0x0833,0x27);//
    write_cmos_sensor(0x0834,0x27);//
    write_cmos_sensor(0x0835,0x1F);//
    write_cmos_sensor(0x0836,0x87);//
    write_cmos_sensor(0x0837,0x2F);//
    write_cmos_sensor(0x0839,0x1F);//
    write_cmos_sensor(0x083A,0x17);//
    write_cmos_sensor(0x083B,0x02);//

    // integration time setting
    write_cmos_sensor(0x0202,0x05);//
    write_cmos_sensor(0x0203,0x1C);//
if(imgsensor.update_sensor_otp_awb != 0){}
else {
    //gain setting
    write_cmos_sensor(0x0205,0x00);//
    write_cmos_sensor(0x020E,0x01);//
    write_cmos_sensor(0x020F,0x00);//
    write_cmos_sensor(0x0210,0x01);//
    write_cmos_sensor(0x0211,0x00);//
    write_cmos_sensor(0x0212,0x01);//
    write_cmos_sensor(0x0213,0x00);//
    write_cmos_sensor(0x0214,0x01);//
    write_cmos_sensor(0x0215,0x00);//
}
    //HDRSetting
#if 0
    //hdr setting
    write_cmos_sensor(0x0230,0x00);//
    write_cmos_sensor(0x0231,0x00);//
    write_cmos_sensor(0x0233,0x00);//
    write_cmos_sensor(0x0234,0x00);//
    write_cmos_sensor(0x0235,0x40);//
    write_cmos_sensor(0x0238,0x00);//
    write_cmos_sensor(0x0239,0x04);//
    write_cmos_sensor(0x023B,0x00);//
    write_cmos_sensor(0x023C,0x01);//
    write_cmos_sensor(0x33B0,0x04);//
    write_cmos_sensor(0x33B1,0x00);//
	write_cmos_sensor(0x33B3,0x00);//
	write_cmos_sensor(0x33B4,0x01);//
	write_cmos_sensor(0x3800,0x00);//
#endif

    write_cmos_sensor(0x3A43,0x01);//
    write_cmos_sensor(0x0100,0x01);// STREAM START

}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);

    if (enable) {
        // 0x5E00[8]: 1 enable,  0 disable
        // 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        write_cmos_sensor(0x30D8, 0x10);
        write_cmos_sensor(0x0600, 0x00);
        write_cmos_sensor(0x0601, 0x02);
    } else {
        // 0x5E00[8]: 1 enable,  0 disable
        // 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        write_cmos_sensor(0x30D8, 0x00);
    }
    spin_lock(&imgsensor_drv_lock);
    imgsensor.test_pattern = enable;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   get_imgsensor_id
*
* DESCRIPTION
*   This function get the sensor ID
*
* PARAMETERS
*   *sensorID : return the sensor ID
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id)
{
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    //sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            *sensor_id = return_sensor_id();
            if (*sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
                return ERROR_NONE;
            }
            LOG_INF("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        retry = 2;
    }
    if (*sensor_id != imgsensor_info.sensor_id) {
        // if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF
        *sensor_id = 0xFFFFFFFF;
        return ERROR_SENSOR_CONNECT_FAIL;
    }
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*   open
*
* DESCRIPTION
*   This function initialize the registers of CMOS sensor
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
    //const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
    kal_uint8 i = 0;
    kal_uint8 retry = 2;
    kal_uint32 sensor_id = 0;
    LOG_1;
    LOG_2;

    //sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
            sensor_id = return_sensor_id();
            if (sensor_id == imgsensor_info.sensor_id) {
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
                break;
            }
            LOG_INF("Read sensor id fail, id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        if (sensor_id == imgsensor_info.sensor_id)
            break;
        retry = 2;
    }
    if (imgsensor_info.sensor_id != sensor_id)
        return ERROR_SENSOR_CONNECT_FAIL;

    /* initail sequence write in  */
    sensor_init();

    spin_lock(&imgsensor_drv_lock);

    imgsensor.autoflicker_en= KAL_FALSE;
    imgsensor.sensor_mode = IMGSENSOR_MODE_INIT;
    imgsensor.pclk = imgsensor_info.pre.pclk;
    imgsensor.frame_length = imgsensor_info.pre.framelength;
    imgsensor.line_length = imgsensor_info.pre.linelength;
    imgsensor.min_frame_length = imgsensor_info.pre.framelength;
    imgsensor.dummy_pixel = 0;
    imgsensor.dummy_line = 0;
    imgsensor.ihdr_en = KAL_FALSE;
    imgsensor.test_pattern = KAL_FALSE;
    imgsensor.current_fps = imgsensor_info.pre.max_framerate;
    spin_unlock(&imgsensor_drv_lock);

    return ERROR_NONE;
}   /*  open  */



/*************************************************************************
* FUNCTION
*   close
*
* DESCRIPTION
*
*
* PARAMETERS
*   None
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
    LOG_INF("E reset otp flag\n");
	otp_clear_flag();//clear otp flag

    /*No Need to implement this function*/

    return ERROR_NONE;
}   /*  close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*   This function start the sensor preview.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 preview(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    kal_uint32 sensor_id = 0;

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
    sensor_id = return_sensor_id();
    LOG_INF("L, sensor_id[%d]\n",sensor_id);
    return ERROR_NONE;
}   /*  preview   */

/*************************************************************************
* FUNCTION
*   capture
*
* DESCRIPTION
*   This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                          MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");
    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
    if (imgsensor.current_fps == 300) {
        imgsensor.pclk = imgsensor_info.cap.pclk;
        imgsensor.line_length = imgsensor_info.cap.linelength;
        imgsensor.frame_length = imgsensor_info.cap.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    else  {  //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
        imgsensor.pclk = imgsensor_info.cap1.pclk;
        imgsensor.line_length = imgsensor_info.cap1.linelength;
        imgsensor.frame_length = imgsensor_info.cap1.framelength;
        imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
        imgsensor.autoflicker_en = KAL_FALSE;
    }
    spin_unlock(&imgsensor_drv_lock);
    LOG_INF("Caputre fps:%d\n",imgsensor.current_fps);
    capture_setting(imgsensor.current_fps);

    LOG_INF("L\n");
    return ERROR_NONE;
}   /* capture() */
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
    //imgsensor.current_fps = 300;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
#ifdef IHDR_USED
    if(imgsensor.ihdr_en)
        IMX135MIPI_set_Video_IHDR(1);
    else
        IMX135MIPI_set_Video_IHDR(0);
#else
    normal_video_setting(imgsensor.current_fps);
#endif

    LOG_INF("L\n");
    return ERROR_NONE;
}   /*  normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_HIGH_SPEED_VIDEO;
    imgsensor.pclk = imgsensor_info.hs_video.pclk;
    //imgsensor.video_mode = KAL_TRUE;
    imgsensor.line_length = imgsensor_info.hs_video.linelength;
    imgsensor.frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.min_frame_length = imgsensor_info.hs_video.framelength;
    imgsensor.dummy_line = 0;
    imgsensor.dummy_pixel = 0;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    hs_video_setting();
    LOG_INF("L\n");
    return ERROR_NONE;
}   /*  hs_video   */

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
    LOG_INF("L\n");
    return ERROR_NONE;
}   /*  slim_video   */

/*************************************************************************
* FUNCTION
* Custom1
*
* DESCRIPTION
*   This function start the sensor Custom1.
*
* PARAMETERS
*   *image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*   None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 Custom1(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM1;
    imgsensor.pclk = imgsensor_info.custom1.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom1.linelength;
    imgsensor.frame_length = imgsensor_info.custom1.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom1.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom1   */

static kal_uint32 Custom2(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM2;
    imgsensor.pclk = imgsensor_info.custom2.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom2.linelength;
    imgsensor.frame_length = imgsensor_info.custom2.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom2.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom2   */

static kal_uint32 Custom3(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM3;
    imgsensor.pclk = imgsensor_info.custom3.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom3.linelength;
    imgsensor.frame_length = imgsensor_info.custom3.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom3.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom3   */

static kal_uint32 Custom4(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM4;
    imgsensor.pclk = imgsensor_info.custom4.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom4.linelength;
    imgsensor.frame_length = imgsensor_info.custom4.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom4.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom4   */


static kal_uint32 Custom5(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("E\n");

    spin_lock(&imgsensor_drv_lock);
    imgsensor.sensor_mode = IMGSENSOR_MODE_CUSTOM5;
    imgsensor.pclk = imgsensor_info.custom5.pclk;
    //imgsensor.video_mode = KAL_FALSE;
    imgsensor.line_length = imgsensor_info.custom5.linelength;
    imgsensor.frame_length = imgsensor_info.custom5.framelength;
    imgsensor.min_frame_length = imgsensor_info.custom5.framelength;
    imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    preview_setting();
    return ERROR_NONE;
}   /*  Custom5   */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    LOG_INF("E\n");

    LOG_INF("imgsensor_info.cap.grabwindow_width: %d\n", imgsensor_info.cap.grabwindow_width);
    sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
    sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;

    sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
    sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

    sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
    sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;


    sensor_resolution->SensorHighSpeedVideoWidth     = imgsensor_info.hs_video.grabwindow_width;
    sensor_resolution->SensorHighSpeedVideoHeight    = imgsensor_info.hs_video.grabwindow_height;

    sensor_resolution->SensorSlimVideoWidth  = imgsensor_info.slim_video.grabwindow_width;
    sensor_resolution->SensorSlimVideoHeight     = imgsensor_info.slim_video.grabwindow_height;

    sensor_resolution->SensorCustom1Width  = imgsensor_info.custom1.grabwindow_width;
    sensor_resolution->SensorCustom1Height     = imgsensor_info.custom1.grabwindow_height;

    sensor_resolution->SensorCustom2Width  = imgsensor_info.custom2.grabwindow_width;
    sensor_resolution->SensorCustom2Height     = imgsensor_info.custom2.grabwindow_height;

    sensor_resolution->SensorCustom3Width  = imgsensor_info.custom3.grabwindow_width;
    sensor_resolution->SensorCustom3Height     = imgsensor_info.custom3.grabwindow_height;

    sensor_resolution->SensorCustom4Width  = imgsensor_info.custom4.grabwindow_width;
    sensor_resolution->SensorCustom4Height     = imgsensor_info.custom4.grabwindow_height;

    sensor_resolution->SensorCustom5Width  = imgsensor_info.custom5.grabwindow_width;
    sensor_resolution->SensorCustom5Height     = imgsensor_info.custom5.grabwindow_height;

    LOG_INF("L\n");
    return ERROR_NONE;
}   /*  get_resolution  */

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
                      MSDK_SENSOR_INFO_STRUCT *sensor_info,
                      MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
    LOG_INF("scenario_id = %d\n", scenario_id);


    //sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
    //sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
    //imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

    sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
    sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
    sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
    sensor_info->SensorInterruptDelayLines = 1; /* not use */
    sensor_info->SensorResetActiveHigh = FALSE; /* not use */
    sensor_info->SensorResetDelayCount = 5; /* not use */

    sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
    sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
    sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
    sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

    sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame;
    sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame;
    sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
    sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
    sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;
    sensor_info->Custom1DelayFrame = imgsensor_info.custom1_delay_frame;
    sensor_info->Custom2DelayFrame = imgsensor_info.custom2_delay_frame;
    sensor_info->Custom3DelayFrame = imgsensor_info.custom3_delay_frame;
    sensor_info->Custom4DelayFrame = imgsensor_info.custom4_delay_frame;
    sensor_info->Custom5DelayFrame = imgsensor_info.custom5_delay_frame;

    sensor_info->SensorMasterClockSwitch = 0; /* not use */
    sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;

    sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame;          /* The frame of setting shutter default 0 for TG int */
    sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;    /* The frame of setting sensor gain */
    sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;
    sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
    sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
    sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;

    sensor_info->SensorMIPILaneNumber = imgsensor_info.mipi_lane_num;
    sensor_info->SensorClockFreq = imgsensor_info.mclk;
    sensor_info->SensorClockDividCount = 5; /* not use */
    sensor_info->SensorClockRisingCount = 0;
    sensor_info->SensorClockFallingCount = 2; /* not use */
    sensor_info->SensorPixelClockCount = 3; /* not use */
    sensor_info->SensorDataLatchCount = 2; /* not use */

    sensor_info->MIPIDataLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->MIPICLKLowPwr2HighSpeedTermDelayCount = 0;
    sensor_info->SensorWidthSampling = 0;  // 0 is default 1x
    sensor_info->SensorHightSampling = 0;   // 0 is default 1x
    sensor_info->SensorPacketECCOrder = 1;

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            sensor_info->SensorGrabStartX = imgsensor_info.cap.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.cap.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.cap.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:

            sensor_info->SensorGrabStartX = imgsensor_info.normal_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.normal_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.normal_video.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.hs_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.hs_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.hs_video.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            sensor_info->SensorGrabStartX = imgsensor_info.slim_video.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.slim_video.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.slim_video.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            sensor_info->SensorGrabStartX = imgsensor_info.custom1.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.custom1.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            sensor_info->SensorGrabStartX = imgsensor_info.custom2.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.custom2.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            sensor_info->SensorGrabStartX = imgsensor_info.custom3.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.custom3.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            sensor_info->SensorGrabStartX = imgsensor_info.custom4.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.custom4.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            sensor_info->SensorGrabStartX = imgsensor_info.custom5.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.custom5.starty;
            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.custom1.mipi_data_lp2hs_settle_dc;

            break;
        default:
            sensor_info->SensorGrabStartX = imgsensor_info.pre.startx;
            sensor_info->SensorGrabStartY = imgsensor_info.pre.starty;

            sensor_info->MIPIDataLowPwr2HighSpeedSettleDelayCount = imgsensor_info.pre.mipi_data_lp2hs_settle_dc;
            break;
    }

    return ERROR_NONE;
}   /*  get_info  */


static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
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
            normal_video(image_window, sensor_config_data);  // VideoFullSizeSetting
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            hs_video(image_window, sensor_config_data);  // VideoHDSetting_120fps
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            slim_video(image_window, sensor_config_data); // VideoHDSetting
            break;
        case MSDK_SCENARIO_ID_CUSTOM1:
            Custom1(image_window, sensor_config_data); // Custom1
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            Custom2(image_window, sensor_config_data); // Custom1
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            Custom3(image_window, sensor_config_data); // Custom1
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            Custom4(image_window, sensor_config_data); // Custom1
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            Custom5(image_window, sensor_config_data); // Custom1
            break;
        default:
            LOG_INF("Error ScenarioId setting");
            preview(image_window, sensor_config_data);
            return ERROR_INVALID_SCENARIO_ID;
    }
    return ERROR_NONE;
}   /* control() */



static kal_uint32 set_video_mode(UINT16 framerate)
{
    LOG_INF("framerate = %d\n ", framerate);
    // SetVideoMode Function should fix framerate
    if (framerate == 0)
        // Dynamic frame rate
        return ERROR_NONE;
    spin_lock(&imgsensor_drv_lock);
    if ((framerate == 300) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 296;
    else if ((framerate == 150) && (imgsensor.autoflicker_en == KAL_TRUE))
        imgsensor.current_fps = 146;
    else
        imgsensor.current_fps = framerate;
    spin_unlock(&imgsensor_drv_lock);
    set_max_framerate(imgsensor.current_fps,1);
    set_dummy();
    return ERROR_NONE;
}

static kal_uint32 set_auto_flicker_mode(kal_bool enable, UINT16 framerate)
{
    LOG_INF("enable = %d, framerate = %d \n", enable, framerate);
    spin_lock(&imgsensor_drv_lock);
    if (enable) //enable auto flicker
        imgsensor.autoflicker_en = KAL_TRUE;
    else //Cancel Auto flick
        imgsensor.autoflicker_en = KAL_FALSE;
    spin_unlock(&imgsensor_drv_lock);
    return ERROR_NONE;
}


static kal_uint32 set_max_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 framerate)
{
    kal_uint32 frame_length;

    LOG_INF("scenario_id = %d, framerate = %d\n", scenario_id, framerate);

    switch (scenario_id) {
        case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
            break;
        case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
            if(framerate == 0)
                return ERROR_NONE;
            frame_length = imgsensor_info.normal_video.pclk / framerate * 10 / imgsensor_info.normal_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.normal_video.framelength) ? (frame_length - imgsensor_info.normal_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.normal_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
            break;
        case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
            if (imgsensor.current_fps == 300){
                    frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                    spin_lock(&imgsensor_drv_lock);
                    imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
                    imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
                    imgsensor.min_frame_length = imgsensor.frame_length;
                    spin_unlock(&imgsensor_drv_lock);
                }
            else{
                    frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                    spin_lock(&imgsensor_drv_lock);
                    imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
                    imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
                    spin_unlock(&imgsensor_drv_lock);
            }
            //set_dummy();
            break;
        case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
            frame_length = imgsensor_info.hs_video.pclk / framerate * 10 / imgsensor_info.hs_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.hs_video.framelength) ? (frame_length - imgsensor_info.hs_video.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.hs_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
            break;
        case MSDK_SCENARIO_ID_SLIM_VIDEO:
            frame_length = imgsensor_info.slim_video.pclk / framerate * 10 / imgsensor_info.slim_video.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.slim_video.framelength) ? (frame_length - imgsensor_info.slim_video.framelength): 0;
            imgsensor.frame_length = imgsensor_info.slim_video.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
        case MSDK_SCENARIO_ID_CUSTOM1:
            frame_length = imgsensor_info.custom1.pclk / framerate * 10 / imgsensor_info.custom1.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom1.framelength) ? (frame_length - imgsensor_info.custom1.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            frame_length = imgsensor_info.custom2.pclk / framerate * 10 / imgsensor_info.custom2.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom2.framelength) ? (frame_length - imgsensor_info.custom2.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom2.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            frame_length = imgsensor_info.custom3.pclk / framerate * 10 / imgsensor_info.custom3.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom3.framelength) ? (frame_length - imgsensor_info.custom3.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom3.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            frame_length = imgsensor_info.custom4.pclk / framerate * 10 / imgsensor_info.custom4.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom4.framelength) ? (frame_length - imgsensor_info.custom4.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom4.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            frame_length = imgsensor_info.custom5.pclk / framerate * 10 / imgsensor_info.custom5.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.custom5.framelength) ? (frame_length - imgsensor_info.custom5.framelength) : 0;
            if (imgsensor.dummy_line < 0)
                imgsensor.dummy_line = 0;
            imgsensor.frame_length = imgsensor_info.custom1.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
            break;
        default:  //coding with  preview scenario by default
            frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
            spin_lock(&imgsensor_drv_lock);
            imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
            imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
            imgsensor.min_frame_length = imgsensor.frame_length;
            spin_unlock(&imgsensor_drv_lock);
            //set_dummy();
            LOG_INF("error scenario_id = %d, we use preview scenario \n", scenario_id);
            break;
    }
    return ERROR_NONE;
}


static kal_uint32 get_default_framerate_by_scenario(MSDK_SCENARIO_ID_ENUM scenario_id, MUINT32 *framerate)
{
    LOG_INF("scenario_id = %d\n", scenario_id);

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
        case MSDK_SCENARIO_ID_CUSTOM1:
            *framerate = imgsensor_info.custom1.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM2:
            *framerate = imgsensor_info.custom2.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM3:
            *framerate = imgsensor_info.custom3.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM4:
            *framerate = imgsensor_info.custom4.max_framerate;
            break;
        case MSDK_SCENARIO_ID_CUSTOM5:
            *framerate = imgsensor_info.custom5.max_framerate;
            break;
        default:
            break;
    }

    return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
                             UINT8 *feature_para,UINT32 *feature_para_len)
{
    UINT16 *feature_return_para_16=(UINT16 *) feature_para;
    UINT16 *feature_data_16=(UINT16 *) feature_para;
    UINT32 *feature_return_para_32=(UINT32 *) feature_para;
    UINT32 *feature_data_32=(UINT32 *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;
    unsigned long long *feature_return_para=(unsigned long long *) feature_para;

    SENSOR_WINSIZE_INFO_STRUCT *wininfo;
    MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;

    LOG_INF("feature_id = %d\n", feature_id);
    switch (feature_id) {
        case SENSOR_FEATURE_GET_PERIOD:
            *feature_return_para_16++ = imgsensor.line_length;
            *feature_return_para_16 = imgsensor.frame_length;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_GET_PIXEL_CLOCK_FREQ:
            LOG_INF("feature_Control imgsensor.pclk = %d,imgsensor.current_fps = %d\n", imgsensor.pclk,imgsensor.current_fps);
            *feature_return_para_32 = imgsensor.pclk;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_ESHUTTER:
            set_shutter(*feature_data);
            break;
        case SENSOR_FEATURE_SET_NIGHTMODE:
            night_mode((BOOL) *feature_data);
            break;
        case SENSOR_FEATURE_SET_GAIN:
            set_gain((UINT16) *feature_data);
            break;
        case SENSOR_FEATURE_SET_FLASHLIGHT:
            break;
        case SENSOR_FEATURE_SET_ISP_MASTER_CLOCK_FREQ:
            break;
        case SENSOR_FEATURE_SET_REGISTER:
            write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
            break;
        case SENSOR_FEATURE_GET_REGISTER:
            sensor_reg_data->RegData = read_cmos_sensor(sensor_reg_data->RegAddr);
            break;
        case SENSOR_FEATURE_GET_LENS_DRIVER_ID:
            // get the lens driver ID from EEPROM or just return LENS_DRIVER_ID_DO_NOT_CARE
            // if EEPROM does not exist in camera module.
            *feature_return_para_32=LENS_DRIVER_ID_DO_NOT_CARE;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_VIDEO_MODE:
            set_video_mode(*feature_data);
            break;
        case SENSOR_FEATURE_CHECK_SENSOR_ID:
            get_imgsensor_id(feature_return_para_32);
            break;
        case SENSOR_FEATURE_SET_AUTO_FLICKER_MODE:
            set_auto_flicker_mode((BOOL)*feature_data_16,*(feature_data_16+1));
            break;
        case SENSOR_FEATURE_SET_MAX_FRAME_RATE_BY_SCENARIO:
            set_max_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data, *(feature_data+1));
            break;
        case SENSOR_FEATURE_GET_DEFAULT_FRAME_RATE_BY_SCENARIO:
            get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*(feature_data), (MUINT32 *)(uintptr_t)(*(feature_data+1)));
            break;
        case SENSOR_FEATURE_SET_TEST_PATTERN:
            set_test_pattern_mode((BOOL)*feature_data);
            break;
        case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing
            *feature_return_para_32 = imgsensor_info.checksum_value;
            *feature_para_len=4;
            break;
        case SENSOR_FEATURE_SET_FRAMERATE:
            LOG_INF("current fps :%d\n", (UINT32)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.current_fps = *feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;
        case SENSOR_FEATURE_SET_SENSOR_OTP_AWB_CMD:
            LOG_INF("Update sensor awb from otp :%d\n", (BOOL)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.update_sensor_otp_awb = (BOOL)*feature_data;
            spin_unlock(&imgsensor_drv_lock);
            if(0 != imgsensor.update_sensor_otp_awb || 0 != imgsensor.update_sensor_otp_lsc) {
                otp_update(imgsensor.update_sensor_otp_awb, imgsensor.update_sensor_otp_lsc);
            }
            break;
        case SENSOR_FEATURE_SET_SENSOR_OTP_LSC_CMD:
            LOG_INF("Update sensor lsc from otp :%d\n", (BOOL)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.update_sensor_otp_lsc = (BOOL)*feature_data;
            spin_unlock(&imgsensor_drv_lock);
            if(0 != imgsensor.update_sensor_otp_awb || 0 != imgsensor.update_sensor_otp_lsc) {
                otp_update(imgsensor.update_sensor_otp_awb, imgsensor.update_sensor_otp_lsc);
            }
            break;
        case SENSOR_FEATURE_SET_HDR:
            LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
            spin_lock(&imgsensor_drv_lock);
            imgsensor.ihdr_en = (BOOL)*feature_data;
            spin_unlock(&imgsensor_drv_lock);
            break;
        case SENSOR_FEATURE_GET_CROP_INFO:
            LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", (UINT32)*feature_data);

            wininfo = (SENSOR_WINSIZE_INFO_STRUCT *)(uintptr_t)(*(feature_data+1));

            switch (*feature_data_32) {
                case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[1],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[2],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[3],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_SLIM_VIDEO:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[4],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
                case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
                default:
                    memcpy((void *)wininfo,(void *)&imgsensor_winsize_info[0],sizeof(SENSOR_WINSIZE_INFO_STRUCT));
                    break;
            }
        case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
            LOG_INF("SENSOR_SET_SENSOR_IHDR LE=%d, SE=%d, Gain=%d\n",(UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            ihdr_write_shutter_gain((UINT16)*feature_data,(UINT16)*(feature_data+1),(UINT16)*(feature_data+2));
            break;
        default:
            break;
    }

    return ERROR_NONE;
}   /*  feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
    open,
    get_info,
    get_resolution,
    feature_control,
    control,
    close
};

UINT32 IMX135_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&sensor_func;
    return ERROR_NONE;
}   /*  IMX135_MIPI_RAW_SensorInit  */
