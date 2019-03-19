/**
 * Elliptic Labs
 */

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <sound/asound.h>
#include <sound/soc.h>
#include <sound/control.h>
#include "msm-pcm-routing-v2.h"
#include <sound/q6audio-v2.h>
#include <sound/q6common.h>
#include <sound/apr_audio-v2.h>
#include <sound/apr_elliptic.h>
#include <elliptic/elliptic_mixer_controls.h>
#include <elliptic/elliptic_data_io.h>

#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

enum {
	HALL_SLIDER_UP = 4,
	HALL_SLIDER_DOWN = 5,
	HALL_SLIDING = 6,
};

enum driver_sensor_type {
	DRIVER_SENSOR_HALL = 35,
};

struct driver_sensor_event {
	enum driver_sensor_type type;
	union {
		int32_t event;
		int32_t reserved[2];
	};
};



static int afe_set_parameter(int port,
		int param_id,
		int module_id,
		struct afe_ultrasound_set_params_t *prot_config,
		uint32_t length)
{
	struct afe_port_cmd_set_param_v2 *set_param_v2 = NULL;
	uint32_t set_param_v2_size = sizeof(struct afe_port_cmd_set_param_v2);
	struct afe_port_cmd_set_param_v3 *set_param_v3 = NULL;
	uint32_t set_param_v3_size = sizeof(struct afe_port_cmd_set_param_v3);
	struct param_hdr_v3 param_hdr = {0};
	u16 port_id = 0;
	int index = 0;
	u8 *packed_param_data = NULL;
	int packed_data_size = sizeof(union param_hdrs) + length;
	int ret = 0;

	pr_debug("[ELUS]: inside %s\n", __func__);

	port_id = q6audio_get_port_id(port);
	ret = q6audio_validate_port(port_id);
	if (ret < 0) {
		pr_err("%s: Not a valid port id = 0x%x ret %d\n", __func__,
		       port_id, ret);
		return -EINVAL;
	}
	index = q6audio_get_port_index(port);

	param_hdr.module_id = module_id;
	param_hdr.instance_id = INSTANCE_ID_0;
	param_hdr.param_id = param_id;
	param_hdr.param_size = length;
	pr_debug("[ELUS]: param_size %d\n", length);

	packed_param_data = kzalloc(packed_data_size, GFP_KERNEL);
	if (packed_param_data == NULL)
		return -ENOMEM;

	ret = q6common_pack_pp_params(packed_param_data, &param_hdr, (u8 *)prot_config,
				      &packed_data_size);
	if (ret) {
		pr_err("%s: Failed to pack param header and data, error %d\n",
		       __func__, ret);
		goto fail_cmd;
	}

	if (q6common_is_instance_id_supported()) {
		set_param_v3_size += packed_data_size;
		set_param_v3 = kzalloc(set_param_v3_size, GFP_KERNEL);
		if (set_param_v3 == NULL) {
			ret = -ENOMEM;
			goto fail_cmd;
		}

		set_param_v3->apr_hdr.hdr_field =
			APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
					APR_PKT_VER);
		set_param_v3->apr_hdr.pkt_size = sizeof(struct afe_port_cmd_set_param_v3) +
											packed_data_size;
		set_param_v3->apr_hdr.src_port = 0;
		set_param_v3->apr_hdr.dest_port = 0;
		set_param_v3->apr_hdr.token = index;
		set_param_v3->apr_hdr.opcode = AFE_PORT_CMD_SET_PARAM_V3;
		set_param_v3->port_id = port_id;
		set_param_v3->payload_size = packed_data_size;
		memcpy(&set_param_v3->param_data, packed_param_data,
			       packed_data_size);

		atomic_set(elus_afe.ptr_state, 1);
		ret = apr_send_pkt(*elus_afe.ptr_apr, (uint32_t *) set_param_v3);
	} else {
		set_param_v2_size += packed_data_size;
		set_param_v2 = kzalloc(set_param_v2_size, GFP_KERNEL);
		if (set_param_v2 == NULL) {
			ret = -ENOMEM;
			goto fail_cmd;
		}

		set_param_v2->apr_hdr.hdr_field =
			APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
				      APR_PKT_VER);
		set_param_v2->apr_hdr.pkt_size = sizeof(struct afe_port_cmd_set_param_v2) +
											packed_data_size;
		set_param_v2->apr_hdr.src_port = 0;
		set_param_v2->apr_hdr.dest_port = 0;
		set_param_v2->apr_hdr.token = index;
		set_param_v2->apr_hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
		set_param_v2->port_id = port_id;
		set_param_v2->payload_size = packed_data_size;
		memcpy(&set_param_v2->param_data, packed_param_data,
			       packed_data_size);

		atomic_set(elus_afe.ptr_state, 1);
		ret = apr_send_pkt(*elus_afe.ptr_apr, (uint32_t *) set_param_v2);
	}
	if (ret < 0) {
		pr_err("%s: Setting param for port %d param[0x%x]failed\n",
			   __func__, port, param_id);
		goto fail_cmd;
	}
	ret = wait_event_timeout(elus_afe.ptr_wait[index],
		(atomic_read(elus_afe.ptr_state) == 0),
		msecs_to_jiffies(elus_afe.timeout_ms*10));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(elus_afe.ptr_status) != 0) {
		pr_err("%s: set param cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = 0;
fail_cmd:
	pr_debug("%s param_id %x status %d\n", __func__, param_id, ret);
	kfree(set_param_v2);
	kfree(set_param_v3);
	kfree(packed_param_data);
	return ret;
}


int32_t ultrasound_apr_set_parameter(int32_t port_id, uint32_t param_id,
	u8 *user_params, int32_t length) {

	int32_t  ret = 0;
	uint32_t module_id;

	if (port_id == ELLIPTIC_PORT_ID)
		module_id = ELLIPTIC_ULTRASOUND_MODULE_TX;
	else
		module_id = ELLIPTIC_ULTRASOUND_MODULE_RX;

	ret = afe_set_parameter(port_id,
		param_id, module_id,
		(struct afe_ultrasound_set_params_t *)user_params,
		length);

	return ret;
}

static int32_t process_version_msg(uint32_t *payload, uint32_t payload_size)
{
	struct elliptic_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[ELUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= ELLIPTIC_VERSION_INFO_SIZE) {
		pr_debug("[ELUS]: elliptic_version copied to local AP cache");
		data_block =
		elliptic_get_shared_obj(
			ELLIPTIC_OBJ_ID_VERSION_INFO);
		copy_size = min_t(size_t, data_block->size,
			(size_t)ELLIPTIC_VERSION_INFO_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_branch_msg(uint32_t *payload, uint32_t payload_size)
{
	struct elliptic_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[ELUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= ELLIPTIC_BRANCH_INFO_SIZE) {
		pr_debug("[ELUS]: elliptic_branch copied to local AP cache");
		data_block =
		elliptic_get_shared_obj(
			ELLIPTIC_OBJ_ID_BRANCH_INFO);
		copy_size = min_t(size_t, data_block->size,
			(size_t)ELLIPTIC_BRANCH_INFO_MAX_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_tag_msg(uint32_t *payload, uint32_t payload_size)
{
	struct elliptic_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[ELUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= ELLIPTIC_TAG_INFO_SIZE) {
		pr_debug("[ELUS]: elliptic_tag copied to local AP cache");
		data_block =
		elliptic_get_shared_obj(
			ELLIPTIC_OBJ_ID_TAG_INFO);
		copy_size = min_t(size_t, data_block->size,
			(size_t)ELLIPTIC_TAG_INFO_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_calibration_msg(uint32_t *payload, uint32_t payload_size)
{
	struct elliptic_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[ELUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= ELLIPTIC_CALIBRATION_DATA_SIZE) {
		pr_debug("[ELUS]: calibration_data copied to local AP cache");

		data_block = elliptic_get_shared_obj(
			ELLIPTIC_OBJ_ID_CALIBRATION_DATA);
		copy_size = min_t(size_t, data_block->size,
			(size_t)ELLIPTIC_CALIBRATION_DATA_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		elliptic_set_calibration_data((u8 *)&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_calibration_v2_msg(uint32_t *payload, uint32_t payload_size)
{
	struct elliptic_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[ELUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= ELLIPTIC_CALIBRATION_V2_DATA_SIZE) {
		pr_debug("[ELUS]: calibration_data copied to local AP cache");

		data_block = elliptic_get_shared_obj(
			ELLIPTIC_OBJ_ID_CALIBRATION_V2_DATA);
		copy_size = min_t(size_t, data_block->size,
			(size_t)ELLIPTIC_CALIBRATION_V2_DATA_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		elliptic_set_calibration_data((u8 *)&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_ml_msg(uint32_t *payload, uint32_t payload_size)
{
	struct elliptic_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[ELUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= ELLIPTIC_ML_DATA_SIZE) {
		pr_debug("[ELUS]: ml_data copied to local AP cache");

		data_block = elliptic_get_shared_obj(
			ELLIPTIC_OBJ_ID_ML_DATA);
		copy_size = min_t(size_t, data_block->size,
			(size_t)ELLIPTIC_ML_DATA_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_diagnostics_msg(uint32_t *payload, uint32_t payload_size)
{
	struct elliptic_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[ELUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= ELLIPTIC_DIAGNOSTICS_DATA_SIZE) {
		pr_debug("[ELUS]: diagnostics_data copied to local AP cache");

		data_block = elliptic_get_shared_obj(
			ELLIPTIC_OBJ_ID_DIAGNOSTICS_DATA);
		copy_size = min_t(size_t, data_block->size,
			(size_t)ELLIPTIC_DIAGNOSTICS_DATA_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_sensorhub_msg(uint32_t *payload, uint32_t payload_size)
{
	int32_t  ret = 0;

	pr_err("[ELUS]: %s, paramId:%u, size:%d\n",
			__func__, payload[1], payload_size);

	return ret;
}

int32_t elliptic_process_apr_payload(uint32_t *payload)
{
	uint32_t payload_size = 0;
	int32_t  ret = -1;

	if (payload[0] == ELLIPTIC_ULTRASOUND_MODULE_TX) {
		/* payload format
		*   payload[0] = Module ID
		*   payload[1] = Param ID
		*   payload[2] = LSB - payload size
		*		MSB - reserved(TBD)
		*   payload[3] = US data payload starts from here
		*/
		payload_size = payload[2] & 0xFFFF;

		switch (payload[1]) {
		case ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_VERSION:
			ret = process_version_msg(payload, payload_size);
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_BUILD_BRANCH:
			ret = process_branch_msg(payload, payload_size);
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_TAG:
			ret = process_tag_msg(payload, payload_size);
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_CALIBRATION_DATA:
			ret = process_calibration_msg(payload, payload_size);
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_CALIBRATION_V2_DATA:
			ret = process_calibration_v2_msg(payload, payload_size);
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_ML_DATA:
			ret = process_ml_msg(payload, payload_size);
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_DIAGNOSTICS_DATA:
			ret = process_diagnostics_msg(payload, payload_size);
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_SENSORHUB:
			ret = process_sensorhub_msg(payload, payload_size);
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_ENGINE_DATA:
			ret = elliptic_data_push(
				ELLIPTIC_ALL_DEVICES,
				(const char *)&payload[3],
				(size_t)payload_size,
				ELLIPTIC_DATA_PUSH_FROM_KERNEL);

			if (ret != 0) {
				pr_err("[ELUS] : failed to push apr payload to elliptic device");
				return ret;
			}
			ret = payload_size;
			break;
		default:
			{
				pr_err("[ELUS] : elliptic_process_apr_payload, Illegal paramId:%u", payload[1]);
			}
			break;
		}
	} else {
		pr_debug("[ELUS]: Invalid Ultrasound Module ID %d\n",
			payload[0]);
	}
	return ret;
}

int elliptic_set_hall_state(int state)
{
	struct driver_sensor_event dse;
	int ret = -1;

	dse.type = DRIVER_SENSOR_HALL;

	switch (state) {
		case 0:
			dse.event = HALL_SLIDER_UP;
		break;
		case 1:
			dse.event = HALL_SLIDER_DOWN;
		break;
		case 2:
			dse.event = HALL_SLIDING;
		break;
		default:
			pr_err("%s Invalid HALL state:%d\n", __func__, state);
		return ret;
	}

	ret = afe_set_parameter(ELLIPTIC_PORT_ID,
		2, ELLIPTIC_ULTRASOUND_MODULE_TX,
		(struct afe_ultrasound_set_params_t *)&dse,
		sizeof(dse));
	return ret;
}
EXPORT_SYMBOL(elliptic_set_hall_state);
