/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/msm_audio_ion.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/msm_audio.h>
#include <sound/apr_audio-v2.h>
#include <linux/qdsp6v2/apr_us.h>
#include "q6usm.h"

#define ADSP_MEMORY_MAP_SHMEM8_4K_POOL 3

#define MEM_4K_OFFSET 4095
#define MEM_4K_MASK 0xfffff000

#define USM_SESSION_MAX 0x02 /* aDSP:USM limit */

#define READDONE_IDX_STATUS     0

#define WRITEDONE_IDX_STATUS    0

/* Standard timeout in the asynchronous ops */
#define Q6USM_TIMEOUT_JIFFIES	(1*HZ) /* 1 sec */

static DEFINE_MUTEX(session_lock);

static struct us_client *session[USM_SESSION_MAX];
static int32_t q6usm_mmapcallback(struct apr_client_data *data, void *priv);
static int32_t q6usm_callback(struct apr_client_data *data, void *priv);
static void q6usm_add_hdr(struct us_client *usc, struct apr_hdr *hdr,
			  uint32_t pkt_size, bool cmd_flg);

struct usm_mmap {
	atomic_t ref_cnt;
	atomic_t cmd_state;
	wait_queue_head_t cmd_wait;
	void *apr;
	int mem_handle;
};

static struct usm_mmap this_mmap;

static void q6usm_add_mmaphdr(struct apr_hdr *hdr,
			      uint32_t pkt_size, bool cmd_flg, u32 token)
{
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				       APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	hdr->src_port = 0;
	hdr->dest_port = 0;
	if (cmd_flg) {
		hdr->token = token;
		atomic_set(&this_mmap.cmd_state, 1);
	}
	hdr->pkt_size  = pkt_size;
	return;
}

static int q6usm_memory_map(phys_addr_t buf_add, int dir, uint32_t bufsz,
		uint32_t bufcnt, uint32_t session, uint32_t *mem_handle)
{
	struct usm_cmd_memory_map_region mem_region_map;
	int rc = 0;

	if (this_mmap.apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	q6usm_add_mmaphdr(&mem_region_map.hdr,
			  sizeof(struct usm_cmd_memory_map_region), true,
			  ((session << 8) | dir));

	mem_region_map.hdr.opcode = USM_CMD_SHARED_MEM_MAP_REGION;
	mem_region_map.mempool_id = ADSP_MEMORY_MAP_SHMEM8_4K_POOL;

	mem_region_map.num_regions = 1;
	mem_region_map.flags = 0;

	mem_region_map.shm_addr_lsw = lower_32_bits(buf_add);
	mem_region_map.shm_addr_msw =
			msm_audio_populate_upper_32_bits(buf_add);
	mem_region_map.mem_size_bytes = bufsz * bufcnt;

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) &mem_region_map);
	if (rc < 0) {
		pr_err("%s: mem_map op[0x%x]rc[%d]\n",
		       __func__, mem_region_map.hdr.opcode, rc);
		rc = -EINVAL;
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
				(atomic_read(&this_mmap.cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: timeout. waited for memory_map\n", __func__);
	} else {
		*mem_handle = this_mmap.mem_handle;
		rc = 0;
	}
fail_cmd:
	return rc;
}

int q6usm_memory_unmap(phys_addr_t buf_add, int dir, uint32_t session,
			uint32_t mem_handle)
{
	struct usm_cmd_memory_unmap_region mem_unmap;
	int rc = 0;

	if (this_mmap.apr == NULL) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	q6usm_add_mmaphdr(&mem_unmap.hdr,
			  sizeof(struct usm_cmd_memory_unmap_region), true,
			  ((session << 8) | dir));
	mem_unmap.hdr.opcode = USM_CMD_SHARED_MEM_UNMAP_REGION;
	mem_unmap.mem_map_handle = mem_handle;

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) &mem_unmap);
	if (rc < 0) {
		pr_err("%s: mem_unmap op[0x%x] rc[%d]\n",
		       __func__, mem_unmap.hdr.opcode, rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
				(atomic_read(&this_mmap.cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: timeout. waited for memory_unmap\n", __func__);
	} else
		rc = 0;
fail_cmd:
	return rc;
}

static int q6usm_session_alloc(struct us_client *usc)
{
	int ind = 0;

	mutex_lock(&session_lock);
	for (ind = 0; ind < USM_SESSION_MAX; ++ind) {
		if (!session[ind]) {
			session[ind] = usc;
			mutex_unlock(&session_lock);
			++ind; /* session id: 0 reserved */
			pr_debug("%s: session[%d] was allocated\n",
				  __func__, ind);
			return ind;
		}
	}
	mutex_unlock(&session_lock);
	return -ENOMEM;
}

static void q6usm_session_free(struct us_client *usc)
{
	/* Session index was incremented during allocation */
	uint16_t ind = (uint16_t)usc->session - 1;

	pr_debug("%s: to free session[%d]\n", __func__, ind);
	if (ind < USM_SESSION_MAX) {
		mutex_lock(&session_lock);
		session[ind] = NULL;
		mutex_unlock(&session_lock);
	}
}

static int q6usm_us_client_buf_free(unsigned int dir,
			     struct us_client *usc)
{
	struct us_port_data *port;
	int rc = 0;

	if ((usc == NULL) ||
	    ((dir != IN) && (dir != OUT)))
		return -EINVAL;

	mutex_lock(&usc->cmd_lock);
	port = &usc->port[dir];
	if (port == NULL) {
		mutex_unlock(&usc->cmd_lock);
		return -EINVAL;
	}

	if (port->data == NULL) {
		mutex_unlock(&usc->cmd_lock);
		return 0;
	}

	rc = q6usm_memory_unmap(port->phys, dir, usc->session,
				*((uint32_t *)port->ext));
	pr_debug("%s: data[%pK]phys[%llx][%pK]\n", __func__,
		 (void *)port->data, (u64)port->phys, (void *)&port->phys);

	msm_audio_ion_free(port->client, port->handle);

	port->data = NULL;
	port->phys = 0;
	port->buf_size = 0;
	port->buf_cnt = 0;
	port->client = NULL;
	port->handle = NULL;

	mutex_unlock(&usc->cmd_lock);
	return rc;
}

int q6usm_us_param_buf_free(unsigned int dir,
			struct us_client *usc)
{
	struct us_port_data *port;
	int rc = 0;

	if ((usc == NULL) ||
		((dir != IN) && (dir != OUT)))
		return -EINVAL;

	mutex_lock(&usc->cmd_lock);
	port = &usc->port[dir];
	if (port == NULL) {
		mutex_unlock(&usc->cmd_lock);
		return -EINVAL;
	}

	if (port->param_buf == NULL) {
		mutex_unlock(&usc->cmd_lock);
		return 0;
	}

	rc = q6usm_memory_unmap(port->param_phys, dir, usc->session,
				*((uint32_t *)port->param_buf_mem_handle));
	pr_debug("%s: data[%pK]phys[%llx][%pK]\n", __func__,
		 (void *)port->param_buf, (u64)port->param_phys,
		 (void *)&port->param_phys);

	msm_audio_ion_free(port->param_client, port->param_handle);

	port->param_buf = NULL;
	port->param_phys = 0;
	port->param_buf_size = 0;
	port->param_client = NULL;
	port->param_handle = NULL;

	mutex_unlock(&usc->cmd_lock);
	return rc;
}

void q6usm_us_client_free(struct us_client *usc)
{
	int loopcnt = 0;
	struct us_port_data *port;
	uint32_t *p_mem_handle = NULL;

	if ((usc == NULL) ||
	    !(usc->session))
		return;

	for (loopcnt = 0; loopcnt <= OUT; ++loopcnt) {
		port = &usc->port[loopcnt];
		if (port->data == NULL)
			continue;
		pr_debug("%s: loopcnt = %d\n", __func__, loopcnt);
		q6usm_us_client_buf_free(loopcnt, usc);
		q6usm_us_param_buf_free(loopcnt, usc);
	}
	q6usm_session_free(usc);
	apr_deregister(usc->apr);

	pr_debug("%s: APR De-Register\n", __func__);

	if (atomic_read(&this_mmap.ref_cnt) <= 0) {
		pr_err("%s: APR Common Port Already Closed\n", __func__);
		goto done;
	}

	atomic_dec(&this_mmap.ref_cnt);
	if (atomic_read(&this_mmap.ref_cnt) == 0) {
		apr_deregister(this_mmap.apr);
		pr_debug("%s: APR De-Register common port\n", __func__);
	}

done:
	p_mem_handle = (uint32_t *)usc->port[IN].ext;
	kfree(p_mem_handle);
	kfree(usc);
	pr_debug("%s:\n", __func__);
	return;
}

struct us_client *q6usm_us_client_alloc(
	void (*cb)(uint32_t, uint32_t, uint32_t *, void *),
	void *priv)
{
	struct us_client *usc;
	uint32_t *p_mem_handle = NULL;
	int n;
	int lcnt = 0;

	usc = kzalloc(sizeof(struct us_client), GFP_KERNEL);
	if (usc == NULL) {
		pr_err("%s: us_client allocation failed\n", __func__);
		return NULL;
	}
	p_mem_handle = kzalloc(sizeof(uint32_t) * 4, GFP_KERNEL);
	if (p_mem_handle == NULL) {
		pr_err("%s: p_mem_handle allocation failed\n", __func__);
		kfree(usc);
		return NULL;
	}

	n = q6usm_session_alloc(usc);
	if (n <= 0)
		goto fail_session;
	usc->session = n;
	usc->cb = cb;
	usc->priv = priv;
	usc->apr = apr_register("ADSP", "USM", \
				(apr_fn)q6usm_callback,\
				((usc->session) << 8 | 0x0001),\
				usc);

	if (usc->apr == NULL) {
		pr_err("%s: Registration with APR failed\n", __func__);
		goto fail;
	}
	pr_debug("%s: Registering the common port with APR\n", __func__);
	if (atomic_read(&this_mmap.ref_cnt) == 0) {
		this_mmap.apr = apr_register("ADSP", "USM",
					     (apr_fn)q6usm_mmapcallback,
					     0x0FFFFFFFF, &this_mmap);
		if (this_mmap.apr == NULL) {
			pr_err("%s: USM port registration failed\n",
			       __func__);
			goto fail;
		}
	}

	atomic_inc(&this_mmap.ref_cnt);
	init_waitqueue_head(&usc->cmd_wait);
	mutex_init(&usc->cmd_lock);
	for (lcnt = 0; lcnt <= OUT; ++lcnt) {
		mutex_init(&usc->port[lcnt].lock);
		spin_lock_init(&usc->port[lcnt].dsp_lock);
		usc->port[lcnt].ext = (void *)p_mem_handle++;
		usc->port[lcnt].param_buf_mem_handle = (void *)p_mem_handle++;
		pr_err("%s: usc->port[%d].ext=%pK;\n",
		       __func__, lcnt, usc->port[lcnt].ext);
	}
	atomic_set(&usc->cmd_state, 0);

	return usc;
fail:
	kfree(p_mem_handle);
	q6usm_us_client_free(usc);
	return NULL;
fail_session:
	kfree(p_mem_handle);
	kfree(usc);
	return NULL;
}

int q6usm_us_client_buf_alloc(unsigned int dir,
			      struct us_client *usc,
			      unsigned int bufsz,
			      unsigned int bufcnt)
{
	int rc = 0;
	struct us_port_data *port = NULL;
	unsigned int size = bufsz*bufcnt;
	size_t len;

	if ((usc == NULL) ||
	    ((dir != IN) && (dir != OUT)) || (size == 0) ||
	    (usc->session <= 0 || usc->session > USM_SESSION_MAX)) {
		pr_err("%s: wrong parameters: size=%d; bufcnt=%d\n",
		       __func__, size, bufcnt);
		return -EINVAL;
	}

	mutex_lock(&usc->cmd_lock);

	port = &usc->port[dir];

	/* The size to allocate should be multiple of 4K bytes */
	size = PAGE_ALIGN(size);

	rc = msm_audio_ion_alloc("ultrasound_client",
		&port->client, &port->handle,
		size, &port->phys,
		&len, &port->data);

	if (rc) {
		pr_err("%s: US ION allocation failed, rc = %d\n",
			__func__, rc);
		mutex_unlock(&usc->cmd_lock);
		return -ENOMEM;
	}

	port->buf_cnt = bufcnt;
	port->buf_size = bufsz;
	pr_debug("%s: data[%pK]; phys[%llx]; [%pK]\n", __func__,
		 (void *)port->data,
		 (u64)port->phys,
		 (void *)&port->phys);

	rc = q6usm_memory_map(port->phys, dir, size, 1, usc->session,
				(uint32_t *)port->ext);
	if (rc < 0) {
		pr_err("%s: CMD Memory_map failed\n", __func__);
		mutex_unlock(&usc->cmd_lock);
		q6usm_us_client_buf_free(dir, usc);
		q6usm_us_param_buf_free(dir, usc);
	} else {
		mutex_unlock(&usc->cmd_lock);
		rc = 0;
	}

	return rc;
}

int q6usm_us_param_buf_alloc(unsigned int dir,
			struct us_client *usc,
			unsigned int bufsz)
{
	int rc = 0;
	struct us_port_data *port = NULL;
	unsigned int size = bufsz;
	size_t len;

	if ((usc == NULL) ||
		((dir != IN) && (dir != OUT)) ||
		(usc->session <= 0 || usc->session > USM_SESSION_MAX)) {
		pr_err("%s: wrong parameters: direction=%d, bufsz=%d\n",
			__func__, dir, bufsz);
		return -EINVAL;
	}

	mutex_lock(&usc->cmd_lock);

	port = &usc->port[dir];

	if (bufsz == 0) {
		pr_debug("%s: bufsz=0, get/set param commands are forbidden\n",
			__func__);
		port->param_buf = NULL;
		mutex_unlock(&usc->cmd_lock);
		return rc;
	}

	/* The size to allocate should be multiple of 4K bytes */
	size = PAGE_ALIGN(size);

	rc = msm_audio_ion_alloc("ultrasound_client",
		&port->param_client, &port->param_handle,
		size, &port->param_phys,
		&len, &port->param_buf);

	if (rc) {
		pr_err("%s: US ION allocation failed, rc = %d\n",
			__func__, rc);
		mutex_unlock(&usc->cmd_lock);
		return -ENOMEM;
	}

	port->param_buf_size = bufsz;
	pr_debug("%s: param_buf[%pK]; param_phys[%llx]; [%pK]\n", __func__,
		 (void *)port->param_buf,
		 (u64)port->param_phys,
		 (void *)&port->param_phys);

	rc = q6usm_memory_map(port->param_phys, (IN | OUT), size, 1,
			usc->session, (uint32_t *)port->param_buf_mem_handle);
	if (rc < 0) {
		pr_err("%s: CMD Memory_map failed\n", __func__);
		mutex_unlock(&usc->cmd_lock);
		q6usm_us_client_buf_free(dir, usc);
		q6usm_us_param_buf_free(dir, usc);
	} else {
		mutex_unlock(&usc->cmd_lock);
		rc = 0;
	}

	return rc;
}

static int32_t q6usm_mmapcallback(struct apr_client_data *data, void *priv)
{
	uint32_t token;
	uint32_t *payload = data->payload;

	pr_debug("%s: ptr0[0x%x]; ptr1[0x%x]; opcode[0x%x]\n",
		 __func__, payload[0], payload[1], data->opcode);
	pr_debug("%s: token[0x%x]; payload_size[%d]; src[%d]; dest[%d];\n",
		 __func__, data->token, data->payload_size,
		 data->src_port, data->dest_port);

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		/* status field check */
		if (payload[1]) {
			pr_err("%s: wrong response[%d] on cmd [%d]\n",
			       __func__, payload[1], payload[0]);
		} else {
			token = data->token;
			switch (payload[0]) {
			case USM_CMD_SHARED_MEM_UNMAP_REGION:
				if (atomic_read(&this_mmap.cmd_state)) {
					atomic_set(&this_mmap.cmd_state, 0);
					wake_up(&this_mmap.cmd_wait);
				}
			case USM_CMD_SHARED_MEM_MAP_REGION:
				/* For MEM_MAP, additional answer is waited, */
				/* therfore, no wake-up here */
				pr_debug("%s: cmd[0x%x]; result[0x%x]\n",
					 __func__, payload[0], payload[1]);
				break;
			default:
				pr_debug("%s: wrong command[0x%x]\n",
					 __func__, payload[0]);
				break;
			}
		}
	} else {
		if (data->opcode == USM_CMDRSP_SHARED_MEM_MAP_REGION) {
			this_mmap.mem_handle = payload[0];
			pr_debug("%s: memory map handle = 0x%x",
				__func__, payload[0]);
			if (atomic_read(&this_mmap.cmd_state)) {
				atomic_set(&this_mmap.cmd_state, 0);
				wake_up(&this_mmap.cmd_wait);
			}
		}
	}
	return 0;
}


static int32_t q6usm_callback(struct apr_client_data *data, void *priv)
{
	struct us_client *usc = (struct us_client *)priv;
	unsigned long dsp_flags;
	uint32_t *payload = data->payload;
	uint32_t token = data->token;
	uint32_t opcode = Q6USM_EVENT_UNDEF;

	if (usc == NULL) {
		pr_err("%s: client info is NULL\n", __func__);
		return -EINVAL;
	}

	if (data->opcode == APR_BASIC_RSP_RESULT) {
		/* status field check */
		if (payload[1]) {
			pr_err("%s: wrong response[%d] on cmd [%d]\n",
			       __func__, payload[1], payload[0]);
			if (usc->cb)
				usc->cb(data->opcode, token,
					(uint32_t *)data->payload, usc->priv);
		} else {
			switch (payload[0]) {
			case USM_SESSION_CMD_RUN:
			case USM_STREAM_CMD_CLOSE:
				if (token != usc->session) {
					pr_err("%s: wrong token[%d]",
					       __func__, token);
					break;
				}
			case USM_STREAM_CMD_OPEN_READ:
			case USM_STREAM_CMD_OPEN_WRITE:
			case USM_STREAM_CMD_SET_ENC_PARAM:
			case USM_DATA_CMD_MEDIA_FORMAT_UPDATE:
			case USM_SESSION_CMD_SIGNAL_DETECT_MODE:
			case USM_STREAM_CMD_SET_PARAM:
			case USM_STREAM_CMD_GET_PARAM:
				if (atomic_read(&usc->cmd_state)) {
					atomic_set(&usc->cmd_state, 0);
					wake_up(&usc->cmd_wait);
				}
				if (usc->cb)
					usc->cb(data->opcode, token,
						(uint32_t *)data->payload,
						usc->priv);
				break;
			default:
				break;
			}
		}
		return 0;
	}

	switch (data->opcode) {
	case RESET_EVENTS: {
		pr_err("%s: Reset event is received: %d %d\n",
				__func__,
				data->reset_event,
				data->reset_proc);

		opcode = RESET_EVENTS;

		apr_reset(this_mmap.apr);
		this_mmap.apr = NULL;

		apr_reset(usc->apr);
		usc->apr = NULL;

		break;
	}


	case USM_DATA_EVENT_READ_DONE: {
		struct us_port_data *port = &usc->port[OUT];

		opcode = Q6USM_EVENT_READ_DONE;
		spin_lock_irqsave(&port->dsp_lock, dsp_flags);
		if (payload[READDONE_IDX_STATUS]) {
			pr_err("%s: wrong READDONE[%d]; token[%d]\n",
			       __func__,
			       payload[READDONE_IDX_STATUS],
			       token);
			token = USM_WRONG_TOKEN;
			spin_unlock_irqrestore(&port->dsp_lock,
					       dsp_flags);
			break;
		}

		if (port->expected_token != token) {
			u32 cpu_buf = port->cpu_buf;
			pr_err("%s: expected[%d] != token[%d]\n",
				__func__, port->expected_token, token);
			pr_debug("%s: dsp_buf=%d; cpu_buf=%d;\n",
				__func__,   port->dsp_buf, cpu_buf);

			token = USM_WRONG_TOKEN;
			/* To prevent data handle continiue */
			port->expected_token = USM_WRONG_TOKEN;
			spin_unlock_irqrestore(&port->dsp_lock,
					       dsp_flags);
			break;
		} /* port->expected_token != data->token */

		port->expected_token = token + 1;
		if (port->expected_token == port->buf_cnt)
			port->expected_token = 0;

		/* gap support */
		if (port->expected_token != port->cpu_buf) {
			port->dsp_buf = port->expected_token;
			token = port->dsp_buf; /* for callback */
		} else
			port->dsp_buf = token;

		spin_unlock_irqrestore(&port->dsp_lock, dsp_flags);
		break;
	} /* case USM_DATA_EVENT_READ_DONE */

	case USM_DATA_EVENT_WRITE_DONE: {
		struct us_port_data *port = &usc->port[IN];

		opcode = Q6USM_EVENT_WRITE_DONE;
		if (payload[WRITEDONE_IDX_STATUS]) {
			pr_err("%s: wrong WRITEDONE_IDX_STATUS[%d]\n",
			       __func__,
			       payload[WRITEDONE_IDX_STATUS]);
			break;
		}

		spin_lock_irqsave(&port->dsp_lock, dsp_flags);
		port->dsp_buf = token + 1;
		if (port->dsp_buf == port->buf_cnt)
			port->dsp_buf = 0;
		spin_unlock_irqrestore(&port->dsp_lock, dsp_flags);

		break;
	} /* case USM_DATA_EVENT_WRITE_DONE */

	case USM_SESSION_EVENT_SIGNAL_DETECT_RESULT: {
		pr_debug("%s: US detect result: result=%d",
			 __func__,
			 payload[0]);
		opcode = Q6USM_EVENT_SIGNAL_DETECT_RESULT;

		break;
	} /* case USM_SESSION_EVENT_SIGNAL_DETECT_RESULT */

	default:
		return 0;

	} /* switch */

	if (usc->cb)
		usc->cb(opcode, token,
			data->payload, usc->priv);

	return 0;
}

uint32_t q6usm_get_virtual_address(int dir,
				   struct us_client *usc,
				   struct vm_area_struct *vms)
{
	uint32_t ret = 0xffffffff;

	if (vms && (usc != NULL) && ((dir == IN) || (dir == OUT))) {
		struct us_port_data *port = &usc->port[dir];
		int size = PAGE_ALIGN(port->buf_size * port->buf_cnt);
		struct audio_buffer ab;

		ab.phys = port->phys;
		ab.data = port->data;
		ab.used = 1;
		ab.size = size;
		ab.actual_size = size;
		ab.handle = port->handle;
		ab.client = port->client;

		ret = msm_audio_ion_mmap(&ab, vms);

	}
	return ret;
}

static void q6usm_add_hdr(struct us_client *usc, struct apr_hdr *hdr,
			  uint32_t pkt_size, bool cmd_flg)
{
	mutex_lock(&usc->cmd_lock);
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				       APR_HDR_LEN(sizeof(struct apr_hdr)),\
				       APR_PKT_VER);
	hdr->src_svc = ((struct apr_svc *)usc->apr)->id;
	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->dest_svc = APR_SVC_USM;
	hdr->dest_domain = APR_DOMAIN_ADSP;
	hdr->src_port = (usc->session << 8) | 0x0001;
	hdr->dest_port = (usc->session << 8) | 0x0001;
	if (cmd_flg) {
		hdr->token = usc->session;
		atomic_set(&usc->cmd_state, 1);
	}
	hdr->pkt_size  = pkt_size;
	mutex_unlock(&usc->cmd_lock);
	return;
}

static uint32_t q6usm_ext2int_format(uint32_t ext_format)
{
	uint32_t int_format = INVALID_FORMAT;
	switch (ext_format) {
	case FORMAT_USPS_EPOS:
		int_format = US_POINT_EPOS_FORMAT_V2;
		break;
	case FORMAT_USRAW:
		int_format = US_RAW_FORMAT_V2;
		break;
	case FORMAT_USPROX:
		int_format = US_PROX_FORMAT_V4;
		break;
	case FORMAT_USGES_SYNC:
		int_format = US_GES_SYNC_FORMAT;
		break;
	case FORMAT_USRAW_SYNC:
		int_format = US_RAW_SYNC_FORMAT;
		break;
	default:
		pr_err("%s: Invalid format[%d]\n", __func__, ext_format);
		break;
	}

	return int_format;
}

int q6usm_open_read(struct us_client *usc,
		    uint32_t format)
{
	uint32_t int_format = INVALID_FORMAT;
	int rc = 0x00;
	struct usm_stream_cmd_open_read open;

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: client or its apr is NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: session[%d]", __func__, usc->session);

	q6usm_add_hdr(usc, &open.hdr, sizeof(open), true);
	open.hdr.opcode = USM_STREAM_CMD_OPEN_READ;
	open.src_endpoint = 0; /* AFE */
	open.pre_proc_top = 0; /* No preprocessing required */

	int_format = q6usm_ext2int_format(format);
	if (int_format == INVALID_FORMAT)
		return -EINVAL;

	open.uMode = STREAM_PRIORITY_NORMAL;
	open.format = int_format;

	rc = apr_send_pkt(usc->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("%s: open failed op[0x%x]rc[%d]\n",
		       __func__, open.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(usc->cmd_wait,
				(atomic_read(&usc->cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: timeout, waited for OPEN_READ rc[%d]\n",
		       __func__, rc);
		goto fail_cmd;
	} else
		rc = 0;
fail_cmd:
	return rc;
}


int q6usm_enc_cfg_blk(struct us_client *usc, struct us_encdec_cfg *us_cfg)
{
	uint32_t int_format = INVALID_FORMAT;
	struct usm_stream_cmd_encdec_cfg_blk  enc_cfg_obj;
	struct usm_stream_cmd_encdec_cfg_blk  *enc_cfg = &enc_cfg_obj;
	int rc = 0;
	uint32_t total_cfg_size =
		sizeof(struct usm_stream_cmd_encdec_cfg_blk);
	uint32_t round_params_size = 0;
	uint8_t  is_allocated = 0;


	if ((usc == NULL) || (us_cfg == NULL)) {
		pr_err("%s: wrong input", __func__);
		return -EINVAL;
	}

	int_format = q6usm_ext2int_format(us_cfg->format_id);
	if (int_format == INVALID_FORMAT) {
		pr_err("%s: wrong input format[%d]",
		       __func__, us_cfg->format_id);
		return -EINVAL;
	}

	/* Transparent configuration data is after enc_cfg */
	/* Integer number of u32s is requred */
	round_params_size = ((us_cfg->params_size + 3)/4) * 4;
	if (round_params_size > USM_MAX_CFG_DATA_SIZE) {
		/* Dynamic allocated encdec_cfg_blk is required */
		/* static part use */
		round_params_size -= USM_MAX_CFG_DATA_SIZE;
		total_cfg_size += round_params_size;
		enc_cfg = kzalloc(total_cfg_size, GFP_KERNEL);
		if (enc_cfg == NULL) {
			pr_err("%s: enc_cfg[%d] allocation failed\n",
			       __func__, total_cfg_size);
			return -ENOMEM;
		}
		is_allocated = 1;
	} else
		round_params_size = 0;

	q6usm_add_hdr(usc, &enc_cfg->hdr, total_cfg_size, true);

	enc_cfg->hdr.opcode = USM_STREAM_CMD_SET_ENC_PARAM;
	enc_cfg->param_id = USM_PARAM_ID_ENCDEC_ENC_CFG_BLK;
	enc_cfg->param_size = sizeof(struct usm_encode_cfg_blk)+
				round_params_size;
	enc_cfg->enc_blk.frames_per_buf = 1;
	enc_cfg->enc_blk.format_id = int_format;
	enc_cfg->enc_blk.cfg_size = sizeof(struct usm_cfg_common)+
				    USM_MAX_CFG_DATA_SIZE +
				    round_params_size;
	memcpy(&(enc_cfg->enc_blk.cfg_common), &(us_cfg->cfg_common),
	       sizeof(struct usm_cfg_common));

	/* Transparent data copy */
	memcpy(enc_cfg->enc_blk.transp_data, us_cfg->params,
	       us_cfg->params_size);
	pr_debug("%s: cfg_size[%d], params_size[%d]\n",
		__func__,
		enc_cfg->enc_blk.cfg_size,
		us_cfg->params_size);
	pr_debug("%s: params[%d,%d,%d,%d, %d,%d,%d,%d]\n",
		__func__,
		enc_cfg->enc_blk.transp_data[0],
		enc_cfg->enc_blk.transp_data[1],
		enc_cfg->enc_blk.transp_data[2],
		enc_cfg->enc_blk.transp_data[3],
		enc_cfg->enc_blk.transp_data[4],
		enc_cfg->enc_blk.transp_data[5],
		enc_cfg->enc_blk.transp_data[6],
		enc_cfg->enc_blk.transp_data[7]
	       );
	pr_debug("%s: srate:%d, ch=%d, bps= %d;\n",
		__func__, enc_cfg->enc_blk.cfg_common.sample_rate,
		enc_cfg->enc_blk.cfg_common.ch_cfg,
		enc_cfg->enc_blk.cfg_common.bits_per_sample);
	pr_debug("dmap:[0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x]; dev_id=0x%x\n",
		enc_cfg->enc_blk.cfg_common.data_map[0],
		enc_cfg->enc_blk.cfg_common.data_map[1],
		enc_cfg->enc_blk.cfg_common.data_map[2],
		enc_cfg->enc_blk.cfg_common.data_map[3],
		enc_cfg->enc_blk.cfg_common.data_map[4],
		enc_cfg->enc_blk.cfg_common.data_map[5],
		enc_cfg->enc_blk.cfg_common.data_map[6],
		enc_cfg->enc_blk.cfg_common.data_map[7],
		enc_cfg->enc_blk.cfg_common.dev_id);

	rc = apr_send_pkt(usc->apr, (uint32_t *) enc_cfg);
	if (rc < 0) {
		pr_err("%s:Comamnd open failed\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(usc->cmd_wait,
				(atomic_read(&usc->cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: timeout opcode[0x%x]\n",
		       __func__, enc_cfg->hdr.opcode);
	} else
		rc = 0;

fail_cmd:
	if (is_allocated == 1)
		kfree(enc_cfg);

	return rc;
}

int q6usm_dec_cfg_blk(struct us_client *usc, struct us_encdec_cfg *us_cfg)
{

	uint32_t int_format = INVALID_FORMAT;
	struct usm_stream_media_format_update dec_cfg_obj;
	struct usm_stream_media_format_update *dec_cfg = &dec_cfg_obj;

	int rc = 0;
	uint32_t total_cfg_size = sizeof(struct usm_stream_media_format_update);
	uint32_t round_params_size = 0;
	uint8_t  is_allocated = 0;


	if ((usc == NULL) || (us_cfg == NULL)) {
		pr_err("%s: wrong input", __func__);
		return -EINVAL;
	}

	int_format = q6usm_ext2int_format(us_cfg->format_id);
	if (int_format == INVALID_FORMAT) {
		pr_err("%s: wrong input format[%d]",
		       __func__, us_cfg->format_id);
		return -EINVAL;
	}

	/* Transparent configuration data is after enc_cfg */
	/* Integer number of u32s is requred */
	round_params_size = ((us_cfg->params_size + 3)/4) * 4;
	if (round_params_size > USM_MAX_CFG_DATA_SIZE) {
		/* Dynamic allocated encdec_cfg_blk is required */
		/* static part use */
		round_params_size -= USM_MAX_CFG_DATA_SIZE;
		total_cfg_size += round_params_size;
		dec_cfg = kzalloc(total_cfg_size, GFP_KERNEL);
		if (dec_cfg == NULL) {
			pr_err("%s:dec_cfg[%d] allocation failed\n",
			       __func__, total_cfg_size);
			return -ENOMEM;
		}
		is_allocated = 1;
	} else { /* static transp_data is enough */
		round_params_size = 0;
	}

	q6usm_add_hdr(usc, &dec_cfg->hdr, total_cfg_size, true);

	dec_cfg->hdr.opcode = USM_DATA_CMD_MEDIA_FORMAT_UPDATE;
	dec_cfg->format_id = int_format;
	dec_cfg->cfg_size = sizeof(struct usm_cfg_common) +
			    USM_MAX_CFG_DATA_SIZE +
			    round_params_size;
	memcpy(&(dec_cfg->cfg_common), &(us_cfg->cfg_common),
	       sizeof(struct usm_cfg_common));
	/* Transparent data copy */
	memcpy(dec_cfg->transp_data, us_cfg->params, us_cfg->params_size);
	pr_debug("%s: cfg_size[%d], params_size[%d]; parambytes[%d,%d,%d,%d]\n",
		__func__,
		dec_cfg->cfg_size,
		us_cfg->params_size,
		dec_cfg->transp_data[0],
		dec_cfg->transp_data[1],
		dec_cfg->transp_data[2],
		dec_cfg->transp_data[3]
	       );

	rc = apr_send_pkt(usc->apr, (uint32_t *) dec_cfg);
	if (rc < 0) {
		pr_err("%s:Comamnd open failed\n", __func__);
		rc = -EINVAL;
		goto fail_cmd;
	}
	rc = wait_event_timeout(usc->cmd_wait,
				(atomic_read(&usc->cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: timeout opcode[0x%x]\n",
		       __func__, dec_cfg->hdr.opcode);
	} else
		rc = 0;

fail_cmd:
	if (is_allocated == 1)
		kfree(dec_cfg);

	return rc;
}

int q6usm_open_write(struct us_client *usc,
		     uint32_t format)
{
	int rc = 0;
	uint32_t int_format = INVALID_FORMAT;
	struct usm_stream_cmd_open_write open;

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	pr_debug("%s: session[%d]", __func__, usc->session);

	q6usm_add_hdr(usc, &open.hdr, sizeof(open), true);
	open.hdr.opcode = USM_STREAM_CMD_OPEN_WRITE;

	int_format = q6usm_ext2int_format(format);
	if (int_format == INVALID_FORMAT) {
		pr_err("%s: wrong format[%d]", __func__, format);
		return -EINVAL;
	}

	open.format = int_format;

	rc = apr_send_pkt(usc->apr, (uint32_t *) &open);
	if (rc < 0) {
		pr_err("%s:open failed op[0x%x]rc[%d]\n", \
		       __func__, open.hdr.opcode, rc);
		goto fail_cmd;
	}
	rc = wait_event_timeout(usc->cmd_wait,
				(atomic_read(&usc->cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s:timeout. waited for OPEN_WRITR rc[%d]\n",
		       __func__, rc);
		goto fail_cmd;
	} else
		rc = 0;

fail_cmd:
	return rc;
}

int q6usm_run(struct us_client *usc, uint32_t flags,
	      uint32_t msw_ts, uint32_t lsw_ts)
{
	struct usm_stream_cmd_run run;
	int rc = 0;

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	q6usm_add_hdr(usc, &run.hdr, sizeof(run), true);

	run.hdr.opcode = USM_SESSION_CMD_RUN;
	run.flags    = flags;
	run.msw_ts   = msw_ts;
	run.lsw_ts   = lsw_ts;

	rc = apr_send_pkt(usc->apr, (uint32_t *) &run);
	if (rc < 0) {
		pr_err("%s: Commmand run failed[%d]\n", __func__, rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(usc->cmd_wait,
				(atomic_read(&usc->cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: timeout. waited for run success rc[%d]\n",
		       __func__, rc);
	} else
		rc = 0;

fail_cmd:
	return rc;
}



int q6usm_read(struct us_client *usc, uint32_t read_ind)
{
	struct usm_stream_cmd_read read;
	struct us_port_data *port = NULL;
	int rc = 0;
	u32 read_counter = 0;
	u32 loop_ind = 0;
	u64 buf_addr = 0;

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	port = &usc->port[OUT];

	if (read_ind > port->buf_cnt) {
		pr_err("%s: wrong read_ind[%d]\n",
		       __func__, read_ind);
		return -EINVAL;
	}
	if (read_ind == port->cpu_buf) {
		pr_err("%s: no free region\n", __func__);
		return 0;
	}

	if (read_ind > port->cpu_buf) { /* 1 range */
		read_counter = read_ind - port->cpu_buf;
	} else { /* 2 ranges */
		read_counter = (port->buf_cnt - port->cpu_buf) + read_ind;
	}

	q6usm_add_hdr(usc, &read.hdr, sizeof(read), false);

	read.hdr.opcode = USM_DATA_CMD_READ;
	read.buf_size = port->buf_size;
	buf_addr = (u64)(port->phys) + port->buf_size * (port->cpu_buf);
	read.buf_addr_lsw = lower_32_bits(buf_addr);
	read.buf_addr_msw = msm_audio_populate_upper_32_bits(buf_addr);
	read.mem_map_handle = *((uint32_t *)(port->ext));

	for (loop_ind = 0; loop_ind < read_counter; ++loop_ind) {
		u32 temp_cpu_buf = port->cpu_buf;

		buf_addr = (u64)(port->phys) +
				port->buf_size * (port->cpu_buf);
		read.buf_addr_lsw = lower_32_bits(buf_addr);
		read.buf_addr_msw = msm_audio_populate_upper_32_bits(buf_addr);
		read.seq_id = port->cpu_buf;
		read.hdr.token = port->cpu_buf;
		read.counter = 1;

		++(port->cpu_buf);
		if (port->cpu_buf == port->buf_cnt)
			port->cpu_buf = 0;

		rc = apr_send_pkt(usc->apr, (uint32_t *) &read);

		if (rc < 0) {
			port->cpu_buf = temp_cpu_buf;

			pr_err("%s:read op[0x%x]rc[%d]\n",
			       __func__, read.hdr.opcode, rc);
			break;
		} else
			rc = 0;
	} /* bufs loop */

	return rc;
}

int q6usm_write(struct us_client *usc, uint32_t write_ind)
{
	int rc = 0;
	struct usm_stream_cmd_write cmd_write;
	struct us_port_data *port = NULL;
	u32 current_dsp_buf = 0;
	u64 buf_addr = 0;

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	port = &usc->port[IN];

	current_dsp_buf = port->dsp_buf;
	/* free region, caused by new dsp_buf report from DSP, */
	/* can be only extended */
	if (port->cpu_buf >= current_dsp_buf) {
		/* 2 -part free region, including empty buffer */
		if ((write_ind <= port->cpu_buf)  &&
		    (write_ind > current_dsp_buf)) {
			pr_err("%s: wrong w_ind[%d]; d_buf=%d; c_buf=%d\n",
			       __func__, write_ind,
			       current_dsp_buf, port->cpu_buf);
			return -EINVAL;
		}
	} else {
		/* 1 -part free region */
		if ((write_ind <= port->cpu_buf)  ||
		    (write_ind > current_dsp_buf)) {
			pr_err("%s: wrong w_ind[%d]; d_buf=%d; c_buf=%d\n",
			       __func__, write_ind,
			       current_dsp_buf, port->cpu_buf);
			return -EINVAL;
		}
	}

	q6usm_add_hdr(usc, &cmd_write.hdr, sizeof(cmd_write), false);

	cmd_write.hdr.opcode = USM_DATA_CMD_WRITE;
	cmd_write.buf_size = port->buf_size;
	buf_addr = (u64)(port->phys) + port->buf_size * (port->cpu_buf);
	cmd_write.buf_addr_lsw = lower_32_bits(buf_addr);
	cmd_write.buf_addr_msw = msm_audio_populate_upper_32_bits(buf_addr);
	cmd_write.mem_map_handle = *((uint32_t *)(port->ext));
	cmd_write.res0 = 0;
	cmd_write.res1 = 0;
	cmd_write.res2 = 0;

	while (port->cpu_buf != write_ind) {
		u32 temp_cpu_buf = port->cpu_buf;

		buf_addr = (u64)(port->phys) +
				port->buf_size * (port->cpu_buf);
		cmd_write.buf_addr_lsw = lower_32_bits(buf_addr);
		cmd_write.buf_addr_msw =
				msm_audio_populate_upper_32_bits(buf_addr);
		cmd_write.seq_id = port->cpu_buf;
		cmd_write.hdr.token = port->cpu_buf;

		++(port->cpu_buf);
		if (port->cpu_buf == port->buf_cnt)
			port->cpu_buf = 0;

		rc = apr_send_pkt(usc->apr, (uint32_t *) &cmd_write);

		if (rc < 0) {
			port->cpu_buf = temp_cpu_buf;
			pr_err("%s:write op[0x%x];rc[%d];cpu_buf[%d]\n",
			       __func__, cmd_write.hdr.opcode,
			       rc, port->cpu_buf);
			break;
		}

		rc = 0;
	}

	return rc;
}

bool q6usm_is_write_buf_full(struct us_client *usc, uint32_t *free_region)
{
	struct us_port_data *port = NULL;
	u32 cpu_buf = 0;

	if ((usc == NULL) || !free_region) {
		pr_err("%s: input data wrong\n", __func__);
		return false;
	}
	port = &usc->port[IN];
	cpu_buf = port->cpu_buf + 1;
	if (cpu_buf == port->buf_cnt)
		cpu_buf = 0;

	*free_region = port->dsp_buf;

	return cpu_buf == *free_region;
}

int q6usm_cmd(struct us_client *usc, int cmd)
{
	struct apr_hdr hdr;
	int rc = 0;
	atomic_t *state;

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	q6usm_add_hdr(usc, &hdr, sizeof(hdr), true);
	switch (cmd) {
	case CMD_CLOSE:
		hdr.opcode = USM_STREAM_CMD_CLOSE;
		state = &usc->cmd_state;
		break;

	default:
		pr_err("%s:Invalid format[%d]\n", __func__, cmd);
		goto fail_cmd;
	}

	rc = apr_send_pkt(usc->apr, (uint32_t *) &hdr);
	if (rc < 0) {
		pr_err("%s: Command 0x%x failed\n", __func__, hdr.opcode);
		goto fail_cmd;
	}
	rc = wait_event_timeout(usc->cmd_wait, (atomic_read(state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s:timeout. waited for response opcode[0x%x]\n",
		       __func__, hdr.opcode);
	} else
		rc = 0;
fail_cmd:
	return rc;
}

int q6usm_set_us_detection(struct us_client *usc,
			   struct usm_session_cmd_detect_info *detect_info,
			   uint16_t detect_info_size)
{
	int rc = 0;

	if ((usc == NULL) ||
	    (detect_info_size == 0) ||
	    (detect_info == NULL)) {
		pr_err("%s: wrong input: usc=0x%pK, inf_size=%d; info=0x%pK",
		       __func__,
		       usc,
		       detect_info_size,
		       detect_info);
		return -EINVAL;
	}

	q6usm_add_hdr(usc, &detect_info->hdr, detect_info_size, true);

	detect_info->hdr.opcode = USM_SESSION_CMD_SIGNAL_DETECT_MODE;

	rc = apr_send_pkt(usc->apr, (uint32_t *)detect_info);
	if (rc < 0) {
		pr_err("%s:Comamnd signal detect failed\n", __func__);
		return -EINVAL;
	}
	rc = wait_event_timeout(usc->cmd_wait,
				(atomic_read(&usc->cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: CMD_SIGNAL_DETECT_MODE: timeout=%d\n",
		       __func__, Q6USM_TIMEOUT_JIFFIES);
	} else
		rc = 0;

	return rc;
}

int q6usm_set_us_stream_param(int dir, struct us_client *usc,
		uint32_t module_id, uint32_t param_id, uint32_t buf_size)
{
	int rc = 0;
	struct usm_stream_cmd_set_param cmd_set_param;
	struct us_port_data *port = NULL;

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	port = &usc->port[dir];

	q6usm_add_hdr(usc, &cmd_set_param.hdr, sizeof(cmd_set_param), true);

	cmd_set_param.hdr.opcode = USM_STREAM_CMD_SET_PARAM;
	cmd_set_param.buf_size = buf_size;
	cmd_set_param.buf_addr_msw =
			msm_audio_populate_upper_32_bits(port->param_phys);
	cmd_set_param.buf_addr_lsw = lower_32_bits(port->param_phys);
	cmd_set_param.mem_map_handle =
			*((uint32_t *)(port->param_buf_mem_handle));
	cmd_set_param.module_id = module_id;
	cmd_set_param.param_id = param_id;
	cmd_set_param.hdr.token = 0;

	rc = apr_send_pkt(usc->apr, (uint32_t *) &cmd_set_param);

	if (rc < 0) {
		pr_err("%s:write op[0x%x];rc[%d]\n",
			__func__, cmd_set_param.hdr.opcode, rc);
	}

	rc = wait_event_timeout(usc->cmd_wait,
				(atomic_read(&usc->cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: CMD_SET_PARAM: timeout=%d\n",
			__func__, Q6USM_TIMEOUT_JIFFIES);
	} else
		rc = 0;

	return rc;
}

int q6usm_get_us_stream_param(int dir, struct us_client *usc,
		uint32_t module_id, uint32_t param_id, uint32_t buf_size)
{
	int rc = 0;
	struct usm_stream_cmd_get_param cmd_get_param;
	struct us_port_data *port = NULL;

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}
	port = &usc->port[dir];

	q6usm_add_hdr(usc, &cmd_get_param.hdr, sizeof(cmd_get_param), true);

	cmd_get_param.hdr.opcode = USM_STREAM_CMD_GET_PARAM;
	cmd_get_param.buf_size = buf_size;
	cmd_get_param.buf_addr_msw =
			msm_audio_populate_upper_32_bits(port->param_phys);
	cmd_get_param.buf_addr_lsw = lower_32_bits(port->param_phys);
	cmd_get_param.mem_map_handle =
			*((uint32_t *)(port->param_buf_mem_handle));
	cmd_get_param.module_id = module_id;
	cmd_get_param.param_id = param_id;
	cmd_get_param.hdr.token = 0;

	rc = apr_send_pkt(usc->apr, (uint32_t *) &cmd_get_param);

	if (rc < 0) {
		pr_err("%s:write op[0x%x];rc[%d]\n",
			__func__, cmd_get_param.hdr.opcode, rc);
	}

	rc = wait_event_timeout(usc->cmd_wait,
				(atomic_read(&usc->cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: CMD_GET_PARAM: timeout=%d\n",
			__func__, Q6USM_TIMEOUT_JIFFIES);
	} else
		rc = 0;

	return rc;
}

static int __init q6usm_init(void)
{
	pr_debug("%s\n", __func__);
	init_waitqueue_head(&this_mmap.cmd_wait);
	memset(session, 0, sizeof(session));
	return 0;
}

device_initcall(q6usm_init);
