/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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
#include <soc/qcom/subsystem_notif.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/service-notifier.h>
#include "core.h"
#include "qmi.h"
#include "snoc.h"
#include <soc/qcom/icnss.h>

static int
ath10k_snoc_service_notifier_notify(struct notifier_block *nb,
				    unsigned long notification, void *data)
{
	struct ath10k_snoc *ar_snoc = container_of(nb, struct ath10k_snoc,
					       service_notifier_nb);
	enum pd_subsys_state *state = data;
	struct ath10k *ar = ar_snoc->ar;

	switch (notification) {
	case SERVREG_NOTIF_SERVICE_STATE_DOWN_V01:
		ath10k_dbg(ar, ATH10K_DBG_SNOC, "Service down, data: 0x%pK\n",
			   data);

		if (!state || *state != ROOT_PD_SHUTDOWN)
			atomic_set(&ar_snoc->fw_crashed, 1);

		ath10k_dbg(ar, ATH10K_DBG_SNOC, "PD went down %d\n",
			   atomic_read(&ar_snoc->fw_crashed));
		break;
	case SERVREG_NOTIF_SERVICE_STATE_UP_V01:
		ath10k_dbg(ar, ATH10K_DBG_SNOC, "Service up\n");
		queue_work(ar->workqueue, &ar->restart_work);
		break;
	default:
		ath10k_dbg(ar, ATH10K_DBG_SNOC,
			   "Service state Unknown, notification: 0x%lx\n",
			    notification);
		return NOTIFY_DONE;
	}
	return NOTIFY_OK;
}

static int ath10k_snoc_get_service_location_notify(struct notifier_block *nb,
						   unsigned long opcode,
						   void *data)
{
	struct ath10k_snoc *ar_snoc = container_of(nb, struct ath10k_snoc,
						   get_service_nb);
	struct ath10k *ar = ar_snoc->ar;
	struct pd_qmi_client_data *pd = data;
	int curr_state;
	int ret;
	int i;
	struct ath10k_service_notifier_context *notifier;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Get service notify opcode: %lu\n",
		   opcode);

	if (opcode != LOCATOR_UP)
		return NOTIFY_DONE;

	if (!pd->total_domains) {
		ath10k_err(ar, "Did not find any domains\n");
		ret = -ENOENT;
		goto out;
	}

	notifier = kcalloc(pd->total_domains,
			   sizeof(struct ath10k_service_notifier_context),
			   GFP_KERNEL);
	if (!notifier) {
		ret = -ENOMEM;
		goto out;
	}

	ar_snoc->service_notifier_nb.notifier_call =
					ath10k_snoc_service_notifier_notify;

	for (i = 0; i < pd->total_domains; i++) {
		ath10k_dbg(ar, ATH10K_DBG_SNOC,
			   "%d: domain_name: %s, instance_id: %d\n", i,
				   pd->domain_list[i].name,
				   pd->domain_list[i].instance_id);

		notifier[i].handle =
			service_notif_register_notifier(
					pd->domain_list[i].name,
					pd->domain_list[i].instance_id,
					&ar_snoc->service_notifier_nb,
					&curr_state);
		notifier[i].instance_id = pd->domain_list[i].instance_id;
		strlcpy(notifier[i].name, pd->domain_list[i].name,
			QMI_SERVREG_LOC_NAME_LENGTH_V01 + 1);

		if (IS_ERR(notifier[i].handle)) {
			ath10k_err(ar, "%d: Unable to register notifier for %s(0x%x)\n",
				   i, pd->domain_list->name,
				   pd->domain_list->instance_id);
			ret = PTR_ERR(notifier[i].handle);
			goto free_handle;
		}
	}

	ar_snoc->service_notifier = notifier;
	ar_snoc->total_domains = pd->total_domains;

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "PD restart enabled\n");

	return NOTIFY_OK;

free_handle:
	for (i = 0; i < pd->total_domains; i++) {
		if (notifier[i].handle) {
			service_notif_unregister_notifier(
						notifier[i].handle,
						&ar_snoc->service_notifier_nb);
		}
	}
	kfree(notifier);

out:
	ath10k_err(ar, "PD restart not enabled: %d\n", ret);

	return NOTIFY_OK;
}

int ath10k_snoc_pd_restart_enable(struct ath10k *ar)
{
	int ret;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Get service location\n");

	ar_snoc->get_service_nb.notifier_call =
		ath10k_snoc_get_service_location_notify;
	ret = get_service_location(ATH10K_SERVICE_LOCATION_CLIENT_NAME,
				   ATH10K_WLAN_SERVICE_NAME,
				   &ar_snoc->get_service_nb);
	if (ret) {
		ath10k_err(ar, "Get service location failed: %d\n", ret);
		goto out;
	}

	return 0;
out:
	ath10k_err(ar, "PD restart not enabled: %d\n", ret);
	return ret;
}

int ath10k_snoc_pdr_unregister_notifier(struct ath10k *ar)
{
	int i;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	for (i = 0; i < ar_snoc->total_domains; i++) {
		if (ar_snoc->service_notifier[i].handle)
			service_notif_unregister_notifier(
				ar_snoc->service_notifier[i].handle,
				&ar_snoc->service_notifier_nb);
	}

	kfree(ar_snoc->service_notifier);

	ar_snoc->service_notifier = NULL;

	return 0;
}

static int ath10k_snoc_modem_notifier_nb(struct notifier_block *nb,
					 unsigned long code,
					 void *data)
{
	struct notif_data *notif = data;
	struct ath10k_snoc *ar_snoc = container_of(nb, struct ath10k_snoc,
						   modem_ssr_nb);
	struct ath10k *ar = ar_snoc->ar;

	if (code != SUBSYS_BEFORE_SHUTDOWN)
		return NOTIFY_OK;

	if (notif->crashed)
		atomic_set(&ar_snoc->fw_crashed, 1);

	ath10k_dbg(ar, ATH10K_DBG_SNOC, "Modem went down %d\n",
		   atomic_read(&ar_snoc->fw_crashed));
	if (notif->crashed)
		queue_work(ar->workqueue, &ar->restart_work);

	return NOTIFY_OK;
}

int ath10k_snoc_modem_ssr_register_notifier(struct ath10k *ar)
{
	int ret = 0;
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	ar_snoc->modem_ssr_nb.notifier_call = ath10k_snoc_modem_notifier_nb;

	ar_snoc->modem_notify_handler =
		subsys_notif_register_notifier("modem", &ar_snoc->modem_ssr_nb);

	if (IS_ERR(ar_snoc->modem_notify_handler)) {
		ret = PTR_ERR(ar_snoc->modem_notify_handler);
		ath10k_err(ar, "Modem register notifier failed: %d\n", ret);
	}

	return ret;
}

int ath10k_snoc_modem_ssr_unregister_notifier(struct ath10k *ar)
{
	struct ath10k_snoc *ar_snoc = ath10k_snoc_priv(ar);

	subsys_notif_unregister_notifier(ar_snoc->modem_notify_handler,
					 &ar_snoc->modem_ssr_nb);
	ar_snoc->modem_notify_handler = NULL;

	return 0;
}

