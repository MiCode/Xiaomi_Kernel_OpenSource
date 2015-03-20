/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

static const char *lpass_subsys_name = "adsp";

static void apr_set_subsys_state_v2(void)
{
	apr_set_q6_state(APR_SUBSYS_DOWN);
	apr_set_modem_state(APR_SUBSYS_UP);
}

enum apr_subsys_state apr_get_q6_state_v2(void)
{
	return apr_get_q6_state();
}

const char *apr_get_lpass_subsys_name(void)
{
	return lpass_subsys_name;
}

static uint16_t apr_get_data_src_v2(struct apr_hdr *hdr)
{
	if (hdr->src_domain == APR_DOMAIN_MODEM)
		return APR_DEST_MODEM;
	else if (hdr->src_domain == APR_DOMAIN_ADSP)
		return APR_DEST_QDSP6;
	else {
		pr_err("APR: Pkt from wrong source: %d\n", hdr->src_domain);
		return APR_DEST_MAX;		/*RETURN INVALID VALUE*/
	}
}

static int apr_get_dest_id_v2(char *dest)
{
	if (!strcmp(dest, "ADSP"))
		return APR_DEST_QDSP6;
	else
		return APR_DEST_MODEM;
}

static void subsys_notif_register_v2(struct notifier_block *mod_notif,
				struct notifier_block *lp_notif)
{
	subsys_notif_register_notifier("modem", mod_notif);
	subsys_notif_register_notifier(apr_get_lpass_subsys_name(), lp_notif);
}

static uint16_t apr_get_reset_domain_v2(uint16_t proc)
{
	return proc;
}

static bool apr_register_voice_svc_v2(void)
{
	return true;
}

int apr_get_v2_ops(struct apr_func_dsp *ops)
{
	int ret = 0;
	if (ops) {
		ops->apr_get_data_src = apr_get_data_src_v2;
		ops->apr_get_dest_id = apr_get_dest_id_v2;
		ops->apr_get_reset_domain = apr_get_reset_domain_v2;
		ops->apr_register_voice_svc = apr_register_voice_svc_v2;
		ops->apr_set_subsys_state = apr_set_subsys_state_v2;
		ops->apr_get_adsp_state = apr_get_q6_state_v2;
		ops->apr_get_adsp_subsys_name = apr_get_lpass_subsys_name;
		ops->subsys_notif_register = subsys_notif_register_v2;
	} else {
		pr_err("%s: Invalid params\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}
