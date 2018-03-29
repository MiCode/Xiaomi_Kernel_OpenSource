/*
 * Copyright(c) 2012, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */





#ifndef _SP_TX_DRV_H
#define _SP_TX_DRV_H

#ifdef _SP_TX_DRV_C_
#define _SP_TX_DRV_EX_C_
#else
#define _SP_TX_DRV_EX_C_ extern
#endif

typedef unsigned char BYTE;
typedef unsigned char unchar;
typedef unsigned int uint;
//typedef bit bool;
//typedef unsigned long ulong;
//typedef unsigned int WORD;

#define Display_NoHDCP
#define Redo_HDCP
#define Standard_DP

#define MAX_BUF_CNT 6

#define SP_TX_AVI_SEL 0x01
#define SP_TX_SPD_SEL 0x02
#define SP_TX_MPEG_SEL 0x04

#define REDUCE_REPEAT_PRINT_INFO 0


#define AUTO_TEST_CTS 1
#define ENABLE_3D 1
#define ANX7730_VIDEO_STB 1
/*==for wlf stat==*/
#define UPDATE_ANX7730_SW 0
#define HARDWARE_POWER_CHANGE 0
/*==end ==*/
#define SP_TX_HDCP_FAIL_THRESHOLD         10

#define EDID_Dev_ADDR 0xa0
#define EDID_SegmentPointer_ADDR 0x60

#define SP_TX_PORT0_ADDR 0x70
#define SP_TX_PORT1_ADDR 0x74
#define SP_TX_PORT2_ADDR 0x72
#define MIPI_RX_PORT1_ADDR 0x7A


struct Bist_Video_Format {
	char number;
	char video_type[32];
	unsigned int pixel_frequency;
	unsigned int h_total_length;
	unsigned int h_active_length;
	unsigned int v_total_length;
	unsigned int v_active_length;
	unsigned int h_front_porch;
	unsigned int h_sync_width;
	unsigned int h_back_porch;
	unsigned int v_front_porch;
	unsigned int v_sync_width;
	unsigned int v_back_porch;
	unsigned char h_sync_polarity;
	unsigned char v_sync_polarity;
	unsigned char is_interlaced;
	unsigned char pix_repeat_times;
	unsigned char frame_rate;
	unsigned char bpp_mode;
	unsigned char video_mode;
};

struct LT_Result {
	BYTE LT_1_54;
	BYTE LT_1_27;
	BYTE LT_1_162;
	BYTE LT_2_54;
	BYTE LT_2_27;
	BYTE LT_2_162;

};


typedef enum {
	SP_TX_PWR_REG,
	SP_TX_PWR_HDCP,
	SP_TX_PWR_AUDIO,
	SP_TX_PWR_VIDEO,
	SP_TX_PWR_LINK,
	SP_TX_PWR_TOTAL
} SP_TX_POWER_BLOCK;



typedef enum {
	AUDIO_BIST,
	AUDIO_SPDIF,
	AUDIO_I2S,
	AUDIO_SLIMBUS
} AudioType;

typedef enum {
	AUDIO_FS_441K = 0x00,
	AUDIO_FS_48K   = 0x02,
	AUDIO_FS_32K   = 0x03,
	AUDIO_FS_882K = 0x08,
	AUDIO_FS_96K   = 0x0a,
	AUDIO_FS_1764K= 0x0c,
	AUDIO_FS_192K =  0x0e
} AudioFs;

typedef enum {
	AUDIO_W_LEN_16_20MAX = 0x02,
	AUDIO_W_LEN_18_20MAX = 0x04,
	AUDIO_W_LEN_17_20MAX = 0x0c,
	AUDIO_W_LEN_19_20MAX = 0x08,
	AUDIO_W_LEN_20_20MAX = 0x0a,
	AUDIO_W_LEN_20_24MAX = 0x03,
	AUDIO_W_LEN_22_24MAX = 0x05,
	AUDIO_W_LEN_21_24MAX = 0x0d,
	AUDIO_W_LEN_23_24MAX = 0x09,
	AUDIO_W_LEN_24_24MAX = 0x0b
} AudioWdLen;

enum
{
	VIDEO_3D_NONE		= 0x00,
	VIDEO_3D_FRAME_PACKING		= 0x01,
	VIDEO_3D_TOP_AND_BOTTOM		= 0x02,
	VIDEO_3D_SIDE_BY_SIDE		= 0x03,
	VIDEO_3D_INITIAL	= 0xFF
};

typedef enum {
	I2S_CH_2 =0x01,
	I2S_CH_4 =0x03,
	I2S_CH_6 =0x05,
	I2S_CH_8 =0x07
} I2SChNum;

typedef enum {
	I2S_LAYOUT_0,
	I2S_LAYOUT_1
} I2SLayOut;


typedef struct {
	I2SChNum Channel_Num;
	I2SLayOut  AUDIO_LAYOUT;
	unsigned char SHIFT_CTRL;
	unsigned char DIR_CTRL;
	unsigned char WS_POL;
	unsigned char JUST_CTRL;
	unsigned char EXT_VUCP;
	unsigned char I2S_SW;
	unsigned char Channel_status1;
	unsigned char Channel_status2;
	unsigned char Channel_status3;
	unsigned char Channel_status4;
	unsigned char Channel_status5;

} I2S_FORMAT;




struct AudioFormat {
	AudioType bAudioType;
	unsigned char  bAudio_Fs;
	unsigned char bAudio_word_len;
	I2S_FORMAT bI2S_FORMAT;
};

typedef enum {
	AVI_PACKETS,
	SPD_PACKETS,
#if(ENABLE_3D)
	VSI_PACKETS,
#endif
	MPEG_PACKETS
} PACKETS_TYPE;

struct Packet_AVI {
	unsigned char AVI_data[13];
} ;


struct Packet_SPD {
	unsigned char SPD_data[25];
};

#define MPEG_PACKET_SIZE 10
struct Packet_MPEG {
	unsigned char MPEG_data[MPEG_PACKET_SIZE];
} ;


struct AudiInfoframe {
	unsigned char type;
	unsigned char version;
	unsigned char length;
	unsigned char pb_byte[10]; //modify to 10 bytes from 28.2008/10/23
};


typedef struct subEmbedded_Sync_t {
	unsigned char Embedded_Sync;
	unsigned char Extend_Embedded_Sync_flag;
} subEmbedded_Sync;

typedef struct subYC_MUX_t {
	unsigned char YC_MUX;
	unsigned char YC_BIT_SEL;
} subYC_MUX;

struct LVTTL_HW_Interface {
	unsigned char DE_reDenerate;
	unsigned char DDR_Mode;
	//unsigned char BPC;
	unsigned char Clock_EDGE;// 1:negedge 0:posedge
	subEmbedded_Sync sEmbedded_Sync;// 1:valueable
	subYC_MUX sYC_MUX;
};

typedef enum {
	LVTTL_RGB,
	MIPI_DSI
} VideoInterface;

typedef enum {
	COLOR_6_BIT,
	COLOR_8_BIT,
	COLOR_10_BIT,
	COLOR_12_BIT
} ColorDepth;

typedef enum {
	COLOR_RGB,
	COLOR_YCBCR_422,
	COLOR_YCBCR_444
} ColorSpace;

struct VideoFormat {
	VideoInterface Interface;// 0:LVTTL ; 1:mipi
	ColorDepth bColordepth;
	ColorSpace bColorSpace;
	struct LVTTL_HW_Interface bLVTTL_HW_Interface;
};

typedef enum {
	COMMON_INT_1 = 0,
	COMMON_INT_2 = 1,
	COMMON_INT_3 = 2,
	COMMON_INT_4 = 3,
	SP_INT_STATUS = 6
} INTStatus;


typedef enum {
	PLL_BLOCK,
	AUX_BLOCK,
	CH0_BLOCK,
	CH1_BLOCK,
	ANALOG_TOTAL,
	POWER_ALL
} ANALOG_PWD_BLOCK;

typedef enum {
	PRBS_7,
	D10_2,
	TRAINING_PTN1,
	TRAINING_PTN2,
	NO_PATTERN
} PATTERN_SET;

typedef enum {
	BW_54G = 0x14,
	//BW_45G = 0x10,
	BW_27G = 0x0A,
	BW_162G = 0x06,
	BW_NULL = 0x00
} SP_LINK_BW;

typedef enum {
	LINKTRAINING_START,
	CLOCK_RECOVERY_PROCESS,
	EQ_TRAINING_PROCESS,
	LINKTRAINING_FINISHED
} SP_SW_LINK_State;

struct MIPI_Video_Format {
	char MIPI_number;
	char MIPI_video_type[32];
	unsigned int MIPI_pixel_frequency;
	unsigned int MIPI_HTOTAL;
	unsigned int MIPI_HActive;
	unsigned int MIPI_VTOTAL;
	unsigned int MIPI_VActive;

	unsigned int MIPI_H_Front_Porch;
	unsigned int MIPI_H_Sync_Width;
	unsigned int MIPI_H_Back_Porch;


	unsigned int MIPI_V_Front_Porch;
	unsigned int MIPI_V_Sync_Width;
	unsigned int MIPI_V_Back_Porch;
};

enum SP_TX_SEND_MSG {
	MSG_OCM_EN,
	MSG_INPUT_HDMI,
	MSG_INPUT_DVI,
	MSG_CLEAR_IRQ,
};


typedef enum {
	LINK_TRAINING_INIT,
	LINK_TRAINING_PRE_CONFIG,
	LINK_TRAINING_START,
	LINK_TRAINING_WAITTING_FINISH,
	LINK_TRAINING_ERROR,
	LINK_TRAINING_FINISH,
	LINK_TRAINING_END,
	LINK_TRAINING_STATE_NUM
} SP_TX_Link_Training_State;
typedef enum {
	VIDEO_CONFIG_INIT,
	VIDEO_CONFIG_START,
	VIDEO_CONFIG_FINISH,
	VIDEO_CONFIG_SEND_VIDEO_PACKET,
	VIDEO_CONFIG_END,
	VIDEO_CONFIG_STATE_NUM
} VIDEO_CONFIG_STATE;
typedef enum {
	HDCP_PROCESS_INIT,
	HDCP_CAPABLE_CHECK,
	HDCP_WAITTING_VID_STB,
	HDCP_HW_ENABLE,
	HDCP_WAITTING_FINISH,
	HDCP_FINISH,
	HDCP_FAILE,
	HDCP_NOT_SUPPORT,
	HDCP_PROCESS_STATE_NUM
} HDCP_Process_State;

#define sp_write_reg_mask(address, offset, mask, reg_value) \
	do { \
		unsigned char temp_value; \
		sp_read_reg(address, offset, &temp_value); \
		sp_write_reg(address, offset, (temp_value&mask)|reg_value); \
		}while(0);

#define SP_TX_POWER_ON 1
#define SP_TX_POWER_DOWN 0

#if(ENABLE_3D)


#define PACKET_TYPE_FOR_3D 0x81
#define PACKET_VERSION_FOR_3D 0x01
void SP_TX_Power_Enable(SP_TX_POWER_BLOCK sp_tx_pd_block, BYTE power);

void sp_tx_send_3d_vsi_packet_to_7730(BYTE video_format);
#endif

#if(ANX7730_VIDEO_STB)
#define SP_TX_DS_VID_STB_TH 20

#endif

void SP_TX_Initialization(struct VideoFormat* pInputFormat);
void SP_TX_BIST_Format_Config(unsigned int sp_tx_bist_select_number);
void SP_TX_BIST_Format_Resolution(unsigned int sp_tx_bist_select_number);
void SP_TX_Config_BIST_Video (BYTE cBistIndex,struct VideoFormat* pInputFormat);
void SP_TX_Show_Infomation(void);
void SP_TX_Power_Down(SP_TX_POWER_BLOCK sp_tx_pd_block);
void SP_TX_Power_On(SP_TX_POWER_BLOCK sp_tx_pd_block);
void SP_TX_AVI_Setup(void);
//void SP_TX_Enable_Video_Input(void);
//BYTE SP_TX_AUX_DPCDRead_Byte(BYTE addrh, BYTE addrm, BYTE addrl);
BYTE SP_TX_AUX_DPCDWrite_Byte(BYTE addrh, BYTE addrm, BYTE addrl, BYTE data1);
void SP_TX_HW_HDCP_Enable(void);
void SP_TX_HW_HDCP_Disable(void);
void SP_TX_Clean_HDCP(void);
void SP_TX_PBBS7_Test(void);
void SP_TX_Insert_Err(void);
void SP_TX_EnhaceMode_Set(void);
void SP_TX_CONFIG_SSC(SP_LINK_BW linkbw);
void SP_TX_Config_Audio(struct AudioFormat *bAudio);
void SP_TX_Config_Audio_I2S(struct AudioFormat *bAudioFormat) ;
void SP_TX_Config_Audio_SPDIF(void) ;
void SP_TX_Config_Audio_BIST(struct AudioFormat *bAudioFormat);
void SP_TX_Config_Audio_Slimbus(struct AudioFormat *bAudioFormat);
BYTE SP_TX_Chip_Located(void);
//void SP_TX_Hardware_Reset(void);
void SP_TX_Hardware_PowerOn(void);
void SP_TX_Hardware_PowerDown(void);
void vbus_power_ctrl(BYTE ON);
void SP_TX_RST_AUX(void);
BYTE SP_TX_AUX_DPCDRead_Bytes(BYTE addrh, BYTE addrm, BYTE addrl,BYTE cCount,BYTE * pBuf);
BYTE SP_TX_Wait_AUX_Finished(void);
//void SP_TX_Wait_AUX_Finished(void);
BYTE SP_TX_AUX_DPCDWrite_Bytes(BYTE addrh, BYTE addrm, BYTE addrl,BYTE cCount,BYTE *  pBuf);
void SP_TX_SW_Reset(void);
void SP_TX_SPREAD_Enable(BYTE bEnable);
void SP_TX_Enable_Audio_Output(BYTE bEnable);
void SP_TX_Disable_Audio_Input(void);
void SP_TX_AudioInfoFrameSetup(struct AudioFormat *bAudioFormat);
void SP_TX_InfoFrameUpdate(struct AudiInfoframe* pAudioInfoFrame);
void SP_TX_Get_Int_status(INTStatus IntIndex, BYTE *cStatus);
//void SP_TX_Get_HPD_status( BYTE *cStatus);
BYTE SP_TX_Get_PLL_Lock_Status(void);
void SP_TX_Get_HDCP_status( BYTE *cStatus);
void SP_TX_HDCP_ReAuth(void);
//void SP_TX_Get_Rx_LaneCount(BYTE bMax,BYTE *cLaneCnt);
//void SP_TX_Set_Rx_laneCount(BYTE cLaneCnt);
void SP_TX_Get_Rx_BW(BYTE bMax,BYTE *cBw);
void SP_TX_Set_Rx_BW(BYTE cBw);
void SP_TX_Get_Link_BW(BYTE *bwtype);
//void SP_TX_Get_Lane_Count(BYTE *count);
void SP_TX_Get_Link_Status(BYTE *cStatus);
void SP_TX_Get_lane_Setting(BYTE cLane, BYTE *cSetting);
void SP_TX_EDID_Read_Initial(void);
BYTE SP_TX_AUX_EDIDRead_Byte(BYTE offset);
void SP_TX_Parse_Segments_EDID(BYTE segment, BYTE offset);
BYTE SP_TX_Get_EDID_Block(void);
void SP_TX_AddrOnly_Set(BYTE bSet);
void SP_TX_Scramble_Enable(BYTE bEnabled);
void SP_TX_API_M_GEN_CLK_Select(BYTE bSpreading);
void SP_TX_Config_Packets(PACKETS_TYPE bType);
void SP_TX_Load_Packet (PACKETS_TYPE type);
BYTE SP_TX_Config_Video_LVTTL (struct VideoFormat* pInputFormat);
BYTE SP_TX_Config_Video_MIPI (void);
BYTE SP_TX_BW_LC_Sel(struct VideoFormat* pInputFormat);
void SP_TX_Embedded_Sync(struct VideoFormat* pInputFormat, unsigned int sp_tx_bist_select_number);
void SP_TX_DE_reGenerate (unsigned int sp_tx_bist_select_number);
void SP_TX_PCLK_Calc(SP_LINK_BW hbr_rbr);
void SP_TX_SW_Link_Training (void);
BYTE SP_TX_HW_Link_Training (void);
BYTE SP_TX_LT_Pre_Config(void);
void SP_TX_LVTTL_Bit_Mapping(struct VideoFormat* pInputFormat);
void SP_TX_Video_Mute(BYTE enable);
void SP_TX_Lanes_PWR_Ctrl(ANALOG_PWD_BLOCK eBlock, BYTE powerdown);
void SP_TX_AUX_WR (BYTE offset);
void SP_TX_AUX_RD (BYTE len_cmd);
void SP_TX_MIPI_CONFIG_Flag_Set(BYTE bConfigured);
void SP_TX_Config_MIPI_Video_Format(void);
void MIPI_Format_Index_Set(BYTE bFormatIndex);
BYTE MIPI_Format_Index_Get(void);
BYTE MIPI_CheckSum_Status_OK(void);
void system_power_ctrl(BYTE ON);
void slimport_config_video_output(void);


#define	EmbededSync     1
#define	SeparateSync     0
#define	NoDE     1
#define	SeparateDE     0
#define	YCMUX    1
#define	UnYCMUX     0
#define	DDR    1
#define	SDR     0
#define	Negedge    1
#define	Posedge       0


typedef enum {
	SP_TX_INITIAL = 1,
	SP_TX_WAIT_SLIMPORT_PLUGIN,
	SP_TX_PARSE_EDID,
	SP_TX_CONFIG_VIDEO_INPUT,
	SP_TX_LINK_TRAINING,
	SP_TX_CONFIG_VIDEO_OUTPUT,
	SP_TX_HDCP_AUTHENTICATION,
	SP_TX_CONFIG_AUDIO,
	SP_TX_PLAY_BACK
} SP_TX_System_State;

void SP_CTRL_Dump_Reg(void);
void SP_CTRL_Main_Procss(void);
BYTE SP_CTRL_Chip_Detect(void);
void SP_CTRL_Chip_Initial(void);
void SP_CTRL_Set_System_State(SP_TX_System_State ss);
void SP_CTRL_Clean_HDCP(void);
void SP_CTRL_nbc12429_setting(int frequency);
//for mipi interrupt process
SP_TX_System_State get_system_state(void);
int SP_Get_Connect_Status(void);

void SP_CTRL_AUDIO_FORMAT_Set(AudioType cAudio_Type,AudioFs cAudio_Fs,AudioWdLen cAudio_Word_Len);
void SP_CTRL_I2S_CONFIG_Set(I2SChNum cCh_Num, I2SLayOut cI2S_Layout);



_SP_TX_DRV_EX_C_ BYTE ByteBuf[MAX_BUF_CNT];

_SP_TX_DRV_EX_C_ SP_TX_System_State sp_tx_system_state;
//for EDID
_SP_TX_DRV_EX_C_ BYTE edid_pclk_out_of_range;//the flag when clock out of 256Mhz
_SP_TX_DRV_EX_C_ BYTE sp_tx_edid_err_code;//EDID read  error type flag
_SP_TX_DRV_EX_C_ BYTE bEDIDBreak; //EDID access break


//for HDCP

_SP_TX_DRV_EX_C_ BYTE sp_tx_pd_mode; //ANX7805 power state flag
//for bist video index
_SP_TX_DRV_EX_C_ unsigned int bBIST_FORMAT_INDEX;
_SP_TX_DRV_EX_C_ BYTE bBIST_FORMAT_INDEX_backup;
_SP_TX_DRV_EX_C_ BYTE Force_Video_Resolution;
//external interrupt flag
_SP_TX_DRV_EX_C_ BYTE ext_int_index;

_SP_TX_DRV_EX_C_ struct VideoFormat SP_TX_Video_Input;
_SP_TX_DRV_EX_C_ struct AudioFormat SP_TX_Audio_Input;

enum RX_CBL_TYPE {
	RX_HDMI = 0x01,
	RX_DP = 0x02,
	RX_VGA = 0x03,
	RX_NULL = 0x00
};
_SP_TX_DRV_EX_C_ enum RX_CBL_TYPE sp_tx_rx_type;

_SP_TX_DRV_EX_C_ BYTE CEC_abort_message_received;
_SP_TX_DRV_EX_C_ BYTE CEC_get_physical_adress_message_received;
_SP_TX_DRV_EX_C_ BYTE  CEC_logic_addr;
_SP_TX_DRV_EX_C_ int      CEC_loop_number;
_SP_TX_DRV_EX_C_ BYTE CEC_resent_flag;



_SP_TX_DRV_EX_C_ SP_TX_Link_Training_State sp_tx_link_training_state;
_SP_TX_DRV_EX_C_ HDCP_Process_State hdcp_process_state;

//for EDID
_SP_TX_DRV_EX_C_ BYTE checksum;
_SP_TX_DRV_EX_C_ BYTE SP_TX_EDID_PREFERRED[18];//edid DTD array
_SP_TX_DRV_EX_C_ BYTE sp_tx_ds_edid_hdmi; //Downstream is HDMI flag
_SP_TX_DRV_EX_C_ BYTE sp_tx_ds_edid_3d_present;//downstream monitor support 3D
_SP_TX_DRV_EX_C_ BYTE DTDbeginAddr;
_SP_TX_DRV_EX_C_ BYTE EDID_Print_Enable;
_SP_TX_DRV_EX_C_ BYTE EDIDExtBlock[128];

_SP_TX_DRV_EX_C_ unsigned long pclk; //input video pixel clock
_SP_TX_DRV_EX_C_ long int M_val,N_val;
_SP_TX_DRV_EX_C_ SP_LINK_BW sp_tx_bw;  //linktraining banwidth
_SP_TX_DRV_EX_C_ BYTE sp_tx_lane_count; //link training lane count
#if(AUTO_TEST_CTS)
_SP_TX_DRV_EX_C_ BYTE sp_tx_test_lt;
_SP_TX_DRV_EX_C_ BYTE sp_tx_test_bw;
_SP_TX_DRV_EX_C_ BYTE sp_tx_test_edid;
#endif

_SP_TX_DRV_EX_C_ struct AudiInfoframe SP_TX_AudioInfoFrmae;
_SP_TX_DRV_EX_C_ struct Packet_AVI SP_TX_Packet_AVI;
_SP_TX_DRV_EX_C_ struct Packet_MPEG SP_TX_Packet_MPEG;
_SP_TX_DRV_EX_C_ struct Packet_SPD SP_TX_Packet_SPD;


_SP_TX_DRV_EX_C_ BYTE bMIPI_Configured;
_SP_TX_DRV_EX_C_ BYTE bMIPIFormatIndex;
//for MIPI Video format config

_SP_TX_DRV_EX_C_ BYTE bEDID_extblock[128];
_SP_TX_DRV_EX_C_ BYTE bEDID_firstblock[128];
_SP_TX_DRV_EX_C_ BYTE bEDID_fourblock[256];

_SP_TX_DRV_EX_C_ bool audio_format_change;
_SP_TX_DRV_EX_C_ bool video_format_change;
_SP_TX_DRV_EX_C_ int three_3d_format;


#if(REDUCE_REPEAT_PRINT_INFO)
#define LOOP_PRINT_MSG_MAX 16
_SP_TX_DRV_EX_C_ BYTE maybe_repeat_print_info_flag;
_SP_TX_DRV_EX_C_ BYTE repeat_printf_info_count[LOOP_PRINT_MSG_MAX];
typedef enum {
	REPEAT_PRINT_INFO_INIT,
	REPEAT_PRINT_INFO_START,
	REPEAT_PRINT_INFO_CONFIRM,
	REPEAT_PRINT_INFO_CLEAR,
	REPEAT_PRINT_INFO_NUM
} Repeat_print_info_State;


#endif


#endif
