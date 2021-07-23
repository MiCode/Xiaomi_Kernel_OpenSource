/*****************************************************************************
 *
 * Filename:
 * ---------
 *     gc5035mipi_Sensor.h
 *
 * Project:
 * --------
 *     ALPS
 *
 * Description:
 * ------------
 *     CMOS sensor header file
 *
 ****************************************************************************/
#ifndef __GC5035_MIPI_SENSOR_H__
#define __GC5035_MIPI_SENSOR_H__

/* SENSOR MIRROR FLIP INFO */
#define GC5035_MIRROR_FLIP_ENABLE         0
#if GC5035_MIRROR_FLIP_ENABLE
#define GC5035_MIRROR                     0x83
#define GC5035_RSTDUMMY1                  0x03
#define GC5035_RSTDUMMY2                  0xfc
#else
#define GC5035_MIRROR                     0x80
#define GC5035_RSTDUMMY1                  0x02
#define GC5035_RSTDUMMY2                  0x7c
#endif

/* SENSOR PRIVATE INFO FOR GAIN SETTING */
#define GC5035_SENSOR_GAIN_BASE             256
#define GC5035_SENSOR_GAIN_MAX              (16 * GC5035_SENSOR_GAIN_BASE)
#define GC5035_SENSOR_GAIN_MAX_VALID_INDEX  17
#define GC5035_SENSOR_GAIN_MAP_SIZE         17
#define GC5035_SENSOR_DGAIN_BASE            0x100

/* SENSOR PRIVATE INFO FOR OTP SETTINGS */
#define GC5035_OTP_FOR_CUSTOMER            0
#define GC5035_OTP_DEBUG                   0

/* DEBUG */
#if GC5035_OTP_DEBUG
#define GC5035_OTP_START_ADDR              0x0000
#endif

#define GC5035_OTP_DATA_LENGTH             1024

/* OTP FLAG TYPE */
#define GC5035_OTP_FLAG_EMPTY              0x00
#define GC5035_OTP_FLAG_VALID              0x01
#define GC5035_OTP_FLAG_INVALID            0x02
#define GC5035_OTP_FLAG_INVALID2           0x03
#define GC5035_OTP_GET_OFFSET(size)           (size << 3)
#define GC5035_OTP_GET_2BIT_FLAG(flag, bit)   ((flag >> bit) & 0x03)
#define GC5035_OTP_CHECK_1BIT_FLAG(flag, bit) (((flag >> bit) & 0x01) == GC5035_OTP_FLAG_VALID)

#define GC5035_OTP_ID_SIZE                 9
#define GC5035_OTP_ID_DATA_OFFSET          0x0020

/* OTP DPC PARAMETERS */
#define GC5035_OTP_DPC_FLAG_OFFSET         0x0068
#define GC5035_OTP_DPC_TOTAL_NUMBER_OFFSET 0x0070
#define GC5035_OTP_DPC_ERROR_NUMBER_OFFSET 0x0078

/* OTP REGISTER UPDATE PARAMETERS */
#define GC5035_OTP_REG_FLAG_OFFSET         0x0880
#define GC5035_OTP_REG_DATA_OFFSET         0x0888
#define GC5035_OTP_REG_MAX_GROUP           5
#define GC5035_OTP_REG_BYTE_PER_GROUP      5
#define GC5035_OTP_REG_REG_PER_GROUP       2
#define GC5035_OTP_REG_BYTE_PER_REG        2
#define GC5035_OTP_REG_DATA_SIZE           (GC5035_OTP_REG_MAX_GROUP * GC5035_OTP_REG_BYTE_PER_GROUP)
#define GC5035_OTP_REG_REG_SIZE            (GC5035_OTP_REG_MAX_GROUP * GC5035_OTP_REG_REG_PER_GROUP)

/* DPC STRUCTURE */
struct gc5035_dpc_t {
	kal_uint8 flag;
	kal_uint16 total_num;
};

struct gc5035_reg_t {
	kal_uint8 page;
	kal_uint8 addr;
	kal_uint8 value;
};

/* REGISTER UPDATE STRUCTURE */
struct gc5035_reg_update_t {
	kal_uint8 flag;
	kal_uint8 cnt;
	struct gc5035_reg_t reg[GC5035_OTP_REG_REG_SIZE];
};

/* OTP STRUCTURE */
struct gc5035_otp_t {
	kal_uint8 otp_id[GC5035_OTP_ID_SIZE];
	struct gc5035_dpc_t dpc;
	struct gc5035_reg_update_t regs;
};

enum {
	IMGSENSOR_MODE_INIT,
	IMGSENSOR_MODE_PREVIEW,
	IMGSENSOR_MODE_CAPTURE,
	IMGSENSOR_MODE_VIDEO,
	IMGSENSOR_MODE_HIGH_SPEED_VIDEO,
	IMGSENSOR_MODE_SLIM_VIDEO,
};

struct imgsensor_mode_struct {
	kal_uint32 pclk;                /* record different mode's pclk */
	kal_uint32 linelength;          /* record different mode's linelength */
	kal_uint32 framelength;         /* record different mode's framelength */
	kal_uint8  startx;              /* record different mode's startx of grabwindow */
	kal_uint8  starty;              /* record different mode's startx of grabwindow */
	kal_uint16 grabwindow_width;    /* record different mode's width of grabwindow */
	kal_uint16 grabwindow_height;   /* record different mode's height of grabwindow */
	kal_uint32 mipi_pixel_rate;
	/* following for MIPIDataLowPwr2HighSpeedSettleDelayCount by different scenario */
	kal_uint8  mipi_data_lp2hs_settle_dc;
	/* following for GetDefaultFramerateByScenario() */
	kal_uint16 max_framerate;
};

/* SENSOR PRIVATE STRUCT FOR VARIABLES */
struct imgsensor_struct {
	kal_uint8  mirror;                /* mirrorflip information */
	kal_uint8  sensor_mode;           /* record IMGSENSOR_MODE enum value */
	kal_uint32 shutter;               /* current shutter */
	kal_uint16 gain;                  /* current gain */
	kal_uint32 pclk;                  /* current pclk */
	kal_uint32 frame_length;          /* current framelength */
	kal_uint32 line_length;           /* current linelength */
	kal_uint32 min_frame_length;      /* current min  framelength to max framerate */
	kal_uint16 dummy_pixel;           /* current dummypixel */
	kal_uint16 dummy_line;            /* current dummline */
	kal_uint16 current_fps;           /* current max fps */
	kal_bool   autoflicker_en;        /* record autoflicker enable or disable */
	kal_bool   test_pattern;          /* record test pattern mode or not */
	enum MSDK_SCENARIO_ID_ENUM current_scenario_id; /* current scenario id */
	kal_bool   ihdr_en;                        /* ihdr enable or disable */
	kal_uint8  i2c_write_id;          /* record current sensor's i2c write id */
};

/* SENSOR PRIVATE STRUCT FOR CONSTANT */
struct imgsensor_info_struct {
	kal_uint32 sensor_id;                     /* record sensor id defined in Kd_imgsensor.h */
	kal_uint32 checksum_value;                /* checksum value for Camera Auto Test */
	struct imgsensor_mode_struct pre;         /* preview scenario relative information */
	struct imgsensor_mode_struct cap;         /* capture scenario relative information */
	struct imgsensor_mode_struct cap1;        /* capture for PIP 24fps relative information */
	/* capture1 mode must use same framelength, linelength with Capture mode for shutter calculate */
	struct imgsensor_mode_struct cap2;
	struct imgsensor_mode_struct normal_video;/* normal video  scenario relative information */
	struct imgsensor_mode_struct hs_video;    /* high speed video scenario relative information */
	struct imgsensor_mode_struct slim_video;  /* slim video for VT scenario relative information */
	kal_uint8 ae_shut_delay_frame;            /* shutter delay frame for AE cycle */
	kal_uint8 ae_sensor_gain_delay_frame;     /* sensor gain delay frame for AE cycle */
	kal_uint8 ae_ispGain_delay_frame;         /* isp gain delay frame for AE cycle */
	kal_uint8 frame_time_delay_frame;
	kal_uint8 ihdr_support;                   /* 1, support; 0,not support */
	kal_uint8 ihdr_le_firstline;              /* 1,le first ; 0, se first */
	kal_uint8 sensor_mode_num;                /* support sensor mode num */
	kal_uint8 cap_delay_frame;                /* enter capture delay frame num */
	kal_uint8 pre_delay_frame;                /* enter preview delay frame num */
	kal_uint8 video_delay_frame;              /* enter video delay frame num */
	kal_uint8 hs_video_delay_frame;           /* enter high speed video  delay frame num */
	kal_uint8 slim_video_delay_frame;         /* enter slim video delay frame num */
	kal_uint8 margin;                         /* sensor framelength & shutter margin */
	kal_uint32 min_shutter;                   /* min shutter */
	kal_uint32 max_frame_length;              /* max framelength by sensor register's limitation */
	kal_uint8 isp_driving_current;            /* mclk driving current */
	kal_uint8 sensor_interface_type;          /* sensor_interface_type */
	kal_uint8 mipi_sensor_type;
	/* 0, MIPI_OPHY_NCSI2; 1, MIPI_OPHY_CSI2, default is NCSI2, don't modify this para */
	kal_uint8 mipi_settle_delay_mode;
	/* 0, high speed signal auto detect; 1, use settle delay, unit is ns*/
	/* default is auto detect, don't modify this para */
	kal_uint8 sensor_output_dataformat;       /* sensor output first pixel color */
	kal_uint8 mclk;                           /* mclk value, suggest 24 or 26 for 24Mhz or 26Mhz */
	kal_uint8 mipi_lane_num;                  /* mipi lane num */
	kal_uint8 i2c_addr_table[5];
	/* record sensor support all write id addr, only supprt 4must end with 0xff */
	kal_uint32 hs_trail;
};

extern int iReadRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u8 *a_pRecvData, u16 a_sizeRecvData, u16 i2cId);
extern int iWriteRegI2C(u8 *a_pSendData, u16 a_sizeSendData, u16 i2cId);
extern int iWriteReg(u16 a_u2Addr, u32 a_u4Data, u32 a_u4Bytes, u16 i2cId);

#endif
