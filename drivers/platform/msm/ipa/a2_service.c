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

#include <linux/kernel.h>
#include <linux/types.h>
#include <mach/bam_dmux.h>
#include <mach/ipa.h>
#include <mach/sps.h>
#include "ipa_i.h"

static struct a2_service_cb_type {
	void *tx_complete_cb;
	void *rx_cb;
	u32 producer_handle;
	u32 consumer_handle;
} a2_service_cb;

static struct sps_mem_buffer data_mem_buf[2];
static struct sps_mem_buffer desc_mem_buf[2];

static int connect_pipe_ipa(enum a2_mux_pipe_direction pipe_dir,
			u8 *usb_pipe_idx,
			u32 *clnt_hdl,
			struct sps_pipe *pipe);

static int a2_ipa_connect_pipe(struct ipa_connect_params *in_params,
		struct ipa_sps_params *out_params, u32 *clnt_hdl);

/**
 * a2_mux_initialize() - initialize A2 MUX module
 *
 * Return codes:
 * 0: success
 */
int a2_mux_initialize(void)
{
	(void) msm_bam_dmux_ul_power_vote();

	return 0;
}

/**
 * a2_mux_close() - close A2 MUX module
 *
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int a2_mux_close(void)
{
	int ret = 0;

	(void) msm_bam_dmux_ul_power_unvote();

	ret = ipa_disconnect(a2_service_cb.consumer_handle);
	if (0 != ret) {
		pr_err("%s: ipa_disconnect failure\n", __func__);
		goto bail;
	}

	ret = ipa_disconnect(a2_service_cb.producer_handle);
	if (0 != ret) {
		pr_err("%s: ipa_disconnect failure\n", __func__);
		goto bail;
	}

	ret = 0;

bail:

	return ret;
}

/**
 * a2_mux_open_port() - open connection to A2
 * @wwan_logical_channel_id:	 WWAN logical channel ID
 * @rx_cb:	Rx callback
 * @tx_complete_cb:	Tx completed callback
 *
 * Return codes:
 * 0: success
 * -EINVAL: invalid parameters
 */
int a2_mux_open_port(int wwan_logical_channel_id, void *rx_cb,
		void *tx_complete_cb)
{
	int ret = 0;
	u8 src_pipe = 0;
	u8 dst_pipe = 0;
	struct sps_pipe *a2_to_ipa_pipe = NULL;
	struct sps_pipe *ipa_to_a2_pipe = NULL;

	(void) wwan_logical_channel_id;

	a2_service_cb.rx_cb = rx_cb;
	a2_service_cb.tx_complete_cb = tx_complete_cb;

	ret = connect_pipe_ipa(A2_TO_IPA,
			&src_pipe,
			&(a2_service_cb.consumer_handle),
			a2_to_ipa_pipe);
	if (ret) {
		pr_err("%s: A2 to IPA pipe connection failure\n", __func__);
		goto bail;
	}

	ret = connect_pipe_ipa(IPA_TO_A2,
			&dst_pipe,
			&(a2_service_cb.producer_handle),
			ipa_to_a2_pipe);
	if (ret) {
		pr_err("%s: IPA to A2 pipe connection failure\n", __func__);
		sps_disconnect(a2_to_ipa_pipe);
		sps_free_endpoint(a2_to_ipa_pipe);
		(void) ipa_disconnect(a2_service_cb.consumer_handle);
		goto bail;
	}

	ret = 0;

bail:

	return ret;
}

static int connect_pipe_ipa(enum a2_mux_pipe_direction pipe_dir,
			u8 *usb_pipe_idx,
			u32 *clnt_hdl,
			struct sps_pipe *pipe)
{
	int ret;
	struct sps_connect connection = {0, };
	u32 a2_handle = 0;
	u32 a2_phy_addr = 0;
	struct a2_mux_pipe_connection pipe_connection = { 0, };
	struct ipa_connect_params ipa_in_params;
	struct ipa_sps_params sps_out_params;

	memset(&ipa_in_params, 0, sizeof(ipa_in_params));
	memset(&sps_out_params, 0, sizeof(sps_out_params));

	if (!usb_pipe_idx || !clnt_hdl) {
		pr_err("connect_pipe_ipa :: null arguments\n");
		ret = -EINVAL;
		goto bail;
	}

	ret = ipa_get_a2_mux_pipe_info(pipe_dir, &pipe_connection);
	if (ret) {
		pr_err("ipa_get_a2_mux_pipe_info failed\n");
		goto bail;
	}

	if (pipe_dir == A2_TO_IPA) {
		a2_phy_addr = pipe_connection.src_phy_addr;
		ipa_in_params.client = IPA_CLIENT_A2_TETHERED_PROD;
		ipa_in_params.ipa_ep_cfg.mode.mode = IPA_DMA;
		ipa_in_params.ipa_ep_cfg.mode.dst = IPA_CLIENT_USB_CONS;
		pr_err("-*&- pipe_connection->src_pipe_index = %d\n",
				pipe_connection.src_pipe_index);
		ipa_in_params.client_ep_idx = pipe_connection.src_pipe_index;
	} else {
		a2_phy_addr = pipe_connection.dst_phy_addr;
		ipa_in_params.client = IPA_CLIENT_A2_TETHERED_CONS;
		ipa_in_params.client_ep_idx = pipe_connection.dst_pipe_index;
	}

	ret = sps_phy2h(a2_phy_addr, &a2_handle);
	if (ret) {
		pr_err("%s: sps_phy2h failed (A2 BAM) %d\n", __func__, ret);
		goto bail;
	}

	ipa_in_params.client_bam_hdl = a2_handle;
	ipa_in_params.desc_fifo_sz = pipe_connection.desc_fifo_size;
	ipa_in_params.data_fifo_sz = pipe_connection.data_fifo_size;

	if (pipe_connection.mem_type == IPA_SPS_PIPE_MEM) {
		pr_debug("%s: A2 BAM using SPS pipe memory\n", __func__);
		ret = sps_setup_bam2bam_fifo(&data_mem_buf[pipe_dir],
				pipe_connection.data_fifo_base_offset,
				pipe_connection.data_fifo_size, 1);
		if (ret) {
			pr_err("%s: data fifo setup failure %d\n",
					__func__, ret);
			goto bail;
		}

		ret = sps_setup_bam2bam_fifo(&desc_mem_buf[pipe_dir],
				pipe_connection.desc_fifo_base_offset,
				pipe_connection.desc_fifo_size, 1);
		if (ret) {
			pr_err("%s: desc. fifo setup failure %d\n",
					__func__, ret);
			goto bail;
		}

		ipa_in_params.data = data_mem_buf[pipe_dir];
		ipa_in_params.desc = desc_mem_buf[pipe_dir];
	}

	ret = a2_ipa_connect_pipe(&ipa_in_params,
			&sps_out_params,
			clnt_hdl);
	if (ret) {
		pr_err("-**- USB-IPA info: ipa_connect failed\n");
		pr_err("%s: usb_ipa_connect_pipe failed\n", __func__);
		goto bail;
	}

	pipe = sps_alloc_endpoint();
	if (pipe == NULL) {
		pr_err("%s: sps_alloc_endpoint failed\n", __func__);
		ret = -ENOMEM;
		goto a2_ipa_connect_pipe_failed;
	}

	ret = sps_get_config(pipe, &connection);
	if (ret) {
		pr_err("%s: tx get config failed %d\n", __func__, ret);
		goto get_config_failed;
	}

	if (pipe_dir == A2_TO_IPA) {
		connection.mode = SPS_MODE_SRC;
		*usb_pipe_idx = connection.src_pipe_index;
		connection.source = a2_handle;
		connection.destination = sps_out_params.ipa_bam_hdl;
		connection.src_pipe_index = pipe_connection.src_pipe_index;
		connection.dest_pipe_index = sps_out_params.ipa_ep_idx;
	} else {
		connection.mode = SPS_MODE_DEST;
		*usb_pipe_idx = connection.dest_pipe_index;
		connection.source = sps_out_params.ipa_bam_hdl;
		connection.destination = a2_handle;
		connection.src_pipe_index = sps_out_params.ipa_ep_idx;
		connection.dest_pipe_index = pipe_connection.dst_pipe_index;
	}

	connection.event_thresh = 16;
	connection.data = sps_out_params.data;
	connection.desc = sps_out_params.desc;

	ret = sps_connect(pipe, &connection);
	if (ret < 0) {
		pr_err("%s: tx connect error %d\n", __func__, ret);
		goto error;
	}

	ret = 0;
	goto bail;
error:
	sps_disconnect(pipe);
get_config_failed:
	sps_free_endpoint(pipe);
a2_ipa_connect_pipe_failed:
	(void) ipa_disconnect(*clnt_hdl);
bail:
	return ret;
}

static int a2_ipa_connect_pipe(struct ipa_connect_params *in_params,
		struct ipa_sps_params *out_params, u32 *clnt_hdl)
{
	return ipa_connect(in_params, out_params, clnt_hdl);
}

