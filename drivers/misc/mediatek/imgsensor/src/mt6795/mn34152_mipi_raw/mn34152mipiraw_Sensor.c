/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 MN34152mipi_Sensor.c
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

#include "mn34152mipiraw_Sensor.h"

#define PFX "MN34152_camera_sensor"
//#define LOG_WRN(format, args...) xlog_printk(ANDROID_LOG_WARN ,PFX, "[%S] " format, __FUNCTION__, ##args)
//#defineLOG_INF(format, args...) xlog_printk(ANDROID_LOG_INFO ,PFX, "[%s] " format, __FUNCTION__, ##args)
//#define LOG_DBG(format, args...) xlog_printk(ANDROID_LOG_DEBUG ,PFX, "[%S] " format, __FUNCTION__, ##args)
#define LOG_INF(format, args...)	xlog_printk(ANDROID_LOG_INFO   , PFX, "[%s] " format, __FUNCTION__, ##args)

static DEFINE_SPINLOCK(imgsensor_drv_lock);

#define OTP_SIZE 452

static imgsensor_info_struct imgsensor_info = { 
	.sensor_id = MN34152_SENSOR_ID,
	
	.checksum_value =0xc15d2913,
	
	.pre = {
        .pclk = 237600000,              //record different mode's pclk
		.linelength = 2404,				//record different mode's linelength
        .framelength = 3296,         //record different mode's framelength
		.startx = 0,					//record different mode's startx of grabwindow
		.starty = 0,					//record different mode's starty of grabwindow
		.grabwindow_width = 2112,		//record different mode's width of grabwindow
		.grabwindow_height = 1584,		//record different mode's height of grabwindow
		/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
		.mipi_data_lp2hs_settle_dc = 85,
		/*	 following for GetDefaultFramerateByScenario()	*/
		.max_framerate = 300,	
	},
	.cap = {
		.pclk =475200000,
		.linelength = 4804,
		.framelength = 3300,
		.startx =0,
		.starty = 0,
		.grabwindow_width = 4224, /* 0x1080 */
		.grabwindow_height = 3168,/* 0x0c60 */
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.cap1 = {
		.pclk =475200000,
		.linelength = 4804,
		.framelength = 3300,
		.startx = 0,
		.starty =0,
		.grabwindow_width = 4224,
		.grabwindow_height = 3168,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 240,	
	},
	.normal_video = {
		.pclk = 237600000,
		.linelength = 2404,
		.framelength = 1648,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2112,
		.grabwindow_height = 1584,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 300,
	},
	.hs_video = {
		.pclk = 237600000,
		.linelength = 2404,
		.framelength =1648,
		.startx = 0,
		.starty = 0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1184,
		.mipi_data_lp2hs_settle_dc = 85,
		.max_framerate = 600,
	},
	.slim_video = {  /*using preview setting*/
		.pclk =237600000,
		.linelength = 2404,
		.framelength = 1648,
		.startx = 0,
		.starty =0,
		.grabwindow_width = 2104,
		.grabwindow_height = 1184,
		.mipi_data_lp2hs_settle_dc =85,
		.max_framerate = 300,
	},
	.margin = 10,
	.min_shutter = 1,
	.max_frame_length = 0xffff,
	.ae_shut_delay_frame = 0,
	.ae_sensor_gain_delay_frame =1,
	.ae_ispGain_delay_frame = 2,
	.ihdr_support = 0,	  //1, support; 0,not support
	.ihdr_le_firstline = 0,  //1,le first ; 0, se first
	.sensor_mode_num = 5,	  //support sensor mode num
	
	.cap_delay_frame = 3, 
	.pre_delay_frame = 3, 
	.video_delay_frame = 3,
	.hs_video_delay_frame = 3,
	.slim_video_delay_frame = 3,
	
	.isp_driving_current = ISP_DRIVING_8MA,
	.sensor_interface_type = SENSOR_INTERFACE_TYPE_MIPI,
	.mipi_sensor_type = MIPI_OPHY_NCSI2, //0,MIPI_OPHY_NCSI2;  1,MIPI_OPHY_CSI2
	.mipi_settle_delay_mode = MIPI_SETTLEDELAY_AUTO, //0,MIPI_SETTLEDELAY_AUTO; 1,MIPI_SETTLEDELAY_MANNUAL
	.sensor_output_dataformat = SENSOR_OUTPUT_FORMAT_RAW_Gr,
	.mclk = 24,
	.mipi_lane_num = SENSOR_MIPI_4_LANE,
	.i2c_addr_table = {0x20,0xff},
};


static imgsensor_struct imgsensor = {
	.mirror = IMAGE_NORMAL,				//mirrorflip information
	.sensor_mode = IMGSENSOR_MODE_INIT, //IMGSENSOR_MODE enum value,record current sensor mode,such as: INIT, Preview, Capture, Video,High Speed Video, Slim Video
	.shutter = 0x3D0,					//current shutter
	.gain = 0x100,						//current gain
	.dummy_pixel = 0,					//current dummypixel
	.dummy_line = 0,					//current dummyline
	.current_fps = 0,  //full size current fps : 24fps for PIP, 30fps for Normal or ZSD
	.autoflicker_en = KAL_FALSE,  //auto flicker enable: KAL_FALSE for disable auto flicker, KAL_TRUE for enable auto flicker
	.test_pattern = KAL_FALSE,		//test pattern mode or not. KAL_FALSE for in test pattern mode, KAL_TRUE for normal output
	.current_scenario_id = MSDK_SCENARIO_ID_CAMERA_PREVIEW,//current scenario id
	.ihdr_mode = 0, //sensor need support LE, SE with HDR feature
	.i2c_write_id = 0x20,
};


/* Sensor output window information */
 SENSOR_WINSIZE_INFO_STRUCT imgsensor_winsize_info[5]=    
 {{ 4224, 3168, 0000, 0000, 4224, 3168, 2112, 1584, 0000, 0000, 2112, 1584, 0000, 0000, 2112, 1584}, // Preview 
  { 4224, 3168, 0000, 0000, 4224, 3168, 4224, 3168, 0000, 0000, 4224, 3168, 0000, 0000, 4224, 3168}, // capture 
  { 4224, 3168, 0000, 0000, 4224, 3168, 2112, 1584, 0000, 0000, 2112, 1584, 0000, 0000, 2112, 1584}, // video 
  { 4224, 3168, 0000, 0000, 4224, 3168, 2112, 1584, 0000, 0000, 2112, 1584, 0000, 0000, 2112, 1584}, //hight speed video 
  { 4224, 3168, 0000, 0000, 4224, 3168, 2112, 1584, 0000, 0000, 2112, 1584, 0000, 0000, 2112, 1584}};// slim video 

#define MIPI_MaxGainIndex 232

static kal_uint16 sensorGainMapping[MIPI_MaxGainIndex][2] = {
	{64  ,256  },
	{66  ,259  },
	{67  ,260  },
	{69  ,263  },
	{71  ,266  },
	{72  ,267  },
	{73  ,268  },
	{76  ,271  },
	{80  ,277  },
	{81  ,278  },
	{82  ,279  },
	{83  ,280  },
	{86  ,283  },
	{90  ,287  },
	{93  ,291  },
	{94  ,292  },
	{95  ,293  },
	{99  ,296  },
	{102 ,299  },
	{103 ,300  },
	{104 ,301  },
	{105 ,302  },
	{106 ,303  },
	{108 ,304  },
	{109 ,305  },
	{110 ,306  },
	{111 ,307  },
	{112 ,308  },
	{113 ,309  },
	{115 ,310  },
	{116 ,311  },
	{117 ,312  },
	{118 ,313  },
	{120 ,314  },
	{121 ,315  },
	{122 ,316  },
	{124 ,317  },
	{125 ,318  },
	{126 ,319  },
	{128 ,320  },
	{129 ,321  },
	{131 ,322  },
	{132 ,323  },
	{133 ,324  },
	{135 ,325  },
	{136 ,326  },
	{138 ,327  },
	{140 ,328  },
	{141 ,329  },
	{142 ,330  },
	{144 ,331  },
	{145 ,332  },
	{147 ,333  },
	{148 ,334  },
	{150 ,335  },
	{152 ,336  },
	{154 ,337  },
	{155 ,338  },
	{157 ,339  },
	{159 ,340  },
	{160 ,341  },
	{162 ,342  },
	{164 ,343  },
	{166 ,344  },
	{167 ,345  },
	{169 ,346  },
	{171 ,347  },
	{173 ,348  },
	{175 ,349  },
	{177 ,350  },
	{179 ,351  },
	{180 ,352  },
	{182 ,353  },
	{184 ,354  },
	{186 ,355  },
	{188 ,356  },
	{190 ,357  },
	{193 ,358  },
	{195 ,359  },
	{196 ,360  },
	{199 ,361  },
	{201 ,362  },
	{203 ,363  },
	{205 ,364  },
	{207 ,365  },
	{210 ,366  },
	{212 ,367  },
	{214 ,368  },
	{217 ,369  },
	{219 ,370  },
	{221 ,371  },
	{224 ,372  },
	{227 ,373  },
	{228 ,374  },
	{231 ,375  },
	{234 ,376  },
	{236 ,377  },
	{239 ,378  },
	{241 ,379  },
	{244 ,380  },
	{246 ,381  },
	{250 ,382  },
	{252 ,383  },
	{255 ,384  },
	{257 ,385  },
	{260 ,386  },
	{263 ,387  },
	{266 ,388  },
	{269 ,389  },
	{272 ,390  },
	{275 ,391  },
	{278 ,392  },
	{281 ,393  },
	{284 ,394  },
	{287 ,395  },
	{290 ,396  },
	{293 ,397  },
	{296 ,398  },
	{300 ,399  },
	{303 ,400  },
	{306 ,401  },
	{309 ,402  },
	{313 ,403  },
	{316 ,404  },
	{319 ,405  },
	{323 ,406  },
	{326 ,407  },
	{330 ,408  },
	{333 ,409  },
	{337 ,410  },
	{341 ,411  },
	{345 ,412  },
	{348 ,413  },
	{352 ,414  },
	{356 ,415  },
	{360 ,416  },
	{364 ,417  },
	{368 ,418  },
	{372 ,419  },
	{376 ,420  },
	{380 ,421  },
	{384 ,422  },
	{388 ,423  },
	{392 ,424  },
	{397 ,425  },
	{401 ,426  },
	{405 ,427  },
	{410 ,428  },
	{414 ,429  },
	{419 ,430  },
	{423 ,431  },
	{428 ,432  },
	{433 ,433  },
	{437 ,434  },
	{442 ,435  },
	{447 ,436  },
	{451 ,437  },
	{456 ,438  },
	{461 ,439  },
	{467 ,440  },
	{472 ,441  },
	{477 ,442  },
	{482 ,443  },
	{487 ,444  },
	{492 ,445  },
	{497 ,446  },
	{503 ,447  },
	{508 ,448  },
	{514 ,449  },
	{520 ,450  },
	{525 ,451  },
	{531 ,452  },
	{536 ,453  },
	{542 ,454  },
	{548 ,455  },
	{554 ,456  },
	{560 ,457  },
	{566 ,458  },
	{572 ,459  },
	{579 ,460  },
	{585 ,461  },
	{591 ,462  },
	{598 ,463  },
	{604 ,464  },
	{611 ,465  },
	{618 ,466  },
	{624 ,467  },
	{631 ,468  },
	{637 ,469  },
	{644 ,470  },
	{652 ,471  },
	{659 ,472  },
	{666 ,473  },
	{673 ,474  },
	{680 ,475  },
	{688 ,476  },
	{695 ,477  },
	{703 ,478  },
	{710 ,479  },
	{718 ,480  },
	{726 ,481  },
	{734 ,482  },
	{742 ,483  },
	{750 ,484  },
	{758 ,485  },
	{766 ,486  },
	{774 ,487  },
	{783 ,488  },
	{791 ,489  },
	{800 ,490  },
	{808 ,491  },
	{817 ,492  },
	{826 ,493  },
	{835 ,494  },
	{844 ,495  },
	{854 ,496  },
	{863 ,497  },
	{872 ,498  },
	{881 ,499  },
	{891 ,500  },
	{900 ,501  },
	{911 ,502  },
	{920 ,503  },
	{931 ,504  },
	{941 ,505  },
	{951 ,506  },
	{961 ,507  },
	{972 ,508  },
	{982 ,509  },
	{993 ,510  },
	{1004,511  },
	{1014,512  }
};


extern int iReadReg(u16 a_u2Addr , u8 * a_puBuff , u16 i2cId);
extern int iWriteReg(u16 a_u2Addr , u32 a_u4Data , u32 a_u4Bytes , u16 i2cId);
static kal_uint16 read_cmos_sensor(kal_uint32 addr)
{
	kal_uint16 get_byte=0;
      iReadReg((u16) addr ,(u8*)&get_byte, imgsensor.i2c_write_id);
      return get_byte;
}

#define write_cmos_sensor(addr, para) iWriteReg((u16) addr , (u32) para , 1,  imgsensor.i2c_write_id)

static void set_dummy()
{
	LOG_INF("dummyline = %d, dummypixels = %d \n", imgsensor.dummy_line, imgsensor.dummy_pixel);
    write_cmos_sensor(0x0104, 1); 
	   
	write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
	write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);	  
    /*   Due to sensor setting restriction,
          basically we don't want you to change the line_length_pck(reg0x342/0x343).
      */
    //write_cmos_sensor(0x0342, imgsensor.line_length >> 8);
	//write_cmos_sensor(0x0343, imgsensor.line_length & 0xFF);

	write_cmos_sensor(0x0104, 0);
  
}	/*	set_dummy  */


static void set_max_framerate(UINT16 framerate,kal_bool min_framelength_en)
{
	kal_int16 dummy_line;
	kal_uint32 frame_length = imgsensor.frame_length;
	//unsigned long flags;

	LOG_INF("framerate = %d, min framelength should enable? \n", framerate,min_framelength_en);
   
	frame_length = imgsensor.pclk / framerate * 10 / imgsensor.line_length;
	spin_lock(&imgsensor_drv_lock);
	if (frame_length >= imgsensor.min_frame_length)
		imgsensor.frame_length = frame_length;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	imgsensor.dummy_line = imgsensor.frame_length - imgsensor.min_frame_length;
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
	   
	spin_lock(&imgsensor_drv_lock);
	if (shutter > imgsensor.min_frame_length - imgsensor_info.margin)		
		imgsensor.frame_length = shutter + imgsensor_info.margin;
	else
		imgsensor.frame_length = imgsensor.min_frame_length;
	if (imgsensor.frame_length > imgsensor_info.max_frame_length)
		imgsensor.frame_length = imgsensor_info.max_frame_length;
	spin_unlock(&imgsensor_drv_lock);
	if (shutter < imgsensor_info.min_shutter) shutter = imgsensor_info.min_shutter;
	
	if (imgsensor.autoflicker_en) { 
		realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
		if(realtime_fps >= 297 && realtime_fps <= 305)
			set_max_framerate(296,0);
		else if(realtime_fps >= 147 && realtime_fps <= 150)
			set_max_framerate(146,0);	
		else {
		// Extend frame length
		//write_cmos_sensor(0x0104, 1); 
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		//write_cmos_sensor(0x0104, 0);
	    }
	} else {
		// Extend frame length 
		//write_cmos_sensor(0x0104, 1); 
		write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
		write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
		//write_cmos_sensor(0x0104, 0);
	}

	// Update Shutter
    //write_cmos_sensor(0x0104, 1);      	
    write_cmos_sensor(0x0202, (shutter >> 8) & 0xFF);
    write_cmos_sensor(0x0203, shutter  & 0xFF);	
    //write_cmos_sensor(0x0104, 0);   
  
	LOG_INF("shutter =%d, framelength =%d\n", shutter,imgsensor.frame_length);

	//LOG_INF("frame_length = %d ", frame_length);
	
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
	kal_uint8 iI;

	for (iI = 0; iI < (MIPI_MaxGainIndex-1); iI++) {
			if(gain <= sensorGainMapping[iI][0]){	
				break;
			}
		}

	return sensorGainMapping[iI][1];
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

	  write_cmos_sensor(0x0104, 1);
    write_cmos_sensor(0x0204, (reg_gain>>8)& 0xFF);
    write_cmos_sensor(0x0205, reg_gain & 0xFF);
      write_cmos_sensor(0x0104, 0);

	
	return gain;
}	/*	set_gain  */

static void ihdr_write_shutter_gain(kal_uint16 le, kal_uint16 se, kal_uint16 gain)
{
	LOG_INF("le:0x%x, se:0x%x, gain:0x%x\n",le,se,gain);
    kal_uint16 realtime_fps = 0;
    kal_uint32 frame_length = 0;
    kal_uint16 reg_gain;
    spin_lock(&imgsensor_drv_lock);
    if (le > imgsensor.min_frame_length - imgsensor_info.margin)       
        imgsensor.frame_length = le + imgsensor_info.margin;
    else
        imgsensor.frame_length = imgsensor.min_frame_length;
    if (imgsensor.frame_length > imgsensor_info.max_frame_length)
        imgsensor.frame_length = imgsensor_info.max_frame_length;
    spin_unlock(&imgsensor_drv_lock);
    if (le < imgsensor_info.min_shutter) le = imgsensor_info.min_shutter;
    if (imgsensor.autoflicker_en) { 
        realtime_fps = imgsensor.pclk / imgsensor.line_length * 10 / imgsensor.frame_length;
        if(realtime_fps >= 297 && realtime_fps <= 305)
            set_max_framerate(296,0);
        else if(realtime_fps >= 147 && realtime_fps <= 150)
            set_max_framerate(146,0);
        else {
        write_cmos_sensor(0x0104, 1); 
        write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
        write_cmos_sensor(0x0104, 0);
        }
    } else {
        write_cmos_sensor(0x0104, 1); 
        write_cmos_sensor(0x0340, imgsensor.frame_length >> 8);
        write_cmos_sensor(0x0341, imgsensor.frame_length & 0xFF);
        write_cmos_sensor(0x0104, 0);
    }
    write_cmos_sensor(0x0104, 1);
    /* Long exposure */
    write_cmos_sensor(0x0202, (le >> 8) & 0xFF);
    write_cmos_sensor(0x0203, le  & 0xFF);
    /* Short exposure */
    write_cmos_sensor(0x0224, (se >> 8) & 0xFF);
    write_cmos_sensor(0x0225, se  & 0xFF); 
    reg_gain = gain2reg(gain);
    spin_lock(&imgsensor_drv_lock);
    imgsensor.gain = reg_gain; 
    spin_unlock(&imgsensor_drv_lock);
    /* Global analog Gain for Long expo*/
    write_cmos_sensor(0x0204, (reg_gain>>8)& 0xFF);
    write_cmos_sensor(0x0205, reg_gain & 0xFF);
    /* Global analog Gain for Short expo*/
    write_cmos_sensor(0x0216, (reg_gain>>8)& 0xFF);
    write_cmos_sensor(0x0217, reg_gain & 0xFF);
    write_cmos_sensor(0x0104, 0);

}



static void set_mirror_flip(kal_uint8 image_mirror)
{
	LOG_INF("image_mirror = %d\n", image_mirror);

    kal_uint8 flip_mirror = 0x00;       
    flip_mirror = read_cmos_sensor(0x0101);


	switch(image_mirror)
    {

       case IMAGE_H_MIRROR: //IMAGE_NORMAL:
           flip_mirror &= 0xFD;
           write_cmos_sensor(0x0101, (flip_mirror | (0x01)));  //Set mirror
           break;
           
       case IMAGE_NORMAL://IMAGE_V_MIRROR:
           write_cmos_sensor(0x0101, (flip_mirror & (0xFC)));//Set normal
           break;
           
       case IMAGE_HV_MIRROR://IMAGE_H_MIRROR:
           write_cmos_sensor(0x0101, (flip_mirror | (0x03)));  //Set mirror & flip
           break;
           
       case IMAGE_V_MIRROR://IMAGE_HV_MIRROR:
           flip_mirror &= 0xFE;
           write_cmos_sensor(0x0101, (flip_mirror | (0x02)));  //Set flip
           break;

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
static void night_mode(kal_bool enable)
{
/*No Need to implement this function*/ 
}	/*	night_mode	*/

static void sensor_init(void)
{
	LOG_INF("MN34152_Sensor_Init enter :\n ");
    /* 1)Iniial reg setting */
    write_cmos_sensor(0x300A,0x09);
    write_cmos_sensor(0x300B,0x03);
    write_cmos_sensor(0x0304,0x00);
    write_cmos_sensor(0x0305,0x03);
    write_cmos_sensor(0x0306,0x00);
    write_cmos_sensor(0x0307,0xC6);
    write_cmos_sensor(0x3004,0x01);
    write_cmos_sensor(0x3005,0xDF);
    write_cmos_sensor(0x3000,0x03);
    
    write_cmos_sensor(0x3007,0x40);
    write_cmos_sensor(0x3000,0x43);
    write_cmos_sensor(0x300A,0x08);
    msleep(80);
    write_cmos_sensor(0x0102,0x00);
    write_cmos_sensor(0x0103,0x00);
    write_cmos_sensor(0x0104,0x00);
    write_cmos_sensor(0x0105,0x01);
    write_cmos_sensor(0x0110,0x00);
    write_cmos_sensor(0x00FF,0x00);
    write_cmos_sensor(0x0112,0x0A);
    write_cmos_sensor(0x0113,0x0A);
    write_cmos_sensor(0x0120,0x00);
    write_cmos_sensor(0x0300,0x00);
    write_cmos_sensor(0x0301,0x0A);
    write_cmos_sensor(0x0302,0x00);
    write_cmos_sensor(0x0303,0x01);
    write_cmos_sensor(0x3006,0x80);
    write_cmos_sensor(0x3008,0x00);
    write_cmos_sensor(0x3012,0x00);
    write_cmos_sensor(0x3013,0x00);
    write_cmos_sensor(0x3011,0x00);
    write_cmos_sensor(0x300D,0x00);
    write_cmos_sensor(0x300E,0x00);
    write_cmos_sensor(0x300F,0x88);
    write_cmos_sensor(0x3014,0x10);
    write_cmos_sensor(0x3015,0x00);
    write_cmos_sensor(0x3016,0x23);
    write_cmos_sensor(0x3017,0x02);
    write_cmos_sensor(0x3018,0x07);
    write_cmos_sensor(0x0202,0x06);
    write_cmos_sensor(0x0203,0x6C);
    write_cmos_sensor(0x0341,0x70);
    write_cmos_sensor(0x0342,0x12);
    write_cmos_sensor(0x0343,0xC4);
    write_cmos_sensor(0x3030,0xC0);
    write_cmos_sensor(0x3034,0x2A);
    write_cmos_sensor(0x3089,0x0C);
    write_cmos_sensor(0x308A,0xE4);
    write_cmos_sensor(0x308B,0x12);
    write_cmos_sensor(0x308C,0xC4);
    write_cmos_sensor(0x30BB,0x20);
    write_cmos_sensor(0x321B,0x04);
    write_cmos_sensor(0x3252,0x02);
    write_cmos_sensor(0x327C,0xCF);
    write_cmos_sensor(0x329E,0xFB);
    write_cmos_sensor(0x32A6,0x60);
    write_cmos_sensor(0x32C2,0x12);
    write_cmos_sensor(0x3308,0x6A);
    write_cmos_sensor(0x330A,0x78);
    write_cmos_sensor(0x3313,0x14);
    write_cmos_sensor(0x3315,0x14);
    write_cmos_sensor(0x3359,0x30);
    write_cmos_sensor(0x338B,0x3B);
    write_cmos_sensor(0x338D,0x34);
    write_cmos_sensor(0x3390,0x3B);
    write_cmos_sensor(0x3392,0x34);
    write_cmos_sensor(0x339C,0x0A);
    write_cmos_sensor(0x33A3,0x0D);
    write_cmos_sensor(0x33CF,0x07);
    write_cmos_sensor(0x33D7,0x1B);
    write_cmos_sensor(0x340E,0x05);
    write_cmos_sensor(0x343A,0x80);
    write_cmos_sensor(0x343B,0x01);
    write_cmos_sensor(0x343C,0xF6);
    write_cmos_sensor(0x343D,0x90);
    write_cmos_sensor(0x345F,0x54);
    write_cmos_sensor(0x3466,0xD9);
    write_cmos_sensor(0x3612,0x8C);
    write_cmos_sensor(0x3613,0x0C);
    write_cmos_sensor(0x3618,0x21);
    write_cmos_sensor(0x3619,0x03);
    write_cmos_sensor(0x361A,0x64);
    write_cmos_sensor(0x361B,0x03);
    write_cmos_sensor(0x361C,0x67);
    write_cmos_sensor(0x361D,0x03);
    write_cmos_sensor(0x361E,0x78);
    write_cmos_sensor(0x361F,0x03);
    write_cmos_sensor(0x3620,0x7F);
    write_cmos_sensor(0x3621,0x03);
    write_cmos_sensor(0x3629,0xC5);
    write_cmos_sensor(0x362F,0x1D);
    write_cmos_sensor(0x36E1,0x7E);
    write_cmos_sensor(0x36E3,0x7E);
    write_cmos_sensor(0x3900,0x80);
    write_cmos_sensor(0x3905,0x04);
    write_cmos_sensor(0x392D,0x20);
    write_cmos_sensor(0x3930,0x82);
    write_cmos_sensor(0x3936,0x07);
											
	LOG_INF("MN34152_Sensor_Init exit :\n ");
}   /*  MN34152_Sensor_Init  */	/*	sensor_init  */

static void preview_setting(void)
{
	LOG_INF("MN34152PreviewSetting_4lane_30fps enter :\n ");
    /* 11) Streaming OFF register setting */
    write_cmos_sensor(0x0100,0x00);
    msleep(10);
    write_cmos_sensor(0x3000,0x73);
    write_cmos_sensor(0x30BB,0x00);
    write_cmos_sensor(0x0202,0x0C);
    write_cmos_sensor(0x0203,0xDC);
    write_cmos_sensor(0x3032,0x40);
    write_cmos_sensor(0x3042,0xCB);
    write_cmos_sensor(0x306D,0x05);
    write_cmos_sensor(0x306E,0x25);
    write_cmos_sensor(0x306F,0x0C);
    write_cmos_sensor(0x3070,0xE0);
    write_cmos_sensor(0x3071,0x12);
    write_cmos_sensor(0x3072,0xC4);
    write_cmos_sensor(0x3075,0x00);
    write_cmos_sensor(0x3076,0x02);
    write_cmos_sensor(0x3079,0x0C);
    write_cmos_sensor(0x307A,0x61);
    write_cmos_sensor(0x307D,0x06);
    write_cmos_sensor(0x307E,0x30);
    write_cmos_sensor(0x30BB,0x20);
    write_cmos_sensor(0x3226,0x08);
    write_cmos_sensor(0x3228,0x74);
    write_cmos_sensor(0x325D,0x1F);
    write_cmos_sensor(0x325F,0x1D);
    write_cmos_sensor(0x3264,0x37);
    write_cmos_sensor(0x3266,0x3E);
    write_cmos_sensor(0x3268,0x3E);
    write_cmos_sensor(0x326A,0x38);
    write_cmos_sensor(0x326C,0x39);
    write_cmos_sensor(0x326E,0x36);
    write_cmos_sensor(0x3270,0x35);
    write_cmos_sensor(0x3272,0x35);
    write_cmos_sensor(0x3274,0x2E);
    write_cmos_sensor(0x3276,0x2D);
    write_cmos_sensor(0x3278,0x2F);
    write_cmos_sensor(0x327A,0x2C);
    write_cmos_sensor(0x327D,0x66);
    write_cmos_sensor(0x327E,0x30);
    write_cmos_sensor(0x327F,0x05);
    write_cmos_sensor(0x3280,0x55);
    write_cmos_sensor(0x3281,0x25);
    write_cmos_sensor(0x3282,0x23);
    write_cmos_sensor(0x3283,0x03);
    write_cmos_sensor(0x3302,0x2B);
    write_cmos_sensor(0x3304,0x2B);
    write_cmos_sensor(0x330E,0x4D);
    write_cmos_sensor(0x3321,0x48);
    write_cmos_sensor(0x3322,0x06);
    write_cmos_sensor(0x3327,0x70);
    write_cmos_sensor(0x3328,0x06);
    write_cmos_sensor(0x3349,0xC5);
    write_cmos_sensor(0x334F,0x1D);
    write_cmos_sensor(0x3353,0x18);
    write_cmos_sensor(0x335A,0x2B);
    write_cmos_sensor(0x335E,0x3B);
    write_cmos_sensor(0x335F,0x3B);
    write_cmos_sensor(0x3360,0x06);
    write_cmos_sensor(0x3361,0x29);
    write_cmos_sensor(0x337A,0x3B);
    write_cmos_sensor(0x337B,0x3B);
    write_cmos_sensor(0x3381,0x29);
    write_cmos_sensor(0x3382,0x29);
    write_cmos_sensor(0x3385,0x06);
    write_cmos_sensor(0x3387,0x26);
    write_cmos_sensor(0x3388,0x0E);
    write_cmos_sensor(0x3389,0x3B);
    write_cmos_sensor(0x338A,0x29);
    write_cmos_sensor(0x3396,0x06);
    write_cmos_sensor(0x3397,0x06);
    write_cmos_sensor(0x33AB,0x18);
    write_cmos_sensor(0x33BC,0x18);
    write_cmos_sensor(0x33CA,0x01);
    write_cmos_sensor(0x33CC,0x64);
    write_cmos_sensor(0x33D0,0x76);
    write_cmos_sensor(0x33D3,0x11);
    write_cmos_sensor(0x33D4,0x13);
    write_cmos_sensor(0x33D8,0x30);
    write_cmos_sensor(0x33D9,0x17);
    write_cmos_sensor(0x33DA,0x11);
    write_cmos_sensor(0x33DB,0x1D);
    write_cmos_sensor(0x33DC,0x10);
    write_cmos_sensor(0x33DD,0x36);
    write_cmos_sensor(0x33DE,0x29);
    write_cmos_sensor(0x33DF,0x29);
    write_cmos_sensor(0x33E1,0x7E);
    write_cmos_sensor(0x33E2,0x03);
    write_cmos_sensor(0x33E3,0x7E);
    write_cmos_sensor(0x33E4,0x03);
    write_cmos_sensor(0x3410,0xF6);
    write_cmos_sensor(0x341D,0x34);
    write_cmos_sensor(0x3421,0x10);
    write_cmos_sensor(0x3422,0x0B);
    write_cmos_sensor(0x3423,0xD0);
    write_cmos_sensor(0x3424,0x80);
    write_cmos_sensor(0x3425,0x01);
    write_cmos_sensor(0x3426,0xF6);
    write_cmos_sensor(0x3427,0x90);
    write_cmos_sensor(0x3428,0x81);
    write_cmos_sensor(0x342C,0x60);
    write_cmos_sensor(0x342D,0x83);
    write_cmos_sensor(0x342E,0x0D);
    write_cmos_sensor(0x342F,0xD1);
    write_cmos_sensor(0x3467,0xFD);
    write_cmos_sensor(0x346A,0xBB);
    write_cmos_sensor(0x3471,0x40);
    write_cmos_sensor(0x3472,0x60);
    write_cmos_sensor(0x3473,0x30);
    write_cmos_sensor(0x3474,0x1C);
    write_cmos_sensor(0x3475,0x04);
    write_cmos_sensor(0x3476,0x06);
    write_cmos_sensor(0x347E,0x01);
    write_cmos_sensor(0x347F,0x00);
    write_cmos_sensor(0x3912,0x13);
    write_cmos_sensor(0x3914,0x30);
    write_cmos_sensor(0x3916,0x10);
    write_cmos_sensor(0x391C,0x02);
    write_cmos_sensor(0x391D,0x13);
    write_cmos_sensor(0x391E,0x04);
    write_cmos_sensor(0x391F,0x01);
    write_cmos_sensor(0x3920,0x0A);
    write_cmos_sensor(0x3921,0x07);
    write_cmos_sensor(0x3922,0x03);
    write_cmos_sensor(0x3923,0x03);
    write_cmos_sensor(0x3924,0x08);
    write_cmos_sensor(0x3925,0x04);
    write_cmos_sensor(0x30BB,0x60);
    write_cmos_sensor(0x331F,0x17);
    write_cmos_sensor(0x363E,0x4C);
    write_cmos_sensor(0x363F,0x06);
    write_cmos_sensor(0x3640,0x4C);
    write_cmos_sensor(0x3641,0x06);
    write_cmos_sensor(0x3000,0xF3);
	
    /* 8) Streaming On register setting */
    write_cmos_sensor(0x0100,0x01);
    msleep(10);

	LOG_INF("MN34152PreviewSetting exit :\n ");
}   /*  preview_setting  */

static void capture_setting(kal_uint16 currefps)
{
	LOG_INF("E! currefps:%d\n",currefps);
    /* 11) Streaming OFF register setting */
    write_cmos_sensor(0x0100,0x00);
    msleep(10);
    write_cmos_sensor(0x3000,0x73);
    write_cmos_sensor(0x30BB,0x01);
    write_cmos_sensor(0x0202,0x0C);
    write_cmos_sensor(0x0203,0xE0);
    write_cmos_sensor(0x3032,0x40);
    write_cmos_sensor(0x3042,0xCB);
    write_cmos_sensor(0x3226,0x08);
    write_cmos_sensor(0x3228,0x74);
    write_cmos_sensor(0x325D,0x1F);
    write_cmos_sensor(0x325F,0x1D);
    write_cmos_sensor(0x3264,0x37);
    write_cmos_sensor(0x3266,0x3E);
    write_cmos_sensor(0x3268,0x3E);
    write_cmos_sensor(0x326A,0x38);
    write_cmos_sensor(0x326C,0x39);
    write_cmos_sensor(0x326E,0x36);
    write_cmos_sensor(0x3270,0x35);
    write_cmos_sensor(0x3272,0x35);
    write_cmos_sensor(0x3274,0x2E);
    write_cmos_sensor(0x3276,0x2D);
    write_cmos_sensor(0x3278,0x2F);
    write_cmos_sensor(0x327A,0x2C);
    write_cmos_sensor(0x327D,0x66);
    write_cmos_sensor(0x327E,0x30);
    write_cmos_sensor(0x327F,0x05);
    write_cmos_sensor(0x3280,0x55);
    write_cmos_sensor(0x3281,0x25);
    write_cmos_sensor(0x3282,0x23);
    write_cmos_sensor(0x3283,0x03);
    write_cmos_sensor(0x3321,0x8A);
    write_cmos_sensor(0x3322,0x0C);
    write_cmos_sensor(0x334F,0x1D);
    write_cmos_sensor(0x3353,0x18);
    write_cmos_sensor(0x335A,0x2B);
    write_cmos_sensor(0x335E,0x3B);
    write_cmos_sensor(0x335F,0x3B);
    write_cmos_sensor(0x3360,0x06);
    write_cmos_sensor(0x3361,0x29);
    write_cmos_sensor(0x337A,0x3B);
    write_cmos_sensor(0x337B,0x3B);
    write_cmos_sensor(0x3381,0x29);
    write_cmos_sensor(0x3382,0x29);
    write_cmos_sensor(0x3385,0x06);
    write_cmos_sensor(0x3387,0x26);
    write_cmos_sensor(0x3388,0x0E);
    write_cmos_sensor(0x3389,0x3B);
    write_cmos_sensor(0x338A,0x29);
    write_cmos_sensor(0x3396,0x06);
    write_cmos_sensor(0x3397,0x06);
    write_cmos_sensor(0x33AB,0x18);
    write_cmos_sensor(0x33BC,0x18);
    write_cmos_sensor(0x33CA,0x01);
    write_cmos_sensor(0x33CC,0x64);
    write_cmos_sensor(0x33D0,0x76);
    write_cmos_sensor(0x33D3,0x11);
    write_cmos_sensor(0x33D4,0x13);
    write_cmos_sensor(0x33D8,0x30);
    write_cmos_sensor(0x33D9,0x17);
    write_cmos_sensor(0x33DA,0x11);
    write_cmos_sensor(0x33DB,0x1D);
    write_cmos_sensor(0x33DC,0x10);
    write_cmos_sensor(0x33DD,0x36);
    write_cmos_sensor(0x33DE,0x29);
    write_cmos_sensor(0x33DF,0x29);
    write_cmos_sensor(0x3467,0xFD);
    write_cmos_sensor(0x346A,0xBB);
    write_cmos_sensor(0x3471,0x40);
    write_cmos_sensor(0x3472,0x60);
    write_cmos_sensor(0x3473,0x30);
    write_cmos_sensor(0x3474,0x1C);
    write_cmos_sensor(0x3475,0x04);
    write_cmos_sensor(0x3476,0x06);
    write_cmos_sensor(0x347E,0x01);
    write_cmos_sensor(0x347F,0x00);
    write_cmos_sensor(0x3912,0x13);
    write_cmos_sensor(0x3914,0x30);
    write_cmos_sensor(0x3916,0x10);
    write_cmos_sensor(0x391C,0x05);
    write_cmos_sensor(0x391D,0x26);
    write_cmos_sensor(0x391E,0x08);
    write_cmos_sensor(0x391F,0x01);
    write_cmos_sensor(0x3920,0x0F);
    write_cmos_sensor(0x3921,0x0E);
    write_cmos_sensor(0x3922,0x07);
    write_cmos_sensor(0x3923,0x06);
    write_cmos_sensor(0x3924,0x0F);
    write_cmos_sensor(0x3925,0x09);
    write_cmos_sensor(0x30BB,0x61);
    write_cmos_sensor(0x331F,0x29);
    write_cmos_sensor(0x363E,0x8E);
    write_cmos_sensor(0x363F,0x0C);
    write_cmos_sensor(0x3640,0x8E);
    write_cmos_sensor(0x3641,0x0C);
    write_cmos_sensor(0x3000,0xF3);
	
    /* 8) Streaming On register setting */
    write_cmos_sensor(0x0100,0x01);
    msleep(10);
    LOG_INF("capture_setting exit :\n ");
}

static void normal_video_setting(kal_uint16 currefps)
{
    LOG_INF("Mnormal_video_setting enter :\n ");
    
    preview_setting();

    LOG_INF("normal_video_setting exit :\n ");
}


static void hs_video_setting()
{
	LOG_INF("E\n");
	//2112x1584 60fps
    /* 11) Streaming OFF register setting */
    write_cmos_sensor(0x0100,0x00);
    msleep(10);
	write_cmos_sensor(0x3000,0x73);
	write_cmos_sensor(0x30BB,0x00);
	write_cmos_sensor(0x0202,0x06);
	write_cmos_sensor(0x0203,0x6C);
	write_cmos_sensor(0x3032,0x40);
	write_cmos_sensor(0x3042,0xCB);
	write_cmos_sensor(0x306D,0x05);
	write_cmos_sensor(0x306E,0x25);
	write_cmos_sensor(0x306F,0x06);
	write_cmos_sensor(0x3070,0x70);
	write_cmos_sensor(0x3071,0x12);
	write_cmos_sensor(0x3072,0xC4);
	write_cmos_sensor(0x3075,0x00);
	write_cmos_sensor(0x3076,0x02);
	write_cmos_sensor(0x3079,0x0C);
	write_cmos_sensor(0x307A,0x61);
	write_cmos_sensor(0x307D,0x06);
	write_cmos_sensor(0x307E,0x30);
	write_cmos_sensor(0x30BB,0x20);
	write_cmos_sensor(0x3226,0x08);
	write_cmos_sensor(0x3228,0x74);
	write_cmos_sensor(0x325D,0x1F);
	write_cmos_sensor(0x325F,0x1D);
	write_cmos_sensor(0x3264,0x37);
	write_cmos_sensor(0x3266,0x3E);
	write_cmos_sensor(0x3268,0x3E);
	write_cmos_sensor(0x326A,0x38);
	write_cmos_sensor(0x326C,0x39);
	write_cmos_sensor(0x326E,0x36);
	write_cmos_sensor(0x3270,0x35);
	write_cmos_sensor(0x3272,0x35);
	write_cmos_sensor(0x3274,0x2E);
	write_cmos_sensor(0x3276,0x2D);
	write_cmos_sensor(0x3278,0x2F);
	write_cmos_sensor(0x327A,0x2C);
	write_cmos_sensor(0x327D,0x66);
	write_cmos_sensor(0x327E,0x30);
	write_cmos_sensor(0x327F,0x05);
	write_cmos_sensor(0x3280,0x55);
	write_cmos_sensor(0x3281,0x25);
	write_cmos_sensor(0x3282,0x23);
	write_cmos_sensor(0x3283,0x03);
	write_cmos_sensor(0x3302,0x2B);
	write_cmos_sensor(0x3304,0x2B);
	write_cmos_sensor(0x330E,0x4D);
	write_cmos_sensor(0x3321,0x48);
	write_cmos_sensor(0x3322,0x06);
	write_cmos_sensor(0x3327,0x70);
	write_cmos_sensor(0x3328,0x06);
	write_cmos_sensor(0x3349,0xC5);
	write_cmos_sensor(0x334F,0x1D);
	write_cmos_sensor(0x3353,0x18);
	write_cmos_sensor(0x335A,0x2B);
	write_cmos_sensor(0x335E,0x3B);
	write_cmos_sensor(0x335F,0x3B);
	write_cmos_sensor(0x3360,0x06);
	write_cmos_sensor(0x3361,0x29);
	write_cmos_sensor(0x337A,0x3B);
	write_cmos_sensor(0x337B,0x3B);
	write_cmos_sensor(0x3381,0x29);
	write_cmos_sensor(0x3382,0x29);
	write_cmos_sensor(0x3385,0x06);
	write_cmos_sensor(0x3387,0x26);
	write_cmos_sensor(0x3388,0x0E);
	write_cmos_sensor(0x3389,0x3B);
	write_cmos_sensor(0x338A,0x29);
	write_cmos_sensor(0x3396,0x06);
	write_cmos_sensor(0x3397,0x06);
	write_cmos_sensor(0x33AB,0x18);
	write_cmos_sensor(0x33BC,0x18);
	write_cmos_sensor(0x33CA,0x01);
	write_cmos_sensor(0x33CC,0x64);
	write_cmos_sensor(0x33D0,0x76);
	write_cmos_sensor(0x33D3,0x11);
	write_cmos_sensor(0x33D4,0x13);
	write_cmos_sensor(0x33D8,0x30);
	write_cmos_sensor(0x33D9,0x17);
	write_cmos_sensor(0x33DA,0x11);
	write_cmos_sensor(0x33DB,0x1D);
	write_cmos_sensor(0x33DC,0x10);
	write_cmos_sensor(0x33DD,0x36);
	write_cmos_sensor(0x33DE,0x29);
	write_cmos_sensor(0x33DF,0x29);
	write_cmos_sensor(0x33E1,0x7E);
	write_cmos_sensor(0x33E2,0x03);
	write_cmos_sensor(0x33E3,0x7E);
	write_cmos_sensor(0x33E4,0x03);
	write_cmos_sensor(0x3410,0xF6);
	write_cmos_sensor(0x341D,0x34);
	write_cmos_sensor(0x3421,0x10);
	write_cmos_sensor(0x3422,0x0B);
	write_cmos_sensor(0x3423,0xD0);
	write_cmos_sensor(0x3424,0x80);
	write_cmos_sensor(0x3425,0x01);
	write_cmos_sensor(0x3426,0xF6);
	write_cmos_sensor(0x3427,0x90);
	write_cmos_sensor(0x3428,0x81);
	write_cmos_sensor(0x342C,0x60);
	write_cmos_sensor(0x342D,0x83);
	write_cmos_sensor(0x342E,0x0D);
	write_cmos_sensor(0x342F,0xD1);
	write_cmos_sensor(0x3467,0xFD);
	write_cmos_sensor(0x346A,0xBB);
	write_cmos_sensor(0x3471,0x40);
	write_cmos_sensor(0x3472,0x60);
	write_cmos_sensor(0x3473,0x30);
	write_cmos_sensor(0x3474,0x1C);
	write_cmos_sensor(0x3475,0x04);
	write_cmos_sensor(0x3476,0x06);
	write_cmos_sensor(0x347E,0x01);
	write_cmos_sensor(0x347F,0x00);
	write_cmos_sensor(0x3912,0x13);
	write_cmos_sensor(0x3914,0x30);
	write_cmos_sensor(0x3916,0x10);
	write_cmos_sensor(0x391C,0x02);
	write_cmos_sensor(0x391D,0x13);
	write_cmos_sensor(0x391E,0x04);
	write_cmos_sensor(0x391F,0x01);
	write_cmos_sensor(0x3920,0x0A);
	write_cmos_sensor(0x3921,0x07);
	write_cmos_sensor(0x3922,0x03);
	write_cmos_sensor(0x3923,0x03);
	write_cmos_sensor(0x3924,0x08);
	write_cmos_sensor(0x3925,0x04);
	write_cmos_sensor(0x30BB,0x60);
	write_cmos_sensor(0x331F,0x17);
	write_cmos_sensor(0x363E,0x4C);
	write_cmos_sensor(0x363F,0x06);
	write_cmos_sensor(0x3640,0x4C);
	write_cmos_sensor(0x3641,0x06);
	write_cmos_sensor(0x3000,0xF3);
	
    /* 8) Streaming On register setting */
    write_cmos_sensor(0x0100,0x01);
    msleep(10);

}

static void slim_video_setting()
{
	LOG_INF("E\n");
	preview_setting();
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
	kal_uint8 retry = 3;
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			*sensor_id = read_cmos_sensor(0x0001);
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
	LOG_INF("PLATFORM:MT6595,MIPI 2LANE\n");
	LOG_INF("preview 1280*960@30fps,864Mbps/lane; video 1280*960@30fps,864Mbps/lane; capture 5M@30fps,864Mbps/lane\n");
	
	//sensor have two i2c address 0x6c 0x6d & 0x21 0x20, we should detect the module used i2c address
	while (imgsensor_info.i2c_addr_table[i] != 0xff) {
		spin_lock(&imgsensor_drv_lock);
		imgsensor.i2c_write_id = imgsensor_info.i2c_addr_table[i];
		spin_unlock(&imgsensor_drv_lock);
		do {
			sensor_id = read_cmos_sensor(0x0001);
			if (sensor_id == imgsensor_info.sensor_id) {				
				LOG_INF("i2c write id: 0x%x, sensor id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);	  
				break;
			}	
			LOG_INF("Read sensor id fail, id: 0x%x\n", imgsensor.i2c_write_id,sensor_id);
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
	imgsensor.shutter = 0x3D0;
	imgsensor.gain = 0x100;
	imgsensor.pclk = imgsensor_info.pre.pclk;
	imgsensor.frame_length = imgsensor_info.pre.framelength;
	imgsensor.line_length = imgsensor_info.pre.linelength;
	imgsensor.min_frame_length = imgsensor_info.pre.framelength;
	imgsensor.dummy_pixel = 0;
	imgsensor.dummy_line = 0;
	imgsensor.ihdr_mode = 0;
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
	//hs_video_setting();

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
	LOG_INF("E\n");
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_CAPTURE;
	if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {//PIP capture: 24fps for less than 13M, 20fps for 16M,15fps for 20M
		imgsensor.pclk = imgsensor_info.cap1.pclk;
		imgsensor.line_length = imgsensor_info.cap1.linelength;
		imgsensor.frame_length = imgsensor_info.cap1.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap1.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	} else {
		if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
			LOG_INF("Warning: current_fps %d fps is not support, so use cap1's setting: %d fps!\n",imgsensor_info.cap1.max_framerate/10);
		imgsensor.pclk = imgsensor_info.cap.pclk;
		imgsensor.line_length = imgsensor_info.cap.linelength;
		imgsensor.frame_length = imgsensor_info.cap.framelength;  
		imgsensor.min_frame_length = imgsensor_info.cap.framelength;
		imgsensor.autoflicker_en = KAL_FALSE;
	}
	spin_unlock(&imgsensor_drv_lock);
    // 
	capture_setting(imgsensor.current_fps); 
	
	return ERROR_NONE;
}	/* capture() */
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
}	/*	normal_video   */

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
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	hs_video_setting();
	
	return ERROR_NONE;
}	/*	hs_video   */

static kal_uint32 slim_video(MSDK_SENSOR_EXPOSURE_WINDOW_STRUCT *image_window,
					  MSDK_SENSOR_CONFIG_STRUCT *sensor_config_data)
{
	LOG_INF("E\n");
	
	spin_lock(&imgsensor_drv_lock);
	imgsensor.sensor_mode = IMGSENSOR_MODE_SLIM_VIDEO;
	imgsensor.pclk = imgsensor_info.slim_video.pclk;
	//imgsensor.video_mode = KAL_TRUE;
	imgsensor.line_length = imgsensor_info.slim_video.linelength;
	imgsensor.frame_length = imgsensor_info.slim_video.framelength; 
	imgsensor.min_frame_length = imgsensor_info.slim_video.framelength;
	imgsensor.dummy_line = 0;
	imgsensor.dummy_pixel = 0;
	//imgsensor.current_fps = 300;
	imgsensor.autoflicker_en = KAL_FALSE;
	spin_unlock(&imgsensor_drv_lock);
	slim_video_setting();
	
	return ERROR_NONE;
}	/*	slim_video	 */



static kal_uint32 get_resolution(MSDK_SENSOR_RESOLUTION_INFO_STRUCT *sensor_resolution)
{
	LOG_INF("E\n");
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
	return ERROR_NONE;
}	/*	get_resolution	*/

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
	sensor_info->SensorInterruptDelayLines = 4; /* not use */
	sensor_info->SensorResetActiveHigh = FALSE; /* not use */
	sensor_info->SensorResetDelayCount = 5; /* not use */

	sensor_info->SensroInterfaceType = imgsensor_info.sensor_interface_type;
	//sensor_info->MIPIsensorType = imgsensor_info.mipi_sensor_type;
	//sensor_info->SettleDelayMode = imgsensor_info.mipi_settle_delay_mode;
	sensor_info->SensorOutputDataFormat = imgsensor_info.sensor_output_dataformat;

	sensor_info->CaptureDelayFrame = imgsensor_info.cap_delay_frame; 
	sensor_info->PreviewDelayFrame = imgsensor_info.pre_delay_frame; 
	sensor_info->VideoDelayFrame = imgsensor_info.video_delay_frame;
	sensor_info->HighSpeedVideoDelayFrame = imgsensor_info.hs_video_delay_frame;
	sensor_info->SlimVideoDelayFrame = imgsensor_info.slim_video_delay_frame;

	sensor_info->SensorMasterClockSwitch = 0; /* not use */
	sensor_info->SensorDrivingCurrent = imgsensor_info.isp_driving_current;
	
	sensor_info->AEShutDelayFrame = imgsensor_info.ae_shut_delay_frame; 		 /* The frame of setting shutter default 0 for TG int */
	sensor_info->AESensorGainDelayFrame = imgsensor_info.ae_sensor_gain_delay_frame;	/* The frame of setting sensor gain */
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
	sensor_info->SensorHightSampling = 0;	// 0 is default 1x 
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
}	/*	get_info  */


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
        	  if (imgsensor.current_fps == imgsensor_info.cap1.max_framerate) {
                frame_length = imgsensor_info.cap1.pclk / framerate * 10 / imgsensor_info.cap1.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap1.framelength) ? (frame_length - imgsensor_info.cap1.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap1.framelength + imgsensor.dummy_line;
		            imgsensor.min_frame_length = imgsensor.frame_length;
		            spin_unlock(&imgsensor_drv_lock);
            } else {
        		    if (imgsensor.current_fps != imgsensor_info.cap.max_framerate)
                    LOG_INF("Warning: current_fps %d fps is not support, so use cap's setting: %d fps!\n",framerate,imgsensor_info.cap.max_framerate/10);
                frame_length = imgsensor_info.cap.pclk / framerate * 10 / imgsensor_info.cap.linelength;
                spin_lock(&imgsensor_drv_lock);
		            imgsensor.dummy_line = (frame_length > imgsensor_info.cap.framelength) ? (frame_length - imgsensor_info.cap.framelength) : 0;
		            imgsensor.frame_length = imgsensor_info.cap.framelength + imgsensor.dummy_line;
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
			break;		
		default:  //coding with  preview scenario by default
			frame_length = imgsensor_info.pre.pclk / framerate * 10 / imgsensor_info.pre.linelength;
			spin_lock(&imgsensor_drv_lock);
			imgsensor.dummy_line = (frame_length > imgsensor_info.pre.framelength) ? (frame_length - imgsensor_info.pre.framelength) : 0;
			imgsensor.frame_length = imgsensor_info.pre.framelength + imgsensor.dummy_line;
			imgsensor.min_frame_length = imgsensor.frame_length;
			spin_unlock(&imgsensor_drv_lock);
			set_dummy();	
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

static kal_uint32 set_test_pattern_mode(kal_bool enable)
{
	LOG_INF("enable: %d\n", enable);

	if (enable) {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x0601,0x0002);
	} else {
		// 0x5E00[8]: 1 enable,  0 disable
		// 0x5E00[1:0]; 00 Color bar, 01 Random Data, 10 Square, 11 BLACK
		write_cmos_sensor(0x0601,0x0000);
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
        case SENSOR_FEATURE_SET_HDR:
           // LOG_INF("ihdr enable :%d\n", (BOOL)*feature_data);
           // spin_lock(&imgsensor_drv_lock);
           // imgsensor.ihdr_en = (BOOL)*feature_data;
           // spin_unlock(&imgsensor_drv_lock);
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

//kin0603
UINT32 MN34152_MIPI_RAW_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
//UINT32 MN34152_MIPI_SensorInit(PSENSOR_FUNCTION_STRUCT *pfFunc)
{
	/* To Do : Check Sensor status here */
	if (pfFunc!=NULL)
		*pfFunc=&sensor_func;
	return ERROR_NONE;
}	/*	OV5693_MIPI_RAW_SensorInit	*/
