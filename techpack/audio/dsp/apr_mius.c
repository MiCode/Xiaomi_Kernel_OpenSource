/**
 * Mi
 */

#include <linux/slab.h>
#include <linux/wait.h>
#include <linux/jiffies.h>
#include <sound/asound.h>
#include <sound/soc.h>
#include <sound/control.h>
#include "../asoc/msm-pcm-routing-v2.h"
#include <dsp/q6audio-v2.h>
#include <dsp/q6common.h>
#include <dsp/apr_audio-v2.h>
#include <dsp/apr_mius.h>
#include <mius/mius_mixer_controls.h>
#include <mius/mius_data_io.h>

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
		struct afe_mi_ultrasound_set_params_t *prot_config,
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

	pr_debug("[MIUS]: inside %s\n", __func__);

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
	pr_debug("[MIUS]: param_size %d\n", length);

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

		atomic_set(mius_afe.ptr_state, 1);
		ret = apr_send_pkt(*mius_afe.ptr_apr, (uint32_t *) set_param_v3);
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

		atomic_set(mius_afe.ptr_state, 1);
		ret = apr_send_pkt(*mius_afe.ptr_apr, (uint32_t *) set_param_v2);
	}
	if (ret < 0) {
		pr_err("%s: Setting param for port %d param[0x%x]failed\n",
			   __func__, port, param_id);
		goto fail_cmd;
	}
	ret = wait_event_timeout(mius_afe.ptr_wait[index],
		(atomic_read(mius_afe.ptr_state) == 0),
		msecs_to_jiffies(mius_afe.timeout_ms*10));
	if (!ret) {
		pr_err("%s: wait_event timeout\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	if (atomic_read(mius_afe.ptr_status) != 0) {
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


int32_t mi_ultrasound_apr_set_parameter(int32_t port_id, uint32_t param_id,
	u8 *user_params, int32_t length)
{
	int32_t  ret = 0;
	uint32_t module_id;

	if (port_id == MIUS_PORT_ID)
		module_id = MIUS_ULTRASOUND_MODULE_TX;
	else
		module_id = MIUS_ULTRASOUND_MODULE_RX;

	ret = afe_set_parameter(port_id,
		param_id, module_id,
		(struct afe_mi_ultrasound_set_params_t *)user_params,
		length);

	return ret;
}

#if 0
static int32_t process_version_msg(uint32_t *payload, uint32_t payload_size)
{
	struct mius_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[MIUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= MIUS_VERSION_INFO_SIZE) {
		pr_debug("[MIUS]: mius_version copied to local AP cache");
		data_block =
		mius_get_shared_obj(
			MIUS_OBJ_ID_VERSION_INFO);
		copy_size = min_t(size_t, data_block->size,
			(size_t)MIUS_VERSION_INFO_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_branch_msg(uint32_t *payload, uint32_t payload_size)
{
	struct mius_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[MIUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= MIUS_BRANCH_INFO_SIZE) {
		pr_debug("[MIUS]: mius_branch copied to local AP cache");
		data_block =
		mius_get_shared_obj(
			MIUS_OBJ_ID_BRANCH_INFO);
		copy_size = min_t(size_t, data_block->size,
			(size_t)MIUS_BRANCH_INFO_MAX_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_tag_msg(uint32_t *payload, uint32_t payload_size)
{
	struct mius_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[MIUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= MIUS_TAG_INFO_SIZE) {
		pr_debug("[MIUS]: mius_tag copied to local AP cache");
		data_block =
		mius_get_shared_obj(
			MIUS_OBJ_ID_TAG_INFO);
		copy_size = min_t(size_t, data_block->size,
			(size_t)MIUS_TAG_INFO_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_calibration_msg(uint32_t *payload, uint32_t payload_size)
{
	struct mius_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[MIUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= MIUS_CALIBRATION_DATA_SIZE) {
		pr_debug("[MIUS]: calibration_data copied to local AP cache");

		data_block = mius_get_shared_obj(
			MIUS_OBJ_ID_CALIBRATION_DATA);
		copy_size = min_t(size_t, data_block->size,
			(size_t)MIUS_CALIBRATION_DATA_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		mius_set_calibration_data((u8 *)&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_calibration_v2_msg(uint32_t *payload, uint32_t payload_size)
{
	struct mius_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[MIUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= MIUS_CALIBRATION_V2_DATA_SIZE) {
		pr_debug("[MIUS]: calibration_data copied to local AP cache");

		data_block = mius_get_shared_obj(
			MIUS_OBJ_ID_CALIBRATION_V2_DATA);
		copy_size = min_t(size_t, data_block->size,
			(size_t)MIUS_CALIBRATION_V2_DATA_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		mius_set_calibration_data((u8 *)&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_ml_msg(uint32_t *payload, uint32_t payload_size)
{
	struct mius_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[MIUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= MIUS_ML_DATA_SIZE) {
		pr_debug("[MIUS]: ml_data copied to local AP cache");

		data_block = mius_get_shared_obj(
			MIUS_OBJ_ID_ML_DATA);
		copy_size = min_t(size_t, data_block->size,
			(size_t)MIUS_ML_DATA_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_diagnostics_msg(uint32_t *payload, uint32_t payload_size)
{
	struct mius_shared_data_block *data_block = NULL;
	size_t copy_size = 0;
	int32_t  ret = -1;

	pr_err("[MIUS]: %s() size:%d\n", __func__, payload_size);

	if (payload_size >= MIUS_DIAGNOSTICS_DATA_SIZE) {
		pr_debug("[MIUS]: diagnostics_data copied to local AP cache");

		data_block = mius_get_shared_obj(
			MIUS_OBJ_ID_DIAGNOSTICS_DATA);
		copy_size = min_t(size_t, data_block->size,
			(size_t)MIUS_DIAGNOSTICS_DATA_SIZE);

		memcpy((u8 *)data_block->buffer,
			&payload[3], copy_size);
		ret = (int32_t)copy_size;
	}
	return ret;
}

static int32_t process_sensorhub_msg(uint32_t *payload, uint32_t payload_size)
{
	int32_t  ret = 0;

	pr_err("[MIUS]: %s, paramId:%u, size:%d\n",
			__func__, payload[1], payload_size);

	return ret;
}

#endif

extern int us_afe_callback(int data);
static int ups_event;

int32_t mius_process_apr_payload(uint32_t *payload)
{
	uint32_t payload_size = 0;
	int32_t  ret = -1;

	//if (payload[0] == MIUS_ULTRASOUND_MODULE_TX) {
	if (true) {
		/* payload format
		*   payload[0] = Module ID
		*   payload[1] = Param ID
		*   payload[2] = LSB - payload size
		*		MSB - reserved(TBD)
		*   payload[3] = US data payload starts from here
		*/
		payload_size = payload[2] & 0xFFFF;
#if 0
		switch (payload[1]) {
		case MIUS_ULTRASOUND_PARAM_ID_ENGINE_VERSION:
			ret = process_version_msg(payload, payload_size);
			break;
		case MIUS_ULTRASOUND_PARAM_ID_BUILD_BRANCH:
			ret = process_branch_msg(payload, payload_size);
			break;
		case MIUS_ULTRASOUND_PARAM_ID_TAG:
			ret = process_tag_msg(payload, payload_size);
			break;
		case MIUS_ULTRASOUND_PARAM_ID_CALIBRATION_DATA:
			ret = process_calibration_msg(payload, payload_size);
			break;
		case MIUS_ULTRASOUND_PARAM_ID_CALIBRATION_V2_DATA:
			ret = process_calibration_v2_msg(payload, payload_size);
			break;
		case MIUS_ULTRASOUND_PARAM_ID_ML_DATA:
			ret = process_ml_msg(payload, payload_size);
			break;
		case MIUS_ULTRASOUND_PARAM_ID_DIAGNOSTICS_DATA:
			ret = process_diagnostics_msg(payload, payload_size);
			break;
		case MIUS_ULTRASOUND_PARAM_ID_SENSORHUB:
			ret = process_sensorhub_msg(payload, payload_size);
			break;
		case MIUS_ULTRASOUND_PARAM_ID_ENGINE_DATA:
#endif
			printk(KERN_DEBUG "[MIUS] mi us payload[3] = %d", (int)payload[3]);
			if (payload[3] == 0 || payload[3] == 1) {
				ups_event = payload[3];
				ret = (int32_t)us_afe_callback((const uint32_t)payload[3]);
			} else {

				ups_event = ups_event ^ 1;
				printk(KERN_DEBUG "[MIUS] >> change ups to %d", ups_event);
				ret = (int32_t)us_afe_callback((uint32_t)ups_event);
			}

			if (ret != 0) {
				pr_err("[MIUS] : failed to push apr payload to mius device");
				return ret;
			}
			ret = payload_size;
#if 0
			break;
		default:
			{
				pr_err("[MIUS] : mius_process_apr_payload, Illegal paramId:%u", payload[1]);
			}
			break;
		}
#endif
	} else {
		pr_debug("[MIUS]: Invalid Ultrasound Module ID %d\n",
			payload[0]);
	}
	return ret;
}

int mius_set_hall_state(int state)
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

	ret = afe_set_parameter(MIUS_PORT_ID,
		2, MIUS_ULTRASOUND_MODULE_TX,
		(struct afe_mi_ultrasound_set_params_t *)&dse,
		sizeof(dse));
	return ret;
}
EXPORT_SYMBOL(mius_set_hall_state);
