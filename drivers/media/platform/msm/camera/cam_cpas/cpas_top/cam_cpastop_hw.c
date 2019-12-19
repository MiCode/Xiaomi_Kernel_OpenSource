/* Copyright (c) 2017-2018, The Linux Foundation. All rights reserved.
 * Copyright (C) 2019 XiaoMi, Inc.
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

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/slab.h>

#include "cam_cpas_hw_intf.h"
#include "cam_cpas_hw.h"
#include "cam_cpastop_hw.h"
#include "cam_io_util.h"
#include "cam_cpas_soc.h"
#include "cpastop100.h"
#include "cpastop_v170_110.h"

struct cam_camnoc_info *camnoc_info;

#define CAMNOC_SLAVE_MAX_ERR_CODE 7
static const char * const camnoc_salve_err_code[] = {
	"Target Error",              /* err code 0 */
	"Address decode error",      /* err code 1 */
	"Unsupported request",       /* err code 2 */
	"Disconnected target",       /* err code 3 */
	"Security violation",        /* err code 4 */
	"Hidden security violation", /* err code 5 */
	"Timeout Error",             /* err code 6 */
	"Unknown Error",             /* unknown err code */
};

static int cam_cpastop_get_hw_info(struct cam_hw_info *cpas_hw,
	struct cam_cpas_hw_caps *hw_caps)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	int32_t reg_indx = cpas_core->regbase_index[CAM_CPAS_REG_CPASTOP];
	uint32_t reg_value;

	if (reg_indx == -1)
		return -EINVAL;

	hw_caps->camera_family = CAM_FAMILY_CPAS_SS;

	reg_value = cam_io_r_mb(soc_info->reg_map[reg_indx].mem_base + 0x0);
	hw_caps->camera_version.major =
		CAM_BITS_MASK_SHIFT(reg_value, 0xff0000, 0x10);
	hw_caps->camera_version.minor =
		CAM_BITS_MASK_SHIFT(reg_value, 0xff00, 0x8);
	hw_caps->camera_version.incr =
		CAM_BITS_MASK_SHIFT(reg_value, 0xff, 0x0);

	reg_value = cam_io_r_mb(soc_info->reg_map[reg_indx].mem_base + 0x4);
	hw_caps->cpas_version.major =
		CAM_BITS_MASK_SHIFT(reg_value, 0xf0000000, 0x1c);
	hw_caps->cpas_version.minor =
		CAM_BITS_MASK_SHIFT(reg_value, 0xfff0000, 0x10);
	hw_caps->cpas_version.incr =
		CAM_BITS_MASK_SHIFT(reg_value, 0xffff, 0x0);

	reg_value = cam_io_r_mb(soc_info->reg_map[reg_indx].mem_base + 0x8);
	hw_caps->camera_capability = reg_value;

	CAM_DBG(CAM_FD, "Family %d, version %d.%d.%d, cpas %d.%d.%d, cap 0x%x",
		hw_caps->camera_family, hw_caps->camera_version.major,
		hw_caps->camera_version.minor, hw_caps->camera_version.incr,
		hw_caps->cpas_version.major, hw_caps->cpas_version.minor,
		hw_caps->cpas_version.incr, hw_caps->camera_capability);

	return 0;
}

static int cam_cpastop_setup_regbase_indices(struct cam_hw_soc_info *soc_info,
	int32_t regbase_index[], int32_t num_reg_map)
{
	uint32_t index;
	int rc;

	if (num_reg_map > CAM_CPAS_REG_MAX) {
		CAM_ERR(CAM_CPAS, "invalid num_reg_map=%d", num_reg_map);
		return -EINVAL;
	}

	if (soc_info->num_mem_block > CAM_SOC_MAX_BLOCK) {
		CAM_ERR(CAM_CPAS, "invalid num_mem_block=%d",
			soc_info->num_mem_block);
		return -EINVAL;
	}

	rc = cam_common_util_get_string_index(soc_info->mem_block_name,
		soc_info->num_mem_block, "cam_cpas_top", &index);
	if ((rc == 0) && (index < num_reg_map)) {
		regbase_index[CAM_CPAS_REG_CPASTOP] = index;
	} else {
		CAM_ERR(CAM_CPAS, "regbase not found for CPASTOP, rc=%d, %d %d",
			rc, index, num_reg_map);
		return -EINVAL;
	}

	rc = cam_common_util_get_string_index(soc_info->mem_block_name,
		soc_info->num_mem_block, "cam_camnoc", &index);
	if ((rc == 0) && (index < num_reg_map)) {
		regbase_index[CAM_CPAS_REG_CAMNOC] = index;
	} else {
		CAM_ERR(CAM_CPAS, "regbase not found for CAMNOC, rc=%d, %d %d",
			rc, index, num_reg_map);
		return -EINVAL;
	}

	return 0;
}

static int cam_cpastop_handle_errlogger(struct cam_cpas *cpas_core,
	struct cam_hw_soc_info *soc_info,
	struct cam_camnoc_irq_slave_err_data *slave_err)
{
	int camnoc_index = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];
	int err_code_index = 0;

	if (!camnoc_info->err_logger) {
		CAM_ERR_RATE_LIMIT(CAM_CPAS, "Invalid err logger info");
		return -EINVAL;
	}

	slave_err->mainctrl.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->mainctrl);

	slave_err->errvld.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->errvld);

	slave_err->errlog0_low.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->errlog0_low);

	slave_err->errlog0_high.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->errlog0_high);

	slave_err->errlog1_low.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->errlog1_low);

	slave_err->errlog1_high.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->errlog1_high);

	slave_err->errlog2_low.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->errlog2_low);

	slave_err->errlog2_high.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->errlog2_high);

	slave_err->errlog3_low.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->errlog3_low);

	slave_err->errlog3_high.value = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->err_logger->errlog3_high);

	CAM_ERR_RATE_LIMIT(CAM_CPAS,
		"Possible memory configuration issue, fault at SMMU raised as CAMNOC SLAVE_IRQ");

	CAM_ERR_RATE_LIMIT(CAM_CPAS,
		"mainctrl[0x%x 0x%x] errvld[0x%x 0x%x] stall_en=%d, fault_en=%d, err_vld=%d",
		camnoc_info->err_logger->mainctrl,
		slave_err->mainctrl.value,
		camnoc_info->err_logger->errvld,
		slave_err->errvld.value,
		slave_err->mainctrl.stall_en,
		slave_err->mainctrl.fault_en,
		slave_err->errvld.err_vld);

	err_code_index = slave_err->errlog0_low.err_code;
	if (err_code_index > CAMNOC_SLAVE_MAX_ERR_CODE)
		err_code_index = CAMNOC_SLAVE_MAX_ERR_CODE;

	CAM_ERR_RATE_LIMIT(CAM_CPAS,
		"errlog0 low[0x%x 0x%x] high[0x%x 0x%x] loginfo_vld=%d, word_error=%d, non_secure=%d, device=%d, opc=%d, err_code=%d(%s) sizef=%d, addr_space=%d, len1=%d",
		camnoc_info->err_logger->errlog0_low,
		slave_err->errlog0_low.value,
		camnoc_info->err_logger->errlog0_high,
		slave_err->errlog0_high.value,
		slave_err->errlog0_low.loginfo_vld,
		slave_err->errlog0_low.word_error,
		slave_err->errlog0_low.non_secure,
		slave_err->errlog0_low.device,
		slave_err->errlog0_low.opc,
		slave_err->errlog0_low.err_code,
		camnoc_salve_err_code[err_code_index],
		slave_err->errlog0_low.sizef,
		slave_err->errlog0_low.addr_space,
		slave_err->errlog0_high.len1);

	CAM_ERR_RATE_LIMIT(CAM_CPAS,
		"errlog1_low[0x%x 0x%x]  errlog1_high[0x%x 0x%x] errlog2_low[0x%x 0x%x]  errlog2_high[0x%x 0x%x] errlog3_low[0x%x 0x%x]  errlog3_high[0x%x 0x%x]",
		camnoc_info->err_logger->errlog1_low,
		slave_err->errlog1_low.value,
		camnoc_info->err_logger->errlog1_high,
		slave_err->errlog1_high.value,
		camnoc_info->err_logger->errlog2_low,
		slave_err->errlog2_low.value,
		camnoc_info->err_logger->errlog2_high,
		slave_err->errlog2_high.value,
		camnoc_info->err_logger->errlog3_low,
		slave_err->errlog3_low.value,
		camnoc_info->err_logger->errlog3_high,
		slave_err->errlog3_high.value);

	return 0;
}

static int cam_cpastop_handle_ubwc_enc_err(struct cam_cpas *cpas_core,
	struct cam_hw_soc_info *soc_info, int i,
	struct cam_camnoc_irq_ubwc_enc_data *enc_err)
{
	int camnoc_index = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];

	enc_err->encerr_status.value =
		cam_io_r_mb(soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->irq_err[i].err_status.offset);

	/* Let clients handle the UBWC errors */
	CAM_DBG(CAM_CPAS,
		"ubwc enc err [%d]: offset[0x%x] value[0x%x]",
		i, camnoc_info->irq_err[i].err_status.offset,
		enc_err->encerr_status.value);

	return 0;
}

static int cam_cpastop_handle_ubwc_dec_err(struct cam_cpas *cpas_core,
	struct cam_hw_soc_info *soc_info, int i,
	struct cam_camnoc_irq_ubwc_dec_data *dec_err)
{
	int camnoc_index = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];

	dec_err->decerr_status.value =
		cam_io_r_mb(soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->irq_err[i].err_status.offset);

	/* Let clients handle the UBWC errors */
	CAM_DBG(CAM_CPAS,
		"ubwc dec err status [%d]: offset[0x%x] value[0x%x] thr_err=%d, fcl_err=%d, len_md_err=%d, format_err=%d",
		i, camnoc_info->irq_err[i].err_status.offset,
		dec_err->decerr_status.value,
		dec_err->decerr_status.thr_err,
		dec_err->decerr_status.fcl_err,
		dec_err->decerr_status.len_md_err,
		dec_err->decerr_status.format_err);

	return 0;
}

static int cam_cpastop_handle_ahb_timeout_err(struct cam_hw_info *cpas_hw,
	struct cam_camnoc_irq_ahb_timeout_data *ahb_err)
{
	CAM_ERR_RATE_LIMIT(CAM_CPAS, "ahb timout error");

	return 0;
}

static int cam_cpastop_disable_test_irq(struct cam_hw_info *cpas_hw)
{
	camnoc_info->irq_sbm->sbm_clear.value &= ~0x4;
	camnoc_info->irq_sbm->sbm_enable.value &= ~0x100;
	camnoc_info->irq_err[CAM_CAMNOC_HW_IRQ_CAMNOC_TEST].enable = false;

	return 0;
}

static int cam_cpastop_reset_irq(struct cam_hw_info *cpas_hw)
{
	int i;

	cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
		&camnoc_info->irq_sbm->sbm_clear);
	for (i = 0; i < camnoc_info->irq_err_size; i++) {
		if (camnoc_info->irq_err[i].enable)
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->irq_err[i].err_clear);
	}

	cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
		&camnoc_info->irq_sbm->sbm_enable);
	for (i = 0; i < camnoc_info->irq_err_size; i++) {
		if (camnoc_info->irq_err[i].enable)
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->irq_err[i].err_enable);
	}

	return 0;
}

static void cam_cpastop_notify_clients(struct cam_cpas *cpas_core,
	struct cam_cpas_irq_data *irq_data)
{
	int i;
	struct cam_cpas_client *cpas_client;
	bool error_handled = false;

	CAM_DBG(CAM_CPAS,
		"Notify CB : num_clients=%d, registered=%d, started=%d",
		cpas_core->num_clients, cpas_core->registered_clients,
		cpas_core->streamon_clients);

	for (i = 0; i < cpas_core->num_clients; i++) {
		if (CAM_CPAS_CLIENT_STARTED(cpas_core, i)) {
			cpas_client = cpas_core->cpas_client[i];
			if (cpas_client->data.cam_cpas_client_cb) {
				CAM_DBG(CAM_CPAS,
					"Calling client CB %d : %d",
					i, irq_data->irq_type);
				error_handled =
					cpas_client->data.cam_cpas_client_cb(
					cpas_client->data.client_handle,
					cpas_client->data.userdata,
					irq_data);
				if (error_handled)
					break;
			}
		}
	}
}

static void cam_cpastop_work(struct work_struct *work)
{
	struct cam_cpas_work_payload *payload;
	struct cam_hw_info *cpas_hw;
	struct cam_cpas *cpas_core;
	struct cam_hw_soc_info *soc_info;
	int i;
	enum cam_camnoc_hw_irq_type irq_type;
	struct cam_cpas_irq_data irq_data;

	payload = container_of(work, struct cam_cpas_work_payload, work);
	if (!payload) {
		CAM_ERR(CAM_CPAS, "NULL payload");
		return;
	}

	cpas_hw = payload->hw;
	cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	soc_info = &cpas_hw->soc_info;

	if (!atomic_inc_not_zero(&cpas_core->irq_count)) {
		CAM_ERR(CAM_CPAS, "CPAS off");
		return;
	}

	for (i = 0; i < camnoc_info->irq_err_size; i++) {
		if ((payload->irq_status & camnoc_info->irq_err[i].sbm_port) &&
			(camnoc_info->irq_err[i].enable)) {
			irq_type = camnoc_info->irq_err[i].irq_type;
			CAM_ERR_RATE_LIMIT(CAM_CPAS, "Error occurred, type=%d", irq_type);
			memset(&irq_data, 0x0, sizeof(irq_data));
			irq_data.irq_type = (enum cam_camnoc_irq_type)irq_type;

			switch (irq_type) {
			case CAM_CAMNOC_HW_IRQ_SLAVE_ERROR:
				cam_cpastop_handle_errlogger(
					cpas_core, soc_info,
					&irq_data.u.slave_err);
				break;
			case CAM_CAMNOC_HW_IRQ_IFE02_UBWC_ENCODE_ERROR:
			case CAM_CAMNOC_HW_IRQ_IFE13_UBWC_ENCODE_ERROR:
			case CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_ENCODE_ERROR:
				cam_cpastop_handle_ubwc_enc_err(
					cpas_core, soc_info, i,
					&irq_data.u.enc_err);
				break;
			case CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_DECODE_ERROR:
				cam_cpastop_handle_ubwc_dec_err(
					cpas_core, soc_info, i,
					&irq_data.u.dec_err);
				break;
			case CAM_CAMNOC_HW_IRQ_AHB_TIMEOUT:
				cam_cpastop_handle_ahb_timeout_err(
					cpas_hw, &irq_data.u.ahb_err);
				break;
			case CAM_CAMNOC_HW_IRQ_CAMNOC_TEST:
				CAM_DBG(CAM_CPAS, "TEST IRQ");
				break;
			default:
				CAM_ERR(CAM_CPAS, "Invalid IRQ type");
				break;
			}

			cam_cpastop_notify_clients(cpas_core, &irq_data);

			payload->irq_status &=
				~camnoc_info->irq_err[i].sbm_port;
		}
	}
	atomic_dec(&cpas_core->irq_count);
	wake_up(&cpas_core->irq_count_wq);
	CAM_DBG(CAM_CPAS, "irq_count=%d\n", atomic_read(&cpas_core->irq_count));

	if (payload->irq_status)
		CAM_ERR(CAM_CPAS, "IRQ not handled irq_status=0x%x",
			payload->irq_status);

	kfree(payload);
}

static irqreturn_t cam_cpastop_handle_irq(int irq_num, void *data)
{
	struct cam_hw_info *cpas_hw = (struct cam_hw_info *)data;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	int camnoc_index = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];
	struct cam_cpas_work_payload *payload;

	if (!atomic_inc_not_zero(&cpas_core->irq_count)) {
		CAM_ERR(CAM_CPAS, "CPAS off");
		return IRQ_HANDLED;
	}

	payload = kzalloc(sizeof(struct cam_cpas_work_payload), GFP_ATOMIC);
	if (!payload)
		goto done;

	payload->irq_status = cam_io_r_mb(
		soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->irq_sbm->sbm_status.offset);

	CAM_DBG(CAM_CPAS, "IRQ callback, irq_status=0x%x", payload->irq_status);

	payload->hw = cpas_hw;
	INIT_WORK((struct work_struct *)&payload->work, cam_cpastop_work);

	if (TEST_IRQ_ENABLE)
		cam_cpastop_disable_test_irq(cpas_hw);

	cam_cpastop_reset_irq(cpas_hw);

	queue_work(cpas_core->work_queue, &payload->work);
done:
	atomic_dec(&cpas_core->irq_count);
	wake_up(&cpas_core->irq_count_wq);

	return IRQ_HANDLED;
}

static int cam_cpastop_poweron(struct cam_hw_info *cpas_hw)
{
	int i;

	cam_cpastop_reset_irq(cpas_hw);

	for (i = 0; i < camnoc_info->specific_size; i++) {
		if (camnoc_info->specific[i].enable) {
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].priority_lut_low);
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].priority_lut_high);
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].urgency);
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].danger_lut);
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].safe_lut);
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].ubwc_ctl);
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].flag_out_set0_low);
		}
	}

	return 0;
}

static int cam_cpastop_poweroff(struct cam_hw_info *cpas_hw)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	int camnoc_index = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];
	int rc = 0;
	struct cam_cpas_hw_errata_wa_list *errata_wa_list =
		camnoc_info->errata_wa_list;

	if (!errata_wa_list)
		return 0;

	if (errata_wa_list->camnoc_flush_slave_pending_trans.enable) {
		struct cam_cpas_hw_errata_wa *errata_wa =
			&errata_wa_list->camnoc_flush_slave_pending_trans;

		rc = cam_io_poll_value_wmask(
			soc_info->reg_map[camnoc_index].mem_base +
			errata_wa->data.reg_info.offset,
			errata_wa->data.reg_info.value,
			errata_wa->data.reg_info.mask,
			CAM_CPAS_POLL_RETRY_CNT,
			CAM_CPAS_POLL_MIN_USECS, CAM_CPAS_POLL_MAX_USECS);
		if (rc) {
			CAM_DBG(CAM_CPAS,
				"camnoc flush slave pending trans failed");
			/* Do not return error, passthrough */
			rc = 0;
		}
	}

	return rc;
}

static int cam_cpastop_init_hw_version(struct cam_hw_info *cpas_hw,
	struct cam_cpas_hw_caps *hw_caps)
{
	if ((hw_caps->camera_version.major == 1) &&
		(hw_caps->camera_version.minor == 7) &&
		(hw_caps->camera_version.incr == 0)) {
		if ((hw_caps->cpas_version.major == 1) &&
			(hw_caps->cpas_version.minor == 0) &&
			(hw_caps->cpas_version.incr == 0)) {
			camnoc_info = &cam170_cpas100_camnoc_info;
		} else if ((hw_caps->cpas_version.major == 1) &&
			(hw_caps->cpas_version.minor == 1) &&
			(hw_caps->cpas_version.incr == 0)) {
			camnoc_info = &cam170_cpas110_camnoc_info;
		} else {
			CAM_ERR(CAM_CPAS, "CPAS Version not supported %d.%d.%d",
				hw_caps->cpas_version.major,
				hw_caps->cpas_version.minor,
				hw_caps->cpas_version.incr);
			return -EINVAL;
		}
	} else {
		CAM_ERR(CAM_CPAS, "Camera Version not supported %d.%d.%d",
			hw_caps->camera_version.major,
			hw_caps->camera_version.minor,
			hw_caps->camera_version.incr);
		return -EINVAL;
	}

	return 0;
}

int cam_cpastop_get_internal_ops(struct cam_cpas_internal_ops *internal_ops)
{
	if (!internal_ops) {
		CAM_ERR(CAM_CPAS, "invalid NULL param");
		return -EINVAL;
	}

	internal_ops->get_hw_info = cam_cpastop_get_hw_info;
	internal_ops->init_hw_version = cam_cpastop_init_hw_version;
	internal_ops->handle_irq = cam_cpastop_handle_irq;
	internal_ops->setup_regbase = cam_cpastop_setup_regbase_indices;
	internal_ops->power_on = cam_cpastop_poweron;
	internal_ops->power_off = cam_cpastop_poweroff;

	return 0;
}
