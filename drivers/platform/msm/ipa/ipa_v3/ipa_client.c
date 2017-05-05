/* Copyright (c) 2012-2017, The Linux Foundation. All rights reserved.
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
#include "linux/msm_gsi.h"

/*
 * These values were determined empirically and shows good E2E bi-
 * directional throughputs
 */
#define IPA_HOLB_TMR_EN 0x1
#define IPA_HOLB_TMR_DIS 0x0
#define IPA_HOLB_TMR_DEFAULT_VAL 0x1ff
#define IPA_POLL_AGGR_STATE_RETRIES_NUM 3
#define IPA_POLL_AGGR_STATE_SLEEP_MSEC 1

#define IPA_PKT_FLUSH_TO_US 100

#define IPA_POLL_FOR_EMPTINESS_NUM 50
#define IPA_POLL_FOR_EMPTINESS_SLEEP_USEC 20
#define IPA_CHANNEL_STOP_IN_PROC_TO_MSEC 5
#define IPA_CHANNEL_STOP_IN_PROC_SLEEP_USEC 200

/* xfer_rsc_idx should be 7 bits */
#define IPA_XFER_RSC_IDX_MAX 127

static int ipa3_is_xdci_channel_empty(struct ipa3_ep_context *ep,
	bool *is_empty);

int ipa3_enable_data_path(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep = &ipa3_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	int res = 0;
	struct ipahal_reg_endp_init_rsrc_grp rsrc_grp;

	/* Assign the resource group for pipe */
	memset(&rsrc_grp, 0, sizeof(rsrc_grp));
	rsrc_grp.rsrc_grp = ipa_get_ep_group(ep->client);
	if (rsrc_grp.rsrc_grp == -1) {
		IPAERR("invalid group for client %d\n", ep->client);
		WARN_ON(1);
		return -EFAULT;
	}

	IPADBG("Setting group %d for pipe %d\n",
		rsrc_grp.rsrc_grp, clnt_hdl);
	ipahal_write_reg_n_fields(IPA_ENDP_INIT_RSRC_GRP_n, clnt_hdl,
		&rsrc_grp);

	IPADBG("Enabling data path\n");
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0 , sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_DIS;
		holb_cfg.tmr_val = 0;
		res = ipa3_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	/* Enable the pipe */
	if (IPA_CLIENT_IS_CONS(ep->client) &&
	    (ep->keep_ipa_awake ||
	     ipa3_ctx->resume_on_connect[ep->client] ||
	     !ipa3_should_pipe_be_suspended(ep->client))) {
		memset(&ep_cfg_ctrl, 0 , sizeof(ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = false;
		res = ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}

	return res;
}

int ipa3_disable_data_path(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep = &ipa3_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	struct ipa_ep_cfg_aggr ep_aggr;
	int res = 0;

	IPADBG("Disabling data path\n");
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		memset(&holb_cfg, 0, sizeof(holb_cfg));
		holb_cfg.en = IPA_HOLB_TMR_EN;
		holb_cfg.tmr_val = 0;
		res = ipa3_cfg_ep_holb(clnt_hdl, &holb_cfg);
	}

	/* Suspend the pipe */
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		/*
		 * for RG10 workaround uC needs to be loaded before pipe can
		 * be suspended in this case.
		 */
		if (ipa3_ctx->apply_rg10_wa && ipa3_uc_state_check()) {
			IPADBG("uC is not loaded yet, waiting...\n");
			res = wait_for_completion_timeout(
				&ipa3_ctx->uc_loaded_completion_obj, 60 * HZ);
			if (res == 0)
				IPADBG("timeout waiting for uC to load\n");
		}

		memset(&ep_cfg_ctrl, 0 , sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = true;
		res = ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}

	udelay(IPA_PKT_FLUSH_TO_US);
	ipahal_read_reg_n_fields(IPA_ENDP_INIT_AGGR_n, clnt_hdl, &ep_aggr);
	if (ep_aggr.aggr_en) {
		res = ipa3_tag_aggr_force_close(clnt_hdl);
		if (res) {
			IPAERR("tag process timeout, client:%d err:%d\n",
				   clnt_hdl, res);
			BUG();
		}
	}

	return res;
}

static int ipa3_smmu_map_peer_bam(unsigned long dev)
{
	phys_addr_t base;
	u32 size;
	struct iommu_domain *smmu_domain;
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx();

	if (!ipa3_ctx->smmu_s1_bypass) {
		if (ipa3_ctx->peer_bam_map_cnt == 0) {
			if (sps_get_bam_addr(dev, &base, &size)) {
				IPAERR("Fail to get addr\n");
				return -EINVAL;
			}
			smmu_domain = ipa3_get_smmu_domain();
			if (smmu_domain != NULL) {
				if (ipa3_iommu_map(smmu_domain,
					cb->va_end,
					rounddown(base, PAGE_SIZE),
					roundup(size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE),
					IOMMU_READ | IOMMU_WRITE |
					IOMMU_DEVICE)) {
					IPAERR("Fail to ipa3_iommu_map\n");
					return -EINVAL;
				}
			}

			ipa3_ctx->peer_bam_iova = cb->va_end;
			ipa3_ctx->peer_bam_pa = base;
			ipa3_ctx->peer_bam_map_size = size;
			ipa3_ctx->peer_bam_dev = dev;

			IPADBG("Peer bam %lu mapped\n", dev);
		} else {
			WARN_ON(dev != ipa3_ctx->peer_bam_dev);
		}

		ipa3_ctx->peer_bam_map_cnt++;
	}

	return 0;
}

static int ipa3_connect_configure_sps(const struct ipa_connect_params *in,
				     struct ipa3_ep_context *ep, int ipa_ep_idx)
{
	int result = -EFAULT;

	/* Default Config */
	ep->ep_hdl = sps_alloc_endpoint();

	if (ipa3_smmu_map_peer_bam(in->client_bam_hdl)) {
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
		ep->connect.dest_iova = ipa3_ctx->peer_bam_iova;
		ep->connect.source = ipa3_ctx->bam_handle;
		ep->connect.dest_pipe_index =
			in->client_ep_idx;
		ep->connect.src_pipe_index = ipa_ep_idx;
	} else {
		ep->connect.mode = SPS_MODE_DEST;
		ep->connect.source = in->client_bam_hdl;
		ep->connect.source_iova = ipa3_ctx->peer_bam_iova;
		ep->connect.destination = ipa3_ctx->bam_handle;
		ep->connect.src_pipe_index = in->client_ep_idx;
		ep->connect.dest_pipe_index = ipa_ep_idx;
	}

	return 0;
}

static int ipa3_connect_allocate_fifo(const struct ipa_connect_params *in,
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
		if (ipa3_pipe_mem_alloc(&ofst, fifo_size)) {
			IPAERR("FIFO pipe mem alloc fail ep %u\n",
				ipa_ep_idx);
			mem_buff_ptr->base =
				dma_alloc_coherent(ipa3_ctx->pdev,
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
			dma_alloc_coherent(ipa3_ctx->pdev, mem_buff_ptr->size,
			&dma_addr, GFP_KERNEL);
	}
	if (ipa3_ctx->smmu_s1_bypass) {
		mem_buff_ptr->phys_base = dma_addr;
	} else {
		mem_buff_ptr->iova = dma_addr;
		smmu_domain = ipa_get_smmu_domain();
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
 * ipa3_connect() - low-level IPA client connect
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
int ipa3_connect(const struct ipa_connect_params *in,
		struct ipa_sps_params *sps,
		u32 *clnt_hdl)
{
	int ipa_ep_idx;
	int result = -EFAULT;
	struct ipa3_ep_context *ep;
	struct ipahal_reg_ep_cfg_status ep_status;
	unsigned long base;
	struct iommu_domain *smmu_domain;

	IPADBG("connecting client\n");

	if (in == NULL || sps == NULL || clnt_hdl == NULL ||
	    in->client >= IPA_CLIENT_MAX ||
	    in->desc_fifo_sz == 0 || in->data_fifo_sz == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ipa_ep_idx = ipa3_get_ep_mapping(in->client);
	if (ipa_ep_idx == -1) {
		IPAERR("fail to alloc EP.\n");
		goto fail;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->valid) {
		IPAERR("EP already allocated.\n");
		goto fail;
	}

	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
	IPA_ACTIVE_CLIENTS_INC_EP(in->client);

	ep->skip_ep_cfg = in->skip_ep_cfg;
	ep->valid = 1;
	ep->client = in->client;
	ep->client_notify = in->notify;
	ep->priv = in->priv;
	ep->keep_ipa_awake = in->keep_ipa_awake;

	result = ipa3_enable_data_path(ipa_ep_idx);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n", result,
				ipa_ep_idx);
		goto ipa_cfg_ep_fail;
	}

	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &in->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto ipa_cfg_ep_fail;
		}
		/* Setting EP status 0 */
		memset(&ep_status, 0, sizeof(ep_status));
		if (ipa3_cfg_ep_status(ipa_ep_idx, &ep_status)) {
			IPAERR("fail to configure status of EP.\n");
			goto ipa_cfg_ep_fail;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("Skipping endpoint configuration.\n");
	}

	result = ipa3_connect_configure_sps(in, ep, ipa_ep_idx);
	if (result) {
		IPAERR("fail to configure SPS.\n");
		goto ipa_cfg_ep_fail;
	}

	if (!ipa3_ctx->smmu_s1_bypass &&
			(in->desc.base == NULL ||
			 in->data.base == NULL)) {
		IPAERR(" allocate FIFOs data_fifo=0x%p desc_fifo=0x%p.\n",
				in->data.base, in->desc.base);
		goto desc_mem_alloc_fail;
	}

	if (in->desc.base == NULL) {
		result = ipa3_connect_allocate_fifo(in, &ep->connect.desc,
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
		result = ipa3_connect_allocate_fifo(in, &ep->connect.data,
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

	if (!ipa3_ctx->smmu_s1_bypass) {
		ep->connect.data.iova = ep->connect.data.phys_base;
		base = ep->connect.data.iova;
		smmu_domain = ipa_get_smmu_domain();
		if (smmu_domain != NULL) {
			if (ipa3_iommu_map(smmu_domain,
				rounddown(base, PAGE_SIZE),
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.data.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE)) {
				IPAERR("Fail to ipa3_iommu_map data FIFO\n");
				goto iommu_map_data_fail;
			}
		}
		ep->connect.desc.iova = ep->connect.desc.phys_base;
		base = ep->connect.desc.iova;
		if (smmu_domain != NULL) {
			if (ipa3_iommu_map(smmu_domain,
				rounddown(base, PAGE_SIZE),
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.desc.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE)) {
				IPAERR("Fail to ipa3_iommu_map desc FIFO\n");
				goto iommu_map_desc_fail;
			}
		}
	}

	if (IPA_CLIENT_IS_USB_CONS(in->client))
		ep->connect.event_thresh = IPA_USB_EVENT_THRESHOLD;
	else
		ep->connect.event_thresh = IPA_EVENT_THRESHOLD;
	ep->connect.options = SPS_O_AUTO_ENABLE;    /* BAM-to-BAM */

	result = ipa3_sps_connect_safe(ep->ep_hdl, &ep->connect, in->client);
	if (result) {
		IPAERR("sps_connect fails.\n");
		goto sps_connect_fail;
	}

	sps->ipa_bam_hdl = ipa3_ctx->bam_handle;
	sps->ipa_ep_idx = ipa_ep_idx;
	*clnt_hdl = ipa_ep_idx;
	memcpy(&sps->desc, &ep->connect.desc, sizeof(struct sps_mem_buffer));
	memcpy(&sps->data, &ep->connect.data, sizeof(struct sps_mem_buffer));

	ipa3_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(in->client))
		ipa3_install_dflt_flt_rules(ipa_ep_idx);

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(in->client);

	IPADBG("client %d (ep: %d) connected\n", in->client, ipa_ep_idx);

	return 0;

sps_connect_fail:
	if (!ipa3_ctx->smmu_s1_bypass) {
		base = ep->connect.desc.iova;
		smmu_domain = ipa_get_smmu_domain();
		if (smmu_domain != NULL) {
			iommu_unmap(smmu_domain,
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.desc.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE));
		}
	}
iommu_map_desc_fail:
	if (!ipa3_ctx->smmu_s1_bypass) {
		base = ep->connect.data.iova;
		smmu_domain = ipa_get_smmu_domain();
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
			dma_free_coherent(ipa3_ctx->pdev,
				  ep->connect.data.size,
				  ep->connect.data.base,
				  ep->connect.data.phys_base);
		else
			ipa3_pipe_mem_free(ep->data_fifo_pipe_mem_ofst,
				  ep->connect.data.size);
	}
data_mem_alloc_fail:
	if (!ep->desc_fifo_client_allocated) {
		if (!ep->desc_fifo_in_pipe_mem)
			dma_free_coherent(ipa3_ctx->pdev,
				  ep->connect.desc.size,
				  ep->connect.desc.base,
				  ep->connect.desc.phys_base);
		else
			ipa3_pipe_mem_free(ep->desc_fifo_pipe_mem_ofst,
				  ep->connect.desc.size);
	}
desc_mem_alloc_fail:
	sps_free_endpoint(ep->ep_hdl);
ipa_cfg_ep_fail:
	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
	IPA_ACTIVE_CLIENTS_DEC_EP(in->client);
fail:
	return result;
}

static int ipa3_smmu_unmap_peer_bam(unsigned long dev)
{
	size_t len;
	struct iommu_domain *smmu_domain;
	struct ipa_smmu_cb_ctx *cb = ipa3_get_smmu_ctx();

	if (!ipa3_ctx->smmu_s1_bypass) {
		WARN_ON(dev != ipa3_ctx->peer_bam_dev);
		ipa3_ctx->peer_bam_map_cnt--;
		if (ipa3_ctx->peer_bam_map_cnt == 0) {
			len = roundup(ipa3_ctx->peer_bam_map_size +
					ipa3_ctx->peer_bam_pa -
					rounddown(ipa3_ctx->peer_bam_pa,
						PAGE_SIZE), PAGE_SIZE);
			smmu_domain = ipa3_get_smmu_domain();
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
 * ipa3_disconnect() - low-level IPA client disconnect
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
int ipa3_disconnect(u32 clnt_hdl)
{
	int result;
	struct ipa3_ep_context *ep;
	unsigned long peer_bam;
	unsigned long base;
	struct iommu_domain *smmu_domain;
	struct ipa_disable_force_clear_datapath_req_msg_v01 req = {0};
	int res;
	enum ipa_client_type client_type;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];
	client_type = ipa3_get_client_mapping(clnt_hdl);
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(client_type);

	/* Set Disconnect in Progress flag. */
	spin_lock(&ipa3_ctx->disconnect_lock);
	ep->disconnect_in_progress = true;
	spin_unlock(&ipa3_ctx->disconnect_lock);

	result = ipa3_disable_data_path(clnt_hdl);
	if (result) {
		IPAERR("disable data path failed res=%d clnt=%d.\n", result,
				clnt_hdl);
		return -EPERM;
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

	if (ipa3_smmu_unmap_peer_bam(peer_bam)) {
		IPAERR("fail to iommu unmap peer BAM.\n");
		return -EPERM;
	}

	if (!ep->desc_fifo_client_allocated &&
	     ep->connect.desc.base) {
		if (!ep->desc_fifo_in_pipe_mem)
			dma_free_coherent(ipa3_ctx->pdev,
					  ep->connect.desc.size,
					  ep->connect.desc.base,
					  ep->connect.desc.phys_base);
		else
			ipa3_pipe_mem_free(ep->desc_fifo_pipe_mem_ofst,
					  ep->connect.desc.size);
	}

	if (!ep->data_fifo_client_allocated &&
	     ep->connect.data.base) {
		if (!ep->data_fifo_in_pipe_mem)
			dma_free_coherent(ipa3_ctx->pdev,
					  ep->connect.data.size,
					  ep->connect.data.base,
					  ep->connect.data.phys_base);
		else
			ipa3_pipe_mem_free(ep->data_fifo_pipe_mem_ofst,
					  ep->connect.data.size);
	}

	if (!ipa3_ctx->smmu_s1_bypass) {
		base = ep->connect.desc.iova;
		smmu_domain = ipa_get_smmu_domain();
		if (smmu_domain != NULL) {
			iommu_unmap(smmu_domain,
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.desc.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE));
		}
	}

	if (!ipa3_ctx->smmu_s1_bypass) {
		base = ep->connect.data.iova;
		smmu_domain = ipa_get_smmu_domain();
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

	ipa3_delete_dflt_flt_rules(clnt_hdl);

	/* If APPS flow control is not enabled, send a message to modem to
	 * enable flow control honoring.
	 */
	if (!ipa3_ctx->tethered_flow_control && ep->qmi_request_sent) {
		/* Send a message to modem to disable flow control honoring. */
		req.request_id = clnt_hdl;
		res = ipa3_qmi_disable_force_clear_datapath_send(&req);
		if (res) {
			IPADBG("disable_force_clear_datapath failed %d\n",
				res);
		}
	}

	spin_lock(&ipa3_ctx->disconnect_lock);
	memset(&ipa3_ctx->ep[clnt_hdl], 0, sizeof(struct ipa3_ep_context));
	spin_unlock(&ipa3_ctx->disconnect_lock);
	IPA_ACTIVE_CLIENTS_DEC_EP(client_type);

	IPADBG("client (ep: %d) disconnected\n", clnt_hdl);

	return 0;
}

/**
* ipa3_reset_endpoint() - reset an endpoint from BAM perspective
* @clnt_hdl: [in] IPA client handle
*
* Returns:	0 on success, negative on failure
*
* Note:	Should not be called from atomic context
*/
int ipa3_reset_endpoint(u32 clnt_hdl)
{
	int res;
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes) {
		IPAERR("Bad parameters.\n");
		return -EFAULT;
	}
	ep = &ipa3_ctx->ep[clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	res = sps_disconnect(ep->ep_hdl);
	if (res) {
		IPAERR("sps_disconnect() failed, res=%d.\n", res);
		goto bail;
	} else {
		res = ipa3_sps_connect_safe(ep->ep_hdl, &ep->connect,
			ep->client);
		if (res) {
			IPAERR("sps_connect() failed, res=%d.\n", res);
			goto bail;
		}
	}

bail:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return res;
}

/**
 * ipa3_sps_connect_safe() - connect endpoint from BAM prespective
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
int ipa3_sps_connect_safe(struct sps_pipe *h, struct sps_connect *connect,
			 enum ipa_client_type ipa_client)
{
	int res;

	if (ipa3_ctx->ipa_hw_type > IPA_HW_v2_5 ||
			ipa3_ctx->skip_uc_pipe_reset) {
		IPADBG("uC pipe reset is not required\n");
	} else {
		res = ipa3_uc_reset_pipe(ipa_client);
		if (res)
			return res;
	}
	return sps_connect(h, connect);
}

static void ipa_chan_err_cb(struct gsi_chan_err_notify *notify)
{
	if (notify) {
		switch (notify->evt_id) {
		case GSI_CHAN_INVALID_TRE_ERR:
			IPAERR("Received GSI_CHAN_INVALID_TRE_ERR\n");
			break;
		case GSI_CHAN_NON_ALLOCATED_EVT_ACCESS_ERR:
			IPAERR("Received GSI_CHAN_NON_ALLOC_EVT_ACCESS_ERR\n");
			break;
		case GSI_CHAN_OUT_OF_BUFFERS_ERR:
			IPAERR("Received GSI_CHAN_OUT_OF_BUFFERS_ERR\n");
			break;
		case GSI_CHAN_OUT_OF_RESOURCES_ERR:
			IPAERR("Received GSI_CHAN_OUT_OF_RESOURCES_ERR\n");
			break;
		case GSI_CHAN_UNSUPPORTED_INTER_EE_OP_ERR:
			IPAERR("Received GSI_CHAN_UNSUPP_INTER_EE_OP_ERR\n");
			break;
		case GSI_CHAN_HWO_1_ERR:
			IPAERR("Received GSI_CHAN_HWO_1_ERR\n");
			break;
		default:
			IPAERR("Unexpected err evt: %d\n", notify->evt_id);
		}
		BUG();
	}
}

static void ipa_xfer_cb(struct gsi_chan_xfer_notify *notify)
{
	return;
}

static int ipa3_reconfigure_channel_to_gpi(struct ipa3_ep_context *ep,
	struct gsi_chan_props *orig_chan_props,
	struct ipa_mem_buffer *chan_dma)
{
	struct gsi_chan_props chan_props;
	enum gsi_status gsi_res;
	dma_addr_t chan_dma_addr;
	int result;

	/* Set up channel properties */
	memset(&chan_props, 0, sizeof(struct gsi_chan_props));
	chan_props.prot = GSI_CHAN_PROT_GPI;
	chan_props.dir = GSI_CHAN_DIR_FROM_GSI;
	chan_props.ch_id = orig_chan_props->ch_id;
	chan_props.evt_ring_hdl = orig_chan_props->evt_ring_hdl;
	chan_props.re_size = GSI_CHAN_RE_SIZE_16B;
	chan_props.ring_len = 2 * GSI_CHAN_RE_SIZE_16B;
	chan_props.ring_base_vaddr =
		dma_alloc_coherent(ipa3_ctx->pdev, chan_props.ring_len,
		&chan_dma_addr, 0);
	chan_props.ring_base_addr = chan_dma_addr;
	chan_dma->base = chan_props.ring_base_vaddr;
	chan_dma->phys_base = chan_props.ring_base_addr;
	chan_dma->size = chan_props.ring_len;
	chan_props.use_db_eng = GSI_CHAN_DIRECT_MODE;
	chan_props.max_prefetch = GSI_ONE_PREFETCH_SEG;
	chan_props.low_weight = 1;
	chan_props.chan_user_data = NULL;
	chan_props.err_cb = ipa_chan_err_cb;
	chan_props.xfer_cb = ipa_xfer_cb;

	gsi_res = gsi_set_channel_cfg(ep->gsi_chan_hdl, &chan_props, NULL);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error setting channel properties\n");
		result = -EFAULT;
		goto set_chan_cfg_fail;
	}

	return 0;

set_chan_cfg_fail:
	dma_free_coherent(ipa3_ctx->pdev, chan_dma->size,
		chan_dma->base, chan_dma->phys_base);
	return result;

}

static int ipa3_restore_channel_properties(struct ipa3_ep_context *ep,
	struct gsi_chan_props *chan_props,
	union gsi_channel_scratch *chan_scratch)
{
	enum gsi_status gsi_res;

	gsi_res = gsi_set_channel_cfg(ep->gsi_chan_hdl, chan_props,
		chan_scratch);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error restoring channel properties\n");
		return -EFAULT;
	}

	return 0;
}

static int ipa3_reset_with_open_aggr_frame_wa(u32 clnt_hdl,
	struct ipa3_ep_context *ep)
{
	int result = -EFAULT;
	enum gsi_status gsi_res;
	struct gsi_chan_props orig_chan_props;
	union gsi_channel_scratch orig_chan_scratch;
	struct ipa_mem_buffer chan_dma;
	void *buff;
	dma_addr_t dma_addr;
	struct gsi_xfer_elem xfer_elem;
	int i;
	int aggr_active_bitmap = 0;
	bool pipe_suspended = false;
	struct ipa_ep_cfg_ctrl ctrl;

	IPADBG("Applying reset channel with open aggregation frame WA\n");
	ipahal_write_reg(IPA_AGGR_FORCE_CLOSE, (1 << clnt_hdl));

	/* Reset channel */
	gsi_res = gsi_reset_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error resetting channel: %d\n", gsi_res);
		return -EFAULT;
	}

	/* Reconfigure channel to dummy GPI channel */
	memset(&orig_chan_props, 0, sizeof(struct gsi_chan_props));
	memset(&orig_chan_scratch, 0, sizeof(union gsi_channel_scratch));
	gsi_res = gsi_get_channel_cfg(ep->gsi_chan_hdl, &orig_chan_props,
		&orig_chan_scratch);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error getting channel properties: %d\n", gsi_res);
		return -EFAULT;
	}
	memset(&chan_dma, 0, sizeof(struct ipa_mem_buffer));
	result = ipa3_reconfigure_channel_to_gpi(ep, &orig_chan_props,
		&chan_dma);
	if (result)
		return -EFAULT;

	ipahal_read_reg_n_fields(IPA_ENDP_INIT_CTRL_n, clnt_hdl, &ctrl);
	if (ctrl.ipa_ep_suspend) {
		IPADBG("pipe is suspended, remove suspend\n");
		pipe_suspended = true;
		ctrl.ipa_ep_suspend = false;
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_CTRL_n,
			clnt_hdl, &ctrl);
	}

	/* Start channel and put 1 Byte descriptor on it */
	gsi_res = gsi_start_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error starting channel: %d\n", gsi_res);
		goto start_chan_fail;
	}

	memset(&xfer_elem, 0, sizeof(struct gsi_xfer_elem));
	buff = dma_alloc_coherent(ipa3_ctx->pdev, 1, &dma_addr,
		GFP_KERNEL);
	xfer_elem.addr = dma_addr;
	xfer_elem.len = 1;
	xfer_elem.flags = GSI_XFER_FLAG_EOT;
	xfer_elem.type = GSI_XFER_ELEM_DATA;

	gsi_res = gsi_queue_xfer(ep->gsi_chan_hdl, 1, &xfer_elem,
		true);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error queueing xfer: %d\n", gsi_res);
		result = -EFAULT;
		goto queue_xfer_fail;
	}

	/* Wait for aggregation frame to be closed and stop channel*/
	for (i = 0; i < IPA_POLL_AGGR_STATE_RETRIES_NUM; i++) {
		aggr_active_bitmap = ipahal_read_reg(IPA_STATE_AGGR_ACTIVE);
		if (!(aggr_active_bitmap & (1 << clnt_hdl)))
			break;
		msleep(IPA_POLL_AGGR_STATE_SLEEP_MSEC);
	}

	if (aggr_active_bitmap & (1 << clnt_hdl)) {
		IPAERR("Failed closing aggr frame for client: %d\n",
			clnt_hdl);
		BUG();
	}

	dma_free_coherent(ipa3_ctx->pdev, 1, buff, dma_addr);

	result = ipa3_stop_gsi_channel(clnt_hdl);
	if (result) {
		IPAERR("Error stopping channel: %d\n", result);
		goto start_chan_fail;
	}

	/* Reset channel */
	gsi_res = gsi_reset_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error resetting channel: %d\n", gsi_res);
		result = -EFAULT;
		goto start_chan_fail;
	}

	/*
	 * Need to sleep for 1ms as required by H/W verified
	 * sequence for resetting GSI channel
	 */
	msleep(IPA_POLL_AGGR_STATE_SLEEP_MSEC);

	if (pipe_suspended) {
		IPADBG("suspend the pipe again\n");
		ctrl.ipa_ep_suspend = true;
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_CTRL_n,
			clnt_hdl, &ctrl);
	}

	/* Restore channels properties */
	result = ipa3_restore_channel_properties(ep, &orig_chan_props,
		&orig_chan_scratch);
	if (result)
		goto restore_props_fail;
	dma_free_coherent(ipa3_ctx->pdev, chan_dma.size,
		chan_dma.base, chan_dma.phys_base);

	return 0;

queue_xfer_fail:
	ipa3_stop_gsi_channel(clnt_hdl);
	dma_free_coherent(ipa3_ctx->pdev, 1, buff, dma_addr);
start_chan_fail:
	if (pipe_suspended) {
		IPADBG("suspend the pipe again\n");
		ctrl.ipa_ep_suspend = true;
		ipahal_write_reg_n_fields(IPA_ENDP_INIT_CTRL_n,
			clnt_hdl, &ctrl);
	}
	ipa3_restore_channel_properties(ep, &orig_chan_props,
		&orig_chan_scratch);
restore_props_fail:
	dma_free_coherent(ipa3_ctx->pdev, chan_dma.size,
		chan_dma.base, chan_dma.phys_base);
	return result;
}

int ipa3_reset_gsi_channel(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;
	int aggr_active_bitmap = 0;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	/*
	 * Check for open aggregation frame on Consumer EP -
	 * reset with open aggregation frame WA
	 */
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		aggr_active_bitmap = ipahal_read_reg(IPA_STATE_AGGR_ACTIVE);
		if (aggr_active_bitmap & (1 << clnt_hdl)) {
			result = ipa3_reset_with_open_aggr_frame_wa(clnt_hdl,
				ep);
			if (result)
				goto reset_chan_fail;
			goto finish_reset;
		}
	}

	/*
	 * Reset channel
	 * If the reset called after stop, need to wait 1ms
	 */
	msleep(IPA_POLL_AGGR_STATE_SLEEP_MSEC);
	gsi_res = gsi_reset_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error resetting channel: %d\n", gsi_res);
		result = -EFAULT;
		goto reset_chan_fail;
	}

finish_reset:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("exit\n");
	return 0;

reset_chan_fail:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_reset_gsi_event_ring(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	/* Reset event ring */
	gsi_res = gsi_reset_evt_ring(ep->gsi_evt_ring_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error resetting event: %d\n", gsi_res);
		result = -EFAULT;
		goto reset_evt_fail;
	}

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("exit\n");
	return 0;

reset_evt_fail:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

static bool ipa3_is_legal_params(struct ipa_request_gsi_channel_params *params)
{
	if (params->client >= IPA_CLIENT_MAX)
		return false;
	else
		return true;
}

int ipa3_smmu_map_peer_reg(phys_addr_t phys_addr, bool map)
{
	struct iommu_domain *smmu_domain;
	int res;

	if (ipa3_ctx->smmu_s1_bypass)
		return 0;

	smmu_domain = ipa3_get_smmu_domain();
	if (!smmu_domain) {
		IPAERR("invalid smmu domain\n");
		return -EINVAL;
	}

	if (map) {
		res = ipa3_iommu_map(smmu_domain, phys_addr, phys_addr,
			PAGE_SIZE, IOMMU_READ | IOMMU_WRITE | IOMMU_DEVICE);
	} else {
		res = iommu_unmap(smmu_domain, phys_addr, PAGE_SIZE);
		res = (res != PAGE_SIZE);
	}
	if (res) {
		IPAERR("Fail to %s reg 0x%pa\n", map ? "map" : "unmap",
			&phys_addr);
		return -EINVAL;
	}

	IPADBG("Peer reg 0x%pa %s\n", &phys_addr, map ? "map" : "unmap");

	return 0;
}

int ipa3_smmu_map_peer_buff(u64 iova, phys_addr_t phys_addr, u32 size, bool map)
{
	struct iommu_domain *smmu_domain;
	int res;

	if (ipa3_ctx->smmu_s1_bypass)
		return 0;

	smmu_domain = ipa3_get_smmu_domain();
	if (!smmu_domain) {
		IPAERR("invalid smmu domain\n");
		return -EINVAL;
	}

	if (map) {
		res = ipa3_iommu_map(smmu_domain,
			rounddown(iova, PAGE_SIZE),
			rounddown(phys_addr, PAGE_SIZE),
			roundup(size + iova - rounddown(iova, PAGE_SIZE),
			PAGE_SIZE),
			IOMMU_READ | IOMMU_WRITE);
		if (res) {
			IPAERR("Fail to map 0x%llx->0x%pa\n", iova, &phys_addr);
			return -EINVAL;
		}
	} else {
		res = iommu_unmap(smmu_domain,
			rounddown(iova, PAGE_SIZE),
			roundup(size + iova - rounddown(iova, PAGE_SIZE),
			PAGE_SIZE));
		if (res != roundup(size + iova - rounddown(iova, PAGE_SIZE),
			PAGE_SIZE)) {
			IPAERR("Fail to unmap 0x%llx->0x%pa\n",
				iova, &phys_addr);
			return -EINVAL;
		}
	}

	IPADBG("Peer buff %s 0x%llx->0x%pa\n", map ? "map" : "unmap",
		iova, &phys_addr);

	return 0;
}


int ipa3_request_gsi_channel(struct ipa_request_gsi_channel_params *params,
			     struct ipa_req_chan_out_params *out_params)
{
	int ipa_ep_idx;
	int result = -EFAULT;
	struct ipa3_ep_context *ep;
	struct ipahal_reg_ep_cfg_status ep_status;
	unsigned long gsi_dev_hdl;
	enum gsi_status gsi_res;
	struct ipa_gsi_ep_config gsi_ep_cfg;
	struct ipa_gsi_ep_config *gsi_ep_cfg_ptr = &gsi_ep_cfg;

	IPADBG("entry\n");
	if (params == NULL || out_params == NULL ||
		!ipa3_is_legal_params(params)) {
		IPAERR("bad parameters\n");
		return -EINVAL;
	}

	ipa_ep_idx = ipa3_get_ep_mapping(params->client);
	if (ipa_ep_idx == -1) {
		IPAERR("fail to alloc EP.\n");
		goto fail;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (ep->valid) {
		IPAERR("EP already allocated.\n");
		goto fail;
	}

	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	ep->skip_ep_cfg = params->skip_ep_cfg;
	ep->valid = 1;
	ep->client = params->client;
	ep->client_notify = params->notify;
	ep->priv = params->priv;
	ep->keep_ipa_awake = params->keep_ipa_awake;

	if (!ep->skip_ep_cfg) {
		if (ipa3_cfg_ep(ipa_ep_idx, &params->ipa_ep_cfg)) {
			IPAERR("fail to configure EP.\n");
			goto ipa_cfg_ep_fail;
		}
		/* Setting EP status 0 */
		memset(&ep_status, 0, sizeof(ep_status));
		if (ipa3_cfg_ep_status(ipa_ep_idx, &ep_status)) {
			IPAERR("fail to configure status of EP.\n");
			goto ipa_cfg_ep_fail;
		}
		IPADBG("ep configuration successful\n");
	} else {
		IPADBG("Skipping endpoint configuration.\n");
	}

	out_params->clnt_hdl = ipa_ep_idx;

	result = ipa3_enable_data_path(out_params->clnt_hdl);
	if (result) {
		IPAERR("enable data path failed res=%d clnt=%d.\n", result,
				out_params->clnt_hdl);
		goto ipa_cfg_ep_fail;
	}

	gsi_dev_hdl = ipa3_ctx->gsi_dev_hdl;
	gsi_res = gsi_alloc_evt_ring(&params->evt_ring_params, gsi_dev_hdl,
		&ep->gsi_evt_ring_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error allocating event ring: %d\n", gsi_res);
		result = -EFAULT;
		goto ipa_cfg_ep_fail;
	}

	gsi_res = gsi_write_evt_ring_scratch(ep->gsi_evt_ring_hdl,
		params->evt_scratch);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error writing event ring scratch: %d\n", gsi_res);
		result = -EFAULT;
		goto write_evt_scratch_fail;
	}

	memset(gsi_ep_cfg_ptr, 0, sizeof(struct ipa_gsi_ep_config));
	gsi_ep_cfg_ptr = ipa_get_gsi_ep_info(ipa_ep_idx);
	params->chan_params.evt_ring_hdl = ep->gsi_evt_ring_hdl;
	params->chan_params.ch_id = gsi_ep_cfg_ptr->ipa_gsi_chan_num;
	gsi_res = gsi_alloc_channel(&params->chan_params, gsi_dev_hdl,
		&ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error allocating channel: %d, chan_id: %d\n", gsi_res,
			params->chan_params.ch_id);
		result = -EFAULT;
		goto write_evt_scratch_fail;
	}

	memcpy(&ep->chan_scratch, &params->chan_scratch,
		sizeof(union __packed gsi_channel_scratch));
	ep->chan_scratch.xdci.max_outstanding_tre =
		params->chan_params.re_size * gsi_ep_cfg_ptr->ipa_if_tlv;
	gsi_res = gsi_write_channel_scratch(ep->gsi_chan_hdl,
		params->chan_scratch);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error writing channel scratch: %d\n", gsi_res);
		result = -EFAULT;
		goto write_chan_scratch_fail;
	}

	gsi_res = gsi_query_channel_db_addr(ep->gsi_chan_hdl,
		&out_params->db_reg_phs_addr_lsb,
		&out_params->db_reg_phs_addr_msb);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error querying channel DB registers addresses: %d\n",
			gsi_res);
		result = -EFAULT;
		goto write_chan_scratch_fail;
	}

	ep->gsi_mem_info.evt_ring_len = params->evt_ring_params.ring_len;
	ep->gsi_mem_info.evt_ring_base_addr =
		params->evt_ring_params.ring_base_addr;
	ep->gsi_mem_info.evt_ring_base_vaddr =
		params->evt_ring_params.ring_base_vaddr;
	ep->gsi_mem_info.chan_ring_len = params->chan_params.ring_len;
	ep->gsi_mem_info.chan_ring_base_addr =
		params->chan_params.ring_base_addr;
	ep->gsi_mem_info.chan_ring_base_vaddr =
		params->chan_params.ring_base_vaddr;

	ipa3_ctx->skip_ep_cfg_shadow[ipa_ep_idx] = ep->skip_ep_cfg;
	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(params->client))
		ipa3_install_dflt_flt_rules(ipa_ep_idx);

	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("client %d (ep: %d) connected\n", params->client, ipa_ep_idx);
	IPADBG("exit\n");

	return 0;

write_chan_scratch_fail:
	gsi_dealloc_channel(ep->gsi_chan_hdl);
write_evt_scratch_fail:
	gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
ipa_cfg_ep_fail:
	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();
fail:
	return result;
}

int ipa3_set_usb_max_packet_size(
	enum ipa_usb_max_usb_packet_size usb_max_packet_size)
{
	struct gsi_device_scratch dev_scratch;
	enum gsi_status gsi_res;

	IPADBG("entry\n");

	IPA_ACTIVE_CLIENTS_INC_SIMPLE();

	memset(&dev_scratch, 0, sizeof(struct gsi_device_scratch));
	dev_scratch.mhi_base_chan_idx_valid = false;
	dev_scratch.max_usb_pkt_size_valid = true;
	dev_scratch.max_usb_pkt_size = usb_max_packet_size;

	gsi_res = gsi_write_device_scratch(ipa3_ctx->gsi_dev_hdl,
		&dev_scratch);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error writing device scratch: %d\n", gsi_res);
		return -EFAULT;
	}
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();

	IPADBG("exit\n");
	return 0;
}

int ipa3_xdci_connect(u32 clnt_hdl)
{
	int result;
	struct ipa3_ep_context *ep;

	IPADBG("entry\n");

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	result = ipa3_start_gsi_channel(clnt_hdl);
	if (result) {
		IPAERR("failed to start gsi channel clnt_hdl=%u\n", clnt_hdl);
		goto exit;
	}

	result = ipa3_enable_data_path(clnt_hdl);
	if (result) {
		IPAERR("enable data path failed res=%d clnt_hdl=%d.\n", result,
			clnt_hdl);
		goto stop_ch;
	}

	IPADBG("exit\n");
	goto exit;

stop_ch:
	(void)ipa3_stop_gsi_channel(clnt_hdl);
exit:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_xdci_start(u32 clnt_hdl, u8 xferrscidx, bool xferrscidx_valid)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes  ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 ||
		xferrscidx < 0 || xferrscidx > IPA_XFER_RSC_IDX_MAX) {
		IPAERR("Bad parameters.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	if (xferrscidx_valid) {
		ep->chan_scratch.xdci.xferrscidx = xferrscidx;
		gsi_res = gsi_write_channel_scratch(ep->gsi_chan_hdl,
			ep->chan_scratch);
		if (gsi_res != GSI_STATUS_SUCCESS) {
			IPAERR("Error writing channel scratch: %d\n", gsi_res);
			goto write_chan_scratch_fail;
		}
	}
	gsi_res = gsi_start_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error starting channel: %d\n", gsi_res);
		goto write_chan_scratch_fail;
	}
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("exit\n");
	return 0;

write_chan_scratch_fail:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

static int ipa3_get_gsi_chan_info(struct gsi_chan_info *gsi_chan_info,
	unsigned long chan_hdl)
{
	enum gsi_status gsi_res;

	memset(gsi_chan_info, 0, sizeof(struct gsi_chan_info));
	gsi_res = gsi_query_channel_info(chan_hdl, gsi_chan_info);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error querying channel info: %d\n", gsi_res);
		return -EFAULT;
	}
	if (!gsi_chan_info->evt_valid) {
		IPAERR("Event info invalid\n");
		return -EFAULT;
	}

	return 0;
}

static bool ipa3_is_xdci_channel_with_given_info_empty(
	struct ipa3_ep_context *ep, struct gsi_chan_info *chan_info)
{
	bool is_empty = false;

	if (!IPA_CLIENT_IS_CONS(ep->client)) {
		/* For UL channel: chan.RP == chan.WP */
		is_empty = (chan_info->rp == chan_info->wp);
	} else {
		/* For DL channel: */
		if (chan_info->wp !=
		    (ep->gsi_mem_info.chan_ring_base_addr +
		     ep->gsi_mem_info.chan_ring_len -
		     GSI_CHAN_RE_SIZE_16B)) {
			/*  if chan.WP != LINK TRB: chan.WP == evt.RP */
			is_empty = (chan_info->wp == chan_info->evt_rp);
		} else {
			/*
			 * if chan.WP == LINK TRB: chan.base_xfer_ring_addr
			 * == evt.RP
			 */
			is_empty = (ep->gsi_mem_info.chan_ring_base_addr ==
				chan_info->evt_rp);
		}
	}

	return is_empty;
}

static int ipa3_is_xdci_channel_empty(struct ipa3_ep_context *ep,
	bool *is_empty)
{
	struct gsi_chan_info chan_info;
	int res;

	if (!ep || !is_empty || !ep->valid) {
		IPAERR("Input Error\n");
		return -EFAULT;
	}

	res = ipa3_get_gsi_chan_info(&chan_info, ep->gsi_chan_hdl);
	if (res) {
		IPAERR("Failed to get GSI channel info\n");
		return -EFAULT;
	}

	*is_empty = ipa3_is_xdci_channel_with_given_info_empty(ep, &chan_info);

	return 0;
}

int ipa3_enable_force_clear(u32 request_id, bool throttle_source,
	u32 source_pipe_bitmask)
{
	struct ipa_enable_force_clear_datapath_req_msg_v01 req;
	int result;

	memset(&req, 0, sizeof(req));
	req.request_id = request_id;
	req.source_pipe_bitmask = source_pipe_bitmask;
	if (throttle_source) {
		req.throttle_source_valid = 1;
		req.throttle_source = 1;
	}
	result = ipa3_qmi_enable_force_clear_datapath_send(&req);
	if (result) {
		IPAERR("ipa3_qmi_enable_force_clear_datapath_send failed %d\n",
			result);
		return result;
	}

	return 0;
}

int ipa3_disable_force_clear(u32 request_id)
{
	struct ipa_disable_force_clear_datapath_req_msg_v01 req;
	int result;

	memset(&req, 0, sizeof(req));
	req.request_id = request_id;
	result = ipa3_qmi_disable_force_clear_datapath_send(&req);
	if (result) {
		IPAERR("ipa3_qmi_disable_force_clear_datapath_send failed %d\n",
			result);
		return result;
	}

	return 0;
}

/* Clocks should be voted before invoking this function */
static int ipa3_xdci_stop_gsi_channel(u32 clnt_hdl, bool *stop_in_proc)
{
	int res;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 ||
		!stop_in_proc) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	res = ipa3_stop_gsi_channel(clnt_hdl);
	if (res != 0 && res != -GSI_STATUS_AGAIN &&
		res != -GSI_STATUS_TIMED_OUT) {
		IPAERR("xDCI stop channel failed res=%d\n", res);
		return -EFAULT;
	}

	if (res)
		*stop_in_proc = true;
	else
		*stop_in_proc = false;

	IPADBG("xDCI channel is %s (result=%d)\n",
		res ? "STOP_IN_PROC/TimeOut" : "STOP", res);

	IPADBG("exit\n");
	return 0;
}

/* Clocks should be voted before invoking this function */
static int ipa3_xdci_stop_gsi_ch_brute_force(u32 clnt_hdl,
	bool *stop_in_proc)
{
	unsigned long jiffies_start;
	unsigned long jiffies_timeout =
		msecs_to_jiffies(IPA_CHANNEL_STOP_IN_PROC_TO_MSEC);
	int res;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0 ||
		!stop_in_proc) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	jiffies_start = jiffies;
	while (1) {
		res = ipa3_xdci_stop_gsi_channel(clnt_hdl,
			stop_in_proc);
		if (res) {
			IPAERR("failed to stop xDCI channel hdl=%d\n",
				clnt_hdl);
			return res;
		}

		if (!*stop_in_proc) {
			IPADBG("xDCI channel STOP hdl=%d\n", clnt_hdl);
			return res;
		}

		/*
		 * Give chance to the previous stop request to be accomplished
		 * before the retry
		 */
		udelay(IPA_CHANNEL_STOP_IN_PROC_SLEEP_USEC);

		if (time_after(jiffies, jiffies_start + jiffies_timeout)) {
			IPADBG("timeout waiting for xDCI channel emptiness\n");
			return res;
		}
	}
}

/* Clocks should be voted for before invoking this function */
static int ipa3_stop_ul_chan_with_data_drain(u32 qmi_req_id,
		u32 source_pipe_bitmask, bool should_force_clear, u32 clnt_hdl)
{
	int result;
	bool is_empty = false;
	int i;
	bool stop_in_proc;
	struct ipa3_ep_context *ep;

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	/* first try to stop the channel */
	result = ipa3_xdci_stop_gsi_ch_brute_force(clnt_hdl,
			&stop_in_proc);
	if (result) {
		IPAERR("fail to stop UL channel - hdl=%d clnt=%d\n",
			clnt_hdl, ep->client);
		goto exit;
	}
	if (!stop_in_proc)
		goto exit;

	/* if stop_in_proc, lets wait for emptiness */
	for (i = 0; i < IPA_POLL_FOR_EMPTINESS_NUM; i++) {
		result = ipa3_is_xdci_channel_empty(ep, &is_empty);
		if (result)
			goto exit;
		if (is_empty)
			break;
		udelay(IPA_POLL_FOR_EMPTINESS_SLEEP_USEC);
	}
	/* In case of empty, lets try to stop the channel again */
	if (is_empty) {
		result = ipa3_xdci_stop_gsi_ch_brute_force(clnt_hdl,
			&stop_in_proc);
		if (result) {
			IPAERR("fail to stop UL channel - hdl=%d clnt=%d\n",
				clnt_hdl, ep->client);
			goto exit;
		}
		if (!stop_in_proc)
			goto exit;
	}
	/* if still stop_in_proc or not empty, activate force clear */
	if (should_force_clear) {
		result = ipa3_enable_force_clear(qmi_req_id, false,
			source_pipe_bitmask);
		if (result)
			goto exit;
	}
	/* with force clear, wait for emptiness */
	for (i = 0; i < IPA_POLL_FOR_EMPTINESS_NUM; i++) {
		result = ipa3_is_xdci_channel_empty(ep, &is_empty);
		if (result)
			goto disable_force_clear_and_exit;
		if (is_empty)
			break;

		udelay(IPA_POLL_FOR_EMPTINESS_SLEEP_USEC);
	}
	/* try to stop for the last time */
	result = ipa3_xdci_stop_gsi_ch_brute_force(clnt_hdl,
		&stop_in_proc);
	if (result) {
		IPAERR("fail to stop UL channel - hdl=%d clnt=%d\n",
			clnt_hdl, ep->client);
		goto disable_force_clear_and_exit;
	}
	result = stop_in_proc ? -EFAULT : 0;

disable_force_clear_and_exit:
	if (should_force_clear)
		ipa3_disable_force_clear(qmi_req_id);
exit:
	return result;
}

int ipa3_xdci_disconnect(u32 clnt_hdl, bool should_force_clear, u32 qmi_req_id)
{
	struct ipa3_ep_context *ep;
	int result;
	u32 source_pipe_bitmask = 0;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	ipa3_disable_data_path(clnt_hdl);

	if (!IPA_CLIENT_IS_CONS(ep->client)) {
		IPADBG("Stopping PROD channel - hdl=%d clnt=%d\n",
			clnt_hdl, ep->client);
		source_pipe_bitmask = 1 <<
			ipa3_get_ep_mapping(ep->client);
		result = ipa3_stop_ul_chan_with_data_drain(qmi_req_id,
			source_pipe_bitmask, should_force_clear, clnt_hdl);
		if (result) {
			IPAERR("Fail to stop UL channel with data drain\n");
			WARN_ON(1);
			goto stop_chan_fail;
		}
	} else {
		IPADBG("Stopping CONS channel - hdl=%d clnt=%d\n",
			clnt_hdl, ep->client);
		result = ipa3_stop_gsi_channel(clnt_hdl);
		if (result) {
			IPAERR("Error stopping channel (CONS client): %d\n",
				result);
			goto stop_chan_fail;
		}
	}
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("exit\n");
	return 0;

stop_chan_fail:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_release_gsi_channel(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	gsi_res = gsi_dealloc_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error deallocating channel: %d\n", gsi_res);
		goto dealloc_chan_fail;
	}

	gsi_res = gsi_dealloc_evt_ring(ep->gsi_evt_ring_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error deallocating event: %d\n", gsi_res);
		goto dealloc_chan_fail;
	}

	if (!ep->skip_ep_cfg && IPA_CLIENT_IS_PROD(ep->client))
		ipa3_delete_dflt_flt_rules(clnt_hdl);

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	memset(&ipa3_ctx->ep[clnt_hdl], 0, sizeof(struct ipa3_ep_context));

	IPADBG("exit\n");
	return 0;

dealloc_chan_fail:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_xdci_suspend(u32 ul_clnt_hdl, u32 dl_clnt_hdl,
	bool should_force_clear, u32 qmi_req_id, bool is_dpl)
{
	struct ipa3_ep_context *ul_ep, *dl_ep;
	int result = -EFAULT;
	u32 source_pipe_bitmask = 0;
	bool dl_data_pending = true;
	bool ul_data_pending = true;
	int i;
	bool is_empty = false;
	struct gsi_chan_info ul_gsi_chan_info, dl_gsi_chan_info;
	int aggr_active_bitmap = 0;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	/* In case of DPL, dl is the DPL channel/client */

	IPADBG("entry\n");
	if (dl_clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[dl_clnt_hdl].valid == 0 ||
		(!is_dpl && (ul_clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[ul_clnt_hdl].valid == 0))) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	dl_ep = &ipa3_ctx->ep[dl_clnt_hdl];
	if (!is_dpl)
		ul_ep = &ipa3_ctx->ep[ul_clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(dl_clnt_hdl));

	result = ipa3_get_gsi_chan_info(&dl_gsi_chan_info,
		dl_ep->gsi_chan_hdl);
	if (result)
		goto disable_clk_and_exit;

	if (!is_dpl) {
		result = ipa3_get_gsi_chan_info(&ul_gsi_chan_info,
			ul_ep->gsi_chan_hdl);
		if (result)
			goto disable_clk_and_exit;
	}

	for (i = 0; i < IPA_POLL_FOR_EMPTINESS_NUM; i++) {
		if (!dl_data_pending && !ul_data_pending)
			break;
		result = ipa3_is_xdci_channel_empty(dl_ep, &is_empty);
		if (result)
			goto disable_clk_and_exit;
		if (!is_empty) {
			dl_data_pending = true;
			break;
		}
		dl_data_pending = false;
		if (!is_dpl) {
			result = ipa3_is_xdci_channel_empty(ul_ep, &is_empty);
			if (result)
				goto disable_clk_and_exit;
			ul_data_pending = !is_empty;
		} else {
			ul_data_pending = false;
		}

		udelay(IPA_POLL_FOR_EMPTINESS_SLEEP_USEC);
	}

	if (!dl_data_pending) {
		aggr_active_bitmap = ipahal_read_reg(IPA_STATE_AGGR_ACTIVE);
		if (aggr_active_bitmap & (1 << dl_clnt_hdl)) {
			IPADBG("DL/DPL data pending due to open aggr. frame\n");
			dl_data_pending = true;
		}
	}
	if (dl_data_pending) {
		IPAERR("DL/DPL data pending, can't suspend\n");
		result = -EFAULT;
		goto disable_clk_and_exit;
	}

	/* Suspend the DL/DPL EP */
	memset(&ep_cfg_ctrl, 0 , sizeof(struct ipa_ep_cfg_ctrl));
	ep_cfg_ctrl.ipa_ep_suspend = true;
	ipa3_cfg_ep_ctrl(dl_clnt_hdl, &ep_cfg_ctrl);

	/*
	 * Check if DL/DPL channel is empty again, data could enter the channel
	 * before its IPA EP was suspended
	 */
	result = ipa3_is_xdci_channel_empty(dl_ep, &is_empty);
	if (result)
		goto unsuspend_dl_and_exit;
	if (!is_empty) {
		IPAERR("DL/DPL data pending, can't suspend\n");
		result = -EFAULT;
		goto unsuspend_dl_and_exit;
	}

	/* STOP UL channel */
	if (!is_dpl) {
		source_pipe_bitmask = 1 << ipa3_get_ep_mapping(ul_ep->client);
		result = ipa3_stop_ul_chan_with_data_drain(qmi_req_id,
			source_pipe_bitmask, should_force_clear, ul_clnt_hdl);
		if (result) {
			IPAERR("Error stopping UL channel: result = %d\n",
				result);
			goto unsuspend_dl_and_exit;
		}
	}

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(dl_clnt_hdl));

	IPADBG("exit\n");
	return 0;

unsuspend_dl_and_exit:
	/* Unsuspend the DL EP */
	memset(&ep_cfg_ctrl, 0, sizeof(struct ipa_ep_cfg_ctrl));
	ep_cfg_ctrl.ipa_ep_suspend = false;
	ipa3_cfg_ep_ctrl(dl_clnt_hdl, &ep_cfg_ctrl);
disable_clk_and_exit:
	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(dl_clnt_hdl));
	return result;
}

int ipa3_start_gsi_channel(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	int result = -EFAULT;
	enum gsi_status gsi_res;

	IPADBG("entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes  ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameters.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));

	gsi_res = gsi_start_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error starting channel: %d\n", gsi_res);
		goto start_chan_fail;
	}

	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("exit\n");
	return 0;

start_chan_fail:
	if (!ep->keep_ipa_awake)
		IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));
	return result;
}

int ipa3_xdci_resume(u32 ul_clnt_hdl, u32 dl_clnt_hdl, bool is_dpl)
{
	struct ipa3_ep_context *ul_ep, *dl_ep;
	enum gsi_status gsi_res;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;

	/* In case of DPL, dl is the DPL channel/client */

	IPADBG("entry\n");
	if (dl_clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[dl_clnt_hdl].valid == 0 ||
		(!is_dpl && (ul_clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[ul_clnt_hdl].valid == 0))) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	dl_ep = &ipa3_ctx->ep[dl_clnt_hdl];
	if (!is_dpl)
		ul_ep = &ipa3_ctx->ep[ul_clnt_hdl];
	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(dl_clnt_hdl));

	/* Unsuspend the DL/DPL EP */
	memset(&ep_cfg_ctrl, 0 , sizeof(struct ipa_ep_cfg_ctrl));
	ep_cfg_ctrl.ipa_ep_suspend = false;
	ipa3_cfg_ep_ctrl(dl_clnt_hdl, &ep_cfg_ctrl);

	/* Start UL channel */
	if (!is_dpl) {
		gsi_res = gsi_start_channel(ul_ep->gsi_chan_hdl);
		if (gsi_res != GSI_STATUS_SUCCESS)
			IPAERR("Error starting UL channel: %d\n", gsi_res);
	}

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(dl_clnt_hdl));

	IPADBG("exit\n");
	return 0;
}
/**
 * ipa3_clear_endpoint_delay() - Remove ep delay set on the IPA pipe before
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
int ipa3_clear_endpoint_delay(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep;
	struct ipa_ep_cfg_ctrl ep_ctrl = {0};
	struct ipa_enable_force_clear_datapath_req_msg_v01 req = {0};
	int res;

	if (unlikely(!ipa3_ctx)) {
		IPAERR("IPA driver was not initialized\n");
		return -EINVAL;
	}

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ipa3_ctx->tethered_flow_control) {
		IPADBG("APPS flow control is not enabled\n");
		/* Send a message to modem to disable flow control honoring. */
		req.request_id = clnt_hdl;
		req.source_pipe_bitmask = 1 << clnt_hdl;
		res = ipa3_qmi_enable_force_clear_datapath_send(&req);
		if (res) {
			IPADBG("enable_force_clear_datapath failed %d\n",
				res);
		}
		ep->qmi_request_sent = true;
	}

	IPA_ACTIVE_CLIENTS_INC_EP(ipa3_get_client_mapping(clnt_hdl));
	/* Set disconnect in progress flag so further flow control events are
	 * not honored.
	 */
	spin_lock(&ipa3_ctx->disconnect_lock);
	ep->disconnect_in_progress = true;
	spin_unlock(&ipa3_ctx->disconnect_lock);

	/* If flow is disabled at this point, restore the ep state.*/
	ep_ctrl.ipa_ep_delay = false;
	ep_ctrl.ipa_ep_suspend = false;
	ipa3_cfg_ep_ctrl(clnt_hdl, &ep_ctrl);

	IPA_ACTIVE_CLIENTS_DEC_EP(ipa3_get_client_mapping(clnt_hdl));

	IPADBG("client (ep: %d) removed ep delay\n", clnt_hdl);

	return 0;
}
