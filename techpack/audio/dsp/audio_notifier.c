/* Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/service-notifier.h>
#include <dsp/audio_notifier.h>
#include "audio_ssr.h"
#include "audio_pdr.h"

/* Audio states internal to notifier. Client */
/* used states defined in audio_notifier.h */
/* for AUDIO_NOTIFIER_SERVICE_DOWN & UP */
#define NO_SERVICE -2
#define UNINIT_SERVICE -1

/*
 * Used for each client registered with audio notifier
 */
struct client_data {
	struct list_head        list;
	/* Notifier block given by client */
	struct notifier_block   *nb;
	char                    client_name[20];
	int                     service;
	int                     domain;
};

/*
 * Used for each service and domain combination
 * Tracks information specific to the underlying
 * service.
 */
struct service_info {
	const char                      name[20];
	int                             domain_id;
	int                             state;
	void                            *handle;
	/* Notifier block registered to service */
	struct notifier_block           *nb;
	/* Used to determine when to register and deregister service */
	int                             num_of_clients;
	/* List of all clients registered to the service and domain */
	struct srcu_notifier_head       client_nb_list;
};

static int audio_notifer_ssr_adsp_cb(struct notifier_block *this,
				     unsigned long opcode, void *data);
static int audio_notifer_ssr_modem_cb(struct notifier_block *this,
				     unsigned long opcode, void *data);
static int audio_notifer_pdr_adsp_cb(struct notifier_block *this,
				     unsigned long opcode, void *data);

static struct notifier_block notifier_ssr_adsp_nb = {
	.notifier_call  = audio_notifer_ssr_adsp_cb,
	.priority = 0,
};

static struct notifier_block notifier_ssr_modem_nb = {
	.notifier_call  = audio_notifer_ssr_modem_cb,
	.priority = 0,
};

static struct notifier_block notifier_pdr_adsp_nb = {
	.notifier_call  = audio_notifer_pdr_adsp_cb,
	.priority = 0,
};

static struct service_info service_data[AUDIO_NOTIFIER_MAX_SERVICES]
				       [AUDIO_NOTIFIER_MAX_DOMAINS] = {

	{{
		.name = "SSR_ADSP",
		.domain_id = AUDIO_SSR_DOMAIN_ADSP,
		.state = AUDIO_NOTIFIER_SERVICE_DOWN,
		.nb = &notifier_ssr_adsp_nb
	},
	{
		.name = "SSR_MODEM",
		.domain_id = AUDIO_SSR_DOMAIN_MODEM,
		.state = AUDIO_NOTIFIER_SERVICE_DOWN,
		.nb = &notifier_ssr_modem_nb
	} },

	{{
		.name = "PDR_ADSP",
		.domain_id = AUDIO_PDR_DOMAIN_ADSP,
		.state = UNINIT_SERVICE,
		.nb = &notifier_pdr_adsp_nb
	},
	{	/* PDR MODEM service not enabled */
		.name = "INVALID",
		.state = NO_SERVICE,
		.nb = NULL
	} }
};

/* Master list of all audio notifier clients */
struct list_head   client_list;
struct mutex       notifier_mutex;

static int audio_notifer_get_default_service(int domain)
{
	int service = NO_SERVICE;

	/* initial service to connect per domain */
	switch (domain) {
	case AUDIO_NOTIFIER_ADSP_DOMAIN:
		service = AUDIO_NOTIFIER_PDR_SERVICE;
		break;
	case AUDIO_NOTIFIER_MODEM_DOMAIN:
		service = AUDIO_NOTIFIER_SSR_SERVICE;
		break;
	}

	return service;
}

static void audio_notifer_disable_service(int service)
{
	int i;

	for (i = 0; i < AUDIO_NOTIFIER_MAX_DOMAINS; i++)
		service_data[service][i].state = NO_SERVICE;
}

static bool audio_notifer_is_service_enabled(int service)
{
	int i;

	for (i = 0; i < AUDIO_NOTIFIER_MAX_DOMAINS; i++)
		if (service_data[service][i].state != NO_SERVICE)
			return true;
	return false;
}

static void audio_notifer_init_service(int service)
{
	int i;

	for (i = 0; i < AUDIO_NOTIFIER_MAX_DOMAINS; i++) {
		if (service_data[service][i].state == UNINIT_SERVICE)
			service_data[service][i].state =
				AUDIO_NOTIFIER_SERVICE_DOWN;
	}
}

static int audio_notifer_reg_service(int service, int domain)
{
	void *handle;
	int ret = 0;
	int curr_state = AUDIO_NOTIFIER_SERVICE_DOWN;

	switch (service) {
	case AUDIO_NOTIFIER_SSR_SERVICE:
		handle = audio_ssr_register(
			service_data[service][domain].domain_id,
			service_data[service][domain].nb);
		break;
	case AUDIO_NOTIFIER_PDR_SERVICE:
		handle = audio_pdr_service_register(
			service_data[service][domain].domain_id,
			service_data[service][domain].nb, &curr_state);

		if (curr_state == SERVREG_NOTIF_SERVICE_STATE_UP_V01)
			curr_state = AUDIO_NOTIFIER_SERVICE_UP;
		else
			curr_state = AUDIO_NOTIFIER_SERVICE_DOWN;
		break;
	default:
		pr_err("%s: Invalid service %d\n",
			__func__, service);
		ret = -EINVAL;
		goto done;
	}
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("%s: handle is incorrect for service %s\n",
			__func__, service_data[service][domain].name);
		ret = -EINVAL;
		goto done;
	}
	service_data[service][domain].state = curr_state;
	service_data[service][domain].handle = handle;

	pr_info("%s: service %s is in use\n",
		__func__, service_data[service][domain].name);
	pr_debug("%s: service %s has current state %d, handle 0x%pK\n",
		__func__, service_data[service][domain].name,
		service_data[service][domain].state,
		service_data[service][domain].handle);
done:
	return ret;
}

static int audio_notifer_dereg_service(int service, int domain)
{
	int ret;

	switch (service) {
	case AUDIO_NOTIFIER_SSR_SERVICE:
		ret = audio_ssr_deregister(
			service_data[service][domain].handle,
			service_data[service][domain].nb);
		break;
	case AUDIO_NOTIFIER_PDR_SERVICE:
		ret = audio_pdr_service_deregister(
			service_data[service][domain].handle,
			service_data[service][domain].nb);
		break;
	default:
		pr_err("%s: Invalid service %d\n",
			__func__, service);
		ret = -EINVAL;
		goto done;
	}
	if (ret < 0) {
		pr_err("%s: deregister failed for service %s, ret %d\n",
			__func__, service_data[service][domain].name, ret);
		goto done;
	}

	pr_debug("%s: service %s with handle 0x%pK deregistered\n",
		__func__, service_data[service][domain].name,
		service_data[service][domain].handle);

	service_data[service][domain].state = AUDIO_NOTIFIER_SERVICE_DOWN;
	service_data[service][domain].handle = NULL;
done:
	return ret;
}

static int audio_notifer_reg_client_service(struct client_data *client_data,
					    int service)
{
	int ret = 0;
	int domain = client_data->domain;
	struct audio_notifier_cb_data data;

	switch (service) {
	case AUDIO_NOTIFIER_SSR_SERVICE:
	case AUDIO_NOTIFIER_PDR_SERVICE:
		if (service_data[service][domain].num_of_clients == 0)
			ret = audio_notifer_reg_service(service, domain);
		break;
	default:
		pr_err("%s: Invalid service for client %s, service %d, domain %d\n",
			__func__, client_data->client_name, service, domain);
		ret = -EINVAL;
		goto done;
	}

	if (ret < 0) {
		pr_err("%s: service registration failed on service %s for client %s\n",
			__func__, service_data[service][domain].name,
			client_data->client_name);
		goto done;
	}

	client_data->service = service;
	srcu_notifier_chain_register(
		&service_data[service][domain].client_nb_list,
		client_data->nb);
	service_data[service][domain].num_of_clients++;

	pr_debug("%s: registered client %s on service %s, current state 0x%x\n",
		__func__, client_data->client_name,
		service_data[service][domain].name,
		service_data[service][domain].state);

	/*
	 * PDR registration returns current state
	 * Force callback of client with current state for PDR
	 */
	if (client_data->service == AUDIO_NOTIFIER_PDR_SERVICE) {
		data.service = service;
		data.domain = domain;
		(void)client_data->nb->notifier_call(client_data->nb,
			service_data[service][domain].state, &data);
	}
done:
	return ret;
}

static int audio_notifer_reg_client(struct client_data *client_data)
{
	int ret = 0;
	int service;
	int domain = client_data->domain;

	service = audio_notifer_get_default_service(domain);
	if (service < 0) {
		pr_err("%s: service %d is incorrect\n", __func__, service);
		ret = -EINVAL;
		goto done;
	}

	/* Search through services to find a valid one to register client on. */
	for (; service >= 0; service--) {
		/* If a service is not initialized, wait for it to come up. */
		if (service_data[service][domain].state == UNINIT_SERVICE)
			goto done;
		/* Skip unsupported service and domain combinations. */
		if (service_data[service][domain].state < 0)
			continue;
		/* Only register clients who have not acquired a service. */
		if (client_data->service != NO_SERVICE)
			continue;

		/*
		 * Only register clients, who have not acquired a service, on
		 * the best available service for their domain. Uninitialized
		 * services will try to register all of their clients after
		 * they initialize correctly or will disable their service and
		 * register clients on the next best avaialable service.
		 */
		pr_debug("%s: register client %s on service %s",
				__func__, client_data->client_name,
				service_data[service][domain].name);

		ret = audio_notifer_reg_client_service(client_data, service);
		if (ret < 0)
			pr_err("%s: client %s failed to register on service %s",
				__func__, client_data->client_name,
				service_data[service][domain].name);
	}

done:
	return ret;
}

static int audio_notifer_dereg_client(struct client_data *client_data)
{
	int ret = 0;
	int service = client_data->service;
	int domain = client_data->domain;

	switch (client_data->service) {
	case AUDIO_NOTIFIER_SSR_SERVICE:
	case AUDIO_NOTIFIER_PDR_SERVICE:
		if (service_data[service][domain].num_of_clients == 1)
			ret = audio_notifer_dereg_service(service, domain);
		break;
	case NO_SERVICE:
		goto done;
	default:
		pr_err("%s: Invalid service for client %s, service %d\n",
			__func__, client_data->client_name,
			client_data->service);
		ret = -EINVAL;
		goto done;
	}

	if (ret < 0) {
		pr_err("%s: deregister failed for client %s on service %s, ret %d\n",
			__func__, client_data->client_name,
			service_data[service][domain].name, ret);
		goto done;
	}

	ret = srcu_notifier_chain_unregister(&service_data[service][domain].
					     client_nb_list, client_data->nb);
	if (ret < 0) {
		pr_err("%s: srcu_notifier_chain_unregister failed, ret %d\n",
			__func__, ret);
		goto done;
	}

	pr_debug("%s: deregistered client %s on service %s\n",
		__func__, client_data->client_name,
		service_data[service][domain].name);

	client_data->service = NO_SERVICE;
	if (service_data[service][domain].num_of_clients > 0)
		service_data[service][domain].num_of_clients--;
done:
	return ret;
}

static void audio_notifer_reg_all_clients(void)
{
	struct list_head *ptr, *next;
	struct client_data *client_data;
	int ret;

	list_for_each_safe(ptr, next, &client_list) {
		client_data = list_entry(ptr, struct client_data, list);

		ret = audio_notifer_reg_client(client_data);
		if (ret < 0)
			pr_err("%s: audio_notifer_reg_client failed for client %s, ret %d\n",
				__func__, client_data->client_name,
				ret);
	}
}

static int audio_notifer_pdr_callback(struct notifier_block *this,
				      unsigned long opcode, void *data)
{
	pr_debug("%s: Audio PDR framework state 0x%lx\n",
		__func__, opcode);
	mutex_lock(&notifier_mutex);
	if (opcode == AUDIO_PDR_FRAMEWORK_DOWN)
		audio_notifer_disable_service(AUDIO_NOTIFIER_PDR_SERVICE);
	else
		audio_notifer_init_service(AUDIO_NOTIFIER_PDR_SERVICE);

	audio_notifer_reg_all_clients();
	mutex_unlock(&notifier_mutex);
	return 0;
}

static struct notifier_block pdr_nb = {
	.notifier_call  = audio_notifer_pdr_callback,
	.priority = 0,
};

static int audio_notifer_convert_opcode(unsigned long opcode,
					unsigned long *notifier_opcode)
{
	int ret = 0;

	switch (opcode) {
	case SUBSYS_BEFORE_SHUTDOWN:
	case SERVREG_NOTIF_SERVICE_STATE_DOWN_V01:
		*notifier_opcode = AUDIO_NOTIFIER_SERVICE_DOWN;
		break;
	case SUBSYS_AFTER_POWERUP:
	case SERVREG_NOTIF_SERVICE_STATE_UP_V01:
		*notifier_opcode = AUDIO_NOTIFIER_SERVICE_UP;
		break;
	default:
		pr_debug("%s: Unused opcode 0x%lx\n", __func__, opcode);
		ret = -EINVAL;
	}

	return ret;
}

static int audio_notifer_service_cb(unsigned long opcode,
				    int service, int domain)
{
	int ret = 0;
	unsigned long notifier_opcode;
	struct audio_notifier_cb_data data;

	if (audio_notifer_convert_opcode(opcode, &notifier_opcode) < 0)
		goto done;

	data.service = service;
	data.domain = domain;

	pr_debug("%s: service %s, opcode 0x%lx\n",
		__func__, service_data[service][domain].name, notifier_opcode);

	mutex_lock(&notifier_mutex);

	service_data[service][domain].state = notifier_opcode;
	ret = srcu_notifier_call_chain(&service_data[service][domain].
		client_nb_list, notifier_opcode, &data);
	if (ret < 0)
		pr_err("%s: srcu_notifier_call_chain returned %d, service %s, opcode 0x%lx\n",
			__func__, ret, service_data[service][domain].name,
			notifier_opcode);

	mutex_unlock(&notifier_mutex);
done:
	return NOTIFY_OK;
}

static int audio_notifer_pdr_adsp_cb(struct notifier_block *this,
				     unsigned long opcode, void *data)
{
	return audio_notifer_service_cb(opcode,
					AUDIO_NOTIFIER_PDR_SERVICE,
					AUDIO_NOTIFIER_ADSP_DOMAIN);
}

static int audio_notifer_ssr_adsp_cb(struct notifier_block *this,
				     unsigned long opcode, void *data)
{
	if (opcode == SUBSYS_BEFORE_SHUTDOWN)
		audio_ssr_send_nmi(data);

	return audio_notifer_service_cb(opcode,
					AUDIO_NOTIFIER_SSR_SERVICE,
					AUDIO_NOTIFIER_ADSP_DOMAIN);
}

static int audio_notifer_ssr_modem_cb(struct notifier_block *this,
				      unsigned long opcode, void *data)
{
	return audio_notifer_service_cb(opcode,
					AUDIO_NOTIFIER_SSR_SERVICE,
					AUDIO_NOTIFIER_MODEM_DOMAIN);
}

int audio_notifier_deregister(char *client_name)
{
	int ret = 0;
	int ret2;
	struct list_head *ptr, *next;
	struct client_data *client_data = NULL;

	if (client_name == NULL) {
		pr_err("%s: client_name is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}
	mutex_lock(&notifier_mutex);
	list_for_each_safe(ptr, next, &client_list) {
		client_data = list_entry(ptr, struct client_data, list);
		if (!strcmp(client_name, client_data->client_name)) {
			ret2 = audio_notifer_dereg_client(client_data);
			if (ret2 < 0) {
				pr_err("%s: audio_notifer_dereg_client failed, ret %d\n, service %d, domain %d",
					__func__, ret2, client_data->service,
					client_data->domain);
				ret = ret2;
				continue;
			}
			list_del(&client_data->list);
			kfree(client_data);
		}
	}
	mutex_unlock(&notifier_mutex);
done:
	return ret;
}
EXPORT_SYMBOL(audio_notifier_deregister);

int audio_notifier_register(char *client_name, int domain,
			    struct notifier_block *nb)
{
	int ret;
	struct client_data *client_data;

	if (client_name == NULL) {
		pr_err("%s: client_name is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	} else if (nb == NULL) {
		pr_err("%s: Notifier block is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	client_data = kmalloc(sizeof(*client_data), GFP_KERNEL);
	if (client_data == NULL) {
		ret = -ENOMEM;
		goto done;
	}
	INIT_LIST_HEAD(&client_data->list);
	client_data->nb = nb;
	strlcpy(client_data->client_name, client_name,
		sizeof(client_data->client_name));
	client_data->service = NO_SERVICE;
	client_data->domain = domain;

	mutex_lock(&notifier_mutex);
	ret = audio_notifer_reg_client(client_data);
	if (ret < 0) {
		mutex_unlock(&notifier_mutex);
		pr_err("%s: audio_notifer_reg_client for client %s failed ret = %d\n",
			__func__, client_data->client_name,
			ret);
		kfree(client_data);
		goto done;
	}
	list_add_tail(&client_data->list, &client_list);
	mutex_unlock(&notifier_mutex);
done:
	return ret;
}
EXPORT_SYMBOL(audio_notifier_register);

static int __init audio_notifier_subsys_init(void)
{
	int i, j;

	mutex_init(&notifier_mutex);
	INIT_LIST_HEAD(&client_list);
	for (i = 0; i < AUDIO_NOTIFIER_MAX_SERVICES; i++) {
		for (j = 0; j < AUDIO_NOTIFIER_MAX_DOMAINS; j++) {
			if (service_data[i][j].state <= NO_SERVICE)
				continue;

			srcu_init_notifier_head(
				&service_data[i][j].client_nb_list);
		}
	}

	return 0;
}
subsys_initcall(audio_notifier_subsys_init);

static int __init audio_notifier_init(void)
{
	int ret;

	ret = audio_pdr_register(&pdr_nb);
	if (ret < 0) {
		pr_debug("%s: PDR register failed, ret = %d, disable service\n",
			__func__, ret);
		audio_notifer_disable_service(AUDIO_NOTIFIER_PDR_SERVICE);
	}

	/* Do not return error since PDR enablement is not critical */
	return 0;
}
module_init(audio_notifier_init);

static int __init audio_notifier_late_init(void)
{
	/*
	 * If pdr registration failed, register clients on next service
	 * Do in late init to ensure that SSR subsystem is initialized
	 */
	mutex_lock(&notifier_mutex);
	if (!audio_notifer_is_service_enabled(AUDIO_NOTIFIER_PDR_SERVICE))
		audio_notifer_reg_all_clients();

	mutex_unlock(&notifier_mutex);
	return 0;
}
late_initcall(audio_notifier_late_init);
