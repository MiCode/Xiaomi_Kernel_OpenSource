/* Copyright (c) 2018 The Linux Foundation. All rights reserved.
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

#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mhi.h>
#include "ipa_qmi_service.h"
#include "../ipa_common_i.h"
#include "ipa_i.h"

#define IMP_DRV_NAME "ipa_mhi_proxy"

#define IMP_DBG(fmt, args...) \
	do { \
		pr_debug(IMP_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IMP_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IMP_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IMP_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IMP_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IMP_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)


#define IMP_ERR(fmt, args...) \
	do { \
		pr_err(IMP_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
				IMP_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				IMP_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)


#define IMP_FUNC_ENTRY() \
	IMP_DBG_LOW("ENTRY\n")
#define IMP_FUNC_EXIT() \
	IMP_DBG_LOW("EXIT\n")

#define IMP_IPA_UC_UL_CH_n 0
#define IMP_IPA_UC_UL_EV_n 1
#define IMP_IPA_UC_DL_CH_n 2
#define IMP_IPA_UC_DL_EV_n 3
#define IMP_IPA_UC_m 1

/* each pair of UL/DL channels are defined below */
static const struct mhi_device_id mhi_driver_match_table[] = {
	{ .chan = "IP_HW_OFFLOAD_0" },
	{},
};

static int imp_mhi_probe_cb(struct mhi_device *, const struct mhi_device_id *);
static void imp_mhi_remove_cb(struct mhi_device *);
static void imp_mhi_status_cb(struct mhi_device *, enum MHI_CB);

static struct mhi_driver mhi_driver = {
	.id_table = mhi_driver_match_table,
	.probe = imp_mhi_probe_cb,
	.remove = imp_mhi_remove_cb,
	.status_cb = imp_mhi_status_cb,
	.driver = {
		.name = IMP_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

struct imp_channel_context_type {
	u32 chstate:8;
	u32 brsmode:2;
	u32 pollcfg:6;
	u32 reserved:16;

	u32 chtype;

	u32 erindex;

	u64 rbase;

	u64 rlen;

	u64 rpp;

	u64 wpp;
} __packed;

struct imp_event_context_type {
	u32 reserved:8;
	u32 intmodc:8;
	u32 intmodt:16;

	u32 ertype;

	u32 msivec;

	u64 rbase;

	u64 rlen;

	u64 rpp;

	u64 wpp;
} __packed;

struct imp_iova_addr {
	dma_addr_t base;
	unsigned int size;
};

struct imp_dev_info {
	struct platform_device *pdev;
	bool smmu_enabled;
	struct imp_iova_addr ctrl;
	struct imp_iova_addr data;
	u32 chdb_base;
	u32 erdb_base;
};

struct imp_event_props {
	u16 id;
	phys_addr_t doorbell;
	u16 uc_mbox_n;
	struct imp_event_context_type ev_ctx;
};

struct imp_event {
	struct imp_event_props props;
};

struct imp_channel_props {
	enum dma_data_direction dir;
	u16 id;
	phys_addr_t doorbell;
	u16 uc_mbox_n;
	struct imp_channel_context_type ch_ctx;

};

struct imp_channel {
	struct imp_channel_props props;
	struct imp_event event;
};

enum imp_state {
	IMP_INVALID = 0,
	IMP_PROBED,
	IMP_READY,
	IMP_STARTED
};

struct imp_qmi_cache {
	struct ipa_mhi_ready_indication_msg_v01 ready_ind;
	struct ipa_mhi_alloc_channel_req_msg_v01 alloc_ch_req;
	struct ipa_mhi_alloc_channel_resp_msg_v01 alloc_ch_resp;
};

struct imp_mhi_driver {
	struct mhi_device *mhi_dev;
	struct imp_channel ul_chan;
	struct imp_channel dl_chan;
};

struct imp_context {
	struct imp_dev_info dev_info;
	struct imp_mhi_driver md;
	struct mutex mutex;
	struct mutex lpm_mutex;
	enum imp_state state;
	bool in_lpm;
	bool lpm_disabled;
	struct imp_qmi_cache qmi;

};

static struct imp_context *imp_ctx;

static void _populate_smmu_info(struct ipa_mhi_ready_indication_msg_v01 *req)
{
	req->smmu_info_valid = true;
	req->smmu_info.iova_ctl_base_addr = imp_ctx->dev_info.ctrl.base;
	req->smmu_info.iova_ctl_size = imp_ctx->dev_info.ctrl.size;
	req->smmu_info.iova_data_base_addr = imp_ctx->dev_info.data.base;
	req->smmu_info.iova_data_size = imp_ctx->dev_info.data.size;
}

static void imp_mhi_trigger_ready_ind(void)
{
	struct ipa_mhi_ready_indication_msg_v01 *req
		= &imp_ctx->qmi.ready_ind;
	int ret;
	struct imp_channel *ch;
	struct ipa_mhi_ch_init_info_type_v01 *ch_info;

	IMP_FUNC_ENTRY();
	if (imp_ctx->state != IMP_PROBED) {
		IMP_ERR("invalid state %d\n", imp_ctx->state);
		goto exit;
	}

	if (imp_ctx->dev_info.smmu_enabled)
		_populate_smmu_info(req);

	req->ch_info_arr_len = 0;
	BUILD_BUG_ON(QMI_IPA_REMOTE_MHI_CHANNELS_NUM_MAX_V01 < 2);

	/* UL channel */
	ch = &imp_ctx->md.ul_chan;
	ch_info = &req->ch_info_arr[req->ch_info_arr_len];

	ch_info->ch_id = ch->props.id;
	ch_info->direction_type = ch->props.dir;
	ch_info->er_id = ch->event.props.id;

	/* uC is a doorbell proxy between local Q6 and remote Q6 */
	ch_info->ch_doorbell_addr = ipa3_ctx->ipa_wrapper_base +
		ipahal_get_reg_base() +
		ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
		IMP_IPA_UC_m,
		ch->props.uc_mbox_n);

	ch_info->er_doorbell_addr = ipa3_ctx->ipa_wrapper_base +
		ipahal_get_reg_base() +
		ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
		IMP_IPA_UC_m,
		ch->event.props.uc_mbox_n);
	req->ch_info_arr_len++;

	/* DL channel */
	ch = &imp_ctx->md.dl_chan;
	ch_info = &req->ch_info_arr[req->ch_info_arr_len];

	ch_info->ch_id = ch->props.id;
	ch_info->direction_type = ch->props.dir;
	ch_info->er_id = ch->event.props.id;

	/* uC is a doorbell proxy between local Q6 and remote Q6 */
	ch_info->ch_doorbell_addr = ipa3_ctx->ipa_wrapper_base +
		ipahal_get_reg_base() +
		ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
		IMP_IPA_UC_m,
		ch->props.uc_mbox_n);

	ch_info->er_doorbell_addr = ipa3_ctx->ipa_wrapper_base +
		ipahal_get_reg_base() +
		ipahal_get_reg_mn_ofst(IPA_UC_MAILBOX_m_n,
		IMP_IPA_UC_m,
		ch->event.props.uc_mbox_n);
	req->ch_info_arr_len++;

	IMP_DBG("sending IND to modem\n");
	ret = ipa3_qmi_send_mhi_ready_indication(req);
	if (ret) {
		IMP_ERR("failed to send ready indication to modem %d\n", ret);
		return;
	}

	imp_ctx->state = IMP_READY;

exit:
	IMP_FUNC_EXIT();
}

static struct imp_channel *imp_get_ch_by_id(u16 id)
{
	if (imp_ctx->md.ul_chan.props.id == id)
		return &imp_ctx->md.ul_chan;

	if (imp_ctx->md.dl_chan.props.id == id)
		return &imp_ctx->md.dl_chan;

	return NULL;
}

static struct ipa_mhi_er_info_type_v01 *
	_find_ch_in_er_info_arr(struct ipa_mhi_alloc_channel_req_msg_v01 *req,
	u16 id)
{
	int i;

	if (req->er_info_arr_len > QMI_IPA_REMOTE_MHI_CHANNELS_NUM_MAX_V01)
		return NULL;

	for (i = 0; i < req->tr_info_arr_len; i++)
		if (req->er_info_arr[i].er_id == id)
			return &req->er_info_arr[i];
	return NULL;
}

/* round addresses for closest page per SMMU requirements */
static inline void imp_smmu_round_to_page(uint64_t iova, uint64_t pa,
	uint64_t size, unsigned long *iova_p, phys_addr_t *pa_p, u32 *size_p)
{
	*iova_p = rounddown(iova, PAGE_SIZE);
	*pa_p = rounddown(pa, PAGE_SIZE);
	*size_p = roundup(size + pa - *pa_p, PAGE_SIZE);
}

static void __map_smmu_info(struct device *dev,
	struct imp_iova_addr *partition, int num_mapping,
	struct ipa_mhi_mem_addr_info_type_v01 *map_info,
	bool map)
{
	int i;
	struct iommu_domain *domain;
	unsigned long iova_p;
	phys_addr_t pa_p;
	u32 size_p;

	domain = iommu_get_domain_for_dev(dev);
	if (!domain) {
		IMP_ERR("domain is NULL for dev\n");
		return;
	}

	for (i = 0; i < num_mapping; i++) {
		int prot = IOMMU_READ | IOMMU_WRITE;
		u32 ipa_base = ipa3_ctx->ipa_wrapper_base +
			ipa3_ctx->ctrl->ipa_reg_base_ofst;
		u32 ipa_size = ipa3_ctx->ipa_wrapper_size;

		imp_smmu_round_to_page(map_info[i].iova, map_info[i].pa,
			map_info[i].size, &iova_p, &pa_p, &size_p);

		if (map) {
			/* boundary check */
			WARN_ON(partition->base > iova_p ||
				(partition->base + partition->size) <
				(iova_p + size_p));

			/* for IPA uC MBOM we need to map with device type */
			if (pa_p - ipa_base < ipa_size)
				prot |= IOMMU_MMIO;

			IMP_DBG("mapping 0x%lx to 0x%pa size %d\n",
				iova_p, &pa_p, size_p);
			iommu_map(domain,
				iova_p, pa_p, size_p, prot);
		} else {
			IMP_DBG("unmapping 0x%lx to 0x%pa size %d\n",
				iova_p, &pa_p, size_p);
			iommu_unmap(domain, iova_p, size_p);
		}
	}
}

static int __imp_configure_mhi_device(
	struct ipa_mhi_alloc_channel_req_msg_v01 *req,
	struct ipa_mhi_alloc_channel_resp_msg_v01 *resp)
{
	struct mhi_buf ch_config[2];
	int i;
	struct ipa_mhi_er_info_type_v01 *er_info;
	struct imp_channel *ch;
	int ridx = 0;
	int ret;

	IMP_FUNC_ENTRY();

	/* configure MHI */
	for (i = 0; i < req->tr_info_arr_len; i++) {
		ch = imp_get_ch_by_id(req->tr_info_arr[i].ch_id);
		if (!ch) {
			IMP_ERR("unknown channel %d\n",
				req->tr_info_arr[i].ch_id);
			resp->alloc_resp_arr[ridx].ch_id =
				req->tr_info_arr[i].ch_id;
			resp->alloc_resp_arr[ridx].is_success = 0;
			ridx++;
			resp->alloc_resp_arr_len = ridx;
			resp->resp.result = IPA_QMI_RESULT_FAILURE_V01;
			resp->resp.error = IPA_QMI_ERR_INVALID_ID_V01;
			return -EINVAL;
		}

		/* populate CCA */
		if (req->tr_info_arr[i].brst_mode_type ==
			QMI_IPA_BURST_MODE_ENABLED_V01)
			ch->props.ch_ctx.brsmode = 3;
		else if (req->tr_info_arr[i].brst_mode_type ==
			QMI_IPA_BURST_MODE_DISABLED_V01)
			ch->props.ch_ctx.brsmode = 2;
		else
			ch->props.ch_ctx.brsmode = 0;

		ch->props.ch_ctx.pollcfg = req->tr_info_arr[i].poll_cfg;
		ch->props.ch_ctx.chtype = ch->props.dir;
		ch->props.ch_ctx.erindex = ch->event.props.id;
		ch->props.ch_ctx.rbase = req->tr_info_arr[i].ring_iova;
		ch->props.ch_ctx.rlen = req->tr_info_arr[i].ring_len;
		ch->props.ch_ctx.rpp = req->tr_info_arr[i].rp;
		ch->props.ch_ctx.wpp = req->tr_info_arr[i].wp;

		ch_config[0].buf = &ch->props.ch_ctx;
		ch_config[0].len = sizeof(ch->props.ch_ctx);
		ch_config[0].name = "CCA";

		/* populate ECA */
		er_info = _find_ch_in_er_info_arr(req, ch->event.props.id);
		if (!er_info) {
			IMP_ERR("no event ring for ch %d\n",
				req->tr_info_arr[i].ch_id);
			resp->alloc_resp_arr[ridx].ch_id =
				req->tr_info_arr[i].ch_id;
			resp->alloc_resp_arr[ridx].is_success = 0;
			ridx++;
			resp->alloc_resp_arr_len = ridx;
			resp->resp.result = IPA_QMI_RESULT_FAILURE_V01;
			resp->resp.error = IPA_QMI_ERR_INTERNAL_V01;
			return -EINVAL;
		}

		ch->event.props.ev_ctx.intmodc = er_info->intmod_count;
		ch->event.props.ev_ctx.intmodt = er_info->intmod_cycles;
		ch->event.props.ev_ctx.ertype = 1;
		ch->event.props.ev_ctx.msivec = er_info->msi_addr;
		ch->event.props.ev_ctx.rbase = er_info->ring_iova;
		ch->event.props.ev_ctx.rlen = er_info->ring_len;
		ch->event.props.ev_ctx.rpp = er_info->rp;
		ch->event.props.ev_ctx.wpp = er_info->wp;
		ch_config[1].buf = &ch->event.props.ev_ctx;
		ch_config[1].len = sizeof(ch->event.props.ev_ctx);
		ch_config[1].name = "ECA";

		IMP_DBG("Configuring MHI device for ch %d\n", ch->props.id);
		ret = mhi_device_configure(imp_ctx->md.mhi_dev, ch->props.dir,
			ch_config, 2);
		if (ret) {
			IMP_ERR("mhi_device_configure failed for ch %d\n",
				req->tr_info_arr[i].ch_id);
			resp->alloc_resp_arr[ridx].ch_id =
				req->tr_info_arr[i].ch_id;
			resp->alloc_resp_arr[ridx].is_success = 0;
			ridx++;
			resp->alloc_resp_arr_len = ridx;
			resp->resp.result = IPA_QMI_RESULT_FAILURE_V01;
			resp->resp.error = IPA_QMI_ERR_INTERNAL_V01;
			return -EINVAL;
		}
	}

	IMP_FUNC_EXIT();

	return 0;
}

/**
 * imp_handle_allocate_channel_req() - Allocate a new MHI channel
 *
 * Allocates MHI channel and start them.
 *
 * Return: QMI return codes
 */
struct ipa_mhi_alloc_channel_resp_msg_v01 *imp_handle_allocate_channel_req(
		struct ipa_mhi_alloc_channel_req_msg_v01 *req)
{
	int ret;
	struct ipa_mhi_alloc_channel_resp_msg_v01 *resp =
		&imp_ctx->qmi.alloc_ch_resp;

	IMP_FUNC_ENTRY();

	mutex_lock(&imp_ctx->mutex);

	memset(resp, 0, sizeof(*resp));

	if (imp_ctx->state != IMP_READY) {
		IMP_ERR("invalid state %d\n", imp_ctx->state);
		resp->resp.result = IPA_QMI_RESULT_FAILURE_V01;
		resp->resp.error = IPA_QMI_ERR_INCOMPATIBLE_STATE_V01;
		mutex_unlock(&imp_ctx->mutex);
		return resp;
	}

	/* cache the req */
	memcpy(&imp_ctx->qmi.alloc_ch_req, req, sizeof(*req));

	if (req->tr_info_arr_len > QMI_IPA_REMOTE_MHI_CHANNELS_NUM_MAX_V01) {
		IMP_ERR("invalid tr_info_arr_len %d\n", req->tr_info_arr_len);
		resp->resp.result = IPA_QMI_RESULT_FAILURE_V01;
		resp->resp.error = IPA_QMI_ERR_NO_MEMORY_V01;
		mutex_unlock(&imp_ctx->mutex);
		return resp;
	}

	if ((req->ctrl_addr_map_info_len == 0 ||
	     req->data_addr_map_info_len == 0) &&
	     imp_ctx->dev_info.smmu_enabled) {
		IMP_ERR("no mapping provided, but smmu is enabled\n");
		resp->resp.result = IPA_QMI_RESULT_FAILURE_V01;
		resp->resp.error = IPA_QMI_ERR_INTERNAL_V01;
		mutex_unlock(&imp_ctx->mutex);
		return resp;
	}

	if (imp_ctx->dev_info.smmu_enabled) {
		/* map CTRL */
		__map_smmu_info(imp_ctx->md.mhi_dev->dev.parent,
			&imp_ctx->dev_info.ctrl,
			req->ctrl_addr_map_info_len,
			req->ctrl_addr_map_info,
			true);

		/* map DATA */
		__map_smmu_info(imp_ctx->md.mhi_dev->dev.parent,
			&imp_ctx->dev_info.data,
			req->data_addr_map_info_len,
			req->data_addr_map_info,
			true);
	}

	resp->alloc_resp_arr_valid = true;
	ret = __imp_configure_mhi_device(req, resp);
	if (ret)
		goto fail_smmu;

	IMP_DBG("Starting MHI channels %d and %d\n",
		imp_ctx->md.ul_chan.props.id,
		imp_ctx->md.dl_chan.props.id);
	ret = mhi_prepare_for_transfer(imp_ctx->md.mhi_dev);
	if (ret) {
		IMP_ERR("mhi_prepare_for_transfer failed %d\n", ret);
		resp->alloc_resp_arr[resp->alloc_resp_arr_len]
			.ch_id = imp_ctx->md.ul_chan.props.id;
		resp->alloc_resp_arr[resp->alloc_resp_arr_len]
			.is_success = 0;
		resp->alloc_resp_arr_len++;
		resp->alloc_resp_arr[resp->alloc_resp_arr_len]
			.ch_id = imp_ctx->md.dl_chan.props.id;
		resp->alloc_resp_arr[resp->alloc_resp_arr_len]
			.is_success = 0;
		resp->alloc_resp_arr_len++;
		resp->resp.result = IPA_QMI_RESULT_FAILURE_V01;
		resp->resp.error = IPA_QMI_ERR_INTERNAL_V01;
		goto fail_smmu;
	}

	resp->alloc_resp_arr[resp->alloc_resp_arr_len]
		.ch_id = imp_ctx->md.ul_chan.props.id;
	resp->alloc_resp_arr[resp->alloc_resp_arr_len]
		.is_success = 1;
	resp->alloc_resp_arr_len++;

	resp->alloc_resp_arr[resp->alloc_resp_arr_len]
		.ch_id = imp_ctx->md.dl_chan.props.id;
	resp->alloc_resp_arr[resp->alloc_resp_arr_len]
		.is_success = 1;
	resp->alloc_resp_arr_len++;

	imp_ctx->state = IMP_STARTED;
	mutex_unlock(&imp_ctx->mutex);
	IMP_FUNC_EXIT();

	resp->resp.result = IPA_QMI_RESULT_SUCCESS_V01;
	return resp;

fail_smmu:
	if (imp_ctx->dev_info.smmu_enabled) {
		/* unmap CTRL */
		__map_smmu_info(imp_ctx->md.mhi_dev->dev.parent,
			&imp_ctx->dev_info.ctrl,
			req->ctrl_addr_map_info_len,
			req->ctrl_addr_map_info,
			false);

		/* unmap DATA */
		__map_smmu_info(imp_ctx->md.mhi_dev->dev.parent,
			&imp_ctx->dev_info.data,
			req->data_addr_map_info_len,
			req->data_addr_map_info,
			false);
	}
	mutex_unlock(&imp_ctx->mutex);
	return resp;
}

/**
 * imp_handle_vote_req() - Votes for MHI / PCIe clocks
 *
 * Hold a vote to prevent / allow low power mode on MHI.
 *
 * Return: 0 on success, negative otherwise
 */
int imp_handle_vote_req(bool vote)
{
	int ret;

	IMP_DBG_LOW("vote %d\n", vote);

	mutex_lock(&imp_ctx->mutex);
	if (imp_ctx->state != IMP_STARTED) {
		IMP_ERR("unexpected vote when in state %d\n", imp_ctx->state);
		mutex_unlock(&imp_ctx->mutex);
		return -EPERM;
	}

	if (vote == imp_ctx->lpm_disabled) {
		IMP_ERR("already voted/devoted %d\n", vote);
		mutex_unlock(&imp_ctx->mutex);
		return -EPERM;
	}
	mutex_unlock(&imp_ctx->mutex);

	/*
	 * Unlock the mutex before calling into mhi for clock vote
	 * to avoid deadlock on imp mutex.
	 * Calls into mhi are synchronous and imp callbacks are
	 * executed from mhi context.
	 */
	if (vote) {
		ret = mhi_device_get_sync(imp_ctx->md.mhi_dev);
		if (ret) {
			IMP_ERR("mhi_sync_get failed %d\n", ret);
			return ret;
		}
	} else {
		mhi_device_put(imp_ctx->md.mhi_dev);
	}

	mutex_lock(&imp_ctx->mutex);
	if (vote)
		imp_ctx->lpm_disabled = true;
	else
		imp_ctx->lpm_disabled = false;
	mutex_unlock(&imp_ctx->mutex);

	return 0;
}

static int imp_read_iova_from_dtsi(const char *node, struct imp_iova_addr *out)
{
	u32 iova_mapping[2];
	struct device_node *of_node = imp_ctx->dev_info.pdev->dev.of_node;

	if (of_property_read_u32_array(of_node, node, iova_mapping, 2)) {
		IMP_DBG("failed to read of_node %s\n", node);
		return -EINVAL;
	}

	out->base = iova_mapping[0];
	out->size = iova_mapping[1];
	IMP_DBG("%s: base: 0x%pad size: 0x%x\n", node, &out->base, out->size);

	return 0;
}

static void imp_mhi_shutdown(void)
{
	struct ipa_mhi_cleanup_req_msg_v01 req = { 0 };

	IMP_FUNC_ENTRY();

	if (imp_ctx->state == IMP_STARTED) {
		req.cleanup_valid = true;
		req.cleanup = true;
		ipa3_qmi_send_mhi_cleanup_request(&req);
		if (imp_ctx->dev_info.smmu_enabled) {
			struct ipa_mhi_alloc_channel_req_msg_v01 *creq
				= &imp_ctx->qmi.alloc_ch_req;

			/* unmap CTRL */
			__map_smmu_info(imp_ctx->md.mhi_dev->dev.parent,
				&imp_ctx->dev_info.ctrl,
				creq->ctrl_addr_map_info_len,
				creq->ctrl_addr_map_info,
				false);

			/* unmap DATA */
			__map_smmu_info(imp_ctx->md.mhi_dev->dev.parent,
				&imp_ctx->dev_info.data,
				creq->data_addr_map_info_len,
				creq->data_addr_map_info,
				false);
		}
		if (imp_ctx->lpm_disabled) {
			mhi_device_put(imp_ctx->md.mhi_dev);
			imp_ctx->lpm_disabled = false;
		}

		/* unmap MHI doorbells from IPA uC SMMU */
		if (!ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC]) {
			struct ipa_smmu_cb_ctx *cb =
				ipa3_get_smmu_ctx(IPA_SMMU_CB_UC);
			unsigned long iova_p;
			phys_addr_t pa_p;
			u32 size_p;

			imp_smmu_round_to_page(imp_ctx->dev_info.chdb_base,
				imp_ctx->dev_info.chdb_base, PAGE_SIZE,
				&iova_p, &pa_p, &size_p);

			iommu_unmap(cb->mapping->domain, iova_p, size_p);
		}
	}
	if (!imp_ctx->in_lpm &&
		(imp_ctx->state == IMP_READY ||
			imp_ctx->state == IMP_STARTED)) {
		IMP_DBG("devote IMP with state= %d\n", imp_ctx->state);
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL("IMP");
	}
	imp_ctx->in_lpm = false;
	imp_ctx->state = IMP_PROBED;

	IMP_FUNC_EXIT();
}

static int imp_mhi_probe_cb(struct mhi_device *mhi_dev,
	const struct mhi_device_id *id)
{
	struct imp_channel *ch;
	struct imp_event *ev;
	int ret;

	IMP_FUNC_ENTRY();

	if (id != &mhi_driver_match_table[0]) {
		IMP_ERR("only chan=%s is supported for now\n",
			mhi_driver_match_table[0].chan);
		return -EPERM;
	}

	/* vote for IPA clock. IPA clock will be devoted when MHI enters LPM */
	IPA_ACTIVE_CLIENTS_INC_SPECIAL("IMP");

	imp_ctx->md.mhi_dev = mhi_dev;

	mutex_lock(&imp_ctx->mutex);
	/* store UL channel properties */
	ch = &imp_ctx->md.ul_chan;
	ev = &imp_ctx->md.ul_chan.event;

	ch->props.id = mhi_dev->ul_chan_id;
	ch->props.dir = DMA_TO_DEVICE;
	ch->props.doorbell = imp_ctx->dev_info.chdb_base + ch->props.id * 8;
	ch->props.uc_mbox_n = IMP_IPA_UC_UL_CH_n;
	IMP_DBG("ul ch id %d doorbell 0x%pa uc_mbox_n %d\n",
		ch->props.id, &ch->props.doorbell, ch->props.uc_mbox_n);

	ret = ipa3_uc_send_remote_ipa_info(ch->props.doorbell,
		ch->props.uc_mbox_n);
	if (ret)
		goto fail;
	IMP_DBG("mapped ch db 0x%pad to mbox %d\n", &ch->props.doorbell,
			ch->props.uc_mbox_n);

	ev->props.id = mhi_dev->ul_event_id;
	ev->props.doorbell = imp_ctx->dev_info.erdb_base + ev->props.id * 8;
	ev->props.uc_mbox_n = IMP_IPA_UC_UL_EV_n;
	IMP_DBG("allocated ev %d\n", ev->props.id);

	ret = ipa3_uc_send_remote_ipa_info(ev->props.doorbell,
		ev->props.uc_mbox_n);
	if (ret)
		goto fail;
	IMP_DBG("mapped ch db 0x%pad to mbox %d\n", &ev->props.doorbell,
		ev->props.uc_mbox_n);

	/* store DL channel properties */
	ch = &imp_ctx->md.dl_chan;
	ev = &imp_ctx->md.dl_chan.event;

	ch->props.dir = DMA_FROM_DEVICE;
	ch->props.id = mhi_dev->dl_chan_id;
	ch->props.doorbell = imp_ctx->dev_info.chdb_base + ch->props.id * 8;
	ch->props.uc_mbox_n = IMP_IPA_UC_DL_CH_n;
	IMP_DBG("dl ch id %d doorbell 0x%pa uc_mbox_n %d\n",
		ch->props.id, &ch->props.doorbell, ch->props.uc_mbox_n);

	ret = ipa3_uc_send_remote_ipa_info(ch->props.doorbell,
		ch->props.uc_mbox_n);
	if (ret)
		goto fail;
	IMP_DBG("mapped ch db 0x%pad to mbox %d\n", &ch->props.doorbell,
		ch->props.uc_mbox_n);

	ev->props.id = mhi_dev->dl_event_id;
	ev->props.doorbell = imp_ctx->dev_info.erdb_base + ev->props.id * 8;
	ev->props.uc_mbox_n = IMP_IPA_UC_DL_EV_n;
	IMP_DBG("allocated ev %d\n", ev->props.id);

	ret = ipa3_uc_send_remote_ipa_info(ev->props.doorbell,
		ev->props.uc_mbox_n);
	if (ret)
		goto fail;
	IMP_DBG("mapped ch db 0x%pad to mbox %d\n", &ev->props.doorbell,
		ev->props.uc_mbox_n);

	/*
	 * Map MHI doorbells to IPA uC SMMU.
	 * Both channel and event doorbells resides in a single page.
	 */
	if (!ipa3_ctx->s1_bypass_arr[IPA_SMMU_CB_UC]) {
		struct ipa_smmu_cb_ctx *cb =
			ipa3_get_smmu_ctx(IPA_SMMU_CB_UC);
		unsigned long iova_p;
		phys_addr_t pa_p;
		u32 size_p;

		imp_smmu_round_to_page(imp_ctx->dev_info.chdb_base,
			imp_ctx->dev_info.chdb_base, PAGE_SIZE,
			&iova_p, &pa_p, &size_p);

		ret = ipa3_iommu_map(cb->mapping->domain, iova_p, pa_p, size_p,
			IOMMU_READ | IOMMU_WRITE | IOMMU_MMIO);
		if (ret)
			goto fail;
	}

	imp_mhi_trigger_ready_ind();

	mutex_unlock(&imp_ctx->mutex);

	IMP_FUNC_EXIT();
	return 0;

fail:
	mutex_unlock(&imp_ctx->mutex);
	IPA_ACTIVE_CLIENTS_DEC_SPECIAL("IMP");
	return ret;
}

static void imp_mhi_remove_cb(struct mhi_device *mhi_dev)
{
	IMP_FUNC_ENTRY();

	mutex_lock(&imp_ctx->mutex);
	imp_mhi_shutdown();
	mutex_unlock(&imp_ctx->mutex);
	IMP_FUNC_EXIT();
}

static void imp_mhi_status_cb(struct mhi_device *mhi_dev, enum MHI_CB mhi_cb)
{
	IMP_DBG("%d\n", mhi_cb);

	mutex_lock(&imp_ctx->lpm_mutex);
	if (mhi_dev != imp_ctx->md.mhi_dev) {
		IMP_DBG("ignoring secondary callbacks\n");
		mutex_unlock(&imp_ctx->lpm_mutex);
		return;
	}

	switch (mhi_cb) {
	case MHI_CB_IDLE:
		break;
	case MHI_CB_LPM_ENTER:
		if (imp_ctx->state == IMP_STARTED) {
			if (!imp_ctx->in_lpm) {
				IPA_ACTIVE_CLIENTS_DEC_SPECIAL("IMP");
				imp_ctx->in_lpm = true;
			} else {
				IMP_ERR("already in LPM\n");
			}
		}
		break;
	case MHI_CB_LPM_EXIT:
		if (imp_ctx->state == IMP_STARTED) {
			if (imp_ctx->in_lpm) {
				IPA_ACTIVE_CLIENTS_INC_SPECIAL("IMP");
				imp_ctx->in_lpm = false;
			} else {
				IMP_ERR("not in LPM\n");
			}
		}
		break;

	case MHI_CB_EE_RDDM:
	case MHI_CB_PENDING_DATA:
		IMP_ERR("unexpected event %d\n", mhi_cb);
		break;
	}
	mutex_unlock(&imp_ctx->lpm_mutex);
}

static int imp_probe(struct platform_device *pdev)
{
	int ret;

	IMP_FUNC_ENTRY();

	if (ipa3_uc_state_check()) {
		IMP_DBG("uC not ready yet\n");
		return -EPROBE_DEFER;
	}

	imp_ctx->dev_info.pdev = pdev;
	imp_ctx->dev_info.smmu_enabled = true;
	ret = imp_read_iova_from_dtsi("qcom,ctrl-iova",
		&imp_ctx->dev_info.ctrl);
	if (ret)
		imp_ctx->dev_info.smmu_enabled = false;

	ret = imp_read_iova_from_dtsi("qcom,data-iova",
		&imp_ctx->dev_info.data);
	if (ret)
		imp_ctx->dev_info.smmu_enabled = false;

	IMP_DBG("smmu_enabled=%d\n", imp_ctx->dev_info.smmu_enabled);

	if (of_property_read_u32(pdev->dev.of_node, "qcom,mhi-chdb-base",
		&imp_ctx->dev_info.chdb_base)) {
		IMP_ERR("failed to read of_node %s\n", "qcom,mhi-chdb-base");
		return -EINVAL;
	}
	IMP_DBG("chdb-base=0x%x\n", imp_ctx->dev_info.chdb_base);

	if (of_property_read_u32(pdev->dev.of_node, "qcom,mhi-erdb-base",
		&imp_ctx->dev_info.erdb_base)) {
		IMP_ERR("failed to read of_node %s\n", "qcom,mhi-erdb-base");
		return -EINVAL;
	}
	IMP_DBG("erdb-base=0x%x\n", imp_ctx->dev_info.erdb_base);

	imp_ctx->state = IMP_PROBED;
	ret = mhi_driver_register(&mhi_driver);
	if (ret) {
		IMP_ERR("mhi_driver_register failed %d\n", ret);
		mutex_unlock(&imp_ctx->mutex);
		return ret;
	}

	IMP_FUNC_EXIT();
	return 0;
}

static int imp_remove(struct platform_device *pdev)
{
	IMP_FUNC_ENTRY();
	mhi_driver_unregister(&mhi_driver);
	mutex_lock(&imp_ctx->mutex);
	if (!imp_ctx->in_lpm && (imp_ctx->state == IMP_READY ||
		imp_ctx->state == IMP_STARTED)) {
		IMP_DBG("devote IMP with state= %d\n", imp_ctx->state);
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL("IMP");
	}
	imp_ctx->lpm_disabled = false;
	imp_ctx->state = IMP_INVALID;
	mutex_unlock(&imp_ctx->mutex);

	mutex_lock(&imp_ctx->lpm_mutex);
	imp_ctx->in_lpm = false;
	mutex_unlock(&imp_ctx->lpm_mutex);

	return 0;
}

static const struct of_device_id imp_dt_match[] = {
	{ .compatible = "qcom,ipa-mhi-proxy" },
	{},
};
MODULE_DEVICE_TABLE(of, imp_dt_match);

static struct platform_driver ipa_mhi_proxy_driver = {
	.driver = {
		.name = "ipa_mhi_proxy",
		.owner = THIS_MODULE,
		.of_match_table = imp_dt_match,
	},
	.probe = imp_probe,
	.remove = imp_remove,
};

/**
 * imp_handle_modem_ready() - Registers IMP as a platform device
 *
 * This function is called after modem is loaded and QMI handshake is done.
 * IMP will register itself as a platform device, and on support device the
 * probe function will get called.
 *
 * Return: None
 */
void imp_handle_modem_ready(void)
{

	if (!imp_ctx) {
		imp_ctx = kzalloc(sizeof(*imp_ctx), GFP_KERNEL);
		if (!imp_ctx)
			return;

		mutex_init(&imp_ctx->mutex);
		mutex_init(&imp_ctx->lpm_mutex);
	}

	if (imp_ctx->state != IMP_INVALID) {
		IMP_ERR("unexpected state %d\n", imp_ctx->state);
		return;
	}

	IMP_DBG("register platform device\n");
	platform_driver_register(&ipa_mhi_proxy_driver);
}

/**
 * imp_handle_modem_shutdown() - Handles modem SSR
 *
 * Performs MHI cleanup when modem is going to SSR (Subsystem Restart).
 *
 * Return: None
 */
void imp_handle_modem_shutdown(void)
{
	IMP_FUNC_ENTRY();

	if (!imp_ctx)
		return;

	mutex_lock(&imp_ctx->mutex);

	if (imp_ctx->state == IMP_INVALID) {
		mutex_unlock(&imp_ctx->mutex);
		return;
	}
	if (imp_ctx->state == IMP_STARTED) {
		mhi_unprepare_from_transfer(imp_ctx->md.mhi_dev);
		imp_ctx->state = IMP_READY;
	}

	if (imp_ctx->state == IMP_READY) {
		if (imp_ctx->dev_info.smmu_enabled) {
			struct ipa_mhi_alloc_channel_req_msg_v01 *creq
				= &imp_ctx->qmi.alloc_ch_req;

			/* unmap CTRL */
			__map_smmu_info(imp_ctx->md.mhi_dev->dev.parent,
				&imp_ctx->dev_info.ctrl,
				creq->ctrl_addr_map_info_len,
				creq->ctrl_addr_map_info,
				false);

			/* unmap DATA */
			__map_smmu_info(imp_ctx->md.mhi_dev->dev.parent,
				&imp_ctx->dev_info.data,
				creq->data_addr_map_info_len,
				creq->data_addr_map_info,
				false);
		}
	}

	imp_ctx->state = IMP_PROBED;
	mutex_unlock(&imp_ctx->mutex);

	IMP_FUNC_EXIT();

	platform_driver_unregister(&ipa_mhi_proxy_driver);
}

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("IPA MHI Proxy Driver");
