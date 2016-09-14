/* Copyright (c) 2016, The Linux Foundation. All rights reserved.
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
#include <linux/qdsp6v2/audio_ssr.h>
#include <soc/qcom/scm.h>
#include <soc/qcom/subsystem_restart.h>
#include <soc/qcom/subsystem_notif.h>

#define SCM_Q6_NMI_CMD 0x1

static char *audio_ssr_domains[] = {
	"adsp",
	"modem"
};

void *audio_ssr_register(int domain_id, struct notifier_block *nb)
{
	if ((domain_id < 0) ||
	    (domain_id >= AUDIO_SSR_DOMAIN_MAX)) {
		pr_err("%s: Invalid service ID %d\n", __func__, domain_id);
		return ERR_PTR(-EINVAL);
	}

	return subsys_notif_register_notifier(
		audio_ssr_domains[domain_id], nb);
}
EXPORT_SYMBOL(audio_ssr_register);

int audio_ssr_deregister(void *handle, struct notifier_block *nb)
{
	return subsys_notif_unregister_notifier(handle, nb);
}
EXPORT_SYMBOL(audio_ssr_deregister);

void audio_ssr_send_nmi(void *ssr_cb_data)
{
	struct notif_data *data = (struct notif_data *)ssr_cb_data;
	struct scm_desc desc;

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
		pr_debug("%s: Q6 NMI was sent.\n", __func__);
	}
}
EXPORT_SYMBOL(audio_ssr_send_nmi);
