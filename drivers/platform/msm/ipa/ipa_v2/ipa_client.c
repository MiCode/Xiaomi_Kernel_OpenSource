/* Copyright (c) 2012-2016, The Linux Foundation. All rights reserved.
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
#include <asm/barrier.h>
#include <linux/delay.h>
#include <linux/device.h>
#include "ipa_i.h"

/*
 * These values were determined empirically and shows good E2E bi-
 * directional throughputs
 */
#define IPA_HOLB_TMR_EN 0x1
#define IPA_HOLB_TMR_DIS 0x0
#define IPA_HOLB_TMR_DEFAULT_VAL 0x1ff

#define IPA_PKT_FLUSH_TO_US 100

int ipa_enable_data_path(u32 clnt_hdl)
{
	struct ipa_ep_context *ep = &ipa_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	int res = 0;

	IPADBG("Enabling data path\n");
	/* From IPA 2.0, disable HOLB */
	if ((ipa_ctx->ipa_hw_type >= IPA_HW_v2_0) &&
		IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0, sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_DIS;
		holb_cfg.tmr_val = 0;
		res = ipa2_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	/* Enable the pipe */
	if (IPA_CLIENT_IS_CONS(ep->client) &&
	    (ep->keep_ipa_awake ||
	     ipa_ctx->resume_on_connect[ep->client] ||
	     !ipa_should_pipe_be_suspended(ep->client))) {
		memset(&ep_cfg_ctrl, 0, sizeof(ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = false;
		ipa2_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}

	return res;
}

int ipa_disable_data_path(u32 clnt_hdl)
{
	struct ipa_ep_context *ep = &ipa_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	u32 aggr_init;
	int res = 0;

	IPADBG("Disabling data path\n");
	/* On IPA 2.0, enable HOLB in order to prevent IPA from stalling */
	if ((ipa_ctx->ipa_hw_type >= IPA_HW_v2_0) &&
		IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0, sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_EN;
		holb_cfg.tmr_val = 0;
		res = ipa2_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	/* Suspend the pipe */
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = true;
		ipa2_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}

	udelay(IPA_PKT_FLUSH_TO_US);
	aggr_init = ipa_read_reg(ipa_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v2_0(clnt_hdl));
	if (((aggr_init & IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK) >>
	    IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT) == IPA_ENABLE_AGGR) {
		res = ipa_tag_aggr_force_close(clnt_hdl);
		if (res) {
			IPAERR("tag process timeout, client:%d err:%d\n",
				clnt_hdl, res);
			BUG();
		}
	}

	return res;
}

static int ipa2_smmu_map_peer_bam(unsigned long dev)
{
	phys_addr_t base;
	u32 size;
	struct iommu_domain *smmu_domain;
	struct ipa_smmu_cb_ctx *cb = ipa2_get_smmu_ctx();

	if (!ipa_ctx->smmu_s1_bypass) {
		if (ipa_ctx->peer_bam_map_cnt == 0) {
			if (sps_get_bam_addr(dev, &base, &size)) {
				IPAERR("Fail to get addr\n");
				return -EINVAL;
			}
			smmu_domain = ipa2_get_smmu_domain();
			if (smmu_domain != NULL) {
				if (ipa_iommu_map(smmu_domain,
					cb->va_end,
					rounddown(base, PAGE_SIZE),
					roundup(size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE),
					IOMMU_READ | IOMMU_WRITE |
					IOMMU_DEVICE)) {
					IPAERR("Fail to ipa_iommu_map\n");
					return -EINVAL;
				}
			}

			ipa_ctx->peer_bam_iova = cb->va_end;
			ipa_ctx->peer_bam_pa = base;
			ipa_ctx->peer_bam_map_size = size;
			ipa_ctx->peer_bam_dev = dev;

			IPADBG("Peer bam %lu mapped\n", dev);
		} else {
			WARN_ON(dev != ipa_ctx->peer_bam_dev);
		}

		ipa_ctx->peer_bam_map_cnt++;
	}

	return 0;
}

static int ipa_connect_configure_sps(const struct ipa_connect_params *in,
				     struct ipa_ep_context *ep, int ipa_ep_idx)
{
	int result = -EFAULT;

	/* Default Config */
	ep->ep_hdl = sps_alloc_endpoint();

	if (ipa2_smmu_map_peer_bam(in->client_bam_hdl)) {
		IPAERR("fail to iommu map peer BAM.\n");
		return -EFAULT;
	}

	if (ep->ep_hdl == NULL) {
		IPAERR("SPS EP alloc failed EP.\n");
		return -EFAULT;
	}

	result = sps_get_config(ep->ep_hdl,
		&ep->connect);
	if (result) {
		IPAERR("fail to get config.\n");
		return -EFAULT;
	}

	/* Specific Config */
	if (IPA_CLIENT_IS_CONS(in->client)) {
		ep->connect.mode = SPS_MODE_SRC;
		ep->connect.destination =
			in->client_bam_hdl;
		ep->connect.dest_iova = ipa_ctx->peer_bam_iova;
		ep->connect.source = ipa_ctx->bam_handle;
		ep->connect.dest_pipe_index =
			in->client_ep_idx;
		ep->connect.src_pipe_index = ipa_ep_idx;
	} else {
		ep->connect.mode = SPS_MODE_DEST;
		ep->connect.source = in->client_bam_hdl;
		ep->connect.source_iova = ipa_ctx->peer_bam_iova;
		ep->connect.destination = ipa_ctx->bam_handle;
		ep->connect.src_pipe_index = in->client_ep_idx;
		ep->connect.dest_pipe_index = ipa_ep_idx;
	}

	return 0;
}

static int ipa_connect_allocate_fifo(const struct ipa_connect_params *in,
				     struct sps_mem_buffer *mem_buff_ptr,
				     bool *fifo_in_pipe_mem_ptr,
				     u32 *fifo_pipe_mem_ofst_ptr,
				     u32 fifo_size, int ipa_ep_idx)
{
	dma_addr_t dma_addr;
	u32 ofst;
	int result = -EFAULT;
	struct iommu_domain *smmu_domain;

	mem_buff_ptr->size = fifo_size;
	if (in->pipe_mem_preferred) {
		if (ipa_pipe_mem_alloc(&ofst, fifo_size)) {
			IPAERR("FIFO pipe mem alloc fail ep %u\n",
				ipa_ep_idx);
			mem_buff_ptr->base =
				dma_alloc_coherent(ipa_ctx->pdev,
				mem_buff_ptr->size,
				&dma_addr, GFP_KERNEL);
		} else {
			memset(mem_buff_ptr, 0, sizeof(struct sps_mem_buffer));
			result = sps_setup_bam2bam_fifo(mem_buff_ptr, ofst,
				fifo_size, 1);
			WARN_ON(result);
			*fifo_in_pipe_mem_ptr = 1;
			dma_addr = mem_buff_ptr->phys_base;
			*fifo_pipe_mem_ofst_ptr = ofst;
		}
	} else {
		mem_buff_ptr->base =
			dma_alloc_coherent(ipa_ctx->pdev, mem_buff_ptr->size,
			&dma_addr, GFP_KERNEL);
	}
	if (ipa_ctx->smmu_s1_bypass) {
		mem_buff_ptr->phys_base = dma_addr;
	} else {
		mem_buff_ptr->iova = dma_addr;
		smmu_domain = ipa2_get_smmu_domain();
		if (smmu_domain != NULL) {
			mem_buff_ptr->phys_base =
				iommu_iova_to_phys(smmu_domain, dma_addr);
		}
	}
	if (mem_buff_ptr->base == NULL) {
		IPAERR("fail to get DMA memory.\n");
		return -EFAULT;
	}

	return 0;
}

/**
 * ipa2_connect() - low-level IPA client connect
 * @in:	[in] input parameters from client
 * @sps:	[out] sps output from IPA needed by client for sps_connect
 * @clnt_hdl:	[out] opaque client handle assigned by IPA to client
 *
 * Should be called by the driver of the peripheral that wants to connect to
 * IPA in BAM-BAM mode. these peripherals are USB and HSIC. this api
 * expects caller to take responsibility to add any needed headers, routing
 * and filtering tables and rules as needed.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_connect(const struct ipa_connect_params *in,
		struct ipa_sps_params *sps, u32 *clnt_hdl)
{
	int ipa_ep_idx;
	int result = -EFAULT;
	struct ipa_ep_context *ep;
	struct ipa_ep_cfg_status ep_status;
	unsigned long base;
	struct iommu_domain *smmu_domain;

	if (unlikely(!ipa_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	IPADBG("connecting client\n");

	if (in == NULL || sps == NULL || clnt_hdl == NULL ||
	    in->client >= IPA_CLIENT_MAX ||
	    in->desc_fifo_sz == 0 || in->data_fifo_sz == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ipa_ep_idx = ipa2_get_ep_mapping(in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("fail to alloc EP.\n");
		goto fail;
	}

	ep = &ipa_ctx->ep[ipa_ep_idx];

	if (ep->valid) {
		IPAERR("EP already allocated.\n");
		goto fail;
	}

	memset(&ipa_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa_ep_context));
	IPA_ACTIVE_CLIENTS_INC_EP(in->client);


	ep->skip_ep_cfg = in->skip_ep_cfg;
	ep->valid = 1;
	ep->client = in->client;
	ep->client_notify = in->notify;
	ep->priv = in->priv;
	ep->keep_ipa_awake = in->keep_ipa_awake;

	/* Notify uc to start monitoring holb on USB BAM Producer pipe. */
	if (IPA_CLIENT_IS_USB_CONS(in->client)) {
		ipa_uc_monitor_holb(in->client, true);
		IPADBG("Enabling holb monitor for client:%d", in->client);
	}

	result = ipa_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n", result,
				ipa_ep_idx);
		goto ipa_cfg_ep_fail;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa2_cfg_ep(ipa_ep_idx, &in->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto ipa_cfg_ep_fail;
		}
		/* Setting EP status 0 */
		memset(&ep_status, 0, sizeof(ep_status));
		if (ipa2_cfg_ep_status(ipa_ep_idx, &ep_status)) {
			IPAERR("fail to configure status of EP.\n");
			goto ipa_cfg_ep_fail;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("Skipping endpoint configuration.\n");
	}

	result = ipa_connect_configure_sps(in, ep, ipa_ep_idx);
	if (result) {
		IPAERR("fail to configure SPS.\n");
		goto ipa_cfg_ep_fail;
	}

	if (!ipa_ctx->smmu_s1_bypass &&
			(in->desc.base == NULL ||
			 in->data.base == NULL)) {
		IPAERR(" allocate FIFOs data_fifo=0x%p desc_fifo=0x%p.\n",
				in->data.base, in->desc.base);
		goto desc_mem_alloc_fail;
	}

	if (in->desc.base == NULL) {
		result = ipa_connect_allocate_fifo(in, &ep->connect.desc,
						  &ep->desc_fifo_in_pipe_mem,
						  &ep->desc_fifo_pipe_mem_ofst,
						  in->desc_fifo_sz, ipa_ep_idx);
		if (result) {
			IPAERR("fail to allocate DESC FIFO.\n");
			goto desc_mem_alloc_fail;
		}
	} else {
		IPADBG("client allocated DESC FIFO\n");
		ep->connect.desc = in->desc;
		ep->desc_fifo_client_allocated = 1;
	}
	IPADBG("Descriptor FIFO pa=%pa, size=%d\n", &ep->connect.desc.phys_base,
	       ep->connect.desc.size);

	if (in->data.base == NULL) {
		result = ipa_connect_allocate_fifo(in, &ep->connect.data,
						&ep->data_fifo_in_pipe_mem,
						&ep->data_fifo_pipe_mem_ofst,
						in->data_fifo_sz, ipa_ep_idx);
		if (result) {
			IPAERR("fail to allocate DATA FIFO.\n");
			goto data_mem_alloc_fail;
		}
	} else {
		IPADBG("client allocated DATA FIFO\n");
		ep->connect.data = in->data;
		ep->data_fifo_client_allocated = 1;
	}
	IPADBG("Data FIFO pa=%pa, size=%d\n", &ep->connect.data.phys_base,
	       ep->connect.data.size);

	if (!ipa_ctx->smmu_s1_bypass) {
		ep->connect.data.iova = ep->connect.data.phys_base;
		base = ep->connect.data.iova;
		smmu_domain = ipa2_get_smmu_domain();
		if (smmu_domain != NULL) {
			if (ipa_iommu_map(smmu_domain,
				rounddown(base, PAGE_SIZE),
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.data.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE)) {
				IPAERR("Fail to ipa_iommu_map data FIFO\n");
				goto iommu_map_data_fail;
			}
		}
		ep->connect.desc.iova = ep->connect.desc.phys_base;
		base = ep->connect.desc.iova;
		if (smmu_domain != NULL) {
			if (ipa_iommu_map(smmu_domain,
				rounddown(base, PAGE_SIZE),
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.desc.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE)) {
				IPAERR("Fail to ipa_iommu_map desc FIFO\n");
				goto iommu_map_desc_fail;
			}
		}
	}

	if ((ipa_ctx->ipa_hw_type == IPA_HW_v2_0 ||
		ipa_ctx->ipa_hw_type == IPA_HW_v2_5 ||
		ipa_ctx->ipa_hw_type == IPA_HW_v2_6L) &&
		IPA_CLIENT_IS_USB_CONS(in->client))
		ep->connect.event_thresh = IPA_USB_EVENT_THRESHOLD;
	else
		ep->connect.event_thresh = IPA_EVENT_THRESHOLD;
	ep->connect.options = SPS_O_AUTO_ENABLE;    /* BAM-to-BAM */

	result = ipa_sps_connect_safe(ep->ep_hdl, &ep->connect, in->client);
	if (result) {
		IPAERR("sps_connect fails.\n");
		goto sps_connect_fail;
	}

	sps->ipa_bam_hdl = ipa_ctx->bam_handle;
	sps->ipa_ep_idx = ipa_ep_idx;
	*clnt_hdl = ipa_ep_idx;
	memcpy(&sps->desc, &ep->connect.desc, sizeof(struct sps_mem_buffer));
	memcpy(&sps->data, &ep->connect.data, sizeof(struct sps_mem_buffer));

	ipa_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(in->client))
		ipa_install_dflt_flt_rules(ipa_ep_idx);

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(in->client);

	IPADBG("client %d (ep: %d) connected\n", in->client, ipa_ep_idx);

	return 0;

sps_connect_fail:
	if (!ipa_ctx->smmu_s1_bypass) {
		base = ep->connect.desc.iova;
		smmu_domain = ipa2_get_smmu_domain();
		if (smmu_domain != NULL) {
			iommu_unmap(smmu_domain,
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.desc.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE));
		}
	}
iommu_map_desc_fail:
	if (!ipa_ctx->smmu_s1_bypass) {
		base = ep->connect.data.iova;
		smmu_domain = ipa2_get_smmu_domain();
		if (smmu_domain != NULL) {
			iommu_unmap(smmu_domain,
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.data.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE));
		}
	}
iommu_map_data_fail:
	if (!ep->data_fifo_client_allocated) {
		if (!ep->data_fifo_in_pipe_mem)
			dma_free_coherent(ipa_ctx->pdev,
				  ep->connect.data.size,
				  ep->connect.data.base,
				  ep->connect.data.phys_base);
		else
			ipa_pipe_mem_free(ep->data_fifo_pipe_mem_ofst,
				  ep->connect.data.size);
	}
data_mem_alloc_fail:
	if (!ep->desc_fifo_client_allocated) {
		if (!ep->desc_fifo_in_pipe_mem)
			dma_free_coherent(ipa_ctx->pdev,
				  ep->connect.desc.size,
				  ep->connect.desc.base,
				  ep->connect.desc.phys_base);
		else
			ipa_pipe_mem_free(ep->desc_fifo_pipe_mem_ofst,
				  ep->connect.desc.size);
	}
desc_mem_alloc_fail:
	sps_free_endpoint(ep->ep_hdl);
ipa_cfg_ep_fail:
	memset(&ipa_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa_ep_context));
	IPA_ACTIVE_CLIENTS_DEC_EP(in->client);
fail:
	return result;
}

static int ipa2_smmu_unmap_peer_bam(unsigned long dev)
{
	size_t len;
	struct iommu_domain *smmu_domain;
	struct ipa_smmu_cb_ctx *cb = ipa2_get_smmu_ctx();

	if (!ipa_ctx->smmu_s1_bypass) {
		WARN_ON(dev != ipa_ctx->peer_bam_dev);
		ipa_ctx->peer_bam_map_cnt--;
		if (ipa_ctx->peer_bam_map_cnt == 0) {
			len = roundup(ipa_ctx->peer_bam_map_size +
					ipa_ctx->peer_bam_pa -
					rounddown(ipa_ctx->peer_bam_pa,
						PAGE_SIZE), PAGE_SIZE);
			smmu_domain = ipa2_get_smmu_domain();
			if (smmu_domain != NULL) {
				if (iommu_unmap(smmu_domain,
					cb->va_end, len) != len) {
					IPAERR("Fail to iommu_unmap\n");
					return -EINVAL;
				}
				IPADBG("Peer bam %lu unmapped\n", dev);
			}
		}
	}

	return 0;
}

/**
 * ipa2_disconnect() - low-level IPA client disconnect
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Should be called by the driver of the peripheral that wants to disconnect
 * from IPA in BAM-BAM mode. this api expects caller to take responsibility to
 * free any needed headers, routing and filtering tables and rules as needed.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_disconnect(u32 clnt_hdl)
{
	int result;
	struct ipa_ep_context *ep;
	unsigned long peer_bam;
	unsigned long base;
	struct iommu_domain *smmu_domain;
	struct ipa_disable_force_clear_datapath_req_msg_v01 req = {0};
	int res;
	enum ipa_client_type client_type;

	if (unlikely(!ipa_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (clnt_hdl >= ipa_ctx->ipa_num_pipes ||
		ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[clnt_hdl];
	client_type = ipa2_get_client_mapping(clnt_hdl);
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(client_type);

	/* For USB 2.0 controller, first the ep will be disabled.
	 * so this sequence is not needed again when disconnecting the pipe.
	 */
	if (!ep->ep_disabled) {
		/* Set Disconnect in Progress flag. */
		spin_lock(&ipa_ctx->disconnect_lock);
		ep->disconnect_in_progress = true;
		spin_unlock(&ipa_ctx->disconnect_lock);

		/* Notify uc to stop monitoring holb on USB BAM
		 * Producer pipe.
		 */
		if (IPA_CLIENT_IS_USB_CONS(ep->client)) {
			ipa_uc_monitor_holb(ep->client, false);
			IPADBG("Disabling holb monitor for client: %d\n",
				ep->client);
		}

		result = ipa_disable_data_path(clnt_hdl);
		if (result) {
			IPAERR("disable data path failed res=%d clnt=%d.\n",
				result, clnt_hdl);
			return -EPERM;
		}
	}

	result = sps_disconnect(ep->ep_hdl);
	if (result) {
		IPAERR("SPS disconnect failed.\n");
		return -EPERM;
	}

	if (IPA_CLIENT_IS_CONS(ep->client))
		peer_bam = ep->connect.destination;
	else
		peer_bam = ep->connect.source;

	if (ipa2_smmu_unmap_peer_bam(peer_bam)) {
		IPAERR("fail to iommu unmap peer BAM.\n");
		return -EPERM;
	}

	if (!ep->desc_fifo_client_allocated &&
	     ep->connect.desc.base) {
		if (!ep->desc_fifo_in_pipe_mem)
			dma_free_coherent(ipa_ctx->pdev,
					  ep->connect.desc.size,
					  ep->connect.desc.base,
					  ep->connect.desc.phys_base);
		else
			ipa_pipe_mem_free(ep->desc_fifo_pipe_mem_ofst,
					  ep->connect.desc.size);
	}

	if (!ep->data_fifo_client_allocated &&
	     ep->connect.data.base) {
		if (!ep->data_fifo_in_pipe_mem)
			dma_free_coherent(ipa_ctx->pdev,
					  ep->connect.data.size,
					  ep->connect.data.base,
					  ep->connect.data.phys_base);
		else
			ipa_pipe_mem_free(ep->data_fifo_pipe_mem_ofst,
					  ep->connect.data.size);
	}

	if (!ipa_ctx->smmu_s1_bypass) {
		base = ep->connect.desc.iova;
		smmu_domain = ipa2_get_smmu_domain();
		if (smmu_domain != NULL) {
			iommu_unmap(smmu_domain,
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.desc.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE));
		}
	}

	if (!ipa_ctx->smmu_s1_bypass) {
		base = ep->connect.data.iova;
		smmu_domain = ipa2_get_smmu_domain();
		if (smmu_domain != NULL) {
			iommu_unmap(smmu_domain,
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.data.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE));
		}
	}

	result = sps_free_endpoint(ep->ep_hdl);
	if (result) {
		IPAERR("SPS de-alloc EP failed.\n");
		return -EPERM;
	}

	ipa_delete_dflt_flt_rules(clnt_hdl);

	/* If APPS flow control is not enabled, send a message to modem to
	 * enable flow control honoring.
	 */
	if (!ipa_ctx->tethered_flow_control && ep->qmi_request_sent) {
		/* Send a message to modem to disable flow control honoring. */
		req.request_id = clnt_hdl;
		res = qmi_disable_force_clear_datapath_send(&req);
		if (res) {
			IPADBG("disable_force_clear_datapath failed %d\n",
				res);
		}
	}

	spin_lock(&ipa_ctx->disconnect_lock);
	memset(&ipa_ctx->ep[clnt_hdl], 0, sizeof(struct ipa_ep_context));
	spin_unlock(&ipa_ctx->disconnect_lock);

	IPA_ACTIVE_CLIENTS_DEC_EP(client_type);

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}

/**
* ipa2_reset_endpoint() - reset an endpoint from BAM perspective
* @clnt_hdl: [in] IPA client handle
*
* Returns:	0 on success, negative on failure
*
* Note:	Should not be called from atomic context
*/
int ipa2_reset_endpoint(u32 clnt_hdl)
{
	int res;
	struct ipa_ep_context *ep;

	if (unlikely(!ipa_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (clnt_hdl >= ipa_ctx->ipa_num_pipes) {
		IPAERR("Bad parameters.\n");
		return -EFAULT;
	}
	ep = &ipa_ctx->ep[clnt_hdl];

	IPA_ACTIVE_CLIENTS_INC_EP(ipa2_get_client_mapping(clnt_hdl));
	res = sps_disconnect(ep->ep_hdl);
	if (res) {
		IPAERR("sps_disconnect() failed, res=%d.\n", res);
		goto bail;
	} else {
		res = ipa_sps_connect_safe(ep->ep_hdl, &ep->connect,
			ep->client);
		if (res) {
			IPAERR("sps_connect() failed, res=%d.\n", res);
			goto bail;
		}
	}

bail:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa2_get_client_mapping(clnt_hdl));

	return res;
}

/**
 * ipa2_clear_endpoint_delay() - Remove ep delay set on the IPA pipe before
 * client disconnect.
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Should be called by the driver of the peripheral that wants to remove
 * ep delay on IPA consumer ipe before disconnect in BAM-BAM mode. this api
 * expects caller to take responsibility to free any needed headers, routing
 * and filtering tables and rules as needed.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_clear_endpoint_delay(u32 clnt_hdl)
{
	struct ipa_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_ctrl = {0};
	struct ipa_enable_force_clear_datapath_req_msg_v01 req = {0};
	int res;

	if (unlikely(!ipa_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (clnt_hdl >= ipa_ctx->ipa_num_pipes ||
		ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[clnt_hdl];

	if (!ipa_ctx->tethered_flow_control) {
		IPADBG("APPS flow control is not enabled\n");
		/* Send a message to modem to disable flow control honoring. */
		req.request_id = clnt_hdl;
		req.source_pipe_bitmask = 1 << clnt_hdl;
		res = qmi_enable_force_clear_datapath_send(&req);
		if (res) {
			IPADBG("enable_force_clear_datapath failed %d\n",
				res);
		}
		ep->qmi_request_sent = true;
	}

	IPA_ACTIVE_CLIENTS_INC_EP(ipa2_get_client_mapping(clnt_hdl));
	/* Set disconnect in progress flag so further flow control events are
	 * not honored.
	 */
	spin_lock(&ipa_ctx->disconnect_lock);
	ep->disconnect_in_progress = true;
	spin_unlock(&ipa_ctx->disconnect_lock);

	/* If flow is disabled at this point, restore the ep state.*/
	ep_ctrl.ipa_ep_delay = false;
	ep_ctrl.ipa_ep_suspend = false;
	ipa2_cfg_ep_ctrl(clnt_hdl, &ep_ctrl);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa2_get_client_mapping(clnt_hdl));

	IPADBG("client (ep: %d) removed ep delay\n", clnt_hdl);

	return 0;
}

/**
 * ipa2_disable_endpoint() - low-level IPA client disable endpoint
 * @clnt_hdl:	[in] opaque client handle assigned by IPA to client
 *
 * Should be called by the driver of the peripheral that wants to
 * disable the pipe from IPA in BAM-BAM mode.
 *
 * Returns:	0 on success, negative on failure
 *
 * Note:	Should not be called from atomic context
 */
int ipa2_disable_endpoint(u32 clnt_hdl)
{
	int result;
	struct ipa_ep_context *ep;
	enum ipa_client_type client_type;
	unsigned long bam;

	if (unlikely(!ipa_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (clnt_hdl >= ipa_ctx->ipa_num_pipes ||
		ipa_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa_ctx->ep[clnt_hdl];
	client_type = ipa2_get_client_mapping(clnt_hdl);
	IPA_ACTIVE_CLIENTS_INC_EP(client_type);

	/* Set Disconnect in Progress flag. */
	spin_lock(&ipa_ctx->disconnect_lock);
	ep->disconnect_in_progress = true;
	spin_unlock(&ipa_ctx->disconnect_lock);

	/* Notify uc to stop monitoring holb on USB BAM Producer pipe. */
	if (IPA_CLIENT_IS_USB_CONS(ep->client)) {
		ipa_uc_monitor_holb(ep->client, false);
		IPADBG("Disabling holb monitor for client: %d\n", ep->client);
	}

	result = ipa_disable_data_path(clnt_hdl);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
				clnt_hdl);
		goto fail;
	}

	if (IPA_CLIENT_IS_CONS(ep->client))
		bam = ep->connect.source;
	else
		bam = ep->connect.destination;

	result = sps_pipe_reset(bam, clnt_hdl);
	if (result) {
		IPAERR("SPS pipe reset failed.\n");
		goto fail;
	}

	ep->ep_disabled = true;

	IPA_ACTIVE_CLIENTS_DEC_EP(client_type);

	IPADBG("client (ep: %d) disabled\n", clnt_hdl);

	return 0;

fail:
	IPA_ACTIVE_CLIENTS_DEC_EP(client_type);
	return -EPERM;
}


/**
 * ipa_sps_connect_safe() - connect endpoint from BAM prespective
 * @h: [in] sps pipe handle
 * @connect: [in] sps connect parameters
 * @ipa_client: [in] ipa client handle representing the pipe
 *
 * This function connects a BAM pipe using SPS driver sps_connect() API
 * and by requesting uC interface to reset the pipe, avoids an IPA HW
 * limitation that does not allow resetting a BAM pipe during traffic in
 * IPA TX command queue.
 *
 * Returns:	0 on success, negative on failure
 */
int ipa_sps_connect_safe(struct sps_pipe *h, struct sps_connect *connect,
			 enum ipa_client_type ipa_client)
{
	int res;

	if (ipa_ctx->ipa_hw_type > IPA_HW_v2_5 || ipa_ctx->skip_uc_pipe_reset) {
		IPADBG("uC pipe reset is not required\n");
	} else {
		res = ipa_uc_reset_pipe(ipa_client);
		if (res)
			return res;
	}
	return sps_connect(h, connect);
}
EXPORT_SYMBOL(ipa_sps_connect_safe);
