// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2023-2024, Qualcomm Innovation Center, Inc. All rights reserved.
 */
#define pr_fmt(fmt) "%s " fmt, KBUILD_MODNAME

#include <linux/bitfield.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#if IS_ENABLED(CONFIG_IPC_LOGGING)
#include <linux/ipc_logging.h>
#endif
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>

#include <soc/qcom/crm.h>

#define CREATE_TRACE_POINTS
#include "trace-crm.h"

#define field_get(_mask, _reg) (((_reg) & (_mask)) >> (ffs(_mask) - 1))

#define CRM_DRV_IPC_LOG_SIZE		2
#define MAX_NAME_LENGTH			20

#define PERF_OL_VCD			0
#define BW_VOTE_VCD			1
#define BW_PT_VOTE_VCD			2
#define MAX_VCD_TYPE			3

/* Capability flags  */
#define PERF_OL_VOTING_FLAG	BIT(0)
#define BW_VOTING_FLAG		BIT(1)
#define BW_PT_VOTING_FLAG	BIT(2)

#define VPAGE_SHIFT_BITS		0xFFF

/* Applicable for HW & SW DRVs BW Registers */
#define PERF_OL_VALUE_BITS		0x7

/* Applicable for HW & SW DRVs BW Registers */
#define BW_VOTE_VALID			BIT(29)
/* Applicable only for SW DRVs BW PT Registers */
#define BW_PT_VOTE_VALID		BIT(29)
#define BW_PT_VOTE_TRIGGER		BIT(0)
/* Applicable only for SW DRVs BW Registers */
#define BW_VOTE_RESP_REQ		BIT(31)

/* Set 1 to Enable IRQ for each VCD */
#define IRQ_ENABLE_BIT			BIT(0)
#define IRQ_CLEAR_BIT			BIT(0)

/* Offsets for CURR_PERF_OL Register */
#define CURR_PER_OL_MASK		0x7

/* Set 1 to Enable CHN_BEHAVE and CHANNEL_SWITCH_CTRL for each HW DRV */
#define CHN_BEHAVE_BIT			BIT(0)
#define CHN_SWITCH_CTRL		BIT(1)

/* SW DRV has ACTIVE, SLEEP and WAKE PWR STATES */
#define MAX_SW_DRV_PWR_STATES		3

/* Time out for ACTIVE Only PWR STATE completion IRQ */
#define CRM_TIMEOUT_MS			msecs_to_jiffies(CONFIG_QCOM_RPMH_TIMEOUT)

#define CH0				0
#define CH0_CHN_BUSY			BIT(0)
#define CH1				1
#define CH1_CHN_BUSY			BIT(1)

#define crm_print_reg(addr, val)\
			pr_warn("addr:0x%x, val:0x%x\n", addr, val)
#define crm_print_hw_reg(drv_num, channel, res_type, res_num, pwr_st, addr, val)\
			pr_warn("drv:%d, chn:%d, %s:%d, pwr_st:%d, addr:0x%x, val:0x%x\n",\
						drv_num, channel, res_type == PERF_OL_VCD ?\
						"vcd" : "node", res_num, pwr_st, addr, val)
#define crm_print_sw_reg(drv_num, res_type, res_num, pwr_st, addr, val)\
			pr_warn("drv:%d, %s:%d, pwr_st:%d, addr:0x%x, val:0x%x\n",\
						drv_num, res_type == PERF_OL_VCD ?\
						"vcd" : "node", res_num, pwr_st, addr, val)

enum {
	CRM_VERSION,
	MAJOR_VERSION,
	MINOR_VERSION,
	CRM_CFG_PARAM_1,
	NUM_OF_NODES_PT,
	NUM_VCD_VOTED_BY_BW,
	NUM_SW_DRVS,
	NUM_HW_DRVS,
	NUM_OF_RAILS,
	NUM_VCD_VOTED_BY_PERF_OL,
	NUM_CHANNELS,
	NUM_PWR_STATES_PER_CH,
	CRM_CFG_PARAM_2,
	NUM_OF_NODES,
	CRM_ENABLE,
	CFG_REG_MAX,
};

enum {
/* CRM DRV Register */
	DRV_BASE,
	DRV_DISTANCE,
/* VCD or ND Distance */
	DRV_RESOURCE_DISTANCE,
/* DRV's PWR_ST Registers */
	PWR_ST0,
	PWR_ST1,
	PWR_ST2,
	PWR_ST3,
	PWR_ST4,
/* DRV's PWR_ST Passthrough Registers */
	PWR_ST0_PT = PWR_ST0,
	PWR_ST1_PT = PWR_ST1,
	PWR_ST2_PT = PWR_ST2,
	PWR_ST3_PT = PWR_ST3,
	PWR_ST4_PT = PWR_ST4,
/* Offset for power state distances in a channel */
	PWR_ST_CHN_DISTANCE,
/* VCD's IRQ Registers, one per VCD at VCD_DISTANCE */
	IRQ_STATUS,
	IRQ_CLEAR,
	IRQ_ENABLE,
	FSM_STATUS,
	CRMB_PT_TRIGGER,
	STATUS,
	PWR_IDX_STATUS,
	CRM_CLIENT_REG_MAX,
};

enum {
/* SW DRV's PWR_ST mapped to PWR_ST0/1/2 for ACTIVE/SLEEP/WAKE */
	ACTIVE_VOTE = PWR_ST0,
	SLEEP_VOTE = PWR_ST1,
	WAKE_VOTE = PWR_ST2,
};

enum {
/* DRV's Channel Registers, one per DRV at CH_DRV_DISTANCE */
	CHN_BUSY,
	CHN_UPDATE,
	CHN_BEHAVE,
	CHN_DRV_DISTANCE,
	CHN_REG_MAX,
};

enum {
/* CRM DRV Register */
	CRM_BASE,
	CRM_DISTANCE,
	CRMB_BASE = CRM_BASE,
	CRMB_DISTANCE = CRM_DISTANCE,
/* CRMB Registers */
	STATUS_BE,
	STATUS_FE,
	CRMB_REG_MAX,
};

enum {
/* CRM DRV Register */
	CRMB_PT_BASE = CRM_BASE,
	CRMB_PT_DISTANCE = CRM_DISTANCE,
/* CRMB_PT Registers */
	TCS_CMD_DATA,
	TCS_CMD_ADDR,
	TCS_CMD_CTRL,
	TCS_CMD_STATUS,
	TCS_CMD_ENABLE,
	CRMB_PT_FSM_STATUS,
	CRMB_PT_REG_MAX,
};

enum {
/* CRM DRV Register */
	CRMC_BASE = CRM_BASE,
	CRMC_DISTANCE = CRM_DISTANCE,
/* CRMC Registers */
	AGGR_PERF_OL,
	AGGR_PERF_OL_RESOURCE_DISTANCE,
	CURR_PERF_OL,
	CURR_PERF_OL_RESOURCE_DISTANCE,
	SEQ_STATUS,
	SEQ_STATUS_RESOURCE_DISTANCE,
	CRMC_REG_MAX,
};

enum {
/* CRM DRV Register */
	CRMV_BASE = CRM_BASE,
	CRMV_DISTANCE = CRM_DISTANCE,
/* CRMV Registers */
	AGGR_VOL_STS,
	SEQ_VOL_STS,
	CURR_VOL_STS,
	RAIL_FSM_STS,
	RAIL_TCS_STS,
	CRMV_REG_MAX,
};

enum channel_type {
	CHN_IN_USE,
	CHN_FREE,
};

struct crm_desc {
	bool set_chn_behave;
	bool set_hw_chn_switch_ctrl;
	u32 crm_capability;
	u32 cfg_regs[CFG_REG_MAX];
	u32 chn_regs[CHN_REG_MAX];
	u32 crmb_regs[CRMB_REG_MAX];
	u32 crmb_pt_regs[CRMB_PT_REG_MAX];
	u32 crmc_regs[CRMC_REG_MAX];
	u32 crmv_regs[CRMV_REG_MAX];
	u32 hw_drv_perf_ol_vcd_regs[CRM_CLIENT_REG_MAX];
	u32 hw_drv_bw_vote_vcd_regs[CRM_CLIENT_REG_MAX];
	u32 hw_drv_bw_pt_vote_vcd_regs[CRM_CLIENT_REG_MAX];
	u32 sw_drv_perf_ol_vcd_regs[CRM_CLIENT_REG_MAX];
	u32 sw_drv_bw_vote_vcd_regs[CRM_CLIENT_REG_MAX];
	u32 sw_drv_bw_pt_vote_vcd_regs[CRM_CLIENT_REG_MAX];
};

/**
 * struct crm_sw_votes: SW DRV's ACTIVE_VOTEs in progress.
 * One per VCD.
 *
 * @cmd:                The ACTIVE_VOTE being sent to CRM.
 * @compl:              Wait for completion if the cmd->wait is set.
 *                      Applicable only for ACTIVE_VOTEs.
 * @in_progress:        Indicates if the cmd is in flight.
 * @wait:               Wait queue used to wait for @in_progress to be false.
 *                      This is needed because HW do not keep a record of new
 *                      requests issued until current one is completed.
 */
struct crm_sw_votes {
	struct crm_cmd cmd;
	struct completion compl;
	bool in_progress;
	wait_queue_head_t wait;
};

/**
 * struct crm_vcd: The Virtual Clock Domain's (VCDs) of the CRM.
 * One per VCD type.
 *
 * @cache:              Cache of vcd's power_state to data
 * @num_pwr_states:     Number of pwr state that DRV VCD can vote for.
 * @num_resources:      Number of VCD resources (for PERF_OL votes) OR
 *                      Number of Node resoureces (for BW votes)
 * @cache_dirty:        Flag to indicate if all the votes are applied.
 * @offsets:            Register offsets for DRV controller.
 * @sw_votes:           Cache of SW DRV's ACTIVE_VOTEs.
 */
struct crm_vcd {
	u32 **cache;
	u32 num_pwr_states;
	u32 num_resources;
	u32 *offsets;
	bool cache_dirty;
	struct crm_sw_votes *sw_votes;
};

/**
 * struct crm_drv: The Direct Resource Voter (DRV) of the
 * CESTA Resource manager (CRM).
 *
 * @name:               Controller identifier.
 * @base:               Base address of the CRM device.
 * @drv_id:             DRV (Direct Resource Voter) number.
 * @num_channels:       Number of Channels, Applicable only for HW DRV
 * @vcd:                VCDs in this DRV.
 * @irq:                IRQ at gic.
 * @initialized:        Whether DRV is initialized
 * @lock:               Synchronize state of the controller.  If CRM's cache's
 *                      lock will also be held, the order is: drv->cache_lock
 *                      then drv->lock.
 * @cache_lock:         Synchronize VCD cache updates
 * @client:             Handle to the DRV's client.
 * @ipc_log_ctx:        IPC logger handle
 */
struct crm_drv {
	enum crm_drv_type drv_type;
	char name[MAX_NAME_LENGTH];
	void __iomem *base;
	u32 drv_id;
	u32 num_channels;
	u32 *offsets;
	struct crm_vcd vcd[MAX_VCD_TYPE];
	spinlock_t lock;
	spinlock_t cache_lock;
	int irq;
	bool initialized;
	bool set_hw_chn_switch_ctrl;
	void *ipc_log_ctx;
};

/**
 * struct crm_mgr: The CRM HW block used for aggregating votes
 * from SW and HW DRVs.
 *
 * @name:                   CRM HW block name.
 * @base:                   Base address of the CRM block.
 * @num_resources:          Number of PERF_OL or BW_VOTE resources.
 * @offsets:                Register offsets for CRM manager.
 */
struct crm_mgr {
	char name[MAX_NAME_LENGTH];
	void __iomem *base;
	u32 num_resources;
	u32 *offsets;
};

/**
 * struct crm_drv_top: Our representation of the top CRM device.
 *
 * @name:               CRM device name.
 * @base:               Base address of the CRM device.
 * @hw_drvs:            Controller for each HW DRV
 * @num_hw_drvs:        Number of HW DRV controllers in the CRM device
 * @max_hw_drv:         Max id of HW DRV controller in use
 * @num_channels:       Number of Channels, Applicable only for HW DRV
 * @sw_drvs:            Controller for each SW DRV
 * @num_sw_drvs:        Number of SW DRV controllers in the CRM device
 * @max_sw_drv:         Max id of SW DRV controller in use
 * @crmb_mgr:           Controller for CRMB device.
 * @crmb_pt_mgr:        Controller for CRMB_PT device.
 * @crmc_mgr:           Controller for CRMC device.
 * @crmv_mgr:           Controller for CRMV device.
 * @list:               CRM device added in crm_dev_list.
 * @desc:               CRM description
 * @dev:                CRM dev
 * @pdev:               CRM platform device
 */
struct crm_drv_top {
	char name[MAX_NAME_LENGTH];
	void __iomem *common;
	void __iomem *base;
	struct crm_drv *hw_drvs;
	int num_hw_drvs;
	int max_hw_drv;
	u32 num_channels;
	struct crm_drv *sw_drvs;
	int num_sw_drvs;
	int max_sw_drv;
	struct crm_mgr crmb_mgr;
	struct crm_mgr crmb_pt_mgr;
	struct crm_mgr crmc_mgr;
	struct crm_mgr crmv_mgr;
	struct list_head list;
	const struct crm_desc *desc;
	struct device *dev;
	struct platform_device *pdev;
};

static LIST_HEAD(crm_dev_list);

static inline u32 get_crm_phy_addr(void __iomem *base)
{
	return page_to_phys(vmalloc_to_page(base));
}

static inline u32 crm_get_channel_offset(const struct crm_drv *drv, u32 reg)
{
	return drv->offsets[reg] + drv->drv_id * drv->offsets[CHN_DRV_DISTANCE];
}

static void write_crm_channel(const struct crm_drv *drv, u32 reg, u32 data)
{
	u32 offset = crm_get_channel_offset(drv, reg);

	writel_relaxed(data, drv->base + offset);
}

static u32 read_crm_channel(const struct crm_drv *drv, u32 reg)
{
	u32 offset = crm_get_channel_offset(drv, reg);

	return readl_relaxed(drv->base + offset);
}

static inline u32 crm_get_offset(const struct crm_drv *drv, u32 reg, u32 ch, u32 vcd_type,
			  u32 resource_idx)
{
	const struct crm_vcd *vcd = &drv->vcd[vcd_type];
	u32 offset;

	offset = vcd->offsets[DRV_BASE] + drv->drv_id * vcd->offsets[DRV_DISTANCE];
	offset += vcd->offsets[reg];
	offset += ch * vcd->offsets[PWR_ST_CHN_DISTANCE];
	offset += resource_idx * vcd->offsets[DRV_RESOURCE_DISTANCE];

	return offset;
}

static void write_crm_reg(const struct crm_drv *drv, u32 reg, u32 ch, u32 vcd_type,
			  u32 resource_idx, u32 data)
{
	u32 offset = crm_get_offset(drv, reg, ch, vcd_type, resource_idx);

	writel_relaxed(data, drv->base + offset);
}

static u32 read_crm_reg(const struct crm_drv *drv, u32 reg, u32 ch, u32 vcd_type,
			u32 resource_idx)
{
	u32 offset = crm_get_offset(drv, reg, ch, vcd_type, resource_idx);

	return readl_relaxed(drv->base + offset);
}

static inline u32 crmc_mgr_get_offset(const struct crm_mgr *mgr,
					u32 reg, u32 resource_idx)
{
	u32 offset;
	u32 resource_distance = 0;

	switch (reg) {
	case AGGR_PERF_OL:
		resource_distance = AGGR_PERF_OL_RESOURCE_DISTANCE;
		break;
	case CURR_PERF_OL:
		resource_distance = CURR_PERF_OL_RESOURCE_DISTANCE;
		break;
	case SEQ_STATUS:
		resource_distance = SEQ_STATUS_RESOURCE_DISTANCE;
		break;
	default:
		resource_distance = 0;
	}

	offset = mgr->offsets[CRM_BASE] + resource_idx * mgr->offsets[CRM_DISTANCE];
	offset += resource_idx * mgr->offsets[resource_distance];
	offset += mgr->offsets[reg];

	return offset;
}

static inline u32 crmb_crmv_mgr_get_offset(const struct crm_mgr *mgr,
					u32 reg, u32 resource_idx)
{
	u32 offset;

	offset = mgr->offsets[CRM_BASE] + resource_idx * mgr->offsets[CRM_DISTANCE];
	offset += resource_idx * mgr->offsets[0];
	offset += mgr->offsets[reg];

	return offset;
}

static u32 read_crmb_mgr_reg(const struct crm_mgr *crmb_mgr,
				u32 *offset, u32 reg, u32 resource_idx)
{
	*offset = crmb_crmv_mgr_get_offset(crmb_mgr, reg, resource_idx);

	return readl_relaxed(crmb_mgr->base + *offset);
}

static u32 read_crmc_mgr_reg(const struct crm_mgr *crmc_mgr,
				u32 *offset, u32 reg, u32 resource_idx)
{
	*offset = crmc_mgr_get_offset(crmc_mgr, reg, resource_idx);

	return readl_relaxed(crmc_mgr->base + *offset);
}

static u32 read_crmv_mgr_reg(const struct crm_mgr *crmv_mgr,
				u32 *offset, u32 reg, u32 resource_idx)
{
	*offset = crmb_crmv_mgr_get_offset(crmv_mgr, reg, resource_idx);

	return readl_relaxed(crmv_mgr->base + *offset);
}

static struct crm_drv *get_crm_drv(const struct device *dev, enum crm_drv_type drv_type,
				   u32 drv_id)
{
	struct crm_drv_top *crm;
	int i, num_drvs;
	struct crm_drv *drvs;

	if (!dev)
		return NULL;

	crm = dev_get_drvdata(dev);

	if (drv_type == CRM_HW_DRV) {
		num_drvs = crm->num_hw_drvs;
		drvs = crm->hw_drvs;
	} else {
		num_drvs = crm->num_sw_drvs;
		drvs = crm->sw_drvs;
	}

	for (i = 0; i < num_drvs; i++) {
		if (drv_id == drvs[i].drv_id)
			return &drvs[i];
	}

	return NULL;
}

/**
 * crm_get_channel() - Get the channel to Update the data
 * @drv:       The CRM DRV controller.
 * @chn_type:  The type of channel to find.
 *
 * Return:
 * * 0			- Success
 * * -Error             - Error code
 */
static int crm_get_channel(struct crm_drv *drv, enum channel_type ch_type, u32 *ch)
{
	u32 chn_update;

	if (drv->num_channels == 0)
		return -EBUSY;

	/* Select Unused channel */
	chn_update = read_crm_channel(drv, CHN_UPDATE);
	if (!chn_update) {
		/* Start with ch0 if none are in use */
		*ch = CH0;
		return 0;
	}

	if (chn_update & CH0_CHN_BUSY)
		*ch = ch_type == CHN_FREE ? CH1 : CH0;
	else if (chn_update & CH1_CHN_BUSY)
		*ch = ch_type == CHN_FREE ? CH0 : CH1;
	else
		return -EBUSY;

	return 0;
}

int crm_channel_switch_complete(const struct crm_drv *drv, u32 ch)
{
	u32 sts;
	int retry = 100, ret = 0;

	do {
		sts = read_crm_channel(drv, CHN_BUSY);
		if (ch == 0)
			sts &= CH0_CHN_BUSY;
		else
			sts &= CH1_CHN_BUSY;

		retry--;
		/*
		 * Wait till all the votes are applied to new
		 * channel during channel switch.
		 * Maximum delay of 5 msec.
		 */
		udelay(100);
	} while ((sts != BIT(ch)) && retry);

	if (!retry)
		ret = -EBUSY;

	trace_crm_switch_channel(drv->name, ch, ret);
#if IS_ENABLED(CONFIG_IPC_LOGGING)
	ipc_log_string(drv->ipc_log_ctx, "Switch Channel: ch: %u ret: %d", ch, ret);
#endif

	return ret;
}

/**
 * crm_switch_channel() - Switch to the channel
 * @drv:     The controller DRV.
 * @ch:      The channel number to switch to.
 *
 * NOTE: Caller should ensure serialization before making this call.
 * Return:
 * * 0			- Success
 * * -Error             - Error code
 */
int crm_switch_channel(const struct crm_drv *drv, u32 ch)
{
	write_crm_channel(drv, CHN_UPDATE, BIT(ch));

	if (!drv->set_hw_chn_switch_ctrl)
		return crm_channel_switch_complete(drv, ch);

	return 0;
}

static u32 crm_get_pwr_state_reg(int pwr_state)
{
	u32 reg;

	switch (pwr_state) {
	case 0:
		reg = PWR_ST0;
		break;
	case 1:
		reg = PWR_ST1;
		break;
	case 2:
		reg = PWR_ST2;
		break;
	case 3:
		reg = PWR_ST3;
		break;
	case 4:
		reg = PWR_ST4;
		break;
	default:
		WARN_ON(1);
		reg = PWR_ST0;
	}

	return reg;
}

static int _crm_dump_drv_regs(struct crm_drv *drv, struct crm_drv_top *crm)
{
	struct crm_vcd *vcd;
	u32 chn = 0, reg;
	u32 phy_base, data, offset;
	int m, j, k;
	int ret = 0;
	bool gic_irq_sts;
	struct irq_data *gic_irq_data;

	phy_base = get_crm_phy_addr(drv->base);
	pr_warn("%s DRV%d Regs\n", drv->drv_type ? "SW" : "HW", drv->drv_id);

	spin_lock(&drv->cache_lock);
	if (drv->drv_type == CRM_SW_DRV)
		goto skip_channel;

	ret = crm_get_channel(drv, CHN_IN_USE, &chn);
	if (ret) {
		spin_unlock(&drv->cache_lock);
		return ret;
	}

	offset = crm_get_channel_offset(drv, CHN_BUSY);
	data = readl_relaxed(drv->base + offset);
	crm_print_reg(phy_base + offset, data);

	for (m = 0; m < MAX_VCD_TYPE; m++) {
		if (!(crm->desc->crm_capability & BIT(m)))
			continue;

		vcd = &drv->vcd[m];
		for (k = 0; k < vcd->num_resources; k++) {
			for (j = 0; j < vcd->num_pwr_states; j++) {
				reg = crm_get_pwr_state_reg(j);

				offset = crm_get_offset(drv, reg, chn, m, k);
				data = readl_relaxed(drv->base + offset);
				crm_print_hw_reg(drv->drv_id, chn, m, k,
						reg-PWR_ST0, phy_base + offset, data);
			}

			pr_warn("DRV%d %s:%d HW Status\n", drv->drv_id,
				BIT(m) == PERF_OL_VOTING_FLAG ? "PERF_OL_VCD" :
				BIT(m) == BW_VOTING_FLAG ? "BW_VOTE_ND" : "BW_PT_VOTE_ND", k);

			offset = crm_get_offset(drv, STATUS, 0, m, k);
			data = readl_relaxed(drv->base + offset);
			crm_print_reg(phy_base + offset, data);
		}

		if (BIT(m) == PERF_OL_VOTING_FLAG) {
			offset = crm_get_offset(drv, PWR_IDX_STATUS, 0, m, 0);
			data = readl_relaxed(drv->base + offset);
			crm_print_reg(phy_base + offset, data);
		}
	}

	spin_unlock(&drv->cache_lock);

	return ret;

skip_channel:
	gic_irq_data = irq_get_irq_data(drv->irq);
	irq_get_irqchip_state(drv->irq, IRQCHIP_STATE_PENDING, &gic_irq_sts);
	pr_warn("HW IRQ %lu is %s at GIC\n", gic_irq_data->hwirq,
		 gic_irq_sts ? "PENDING" : "NOT PENDING");

	for (m = 0; m < MAX_VCD_TYPE; m++) {
		if (!(crm->desc->crm_capability & BIT(m)))
			continue;

		vcd = &drv->vcd[m];
		for (k = 0; k < vcd->num_resources; k++) {
			for (j = 0; j < vcd->num_pwr_states; j++) {
				reg = crm_get_pwr_state_reg(j);
				offset = crm_get_offset(drv, reg, chn, m, k);
				data = readl_relaxed(drv->base + offset);
				crm_print_sw_reg(drv->drv_id, m, k,
						reg - PWR_ST0, phy_base + offset, data);
			}

			pr_warn("DRV%d %s:%d SW Status\n", drv->drv_id,
				BIT(m) == PERF_OL_VOTING_FLAG ? "PERF_OL_VCD" :
				BIT(m) == BW_VOTING_FLAG ? "BW_VOTE_ND" : "BW_PT_VOTE_ND", k);

			offset = crm_get_offset(drv, STATUS, chn, m, k);
			data = readl_relaxed(drv->base + offset);
			crm_print_reg(phy_base + offset, data);

			if (BIT(m) == PERF_OL_VOTING_FLAG) {
				pr_warn("DRV%d SW PERF_OL_VCD IRQ STATUS\n", drv->drv_id);
				offset = crm_get_offset(drv, IRQ_STATUS, chn, m, k);
				data = readl_relaxed(drv->base + offset);
				crm_print_reg(phy_base + offset, data);

				pr_warn("DRV%d SW PERF_OL_VCD FSM Status\n", drv->drv_id);
				offset = crm_get_offset(drv, FSM_STATUS, chn, m, k);
				data = readl_relaxed(drv->base + offset);
				crm_print_reg(phy_base + offset, data);
			}
		}

		if (BIT(m) == BW_VOTING_FLAG) {
			pr_warn("DRV%d SW BW_VOTE_ND IRQ STATUS\n", drv->drv_id);
			offset = crm_get_offset(drv, IRQ_STATUS, chn, m, 0);
			data = readl_relaxed(drv->base + offset);
			crm_print_reg(phy_base + offset, data);

			pr_warn("DRV%d SW BW_VOTE_ND FSM Status\n", drv->drv_id);
			offset = crm_get_offset(drv, FSM_STATUS, chn, m, 0);
			data = readl_relaxed(drv->base + offset);
			crm_print_reg(phy_base + offset, data);
		}

		if (BIT(m) == BW_PT_VOTING_FLAG) {
			pr_warn("DRV%d SW BW_PT_VOTE_ND IRQ STATUS\n", drv->drv_id);
			offset = crm_get_offset(drv, IRQ_STATUS, chn, m, 0);
			data = readl_relaxed(drv->base + offset);
			crm_print_reg(phy_base + offset, data);

			pr_warn("DRV%d SW BW_PT_VOTE_ND CRMB PT TRIGGER Status\n",
						drv->drv_id);
			offset = crm_get_offset(drv, CRMB_PT_TRIGGER, chn, m, 0);
			data = readl_relaxed(drv->base + offset);
			crm_print_reg(phy_base + offset, data);
		}
	}

	spin_unlock(&drv->cache_lock);

	return ret;
}

static int _crm_dump_regs(struct crm_drv_top *crm)
{
	u32 phy_base, data, offset;
	int i, ret = 0;

	if (!(crm->desc->crm_capability & BW_VOTING_FLAG))
		goto dump_crmb_pt;

	pr_warn("CRMB Regs\n");
	phy_base = get_crm_phy_addr(crm->crmb_mgr.base) +
					((unsigned long) crm->crmb_mgr.base & VPAGE_SHIFT_BITS);

	pr_warn("CRMB: num_of_resources:%d\n", crm->crmb_mgr.num_resources);
	for (i = 0; i < crm->crmb_mgr.num_resources; i++) {
		data = read_crmb_mgr_reg(&crm->crmb_mgr, &offset, STATUS_BE, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmb_mgr_reg(&crm->crmb_mgr, &offset, STATUS_FE, i);
		crm_print_reg(phy_base + offset, data);
	}

dump_crmb_pt:
	if (!(crm->desc->crm_capability & BW_PT_VOTING_FLAG))
		goto dump_crmc;

	pr_warn("CRMB_PT Regs\n");
	phy_base = get_crm_phy_addr(crm->crmb_pt_mgr.base) +
					((unsigned long) crm->crmb_pt_mgr.base & VPAGE_SHIFT_BITS);

	pr_warn("CRMB_PT: num_of_resources:%d\n", crm->crmb_pt_mgr.num_resources);
	for (i = 0; i < crm->crmb_pt_mgr.num_resources; i++) {
		data = read_crmb_mgr_reg(&crm->crmb_pt_mgr, &offset, TCS_CMD_DATA, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmb_mgr_reg(&crm->crmb_pt_mgr, &offset, TCS_CMD_ADDR, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmb_mgr_reg(&crm->crmb_pt_mgr, &offset, TCS_CMD_CTRL, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmb_mgr_reg(&crm->crmb_pt_mgr, &offset, TCS_CMD_STATUS, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmb_mgr_reg(&crm->crmb_pt_mgr, &offset, TCS_CMD_ENABLE, i);
		crm_print_reg(phy_base + offset, data);
	}

	data = read_crmb_mgr_reg(&crm->crmb_pt_mgr, &offset, CRMB_PT_FSM_STATUS, 0);
	crm_print_reg(phy_base + offset, data);

dump_crmc:
	pr_warn("CRMC Regs\n");
	phy_base = get_crm_phy_addr(crm->crmc_mgr.base) +
					((unsigned long) crm->crmc_mgr.base & VPAGE_SHIFT_BITS);
	pr_warn("CRMC: num_of_resources:%d\n", crm->crmc_mgr.num_resources);
	for (i = 0; i < crm->crmc_mgr.num_resources; i++) {
		data = read_crmc_mgr_reg(&crm->crmc_mgr, &offset, AGGR_PERF_OL, i);
		crm_print_reg(phy_base + offset, data);
	}

	pr_warn("crmc_base: 0x%x\n", phy_base);
	for (i = 0; i < (crm->crmc_mgr.num_resources + crm->crmb_mgr.num_resources); i++) {
		data = read_crmc_mgr_reg(&crm->crmc_mgr, &offset, CURR_PERF_OL, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmc_mgr_reg(&crm->crmc_mgr, &offset, SEQ_STATUS, i);
		crm_print_reg(phy_base + offset, data);
	}

	pr_warn("CRMV Regs\n");
	phy_base = get_crm_phy_addr(crm->crmv_mgr.base) +
					((unsigned long) crm->crmv_mgr.base & VPAGE_SHIFT_BITS);

	pr_warn("CRMV: num_of_resources:%d\n", crm->crmv_mgr.num_resources);
	for (i = 0; i < crm->crmv_mgr.num_resources; i++) {
		data = read_crmv_mgr_reg(&crm->crmv_mgr, &offset, AGGR_VOL_STS, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmv_mgr_reg(&crm->crmv_mgr, &offset, SEQ_VOL_STS, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmv_mgr_reg(&crm->crmv_mgr, &offset, CURR_VOL_STS, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmv_mgr_reg(&crm->crmv_mgr, &offset, RAIL_FSM_STS, i);
		crm_print_reg(phy_base + offset, data);

		data = read_crmv_mgr_reg(&crm->crmv_mgr, &offset, RAIL_TCS_STS, i);
		crm_print_reg(phy_base + offset, data);
	}

	return ret;
}

static void crm_flush_cache(struct crm_drv *drv, struct crm_vcd *vcd, u32 ch, u32 vcd_type)
{
	int i, j;
	u32 reg;

	for (i = 0; i < vcd->num_resources; i++) {
		for (j = 0; j < vcd->num_pwr_states; j++) {
			reg = crm_get_pwr_state_reg(j);
			write_crm_reg(drv, reg, ch, vcd_type, i, vcd->cache[i][j]);
			trace_crm_write_vcd_votes(drv->name, vcd_type, i, j, vcd->cache[i][j]);
#if IS_ENABLED(CONFIG_IPC_LOGGING)
			ipc_log_string(drv->ipc_log_ctx,
				       "Flush: type: %u resource_idx:%u pwr_state: %u data: %#x",
				       vcd_type, i, j, vcd->cache[i][j]);
#endif
		}
	}
}
/**
 * crm_write_pwr_states() - Flush the power state votes for HW DRVs.
 * @dev:      The CRM device
 * @drv_id:   HW DRV ID for which to flush the power state votes.
 *
 * Find the non-active channel, writes various power states that
 * were cached with crm_write_perf_ol() and crm_write_bw_vote()
 * APIs and does a channel switch.
 *
 * Applicable only for HW DRVs for which the votes are cached.
 * SW DRVs votes are immediately written.
 *
 * Return:
 * * 0			- Success
 * * -Error             - Error code
 */
int crm_write_pwr_states(const struct device *dev, u32 drv_id)
{
	struct crm_drv_top *crm = dev_get_drvdata(dev);
	struct crm_drv *drv = get_crm_drv(dev, CRM_HW_DRV, drv_id);
	struct crm_vcd *vcd;
	u32 ch;
	int i;
	int ret;

	if (!drv || drv->drv_type == CRM_SW_DRV)
		return -EINVAL;

	spin_lock(&drv->cache_lock);

	ret = crm_get_channel(drv, CHN_FREE, &ch);
	if (ret)
		goto exit;

	for (i = 0; i < MAX_VCD_TYPE; i++) {
		if (!(crm->desc->crm_capability & BIT(i)))
			continue;

		vcd = &drv->vcd[i];
		crm_flush_cache(drv, vcd, ch, i);
	}

	ret = crm_switch_channel(drv, ch);
	if (ret)
		goto exit;

exit:
	spin_unlock(&drv->cache_lock);

	/* Dump CRM registers for debug */
	if (ret) {
		_crm_dump_drv_regs(drv, crm);
		_crm_dump_regs(crm);
		BUG_ON(1);
	}

	return ret;
}
EXPORT_SYMBOL_GPL(crm_write_pwr_states);

/**
 * crm_dump_drv_regs() - Dump CRM DRV registers for debug purposes.
 * @name:      The name of the crm device to dump for.
 * @drv_id:    DRV ID for which to dump for.
 *
 * Return:
 * * 0        - Success
 * * -Error   - Error code
 */
int crm_dump_drv_regs(const char *name, u32 drv_id)
{
	struct crm_drv_top *crm;
	struct crm_drv *drv;
	const struct device *dev;

	dev = crm_get_device(name);
	if (IS_ERR(dev))
		return -EINVAL;

	crm = dev_get_drvdata(dev);
	drv = get_crm_drv(dev, CRM_HW_DRV, drv_id);
	if (!drv)
		return -EINVAL;

	return _crm_dump_drv_regs(drv, crm);
}
EXPORT_SYMBOL_GPL(crm_dump_drv_regs);

/**
 * crm_dump_regs() - Dump CRM registers for debug purposes.
 * @name:      The name of the crm device to dump for.
 *
 * Return:
 * * 0        - Success
 * * -Error   - Error code
 */
int crm_dump_regs(const char *name)
{
	struct crm_drv_top *crm;
	const struct device *dev;

	dev = crm_get_device(name);
	if (IS_ERR(dev))
		return -EINVAL;

	crm = dev_get_drvdata(dev);

	return _crm_dump_regs(crm);
}
EXPORT_SYMBOL_GPL(crm_dump_regs);

/**
 * crm_read_curr_perf_ol() - Read current performance level.
 * @name:      The name of the crm device to dump for.
 * @vcd_idx:   The VCD index to read from.
 * @data:      Read CURR_PERF_OL register value into this.
 *
 * Return:
 * * 0        - Success
 * * -Error   - Error code
 */
int crm_read_curr_perf_ol(const char *name, int vcd_idx, u32 *data)
{
	struct crm_drv_top *crm;
	const struct device *dev;
	u32 offset;

	dev = crm_get_device(name);
	if (IS_ERR(dev))
		return -EINVAL;

	crm = dev_get_drvdata(dev);
	if (vcd_idx >= crm->crmc_mgr.num_resources)
		return -EINVAL;

	*data = read_crmc_mgr_reg(&crm->crmc_mgr, &offset,
			CURR_PERF_OL, vcd_idx) & CURR_PER_OL_MASK;

	return 0;
}
EXPORT_SYMBOL_GPL(crm_read_curr_perf_ol);

static void crm_vote_completion(struct crm_sw_votes *votes)
{
	struct completion *compl = &votes->compl;

	votes->in_progress = false;
	complete(compl);
}

/**
 * crm_vote_complete_irq() - Vote completion interrupt handler for SW DRVs.
 * @irq: The IRQ number.
 * @p:   Pointer to "struct crm_drv".
 *
 * Called for ACTIVE_VOTE transfers (those are the only ones we enable the
 * IRQ for) when a transfer is done.
 *
 * Return: IRQ_HANDLED
 */
static irqreturn_t crm_vote_complete_irq(int irq, void *p)
{
	struct crm_drv_top *crm = p;
	struct crm_drv *drv;
	struct crm_vcd *vcd;
	struct crm_sw_votes *votes;
	unsigned long irq_status;
	int i, j, k, num_irq;

	for (i = 0; i < crm->num_sw_drvs; i++) {
		drv = &crm->sw_drvs[i];
		if (!drv->initialized && drv->irq != irq)
			continue;

		spin_lock(&drv->lock);
		for (j = 0; j < MAX_VCD_TYPE; j++) {
			if (!(crm->desc->crm_capability & BIT(j)))
				continue;
			vcd = &drv->vcd[j];

			num_irq = j == BW_PT_VOTE_VCD ? 1 : vcd->num_resources;

			for (k = 0; k < num_irq; k++) {

				irq_status = read_crm_reg(drv, IRQ_STATUS, 0, j, k);
				if (!irq_status)
					continue;

				write_crm_reg(drv, IRQ_CLEAR, 0, j, k, IRQ_CLEAR_BIT);
				trace_crm_irq(drv->name, j, k, irq_status);
#if IS_ENABLED(CONFIG_IPC_LOGGING)
				ipc_log_string(drv->ipc_log_ctx,
					       "IRQ: type: %u resource_idx:%u irq_status: %lu"
						, j, k, irq_status);
#endif

				votes = &vcd->sw_votes[k];
				if (!votes->in_progress) {
					WARN_ON(1);
					continue;
				}

				if (votes->cmd.wait)
					crm_vote_completion(votes);
			}
		}
		spin_unlock(&drv->lock);
	}

	return IRQ_HANDLED;
}

static void crm_fill_cmd(struct crm_cmd *dest, const struct crm_cmd *src)
{
	dest->resource_idx = src->resource_idx;
	dest->pwr_state = src->pwr_state;
	dest->data = src->data;
	dest->wait = src->wait;
}

static u32 crm_get_pwr_state(struct crm_drv *drv, const struct crm_cmd *cmd)
{
	enum crm_sw_drv_state sw;
	enum crm_hw_drv_state hw;
	u32 pwr_state;

	if (drv->drv_type == CRM_HW_DRV) {
		hw = cmd->pwr_state.hw;
		pwr_state = hw;
	} else {
		sw = cmd->pwr_state.sw;
		pwr_state = sw;
	}

	return pwr_state;
}

static int crm_send_cmd(struct crm_drv_top *crm, struct crm_drv *drv,
			u32 vcd_type, const struct crm_cmd *cmd, bool pt_trigger)
{
	struct crm_vcd *vcd = &drv->vcd[vcd_type];
	u32 resource_idx = cmd->resource_idx;
	u32 pwr_state = crm_get_pwr_state(drv, cmd);
	u32 data = cmd->data;
	bool wait = cmd->wait;
	unsigned long flags;
	struct completion *compl = NULL;
	u32 time_left;
	u32 irq_idx;

	spin_lock_irqsave(&drv->lock, flags);

	/* Note: Set BIT(31) for RESP_REQ */
	if ((vcd_type == BW_VOTE_VCD) && wait)
		data |= BW_VOTE_RESP_REQ;

	irq_idx = vcd_type == BW_PT_VOTE_VCD ? 0 : resource_idx;

	switch (pwr_state) {
	case CRM_ACTIVE_STATE:
		/* Wait forever for a previous request to complete */
		wait_event_lock_irq(vcd->sw_votes[irq_idx].wait,
			    !vcd->sw_votes[irq_idx].in_progress,
			    drv->lock);

		compl = &vcd->sw_votes[irq_idx].compl;
		init_completion(compl);
		crm_fill_cmd(&vcd->sw_votes[irq_idx].cmd, cmd);
		vcd->sw_votes[irq_idx].in_progress = true;
		write_crm_reg(drv, PWR_ST0, 0, vcd_type, resource_idx, data);
		break;
	case CRM_SLEEP_STATE:
		write_crm_reg(drv, PWR_ST1, 0, vcd_type, resource_idx, data);
		break;
	case CRM_WAKE_STATE:
		write_crm_reg(drv, PWR_ST2, 0, vcd_type, resource_idx, data);
		break;
	default:
		WARN_ON(1);
		break;
	}

	/* Set COMMIT to start aggregating votes */
	if (pt_trigger) {
		write_crm_reg(drv, CRMB_PT_TRIGGER, 0, vcd_type, 0, BW_PT_VOTE_TRIGGER);
		udelay(1);
		write_crm_reg(drv, CRMB_PT_TRIGGER, 0, vcd_type, 0, 0);
	}

	spin_unlock_irqrestore(&drv->lock, flags);
	trace_crm_write_vcd_votes(drv->name, vcd_type, resource_idx, pwr_state, data);
#if IS_ENABLED(CONFIG_IPC_LOGGING)
	ipc_log_string(drv->ipc_log_ctx,
		       "Write: type: %u resource_idx:%u pwr_state: %u data: %#x",
		       vcd_type, resource_idx, pwr_state, data);
#endif

	if (compl && wait) {
		time_left = wait_for_completion_timeout(compl, CRM_TIMEOUT_MS);
		if (!time_left) {
			_crm_dump_drv_regs(drv, crm);
			_crm_dump_regs(crm);
			//BUG_ON(1);
			return -ETIMEDOUT;
		}
		/* Unblock new requests for same VCD */
		wake_up(&vcd->sw_votes[irq_idx].wait);
	}

	return 0;
}

static void crm_cache_vcd_votes(struct crm_drv *drv, u32 vcd_type, const struct crm_cmd *cmd)
{
	struct crm_vcd *vcd = &drv->vcd[vcd_type];
	u32 resource_idx = cmd->resource_idx;
	u32 pwr_state = crm_get_pwr_state(drv, cmd);
	u32 data = cmd->data;

	spin_lock(&drv->cache_lock);

	vcd->cache[resource_idx][pwr_state] = data;
	vcd->cache_dirty = true;

	spin_unlock(&drv->cache_lock);

	trace_crm_cache_vcd_votes(drv->name, vcd_type, resource_idx, pwr_state, data);
#if IS_ENABLED(CONFIG_IPC_LOGGING)
	ipc_log_string(drv->ipc_log_ctx,
		       "Cache: type: %u resource_idx:%u pwr_state: %u data: %#x",
		       vcd_type, resource_idx, pwr_state, data);
#endif

}

static bool crm_is_invalid_cmd(struct crm_drv *drv, u32 vcd_type, const struct crm_cmd *cmd)
{
	struct crm_vcd *vcd;
	u32 resource_idx;
	u32 pwr_state;
	u32 data;
	bool ret;

	if (!drv || !cmd)
		return true;

	vcd = &drv->vcd[vcd_type];
	resource_idx = cmd->resource_idx;
	pwr_state = crm_get_pwr_state(drv, cmd);
	data = cmd->data;

	if (pwr_state >= vcd->num_pwr_states)
		ret = true;
	else if (resource_idx >= vcd->num_resources)
		ret = true;
	else if (vcd_type == BW_VOTE_VCD && !(data & BW_VOTE_VALID))
		ret = true;
	else if (vcd_type == BW_PT_VOTE_VCD && !(data & BW_PT_VOTE_VALID))
		ret = true;
	else if (vcd_type == PERF_OL_VCD && (data & ~PERF_OL_VALUE_BITS))
		ret = true;
	else
		ret = false;

	return ret;
}

/**
 * crm_write_perf_ol() - Write a perf ol vote for a resource
 * @dev:       The CRM device
 * @drv_type:  The CRM DRV type, either SW or HW DRV.
 * @drv_id:    DRV ID for which the votes are sent
 * @cmd:       The CRM CMD
 *
 * Caches the votes for HW DRV and immediately returns.
 * The votes are written to unused channel with a call to
 * crm_write_pwr_states().
 *
 * Caches the votes for logging and immediately sents the votes for SW DRVs
 * if the @cmd have .wait set and is for ACTIVE_VOTE then waits for completion
 * IRQ before return. for SLEEP_VOTE and WAKE_VOTE no completion IRQ is sent
 * and they are triggered within HW during idle/awake scenarios.
 *
 * Return:
 * * 0			- Success
 * * -Error             - Error code
 */
int crm_write_perf_ol(const struct device *dev, enum crm_drv_type drv_type,
		      u32 drv_id, const struct crm_cmd *cmd)
{
	struct crm_drv_top *crm = dev_get_drvdata(dev);
	struct crm_drv *drv = get_crm_drv(dev, drv_type, drv_id);
	int ret;

	ret = crm_is_invalid_cmd(drv, PERF_OL_VCD, cmd);
	if (ret)
		return -EINVAL;

	/* Cache the votes first */
	crm_cache_vcd_votes(drv, PERF_OL_VCD, cmd);

	/* Send SW DRV votes immediately for ACTIVE/SLEEP/WAKE states */
	if (drv_type == CRM_SW_DRV)
		return crm_send_cmd(crm, drv, PERF_OL_VCD, cmd, false);

	return 0;
}
EXPORT_SYMBOL_GPL(crm_write_perf_ol);

/**
 * crm_write_bw_vote() - Write a bw vote for a resource
 * @dev:       The CRM device
 * @drv_type:  The CRM DRV type, either SW or HW DRV.
 * @drv_id:    DRV ID for which the votes are sent
 * @cmd:       The CRM CMD
 *
 * Caches the votes for HW DRV and immediately returns.
 * The votes are written to unused channel with a call to
 * crm_write_pwr_states().
 *
 * Caches the votes for logging and immediately sents the votes for SW DRVs
 * if the @cmd have .wait set and is for ACTIVE_VOTE then waits for completion
 * IRQ before return. for SLEEP_VOTE and WAKE_VOTE no completion IRQ is sent
 * and they are triggered within HW during idle/awake scenarios.
 *
 * Return:
 * * 0			- Success
 * * -Error             - Error code
 */
int crm_write_bw_vote(const struct device *dev, enum crm_drv_type drv_type,
		      u32 drv_id, const struct crm_cmd *cmd)
{
	struct crm_drv_top *crm = dev_get_drvdata(dev);
	struct crm_drv *drv = get_crm_drv(dev, drv_type, drv_id);
	int ret;

	if (!(crm->desc->crm_capability & BW_VOTING_FLAG))
		return -EPERM;

	ret = crm_is_invalid_cmd(drv, BW_VOTE_VCD, cmd);
	if (ret)
		return -EINVAL;

	/* Cache the votes first */
	crm_cache_vcd_votes(drv, BW_VOTE_VCD, cmd);

	/* Send SW DRV votes immediately for ACTIVE/SLEEP/WAKE states */
	if (drv_type == CRM_SW_DRV)
		return crm_send_cmd(crm, drv, BW_VOTE_VCD, cmd, false);

	return 0;
}
EXPORT_SYMBOL_GPL(crm_write_bw_vote);

/**
 * crm_write_bw_pt_vote() - Write a bw vote for a resource
 * @dev:       The CRM device
 * @drv_type:  The CRM DRV type, either SW or HW DRV.
 * @drv_id:    DRV ID for which the votes are sent
 * @cmd:       The CRM CMD
 *
 * Caches the votes for HW DRV and immediately returns.
 * The votes are written to unused channel with a call to
 * crm_write_pwr_states().
 *
 * Caches the votes for logging and immediately sents the votes for SW DRVs
 * if the @cmd have .wait set and is for ACTIVE_VOTE then waits for completion
 * IRQ before return. For SLEEP_VOTE and WAKE_VOTE no completion IRQ is sent
 * and they are triggered within HW during idle/awake scenarios.
 *
 * Return:
 * * 0			- Success
 * * -Error             - Error code
 */
int crm_write_bw_pt_vote(const struct device *dev, enum crm_drv_type drv_type,
		      u32 drv_id, const struct crm_cmd *cmd)
{
	struct crm_drv_top *crm = dev_get_drvdata(dev);
	struct crm_drv *drv = get_crm_drv(dev, drv_type, drv_id);
	int ret;

	if (!(crm->desc->crm_capability & BW_PT_VOTING_FLAG))
		return -EPERM;

	ret = crm_is_invalid_cmd(drv, BW_PT_VOTE_VCD, cmd);
	if (ret)
		return -EINVAL;

	/* Cache the votes first */
	crm_cache_vcd_votes(drv, BW_PT_VOTE_VCD, cmd);

	/* Send SW DRV votes immediately for ACTIVE/SLEEP/WAKE states */
	if (drv_type == CRM_SW_DRV)
		return crm_send_cmd(crm, drv, BW_PT_VOTE_VCD, cmd, true);

	return 0;
}
EXPORT_SYMBOL_GPL(crm_write_bw_pt_vote);

/**
 * crm_get_device() - Returns a CRM device handle.
 * @name: The CRM device name for which handle is needed.
 *
 * Finds the CRM device from list of available CRM devices.
 * The @name should match the label property in device which are "cam_crm"
 * or "pcie_crm".
 *
 * Return:
 * * Device pointer	- Success
 * * -Error pointer     - Error
 */
const struct device *crm_get_device(const char *name)
{
	struct crm_drv_top *crm;

	list_for_each_entry(crm, &crm_dev_list, list) {
		if (!strcmp(name, crm->name))
			return crm->dev;
	}

	return ERR_PTR(-ENODEV);
}
EXPORT_SYMBOL_GPL(crm_get_device);

static void crm_set_chn_behave(struct crm_drv_top *crm)
{
	int i;

	if (!crm->desc->set_chn_behave)
		return;

	for (i = 0; i < crm->num_hw_drvs; i++)
		write_crm_channel(&crm->hw_drvs[i], CHN_BEHAVE, CHN_BEHAVE_BIT);
}

static void crm_set_hw_chn_switch_ctrl(struct crm_drv_top *crm)
{
	int i;

	if (!crm->desc->set_hw_chn_switch_ctrl)
		return;

	for (i = 0; i < crm->num_hw_drvs; i++)
		write_crm_channel(&crm->hw_drvs[i], CHN_BEHAVE, CHN_SWITCH_CTRL);
}

static int crm_probe_get_irqs(struct crm_drv_top *crm)
{
	struct crm_drv *drvs = crm->sw_drvs;
	struct crm_vcd *vcd;
	int i, j, k;
	int irq;
	int ret;

	if (!crm->num_sw_drvs)
		return 0;

	/* Only SW DRVs have associated vote completion IRQ */
	for (i = 0; i < crm->num_sw_drvs; i++) {
		if (!crm->sw_drvs[i].initialized)
			continue;

		irq = platform_get_irq(crm->pdev, drvs[i].drv_id);
		if (irq < 0)
			return irq;

		ret = devm_request_irq(crm->dev, irq, crm_vote_complete_irq,
					   IRQF_TRIGGER_RISING, drvs[i].name, crm);
		if (ret)
			return ret;

		drvs[i].irq = irq;
		/* SW DRV do not have any channels */
		drvs[i].num_channels = 0;

		/* Additionally allocate memory for sw_votes */
		for (j = 0; j < MAX_VCD_TYPE; j++) {
			if (!(crm->desc->crm_capability & BIT(j)))
				continue;

			vcd = &drvs[i].vcd[j];
			vcd->sw_votes = devm_kcalloc(crm->dev, vcd->num_resources,
						     sizeof(struct crm_sw_votes),
						     GFP_KERNEL);
			if (!vcd->sw_votes)
				return -ENOMEM;

			/* Enable IRQs for all VCDs */
			for (k = 0; k < vcd->num_resources; k++) {
				init_waitqueue_head(&vcd->sw_votes[k].wait);
				write_crm_reg(&drvs[i], IRQ_ENABLE, 0, j, k, IRQ_ENABLE_BIT);
			}
		}
	}

	return 0;
}

static int crm_probe_alloc_vcd_caches(struct crm_drv_top *crm, struct crm_vcd *vcd)
{
	u32 num_resources = vcd->num_resources;
	u32 num_pwr_states = vcd->num_pwr_states;
	int i;

	vcd->cache = devm_kcalloc(crm->dev, num_resources, sizeof(u32 *), GFP_KERNEL);
	if (!vcd->cache)
		return -ENOMEM;

	for (i = 0; i < num_resources; i++) {
		vcd->cache[i] = devm_kcalloc(crm->dev, num_pwr_states, sizeof(u32), GFP_KERNEL);
		if (!vcd->cache[i])
			return -ENOMEM;
	}

	return 0;
}

static int crm_probe_set_vcd_caches(struct crm_drv_top *crm, u32 crm_cfg, u32 crm_cfg_2)
{
	struct crm_vcd *vcd;
	struct crm_drv *drv;
	u32 num_perf_ol_vcds, num_nds, num_pwr_states;
	u32 num_bw_vote_vcds, num_nds_pt, num_rails;
	int i, j, ret;

	num_perf_ol_vcds = field_get(crm->desc->cfg_regs[NUM_VCD_VOTED_BY_PERF_OL], crm_cfg);
	num_bw_vote_vcds = field_get(crm->desc->cfg_regs[NUM_VCD_VOTED_BY_BW], crm_cfg);
	num_pwr_states = field_get(crm->desc->cfg_regs[NUM_PWR_STATES_PER_CH], crm_cfg);
	num_nds_pt = field_get(crm->desc->cfg_regs[NUM_OF_NODES_PT], crm_cfg);
	num_rails = field_get(crm->desc->cfg_regs[NUM_OF_RAILS], crm_cfg);
	num_nds = field_get(crm->desc->cfg_regs[NUM_OF_NODES], crm_cfg_2);

	for (i = 0; i < crm->num_hw_drvs; i++) {
		drv = &crm->hw_drvs[i];

		if (!drv->initialized)
			continue;

		drv->drv_type = CRM_HW_DRV;
		for (j = 0; j < MAX_VCD_TYPE; j++) {
			if (!(crm->desc->crm_capability & BIT(j)))
				continue;
			vcd = &drv->vcd[j];

			if (j == PERF_OL_VCD) {
				vcd->offsets = (u32 *)&crm->desc->hw_drv_perf_ol_vcd_regs;
				vcd->num_resources = num_perf_ol_vcds;
			} else if (j == BW_VOTE_VCD) {
				vcd->offsets = (u32 *)&crm->desc->hw_drv_bw_vote_vcd_regs;
				/* BW_VOTE_VCD can have multiple NDs with which BW can be voted */
				vcd->num_resources = num_nds;
			} else if (j == BW_PT_VOTE_VCD) {
				vcd->offsets = (u32 *)&crm->desc->hw_drv_bw_pt_vote_vcd_regs;
				vcd->num_resources = num_nds_pt;
			} else {
				continue;
			}
			vcd->num_pwr_states = num_pwr_states;
			ret = crm_probe_alloc_vcd_caches(crm, vcd);
			if (ret)
				return ret;
		}
	}

	for (i = 0; i < crm->num_sw_drvs; i++) {
		drv = &crm->sw_drvs[i];

		if (!drv->initialized)
			continue;

		drv->drv_type = CRM_SW_DRV;
		for (j = 0; j < MAX_VCD_TYPE; j++) {
			if (!(crm->desc->crm_capability & BIT(j)))
				continue;
			vcd = &drv->vcd[j];

			if (j == PERF_OL_VCD) {
				vcd->offsets = (u32 *)&crm->desc->sw_drv_perf_ol_vcd_regs;
				vcd->num_resources = num_perf_ol_vcds;
			} else if (j == BW_VOTE_VCD) {
				vcd->offsets = (u32 *)&crm->desc->sw_drv_bw_vote_vcd_regs;
				/* BW_VOTE_VCD can have multiple NDs with which BW can be voted */
				vcd->num_resources = num_nds;
			} else if (j == BW_PT_VOTE_VCD) {
				vcd->offsets = (u32 *)&crm->desc->sw_drv_bw_pt_vote_vcd_regs;
				vcd->num_resources = num_nds_pt;
			} else {
				continue;
			}

			vcd->num_pwr_states = MAX_SW_DRV_PWR_STATES;
			ret = crm_probe_alloc_vcd_caches(crm, vcd);
			if (ret)
				return ret;
		}
	}

	crm->crmb_mgr.offsets = (u32 *)&crm->desc->crmb_regs;
	crm->crmb_mgr.num_resources = num_bw_vote_vcds;
	crm->crmb_pt_mgr.offsets = (u32 *)&crm->desc->crmb_pt_regs;
	crm->crmb_pt_mgr.num_resources = num_nds_pt;
	crm->crmc_mgr.offsets = (u32 *)&crm->desc->crmc_regs;
	crm->crmc_mgr.num_resources = num_perf_ol_vcds;
	crm->crmv_mgr.offsets = (u32 *)&crm->desc->crmv_regs;
	crm->crmv_mgr.num_resources = num_rails;

	return 0;
}

static struct crm_drv *crm_probe_get_drvs(struct crm_drv_top *crm, int num_drvs,
					  const char *prop_name, const char *name)
{
	struct device_node *dn = crm->dev->of_node;
	u32 *drv_ids;
	int i, id;
	int ret;
	struct crm_drv *drvs;

	if (!num_drvs)
		return ERR_PTR(-EINVAL);

	drvs = devm_kcalloc(crm->dev, num_drvs, sizeof(struct crm_drv), GFP_KERNEL);
	if (!drvs)
		return ERR_PTR(-ENOMEM);

	drv_ids = kcalloc(num_drvs, sizeof(u32), GFP_KERNEL);
	if (!drv_ids)
		return ERR_PTR(-ENOMEM);

	ret = of_property_read_u32_array(dn, prop_name, drv_ids, num_drvs);
	if (ret) {
		kfree(drv_ids);
		return ERR_PTR(ret);
	}

	for (i = 0; i < num_drvs; i++) {
		id = drv_ids[i];

		scnprintf(drvs[i].name, sizeof(drvs[i].name), "%s_%s_%d", crm->name, name, id);
		drvs[i].drv_id = id;
		drvs[i].base = crm->base;
		spin_lock_init(&drvs[i].lock);
		spin_lock_init(&drvs[i].cache_lock);

#if IS_ENABLED(CONFIG_IPC_LOGGING)
		drvs[i].ipc_log_ctx = ipc_log_context_create(
						CRM_DRV_IPC_LOG_SIZE,
						drvs[i].name, 0);
#endif

		drvs[i].offsets = (u32 *)&crm->desc->chn_regs;
		drvs[i].num_channels = crm->num_channels;
		drvs[i].initialized = true;
		drvs[i].set_hw_chn_switch_ctrl = crm->desc->set_hw_chn_switch_ctrl;
	}

	kfree(drv_ids);
	return drvs;
}

static int crm_probe_drvs(struct crm_drv_top *crm, struct device_node *dn)
{
	u32 crm_ver, major_ver, minor_ver;
	u32 crm_cfg, crm_cfg_2;

	crm_ver = readl_relaxed(crm->common + crm->desc->cfg_regs[CRM_VERSION]);
	major_ver = field_get(crm->desc->cfg_regs[MAJOR_VERSION], crm_ver);
	minor_ver = field_get(crm->desc->cfg_regs[MINOR_VERSION], crm_ver);

	crm_cfg = readl_relaxed(crm->common + crm->desc->cfg_regs[CRM_CFG_PARAM_1]);
	crm->max_hw_drv = field_get(crm->desc->cfg_regs[NUM_HW_DRVS], crm_cfg);
	crm->max_sw_drv = field_get(crm->desc->cfg_regs[NUM_SW_DRVS], crm_cfg);
	crm->num_channels = field_get(crm->desc->cfg_regs[NUM_CHANNELS], crm_cfg);

	crm->num_hw_drvs = of_property_count_u32_elems(dn, "qcom,hw-drv-ids");
	if (crm->num_hw_drvs < 0) {
		crm->num_hw_drvs = 0;
		goto skip_hw_drvs;
	}

	crm->hw_drvs = crm_probe_get_drvs(crm, crm->num_hw_drvs, "qcom,hw-drv-ids", "hw_drv");
	if (IS_ERR(crm->hw_drvs))
		return PTR_ERR(crm->hw_drvs);

skip_hw_drvs:
	crm->num_sw_drvs = of_property_count_u32_elems(dn, "qcom,sw-drv-ids");
	if (crm->num_sw_drvs < 0) {
		crm->num_sw_drvs = 0;
		goto skip_sw_drvs;
	}

	crm->sw_drvs = crm_probe_get_drvs(crm, crm->num_sw_drvs, "qcom,sw-drv-ids", "sw_drv");
	if (IS_ERR(crm->sw_drvs))
		return PTR_ERR(crm->sw_drvs);

skip_sw_drvs:
	if (crm->num_sw_drvs > crm->max_sw_drv ||
	    crm->num_hw_drvs > crm->max_hw_drv ||
	    (!crm->num_sw_drvs && !crm->num_hw_drvs))
		return -EINVAL;

	crm_cfg_2 = readl_relaxed(crm->common + crm->desc->cfg_regs[CRM_CFG_PARAM_2]);

	return crm_probe_set_vcd_caches(crm, crm_cfg, crm_cfg_2);
}

static int crm_probe_platform_resources(struct platform_device *pdev, struct crm_drv_top *crm)
{
	struct resource *res;

	crm->base = devm_platform_get_and_ioremap_resource(pdev, 0, NULL);
	if (IS_ERR(crm->base))
		return -ENOMEM;

	crm->crmb_mgr.base = devm_platform_get_and_ioremap_resource(pdev, 1, &res);
	if (IS_ERR(crm->crmb_mgr.base))
		return -ENOMEM;
	strscpy(crm->crmb_mgr.name, res->name, sizeof(crm->crmb_mgr.name));

	crm->crmb_pt_mgr.base = devm_platform_get_and_ioremap_resource(pdev, 2, &res);
	if (IS_ERR(crm->crmb_pt_mgr.base))
		return -ENOMEM;
	strscpy(crm->crmb_pt_mgr.name, res->name, sizeof(crm->crmb_pt_mgr.name));

	crm->crmc_mgr.base = devm_platform_get_and_ioremap_resource(pdev, 3, &res);
	if (IS_ERR(crm->crmc_mgr.base))
		return -ENOMEM;
	strscpy(crm->crmc_mgr.name, res->name, sizeof(crm->crmc_mgr.name));

	crm->crmv_mgr.base = devm_platform_get_and_ioremap_resource(pdev, 4, &res);
	if (IS_ERR(crm->crmv_mgr.base))
		return -ENOMEM;
	strscpy(crm->crmv_mgr.name, res->name, sizeof(crm->crmv_mgr.name));

	crm->common = devm_platform_get_and_ioremap_resource(pdev, 5, &res);
	if (IS_ERR(crm->common))
		return -ENOMEM;

	return 0;
}

static int crm_probe(struct platform_device *pdev)
{
	struct device_node *dn = pdev->dev.of_node;
	struct crm_drv_top *crm;
	const char *name;
	u32 crm_en;
	int ret;

	crm = devm_kzalloc(&pdev->dev, sizeof(*crm), GFP_KERNEL);
	if (!crm)
		return -ENOMEM;

	crm->desc = of_device_get_match_data(&pdev->dev);
	if (!crm->desc)
		return -EINVAL;

	name = of_get_property(dn, "label", NULL);
	if (!name)
		name = dev_name(&pdev->dev);

	crm->pdev = pdev;
	crm->dev = &pdev->dev;
	scnprintf(crm->name, sizeof(crm->name), "%s", name);

	ret = crm_probe_platform_resources(pdev, crm);
	if (ret)
		return ret;

	crm_en = readl_relaxed(crm->common + crm->desc->cfg_regs[CRM_ENABLE]);
	if (!crm_en) {
		pr_err("%s: %s not enabled\n", __func__, crm->name);
		return -EINVAL;
	}

	ret = crm_probe_drvs(crm, dn);
	if (ret)
		return ret;

	ret = crm_probe_get_irqs(crm);
	if (ret)
		return ret;

	crm_set_chn_behave(crm);
	crm_set_hw_chn_switch_ctrl(crm);

	INIT_LIST_HEAD(&crm->list);
	list_add_tail(&crm->list, &crm_dev_list);
	dev_set_drvdata(&pdev->dev, crm);

	return ret;
}

static const struct crm_desc pcie_crm_desc_v2 = {
	.set_chn_behave = false,
	.set_hw_chn_switch_ctrl = false,
	.crm_capability = PERF_OL_VOTING_FLAG | BW_PT_VOTING_FLAG,
	.cfg_regs = {
		[CRM_VERSION]			= 0x0,
		[MAJOR_VERSION]			= GENMASK(23, 16),
		[MINOR_VERSION]			= GENMASK(15, 8),
		[CRM_CFG_PARAM_1]		= 0x4,
		[NUM_OF_NODES_PT]		= GENMASK(31, 27),
		[NUM_VCD_VOTED_BY_BW]		= GENMASK(26, 24),
		[NUM_SW_DRVS]			= GENMASK(23, 20),
		[NUM_HW_DRVS]			= GENMASK(19, 16),
		[NUM_OF_RAILS]			= GENMASK(15, 12),
		[NUM_VCD_VOTED_BY_PERF_OL]	= GENMASK(11, 8),
		[NUM_CHANNELS]			= GENMASK(7, 4),
		[NUM_PWR_STATES_PER_CH]		= GENMASK(3, 0),
		[CRM_CFG_PARAM_2]		= 0x8,
		[NUM_OF_NODES]			= GENMASK(30, 26),
		[CRM_ENABLE]			= 0xC,
	},
	.chn_regs = {
		[CHN_BUSY]			 = 0x370,
		[CHN_UPDATE]			 = 0x374,
		[CHN_BEHAVE]			 = 0x378,
		[CHN_DRV_DISTANCE]		 = 0x1000,
	},
	.crmb_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x50,
		[STATUS_BE]			 = 0x18,
		[STATUS_FE]			 = 0x1C,
	},
	.crmb_pt_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x14,
		[TCS_CMD_DATA]			 = 0x0,
		[TCS_CMD_ADDR]			 = 0x4,
		[TCS_CMD_CTRL]			 = 0x8,
		[TCS_CMD_STATUS]		 = 0xC,
		[TCS_CMD_ENABLE]		 = 0x10,
		[CRMB_PT_FSM_STATUS]		 = 0x144,
	},
	.crmc_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0,
		[AGGR_PERF_OL]			 = 0x4,
		[AGGR_PERF_OL_RESOURCE_DISTANCE] = 0xC,
		[CURR_PERF_OL]			 = 0x24,
		[CURR_PERF_OL_RESOURCE_DISTANCE] = 0x2B0,
		[SEQ_STATUS]			 = 0x4C,
		[SEQ_STATUS_RESOURCE_DISTANCE]	 = 0x2B0,
	},
	.crmv_regs = {
		[CRM_BASE]			= 0x0,
		[CRM_DISTANCE]			= 0x40,
		[AGGR_VOL_STS]			= 0x4,
		[SEQ_VOL_STS]			= 0x8,
		[CURR_VOL_STS]			= 0xC,
		[RAIL_FSM_STS]			= 0x14,
		[RAIL_TCS_STS]			= 0x3C,
	},
	.hw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x2C,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST2]			 = 0x8,
		[PWR_ST3]			 = 0xC,
		[PWR_ST4]			 = 0x10,
		[PWR_ST_CHN_DISTANCE]		 = 0x14,
		[STATUS]			 = 0x28,
		[PWR_IDX_STATUS]		 = 0x37C,
	},
	.hw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x2C,
		[PWR_ST0]			 = 0x58,
		[PWR_ST1]			 = 0x5C,
		[PWR_ST2]			 = 0x60,
		[PWR_ST3]			 = 0x64,
		[PWR_ST4]			 = 0x68,
		[PWR_ST_CHN_DISTANCE]		 = 0x14,
		[STATUS]			 = 0x80,
	},
	.hw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x2C,
		[PWR_ST0_PT]			 = 0xB0,
		[PWR_ST1_PT]			 = 0xB4,
		[PWR_ST2_PT]			 = 0xB8,
		[PWR_ST3_PT]			 = 0xBC,
		[PWR_ST4_PT]			 = 0xC0,
		[PWR_ST_CHN_DISTANCE]		 = 0x14,
		[STATUS]			 = 0xD8,
	},
	.sw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x3B0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x20,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST2]			 = 0x8,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0xC,
		[IRQ_STATUS]			 = 0x10,
		[IRQ_CLEAR]			 = 0x14,
		[IRQ_ENABLE]			 = 0x18,
		[FSM_STATUS]			 = 0X1C,
	},
	.sw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x3B0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0]			 = 0x40,
		[PWR_ST1]			 = 0x44,
		[PWR_ST2]			 = 0x48,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x4C,
		[IRQ_STATUS]			 = 0x160,
		[IRQ_CLEAR]			 = 0x164,
		[IRQ_ENABLE]			 = 0x168,
		[FSM_STATUS]			 = 0X16C,
	},
	.sw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x3B0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0_PT]			 = 0x60,
		[PWR_ST1_PT]			 = 0x64,
		[PWR_ST2_PT]			 = 0x68,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x6C,
		[IRQ_STATUS]			 = 0x170,
		[IRQ_CLEAR]			 = 0x174,
		[IRQ_ENABLE]			 = 0x178,
		[CRMB_PT_TRIGGER]		 = 0x270,
	},
};

static const struct crm_desc cam_crm_desc_v2 = {
	.set_chn_behave = true,
	.set_hw_chn_switch_ctrl = false,
	.crm_capability = PERF_OL_VOTING_FLAG | BW_VOTING_FLAG,
	.cfg_regs = {
		[CRM_VERSION]			= 0x0,
		[MAJOR_VERSION]			= GENMASK(23, 16),
		[MINOR_VERSION]			= GENMASK(15, 8),
		[CRM_CFG_PARAM_1]		= 0x4,
		[NUM_OF_NODES_PT]		= GENMASK(31, 27),
		[NUM_VCD_VOTED_BY_BW]		= GENMASK(26, 24),
		[NUM_SW_DRVS]			= GENMASK(23, 20),
		[NUM_HW_DRVS]			= GENMASK(19, 16),
		[NUM_OF_RAILS]			= GENMASK(15, 12),
		[NUM_VCD_VOTED_BY_PERF_OL]	= GENMASK(11, 8),
		[NUM_CHANNELS]			= GENMASK(7, 4),
		[NUM_PWR_STATES_PER_CH]		= GENMASK(3, 0),
		[CRM_CFG_PARAM_2]		= 0x8,
		[NUM_OF_NODES]			= GENMASK(30, 26),
		[CRM_ENABLE]			= 0xC,
	},
	.chn_regs = {
		[CHN_BUSY]			 = 0xDC,
		[CHN_UPDATE]			 = 0xE0,
		[CHN_BEHAVE]			 = 0xE4,
		[CHN_DRV_DISTANCE]		 = 0x29C,
	},
	.crmb_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x50,
		[STATUS_BE]			 = 0x18,
		[STATUS_FE]			 = 0x1C,
	},
	.crmc_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x0,
		[AGGR_PERF_OL]			 = 0x4,
		[AGGR_PERF_OL_RESOURCE_DISTANCE] = 0xC,
		[CURR_PERF_OL]			 = 0x6C,
		[CURR_PERF_OL_RESOURCE_DISTANCE] = 0x210,
		[SEQ_STATUS]			 = 0x94,
		[SEQ_STATUS_RESOURCE_DISTANCE]	 = 0x210,
	},
	.crmv_regs = {
		[CRM_BASE]			= 0x0,
		[CRM_DISTANCE]			= 0x40,
		[AGGR_VOL_STS]			= 0x4,
		[SEQ_VOL_STS]			= 0x8,
		[CURR_VOL_STS]			= 0xC,
		[RAIL_FSM_STS]			= 0x14,
		[RAIL_TCS_STS]			= 0x3C,
	},
	.hw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0x10,
		[PWR_IDX_STATUS]		 = 0xE8,
	},
	.hw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0]			 = 0xA0,
		[PWR_ST1]			 = 0xA4,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0xB0,
	},
	.hw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0_PT]			 = 0xC8,
		[PWR_ST1_PT]			 = 0xCC,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0xD8,
	},
	.sw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x11C,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x20,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST2]			 = 0x8,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0xC,
		[IRQ_STATUS]			 = 0x10,
		[IRQ_CLEAR]			 = 0x14,
		[IRQ_ENABLE]			 = 0x18,
		[FSM_STATUS]			 = 0x1C,
	},
	.sw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x11C,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0]			 = 0x100,
		[PWR_ST1]			 = 0x104,
		[PWR_ST2]			 = 0x108,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x10C,
		[IRQ_STATUS]			 = 0x130,
		[IRQ_CLEAR]			 = 0x134,
		[IRQ_ENABLE]			 = 0x138,
		[FSM_STATUS]			 = 0x13C,
	},
	.sw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x11C,
		[DRV_DISTANCE]			 = 0x29C,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0_PT]			 = 0x120,
		[PWR_ST1_PT]			 = 0x124,
		[PWR_ST2_PT]			 = 0x128,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x12C,
		[IRQ_STATUS]			 = 0x140,
		[IRQ_CLEAR]			 = 0x144,
		[IRQ_ENABLE]			 = 0x148,
		[CRMB_PT_TRIGGER]		 = 0x150,
	},
};

static const struct crm_desc disp_crm_desc_v2 = {
	.set_chn_behave = false,
	.set_hw_chn_switch_ctrl = true,
	.crm_capability = PERF_OL_VOTING_FLAG | BW_VOTING_FLAG | BW_PT_VOTING_FLAG,
	.cfg_regs = {
		[CRM_VERSION]			= 0x0,
		[MAJOR_VERSION]			= GENMASK(23, 16),
		[MINOR_VERSION]			= GENMASK(15, 8),
		[CRM_CFG_PARAM_1]		= 0x4,
		[NUM_OF_NODES_PT]		= GENMASK(31, 27),
		[NUM_VCD_VOTED_BY_BW]		= GENMASK(26, 24),
		[NUM_SW_DRVS]			= GENMASK(23, 20),
		[NUM_HW_DRVS]			= GENMASK(19, 16),
		[NUM_OF_RAILS]			= GENMASK(15, 12),
		[NUM_VCD_VOTED_BY_PERF_OL]	= GENMASK(11, 8),
		[NUM_CHANNELS]			= GENMASK(7, 4),
		[NUM_PWR_STATES_PER_CH]		= GENMASK(3, 0),
		[CRM_CFG_PARAM_2]		= 0x8,
		[NUM_OF_NODES]			= GENMASK(30, 26),
		[CRM_ENABLE]			= 0xC,
	},
	.chn_regs = {
		[CHN_BUSY]			 = 0xA0,
		[CHN_UPDATE]			 = 0xA4,
		[CHN_BEHAVE]			 = 0xA8,
		[CHN_DRV_DISTANCE]		 = 0x1000,
	},
	.crmb_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x78,
		[STATUS_BE]			 = 0x18,
		[STATUS_FE]			 = 0x1C,
	},
	.crmb_pt_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x14,
		[TCS_CMD_DATA]			 = 0x0,
		[TCS_CMD_ADDR]			 = 0x4,
		[TCS_CMD_CTRL]			 = 0x8,
		[TCS_CMD_STATUS]		 = 0xC,
		[TCS_CMD_ENABLE]		 = 0x10,
		[CRMB_PT_FSM_STATUS]		 = 0x7c,
	},
	.crmc_regs = {
		[CRM_BASE]			 = 0x0,
		[CRM_DISTANCE]			 = 0x0,
		[AGGR_PERF_OL]			 = 0x4,
		[AGGR_PERF_OL_RESOURCE_DISTANCE] = 0xC,
		[CURR_PERF_OL]			 = 0x18,
		[CURR_PERF_OL_RESOURCE_DISTANCE] = 0x268,
		[SEQ_STATUS]			 = 0x40,
		[SEQ_STATUS_RESOURCE_DISTANCE]	 = 0x268,
	},
	.crmv_regs = {
		[CRM_BASE]			= 0x0,
		[CRM_DISTANCE]			= 0x40,
		[AGGR_VOL_STS]			= 0x4,
		[SEQ_VOL_STS]			= 0x8,
		[CURR_VOL_STS]			= 0xC,
		[RAIL_FSM_STS]			= 0x14,
		[RAIL_TCS_STS]			= 0x3C,
	},
	.hw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0x10,
		[PWR_IDX_STATUS]		 = 0xAC,
	},
	.hw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0]			 = 0x14,
		[PWR_ST1]			 = 0x18,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0x24,
	},
	.hw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0x0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x14,
		[PWR_ST0_PT]			 = 0x28,
		[PWR_ST1_PT]			 = 0x2C,
		[PWR_ST_CHN_DISTANCE]		 = 0x8,
		[STATUS]			 = 0x38,
	},
	.sw_drv_perf_ol_vcd_regs = {
		[DRV_BASE]			 = 0xE0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x20,
		[PWR_ST0]			 = 0x0,
		[PWR_ST1]			 = 0x4,
		[PWR_ST2]			 = 0x8,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0xC,
		[IRQ_STATUS]			 = 0x10,
		[IRQ_CLEAR]			 = 0x14,
		[IRQ_ENABLE]			 = 0x18,
		[FSM_STATUS]			 = 0x1C,
	},
	.sw_drv_bw_vote_vcd_regs = {
		[DRV_BASE]			 = 0xE0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0]			 = 0x20,
		[PWR_ST1]			 = 0x24,
		[PWR_ST2]			 = 0x28,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x2C,
		[IRQ_STATUS]			 = 0x90,
		[IRQ_CLEAR]			 = 0x94,
		[IRQ_ENABLE]			 = 0x98,
		[FSM_STATUS]			 = 0x9C,
	},
	.sw_drv_bw_pt_vote_vcd_regs = {
		[DRV_BASE]			 = 0xE0,
		[DRV_DISTANCE]			 = 0x1000,
		[DRV_RESOURCE_DISTANCE]		 = 0x10,
		[PWR_ST0_PT]			 = 0x30,
		[PWR_ST1_PT]			 = 0x34,
		[PWR_ST2_PT]			 = 0x38,
		[PWR_ST_CHN_DISTANCE]		 = 0x0,
		[STATUS]			 = 0x3C,
		[IRQ_STATUS]			 = 0xA0,
		[IRQ_CLEAR]			 = 0xA4,
		[IRQ_ENABLE]			 = 0xA8,
		[CRMB_PT_TRIGGER]		 = 0x100,
	},
};

static const struct of_device_id crm_drv_match[] = {
	{ .compatible = "qcom,cam-crm-v2", .data = &cam_crm_desc_v2},
	{ .compatible = "qcom,pcie-crm-v2", .data = &pcie_crm_desc_v2},
	{ .compatible = "qcom,disp-crm-v2", .data = &disp_crm_desc_v2},
	{ }
};
MODULE_DEVICE_TABLE(of, crm_drv_match);

static struct platform_driver crm_driver = {
	.probe = crm_probe,
	.driver = {
		  .name = "crm",
		  .of_match_table = crm_drv_match,
		  .suppress_bind_attrs = true,
	},
};
module_platform_driver(crm_driver);

MODULE_DESCRIPTION("Qualcomm Technologies, Inc. (QTI) CRM-V2 Driver");
MODULE_LICENSE("GPL");
