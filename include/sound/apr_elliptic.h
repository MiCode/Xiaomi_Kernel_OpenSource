#pragma once

#include <linux/types.h>
#include <sound/apr_audio-v2.h>
#include <linux/delay.h>

/* Elliptic Labs UltraSound Module */
#define ELLIPTIC_ULTRASOUND_DISABLE			0
#define ELLIPTIC_ULTRASOUND_ENABLE			1
#define ELLIPTIC_ULTRASOUND_SET_PARAMS			2
#define ELLIPTIC_ULTRASOUND_GET_PARAMS			3
#define ELLIPTIC_ULTRASOUND_RAMP_DOWN			4

/** Param ID definition */
#define ELLIPTIC_ULTRASOUND_PARAM_ID_UPS_DATA            3
#define ELLIPTIC_ULTRASOUND_PARAM_ID_CALIBRATION_DATA   11
#define ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_VERSION     12

#define ELLIPTIC_ENABLE_APR_SIZE			16
#define ELLIPTIC_COFIG_SET_PARAM_SIZE			96

#define ELLIPTIC_ULTRASOUND_MODULE_TX			0x0F010201
#define ELLIPTIC_ULTRASOUND_MODULE_RX			0x0FF10202
#define ULTRASOUND_OPCODE				0x0FF10204

#define ELLIPTIC_DATA_READ_BUSY				0
#define ELLIPTIC_DATA_READ_OK				1
#define ELLIPTIC_DATA_READ_CANCEL			2

/** 512 byte APR payload */
#define ELLIPTIC_GET_PARAMS_SIZE			128
/** System config size is 96 bytes */
#define ELLIPTIC_SET_PARAMS_SIZE			128

/** register */
#define ELLIPTIC_SYSTEM_CONFIGURATION			0
/** bits */
#define ELLIPTIC_SYSTEM_CONFIGURATION_LATENCY		0
#define ELLIPTIC_SYSTEM_CONFIGURATION_SENSITIVITY	1
#define ELLIPTIC_SYSTEM_CONFIGURATION_SPEAKER_SCALING	2
#define ELLIPTIC_SYSTEM_CONFIGURATION_MICROPHONE_INDEX	3
#define ELLIPTIC_SYSTEM_CONFIGURATION_MODE		4
#define ELLIPTIC_SYSTEM_CONFIGURATION_ADAPTIVE_REFS	5

#define ELLIPTIC_CALIBRATION				1
#define ELLIPTIC_CALIBRATION_STATE			0
#define ELLIPTIC_CALIBRATION_PROFILE		1
#define ELLIPTIC_ULTRASOUND_GAIN			2

#define ELLIPTIC_SYSTEM_CONFIGURATION_SIZE		96
#define ELLIPTIC_CALIBRATION_DATA_SIZE          64
#define ELLIPTIC_VERSION_INFO_SIZE              16

#define ELLIPTIC_PORT_ID				SLIMBUS_1_TX

/** Sequence of Elliptic Labs Ultrasound module parameters */
struct afe_ultrasound_set_params_t {
	uint32_t  payload[ELLIPTIC_SET_PARAMS_SIZE];
} __packed;

struct afe_ultrasound_config_command {
	struct apr_hdr                      hdr;
	struct afe_port_cmd_set_param_v2    param;
	struct afe_port_param_data_v2       pdata;
	struct afe_ultrasound_set_params_t  prot_config;
} __packed;

/** Sequence of Elliptic Labs Ultrasound module parameters */
struct afe_ultrasound_get_params_t {
	uint32_t payload[ELLIPTIC_GET_PARAMS_SIZE];
} __packed;

struct afe_ultrasound_get_calib {
	struct afe_port_cmd_get_param_v2   get_param;
	struct afe_port_param_data_v2      pdata;
	struct afe_ultrasound_get_params_t res_cfg;
} __packed;

struct afe_ultrasound_calib_get_resp {
	struct afe_ultrasound_get_params_t res_cfg;
} __packed;

/** Elliptic APR public  */
int elliptic_data_io(uint32_t filter_set, uint32_t elliptic_port_id,
		char *buff, size_t length);
int elliptic_data_io_cancel(void);

/** Elliptic APR private */
extern volatile uint32_t elliptic_engine_started;
int afe_ultrasound_get_calib_data(struct afe_ultrasound_get_calib *calib_resp,
				  uint32_t param_id, uint32_t module_id);
int afe_ultrasound_set_calib_data(int port,
			  int param_id,
			  int module_id,
			  struct afe_ultrasound_set_params_t *prot_config,
			  uint32_t length);

