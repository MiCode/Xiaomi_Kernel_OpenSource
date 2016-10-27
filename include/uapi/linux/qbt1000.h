#ifndef _UAPI_QBT1000_H_
#define _UAPI_QBT1000_H_

#define MAX_NAME_SIZE					 32

/*
* enum qbt1000_commands -
*      enumeration of command options
* @QBT1000_LOAD_APP - cmd loads TZ app
* @QBT1000_UNLOAD_APP - cmd unloads TZ app
* @QBT1000_SEND_TZCMD - sends cmd to TZ app
* @QBT1000_SET_FINGER_DETECT_KEY - sets the input key to send on finger detect
* @QBT1000_CONFIGURE_POWER_KEY - enables/disables sending the power key on
	finger down events
*/
enum qbt1000_commands {
	QBT1000_LOAD_APP = 100,
	QBT1000_UNLOAD_APP = 101,
	QBT1000_SEND_TZCMD = 102,
	QBT1000_SET_FINGER_DETECT_KEY = 103,
	QBT1000_CONFIGURE_POWER_KEY = 104
};

/*
* enum qbt1000_fw_event -
*      enumeration of firmware events
* @FW_EVENT_FINGER_DOWN - finger down detected
* @FW_EVENT_FINGER_UP - finger up detected
* @FW_EVENT_INDICATION - an indication IPC from the firmware is pending
*/
enum qbt1000_fw_event {
	FW_EVENT_FINGER_DOWN = 1,
	FW_EVENT_FINGER_UP = 2,
	FW_EVENT_CBGE_REQUIRED = 3,
};

/*
* struct qbt1000_app -
*      used to load and unload apps in TZ
* @app_handle - qseecom handle for clients
* @name - Name of secure app to load
* @size - Size of requested buffer of secure app
* @high_band_width - 1 - for high bandwidth usage
*                    0 - for normal bandwidth usage
*/
struct qbt1000_app {
	struct qseecom_handle **app_handle;
	char name[MAX_NAME_SIZE];
	uint32_t size;
	uint8_t high_band_width;
};

/*
* struct qbt1000_send_tz_cmd -
*      used to cmds to TZ App
* @app_handle - qseecom handle for clients
* @req_buf - Buffer containing request for secure app
* @req_buf_len - Length of request buffer
* @rsp_buf - Buffer containing response from secure app
* @rsp_buf_len - Length of response buffer
*/
struct qbt1000_send_tz_cmd {
	struct qseecom_handle *app_handle;
	uint8_t *req_buf;
	uint32_t req_buf_len;
	uint8_t *rsp_buf;
	uint32_t rsp_buf_len;
};

/*
* struct qbt1000_erie_event -
*      used to receive events from Erie
* @buf - Buffer containing event from Erie
* @buf_len - Length of buffer
*/
struct qbt1000_erie_event {
	uint8_t *buf;
	uint32_t buf_len;
};

/*
* struct qbt1000_set_finger_detect_key -
*      used to configure the input key which is sent on finger down/up event
* @key_code - Key code to send on finger down/up. 0 disables sending key events
*/
struct qbt1000_set_finger_detect_key {
	unsigned int key_code;
};

/*
* struct qbt1000_configure_power_key -
*      used to configure whether the power key is sent on finger down
* @enable - if non-zero, power key is sent on finger down
*/
struct qbt1000_configure_power_key {
	unsigned int enable;
};

#endif /* _UAPI_QBT1000_H_ */
