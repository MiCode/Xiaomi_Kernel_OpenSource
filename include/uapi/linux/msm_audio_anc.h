#ifndef _UAPI_MSM_AUDIO_ANC_H
#define _UAPI_MSM_AUDIO_ANC_H

#include <linux/types.h>
#include <linux/ioctl.h>

#define ANC_IOCTL_MAGIC 'a'

#define AUDIO_ANC_SET_PARAM		_IOWR(ANC_IOCTL_MAGIC, \
		300, struct audio_anc_packet *)
#define AUDIO_ANC_GET_PARAM		_IOWR(ANC_IOCTL_MAGIC, \
		301, struct audio_anc_packet *)

#define ANC_CMD_START   0
#define ANC_CMD_STOP   1
#define ANC_CMD_RPM    2
#define ANC_CMD_BYPASS_MODE    3
#define ANC_CMD_ALGO_MODULE    4
#define ANC_CMD_ALGO_CALIBRATION    5

/* room for ANC_CMD define extend */
#define ANC_CMD_MAX   0xFF

#define ANC_CALIBRATION_PAYLOAD_SIZE_MAX   100

struct audio_anc_header {
	int32_t data_size;
	int32_t version;
	int32_t anc_cmd;
	int32_t anc_cmd_size;
};

struct audio_anc_rpm_info {
	int32_t rpm;
};

struct audio_anc_bypass_mode {
	int32_t mode;
};
struct audio_anc_algo_module_info {
	int32_t module_id;
};

struct audio_anc_algo_calibration_header {
	uint32_t module_id;
	uint32_t param_id;
	uint32_t payload_size;
};

struct audio_anc_algo_calibration_body {
	int32_t payload[ANC_CALIBRATION_PAYLOAD_SIZE_MAX];
};

struct audio_anc_algo_calibration_info {
	struct audio_anc_algo_calibration_header cali_header;
	struct audio_anc_algo_calibration_body cali_body;
};

union  audio_anc_data {
	struct audio_anc_rpm_info rpm_info;
	struct audio_anc_bypass_mode bypass_mode_info;
	struct audio_anc_algo_module_info algo_info;
	struct audio_anc_algo_calibration_info algo_cali_info;
};

struct audio_anc_packet {
	struct audio_anc_header hdr;
	union  audio_anc_data anc_data;
};

#endif /* _UAPI_MSM_AUDIO_ANC_H */
