/*****************************************************************************
 *
 * Filename:
 * ---------
 *   S5k3M2_2NDmipi_Sensor.c
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

#include "s5k3m2_2nd_mipi_Sensor.h"
#include <linux/dev_info.h>
#define PFX "S5k3M2_2ND_camera_sensor"
#define LOG_1 LOG_INF("S5k3M2_2ND,MIPI 4LANE\n")
#define LOG_2 LOG_INF("preview 2096*1552@30fps,1260Mbps/lane; video 4192*3104@30fps,1260Mbps/lane; capture 13M@30fps,1260Mbps/lane\n")
//#define LOG_DBG(format, args...) xlog_printk(ANDROID_LOG_DEBUG ,PFX, "[%S] " format, __FUNCTION__, ##args)
#define LOG_INF(format, args...)	xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)
#define LOGE(format, args...)   xlog_printk(ANDROID_LOG_ERROR, PFX, "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);
static imgsensor_info_struct imgsensor_info = { 
	.sensor_id = S5K3M2_2ND_SENSOR_ID,
	.checksum_value = 0x752cfcc1,
	.pre = {
		.pclk = 440000000,				//record different mode's pclk
		.linelength = 4592,				//record different mode's linelength
		.framelength =3188, //3168,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2096,		//record different mode's width of grabwindow
		.grabwindow_height = 1552,		//record different mode's height of grabwindow

		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
	.cap = {
		.pclk = 440000000,
		.linelength =4592,
		.framelength = 3188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4192,//5334,
		.grabwindow_height = 3104,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
		},
	.cap1 = {
		.pclk = 336000000,
		.linelength = 4592,
		.framelength = 3188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width =4192,
		.grabwindow_height = 3104,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 240,	
	},
	.normal_video = {
		.pclk = 440000000,
		.linelength = 4592,
		.framelength = 3188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 4192,//5334,
		.grabwindow_height = 3104,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 440000000,
		.linelength = 4592,
		.framelength = 736,
		.startx = 0,
		.starty = 0,
		.grabwindow_width =1280, //1920,
		.grabwindow_height =720,// 1080,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 1200,
	},
	.slim_video = {
		.pclk = 440000000,
		.linelength = 4592,
		.framelength = 3188,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 1280,//1280,
		.grabwindow_height =720,// 720,
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		.max_framerate = 300,
	},
    .custom1 = {
		.pclk = 440000000,				//record different mode's pclk
		.linelength = 4592,				//record different mode's linelength
		.framelength =3188, //3168,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2096,		//record different mode's width of grabwindow
		.grabwindow_height = 1552,		//record different mode's height of grabwindow

		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
    .custom2 = {
		.pclk = 440000000,				//record different mode's pclk
		.linelength = 4592,				//record different mode's linelength
		.framelength =3188, //3168,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2096,		//record different mode's width of grabwindow
		.grabwindow_height = 1552,		//record different mode's height of grabwindow

		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
    .custom3 = {
		.pclk = 440000000,				//record different mode's pclk
		.linelength = 4592,				//record different mode's linelength
		.framelength =3188, //3168,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2096,		//record different mode's width of grabwindow
		.grabwindow_height = 1552,		//record different mode's height of grabwindow

		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
    .custom4 = {
		.pclk = 440000000,				//record different mode's pclk
		.linelength = 4592,				//record different mode's linelength
		.framelength =3188, //3168,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2096,		//record different mode's width of grabwindow
		.grabwindow_height = 1552,		//record different mode's height of grabwindow

		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
    .custom5 = {
		.pclk = 440000000,				//record different mode's pclk
		.linelength = 4592,				//record different mode's linelength
		.framelength =3188, //3168,			//record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2096,		//record different mode's width of grabwindow
		.grabwindow_height = 1552,		//record different mode's height of grabwindow

		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,//unit , ns
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
	.margin = 4,
	.min_shutter = 2,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame = 0,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num
	
	.cap_delay_frame = 2, 
	.pre_delay_frame = 2,  
	.video_delay_frame = 2,
	.hs_video_delay_frame = 2,
	.slim_video_delay_frame = 2,
    .custom1_delay_frame = 2,
    .custom2_delay_frame = 2, 
    .custom3_delay_frame = 2, 
    .custom4_delay_frame = 2, 
    .custom5_delay_frame = 2,
	
	.isp_driving_current = ISP_DRIVING_6MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO,//0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gb,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x5A, 0xff},
    .i2c_speed = 300, // i2c read/write speed
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x200,					//current shutter
	.gain = 0x100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 0,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_en = KAL_FALSE, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x5A,
};


/* Sensor output window information */
static SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5] =	 
{
#if 1
 { 4192, 3104,	  0,  0, 4192, 3104, 2096,  1552, 0000, 0000, 2096, 1552, 0,	0, 2096,  1552}, // Preview 
 { 4192, 3104,	  0,  0, 4192, 3104, 4192,  3104, 0000, 0000, 4192, 3104, 0,	0, 4192,  3104}, // capture 
 { 4192, 3104,	  0,  0, 4192, 3104, 4192,  3104, 0000, 0000, 4192, 3104, 0,	0, 4192,  3104}, // video 
 #if 0
 { 4192, 3104,	  0,  0, 4192, 3104,  688,   512, 0000, 0000,  688,  512, 0,	0,  688,   512},// hight video 120
 #else
 { 4192, 3104,	  0,  485, 4192, 2328, 1280,   720, 0000, 0000, 1280,  720, 0,	0, 1280,   720},// hs video 
 #endif
 { 4192, 3104,	  0,  485, 4192, 2328, 1280,   720, 0000, 0000, 1280,  720, 0,	0, 1280,   720},// slim video 
 { 4192, 3104,	  0,  0, 4192, 3104, 2096,  1552, 0000, 0000, 2096, 1552, 0,	0, 2096,  1552}, // Custom1 (defaultuse preview) 
 { 4192, 3104,	  0,  0, 4192, 3104, 2096,  1552, 0000, 0000, 2096, 1552, 0,	0, 2096,  1552}, // Custom2 
 { 4192, 3104,	  0,  0, 4192, 3104, 2096,  1552, 0000, 0000, 2096, 1552, 0,	0, 2096,  1552}, // Custom3 
 { 4192, 3104,	  0,  0, 4192, 3104, 2096,  1552, 0000, 0000, 2096, 1552, 0,	0, 2096,  1552}, // Custom4 
 { 4192, 3104,	  0,  0, 4192, 3104, 2096,  1552, 0000, 0000, 2096, 1552, 0,	0, 2096,  1552}, // Custom5 
#else
 { 4224, 3136,	   12,  8, 4211, 3127, 2096,  1552, 0000, 0000, 2096, 1552, 0,	 0, 2096,  1552}, // Preview 
  { 4224, 3136,    12,  12, 4211, 3123, 4192,  3104, 0000, 0000, 4192, 3104, 0,	 0, 4192,  3104}, // capture 
  { 4224, 3136,    12,  12, 4211, 3123, 4192,  3104, 0000, 0000, 4192, 3104, 0,	 0, 4192,  3104}, // video 
  { 4224, 3136,    12,  8, 4211, 3127,  688,   512, 0000, 0000,	688,  512, 0,	 0,  688,	512},// hight video 120
  { 4224, 3136,    188,  476, 4035, 2659, 1280,	720, 0000, 0000, 1280,	720, 0,  0, 1280,	720},// slim video 
  { 4224, 3136,	   12,  8, 4211, 3127, 2096,  1552, 0000, 0000, 2096, 1552, 0,	 0, 2096,  1552}, // Custom1 (defaultuse preview) 
  { 4224, 3136,	   12,  8, 4211, 3127, 2096,  1552, 0000, 0000, 2096, 1552, 0,	 0, 2096,  1552}, // Custom2 
  { 4224, 3136,	   12,  8, 4211, 3127, 2096,  1552, 0000, 0000, 2096, 1552, 0,	 0, 2096,  1552}, // Custom3 
  { 4224, 3136,	   12,  8, 4211, 3127, 2096,  1552, 0000, 0000, 2096, 1552, 0,	 0, 2096,  1552}, // Custom4 
  { 4224, 3136,	   12,  8, 4211, 3127, 2096,  1552, 0000, 0000, 2096, 1552, 0,	 0, 2096,  1552}, // Custom5 
  #endif

 };// slim video  

static SET_PD_BLOCK_INFO_T imgsensor_pd_info =
{
    .i4OffsetX = 23,
    .i4OffsetY = 16,
    .i4PitchX  = 64,
    .i4PitchY  = 64,
    .i4PairNum  =16,
    .i4SubBlkW  =16,
    .i4SubBlkH  =16,
//	.i4PosL = {{20,23},{72,23},{36,27},{56,27},{24,43},{68,43},{40,47},{52,47},{40,55},{52,55},{24,59},{68,59},{36,75},{56,75},{20,79},{72,79}},	
//	.i4PosR = {{20,27},{72,27},{36,31},{56,31},{24,39},{68,39},{40,43},{52,43},{40,59},{52,59},{24,63},{68,63},{36,71},{56,71},{20,75},{72,75}},    
	.i4PosL = {{23,20},{75,20},{39,24},{59,24},{27,32},{71,32},{43,36},{55,36},{43,52},{55,52},{27,56},{71,56},{39,64},{59,64},{23,68},{75,68}},    
    .i4PosR = {{23,16},{75,16},{39,20},{59,20},{27,36},{71,36},{43,40},{55,40},{43,48},{55,48},{27,52},{71,52},{39,68},{59,68},{23,72},{75,72}},
};

 
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte, 2, imgsensor.i2c_write_id);
    return ((get_byte<<8)&0xff00)|((get_byte>>8)&0x00ff);
}


static void write_cmos_sensor(kal_uint16 addr, kal_uint16 para)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    char pusendcmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para >> 8),(char)(para & 0xFF)};
    iWriteRegI2C(pusendcmd , 4, imgsensor.i2c_write_id);
}

static kal_uint16 read_cmos_sensor_8(kal_uint16 addr)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    kal_uint16 get_byte=0;
    char pusendcmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
    iReadRegI2C(pusendcmd , 2, (u8*)&get_byte,1,imgsensor.i2c_write_id);
    return get_byte;
}

static void write_cmos_sensor_8(kal_uint16 addr, kal_uint8 para)
{
    kdSetI2CSpeed(imgsensor_info.i2c_speed); // Add this func to set i2c speed by each sensor
    char pusendcmd[4] = {(char)(addr >> 8) , (char)(addr & 0xFF) ,(char)(para & 0xFF)};
    iWriteRegI2C(pusendcmd , 3, imgsensor.i2c_write_id);
}


#define S5k3M2_2ND_USE_AWB_OTP

#if defined(S5k3M2_2ND_USE_AWB_OTP)

#define GAIN_DEFAULT       0x0100
#define S5k3M2_2NDOTP_WRITE_ID         0xB0
static struct S5k3M2_2ND_MIPI_otp_struct current_otp;

inline kal_uint16 S5k3M2_2ND_read_cmos_sensor1(kal_uint32 addr)
{
	kal_uint16 get_byte = 0;
	
	char puSendCmd[2] = {(char)(addr >> 8) , (char)(addr & 0xFF) };
	iReadRegI2C(puSendCmd , 2, (u8*)&get_byte, 1, S5k3M2_2NDOTP_WRITE_ID);
	return get_byte&0x00ff;
}

void S5k3M2_2ND_MIPI_read_otp_wb(struct S5k3M2_2ND_MIPI_otp_struct *otp)
{
   	kal_uint16 golden_R, golden_G, golden_Gr, golden_Gb, golden_B, current_R, current_G, current_Gr, current_Gb, current_B, r_ratio, b_ratio, FLG = 0x00;
   
   	FLG = S5k3M2_2ND_read_cmos_sensor1(0x0000); 
   
  	if(FLG==0)
		LOG_INF("No OTP Data or OTP data is invalid");
   	else
   	{
   		//golden_R = S5k3M2_2ND_read_cmos_sensor1(0x0005);
		//golden_Gr = S5k3M2_2ND_read_cmos_sensor1(0x0006);
 		//golden_Gb = S5k3M2_2ND_read_cmos_sensor1(0x0007);
		//golden_B = S5k3M2_2ND_read_cmos_sensor1(0x0008);
  		//LOG_INF("[S5k3M2_2ND] [S5k3M2_2ND_MIPI_read_otp_wb11] golden_R=0x%x, golden_Gr=0x%x, golden_Gb=0x%x, golden_B=0x%x\n", golden_R, golden_Gr, golden_Gb, golden_B);
		golden_R = 0x4b;
		golden_Gr = 0x8f;
 		golden_Gb = 0x8f;
		golden_B = 0x4e;
  		LOG_INF("[S5k3M2_2ND] [S5k3M2_2ND_MIPI_read_otp_wb] golden_R=0x%x, golden_Gr=0x%x, golden_Gb=0x%x, golden_B=0x%x\n", golden_R, golden_Gr, golden_Gb, golden_B);
  		current_R = S5k3M2_2ND_read_cmos_sensor1(0x0009);
  		current_Gr = S5k3M2_2ND_read_cmos_sensor1(0x000a);
  		current_Gb = S5k3M2_2ND_read_cmos_sensor1(0x000b);
  		current_B = S5k3M2_2ND_read_cmos_sensor1(0x000c); 
  		LOG_INF("[S5k3M2_2ND] [S5k3M2_2ND_MIPI_read_otp_wb] current_R=0x%x, current_Gr=0x%x, current_Gb=0x%x, current_B=0x%x\n", current_R, current_Gr, current_Gb, current_B);
   		golden_G = (golden_Gr + golden_Gb) / 2;
   		current_G = (current_Gr + current_Gb) / 2;

   		if(!golden_G || !current_G || !golden_R || !golden_B || !current_R || !current_B)
     		LOG_INF("WB update error!");
   
   		r_ratio = 512 * golden_R * current_G /( golden_G * current_R );
   		b_ratio = 512 * golden_B * current_G /( golden_G * current_B );

   		otp->r_ratio = r_ratio;
   		otp->b_ratio = b_ratio;
	
   		LOG_INF("[S5k3M2_2ND] [S5k3M2_2ND_MIPI_read_otp_wb] r_ratio=0x%x, b_ratio=0x%x\n", otp->r_ratio, otp->b_ratio);      
   	}
}



void S5k3M2_2ND_MIPI_algorithm_otp_wb1(struct S5k3M2_2ND_MIPI_otp_struct *otp)
{
   kal_uint16 R_GAIN, B_GAIN, Gr_GAIN, Gb_GAIN, G_GAIN, r_ratio, b_ratio;
   
   r_ratio = otp->r_ratio;
   b_ratio = otp->b_ratio;

	if(r_ratio >= 512 )
   {
    	if(b_ratio >= 512) 
        {
        	R_GAIN = GAIN_DEFAULT * r_ratio / 512;
            G_GAIN = GAIN_DEFAULT;     
            B_GAIN = GAIN_DEFAULT * b_ratio / 512;
   		}
        else
        {
           	R_GAIN = GAIN_DEFAULT * 512 / b_ratio  * r_ratio / 512;
           	G_GAIN = GAIN_DEFAULT * 512 / b_ratio;
           	B_GAIN = GAIN_DEFAULT;    
        }
   }
	else                      
   {
   		if(b_ratio >= 512)
    	{
      		R_GAIN = GAIN_DEFAULT;    
      	  	G_GAIN = GAIN_DEFAULT * 512/ r_ratio ;
           	B_GAIN = GAIN_DEFAULT * 512 / r_ratio * b_ratio / 512;
        } 
        else 
        {
           	Gr_GAIN = GAIN_DEFAULT * 512 / r_ratio;
           	Gb_GAIN = GAIN_DEFAULT * 512 / b_ratio;

			if(Gr_GAIN >= Gb_GAIN)
            {
            	R_GAIN = GAIN_DEFAULT;
              	G_GAIN = GAIN_DEFAULT * 512 / r_ratio;
                B_GAIN = GAIN_DEFAULT * 512 / r_ratio * b_ratio / 512;
        	} 
        	else
            {
       			R_GAIN = GAIN_DEFAULT * 512 / b_ratio * r_ratio / 512;
              	G_GAIN = GAIN_DEFAULT * 512 / b_ratio;
              	B_GAIN = GAIN_DEFAULT;
            }
   		}        
	}

   	otp->R_Gain = R_GAIN;
   	otp->B_Gain = B_GAIN;
   	otp->G_Gain = G_GAIN;

   LOG_INF("[S5k3M2_2ND] [S5k3M2_2ND_MIPI_algorithm_otp_wb1] R_gain=0x%x, B_gain=0x%x, G_gain=0x%x\n", otp->R_Gain, otp->B_Gain, otp->G_Gain);    
}



void S5k3M2_2ND_MIPI_write_otp_wb(struct S5k3M2_2ND_MIPI_otp_struct *otp)
{
   kal_uint16 R_GAIN, B_GAIN, G_GAIN;

   R_GAIN = otp->R_Gain;
   B_GAIN = otp->B_Gain;
   G_GAIN = otp->G_Gain;



	
   write_cmos_sensor(0x6028, 0x4000);
   write_cmos_sensor(0x602A, 0x3056);
	write_cmos_sensor(0x6f12, 0x0100);
	write_cmos_sensor(0x602A, 0x020e);
	write_cmos_sensor(0x6f12, G_GAIN);
	write_cmos_sensor(0x602A, 0x0210);
	write_cmos_sensor(0x6f12, R_GAIN);
	write_cmos_sensor(0x602A, 0x0212);
	write_cmos_sensor(0x6f12, B_GAIN);
	write_cmos_sensor(0x602A, 0x0214);
	write_cmos_sensor(0x6f12, G_GAIN);
   //write_cmos_sensor(0x020e, G_GAIN);
   //write_cmos_sensor(0x0210, R_GAIN);
   //write_cmos_sensor(0x0212, B_GAIN);
   //write_cmos_sensor(0x0214, G_GAIN);
}



void S5k3M2_2ND_MIPI_update_wb_register_from_otp(void)
{
	
	
	S5k3M2_2ND_MIPI_algorithm_otp_wb1(&current_otp);
	S5k3M2_2ND_MIPI_write_otp_wb(&current_otp);
}
#endif


static void set_dummy()
{
	 LOG_INF("dummyline = %d, dummypixels = %d ", imgsensor.dummy_line, imgsensor.dummy_pixel);
    //write_cmos_sensor_8(0x0104, 0x01); 
    write_cmos_sensor(0x0340, imgsensor.frame_length);
    write_cmos_sensor(0x0342, imgsensor.line_length);
   // write_cmos_sensor_8(0x0104, 0x00); 
  
}	/*	set_dummy  */
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
}	/*	set_max_framerate  */


static void write_shutter(kal_uint16 shutter)
{
	kal_uint16 realtime_fps = 0;
    kal_uint32 frame_length = 0;
	unsigned long flags;
	spin_lock_irqsave(&imgsensor_drv_lock, flags);
	imgsensor.shutter = shutter;
	spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	
    spin_lock_irqsave(&imgsensor_drv_lock, flags);
    if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)       
        imgsensor.frame_length = shutter + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock_irqrestore(&imgsensor_drv_lock, flags);
	shutter = (shutter < imgsensor_info.min_shutter) ? imgsensor_info.min_shutter : shutter;
	shutter = (shutter > (imgsensor_info.max_frame_length - imgsensor_info.margin)) ? (imgsensor_info.max_frame_length - imgsensor_info.margin) : shutter;
 #if 1   
    if (imgsensor.autoflicker_en) { 
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146,0);	
    } else {
        // Extend frame length
        //write_cmos_sensor_8(0x0104,0x01);
        write_cmos_sensor(0x0340, imgsensor.frame_length);
        //write_cmos_sensor_8(0x0104,0x00);
    }
#endif
    // Update Shutter
    //write_cmos_sensor_8(0x0104,0x01);
	write_cmos_sensor(0x0340, imgsensor.frame_length);
    write_cmos_sensor(0x0202, shutter);
    //write_cmos_sensor_8(0x0104,0x00);
	LOG_INF("Exit! shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);
}	/*	write_shutter  */



/*************************************************************************
* FUNCTION
*	set_shutter
*
* DESCRIPTION
*	This function set e-shutter of sensor to change exposure time.
*
* PARAMETERS
*	iShutter : exposured lines
*
* RETURNS
*	None
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
}	/*	set_shutter */



static kal_uint16 gain2reg(const kal_uint16 gain)
{
	 kal_uint16 reg_gain = 0x0;
    
    reg_gain = gain/2;
    return (kal_uint16)reg_gain;
}

/*************************************************************************
* FUNCTION
*	set_gain
*
* DESCRIPTION
*	This function is to set global gain to sensor.
*
* PARAMETERS
*	iGain : sensor global gain(base: 0x40)
*
* RETURNS
*	the actually gain set to sensor.
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
    if (gain < BASEGAIN || gain > 16 * BASEGAIN) {
        LOG_INF("Error gain setting");

        if (gain < BASEGAIN)
            gain = BASEGAIN;
        else if (gain > 16 * BASEGAIN)
            gain = 16 * BASEGAIN;        
    }
 
    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain; 
    spin_unlock(&imgsensor_drv_lock);
   // LOG_INF("gain = %d , reg_gain = 0x%x ", gain, reg_gain);

    //write_cmos_sensor_8(0x0104, 0x01);
    write_cmos_sensor_8(0x0204,(reg_gain>>8));
    write_cmos_sensor_8(0x0205,(reg_gain&0xff));
    //write_cmos_sensor_8(0x0104, 0x00);
    
    return gain;
}	/*	set_gain  */
static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d", image_mirror);

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
	spin_lock(&imgsensor_drv_lock);
    imgsensor.mirror= image_mirror; 
    spin_unlock(&imgsensor_drv_lock);
    switch (image_mirror) {

        case IMAGE_NORMAL:
            write_cmos_sensor_8(0x0101,0x00);   // Gr
            break;
        case IMAGE_H_MIRROR:
            write_cmos_sensor_8(0x0101,0x01);
            break;
        case IMAGE_V_MIRROR:
            write_cmos_sensor_8(0x0101,0x02);
            break;
        case IMAGE_HV_MIRROR:
            write_cmos_sensor_8(0x0101,0x03);//Gb
            break;
        default:
			LOG_INF("Error image_mirror setting\n");
    }

}

/*************************************************************************
* FUNCTION
*	night_mode
*
* DESCRIPTION
*	This function night mode of sensor.
*
* PARAMETERS
*	bEnable: KAL_TRUE -> enable night mode, otherwise, disable night mode
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static void sensor_init(void)
{
  //s 2014/12/01, bruce
  kal_uint16 chip_id = 0;
  S5k3M2_2ND_MIPI_read_otp_wb(&current_otp);
  chip_id = read_cmos_sensor(0x0002);
  
  if (chip_id == 0xC001) {
  LOG_INF("-- sensor_init, chip id = 0xC001\n");
  //e 2014/12/01, bruce
	LOG_INF("E\n");  
  write_cmos_sensor(0x6028, 0x4000);
  write_cmos_sensor(0x6214, 0x7971);
  write_cmos_sensor(0x6218, 0x0100);
  write_cmos_sensor(0x6028, 0x2000);
  write_cmos_sensor(0x602A, 0x4390);
  write_cmos_sensor(0x6F12, 0x0448);
  write_cmos_sensor(0x6F12, 0x0349);
  write_cmos_sensor(0x6F12, 0x0160);
  write_cmos_sensor(0x6F12, 0xC26A);
  write_cmos_sensor(0x6F12, 0x511A);
  write_cmos_sensor(0x6F12, 0x8180);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x16BA);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x49F0);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1FA0);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x70B5);
  write_cmos_sensor(0x6F12, 0x0446);
  write_cmos_sensor(0x6F12, 0xF848);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0x0068);
  write_cmos_sensor(0x6F12, 0x86B2);
  write_cmos_sensor(0x6F12, 0x050C);
  write_cmos_sensor(0x6F12, 0x3146);
  write_cmos_sensor(0x6F12, 0x2846);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xCDFA);
  write_cmos_sensor(0x6F12, 0x2046);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xCFFA);
  write_cmos_sensor(0x6F12, 0xF34C);
  write_cmos_sensor(0x6F12, 0xF44A);
  write_cmos_sensor(0x6F12, 0x6189);
  write_cmos_sensor(0x6F12, 0x1088);
  write_cmos_sensor(0x6F12, 0x8142);
  write_cmos_sensor(0x6F12, 0x00D9);
  write_cmos_sensor(0x6F12, 0x0846);
  write_cmos_sensor(0x6F12, 0x6081);
  write_cmos_sensor(0x6F12, 0xA389);
  write_cmos_sensor(0x6F12, 0x5188);
  write_cmos_sensor(0x6F12, 0x8B42);
  write_cmos_sensor(0x6F12, 0x00D9);
  write_cmos_sensor(0x6F12, 0x1946);
  write_cmos_sensor(0x6F12, 0xEF4B);
  write_cmos_sensor(0x6F12, 0xA181);
  write_cmos_sensor(0x6F12, 0xC0F3);
  write_cmos_sensor(0x6F12, 0x0900);
  write_cmos_sensor(0x6F12, 0x1880);
  write_cmos_sensor(0x6F12, 0xC1F3);
  write_cmos_sensor(0x6F12, 0x0900);
  write_cmos_sensor(0x6F12, 0x991C);
  write_cmos_sensor(0x6F12, 0x0880);
  write_cmos_sensor(0x6F12, 0x9188);
  write_cmos_sensor(0x6F12, 0x2069);
  write_cmos_sensor(0x6F12, 0x8842);
  write_cmos_sensor(0x6F12, 0x00D8);
  write_cmos_sensor(0x6F12, 0x0846);
  write_cmos_sensor(0x6F12, 0xE849);
  write_cmos_sensor(0x6F12, 0x2061);
  write_cmos_sensor(0x6F12, 0x091D);
  write_cmos_sensor(0x6F12, 0x0880);
  write_cmos_sensor(0x6F12, 0xE64B);
  write_cmos_sensor(0x6F12, 0xC0F3);
  write_cmos_sensor(0x6F12, 0x0141);
  write_cmos_sensor(0x6F12, 0x9B1D);
  write_cmos_sensor(0x6F12, 0x1980);
  write_cmos_sensor(0x6F12, 0xD288);
  write_cmos_sensor(0x6F12, 0x6169);
  write_cmos_sensor(0x6F12, 0x9142);
  write_cmos_sensor(0x6F12, 0x00D8);
  write_cmos_sensor(0x6F12, 0x1146);
  write_cmos_sensor(0x6F12, 0xE14A);
  write_cmos_sensor(0x6F12, 0x6161);
  write_cmos_sensor(0x6F12, 0x0832);
  write_cmos_sensor(0x6F12, 0x1180);
  write_cmos_sensor(0x6F12, 0x921C);
  write_cmos_sensor(0x6F12, 0xC1F3);
  write_cmos_sensor(0x6F12, 0x0141);
  write_cmos_sensor(0x6F12, 0x1180);
  write_cmos_sensor(0x6F12, 0xDB4A);
  write_cmos_sensor(0x6F12, 0x1A32);
  write_cmos_sensor(0x6F12, 0x911E);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x9EFA);
  write_cmos_sensor(0x6F12, 0xA07E);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x0701);
  write_cmos_sensor(0x6F12, 0x208B);
  write_cmos_sensor(0x6F12, 0x61F3);
  write_cmos_sensor(0x6F12, 0x9F20);
  write_cmos_sensor(0x6F12, 0xD749);
  write_cmos_sensor(0x6F12, 0x0C31);
  write_cmos_sensor(0x6F12, 0x0880);
  write_cmos_sensor(0x6F12, 0xD44A);
  write_cmos_sensor(0x6F12, 0x6069);
  write_cmos_sensor(0x6F12, 0x1E32);
  write_cmos_sensor(0x6F12, 0x911E);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x8FFA);
  write_cmos_sensor(0x6F12, 0xA07F);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x0701);
  write_cmos_sensor(0x6F12, 0xA08B);
  write_cmos_sensor(0x6F12, 0x61F3);
  write_cmos_sensor(0x6F12, 0x9F20);
  write_cmos_sensor(0x6F12, 0xD049);
  write_cmos_sensor(0x6F12, 0x0E31);
  write_cmos_sensor(0x6F12, 0x0880);
  write_cmos_sensor(0x6F12, 0x3146);
  write_cmos_sensor(0x6F12, 0x2846);
  write_cmos_sensor(0x6F12, 0xBDE8);
  write_cmos_sensor(0x6F12, 0x7040);
  write_cmos_sensor(0x6F12, 0x0122);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x75BA);
  write_cmos_sensor(0x6F12, 0x2DE9);
  write_cmos_sensor(0x6F12, 0xF041);
  write_cmos_sensor(0x6F12, 0x0446);
  write_cmos_sensor(0x6F12, 0xC648);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0x4068);
  write_cmos_sensor(0x6F12, 0x86B2);
  write_cmos_sensor(0x6F12, 0x050C);
  write_cmos_sensor(0x6F12, 0x3146);
  write_cmos_sensor(0x6F12, 0x2846);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x69FA);
  write_cmos_sensor(0x6F12, 0x2046);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x75FA);
  write_cmos_sensor(0x6F12, 0xC449);
  write_cmos_sensor(0x6F12, 0xC24B);
  write_cmos_sensor(0x6F12, 0x0888);
  write_cmos_sensor(0x6F12, 0x1A88);
  write_cmos_sensor(0x6F12, 0x9042);
  write_cmos_sensor(0x6F12, 0x00D8);
  write_cmos_sensor(0x6F12, 0x1046);
  write_cmos_sensor(0x6F12, 0x0880);
  write_cmos_sensor(0x6F12, 0x4A88);
  write_cmos_sensor(0x6F12, 0x5B88);
  write_cmos_sensor(0x6F12, 0x9A42);
  write_cmos_sensor(0x6F12, 0x00D8);
  write_cmos_sensor(0x6F12, 0x1A46);
  write_cmos_sensor(0x6F12, 0xBD4B);
  write_cmos_sensor(0x6F12, 0x4A80);
  write_cmos_sensor(0x6F12, 0xFA3B);
  write_cmos_sensor(0x6F12, 0xC0F3);
  write_cmos_sensor(0x6F12, 0x0C00);
  write_cmos_sensor(0x6F12, 0x1880);
  write_cmos_sensor(0x6F12, 0x9B1C);
  write_cmos_sensor(0x6F12, 0xC2F3);
  write_cmos_sensor(0x6F12, 0x0C00);
  write_cmos_sensor(0x6F12, 0x1880);
  write_cmos_sensor(0x6F12, 0x4FF4);
  write_cmos_sensor(0x6F12, 0x8033);
  write_cmos_sensor(0x6F12, 0xB3FB);
  write_cmos_sensor(0x6F12, 0xF2F0);
  write_cmos_sensor(0x6F12, 0xB64F);
  write_cmos_sensor(0x6F12, 0x8880);
  write_cmos_sensor(0x6F12, 0xF23F);
  write_cmos_sensor(0x6F12, 0xC0F3);
  write_cmos_sensor(0x6F12, 0x0C00);
  write_cmos_sensor(0x6F12, 0x3880);
  write_cmos_sensor(0x6F12, 0xB4F8);
  write_cmos_sensor(0x6F12, 0x8A00);
  write_cmos_sensor(0x6F12, 0xA2EB);
  write_cmos_sensor(0x6F12, 0x8202);
  write_cmos_sensor(0x6F12, 0x00EB);
  write_cmos_sensor(0x6F12, 0x4200);
  write_cmos_sensor(0x6F12, 0x93FB);
  write_cmos_sensor(0x6F12, 0xF0F0);
  write_cmos_sensor(0x6F12, 0xC880);
  write_cmos_sensor(0x6F12, 0xB91C);
  write_cmos_sensor(0x6F12, 0xC0F3);
  write_cmos_sensor(0x6F12, 0x0C00);
  write_cmos_sensor(0x6F12, 0x0880);
  write_cmos_sensor(0x6F12, 0x3146);
  write_cmos_sensor(0x6F12, 0x2846);
  write_cmos_sensor(0x6F12, 0xBDE8);
  write_cmos_sensor(0x6F12, 0xF041);
  write_cmos_sensor(0x6F12, 0x0122);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x31BA);
  write_cmos_sensor(0x6F12, 0x2DE9);
  write_cmos_sensor(0x6F12, 0xF05F);
  write_cmos_sensor(0x6F12, 0x8946);
  write_cmos_sensor(0x6F12, 0xA949);
  write_cmos_sensor(0x6F12, 0x0446);
  write_cmos_sensor(0x6F12, 0x0668);
  write_cmos_sensor(0x6F12, 0xB1F8);
  write_cmos_sensor(0x6F12, 0xAE00);
  write_cmos_sensor(0x6F12, 0xC640);
  write_cmos_sensor(0x6F12, 0x012E);
  write_cmos_sensor(0x6F12, 0x00D8);
  write_cmos_sensor(0x6F12, 0x0126);
  write_cmos_sensor(0x6F12, 0xA568);
  write_cmos_sensor(0x6F12, 0x8846);
  write_cmos_sensor(0x6F12, 0xC540);
  write_cmos_sensor(0x6F12, 0x012D);
  write_cmos_sensor(0x6F12, 0x00D8);
  write_cmos_sensor(0x6F12, 0x0125);
  write_cmos_sensor(0x6F12, 0x4FF4);
  write_cmos_sensor(0x6F12, 0x805B);
  write_cmos_sensor(0x6F12, 0xE6B3);
  write_cmos_sensor(0x6F12, 0xA149);
  write_cmos_sensor(0x6F12, 0x4878);
  write_cmos_sensor(0x6F12, 0xE8B1);
  write_cmos_sensor(0x6F12, 0x99F8);
  write_cmos_sensor(0x6F12, 0x0C00);
  write_cmos_sensor(0x6F12, 0xD0B1);
  write_cmos_sensor(0x6F12, 0xB8F8);
  write_cmos_sensor(0x6F12, 0xB000);
  write_cmos_sensor(0x6F12, 0xB8B1);
  write_cmos_sensor(0x6F12, 0x208A);
  write_cmos_sensor(0x6F12, 0xD1F8);
  write_cmos_sensor(0x6F12, 0x8010);
  write_cmos_sensor(0x6F12, 0x7043);
  write_cmos_sensor(0x6F12, 0x030A);
  write_cmos_sensor(0x6F12, 0x0020);
  write_cmos_sensor(0x6F12, 0x0003);
  write_cmos_sensor(0x6F12, 0x40EA);
  write_cmos_sensor(0x6F12, 0x1150);
  write_cmos_sensor(0x6F12, 0x0903);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x1AFA);
  write_cmos_sensor(0x6F12, 0x6160);
  write_cmos_sensor(0x6F12, 0x5945);
  write_cmos_sensor(0x6F12, 0x00D8);
  write_cmos_sensor(0x6F12, 0x5946);
  write_cmos_sensor(0x6F12, 0x6160);
  write_cmos_sensor(0x6F12, 0xD8F8);
  write_cmos_sensor(0x6F12, 0xA800);
  write_cmos_sensor(0x6F12, 0x8142);
  write_cmos_sensor(0x6F12, 0x00D3);
  write_cmos_sensor(0x6F12, 0x0146);
  write_cmos_sensor(0x6F12, 0x6160);
  write_cmos_sensor(0x6F12, 0x208A);
  write_cmos_sensor(0x6F12, 0x0023);
  write_cmos_sensor(0x6F12, 0xA6FB);
  write_cmos_sensor(0x6F12, 0x0001);
  write_cmos_sensor(0x6F12, 0x000A);
  write_cmos_sensor(0x6F12, 0x40EA);
  write_cmos_sensor(0x6F12, 0x0160);
  write_cmos_sensor(0x6F12, 0x0A0A);
  write_cmos_sensor(0x6F12, 0x6168);
  write_cmos_sensor(0x6F12, 0xA1FB);
  write_cmos_sensor(0x6F12, 0x0067);
  write_cmos_sensor(0x6F12, 0x03FB);
  write_cmos_sensor(0x6F12, 0x0070);
  write_cmos_sensor(0x6F12, 0x01FB);
  write_cmos_sensor(0x6F12, 0x0201);
  write_cmos_sensor(0x6F12, 0x300B);
  write_cmos_sensor(0x6F12, 0x40EA);
  write_cmos_sensor(0x6F12, 0x0150);
  write_cmos_sensor(0x6F12, 0x0A0B);
  write_cmos_sensor(0x6F12, 0x6169);
  write_cmos_sensor(0x6F12, 0xA1FB);
  write_cmos_sensor(0x6F12, 0x0067);
  write_cmos_sensor(0x6F12, 0x03FB);
  write_cmos_sensor(0x6F12, 0x0070);
  write_cmos_sensor(0x6F12, 0x01FB);
  write_cmos_sensor(0x6F12, 0x0200);
  write_cmos_sensor(0x6F12, 0x0105);
  write_cmos_sensor(0x6F12, 0x00E0);
  write_cmos_sensor(0x6F12, 0x02E0);
  write_cmos_sensor(0x6F12, 0x41EA);
  write_cmos_sensor(0x6F12, 0x1630);
  write_cmos_sensor(0x6F12, 0xE061);
  write_cmos_sensor(0x6F12, 0x99F8);
  write_cmos_sensor(0x6F12, 0x0C00);
  write_cmos_sensor(0x6F12, 0x4FF4);
  write_cmos_sensor(0x6F12, 0x807A);
  write_cmos_sensor(0x6F12, 0x0028);
  write_cmos_sensor(0x6F12, 0x6DD0);
  write_cmos_sensor(0x6F12, 0x7E4E);
  write_cmos_sensor(0x6F12, 0x7078);
  write_cmos_sensor(0x6F12, 0xD8B1);
  write_cmos_sensor(0x6F12, 0xB8F8);
  write_cmos_sensor(0x6F12, 0xB000);
  write_cmos_sensor(0x6F12, 0x4746);
  write_cmos_sensor(0x6F12, 0xB8B1);
  write_cmos_sensor(0x6F12, 0x208A);
  write_cmos_sensor(0x6F12, 0xD6F8);
  write_cmos_sensor(0x6F12, 0x9010);
  write_cmos_sensor(0x6F12, 0x6843);
  write_cmos_sensor(0x6F12, 0x030A);
  write_cmos_sensor(0x6F12, 0x0020);
  write_cmos_sensor(0x6F12, 0x0003);
  write_cmos_sensor(0x6F12, 0x40EA);
  write_cmos_sensor(0x6F12, 0x1150);
  write_cmos_sensor(0x6F12, 0x0903);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xD5F9);
  write_cmos_sensor(0x6F12, 0xE160);
  write_cmos_sensor(0x6F12, 0x5945);
  write_cmos_sensor(0x6F12, 0x00D8);
  write_cmos_sensor(0x6F12, 0x5946);
  write_cmos_sensor(0x6F12, 0xE160);
  write_cmos_sensor(0x6F12, 0xD7F8);
  write_cmos_sensor(0x6F12, 0xA800);
  write_cmos_sensor(0x6F12, 0x8142);
  write_cmos_sensor(0x6F12, 0x00D3);
  write_cmos_sensor(0x6F12, 0x0146);
  write_cmos_sensor(0x6F12, 0xE160);
  write_cmos_sensor(0x6F12, 0xE5B1);
  write_cmos_sensor(0x6F12, 0x208A);
  write_cmos_sensor(0x6F12, 0x0023);
  write_cmos_sensor(0x6F12, 0xA5FB);
  write_cmos_sensor(0x6F12, 0x0001);
  write_cmos_sensor(0x6F12, 0x000A);
  write_cmos_sensor(0x6F12, 0x40EA);
  write_cmos_sensor(0x6F12, 0x0160);
  write_cmos_sensor(0x6F12, 0x0A0A);
  write_cmos_sensor(0x6F12, 0xE168);
  write_cmos_sensor(0x6F12, 0xA1FB);
  write_cmos_sensor(0x6F12, 0x0057);
  write_cmos_sensor(0x6F12, 0x03FB);
  write_cmos_sensor(0x6F12, 0x0070);
  write_cmos_sensor(0x6F12, 0x01FB);
  write_cmos_sensor(0x6F12, 0x0201);
  write_cmos_sensor(0x6F12, 0x280B);
  write_cmos_sensor(0x6F12, 0x40EA);
  write_cmos_sensor(0x6F12, 0x0150);
  write_cmos_sensor(0x6F12, 0x0A0B);
  write_cmos_sensor(0x6F12, 0x6169);
  write_cmos_sensor(0x6F12, 0xA1FB);
  write_cmos_sensor(0x6F12, 0x0057);
  write_cmos_sensor(0x6F12, 0x03FB);
  write_cmos_sensor(0x6F12, 0x0070);
  write_cmos_sensor(0x6F12, 0x01FB);
  write_cmos_sensor(0x6F12, 0x0200);
  write_cmos_sensor(0x6F12, 0x0105);
  write_cmos_sensor(0x6F12, 0x41EA);
  write_cmos_sensor(0x6F12, 0x1539);
  write_cmos_sensor(0x6F12, 0x0021);
  write_cmos_sensor(0x6F12, 0x0902);
  write_cmos_sensor(0x6F12, 0xC4F8);
  write_cmos_sensor(0x6F12, 0x1890);
  write_cmos_sensor(0x6F12, 0x41EA);
  write_cmos_sensor(0x6F12, 0x1960);
  write_cmos_sensor(0x6F12, 0x4FEA);
  write_cmos_sensor(0x6F12, 0x0921);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0xE369);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xA0F9);
  write_cmos_sensor(0x6F12, 0x5948);
  write_cmos_sensor(0x6F12, 0x2184);
  write_cmos_sensor(0x6F12, 0xB0F8);
  write_cmos_sensor(0x6F12, 0xFE04);
  write_cmos_sensor(0x6F12, 0x00B1);
  write_cmos_sensor(0x6F12, 0x2084);
  write_cmos_sensor(0x6F12, 0x7078);
  write_cmos_sensor(0x6F12, 0x38B1);
  write_cmos_sensor(0x6F12, 0xD6F8);
  write_cmos_sensor(0x6F12, 0x9000);
  write_cmos_sensor(0x6F12, 0x20B1);
  write_cmos_sensor(0x6F12, 0xB8F8);
  write_cmos_sensor(0x6F12, 0xB010);
  write_cmos_sensor(0x6F12, 0x09B1);
  write_cmos_sensor(0x6F12, 0x5349);
  write_cmos_sensor(0x6F12, 0x0864);
  write_cmos_sensor(0x6F12, 0x208C);
  write_cmos_sensor(0x6F12, 0x5546);
  write_cmos_sensor(0x6F12, 0x5246);
  write_cmos_sensor(0x6F12, 0x5946);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x8FF9);
  write_cmos_sensor(0x6F12, 0x80B2);
  write_cmos_sensor(0x6F12, 0x2084);
  write_cmos_sensor(0x6F12, 0x4FF4);
  write_cmos_sensor(0x6F12, 0x8031);
  write_cmos_sensor(0x6F12, 0xB1FB);
  write_cmos_sensor(0x6F12, 0xF0F0);
  write_cmos_sensor(0x6F12, 0x80B2);
  write_cmos_sensor(0x6F12, 0x6084);
  write_cmos_sensor(0x6F12, 0x2022);
  write_cmos_sensor(0x6F12, 0x2946);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x83F9);
  write_cmos_sensor(0x6F12, 0x6084);
  write_cmos_sensor(0x6F12, 0xBDE8);
  write_cmos_sensor(0x6F12, 0xF09F);
  write_cmos_sensor(0x6F12, 0x5046);
  write_cmos_sensor(0x6F12, 0xA4F8);
  write_cmos_sensor(0x6F12, 0x20A0);
  write_cmos_sensor(0x6F12, 0xF8E7);
  write_cmos_sensor(0x6F12, 0x4749);
  write_cmos_sensor(0x6F12, 0x90F8);
  write_cmos_sensor(0x6F12, 0xCA00);
  write_cmos_sensor(0x6F12, 0x464A);
  write_cmos_sensor(0x6F12, 0xC97B);
  write_cmos_sensor(0x6F12, 0x40EA);
  write_cmos_sensor(0x6F12, 0x0110);
  write_cmos_sensor(0x6F12, 0x4449);
  write_cmos_sensor(0x6F12, 0x92F8);
  write_cmos_sensor(0x6F12, 0x3924);
  write_cmos_sensor(0x6F12, 0x91F8);
  write_cmos_sensor(0x6F12, 0x3814);
  write_cmos_sensor(0x6F12, 0x0902);
  write_cmos_sensor(0x6F12, 0x41EA);
  write_cmos_sensor(0x6F12, 0x0231);
  write_cmos_sensor(0x6F12, 0x0843);
  write_cmos_sensor(0x6F12, 0x4149);
  write_cmos_sensor(0x6F12, 0x0880);
  write_cmos_sensor(0x6F12, 0x7047);
  write_cmos_sensor(0x6F12, 0x70B5);
  write_cmos_sensor(0x6F12, 0x0646);
  write_cmos_sensor(0x6F12, 0x3448);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0x8168);
  write_cmos_sensor(0x6F12, 0x0C0C);
  write_cmos_sensor(0x6F12, 0x8DB2);
  write_cmos_sensor(0x6F12, 0x2946);
  write_cmos_sensor(0x6F12, 0x2046);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x45F9);
  write_cmos_sensor(0x6F12, 0x3046);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x60F9);
  write_cmos_sensor(0x6F12, 0x3648);
  write_cmos_sensor(0x6F12, 0x90F8);
  write_cmos_sensor(0x6F12, 0xCD10);
  write_cmos_sensor(0x6F12, 0x0129);
  write_cmos_sensor(0x6F12, 0x03D9);
  write_cmos_sensor(0x6F12, 0x90F8);
  write_cmos_sensor(0x6F12, 0x8800);
  write_cmos_sensor(0x6F12, 0x0128);
  write_cmos_sensor(0x6F12, 0x07D0);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0x3248);
  write_cmos_sensor(0x6F12, 0x32B1);
  write_cmos_sensor(0x6F12, 0xB0F8);
  write_cmos_sensor(0x6F12, 0x3604);
  write_cmos_sensor(0x6F12, 0x40F4);
  write_cmos_sensor(0x6F12, 0x8072);
  write_cmos_sensor(0x6F12, 0x03E0);
  write_cmos_sensor(0x6F12, 0x0122);
  write_cmos_sensor(0x6F12, 0xF6E7);
  write_cmos_sensor(0x6F12, 0xB0F8);
  write_cmos_sensor(0x6F12, 0x3624);
  write_cmos_sensor(0x6F12, 0x2E48);
  write_cmos_sensor(0x6F12, 0x801E);
  write_cmos_sensor(0x6F12, 0x0280);
  write_cmos_sensor(0x6F12, 0x2946);
  write_cmos_sensor(0x6F12, 0x2046);
  write_cmos_sensor(0x6F12, 0xBDE8);
  write_cmos_sensor(0x6F12, 0x7040);
  write_cmos_sensor(0x6F12, 0x0122);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x23B9);
  write_cmos_sensor(0x6F12, 0x70B5);
  write_cmos_sensor(0x6F12, 0x0646);
  write_cmos_sensor(0x6F12, 0x1E48);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0xC168);
  write_cmos_sensor(0x6F12, 0x0C0C);
  write_cmos_sensor(0x6F12, 0x8DB2);
  write_cmos_sensor(0x6F12, 0x2946);
  write_cmos_sensor(0x6F12, 0x2046);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x18F9);
  write_cmos_sensor(0x6F12, 0x3046);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x38F9);
  write_cmos_sensor(0x6F12, 0x2349);
  write_cmos_sensor(0x6F12, 0x96F8);
  write_cmos_sensor(0x6F12, 0x2600);
  write_cmos_sensor(0x6F12, 0x0122);
  write_cmos_sensor(0x6F12, 0x81F8);
  write_cmos_sensor(0x6F12, 0x2600);
  write_cmos_sensor(0x6F12, 0x2946);
  write_cmos_sensor(0x6F12, 0x2046);
  write_cmos_sensor(0x6F12, 0xBDE8);
  write_cmos_sensor(0x6F12, 0x7040);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x09B9);
  write_cmos_sensor(0x6F12, 0x2DE9);
  write_cmos_sensor(0x6F12, 0xF041);
  write_cmos_sensor(0x6F12, 0x0546);
  write_cmos_sensor(0x6F12, 0x1748);
  write_cmos_sensor(0x6F12, 0x124E);
  write_cmos_sensor(0x6F12, 0x018A);
  write_cmos_sensor(0x6F12, 0x4069);
  write_cmos_sensor(0x6F12, 0x06F1);
  write_cmos_sensor(0x6F12, 0xF407);
  write_cmos_sensor(0x6F12, 0x4143);
  write_cmos_sensor(0x6F12, 0x4FEA);
  write_cmos_sensor(0x6F12, 0x1138);
  write_cmos_sensor(0x6F12, 0x0024);
  write_cmos_sensor(0x6F12, 0x06EB);
  write_cmos_sensor(0x6F12, 0xC402);
  write_cmos_sensor(0x6F12, 0xD432);
  write_cmos_sensor(0x6F12, 0x0423);
  write_cmos_sensor(0x6F12, 0x3946);
  write_cmos_sensor(0x6F12, 0x4046);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x1CF9);
  write_cmos_sensor(0x6F12, 0x25F8);
  write_cmos_sensor(0x6F12, 0x1400);
  write_cmos_sensor(0x6F12, 0x641C);
  write_cmos_sensor(0x6F12, 0x042C);
  write_cmos_sensor(0x6F12, 0xF2DB);
  write_cmos_sensor(0x6F12, 0x1148);
  write_cmos_sensor(0x6F12, 0x2988);
  write_cmos_sensor(0x6F12, 0x0180);
  write_cmos_sensor(0x6F12, 0x6988);
  write_cmos_sensor(0x6F12, 0x4180);
  write_cmos_sensor(0x6F12, 0xA988);
  write_cmos_sensor(0x6F12, 0x8180);
  write_cmos_sensor(0x6F12, 0xE988);
  write_cmos_sensor(0x6F12, 0xC180);
  write_cmos_sensor(0x6F12, 0xBDE8);
  write_cmos_sensor(0x6F12, 0xF081);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x49E0);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1DE0);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x7B00);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0xD604);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1DC0);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1B50);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x35F0);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x34D0);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1FE0);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x14F0);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0x9B06);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1E20);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0xD22E);
  write_cmos_sensor(0x6F12, 0x70B5);
  write_cmos_sensor(0x6F12, 0x0122);
  write_cmos_sensor(0x6F12, 0xAFF2);
  write_cmos_sensor(0x6F12, 0x2341);
  write_cmos_sensor(0x6F12, 0x4E48);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xF0F8);
  write_cmos_sensor(0x6F12, 0x4E4E);
  write_cmos_sensor(0x6F12, 0x0122);
  write_cmos_sensor(0x6F12, 0xAFF2);
  write_cmos_sensor(0x6F12, 0x6931);
  write_cmos_sensor(0x6F12, 0x3060);
  write_cmos_sensor(0x6F12, 0x4C48);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xE8F8);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0xAFF2);
  write_cmos_sensor(0x6F12, 0x5511);
  write_cmos_sensor(0x6F12, 0x7060);
  write_cmos_sensor(0x6F12, 0x4A48);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xE1F8);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0xAFF2);
  write_cmos_sensor(0x6F12, 0x3B11);
  write_cmos_sensor(0x6F12, 0x4848);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xDBF8);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0xAFF2);
  write_cmos_sensor(0x6F12, 0x0931);
  write_cmos_sensor(0x6F12, 0xB060);
  write_cmos_sensor(0x6F12, 0x4548);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xD4F8);
  write_cmos_sensor(0x6F12, 0x454C);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x6050);
  write_cmos_sensor(0x6F12, 0xE18C);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xD3F8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x434D);
  write_cmos_sensor(0x6F12, 0x4249);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0x81B2);
  write_cmos_sensor(0x6F12, 0xE184);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x0C50);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xC7F8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x3E49);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0x81B2);
  write_cmos_sensor(0x6F12, 0xE184);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x1050);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xBCF8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x3A49);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0x81B2);
  write_cmos_sensor(0x6F12, 0xE184);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x2060);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xB1F8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x3549);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0x81B2);
  write_cmos_sensor(0x6F12, 0xE184);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x2460);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0xA6F8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x3149);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0x81B2);
  write_cmos_sensor(0x6F12, 0xE184);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x2860);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x9BF8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x2C49);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0x81B2);
  write_cmos_sensor(0x6F12, 0xE184);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x2C60);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x90F8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x2849);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0x81B2);
  write_cmos_sensor(0x6F12, 0xE184);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x3060);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x85F8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x2349);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0x81B2);
  write_cmos_sensor(0x6F12, 0xE184);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x6070);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x7AF8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x1F49);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0x81B2);
  write_cmos_sensor(0x6F12, 0xE184);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0x6470);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x6FF8);
  write_cmos_sensor(0x6F12, 0xE08C);
  write_cmos_sensor(0x6F12, 0x1A49);
  write_cmos_sensor(0x6F12, 0x45F8);
  write_cmos_sensor(0x6F12, 0x2010);
  write_cmos_sensor(0x6F12, 0x401C);
  write_cmos_sensor(0x6F12, 0xE084);
  write_cmos_sensor(0x6F12, 0x0122);
  write_cmos_sensor(0x6F12, 0xAFF2);
  write_cmos_sensor(0x6F12, 0xDD11);
  write_cmos_sensor(0x6F12, 0x1748);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x5EF8);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0xAFF2);
  write_cmos_sensor(0x6F12, 0xB511);
  write_cmos_sensor(0x6F12, 0xF060);
  write_cmos_sensor(0x6F12, 0xBDE8);
  write_cmos_sensor(0x6F12, 0x7040);
  write_cmos_sensor(0x6F12, 0x1448);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x55B8);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x4427);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x49E0);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x40F3);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x2A01);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x2AF5);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0xC129);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1FA0);
  write_cmos_sensor(0x6F12, 0x14BF);
  write_cmos_sensor(0x6F12, 0x0320);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x4290);
  write_cmos_sensor(0x6F12, 0x0026);
  write_cmos_sensor(0x6F12, 0x4FF0);
  write_cmos_sensor(0x6F12, 0x0109);
  write_cmos_sensor(0x6F12, 0x0020);
  write_cmos_sensor(0x6F12, 0x02A8);
  write_cmos_sensor(0x6F12, 0xDDF8);
  write_cmos_sensor(0x6F12, 0x08A0);
  write_cmos_sensor(0x6F12, 0x0098);
  write_cmos_sensor(0x6F12, 0x00BF);
  write_cmos_sensor(0x6F12, 0x0690);
  write_cmos_sensor(0x6F12, 0x0498);
  write_cmos_sensor(0x6F12, 0xAFF3);
  write_cmos_sensor(0x6F12, 0x0080);
  write_cmos_sensor(0x6F12, 0x0590);
  write_cmos_sensor(0x6F12, 0x009D);
  write_cmos_sensor(0x6F12, 0xDDF8);
  write_cmos_sensor(0x6F12, 0x0870);
  write_cmos_sensor(0x6F12, 0x0026);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x69F3);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x2711);
  write_cmos_sensor(0x6F12, 0x40F2);
  write_cmos_sensor(0x6F12, 0xA77C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x44F2);
  write_cmos_sensor(0x6F12, 0x274C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x44F2);
  write_cmos_sensor(0x6F12, 0xE12C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x44F2);
  write_cmos_sensor(0x6F12, 0xF30C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x4DF2);
  write_cmos_sensor(0x6F12, 0x8D3C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x40F2);
  write_cmos_sensor(0x6F12, 0x277C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x42F6);
  write_cmos_sensor(0x6F12, 0xF52C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x46F6);
  write_cmos_sensor(0x6F12, 0xF31C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x40F2);
  write_cmos_sensor(0x6F12, 0xD57C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x4CF6);
  write_cmos_sensor(0x6F12, 0x6B7C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x4CF6);
  write_cmos_sensor(0x6F12, 0x0B7C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x30D2);
  write_cmos_sensor(0x6F12, 0x028B);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x04C7);
  write_cmos_sensor(0x602A, 0x1B50);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x602A, 0x1B56);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x602A, 0x1B52);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x602A, 0x1B64);
  write_cmos_sensor(0x6F12, 0x0800);
  write_cmos_sensor(0x602A, 0x1BE0);
  write_cmos_sensor(0x6F12, 0x00D0);
  write_cmos_sensor(0x602A, 0x1BE4);
  write_cmos_sensor(0x6F12, 0x0110);
  write_cmos_sensor(0x602A, 0x1AC4);
  write_cmos_sensor_8(0x6F12, 0x01);
  write_cmos_sensor(0x602A, 0x192D);
  write_cmos_sensor_8(0x6F12, 0x01);
  write_cmos_sensor(0x602A, 0x14F0);
  write_cmos_sensor(0x6F12, 0x0040);
  write_cmos_sensor(0x6F12, 0x0040);
  write_cmos_sensor(0x602A, 0x195E);
  write_cmos_sensor(0x6F12, 0x99BF);
  write_cmos_sensor(0x602A, 0x19B6);
  write_cmos_sensor(0x6F12, 0x99BF);
  write_cmos_sensor(0x602A, 0x1A0E);
  write_cmos_sensor(0x6F12, 0x99BF);
  write_cmos_sensor(0x602A, 0x1A66);
  write_cmos_sensor(0x6F12, 0x99BF);
  write_cmos_sensor(0x602A, 0x14FA);
  write_cmos_sensor_8(0x6F12, 0x0F);
  write_cmos_sensor(0x602A, 0x7B00);
  write_cmos_sensor(0x6F12, 0x0128);
  write_cmos_sensor(0x6F12, 0x00DC);
  write_cmos_sensor(0x6F12, 0x5590);
  write_cmos_sensor(0x6F12, 0x3644);
  write_cmos_sensor(0x602A, 0x7BD4);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0x3000);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1000);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0x3000);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1000);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0x3000);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1000);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0x3000);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1000);
  write_cmos_sensor(0x6F12, 0x0100);
  write_cmos_sensor(0x6F12, 0x0200);
  write_cmos_sensor(0x6F12, 0x0400);
  write_cmos_sensor(0x6F12, 0x0800);
  write_cmos_sensor(0x6028, 0x4000);
  write_cmos_sensor_8(0x0B04, 0x01);
  write_cmos_sensor(0x3B22, 0x1110);
  write_cmos_sensor(0x3BB2, 0x0040);
  write_cmos_sensor(0x3BDC, 0x0700);
  write_cmos_sensor(0x3BDE, 0x0700);
  write_cmos_sensor(0x3BE6, 0x0700);
  write_cmos_sensor(0x327A, 0x0380);
  write_cmos_sensor(0xF42E, 0x200C);
  write_cmos_sensor_8(0x3B0D, 0xFF);
  write_cmos_sensor(0xF49E, 0x004C);
  write_cmos_sensor(0x3A74, 0x08);
  write_cmos_sensor(0xF4A6, 0x00F0);
  write_cmos_sensor(0x3AFA, 0xFBB8);
  write_cmos_sensor(0xF49C, 0x0000);
  write_cmos_sensor(0xF496, 0x0000);
  write_cmos_sensor(0xF476, 0x0040);
  write_cmos_sensor_8(0x3A86, 0x02);
  write_cmos_sensor_8(0x3A92, 0x06);
  write_cmos_sensor_8(0x3AAA, 0x02);
  write_cmos_sensor(0x3AFE, 0x07DF);
  write_cmos_sensor(0xF47A, 0x001B);
  write_cmos_sensor(0xF462, 0x0003);
  write_cmos_sensor(0xF460, 0x0020);
  write_cmos_sensor(0x3B06, 0x000E);
  write_cmos_sensor(0x3AD0, 0x0080);
  write_cmos_sensor(0x3B02, 0x0020);
  write_cmos_sensor(0xF468, 0x0001);
  write_cmos_sensor(0xF494, 0x000E);
  write_cmos_sensor(0xF40C, 0x2180);
  write_cmos_sensor_8(0x3A7A, 0x0F);
  write_cmos_sensor_8(0x3A7B, 0x0F);
  write_cmos_sensor_8(0x3A7C, 0x0F);
  write_cmos_sensor_8(0x3A7D, 0x0F);
  write_cmos_sensor_8(0x3A80, 0x30);
  write_cmos_sensor_8(0x3A81, 0x30);
  write_cmos_sensor_8(0x3A82, 0x30);
  write_cmos_sensor_8(0x3A83, 0x30);
  write_cmos_sensor_8(0x3005, 0x04);
  write_cmos_sensor_8(0x3A5F, 0x02);
  write_cmos_sensor(0x32C4, 0x0002);
  write_cmos_sensor(0x32CA, 0x022B);
  write_cmos_sensor(0x32D0, 0x0001);
  write_cmos_sensor(0x32D6, 0x022B);
  write_cmos_sensor(0x32DC, 0x0003);
  write_cmos_sensor(0x32E2, 0x0010);
  write_cmos_sensor(0x32E8, 0x00DA);
  write_cmos_sensor(0x32EE, 0x010E);
  write_cmos_sensor(0x3864, 0x0075);
  write_cmos_sensor(0x386A, 0x0056);
  write_cmos_sensor(0x3870, 0x004C);
  write_cmos_sensor(0x3876, 0x0011);
  write_cmos_sensor(0x387C, 0x0073);
  write_cmos_sensor(0x3882, 0x0058);
  write_cmos_sensor(0x3888, 0x0048);
  write_cmos_sensor(0x388E, 0x0014);
  write_cmos_sensor(0x3894, 0x0073);
  write_cmos_sensor(0x389A, 0x0058);
  write_cmos_sensor(0x38A0, 0x0048);
  write_cmos_sensor(0x38A6, 0x0014);
  write_cmos_sensor(0x38AC, 0x0014);
  write_cmos_sensor(0x32F4, 0x00D4);
  write_cmos_sensor(0x32FA, 0x022F);
  write_cmos_sensor(0x3300, 0x00DA);
  write_cmos_sensor(0x3306, 0x022D);
  write_cmos_sensor(0x330C, 0x00D4);
  write_cmos_sensor(0x3312, 0x012E);
  write_cmos_sensor(0x3318, 0x0003);
  write_cmos_sensor(0x331E, 0x0057);
  write_cmos_sensor(0x3324, 0x0003);
  write_cmos_sensor(0x332A, 0x0059);
  write_cmos_sensor(0x3330, 0x0003);
  write_cmos_sensor(0x3336, 0x004E);
  write_cmos_sensor(0x333C, 0x0003);
  write_cmos_sensor(0x3342, 0x0052);
  write_cmos_sensor(0x3348, 0x0003);
  write_cmos_sensor(0x334E, 0x00D4);
  write_cmos_sensor(0x3354, 0x0003);
  write_cmos_sensor(0x335A, 0x000B);
  write_cmos_sensor(0x3360, 0x010E);
  write_cmos_sensor(0x3366, 0x0128);
  write_cmos_sensor(0x336C, 0x0003);
  write_cmos_sensor(0x3372, 0x0005);
  write_cmos_sensor(0x3378, 0x010E);
  write_cmos_sensor(0x337E, 0x0110);
  write_cmos_sensor(0x3384, 0x005A);
  write_cmos_sensor(0x338A, 0x0063);
  write_cmos_sensor(0x3390, 0x0234);
  write_cmos_sensor(0x3396, 0x0064);
  write_cmos_sensor(0x339C, 0x022D);
  write_cmos_sensor(0x33A2, 0x0005);
  write_cmos_sensor(0x33A8, 0x0003);
  write_cmos_sensor(0x33AE, 0x000C);
  write_cmos_sensor(0x33B4, 0x0063);
  write_cmos_sensor(0x33BA, 0x009D);
  write_cmos_sensor(0x33C0, 0x00D4);
  write_cmos_sensor(0x33C6, 0x018B);
  write_cmos_sensor(0x33CC, 0x022B);
  write_cmos_sensor(0x33D2, 0x00D7);
  write_cmos_sensor(0x33D8, 0x00EE);
  write_cmos_sensor(0x33DE, 0x00DE);
  write_cmos_sensor(0x33E4, 0x00F6);
  write_cmos_sensor(0x33EA, 0x00E6);
  write_cmos_sensor(0x33F0, 0x00F6);
  write_cmos_sensor(0x33F6, 0x00D7);
  write_cmos_sensor(0x33FC, 0x00D9);
  write_cmos_sensor(0x3402, 0x0001);
  write_cmos_sensor(0x3408, 0x0005);
  write_cmos_sensor(0x340E, 0x00DE);
  write_cmos_sensor(0x3414, 0x00F6);
  write_cmos_sensor(0x341A, 0x0001);
  write_cmos_sensor(0x3420, 0x0005);
  write_cmos_sensor(0x3426, 0x00D7);
  write_cmos_sensor(0x342C, 0x00D9);
  write_cmos_sensor(0x3432, 0x009D);
  write_cmos_sensor(0x3438, 0x00AF);
  write_cmos_sensor(0x343E, 0x00BF);
  write_cmos_sensor(0x3444, 0x00D4);
  write_cmos_sensor(0x344A, 0x018B);
  write_cmos_sensor(0x3450, 0x01D2);
  write_cmos_sensor(0x3456, 0x01E2);
  write_cmos_sensor(0x345C, 0x022B);
  write_cmos_sensor(0x3462, 0x00B1);
  write_cmos_sensor(0x3468, 0x00B6);
  write_cmos_sensor(0x346E, 0x00DA);
  write_cmos_sensor(0x3474, 0x00E0);
  write_cmos_sensor(0x347A, 0x00EB);
  write_cmos_sensor(0x3480, 0x00F1);
  write_cmos_sensor(0x3486, 0x01D4);
  write_cmos_sensor(0x348C, 0x01D9);
  write_cmos_sensor(0x3492, 0x022F);
  write_cmos_sensor(0x3498, 0x0234);
  write_cmos_sensor(0x349E, 0x00B3);
  write_cmos_sensor(0x34A4, 0x00B7);
  write_cmos_sensor(0x34AA, 0x00DC);
  write_cmos_sensor(0x34B0, 0x00E1);
  write_cmos_sensor(0x34B6, 0x00ED);
  write_cmos_sensor(0x34BC, 0x00F2);
  write_cmos_sensor(0x34C2, 0x01D6);
  write_cmos_sensor(0x34C8, 0x01DA);
  write_cmos_sensor(0x34CE, 0x0231);
  write_cmos_sensor(0x34D4, 0x0235);
  write_cmos_sensor(0x34DA, 0x00B4);
  write_cmos_sensor(0x34E0, 0x00B7);
  write_cmos_sensor(0x34E6, 0x00DD);
  write_cmos_sensor(0x34EC, 0x00E1);
  write_cmos_sensor(0x34F2, 0x00EE);
  write_cmos_sensor(0x34F8, 0x00F2);
  write_cmos_sensor(0x34FE, 0x01D7);
  write_cmos_sensor(0x3504, 0x01DA);
  write_cmos_sensor(0x350A, 0x0232);
  write_cmos_sensor(0x3510, 0x0235);
  write_cmos_sensor(0x3516, 0x00B2);
  write_cmos_sensor(0x351C, 0x00B4);
  write_cmos_sensor(0x3522, 0x00D7);
  write_cmos_sensor(0x3528, 0x00D9);
  write_cmos_sensor(0x352E, 0x00E8);
  write_cmos_sensor(0x3534, 0x00EA);
  write_cmos_sensor(0x353A, 0x01D5);
  write_cmos_sensor(0x3540, 0x01D7);
  write_cmos_sensor(0x3546, 0x022D);
  write_cmos_sensor(0x354C, 0x022F);
  write_cmos_sensor(0x3552, 0x0001);
  write_cmos_sensor(0x3558, 0x0005);
  write_cmos_sensor(0x355E, 0x00B6);
  write_cmos_sensor(0x3564, 0x00B9);
  write_cmos_sensor(0x356A, 0x00DC);
  write_cmos_sensor(0x3570, 0x00E1);
  write_cmos_sensor(0x3576, 0x00ED);
  write_cmos_sensor(0x357C, 0x00F2);
  write_cmos_sensor(0x3582, 0x01D9);
  write_cmos_sensor(0x3588, 0x01DC);
  write_cmos_sensor(0x358E, 0x0231);
  write_cmos_sensor(0x3594, 0x0235);
  write_cmos_sensor(0x359A, 0x0001);
  write_cmos_sensor(0x35A0, 0x0005);
  write_cmos_sensor(0x35A6, 0x00B2);
  write_cmos_sensor(0x35AC, 0x00B4);
  write_cmos_sensor(0x35B2, 0x00D7);
  write_cmos_sensor(0x35C4, 0x00D9);
  write_cmos_sensor(0x35CA, 0x00E8);
  write_cmos_sensor(0x35D0, 0x00EA);
  write_cmos_sensor(0x35D6, 0x01D5);
  write_cmos_sensor(0x35DC, 0x01D7);
  write_cmos_sensor(0x35E2, 0x022D);
  write_cmos_sensor(0x35E8, 0x022F);
  write_cmos_sensor(0x35EE, 0x00B6);
  write_cmos_sensor(0x35F4, 0x00DC);
  write_cmos_sensor(0x35FA, 0x01D9);
  write_cmos_sensor(0x3600, 0x022F);
  write_cmos_sensor(0x3606, 0x009C);
  write_cmos_sensor(0x360C, 0x00D6);
  write_cmos_sensor(0x3612, 0x018A);
  write_cmos_sensor(0x3618, 0x022C);
  write_cmos_sensor(0x361E, 0x0001);
  write_cmos_sensor(0x3624, 0x0005);
  write_cmos_sensor(0x362A, 0x00D7);
  write_cmos_sensor(0x3630, 0x00EE);
  write_cmos_sensor(0x3636, 0x00EE);
  write_cmos_sensor(0x363C, 0x00EE);
  write_cmos_sensor(0x3642, 0x00D7);
  write_cmos_sensor(0x3648, 0x00D9);
  write_cmos_sensor(0x364E, 0x00D9);
  write_cmos_sensor(0x3654, 0x00D9);
  write_cmos_sensor(0x365A, 0x003C);
  write_cmos_sensor(0x3660, 0x000A);
  write_cmos_sensor(0x3666, 0x0063);
  write_cmos_sensor(0x366C, 0x0231);
  write_cmos_sensor(0x3672, 0x0064);
  write_cmos_sensor(0x3678, 0x0066);
  write_cmos_sensor(0x367E, 0x00D6);
  write_cmos_sensor(0x3684, 0x00D9);
  write_cmos_sensor(0x368A, 0x022D);
  write_cmos_sensor(0x3690, 0x0230);
  write_cmos_sensor(0x3696, 0x00D6);
  write_cmos_sensor(0x369C, 0x0002);
  write_cmos_sensor(0x36A2, 0x0063);
  write_cmos_sensor(0x36A8, 0x009F);
  write_cmos_sensor(0x36AE, 0x00D6);
  write_cmos_sensor(0x36B4, 0x018D);
  write_cmos_sensor(0x36BA, 0x022D);
  write_cmos_sensor(0x36C0, 0x0000);
  write_cmos_sensor(0x36C6, 0x0000);
  write_cmos_sensor(0x36CC, 0x006E);
  write_cmos_sensor(0x36D2, 0x0234);
  write_cmos_sensor(0x36D8, 0x0000);
  write_cmos_sensor(0x36DE, 0x0000);
  write_cmos_sensor(0x36E4, 0x0000);
  write_cmos_sensor(0x36EA, 0x0000);
  write_cmos_sensor(0x36F0, 0x0000);
  write_cmos_sensor(0x36F6, 0x0000);
  write_cmos_sensor(0x36FC, 0x0000);
  write_cmos_sensor(0x3702, 0x0000);
  write_cmos_sensor(0x3708, 0x0000);
  write_cmos_sensor(0x370E, 0x0000);
  write_cmos_sensor(0x3714, 0x0000);
  write_cmos_sensor(0x371A, 0x0000);
  write_cmos_sensor(0x3720, 0x0000);
  write_cmos_sensor(0x3726, 0x0000);
  write_cmos_sensor(0x372C, 0x0000);
  write_cmos_sensor(0x3732, 0x0000);
  write_cmos_sensor(0x3738, 0x0000);
  write_cmos_sensor(0x373E, 0x0000);
  write_cmos_sensor(0x3744, 0x0065);
  write_cmos_sensor(0x374A, 0x0232);
  write_cmos_sensor(0x3750, 0x0000);
  write_cmos_sensor(0x3756, 0x0000);
  write_cmos_sensor(0x375C, 0x0000);
  write_cmos_sensor(0x3762, 0x0000);
  write_cmos_sensor(0x3768, 0x0000);
  write_cmos_sensor(0x376E, 0x0000);
  write_cmos_sensor(0x3774, 0x0000);
  write_cmos_sensor(0x377A, 0x0000);
  write_cmos_sensor(0x3780, 0x0000);
  write_cmos_sensor(0x3786, 0x0000);
  write_cmos_sensor(0x378C, 0x0000);
  write_cmos_sensor(0x3792, 0x0000);
  write_cmos_sensor(0x3798, 0x0000);
  write_cmos_sensor(0x379E, 0x0000);
  write_cmos_sensor(0x37A4, 0x0000);
  write_cmos_sensor(0x37AA, 0x0000);
  write_cmos_sensor(0x38B2, 0x000A);
  write_cmos_sensor(0x38C4, 0x0002);
  write_cmos_sensor(0x37B0, 0x0008);
  write_cmos_sensor(0x37B6, 0x0010);
  write_cmos_sensor(0x37BC, 0x0020);
  write_cmos_sensor(0x37C2, 0x0028);
  write_cmos_sensor(0x37C8, 0x0038);
  write_cmos_sensor(0x37CE, 0x0040);
  write_cmos_sensor(0x37D4, 0x0050);
  write_cmos_sensor(0x37DA, 0x0058);
  write_cmos_sensor(0x37E0, 0x0068);
  write_cmos_sensor(0x37E6, 0x0070);
  write_cmos_sensor(0x37EC, 0x0080);
  write_cmos_sensor(0x37F2, 0x0088);
  write_cmos_sensor(0x37F8, 0x0098);
  write_cmos_sensor(0x37FE, 0x00A0);
  write_cmos_sensor(0x3804, 0x00B0);
  write_cmos_sensor(0x380A, 0x00B8);
  write_cmos_sensor(0x3810, 0x00F0);
  write_cmos_sensor(0x3816, 0x00F8);
  write_cmos_sensor(0x381C, 0x0108);
  write_cmos_sensor(0x3822, 0x0110);
  write_cmos_sensor(0x3828, 0x0017);
  write_cmos_sensor(0x382E, 0x002F);
  write_cmos_sensor(0x3834, 0x0047);
  write_cmos_sensor(0x383A, 0x005F);
  write_cmos_sensor(0x3840, 0x0077);
  write_cmos_sensor(0x3846, 0x008F);
  write_cmos_sensor(0x384C, 0x00A7);
  write_cmos_sensor(0x3852, 0x00ED);
  write_cmos_sensor(0x3858, 0x00FF);
  write_cmos_sensor(0x385E, 0x0117);
  write_cmos_sensor(0x38CA, 0x0136);
  write_cmos_sensor(0x38D0, 0x0170);
  write_cmos_sensor(0x38D6, 0x028D);
  write_cmos_sensor(0x38DC, 0x0000);
  write_cmos_sensor(0x38E2, 0x0062);
  write_cmos_sensor(0x38E8, 0x023E);
  write_cmos_sensor(0x38EE, 0x001F);
  write_cmos_sensor(0x38F4, 0x0136);
  write_cmos_sensor(0x38FA, 0x0170);
  write_cmos_sensor(0x3900, 0x028D);
  write_cmos_sensor(0x3906, 0x0000);
  write_cmos_sensor(0x390C, 0x0062);
  write_cmos_sensor(0x3912, 0x023E);
  write_cmos_sensor(0x3918, 0x001F);
  write_cmos_sensor(0x391E, 0x0136);
  write_cmos_sensor(0x3924, 0x0172);
  write_cmos_sensor(0x392A, 0x028D);
  write_cmos_sensor(0x3930, 0x0000);
  write_cmos_sensor(0x3936, 0x0062);
  write_cmos_sensor(0x393C, 0x023E);
  write_cmos_sensor(0x3942, 0x001F);
  write_cmos_sensor(0x3948, 0x0136);
  write_cmos_sensor(0x394E, 0x0172);
  write_cmos_sensor(0x3954, 0x028D);
  write_cmos_sensor(0x395A, 0x0000);
  write_cmos_sensor(0x3960, 0x0062);
  write_cmos_sensor(0x3966, 0x023E);
  write_cmos_sensor(0x396C, 0x001F);
  write_cmos_sensor_8(0x3A03, 0x03);
  write_cmos_sensor_8(0x3A04, 0x03);
  write_cmos_sensor_8(0x3A07, 0x03);
  write_cmos_sensor_8(0x3A08, 0x03);
  write_cmos_sensor_8(0x3993, 0x00);
  write_cmos_sensor_8(0x3994, 0x06);
  write_cmos_sensor_8(0x3995, 0x00);
  write_cmos_sensor_8(0x3993, 0x00);
  write_cmos_sensor_8(0x3A16, 0x00);
  write_cmos_sensor_8(0x3A17, 0x00);
  write_cmos_sensor_8(0x3A18, 0x00);
  write_cmos_sensor_8(0x3A19, 0x03);
  write_cmos_sensor_8(0x3A1A, 0x03);
  write_cmos_sensor_8(0x3A1B, 0x06);
  write_cmos_sensor_8(0x3A1E, 0x00);
  write_cmos_sensor_8(0x3A1F, 0x06);
  write_cmos_sensor_8(0x3A32, 0x00);
  write_cmos_sensor_8(0x3A33, 0x06);
  write_cmos_sensor(0x9804, 0x0068);
  write_cmos_sensor(0x9806, 0x10E8);
  write_cmos_sensor(0x9808, 0x002E);
  write_cmos_sensor(0x980A, 0x0C8E);
  write_cmos_sensor(0x623E, 0x0004);

  	mdelay(2);
  // Stream On
  //write_cmos_sensor(0x602A, 0x0100);
  //write_cmos_sensor_8(0x6F12, 0x01);

//  del by yfx
//	otp_wb_update();//vivo zcw++ 20140910
//	otp_lsc_update();

    mdelay(5);//vivo zcw++ 20141027 Add for prevent WAIT_IRQ timeout	
    LOG_INF("Exit\n");
  //s 2014/12/01, bruce
  } else if (chip_id == 0xD101) {
    LOG_INF("-- sensor_init, chip id = 0xD101\n");
  write_cmos_sensor(0x6028, 0x4000);
  write_cmos_sensor(0x6214, 0x7971);
  write_cmos_sensor(0x6218, 0x0100);
  //SW page
  write_cmos_sensor(0x6028, 0x2000);
  write_cmos_sensor(0x602A, 0x448C);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0448);
  write_cmos_sensor(0x6F12, 0x0349);
  write_cmos_sensor(0x6F12, 0x0160);
  write_cmos_sensor(0x6F12, 0xC26A);
  write_cmos_sensor(0x6F12, 0x511A);
  write_cmos_sensor(0x6F12, 0x8180);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x2CB8);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x4538);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1FA0);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x2DE9);
  write_cmos_sensor(0x6F12, 0xF041);
  write_cmos_sensor(0x6F12, 0x0546);
  write_cmos_sensor(0x6F12, 0x1348);
  write_cmos_sensor(0x6F12, 0x134E);
  write_cmos_sensor(0x6F12, 0x018A);
  write_cmos_sensor(0x6F12, 0x4069);
  write_cmos_sensor(0x6F12, 0x06F1);
  write_cmos_sensor(0x6F12, 0x2007);
  write_cmos_sensor(0x6F12, 0x4143);
  write_cmos_sensor(0x6F12, 0x4FEA);
  write_cmos_sensor(0x6F12, 0x1138);
  write_cmos_sensor(0x6F12, 0x0024);
  write_cmos_sensor(0x6F12, 0x06EB);
  write_cmos_sensor(0x6F12, 0xC402);
  write_cmos_sensor(0x6F12, 0x0423);
  write_cmos_sensor(0x6F12, 0x3946);
  write_cmos_sensor(0x6F12, 0x4046);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x1EF8);
  write_cmos_sensor(0x6F12, 0x25F8);
  write_cmos_sensor(0x6F12, 0x1400);
  write_cmos_sensor(0x6F12, 0x641C);
  write_cmos_sensor(0x6F12, 0x042C);
  write_cmos_sensor(0x6F12, 0xF3DB);
  write_cmos_sensor(0x6F12, 0x0A48);
  write_cmos_sensor(0x6F12, 0x2988);
  write_cmos_sensor(0x6F12, 0x0180);
  write_cmos_sensor(0x6F12, 0x6988);
  write_cmos_sensor(0x6F12, 0x4180);
  write_cmos_sensor(0x6F12, 0xA988);
  write_cmos_sensor(0x6F12, 0x8180);
  write_cmos_sensor(0x6F12, 0xE988);
  write_cmos_sensor(0x6F12, 0xC180);
  write_cmos_sensor(0x6F12, 0xBDE8);
  write_cmos_sensor(0x6F12, 0xF081);
  write_cmos_sensor(0x6F12, 0x0022);
  write_cmos_sensor(0x6F12, 0xAFF2);
  write_cmos_sensor(0x6F12, 0x4B01);
  write_cmos_sensor(0x6F12, 0x0448);
  write_cmos_sensor(0x6F12, 0x00F0);
  write_cmos_sensor(0x6F12, 0x0DB8);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x34D0);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x7900);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0xD22E);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x2941);
  write_cmos_sensor(0x6F12, 0x40F2);
  write_cmos_sensor(0x6F12, 0xFD7C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x4DF2);
  write_cmos_sensor(0x6F12, 0x474C);
  write_cmos_sensor(0x6F12, 0xC0F2);
  write_cmos_sensor(0x6F12, 0x000C);
  write_cmos_sensor(0x6F12, 0x6047);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x30D2);
  write_cmos_sensor(0x6F12, 0x029C);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x6F12, 0x0001);
  write_cmos_sensor(0x602A, 0x7900);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0x3000);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1000);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0x3000);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1000);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0x3000);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1000);
  write_cmos_sensor(0x6F12, 0x4000);
  write_cmos_sensor(0x6F12, 0x3000);
  write_cmos_sensor(0x6F12, 0x2000);
  write_cmos_sensor(0x6F12, 0x1000);
  write_cmos_sensor(0x6F12, 0x0100);
  write_cmos_sensor(0x6F12, 0x0200);
  write_cmos_sensor(0x6F12, 0x0400);
  write_cmos_sensor(0x6F12, 0x0800);
  write_cmos_sensor(0x602A, 0x43F0);
  write_cmos_sensor(0x6F12, 0x0128);
  write_cmos_sensor(0x6F12, 0x00DC);
  write_cmos_sensor(0x6F12, 0x5590);
  write_cmos_sensor(0x6F12, 0x3644);
  write_cmos_sensor(0x602A, 0x1B50);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x602A, 0x1B54);
  write_cmos_sensor(0x6F12, 0x0000);
  write_cmos_sensor(0x602A, 0x1B64);
  write_cmos_sensor(0x6F12, 0x0800);
  write_cmos_sensor(0x602A, 0x1926);
  write_cmos_sensor(0x6F12, 0x0011);
  write_cmos_sensor(0x602A, 0x14FA);
  write_cmos_sensor_8(0x6F12, 0x0F);
  write_cmos_sensor(0x602A, 0x4473);
  write_cmos_sensor_8(0x6F12, 0x02);
	//Global  
  write_cmos_sensor(0x6028, 0x4000);
  write_cmos_sensor_8(0x0B04, 0x01);
  write_cmos_sensor(0x3B22, 0x1110);
  write_cmos_sensor(0xF42E, 0x200C);
  write_cmos_sensor(0xF49E, 0x004C);
  write_cmos_sensor(0xF4A6, 0x00F0);
  write_cmos_sensor(0x3AFA, 0xFBB8);
  write_cmos_sensor(0xF49C, 0x0000);
  write_cmos_sensor(0xF496, 0x0000);
  write_cmos_sensor(0xF476, 0x0040);
  write_cmos_sensor_8(0x3AAA, 0x02);
  write_cmos_sensor(0x3AFE, 0x07DF);
  write_cmos_sensor(0xF47A, 0x001B);
  write_cmos_sensor(0xF462, 0x0003);
  write_cmos_sensor(0xF460, 0x0020);
  write_cmos_sensor(0x3B06, 0x000E);
  write_cmos_sensor(0x3AD0, 0x0080);
  write_cmos_sensor(0x3B02, 0x0020);
  write_cmos_sensor(0xF468, 0x0001);
  write_cmos_sensor(0xF494, 0x000E);
  write_cmos_sensor(0xF40C, 0x2180);
  write_cmos_sensor(0x3870, 0x004C);
  write_cmos_sensor(0x3876, 0x0011);
  write_cmos_sensor(0x3366, 0x0128);
  write_cmos_sensor(0x3852, 0x00EA);
  write_cmos_sensor(0x623E, 0x0004);
  write_cmos_sensor(0x3B5C, 0x0006);
  } else { // all other ID
    LOG_INF("-- sensor_init, Read back other chip id = 0x%x\n", chip_id);
  }
  //e 2014/12/01, bruce
  S5k3M2_2ND_MIPI_update_wb_register_from_otp();
}	/*	sensor_init  */
/**********************************************************************************************************************/
//$MIPI[Width:2096,Height:1552,Format:RAW10,Lane:4,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:4,DataRate:1260,useEmbData:0]
//$MV1[MCLK:24,Width:2096,Height:1552,Format:MIPI_RAW10,mipi_lane:4,mipi_datarate:1260,pvi_pclk_inverse:0]
//=====================================================
// 3M2XXM
// 2X2 Binning Normal Mode
// X_output size : 2096
// Y_output size : 1552
// Frame_rate : 30.06 fps
// Output_format RAW 10
// Output_lanes 4
// Output_clock_mhz : 1272 Mhz
// System_clock_mhz : 440 Mhz
// Input_clock_mhz : 24 Mhz
// TnP R651
//=====================================================
/**********************************************************************************************************************/
static void preview_setting(void)
{
        // s 2014/12/01, bruce
	kal_uint16 chip_id = 0;

        chip_id = read_cmos_sensor(0x0002);
	
	if (chip_id == 0xC001) {
		LOG_INF("--preview_setting chip_id = 0xC001\n");
        // e 2014/12/01, bruce
	LOG_INF("hesong 3 Preview E! ");
	//p200

write_cmos_sensor(0x6028,0x4000);
write_cmos_sensor(0x32CA,0x022B);
write_cmos_sensor(0x32D6,0x022B);
write_cmos_sensor(0x0344,0x000C);
write_cmos_sensor(0x0346,0x0008);
write_cmos_sensor(0x0348,0x1073);
write_cmos_sensor(0x034A,0x0C37);
write_cmos_sensor(0x034C,0x0830);
write_cmos_sensor(0x034E,0x0610);
write_cmos_sensor_8(0x0901,0x12);
write_cmos_sensor(0x0380,0x0001);
write_cmos_sensor(0x0382,0x0001);
write_cmos_sensor(0x0384,0x0001);
write_cmos_sensor(0x0386,0x0003);
write_cmos_sensor(0x0400,0x0001);
write_cmos_sensor(0x0404,0x0020);
write_cmos_sensor_8(0x0114,0x03);
write_cmos_sensor_8(0x0111,0x02);
write_cmos_sensor(0x112C,0x0000);
write_cmos_sensor(0x112E,0x0000);
write_cmos_sensor(0x0136,0x1800);
write_cmos_sensor(0x0304,0x0006);
write_cmos_sensor(0x0306,0x006E);
write_cmos_sensor(0x0302,0x0001);
write_cmos_sensor(0x0300,0x0004);
write_cmos_sensor(0x030C,0x0004);
write_cmos_sensor(0x030E,0x006A);
write_cmos_sensor(0x030A,0x0001);
write_cmos_sensor(0x0308,0x0008);
write_cmos_sensor(0x0342,0x11F0);
write_cmos_sensor(0x0340,0x0C74);
write_cmos_sensor(0x0202,0x0200);
write_cmos_sensor(0x0200,0x0400);
write_cmos_sensor_8(0x0B05,0x00);
write_cmos_sensor_8(0x0B08,0x00);
write_cmos_sensor_8(0x0B00,0x00);
write_cmos_sensor(0x3BBC,0x0000);
write_cmos_sensor(0x3BBE,0x0000);
write_cmos_sensor(0x3BC0,0x0000);
write_cmos_sensor(0x3BC2,0x0000);
write_cmos_sensor(0x3BC4,0x0000);
write_cmos_sensor(0x3BC6,0x0000);
write_cmos_sensor(0x3BC8,0x0000);
write_cmos_sensor(0x3BCA,0x0000);
write_cmos_sensor(0x3BCC,0x0000);
write_cmos_sensor(0x3BCE,0x0000);
write_cmos_sensor(0x3BD0,0x0000);
write_cmos_sensor(0x3BD2,0x0000);
write_cmos_sensor(0x3BD4,0x0000);
write_cmos_sensor(0x3BD6,0x0000);
write_cmos_sensor(0x3BD8,0x0000);
write_cmos_sensor(0x3BDA,0x0000);
write_cmos_sensor_8(0x3B3C,0x01);
write_cmos_sensor(0x3B34,0x3030);
write_cmos_sensor(0x3B36,0x3030);
write_cmos_sensor(0x3B38,0x3030);
write_cmos_sensor(0x3B3A,0x3030);
write_cmos_sensor(0x3C20,0x0080);
write_cmos_sensor(0x3C22,0x0080);
write_cmos_sensor(0x3C24,0x0080);
write_cmos_sensor(0x3C26,0x0080);
write_cmos_sensor(0x3C28,0x0080);
write_cmos_sensor(0x3C2A,0x0080);
write_cmos_sensor(0x3C2C,0x0080);
write_cmos_sensor(0x3C2E,0x0080);
write_cmos_sensor(0x3C30,0x0080);
write_cmos_sensor(0x3C32,0x0080);
write_cmos_sensor(0x3C34,0x0080);
write_cmos_sensor(0x3C36,0x0080);
write_cmos_sensor(0x3C38,0x0080);
write_cmos_sensor(0x3C3A,0x0080);
write_cmos_sensor(0x3C3C,0x0080);
write_cmos_sensor(0x3C3E,0x0080);
write_cmos_sensor(0x306A,0x0068);
write_cmos_sensor_8(0x0100,0x01);

	LOG_INF("Exit");
// s 2014/12/01, bruce
} else if (chip_id == 0xD101) {
	LOG_INF("--preview_setting, chip_id = 0xD101\n");
write_cmos_sensor_8(0x0100,0x00);
mdelay(200);

	write_cmos_sensor(0x6028,0x2000);
	write_cmos_sensor(0x602A,0x14F0);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x0344,0x000C);
	write_cmos_sensor(0x0346,0x0008);
	write_cmos_sensor(0x0348,0x1073);
	write_cmos_sensor(0x034A,0x0C37);
	write_cmos_sensor(0x034C,0x0830);
	write_cmos_sensor(0x034E,0x0610);
	write_cmos_sensor_8(0x0901,0x12);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x0003);
	write_cmos_sensor(0x0400,0x0001);
	write_cmos_sensor(0x0404,0x0020);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0004);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x0C74);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x306A,0x0068);
	write_cmos_sensor_8(0x0100,0x01);	
} else {
	LOG_INF("--preview_setting, chip_id = 0x0%x\n", chip_id);
}
// e 2014/12/01, bruce
}	/*	preview_setting  */
/**********************************************************************************************************************/
//$MIPI[Width:4192,Height:3104,Format:RAW10,Lane:4,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:4,DataRate:1260,useEmbData:0]
//$MV1[MCLK:24,Width:4192,Height:3104,Format:MIPI_RAW10,mipi_lane:4,mipi_datarate:1260,pvi_pclk_inverse:0]

//=====================================================
// 3M2XXM
// Full Resolution normal Mode
// X_output size : 4192
// Y_output size : 3104
// Frame_rate : 30 fps
// Output_format RAW 10
// Output_lanes 4
// Output_clock_mhz : 1272 Mhz
// System_clock_mhz : 440 Mhz , VT_PIX_clk : 88Mhz
// Input_clock_mhz : 24 Mhz
// TnP R651
//=====================================================
//200
/**********************************************************************************************************************/
static void normal_capture_setting()
{
	// s bruce, 2014/12/01
	kal_uint16 chip_id = 0;
		chip_id = read_cmos_sensor(0x0002);
	if (chip_id == 0xC001) {
	LOG_INF("-- normal_capture_settings, chip id = 0xC001\n");
	// e bruce, 2014/12/01
	LOG_INF("hesong 3 Normal capture E! ");


	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x32CA,0x022B);
	write_cmos_sensor(0x32D6,0x022B);
	write_cmos_sensor(0x0344,0x000C);
	write_cmos_sensor(0x0346,0x000C);
	write_cmos_sensor(0x0348,0x1073);
	write_cmos_sensor(0x034A,0x0C33);
	write_cmos_sensor(0x034C,0x1060);
	write_cmos_sensor(0x034E,0x0C20);
	write_cmos_sensor_8(0x0901,0x11);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x0001);
	write_cmos_sensor(0x0400,0x0002);
	write_cmos_sensor(0x0404,0x0010);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0004);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x0C74);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor(0x3BBC,0x0000);
	write_cmos_sensor(0x3BBE,0x0000);
	write_cmos_sensor(0x3BC0,0x0000);
	write_cmos_sensor(0x3BC2,0x0000);
	write_cmos_sensor(0x3BC4,0x0000);
	write_cmos_sensor(0x3BC6,0x0000);
	write_cmos_sensor(0x3BC8,0x0000);
	write_cmos_sensor(0x3BCA,0x0000);
	write_cmos_sensor(0x3BCC,0x0000);
	write_cmos_sensor(0x3BCE,0x0000);
	write_cmos_sensor(0x3BD0,0x0000);
	write_cmos_sensor(0x3BD2,0x0000);
	write_cmos_sensor(0x3BD4,0x0000);
	write_cmos_sensor(0x3BD6,0x0000);
	write_cmos_sensor(0x3BD8,0x0000);
	write_cmos_sensor(0x3BDA,0x0000);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x3C20,0x0080);
	write_cmos_sensor(0x3C22,0x0080);
	write_cmos_sensor(0x3C24,0x0080);
	write_cmos_sensor(0x3C26,0x0080);
	write_cmos_sensor(0x3C28,0x0080);
	write_cmos_sensor(0x3C2A,0x0080);
	write_cmos_sensor(0x3C2C,0x0080);
	write_cmos_sensor(0x3C2E,0x0080);
	write_cmos_sensor(0x3C30,0x0080);
	write_cmos_sensor(0x3C32,0x0080);
	write_cmos_sensor(0x3C34,0x0080);
	write_cmos_sensor(0x3C36,0x0080);
	write_cmos_sensor(0x3C38,0x0080);
	write_cmos_sensor(0x3C3A,0x0080);
	write_cmos_sensor(0x3C3C,0x0080);
	write_cmos_sensor(0x3C3E,0x0080);
	write_cmos_sensor(0x306A,0x0068);
	write_cmos_sensor_8(0x0100,0x01);




	LOG_INF( "Exit!");	
// s bruce, 2014/12/01
} else if (chip_id == 0xD101) {
	LOG_INF( "-- normal_capture_setting, chip_id = 0xD101\n");
write_cmos_sensor_8(0x0100,0x00);
mdelay(200);
write_cmos_sensor(0x6028,0x2000);
write_cmos_sensor(0x602A,0x14F0);
write_cmos_sensor(0x6F12,0x0040);
write_cmos_sensor(0x6F12,0x0040);
write_cmos_sensor(0x6028,0x4000);
write_cmos_sensor(0x0344,0x000C);
write_cmos_sensor(0x0346,0x000C);
write_cmos_sensor(0x0348,0x1073);
write_cmos_sensor(0x034A,0x0C33);
write_cmos_sensor(0x034C,0x1060);
write_cmos_sensor(0x034E,0x0C20);
write_cmos_sensor_8(0x0901,0x11);
write_cmos_sensor(0x0380,0x0001);
write_cmos_sensor(0x0382,0x0001);
write_cmos_sensor(0x0384,0x0001);
write_cmos_sensor(0x0386,0x0001);
write_cmos_sensor(0x0400,0x0002);
write_cmos_sensor(0x0404,0x0010);
write_cmos_sensor_8(0x0114,0x03);
write_cmos_sensor_8(0x0111,0x02);
write_cmos_sensor(0x112C,0x0000);
write_cmos_sensor(0x112E,0x0000);
write_cmos_sensor(0x0136,0x1800);
write_cmos_sensor(0x0304,0x0006);
write_cmos_sensor(0x0306,0x006E);
write_cmos_sensor(0x0302,0x0001);
write_cmos_sensor(0x0300,0x0004);
write_cmos_sensor(0x030C,0x0004);
write_cmos_sensor(0x030E,0x006A);
write_cmos_sensor(0x030A,0x0001);
write_cmos_sensor(0x0308,0x0008);
write_cmos_sensor(0x0342,0x11F0);
write_cmos_sensor(0x0340,0x0C74);
write_cmos_sensor(0x0202,0x0200);
write_cmos_sensor(0x0200,0x0400);
write_cmos_sensor_8(0x0B05,0x00);
write_cmos_sensor_8(0x0B08,0x00);
write_cmos_sensor_8(0x0B00,0x00);
write_cmos_sensor_8(0x3B3C,0x01);
write_cmos_sensor(0x3B34,0x3030);
write_cmos_sensor(0x3B36,0x3030);
write_cmos_sensor(0x3B38,0x3030);
write_cmos_sensor(0x3B3A,0x3030);
write_cmos_sensor(0x306A,0x0068);
write_cmos_sensor_8(0x0100,0x01);	
} else {
	LOG_INF( "-- normal_capture_setting, chip_id = 0x%x\n", chip_id);
}
// e bruce, 2014/12/01	
}

/**********************************************************************************************************************/
//$MIPI[Width:4192,Height:3104,Format:RAW10,Lane:4,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:4,DataRate:1260,useEmbData:0]
//$MV1[MCLK:24,Width:4192,Height:3104,Format:MIPI_RAW10,mipi_lane:4,mipi_datarate:1260,pvi_pclk_inverse:0]
//=====================================================
// 3M2XXM
// Full Resolution normal Mode
// X_output size : 4192
// Y_output size : 3104
// Frame_rate : 24.07 fps
// Output_format RAW 10
// Output_lanes 4
// Output_clock_mhz : 1272 Mhz
// System_clock_mhz : 352 Mhz , VT_PIX_clk : 88Mhz
// Input_clock_mhz : 24 Mhz
// TnP R651
//=====================================================
/**********************************************************************************************************************/
static void pip_capture_setting()
{
	// s bruce, 2014/12/01
	kal_uint16 chip_id = 0;
		
	chip_id = read_cmos_sensor(0x0002);
	if (chip_id == 0xC001) {
	LOG_INF( "--pip_capture_setting, chip_id = 0xC001\n");
	// e bruce, 2014/12/01
	LOG_INF( "S5k3M2_2ND PIP setting Enter!"); 
	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x32CA,0x022B);
	write_cmos_sensor(0x32D6,0x022B);
	write_cmos_sensor(0x0344,0x000C);
	write_cmos_sensor(0x0346,0x000C);
	write_cmos_sensor(0x0348,0x1073);
	write_cmos_sensor(0x034A,0x0C33);
	write_cmos_sensor(0x034C,0x1060);
	write_cmos_sensor(0x034E,0x0C20);
	write_cmos_sensor_8(0x0901,0x11);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x0001);
	write_cmos_sensor(0x0400,0x0002);
	write_cmos_sensor(0x0404,0x0010);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0005);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x0C74);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor(0x3BBC,0x0000);
	write_cmos_sensor(0x3BBE,0x0000);
	write_cmos_sensor(0x3BC0,0x0000);
	write_cmos_sensor(0x3BC2,0x0000);
	write_cmos_sensor(0x3BC4,0x0000);
	write_cmos_sensor(0x3BC6,0x0000);
	write_cmos_sensor(0x3BC8,0x0000);
	write_cmos_sensor(0x3BCA,0x0000);
	write_cmos_sensor(0x3BCC,0x0000);
	write_cmos_sensor(0x3BCE,0x0000);
	write_cmos_sensor(0x3BD0,0x0000);
	write_cmos_sensor(0x3BD2,0x0000);
	write_cmos_sensor(0x3BD4,0x0000);
	write_cmos_sensor(0x3BD6,0x0000);
	write_cmos_sensor(0x3BD8,0x0000);
	write_cmos_sensor(0x3BDA,0x0000);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x3C20,0x0080);
	write_cmos_sensor(0x3C22,0x0080);
	write_cmos_sensor(0x3C24,0x0080);
	write_cmos_sensor(0x3C26,0x0080);
	write_cmos_sensor(0x3C28,0x0080);
	write_cmos_sensor(0x3C2A,0x0080);
	write_cmos_sensor(0x3C2C,0x0080);
	write_cmos_sensor(0x3C2E,0x0080);
	write_cmos_sensor(0x3C30,0x0080);
	write_cmos_sensor(0x3C32,0x0080);
	write_cmos_sensor(0x3C34,0x0080);
	write_cmos_sensor(0x3C36,0x0080);
	write_cmos_sensor(0x3C38,0x0080);
	write_cmos_sensor(0x3C3A,0x0080);
	write_cmos_sensor(0x3C3C,0x0080);
	write_cmos_sensor(0x3C3E,0x0080);
	write_cmos_sensor(0x306A,0x0068);
	write_cmos_sensor_8(0x0100,0x01);
	LOG_INF("Exit");
// s bruce, 2014/12/01
} else if (chip_id == 0xD101) {
	LOG_INF( "--pip_capture_setting, chip_id = 0xD101\n");
	write_cmos_sensor(0x6028,0x2000);
	write_cmos_sensor(0x602A,0x14F0);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x0344,0x000C);
	write_cmos_sensor(0x0346,0x000C);
	write_cmos_sensor(0x0348,0x1073);
	write_cmos_sensor(0x034A,0x0C33);
	write_cmos_sensor(0x034C,0x1060);
	write_cmos_sensor(0x034E,0x0C20);
	write_cmos_sensor_8(0x0901,0x11);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x0001);
	write_cmos_sensor(0x0400,0x0002);
	write_cmos_sensor(0x0404,0x0010);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0005);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x0C74);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x306A,0x0068);
	write_cmos_sensor_8(0x0100,0x01);		
} else {
	LOG_INF( "--pip_capture_setting, chip_id = 0x%x\n", chip_id);
}
// e bruce, 2014/12/01
}

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
	if(currefps==300)
		normal_capture_setting();
	else if(currefps==240) // PIP
		pip_capture_setting();
	else
		pip_capture_setting();
}

static void normal_video_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);	
	normal_capture_setting();	
}
/**********************************************************************************************************************/
//$MIPI[Width:688,Height:512,Format:RAW10,Lane:4,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:4,DataRate:1260,useEmbData:0]
//$MV1[MCLK:24,Width:688,Height:512,Format:MIPI_RAW10,mipi_lane:4,mipi_datarate:1260,pvi_pclk_inverse:0]

//=====================================================
// 3M2XXM
// 6X6 Binning Normal Mode
// X_output size : 688
// Y_output size : 512
// Frame_rate : 120.07 fps
// Output_format RAW 10
// Output_lanes 4
// Output_clock_mhz : 1272 Mhz
// System_clock_mhz : 440 Mhz
// Input_clock_mhz : 24 Mhz
// TnP R651
//=====================================================
/**********************************************************************************************************************/
static void hs_video_setting()
{
	// s bruce, 2014/12/01
#if 0
	kal_uint16 chip_id = 0;
	chip_id = read_cmos_sensor(0x0002);
	if (chip_id == 0xC001) {
		LOG_INF("--hs_video_setting. chip_id = 0xC001\n");
	// e bruce, 2014/12/01
	LOG_INF("E");
//p200

	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x32CA,0x022B);
	write_cmos_sensor(0x32D6,0x022B);
	write_cmos_sensor(0x0344,0x000C);
	write_cmos_sensor(0x0346,0x0008);
	write_cmos_sensor(0x0348,0x1073);
	write_cmos_sensor(0x034A,0x0C37);
	write_cmos_sensor(0x034C,0x02B0);
	write_cmos_sensor(0x034E,0x0200);
	write_cmos_sensor_8(0x0901,0x16);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x000B);
	write_cmos_sensor(0x0400,0x0001);
	write_cmos_sensor(0x0404,0x0060);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0004);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x031E);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor(0x3BBC,0x0000);
	write_cmos_sensor(0x3BBE,0x0000);
	write_cmos_sensor(0x3BC0,0x0000);
	write_cmos_sensor(0x3BC2,0x0000);
	write_cmos_sensor(0x3BC4,0x0000);
	write_cmos_sensor(0x3BC6,0x0000);
	write_cmos_sensor(0x3BC8,0x0000);
	write_cmos_sensor(0x3BCA,0x0000);
	write_cmos_sensor(0x3BCC,0x0000);
	write_cmos_sensor(0x3BCE,0x0000);
	write_cmos_sensor(0x3BD0,0x0000);
	write_cmos_sensor(0x3BD2,0x0000);
	write_cmos_sensor(0x3BD4,0x0000);
	write_cmos_sensor(0x3BD6,0x0000);
	write_cmos_sensor(0x3BD8,0x0000);
	write_cmos_sensor(0x3BDA,0x0000);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x3C20,0x0080);
	write_cmos_sensor(0x3C22,0x0080);
	write_cmos_sensor(0x3C24,0x0080);
	write_cmos_sensor(0x3C26,0x0080);
	write_cmos_sensor(0x3C28,0x0080);
	write_cmos_sensor(0x3C2A,0x0080);
	write_cmos_sensor(0x3C2C,0x0080);
	write_cmos_sensor(0x3C2E,0x0080);
	write_cmos_sensor(0x3C30,0x0080);
	write_cmos_sensor(0x3C32,0x0080);
	write_cmos_sensor(0x3C34,0x0080);
	write_cmos_sensor(0x3C36,0x0080);
	write_cmos_sensor(0x3C38,0x0080);
	write_cmos_sensor(0x3C3A,0x0080);
	write_cmos_sensor(0x3C3C,0x0080);
	write_cmos_sensor(0x3C3E,0x0080);
	write_cmos_sensor(0x306A,0x0068);
	write_cmos_sensor_8(0x0100,0x01);
// s bruce 2014/12/01
} else if (chip_id == 0xD101) {
	LOG_INF("--hs_video_setting. chip_id = 0xD101\n");
	write_cmos_sensor(0x6028,0x2000);
	write_cmos_sensor(0x602A,0x14F0);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x0344,0x000C);
	write_cmos_sensor(0x0346,0x0008);
	write_cmos_sensor(0x0348,0x1073);
	write_cmos_sensor(0x034A,0x0C37);
	write_cmos_sensor(0x034C,0x02B0);
	write_cmos_sensor(0x034E,0x0200);
	write_cmos_sensor_8(0x0901,0x16);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x000B);
	write_cmos_sensor(0x0400,0x0001);
	write_cmos_sensor(0x0404,0x0060);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0004);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x031E);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x306A,0x0068);
	write_cmos_sensor_8(0x0100,0x01);
	write_cmos_sensor_8(0x0100,0x01);	
	
	
} else {
	LOG_INF("--hs_video_setting. chip_id = 0x%x\n", chip_id);
}
// e bruce 2014/12/01
#else

	// s bruce, 2014/12/01
	kal_uint16 chip_id = 0;
	chip_id = read_cmos_sensor(0x0002);
	if (chip_id == 0xC001) {
		LOG_INF("--HS_video_setting. chip_id = 0xC001\n");
	// e bruce, 2014/12/01
	LOG_INF("E");
	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x32CA,0x022B);
	write_cmos_sensor(0x32D6,0x022B);
	write_cmos_sensor(0x0344,0x00BC);
	write_cmos_sensor(0x0346,0x01DC);
	write_cmos_sensor(0x0348,0x0FC3);
	write_cmos_sensor(0x034A,0x0A63);
	write_cmos_sensor(0x034C,0x0500);
	write_cmos_sensor(0x034E,0x02D0);
	write_cmos_sensor_8(0x0901,0x13);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x0005);
	write_cmos_sensor(0x0400,0x0001);
	write_cmos_sensor(0x0404,0x0030);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0004);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x02E0);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor(0x3BBC,0x0000);
	write_cmos_sensor(0x3BBE,0x0000);
	write_cmos_sensor(0x3BC0,0x0000);
	write_cmos_sensor(0x3BC2,0x0000);
	write_cmos_sensor(0x3BC4,0x0000);
	write_cmos_sensor(0x3BC6,0x0000);
	write_cmos_sensor(0x3BC8,0x0000);
	write_cmos_sensor(0x3BCA,0x0000);
	write_cmos_sensor(0x3BCC,0x0000);
	write_cmos_sensor(0x3BCE,0x0000);
	write_cmos_sensor(0x3BD0,0x0000);
	write_cmos_sensor(0x3BD2,0x0000);
	write_cmos_sensor(0x3BD4,0x0000);
	write_cmos_sensor(0x3BD6,0x0000);
	write_cmos_sensor(0x3BD8,0x0000);
	write_cmos_sensor(0x3BDA,0x0000);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x3C20,0x0080);
	write_cmos_sensor(0x3C22,0x0080);
	write_cmos_sensor(0x3C24,0x0080);
	write_cmos_sensor(0x3C26,0x0080);
	write_cmos_sensor(0x3C28,0x0080);
	write_cmos_sensor(0x3C2A,0x0080);
	write_cmos_sensor(0x3C2C,0x0080);
	write_cmos_sensor(0x3C2E,0x0080);
	write_cmos_sensor(0x3C30,0x0080);
	write_cmos_sensor(0x3C32,0x0080);
	write_cmos_sensor(0x3C34,0x0080);
	write_cmos_sensor(0x3C36,0x0080);
	write_cmos_sensor(0x3C38,0x0080);
	write_cmos_sensor(0x3C3A,0x0080);
	write_cmos_sensor(0x3C3C,0x0080);
	write_cmos_sensor(0x3C3E,0x0080);
	write_cmos_sensor(0x306A,0x0068);

	write_cmos_sensor_8(0x0100,0x01);
	//s bruce, 2014/12/01
	} else if (chip_id == 0xD101) {
	LOG_INF("--hs_video_setting. chip_id = 0xD101");
	write_cmos_sensor(0x6028,0x2000);
	write_cmos_sensor(0x602A,0x14F0);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x0344,0x00BC);
	write_cmos_sensor(0x0346,0x01DC);
	write_cmos_sensor(0x0348,0x0FC3);
	write_cmos_sensor(0x034A,0x0A63);
	write_cmos_sensor(0x034C,0x0500);
	write_cmos_sensor(0x034E,0x02D0);
	write_cmos_sensor_8(0x0901,0x13);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x0005);
	write_cmos_sensor(0x0400,0x0001);
	write_cmos_sensor(0x0404,0x0030);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0004);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x02E0);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x306A,0x0068);
	write_cmos_sensor_8(0x0100,0x01);
	write_cmos_sensor_8(0x0100,0x01);		
	} else {
		LOG_INF("--HS_video_setting. chip_id = 0x%x\n", chip_id);
	}
//e bruce, 2014/12/01


#endif
}

/**********************************************************************************************************************/
//$MIPI[Width:1280,Height:720,Format:RAW10,Lane:4,ErrorCheck:0,PolarityData:0,PolarityClock:0,Buffer:4,DataRate:1260,useEmbData:0]
//$MV1[MCLK:24,Width:1280,Height:720,Format:MIPI_RAW10,mipi_lane:4,mipi_datarate:1260,pvi_pclk_inverse:0]
//=====================================================
// 3M2XXM
// 3X3 Binning Normal Mode
// X_output size : 1280
// Y_output size : 720
// Frame_rate : 30.06 fps
// Output_format RAW 10
// Output_lanes 4
// Output_clock_mhz : 1272 Mhz
// System_clock_mhz : 440 Mhz
// Input_clock_mhz : 24 Mhz
// TnP R651
//=====================================================
/**********************************************************************************************************************/
static void slim_video_setting()
{
	// s bruce, 2014/12/01
	kal_uint16 chip_id = 0;
	chip_id = read_cmos_sensor(0x0002);
	if (chip_id == 0xC001) {
		LOG_INF("--slim_video_setting. chip_id = 0xC001\n");
	// e bruce, 2014/12/01
	LOG_INF("E");
	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x32CA,0x022B);
	write_cmos_sensor(0x32D6,0x022B);
	write_cmos_sensor(0x0344,0x00BC);
	write_cmos_sensor(0x0346,0x01DC);
	write_cmos_sensor(0x0348,0x0FC3);
	write_cmos_sensor(0x034A,0x0A63);
	write_cmos_sensor(0x034C,0x0500);
	write_cmos_sensor(0x034E,0x02D0);
	write_cmos_sensor_8(0x0901,0x13);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x0005);
	write_cmos_sensor(0x0400,0x0001);
	write_cmos_sensor(0x0404,0x0030);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0004);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x0C74);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor(0x3BBC,0x0000);
	write_cmos_sensor(0x3BBE,0x0000);
	write_cmos_sensor(0x3BC0,0x0000);
	write_cmos_sensor(0x3BC2,0x0000);
	write_cmos_sensor(0x3BC4,0x0000);
	write_cmos_sensor(0x3BC6,0x0000);
	write_cmos_sensor(0x3BC8,0x0000);
	write_cmos_sensor(0x3BCA,0x0000);
	write_cmos_sensor(0x3BCC,0x0000);
	write_cmos_sensor(0x3BCE,0x0000);
	write_cmos_sensor(0x3BD0,0x0000);
	write_cmos_sensor(0x3BD2,0x0000);
	write_cmos_sensor(0x3BD4,0x0000);
	write_cmos_sensor(0x3BD6,0x0000);
	write_cmos_sensor(0x3BD8,0x0000);
	write_cmos_sensor(0x3BDA,0x0000);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x3C20,0x0080);
	write_cmos_sensor(0x3C22,0x0080);
	write_cmos_sensor(0x3C24,0x0080);
	write_cmos_sensor(0x3C26,0x0080);
	write_cmos_sensor(0x3C28,0x0080);
	write_cmos_sensor(0x3C2A,0x0080);
	write_cmos_sensor(0x3C2C,0x0080);
	write_cmos_sensor(0x3C2E,0x0080);
	write_cmos_sensor(0x3C30,0x0080);
	write_cmos_sensor(0x3C32,0x0080);
	write_cmos_sensor(0x3C34,0x0080);
	write_cmos_sensor(0x3C36,0x0080);
	write_cmos_sensor(0x3C38,0x0080);
	write_cmos_sensor(0x3C3A,0x0080);
	write_cmos_sensor(0x3C3C,0x0080);
	write_cmos_sensor(0x3C3E,0x0080);
	write_cmos_sensor(0x306A,0x0068);

	write_cmos_sensor_8(0x0100,0x01);
	//s bruce, 2014/12/01
	} else if (chip_id == 0xD101) {
	LOG_INF("--slim_video_setting. chip_id = 0xD101");
	write_cmos_sensor(0x6028,0x2000);
	write_cmos_sensor(0x602A,0x14F0);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6F12,0x0040);
	write_cmos_sensor(0x6028,0x4000);
	write_cmos_sensor(0x0344,0x00BC);
	write_cmos_sensor(0x0346,0x01DC);
	write_cmos_sensor(0x0348,0x0FC3);
	write_cmos_sensor(0x034A,0x0A63);
	write_cmos_sensor(0x034C,0x0500);
	write_cmos_sensor(0x034E,0x02D0);
	write_cmos_sensor_8(0x0901,0x13);
	write_cmos_sensor(0x0380,0x0001);
	write_cmos_sensor(0x0382,0x0001);
	write_cmos_sensor(0x0384,0x0001);
	write_cmos_sensor(0x0386,0x0005);
	write_cmos_sensor(0x0400,0x0001);
	write_cmos_sensor(0x0404,0x0030);
	write_cmos_sensor_8(0x0114,0x03);
	write_cmos_sensor_8(0x0111,0x02);
	write_cmos_sensor(0x112C,0x0000);
	write_cmos_sensor(0x112E,0x0000);
	write_cmos_sensor(0x0136,0x1800);
	write_cmos_sensor(0x0304,0x0006);
	write_cmos_sensor(0x0306,0x006E);
	write_cmos_sensor(0x0302,0x0001);
	write_cmos_sensor(0x0300,0x0004);
	write_cmos_sensor(0x030C,0x0004);
	write_cmos_sensor(0x030E,0x006A);
	write_cmos_sensor(0x030A,0x0001);
	write_cmos_sensor(0x0308,0x0008);
	write_cmos_sensor(0x0342,0x11F0);
	write_cmos_sensor(0x0340,0x0C74);
	write_cmos_sensor(0x0202,0x0200);
	write_cmos_sensor(0x0200,0x0400);
	write_cmos_sensor_8(0x0B05,0x00);
	write_cmos_sensor_8(0x0B08,0x00);
	write_cmos_sensor_8(0x0B00,0x00);
	write_cmos_sensor_8(0x3B3C,0x01);
	write_cmos_sensor(0x3B34,0x3030);
	write_cmos_sensor(0x3B36,0x3030);
	write_cmos_sensor(0x3B38,0x3030);
	write_cmos_sensor(0x3B3A,0x3030);
	write_cmos_sensor(0x306A,0x0068);
	write_cmos_sensor_8(0x0100,0x01);
	write_cmos_sensor_8(0x0100,0x01);		
	} else {
		LOG_INF("--slim_video_setting. chip_id = 0x%x\n", chip_id);
	}
//e bruce, 2014/12/01
}

/*************************************************************************
* FUNCTION
*	get_imgsensor_id
*
* DESCRIPTION
*	This function get the sensor ID 
*
* PARAMETERS
*	*sensorID : return the sensor ID 
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 get_imgsensor_id(UINT32 *sensor_id) 
{
	kal_uint8 i = 0;
    kal_uint8 retry = 1;
    kal_uint16 Module_id = 0;
    struct devinfo_struct *dev_sunny;// = (struct devinfo_struct*)kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);
    static kal_uint8 devinfo_add = 2;
    
   
    /*sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address*/
    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
			write_cmos_sensor(0x602C,0x4000);
			write_cmos_sensor(0x602E,0x0000);
			*sensor_id = read_cmos_sensor(0x6F12);
			//*sensor_id = imgsensor_info.sensor_id;
            if (*sensor_id == S5K3M2_SENSOR_ID) {               
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,*sensor_id); 
                Module_id = S5k3M2_2ND_read_cmos_sensor1(0x0005);
                if (Module_id == 0x01)
                {
	                if(devinfo_add !=0)
				    {
				    	dev_sunny = (struct devinfo_struct*)kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);
    
					    dev_sunny->device_type = "Camera";
					    dev_sunny->device_vendor = "Sunny";
					    dev_sunny->device_ic = "S5k3M2_2ND";
					    dev_sunny->device_version = DEVINFO_NULL;
					    dev_sunny->device_module = DEVINFO_NULL;
					    dev_sunny->device_info = DEVINFO_NULL;
					    LOG_INF("<%s:%d>devinfo_add[%d]dev[%x]\n", __func__, __LINE__, devinfo_add, dev_sunny);
				    	dev_sunny->device_used = DEVINFO_USED;
				    	DEVINFO_CHECK_ADD_DEVICE(dev_sunny);
				    	devinfo_add = 0;
				    } 
				    *sensor_id =	imgsensor_info.sensor_id;
	                return ERROR_NONE;
            	}
            	else
            	{
            		*sensor_id = 0xFFFFFFFF;
            		break;
            	}
            }   
            LOG_INF("Read sensor id fail, write id: 0x%x, sensor id = 0x%x\n", imgsensor.i2c_write_id,*sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        retry = 1;
    }
    
    if (*sensor_id != imgsensor_info.sensor_id) {
        // if Sensor ID is not correct, Must set *sensor_id to 0xFFFFFFFF 
        *sensor_id = 0xFFFFFFFF;
        
        if(devinfo_add == 1)
    	{
    		dev_sunny = (struct devinfo_struct*)kmalloc(sizeof(struct devinfo_struct), GFP_KERNEL);

		    dev_sunny->device_type = "Camera";
		    dev_sunny->device_vendor = "Sunny";
		    dev_sunny->device_ic = "S5k3M2_2ND";
		    dev_sunny->device_version = DEVINFO_NULL;
		    dev_sunny->device_module = DEVINFO_NULL;
		    dev_sunny->device_info = DEVINFO_NULL;
		    LOG_INF("<%s:%d>devinfo_add[%d]dev[%x]\n", __func__, __LINE__, devinfo_add, dev_sunny);
	    	dev_sunny->device_used = DEVINFO_UNUSED;
	    	DEVINFO_CHECK_ADD_DEVICE(dev_sunny);
    		devinfo_add = 0;
    	}
    	else
    		if (devinfo_add > 0)
    			devinfo_add--;

        return ERROR_SENSOR_CONNECT_FAIL;
    }
    
    
    return ERROR_NONE;
}


/*************************************************************************
* FUNCTION
*	open
*
* DESCRIPTION
*	This function initialize the registers of CMOS sensor
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 open(void)
{
	//const kal_uint8 i2c_addr[] = {IMGSENSOR_WRITE_ID_1, IMGSENSOR_WRITE_ID_2};
	kal_uint8 i = 0;
	kal_uint8 retry = 2;
	kal_uint16 sensor_id = 0; 
	LOG_1;
	LOG_2;
	//sensor have two i2c address 0x5a 0x5b & 0x21 0x20, we should detect the module used i2c address
	    while (imgsensor_info.i2c_addr_table[i] != 0xff) {
        spin_lock(&imgsensor_drv_lock);
        imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
        spin_unlock(&imgsensor_drv_lock);
        do {
			write_cmos_sensor(0x602C,0x4000);
			write_cmos_sensor(0x602E,0x0000);
            sensor_id =  read_cmos_sensor(0x6F12);
			//sensor_id = imgsensor_info.sensor_id;
            if (sensor_id == S5K3M2_SENSOR_ID) {                
                LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);   
                break;
            }   
            LOG_INF("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
            retry--;
        } while(retry > 0);
        i++;
        if (sensor_id == S5K3M2_SENSOR_ID)
            break;
        retry = 2;
    }        
    if (S5K3M2_SENSOR_ID != sensor_id)
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
}	/*	open  */



/*************************************************************************
* FUNCTION
*	close
*
* DESCRIPTION
*	
*
* PARAMETERS
*	None
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 close(void)
{
	LOG_INF("E\n");

	/*No Need to implement this function*/ 
	
	return ERROR_NONE;
}	/*	close  */


/*************************************************************************
* FUNCTION
* preview
*
* DESCRIPTION
*	This function start the sensor preview.
*
* PARAMETERS
*	*image_window : address pointer of pixel numbers in one period of HSYNC
*  *sensor_config_data : address pointer of line numbers in one period of VSYNC
*
* RETURNS
*	None
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
	set_mirror_flip(IMAGE_HV_MIRROR);
	return ERROR_NONE;
}	/*	preview   */

/*************************************************************************
* FUNCTION
*	capture
*
* DESCRIPTION
*	This function setup the CMOS sensor in capture MY_OUTPUT mode
*
* PARAMETERS
*
* RETURNS
*	None
*
* GLOBALS AFFECTED
*
*************************************************************************/
static kal_uint32 capture(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
						  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;

    if (imgsensor.current_fps == imgsensor_info.cap.max_framerate) // 30fps
    {
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	else //PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
    {
		if (imgsensor.current_fps != imgsensor_info.cap1.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",imgsensor_info.cap1.max_framerate/10);   
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} 

	spin_unlock(&imgsensor_drv_lock);
	LOG_INF("Caputre fps:%d\n",imgsensor.current_fps);
	capture_setting(imgsensor.current_fps); 
    set_mirror_flip(IMAGE_HV_MIRROR);
	return ERROR_NONE;
}	/* capture() */
static kal_uint32 normal_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	
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
	set_mirror_flip(IMAGE_HV_MIRROR);	
	return ERROR_NONE;
}	/*	normal_video   */

static kal_uint32 hs_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	
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
	set_mirror_flip(IMAGE_HV_MIRROR);
	return ERROR_NONE;
}	/*	hs_video   */


static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E");
	
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
	set_mirror_flip(IMAGE_HV_MIRROR);
	return ERROR_NONE;
}

	
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
	LOG_INF("E");
	sensor_resolution->SensorFullWidth = imgsensor_info.cap.grabwindow_width;
	sensor_resolution->SensorFullHeight = imgsensor_info.cap.grabwindow_height;
	
	sensor_resolution->SensorPreviewWidth = imgsensor_info.pre.grabwindow_width;
	sensor_resolution->SensorPreviewHeight = imgsensor_info.pre.grabwindow_height;

	sensor_resolution->SensorVideoWidth = imgsensor_info.normal_video.grabwindow_width;
	sensor_resolution->SensorVideoHeight = imgsensor_info.normal_video.grabwindow_height;		

	
	sensor_resolution->SensorHighSpeedVideoWidth	 = imgsensor_info.hs_video.grabwindow_width;
	sensor_resolution->SensorHighSpeedVideoHeight	 = imgsensor_info.hs_video.grabwindow_height;
	
	sensor_resolution->SensorSlimVideoWidth	 = imgsensor_info.slim_video.grabwindow_width;
	sensor_resolution->SensorSlimVideoHeight	 = imgsensor_info.slim_video.grabwindow_height;
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
	return ERROR_NONE;
}	/*	get_resolution	*/

static kal_uint32 get_info(MSDK_SCENARIO_ID_ENUM scenario_id,
					  MSDK_SENSOR_INFO_STRUCT *sensor_info,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d", scenario_id);

	
	//sensor_info->SensorVideoFrameRate = imgsensor_info.normal_video.max_framerate/10; /* not use */
	//sensor_info->SensorStillCaptureFrameRate= imgsensor_info.cap.max_framerate/10; /* not use */
	//imgsensor_info->SensorWebCamCaptureFrameRate= imgsensor_info.v.max_framerate; /* not use */

	sensor_info->SensorClockPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorClockFallingPolarity = SENSOR_CLOCK_POLARITY_LOW; /* not use */
	sensor_info->SensorHsyncPolarity = SENSOR_CLOCK_POLARITY_LOW; // inverse with datasheet
	sensor_info->SensorVsyncPolarity = SENSOR_CLOCK_POLARITY_LOW;
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
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
	
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
	sensor_info->AEISPGainDelayFrame = imgsensor_info.ae_ispGain_delay_frame;	
	sensor_info->IHDR_Support = imgsensor_info.ihdr_support;
	sensor_info->IHDR_LE_FirstLine = imgsensor_info.ihdr_le_firstline;
	sensor_info->SensorModeNum = imgsensor_info.sensor_mode_num;
	sensor_info->PDAF_Support = 1;	
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
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x 
	sensor_info->SensorPacketECCOrder = 1;
	LOG_INF("<%s:%d>PDAF_Support [%d] \n", __func__, __LINE__, sensor_info->PDAF_Support);
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
}	/*	get_info  */


static kal_uint32 control(MSDK_SCENARIO_ID_ENUM scenario_id, MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("scenario_id = %d", scenario_id);
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
}	/* control() */



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
	LOG_INF("enable = %d, framerate = %d ", enable, framerate);
	spin_lock(&imgsensor_drv_lock);
	if (enable) 	  
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
			if(framerate==300)
			{
			frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			}
			else
			{
			frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
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
           // set_dummy();            
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

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        write_cmos_sensor(0x0600, 0x0002);
	} else {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
        write_cmos_sensor(0x0600, 0x0000);
	}	 
	spin_lock(&imgsensor_drv_lock);
	imgsensor.test_pattern = enable;
	spin_unlock(&imgsensor_drv_lock);
	return ERROR_NONE;
}

static kal_uint32 feature_control(MSDK_SENSOR_FEATURE_ENUM feature_id,
							 UINT8 *feature_para,UINT32 *feature_para_len)
{
	UINT16 *feature_return_para_16=(UINT16 *) feature_para;
	UINT16 *feature_data_16=(UINT16 *) feature_para;
	UINT32 *feature_return_para_32=(UINT32 *) feature_para;
	UINT32 *feature_data_32=(UINT32 *) feature_para;

    SET_PD_BLOCK_INFO_T *PDAFinfo;	
	
	SENSOR_WINSIZE_INFO_STRUCT *wininfo;	
	MSDK_SENSOR_REG_INFO_STRUCT *sensor_reg_data=(MSDK_SENSOR_REG_INFO_STRUCT *) feature_para;
    unsigned long long *feature_data=(unsigned long long *) feature_para;
    unsigned long long *feature_return_para=(unsigned long long *) feature_para;	
 
	LOG_INF("feature_id = %d", feature_id);
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
			write_shutter(*feature_data);
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
			if((sensor_reg_data->RegData>>8)>0)
			   write_cmos_sensor(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
			else
				write_cmos_sensor_8(sensor_reg_data->RegAddr, sensor_reg_data->RegData);
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
			get_default_framerate_by_scenario((MSDK_SCENARIO_ID_ENUM)*feature_data, (MUINT32 *)(uintptr_t)(*(feature_data+1)));
			break;
		case SENSOR_FEATURE_GET_PDAF_DATA:	
			LOG_INF("SENSOR_FEATURE_GET_PDAF_DATA\n");
			read_3m2_2nd_eeprom((kal_uint16 )(*feature_data),(char*)(uintptr_t)(*(feature_data+1)),(kal_uint32)(*(feature_data+2)));
			break;			
		case SENSOR_FEATURE_SET_TEST_PATTERN:
			set_test_pattern_mode((BOOL)*feature_data);
			break;
		case SENSOR_FEATURE_GET_TEST_PATTERN_CHECKSUM_VALUE: //for factory mode auto testing			 
			*feature_return_para_32 = imgsensor_info.checksum_value;
			*feature_para_len=4;							 
			break;				
		case SENSOR_FEATURE_SET_FRAMERATE:
			LOG_INF("current fps :%d\n", *feature_data);
			spin_lock(&imgsensor_drv_lock);
			imgsensor.current_fps = *feature_data;
			spin_unlock(&imgsensor_drv_lock);	
			break;
		case SENSOR_FEATURE_SET_HDR:
			//LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data_16);
			LOG_INF("Warning! Not Support IHDR Feature");
			spin_lock(&imgsensor_drv_lock);
			//imgsensor.ihdr_en = (BOOL)*feature_data_16;
            imgsensor.ihdr_en = KAL_FALSE;
			spin_unlock(&imgsensor_drv_lock);
			break;
		case SENSOR_FEATURE_GET_CROP_INFO:
			LOG_INF("SENSOR_FEATURE_GET_CROP_INFO scenarioId:%d\n", *feature_data);
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
			break;
		case SENSOR_FEATURE_GET_PDAF_INFO:
			LOG_INF("SENSOR_FEATURE_GET_PDAF_INFO scenarioId:%d\n", *feature_data);
			PDAFinfo= (SET_PD_BLOCK_INFO_T *)(uintptr_t)(*(feature_data+1));
		
			switch (*feature_data) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					memcpy((void *)PDAFinfo,(void *)&imgsensor_pd_info,sizeof(SET_PD_BLOCK_INFO_T));
					break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
				default:
					break;
			}
			break;
    // add by yfx
		case SENSOR_FEATURE_GET_SENSOR_PDAF_EEPROM_DATASIZE:	 
			LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_EEPROM_DATASIZE\n");
			*feature_return_para_32 = 1404;
			*feature_para_len=4;
			break;	
		// end	
			
		case SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY:
			LOG_INF("SENSOR_FEATURE_GET_SENSOR_PDAF_CAPACITY scenarioId:%d\n", *feature_data);
			//PDAF capacity enable or not, 2p8 only full size support PDAF
			switch (*feature_data) {
				case MSDK_SCENARIO_ID_CAMERA_CAPTURE_JPEG:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1;
					break;
				case MSDK_SCENARIO_ID_VIDEO_PREVIEW:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 1; // video & capture use same setting
					break;
				case MSDK_SCENARIO_ID_HIGH_SPEED_VIDEO:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
					break;
				case MSDK_SCENARIO_ID_SLIM_VIDEO:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
					break;
				case MSDK_SCENARIO_ID_CAMERA_PREVIEW:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
					break;
				default:
					*(MUINT32 *)(uintptr_t)(*(feature_data+1)) = 0;
					break;
			}
			break;
			
		case SENSOR_FEATURE_SET_IHDR_SHUTTER_GAIN:
		LOG_INF("SENSOR_SET_SENSOR_IHDR is no support");
		break;
		default:
			break;
	}
	return ERROR_NONE;
}	/*	feature_control()  */

static SENSOR_FUNCTION_STRUCT sensor_func = {
	open,
	get_info,
	get_resolution,
	feature_control,
	control,
	close
};

UINT32 S5K3M2_2ND_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	S5k3M2_2ND_MIPI_RAW_SensorInit	*/
