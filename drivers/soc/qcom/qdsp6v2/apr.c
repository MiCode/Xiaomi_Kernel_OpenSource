/* Copyright (c) 2010-2014, 2016 The Linux Foundation. All rights reserved.
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
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/list.h>
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/debugfs.h>
#include <linux/platform_device.h>
#include <linux/sysfs.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/scm.h>
#include <sound/apr_audio-v2.h>
#include <soc/qcom/smd.h>
#include <linux/qdsp6v2/apr.h>
#include <linux/qdsp6v2/apr_tal.h>
#include <linux/qdsp6v2/dsp_debug.h>
#include <linux/ipc_logging.h>

#define SCM_Q6_NMI_CMD 0x1
#define APR_PKT_IPC_LOG_PAGE_CNT 2

static struct apr_q6 q6;
static struct apr_client client[APR_DEST_MAX][APR_CLIENT_MAX];
static void *apr_pkt_ctx;
static wait_queue_head_t dsp_wait;
static wait_queue_head_t modem_wait;
static bool is_modem_up;
/* Subsystem restart: QDSP6 data, functions */
static struct workqueue_struct *apr_reset_workqueue;
static void apr_reset_deregister(struct work_struct *work);
struct apr_reset_work {
	void *handle;
	struct work_struct work;
};

#define APR_PKT_INFO(x...) \
do { \
	if (apr_pkt_ctx) \
		ipc_log_string(apr_pkt_ctx, "<APR>: "x); \
} while (0)


struct apr_svc_table {
	char name[64];
	int idx;
	int id;
	int client_id;
};

static const struct apr_svc_table svc_tbl_qdsp6[] = {
	{
		.name = "AFE",
		.idx = 0,
		.id = APR_SVC_AFE,
		.client_id = APR_CLIENT_AUDIO,
	},
	{
		.name = "ASM",
		.idx = 1,
		.id = APR_SVC_ASM,
		.client_id = APR_CLIENT_AUDIO,
	},
	{
		.name = "ADM",
		.idx = 2,
		.id = APR_SVC_ADM,
		.client_id = APR_CLIENT_AUDIO,
	},
	{
		.name = "CORE",
		.idx = 3,
		.id = APR_SVC_ADSP_CORE,
		.client_id = APR_CLIENT_AUDIO,
	},
	{
		.name = "TEST",
		.idx = 4,
		.id = APR_SVC_TEST_CLIENT,
		.client_id = APR_CLIENT_AUDIO,
	},
	{
		.name = "MVM",
		.idx = 5,
		.id = APR_SVC_ADSP_MVM,
		.client_id = APR_CLIENT_AUDIO,
	},
	{
		.name = "CVS",
		.idx = 6,
		.id = APR_SVC_ADSP_CVS,
		.client_id = APR_CLIENT_AUDIO,
	},
	{
		.name = "CVP",
		.idx = 7,
		.id = APR_SVC_ADSP_CVP,
		.client_id = APR_CLIENT_AUDIO,
	},
	{
		.name = "USM",
		.idx = 8,
		.id = APR_SVC_USM,
		.client_id = APR_CLIENT_AUDIO,
	},
	{
		.name = "VIDC",
		.idx = 9,
		.id = APR_SVC_VIDC,
	},
	{
		.name = "LSM",
		.idx = 10,
		.id = APR_SVC_LSM,
		.client_id = APR_CLIENT_AUDIO,
	},
};

static struct apr_svc_table svc_tbl_voice[] = {
	{
		.name = "VSM",
		.idx = 0,
		.id = APR_SVC_VSM,
		.client_id = APR_CLIENT_VOICE,
	},
	{
		.name = "VPM",
		.idx = 1,
		.id = APR_SVC_VPM,
		.client_id = APR_CLIENT_VOICE,
	},
	{
		.name = "MVS",
		.idx = 2,
		.id = APR_SVC_MVS,
		.client_id = APR_CLIENT_VOICE,
	},
	{
		.name = "MVM",
		.idx = 3,
		.id = APR_SVC_MVM,
		.client_id = APR_CLIENT_VOICE,
	},
	{
		.name = "CVS",
		.idx = 4,
		.id = APR_SVC_CVS,
		.client_id = APR_CLIENT_VOICE,
	},
	{
		.name = "CVP",
		.idx = 5,
		.id = APR_SVC_CVP,
		.client_id = APR_CLIENT_VOICE,
	},
	{
		.name = "SRD",
		.idx = 6,
		.id = APR_SVC_SRD,
		.client_id = APR_CLIENT_VOICE,
	},
	{
		.name = "TEST",
		.idx = 7,
		.id = APR_SVC_TEST_CLIENT,
		.client_id = APR_CLIENT_VOICE,
	},
};

enum apr_subsys_state apr_get_modem_state(void)
{
	return atomic_read(&q6.modem_state);
}

void apr_set_modem_state(enum apr_subsys_state state)
{
	atomic_set(&q6.modem_state, state);
}

enum apr_subsys_state apr_cmpxchg_modem_state(enum apr_subsys_state prev,
					      enum apr_subsys_state new)
{
	return atomic_cmpxchg(&q6.modem_state, prev, new);
}

enum apr_subsys_state apr_get_q6_state(void)
{
	return atomic_read(&q6.q6_state);
}
EXPORT_SYMBOL_GPL(apr_get_q6_state);

int apr_set_q6_state(enum apr_subsys_state state)
{
	pr_debug("%s: setting adsp state %d\n", __func__, state);
	if (state < APR_SUBSYS_DOWN || state > APR_SUBSYS_LOADED)
		return -EINVAL;
	atomic_set(&q6.q6_state, state);
	return 0;
}
EXPORT_SYMBOL_GPL(apr_set_q6_state);

enum apr_subsys_state apr_cmpxchg_q6_state(enum apr_subsys_state prev,
					   enum apr_subsys_state new)
{
	return atomic_cmpxchg(&q6.q6_state, prev, new);
}

int apr_wait_for_device_up(int dest_id)
{
	int rc = -1;
	if (dest_id == APR_DEST_MODEM)
		rc = wait_event_interruptible_timeout(modem_wait,
				    (apr_get_modem_state() == APR_SUBSYS_UP),
				    (1 * HZ));
	else if (dest_id == APR_DEST_QDSP6)
		rc = wait_event_interruptible_timeout(dsp_wait,
				    (apr_get_q6_state() == APR_SUBSYS_UP),
				    (1 * HZ));
	else
		pr_err("%s: unknown dest_id %d\n", __func__, dest_id);
	/* returns left time */
	return rc;
}

int apr_load_adsp_image(void)
{
	int rc = 0;
	mutex_lock(&q6.lock);
	if (apr_get_q6_state() == APR_SUBSYS_UP) {
		q6.pil = subsystem_get("adsp");
		if (IS_ERR(q6.pil)) {
			rc = PTR_ERR(q6.pil);
			pr_err("APR: Unable to load q6 image, error:%d\n", rc);
		} else {
			apr_set_q6_state(APR_SUBSYS_LOADED);
			pr_debug("APR: Image is loaded, stated\n");
		}
	} else if (apr_get_q6_state() == APR_SUBSYS_LOADED) {
		pr_debug("APR: q6 image already loaded\n");
	} else {
		pr_debug("APR: cannot load state %d\n", apr_get_q6_state());
	}
	mutex_unlock(&q6.lock);
	return rc;
}

struct apr_client *apr_get_client(int dest_id, int client_id)
{
	return &client[dest_id][client_id];
}

int apr_send_pkt(void *handle, uint32_t *buf)
{
	struct apr_svc *svc = handle;
	struct apr_client *clnt;
	struct apr_hdr *hdr;
	uint16_t dest_id;
	uint16_t client_id;
	uint16_t w_len;
	int rc;
	unsigned long flags;

	if (!handle || !buf) {
		pr_err("APR: Wrong parameters\n");
		return -EINVAL;
	}
	if (svc->need_reset) {
		pr_err("apr: send_pkt service need reset\n");
		return -ENETRESET;
	}

	if ((svc->dest_id == APR_DEST_QDSP6) &&
	    (apr_get_q6_state() != APR_SUBSYS_LOADED)) {
		pr_err("%s: Still dsp is not Up\n", __func__);
		return -ENETRESET;
	} else if ((svc->dest_id == APR_DEST_MODEM) &&
		   (apr_get_modem_state() == APR_SUBSYS_DOWN)) {
		pr_err("apr: Still Modem is not Up\n");
		return -ENETRESET;
	}

	spin_lock_irqsave(&svc->w_lock, flags);
	dest_id = svc->dest_id;
	client_id = svc->client_id;
	clnt = &client[dest_id][client_id];

	if (!client[dest_id][client_id].handle) {
		pr_err("APR: Still service is not yet opened\n");
		spin_unlock_irqrestore(&svc->w_lock, flags);
		return -EINVAL;
	}
	hdr = (struct apr_hdr *)buf;

	hdr->src_domain = APR_DOMAIN_APPS;
	hdr->src_svc = svc->id;
	hdr->dest_domain = svc->dest_domain;
	hdr->dest_svc = svc->id;

	APR_PKT_INFO("Tx: dest_svc[%d], opcode[0x%X], size[%d]",
			hdr->dest_svc, hdr->opcode, hdr->pkt_size);

	rc = apr_tal_write(clnt->handle, buf,
			(struct apr_pkt_priv *)&svc->pkt_owner,
			hdr->pkt_size);
	if (rc >= 0) {
		w_len = rc;
		if (w_len != hdr->pkt_size) {
			pr_err("%s: Unable to write whole APR pkt successfully: %d\n",
			       __func__, rc);
			rc = -EINVAL;
		}
	} else {
		pr_err("%s: Write APR pkt failed with error %d\n",
			__func__, rc);
	}
	spin_unlock_irqrestore(&svc->w_lock, flags);

	return rc;
}

int apr_pkt_config(void *handle, struct apr_pkt_cfg *cfg)
{
	struct apr_svc *svc = (struct apr_svc *)handle;
	uint16_t dest_id;
	uint16_t client_id;
	struct apr_client *clnt;

	if (!handle) {
		pr_err("%s: Invalid handle\n", __func__);
		return -EINVAL;
	}

	if (svc->need_reset) {
		pr_err("%s: service need reset\n", __func__);
		return -ENETRESET;
	}

	svc->pkt_owner = cfg->pkt_owner;
	dest_id = svc->dest_id;
	client_id = svc->client_id;
	clnt = &client[dest_id][client_id];

	return apr_tal_rx_intents_config(clnt->handle,
		cfg->intents.num_of_intents, cfg->intents.size);
}

struct apr_svc *apr_register(char *dest, char *svc_name, apr_fn svc_fn,
				uint32_t src_port, void *priv)
{
	struct apr_client *clnt;
	int client_id = 0;
	int svc_idx = 0;
	int svc_id = 0;
	int dest_id = 0;
	int domain_id = 0;
	int temp_port = 0;
	struct apr_svc *svc = NULL;
	int rc = 0;
	bool can_open_channel = true;

	if (!dest || !svc_name || !svc_fn)
		return NULL;

	if (!strcmp(dest, "ADSP"))
		domain_id = APR_DOMAIN_ADSP;
	else if (!strcmp(dest, "MODEM")) {
		/* Don't request for SMD channels if destination is MODEM,
		 * as these channels are no longer used and these clients
		 * are to listen only for MODEM SSR events
		 */
		can_open_channel = false;
		domain_id = APR_DOMAIN_MODEM;
	} else {
		pr_err("APR: wrong destination\n");
		goto done;
	}

	dest_id = apr_get_dest_id(dest);

	if (dest_id == APR_DEST_QDSP6) {
		if (apr_get_q6_state() != APR_SUBSYS_LOADED) {
			pr_err("%s: adsp not up\n", __func__);
			return NULL;
		}
		pr_debug("%s: adsp Up\n", __func__);
	} else if (dest_id == APR_DEST_MODEM) {
		if (apr_get_modem_state() == APR_SUBSYS_DOWN) {
			if (is_modem_up) {
				pr_err("%s: modem shutdown due to SSR, ret",
					__func__);
				return NULL;
			}
			pr_debug("%s: Wait for modem to bootup\n", __func__);
			rc = apr_wait_for_device_up(APR_DEST_MODEM);
			if (rc == 0) {
				pr_err("%s: Modem is not Up\n", __func__);
				return NULL;
			}
		}
		pr_debug("%s: modem Up\n", __func__);
	}

	if (apr_get_svc(svc_name, domain_id, &client_id, &svc_idx, &svc_id)) {
		pr_err("%s: apr_get_svc failed\n", __func__);
		goto done;
	}

	clnt = &client[dest_id][client_id];
	mutex_lock(&clnt->m_lock);
	if (!clnt->handle && can_open_channel) {
		clnt->handle = apr_tal_open(client_id, dest_id,
				APR_DL_SMD, apr_cb_func, NULL);
		if (!clnt->handle) {
			svc = NULL;
			pr_err("APR: Unable to open handle\n");
			mutex_unlock(&clnt->m_lock);
			goto done;
		}
	}
	mutex_unlock(&clnt->m_lock);
	svc = &clnt->svc[svc_idx];
	mutex_lock(&svc->m_lock);
	clnt->id = client_id;
	if (svc->need_reset) {
		mutex_unlock(&svc->m_lock);
		pr_err("APR: Service needs reset\n");
		goto done;
	}
	svc->id = svc_id;
	svc->dest_id = dest_id;
	svc->client_id = client_id;
	svc->dest_domain = domain_id;
	svc->pkt_owner = APR_PKT_OWNER_DRIVER;

	if (src_port != 0xFFFFFFFF) {
		temp_port = ((src_port >> 8) * 8) + (src_port & 0xFF);
		pr_debug("port = %d t_port = %d\n", src_port, temp_port);
		if (temp_port >= APR_MAX_PORTS || temp_port < 0) {
			pr_err("APR: temp_port out of bounds\n");
			mutex_unlock(&svc->m_lock);
			return NULL;
		}
		if (!svc->port_cnt && !svc->svc_cnt)
			clnt->svc_cnt++;
		svc->port_cnt++;
		svc->port_fn[temp_port] = svc_fn;
		svc->port_priv[temp_port] = priv;
	} else {
		if (!svc->fn) {
			if (!svc->port_cnt && !svc->svc_cnt)
				clnt->svc_cnt++;
			svc->fn = svc_fn;
			if (svc->port_cnt)
				svc->svc_cnt++;
			svc->priv = priv;
		}
	}

	mutex_unlock(&svc->m_lock);
done:
	return svc;
}


void apr_cb_func(void *buf, int len, void *priv)
{
	struct apr_client_data data;
	struct apr_client *apr_client;
	struct apr_svc *c_svc;
	struct apr_hdr *hdr;
	uint16_t hdr_size;
	uint16_t msg_type;
	uint16_t ver;
	uint16_t src;
	uint16_t svc;
	uint16_t clnt;
	int i;
	int temp_port = 0;
	uint32_t *ptr;

	pr_debug("APR2: len = %d\n", len);
	ptr = buf;
	pr_debug("\n*****************\n");
	for (i = 0; i < len/4; i++)
		pr_debug("%x  ", ptr[i]);
	pr_debug("\n");
	pr_debug("\n*****************\n");

	if (!buf || len <= APR_HDR_SIZE) {
		pr_err("APR: Improper apr pkt received:%pK %d\n", buf, len);
		return;
	}
	hdr = buf;
	APR_PKT_INFO("Rx: dest_svc[%d], opcode[0x%X], size[%d]",
		     hdr->dest_svc, hdr->opcode, hdr->pkt_size);

	ver = hdr->hdr_field;
	ver = (ver & 0x000F);
	if (ver > APR_PKT_VER + 1) {
		pr_err("APR: Wrong version: %d\n", ver);
		return;
	}

	hdr_size = hdr->hdr_field;
	hdr_size = ((hdr_size & 0x00F0) >> 0x4) * 4;
	if (hdr_size < APR_HDR_SIZE) {
		pr_err("APR: Wrong hdr size:%d\n", hdr_size);
		return;
	}

	if (hdr->pkt_size < APR_HDR_SIZE) {
		pr_err("APR: Wrong paket size\n");
		return;
	}
	msg_type = hdr->hdr_field;
	msg_type = (msg_type >> 0x08) & 0x0003;
	if (msg_type >= APR_MSG_TYPE_MAX && msg_type != APR_BASIC_RSP_RESULT) {
		pr_err("APR: Wrong message type: %d\n", msg_type);
		return;
	}

	if (hdr->src_domain >= APR_DOMAIN_MAX ||
		hdr->dest_domain >= APR_DOMAIN_MAX ||
		hdr->src_svc >= APR_SVC_MAX ||
		hdr->dest_svc >= APR_SVC_MAX) {
		pr_err("APR: Wrong APR header\n");
		return;
	}

	svc = hdr->dest_svc;
	if (hdr->src_domain == APR_DOMAIN_MODEM) {
		if (svc == APR_SVC_MVS || svc == APR_SVC_MVM ||
		    svc == APR_SVC_CVS || svc == APR_SVC_CVP ||
		    svc == APR_SVC_TEST_CLIENT)
			clnt = APR_CLIENT_VOICE;
		else {
			pr_err("APR: Wrong svc :%d\n", svc);
			return;
		}
	} else if (hdr->src_domain == APR_DOMAIN_ADSP) {
		if (svc == APR_SVC_AFE || svc == APR_SVC_ASM ||
		    svc == APR_SVC_VSM || svc == APR_SVC_VPM ||
		    svc == APR_SVC_ADM || svc == APR_SVC_ADSP_CORE ||
		    svc == APR_SVC_USM ||
		    svc == APR_SVC_TEST_CLIENT || svc == APR_SVC_ADSP_MVM ||
		    svc == APR_SVC_ADSP_CVS || svc == APR_SVC_ADSP_CVP ||
		    svc == APR_SVC_LSM)
			clnt = APR_CLIENT_AUDIO;
		else if (svc == APR_SVC_VIDC)
			clnt = APR_CLIENT_AUDIO;
		else {
			pr_err("APR: Wrong svc :%d\n", svc);
			return;
		}
	} else {
		pr_err("APR: Pkt from wrong source: %d\n", hdr->src_domain);
		return;
	}

	src = apr_get_data_src(hdr);
	if (src == APR_DEST_MAX)
		return;

	pr_debug("src =%d clnt = %d\n", src, clnt);
	apr_client = &client[src][clnt];
	for (i = 0; i < APR_SVC_MAX; i++)
		if (apr_client->svc[i].id == svc) {
			pr_debug("%d\n", apr_client->svc[i].id);
			c_svc = &apr_client->svc[i];
			break;
		}

	if (i == APR_SVC_MAX) {
		pr_err("APR: service is not registered\n");
		return;
	}
	pr_debug("svc_idx = %d\n", i);
	pr_debug("%x %x %x %pK %pK\n", c_svc->id, c_svc->dest_id,
		 c_svc->client_id, c_svc->fn, c_svc->priv);
	data.payload_size = hdr->pkt_size - hdr_size;
	data.opcode = hdr->opcode;
	data.src = src;
	data.src_port = hdr->src_port;
	data.dest_port = hdr->dest_port;
	data.token = hdr->token;
	data.msg_type = msg_type;
	if (data.payload_size > 0)
		data.payload = (char *)hdr + hdr_size;

	temp_port = ((data.dest_port >> 8) * 8) + (data.dest_port & 0xFF);
	pr_debug("port = %d t_port = %d\n", data.src_port, temp_port);
	if (c_svc->port_cnt && c_svc->port_fn[temp_port])
		c_svc->port_fn[temp_port](&data,  c_svc->port_priv[temp_port]);
	else if (c_svc->fn)
		c_svc->fn(&data, c_svc->priv);
	else
		pr_err("APR: Rxed a packet for NULL callback\n");
}

int apr_get_svc(const char *svc_name, int domain_id, int *client_id,
		int *svc_idx, int *svc_id)
{
	int i;
	int size;
	struct apr_svc_table *tbl;
	int ret = 0;

	if ((domain_id == APR_DOMAIN_ADSP)) {
		tbl = (struct apr_svc_table *)&svc_tbl_qdsp6;
		size = ARRAY_SIZE(svc_tbl_qdsp6);
	} else {
		tbl = (struct apr_svc_table *)&svc_tbl_voice;
		size = ARRAY_SIZE(svc_tbl_voice);
	}

	for (i = 0; i < size; i++) {
		if (!strcmp(svc_name, tbl[i].name)) {
			*client_id = tbl[i].client_id;
			*svc_idx = tbl[i].idx;
			*svc_id = tbl[i].id;
			break;
		}
	}

	pr_debug("%s: svc_name = %s c_id = %d domain_id = %d\n",
		 __func__, svc_name, *client_id, domain_id);
	if (i == size) {
		pr_err("%s: APR: Wrong svc name %s\n", __func__, svc_name);
		ret = -EINVAL;
	}

	return ret;
}

static void apr_reset_deregister(struct work_struct *work)
{
	struct apr_svc *handle = NULL;
	struct apr_reset_work *apr_reset =
			container_of(work, struct apr_reset_work, work);

	handle = apr_reset->handle;
	pr_debug("%s:handle[%pK]\n", __func__, handle);
	apr_deregister(handle);
	kfree(apr_reset);
}

int apr_deregister(void *handle)
{
	struct apr_svc *svc = handle;
	struct apr_client *clnt;
	uint16_t dest_id;
	uint16_t client_id;

	if (!handle)
		return -EINVAL;

	mutex_lock(&svc->m_lock);
	dest_id = svc->dest_id;
	client_id = svc->client_id;
	clnt = &client[dest_id][client_id];

	if (svc->port_cnt > 0 || svc->svc_cnt > 0) {
		if (svc->port_cnt)
			svc->port_cnt--;
		else if (svc->svc_cnt)
			svc->svc_cnt--;
		if (!svc->port_cnt && !svc->svc_cnt) {
			client[dest_id][client_id].svc_cnt--;
			svc->need_reset = 0x0;
		}
	} else if (client[dest_id][client_id].svc_cnt > 0) {
		client[dest_id][client_id].svc_cnt--;
		if (!client[dest_id][client_id].svc_cnt) {
			svc->need_reset = 0x0;
			pr_debug("%s: service is reset %pK\n", __func__, svc);
		}
	}

	if (!svc->port_cnt && !svc->svc_cnt) {
		svc->priv = NULL;
		svc->id = 0;
		svc->fn = NULL;
		svc->dest_id = 0;
		svc->client_id = 0;
		svc->need_reset = 0x0;
	}
	if (client[dest_id][client_id].handle &&
	    !client[dest_id][client_id].svc_cnt) {
		apr_tal_close(client[dest_id][client_id].handle);
		client[dest_id][client_id].handle = NULL;
	}
	mutex_unlock(&svc->m_lock);

	return 0;
}

void apr_reset(void *handle)
{
	struct apr_reset_work *apr_reset_worker = NULL;

	if (!handle)
		return;
	pr_debug("%s: handle[%pK]\n", __func__, handle);

	if (apr_reset_workqueue == NULL) {
		pr_err("%s: apr_reset_workqueue is NULL\n", __func__);
		return;
	}

	apr_reset_worker = kzalloc(sizeof(struct apr_reset_work),
							GFP_ATOMIC);

	if (apr_reset_worker == NULL) {
		pr_err("%s: mem failure\n", __func__);
		return;
	}

	apr_reset_worker->handle = handle;
	INIT_WORK(&apr_reset_worker->work, apr_reset_deregister);
	queue_work(apr_reset_workqueue, &apr_reset_worker->work);
}

/* Dispatch the Reset events to Modem and audio clients */
void dispatch_event(unsigned long code, uint16_t proc)
{
	struct apr_client *apr_client;
	struct apr_client_data data;
	struct apr_svc *svc;
	uint16_t clnt;
	int i, j;

	memset(&data, 0, sizeof(data));
	data.opcode = RESET_EVENTS;
	data.reset_event = code;

	/* Service domain can be different from the processor */
	data.reset_proc = apr_get_reset_domain(proc);

	clnt = APR_CLIENT_AUDIO;
	apr_client = &client[proc][clnt];
	for (i = 0; i < APR_SVC_MAX; i++) {
		mutex_lock(&apr_client->svc[i].m_lock);
		if (apr_client->svc[i].fn) {
			apr_client->svc[i].need_reset = 0x1;
			apr_client->svc[i].fn(&data, apr_client->svc[i].priv);
		}
		if (apr_client->svc[i].port_cnt) {
			svc = &(apr_client->svc[i]);
			svc->need_reset = 0x1;
			for (j = 0; j < APR_MAX_PORTS; j++)
				if (svc->port_fn[j])
					svc->port_fn[j](&data,
						svc->port_priv[j]);
		}
		mutex_unlock(&apr_client->svc[i].m_lock);
	}

	clnt = APR_CLIENT_VOICE;
	apr_client = &client[proc][clnt];
	for (i = 0; i < APR_SVC_MAX; i++) {
		mutex_lock(&apr_client->svc[i].m_lock);
		if (apr_client->svc[i].fn) {
			apr_client->svc[i].need_reset = 0x1;
			apr_client->svc[i].fn(&data, apr_client->svc[i].priv);
		}
		if (apr_client->svc[i].port_cnt) {
			svc = &(apr_client->svc[i]);
			svc->need_reset = 0x1;
			for (j = 0; j < APR_MAX_PORTS; j++)
				if (svc->port_fn[j])
					svc->port_fn[j](&data,
						svc->port_priv[j]);
		}
		mutex_unlock(&apr_client->svc[i].m_lock);
	}
}

static int modem_notifier_cb(struct notifier_block *this, unsigned long code,
			     void *_cmd)
{
	static int boot_count = 2;

	if (boot_count) {
		boot_count--;
		return NOTIFY_OK;
	}

	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("M-Notify: Shutdown started\n");
		apr_set_modem_state(APR_SUBSYS_DOWN);
		dispatch_event(code, APR_DEST_MODEM);
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		pr_debug("M-Notify: Shutdown Completed\n");
		break;
	case SUBSYS_BEFORE_POWERUP:
		pr_debug("M-notify: Bootup started\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		if (apr_cmpxchg_modem_state(APR_SUBSYS_DOWN, APR_SUBSYS_UP) ==
						APR_SUBSYS_DOWN)
			wake_up(&modem_wait);
		is_modem_up = 1;
		pr_debug("M-Notify: Bootup Completed\n");
		break;
	default:
		pr_err("M-Notify: General: %lu\n", code);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block mnb = {
	.notifier_call = modem_notifier_cb,
};

static bool powered_on;

static int lpass_notifier_cb(struct notifier_block *this, unsigned long code,
			     void *_cmd)
{
	static int boot_count = 2;
	struct notif_data *data = (struct notif_data *)_cmd;
	struct scm_desc desc;

	if (boot_count) {
		boot_count--;
		return NOTIFY_OK;
	}

	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("L-Notify: Shutdown started\n");
		apr_set_q6_state(APR_SUBSYS_DOWN);
		dispatch_event(code, APR_DEST_QDSP6);
		if (data && data->crashed) {
			/* Send NMI to QDSP6 via an SCM call. */
			if (!is_scm_armv8()) {
				scm_call_atomic1(SCM_SVC_UTIL,
						 SCM_Q6_NMI_CMD, 0x1);
			} else {
				desc.args[0] = 0x1;
				desc.arginfo = SCM_ARGS(1);
				scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_UTIL,
						 SCM_Q6_NMI_CMD), &desc);
			}
			/* The write should go through before q6 is shutdown */
			mb();
			pr_debug("L-Notify: Q6 NMI was sent.\n");
		}
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		powered_on = false;
		pr_debug("L-Notify: Shutdown Completed\n");
		break;
	case SUBSYS_BEFORE_POWERUP:
		pr_debug("L-notify: Bootup started\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		if (apr_cmpxchg_q6_state(APR_SUBSYS_DOWN,
				APR_SUBSYS_LOADED) == APR_SUBSYS_DOWN)
			wake_up(&dsp_wait);
		powered_on = true;
		pr_debug("L-Notify: Bootup Completed\n");
		break;
	default:
		pr_err("L-Notify: Generel: %lu\n", code);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block lnb = {
	.priority = 0,
	.notifier_call = lpass_notifier_cb,
};

static int panic_handler(struct notifier_block *this,
				unsigned long event, void *ptr)
{
	struct scm_desc desc;

	if (powered_on) {
		/* Send NMI to QDSP6 via an SCM call. */
		if (!is_scm_armv8()) {
			scm_call_atomic1(SCM_SVC_UTIL, SCM_Q6_NMI_CMD, 0x1);
		} else {
			desc.args[0] = 0x1;
			desc.arginfo = SCM_ARGS(1);
			scm_call2_atomic(SCM_SIP_FNID(SCM_SVC_UTIL,
					 SCM_Q6_NMI_CMD), &desc);
		}
	}
	return NOTIFY_DONE;
}

static struct notifier_block panic_nb = {
	.notifier_call  = panic_handler,
};

static int __init apr_init(void)
{
	int i, j, k;

	for (i = 0; i < APR_DEST_MAX; i++)
		for (j = 0; j < APR_CLIENT_MAX; j++) {
			mutex_init(&client[i][j].m_lock);
			for (k = 0; k < APR_SVC_MAX; k++) {
				mutex_init(&client[i][j].svc[k].m_lock);
				spin_lock_init(&client[i][j].svc[k].w_lock);
			}
		}
	apr_set_subsys_state();
	mutex_init(&q6.lock);
	apr_reset_workqueue = create_singlethread_workqueue("apr_driver");
	if (!apr_reset_workqueue)
		return -ENOMEM;
	atomic_notifier_chain_register(&panic_notifier_list, &panic_nb);

	apr_pkt_ctx = ipc_log_context_create(APR_PKT_IPC_LOG_PAGE_CNT,
						"apr", 0);
	if (!apr_pkt_ctx)
		pr_err("%s: Unable to create ipc log context\n", __func__);

	return 0;
}
device_initcall(apr_init);

static int __init apr_late_init(void)
{
	int ret = 0;
	init_waitqueue_head(&dsp_wait);
	init_waitqueue_head(&modem_wait);
	subsys_notif_register(&mnb, &lnb);
	return ret;
}
late_initcall(apr_late_init);
