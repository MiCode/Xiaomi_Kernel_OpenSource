/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 s5k3l8mipiraw_Sensor.h
 *
 * Project:
 * --------
 *	 ALPS
 *	PengtaoFan
 * Description:
 * ------------
 *	 CMOS sensor header file
 *
 ****************************************************************************/
#ifndef _S5K3L8MIPI_SENSOR_H
#define _S5K3L8MIPI_SENSOR_H

/* 表示sensor的几种工作模式状态：init preview capture video hvideo svideo */
typedef enum{
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
	IMGSENSOR_MODE_CUSTOM1,
	IMGSENSOR_MODE_CUSTOM2,
	IMGSENSOR_MODE_CUSTOM3,
	IMGSENSOR_MODE_CUSTOM4,
	IMGSENSOR_MODE_CUSTOM5,
} IMGSENSOR_MODE;

/* 表示几种（不同工作模式状态下）的sensor参数信息 */
typedef struct imgsensor_mode_struct {
	kal_uint32 pclk;
	kal_uint32 linelength;
	kal_uint32 framelength;

	kal_uint8 startx;				/* record different mode's startx of grabwindow */
	kal_uint8 starty;				/* record different mode's startx of grabwindow */

	kal_uint16 grabwindow_width;	/* record different mode's width of grabwindow */
	kal_uint16 grabwindow_height;	/* record different mode's height of grabwindow */

	/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
	kal_uint8 mipi_data_lp2hs_settle_dc;

	/*	 following for GetDefaultFramerateByScenario()	*/
	kal_uint16 max_framerate;

} imgsensor_mode_struct;

/* 表示（当前状态工作模式）下的sensor参数信息 */
/* SENSOR PRIVATE STRUCT FOR VARIABLES*/
typedef struct imgsensor_struct {
	kal_uint8 mirror;

	kal_uint8 sensor_mode;

	kal_uint32 shutter;
	kal_uint16 gain;

	kal_uint32 pclk;

	kal_uint32 frame_length;
	kal_uint32 line_length;

	kal_uint32 min_frame_length;
	kal_uint16 dummy_pixel;
	kal_uint16 dummy_line;

	kal_uint16 current_fps;
	kal_bool   autoflicker_en;
	kal_bool test_pattern;
	MSDK_SCENARIO_ID_ENUM current_scenario_id;
	kal_bool  ihdr_en;

	kal_uint8 i2c_write_id;
} imgsensor_struct;


/* SENSOR PRIVATE STRUCT FOR CONSTANT*/
typedef struct imgsensor_info_struct {
	kal_uint16 sensor_id;
	kal_uint32 checksum_value;
	imgsensor_mode_struct pre;
	imgsensor_mode_struct cap;
	imgsensor_mode_struct cap1;
	imgsensor_mode_struct normal_video;
	imgsensor_mode_struct hs_video;
	imgsensor_mode_struct slim_video;
	imgsensor_mode_struct custom1;
	imgsensor_mode_struct custom2;
	imgsensor_mode_struct custom3;
	imgsensor_mode_struct custom4;
	imgsensor_mode_struct custom5;

	kal_uint8  ae_shut_delay_frame;	/* shutter delay frame for AE cycle */
	kal_uint8  ae_sensor_gain_delay_frame;	/* sensor gain delay frame for AE cycle */
	kal_uint8  ae_ispGain_delay_frame;	/* isp gain delay frame for AE cycle */
	kal_uint8  ihdr_support;
	kal_uint8  ihdr_le_firstline;	/* 1,le first ; 0, se first */
	kal_uint8  sensor_mode_num;		/* support sensor mode num */

	kal_uint8  cap_delay_frame;		/* enter capture delay frame num */
	kal_uint8  pre_delay_frame;		/* enter preview delay frame num */
	kal_uint8  video_delay_frame;	/* enter video delay frame num */
	kal_uint8  hs_video_delay_frame;	/* enter high speed video  delay frame num */
	kal_uint8  slim_video_delay_frame;	/* enter slim video delay frame num */
	kal_uint8  custom1_delay_frame;     /* enter custom1 delay frame num */
	kal_uint8  custom2_delay_frame;     /* enter custom1 delay frame num */
	kal_uint8  custom3_delay_frame;     /* enter custom1 delay frame num */
	kal_uint8  custom4_delay_frame;     /* enter custom1 delay frame num */
	kal_uint8  custom5_delay_frame;     /* enter custom1 delay frame num */

	kal_uint8  margin;				/* sensor framelength & shutter margin */
	kal_uint32 min_shutter;			/* min shutter */
	kal_uint32 max_frame_length;	/* max framelength by sensor register's limitation */

	kal_uint8  isp_driving_current;	/* mclk driving current */
	kal_uint8  sensor_interface_type; /*sensor_interface_type */
	kal_uint8  mipi_sensor_type; /* 0,MIPI_OPHY_NCSI2; 1,MIPI_OPHY_CSI2, default is NCSI2, don't modify this para */
	kal_uint8  mipi_settle_delay_mode; /* 0, high speed signal auto detect; 1, use settle delay,unit is ns, default is auto detect, don't modify this para */
	kal_uint8  sensor_output_dataformat; /*sensor output first pixel color */
	kal_uint8  mclk;				/* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */

	kal_uint8  mipi_lane_num;		/* mipi lane num */
	kal_uint8  i2c_addr_table[5];	/* record sensor support all write id addr, only supprt 4must end with 0xff */
	kal_uint32  i2c_speed;
} imgsensor_info_struct;

/* SENSOR READ/WRITE ID */
/* #define IMGSENSOR_WRITE_ID_1 (0x6c) */
/* #define IMGSENSOR_READ_ID_1  (0x6d) */
/* #define IMGSENSOR_WRITE_ID_2 (0x20) */
/* #define IMGSENSOR_READ_ID_2  (0x21) */

extern int iReadRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData , u16 a_sizeSendData, u16 i2cId);
extern void kdSetI2CSpeed(u16 i2cSpeed);
extern bool S5K3L8_read_eeprom(kal_uint16 addr, BYTE *data, kal_uint32 size);

#endif

/*
PREVIEW:尽量用binning mode， 并告知是Binning average还是Binning sum？AVERAGE
使用GP方式， shutter当前桢设置后，下下桢生效）
Static  DPC ON
slim video   不用到120fps吧。
get sensor id and Open()  has more code need to reused. need to modify the two place
20150513 第一次合入PDAF参数

*/
