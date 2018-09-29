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
#include <dsp/q6audio-v2.h>
#include <dsp/apr_audio-v2.h>
#include <asoc/apr_elliptic.h>
#include "msm-elliptic.h"

#include "elliptic/elliptic_data_io.h"
#include "elliptic/elliptic_mixer_controls.h"


#ifndef min
#define min(a, b) (((a) < (b)) ? (a) : (b))
#endif

#if 0
int afe_ultrasound_set_calib_data(int port,
		int param_id,
		int module_id,
		struct afe_ultrasound_set_params_t *prot_config,
		uint32_t length)
{
	int ret = -EINVAL;
	int index = 0;
	struct afe_ultrasound_config_command configV;
	struct afe_ultrasound_config_command *config;

	config = &configV;
	pr_debug("[ELUS]: inside %s\n", __func__);
	memset(config, 0, sizeof(struct afe_ultrasound_config_command));
	if (!prot_config) {
		pr_err("%s Invalid params\n", __func__);
		goto fail_cmd;
	}
	if ((q6audio_validate_port(port) < 0)) {
		pr_err("%s invalid port %d\n", __func__, port);
		goto fail_cmd;
	}
	index = q6audio_get_port_index(port);
	config->pdata.module_id = module_id;
	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						  APR_HDR_LEN(APR_HDR_SIZE),
						  APR_PKT_VER);
	config->hdr.pkt_size = sizeof(struct afe_ultrasound_config_command);
	config->hdr.src_port = 0;
	config->hdr.dest_port = 0;
	config->hdr.token = index;
	config->hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config->param.port_id = q6audio_get_port_id(port);
	config->param.payload_size =
			sizeof(struct afe_ultrasound_config_command) -
			sizeof(config->hdr) - sizeof(config->param);
	config->pdata.param_id = param_id;
	config->pdata.param_size = length;
	pr_debug("[ELUS]: param_size %d\n", length);
	memcpy(&config->prot_config, prot_config,
		   sizeof(struct afe_ultrasound_set_params_t));
	atomic_set(elus_afe.ptr_state, 1);
	ret = apr_send_pkt(*elus_afe.ptr_apr, (uint32_t *) config);
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
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = 0;
fail_cmd:
	pr_debug("%s config->pdata.param_id %x status %d\n",
	__func__, config->pdata.param_id, ret);
	return ret;
}

#endif

int32_t ultrasound_apr_set(int32_t port_id, uint32_t *param_id,
	u8 *user_params, int32_t length) {

	int32_t  ret = 0;
	uint32_t module_id;
	int index = 0;

	struct afe_ultrasound_config_command configV;
	struct afe_ultrasound_config_command *config;
	struct afe_ultrasound_set_params_t *prot_config;

	config = &configV;
	prot_config = &config->prot_config;

	pr_debug("[ELUS]: inside %s\n", __func__);
	memset(config, 0, sizeof(struct afe_ultrasound_config_command));

	if (port_id == ELLIPTIC_PORT_ID)
		module_id = ELLIPTIC_ULTRASOUND_MODULE_TX;
	else
		module_id = ELLIPTIC_ULTRASOUND_MODULE_RX;

	switch (*param_id) {
	/* Elliptic tinymix controls */
	case ELLIPTIC_ULTRASOUND_ENABLE:
	{
		int32_t array[4] = {1, 0, 0, 0};
		memcpy(prot_config, array, sizeof(array));
		length = ELLIPTIC_ENABLE_APR_SIZE;
	}
	break;
	case ELLIPTIC_ULTRASOUND_DISABLE:
	{
		int32_t array[4] = {0, 0, 0, 0};
		memcpy(prot_config, array, sizeof(array));
		length = ELLIPTIC_ENABLE_APR_SIZE;
	}
	break;
	case ELLIPTIC_ULTRASOUND_RAMP_DOWN:
	{
		int32_t array[4] = {-1, 0, 0, 0};
		memcpy(prot_config, array, sizeof(array));
		length = ELLIPTIC_ENABLE_APR_SIZE;
	}
	break;
	case ELLIPTIC_ULTRASOUND_SET_PARAMS:
	{
		if (!user_params) {
			pr_err("%s Invalid params\n", __func__);
			goto fail_cmd;
		}

		memcpy(prot_config, user_params, length);
		break;
	}
	default:
		goto fail_cmd;
	}

	if ((q6audio_validate_port(port_id) < 0)) {
		pr_err("%s invalid port %d\n", __func__, port_id);
		goto fail_cmd;
	}

	index = q6audio_get_port_index(port_id);
	config->pdata.module_id = module_id;
	config->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						  APR_HDR_LEN(APR_HDR_SIZE),
						  APR_PKT_VER);
	config->hdr.pkt_size = sizeof(struct afe_ultrasound_config_command);
	config->hdr.src_port = 0;
	config->hdr.dest_port = 0;
	config->hdr.token = index;
	config->hdr.opcode = AFE_PORT_CMD_SET_PARAM_V2;
	config->param.port_id = q6audio_get_port_id(port_id);
	config->param.payload_size =
			sizeof(struct afe_ultrasound_config_command) -
			sizeof(config->hdr) - sizeof(config->param);
	config->pdata.param_id = *param_id;
	config->pdata.param_size = length;
	pr_debug("[ELUS]: param_size %d\n", length);

	atomic_set(elus_afe.ptr_state, 1);
	ret = apr_send_pkt(*elus_afe.ptr_apr, (uint32_t *) config);
	if (ret < 0) {
		pr_err("%s: Setting param for port %d param[0x%x]failed\n",
			   __func__, port_id, *param_id);
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
		pr_err("%s: config cmd failed\n", __func__);
		ret = -EINVAL;
		goto fail_cmd;
	}
	ret = 0;
	pr_debug("%s config->pdata.param_id %x status %d\n",
	__func__, config->pdata.param_id, ret);

fail_cmd:
	return ret;
}

int32_t elliptic_process_apr_payload(uint32_t *payload)
{
	uint32_t payload_size = 0;
	int32_t  ret = -1;
	struct elliptic_shared_data_block *data_block = NULL;
	size_t copy_size = 0;

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
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_CALIBRATION_DATA:
			if (payload_size >= ELLIPTIC_CALIBRATION_DATA_SIZE) {
				pr_debug("[ELUS]: calibration_data copied to local AP cache");

				data_block = elliptic_get_shared_obj(
					ELLIPTIC_OBJ_ID_CALIBRATION_DATA);
				copy_size = min_t(size_t, data_block->size,
					(size_t)ELLIPTIC_CALIBRATION_DATA_SIZE);

				memcpy((u8 *)data_block->buffer,
					&payload[3], copy_size);
				ret = (int32_t)copy_size;
			}
			break;
		case ELLIPTIC_ULTRASOUND_PARAM_ID_UPS_DATA:
		default:
			if (payload_size <=
				sizeof(struct afe_ultrasound_calib_get_resp)) {
				ret = elliptic_data_push(
					(const char *)&payload[3],
					(size_t)payload_size);

				if (ret != 0) {
					pr_err("[ELUS] : failed to push apr payload to elliptic device");
					return ret;
				}
				ret = payload_size;
			}
			break;
		}
	} else {
		pr_debug("[ELUS]: Invalid Ultrasound Module ID %d\n",
			payload[0]);
	}
	return ret;
}

