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
#define LOG_FLAG	"sia81xx_afe"

#include <linux/slab.h>
#include <linux/wait.h>
//#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/version.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,1))
#include <dsp/msm_audio_ion.h>
#include <dsp/q6afe-v2.h>
#include <dsp/q6audio-v2.h>

#else
#include <linux/msm_audio_ion.h>
#include <sound/q6afe-v2.h>
#include <sound/q6audio-v2.h>
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
#include <ipc/apr.h>
#endif


#include "sia81xx_tuning_if.h"


#define TIMEOUT_MS 				(1000) /* the same to q6afe.c */
#define CAL_BLOCK_MAP_SIZE		(SZ_4K)

static DEFINE_MUTEX(sia81xx_afe_list_mutex);
static LIST_HEAD(sia81xx_afe_list);


struct sia81xx_cal_block_data {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,11,12))
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

typedef struct sia81xx_afe {
	uint16_t afe_port_id;
	void *apr;
	wait_queue_head_t wait;
	atomic_t state;
	struct sia81xx_cal_block_data cal_block;

	struct list_head list;
	struct mutex afe_lock;
}SIA81XX_AFE;

/* unknown qcom modify apr_audio at which kernel version,
 * but must been modified at 4.14.0 + */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
typedef struct afe_port_cmd_set_param_v2 afe_cmd_sia81xx_set_cal;
typedef struct afe_port_cmd_get_param_v2 afe_cmd_sia81xx_get_cal;
typedef struct param_hdr_v1 afe_cmd_sia81xx_param_payload;
#else
typedef struct __afe_cmd_sia81xx_set_cal {
	struct apr_hdr hdr;
	struct afe_port_cmd_set_param_v2 param;
}__packed afe_cmd_sia81xx_set_cal;

typedef struct __afe_cmd_sia81xx_get_cal {
	struct apr_hdr hdr;
	struct afe_port_cmd_get_param_v2 param;
}__packed afe_cmd_sia81xx_get_cal;

typedef struct afe_port_param_data_v2 afe_cmd_sia81xx_param_payload;
#endif

typedef struct __afe_cmd_sia81xx_set_mmap {
	struct afe_service_cmd_shared_mem_map_regions regions;
	struct  afe_service_shared_map_region_payload payload;
}__packed afe_cmd_sia81xx_set_mmap;

#ifdef SIA81XX_COMPILE_TO_MODULE
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

static int sia81xx_apr_cmd_memory_map(SIA81XX_AFE *afe);


static int afe_port_to_apr_port(
	uint16_t afe_port)
{
	/* range: 0x10 ~ APR_MAX_PORTS£¬ to avoid the port : 0x00 */
	return (afe_port % (APR_MAX_PORTS - 0x10)) + 0x10;
}

static int sia81xx_fill_apr_hdr(
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


static int32_t sia81xx_afe_callback(
	struct apr_client_data *data, 
	void *priv)
{

	SIA81XX_AFE *afe = NULL;

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

	/* priv must a SIA81XX_AFE type pointer */
	afe = (SIA81XX_AFE *)priv;

	if(RESET_EVENTS == data->opcode) {
		pr_info("[ info][%s] %s: RESET_EVENTS, event : %d !! \r\n", 
			LOG_FLAG, __func__, (int)data->reset_event);
		
		afe->cal_block.dsp_mmap_handle = 0;
				
		if (afe->apr) {
			apr_reset(afe->apr);
			atomic_set(&afe->state, 0);
			afe->apr = NULL;
		}
	}

	switch(data->opcode) {
		case AFE_SERVICE_CMDRSP_SHARED_MEM_MAP_REGIONS: 
		{			
			struct afe_service_cmdrsp_shared_mem_map_regions *payload = NULL;
			
			if(!data->payload_size) {
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
			
			if(!data->payload_size) {
				pr_err("[  err][%s] %s: invalid payload_size = %d "
					"with opcode = 0x%08x \r\n", 
					LOG_FLAG, __func__, data->payload_size, data->opcode);
				break;
			}

			payload = (uint32_t *)data->payload;
			switch (payload[0]) {
				case AFE_PORT_CMD_SET_PARAM_V2 : 
				{
					atomic_set(&afe->state, 0);
					wake_up(&afe->wait);
					
					break;
				}
				case AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS :
				{
					afe->cal_block.dsp_mmap_handle = 0;
					
					atomic_set(&afe->state, 0);
					wake_up(&afe->wait);
					
					break;
				}
				default : 
				{
					pr_err("[  err][%s] %s: unknow payload[0] = 0x%08x "
						"with opcode = 0x%08x \r\n", 
					LOG_FLAG, __func__, payload[0], data->opcode);
					break;
				}
			}
			
			break;
		}
		default :
		{
			pr_err("[  err][%s] %s: invalid opcode = 0x%08x \r\n", 
				LOG_FLAG, __func__, data->opcode);
			break;
		}
	}

	return 0;
}

static int sia81xx_apr_send(
	SIA81XX_AFE *afe, 
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
	SIA81XX_AFE *afe)
{
	int ret = 0;
	
	if (NULL == afe->apr) {
		afe->apr = apr_register("ADSP", "AFE", sia81xx_afe_callback,
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
	SIA81XX_AFE *afe)
{
	int ret = 0;
	
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,11,12))
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
		ret = sia81xx_apr_cmd_memory_map(afe);
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
	SIA81XX_AFE *afe)
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
	SIA81XX_AFE *afe)
{
	if(NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe !! \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}
	
	if(0 != check_apr(afe)) {
		pr_err("[  err][%s] %s: check_apr failed !! \r\n", 
			LOG_FLAG, __func__);
		return -EINVAL;
	}
	
	if(0 != check_cal_block(afe)) {
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

static void sia81xx_write_param_to_payload(
	afe_cmd_sia81xx_param_payload *payload, 
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

static int sia81xx_read_param_from_payload(
	afe_cmd_sia81xx_param_payload *payload, 
	uint32_t module_id, 
	uint32_t param_id, 
	uint32_t param_size, 
	uint8_t *param)
{
	if(payload->module_id != module_id) {
		pr_err("[  err][%s] %s: unmatched module_id 0x%08x, "
			"payload->module_id 0x%08x \r\n", 
			LOG_FLAG, __func__, module_id, payload->module_id);
		return -EINVAL;
	}
	
	if(payload->param_id != param_id) {
		pr_err("[  err][%s] %s: unmatched param_id 0x%08x, "
			"payload->param_id 0x%08x \r\n", 
			LOG_FLAG, __func__, param_id, payload->param_id);
		return -EINVAL;
	}
	
	if(payload->param_size > param_size) {
		pr_err("[  err][%s] %s: unmatched param_size 0x%08x, "
			"payload->param_size 0x%08x \r\n", 
			LOG_FLAG, __func__, param_size, payload->param_size);
		return -EINVAL;
	}
	
	memcpy(param, (void *)(payload + 1), payload->param_size);

	return payload->param_size;
}


static int sia81xx_apr_cmd_memory_map(
	SIA81XX_AFE *afe)
{
	int ret = 0;
	afe_cmd_sia81xx_set_mmap apr_cmd;

	if(NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe !! \r\n", 
			LOG_FLAG, __func__);
		ret = -EINVAL;
		goto err;
	}
	
	if(0 != check_apr(afe)) {
		pr_err("[  err][%s] %s: check_apr failed !! \r\n", 
			LOG_FLAG, __func__);
		ret = -EINVAL;
		goto err;
	}

	/* clear cmd */
	memset((void *)(&apr_cmd), 0x00, sizeof(apr_cmd));

	/* fill cmd head */
	sia81xx_fill_apr_hdr(&(apr_cmd.regions.hdr), 
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
	if(0 > sia81xx_apr_send(afe, &apr_cmd)) {
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

static int sia81xx_apr_cmd_memory_unmap(
	SIA81XX_AFE *afe)
{
	int ret = 0;
	struct afe_service_cmd_shared_mem_unmap_regions apr_cmd;

	if(NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe !! \r\n", 
			LOG_FLAG, __func__);
		ret = -EINVAL;
		goto err;
	}

	if(0 == afe->cal_block.dsp_mmap_handle)
		return 0;
	
	if(0 != check_apr(afe)) {
		pr_err("[  err][%s] %s: check_apr failed !! \r\n", 
			LOG_FLAG, __func__);
		ret = -EINVAL;
		goto err;
	}

	/* clear cmd */
	memset((void *)(&apr_cmd), 0x00, sizeof(apr_cmd));

	/* fill cmd head */
	sia81xx_fill_apr_hdr(
			&(apr_cmd.hdr), 
			afe_port_to_apr_port(afe->afe_port_id), 
			AFE_SERVICE_CMD_SHARED_MEM_UNMAP_REGIONS, 
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.mem_map_handle = afe->cal_block.dsp_mmap_handle;

	/* send the cmd to dsp through apr, and wait dsp returned msg */
	if(0 > sia81xx_apr_send(afe, &apr_cmd)) {
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

static unsigned int sia81xx_afe_list_count(void)
{
	unsigned count = 0;
	SIA81XX_AFE *afe = NULL;
	
	mutex_lock(&sia81xx_afe_list_mutex);

	list_for_each_entry(afe, &sia81xx_afe_list, list) {
		count ++;
	}

	mutex_unlock(&sia81xx_afe_list_mutex);

	return count;
}

static SIA81XX_AFE *find_sia81xx_afe_list(
	uint16_t afe_port_id)
{
	SIA81XX_AFE *afe = NULL;

	mutex_lock(&sia81xx_afe_list_mutex);

	list_for_each_entry(afe, &sia81xx_afe_list, list) {
		if(afe_port_id == afe->afe_port_id) {
			mutex_unlock(&sia81xx_afe_list_mutex);
			return afe;
		}
	}

	mutex_unlock(&sia81xx_afe_list_mutex);

	return NULL;
}

static void add_sia81xx_afe_list(
	SIA81XX_AFE *afe)
{
	if(NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe \r\n", LOG_FLAG, __func__);
		return ;
	}

	if(NULL != find_sia81xx_afe_list(afe->afe_port_id))
		return ;

	mutex_lock(&sia81xx_afe_list_mutex);
	list_add(&afe->list, &sia81xx_afe_list);
	mutex_unlock(&sia81xx_afe_list_mutex);

	pr_debug("[debug][%s] %s: add afe port id : %u, count = %u \r\n", 
		LOG_FLAG, __func__, afe->afe_port_id, sia81xx_afe_list_count());
}

static void del_sia81xx_afe_list(
	SIA81XX_AFE *afe)
{
	if(NULL == afe) {
		pr_err("[  err][%s] %s: NULL == afe \r\n", LOG_FLAG, __func__);
		return ;
	}

	pr_debug("[debug][%s] %s: del fe port id : %u, count = %u \r\n", 
		LOG_FLAG, __func__, afe->afe_port_id, sia81xx_afe_list_count());

	mutex_lock(&sia81xx_afe_list_mutex);
	list_del(&afe->list);
	mutex_unlock(&sia81xx_afe_list_mutex);
}

int sia81xx_afe_send(
	unsigned long afe_handle, 
	uint32_t module_id, 
	uint32_t param_id, 
	uint32_t param_size, 
	uint8_t *buf)
{
	int ret = 0;
	afe_cmd_sia81xx_set_cal apr_cmd;
	SIA81XX_AFE *afe = (SIA81XX_AFE *)afe_handle;

	if(NULL == afe) 
		return -EINVAL;

	if(APR_SUBSYS_LOADED != apr_get_q6_state()) {
		pr_err("[  err][%s] %s: q6_state : %u \r\n", 
			LOG_FLAG, __func__, (unsigned int)apr_get_q6_state());
		return -EINVAL;
	}

	mutex_lock(&afe->afe_lock);

	ret = chack_afe_validity(afe);
	if(0 != ret) {
		pr_err("[  err][%s] %s: chack_afe_validity failed !! \r\n", 
			LOG_FLAG, __func__);
		goto err;
	}

	/* is the pa size has a capacity of holding the param */
	if (param_size > 
		(afe->cal_block.pa_size- sizeof(afe_cmd_sia81xx_param_payload))) {
		pr_err("[  err][%s] %s: invalid payload size = %d \r\n", 
			LOG_FLAG, __func__, param_size);
		ret = -EINVAL;
		goto err;
	}

	/* write param to out-of-band payload */
	sia81xx_write_param_to_payload(
		(afe_cmd_sia81xx_param_payload *)(afe->cal_block.kvaddr), 
		module_id, 
		param_id, 
		param_size, 
		buf);

	/* clear cmd */
	memset((void *)(&apr_cmd), 0x00, sizeof(apr_cmd));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
	/* fill cmd head */
	sia81xx_fill_apr_hdr(
			(struct apr_hdr *)(&apr_cmd.apr_hdr),
			afe_port_to_apr_port(afe->afe_port_id), 
			AFE_PORT_CMD_SET_PARAM_V2, 
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.port_id = afe->afe_port_id;
	apr_cmd.payload_size = 
		param_size + sizeof(afe_cmd_sia81xx_param_payload);
	apr_cmd.mem_hdr.data_payload_addr_lsw =
		lower_32_bits(afe->cal_block.paddr);
	apr_cmd.mem_hdr.data_payload_addr_msw =
		msm_audio_populate_upper_32_bits(afe->cal_block.paddr);
	apr_cmd.mem_hdr.mem_map_handle = afe->cal_block.dsp_mmap_handle;
#else
	/* fill cmd head */
	sia81xx_fill_apr_hdr(
			(struct apr_hdr *)(&apr_cmd.hdr),
			afe_port_to_apr_port(afe->afe_port_id), 
			AFE_PORT_CMD_SET_PARAM_V2, 
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.param.port_id = afe->afe_port_id;
	apr_cmd.param.payload_size = 
		param_size + sizeof(afe_cmd_sia81xx_param_payload);
	apr_cmd.param.payload_address_lsw =
		lower_32_bits(afe->cal_block.paddr);
	apr_cmd.param.payload_address_msw =
		msm_audio_populate_upper_32_bits(afe->cal_block.paddr);
	apr_cmd.param.mem_map_handle = afe->cal_block.dsp_mmap_handle;
#endif

	/* send the cmd to dsp through apr, and wait dsp returned msg */
	ret = sia81xx_apr_send(afe, &apr_cmd);
	if(0 > ret) {
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

int sia81xx_afe_read(
	unsigned long afe_handle, 
	uint32_t module_id, 
	uint32_t param_id, 
	uint32_t param_size, 
	uint8_t *buf)
{
	int ret = 0;
	afe_cmd_sia81xx_get_cal apr_cmd;
	SIA81XX_AFE *afe = (SIA81XX_AFE *)afe_handle;

	if(NULL == afe) 
		return -EINVAL;

	if(APR_SUBSYS_LOADED != apr_get_q6_state()) {
		pr_err("[  err][%s] %s: q6_state : %u \r\n", 
			LOG_FLAG, __func__, (unsigned int)apr_get_q6_state());
		return -EINVAL;
	}
	
	mutex_lock(&afe->afe_lock);
	
	ret = chack_afe_validity(afe);
	if(0 != ret) {
		pr_err("[  err][%s] %s: chack_afe_validity failed !! \r\n", 
			LOG_FLAG, __func__);
		goto err;
	}

	/* is the pa size has a capacity of holding the param */
	if (param_size > 
		(afe->cal_block.pa_size - sizeof(afe_cmd_sia81xx_param_payload))) {
		pr_err("[  err][%s] %s: invalid payload size = %d \r\n", 
			LOG_FLAG, __func__, param_size);
		ret = -EINVAL;
		goto err;
	}

	/* write param to out-of-band payload */
	sia81xx_write_param_to_payload(
		(afe_cmd_sia81xx_param_payload *)(afe->cal_block.kvaddr), 
		module_id, 
		param_id, 
		param_size, 
		buf);

	/* clear cmd */
	memset((void *)(&apr_cmd), 0x00, sizeof(apr_cmd));

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,0))
	/* fill cmd head */
	sia81xx_fill_apr_hdr(
			(struct apr_hdr *)(&apr_cmd.apr_hdr),
			afe_port_to_apr_port(afe->afe_port_id), 
			AFE_PORT_CMD_GET_PARAM_V2, 
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.port_id = afe->afe_port_id;
	apr_cmd.payload_size = 
		param_size + sizeof(afe_cmd_sia81xx_param_payload);
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
	sia81xx_fill_apr_hdr(
			(struct apr_hdr *)(&apr_cmd.hdr),
			afe_port_to_apr_port(afe->afe_port_id), 
			AFE_PORT_CMD_GET_PARAM_V2, 
			sizeof(apr_cmd));

	/* fill cmd entity */
	apr_cmd.param.port_id = afe->afe_port_id;
	apr_cmd.param.payload_size = 
		param_size + sizeof(afe_cmd_sia81xx_param_payload);
	apr_cmd.param.payload_address_lsw =
		lower_32_bits(afe->cal_block.paddr);
	apr_cmd.param.payload_address_msw =
		msm_audio_populate_upper_32_bits(afe->cal_block.paddr);
	apr_cmd.param.mem_map_handle = afe->cal_block.dsp_mmap_handle;
	apr_cmd.param.module_id = module_id;
	apr_cmd.param.param_id = param_id;
#endif

	/* send the cmd to dsp through apr, and wait dsp returned msg */
	ret = sia81xx_apr_send(afe, &apr_cmd);
	if(0 > ret) {
		goto err;
	}

	/* read data from dsp returned, and copy this to the user buf */
	ret = sia81xx_read_param_from_payload(
			(afe_cmd_sia81xx_param_payload *)(afe->cal_block.kvaddr), 
			module_id, 
			param_id, 
			param_size, 
			buf);
	if(0 > ret){
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

unsigned long sia81xx_afe_open(
	uint32_t afe_prot_id) {

	SIA81XX_AFE *afe;

	/* when open afe, maybe q6 has not start, 
	 * so must be considered this case.
	 * 0 == apr_get_q6_state() */

	afe = find_sia81xx_afe_list(afe_prot_id);
	if(NULL != afe)
		return (unsigned long)afe;
	
	afe = kzalloc(sizeof(SIA81XX_AFE), GFP_KERNEL);
	if(NULL == afe) {
		pr_err("[  err][%s] %s: kzalloc afe failed !! \r\n", 
			LOG_FLAG, __func__);
		return 0;
	}

#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,11,12))
	afe->cal_block.fd = 0;
#else
	afe->cal_block.ion_client = NULL;
	afe->cal_block.ion_handle = NULL;
#endif

	atomic_set(&(afe->state), 0);
	init_waitqueue_head(&(afe->wait));

	afe->afe_port_id = afe_prot_id;
	mutex_init(&afe->afe_lock);

	add_sia81xx_afe_list(afe);
	
	pr_info("[ info][%s] %s: afe open success !! "
		"afe prot : 0x%04x \r\n", 
		LOG_FLAG, __func__, afe_prot_id);
	
	return (unsigned long)afe;
}

int sia81xx_afe_close(
	unsigned long afe_handle) {

	int ret = 0;
	SIA81XX_AFE *afe = (SIA81XX_AFE *)afe_handle;

	if(NULL == afe) {
		pr_warn("[ warn][%s] %s: NULL == afe \r\n", 
			LOG_FLAG, __func__);
		return 0;
	}

	pr_info("[ info][%s] %s: afe closeing !! "
		"afe prot : 0x%04x \r\n", 
		LOG_FLAG, __func__, afe->afe_port_id);

	/* to ask dsp unmap memory */
	ret = sia81xx_apr_cmd_memory_unmap(afe);
	if(0 != ret) {
		pr_err("[  err][%s] %s: unmap error, close failed !! ret = %d \r\n", 
			LOG_FLAG, __func__, ret);
		return ret;
	}

	/* free ion buf */
#if (LINUX_VERSION_CODE > KERNEL_VERSION(4,11,12))
	if(0 != afe->cal_block.fd) {
		ret = msm_audio_ion_free((struct dma_buf *)afe->cal_block.fd);
		if(0 != ret) {
			pr_err("[  err][%s] %s: ion free error, close failed !! "
				"ret = %d \r\n", 
				LOG_FLAG, __func__, ret);
			return ret;
		} else {
			afe->cal_block.fd = 0;
		}
	}
#else
	if((NULL != afe->cal_block.ion_client) || 
		(NULL != afe->cal_block.ion_handle)) {
		
		ret = msm_audio_ion_free(
			afe->cal_block.ion_client, afe->cal_block.ion_handle);
		if(0 != ret) {
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
	if(0 != ret) {
		pr_err("[  err][%s] %s: 0 != apr_deregister, close failed !! \r\n", 
			LOG_FLAG, __func__);
		return ret;
	} else {
		afe->apr = NULL;
	}

	del_sia81xx_afe_list(afe);
	
	kfree(afe);

	return 0;
}

static int sia81xx_afe_init(void) {
	
	int ret = 0;

	pr_info("[ info][%s] %s: run !! \r\n", 
		LOG_FLAG, __func__);

	return ret;
}

static void sia81xx_afe_exit(void) {
	
	pr_info("[ info][%s] %s: run !! \r\n", 
		LOG_FLAG, __func__);
}

struct sia81xx_cal_opt tuning_if_opt = {
	.init = sia81xx_afe_init,
	.exit = sia81xx_afe_exit,
	.open = sia81xx_afe_open,
	.close = sia81xx_afe_close,
	.read = sia81xx_afe_read,
	.write = sia81xx_afe_send
};


