/*
 * Support for Clovertrail PNW Camera Imaging ISP subsystem.
 *
 * Copyright (c) 2013 Intel Corporation. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#ifndef __ATOMISP_COMPAT_CSS15_H__
#define __ATOMISP_COMPAT_CSS15_H__

#include <media/v4l2-mediabus.h>

#include "sh_css.h"
#include "sh_css_sp.h"

#define atomisp_css_pipe_id sh_css_pipe_id
#define atomisp_css_pipeline	sh_css_pipeline
#define atomisp_css_buffer_type sh_css_buffer_type
#define atomisp_css_dis_data sh_css_dis_data
#define atomisp_css_irq_info sh_css_interrupt_info
#define atomisp_css_bayer_order sh_css_bayer_order
#define atomisp_css_stream_format sh_css_input_format
#define atomisp_css_capture_mode sh_css_capture_mode
#define atomisp_css_input_mode sh_css_input_mode
#define atomisp_css_frame sh_css_frame
#define atomisp_css_frame_format sh_css_frame_format
#define atomisp_css_frame_info sh_css_frame_info
#define atomisp_css_dp_config	sh_css_dp_config
#define atomisp_css_wb_config	sh_css_wb_config
#define atomisp_css_cc_config	sh_css_cc_config
#define atomisp_css_nr_config	sh_css_nr_config
#define atomisp_css_ee_config	sh_css_ee_config
#define atomisp_css_ob_config	sh_css_ob_config
#define atomisp_css_de_config	sh_css_de_config
#define atomisp_css_ce_config	sh_css_ce_config
#define atomisp_css_gc_config	sh_css_gc_config
#define atomisp_css_tnr_config	sh_css_tnr_config
#define atomisp_css_3a_config	sh_css_3a_config
#define atomisp_css_gamma_table	sh_css_gamma_table
#define atomisp_css_ctc_table	sh_css_ctc_table
#define atomisp_css_macc_table	sh_css_macc_table
#define atomisp_css_grid_info	sh_css_grid_info
#define atomisp_css_shading_table	sh_css_shading_table
#define atomisp_css_morph_table	sh_css_morph_table
#define atomisp_css_fw_info	sh_css_fw_info

#define CSS_PIPE_ID_PREVIEW	SH_CSS_PREVIEW_PIPELINE
#define CSS_PIPE_ID_COPY	SH_CSS_COPY_PIPELINE
#define CSS_PIPE_ID_VIDEO	SH_CSS_VIDEO_PIPELINE
#define CSS_PIPE_ID_CAPTURE	SH_CSS_CAPTURE_PIPELINE
#define CSS_PIPE_ID_ACC		SH_CSS_ACC_PIPELINE
#define CSS_PIPE_ID_NUM		SH_CSS_NR_OF_PIPELINES

#define CSS_INPUT_MODE_SENSOR	SH_CSS_INPUT_MODE_SENSOR
#define CSS_INPUT_MODE_FIFO	SH_CSS_INPUT_MODE_FIFO
#define CSS_INPUT_MODE_TPG	SH_CSS_INPUT_MODE_TPG
#define CSS_INPUT_MODE_PRBS	SH_CSS_INPUT_MODE_PRBS
#define CSS_INPUT_MODE_MEMORY	SH_CSS_INPUT_MODE_MEMORY

#define CSS_IRQ_INFO_EVENTS_READY	SH_CSS_IRQ_INFO_BUFFER_DONE
#define CSS_IRQ_INFO_INVALID_FIRST_FRAME \
	SH_CSS_IRQ_INFO_INVALID_FIRST_FRAME

#if defined(HAS_IRQ_MAP_VERSION_2)

#define CSS_IRQ_INFO_INPUT_SYSTEM_ERROR	SH_CSS_IRQ_INFO_INPUT_SYSTEM_ERROR
#define CSS_IRQ_INFO_IF_ERROR	SH_CSS_IRQ_INFO_IF_ERROR

#elif defined(HAS_IRQ_MAP_VERSION_1) || defined(HAS_IRQ_MAP_VERSION_1_DEMO)

#define CSS_IRQ_INFO_CSS_RECEIVER_ERROR	SH_CSS_IRQ_INFO_CSS_RECEIVER_ERROR

#endif

#define CSS_BUFFER_TYPE_NUM	SH_CSS_BUFFER_TYPE_NR_OF_TYPES

#define CSS_FRAME_FLASH_STATE_NONE	SH_CSS_FRAME_NO_FLASH
#define CSS_FRAME_FLASH_STATE_PARTIAL	SH_CSS_FRAME_PARTIAL_FLASH
#define CSS_FRAME_FLASH_STATE_FULL	SH_CSS_FRAME_FULLY_FLASH

#define CSS_BAYER_ORDER_GRBG	sh_css_bayer_order_grbg
#define CSS_BAYER_ORDER_RGGB	sh_css_bayer_order_rggb
#define CSS_BAYER_ORDER_BGGR	sh_css_bayer_order_bggr
#define CSS_BAYER_ORDER_GBRG	sh_css_bayer_order_gbrg

/*
 * Hide SH_ naming difference in otherwise common CSS macros.
 */
#define CSS_ID(val)	(SH_ ## val)
#define CSS_EVENT(val)	(SH_CSS_EVENT_ ## val)
#define CSS_FORMAT(val)	(SH_CSS_INPUT_FORMAT_ ## val)

struct atomisp_css_env {
	struct sh_css_env isp_css_env;
};

struct atomisp_s3a_buf {
	union sh_css_s3a_data s3a_data;
	struct list_head list;
};

struct atomisp_dis_buf {
	struct atomisp_css_dis_data dis_data;
	struct list_head list;
};

struct atomisp_css_buffer {
	struct sh_css_buffer css_buffer;
	struct atomisp_css_dis_data *dis_data;
	union sh_css_s3a_data *s3a_data;
};

struct atomisp_css_event_type {
	enum sh_css_event_type type;
};

struct atomisp_css_event {
	enum atomisp_css_pipe_id pipe;
	struct atomisp_css_event_type event;
};

void atomisp_css_mmu_invalidate_tlb(void);

#endif
