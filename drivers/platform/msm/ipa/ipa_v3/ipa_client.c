/* Copyright (c) 2012-2015, The Linux Foundation. All rights reserved.
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

int ipa3_enable_data_path(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep = &ipa3_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	int res = 0;
	u32 reg_val = 0;

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
		ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}

	/* Assign the resource group for pipe*/
	if (ipa_get_ep_group(ep->client) == -1) {
		IPAERR("invalid group for client %d\n", ep->client);
		WARN_ON(1);
		return -EFAULT;
	}

	IPADBG("Setting group %d for pipe %d\n",
		ipa_get_ep_group(ep->client), clnt_hdl);
	IPA_SETFIELD_IN_REG(reg_val, ipa_get_ep_group(ep->client),
		IPA_ENDP_INIT_RSRC_GRP_n_RSRC_GRP_SHFT,
		IPA_ENDP_INIT_RSRC_GRP_n_RSRC_GRP_BMSK);
	ipa_write_reg(ipa3_ctx->mmio,
		IPA_ENDP_INIT_RSRC_GRP_n(clnt_hdl), reg_val);

	return res;
}

int ipa3_disable_data_path(u32 clnt_hdl)
{
	struct ipa3_ep_context *ep = &ipa3_ctx->ep[clnt_hdl];
	struct ipa_ep_cfg_holb holb_cfg;
	struct ipa_ep_cfg_ctrl ep_cfg_ctrl;
	u32 aggr_init;
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
		memset(&ep_cfg_ctrl, 0 , sizeof(struct ipa_ep_cfg_ctrl));
		ep_cfg_ctrl.ipa_ep_suspend = true;
		ipa3_cfg_ep_ctrl(clnt_hdl, &ep_cfg_ctrl);
	}

	udelay(IPA_PKT_FLUSH_TO_US);
	aggr_init = ipa_read_reg(ipa3_ctx->mmio,
			IPA_ENDP_INIT_AGGR_N_OFST_v3_0(clnt_hdl));
	if (((aggr_init & IPA_ENDP_INIT_AGGR_N_AGGR_EN_BMSK) >>
	    IPA_ENDP_INIT_AGGR_N_AGGR_EN_SHFT) == IPA_ENABLE_AGGR)
		ipa3_tag_aggr_force_close(clnt_hdl);

	return res;
}

static int ipa3_smmu_map_peer_bam(unsigned long dev)
{
	phys_addr_t base;
	u32 size;
	struct iommu_domain *smmu_domain;

	if (ipa3_ctx->smmu_present) {
		if (ipa3_ctx->peer_bam_map_cnt == 0) {
			if (sps_get_bam_addr(dev, &base, &size)) {
				IPAERR("Fail to get addr\n");
				return -EINVAL;
			}
			smmu_domain = ipa3_get_smmu_domain();
			if (smmu_domain != NULL) {
				if (iommu_map(smmu_domain,
					IPA_SMMU_AP_VA_END,
					rounddown(base, PAGE_SIZE),
					roundup(size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE),
					IOMMU_READ | IOMMU_WRITE |
					IOMMU_DEVICE)) {
					IPAERR("Fail to iommu_map\n");
					return -EINVAL;
				}
			}

			ipa3_ctx->peer_bam_iova = IPA_SMMU_AP_VA_END;
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
	if (!ipa3_ctx->smmu_present) {
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
	struct ipa3_ep_cfg_status ep_status;
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
	ipa3_inc_client_enable_clks();

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

	if (ipa3_ctx->smmu_present &&
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

	if (ipa3_ctx->smmu_present) {
		ep->connect.data.iova = ep->connect.data.phys_base;
		base = ep->connect.data.iova;
		smmu_domain = ipa_get_smmu_domain();
		if (smmu_domain != NULL) {
			if (iommu_map(smmu_domain,
				rounddown(base, PAGE_SIZE),
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.data.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE)) {
				IPAERR("Fail to iommu_map data FIFO\n");
				goto iommu_map_data_fail;
			}
		}
		ep->connect.desc.iova = ep->connect.desc.phys_base;
		base = ep->connect.desc.iova;
		if (smmu_domain != NULL) {
			if (iommu_map(smmu_domain,
				rounddown(base, PAGE_SIZE),
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.desc.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE),
				IOMMU_READ | IOMMU_WRITE)) {
				IPAERR("Fail to iommu_map desc FIFO\n");
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
		ipa3_dec_client_disable_clks();

	IPADBG("client %d (ep: %d) connected\n", in->client, ipa_ep_idx);

	return 0;

sps_connect_fail:
	if (ipa3_ctx->smmu_present) {
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
	if (ipa3_ctx->smmu_present) {
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
	ipa3_dec_client_disable_clks();
fail:
	return result;
}

static int ipa3_smmu_unmap_peer_bam(unsigned long dev)
{
	size_t len;
	struct iommu_domain *smmu_domain;

	if (ipa3_ctx->smmu_present) {
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
					IPA_SMMU_AP_VA_END, len) != len) {
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

	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("bad parm.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		ipa3_inc_client_enable_clks();

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

	if (ipa3_ctx->smmu_present) {
		base = ep->connect.desc.iova;
		smmu_domain = ipa_get_smmu_domain();
		if (smmu_domain != NULL) {
			iommu_unmap(smmu_domain,
				rounddown(base, PAGE_SIZE),
				roundup(ep->connect.desc.size + base -
					rounddown(base, PAGE_SIZE), PAGE_SIZE));
		}
	}

	if (ipa3_ctx->smmu_present) {
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

	spin_lock(&ipa3_ctx->lan_rx_clnt_notify_lock);
	memset(&ipa3_ctx->ep[clnt_hdl], 0, sizeof(struct ipa3_ep_context));
	spin_unlock(&ipa3_ctx->lan_rx_clnt_notify_lock);

	ipa3_dec_client_disable_clks();

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

	ipa3_inc_client_enable_clks();
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
	ipa3_dec_client_disable_clks();

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
	struct ipa3_mem_buffer *xfer_data;

	IPADBG("event %d notified\n", notify->evt_id);
	BUG_ON(notify->xfer_user_data == NULL);

	xfer_data = (struct ipa3_mem_buffer *)notify->xfer_user_data;
	dma_free_coherent(ipa3_ctx->pdev, xfer_data->size,
		xfer_data->base, xfer_data->phys_base);
	kfree(xfer_data);
}

static int ipa3_reconfigure_channel_to_gpi(struct ipa3_ep_context *ep,
	struct gsi_chan_props *orig_chan_props,
	struct ipa3_mem_buffer *chan_dma)
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
	struct ipa3_mem_buffer chan_dma;
	void *buff;
	dma_addr_t dma_addr;
	struct gsi_xfer_elem xfer_elem;
	struct ipa3_mem_buffer *xfer_data;
	int i;
	int aggr_active_bitmap = 0;

	IPADBG("Applying reset channel with open aggregation frame WA\n");
	ipa_write_reg(ipa3_ctx->mmio, IPA_AGGR_FORCE_CLOSE_OFST,
		(1 << clnt_hdl));

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
	memset(&chan_dma, 0, sizeof(struct ipa3_mem_buffer));
	result = ipa3_reconfigure_channel_to_gpi(ep, &orig_chan_props,
		&chan_dma);
	if (result)
		return -EFAULT;

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
	xfer_data = kzalloc(sizeof(struct ipa3_mem_buffer),
		GFP_ATOMIC);
	if (xfer_data == NULL) {
		IPAERR("Error allocating memory\n");
		goto xfer_data_alloc_fail;
	}
	memset(xfer_data, 0, sizeof(struct ipa3_mem_buffer));
	xfer_data->base = buff;
	xfer_data->phys_base = dma_addr;
	xfer_data->size = 1;
	xfer_elem.xfer_user_data = (void *)xfer_data;
	gsi_res = gsi_queue_xfer(ep->gsi_chan_hdl, 1, &xfer_elem,
		true);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error queueing xfer: %d\n", gsi_res);
		result = -EFAULT;
		goto queue_xfer_fail;
	}

	/* Wait for aggregation frame to be closed and stop channel*/
	for (i = 0; i < IPA_POLL_AGGR_STATE_RETRIES_NUM; i++) {
		aggr_active_bitmap = ipa_read_reg(ipa3_ctx->mmio,
			IPA_STATE_AGGR_ACTIVE_OFST);
		if (!(aggr_active_bitmap & (1 << clnt_hdl)))
			break;
		msleep(IPA_POLL_AGGR_STATE_SLEEP_MSEC);
	}

	if (aggr_active_bitmap & (1 << clnt_hdl)) {
		IPAERR("Failed closing aggr frame for client: %d\n",
			clnt_hdl);
		BUG();
	}

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
	kfree(xfer_data);
xfer_data_alloc_fail:
	dma_free_coherent(ipa3_ctx->pdev, 1, buff, dma_addr);
start_chan_fail:
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

	IPADBG("ipa3_reset_gsi_channel: entry\n");
	if (clnt_hdl >= ipa3_ctx->ipa_num_pipes ||
		ipa3_ctx->ep[clnt_hdl].valid == 0) {
		IPAERR("Bad parameter.\n");
		return -EINVAL;
	}

	ep = &ipa3_ctx->ep[clnt_hdl];

	if (!ep->keep_ipa_awake)
		ipa3_inc_client_enable_clks();

	/*
	 * Check for open aggregation frame on Consumer EP -
	 * reset with open aggregation frame WA
	 */
	if (IPA_CLIENT_IS_CONS(ep->client)) {
		aggr_active_bitmap = ipa_read_reg(ipa3_ctx->mmio,
				IPA_STATE_AGGR_ACTIVE_OFST);
		if (aggr_active_bitmap & (1 << clnt_hdl)) {
			result = ipa3_reset_with_open_aggr_frame_wa(clnt_hdl,
				ep);
			if (result)
				goto reset_chan_fail;
			goto finish_reset;
		}
	}

	/* Reset channel */
	gsi_res = gsi_reset_channel(ep->gsi_chan_hdl);
	if (gsi_res != GSI_STATUS_SUCCESS) {
		IPAERR("Error resetting channel: %d\n", gsi_res);
		result = -EFAULT;
		goto reset_chan_fail;
	}

finish_reset:
	if (!ep->keep_ipa_awake)
		ipa3_dec_client_disable_clks();

	IPADBG("ipa3_reset_gsi_channel: exit\n");
	return 0;

reset_chan_fail:
	if (!ep->keep_ipa_awake)
		ipa3_dec_client_disable_clks();
	return result;
}
