// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */


#include <linux/ratelimit.h>
#include <linux/slab.h>
#include <uapi/media/cam_isp.h>
#include "cam_io_util.h"
#include "cam_debug_util.h"
#include "cam_cdm_util.h"
#include "cam_hw_intf.h"
#include "cam_ife_hw_mgr.h"
#include "cam_vfe_hw_intf.h"
#include "cam_irq_controller.h"
#include "cam_tasklet_util.h"
#include "cam_vfe_bus.h"
#include "cam_vfe_bus_ver3.h"
#include "cam_vfe_core.h"
#include "cam_debug_util.h"
#include "cam_cpas_api.h"

static const char drv_name[] = "vfe_bus";

#define CAM_VFE_BUS_VER3_IRQ_REG0                0
#define CAM_VFE_BUS_VER3_IRQ_REG1                1
#define CAM_VFE_BUS_VER3_IRQ_MAX                 2

#define CAM_VFE_BUS_VER3_PAYLOAD_MAX             256

#define CAM_VFE_RDI_BUS_DEFAULT_WIDTH               0xFFFF
#define CAM_VFE_RDI_BUS_DEFAULT_STRIDE              0xFFFF
#define CAM_VFE_BUS_VER3_INTRA_CLIENT_MASK          0x3

#define MAX_BUF_UPDATE_REG_NUM   \
	((sizeof(struct cam_vfe_bus_ver3_reg_offset_bus_client) +  \
	sizeof(struct cam_vfe_bus_ver3_reg_offset_ubwc_client))/4)
#define MAX_REG_VAL_PAIR_SIZE    \
	(MAX_BUF_UPDATE_REG_NUM * 2 * CAM_PACKET_MAX_PLANES)

static uint32_t bus_error_irq_mask[2] = {
	0xC0000000,
	0x00000000,
};

static uint32_t rup_irq_mask[2] = {
	0x0000003F,
	0x00000000,
};

enum cam_vfe_bus_ver3_packer_format {
	PACKER_FMT_VER3_PLAIN_128,
	PACKER_FMT_VER3_PLAIN_8,
	PACKER_FMT_VER3_PLAIN_8_ODD_EVEN,
	PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10,
	PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10_ODD_EVEN,
	PACKER_FMT_VER3_PLAIN_16_10BPP,
	PACKER_FMT_VER3_PLAIN_16_12BPP,
	PACKER_FMT_VER3_PLAIN_16_14BPP,
	PACKER_FMT_VER3_PLAIN_16_16BPP,
	PACKER_FMT_VER3_PLAIN_32,
	PACKER_FMT_VER3_PLAIN_64,
	PACKER_FMT_VER3_TP_10,
	PACKER_FMT_VER3_MAX,
};

struct cam_vfe_bus_ver3_common_data {
	uint32_t                                    core_index;
	void __iomem                               *mem_base;
	struct cam_hw_intf                         *hw_intf;
	void                                       *bus_irq_controller;
	void                                       *vfe_irq_controller;
	struct cam_vfe_bus_ver3_reg_offset_common  *common_reg;
	uint32_t                                    io_buf_update[
		MAX_REG_VAL_PAIR_SIZE];

	struct cam_vfe_bus_irq_evt_payload          evt_payload[
		CAM_VFE_BUS_VER3_PAYLOAD_MAX];
	struct list_head                            free_payload_list;
	spinlock_t                                  spin_lock;
	struct mutex                                bus_mutex;
	uint32_t                                    secure_mode;
	uint32_t                                    num_sec_out;
	uint32_t                                    addr_no_sync;
	bool                                        is_lite;
};

struct cam_vfe_bus_ver3_wm_resource_data {
	uint32_t             index;
	struct cam_vfe_bus_ver3_common_data            *common_data;
	struct cam_vfe_bus_ver3_reg_offset_bus_client  *hw_regs;
	void                                           *ctx;

	bool                 init_cfg_done;
	bool                 hfr_cfg_done;

	uint32_t             offset;
	uint32_t             width;
	uint32_t             height;
	uint32_t             stride;
	uint32_t             format;
	enum cam_vfe_bus_ver3_packer_format pack_fmt;

	uint32_t             burst_len;

	uint32_t             en_ubwc;
	bool                 ubwc_updated;
	uint32_t             packer_cfg;
	uint32_t             h_init;
	uint32_t             ubwc_meta_addr;
	uint32_t             ubwc_meta_cfg;
	uint32_t             ubwc_mode_cfg;
	uint32_t             ubwc_stats_ctrl;
	uint32_t             ubwc_ctrl_2;

	uint32_t             irq_subsample_period;
	uint32_t             irq_subsample_pattern;
	uint32_t             framedrop_period;
	uint32_t             framedrop_pattern;

	uint32_t             en_cfg;
	uint32_t             is_dual;
};

struct cam_vfe_bus_ver3_comp_grp_data {
	enum cam_vfe_bus_ver3_comp_grp_type          comp_grp_type;
	struct cam_vfe_bus_ver3_common_data         *common_data;
	struct cam_vfe_bus_ver3_reg_offset_comp_grp *hw_regs;

	uint32_t                                     irq_enabled;

	uint32_t                                     is_master;
	uint32_t                                     is_dual;
	uint32_t                                     dual_slave_core;
	uint32_t                                     intra_client_mask;
	uint32_t                                     addr_sync_mode;

	uint32_t                                     acquire_dev_cnt;
	uint32_t                                     irq_trigger_cnt;

	void                                        *ctx;
};

struct cam_vfe_bus_ver3_vfe_out_data {
	uint32_t                              out_type;
	struct cam_vfe_bus_ver3_common_data  *common_data;

	uint32_t                         num_wm;
	struct cam_isp_resource_node    *wm_res[PLANE_MAX];

	struct cam_isp_resource_node    *comp_grp;
	enum cam_isp_hw_sync_mode        dual_comp_sync_mode;
	uint32_t                         dual_hw_alternate_vfe_id;
	struct list_head                 vfe_out_list;

	uint32_t                         format;
	uint32_t                         max_width;
	uint32_t                         max_height;
	struct cam_cdm_utils_ops        *cdm_util_ops;
	uint32_t                         secure_mode;
};

struct cam_vfe_bus_ver3_priv {
	struct cam_vfe_bus_ver3_common_data common_data;
	uint32_t                            num_client;
	uint32_t                            num_out;

	struct cam_isp_resource_node  bus_client[CAM_VFE_BUS_VER3_MAX_CLIENTS];
	struct cam_isp_resource_node  comp_grp[CAM_VFE_BUS_VER3_COMP_GRP_MAX];
	struct cam_isp_resource_node  vfe_out[CAM_VFE_BUS_VER3_VFE_OUT_MAX];

	struct list_head                    free_comp_grp;
	struct list_head                    used_comp_grp;

	uint32_t                            irq_handle;
	uint32_t                            error_irq_handle;
	uint32_t                            rup_irq_handle;
	void                               *tasklet_info;
};

static int cam_vfe_bus_ver3_process_cmd(
	struct cam_isp_resource_node *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size);

static int cam_vfe_bus_ver3_get_evt_payload(
	struct cam_vfe_bus_ver3_common_data  *common_data,
	struct cam_vfe_bus_irq_evt_payload  **evt_payload)
{
	int rc;

	spin_lock(&common_data->spin_lock);
	if (list_empty(&common_data->free_payload_list)) {
		*evt_payload = NULL;
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No free payload");
		rc = -ENODEV;
		goto done;
	}

	*evt_payload = list_first_entry(&common_data->free_payload_list,
		struct cam_vfe_bus_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	rc = 0;
done:
	spin_unlock(&common_data->spin_lock);
	return rc;
}

static int cam_vfe_bus_ver3_put_evt_payload(void     *core_info,
	struct cam_vfe_bus_irq_evt_payload     **evt_payload)
{
	struct cam_vfe_bus_ver3_common_data *common_data = NULL;
	uint32_t  *ife_irq_regs = NULL;
	uint32_t   status_reg0, status_reg1;
	unsigned long flags;

	if (!core_info) {
		CAM_ERR(CAM_ISP, "Invalid param core_info NULL");
		return -EINVAL;
	}
	if (*evt_payload == NULL) {
		CAM_ERR(CAM_ISP, "No payload to put");
		return -EINVAL;
	}
	(*evt_payload)->error_type = 0;
	ife_irq_regs = (*evt_payload)->irq_reg_val;
	status_reg0 = ife_irq_regs[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0];
	status_reg1 = ife_irq_regs[CAM_IFE_IRQ_BUS_VER3_REG_STATUS1];

	if (status_reg0 || status_reg1) {
		CAM_DBG(CAM_ISP, "status0 0x%x status1 0x%x",
			status_reg0, status_reg1);
		return 0;
	}

	common_data = core_info;

	spin_lock_irqsave(&common_data->spin_lock, flags);
	list_add_tail(&(*evt_payload)->list,
		&common_data->free_payload_list);
	spin_unlock_irqrestore(&common_data->spin_lock, flags);

	*evt_payload = NULL;

	CAM_DBG(CAM_ISP, "Done");
	return 0;
}

static int cam_vfe_bus_ver3_get_intra_client_mask(
	enum cam_vfe_bus_ver3_vfe_core_id  dual_slave_core,
	enum cam_vfe_bus_ver3_vfe_core_id  current_core,
	uint32_t                          *intra_client_mask)
{
	int rc = 0;
	uint32_t version_based_intra_client_mask = 0x1;

	*intra_client_mask = 0;

	if (dual_slave_core == current_core) {
		CAM_ERR(CAM_ISP,
			"Invalid params. Same core as Master and Slave");
		return -EINVAL;
	}

	switch (current_core) {
	case CAM_VFE_BUS_VER3_VFE_CORE_0:
		switch (dual_slave_core) {
		case CAM_VFE_BUS_VER3_VFE_CORE_1:
			*intra_client_mask = version_based_intra_client_mask;
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid value for slave core %u",
				dual_slave_core);
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_CORE_1:
		switch (dual_slave_core) {
		case CAM_VFE_BUS_VER3_VFE_CORE_0:
			*intra_client_mask = version_based_intra_client_mask;
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid value for slave core %u",
				dual_slave_core);
			rc = -EINVAL;
			break;
		}
		break;
	default:
		CAM_ERR(CAM_ISP,
			"Invalid value for master core %u", current_core);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static bool cam_vfe_bus_ver3_can_be_secure(uint32_t out_type)
{
	switch (out_type) {
	case CAM_VFE_BUS_VER3_VFE_OUT_FULL:
	case CAM_VFE_BUS_VER3_VFE_OUT_DS4:
	case CAM_VFE_BUS_VER3_VFE_OUT_DS16:
	case CAM_VFE_BUS_VER3_VFE_OUT_FD:
	case CAM_VFE_BUS_VER3_VFE_OUT_RAW_DUMP:
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI0:
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI1:
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI2:
	case CAM_VFE_BUS_VER3_VFE_OUT_FULL_DISP:
	case CAM_VFE_BUS_VER3_VFE_OUT_DS4_DISP:
	case CAM_VFE_BUS_VER3_VFE_OUT_DS16_DISP:
		return true;

	case CAM_VFE_BUS_VER3_VFE_OUT_PDAF:
	case CAM_VFE_BUS_VER3_VFE_OUT_2PD:
	case CAM_VFE_BUS_VER3_VFE_OUT_LCR:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BE:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BHIST:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_TL_BG:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_BF:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_AWB_BG:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_BHIST:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_RS:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_CS:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_IHIST:
	default:
		return false;
	}
}

static enum cam_vfe_bus_ver3_vfe_out_type
	cam_vfe_bus_ver3_get_out_res_id(uint32_t res_type)
{
	switch (res_type) {
	case CAM_ISP_IFE_OUT_RES_FULL:
		return CAM_VFE_BUS_VER3_VFE_OUT_FULL;
	case CAM_ISP_IFE_OUT_RES_DS4:
		return CAM_VFE_BUS_VER3_VFE_OUT_DS4;
	case CAM_ISP_IFE_OUT_RES_DS16:
		return CAM_VFE_BUS_VER3_VFE_OUT_DS16;
	case CAM_ISP_IFE_OUT_RES_FD:
		return CAM_VFE_BUS_VER3_VFE_OUT_FD;
	case CAM_ISP_IFE_OUT_RES_RAW_DUMP:
		return CAM_VFE_BUS_VER3_VFE_OUT_RAW_DUMP;
	case CAM_ISP_IFE_OUT_RES_PDAF:
		return CAM_VFE_BUS_VER3_VFE_OUT_PDAF;
	case CAM_ISP_IFE_OUT_RES_2PD:
		return CAM_VFE_BUS_VER3_VFE_OUT_2PD;
	case CAM_ISP_IFE_OUT_RES_RDI_0:
		return CAM_VFE_BUS_VER3_VFE_OUT_RDI0;
	case CAM_ISP_IFE_OUT_RES_RDI_1:
		return CAM_VFE_BUS_VER3_VFE_OUT_RDI1;
	case CAM_ISP_IFE_OUT_RES_RDI_2:
		return CAM_VFE_BUS_VER3_VFE_OUT_RDI2;
	case CAM_ISP_IFE_OUT_RES_RDI_3:
		return CAM_VFE_BUS_VER3_VFE_OUT_RDI3;
	case CAM_ISP_IFE_OUT_RES_STATS_HDR_BE:
		return CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BE;
	case CAM_ISP_IFE_OUT_RES_STATS_HDR_BHIST:
		return CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BHIST;
	case CAM_ISP_IFE_OUT_RES_STATS_TL_BG:
		return CAM_VFE_BUS_VER3_VFE_OUT_STATS_TL_BG;
	case CAM_ISP_IFE_OUT_RES_STATS_BF:
		return CAM_VFE_BUS_VER3_VFE_OUT_STATS_BF;
	case CAM_ISP_IFE_OUT_RES_STATS_AWB_BG:
		return CAM_VFE_BUS_VER3_VFE_OUT_STATS_AWB_BG;
	case CAM_ISP_IFE_OUT_RES_STATS_BHIST:
		return CAM_VFE_BUS_VER3_VFE_OUT_STATS_BHIST;
	case CAM_ISP_IFE_OUT_RES_STATS_RS:
		return CAM_VFE_BUS_VER3_VFE_OUT_STATS_RS;
	case CAM_ISP_IFE_OUT_RES_STATS_CS:
		return CAM_VFE_BUS_VER3_VFE_OUT_STATS_CS;
	case CAM_ISP_IFE_OUT_RES_STATS_IHIST:
		return CAM_VFE_BUS_VER3_VFE_OUT_STATS_IHIST;
	case CAM_ISP_IFE_OUT_RES_FULL_DISP:
		return CAM_VFE_BUS_VER3_VFE_OUT_FULL_DISP;
	case CAM_ISP_IFE_OUT_RES_DS4_DISP:
		return CAM_VFE_BUS_VER3_VFE_OUT_DS4_DISP;
	case CAM_ISP_IFE_OUT_RES_DS16_DISP:
		return CAM_VFE_BUS_VER3_VFE_OUT_DS16_DISP;
	case CAM_ISP_IFE_OUT_RES_LCR:
		return CAM_VFE_BUS_VER3_VFE_OUT_LCR;
	default:
		return CAM_VFE_BUS_VER3_VFE_OUT_MAX;
	}
}

static int cam_vfe_bus_ver3_get_num_wm(
	enum cam_vfe_bus_ver3_vfe_out_type    res_type,
	uint32_t                              format)
{
	switch (res_type) {
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI0:
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI1:
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI2:
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI3:
		switch (format) {
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_MIPI_RAW_12:
		case CAM_FORMAT_MIPI_RAW_14:
		case CAM_FORMAT_MIPI_RAW_16:
		case CAM_FORMAT_MIPI_RAW_20:
		case CAM_FORMAT_DPCM_10_6_10:
		case CAM_FORMAT_DPCM_10_8_10:
		case CAM_FORMAT_DPCM_12_6_12:
		case CAM_FORMAT_DPCM_12_8_12:
		case CAM_FORMAT_DPCM_14_8_14:
		case CAM_FORMAT_DPCM_14_10_14:
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_PLAIN16_16:
		case CAM_FORMAT_PLAIN32_20:
		case CAM_FORMAT_PLAIN128:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_FULL:
	case CAM_VFE_BUS_VER3_VFE_OUT_FULL_DISP:
		switch (format) {
		case CAM_FORMAT_NV21:
		case CAM_FORMAT_NV12:
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_TP10:
		case CAM_FORMAT_UBWC_NV12:
		case CAM_FORMAT_UBWC_NV12_4R:
		case CAM_FORMAT_UBWC_TP10:
		case CAM_FORMAT_UBWC_P010:
		case CAM_FORMAT_PLAIN16_10:
			return 2;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_FD:
		switch (format) {
		case CAM_FORMAT_NV21:
		case CAM_FORMAT_NV12:
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_TP10:
		case CAM_FORMAT_PLAIN16_10:
			return 2;
		case CAM_FORMAT_Y_ONLY:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_DS4:
	case CAM_VFE_BUS_VER3_VFE_OUT_DS4_DISP:
	case CAM_VFE_BUS_VER3_VFE_OUT_DS16:
	case CAM_VFE_BUS_VER3_VFE_OUT_DS16_DISP:
		switch (format) {
		case CAM_FORMAT_PD8:
		case CAM_FORMAT_PD10:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_RAW_DUMP:
		switch (format) {
		case CAM_FORMAT_ARGB_14:
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_PDAF:
		switch (format) {
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_2PD:
		switch (format) {
		case CAM_FORMAT_PLAIN16_8:
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_PLAIN16_16:
		case CAM_FORMAT_PLAIN64:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BE:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BHIST:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_TL_BG:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_BF:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_AWB_BG:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_BHIST:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_CS:
		switch (format) {
		case CAM_FORMAT_PLAIN64:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_RS:
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_IHIST:
		switch (format) {
		case CAM_FORMAT_PLAIN16_16:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_LCR:
		return 1;
	default:
		break;
	}

	CAM_ERR(CAM_ISP, "Unsupported format %u for resource_type %u",
		format, res_type);

	return -EINVAL;
}

static int cam_vfe_bus_ver3_get_wm_idx(
	enum cam_vfe_bus_ver3_vfe_out_type vfe_out_res_id,
	enum cam_vfe_bus_plane_type plane,
	bool is_lite)
{
	int wm_idx = -1;

	switch (vfe_out_res_id) {
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI0:
		switch (plane) {
		case PLANE_Y:
			if (is_lite)
				wm_idx = 23;
			else
				wm_idx = 0;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI1:
		switch (plane) {
		case PLANE_Y:
			if (is_lite)
				wm_idx = 24;
			else
				wm_idx = 1;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI2:
		switch (plane) {
		case PLANE_Y:
			if (is_lite)
				wm_idx = 25;
			else
				wm_idx = 2;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_RDI3:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 3;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_FULL:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 0;
			break;
		case PLANE_C:
			wm_idx = 1;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_DS4:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 2;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_DS16:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 3;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_FD:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 8;
			break;
		case PLANE_C:
			wm_idx = 9;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_RAW_DUMP:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 10;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_PDAF:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 21;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_2PD:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 11;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BE:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 12;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_HDR_BHIST:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 13;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_TL_BG:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 14;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_BF:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 20;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_AWB_BG:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 15;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_BHIST:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 16;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_RS:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 17;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_CS:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 18;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_STATS_IHIST:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 19;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_FULL_DISP:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 4;
			break;
		case PLANE_C:
			wm_idx = 5;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_DS4_DISP:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 6;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_DS16_DISP:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 7;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER3_VFE_OUT_LCR:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 22;
			break;
		default:
			break;
		}
	default:
		break;
	}

	return wm_idx;
}

static enum cam_vfe_bus_ver3_packer_format
	cam_vfe_bus_ver3_get_packer_fmt(uint32_t out_fmt, int wm_index)
{
	switch (out_fmt) {
	case CAM_FORMAT_MIPI_RAW_6:
	case CAM_FORMAT_MIPI_RAW_8:
	case CAM_FORMAT_MIPI_RAW_10:
	case CAM_FORMAT_MIPI_RAW_12:
	case CAM_FORMAT_MIPI_RAW_14:
	case CAM_FORMAT_MIPI_RAW_16:
	case CAM_FORMAT_MIPI_RAW_20:
	case CAM_FORMAT_PLAIN16_8:
	case CAM_FORMAT_PLAIN128:
	case CAM_FORMAT_PD8:
		return PACKER_FMT_VER3_PLAIN_128;
	case CAM_FORMAT_PLAIN8:
		return PACKER_FMT_VER3_PLAIN_8;
	case CAM_FORMAT_NV21:
		if ((wm_index == 1) || (wm_index == 3) || (wm_index == 5))
			return PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10_ODD_EVEN;
	case CAM_FORMAT_NV12:
	case CAM_FORMAT_UBWC_NV12:
	case CAM_FORMAT_UBWC_NV12_4R:
	case CAM_FORMAT_Y_ONLY:
		return PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10;
	case CAM_FORMAT_PLAIN16_10:
		return PACKER_FMT_VER3_PLAIN_16_10BPP;
	case CAM_FORMAT_PLAIN16_12:
		return PACKER_FMT_VER3_PLAIN_16_12BPP;
	case CAM_FORMAT_PLAIN16_14:
		return PACKER_FMT_VER3_PLAIN_16_14BPP;
	case CAM_FORMAT_PLAIN16_16:
		return PACKER_FMT_VER3_PLAIN_16_16BPP;
	case CAM_FORMAT_PLAIN32:
	case CAM_FORMAT_ARGB:
		return PACKER_FMT_VER3_PLAIN_32;
	case CAM_FORMAT_PLAIN64:
	case CAM_FORMAT_ARGB_16:
	case CAM_FORMAT_PD10:
		return PACKER_FMT_VER3_PLAIN_64;
	case CAM_FORMAT_UBWC_TP10:
	case CAM_FORMAT_TP10:
		return PACKER_FMT_VER3_TP_10;
	default:
		return PACKER_FMT_VER3_MAX;
	}
}

static int cam_vfe_bus_ver3_handle_rup_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int32_t                                     rc;
	int                                         i;
	struct cam_vfe_bus_ver3_priv               *bus_priv;
	struct cam_vfe_bus_irq_evt_payload         *evt_payload;

	bus_priv = th_payload->handler_priv;
	if (!bus_priv) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No resource");
		return -ENODEV;
	}

	CAM_DBG(CAM_ISP, "bus_IRQ status_0 = 0x%x, bus_IRQ status_1 = 0x%x",
		th_payload->evt_status_arr[0],
		th_payload->evt_status_arr[1]);

	rc  = cam_vfe_bus_ver3_get_evt_payload(&bus_priv->common_data,
		&evt_payload);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"No tasklet_cmd is free in queue");
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"IRQ status_0 = 0x%x status_1 = 0x%x",
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[1]);

		return rc;
	}

	evt_payload->core_index = bus_priv->common_data.core_index;
	evt_payload->evt_id  = evt_id;
	evt_payload->ctx = &bus_priv->common_data;
	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];
	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static int cam_vfe_bus_ver3_handle_rup_bottom_half(void *handler_priv,
	void *evt_payload_priv)
{
	int                                   ret = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_vfe_bus_irq_evt_payload   *payload;
	uint32_t                              irq_status0;

	if (!handler_priv || !evt_payload_priv) {
		CAM_ERR(CAM_ISP, "Invalid params");
		return ret;
	}

	payload = evt_payload_priv;
	irq_status0 = payload->irq_reg_val[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0];

	if (irq_status0 & 0x01) {
		CAM_DBG(CAM_ISP, "Received REG_UPDATE_ACK");
		ret = CAM_VFE_IRQ_STATUS_SUCCESS;
	}
	CAM_DBG(CAM_ISP,
		"event ID:%d, bus_irq_status_0 = 0x%x returning status = %d",
		payload->evt_id, irq_status0, ret);

	if (ret == CAM_VFE_IRQ_STATUS_SUCCESS)
		cam_vfe_bus_ver3_put_evt_payload(payload->ctx, &payload);

	return ret;
}

static int cam_vfe_bus_ver3_acquire_wm(
	struct cam_vfe_bus_ver3_priv          *ver3_bus_priv,
	struct cam_isp_out_port_info          *out_port_info,
	void                                  *tasklet,
	void                                  *ctx,
	enum cam_vfe_bus_ver3_vfe_out_type     vfe_out_res_id,
	enum cam_vfe_bus_plane_type            plane,
	struct cam_isp_resource_node         **wm_res,
	uint32_t                               is_dual,
	enum cam_vfe_bus_ver3_comp_grp_type   *comp_grp_id)
{
	uint32_t wm_idx = 0;
	struct cam_isp_resource_node              *wm_res_local = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data  *rsrc_data = NULL;

	*wm_res = NULL;

	/* No need to allocate for BUS VER2. VFE OUT to WM is fixed. */
	wm_idx = cam_vfe_bus_ver3_get_wm_idx(vfe_out_res_id, plane,
		ver3_bus_priv->common_data.is_lite);
	if (wm_idx < 0 || wm_idx >= ver3_bus_priv->num_client) {
		CAM_ERR(CAM_ISP, "Unsupported VFE out %d plane %d",
			vfe_out_res_id, plane);
		return -EINVAL;
	}

	wm_res_local = &ver3_bus_priv->bus_client[wm_idx];
	if (wm_res_local->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "WM:%d not available state:%d",
			wm_idx, wm_res_local->res_state);
		return -EALREADY;
	}
	wm_res_local->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	wm_res_local->tasklet_info = tasklet;

	rsrc_data = wm_res_local->res_priv;
	rsrc_data->ctx = ctx;
	rsrc_data->format = out_port_info->format;
	rsrc_data->pack_fmt = cam_vfe_bus_ver3_get_packer_fmt(rsrc_data->format,
		wm_idx);

	rsrc_data->width = out_port_info->width;
	rsrc_data->height = out_port_info->height;
	rsrc_data->is_dual = is_dual;
	/* Set WM offset value to default */
	rsrc_data->offset  = 0;
	CAM_DBG(CAM_ISP, "WM:%d width %d height %d", rsrc_data->index,
		rsrc_data->width, rsrc_data->height);

	if (ver3_bus_priv->common_data.is_lite)
		goto rdi_config;

	if (rsrc_data->index > 22) {
rdi_config:
		/* WM 23-25 refers to RDI 0/ RDI 1/RDI 2 */
		switch (rsrc_data->format) {
		case CAM_FORMAT_MIPI_RAW_6:
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_MIPI_RAW_12:
		case CAM_FORMAT_MIPI_RAW_14:
		case CAM_FORMAT_MIPI_RAW_16:
		case CAM_FORMAT_MIPI_RAW_20:
		case CAM_FORMAT_PLAIN128:
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->pack_fmt = 0x0;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
			break;
		case CAM_FORMAT_PLAIN8:
			rsrc_data->en_cfg = 0x1;
			rsrc_data->pack_fmt = 0x1;
			rsrc_data->stride = rsrc_data->width * 2;
			break;
		case CAM_FORMAT_PLAIN16_10:
		case CAM_FORMAT_PLAIN16_12:
		case CAM_FORMAT_PLAIN16_14:
		case CAM_FORMAT_PLAIN16_16:
		case CAM_FORMAT_PLAIN32_20:
			rsrc_data->width = CAM_VFE_RDI_BUS_DEFAULT_WIDTH;
			rsrc_data->height = 0;
			rsrc_data->stride = CAM_VFE_RDI_BUS_DEFAULT_STRIDE;
			rsrc_data->pack_fmt = 0x0;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
			break;
		case CAM_FORMAT_PLAIN64:
			rsrc_data->en_cfg = 0x1;
			rsrc_data->pack_fmt = 0xA;
			break;
		default:
			CAM_ERR(CAM_ISP, "Unsupported RDI format %d",
				rsrc_data->format);
			return -EINVAL;
		}
	} else if ((rsrc_data->index < 2) ||
		(rsrc_data->index == 8) || (rsrc_data->index == 9) ||
		(rsrc_data->index == 4) || (rsrc_data->index == 5)) {
		/*
		 * WM 0-1 FULL_OUT, WM 8-9 FD_OUT,
		 * WM 4-5 FULL_DISP
		 */
		switch (rsrc_data->format) {
		case CAM_FORMAT_UBWC_NV12_4R:
			rsrc_data->en_ubwc = 1;
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		case CAM_FORMAT_UBWC_NV12:
			rsrc_data->en_ubwc = 1;
			/* Fall through for NV12 */
		case CAM_FORMAT_NV21:
		case CAM_FORMAT_NV12:
		case CAM_FORMAT_Y_ONLY:
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		case CAM_FORMAT_UBWC_TP10:
			rsrc_data->en_ubwc = 1;
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		case CAM_FORMAT_TP10:
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		case CAM_FORMAT_PLAIN16_10:
			switch (plane) {
			case PLANE_C:
				rsrc_data->height /= 2;
				break;
			case PLANE_Y:
				break;
			default:
				CAM_ERR(CAM_ISP, "Invalid plane %d", plane);
				return -EINVAL;
			}
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid format %d",
				rsrc_data->format);
			return -EINVAL;
		}
		rsrc_data->en_cfg = 0x1;
	} else if (rsrc_data->index == 20) {
		/* WM 20 stats BAF */
		rsrc_data->width = 0;
		rsrc_data->height = 0;
		rsrc_data->stride = 1;
		rsrc_data->en_cfg = (0x1 << 16) | 0x1;
	} else if (rsrc_data->index > 11 && rsrc_data->index < 20) {
		/* WM 12-19 stats */
		rsrc_data->width = 0;
		rsrc_data->height = 0;
		rsrc_data->stride = 1;
		rsrc_data->en_cfg = (0x1 << 16) | 0x1;
	} else if (rsrc_data->index == 11 || rsrc_data->index == 21) {
		/* WM 21/11 PDAF/2PD */
		rsrc_data->width = 0;
		rsrc_data->height = 0;
		rsrc_data->stride = 1;
		rsrc_data->en_cfg = (0x1 << 16) | 0x1;
		if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_PDAF)
			/* LSB aligned */
			rsrc_data->pack_fmt |= 0x10;
	} else if (rsrc_data->index == 10) {
		/* WM 10 Raw dump */
		rsrc_data->stride = rsrc_data->width;
		rsrc_data->en_cfg = 0x1;
		/* LSB aligned */
		rsrc_data->pack_fmt |= 0x10;
	} else if (rsrc_data->index == 22) {
		switch (rsrc_data->format) {
		case CAM_FORMAT_PLAIN16_16:
			rsrc_data->width = 0;
			rsrc_data->height = 0;
			rsrc_data->stride = 1;
			rsrc_data->en_cfg = (0x1 << 16) | 0x1;
			/* LSB aligned */
			rsrc_data->pack_fmt |= 0x10;
			break;
		default:
			CAM_ERR(CAM_ISP, "Invalid format %d",
				rsrc_data->format);
			return -EINVAL;
		}
	} else {
		/* Write master 2-3 and 6-7 DS ports */

		rsrc_data->height = rsrc_data->height / 2;
		rsrc_data->width  = rsrc_data->width / 2;
		rsrc_data->en_cfg = 0x1;
	}

	*wm_res = wm_res_local;
	*comp_grp_id = rsrc_data->hw_regs->comp_group;

	CAM_DBG(CAM_ISP, "WM:%d processed width %d, processed  height %d",
		rsrc_data->index, rsrc_data->width, rsrc_data->height);
	return 0;
}

static int cam_vfe_bus_ver3_release_wm(void   *bus_priv,
	struct cam_isp_resource_node     *wm_res)
{
	struct cam_vfe_bus_ver3_wm_resource_data   *rsrc_data =
		wm_res->res_priv;

	rsrc_data->offset = 0;
	rsrc_data->width = 0;
	rsrc_data->height = 0;
	rsrc_data->stride = 0;
	rsrc_data->format = 0;
	rsrc_data->pack_fmt = 0;
	rsrc_data->burst_len = 0;
	rsrc_data->irq_subsample_period = 0;
	rsrc_data->irq_subsample_pattern = 0;
	rsrc_data->framedrop_period = 0;
	rsrc_data->framedrop_pattern = 0;
	rsrc_data->packer_cfg = 0;
	rsrc_data->en_ubwc = 0;
	rsrc_data->h_init = 0;
	rsrc_data->ubwc_meta_addr = 0;
	rsrc_data->ubwc_meta_cfg = 0;
	rsrc_data->ubwc_mode_cfg = 0;
	rsrc_data->ubwc_stats_ctrl = 0;
	rsrc_data->ubwc_ctrl_2 = 0;
	rsrc_data->init_cfg_done = false;
	rsrc_data->hfr_cfg_done = false;
	rsrc_data->ubwc_updated = false;
	rsrc_data->en_cfg = 0;
	rsrc_data->is_dual = 0;

	wm_res->tasklet_info = NULL;
	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	CAM_DBG(CAM_ISP, "Release WM:%d", rsrc_data->index);

	return 0;
}

static int cam_vfe_bus_ver3_start_wm(struct cam_isp_resource_node *wm_res)
{
	int val = 0;
	struct cam_vfe_bus_ver3_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_vfe_bus_ver3_common_data        *common_data =
		rsrc_data->common_data;
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_regs;

	ubwc_regs = (struct cam_vfe_bus_ver3_reg_offset_ubwc_client *)
		rsrc_data->hw_regs->ubwc_regs;

	cam_io_w(0xf, common_data->mem_base + rsrc_data->hw_regs->burst_limit);

	cam_io_w((rsrc_data->height << 16) | rsrc_data->width,
		common_data->mem_base + rsrc_data->hw_regs->image_cfg_0);
	cam_io_w(rsrc_data->pack_fmt,
		common_data->mem_base + rsrc_data->hw_regs->packer_cfg);

	/* Configure stride for RDIs on full IFE */
	if (!common_data->is_lite && rsrc_data->index > 22)
		cam_io_w_mb(rsrc_data->stride, (common_data->mem_base +
			rsrc_data->hw_regs->image_cfg_2));

	/* Configure stride for RDIs on IFE lite */
	if (common_data->is_lite)
		cam_io_w_mb(rsrc_data->stride, (common_data->mem_base +
			rsrc_data->hw_regs->image_cfg_2));

	/* enable ubwc if needed*/
	if (rsrc_data->en_ubwc) {
		val = cam_io_r_mb(common_data->mem_base + ubwc_regs->mode_cfg);
		val |= 0x1;
		cam_io_w_mb(val, common_data->mem_base + ubwc_regs->mode_cfg);
	}

	/* Enable WM */
	cam_io_w_mb(rsrc_data->en_cfg, common_data->mem_base +
		rsrc_data->hw_regs->cfg);

	CAM_DBG(CAM_ISP, "WM:%d width = %d, height = %d", rsrc_data->index,
		rsrc_data->width, rsrc_data->height);
	CAM_DBG(CAM_ISP, "WM:%d pk_fmt = %d", rsrc_data->index,
		rsrc_data->pack_fmt & PACKER_FMT_VER3_MAX);
	CAM_DBG(CAM_ISP, "WM:%d stride = %d, burst len = %d",
		rsrc_data->index, rsrc_data->stride, 0xf);
	CAM_DBG(CAM_ISP, "Start WM:%d offset 0x%x val 0x%x",
		rsrc_data->index, (uint32_t) rsrc_data->hw_regs->cfg,
		rsrc_data->en_cfg);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return 0;
}

static int cam_vfe_bus_ver3_stop_wm(struct cam_isp_resource_node *wm_res)
{
	struct cam_vfe_bus_ver3_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_vfe_bus_ver3_common_data        *common_data =
		rsrc_data->common_data;

	/* Disable WM */
	cam_io_w_mb(0x0, common_data->mem_base + rsrc_data->hw_regs->cfg);
	CAM_DBG(CAM_ISP, "Stop WM:%d", rsrc_data->index);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	rsrc_data->init_cfg_done = false;
	rsrc_data->hfr_cfg_done = false;
	rsrc_data->ubwc_updated = false;

	return 0;
}

static int cam_vfe_bus_ver3_handle_wm_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_bus_ver3_handle_wm_done_bottom_half(void *wm_node,
	void *evt_payload_priv)
{
	return -EPERM;
}

static int cam_vfe_bus_ver3_init_wm_resource(uint32_t index,
	struct cam_vfe_bus_ver3_priv    *ver3_bus_priv,
	struct cam_vfe_bus_ver3_hw_info *ver3_hw_info,
	struct cam_isp_resource_node    *wm_res)
{
	struct cam_vfe_bus_ver3_wm_resource_data *rsrc_data;

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_ver3_wm_resource_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		CAM_DBG(CAM_ISP, "Failed to alloc for WM res priv");
		return -ENOMEM;
	}
	wm_res->res_priv = rsrc_data;

	rsrc_data->index = index;
	rsrc_data->hw_regs = &ver3_hw_info->bus_client_reg[index];
	rsrc_data->common_data = &ver3_bus_priv->common_data;

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&wm_res->list);

	wm_res->start = cam_vfe_bus_ver3_start_wm;
	wm_res->stop = cam_vfe_bus_ver3_stop_wm;
	wm_res->top_half_handler = cam_vfe_bus_ver3_handle_wm_done_top_half;
	wm_res->bottom_half_handler =
		cam_vfe_bus_ver3_handle_wm_done_bottom_half;
	wm_res->hw_intf = ver3_bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_vfe_bus_ver3_deinit_wm_resource(
	struct cam_isp_resource_node    *wm_res)
{
	struct cam_vfe_bus_ver3_wm_resource_data *rsrc_data;

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	INIT_LIST_HEAD(&wm_res->list);

	wm_res->start = NULL;
	wm_res->stop = NULL;
	wm_res->top_half_handler = NULL;
	wm_res->bottom_half_handler = NULL;
	wm_res->hw_intf = NULL;

	rsrc_data = wm_res->res_priv;
	wm_res->res_priv = NULL;
	if (!rsrc_data)
		return -ENOMEM;
	kfree(rsrc_data);

	return 0;
}

static bool cam_vfe_bus_ver3_match_comp_grp(
	struct cam_vfe_bus_ver3_priv           *ver3_bus_priv,
	struct cam_isp_resource_node          **comp_grp,
	uint32_t                                comp_grp_id)
{
	struct cam_vfe_bus_ver3_comp_grp_data  *rsrc_data = NULL;
	struct cam_isp_resource_node           *comp_grp_local = NULL;

	list_for_each_entry(comp_grp_local,
		&ver3_bus_priv->used_comp_grp, list) {
		rsrc_data = comp_grp_local->res_priv;
		if (rsrc_data->comp_grp_type == comp_grp_id) {
			/* Match found */
			*comp_grp = comp_grp_local;
			return true;
		}
	}

	list_for_each_entry(comp_grp_local,
		&ver3_bus_priv->free_comp_grp, list) {
		rsrc_data = comp_grp_local->res_priv;
		if (rsrc_data->comp_grp_type == comp_grp_id) {
			/* Match found */
			*comp_grp = comp_grp_local;
			list_del(&comp_grp_local->list);
			list_add_tail(&comp_grp_local->list,
			&ver3_bus_priv->used_comp_grp);
			return false;
		}
	}

	*comp_grp = NULL;
	return false;
}

static int cam_vfe_bus_ver3_acquire_comp_grp(
	struct cam_vfe_bus_ver3_priv        *ver3_bus_priv,
	struct cam_isp_out_port_info        *out_port_info,
	void                                *tasklet,
	void                                *ctx,
	uint32_t                             is_dual,
	uint32_t                             is_master,
	enum cam_vfe_bus_ver3_vfe_core_id    dual_slave_core,
	struct cam_isp_resource_node       **comp_grp,
	enum cam_vfe_bus_ver3_comp_grp_type  comp_grp_id)
{
	int rc = 0;
	struct cam_isp_resource_node           *comp_grp_local = NULL;
	struct cam_vfe_bus_ver3_comp_grp_data  *rsrc_data = NULL;
	bool previously_acquired = false;

	if (comp_grp_id >= CAM_VFE_BUS_VER3_COMP_GRP_0 &&
		comp_grp_id <= CAM_VFE_BUS_VER3_COMP_GRP_13) {
		/* Check if matching comp_grp has already been acquired */
		previously_acquired = cam_vfe_bus_ver3_match_comp_grp(
			ver3_bus_priv, &comp_grp_local, comp_grp_id);
	}

	if (!comp_grp_local) {
		CAM_ERR(CAM_ISP, "Invalid comp_grp_type:%d", comp_grp_id);
		return -ENODEV;
	}

	rsrc_data = comp_grp_local->res_priv;

	if (!previously_acquired) {
		if (is_dual) {
			rc = cam_vfe_bus_ver3_get_intra_client_mask(
				dual_slave_core,
				comp_grp_local->hw_intf->hw_idx,
				&rsrc_data->intra_client_mask);
			if (rc)
				return rc;
		}

		comp_grp_local->tasklet_info = tasklet;
		comp_grp_local->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

		rsrc_data->is_master = is_master;
		rsrc_data->is_dual = is_dual;

		if (is_master)
			rsrc_data->addr_sync_mode = 0;
		else
			rsrc_data->addr_sync_mode = 1;

	} else {
		rsrc_data = comp_grp_local->res_priv;
		/* Do not support runtime change in composite mask */
		if (comp_grp_local->res_state ==
			CAM_ISP_RESOURCE_STATE_STREAMING) {
			CAM_ERR(CAM_ISP, "Invalid State %d Comp Grp %u",
				comp_grp_local->res_state,
				rsrc_data->comp_grp_type);
			return -EBUSY;
		}
	}

	CAM_DBG(CAM_ISP, "Acquire comp_grp:%u", rsrc_data->comp_grp_type);

	rsrc_data->ctx = ctx;
	rsrc_data->acquire_dev_cnt++;
	*comp_grp = comp_grp_local;

	return rc;
}

static int cam_vfe_bus_ver3_release_comp_grp(
	struct cam_vfe_bus_ver3_priv         *ver3_bus_priv,
	struct cam_isp_resource_node         *in_comp_grp)
{
	struct cam_isp_resource_node           *comp_grp = NULL;
	struct cam_vfe_bus_ver3_comp_grp_data  *in_rsrc_data = NULL;
	int match_found = 0;

	if (!in_comp_grp) {
		CAM_ERR(CAM_ISP, "Invalid Params Comp Grp %pK", in_comp_grp);
		return -EINVAL;
	}

	if (in_comp_grp->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "Already released Comp Grp");
		return 0;
	}

	if (in_comp_grp->res_state == CAM_ISP_RESOURCE_STATE_STREAMING) {
		CAM_ERR(CAM_ISP, "Invalid State %d",
			in_comp_grp->res_state);
		return -EBUSY;
	}

	in_rsrc_data = in_comp_grp->res_priv;
	CAM_DBG(CAM_ISP, "Comp Grp type %u", in_rsrc_data->comp_grp_type);

	list_for_each_entry(comp_grp, &ver3_bus_priv->used_comp_grp, list) {
		if (comp_grp == in_comp_grp) {
			match_found = 1;
			break;
		}
	}

	if (!match_found) {
		CAM_ERR(CAM_ISP, "Could not find comp_grp_type:%u",
			in_rsrc_data->comp_grp_type);
		return -ENODEV;
	}

	in_rsrc_data->acquire_dev_cnt--;
	if (in_rsrc_data->acquire_dev_cnt == 0) {
		list_del(&comp_grp->list);

		in_rsrc_data->dual_slave_core = CAM_VFE_BUS_VER3_VFE_CORE_MAX;
		in_rsrc_data->addr_sync_mode = 0;

		comp_grp->tasklet_info = NULL;
		comp_grp->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

		list_add_tail(&comp_grp->list, &ver3_bus_priv->free_comp_grp);
	}

	return 0;
}

static int cam_vfe_bus_ver3_start_comp_grp(
	struct cam_isp_resource_node *comp_grp)
{
	int rc = 0;
	uint32_t val;
	struct cam_vfe_bus_ver3_comp_grp_data *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_common_data *common_data = NULL;
	uint32_t bus_irq_reg_mask[CAM_VFE_BUS_VER3_IRQ_MAX] = {0};

	rsrc_data = comp_grp->res_priv;
	common_data = rsrc_data->common_data;

	CAM_DBG(CAM_ISP, "comp_grp_type:%d streaming state:%d",
		rsrc_data->comp_grp_type, comp_grp->res_state);

	if (comp_grp->res_state == CAM_ISP_RESOURCE_STATE_STREAMING)
		return 0;

	if (rsrc_data->is_dual) {
		if (rsrc_data->is_master) {
			val = cam_io_r_mb(common_data->mem_base +
				common_data->common_reg->comp_cfg_0);
			val |= (0x1 << (rsrc_data->comp_grp_type + 14));
			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_0);

			val = cam_io_r_mb(common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
			val |= (0x1 << rsrc_data->comp_grp_type);
			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
		} else {
			val = cam_io_r_mb(common_data->mem_base +
				common_data->common_reg->comp_cfg_0);
			val |= (0x1 << rsrc_data->comp_grp_type);
			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_0);

			val = cam_io_r_mb(common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
			val |= (0x1 << rsrc_data->comp_grp_type);
			cam_io_w_mb(val, common_data->mem_base +
				common_data->common_reg->comp_cfg_1);
		}
	}

	bus_irq_reg_mask[CAM_VFE_BUS_VER3_IRQ_REG0] =
		(0x1 << (rsrc_data->comp_grp_type + 6));

	/*
	 * For Dual composite subscribe IRQ only for master
	 * For regular composite, subscribe IRQ always
	 */
	CAM_DBG(CAM_ISP, "Subscribe comp_grp_type:%d IRQ",
		rsrc_data->comp_grp_type);
	if ((rsrc_data->is_dual && rsrc_data->is_master) ||
		(!rsrc_data->is_dual)) {
		comp_grp->irq_handle = cam_irq_controller_subscribe_irq(
			common_data->bus_irq_controller, CAM_IRQ_PRIORITY_1,
			bus_irq_reg_mask, comp_grp,
			comp_grp->top_half_handler,
			cam_ife_mgr_do_tasklet_buf_done,
			comp_grp->tasklet_info, &tasklet_bh_api);
		if (comp_grp->irq_handle < 0) {
			CAM_ERR(CAM_ISP, "Subscribe IRQ failed for comp_grp %d",
				rsrc_data->comp_grp_type);
			return -EFAULT;
		}
	}
	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return rc;
}

static int cam_vfe_bus_ver3_stop_comp_grp(
	struct cam_isp_resource_node          *comp_grp)
{
	int rc = 0;
	struct cam_vfe_bus_ver3_comp_grp_data *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_common_data   *common_data = NULL;

	rsrc_data = comp_grp->res_priv;
	common_data = rsrc_data->common_data;

	/* Unsubscribe IRQ */
	if ((rsrc_data->is_dual && rsrc_data->is_master) ||
		(!rsrc_data->is_dual)) {
		rc = cam_irq_controller_unsubscribe_irq(
			common_data->bus_irq_controller,
			comp_grp->irq_handle);
	}
	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return rc;
}

static int cam_vfe_bus_ver3_handle_comp_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int32_t                                     rc;
	int                                         i;
	struct cam_isp_resource_node               *comp_grp = NULL;
	struct cam_vfe_bus_ver3_comp_grp_data      *rsrc_data = NULL;
	struct cam_vfe_bus_irq_evt_payload         *evt_payload;

	comp_grp = th_payload->handler_priv;
	if (!comp_grp) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "No resource");
		return -ENODEV;
	}

	rsrc_data = comp_grp->res_priv;

	CAM_DBG(CAM_ISP, "IRQ status_0 = 0x%x", th_payload->evt_status_arr[0]);
	CAM_DBG(CAM_ISP, "IRQ status_1 = 0x%x", th_payload->evt_status_arr[1]);

	rc  = cam_vfe_bus_ver3_get_evt_payload(rsrc_data->common_data,
		&evt_payload);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"No tasklet_cmd is free in queue");
		CAM_ERR_RATE_LIMIT(CAM_ISP,
			"IRQ status_0 = 0x%x status_1 = 0x%x",
			th_payload->evt_status_arr[0],
			th_payload->evt_status_arr[1]);

		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	evt_payload->ctx = rsrc_data->ctx;
	evt_payload->core_index = rsrc_data->common_data->core_index;
	evt_payload->evt_id  = evt_id;

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	th_payload->evt_payload_priv = evt_payload;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;
}

static int cam_vfe_bus_ver3_handle_comp_done_bottom_half(
	void                *handler_priv,
	void                *evt_payload_priv)
{
	int rc = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node          *comp_grp = handler_priv;
	struct cam_vfe_bus_irq_evt_payload    *evt_payload = evt_payload_priv;
	struct cam_vfe_bus_ver3_comp_grp_data *rsrc_data = comp_grp->res_priv;
	uint32_t                              *cam_ife_irq_regs;
	uint32_t                               status0_reg;

	CAM_DBG(CAM_ISP, "comp_grp_type:%d", rsrc_data->comp_grp_type);

	if (!evt_payload)
		return rc;

	if (rsrc_data->is_dual && (!rsrc_data->is_master)) {
		CAM_ERR(CAM_ISP, "Invalid comp_grp_type:%u is_master:%u",
			rsrc_data->comp_grp_type, rsrc_data->is_master);
		return rc;
	}

	cam_ife_irq_regs = evt_payload->irq_reg_val;
	status0_reg = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0];

	if (status0_reg & BIT(rsrc_data->comp_grp_type + 6)) {
		rsrc_data->irq_trigger_cnt++;
		if (rsrc_data->irq_trigger_cnt ==
			rsrc_data->acquire_dev_cnt) {
			cam_ife_irq_regs[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0] &=
					~BIT(rsrc_data->comp_grp_type + 6);
			rsrc_data->irq_trigger_cnt = 0;
		}
		rc = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	CAM_DBG(CAM_ISP, "status_0_reg = 0x%x, bit index = %d rc %d",
		status0_reg, (rsrc_data->comp_grp_type + 6), rc);

	if (rc == CAM_VFE_IRQ_STATUS_SUCCESS)
		cam_vfe_bus_ver3_put_evt_payload(rsrc_data->common_data,
			&evt_payload);

	return rc;
}

static int cam_vfe_bus_ver3_init_comp_grp(uint32_t index,
	struct cam_vfe_bus_ver3_priv    *ver3_bus_priv,
	struct cam_vfe_bus_ver3_hw_info *ver3_hw_info,
	struct cam_isp_resource_node    *comp_grp)
{
	struct cam_vfe_bus_ver3_comp_grp_data *rsrc_data = NULL;

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_ver3_comp_grp_data),
		GFP_KERNEL);
	if (!rsrc_data)
		return -ENOMEM;

	comp_grp->res_priv = rsrc_data;

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&comp_grp->list);

	rsrc_data->comp_grp_type   = index;
	rsrc_data->common_data     = &ver3_bus_priv->common_data;
	rsrc_data->dual_slave_core = CAM_VFE_BUS_VER3_VFE_CORE_MAX;

	list_add_tail(&comp_grp->list, &ver3_bus_priv->free_comp_grp);

	comp_grp->start = cam_vfe_bus_ver3_start_comp_grp;
	comp_grp->stop = cam_vfe_bus_ver3_stop_comp_grp;
	comp_grp->top_half_handler = cam_vfe_bus_ver3_handle_comp_done_top_half;
	comp_grp->bottom_half_handler =
		cam_vfe_bus_ver3_handle_comp_done_bottom_half;
	comp_grp->hw_intf = ver3_bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_vfe_bus_ver3_deinit_comp_grp(
	struct cam_isp_resource_node    *comp_grp)
{
	struct cam_vfe_bus_ver3_comp_grp_data *rsrc_data =
		comp_grp->res_priv;

	comp_grp->start = NULL;
	comp_grp->stop = NULL;
	comp_grp->top_half_handler = NULL;
	comp_grp->bottom_half_handler = NULL;
	comp_grp->hw_intf = NULL;

	list_del_init(&comp_grp->list);
	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;

	comp_grp->res_priv = NULL;

	if (!rsrc_data) {
		CAM_ERR(CAM_ISP, "comp_grp_priv is NULL");
		return -ENODEV;
	}
	kfree(rsrc_data);

	return 0;
}

static int cam_vfe_bus_ver3_get_secure_mode(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	bool *mode = cmd_args;
	struct cam_isp_resource_node *res =
		(struct cam_isp_resource_node *) priv;
	struct cam_vfe_bus_ver3_vfe_out_data *rsrc_data =
		(struct cam_vfe_bus_ver3_vfe_out_data *)res->res_priv;

	*mode = (rsrc_data->secure_mode == CAM_SECURE_MODE_SECURE) ?
		true : false;

	return 0;
}

static int cam_vfe_bus_ver3_acquire_vfe_out(void *bus_priv, void *acquire_args,
	uint32_t args_size)
{
	int                                     rc = -ENODEV;
	int                                     i;
	enum cam_vfe_bus_ver3_vfe_out_type      vfe_out_res_id;
	uint32_t                                format;
	int                                     num_wm;
	struct cam_vfe_bus_ver3_priv           *ver3_bus_priv = bus_priv;
	struct cam_vfe_acquire_args            *acq_args = acquire_args;
	struct cam_vfe_hw_vfe_out_acquire_args *out_acquire_args;
	struct cam_isp_resource_node           *rsrc_node = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data   *rsrc_data = NULL;
	uint32_t                                secure_caps = 0, mode;
	enum cam_vfe_bus_ver3_comp_grp_type     comp_grp_id;

	if (!bus_priv || !acquire_args) {
		CAM_ERR(CAM_ISP, "Invalid Param");
		return -EINVAL;
	}

	out_acquire_args = &acq_args->vfe_out;
	format = out_acquire_args->out_port_info->format;

	CAM_DBG(CAM_ISP, "Acquiring resource type 0x%x",
		out_acquire_args->out_port_info->res_type);

	vfe_out_res_id = cam_vfe_bus_ver3_get_out_res_id(
		out_acquire_args->out_port_info->res_type);
	if (vfe_out_res_id == CAM_VFE_BUS_VER3_VFE_OUT_MAX)
		return -ENODEV;

	num_wm = cam_vfe_bus_ver3_get_num_wm(vfe_out_res_id, format);
	if (num_wm < 1)
		return -EINVAL;

	rsrc_node = &ver3_bus_priv->vfe_out[vfe_out_res_id];
	if (rsrc_node->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		CAM_ERR(CAM_ISP, "Resource not available: Res_id %d state:%d",
			vfe_out_res_id, rsrc_node->res_state);
		return -EBUSY;
	}

	rsrc_data = rsrc_node->res_priv;
	secure_caps = cam_vfe_bus_ver3_can_be_secure(
		rsrc_data->out_type);
	mode = out_acquire_args->out_port_info->secure_mode;
	mutex_lock(&rsrc_data->common_data->bus_mutex);
	if (secure_caps) {
		if (!rsrc_data->common_data->num_sec_out) {
			rsrc_data->secure_mode = mode;
			rsrc_data->common_data->secure_mode = mode;
		} else {
			if (mode == rsrc_data->common_data->secure_mode) {
				rsrc_data->secure_mode =
					rsrc_data->common_data->secure_mode;
			} else {
				rc = -EINVAL;
				CAM_ERR_RATE_LIMIT(CAM_ISP,
					"Mismatch: Acquire mode[%d], drvr mode[%d]",
					rsrc_data->common_data->secure_mode,
					mode);
				mutex_unlock(
					&rsrc_data->common_data->bus_mutex);
				return -EINVAL;
			}
		}
		rsrc_data->common_data->num_sec_out++;
	}
	mutex_unlock(&rsrc_data->common_data->bus_mutex);

	ver3_bus_priv->tasklet_info = acq_args->tasklet;
	rsrc_data->num_wm = num_wm;
	rsrc_node->res_id = out_acquire_args->out_port_info->res_type;
	rsrc_node->tasklet_info = acq_args->tasklet;
	rsrc_node->cdm_ops = out_acquire_args->cdm_ops;
	rsrc_data->cdm_util_ops = out_acquire_args->cdm_ops;

	/* Acquire WM and retrieve COMP GRP ID */
	for (i = 0; i < num_wm; i++) {
		rc = cam_vfe_bus_ver3_acquire_wm(ver3_bus_priv,
			out_acquire_args->out_port_info,
			acq_args->tasklet,
			out_acquire_args->ctx,
			vfe_out_res_id,
			i,
			&rsrc_data->wm_res[i],
			out_acquire_args->is_dual,
			&comp_grp_id);
		if (rc) {
			CAM_ERR(CAM_ISP,
				"VFE%d WM acquire failed for Out %d rc=%d",
				rsrc_data->common_data->core_index,
				vfe_out_res_id, rc);
			goto release_wm;
		}
	}

	/* Acquire composite group using COMP GRP ID */
	rc = cam_vfe_bus_ver3_acquire_comp_grp(ver3_bus_priv,
		out_acquire_args->out_port_info,
		acq_args->tasklet,
		out_acquire_args->ctx,
		out_acquire_args->is_dual,
		out_acquire_args->is_master,
		out_acquire_args->dual_slave_core,
		&rsrc_data->comp_grp,
		comp_grp_id);
	if (rc) {
		CAM_ERR(CAM_ISP,
			"VFE%d Comp_Grp acquire fail for Out %d rc=%d",
			rsrc_data->common_data->core_index,
			vfe_out_res_id, rc);
		return rc;
	}


	rsrc_node->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	out_acquire_args->rsrc_node = rsrc_node;

	CAM_DBG(CAM_ISP, "Acquire successful");
	return rc;

release_wm:
	for (i--; i >= 0; i--)
		cam_vfe_bus_ver3_release_wm(ver3_bus_priv,
			rsrc_data->wm_res[i]);

	cam_vfe_bus_ver3_release_comp_grp(ver3_bus_priv, rsrc_data->comp_grp);

	return rc;
}

static int cam_vfe_bus_ver3_release_vfe_out(void *bus_priv, void *release_args,
	uint32_t args_size)
{
	uint32_t i;
	struct cam_isp_resource_node          *vfe_out = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data = NULL;
	uint32_t                               secure_caps = 0;

	if (!bus_priv || !release_args) {
		CAM_ERR(CAM_ISP, "Invalid input bus_priv %pK release_args %pK",
			bus_priv, release_args);
		return -EINVAL;
	}

	vfe_out = release_args;
	rsrc_data = vfe_out->res_priv;

	if (vfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Invalid resource state:%d",
			vfe_out->res_state);
	}

	for (i = 0; i < rsrc_data->num_wm; i++)
		cam_vfe_bus_ver3_release_wm(bus_priv, rsrc_data->wm_res[i]);
	rsrc_data->num_wm = 0;

	if (rsrc_data->comp_grp)
		cam_vfe_bus_ver3_release_comp_grp(bus_priv,
			rsrc_data->comp_grp);
	rsrc_data->comp_grp = NULL;

	vfe_out->tasklet_info = NULL;
	vfe_out->cdm_ops = NULL;
	rsrc_data->cdm_util_ops = NULL;

	secure_caps = cam_vfe_bus_ver3_can_be_secure(rsrc_data->out_type);
	mutex_lock(&rsrc_data->common_data->bus_mutex);
	if (secure_caps) {
		if (rsrc_data->secure_mode ==
			rsrc_data->common_data->secure_mode) {
			rsrc_data->common_data->num_sec_out--;
			rsrc_data->secure_mode =
				CAM_SECURE_MODE_NON_SECURE;
		} else {
			/*
			 * The validity of the mode is properly
			 * checked while acquiring the output port.
			 * not expected to reach here, unless there is
			 * some corruption.
			 */
			CAM_ERR(CAM_ISP, "driver[%d],resource[%d] mismatch",
				rsrc_data->common_data->secure_mode,
				rsrc_data->secure_mode);
		}

		if (!rsrc_data->common_data->num_sec_out)
			rsrc_data->common_data->secure_mode =
				CAM_SECURE_MODE_NON_SECURE;
	}
	mutex_unlock(&rsrc_data->common_data->bus_mutex);

	if (vfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED)
		vfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_vfe_bus_ver3_start_vfe_out(
	struct cam_isp_resource_node          *vfe_out)
{
	int rc = 0, i;
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data = NULL;
	struct cam_vfe_bus_ver3_common_data   *common_data = NULL;

	if (!vfe_out) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = vfe_out->res_priv;
	common_data = rsrc_data->common_data;

	CAM_DBG(CAM_ISP, "Start resource index %d", rsrc_data->out_type);

	if (vfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_ERR(CAM_ISP, "Invalid resource state:%d",
			vfe_out->res_state);
		return -EACCES;
	}

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_vfe_bus_ver3_start_wm(rsrc_data->wm_res[i]);

	if (rsrc_data->comp_grp)
		rc = cam_vfe_bus_ver3_start_comp_grp(rsrc_data->comp_grp);

	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_vfe_bus_ver3_stop_vfe_out(
	struct cam_isp_resource_node          *vfe_out)
{
	int rc = 0, i;
	struct cam_vfe_bus_ver3_vfe_out_data  *rsrc_data = NULL;

	if (!vfe_out) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}

	rsrc_data = vfe_out->res_priv;

	if (vfe_out->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE ||
		vfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		CAM_DBG(CAM_ISP, "vfe_out res_state is %d", vfe_out->res_state);
		return rc;
	}

	if (rsrc_data->comp_grp)
		rc = cam_vfe_bus_ver3_stop_comp_grp(rsrc_data->comp_grp);

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_vfe_bus_ver3_stop_wm(rsrc_data->wm_res[i]);

	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_vfe_bus_ver3_handle_vfe_out_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_bus_ver3_handle_vfe_out_done_bottom_half(
	void                *handler_priv,
	void                *evt_payload_priv)
{
	int rc = -EINVAL;
	struct cam_isp_resource_node         *vfe_out = handler_priv;
	struct cam_vfe_bus_ver3_vfe_out_data *rsrc_data = vfe_out->res_priv;
	struct cam_vfe_bus_irq_evt_payload   *evt_payload = evt_payload_priv;

	if (evt_payload->evt_id == CAM_ISP_HW_EVENT_REG_UPDATE) {
		rc = cam_vfe_bus_ver3_handle_rup_bottom_half(
			handler_priv, evt_payload_priv);
		return rc;
	}
	/* We only handle composite buf done */
	if (rsrc_data->comp_grp) {
		rc = rsrc_data->comp_grp->bottom_half_handler(
			rsrc_data->comp_grp, evt_payload_priv);
	}

	CAM_DBG(CAM_ISP, "vfe_out %d rc %d", rsrc_data->out_type, rc);

	return rc;
}

static int cam_vfe_bus_ver3_init_vfe_out_resource(uint32_t  index,
	struct cam_vfe_bus_ver3_priv                  *ver3_bus_priv,
	struct cam_vfe_bus_ver3_hw_info               *ver3_hw_info)
{
	struct cam_isp_resource_node         *vfe_out = NULL;
	struct cam_vfe_bus_ver3_vfe_out_data *rsrc_data = NULL;
	int rc = 0;
	int32_t vfe_out_type =
		ver3_hw_info->vfe_out_hw_info[index].vfe_out_type;

	if (vfe_out_type < 0 ||
		vfe_out_type >= CAM_VFE_BUS_VER3_VFE_OUT_MAX) {
		CAM_ERR(CAM_ISP, "Init VFE Out failed, Invalid type=%d",
			vfe_out_type);
		return -EINVAL;
	}

	vfe_out = &ver3_bus_priv->vfe_out[vfe_out_type];
	if (vfe_out->res_state != CAM_ISP_RESOURCE_STATE_UNAVAILABLE ||
		vfe_out->res_priv) {
		CAM_ERR(CAM_ISP, "vfe_out_type %d has already been initialized",
			vfe_out_type);
		return -EFAULT;
	}

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_ver3_vfe_out_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		rc = -ENOMEM;
		return rc;
	}

	vfe_out->res_priv = rsrc_data;

	vfe_out->res_type = CAM_ISP_RESOURCE_VFE_OUT;
	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&vfe_out->list);

	rsrc_data->out_type    =
		ver3_hw_info->vfe_out_hw_info[index].vfe_out_type;
	rsrc_data->common_data = &ver3_bus_priv->common_data;
	rsrc_data->max_width   =
		ver3_hw_info->vfe_out_hw_info[index].max_width;
	rsrc_data->max_height  =
		ver3_hw_info->vfe_out_hw_info[index].max_height;
	rsrc_data->secure_mode = CAM_SECURE_MODE_NON_SECURE;

	vfe_out->start = cam_vfe_bus_ver3_start_vfe_out;
	vfe_out->stop = cam_vfe_bus_ver3_stop_vfe_out;
	vfe_out->top_half_handler =
		cam_vfe_bus_ver3_handle_vfe_out_done_top_half;
	vfe_out->bottom_half_handler =
		cam_vfe_bus_ver3_handle_vfe_out_done_bottom_half;
	vfe_out->process_cmd = cam_vfe_bus_ver3_process_cmd;
	vfe_out->hw_intf = ver3_bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_vfe_bus_ver3_deinit_vfe_out_resource(
	struct cam_isp_resource_node    *vfe_out)
{
	struct cam_vfe_bus_ver3_vfe_out_data *rsrc_data = vfe_out->res_priv;

	if (vfe_out->res_state == CAM_ISP_RESOURCE_STATE_UNAVAILABLE) {
		/*
		 * This is not error. It can happen if the resource is
		 * never supported in the HW.
		 */
		CAM_DBG(CAM_ISP, "HW%d Res %d already deinitialized");
		return 0;
	}

	vfe_out->start = NULL;
	vfe_out->stop = NULL;
	vfe_out->top_half_handler = NULL;
	vfe_out->bottom_half_handler = NULL;
	vfe_out->hw_intf = NULL;

	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_UNAVAILABLE;
	INIT_LIST_HEAD(&vfe_out->list);
	vfe_out->res_priv = NULL;

	if (!rsrc_data)
		return -ENOMEM;
	kfree(rsrc_data);

	return 0;
}

static int cam_vfe_bus_ver3_handle_irq(uint32_t    evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	struct cam_vfe_bus_ver3_priv          *bus_priv;
	int rc = 0;

	bus_priv     = th_payload->handler_priv;
	CAM_DBG(CAM_ISP, "Enter");
	rc = cam_irq_controller_handle_irq(evt_id,
		bus_priv->common_data.bus_irq_controller);
	return (rc == IRQ_HANDLED) ? 0 : -EINVAL;
}

static int cam_vfe_bus_ver3_err_irq_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	int i = 0, rc = 0;
	struct cam_vfe_bus_ver3_priv *bus_priv =
		th_payload->handler_priv;
	struct cam_vfe_bus_irq_evt_payload *evt_payload;

	CAM_ERR_RATE_LIMIT(CAM_ISP, "Bus Err IRQ");
	for (i = 0; i < th_payload->num_registers; i++) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "vfe:%d: IRQ_Status%d: 0x%x",
		bus_priv->common_data.core_index, i,
			th_payload->evt_status_arr[i]);
	}
	cam_irq_controller_disable_irq(bus_priv->common_data.bus_irq_controller,
		bus_priv->error_irq_handle);

	rc  = cam_vfe_bus_ver3_get_evt_payload(&bus_priv->common_data,
		&evt_payload);
	if (rc) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Cannot get payload");
		return rc;
	}

	for (i = 0; i < th_payload->num_registers; i++)
		evt_payload->irq_reg_val[i] = th_payload->evt_status_arr[i];

	evt_payload->core_index = bus_priv->common_data.core_index;
	evt_payload->evt_id  = evt_id;

	evt_payload->ctx = &bus_priv->common_data;
	evt_payload->ccif_violation_status = cam_io_r_mb(
		bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->ccif_violation_status);

	evt_payload->overflow_status = cam_io_r_mb(
		bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->overflow_status);
	cam_io_w_mb(0x1, bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->overflow_status_clear);

	evt_payload->image_size_violation_status = cam_io_r_mb(
		bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->image_size_violation_status);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static int cam_vfe_bus_ver3_err_irq_bottom_half(void *ctx_priv,
	void *evt_payload_priv)
{
	struct cam_vfe_bus_irq_evt_payload *evt_payload;
	struct cam_vfe_bus_ver3_common_data *common_data;
	uint32_t val = 0, image_size_violation = 0, ccif_violation = 0;

	if (!ctx_priv || !evt_payload_priv)
		return -EINVAL;

	evt_payload = evt_payload_priv;
	common_data = evt_payload->ctx;

	val = evt_payload->irq_reg_val[CAM_IFE_IRQ_BUS_VER3_REG_STATUS0];
	image_size_violation = (val >> 31) & 0x1;
	ccif_violation = (val >> 30) & 0x1;

	CAM_ERR(CAM_ISP, "Bus Violation");
	CAM_ERR(CAM_ISP, "image_size_violation %d ccif_violation %d",
		image_size_violation, ccif_violation);

	if (image_size_violation) {
		val = evt_payload->image_size_violation_status;

		if (val & 0x01)
			CAM_INFO(CAM_ISP, "VID Y 1:1 image size violation");

		if (val & 0x02)
			CAM_INFO(CAM_ISP, "VID C 1:1 image size violation");

		if (val & 0x04)
			CAM_INFO(CAM_ISP, "VID YC 4:1 image size violation");

		if (val & 0x08)
			CAM_INFO(CAM_ISP, "VID YC 16:1 image size violation");

		if (val & 0x010)
			CAM_INFO(CAM_ISP, "DISP Y 1:1 image size violation");

		if (val & 0x020)
			CAM_INFO(CAM_ISP, "DISP C 1:1 image size violation");

		if (val & 0x040)
			CAM_INFO(CAM_ISP, "DISP YC 4:1 image size violation");

		if (val & 0x080)
			CAM_INFO(CAM_ISP, "DISP YC 16:1 image size violation");

		if (val & 0x0100)
			CAM_INFO(CAM_ISP, "FD Y image size violation");

		if (val & 0x0200)
			CAM_INFO(CAM_ISP, "FD C image size violation");

		if (val & 0x0400)
			CAM_INFO(CAM_ISP,
			"PIXEL RAW DUMP image size violation");

		if (val & 0x0800)
			CAM_INFO(CAM_ISP, "CAMIF PD image size violation");

		if (val & 0x01000)
			CAM_INFO(CAM_ISP, "STATS HDR BE image size violation");

		if (val & 0x02000)
			CAM_INFO(CAM_ISP,
			"STATS HDR BHIST image size violation");

		if (val & 0x04000)
			CAM_INFO(CAM_ISP,
			"STATS TINTLESS BG image size violation");

		if (val & 0x08000)
			CAM_INFO(CAM_ISP, "STATS AWB BG image size violation");

		if (val & 0x010000)
			CAM_INFO(CAM_ISP, "STATS BHIST image size violation");

		if (val & 0x020000)
			CAM_INFO(CAM_ISP, "STATS RS image size violation");

		if (val & 0x040000)
			CAM_INFO(CAM_ISP, "STATS CS image size violation");

		if (val & 0x080000)
			CAM_INFO(CAM_ISP, "STATS IHIST image size violation");

		if (val & 0x0100000)
			CAM_INFO(CAM_ISP, "STATS BAF image size violation");

		if (val & 0x0200000)
			CAM_INFO(CAM_ISP, "PDAF image size violation");

		if (val & 0x0400000)
			CAM_INFO(CAM_ISP, "LCR image size violation");

		if (val & 0x0800000)
			CAM_INFO(CAM_ISP, "RDI 0 image size violation");

		if (val & 0x01000000)
			CAM_INFO(CAM_ISP, "RDI 1 image size violation");

		if (val & 0x02000000)
			CAM_INFO(CAM_ISP, "RDI 2 image size violation");

	}

	if (ccif_violation) {
		val = evt_payload->ccif_violation_status;

		if (val & 0x01)
			CAM_INFO(CAM_ISP, "VID Y 1:1 ccif violation");

		if (val & 0x02)
			CAM_INFO(CAM_ISP, "VID C 1:1 ccif violation");

		if (val & 0x04)
			CAM_INFO(CAM_ISP, "VID YC 4:1 ccif violation");

		if (val & 0x08)
			CAM_INFO(CAM_ISP, "VID YC 16:1 ccif violation");

		if (val & 0x010)
			CAM_INFO(CAM_ISP, "DISP Y 1:1 ccif violation");

		if (val & 0x020)
			CAM_INFO(CAM_ISP, "DISP C 1:1 ccif violation");

		if (val & 0x040)
			CAM_INFO(CAM_ISP, "DISP YC 4:1 ccif violation");

		if (val & 0x080)
			CAM_INFO(CAM_ISP, "DISP YC 16:1 ccif violation");

		if (val & 0x0100)
			CAM_INFO(CAM_ISP, "FD Y ccif violation");

		if (val & 0x0200)
			CAM_INFO(CAM_ISP, "FD C ccif violation");

		if (val & 0x0400)
			CAM_INFO(CAM_ISP, "PIXEL RAW DUMP ccif violation");

		if (val & 0x0800)
			CAM_INFO(CAM_ISP, "CAMIF PD ccif violation");

		if (val & 0x01000)
			CAM_INFO(CAM_ISP, "STATS HDR BE ccif violation");

		if (val & 0x02000)
			CAM_INFO(CAM_ISP, "STATS HDR BHIST ccif violation");

		if (val & 0x04000)
			CAM_INFO(CAM_ISP, "STATS TINTLESS BG ccif violation");

		if (val & 0x08000)
			CAM_INFO(CAM_ISP, "STATS AWB BG ccif violation");

		if (val & 0x010000)
			CAM_INFO(CAM_ISP, "STATS BHIST ccif violation");

		if (val & 0x020000)
			CAM_INFO(CAM_ISP, "STATS RS ccif violation");

		if (val & 0x040000)
			CAM_INFO(CAM_ISP, "STATS CS ccif violation");

		if (val & 0x080000)
			CAM_INFO(CAM_ISP, "STATS IHIST ccif violation");

		if (val & 0x0100000)
			CAM_INFO(CAM_ISP, "STATS BAF ccif violation");

		if (val & 0x0200000)
			CAM_INFO(CAM_ISP, "PDAF ccif violation");

		if (val & 0x0400000)
			CAM_INFO(CAM_ISP, "LCR ccif violation");

		if (val & 0x0800000)
			CAM_INFO(CAM_ISP, "RDI 0 ccif violation");

		if (val & 0x01000000)
			CAM_INFO(CAM_ISP, "RDI 1 ccif violation");

		if (val & 0x02000000)
			CAM_INFO(CAM_ISP, "RDI 2 ccif violation");

	}

	cam_vfe_bus_ver3_put_evt_payload(common_data, &evt_payload);
	return 0;
}

static void cam_vfe_bus_ver3_update_ubwc_meta_addr(
	uint32_t *reg_val_pair,
	uint32_t  *j,
	void     *regs,
	uint64_t  image_buf)
{
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_regs;

	if (!regs || !reg_val_pair || !j) {
		CAM_ERR(CAM_ISP, "Invalid args");
		goto end;
	}

	ubwc_regs = (struct cam_vfe_bus_ver3_reg_offset_ubwc_client *)regs;
	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->meta_addr, image_buf);

end:
	return;
}

static int cam_vfe_bus_ver3_update_ubwc_regs(
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data,
	uint32_t *reg_val_pair,	uint32_t i, uint32_t *j)
{
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_regs;
	uint32_t ubwc_bw_limit = 0;
	int rc = 0;

	if (!wm_data || !reg_val_pair || !j) {
		CAM_ERR(CAM_ISP, "Invalid args");
		rc = -EINVAL;
		goto end;
	}

	ubwc_regs = (struct cam_vfe_bus_ver3_reg_offset_ubwc_client *)
		wm_data->hw_regs->ubwc_regs;

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		wm_data->hw_regs->packer_cfg, wm_data->packer_cfg);
	CAM_DBG(CAM_ISP, "WM:%d packer cfg 0x%x",
		wm_data->index, reg_val_pair[*j-1]);

	if (wm_data->is_dual) {
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
			wm_data->hw_regs->image_cfg_1, wm_data->offset);
	} else {
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
			wm_data->hw_regs->image_cfg_1, wm_data->h_init);
		CAM_DBG(CAM_ISP, "WM:%d h_init 0x%x",
			wm_data->index, reg_val_pair[*j-1]);
	}

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->meta_cfg, wm_data->ubwc_meta_cfg);
	CAM_DBG(CAM_ISP, "WM:%d meta stride 0x%x",
		wm_data->index, reg_val_pair[*j-1]);

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->mode_cfg, wm_data->ubwc_mode_cfg);
	CAM_DBG(CAM_ISP, "WM:%d ubwc_mode_cfg 0x%x",
		wm_data->index, reg_val_pair[*j-1]);

	CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
		ubwc_regs->ctrl_2, wm_data->ubwc_ctrl_2);
	CAM_DBG(CAM_ISP, "WM:%d ubwc_ctrl_2 0x%x",
		wm_data->index, reg_val_pair[*j-1]);

	switch (wm_data->format) {
	case CAM_FORMAT_UBWC_TP10:
		ubwc_bw_limit = (0x8 << 1) | BIT(0);
		break;
	case CAM_FORMAT_UBWC_NV12_4R:
		ubwc_bw_limit = (0xB << 1) | BIT(0);
		break;
	default:
		ubwc_bw_limit = 0;
		break;
	}

	if (ubwc_bw_limit) {
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, *j,
			ubwc_regs->bw_limit, ubwc_bw_limit);
		CAM_DBG(CAM_ISP, "WM:%d ubwc bw limit 0x%x",
			wm_data->index, ubwc_bw_limit);
	}

end:
	return rc;
}

static int cam_vfe_bus_ver3_update_wm(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update         *update_buf;
	struct cam_buf_io_cfg                    *io_cfg;
	struct cam_vfe_bus_ver3_vfe_out_data     *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data = NULL;
	struct cam_vfe_bus_ver3_reg_offset_ubwc_client *ubwc_client = NULL;
	uint32_t *reg_val_pair;
	uint32_t  i, j, k, size = 0;
	uint32_t  frame_inc = 0, val;
	uint32_t loop_size = 0;

	bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
	update_buf =  (struct cam_isp_hw_get_cmd_update *) cmd_args;

	vfe_out_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		update_buf->res->res_priv;

	if (!vfe_out_data || !vfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Failed! Invalid data");
		return -EINVAL;
	}

	if (update_buf->wm_update->num_buf != vfe_out_data->num_wm) {
		CAM_ERR(CAM_ISP,
			"Failed! Invalid number buffers:%d required:%d",
			update_buf->wm_update->num_buf, vfe_out_data->num_wm);
		return -EINVAL;
	}

	reg_val_pair = &vfe_out_data->common_data->io_buf_update[0];
	io_cfg = update_buf->wm_update->io_cfg;

	for (i = 0, j = 0; i < vfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - MAX_BUF_UPDATE_REG_NUM * 2)) {
			CAM_ERR(CAM_ISP,
				"reg_val_pair %d exceeds the array limit %zu",
				j, MAX_REG_VAL_PAIR_SIZE);
			return -ENOMEM;
		}

		wm_data = vfe_out_data->wm_res[i]->res_priv;
		ubwc_client = wm_data->hw_regs->ubwc_regs;
		/* update width register */
		val = cam_io_r_mb(wm_data->common_data->mem_base +
			wm_data->hw_regs->image_cfg_0);
		/* mask previously written width but preserve height */
		val = val & 0xFFFF0000;
		val |= wm_data->width;
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->image_cfg_0, val);
		CAM_DBG(CAM_ISP, "WM %d image height and width 0x%x",
			wm_data->index, reg_val_pair[j-1]);

		/* For initial configuration program all bus registers */
		val = io_cfg->planes[i].plane_stride;
		CAM_DBG(CAM_ISP, "before stride %d", val);
		val = ALIGNUP(val, 16);
		if (val != io_cfg->planes[i].plane_stride &&
			val != wm_data->stride)
			CAM_WARN(CAM_ISP, "Warning stride %u expected %u",
				io_cfg->planes[i].plane_stride, val);

		if ((wm_data->stride != val ||
			!wm_data->init_cfg_done) &&
			((!bus_priv->common_data.is_lite && wm_data->index < 23)
			|| bus_priv->common_data.is_lite)) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_cfg_2,
				io_cfg->planes[i].plane_stride);
			wm_data->stride = val;
			CAM_DBG(CAM_ISP, "WM %d image stride 0x%x",
				wm_data->index, reg_val_pair[j-1]);
		}

		if (wm_data->en_ubwc) {
			if (!wm_data->hw_regs->ubwc_regs) {
				CAM_ERR(CAM_ISP,
					"No UBWC register to configure.");
				return -EINVAL;
			}
			if (wm_data->ubwc_updated) {
				wm_data->ubwc_updated = false;
				cam_vfe_bus_ver3_update_ubwc_regs(
					wm_data, reg_val_pair, i, &j);
			}

			/* UBWC meta address */
			cam_vfe_bus_ver3_update_ubwc_meta_addr(
				reg_val_pair, &j,
				wm_data->hw_regs->ubwc_regs,
				update_buf->wm_update->image_buf[i]);
			CAM_DBG(CAM_ISP, "WM %d ubwc meta addr 0x%llx",
				wm_data->index,
				update_buf->wm_update->image_buf[i]);
		}

		if (wm_data->en_ubwc) {
			frame_inc = ALIGNUP(io_cfg->planes[i].plane_stride *
			    io_cfg->planes[i].slice_height, 4096);
			frame_inc += io_cfg->planes[i].meta_size;
			CAM_DBG(CAM_ISP,
				"WM %d frm %d: ht: %d stride %d meta: %d",
				wm_data->index, frame_inc,
				io_cfg->planes[i].slice_height,
				io_cfg->planes[i].plane_stride,
				io_cfg->planes[i].meta_size);
		} else {
			frame_inc = io_cfg->planes[i].plane_stride *
				io_cfg->planes[i].slice_height;
		}

		if ((!bus_priv->common_data.is_lite && wm_data->index > 22) ||
			bus_priv->common_data.is_lite)
			loop_size = wm_data->irq_subsample_period + 1;
		else
			loop_size = 1;



		/* WM Image address */
		for (k = 0; k < loop_size; k++) {
			if (wm_data->en_ubwc)
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->image_addr,
					update_buf->wm_update->image_buf[i] +
					io_cfg->planes[i].meta_size +
					k * frame_inc);
			else
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->image_addr,
					update_buf->wm_update->image_buf[i] +
					wm_data->offset + k * frame_inc);
			CAM_DBG(CAM_ISP, "WM %d image address 0x%x",
				wm_data->index, reg_val_pair[j-1]);
		}

		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->frame_incr, frame_inc);
		CAM_DBG(CAM_ISP, "WM %d frame_inc %d",
			wm_data->index, reg_val_pair[j-1]);


		/* enable the WM */
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->cfg,
			wm_data->en_cfg);

		/* set initial configuration done */
		if (!wm_data->init_cfg_done)
			wm_data->init_cfg_done = true;
	}

	size = vfe_out_data->cdm_util_ops->cdm_required_size_reg_random(j/2);

	/* cdm util returns dwords, need to convert to bytes */
	if ((size * 4) > update_buf->cmd.size) {
		CAM_ERR(CAM_ISP,
			"Failed! Buf size:%d insufficient, expected size:%d",
			update_buf->cmd.size, size);
		return -ENOMEM;
	}

	vfe_out_data->cdm_util_ops->cdm_write_regrandom(
		update_buf->cmd.cmd_buf_addr, j/2, reg_val_pair);

	/* cdm util returns dwords, need to convert to bytes */
	update_buf->cmd.used_bytes = size * 4;

	return 0;
}

static int cam_vfe_bus_ver3_update_hfr(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv             *bus_priv;
	struct cam_isp_hw_get_cmd_update         *update_hfr;
	struct cam_vfe_bus_ver3_vfe_out_data     *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data *wm_data = NULL;
	struct cam_isp_port_hfr_config           *hfr_cfg = NULL;
	uint32_t *reg_val_pair;
	uint32_t  i, j, size = 0;

	bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
	update_hfr =  (struct cam_isp_hw_get_cmd_update *) cmd_args;

	vfe_out_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		update_hfr->res->res_priv;

	if (!vfe_out_data || !vfe_out_data->cdm_util_ops) {
		CAM_ERR(CAM_ISP, "Failed! Invalid data");
		return -EINVAL;
	}

	reg_val_pair = &vfe_out_data->common_data->io_buf_update[0];
	hfr_cfg = update_hfr->hfr_update;

	for (i = 0, j = 0; i < vfe_out_data->num_wm; i++) {
		if (j >= (MAX_REG_VAL_PAIR_SIZE - MAX_BUF_UPDATE_REG_NUM * 2)) {
			CAM_ERR(CAM_ISP,
				"reg_val_pair %d exceeds the array limit %zu",
				j, MAX_REG_VAL_PAIR_SIZE);
			return -ENOMEM;
		}

		wm_data = vfe_out_data->wm_res[i]->res_priv;

		if (((!bus_priv->common_data.is_lite && wm_data->index > 22) ||
			bus_priv->common_data.is_lite) &&
			hfr_cfg->subsample_period > 3) {
			CAM_ERR(CAM_ISP,
				"RDI doesn't support irq subsample period %d",
				hfr_cfg->subsample_period);
			return -EINVAL;
		}

		if ((wm_data->framedrop_pattern !=
			hfr_cfg->framedrop_pattern) ||
			!wm_data->hfr_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->framedrop_pattern,
				hfr_cfg->framedrop_pattern);
			wm_data->framedrop_pattern = hfr_cfg->framedrop_pattern;
			CAM_DBG(CAM_ISP, "WM:%d framedrop pattern 0x%x",
				wm_data->index, wm_data->framedrop_pattern);
		}

		if (wm_data->framedrop_period != hfr_cfg->framedrop_period ||
			!wm_data->hfr_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->framedrop_period,
				hfr_cfg->framedrop_period);
			wm_data->framedrop_period = hfr_cfg->framedrop_period;
			CAM_DBG(CAM_ISP, "WM:%d framedrop period 0x%x",
				wm_data->index, wm_data->framedrop_period);
		}

		if (wm_data->irq_subsample_period != hfr_cfg->subsample_period
			|| !wm_data->hfr_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->irq_subsample_period,
				hfr_cfg->subsample_period);
			wm_data->irq_subsample_period =
				hfr_cfg->subsample_period;
			CAM_DBG(CAM_ISP, "WM:%d irq subsample period 0x%x",
				wm_data->index, wm_data->irq_subsample_period);
		}

		if (wm_data->irq_subsample_pattern != hfr_cfg->subsample_pattern
			|| !wm_data->hfr_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->irq_subsample_pattern,
				hfr_cfg->subsample_pattern);
			wm_data->irq_subsample_pattern =
				hfr_cfg->subsample_pattern;
			CAM_DBG(CAM_ISP, "WM:%d irq subsample pattern 0x%x",
				wm_data->index, wm_data->irq_subsample_pattern);
		}

		/* set initial configuration done */
		if (!wm_data->hfr_cfg_done)
			wm_data->hfr_cfg_done = true;
	}

	size = vfe_out_data->cdm_util_ops->cdm_required_size_reg_random(j/2);

	/* cdm util returns dwords, need to convert to bytes */
	if ((size * 4) > update_hfr->cmd.size) {
		CAM_ERR(CAM_ISP,
			"Failed! Buf size:%d insufficient, expected size:%d",
			update_hfr->cmd.size, size);
		return -ENOMEM;
	}

	vfe_out_data->cdm_util_ops->cdm_write_regrandom(
		update_hfr->cmd.cmd_buf_addr, j/2, reg_val_pair);

	/* cdm util returns dwords, need to convert to bytes */
	update_hfr->cmd.used_bytes = size * 4;

	return 0;
}

static int cam_vfe_bus_ver3_update_ubwc_config(void *cmd_args)
{
	return 0;
}

static uint32_t cam_vfe_bus_ver3_convert_bytes_to_pixels(uint32_t packer_fmt,
	uint32_t width)
{
	int pixels = 0;

	switch (packer_fmt) {
	case PACKER_FMT_VER3_PLAIN_128:
		pixels = width / 16;
		break;
	case PACKER_FMT_VER3_PLAIN_8:
	case PACKER_FMT_VER3_PLAIN_8_ODD_EVEN:
		pixels = width;
		break;
	case PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10:
	case PACKER_FMT_VER3_PLAIN_8_LSB_MSB_10_ODD_EVEN:
		pixels = width * 8 / 10;
		break;
	case PACKER_FMT_VER3_PLAIN_16_10BPP:
	case PACKER_FMT_VER3_PLAIN_16_12BPP:
	case PACKER_FMT_VER3_PLAIN_16_14BPP:
	case PACKER_FMT_VER3_PLAIN_16_16BPP:
		pixels = width / 2;
		break;
	case PACKER_FMT_VER3_PLAIN_32:
		pixels = width / 4;
		break;
	case PACKER_FMT_VER3_PLAIN_64:
		pixels = width / 8;
		break;
	case PACKER_FMT_VER3_TP_10:
		pixels = width * 3 / 4;
		break;
	case PACKER_FMT_VER3_MAX:
	default:
		CAM_ERR(CAM_ISP, "Invalid packer cfg 0x%x", packer_fmt);
		break;
	}

	return pixels;
}

static int cam_vfe_bus_ver3_update_stripe_cfg(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv                *bus_priv;
	struct cam_isp_hw_dual_isp_update_args      *stripe_args;
	struct cam_vfe_bus_ver3_vfe_out_data        *vfe_out_data = NULL;
	struct cam_vfe_bus_ver3_wm_resource_data    *wm_data = NULL;
	struct cam_isp_dual_stripe_config           *stripe_config;
	uint32_t outport_id, ports_plane_idx, i;

	bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
	stripe_args = (struct cam_isp_hw_dual_isp_update_args *)cmd_args;

	vfe_out_data = (struct cam_vfe_bus_ver3_vfe_out_data *)
		stripe_args->res->res_priv;

	if (!vfe_out_data) {
		CAM_ERR(CAM_ISP, "Failed! Invalid data");
		return -EINVAL;
	}

	outport_id = stripe_args->res->res_id & 0xFF;
	if (stripe_args->res->res_id < CAM_ISP_IFE_OUT_RES_BASE ||
		stripe_args->res->res_id >= CAM_ISP_IFE_OUT_RES_MAX)
		return 0;

	ports_plane_idx = (stripe_args->split_id *
	(stripe_args->dual_cfg->num_ports * CAM_PACKET_MAX_PLANES)) +
	(outport_id * CAM_PACKET_MAX_PLANES);
	for (i = 0; i < vfe_out_data->num_wm; i++) {
		wm_data = vfe_out_data->wm_res[i]->res_priv;
		stripe_config = (struct cam_isp_dual_stripe_config  *)
			&stripe_args->dual_cfg->stripes[ports_plane_idx + i];
		wm_data->width = cam_vfe_bus_ver3_convert_bytes_to_pixels(
			wm_data->pack_fmt, stripe_config->width);
		wm_data->offset = stripe_config->offset;
		CAM_DBG(CAM_ISP, "id:%x WM:%d width:0x%x offset:%x",
			stripe_args->res->res_id, wm_data->index,
			wm_data->width, wm_data->offset);
	}

	return 0;
}

static int cam_vfe_bus_ver3_start_hw(void *hw_priv,
	void *start_hw_args, uint32_t arg_size)
{
	return cam_vfe_bus_ver3_start_vfe_out(hw_priv);
}

static int cam_vfe_bus_ver3_stop_hw(void *hw_priv,
	void *stop_hw_args, uint32_t arg_size)
{
	return cam_vfe_bus_ver3_stop_vfe_out(hw_priv);
}

static int cam_vfe_bus_ver3_init_hw(void *hw_priv,
	void *init_hw_args, uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv    *bus_priv = hw_priv;
	uint32_t                         top_irq_reg_mask[2] = {0};

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Invalid args");
		return -EINVAL;
	}

	top_irq_reg_mask[0] = (1 << 7);

	bus_priv->irq_handle = cam_irq_controller_subscribe_irq(
		bus_priv->common_data.vfe_irq_controller,
		CAM_IRQ_PRIORITY_2,
		top_irq_reg_mask,
		bus_priv,
		cam_vfe_bus_ver3_handle_irq,
		NULL,
		NULL,
		NULL);

	if ((int)bus_priv->irq_handle <= 0) {
		CAM_ERR(CAM_ISP, "Failed to subscribe BUS IRQ");
		return -EFAULT;
	}

	if (bus_priv->tasklet_info != NULL) {
		bus_priv->error_irq_handle = cam_irq_controller_subscribe_irq(
			bus_priv->common_data.bus_irq_controller,
			CAM_IRQ_PRIORITY_0,
			bus_error_irq_mask,
			bus_priv,
			cam_vfe_bus_ver3_err_irq_top_half,
			cam_vfe_bus_ver3_err_irq_bottom_half,
			bus_priv->tasklet_info,
			&tasklet_bh_api);

		if ((int)bus_priv->error_irq_handle <= 0) {
			CAM_ERR(CAM_ISP, "Failed to subscribe BUS Error IRQ");
			return -EFAULT;
		}
	}

	if (bus_priv->tasklet_info != NULL) {
		bus_priv->rup_irq_handle = cam_irq_controller_subscribe_irq(
			bus_priv->common_data.bus_irq_controller,
			CAM_IRQ_PRIORITY_0,
			rup_irq_mask,
			bus_priv,
			cam_vfe_bus_ver3_handle_rup_top_half,
			cam_ife_mgr_do_tasklet_reg_update,
			bus_priv->tasklet_info,
			&tasklet_bh_api);

		if (bus_priv->rup_irq_handle <= 0) {
			CAM_ERR(CAM_ISP, "Failed to subscribe RUP IRQ");
			return -EFAULT;
		}
	}

	// no clock gating at bus input
	CAM_INFO(CAM_ISP, "Overriding clock gating at bus input");
	cam_io_w_mb(0x3FFFFFF, bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->cgc_ovd);

	// BUS_WR_TEST_BUS_CTRL
	cam_io_w_mb(0x0, bus_priv->common_data.mem_base +
		bus_priv->common_data.common_reg->test_bus_ctrl);

	return 0;
}

static int cam_vfe_bus_ver3_deinit_hw(void *hw_priv,
	void *deinit_hw_args, uint32_t arg_size)
{
	struct cam_vfe_bus_ver3_priv    *bus_priv = hw_priv;
	int                              rc = 0, i;

	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "Error: Invalid args");
		return -EINVAL;
	}

	if (bus_priv->error_irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			bus_priv->common_data.bus_irq_controller,
			bus_priv->error_irq_handle);
		if (rc)
			CAM_ERR(CAM_ISP,
				"Failed to unsubscribe error irq rc=%d", rc);

		bus_priv->error_irq_handle = 0;
	}

	if (bus_priv->irq_handle) {
		rc = cam_irq_controller_unsubscribe_irq(
			bus_priv->common_data.vfe_irq_controller,
			bus_priv->irq_handle);
		if (rc)
			CAM_ERR(CAM_ISP,
				"Failed to unsubscribe irq rc=%d", rc);

		bus_priv->irq_handle = 0;
	}

	INIT_LIST_HEAD(&bus_priv->common_data.free_payload_list);
	for (i = 0; i < CAM_VFE_BUS_VER3_PAYLOAD_MAX; i++) {
		INIT_LIST_HEAD(&bus_priv->common_data.evt_payload[i].list);
		list_add_tail(&bus_priv->common_data.evt_payload[i].list,
			&bus_priv->common_data.free_payload_list);
	}

	return rc;
}

static int __cam_vfe_bus_ver3_process_cmd(void *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	return cam_vfe_bus_ver3_process_cmd(priv, cmd_type, cmd_args, arg_size);
}

static int cam_vfe_bus_ver3_process_cmd(
	struct cam_isp_resource_node *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;
	struct cam_vfe_bus_ver3_priv		 *bus_priv;

	if (!priv || !cmd_args) {
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid input arguments");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_ISP_HW_CMD_GET_BUF_UPDATE:
		rc = cam_vfe_bus_ver3_update_wm(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_HFR_UPDATE:
		rc = cam_vfe_bus_ver3_update_hfr(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_GET_SECURE_MODE:
		rc = cam_vfe_bus_ver3_get_secure_mode(priv, cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STRIPE_UPDATE:
		rc = cam_vfe_bus_ver3_update_stripe_cfg(priv,
			cmd_args, arg_size);
		break;
	case CAM_ISP_HW_CMD_STOP_BUS_ERR_IRQ:
		bus_priv = (struct cam_vfe_bus_ver3_priv  *) priv;
		if (bus_priv->error_irq_handle) {
			CAM_DBG(CAM_ISP, "Mask off bus error irq handler");
			rc = cam_irq_controller_unsubscribe_irq(
				bus_priv->common_data.bus_irq_controller,
				bus_priv->error_irq_handle);
			if (rc)
				CAM_ERR(CAM_ISP,
					"Failed to unsubscribe error irq rc=%d",
					rc);

			bus_priv->error_irq_handle = 0;
		}
		break;
	case CAM_ISP_HW_CMD_UBWC_UPDATE:
		rc = cam_vfe_bus_ver3_update_ubwc_config(cmd_args);
		break;
	default:
		CAM_ERR_RATE_LIMIT(CAM_ISP, "Invalid camif process command:%d",
			cmd_type);
		break;
	}

	return rc;
}

int cam_vfe_bus_ver3_init(
	struct cam_hw_soc_info               *soc_info,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *vfe_irq_controller,
	struct cam_vfe_bus                  **vfe_bus)
{
	int i, rc = 0;
	struct cam_vfe_bus_ver3_priv    *bus_priv = NULL;
	struct cam_vfe_bus              *vfe_bus_local;
	struct cam_vfe_bus_ver3_hw_info *ver3_hw_info = bus_hw_info;

	CAM_DBG(CAM_ISP, "Enter");

	if (!soc_info || !hw_intf || !bus_hw_info || !vfe_irq_controller) {
		CAM_ERR(CAM_ISP,
			"Inval_prms soc_info:%pK hw_intf:%pK hw_info%pK",
			soc_info, hw_intf, bus_hw_info);
		CAM_ERR(CAM_ISP, "controller: %pK", vfe_irq_controller);
		rc = -EINVAL;
		goto end;
	}

	vfe_bus_local = kzalloc(sizeof(struct cam_vfe_bus), GFP_KERNEL);
	if (!vfe_bus_local) {
		CAM_DBG(CAM_ISP, "Failed to alloc for vfe_bus");
		rc = -ENOMEM;
		goto end;
	}

	bus_priv = kzalloc(sizeof(struct cam_vfe_bus_ver3_priv),
		GFP_KERNEL);
	if (!bus_priv) {
		CAM_DBG(CAM_ISP, "Failed to alloc for vfe_bus_priv");
		rc = -ENOMEM;
		goto free_bus_local;
	}
	vfe_bus_local->bus_priv = bus_priv;

	bus_priv->num_client                     = ver3_hw_info->num_client;
	bus_priv->num_out                        = ver3_hw_info->num_out;
	bus_priv->common_data.num_sec_out        = 0;
	bus_priv->common_data.secure_mode        = CAM_SECURE_MODE_NON_SECURE;
	bus_priv->common_data.core_index         = soc_info->index;
	bus_priv->common_data.mem_base           =
		CAM_SOC_GET_REG_MAP_START(soc_info, VFE_CORE_BASE_IDX);
	bus_priv->common_data.hw_intf            = hw_intf;
	bus_priv->common_data.vfe_irq_controller = vfe_irq_controller;
	bus_priv->common_data.common_reg         = &ver3_hw_info->common_reg;

	if (strnstr(soc_info->compatible, "lite",
		strlen(soc_info->compatible)) != NULL)
		bus_priv->common_data.is_lite = true;
	else
		bus_priv->common_data.is_lite = false;

	mutex_init(&bus_priv->common_data.bus_mutex);

	rc = cam_irq_controller_init(drv_name, bus_priv->common_data.mem_base,
		&ver3_hw_info->common_reg.irq_reg_info,
		&bus_priv->common_data.bus_irq_controller);
	if (rc) {
		CAM_ERR(CAM_ISP, "cam_irq_controller_init failed");
		goto free_bus_priv;
	}

	INIT_LIST_HEAD(&bus_priv->free_comp_grp);
	INIT_LIST_HEAD(&bus_priv->used_comp_grp);

	for (i = 0; i < bus_priv->num_client; i++) {
		rc = cam_vfe_bus_ver3_init_wm_resource(i, bus_priv, bus_hw_info,
			&bus_priv->bus_client[i]);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Init WM failed rc=%d", rc);
			goto deinit_wm;
		}
	}

	for (i = 0; i < CAM_VFE_BUS_VER3_COMP_GRP_MAX; i++) {
		rc = cam_vfe_bus_ver3_init_comp_grp(i, bus_priv, bus_hw_info,
			&bus_priv->comp_grp[i]);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Init Comp Grp failed rc=%d", rc);
			goto deinit_comp_grp;
		}
	}

	for (i = 0; i < bus_priv->num_out; i++) {
		rc = cam_vfe_bus_ver3_init_vfe_out_resource(i, bus_priv,
			bus_hw_info);
		if (rc < 0) {
			CAM_ERR(CAM_ISP, "Init VFE Out failed rc=%d", rc);
			goto deinit_vfe_out;
		}
	}

	spin_lock_init(&bus_priv->common_data.spin_lock);
	INIT_LIST_HEAD(&bus_priv->common_data.free_payload_list);
	for (i = 0; i < CAM_VFE_BUS_VER3_PAYLOAD_MAX; i++) {
		INIT_LIST_HEAD(&bus_priv->common_data.evt_payload[i].list);
		list_add_tail(&bus_priv->common_data.evt_payload[i].list,
			&bus_priv->common_data.free_payload_list);
	}

	vfe_bus_local->hw_ops.reserve      = cam_vfe_bus_ver3_acquire_vfe_out;
	vfe_bus_local->hw_ops.release      = cam_vfe_bus_ver3_release_vfe_out;
	vfe_bus_local->hw_ops.start        = cam_vfe_bus_ver3_start_hw;
	vfe_bus_local->hw_ops.stop         = cam_vfe_bus_ver3_stop_hw;
	vfe_bus_local->hw_ops.init         = cam_vfe_bus_ver3_init_hw;
	vfe_bus_local->hw_ops.deinit       = cam_vfe_bus_ver3_deinit_hw;
	vfe_bus_local->top_half_handler    = cam_vfe_bus_ver3_handle_irq;
	vfe_bus_local->bottom_half_handler = NULL;
	vfe_bus_local->hw_ops.process_cmd  = __cam_vfe_bus_ver3_process_cmd;

	*vfe_bus = vfe_bus_local;

	CAM_DBG(CAM_ISP, "Exit");
	return rc;

deinit_vfe_out:
	if (i < 0)
		i = CAM_VFE_BUS_VER3_VFE_OUT_MAX;
	for (--i; i >= 0; i--)
		cam_vfe_bus_ver3_deinit_vfe_out_resource(&bus_priv->vfe_out[i]);

deinit_comp_grp:
	if (i < 0)
		i = CAM_VFE_BUS_VER3_COMP_GRP_MAX;
	for (--i; i >= 0; i--)
		cam_vfe_bus_ver3_deinit_comp_grp(&bus_priv->comp_grp[i]);

deinit_wm:
	if (i < 0)
		i = bus_priv->num_client;
	for (--i; i >= 0; i--)
		cam_vfe_bus_ver3_deinit_wm_resource(&bus_priv->bus_client[i]);

free_bus_priv:
	kfree(vfe_bus_local->bus_priv);

free_bus_local:
	kfree(vfe_bus_local);

end:
	return rc;
}

int cam_vfe_bus_ver3_deinit(
	struct cam_vfe_bus                  **vfe_bus)
{
	int i, rc = 0;
	struct cam_vfe_bus_ver3_priv    *bus_priv = NULL;
	struct cam_vfe_bus              *vfe_bus_local;

	if (!vfe_bus || !*vfe_bus) {
		CAM_ERR(CAM_ISP, "Invalid input");
		return -EINVAL;
	}
	vfe_bus_local = *vfe_bus;

	bus_priv = vfe_bus_local->bus_priv;
	if (!bus_priv) {
		CAM_ERR(CAM_ISP, "bus_priv is NULL");
		rc = -ENODEV;
		goto free_bus_local;
	}

	INIT_LIST_HEAD(&bus_priv->common_data.free_payload_list);
	for (i = 0; i < CAM_VFE_BUS_VER3_PAYLOAD_MAX; i++)
		INIT_LIST_HEAD(&bus_priv->common_data.evt_payload[i].list);

	for (i = 0; i < bus_priv->num_client; i++) {
		rc = cam_vfe_bus_ver3_deinit_wm_resource(
			&bus_priv->bus_client[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"Deinit WM failed rc=%d", rc);
	}

	for (i = 0; i < CAM_VFE_BUS_VER3_COMP_GRP_MAX; i++) {
		rc = cam_vfe_bus_ver3_deinit_comp_grp(&bus_priv->comp_grp[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"Deinit Comp Grp failed rc=%d", rc);
	}

	for (i = 0; i < CAM_VFE_BUS_VER3_VFE_OUT_MAX; i++) {
		rc = cam_vfe_bus_ver3_deinit_vfe_out_resource(
			&bus_priv->vfe_out[i]);
		if (rc < 0)
			CAM_ERR(CAM_ISP,
				"Deinit VFE Out failed rc=%d", rc);
	}

	INIT_LIST_HEAD(&bus_priv->free_comp_grp);
	INIT_LIST_HEAD(&bus_priv->used_comp_grp);

	rc = cam_irq_controller_deinit(
		&bus_priv->common_data.bus_irq_controller);
	if (rc)
		CAM_ERR(CAM_ISP,
			"Deinit IRQ Controller failed rc=%d", rc);

	mutex_destroy(&bus_priv->common_data.bus_mutex);
	kfree(vfe_bus_local->bus_priv);

free_bus_local:
	kfree(vfe_bus_local);

	*vfe_bus = NULL;

	return rc;
}
