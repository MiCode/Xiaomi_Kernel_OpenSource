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

#define pr_fmt(fmt) "%s:%d " fmt, __func__, __LINE__

#include <linux/ratelimit.h>
#include <linux/slab.h>
#include "cam_io_util.h"
#include "cam_cdm_util.h"
#include "cam_hw_intf.h"
#include "cam_vfe_hw_intf.h"
#include "cam_irq_controller.h"
#include "cam_vfe_bus.h"
#include "cam_vfe_bus_ver2.h"
#include "cam_vfe_core.h"

#undef CDBG
#define CDBG(fmt, args...) pr_debug(fmt, ##args)

#define FRAME_BASED_EN 0

#define MAX_BUF_UPDATE_REG_NUM   20
#define MAX_REG_VAL_PAIR_SIZE    \
		(MAX_BUF_UPDATE_REG_NUM * 2 * CAM_PACKET_MAX_PLANES)

#define CAM_VFE_ADD_REG_VAL_PAIR(buf_array, index, offset, val)    \
		do {                                               \
			buf_array[index++] = offset;               \
			buf_array[index++] = val;                  \
		} while (0)

static uint32_t irq_reg_offset[CAM_IFE_BUS_IRQ_REGISTERS_MAX] = {
	0x0000205C,
	0x00002060,
	0x00002064,
	0x0000206C,
	0x00002070,
	0x00002074,
	0x00002078,
};

enum cam_vfe_bus_packer_format {
	PACKER_FMT_PLAIN_128                   = 0x0,
	PACKER_FMT_PLAIN_8                     = 0x1,
	PACKER_FMT_PLAIN_16_10BPP              = 0x2,
	PACKER_FMT_PLAIN_16_12BPP              = 0x3,
	PACKER_FMT_PLAIN_16_14BPP              = 0x4,
	PACKER_FMT_PLAIN_16_16BPP              = 0x5,
	PACKER_FMT_ARGB_10                     = 0x6,
	PACKER_FMT_ARGB_12                     = 0x7,
	PACKER_FMT_ARGB_14                     = 0x8,
	PACKER_FMT_PLAIN_32_20BPP              = 0x9,
	PACKER_FMT_PLAIN_64                    = 0xA,
	PACKER_FMT_TP_10                       = 0xB,
	PACKER_FMT_PLAIN_32_32BPP              = 0xC,
	PACKER_FMT_PLAIN_8_ODD_EVEN            = 0xD,
	PACKER_FMT_PLAIN_8_LSB_MSB_10          = 0xE,
	PACKER_FMT_PLAIN_8_LSB_MSB_10_ODD_EVEN = 0xF,
	PACKER_FMT_MAX                         = 0xF,
};

struct cam_vfe_bus_ver2_common_data {
	void __iomem                               *mem_base;
	struct cam_hw_intf                         *hw_intf;
	void                                       *bus_irq_controller;
	void                                       *vfe_irq_controller;
	struct cam_vfe_bus_ver2_reg_offset_common  *common_reg;
	uint32_t                                    io_buf_update[
							MAX_REG_VAL_PAIR_SIZE];
};

struct cam_vfe_bus_ver2_wm_resource_data {
	uint32_t             index;
	struct cam_vfe_bus_ver2_common_data            *common_data;
	struct cam_vfe_bus_ver2_reg_offset_bus_client  *hw_regs;

	uint32_t             irq_enabled;

	uint32_t             init_cfg_done;
	uint32_t             offset;
	uint32_t             width;
	uint32_t             height;
	uint32_t             stride;
	uint32_t             format;
	enum cam_vfe_bus_packer_format pack_fmt;

	uint32_t             burst_len;
	uint32_t             frame_based;

	uint32_t             en_ubwc;
	uint32_t             packer_cfg;
	uint32_t             tile_cfg;
	uint32_t             h_init;
	uint32_t             v_init;
	uint32_t             ubwc_meta_stride;
	uint32_t             ubwc_mode_cfg;
	uint32_t             ubwc_meta_offset;

	uint32_t             irq_subsample_period;
	uint32_t             irq_subsample_pattern;
	uint32_t             framedrop_period;
	uint32_t             framedrop_pattern;

	uint32_t             en_cfg;
};

struct cam_vfe_bus_ver2_comp_grp_data {
	enum cam_vfe_bus_ver2_comp_grp_type          comp_grp_type;
	struct cam_vfe_bus_ver2_common_data         *common_data;
	struct cam_vfe_bus_ver2_reg_offset_comp_grp *hw_regs;

	uint32_t                         irq_enabled;
	uint32_t                         comp_grp_local_idx;
	uint32_t                         unique_id;

	uint32_t                         is_master;
	uint32_t                         dual_slave_core;
	uint32_t                         intra_client_mask;
	uint32_t                         composite_mask;
};

struct cam_vfe_bus_ver2_vfe_out_data {
	uint32_t                              out_type;
	struct cam_vfe_bus_ver2_common_data  *common_data;

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
};


struct cam_vfe_bus_ver2_priv {
	struct cam_vfe_bus_ver2_common_data common_data;

	struct cam_isp_resource_node  bus_client[CAM_VFE_BUS_VER2_MAX_CLIENTS];
	struct cam_isp_resource_node  comp_grp[CAM_VFE_BUS_VER2_COMP_GRP_MAX];
	struct cam_isp_resource_node  vfe_out[CAM_VFE_BUS_VER2_VFE_OUT_MAX];

	struct list_head                    free_comp_grp;
	struct list_head                    free_dual_comp_grp;
	struct list_head                    used_comp_grp;

	struct cam_vfe_bus_irq_evt_payload  evt_payload[128];
	struct list_head                    free_payload_list;
};

static int cam_vfe_bus_put_evt_payload(void     *core_info,
	struct cam_vfe_bus_irq_evt_payload     **evt_payload);

static int cam_vfe_bus_ver2_get_intra_client_mask(
	enum cam_vfe_bus_ver2_vfe_core_id  dual_slave_core,
	enum cam_vfe_bus_ver2_vfe_core_id  current_core,
	uint32_t                          *intra_client_mask)
{
	int rc = 0;

	*intra_client_mask = 0;

	if (dual_slave_core == current_core) {
		pr_err("Invalid params. Same core as Master and Slave\n");
		return -EINVAL;
	}

	switch (current_core) {
	case CAM_VFE_BUS_VER2_VFE_CORE_0:
		switch (dual_slave_core) {
		case CAM_VFE_BUS_VER2_VFE_CORE_1:
			*intra_client_mask = 0x1;
			break;
		case CAM_VFE_BUS_VER2_VFE_CORE_2:
			*intra_client_mask = 0x2;
			break;
		default:
			pr_err("Invalid value for slave core %u\n",
				dual_slave_core);
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_CORE_1:
		switch (dual_slave_core) {
		case CAM_VFE_BUS_VER2_VFE_CORE_0:
			*intra_client_mask = 0x1;
			break;
		case CAM_VFE_BUS_VER2_VFE_CORE_2:
			*intra_client_mask = 0x2;
			break;
		default:
			pr_err("Invalid value for slave core %u\n",
				dual_slave_core);
			rc = -EINVAL;
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_CORE_2:
		switch (dual_slave_core) {
		case CAM_VFE_BUS_VER2_VFE_CORE_0:
			*intra_client_mask = 0x1;
			break;
		case CAM_VFE_BUS_VER2_VFE_CORE_1:
			*intra_client_mask = 0x2;
			break;
		default:
			pr_err("Invalid value for slave core %u\n",
				dual_slave_core);
			rc = -EINVAL;
			break;
		}
		break;
	default:
		pr_err("Invalid value for master core %u\n", current_core);
		rc = -EINVAL;
		break;
	}

	return rc;
}

static enum cam_vfe_bus_ver2_vfe_out_type
	cam_vfe_bus_get_out_res_id(uint32_t res_type)
{
	switch (res_type) {
	case CAM_ISP_IFE_OUT_RES_FULL:
		return CAM_VFE_BUS_VER2_VFE_OUT_FULL;
	case CAM_ISP_IFE_OUT_RES_DS4:
		return CAM_VFE_BUS_VER2_VFE_OUT_DS4;
	case CAM_ISP_IFE_OUT_RES_DS16:
		return CAM_VFE_BUS_VER2_VFE_OUT_DS16;
	case CAM_ISP_IFE_OUT_RES_FD:
		return CAM_VFE_BUS_VER2_VFE_OUT_FD;
	case CAM_ISP_IFE_OUT_RES_RAW_DUMP:
		return CAM_VFE_BUS_VER2_VFE_OUT_RAW_DUMP;
	case CAM_ISP_IFE_OUT_RES_PDAF:
		return CAM_VFE_BUS_VER2_VFE_OUT_PDAF;
	case CAM_ISP_IFE_OUT_RES_RDI_0:
		return CAM_VFE_BUS_VER2_VFE_OUT_RDI0;
	case CAM_ISP_IFE_OUT_RES_RDI_1:
		return CAM_VFE_BUS_VER2_VFE_OUT_RDI1;
	case CAM_ISP_IFE_OUT_RES_RDI_2:
		return CAM_VFE_BUS_VER2_VFE_OUT_RDI2;
	case CAM_ISP_IFE_OUT_RES_STATS_HDR_BE:
		return CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BE;
	case CAM_ISP_IFE_OUT_RES_STATS_HDR_BHIST:
		return CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BHIST;
	case CAM_ISP_IFE_OUT_RES_STATS_TL_BG:
		return CAM_VFE_BUS_VER2_VFE_OUT_STATS_TL_BG;
	case CAM_ISP_IFE_OUT_RES_STATS_BF:
		return CAM_VFE_BUS_VER2_VFE_OUT_STATS_BF;
	case CAM_ISP_IFE_OUT_RES_STATS_AWB_BG:
		return CAM_VFE_BUS_VER2_VFE_OUT_STATS_AWB_BG;
	case CAM_ISP_IFE_OUT_RES_STATS_BHIST:
		return CAM_VFE_BUS_VER2_VFE_OUT_STATS_BHIST;
	case CAM_ISP_IFE_OUT_RES_STATS_RS:
		return CAM_VFE_BUS_VER2_VFE_OUT_STATS_RS;
	case CAM_ISP_IFE_OUT_RES_STATS_CS:
		return CAM_VFE_BUS_VER2_VFE_OUT_STATS_CS;
	case CAM_ISP_IFE_OUT_RES_STATS_IHIST:
		return CAM_VFE_BUS_VER2_VFE_OUT_STATS_IHIST;
	default:
		return CAM_VFE_BUS_VER2_VFE_OUT_MAX;
	}
}

static int cam_vfe_bus_get_num_wm(
	enum cam_vfe_bus_ver2_vfe_out_type    res_type,
	uint32_t                              format)
{
	switch (res_type) {
	case CAM_VFE_BUS_VER2_VFE_OUT_RDI0:
	case CAM_VFE_BUS_VER2_VFE_OUT_RDI1:
	case CAM_VFE_BUS_VER2_VFE_OUT_RDI2:
		switch (format) {
		case CAM_FORMAT_MIPI_RAW_8:
		case CAM_FORMAT_MIPI_RAW_10:
		case CAM_FORMAT_MIPI_RAW_12:
		case CAM_FORMAT_MIPI_RAW_14:
		case CAM_FORMAT_MIPI_RAW_16:
		case CAM_FORMAT_MIPI_RAW_20:
		case CAM_FORMAT_PLAIN128:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_FULL:
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
			return 2;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_FD:
		switch (format) {
		case CAM_FORMAT_NV21:
		case CAM_FORMAT_NV12:
		case CAM_FORMAT_PLAIN8:
		case CAM_FORMAT_TP10:
		case CAM_FORMAT_PLAIN16_10:
			return 2;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_DS4:
	case CAM_VFE_BUS_VER2_VFE_OUT_DS16:
		switch (format) {
		case CAM_FORMAT_PD8:
		case CAM_FORMAT_PD10:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_RAW_DUMP:
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
	case CAM_VFE_BUS_VER2_VFE_OUT_PDAF:
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
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BE:
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BHIST:
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_TL_BG:
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_BF:
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_AWB_BG:
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_BHIST:
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_CS:
		switch (format) {
		case CAM_FORMAT_PLAIN64:
			return 1;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_RS:
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_IHIST:
		switch (format) {
		case CAM_FORMAT_PLAIN16_16:
			return 1;
		default:
			break;
		}
		break;
	default:
		break;
	}

	pr_err("Unsupported format %u for resource_type %u", format, res_type);

	return -EINVAL;
}

static int cam_vfe_bus_get_wm_idx(
	enum cam_vfe_bus_ver2_vfe_out_type vfe_out_res_id,
	enum cam_vfe_bus_plane_type plane)
{
	int wm_idx = -1;

	switch (vfe_out_res_id) {
	case CAM_VFE_BUS_VER2_VFE_OUT_FULL:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 3;
			break;
		case PLANE_C:
			wm_idx = 4;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_DS4:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 5;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_DS16:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 6;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_FD:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 7;
			break;
		case PLANE_C:
			wm_idx = 8;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_RAW_DUMP:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 9;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_PDAF:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 10;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_RDI0:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 0;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_RDI1:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 1;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_RDI2:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 2;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BE:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 11;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_HDR_BHIST:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 12;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_TL_BG:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 13;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_BF:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 14;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_AWB_BG:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 15;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_BHIST:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 16;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_RS:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 17;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_CS:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 18;
			break;
		default:
			break;
		}
		break;
	case CAM_VFE_BUS_VER2_VFE_OUT_STATS_IHIST:
		switch (plane) {
		case PLANE_Y:
			wm_idx = 19;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return wm_idx;
}

static enum cam_vfe_bus_packer_format
	cam_vfe_bus_get_packer_fmt(uint32_t out_fmt)
{
	switch (out_fmt) {
	case CAM_FORMAT_NV21:
	case CAM_FORMAT_NV12:
		return PACKER_FMT_PLAIN_8;
	default:
		return PACKER_FMT_MAX;
	}
}

static int cam_vfe_bus_acquire_wm(
	struct cam_vfe_bus_ver2_priv          *ver2_bus_priv,
	struct cam_isp_out_port_info          *out_port_info,
	enum cam_vfe_bus_ver2_vfe_out_type     vfe_out_res_id,
	enum cam_vfe_bus_plane_type            plane,
	enum cam_isp_hw_split_id               split_id,
	uint32_t                               subscribe_irq,
	struct cam_isp_resource_node         **wm_res,
	uint32_t                              *client_done_mask)
{
	uint32_t wm_idx = 0;
	struct cam_isp_resource_node              *wm_res_local = NULL;
	struct cam_vfe_bus_ver2_wm_resource_data  *rsrc_data = NULL;

	*wm_res = NULL;
	*client_done_mask = 0;

	/* No need to allocate for BUS VER2. VFE OUT to WM is fixed. */
	wm_idx = cam_vfe_bus_get_wm_idx(vfe_out_res_id, plane);
	if (wm_idx < 0 || wm_idx >= CAM_VFE_BUS_VER2_MAX_CLIENTS) {
		pr_err("Unsupported VFE out %d plane %d\n",
			vfe_out_res_id, plane);
		return -EINVAL;
	}

	wm_res_local = &ver2_bus_priv->bus_client[wm_idx];
	wm_res_local->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	rsrc_data = wm_res_local->res_priv;
	rsrc_data->irq_enabled = subscribe_irq;
	rsrc_data->format = out_port_info->format;
	rsrc_data->pack_fmt = cam_vfe_bus_get_packer_fmt(rsrc_data->format);

	rsrc_data->width = out_port_info->width;
	rsrc_data->height = out_port_info->height;

	if (rsrc_data->index < 3) {
		rsrc_data->width = rsrc_data->width * 5/4 * rsrc_data->height;
		rsrc_data->height = 1;
		rsrc_data->pack_fmt = 0x0;
		rsrc_data->en_cfg = 0x3;
	} else if (rsrc_data->index < 5) {
		switch (plane) {
		case PLANE_Y:
			switch (rsrc_data->format) {
			case CAM_FORMAT_UBWC_NV12:
			case CAM_FORMAT_UBWC_NV12_4R:
			case CAM_FORMAT_UBWC_TP10:
				rsrc_data->en_ubwc = 1;
				break;
			default:
				break;
			}
			break;
		case PLANE_C:
			switch (rsrc_data->format) {
			case CAM_FORMAT_NV21:
			case CAM_FORMAT_NV12:
				rsrc_data->height /= 2;
				break;
			case CAM_FORMAT_UBWC_NV12:
			case CAM_FORMAT_UBWC_NV12_4R:
			case CAM_FORMAT_UBWC_TP10:
				rsrc_data->height /= 2;
				rsrc_data->en_ubwc = 1;
				break;
			default:
				break;
			}
			break;
		default:
			pr_err("Invalid plane type %d\n", plane);
			return -EINVAL;
		}
		rsrc_data->pack_fmt = 0xE;
		rsrc_data->en_cfg = 0x1;
	} else {
		rsrc_data->width = rsrc_data->width * 4;
		rsrc_data->height = rsrc_data->height / 2;
		rsrc_data->pack_fmt = 0x0;
		rsrc_data->en_cfg = 0x1;
	}

	if (vfe_out_res_id >= CAM_ISP_IFE_OUT_RES_RDI_0 &&
		vfe_out_res_id <= CAM_ISP_IFE_OUT_RES_RDI_3)
		rsrc_data->frame_based = 1;

	*client_done_mask = (1 << wm_idx);
	*wm_res = wm_res_local;

	return 0;
}

static int cam_vfe_bus_release_wm(void   *bus_priv,
	struct cam_isp_resource_node     *wm_res)
{
	struct cam_vfe_bus_ver2_wm_resource_data   *rsrc_data =
		wm_res->res_priv;

	rsrc_data->irq_enabled = 0;
	rsrc_data->offset = 0;
	rsrc_data->width = 0;
	rsrc_data->height = 0;
	rsrc_data->stride = 0;
	rsrc_data->format = 0;
	rsrc_data->pack_fmt = 0;
	rsrc_data->burst_len = 0;
	rsrc_data->frame_based = 0;
	rsrc_data->irq_subsample_period = 0;
	rsrc_data->irq_subsample_pattern = 0;
	rsrc_data->framedrop_period = 0;
	rsrc_data->framedrop_pattern = 0;
	rsrc_data->packer_cfg = 0;
	rsrc_data->en_ubwc = 0;
	rsrc_data->tile_cfg = 0;
	rsrc_data->h_init = 0;
	rsrc_data->v_init = 0;
	rsrc_data->ubwc_meta_stride = 0;
	rsrc_data->ubwc_mode_cfg = 0;
	rsrc_data->ubwc_meta_offset = 0;
	rsrc_data->init_cfg_done = 0;
	rsrc_data->en_cfg = 0;
	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_vfe_bus_start_wm(struct cam_isp_resource_node *wm_res)
{
	int rc = 0;
	struct cam_vfe_bus_ver2_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_vfe_bus_ver2_common_data        *common_data =
		rsrc_data->common_data;

	cam_io_w_mb(0, common_data->mem_base + rsrc_data->hw_regs->header_addr);
	cam_io_w_mb(0, common_data->mem_base + rsrc_data->hw_regs->header_cfg);
	cam_io_w_mb(0, common_data->mem_base + rsrc_data->hw_regs->frame_inc);
	cam_io_w(0xf, common_data->mem_base + rsrc_data->hw_regs->burst_limit);

	cam_io_w_mb(rsrc_data->width,
		common_data->mem_base + rsrc_data->hw_regs->buffer_width_cfg);
	cam_io_w(rsrc_data->height,
		common_data->mem_base + rsrc_data->hw_regs->buffer_height_cfg);
	cam_io_w(rsrc_data->pack_fmt,
		common_data->mem_base + rsrc_data->hw_regs->packer_cfg);

	cam_io_w(0xFFFFFFFF, common_data->mem_base +
		rsrc_data->hw_regs->irq_subsample_pattern);
	cam_io_w(0x0, common_data->mem_base +
		rsrc_data->hw_regs->irq_subsample_period);

	cam_io_w(0xFFFFFFFF,
		common_data->mem_base + rsrc_data->hw_regs->framedrop_pattern);
	cam_io_w(0x0,
		common_data->mem_base + rsrc_data->hw_regs->framedrop_period);

	CDBG("WM res %d width = %d, height = %d\n", rsrc_data->index,
		rsrc_data->width, rsrc_data->height);
	CDBG("WM res %d pk_fmt = %d\n", rsrc_data->index,
		rsrc_data->pack_fmt & PACKER_FMT_MAX);
	CDBG("WM res %d stride = %d, burst len = %d\n",
		rsrc_data->index, rsrc_data->stride, 0xf);
	CDBG("enable WM res %d offset 0x%x val 0x%x\n", rsrc_data->index,
		(uint32_t) rsrc_data->hw_regs->cfg, rsrc_data->en_cfg);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	return rc;
}

static int cam_vfe_bus_stop_wm(struct cam_isp_resource_node *wm_res)
{
	int rc = 0;
	struct cam_vfe_bus_ver2_wm_resource_data   *rsrc_data =
		wm_res->res_priv;
	struct cam_vfe_bus_ver2_common_data        *common_data =
		rsrc_data->common_data;

	/* Disble WM */
	cam_io_w_mb(0x0,
		common_data->mem_base + rsrc_data->hw_regs->cfg);

	CDBG("irq_enabled %d", rsrc_data->irq_enabled);
	/* Unsubscribe IRQ */
	if (rsrc_data->irq_enabled) {
		/*
		 * Currently all WM IRQ are unsubscribed in one place. Need to
		 * make it dynamic.
		 */
	}

	/* Halt & Reset WM */
	cam_io_w_mb(BIT(rsrc_data->index),
		common_data->mem_base + common_data->common_reg->sw_reset);

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return rc;
}

static int cam_vfe_bus_handle_wm_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_bus_handle_wm_done_bottom_half(void *wm_node,
	void *evt_payload_priv)
{
	int rc = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node          *wm_res = wm_node;
	struct cam_vfe_bus_irq_evt_payload    *evt_payload = evt_payload_priv;
	struct cam_vfe_bus_ver2_wm_resource_data *rsrc_data =
		(wm_res == NULL) ? NULL : wm_res->res_priv;
	uint32_t  *cam_ife_irq_regs;
	uint32_t   status_reg;

	if (!evt_payload || !rsrc_data)
		return rc;

	cam_ife_irq_regs = evt_payload->irq_reg_val;
	status_reg = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_STATUS1];

	if (status_reg & BIT(rsrc_data->index)) {
		cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_STATUS1] &=
			~BIT(rsrc_data->index);
		rc = CAM_VFE_IRQ_STATUS_SUCCESS;
	}

	if (rc == CAM_VFE_IRQ_STATUS_SUCCESS)
		cam_vfe_bus_put_evt_payload(evt_payload->core_info,
			&evt_payload);

	return rc;
}

static int cam_vfe_bus_init_wm_resource(uint32_t index,
	struct cam_vfe_bus_ver2_priv    *ver2_bus_priv,
	struct cam_vfe_bus_ver2_hw_info *ver2_hw_info,
	struct cam_isp_resource_node    *wm_res)
{
	int rc = 0;
	struct cam_vfe_bus_ver2_wm_resource_data *rsrc_data;

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_ver2_wm_resource_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		CDBG("Failed to alloc for wm res priv\n");
		rc = -ENOMEM;
		return rc;
	}
	wm_res->res_priv = rsrc_data;

	rsrc_data->index = index;
	rsrc_data->hw_regs = &ver2_hw_info->bus_client_reg[index];
	rsrc_data->common_data = &ver2_bus_priv->common_data;

	wm_res->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&wm_res->list);

	wm_res->start = cam_vfe_bus_start_wm;
	wm_res->stop = cam_vfe_bus_stop_wm;
	wm_res->top_half_handler = cam_vfe_bus_handle_wm_done_top_half;
	wm_res->bottom_half_handler = cam_vfe_bus_handle_wm_done_bottom_half;
	wm_res->hw_intf = ver2_bus_priv->common_data.hw_intf;

	return rc;
}

static void cam_vfe_bus_add_wm_to_comp_grp(
	struct cam_isp_resource_node    *comp_grp,
	uint32_t                         composite_mask)
{
	struct cam_vfe_bus_ver2_comp_grp_data  *rsrc_data = comp_grp->res_priv;

	rsrc_data->composite_mask |= composite_mask;
}

static void cam_vfe_bus_match_comp_grp(
	struct cam_vfe_bus_ver2_priv  *ver2_bus_priv,
	struct cam_isp_resource_node **comp_grp,
	uint32_t                       comp_grp_local_idx,
	uint32_t                       unique_id)
{
	struct cam_vfe_bus_ver2_comp_grp_data  *rsrc_data = NULL;
	struct cam_isp_resource_node           *comp_grp_local = NULL;

	list_for_each_entry(comp_grp_local,
		&ver2_bus_priv->used_comp_grp, list) {
		rsrc_data = comp_grp_local->res_priv;
		if (rsrc_data->comp_grp_local_idx == comp_grp_local_idx &&
			rsrc_data->unique_id == unique_id) {
			/* Match found */
			*comp_grp = comp_grp_local;
			return;
		}
	}

	*comp_grp = NULL;
}

static int cam_vfe_bus_acquire_comp_grp(
	struct cam_vfe_bus_ver2_priv        *ver2_bus_priv,
	struct cam_isp_out_port_info        *out_port_info,
	uint32_t                             unique_id,
	uint32_t                             is_dual,
	uint32_t                             is_master,
	enum cam_vfe_bus_ver2_vfe_core_id    dual_slave_core,
	struct cam_isp_resource_node       **comp_grp)
{
	int rc = 0;
	struct cam_isp_resource_node           *comp_grp_local = NULL;
	struct cam_vfe_bus_ver2_comp_grp_data  *rsrc_data = NULL;

	/* Check if matching comp_grp already acquired */
	cam_vfe_bus_match_comp_grp(ver2_bus_priv, &comp_grp_local,
		out_port_info->comp_grp_id, unique_id);

	if (!comp_grp_local) {
		/* First find a free group */
		if (is_dual) {
			if (list_empty(&ver2_bus_priv->free_dual_comp_grp)) {
				pr_err("No Free Composite Group\n");
				return -ENODEV;
			}
			comp_grp_local = list_first_entry(
				&ver2_bus_priv->free_dual_comp_grp,
				struct cam_isp_resource_node, list);
			rsrc_data = comp_grp_local->res_priv;
			rc = cam_vfe_bus_ver2_get_intra_client_mask(
				dual_slave_core,
				comp_grp_local->hw_intf->hw_idx,
				&rsrc_data->intra_client_mask);
		} else {
			if (list_empty(&ver2_bus_priv->free_comp_grp)) {
				pr_err("No Free Composite Group\n");
				return -ENODEV;
			}
			comp_grp_local = list_first_entry(
				&ver2_bus_priv->free_comp_grp,
				struct cam_isp_resource_node, list);
			rsrc_data = comp_grp_local->res_priv;
		}

		list_del(&comp_grp_local->list);
		comp_grp_local->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

		rsrc_data->is_master = is_master;
		rsrc_data->composite_mask = 0;
		rsrc_data->unique_id = unique_id;
		rsrc_data->comp_grp_local_idx = out_port_info->comp_grp_id;

		list_add_tail(&comp_grp_local->list,
			&ver2_bus_priv->used_comp_grp);

	} else {
		rsrc_data = comp_grp_local->res_priv;
		/* Do not support runtime change in composite mask */
		if (comp_grp_local->res_state ==
			CAM_ISP_RESOURCE_STATE_STREAMING) {
			pr_err("Invalid State %d Comp Grp %u\n",
				comp_grp_local->res_state,
				rsrc_data->comp_grp_type);
			return -EBUSY;
		}
	}

	*comp_grp = comp_grp_local;

	return rc;
}

static int cam_vfe_bus_release_comp_grp(
	struct cam_vfe_bus_ver2_priv         *ver2_bus_priv,
	struct cam_isp_resource_node         *in_comp_grp)
{
	struct cam_isp_resource_node           *comp_grp = NULL;
	struct cam_vfe_bus_ver2_comp_grp_data  *in_rsrc_data = NULL;
	int match_found = 0;

	if (!in_comp_grp) {
		pr_err("Invalid Params Comp Grp %pK\n", in_rsrc_data);
		return -EINVAL;
	}

	if (in_comp_grp->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		/* Already Released. Do Nothing */
		return 0;
	}

	in_rsrc_data = in_comp_grp->res_priv;

	list_for_each_entry(comp_grp, &ver2_bus_priv->used_comp_grp, list) {
		if (comp_grp == in_comp_grp) {
			match_found = 1;
			break;
		}
	}

	if (!match_found) {
		pr_err("Could not find matching Comp Grp type %u\n",
			in_rsrc_data->comp_grp_type);
		return -ENODEV;
	}


	list_del(&comp_grp->list);
	if (in_rsrc_data->comp_grp_type >= CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0 &&
		in_rsrc_data->comp_grp_type <= CAM_VFE_BUS_VER2_COMP_GRP_DUAL_5)
		list_add_tail(&comp_grp->list,
			&ver2_bus_priv->free_dual_comp_grp);
	else if (in_rsrc_data->comp_grp_type >= CAM_VFE_BUS_VER2_COMP_GRP_0
		&& in_rsrc_data->comp_grp_type <= CAM_VFE_BUS_VER2_COMP_GRP_5)
		list_add_tail(&comp_grp->list, &ver2_bus_priv->free_comp_grp);

	in_rsrc_data->unique_id = 0;
	in_rsrc_data->comp_grp_local_idx = 0;
	in_rsrc_data->composite_mask = 0;
	in_rsrc_data->dual_slave_core = CAM_VFE_BUS_VER2_VFE_CORE_MAX;

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_vfe_bus_start_comp_grp(struct cam_isp_resource_node *comp_grp)
{
	int rc = 0;
	struct cam_vfe_bus_ver2_comp_grp_data      *rsrc_data =
		comp_grp->res_priv;
	struct cam_vfe_bus_ver2_common_data        *common_data =
		rsrc_data->common_data;

	/*
	 * Individual Comp_Grp Subscribe IRQ can be done here once
	 * dynamic IRQ enable support is added.
	 */

	cam_io_w_mb(rsrc_data->composite_mask, common_data->mem_base +
		rsrc_data->hw_regs->comp_mask);

	CDBG("composite_mask is 0x%x\n", rsrc_data->composite_mask);
	CDBG("composite_mask addr 0x%x\n",  rsrc_data->hw_regs->comp_mask);

	if (rsrc_data->comp_grp_type >= CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0 &&
		rsrc_data->comp_grp_type <= CAM_VFE_BUS_VER2_COMP_GRP_DUAL_5 &&
		rsrc_data->is_master) {
		int dual_comp_grp = (rsrc_data->comp_grp_type -
			CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0);
		int intra_client_en = cam_io_r_mb(common_data->mem_base +
			common_data->common_reg->dual_master_comp_cfg);

		/* 2 Bits per comp_grp. Hence left shift by comp_grp * 2 */
		intra_client_en |=
			(rsrc_data->intra_client_mask << dual_comp_grp * 2);

		cam_io_w_mb(intra_client_en, common_data->mem_base +
			common_data->common_reg->dual_master_comp_cfg);
	}

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;
	return rc;
}

static int cam_vfe_bus_stop_comp_grp(struct cam_isp_resource_node *comp_grp)
{
	int rc = 0;
	struct cam_vfe_bus_ver2_comp_grp_data      *rsrc_data =
		comp_grp->res_priv;
	struct cam_vfe_bus_ver2_common_data        *common_data =
		rsrc_data->common_data;

	/* Unsubscribe IRQ */

	cam_io_w_mb(rsrc_data->composite_mask, common_data->mem_base +
		rsrc_data->hw_regs->comp_mask);
	if (rsrc_data->comp_grp_type >= CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0 &&
		rsrc_data->comp_grp_type <= CAM_VFE_BUS_VER2_COMP_GRP_DUAL_5 &&
		rsrc_data->is_master) {
		int dual_comp_grp = (rsrc_data->comp_grp_type -
			CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0);
		int intra_client_en = cam_io_r_mb(common_data->mem_base +
			common_data->common_reg->dual_master_comp_cfg);

		/* 2 Bits per comp_grp. Hence left shift by comp_grp * 2 */
		intra_client_en &=
			~(rsrc_data->intra_client_mask << dual_comp_grp * 2);

		cam_io_w_mb(intra_client_en, common_data->mem_base +
			common_data->common_reg->dual_master_comp_cfg);
	}

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;

	return rc;
}

static int cam_vfe_bus_handle_comp_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_bus_handle_comp_done_bottom_half(
	void                *handler_priv,
	void                *evt_payload_priv)
{
	int rc = CAM_VFE_IRQ_STATUS_ERR;
	struct cam_isp_resource_node          *comp_grp = handler_priv;
	struct cam_vfe_bus_irq_evt_payload    *evt_payload = evt_payload_priv;
	struct cam_vfe_bus_ver2_comp_grp_data *rsrc_data = comp_grp->res_priv;
	uint32_t                              *cam_ife_irq_regs;
	uint32_t                               status_reg;
	uint32_t                               comp_err_reg;
	uint32_t                               comp_grp_id;

	if (!evt_payload)
		return rc;

	cam_ife_irq_regs = evt_payload->irq_reg_val;

	CDBG("comp grp type %d\n", rsrc_data->comp_grp_type);
	switch (rsrc_data->comp_grp_type) {
	case CAM_VFE_BUS_VER2_COMP_GRP_0:
	case CAM_VFE_BUS_VER2_COMP_GRP_1:
	case CAM_VFE_BUS_VER2_COMP_GRP_2:
	case CAM_VFE_BUS_VER2_COMP_GRP_3:
	case CAM_VFE_BUS_VER2_COMP_GRP_4:
	case CAM_VFE_BUS_VER2_COMP_GRP_5:
		comp_grp_id = (rsrc_data->comp_grp_type -
			CAM_VFE_BUS_VER2_COMP_GRP_0);

		/* Check for Regular composite error */
		status_reg = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_STATUS0];

		comp_err_reg = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_COMP_ERR];
		if ((status_reg & BIT(11)) &&
			(comp_err_reg & rsrc_data->composite_mask)) {
			/* Check for Regular composite error */
			rc = CAM_VFE_IRQ_STATUS_ERR_COMP;
			break;
		}

		comp_err_reg = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_COMP_OWRT];
		/* Check for Regular composite Overwrite */
		if ((status_reg & BIT(12)) &&
			(comp_err_reg & rsrc_data->composite_mask)) {
			rc = CAM_VFE_IRQ_STATUS_COMP_OWRT;
			break;
		}

		/* Regular Composite SUCCESS */
		if (status_reg & BIT(comp_grp_id + 5)) {
			cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_STATUS0] &=
				~BIT(comp_grp_id + 5);
			rc = CAM_VFE_IRQ_STATUS_SUCCESS;
		}

		CDBG("status reg = 0x%x, bit index = %d\n",
			status_reg, (comp_grp_id + 5));
		break;

	case CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0:
	case CAM_VFE_BUS_VER2_COMP_GRP_DUAL_1:
	case CAM_VFE_BUS_VER2_COMP_GRP_DUAL_2:
	case CAM_VFE_BUS_VER2_COMP_GRP_DUAL_3:
	case CAM_VFE_BUS_VER2_COMP_GRP_DUAL_4:
	case CAM_VFE_BUS_VER2_COMP_GRP_DUAL_5:
		comp_grp_id = (rsrc_data->comp_grp_type -
			CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0);

		/* Check for DUAL composite error */
		status_reg = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_STATUS2];

		comp_err_reg = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_DUAL_COMP_ERR];
		if ((status_reg & BIT(6)) &&
			(comp_err_reg & rsrc_data->composite_mask)) {
			/* Check for DUAL composite error */
			rc = CAM_VFE_IRQ_STATUS_ERR_COMP;
			break;
		}

		/* Check for Dual composite Overwrite */
		comp_err_reg = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_DUAL_COMP_OWRT];
		if ((status_reg & BIT(7)) &&
			(comp_err_reg & rsrc_data->composite_mask)) {
			rc = CAM_VFE_IRQ_STATUS_COMP_OWRT;
			break;
		}

		/* DUAL Composite SUCCESS */
		if (status_reg & BIT(comp_grp_id)) {
			cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_STATUS2] &=
				~BIT(comp_grp_id + 5);
			rc = CAM_VFE_IRQ_STATUS_SUCCESS;
		}

		break;
	default:
		rc = CAM_VFE_IRQ_STATUS_ERR;
		break;
	}

	if (rc == CAM_VFE_IRQ_STATUS_SUCCESS)
		cam_vfe_bus_put_evt_payload(evt_payload->core_info,
			&evt_payload);

	return rc;
}

static int cam_vfe_bus_init_comp_grp(uint32_t index,
	struct cam_vfe_bus_ver2_priv    *ver2_bus_priv,
	struct cam_vfe_bus_ver2_hw_info *ver2_hw_info,
	struct cam_isp_resource_node    *comp_grp)
{
	struct cam_vfe_bus_ver2_comp_grp_data *rsrc_data =
		comp_grp->res_priv;

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_ver2_comp_grp_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		CDBG("Failed to alloc for comp_grp_priv\n");
		return -ENOMEM;
	}
	comp_grp->res_priv = rsrc_data;

	comp_grp->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&comp_grp->list);

	rsrc_data->comp_grp_type   = index;
	rsrc_data->common_data     = &ver2_bus_priv->common_data;
	rsrc_data->hw_regs         = &ver2_hw_info->comp_grp_reg[index];
	rsrc_data->dual_slave_core = CAM_VFE_BUS_VER2_VFE_CORE_MAX;


	if (rsrc_data->comp_grp_type >= CAM_VFE_BUS_VER2_COMP_GRP_DUAL_0 &&
		rsrc_data->comp_grp_type <= CAM_VFE_BUS_VER2_COMP_GRP_DUAL_5)
		list_add_tail(&comp_grp->list,
			&ver2_bus_priv->free_dual_comp_grp);
	else if (rsrc_data->comp_grp_type >= CAM_VFE_BUS_VER2_COMP_GRP_0
		&& rsrc_data->comp_grp_type <= CAM_VFE_BUS_VER2_COMP_GRP_5)
		list_add_tail(&comp_grp->list, &ver2_bus_priv->free_comp_grp);

	comp_grp->start = cam_vfe_bus_start_comp_grp;
	comp_grp->stop = cam_vfe_bus_stop_comp_grp;
	comp_grp->top_half_handler = cam_vfe_bus_handle_comp_done_top_half;
	comp_grp->bottom_half_handler =
		cam_vfe_bus_handle_comp_done_bottom_half;
	comp_grp->hw_intf = ver2_bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_vfe_bus_acquire_vfe_out(void *bus_priv, void *acquire_args)
{
	int                                     rc = -ENODEV;
	int                                     i;
	enum cam_vfe_bus_ver2_vfe_out_type      vfe_out_res_id;
	uint32_t                                format;
	uint32_t                                num_wm;
	uint32_t                                subscribe_irq;
	uint32_t                                client_done_mask;
	struct cam_vfe_bus_ver2_priv           *ver2_bus_priv = bus_priv;
	struct cam_vfe_acquire_args            *acq_args = acquire_args;
	struct cam_vfe_hw_vfe_out_acquire_args *out_acquire_args;
	struct cam_isp_resource_node           *rsrc_node = NULL;
	struct cam_vfe_bus_ver2_vfe_out_data   *rsrc_data = NULL;

	if (!bus_priv || !acquire_args) {
		pr_err("Invalid Param");
		return -EINVAL;
	}

	out_acquire_args = &acq_args->vfe_out;
	format = out_acquire_args->out_port_info->format;

	CDBG("Acquiring resource type 0x%x\n",
		out_acquire_args->out_port_info->res_type);

	vfe_out_res_id = cam_vfe_bus_get_out_res_id(
		out_acquire_args->out_port_info->res_type);
	if (vfe_out_res_id == CAM_VFE_BUS_VER2_VFE_OUT_MAX)
		return -ENODEV;

	num_wm = cam_vfe_bus_get_num_wm(vfe_out_res_id, format);
	if (num_wm < 1)
		return -EINVAL;

	rsrc_node = &ver2_bus_priv->vfe_out[vfe_out_res_id];
	if (rsrc_node->res_state != CAM_ISP_RESOURCE_STATE_AVAILABLE) {
		pr_err("Resource not available: Res_id %d state:%d\n",
			vfe_out_res_id, rsrc_node->res_state);
		return -EBUSY;
	}

	rsrc_data = rsrc_node->res_priv;
	rsrc_data->num_wm = num_wm;
	rsrc_node->res_id = out_acquire_args->out_port_info->res_type;
	rsrc_node->tasklet_info = acq_args->tasklet;
	rsrc_node->cdm_ops = out_acquire_args->cdm_ops;
	rsrc_data->cdm_util_ops = out_acquire_args->cdm_ops;

	/* Reserve Composite Group */
	if (num_wm > 1 || (out_acquire_args->out_port_info->comp_grp_id >
		CAM_ISP_RES_COMP_GROUP_NONE &&
		out_acquire_args->out_port_info->comp_grp_id <
		CAM_ISP_RES_COMP_GROUP_ID_MAX)) {
		rc = cam_vfe_bus_acquire_comp_grp(ver2_bus_priv,
			out_acquire_args->out_port_info,
			out_acquire_args->unique_id,
			out_acquire_args->is_dual,
			out_acquire_args->is_master,
			out_acquire_args->dual_slave_core,
			&rsrc_data->comp_grp);
		if (rc < 0)
			return rc;

		subscribe_irq = 0;
	} else
		subscribe_irq = 1;

	/* Reserve WM */
	for (i = 0; i < num_wm; i++) {
		rc = cam_vfe_bus_acquire_wm(ver2_bus_priv,
			out_acquire_args->out_port_info,
			vfe_out_res_id,
			i,
			out_acquire_args->split_id,
			subscribe_irq,
			&rsrc_data->wm_res[i],
			&client_done_mask);
		if (rc < 0)
			goto release_wm;

		if (rsrc_data->comp_grp)
			cam_vfe_bus_add_wm_to_comp_grp(rsrc_data->comp_grp,
				client_done_mask);
	}

	rsrc_node->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	out_acquire_args->rsrc_node = rsrc_node;

	CDBG("Acquire successful\n");
	return rc;

release_wm:
	for (i--; i >= 0; i--)
		cam_vfe_bus_release_wm(ver2_bus_priv, rsrc_data->wm_res[i]);

	cam_vfe_bus_release_comp_grp(ver2_bus_priv,
		rsrc_data->comp_grp);

	return rc;
}

static int cam_vfe_bus_release_vfe_out(void *bus_priv,
	struct cam_isp_resource_node        *vfe_out)
{
	uint32_t i;
	struct cam_vfe_bus_ver2_vfe_out_data  *rsrc_data = vfe_out->res_priv;

	if (vfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		pr_err("Error! Invalid resource state:%d\n",
			vfe_out->res_state);
	}

	for (i = 0; i < rsrc_data->num_wm; i++)
		cam_vfe_bus_release_wm(bus_priv, rsrc_data->wm_res[i]);
	rsrc_data->num_wm = 0;

	if (rsrc_data->comp_grp)
		cam_vfe_bus_release_comp_grp(bus_priv, rsrc_data->comp_grp);
	rsrc_data->comp_grp = NULL;

	vfe_out->tasklet_info = NULL;
	vfe_out->cdm_ops = NULL;
	rsrc_data->cdm_util_ops = NULL;

	if (vfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED)
		vfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;

	return 0;
}

static int cam_vfe_bus_start_vfe_out(struct cam_isp_resource_node *vfe_out)
{
	int rc = 0, i;
	struct cam_vfe_bus_ver2_vfe_out_data  *rsrc_data = vfe_out->res_priv;
	struct cam_vfe_bus_ver2_common_data   *common_data =
		rsrc_data->common_data;

	CDBG("Start resource index %d\n", rsrc_data->out_type);

	if (vfe_out->res_state != CAM_ISP_RESOURCE_STATE_RESERVED) {
		pr_err("Error! Invalid resource state:%d\n",
			vfe_out->res_state);
		return -EACCES;
	}

	/* Enable IRQ Mask */
	cam_io_w_mb(0x00001FE0, common_data->mem_base + 0x2044);
	cam_io_w_mb(0x000FFFE7, common_data->mem_base + 0x2048);
	cam_io_w_mb(0x000000FF, common_data->mem_base + 0x204c);

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_vfe_bus_start_wm(rsrc_data->wm_res[i]);

	if (rsrc_data->comp_grp)
		rc = cam_vfe_bus_start_comp_grp(rsrc_data->comp_grp);

	/* VFE_MODULE_BUS_CGC_OVERRIDE */
	cam_io_w_mb(0xFFFFFFFF, rsrc_data->common_data->mem_base + 0x0000003C);
	/* VFE_MODULE_COLOR_CGC_OVERRIDE */
	cam_io_w_mb(0xFFFFFFFF, rsrc_data->common_data->mem_base + 0x00000034);
	/* VFE_MODULE_ZOOM_CGC_OVERRIDE */
	cam_io_w_mb(0xFFFFFFFF, rsrc_data->common_data->mem_base + 0x38);
	/* VFE_MODULE_LENS_CGC_OVERRIDE */
	cam_io_w_mb(0xFFFFFFFF, rsrc_data->common_data->mem_base + 0x0000002C);
	/* VFE_MODULE_STATS_CGC_OVERRIDE */
	cam_io_w_mb(0xFFFFFFFF, rsrc_data->common_data->mem_base + 0x00000030);

	/* BUS_WR_INPUT_IF_ADDR_SYNC_CFG */
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x0000207C);
	/*  BUS_WR_INPUT_IF_ADDR_SYNC_FRAME_HEADER */
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x00002080);
	/* BUS_WR_INPUT_IF_ADDR_SYNC_NO_SYNC */
	cam_io_w_mb(0xFFFFF, rsrc_data->common_data->mem_base + 0x00002084);
	/*  BUS_WR_INPUT_IF_ADDR_SYNC_0 */
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x00002088);
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x0000208c);
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x00002090);
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x00002094);
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x00002098);
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x0000209c);
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x000020a0);
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x000020a4);

	/* no clock gating at bus input */
	cam_io_w_mb(0xFFFFF, rsrc_data->common_data->mem_base + 0x0000200C);

	/* BUS_WR_TEST_BUS_CTRL */
	cam_io_w_mb(0x0, rsrc_data->common_data->mem_base + 0x0000211C);

	return rc;
}

static int cam_vfe_bus_stop_vfe_out(struct cam_isp_resource_node *vfe_out)
{
	int rc = 0, i;
	struct cam_vfe_bus_ver2_vfe_out_data *rsrc_data = vfe_out->res_priv;

	if (vfe_out->res_state == CAM_ISP_RESOURCE_STATE_AVAILABLE ||
		vfe_out->res_state == CAM_ISP_RESOURCE_STATE_RESERVED) {
		return rc;
	}

	if (rsrc_data->comp_grp)
		rc = cam_vfe_bus_stop_comp_grp(rsrc_data->comp_grp);

	for (i = 0; i < rsrc_data->num_wm; i++)
		rc = cam_vfe_bus_stop_wm(rsrc_data->wm_res[i]);

	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_STREAMING;

	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_RESERVED;
	return rc;
}

static int cam_vfe_bus_handle_vfe_out_done_top_half(uint32_t evt_id,
	struct cam_irq_th_payload *th_payload)
{
	return -EPERM;
}

static int cam_vfe_bus_handle_vfe_out_done_bottom_half(
	void                *handler_priv,
	void                *evt_payload_priv)
{
	int rc = -EINVAL;
	struct cam_isp_resource_node         *vfe_out = handler_priv;
	struct cam_vfe_bus_ver2_vfe_out_data *rsrc_data = vfe_out->res_priv;

	/*
	 * If this resource has Composite Group then we only handle
	 * Composite done. We acquire Composite if number of WM > 1.
	 * So Else case is only one individual buf_done = WM[0].
	 */
	if (rsrc_data->comp_grp) {
		rc = rsrc_data->comp_grp->bottom_half_handler(
			rsrc_data->comp_grp, evt_payload_priv);
	} else {
		rc = rsrc_data->wm_res[0]->bottom_half_handler(
			rsrc_data->wm_res[0], evt_payload_priv);
	}

	return rc;
}

static int cam_vfe_bus_init_vfe_out_resource(uint32_t index,
	struct cam_vfe_bus_ver2_priv    *ver2_bus_priv,
	struct cam_vfe_bus_ver2_hw_info *ver2_hw_info,
	struct cam_isp_resource_node    *vfe_out)
{
	struct cam_vfe_bus_ver2_vfe_out_data *rsrc_data = NULL;
	int rc = 0;

	rsrc_data = kzalloc(sizeof(struct cam_vfe_bus_ver2_vfe_out_data),
		GFP_KERNEL);
	if (!rsrc_data) {
		CDBG("Error! Failed to alloc for vfe out priv\n");
		rc = -ENOMEM;
		return rc;
	}
	vfe_out->res_priv = rsrc_data;

	vfe_out->res_type = CAM_ISP_RESOURCE_VFE_OUT;
	vfe_out->res_state = CAM_ISP_RESOURCE_STATE_AVAILABLE;
	INIT_LIST_HEAD(&vfe_out->list);

	rsrc_data->out_type    = index;
	rsrc_data->common_data = &ver2_bus_priv->common_data;
	rsrc_data->max_width   =
		ver2_hw_info->vfe_out_hw_info[index].max_width;
	rsrc_data->max_height  =
		ver2_hw_info->vfe_out_hw_info[index].max_height;

	vfe_out->start = cam_vfe_bus_start_vfe_out;
	vfe_out->stop = cam_vfe_bus_stop_vfe_out;
	vfe_out->top_half_handler = cam_vfe_bus_handle_vfe_out_done_top_half;
	vfe_out->bottom_half_handler =
		cam_vfe_bus_handle_vfe_out_done_bottom_half;
	vfe_out->hw_intf = ver2_bus_priv->common_data.hw_intf;

	return 0;
}

static int cam_vfe_bus_get_evt_payload(
	struct cam_vfe_bus_ver2_priv         *bus_priv,
	struct cam_vfe_bus_irq_evt_payload  **evt_payload)
{
	if (list_empty(&bus_priv->free_payload_list)) {
		*evt_payload = NULL;
		pr_err("No free payload\n");
		return -ENODEV;
	}

	*evt_payload = list_first_entry(&bus_priv->free_payload_list,
		struct cam_vfe_bus_irq_evt_payload, list);
	list_del_init(&(*evt_payload)->list);
	return 0;
}

static int cam_vfe_bus_put_evt_payload(void     *core_info,
	struct cam_vfe_bus_irq_evt_payload     **evt_payload)
{
	struct cam_vfe_bus_ver2_priv         *bus_priv = NULL;
	uint32_t  *cam_ife_irq_regs = (*evt_payload)->irq_reg_val;
	uint32_t   status_reg0, status_reg1;

	status_reg0 = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_STATUS0];
	status_reg1 = cam_ife_irq_regs[CAM_IFE_IRQ_BUS_REG_STATUS1];

	if (status_reg0 || status_reg1) {
		CDBG("status0 0x%x status1 0x%x\n", status_reg0, status_reg1);
		return 0;
	}

	if (!core_info) {
		pr_err("Invalid param core_info NULL");
		return -EINVAL;
	}
	if (*evt_payload == NULL) {
		pr_err("No payload to put\n");
		return -EINVAL;
	}
	bus_priv = (*evt_payload)->bus_priv;
	list_add_tail(&(*evt_payload)->list, &bus_priv->free_payload_list);
	*evt_payload = NULL;
	return 0;
}

static int cam_vfe_bus_ver2_handle_irq(uint32_t    evt_id,
	struct cam_irq_th_payload                 *th_payload)
{
	int32_t                                rc;
	int                                    i;
	struct cam_vfe_irq_handler_priv       *handler_priv;
	struct cam_vfe_hw_core_info           *core_info;
	struct cam_vfe_bus_irq_evt_payload    *evt_payload;
	struct cam_vfe_bus                    *bus_info;
	struct cam_vfe_bus_ver2_priv          *bus_priv;
	struct cam_irq_controller_reg_info    *reg_info;
	uint32_t                               irq_mask;

	handler_priv = th_payload->handler_priv;
	core_info    = handler_priv->core_info;
	bus_info     = core_info->vfe_bus;
	bus_priv     = bus_info->bus_priv;
	reg_info     = &bus_priv->common_data.common_reg->irq_reg_info;

	/*
	 *  add reset ack handling here once supported.
	 *  Just clear all the bus irq status registers and ignore the reset.
	 */

	CDBG("Enter\n");
	rc  = cam_vfe_bus_get_evt_payload(bus_priv, &evt_payload);
	if (rc) {
		pr_err("No tasklet_cmd is free in queue\n");
		return rc;
	}

	cam_isp_hw_get_timestamp(&evt_payload->ts);

	evt_payload->core_index = handler_priv->core_index;
	evt_payload->core_info  = handler_priv->core_info;
	evt_payload->bus_priv   = bus_priv;
	CDBG("core_idx %d, core_info %llx\n", handler_priv->core_index,
			(uint64_t)handler_priv->core_info);

	for (i = 0; i < CAM_IFE_BUS_IRQ_REGISTERS_MAX; i++) {
		irq_mask = cam_io_r(handler_priv->mem_base +
			irq_reg_offset[i] - (0xC * 2));
		evt_payload->irq_reg_val[i] = irq_mask &
			cam_io_r(handler_priv->mem_base + irq_reg_offset[i]);
		CDBG("irq_status%d = 0x%x\n", i, evt_payload->irq_reg_val[i]);
	}
	for (i = 0; i <= CAM_IFE_IRQ_BUS_REG_STATUS2; i++) {
		cam_io_w(evt_payload->irq_reg_val[i], handler_priv->mem_base +
			reg_info->irq_reg_set[i].clear_reg_offset);
		CDBG("Clear irq_status%d = 0x%x offset 0x%x\n", i,
			evt_payload->irq_reg_val[i],
			reg_info->irq_reg_set[i].clear_reg_offset);
	}
	cam_io_w(reg_info->global_clear_bitmask, handler_priv->mem_base +
		reg_info->global_clear_offset);
	CDBG("Global clear bitmask = 0x%x offset 0x%x\n",
			reg_info->global_clear_bitmask,
			reg_info->global_clear_offset);

	th_payload->evt_payload_priv = evt_payload;

	return rc;
}

static int cam_vfe_bus_update_buf(void *priv, void *cmd_args,
	uint32_t arg_size)
{
	struct cam_vfe_bus_ver2_priv             *bus_priv;
	struct cam_isp_hw_get_buf_update         *update_buf;
	struct cam_buf_io_cfg                    *io_cfg;
	struct cam_vfe_bus_ver2_vfe_out_data     *vfe_out_data = NULL;
	struct cam_vfe_bus_ver2_wm_resource_data *wm_data = NULL;
	uint32_t *reg_val_pair;
	uint32_t  i, j, size = 0;

	/*
	 * Need the entire buf io config so we can get the stride info
	 * for the wm.
	 */

	bus_priv = (struct cam_vfe_bus_ver2_priv  *) priv;
	update_buf =  (struct cam_isp_hw_get_buf_update *) cmd_args;

	vfe_out_data = (struct cam_vfe_bus_ver2_vfe_out_data *)
		update_buf->cdm.res->res_priv;

	if (!vfe_out_data || !vfe_out_data->cdm_util_ops) {
		pr_err("Failed! Invalid data\n");
		return -EINVAL;
	}

	if (update_buf->num_buf != vfe_out_data->num_wm) {
		pr_err("Failed! Invalid number buffers:%d required:%d\n",
			update_buf->num_buf, vfe_out_data->num_wm);
		return -ENOMEM;
	}

	reg_val_pair = &vfe_out_data->common_data->io_buf_update[0];
	io_cfg = update_buf->io_cfg;

	for (i = 0, j = 0; i < vfe_out_data->num_wm; i++) {
		wm_data = vfe_out_data->wm_res[i]->res_priv;

		/* For initial configuration program all bus registers */
		if (wm_data->stride != io_cfg->planes[i].plane_stride ||
			!wm_data->init_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->stride,
				io_cfg->planes[i].plane_stride);
			wm_data->stride = io_cfg->planes[i].plane_stride;
		}
		CDBG("image stride 0x%x\n", wm_data->stride);

		if (wm_data->framedrop_pattern != io_cfg->framedrop_pattern ||
			!wm_data->init_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->framedrop_pattern,
				io_cfg->framedrop_pattern);
			wm_data->framedrop_pattern = io_cfg->framedrop_pattern;
		}
		CDBG("framedrop pattern 0x%x\n", wm_data->framedrop_pattern);

		if (wm_data->framedrop_period != io_cfg->framedrop_period ||
			!wm_data->init_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->framedrop_period,
				io_cfg->framedrop_period);
			wm_data->framedrop_period = io_cfg->framedrop_period;
		}
		CDBG("framedrop period 0x%x\n", wm_data->framedrop_period);

		if (wm_data->irq_subsample_period != io_cfg->subsample_period
			|| !wm_data->init_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->irq_subsample_period,
				io_cfg->subsample_period);
			wm_data->irq_subsample_period =
				io_cfg->subsample_period;
		}
		CDBG("irq subsample period 0x%x\n",
			wm_data->irq_subsample_period);

		if (wm_data->irq_subsample_pattern != io_cfg->subsample_pattern
			|| !wm_data->init_cfg_done) {
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->irq_subsample_pattern,
				io_cfg->subsample_pattern);
			wm_data->irq_subsample_pattern =
				io_cfg->subsample_pattern;
		}
		CDBG("irq subsample pattern 0x%x\n",
			wm_data->irq_subsample_pattern);

		if (wm_data->en_ubwc) {
			if (!wm_data->hw_regs->ubwc_regs) {
				pr_err("%s: No UBWC register to configure.\n",
					__func__);
				return -EINVAL;
			}
			if (wm_data->packer_cfg !=
				io_cfg->planes[i].packer_config ||
				!wm_data->init_cfg_done) {
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->packer_cfg,
					io_cfg->planes[i].packer_config);
				wm_data->packer_cfg =
					io_cfg->planes[i].packer_config;
			}
			CDBG("packer cfg 0x%x\n", wm_data->packer_cfg);

			if (wm_data->tile_cfg != io_cfg->planes[i].tile_config
				|| !wm_data->init_cfg_done) {
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->ubwc_regs->tile_cfg,
					io_cfg->planes[i].tile_config);
				wm_data->tile_cfg =
					io_cfg->planes[i].tile_config;
			}
			CDBG("tile cfg 0x%x\n", wm_data->tile_cfg);

			if (wm_data->h_init != io_cfg->planes[i].h_init ||
				!wm_data->init_cfg_done) {
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->ubwc_regs->h_init,
					io_cfg->planes[i].h_init);
				wm_data->h_init = io_cfg->planes[i].h_init;
			}
			CDBG("h_init 0x%x\n", wm_data->h_init);

			if (wm_data->v_init != io_cfg->planes[i].v_init ||
				!wm_data->init_cfg_done) {
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->ubwc_regs->v_init,
					io_cfg->planes[i].v_init);
				wm_data->v_init = io_cfg->planes[i].v_init;
			}
			CDBG("v_init 0x%x\n", wm_data->v_init);

			if (wm_data->ubwc_meta_stride !=
				io_cfg->planes[i].meta_stride ||
				!wm_data->init_cfg_done) {
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->ubwc_regs->
					meta_stride,
					io_cfg->planes[i].meta_stride);
				wm_data->ubwc_meta_stride =
					io_cfg->planes[i].meta_stride;
			}
			CDBG("meta stride 0x%x\n", wm_data->ubwc_meta_stride);

			if (wm_data->ubwc_mode_cfg !=
				io_cfg->planes[i].mode_config ||
				!wm_data->init_cfg_done) {
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->ubwc_regs->mode_cfg,
					io_cfg->planes[i].mode_config);
				wm_data->ubwc_mode_cfg =
					io_cfg->planes[i].mode_config;
			}
			CDBG("ubwc mode cfg 0x%x\n", wm_data->ubwc_mode_cfg);

			if (wm_data->ubwc_meta_offset !=
				io_cfg->planes[i].meta_offset ||
				!wm_data->init_cfg_done) {
				CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
					wm_data->hw_regs->ubwc_regs->
					meta_offset,
					io_cfg->planes[i].meta_offset);
				wm_data->ubwc_meta_offset =
					io_cfg->planes[i].meta_offset;
			}
			CDBG("ubwc meta offset 0x%x\n",
				wm_data->ubwc_meta_offset);

			/* UBWC meta address */
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->ubwc_regs->meta_addr,
				update_buf->image_buf[i]);
			CDBG("ubwc meta addr 0x%llx\n",
				update_buf->image_buf[i]);
		}

		/* WM Image address */
		if (wm_data->en_ubwc)
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_addr,
				(update_buf->image_buf[i] +
				io_cfg->planes[i].meta_size));
		else
			CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
				wm_data->hw_regs->image_addr,
				update_buf->image_buf[i]);

		CDBG("image address 0x%x\n", reg_val_pair[j-1]);

		/* enable the WM */
		CAM_VFE_ADD_REG_VAL_PAIR(reg_val_pair, j,
			wm_data->hw_regs->cfg,
			wm_data->en_cfg);

		/* set initial configuration done */
		if (!wm_data->init_cfg_done)
			wm_data->init_cfg_done = 1;
	}

	size = vfe_out_data->cdm_util_ops->cdm_required_size_reg_random(j/2);

	/* cdm util returns dwords, need to convert to bytes */
	if ((size * 4) > update_buf->cdm.size) {
		pr_err("Failed! Buf size:%d insufficient, expected size:%d\n",
			update_buf->cdm.size, size);
		return -ENOMEM;
	}

	vfe_out_data->cdm_util_ops->cdm_write_regrandom(
		update_buf->cdm.cmd_buf_addr, j/2, reg_val_pair);

	/* cdm util returns dwords, need to convert to bytes */
	update_buf->cdm.used_bytes = size * 4;

	return 0;
}

static int cam_vfe_bus_process_cmd(void *priv,
	uint32_t cmd_type, void *cmd_args, uint32_t arg_size)
{
	int rc = -EINVAL;

	if (!priv || !cmd_args) {
		pr_err_ratelimited("Error! Invalid input arguments\n");
		return -EINVAL;
	}

	switch (cmd_type) {
	case CAM_VFE_HW_CMD_GET_BUF_UPDATE:
		rc = cam_vfe_bus_update_buf(priv, cmd_args, arg_size);
		break;
	default:
		pr_err_ratelimited("Error! Invalid camif process command:%d\n",
			cmd_type);
		break;
	}

	return rc;
}

int cam_vfe_bus_ver2_init(
	void __iomem                         *mem_base,
	struct cam_hw_intf                   *hw_intf,
	void                                 *bus_hw_info,
	void                                 *vfe_irq_controller,
	struct cam_vfe_bus                  **vfe_bus)
{
	int i, rc = 0;
	struct cam_vfe_bus_ver2_priv    *bus_priv = NULL;
	struct cam_vfe_bus              *vfe_bus_local;
	struct cam_vfe_bus_ver2_hw_info *ver2_hw_info = bus_hw_info;

	CDBG("Enter\n");

	vfe_bus_local = kzalloc(sizeof(struct cam_vfe_bus), GFP_KERNEL);
	if (!vfe_bus_local) {
		CDBG("Failed to alloc for vfe_bus\n");
		rc = -ENOMEM;
		goto err_alloc_bus;
	}

	bus_priv = kzalloc(sizeof(struct cam_vfe_bus_ver2_priv),
		GFP_KERNEL);
	if (!bus_priv) {
		CDBG("Failed to alloc for vfe_bus_priv\n");
		rc = -ENOMEM;
		goto err_alloc_priv;
	}
	vfe_bus_local->bus_priv = bus_priv;

	bus_priv->common_data.mem_base           = mem_base;
	bus_priv->common_data.hw_intf            = hw_intf;
	bus_priv->common_data.vfe_irq_controller = vfe_irq_controller;
	bus_priv->common_data.common_reg         = &ver2_hw_info->common_reg;

	INIT_LIST_HEAD(&bus_priv->free_comp_grp);
	INIT_LIST_HEAD(&bus_priv->free_dual_comp_grp);
	INIT_LIST_HEAD(&bus_priv->used_comp_grp);

	for (i = 0; i < CAM_VFE_BUS_VER2_MAX_CLIENTS; i++) {
		rc = cam_vfe_bus_init_wm_resource(i, bus_priv, bus_hw_info,
			&bus_priv->bus_client[i]);
		if (rc < 0) {
			pr_err("Error! Init WM failed\n");
			goto err_init_wm;
		}
	}

	for (i = 0; i < CAM_VFE_BUS_VER2_COMP_GRP_MAX; i++) {
		rc = cam_vfe_bus_init_comp_grp(i, bus_priv, bus_hw_info,
			&bus_priv->comp_grp[i]);
		if (rc < 0) {
			pr_err("Error! Init Comp Grp failed\n");
			goto err_init_comp_grp;
		}
	}

	for (i = 0; i < CAM_VFE_BUS_VER2_VFE_OUT_MAX; i++) {
		rc = cam_vfe_bus_init_vfe_out_resource(i, bus_priv, bus_hw_info,
			&bus_priv->vfe_out[i]);
		if (rc < 0) {
			pr_err("Error! Init VFE Out failed\n");
			goto err_init_vfe_out;
		}
	}

	INIT_LIST_HEAD(&bus_priv->free_payload_list);
	for (i = 0; i < 128; i++) {
		INIT_LIST_HEAD(&bus_priv->evt_payload[i].list);
		list_add_tail(&bus_priv->evt_payload[i].list,
			&bus_priv->free_payload_list);
	}

	vfe_bus_local->acquire_resource = cam_vfe_bus_acquire_vfe_out;
	vfe_bus_local->release_resource = cam_vfe_bus_release_vfe_out;
	vfe_bus_local->start_resource   = cam_vfe_bus_start_vfe_out;
	vfe_bus_local->stop_resource    = cam_vfe_bus_stop_vfe_out;
	vfe_bus_local->top_half_handler = cam_vfe_bus_ver2_handle_irq;
	vfe_bus_local->bottom_half_handler = NULL;
	vfe_bus_local->process_cmd      = cam_vfe_bus_process_cmd;

	*vfe_bus = vfe_bus_local;

	return rc;

err_init_vfe_out:
err_init_comp_grp:
err_init_wm:
	kfree(vfe_bus_local->bus_priv);
err_alloc_priv:
	kfree(vfe_bus_local);
err_alloc_bus:
	return rc;
}
