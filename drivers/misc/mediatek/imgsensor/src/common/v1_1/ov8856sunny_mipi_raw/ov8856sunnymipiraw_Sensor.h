/*****************************************************************************
 *
 * Filename:
 * ---------
 *	 OV8856SUNNYmipi_Sensor.h
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
#ifndef _OV8856SUNNYMIPI_SENSOR_H
#define _OV8856SUNNYMIPI_SENSOR_H
#include <linux/types.h>
#include "kd_camera_typedef.h"

typedef enum{
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
} IMGSENSOR_MODE;

typedef struct imgsensor_mode_struct {
	kal_uint32 pclk;
	kal_uint32 linelength;
	kal_uint32 framelength;

	kal_uint8 startx;
	kal_uint8 starty;

	kal_uint16 grabwindow_width;
	kal_uint16 grabwindow_height;

	/*	 following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario	*/
	kal_uint8 mipi_data_lp2hs_settle_dc;

	/*	 following for GetDefaultFramerateByScenario()	*/
	kal_uint16 max_framerate;
	kal_uint32 mipi_pixel_rate;

} imgsensor_mode_struct;

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
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id;
	kal_uint8  ihdr_en;

	kal_uint8 i2c_write_id;
	struct IMGSENSOR_AE_FRM_MODE ae_frm_mode;
	kal_uint8 current_ae_effective_frame;

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

	kal_uint8  ae_shut_delay_frame;
	kal_uint8  ae_sensor_gain_delay_frame;
	kal_uint8  ae_ispGain_delay_frame;
	kal_uint8  ihdr_support;
	kal_uint8  ihdr_le_firstline;
	kal_uint8  sensor_mode_num;

	kal_uint8  cap_delay_frame;
	kal_uint8  pre_delay_frame;
	kal_uint8  video_delay_frame;
	kal_uint8  hs_video_delay_frame;
	kal_uint8  slim_video_delay_frame;

	kal_uint8  margin;
	kal_uint32 min_shutter;
	kal_uint32 max_frame_length;

	kal_uint8  isp_driving_current;
	kal_uint8  sensor_interface_type;
	kal_uint8  mipi_sensor_type;
	kal_uint8  mipi_settle_delay_mode;
	kal_uint8  sensor_output_dataformat;
	kal_uint8  mclk;

	kal_uint8  mipi_lane_num;
	kal_uint8  i2c_addr_table[5];

#ifdef CONFIG_RLK_CAM_PERFORMANCE_IMPROVE
	kal_uint32  i2c_speed;
#endif

} imgsensor_info_struct;

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
static kal_uint16 read_cmos_sensor(kal_uint32 addr);
static void write_cmos_sensor(kal_uint32 addr, kal_uint32 para);
 #define PFX "OV8856SUNNY"
 #define LOG_INF(format, args...)    pr_debug(PFX "[%s] " format, __FUNCTION__, ##args)
#endif
