/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/dma-mapping.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/msm_audio.h>
#include <sound/apr_audio.h>
#include <mach/qdsp6v2/apr_us_a.h>
#include "q6usm.h"

#define SESSION_MAX 0x02 /* aDSP:USM limit */

#define READDONE_IDX_STATUS     0
#define READDONE_IDX_BUFFER     1
#define READDONE_IDX_SIZE       2
#define READDONE_IDX_OFFSET     3
#define READDONE_IDX_MSW_TS     4
#define READDONE_IDX_LSW_TS     5
#define READDONE_IDX_FLAGS      6
#define READDONE_IDX_NUMFRAMES  7
#define READDONE_IDX_ID         8

#define WRITEDONE_IDX_STATUS    0

/* Standard timeout in the asynchronous ops */
#define Q6USM_TIMEOUT_JIFFIES	(1*HZ) /* 1 sec */

static DEFINE_MUTEX(session_lock);

static struct us_client *session[SESSION_MAX];
static int32_t q6usm_mmapcallback(struct apr_client_data *data, void *priv);
static int32_t q6usm_callback(struct apr_client_data *data, void *priv);
static void q6usm_add_hdr(struct us_client *usc, struct apr_hdr *hdr,
			  uint32_t pkt_size, bool cmd_flg);

struct usm_mmap {
	atomic_t ref_cnt;
	atomic_t cmd_state;
	wait_queue_head_t cmd_wait;
	void *apr;
};

static struct usm_mmap this_mmap;

static void q6usm_add_mmaphdr(struct us_client *usc, struct apr_hdr *hdr,
			      uint32_t pkt_size, bool cmd_flg)
{
	hdr->hdr_field = APR_HDR_FIELD(APR_MSG_TYPE_SEQ_CMD, \
				       APR_HDR_LEN(APR_HDR_SIZE), APR_PKT_VER);
	hdr->src_port = 0;
	hdr->dest_port = 0;
	if (cmd_flg) {
		hdr->token = 0;
		atomic_set(&this_mmap.cmd_state, 1);
	}
	hdr->pkt_size  = pkt_size;
	return;
}

static int q6usm_memory_map(struct us_client *usc, uint32_t buf_add, int dir,
		     uint32_t bufsz, uint32_t bufcnt)
{
	struct usm_stream_cmd_memory_map mem_map;
	int rc = 0;

	if ((usc == NULL) || (usc->apr == NULL) || (this_mmap.apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	q6usm_add_mmaphdr(usc, &mem_map.hdr,
			  sizeof(struct usm_stream_cmd_memory_map), true);
	mem_map.hdr.opcode = USM_SESSION_CMD_MEMORY_MAP;

	mem_map.buf_add = buf_add;
	mem_map.buf_size = bufsz * bufcnt;
	mem_map.mempool_id = 0;

	pr_debug("%s: buf add[%x]  buf_add_parameter[%x]\n",
		 __func__, mem_map.buf_add, buf_add);

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) &mem_map);
	if (rc < 0) {
		pr_err("%s: mem_map op[0x%x]rc[%d]\n",
		       __func__, mem_map.hdr.opcode, rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
				(atomic_read(&this_mmap.cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: timeout. waited for memory_map\n", __func__);
	} else
		rc = 0;
fail_cmd:
	return rc;
}

int q6usm_memory_unmap(struct us_client *usc, uint32_t buf_add, int dir)
{
	struct usm_stream_cmd_memory_unmap mem_unmap;
	int rc = 0;

	if ((usc == NULL) || (usc->apr == NULL) || (this_mmap.apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

	q6usm_add_mmaphdr(usc, &mem_unmap.hdr,
			  sizeof(struct usm_stream_cmd_memory_unmap), true);
	mem_unmap.hdr.opcode = USM_SESSION_CMD_MEMORY_UNMAP;
	mem_unmap.buf_add = buf_add;

	rc = apr_send_pkt(this_mmap.apr, (uint32_t *) &mem_unmap);
	if (rc < 0) {
		pr_err("%s:mem_unmap op[0x%x]rc[%d]\n",
		       __func__, mem_unmap.hdr.opcode, rc);
		goto fail_cmd;
	}

	rc = wait_event_timeout(this_mmap.cmd_wait,
				(atomic_read(&this_mmap.cmd_state) == 0),
				Q6USM_TIMEOUT_JIFFIES);
	if (!rc) {
		rc = -ETIME;
		pr_err("%s: timeout. waited for memory_map\n", __func__);
	} else
		rc = 0;
fail_cmd:
	return rc;
}

static int q6usm_session_alloc(struct us_client *usc)
{
	int ind = 0;

	mutex_lock(&session_lock);
	for (ind = 0; ind < SESSION_MAX; ++ind) {
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
	if (ind < SESSION_MAX) {
		mutex_lock(&session_lock);
		session[ind] = 0;
		mutex_unlock(&session_lock);
	}
}

int q6usm_us_client_buf_free(unsigned int dir,
			     struct us_client *usc)
{
	struct us_port_data *port;
	int rc = 0;
	uint32_t size = 0;

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

	rc = q6usm_memory_unmap(usc, port->phys, dir);
	if (rc)
		pr_err("%s: CMD Memory_unmap* failed\n", __func__);

	pr_debug("%s: data[%p]phys[%p][%p]\n", __func__,
		 (void *)port->data, (void *)port->phys, (void *)&port->phys);
	size = port->buf_size * port->buf_cnt;
	dma_free_coherent(NULL, size, port->data, port->phys);
	port->data = NULL;
	port->phys = 0;
	port->buf_size = 0;
	port->buf_cnt = 0;

	mutex_unlock(&usc->cmd_lock);
	return 0;
}

void q6usm_us_client_free(struct us_client *usc)
{
	int loopcnt = 0;
	struct us_port_data *port;

	if ((usc == NULL) ||
	    !(usc->session))
		return;

	for (loopcnt = 0; loopcnt <= OUT; ++loopcnt) {
		port = &usc->port[loopcnt];
		if (port->data == NULL)
			continue;
		pr_debug("%s: loopcnt = %d\n", __func__, loopcnt);
		q6usm_us_client_buf_free(loopcnt, usc);
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
	kfree(usc);
	pr_debug("%s:\n", __func__);
	return;
}

struct us_client *q6usm_us_client_alloc(
	void (*cb)(uint32_t, uint32_t, uint32_t *, void *),
	void *priv)
{
	struct us_client *usc;
	int n;
	int lcnt = 0;

	usc = kzalloc(sizeof(struct us_client), GFP_KERNEL);
	if (usc == NULL)
		return NULL;
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
	}
	atomic_set(&usc->cmd_state, 0);

	return usc;
fail:
	q6usm_us_client_free(usc);
	return NULL;
fail_session:
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

	if ((usc == NULL) ||
	    ((dir != IN) && (dir != OUT)) || (size == 0) ||
	    (usc->session <= 0 || usc->session > SESSION_MAX)) {
		pr_err("%s: wrong parameters: size=%d; bufcnt=%d\n",
		       __func__, size, bufcnt);
		return -EINVAL;
	}

	mutex_lock(&usc->cmd_lock);

	port = &usc->port[dir];

	port->data = dma_alloc_coherent(NULL, size, &(port->phys), GFP_KERNEL);
	if (port->data == NULL) {
		pr_err("%s: US region allocation failed\n", __func__);
		mutex_unlock(&usc->cmd_lock);
		return -ENOMEM;
	}

	port->buf_cnt = bufcnt;
	port->buf_size = bufsz;
	pr_debug("%s: data[%p]; phys[%p]; [%p]\n", __func__,
		 (void *)port->data,
		 (void *)port->phys,
		 (void *)&port->phys);

	rc = q6usm_memory_map(usc, port->phys, dir, size, 1);
	if (rc < 0) {
		pr_err("%s: CMD Memory_map failed\n", __func__);
		mutex_unlock(&usc->cmd_lock);
		q6usm_us_client_buf_free(dir, usc);
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
			case USM_SESSION_CMD_MEMORY_MAP:
			case USM_SESSION_CMD_MEMORY_UNMAP:
				pr_debug("%s: cmd[0x%x]; result[0x%x]\n",
					 __func__, payload[0], payload[1]);
				if (atomic_read(&this_mmap.cmd_state)) {
					atomic_set(&this_mmap.cmd_state, 0);
					wake_up(&this_mmap.cmd_wait);
				}
				break;
			default:
				pr_debug("%s: wrong command[0x%x]\n",
					 __func__, payload[0]);
				break;
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
		ret = dma_mmap_coherent(NULL, vms,
					port->data, port->phys,
					port->buf_size * port->buf_cnt);
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
	hdr->pkt_size  = APR_PKT_SIZE(APR_HDR_SIZE, pkt_size);
	mutex_unlock(&usc->cmd_lock);
	return;
}

static uint32_t q6usm_ext2int_format(uint32_t ext_format)
{
	uint32_t int_format = INVALID_FORMAT;
	switch (ext_format) {
	case FORMAT_USPS_EPOS:
		int_format = US_POINT_EPOS_FORMAT;
		break;
	case FORMAT_USRAW:
		int_format = US_RAW_FORMAT;
		break;
	case FORMAT_USPROX:
		int_format = US_PROX_FORMAT;
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

	pr_debug("%s: session[%d]", __func__, usc->session);

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: client or its apr is NULL\n", __func__);
		return -EINVAL;
	}

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
	size_t min_common_size;

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

	q6usm_add_hdr(usc, &enc_cfg->hdr, total_cfg_size - APR_HDR_SIZE, true);

	enc_cfg->hdr.opcode = USM_STREAM_CMD_SET_ENC_PARAM;
	enc_cfg->param_id = USM_PARAM_ID_ENCDEC_ENC_CFG_BLK;
	enc_cfg->param_size = sizeof(struct usm_encode_cfg_blk)+
				round_params_size;
	enc_cfg->enc_blk.frames_per_buf = 1;
	enc_cfg->enc_blk.format_id = int_format;
	min_common_size = min(sizeof(struct usm_cfg_common),
			      sizeof(struct usm_cfg_common_a));
	enc_cfg->enc_blk.cfg_size = min_common_size +
				    USM_MAX_CFG_DATA_SIZE +
				    round_params_size;
	memcpy(&(enc_cfg->enc_blk.cfg_common), &(us_cfg->cfg_common),
	       min_common_size);
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
	pr_debug("%s: srate:%d, ch=%d, bps= %d; dmap:[0x%x,0x%x,0x%x,0x%x];\n",
		__func__, enc_cfg->enc_blk.cfg_common.sample_rate,
		enc_cfg->enc_blk.cfg_common.ch_cfg,
		enc_cfg->enc_blk.cfg_common.bits_per_sample,
		enc_cfg->enc_blk.cfg_common.data_map[0],
		enc_cfg->enc_blk.cfg_common.data_map[1],
		enc_cfg->enc_blk.cfg_common.data_map[2],
		enc_cfg->enc_blk.cfg_common.data_map[3]);
	pr_debug("dev_id=0x%x\n",
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
	size_t min_common_size;

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

	q6usm_add_hdr(usc, &dec_cfg->hdr, total_cfg_size - APR_HDR_SIZE, true);

	dec_cfg->hdr.opcode = USM_DATA_CMD_MEDIA_FORMAT_UPDATE;
	dec_cfg->format_id = int_format;
	min_common_size = min(sizeof(struct usm_cfg_common),
			      sizeof(struct usm_cfg_common_a));
	dec_cfg->cfg_size = min_common_size +
			    USM_MAX_CFG_DATA_SIZE +
			    round_params_size;
	memcpy(&(dec_cfg->cfg_common), &(us_cfg->cfg_common),
	       min_common_size);
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

	pr_debug("%s: session[%d]", __func__, usc->session);

	if ((usc == NULL) || (usc->apr == NULL)) {
		pr_err("%s: APR handle NULL\n", __func__);
		return -EINVAL;
	}

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

	q6usm_add_hdr(usc, &read.hdr, (sizeof(read) - APR_HDR_SIZE), false);

	read.hdr.opcode = USM_DATA_CMD_READ;
	read.buf_size = port->buf_size;

	for (loop_ind = 0; loop_ind < read_counter; ++loop_ind) {
		u32 temp_cpu_buf = port->cpu_buf;

		read.buf_add = (uint32_t)(port->phys) +
			       port->buf_size * (port->cpu_buf);
		read.uid = port->cpu_buf;
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

	q6usm_add_hdr(usc, &cmd_write.hdr,
		      (sizeof(cmd_write) - APR_HDR_SIZE), false);

	cmd_write.hdr.opcode = USM_DATA_CMD_WRITE;
	cmd_write.buf_size = port->buf_size;

	while (port->cpu_buf != write_ind) {
		u32 temp_cpu_buf = port->cpu_buf;

		cmd_write.buf_add = (uint32_t)(port->phys) +
				    port->buf_size * (port->cpu_buf);
		cmd_write.uid = port->cpu_buf;
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
	q6usm_add_hdr(usc, &hdr, (sizeof(hdr) - APR_HDR_SIZE), true);
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
		pr_err("%s: wrong input: usc=0x%p, inf_size=%d; info=0x%p",
		       __func__,
		       usc,
		       detect_info_size,
		       detect_info);
		return -EINVAL;
	}

	q6usm_add_hdr(usc, &detect_info->hdr,
		      detect_info_size - APR_HDR_SIZE, true);

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

static int __init q6usm_init(void)
{
	pr_debug("%s\n", __func__);
	init_waitqueue_head(&this_mmap.cmd_wait);
	memset(session, 0, sizeof(session));
	return 0;
}

device_initcall(q6usm_init);
