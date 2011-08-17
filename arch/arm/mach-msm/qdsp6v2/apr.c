/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
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
#include <linux/mutex.h>
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
#include <mach/peripheral-loader.h>
#include <mach/msm_smd.h>
#include <mach/qdsp6v2/apr.h>
#include <mach/qdsp6v2/apr_tal.h>
#include <mach/qdsp6v2/dsp_debug.h>
#include <mach/subsystem_notif.h>
#include <mach/subsystem_restart.h>

struct apr_q6 q6;
struct apr_client client[APR_DEST_MAX][APR_CLIENT_MAX];
static atomic_t dsp_state;
static atomic_t modem_state;

static wait_queue_head_t  dsp_wait;
static wait_queue_head_t  modem_wait;
/* Subsystem restart: QDSP6 data, functions */
static struct workqueue_struct *apr_reset_workqueue;
static void apr_reset_deregister(struct work_struct *work);
struct apr_reset_work {
	void *handle;
	struct work_struct work;
};


int apr_send_pkt(void *handle, uint32_t *buf)
{
	struct apr_svc *svc = handle;
	struct apr_client *clnt;
	struct apr_hdr *hdr;
	uint16_t dest_id;
	uint16_t client_id;
	uint16_t w_len;
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
					(atomic_read(&dsp_state) == 0)) {
		pr_err("apr: Still dsp is not Up\n");
		return -ENETRESET;
	} else if ((svc->dest_id == APR_DEST_MODEM) &&
					(atomic_read(&modem_state) == 0)) {
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
	if (dest_id == APR_DEST_MODEM)
		hdr->dest_domain = APR_DOMAIN_MODEM;
	else if (dest_id == APR_DEST_QDSP6)
		hdr->dest_domain = APR_DOMAIN_ADSP;

	hdr->dest_svc = svc->id;

	w_len = apr_tal_write(clnt->handle, buf, hdr->pkt_size);
	if (w_len != hdr->pkt_size)
		pr_err("Unable to write APR pkt successfully: %d\n", w_len);
	spin_unlock_irqrestore(&svc->w_lock, flags);

	return w_len;
}

static void apr_cb_func(void *buf, int len, void *priv)
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
		pr_err("APR: Improper apr pkt received:%p %d\n",
								buf, len);
		return;
	}
	hdr = buf;

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
	if (msg_type >= APR_MSG_TYPE_MAX &&
			msg_type != APR_BASIC_RSP_RESULT) {
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
		src = APR_DEST_MODEM;
		if (svc == APR_SVC_MVS || svc == APR_SVC_MVM ||
			svc == APR_SVC_CVS || svc == APR_SVC_CVP ||
			svc == APR_SVC_TEST_CLIENT)
			clnt = APR_CLIENT_VOICE;
		else {
			pr_err("APR: Wrong svc :%d\n", svc);
			return;
		}
	} else if (hdr->src_domain == APR_DOMAIN_ADSP) {
		src = APR_DEST_QDSP6;
		if (svc == APR_SVC_AFE || svc == APR_SVC_ASM ||
			svc == APR_SVC_VSM || svc == APR_SVC_VPM ||
			svc == APR_SVC_ADM || svc == APR_SVC_ADSP_CORE ||
			svc == APR_SVC_TEST_CLIENT || svc == APR_SVC_ADSP_MVM ||
			svc == APR_SVC_ADSP_CVS || svc == APR_SVC_ADSP_CVP)
			clnt = APR_CLIENT_AUDIO;
		else {
			pr_err("APR: Wrong svc :%d\n", svc);
			return;
		}
	} else {
		pr_err("APR: Pkt from wrong source: %d\n", hdr->src_domain);
		return;
	}

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
	pr_debug("%x %x %x %p %p\n", c_svc->id, c_svc->dest_id,
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

	temp_port = ((data.src_port >> 8) * 8) + (data.src_port & 0xFF);
	pr_debug("port = %d t_port = %d\n", data.src_port, temp_port);
	if (c_svc->port_cnt && c_svc->port_fn[temp_port])
		c_svc->port_fn[temp_port](&data,  c_svc->port_priv[temp_port]);
	else if (c_svc->fn)
		c_svc->fn(&data, c_svc->priv);
	else
		pr_err("APR: Rxed a packet for NULL callback\n");
}

struct apr_svc *apr_register(char *dest, char *svc_name, apr_fn svc_fn,
					uint32_t src_port, void *priv)
{
	int client_id = 0;
	int svc_idx = 0;
	int svc_id = 0;
	int dest_id = 0;
	int temp_port = 0;
	struct apr_svc *svc = NULL;
	int rc = 0;

	if (!dest || !svc_name || !svc_fn)
		return NULL;

	if (!strcmp(dest, "ADSP"))
		dest_id = APR_DEST_QDSP6;
	else if (!strcmp(dest, "MODEM")) {
		dest_id = APR_DEST_MODEM;
	} else {
		pr_err("APR: wrong destination\n");
		goto done;
	}

	if ((dest_id == APR_DEST_QDSP6) &&
				(atomic_read(&dsp_state) == 0)) {
		rc = wait_event_timeout(dsp_wait,
				(atomic_read(&dsp_state) == 1), 5*HZ);
		if (rc == 0) {
			pr_err("apr: Still dsp is not Up\n");
			return NULL;
		}
	} else if ((dest_id == APR_DEST_MODEM) &&
					(atomic_read(&modem_state) == 0)) {
		rc = wait_event_timeout(modem_wait,
			(atomic_read(&modem_state) == 1), 5*HZ);
		if (rc == 0) {
			pr_err("apr: Still Modem is not Up\n");
			return NULL;
		}
	}

	if (!strcmp(svc_name, "AFE")) {
		client_id = APR_CLIENT_AUDIO;
		svc_idx = 0;
		svc_id = APR_SVC_AFE;
	} else if (!strcmp(svc_name, "ASM")) {
		client_id = APR_CLIENT_AUDIO;
		svc_idx = 1;
		svc_id = APR_SVC_ASM;
	} else if (!strcmp(svc_name, "ADM")) {
		client_id = APR_CLIENT_AUDIO;
		svc_idx = 2;
		svc_id = APR_SVC_ADM;
	} else if (!strcmp(svc_name, "CORE")) {
		client_id = APR_CLIENT_AUDIO;
		svc_idx = 3;
		svc_id = APR_SVC_ADSP_CORE;
	} else if (!strcmp(svc_name, "TEST")) {
		if (dest_id == APR_DEST_QDSP6) {
			client_id = APR_CLIENT_AUDIO;
			svc_idx = 4;
		} else {
			client_id = APR_CLIENT_VOICE;
			svc_idx = 7;
		}
		svc_id = APR_SVC_TEST_CLIENT;
	} else if (!strcmp(svc_name, "VSM")) {
		client_id = APR_CLIENT_VOICE;
		svc_idx = 0;
		svc_id = APR_SVC_VSM;
	} else if (!strcmp(svc_name, "VPM")) {
		client_id = APR_CLIENT_VOICE;
		svc_idx = 1;
		svc_id = APR_SVC_VPM;
	} else if (!strcmp(svc_name, "MVS")) {
		client_id = APR_CLIENT_VOICE;
		svc_idx = 2;
		svc_id = APR_SVC_MVS;
	} else if (!strcmp(svc_name, "MVM")) {
		if (dest_id == APR_DEST_MODEM) {
			client_id = APR_CLIENT_VOICE;
			svc_idx = 3;
			svc_id = APR_SVC_MVM;
		} else {
			client_id = APR_CLIENT_AUDIO;
			svc_idx = 5;
			svc_id = APR_SVC_ADSP_MVM;
		}
	} else if (!strcmp(svc_name, "CVS")) {
		if (dest_id == APR_DEST_MODEM) {
			client_id = APR_CLIENT_VOICE;
			svc_idx = 4;
			svc_id = APR_SVC_CVS;
		} else {
			client_id = APR_CLIENT_AUDIO;
			svc_idx = 6;
			svc_id = APR_SVC_ADSP_CVS;
		}
	} else if (!strcmp(svc_name, "CVP")) {
		if (dest_id == APR_DEST_MODEM) {
			client_id = APR_CLIENT_VOICE;
			svc_idx = 5;
			svc_id = APR_SVC_CVP;
		} else {
			client_id = APR_CLIENT_AUDIO;
			svc_idx = 7;
			svc_id = APR_SVC_ADSP_CVP;
		}
	} else if (!strcmp(svc_name, "SRD")) {
		client_id = APR_CLIENT_VOICE;
		svc_idx = 6;
		svc_id = APR_SVC_SRD;
	} else {
		pr_err("APR: Wrong svc name\n");
		goto done;
	}

	pr_debug("svc name = %s c_id = %d dest_id = %d\n",
				svc_name, client_id, dest_id);
	mutex_lock(&q6.lock);
	if (q6.state == APR_Q6_NOIMG) {
		q6.pil = pil_get("q6");
		if (!q6.pil) {
			pr_err("APR: Unable to load q6 image\n");
			mutex_unlock(&q6.lock);
			return svc;
		}
		q6.state = APR_Q6_LOADED;
	}
	mutex_unlock(&q6.lock);
	mutex_lock(&client[dest_id][client_id].m_lock);
	if (!client[dest_id][client_id].handle) {
		client[dest_id][client_id].handle = apr_tal_open(client_id,
				dest_id, APR_DL_SMD, apr_cb_func, NULL);
		if (!client[dest_id][client_id].handle) {
			svc = NULL;
			pr_err("APR: Unable to open handle\n");
			mutex_unlock(&client[dest_id][client_id].m_lock);
			goto done;
		}
	}
	mutex_unlock(&client[dest_id][client_id].m_lock);
	svc = &client[dest_id][client_id].svc[svc_idx];
	mutex_lock(&svc->m_lock);
	client[dest_id][client_id].id = client_id;
	if (svc->need_reset) {
		mutex_unlock(&svc->m_lock);
		pr_err("APR: Service needs reset\n");
		goto done;
	}
	svc->priv = priv;
	svc->id = svc_id;
	svc->dest_id = dest_id;
	svc->client_id = client_id;
	if (src_port != 0xFFFFFFFF) {
		temp_port = ((src_port >> 8) * 8) + (src_port & 0xFF);
		pr_debug("port = %d t_port = %d\n", src_port, temp_port);
		if (temp_port > APR_MAX_PORTS || temp_port < 0) {
			pr_err("APR: temp_port out of bounds\n");
			mutex_unlock(&svc->m_lock);
			return NULL;
		}
		if (!svc->port_cnt && !svc->svc_cnt)
			client[dest_id][client_id].svc_cnt++;
		svc->port_cnt++;
		svc->port_fn[temp_port] = svc_fn;
		svc->port_priv[temp_port] = priv;
	} else {
		if (!svc->fn) {
			if (!svc->port_cnt && !svc->svc_cnt)
				client[dest_id][client_id].svc_cnt++;
			svc->fn = svc_fn;
			if (svc->port_cnt)
				svc->svc_cnt++;
		}
	}

	mutex_unlock(&svc->m_lock);
done:
	return svc;
}

static void apr_reset_deregister(struct work_struct *work)
{
	struct apr_svc *handle = NULL;
	struct apr_reset_work *apr_reset =
			container_of(work, struct apr_reset_work, work);

	handle = apr_reset->handle;
	pr_debug("%s:handle[%p]\n", __func__, handle);
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
			pr_debug("%s: service is reset %p\n", __func__, svc);
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
	pr_debug("%s: handle[%p]\n", __func__, handle);

	apr_reset_worker = kzalloc(sizeof(struct apr_reset_work),
					GFP_ATOMIC);
	if (apr_reset_worker == NULL || apr_reset_workqueue == NULL) {
		pr_err("%s: mem failure\n", __func__);
		return;
	}
	apr_reset_worker->handle = handle;
	INIT_WORK(&apr_reset_worker->work, apr_reset_deregister);
	queue_work(apr_reset_workqueue, &apr_reset_worker->work);
}

void change_q6_state(int state)
{
	mutex_lock(&q6.lock);
	q6.state = state;
	mutex_unlock(&q6.lock);
}

int adsp_state(int state)
{
	pr_info("dsp state = %d\n", state);
	return 0;
}

/* Dispatch the Reset events to Modem and audio clients */
void dispatch_event(unsigned long code, unsigned short proc)
{
	struct apr_client *apr_client;
	struct apr_client_data data;
	struct apr_svc *svc;
	uint16_t clnt;
	int i, j;

	data.opcode = RESET_EVENTS;
	data.reset_event = code;
	data.reset_proc = proc;

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
	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("M-Notify: Shutdown started\n");
		atomic_set(&modem_state, 0);
		dispatch_event(code, APR_DEST_MODEM);
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		pr_debug("M-Notify: Shutdown Completed\n");
		break;
	case SUBSYS_BEFORE_POWERUP:
		pr_debug("M-notify: Bootup started\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		if (atomic_read(&modem_state) == 0) {
			atomic_set(&modem_state, 1);
			wake_up(&modem_wait);
		}
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

static int lpass_notifier_cb(struct notifier_block *this, unsigned long code,
								void *_cmd)
{
	switch (code) {
	case SUBSYS_BEFORE_SHUTDOWN:
		pr_debug("L-Notify: Shutdown started\n");
		atomic_set(&dsp_state, 0);
		dispatch_event(code, APR_DEST_QDSP6);
		break;
	case SUBSYS_AFTER_SHUTDOWN:
		pr_debug("L-Notify: Shutdown Completed\n");
		break;
	case SUBSYS_BEFORE_POWERUP:
		pr_debug("L-notify: Bootup started\n");
		break;
	case SUBSYS_AFTER_POWERUP:
		if (atomic_read(&dsp_state) == 0) {
			atomic_set(&dsp_state, 1);
			wake_up(&dsp_wait);
		}
		pr_debug("L-Notify: Bootup Completed\n");
		break;
	default:
		pr_err("L-Notify: Generel: %lu\n", code);
		break;
	}
	return NOTIFY_DONE;
}

static struct notifier_block lnb = {
	.notifier_call = lpass_notifier_cb,
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
	mutex_init(&q6.lock);
	dsp_debug_register(adsp_state);
	apr_reset_workqueue =
		create_singlethread_workqueue("apr_driver");
	if (!apr_reset_workqueue)
		return -ENOMEM;
	return 0;
}
device_initcall(apr_init);

static int __init apr_late_init(void)
{
	void *ret;
	init_waitqueue_head(&dsp_wait);
	init_waitqueue_head(&modem_wait);
	atomic_set(&dsp_state, 1);
	atomic_set(&modem_state, 1);
	ret = subsys_notif_register_notifier("modem", &mnb);
	pr_debug("subsys_register_notifier: ret1 = %p\n", ret);
	ret = subsys_notif_register_notifier("lpass", &lnb);
	pr_debug("subsys_register_notifier: ret2 = %p\n", ret);

	return 0;
}
late_initcall(apr_late_init);
