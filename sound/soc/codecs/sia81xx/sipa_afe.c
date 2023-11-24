/*
 * Copyright (C) 2018, SI-IN, Yun Shi (yun.shi@si-in.com).
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

#define DEBUG
#define LOG_FLAG	"sipa_afe"

#include <linux/slab.h>
#include <linux/wait.h>
//#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 4, 1))
#include <dsp/msm_audio_ion.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6audio-v2.h>

#else
#include <linux/msm_audio_ion.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
#include <ipc/apr.h>
#endif

#include "sipa_tuning_if.h"

#define TIMEOUT_MS 				(1000) /* the same to q6afe.c */
#define CAL_BLOCK_MAP_SIZE		(SZ_4K)

static DEFINE_MUTEX(sipa_afe_list_mutex);
static LIST_HEAD(sipa_afe_list);

struct sipa_cal_block_data {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 12))
	/* although now, the field is "struct dma_buf *",
	 * but maybe after linux 5.1.x, this field while be changed to "int fd"
	 * so, let this field as "long long" type for compatible consider */
	long long fd;
#else
	struct ion_client	*ion_client;
	struct ion_handle	*ion_handle;
#endif

	void				*kvaddr;
	phys_addr_t			paddr;
	size_t				pa_size;

	uint32_t			dsp_mmap_handle;
};

typedef struct sipa_afe {
	uint16_t afe_port_id;
	void *apr;
	wait_queue_head_t wait;
	atomic_t state;
	struct sipa_cal_block_data cal_block;

	struct list_head list;
	struct mutex afe_lock;
} SIPA_AFE;

/* unknown qcom modify apr_audio at which kernel version,
 * but must been modified at 4.14.0 + */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
typedef struct afe_port_cmd_set_param_v2 afe_cmd_sipa_set_cal;
typedef struct afe_port_cmd_get_param_v2 afe_cmd_sipa_get_cal;
typedef struct param_hdr_v1 afe_cmd_sipa_param_payload;
#else
typedef struct __afe_cmd_sipa_set_cal {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
} __packed afe_cmd_sipa_set_cal;

typedef struct __afe_cmd_sipa_get_cal {
	struct apr_hdr hdr;
	struct afe_port_cmd_get_param_v2 param;
} __packed afe_cmd_sipa_get_cal;

typedef struct afe_port_param_data_v2 afe_cmd_sipa_param_payload;
#endif

typedef struct __afe_cmd_sipa_set_mmap {
	struct afe_service_cmd_shared_mem_map_regions regions;
	struct  afe_service_shared_map_region_payload payload;
} __packed afe_cmd_sipa_set_mmap;

#ifdef SIPA_COMPILE_TO_MODULE
extern struct apr_svc *apr_register(
	char *dest,
	char *svc_name,
	apr_fn svc_fn,
	uint32_t src_port,
	void *priv);

extern int apr_send_pkt(
	void *handle,
	uint32_t *buf);

extern int msm_audio_ion_alloc(
	struct dma_buf **dma_buf,
	size_t bufsz,
	dma_addr_t *paddr,
	size_t *plen,
	void **vaddr);

extern int msm_audio_ion_free(
	struct dma_buf *dma_buf);

extern int apr_deregister(
	void *handle);
#endif

static int sipa_apr_cmd_memory_map(SIPA_AFE *afe);


static int afe_port_to_apr_port(
	uint16_t afe_port)
{
	/* range: 0x10 ~ APR_MAX_PORTS�� to avoid the port : 0x00 */
	return (afe_port % (APR_MAX_PORTS - 0x10)) + 0x10;
}

static int sipa_fill_apr_hdr(
	struct apr_hdr *apr_hdr,
	uint32_t port,
	uint32_t opcode,
	uint32_t apr_msg_size)
{
	if (apr_hdr == NULL) {
		pr_err("[  err][%s] %s: invalid APR pointer \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	apr_hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD,
		APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	apr_hdr->pkt_size = apr_msg_size; /* total len, include the hdr */
	apr_hdr->src_svc = APR_SVC_AFE;
	apr_hdr->src_domain = APR_DOMAIN_APPS;
	apr_hdr->src_port = port;	/* apr port id, dsp will use this value */
								/*as dest_port when response this cmd */
	apr_hdr->dest_svc = APR_SVC_AFE;
	apr_hdr->dest_domain = APR_DOMAIN_ADSP;
	apr_hdr->dest_port = 0;
	apr_hdr->token = port;
	apr_hdr->opcode = opcode;

	return 0;
}


static int32_t sipa_afe_callback(
	struct apr_client_data *data,
	void *priv)
{

	SIPA_AFE *afe = NULL;

	if (!data) {
		pr_err("[  err][%s] %s: invalid param data \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (!priv) {
		pr_err("[  err][%s] %s: invalid param priv \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	/* priv must a SIPA_AFE type pointer */
	afe = (SIPA_AFE *)priv;

	if (RESET_EVENTS == data->opcode) {
		pr_info("[ info][%s] %s: RESET_EVENTS, event : %d !! \r\n",
			LOG_FLAG, __func__, (int)data->reset_event);

		afe->cal_block.dsp_mmap_handle = 0;

		if (afe->apr) {
			apr_reset(afe->apr);
			atomic_set(&afe->state, 0);
			afe->apr = NULL;
		}
	}

	switch (data->opcode) {
	case AFE_SERVICE_CMDRSP_SHARED_MEM_MAP_REGIONS:
	{
		struct afe_service_cmdrsp_shared_mem_map_regions *payload = NULL;

		if (!data->payload_size) {
			pr_err("[  err][%s] %s: invalid payload_size = %d "
				"with opcode = 0x%08x \r\n",
				LOG_FLAG, __func__, data->payload_size, data->opcode);
			break;
		}

		payload =
			(struct afe_service_cmdrsp_shared_mem_map_regions *)data->payload;
		afe->cal_block.dsp_mmap_handle = payload->mem_map_handle;

		atomic_set(&afe->state, 0);
		wake_up(&afe->wait);

		break;
	}
	case AFE_PORT_CMDRSP_GET_PARAM_V2:
	{
		atomic_set(&afe->state, 0);
		wake_up(&afe->wait);

		break;
	}
	case APR_BASIC_RSP_RESULT:
	{
		uint32_t *payload = NULL;

		if (!data->payload_size) {
			pr_err("[  err][%s] %s: invalid payload_size = %d "
				"with opcode = 0x%08x \r\n",
				LOG_FLAG, __func__, data->payload_size, data->opcode);
			break;
		}

		payload = (uint32_t *)data->payload;
		switch (payload[0]) {
		case AFE_PORT_CMD_SET_PARAM_V2:
		{
			atomic_set(&afe->state, 0);
			wake_up(&afe->wait);

			break;
		}
		case AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS:
		{
			afe->cal_block.dsp_mmap_handle = 0;

			atomic_set(&afe->state, 0);
			wake_up(&afe->wait);

			break;
		}
		default:
			pr_err("[  err][%s] %s: unknow payload[0] = 0x%08x "
				"with opcode = 0x%08x \r\n",
			LOG_FLAG, __func__, payload[0], data->opcode);
			break;
		}

		break;
	}
	default:
		pr_err("[  err][%s] %s: invalid opcode = 0x%08x \r\n",
			LOG_FLAG, __func__, data->opcode);
		break;
	}

	return 0;
}

static int sipa_apr_send(
	SIPA_AFE *afe,
	void *cmd)
{
	int ret = 0;

	/* is afe sending */
	if (1 == atomic_read(&afe->state)) {
		ret = -EBUSY;
		goto err;
	}

	/* set afe useing flag */
	atomic_set(&afe->state, 1);

	/* sending the cmd as a apr msg */
	ret = apr_send_pkt(afe->apr, (uint32_t *)cmd);
	if (ret < 0) {
		ret = -EINVAL;
		goto err;
	}

	/* timeout protect, avoid wating forever */
	if (!(wait_event_timeout(afe->wait,
				 (atomic_read(&afe->state) == 0),
				 msecs_to_jiffies(TIMEOUT_MS)))) {
		ret = -ENOTBLK;
		goto err;
	}

	return ret;

err:
	pr_err("[  err][%s] %s: error ret = %d \r\n",
		LOG_FLAG, __func__, ret);
	return ret;
}

static int check_apr(
	SIPA_AFE *afe)
{
	int ret = 0;

	if (NULL == afe->apr) {
		afe->apr = apr_register("ADSP", "AFE", sipa_afe_callback,
			afe_port_to_apr_port(afe->afe_port_id), afe);
		if (NULL == afe->apr) {
			pr_err("[  err][%s] %s: Unable to register AFE \r\n",
				LOG_FLAG, __func__);
			ret = -EINVAL;
		}
	}

	return ret;
}

static int check_cal_block(
	SIPA_AFE *afe)
{
	int ret = 0;

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 12))
	if (0 == afe->cal_block.fd) {
		ret = msm_audio_ion_alloc((struct dma_buf **)(&(afe->cal_block.fd)),
				CAL_BLOCK_MAP_SIZE, &(afe->cal_block.paddr),
				&(afe->cal_block.pa_size), &(afe->cal_block.kvaddr));
#else
	if (NULL == afe->cal_block.ion_handle) {
		ret = msm_audio_ion_alloc("sia81xx_cal",
				&(afe->cal_block.ion_client),
				&(afe->cal_block.ion_handle), CAL_BLOCK_MAP_SIZE,
				&(afe->cal_block.paddr), &(afe->cal_block.pa_size),
				&(afe->cal_block.kvaddr));
#endif
		if (ret < 0) {
			pr_err("[  err][%s] %s: allocate buffer failed! ret = %d\n \r\n",
				LOG_FLAG, __func__, ret);
			goto err;
		} else {
			pr_info("[ info][%s] %s: afe[0x%04x] allocate buffer success! "
				"pa_size = %zu \r\n",
				LOG_FLAG, __func__, afe->afe_port_id,
				afe->cal_block.pa_size);
		}
	}

	if (0 == afe->cal_block.dsp_mmap_handle) {
		ret = sipa_apr_cmd_memory_map(afe);
		if (ret != 0) {
			pr_err("[  err][%s] %s: map buffer failed! ret = %d\n \r\n",
				LOG_FLAG, __func__, ret);
			goto err;
		} else {
			pr_info("[ info][%s] %s: afe[0x%04x] map buffer success! "
				"handle = 0x%08x \r\n",
				LOG_FLAG, __func__, afe->afe_port_id,
				afe->cal_block.dsp_mmap_handle);
		}
	}

	return ret;

err:
	return ret;
}

static int check_afe_port_id(
	SIPA_AFE *afe)
{
	int ret = 0;

	if (afe_port_to_apr_port(afe->afe_port_id) >= APR_MAX_PORTS) {
		pr_err("[  err][%s] %s: invalid AFE port = 0x%04x \r\n",
			LOG_FLAG, __func__, afe->afe_port_id);
		ret = -EINVAL;
	}

	return ret;
}

static inline int chack_afe_validity(
	SIPA_AFE *afe)
{
	if (NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe !! \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (0 != check_apr(afe)) {
		pr_err("[  err][%s] %s: check_apr failed !! \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (0 != check_cal_block(afe)) {
		pr_err("[  err][%s] %s: check_cal_block failed !! \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	if (0 != check_afe_port_id(afe)) {
		pr_err("[err][%s] %s: check_afe_port_id failed !! \r\n",
			LOG_FLAG, __func__);
		return -EINVAL;
	}

	return 0;
}

static void sipa_write_param_to_payload(
	afe_cmd_sipa_param_payload *payload,
	uint32_t module_id,
	uint32_t param_id,
	uint32_t param_size,
	uint8_t *param)
{
	payload->module_id = module_id;
	payload->param_id = param_id;
	payload->param_size = param_size;
	payload->reserved = 0;

	memcpy((void *)(payload + 1), param, payload->param_size);
}

static int sipa_read_param_from_payload(
	afe_cmd_sipa_param_payload *payload,
	uint32_t module_id,
	uint32_t param_id,
	uint32_t param_size,
	uint8_t *param)
{
	if (payload->module_id != module_id) {
		pr_err("[  err][%s] %s: unmatched module_id 0x%08x, "
			"payload->module_id 0x%08x \r\n",
			LOG_FLAG, __func__, module_id, payload->module_id);
		return -EINVAL;
	}

	if (payload->param_id != param_id) {
		pr_err("[  err][%s] %s: unmatched param_id 0x%08x, "
			"payload->param_id 0x%08x \r\n",
			LOG_FLAG, __func__, param_id, payload->param_id);
		return -EINVAL;
	}

	if (payload->param_size > param_size) {
		pr_err("[  err][%s] %s: unmatched param_size 0x%08x, "
			"payload->param_size 0x%08x \r\n",
			LOG_FLAG, __func__, param_size, payload->param_size);
		return -EINVAL;
	}

	memcpy(param, (void *)(payload + 1), payload->param_size);

	return payload->param_size;
}


static int sipa_apr_cmd_memory_map(
	SIPA_AFE *afe)
{
	int ret = 0;
	afe_cmd_sipa_set_mmap apr_cmd;

	if (NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe !! \r\n",
			LOG_FLAG, __func__);
		ret = -EINVAL;
		goto err;
	}

	if (0 != check_apr(afe)) {
		pr_err("[  err][%s] %s: check_apr failed !! \r\n",
			LOG_FLAG, __func__);
		ret = -EINVAL;
		goto err;
	}

	/* clear cmd */
	memset((void *)(&apr_cmd), 0x00, sizeof(apr_cmd));

	/* fill cmd head */
	sipa_fill_apr_hdr(&(apr_cmd.regions.hdr),
		afe_port_to_apr_port(afe->afe_port_id),
		AFE_SERVICE_CMD_SHARED_MEM_MAP_REGIONS, sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.regions.mem_pool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL;
	apr_cmd.regions.num_regions = 1;
	apr_cmd.regions.property_flag = 0x00;

	apr_cmd.payload.shm_addr_lsw = lower_32_bits(afe->cal_block.paddr);
	apr_cmd.payload.shm_addr_msw =
		msm_audio_populate_upper_32_bits(afe->cal_block.paddr);
	apr_cmd.payload.mem_size_bytes = afe->cal_block.pa_size;

	/* clear old dsp mmp value, and wait a new value return from dsp,
	 *  if the cmd command success */
	afe->cal_block.dsp_mmap_handle = 0;

	/* send the cmd to dsp through apr, and wait dsp returned msg */
	if (0 > sipa_apr_send(afe, &apr_cmd)) {
		goto err;
	}

	pr_debug("[debug][%s] %s: dsp map success !! ret = %d \r\n",
		LOG_FLAG, __func__, ret);

	return ret;

err:
	pr_err("[  err][%s] %s: error ret = %d \r\n",
		LOG_FLAG, __func__, ret);
	return ret;
}

static int sipa_apr_cmd_memory_unmap(
	SIPA_AFE *afe)
{
	int ret = 0;
	struct afe_service_cmd_shared_mem_unmap_regions apr_cmd;

	if (NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe !! \r\n",
			LOG_FLAG, __func__);
		ret = -EINVAL;
		goto err;
	}

	if (0 == afe->cal_block.dsp_mmap_handle)
		return 0;

	if (0 != check_apr(afe)) {
		pr_err("[  err][%s] %s: check_apr failed !! \r\n",
			LOG_FLAG, __func__);
		ret = -EINVAL;
		goto err;
	}

	/* clear cmd */
	memset((void *)(&apr_cmd), 0x00, sizeof(apr_cmd));

	/* fill cmd head */
	sipa_fill_apr_hdr(
			&(apr_cmd.hdr),
			afe_port_to_apr_port(afe->afe_port_id),
			AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS,
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.mem_map_handle = afe->cal_block.dsp_mmap_handle;

	/* send the cmd to dsp through apr, and wait dsp returned msg */
	if (0 > sipa_apr_send(afe, &apr_cmd)) {
		goto err;
	}

	pr_debug("[debug][%s] %s: dsp unmap success !! ret = %d \r\n",
		LOG_FLAG, __func__, ret);

	return ret;

err:
	pr_err("[  err][%s] %s: error ret = %d \r\n",
		LOG_FLAG, __func__, ret);
	return ret;
}

static unsigned int sipa_afe_list_count(void)
{
	unsigned count = 0;
	SIPA_AFE *afe = NULL;

	mutex_lock(&sipa_afe_list_mutex);

	list_for_each_entry(afe, &sipa_afe_list, list) {
		count++;
	}

	mutex_unlock(&sipa_afe_list_mutex);

	return count;
}

static SIPA_AFE *find_sipa_afe_list(
	uint16_t afe_port_id)
{
	SIPA_AFE *afe = NULL;

	mutex_lock(&sipa_afe_list_mutex);

	list_for_each_entry(afe, &sipa_afe_list, list) {
		if (afe_port_id == afe->afe_port_id) {
			mutex_unlock(&sipa_afe_list_mutex);
			return afe;
		}
	}

	mutex_unlock(&sipa_afe_list_mutex);

	return NULL;
}

static void add_sipa_afe_list(
	SIPA_AFE *afe)
{
	if (NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe \r\n", LOG_FLAG, __func__);
		return ;
	}

	if (NULL != find_sipa_afe_list(afe->afe_port_id))
		return ;

	mutex_lock(&sipa_afe_list_mutex);
	list_add(&afe->list, &sipa_afe_list);
	mutex_unlock(&sipa_afe_list_mutex);

	pr_debug("[debug][%s] %s: add afe port id : %u, count = %u \r\n",
		LOG_FLAG, __func__, afe->afe_port_id, sipa_afe_list_count());
}

static void del_sipa_afe_list(
	SIPA_AFE *afe)
{
	if (NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe \r\n", LOG_FLAG, __func__);
		return ;
	}

	pr_debug("[debug][%s] %s: del fe port id : %u, count = %u \r\n",
		LOG_FLAG, __func__, afe->afe_port_id, sipa_afe_list_count());

	mutex_lock(&sipa_afe_list_mutex);
	list_del(&afe->list);
	mutex_unlock(&sipa_afe_list_mutex);
}

int sipa_afe_send(
	unsigned long afe_handle,
	uint32_t module_id,
	uint32_t param_id,
	uint32_t param_size,
	uint8_t *buf)
{
	int ret = 0;
	afe_cmd_sipa_set_cal apr_cmd;
	SIPA_AFE *afe = (SIPA_AFE *)afe_handle;

	if (NULL == afe)
		return -EINVAL;

	if (APR_SUBSYS_LOADED != apr_get_q6_state()) {
		pr_err("[  err][%s] %s: q6_state : %u \r\n",
			LOG_FLAG, __func__, (unsigned int)apr_get_q6_state());
		return -EINVAL;
	}

	mutex_lock(&afe->afe_lock);

	ret = chack_afe_validity(afe);
	if (0 != ret) {
		pr_err("[  err][%s] %s: chack_afe_validity failed !! \r\n",
			LOG_FLAG, __func__);
		goto err;
	}

	/* is the pa size has a capacity of holding the param */
	if (param_size >
		(afe->cal_block.pa_size - sizeof(afe_cmd_sipa_param_payload))) {
		pr_err("[  err][%s] %s: invalid payload size = %d \r\n",
			LOG_FLAG, __func__, param_size);
		ret = -EINVAL;
		goto err;
	}

	/* write param to out-of-band payload */
	sipa_write_param_to_payload(
		(afe_cmd_sipa_param_payload *)(afe->cal_block.kvaddr),
		module_id,
		param_id,
		param_size,
		buf);

	/* clear cmd */
	memset((void *)(&apr_cmd), 0x00, sizeof(apr_cmd));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
	/* fill cmd head */
	sipa_fill_apr_hdr(
			(struct apr_hdr *)(&apr_cmd.apr_hdr),
			afe_port_to_apr_port(afe->afe_port_id),
			AFE_PORT_CMD_SET_PARAM_V2,
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.port_id = afe->afe_port_id;
	apr_cmd.payload_size =
		param_size + sizeof(afe_cmd_sipa_param_payload);
	apr_cmd.mem_hdr.data_payload_addr_lsw =
		lower_32_bits(afe->cal_block.paddr);
	apr_cmd.mem_hdr.data_payload_addr_msw =
		msm_audio_populate_upper_32_bits(afe->cal_block.paddr);
	apr_cmd.mem_hdr.mem_map_handle = afe->cal_block.dsp_mmap_handle;
#else
	/* fill cmd head */
	sipa_fill_apr_hdr(
			(struct apr_hdr *)(&apr_cmd.hdr),
			afe_port_to_apr_port(afe->afe_port_id),
			AFE_PORT_CMD_SET_PARAM_V2,
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.param.port_id = afe->afe_port_id;
	apr_cmd.param.payload_size =
		param_size + sizeof(afe_cmd_sipa_param_payload);
	apr_cmd.param.payload_address_lsw =
		lower_32_bits(afe->cal_block.paddr);
	apr_cmd.param.payload_address_msw =
		msm_audio_populate_upper_32_bits(afe->cal_block.paddr);
	apr_cmd.param.mem_map_handle = afe->cal_block.dsp_mmap_handle;
#endif

	/* send the cmd to dsp through apr, and wait dsp returned msg */
	ret = sipa_apr_send(afe, &apr_cmd);
	if (0 > ret) {
		goto err;
	}

	pr_debug("[debug][%s] %s: afe send ret = %d, handle 0x%08x, "
		"module_id 0x%08x, param_id 0x%08x, size %d \r\n",
		LOG_FLAG, __func__, ret, afe->cal_block.dsp_mmap_handle,
		module_id, param_id, param_size);

	mutex_unlock(&afe->afe_lock);

	return ret;

err:
	pr_err("[  err][%s] %s: afe send failed ret = %d, "
		"module_id 0x%08x, param_id 0x%08x, size %d \r\n",
		LOG_FLAG, __func__, ret, module_id, param_id, param_size);

	mutex_unlock(&afe->afe_lock);

	return ret;
}

int sipa_afe_read(
	unsigned long afe_handle,
	uint32_t module_id,
	uint32_t param_id,
	uint32_t param_size,
	uint8_t *buf)
{
	int ret = 0;
	afe_cmd_sipa_get_cal apr_cmd;
	SIPA_AFE *afe = (SIPA_AFE *)afe_handle;

	if (NULL == afe)
		return -EINVAL;

	if (APR_SUBSYS_LOADED != apr_get_q6_state()) {
		pr_err("[  err][%s] %s: q6_state : %u \r\n",
			LOG_FLAG, __func__, (unsigned int)apr_get_q6_state());
		return -EINVAL;
	}

	mutex_lock(&afe->afe_lock);

	ret = chack_afe_validity(afe);
	if (0 != ret) {
		pr_err("[  err][%s] %s: chack_afe_validity failed !! \r\n",
			LOG_FLAG, __func__);
		goto err;
	}

	/* is the pa size has a capacity of holding the param */
	if (param_size >
		(afe->cal_block.pa_size - sizeof(afe_cmd_sipa_param_payload))) {
		pr_err("[  err][%s] %s: invalid payload size = %d \r\n",
			LOG_FLAG, __func__, param_size);
		ret = -EINVAL;
		goto err;
	}

	/* write param to out-of-band payload */
	sipa_write_param_to_payload(
		(afe_cmd_sipa_param_payload *)(afe->cal_block.kvaddr),
		module_id,
		param_id,
		param_size,
		buf);

	/* clear cmd */
	memset((void *)(&apr_cmd), 0x00, sizeof(apr_cmd));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 14, 0))
	/* fill cmd head */
	sipa_fill_apr_hdr(
			(struct apr_hdr *)(&apr_cmd.apr_hdr),
			afe_port_to_apr_port(afe->afe_port_id),
			AFE_PORT_CMD_GET_PARAM_V2,
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.port_id = afe->afe_port_id;
	apr_cmd.payload_size =
		param_size + sizeof(afe_cmd_sipa_param_payload);
	apr_cmd.mem_hdr.data_payload_addr_lsw =
		lower_32_bits(afe->cal_block.paddr);
	apr_cmd.mem_hdr.data_payload_addr_msw =
		msm_audio_populate_upper_32_bits(afe->cal_block.paddr);
	apr_cmd.mem_hdr.mem_map_handle = afe->cal_block.dsp_mmap_handle;
	apr_cmd.module_id = module_id;
	apr_cmd.param_id = param_id;
	apr_cmd.param_hdr.module_id = module_id;
	apr_cmd.param_hdr.param_id = param_id;
	apr_cmd.param_hdr.param_size = param_size;
#else
	/* fill cmd head */
	sipa_fill_apr_hdr(
			(struct apr_hdr *)(&apr_cmd.hdr),
			afe_port_to_apr_port(afe->afe_port_id),
			AFE_PORT_CMD_GET_PARAM_V2,
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.param.port_id = afe->afe_port_id;
	apr_cmd.param.payload_size =
		param_size + sizeof(afe_cmd_sipa_param_payload);
	apr_cmd.param.payload_address_lsw =
		lower_32_bits(afe->cal_block.paddr);
	apr_cmd.param.payload_address_msw =
		msm_audio_populate_upper_32_bits(afe->cal_block.paddr);
	apr_cmd.param.mem_map_handle = afe->cal_block.dsp_mmap_handle;
	apr_cmd.param.module_id = module_id;
	apr_cmd.param.param_id = param_id;
#endif

	/* send the cmd to dsp through apr, and wait dsp returned msg */
	ret = sipa_apr_send(afe, &apr_cmd);
	if (0 > ret) {
		goto err;
	}

	/* read data from dsp returned, and copy this to the user buf */
	ret = sipa_read_param_from_payload(
			(afe_cmd_sipa_param_payload *)(afe->cal_block.kvaddr),
			module_id,
			param_id,
			param_size,
			buf);
	if (0 > ret) {
		goto err;
	}

	pr_debug("[debug][%s] %s: afe read ret = %d, handle 0x%08x, "
		"module_id 0x%08x, param_id 0x%08x, size %d \r\n",
		LOG_FLAG, __func__, ret, afe->cal_block.dsp_mmap_handle,
		module_id, param_id, param_size);

	mutex_unlock(&afe->afe_lock);

	return ret;

err:
	pr_err("[  err][%s] %s: afe read failed ret = %d, "
		"module_id 0x%08x, param_id 0x%08x, size %d \r\n",
		LOG_FLAG, __func__, ret, module_id, param_id, param_size);

	mutex_unlock(&afe->afe_lock);

	return ret;
}

unsigned long sipa_afe_open(
	uint32_t afe_prot_id)
{
	SIPA_AFE *afe;

	/* when open afe, maybe q6 has not start,
	 * so must be considered this case.
	 * 0 == apr_get_q6_state() */

	afe = find_sipa_afe_list(afe_prot_id);
	if (NULL != afe)
		return (unsigned long)afe;

	afe = kzalloc(sizeof(SIPA_AFE), GFP_KERNEL);
	if (NULL == afe) {
		pr_err("[  err][%s] %s: kzalloc afe failed !! \r\n",
			LOG_FLAG, __func__);
		return 0;
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 12))
	afe->cal_block.fd = 0;
#else
	afe->cal_block.ion_client = NULL;
	afe->cal_block.ion_handle = NULL;
#endif

	atomic_set(&(afe->state), 0);
	init_waitqueue_head(&(afe->wait));

	afe->afe_port_id = afe_prot_id;
	mutex_init(&afe->afe_lock);

	add_sipa_afe_list(afe);

	pr_info("[ info][%s] %s: afe open success !! "
		"afe prot : 0x%04x \r\n",
		LOG_FLAG, __func__, afe_prot_id);

	return (unsigned long)afe;
}

int sipa_afe_close(
	unsigned long afe_handle)
{
	int ret = 0;
	SIPA_AFE *afe = (SIPA_AFE *)afe_handle;

	if (NULL == afe) {
		pr_warn("[ warn][%s] %s: NULL == afe \r\n",
			LOG_FLAG, __func__);
		return 0;
	}

	pr_info("[ info][%s] %s: afe closeing !! "
		"afe prot : 0x%04x \r\n",
		LOG_FLAG, __func__, afe->afe_port_id);

	/* to ask dsp unmap memory */
	ret = sipa_apr_cmd_memory_unmap(afe);
	if (0 != ret) {
		pr_err("[  err][%s] %s: unmap error, close failed !! ret = %d \r\n",
			LOG_FLAG, __func__, ret);
		return ret;
	}

	/* free ion buf */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4, 11, 12))
	if (0 != afe->cal_block.fd) {
		ret = msm_audio_ion_free((struct dma_buf *)afe->cal_block.fd);
		if (0 != ret) {
			pr_err("[  err][%s] %s: ion free error, close failed !! "
				"ret = %d \r\n",
				LOG_FLAG, __func__, ret);
			return ret;
		} else {
			afe->cal_block.fd = 0;
		}
	}
#else
	if ((NULL != afe->cal_block.ion_client) ||
		(NULL != afe->cal_block.ion_client)) {

		ret = msm_audio_ion_free(
			afe->cal_block.ion_client, afe->cal_block.ion_handle);
		if (0 != ret) {
			pr_err("[  err][%s] %s: ion free error, close failed !! "
				"ret = %d \r\n",
				LOG_FLAG, __func__, ret);
			return ret;
		} else {
			afe->cal_block.ion_client = NULL;
			afe->cal_block.ion_handle = NULL;
		}
	}
#endif

	/* unregister apr */
	ret = apr_deregister(afe->apr);
	if (0 != ret) {
		pr_err("[  err][%s] %s: 0 != apr_deregister, close failed !! \r\n",
			LOG_FLAG, __func__);
		return ret;
	} else {
		afe->apr = NULL;
	}

	del_sipa_afe_list(afe);

	kfree(afe);

	return 0;
}

static int sipa_afe_init(void)
{
	int ret = 0;

	pr_info("[ info][%s] %s: run !! \r\n",
		LOG_FLAG, __func__);

	return ret;
}

static void sipa_afe_exit(void)
{
	pr_info("[ info][%s] %s: run !! \r\n",
		LOG_FLAG, __func__);
}

struct sipa_cal_opt tuning_if_opt = {
	.init = sipa_afe_init,
	.exit = sipa_afe_exit,
	.open = sipa_afe_open,
	.close = sipa_afe_close,
	.read = sipa_afe_read,
	.write = sipa_afe_send
};

static long sipa_tuning_cmd_unlocked_ioctl(struct file *fp,
	unsigned int cmd, unsigned long arg)
{
    sipa_turning_t *priv = g_sipa_turning;
    struct dev_comm_data *cmd_up = NULL;
	struct dev_comm_data *cmd_down = NULL;
    int ret = 0;

	pr_info("[ info][%s] %s: enter\n", LOG_FLAG, __func__);

	switch (cmd) {
        case SIPA_TUNING_CTRL_WR_UP: {
                pr_info("[ info][%s] %s: write cmd\n", LOG_FLAG, __func__);

				if (copy_from_user(priv->cmdup.data, (void __user *)arg, sizeof(dev_comm_data_t))) {
					pr_err("[  err][%s] %s: copy from user failed\n", LOG_FLAG, __func__);
					return -EFAULT;
				}

				cmd_up = (struct dev_comm_data *)priv->cmdup.data;
				priv->cmdup.len = DEV_COMM_DATA_LEN(cmd_up);
				if (copy_from_user(priv->cmdup.data,  (void __user *)arg, priv->cmdup.len)) {
					pr_err("[  err][%s] %s: copy from user failed\n", LOG_FLAG, __func__);
					return -EFAULT;
				}

				cal_handle = tuning_if_opt.open(g_dyn_ud_vdd_port);
				if (0 == cal_handle) {
					pr_err("[  err][%s] %s: tuning_if_opt.open failed \r\n",
						LOG_FLAG, __func__);
					return -EINVAL;
				}

				if (cmd_up->opt == OPT_SET_CAL_VAL) {
					ret = tuning_if_opt.write(cal_handle, cmd_up->reserve,
								cmd_up->param_id, cmd_up->payload_size, cmd_up->payload);
					if (0 > ret) {
						pr_err("[  err][%s] %s: tuning_if_opt.write failed ret = %d \r\n",
							LOG_FLAG, __func__, ret);
						return -EINVAL;
					}
				} else if (cmd_up->opt == OPT_GET_CAL_VAL) {
					ret = tuning_if_opt.read(cal_handle, cmd_up->reserve,
									cmd_up->param_id, cmd_up->payload_size, cmd_up->payload);
					if (0 > ret) {
						pr_err("[  err][%s] %s: tuning_if_opt.read failed ret = %d \r\n",
							LOG_FLAG, __func__, ret);
						return -EINVAL;
					}

					cmd_down = (struct dev_comm_data *)priv->cmddown.data;
					cmd_down->opt = cmd_up->opt;
					cmd_down->param_id = cmd_up->param_id;
					cmd_down->payload_size = cmd_up->payload_size;
					memcpy(cmd_down->payload, cmd_up->payload, cmd_up->payload_size);
					priv->cmddown.len = DEV_COMM_DATA_LEN(cmd_down);

					priv->cmddown.flag = true;
					wake_up_interruptible(&priv->cmddown.wq);
				}
            }
            break;
        case SIPA_TUNING_CTRL_RD_UP: {

            }
        case SIPA_TUNING_CTRL_WR_DOWN: {

            }
            break;
        case SIPA_TUNING_CTRL_RD_DOWN: {
                ret = wait_event_interruptible(priv->cmddown.wq, priv->cmddown.flag);
                if (ret) {
					pr_err("[  err][%s] %s: wait_event failed\n", LOG_FLAG, __func__);
                    return -ERESTART; 
                }
                if (copy_to_user((void __user *)arg, priv->cmddown.data, priv->cmddown.len)) {
                    return -EFAULT;
                }
                priv->cmddown.flag = false;
                pr_info("[ info][%s] %s: read cmd, len:%d \n", LOG_FLAG, __func__, priv->cmddown.len);
            }
            break;
        default:
	        pr_info("[ info][%s] %s: unsuport cmd:0x%x\n", LOG_FLAG, __func__, cmd);
            return -EFAULT;
    }
	return 0;
}

struct file_operations sipa_turning_cmd_fops = {
    .owner = THIS_MODULE,
	.unlocked_ioctl = sipa_tuning_cmd_unlocked_ioctl,
	.compat_ioctl = sipa_tuning_cmd_unlocked_ioctl,
};

extern uint32_t g_dyn_ud_vdd_port;
static long sipa_tuning_tool_unlocked_ioctl(struct file *fp,
	unsigned int cmd, unsigned long arg)
{
	unsigned long cal_handle = 0;
    sipa_turning_t *priv = g_sipa_turning;
    struct dev_comm_data *tool_up = NULL;
	struct dev_comm_data *tool_down = NULL;
    int ret = 0;

	pr_info("[ info][%s] %s: enter\n", LOG_FLAG, __func__);

	switch (cmd) {
        case SIPA_TUNING_CTRL_WR_UP: {
				if (copy_from_user(priv->toolup.data, (void __user *)arg, sizeof(dev_comm_data_t))) {
					pr_err("[  err][%s] %s: copy from user failed\n", LOG_FLAG, __func__);
					return -EFAULT;
				}

				tool_up = (struct dev_comm_data *)priv->toolup.data;
				priv->toolup.len = DEV_COMM_DATA_LEN(tool_up);
				if (copy_from_user(priv->toolup.data,  (void __user *)arg, priv->toolup.len)) {
					pr_err("[  err][%s] %s: copy from user failed\n", LOG_FLAG, __func__);
					return -EFAULT;
				}

				pr_info("[ info][%s] %s: datalen:%lu payload len:%d\n", LOG_FLAG, __func__, len, priv->toolup.len);

				cal_handle = tuning_if_opt.open(g_dyn_ud_vdd_port);
				if (0 == cal_handle) {
					pr_err("[  err][%s] %s: tuning_if_opt.open failed \r\n",
						LOG_FLAG, __func__);
					return -EINVAL;
				}

				if (tool_up->opt == OPT_SET_CAL_VAL) {
					ret = tuning_if_opt.write(cal_handle, tool_up->reserve,
								tool_up->param_id, tool_up->payload_size, tool_up->payload);
					if (0 > ret) {
						pr_err("[  err][%s] %s: tuning_if_opt.write failed ret = %d \r\n",
							LOG_FLAG, __func__, ret);
						return -EINVAL;
					}
				} else if (tool_up->opt == OPT_GET_CAL_VAL) {
					ret = tuning_if_opt.read(cal_handle, tool_up->reserve,
									tool_up->param_id, tool_up->payload_size, tool_up->payload);
					if (0 > ret) {
						pr_err("[  err][%s] %s: tuning_if_opt.read failed ret = %d \r\n",
							LOG_FLAG, __func__, ret);
						return -EINVAL;
					}

					tool_down = (struct dev_comm_data *)priv->tooldown.data;
					tool_down->opt = tool_up->opt;
					tool_down->param_id = tool_up->param_id;
					tool_down->payload_size = tool_up->payload_size;
					memcpy(tool_down->payload, tool_up->payload, tool_up->payload_size);
					priv->tooldown.len = DEV_COMM_DATA_LEN(tool_down);

					priv->tooldown.flag = true;
					wake_up_interruptible(&priv->tooldown.wq);

					return ret;
				}
			}
            break;
        case SIPA_TUNING_CTRL_RD_UP: {

            }
            break;
        case SIPA_TUNING_CTRL_RD_DOWN: {
				ret = wait_event_interruptible(priv->tooldown.wq, priv->tooldown.flag);
				if (ret) {
					pr_err("[  err][%s] %s: wait_event failed\n", LOG_FLAG, __func__);
					return -ERESTART; 
				}

				if (copy_to_user((void __user *)arg, priv->tooldown.data, priv->tooldown.len)) {
					pr_err("[  err][%s] %s: copy to user failed\n", LOG_FLAG, __func__);
					return -EFAULT;
				}

				priv->tooldown.flag = false;
				pr_info("[ info][%s] %s: read:%d\n", LOG_FLAG, __func__, priv->tooldown.len);
				ret = priv->tooldown.len;
            }
            break;
        case SIPA_TUNING_CTRL_WR_DOWN: {

            }
            break;
        default:
	        pr_info("[ info][%s] %s: unsuport cmd:0x%x\n", LOG_FLAG, __func__, cmd);
            return -EFAULT;
    }
	return 0;
}

struct file_operations sipa_turning_tool_fops = {
    .owner = THIS_MODULE,
	.unlocked_ioctl = sipa_tuning_tool_unlocked_ioctl,
	.compat_ioctl = sipa_tuning_tool_unlocked_ioctl,
};