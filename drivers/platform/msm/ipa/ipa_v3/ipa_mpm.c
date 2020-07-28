// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mhi.h>
#include <linux/msm_gsi.h>
#include <linux/delay.h>
#include <linux/log2.h>
#include <linux/gfp.h>
#include "../ipa_common_i.h"
#include "ipa_i.h"
#include "ipa_qmi_service.h"

#define IPA_MPM_DRV_NAME "ipa_mpm"

#define IPA_MPM_DBG(fmt, args...) \
	do { \
		pr_debug(IPA_MPM_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
			IPA_MPM_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_MPM_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)

#define IPA_MPM_DBG_LOW(fmt, args...) \
	do { \
		pr_debug(IPA_MPM_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
			IPA_MPM_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)


#define IPA_MPM_ERR(fmt, args...) \
	do { \
		pr_err(IPA_MPM_DRV_NAME " %s:%d " fmt, \
			__func__, __LINE__, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf(), \
				IPA_MPM_DRV_NAME " %s:%d " fmt, ## args); \
		IPA_IPC_LOGGING(ipa_get_ipc_logbuf_low(), \
				IPA_MPM_DRV_NAME " %s:%d " fmt, ## args); \
	} while (0)


#define IPA_MPM_FUNC_ENTRY() \
	IPA_MPM_DBG("ENTRY\n")
#define IPA_MPM_FUNC_EXIT() \
	IPA_MPM_DBG("EXIT\n")

#define IPA_MPM_MAX_MHIP_CHAN 3

#define IPA_MPM_MHI_HOST_UL_CHANNEL 4
#define IPA_MPM_MHI_HOST_DL_CHANNEL  5
#define TETH_AGGR_TIME_LIMIT 1000 /* 1ms */
#define TETH_AGGR_BYTE_LIMIT 24
#define TETH_AGGR_DL_BYTE_LIMIT 16
#define TRE_BUFF_SIZE 32768
#define RNDIS_IPA_DFLT_RT_HDL 0
#define IPA_POLL_FOR_EMPTINESS_NUM 50
#define IPA_POLL_FOR_EMPTINESS_SLEEP_USEC 20
#define IPA_CHANNEL_STOP_IN_PROC_TO_MSEC 5
#define IPA_CHANNEL_STOP_IN_PROC_SLEEP_USEC 200
#define IPA_MHIP_HOLB_TMO 31 /* value to match granularity on ipa HW 4.5 */
#define IPA_MPM_FLOW_CTRL_ADD 1
#define IPA_MPM_FLOW_CTRL_DELETE 0
#define IPA_MPM_NUM_OF_INIT_CMD_DESC 2
#define IPA_UC_FC_DB_ADDR 0x1EC2088
#define IPA_MAX_BW_REG_DEREG_CACHE 20

enum bw_reg_dereg_type {
	BW_VOTE_WAN_NOTIFY = 0,
	BW_UNVOTE_WAN_NOTIFY = 1,
	BW_VOTE_PROBE_CB = 2,
	BW_VOTE_XDCI_ENABLE = 3,
	BW_UNVOTE_XDCI_DISABLE = 4,
};

enum mhip_re_type {
	MHIP_RE_XFER = 0x2,
	MHIP_RE_NOP = 0x4,
};

enum ipa_mpm_mhi_ch_id_type {
	IPA_MPM_MHIP_CH_ID_0,
	IPA_MPM_MHIP_CH_ID_1,
	IPA_MPM_MHIP_CH_ID_2,
	IPA_MPM_MHIP_CH_ID_MAX,
};

enum ipa_mpm_dma_data_direction {
	DMA_HIPA_BIDIRECTIONAL = 0,
	DMA_TO_HIPA = 1,
	DMA_FROM_HIPA = 2,
	DMA_HIPA_NONE = 3,
};

enum ipa_mpm_ipa_teth_client_type {
	IPA_MPM_MHIP_USB,
	IPA_MPM_MHIP_WIFI,
};

enum ipa_mpm_mhip_client_type {
	IPA_MPM_MHIP_INIT,
	/* USB RMNET CLIENT */
	IPA_MPM_MHIP_USB_RMNET,
	/* USB RNDIS / WIFI CLIENT */
	IPA_MPM_MHIP_TETH,
	/* USB DPL CLIENT */
	IPA_MPM_MHIP_USB_DPL,
	IPA_MPM_MHIP_NONE,
};

enum ipa_mpm_clk_vote_type {
	CLK_ON,
	CLK_OFF,
};

enum mhip_status_type {
	MHIP_STATUS_SUCCESS,
	MHIP_STATUS_NO_OP,
	MHIP_STATUS_FAIL,
	MHIP_STATUS_BAD_STATE,
	MHIP_STATUS_EP_NOT_FOUND,
	MHIP_STATUS_EP_NOT_READY,
};

enum mhip_smmu_domain_type {
	MHIP_SMMU_DOMAIN_IPA,
	MHIP_SMMU_DOMAIN_PCIE,
	MHIP_SMMU_DOMAIN_NONE,
};

enum ipa_mpm_start_stop_type {
	MPM_MHIP_STOP,
	MPM_MHIP_START,
};
/* each pair of UL/DL channels are defined below */
static const struct mhi_device_id mhi_driver_match_table[] = {
	{ .chan = "IP_HW_MHIP_0" }, /* for rndis/Wifi teth pipes */
	{ .chan = "IP_HW_MHIP_1" }, /* for MHIP rmnet */
	{ .chan = "IP_HW_ADPL" }, /* ADPL/ODL DL pipe */
};

static const char *ipa_mpm_mhip_chan_str[IPA_MPM_MHIP_CH_ID_MAX] = {
	__stringify(IPA_MPM_MHIP_TETH),
	__stringify(IPA_MPM_MHIP_USB_RMNET),
	__stringify(IPA_MPM_MHIP_USB_DPL),
};
/*
 * MHI PRIME GSI Descriptor format that Host IPA uses.
 */
struct __packed mhi_p_desc {
	uint64_t buffer_ptr;
	uint16_t buff_len;
	uint16_t resvd1;
	uint16_t chain : 1;
	uint16_t resvd4 : 7;
	uint16_t ieob : 1;
	uint16_t ieot : 1;
	uint16_t bei : 1;
	uint16_t sct : 1;
	uint16_t resvd3 : 4;
	uint8_t re_type;
	uint8_t resvd2;
};

/*
 * MHI PRIME Channel Context and Event Context Array
 * Information that is sent to Device IPA.
 */
struct ipa_mpm_channel_context_type {
	u32 chstate : 8;
	u32 reserved1 : 24;
	u32 chtype;
	u32 erindex;
	u64 rbase;
	u64 rlen;
	u64 reserved2;
	u64 reserved3;
} __packed;

struct ipa_mpm_event_context_type {
	u32 reserved1 : 8;
	u32 update_rp_modc : 8;
	u32 update_rp_intmodt : 16;
	u32 ertype;
	u32 update_rp_addr;
	u64 rbase;
	u64 rlen;
	u32 buff_size : 16;
	u32 reserved2 : 16;
	u32 reserved3;
	u64 reserved4;
} __packed;

struct ipa_mpm_pipes_info_type {
	enum ipa_client_type ipa_client;
	struct ipa_ep_cfg ep_cfg;
};

struct ipa_mpm_channel_type {
	struct ipa_mpm_pipes_info_type dl_cons;
	struct ipa_mpm_pipes_info_type ul_prod;
	enum ipa_mpm_mhip_client_type mhip_client;
};

static struct ipa_mpm_channel_type ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_MAX];

/* For configuring IPA_CLIENT_MHI_PRIME_TETH_CONS */
static struct ipa_ep_cfg mhip_dl_teth_ep_cfg = {
	.mode = {
		.mode = IPA_BASIC,
		.dst = IPA_CLIENT_MHI_PRIME_TETH_CONS,
	},
	.hdr = {
		.hdr_len = 4,
		.hdr_ofst_metadata_valid = 1,
		.hdr_ofst_metadata = 1,
		.hdr_ofst_pkt_size_valid = 1,
		.hdr_ofst_pkt_size = 2,
	},
	.hdr_ext = {
		.hdr_total_len_or_pad_valid = true,
		.hdr_payload_len_inc_padding = true,
	},
	.aggr = {
		.aggr_en = IPA_ENABLE_DEAGGR,
		.aggr = IPA_QCMAP,
		.aggr_byte_limit = TETH_AGGR_DL_BYTE_LIMIT,
		.aggr_time_limit = TETH_AGGR_TIME_LIMIT,
	},
};

static struct ipa_ep_cfg mhip_ul_teth_ep_cfg = {
	.mode = {
		.mode = IPA_BASIC,
		.dst = IPA_CLIENT_MHI_PRIME_TETH_PROD,
	},
	.hdr = {
		.hdr_len = 4,
		.hdr_ofst_metadata_valid = 1,
		.hdr_ofst_metadata = 0,
		.hdr_ofst_pkt_size_valid = 1,
		.hdr_ofst_pkt_size = 2,
	},
	.hdr_ext = {
		.hdr_total_len_or_pad_valid = true,
		.hdr_payload_len_inc_padding = true,
	},
	.aggr = {
		.aggr_en = IPA_ENABLE_AGGR,
		.aggr = IPA_QCMAP,
		.aggr_byte_limit = TETH_AGGR_BYTE_LIMIT,
		.aggr_time_limit = TETH_AGGR_TIME_LIMIT,
	},

};

/* WARNING!! Temporary for rndis intgration only */


/* For configuring IPA_CLIENT_MHIP_RMNET_PROD */
static struct ipa_ep_cfg mhip_dl_rmnet_ep_cfg = {
	.mode = {
		.mode = IPA_DMA,
		.dst = IPA_CLIENT_USB_CONS,
	},
};

/* For configuring IPA_CLIENT_MHIP_RMNET_CONS */
static struct ipa_ep_cfg mhip_ul_rmnet_ep_cfg = {
	.mode = {
		.mode = IPA_DMA,
		.dst = IPA_CLIENT_USB_CONS,
	},
};

/* For configuring IPA_CLIENT_MHIP_DPL_PROD using USB*/
static struct ipa_ep_cfg mhip_dl_dpl_ep_cfg = {
	.mode = {
		.mode = IPA_DMA,
		.dst = IPA_CLIENT_USB_DPL_CONS,
	},
};


struct ipa_mpm_iova_addr {
	dma_addr_t base;
	unsigned int size;
};

struct ipa_mpm_dev_info {
	struct platform_device *pdev;
	struct device *dev;
	bool ipa_smmu_enabled;
	bool pcie_smmu_enabled;
	struct ipa_mpm_iova_addr ctrl;
	struct ipa_mpm_iova_addr data;
	u32 chdb_base;
	u32 erdb_base;
	bool is_cache_coherent;
};

struct ipa_mpm_event_props {
	u16 id;
	phys_addr_t device_db;
	struct ipa_mpm_event_context_type ev_ctx;
};

struct ipa_mpm_channel_props {
	u16 id;
	phys_addr_t device_db;
	struct ipa_mpm_channel_context_type ch_ctx;
};

enum ipa_mpm_gsi_state {
	GSI_ERR,
	GSI_INIT,
	GSI_ALLOCATED,
	GSI_STARTED,
	GSI_STOPPED,
};

enum ipa_mpm_remote_state {
	MPM_MHIP_REMOTE_STOP,
	MPM_MHIP_REMOTE_START,
	MPM_MHIP_REMOTE_ERR,
};

struct ipa_mpm_channel {
	struct ipa_mpm_channel_props chan_props;
	struct ipa_mpm_event_props evt_props;
	enum ipa_mpm_gsi_state gsi_state;
	dma_addr_t db_host_iova;
	dma_addr_t db_device_iova;
};

enum ipa_mpm_teth_state {
	IPA_MPM_TETH_INIT = 0,
	IPA_MPM_TETH_INPROGRESS,
	IPA_MPM_TETH_CONNECTED,
};

enum ipa_mpm_mhip_chan {
	IPA_MPM_MHIP_CHAN_UL,
	IPA_MPM_MHIP_CHAN_DL,
	IPA_MPM_MHIP_CHAN_BOTH,
};

struct ipa_mpm_clk_cnt_type {
	atomic_t pcie_clk_cnt;
	atomic_t ipa_clk_cnt;
};

struct producer_rings {
	struct mhi_p_desc *tr_va;
	struct mhi_p_desc *er_va;
	void *tr_buff_va[IPA_MPM_MAX_RING_LEN];
	dma_addr_t tr_pa;
	dma_addr_t er_pa;
	dma_addr_t tr_buff_c_iova[IPA_MPM_MAX_RING_LEN];
	/*
	 * The iova generated for AP CB,
	 * used only for dma_map_single to flush the cache.
	 */
	dma_addr_t ap_iova_er;
	dma_addr_t ap_iova_tr;
	dma_addr_t ap_iova_buff[IPA_MPM_MAX_RING_LEN];
};

struct ipa_mpm_mhi_driver {
	struct mhi_device *mhi_dev;
	struct producer_rings ul_prod_ring;
	struct producer_rings dl_prod_ring;
	struct ipa_mpm_channel ul_prod;
	struct ipa_mpm_channel dl_cons;
	enum ipa_mpm_mhip_client_type mhip_client;
	enum ipa_mpm_teth_state teth_state;
	bool init_complete;
	/* General MPM mutex to protect concurrent update of MPM GSI states */
	struct mutex mutex;
	/*
	 * Mutex to protect mhi_dev update/ access, for concurrency such as
	 * 5G SSR and USB disconnect/connect.
	 */
	struct mutex mhi_mutex;
	bool in_lpm;
	struct ipa_mpm_clk_cnt_type clk_cnt;
	enum ipa_mpm_remote_state remote_state;
};

struct bw_cache {
	int bw_reg_dereg_type;
	int ref_count;
};

struct ipa_mpm_context {
	struct ipa_mpm_dev_info dev_info;
	struct ipa_mpm_mhi_driver md[IPA_MPM_MAX_MHIP_CHAN];
	struct mutex mutex;
	atomic_t probe_cnt;
	atomic_t pcie_clk_total_cnt;
	atomic_t ipa_clk_total_cnt;
	atomic_t flow_ctrl_mask;
	atomic_t adpl_over_usb_available;
	atomic_t adpl_over_odl_available;
	atomic_t active_teth_count;
	atomic_t voted_before;
	struct bw_cache bw_reg_dereg_cache[IPA_MAX_BW_REG_DEREG_CACHE];
	int cache_index;
	struct device *parent_pdev;
	struct ipa_smmu_cb_ctx carved_smmu_cb;
	struct device *mhi_parent_dev;
	/* for ipa_uc_fc_db*/
	phys_addr_t uc_fc_db;
	unsigned long uc_fc_db_iova;
};

#define IPA_MPM_DESC_SIZE (sizeof(struct mhi_p_desc))
/* WA: Make the IPA_MPM_PAGE_SIZE from 16k (next power of ring size) to
 * 32k. This is to make sure IOMMU map happens for the same size
 * for all TR/ER and doorbells.
 */
#define IPA_MPM_PAGE_SIZE TRE_BUFF_SIZE


static struct ipa_mpm_context *ipa_mpm_ctx;
static struct platform_device *m_pdev;
static int ipa_mpm_mhi_probe_cb(struct mhi_device *,
	const struct mhi_device_id *);
static void ipa_mpm_mhi_remove_cb(struct mhi_device *);
static void ipa_mpm_mhi_status_cb(struct mhi_device *, enum MHI_CB);
static void ipa_mpm_change_teth_state(int probe_id,
	enum ipa_mpm_teth_state ip_state);
static void ipa_mpm_change_gsi_state(int probe_id,
	enum ipa_mpm_mhip_chan mhip_chan,
	enum ipa_mpm_gsi_state next_state);
static int ipa_mpm_probe(struct platform_device *pdev);
static int ipa_mpm_vote_unvote_pcie_clk(enum ipa_mpm_clk_vote_type vote,
	int probe_id, bool is_force, bool *is_acted);
static void ipa_mpm_vote_unvote_ipa_clk(enum ipa_mpm_clk_vote_type vote,
	int probe_id);
static enum mhip_status_type ipa_mpm_start_stop_mhip_chan(
	enum ipa_mpm_mhip_chan mhip_chan,
	int probe_id,
	enum ipa_mpm_start_stop_type start_stop);
static int ipa_mpm_start_mhip_holb_tmo(u32 clnt_hdl);

static struct mhi_driver mhi_driver = {
	.id_table = mhi_driver_match_table,
	.probe = ipa_mpm_mhi_probe_cb,
	.remove = ipa_mpm_mhi_remove_cb,
	.status_cb = ipa_mpm_mhi_status_cb,
	.driver = {
		.name = IPA_MPM_DRV_NAME,
		.owner = THIS_MODULE,
	},
};

static void ipa_mpm_ipa3_delayed_probe(struct work_struct *work)
{
	(void)ipa_mpm_probe(m_pdev);
}

static DECLARE_WORK(ipa_mpm_ipa3_scheduled_probe, ipa_mpm_ipa3_delayed_probe);

static void ipa_mpm_ipa3_ready_cb(void *user_data)
{
	struct platform_device *pdev = (struct platform_device *)(user_data);

	m_pdev = pdev;

	IPA_MPM_DBG("IPA ready callback has been triggered\n");

	schedule_work(&ipa_mpm_ipa3_scheduled_probe);
}

static void ipa_mpm_gsi_evt_ring_err_cb(struct gsi_evt_err_notify *err_data)
{
	IPA_MPM_ERR("GSI EVT RING ERROR, not expected..\n");
	ipa_assert();
}

static void ipa_mpm_gsi_chan_err_cb(struct gsi_chan_err_notify *err_data)
{
	IPA_MPM_ERR("GSI CHAN ERROR, not expected..\n");
	ipa_assert();
}

static int ipa_mpm_set_dma_mode(enum ipa_client_type src_pipe,
	enum ipa_client_type dst_pipe, bool reset)
{
	struct ipahal_imm_cmd_pyld *cmd_pyld[IPA_MPM_NUM_OF_INIT_CMD_DESC];
	struct ipahal_imm_cmd_register_write reg_write_coal_close;
	struct ipahal_reg_valmask valmask;
	struct ipa3_desc desc[IPA_MPM_NUM_OF_INIT_CMD_DESC];
	int i, num_cmd = 0, result = 0;
	struct ipa_ep_cfg ep_cfg = { { 0 } };

	IPA_MPM_FUNC_ENTRY();
	IPA_MPM_DBG("DMA from %d to %d reset=%d\n", src_pipe, dst_pipe, reset);

	memset(desc, 0, sizeof(desc));
	memset(cmd_pyld, 0, sizeof(cmd_pyld));

	/* First step is to clear IPA Pipeline before changing DMA mode */
	if (ipa3_get_ep_mapping(src_pipe) != IPA_EP_NOT_ALLOCATED) {
		i = ipa3_get_ep_mapping(src_pipe);
		reg_write_coal_close.skip_pipeline_clear = false;
		reg_write_coal_close.pipeline_clear_options = IPAHAL_HPS_CLEAR;
		reg_write_coal_close.offset = ipahal_get_reg_ofst(
			IPA_AGGR_FORCE_CLOSE);
		ipahal_get_aggr_force_close_valmask(i, &valmask);
		reg_write_coal_close.value = valmask.val;
		reg_write_coal_close.value_mask = valmask.mask;
		cmd_pyld[num_cmd] = ipahal_construct_imm_cmd(
					IPA_IMM_CMD_REGISTER_WRITE,
					&reg_write_coal_close, false);

		if (!cmd_pyld[num_cmd]) {
			IPA_MPM_ERR("failed to construct coal close IC\n");
			result = -ENOMEM;
			goto destroy_imm_cmd;
		}
		ipa3_init_imm_cmd_desc(&desc[num_cmd], cmd_pyld[num_cmd]);
		++num_cmd;
	}
	/* NO-OP IC for ensuring that IPA pipeline is empty */
	cmd_pyld[num_cmd] =
		ipahal_construct_nop_imm_cmd(false, IPAHAL_HPS_CLEAR, false);
	if (!cmd_pyld[num_cmd]) {
		IPA_MPM_ERR("failed to construct NOP imm cmd\n");
		result = -ENOMEM;
		goto destroy_imm_cmd;
	}

	result = ipa3_send_cmd(num_cmd, desc);
	if (result) {
		IPAERR("fail to send Reset Pipeline immediate command\n");
		goto destroy_imm_cmd;
	}

	/* Reset to basic if reset = 1, otherwise set to DMA */
	if (reset)
		ep_cfg.mode.mode = IPA_BASIC;
	else
		ep_cfg.mode.mode = IPA_DMA;
	ep_cfg.mode.dst = dst_pipe;
	ep_cfg.seq.set_dynamic = true;

	result = ipa_cfg_ep(ipa_get_ep_mapping(src_pipe), &ep_cfg);
	IPA_MPM_FUNC_EXIT();

destroy_imm_cmd:
	for (i = 0; i < num_cmd; ++i)
		ipahal_destroy_imm_cmd(cmd_pyld[i]);

	return result;
}

static int ipa_mpm_start_mhip_holb_tmo(u32 clnt_hdl)
{
	struct ipa_ep_cfg_holb holb_cfg;

	memset(&holb_cfg, 0, sizeof(holb_cfg));
	holb_cfg.en = IPA_HOLB_TMR_EN;
	/* 31 ms timer, which is less than tag timeout */
	holb_cfg.tmr_val = IPA_MHIP_HOLB_TMO;
	return ipa3_cfg_ep_holb(clnt_hdl, &holb_cfg);
}

/**
 * ipa_mpm_smmu_map() - SMMU maps ring and the buffer pointer.
 * @va_addr: virtual address that needs to be mapped
 * @sz: size of the address to be mapped
 * @dir: ipa_mpm_dma_data_direction
 * @ap_cb_iova: iova for AP context bank
 *
 * This function SMMU maps both ring and the buffer pointer.
 * The ring pointers will be aligned to ring size and
 * the buffer pointers should be aligned to buffer size.
 *
 * Returns: iova of the mapped address
 */
static dma_addr_t ipa_mpm_smmu_map(void *va_addr,
	int sz,
	int dir,
	dma_addr_t *ap_cb_iova)
{
	struct iommu_domain *ipa_smmu_domain, *pcie_smmu_domain;
	phys_addr_t phys_addr;
	dma_addr_t iova;
	int smmu_enabled;
	unsigned long iova_p;
	phys_addr_t pa_p;
	u32 size_p;
	int prot = IOMMU_READ | IOMMU_WRITE;
	struct ipa_smmu_cb_ctx *cb = &ipa_mpm_ctx->carved_smmu_cb;
	unsigned long carved_iova = roundup(cb->next_addr, IPA_MPM_PAGE_SIZE);
	int ret = 0;

	/* check cache coherent */
	if (ipa_mpm_ctx->dev_info.is_cache_coherent)  {
		IPA_MPM_DBG_LOW("enable cache coherent\n");
		prot |= IOMMU_CACHE;
	}

	if (carved_iova >= cb->va_end) {
		IPA_MPM_ERR("running out of carved_iova %lx\n", carved_iova);
		ipa_assert();
	}
	/*
	 * Both Host IPA and PCIE SMMU should be enabled or disabled
	 * for proceed.
	 * If SMMU Enabled => iova == pa
	 * If SMMU Disabled => iova == iommu mapped iova
	 * dma_map_single ensures cache is flushed and the memory is not
	 * touched again until dma_unmap_single() is called
	 */
	smmu_enabled = (ipa_mpm_ctx->dev_info.ipa_smmu_enabled &&
		ipa_mpm_ctx->dev_info.pcie_smmu_enabled) ? 1 : 0;

	if (smmu_enabled) {
		/* Map the phys addr to both PCIE and IPA AP CB
		 * from the carved out common iova range.
		 */
		ipa_smmu_domain = ipa3_get_smmu_domain();

		if (!ipa_smmu_domain) {
			IPA_MPM_ERR("invalid IPA smmu domain\n");
			ipa_assert();
		}

		if (!ipa_mpm_ctx->mhi_parent_dev) {
			IPA_MPM_ERR("invalid PCIE SMMU domain\n");
			ipa_assert();
		}

		phys_addr = virt_to_phys((void *) va_addr);

		IPA_SMMU_ROUND_TO_PAGE(carved_iova, phys_addr, sz,
					iova_p, pa_p, size_p);

	/* Flush the cache with dma_map_single for IPA AP CB */
		*ap_cb_iova = dma_map_single(ipa3_ctx->pdev, va_addr,
					size_p, dir);

		if (dma_mapping_error(ipa3_ctx->pdev, *ap_cb_iova)) {
			IPA_MPM_ERR("dma_map_single failure for entry\n");
			goto fail_dma_mapping;
		}

		ret = ipa3_iommu_map(ipa_smmu_domain, iova_p,
					pa_p, size_p, prot);
		if (ret) {
			IPA_MPM_ERR("IPA IOMMU returned failure, ret = %d\n",
					ret);
			ipa_assert();
		}

		pcie_smmu_domain = iommu_get_domain_for_dev(
			ipa_mpm_ctx->mhi_parent_dev);
		if (!pcie_smmu_domain) {
			IPA_MPM_ERR("invalid pcie smmu domain\n");
			ipa_assert();
		}
		ret = iommu_map(pcie_smmu_domain, iova_p, pa_p, size_p, prot);

		if (ret) {
			IPA_MPM_ERR("PCIe IOMMU returned failure, ret = %d\n",
				ret);
			ipa_assert();
		}

		cb->next_addr = iova_p + size_p;
		iova = iova_p;
	} else {
		if (dir == DMA_TO_HIPA)
			iova = dma_map_single(ipa3_ctx->pdev, va_addr,
				ipa3_ctx->mpm_ring_size_dl *
				IPA_MPM_DESC_SIZE, dir);
		else
			iova = dma_map_single(ipa3_ctx->pdev, va_addr,
				ipa3_ctx->mpm_ring_size_ul *
				IPA_MPM_DESC_SIZE, dir);

		if (dma_mapping_error(ipa3_ctx->pdev, iova)) {
			IPA_MPM_ERR("dma_map_single failure for entry\n");
			goto fail_dma_mapping;
		}

		*ap_cb_iova = iova;
	}
	return iova;

fail_dma_mapping:
	iova = 0;
	ipa_assert();
	return iova;
}

/**
 * ipa_mpm_smmu_unmap() - SMMU unmaps ring and the buffer pointer.
 * @va_addr: virtual address that needs to be mapped
 * @sz: size of the address to be mapped
 * @dir: ipa_mpm_dma_data_direction
 * @ap_cb_iova: iova for AP context bank
 *
 * This function SMMU unmaps both ring and the buffer pointer.
 * The ring pointers will be aligned to ring size and
 * the buffer pointers should be aligned to buffer size.
 *
 * Return: none
 */
static void ipa_mpm_smmu_unmap(dma_addr_t carved_iova, int sz, int dir,
	dma_addr_t ap_cb_iova)
{
	unsigned long iova_p;
	unsigned long pa_p;
	u32 size_p = 0;
	struct iommu_domain *ipa_smmu_domain, *pcie_smmu_domain;
	struct ipa_smmu_cb_ctx *cb = &ipa_mpm_ctx->carved_smmu_cb;
	int smmu_enabled = (ipa_mpm_ctx->dev_info.ipa_smmu_enabled &&
		ipa_mpm_ctx->dev_info.pcie_smmu_enabled) ? 1 : 0;

	if (carved_iova <= 0) {
		IPA_MPM_ERR("carved_iova is zero/negative\n");
		return;
	}

	if (smmu_enabled) {
		ipa_smmu_domain = ipa3_get_smmu_domain();
		if (!ipa_smmu_domain) {
			IPA_MPM_ERR("invalid IPA smmu domain\n");
			ipa_assert();
		}

		if (!ipa_mpm_ctx->mhi_parent_dev) {
			IPA_MPM_ERR("invalid PCIE SMMU domain\n");
			ipa_assert();
		}

		IPA_SMMU_ROUND_TO_PAGE(carved_iova, carved_iova, sz,
			iova_p, pa_p, size_p);
		pcie_smmu_domain = iommu_get_domain_for_dev(
			ipa_mpm_ctx->mhi_parent_dev);
		if (pcie_smmu_domain) {
			iommu_unmap(pcie_smmu_domain, iova_p, size_p);
		} else {
			IPA_MPM_ERR("invalid PCIE SMMU domain\n");
			ipa_assert();
		}
		iommu_unmap(ipa_smmu_domain, iova_p, size_p);

		cb->next_addr -= size_p;
		dma_unmap_single(ipa3_ctx->pdev, ap_cb_iova,
			size_p, dir);
	} else {
		if (dir == DMA_TO_HIPA)
			dma_unmap_single(ipa3_ctx->pdev, ap_cb_iova,
				ipa3_ctx->mpm_ring_size_dl *
				IPA_MPM_DESC_SIZE, dir);
		else
			dma_unmap_single(ipa3_ctx->pdev, ap_cb_iova,
				ipa3_ctx->mpm_ring_size_ul *
				IPA_MPM_DESC_SIZE, dir);
	}
}

static u32 ipa_mpm_smmu_map_doorbell(enum mhip_smmu_domain_type smmu_domain,
	u32 pa_addr)
{
	/*
	 * Doorbells are already in PA, map these to
	 * PCIE/IPA doman if SMMUs are enabled.
	 */
	struct iommu_domain *ipa_smmu_domain, *pcie_smmu_domain;
	int smmu_enabled;
	unsigned long iova_p;
	phys_addr_t pa_p;
	u32 size_p;
	int ret = 0;
	int prot = IOMMU_READ | IOMMU_WRITE;
	struct ipa_smmu_cb_ctx *cb = &ipa_mpm_ctx->carved_smmu_cb;
	unsigned long carved_iova = roundup(cb->next_addr, IPA_MPM_PAGE_SIZE);
	u32 iova = 0;
	u64 offset = 0;

	/* check cache coherent */
	if (ipa_mpm_ctx->dev_info.is_cache_coherent)  {
		IPA_MPM_DBG(" enable cache coherent\n");
		prot |= IOMMU_CACHE;
	}

	if (carved_iova >= cb->va_end) {
		IPA_MPM_ERR("running out of carved_iova %lx\n", carved_iova);
		ipa_assert();
	}

	smmu_enabled = (ipa_mpm_ctx->dev_info.ipa_smmu_enabled &&
		ipa_mpm_ctx->dev_info.pcie_smmu_enabled) ? 1 : 0;

	if (smmu_enabled) {
		IPA_SMMU_ROUND_TO_PAGE(carved_iova, pa_addr, IPA_MPM_PAGE_SIZE,
					iova_p, pa_p, size_p);
		if (smmu_domain == MHIP_SMMU_DOMAIN_IPA) {
			ipa_smmu_domain = ipa3_get_smmu_domain();
			if (!ipa_smmu_domain) {
				IPA_MPM_ERR("invalid IPA smmu domain\n");
				ipa_assert();
			}
			ret = ipa3_iommu_map(ipa_smmu_domain,
				iova_p, pa_p, size_p, prot);
			if (ret) {
				IPA_MPM_ERR("IPA doorbell mapping failed\n");
				ipa_assert();
			}
			offset = pa_addr - pa_p;
		} else if (smmu_domain == MHIP_SMMU_DOMAIN_PCIE) {
			pcie_smmu_domain = iommu_get_domain_for_dev(
				ipa_mpm_ctx->mhi_parent_dev);
			if (!pcie_smmu_domain) {
				IPA_MPM_ERR("invalid IPA smmu domain\n");
				ipa_assert();
			}
			ret = iommu_map(pcie_smmu_domain,
					iova_p, pa_p, size_p, prot);
			if (ret) {
				IPA_MPM_ERR("PCIe doorbell mapping failed\n");
				ipa_assert();
			}
			offset = pa_addr - pa_p;
		}
		iova = iova_p + offset;
		cb->next_addr = iova_p + IPA_MPM_PAGE_SIZE;
	} else {
		iova = pa_addr;
	}
	return iova;
}

static void ipa_mpm_smmu_unmap_doorbell(enum mhip_smmu_domain_type smmu_domain,
	dma_addr_t iova)
{
	/*
	 * Doorbells are already in PA, map these to
	 * PCIE/IPA doman if SMMUs are enabled.
	 */
	struct iommu_domain *ipa_smmu_domain, *pcie_smmu_domain;
	int smmu_enabled;
	unsigned long iova_p;
	phys_addr_t pa_p;
	u32 size_p;
	struct ipa_smmu_cb_ctx *cb = &ipa_mpm_ctx->carved_smmu_cb;

	smmu_enabled = (ipa_mpm_ctx->dev_info.ipa_smmu_enabled &&
		ipa_mpm_ctx->dev_info.pcie_smmu_enabled) ? 1 : 0;

	if (smmu_enabled) {
		IPA_SMMU_ROUND_TO_PAGE(iova, iova, IPA_MPM_PAGE_SIZE,
					iova_p, pa_p, size_p);
		if (smmu_domain == MHIP_SMMU_DOMAIN_IPA) {
			ipa_smmu_domain = ipa3_get_smmu_domain();
			if (ipa_smmu_domain) {
				iommu_unmap(ipa_smmu_domain, iova_p, size_p);
			} else {
				IPA_MPM_ERR("invalid IPA smmu domain\n");
				ipa_assert();
			}
		} else if (smmu_domain == MHIP_SMMU_DOMAIN_PCIE) {
			pcie_smmu_domain = iommu_get_domain_for_dev(
				ipa_mpm_ctx->mhi_parent_dev);
			if (pcie_smmu_domain) {
				iommu_unmap(pcie_smmu_domain, iova_p, size_p);
			} else {
				IPA_MPM_ERR("invalid PCIE smmu domain\n");
				ipa_assert();
			}
			cb->next_addr -=  IPA_MPM_PAGE_SIZE;
		}
	}
}
static int get_idx_from_id(const struct mhi_device_id *id)
{
	return (id - mhi_driver_match_table);
}

static void get_ipa3_client(int id,
	enum ipa_client_type *ul_prod,
	enum ipa_client_type *dl_cons)
{
	IPA_MPM_FUNC_ENTRY();

	if (id >= IPA_MPM_MHIP_CH_ID_MAX) {
		*ul_prod = IPA_CLIENT_MAX;
		*dl_cons = IPA_CLIENT_MAX;
	} else {
		*ul_prod = ipa_mpm_pipes[id].ul_prod.ipa_client;
		*dl_cons = ipa_mpm_pipes[id].dl_cons.ipa_client;
	}
	IPA_MPM_FUNC_EXIT();
}

static int ipa_mpm_connect_mhip_gsi_pipe(enum ipa_client_type mhip_client,
	int mhi_idx, struct ipa_req_chan_out_params *out_params)
{
	int ipa_ep_idx;
	int res;
	struct mhi_p_desc *er_ring_va, *tr_ring_va;
	void *buff_va;
	dma_addr_t er_carved_iova, tr_carved_iova;
	dma_addr_t ap_cb_tr_iova, ap_cb_er_iova, ap_cb_buff_iova;
	struct ipa_request_gsi_channel_params gsi_params;
	int dir;
	int i, k;
	int result;
	struct ipa3_ep_context *ep;
	int ring_size;

	if (mhip_client == IPA_CLIENT_MAX)
		goto fail_gen;

	if ((mhi_idx < IPA_MPM_MHIP_CH_ID_0) ||
		(mhi_idx >= IPA_MPM_MHIP_CH_ID_MAX))
		goto fail_gen;

	ipa_ep_idx = ipa3_get_ep_mapping(mhip_client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPA_MPM_ERR("fail to find channel EP.\n");
		goto fail_gen;
	}
	ep = &ipa3_ctx->ep[ipa_ep_idx];
	if (ep->valid == 1) {
		IPAERR("EP %d already allocated.\n", ipa_ep_idx);
		return 0;
	}

	IPA_MPM_DBG("connecting client %d (ep: %d)\n", mhip_client, ipa_ep_idx);

	IPA_MPM_FUNC_ENTRY();

	if (IPA_CLIENT_IS_PROD(mhip_client) &&
		(ipa3_ctx->mpm_ring_size_dl *
			IPA_MPM_DESC_SIZE > PAGE_SIZE)) {
		IPA_MPM_ERR("Ring Size dl / allocation mismatch\n");
		ipa_assert();
	}

	if (IPA_CLIENT_IS_PROD(mhip_client) &&
		(ipa3_ctx->mpm_ring_size_ul *
			IPA_MPM_DESC_SIZE > PAGE_SIZE)) {
		IPA_MPM_ERR("Ring Size ul / allocation mismatch\n");
		ipa_assert();
	}

	/* Only ring need alignment, separate from buffer */
	er_ring_va = (struct mhi_p_desc *) get_zeroed_page(GFP_KERNEL);

	if (!er_ring_va)
		goto fail_evt_alloc;

	tr_ring_va = (struct mhi_p_desc *) get_zeroed_page(GFP_KERNEL);

	if (!tr_ring_va)
		goto fail_tr_alloc;

	tr_ring_va[0].re_type = MHIP_RE_NOP;

	dir = IPA_CLIENT_IS_PROD(mhip_client) ?
		DMA_TO_HIPA : DMA_FROM_HIPA;

	/* allocate transfer ring elements */
	if (IPA_CLIENT_IS_PROD(mhip_client))
		ring_size = ipa3_ctx->mpm_ring_size_dl;
	else
		ring_size = ipa3_ctx->mpm_ring_size_ul;

	for (i = 1, k = 1; i < ring_size; i++, k++) {
		buff_va = kzalloc(TRE_BUFF_SIZE, GFP_KERNEL);
		if (!buff_va)
			goto fail_buff_alloc;

		tr_ring_va[i].buffer_ptr =
			ipa_mpm_smmu_map(buff_va, TRE_BUFF_SIZE, dir,
					&ap_cb_buff_iova);

		if (!tr_ring_va[i].buffer_ptr)
			goto fail_smmu_map_ring;

		tr_ring_va[i].buff_len = TRE_BUFF_SIZE;
		tr_ring_va[i].chain = 0;
		tr_ring_va[i].ieob = 0;
		tr_ring_va[i].ieot = 0;
		tr_ring_va[i].bei = 0;
		tr_ring_va[i].sct = 0;
		tr_ring_va[i].re_type = MHIP_RE_XFER;

		if (IPA_CLIENT_IS_PROD(mhip_client)) {
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_buff_va[k] =
						buff_va;
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_buff_c_iova[k]
						= tr_ring_va[i].buffer_ptr;
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_buff[k] =
						ap_cb_buff_iova;
		} else {
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_buff_va[k] =
						buff_va;
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_buff_c_iova[k]
						= tr_ring_va[i].buffer_ptr;
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_buff[k] =
						ap_cb_buff_iova;
		}
	}

	tr_carved_iova = ipa_mpm_smmu_map(tr_ring_va, PAGE_SIZE, dir,
		&ap_cb_tr_iova);
	if (!tr_carved_iova)
		goto fail_smmu_map_ring;

	er_carved_iova = ipa_mpm_smmu_map(er_ring_va, PAGE_SIZE, dir,
		&ap_cb_er_iova);
	if (!er_carved_iova)
		goto fail_smmu_map_ring;

	/* Store Producer channel rings */
	if (IPA_CLIENT_IS_PROD(mhip_client)) {
		/* Device UL */
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_va = er_ring_va;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_va = tr_ring_va;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_pa = er_carved_iova;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_pa = tr_carved_iova;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_tr =
			ap_cb_tr_iova;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_er =
			ap_cb_er_iova;
	} else {
		/* Host UL */
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_va = er_ring_va;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_va = tr_ring_va;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_pa = er_carved_iova;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_pa = tr_carved_iova;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_tr =
			ap_cb_tr_iova;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_er =
			ap_cb_er_iova;
	}

	memset(&gsi_params, 0, sizeof(struct ipa_request_gsi_channel_params));

	if (IPA_CLIENT_IS_PROD(mhip_client))
		gsi_params.ipa_ep_cfg =
		ipa_mpm_pipes[mhi_idx].dl_cons.ep_cfg;
	else
		gsi_params.ipa_ep_cfg =
		ipa_mpm_pipes[mhi_idx].ul_prod.ep_cfg;

	gsi_params.client = mhip_client;
	gsi_params.skip_ep_cfg = false;

	/*
	 * RP update address = Device channel DB address
	 * CLIENT_PROD -> Host DL
	 * CLIENT_CONS -> Host UL
	 */
	if (IPA_CLIENT_IS_PROD(mhip_client)) {
		gsi_params.evt_ring_params.rp_update_addr =
			ipa_mpm_smmu_map_doorbell(
			MHIP_SMMU_DOMAIN_IPA,
			ipa_mpm_ctx->md[mhi_idx].dl_cons.chan_props.device_db);
		if (gsi_params.evt_ring_params.rp_update_addr == 0)
			goto fail_smmu_map_db;

		ipa_mpm_ctx->md[mhi_idx].dl_cons.db_host_iova =
			gsi_params.evt_ring_params.rp_update_addr;

		gsi_params.evt_ring_params.ring_base_addr =
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_pa;
		gsi_params.chan_params.ring_base_addr =
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_pa;
	} else {
		gsi_params.evt_ring_params.rp_update_addr =
			ipa_mpm_smmu_map_doorbell(
			MHIP_SMMU_DOMAIN_IPA,
			ipa_mpm_ctx->md[mhi_idx].ul_prod.chan_props.device_db);
		if (gsi_params.evt_ring_params.rp_update_addr == 0)
			goto fail_smmu_map_db;
		ipa_mpm_ctx->md[mhi_idx].ul_prod.db_host_iova =
			gsi_params.evt_ring_params.rp_update_addr;
		gsi_params.evt_ring_params.ring_base_addr =
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_pa;
		gsi_params.chan_params.ring_base_addr =
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_pa;
	}

	/* Fill Event ring params */
	gsi_params.evt_ring_params.intf = GSI_EVT_CHTYPE_MHIP_EV;
	gsi_params.evt_ring_params.intr = GSI_INTR_MSI;
	gsi_params.evt_ring_params.re_size = GSI_EVT_RING_RE_SIZE_16B;
	gsi_params.evt_ring_params.ring_len =
		(ring_size) * GSI_EVT_RING_RE_SIZE_16B;
	gsi_params.evt_ring_params.ring_base_vaddr = NULL;
	gsi_params.evt_ring_params.int_modt = 0;
	gsi_params.evt_ring_params.int_modc = 0;
	gsi_params.evt_ring_params.intvec = 0;
	gsi_params.evt_ring_params.msi_addr = 0;
	gsi_params.evt_ring_params.exclusive = true;
	gsi_params.evt_ring_params.err_cb = ipa_mpm_gsi_evt_ring_err_cb;
	gsi_params.evt_ring_params.user_data = NULL;

	/* Evt Scratch Params */
	/* Disable the Moderation for ringing doorbells */
	gsi_params.evt_scratch.mhip.rp_mod_threshold = 1;
	gsi_params.evt_scratch.mhip.rp_mod_timer = 0;
	gsi_params.evt_scratch.mhip.rp_mod_counter = 0;
	gsi_params.evt_scratch.mhip.rp_mod_timer_id = 0;
	gsi_params.evt_scratch.mhip.rp_mod_timer_running = 0;
	gsi_params.evt_scratch.mhip.fixed_buffer_sz = TRE_BUFF_SIZE;

	/* Channel Params */
	gsi_params.chan_params.prot = GSI_CHAN_PROT_MHIP;
	gsi_params.chan_params.dir = IPA_CLIENT_IS_PROD(mhip_client) ?
		GSI_CHAN_DIR_TO_GSI : GSI_CHAN_DIR_FROM_GSI;
	/* chan_id is set in ipa3_request_gsi_channel() */
	gsi_params.chan_params.re_size = GSI_CHAN_RE_SIZE_16B;
	gsi_params.chan_params.ring_len =
		(ring_size) * GSI_EVT_RING_RE_SIZE_16B;
	gsi_params.chan_params.ring_base_vaddr = NULL;
	gsi_params.chan_params.use_db_eng = GSI_CHAN_DIRECT_MODE;
	gsi_params.chan_params.max_prefetch = GSI_ONE_PREFETCH_SEG;
	gsi_params.chan_params.low_weight = 1;
	gsi_params.chan_params.xfer_cb = NULL;
	gsi_params.chan_params.err_cb = ipa_mpm_gsi_chan_err_cb;
	gsi_params.chan_params.chan_user_data = NULL;

	/* Channel scratch */
	gsi_params.chan_scratch.mhip.assert_bit_40 = 0;
	gsi_params.chan_scratch.mhip.host_channel = 1;

	res = ipa3_request_gsi_channel(&gsi_params, out_params);
	if (res) {
		IPA_MPM_ERR("failed to allocate GSI channel res=%d\n", res);
		goto fail_alloc_channel;
	}

	if (IPA_CLIENT_IS_CONS(mhip_client)) {
		/*
		 * Enable HOLB timer one time after bootup/SSR.
		 * The HOLB timeout drops the packets on MHIP if
		 * there is a stall on MHIP TX pipe greater than
		 * configured timeout.
		 */
		result = ipa_mpm_start_mhip_holb_tmo(ipa_ep_idx);
		if (result) {
			IPA_MPM_ERR("HOLB config failed for %d, fail = %d\n",
				ipa_ep_idx, result);
			goto fail_alloc_channel;
		}
	}

	if (IPA_CLIENT_IS_PROD(mhip_client))
		ipa_mpm_change_gsi_state(mhi_idx,
			IPA_MPM_MHIP_CHAN_DL,
			GSI_ALLOCATED);
	else
		ipa_mpm_change_gsi_state(mhi_idx,
			IPA_MPM_MHIP_CHAN_UL,
			GSI_ALLOCATED);
	result = ipa3_start_gsi_channel(ipa_ep_idx);
	if (result) {
		IPA_MPM_ERR("start MHIP channel %d failed\n", mhip_client);
		if (IPA_CLIENT_IS_PROD(mhip_client))
			ipa_mpm_change_gsi_state(mhi_idx,
				IPA_MPM_MHIP_CHAN_DL, GSI_ERR);
		else
			ipa_mpm_change_gsi_state(mhi_idx,
				IPA_MPM_MHIP_CHAN_UL, GSI_ERR);
		goto fail_start_channel;
	}
	if (IPA_CLIENT_IS_PROD(mhip_client))
		ipa_mpm_change_gsi_state(mhi_idx,
			IPA_MPM_MHIP_CHAN_DL, GSI_STARTED);
	else
		ipa_mpm_change_gsi_state(mhi_idx,
			IPA_MPM_MHIP_CHAN_UL, GSI_STARTED);

	/* Fill in the Device Context params */
	if (IPA_CLIENT_IS_PROD(mhip_client)) {
		/* This is the DL channel :: Device -> Host */
		ipa_mpm_ctx->md[mhi_idx].dl_cons.evt_props.ev_ctx.rbase =
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_pa;
		ipa_mpm_ctx->md[mhi_idx].dl_cons.chan_props.ch_ctx.rbase =
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_pa;
	} else {
		ipa_mpm_ctx->md[mhi_idx].ul_prod.evt_props.ev_ctx.rbase =
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_pa;
		ipa_mpm_ctx->md[mhi_idx].ul_prod.chan_props.ch_ctx.rbase =
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_pa;
	}

	IPA_MPM_FUNC_EXIT();

	return 0;

fail_start_channel:
	ipa3_disable_data_path(ipa_ep_idx);
	ipa3_stop_gsi_channel(ipa_ep_idx);
fail_alloc_channel:
	ipa3_release_gsi_channel(ipa_ep_idx);
fail_smmu_map_db:
fail_smmu_map_ring:
fail_tr_alloc:
fail_evt_alloc:
fail_buff_alloc:
	ipa_assert();
fail_gen:
	return -EFAULT;
}

static void ipa_mpm_clean_mhip_chan(int mhi_idx,
	enum ipa_client_type mhip_client)
{
	int dir;
	int i;
	int ipa_ep_idx;
	int result;
	int ring_size;

	IPA_MPM_FUNC_ENTRY();

	if (mhip_client == IPA_CLIENT_MAX)
		return;

	if ((mhi_idx < IPA_MPM_MHIP_CH_ID_0) ||
		(mhi_idx >= IPA_MPM_MHIP_CH_ID_MAX))
		return;

	dir = IPA_CLIENT_IS_PROD(mhip_client) ?
		DMA_TO_HIPA : DMA_FROM_HIPA;

	ipa_ep_idx = ipa3_get_ep_mapping(mhip_client);
	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPA_MPM_ERR("fail to find channel EP.\n");
		return;
	}

	/* For the uplink channels, enable HOLB. */
	if (IPA_CLIENT_IS_CONS(mhip_client))
		ipa3_disable_data_path(ipa_ep_idx);

	/* Release channel */
	result = ipa3_stop_gsi_channel(ipa_ep_idx);
	if (result) {
		IPA_MPM_ERR("Stop channel for MHIP_Client =  %d failed\n",
					mhip_client);
		goto fail_chan;
	}
	result = ipa3_reset_gsi_channel(ipa_ep_idx);
	if (result) {
		IPA_MPM_ERR("Reset channel for MHIP_Client =  %d failed\n",
					mhip_client);
		goto fail_chan;
	}
	result = ipa3_reset_gsi_event_ring(ipa_ep_idx);
	if (result) {
		IPA_MPM_ERR("Reset ev ring for MHIP_Client =  %d failed\n",
					mhip_client);
		goto fail_chan;
	}
	result = ipa3_release_gsi_channel(ipa_ep_idx);
	if (result) {
		IPA_MPM_ERR("Release tr ring for MHIP_Client =  %d failed\n",
					mhip_client);
		if (IPA_CLIENT_IS_PROD(mhip_client))
			ipa_mpm_change_gsi_state(mhi_idx,
				IPA_MPM_MHIP_CHAN_DL, GSI_ERR);
		else
			ipa_mpm_change_gsi_state(mhi_idx,
				IPA_MPM_MHIP_CHAN_UL, GSI_ERR);
		goto fail_chan;
	}

	if (IPA_CLIENT_IS_PROD(mhip_client))
		ipa_mpm_change_gsi_state(mhi_idx,
					IPA_MPM_MHIP_CHAN_DL, GSI_INIT);
	else
		ipa_mpm_change_gsi_state(mhi_idx,
					IPA_MPM_MHIP_CHAN_UL, GSI_INIT);

	memset(&ipa3_ctx->ep[ipa_ep_idx], 0, sizeof(struct ipa3_ep_context));

	/* Unmap Doorbells */
	if (IPA_CLIENT_IS_PROD(mhip_client)) {
		ipa_mpm_smmu_unmap_doorbell(MHIP_SMMU_DOMAIN_PCIE,
			ipa_mpm_ctx->md[mhi_idx].dl_cons.db_device_iova);

		ipa_mpm_smmu_unmap_doorbell(MHIP_SMMU_DOMAIN_IPA,
			ipa_mpm_ctx->md[mhi_idx].dl_cons.db_host_iova);

		ipa_mpm_ctx->md[mhi_idx].dl_cons.db_host_iova = 0;
		ipa_mpm_ctx->md[mhi_idx].dl_cons.db_device_iova = 0;

	} else {
		ipa_mpm_smmu_unmap_doorbell(MHIP_SMMU_DOMAIN_PCIE,
			ipa_mpm_ctx->md[mhi_idx].ul_prod.db_device_iova);

		ipa_mpm_smmu_unmap_doorbell(MHIP_SMMU_DOMAIN_IPA,
			ipa_mpm_ctx->md[mhi_idx].ul_prod.db_host_iova);

		ipa_mpm_ctx->md[mhi_idx].ul_prod.db_host_iova = 0;
		ipa_mpm_ctx->md[mhi_idx].ul_prod.db_device_iova = 0;
	}

	/* deallocate/Unmap transfer ring buffers */
	if (IPA_CLIENT_IS_PROD(mhip_client))
		ring_size = ipa3_ctx->mpm_ring_size_dl_cache;
	else
		ring_size = ipa3_ctx->mpm_ring_size_ul_cache;

	for (i = 1; i < ring_size; i++) {
		if (IPA_CLIENT_IS_PROD(mhip_client)) {
			ipa_mpm_smmu_unmap(
			(dma_addr_t)
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_buff_c_iova[i],
			TRE_BUFF_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_buff[i]);
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_buff_c_iova[i]
								= 0;
			kfree(
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_buff_va[i]);
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_buff_va[i]
								= NULL;
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_buff[i]
								= 0;
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_buff_c_iova[i]
								= 0;
		} else {
			ipa_mpm_smmu_unmap(
			(dma_addr_t)
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_buff_c_iova[i],
			TRE_BUFF_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_buff[i]
			);
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_buff_c_iova[i]
								= 0;
			kfree(
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_buff_va[i]);
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_buff_va[i]
								= NULL;
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_buff[i]
								= 0;
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_buff_c_iova[i]
								= 0;
		}
	}

	/* deallocate/Unmap rings */
	if (IPA_CLIENT_IS_PROD(mhip_client)) {
		ipa_mpm_smmu_unmap(
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_pa,
			PAGE_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_er);

		ipa_mpm_smmu_unmap(
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_pa,
			PAGE_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_tr);

		if (ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_va) {
			free_page((unsigned long)
				ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_va);
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_va = NULL;
		}

		if (ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_va) {
			free_page((unsigned long)
				ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_va);
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_va = NULL;
		}

		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_er = 0;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_tr = 0;
	} else {
		ipa_mpm_smmu_unmap(
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_pa,
			PAGE_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_tr);
		ipa_mpm_smmu_unmap(
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_pa,
			PAGE_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_er);

		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_pa = 0;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_pa = 0;

		if (ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_va) {
			free_page((unsigned long)
				ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_va);
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_va = NULL;
		}

		if (ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_va) {
			free_page((unsigned long)
				ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_va);
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_va = NULL;
		}

		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_er = 0;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_tr = 0;
	}

	IPA_MPM_FUNC_EXIT();
	return;
fail_chan:
	ipa_assert();
}

/* round addresses for closest page per SMMU requirements */
static inline void ipa_mpm_smmu_round_to_page(uint64_t iova, uint64_t pa,
	uint64_t size, unsigned long *iova_p, phys_addr_t *pa_p, u32 *size_p)
{
	*iova_p = rounddown(iova, PAGE_SIZE);
	*pa_p = rounddown(pa, PAGE_SIZE);
	*size_p = roundup(size + pa - *pa_p, PAGE_SIZE);
}


static int __ipa_mpm_configure_mhi_device(struct ipa_mpm_channel *ch,
	int mhi_idx, int dir)
{
	struct mhi_buf ch_config[2];
	int ret;

	IPA_MPM_FUNC_ENTRY();

	if (ch == NULL) {
		IPA_MPM_ERR("ch config is NULL\n");
		return -EINVAL;
	}

	/* Populate CCA */
	ch_config[0].buf = &ch->chan_props.ch_ctx;
	ch_config[0].len = sizeof(ch->chan_props.ch_ctx);
	ch_config[0].name = "CCA";

	/* populate ECA */
	ch_config[1].buf = &ch->evt_props.ev_ctx;
	ch_config[1].len = sizeof(ch->evt_props.ev_ctx);
	ch_config[1].name = "ECA";

	IPA_MPM_DBG("Configuring MHI PRIME device for mhi_idx %d\n", mhi_idx);

	ret = mhi_device_configure(ipa_mpm_ctx->md[mhi_idx].mhi_dev, dir,
			ch_config, 2);
	if (ret) {
		IPA_MPM_ERR("mhi_device_configure failed\n");
		return -EINVAL;
	}
	IPA_MPM_FUNC_EXIT();
	return 0;
}

static void ipa_mpm_mhip_shutdown(int mhip_idx)
{
	enum ipa_client_type ul_prod_chan, dl_cons_chan;

	IPA_MPM_FUNC_ENTRY();

	get_ipa3_client(mhip_idx, &ul_prod_chan, &dl_cons_chan);

	if (mhip_idx != IPA_MPM_MHIP_CH_ID_2)
		/* For DPL, stop only DL channel */
		ipa_mpm_clean_mhip_chan(mhip_idx, ul_prod_chan);

	ipa_mpm_clean_mhip_chan(mhip_idx, dl_cons_chan);

	if (!ipa_mpm_ctx->md[mhip_idx].in_lpm) {
		ipa_mpm_vote_unvote_ipa_clk(CLK_OFF, mhip_idx);
		/* while in modem shutdown scenarios such as SSR, no explicit
		 * PCIe vote is needed.
		 */
		ipa_mpm_ctx->md[mhip_idx].in_lpm = true;
	}
	mutex_lock(&ipa_mpm_ctx->md[mhip_idx].mhi_mutex);
	ipa_mpm_ctx->md[mhip_idx].mhi_dev = NULL;
	mutex_unlock(&ipa_mpm_ctx->md[mhip_idx].mhi_mutex);
	IPA_MPM_FUNC_EXIT();
}

/**
 * @ipa_mpm_vote_unvote_pcie_clk - Vote/Unvote PCIe Clock per probe_id
 *                                 Returns if success or failure.
 * @ipa_mpm_clk_vote_type - Vote or Unvote for PCIe Clock
 * @probe_id - MHI probe_id per client.
 * @is_force - Forcebly casts vote - should be true only in probe.
 * @is_acted - Output param - This indicates the clk is actually voted or not
 *             The flag output is checked only when we vote for clocks.
 * Return value: PCIe clock voting is success or failure.
 */
static int ipa_mpm_vote_unvote_pcie_clk(enum ipa_mpm_clk_vote_type vote,
	int probe_id,
	bool is_force,
	bool *is_acted)
{
	int result = 0;

	if (probe_id >= IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("probe_id not found\n");
		return -EINVAL;
	}

	if (vote > CLK_OFF) {
		IPA_MPM_ERR("Invalid vote\n");
		return -EINVAL;
	}

	if (!is_acted) {
		IPA_MPM_ERR("Invalid clk_vote ptr\n");
		return -EFAULT;
	}

	mutex_lock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
	if (ipa_mpm_ctx->md[probe_id].mhi_dev == NULL) {
		IPA_MPM_ERR("MHI not initialized yet\n");
		*is_acted = false;
		mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
		return 0;
	}

	if (!ipa_mpm_ctx->md[probe_id].init_complete &&
		!is_force) {
		/*
		 * SSR might be in progress, dont have to vote/unvote for
		 * IPA clocks as it will be taken care in remove_cb/subsequent
		 * probe.
		 */
		IPA_MPM_DBG("SSR in progress, return\n");
		*is_acted = false;
		mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
		return 0;
	}
	mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);

	IPA_MPM_DBG("PCIe clock vote/unvote = %d probe_id = %d clk_cnt = %d\n",
		vote, probe_id,
		atomic_read(&ipa_mpm_ctx->md[probe_id].clk_cnt.pcie_clk_cnt));

	if (vote == CLK_ON) {
		result = mhi_device_get_sync(
			ipa_mpm_ctx->md[probe_id].mhi_dev,
				MHI_VOTE_BUS | MHI_VOTE_DEVICE);
		if (result) {
			IPA_MPM_ERR("mhi_sync_get failed for probe_id %d\n",
				result, probe_id);
			*is_acted = false;
			return result;
		}

		IPA_MPM_DBG("probe_id %d PCIE clock now ON\n", probe_id);
		atomic_inc(&ipa_mpm_ctx->md[probe_id].clk_cnt.pcie_clk_cnt);
		atomic_inc(&ipa_mpm_ctx->pcie_clk_total_cnt);
	} else {
		if ((atomic_read(
			&ipa_mpm_ctx->md[probe_id].clk_cnt.pcie_clk_cnt)
								== 0)) {
			IPA_MPM_ERR("probe_id %d PCIE clock already devoted\n",
				probe_id);
			*is_acted = true;
			return 0;
		}
		mhi_device_put(ipa_mpm_ctx->md[probe_id].mhi_dev,
				MHI_VOTE_BUS | MHI_VOTE_DEVICE);
		IPA_MPM_DBG("probe_id %d PCIE clock off\n", probe_id);
		atomic_dec(&ipa_mpm_ctx->md[probe_id].clk_cnt.pcie_clk_cnt);
		atomic_dec(&ipa_mpm_ctx->pcie_clk_total_cnt);
	}
	*is_acted = true;
	return result;
}

/*
 * Turning on/OFF IPA Clock is done only once- for all clients
 */
static void ipa_mpm_vote_unvote_ipa_clk(enum ipa_mpm_clk_vote_type vote,
	int probe_id)
{
	if (vote > CLK_OFF)
		return;

	IPA_MPM_DBG("IPA clock vote/unvote = %d probe_id = %d clk_cnt = %d\n",
		vote, probe_id,
		atomic_read(&ipa_mpm_ctx->md[probe_id].clk_cnt.ipa_clk_cnt));

	if (vote == CLK_ON) {
		IPA_ACTIVE_CLIENTS_INC_SPECIAL(ipa_mpm_mhip_chan_str[probe_id]);
		IPA_MPM_DBG("IPA clock now ON for probe_id %d\n", probe_id);
		atomic_inc(&ipa_mpm_ctx->md[probe_id].clk_cnt.ipa_clk_cnt);
		atomic_inc(&ipa_mpm_ctx->ipa_clk_total_cnt);
	} else {
		if ((atomic_read
			(&ipa_mpm_ctx->md[probe_id].clk_cnt.ipa_clk_cnt)
								== 0)) {
			IPA_MPM_ERR("probe_id %d IPA clock count < 0\n",
				probe_id);
			return;
		}
		IPA_ACTIVE_CLIENTS_DEC_SPECIAL(ipa_mpm_mhip_chan_str[probe_id]);
		IPA_MPM_DBG("probe_id %d IPA clock off\n", probe_id);
		atomic_dec(&ipa_mpm_ctx->md[probe_id].clk_cnt.ipa_clk_cnt);
		atomic_dec(&ipa_mpm_ctx->ipa_clk_total_cnt);
	}
}

/**
 * @ipa_mpm_start_stop_remote_mhip_chan - Start/Stop Remote device side MHIP
 *                                        channels.
 * @ipa_mpm_clk_vote_type - Vote or Unvote for PCIe Clock
 * @probe_id - MHI probe_id per client.
 * @ipa_mpm_start_stop_type - Start/Stop remote channels.
 * @is_force - Forcebly casts remote channels to be started/stopped.
 *             should be true only in probe.
 * Return value: 0 if success or error value.
 */
static int ipa_mpm_start_stop_remote_mhip_chan(
	int probe_id,
	enum ipa_mpm_start_stop_type start_stop,
	bool is_force)
{
	int ret = 0;
	struct mhi_device *mhi_dev = ipa_mpm_ctx->md[probe_id].mhi_dev;

	/* Sanity check to make sure Remote channels can be started.
	 * If probe in progress, mhi_prepare_for_transfer will start
	 * the remote channels so no need to start it from here.
	 */
	mutex_lock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
	if (!ipa_mpm_ctx->md[probe_id].init_complete && !is_force) {
		IPA_MPM_ERR("MHI not initialized yet, probe in progress\n");
		mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
		return ret;
	}

	/* For error state, expect modem SSR to recover from error */
	if (ipa_mpm_ctx->md[probe_id].remote_state == MPM_MHIP_REMOTE_ERR) {
		IPA_MPM_ERR("Remote channels in err state for %d\n", probe_id);
		mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
		return -EFAULT;
	}
	mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);

	if (start_stop == MPM_MHIP_START) {
		if (ipa_mpm_ctx->md[probe_id].remote_state ==
				MPM_MHIP_REMOTE_START) {
			IPA_MPM_DBG("Remote channel already started for %d\n",
				probe_id);
		} else {
			ret = mhi_resume_transfer(mhi_dev);
			mutex_lock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
			if (ret)
				ipa_mpm_ctx->md[probe_id].remote_state =
							MPM_MHIP_REMOTE_ERR;
			else
				ipa_mpm_ctx->md[probe_id].remote_state =
							MPM_MHIP_REMOTE_START;
			mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
		}
	} else {
		if (ipa_mpm_ctx->md[probe_id].remote_state ==
				MPM_MHIP_REMOTE_STOP) {
			IPA_MPM_DBG("Remote channel already stopped for %d\n",
					probe_id);
		} else {
			ret = mhi_pause_transfer(mhi_dev);
			mutex_lock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
			if (ret)
				ipa_mpm_ctx->md[probe_id].remote_state =
							MPM_MHIP_REMOTE_ERR;
			else
				ipa_mpm_ctx->md[probe_id].remote_state =
							MPM_MHIP_REMOTE_STOP;
			mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
		}
	}
	return ret;
}

static enum mhip_status_type ipa_mpm_start_stop_mhip_chan(
	enum ipa_mpm_mhip_chan mhip_chan,
	int probe_id,
	enum ipa_mpm_start_stop_type start_stop)
{
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;
	bool is_start;
	enum ipa_client_type ul_chan, dl_chan;
	u32 source_pipe_bitmask = 0;
	enum gsi_status gsi_res = GSI_STATUS_SUCCESS;
	int result;

	IPA_MPM_FUNC_ENTRY();

	if (mhip_chan > IPA_MPM_MHIP_CHAN_BOTH) {
		IPA_MPM_ERR("MHI not initialized yet\n");
		return MHIP_STATUS_FAIL;
	}

	if (probe_id >= IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("MHI not initialized yet\n");
		return MHIP_STATUS_FAIL;
	}

	get_ipa3_client(probe_id, &ul_chan, &dl_chan);

	if (mhip_chan == IPA_MPM_MHIP_CHAN_UL) {
		ipa_ep_idx = ipa3_get_ep_mapping(ul_chan);
	} else if (mhip_chan == IPA_MPM_MHIP_CHAN_DL) {
		ipa_ep_idx = ipa3_get_ep_mapping(dl_chan);
	} else if (mhip_chan == IPA_MPM_MHIP_CHAN_BOTH) {
		ipa_ep_idx = ipa3_get_ep_mapping(ul_chan);
		ipa_ep_idx = ipa3_get_ep_mapping(dl_chan);
	}

	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPA_MPM_ERR("fail to get EP# for idx %d\n", ipa_ep_idx);
		return MHIP_STATUS_EP_NOT_FOUND;
	}

	mutex_lock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
	if (!ipa_mpm_ctx->md[probe_id].init_complete) {
		IPA_MPM_ERR("MHIP probe %d not initialized\n", probe_id);
		mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
		return MHIP_STATUS_EP_NOT_READY;
	}
	mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	if (mhip_chan == IPA_MPM_MHIP_CHAN_UL) {
		IPA_MPM_DBG("current GSI state = %d, action = %d\n",
			ipa_mpm_ctx->md[probe_id].ul_prod.gsi_state,
			start_stop);
		if (ipa_mpm_ctx->md[probe_id].ul_prod.gsi_state <
			GSI_ALLOCATED) {
			IPA_MPM_ERR("GSI chan is not allocated yet\n");
			return MHIP_STATUS_EP_NOT_READY;
		}
	} else if (mhip_chan == IPA_MPM_MHIP_CHAN_DL) {
		IPA_MPM_DBG("current GSI state = %d, action = %d\n",
			ipa_mpm_ctx->md[probe_id].dl_cons.gsi_state,
			start_stop);
		if (ipa_mpm_ctx->md[probe_id].dl_cons.gsi_state <
			GSI_ALLOCATED) {
			IPA_MPM_ERR("GSI chan is not allocated yet\n");
			return MHIP_STATUS_EP_NOT_READY;
		}
	}

	is_start = (start_stop == MPM_MHIP_START) ? true : false;

	if (is_start) {
		if (mhip_chan == IPA_MPM_MHIP_CHAN_UL) {
			if (ipa_mpm_ctx->md[probe_id].ul_prod.gsi_state ==
				GSI_STARTED) {
				IPA_MPM_ERR("GSI chan is already started\n");
				return MHIP_STATUS_NO_OP;
			}
		}

		if (mhip_chan == IPA_MPM_MHIP_CHAN_DL) {
			if (ipa_mpm_ctx->md[probe_id].dl_cons.gsi_state ==
				GSI_STARTED) {
				IPA_MPM_ERR("GSI chan is already started\n");
				return MHIP_STATUS_NO_OP;
			}
		}
		/* Start GSI channel */
		gsi_res = ipa3_start_gsi_channel(ipa_ep_idx);
		if (gsi_res != GSI_STATUS_SUCCESS) {
			IPA_MPM_ERR("Error starting channel: err = %d\n",
					gsi_res);
			goto gsi_chan_fail;
		} else {
			ipa_mpm_change_gsi_state(probe_id, mhip_chan,
					GSI_STARTED);
		}
	} else {
		if (mhip_chan == IPA_MPM_MHIP_CHAN_UL) {
			if (ipa_mpm_ctx->md[probe_id].ul_prod.gsi_state ==
				GSI_STOPPED) {
				IPA_MPM_ERR("GSI chan is already stopped\n");
				return MHIP_STATUS_NO_OP;
			} else if (ipa_mpm_ctx->md[probe_id].ul_prod.gsi_state
				!= GSI_STARTED) {
				IPA_MPM_ERR("GSI chan isn't already started\n");
				return MHIP_STATUS_NO_OP;
			}
		}

		if (mhip_chan == IPA_MPM_MHIP_CHAN_DL) {
			if (ipa_mpm_ctx->md[probe_id].dl_cons.gsi_state ==
				GSI_STOPPED) {
				IPA_MPM_ERR("GSI chan is already stopped\n");
				return MHIP_STATUS_NO_OP;
			} else if (ipa_mpm_ctx->md[probe_id].dl_cons.gsi_state
				!= GSI_STARTED) {
				IPA_MPM_ERR("GSI chan isn't already started\n");
				return MHIP_STATUS_NO_OP;
			}
		}

		if (mhip_chan == IPA_MPM_MHIP_CHAN_UL) {
			source_pipe_bitmask = 1 <<
				ipa3_get_ep_mapping(ep->client);
			/* First Stop UL GSI channel before unvote PCIe clock */
			result = ipa3_stop_gsi_channel(ipa_ep_idx);

			if (result) {
				IPA_MPM_ERR("UL chan stop failed\n");
				goto gsi_chan_fail;
			} else {
				ipa_mpm_change_gsi_state(probe_id, mhip_chan,
							GSI_STOPPED);
			}
		}

		if (mhip_chan == IPA_MPM_MHIP_CHAN_DL) {
			result = ipa3_stop_gsi_channel(ipa_ep_idx);
			if (result) {
				IPA_MPM_ERR("Fail to stop DL channel\n");
				goto gsi_chan_fail;
			} else {
				ipa_mpm_change_gsi_state(probe_id, mhip_chan,
							GSI_STOPPED);
			}
		}
	}
	IPA_MPM_FUNC_EXIT();

	return MHIP_STATUS_SUCCESS;
gsi_chan_fail:
	ipa3_disable_data_path(ipa_ep_idx);
	ipa_mpm_change_gsi_state(probe_id, mhip_chan, GSI_ERR);
	ipa_assert();
	return MHIP_STATUS_FAIL;
}

int ipa_mpm_notify_wan_state(struct wan_ioctl_notify_wan_state *state)
{
	int probe_id = IPA_MPM_MHIP_CH_ID_MAX;
	int i;
	static enum mhip_status_type status;
	int ret = 0;
	enum ipa_mpm_mhip_client_type mhip_client = IPA_MPM_MHIP_TETH;
	bool is_acted = true;
	const struct ipa_gsi_ep_config *ep_cfg;
	uint32_t flow_ctrl_mask = 0;

	if (!state)
		return -EPERM;

	if (!ipa3_is_mhip_offload_enabled())
		return -EPERM;

	for (i = 0; i < IPA_MPM_MHIP_CH_ID_MAX; i++) {
		if (ipa_mpm_pipes[i].mhip_client == mhip_client) {
			probe_id = i;
			break;
		}
	}

	if (probe_id == IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("Unknown probe_id\n");
		return -EPERM;
	}

	IPA_MPM_DBG("WAN backhaul available for probe_id = %d\n", probe_id);

	if (state->up) {
		/* Start UL MHIP channel for offloading tethering connection */
		ret = ipa_mpm_vote_unvote_pcie_clk(CLK_ON, probe_id,
			false, &is_acted);
		if (ret) {
			IPA_MPM_ERR("Err %d cloking on PCIe clk %d\n", ret);
			return ret;
		}

		/*
		 * Make sure to start Device side channels before
		 * starting Host side UL channels. This is to make
		 * sure device side access host side only after
		 * Host IPA gets voted.
		 */
		ret = ipa_mpm_start_stop_remote_mhip_chan(probe_id,
							MPM_MHIP_START,
							false);
		if (ret) {
			/*
			 * This can fail only when modem is in SSR state.
			 * Eventually there would be a remove callback,
			 * so return a failure.
			 */
			IPA_MPM_ERR("MHIP remote chan start fail = %d\n", ret);

			if (is_acted)
				ipa_mpm_vote_unvote_pcie_clk(CLK_OFF,
					probe_id,
					false,
					&is_acted);

			return ret;
		}
		IPA_MPM_DBG("MHIP remote channels are started\n");

		 /*
		  * Update flow control monitoring end point info.
		  * This info will be used to set delay on the end points upon
		  * hitting RED water mark.
		  */
		ep_cfg = ipa3_get_gsi_ep_info(IPA_CLIENT_WLAN2_PROD);

		if (!ep_cfg)
			IPA_MPM_ERR("ep = %d not allocated yet\n",
					IPA_CLIENT_WLAN2_PROD);
		else
			flow_ctrl_mask |= 1 << (ep_cfg->ipa_gsi_chan_num);

		ep_cfg = ipa3_get_gsi_ep_info(IPA_CLIENT_USB_PROD);

		if (!ep_cfg)
			IPA_MPM_ERR("ep = %d not allocated yet\n",
					IPA_CLIENT_USB_PROD);
		else
			flow_ctrl_mask |= 1 << (ep_cfg->ipa_gsi_chan_num);

		atomic_set(&ipa_mpm_ctx->flow_ctrl_mask, flow_ctrl_mask);

		ret = ipa3_uc_send_update_flow_control(flow_ctrl_mask,
						IPA_MPM_FLOW_CTRL_ADD);

		if (ret)
			IPA_MPM_ERR("Err = %d setting uc flow control\n", ret);

		status = ipa_mpm_start_stop_mhip_chan(
				IPA_MPM_MHIP_CHAN_UL, probe_id, MPM_MHIP_START);
		switch (status) {
		case MHIP_STATUS_SUCCESS:
			ipa_mpm_ctx->md[probe_id].teth_state =
						IPA_MPM_TETH_CONNECTED;
			/* Register for BW indication from Q6 */
			if (!ipa3_qmi_reg_dereg_for_bw(true,
				BW_VOTE_WAN_NOTIFY))
				IPA_MPM_ERR(
					"Failed rgstring for QMIBW Ind, might be SSR");
			break;
		case MHIP_STATUS_EP_NOT_READY:
		case MHIP_STATUS_NO_OP:
			IPA_MPM_DBG("UL chan already start, status = %d\n",
					status);
			if (is_acted) {
				return ipa_mpm_vote_unvote_pcie_clk(CLK_OFF,
						probe_id,
						false,
						&is_acted);
			}
			break;
		case MHIP_STATUS_FAIL:
		case MHIP_STATUS_BAD_STATE:
		case MHIP_STATUS_EP_NOT_FOUND:
			IPA_MPM_ERR("UL chan start err =%d\n", status);
			if (is_acted)
				ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
					false, &is_acted);
			ipa_assert();
			return -EFAULT;
		default:
			IPA_MPM_ERR("Err not found\n");
			if (is_acted)
				ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
					false, &is_acted);
			ret = -EFAULT;
			break;
		}
		ipa_mpm_ctx->md[probe_id].mhip_client = mhip_client;
	} else {
		/*
		 * Update flow control monitoring end point info.
		 * This info will be used to reset delay on the end points.
		 */
		flow_ctrl_mask =
			atomic_read(&ipa_mpm_ctx->flow_ctrl_mask);

		ret = ipa3_uc_send_update_flow_control(flow_ctrl_mask,
						IPA_MPM_FLOW_CTRL_DELETE);
		flow_ctrl_mask = 0;
		atomic_set(&ipa_mpm_ctx->flow_ctrl_mask, 0);

		if (ret) {
			IPA_MPM_ERR("Err = %d resetting uc flow control\n",
					ret);
			ipa_assert();
		}

		/* De-register for BW indication from Q6*/
		if (atomic_read(&ipa_mpm_ctx->active_teth_count) >= 1) {
			if (!ipa3_qmi_reg_dereg_for_bw(false,
				BW_UNVOTE_WAN_NOTIFY))
				IPA_MPM_DBG(
					"Failed De-rgstrng QMI BW Indctn,might be SSR");
		} else {
			IPA_MPM_ERR(
				"Active teth count is %d",
				atomic_read(&ipa_mpm_ctx->active_teth_count));
		}

		/*
		 * Make sure to stop Device side channels before
		 * stopping Host side UL channels. This is to make
		 * sure device side doesn't access host IPA after
		 * Host IPA gets devoted.
		 */
		ret = ipa_mpm_start_stop_remote_mhip_chan(probe_id,
						MPM_MHIP_STOP,
						false);
		if (ret) {
			/*
			 * This can fail only when modem is in SSR state.
			 * Eventually there would be a remove callback,
			 * so return a failure.
			 */
			IPA_MPM_ERR("MHIP remote chan stop fail = %d\n", ret);
			return ret;
		}
		IPA_MPM_DBG("MHIP remote channels are stopped\n");

		status = ipa_mpm_start_stop_mhip_chan(
					IPA_MPM_MHIP_CHAN_UL, probe_id,
					MPM_MHIP_STOP);
		switch (status) {
		case MHIP_STATUS_SUCCESS:
			ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_INIT);
			break;
		case MHIP_STATUS_NO_OP:
		case MHIP_STATUS_EP_NOT_READY:
			IPA_MPM_DBG("UL chan already stop, status = %d\n",
					status);
			break;
		case MHIP_STATUS_FAIL:
		case MHIP_STATUS_BAD_STATE:
		case MHIP_STATUS_EP_NOT_FOUND:
			IPA_MPM_ERR("UL chan cant be stopped err =%d\n",
				status);
			ipa_assert();
			return -EFAULT;
		default:
			IPA_MPM_ERR("Err not found\n");
			return -EFAULT;
		}
		/* Stop UL MHIP channel for offloading tethering connection */
		ret = ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
					false, &is_acted);

		if (ret) {
			IPA_MPM_ERR("Error cloking off PCIe clk, err = %d\n",
				ret);
			return ret;
		}
		ipa_mpm_ctx->md[probe_id].mhip_client = IPA_MPM_MHIP_NONE;
	}
	return ret;
}

static void ipa_mpm_change_gsi_state(int probe_id,
	enum ipa_mpm_mhip_chan mhip_chan,
	enum ipa_mpm_gsi_state next_state)
{

	if (probe_id >= IPA_MPM_MHIP_CH_ID_MAX)
		return;

	if (mhip_chan == IPA_MPM_MHIP_CHAN_UL) {
		mutex_lock(&ipa_mpm_ctx->md[probe_id].mutex);
		ipa_mpm_ctx->md[probe_id].ul_prod.gsi_state = next_state;
		IPA_MPM_DBG("GSI next_state = %d\n",
			ipa_mpm_ctx->md[probe_id].ul_prod.gsi_state);
		 mutex_unlock(&ipa_mpm_ctx->md[probe_id].mutex);
	}

	if (mhip_chan == IPA_MPM_MHIP_CHAN_DL) {
		mutex_lock(&ipa_mpm_ctx->md[probe_id].mutex);
		ipa_mpm_ctx->md[probe_id].dl_cons.gsi_state = next_state;
		IPA_MPM_DBG("GSI next_state = %d\n",
			ipa_mpm_ctx->md[probe_id].dl_cons.gsi_state);
		 mutex_unlock(&ipa_mpm_ctx->md[probe_id].mutex);
	}
}

static void ipa_mpm_change_teth_state(int probe_id,
	enum ipa_mpm_teth_state next_state)
{
	enum ipa_mpm_teth_state curr_state;

	if (probe_id >= IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("Unknown probe_id\n");
		return;
	}

	curr_state = ipa_mpm_ctx->md[probe_id].teth_state;

	IPA_MPM_DBG("curr_state = %d, ip_state = %d mhip_s\n",
		curr_state, next_state);

	switch (curr_state) {
	case IPA_MPM_TETH_INIT:
		if (next_state == IPA_MPM_TETH_CONNECTED)
			next_state = IPA_MPM_TETH_INPROGRESS;
		break;
	case IPA_MPM_TETH_INPROGRESS:
		break;
	case IPA_MPM_TETH_CONNECTED:
		break;
	default:
		IPA_MPM_ERR("No change in state\n");
		break;
	}

	ipa_mpm_ctx->md[probe_id].teth_state = next_state;
	IPA_MPM_DBG("next_state = %d\n", next_state);
}

static void ipa_mpm_read_channel(enum ipa_client_type chan)
{
	struct gsi_chan_info chan_info;
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;
	int res;

	ipa_ep_idx = ipa3_get_ep_mapping(chan);

	if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
		IPAERR("failed to get idx");
		return;
	}

	ep = &ipa3_ctx->ep[ipa_ep_idx];

	IPA_MPM_DBG("Reading channel for chan %d, ep = %d, gsi_chan_hdl = %d\n",
		chan, ep, ep->gsi_chan_hdl);

	res = ipa3_get_gsi_chan_info(&chan_info, ep->gsi_chan_hdl);
	if (res)
		IPA_MPM_ERR("Reading of channel failed for ep %d\n", ep);
}

/* ipa_mpm_mhi_probe_cb is received for each MHI'/MHI channel
 * Currently we have 4 MHI channels.
 */
static int ipa_mpm_mhi_probe_cb(struct mhi_device *mhi_dev,
	const struct mhi_device_id *mhi_id)
{
	struct ipa_mpm_channel *ch;
	int ret;
	enum ipa_client_type ul_prod, dl_cons;
	int probe_id;
	struct ipa_req_chan_out_params ul_out_params, dl_out_params;
	void __iomem  *db_addr;
	int ipa_ep_idx;
	struct ipa3_ep_context *ep;
	u32 evt_ring_db_addr_low, evt_ring_db_addr_high;
	u32 wp_addr;
	int pipe_idx;
	bool is_acted = true;
	uint64_t flow_ctrl_mask = 0;
	bool add_delete = false;

	IPA_MPM_FUNC_ENTRY();

	if (ipa_mpm_ctx == NULL) {
		IPA_MPM_ERR("ipa_mpm_ctx is NULL not expected, returning..\n");
		return -ENOMEM;
	}

	probe_id = get_idx_from_id(mhi_id);

	if (probe_id >= IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("chan=%pK is not supported for now\n", mhi_id);
		return -EPERM;
	}

	if (ipa_mpm_ctx->md[probe_id].init_complete) {
		IPA_MPM_ERR("Probe initialization already done, returning\n");
		return 0;
	}

	IPA_MPM_DBG("Received probe for id=%d\n", probe_id);

	get_ipa3_client(probe_id, &ul_prod, &dl_cons);

	/* Vote for IPA clock for first time in initialization seq.
	 * IPA clock will be devoted when MHI enters LPM
	 * PCIe clock will be voted / devoted with every channel probe
	 * we receive.
	 * ul_prod = Host -> Device
	 * dl_cons = Device -> Host
	 */
	ipa_mpm_ctx->md[probe_id].mhi_dev = mhi_dev;
	ipa_mpm_ctx->mhi_parent_dev =
		ipa_mpm_ctx->md[probe_id].mhi_dev->dev.parent;

	mutex_lock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
	ipa_mpm_ctx->md[probe_id].remote_state = MPM_MHIP_REMOTE_STOP;
	mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
	ret = ipa_mpm_vote_unvote_pcie_clk(CLK_ON, probe_id, true, &is_acted);
	if (ret) {
		IPA_MPM_ERR("Err %d voitng PCIe clocks\n", ret);
		return -EPERM;
	}

	ipa_mpm_vote_unvote_ipa_clk(CLK_ON, probe_id);
	ipa_mpm_ctx->md[probe_id].in_lpm = false;
	IPA_MPM_DBG("ul chan = %d, dl_chan = %d\n", ul_prod, dl_cons);

	/*
	 * Set up MHI' pipes for Device IPA filling in
	 * Channel Context and Event Context.
	 * These params will be sent to Device side.
	 * UL CHAN = HOST -> Device
	 * DL CHAN = Device -> HOST
	 * per channel a TRE and EV is allocated.
	 * for a UL channel -
	 * IPA HOST PROD TRE -> IPA DEVICE CONS EV
	 * IPA HOST PROD EV ->  IPA DEVICE CONS TRE
	 * for a DL channel -
	 * IPA Device PROD TRE -> IPA HOST CONS EV
	 * IPA Device PROD EV ->  IPA HOST CONS TRE
	 */
	if (ul_prod != IPA_CLIENT_MAX) {
		/* store UL properties */
		ch = &ipa_mpm_ctx->md[probe_id].ul_prod;
		/* Store Channel properties */
		ch->chan_props.id = mhi_dev->ul_chan_id;
		ch->chan_props.device_db =
			ipa_mpm_ctx->dev_info.chdb_base +
			ch->chan_props.id * 8;
		/* Fill Channel Conext to be sent to Device side */
		ch->chan_props.ch_ctx.chtype =
			IPA_MPM_MHI_HOST_UL_CHANNEL;
		ch->chan_props.ch_ctx.erindex =
			mhi_dev->ul_event_id;
		ch->chan_props.ch_ctx.rlen = (ipa3_ctx->mpm_ring_size_ul) *
			GSI_EVT_RING_RE_SIZE_16B;
		/* Store Event properties */
		ch->evt_props.ev_ctx.update_rp_modc = 1;
		ch->evt_props.ev_ctx.update_rp_intmodt = 0;
		ch->evt_props.ev_ctx.ertype = 1;
		ch->evt_props.ev_ctx.rlen = (ipa3_ctx->mpm_ring_size_ul) *
			GSI_EVT_RING_RE_SIZE_16B;
		ch->evt_props.ev_ctx.buff_size = TRE_BUFF_SIZE;
		ch->evt_props.device_db =
			ipa_mpm_ctx->dev_info.erdb_base +
			ch->chan_props.ch_ctx.erindex * 8;

		/* Map uc-db and put in reserve2 */
		if (probe_id == IPA_MPM_MHIP_CH_ID_0) {
			/* map uc-fc-mb */
			ipa_mpm_ctx->uc_fc_db_iova =
				ipa_mpm_smmu_map_doorbell(MHIP_SMMU_DOMAIN_PCIE,
				ipa_mpm_ctx->uc_fc_db);
			ch->chan_props.ch_ctx.reserved2 =
				ipa_mpm_ctx->uc_fc_db_iova;
			IPA_MPM_DBG("configure reserved2 %lx\n",
				ch->chan_props.ch_ctx.reserved2);
		}

		/* connect Host GSI pipes with MHI' protocol */
		ret = ipa_mpm_connect_mhip_gsi_pipe(ul_prod,
			probe_id, &ul_out_params);
		if (ret) {
			IPA_MPM_ERR("failed connecting MPM client %d\n",
					ul_prod);
			goto fail_gsi_setup;
		}

		ch->evt_props.ev_ctx.update_rp_addr =
			ipa_mpm_smmu_map_doorbell(
				MHIP_SMMU_DOMAIN_PCIE,
				ul_out_params.db_reg_phs_addr_lsb);
		if (ch->evt_props.ev_ctx.update_rp_addr == 0)
			ipa_assert();

		ipa_mpm_ctx->md[probe_id].ul_prod.db_device_iova =
			ch->evt_props.ev_ctx.update_rp_addr;

		ret = __ipa_mpm_configure_mhi_device(
				ch, probe_id, DMA_TO_HIPA);
		if (ret) {
			IPA_MPM_ERR("configure_mhi_dev fail %d\n",
					ret);
			goto fail_smmu;
		}
	}

	if (dl_cons != IPA_CLIENT_MAX) {
		/* store DL channel properties */
		ch = &ipa_mpm_ctx->md[probe_id].dl_cons;
		/* Store Channel properties */
		ch->chan_props.id = mhi_dev->dl_chan_id;
		ch->chan_props.device_db =
			ipa_mpm_ctx->dev_info.chdb_base +
			ch->chan_props.id * 8;
		/* Fill Channel Conext to be be sent to Dev side */
		ch->chan_props.ch_ctx.chstate = 1;
		ch->chan_props.ch_ctx.chtype =
			IPA_MPM_MHI_HOST_DL_CHANNEL;
		ch->chan_props.ch_ctx.erindex = mhi_dev->dl_event_id;
		ch->chan_props.ch_ctx.rlen = (ipa3_ctx->mpm_ring_size_dl) *
			GSI_EVT_RING_RE_SIZE_16B;
		/* Store Event properties */
		ch->evt_props.ev_ctx.update_rp_modc = 0;
		ch->evt_props.ev_ctx.update_rp_intmodt = 0;
		ch->evt_props.ev_ctx.ertype = 1;
		ch->evt_props.ev_ctx.rlen = (ipa3_ctx->mpm_ring_size_dl) *
			GSI_EVT_RING_RE_SIZE_16B;
		ch->evt_props.ev_ctx.buff_size = TRE_BUFF_SIZE;
		ch->evt_props.device_db =
			ipa_mpm_ctx->dev_info.erdb_base +
			ch->chan_props.ch_ctx.erindex * 8;

		/* connect Host GSI pipes with MHI' protocol */
		ret = ipa_mpm_connect_mhip_gsi_pipe(dl_cons,
			probe_id, &dl_out_params);
		if (ret) {
			IPA_MPM_ERR("connecting MPM client = %d failed\n",
				dl_cons);
			goto fail_gsi_setup;
		}

		ch->evt_props.ev_ctx.update_rp_addr =
			ipa_mpm_smmu_map_doorbell(
					MHIP_SMMU_DOMAIN_PCIE,
					dl_out_params.db_reg_phs_addr_lsb);

		if (ch->evt_props.ev_ctx.update_rp_addr == 0)
			ipa_assert();

		ipa_mpm_ctx->md[probe_id].dl_cons.db_device_iova =
			ch->evt_props.ev_ctx.update_rp_addr;

		ret = __ipa_mpm_configure_mhi_device(ch, probe_id,
					DMA_FROM_HIPA);
		if (ret) {
			IPA_MPM_ERR("mpm_config_mhi_dev failed %d\n", ret);
			goto fail_smmu;
		}
	}

	ret = mhi_prepare_for_transfer(ipa_mpm_ctx->md[probe_id].mhi_dev);
	if (ret) {
		IPA_MPM_ERR("mhi_prepare_for_transfer failed %d\n", ret);
		/*
		 * WA to handle prepare_for_tx failures.
		 * Though prepare for transfer fails, indicate success
		 * to MHI driver. remove_cb will be called eventually when
		 * Device side comes from where pending cleanup happens.
		 */
		mutex_lock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
		atomic_inc(&ipa_mpm_ctx->probe_cnt);
		ipa_mpm_ctx->md[probe_id].init_complete = false;
		mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
		IPA_MPM_FUNC_EXIT();
		return 0;
	}

	/* mhi_prepare_for_transfer translates to starting remote channels */
	mutex_lock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
	ipa_mpm_ctx->md[probe_id].remote_state = MPM_MHIP_REMOTE_START;
	mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
	/*
	 * Ring initial channel db - Host Side UL and Device side DL channel.
	 * To ring doorbell, write "WP" into doorbell register.
	 * This WP should be set to 1 element less than ring max.
	 */

	/* Ring UL PRODUCER TRANSFER RING (HOST IPA -> DEVICE IPA) Doorbell */
	if (ul_prod != IPA_CLIENT_MAX) {
		IPA_MPM_DBG("Host UL TR PA DB = 0X%0x\n",
			ul_out_params.db_reg_phs_addr_lsb);

		db_addr = ioremap(
			(phys_addr_t)(ul_out_params.db_reg_phs_addr_lsb), 4);

		wp_addr = ipa_mpm_ctx->md[probe_id].ul_prod_ring.tr_pa +
			((ipa3_ctx->mpm_ring_size_ul - 1) *
			GSI_CHAN_RE_SIZE_16B);

		iowrite32(wp_addr, db_addr);

		IPA_MPM_DBG("Host UL TR  DB = 0X%pK, wp_addr = 0X%0x",
			db_addr, wp_addr);

		iounmap(db_addr);
		ipa_mpm_read_channel(ul_prod);

		/* Ring UL PRODUCER EVENT RING (HOST IPA -> DEVICE IPA) Doorbell
		 * Ring the event DB to a value outside the
		 * ring range such that rp and wp never meet.
		 */
		ipa_ep_idx = ipa3_get_ep_mapping(ul_prod);

		if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
			IPA_MPM_ERR("fail to alloc EP.\n");
			goto fail_start_channel;
		}
		ep = &ipa3_ctx->ep[ipa_ep_idx];

		IPA_MPM_DBG("for ep_idx %d , gsi_evt_ring_hdl = %ld\n",
			ipa_ep_idx, ep->gsi_evt_ring_hdl);
		gsi_query_evt_ring_db_addr(ep->gsi_evt_ring_hdl,
			&evt_ring_db_addr_low, &evt_ring_db_addr_high);

		IPA_MPM_DBG("Host UL ER PA DB = 0X%0x\n",
			evt_ring_db_addr_low);

		db_addr = ioremap((phys_addr_t)(evt_ring_db_addr_low), 4);

		wp_addr = ipa_mpm_ctx->md[probe_id].ul_prod_ring.er_pa +
			((ipa3_ctx->mpm_ring_size_ul + 1) *
			GSI_EVT_RING_RE_SIZE_16B);
		IPA_MPM_DBG("Host UL ER  DB = 0X%pK, wp_addr = 0X%0x",
			db_addr, wp_addr);

		iowrite32(wp_addr, db_addr);
		iounmap(db_addr);

		/* Ring DEVICE IPA DL CONSUMER Event Doorbell */
		db_addr = ioremap((phys_addr_t)
			(ipa_mpm_ctx->md[probe_id].ul_prod.evt_props.device_db),
			4);

		wp_addr = ipa_mpm_ctx->md[probe_id].ul_prod_ring.tr_pa +
			((ipa3_ctx->mpm_ring_size_ul + 1) *
			GSI_EVT_RING_RE_SIZE_16B);

		iowrite32(wp_addr, db_addr);
		iounmap(db_addr);
	}

	/* Ring DL PRODUCER (DEVICE IPA -> HOST IPA) Doorbell */
	if (dl_cons != IPA_CLIENT_MAX) {
		db_addr = ioremap((phys_addr_t)
		(ipa_mpm_ctx->md[probe_id].dl_cons.chan_props.device_db),
		4);

		wp_addr = ipa_mpm_ctx->md[probe_id].dl_prod_ring.tr_pa +
			((ipa3_ctx->mpm_ring_size_dl - 1) *
			GSI_CHAN_RE_SIZE_16B);

		IPA_MPM_DBG("Device DL TR  DB = 0X%pK, wp_addr = 0X%0x",
			db_addr, wp_addr);

		iowrite32(wp_addr, db_addr);

		iounmap(db_addr);

		/*
		 * Ring event ring DB on Device side.
		 * ipa_mpm should ring the event DB to a value outside the
		 * ring range such that rp and wp never meet.
		 */
		db_addr =
		ioremap(
		(phys_addr_t)
		(ipa_mpm_ctx->md[probe_id].dl_cons.evt_props.device_db),
		4);

		wp_addr = ipa_mpm_ctx->md[probe_id].dl_prod_ring.er_pa +
			((ipa3_ctx->mpm_ring_size_dl + 1) *
			GSI_EVT_RING_RE_SIZE_16B);

		iowrite32(wp_addr, db_addr);
		IPA_MPM_DBG("Device  UL ER  DB = 0X%pK,wp_addr = 0X%0x",
			db_addr, wp_addr);
		iounmap(db_addr);

		/* Ring DL EVENT RING CONSUMER (DEVICE IPA CONSUMER) Doorbell */
		ipa_ep_idx = ipa3_get_ep_mapping(dl_cons);

		if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
			IPA_MPM_ERR("fail to alloc EP.\n");
			goto fail_start_channel;
		}
		ep = &ipa3_ctx->ep[ipa_ep_idx];

		gsi_query_evt_ring_db_addr(ep->gsi_evt_ring_hdl,
			&evt_ring_db_addr_low, &evt_ring_db_addr_high);
		IPA_MPM_DBG("Host DL ER PA DB = 0X%0x\n",
				evt_ring_db_addr_low);
		db_addr = ioremap((phys_addr_t)(evt_ring_db_addr_low), 4);

		wp_addr = ipa_mpm_ctx->md[probe_id].dl_prod_ring.tr_pa +
			((ipa3_ctx->mpm_ring_size_dl + 1) *
			GSI_EVT_RING_RE_SIZE_16B);
		iowrite32(wp_addr, db_addr);
		IPA_MPM_DBG("Host  DL ER  DB = 0X%pK, wp_addr = 0X%0x",
			db_addr, wp_addr);
		iounmap(db_addr);
	}

	/* Check if TETH connection is in progress.
	 * If teth isn't started by now, then Stop UL channel.
	 */
	switch (ipa_mpm_ctx->md[probe_id].teth_state) {
	case IPA_MPM_TETH_INIT:
		/*
		 * Make sure to stop Device side channels before
		 * stopping Host side UL channels. This is to make
		 * sure Device side doesn't access host side IPA if
		 * Host IPA gets unvoted.
		 */
		ret = ipa_mpm_start_stop_remote_mhip_chan(probe_id,
						MPM_MHIP_STOP, true);
		if (ret) {
			/*
			 * This can fail only when modem is in SSR.
			 * Eventually there would be a remove callback,
			 * so return a failure.
			 */
			IPA_MPM_ERR("MHIP remote chan stop fail = %d\n", ret);
			return ret;
		}
		if (ul_prod != IPA_CLIENT_MAX) {
			/* No teth started yet, disable UL channel */
			ipa_ep_idx = ipa3_get_ep_mapping(ul_prod);
			if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
				IPA_MPM_ERR("fail to alloc EP.\n");
				goto fail_stop_channel;
			}
			ret = ipa3_stop_gsi_channel(ipa_ep_idx);
			if (ret) {
				IPA_MPM_ERR("MHIP Stop channel err = %d\n",
					ret);
				goto fail_stop_channel;
			}
			ipa_mpm_change_gsi_state(probe_id,
				IPA_MPM_MHIP_CHAN_UL,
				GSI_STOPPED);
		}
		if (is_acted)
			ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
							true, &is_acted);
		break;
	case IPA_MPM_TETH_INPROGRESS:
	case IPA_MPM_TETH_CONNECTED:
		IPA_MPM_DBG("UL channel is already started, continue\n");
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_CONNECTED);

		/* Lift the delay for rmnet USB prod pipe */
		if (probe_id == IPA_MPM_MHIP_CH_ID_1) {
			pipe_idx = ipa3_get_ep_mapping(IPA_CLIENT_USB_PROD);
			ipa3_xdci_ep_delay_rm(pipe_idx);
			/* Register for BW indication from Q6*/
			if (ipa_mpm_ctx->md[probe_id].teth_state ==
				IPA_MPM_TETH_CONNECTED)
				if (!ipa3_qmi_reg_dereg_for_bw(true,
					BW_VOTE_PROBE_CB))
					IPA_MPM_DBG(
						"QMI BW reg Req failed,might be SSR");
		}
		break;
	default:
		IPA_MPM_DBG("No op for UL channel, in teth state = %d",
			ipa_mpm_ctx->md[probe_id].teth_state);
		break;
	}

	atomic_inc(&ipa_mpm_ctx->probe_cnt);
	/* Check if ODL/USB DPL pipe is connected before probe */
	if (probe_id == IPA_MPM_MHIP_CH_ID_2) {
		if (ipa3_is_odl_connected())
			ret = ipa_mpm_set_dma_mode(
				IPA_CLIENT_MHI_PRIME_DPL_PROD,
				IPA_CLIENT_ODL_DPL_CONS, false);
		else if (atomic_read(&ipa_mpm_ctx->adpl_over_usb_available))
			ret = ipa_mpm_set_dma_mode(
				IPA_CLIENT_MHI_PRIME_DPL_PROD,
				IPA_CLIENT_USB_DPL_CONS, false);
		if (ret)
			IPA_MPM_ERR("DPL DMA to ODL/USB failed, ret = %d\n",
				ret);
	}
	mutex_lock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
	ipa_mpm_ctx->md[probe_id].init_complete = true;
	mutex_unlock(&ipa_mpm_ctx->md[probe_id].mhi_mutex);
	/* Update Flow control Monitoring, only for the teth UL Prod pipes */
	if (probe_id == IPA_MPM_MHIP_CH_ID_0) {
		ipa_ep_idx = ipa3_get_ep_mapping(ul_prod);
		ep = &ipa3_ctx->ep[ipa_ep_idx];
		/* not enable threshold based uc-flow-control */
		ret = ipa3_uc_send_enable_flow_control(ep->gsi_chan_hdl,
			0);
		IPA_MPM_DBG("Updated uc threshold to %d",
			ipa3_ctx->mpm_uc_thresh);
		if (ret) {
			IPA_MPM_ERR("Err %d flow control enable\n", ret);
			goto fail_flow_control;
		}
		IPA_MPM_DBG("Flow Control enabled for %d", probe_id);
		flow_ctrl_mask = atomic_read(&ipa_mpm_ctx->flow_ctrl_mask);
		add_delete = flow_ctrl_mask > 0 ? 1 : 0;
		ret = ipa3_uc_send_update_flow_control(flow_ctrl_mask,
							add_delete);
		if (ret) {
			IPA_MPM_ERR("Err %d flow control update\n", ret);
			goto fail_flow_control;
		}
		IPA_MPM_DBG("Flow Control updated for %d", probe_id);
	}
	/* cache the current ring-size */
	ipa3_ctx->mpm_ring_size_ul_cache = ipa3_ctx->mpm_ring_size_ul;
	ipa3_ctx->mpm_ring_size_dl_cache = ipa3_ctx->mpm_ring_size_dl;
	IPA_MPM_DBG("Mpm ring size ul/dl %d / %d",
		ipa3_ctx->mpm_ring_size_ul, ipa3_ctx->mpm_ring_size_dl);

	IPA_MPM_FUNC_EXIT();

	return 0;

fail_gsi_setup:
fail_start_channel:
fail_stop_channel:
fail_smmu:
fail_flow_control:
	if (ipa_mpm_ctx->dev_info.ipa_smmu_enabled)
		IPA_MPM_DBG("SMMU failed\n");
	if (is_acted)
		ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id, true,
					&is_acted);
	ipa_mpm_vote_unvote_ipa_clk(CLK_OFF, probe_id);
	ipa_assert();
	return ret;
}

static void ipa_mpm_init_mhip_channel_info(void)
{
	/* IPA_MPM_MHIP_CH_ID_0 => MHIP TETH PIPES  */
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_0].dl_cons.ipa_client =
		IPA_CLIENT_MHI_PRIME_TETH_PROD;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_0].dl_cons.ep_cfg =
		mhip_dl_teth_ep_cfg;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_0].ul_prod.ipa_client =
		IPA_CLIENT_MHI_PRIME_TETH_CONS;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_0].ul_prod.ep_cfg =
		mhip_ul_teth_ep_cfg;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_0].ul_prod.ep_cfg.aggr.aggr_byte_limit
			= ipa3_ctx->mpm_teth_aggr_size;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_0].mhip_client =
		IPA_MPM_MHIP_TETH;

	IPA_MPM_DBG("Teth Aggregation byte limit =%d\n",
		ipa3_ctx->mpm_teth_aggr_size);

	/* IPA_MPM_MHIP_CH_ID_1 => MHIP RMNET PIPES */
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_1].dl_cons.ipa_client =
		IPA_CLIENT_MHI_PRIME_RMNET_PROD;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_1].dl_cons.ep_cfg =
		mhip_dl_rmnet_ep_cfg;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_1].ul_prod.ipa_client =
		IPA_CLIENT_MHI_PRIME_RMNET_CONS;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_1].ul_prod.ep_cfg =
		mhip_ul_rmnet_ep_cfg;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_1].mhip_client =
		IPA_MPM_MHIP_USB_RMNET;

	/* IPA_MPM_MHIP_CH_ID_2 => MHIP ADPL PIPE */
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_2].dl_cons.ipa_client =
		IPA_CLIENT_MHI_PRIME_DPL_PROD;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_2].dl_cons.ep_cfg =
		mhip_dl_dpl_ep_cfg;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_2].ul_prod.ipa_client =
		IPA_CLIENT_MAX;
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_2].mhip_client =
		IPA_MPM_MHIP_USB_DPL;
}

static void ipa_mpm_mhi_remove_cb(struct mhi_device *mhi_dev)
{
	int mhip_idx;

	IPA_MPM_FUNC_ENTRY();

	for (mhip_idx = 0; mhip_idx < IPA_MPM_MHIP_CH_ID_MAX; mhip_idx++) {
		if (mhi_dev == ipa_mpm_ctx->md[mhip_idx].mhi_dev)
			break;
	}
	if (mhip_idx >= IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_DBG("remove_cb for mhip_idx = %d not probed before\n",
			mhip_idx);
		return;
	}

	IPA_MPM_DBG("remove_cb for mhip_idx = %d", mhip_idx);

	mutex_lock(&ipa_mpm_ctx->md[mhip_idx].mhi_mutex);
	ipa_mpm_ctx->md[mhip_idx].init_complete = false;
	mutex_unlock(&ipa_mpm_ctx->md[mhip_idx].mhi_mutex);

	if (mhip_idx == IPA_MPM_MHIP_CH_ID_0) {
		ipa3_uc_send_disable_flow_control();
		/* unmap uc-fc-mb */
		ipa_mpm_smmu_unmap_doorbell(MHIP_SMMU_DOMAIN_PCIE,
			ipa_mpm_ctx->uc_fc_db_iova);
	}

	ipa_mpm_mhip_shutdown(mhip_idx);

	atomic_dec(&ipa_mpm_ctx->probe_cnt);

	if (atomic_read(&ipa_mpm_ctx->probe_cnt) == 0) {
		/* Last probe done, reset Everything here */
		ipa_mpm_ctx->mhi_parent_dev = NULL;
		ipa_mpm_ctx->carved_smmu_cb.next_addr =
			ipa_mpm_ctx->carved_smmu_cb.va_start;
		atomic_set(&ipa_mpm_ctx->pcie_clk_total_cnt, 0);
		/* Force set to zero during SSR */
		atomic_set(&ipa_mpm_ctx->active_teth_count, 0);
		for (mhip_idx = 0;
			mhip_idx < IPA_MPM_MHIP_CH_ID_MAX; mhip_idx++) {
			atomic_set(
				&ipa_mpm_ctx->md[mhip_idx].clk_cnt.pcie_clk_cnt,
				0);
		}
	}

	IPA_MPM_FUNC_EXIT();
}

static void ipa_mpm_mhi_status_cb(struct mhi_device *mhi_dev,
				enum MHI_CB mhi_cb)
{
	int mhip_idx;
	enum mhip_status_type status;

	IPA_MPM_DBG("%d\n", mhi_cb);

	for (mhip_idx = 0; mhip_idx < IPA_MPM_MHIP_CH_ID_MAX; mhip_idx++) {
		if (mhi_dev == ipa_mpm_ctx->md[mhip_idx].mhi_dev)
			break;
	}
	if (mhip_idx >= IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_DBG("ignoring secondary callbacks\n");
		return;
	}

	mutex_lock(&ipa_mpm_ctx->md[mhip_idx].mhi_mutex);
	if (!ipa_mpm_ctx->md[mhip_idx].init_complete) {
		/*
		 * SSR might be in progress, dont have to vote/unvote for
		 * IPA clocks as it will be taken care in remove_cb/subsequent
		 * probe.
		 */
		IPA_MPM_DBG("SSR in progress, return\n");
		mutex_unlock(&ipa_mpm_ctx->md[mhip_idx].mhi_mutex);
		return;
	}
	mutex_unlock(&ipa_mpm_ctx->md[mhip_idx].mhi_mutex);

	switch (mhi_cb) {
	case MHI_CB_IDLE:
		break;
	case MHI_CB_LPM_ENTER:
		if (!ipa_mpm_ctx->md[mhip_idx].in_lpm) {
			status = ipa_mpm_start_stop_mhip_chan(
				IPA_MPM_MHIP_CHAN_DL,
				mhip_idx, MPM_MHIP_STOP);
			IPA_MPM_DBG("status = %d\n", status);
			ipa_mpm_vote_unvote_ipa_clk(CLK_OFF, mhip_idx);
			ipa_mpm_ctx->md[mhip_idx].in_lpm = true;
		} else {
			IPA_MPM_DBG("Already in lpm\n");
		}
		break;
	case MHI_CB_LPM_EXIT:
		if (ipa_mpm_ctx->md[mhip_idx].in_lpm) {
			ipa_mpm_vote_unvote_ipa_clk(CLK_ON, mhip_idx);
			status = ipa_mpm_start_stop_mhip_chan(
				IPA_MPM_MHIP_CHAN_DL,
				mhip_idx, MPM_MHIP_START);
			IPA_MPM_DBG("status = %d\n", status);
			ipa_mpm_ctx->md[mhip_idx].in_lpm = false;
		} else {
			IPA_MPM_DBG("Already out of lpm\n");
		}
		break;
	default:
		IPA_MPM_ERR("unexpected event %d\n", mhi_cb);
		break;
	}
}

static void ipa_mpm_mhip_map_prot(enum ipa_usb_teth_prot prot,
	enum ipa_mpm_mhip_client_type *mhip_client)
{
	switch (prot) {
	case IPA_USB_RNDIS:
		*mhip_client = IPA_MPM_MHIP_TETH;
		break;
	case IPA_USB_RMNET:
		*mhip_client = IPA_MPM_MHIP_USB_RMNET;
		break;
	case IPA_USB_DIAG:
		*mhip_client = IPA_MPM_MHIP_USB_DPL;
		break;
	default:
		*mhip_client = IPA_MPM_MHIP_NONE;
		break;
	}
	IPA_MPM_DBG("Mapped xdci prot %d -> MHIP prot %d\n", prot,
		*mhip_client);
}

int ipa_mpm_mhip_xdci_pipe_enable(enum ipa_usb_teth_prot xdci_teth_prot)
{
	int probe_id = IPA_MPM_MHIP_CH_ID_MAX;
	int i;
	enum ipa_mpm_mhip_client_type mhip_client;
	enum mhip_status_type status;
	int pipe_idx;
	bool is_acted = true;
	int ret = 0;

	if (ipa_mpm_ctx == NULL) {
		IPA_MPM_ERR("MPM not platform probed yet, returning ..\n");
		return 0;
	}

	ipa_mpm_mhip_map_prot(xdci_teth_prot, &mhip_client);

	for (i = 0; i < IPA_MPM_MHIP_CH_ID_MAX; i++) {
		if (ipa_mpm_pipes[i].mhip_client == mhip_client) {
			probe_id = i;
			break;
		}
	}

	if ((probe_id < IPA_MPM_MHIP_CH_ID_0) ||
		(probe_id >= IPA_MPM_MHIP_CH_ID_MAX)) {
		IPA_MPM_ERR("Unknown probe_id\n");
		return 0;
	}

	if (probe_id == IPA_MPM_MHIP_CH_ID_0) {
		/* For rndis, the MPM processing happens in WAN State IOCTL */
		IPA_MPM_DBG("MPM Xdci connect for rndis, no -op\n");
		return 0;
	}

	IPA_MPM_DBG("Connect xdci prot %d -> mhip_client = %d probe_id = %d\n",
			xdci_teth_prot, mhip_client, probe_id);

	ipa_mpm_ctx->md[probe_id].mhip_client = mhip_client;

	ret = ipa_mpm_vote_unvote_pcie_clk(CLK_ON, probe_id,
		false, &is_acted);
	if (ret) {
		IPA_MPM_ERR("Error cloking on PCIe clk, err = %d\n", ret);
			return ret;
	}

	/*
	 * Make sure to start Device side channels before
	 * starting Host side UL channels. This is to make
	 * sure device side access host side IPA only when
	 * Host IPA gets voted.
	 */
	ret = ipa_mpm_start_stop_remote_mhip_chan(probe_id,
						MPM_MHIP_START, false);
	if (ret) {
		/*
		 * This can fail only when modem is in SSR state.
		 * Eventually there would be a remove callback,
		 * so return a failure. Dont have to unvote PCIE here.
		 */
		IPA_MPM_ERR("MHIP remote chan start fail = %d\n",
				ret);
		return ret;
	}

	IPA_MPM_DBG("MHIP remote channel start success\n");

	switch (mhip_client) {
	case IPA_MPM_MHIP_USB_RMNET:
		ipa_mpm_set_dma_mode(IPA_CLIENT_USB_PROD,
			IPA_CLIENT_MHI_PRIME_RMNET_CONS, false);
		break;
	case IPA_MPM_MHIP_USB_DPL:
		IPA_MPM_DBG("connecting DPL prot %d\n", mhip_client);
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_CONNECTED);
		atomic_set(&ipa_mpm_ctx->adpl_over_usb_available, 1);
		return 0;
	default:
		IPA_MPM_ERR("mhip_client = %d not processed\n", mhip_client);
		if (is_acted) {
			ret = ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
				false, &is_acted);
			if (ret) {
				IPA_MPM_ERR("Err unvoting PCIe clk, err = %d\n",
					ret);
				return ret;
			}
		}
		ipa_assert();
		return -EINVAL;
	}

	if (mhip_client != IPA_MPM_MHIP_USB_DPL)
		/* Start UL MHIP channel for offloading teth connection */
		status = ipa_mpm_start_stop_mhip_chan(IPA_MPM_MHIP_CHAN_UL,
							probe_id,
							MPM_MHIP_START);
	switch (status) {
	case MHIP_STATUS_SUCCESS:
	case MHIP_STATUS_NO_OP:
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_CONNECTED);
		/* Register for BW indication from Q6*/
		if (!ipa3_qmi_reg_dereg_for_bw(true, BW_VOTE_XDCI_ENABLE))
			IPA_MPM_DBG("Fail regst QMI BW Indctn,might be SSR");

		pipe_idx = ipa3_get_ep_mapping(IPA_CLIENT_USB_PROD);

		/* Lift the delay for rmnet USB prod pipe */
		ipa3_xdci_ep_delay_rm(pipe_idx);
		if (status == MHIP_STATUS_NO_OP && is_acted) {
			/* Channels already have been started,
			 * we can devote for pcie clocks
			 */
			ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
				false, &is_acted);
		}
		break;
	case MHIP_STATUS_EP_NOT_READY:
		if (is_acted)
			ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
				false, &is_acted);
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_INPROGRESS);
		break;
	case MHIP_STATUS_FAIL:
	case MHIP_STATUS_BAD_STATE:
	case MHIP_STATUS_EP_NOT_FOUND:
		IPA_MPM_ERR("UL chan cant be started err =%d\n", status);
		if (is_acted)
			ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
				false, &is_acted);
		ret = -EFAULT;
		break;
	default:
		if (is_acted)
			ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
				false, &is_acted);
		IPA_MPM_ERR("Err not found\n");
		break;
	}
	return ret;
}

int ipa_mpm_mhip_xdci_pipe_disable(enum ipa_usb_teth_prot xdci_teth_prot)
{
	int probe_id = IPA_MPM_MHIP_CH_ID_MAX;
	int i;
	enum ipa_mpm_mhip_client_type mhip_client;
	enum mhip_status_type status;
	int ret = 0;
	bool is_acted = true;

	if (ipa_mpm_ctx == NULL) {
		IPA_MPM_ERR("MPM not platform probed, returning ..\n");
		return 0;
	}

	ipa_mpm_mhip_map_prot(xdci_teth_prot, &mhip_client);

	for (i = 0; i < IPA_MPM_MHIP_CH_ID_MAX; i++) {
		if (ipa_mpm_pipes[i].mhip_client == mhip_client) {
			probe_id = i;
			break;
		}
	}

	if ((probe_id < IPA_MPM_MHIP_CH_ID_0) ||
		(probe_id >= IPA_MPM_MHIP_CH_ID_MAX)) {
		IPA_MPM_ERR("Unknown probe_id\n");
		return 0;
	}

	if (probe_id == IPA_MPM_MHIP_CH_ID_0) {
		/* For rndis, the MPM processing happens in WAN State IOCTL */
		IPA_MPM_DBG("MPM Xdci disconnect for rndis, no -op\n");
		return 0;
	}

	IPA_MPM_DBG("xdci disconnect prot %d mhip_client = %d probe_id = %d\n",
			xdci_teth_prot, mhip_client, probe_id);
	/*
	 * Make sure to stop Device side channels before
	 * stopping Host side UL channels. This is to make
	 * sure device side doesn't access host side IPA if
	 * Host IPA gets unvoted.
	 */

	/* stop remote mhip-dpl ch if ODL not enable */
	if ((!atomic_read(&ipa_mpm_ctx->adpl_over_odl_available))
			|| (probe_id != IPA_MPM_MHIP_CH_ID_2)) {
		ret = ipa_mpm_start_stop_remote_mhip_chan(probe_id,
							MPM_MHIP_STOP, false);
		if (ret) {
			/*
			 * This can fail only when modem is in SSR state.
			 * Eventually there would be a remove callback,
			 * so return a failure.
			 */
			IPA_MPM_ERR("MHIP remote chan stop fail = %d\n", ret);
			return ret;
		}
		IPA_MPM_DBG("MHIP remote channels are stopped(id=%d)\n",
			probe_id);
	}


	switch (mhip_client) {
	case IPA_MPM_MHIP_USB_RMNET:
		ret = ipa_mpm_set_dma_mode(IPA_CLIENT_USB_PROD,
			IPA_CLIENT_APPS_LAN_CONS, true);
		if (ret) {
			IPA_MPM_ERR("failed to reset dma mode\n");
			return ret;
		}
		break;
	case IPA_MPM_MHIP_TETH:
		IPA_MPM_DBG("Rndis Disconnect, wait for wan_state ioctl\n");
		return 0;
	case IPA_MPM_MHIP_USB_DPL:
		IPA_MPM_DBG("Teth Disconnecting for DPL\n");

		/* change teth state only if ODL is disconnected */
		if (!ipa3_is_odl_connected()) {
			ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_INIT);
			ipa_mpm_ctx->md[probe_id].mhip_client =
				IPA_MPM_MHIP_NONE;
		}
		ret = ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
			false, &is_acted);
		if (ret)
			IPA_MPM_ERR("Error clking off PCIe clk err%d\n", ret);
		atomic_set(&ipa_mpm_ctx->adpl_over_usb_available, 0);
		return ret;
	default:
		IPA_MPM_ERR("mhip_client = %d not supported\n", mhip_client);
		return 0;
	}

	status = ipa_mpm_start_stop_mhip_chan(IPA_MPM_MHIP_CHAN_UL,
		probe_id, MPM_MHIP_STOP);

	switch (status) {
	case MHIP_STATUS_SUCCESS:
	case MHIP_STATUS_NO_OP:
	case MHIP_STATUS_EP_NOT_READY:
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_INIT);
		/* De-register for BW indication from Q6*/
		if (atomic_read(&ipa_mpm_ctx->active_teth_count) >= 1) {
			if (!ipa3_qmi_reg_dereg_for_bw(false,
				BW_UNVOTE_XDCI_DISABLE))
				IPA_MPM_DBG(
					"Failed De-rgstrng QMI BW Indctn,might be SSR");
		} else {
			IPA_MPM_ERR(
				"Active tethe count is %d",
				atomic_read(&ipa_mpm_ctx->active_teth_count));
		}
		break;
	case MHIP_STATUS_FAIL:
	case MHIP_STATUS_BAD_STATE:
	case MHIP_STATUS_EP_NOT_FOUND:
		IPA_MPM_ERR("UL chan cant be started err =%d\n", status);
		ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
			false, &is_acted);
		return -EFAULT;
		break;
	default:
		IPA_MPM_ERR("Err not found\n");
		break;
	}

	ret = ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id,
		false, &is_acted);

	if (ret) {
		IPA_MPM_ERR("Error cloking off PCIe clk, err = %d\n", ret);
		return ret;
	}
	ipa_mpm_ctx->md[probe_id].mhip_client = IPA_MPM_MHIP_NONE;

	return ret;
}

static int ipa_mpm_populate_smmu_info(struct platform_device *pdev)
{
	struct ipa_smmu_in_params smmu_in;
	struct ipa_smmu_out_params smmu_out;
	u32 carved_iova_ap_mapping[2];
	struct ipa_smmu_cb_ctx *cb;
	struct ipa_smmu_cb_ctx *ap_cb = ipa3_get_smmu_ctx(IPA_SMMU_CB_AP);
	int ret = 0;

	if (ipa_mpm_ctx->carved_smmu_cb.valid) {
		IPA_MPM_DBG("SMMU Context allocated, returning ..\n");
		return ret;
	}

	cb = &ipa_mpm_ctx->carved_smmu_cb;

	/* get IPA SMMU enabled status */
	smmu_in.smmu_client = IPA_SMMU_AP_CLIENT;
	if (ipa_get_smmu_params(&smmu_in, &smmu_out))
		ipa_mpm_ctx->dev_info.ipa_smmu_enabled = false;
	else
		ipa_mpm_ctx->dev_info.ipa_smmu_enabled =
		smmu_out.smmu_enable;

	/* get cache_coherent enable or not */
	ipa_mpm_ctx->dev_info.is_cache_coherent = ap_cb->is_cache_coherent;
	if (of_property_read_u32_array(pdev->dev.of_node, "qcom,iova-mapping",
		carved_iova_ap_mapping, 2)) {
		IPA_MPM_ERR("failed to read of_node %s\n",
			"qcom,mpm-iova-mapping");
		return -EINVAL;
	}
	ipa_mpm_ctx->dev_info.pcie_smmu_enabled = true;

	if (ipa_mpm_ctx->dev_info.ipa_smmu_enabled !=
		ipa_mpm_ctx->dev_info.pcie_smmu_enabled) {
		IPA_MPM_DBG("PCIE/IPA SMMU config mismatch\n");
		return -EINVAL;
	}

	cb->va_start = carved_iova_ap_mapping[0];
	cb->va_size = carved_iova_ap_mapping[1];
	cb->va_end = cb->va_start + cb->va_size;

	if (cb->va_end >= ap_cb->va_start) {
		IPA_MPM_ERR("MPM iommu and AP overlap addr 0x%lx\n",
				cb->va_start);
		ipa_assert();
		return -EFAULT;
	}

	cb->dev = ipa_mpm_ctx->dev_info.dev;
	cb->valid = true;
	cb->next_addr = cb->va_start;

	if (dma_set_mask_and_coherent(ipa_mpm_ctx->dev_info.dev,
		DMA_BIT_MASK(64))) {
		IPA_MPM_ERR("setting DMA mask to 64 failed.\n");
		return -EINVAL;
	}

	return ret;
}

static int ipa_mpm_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i = 0;
	int idx = 0;

	IPA_MPM_FUNC_ENTRY();

	if (ipa_mpm_ctx) {
		IPA_MPM_DBG("MPM is already probed, returning\n");
		return 0;
	}

	ret = ipa_register_ipa_ready_cb(ipa_mpm_ipa3_ready_cb, (void *)pdev);
	/*
	 * If we received -EEXIST, IPA has initialized. So we need
	 * to continue the probing process.
	 */
	if (!ret) {
		IPA_MPM_DBG("IPA not ready yet, registering callback\n");
		return ret;
	}
	IPA_MPM_DBG("IPA is ready, continue with probe\n");

	ipa_mpm_ctx = kzalloc(sizeof(*ipa_mpm_ctx), GFP_KERNEL);

	if (!ipa_mpm_ctx)
		return -ENOMEM;

	for (i = 0; i < IPA_MPM_MHIP_CH_ID_MAX; i++) {
		mutex_init(&ipa_mpm_ctx->md[i].mutex);
		mutex_init(&ipa_mpm_ctx->md[i].mhi_mutex);
	}
	mutex_init(&ipa_mpm_ctx->mutex);
	ipa_mpm_ctx->cache_index = 0;

	ipa_mpm_ctx->dev_info.pdev = pdev;
	ipa_mpm_ctx->dev_info.dev = &pdev->dev;

	/* uc_fc_fb, might define in dtsi */
	ipa_mpm_ctx->uc_fc_db = IPA_UC_FC_DB_ADDR;

	ipa_mpm_init_mhip_channel_info();

	if (of_property_read_u32(pdev->dev.of_node, "qcom,mhi-chdb-base",
		&ipa_mpm_ctx->dev_info.chdb_base)) {
		IPA_MPM_ERR("failed to read qcom,mhi-chdb-base\n");
		goto fail_probe;
	}
	IPA_MPM_DBG("chdb-base=0x%x\n", ipa_mpm_ctx->dev_info.chdb_base);

	if (of_property_read_u32(pdev->dev.of_node, "qcom,mhi-erdb-base",
		&ipa_mpm_ctx->dev_info.erdb_base)) {
		IPA_MPM_ERR("failed to read qcom,mhi-erdb-base\n");
		goto fail_probe;
	}
	IPA_MPM_DBG("erdb-base=0x%x\n", ipa_mpm_ctx->dev_info.erdb_base);

	ret = ipa_mpm_populate_smmu_info(pdev);

	if (ret) {
		IPA_MPM_DBG("SMMU Config failed\n");
		goto fail_probe;
	}

	atomic_set(&ipa_mpm_ctx->ipa_clk_total_cnt, 0);
	atomic_set(&ipa_mpm_ctx->pcie_clk_total_cnt, 0);
	atomic_set(&ipa_mpm_ctx->flow_ctrl_mask, 0);
	atomic_set(&ipa_mpm_ctx->active_teth_count, 0);
	atomic_set(&ipa_mpm_ctx->voted_before, 1);

	for (idx = 0; idx < IPA_MPM_MHIP_CH_ID_MAX; idx++) {
		ipa_mpm_ctx->md[idx].ul_prod.gsi_state = GSI_INIT;
		ipa_mpm_ctx->md[idx].dl_cons.gsi_state = GSI_INIT;
		atomic_set(&ipa_mpm_ctx->md[idx].clk_cnt.ipa_clk_cnt, 0);
		atomic_set(&ipa_mpm_ctx->md[idx].clk_cnt.pcie_clk_cnt, 0);
	}

	ret = mhi_driver_register(&mhi_driver);
	if (ret) {
		IPA_MPM_ERR("mhi_driver_register failed %d\n", ret);
		goto fail_probe;
	}
	IPA_MPM_FUNC_EXIT();
	return 0;

fail_probe:
	kfree(ipa_mpm_ctx);
	ipa_mpm_ctx = NULL;
	return -EFAULT;
}

static int ipa_mpm_remove(struct platform_device *pdev)
{
	IPA_MPM_FUNC_ENTRY();

	mhi_driver_unregister(&mhi_driver);
	IPA_MPM_FUNC_EXIT();
	return 0;
}

static const struct of_device_id ipa_mpm_dt_match[] = {
	{ .compatible = "qcom,ipa-mpm" },
	{},
};
MODULE_DEVICE_TABLE(of, ipa_mpm_dt_match);

static struct platform_driver ipa_ipa_mpm_driver = {
	.driver = {
		.name = "ipa_mpm",
		.of_match_table = ipa_mpm_dt_match,
	},
	.probe = ipa_mpm_probe,
	.remove = ipa_mpm_remove,
};

/**
 * ipa_mpm_init() - Registers ipa_mpm as a platform device for a APQ
 *
 * This function is called after bootup for APQ device.
 * ipa_mpm will register itself as a platform device, and probe
 * function will get called.
 *
 * Return: None
 */
static int __init ipa_mpm_init(void)
{
	IPA_MPM_DBG("register ipa_mpm platform device\n");
	return platform_driver_register(&ipa_ipa_mpm_driver);
}

/**
 * ipa3_is_mhip_offload_enabled() - check if IPA MPM module was initialized
 * successfully. If it is initialized, MHIP is enabled for teth
 *
 * Return value: 1 for yes; 0 for no
 */
int ipa3_is_mhip_offload_enabled(void)
{
	if (ipa_mpm_ctx == NULL)
		return 0;
	else
		return 1;
}

int ipa_mpm_panic_handler(char *buf, int size)
{
	int i;
	int cnt = 0;

	cnt = scnprintf(buf, size,
			"\n---- MHIP Active Clients Table ----\n");
	cnt += scnprintf(buf + cnt, size - cnt,
			"Total PCIe active clients count: %d\n",
			atomic_read(&ipa_mpm_ctx->pcie_clk_total_cnt));
	cnt += scnprintf(buf + cnt, size - cnt,
			"Total IPA active clients count: %d\n",
			atomic_read(&ipa_mpm_ctx->ipa_clk_total_cnt));

	for (i = 0; i < IPA_MPM_MHIP_CH_ID_MAX; i++) {
		cnt += scnprintf(buf + cnt, size - cnt,
			"client id: %d ipa vote cnt: %d pcie vote cnt\n", i,
			atomic_read(&ipa_mpm_ctx->md[i].clk_cnt.ipa_clk_cnt),
			atomic_read(&ipa_mpm_ctx->md[i].clk_cnt.pcie_clk_cnt));
	}
	return cnt;
}

/**
 * ipa3_get_mhip_gsi_stats() - Query MHIP gsi stats from uc
 * @stats:	[inout] stats blob from client populated by driver
 *
 * Returns:	0 on success, negative on failure
 *
 * @note Cannot be called from atomic context
 *
 */
int ipa3_get_mhip_gsi_stats(struct ipa_uc_dbg_ring_stats *stats)
{
	int i;

	if (!ipa3_ctx->mhip_ctx.dbg_stats.uc_dbg_stats_mmio) {
		IPAERR("bad parms NULL mhip_gsi_stats_mmio\n");
		return -EINVAL;
	}
	IPA_ACTIVE_CLIENTS_INC_SIMPLE();
	for (i = 0; i < MAX_MHIP_CHANNELS; i++) {
		stats->ring[i].ringFull = ioread32(
			ipa3_ctx->mhip_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGFULL_OFF);
		stats->ring[i].ringEmpty = ioread32(
			ipa3_ctx->mhip_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGEMPTY_OFF);
		stats->ring[i].ringUsageHigh = ioread32(
			ipa3_ctx->mhip_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGUSAGEHIGH_OFF);
		stats->ring[i].ringUsageLow = ioread32(
			ipa3_ctx->mhip_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGUSAGELOW_OFF);
		stats->ring[i].RingUtilCount = ioread32(
			ipa3_ctx->mhip_ctx.dbg_stats.uc_dbg_stats_mmio
			+ i * IPA3_UC_DEBUG_STATS_OFF +
			IPA3_UC_DEBUG_STATS_RINGUTILCOUNT_OFF);
	}
	IPA_ACTIVE_CLIENTS_DEC_SIMPLE();


	return 0;
}

/**
 * ipa3_mpm_enable_adpl_over_odl() - Enable or disable ADPL over ODL
 * @enable:	true for enable, false for disable
 *
 * Returns:	0 on success, negative on failure
 *
 */
int ipa3_mpm_enable_adpl_over_odl(bool enable)
{
	int ret;
	bool is_acted = true;

	IPA_MPM_FUNC_ENTRY();

	if (!ipa3_is_mhip_offload_enabled()) {
		IPA_MPM_ERR("mpm ctx is NULL\n");
		return -EPERM;
	}

	if (enable) {
		/* inc clk count and set DMA to ODL */
		IPA_MPM_DBG("mpm enabling ADPL over ODL\n");

		ret = ipa_mpm_vote_unvote_pcie_clk(CLK_ON,
			IPA_MPM_MHIP_CH_ID_2, false, &is_acted);
		if (ret) {
			IPA_MPM_ERR("Err %d cloking on PCIe clk\n", ret);
				return ret;
		}

		ret = ipa_mpm_set_dma_mode(IPA_CLIENT_MHI_PRIME_DPL_PROD,
			IPA_CLIENT_ODL_DPL_CONS, false);
		if (ret) {
			IPA_MPM_ERR("MPM failed to set dma mode to ODL\n");
			if (is_acted)
				ipa_mpm_vote_unvote_pcie_clk(CLK_OFF,
					IPA_MPM_MHIP_CH_ID_2,
					false,
					&is_acted);
			return ret;
		}

		/* start remote mhip-dpl ch */
		ret = ipa_mpm_start_stop_remote_mhip_chan(IPA_MPM_MHIP_CH_ID_2,
					MPM_MHIP_START, false);
		if (ret) {
			/*
			 * This can fail only when modem is in SSR state.
			 * Eventually there would be a remove callback,
			 * so return a failure. Dont have to unvote PCIE here.
			 */
			IPA_MPM_ERR("MHIP remote chan start fail = %d\n",
					ret);
			return ret;
		}
		IPA_MPM_DBG("MHIP remote channels are started(id=%d)\n",
			IPA_MPM_MHIP_CH_ID_2);
		atomic_set(&ipa_mpm_ctx->adpl_over_odl_available, 1);

		ipa_mpm_change_teth_state(IPA_MPM_MHIP_CH_ID_2,
			IPA_MPM_TETH_CONNECTED);
	} else {
		/* stop remote mhip-dpl ch if adpl not enable */
		if (!atomic_read(&ipa_mpm_ctx->adpl_over_usb_available)) {
			ret = ipa_mpm_start_stop_remote_mhip_chan(
				IPA_MPM_MHIP_CH_ID_2, MPM_MHIP_STOP, false);
			if (ret) {
				/*
				 * This can fail only when modem in SSR state.
				 * Eventually there would be a remove callback,
				 * so return a failure.
				 */
				IPA_MPM_ERR("MHIP remote chan stop fail = %d\n",
					ret);
				return ret;
			}
			IPA_MPM_DBG("MHIP remote channels are stopped(id=%d)\n",
				IPA_MPM_MHIP_CH_ID_2);
		}
		atomic_set(&ipa_mpm_ctx->adpl_over_odl_available, 0);

		/* dec clk count and set DMA to USB */
		IPA_MPM_DBG("mpm disabling ADPL over ODL\n");
		ret = ipa_mpm_vote_unvote_pcie_clk(CLK_OFF,
						IPA_MPM_MHIP_CH_ID_2,
						false,
						&is_acted);
		if (ret) {
			IPA_MPM_ERR("Err %d cloking off PCIe clk\n",
				ret);
			return ret;
		}

		ret = ipa_mpm_set_dma_mode(IPA_CLIENT_MHI_PRIME_DPL_PROD,
			IPA_CLIENT_USB_DPL_CONS, false);
		if (ret) {
			IPA_MPM_ERR("MPM failed to set dma mode to USB\n");
			if (ipa_mpm_vote_unvote_pcie_clk(CLK_ON,
							IPA_MPM_MHIP_CH_ID_2,
							false,
							&is_acted))
				IPA_MPM_ERR("Err clocking on pcie\n");
			return ret;
		}

		/* If USB is not available then reset teth state */
		if (atomic_read(&ipa_mpm_ctx->adpl_over_usb_available)) {
			IPA_MPM_DBG("mpm enabling ADPL over USB\n");
		} else {
			ipa_mpm_change_teth_state(IPA_MPM_MHIP_CH_ID_2,
				IPA_MPM_TETH_INIT);
			IPA_MPM_DBG("USB disconnected. ADPL on standby\n");
		}
	}

	IPA_MPM_FUNC_EXIT();
	return ret;
}

int ipa3_qmi_reg_dereg_for_bw(bool bw_reg, int bw_reg_dereg_type)
{
	int rt;

	mutex_lock(&ipa_mpm_ctx->mutex);
	ipa_mpm_ctx->bw_reg_dereg_cache[
		ipa_mpm_ctx->cache_index].bw_reg_dereg_type =
		bw_reg_dereg_type;
	ipa_mpm_ctx->bw_reg_dereg_cache[
		ipa_mpm_ctx->cache_index].ref_count =
		atomic_read(&ipa_mpm_ctx->active_teth_count);
	ipa_mpm_ctx->cache_index =
		(ipa_mpm_ctx->cache_index + 1) % IPA_MAX_BW_REG_DEREG_CACHE;
	mutex_unlock(&ipa_mpm_ctx->mutex);

	if (bw_reg) {
		atomic_inc(&ipa_mpm_ctx->active_teth_count);
		if (atomic_read(&ipa_mpm_ctx->active_teth_count) == 1) {
			rt = ipa3_qmi_req_ind(true);
			if (rt < 0) {
				IPA_MPM_ERR("QMI BW regst fail, rt = %d", rt);
				atomic_dec(&ipa_mpm_ctx->active_teth_count);
				/* Using voted_before for keeping track of
				 * request successful or not, so that we don't
				 * request for devote when tether turned off
				 */
				atomic_set(&ipa_mpm_ctx->voted_before, 0);
				return false;
			}
			IPA_MPM_DBG("QMI BW regst success from %d",
				ipa_mpm_ctx->bw_reg_dereg_cache[
					(ipa_mpm_ctx->cache_index -
					1) % IPA_MAX_BW_REG_DEREG_CACHE].
					bw_reg_dereg_type);
		} else {
			IPA_MPM_DBG("bw_change to %d no-op, teth_count = %d",
				bw_reg,
				atomic_read(&ipa_mpm_ctx->active_teth_count));
		}
	} else {
		atomic_dec(&ipa_mpm_ctx->active_teth_count);
		if (atomic_read(&ipa_mpm_ctx->active_teth_count) == 0) {
			if (atomic_read(&ipa_mpm_ctx->voted_before) == 0) {
				atomic_inc(&ipa_mpm_ctx->active_teth_count);
				atomic_set(&ipa_mpm_ctx->voted_before, 1);
				return false;
			}
			rt = ipa3_qmi_req_ind(false);
			if (rt < 0) {
				IPA_MPM_ERR("QMI BW de-regst fail, rt= %d", rt);
				return false;
			}
			IPA_MPM_DBG("QMI BW De-regst success %d",
				ipa_mpm_ctx->bw_reg_dereg_cache[
					(ipa_mpm_ctx->cache_index -
					1) % IPA_MAX_BW_REG_DEREG_CACHE].
					bw_reg_dereg_type);
		} else {
			IPA_MPM_DBG("bw_change to %d no-op, teth_count = %d",
				bw_reg,
				atomic_read(&ipa_mpm_ctx->active_teth_count));
		}
	}
	return true;
}

late_initcall(ipa_mpm_init);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MHI Proxy Manager Driver");
