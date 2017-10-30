/*
 * Copyright (c) 2012-2016 The Linux Foundation. All rights reserved.
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
#include <linux/qdsp6v2/apr.h>
#include <linux/qdsp6v2/apr_tal.h>
#include <linux/qdsp6v2/dsp_debug.h>
#include <linux/qdsp6v2/audio_notifier.h>

enum apr_subsys_state apr_get_subsys_state(void)
{
	return apr_get_q6_state();
}

void apr_set_subsys_state(void)
{
	apr_set_q6_state(APR_SUBSYS_DOWN);
	apr_set_modem_state(APR_SUBSYS_UP);
}

uint16_t apr_get_data_src(struct apr_hdr *hdr)
{
	if (hdr->src_domain == APR_DOMAIN_MODEM)
		return APR_DEST_MODEM;
	else if (hdr->src_domain == APR_DOMAIN_ADSP)
		return APR_DEST_QDSP6;
	else if (hdr->src_domain == APR_DOMAIN_SDSP)
		return APR_DEST_DSPS;
	else {
		pr_err("APR: Pkt from wrong source: %d\n", hdr->src_domain);
		return APR_DEST_MAX;		/*RETURN INVALID VALUE*/
	}
}

int apr_get_dest_id(char *dest)
{
	if (!strcmp(dest, "ADSP"))
		return APR_DEST_QDSP6;
	else if (!strcmp(dest, "SDSP"))
		return APR_DEST_DSPS;
	else
		return APR_DEST_MODEM;
}

void subsys_notif_register(char *client_name, int domain,
			   struct notifier_block *nb)
{
	int ret;

	ret = audio_notifier_register(client_name, domain, nb);
	if (ret < 0)
		pr_err("%s: Audio notifier register failed for domain %d ret = %d\n",
			__func__, domain, ret);
}

uint16_t apr_get_reset_domain(uint16_t proc)
{
	return proc;
}
