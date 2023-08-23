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

typedef signed char i8;									/*basic type that represent one signed byte (or 8 bits)*/

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
#define FTS_CMD_SCAN_MODE					0xA0	/*OP Code to set scan mode*/
/** @} */

/** @defgroup feat_sel	 Feature Select
* @ingroup host_command
* Set the system defined features to enable/disable according the parameters
* @{
*/
#define FTS_CMD_FEATURE						0xA2	/*OP code to set features*/
/** @} */

/** @defgroup sys_cmd  System Command
* @ingroup host_command
* Execute a system command to perform core tasks
* @{
*/
#define FTS_CMD_SYSTEM						0xA4	/*OP code to write s system command*/

#define FTS_CMD_LOCKDOWN_ID				0x70
#define FTS_CMD_CUSTOM			0xC0
/** @} */

/** @} */

/*SCAN MODE OPTION (0xA0)*/
/** @defgroup scan_opt	 Scan Mode Option
* @ingroup scan_mode
* Valid scanning modes and their options
* @{
*/
#define SCAN_MODE_ACTIVE					0x00	/*Select the Active scanning mode*/
#define SCAN_MODE_LOW_POWER					0x01	/*Select the low power scanning mode*/
#define SCAN_MODE_JIG_1						0x02	/*Select the Jig test 1*/
#define SCAN_MODE_LOCKED					0x03	/*Select the Scan mode which will be locked*/
/** @}*/

/** @defgroup active_bitmask Active Mode Bitmask
* @ingroup scan_opt
* Bitmask to use to enables the specific scanning with the SCAN_MODE_ACTIVE option
* @{
*/
#define ACTIVE_MULTI_TOUCH					0x01	/*Bit 0 MS/SS scan*/
#define ACTIVE_KEY							0x02	/*Bit 1 Key scan*/
#define ACTIVE_HOVER						0x04	/*Bit 2 Hover scan*/
#define ACTIVE_PROXIMITY					0x08	/*Bit 3 Proximity scan*/
#define ACTIVE_FORCE						0x10	/*Bit 4 Force scan*/
/** @}*/

/** @defgroup locked_opt Locked Mode Option
* @ingroup scan_opt
* Options to enable and lock specific scanning with the SCAN_MODE_LOCKED option
* @{
*/
#define LOCKED_ACTIVE						0x00	/*Active Scan Mode*/
#define LOCKED_HOVER						0x01	/*Hover Scan Mode*/
#define LOCKED_IDLE							0x02	/*Idle Scan Mode*/
#define LOCKED_LP_DETECT					0x10	/*Low Power SS*/
#define LOCKED_LP_ACTIVE					0x11	/*Low Power MS*/
/** @}*/

/** @defgroup feat_opt	 Feature Selection Option
* @ingroup feat_sel
* System defined features that can be enable/disable
* @{
*/
#define FEAT_SEL_GLOVE						0x00	/*Glove Mode*/
#define FEAT_SEL_COVER						0x01	/*Cover Mode*/
#define FEAT_SEL_CHARGER					0x02	/*Charger Mode*/
#define FEAT_SEL_GESTURE					0x03	/*Gesture Mode*/
#define FEAT_SEL_GRIP						0x04	/*Grip Detection*/
#define FEAT_SEL_STYLUS						0x07	/*Stylus Mode (this is a driver define, not available in FW)*/
/** @}*/

#define FEAT_ENABLE							1		/*General value to enable a feature*/
#define FEAT_DISABLE						0		/*General value to disable a feature*/

/** @defgroup charger_opt	 Charger Mode Option
* @ingroup feat_sel
* Option for Charger Mode, it is a bitmask where the each bit indicate a different kind of chager
* @{
*/
#define	CHARGER_CABLE						0x01	/*normal usb charger*/
#define CHARGER_WIRLESS						0x02	/*wireless charger*/
/** @}*/

/** @defgroup gesture_opt	 Gesture Mode Option
* @ingroup feat_sel
* Gesture IDs of the predefined gesture recognized by the fw.
* The ID represent also the position of the corresponding bit in the gesture mask
* @{
*/
#define GEST_ID_UP_1F						0x01	/*Bottom to Top line*/
#define GEST_ID_DOWN_1F						0x02	/*Top to bottom line*/
#define GEST_ID_LEFT_1F						0x03	/*Right to left line*/
#define GEST_ID_RIGHT_1F					0x04	/*Left to right line*/
#define	GEST_ID_DBLTAP						0x05	/*Double Tap*/
#define GEST_ID_O							0x06	/*'O'*/
#define GEST_ID_C							0x07	/*'C'*/
#define GEST_ID_M							0x08	/*'M'*/
#define GEST_ID_W							0x09	/*'W'*/
#define GEST_ID_E							0x0A	/*'e'*/
#define GEST_ID_L							0x0B	/*'L'*/
#define GEST_ID_F							0x0C	/*'F'*/
#define GEST_ID_V							0x0D	/*'V'*/
#define GEST_ID_AT							0x0E	/*'@'*/
#define GEST_ID_S							0x0F	/*'S'*/
#define GEST_ID_Z							0x10	/*'Z'*/
#define GEST_ID_LEFTBRACE					0x11	/*'<'*/
#define GEST_ID_RIGHTBRACE					0x12	/*'>'*/
#define GEST_ID_CARET						0x13	/*'^'*/
#define GEST_ID_LONG_PRESS					0x18	/*'^'*/
#define GEST_ID_SINGTAP						0x19	/*'^'*/
/** @}*/

/** @defgroup sys_opt	 System Command Option
* @ingroup sys_cmd
* Valid System Command Parameters
* @{
*/
#define SYS_CMD_SPECIAL						0x00	/*Special Commands*/
#define SYS_CMD_INT							0x01	/*FW Interrupt Control*/
#define SYS_CMD_FORCE_CAL					0x02	/*Force Calibration*/
#define SYS_CMD_CX_TUNING					0x03	/*CX initialization*/
#define SYS_CMD_ITO							0x04	/*ITO test*/
#define SYS_CMD_SAVE_FLASH					0x05	/*Saving to flash*/
#define SYS_CMD_LOAD_DATA					0x06	/*Load Host data memory*/
#define SYS_CMD_SPECIAL_TUNING				0x08	/*Perform some special tuning*/
/** @} */

/** @defgroup sys_special_opt	 Special Command Option
* @ingroup sys_cmd
* Valid special command
* @{
*/
#define SPECIAL_SYS_RESET					0x00	/*System Reset triggered by the FW*/
#define SPECIAL_FIFO_FLUSH					0x01	/*Flush of the FIFO*/
#define SPECIAL_PANEL_INIT					0x02	/*Panel Initialization*/
#define SPECIAL_FULL_PANEL_INIT				0x03	/*Full panel initialization*/
/** @} */

#define CAL_MS_TOUCH						0x00	/*Mutual Sense Touch*/
#define CAL_MS_LOW_POWER					0x01	/*Mutual Sense Touch in low power mode*/
#define CAL_SS_TOUCH						0x02	/*Self Sense Touch*/
#define CAL_SS_IDLE							0x03	/*Self Sense Touch in idle mode*/
#define CAL_MS_KEY							0x04	/*Mutual Sense Key*/
#define CAL_SS_KEY							0x05	/*Self Sense Key*/
#define CAL_MS_FORCE						0x06	/*Mutual Sense Force*/
#define CAL_SS_FORCE						0x07	/*Self Sense Force*/

/** @defgroup ito_opt	 ITO Test Option
* @ingroup sys_cmd
* Valid option for the ITO test
* @{
*/
#define ITO_FORCE_OPEN						0x00	/*Check if some force channels is open*/
#define ITO_SENSE_OPEN						0x01	/*Check if some sense channels is open*/
#define ITO_FORCE_GROUND					0x02	/*Check if some force channels is short to ground*/
#define ITO_SENSE_GROUND					0x03	/*Check if some sense channels is short to ground*/
#define ITO_FORCE_VDD						0x04	/*Check if some force channels is short to VDD*/
#define ITO_SENSE_VDD						0x05	/*Check if some sense channels is short to VDD*/
#define ITO_FORCE_FORCE						0x06	/*Check force to force channels*/
#define ITO_FORCE_SENSE						0x07	/*Check force to sense channels*/
#define ITO_SENSE_SENSE						0x08	/*Check sense to sense channels*/
#define ITO_KEY_FORCE_OPEN					0x09	/*Check if some force channels used for the key is open*/
#define ITO_KEY_SENSE_OPEN					0x0A	/*Check if some sense channels used for the key is open*/
/** @}*/

/** @defgroup save_opt	 Save to Flash Option
* @ingroup sys_cmd
* Valid option for saving data to the Flash
* @{
*/
#define	SAVE_FW_CONF						0x01	/*Save the confing to the flash*/
#define SAVE_CX								0x02	/*Save the CX to the flash*/
#define SAVE_PANEL_CONF						0x04	/*Save the Panel configuration to the flash*/
/** @}*/

/** @defgroup load_opt	 Load Host Data Option
* @ingroup sys_cmd
* Valid option to ask to the FW to load host data into the memory
* @{
*/
#define LOAD_SYS_INFO						0x01	/*Load System Info*/
#define LOAD_CX_MS_TOUCH					0x10	/*Load MS Init Data for Active Mode*/
#define LOAD_CX_MS_LOW_POWER				0x11	/*Load MS Init Data for Low Power Mode*/
#define LOAD_CX_SS_TOUCH					0x12	/*Load SS Init Data for Active Mode*/
#define LOAD_CX_SS_TOUCH_IDLE				0x13	/*Load SS Init Data for Low Power Mode*/
#define LOAD_CX_MS_KEY						0x14	/*Load MS Init Data for Key*/
#define LOAD_CX_SS_KEY						0x15	/*Load SS Init Data for Key*/
#define LOAD_CX_MS_FORCE					0x16	/*Load MS Init Data for Force*/
#define LOAD_CX_SS_FORCE					0x17	/*Load SS Init Data for Force*/
#define LOAD_CX_SS_HOVER					0x18	/*Load SS Hover Init Data for* Force */
#define LOAD_SYNC_FRAME_RAW					0x30	/*Load a Synchronized Raw Frame*/
#define LOAD_SYNC_FRAME_FILTER				0x31	/*Load a Synchronized Filter Frame*/
#define LOAD_SYNC_FRAME_STRENGTH			0x33	/*Load a Synchronized Strength Frame*/
#define LOAD_SYNC_FRAME_BASELINE			0x32	/*Load a Synchronized Baseline Frame*/
#define LOAD_PANEL_CX_TOT_MS_TOUCH			0x50	/*Load TOT MS Init Data for Active Mode*/
#define LOAD_PANEL_CX_TOT_MS_LOW_POWER		0x51	/*Load TOT MS Init Data for Low Power Mode*/
#define LOAD_PANEL_CX_TOT_SS_TOUCH			0x52	/*Load TOT SS Init Data for Active Mode*/
#define LOAD_PANEL_CX_TOT_SS_TOUCH_IDLE		0x53	/*Load TOT SS Init Data for Low Power Mode*/
#define LOAD_PANEL_CX_TOT_MS_KEY			0x54	/*Load TOT MS Init Data for Key*/
#define LOAD_PANEL_CX_TOT_SS_KEY			0x55	/*Load TOT SS Init Data for Key*/
#define LOAD_PANEL_CX_TOT_MS_FORCE			0x56	/*Load TOT MS Init Data for Force*/
#define LOAD_PANEL_CX_TOT_SS_FORCE			0x57	/*Load TOT SS Init Data for Force*/
#define STAPI_HOST_DATA_ID_PANEL_CX_SS_HVR 	0x58		/*Load HOVER TOT SS Init Data for Activity*/

/** @}*/

#define	SPECIAL_TUNING_LP_TIMER				0x01	/*Perform LP Timer calibration*/
#define SPECIAL_TUNING_IOFF					0x02	/*Perform Ioff calibration*/

/** @}*/

/** @defgroup events_group	 FW Event IDs and Types
* Event IDs and Types pushed by the FW into the FIFO
* @{
*/
#define EVT_ID_NOEVENT						0x00	/*No Events*/
#define EVT_ID_CONTROLLER_READY				0x03	/*Controller ready, issued after a system reset.*/
#define EVT_ID_ENTER_POINT					0x13	/*Touch enter in the sensing area*/
#define EVT_ID_MOTION_POINT					0x23	/*Touch motion (a specific touch changed position)*/
#define EVT_ID_LEAVE_POINT					0x33	/*Touch leave the sensing area*/
#define EVT_ID_STATUS_UPDATE				0x43	/*FW report a system condition change*/
#define EVT_ID_USER_REPORT					0x53	/*User related events triggered (keys, gestures, proximity etc)*/
#define EVT_ID_DEBUG						0xE3	/*Debug Info*/
#define EVT_ID_ERROR						0xF3	/*Error Event*/

#define NUM_EVT_ID							(((EVT_ID_ERROR & 0xF0) >> 4) + 1)	/*Max number of unique event IDs supported*/
/** @}*/

/** @defgroup status_type	 Status Event Types
* @ingroup events_group
* Types of EVT_ID_STATUS_UPDATE events
* @{
*/
#define EVT_TYPE_STATUS_ECHO				0x01	/*Echo event, contain the first 5 bytes of the FW command sent*/
#define EVT_TYPE_STATUS_FRAME_DROP			0x03	/*Some frame was skipped during the elaboration*/
#define EVT_TYPE_STATUS_FORCE_CAL			0x05	/*Force Calibration has triggered*/
#define EVT_TYPE_STATUS_WATER				0x06	/*Water Mode*/
#define EVT_TYPE_STATUS_SS_RAW_SAT			0x07	/*Self Sense data saturated*/
#define EVT_TYPE_STATUS_POCKET				0x0A
/** @} */

/** @defgroup user_type	 User Event Types
* @ingroup events_group
* Types of EVT_ID_USER_REPORT events generated by thw FW
* @{
*/
#define EVT_TYPE_USER_KEY					0x00	/*Keys pressed/relesed event report*/
#define EVT_TYPE_USER_PROXIMITY				0x01	/*Proximity detection event report*/
#define EVT_TYPE_USER_GESTURE				0x02	/*Gesture detection event report*/
#define EVT_TYPE_USER_EARDET				0x03	/*ear detection event report*/

/** @}*/

/** @defgroup error_type  Error Event Types
* @ingroup events_group
* Types of EVT_ID_ERROR events reported by the FW
* @{
*/
#define EVT_TYPE_ERROR_WATCHDOG				0x06	/*Watchdog timer expired*/

#define EVT_TYPE_ERROR_CRC_CFG_HEAD			0x20	/*CRC error in the Config Area Header*/
#define EVT_TYPE_ERROR_CRC_CFG				0x21	/*CRC error in the Config Area*/
#define EVT_TYPE_ERROR_CRC_PANEL_HEAD		0x22	/*CRC error in the Panel Area Header*/
#define EVT_TYPE_ERROR_CRC_PANEL			0x23	/*CRC error in the Panel Area*/

#define EVT_TYPE_ERROR_ITO_FORCETOGND		0x60	/*Force channel/s short to ground*/
#define EVT_TYPE_ERROR_ITO_SENSETOGND		0x61	/*Sense channel/s short to ground*/
#define EVT_TYPE_ERROR_ITO_FORCETOVDD		0x62	/*Force channel/s short to VDD*/
#define EVT_TYPE_ERROR_ITO_SENSETOVDD		0x63	/*Sense channel/s short to VDD*/
#define EVT_TYPE_ERROR_ITO_FORCE_P2P		0x64	/*Pin to Pin short Force channel/s*/
#define EVT_TYPE_ERROR_ITO_SENSE_P2P		0x65	/*Pin to Pin short Sense channel/s*/
#define EVT_TYPE_ERROR_ITO_FORCEOPEN		0x66	/*Force Panel open*/
#define EVT_TYPE_ERROR_ITO_SENSEOPEN		0x67	/*Sense Panel open*/
#define EVT_TYPE_ERROR_ITO_KEYOPEN			0x68	/*Key open*/

#define EVT_TYPE_ERROR_CRC_CX_HEAD			0xA0	/*CRC error in the CX Area Header*/
#define EVT_TYPE_ERROR_CRC_CX				0xA1	/*CRC error in the CX Area*/
#define EVT_TYPE_ERROR_CRC_CX_SUB_HEAD		0xA5	/*CRC error in the CX Subsection Area Header*/
#define EVT_TYPE_ERROR_CRC_CX_SUB			0xA6	/*CRC error in the CX Subsection Area*/

#define EVT_TYPE_ERROR_ESD					0xF0	/*ESD error*/
#define EVT_TYPE_ERROR_M3						0x02	/*ic M3 run error*/

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
#define ADDR_CONFIG_ID						0x0010	/*Starting Address of the config ID*/
#define CONFIG_ID_BYTE						2		/*Number of bytes of config ID*/
#define ADDR_CONFIG_SENSE_LEN				0x0030	/*Address where is stored the number of sense channels*/
#define ADDR_CONFIG_FORCE_LEN				0x0031	/*Address where is stored the number of force channels*/
/** @}*/

/** @}*/

#define ERROR_DUMP_ROW_SIZE					32		/*number of rows of the error memory*/
#define ERROR_DUMP_COL_SIZE					4		/*number of bytes for each row of the error memory*/
#define ERROR_DUMP_SIGNATURE				0xFA5005AF	/*first row signature of a proper dump*/

#define TOUCH_TYPE_INVALID					0x00	/*Invalid touch type*/
#define TOUCH_TYPE_FINGER					0x01	/*Finger touch*/
#define TOUCH_TYPE_GLOVE					0x02	/*Glove touch*/
#define TOUCH_TYPE_STYLUS					0x03	/*Stylus touch*/
#define TOUCH_TYPE_PALM						0x04	/*Palm touch*/
#define TOUCH_TYPE_HOVER					0x05	/*Hovering touch*/

#define FTS_KEY_0							0x01	/*Key 0 bit*/
#define FTS_KEY_1							0x02	/*Key 1 bit*/
#define FTS_KEY_2							0x04	/*Key 2 bit*/
#define FTS_KEY_3							0x08	/*Key 3 bit*/
#define FTS_KEY_4							0x10	/*Key 4 bit*/
#define FTS_KEY_5							0x20	/*Key 5 bit*/
#define FTS_KEY_6							0x40	/*Key 6 bit*/
#define FTS_KEY_7							0x80	/*Key 7 bit*/

#endif
