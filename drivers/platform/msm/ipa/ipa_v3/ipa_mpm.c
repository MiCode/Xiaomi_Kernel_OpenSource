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

#include <linux/dma-mapping.h>
#include <linux/list.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/module.h>
#include <linux/mhi.h>
#include <linux/msm_gsi.h>
#include <linux/delay.h>
#include <linux/log2.h>
#include "../ipa_common_i.h"
#include "ipa_i.h"

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

#define IPA_MPM_NUM_RING_DESC 0x400
#define IPA_MPM_RING_LEN (IPA_MPM_NUM_RING_DESC - 10)

#define IPA_MPM_MHI_HOST_UL_CHANNEL 4
#define IPA_MPM_MHI_HOST_DL_CHANNEL  5
#define DEFAULT_AGGR_TIME_LIMIT 1000 /* 1ms */
#define DEFAULT_AGGR_PKT_LIMIT 0
#define TETH_AGGR_TIME_LIMIT 10000
#define TETH_AGGR_BYTE_LIMIT 24
#define TETH_AGGR_DL_BYTE_LIMIT 16
#define TRE_BUFF_SIZE 32768
#define IPA_HOLB_TMR_EN 0x1
#define IPA_HOLB_TMR_DIS 0x0
#define RNDIS_IPA_DFLT_RT_HDL 0
#define IPA_POLL_FOR_EMPTINESS_NUM 50
#define IPA_POLL_FOR_EMPTINESS_SLEEP_USEC 20
#define IPA_CHANNEL_STOP_IN_PROC_TO_MSEC 5
#define IPA_CHANNEL_STOP_IN_PROC_SLEEP_USEC 200

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

enum ipa_mpm_start_stop_type {
	STOP,
	START,
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

/* each pair of UL/DL channels are defined below */
static const struct mhi_device_id mhi_driver_match_table[] = {
	{ .chan = "IP_HW_MHIP_0" }, // for rmnet pipes
	{ .chan = "IP_HW_MHIP_1" }, // for MHIP teth pipes - rndis/wifi
	{ .chan = "IP_HW_ADPL" }, // DPL/ODL DL pipe
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
		.aggr_en = IPA_BYPASS_AGGR, /* temporarily disabled */
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

/* For configuring IPA_CLIENT_MHIP_DPL_PROD */
static struct ipa_ep_cfg mhip_dl_dpl_ep_cfg = {
	.mode = {
		.mode = IPA_DMA,
		.dst = IPA_CLIENT_USB_CONS,
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

struct producer_rings {
	struct mhi_p_desc *tr_va;
	struct mhi_p_desc *er_va;
	void *tre_buff_va[IPA_MPM_RING_LEN];
	dma_addr_t tr_pa;
	dma_addr_t er_pa;
	dma_addr_t tre_buff_iova[IPA_MPM_RING_LEN];
	/*
	 * The iova generated for AP CB,
	 * used only for dma_map_single to flush the cache.
	 */
	dma_addr_t ap_iova_er;
	dma_addr_t ap_iova_tr;
	dma_addr_t ap_iova_buff[IPA_MPM_RING_LEN];
};

struct ipa_mpm_mhi_driver {
	struct mhi_device *mhi_dev;
	struct producer_rings ul_prod_ring;
	struct producer_rings dl_prod_ring;
	struct ipa_mpm_channel ul_prod;
	struct ipa_mpm_channel dl_cons;
	enum ipa_mpm_mhip_client_type mhip_client;
	enum ipa_mpm_teth_state teth_state;
	struct mutex mutex;
	bool init_complete;
	struct mutex lpm_mutex;
	bool in_lpm;
};

struct ipa_mpm_context {
	struct ipa_mpm_dev_info dev_info;
	struct ipa_mpm_mhi_driver md[IPA_MPM_MAX_MHIP_CHAN];
	struct mutex mutex;
	atomic_t ipa_clk_ref_cnt;
	atomic_t pcie_clk_ref_cnt;
	atomic_t probe_cnt;
	struct device *parent_pdev;
	struct ipa_smmu_cb_ctx carved_smmu_cb;
	struct device *mhi_parent_dev;
};

#define IPA_MPM_DESC_SIZE (sizeof(struct mhi_p_desc))
#define IPA_MPM_RING_TOTAL_SIZE (IPA_MPM_RING_LEN * IPA_MPM_DESC_SIZE)
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
static int ipa_mpm_start_stop_mhip_data_path(int probe_id,
	enum ipa_mpm_start_stop_type start);
static int ipa_mpm_probe(struct platform_device *pdev);
static int ipa_mpm_vote_unvote_pcie_clk(enum ipa_mpm_clk_vote_type vote,
	int probe_id);
static void ipa_mpm_vote_unvote_ipa_clk(enum ipa_mpm_clk_vote_type vote);
static enum mhip_status_type ipa_mpm_start_stop_mhip_chan(
	enum ipa_mpm_mhip_chan mhip_chan,
	int probe_id,
	enum ipa_mpm_start_stop_type start_stop);

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

	if (carved_iova >= cb->va_end) {
		IPA_MPM_ERR("running out of carved_iova %x\n", carved_iova);
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
						IPA_MPM_RING_TOTAL_SIZE, dir);
		ret = ipa3_iommu_map(ipa_smmu_domain, iova_p,
					pa_p, size_p, prot);
		if (ret) {
			IPA_MPM_ERR("IPA IOMMU returned failure, ret = %d\n",
					ret);
			ipa_assert();
		}

		pcie_smmu_domain = iommu_get_domain_for_dev(
			ipa_mpm_ctx->mhi_parent_dev);
		ret = iommu_map(pcie_smmu_domain, iova_p, pa_p, size_p, prot);

		if (ret) {
			IPA_MPM_ERR("PCIe IOMMU returned failure, ret = %d\n",
				ret);
			ipa_assert();
		}

		iova = iova_p;
		cb->next_addr = iova_p + size_p;
	} else {
		iova = dma_map_single(ipa3_ctx->pdev, va_addr,
					IPA_MPM_RING_TOTAL_SIZE, dir);
		*ap_cb_iova = iova;
	}
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
		WARN_ON(1);
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
		iommu_unmap(pcie_smmu_domain, iova_p, size_p);
		iommu_unmap(ipa_smmu_domain, iova_p, size_p);

		cb->next_addr -= size_p;
		dma_unmap_single(ipa3_ctx->pdev, ap_cb_iova,
			IPA_MPM_RING_TOTAL_SIZE, dir);
	} else {
		dma_unmap_single(ipa3_ctx->pdev, ap_cb_iova,
			IPA_MPM_RING_TOTAL_SIZE, dir);
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

	if (carved_iova >= cb->va_end) {
		IPA_MPM_ERR("running out of carved_iova %x\n", carved_iova);
		ipa_assert();
	}

	smmu_enabled = (ipa_mpm_ctx->dev_info.ipa_smmu_enabled &&
		ipa_mpm_ctx->dev_info.pcie_smmu_enabled) ? 1 : 0;

	if (smmu_enabled) {
		IPA_SMMU_ROUND_TO_PAGE(carved_iova, pa_addr, IPA_MPM_PAGE_SIZE,
					iova_p, pa_p, size_p);
		if (smmu_domain == MHIP_SMMU_DOMAIN_IPA) {
			ipa_smmu_domain = ipa3_get_smmu_domain();
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
			iommu_unmap(ipa_smmu_domain, iova_p, size_p);
		} else if (smmu_domain == MHIP_SMMU_DOMAIN_PCIE) {
			pcie_smmu_domain = iommu_get_domain_for_dev(
				ipa_mpm_ctx->mhi_parent_dev);
			 iommu_unmap(pcie_smmu_domain, iova_p, size_p);
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
	struct mhi_p_desc *ev_ring;
	struct mhi_p_desc *tr_ring;
	int tr_ring_sz, ev_ring_sz;
	dma_addr_t ev_ring_iova, tr_ring_iova;
	dma_addr_t ap_cb_iova;
	dma_addr_t ap_cb_er_iova;
	struct ipa_request_gsi_channel_params gsi_params;
	int dir;
	int i;
	void *buff;
	int result;
	int k;
	struct ipa3_ep_context *ep;

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

	ev_ring_sz = IPA_MPM_RING_TOTAL_SIZE;
	ev_ring = kzalloc(ev_ring_sz, GFP_KERNEL);
	if (!ev_ring)
		goto fail_evt_alloc;

	tr_ring_sz = IPA_MPM_RING_TOTAL_SIZE;
	tr_ring = kzalloc(tr_ring_sz, GFP_KERNEL);
	if (!tr_ring)
		goto fail_tr_alloc;

	tr_ring[0].re_type = MHIP_RE_NOP;

	dir = IPA_CLIENT_IS_PROD(mhip_client) ?
		DMA_TO_HIPA : DMA_FROM_HIPA;

	/* allocate transfer ring elements */
	for (i = 1, k = 1; i < IPA_MPM_RING_LEN; i++, k++) {
		buff = kzalloc(TRE_BUFF_SIZE, GFP_KERNEL);

		if (!buff)
			goto fail_buff_alloc;

		tr_ring[i].buffer_ptr =
			ipa_mpm_smmu_map(buff, TRE_BUFF_SIZE, dir,
				&ap_cb_iova);
		if (!tr_ring[i].buffer_ptr)
			goto fail_smmu_map_ring;

		if (IPA_CLIENT_IS_PROD(mhip_client)) {
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tre_buff_va[k] =
							buff;
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tre_buff_iova[k] =
							tr_ring[i].buffer_ptr;
		} else {
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tre_buff_va[k] =
							buff;
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tre_buff_iova[k] =
							tr_ring[i].buffer_ptr;
		}


		tr_ring[i].buff_len = TRE_BUFF_SIZE;
		tr_ring[i].chain = 0;
		tr_ring[i].ieob = 0;
		tr_ring[i].ieot = 0;
		tr_ring[i].bei = 0;
		tr_ring[i].sct = 0;
		tr_ring[i].re_type = MHIP_RE_XFER;

		if (IPA_CLIENT_IS_PROD(mhip_client))
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_buff[k] =
				ap_cb_iova;
		else
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_buff[k] =
				ap_cb_iova;
	}

	tr_ring_iova = ipa_mpm_smmu_map(tr_ring, IPA_MPM_PAGE_SIZE, dir,
		&ap_cb_iova);
	if (!tr_ring_iova)
		goto fail_smmu_map_ring;

	ev_ring_iova = ipa_mpm_smmu_map(ev_ring, IPA_MPM_PAGE_SIZE, dir,
		&ap_cb_er_iova);
	if (!ev_ring_iova)
		goto fail_smmu_map_ring;

	/* Store Producer channel rings */
	if (IPA_CLIENT_IS_PROD(mhip_client)) {
		/* Device UL */
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_va = ev_ring;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_va = tr_ring;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_pa = ev_ring_iova;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_pa = tr_ring_iova;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_tr =
			ap_cb_iova;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_er =
			ap_cb_er_iova;
	} else {
		/* Host UL */
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_va = ev_ring;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_va = tr_ring;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_pa = ev_ring_iova;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_pa = tr_ring_iova;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_tr =
			ap_cb_iova;
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
		(IPA_MPM_RING_LEN) * GSI_EVT_RING_RE_SIZE_16B;
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

	if (IPA_CLIENT_IS_PROD(mhip_client))
		gsi_params.evt_scratch.mhip.rp_mod_threshold = 4;

	/* Channel Params */
	gsi_params.chan_params.prot = GSI_CHAN_PROT_MHIP;
	gsi_params.chan_params.dir = IPA_CLIENT_IS_PROD(mhip_client) ?
		GSI_CHAN_DIR_TO_GSI : GSI_CHAN_DIR_FROM_GSI;
	/* chan_id is set in ipa3_request_gsi_channel() */
	gsi_params.chan_params.re_size = GSI_CHAN_RE_SIZE_16B;
	gsi_params.chan_params.ring_len =
		(IPA_MPM_RING_LEN) * GSI_EVT_RING_RE_SIZE_16B;
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

	/* Release channel */
	if (mhi_idx == IPA_MPM_MHIP_CH_ID_2) {
		/* Note: DPL not supported yet */
		IPA_MPM_ERR("DPL not supported yet. returning\n");
		return;
	}

	ipa3_set_reset_client_prod_pipe_delay(true,
					IPA_CLIENT_USB_PROD);

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
	for (i = 1; i < IPA_MPM_RING_LEN; i++) {
		if (IPA_CLIENT_IS_PROD(mhip_client)) {
			ipa_mpm_smmu_unmap(
			(dma_addr_t)
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tre_buff_iova[i],
			TRE_BUFF_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_buff[i]);
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tre_buff_iova[i]
								= 0;
			kfree(
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tre_buff_va[i]);
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tre_buff_va[i]
								= NULL;
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_buff[i]
								= 0;
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tre_buff_iova[i]
								= 0;
		} else {
			ipa_mpm_smmu_unmap(
			(dma_addr_t)
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tre_buff_iova[i],
			TRE_BUFF_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_buff[i]
			);
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tre_buff_iova[i]
								= 0;
			kfree(
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tre_buff_va[i]);
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tre_buff_va[i]
								= NULL;
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_buff[i]
								= 0;
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tre_buff_iova[i]
								= 0;
		}
	}

	/* deallocate/Unmap rings */
	if (IPA_CLIENT_IS_PROD(mhip_client)) {
		ipa_mpm_smmu_unmap(
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_pa,
			IPA_MPM_PAGE_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_er);

		ipa_mpm_smmu_unmap(
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_pa,
			IPA_MPM_PAGE_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_tr);

		kfree(ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_va);
		kfree(ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_va);

		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.er_va = NULL;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.tr_va = NULL;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_tr = 0;
		ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_er = 0;


	} else {
		ipa_mpm_smmu_unmap(
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_pa,
			IPA_MPM_PAGE_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].dl_prod_ring.ap_iova_tr);
		ipa_mpm_smmu_unmap(
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_pa,
			IPA_MPM_PAGE_SIZE, dir,
			ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.ap_iova_er);

		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_pa = 0;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_pa = 0;

		kfree(ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_va);
		kfree(ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_va);

		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.er_va = NULL;
		ipa_mpm_ctx->md[mhi_idx].ul_prod_ring.tr_va = NULL;
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
	if (mhip_idx == IPA_MPM_MHIP_CH_ID_2) {
		IPA_MPM_ERR("DPL - return\n");
		return;
	}

	ipa_mpm_clean_mhip_chan(mhip_idx, ul_prod_chan);
	ipa_mpm_clean_mhip_chan(mhip_idx, dl_cons_chan);


	mutex_lock(&ipa_mpm_ctx->md[mhip_idx].lpm_mutex);
	if (!ipa_mpm_ctx->md[mhip_idx].in_lpm) {
		ipa_mpm_vote_unvote_ipa_clk(CLK_OFF);
		ipa_mpm_ctx->md[mhip_idx].in_lpm = true;
	}
	mutex_unlock(&ipa_mpm_ctx->md[mhip_idx].lpm_mutex);
	IPA_MPM_FUNC_EXIT();
}

/*
 * Turning on/OFF PCIE Clock is done once for all clients.
 * Always vote for Probe_ID 0 as a standard.
 */
static int ipa_mpm_vote_unvote_pcie_clk(enum ipa_mpm_clk_vote_type vote,
	int probe_id)
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

	if (ipa_mpm_ctx->md[probe_id].mhi_dev == NULL) {
		IPA_MPM_ERR("MHI not initialized yet\n");
		return 0;
	}
	if (vote == CLK_ON) {
		if ((atomic_read(&ipa_mpm_ctx->pcie_clk_ref_cnt) == 0)) {
			result = mhi_device_get_sync(
				ipa_mpm_ctx->md[probe_id].mhi_dev);
			if (result) {
				IPA_MPM_ERR("mhi_sync_get failed %d\n",
					result);
				return result;
			}
			IPA_MPM_DBG("PCIE clock now ON\n");
		}
		atomic_inc(&ipa_mpm_ctx->pcie_clk_ref_cnt);
	} else {
		if ((atomic_read(&ipa_mpm_ctx->pcie_clk_ref_cnt) == 1)) {
			mhi_device_put(ipa_mpm_ctx->md[probe_id].mhi_dev);
			IPA_MPM_DBG("PCIE clock off ON\n");
		}
		atomic_dec(&ipa_mpm_ctx->pcie_clk_ref_cnt);
	}

	return result;
}

/*
 * Turning on/OFF IPA Clock is done only once- for all clients
 */
static void ipa_mpm_vote_unvote_ipa_clk(enum ipa_mpm_clk_vote_type vote)
{
	if (vote > CLK_OFF)
		return;

	if (vote == CLK_ON) {
		if ((!atomic_read(&ipa_mpm_ctx->ipa_clk_ref_cnt))) {
			IPA_ACTIVE_CLIENTS_INC_SPECIAL("ipa_mpm");
			IPA_MPM_DBG("IPA clock now ON\n");
		}
		atomic_inc(&ipa_mpm_ctx->ipa_clk_ref_cnt);
	} else {
		if ((atomic_read(&ipa_mpm_ctx->ipa_clk_ref_cnt) == 1)) {
			IPA_ACTIVE_CLIENTS_DEC_SPECIAL("ipa_mpm");
			IPA_MPM_DBG("IPA clock now OFF\n");
		}
		atomic_dec(&ipa_mpm_ctx->ipa_clk_ref_cnt);
	}
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

	is_start = (start_stop == START) ? true : false;

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

int ipa_mpm_notify_wan_state(void)
{
	int probe_id = IPA_MPM_MHIP_CH_ID_MAX;
	int i;
	static enum mhip_status_type status;
	int ret = 0;
	enum ipa_client_type ul_chan, dl_chan;
	enum ipa_mpm_mhip_client_type mhip_client = IPA_MPM_MHIP_TETH;

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
	get_ipa3_client(probe_id, &ul_chan, &dl_chan);

	/* Start UL MHIP channel for offloading the tethering connection */
	ret = ipa_mpm_vote_unvote_pcie_clk(CLK_ON, probe_id);

	if (ret) {
		IPA_MPM_ERR("Error cloking on PCIe clk, err = %d\n", ret);
		return ret;
	}

	status = ipa_mpm_start_stop_mhip_chan(
				IPA_MPM_MHIP_CHAN_UL, probe_id, START);
	switch (status) {
	case MHIP_STATUS_SUCCESS:
	case MHIP_STATUS_NO_OP:
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_CONNECTED);
		ret = ipa_mpm_start_stop_mhip_data_path(probe_id, START);

		if (ret) {
			IPA_MPM_ERR("Couldnt start UL GSI channel");
			ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
			return ret;
		}

		if (status == MHIP_STATUS_NO_OP) {
			/* Channels already have been started,
			 * we can devote for pcie clocks
			 */
			ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
		}
		break;
	case MHIP_STATUS_EP_NOT_READY:
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_INPROGRESS);
		break;
	case MHIP_STATUS_FAIL:
	case MHIP_STATUS_BAD_STATE:
	case MHIP_STATUS_EP_NOT_FOUND:
		IPA_MPM_ERR("UL chan cant be started err =%d\n", status);
		ret = ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
		return -EFAULT;
	default:
		IPA_MPM_ERR("Err not found\n");
		ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
		ret = -EFAULT;
		break;
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

	IPA_MPM_ERR("Reading channel for chan %d, ep = %d, gsi_chan_hdl = %d\n",
		chan, ep, ep->gsi_chan_hdl);

	res = ipa3_get_gsi_chan_info(&chan_info, ep->gsi_chan_hdl);
	if (res)
		IPA_MPM_ERR("Reading of channel failed for ep %d\n", ep);
}

static int ipa_mpm_start_stop_mhip_data_path(int probe_id,
	enum ipa_mpm_start_stop_type start)
{
	int ipa_ep_idx;
	int res = 0;
	enum ipa_client_type ul_chan, dl_chan;

	if (probe_id >= IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("Unknown probe_id\n");
		return 0;
	}
	get_ipa3_client(probe_id, &ul_chan, &dl_chan);
	IPA_MPM_DBG("Start/Stop Data Path ? = %d\n", start);


	/* MHIP Start Data path:
	 * IPA MHIP Producer: remove HOLB
	 * IPA MHIP Consumer : no op as there is no delay on these pipes.
	 */
	if (start) {
		IPA_MPM_DBG("Enabling data path\n");
		if (ul_chan != IPA_CLIENT_MAX) {
			/* Remove HOLB on the producer pipe */
			IPA_MPM_DBG("Removing HOLB on ep = %s\n",
				__stringify(ul_chan));
			ipa_ep_idx = ipa3_get_ep_mapping(ul_chan);

			if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
				IPAERR("failed to get idx");
				return ipa_ep_idx;
			}

			res = ipa3_enable_data_path(ipa_ep_idx);
			if (res)
				IPA_MPM_ERR("Enable data path failed res=%d\n",
					res);
		}
	} else {
		IPA_MPM_DBG("Disabling data path\n");
		if (ul_chan != IPA_CLIENT_MAX) {
			/* Set HOLB on the producer pipe */
			ipa_ep_idx = ipa3_get_ep_mapping(ul_chan);

			if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
				IPAERR("failed to get idx");
				return ipa_ep_idx;
			}

			res = ipa3_disable_data_path(ipa_ep_idx);
			if (res)
				IPA_MPM_ERR("disable data path failed res=%d\n",
					res);
		}
	}

	return res;
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

	IPA_MPM_FUNC_ENTRY();

	if (ipa_mpm_ctx == NULL) {
		IPA_MPM_ERR("ipa_mpm_ctx is NULL not expected, returning..\n");
		return -ENOMEM;
	}

	probe_id = get_idx_from_id(mhi_id);

	if (probe_id >= IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("chan=%s is not supported for now\n", mhi_id);
		return -EPERM;
	}

	if (ipa_mpm_ctx->md[probe_id].init_complete) {
		IPA_MPM_ERR("Probe initialization already done, returning\n");
		return -EPERM;
	}

	IPA_MPM_DBG("Received probe for id=%d\n", probe_id);

	if (probe_id == IPA_MPM_MHIP_CH_ID_2) {
		/* NOTE :: DPL not supported yet , remove later */
		IPA_MPM_DBG("DPL not supported yet - returning for DPL..\n");
		return 0;
	}

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

	ipa_mpm_vote_unvote_pcie_clk(CLK_ON, probe_id);
	mutex_lock(&ipa_mpm_ctx->md[probe_id].lpm_mutex);
	ipa_mpm_vote_unvote_ipa_clk(CLK_ON);
	ipa_mpm_ctx->md[probe_id].in_lpm = false;
	mutex_unlock(&ipa_mpm_ctx->md[probe_id].lpm_mutex);
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
	if (probe_id != IPA_MPM_MHIP_CH_ID_2) {
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
			ch->chan_props.ch_ctx.rlen = (IPA_MPM_RING_LEN) *
				GSI_EVT_RING_RE_SIZE_16B;
			/* Store Event properties */
			ch->evt_props.ev_ctx.update_rp_modc = 0;
			ch->evt_props.ev_ctx.update_rp_intmodt = 0;
			ch->evt_props.ev_ctx.ertype = 1;
			ch->evt_props.ev_ctx.rlen = (IPA_MPM_RING_LEN) *
				GSI_EVT_RING_RE_SIZE_16B;
			ch->evt_props.ev_ctx.buff_size = TRE_BUFF_SIZE;
			ch->evt_props.device_db =
				ipa_mpm_ctx->dev_info.erdb_base +
				ch->chan_props.ch_ctx.erindex * 8;
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
		ch->chan_props.ch_ctx.rlen = (IPA_MPM_RING_LEN) *
			GSI_EVT_RING_RE_SIZE_16B;
		/* Store Event properties */
		ch->evt_props.ev_ctx.update_rp_modc = 0;
		ch->evt_props.ev_ctx.update_rp_intmodt = 0;
		ch->evt_props.ev_ctx.ertype = 1;
		ch->evt_props.ev_ctx.rlen = (IPA_MPM_RING_LEN) *
			GSI_EVT_RING_RE_SIZE_16B;
		ch->evt_props.ev_ctx.buff_size = TRE_BUFF_SIZE;
		ch->evt_props.device_db =
			ipa_mpm_ctx->dev_info.erdb_base +
			ch->chan_props.ch_ctx.erindex * 8;
	}
	/* connect Host GSI pipes with MHI' protocol */
	if (probe_id != IPA_MPM_MHIP_CH_ID_2)  {
		ret = ipa_mpm_connect_mhip_gsi_pipe(ul_prod,
			probe_id, &ul_out_params);
		if (ret) {
			IPA_MPM_ERR("failed connecting MPM client %d\n",
					ul_prod);
			goto fail_gsi_setup;
		}
	}
	ret = ipa_mpm_connect_mhip_gsi_pipe(dl_cons, probe_id, &dl_out_params);
	if (ret) {
		IPA_MPM_ERR("connecting MPM client = %d failed\n",
			dl_cons);
		goto fail_gsi_setup;
	}
	if (probe_id != IPA_MPM_MHIP_CH_ID_2)  {
		if (ul_prod != IPA_CLIENT_MAX) {
			ch = &ipa_mpm_ctx->md[probe_id].ul_prod;
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
	}

	if (dl_cons != IPA_CLIENT_MAX) {
		ch = &ipa_mpm_ctx->md[probe_id].dl_cons;
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
		goto fail_smmu;
	}

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
			((IPA_MPM_RING_LEN - 1) * GSI_CHAN_RE_SIZE_16B);

		iowrite32(wp_addr, db_addr);

		IPA_MPM_DBG("Host UL TR  DB = 0X%0x, wp_addr = 0X%0x",
			db_addr, wp_addr);

		iounmap(db_addr);
		ipa_mpm_read_channel(ul_prod);
	}

	/* Ring UL PRODUCER EVENT RING (HOST IPA -> DEVICE IPA) Doorbell
	 * Ring the event DB to a value outside the
	 * ring range such that rp and wp never meet.
	 */
	if (ul_prod != IPA_CLIENT_MAX) {
		ipa_ep_idx = ipa3_get_ep_mapping(ul_prod);

		if (ipa_ep_idx == IPA_EP_NOT_ALLOCATED) {
			IPA_MPM_ERR("fail to alloc EP.\n");
			goto fail_start_channel;
		}
		ep = &ipa3_ctx->ep[ipa_ep_idx];

		IPA_MPM_DBG("for ep_idx %d , gsi_evt_ring_hdl = %d\n",
			ipa_ep_idx, ep->gsi_evt_ring_hdl);
		gsi_query_evt_ring_db_addr(ep->gsi_evt_ring_hdl,
			&evt_ring_db_addr_low, &evt_ring_db_addr_high);

		IPA_MPM_DBG("Host UL ER PA DB = 0X%0x\n",
			evt_ring_db_addr_low);

		db_addr = ioremap((phys_addr_t)(evt_ring_db_addr_low), 4);

		wp_addr = ipa_mpm_ctx->md[probe_id].ul_prod_ring.er_pa +
			((IPA_MPM_RING_LEN + 1) * GSI_EVT_RING_RE_SIZE_16B);
		IPA_MPM_DBG("Host UL ER  DB = 0X%0x, wp_addr = 0X%0x",
			db_addr, wp_addr);

		iowrite32(wp_addr, db_addr);
		iounmap(db_addr);
	}

	/* Ring DEVICE IPA DL CONSUMER Event Doorbell */
	if (ul_prod != IPA_CLIENT_MAX) {
		db_addr = ioremap((phys_addr_t)
			(ipa_mpm_ctx->md[probe_id].ul_prod.evt_props.device_db),
			4);

		wp_addr = ipa_mpm_ctx->md[probe_id].ul_prod_ring.tr_pa +
			((IPA_MPM_RING_LEN + 1) * GSI_EVT_RING_RE_SIZE_16B);

		iowrite32(wp_addr, db_addr);
		iounmap(db_addr);
	}

	/* Ring DL PRODUCER (DEVICE IPA -> HOST IPA) Doorbell */
	if (dl_cons != IPA_CLIENT_MAX) {
		db_addr = ioremap((phys_addr_t)
		(ipa_mpm_ctx->md[probe_id].dl_cons.chan_props.device_db),
		4);

		wp_addr = ipa_mpm_ctx->md[probe_id].dl_prod_ring.tr_pa +
			((IPA_MPM_RING_LEN - 1) * GSI_CHAN_RE_SIZE_16B);

		IPA_MPM_DBG("Device DL TR  DB = 0X%0X, wp_addr = 0X%0x",
			db_addr, wp_addr);

		iowrite32(wp_addr, db_addr);

		iounmap(db_addr);
	}

	/*
	 * Ring event ring DB on Device side.
	 * ipa_mpm should ring the event DB to a value outside the
	 * ring range such that rp and wp never meet.
	 */
	if (dl_cons != IPA_CLIENT_MAX) {
		db_addr =
		ioremap(
		(phys_addr_t)
		(ipa_mpm_ctx->md[probe_id].dl_cons.evt_props.device_db),
		4);

		wp_addr = ipa_mpm_ctx->md[probe_id].dl_prod_ring.er_pa +
			((IPA_MPM_RING_LEN + 1) * GSI_EVT_RING_RE_SIZE_16B);

		iowrite32(wp_addr, db_addr);
		IPA_MPM_DBG("Device  UL ER  DB = 0X%0X,wp_addr = 0X%0x",
			db_addr, wp_addr);
		iounmap(db_addr);
	}

	/* Ring DL EVENT RING CONSUMER (DEVICE IPA CONSUMER) Doorbell */
	if (dl_cons != IPA_CLIENT_MAX) {
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
			((IPA_MPM_RING_LEN + 1) * GSI_EVT_RING_RE_SIZE_16B);
		iowrite32(wp_addr, db_addr);
		IPA_MPM_DBG("Host  DL ER  DB = 0X%0X, wp_addr = 0X%0x",
			db_addr, wp_addr);
		iounmap(db_addr);
	}

	/* Check if TETH connection is in progress.
	 * If teth isn't started by now, then Stop UL channel.
	 */
	switch (ipa_mpm_ctx->md[probe_id].teth_state) {
	case IPA_MPM_TETH_INIT:
		/* No teth started yet, disable UL channel */
		ipa_mpm_start_stop_mhip_chan(IPA_MPM_MHIP_CHAN_UL,
						probe_id, STOP);

		/* Disable data path */
		if (ipa_mpm_start_stop_mhip_data_path(probe_id, STOP)) {
			IPA_MPM_ERR("MHIP Enable data path failed\n");
			goto fail_start_channel;
		}
		ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
		break;
	case IPA_MPM_TETH_INPROGRESS:
	case IPA_MPM_TETH_CONNECTED:
		IPA_MPM_DBG("UL channel is already started, continue\n");
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_CONNECTED);

		/* Enable data path */
		if (ipa_mpm_start_stop_mhip_data_path(probe_id, START)) {
			IPA_MPM_ERR("MHIP Enable data path failed\n");
			goto fail_start_channel;
		}

		/* Lyft the delay for rmnet USB prod pipe */
		ipa3_set_reset_client_prod_pipe_delay(false,
			IPA_CLIENT_USB_PROD);
		break;
	default:
		IPA_MPM_DBG("No op for UL channel, in teth state = %d",
			ipa_mpm_ctx->md[probe_id].teth_state);
		break;
	}

	atomic_inc(&ipa_mpm_ctx->probe_cnt);
	IPA_MPM_FUNC_EXIT();
	return 0;

fail_gsi_setup:
fail_start_channel:
fail_smmu:
	if (ipa_mpm_ctx->dev_info.ipa_smmu_enabled)
		IPA_MPM_DBG("SMMU failed\n");
	ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
	ipa_mpm_vote_unvote_ipa_clk(CLK_OFF);
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
	ipa_mpm_pipes[IPA_MPM_MHIP_CH_ID_0].mhip_client =
		IPA_MPM_MHIP_TETH;

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

	ipa_mpm_mhip_shutdown(mhip_idx);
	atomic_dec(&ipa_mpm_ctx->probe_cnt);

	if (atomic_read(&ipa_mpm_ctx->probe_cnt) == 0) {
		/* Last probe done, reset Everything here */
		ipa_mpm_ctx->mhi_parent_dev = NULL;
		ipa_mpm_ctx->carved_smmu_cb.next_addr =
			ipa_mpm_ctx->carved_smmu_cb.va_start;
		atomic_set(&ipa_mpm_ctx->pcie_clk_ref_cnt, 0);
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

	mutex_lock(&ipa_mpm_ctx->md[mhip_idx].lpm_mutex);
	switch (mhi_cb) {
	case MHI_CB_IDLE:
		break;
	case MHI_CB_LPM_ENTER:
		if (!ipa_mpm_ctx->md[mhip_idx].in_lpm) {
			status = ipa_mpm_start_stop_mhip_chan(
				IPA_MPM_MHIP_CHAN_DL,
				mhip_idx, STOP);
			IPA_MPM_DBG("status = %d\n", status);
			ipa_mpm_vote_unvote_ipa_clk(CLK_OFF);
			ipa_mpm_ctx->md[mhip_idx].in_lpm = true;
		} else {
			IPA_MPM_DBG("Already in lpm\n");
		}
		break;
	case MHI_CB_LPM_EXIT:
		if (ipa_mpm_ctx->md[mhip_idx].in_lpm) {
			ipa_mpm_vote_unvote_ipa_clk(CLK_ON);
			status = ipa_mpm_start_stop_mhip_chan(
				IPA_MPM_MHIP_CHAN_DL,
				mhip_idx, START);
			IPA_MPM_DBG("status = %d\n", status);
			ipa_mpm_ctx->md[mhip_idx].in_lpm = false;
		} else {
			IPA_MPM_DBG("Already out of lpm\n");
		}
		break;
	case MHI_CB_EE_RDDM:
	case MHI_CB_PENDING_DATA:
	case MHI_CB_SYS_ERROR:
	case MHI_CB_FATAL_ERROR:
		IPA_MPM_ERR("unexpected event %d\n", mhi_cb);
		break;
	}
	mutex_unlock(&ipa_mpm_ctx->md[mhip_idx].lpm_mutex);
}

static int ipa_mpm_set_dma_mode(enum ipa_client_type src_pipe,
	enum ipa_client_type dst_pipe)
{
	int result = 0;
	struct ipa_ep_cfg ep_cfg = { { 0 } };

	IPA_MPM_FUNC_ENTRY();
	IPA_MPM_DBG("DMA from %d to %d\n", src_pipe, dst_pipe);

	/* Set USB PROD PIPE DMA to MHIP PROD PIPE */
	ep_cfg.mode.mode = IPA_DMA;
	ep_cfg.mode.dst = dst_pipe;
	ep_cfg.seq.set_dynamic = true;

	result = ipa_cfg_ep(ipa_get_ep_mapping(src_pipe), &ep_cfg);
	IPA_MPM_FUNC_EXIT();

	return result;
}

int ipa_mpm_reset_dma_mode(enum ipa_client_type src_pipe,
	enum ipa_client_type dst_pipe)
{
	int result = 0;
	struct ipa_ep_cfg ep_cfg = { { 0 } };

	IPA_MPM_FUNC_ENTRY();
	IPA_MPM_DBG("DMA from %d to %d\n", src_pipe, dst_pipe);

	/* Set USB PROD PIPE DMA to MHIP PROD PIPE */
	ep_cfg.mode.mode = IPA_BASIC;
	ep_cfg.mode.dst = IPA_CLIENT_APPS_LAN_CONS;
	ep_cfg.seq.set_dynamic = true;

	result = ipa_cfg_ep(ipa_get_ep_mapping(src_pipe), &ep_cfg);
	IPA_MPM_FUNC_EXIT();

	return result;
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

	if (probe_id == IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("Unknown probe_id\n");
		return 0;
	}

	IPA_MPM_DBG("Connect xdci prot %d -> mhip_client = %d probe_id = %d\n",
		xdci_teth_prot, mhip_client, probe_id);

	ipa_mpm_ctx->md[probe_id].mhip_client = mhip_client;

	switch (mhip_client) {
	case IPA_MPM_MHIP_USB_RMNET:
		ipa_mpm_set_dma_mode(IPA_CLIENT_USB_PROD,
			IPA_CLIENT_MHI_PRIME_RMNET_CONS);
		break;
	case IPA_MPM_MHIP_TETH:
	case IPA_MPM_MHIP_USB_DPL:
		IPA_MPM_DBG("Teth connecting for prot %d\n", mhip_client);
		return 0;
	default:
		IPA_MPM_ERR("mhip_client = %d not supported\n", mhip_client);
		ret = 0;
		break;
	}

	/* Start UL MHIP channel for offloading the tethering connection */
	ret = ipa_mpm_vote_unvote_pcie_clk(CLK_ON, probe_id);

	if (ret) {
		IPA_MPM_ERR("Error cloking on PCIe clk, err = %d\n", ret);
		return ret;
	}

	status = ipa_mpm_start_stop_mhip_chan(IPA_MPM_MHIP_CHAN_UL,
						probe_id, START);

	switch (status) {
	case MHIP_STATUS_SUCCESS:
	case MHIP_STATUS_NO_OP:
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_CONNECTED);
		ipa_mpm_start_stop_mhip_data_path(probe_id, START);
		/* Lift the delay for rmnet USB prod pipe */
		ipa3_set_reset_client_prod_pipe_delay(false,
			IPA_CLIENT_USB_PROD);
		if (status == MHIP_STATUS_NO_OP) {
			/* Channels already have been started,
			 * we can devote for pcie clocks
			 */
			ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
		}
		break;
	case MHIP_STATUS_EP_NOT_READY:
		ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_INPROGRESS);
		break;
	case MHIP_STATUS_FAIL:
	case MHIP_STATUS_BAD_STATE:
	case MHIP_STATUS_EP_NOT_FOUND:
		IPA_MPM_ERR("UL chan cant be started err =%d\n", status);
		ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
		ret = -EFAULT;
		break;
	default:
		ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
		IPA_MPM_ERR("Err not found\n");
		break;
	}
	return ret;
}

int ipa_mpm_mhip_ul_data_stop(enum ipa_usb_teth_prot xdci_teth_prot)
{
	int probe_id = IPA_MPM_MHIP_CH_ID_MAX;
	int i;
	enum ipa_mpm_mhip_client_type mhip_client;
	int ret = 0;

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

	if (probe_id == IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("Invalid probe_id\n");
		return 0;
	}

	IPA_MPM_DBG("Map xdci prot %d to mhip_client = %d probe_id = %d\n",
		xdci_teth_prot, mhip_client, probe_id);

	ret = ipa_mpm_start_stop_mhip_data_path(probe_id, STOP);

	if (ret)
		IPA_MPM_ERR("Error stopping UL path, err = %d\n", ret);

	return ret;
}

int ipa_mpm_mhip_xdci_pipe_disable(enum ipa_usb_teth_prot xdci_teth_prot)
{
	int probe_id = IPA_MPM_MHIP_CH_ID_MAX;
	int i;
	enum ipa_mpm_mhip_client_type mhip_client;
	enum mhip_status_type status;
	int ret = 0;

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

	if (probe_id == IPA_MPM_MHIP_CH_ID_MAX) {
		IPA_MPM_ERR("Invalid probe_id\n");
		return 0;
	}

	IPA_MPM_DBG("xdci disconnect prot %d mhip_client = %d probe_id = %d\n",
			xdci_teth_prot, mhip_client, probe_id);

	switch (mhip_client) {
	case IPA_MPM_MHIP_USB_RMNET:
	case IPA_MPM_MHIP_TETH:
		IPA_MPM_DBG("Teth Disconnecting for prot %d\n", mhip_client);
		break;
	case IPA_MPM_MHIP_USB_DPL:
		IPA_MPM_DBG("Teth Disconnecting for DPL, return\n");
		return 0;
	default:
		IPA_MPM_ERR("mhip_client = %d not supported\n", mhip_client);
		return 0;
	}

	status = ipa_mpm_start_stop_mhip_chan(IPA_MPM_MHIP_CHAN_UL,
		probe_id, STOP);

	switch (status) {
	case MHIP_STATUS_SUCCESS:
	case MHIP_STATUS_NO_OP:
	case MHIP_STATUS_EP_NOT_READY:
		ipa_mpm_change_teth_state(probe_id, IPA_MPM_TETH_INIT);
		ipa_mpm_start_stop_mhip_data_path(probe_id, STOP);
		break;
	case MHIP_STATUS_FAIL:
	case MHIP_STATUS_BAD_STATE:
	case MHIP_STATUS_EP_NOT_FOUND:
		IPA_MPM_ERR("UL chan cant be started err =%d\n", status);
		ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);
		return -EFAULT;
		break;
	default:
		IPA_MPM_ERR("Err not found\n");
		break;
	}

	ret = ipa_mpm_vote_unvote_pcie_clk(CLK_OFF, probe_id);

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

	if (cb->va_start >= ap_cb->va_start || cb->va_end >= ap_cb->va_start) {
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
		mutex_init(&ipa_mpm_ctx->md[i].lpm_mutex);
	}

	ipa_mpm_ctx->dev_info.pdev = pdev;
	ipa_mpm_ctx->dev_info.dev = &pdev->dev;

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

	atomic_set(&ipa_mpm_ctx->ipa_clk_ref_cnt, 0);
	atomic_set(&ipa_mpm_ctx->pcie_clk_ref_cnt, 0);

	for (idx = 0; idx < IPA_MPM_MHIP_CH_ID_MAX; idx++) {
		ipa_mpm_ctx->md[idx].ul_prod.gsi_state = GSI_INIT;
		ipa_mpm_ctx->md[idx].dl_cons.gsi_state = GSI_INIT;
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

late_initcall(ipa_mpm_init);
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("MHI Proxy Manager Driver");
