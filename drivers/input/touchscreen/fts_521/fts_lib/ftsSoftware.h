/*

**************************************************************************
**                        STMicroelectronics							**
**************************************************************************
**                        marco.cali@st.com								**
**************************************************************************
*                                                                        *
*							FW related data								 *
*                                                                        *
**************************************************************************
**************************************************************************

*/

/*!
* \file ftsSoftware.h
* \brief Contains all the definitions and information related to the IC from a fw/driver point of view
*/

#ifndef FTS_SOFTWARE_H
#define FTS_SOFTWARE_H
#include <linux/types.h>
#include "ftsHardware.h"


typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;


typedef signed char i8;

/**
 *	Enumerator which contains all the possible address length expressed in bytes.
 */
typedef enum {
	NO_ADDR = 0,
	BITS_8 = 1,
	BITS_16 = 2,
	BITS_24 = 3,
	BITS_32 = 4,
	BITS_40 = 5,
	BITS_48 = 6,
	BITS_56 = 7,
	BITS_64 = 8
} AddrSize;

/********************  NEW API  *********************/


/** @defgroup host_command Fw Host op codes
 * Valid op codes for fw commands
 * @{
 */

/** @defgroup scan_mode	Scan Mode
* @ingroup host_command
* Set the scanning mode required according to the parameters
* @{
*/
#define FTS_CMD_SCAN_MODE					0xA0
/** @} */

/** @defgroup feat_sel	 Feature Select
* @ingroup host_command
* Set the system defined features to enable/disable according the parameters
* @{
*/
#define FTS_CMD_FEATURE						0xA2
/** @} */

/** @defgroup sys_cmd  System Command
* @ingroup host_command
* Execute a system command to perform core tasks
* @{
*/
#define FTS_CMD_SYSTEM						0xA4
/** @} */

#define FTS_CMD_LOCKDOWN_ID					0x70

		/** @} */



/** @defgroup scan_opt	 Scan Mode Option
* @ingroup scan_mode
* Valid scanning modes and their options
* @{
*/
#define SCAN_MODE_ACTIVE					0x00
#define SCAN_MODE_LOW_POWER					0x01
#define SCAN_MODE_JIG_1						0x02
/** @}*/


/** @defgroup active_bitmask Active Mode Bitmask
* @ingroup scan_opt
* Bitmask to use to enables the specific scanning with the SCAN_MODE_ACTIVE option
* @{
*/
#define ACTIVE_MULTI_TOUCH					0x01
#define ACTIVE_KEY							0x02
#define ACTIVE_HOVER						0x04
#define ACTIVE_PROXIMITY					0x08
#define ACTIVE_FORCE						0x10
/** @}*/



/** @defgroup feat_opt	 Feature Selection Option
* @ingroup feat_sel
* System defined features that can be enable/disable
* @{
*/
#define FEAT_SEL_GLOVE						0x00
#define FEAT_SEL_COVER						0x01
#define FEAT_SEL_CHARGER					0x02
#define FEAT_SEL_GESTURE					0x03
#define FEAT_SEL_GRIP						0x04
#define FEAT_SEL_STYLUS						0x07
/** @}*/


#define FEAT_ENABLE							1
#define FEAT_DISABLE						0


/** @defgroup charger_opt	 Charger Mode Option
* @ingroup feat_sel
* Option for Charger Mode, it is a bitmask where the each bit indicate a different kind of chager
* @{
*/
#define	CHARGER_CABLE						0x01
#define CHARGER_WIRLESS						0x02
/** @}*/


/** @defgroup gesture_opt	 Gesture Mode Option
* @ingroup feat_sel
* Gesture IDs of the predefined gesture recognized by the fw.
* The ID represent also the position of the corresponding bit in the gesture mask
* @{
*/
#define GEST_ID_UP_1F						0x01
#define GEST_ID_DOWN_1F						0x02
#define GEST_ID_LEFT_1F						0x03
#define GEST_ID_RIGHT_1F					0x04
#define	GEST_ID_DBLTAP						0x05
#define GEST_ID_O							0x06
#define GEST_ID_C							0x07
#define GEST_ID_M							0x08
#define GEST_ID_W							0x09
#define GEST_ID_E							0x0A
#define GEST_ID_L							0x0B
#define GEST_ID_F							0x0C
#define GEST_ID_V							0x0D
#define GEST_ID_AT							0x0E
#define GEST_ID_S							0x0F
#define GEST_ID_Z							0x10
#define GEST_ID_LEFTBRACE					0x11
#define GEST_ID_RIGHTBRACE					0x12
#define GEST_ID_CARET						0x13
/** @}*/



/** @defgroup sys_opt	 System Command Option
* @ingroup sys_cmd
* Valid System Command Parameters
* @{
*/
#define SYS_CMD_SPECIAL						0x00
#define SYS_CMD_INT							0x01
#define SYS_CMD_FORCE_CAL					0x02
#define SYS_CMD_CX_TUNING					0x03
#define SYS_CMD_ITO							0x04
#define SYS_CMD_SAVE_FLASH					0x05
#define SYS_CMD_LOAD_DATA					0x06
/** @} */



/** @defgroup sys_special_opt	 Special Command Option
* @ingroup sys_cmd
* Valid special command
* @{
*/
#define SPECIAL_SYS_RESET					0x00
#define SPECIAL_FIFO_FLUSH					0x01
#define SPECIAL_PANEL_INIT					0x02
#define SPECIAL_FULL_PANEL_INIT				0x03
/** @} */


#define CAL_MS_TOUCH						0x00
#define CAL_MS_LOW_POWER					0x01
#define CAL_SS_TOUCH						0x02
#define CAL_SS_IDLE							0x03
#define CAL_MS_KEY							0x04
#define CAL_SS_KEY							0x05
#define CAL_MS_FORCE						0x06
#define CAL_SS_FORCE						0x07


/** @defgroup ito_opt	 ITO Test Option
* @ingroup sys_cmd
* Valid option for the ITO test
* @{
*/
#define ITO_FORCE_OPEN						0x00
#define ITO_SENSE_OPEN						0x01
#define ITO_FORCE_GROUND					0x02
#define ITO_SENSE_GROUND					0x03
#define ITO_FORCE_VDD						0x04
#define ITO_SENSE_VDD						0x05
#define ITO_FORCE_FORCE						0x06
#define ITO_FORCE_SENSE						0x07
#define ITO_SENSE_SENSE						0x08
#define ITO_KEY_FORCE_OPEN					0x09
#define ITO_KEY_SENSE_OPEN					0x0A
/** @}*/


/** @defgroup save_opt	 Save to Flash Option
* @ingroup sys_cmd
* Valid option for saving data to the Flash
* @{
*/
#define	SAVE_FW_CONF						0x01
#define SAVE_CX								0x02
#define SAVE_PANEL_CONF						0x04
/** @}*/


/** @defgroup load_opt	 Load Host Data Option
* @ingroup sys_cmd
* Valid option to ask to the FW to load host data into the memory
* @{
*/
#define LOAD_SYS_INFO						0x01
#define LOAD_CX_MS_TOUCH					0x10
#define LOAD_CX_MS_LOW_POWER				0x11
#define LOAD_CX_SS_TOUCH					0x12
#define LOAD_CX_SS_TOUCH_IDLE				0x13
#define LOAD_CX_MS_KEY						0x14
#define LOAD_CX_SS_KEY						0x15
#define LOAD_CX_MS_FORCE					0x16
#define LOAD_CX_SS_FORCE					0x17
#define LOAD_SYNC_FRAME_RAW					0x30
#define LOAD_SYNC_FRAME_FILTER				0x31
#define LOAD_SYNC_FRAME_S0TRENGTH			0x33
#define LOAD_SYNC_FRAME_BASELINE			0x32
#define LOAD_PANEL_CX_TOT_MS_TOUCH			0x50
#define LOAD_PANEL_CX_TOT_MS_LOW_POWER		0x51
#define LOAD_PANEL_CX_TOT_SS_TOUCH			0x52
#define LOAD_PANEL_CX_TOT_SS_TOUCH_IDLE		0x53
#define LOAD_PANEL_CX_TOT_MS_KEY			0x54
#define LOAD_PANEL_CX_TOT_SS_KEY			0x55
#define LOAD_PANEL_CX_TOT_MS_FORCE			0x56
#define LOAD_PANEL_CX_TOT_SS_FORCE			0x57
/** @}*/


/** @defgroup events_group	 FW Event IDs and Types
* Event IDs and Types pushed by the FW into the FIFO
* @{
*/
#define EVT_ID_NOEVENT						0x00
#define EVT_ID_CONTROLLER_READY				0x03
#define EVT_ID_ENTER_POINT					0x13
#define EVT_ID_MOTION_POINT					0x23
#define EVT_ID_LEAVE_POINT					0x33
#define EVT_ID_STATUS_UPDATE				0x43
#define EVT_ID_USER_REPORT					0x53
#define EVT_ID_DEBUG						0xE3
#define EVT_ID_ERROR						0xF3

#define NUM_EVT_ID							((EVT_ID_ERROR&0xF0)>>4)
/** @}*/


/** @defgroup status_type	 Status Event Types
* @ingroup events_group
* Types of EVT_ID_STATUS_UPDATE events
* @{
*/
#define EVT_TYPE_STATUS_ECHO				0x01
#define EVT_TYPE_STATUS_FRAME_DROP			0x03
#define EVT_TYPE_STATUS_FORCE_CAL			0x05
#define EVT_TYPE_STATUS_WATER				0x06
#define EVT_TYPE_STATUS_SS_RAW_SAT			0x07
/** @} */


/** @defgroup user_type	 User Event Types
* @ingroup events_group
* Types of EVT_ID_USER_REPORT events generated by thw FW
* @{
*/
#define EVT_TYPE_USER_KEY					0x00
#define EVT_TYPE_USER_PROXIMITY				0x01
#define EVT_TYPE_USER_GESTURE				0x02
/** @}*/


/** @defgroup error_type  Error Event Types
* @ingroup events_group
* Types of EVT_ID_ERROR events reported by the FW
* @{
*/
#define EVT_TYPE_ERROR_WATCHDOG				0x06

#define EVT_TYPE_ERROR_CRC_CFG_HEAD			0x20
#define EVT_TYPE_ERROR_CRC_CFG				0x21
#define EVT_TYPE_ERROR_CRC_PANEL_HEAD		0x22
#define EVT_TYPE_ERROR_CRC_PANEL			0x23

#define EVT_TYPE_ERROR_ITO_FORCETOGND		0x60
#define EVT_TYPE_ERROR_ITO_SENSETOGND		0x61
#define EVT_TYPE_ERROR_ITO_FORCETOVDD		0x62
#define EVT_TYPE_ERROR_ITO_SENSETOVDD		0x63
#define EVT_TYPE_ERROR_ITO_FORCE_P2P		0x64
#define EVT_TYPE_ERROR_ITO_SENSE_P2P		0x65
#define EVT_TYPE_ERROR_ITO_FORCEOPEN		0x66
#define EVT_TYPE_ERROR_ITO_SENSEOPEN		0x67
#define EVT_TYPE_ERROR_ITO_KEYOPEN			0x68

#define EVT_TYPE_ERROR_CRC_CX_HEAD			0xA0
#define EVT_TYPE_ERROR_CRC_CX				0xA1
#define EVT_TYPE_ERROR_CRC_CX_SUB_HEAD		0xA5
#define EVT_TYPE_ERROR_CRC_CX_SUB			0xA6

#define EVT_TYPE_ERROR_ESD					0xF0
/** @}*/

/** @defgroup address Chip Address
 * Collection of HW and SW Addresses useful to collect different kind of data
 * @{
 */

/** @defgroup config_adr SW Address
 * @ingroup address
 * Important addresses of data stored into Config memory (and sometimes their dimensions)
 * @{
 */
#define ADDR_CONFIG_ID						0x0010
#define CONFIG_ID_BYTE						2
#define ADDR_CONFIG_SENSE_LEN				0x0030
#define ADDR_CONFIG_FORCE_LEN				0x0031
/** @}*/

/** @}*/


#define ERROR_DUMP_ROW_SIZE					32
#define ERROR_DUMP_COL_SIZE					4
#define ERROR_DUMP_SIGNATURE				0xFA5005AF


#define TOUCH_TYPE_INVALID					0x00
#define TOUCH_TYPE_FINGER					0x01
#define TOUCH_TYPE_GLOVE					0x02
#define TOUCH_TYPE_STYLUS					0x03
#define TOUCH_TYPE_PALM						0x04
#define TOUCH_TYPE_HOVER					0x05


#define FTS_KEY_0							0x01
#define FTS_KEY_1							0x02
#define FTS_KEY_2							0x04
#define FTS_KEY_3							0x08
#define FTS_KEY_4							0x10
#define FTS_KEY_5							0x20
#define FTS_KEY_6							0x40
#define FTS_KEY_7							0x80

#endif
