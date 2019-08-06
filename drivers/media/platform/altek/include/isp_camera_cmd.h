/*
 * File: isp_camera_cmd.h
 * Description: The structure and API definition ISP camera command
 *  It is a header file that define structure and API for ISP camera command
 *
 * Copyright 2019-2030  Altek Semiconductor Corporation
 *
 * History
 *   2013/09/18; Aaron Chuang; Initial version
 *   2013/12/05; Bruce Chung; 2nd version
 */

/*
 * This file is part of al6100.
 *
 * al6100 is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License version 2, as published by
 * the Free Software Foundation.
 *
 * al6100 is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTIBILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU General Public License version 2 for
 * more details.
 *
 * You should have received a copy of the General Public License version 2
 * along with al6100. If not, see https://www.gnu.org/licenses/gpl-2.0.html.
 */


#ifndef _ISPCAMERA_CMD_H_
#define _ISPCAMERA_CMD_H_

/*
 *@addtogroup ISPCameraCmd
 *@{
 */

/******Include File******/


#include "mtype.h"


/******Public Constant Definition******/

#define T_MEMSIZE (936*1024)
#define T_SPI_CMD_LENGTH 64

#define ISPCMD_DUMMYBYTES 4
#define FWVER_INFOSIZE_MAX 34

#define ISPCMD_LENFLDBYTES 2
#define ISPCMD_OPCODEBYTES 2
#define ReportRegCount 27
#define ISPCMD_CKSUMBYTES  2

/*length field size = 2, opcode field size = 2 */
#define ISPCMD_CMDSIZE ((ISPCMD_LENFLDBYTES) + (ISPCMD_OPCODEBYTES))

/*length field size = 2, opcode field size = 2, dummy bytes = 4*/
#define ISPCMD_CMDSIZEWDUMMY (ISPCMD_LENFLDBYTES+\
				ISPCMD_OPCODEBYTES+\
				ISPCMD_DUMMYBYTES)

#define ISPCMD_FILENAME_SIZE 15

#define ISPCMD_EXEBIN_ADDRBYTES 4
#define ISPCMD_EXEBIN_TOTALSIZEBYTES 4
#define ISPCMD_EXEBIN_BLOCKSIZEBYTES 4
#define ISPCMD_EXEBIN_CKSUMBYTES 4
#define ISPCMD_EXEBIN_INFOBYTES (ISPCMD_EXEBIN_ADDRBYTES+\
				ISPCMD_EXEBIN_TOTALSIZEBYTES+\
				ISPCMD_EXEBIN_BLOCKSIZEBYTES+\
				ISPCMD_EXEBIN_CKSUMBYTES)

/* Definition for Error code array number*/
#define MAX_RECERRORCODE_NUM 10

/*log buffer size*/
#define	LEVEL_LOG_BUFFER_SIZE (1024*4)



/*calibration profile*/
#define ISPCMD_CAMERA_SET_SENSORMODE 0x300A
#define ISPCMD_CAMERA_GET_SENSORMODE 0x300B
#define ISPCMD_CAMERA_SET_OUTPUTFORMAT 0x300D
#define ISPCMD_CAMERA_SET_CP_MODE 0x300E
#define ISPCMD_CAMERA_SET_AE_STATISTICS 0x300F
#define ISPCMD_CAMERA_PREVIEWSTREAMONOFF 0x3010
#define ISPCMD_CAMERA_DUALPDYCALCULATIONWEIGHT 0x3011
#define ISPCMD_LED_POWERCONTROL 0x3012
#define ISPCMD_CAMERA_ACTIVE_AE 0x3013
#define ISPCMD_ISP_AECONTROLONOFF 0x3014
#define ISPCMD_CAMERA_SET_FRAMERATELIMITS 0x3015
#define ISPCMD_CAMERA_SET_PERIODDROPFRAME 0x3016
#define ISPCMD_CAMERA_SET_MAX_EXPOSURE 0x3017
#define ISPCMD_CAMERA_SET_AE_TARGET_MEAN 0x3018
#define ISPCMD_CAMERA_FRAME_SYNC_CONTROL 0x3019
#define ISPCMD_CAMERA_SET_SHOT_MODE 0x301A
#define ISPCMD_CAMERA_LIGHTING_CTRL 0x301B
#define ISPCMD_CAMERA_DEPTH_COMPENSATION 0x301C
#define ISPCMD_CAMERA_TRIGGER_DEPTH_PROCESS_CTRL 0x301D
#define ISPCMD_CAMERA_SET_MIN_EXPOSURE 0x301E
#define ISPCMD_CAMERA_SET_MAX_EXPOSURE_SLOPE 0x301F
#define ISPCMD_CAMERA_SET_LED_ACTIVE_DELAY 0x3020
#define ISPCMD_CAMERA_ISPLEDLEVELCONTROLONOFF   0x3021

/* Bulk Data*/
#define ISPCMD_BULK_WRITE_BASICCODE 0x2002
#define ISPCMD_BULK_WRITE_BASICCODE_CODEADDR 0x2003
#define ISPCMD_BULK_WRITE_BASICCODE_CODESIZE 0x2004
#define ISPCMD_BULK_WRITE_BASICCODE_CODESUM 0x2005
#define ISPCMD_BULK_READ_CALIBRATION_DATA 0x2007
#define ISPCMD_BULK_WRITE_BOOTCODE 0x2008
#define ISPCMD_BULK_WRITE_BOOTCODE_SHORT_LEN 0x2009
#define ISPCMD_BULK_READ_MEMORY 0x2101
#define ISPCMD_BULK_READ_COMLOG 0x2102
#define ISPCMD_BULK_WRITE_CALIBRATION_DATA 0x210B
#define ISPCMD_BULK_WRITE_DEPTH_RECTAB_INVRECT 0x210C
#define ISPCMD_BULK_WRITE_CALIBRATION_NO_BLOCK 0x210D
#define ISPCMD_BULK_WRITE_SPINOR_DATA 0x210E

/*basic setting*/
#define ISPCMD_BASIC_SET_DEPTH_3A_INFO 0x10B9
#define ISPCMD_BASIC_SET_DEPTH_AUTO_INTERLEAVE_MODE 0x10BC
#define ISPCMD_BASIC_SET_INTERLEAVE_MODE_DEPTH_TYPE 0x10BD
#define ISPCMD_BASIC_SET_DEPTH_POLISH_LEVEL 0x10BE
#define ISPCMD_BASIC_SET_EXPOSURE_PARAM 0x10BF
#define ISPCMD_BASIC_SET_DEPTH_STREAM_SIZE 0x10C0
/*system cmd*/
#define ISPCMD_SYSTEM_GET_STATUSOFLASTEXECUTEDCOMMAND 0x0015
#define ISPCMD_SYSTEM_GET_ERRORCODE 0x0016
#define ISPCMD_SYSTEM_SET_ISPREGISTER 0x0100
#define ISPCMD_SYSTEM_GET_ISPREGISTER 0x0101
/*#define ISPCMD_SYSTEM_SET_DEBUGCMD 0x0104*/
#define ISPCMD_SYSTEM_SET_COMLOGLEVEL 0x0109
#define ISPCMD_SYSTEM_GET_CHIPTESTREPORT 0x010A
#define ISPCMD_SYSTEM_GET_CHIP_THERMAL		0x0115

/*operarion code*/
#define ISPCMD_MINIISPOPEN 0x4000

/* constants for memory dump mode*/
#define T_MEMDUMP_CPURUN 0
#define T_MEMDUMP_CPUHALT 1

#define EEPROM_BUFFER_ADDRESS		0x00000000
#define EEPROM_BUFFER_SIZE			0x4000
/******Public Type Declaration******/
/*mode id*/
/*define for ISP decide mode*/
enum mini_isp_mode {
	MINI_ISP_MODE_NORMAL = 0x0000,
	MINI_ISP_MODE_E2A = 0x0001,
	MINI_ISP_MODE_A2E = 0x0002,
	MINI_ISP_MODE_LEAVE_BYPASS = 0x0003,
	MINI_ISP_MODE_GET_CHIP_ID = 0x0004,
	MINI_ISP_MODE_SET_CP_MODE = 0x0005,
	MINI_ISP_MODE_LEAVE_CP_MODE = 0x0006,
	MINI_ISP_MODE_CHIP_INIT = 0x0007,
	MINI_ISP_MODE_BYPASS = 0x1000,
	MINI_ISP_MODE_QUARTER_BYPASS = 0x1001,
};

#pragma pack(push, 1)
#pragma pack(1)
struct transferdata {
	u16 opcode;
	void *data;
};

/*camera profile cmd use structure*/
/**
 *@struct isp_cmd_set_sensor_mode(opcode:300A)
 *@brief ISP master cmd for set sensor mode
 *\param sensor_on_off [In],sensor on/off
 *\param scenario_id[In], Scenario ID
 *\param mipi_tx_skew_enable[In],  mipi tx skew on(1)/off(0)
 *\param ae_weighting_table_index[In]
 *\param merge_mode_enable[In]
 *\ bit[0:3] :
 *\ set 0 for normal mode
 *\ set 1 for merge mode, only for image samller than 640X480 case
 *\ set 2 for depth test pattern mode
 *\ bit[4] :
 *\ set 0 for turn on sensor by AP.
 *\ set 1 for turn on sensor by AL6100.
 *\ bit[5] :
 *\ set 0 for disable time stamp function.
 *\ set 1 for enable time stamp function.
 *\       Time stamp data will be put after ir raw data.
 *\       Besides, the original metadata will be cancelled.
 */
#pragma pack(1)
struct isp_cmd_set_sensor_mode {
	u8 sensor_on_off;
	u8 scenario_id;
	u8 mipi_tx_skew;
	u8 ae_weighting_table_index;
	u8 merge_mode_enable;
	u8 reserve[2];
};

/**
 *@struct isp_cmd_get_sensor_mode(opcode:300B)
 *@brief ISP master cmd for get sensor mode
 */
#pragma pack(1)
struct isp_cmd_get_sensor_mode {
	bool on; /* On/off flag*/
	u8 scenario_id; /* scenario mode*/
	u8 reserve[4];
};

/**
 *@struct isp_cmd_set_output_format(opcode:300D)
 *@brief ISP master cmd for set depth output format

 depth_size[In]depth_map_setting = resolution | opereation_mode
 resolution
 0: Disable depth function (Depth engine is disable)
 1: 180p
 2: 360p
 3: 720p
 4: 480p
 opereation_mode,
 0x00: DEPTH_BIT_DG_ONLY
 0x10: DEPTH_BIT_DP
 0x40: DEPTH_BIT_HIGH_DISTORTION_RATE

 reserve[0] [In]depth_process_type_and_qlevel = process_type | quality level
 B[0:2] process_type: value 0x6 as reserve
 B[3:6] quality level: 0 ~ 15

 reserve[1] [In]
 B[0:0] InvRect bypass: set 1 for enable InvRect bypass. set 0 disable.
 return Error code
 */
#pragma pack(1)
struct isp_cmd_set_output_format {
	u8 depth_size;
	u8 reserve[2];
};

/**
 *@struct isp_cmd_ae_statistics(opcode:300F)
 *@brief ae statistics
 *\gr_channel_weight (0~65535)
 *\gb_channel_weight (0~65535)
 *\r_channel_weight (0~65535)
 *\b_channel_weight (0~65535)
 *\shift_bits (0 ~ 15, 0 means no shift)
 */
#pragma pack(1)
struct isp_cmd_ae_statistics {
	u16 gr_channel_weight;
	u16 gb_channel_weight;
	u16 r_channel_weight;
	u16 b_channel_weight;
	u8 shift_bits;
};

/**
 *@struct isp_cmd_preview_stream_on_off(opcode:3010)
 *@briefISP master cmd for control tx stream on or off
 */
#pragma pack(1)
struct isp_cmd_preview_stream_on_off {
	u8 tx0_stream_on_off;
	u8 tx1_stream_on_off;
	u8 reserve;
};

/**
 *@struct isp_cmd_dual_pd_y_calculation_weightings(opcode:3011)
 *@briefISP master cmd for control dual pd y calculation weightings
 */
#pragma pack(1)
struct isp_cmd_dual_pd_y_calculation_weightings {
	u8 y_weight_gr_short;
	u8 y_weight_gr_long;
	u8 y_weight_r_short;
	u8 y_weight_r_long;
	u8 y_weight_b_short;
	u8 y_weight_b_long;
	u8 y_weight_gb_short;
	u8 y_weight_gb_long;
	u8 y_sum_right_shift;
};

/**
 *@struct isp_cmd_led_power_control(opcode:3012)
 *@briefISP master cmd for control led power
 * u8 led_on_off:
 *     0: off
 *     1: always on
 *     2: AP control pulse mode
 *     3: AHCC control pulse mode
 * u8 led_power_level: (0~255)
 * u8 control_projector_id:
 *     0: projector
 *     1:illuminator
 * u32 delay_after_sof,
 *     when led_on_off = 2, use this param to set delay time
 *     between SOF and start pulse time
 * u32 pulse_time:
 *     when led_on_off = 2, use this param to set pulse time
 * u8 control_mode:
 *     when led_on_off = 2,
 *     use this param to decide if pulse time
 *     met next SOF need triggler imediately or not
 * u8 reserved:
 *     reserved; always set to 0
 * u8 rolling_shutter:
 *     when led_on_off = 3, use this param to let
 *     projector/illuminator konw hoe exposure deal
 */
#pragma pack(1)
struct isp_cmd_led_power_control {
	u8 led_on_off;
	u8 led_power_level;
	u8 control_projector_id;
	u32 delay_after_sof;
	u32 pulse_time;
	u8  control_mode;
	u8  reserved;
	u8 rolling_shutter;
};

/**
 *@struct isp_cmd_active_ae(opcode:3013)
 *@briefISP master cmd for avtive AE
 */
#pragma pack(1)
struct isp_cmd_active_ae {
	u8 active_ae;
	u16 f_number_x1000;
};

/**
 *@struct isp_cmd_isp_ae_control_on_off(opcode:3014)
 *@briefISP master cmd for isp AE control on off
 */
#pragma pack(1)
struct isp_cmd_isp_ae_control_on_off {
	u8 isp_ae_control_mode_on_off;
};


/**
 *@struct isp_cmd_frame_rate_limits(opcode:3015)
 *@brief set frame rate limits
 */
#pragma pack(1)
struct isp_cmd_frame_rate_limits {
	u16 main_min_framerate_x100;
	u16 main_max_framerate_x100;
	u16 sub_min_framerate_x100;
	u16 sub_max_framerate_x100;
};

/**
 *@struct isp_cmd_period_drop_frame(opcode:3016)
 *@brief set period drop frame
 */
#pragma pack(1)
struct isp_cmd_period_drop_frame {
	u8 period_drop_type;/* 0: no drop, 1: drop active, 2; drop passive */
};

/**
 *@struct isp_cmd_target_mean(opcode:3018)
 *@brief set target mean
 */
#pragma pack(1)
struct isp_cmd_target_mean {
	u16 target_mean;/* 0~255 */
	u8  ucActiveDevice;
};

#pragma pack(1)
struct isp_cmd_frame_sync_control {
	u8 control_deviceID;	/* bit 0~3: Device ID */
				/* 0: OutDevice_0, 1: OutDevice_1. */
				/* Device name is based on project definition */
				/* bit 4~7: Reference signal source */
				/* 0: Dual sensor sync, 1: GPI */
	u8 delay_framephase;	/* Unit: frame number */
	u8 active_framephase;	/* Unit: frame number */
	u8 deactive_framephase;	/* Unit: frame number */
	u8 active_timelevel;	/* bit 0~3 : Active time level */
		/* 0: one frame; others : value*100(us), range (1~10) */
		/* bit 4~7: mode setting */
		/* 0: normal mode 1: always off mode 2: always on mode */
	u8 reserve[3];
};


/*
Param: shot_mode => value 0: Manual, value 1: Auto + N fps
Param: frame_rate => Please set frame rate when shot mode is Auto + N fps
*/
#pragma pack(1)
struct isp_cmd_set_shot_mode {
	u8 shot_mode;
	u16 frame_rate; /* corresponding frame rate; Ex. 3002 = 30.02 fps */
	u8 reserve[1];
};

/*
*/
#pragma pack(1)
struct isp_cmd_lighting_ctrl {
	u32 cycle_len;	/* effective length in cycle[] */
	struct {
		u8 source;
			/* source of lighting (bit-map) */
			/* 0 means none */
			/* bit 0 represents an IR Project */
			/* bit 1 represents a Flood Illuminator */
		u8 TxDrop;
			/* mipi tx drop control (bit-map) */
			/* 0 means mipi tx0 and mipi tx1 output */
			/* bit 0 represents mipi tx0 drop */
			/* bit 1 represents mipi tx1 drop */
		u16 co_frame_rate;
			/* corresponding frame rate; Ex. 3002 = 30.02 fps */
			/* 0 means the same as sensor */
	} cycle[8];
};

/*basic cmd use structure*/

/**
 *@struct isp_cmd_depth_3a_info(opcode:10B9)
 *@brief depth 3A information
 */
#pragma pack(1)
struct isp_cmd_depth_3a_info {
	u16 hdr_ratio;
	u32 main_cam_exp_time;
	u16 main_cam_exp_gain;
	u16 main_cam_amb_r_gain;
	u16 main_cam_amb_g_gain;
	u16 main_cam_amb_b_gain;
	u16 main_cam_iso;
	u16 main_cam_bv;
	s16 main_cam_vcm_position;
	u8  main_cam_vcm_status;
	u32 sub_cam_exp_time;
	u16 sub_cam_exp_gain;
	u16 sub_cam_amb_r_gain;
	u16 sub_cam_amb_g_gain;
	u16 sub_cam_amb_b_gain;
	u16 sub_cam_iso;
	u16 sub_cam_bv;
	s16 sub_cam_vcm_position;
	u8  sub_cam_vcm_status;
	u16 main_cam_isp_d_gain;
	u16 sub_cam_isp_d_gain;
	s16 hdr_long_exp_ev_x1000;
	s16 hdr_short_exp_ev_x1000;
	u16 ghost_prevent_low;
	u16 ghost_prevent_high;
	u8 depth_proc_mode;
};

/**
 *@struct isp_cmd_depth_auto_interleave_param(opcode:10BC)
 *@brief depth Interleave mode param
 */
#pragma pack(1)
struct isp_cmd_depth_auto_interleave_param {
	u8 depth_interleave_mode_on_off;/*1: on, 0: off*/
	u8 skip_frame_num_after_illuminator_pulse;
	u8 projector_power_level;/*0~255*/
	u8 illuminator_power_level;/*0~255*/
};

/**
 *@struct isp_cmd_interleave_mode_depth_type(opcode:10BD)
 *@brief interleave mode projector with depth type
 */
#pragma pack(1)
struct isp_cmd_interleave_mode_depth_type {
	u8 projector_interleave_mode_with_depth_type;
		/*1: passive, 0: active, default active*/
};

/**
 *@struct isp_cmd_depth_polish_level(opcode:10BE)
 *@brief set depth polish level
 */
#pragma pack(1)
struct isp_cmd_depth_polish_level {
	u8 depth_polish_level;/*0~100*/
};

/**
 *@struct isp_cmd_exposure_param(opcode:10BF)
 *@brief set exposure param
 */
#pragma pack(1)
struct isp_cmd_exposure_param {
	u32 udExpTime;/*unit:us*/
	u16 uwISO;
	u8 ucActiveDevice;
};

/**
 *@struct isp_cmd_depth_stream_size(opcode:10C0)
 *@brief set depth stream size
 */
#pragma pack(1)
struct isp_cmd_depth_stream_size {
	u8 depth_stream_size;/*0 : normal, 1: Binning x2*/
};

/*system cmd use structure*/

/* ISP operation mode*/
enum ispctrl_operation_mode {
	ISPCTRL_TEST_MODE,
	ISPCTRL_STILLLV_MODE,
	ISPCTRL_VIDEOLV_MODE,
	ISPCTRL_CONCURRENT_MODE,
	ISPCTRL_BYPASS_MODE,
	ISPCTRL_POWERDOWN_MODE
};

/**
 *@struct system_cmd_isp_mode(opcode:0010 and 0011)
 *@brief depth 3A information
 */
#pragma pack(1)
struct system_cmd_isp_mode {
	u8 isp_mode;  /*ispctrl_operation_mode*/
};

/**
 *@struct system_cmd_status_of_last_command(opcode:0015)
 *@brief last execution command status
 */
#pragma pack(1)
struct system_cmd_status_of_last_command {
	u16 opcode;
	u32 execution_status;
};

/**
 *@struct system_cmd_isp_register(opcode:0100 and 0101)
 *@brief use on set/get isp register
 */
#pragma pack(1)
struct system_cmd_isp_register {
	u32 address;
	u32 value;
};

/**
 *@struct system_cmd_debug_mode(opcode:0104)
 *@brief use on get irq status
 */
#pragma pack(1)
struct system_cmd_debug_mode {
	u8 on;
};

/**
 *@struct system_cmd_common_log_level(opcode:0109)
 *@brief use on set common log level
 */
#pragma pack(1)
struct system_cmd_common_log_level {
	u32 log_level;
};



/*bulk cmd use structure*/

/**
 *@struct memmory_dump_hdr_info
 *@brief use on isp memory read
 */
#pragma pack(1)
struct memmory_dump_hdr_info {
	u32 start_addr;
	u32 total_size;
	u32 block_size;
	u32 dump_mode;
};


/**
 *@struct common_log_hdr_info
 *@brief Bulk data for memory dump header
 */
#pragma pack(1)
struct common_log_hdr_info {
	u32 total_size;
	u32 block_size;
};

struct depthflowmgrpv_dgmgr_recta_quality_param {
	u32 recta_quality_rect_core_sel_param;
	u32 recta_quality_rect_ldc_param[43];
	u32 recta_quality_rect_hgh_param[9];
	u32 recta_quality_rect_bilinear_param;
};

struct depthflowmgrpv_dgmgr_rectb_quality_param {
	u32 rectb_quality_rect_core_sel_param;
	u32 rectb_quality_rect_ldc_param[43];
	u32 rectb_quality_rect_hgh_param[9];
	u32 rectb_quality_rect_bilinear_param;
};

struct depthflowmgrpv_invrectmgr_quality_param {
	u32 recta_quality_irect_core_sel_param;
	u32 recta_quality_irect_ldc_param[43];
	u32 recta_quality_irect_hgh_param[9];
	u32 recta_quality_irect_bilinear_param;
	u32 rectb_quality_irect_bilinear_param;
};

/**
 *@struct depth_rectab_invrect_param(opcode:0x210C)
 *@brief  depth rect A, B, INV rect parameter structure
 */
#pragma pack(1)
struct depth_rectab_invrect_param {
	struct depthflowmgrpv_dgmgr_recta_quality_param	depth_recta_quality;
	struct depthflowmgrpv_dgmgr_rectb_quality_param	depth_rectb_quality;
	struct depthflowmgrpv_invrectmgr_quality_param	depth_invrect_quality;
};

/**
 *@struct isp_cmd_depth_rectab_invrect_info(opcode:0x210C)
 *@brief  depth rect A, B, INV rect buffer address
 */
#pragma pack(1)
struct isp_cmd_depth_rectab_invrect_info {
	struct depth_rectab_invrect_param rect_param[3];
	u8 trans_mode;
	u32 block_size;
};

/**
 *@struct isp_cmd_depth_compensation_param(opcode:0x301C)
 *@brief  depth compensation param
 */
#pragma pack(1)
struct isp_cmd_depth_compensation_param {
	u8 en_updated;
	u16 short_distance_value;
	s8 compensation;
	u8 reserve[4];
};

/**
@struct isp_cmd_cycle_trigger_depth_process
@brief cycle trigger depth process
*/
#pragma pack(1)
struct isp_cmd_cycle_trigger_depth_process {
	u8 cycleLen;
		/* effective length in cycle[], Range: 1~16 frames one cycle*/
	u16 depth_triggerBitField;
		/* bit 0 : 1st frame, bit 1 : 2nd frame, ... */
	u16 depthoutput_triggerBitField;
		/* bit 0 : 1st frame, bit 1 : 2nd frame, ... */
		/* 0 : Not trigger, 1: Trigger */
	u8 Reserved[3];
};

/**
@struct isp_cmd_max_exposure_slope
@brief set max exposure slope
*/
#pragma pack(1)
struct isp_cmd_max_exposure_slope {
	u32 max_exposure_slope;
	u8 ucActiveDevice;
	u8 Reserved[3];
};

/******Public Function Prototype******/

#pragma pack(pop)
/******End of File******/

/**
 *@}
 */

#endif /* _ISPCAMERA_CMD_H_*/
