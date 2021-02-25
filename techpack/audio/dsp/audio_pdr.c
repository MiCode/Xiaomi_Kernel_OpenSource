// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2016-2017, The Linux Foundation. All rights reserved.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <soc/qcom/service-locator.h>
#include <soc/qcom/service-notifier.h>
#include "audio_pdr.h"

static struct pd_qmi_client_data audio_pdr_services[AUDIO_PDR_DOMAIN_MAX] = {
	{	/* AUDIO_PDR_DOMAIN_ADSP */
		.client_name = "audio_pdr_adsp",
		.service_name = "avs/audio"
	}
};

struct srcu_notifier_head audio_pdr_cb_list;

static int audio_pdr_locator_callback(struct notifier_block *this,
				      unsigned long opcode, void *data)
{
	unsigned long pdr_state = AUDIO_PDR_FRAMEWORK_DOWN;

	if (opcode == LOCATOR_DOWN) {
		pr_debug("%s: Service %s is down!", __func__,
			audio_pdr_services[AUDIO_PDR_DOMAIN_ADSP].
			service_name);
		goto done;
	}

	memcpy(&audio_pdr_services, data,
		sizeof(audio_pdr_services[AUDIO_PDR_DOMAIN_ADSP]));
	if (audio_pdr_services[AUDIO_PDR_DOMAIN_ADSP].total_domains == 1) {
		pr_debug("%s: Service %s, returned total domains %d, ",
			__func__,
			audio_pdr_services[AUDIO_PDR_DOMAIN_ADSP].service_name,
			audio_pdr_services[AUDIO_PDR_DOMAIN_ADSP].
			total_domains);
		pdr_state = AUDIO_PDR_FRAMEWORK_UP;
		goto done;
	} else
		pr_err("%s: Service %s returned invalid total domains %d",
			__func__,
			audio_pdr_services[AUDIO_PDR_DOMAIN_ADSP].service_name,
			audio_pdr_services[AUDIO_PDR_DOMAIN_ADSP].
			total_domains);
done:
	srcu_notifier_call_chain(&audio_pdr_cb_list, pdr_state, NULL);
	return NOTIFY_OK;
}

static struct notifier_block audio_pdr_locator_nb = {
	.notifier_call = audio_pdr_locator_callback,
	.priority = 0,
};

/**
 * audio_pdr_register -
 *        register to PDR framework
 *
 * @nb: notifier block
 *
 * Returns 0 on success or error on failure
 */
int audio_pdr_register(struct notifier_block *nb)
{
	if (nb == NULL) {
		pr_err("%s: Notifier block is NULL\n", __func__);
		return -EINVAL;
	}
	return srcu_notifier_chain_register(&audio_pdr_cb_list, nb);
}
EXPORT_SYMBOL(audio_pdr_register);

/**
 * audio_pdr_deregister -
 *        Deregister from PDR framework
 *
 * @nb: notifier block
 *
 * Returns 0 on success or error on failure
 */
int audio_pdr_deregister(struct notifier_block *nb)
{
	if (nb == NULL) {
		pr_err("%s: Notifier block is NULL\n", __func__);
		return -EINVAL;
	}
	return srcu_notifier_chain_unregister(&audio_pdr_cb_list, nb);
}
EXPORT_SYMBOL(audio_pdr_deregister);

void *audio_pdr_service_register(int domain_id,
				 struct notifier_block *nb, int *curr_state)
{
	void *handle;

	if ((domain_id < 0) ||
	    (domain_id >= AUDIO_PDR_DOMAIN_MAX)) {
		pr_err("%s: Invalid service ID %d\n", __func__, domain_id);
		return ERR_PTR(-EINVAL);
	}

	handle = service_notif_register_notifier(
		audio_pdr_services[domain_id].domain_list[0].name,
		audio_pdr_services[domain_id].domain_list[0].instance_id,
		nb, curr_state);
	if (IS_ERR_OR_NULL(handle)) {
		pr_err("%s: Failed to register for service %s, instance %d\n",
			__func__,
			audio_pdr_services[domain_id].domain_list[0].name,
			audio_pdr_services[domain_id].domain_list[0].
			instance_id);
	}
	return handle;
}
EXPORT_SYMBOL(audio_pdr_service_register);

int audio_pdr_service_deregister(void *service_handle,
	struct notifier_block *nb)
{
	int ret;

	if (service_handle == NULL) {
		pr_err("%s: service handle is NULL\n", __func__);
		ret = -EINVAL;
		goto done;
	}

	ret = service_notif_unregister_notifier(
		service_handle, nb);
	if (ret < 0)
		pr_err("%s: Failed to deregister service ret %d\n",
			__func__, ret);
done:
	return ret;
}
EXPORT_SYMBOL(audio_pdr_service_deregister);

static int __init audio_pdr_subsys_init(void)
{
	srcu_init_notifier_head(&audio_pdr_cb_list);
	return 0;
}

static int __init audio_pdr_late_init(void)
{
	int ret;

	audio_pdr_subsys_init();

	ret = get_service_location(
		audio_pdr_services[AUDIO_PDR_DOMAIN_ADSP].client_name,
		audio_pdr_services[AUDIO_PDR_DOMAIN_ADSP].service_name,
		&audio_pdr_locator_nb);
	if (ret < 0) {
		pr_err("%s get_service_location failed ret %d\n",
			__func__, ret);
		srcu_notifier_call_chain(&audio_pdr_cb_list,
					 AUDIO_PDR_FRAMEWORK_DOWN, NULL);
	}

	return ret;
}
module_init(audio_pdr_late_init);

static void __exit audio_pdr_late_exit(void)
{
}
module_exit(audio_pdr_late_exit);

MODULE_DESCRIPTION("PDR framework driver");
MODULE_LICENSE("GPL v2");
