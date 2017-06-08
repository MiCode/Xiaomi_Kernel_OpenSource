/* Copyright (c) 2017, The Linux Foundation. All rights reserved.
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

#include "cam_cpas_hw_intf.h"
#include "cam_cpas_hw.h"
#include "cam_cpastop_hw.h"
#include "cam_io_util.h"
#include "cam_cpas_soc.h"
#include "cpastop100.h"

struct cam_camnoc_info *camnoc_info;

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
		BITS_MASK_SHIFT(reg_value, 0xff0000, 0x10);
	hw_caps->camera_version.minor =
		BITS_MASK_SHIFT(reg_value, 0xff00, 0x8);
	hw_caps->camera_version.incr =
		BITS_MASK_SHIFT(reg_value, 0xff, 0x0);

	reg_value = cam_io_r_mb(soc_info->reg_map[reg_indx].mem_base + 0x4);
	hw_caps->cpas_version.major =
		BITS_MASK_SHIFT(reg_value, 0xf0000000, 0x1c);
	hw_caps->cpas_version.minor =
		BITS_MASK_SHIFT(reg_value, 0xfff0000, 0x10);
	hw_caps->cpas_version.incr =
		BITS_MASK_SHIFT(reg_value, 0xffff, 0x0);

	reg_value = cam_io_r_mb(soc_info->reg_map[reg_indx].mem_base + 0x8);
	hw_caps->camera_capability = reg_value;

	CPAS_CDBG("Family %d, version %d.%d.%d, cpas %d.%d.%d, cap 0x%x\n",
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
		pr_err("invalid num_reg_map=%d\n", num_reg_map);
		return -EINVAL;
	}

	if (soc_info->num_mem_block > CAM_SOC_MAX_BLOCK) {
		pr_err("invalid num_mem_block=%d\n", soc_info->num_mem_block);
		return -EINVAL;
	}

	rc = cam_cpas_util_get_string_index(soc_info->mem_block_name,
		soc_info->num_mem_block, "cam_cpas_top", &index);
	if ((rc == 0) && (index < num_reg_map)) {
		regbase_index[CAM_CPAS_REG_CPASTOP] = index;
	} else {
		pr_err("regbase not found for CPASTOP, rc=%d, %d %d\n",
			rc, index, num_reg_map);
		return -EINVAL;
	}

	rc = cam_cpas_util_get_string_index(soc_info->mem_block_name,
		soc_info->num_mem_block, "cam_camnoc", &index);
	if ((rc == 0) && (index < num_reg_map)) {
		regbase_index[CAM_CPAS_REG_CAMNOC] = index;
	} else {
		pr_err("regbase not found for CAMNOC, rc=%d, %d %d\n",
			rc, index, num_reg_map);
		return -EINVAL;
	}

	return 0;
}

static int cam_cpastop_handle_errlogger(struct cam_cpas *cpas_core,
	struct cam_hw_soc_info *soc_info)
{
	uint32_t reg_value;
	int i;
	int camnoc_index = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];

	for (i = 0; i < camnoc_info->error_logger_size; i++) {
		reg_value = cam_io_r_mb(
			soc_info->reg_map[camnoc_index].mem_base +
			camnoc_info->error_logger[i]);
		pr_err("ErrorLogger[%d] : 0x%x\n", i, reg_value);
	}

	return 0;
}

static int cam_cpastop_handle_ubwc_err(struct cam_cpas *cpas_core,
	struct cam_hw_soc_info *soc_info, int i)
{
	uint32_t reg_value;
	int camnoc_index = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];

	reg_value = cam_io_r_mb(soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->irq_err[i].err_status.offset);

	pr_err("Dumping ubwc error status : 0x%x\n", reg_value);

	return 0;
}

static int cam_cpastop_handle_ahb_timeout_err(struct cam_hw_info *cpas_hw)
{
	pr_err("ahb timout error\n");

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

irqreturn_t cam_cpastop_handle_irq(int irq_num, void *data)
{
	uint32_t irq_status;
	struct cam_hw_info *cpas_hw = (struct cam_hw_info *)data;
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	int camnoc_index = cpas_core->regbase_index[CAM_CPAS_REG_CAMNOC];
	int i;
	enum cam_camnoc_hw_irq_type irq_type;

	irq_status = cam_io_r_mb(soc_info->reg_map[camnoc_index].mem_base +
		camnoc_info->irq_sbm->sbm_status.offset);

	pr_err("IRQ callback, irq_status=0x%x\n", irq_status);

	for (i = 0; i < camnoc_info->irq_err_size; i++) {
		if ((irq_status & camnoc_info->irq_err[i].sbm_port) &&
			(camnoc_info->irq_err[i].enable)) {
			irq_type = camnoc_info->irq_err[i].irq_type;
			pr_err("Error occurred, type=%d\n", irq_type);

			switch (irq_type) {
			case CAM_CAMNOC_HW_IRQ_SLAVE_ERROR:
				cam_cpastop_handle_errlogger(cpas_core,
					soc_info);
				break;
			case CAM_CAMNOC_HW_IRQ_IFE02_UBWC_ENCODE_ERROR:
			case CAM_CAMNOC_HW_IRQ_IFE13_UBWC_ENCODE_ERROR:
			case CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_DECODE_ERROR:
			case CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_ENCODE_ERROR:
				cam_cpastop_handle_ubwc_err(cpas_core,
					soc_info, i);
				break;
			case CAM_CAMNOC_HW_IRQ_AHB_TIMEOUT:
				cam_cpastop_handle_ahb_timeout_err(cpas_hw);
				break;
			case CAM_CAMNOC_HW_IRQ_CAMNOC_TEST:
				CPAS_CDBG("TEST IRQ\n");
				break;
			default:
				break;
			}

			irq_status &= ~camnoc_info->irq_err[i].sbm_port;
		}
	}

	if (irq_status)
		pr_err("IRQ not handled, irq_status=0x%x\n", irq_status);

	if (TEST_IRQ_ENABLE)
		cam_cpastop_disable_test_irq(cpas_hw);

	cam_cpastop_reset_irq(cpas_hw);

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
			pr_err("camnoc flush slave pending trans failed\n");
			/* Do not return error, passthrough */
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
		} else {
			pr_err("CPAS Version not supported %d.%d.%d\n",
				hw_caps->cpas_version.major,
				hw_caps->cpas_version.minor,
				hw_caps->cpas_version.incr);
			return -EINVAL;
		}
	} else {
		pr_err("Camera Version not supported %d.%d.%d\n",
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
		pr_err("invalid NULL param\n");
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
