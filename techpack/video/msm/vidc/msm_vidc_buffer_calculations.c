// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019-2020, The Linux Foundation. All rights reserved.
 */

#include "msm_vidc_debug.h"
#include "msm_vidc_common.h"
#include "msm_vidc_buffer_calculations.h"
#include "msm_vidc_clocks.h"

#define VP9_REFERENCE_COUNT 8

/* minimum number of input buffers */
#define MIN_INPUT_BUFFERS 4

/* Decoder buffer count macros */
/* total input buffers in case of decoder batch */
#define BATCH_DEC_TOTAL_INPUT_BUFFERS 6

/* extra output buffers in case of decoder batch */
#define BATCH_DEC_EXTRA_OUTPUT_BUFFERS 6

/* Encoder buffer count macros */
/* minimum number of output buffers */
#define MIN_ENC_OUTPUT_BUFFERS 4

/* extra output buffers for encoder HEIF usecase */
#define HEIF_ENC_TOTAL_OUTPUT_BUFFERS 12

/* extra buffer count for heif decoder */
#define HEIF_DEC_TOTAL_INPUT_BUFFERS 12
#define HEIF_DEC_EXTRA_OUTPUT_BUFFERS 8

#define HFI_COLOR_FORMAT_YUV420_NV12_UBWC_Y_TILE_WIDTH 32
#define HFI_COLOR_FORMAT_YUV420_NV12_UBWC_Y_TILE_HEIGHT 8
#define HFI_COLOR_FORMAT_YUV420_NV12_UBWC_UV_TILE_WIDTH 16
#define HFI_COLOR_FORMAT_YUV420_NV12_UBWC_UV_TILE_HEIGHT 8
#define HFI_COLOR_FORMAT_YUV420_TP10_UBWC_Y_TILE_WIDTH 48
#define HFI_COLOR_FORMAT_YUV420_TP10_UBWC_Y_TILE_HEIGHT 4
#define BUFFER_ALIGNMENT_4096_BYTES 4096
#define VENUS_METADATA_STRIDE_MULTIPLE     64
#define VENUS_METADATA_HEIGHT_MULTIPLE     16
#define HFI_UBWC_CALC_METADATA_PLANE_STRIDE \
	((metadataStride, width, metadataStrideMultiple, tileWidthInPels) \
	metadataStride = ALIGN(((width + (tileWidthInPels - 1)) / \
		tileWidthInPels), metadataStrideMultiple))
#define HFI_UBWC_METADATA_PLANE_BUFHEIGHT \
	((metadataBufHeight, height, metadataHeightMultiple, tileHeightInPels) \
	metadataBufHeight = ALIGN(((height + (tileHeightInPels - 1)) / \
		tileHeightInPels), metadataHeightMultiple))
#define HFI_UBWC_METADATA_PLANE_BUFFER_SIZE \
	((buffersize, MetadataStride, MetadataBufHeight) \
	buffersize = ALIGN(MetadataStride * MetadataBufHeight, \
		BUFFER_ALIGNMENT_4096_BYTES))
#define HFI_UBWC_UV_METADATA_PLANE_STRIDE \
	((metadataStride, width, metadataStrideMultiple, tileWidthInPels) \
		metadataStride = ALIGN(((((width + 1) >> 1) + \
		(tileWidthInPels - 1)) / tileWidthInPels), \
		metadataStrideMultiple))
#define HFI_UBWC_UV_METADATA_PLANE_BUFHEIGHT \
	((metadataBufHeight, height, metadataHeightMultiple, tileHeightInPels) \
	metadataBufHeight = ALIGN(((((height + 1) >> 1) + \
		(tileHeightInPels - 1)) / tileHeightInPels), \
		metadataHeightMultiple))

#define BUFFER_ALIGNMENT_SIZE(x) x

#define VENUS_DMA_ALIGNMENT BUFFER_ALIGNMENT_SIZE(256)

#define MAX_FE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE   64
#define MAX_FE_NBR_CTRL_LCU32_LINE_BUFFER_SIZE   64
#define MAX_FE_NBR_CTRL_LCU16_LINE_BUFFER_SIZE   64
#define MAX_FE_NBR_DATA_LUMA_LINE_BUFFER_SIZE   640
#define MAX_FE_NBR_DATA_CB_LINE_BUFFER_SIZE     320
#define MAX_FE_NBR_DATA_CR_LINE_BUFFER_SIZE     320

#define MAX_SE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE     (128 / 8)
#define MAX_SE_NBR_CTRL_LCU32_LINE_BUFFER_SIZE     (128 / 8)
#define MAX_SE_NBR_CTRL_LCU16_LINE_BUFFER_SIZE     (128 / 8)

#define MAX_PE_NBR_DATA_LCU64_LINE_BUFFER_SIZE     (64 * 2 * 3)
#define MAX_PE_NBR_DATA_LCU32_LINE_BUFFER_SIZE     (32 * 2 * 3)
#define MAX_PE_NBR_DATA_LCU16_LINE_BUFFER_SIZE     (16 * 2 * 3)

#define MAX_TILE_COLUMNS 32     /* 8K/256 */

#define VPP_CMD_MAX_SIZE (1 << 20)
#define NUM_HW_PIC_BUF 32
#define BIN_BUFFER_THRESHOLD (1280 * 736)
#define H264D_MAX_SLICE 1800
#define SIZE_H264D_BUFTAB_T  256 // sizeof(h264d_buftab_t) aligned to 256
#define SIZE_H264D_HW_PIC_T (1 << 11) // sizeof(h264d_hw_pic_t) 32 aligned
#define SIZE_H264D_BSE_CMD_PER_BUF (32 * 4)
#define SIZE_H264D_VPP_CMD_PER_BUF 512

// Line Buffer definitions
/* one for luma and 1/2 for each chroma */
#define SIZE_H264D_LB_FE_TOP_DATA(width, height) \
	(MAX_FE_NBR_DATA_LUMA_LINE_BUFFER_SIZE * \
	ALIGN(width, 16) * 3)

#define SIZE_H264D_LB_FE_TOP_CTRL(width, height) \
	(MAX_FE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE * \
	((width + 15) >> 4))

#define SIZE_H264D_LB_FE_LEFT_CTRL(width, height) \
	(MAX_FE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE * \
	((height + 15) >> 4))

#define SIZE_H264D_LB_SE_TOP_CTRL(width, height) \
	(MAX_SE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE * \
	((width + 15) >> 4))

#define SIZE_H264D_LB_SE_LEFT_CTRL(width, height) \
	(MAX_SE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE * \
	((height + 15) >> 4))

#define SIZE_H264D_LB_PE_TOP_DATA(width, height) \
	(MAX_PE_NBR_DATA_LCU64_LINE_BUFFER_SIZE * \
	((width + 15) >> 4))

#define SIZE_H264D_LB_VSP_TOP(width, height) \
	((((width + 15) >> 4) << 7))

#define SIZE_H264D_LB_RECON_DMA_METADATA_WR(width, height) \
	(ALIGN(height, 16) * 32)

#define SIZE_H264D_QP(width, height) \
	(((width + 63) >> 6) * ((height + 63) >> 6) * 128)

#define SIZE_HW_PIC(sizePerBuf) \
	(NUM_HW_PIC_BUF * sizePerBuf)

#define H264_CABAC_HDR_RATIO_HD_TOT 1
#define H264_CABAC_RES_RATIO_HD_TOT 3

/*
 * some content need more bin buffer, but limit buffer
 * size for high resolution
 */


#define NUM_SLIST_BUF_H264            (256 + 32)
#define SIZE_SLIST_BUF_H264           512
#define SIZE_SEI_USERDATA             4096

#define LCU_MAX_SIZE_PELS 64
#define LCU_MIN_SIZE_PELS 16

#define H265D_MAX_SLICE 3600
#define SIZE_H265D_HW_PIC_T SIZE_H264D_HW_PIC_T
#define SIZE_H265D_BSE_CMD_PER_BUF (16 * sizeof(u32))
#define SIZE_H265D_VPP_CMD_PER_BUF 256

#define SIZE_H265D_LB_FE_TOP_DATA(width, height) \
	(MAX_FE_NBR_DATA_LUMA_LINE_BUFFER_SIZE * \
	(ALIGN(width, 64) + 8) * 2)

#define SIZE_H265D_LB_FE_TOP_CTRL(width, height) \
	(MAX_FE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE * \
	(ALIGN(width, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS))

#define SIZE_H265D_LB_FE_LEFT_CTRL(width, height) \
	(MAX_FE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE * \
	(ALIGN(height, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS))

#define SIZE_H265D_LB_SE_TOP_CTRL(width, height) \
	((LCU_MAX_SIZE_PELS / 8 * (128 / 8)) * \
	((width + 15) >> 4))

#define SIZE_H265D_LB_SE_LEFT_CTRL(width, height) \
	(max(((height + 16 - 1) / 8) * MAX_SE_NBR_CTRL_LCU16_LINE_BUFFER_SIZE,\
	max(((height + 32 - 1) / 8) * MAX_SE_NBR_CTRL_LCU32_LINE_BUFFER_SIZE, \
	((height + 64 - 1) / 8) * MAX_SE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE)))

#define SIZE_H265D_LB_PE_TOP_DATA(width, height) \
	(MAX_PE_NBR_DATA_LCU64_LINE_BUFFER_SIZE * \
	(ALIGN(width, LCU_MIN_SIZE_PELS) / LCU_MIN_SIZE_PELS))

#define SIZE_H265D_LB_VSP_TOP(width, height) \
	(((width + 63) >> 6) * 128)

#define SIZE_H265D_LB_VSP_LEFT(width, height) \
	(((height + 63) >> 6) * 128)

#define SIZE_H265D_LB_RECON_DMA_METADATA_WR(width, height) \
	SIZE_H264D_LB_RECON_DMA_METADATA_WR(width, height)

#define SIZE_H265D_QP(width, height) SIZE_H264D_QP(width, height)

#define H265_CABAC_HDR_RATIO_HD_TOT 2
#define H265_CABAC_RES_RATIO_HD_TOT 2

/*
 * some content need more bin buffer, but limit buffer size
 * for high resolution
 */

#define SIZE_SLIST_BUF_H265 (1 << 10)
#define NUM_SLIST_BUF_H265 (80 + 20)
#define H265_NUM_TILE_COL 32
#define H265_NUM_TILE_ROW 128
#define H265_NUM_TILE (H265_NUM_TILE_ROW * H265_NUM_TILE_COL + 1)

#define SIZE_VPXD_LB_FE_LEFT_CTRL(width, height) \
	max(((height + 15) >> 4) * MAX_FE_NBR_CTRL_LCU16_LINE_BUFFER_SIZE, \
	max(((height + 31) >> 5) * MAX_FE_NBR_CTRL_LCU32_LINE_BUFFER_SIZE, \
	((height + 63) >> 6) * MAX_FE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE))
#define SIZE_VPXD_LB_FE_TOP_CTRL(width, height) \
	(((ALIGN(width, 64) + 8) * 10 * 2)) /* + small line */
#define SIZE_VPXD_LB_SE_TOP_CTRL(width, height) \
	(((width + 15) >> 4) * MAX_FE_NBR_CTRL_LCU16_LINE_BUFFER_SIZE)
#define SIZE_VPXD_LB_SE_LEFT_CTRL(width, height) \
	max(((height + 15) >> 4) * MAX_SE_NBR_CTRL_LCU16_LINE_BUFFER_SIZE, \
	max(((height + 31) >> 5) * MAX_SE_NBR_CTRL_LCU32_LINE_BUFFER_SIZE, \
	((height + 63) >> 6) * MAX_SE_NBR_CTRL_LCU64_LINE_BUFFER_SIZE))
#define SIZE_VPXD_LB_RECON_DMA_METADATA_WR(width, height) \
	ALIGN((ALIGN(height, 16) / (4 / 2)) * 64, BUFFER_ALIGNMENT_SIZE(32))
#define SIZE_VP8D_LB_FE_TOP_DATA(width, height) \
	((ALIGN(width, 16) + 8) * 10 * 2)
#define SIZE_VP9D_LB_FE_TOP_DATA(width, height) \
	((ALIGN(ALIGN(width, 16), 64) + 8) * 10 * 2)
#define SIZE_VP8D_LB_PE_TOP_DATA(width, height) \
	((ALIGN(width, 16) >> 4) * 64)
#define SIZE_VP9D_LB_PE_TOP_DATA(width, height) \
	((ALIGN(ALIGN(width, 16), 64) >> 6) * 176)
#define SIZE_VP8D_LB_VSP_TOP(width, height) \
	(((ALIGN(width, 16) >> 4) * 64 / 2) + 256)
#define SIZE_VP9D_LB_VSP_TOP(width, height) \
	(((ALIGN(ALIGN(width, 16), 64) >> 6) * 64 * 8) + 256)


#define HFI_IRIS2_VP9D_COMV_SIZE \
	((((8192 + 63) >> 6) * ((4320 + 63) >> 6) * 8 * 8 * 2 * 8))

#define VPX_DECODER_FRAME_CONCURENCY_LVL 2
#define VPX_DECODER_FRAME_BIN_HDR_BUDGET_RATIO_NUM 1
#define VPX_DECODER_FRAME_BIN_HDR_BUDGET_RATIO_DEN 2
#define VPX_DECODER_FRAME_BIN_RES_BUDGET_RATIO_NUM 3
#define VPX_DECODER_FRAME_BIN_RES_BUDGET_RATIO_DEN 2

#define VP8_NUM_FRAME_INFO_BUF (5 + 1)
#define VP9_NUM_FRAME_INFO_BUF (32)
#define VP8_NUM_PROBABILITY_TABLE_BUF (VP8_NUM_FRAME_INFO_BUF)
#define VP9_NUM_PROBABILITY_TABLE_BUF (VP9_NUM_FRAME_INFO_BUF + 4)
#define VP8_PROB_TABLE_SIZE 3840
#define VP9_PROB_TABLE_SIZE 3840

#define VP9_UDC_HEADER_BUF_SIZE (3 * 128)
#define MAX_SUPERFRAME_HEADER_LEN (34)
#define CCE_TILE_OFFSET_SIZE ALIGN(32 * 4 * 4, BUFFER_ALIGNMENT_SIZE(32))

#define QMATRIX_SIZE (sizeof(u32) * 128 + 256)
#define MP2D_QPDUMP_SIZE 115200

#define HFI_IRIS2_ENC_PERSIST_SIZE 204800

#define HFI_MAX_COL_FRAME 6
#define HFI_VENUS_VENC_TRE_WB_BUFF_SIZE (65 << 4) // bytes
#define HFI_VENUS_VENC_DB_LINE_BUFF_PER_MB      512
#define HFI_VENUS_VPPSG_MAX_REGISTERS  2048
#define HFI_VENUS_WIDTH_ALIGNMENT 128
#define HFI_VENUS_WIDTH_TEN_BIT_ALIGNMENT 192
#define HFI_VENUS_HEIGHT_ALIGNMENT 32

#define SYSTEM_LAL_TILE10 192
#define NUM_MBS_720P (((1280 + 15) >> 4) * ((720 + 15) >> 4))
#define NUM_MBS_4k (((4096 + 15) >> 4) * ((2304 + 15) >> 4))
#define MB_SIZE_IN_PIXEL (16 * 16)
#define HDR10PLUS_PAYLOAD_SIZE 1024
#define HDR10_HIST_EXTRADATA_SIZE 4096

static int msm_vidc_get_extra_input_buff_count(struct msm_vidc_inst *inst);
static int msm_vidc_get_extra_output_buff_count(struct msm_vidc_inst *inst);

static inline u32 calculate_h264d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced, u32 delay,
	u32 num_vpp_pipes);
static inline u32 calculate_h265d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced, u32 delay,
	u32 num_vpp_pipes);
static inline u32 calculate_vpxd_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced, u32 delay,
	u32 num_vpp_pipes);
static inline u32 calculate_mpeg2d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced, u32 delay,
	u32 num_vpp_pipes);

static inline u32 calculate_enc_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 lcu_size, u32 num_vpp_pipes);
static inline u32 calculate_h264e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 num_vpp_pipes);
static inline u32 calculate_h265e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 num_vpp_pipes);
static inline u32 calculate_vp8e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 num_vpp_pipes);

static inline u32 calculate_h264d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes);
static inline u32 calculate_h265d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes);
static inline u32 calculate_vp8d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes);
static inline u32 calculate_vp9d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes);
static inline u32 calculate_mpeg2d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes);

static inline u32 calculate_h264e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit, u32 num_vpp_pipes);
static inline u32 calculate_h265e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit, u32 num_vpp_pipes);
static inline u32 calculate_vp8e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit, u32 num_vpp_pipes);

static inline u32 calculate_enc_scratch2_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit, bool downscale,
	u32 rotation_val, u32 flip);

static inline u32 calculate_enc_persist_size(void);

static inline u32 calculate_h264d_persist1_size(void);
static inline u32 calculate_h265d_persist1_size(void);
static inline u32 calculate_vp8d_persist1_size(void);
static inline u32 calculate_vp9d_persist1_size(void);
static inline u32 calculate_mpeg2d_persist1_size(void);

static struct msm_vidc_dec_buff_size_calculators h264d_calculators = {
	.calculate_scratch_size = calculate_h264d_scratch_size,
	.calculate_scratch1_size = calculate_h264d_scratch1_size,
	.calculate_persist1_size = calculate_h264d_persist1_size,
};

static struct msm_vidc_dec_buff_size_calculators h265d_calculators = {
	.calculate_scratch_size = calculate_h265d_scratch_size,
	.calculate_scratch1_size = calculate_h265d_scratch1_size,
	.calculate_persist1_size = calculate_h265d_persist1_size,
};

static struct msm_vidc_dec_buff_size_calculators vp8d_calculators = {
	.calculate_scratch_size = calculate_vpxd_scratch_size,
	.calculate_scratch1_size = calculate_vp8d_scratch1_size,
	.calculate_persist1_size = calculate_vp8d_persist1_size,
};

static struct msm_vidc_dec_buff_size_calculators vp9d_calculators = {
	.calculate_scratch_size = calculate_vpxd_scratch_size,
	.calculate_scratch1_size = calculate_vp9d_scratch1_size,
	.calculate_persist1_size = calculate_vp9d_persist1_size,
};

static struct msm_vidc_dec_buff_size_calculators mpeg2d_calculators = {
	.calculate_scratch_size = calculate_mpeg2d_scratch_size,
	.calculate_scratch1_size = calculate_mpeg2d_scratch1_size,
	.calculate_persist1_size = calculate_mpeg2d_persist1_size,
};

static struct msm_vidc_enc_buff_size_calculators h264e_calculators = {
	.calculate_scratch_size = calculate_h264e_scratch_size,
	.calculate_scratch1_size = calculate_h264e_scratch1_size,
	.calculate_scratch2_size = calculate_enc_scratch2_size,
	.calculate_persist_size = calculate_enc_persist_size,
};

static struct msm_vidc_enc_buff_size_calculators h265e_calculators = {
	.calculate_scratch_size = calculate_h265e_scratch_size,
	.calculate_scratch1_size = calculate_h265e_scratch1_size,
	.calculate_scratch2_size = calculate_enc_scratch2_size,
	.calculate_persist_size = calculate_enc_persist_size,
};

static struct msm_vidc_enc_buff_size_calculators vp8e_calculators = {
	.calculate_scratch_size = calculate_vp8e_scratch_size,
	.calculate_scratch1_size = calculate_vp8e_scratch1_size,
	.calculate_scratch2_size = calculate_enc_scratch2_size,
	.calculate_persist_size = calculate_enc_persist_size,
};

int msm_vidc_get_decoder_internal_buffer_sizes(struct msm_vidc_inst *inst)
{
	struct msm_vidc_dec_buff_size_calculators *dec_calculators;
	u32 width, height, i, out_min_count, num_vpp_pipes;
	struct v4l2_format *f;
	u32 vpp_delay;

	if (!inst || !inst->core || !inst->core->platform_data) {
		d_vpr_e("%s: Instance is null!", __func__);
		return -EINVAL;
	}

	vpp_delay = inst->bse_vpp_delay;

	num_vpp_pipes = inst->core->platform_data->num_vpp_pipes;
	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	switch (f->fmt.pix_mp.pixelformat) {
	case V4L2_PIX_FMT_H264:
		dec_calculators = &h264d_calculators;
		break;
	case V4L2_PIX_FMT_HEVC:
		dec_calculators = &h265d_calculators;
		break;
	case V4L2_PIX_FMT_VP8:
		dec_calculators = &vp8d_calculators;
		break;
	case V4L2_PIX_FMT_VP9:
		dec_calculators = &vp9d_calculators;
		break;
	case V4L2_PIX_FMT_MPEG2:
		dec_calculators = &mpeg2d_calculators;
		break;
	default:
		s_vpr_e(inst->sid,
			"Invalid pix format. Internal buffer cal not defined : %x\n",
			f->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}

	width = f->fmt.pix_mp.width;
	height = f->fmt.pix_mp.height;
	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		struct hal_buffer_requirements *curr_req;
		bool valid_buffer_type = false;

		curr_req = &inst->buff_req.buffer[i];
		if (curr_req->buffer_type == HAL_BUFFER_INTERNAL_SCRATCH) {
			bool is_interlaced = false;

			is_interlaced = (inst->pic_struct ==
				MSM_VIDC_PIC_STRUCT_MAYBE_INTERLACED);
			curr_req->buffer_size =
				dec_calculators->calculate_scratch_size(
					inst, width, height, is_interlaced,
					vpp_delay, num_vpp_pipes);
			valid_buffer_type = true;
		} else  if (curr_req->buffer_type ==
			HAL_BUFFER_INTERNAL_SCRATCH_1) {
			struct msm_vidc_format *fmt = NULL;

			fmt = &inst->fmts[OUTPUT_PORT];
			out_min_count = fmt->count_min;
			out_min_count =
				max(vpp_delay + 1, out_min_count);
			curr_req->buffer_size =
				dec_calculators->calculate_scratch1_size(
					inst, width, height, out_min_count,
					is_secondary_output_mode(inst),
					num_vpp_pipes);
			valid_buffer_type = true;
		} else if (curr_req->buffer_type ==
			HAL_BUFFER_INTERNAL_PERSIST_1) {
			curr_req->buffer_size =
				dec_calculators->calculate_persist1_size();
			valid_buffer_type = true;
		}

		if (valid_buffer_type) {
			curr_req->buffer_alignment = 256;
			curr_req->buffer_count_actual =
				curr_req->buffer_count_min =
				curr_req->buffer_count_min_host = 1;
		}
	}
	return 0;
}

int msm_vidc_get_num_ref_frames(struct msm_vidc_inst *inst)
{
	int num_ref = 1;
	int num_bframes = -1, ltr_count = -1;
	struct v4l2_ctrl *bframe_ctrl = NULL;
	struct v4l2_ctrl *ltr_ctrl = NULL;
	struct v4l2_ctrl *frame_t = NULL;
	struct v4l2_ctrl *max_layer = NULL;
	u32 codec;

	bframe_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
	num_bframes = bframe_ctrl->val;
	if (num_bframes > 0)
		num_ref = num_bframes + 1;

	ltr_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	ltr_count = ltr_ctrl->val;
	/* B and LTR can't be at same time */
	if (ltr_count > 0)
		num_ref = num_ref + ltr_count;

	frame_t = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_TYPE);
	max_layer = get_ctrl(inst,
		V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
	if (frame_t->val == V4L2_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_P &&
		max_layer->val > 0) {
		codec = get_v4l2_codec(inst);
		/* LTR and B - frame not supported with hybrid HP */
		if (inst->hybrid_hp)
			num_ref = (max_layer->val - 1);
		else if (codec == V4L2_PIX_FMT_HEVC)
			num_ref = ((max_layer->val + 1) / 2) + ltr_count;
		else if ((codec == V4L2_PIX_FMT_H264) && (max_layer->val <= 4))
			num_ref = ((1 << (max_layer->val - 1)) - 1) + ltr_count;
		else
			num_ref = ((max_layer->val + 1) / 2) + ltr_count;
	}

	if (is_hier_b_session(inst)) {
		num_ref = (1 << (max_layer->val - 1)) / 2 + 1;
	}

	return num_ref;
}

int msm_vidc_get_encoder_internal_buffer_sizes(struct msm_vidc_inst *inst)
{
	struct msm_vidc_enc_buff_size_calculators *enc_calculators;
	u32 width, height, i, num_ref, num_vpp_pipes;
	u32 rotation_val = 0, flip = 0;
	bool is_tenbit = false, is_downscale = false;
	int num_bframes;
	struct v4l2_ctrl *bframe, *rotation, *hflip, *vflip;
	struct v4l2_format *f;

	if (!inst || !inst->core || !inst->core->platform_data) {
		d_vpr_e("%s: Instance is null!", __func__);
		return -EINVAL;
	}

	num_vpp_pipes = inst->core->platform_data->num_vpp_pipes;
	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	switch (f->fmt.pix_mp.pixelformat) {
	case V4L2_PIX_FMT_H264:
		enc_calculators = &h264e_calculators;
		break;
	case V4L2_PIX_FMT_HEVC:
		enc_calculators = &h265e_calculators;
		break;
	case V4L2_PIX_FMT_VP8:
		enc_calculators = &vp8e_calculators;
		break;
	default:
		s_vpr_e(inst->sid,
			"Invalid pix format. Internal buffer cal not defined : %x ",
			f->fmt.pix_mp.pixelformat);
		return -EINVAL;
	}

	bframe = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
	num_bframes = bframe->val;
	if (num_bframes < 0) {
		s_vpr_e(inst->sid, "%s: get num bframe failed\n", __func__);
		return -EINVAL;
	}
	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	rotation = get_ctrl(inst, V4L2_CID_ROTATE);
	rotation_val = rotation->val;
	if (rotation_val == 90 || rotation_val == 270) {
		/* Internal buffer size calculators are based on rotated w x h */
		width = f->fmt.pix_mp.height;
		height = f->fmt.pix_mp.width;
	} else {
		width = f->fmt.pix_mp.width;
		height = f->fmt.pix_mp.height;
	}
	hflip = get_ctrl(inst, V4L2_CID_HFLIP);
	vflip = get_ctrl(inst, V4L2_CID_VFLIP);
	flip = hflip->val | vflip->val;

	num_ref = msm_vidc_get_num_ref_frames(inst);
	is_tenbit = (inst->bit_depth == MSM_VIDC_BIT_DEPTH_10);
	is_downscale = vidc_scalar_enabled(inst);

	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		struct hal_buffer_requirements *curr_req;
		bool valid_buffer_type = false;

		curr_req = &inst->buff_req.buffer[i];
		if (curr_req->buffer_type == HAL_BUFFER_INTERNAL_SCRATCH) {
			curr_req->buffer_size =
				enc_calculators->calculate_scratch_size(
					inst, width, height,
					inst->clk_data.work_mode,
					num_vpp_pipes);
			valid_buffer_type = true;
		} else  if (curr_req->buffer_type ==
			HAL_BUFFER_INTERNAL_SCRATCH_1) {
			curr_req->buffer_size =
				enc_calculators->calculate_scratch1_size(
					inst, width, height, num_ref,
					is_tenbit, num_vpp_pipes);
			valid_buffer_type = true;
		} else if (curr_req->buffer_type ==
			HAL_BUFFER_INTERNAL_SCRATCH_2) {
			curr_req->buffer_size =
				enc_calculators->calculate_scratch2_size(
					inst, width, height, num_ref,
					is_tenbit, is_downscale, rotation_val, flip);
			valid_buffer_type = true;
		} else if (curr_req->buffer_type ==
			HAL_BUFFER_INTERNAL_PERSIST) {
			curr_req->buffer_size =
				enc_calculators->calculate_persist_size();
			valid_buffer_type = true;
		}

		if (valid_buffer_type) {
			curr_req->buffer_alignment = 256;
			curr_req->buffer_count_actual =
				curr_req->buffer_count_min =
				curr_req->buffer_count_min_host = 1;
		}
	}
	return 0;
}

int msm_vidc_calculate_internal_buffer_sizes(struct msm_vidc_inst *inst)
{
	if (!inst) {
		d_vpr_e("%s: Instance is null!", __func__);
		return -EINVAL;
	}

	if (inst->session_type == MSM_VIDC_DECODER)
		return msm_vidc_get_decoder_internal_buffer_sizes(inst);
	else if (inst->session_type == MSM_VIDC_ENCODER)
		return msm_vidc_get_encoder_internal_buffer_sizes(inst);

	return 0;
}

void msm_vidc_init_buffer_size_calculators(struct msm_vidc_inst *inst)
{
	struct msm_vidc_core *core;
	uint32_t vpu;

	if (!inst)
		return;

	inst->buffer_size_calculators = NULL;
	core = inst->core;
	vpu = core->platform_data->vpu_ver;

	/* Change this to IRIS2 when ready */
	if (vpu == VPU_VERSION_IRIS2 || vpu == VPU_VERSION_IRIS2_1)
		inst->buffer_size_calculators =
			msm_vidc_calculate_internal_buffer_sizes;
}

int msm_vidc_calculate_input_buffer_count(struct msm_vidc_inst *inst)
{
	struct msm_vidc_format *fmt;
	int extra_buff_count = 0;
	struct v4l2_ctrl *max_layer = NULL;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	fmt = &inst->fmts[INPUT_PORT];

	if (!is_decode_session(inst) && !is_encode_session(inst))
		return 0;

	/* do not change buffer count while session is running  */
	if (inst->state == MSM_VIDC_START_DONE)
		return 0;

	if (is_thumbnail_session(inst)) {
		fmt->count_min = fmt->count_min_host = fmt->count_actual =
			SINGLE_INPUT_BUFFER;
		return 0;
	}

	if (is_grid_session(inst)) {
		fmt->count_min = fmt->count_min_host = fmt->count_actual =
			SINGLE_INPUT_BUFFER + 1;
		return 0;
	}

	extra_buff_count = msm_vidc_get_extra_buff_count(inst,
				HAL_BUFFER_INPUT);
	fmt->count_min = MIN_INPUT_BUFFERS;

	if (is_hier_b_session(inst)) {
		max_layer = get_ctrl(inst,
			V4L2_CID_MPEG_VIDC_VIDEO_HEVC_MAX_HIER_CODING_LAYER);
		fmt->count_min = (1 << (max_layer->val - 1)) + 2;
	}

	fmt->count_min_host = fmt->count_actual =
		fmt->count_min + extra_buff_count;

	s_vpr_h(inst->sid, "%s: input min %d min_host %d actual %d\n",
		__func__, fmt->count_min,
		fmt->count_min_host, fmt->count_actual);

	return 0;
}

int msm_vidc_calculate_output_buffer_count(struct msm_vidc_inst *inst)
{
	struct msm_vidc_format *fmt;
	int extra_buff_count = 0;
	u32 codec, output_min_count;

	if (!inst) {
		d_vpr_e("%s: invalid params\n", __func__);
		return -EINVAL;
	}
	fmt = &inst->fmts[OUTPUT_PORT];
	codec = get_v4l2_codec(inst);

	if (!is_decode_session(inst) && !is_encode_session(inst))
		return 0;

	/* do not change buffer count while session is running  */
	if (inst->state == MSM_VIDC_START_DONE)
		return 0;

	if (is_thumbnail_session(inst)) {
		fmt->count_min = (codec == V4L2_PIX_FMT_VP9) ?
			VP9_REFERENCE_COUNT : SINGLE_OUTPUT_BUFFER;
		fmt->count_min_host = fmt->count_actual = fmt->count_min;
		return 0;
	}

	/* Update output buff count: Changes for decoder based on codec */
	if (is_decode_session(inst)) {
		switch (codec) {
		case V4L2_PIX_FMT_MPEG2:
		case V4L2_PIX_FMT_VP8:
			output_min_count = 6;
			break;
		case V4L2_PIX_FMT_VP9:
			output_min_count = 9;
			break;
		default:
			output_min_count = 4; //H264, HEVC
		}
	} else {
		output_min_count = MIN_ENC_OUTPUT_BUFFERS;
	}

	if (is_vpp_delay_allowed(inst)) {
		output_min_count =
			max(output_min_count, (u32)MAX_BSE_VPP_DELAY);
		output_min_count =
			max(output_min_count, (u32)(msm_vidc_vpp_delay & 0x1F));
	}

	extra_buff_count = msm_vidc_get_extra_buff_count(inst,
				HAL_BUFFER_OUTPUT);
	fmt->count_min = output_min_count;
	fmt->count_min_host = fmt->count_actual =
		fmt->count_min + extra_buff_count;

	s_vpr_h(inst->sid, "%s: output min %d min_host %d actual %d\n",
		__func__, fmt->count_min, fmt->count_min_host,
		fmt->count_actual);

	return 0;
}

int msm_vidc_calculate_buffer_counts(struct msm_vidc_inst *inst)
{
	int rc;

	rc = msm_vidc_calculate_input_buffer_count(inst);
	if (rc)
		return rc;
	rc = msm_vidc_calculate_output_buffer_count(inst);
	if (rc)
		return rc;

	return rc;
}

int msm_vidc_get_extra_buff_count(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type)
{
	if (!inst || !inst->core) {
		d_vpr_e("%s: Invalid args\n", __func__);
		return 0;
	}

	if (!is_decode_session(inst) && !is_encode_session(inst))
		return 0;

	if (buffer_type == HAL_BUFFER_OUTPUT)
		return msm_vidc_get_extra_output_buff_count(inst);
	else if (buffer_type == HAL_BUFFER_INPUT)
		return msm_vidc_get_extra_input_buff_count(inst);

	return 0;
}

static int msm_vidc_get_extra_input_buff_count(struct msm_vidc_inst *inst)
{
	unsigned int extra_input_count = 0;
	struct msm_vidc_core *core;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	core = inst->core;

	if (is_heif_decoder(inst))
		return (HEIF_DEC_TOTAL_INPUT_BUFFERS - MIN_INPUT_BUFFERS);

	/*
	 * For thumbnail session, extra buffers are not required as
	 * neither dcvs nor batching will be enabled.
	 */
	if (is_thumbnail_session(inst))
		return extra_input_count;

	if (is_decode_session(inst)) {
		/* add 2 extra buffers for batching */
		if (inst->batch.enable)
			extra_input_count = (BATCH_DEC_TOTAL_INPUT_BUFFERS -
				MIN_INPUT_BUFFERS);
	} else if (is_encode_session(inst)) {
		/* add 4 extra buffers for dcvs */
		if (core->resources.dcvs)
			extra_input_count = DCVS_ENC_EXTRA_INPUT_BUFFERS;
	}
	return extra_input_count;
}

static int msm_vidc_get_extra_output_buff_count(struct msm_vidc_inst *inst)
{
	unsigned int extra_output_count = 0;
	struct msm_vidc_core *core;

	if (!inst || !inst->core) {
		d_vpr_e("%s: invalid params %pK\n", __func__, inst);
		return -EINVAL;
	}

	core = inst->core;

	if (is_heif_decoder(inst))
		return HEIF_DEC_EXTRA_OUTPUT_BUFFERS;

	/*
	 * For a non-realtime session, extra buffers are not required.
	 * For thumbnail session, extra buffers are not required as
	 * neither dcvs nor batching will be enabled.
	 */
	if (!is_realtime_session(inst) || is_thumbnail_session(inst))
		return extra_output_count;

	/* For HEIF encoder, we are increasing buffer count */
	if (is_image_session(inst)) {
		extra_output_count = (HEIF_ENC_TOTAL_OUTPUT_BUFFERS -
			MIN_ENC_OUTPUT_BUFFERS);
		return extra_output_count;
	}

	if (is_decode_session(inst)) {
		/* add 4 extra buffers for dcvs */
		if (core->resources.dcvs)
			extra_output_count = DCVS_DEC_EXTRA_OUTPUT_BUFFERS;
		/*
		 * Minimum number of decoder output buffers is codec specific.
		 * If platform supports decode batching ensure minimum 6 extra
		 * output buffers. Else add 4 extra output buffers for DCVS.
		 */
		if (inst->batch.enable)
			extra_output_count = BATCH_DEC_EXTRA_OUTPUT_BUFFERS;
	}
	return extra_output_count;
}

u32 msm_vidc_calculate_dec_input_frame_size(struct msm_vidc_inst *inst, u32 buffer_size_limit)
{
	u32 frame_size, num_mbs;
	u32 div_factor = 1;
	u32 base_res_mbs = NUM_MBS_4k;
	struct v4l2_format *f;

	/*
	 * Decoder input size calculation:
	 * If clip is 8k buffer size is calculated for 8k : 8k mbs/4
	 * For 8k cases we expect width/height to be set always.
	 * In all other cases size is calculated for 4k:
	 * 4k mbs for VP8/VP9 and 4k/2 for remaining codecs
	 */
	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	num_mbs = msm_vidc_get_mbs_per_frame(inst);
	if (num_mbs > NUM_MBS_4k) {
		div_factor = 4;
		base_res_mbs = inst->capability.cap[CAP_MBS_PER_FRAME].max;
	} else {
		base_res_mbs = NUM_MBS_4k;
		if (f->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_VP9)
			div_factor = 1;
		else
			div_factor = 2;
	}

	if (is_secure_session(inst))
		div_factor = div_factor << 1;

	/*
	 * For targets that doesn't support 4k, consider max mb's for that
	 * target and allocate max input buffer size for the same
	 */
	if (inst->core->platform_data->vpu_ver == VPU_VERSION_AR50_LITE) {
		base_res_mbs = inst->capability.cap[CAP_MBS_PER_FRAME].max;
		div_factor = 1;
		if (num_mbs < NUM_MBS_720P)
			base_res_mbs = base_res_mbs * 2;
	}
	/* For HEIF image, use the actual resolution to calc buffer size */
	if (is_heif_decoder(inst)) {
		base_res_mbs = num_mbs;
		div_factor = 1;
	}

	frame_size = base_res_mbs * MB_SIZE_IN_PIXEL * 3 / 2 / div_factor;

	 /* multiply by 10/8 (1.25) to get size for 10 bit case */
	if ((f->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_VP9 ||
		f->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_HEVC) &&
		inst->core->platform_data->vpu_ver != VPU_VERSION_AR50_LITE)
		frame_size = frame_size + (frame_size >> 2);

	if (buffer_size_limit && (buffer_size_limit < frame_size)) {
		frame_size = buffer_size_limit;
		s_vpr_h(inst->sid, "input buffer size limited to %d\n",
			frame_size);
	} else {
		s_vpr_h(inst->sid, "set input buffer size to %d\n",
			frame_size);
	}

	return ALIGN(frame_size, SZ_4K);
}

u32 msm_vidc_calculate_dec_output_frame_size(struct msm_vidc_inst *inst)
{
	u32 hfi_fmt;
	struct v4l2_format *f;

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	hfi_fmt = msm_comm_convert_color_fmt(f->fmt.pix_mp.pixelformat,
					inst->sid);
	return VENUS_BUFFER_SIZE(hfi_fmt, f->fmt.pix_mp.width,
			f->fmt.pix_mp.height);
}

u32 msm_vidc_calculate_dec_output_extra_size(struct msm_vidc_inst *inst)
{
	struct v4l2_format *f;

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	return VENUS_EXTRADATA_SIZE(f->fmt.pix_mp.width, f->fmt.pix_mp.height);
}

u32 msm_vidc_calculate_enc_input_frame_size(struct msm_vidc_inst *inst)
{
	u32 hfi_fmt;
	struct v4l2_format *f;

	f = &inst->fmts[INPUT_PORT].v4l2_fmt;
	hfi_fmt = msm_comm_convert_color_fmt(f->fmt.pix_mp.pixelformat,
					inst->sid);
	return VENUS_BUFFER_SIZE(hfi_fmt, f->fmt.pix_mp.width,
			f->fmt.pix_mp.height);
}

u32 msm_vidc_calculate_enc_output_frame_size(struct msm_vidc_inst *inst)
{
	u32 frame_size;
	u32 mbs_per_frame;
	u32 width, height;
	struct v4l2_format *f;

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	/*
	 * Encoder output size calculation: 32 Align width/height
	 * For CQ or heic session : YUVsize * 2
	 * For resolution < 720p : YUVsize * 4
	 * For resolution > 720p & <= 4K : YUVsize / 2
	 * For resolution > 4k : YUVsize / 4
	 * Initially frame_size = YUVsize * 2;
	 */

	if (is_grid_session(inst)) {
		f->fmt.pix_mp.width = f->fmt.pix_mp.height = HEIC_GRID_DIMENSION;
	}
	width = ALIGN(f->fmt.pix_mp.width, BUFFER_ALIGNMENT_SIZE(32));
	height = ALIGN(f->fmt.pix_mp.height, BUFFER_ALIGNMENT_SIZE(32));
	mbs_per_frame = NUM_MBS_PER_FRAME(width, height);
	frame_size = (width * height * 3);

	if (inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ ||
		is_grid_session(inst) || is_image_session(inst))
		goto calc_done;

	if (mbs_per_frame < NUM_MBS_720P)
		frame_size = frame_size << 1;
	else if (mbs_per_frame <= NUM_MBS_4k)
		frame_size = frame_size >> 2;
	else
		frame_size = frame_size >> 3;

	if (inst->rc_type == RATE_CONTROL_OFF)
		frame_size = frame_size << 1;

	if (inst->rc_type == RATE_CONTROL_LOSSLESS)
		frame_size = (width * height * 9) >> 2;

	/* multiply by 10/8 (1.25) to get size for 10 bit case */
	if (inst->core->platform_data->vpu_ver != VPU_VERSION_AR50_LITE &&
		f->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_HEVC) {
		frame_size = frame_size + (frame_size >> 2);
	}

calc_done:
	return ALIGN(frame_size, SZ_4K);
}

static inline u32 ROI_EXTRADATA_SIZE(
	u32 width, u32 height, u32 lcu_size) {
	u32 lcu_width = 0;
	u32 lcu_height = 0;
	u32 n_shift = 0;

	while (lcu_size && !(lcu_size & 0x1)) {
		n_shift++;
		lcu_size = lcu_size >> 1;
	}
	lcu_width = (width + (lcu_size - 1)) >> n_shift;
	lcu_height = (height + (lcu_size - 1)) >> n_shift;

	return (((lcu_width + 7) >> 3) << 3) * lcu_height * 2;
}

u32 msm_vidc_calculate_enc_input_extra_size(struct msm_vidc_inst *inst)
{
	u32 size = 0;
	u32 extradata_count = 0;
	struct v4l2_format *f;

	f = &inst->fmts[OUTPUT_PORT].v4l2_fmt;
	/* Add size for default extradata */
	size += sizeof(struct msm_vidc_enc_cvp_metadata_payload);
	extradata_count++;

	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_ROI) {
		u32 lcu_size = 16;

		if (f->fmt.pix_mp.pixelformat == V4L2_PIX_FMT_HEVC)
			lcu_size = 32;

		f = &inst->fmts[INPUT_PORT].v4l2_fmt;
		size += ROI_EXTRADATA_SIZE(f->fmt.pix_mp.width,
			f->fmt.pix_mp.height, lcu_size);
		extradata_count++;
	}

	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_HDR10PLUS) {
		size += HDR10PLUS_PAYLOAD_SIZE;
		extradata_count++;
	}

	/* Add extradata header sizes including EXTRADATA_NONE */
	if (size)
		size += sizeof(struct msm_vidc_extradata_header) *
				(extradata_count + 1);

	return ALIGN(size, SZ_4K);
}

u32 msm_vidc_calculate_enc_output_extra_size(struct msm_vidc_inst *inst)
{
	u32 size = 0;

	if (inst->prop.extradata_ctrls & EXTRADATA_ADVANCED)
		size += sizeof(struct msm_vidc_metadata_ltr_payload);

	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_FRAME_QP)
		size += sizeof(struct msm_vidc_frame_qp_payload);

	/* Add size for extradata none */
	if (size)
		size += sizeof(struct msm_vidc_extradata_header);

	return ALIGN(size, SZ_4K);
}

static inline u32 size_vpss_lb(u32 width, u32 height, u32 num_vpp_pipes)
{
	u32 vpss_4tap_top_buffer_size, vpss_div2_top_buffer_size;
	u32 vpss_4tap_left_buffer_size, vpss_div2_left_buffer_size;
	u32 opb_wr_top_line_luma_buf_size, opb_wr_top_line_chroma_buf_size;
	u32 opb_lb_wr_llb_y_buffer_size, opb_lb_wr_llb_uv_buffer_size;
	u32 macrotiling_size;
	u32 size = 0;

	vpss_4tap_top_buffer_size = vpss_div2_top_buffer_size =
		vpss_4tap_left_buffer_size = vpss_div2_left_buffer_size = 0;
	macrotiling_size = 32;
	opb_wr_top_line_luma_buf_size = ALIGN(width, macrotiling_size) /
		macrotiling_size * 256;
	opb_wr_top_line_luma_buf_size = ALIGN(opb_wr_top_line_luma_buf_size,
		VENUS_DMA_ALIGNMENT) + (MAX_TILE_COLUMNS - 1) * 256;
	opb_wr_top_line_luma_buf_size = max(opb_wr_top_line_luma_buf_size,
		(32 * ALIGN(height, 16)));
	opb_wr_top_line_chroma_buf_size = opb_wr_top_line_luma_buf_size;
	opb_lb_wr_llb_uv_buffer_size = opb_lb_wr_llb_y_buffer_size =
		ALIGN((ALIGN(height, 16) / 2) *
			64, BUFFER_ALIGNMENT_SIZE(32));
	size = num_vpp_pipes * 2 * (vpss_4tap_top_buffer_size +
		vpss_div2_top_buffer_size) +
		2 * (vpss_4tap_left_buffer_size +
		vpss_div2_left_buffer_size) +
	opb_wr_top_line_luma_buf_size +
	opb_wr_top_line_chroma_buf_size +
	opb_lb_wr_llb_uv_buffer_size +
	opb_lb_wr_llb_y_buffer_size;

	return size;
}

static inline u32 hfi_iris2_h264d_comv_size(u32 width, u32 height,
	u32 yuv_buf_min_count)
{
	u32 comv_size = 0;
	u32 frame_width_in_mbs = ((width + 15) >> 4);
	u32 frame_height_in_mbs = ((height + 15) >> 4);
	u32 col_mv_aligned_width = (frame_width_in_mbs << 7);
	u32 col_zero_aligned_width = (frame_width_in_mbs << 2);
	u32 col_zero_size = 0, size_colloc = 0;

	col_mv_aligned_width = ALIGN(col_mv_aligned_width,
		BUFFER_ALIGNMENT_SIZE(16));
	col_zero_aligned_width = ALIGN(col_zero_aligned_width,
		BUFFER_ALIGNMENT_SIZE(16));
	col_zero_size = col_zero_aligned_width *
		((frame_height_in_mbs + 1) >> 1);
	col_zero_size = ALIGN(col_zero_size, BUFFER_ALIGNMENT_SIZE(64));
	col_zero_size <<= 1;
	col_zero_size = ALIGN(col_zero_size, BUFFER_ALIGNMENT_SIZE(512));
	size_colloc = col_mv_aligned_width  * ((frame_height_in_mbs + 1) >> 1);
	size_colloc = ALIGN(size_colloc, BUFFER_ALIGNMENT_SIZE(64));
	size_colloc <<= 1;
	size_colloc = ALIGN(size_colloc, BUFFER_ALIGNMENT_SIZE(512));
	size_colloc += (col_zero_size + SIZE_H264D_BUFTAB_T * 2);
	comv_size = size_colloc * yuv_buf_min_count;
	comv_size += BUFFER_ALIGNMENT_SIZE(512);

	return comv_size;
}

static inline u32 size_h264d_bse_cmd_buf(u32 height)
{
	u32 aligned_height = ALIGN(height, BUFFER_ALIGNMENT_SIZE(32));

	return min_t(u32, (((aligned_height + 15) >> 4) * 3 * 4),
		H264D_MAX_SLICE) *
		SIZE_H264D_BSE_CMD_PER_BUF;
}

static inline u32 size_h264d_vpp_cmd_buf(u32 height)
{
	u32 size = 0;
	u32 aligned_height = ALIGN(height, BUFFER_ALIGNMENT_SIZE(32));

	size = min_t(u32, (((aligned_height + 15) >> 4) * 3 * 4),
		H264D_MAX_SLICE) *
		SIZE_H264D_VPP_CMD_PER_BUF;
	if (size > VPP_CMD_MAX_SIZE)
		size = VPP_CMD_MAX_SIZE;
	return size;
}

static inline u32 hfi_iris2_h264d_non_comv_size(u32 width, u32 height,
	u32 num_vpp_pipes)
{
	u32 size;
	u32 size_bse, size_vpp;

	size_bse = size_h264d_bse_cmd_buf(height);
	size_vpp = size_h264d_vpp_cmd_buf(height);
	size = ALIGN(size_bse, VENUS_DMA_ALIGNMENT) +
		ALIGN(size_vpp, VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_HW_PIC(SIZE_H264D_HW_PIC_T), VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H264D_LB_FE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H264D_LB_FE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H264D_LB_FE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_H264D_LB_SE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H264D_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_H264D_LB_PE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H264D_LB_VSP_TOP(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H264D_LB_RECON_DMA_METADATA_WR(width, height),
			VENUS_DMA_ALIGNMENT) * 2 +
		ALIGN(SIZE_H264D_QP(width, height), VENUS_DMA_ALIGNMENT);
	size = ALIGN(size, VENUS_DMA_ALIGNMENT);
	return size;
}

static inline u32 size_h264d_hw_bin_buffer(u32 width, u32 height, u32 delay,
	u32 num_vpp_pipes)
{
	u32 size_yuv, size_bin_hdr, size_bin_res;
	u32 size = 0;
	u32 product;

	product = width * height;
	size_yuv = (product <= BIN_BUFFER_THRESHOLD) ?
			((BIN_BUFFER_THRESHOLD * 3) >> 1) :
			((product * 3) >> 1);

	size_bin_hdr = size_yuv * H264_CABAC_HDR_RATIO_HD_TOT;
	size_bin_res = size_yuv * H264_CABAC_RES_RATIO_HD_TOT;
	size_bin_hdr = size_bin_hdr * (((((u32)(delay)) & 31) / 10) + 2) / 2;
	size_bin_res = size_bin_res * (((((u32)(delay)) & 31) / 10) + 2) / 2;
	size_bin_hdr = ALIGN(size_bin_hdr / num_vpp_pipes,
		VENUS_DMA_ALIGNMENT) * num_vpp_pipes;
	size_bin_res = ALIGN(size_bin_res / num_vpp_pipes,
		VENUS_DMA_ALIGNMENT) * num_vpp_pipes;
	size = size_bin_hdr + size_bin_res;
	return size;
}

static inline u32 calculate_h264d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced, u32 delay, u32 num_vpp_pipes)
{
	u32 aligned_width = ALIGN(width, BUFFER_ALIGNMENT_SIZE(16));
	u32 aligned_height = ALIGN(height, BUFFER_ALIGNMENT_SIZE(16));
	u32 size = 0;

	if (!is_interlaced)
		size = size_h264d_hw_bin_buffer(aligned_width, aligned_height,
						delay, num_vpp_pipes);
	else
		size = 0;

	return size;
}

static inline u32 size_h265d_bse_cmd_buf(u32 width, u32 height)
{
	u32 size;

	size = (ALIGN(width, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS) *
		(ALIGN(height, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS) *
		NUM_HW_PIC_BUF;
	size = min_t(u32, size, H265D_MAX_SLICE + 1);
	size = 2 * size * SIZE_H265D_BSE_CMD_PER_BUF;
	size = ALIGN(size, VENUS_DMA_ALIGNMENT);

	return size;
}

static inline u32 size_h265d_vpp_cmd_buf(u32 width, u32 height)
{
	u32 size = 0;

	size = (ALIGN(width, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS) *
		(ALIGN(height, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS) *
		NUM_HW_PIC_BUF;
	size = min_t(u32, size, H265D_MAX_SLICE + 1);
	size = ALIGN(size, 4);
	size = 2 * size * SIZE_H265D_VPP_CMD_PER_BUF;
	size = ALIGN(size, VENUS_DMA_ALIGNMENT);
	if (size > VPP_CMD_MAX_SIZE)
		size = VPP_CMD_MAX_SIZE;
	return size;
}

static inline u32 hfi_iris2_h265d_comv_size(u32 width, u32 height,
	u32 yuv_buf_count_min)
{
	u32 size = 0;

	size = ALIGN(((((width + 15) >> 4) * ((height + 15) >> 4)) << 8),
		BUFFER_ALIGNMENT_SIZE(512));
	size *= yuv_buf_count_min;
	size += BUFFER_ALIGNMENT_SIZE(512);

	return size;
}

static inline u32 hfi_iris2_h265d_non_comv_size(u32 width, u32 height,
	u32 num_vpp_pipes)
{
	u32 size_bse, size_vpp;
	u32 size = 0;

	size_bse = size_h265d_bse_cmd_buf(width, height);
	size_vpp = size_h265d_vpp_cmd_buf(width, height);
	size = ALIGN(size_bse, VENUS_DMA_ALIGNMENT) +
		ALIGN(size_vpp, VENUS_DMA_ALIGNMENT) +
		ALIGN(NUM_HW_PIC_BUF * 20 * 22 * 4, VENUS_DMA_ALIGNMENT) +
		ALIGN(2 * sizeof(u16) *
		(ALIGN(width, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS) *
			(ALIGN(height, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_HW_PIC(SIZE_H265D_HW_PIC_T), VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H265D_LB_FE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H265D_LB_FE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H265D_LB_FE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_H265D_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_H265D_LB_SE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H265D_LB_PE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H265D_LB_VSP_TOP(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H265D_LB_VSP_LEFT(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_H265D_LB_RECON_DMA_METADATA_WR(width, height),
			VENUS_DMA_ALIGNMENT) * 4 +
		ALIGN(SIZE_H265D_QP(width, height), VENUS_DMA_ALIGNMENT);
	size = ALIGN(size, VENUS_DMA_ALIGNMENT);
	return size;
}

static inline u32 size_h265d_hw_bin_buffer(u32 width, u32 height, u32 delay,
	u32 num_vpp_pipes)
{
	u32 size = 0;
	u32 size_yuv, size_bin_hdr, size_bin_res;
	u32 product;

	product = width * height;
	size_yuv = (product <= BIN_BUFFER_THRESHOLD) ?
		((BIN_BUFFER_THRESHOLD * 3) >> 1) :
		((product * 3) >> 1);
	size_bin_hdr = size_yuv * H265_CABAC_HDR_RATIO_HD_TOT;
	size_bin_res = size_yuv * H265_CABAC_RES_RATIO_HD_TOT;
	size_bin_hdr = size_bin_hdr * (((((u32)(delay)) & 31) / 10) + 2) / 2;
	size_bin_res = size_bin_res * (((((u32)(delay)) & 31) / 10) + 2) / 2;
	size_bin_hdr = ALIGN(size_bin_hdr / num_vpp_pipes,
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes;
	size_bin_res = ALIGN(size_bin_res / num_vpp_pipes,
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes;
	size = size_bin_hdr + size_bin_res;

	return size;
}

static inline u32 calculate_h265d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced, u32 delay, u32 num_vpp_pipes)
{
	u32 aligned_width = ALIGN(width, BUFFER_ALIGNMENT_SIZE(16));
	u32 aligned_height = ALIGN(height, BUFFER_ALIGNMENT_SIZE(16));
	u32 size = 0;

	if (!is_interlaced)
		size = size_h265d_hw_bin_buffer(aligned_width, aligned_height,
						delay, num_vpp_pipes);
	else
		size = 0;

	return size;
}

static inline u32 calculate_vpxd_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced, u32 delay, u32 num_vpp_pipes)
{
	u32 aligned_width = ALIGN(width, BUFFER_ALIGNMENT_SIZE(16));
	u32 aligned_height = ALIGN(height, BUFFER_ALIGNMENT_SIZE(16));
	u32 size = 0;
	u32 size_yuv = aligned_width * aligned_height * 3 / 2;

	if (!is_interlaced) {
		/* binbuffer1_size + binbufer2_size */
		u32 binbuffer1_size = 0, binbuffer2_size = 0;

		binbuffer1_size = ALIGN(max_t(u32, size_yuv,
			((BIN_BUFFER_THRESHOLD * 3) >> 1)) *
			VPX_DECODER_FRAME_CONCURENCY_LVL *
			VPX_DECODER_FRAME_BIN_HDR_BUDGET_RATIO_NUM /
			VPX_DECODER_FRAME_BIN_HDR_BUDGET_RATIO_DEN /
			num_vpp_pipes,
			VENUS_DMA_ALIGNMENT);
		binbuffer2_size = ALIGN(max_t(u32, size_yuv,
			((BIN_BUFFER_THRESHOLD * 3) >> 1)) *
			VPX_DECODER_FRAME_CONCURENCY_LVL *
			VPX_DECODER_FRAME_BIN_RES_BUDGET_RATIO_NUM /
			VPX_DECODER_FRAME_BIN_RES_BUDGET_RATIO_DEN /
			num_vpp_pipes,
			VENUS_DMA_ALIGNMENT);
		size = binbuffer1_size + binbuffer2_size;
		size = size * num_vpp_pipes;
	} else {
		size = 0;
	}

	return size;
}

static inline u32 calculate_mpeg2d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced, u32 delay, u32 num_vpp_pipes)
{
	return 0;
}

static inline u32 calculate_enc_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 lcu_size, u32 num_vpp_pipes)
{
	u32 aligned_width, aligned_height, bitstream_size;
	u32 total_bitbin_buffers = 0, size_singlePipe, bitbin_size = 0;
	u32 sao_bin_buffer_size, padded_bin_size, size = 0;

	aligned_width = ALIGN(width, lcu_size);
	aligned_height = ALIGN(height, lcu_size);
	bitstream_size = msm_vidc_calculate_enc_output_frame_size(inst);

	bitstream_size = ALIGN(bitstream_size, VENUS_DMA_ALIGNMENT);
	if (work_mode == HFI_WORKMODE_2) {
		total_bitbin_buffers = 3;
		bitbin_size = bitstream_size * 17 / 10;
		bitbin_size = ALIGN(bitbin_size, VENUS_DMA_ALIGNMENT);
	} else {
		total_bitbin_buffers = 1;
		bitstream_size = aligned_width * aligned_height * 3;
		bitbin_size = ALIGN(bitstream_size, VENUS_DMA_ALIGNMENT);
	}
	if (aligned_width * aligned_height >= 3840 * 2160)
		size_singlePipe = bitbin_size / 4;
	else if (num_vpp_pipes > 2)
		size_singlePipe = bitbin_size / 2;
	else
		size_singlePipe = bitbin_size;
	if (inst->rc_type == RATE_CONTROL_LOSSLESS)
		size_singlePipe <<= 1;
	size_singlePipe = ALIGN(size_singlePipe, VENUS_DMA_ALIGNMENT);
	sao_bin_buffer_size = (64 * (((width + BUFFER_ALIGNMENT_SIZE(32)) *
		(height + BUFFER_ALIGNMENT_SIZE(32))) >> 10)) + 384;
	padded_bin_size = ALIGN(size_singlePipe, VENUS_DMA_ALIGNMENT);
	size_singlePipe = sao_bin_buffer_size + padded_bin_size;
	size_singlePipe = ALIGN(size_singlePipe, VENUS_DMA_ALIGNMENT);
	bitbin_size = size_singlePipe * num_vpp_pipes;
	size = ALIGN(bitbin_size, VENUS_DMA_ALIGNMENT) * total_bitbin_buffers
			+ 512;

	return size;
}

static inline u32 calculate_h264e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 num_vpp_pipes)
{
	return calculate_enc_scratch_size(inst, width, height, work_mode, 16,
		num_vpp_pipes);
}

static inline u32 calculate_h265e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 num_vpp_pipes)
{
	return calculate_enc_scratch_size(inst, width, height, work_mode, 32,
		num_vpp_pipes);
}

static inline u32 calculate_vp8e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 num_vpp_pipes)
{
	return calculate_enc_scratch_size(inst, width, height, work_mode, 16,
		num_vpp_pipes);
}

static inline u32 calculate_h264d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes)
{
	u32 co_mv_size = 0, nonco_mv_size = 0;
	u32 vpss_lb_size = 0;
	u32 size = 0;

	co_mv_size = hfi_iris2_h264d_comv_size(width, height, min_buf_count);
	nonco_mv_size = hfi_iris2_h264d_non_comv_size(width, height,
			num_vpp_pipes);
	if (split_mode_enabled)
		vpss_lb_size = size_vpss_lb(width, height, num_vpp_pipes);
	size = co_mv_size + nonco_mv_size + vpss_lb_size;
	return size;
}

static inline u32 calculate_h265d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes)
{
	u32 co_mv_size = 0, nonco_mv_size = 0;
	u32 vpss_lb_size = 0;
	u32 size = 0;

	co_mv_size = hfi_iris2_h265d_comv_size(width, height, min_buf_count);
	nonco_mv_size =
		hfi_iris2_h265d_non_comv_size(width, height, num_vpp_pipes);
	if (split_mode_enabled)
		vpss_lb_size = size_vpss_lb(width, height, num_vpp_pipes);

	size = co_mv_size + nonco_mv_size + vpss_lb_size +
			HDR10_HIST_EXTRADATA_SIZE;
	return size;
}

static inline u32 hfi_iris2_vp8d_comv_size(u32 width, u32 height,
	u32 yuv_min_buf_count)
{
	return (((width + 15) >> 4) * ((height + 15) >> 4) * 8 * 2);
}

static inline u32 calculate_vp8d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes)
{
	u32 vpss_lb_size = 0;
	u32 size = 0;

	size = hfi_iris2_vp8d_comv_size(width, height, 0);
	size += ALIGN(SIZE_VPXD_LB_FE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_VPXD_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_VP8D_LB_VSP_TOP(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VPXD_LB_FE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		2 * ALIGN(SIZE_VPXD_LB_RECON_DMA_METADATA_WR(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VPXD_LB_SE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VP8D_LB_PE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VP8D_LB_FE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT);
	if (split_mode_enabled)
		vpss_lb_size = size_vpss_lb(width, height, num_vpp_pipes);

	size += vpss_lb_size;
	return size;
}

static inline u32 calculate_vp9d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes)
{
	u32 vpss_lb_size = 0;
	u32 size = 0;

	size = ALIGN(SIZE_VPXD_LB_FE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_VPXD_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_VP9D_LB_VSP_TOP(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VPXD_LB_FE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		2 * ALIGN(SIZE_VPXD_LB_RECON_DMA_METADATA_WR(width, height),
				VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VPXD_LB_SE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VP9D_LB_PE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VP9D_LB_FE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT);
	if (split_mode_enabled)
		vpss_lb_size = size_vpss_lb(width, height, num_vpp_pipes);

	size += vpss_lb_size + HDR10_HIST_EXTRADATA_SIZE;
	return size;
}

static inline u32 calculate_mpeg2d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled,
	u32 num_vpp_pipes)
{
	u32 vpss_lb_size = 0;
	u32 size = 0;

	size = ALIGN(SIZE_VPXD_LB_FE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_VPXD_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes +
		ALIGN(SIZE_VP8D_LB_VSP_TOP(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VPXD_LB_FE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		2 * ALIGN(SIZE_VPXD_LB_RECON_DMA_METADATA_WR(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VPXD_LB_SE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VP8D_LB_PE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_VP8D_LB_FE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT);
	if (split_mode_enabled)
		vpss_lb_size = size_vpss_lb(width, height, num_vpp_pipes);

	size += vpss_lb_size;
	return size;
}

static inline u32 calculate_enc_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 lcu_size, u32 num_ref, bool ten_bit,
	u32 num_vpp_pipes, bool is_h265)
{
	u32 line_buf_ctrl_size, line_buf_data_size, leftline_buf_ctrl_size;
	u32 line_buf_sde_size, sps_pps_slice_hdr, topline_buf_ctrl_size_FE;
	u32 leftline_buf_ctrl_size_FE, line_buf_recon_pix_size;
	u32 leftline_buf_recon_pix_size, lambda_lut_size, override_buffer_size;
	u32 col_mv_buf_size, vpp_reg_buffer_size, ir_buffer_size;
	u32 vpss_line_buf, leftline_buf_meta_recony, col_rc_buf_size;
	u32 h265e_framerc_bufsize, h265e_lcubitcnt_bufsize;
	u32 h265e_lcubitmap_bufsize, se_stats_bufsize;
	u32 bse_reg_buffer_size, bse_slice_cmd_buffer_size, slice_info_bufsize;
	u32 line_buf_ctrl_size_buffid2, slice_cmd_buffer_size;
	u32 width_lcu_num, height_lcu_num, width_coded, height_coded;
	u32 frame_num_lcu, linebuf_meta_recon_uv, topline_bufsize_fe_1stg_sao;
	u32 output_mv_bufsize = 0, temp_scratch_mv_bufsize = 0;
	u32 size, bit_depth, num_LCUMB;
	u32 vpss_lineBufferSize_1 = 0;
	u32 width_mb_num = ((width + 15) >> 4);
	u32 height_mb_num = ((height + 15) >> 4);

	width_lcu_num = ((width)+(lcu_size)-1) / (lcu_size);
	height_lcu_num = ((height)+(lcu_size)-1) / (lcu_size);
	frame_num_lcu = width_lcu_num * height_lcu_num;
	width_coded = width_lcu_num * (lcu_size);
	height_coded = height_lcu_num * (lcu_size);
	num_LCUMB = (height_coded / lcu_size) * ((width_coded + lcu_size * 8) / lcu_size);
	slice_info_bufsize = (256 + (frame_num_lcu << 4));
	slice_info_bufsize = ALIGN(slice_info_bufsize, VENUS_DMA_ALIGNMENT);
	line_buf_ctrl_size = ALIGN(width_coded, VENUS_DMA_ALIGNMENT);
	line_buf_ctrl_size_buffid2 = ALIGN(width_coded, VENUS_DMA_ALIGNMENT);

	bit_depth = ten_bit ? 10 : 8;
	line_buf_data_size = (((((bit_depth * width_coded + 1024) +
	(VENUS_DMA_ALIGNMENT - 1)) & (~(VENUS_DMA_ALIGNMENT - 1))) * 1) +
	(((((bit_depth * width_coded + 1024) >> 1) +
	(VENUS_DMA_ALIGNMENT - 1)) &
	(~(VENUS_DMA_ALIGNMENT - 1))) * 2));
	leftline_buf_ctrl_size = (is_h265) ?
		((height_coded + (BUFFER_ALIGNMENT_SIZE(32))) /
		BUFFER_ALIGNMENT_SIZE(32) * 4 * 16) :
		((height_coded + 15) / 16 * 5 * 16);
	if (num_vpp_pipes > 1) {
		leftline_buf_ctrl_size += BUFFER_ALIGNMENT_SIZE(512);
		leftline_buf_ctrl_size = ALIGN(leftline_buf_ctrl_size,
			BUFFER_ALIGNMENT_SIZE(512)) * num_vpp_pipes;
	}
	leftline_buf_ctrl_size = ALIGN(leftline_buf_ctrl_size,
		VENUS_DMA_ALIGNMENT);
	leftline_buf_recon_pix_size = (((ten_bit + 1) * 2 *
		(height_coded)+VENUS_DMA_ALIGNMENT) +
	(VENUS_DMA_ALIGNMENT << (num_vpp_pipes - 1)) - 1) &
		(~((VENUS_DMA_ALIGNMENT << (num_vpp_pipes - 1)) - 1)) * 1;
	topline_buf_ctrl_size_FE = (is_h265) ? (64 * (width_coded >> 5)) :
		(VENUS_DMA_ALIGNMENT + 16 * (width_coded >> 4));
	topline_buf_ctrl_size_FE = ALIGN(topline_buf_ctrl_size_FE,
		VENUS_DMA_ALIGNMENT);
	leftline_buf_ctrl_size_FE = (((VENUS_DMA_ALIGNMENT + 64 *
		(height_coded >> 4)) +
		(VENUS_DMA_ALIGNMENT << (num_vpp_pipes - 1)) - 1) &
		(~((VENUS_DMA_ALIGNMENT << (num_vpp_pipes - 1)) - 1)) * 1) *
		num_vpp_pipes;
	leftline_buf_meta_recony = (VENUS_DMA_ALIGNMENT + 64 *
		((height_coded) / (8 * (ten_bit ? 4 : 8))));
	leftline_buf_meta_recony = ALIGN(leftline_buf_meta_recony,
		VENUS_DMA_ALIGNMENT);
	leftline_buf_meta_recony = leftline_buf_meta_recony *
		num_vpp_pipes;
	linebuf_meta_recon_uv = (VENUS_DMA_ALIGNMENT + 64 *
		((height_coded) / (4 * (ten_bit ? 4 : 8))));
	linebuf_meta_recon_uv = ALIGN(linebuf_meta_recon_uv,
		VENUS_DMA_ALIGNMENT);
	linebuf_meta_recon_uv = linebuf_meta_recon_uv *
		num_vpp_pipes;
	line_buf_recon_pix_size = ((ten_bit ? 3 : 2) * width_coded);
	line_buf_recon_pix_size = ALIGN(line_buf_recon_pix_size,
		VENUS_DMA_ALIGNMENT);
	slice_cmd_buffer_size = ALIGN(20480, VENUS_DMA_ALIGNMENT);
	sps_pps_slice_hdr = 2048 + 4096;
	col_mv_buf_size = (is_h265) ? (16 * ((frame_num_lcu << 2) +
		BUFFER_ALIGNMENT_SIZE(32))) :
		(3 * 16 * (width_lcu_num * height_lcu_num +
		BUFFER_ALIGNMENT_SIZE(32)));
	col_mv_buf_size = ALIGN(col_mv_buf_size, VENUS_DMA_ALIGNMENT)
		* (num_ref + 1);
	col_rc_buf_size = (((width_mb_num + 7) >> 3) *
		16 * 2 * height_mb_num);
	col_rc_buf_size = ALIGN(col_rc_buf_size,
		VENUS_DMA_ALIGNMENT) * HFI_MAX_COL_FRAME;
	h265e_framerc_bufsize = (is_h265) ? (256 + 16 *
		(14 + (((height_coded >> 5) + 7) >> 3))) :
		(256 + 16 * (14 + (((height_coded >> 4) + 7) >> 3)));
	h265e_framerc_bufsize *= 6;   /* multiply by max numtilescol*/
	if (num_vpp_pipes > 1)
		h265e_framerc_bufsize = ALIGN(h265e_framerc_bufsize,
			VENUS_DMA_ALIGNMENT) * num_vpp_pipes;

	h265e_framerc_bufsize = ALIGN(h265e_framerc_bufsize,
		BUFFER_ALIGNMENT_SIZE(512)) * HFI_MAX_COL_FRAME;
	h265e_lcubitcnt_bufsize = (256 + 4 * frame_num_lcu);
	h265e_lcubitcnt_bufsize = ALIGN(h265e_lcubitcnt_bufsize,
		VENUS_DMA_ALIGNMENT);
	h265e_lcubitmap_bufsize = 256 + (frame_num_lcu >> 3);
	h265e_lcubitmap_bufsize = ALIGN(h265e_lcubitmap_bufsize,
		VENUS_DMA_ALIGNMENT);
	line_buf_sde_size = 256 + 16 * (width_coded >> 4);
	line_buf_sde_size = ALIGN(line_buf_sde_size, VENUS_DMA_ALIGNMENT);
	if ((width_coded * height_coded) > (4096 * 2160))
		se_stats_bufsize = 0;
	else if ((width_coded * height_coded) > (1920 * 1088))
		se_stats_bufsize = (40 * 4 * frame_num_lcu + 256 + 256);
	else
		se_stats_bufsize = (1024 * frame_num_lcu + 256 + 256);

	se_stats_bufsize = ALIGN(se_stats_bufsize, VENUS_DMA_ALIGNMENT) * 2;
	bse_slice_cmd_buffer_size = ((((8192 << 2) + 7) & (~7)) * 6);
	bse_reg_buffer_size = ((((512 << 3) + 7) & (~7)) * 4);
	vpp_reg_buffer_size = ((((HFI_VENUS_VPPSG_MAX_REGISTERS << 3) + 31) &
		(~31)) * 10);
	lambda_lut_size = (256 * 11);
	override_buffer_size = 16 * ((num_LCUMB + 7) >> 3);
	override_buffer_size = ALIGN(override_buffer_size,
		VENUS_DMA_ALIGNMENT) * 2;
	ir_buffer_size = (((frame_num_lcu << 1) + 7) & (~7)) * 3;
	vpss_lineBufferSize_1 = ((((8192) >> 2) << 5) * num_vpp_pipes) + 64;
	vpss_line_buf = (((((max(width_coded, height_coded) + 3) >> 2) << 5)
		 + 256) * 16) + vpss_lineBufferSize_1;
	topline_bufsize_fe_1stg_sao = (16 * (width_coded >> 5));
	topline_bufsize_fe_1stg_sao = ALIGN(topline_bufsize_fe_1stg_sao,
		VENUS_DMA_ALIGNMENT);
	size = line_buf_ctrl_size + line_buf_data_size +
		line_buf_ctrl_size_buffid2 + leftline_buf_ctrl_size +
		vpss_line_buf + col_mv_buf_size + topline_buf_ctrl_size_FE +
		leftline_buf_ctrl_size_FE + line_buf_recon_pix_size +
		leftline_buf_recon_pix_size + leftline_buf_meta_recony +
		linebuf_meta_recon_uv + col_rc_buf_size +
		h265e_framerc_bufsize + h265e_lcubitcnt_bufsize +
		h265e_lcubitmap_bufsize + line_buf_sde_size +
		topline_bufsize_fe_1stg_sao + override_buffer_size +
		bse_reg_buffer_size + vpp_reg_buffer_size +
		sps_pps_slice_hdr + slice_cmd_buffer_size +
		bse_slice_cmd_buffer_size + ir_buffer_size + slice_info_bufsize
		+ lambda_lut_size + se_stats_bufsize + temp_scratch_mv_bufsize
		+ output_mv_bufsize + 1024;
	return size;
}

static inline u32 calculate_h264e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit, u32 num_vpp_pipes)
{
	return calculate_enc_scratch1_size(inst, width, height, 16,
		num_ref, ten_bit, num_vpp_pipes, false);
}

static inline u32 calculate_h265e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit, u32 num_vpp_pipes)
{
	return calculate_enc_scratch1_size(inst, width, height, 32,
		num_ref, ten_bit, num_vpp_pipes, true);
}

static inline u32 calculate_vp8e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit, u32 num_vpp_pipes)
{
	(void)num_vpp_pipes;
	return calculate_enc_scratch1_size(inst, width, height, 16,
		num_ref, ten_bit, 1, false);
}

static inline u32 hfi_ubwc_calc_metadata_plane_stride(u32 width,
	u32 metadata_stride_multi, u32 tile_width_pels)
{
	return ALIGN(((width + (tile_width_pels - 1)) / tile_width_pels),
		metadata_stride_multi);
}

static inline u32 hfi_ubwc_metadata_plane_bufheight(u32 height,
	u32 metadata_height_multi, u32 tile_height_pels)
{
	return ALIGN(((height + (tile_height_pels - 1)) / tile_height_pels),
		metadata_height_multi);
}

static inline u32 hfi_ubwc_metadata_plane_buffer_size(u32 metadata_stride,
	u32 metadata_buf_height)
{
	return ALIGN(metadata_stride * metadata_buf_height,
		BUFFER_ALIGNMENT_4096_BYTES);
}

static inline u32 hfi_ubwc_uv_metadata_plane_stride(u32 width,
	u32 metadata_stride_multi, u32 tile_width_pels)
{
	return ALIGN(((((width + 1) >> 1) + (tile_width_pels - 1)) /
		tile_width_pels), metadata_stride_multi);
}

static inline u32 hfi_ubwc_uv_metadata_plane_bufheight(u32 height,
	u32 metadata_height_multi, u32 tile_height_pels)
{
	return ALIGN(((((height + 1) >> 1) + (tile_height_pels - 1)) /
		tile_height_pels), metadata_height_multi);
}

static inline u32 hfi_iris2_enc_dpb_buffer_size(u32 width, u32 height,
	bool ten_bit)
{
	u32 aligned_width, aligned_height, chroma_height, ref_buf_height;
	u32 luma_size, chroma_size;
	u32 metadata_stride, meta_buf_height, meta_size_y, meta_size_c;
	u32 ref_luma_stride_bytes, ref_chroma_height_bytes;
	u32 ref_buf_size = 0, ref_stride;
	u32 size;

	if (!ten_bit) {
		aligned_height = ALIGN(height, HFI_VENUS_HEIGHT_ALIGNMENT);
		chroma_height = height >> 1;
		chroma_height = ALIGN(chroma_height,
			HFI_VENUS_HEIGHT_ALIGNMENT);
		aligned_width = ALIGN(width, HFI_VENUS_WIDTH_ALIGNMENT);
		metadata_stride = hfi_ubwc_calc_metadata_plane_stride(width,
			64, HFI_COLOR_FORMAT_YUV420_NV12_UBWC_Y_TILE_WIDTH);
		meta_buf_height = hfi_ubwc_metadata_plane_bufheight(height,
			16, HFI_COLOR_FORMAT_YUV420_NV12_UBWC_Y_TILE_HEIGHT);
		meta_size_y = hfi_ubwc_metadata_plane_buffer_size(
			metadata_stride, meta_buf_height);
		meta_size_c = hfi_ubwc_metadata_plane_buffer_size(
			metadata_stride, meta_buf_height);
		size = (aligned_height + chroma_height) * aligned_width +
			meta_size_y + meta_size_c;
	} else {
		ref_buf_height = (height + (HFI_VENUS_HEIGHT_ALIGNMENT - 1))
			& (~(HFI_VENUS_HEIGHT_ALIGNMENT - 1));
		ref_luma_stride_bytes = ((width + SYSTEM_LAL_TILE10 - 1) /
			SYSTEM_LAL_TILE10) * SYSTEM_LAL_TILE10;
		ref_stride = 4 * (ref_luma_stride_bytes / 3);
		ref_stride = (ref_stride + (BUFFER_ALIGNMENT_SIZE(128) - 1)) &
			(~(BUFFER_ALIGNMENT_SIZE(128) - 1));
		luma_size = ref_buf_height * ref_stride;
		ref_chroma_height_bytes = (((height + 1) >> 1) +
			(BUFFER_ALIGNMENT_SIZE(32) - 1)) &
			(~(BUFFER_ALIGNMENT_SIZE(32) - 1));
		chroma_size = ref_stride * ref_chroma_height_bytes;
		luma_size = (luma_size + (BUFFER_ALIGNMENT_4096_BYTES - 1)) &
			(~(BUFFER_ALIGNMENT_4096_BYTES - 1));
		chroma_size = (chroma_size +
			(BUFFER_ALIGNMENT_4096_BYTES - 1)) &
			(~(BUFFER_ALIGNMENT_4096_BYTES - 1));
		ref_buf_size = luma_size + chroma_size;
		metadata_stride = hfi_ubwc_calc_metadata_plane_stride(
			width,
			VENUS_METADATA_STRIDE_MULTIPLE,
			HFI_COLOR_FORMAT_YUV420_TP10_UBWC_Y_TILE_WIDTH);
		meta_buf_height = hfi_ubwc_metadata_plane_bufheight(
			height,
			VENUS_METADATA_HEIGHT_MULTIPLE,
			HFI_COLOR_FORMAT_YUV420_TP10_UBWC_Y_TILE_HEIGHT);
		meta_size_y = hfi_ubwc_metadata_plane_buffer_size(
			metadata_stride, meta_buf_height);
		meta_size_c = hfi_ubwc_metadata_plane_buffer_size(
			metadata_stride, meta_buf_height);
		size = ref_buf_size + meta_size_y + meta_size_c;
	}
	return size;
}

static inline u32 calculate_enc_scratch2_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit, bool downscale,
	u32 rotation_val, u32 flip)
{
	u32 size;

	size = hfi_iris2_enc_dpb_buffer_size(width, height, ten_bit);
	size = size * (num_ref + 1) + 4096;
	if (downscale && (rotation_val || flip)) {
	/* VPSS output is always 128 x 32 (8-bit) or 192 x 16 (10-bit) aligned */
		if (rotation_val == 90 || rotation_val == 270)
			size += hfi_iris2_enc_dpb_buffer_size(height, width, ten_bit);
		else
			size += hfi_iris2_enc_dpb_buffer_size(width, height, ten_bit);
		size += 4096;
	}
	return size;
}

static inline u32 calculate_enc_persist_size(void)
{
	return HFI_IRIS2_ENC_PERSIST_SIZE;
}

static inline u32 calculate_h264d_persist1_size(void)
{
	u32 size = 0;

	size = ALIGN((SIZE_SLIST_BUF_H264 * NUM_SLIST_BUF_H264
			+ NUM_HW_PIC_BUF * SIZE_SEI_USERDATA),
			VENUS_DMA_ALIGNMENT);
	return size;
}

static inline u32 calculate_h265d_persist1_size(void)
{
	u32 size = 0;

	size = ALIGN((SIZE_SLIST_BUF_H265 * NUM_SLIST_BUF_H265 + H265_NUM_TILE
			* sizeof(u32) + NUM_HW_PIC_BUF * SIZE_SEI_USERDATA),
			VENUS_DMA_ALIGNMENT);
	return size;
}

static inline u32 calculate_vp8d_persist1_size(void)
{
	u32 size = 0;

	size = ALIGN(VP8_NUM_PROBABILITY_TABLE_BUF * VP8_PROB_TABLE_SIZE,
			VENUS_DMA_ALIGNMENT);
	return size;
}

static inline u32 calculate_vp9d_persist1_size(void)
{
	u32 size = 0;

	size = ALIGN(VP9_NUM_PROBABILITY_TABLE_BUF * VP9_PROB_TABLE_SIZE,
			VENUS_DMA_ALIGNMENT) +
			ALIGN(HFI_IRIS2_VP9D_COMV_SIZE, VENUS_DMA_ALIGNMENT) +
			ALIGN(MAX_SUPERFRAME_HEADER_LEN, VENUS_DMA_ALIGNMENT) +
			ALIGN(VP9_UDC_HEADER_BUF_SIZE, VENUS_DMA_ALIGNMENT) +
			ALIGN(VP9_NUM_FRAME_INFO_BUF * CCE_TILE_OFFSET_SIZE,
			VENUS_DMA_ALIGNMENT);
	return size;
}

static inline u32 calculate_mpeg2d_persist1_size(void)
{
	return QMATRIX_SIZE + MP2D_QPDUMP_SIZE;
}
