// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2021, The Linux Foundation. All rights reserved.
 * Copyright (c) 2022 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/module.h>
#include <linux/timer.h>
#include <linux/kernel.h>

#include <media/cam_req_mgr.h>
#include "cam_soc_util.h"
#include "cam_smmu_api.h"
#include "cam_cdm_intf_api.h"
#include "cam_cdm.h"
#include "cam_cdm_core_common.h"
#include "cam_cdm_soc.h"
#include "cam_io_util.h"
#include "cam_cdm_hw_reg_1_0.h"
#include "cam_cdm_hw_reg_1_1.h"
#include "cam_cdm_hw_reg_1_2.h"
#include "cam_cdm_hw_reg_2_0.h"
#include "cam_cdm_hw_reg_2_1.h"
#include "camera_main.h"
#include "cam_trace.h"
#include "cam_req_mgr_workq.h"
#include "cam_common_util.h"

#define CAM_CDM_BL_FIFO_WAIT_TIMEOUT         2000
#define CAM_CDM_DBG_GEN_IRQ_USR_DATA         0xff
#define CAM_CDM_MAX_BL_LENGTH                0x100000
#define CAM_CDM_FIFO_LEN_REG_LEN_MASK        0xFFFFF
#define CAM_CDM_FIFO_LEN_REG_TAG_MASK        0xFF
#define CAM_CDM_FIFO_LEN_REG_TAG_SHIFT       24
#define CAM_CDM_FIFO_LEN_REG_ARB_SHIFT       20

static void cam_hw_cdm_work(struct work_struct *work);

/* DT match table entry for all CDM variants*/
static const struct of_device_id msm_cam_hw_cdm_dt_match[] = {
	{
		.compatible = CAM_HW_CDM_CPAS_0_NAME,
		.data = &cam_cdm_1_0_reg_offset,
	},
	{
		.compatible = CAM_HW_CDM_CPAS_NAME_1_0,
		.data = &cam_cdm_1_0_reg_offset,
	},
	{
		.compatible = CAM_HW_CDM_CPAS_NAME_1_1,
		.data = &cam_cdm_1_1_reg_offset,
	},
	{
		.compatible = CAM_HW_CDM_CPAS_NAME_1_2,
		.data = &cam_cdm_1_2_reg_offset,
	},
	{
		.compatible = CAM_HW_CDM_IFE_NAME_1_2,
		.data = &cam_cdm_1_2_reg_offset,
	},
	{
		.compatible = CAM_HW_CDM_CPAS_NAME_2_0,
		.data = &cam_cdm_2_0_reg_offset,
	},
	{
		.compatible = CAM_HW_CDM_OPE_NAME_2_0,
		.data = &cam_cdm_2_0_reg_offset,
	},
	{
		.compatible = CAM_HW_CDM_CPAS_NAME_2_1,
		.data = &cam_cdm_2_1_reg_offset,
	},
	{
		.compatible = CAM_HW_CDM_RT_NAME_2_1,
		.data = &cam_cdm_2_1_reg_offset,
	},
	{
		.compatible = CAM_HW_CDM_OPE_NAME_2_1,
		.data = &cam_cdm_2_1_reg_offset,
	},
	{},
};

static enum cam_cdm_id cam_hw_cdm_get_id_by_name(char *name)
{
	if (strnstr(name, CAM_HW_CDM_CPAS_0_NAME,
			strlen(CAM_HW_CDM_CPAS_0_NAME)))
		return CAM_CDM_CPAS;
	if (strnstr(name, CAM_HW_CDM_CPAS_NAME_1_0,
			strlen(CAM_HW_CDM_CPAS_NAME_1_0)))
		return CAM_CDM_CPAS;
	if (strnstr(name, CAM_HW_CDM_CPAS_NAME_1_1,
			strlen(CAM_HW_CDM_CPAS_NAME_1_1)))
		return CAM_CDM_CPAS;
	if (strnstr(name, CAM_HW_CDM_CPAS_NAME_1_2,
			strlen(CAM_HW_CDM_CPAS_NAME_1_2)))
		return CAM_CDM_CPAS;
	if (strnstr(name, CAM_HW_CDM_IFE_NAME_1_2,
			strlen(CAM_HW_CDM_IFE_NAME_1_2)))
		return CAM_CDM_IFE;
	if (strnstr(name, CAM_HW_CDM_CPAS_NAME_2_0,
			strlen(CAM_HW_CDM_CPAS_NAME_2_0)))
		return CAM_CDM_CPAS;
	if (strnstr(name, CAM_HW_CDM_OPE_NAME_2_0,
			strlen(CAM_HW_CDM_OPE_NAME_2_0)))
		return CAM_CDM_OPE;
	if (strnstr(name, CAM_HW_CDM_CPAS_NAME_2_1,
			strlen(CAM_HW_CDM_CPAS_NAME_2_1)))
		return CAM_CDM_CPAS;
	if (strnstr(name, CAM_HW_CDM_RT_NAME_2_1,
			strlen(CAM_HW_CDM_RT_NAME_2_1)))
		return CAM_CDM_RT;
	if (strnstr(name, CAM_HW_CDM_OPE_NAME_2_1,
			strlen(CAM_HW_CDM_OPE_NAME_2_1)))
		return CAM_CDM_OPE;

	return CAM_CDM_MAX;
}

static int cam_hw_cdm_enable_bl_done_irq(struct cam_hw_info *cdm_hw,
	bool enable, uint32_t fifo_idx)
{
	int rc = -EIO;
	uint32_t irq_mask = 0;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;

	if (cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->irq_reg[fifo_idx]->irq_mask,
			&irq_mask)) {
		CAM_ERR(CAM_CDM, "Failed to read CDM IRQ mask");
		return rc;
	}

	if (enable == true) {
		if (cam_cdm_write_hw_reg(cdm_hw,
				core->offsets->irq_reg[fifo_idx]->irq_mask,
				(irq_mask | 0x4))) {
			CAM_ERR(CAM_CDM, "Write failed to enable BL done irq");
		} else {
			set_bit(fifo_idx, &core->cdm_status);
			rc = 0;
			CAM_DBG(CAM_CDM, "BL done irq enabled =%d",
				test_bit(fifo_idx, &core->cdm_status));
		}
	} else {
		if (cam_cdm_write_hw_reg(cdm_hw,
				core->offsets->irq_reg[fifo_idx]->irq_mask,
				(irq_mask & 0x70003))) {
			CAM_ERR(CAM_CDM, "Write failed to disable BL done irq");
		} else {
			clear_bit(fifo_idx, &core->cdm_status);
			rc = 0;
			CAM_DBG(CAM_CDM, "BL done irq disable =%d",
				test_bit(fifo_idx, &core->cdm_status));
		}
	}
	return rc;
}

static int cam_hw_cdm_pause_core(struct cam_hw_info *cdm_hw, bool pause)
{
	int rc = 0;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	uint32_t val = 0x1;

	if (pause)
		val |= 0x2;

	if (cam_cdm_write_hw_reg(cdm_hw,
			core->offsets->cmn_reg->core_en, val)) {
		CAM_ERR(CAM_CDM, "Failed to Write core_en for %s%u",
			cdm_hw->soc_info.label_name,
			cdm_hw->soc_info.index);
		rc = -EIO;
	}

	return rc;
}

int cam_hw_cdm_enable_core_dbg(struct cam_hw_info *cdm_hw, uint32_t value)
{
	int rc = 0;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;

	if (cam_cdm_write_hw_reg(cdm_hw,
			core->offsets->cmn_reg->core_debug,
			value)) {
		CAM_ERR(CAM_CDM, "Failed to Write CDM HW core debug");
		rc = -EIO;
	}

	return rc;
}

int cam_hw_cdm_disable_core_dbg(struct cam_hw_info *cdm_hw)
{
	int rc = 0;
	struct cam_cdm *cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	if (cam_cdm_write_hw_reg(cdm_hw,
			cdm_core->offsets->cmn_reg->core_debug, 0)) {
		CAM_ERR(CAM_CDM, "Failed to Write CDM HW core debug");
		rc = -EIO;
	}

	return rc;
}

void cam_hw_cdm_dump_scratch_registors(struct cam_hw_info *cdm_hw)
{
	uint32_t dump_reg = 0;
	int i;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;

	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->core_en, &dump_reg);
	CAM_ERR(CAM_CDM, "dump core en=%x", dump_reg);

	for (i = 0; i < core->offsets->reg_data->num_scratch_reg; i++) {
		cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->cmn_reg->scratch[i]->scratch_reg,
			&dump_reg);
		CAM_ERR(CAM_CDM, "dump scratch%d=%x", i, dump_reg);
	}
}

int cam_hw_cdm_bl_fifo_pending_bl_rb_in_fifo(
	struct cam_hw_info *cdm_hw,
	uint32_t fifo_idx,
	uint32_t *pending_bl_req)
{
	int rc = 0;
	uint32_t fifo_reg;
	uint32_t fifo_id;

	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;

	if (fifo_idx >= CAM_CDM_BL_FIFO_REG_NUM) {
		CAM_ERR(CAM_CDM,
			"BL_FIFO index is wrong. fifo_idx %d",
			fifo_idx);
		rc = -EINVAL;
		goto end;
	}

	fifo_reg = fifo_idx / 2;
	fifo_id = fifo_idx % 2;

	if (core->offsets->cmn_reg->pending_req[fifo_reg]) {
		if (cam_cdm_read_hw_reg(cdm_hw,
				core->offsets->cmn_reg->pending_req
					[fifo_reg]->rb_offset,
				pending_bl_req)) {
			CAM_ERR(CAM_CDM, "Error reading CDM register");
			rc = -EIO;
			goto end;
		}

		*pending_bl_req = (*pending_bl_req >> (
			core->offsets->cmn_reg->pending_req
				[fifo_reg]->rb_next_fifo_shift *
			fifo_id)) & core->offsets->cmn_reg->pending_req
				[fifo_reg]->rb_mask;
		rc = 0;
	}

	CAM_DBG(CAM_CDM, "Number of pending bl entries:%d in fifo: %d",
			*pending_bl_req, fifo_id);

end:
	return rc;
}

static void cam_hw_cdm_dump_bl_fifo_data(struct cam_hw_info *cdm_hw)
{
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	int i, j;
	uint32_t num_pending_req = 0, dump_reg[2];

	for (i = 0; i < core->offsets->reg_data->num_bl_fifo; i++) {
		cam_hw_cdm_bl_fifo_pending_bl_rb_in_fifo(cdm_hw, i, &num_pending_req);

		CAM_INFO(CAM_CDM, "Fifo:%d content dump. num_pending_BLs: %d", i, num_pending_req);

		if (!num_pending_req)
			continue;

		for (j = 0; j < core->bl_fifo[i].bl_depth; j++) {
			cam_cdm_write_hw_reg(cdm_hw, core->offsets->cmn_reg->bl_fifo_rb, j);
			cam_cdm_read_hw_reg(cdm_hw, core->offsets->cmn_reg->bl_fifo_base_rb,
				&dump_reg[0]);
			cam_cdm_read_hw_reg(cdm_hw, core->offsets->cmn_reg->bl_fifo_len_rb,
				&dump_reg[1]);
			CAM_INFO(CAM_CDM,
				"BL_entry:%d base_addr:0x%x, len:%d, ARB:%d, tag:%d",
				j, dump_reg[0],
				(dump_reg[1] & CAM_CDM_CURRENT_BL_LEN),
				(dump_reg[1] & CAM_CDM_CURRENT_BL_ARB) >>
					CAM_CDM_CURRENT_BL_ARB_SHIFT,
				(dump_reg[1] & CAM_CDM_CURRENT_BL_TAG) >>
					CAM_CDM_CURRENT_BL_TAG_SHIFT);
		}
	}
}

void cam_hw_cdm_dump_core_debug_registers(struct cam_hw_info *cdm_hw,
	bool pause_core)
{
	uint32_t dump_reg[4], core_dbg = 0x100;
	uint32_t cdm_version = 0;
	int i;
	bool is_core_paused_already;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	const struct cam_cdm_icl_regs *inv_cmd_log =
		core->offsets->cmn_reg->icl_reg;

	CAM_INFO(CAM_CDM, "Dumping debug data for %s%u",
		cdm_hw->soc_info.label_name, cdm_hw->soc_info.index);


	if (pause_core) {
		cam_hw_cdm_pause_core(cdm_hw, true);
		usleep_range(1000, 1010);
	}

	cam_cdm_read_hw_reg(cdm_hw, core->offsets->cmn_reg->cdm_hw_version,
		&cdm_version);

	if (core_dbg & CAM_CDM_CORE_DBG_TEST_BUS_EN_MASK) {
		for (i = 0; i < CAM_CDM_NUM_TEST_BUS; i++) {
			core_dbg &= ~CAM_CDM_CORE_DBG_TEST_BUS_SEL_MASK;
			core_dbg |= ((i << CAM_CDM_CORE_DBG_TEST_BUS_SEL_SHIFT) &
				(CAM_CDM_CORE_DBG_TEST_BUS_SEL_MASK));
			cam_hw_cdm_enable_core_dbg(cdm_hw, core_dbg);
			cam_cdm_read_hw_reg(cdm_hw, core->offsets->cmn_reg->debug_status,
				&dump_reg[0]);

			CAM_INFO(CAM_CDM, "Core_dbg: 0x%x, Debug_status[%d]: 0x%x",
				core_dbg, i, dump_reg[0]);
		}

		core_dbg &= ~(CAM_CDM_CORE_DBG_TEST_BUS_EN_MASK |
			CAM_CDM_CORE_DBG_TEST_BUS_SEL_MASK);
		cam_hw_cdm_enable_core_dbg(cdm_hw, core_dbg);
	} else {
		cam_hw_cdm_enable_core_dbg(cdm_hw, core_dbg);

		cam_cdm_read_hw_reg(cdm_hw, core->offsets->cmn_reg->debug_status,
			&dump_reg[0]);

		CAM_INFO(CAM_CDM, "Debug_status: 0x%x", dump_reg[0]);
	}

	cam_cdm_read_hw_reg(cdm_hw, core->offsets->cmn_reg->core_en, &dump_reg[0]);
	cam_cdm_read_hw_reg(cdm_hw, core->offsets->cmn_reg->usr_data, &dump_reg[1]);
	CAM_INFO(CAM_CDM, "Core_en: %u, Core_pause: %u User_data: 0x%x",
		(dump_reg[0] & CAM_CDM_CORE_EN_MASK),
		(bool)(dump_reg[0] & CAM_CDM_CORE_PAUSE_MASK),
		dump_reg[1]);

	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->current_used_ahb_base, &dump_reg[0]);

	if (cdm_version >= CAM_CDM_VERSION_2_0)
		CAM_INFO(CAM_CDM,
			"Current AHB base address: 0x%x set by change base cmd by fifo: %u",
			dump_reg[0] & CAM_CDM_AHB_ADDR_MASK,
			(dump_reg[0] & CAM_CDM_AHB_LOG_CID_MASK) >>
				CAM_CDM_AHB_LOG_CID_SHIFT);
	else
		CAM_INFO(CAM_CDM,
			"Current AHB base address: 0x%x set by change base cmd",
			dump_reg[0] & CAM_CDM_AHB_ADDR_MASK);

	if (core_dbg & CAM_CDM_CORE_DBG_LOG_AHB_MASK) {
		cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->cmn_reg->last_ahb_addr,
			&dump_reg[0]);
		cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->cmn_reg->last_ahb_data,
			&dump_reg[1]);

		if (cdm_version >= CAM_CDM_VERSION_2_0)
			CAM_INFO(CAM_CDM,
				"Last AHB addr: 0x%x, data: 0x%x that cdm sent out from fifo: %u",
				(dump_reg[0] & CAM_CDM_AHB_ADDR_MASK),
				dump_reg[1],
				(dump_reg[0] & CAM_CDM_AHB_LOG_CID_MASK) >>
					CAM_CDM_AHB_LOG_CID_SHIFT);
		else
			CAM_INFO(CAM_CDM,
				"Last AHB addr: 0x%x, data: 0x%x that cdm sent out",
				(dump_reg[0] & CAM_CDM_AHB_ADDR_MASK),
				dump_reg[1]);
	} else {
		CAM_INFO(CAM_CDM, "CDM HW AHB dump not enabled");
	}

	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->last_ahb_err_addr,
		&dump_reg[0]);
	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->last_ahb_err_data,
		&dump_reg[1]);

	if (cdm_version >= CAM_CDM_VERSION_2_0)
		CAM_INFO(CAM_CDM,
			"Last Bad AHB addr: 0x%x and data: 0x%x from fifo: %u",
			(dump_reg[0] & CAM_CDM_AHB_ADDR_MASK), dump_reg[1],
			(dump_reg[0] & CAM_CDM_AHB_LOG_CID_MASK) >>
				CAM_CDM_AHB_LOG_CID_SHIFT);
	else
		CAM_INFO(CAM_CDM, "Last Bad AHB addr: 0x%x and data: 0x%x",
			(dump_reg[0] & CAM_CDM_AHB_ADDR_MASK), dump_reg[1]);

	if (inv_cmd_log) {
		if (inv_cmd_log->misc_regs) {
			cam_cdm_read_hw_reg(cdm_hw,
				inv_cmd_log->misc_regs->icl_status,
				&dump_reg[0]);
			CAM_INFO(CAM_CDM,
				"ICL_Status: last_invalid_fifo: %u, last known good fifo: %u",
				(dump_reg[0] & CAM_CDM_ICL_STATUS_INV_CID_MASK),
				(dump_reg[0] &
					CAM_CDM_ICL_STATUS_LAST_CID_MASK) >>
					CAM_CDM_ICL_STATUS_LAST_CID_SHIFT);
			cam_cdm_read_hw_reg(cdm_hw,
				inv_cmd_log->misc_regs->icl_inv_bl_addr,
				&dump_reg[0]);
			CAM_INFO(CAM_CDM,
				"Last Inv Command BL's base_addr: 0x%x",
				dump_reg[0]);
		}
		if (inv_cmd_log->data_regs) {
			cam_cdm_read_hw_reg(cdm_hw,
				inv_cmd_log->data_regs->icl_inv_data,
				&dump_reg[0]);
			CAM_INFO(CAM_CDM, "First word of Last Inv cmd: 0x%x",
				dump_reg[0]);

			cam_cdm_read_hw_reg(cdm_hw,
				inv_cmd_log->data_regs->icl_last_data_0,
				&dump_reg[0]);
			cam_cdm_read_hw_reg(cdm_hw,
				inv_cmd_log->data_regs->icl_last_data_1,
				&dump_reg[1]);
			cam_cdm_read_hw_reg(cdm_hw,
				inv_cmd_log->data_regs->icl_last_data_2,
				&dump_reg[2]);

			CAM_INFO(CAM_CDM,
				"Last good cdm command's word[0]: 0x%x, word[1]: 0x%x, word[2]: 0x%x",
				dump_reg[0], dump_reg[1], dump_reg[2]);
		}
	}

	if (core_dbg & CAM_CDM_CORE_DBG_FIFO_RB_EN_MASK) {
		cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->cmn_reg->core_en, &dump_reg[0]);
		is_core_paused_already = (bool)(dump_reg[0] & 0x20);
		if (!is_core_paused_already) {
			cam_hw_cdm_pause_core(cdm_hw, true);
			usleep_range(1000, 1010);
		}

		cam_hw_cdm_dump_bl_fifo_data(cdm_hw);

		if (!is_core_paused_already)
			cam_hw_cdm_pause_core(cdm_hw, false);
	}

	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->core_cfg, &dump_reg[0]);

	if (cdm_version >= CAM_CDM_VERSION_2_0)
		CAM_INFO(CAM_CDM,
			"Core cfg: AHB_Burst_Len: %u, AHB_Burst_En: %u, AHB_stop_on_err: %u, Priority: %s, Imp_Wait: %u, Pririty_mask: 0x%x",
			dump_reg[0] & CAM_CDM_CORE_CFG_AHB_BURST_LEN_MASK,
			(bool)(dump_reg[0] &
				CAM_CDM_CORE_CFG_AHB_BURST_EN_MASK),
			(bool)(dump_reg[0] &
				CAM_CDM_CORE_CFG_AHB_STOP_ON_ERR_MASK),
			(dump_reg[0] & CAM_CDM_CORE_CFG_ARB_SEL_RR_MASK) ? "RR":
				"PRI",
			(bool)(dump_reg[0] &
				CAM_CDM_CORE_CFG_IMPLICIT_WAIT_EN_MASK),
			(dump_reg[0] & CAM_CDM_CORE_CFG_PRIORITY_MASK) >>
				CAM_CDM_CORE_CFG_PRIORITY_SHIFT);
	else
		CAM_INFO(CAM_CDM,
			"Core cfg: AHB_Burst_Len: %u, AHB_Burst_En: %u, AHB_stop_on_err: %u",
			dump_reg[0] & CAM_CDM_CORE_CFG_AHB_BURST_LEN_MASK,
			(bool)(dump_reg[0] &
				CAM_CDM_CORE_CFG_AHB_BURST_EN_MASK),
			(bool)(dump_reg[0] &
				CAM_CDM_CORE_CFG_AHB_STOP_ON_ERR_MASK));

	if (cdm_version >= CAM_CDM_VERSION_2_1) {
		cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->cmn_reg->irq_context_status,
			&dump_reg[0]);
		CAM_INFO(CAM_CDM, "irq_context_status: 0x%x", dump_reg[0]);
	}

	for (i = 0; i < core->offsets->reg_data->num_bl_fifo_irq; i++) {
		cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->irq_reg[i]->irq_status, &dump_reg[0]);
		cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->irq_reg[i]->irq_set, &dump_reg[1]);
		cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->irq_reg[i]->irq_mask, &dump_reg[2]);
		cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->irq_reg[i]->irq_clear, &dump_reg[3]);

		CAM_INFO(CAM_CDM,
			"cnt %d irq status 0x%x set 0x%x mask 0x%x clear 0x%x",
			i, dump_reg[0], dump_reg[1], dump_reg[2], dump_reg[3]);
	}

	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->current_bl_base, &dump_reg[0]);
	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->current_bl_len, &dump_reg[1]);

	if (cdm_version >= CAM_CDM_VERSION_2_0)
		CAM_INFO(CAM_CDM,
			"Last fetched BL by cdm from fifo: %u has Base: 0x%x, len: %d ARB: %d tag: %d ",
			(dump_reg[1] & CAM_CDM_CURRENT_BL_FIFO) >>
				CAM_CDM_CURRENT_BL_FIFO_SHIFT,
			dump_reg[0],
			(dump_reg[1] & CAM_CDM_CURRENT_BL_LEN),
			(dump_reg[1] & CAM_CDM_CURRENT_BL_ARB) >>
				CAM_CDM_CURRENT_BL_ARB_SHIFT,
			(dump_reg[1] & CAM_CDM_CURRENT_BL_TAG) >>
				CAM_CDM_CURRENT_BL_TAG_SHIFT);
	else
		CAM_INFO(CAM_CDM,
			"Last fetched BL by cdm has Base: 0x%x, len: %d tag: %d ",
			dump_reg[0],
			(dump_reg[1] & CAM_CDM_CURRENT_BL_LEN),
			(dump_reg[1] & CAM_CDM_CURRENT_BL_TAG) >>
				CAM_CDM_CURRENT_BL_TAG_SHIFT);

	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->wait_status, &dump_reg[0]);
	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->comp_wait[0]->comp_wait_status,
		&dump_reg[1]);
	cam_cdm_read_hw_reg(cdm_hw,
		core->offsets->cmn_reg->comp_wait[1]->comp_wait_status,
		&dump_reg[2]);
	CAM_INFO(CAM_CDM, "Wait status: 0x%x, Comp_wait_status0: 0x%x:, Comp_wait_status1: 0x%x",
		dump_reg[0], dump_reg[1], dump_reg[2]);

	cam_hw_cdm_disable_core_dbg(cdm_hw);
	if (pause_core)
		cam_hw_cdm_pause_core(cdm_hw, false);
}

enum cam_cdm_arbitration cam_cdm_get_arbitration_type(
		uint32_t cdm_version,
		enum cam_cdm_id id)
{
	enum cam_cdm_arbitration arbitration;

	if (cdm_version < CAM_CDM_VERSION_2_0) {
		arbitration = CAM_CDM_ARBITRATION_NONE;
		goto end;
	}
	switch (id) {
	case CAM_CDM_CPAS:
		arbitration = CAM_CDM_ARBITRATION_ROUND_ROBIN;
		break;
	default:
		arbitration = CAM_CDM_ARBITRATION_PRIORITY_BASED;
		break;
	}
end:
	return arbitration;
}

int cam_hw_cdm_set_cdm_blfifo_cfg(struct cam_hw_info *cdm_hw)
{
	int rc = 0, i;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;

	for (i = 0; i < core->offsets->reg_data->num_bl_fifo_irq; i++) {
		if (!core->bl_fifo[i].bl_depth)
			continue;

		rc = cam_cdm_write_hw_reg(cdm_hw,
			core->offsets->irq_reg[i]->irq_mask, 0x70003);
		if (rc) {
			CAM_ERR(CAM_CDM,
				"Unable to write to %s%u irq mask register",
				cdm_hw->soc_info.label_name,
				cdm_hw->soc_info.index);
			rc = -EIO;
			goto end;
		}
	}

	if (core->hw_version >= CAM_CDM_VERSION_2_0) {
		for (i = 0; i < core->offsets->reg_data->num_bl_fifo; i++) {
			rc = cam_cdm_write_hw_reg(cdm_hw,
				core->offsets->bl_fifo_reg[i]->bl_fifo_cfg,
				core->bl_fifo[i].bl_depth
				<< CAM_CDM_BL_FIFO_LENGTH_CFG_SHIFT);
			if (rc) {
				CAM_ERR(CAM_CDM,
					"Unable to write to %s%u irq mask register",
					cdm_hw->soc_info.label_name,
					cdm_hw->soc_info.index);
				rc = -EIO;
				goto end;
			}
		}
	}
end:
	return rc;
}

int cam_hw_cdm_set_cdm_core_cfg(struct cam_hw_info *cdm_hw)
{
	uint32_t cdm_version;
	uint32_t cfg_mask = 0;
	int rc;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	struct cam_cdm_private_dt_data *pvt_data =
		(struct cam_cdm_private_dt_data *)cdm_hw->soc_info.soc_private;

	cfg_mask = cfg_mask |
			CAM_CDM_AHB_STOP_ON_ERROR|
			CAM_CDM_AHB_BURST_EN|
			CAM_CDM_AHB_BURST_LEN_16;

	/* use version from cdm_core structure. */
	if (cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->cmn_reg->cdm_hw_version,
			&cdm_version)) {
		CAM_ERR(CAM_CDM, "Error reading %s%u register",
			cdm_hw->soc_info.label_name,
			cdm_hw->soc_info.index);
		rc = -EIO;
		goto end;
	}

	if (cdm_version >= CAM_CDM_VERSION_2_0) {
		if (core->id != CAM_CDM_CPAS &&
			(!pvt_data->is_single_ctx_cdm))
			cfg_mask = cfg_mask | CAM_CDM_IMPLICIT_WAIT_EN;

		if (core->arbitration == CAM_CDM_ARBITRATION_ROUND_ROBIN)
			cfg_mask = cfg_mask | CAM_CDM_ARB_SEL_RR;

	}

	if (cdm_version >= CAM_CDM_VERSION_2_1) {
		cfg_mask = cfg_mask | ((uint32_t)pvt_data->priority_group <<
			core->offsets->cmn_reg->priority_group_bit_offset);
	}

	rc = cam_cdm_write_hw_reg(cdm_hw,
			core->offsets->cmn_reg->core_cfg, cfg_mask);
	if (rc) {
		CAM_ERR(CAM_CDM, "Error writing %s%u core cfg",
			cdm_hw->soc_info.label_name,
			cdm_hw->soc_info.index);
		rc = -EIO;
		goto end;
	}

end:
	return rc;
}

int cam_hw_cdm_wait_for_bl_fifo(
		struct cam_hw_info *cdm_hw,
		uint32_t            bl_count,
		uint32_t            fifo_idx)
{
	uint32_t pending_bl = 0;
	int32_t available_bl_slots = 0;
	int rc = -EIO;
	long time_left;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	struct cam_cdm_bl_fifo *bl_fifo = NULL;

	if (fifo_idx >= CAM_CDM_BL_FIFO_MAX) {
		rc = -EINVAL;
		CAM_ERR(CAM_CDM,
			"Invalid fifo index %d rc = %d",
			fifo_idx, rc);
		goto end;
	}

	bl_fifo = &core->bl_fifo[fifo_idx];

	do {
		if (cam_hw_cdm_bl_fifo_pending_bl_rb_in_fifo(cdm_hw, fifo_idx, &pending_bl)) {
			CAM_ERR(CAM_CDM, "Failed to read CDM pending BL's");
			rc = -EIO;
			break;
		}
		available_bl_slots = bl_fifo->bl_depth - pending_bl;
		if (available_bl_slots < 0) {
			CAM_ERR(CAM_CDM, "Invalid available slots %d:%d:%d",
				available_bl_slots, bl_fifo->bl_depth,
				pending_bl);
			break;
		}
		if (0 == (available_bl_slots - 1)) {
			reinit_completion(&core->bl_fifo[fifo_idx].bl_complete);

			rc = cam_hw_cdm_enable_bl_done_irq(cdm_hw,
				true, fifo_idx);
			if (rc) {
				CAM_ERR(CAM_CDM, "Enable BL done irq failed");
				break;
			}
			time_left = cam_common_wait_for_completion_timeout(
				&core->bl_fifo[fifo_idx].bl_complete,
				msecs_to_jiffies(
				CAM_CDM_BL_FIFO_WAIT_TIMEOUT));
			if (time_left <= 0) {
				CAM_ERR(CAM_CDM,
					"CDM HW BL Wait timed out failed");
				if (cam_hw_cdm_enable_bl_done_irq(cdm_hw,
					false, fifo_idx))
					CAM_ERR(CAM_CDM,
						"Disable BL done irq failed");
				rc = -EIO;
				break;
			}
			if (cam_hw_cdm_enable_bl_done_irq(cdm_hw,
					false, fifo_idx))
				CAM_ERR(CAM_CDM, "Disable BL done irq failed");
			rc = 1;
			CAM_DBG(CAM_CDM, "CDM HW is ready for data");
		} else {
			CAM_DBG(CAM_CDM,
				"BL slot available_cnt=%d requested=%d",
				(available_bl_slots - 1), bl_count);
			rc = available_bl_slots - 1;
			break;
		}
	} while (1);

end:

	return rc;
}

bool cam_hw_cdm_bl_write(
		struct cam_hw_info *cdm_hw, uint32_t src,
		uint32_t len, uint32_t tag, bool set_arb,
		uint32_t fifo_idx)
{
	struct cam_cdm *cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	CAM_DBG(CAM_CDM, "%s%d Base: 0x%x, Len: %u, Tag: %u, set_arb: %u, fifo_idx: %u",
		cdm_hw->soc_info.label_name, cdm_hw->soc_info.index,
		src, len, tag, set_arb, fifo_idx);

	if (cam_cdm_write_hw_reg(cdm_hw,
		cdm_core->offsets->bl_fifo_reg[fifo_idx]->bl_fifo_base,
		src)) {
		CAM_ERR(CAM_CDM, "Failed to write CDM base to BL base");
		return true;
	}
	if (cam_cdm_write_hw_reg(cdm_hw,
		cdm_core->offsets->bl_fifo_reg[fifo_idx]->bl_fifo_len,
		((len & CAM_CDM_FIFO_LEN_REG_LEN_MASK) |
			((tag & CAM_CDM_FIFO_LEN_REG_TAG_MASK) << CAM_CDM_FIFO_LEN_REG_TAG_SHIFT)) |
			((set_arb) ? (1 << CAM_CDM_FIFO_LEN_REG_ARB_SHIFT) : (0)))) {
		CAM_ERR(CAM_CDM, "Failed to write CDM BL len");
		return true;
	}
	return false;
}

bool cam_hw_cdm_commit_bl_write(struct cam_hw_info *cdm_hw, uint32_t fifo_idx)
{
	struct cam_cdm *cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	if (cam_cdm_write_hw_reg(cdm_hw,
		cdm_core->offsets->bl_fifo_reg[fifo_idx]->bl_fifo_store,
		1)) {
		CAM_ERR(CAM_CDM, "Failed to write CDM commit BL");
		return true;
	}
	return false;
}

int cam_hw_cdm_submit_gen_irq(
	struct cam_hw_info *cdm_hw,
	struct cam_cdm_hw_intf_cmd_submit_bl *req,
	uint32_t fifo_idx, bool set_arb)
{
	struct cam_cdm_bl_cb_request_entry *node;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	uint32_t len;
	int rc;
	bool bit_wr_enable = false;

	if (core->bl_fifo[fifo_idx].bl_tag >
		(core->bl_fifo[fifo_idx].bl_depth - 1)) {
		CAM_ERR(CAM_CDM,
			"Invalid bl_tag=%d bl_depth=%d fifo_idx=%d",
			core->bl_fifo[fifo_idx].bl_tag,
			core->bl_fifo[fifo_idx].bl_depth,
			fifo_idx);
		rc = -EINVAL;
		goto end;
	}
	CAM_DBG(CAM_CDM, "CDM write BL last cmd tag=%x total=%d cookie=%d",
		core->bl_fifo[fifo_idx].bl_tag,
		req->data->cmd_arrary_count,
		req->data->cookie);

	node = kzalloc(sizeof(struct cam_cdm_bl_cb_request_entry),
			GFP_KERNEL);
	if (!node) {
		rc = -ENOMEM;
		goto end;
	}

	if (core->offsets->reg_data->num_bl_fifo > 1)
		bit_wr_enable = true;

	node->request_type = CAM_HW_CDM_BL_CB_CLIENT;
	node->client_hdl = req->handle;
	node->cookie = req->data->cookie;
	node->bl_tag = core->bl_fifo[fifo_idx].bl_tag;
	node->userdata = req->data->userdata;
	list_add_tail(&node->entry, &core->bl_fifo[fifo_idx].bl_request_list);
	len = core->ops->cdm_required_size_genirq() *
		core->bl_fifo[fifo_idx].bl_tag;
	core->ops->cdm_write_genirq(
		((uint32_t *)core->gen_irq[fifo_idx].kmdvaddr + len),
		core->bl_fifo[fifo_idx].bl_tag,
		bit_wr_enable, fifo_idx);
	rc = cam_hw_cdm_bl_write(cdm_hw,
		(core->gen_irq[fifo_idx].vaddr + (4*len)),
		((4 * core->ops->cdm_required_size_genirq()) - 1),
		core->bl_fifo[fifo_idx].bl_tag,
		set_arb, fifo_idx);
	if (rc) {
		CAM_ERR(CAM_CDM, "CDM hw bl write failed for gen irq bltag=%d",
			core->bl_fifo[fifo_idx].bl_tag);
		list_del_init(&node->entry);
		kfree(node);
		node = NULL;
		rc = -EIO;
		goto end;
	}

	if (cam_presil_mode_enabled()) {
		CAM_DBG(CAM_PRESIL,
			"Sending CDM gen irq cmd buffer:%d with iommu_hdl:%d",
			core->gen_irq[fifo_idx].handle, core->iommu_hdl.non_secure);

		rc = cam_mem_mgr_send_buffer_to_presil(core->iommu_hdl.non_secure,
			core->gen_irq[fifo_idx].handle);
		if (rc) {
			CAM_ERR(CAM_PRESIL,
				"Failed to send CDM gen irq cmd buffer fifo_idx:%d mem_handle:%d rc:%d",
				fifo_idx, core->gen_irq[fifo_idx].handle, rc);
			goto end;
		}
	}

	if (cam_hw_cdm_commit_bl_write(cdm_hw, fifo_idx)) {
		CAM_ERR(CAM_CDM,
			"Cannot commit the genirq BL with tag tag=%d",
			core->bl_fifo[fifo_idx].bl_tag);
		list_del_init(&node->entry);
		kfree(node);
		node = NULL;
		rc = -EIO;
	}

	trace_cam_log_event("CDM_START", "CDM_START_IRQ", req->data->cookie, 0);

end:
	return rc;
}

int cam_hw_cdm_submit_debug_gen_irq(
	struct cam_hw_info *cdm_hw,
	uint32_t            fifo_idx)
{
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	uint32_t len;
	int rc;
	bool bit_wr_enable = false;

	CAM_DBG(CAM_CDM,
		"CDM write BL last cmd tag=0x%x",
		core->bl_fifo[fifo_idx].bl_tag);

	if (core->offsets->reg_data->num_bl_fifo > 1)
		bit_wr_enable = true;

	len = core->ops->cdm_required_size_genirq() *
		core->bl_fifo[fifo_idx].bl_tag;
	core->ops->cdm_write_genirq(
		((uint32_t *)core->gen_irq[fifo_idx].kmdvaddr + len),
		CAM_CDM_DBG_GEN_IRQ_USR_DATA, bit_wr_enable, fifo_idx);
	rc = cam_hw_cdm_bl_write(cdm_hw,
		(core->gen_irq[fifo_idx].vaddr + (4*len)),
		((4 * core->ops->cdm_required_size_genirq()) - 1),
		core->bl_fifo[fifo_idx].bl_tag,
		false, fifo_idx);
	if (rc) {
		CAM_ERR(CAM_CDM,
			"CDM hw bl write failed for dbggenirq USRdata=%d tag 0x%x",
			CAM_CDM_DBG_GEN_IRQ_USR_DATA,
			core->bl_fifo[fifo_idx].bl_tag);
		rc = -EIO;
		goto end;
	}
	if (cam_hw_cdm_commit_bl_write(cdm_hw, fifo_idx)) {
		CAM_ERR(CAM_CDM,
			"Cannot commit the dbggenirq BL with tag tag=0x%x",
			core->bl_fifo[fifo_idx].bl_tag);
		rc = -EIO;
		goto end;
	}

end:
	return rc;
}

int cam_hw_cdm_submit_bl(struct cam_hw_info *cdm_hw,
	struct cam_cdm_hw_intf_cmd_submit_bl *req,
	struct cam_cdm_client *client)
{
	unsigned int i;
	int rc = 0;
	struct cam_cdm_bl_request *cdm_cmd = req->data;
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	struct cam_cdm_bl_fifo *bl_fifo = NULL;
	uint32_t fifo_idx = 0;
	int write_count = 0;

	fifo_idx = CAM_CDM_GET_BLFIFO_IDX(client->handle);

	CAM_DBG(CAM_CDM, "Submit bl to %s%u", cdm_hw->soc_info.label_name,
		cdm_hw->soc_info.index);
	if (fifo_idx >= CAM_CDM_BL_FIFO_MAX) {
		rc = -EINVAL;
		CAM_ERR(CAM_CDM, "Invalid handle 0x%x, rc = %d",
			client->handle, rc);
		goto end;
	}

	bl_fifo = &core->bl_fifo[fifo_idx];

	if (req->data->cmd_arrary_count > bl_fifo->bl_depth) {
		CAM_INFO(CAM_CDM,
			"requested BL more than max size, cnt=%d max=%d",
			req->data->cmd_arrary_count,
			bl_fifo->bl_depth);
	}

	mutex_lock(&core->bl_fifo[fifo_idx].fifo_lock);
	mutex_lock(&client->lock);

	if (test_bit(CAM_CDM_ERROR_HW_STATUS, &core->cdm_status) ||
			test_bit(CAM_CDM_RESET_HW_STATUS, &core->cdm_status)) {
		mutex_unlock(&client->lock);
		mutex_unlock(&core->bl_fifo[fifo_idx].fifo_lock);
		return -EAGAIN;
	}

	for (i = 0; i < req->data->cmd_arrary_count ; i++) {
		dma_addr_t hw_vaddr_ptr = 0;
		size_t len = 0;

		if ((!cdm_cmd->cmd[i].len) || (cdm_cmd->cmd[i].len > CAM_CDM_MAX_BL_LENGTH)) {
			CAM_ERR(CAM_CDM,
				"cmd len=: %d is invalid_ent: %d, num_cmd_ent: %d",
				cdm_cmd->cmd[i].len, i,
				req->data->cmd_arrary_count);
			rc = -EINVAL;
			break;
		}
		if (test_bit(CAM_CDM_ERROR_HW_STATUS, &core->cdm_status) ||
				test_bit(CAM_CDM_RESET_HW_STATUS,
					&core->cdm_status)) {
			CAM_ERR_RATE_LIMIT(CAM_CDM,
				"In error/reset state cnt=%d total cnt=%d cdm_status 0x%x",
				i, req->data->cmd_arrary_count,
				core->cdm_status);
			rc = -EAGAIN;
			break;
		}
		if (write_count == 0) {
			write_count = cam_hw_cdm_wait_for_bl_fifo(cdm_hw,
				(req->data->cmd_arrary_count - i), fifo_idx);
			if (write_count < 0) {
				CAM_ERR(CAM_CDM,
					"wait for bl fifo failed for ent: %u", i);
				rc = -EIO;
				break;
			}
		}

		if (req->data->type == CAM_CDM_BL_CMD_TYPE_MEM_HANDLE) {
			rc = cam_mem_get_io_buf(cdm_cmd->cmd[i].bl_addr.mem_handle,
				core->iommu_hdl.non_secure, &hw_vaddr_ptr,
				&len, NULL);
			if (rc) {
				CAM_ERR(CAM_CDM,
					"Getting a hwva from mem_hdl failed. rc: %d, cmd_ent: %u",
					rc, i);
				rc = -EINVAL;
				break;
			}
		} else if (req->data->type == CAM_CDM_BL_CMD_TYPE_HW_IOVA) {
			if (!cdm_cmd->cmd[i].bl_addr.hw_iova) {
				CAM_ERR(CAM_CDM, "hw_iova is null for ent: %d", i);
				rc = -EINVAL;
				break;
			}

			rc = 0;
			hw_vaddr_ptr = (dma_addr_t)cdm_cmd->cmd[i].bl_addr.hw_iova;
			len = cdm_cmd->cmd[i].len + cdm_cmd->cmd[i].offset;
		} else {
			CAM_ERR(CAM_CDM,
				"Only mem hdl/hw va type is supported %d",
				req->data->type);
			rc = -EINVAL;
			break;
		}

		if ((hw_vaddr_ptr) && (len) && (len >= cdm_cmd->cmd[i].offset)) {
			if ((len - cdm_cmd->cmd[i].offset) < cdm_cmd->cmd[i].len) {
				CAM_ERR(CAM_CDM,
					"Not enough buffer cmd offset: %u cmd length: %u",
					cdm_cmd->cmd[i].offset,
					cdm_cmd->cmd[i].len);
				rc = -EINVAL;
				break;
			}

			CAM_DBG(CAM_CDM, "Got the hwva: %pK, type: %u",
				hw_vaddr_ptr, req->data->type);

			rc = cam_hw_cdm_bl_write(cdm_hw,
				((uint32_t)hw_vaddr_ptr + cdm_cmd->cmd[i].offset),
				(cdm_cmd->cmd[i].len - 1),
				core->bl_fifo[fifo_idx].bl_tag,
				cdm_cmd->cmd[i].arbitrate,
				fifo_idx);
			if (rc) {
				CAM_ERR(CAM_CDM, "Hw bl write failed %d:%d",
					i, req->data->cmd_arrary_count);
				rc = -EIO;
				break;
			}

			if (cam_hw_cdm_commit_bl_write(cdm_hw, fifo_idx)) {
				CAM_ERR(CAM_CDM, "Commit failed for BL: %d Tag: %u",
					i, core->bl_fifo[fifo_idx].bl_tag);
				rc = -EIO;
				break;
			}

			CAM_DBG(CAM_CDM, "Commit success for BL: %d of %d, Tag: %u", (i + 1),
				req->data->cmd_arrary_count,
				core->bl_fifo[fifo_idx].bl_tag);

			write_count--;
			core->bl_fifo[fifo_idx].bl_tag++;
			core->bl_fifo[fifo_idx].bl_tag %= (bl_fifo->bl_depth - 1);

			if (cdm_cmd->cmd[i].enable_debug_gen_irq) {
				if (write_count == 0) {
					write_count =
						cam_hw_cdm_wait_for_bl_fifo(cdm_hw, 1, fifo_idx);
					if (write_count < 0) {
						CAM_ERR(CAM_CDM, "wait for bl fifo failed %d:%d",
							i, req->data->cmd_arrary_count);
						rc = -EIO;
						break;
					}
				}

				rc = cam_hw_cdm_submit_debug_gen_irq(cdm_hw, fifo_idx);
				if (!rc) {
					CAM_DBG(CAM_CDM,
						"Commit success for Dbg_GenIRQ_BL, Tag: %d",
						core->bl_fifo[fifo_idx].bl_tag);
					write_count--;
					core->bl_fifo[fifo_idx].bl_tag++;
					core->bl_fifo[fifo_idx].bl_tag %= (bl_fifo->bl_depth - 1);
				} else {
					CAM_WARN(CAM_CDM,
						"Failed in submitting the debug gen entry. rc: %d",
						rc);
					continue;
				}
			}

			if (req->data->flag && (i == (req->data->cmd_arrary_count - 1))) {

				if (write_count == 0) {
					write_count =
						cam_hw_cdm_wait_for_bl_fifo(cdm_hw, 1, fifo_idx);
					if (write_count < 0) {
						CAM_ERR(CAM_CDM, "wait for bl fifo failed %d:%d",
							i, req->data->cmd_arrary_count);
						rc = -EIO;
						break;
					}
				}

				if (core->arbitration == CAM_CDM_ARBITRATION_PRIORITY_BASED)
					cdm_cmd->gen_irq_arb = true;
				else
					cdm_cmd->gen_irq_arb = false;

				rc = cam_hw_cdm_submit_gen_irq(cdm_hw, req, fifo_idx,
					cdm_cmd->gen_irq_arb);
				if (!rc) {
					CAM_DBG(CAM_CDM, "Commit success for GenIRQ_BL, Tag: %d",
						core->bl_fifo[fifo_idx].bl_tag);
					core->bl_fifo[fifo_idx].bl_tag++;
					core->bl_fifo[fifo_idx].bl_tag %= (bl_fifo->bl_depth - 1);
				}
			}
		} else {
			CAM_ERR(CAM_CDM,
				"Sanity check failed for cdm_cmd: %d, Hdl: 0x%x, len: %zu, offset: 0x%x, num_cmds: %d",
				i, cdm_cmd->cmd[i].bl_addr.mem_handle, len, cdm_cmd->cmd[i].offset,
				req->data->cmd_arrary_count);
			rc = -EINVAL;
			break;
		}
	}
	mutex_unlock(&client->lock);
	mutex_unlock(&core->bl_fifo[fifo_idx].fifo_lock);

end:
	return rc;

}

static void cam_hw_cdm_reset_cleanup(
	struct cam_hw_info *cdm_hw,
	uint32_t            handle)
{
	struct cam_cdm *core = (struct cam_cdm *)cdm_hw->core_info;
	int i;
	struct cam_cdm_bl_cb_request_entry *node, *tnode;
	bool flush_hw = false;
	bool reset_err = false;

	if (test_bit(CAM_CDM_ERROR_HW_STATUS, &core->cdm_status) ||
		test_bit(CAM_CDM_FLUSH_HW_STATUS, &core->cdm_status))
		flush_hw = true;

	if (test_bit(CAM_CDM_RESET_ERR_STATUS, &core->cdm_status))
		reset_err = true;

	for (i = 0; i < core->offsets->reg_data->num_bl_fifo; i++) {
		list_for_each_entry_safe(node, tnode,
			&core->bl_fifo[i].bl_request_list, entry) {
			if (node->request_type ==
					CAM_HW_CDM_BL_CB_CLIENT) {
				CAM_DBG(CAM_CDM,
					"Notifying client %d for tag %d",
					node->client_hdl, node->bl_tag);
				if (flush_hw) {
					enum cam_cdm_cb_status status;

					status = reset_err ?
						CAM_CDM_CB_STATUS_HW_ERROR :
						CAM_CDM_CB_STATUS_HW_RESUBMIT;

					cam_cdm_notify_clients(cdm_hw,
						(node->client_hdl == handle) ?
						CAM_CDM_CB_STATUS_HW_FLUSH :
						status,
						(void *)node);
				}
				else
					cam_cdm_notify_clients(cdm_hw,
						CAM_CDM_CB_STATUS_HW_RESET_DONE,
						(void *)node);
			}
			list_del_init(&node->entry);
			kfree(node);
			node = NULL;
		}
		core->bl_fifo[i].bl_tag = 0;
		core->bl_fifo[i].last_bl_tag_done = -1;
		atomic_set(&core->bl_fifo[i].work_record, 0);
	}
}

static void cam_hw_cdm_work(struct work_struct *work)
{
	struct cam_cdm_work_payload *payload;
	struct cam_hw_info *cdm_hw;
	struct cam_cdm *core;
	int i, fifo_idx;
	struct cam_cdm_bl_cb_request_entry *tnode = NULL;
	struct cam_cdm_bl_cb_request_entry *node = NULL;

	payload = container_of(work, struct cam_cdm_work_payload, work);
	if (!payload) {
		CAM_ERR(CAM_CDM, "NULL payload");
		return;
	}

	cdm_hw = payload->hw;
	core = (struct cam_cdm *)cdm_hw->core_info;
	fifo_idx = payload->fifo_idx;
	if ((fifo_idx >= core->offsets->reg_data->num_bl_fifo) ||
		(!core->bl_fifo[fifo_idx].bl_depth)) {
		CAM_ERR(CAM_CDM, "Invalid fifo idx %d",
			fifo_idx);
		kfree(payload);
		payload = NULL;
		return;
	}

	cam_common_util_thread_switch_delay_detect(
		"CDM workq schedule",
		payload->workq_scheduled_ts,
		CAM_WORKQ_SCHEDULE_TIME_THRESHOLD);

	CAM_DBG(CAM_CDM, "IRQ status=0x%x", payload->irq_status);
	if (payload->irq_status &
		CAM_CDM_IRQ_STATUS_INLINE_IRQ_MASK) {
		CAM_DBG(CAM_CDM, "inline IRQ data=0x%x last tag: 0x%x",
			payload->irq_data,
			core->bl_fifo[payload->fifo_idx]
				.last_bl_tag_done);

		if (payload->irq_data == 0xff) {
			CAM_INFO(CAM_CDM, "%s%u Debug genirq received",
				cdm_hw->soc_info.label_name,
				cdm_hw->soc_info.index);
			kfree(payload);
			payload = NULL;
			return;
		}

		mutex_lock(&cdm_hw->hw_mutex);
		mutex_lock(&core->bl_fifo[fifo_idx].fifo_lock);

		if (atomic_read(&core->bl_fifo[fifo_idx].work_record))
			atomic_dec(&core->bl_fifo[fifo_idx].work_record);

		if (list_empty(&core->bl_fifo[fifo_idx]
				.bl_request_list)) {
			CAM_INFO(CAM_CDM,
				"Fifo list empty, idx %d tag %d arb %d",
				fifo_idx, payload->irq_data,
				core->arbitration);
			mutex_unlock(&core->bl_fifo[fifo_idx]
					.fifo_lock);
			mutex_unlock(&cdm_hw->hw_mutex);
			return;
		}

		if (core->bl_fifo[fifo_idx].last_bl_tag_done !=
			payload->irq_data) {
			core->bl_fifo[fifo_idx].last_bl_tag_done =
				payload->irq_data;
			list_for_each_entry_safe(node, tnode,
				&core->bl_fifo[fifo_idx].bl_request_list,
				entry) {
				if (node->request_type ==
					CAM_HW_CDM_BL_CB_CLIENT) {
					cam_cdm_notify_clients(cdm_hw,
					CAM_CDM_CB_STATUS_BL_SUCCESS,
					(void *)node);
				} else if (node->request_type ==
					CAM_HW_CDM_BL_CB_INTERNAL) {
					CAM_ERR(CAM_CDM,
						"Invalid node=%pK %d",
						node,
						node->request_type);
				}
				list_del_init(&node->entry);
				if (node->bl_tag == payload->irq_data) {
					kfree(node);
					node = NULL;
					break;
				}
				kfree(node);
				node = NULL;
			}
		} else {
			CAM_INFO(CAM_CDM,
				"Skip GenIRQ, tag 0x%x fifo %d",
				payload->irq_data, payload->fifo_idx);
		}
		mutex_unlock(&core->bl_fifo[payload->fifo_idx]
			.fifo_lock);
		mutex_unlock(&cdm_hw->hw_mutex);
	}

	if (payload->irq_status &
		CAM_CDM_IRQ_STATUS_BL_DONE_MASK) {
		if (test_bit(payload->fifo_idx, &core->cdm_status)) {
			CAM_DBG(CAM_CDM, "%s%u HW BL done IRQ",
				cdm_hw->soc_info.label_name,
				cdm_hw->soc_info.index);
			complete(&core->bl_fifo[payload->fifo_idx]
				.bl_complete);
		}
	}
	if (payload->irq_status &
		CAM_CDM_IRQ_STATUS_ERRORS) {
		int reset_hw_hdl = 0x0;

		CAM_ERR_RATE_LIMIT(CAM_CDM,
			"%s%u Error IRQ status %d\n",
			cdm_hw->soc_info.label_name,
			cdm_hw->soc_info.index, payload->irq_status);
		set_bit(CAM_CDM_ERROR_HW_STATUS, &core->cdm_status);
		mutex_lock(&cdm_hw->hw_mutex);
		for (i = 0; i < core->offsets->reg_data->num_bl_fifo; i++)
			mutex_lock(&core->bl_fifo[i].fifo_lock);

		cam_hw_cdm_dump_core_debug_registers(cdm_hw, true);

		if (payload->irq_status &
		CAM_CDM_IRQ_STATUS_ERROR_INV_CMD_MASK) {
			node = list_first_entry_or_null(
			&core->bl_fifo[payload->fifo_idx].bl_request_list,
			struct cam_cdm_bl_cb_request_entry, entry);

			if (node != NULL) {
				if (node->request_type ==
					CAM_HW_CDM_BL_CB_CLIENT) {
					cam_cdm_notify_clients(cdm_hw,
					CAM_CDM_CB_STATUS_INVALID_BL_CMD,
						(void *)node);
				} else if (node->request_type ==
					CAM_HW_CDM_BL_CB_INTERNAL) {
					CAM_ERR(CAM_CDM,
						"Invalid node=%pK %d", node,
						node->request_type);
				}
				list_del_init(&node->entry);
				kfree(node);
			}
		}

		for (i = 0; i < core->offsets->reg_data->num_bl_fifo; i++)
			mutex_unlock(&core->bl_fifo[i].fifo_lock);

		if (payload->irq_status &
			CAM_CDM_IRQ_STATUS_ERROR_INV_CMD_MASK)
			cam_hw_cdm_reset_hw(cdm_hw, reset_hw_hdl);

		mutex_unlock(&cdm_hw->hw_mutex);
		if (!(payload->irq_status &
				CAM_CDM_IRQ_STATUS_ERROR_INV_CMD_MASK))
			clear_bit(CAM_CDM_ERROR_HW_STATUS,
				&core->cdm_status);
	}
	kfree(payload);
	payload = NULL;

}

static void cam_hw_cdm_iommu_fault_handler(struct cam_smmu_pf_info *pf_info)
{
	struct cam_hw_info *cdm_hw = NULL;
	struct cam_cdm *core = NULL;
	struct cam_cdm_private_dt_data *pvt_data;
	int i;

	if (!pf_info) {
		CAM_ERR(CAM_CDM, "pf_info is null");
		return;
	}

	if (pf_info->token) {
		cdm_hw = (struct cam_hw_info *)pf_info->token;
		core = (struct cam_cdm *)cdm_hw->core_info;
		pvt_data = (struct cam_cdm_private_dt_data *) cdm_hw->soc_info.soc_private;
		CAM_ERR_RATE_LIMIT(CAM_CDM, "Page fault iova addr %pK\n",
			(void *)pf_info->iova);

		/* Check if the PID and MID are valid, if not handle the pf */
		if ((pvt_data->pid >= 0) && (pvt_data->mid >= 0)) {
			if (((pf_info->pid == pvt_data->pid) && (pf_info->mid == pvt_data->mid)))
				goto handle_cdm_pf;
			else
				return;
		}

handle_cdm_pf:
		set_bit(CAM_CDM_ERROR_HW_STATUS, &core->cdm_status);
		mutex_lock(&cdm_hw->hw_mutex);
		for (i = 0; i < core->offsets->reg_data->num_bl_fifo; i++)
			mutex_lock(&core->bl_fifo[i].fifo_lock);
		if (cdm_hw->hw_state == CAM_HW_STATE_POWER_UP) {
			cam_hw_cdm_dump_core_debug_registers(cdm_hw, true);
		} else
			CAM_INFO(CAM_CDM, "%s%u hw is power in off state",
				cdm_hw->soc_info.label_name,
				cdm_hw->soc_info.index);
		for (i = 0; i < core->offsets->reg_data->num_bl_fifo; i++)
			mutex_unlock(&core->bl_fifo[i].fifo_lock);
		cam_cdm_notify_clients(cdm_hw, CAM_CDM_CB_STATUS_PAGEFAULT,
			(void *)pf_info->iova);
		mutex_unlock(&cdm_hw->hw_mutex);
		clear_bit(CAM_CDM_ERROR_HW_STATUS, &core->cdm_status);
	} else {
		CAM_ERR(CAM_CDM, "Invalid token");
	}
}

irqreturn_t cam_hw_cdm_irq(int irq_num, void *data)
{
	struct cam_hw_info *cdm_hw = data;
	struct cam_hw_soc_info *soc_info = &cdm_hw->soc_info;
	struct cam_cdm *cdm_core = cdm_hw->core_info;
	struct cam_cdm_work_payload *payload[CAM_CDM_BL_FIFO_MAX] = {0};
	uint8_t rst_done_cnt = 0;
	uint32_t user_data = 0;
	uint32_t irq_status[CAM_CDM_BL_FIFO_MAX] = {0};
	uint32_t irq_context_summary = 0xF;
	bool work_status;
	int i;

	CAM_DBG(CAM_CDM, "Got irq hw_version 0x%x from %s%u",
		cdm_core->hw_version, soc_info->label_name,
		soc_info->index);
	cam_hw_util_hw_lock(cdm_hw);
	if (cdm_hw->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_DBG(CAM_CDM, "CDM is in power down state");
		cam_hw_util_hw_unlock(cdm_hw);
		return IRQ_HANDLED;
	}
	if (cdm_core->hw_version >= CAM_CDM_VERSION_2_1) {
		if (cam_cdm_read_hw_reg(cdm_hw,
			cdm_core->offsets->cmn_reg->irq_context_status,
			&irq_context_summary)) {
			CAM_ERR(CAM_CDM, "Failed to read CDM HW IRQ status");
		}
	}
	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo_irq; i++) {
		if (!(BIT(i) & irq_context_summary)) {
			continue;
		}
		if (cam_cdm_read_hw_reg(cdm_hw,
			cdm_core->offsets->irq_reg[i]->irq_status,
			&irq_status[i])) {
			CAM_ERR(CAM_CDM, "Failed to read %s%u HW IRQ status",
				soc_info->label_name,
				soc_info->index);
		}
		if (cam_cdm_write_hw_reg(cdm_hw,
			cdm_core->offsets->irq_reg[i]->irq_clear,
			irq_status[i])) {
			CAM_ERR(CAM_CDM, "Failed to Write %s%u HW IRQ Clear",
				soc_info->label_name,
				soc_info->index);
		}
	}

	if (cam_cdm_write_hw_reg(cdm_hw,
		cdm_core->offsets->irq_reg[0]->irq_clear_cmd, 0x01))
		CAM_ERR(CAM_CDM, "Failed to Write %s%u HW IRQ clr cmd",
				soc_info->label_name,
				soc_info->index);
	if (cam_cdm_read_hw_reg(cdm_hw,
			cdm_core->offsets->cmn_reg->usr_data,
			&user_data))
		CAM_ERR(CAM_CDM, "Failed to read %s%u HW IRQ data",
				soc_info->label_name,
				soc_info->index);

	cam_hw_util_hw_unlock(cdm_hw);

	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo_irq; i++) {
		if (!irq_status[i])
			continue;

		if (irq_status[i] & CAM_CDM_IRQ_STATUS_RST_DONE_MASK) {
			rst_done_cnt++;
			continue;
		}

		payload[i] = kzalloc(sizeof(struct cam_cdm_work_payload),
			GFP_ATOMIC);

		if (!payload[i]) {
			CAM_ERR(CAM_CDM,
				"failed to allocate memory for fifo %d payload",
				i);
			continue;
		}

		if (irq_status[i] & CAM_CDM_IRQ_STATUS_INLINE_IRQ_MASK) {

			payload[i]->irq_data = (user_data >> (i * 0x8)) &
				CAM_CDM_IRQ_STATUS_USR_DATA_MASK;

			if (payload[i]->irq_data ==
				CAM_CDM_DBG_GEN_IRQ_USR_DATA)
				CAM_INFO(CAM_CDM, "Debug gen_irq received");

			atomic_inc(&cdm_core->bl_fifo[i].work_record);
		}

		CAM_DBG(CAM_CDM,
			"Rcvd of fifo %d userdata 0x%x tag 0x%x irq_stat 0x%x",
			i, user_data, payload[i]->irq_data, irq_status[i]);

		payload[i]->fifo_idx = i;
		payload[i]->irq_status = irq_status[i];
		payload[i]->hw = cdm_hw;

		INIT_WORK((struct work_struct *)&payload[i]->work,
			cam_hw_cdm_work);

		trace_cam_log_event("CDM_DONE", "CDM_DONE_IRQ",
			payload[i]->irq_status,
			cdm_hw->soc_info.index);
		if (cam_cdm_write_hw_reg(cdm_hw,
				cdm_core->offsets->irq_reg[i]->irq_clear,
				payload[i]->irq_status)) {
			CAM_ERR(CAM_CDM, "Failed to Write %s%u HW IRQ Clear",
				soc_info->label_name,
				soc_info->index);
			kfree(payload[i]);
			return IRQ_HANDLED;
		}

		payload[i]->workq_scheduled_ts = ktime_get();

		work_status = queue_work(
			cdm_core->bl_fifo[i].work_queue,
			&payload[i]->work);

		if (work_status == false) {
			CAM_ERR(CAM_CDM,
				"Failed to queue work for FIFO: %d irq=0x%x",
				i, payload[i]->irq_status);
			kfree(payload[i]);
			payload[i] = NULL;
		}
	}
	if (rst_done_cnt == cdm_core->offsets->reg_data->num_bl_fifo_irq) {
		CAM_DBG(CAM_CDM, "%s%u HW reset done IRQ",
			soc_info->label_name,
			soc_info->index);
		complete(&cdm_core->reset_complete);
	}
	if (rst_done_cnt &&
		(rst_done_cnt != cdm_core->offsets->reg_data->num_bl_fifo_irq))
		CAM_INFO(CAM_CDM,
			"%s%u Reset IRQ received for %d fifos instead of %d",
			soc_info->label_name,
			soc_info->index, rst_done_cnt,
			cdm_core->offsets->reg_data->num_bl_fifo_irq);
	return IRQ_HANDLED;
}

int cam_hw_cdm_alloc_genirq_mem(void *hw_priv)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_mem_mgr_request_desc genirq_alloc_cmd;
	struct cam_mem_mgr_memory_desc genirq_alloc_out;
	struct cam_cdm *cdm_core = NULL;
	int rc = -EINVAL, i;

	if (!hw_priv)
		return rc;

	cdm_core = (struct cam_cdm *)cdm_hw->core_info;
	genirq_alloc_cmd.align = 0;
	genirq_alloc_cmd.smmu_hdl = cdm_core->iommu_hdl.non_secure;
	genirq_alloc_cmd.flags = CAM_MEM_FLAG_HW_READ_WRITE;
	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++) {
		if (!cdm_core->bl_fifo[i].bl_depth)
			continue;

		genirq_alloc_cmd.size = (8 *
				cdm_core->bl_fifo[i].bl_depth);
		rc = cam_mem_mgr_request_mem(&genirq_alloc_cmd,
				&genirq_alloc_out);
		if (rc) {
			CAM_ERR(CAM_CDM,
				"Failed to get genirq cmd space rc=%d",
				rc);
			goto end;
		}
		cdm_core->gen_irq[i].handle = genirq_alloc_out.mem_handle;
		cdm_core->gen_irq[i].vaddr = (genirq_alloc_out.iova &
			0xFFFFFFFF);
		cdm_core->gen_irq[i].kmdvaddr = genirq_alloc_out.kva;
		cdm_core->gen_irq[i].size = genirq_alloc_out.len;
	}
end:
	return rc;
}

int cam_hw_cdm_release_genirq_mem(void *hw_priv)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_cdm *cdm_core = NULL;
	struct cam_mem_mgr_memory_desc genirq_release_cmd;
	int rc = -EINVAL, i;

	if (!hw_priv)
		return rc;

	cdm_core = (struct cam_cdm *)cdm_hw->core_info;
	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++) {
		if (!cdm_core->bl_fifo[i].bl_depth)
			continue;

		genirq_release_cmd.mem_handle = cdm_core->gen_irq[i].handle;
		rc = cam_mem_mgr_release_mem(&genirq_release_cmd);
		if (rc)
			CAM_ERR(CAM_CDM,
				"Failed to put genirq cmd space for hw rc %d",
				rc);
	}

	return rc;
}

int cam_hw_cdm_reset_hw(struct cam_hw_info *cdm_hw, uint32_t handle)
{
	struct cam_cdm *cdm_core = NULL;
	struct cam_hw_soc_info *soc_info = &cdm_hw->soc_info;
	long time_left;
	int i, rc = -EIO;
	int reset_val = 1;

	cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++)
		mutex_lock(&cdm_core->bl_fifo[i].fifo_lock);

	set_bit(CAM_CDM_RESET_HW_STATUS, &cdm_core->cdm_status);

	reinit_completion(&cdm_core->reset_complete);

	/* First pause CDM, If it fails still proceed to reset CDM HW */
	cam_hw_cdm_pause_core(cdm_hw, true);
	usleep_range(1000, 1010);

	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++) {
		if (!cdm_core->bl_fifo[i].bl_depth)
			continue;

		reset_val = reset_val |
			(1 << (i + CAM_CDM_BL_FIFO_FLUSH_SHIFT));
		if (cam_cdm_write_hw_reg(cdm_hw,
				cdm_core->offsets->irq_reg[i]->irq_mask,
				0x70003)) {
			CAM_ERR(CAM_CDM, "Failed to Write %s%u HW IRQ mask",
				soc_info->label_name,
				soc_info->index);
			goto end;
		}
	}

	if (cam_cdm_write_hw_reg(cdm_hw,
			cdm_core->offsets->cmn_reg->rst_cmd, reset_val)) {
		CAM_ERR(CAM_CDM, "Failed to Write %s%u HW reset",
			soc_info->label_name,
			soc_info->index);
		goto end;
	}

	CAM_DBG(CAM_CDM, "Waiting for %s%u HW reset done",
		soc_info->label_name, soc_info->index);
	time_left = cam_common_wait_for_completion_timeout(
			&cdm_core->reset_complete,
			msecs_to_jiffies(CAM_CDM_HW_RESET_TIMEOUT));

	if (time_left <= 0) {
		rc = -ETIMEDOUT;
		CAM_ERR(CAM_CDM, "%s%u HW reset Wait failed rc=%d",
			soc_info->label_name,
			soc_info->index, rc);
		goto end;
	}

	rc = cam_hw_cdm_set_cdm_core_cfg(cdm_hw);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to configure %s%u rc=%d",
			soc_info->label_name,
			soc_info->index, rc);
		goto end;
	}

	rc = cam_hw_cdm_set_cdm_blfifo_cfg(cdm_hw);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to configure %s%u fifo rc=%d",
			soc_info->label_name,
			soc_info->index, rc);
		goto end;
	}

	cam_hw_cdm_reset_cleanup(cdm_hw, handle);
end:
	clear_bit(CAM_CDM_RESET_HW_STATUS, &cdm_core->cdm_status);
	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++)
		mutex_unlock(&cdm_core->bl_fifo[i].fifo_lock);

	return rc;
}

int cam_hw_cdm_handle_error_info(
	struct cam_hw_info *cdm_hw,
	uint32_t            handle)
{
	struct cam_cdm *cdm_core = NULL;
	struct cam_cdm_bl_cb_request_entry *node = NULL;
	long time_left;
	int i, rc = -EIO, reset_hw_hdl = 0x0;
	uint32_t current_bl_data = 0, current_fifo = 0, current_tag = 0;
	int reset_val = 1;
	struct cam_hw_soc_info *soc_info = &cdm_hw->soc_info;

	cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++)
		mutex_lock(&cdm_core->bl_fifo[i].fifo_lock);

	reinit_completion(&cdm_core->reset_complete);
	set_bit(CAM_CDM_RESET_HW_STATUS, &cdm_core->cdm_status);
	set_bit(CAM_CDM_FLUSH_HW_STATUS, &cdm_core->cdm_status);

	if (cdm_hw->hw_state == CAM_HW_STATE_POWER_DOWN) {
		CAM_WARN(CAM_CDM, "CDM is in power down state");
		goto end;
	}

	/* First pause CDM, If it fails still proceed to dump debug info */
	cam_hw_cdm_pause_core(cdm_hw, true);

	rc = cam_cdm_read_hw_reg(cdm_hw,
			cdm_core->offsets->cmn_reg->current_bl_len,
			&current_bl_data);

	current_fifo = ((CAM_CDM_CURRENT_BL_FIFO & current_bl_data)
		>> CAM_CDM_CURRENT_BL_FIFO_SHIFT);
	current_tag = ((CAM_CDM_CURRENT_BL_TAG & current_bl_data)
		>> CAM_CDM_CURRENT_BL_TAG_SHIFT);

	if (current_fifo >= CAM_CDM_BL_FIFO_MAX) {
		rc = -EFAULT;
		goto end;
	}

	CAM_ERR(CAM_CDM, "Hang detected for %s%u's fifo %d with tag 0x%x",
		soc_info->label_name, soc_info->index,
		current_fifo, current_tag);

	/* dump cdm registers for further debug */
	cam_hw_cdm_dump_core_debug_registers(cdm_hw, false);

	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++) {
		if (!cdm_core->bl_fifo[i].bl_depth)
			continue;

		reset_val = reset_val |
			(1 << (i + CAM_CDM_BL_FIFO_FLUSH_SHIFT));
		if (cam_cdm_write_hw_reg(cdm_hw,
				cdm_core->offsets->irq_reg[i]->irq_mask,
				0x70003)) {
			CAM_ERR(CAM_CDM, "Failed to Write CDM HW IRQ mask");
			goto end;
		}
	}

	if (cam_cdm_write_hw_reg(cdm_hw,
			cdm_core->offsets->cmn_reg->rst_cmd, reset_val)) {
		CAM_ERR(CAM_CDM, "Failed to Write CDM HW reset");
		goto end;
	}

	CAM_DBG(CAM_CDM, "Waiting for CDM HW resetdone");
	time_left = cam_common_wait_for_completion_timeout(
			&cdm_core->reset_complete,
			msecs_to_jiffies(CAM_CDM_HW_RESET_TIMEOUT));

	if (time_left <= 0) {
		rc = -ETIMEDOUT;
		CAM_ERR(CAM_CDM, "CDM HW reset Wait failed rc=%d", rc);
		set_bit(CAM_CDM_RESET_ERR_STATUS, &cdm_core->cdm_status);
	}

	rc = cam_hw_cdm_set_cdm_core_cfg(cdm_hw);

	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to configure CDM rc=%d", rc);
		goto end;
	}

	rc = cam_hw_cdm_set_cdm_blfifo_cfg(cdm_hw);

	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to configure CDM fifo rc=%d", rc);
		goto end;
	}

	node = list_first_entry_or_null(
			&cdm_core->bl_fifo[current_fifo].bl_request_list,
			struct cam_cdm_bl_cb_request_entry, entry);

	if (node != NULL) {
		if (node->request_type == CAM_HW_CDM_BL_CB_CLIENT) {
			cam_cdm_notify_clients(cdm_hw,
					CAM_CDM_CB_STATUS_HW_ERROR,
					(void *)node);
		} else if (node->request_type == CAM_HW_CDM_BL_CB_INTERNAL) {
			CAM_ERR(CAM_CDM, "Invalid node=%pK %d", node,
					node->request_type);
		}
		list_del_init(&node->entry);
		kfree(node);
		node = NULL;
	}

	cam_hw_cdm_reset_cleanup(cdm_hw, reset_hw_hdl);
end:
	clear_bit(CAM_CDM_FLUSH_HW_STATUS, &cdm_core->cdm_status);
	clear_bit(CAM_CDM_RESET_HW_STATUS, &cdm_core->cdm_status);
	clear_bit(CAM_CDM_RESET_ERR_STATUS, &cdm_core->cdm_status);
	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++)
		mutex_unlock(&cdm_core->bl_fifo[i].fifo_lock);

	return rc;
}

int cam_hw_cdm_flush_hw(struct cam_hw_info *cdm_hw, uint32_t handle)
{
	struct cam_cdm *cdm_core = NULL;
	int rc = 0;

	cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	set_bit(CAM_CDM_FLUSH_HW_STATUS, &cdm_core->cdm_status);
	rc = cam_hw_cdm_reset_hw(cdm_hw, handle);
	clear_bit(CAM_CDM_FLUSH_HW_STATUS, &cdm_core->cdm_status);

	return rc;
}

int cam_hw_cdm_handle_error(
	struct cam_hw_info *cdm_hw,
	uint32_t            handle)
{
	struct cam_cdm *cdm_core = NULL;
	int rc = 0;

	cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	rc = cam_hw_cdm_handle_error_info(cdm_hw, handle);

	return rc;
}

int cam_hw_cdm_hang_detect(
	struct cam_hw_info *cdm_hw,
	uint32_t            handle,
	uint32_t            hang_detect_ife_ope)
{
	struct cam_cdm *cdm_core = NULL;
	struct cam_hw_soc_info *soc_info;
	int i, rc = -1;
	uint32_t fifo_idx;

	fifo_idx = CAM_CDM_GET_BLFIFO_IDX(handle);
	cdm_core = (struct cam_cdm *)cdm_hw->core_info;
	soc_info = &cdm_hw->soc_info;

	if (hang_detect_ife_ope & CAM_ISP) {
		if (atomic_read(&cdm_core->bl_fifo[fifo_idx].work_record)) {
			CAM_WARN(CAM_CDM,
				"workqueue got delayed for %s%u , work_record :%u",
				soc_info->label_name, soc_info->index,
				atomic_read(&cdm_core->bl_fifo[fifo_idx].work_record));
			rc = 0;
		}
	} else {
		for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++) {
			if (atomic_read(&cdm_core->bl_fifo[i].work_record)) {
				CAM_WARN(CAM_CDM,
					"workqueue got delayed for %s%u, work_record :%u",
					soc_info->label_name, soc_info->index,
					atomic_read(&cdm_core->bl_fifo[i].work_record));
				rc = 0;
				break;
			}
		}
	}

	return rc;
}

int cam_hw_cdm_get_cdm_config(struct cam_hw_info *cdm_hw)
{
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_cdm *core = NULL;
	int rc = 0;

	core = (struct cam_cdm *)cdm_hw->core_info;
	soc_info = &cdm_hw->soc_info;
	rc = cam_soc_util_enable_platform_resource(soc_info, true,
			CAM_SVS_VOTE, true);
	if (rc) {
		CAM_ERR(CAM_CDM, "Enable platform failed for dev %s",
				soc_info->dev_name);
		goto end;
	} else {
		CAM_DBG(CAM_CDM, "%s%u init success",
			soc_info->label_name, soc_info->index);
		cdm_hw->hw_state = CAM_HW_STATE_POWER_UP;
	}

	if (cam_cdm_read_hw_reg(cdm_hw,
			core->offsets->cmn_reg->cdm_hw_version,
			&core->hw_version)) {
		CAM_ERR(CAM_CDM, "Failed to read HW Version for %s%u",
			soc_info->label_name, soc_info->index);
		rc = -EIO;
		goto disable_platform_resource;
	}

	if (core->offsets->cmn_reg->cam_version) {
		if (cam_cdm_read_hw_reg(cdm_hw,
				core->offsets->cmn_reg->cam_version->hw_version,
				&core->hw_family_version)) {
			CAM_ERR(CAM_CDM, "Failed to read %s%d family Version",
				soc_info->label_name, soc_info->index);
			rc = -EIO;
			goto disable_platform_resource;
		}
	}

	if (cam_presil_mode_enabled()) {
		uint32_t override_family = 0;
		uint32_t override_version = 0;

		rc = of_property_read_u32(soc_info->pdev->dev.of_node,
			"override-cdm-family", &override_family);
		if (rc) {
			CAM_INFO(CAM_CDM,
				"no cdm family override,using current hw family 0x%x",
				core->hw_family_version);
			rc = 0;
		} else {
			core->hw_family_version = override_family;
		}

		rc = of_property_read_u32(soc_info->pdev->dev.of_node,
			"override-cdm-version", &override_version);
		if (rc) {
			CAM_INFO(CAM_CDM,
				"no cdm version override,using current hw version 0x%x",
				core->hw_version);
			rc = 0;
		} else {
			core->hw_version = override_version;
		}
	}

	CAM_DBG(CAM_CDM,
		"%s%d Hw version read success family =%x hw =%x",
		soc_info->label_name, soc_info->index,
		core->hw_family_version, core->hw_version);

	core->ops = cam_cdm_get_ops(core->hw_version, NULL,
		false);

	if (!core->ops) {
		CAM_ERR(CAM_CDM, "Failed to util ops for cdm hw name %s",
			core->name);
		rc = -EINVAL;
		goto disable_platform_resource;
	}

disable_platform_resource:
	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);

	if (rc) {
		CAM_ERR(CAM_CDM, "disable platform failed for dev %s",
				soc_info->dev_name);
	} else {
		CAM_DBG(CAM_CDM, "%s%d Deinit success",
			soc_info->label_name, soc_info->index);
		cdm_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	}
end:
	return rc;
}

int cam_hw_cdm_init(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_cdm *cdm_core = NULL;
	int rc, i, reset_hw_hdl = 0x0;
	unsigned long flags = 0;

	if (!hw_priv)
		return -EINVAL;

	soc_info = &cdm_hw->soc_info;
	cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	rc = cam_soc_util_enable_platform_resource(soc_info, true,
		CAM_SVS_VOTE, true);
	if (rc) {
		CAM_ERR(CAM_CDM, "Enable platform failed for %s%d",
			soc_info->label_name, soc_info->index);
		goto end;
	}
	flags = cam_hw_util_hw_lock_irqsave(cdm_hw);
	cdm_hw->hw_state = CAM_HW_STATE_POWER_UP;
	cam_hw_util_hw_unlock_irqrestore(cdm_hw, flags);

	CAM_DBG(CAM_CDM, "Enable soc done for %s%d",
		soc_info->label_name, soc_info->index);

/* Before triggering the reset to HW, clear the reset complete */
	clear_bit(CAM_CDM_ERROR_HW_STATUS, &cdm_core->cdm_status);

	for (i = 0; i < CAM_CDM_BL_FIFO_MAX; i++) {
		clear_bit(i, &cdm_core->cdm_status);
		reinit_completion(&cdm_core->bl_fifo[i].bl_complete);
	}
	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++) {
		cdm_core->bl_fifo[i].last_bl_tag_done = -1;
		atomic_set(&cdm_core->bl_fifo[i].work_record, 0);
	}

	rc = cam_hw_cdm_reset_hw(cdm_hw, reset_hw_hdl);

	if (rc) {
		CAM_ERR(CAM_CDM, "%s%u HW reset Wait failed rc=%d",
			soc_info->label_name,
			soc_info->index, rc);
		goto disable_return;
	} else {
		CAM_DBG(CAM_CDM, "%s%u Init success",
			soc_info->label_name, soc_info->index);
		for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++) {
			if (!cdm_core->bl_fifo[i].bl_depth)
				continue;

			cam_cdm_write_hw_reg(cdm_hw,
					cdm_core->offsets->irq_reg[i]->irq_mask,
					0x70003);
		}
		rc = 0;
		goto end;
	}

disable_return:
	rc = -EIO;
	flags = cam_hw_util_hw_lock_irqsave(cdm_hw);
	cdm_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	cam_hw_util_hw_unlock_irqrestore(cdm_hw, flags);
	cam_soc_util_disable_platform_resource(soc_info, true, true);
end:
	return rc;
}

int cam_hw_cdm_deinit(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_hw_info *cdm_hw = hw_priv;
	struct cam_hw_soc_info *soc_info = NULL;
	struct cam_cdm *cdm_core = NULL;
	struct cam_cdm_bl_cb_request_entry *node, *tnode;
	int rc = 0, i;
	uint32_t reset_val = 1;
	long time_left;
	unsigned long flags = 0;

	if (!hw_priv)
		return -EINVAL;

	soc_info = &cdm_hw->soc_info;
	cdm_core = (struct cam_cdm *)cdm_hw->core_info;

	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++)
		mutex_lock(&cdm_core->bl_fifo[i].fifo_lock);

	/*clear bl request */
	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++) {
		list_for_each_entry_safe(node, tnode,
			&cdm_core->bl_fifo[i].bl_request_list, entry) {
			list_del_init(&node->entry);
			kfree(node);
			node = NULL;
		}
	}

	set_bit(CAM_CDM_RESET_HW_STATUS, &cdm_core->cdm_status);
	reinit_completion(&cdm_core->reset_complete);

	/* First pause CDM, If it fails still proceed to reset CDM HW */
	cam_hw_cdm_pause_core(cdm_hw, true);
	usleep_range(1000, 1010);

	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++) {
		if (!cdm_core->bl_fifo[i].bl_depth)
			continue;

		reset_val = reset_val |
			(1 << (i + CAM_CDM_BL_FIFO_FLUSH_SHIFT));
		if (cam_cdm_write_hw_reg(cdm_hw,
				cdm_core->offsets->irq_reg[i]->irq_mask,
				0x70003)) {
			CAM_ERR(CAM_CDM, "Failed to Write CDM HW IRQ mask");
		}
	}

	if (cam_cdm_write_hw_reg(cdm_hw,
			cdm_core->offsets->cmn_reg->rst_cmd, reset_val)) {
		CAM_ERR(CAM_CDM, "Failed to Write CDM HW reset");
	}

	CAM_DBG(CAM_CDM, "Waiting for %s%u HW reset done",
		soc_info->label_name, soc_info->index);
	time_left = cam_common_wait_for_completion_timeout(
			&cdm_core->reset_complete,
			msecs_to_jiffies(CAM_CDM_HW_RESET_TIMEOUT));

	if (time_left <= 0) {
		rc = -ETIMEDOUT;
		CAM_ERR(CAM_CDM, "%s%u HW reset Wait failed rc=%d",
		soc_info->label_name, soc_info->index, rc);
	}

	clear_bit(CAM_CDM_RESET_HW_STATUS, &cdm_core->cdm_status);
	for (i = 0; i < cdm_core->offsets->reg_data->num_bl_fifo; i++)
		mutex_unlock(&cdm_core->bl_fifo[i].fifo_lock);

	flags = cam_hw_util_hw_lock_irqsave(cdm_hw);
	cdm_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	cam_hw_util_hw_unlock_irqrestore(cdm_hw, flags);
	rc = cam_soc_util_disable_platform_resource(soc_info, true, true);
	if (rc) {
		CAM_ERR(CAM_CDM, "disable platform failed for %s%u",
			soc_info->label_name, soc_info->index);
	} else {
		CAM_DBG(CAM_CDM, "%s%u Deinit success",
			soc_info->label_name, soc_info->index);
	}

	return rc;
}

static int cam_hw_cdm_component_bind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc, len = 0, i, j;
	struct cam_hw_info *cdm_hw = NULL;
	struct cam_hw_intf *cdm_hw_intf = NULL;
	struct cam_cdm *cdm_core = NULL;
	struct cam_cdm_private_dt_data *soc_private = NULL;
	struct cam_cpas_register_params cpas_parms;
	struct cam_ahb_vote ahb_vote;
	struct cam_axi_vote axi_vote = {0};
	char cdm_name[128], work_q_name[128];
	struct platform_device *pdev = to_platform_device(dev);

	cdm_hw_intf = kzalloc(sizeof(struct cam_hw_intf), GFP_KERNEL);
	if (!cdm_hw_intf)
		return -ENOMEM;

	cdm_hw = kzalloc(sizeof(struct cam_hw_info), GFP_KERNEL);
	if (!cdm_hw) {
		kfree(cdm_hw_intf);
		cdm_hw_intf = NULL;
		return -ENOMEM;
	}

	cdm_hw->core_info = kzalloc(sizeof(struct cam_cdm), GFP_KERNEL);
	if (!cdm_hw->core_info) {
		kfree(cdm_hw);
		cdm_hw = NULL;
		kfree(cdm_hw_intf);
		cdm_hw_intf = NULL;
		return -ENOMEM;
	}

	cdm_hw->hw_state = CAM_HW_STATE_POWER_DOWN;
	cdm_hw->soc_info.pdev = pdev;
	cdm_hw->soc_info.dev = &pdev->dev;
	cdm_hw->soc_info.dev_name = pdev->name;
	cdm_hw_intf->hw_type = CAM_HW_CDM;
	cdm_hw->open_count = 0;
	mutex_init(&cdm_hw->hw_mutex);
	cam_hw_util_init_hw_lock(cdm_hw);
	init_completion(&cdm_hw->hw_complete);

	rc = cam_hw_cdm_soc_get_dt_properties(cdm_hw, msm_cam_hw_cdm_dt_match);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to get dt properties");
		goto release_mem;
	}
	cdm_hw_intf->hw_idx = cdm_hw->soc_info.index;
	cdm_core = (struct cam_cdm *)cdm_hw->core_info;
	soc_private = (struct cam_cdm_private_dt_data *)
		cdm_hw->soc_info.soc_private;
	if (soc_private->dt_cdm_shared == true)
		cdm_core->flags = CAM_CDM_FLAG_SHARED_CDM;
	else
		cdm_core->flags = CAM_CDM_FLAG_PRIVATE_CDM;

	cdm_core->id = cam_hw_cdm_get_id_by_name(cdm_core->name);

	CAM_DBG(CAM_CDM, "cdm_name %s", cdm_core->name);

	if (cdm_core->id >= CAM_CDM_MAX) {
		CAM_ERR(CAM_CDM, "Failed to get CDM HW name for %s",
			cdm_core->name);
		goto release_private_mem;
	}

	init_completion(&cdm_core->reset_complete);
	cdm_hw_intf->hw_priv = cdm_hw;
	cdm_hw_intf->hw_ops.get_hw_caps = cam_cdm_get_caps;
	cdm_hw_intf->hw_ops.init = cam_hw_cdm_init;
	cdm_hw_intf->hw_ops.deinit = cam_hw_cdm_deinit;
	cdm_hw_intf->hw_ops.start = cam_cdm_stream_start;
	cdm_hw_intf->hw_ops.stop = cam_cdm_stream_stop;
	cdm_hw_intf->hw_ops.read = NULL;
	cdm_hw_intf->hw_ops.write = NULL;
	cdm_hw_intf->hw_ops.process_cmd = cam_cdm_process_cmd;
	mutex_lock(&cdm_hw->hw_mutex);

	CAM_DBG(CAM_CDM, "type %d index %d", cdm_hw_intf->hw_type,
		cdm_hw_intf->hw_idx);

	platform_set_drvdata(pdev, cdm_hw_intf);

	snprintf(cdm_name, sizeof(cdm_name), "%s", cdm_hw->soc_info.label_name);

	rc = cam_smmu_get_handle(cdm_name, &cdm_core->iommu_hdl.non_secure);
	if (rc < 0) {
		if (rc != -EALREADY) {
			CAM_ERR(CAM_CDM,
				"%s get iommu handle failed, rc = %d",
				cdm_name, rc);
			goto unlock_release_mem;
		}
		rc = 0;
	}

	cam_smmu_set_client_page_fault_handler(cdm_core->iommu_hdl.non_secure,
		cam_hw_cdm_iommu_fault_handler, cdm_hw);

	cdm_core->iommu_hdl.secure = -1;

	for (i = 0; i < CAM_CDM_BL_FIFO_MAX; i++) {
		INIT_LIST_HEAD(&cdm_core->bl_fifo[i].bl_request_list);

		mutex_init(&cdm_core->bl_fifo[i].fifo_lock);

		init_completion(&cdm_core->bl_fifo[i].bl_complete);

		len = strlcpy(work_q_name, cdm_hw->soc_info.label_name,
				sizeof(work_q_name));
		snprintf(work_q_name + len, sizeof(work_q_name) - len, "%d_%d", cdm_hw->soc_info.index, i);
		cdm_core->bl_fifo[i].work_queue = alloc_workqueue(work_q_name,
				WQ_UNBOUND | WQ_MEM_RECLAIM | WQ_SYSFS,
				CAM_CDM_INFLIGHT_WORKS);
		if (!cdm_core->bl_fifo[i].work_queue) {
			CAM_ERR(CAM_CDM,
				"Workqueue allocation failed for FIFO %d, cdm %s",
				i, cdm_core->name);
			goto failed_workq_create;
		}

		CAM_DBG(CAM_CDM, "wq %s", work_q_name);
	}

	rc = cam_soc_util_request_platform_resource(&cdm_hw->soc_info,
			cam_hw_cdm_irq, cdm_hw);
	if (rc) {
		CAM_ERR(CAM_CDM,
			"Failed to request platform resource for %s%u",
			cdm_hw->soc_info.label_name,
			cdm_hw->soc_info.index);
		goto destroy_non_secure_hdl;
	}
	cpas_parms.cam_cpas_client_cb = cam_cdm_cpas_cb;
	cpas_parms.cell_index = cdm_hw->soc_info.index;
	cpas_parms.dev = &pdev->dev;
	cpas_parms.userdata = cdm_hw_intf;
	strlcpy(cpas_parms.identifier, cdm_hw->soc_info.label_name,
		CAM_HW_IDENTIFIER_LENGTH);
	rc = cam_cpas_register_client(&cpas_parms);
	if (rc) {
		CAM_ERR(CAM_CDM, "Virtual CDM CPAS registration failed");
		goto release_platform_resource;
	}
	CAM_DBG(CAM_CDM, "CPAS registration successful handle=%d",
		cpas_parms.client_handle);
	cdm_core->cpas_handle = cpas_parms.client_handle;

	ahb_vote.type = CAM_VOTE_ABSOLUTE;
	ahb_vote.vote.level = CAM_LOWSVS_VOTE;
	axi_vote.num_paths = 1;
	axi_vote.axi_path[0].path_data_type = CAM_AXI_PATH_DATA_ALL;
	axi_vote.axi_path[0].transac_type = CAM_AXI_TRANSACTION_READ;
	axi_vote.axi_path[0].camnoc_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ab_bw = CAM_CPAS_DEFAULT_AXI_BW;
	axi_vote.axi_path[0].mnoc_ib_bw = CAM_CPAS_DEFAULT_AXI_BW;

	rc = cam_cpas_start(cdm_core->cpas_handle, &ahb_vote, &axi_vote);
	if (rc) {
		CAM_ERR(CAM_CDM, "CPAS start failed");
		goto cpas_unregister;
	}

	rc = cam_hw_cdm_get_cdm_config(cdm_hw);
	if (rc) {
		CAM_ERR(CAM_CDM, "Failed to get cdm configuration rc = %d", rc);
		goto cpas_stop;
	}

	if (cdm_core->hw_version < CAM_CDM_VERSION_2_0) {
		for (i = 0; i < CAM_CDM_BL_FIFO_MAX; i++) {
			cdm_core->bl_fifo[i].bl_depth =
				CAM_CDM_BL_FIFO_LENGTH_MAX_DEFAULT;
			CAM_DBG(CAM_CDM, "Setting FIFO%d length to %d",
				i, cdm_core->bl_fifo[i].bl_depth);
		}
	} else {
		for (i = 0; i < CAM_CDM_BL_FIFO_MAX; i++) {
			cdm_core->bl_fifo[i].bl_depth =
				soc_private->fifo_depth[i];
			CAM_DBG(CAM_CDM, "Setting FIFO%d length to %d",
				i, cdm_core->bl_fifo[i].bl_depth);
		}
	}

	cdm_core->arbitration = cam_cdm_get_arbitration_type(
		cdm_core->hw_version, cdm_core->id);

	cdm_core->cdm_status = CAM_CDM_HW_INIT_STATUS;

	cdm_core->ops = cam_cdm_get_ops(cdm_core->hw_version, NULL,
		false);

	if (!cdm_core->ops) {
		CAM_ERR(CAM_CDM, "Failed to util ops for %s%u HW",
			cdm_hw->soc_info.label_name,
			cdm_hw->soc_info.index);
		rc = -EINVAL;
		goto cpas_stop;
	}

	if (!cam_cdm_set_cam_hw_version(cdm_core->hw_version,
		&cdm_core->version)) {
		CAM_ERR(CAM_CDM, "Failed to set cam hw version for hw");
		rc = -EINVAL;
		goto cpas_stop;
	}

	rc = cam_cpas_stop(cdm_core->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_CDM, "CPAS stop failed");
		goto cpas_unregister;
	}

	rc = cam_cdm_intf_register_hw_cdm(cdm_hw_intf,
		soc_private, CAM_HW_CDM, &cdm_core->index);
	if (rc) {
		CAM_ERR(CAM_CDM, "HW CDM Interface registration failed");
		goto cpas_unregister;
	}
	mutex_unlock(&cdm_hw->hw_mutex);

	CAM_DBG(CAM_CDM, "%s component bound successfully", cdm_core->name);

	return rc;

cpas_stop:
	if (cam_cpas_stop(cdm_core->cpas_handle))
		CAM_ERR(CAM_CDM, "CPAS stop failed");
cpas_unregister:
	if (cam_cpas_unregister_client(cdm_core->cpas_handle))
		CAM_ERR(CAM_CDM, "CPAS unregister failed");
release_platform_resource:
	if (cam_soc_util_release_platform_resource(&cdm_hw->soc_info))
		CAM_ERR(CAM_CDM, "Release platform resource failed");
failed_workq_create:
	for (j = 0; j < i; j++) {
		flush_workqueue(cdm_core->bl_fifo[j].work_queue);
		destroy_workqueue(cdm_core->bl_fifo[j].work_queue);
	}
destroy_non_secure_hdl:
	cam_smmu_set_client_page_fault_handler(cdm_core->iommu_hdl.non_secure,
		NULL, cdm_hw);
	if (cam_smmu_destroy_handle(cdm_core->iommu_hdl.non_secure))
		CAM_ERR(CAM_CDM, "Release iommu secure hdl failed");
unlock_release_mem:
	mutex_unlock(&cdm_hw->hw_mutex);
release_private_mem:
	kfree(cdm_hw->soc_info.soc_private);
	cdm_hw->soc_info.soc_private = NULL;
release_mem:
	mutex_destroy(&cdm_hw->hw_mutex);
	kfree(cdm_hw_intf);
	cdm_hw_intf = NULL;
	kfree(cdm_hw->core_info);
	cdm_hw->core_info = NULL;
	kfree(cdm_hw);
	cdm_hw = NULL;
	return rc;
}

static void cam_hw_cdm_component_unbind(struct device *dev,
	struct device *master_dev, void *data)
{
	int rc = -EBUSY, i;
	struct cam_hw_info *cdm_hw = NULL;
	struct cam_hw_intf *cdm_hw_intf = NULL;
	struct cam_cdm *cdm_core = NULL;
	struct platform_device *pdev = to_platform_device(dev);

	cdm_hw_intf = platform_get_drvdata(pdev);
	if (!cdm_hw_intf) {
		CAM_ERR(CAM_CDM, "Failed to get dev private data");
		return;
	}

	cdm_hw = cdm_hw_intf->hw_priv;
	if (!cdm_hw) {
		CAM_ERR(CAM_CDM,
			"Failed to get hw private data for type=%d idx=%d",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx);
		return;
	}

	cdm_core = cdm_hw->core_info;
	if (!cdm_core) {
		CAM_ERR(CAM_CDM,
			"Failed to get hw core data for type=%d idx=%d",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx);
		return;
	}

	if (cdm_hw->open_count != 0) {
		CAM_ERR(CAM_CDM, "Hw open count invalid type=%d idx=%d cnt=%d",
			cdm_hw_intf->hw_type, cdm_hw_intf->hw_idx,
			cdm_hw->open_count);
		return;
	}

	if (cdm_hw->hw_state == CAM_HW_STATE_POWER_UP) {
		rc = cam_hw_cdm_deinit(cdm_hw, NULL, 0);
		if (rc) {
			CAM_ERR(CAM_CDM, "Deinit failed for hw");
			return;
		}
	}

	rc = cam_cdm_intf_deregister_hw_cdm(cdm_hw_intf,
		cdm_hw->soc_info.soc_private, CAM_HW_CDM, cdm_core->index);
	if (rc) {
		CAM_ERR(CAM_CDM,
			"HW_CDM interface deregistration failed: rd: %d", rc);
	}

	rc = cam_cpas_unregister_client(cdm_core->cpas_handle);
	if (rc) {
		CAM_ERR(CAM_CDM, "CPAS unregister failed");
		return;
	}

	if (cam_soc_util_release_platform_resource(&cdm_hw->soc_info))
		CAM_ERR(CAM_CDM, "Release platform resource failed");

	for (i = 0; i < CAM_CDM_BL_FIFO_MAX; i++) {
		flush_workqueue(cdm_core->bl_fifo[i].work_queue);
		destroy_workqueue(cdm_core->bl_fifo[i].work_queue);
	}

	cam_smmu_unset_client_page_fault_handler(
		cdm_core->iommu_hdl.non_secure, cdm_hw);
	if (cam_smmu_destroy_handle(cdm_core->iommu_hdl.non_secure))
		CAM_ERR(CAM_CDM, "Release iommu secure hdl failed");

	mutex_destroy(&cdm_hw->hw_mutex);
	kfree(cdm_hw->soc_info.soc_private);
	cdm_hw->soc_info.soc_private = NULL;
	kfree(cdm_hw_intf);
	cdm_hw_intf = NULL;
	kfree(cdm_hw->core_info);
	cdm_hw->core_info = NULL;
	kfree(cdm_hw);
	cdm_hw = NULL;
}

const static struct component_ops cam_hw_cdm_component_ops = {
	.bind = cam_hw_cdm_component_bind,
	.unbind = cam_hw_cdm_component_unbind,
};

int cam_hw_cdm_probe(struct platform_device *pdev)
{
	int rc = 0;

	CAM_DBG(CAM_CDM, "Adding HW CDM component");
	rc = component_add(&pdev->dev, &cam_hw_cdm_component_ops);
	if (rc)
		CAM_ERR(CAM_CDM, "failed to add component rc: %d", rc);

	return rc;
}

int cam_hw_cdm_remove(struct platform_device *pdev)
{
	component_del(&pdev->dev, &cam_hw_cdm_component_ops);
	return 0;
}

struct platform_driver cam_hw_cdm_driver = {
	.probe = cam_hw_cdm_probe,
	.remove = cam_hw_cdm_remove,
	.driver = {
		.name = "msm_cam_cdm",
		.owner = THIS_MODULE,
		.of_match_table = msm_cam_hw_cdm_dt_match,
		.suppress_bind_attrs = true,
	},
};

int cam_hw_cdm_init_module(void)
{
	return platform_driver_register(&cam_hw_cdm_driver);
}

void cam_hw_cdm_exit_module(void)
{
	platform_driver_unregister(&cam_hw_cdm_driver);
}

MODULE_DESCRIPTION("MSM Camera HW CDM driver");
MODULE_LICENSE("GPL v2");
