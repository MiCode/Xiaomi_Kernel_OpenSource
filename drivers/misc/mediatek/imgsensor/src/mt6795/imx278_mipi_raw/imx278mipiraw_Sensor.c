/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 IMX278mipi_Sensor.c
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

#include "imx278mipiraw_Sensor.h"

/****************************Modify following Strings for debug****************************/
#define PFX "IMX278_camera_sensor"

#define LOG_1 LOG_INF("IMX278,MIPI 4LANE\n")
#define LOG_2 LOG_INF("preview 2100*1560@30fps,864Mbps/lane; video 4208*3120@30fps,864Mbps/lane; capture 13M@30fps,864Mbps/lane\n")
/****************************   Modify end    *******************************************/

#define LOG_INF(format, args...)    xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define MIPI_SETTLEDELAY_AUTO     0
#define MIPI_SETTLEDELAY_MANNUAL  1

static imgsensor_info_struct imgsensor_info = { 
    .sensor_id = IMX278_SENSOR_ID,

    //.checksum_value = 0x215125a0,
    //.checksum_value = 0xee2ea559,
    .checksum_value = 0x6b3a163d,
    .pre = {
        .pclk = 244000000,              //record different mode's pclk
        .linelength = 4976,             //record different mode's linelength
        .framelength = 1620,            //record different mode's framelength
        .startx = 0,                    //record different mode's startx of grabwindow
        .starty = 0,                    //record different mode's starty of grabwindow
		.grabwindow_width = 2100,//2096,		//record different mode's width of grabwindow
		.grabwindow_height = 1560,//1552,		//record different mode's height of grabwindow
        /*   following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario   */
        .mipi_data_lp2hs_settle_dc = 85,
        /*   following for GetDefaultFramerateByScenario()  */
        .max_framerate = 300,   
    },
    .cap = {   // 30  fps  capture
        .pclk = 480000000,
        .linelength = 4976,
        .framelength = 3220,
        .startx = 0,
        .starty = 0,
		.grabwindow_width = 4208,//4192,
		.grabwindow_height = 3120,//3104,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .cap1 = {    // 24 fps  capture
        .pclk = 388000000,
        .linelength = 4976,
        .framelength = 3220,
        .startx = 0,
        .starty = 0,
		.grabwindow_width = 4208,//4192,
		.grabwindow_height = 3120,//3104,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 240,   
    },
    .normal_video = {
        .pclk = 244000000,
        .linelength = 4976,
		.framelength = 1620,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2100,//4192,
		.grabwindow_height = 1560,//3104,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .hs_video = {     // 120 fps
        .pclk = 480000000,
        .linelength = 4976,
        .framelength = 800,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1316,
        .grabwindow_height = 480,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 1200,
    },
    .slim_video = {
        .pclk = 184000000,//231270000,
        .linelength = 4976,
        .framelength = 1228,//1640,
        .startx = 0,
        .starty = 0,
        .grabwindow_width = 1400,
        .grabwindow_height = 782,
        .mipi_data_lp2hs_settle_dc = 85,
        .max_framerate = 300,
    },
    .margin = 10,
    .min_shutter = 1,
    .max_frame_length = 0xffff,
    .ae_shut_delay_frame = 0,
    .ae_sensor_gain_delay_frame = 0,
    .ae_ispGain_delay_frame = 2,
    .ihdr_support = 0,    //1, support; 0,not support
    .ihdr_le_firstline = 0,  //1,le first ; 0, se first
    .sensor_mode_num = 10,   //support sensor mode num
    
    .cap_delay_frame = 2, 
    .pre_delay_frame = 2, 
    .video_delay_frame = 5,
    .hs_video_delay_frame = 5,
    .slim_video_delay_frame = 5,
    
    .isp_driving_current = ISP_DRIVING_8MA,
    .sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
    .mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2 
    .mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
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
    //.update_sensor_otp_awb = 0,
    //.update_sensor_otp_lsc = 0,
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[10] =    
{{ 4208, 3120, 0000, 0000, 4208, 3120, 2104, 1560, 0002, 0000, 2100, 1560, 0000, 0000, 2100, 1560}, // Preview 2112*1558
 { 4208, 3120, 0000, 0000, 4208, 3120, 4208, 3120, 0000, 0000, 4208, 3120, 0000, 0000, 4208, 3120}, // capture 4206*3128
 { 4208, 3120, 0000, 0000, 4208, 3120, 4208, 3120, 0000, 0000, 4208, 3120, 0000, 0000, 4208, 3120}, // video 
 { 4208, 3120, 0000, 792,  4208, 2328, 1320,  732,    2,    4, 1316,  480, 0000, 0000, 1316,  480}, //hight speed video 
 { 4208, 3120, 0000, 384,  4208, 2736, 1402,  782, 0002, 0000, 1400,  782, 0000, 0000, 1400,  782},// slim video
 };// slim video  
//#define IMX135_OTP_Enable 1

// Gain Index
#define MaxGainIndex (104)
static kal_uint16 sensorGainMapping[MaxGainIndex][2] ={
	{71 , 53 },//1.12
	{72 , 57 },//1.13
	{73 , 65 },//1.15
	{74 , 69 },//1.16
	{75 , 73 },//1.17
	{76 , 80 },//1.19
	{78 , 91 },//1.22
	{79 , 98 },//1.24
	{80 , 101},//1.25
	{81 , 108},//1.27
	{82 , 111},//1.28
	{83 , 117},
	{84 , 120},
	{85 , 126},
	{86 , 132},
	{89 , 143},
	{90 , 148},
	{94 , 163},
	{96 , 170},
	{99 , 181},
	{102, 191},
	{108, 207},
	{113, 222},
	{116, 230},
	{117, 232},
	{119, 236},
	{120, 239},
	{122, 244},
	{123, 245},
	{124, 248},
	{125, 249},
	{129, 258},
	{131, 262},
	{133, 266},
	{134, 268},
	{135, 269},
	{136, 272},
	{142, 281},
	{143, 282},
	{144, 284},
	{145, 286},
	{146, 288},
	{147, 288},
	{148, 290},
	{149, 292},
	{150, 294},
	{151, 295},
	{152, 297},
	{153, 299},
	{154, 300},
	{155, 301},
	{156, 302},
	{157, 304},
	{160, 307},
	{169, 318},
	{175, 325},
	{180, 330},
	{185, 335},
	{193, 342},
	{197, 346},
	{198, 247},
	{206, 353},
	{208, 355},
	{216, 360},
	{221, 364},
	{223, 365},
	{228, 368},
	{231, 370},
	{235, 373},
	{241, 376},
	{243, 377},
	{250, 381},
	{254, 383},
	{264, 388},
	{266, 389},
	{270, 391},
	{275, 393},
	{280, 395},
	{284, 397},
	{290, 399},
	{298, 402},
	{300, 403},
	{306, 405},
	{312, 407},
	{318, 409},
	{324, 411},
	{330, 413},
	{337, 415},
	{345, 417},
	{352, 419},
	{360, 421},
	{368, 423},
	{377, 425},
	{385, 427},
	{394, 429},
	{404, 431},
	{414, 433},
	{425, 435},
	{437, 437},
	{448, 439},
	{468, 442},
	{475, 443},
	{489, 445},
	{504, 447}	
};

//extern bool otp_update(BYTE update_sensor_otp_awb, BYTE update_sensor_otp_lsc);



static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    
    kal_uint16 get_byte=0;

    char pu_send_cmd[2] = {(char)(addr >> 8), (char)(addr & 0xFF) };
    iReadRegI2C(pu_send_cmd, 2, (u8*)&get_byte, 1, imgsensor.i2c_write_id);

    return get_byte;
}

static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
        
    char pu_send_cmd[3] = {(char)(addr >> 8), (char)(addr & 0xFF), (char)(para & 0xFF)};
    iWriteRegI2C(pu_send_cmd, 3, imgsensor.i2c_write_id);
}

  

static void set_dummy()
{
    LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);

    write_cmos_sensor(0x0104, 0x01); 
	  write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
    write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);     
	  write_cmos_sensor(0x0342, (imgsensor.line_length >> 8) & 0xFF);
    write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);
    write_cmos_sensor(0x0104, 0x00); 
  
}   /*  set_dummy  */

static kal_uint32 return_sensor_id()
{   
  write_cmos_sensor(0x0A02, 0x13);//specify OTP Page Add for read
	write_cmos_sensor(0x0A00, 0x01);//Turn on OTP Read Mode
	write_cmos_sensor(0x0A01, 0x01);//check statu
	return (((read_cmos_sensor(0x0A28) << 8) | (read_cmos_sensor(0x0A29) & 0xF0) ) >> 4);
}

static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
    kal_int16 dummy_line;
    kal_uint32 frame_length = imgsensor.frame_length;
    //unsigned long flags;

    LOG_INF("framerate = %d, min framelength should enable? \n", framerate,min_framelength_en);
   
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
    set_dummy();
}   /*  set_max_framerate  */


static void write_shutter(kal_uint16 shutter)
{

   //shutter = 0x07D0;
   #if 1
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
    
    if (imgsensor.autoflicker_en) { 
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296,0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146,0);
        else {
        // Extend frame length
        write_cmos_sensor(0x0104, 0x01); 
		    write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
        write_cmos_sensor(0x0104, 0x00); 
            }
    } else {
        // Extend frame length
        write_cmos_sensor(0x0104, 0x01); 
		    write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8) & 0xFF);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
        write_cmos_sensor(0x0104, 0x00); 
    }

    // Update Shutter
    
    write_cmos_sensor(0x0104, 0x01); 
    write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0x0203, shutter  & 0xFF); 
    write_cmos_sensor(0x0104, 0x00); 
    LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
#endif
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

   //gain = 0x80;
#if 1
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

        write_cmos_sensor(0x0104, 0x01); 
        write_cmos_sensor(0x0204, (reg_gain>>8)& 0xFF);
        write_cmos_sensor(0x0205, reg_gain & 0xFF);
        write_cmos_sensor(0x0104, 0x00); 
    
    return gain;
	#endif
}   /*  set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
    kal_uint8 iRation;
    kal_uint8 iReg;

#if 0
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
        write_cmos_sensor(0x0104, 0x01); 
		    write_cmos_sensor(0x0340, (imgsensor.frame_length >> 8)& 0xFF);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);

        write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
        write_cmos_sensor(0x0203, le & 0xFF);    
        
        write_cmos_sensor(0x0230, (se >> 8) & 0xFF);
        write_cmos_sensor(0x0231, se & 0x0F); 
        
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
                write_cmos_sensor(0x0104, 0x00); 
        
    }

	#endif

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
			      write_cmos_sensor(0x0101, iTemp);    //Set normal
            break;
        case IMAGE_H_MIRROR:
            write_cmos_sensor(0x0101, iTemp | 0x01); //Set mirror
            break;
        case IMAGE_V_MIRROR:
            write_cmos_sensor(0x0101, iTemp | 0x02); //Set flip
            break;
        case IMAGE_HV_MIRROR:
            write_cmos_sensor(0x0101, iTemp | 0x03); //Set mirror and flip
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
	LOG_INF("Enter sensor_init,FOR TEST\n");

    write_cmos_sensor(0x0136, 0x18);//INCK
    write_cmos_sensor(0x0137, 0x00);//INCK
    write_cmos_sensor(0x3042, 0x01);//
    write_cmos_sensor(0x5CE8, 0x00);
	  write_cmos_sensor(0x5CE9, 0x91);
	  write_cmos_sensor(0x5CEA, 0x00);
	  write_cmos_sensor(0x5CEB, 0x2A);
	  write_cmos_sensor(0x5F1B, 0x01);
	  write_cmos_sensor(0x5C2C, 0x01);
	  write_cmos_sensor(0x5C2D, 0xFF);
	  write_cmos_sensor(0x5C2E, 0x00);
	  write_cmos_sensor(0x5C2F, 0x00);
    write_cmos_sensor(0x5F0D, 0x6E);//
    write_cmos_sensor(0x5F0E, 0x7C);//
    write_cmos_sensor(0x5F0F, 0x14);//
    write_cmos_sensor(0x6100, 0x30);//
    write_cmos_sensor(0x6101, 0x12);//
    write_cmos_sensor(0x6102, 0x14);//
    write_cmos_sensor(0x6104, 0x91);//
    write_cmos_sensor(0x6105, 0x30);//
    write_cmos_sensor(0x6106, 0x11);//
    write_cmos_sensor(0x6107, 0x12);//
    write_cmos_sensor(0x6505, 0xC4);//
    write_cmos_sensor(0x6507, 0x25);
    write_cmos_sensor(0x6508, 0xE1);//
    write_cmos_sensor(0x6509, 0xD7);//
    write_cmos_sensor(0x650A, 0x20);
    write_cmos_sensor(0x7382, 0x01);//
    write_cmos_sensor(0x7383, 0x13);//
    write_cmos_sensor(0x7788, 0x04);//
    write_cmos_sensor(0x9006, 0x0C);
    //write_cmos_sensor(0x9332, 0x02);//remove @4/22
    //write_cmos_sensor(0x9333, 0x02);//
    //write_cmos_sensor(0x9335, 0x02);//
    //write_cmos_sensor(0x9336, 0x02);//
    //write_cmos_sensor(0x9845, 0x0F);//
    //write_cmos_sensor(0x989E, 0x16);//
    //write_cmos_sensor(0x9921, 0x0A);//
    
    write_cmos_sensor(0xB000,0x78);
	  write_cmos_sensor(0xB001,0xB6);
	  write_cmos_sensor(0xB002,0x78);
	  write_cmos_sensor(0xB003,0xB7);
	  write_cmos_sensor(0xB004,0x98);
	  write_cmos_sensor(0xB005,0x00);
	  write_cmos_sensor(0xB006,0x98);
	  write_cmos_sensor(0xB007,0x01);
	  write_cmos_sensor(0xB008,0x98);
   	write_cmos_sensor(0xB009,0x02);
	  write_cmos_sensor(0xB00A,0xA9);
	  write_cmos_sensor(0xB00B,0x0F);
	  write_cmos_sensor(0xB00C,0xA9);
	  write_cmos_sensor(0xB00D,0x12);
	  write_cmos_sensor(0xB00E,0xA9);
	  write_cmos_sensor(0xB00F,0x15);
	  write_cmos_sensor(0xB010,0xA9);
	  write_cmos_sensor(0xB011,0x16);
	  write_cmos_sensor(0xB012,0xA9);
	  write_cmos_sensor(0xB013,0x17);
	  write_cmos_sensor(0xB014,0xA9);
	  write_cmos_sensor(0xB015,0x18);
	  write_cmos_sensor(0xB016,0xA9);
	  write_cmos_sensor(0xB017,0x19);
  #if 1
	 //Image quality
    //Lens shading correction
	 write_cmos_sensor(0x3020, 0x02);
		 
	//Defect Pixel Correction	 
	 write_cmos_sensor(0x73A0, 0x21);
	 write_cmos_sensor(0x73A1, 0x03);
	 write_cmos_sensor(0x7709, 0x05);
	 write_cmos_sensor(0x7786, 0x09);
	 write_cmos_sensor(0x778A, 0x00);
	 write_cmos_sensor(0x778B, 0x60);
	 write_cmos_sensor(0x951E, 0x3D);
	 write_cmos_sensor(0x951F, 0x05);
	 write_cmos_sensor(0x9521, 0x12);
	 write_cmos_sensor(0x9522, 0x02);
	 write_cmos_sensor(0x9524, 0x00);
	 write_cmos_sensor(0x9525, 0x31);
	 write_cmos_sensor(0x9526, 0x00);
	 write_cmos_sensor(0x9527, 0x04);
	 write_cmos_sensor(0x952A, 0x00);
	 write_cmos_sensor(0x952B, 0x7A);
	 write_cmos_sensor(0x952C, 0x00);
	 write_cmos_sensor(0x952D, 0x0B);
	 //new add
	 write_cmos_sensor(0x9606, 0x00);
	 write_cmos_sensor(0x9607, 0x90);
	 
	 write_cmos_sensor(0x9618, 0x00);
	 write_cmos_sensor(0x9619, 0x52);
	 write_cmos_sensor(0x961E, 0x00);
	 write_cmos_sensor(0x961F, 0x1E);
	 //new add reg 
	 write_cmos_sensor(0x9620, 0x00);
	 write_cmos_sensor(0x9621, 0x18);
	 write_cmos_sensor(0x9622, 0x00);
	 write_cmos_sensor(0x9623, 0x18);
	 write_cmos_sensor(0x9706, 0x00);
	 write_cmos_sensor(0x9707, 0x90);
	 
	 write_cmos_sensor(0x9718, 0x00);
	 write_cmos_sensor(0x9719, 0x32);
	 //add @4/22
	 write_cmos_sensor(0x971A, 0x00);
	 write_cmos_sensor(0x971B, 0x32);
	 write_cmos_sensor(0x971C, 0x00);
	 write_cmos_sensor(0x971D, 0x32);
	 
	 write_cmos_sensor(0x971E, 0x00);
	 write_cmos_sensor(0x971F, 0x1E);
	 //add @4/22
	 write_cmos_sensor(0x9720, 0x00);
	 write_cmos_sensor(0x9721, 0x20);
	 write_cmos_sensor(0x9722, 0x00);
	 write_cmos_sensor(0x9723, 0x20);
	  
	//White remosaic setting	 

	 write_cmos_sensor(0x7837, 0x00);
	 write_cmos_sensor(0x7965, 0x0F);
	 write_cmos_sensor(0x7966, 0x0A);
	 write_cmos_sensor(0x7967, 0x07);
	 write_cmos_sensor(0x7968, 0x0F);
	 write_cmos_sensor(0x7969, 0x60);
	 write_cmos_sensor(0x796B, 0x32);
	 write_cmos_sensor(0xA85B, 0x10);
	 write_cmos_sensor(0x924E, 0x42);
	 
	 //new add
	 write_cmos_sensor(0x9250, 0x78);
	 write_cmos_sensor(0x9251, 0x3C);
	 write_cmos_sensor(0x9252, 0x14);
	 //add @4/22
	 write_cmos_sensor(0x9332, 0x02);
	 write_cmos_sensor(0x9333, 0x02);
	 write_cmos_sensor(0x9335, 0x02);
	 write_cmos_sensor(0x9336, 0x02);
	 write_cmos_sensor(0x9357, 0x04);
	 write_cmos_sensor(0x9359, 0x04);
	 write_cmos_sensor(0x935A, 0x04);
	 
	 write_cmos_sensor(0x9809, 0x02);
	 write_cmos_sensor(0x980A, 0x02);
	 write_cmos_sensor(0x980B, 0x02);
	 write_cmos_sensor(0x980D, 0x00);
	 write_cmos_sensor(0x980E, 0x00);
	 write_cmos_sensor(0x980F, 0x06);
	 write_cmos_sensor(0x9812, 0x00);
	 write_cmos_sensor(0x9813, 0x00);
	 write_cmos_sensor(0x9814, 0x00);
	 write_cmos_sensor(0x981B, 0x1E);
	 write_cmos_sensor(0x981C, 0x23);
	 write_cmos_sensor(0x981D, 0x23);
	 write_cmos_sensor(0x981E, 0x28);
	 write_cmos_sensor(0x981F, 0x55);
	 write_cmos_sensor(0x9820, 0x55);
	 write_cmos_sensor(0x9822, 0x1B);
	 write_cmos_sensor(0x9823, 0x1B);
	 write_cmos_sensor(0x9824, 0x0A);
	 write_cmos_sensor(0x9825, 0x00);
	 write_cmos_sensor(0x9826, 0x00);
	 write_cmos_sensor(0x9827, 0x69);
	 write_cmos_sensor(0x9828, 0xA0);
	 write_cmos_sensor(0x9829, 0xA0);
	 write_cmos_sensor(0x982A, 0x00);
	 write_cmos_sensor(0x982B, 0x80);
	 write_cmos_sensor(0x982C, 0x00);
	 write_cmos_sensor(0x982D, 0x8C);
	 write_cmos_sensor(0x982E, 0x00);
	 write_cmos_sensor(0x982F, 0x8C);
	 write_cmos_sensor(0x9830, 0x04);
	 write_cmos_sensor(0x9831, 0x80);
	 write_cmos_sensor(0x9832, 0x05);
	 write_cmos_sensor(0x9833, 0x00);
	 write_cmos_sensor(0x9834, 0x05);
	 write_cmos_sensor(0x9835, 0x00);
	 write_cmos_sensor(0x9836, 0x00);
	 write_cmos_sensor(0x9837, 0x80);
	 write_cmos_sensor(0x9838, 0x00);
	 write_cmos_sensor(0x9839, 0x80);
	 write_cmos_sensor(0x983A, 0x00);
	 write_cmos_sensor(0x983B, 0x80);
	 write_cmos_sensor(0x983C, 0x0E);
   write_cmos_sensor(0x983D, 0x01);
	 write_cmos_sensor(0x983E, 0x01);
	 write_cmos_sensor(0x983F, 0x0E);
	 //new add reg
	 write_cmos_sensor(0x9840, 0x06); 
	 write_cmos_sensor(0x9845, 0x0E);
	 write_cmos_sensor(0x9846, 0x00);
	 
	 
	 write_cmos_sensor(0x9848, 0x0E);
	 write_cmos_sensor(0x9849, 0x06);
	 write_cmos_sensor(0x984A, 0x06);
	 //new add reg
	 write_cmos_sensor(0x9871, 0x14);
	 write_cmos_sensor(0x9872, 0x0E);
	 
	 write_cmos_sensor(0x9877, 0x7F);
	 write_cmos_sensor(0x9878, 0x1E);
	 write_cmos_sensor(0x9879, 0x09);
	 write_cmos_sensor(0x987B, 0x0E);
	 write_cmos_sensor(0x987C, 0x0E);
	 write_cmos_sensor(0x988A, 0x13);
	 write_cmos_sensor(0x988B, 0x13);
	 write_cmos_sensor(0x9893, 0x13);
	 write_cmos_sensor(0x9894, 0x13);
	 write_cmos_sensor(0x9898, 0x75);
	 write_cmos_sensor(0x9899, 0x2D);
	 write_cmos_sensor(0x989A, 0x26);
	 write_cmos_sensor(0x989E, 0x96);
	 write_cmos_sensor(0x989F, 0x26);
	 write_cmos_sensor(0x98A0, 0x0D);
	 write_cmos_sensor(0x98A1, 0x43);
	 write_cmos_sensor(0x98A2, 0x0E);
	 write_cmos_sensor(0x98A3, 0x03);
	 write_cmos_sensor(0x98AB, 0x66);
	 write_cmos_sensor(0x98AC, 0x66);
	 write_cmos_sensor(0x98B1, 0x4D);
	 write_cmos_sensor(0x98B2, 0x4D);
	 write_cmos_sensor(0x98B4, 0x0D);
	 write_cmos_sensor(0x98B5, 0x0D);
	 write_cmos_sensor(0x98BC, 0x7A);
	 write_cmos_sensor(0x98BD, 0x66);//
	 write_cmos_sensor(0x98BE, 0x78);
	 write_cmos_sensor(0x98C2, 0x66);
	 write_cmos_sensor(0x98C3, 0x62);
	 write_cmos_sensor(0x98C4, 0x62);
	 write_cmos_sensor(0x98C6, 0x14);//
	 write_cmos_sensor(0x98CE, 0x7A);
	 write_cmos_sensor(0x98CF, 0x78);
	 write_cmos_sensor(0x98D0, 0x78);
	 write_cmos_sensor(0x98D4, 0x66);
	 write_cmos_sensor(0x98D5, 0x62);
	 write_cmos_sensor(0x98D6, 0x62);
	 write_cmos_sensor(0x9921, 0x0A);//
	 write_cmos_sensor(0x9922, 0x01);
	 write_cmos_sensor(0x9923, 0x01);
	 write_cmos_sensor(0x9928, 0xA0);
	 write_cmos_sensor(0x9929, 0xA0);
	 write_cmos_sensor(0x9949, 0x06);
	 write_cmos_sensor(0x994A, 0x06);
	 write_cmos_sensor(0x9999, 0x26);
	 write_cmos_sensor(0x999A, 0x26);
	 write_cmos_sensor(0x999F, 0x0D);
	 write_cmos_sensor(0x99A0, 0x0D);
	 write_cmos_sensor(0x99A2, 0x03);
	 write_cmos_sensor(0x99A3, 0x03);
	 write_cmos_sensor(0x99BD, 0x78);
	 write_cmos_sensor(0x99BE, 0x78);
	 write_cmos_sensor(0x99C3, 0x62);
	 write_cmos_sensor(0x99C4, 0x62);
	 write_cmos_sensor(0x99CF, 0x78);
	 write_cmos_sensor(0x99D0, 0x78);
	 write_cmos_sensor(0x99D5, 0x62);
	 write_cmos_sensor(0x99D6, 0x62);
	 write_cmos_sensor(0xA900, 0x00);
	 write_cmos_sensor(0xA901, 0x00);
	 write_cmos_sensor(0xA90B, 0x00);
	 write_cmos_sensor(0x9342, 0x04);
	 write_cmos_sensor(0x934D, 0x04);
	 write_cmos_sensor(0x934F, 0x04);
	 write_cmos_sensor(0x9350, 0x04);
	 
 
//White Sensitive Rate setting
		 
	write_cmos_sensor(0x3011, 0x93);
	write_cmos_sensor(0xAF00, 0x01);
	write_cmos_sensor(0xAF01, 0x00);
	write_cmos_sensor(0xAF02, 0x00);
	write_cmos_sensor(0xAF03, 0xDB);
	write_cmos_sensor(0xAF04, 0x01);
	write_cmos_sensor(0xAF05, 0x00);
	write_cmos_sensor(0xAF06, 0x01);
	write_cmos_sensor(0xAF07, 0xD2);
	write_cmos_sensor(0xAF08, 0x02);
	write_cmos_sensor(0xAF09, 0x3D);
	write_cmos_sensor(0xAF0A, 0x02);
	write_cmos_sensor(0xAF0B, 0x83);
#endif
LOG_INF("Exit sensor_init\n");

}   /*  sensor_init  */



static void preview_setting(void)   //PreviewSetting
{

  #if 1
   LOG_INF("Enter preview_setting\n");
   //kal_uint32 p1,p2;

    //Preview_1/2bin_avg_vblank1.18ms_30.26fps MIPI610Mbps 
    write_cmos_sensor(0x0100,0x00);// STREAM STop
    //p1 = read_cmos_sensor_8(0x0100);
    //LOG_INF("preview setting 0x0100 = %d\n",p1);

    //write_cmos_sensor(0x3000, 0x00);//  Manufacturer specified register
   // write_cmos_sensor(0x3001, 0x00);// 
    //Output format Setting
    write_cmos_sensor(0x0112, 0x0A);//   data format 0A0A:RAW10
    write_cmos_sensor(0x0113, 0x0A);//   
    write_cmos_sensor(0x0114, 0x03);//   CSI lane number
    //Clock Setting
    write_cmos_sensor(0x0301, 0x05);//   IVTPXCK_DIV
    write_cmos_sensor(0x0303, 0x02);//   IVTSYCK_DIV
    write_cmos_sensor(0x0305, 0x0C);//   PREPLLCK_IVT_DIV
    write_cmos_sensor(0x0306, 0x01);//   PLL_IVT_MPY 
    write_cmos_sensor(0x0307, 0x31);//   PLL_IVT_MPY
    write_cmos_sensor(0x0309, 0x0A);//   IOPPXCK_DIV
    write_cmos_sensor(0x030B, 0x01);//   IOPSYCK_DIV
    write_cmos_sensor(0x030D, 0x0C);//   PREPLLCK_IOP_DIV
    write_cmos_sensor(0x030E, 0x05);//   PLL_IOP_MPY[10:8]
    write_cmos_sensor(0x030F, 0x14);//   PLL_IOP_MPY[7:0]
    write_cmos_sensor(0x0310, 0x00);//   PLL_MULT_DIV
    write_cmos_sensor(0x3041, 0x01);//   ?

    //linelength,framelength setting
    //mdelay(35);
    write_cmos_sensor(0x0342, 0x13);//   
    write_cmos_sensor(0x0343, 0x70);//   
    write_cmos_sensor(0x0340, 0x06);//   
    write_cmos_sensor(0x0341, 0x54);// 
    //ROI setting
    write_cmos_sensor(0x0344, 0x00);// x_addr_start
    write_cmos_sensor(0x0345, 0x00);// x_addr_start
    write_cmos_sensor(0x0346, 0x00);// y_addr_start
    write_cmos_sensor(0x0347, 0x00);// y_addr_start
    write_cmos_sensor(0x0348, 0x10);//  x_addr_end
    write_cmos_sensor(0x0349, 0x6F);//  x_addr_end
    write_cmos_sensor(0x034A, 0x0C);//  y_addr_end
    write_cmos_sensor(0x034B, 0x2F);// y_addr_end
	//Analog Image Size Setting
    write_cmos_sensor(0x0381, 0x01); //x_even_inc 
    write_cmos_sensor(0x0383, 0x01); //x_odd_inc
    write_cmos_sensor(0x0385, 0x01); 
    write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);//Binning mode
	write_cmos_sensor(0x0901, 0x12);//Binning type[7:4]H,[3:0]V
	//Digital Image Size Setting
	write_cmos_sensor(0x0401, 0x01);//H scaling
  write_cmos_sensor(0x0404, 0x00);//Scale factor[8]
	write_cmos_sensor(0x0405, 0x20);//SCALE_M[7:0]
	write_cmos_sensor(0x0408, 0x00);//DIG_CROP_X_OFFSET[12:8]
	write_cmos_sensor(0x0409, 0x02);//DIG_CROP_X_OFFSET[7:0]
	write_cmos_sensor(0x040A, 0x00);//DIG_CROP_Y_OFFSET[11:8]
	write_cmos_sensor(0x040B, 0x00);//DIG_CROP_Y_OFFSET[7:0]
	write_cmos_sensor(0x040C, 0x10);//DIG_CROP_IMAGE_WIDTH[12:8]
	write_cmos_sensor(0x040D, 0x6A);//DIG_CROP_IMAGE_WIDTH[7:0]
	write_cmos_sensor(0x040E, 0x06);//DIG_CROP_IMAGE_HEIGHT[11:8] *1
	write_cmos_sensor(0x040F, 0x18);//DIG_CROP_IMAGE_HEIGHT[7:0] *1
	write_cmos_sensor(0x3038, 0x00);//SCALE_MODE_EXT[0] 0:disable 1:enable
	write_cmos_sensor(0x303A, 0x00);//SCALE_M_EXT[8]
	write_cmos_sensor(0x303B, 0x10);//SCALE_M_EXT[7:0] H-direction : Scaling ratio Dedault
   //Output Size Setting
   write_cmos_sensor(0x034C, 0x08);//X_OUT_SIZE[12:8]
   write_cmos_sensor(0x034D, 0x34);//X_OUT_SIZE[7:0]
   write_cmos_sensor(0x034E, 0x06);//Y_OUT_SIZE[11:8]
   write_cmos_sensor(0x034F, 0x18);//Y_OUT_SIZE[7:0]
   //White remosaic setting 

   write_cmos_sensor(0x3029, 0x01);
   write_cmos_sensor(0x3A00, 0x01);
   write_cmos_sensor(0x3A01, 0x01);
   write_cmos_sensor(0x3A02, 0x05);
   write_cmos_sensor(0x3A03, 0x05);
   write_cmos_sensor(0x3A04, 0x05);
   write_cmos_sensor(0x3A05, 0xF8);
   write_cmos_sensor(0x3A06, 0x40);
   write_cmos_sensor(0x3A07, 0xFE);
   write_cmos_sensor(0x3A08, 0x10);
   write_cmos_sensor(0x3A09, 0x14);
   write_cmos_sensor(0x3A0A, 0xFE);
   write_cmos_sensor(0x3A0B, 0x44);
   //Integration Time Setting
   write_cmos_sensor(0x0202, 0x06);
   write_cmos_sensor(0x0203, 0x4A);
   //Gain setting
   
   write_cmos_sensor(0x0204, 0x00);
   write_cmos_sensor(0x0205, 0x00);
   write_cmos_sensor(0x020E, 0x01);
   write_cmos_sensor(0x020F, 0x00);

   write_cmos_sensor(0x0100,0x01);// STREAM START
    //p2 = read_cmos_sensor_8(0x0100);
    //LOG_INF("preview setting 0x0100 = %d\n",p2);
   LOG_INF("Exit preview_setting\n");
#endif
}   /*  preview_setting  */

static void capture_setting(kal_uint16 currefps)  // IMX135MIPI_set_13M
{


    LOG_INF("E! currefps:%d\n",currefps);
	LOG_INF("Enter capture_setting\n");
	//Full_29.95fps_vblank1.02ms MIPI 1200Mbps
    write_cmos_sensor(0x0100, 0x00);// STREAM STop
    //Preset Setting
    //write_cmos_sensor(0x3000, 0x00);//   
    //write_cmos_sensor(0x3001, 0x00);//   
  
    //Output format Setting
    write_cmos_sensor(0x0112, 0x0A);//   
    write_cmos_sensor(0x0113, 0x0A);//   
    write_cmos_sensor(0x0114, 0x03);//   
    //Clock Setting
    write_cmos_sensor(0x0301, 0x05);//   
    write_cmos_sensor(0x0303, 0x02);//   
    write_cmos_sensor(0x0305, 0x0C);//   
    write_cmos_sensor(0x0306, 0x02);//   PLL
    write_cmos_sensor(0x0307, 0x58);//   
    write_cmos_sensor(0x0309, 0x0A);//   
    write_cmos_sensor(0x030B, 0x01);// 
    write_cmos_sensor(0x030D, 0x0C);// 
    write_cmos_sensor(0x030E, 0x05);// 
    write_cmos_sensor(0x030F, 0x14);// 
    write_cmos_sensor(0x0310, 0x00);// PLL mode
    write_cmos_sensor(0x3041, 0x01);// 

    //linelength,framelength setting
    //mdelay(35);
    write_cmos_sensor(0x0342, 0x13);//   
    write_cmos_sensor(0x0343, 0x70);//   
    write_cmos_sensor(0x0340, 0x0C);//   
    write_cmos_sensor(0x0341, 0x94);// 
    //ROI setting
    write_cmos_sensor(0x0344, 0x00);
    write_cmos_sensor(0x0345, 0x00);  
    write_cmos_sensor(0x0346, 0x00); 
    write_cmos_sensor(0x0347, 0x00);  
    write_cmos_sensor(0x0348, 0x10);  
    write_cmos_sensor(0x0349, 0x6F);  
    write_cmos_sensor(0x034A, 0x0C);  
    write_cmos_sensor(0x034B, 0x2F); 
	//Analog Image Size Setting
    write_cmos_sensor(0x0381, 0x01);  
    write_cmos_sensor(0x0383, 0x01); 
    write_cmos_sensor(0x0385, 0x01); 
    write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x00);
	write_cmos_sensor(0x0901, 0x11);
	//Digital Image Size Setting
	write_cmos_sensor(0x0401, 0x00);
    write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x10);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x00);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x040C, 0x10);
	write_cmos_sensor(0x040D, 0x70);
	write_cmos_sensor(0x040E, 0x0C);
	write_cmos_sensor(0x040F, 0x30);
	write_cmos_sensor(0x3038, 0x00);
	write_cmos_sensor(0x303A, 0x00);
	write_cmos_sensor(0x303B, 0x10);
   //Output Size Setting
   write_cmos_sensor(0x034C, 0x10);
   write_cmos_sensor(0x034D, 0x70);//4208
   write_cmos_sensor(0x034E, 0x0C);
   write_cmos_sensor(0x034F, 0x30);//3120
   //White remosaic setting 

   write_cmos_sensor(0x3029, 0x00);
   write_cmos_sensor(0x3A00, 0x00);
   write_cmos_sensor(0x3A01, 0x00);
   write_cmos_sensor(0x3A02, 0x05);
   write_cmos_sensor(0x3A03, 0x05);
   write_cmos_sensor(0x3A04, 0x05);
   write_cmos_sensor(0x3A05, 0xF8);
   write_cmos_sensor(0x3A06, 0x40);
   write_cmos_sensor(0x3A07, 0xFE);
   write_cmos_sensor(0x3A08, 0x10);
   write_cmos_sensor(0x3A09, 0x14);
   write_cmos_sensor(0x3A0A, 0xFE);
   write_cmos_sensor(0x3A0B, 0x44);
   //Integration Time Setting
   write_cmos_sensor(0x0202, 0x0C);
   write_cmos_sensor(0x0203, 0x8A);
   //Gain setting
   
   write_cmos_sensor(0x0204, 0x00);
   write_cmos_sensor(0x0205, 0x00);
   write_cmos_sensor(0x020E, 0x01);
   write_cmos_sensor(0x020F, 0x00);
   write_cmos_sensor(0x0100, 0x01);//STREAM START
   
   LOG_INF("Exit capture_setting\n");
        
}

static void normal_video_setting(kal_uint16 currefps)    // VideoFullSizeSetting
{
	
	preview_setting();
   #if 0
   LOG_INF("Enter normal_video_setting_samewith_capture\n");
   //4k2k_30.06fps_vblank1.22ms
   write_cmos_sensor(0x0100, 0x00);// STREAM STop
   //Preset Setting
   //write_cmos_sensor(0x3000, 0x00);//   
   //write_cmos_sensor(0x3001, 0x00);//   
 
   //Output format Setting
   write_cmos_sensor(0x0112, 0x0A);//	
   write_cmos_sensor(0x0113, 0x0A);//	
   write_cmos_sensor(0x0114, 0x03);//	
   //Clock Setting
   write_cmos_sensor(0x0301, 0x05);//	
   write_cmos_sensor(0x0303, 0x02);//	
   write_cmos_sensor(0x0305, 0x0C);//	
   write_cmos_sensor(0x0306, 0x01);//	PLL
   write_cmos_sensor(0x0307, 0xCC);//	
   write_cmos_sensor(0x0309, 0x0A);//	
   write_cmos_sensor(0x030B, 0x01);// 
   write_cmos_sensor(0x030D, 0x0C);// 
   write_cmos_sensor(0x030E, 0x05);// 
   write_cmos_sensor(0x030F, 0x14);// 
   write_cmos_sensor(0x0310, 0x00);// PLL mode
   write_cmos_sensor(0x3041, 0x01);// 

   //linelength,framelength setting
   //mdelay(35);
   write_cmos_sensor(0x0342, 0x13);//	
   write_cmos_sensor(0x0343, 0x70);//	
   write_cmos_sensor(0x0340, 0x09);//	
   write_cmos_sensor(0x0341, 0x9C);// 
   //ROI setting
   write_cmos_sensor(0x0344, 0x00);
   write_cmos_sensor(0x0345, 0x00);  
   write_cmos_sensor(0x0346, 0x01); 
   write_cmos_sensor(0x0347, 0x78);  
   write_cmos_sensor(0x0348, 0x10);  
   write_cmos_sensor(0x0349, 0x6F);  
   write_cmos_sensor(0x034A, 0x0A);  
   write_cmos_sensor(0x034B, 0xB7); 
   //Analog Image Size Setting
   write_cmos_sensor(0x0381, 0x01);  
   write_cmos_sensor(0x0383, 0x01); 
   write_cmos_sensor(0x0385, 0x01); 
   write_cmos_sensor(0x0387, 0x01);
   write_cmos_sensor(0x0900, 0x00);
   write_cmos_sensor(0x0901, 0x11);
   //Digital Image Size Setting
   write_cmos_sensor(0x0401, 0x00);
   write_cmos_sensor(0x0404, 0x00);
   write_cmos_sensor(0x0405, 0x10);
   write_cmos_sensor(0x0408, 0x00);
   write_cmos_sensor(0x0409, 0x00);
   write_cmos_sensor(0x040A, 0x00);
   write_cmos_sensor(0x040B, 0x00);
   write_cmos_sensor(0x040C, 0x10);
   write_cmos_sensor(0x040D, 0x70);
   write_cmos_sensor(0x040E, 0x09);
   write_cmos_sensor(0x040F, 0x40);
   write_cmos_sensor(0x3038, 0x00);
   write_cmos_sensor(0x303A, 0x00);
   write_cmos_sensor(0x303B, 0x10);
  //Output Size Setting
  write_cmos_sensor(0x034C, 0x10);
  write_cmos_sensor(0x034D, 0x70);//
  write_cmos_sensor(0x034E, 0x09);
  write_cmos_sensor(0x034F, 0x40);//
  //White remosaic setting 

  write_cmos_sensor(0x3029, 0x00);
  write_cmos_sensor(0x3A00, 0x00);
  write_cmos_sensor(0x3A01, 0x00);
  write_cmos_sensor(0x3A02, 0x05);
  write_cmos_sensor(0x3A03, 0x05);
  write_cmos_sensor(0x3A04, 0x05);
  write_cmos_sensor(0x3A05, 0xF8);
  write_cmos_sensor(0x3A06, 0x40);
  write_cmos_sensor(0x3A07, 0xFE);
  write_cmos_sensor(0x3A08, 0x10);
  write_cmos_sensor(0x3A09, 0x14);
  write_cmos_sensor(0x3A0A, 0xFE);
  write_cmos_sensor(0x3A0B, 0x44);
  //Integration Time Setting
  write_cmos_sensor(0x0202, 0x09);
  write_cmos_sensor(0x0203, 0x92);
  //Gain setting
  
  write_cmos_sensor(0x0204, 0x00);
  write_cmos_sensor(0x0205, 0x00);
  write_cmos_sensor(0x020E, 0x01);
  write_cmos_sensor(0x020F, 0x00);
  write_cmos_sensor(0x0100, 0x01);//STREAM START
#endif
        
}
static void hs_video_setting()  // VideoHDSetting_120fps
{
    LOG_INF("E\n  hs_Video  120fps ");
    //PLL setting 
    write_cmos_sensor(0x0100, 0x00);// STREAM STop
    //Preset Setting
    //write_cmos_sensor(0x3000, 0x00);//   
    //write_cmos_sensor(0x3001, 0x00);//   
    //Output format Setting
    write_cmos_sensor(0x0112, 0x0A);//   
    write_cmos_sensor(0x0113, 0x0A);//   
    write_cmos_sensor(0x0114, 0x03);//   
    //Clock Setting
    write_cmos_sensor(0x0301, 0x05);//   
    write_cmos_sensor(0x0303, 0x02);//   
    write_cmos_sensor(0x0305, 0x0C);//   
    write_cmos_sensor(0x0306, 0x02);//   
    write_cmos_sensor(0x0307, 0x58);//   
    write_cmos_sensor(0x0309, 0x0A);//   
    write_cmos_sensor(0x030B, 0x01);// 
    write_cmos_sensor(0x030D, 0x0C);// 
    write_cmos_sensor(0x030E, 0x05);// 
    write_cmos_sensor(0x030F, 0x14);// 
    write_cmos_sensor(0x0310, 0x00);// 
    write_cmos_sensor(0x3041, 0x01);// 

    //linelength,framelength setting
    //mdelay(35);
    write_cmos_sensor(0x0342, 0x13);//   
    write_cmos_sensor(0x0343, 0x70);//   
    write_cmos_sensor(0x0340, 0x03);//   
    write_cmos_sensor(0x0341, 0x20);// 
    //ROI setting
    write_cmos_sensor(0x0344, 0x00);
    write_cmos_sensor(0x0345, 0x00);  
    write_cmos_sensor(0x0346, 0x03); 
    write_cmos_sensor(0x0347, 0x18);  
    write_cmos_sensor(0x0348, 0x10);  
    write_cmos_sensor(0x0349, 0x6F);  
    write_cmos_sensor(0x034A, 0x09);  
    write_cmos_sensor(0x034B, 0x17); 
	//Analog Image Size Setting
    write_cmos_sensor(0x0381, 0x01);  
    write_cmos_sensor(0x0383, 0x01); 
    write_cmos_sensor(0x0385, 0x01); 
    write_cmos_sensor(0x0387, 0x01);
	write_cmos_sensor(0x0900, 0x01);
	write_cmos_sensor(0x0901, 0x12);
	//Digital Image Size Setting
	write_cmos_sensor(0x0401, 0x02);
  write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x19);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x04);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x08);
	write_cmos_sensor(0x040C, 0x10);
	write_cmos_sensor(0x040D, 0x66);
	write_cmos_sensor(0x040E, 0x02);
	write_cmos_sensor(0x040F, 0xF0);
	write_cmos_sensor(0x3038, 0x01);
	write_cmos_sensor(0x303A, 0x00);
	write_cmos_sensor(0x303B, 0x33);
   //Output Size Setting
   write_cmos_sensor(0x034C, 0x05);
   write_cmos_sensor(0x034D, 0x24);
   write_cmos_sensor(0x034E, 0x01);
   write_cmos_sensor(0x034F, 0xE0);
   //White remosaic setting 

   write_cmos_sensor(0x3029, 0x01);
   write_cmos_sensor(0x3A00, 0x01);
   write_cmos_sensor(0x3A01, 0x01);
   write_cmos_sensor(0x3A02, 0x05);
   write_cmos_sensor(0x3A03, 0x05);
   write_cmos_sensor(0x3A04, 0x05);
   write_cmos_sensor(0x3A05, 0xF8);
   write_cmos_sensor(0x3A06, 0x40);
   write_cmos_sensor(0x3A07, 0xFE);
   write_cmos_sensor(0x3A08, 0x10);
   write_cmos_sensor(0x3A09, 0x14);
   write_cmos_sensor(0x3A0A, 0xFE);
   write_cmos_sensor(0x3A0B, 0x44);
   //Integration Time Setting
   write_cmos_sensor(0x0202, 0x03);
   write_cmos_sensor(0x0203, 0x16);
   //Gain setting
   
   write_cmos_sensor(0x0204, 0x00);
   write_cmos_sensor(0x0205, 0x00);
   write_cmos_sensor(0x020E, 0x01);
   write_cmos_sensor(0x020F, 0x00);
   write_cmos_sensor(0x0100, 0x01);// STREAM START
}

static void slim_video_setting()  // VideoHDSetting
{
    LOG_INF("E\n  Slim_Video  30fps ");
    //PLL setting 
    write_cmos_sensor(0x0100,0x00);// STREAM STop

        //Preset Setting
    //write_cmos_sensor(0x3000, 0x00);//   
    //write_cmos_sensor(0x3001, 0x00);//   
    //Output format Setting
    write_cmos_sensor(0x0112, 0x0A);//   
    write_cmos_sensor(0x0113, 0x0A);//   
    write_cmos_sensor(0x0114, 0x03);//   
    //Clock Setting
    write_cmos_sensor(0x0301, 0x05);//   
    write_cmos_sensor(0x0303, 0x02);//   
    write_cmos_sensor(0x0305, 0x0C);//   
    write_cmos_sensor(0x0306, 0x01);//   
    write_cmos_sensor(0x0307, 0xCC);//   
    write_cmos_sensor(0x0309, 0x0A);//   
    write_cmos_sensor(0x030B, 0x01);// 
    write_cmos_sensor(0x030D, 0x0C);// 
    write_cmos_sensor(0x030E, 0x05);// 
    write_cmos_sensor(0x030F, 0x14);// 
    write_cmos_sensor(0x0310, 0x00);// 
    write_cmos_sensor(0x3041, 0x01);// 

    //linelength,framelength setting
    //mdelay(35);
    write_cmos_sensor(0x0342, 0x13);//   
    write_cmos_sensor(0x0343, 0x70);//   
    write_cmos_sensor(0x0340, 0x04);//   
    write_cmos_sensor(0x0341, 0xCC);// 
    //ROI setting
    write_cmos_sensor(0x0344, 0x00);
    write_cmos_sensor(0x0345, 0x00);  
    write_cmos_sensor(0x0346, 0x01); 
    write_cmos_sensor(0x0347, 0x80);  
    write_cmos_sensor(0x0348, 0x10);  
    write_cmos_sensor(0x0349, 0x6F);  
    write_cmos_sensor(0x034A, 0x0A);  
    write_cmos_sensor(0x034B, 0xAF); 
	//Analog Image Size Setting
    write_cmos_sensor(0x0381, 0x01);  
    write_cmos_sensor(0x0383, 0x01); 
    write_cmos_sensor(0x0385, 0x01); 
    write_cmos_sensor(0x0387, 0x01);
	  write_cmos_sensor(0x0900, 0x01);
	 write_cmos_sensor(0x0901, 0x12);
	//Digital Image Size Setting
	write_cmos_sensor(0x0401, 0x02);
    write_cmos_sensor(0x0404, 0x00);
	write_cmos_sensor(0x0405, 0x18);
	write_cmos_sensor(0x0408, 0x00);
	write_cmos_sensor(0x0409, 0x02);
	write_cmos_sensor(0x040A, 0x00);
	write_cmos_sensor(0x040B, 0x00);
	write_cmos_sensor(0x040C, 0x10);
	write_cmos_sensor(0x040D, 0x6A);
	write_cmos_sensor(0x040E, 0x04);
	write_cmos_sensor(0x040F, 0x96);
	write_cmos_sensor(0x3038, 0x01);
	write_cmos_sensor(0x303A, 0x00);
	write_cmos_sensor(0x303B, 0x30);
   //Output Size Setting
   write_cmos_sensor(0x034C, 0x05);
   write_cmos_sensor(0x034D, 0x78);
   write_cmos_sensor(0x034E, 0x03);
   write_cmos_sensor(0x034F, 0x0E);
   //White remosaic setting 

   write_cmos_sensor(0x3029, 0x01);
   write_cmos_sensor(0x3A00, 0x01);
   write_cmos_sensor(0x3A01, 0x01);
   write_cmos_sensor(0x3A02, 0x05);
   write_cmos_sensor(0x3A03, 0x05);
   write_cmos_sensor(0x3A04, 0x05);
   write_cmos_sensor(0x3A05, 0xF8);
   write_cmos_sensor(0x3A06, 0x40);
   write_cmos_sensor(0x3A07, 0xFE);
   write_cmos_sensor(0x3A08, 0x10);
   write_cmos_sensor(0x3A09, 0x14);
   write_cmos_sensor(0x3A0A, 0xFE);
   write_cmos_sensor(0x3A0B, 0x44);
   //Integration Time Setting
   write_cmos_sensor(0x0202, 0x04);
   write_cmos_sensor(0x0203, 0xC2);
   //Gain setting
   
   write_cmos_sensor(0x0204, 0x00);
   write_cmos_sensor(0x0205, 0x00);
   write_cmos_sensor(0x020E, 0x01);
   write_cmos_sensor(0x020F, 0x00);
    write_cmos_sensor(0x0100,0x01);// STREAM START
}

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
    LOG_INF("enable: %d\n", enable);

    if (enable) {
        // 0x5E00[8]: 1 enable,  0 disable
        // 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        //write_cmos_sensor(0x30D8, 0x10);
       // write_cmos_sensor(0x0600, 0x00);
        write_cmos_sensor(0x0601, 0x04);
    } else {
        // 0x5E00[8]: 1 enable,  0 disable
        // 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        //write_cmos_sensor(0x30D8, 0x00);
        write_cmos_sensor(0x0601, 0x00);
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
            	  LOG_INF("test\n");              
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);   
                break;
            }   
            LOG_INF("Read sensor id fail,id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
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
    LOG_INF("E\n");

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
	LOG_INF("Exit preview\n");
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
    normal_video_setting(imgsensor.current_fps);
    
    
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
    
    return ERROR_NONE;
}   /*  slim_video   */

static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
    LOG_INF("E\n");
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
        default:
            break;
    }

    return ERROR_NONE;
}
static void setAwbGain(SET_SENSOR_AWB_GAIN* rggb)
{
    UINT32 rH;
    UINT32 rL;
    UINT32 grH;
    UINT32 grL;
    UINT32 gbH;
    UINT32 gbL;
    UINT32 bH;
    UINT32 bL;

    rggb->ABS_GAIN_R/=2;
    rggb->ABS_GAIN_GR/=2;
    rggb->ABS_GAIN_GB/=2;
    rggb->ABS_GAIN_B/=2;

    rH = rggb->ABS_GAIN_R>>8;
    rL = rggb->ABS_GAIN_R&255;
    grH= rggb->ABS_GAIN_GR>>8;
    grL= rggb->ABS_GAIN_GR&255;
    gbH= rggb->ABS_GAIN_GB>>8;
    gbL= rggb->ABS_GAIN_GB&255;
    bH = rggb->ABS_GAIN_B>>8;
    bL = rggb->ABS_GAIN_B&255;

    //gr
    write_cmos_sensor(0x0b8e, grH);
    write_cmos_sensor(0x0b8f, grL);
    //r
    write_cmos_sensor(0x0b90, rH);
    write_cmos_sensor(0x0b91, rL);
    //b
    write_cmos_sensor(0x0b92, bH);
    write_cmos_sensor(0x0b93, bL);
    //gb
    write_cmos_sensor(0x0b94, gbH);
    write_cmos_sensor(0x0b95, gbL);

    LOG_INF("rgbw setAwbGain %d %d %d %d\n", rggb->ABS_GAIN_R, rggb->ABS_GAIN_GR, rggb->ABS_GAIN_GB, rggb->ABS_GAIN_B);
}


#define SWAP_INT32(a, b) {INT32 c; c=a; a=b; b=c;}
static INT32 interp(INT32 x1, INT32 y1, INT32 x2, INT32 y2, INT32 x)
{
	INT32 temp;
	if(x2<x1)
	{
		SWAP_INT32(x1, x2);
		SWAP_INT32(y1, y2);
	}

	if(x<x1)
		return y1;
	if(x>x2)
		return y2;
	if(x1==x2)
		return y1;
	return y1+(x-x1)*(y2-y1)/(x2-x1);
}
static void setIso(SET_SENSOR_ISO* para)
{
    UINT32 iso;
    UINT32 sensorMode;
    iso = para->ISO;
    sensorMode = para->SENSOR_MODE;

    INT32 val;
    val = interp(100, 0xb9, 400, 0x93, iso);


    write_cmos_sensor(0x3011, val);
    LOG_INF("rgbw setIso, iso=%d mode=%d, reg=%d\n", iso, sensorMode, val);

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
        case SENSOR_FEATURE_SET_AWB_GAIN:
            setAwbGain((SET_SENSOR_AWB_GAIN*)feature_para);
            break;
        case SENSOR_FEATURE_SET_ISO:
            setIso((SET_SENSOR_ISO*)feature_para);
            break;
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
            #if 0
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
            #endif          
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

UINT32 IMX278_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
    /* To Do : Check Sensor status here */
    if (pfFunc!=NULL)
        *pfFunc=&sensor_func;
    return ERROR_NONE;
}   /*  IMX135_MIPI_RAW_SensorInit  */
