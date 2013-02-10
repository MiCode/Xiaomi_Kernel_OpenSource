/* Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

#include <linux/export.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <mach/bam_dmux.h>
#include <mach/ipa.h>
#include <mach/sps.h>
#include "a2_service.h"

static struct rmnet_bridge_cb_type {
	u32 producer_handle;
	u32 consumer_handle;
	u32 ipa_producer_handle;
	u32 ipa_consumer_handle;
	bool is_connected;
} rmnet_bridge_cb;

/**
* rmnet_bridge_init() - Initialize RmNet bridge module
*
* Return codes:
* 0: success
*/
int rmnet_bridge_init(void)
{
	memset(&rmnet_bridge_cb, 0, sizeof(struct rmnet_bridge_cb_type));

	return 0;
}
EXPORT_SYMBOL(rmnet_bridge_init);

/**
* rmnet_bridge_disconnect() - Disconnect RmNet bridge module
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int rmnet_bridge_disconnect(void)
{
	int ret = 0;
	if (false == rmnet_bridge_cb.is_connected) {
		pr_err("%s: trying to disconnect already disconnected RmNet bridge\n",
		       __func__);
		goto bail;
	}

	rmnet_bridge_cb.is_connected = false;

	ret = ipa_bridge_teardown(IPA_BRIDGE_DIR_DL, IPA_BRIDGE_TYPE_TETHERED,
				  rmnet_bridge_cb.ipa_consumer_handle);
	ret = ipa_bridge_teardown(IPA_BRIDGE_DIR_UL, IPA_BRIDGE_TYPE_TETHERED,
				  rmnet_bridge_cb.ipa_producer_handle);
bail:
	return ret;
}
EXPORT_SYMBOL(rmnet_bridge_disconnect);

/**
* rmnet_bridge_connect() - Connect RmNet bridge module
* @producer_hdl:	IPA producer handle
* @consumer_hdl:	IPA consumer handle
* @wwan_logical_channel_id:	WWAN logical channel ID
*
* Return codes:
* 0: success
* -EINVAL: invalid parameters
*/
int rmnet_bridge_connect(u32 producer_hdl,
			 u32 consumer_hdl,
			 int wwan_logical_channel_id)
{
	struct ipa_sys_connect_params props;
	int ret = 0;

	if (true == rmnet_bridge_cb.is_connected) {
		ret = 0;
		pr_err("%s: trying to connect already connected RmNet bridge\n",
		       __func__);
		goto bail;
	}

	rmnet_bridge_cb.consumer_handle = consumer_hdl;
	rmnet_bridge_cb.producer_handle = producer_hdl;
	rmnet_bridge_cb.is_connected = true;

	memset(&props, 0, sizeof(props));
	props.ipa_ep_cfg.mode.mode = IPA_DMA;
	props.ipa_ep_cfg.mode.dst = IPA_CLIENT_USB_CONS;
	props.client = IPA_CLIENT_A2_TETHERED_PROD;
	props.desc_fifo_sz = 0x800;
	/* setup notification callback if needed */

	ret = ipa_bridge_setup(IPA_BRIDGE_DIR_DL, IPA_BRIDGE_TYPE_TETHERED,
			&props, &rmnet_bridge_cb.ipa_consumer_handle);
	if (ret) {
		pr_err("%s: IPA DL bridge setup failure\n", __func__);
		goto bail_dl;
	}

	memset(&props, 0, sizeof(props));
	props.client = IPA_CLIENT_A2_TETHERED_CONS;
	props.desc_fifo_sz = 0x800;
	/* setup notification callback if needed */

	ret = ipa_bridge_setup(IPA_BRIDGE_DIR_UL, IPA_BRIDGE_TYPE_TETHERED,
			&props, &rmnet_bridge_cb.ipa_producer_handle);
	if (ret) {
		pr_err("%s: IPA UL bridge setup failure\n", __func__);
		goto bail_ul;
	}
	return 0;
bail_ul:
	ipa_bridge_teardown(IPA_BRIDGE_DIR_DL, IPA_BRIDGE_TYPE_TETHERED,
			    rmnet_bridge_cb.ipa_consumer_handle);
bail_dl:
	rmnet_bridge_cb.is_connected = false;
bail:
	return ret;
}
EXPORT_SYMBOL(rmnet_bridge_connect);

void rmnet_bridge_get_client_handles(u32 *producer_handle,
		u32 *consumer_handle)
{
	if (producer_handle == NULL || consumer_handle == NULL)
		return;

	*producer_handle = rmnet_bridge_cb.producer_handle;
	*consumer_handle = rmnet_bridge_cb.consumer_handle;
}
