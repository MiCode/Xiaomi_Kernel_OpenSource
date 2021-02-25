// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/slab.h>

#include "cam_compat.h"
#include "cam_cpas_hw_intf.h"
#include "cam_cpas_hw.h"
#include "cam_cpastop_hw.h"
#include "cam_io_util.h"
#include "cam_cpas_soc.h"
#include "cpastop100.h"
#include "cpastop_v150_100.h"
#include "cpastop_v170_200.h"
#include "cpastop_v170_110.h"
#include "cpastop_v175_100.h"
#include "cpastop_v175_101.h"
#include "cpastop_v175_120.h"
#include "cpastop_v175_130.h"
#include "cpastop_v480_100.h"
#include "cpastop_v480_custom.h"
#include "cpastop_v580_100.h"
#include "cpastop_v580_custom.h"
#include "cpastop_v540_100.h"
#include "cpastop_v520_100.h"
#include "cpastop_v545_100.h"
#include "cpastop_v570_200.h"
#include "cpastop_v680_100.h"
#include "cam_req_mgr_workq.h"

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

static const uint32_t cam_cpas_hw_version_map
	[CAM_CPAS_CAMERA_VERSION_ID_MAX][CAM_CPAS_VERSION_ID_MAX] = {
	/* for camera_150 */
	{
		CAM_CPAS_TITAN_150_V100,
		0,
		0,
		0,
		0,
		0,
	},
	/* for camera_170 */
	{
		CAM_CPAS_TITAN_170_V100,
		0,
		CAM_CPAS_TITAN_170_V110,
		CAM_CPAS_TITAN_170_V120,
		0,
		CAM_CPAS_TITAN_170_V200,
	},
	/* for camera_175 */
	{
		CAM_CPAS_TITAN_175_V100,
		CAM_CPAS_TITAN_175_V101,
		0,
		CAM_CPAS_TITAN_175_V120,
		CAM_CPAS_TITAN_175_V130,
		0,
	},
	/* for camera_480 */
	{
		CAM_CPAS_TITAN_480_V100,
		0,
		0,
		0,
		0,
		0,
	},
	/* for camera_580 */
	{
		CAM_CPAS_TITAN_580_V100,
		0,
		0,
		0,
		0,
		0,
	},
	/* for camera_520 */
	{
		CAM_CPAS_TITAN_520_V100,
		0,
		0,
		0,
		0,
		0,

	},
	/* for camera_540 */
	{
		CAM_CPAS_TITAN_540_V100,
		0,
		0,
		0,
		0,
		0,
	},
	/* for camera_545 */
	{
		CAM_CPAS_TITAN_545_V100,
		0,
		0,
		0,
		0,
		0,
	},
	/* for camera_570 */
	{
		0,
		0,
		0,
		0,
		0,
		CAM_CPAS_TITAN_570_V200,
	},
};

static int cam_cpas_translate_camera_cpas_version_id(
	uint32_t cam_version,
	uint32_t cpas_version,
	uint32_t *cam_version_id,
	uint32_t *cpas_version_id)
{

	switch (cam_version) {

	case CAM_CPAS_CAMERA_VERSION_150:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_150;
		break;

	case CAM_CPAS_CAMERA_VERSION_170:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_170;
		break;

	case CAM_CPAS_CAMERA_VERSION_175:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_175;
		break;

	case CAM_CPAS_CAMERA_VERSION_480:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_480;
		break;

	case CAM_CPAS_CAMERA_VERSION_520:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_520;
		break;

	case CAM_CPAS_CAMERA_VERSION_540:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_540;
		break;

	case CAM_CPAS_CAMERA_VERSION_580:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_580;
		break;

	case CAM_CPAS_CAMERA_VERSION_545:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_545;
		break;

	case CAM_CPAS_CAMERA_VERSION_570:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_570;
		break;

	case CAM_CPAS_CAMERA_VERSION_680:
		*cam_version_id = CAM_CPAS_CAMERA_VERSION_ID_680;
		break;

	default:
		CAM_ERR(CAM_CPAS, "Invalid cam version %u",
			cam_version);
		return -EINVAL;
	}

	switch (cpas_version) {

	case CAM_CPAS_VERSION_100:
		*cpas_version_id = CAM_CPAS_VERSION_ID_100;
		break;

	case CAM_CPAS_VERSION_101:
		*cpas_version_id = CAM_CPAS_VERSION_ID_101;
		break;
	case CAM_CPAS_VERSION_110:
		*cpas_version_id = CAM_CPAS_VERSION_ID_110;
		break;

	case CAM_CPAS_VERSION_120:
		*cpas_version_id = CAM_CPAS_VERSION_ID_120;
		break;

	case CAM_CPAS_VERSION_130:
		*cpas_version_id = CAM_CPAS_VERSION_ID_130;
		break;

	case CAM_CPAS_VERSION_200:
		*cpas_version_id = CAM_CPAS_VERSION_ID_200;
		break;

	default:
		CAM_ERR(CAM_CPAS, "Invalid cpas version %u",
			cpas_version);
		return -EINVAL;
	}
	return 0;
}

static int cam_cpastop_get_hw_info(struct cam_hw_info *cpas_hw,
	struct cam_cpas_hw_caps *hw_caps)
{
	struct cam_cpas *cpas_core = (struct cam_cpas *) cpas_hw->core_info;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;
	int32_t reg_indx = cpas_core->regbase_index[CAM_CPAS_REG_CPASTOP];
	uint32_t reg_value;
	uint32_t cam_version, cpas_version;
	uint32_t cam_version_id, cpas_version_id;
	int rc;

	if (reg_indx == -1)
		return -EINVAL;

	hw_caps->camera_family = CAM_FAMILY_CPAS_SS;

	cam_version = cam_io_r_mb(soc_info->reg_map[reg_indx].mem_base + 0x0);
	hw_caps->camera_version.major =
		CAM_BITS_MASK_SHIFT(cam_version, 0xff0000, 0x10);
	hw_caps->camera_version.minor =
		CAM_BITS_MASK_SHIFT(cam_version, 0xff00, 0x8);
	hw_caps->camera_version.incr =
		CAM_BITS_MASK_SHIFT(cam_version, 0xff, 0x0);

	cpas_version = cam_io_r_mb(soc_info->reg_map[reg_indx].mem_base + 0x4);
	hw_caps->cpas_version.major =
		CAM_BITS_MASK_SHIFT(cpas_version, 0xf0000000, 0x1c);
	hw_caps->cpas_version.minor =
		CAM_BITS_MASK_SHIFT(cpas_version, 0xfff0000, 0x10);
	hw_caps->cpas_version.incr =
		CAM_BITS_MASK_SHIFT(cpas_version, 0xffff, 0x0);

	reg_value = cam_io_r_mb(soc_info->reg_map[reg_indx].mem_base + 0x8);
	hw_caps->camera_capability = reg_value;

	CAM_DBG(CAM_FD, "Family %d, version %d.%d.%d, cpas %d.%d.%d, cap 0x%x",
		hw_caps->camera_family, hw_caps->camera_version.major,
		hw_caps->camera_version.minor, hw_caps->camera_version.incr,
		hw_caps->cpas_version.major, hw_caps->cpas_version.minor,
		hw_caps->cpas_version.incr, hw_caps->camera_capability);

	soc_info->hw_version = CAM_CPAS_TITAN_NONE;
	rc  = cam_cpas_translate_camera_cpas_version_id(cam_version,
		cpas_version, &cam_version_id, &cpas_version_id);
	if (rc) {
		CAM_ERR(CAM_CPAS, "Invalid Version, Camera: 0x%x CPAS: 0x%x",
			cam_version, cpas_version);
		return -EINVAL;
	}

	soc_info->hw_version =
		cam_cpas_hw_version_map[cam_version_id][cpas_version_id];
	CAM_DBG(CAM_CPAS, "CPAS HW VERSION %x", soc_info->hw_version);

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
	} else if (rc) {
		CAM_ERR(CAM_CPAS, "failed to get index for CPASTOP rc=%d", rc);
		return rc;
	} else {
		CAM_ERR(CAM_CPAS, "regbase not found for CPASTOP, rc=%d, %d %d",
			rc, index, num_reg_map);
		return -EINVAL;
	}

	rc = cam_common_util_get_string_index(soc_info->mem_block_name,
		soc_info->num_mem_block, "cam_camnoc", &index);
	if ((rc == 0) && (index < num_reg_map)) {
		regbase_index[CAM_CPAS_REG_CAMNOC] = index;
	} else if (rc) {
		CAM_ERR(CAM_CPAS, "failed to get index for CAMNOC rc=%d", rc);
		return rc;
	} else {
		CAM_ERR(CAM_CPAS, "regbase not found for CAMNOC, rc=%d, %d %d",
			rc, index, num_reg_map);
		return -EINVAL;
	}

	/* optional - rpmh register map */
	rc = cam_common_util_get_string_index(soc_info->mem_block_name,
		soc_info->num_mem_block, "cam_rpmh", &index);
	if ((rc == 0) && (index < num_reg_map)) {
		regbase_index[CAM_CPAS_REG_RPMH] = index;
		CAM_DBG(CAM_CPAS, "regbase found for RPMH, rc=%d, %d %d",
			rc, index, num_reg_map);
	} else {
		CAM_DBG(CAM_CPAS, "regbase not found for RPMH, rc=%d, %d %d",
			rc, index, num_reg_map);
		regbase_index[CAM_CPAS_REG_RPMH] = -1;
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
	CAM_ERR_RATE_LIMIT(CAM_CPAS, "ahb timeout error");

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

	if (camnoc_info->irq_sbm->sbm_enable.enable == false)
		return 0;

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

	cam_req_mgr_thread_switch_delay_detect(
			payload->workq_scheduled_ts);

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
			CAM_ERR_RATE_LIMIT(CAM_CPAS,
				"Error occurred, type=%d", irq_type);
			memset(&irq_data, 0x0, sizeof(irq_data));
			irq_data.irq_type = (enum cam_camnoc_irq_type)irq_type;

			switch (irq_type) {
			case CAM_CAMNOC_HW_IRQ_SLAVE_ERROR:
				cam_cpastop_handle_errlogger(
					cpas_core, soc_info,
					&irq_data.u.slave_err);
				break;
			case CAM_CAMNOC_HW_IRQ_IFE_UBWC_STATS_ENCODE_ERROR:
			case CAM_CAMNOC_HW_IRQ_IFE02_UBWC_ENCODE_ERROR:
			case CAM_CAMNOC_HW_IRQ_IFE13_UBWC_ENCODE_ERROR:
			case CAM_CAMNOC_HW_IRQ_IPE_BPS_UBWC_ENCODE_ERROR:
				cam_cpastop_handle_ubwc_enc_err(
					cpas_core, soc_info, i,
					&irq_data.u.enc_err);
				break;
			case CAM_CAMNOC_HW_IRQ_IPE1_BPS_UBWC_DECODE_ERROR:
			case CAM_CAMNOC_HW_IRQ_IPE0_UBWC_DECODE_ERROR:
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

	payload->workq_scheduled_ts = ktime_get();
	queue_work(cpas_core->work_queue, &payload->work);
done:
	atomic_dec(&cpas_core->irq_count);
	wake_up(&cpas_core->irq_count_wq);

	return IRQ_HANDLED;
}

static int cam_cpastop_print_poweron_settings(struct cam_hw_info *cpas_hw)
{
	int i;

	for (i = 0; i < camnoc_info->specific_size; i++) {
		if (camnoc_info->specific[i].enable) {
			CAM_INFO(CAM_CPAS, "Reading QoS settings for %d",
				camnoc_info->specific[i].port_type);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].priority_lut_low);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].priority_lut_high);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].urgency);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].danger_lut);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].safe_lut);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].ubwc_ctl);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].flag_out_set0_low);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].qosgen_mainctl);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].qosgen_shaping_low);
			cam_cpas_util_reg_read(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].qosgen_shaping_high);
		}
	}

	return 0;
}

static int cam_cpastop_poweron(struct cam_hw_info *cpas_hw)
{
	int i;
	struct cam_cpas_hw_errata_wa_list *errata_wa_list =
		camnoc_info->errata_wa_list;
	struct cam_cpas_hw_errata_wa *errata_wa;

	cam_cpastop_reset_irq(cpas_hw);
	for (i = 0; i < camnoc_info->specific_size; i++) {
		if (camnoc_info->specific[i].enable) {
			CAM_DBG(CAM_CPAS, "Updating QoS settings for %d",
				camnoc_info->specific[i].port_type);
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
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].qosgen_mainctl);
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].qosgen_shaping_low);
			cam_cpas_util_reg_update(cpas_hw, CAM_CPAS_REG_CAMNOC,
				&camnoc_info->specific[i].qosgen_shaping_high);
		}
	}

	if (errata_wa_list) {
		errata_wa = &errata_wa_list->tcsr_camera_hf_sf_ares_glitch;
		if (errata_wa->enable)
			cam_cpastop_scm_write(errata_wa);
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
	int rc = 0;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;

	CAM_DBG(CAM_CPAS,
		"hw_version=0x%x Camera Version %d.%d.%d, cpas version %d.%d.%d",
		soc_info->hw_version,
		hw_caps->camera_version.major,
		hw_caps->camera_version.minor,
		hw_caps->camera_version.incr,
		hw_caps->cpas_version.major,
		hw_caps->cpas_version.minor,
		hw_caps->cpas_version.incr);

	switch (soc_info->hw_version) {
	case CAM_CPAS_TITAN_170_V100:
		camnoc_info = &cam170_cpas100_camnoc_info;
		break;
	case CAM_CPAS_TITAN_170_V110:
		camnoc_info = &cam170_cpas110_camnoc_info;
		break;
	case CAM_CPAS_TITAN_170_V200:
		camnoc_info = &cam170_cpas200_camnoc_info;
		break;
	case CAM_CPAS_TITAN_175_V100:
		camnoc_info = &cam175_cpas100_camnoc_info;
		break;
	case CAM_CPAS_TITAN_175_V101:
		camnoc_info = &cam175_cpas101_camnoc_info;
		break;
	case CAM_CPAS_TITAN_175_V120:
		camnoc_info = &cam175_cpas120_camnoc_info;
		break;
	case CAM_CPAS_TITAN_175_V130:
		camnoc_info = &cam175_cpas130_camnoc_info;
		break;
	case CAM_CPAS_TITAN_150_V100:
		camnoc_info = &cam150_cpas100_camnoc_info;
		break;
	case CAM_CPAS_TITAN_480_V100:
		camnoc_info = &cam480_cpas100_camnoc_info;
		break;
	case CAM_CPAS_TITAN_580_V100:
		camnoc_info = &cam580_cpas100_camnoc_info;
		break;
	case CAM_CPAS_TITAN_540_V100:
		camnoc_info = &cam540_cpas100_camnoc_info;
		break;
	case CAM_CPAS_TITAN_520_V100:
		camnoc_info = &cam520_cpas100_camnoc_info;
		break;
	case CAM_CPAS_TITAN_545_V100:
		camnoc_info = &cam545_cpas100_camnoc_info;
		break;
	case CAM_CPAS_TITAN_570_V200:
		camnoc_info = &cam570_cpas200_camnoc_info;
		break;
	case CAM_CPAS_TITAN_680_V100:
		camnoc_info = &cam680_cpas100_camnoc_info;
		break;
	default:
		CAM_ERR(CAM_CPAS, "Camera Version not supported %d.%d.%d",
			hw_caps->camera_version.major,
			hw_caps->camera_version.minor,
			hw_caps->camera_version.incr);
		rc = -EINVAL;
		break;
	}

	return 0;
}

static int cam_cpastop_setup_qos_settings(struct cam_hw_info *cpas_hw,
	uint32_t selection_mask)
{
	int rc = 0;
	struct cam_hw_soc_info *soc_info = &cpas_hw->soc_info;

	CAM_DBG(CAM_CPAS,
		"QoS selection : hw_version=0x%x selection_mask 0x%x",
		soc_info->hw_version,
		selection_mask);

	switch (soc_info->hw_version) {
	case CAM_CPAS_TITAN_480_V100:
		if (selection_mask & CAM_CPAS_QOS_CUSTOM_SETTINGS_MASK)
			camnoc_info = &cam480_custom_camnoc_info;
		else if (selection_mask & CAM_CPAS_QOS_DEFAULT_SETTINGS_MASK)
			camnoc_info = &cam480_cpas100_camnoc_info;
		else
			CAM_ERR(CAM_CPAS, "Invalid selection mask 0x%x",
				selection_mask);
		break;
	case CAM_CPAS_TITAN_580_V100:
		if (selection_mask & CAM_CPAS_QOS_CUSTOM_SETTINGS_MASK)
			camnoc_info = &cam580_custom_camnoc_info;
		else if (selection_mask & CAM_CPAS_QOS_DEFAULT_SETTINGS_MASK)
			camnoc_info = &cam580_cpas100_camnoc_info;
		else
			CAM_ERR(CAM_CPAS,
				"Invalid selection mask 0x%x for hw 0x%x",
				selection_mask, soc_info->hw_version);
		break;
	case CAM_CPAS_TITAN_680_V100:
		if ((selection_mask & CAM_CPAS_QOS_CUSTOM_SETTINGS_MASK) ||
			(selection_mask & CAM_CPAS_QOS_DEFAULT_SETTINGS_MASK))
			camnoc_info = &cam680_cpas100_camnoc_info;
		else
			CAM_ERR(CAM_CPAS,
				"Invalid selection mask 0x%x for hw 0x%x",
				selection_mask, soc_info->hw_version);
		break;
	default:
		CAM_WARN(CAM_CPAS, "QoS selection not supported for 0x%x",
			soc_info->hw_version);
		rc = -EINVAL;
		break;
	}

	return rc;
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
	internal_ops->setup_qos_settings = cam_cpastop_setup_qos_settings;
	internal_ops->print_poweron_settings =
		cam_cpastop_print_poweron_settings;

	return 0;
}
