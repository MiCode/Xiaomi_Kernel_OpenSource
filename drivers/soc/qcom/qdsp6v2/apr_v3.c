/*
 * Copyright (c) 2013-2015, The Linux Foundation. All rights reserved.
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

#define DEST_ID APR_DEST_MODEM
static const char *modem_subsys_name = "modem";

static void apr_set_subsys_state_v3(void)
{
	apr_set_modem_state(APR_SUBSYS_DOWN);
}

enum apr_subsys_state apr_get_modem_state_v3(void)
{
	return apr_get_modem_state();
}

const char *apr_get_modem_subsys_name(void)
{
	return modem_subsys_name;
}

static uint16_t apr_get_data_src_v3(struct apr_hdr *hdr)
{
	return DEST_ID;
}

static int apr_get_dest_id_v3(char *dest)
{
	return DEST_ID;
}

static void subsys_notif_register_v3(struct notifier_block *mod_notif,
				struct notifier_block *lp_notif)
{
	subsys_notif_register_notifier("modem", mod_notif);
}

static uint16_t apr_get_reset_domain_v3(uint16_t proc)
{
	return APR_DEST_QDSP6;
}

static bool apr_register_voice_svc_v3(void)
{
	return false;
}

int apr_get_v3_ops(struct apr_func_dsp *ops)
{
	int ret = 0;
	if (ops) {
		ops->apr_get_data_src = apr_get_data_src_v3;
		ops->apr_get_dest_id = apr_get_dest_id_v3;
		ops->apr_get_reset_domain = apr_get_reset_domain_v3;
		ops->apr_register_voice_svc = apr_register_voice_svc_v3;
		ops->apr_set_subsys_state = apr_set_subsys_state_v3;
		ops->apr_get_adsp_state = apr_get_modem_state_v3;
		ops->apr_get_adsp_subsys_name = apr_get_modem_subsys_name;
		ops->subsys_notif_register = subsys_notif_register_v3;
	} else {
		pr_err("%s: Invalid params\n", __func__);
		ret = -EINVAL;
	}
	return ret;
}
