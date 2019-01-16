#ifndef __mt8193hdcp_h__
#define __mt8193hdcp_h__
#ifdef CONFIG_MTK_MT8193_HDMI_SUPPORT


typedef enum {
	HDMI_PLUG_OUT = 0,
	HDMI_PLUG_IN_AND_SINK_POWER_ON,
	HDMI_PLUG_IN_ONLY,
	HDMI_PLUG_IN_EDID,
	HDMI_PLUG_IN_CEC,
	HDMI_PLUG_IN_POWER_EDID
} HDMI_NFY_PLUG_STATE_T;

typedef enum {
	HDMI_EDID_NOT_READY = 0,
	HDMI_EDID_IS_READY,
	HDMI_EDID_IS_ERROR
} HDMI_NFY_EDID_STATE_T;

typedef enum {
	HDCP_RECEIVER_NOT_READY = 0x00,
	HDCP_READ_EDID,
	HDCP_INIT_AUTHENTICATION,
	HDCP_WAIT_R0,
	HDCP_COMPARE_R0,
	HDCP_WAIT_RI,
	HDCP_CHECK_LINK_INTEGRITY,
	HDCP_RE_DO_AUTHENTICATION,
	HDCP_RE_COMPARE_RI,
	HDCP_RE_COMPARE_R0,
	HDCP_CHECK_REPEATER,
	HDCP_WAIT_KSV_LIST,
	HDCP_READ_KSV_LIST,
	HDCP_COMPARE_V,
	HDCP_RETRY_FAIL,
	HDCP_WAIT_RESET_OK,
	HDCP_WAIT_RES_CHG_OK,
	HDCP_WAIT_SINK_UPDATE_RI_DDC_PORT
} HDCP_CTRL_STATE_T;

typedef enum {
	HDMI_STATE_IDLE = 0,
	HDMI_STATE_HOT_PLUG_OUT,
	HDMI_STATE_HOT_PLUGIN_AND_POWER_ON,
	HDMI_STATE_HOT_PLUG_IN_ONLY,
	HDMI_STATE_READ_EDID,
	HDMI_STATE_POWER_ON_READ_EDID,
	HDMI_STATE_POWER_ON_HOT_PLUG_OUT,

} HDMI_CTRL_STATE_T;

typedef enum {
	EXTERNAL_KEY = 0,
	INTERNAL_NOENCRYPT_KEY,
	INTERNAL_ENCRYPT_KEY
} HDMI_HDCP_KEY_T;

typedef enum {
	SI_EDID_VSDB_EXIST = 0,
	SI_HDMI_RECEIVER_STATUS,
	SI_HDMI_PORD_OFF_PLUG_ONLY,
	SI_EDID_EXT_BLOCK_NO,
	SI_EDID_PARSING_RESULT,
	SI_HDMI_SUPPORTS_AI,
	SI_HDMI_HDCP_RESULT,
	SI_REPEATER_DEVICE_COUNT,
	SI_HDMI_CEC_LA,
	SI_HDMI_CEC_ACTIVE_SOURCE,
	SI_HDMI_CEC_PROCESS,
	SI_HDMI_CEC_PARA0,
	SI_HDMI_CEC_PARA1,
	SI_HDMI_CEC_PARA2,
	SI_HDMI_NO_HDCP_TEST,
	SI_HDMI_SRC_CONTROL,
	SI_A_CODE_MODE,
	SI_EDID_AUDIO_CAPABILITY,
	SI_HDMI_AUDIO_INPUT_SOURCE,
	SI_HDMI_AUDIO_CH_NUM,
	SI_HDMI_DVD_AUDIO_PROHIBIT,
	SI_DVD_HDCP_REVOCATION_RESULT
} HDMI_SHARE_INFO_TYPE_T;
#define MAX_HDMI_SHAREINFO  64

/* Notice: there are three device ID for SiI9993 (Receiver) */
#define RX_ID       0x3A	/* Max'0308'04, 0x74 */
/* (2.2) Define the desired register address of receiver */
/* Software Reset Register */
#define RX_REG_SRST           0x05
/* An register (total 8 bytes, address from 0x18 ~ 0x1F) */
#define RX_REG_HDCP_AN        0x18
/* Aksv register (total 5 bytes, address from 0x10 ~ 0x14) */
#define RX_REG_HDCP_AKSV      0x10
/* Bksv register (total 5 bytes, address from 0x00 ~ 0x04) */
#define RX_REG_HDCP_BKSV      0x00
/* BCAPS register */
#define RX_REG_BCAPS          0x40
#define RX_BIT_ADDR_RPTR      0x40	/* bit 6 */
#define RX_BIT_ADDR_READY     0x20	/* bit 5 */

#define RX_REG_BSTATUS1       0x41
#define DEVICE_COUNT_MASK     0xff
#define MAX_DEVS_EXCEEDED  (0x01<<7)
#define MAX_CASCADE_EXCEEDED (0x01<<3)

#define RX_REG_KSV_FIFO       0x43
#define RX_REG_REPEATER_V     0x20
/* Ri register (total 2 bytes, address from 0x08 ~ 0x09) */
#define RX_REG_RI             0x08

/* (2) Define the counter for An register byte */
#define HDCP_AN_COUNT                 8

/* (3) Define the counter for HDCP Aksv register byte */
#define HDCP_AKSV_COUNT               5

/* (3) Define the counter for HDCP Bksv register byte */
#define HDCP_BKSV_COUNT               5

/* (4) Define the counter for Ri register byte */
#define HDCP_RI_COUNT                 2


#define HDCP_WAIT_5MS_TIMEOUT          5	/* 5 ms, */
#define HDCP_WAIT_10MS_TIMEOUT          10	/* 10 ms, */
#define HDCP_WAIT_300MS_TIMEOUT          300	/* 10 ms, */
#define HDCP_WAIT_R0_TIMEOUT          100	/* 25//for timer 5ms      // 100 ms, */
#define HDCP_WAIT_KSV_LIST_TIMEOUT    100	/* 4600//5000//5500//1100//for timer 5ms //5000   // 5.5 sec */
#define HDCP_WAIT_KSV_LIST_RETRY_TIMEOUT    100
#define HDCP_WAIT_RI_TIMEOUT          2500	/* 500//for timer 5ms //2000   // 2.5 sec, */
#define HDCP_WAIT_RE_DO_AUTHENTICATION 200	/* 20////for timer 20ms 200 //kenny 100->200 */
#define HDCP_WAIT_RECEIVER_READY      1000	/* 50//for 20ms timer //1000   // 1 sec, 50*20ms */
#define HDCP_WAIT_RES_CHG_OK_TIMEOUE 500	/* 30//6 */
#define HDCP_WAIT_V_RDY_TIMEOUE 500	/* 30//6 */

#define HDCP_CHECK_KSV_LIST_RDY_RETRY_COUNT    56	/* 10 */




#define  HDMI_H0  0x67452301
#define  HDMI_H1  0xefcdab89
#define  HDMI_H2  0x98badcfe
#define  HDMI_H3  0x10325476
#define  HDMI_H4  0xc3d2e1f0

#define  HDMI_K0  0x5a827999
#define  HDMI_K1  0x6ed9eba1
#define  HDMI_K2  0x8f1bbcdc
#define  HDMI_K3  0xca62c1d6

/* for HDCP key access method */
#define HOST_ACCESS                  1	/* Key accessed by host */
#define NON_HOST_ACCESS_FROM_EEPROM  2	/* Key auto accessed by HDCP hardware from eeprom */
#define NON_HOST_ACCESS_FROM_MCM     3	/* Key auto accessed by HDCP hardware from MCM */
#define NON_HOST_ACCESS_FROM_GCPU    4
/* for HDCP */
#define KSV_BUFF_SIZE 192
#define HDCP_KEY_RESERVE 287

extern void vHDCPReset(void);
extern void vHDCPInitAuth(void);
extern void vDisableHDCP(u8 fgDisableHdcp);
extern void HdcpService(HDCP_CTRL_STATE_T e_hdcp_state);

extern u32 i4SharedInfo(u32 u4Index);
extern void vSetSharedInfo(u32 u4Index, u32 i4Value);
extern void vSendHdmiCmd(u8 u8cmd);
extern void vClearHdmiCmd(void);
extern u8 bCheckHDCPStatus(u8 bMode);
extern u8 bReadGRLInt(void);
extern void bClearGRLInt(u8 bInt);
extern void vSetHDCPState(HDCP_CTRL_STATE_T e_state);
extern u8 bReadHdmiIntMask(void);
extern void vMoveHDCPInternalKey(HDMI_HDCP_KEY_T key);
extern void vInitHdcpKeyGetMethod(u8 bMethod);
extern void mt8193_hdcpkey(u8 *pbhdcpkey);
extern void vShowHdcpRawData(void);
extern void mt8193_mutehdmi(u8 u1flagvideomute, u8 u1flagaudiomute);
#endif
#endif
