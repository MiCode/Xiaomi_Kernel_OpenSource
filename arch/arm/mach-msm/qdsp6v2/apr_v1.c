/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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
#include <linux/err.h>
#include <mach/qdsp6v2/apr.h>
#include <mach/qdsp6v2/apr_tal.h>
#include <mach/qdsp6v2/dsp_debug.h>

static const char *lpass_subsys_name = "lpass";

struct apr_svc *apr_register(char *dest, char *svc_name, apr_fn svc_fn,
			     uint32_t src_port, void *priv)
{
	struct apr_client *client;
	int client_id = 0;
	int svc_idx = 0;
	int svc_id = 0;
	int dest_id = 0;
	int temp_port = 0;
	struct apr_svc *svc = NULL;
	int rc = 0;

	if (!dest || !svc_name || !svc_fn)
		return NULL;

	if (!strncmp(dest, "ADSP", 4))
		dest_id = APR_DEST_QDSP6;
	else if (!strncmp(dest, "MODEM", 5)) {
		dest_id = APR_DEST_MODEM;
	} else {
		pr_err("APR: wrong destination\n");
		goto done;
	}

	if (dest_id == APR_DEST_QDSP6 &&
	    apr_get_q6_state() == APR_SUBSYS_DOWN) {
		pr_info("%s: Wait for Lpass to bootup\n", __func__);
		rc = apr_wait_for_device_up(dest_id);
		if (rc == 0) {
			pr_err("%s: DSP is not Up\n", __func__);
			return NULL;
		}
		pr_info("%s: Lpass Up\n", __func__);
	} else if (dest_id == APR_DEST_MODEM &&
		   (apr_get_modem_state() == APR_SUBSYS_DOWN)) {
		pr_info("%s: Wait for modem to bootup\n", __func__);
		rc = apr_wait_for_device_up(dest_id);
		if (rc == 0) {
			pr_err("%s: Modem is not Up\n", __func__);
			return NULL;
		}
		pr_info("%s: modem Up\n", __func__);
	}

	if (apr_get_svc(svc_name, dest_id, &client_id, &svc_idx, &svc_id)) {
		pr_err("%s: apr_get_svc failed\n", __func__);
		goto done;
	}

	/* APRv1 loads ADSP image automatically */
	apr_load_adsp_image();

	client = apr_get_client(dest_id, client_id);
	mutex_lock(&client->m_lock);
	if (!client->handle) {
		client->handle = apr_tal_open(client_id, dest_id, APR_DL_SMD,
					      apr_cb_func, NULL);
		if (!client->handle) {
			svc = NULL;
			pr_err("APR: Unable to open handle\n");
			mutex_unlock(&client->m_lock);
			goto done;
		}
	}
	mutex_unlock(&client->m_lock);
	svc = &client->svc[svc_idx];
	mutex_lock(&svc->m_lock);
	client->id = client_id;
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
		if (temp_port >= APR_MAX_PORTS || temp_port < 0) {
			pr_err("APR: temp_port out of bounds\n");
			mutex_unlock(&svc->m_lock);
			return NULL;
		}
		if (!svc->port_cnt && !svc->svc_cnt)
			client->svc_cnt++;
		svc->port_cnt++;
		svc->port_fn[temp_port] = svc_fn;
		svc->port_priv[temp_port] = priv;
	} else {
		if (!svc->fn) {
			if (!svc->port_cnt && !svc->svc_cnt)
				client->svc_cnt++;
			svc->fn = svc_fn;
			if (svc->port_cnt)
				svc->svc_cnt++;
		}
	}

	mutex_unlock(&svc->m_lock);
done:
	return svc;
}

void apr_set_subsys_state(void)
{
	apr_set_q6_state(APR_SUBSYS_UP);
	apr_set_modem_state(APR_SUBSYS_UP);
}

const char *apr_get_lpass_subsys_name(void)
{
	return lpass_subsys_name;
}
