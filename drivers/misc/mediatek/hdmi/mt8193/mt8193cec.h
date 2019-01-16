#ifndef __mt8193cec_h__
#define __mt8193cec_h__

#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT

typedef enum {
	CEC_NORMAL_MODE = 0,
	CEC_CTS_MODE,
	CEC_SLT_MODE
} HDMI_CEC_RX_MODE;

typedef enum {
	FAIL_NONE = 0x00,
	FAIL_DIR = 0x01,
	FAIL_MODE = 0x02,
	FAIL_ID = 0x04,
	FAIL_SOURCE = 0x08,
	FAIL_HEADER = 0x10,
	FAIL_CMD = 0x20,
	FAIL_DATA = 0x40,
	FAIL_MAX = 0x80
} CEC_TX_FAIL;

typedef enum {
	STATE_WAIT_TX_DATA_TAKEN = 0x0001,
	STATE_TX_NOACK = 0x0002,	/* CYJ.NOTE */
	STATE_TXING_FRAME = 0x0004,
	STATE_TX_FRAME_SUCCESS = 0x0008,
	STATE_HW_RETX = 0x0010,

	STATE_WAIT_RX_FRAME_COMPLETE = 0x0100,
	STATE_RX_COMPLETE_NEW_FRAME = 0x0200,
	STATE_HW_RX_OVERFLOW = 0x0400,
	STATE_RX_GET_NEW_HEADER = 0x0800
} CEC_SW_STATE;

typedef enum {
	ERR_TX_BUFFER_LOW = 0x0001,
	ERR_TX_UNDERRUN = 0x0002,
	ERR_TX_MISALARM = 0x0004,
	ERR_RX_LOST_EOM = 0x0100,
	ERR_RXQ_OVERFLOW = 0x0200,
	ERR_RX_LOST_HEADER = 0x0400
} CEC_ERR_STATUS;

typedef enum {
	OPCODE_FEATURE_ABORT = 0x00,	/* 4 */
	OPCODE_IMAGE_VIEW_ON = 0x04,	/* 2 */
	OPCODE_TUNER_STEP_INCREMENT = 0x05,	/* 2 */
	OPCODE_TUNER_STEP_DECREMENT = 0x06,	/* 2 */
	OPCODE_TUNER_DEVICE_STATUS = 0x07,	/* 7 or 10 */
	OPCODE_GIVE_TUNER_DEVICE_STATUS = 0x08,	/* 3 */
	OPCODE_RECORD_ON = 0x09,	/* 3~10 */
	OPCODE_RECORD_STATUS = 0x0A,	/* 3 */
	OPCODE_RECORD_OFF = 0x0B,	/* 2 */
	OPCODE_TEXT_VIEW_ON = 0x0D,	/* 2 */
	OPCODE_RECORD_TV_SCREEN = 0x0F,	/* 2 */
	OPCODE_GIVE_DECK_STATUS = 0x1A,	/* 3 */
	OPCODE_DECK_STATUS = 0x1B,	/* 3 */
	OPCODE_SET_MENU_LANGUAGE = 0x32,	/* 5 */
	OPCODE_CLEAR_ANALOGUE_TIMER = 0x33,	/* 13 */
	OPCODE_SET_ANALOGUE_TIMER = 0x34,	/* 13 */
	OPCODE_TIMER_STATUS = 0x35,	/* 3 or 5 */
	OPCODE_STANDBY = 0x36,	/* 2 */
	OPCODE_PLAY = 0x41,	/* 3 */
	OPCODE_DECK_CONTROL = 0x42,	/* 3 */
	OPCODE_TIMER_CLEARED_STATUS = 0x43,	/* 3 */
	OPCODE_USER_CONTROL_PRESSED = 0x44,	/* 3 */
	OPCODE_USER_CONTROL_RELEASED = 0x45,	/* 2 */
	OPCODE_GIVE_OSD_NAME = 0x46,	/* 2 */
	OPCODE_SET_OSD_NAME = 0x47,	/* 3~16 */
	OPCODE_SET_OSD_STRING = 0x64,	/* 4~16 */
	OPCODE_SET_TIMER_PROGRAM_TITLE = 0x67,	/* 3~16 */
	OPCODE_SYSTEM_AUDIO_MODE_REQUEST = 0x70,	/* 4 */
	OPCODE_GIVE_AUDIO_STATUS = 0x71,	/* 2 */
	OPCODE_SET_SYSTEM_AUDIO_MODE = 0x72,	/* 3 */
	OPCODE_REPORT_AUDIO_STATUS = 0x7A,	/* 3 */
	OPCODE_GIVE_SYSTEM_AUDIO_MODE_STATUS = 0x7D,	/* 2 */
	OPCODE_SYSTEM_AUDIO_MODE_STATUS = 0x7E,	/* 3 */
	OPCODE_ROUTING_CHANGE = 0x80,	/* 6 */
	OPCODE_ROUTING_INFORMATION = 0x81,	/* 4 */
	OPCODE_ACTIVE_SOURCE = 0x82,	/* 4 */
	OPCODE_GIVE_PHYSICAL_ADDRESS = 0x83,	/* 2 */
	OPCODE_REPORT_PHYSICAL_ADDRESS = 0x84,	/* 5 */
	OPCODE_REQUEST_ACTIVE_SOURCE = 0x85,	/* 2 */
	OPCODE_SET_STREAM_PATH = 0x86,	/* 4 */
	OPCODE_DEVICE_VENDOR_ID = 0x87,	/* 5 */
	OPCODE_VENDOR_COMMAND = 0x89,	/* <= 16 */
	OPCODE_VENDOR_REMOTE_BUTTON_DOWN = 0x8A,	/* <= 16 */
	OPCODE_VENDOR_REMOTE_BUTTON_UP = 0x8B,	/* 2 */
	OPCODE_GIVE_DEVICE_VENDOR_ID = 0x8C,	/* 2 */
	OPCODE_MENU_REQUEST = 0x8D,	/* 3 */
	OPCODE_MENU_STATUS = 0x8E,	/* 3 */
	OPCODE_GIVE_DEVICE_POWER_STATUS = 0x8F,	/* 2 */
	OPCODE_REPORT_POWER_STATUS = 0x90,	/* 3 */
	OPCODE_GET_MENU_LANGUAGE = 0x91,	/* 2 */
	OPCODE_SELECT_ANALOGUE_SERVICE = 0x92,	/* 6 */
	OPCODE_SELECT_DIGITAL_SERVICE = 0x93,	/* 9 */
	OPCODE_SET_DIGITAL_TIMER = 0x97,	/* 16 */
	OPCODE_CLEAR_DIGITAL_TIMER = 0x99,	/* 16 */
	OPCODE_SET_AUDIO_RATE = 0x9A,	/* 3 */
	OPCODE_INACTIVE_SOURCE = 0x9D,	/* 4 */
	OPCODE_CEC_VERSION = 0x9E,	/* 3 */
	OPCODE_GET_CEC_VERSION = 0x9F,	/* 2 */
	OPCODE_VENDOR_COMMAND_WITH_ID = 0xA0,	/* <= 16 */
	OPCODE_CLEAR_EXTERNAL_TIMER = 0xA1,	/* 10 ~ 11 */
	OPCODE_SET_EXTERNAL_TIMER = 0xA2,	/* 10 ~ 11 */
	OPCODE_ABORT = 0xFF,	/* 2 */
	OPCODE_NONE = 0xFFF
} CEC_OPCODE;

#define RX_Q_SIZE 32
#define TX_Q_SIZE 32
#define RETX_MAX_CNT 2
#define CEC_MAX_MESG_SIZE 16

#define CEC_MAX_DEV_LA_NUM 3
#define CEC_MAX_OPERAND_SIZE       14
#define LOCAL_CEC_MAX_OPERAND_SIZE 14

/* --------FLD help macros, mask32 to mask8,mask16,maskalign ----------*/
	/* mask32 -> mask8 */
#define MSKB0(msk)	(u8)((msk)&0xff)
#define MSKB1(msk)	(u8)(((msk)>>8)&0xff)
#define MSKB2(msk)	(u8)(((msk)>>16)&0xff)
#define MSKB3(msk)	(u8)(((msk)>>24)&0xff)
/* mask32 -> mask16 */
#define MSKW0(msk)	(u16)((msk)&0xffff)
#define MSKW1(msk)	(u16)(((msk)>>8)&0xffff)
#define MSKW2(msk)	(u16)(((msk)>>16)&0xffff)
						/* mask32 -> maskalign */
#define MSKAlignB(msk)	(((msk)&0xff)?(msk):(\
			((msk)&0xff00)?((msk)>>8):(\
			((msk)&0xff0000)?((msk)>>16):((msk)>>24)\
		)\
	))
/* --------FLD help macros, mask32 to mask8,mask16,maskalign ----------*/
#define Fld2Msk32(fld)	/*lint -save -e504 */ ((((u32)1<<Fld_wid(fld))-1)<<Fld_shft(fld))	/*lint -restore */
#define Fld2MskB0(fld)	MSKB0(Fld2Msk32(fld))
#define Fld2MskB1(fld)	MSKB1(Fld2Msk32(fld))
#define Fld2MskB2(fld)	MSKB2(Fld2Msk32(fld))
#define Fld2MskB3(fld)	MSKB3(Fld2Msk32(fld))
#define Fld2MskBX(fld, byte)	((u8)((Fld2Msk32(fld)>>((byte&3)*8))&0xff))

#define Fld2MskW0(fld)	MSKW0(Fld2Msk32(fld))
#define Fld2MskW1(fld)	MSKW1(Fld2Msk32(fld))
#define Fld2MskW2(fld)	MSKW2(Fld2Msk32(fld))
#define Fld2MskWX(fld, byte)	((u16)((Fld2Msk32(fld)>>((byte&3)*8))&0xffff))


#define Fld2MskAlignB(fld)  MSKAlignB(Fld2Msk32(fld))
#define FldshftAlign(fld)	((Fld_shft(fld) < 8)?Fld_shft(fld):(\
			(Fld_shft(fld) < 16)?(Fld_shft(fld)-8):(\
			(Fld_shft(fld) < 24)?(Fld_shft(fld)-16):(Fld_shft(fld)-24)\
		)\
	))
#define ValAlign2Fld(val, fld)	((val)<<FldshftAlign(fld))


/* access method*/
#define	AC_FULLB0		1
#define	AC_FULLB1		2
#define	AC_FULLB2		3
#define	AC_FULLB3		4
#define	AC_FULLW10		5
#define	AC_FULLW21		6
#define	AC_FULLW32		7
#define	AC_FULLDW		8
#define	AC_MSKB0		11
#define	AC_MSKB1		12
#define	AC_MSKB2		13
#define	AC_MSKB3		14
#define	AC_MSKW10		15
#define	AC_MSKW21		16
#define	AC_MSKW32		17
#define	AC_MSKDW		18


#define Fld(wid, shft, ac)	(((unsigned long)(wid)<<16)|((shft)<<8)|(ac))
#define Fld_wid(fld)	    (unsigned char)(((fld)>>16)&0xff)
#define Fld_shft(fld)	    (unsigned char)(((fld)>>8)&0xff)
#define Fld_ac(fld)	        (unsigned char)((fld)&0xff)


#define TR_CONFIG 0x00
#define BYPASS Fld(1, 28, AC_MSKB3)	/* 28 */
#define TR_DEVICE_ADDR3 Fld(4, 24, AC_MSKB3)	/* 27:24 */
#define TR_DEVICE_ADDR2 Fld(4, 20, AC_MSKB2)	/* 23:20 */
#define DEVICE_ADDR Fld(4, 16, AC_MSKB2)	/* 19:16 */
#define TX_G_EN Fld(1, 13, AC_MSKB1)	/* 13 */
#define TX_EN Fld(1, 12, AC_MSKB1)	/* 12 */
#define RX_G_EN Fld(1, 9, AC_MSKB1)	/* 9 */
#define RX_EN Fld(1, 8, AC_MSKB1)	/* 8 */
#define CEC_ID_EN Fld(1, 1, AC_MSKB0)	/* 1 */
#define CEC_EN Fld(1, 0, AC_MSKB0)	/* 0 */

#define CEC_CKGEN 0x04


#define PDN Fld(1, 16, AC_MSKB2)	/* 16 */
#define DIV_SEL Fld(16, 0, AC_FULLW10)	/* 15:0 */

#define RX_T_START_R 0x08

#define RX_TIMER_START_R_MAX Fld(11, 16, AC_MSKW32)	/* 26:16 */
#define RX_TIMER_START_R_MIN Fld(11, 0, AC_MSKW10)	/* 10:0 */
#define RX_T_START_F 0x0c

#define RX_TIMER_START_F_MAX Fld(11, 16, AC_MSKW32)	/* 26:16 */
#define RX_TIMER_START_F_MIN Fld(11, 0, AC_MSKW10)	/* 10:0 */
#define RX_T_DATA 0x10

#define RX_TIMER_DATA_SAMPLE Fld(11, 16, AC_MSKW32)	/* 26:16 */
#define RX_TIMER_DATA_F_MIN Fld(11, 0, AC_MSKW10)	/* 10:0 */
#define RX_T_ACK 0x14

#define RX_TIMER_ACK_R Fld(11, 0, AC_MSKW10)	/* 10:0 */
#define RX_T_ERROR 0x18

#define RX_TIMER_ERROR_D Fld(11, 16, AC_MSKW32)	/* 26:16 */
#define RX_TIMER_ERROR_S Fld(11, 0, AC_MSKW10)	/* 10:0 */
#define TX_T_START 0x1c

#define TX_TIMER_START_F Fld(11, 16, AC_MSKW32)	/* 26:16 */
#define TX_TIMER_START_R Fld(11, 0, AC_MSKW10)	/* 10:0 */
#define TX_T_DATA_R 0x20

#define TX_TIMER_BIT1_R Fld(11, 16, AC_MSKW32)	/* 26:16 */
#define TX_TIMER_BIT0_R Fld(11, 0, AC_MSKW10)	/* 10:0 */
#define TX_T_DATA_F 0x24

#define TX_TIMER_DATA_N Fld(11, 16, AC_MSKW32)	/* 26:16 */
#define TX_TIMER_DATA_F Fld(11, 0, AC_MSKW10)	/* 10:0 */
#define TX_ARB 0x28

#define MAX_RETRANSMIT Fld(4, 12, AC_MSKB1)	/* 15:12 */
#define BCNT_RETRANSMIT_3_0 Fld(5, 7, AC_MSKW10)	/* 11:7 */
#define BCNT_NEW_MSG Fld(4, 4, AC_MSKB0)	/* 7:4 */
#define BCNT_NEW_INIT Fld(4, 0, AC_MSKB0)	/* 3:0 */
#define RX_HEADER 0x40

#define RXED_M3_DATA_MASK Fld(4, 24, AC_MSKB3)	/* 27:24 */
#define RXED_SRC Fld(4, 20, AC_MSKB2)	/* 23:20 */
#define RXED_DST Fld(4, 16, AC_MSKB2)	/* 19:16 */
#define RXED_H_EOM Fld(1, 15, AC_MSKB1)	/* 15 */
#define RXED_D_EOM Fld(1, 13, AC_MSKB1)	/* 13 */
#define RXED_M3_ID Fld(3, 8, AC_MSKB1)	/* 10:8 */
#define RXED_M1_DIR Fld(1, 7, AC_MSKB0)	/* 7 */
#define RXED_M1_PAS Fld(1, 6, AC_MSKB0)	/* 6 */
#define RXED_M1_NAS Fld(1, 5, AC_MSKB0)	/* 5 */
#define RXED_M1_DES Fld(1, 4, AC_MSKB0)	/* 4 */
#define RXED_MODE Fld(2, 0, AC_MSKB0)	/* 1:0 */
#define RX_DATA 0x44

#define RXED_DATA Fld(32, 0, AC_FULLDW)	/* 31:0 */
#define RX_HD_NEXT 0x48

#define RXING_M3_DATA_MASK Fld(4, 24, AC_MSKB3)	/* 27:24 */
#define RXING_SRC Fld(4, 20, AC_MSKB2)	/* 23:20 */
#define RXING_DST Fld(4, 16, AC_MSKB2)	/* 19:16 */
#define RXING_H_EOM Fld(1, 15, AC_MSKB1)	/* 15 */
#define RXING_H_ACK Fld(1, 14, AC_MSKB1)	/* 14 */
#define RXING_D_EOM Fld(1, 13, AC_MSKB1)	/* 13 */
#define RXING_D_ACK Fld(1, 12, AC_MSKB1)	/* 12 */
#define RXING_M3_ID Fld(3, 8, AC_MSKB1)	/* 10:8 */
#define RXING_M1_DIR Fld(1, 7, AC_MSKB0)	/* 7 */
#define RXING_M1_PAS Fld(1, 6, AC_MSKB0)	/* 6 */
#define RXING_M1_NAS Fld(1, 5, AC_MSKB0)	/* 5 */
#define RXING_M1_DES Fld(1, 4, AC_MSKB0)	/* 4 */
#define RXING_MODE Fld(2, 0, AC_MSKB0)	/* 1:0 */
#define RX_DATA_NEXT 0x4c

#define RXING_DATA Fld(32, 0, AC_FULLDW)	/* 31:0 */
#define RX_CAP_50 0x50

#define M1_CAP_50 Fld(6, 10, AC_MSKB1)	/* 15:10 */
#define RX_EVENT 0x54

#define DATA_RDY Fld(1, 23, AC_MSKB2)	/* 23 */
#define HEADER_RDY Fld(1, 22, AC_MSKB2)	/* 22 */
#define MODE_RDY Fld(1, 21, AC_MSKB2)	/* 21 */
#define OV Fld(1, 20, AC_MSKB2)	/* 20 */
#define BR_SB_RDY Fld(1, 18, AC_MSKB2)	/* 18 */
#define SB_RDY Fld(1, 17, AC_MSKB2)	/* 17 */
#define BR_RDY Fld(1, 16, AC_MSKB2)	/* 16 */
#define I_EN_DATA Fld(1, 7, AC_MSKB0)	/* 7 */
#define I_EN_HEADER Fld(1, 6, AC_MSKB0)	/* 6 */
#define I_EN_MODE Fld(1, 5, AC_MSKB0)	/* 5 */
#define I_EN_OV Fld(1, 4, AC_MSKB0)	/* 4 */
#define I_EN_PULSE Fld(1, 3, AC_MSKB0)	/* 3 */
#define I_EN_BR_SB Fld(1, 2, AC_MSKB0)	/* 2 */
#define I_EN_SB  Fld(1, 1, AC_MSKB0)	/* 1 */
#define I_EN_BR Fld(1, 0, AC_MSKB0)	/* 0 */
#define RX_GEN_WD 0x58

#define BUF1_WD Fld(16, 16, AC_FULLW32)	/* 31:16 */
#define BUF0_WD Fld(16, 0, AC_FULLW10)	/* 15:0 */
#define RX_GEN_MASK 0x5c

#define BUF1_MASK Fld(16, 16, AC_FULLW32)	/* 31:16 */
#define BUF0_MASK Fld(16, 0, AC_FULLW10)	/* 15:0 */
#define RX_GEN_RCVD 0x60

#define BUF1_RCVD Fld(16, 16, AC_FULLW32)	/* 31:16 */
#define BUF0_RCVD Fld(16, 0, AC_FULLW10)	/* 15:0 */
#define RX_GEN_INTR 0x64

#define BUF1_INTR Fld(16, 16, AC_FULLW32)	/* 31:16 */
#define BUF0_INTR Fld(16, 0, AC_FULLW10)	/* 15:0 */
#define RX_FAIL 0x68

#define ERR_ONCE Fld(1, 4, AC_MSKB0)	/* 4 */
#define ERR_H Fld(1, 0, AC_MSKB0)	/* 0 */
#define RX_STATUS 0x6c

#define RX_BIT_COUNTER Fld(4, 28, AC_MSKB3)	/* 31:28 */
#define RX_TIMER Fld(11, 16, AC_MSKW32)	/* 26:16 */
#define RX_G_PTR Fld(1, 15, AC_MSKB1)	/* 15 */
#define RX_FSM Fld(14, 0, AC_MSKW10)	/* 13:0 */
#define TX_HD_NEXT 0x80

#define WTX_M3_DATA_MASK Fld(4, 24, AC_MSKB3)	/* 27:24 */
#define WTX_SRC Fld(4, 20, AC_MSKB2)	/* 23:20 */
#define WTX_DST Fld(4, 16, AC_MSKB2)	/* 19:16 */
#define WTX_H_EOM Fld(1, 15, AC_MSKB1)	/* 15 */
#define WTX_D_EOM Fld(1, 13, AC_MSKB1)	/* 13 */
#define WTX_M3_ID Fld(3, 8, AC_MSKB1)	/* 10:8 */
#define WTX_M1_DIR Fld(1, 7, AC_MSKB0)	/* 7 */
#define WTX_M1_PAS Fld(1, 6, AC_MSKB0)	/* 6 */
#define WTX_M1_NAS Fld(1, 5, AC_MSKB0)	/* 5 */
#define WTX_M1_DES Fld(1, 4, AC_MSKB0)	/* 4 */
#define WTX_MODE Fld(2, 0, AC_MSKB0)	/* 1:0 */
#define TX_DATA_NEXT 0x84

#define WTX_DATA Fld(32, 0, AC_FULLDW)	/* 31:0 */
#define TX_HEADER 0x88

#define TXING_M3_DATA_SENT Fld(4, 28, AC_MSKB3)	/* 31:28 */
#define TXING_M3_DATA_MASK Fld(4, 24, AC_MSKB3)	/* 27:24 */
#define TXING_SRC Fld(4, 20, AC_MSKB2)	/* 23:20 */
#define TXING_DST Fld(4, 16, AC_MSKB2)	/* 19:16 */
#define TXING_H_EOM Fld(1, 15, AC_MSKB1)	/* 15 */
#define TXING_H_ACK Fld(1, 14, AC_MSKB1)	/* 14 */
#define TXING_D_EOM Fld(1, 13, AC_MSKB1)	/* 13 */
#define TXING_D_ACK Fld(1, 12, AC_MSKB1)	/* 12 */
#define TXING_M3_ID Fld(3, 8, AC_MSKB1)	/* 10:8 */
#define TXING_M1_DIR Fld(1, 7, AC_MSKB0)	/* 7 */
#define TXING_M1_PAS Fld(1, 6, AC_MSKB0)	/* 6 */
#define TXING_M1_NAS Fld(1, 5, AC_MSKB0)	/* 5 */
#define TXING_M1_DES Fld(1, 4, AC_MSKB0)	/* 4 */
#define TXING_MODE Fld(2, 0, AC_MSKB0)	/* 1:0 */
#define TX_DATA 0x8c

#define TXING_DATA Fld(32, 0, AC_FULLDW)	/* 31:0 */
#define RX_CAP_90 0x90

#define M1_CAP_90 Fld(6, 10, AC_MSKB1)	/* 15:10 */
#define TX_EVENT 0x94

#define UN Fld(1, 20, AC_MSKB2)	/* 20 */
#define FAIL Fld(1, 19, AC_MSKB2)	/* 19 */
#define LOWB Fld(1, 18, AC_MSKB2)	/* 18 */
#define BS Fld(1, 17, AC_MSKB2)	/* 17 */
#define RB_RDY Fld(1, 16, AC_MSKB2)	/* 16 */
#define I_EN_UN Fld(1, 4, AC_MSKB0)	/* 4 */
#define I_EN_FAIL Fld(1, 3, AC_MSKB0)	/* 3 */
#define I_EN_LOW Fld(1, 2, AC_MSKB0)	/* 2 */
#define I_EN_BS Fld(1, 1, AC_MSKB0)	/* 1 */
#define I_EN_RB Fld(1, 0, AC_MSKB0)	/* 0 */
#define TX_GEN_RD 0x98

#define BUF1_RD Fld(16, 16, AC_FULLW32)	/* 31:16 */
#define BUF0_RD Fld(16, 0, AC_FULLW10)	/* 15:0 */
#define TX_GEN_MASK 0x9c

#define BUF1_MASK Fld(16, 16, AC_FULLW32)	/* 31:16 */
#define BUF0_MASK Fld(16, 0, AC_FULLW10)	/* 15:0 */
#define TX_GEN_SENT 0xa0

#define BUF1_SENT Fld(16, 16, AC_FULLW32)	/* 31:16 */
#define BUF0_SENT Fld(16, 0, AC_FULLW10)	/* 15:0 */
#define TX_GEN_INTR 0xa4

#define BUF1_INTR Fld(16, 16, AC_FULLW32)	/* 31:16 */
#define BUF0_INTR Fld(16, 0, AC_FULLW10)	/* 15:0 */
#define TX_FAIL 0xa8

#define RETX_MAX Fld(1, 28, AC_MSKB3)	/* 28 */
#define DATA Fld(1, 24, AC_MSKB3)	/* 24 */
#define CMD Fld(1, 20, AC_MSKB2)	/* 20 */
#define HEADER Fld(1, 16, AC_MSKB2)	/* 16 */
#define SOURCE Fld(1, 12, AC_MSKB1)	/* 12 */
#define ID Fld(1, 8, AC_MSKB1)	/* 8 */
#define FMODE Fld(1, 4, AC_MSKB0)	/* 4 */
#define FDIR Fld(1, 0, AC_MSKB0)	/* 0 */
#define TX_STATUS 0xac

#define TX_BIT_COUNTER Fld(4, 28, AC_MSKB3)	/* 31:28 */
#define TX_TIMER Fld(11, 16, AC_MSKW32)	/* 26:16 */
#define TX_G_PTR Fld(1, 15, AC_MSKB1)	/* 15 */
#define TX_FSM Fld(15, 0, AC_MSKW10)	/* 14:0 */
#define TR_TEST 0xbc

#define PAD_PULL_HIGH Fld(1, 31, AC_MSKB3)	/* 31 */
#define PAD_ENABLE Fld(1, 30, AC_MSKB3)	/* 30 */

#define RX_ADDR_CHECK Fld(1, 8, AC_MSKB1)	/* 8 */
#define TX_COMP_TIM Fld(5, 0, AC_MSKB0)	/* 4:0 */


typedef struct {
	u8 destination:4;
	u8 initiator:4;
} CEC_HEADER_BLOCK;

typedef struct {
	CEC_HEADER_BLOCK header;
	u8 opcode;
	u8 operand[15];
} CEC_FRAME_BLOCK;

typedef struct {
	u8 size;
	u8 sendidx;
	u8 reTXcnt;
	void *txtag;
	CEC_FRAME_BLOCK blocks;
} CEC_FRAME_DESCRIPTION;

typedef struct {
	u8 ui1_la;
	u16 ui2_pa;
} CEC_ADDRESS;

typedef struct {
	u8 ui1_num;
	u8 aui1_la[3];
	u16 ui2_pa;
} CEC_LA_ADDRESS;

typedef struct {
	u8 u1Size;
	u8 au1Data[14];
} CEC_SLT_DATA;

typedef enum {
	CEC_LOG_ADDR_TV = 0,
	CEC_LOG_ADDR_REC_DEV_1,
	CEC_LOG_ADDR_REC_DEV_2,
	CEC_LOG_ADDR_TUNER_1,
	CEC_LOG_ADDR_PLAYBACK_DEV_1,
	CEC_LOG_ADDR_AUD_SYS,
	CEC_LOG_ADDR_TUNER_2,
	CEC_LOG_ADDR_TUNER_3,
	CEC_LOG_ADDR_PLAYBACK_DEV_2,
	CEC_LOG_ADDR_REC_DEV_3,
	CEC_LOG_ADDR_TUNER_4,
	CEC_LOG_ADDR_PLAYBACK_DEV_3,
	CEC_LOG_ADDR_RESERVED_1,
	CEC_LOG_ADDR_RESERVED_2,
	CEC_LOG_ADDR_FREE_USE,
	CEC_LOG_ADDR_UNREGED_BRDCST,
	CEC_LOG_ADDR_MAX
} CEC_DRV_LOG_ADDR_T;

typedef enum {
	GET_CMD_ERR = 0x11,
	GET_CMD_EMPTY = 0x33,
	GET_CMD_UNEMPTY = 0x55
} CEC_CMD_STATE;

typedef struct _CEC_ADDR_CONFIG {
	u8 ui1_la_num;
	u8 e_la[CEC_MAX_DEV_LA_NUM];
	u16 ui2_pa;
	u16 h_cecm_svc;
} CEC_DRV_ADDR_CFG_T;

typedef struct _CEC_FRAME_INFO_T {
	u8 ui1_init_addr;
	u8 ui1_dest_addr;
	u16 ui2_opcode;
	u8 aui1_operand[CEC_MAX_OPERAND_SIZE];
	u32 z_operand_size;
} CEC_FRAME_INFO_T;

typedef struct _CEC_SEND_MSG_T {
	CEC_FRAME_INFO_T t_frame_info;
	u8 b_enqueue_ok;
} CEC_SEND_MSG_T;

extern void mt8193_cec_init(void);
extern u8 mt8193_cec_isrprocess(u8 u1rxmode);
extern void mt8193_cec_mainloop(u8 u1rxmode);
extern void mt8193_CECMWSetLA(CEC_DRV_ADDR_CFG_T *prAddr);
extern void mt8193_u4CecSendSLTData(u8 *pu1Data);
extern void mt8193_CECMWGet(CEC_FRAME_DESCRIPTION *frame);
extern void mt8193_GetSLTData(CEC_SLT_DATA *rCecSltData);
extern void mt8193_CECMWSend(CEC_SEND_MSG_T *msg);
extern void mt8193_CECMWSetEnableCEC(u8 u1EnCec);
extern void mt8193_NotifyApiCECAddress(CEC_ADDRESS *cecaddr);
extern void mt8193_SetPhysicCECAddress(u16 u2pa, u8 u1la);
#endif
#endif
