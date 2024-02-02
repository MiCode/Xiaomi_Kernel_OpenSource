/* Copyright (c) 2019 The Linux Foundation. All rights reserved.
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

#include <linux/msm_gsi.h>

#include "ipa_eth_i.h"

static void ipa_eth_gsi_ev_err(struct gsi_evt_err_notify *notify)
{
	struct ipa_eth_channel *ch = notify->user_data;
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	ipa_eth_dev_err(ch->eth_dev,
			"Error (id=%d, edesc=%04x) in GSI event ring %u",
			notify->evt_id, notify->err_desc,
			ep_ctx->gsi_evt_ring_hdl);
}

static void ipa_eth_gsi_ch_err(struct gsi_chan_err_notify *notify)
{
	struct ipa_eth_channel *ch = notify->chan_user_data;
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	ipa_eth_dev_err(ch->eth_dev,
			"Error (id=%d, edesc=%04x) in GSI channel %u",
			notify->evt_id, notify->err_desc,
			ep_ctx->gsi_chan_hdl);
}

/*
 * ipa_eth_gsi_alloc() - Allocate GSI channel and event ring for an offload
 *                       channel, optionally writing to the ring scratch
 *                       register and fetch the ring doorbell address
 * @ch: Offload channel
 * @gsi_ev_props: Properties of the GSI event ring to be allocated
 * @gsi_ev_scratch: Optional. Points to the value to be written to GSI
 *                  event ring scratch register
 * @gsi_ev_db: Optional. Writes event ring doorbell LSB address to the location
 *                       pointed to by the argument
 * @gsi_ch_props: Properties of the GSI channel to be allocated
 * @gsi_ch_scratch: Optional. Points to the value to be written to GSI
 *                  event ring scratch register
 * @gsi_ch_db: Optional. Writes channel doorbell LSB address to the location
 *                       pointed to by the argument
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_gsi_alloc(struct ipa_eth_channel *ch,
		      struct gsi_evt_ring_props *gsi_ev_props,
		      union gsi_evt_scratch *gsi_ev_scratch,
		      phys_addr_t *gsi_ev_db,
		      struct gsi_chan_props *gsi_ch_props,
		      union gsi_channel_scratch *gsi_ch_scratch,
		      phys_addr_t *gsi_ch_db)
{
	enum gsi_status gsi_rc = GSI_STATUS_SUCCESS;
	const struct ipa_gsi_ep_config *gsi_ep_cfg;
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	if (!ep_ctx->valid) {
		ipa_eth_dev_err(ch->eth_dev, "EP context is not initialized");
		return -EFAULT;
	}

	gsi_ep_cfg = ipa3_get_gsi_ep_info(ep_ctx->client);
	if (!gsi_ep_cfg) {
		ipa_eth_dev_err(ch->eth_dev, "Failed to obtain GSI EP info");
		return -EFAULT;
	}

	if (!gsi_ev_props->err_cb) {
		gsi_ev_props->err_cb = ipa_eth_gsi_ev_err;
		gsi_ev_props->user_data = ch;
	}

	gsi_rc = gsi_alloc_evt_ring(gsi_ev_props, ipa3_ctx->gsi_dev_hdl,
		&ep_ctx->gsi_evt_ring_hdl);
	if (gsi_rc != GSI_STATUS_SUCCESS) {
		ipa_eth_dev_err(ch->eth_dev, "Failed to alloc GSI event ring");
		return -EFAULT;
	}

	ipa_eth_dev_dbg(ch->eth_dev, "GSI event ring handle is %lu",
			ep_ctx->gsi_evt_ring_hdl);

	if (gsi_ev_db) {
		u32 db_addr_lsb = 0;
		u32 db_addr_msb = 0;

		gsi_rc = gsi_query_evt_ring_db_addr(ep_ctx->gsi_evt_ring_hdl,
			&db_addr_lsb, &db_addr_msb);
		if (gsi_rc != GSI_STATUS_SUCCESS) {
			ipa_eth_dev_err(ch->eth_dev,
				"Failed to get DB address for event ring %lu",
				ep_ctx->gsi_evt_ring_hdl);
			goto err_free_ev;
		}

		ipa_eth_dev_dbg(ch->eth_dev,
				"GSI event ring %lu DB address LSB is 0x%08x",
				ep_ctx->gsi_evt_ring_hdl, db_addr_lsb);
		ipa_eth_dev_dbg(ch->eth_dev,
				"GSI event ring %lu DB address MSB is 0x%08x",
				ep_ctx->gsi_evt_ring_hdl, db_addr_msb);

		*gsi_ev_db = db_addr_lsb;
	}

	if (gsi_ev_scratch) {
		gsi_rc = gsi_write_evt_ring_scratch(ep_ctx->gsi_evt_ring_hdl,
				*gsi_ev_scratch);
		if (gsi_rc != GSI_STATUS_SUCCESS) {
			ipa_eth_dev_err(ch->eth_dev,
				"Failed to write scratch for event ring %lu",
				ep_ctx->gsi_evt_ring_hdl);
			goto err_free_ev;
		}
	}

	gsi_ch_props->ch_id = gsi_ep_cfg->ipa_gsi_chan_num;
	gsi_ch_props->evt_ring_hdl = ep_ctx->gsi_evt_ring_hdl;

	gsi_ch_props->prefetch_mode = gsi_ep_cfg->prefetch_mode;
	gsi_ch_props->empty_lvl_threshold = gsi_ep_cfg->prefetch_threshold;

	if (!gsi_ch_props->err_cb) {
		gsi_ch_props->err_cb = ipa_eth_gsi_ch_err;
		gsi_ch_props->chan_user_data = ch;
	}

	gsi_rc = gsi_alloc_channel(gsi_ch_props, ipa3_ctx->gsi_dev_hdl,
		&ep_ctx->gsi_chan_hdl);
	if (gsi_rc != GSI_STATUS_SUCCESS) {
		ipa_eth_dev_err(ch->eth_dev, "Failed to alloc GSI channel");
		goto err_free_ev;
	}

	ipa_eth_dev_dbg(ch->eth_dev, "GSI channel handle is %lu",
			ep_ctx->gsi_chan_hdl);

	if (gsi_ch_db) {
		u32 db_addr_lsb = 0;
		u32 db_addr_msb = 0;

		gsi_rc = gsi_query_channel_db_addr(ep_ctx->gsi_chan_hdl,
			&db_addr_lsb, &db_addr_msb);
		if (gsi_rc != GSI_STATUS_SUCCESS) {
			ipa_eth_dev_err(ch->eth_dev,
				"Failed to get DB address for channel %lu",
				ep_ctx->gsi_chan_hdl);
			goto err_free_ch;
		}

		ipa_eth_dev_dbg(ch->eth_dev,
				"GSI channel %lu DB address LSB is 0x%08x",
				ep_ctx->gsi_chan_hdl, db_addr_lsb);
		ipa_eth_dev_dbg(ch->eth_dev,
				"GSI channel %lu DB address MSB is 0x%08x",
				ep_ctx->gsi_chan_hdl, db_addr_msb);

		*gsi_ch_db = db_addr_lsb;
	}

	if (gsi_ch_scratch) {
		gsi_rc = gsi_write_channel_scratch(ep_ctx->gsi_chan_hdl,
				*gsi_ch_scratch);
		if (gsi_rc != GSI_STATUS_SUCCESS) {
			ipa_eth_dev_err(ch->eth_dev,
				"Failed to write scratch for channel %lu",
				ep_ctx->gsi_chan_hdl);
			goto err_free_ch;
		}
	}

	return 0;

err_free_ch:
	if (gsi_dealloc_channel(ep_ctx->gsi_chan_hdl))
		ipa_eth_dev_err(ch->eth_dev,
				"Failed to dealloc GSI channel %lu",
				ep_ctx->gsi_chan_hdl);

	ep_ctx->gsi_chan_hdl = ~0;

err_free_ev:
	if (gsi_dealloc_evt_ring(ep_ctx->gsi_evt_ring_hdl))
		ipa_eth_dev_err(ch->eth_dev,
				"Failed to dealloc GSI event ring %lu",
				ep_ctx->gsi_evt_ring_hdl);

	ep_ctx->gsi_evt_ring_hdl = ~0;

	return gsi_rc;
}
EXPORT_SYMBOL(ipa_eth_gsi_alloc);

/**
 * ipa_eth_gsi_dealloc() - De-allocate GSI event ring and channel associated
 *                         with an offload channel, previously allocated with
 *                         ipa_eth_gsi_alloc()
 * @ch: Offload channel
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_gsi_dealloc(struct ipa_eth_channel *ch)
{
	enum gsi_status gsi_rc = GSI_STATUS_SUCCESS;
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	if (!ep_ctx->valid) {
		ipa_eth_dev_err(ch->eth_dev, "EP context is not initialized");
		return -EFAULT;
	}

	if (ep_ctx->gsi_chan_hdl != ~0) {
		gsi_rc = gsi_reset_channel(ep_ctx->gsi_chan_hdl);
		if (gsi_rc != GSI_STATUS_SUCCESS) {
			ipa_eth_dev_err(ch->eth_dev,
				"Failed to reset channel %u",
				ep_ctx->gsi_chan_hdl);
			return gsi_rc;
		}

		gsi_rc = gsi_dealloc_channel(ep_ctx->gsi_chan_hdl);
		if (gsi_rc != GSI_STATUS_SUCCESS) {
			ipa_eth_dev_err(ch->eth_dev,
					"Failed to dealloc channel %lu",
					ep_ctx->gsi_chan_hdl);
			return gsi_rc;
		}

		ep_ctx->gsi_chan_hdl = ~0;
	}

	if (ep_ctx->gsi_evt_ring_hdl != ~0) {
		gsi_rc = gsi_reset_evt_ring(ep_ctx->gsi_evt_ring_hdl);
		if (gsi_rc != GSI_STATUS_SUCCESS) {
			ipa_eth_dev_err(ch->eth_dev,
				"Failed to reset event ring %lu",
				ep_ctx->gsi_evt_ring_hdl);
			return gsi_rc;
		}

		gsi_rc = gsi_dealloc_evt_ring(ep_ctx->gsi_evt_ring_hdl);
		if (gsi_rc != GSI_STATUS_SUCCESS) {
			ipa_eth_dev_err(ch->eth_dev,
					"Failed to dealloc event ring %lu",
					ep_ctx->gsi_evt_ring_hdl);
			return gsi_rc;
		}

		ep_ctx->gsi_evt_ring_hdl = ~0;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_eth_gsi_dealloc);

/*
 * ipa_eth_gsi_ring_evtring() - Ring an offload channel event ring doorbell
 * @ch: Offload channel associated with the event ring
 * @value: Value to write to the doorbell
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_gsi_ring_evtring(struct ipa_eth_channel *ch, u64 value)
{
	enum gsi_status gsi_rc = GSI_STATUS_SUCCESS;
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	if (!ep_ctx->valid) {
		ipa_eth_dev_err(ch->eth_dev, "EP context is not initialized");
		return -EFAULT;
	}

	gsi_rc = gsi_ring_evt_ring_db(ep_ctx->gsi_evt_ring_hdl, value);
	if (gsi_rc != GSI_STATUS_SUCCESS) {
		ipa_eth_dev_err(ch->eth_dev,
				"Failed to ring DB for event ring %lu",
				ep_ctx->gsi_evt_ring_hdl);
		return gsi_rc;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_eth_gsi_ring_evtring);

/*
 * ipa_eth_gsi_ring_channel() - Ring an offload channel GSI channel doorbell
 * @ch: Offload channel associated with the GSI channel
 * @value: Value to write to the doorbell
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_gsi_ring_channel(struct ipa_eth_channel *ch, u64 value)
{
	enum gsi_status gsi_rc = GSI_STATUS_SUCCESS;
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	if (!ep_ctx->valid) {
		ipa_eth_dev_err(ch->eth_dev, "EP context is not initialized");
		return -EFAULT;
	}

	gsi_rc = gsi_ring_ch_ring_db(ep_ctx->gsi_chan_hdl, value);
	if (gsi_rc != GSI_STATUS_SUCCESS) {
		ipa_eth_dev_err(ch->eth_dev,
				"Failed to ring DB for channel %lu",
				ep_ctx->gsi_chan_hdl);
		return gsi_rc;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_eth_gsi_ring_channel);

/**
 * ipa_eth_gsi_start() - Start GSI channel associated with offload channel
 * @ch: Offload channel
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_gsi_start(struct ipa_eth_channel *ch)
{
	enum gsi_status gsi_rc = GSI_STATUS_SUCCESS;
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	if (!ep_ctx->valid) {
		ipa_eth_dev_err(ch->eth_dev, "EP context is not initialized");
		return -EFAULT;
	}

	gsi_rc = gsi_start_channel(ep_ctx->gsi_chan_hdl);
	if (gsi_rc != GSI_STATUS_SUCCESS) {
		ipa_eth_dev_err(ch->eth_dev, "Failed to start GSI channel %lu",
				ep_ctx->gsi_chan_hdl);
		return gsi_rc;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_eth_gsi_start);

/**
 * ipa_eth_gsi_stop() - Stop GSI channel associated with offload channel
 * @ch: Offload channel
 *
 * Return: 0 on success, negative errno otherwise
 */
int ipa_eth_gsi_stop(struct ipa_eth_channel *ch)
{
	enum gsi_status gsi_rc = GSI_STATUS_SUCCESS;
	struct ipa3_ep_context *ep_ctx = &ipa3_ctx->ep[ch->ipa_ep_num];

	if (!ep_ctx->valid) {
		ipa_eth_dev_err(ch->eth_dev, "EP context is not initialized");
		return -EFAULT;
	}

	gsi_rc = gsi_stop_channel(ep_ctx->gsi_chan_hdl);
	if (gsi_rc != GSI_STATUS_SUCCESS) {
		ipa_eth_dev_err(ch->eth_dev, "Failed to stop GSI channel %lu",
				ep_ctx->gsi_chan_hdl);
		return gsi_rc;
	}

	return 0;
}
EXPORT_SYMBOL(ipa_eth_gsi_stop);
