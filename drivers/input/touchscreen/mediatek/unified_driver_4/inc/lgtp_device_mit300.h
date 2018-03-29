/***************************************************************************
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 *    File	: lgtp_device_s3320.h
 *    Author(s)   : D3 BSP Touch Team < d3-bsp-touch@lge.com >
 *    Description :
 *
 ***************************************************************************/

#if !defined(_LGTP_DEVICE_MIT300_H_)
#define _LGTP_DEVICE_MIT300_H_

/****************************************************************************
* Nested Include Files
****************************************************************************/
#include <lgtp_common.h>
#include <linux/syscalls.h>


/****************************************************************************
* Mainfest Constants / Defines
****************************************************************************/
#define LPWG_DEBUG_ENABLE					1
#define DELAY_ENABLE						0
#define KNOCKON_DELAY					700

#define MAX_NUM_OF_FINGERS					10
/*Number of channel*/
#define MAX_ROW								26
#define MAX_COL								15
#define MIT_ROW_NUM				0x0B
#define MIT_COL_NUM				0x0C

#define SECTION_NUM     3
#define PAGE_HEADER     3
#define PAGE_DATA       1024
#define PAGE_CRC        2
#define PACKET_SIZE     (PAGE_HEADER + PAGE_DATA + PAGE_CRC)


/*MIP4_REG*/
/* Address */
#define MIP_R0_INFO						0x01
#define MIP_R1_INFO_PRODUCT_NAME			0x00
#define MIP_R1_INFO_RESOLUTION_X			0x10
#define MIP_R1_INFO_RESOLUTION_Y			0x12
#define MIP_R1_INFO_NODE_NUM_X			0x14
#define MIP_R1_INFO_NODE_NUM_Y			0x15
#define MIP_R1_INFO_KEY_NUM				0x16
#define MIP_R1_INFO_VERSION_BOOT			0x20
#define MIP_R1_INFO_VERSION_CORE			0x22
#define MIP_R1_INFO_VERSION_CUSTOM		0x24
#define MIP_R1_INFO_VERSION_PARAM		0x26
#define MIP_R1_INFO_SECT_BOOT_START		0x30
#define MIP_R1_INFO_SECT_BOOT_END		0x31
#define MIP_R1_INFO_SECT_CORE_START		0x32
#define MIP_R1_INFO_SECT_CORE_END		0x33
#define MIP_R1_INFO_SECT_CUSTOM_START	0x34
#define MIP_R1_INFO_SECT_CUSTOM_END		0x35
#define MIP_R1_INFO_SECT_PARAM_START	0x36
#define MIP_R1_INFO_SECT_PARAM_END		0x37
#define MIP_R1_INFO_BUILD_DATE			0x40
#define MIP_R1_INFO_BUILD_TIME			0x44
#define MIP_R1_INFO_CHECKSUM_PRECALC	0x48
#define MIP_R1_INFO_CHECKSUM_REALTIME	0x4A
#define MIP_R1_INFO_CHECKSUM_CALC		0x4C
#define MIP_R1_INFO_PROTOCOL_NAME		0x50
#define MIP_R1_INFO_PROTOCOL_VERSION	0x58
#define MIP_R1_INFO_IC_ID					0x70
#define MIP_R1_INFO_IC_NAME					0x71

#define MIP_R0_EVENT						0x02
#define MIP_R1_EVENT_SUPPORTED_FUNC		0x00
#define MIP_R1_EVENT_FORMAT				0x04
#define MIP_R1_EVENT_SIZE					0x06
#define MIP_R1_EVENT_PACKET_INFO			0x10
#define MIP_R1_EVENT_PACKET_DATA			0x11

#define MIP_R0_CTRL						0x06
#define MIP_R1_CTRL_READY_STATUS			0x00
#define MIP_R1_CTRL_EVENT_READY			0x01
#define MIP_R1_CTRL_MODE					0x10
#define MIP_R1_CTRL_EVENT_TRIGGER_TYPE	0x11
#define MIP_R1_CTRL_RECALIBRATE			0x12
#define MIP_R1_CTRL_POWER_STATE			0x13
#define MIP_R1_CTRL_GESTURE_TYPE			0x14
#define MIP_R1_CTRL_DISABLE_ESD_ALERT	0x18
#define MIP_R1_CTRL_CHARGER_MODE			0x19
#define MIP_R1_CTRL_GLOVE_MODE			0x1A
#define MIP_R1_CTRL_WINDOW_MODE			0x1B
#define MIP_R1_CTRL_PALM_REJECTION		0x1C
#define MIP_R1_CTRL_EDGE_EXPAND			0x1D
#define MIP_R1_CTRL_LPWG_DEBUG_ENABLE	0x1F

#define MIP_R0_PARAM						0x08
#define MIP_R1_PARAM_BUFFER_ADDR			0x00
#define MIP_R1_PARAM_PROTOCOL			0x04
#define MIP_R1_PARAM_MODE					0x10

#define MIP_R0_TEST						0x0A
#define MIP_R1_TEST_BUF_ADDR				0x00
#define MIP_R1_TEST_PROTOCOL				0x02
#define MIP_R1_TEST_TYPE					0x10
#define MIP_R1_TEST_DATA_FORMAT			0x20
#define MIP_R1_TEST_ROW_NUM				0x20
#define MIP_R1_TEST_COL_NUM				0x21
#define MIP_R1_TEST_BUFFER_COL_NUM		0x22
#define MIP_R1_TEST_COL_AXIS				0x23
#define MIP_R1_TEST_KEY_NUM				0x24
#define MIP_R1_TEST_DATA_TYPE			0x25

#define MIP_R0_IMAGE						0x0C
#define MIP_R1_IMAGE_BUF_ADDR			0x00
#define MIP_R1_IMAGE_PROTOCOL_ID			0x04
#define MIP_R1_IMAGE_TYPE					0x10
#define MIP_R1_IMAGE_DATA_FORMAT			0x20
#define MIP_R1_IMAGE_ROW_NUM				0x20
#define MIP_R1_IMAGE_COL_NUM				0x21
#define MIP_R1_IMAGE_BUFFER_COL_NUM		0x22
#define MIP_R1_IMAGE_COL_AXIS			0x23
#define MIP_R1_IMAGE_KEY_NUM				0x24
#define MIP_R1_IMAGE_DATA_TYPE			0x25
#define MIP_R1_IMAGE_FINGER_NUM			0x30
#define MIP_R1_IMAGE_FINGER_AREA			0x31

#define MIP_R0_LPWG						0x0E
#define MIP_R1_VENDOR_INFO				0x00

#define MIP_R0_LOG							0x10
#define MIP_R1_LOG_TRIGGER				0x14

/* Value */
#define MIP_EVENT_INPUT_PRESS			0x80
#define MIP_EVENT_INPUT_SCREEN			0x40
#define MIP_EVENT_INPUT_HOVER			0x20
#define MIP_EVENT_INPUT_PALM				0x10
#define MIP_EVENT_INPUT_ID				0x0F

#define MIP_EVENT_GESTURE_C				1
#define MIP_EVENT_GESTURE_W				2
#define MIP_EVENT_GESTURE_V				3
#define MIP_EVENT_GESTURE_M				4
#define MIP_EVENT_GESTURE_S				5
#define MIP_EVENT_GESTURE_Z				6
#define MIP_EVENT_GESTURE_O				7
#define MIP_EVENT_GESTURE_E				8
#define MIP_EVENT_GESTURE_V_90			9
#define MIP_EVENT_GESTURE_V_180			10
#define MIP_EVENT_GESTURE_FLICK_RIGHT	20
#define MIP_EVENT_GESTURE_FLICK_DOWN	21
#define MIP_EVENT_GESTURE_FLICK_LEFT	22
#define MIP_EVENT_GESTURE_FLICK_UP		23
#define MIP_EVENT_GESTURE_DOUBLE_TAP	24
#define MIP_EVENT_GESTURE_MULTI_TAP		25
#define MIP_EVENT_GESTURE_ALL			0xFFFFFFFF

#define MIP_ALERT_ESD					1
#define MIP_ALERT_WAKEUP				2
#define MIP_ALERT_INPUTTYPE			3
#define MIP_ALERT_F1				0xF1
#define MIP_LPWG_EVENT_TYPE_FAIL		1

#define MIP_CTRL_STATUS_NONE			0x05
#define MIP_CTRL_STATUS_READY		0xA0
#define MIP_CTRL_STATUS_LOG			0x77

#define MIP_CTRL_MODE_NORMAL			0
#define MIP_CTRL_MODE_PARAM			1
#define MIP_CTRL_MODE_TEST_CM		2

#define MIP_CTRL_POWER_ACTIVE		0
#define MIP_CTRL_POWER_LOW			1

#define MIP_TEST_TYPE_NONE			0
#define MIP_TEST_TYPE_CM_DELTA		1
#define MIP_TEST_TYPE_CM_ABS			2
#define MIP_TEST_TYPE_CM_JITTER		3
#define MIP_TEST_TYPE_SHORT			4
#define MIP_TEST_TYPE_INTR_H				5
#define MIP_TEST_TYPE_INTR_L				6
#define MIP_TEST_TYPE_SHORT2				7

#define MIP_IMG_TYPE_NONE				0
#define MIP_IMG_TYPE_INTENSITY		1
#define MIP_IMG_TYPE_RAWDATA			2
#define MIP_IMG_TYPE_WAIT				255

#define MIP_TRIGGER_TYPE_NONE		0
#define MIP_TRIGGER_TYPE_INTR		1
#define MIP_TRIGGER_TYPE_REG			2

#define MIP_LOG_MODE_NONE				0
#define MIP_LOG_MODE_TRIG				1

/* LPWG Register map */
/* Control */
#define MIP_R1_LPWG_START					0x10
#define MIP_R1_LPWG_ENABLE_SENSING			0x11

/* Public */
#define MIP_R1_LPWG_IDLE_REPORTRATE			0x21
#define MIP_R1_LPWG_ACTIVE_REPORTRATE		0x22
#define MIP_R1_LPWG_SENSITIVITY				0x23
#define MIP_R1_LPWG_ACTIVE_AREA			0x24
#define MIP_R1_LPWG_FAIL_REASON			0x2C

/* Knock On */
#define MIP_R1_LPWG_ENABLE					0x40
#define MIP_R1_LPWG_WAKEUP_TAP_COUNT		0x41
#define MIP_R1_LPWG_TOUCH_SLOP				0x42
#define MIP_R1_LPWG_MIN_INTERTAP_DISTANCE	0x44
#define MIP_R1_LPWG_MAX_INTERTAP_DISTANCE	0x46
#define MIP_R1_LPWG_MIN_INTERTAP_TIME		0x48
#define MIP_R1_LPWG_MAX_INTERTAP_TIME		0x4A
#define MIP_R1_LPWG_INT_DELAY_TIME			0x4C

/* Knock Code */
#define MIP_R1_LPWG_ENABLE2					0x50
#define MIP_R1_LPWG_WAKEUP_TAP_COUNT2		0x51
#define MIP_R1_LPWG_TOUCH_SLOP2				0x52
#define MIP_R1_LPWG_MIN_INTERTAP_DISTANCE2	0x54
#define MIP_R1_LPWG_MAX_INTERTAP_DISTANCE2	0x56
#define MIP_R1_LPWG_MIN_INTERTAP_TIME2		0x58
#define MIP_R1_LPWG_MAX_INTERTAP_TIME2		0x5A
#define MIP_R1_LPWG_INT_DELAY_TIME2			0x5C


/****************************************************************************
* Type Definitions
****************************************************************************/
struct melfas_ts_data {
	struct i2c_client *client;
	TouchState currState;
	LpwgSetting lpwgSetting;
};

/* Firmware update error code */
enum fw_update_errno {
	fw_err_file_read = -4,
	fw_err_file_open = -3,
	fw_err_file_type = -2,
	fw_err_download = -1,
	fw_err_none = 0,
	fw_err_uptodate = 1,
};

enum {
	RAW_DATA_SHOW = 0,
	INTENSITY_SHOW,
	ABS_SHOW,
	DELTA_SHOW,
	JITTER_SHOW,
	OPENSHORT_SHOW,
	MUXSHORT_SHOW,
};

enum {
	OUT_OF_AREA = 1,
	PALM_DETECTED,
	DELAY_TIME,
	TAP_TIME,
	TAP_DISTACE,
	TOUCH_SLOPE,
	MULTI_TOUCH,
	LONG_PRESS
};


/****************************************************************************
* Exported Variables
****************************************************************************/


/****************************************************************************
* Macros
****************************************************************************/


/****************************************************************************
* Global Function Prototypes
****************************************************************************/
int mip_bin_fw_version(struct melfas_ts_data *ts, const u8 *fw_data, size_t fw_size, u8 *ver_buf);
int mip_flash_fw(struct melfas_ts_data *ts, const u8 *fw_data, size_t fw_size, bool force,
		 bool section);
ssize_t MIT300_GetTestResult(struct i2c_client *client, char *buf, int *result, int type);


#endif				/* _LGTP_DEVICE_MIT200_H_ */

/* End Of File */
