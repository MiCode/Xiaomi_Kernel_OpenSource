/* Copyright (c) 2012-2019, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_device.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/kobject.h>
#include <linux/delay.h>
#include <dsp/q6core.h>
#include <dsp/audio_cal_utils.h>
#include <dsp/apr_audio-v2.h>
#include <soc/snd_event.h>
#include <ipc/apr.h>
#include "adsp_err.h"

#define TIMEOUT_MS 1000
/*
 * AVS bring up in the modem is optimitized for the new
 * Sub System Restart design and 100 milliseconds timeout
 * is sufficient to make sure the Q6 will be ready.
 */
#define Q6_READY_TIMEOUT_MS 100

#define ADSP_STATE_READY_TIMEOUT_MS 3000

#define APR_ENOTREADY 10

enum {
	META_CAL,
	CUST_TOP_CAL,
	CORE_MAX_CAL
};

enum ver_query_status {
	VER_QUERY_UNATTEMPTED,
	VER_QUERY_UNSUPPORTED,
	VER_QUERY_SUPPORTED
};

struct q6core_avcs_ver_info {
	enum ver_query_status status;
	struct avcs_fwk_ver_info *ver_info;
};

struct q6core_str {
	struct apr_svc *core_handle_q;
	wait_queue_head_t bus_bw_req_wait;
	wait_queue_head_t cmd_req_wait;
	wait_queue_head_t avcs_fwk_ver_req_wait;
	u32 bus_bw_resp_received;
	enum cmd_flags {
		FLAG_NONE,
		FLAG_CMDRSP_LICENSE_RESULT
	} cmd_resp_received_flag;
	u32 avcs_fwk_ver_resp_received;
	struct mutex cmd_lock;
	struct mutex ver_lock;
	union {
		struct avcs_cmdrsp_get_license_validation_result
						cmdrsp_license_result;
	} cmd_resp_payload;
	u32 param;
	struct cal_type_data *cal_data[CORE_MAX_CAL];
	uint32_t mem_map_cal_handle;
	int32_t adsp_status;
	int32_t avs_state;
	struct q6core_avcs_ver_info q6core_avcs_ver_info;
};

static struct q6core_str q6core_lcl;

struct generic_get_data_ {
	int valid;
	int size_in_ints;
	int ints[];
};
static struct generic_get_data_ *generic_get_data;

static DEFINE_MUTEX(kset_lock);
static struct kset *audio_uevent_kset;

static int q6core_init_uevent_kset(void)
{
	int ret = 0;

	mutex_lock(&kset_lock);
	if (audio_uevent_kset)
		goto done;

	/* Create a kset under /sys/kernel/ */
	audio_uevent_kset = kset_create_and_add("q6audio", NULL, kernel_kobj);
	if (!audio_uevent_kset) {
		pr_err("%s: error creating uevent kernel set", __func__);
		ret = -EINVAL;
	}
done:
	mutex_unlock(&kset_lock);
	return ret;
}

static void q6core_destroy_uevent_kset(void)
{
	if (audio_uevent_kset) {
		kset_unregister(audio_uevent_kset);
		audio_uevent_kset = NULL;
	}
}

/**
 * q6core_init_uevent_data - initialize kernel object required to send uevents.
 *
 * @uevent_data: uevent data (dynamically allocated memory).
 * @name: name of the kernel object.
 *
 * Returns 0 on success or error otherwise.
 */
int q6core_init_uevent_data(struct audio_uevent_data *uevent_data, char *name)
{
	int ret = -EINVAL;

	if (!uevent_data || !name)
		return ret;

	ret = q6core_init_uevent_kset();
	if (ret)
		return ret;

	/* Set kset for kobject before initializing the kobject */
	uevent_data->kobj.kset = audio_uevent_kset;

	/* Initialize kobject and add it to kernel */
	ret = kobject_init_and_add(&uevent_data->kobj, &uevent_data->ktype,
					NULL, "%s", name);
	if (ret) {
		pr_err("%s: error initializing uevent kernel object: %d",
			__func__, ret);
		kobject_put(&uevent_data->kobj);
		return ret;
	}

	/* Send kobject add event to the system */
	kobject_uevent(&uevent_data->kobj, KOBJ_ADD);

	return ret;
}
EXPORT_SYMBOL(q6core_init_uevent_data);

/**
 * q6core_destroy_uevent_data - destroy kernel object.
 *
 * @uevent_data: uevent data.
 */
void q6core_destroy_uevent_data(struct audio_uevent_data *uevent_data)
{
	if (uevent_data)
		kobject_put(&uevent_data->kobj);
}
EXPORT_SYMBOL(q6core_destroy_uevent_data);

/**
 * q6core_send_uevent - send uevent to userspace.
 *
 * @uevent_data: uevent data.
 * @event: event to send.
 *
 * Returns 0 on success or error otherwise.
 */
int q6core_send_uevent(struct audio_uevent_data *uevent_data, char *event)
{
	char *env[] = { event, NULL };

	if (!event || !uevent_data)
		return -EINVAL;

	return kobject_uevent_env(&uevent_data->kobj, KOBJ_CHANGE, env);
}
EXPORT_SYMBOL(q6core_send_uevent);

static int parse_fwk_version_info(uint32_t *payload, uint16_t payload_size)
{
	size_t ver_size;
	int num_services;

	pr_debug("%s: Payload info num services %d\n",
		 __func__, payload[4]);

	/*
	 * payload1[4] is the number of services running on DSP
	 * Based on this info, we copy the payload into core
	 * avcs version info structure.
	 */
	if (payload_size < 5 * sizeof(uint32_t)) {
		pr_err("%s: payload has invalid size %d\n",
			__func__, payload_size);
		return -EINVAL;
	}
	num_services = payload[4];
	if (num_services > VSS_MAX_AVCS_NUM_SERVICES) {
		pr_err("%s: num_services: %d greater than max services: %d\n",
		       __func__, num_services, VSS_MAX_AVCS_NUM_SERVICES);
		return -EINVAL;
	}

	/*
	 * Dynamically allocate memory for all
	 * the services based on num_services
	 */
	ver_size = sizeof(struct avcs_get_fwk_version) +
		   num_services * sizeof(struct avs_svc_api_info);

	if (payload_size < ver_size) {
		pr_err("%s: payload has invalid size %d, expected size %zu\n",
			__func__, payload_size, ver_size);
		return -EINVAL;
	}

	q6core_lcl.q6core_avcs_ver_info.ver_info =
		kzalloc(ver_size, GFP_ATOMIC);
	if (q6core_lcl.q6core_avcs_ver_info.ver_info == NULL)
		return -ENOMEM;

	memcpy(q6core_lcl.q6core_avcs_ver_info.ver_info, (uint8_t *) payload,
	       ver_size);
	return 0;
}

static int32_t aprv2_core_fn_q(struct apr_client_data *data, void *priv)
{
	uint32_t *payload1;
	int ret = 0;

	if (data == NULL) {
		pr_err("%s: data argument is null\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: core msg: payload len = %u, apr resp opcode = 0x%x\n",
		__func__,
		data->payload_size, data->opcode);

	switch (data->opcode) {

	case APR_BASIC_RSP_RESULT:{

		if (data->payload_size == 0) {
			pr_err("%s: APR_BASIC_RSP_RESULT No Payload ",
					__func__);
			return 0;
		}

		payload1 = data->payload;

		if (data->payload_size < 2 * sizeof(uint32_t)) {
			pr_err("%s: payload has invalid size %d\n",
				__func__, data->payload_size);
			return -EINVAL;
		}

		switch (payload1[0]) {

		case AVCS_CMD_SHARED_MEM_UNMAP_REGIONS:
			pr_debug("%s: Cmd = AVCS_CMD_SHARED_MEM_UNMAP_REGIONS status[0x%x]\n",
				__func__, payload1[1]);
			/* -ADSP status to match Linux error standard */
			q6core_lcl.adsp_status = -payload1[1];
			q6core_lcl.bus_bw_resp_received = 1;
			wake_up(&q6core_lcl.bus_bw_req_wait);
			break;
		case AVCS_CMD_SHARED_MEM_MAP_REGIONS:
			pr_debug("%s: Cmd = AVCS_CMD_SHARED_MEM_MAP_REGIONS status[0x%x]\n",
				__func__, payload1[1]);
			/* -ADSP status to match Linux error standard */
			q6core_lcl.adsp_status = -payload1[1];
			q6core_lcl.bus_bw_resp_received = 1;
			wake_up(&q6core_lcl.bus_bw_req_wait);
			break;
		case AVCS_CMD_MAP_MDF_SHARED_MEMORY:
			pr_debug("%s: Cmd = AVCS_CMD_MAP_MDF_SHARED_MEMORY status[0x%x]\n",
				__func__, payload1[1]);
			/* -ADSP status to match Linux error standard */
			q6core_lcl.adsp_status = -payload1[1];
			q6core_lcl.bus_bw_resp_received = 1;
			wake_up(&q6core_lcl.bus_bw_req_wait);
			break;
		case AVCS_CMD_REGISTER_TOPOLOGIES:
			pr_debug("%s: Cmd = AVCS_CMD_REGISTER_TOPOLOGIES status[0x%x]\n",
				__func__, payload1[1]);
			/* -ADSP status to match Linux error standard */
			q6core_lcl.adsp_status = -payload1[1];
			q6core_lcl.bus_bw_resp_received = 1;
			wake_up(&q6core_lcl.bus_bw_req_wait);
			break;
		case AVCS_CMD_DEREGISTER_TOPOLOGIES:
			pr_debug("%s: Cmd = AVCS_CMD_DEREGISTER_TOPOLOGIES status[0x%x]\n",
				__func__, payload1[1]);
			q6core_lcl.bus_bw_resp_received = 1;
			wake_up(&q6core_lcl.bus_bw_req_wait);
			break;
		case AVCS_CMD_GET_FWK_VERSION:
			pr_debug("%s: Cmd = AVCS_CMD_GET_FWK_VERSION status[%s]\n",
				__func__, adsp_err_get_err_str(payload1[1]));
			/* ADSP status to match Linux error standard */
			q6core_lcl.adsp_status = -payload1[1];
			if (payload1[1] == ADSP_EUNSUPPORTED)
				q6core_lcl.q6core_avcs_ver_info.status =
					VER_QUERY_UNSUPPORTED;
			q6core_lcl.avcs_fwk_ver_resp_received = 1;
			wake_up(&q6core_lcl.avcs_fwk_ver_req_wait);
			break;
		case AVCS_CMD_LOAD_TOPO_MODULES:
		case AVCS_CMD_UNLOAD_TOPO_MODULES:
			pr_debug("%s: Cmd = %s status[%s]\n",
				__func__,
				(payload1[0] == AVCS_CMD_LOAD_TOPO_MODULES) ?
				"AVCS_CMD_LOAD_TOPO_MODULES" :
				"AVCS_CMD_UNLOAD_TOPO_MODULES",
				adsp_err_get_err_str(payload1[1]));
			break;
		default:
			pr_err("%s: Invalid cmd rsp[0x%x][0x%x] opcode %d\n",
					__func__,
					payload1[0], payload1[1], data->opcode);
			break;
		}
		break;
	}

	case RESET_EVENTS:{
		pr_debug("%s: Reset event received in Core service\n",
			__func__);
		/*
		 * no reset for q6core_avcs_ver_info done as
		 * the data will not change after SSR
		 */
		apr_reset(q6core_lcl.core_handle_q);
		q6core_lcl.core_handle_q = NULL;
		break;
	}
	case AVCS_CMDRSP_SHARED_MEM_MAP_REGIONS:
		if (data->payload_size < sizeof(uint32_t)) {
			pr_err("%s: payload has invalid size %d\n",
				__func__, data->payload_size);
			return -EINVAL;
		}
		payload1 = data->payload;
		pr_debug("%s: AVCS_CMDRSP_SHARED_MEM_MAP_REGIONS handle %d\n",
			__func__, payload1[0]);
		q6core_lcl.mem_map_cal_handle = payload1[0];
		q6core_lcl.bus_bw_resp_received = 1;
		wake_up(&q6core_lcl.bus_bw_req_wait);
		break;
	case AVCS_CMDRSP_ADSP_EVENT_GET_STATE:
		if (data->payload_size < sizeof(uint32_t)) {
			pr_err("%s: payload has invalid size %d\n",
				__func__, data->payload_size);
			return -EINVAL;
		}
		payload1 = data->payload;
		q6core_lcl.param = payload1[0];
		pr_debug("%s: Received ADSP get state response 0x%x\n",
			 __func__, q6core_lcl.param);
		/* ensure .param is updated prior to .bus_bw_resp_received */
		wmb();
		q6core_lcl.bus_bw_resp_received = 1;
		wake_up(&q6core_lcl.bus_bw_req_wait);
		break;
	case AVCS_CMDRSP_GET_LICENSE_VALIDATION_RESULT:
		if (data->payload_size < sizeof(uint32_t)) {
			pr_err("%s: payload has invalid size %d\n",
				__func__, data->payload_size);
			return -EINVAL;
		}
		payload1 = data->payload;
		pr_debug("%s: cmd = LICENSE_VALIDATION_RESULT, result = 0x%x\n",
				__func__, payload1[0]);
		q6core_lcl.cmd_resp_payload.cmdrsp_license_result.result
								= payload1[0];
		q6core_lcl.cmd_resp_received_flag = FLAG_CMDRSP_LICENSE_RESULT;
		wake_up(&q6core_lcl.cmd_req_wait);
		break;
	case AVCS_CMDRSP_GET_FWK_VERSION:
		pr_debug("%s: Received AVCS_CMDRSP_GET_FWK_VERSION\n",
			 __func__);
		payload1 = data->payload;
		ret = parse_fwk_version_info(payload1, data->payload_size);
		if (ret < 0) {
			q6core_lcl.adsp_status = ret;
			pr_err("%s: Failed to parse payload:%d\n",
			       __func__, ret);
		} else {
			q6core_lcl.q6core_avcs_ver_info.status =
						VER_QUERY_SUPPORTED;
		}
		q6core_lcl.avcs_fwk_ver_resp_received = 1;
		wake_up(&q6core_lcl.avcs_fwk_ver_req_wait);
		break;
	default:
		pr_err("%s: Message id from adsp core svc: 0x%x\n",
			__func__, data->opcode);
		if (generic_get_data) {
			generic_get_data->valid = 1;
			generic_get_data->size_in_ints =
				data->payload_size/sizeof(int);
			pr_debug("callback size = %i\n",
				 data->payload_size);
			memcpy(generic_get_data->ints, data->payload,
				data->payload_size);
			q6core_lcl.bus_bw_resp_received = 1;
			wake_up(&q6core_lcl.bus_bw_req_wait);
			break;
		}
		break;
	}

	return 0;
}

void ocm_core_open(void)
{
	if (q6core_lcl.core_handle_q == NULL)
		q6core_lcl.core_handle_q = apr_register("ADSP", "CORE",
					aprv2_core_fn_q, 0xFFFFFFFF, NULL);
	pr_debug("%s: Open_q %pK\n", __func__, q6core_lcl.core_handle_q);
	if (q6core_lcl.core_handle_q == NULL)
		pr_err_ratelimited("%s: Unable to register CORE\n", __func__);
}

struct cal_block_data *cal_utils_get_cal_block_by_key(
		struct cal_type_data *cal_type, uint32_t key)
{
	struct list_head                *ptr, *next;
	struct cal_block_data           *cal_block = NULL;
	struct audio_cal_info_metainfo  *metainfo;

	list_for_each_safe(ptr, next,
		&cal_type->cal_blocks) {

		cal_block = list_entry(ptr,
			struct cal_block_data, list);
		metainfo = (struct audio_cal_info_metainfo *)
			cal_block->cal_info;
		if (metainfo->nKey != key) {
			pr_debug("%s: metainfo key mismatch!!! found:%x, needed:%x\n",
				__func__, metainfo->nKey, key);
		} else {
			pr_debug("%s: metainfo key match found", __func__);
			return cal_block;
		}
	}
	return NULL;
}

static int q6core_send_get_avcs_fwk_ver_cmd(void)
{
	struct apr_hdr avcs_ver_cmd;
	int ret;

	avcs_ver_cmd.hdr_field =
		APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, APR_HDR_LEN(APR_HDR_SIZE),
			      APR_PKT_VER);
	avcs_ver_cmd.pkt_size = sizeof(struct apr_hdr);
	avcs_ver_cmd.src_port = 0;
	avcs_ver_cmd.dest_port = 0;
	avcs_ver_cmd.token = 0;
	avcs_ver_cmd.opcode = AVCS_CMD_GET_FWK_VERSION;

	q6core_lcl.adsp_status = 0;
	q6core_lcl.avcs_fwk_ver_resp_received = 0;

	ret = apr_send_pkt(q6core_lcl.core_handle_q,
			   (uint32_t *) &avcs_ver_cmd);
	if (ret < 0) {
		pr_err("%s: failed to send apr packet, ret=%d\n", __func__,
		       ret);
		goto done;
	}

	ret = wait_event_timeout(q6core_lcl.avcs_fwk_ver_req_wait,
				 (q6core_lcl.avcs_fwk_ver_resp_received == 1),
				 msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout for AVCS fwk version info\n",
		       __func__);
		ret = -ETIMEDOUT;
		goto done;
	}

	if (q6core_lcl.adsp_status < 0) {
		/*
		 * adsp_err_get_err_str expects a positive value but we store
		 * the DSP error as negative to match the Linux error standard.
		 * Pass in the negated value so adsp_err_get_err_str returns
		 * the correct string.
		 */
		pr_err("%s: DSP returned error[%s]\n", __func__,
		       adsp_err_get_err_str(-q6core_lcl.adsp_status));
		ret = adsp_err_get_lnx_err_code(q6core_lcl.adsp_status);
		goto done;
	}

	ret = 0;

done:
	return ret;
}

int q6core_get_service_version(uint32_t service_id,
			       struct avcs_fwk_ver_info *ver_info,
			       size_t size)
{
	struct avcs_fwk_ver_info *cached_ver_info = NULL;
	int i;
	uint32_t num_services;
	size_t ver_size;
	int ret;

	if (ver_info == NULL) {
		pr_err("%s: ver_info is NULL\n", __func__);
		return -EINVAL;
	}

	ret = q6core_get_fwk_version_size(service_id);
	if (ret < 0) {
		pr_err("%s: Failed to get service size for service id %d with error %d\n",
		       __func__, service_id, ret);
		return ret;
	}

	ver_size = ret;
	if (ver_size != size) {
		pr_err("%s: Expected size %zu and provided size %zu do not match\n",
		       __func__, ver_size, size);
		return -EINVAL;
	}

	cached_ver_info = q6core_lcl.q6core_avcs_ver_info.ver_info;
	num_services = cached_ver_info->avcs_fwk_version.num_services;

	if (service_id == AVCS_SERVICE_ID_ALL) {
		memcpy(ver_info, cached_ver_info, ver_size);
		return 0;
	}

	ver_info->avcs_fwk_version = cached_ver_info->avcs_fwk_version;
	for (i = 0; i < num_services; i++) {
		if (cached_ver_info->services[i].service_id == service_id) {
			ver_info->services[0] = cached_ver_info->services[i];
			return 0;
		}
	}
	pr_err("%s: No service matching service ID %d\n", __func__, service_id);
	return -EINVAL;
}
EXPORT_SYMBOL(q6core_get_service_version);

static int q6core_get_avcs_fwk_version(void)
{
	int ret = 0;

	mutex_lock(&(q6core_lcl.ver_lock));
	pr_debug("%s: q6core_avcs_ver_info.status(%d)\n", __func__,
		 q6core_lcl.q6core_avcs_ver_info.status);

	switch (q6core_lcl.q6core_avcs_ver_info.status) {
	case VER_QUERY_SUPPORTED:
		pr_debug("%s: AVCS FWK version query already attempted\n",
			 __func__);
		break;
	case VER_QUERY_UNSUPPORTED:
		ret = -EOPNOTSUPP;
		break;
	case VER_QUERY_UNATTEMPTED:
		pr_debug("%s: Attempting AVCS FWK version query\n", __func__);
		if (q6core_is_adsp_ready()) {
			ret = q6core_send_get_avcs_fwk_ver_cmd();
		} else {
			pr_err("%s: ADSP is not ready to query version\n",
			       __func__);
			ret = -ENODEV;
		}
		break;
	default:
		pr_err("%s: Invalid version query status %d\n", __func__,
		       q6core_lcl.q6core_avcs_ver_info.status);
		ret = -EINVAL;
		break;
	}
	mutex_unlock(&(q6core_lcl.ver_lock));
	return ret;
}

size_t q6core_get_fwk_version_size(uint32_t service_id)
{
	int ret = 0;
	uint32_t num_services;

	ret = q6core_get_avcs_fwk_version();
	if (ret)
		goto done;

	if (q6core_lcl.q6core_avcs_ver_info.ver_info != NULL) {
		num_services = q6core_lcl.q6core_avcs_ver_info.ver_info
					->avcs_fwk_version.num_services;
	} else {
		pr_err("%s: ver_info is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	ret = sizeof(struct avcs_get_fwk_version);
	if (service_id == AVCS_SERVICE_ID_ALL)
		ret += num_services * sizeof(struct avs_svc_api_info);
	else
		ret += sizeof(struct avs_svc_api_info);
done:
	return ret;
}
EXPORT_SYMBOL(q6core_get_fwk_version_size);

/**
 * q6core_get_avcs_version_per_service -
 *       to get api version of a particular service
 *
 * @service_id: id of the service
 *
 * Returns valid version on success or error (negative value) on failure
 */
int q6core_get_avcs_api_version_per_service(uint32_t service_id)
{
	struct avcs_fwk_ver_info *cached_ver_info = NULL;
	int i;
	uint32_t num_services;
	int ret = 0;

	if (service_id == AVCS_SERVICE_ID_ALL)
		return -EINVAL;

	ret = q6core_get_avcs_fwk_version();
	if (ret < 0) {
		pr_err("%s: failure in getting AVCS version\n", __func__);
		return ret;
	}

	cached_ver_info = q6core_lcl.q6core_avcs_ver_info.ver_info;
	num_services = cached_ver_info->avcs_fwk_version.num_services;

	for (i = 0; i < num_services; i++) {
		if (cached_ver_info->services[i].service_id == service_id)
			return cached_ver_info->services[i].api_version;
	}
	pr_err("%s: No service matching service ID %d\n", __func__, service_id);
	return -EINVAL;
}
EXPORT_SYMBOL(q6core_get_avcs_api_version_per_service);

/**
 * core_set_license -
 *       command to set license for module
 *
 * @key: license key hash
 * @module_id: DSP Module ID
 *
 * Returns 0 on success or error on failure
 */
int32_t core_set_license(uint32_t key, uint32_t module_id)
{
	struct avcs_cmd_set_license *cmd_setl = NULL;
	struct cal_block_data *cal_block = NULL;
	int rc = 0, packet_size = 0;

	pr_debug("%s: key:0x%x, id:0x%x\n", __func__, key, module_id);

	mutex_lock(&(q6core_lcl.cmd_lock));
	if (q6core_lcl.cal_data[META_CAL] == NULL) {
		pr_err("%s: cal_data not initialized yet!!\n", __func__);
		rc = -EINVAL;
		goto cmd_unlock;
	}

	mutex_lock(&((q6core_lcl.cal_data[META_CAL])->lock));
	cal_block = cal_utils_get_cal_block_by_key(
				q6core_lcl.cal_data[META_CAL], key);
	if (cal_block == NULL ||
		cal_block->cal_data.kvaddr == NULL ||
		cal_block->cal_data.size <= 0) {
		pr_err("%s: Invalid cal block to send", __func__);
		rc = -EINVAL;
		goto cal_data_unlock;
	}

	packet_size = sizeof(struct avcs_cmd_set_license) +
					cal_block->cal_data.size;
	/*round up total packet_size to next 4 byte boundary*/
	packet_size = ((packet_size + 0x3)>>2)<<2;

	cmd_setl = kzalloc(packet_size, GFP_KERNEL);
	if (cmd_setl == NULL) {
		rc  = -ENOMEM;
		goto cal_data_unlock;
	}

	ocm_core_open();
	if (q6core_lcl.core_handle_q == NULL) {
		pr_err("%s: apr registration for CORE failed\n", __func__);
		rc  = -ENODEV;
		goto fail_cmd;
	}

	cmd_setl->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	cmd_setl->hdr.pkt_size = packet_size;
	cmd_setl->hdr.src_port = 0;
	cmd_setl->hdr.dest_port = 0;
	cmd_setl->hdr.token = 0;
	cmd_setl->hdr.opcode = AVCS_CMD_SET_LICENSE;
	cmd_setl->id = module_id;
	cmd_setl->overwrite = 1;
	cmd_setl->size = cal_block->cal_data.size;
	memcpy((uint8_t *)cmd_setl + sizeof(struct avcs_cmd_set_license),
		cal_block->cal_data.kvaddr,
		cal_block->cal_data.size);
	pr_info("%s: Set license opcode=0x%x, id =0x%x, size = %d\n",
			__func__, cmd_setl->hdr.opcode,
			cmd_setl->id, cmd_setl->size);
	rc = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *)cmd_setl);
	if (rc < 0)
		pr_err("%s: SET_LICENSE failed op[0x%x]rc[%d]\n",
					__func__, cmd_setl->hdr.opcode, rc);

fail_cmd:
	kfree(cmd_setl);
cal_data_unlock:
	mutex_unlock(&((q6core_lcl.cal_data[META_CAL])->lock));
cmd_unlock:
	mutex_unlock(&(q6core_lcl.cmd_lock));

	return rc;
}
EXPORT_SYMBOL(core_set_license);

/**
 * core_get_license_status -
 *       command to retrieve license status for module
 *
 * @module_id: DSP Module ID
 *
 * Returns 0 on success or error on failure
 */
int32_t core_get_license_status(uint32_t module_id)
{
	struct avcs_cmd_get_license_validation_result get_lvr_cmd;
	int ret = 0;

	pr_debug("%s: module_id 0x%x", __func__, module_id);

	mutex_lock(&(q6core_lcl.cmd_lock));
	ocm_core_open();
	if (q6core_lcl.core_handle_q == NULL) {
		pr_err("%s: apr registration for CORE failed\n", __func__);
		ret  = -ENODEV;
		goto fail_cmd;
	}

	get_lvr_cmd.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	get_lvr_cmd.hdr.pkt_size =
		sizeof(struct avcs_cmd_get_license_validation_result);

	get_lvr_cmd.hdr.src_port = 0;
	get_lvr_cmd.hdr.dest_port = 0;
	get_lvr_cmd.hdr.token = 0;
	get_lvr_cmd.hdr.opcode = AVCS_CMD_GET_LICENSE_VALIDATION_RESULT;
	get_lvr_cmd.id = module_id;


	ret = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *) &get_lvr_cmd);
	if (ret < 0) {
		pr_err("%s: license_validation request failed, err %d\n",
							__func__, ret);
		ret = -EREMOTE;
		goto fail_cmd;
	}

	q6core_lcl.cmd_resp_received_flag &= ~(FLAG_CMDRSP_LICENSE_RESULT);
	mutex_unlock(&(q6core_lcl.cmd_lock));
	ret = wait_event_timeout(q6core_lcl.cmd_req_wait,
			(q6core_lcl.cmd_resp_received_flag ==
				FLAG_CMDRSP_LICENSE_RESULT),
				msecs_to_jiffies(TIMEOUT_MS));
	mutex_lock(&(q6core_lcl.cmd_lock));
	if (!ret) {
		pr_err("%s: wait_event timeout for CMDRSP_LICENSE_RESULT\n",
				__func__);
		ret = -ETIME;
		goto fail_cmd;
	}
	q6core_lcl.cmd_resp_received_flag &= ~(FLAG_CMDRSP_LICENSE_RESULT);
	ret = q6core_lcl.cmd_resp_payload.cmdrsp_license_result.result;

fail_cmd:
	mutex_unlock(&(q6core_lcl.cmd_lock));
	pr_info("%s: cmdrsp_license_result.result = 0x%x for module 0x%x\n",
				__func__, ret, module_id);
	return ret;
}
EXPORT_SYMBOL(core_get_license_status);

/**
 * core_set_dolby_manufacturer_id -
 *       command to set dolby manufacturer id
 *
 * @manufacturer_id: Dolby manufacturer id
 *
 * Returns 0 on success or error on failure
 */
uint32_t core_set_dolby_manufacturer_id(int manufacturer_id)
{
	struct adsp_dolby_manufacturer_id payload;
	int rc = 0;

	pr_debug("%s: manufacturer_id :%d\n", __func__, manufacturer_id);
	mutex_lock(&(q6core_lcl.cmd_lock));
	ocm_core_open();
	if (q6core_lcl.core_handle_q) {
		payload.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_EVENT,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
		payload.hdr.pkt_size =
			sizeof(struct adsp_dolby_manufacturer_id);
		payload.hdr.src_port = 0;
		payload.hdr.dest_port = 0;
		payload.hdr.token = 0;
		payload.hdr.opcode = ADSP_CMD_SET_DOLBY_MANUFACTURER_ID;
		payload.manufacturer_id = manufacturer_id;
		pr_debug("%s: Send Dolby security opcode=0x%x manufacturer ID = %d\n",
			__func__,
			payload.hdr.opcode, payload.manufacturer_id);
		rc = apr_send_pkt(q6core_lcl.core_handle_q,
						(uint32_t *)&payload);
		if (rc < 0)
			pr_err("%s: SET_DOLBY_MANUFACTURER_ID failed op[0x%x]rc[%d]\n",
				__func__, payload.hdr.opcode, rc);
	}
	mutex_unlock(&(q6core_lcl.cmd_lock));
	return rc;
}
EXPORT_SYMBOL(core_set_dolby_manufacturer_id);

int32_t q6core_load_unload_topo_modules(uint32_t topo_id,
			bool preload_type)
{
	struct avcs_cmd_load_unload_topo_modules load_unload_topo_modules;
	int ret = 0;

	mutex_lock(&(q6core_lcl.cmd_lock));
	ocm_core_open();
	if (q6core_lcl.core_handle_q == NULL) {
		pr_err("%s: apr registration for CORE failed\n", __func__);
		ret  = -ENODEV;
		goto done;
	}

	memset(&load_unload_topo_modules, 0, sizeof(load_unload_topo_modules));
	load_unload_topo_modules.hdr.hdr_field =
			APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
			APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	load_unload_topo_modules.hdr.pkt_size =
			sizeof(struct avcs_cmd_load_unload_topo_modules);
	load_unload_topo_modules.hdr.src_port = 0;
	load_unload_topo_modules.hdr.dest_port = 0;
	load_unload_topo_modules.hdr.token = 0;

	if (preload_type == CORE_LOAD_TOPOLOGY)
		load_unload_topo_modules.hdr.opcode =
			AVCS_CMD_LOAD_TOPO_MODULES;
	else
		load_unload_topo_modules.hdr.opcode =
			AVCS_CMD_UNLOAD_TOPO_MODULES;

	load_unload_topo_modules.topology_id = topo_id;
	ret = apr_send_pkt(q6core_lcl.core_handle_q,
		(uint32_t *) &load_unload_topo_modules);
	if (ret < 0) {
		pr_err_ratelimited("%s: Load/unload topo modules failed for topology = %d ret = %d\n",
			__func__, topo_id, ret);
		ret = -EINVAL;
	}

done:
	mutex_unlock(&(q6core_lcl.cmd_lock));

	return ret;
}
EXPORT_SYMBOL(q6core_load_unload_topo_modules);

/**
 * q6core_is_adsp_ready - check adsp ready status
 *
 * Returns true if adsp is ready otherwise returns false
 */
bool q6core_is_adsp_ready(void)
{
	int rc = 0;
	bool ret = false;
	struct apr_hdr hdr;

	pr_debug("%s: enter\n", __func__);
	memset(&hdr, 0, sizeof(hdr));
	hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
				      APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	hdr.pkt_size = APR_PKT_SIZE(APR_HDR_SIZE, 0);
	hdr.opcode = AVCS_CMD_ADSP_EVENT_GET_STATE;

	mutex_lock(&(q6core_lcl.cmd_lock));
	ocm_core_open();
	if (q6core_lcl.core_handle_q) {
		q6core_lcl.bus_bw_resp_received = 0;
		rc = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *)&hdr);
		if (rc < 0) {
			pr_err_ratelimited("%s: Get ADSP state APR packet send event %d\n",
				__func__, rc);
			goto bail;
		}

		rc = wait_event_timeout(q6core_lcl.bus_bw_req_wait,
					(q6core_lcl.bus_bw_resp_received == 1),
					msecs_to_jiffies(Q6_READY_TIMEOUT_MS));
		if (rc > 0 && q6core_lcl.bus_bw_resp_received) {
			/* ensure to read updated param by callback thread */
			rmb();
			ret = !!q6core_lcl.param;
		}
	}
bail:
	pr_debug("%s: leave, rc %d, adsp ready %d\n", __func__, rc, ret);
	mutex_unlock(&(q6core_lcl.cmd_lock));
	return ret;
}
EXPORT_SYMBOL(q6core_is_adsp_ready);

int q6core_map_memory_regions(phys_addr_t *buf_add, uint32_t mempool_id,
			uint32_t *bufsz, uint32_t bufcnt, uint32_t *map_handle)
{
	struct avs_cmd_shared_mem_map_regions *mmap_regions = NULL;
	struct avs_shared_map_region_payload *mregions = NULL;
	void *mmap_region_cmd = NULL;
	void *payload = NULL;
	int ret = 0;
	int i = 0;
	int cmd_size = 0;

	cmd_size = sizeof(struct avs_cmd_shared_mem_map_regions)
			+ sizeof(struct avs_shared_map_region_payload)
			* bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (mmap_region_cmd == NULL)
		return -ENOMEM;

	mmap_regions = (struct avs_cmd_shared_mem_map_regions *)mmap_region_cmd;
	mmap_regions->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
								APR_PKT_VER);
	mmap_regions->hdr.pkt_size = cmd_size;
	mmap_regions->hdr.src_port = 0;
	mmap_regions->hdr.dest_port = 0;
	mmap_regions->hdr.token = 0;
	mmap_regions->hdr.opcode = AVCS_CMD_SHARED_MEM_MAP_REGIONS;
	mmap_regions->mem_pool_id = mempool_id & 0x00ff;
	mmap_regions->num_regions = bufcnt & 0x00ff;
	mmap_regions->property_flag = 0x00;

	payload = ((u8 *) mmap_region_cmd +
				sizeof(struct avs_cmd_shared_mem_map_regions));
	mregions = (struct avs_shared_map_region_payload *)payload;

	for (i = 0; i < bufcnt; i++) {
		mregions->shm_addr_lsw = lower_32_bits(buf_add[i]);
		mregions->shm_addr_msw =
				msm_audio_populate_upper_32_bits(buf_add[i]);
		mregions->mem_size_bytes = bufsz[i];
		++mregions;
	}

	pr_debug("%s: sending memory map, addr %pK, size %d, bufcnt = %d\n",
		__func__, buf_add, bufsz[0], mmap_regions->num_regions);

	*map_handle = 0;
	q6core_lcl.adsp_status = 0;
	q6core_lcl.bus_bw_resp_received = 0;
	ret = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *)
		mmap_regions);
	if (ret < 0) {
		pr_err("%s: mmap regions failed %d\n",
			__func__, ret);
		ret = -EINVAL;
		goto done;
	}

	ret = wait_event_timeout(q6core_lcl.bus_bw_req_wait,
				(q6core_lcl.bus_bw_resp_received == 1),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: timeout. waited for memory map\n", __func__);
		ret = -ETIME;
		goto done;
	} else {
		/* set ret to 0 as no timeout happened */
		ret = 0;
	}

	if (q6core_lcl.adsp_status < 0) {
		pr_err("%s: DSP returned error %d\n",
			__func__, q6core_lcl.adsp_status);
		ret = q6core_lcl.adsp_status;
		goto done;
	}

	*map_handle = q6core_lcl.mem_map_cal_handle;
done:
	kfree(mmap_region_cmd);
	return ret;
}

int q6core_memory_unmap_regions(uint32_t mem_map_handle)
{
	struct avs_cmd_shared_mem_unmap_regions unmap_regions;
	int ret = 0;

	memset(&unmap_regions, 0, sizeof(unmap_regions));
	unmap_regions.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	unmap_regions.hdr.pkt_size = sizeof(unmap_regions);
	unmap_regions.hdr.src_svc = APR_SVC_ADSP_CORE;
	unmap_regions.hdr.src_domain = APR_DOMAIN_APPS;
	unmap_regions.hdr.src_port = 0;
	unmap_regions.hdr.dest_svc = APR_SVC_ADSP_CORE;
	unmap_regions.hdr.dest_domain = APR_DOMAIN_ADSP;
	unmap_regions.hdr.dest_port = 0;
	unmap_regions.hdr.token = 0;
	unmap_regions.hdr.opcode = AVCS_CMD_SHARED_MEM_UNMAP_REGIONS;
	unmap_regions.mem_map_handle = mem_map_handle;

	q6core_lcl.adsp_status = 0;
	q6core_lcl.bus_bw_resp_received = 0;

	pr_debug("%s: unmap regions map handle %d\n",
		__func__, mem_map_handle);

	ret = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *)
		&unmap_regions);
	if (ret < 0) {
		pr_err("%s: unmap regions failed %d\n",
			__func__, ret);
		ret = -EINVAL;
		goto done;
	}

	ret = wait_event_timeout(q6core_lcl.bus_bw_req_wait,
				(q6core_lcl.bus_bw_resp_received == 1),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: timeout. waited for memory_unmap\n",
		       __func__);
		ret = -ETIME;
		goto done;
	} else {
		/* set ret to 0 as no timeout happened */
		ret = 0;
	}
	if (q6core_lcl.adsp_status < 0) {
		pr_err("%s: DSP returned error %d\n",
			__func__, q6core_lcl.adsp_status);
		ret = q6core_lcl.adsp_status;
		goto done;
	}
done:
	return ret;
}


int q6core_map_mdf_shared_memory(uint32_t map_handle, phys_addr_t *buf_add,
			uint32_t proc_id, uint32_t *bufsz, uint32_t bufcnt)
{
	struct avs_cmd_map_mdf_shared_memory *mmap_regions = NULL;
	struct avs_shared_map_region_payload *mregions = NULL;
	void *mmap_region_cmd = NULL;
	void *payload = NULL;
	int ret = 0;
	int i = 0;
	int cmd_size = 0;

	cmd_size = sizeof(struct avs_cmd_map_mdf_shared_memory)
			+ sizeof(struct avs_shared_map_region_payload)
			* bufcnt;

	mmap_region_cmd = kzalloc(cmd_size, GFP_KERNEL);
	if (mmap_region_cmd == NULL)
		return -ENOMEM;

	mmap_regions = (struct avs_cmd_map_mdf_shared_memory *)mmap_region_cmd;
	mmap_regions->hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
						APR_HDR_LEN(APR_HDR_SIZE),
								APR_PKT_VER);
	mmap_regions->hdr.pkt_size = cmd_size;
	mmap_regions->hdr.src_port = 0;
	mmap_regions->hdr.dest_port = 0;
	mmap_regions->hdr.token = 0;
	mmap_regions->hdr.opcode = AVCS_CMD_MAP_MDF_SHARED_MEMORY;
	mmap_regions->mem_map_handle = map_handle;
	mmap_regions->proc_id = proc_id & 0x00ff;
	mmap_regions->num_regions = bufcnt & 0x00ff;

	payload = ((u8 *) mmap_region_cmd +
				sizeof(struct avs_cmd_map_mdf_shared_memory));
	mregions = (struct avs_shared_map_region_payload *)payload;

	for (i = 0; i < bufcnt; i++) {
		mregions->shm_addr_lsw = lower_32_bits(buf_add[i]);
		mregions->shm_addr_msw =
				msm_audio_populate_upper_32_bits(buf_add[i]);
		mregions->mem_size_bytes = bufsz[i];
		++mregions;
	}

	pr_debug("%s: sending mdf memory map, addr %pa, size %d, bufcnt = %d\n",
		__func__, buf_add, bufsz[0], mmap_regions->num_regions);

	q6core_lcl.adsp_status = 0;
	q6core_lcl.bus_bw_resp_received = 0;
	ret = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *)
		mmap_regions);
	if (ret < 0) {
		pr_err("%s: mdf memory map failed %d\n",
			__func__, ret);
		ret = -EINVAL;
		goto done;
	}

	ret = wait_event_timeout(q6core_lcl.bus_bw_req_wait,
				(q6core_lcl.bus_bw_resp_received == 1),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: timeout. waited for mdf memory map\n",
			__func__);
		ret = -ETIME;
		goto done;
	} else {
		/* set ret to 0 as no timeout happened */
		ret = 0;
	}

	/*
	 * When the remote DSP is not ready, the ADSP will validate and store
	 * the memory information and return APR_ENOTREADY to HLOS. The ADSP
	 * will map the memory with remote DSP when it is ready. HLOS should
	 * not treat APR_ENOTREADY as an error.
	 */
	if (q6core_lcl.adsp_status != -APR_ENOTREADY) {
		pr_err("%s: DSP returned error %d\n",
			__func__, q6core_lcl.adsp_status);
		ret = q6core_lcl.adsp_status;
		goto done;
	}

done:
	kfree(mmap_region_cmd);
	return ret;
}

static int q6core_dereg_all_custom_topologies(void)
{
	int ret = 0;
	struct avcs_cmd_deregister_topologies dereg_top;

	memset(&dereg_top, 0, sizeof(dereg_top));
	dereg_top.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	dereg_top.hdr.pkt_size = sizeof(dereg_top);
	dereg_top.hdr.src_svc = APR_SVC_ADSP_CORE;
	dereg_top.hdr.src_domain = APR_DOMAIN_APPS;
	dereg_top.hdr.src_port = 0;
	dereg_top.hdr.dest_svc = APR_SVC_ADSP_CORE;
	dereg_top.hdr.dest_domain = APR_DOMAIN_ADSP;
	dereg_top.hdr.dest_port = 0;
	dereg_top.hdr.token = 0;
	dereg_top.hdr.opcode = AVCS_CMD_DEREGISTER_TOPOLOGIES;
	dereg_top.payload_addr_lsw = 0;
	dereg_top.payload_addr_msw = 0;
	dereg_top.mem_map_handle = 0;
	dereg_top.payload_size = 0;
	dereg_top.mode = AVCS_MODE_DEREGISTER_ALL_CUSTOM_TOPOLOGIES;

	q6core_lcl.bus_bw_resp_received = 0;

	pr_debug("%s: Deregister topologies mode %d\n",
		__func__, dereg_top.mode);

	ret = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *) &dereg_top);
	if (ret < 0) {
		pr_err("%s: Deregister topologies failed %d\n",
			__func__, ret);
		goto done;
	}

	ret = wait_event_timeout(q6core_lcl.bus_bw_req_wait,
				(q6core_lcl.bus_bw_resp_received == 1),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout for Deregister topologies\n",
			__func__);
		goto done;
	}
done:
	return ret;
}

static int q6core_send_custom_topologies(void)
{
	int ret = 0;
	int ret2 = 0;
	struct cal_block_data *cal_block = NULL;
	struct avcs_cmd_register_topologies reg_top;

	if (!q6core_is_adsp_ready()) {
		pr_err("%s: ADSP is not ready!\n", __func__);
		return -ENODEV;
	}

	memset(&reg_top, 0, sizeof(reg_top));
	mutex_lock(&q6core_lcl.cal_data[CUST_TOP_CAL]->lock);
	mutex_lock(&q6core_lcl.cmd_lock);

	cal_block = cal_utils_get_only_cal_block(
		q6core_lcl.cal_data[CUST_TOP_CAL]);
	if (cal_block == NULL) {
		pr_debug("%s: cal block is NULL!\n", __func__);
		goto unlock;
	}
	if (cal_block->cal_data.size <= 0) {
		pr_debug("%s: cal size is %zd not sending\n",
			__func__, cal_block->cal_data.size);
		goto unlock;
	}

	q6core_dereg_all_custom_topologies();

	ret = q6core_map_memory_regions(&cal_block->cal_data.paddr,
		ADSP_MEMORY_MAP_SHMEM8_4K_POOL,
		(uint32_t *)&cal_block->map_data.map_size, 1,
		&cal_block->map_data.q6map_handle);
	if (ret)  {
		pr_err("%s: q6core_map_memory_regions failed\n", __func__);
		goto unlock;
	}

	reg_top.hdr.hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	reg_top.hdr.pkt_size = sizeof(reg_top);
	reg_top.hdr.src_svc = APR_SVC_ADSP_CORE;
	reg_top.hdr.src_domain = APR_DOMAIN_APPS;
	reg_top.hdr.src_port = 0;
	reg_top.hdr.dest_svc = APR_SVC_ADSP_CORE;
	reg_top.hdr.dest_domain = APR_DOMAIN_ADSP;
	reg_top.hdr.dest_port = 0;
	reg_top.hdr.token = 0;
	reg_top.hdr.opcode = AVCS_CMD_REGISTER_TOPOLOGIES;
	reg_top.payload_addr_lsw =
		lower_32_bits(cal_block->cal_data.paddr);
	reg_top.payload_addr_msw =
		msm_audio_populate_upper_32_bits(cal_block->cal_data.paddr);
	reg_top.mem_map_handle = cal_block->map_data.q6map_handle;
	reg_top.payload_size = cal_block->cal_data.size;

	q6core_lcl.adsp_status = 0;
	q6core_lcl.bus_bw_resp_received = 0;

	pr_debug("%s: Register topologies addr %pK, size %zd, map handle %d\n",
		__func__, &cal_block->cal_data.paddr, cal_block->cal_data.size,
		cal_block->map_data.q6map_handle);

	ret = apr_send_pkt(q6core_lcl.core_handle_q, (uint32_t *) &reg_top);
	if (ret < 0) {
		pr_err("%s: Register topologies failed %d\n",
			__func__, ret);
		goto unmap;
	}

	ret = wait_event_timeout(q6core_lcl.bus_bw_req_wait,
				(q6core_lcl.bus_bw_resp_received == 1),
				msecs_to_jiffies(TIMEOUT_MS));
	if (!ret) {
		pr_err("%s: wait_event timeout for Register topologies\n",
			__func__);
		goto unmap;
	}

	if (q6core_lcl.adsp_status < 0)
		ret = q6core_lcl.adsp_status;
unmap:
	ret2 = q6core_memory_unmap_regions(cal_block->map_data.q6map_handle);
	if (ret2)  {
		pr_err("%s: q6core_memory_unmap_regions failed for map handle %d\n",
			__func__, cal_block->map_data.q6map_handle);
		ret = ret2;
		goto unlock;
	}

unlock:
	mutex_unlock(&q6core_lcl.cmd_lock);
	mutex_unlock(&q6core_lcl.cal_data[CUST_TOP_CAL]->lock);

	return ret;
}

static int get_cal_type_index(int32_t cal_type)
{
	int ret = -EINVAL;

	switch (cal_type) {
	case AUDIO_CORE_METAINFO_CAL_TYPE:
		ret = META_CAL;
		break;
	case CORE_CUSTOM_TOPOLOGIES_CAL_TYPE:
		ret = CUST_TOP_CAL;
		break;
	default:
		pr_err("%s: invalid cal type %d!\n", __func__, cal_type);
	}
	return ret;
}

static int q6core_alloc_cal(int32_t cal_type,
			    size_t data_size, void *data)
{
	int ret = 0;
	int cal_index;

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}


	ret = cal_utils_alloc_cal(data_size, data,
		q6core_lcl.cal_data[cal_index], 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_alloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		goto done;
	}
done:
	return ret;
}

static int q6core_dealloc_cal(int32_t cal_type,
			      size_t data_size, void *data)
{
	int ret = 0;
	int cal_index;

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}


	ret = cal_utils_dealloc_cal(data_size, data,
					q6core_lcl.cal_data[cal_index]);
	if (ret < 0) {
		pr_err("%s: cal_utils_dealloc_block failed, ret = %d, cal type = %d!\n",
			__func__, ret, cal_type);
		goto done;
	}
done:
	return ret;
}

static int q6core_set_cal(int32_t cal_type,
	size_t data_size, void *data)
{
	int ret = 0;
	int cal_index;

	cal_index = get_cal_type_index(cal_type);
	if (cal_index < 0) {
		pr_err("%s: could not get cal index %d!\n",
			__func__, cal_index);
		ret = -EINVAL;
		goto done;
	}


	ret = cal_utils_set_cal(data_size, data,
				    q6core_lcl.cal_data[cal_index], 0, NULL);
	if (ret < 0) {
		pr_err("%s: cal_utils_set_cal failed, ret = %d, cal type = %d!\n",
		__func__, ret, cal_type);
		goto done;
	}

	if (cal_index == CUST_TOP_CAL)
		ret = q6core_send_custom_topologies();
done:
	return ret;
}

static void q6core_delete_cal_data(void)
{
	pr_debug("%s:\n", __func__);

	cal_utils_destroy_cal_types(CORE_MAX_CAL, q6core_lcl.cal_data);
}


static int q6core_init_cal_data(void)
{
	int ret = 0;
	struct cal_type_info    cal_type_info[] = {
		{{AUDIO_CORE_METAINFO_CAL_TYPE,
		{q6core_alloc_cal, q6core_dealloc_cal, NULL,
		q6core_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} },

		{{CORE_CUSTOM_TOPOLOGIES_CAL_TYPE,
		{q6core_alloc_cal, q6core_dealloc_cal, NULL,
		q6core_set_cal, NULL, NULL} },
		{NULL, NULL, cal_utils_match_buf_num} }
	};
	pr_debug("%s:\n", __func__);

	ret = cal_utils_create_cal_types(CORE_MAX_CAL,
		q6core_lcl.cal_data, cal_type_info);
	if (ret < 0) {
		pr_err("%s: could not create cal type!\n",
			__func__);
		goto err;
	}

	return ret;
err:
	q6core_delete_cal_data();
	return ret;
}

int q6core_is_avs_up(int32_t *avs_state)
{
	unsigned long timeout;
	int32_t adsp_ready = 0;
	int ret = 0;

	timeout = jiffies +
		msecs_to_jiffies(ADSP_STATE_READY_TIMEOUT_MS);

	/* sleep for 100ms before querying AVS up */
	msleep(100);
	do {
		adsp_ready = q6core_is_adsp_ready();
		pr_debug("%s: ADSP Audio is %s\n", __func__,
			 adsp_ready ? "ready" : "not ready");
		if (adsp_ready)
			break;

		/*
		 * ADSP will be coming up after boot up and AVS might
		 * not be fully up when the control reaches here.
		 * So, wait for 50msec before checking ADSP state again.
		 */
		msleep(50);
	} while (time_after(timeout, jiffies));

	*avs_state = adsp_ready;
	pr_debug("%s: ADSP Audio is %s\n", __func__,
	       adsp_ready ? "ready" : "not ready");

	if (!adsp_ready) {
		pr_err_ratelimited("%s: Timeout. ADSP Audio is not ready\n",
				   __func__);
		ret = -ETIMEDOUT;
	}

	return ret;
}
EXPORT_SYMBOL(q6core_is_avs_up);

static int q6core_ssr_enable(struct device *dev, void *data)
{
	int32_t avs_state = 0;
	int ret = 0;

	if (!dev) {
		pr_err("%s: dev is NULL\n", __func__);
		return -EINVAL;
	}

	if (!q6core_lcl.avs_state) {
		ret = q6core_is_avs_up(&avs_state);
		if (ret < 0)
			goto err;
		q6core_lcl.avs_state = avs_state;
	}

err:
	return ret;
}

static void q6core_ssr_disable(struct device *dev, void *data)
{
	/* Reset AVS state to 0 */
	q6core_lcl.avs_state = 0;
}

static const struct snd_event_ops q6core_ssr_ops = {
	.enable = q6core_ssr_enable,
	.disable = q6core_ssr_disable,
};

static int q6core_probe(struct platform_device *pdev)
{
	int32_t avs_state = 0;
	int rc = 0;

	rc = q6core_is_avs_up(&avs_state);
	if (rc < 0)
		goto err;
	q6core_lcl.avs_state = avs_state;

	rc = of_platform_populate(pdev->dev.of_node, NULL, NULL, &pdev->dev);
	if (rc) {
		dev_err(&pdev->dev, "%s: failed to add child nodes, rc=%d\n",
			__func__, rc);
		rc = -EINVAL;
		goto err;
	}
	dev_dbg(&pdev->dev, "%s: added child node\n", __func__);

	rc = snd_event_client_register(&pdev->dev, &q6core_ssr_ops, NULL);
	if (!rc) {
		snd_event_notify(&pdev->dev, SND_EVENT_UP);
	} else {
		dev_err(&pdev->dev,
			"%s: Registration with SND event fwk failed rc = %d\n",
			__func__, rc);
		rc = 0;
	}

err:
	return rc;
}

static int q6core_remove(struct platform_device *pdev)
{
	snd_event_client_deregister(&pdev->dev);
	of_platform_depopulate(&pdev->dev);
	return 0;
}

static const struct of_device_id q6core_of_match[]  = {
	{ .compatible = "qcom,q6core-audio", },
	{},
};

static struct platform_driver q6core_driver = {
	.probe = q6core_probe,
	.remove = q6core_remove,
	.driver = {
		.name = "q6core_audio",
		.owner = THIS_MODULE,
		.of_match_table = q6core_of_match,
	}
};

int __init core_init(void)
{
	memset(&q6core_lcl, 0, sizeof(struct q6core_str));
	init_waitqueue_head(&q6core_lcl.bus_bw_req_wait);
	init_waitqueue_head(&q6core_lcl.cmd_req_wait);
	init_waitqueue_head(&q6core_lcl.avcs_fwk_ver_req_wait);
	q6core_lcl.cmd_resp_received_flag = FLAG_NONE;
	mutex_init(&q6core_lcl.cmd_lock);
	mutex_init(&q6core_lcl.ver_lock);

	q6core_init_cal_data();
	q6core_init_uevent_kset();

	return platform_driver_register(&q6core_driver);
}

void core_exit(void)
{
	mutex_destroy(&q6core_lcl.cmd_lock);
	mutex_destroy(&q6core_lcl.ver_lock);
	q6core_delete_cal_data();
	q6core_destroy_uevent_kset();
	platform_driver_unregister(&q6core_driver);
}
MODULE_DESCRIPTION("ADSP core driver");
MODULE_LICENSE("GPL v2");
