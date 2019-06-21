/*
 * Copyright (c) 2013-2016, The Linux Foundation. All rights reserved.
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

#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/spinlock.h>
#include <linux/kernel.h>
#include <dsp/audio_notifier.h>
#include <ipc/apr.h>
#include <ipc/apr_tal.h>

#define DEST_ID APR_DEST_MODEM

enum apr_subsys_state apr_get_subsys_state(void)
{
	return apr_get_modem_state();
}
EXPORT_SYMBOL(apr_get_subsys_state);

void apr_set_subsys_state(void)
{
	apr_set_modem_state(APR_SUBSYS_DOWN);
}
EXPORT_SYMBOL(apr_set_subsys_state);

uint16_t apr_get_data_src(struct apr_hdr *hdr)
{
	return DEST_ID;
}

int apr_get_dest_id(char *dest)
{
	return DEST_ID;
}

void subsys_notif_register(char *client_name, int domain,
			   struct notifier_block *nb)
{
	int ret;

	if (domain != AUDIO_NOTIFIER_MODEM_DOMAIN) {
		pr_debug("%s: Unused domain %d not registering with notifier\n",
			 __func__, domain);
		return;
	}

	ret = audio_notifier_register(client_name, domain, nb);
	if (ret < 0)
		pr_err("%s: Audio notifier register failed for domain %d ret = %d\n",
			__func__, domain, ret);
}

void subsys_notif_deregister(char *client_name)
{
	int ret;

	ret = audio_notifier_deregister(client_name);
	if (ret < 0)
		pr_err("%s: Audio notifier de-register failed for client %s\n",
			__func__, client_name);
}

uint16_t apr_get_reset_domain(uint16_t proc)
{
	return APR_DEST_QDSP6;
}
