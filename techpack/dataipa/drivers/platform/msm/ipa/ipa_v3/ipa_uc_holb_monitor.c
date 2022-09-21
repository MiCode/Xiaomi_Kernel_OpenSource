// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2020, The Linux Foundation. All rights reserved.
 */

#include "ipa_i.h"
#include "ipa_uc_holb_monitor.h"


/**
 * ipa3_uc_holb_client_handler - Iterates through all HOLB clients and sends
 * ADD_HOLB_MONITOR command if necessary.
 *
 */
void ipa3_uc_holb_client_handler(void)
{
	int client_idx;
	struct ipa_uc_holb_client_info *holb_client;
	int num_clients;

	if (!ipa3_ctx->uc_ctx.ipa_use_uc_holb_monitor)
		return;

	mutex_lock(&ipa3_ctx->uc_ctx.holb_monitor.uc_holb_lock);

	num_clients = ipa3_ctx->uc_ctx.holb_monitor.num_holb_clients;
	ipa3_ctx->uc_ctx.uc_holb_enabled = true;

	for (client_idx = 0; client_idx < num_clients; client_idx++) {
		holb_client =
			&(ipa3_ctx->uc_ctx.holb_monitor.client[client_idx]);
		if (holb_client->state == IPA_HOLB_ADD_PENDING) {
			ipa3_uc_add_holb_monitor(holb_client->gsi_chan_hdl,
			holb_client->action_mask, holb_client->max_stuck_cnt,
			holb_client->ee);
			IPADBG("HOLB Client with GSI %d moved to ADD state\n",
				holb_client->gsi_chan_hdl);
			holb_client->state = IPA_HOLB_ADD;
		}
	}

	mutex_unlock(&ipa3_ctx->uc_ctx.holb_monitor.uc_holb_lock);
}

/**
 * ipa3_get_holb_client_idx_by_ch() - Get client index in client
 * array with a specified gsi channel
 * @gsi_chan_hdl: GSI Channel of the client to be monitored
 *
 * Returns client index in client array
 */
static int ipa3_get_holb_client_idx_by_ch(uint16_t gsi_ch)
{
	int client_idx;
	int num_clients = ipa3_ctx->uc_ctx.holb_monitor.num_holb_clients;
	struct ipa_uc_holb_client_info *holb_client;

	for (client_idx = 0; client_idx < num_clients; client_idx++) {
		holb_client =
			&(ipa3_ctx->uc_ctx.holb_monitor.client[client_idx]);
		if (holb_client->gsi_chan_hdl == gsi_ch)
			return client_idx;
	}
	return -EINVAL;
}

/**
 * ipa3_set_holb_client_by_ch() - Set client parameters for specific
 * gsi channel
 * @client: Client values to be set for the gsi channel
 *
 */
void ipa3_set_holb_client_by_ch(struct ipa_uc_holb_client_info client)
{
	uint16_t gsi_ch;
	int client_idx, num_clients;
	struct ipa_uc_holb_client_info *holb_client;


	mutex_lock(&ipa3_ctx->uc_ctx.holb_monitor.uc_holb_lock);

	gsi_ch = client.gsi_chan_hdl;
	client_idx = ipa3_get_holb_client_idx_by_ch(gsi_ch);

	if (client_idx != -EINVAL) {
		holb_client =
			&(ipa3_ctx->uc_ctx.holb_monitor.client[client_idx]);
	} else {
		num_clients = ipa3_ctx->uc_ctx.holb_monitor.num_holb_clients;
		holb_client =
			&(ipa3_ctx->uc_ctx.holb_monitor.client[num_clients]);
		ipa3_ctx->uc_ctx.holb_monitor.num_holb_clients = ++num_clients;
		holb_client->gsi_chan_hdl = gsi_ch;
	}

	holb_client->debugfs_param = client.debugfs_param;
	if (holb_client->debugfs_param)
		holb_client->max_stuck_cnt = client.max_stuck_cnt;
	IPADBG("HOLB gsi_chan %d with max_stuck_cnt %d, set %d\n",
		gsi_ch, holb_client->max_stuck_cnt, holb_client->debugfs_param);

	mutex_unlock(&ipa3_ctx->uc_ctx.holb_monitor.uc_holb_lock);
}

/**
 * ipa3_uc_client_add_holb_monitor() - Sends ADD_HOLB_MONITOR for gsi channels
 * if uC is enabled, else saves client state
 * @gsi_chan_hdl: GSI Channel of the client to be monitored
 * @action_mask: HOLB action mask
 * @max_stuck_cnt: Max number of attempts uC should try before sending an event
 * @ee: EE that the chid belongs to
 *
 * Return value: 0 on success, negative value otherwise
 */
int ipa3_uc_client_add_holb_monitor(uint16_t gsi_ch, uint32_t action_mask,
		uint32_t max_stuck_cnt, uint8_t ee)
{

	struct ipa_uc_holb_client_info *holb_client;
	int ret = 0;
	int client_idx, num_clients;

	if (!ipa3_ctx->uc_ctx.ipa_use_uc_holb_monitor)
		return ret;

	mutex_lock(&ipa3_ctx->uc_ctx.holb_monitor.uc_holb_lock);

	client_idx = ipa3_get_holb_client_idx_by_ch(gsi_ch);
	if (client_idx != -EINVAL) {
		holb_client =
			&(ipa3_ctx->uc_ctx.holb_monitor.client[client_idx]);
	} else {
		num_clients = ipa3_ctx->uc_ctx.holb_monitor.num_holb_clients;
		holb_client =
			&(ipa3_ctx->uc_ctx.holb_monitor.client[num_clients]);
		ipa3_ctx->uc_ctx.holb_monitor.num_holb_clients = ++num_clients;
	}

	holb_client->gsi_chan_hdl = gsi_ch;
	holb_client->action_mask = action_mask;
	if (!holb_client->debugfs_param)
		holb_client->max_stuck_cnt = max_stuck_cnt;
	holb_client->ee = ee;

	if (ipa3_uc_holb_enabled_check()) {
		if (holb_client->state != IPA_HOLB_ADD) {
			IPADBG("GSI chan %d going to ADD state\n",
				holb_client->gsi_chan_hdl);
			ret = ipa3_uc_add_holb_monitor(
			holb_client->gsi_chan_hdl,
			holb_client->action_mask, holb_client->max_stuck_cnt,
			holb_client->ee);
			holb_client->state = IPA_HOLB_ADD;
		}
	} else {
		IPADBG("GSI chan %d going to ADD_PENDING state\n",
			holb_client->gsi_chan_hdl);
		holb_client->state = IPA_HOLB_ADD_PENDING;
	}


	mutex_unlock(&ipa3_ctx->uc_ctx.holb_monitor.uc_holb_lock);
	return ret;
}

/**
 * ipa3_uc_client_del_holb_monitor() - Sends DEL_HOLB_MONITOR for gsi channels
 * if uC is enabled, else saves client state
 * @gsi_chan_hdl: GSI Channel of the client to be monitored
 * @ee: EE that the chid belongs to
 *
 * Return value: 0 on success, negative value otherwise
 */
int ipa3_uc_client_del_holb_monitor(uint16_t gsi_ch, uint8_t ee)
{

	struct ipa_uc_holb_client_info *holb_client;
	int ret = 0;
	int client_idx;

	if (!ipa3_ctx->uc_ctx.ipa_use_uc_holb_monitor)
		return ret;

	mutex_lock(&ipa3_ctx->uc_ctx.holb_monitor.uc_holb_lock);

	client_idx = ipa3_get_holb_client_idx_by_ch(gsi_ch);
	if (client_idx == -EINVAL) {
		IPAERR("Invalid client with GSI chan %d\n", gsi_ch);
		return client_idx;
	}

	holb_client = &(ipa3_ctx->uc_ctx.holb_monitor.client[client_idx]);
	if (ipa3_uc_holb_enabled_check() &&
			holb_client->state == IPA_HOLB_ADD) {
		IPADBG("GSI chan %d going from ADD to DEL state\n",
			holb_client->gsi_chan_hdl);
		ret = ipa3_uc_del_holb_monitor(holb_client->gsi_chan_hdl,
		ee);
		holb_client->state = IPA_HOLB_DEL;
	} else if (!ipa3_uc_holb_enabled_check() &&
			holb_client->state == IPA_HOLB_ADD_PENDING) {
		IPADBG("GSI chan %d going from ADD_PENDING to DEL state\n",
			holb_client->gsi_chan_hdl);
		holb_client->state = IPA_HOLB_DEL;
	}

	mutex_unlock(&ipa3_ctx->uc_ctx.holb_monitor.uc_holb_lock);
	return ret;
}

void ipa3_uc_holb_event_log(uint16_t gsi_ch, bool enable,
	uint32_t qtimer_lsb, uint32_t qtimer_msb)
{
	struct ipa_uc_holb_client_info *holb_client;
	int client_idx;
	int current_idx;

	if (!ipa3_ctx->uc_ctx.ipa_use_uc_holb_monitor)
		return;

	/* HOLB client indexes are reused when a peripheral is
	 * disconnected and connected back. And so there is no
	 * need to acquire the lock here as we get the events from
	 * uC only after a channel is connected atleast once. Also
	 * if we acquire a lock here we will run into a deadlock
	 * as we can get uC holb events and a response to add/delete
	 * commands at the same time.
	 */
	client_idx = ipa3_get_holb_client_idx_by_ch(gsi_ch);
	if (client_idx == -EINVAL) {
		IPAERR("Invalid client with GSI chan %d\n", gsi_ch);
		return;
	}
	holb_client = &(ipa3_ctx->uc_ctx.holb_monitor.client[client_idx]);
	current_idx = holb_client->current_idx;

	holb_client->events[current_idx].enable = enable;
	holb_client->events[current_idx].qTimerLSB = qtimer_lsb;
	holb_client->events[current_idx].qTimerMSB = qtimer_msb;
	if (enable)
		holb_client->enable_cnt++;
	else
		holb_client->disable_cnt++;
	holb_client->current_idx = (holb_client->current_idx + 1) %
		IPA_HOLB_EVENT_LOG_MAX;
}
