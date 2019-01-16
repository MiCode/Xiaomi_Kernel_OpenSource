#ifndef __SI_DEBUG_H__
#define __SI_DEBUG_H__
#include "si_common.h"
#define RCP_DEBUG           0
#define API_DEBUG_CODE      1
#define MSG_ALWAYS              0x00
#define MSG_ERR                 0x00
#define MSG_STAT                0x01
#define MSG_DBG                 0x02
enum {
	I2C_SUCCESS,
	I2C_READ_FAIL,
	I2C_WRITE_FAIL,
};
#if (API_DEBUG_CODE == 1)
#define DF1_SW_HDCPGOOD         0x01
#define DF1_HW_HDCPGOOD         0x02
#define DF1_SCDT_INT            0x04
#define DF1_SCDT_HI             0x08
#define DF1_SCDT_STABLE         0x10
#define DF1_HDCP_STABLE         0x20
#define DF1_NON_HDCP_STABLE     0x40
#define DF1_RI_CLEARED          0x80
#define DF2_MP_AUTH             0x01
#define DF2_MP_DECRYPT          0x02
#define DF2_HPD                 0x04
#define DF2_HDMI_MODE           0x08
#define DF2_MUTE_ON             0x10
#define DF2_PORT_SWITCH_REQ     0x20
#define DF2_PIPE_PWR            0x40
#define DF2_PORT_PWR_CHG        0x80
#endif
#if !defined __PLATFORM_STATUS_CODES__
#define __PLATFORM_STATUS_CODES__
enum {
	DBG_BRD,
	DBG_DEV,
	DBG_CBUS,
	DBG_CDC,
	DBG_CEC,
	DBG_CPI,
	DBG_EDID,
	DBG_HEAC,
	DBG_RPTR,
	DBG_SWCH,
	DBG_VSIF,
	DBG_TX,
	DBG_EDID_TX,
	DBG_MAX_COMPONENTS
};
typedef enum _SkDebugPrintFlags_t {
	DBGF_TS = 0x0100,
	DBGF_CN = 0x0200,
	DBGF_CP = 0x0400,
	DBGF_CS = 0x0800,
} SkDebugPrintFlags_t;
#endif
#endif
