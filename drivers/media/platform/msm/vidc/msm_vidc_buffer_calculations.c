// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2019, The Linux Foundation. All rights reserved.
 */

#include "msm_vidc_debug.h"
#include "msm_vidc_common.h"
#include "msm_vidc_buffer_calculations.h"

#define MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS MIN_NUM_OUTPUT_BUFFERS
#define MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS MIN_NUM_CAPTURE_BUFFERS
#define MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS_VP9 8

/* extra o/p buffers in case of encoder dcvs */
#define DCVS_ENC_EXTRA_INPUT_BUFFERS 4

/* extra o/p buffers in case of decoder dcvs */
#define DCVS_DEC_EXTRA_OUTPUT_BUFFERS 4

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

#define NUM_OF_VPP_PIPES 4
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

#define NUM_HW_PIC_BUF 10
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
	(ALIGN(height, 8) * 32)

#define SIZE_H264D_QP(width, height) \
	(((width + 63) >> 6) * ((height + 63) >> 6) * 128)

#define SIZE_HW_PIC(sizePerBuf) \
	(NUM_HW_PIC_BUF * sizePerBuf)

#define H264_CABAC_HDR_RATIO_HD_TOT_NUM 1  /* 0.25 */
#define H264_CABAC_HDR_RATIO_HD_TOT_DEN 4
#define H264_CABAC_RES_RATIO_HD_TOT_NUM 3  /* 0.75 */
#define H264_CABAC_RES_RATIO_HD_TOT_DEN 4
/*
 * some content need more bin buffer, but limit buffer
 * size for high resolution
 */


#define NUM_SLIST_BUF_H264            (256 + 32)
#define SIZE_SLIST_BUF_H264           512

#define LCU_MAX_SIZE_PELS 64
#define LCU_MIN_SIZE_PELS 16

#define H265D_MAX_SLICE 600
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

#define H265_CABAC_HDR_RATIO_HD_TOT_NUM 1
#define H265_CABAC_HDR_RATIO_HD_TOT_DEN 2
#define H265_CABAC_RES_RATIO_HD_TOT_NUM 1
#define H265_CABAC_RES_RATIO_HD_TOT_DEN 2
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
	ALIGN((ALIGN(height, 8) / (4 / 2)) * 64, BUFFER_ALIGNMENT_SIZE(32))
#define SIZE_VP8D_LB_FE_TOP_DATA(width, height) \
	((ALIGN(width, 16) + 8) * 10 * 2)
#define SIZE_VP9D_LB_FE_TOP_DATA(width, height) \
	((ALIGN(ALIGN(width, 8), 64) + 8) * 10 * 2)
#define SIZE_VP8D_LB_PE_TOP_DATA(width, height) \
	((ALIGN(width, 16) >> 4) * 64)
#define SIZE_VP9D_LB_PE_TOP_DATA(width, height) \
	((ALIGN(ALIGN(width, 8), 64) >> 6) * 176)
#define SIZE_VP8D_LB_VSP_TOP(width, height) \
	(((ALIGN(width, 16) >> 4) * 64 / 2) + 256)
#define SIZE_VP9D_LB_VSP_TOP(width, height) \
	(((ALIGN(ALIGN(width, 8), 64) >> 6) * 64 * 8) + 256)


#define HFI_IRIS2_VP9D_COMV_SIZE \
	((((8192 + 63) >> 6) * ((4320 + 63) >> 6) * 8 * 8 * 2 * 8))

#define VPX_DECODER_FRAME_CONCURENCY_LVL 2
#define VPX_DECODER_FRAME_BIN_HDR_BUDGET_RATIO_NUM 1
#define VPX_DECODER_FRAME_BIN_HDR_BUDGET_RATIO_DEN 2
#define VPX_DECODER_FRAME_BIN_RES_BUDGET_RATIO_NUM 3
#define VPX_DECODER_FRAME_BIN_RES_BUDGET_RATIO_DEN 2

#define VP8_NUM_FRAME_INFO_BUF (5 + 1)
#define VP9_NUM_FRAME_INFO_BUF (8 + 2 + 1 + 8)
#define VP8_NUM_PROBABILITY_TABLE_BUF (VP8_NUM_FRAME_INFO_BUF)
#define VP9_NUM_PROBABILITY_TABLE_BUF (VP9_NUM_FRAME_INFO_BUF + 4)
#define VP8_PROB_TABLE_SIZE 3840
#define VP9_PROB_TABLE_SIZE 3840

#define VP9_UDC_HEADER_BUF_SIZE (3 * 128)
#define MAX_SUPERFRAME_HEADER_LEN (34)
#define CCE_TILE_OFFSET_SIZE ALIGN(32 * 4 * 4, BUFFER_ALIGNMENT_SIZE(32))

#define QMATRIX_SIZE (sizeof(u32) * 128 + 256)
#define MP2D_QPDUMP_SIZE 115200

#define HFI_IRIS2_ENC_PERSIST_SIZE 102400

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

static inline u32 calculate_h264d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced);
static inline u32 calculate_h265d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced);
static inline u32 calculate_vpxd_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced);
static inline u32 calculate_mpeg2d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced);

static inline u32 calculate_enc_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 lcu_size);
static inline u32 calculate_h264e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode);
static inline u32 calculate_h265e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode);
static inline u32 calculate_vp8e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode);

static inline u32 calculate_h264d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled);
static inline u32 calculate_h265d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled);
static inline u32 calculate_vp8d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled);
static inline u32 calculate_vp9d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled);
static inline u32 calculate_mpeg2d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled);

static inline u32 calculate_h264e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit,
	u32 num_vpp_pipes);
static inline u32 calculate_h265e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit,
	u32 num_vpp_pipes);
static inline u32 calculate_vp8e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit,
	u32 num_vpp_pipes);

static inline u32 calculate_enc_scratch2_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit);

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
	u32 width, height, i, out_min_count;

	if (!inst) {
		dprintk(VIDC_ERR, "Instance is null!");
		return -EINVAL;
	}

	switch (inst->fmts[OUTPUT_PORT].fourcc) {
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
		dprintk(VIDC_ERR,
			"Invalid pix format. Internal buffer cal not defined : %x ",
			inst->fmts[OUTPUT_PORT].fourcc);
		return -EINVAL;
	}

	width = inst->prop.width[OUTPUT_PORT];
	height = inst->prop.height[OUTPUT_PORT];
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
					inst, width, height, is_interlaced);
			valid_buffer_type = true;
		} else  if (curr_req->buffer_type ==
			HAL_BUFFER_INTERNAL_SCRATCH_1) {
			struct hal_buffer_requirements *out_buff;

			out_buff = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
			if (!out_buff)
				return -EINVAL;
			out_min_count = out_buff->buffer_count_min;
			curr_req->buffer_size =
				dec_calculators->calculate_scratch1_size(
					inst, width, height, out_min_count,
					is_secondary_output_mode(inst));
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
	int num_bframes = -1, ltr_count = -1, num_hp_layers;
	struct v4l2_ctrl *bframe_ctrl;
	struct v4l2_ctrl *ltr_ctrl;
	struct v4l2_ctrl *layer_ctrl;

	bframe_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
	num_bframes = bframe_ctrl->val;
	if (num_bframes > 0)
		num_ref = num_bframes + 1;

	ltr_ctrl = get_ctrl(inst, V4L2_CID_MPEG_VIDC_VIDEO_LTRCOUNT);
	ltr_count = ltr_ctrl->val;
	/* B and LTR can't be at same time */
	if (ltr_count > 0)
		num_ref = num_ref + ltr_count;

	layer_ctrl = get_ctrl(inst,
		V4L2_CID_MPEG_VIDEO_HEVC_HIER_CODING_LAYER);
	num_hp_layers = layer_ctrl->val;
	if (num_hp_layers > 0) {
		/* LTR and B - frame not supported with hybrid HP */
		if (inst->hybrid_hp)
			num_ref = (num_hp_layers - 1);
		else if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC)
			num_ref = ((num_hp_layers + 1) / 2) + ltr_count;
		else if ((inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_H264)
				&& (num_hp_layers <= 4))
			num_ref = ((1 << (num_hp_layers - 1)) - 1) + ltr_count;
		else
			num_ref = ((num_hp_layers + 1) / 2) + ltr_count;
	}
	return num_ref;
}

int msm_vidc_get_encoder_internal_buffer_sizes(struct msm_vidc_inst *inst)
{
	struct msm_vidc_enc_buff_size_calculators *enc_calculators;
	u32 width, height, i, num_ref;
	bool is_tenbit = false;
	int num_bframes;
	u32 inp_fmt;
	struct v4l2_ctrl *bframe;

	if (!inst) {
		dprintk(VIDC_ERR, "Instance is null!");
		return -EINVAL;
	}

	switch (inst->fmts[CAPTURE_PORT].fourcc) {
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
		dprintk(VIDC_ERR,
			"Invalid pix format. Internal buffer cal not defined : %x ",
			inst->fmts[CAPTURE_PORT].fourcc);
		return -EINVAL;
	}

	width = inst->prop.width[OUTPUT_PORT];
	height = inst->prop.height[OUTPUT_PORT];
	bframe = get_ctrl(inst, V4L2_CID_MPEG_VIDEO_B_FRAMES);
	num_bframes = bframe->val;
	if (num_bframes < 0) {
		dprintk(VIDC_ERR,
			"%s: get num bframe failed\n", __func__);
		return -EINVAL;
	}

	num_ref = msm_vidc_get_num_ref_frames(inst);
	inp_fmt = inst->fmts[OUTPUT_PORT].fourcc;
	if ((inp_fmt == V4L2_PIX_FMT_NV12_TP10_UBWC) ||
		(inp_fmt == V4L2_PIX_FMT_SDE_Y_CBCR_H2V2_P010_VENUS))
		is_tenbit = true;

	for (i = 0; i < HAL_BUFFER_MAX; i++) {
		struct hal_buffer_requirements *curr_req;
		bool valid_buffer_type = false;

		curr_req = &inst->buff_req.buffer[i];
		if (curr_req->buffer_type == HAL_BUFFER_INTERNAL_SCRATCH) {
			curr_req->buffer_size =
				enc_calculators->calculate_scratch_size(
					inst, width, height,
					inst->clk_data.work_mode);
			valid_buffer_type = true;
		} else  if (curr_req->buffer_type ==
			HAL_BUFFER_INTERNAL_SCRATCH_1) {
			curr_req->buffer_size =
				enc_calculators->calculate_scratch1_size(
					inst, width, height, num_ref,
					is_tenbit,
					inst->clk_data.work_route);
			valid_buffer_type = true;
		} else if (curr_req->buffer_type ==
			HAL_BUFFER_INTERNAL_SCRATCH_2) {
			curr_req->buffer_size =
				enc_calculators->calculate_scratch2_size(
					inst, width, height, num_ref,
					is_tenbit);
			valid_buffer_type = true;
		} else if (curr_req->buffer_type ==
			HAL_BUFFER_INTERNAL_PERSIST) {
			curr_req->buffer_size =
				enc_calculators->calculate_persist_size();
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
		dprintk(VIDC_ERR, "Instance is null!");
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

	if (!inst)
		return;

	inst->buffer_size_calculators = NULL;
	core = inst->core;

	/* Change this to IRIS2 when ready */
	if (core->platform_data->vpu_ver == VPU_VERSION_AR50)
		inst->buffer_size_calculators =
			msm_vidc_calculate_internal_buffer_sizes;
}

int msm_vidc_init_buffer_count(struct msm_vidc_inst *inst)
{
	int extra_buff_count = 0;
	struct hal_buffer_requirements *bufreq;
	int port;

	if (!is_decode_session(inst) && !is_encode_session(inst))
		return 0;

	if (is_decode_session(inst))
		port = OUTPUT_PORT;
	else
		port = CAPTURE_PORT;

	/* Update input buff counts */
	bufreq = get_buff_req_buffer(inst, HAL_BUFFER_INPUT);
	if (!bufreq)
		return -EINVAL;

	extra_buff_count = msm_vidc_get_extra_buff_count(inst,
				HAL_BUFFER_INPUT);
	bufreq->buffer_count_min = inst->fmts[port].input_min_count;
	/* batching needs minimum batch size count of input buffers */
	if (inst->core->resources.decode_batching &&
		is_decode_session(inst) &&
		bufreq->buffer_count_min < inst->batch.size)
		bufreq->buffer_count_min = inst->batch.size;
	bufreq->buffer_count_min_host = bufreq->buffer_count_actual =
				bufreq->buffer_count_min + extra_buff_count;

	dprintk(VIDC_DBG, "%s: %x : input min %d min_host %d actual %d\n",
		__func__, hash32_ptr(inst->session),
		bufreq->buffer_count_min, bufreq->buffer_count_min_host,
		bufreq->buffer_count_actual);

	bufreq = get_buff_req_buffer(inst, HAL_BUFFER_EXTRADATA_INPUT);
	if (!bufreq)
		return -EINVAL;

	bufreq->buffer_count_min = inst->fmts[port].input_min_count;
	bufreq->buffer_count_min_host = bufreq->buffer_count_actual =
				bufreq->buffer_count_min + extra_buff_count;

	/* Update output buff count */
	bufreq = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
	if (!bufreq)
		return -EINVAL;

	extra_buff_count = msm_vidc_get_extra_buff_count(inst,
				HAL_BUFFER_OUTPUT);
	bufreq->buffer_count_min = inst->fmts[port].output_min_count;
	bufreq->buffer_count_min_host = bufreq->buffer_count_actual =
		bufreq->buffer_count_min + extra_buff_count;

	dprintk(VIDC_DBG, "%s: %x : output min %d min_host %d actual %d\n",
		__func__, hash32_ptr(inst->session),
		bufreq->buffer_count_min, bufreq->buffer_count_min_host,
		bufreq->buffer_count_actual);

	bufreq = get_buff_req_buffer(inst, HAL_BUFFER_EXTRADATA_OUTPUT);
	if (!bufreq)
		return -EINVAL;

	bufreq->buffer_count_min = inst->fmts[port].output_min_count;
	bufreq->buffer_count_min_host = bufreq->buffer_count_actual =
		bufreq->buffer_count_min + extra_buff_count;

	return 0;
}
u32 msm_vidc_set_buffer_count_for_thumbnail(struct msm_vidc_inst *inst)
{
	struct hal_buffer_requirements *bufreq;

	bufreq = get_buff_req_buffer(inst, HAL_BUFFER_INPUT);
	if (!bufreq)
		return -EINVAL;

	bufreq->buffer_count_min =
		MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
	bufreq->buffer_count_min_host =
		MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;
	bufreq->buffer_count_actual =
		MIN_NUM_THUMBNAIL_MODE_OUTPUT_BUFFERS;

	bufreq = get_buff_req_buffer(inst, HAL_BUFFER_OUTPUT);
	if (!bufreq)
		return -EINVAL;

	/* VP9 super frame requires multiple frames decoding */
	if (inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_VP9) {
		bufreq->buffer_count_min =
			MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS_VP9;
		bufreq->buffer_count_min_host =
			MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS_VP9;
		bufreq->buffer_count_actual =
			MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS_VP9;
	} else {
		bufreq->buffer_count_min =
			MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS;
		bufreq->buffer_count_min_host =
			MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS;
		bufreq->buffer_count_actual =
			MIN_NUM_THUMBNAIL_MODE_CAPTURE_BUFFERS;
	}
	return 0;
}

int msm_vidc_get_extra_buff_count(struct msm_vidc_inst *inst,
	enum hal_buffer buffer_type)
{
	unsigned int count = 0;

	if (!inst || !inst->core) {
		dprintk(VIDC_ERR, "%s Invalid args\n", __func__);
		return 0;
	}
	/*
	 * no extra buffers for thumbnail session because
	 * neither dcvs nor batching will be enabled
	 */
	if (is_thumbnail_session(inst))
		return 0;

	/* Add DCVS extra buffer count */
	if (inst->core->resources.dcvs) {
		if (is_decode_session(inst) &&
			buffer_type == HAL_BUFFER_OUTPUT) {
			count += DCVS_DEC_EXTRA_OUTPUT_BUFFERS;
		} else if ((is_encode_session(inst) &&
			buffer_type == HAL_BUFFER_INPUT)) {
			count += DCVS_ENC_EXTRA_INPUT_BUFFERS;
		}
	}

	/*
	 * if platform supports decode batching ensure minimum
	 * batch size count of extra buffers added on output port
	 */
	if (buffer_type == HAL_BUFFER_OUTPUT) {
		if (inst->core->resources.decode_batching &&
			is_decode_session(inst) &&
			count < inst->batch.size)
			count = inst->batch.size;
	}

	return count;
}

u32 msm_vidc_calculate_dec_input_frame_size(struct msm_vidc_inst *inst)
{
	u32 frame_size, num_mbs;
	u32 div_factor = 1;
	u32 base_res_mbs = NUM_MBS_4k;
	u32 width = inst->prop.width[OUTPUT_PORT];
	u32 height = inst->prop.height[OUTPUT_PORT];

	/*
	 * Decoder input size calculation:
	 * If clip is 8k buffer size is calculated for 8k : 8k mbs/4
	 * For 8k cases we expect width/height to be set always.
	 * In all other cases size is calculated for 4k:
	 * 4k mbs for VP8/VP9 and 4k/2 for remaining codecs
	 */
	num_mbs = ((width + 15) >> 4) * ((height + 15) >> 4);
	if (num_mbs > NUM_MBS_4k) {
		div_factor = 4;
		base_res_mbs = inst->capability.cap[CAP_MBS_PER_FRAME].max;
	} else {
		base_res_mbs = NUM_MBS_4k;
		if (inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_VP9)
			div_factor = 1;
		else
			div_factor = 2;
	}

	frame_size = base_res_mbs * MB_SIZE_IN_PIXEL * 3 / 2 / div_factor;

	if (is_secure_session(inst)) {
		u32 max_bitrate = inst->capability.cap[CAP_SECURE_BITRATE].max;

		/*
		 * for secure, calc frame_size based on max bitrate,
		 * peak bitrate can be 10 times more and
		 * frame rate assumed to be 30 fps at least
		 */
		frame_size = (max_bitrate * 10 / 8) / 30;
	}

	 /* multiply by 10/8 (1.25) to get size for 10 bit case */
	if ((inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_VP9) ||
		(inst->fmts[OUTPUT_PORT].fourcc == V4L2_PIX_FMT_HEVC))
		frame_size = frame_size + (frame_size >> 2);

	if (inst->buffer_size_limit &&
		(inst->buffer_size_limit < frame_size)) {
		frame_size = inst->buffer_size_limit;
		dprintk(VIDC_DBG, "input buffer size limited to %d\n",
			frame_size);
	} else {
		dprintk(VIDC_DBG, "set input buffer size to %d\n",
			frame_size);
	}

	return ALIGN(frame_size, SZ_4K);
}

u32 msm_vidc_calculate_dec_output_frame_size(struct msm_vidc_inst *inst)
{
	u32 hfi_fmt;

	hfi_fmt = msm_comm_convert_color_fmt(inst->fmts[CAPTURE_PORT].fourcc);
	return VENUS_BUFFER_SIZE(hfi_fmt, inst->prop.width[CAPTURE_PORT],
			inst->prop.height[CAPTURE_PORT]);
}

u32 msm_vidc_calculate_dec_output_extra_size(struct msm_vidc_inst *inst)
{
	return VENUS_EXTRADATA_SIZE(inst->prop.height[CAPTURE_PORT],
			inst->prop.width[CAPTURE_PORT]);
}

u32 msm_vidc_calculate_enc_input_frame_size(struct msm_vidc_inst *inst)
{
	u32 hfi_fmt;

	hfi_fmt = msm_comm_convert_color_fmt(inst->fmts[OUTPUT_PORT].fourcc);
	return VENUS_BUFFER_SIZE(hfi_fmt, inst->prop.width[OUTPUT_PORT],
			inst->prop.height[OUTPUT_PORT]);
}

u32 msm_vidc_calculate_enc_output_frame_size(struct msm_vidc_inst *inst)
{
	u32 frame_size;
	u32 mbs_per_frame;
	u32 width, height;

	/*
	 * Encoder output size calculation: 32 Align width/height
	 * For resolution < 720p : YUVsize * 4
	 * For resolution > 720p & <= 4K : YUVsize / 2
	 * For resolution > 4k : YUVsize / 4
	 * Initially frame_size = YUVsize * 2;
	 */
	width = ALIGN(inst->prop.width[CAPTURE_PORT],
		BUFFER_ALIGNMENT_SIZE(32));
	height = ALIGN(inst->prop.height[CAPTURE_PORT],
		BUFFER_ALIGNMENT_SIZE(32));
	mbs_per_frame = NUM_MBS_PER_FRAME(width, height);
	frame_size = (width * height * 3);

	if (mbs_per_frame < NUM_MBS_720P)
		frame_size = frame_size << 1;
	else if (mbs_per_frame <= NUM_MBS_4k)
		frame_size = frame_size >> 2;
	else
		frame_size = frame_size >> 3;

	if ((inst->rc_type == RATE_CONTROL_OFF) ||
		(inst->rc_type == V4L2_MPEG_VIDEO_BITRATE_MODE_CQ))
		frame_size = frame_size << 1;

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
	u32 width = inst->prop.width[OUTPUT_PORT];
	u32 height = inst->prop.height[OUTPUT_PORT];
	u32 extradata_count = 0;

	/* Add size for default extradata */
	size += sizeof(struct msm_vidc_enc_cvp_metadata_payload);
	extradata_count++;

	if (inst->prop.extradata_ctrls & EXTRADATA_ENC_INPUT_ROI) {
		u32 lcu_size = 16;

		if (inst->fmts[CAPTURE_PORT].fourcc == V4L2_PIX_FMT_HEVC)
			lcu_size = 32;

		size += ROI_EXTRADATA_SIZE(width, height, lcu_size);
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

	/* Add size for extradata none */
	if (size)
		size += sizeof(struct msm_vidc_extradata_header);

	return ALIGN(size, SZ_4K);
}

static inline u32 size_vpss_lb(u32 width, u32 height)
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
		(32 * ALIGN(height, 8)));
	opb_wr_top_line_chroma_buf_size = opb_wr_top_line_luma_buf_size;
	opb_lb_wr_llb_uv_buffer_size = opb_lb_wr_llb_y_buffer_size =
		ALIGN((ALIGN(height, 8) / 4 / 2) *
			64, BUFFER_ALIGNMENT_SIZE(32));
	size = 2 * (vpss_4tap_top_buffer_size +
		vpss_div2_top_buffer_size +
		vpss_4tap_left_buffer_size +
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
	u32 col_mv_aligned_width = (frame_width_in_mbs << 6);
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
	u32 aligned_height = ALIGN(height, BUFFER_ALIGNMENT_SIZE(32));

	return min_t(u32, (((aligned_height + 15) >> 4) * 3 * 4),
		H264D_MAX_SLICE) *
		SIZE_H264D_VPP_CMD_PER_BUF;
}

static inline u32 hfi_iris2_h264d_non_comv_size(u32 width, u32 height)
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
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
		ALIGN(SIZE_H264D_LB_SE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H264D_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
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

static inline u32 size_h264d_hw_bin_buffer(u32 width, u32 height)
{
	u32 size_yuv, size_bin_hdr, size_bin_res;
	u32 size = 0;
	u32 product;

	product = width * height;
	size_yuv = (product <= BIN_BUFFER_THRESHOLD) ?
			((BIN_BUFFER_THRESHOLD * 3) >> 1) :
			((product * 3) >> 1);

	size_bin_hdr = size_yuv * H264_CABAC_HDR_RATIO_HD_TOT_NUM /
		H264_CABAC_HDR_RATIO_HD_TOT_DEN;
	size_bin_res = size_yuv * H264_CABAC_RES_RATIO_HD_TOT_NUM /
		H264_CABAC_RES_RATIO_HD_TOT_DEN;
	size_bin_hdr = ALIGN(size_bin_hdr, VENUS_DMA_ALIGNMENT);
	size_bin_res = ALIGN(size_bin_res, VENUS_DMA_ALIGNMENT);
	size = size_bin_hdr + size_bin_res;
	return size;
}

static inline u32 calculate_h264d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced)
{
	u32 aligned_width = ALIGN(width, BUFFER_ALIGNMENT_SIZE(16));
	u32 aligned_height = ALIGN(height, BUFFER_ALIGNMENT_SIZE(16));
	u32 size = 0;

	if (!is_interlaced) {
		size = size_h264d_hw_bin_buffer(aligned_width, aligned_height);
		size = size * NUM_OF_VPP_PIPES;
	} else {
		size = 0;
	}

	return size;
}

static inline u32 size_h265d_bse_cmd_buf(u32 width, u32 height)
{
	u32 size;

	size = ALIGN(((ALIGN(width, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS) +
		(ALIGN(height, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS)) *
		NUM_HW_PIC_BUF, VENUS_DMA_ALIGNMENT);
	size = min_t(u32, size, H265D_MAX_SLICE + 1);
	size = 2 * size * SIZE_H265D_BSE_CMD_PER_BUF;
	return size;
}

static inline u32 size_h265d_vpp_cmd_buf(u32 width, u32 height)
{
	u32 size = 0;

	size = ALIGN((
		(ALIGN(width, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS) +
		(ALIGN(height, LCU_MAX_SIZE_PELS) / LCU_MIN_SIZE_PELS)) *
		NUM_HW_PIC_BUF, VENUS_DMA_ALIGNMENT);
	size = min_t(u32, size, H265D_MAX_SLICE + 1);
	size = ALIGN(size, 4);
	size = 2 * size * SIZE_H265D_VPP_CMD_PER_BUF;

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

static inline u32 hfi_iris2_h265d_non_comv_size(u32 width, u32 height)
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
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
		ALIGN(SIZE_H265D_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
		ALIGN(SIZE_H265D_LB_SE_TOP_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H265D_LB_PE_TOP_DATA(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H265D_LB_VSP_TOP(width, height),
			VENUS_DMA_ALIGNMENT) +
		ALIGN(SIZE_H265D_LB_VSP_LEFT(width, height),
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
		ALIGN(SIZE_H265D_LB_RECON_DMA_METADATA_WR(width, height),
			VENUS_DMA_ALIGNMENT) * 4 +
		ALIGN(SIZE_H265D_QP(width, height), VENUS_DMA_ALIGNMENT);
	size = ALIGN(size, VENUS_DMA_ALIGNMENT);
	return size;
}

static inline u32 size_h265d_hw_bin_buffer(u32 width, u32 height)
{
	u32 size = 0;
	u32 size_yuv, size_bin_hdr, size_bin_res;
	u32 product;

	product = width * height;
	size_yuv = (product <= BIN_BUFFER_THRESHOLD) ?
		((BIN_BUFFER_THRESHOLD * 3) >> 1) :
		((product * 3) >> 1);
	size_bin_hdr = size_yuv * H265_CABAC_HDR_RATIO_HD_TOT_NUM /
		H265_CABAC_HDR_RATIO_HD_TOT_DEN;
	size_bin_res = size_yuv * H265_CABAC_RES_RATIO_HD_TOT_NUM /
		H265_CABAC_RES_RATIO_HD_TOT_DEN;
	size_bin_hdr = ALIGN(size_bin_hdr, VENUS_DMA_ALIGNMENT);
	size_bin_res = ALIGN(size_bin_res, VENUS_DMA_ALIGNMENT);
	size = size_bin_hdr + size_bin_res;

	return size;
}

static inline u32 calculate_h265d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced)
{
	u32 aligned_width = ALIGN(width, BUFFER_ALIGNMENT_SIZE(16));
	u32 aligned_height = ALIGN(height, BUFFER_ALIGNMENT_SIZE(16));
	u32 size = 0;

	if (!is_interlaced) {
		size = size_h265d_hw_bin_buffer(aligned_width, aligned_height);
		size = size * NUM_OF_VPP_PIPES;
	} else {
		size = 0;
	}

	return size;
}

static inline u32 calculate_vpxd_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced)
{
	u32 aligned_width = ALIGN(width, BUFFER_ALIGNMENT_SIZE(16));
	u32 aligned_height = ALIGN(height, BUFFER_ALIGNMENT_SIZE(16));
	u32 size = 0;
	u32 size_yuv = aligned_width * aligned_height * 3 / 2;

	if (!is_interlaced) {
		/* binbuffer1_size + binbufer2_size */
		u32 binbuffer1_size = 0, binbufer2_size = 0;

		binbuffer1_size = max_t(u32, size_yuv,
			((BIN_BUFFER_THRESHOLD * 3) >> 1)) *
			VPX_DECODER_FRAME_CONCURENCY_LVL *
			VPX_DECODER_FRAME_BIN_HDR_BUDGET_RATIO_NUM /
			VPX_DECODER_FRAME_BIN_HDR_BUDGET_RATIO_DEN;
		binbufer2_size = max_t(u32, size_yuv,
			((BIN_BUFFER_THRESHOLD * 3) >> 1)) *
			VPX_DECODER_FRAME_CONCURENCY_LVL *
			VPX_DECODER_FRAME_BIN_RES_BUDGET_RATIO_NUM /
			VPX_DECODER_FRAME_BIN_RES_BUDGET_RATIO_DEN;
		size = ALIGN(binbuffer1_size + binbufer2_size,
			VENUS_DMA_ALIGNMENT);
	} else {
		size = 0;
	}

	return size;
}

static inline u32 calculate_mpeg2d_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, bool is_interlaced)
{
	return 0;
}

static inline u32 calculate_enc_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode, u32 lcu_size)
{
	u32 ALIGNedWidth, ALIGNedHeight, bitstream_size;
	u32 total_bitbin_buffers = 0, size_singlePipe, bitbin_size = 0;
	u32 sao_bin_buffer_size, padded_bin_size, size = 0;

	ALIGNedWidth = ALIGN(width, lcu_size);
	ALIGNedHeight = ALIGN(height, lcu_size);
	bitstream_size = ALIGNedWidth * ALIGNedHeight * 3;
	if (bitstream_size > (352 * 288 * 4))
		bitstream_size = (bitstream_size >> 2);

	bitstream_size = ALIGN(bitstream_size, VENUS_DMA_ALIGNMENT);
	if (work_mode == HFI_WORKMODE_2) {
		total_bitbin_buffers = 3;
		bitbin_size = bitstream_size * 17 / 10;
		bitbin_size = ALIGN(bitbin_size, VENUS_DMA_ALIGNMENT);
	} else {
		total_bitbin_buffers = 1;
		bitstream_size = ALIGNedWidth * ALIGNedHeight * 3;
		bitbin_size = ALIGN(bitstream_size, VENUS_DMA_ALIGNMENT);
	}
	size_singlePipe = bitbin_size / 2;
	size_singlePipe = ALIGN(size_singlePipe, VENUS_DMA_ALIGNMENT);
	sao_bin_buffer_size = (64 * (((width + BUFFER_ALIGNMENT_SIZE(32)) *
		(height + BUFFER_ALIGNMENT_SIZE(32))) >> 10)) + 384;
	padded_bin_size = ALIGN(size_singlePipe, VENUS_DMA_ALIGNMENT);
	size_singlePipe = sao_bin_buffer_size + padded_bin_size;
	size_singlePipe = ALIGN(size_singlePipe, VENUS_DMA_ALIGNMENT);
	bitbin_size = size_singlePipe * NUM_OF_VPP_PIPES;
	size = ALIGN(bitbin_size, VENUS_DMA_ALIGNMENT) * total_bitbin_buffers;

	return size;
}

static inline u32 calculate_h264e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode)
{
	return calculate_enc_scratch_size(inst, width, height, work_mode, 16);
}

static inline u32 calculate_h265e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode)
{
	return calculate_enc_scratch_size(inst, width, height, work_mode, 32);
}

static inline u32 calculate_vp8e_scratch_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 work_mode)
{
	return calculate_enc_scratch_size(inst, width, height, work_mode, 16);
}

static inline u32 calculate_h264d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled)
{
	u32 co_mv_size = 0, nonco_mv_size = 0;
	u32 vpss_lb_size = 0;
	u32 size = 0;

	co_mv_size = hfi_iris2_h264d_comv_size(width, height, min_buf_count);
	nonco_mv_size = hfi_iris2_h264d_non_comv_size(width, height);
	if (split_mode_enabled)
		vpss_lb_size = size_vpss_lb(width, height);

	size = co_mv_size + nonco_mv_size + vpss_lb_size;
	return size;
}

static inline u32 calculate_h265d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled)
{
	u32 co_mv_size = 0, nonco_mv_size = 0;
	u32 vpss_lb_size = 0;
	u32 size = 0;

	co_mv_size = hfi_iris2_h265d_comv_size(width, height, min_buf_count);
	nonco_mv_size = hfi_iris2_h265d_non_comv_size(width, height);
	if (split_mode_enabled)
		vpss_lb_size = size_vpss_lb(width, height);

	size = co_mv_size + nonco_mv_size + vpss_lb_size;
	return size;
}

static inline u32 hfi_iris2_vp8d_comv_size(u32 width, u32 height,
	u32 yuv_min_buf_count)
{
	return (((width + 15) >> 4) * ((height + 15) >> 4) * 8 * 2);
}

static inline u32 calculate_vp8d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled)
{
	u32 vpss_lb_size = 0;
	u32 size = 0;

	size = hfi_iris2_vp8d_comv_size(width, height, 0);
	size += ALIGN(SIZE_VPXD_LB_FE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
		ALIGN(SIZE_VPXD_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
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
		vpss_lb_size = size_vpss_lb(width, height);

	size += vpss_lb_size;
	return size;
}

static inline u32 calculate_vp9d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled)
{
	u32 vpss_lb_size = 0;
	u32 size = 0;

	size = ALIGN(SIZE_VPXD_LB_FE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
		ALIGN(SIZE_VPXD_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
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
		vpss_lb_size = size_vpss_lb(width, height);

	size += vpss_lb_size;
	return size;
}

static inline u32 calculate_mpeg2d_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 min_buf_count, bool split_mode_enabled)
{
	u32 vpss_lb_size = 0;
	u32 size = 0;

	size = ALIGN(SIZE_VPXD_LB_FE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
		ALIGN(SIZE_VPXD_LB_SE_LEFT_CTRL(width, height),
			VENUS_DMA_ALIGNMENT) * NUM_OF_VPP_PIPES +
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
		vpss_lb_size = size_vpss_lb(width, height);

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
	u32 vpss_line_buf, leftline_buf_meta_recony, h265e_colrcbuf_size;
	u32 h265e_framerc_bufsize, h265e_lcubitcnt_bufsize;
	u32 h265e_lcubitmap_bufsize, se_stats_bufsize;
	u32 bse_reg_buffer_size, bse_slice_cmd_buffer_size, slice_info_bufsize;
	u32 line_buf_ctrl_size_buffid2, slice_cmd_buffer_size;
	u32 width_lcu_num, height_lcu_num, width_coded, height_coded;
	u32 frame_num_lcu, linebuf_meta_recon_uv, topline_bufsize_fe_1stg_sao;
	u32 output_mv_bufsize = 0, temp_scratch_mv_bufsize = 0;
	u32 size, bit_depth;

	width_lcu_num = ((width)+(lcu_size)-1) / (lcu_size);
	height_lcu_num = ((height)+(lcu_size)-1) / (lcu_size);
	frame_num_lcu = width_lcu_num * height_lcu_num;
	width_coded = width_lcu_num * (lcu_size);
	height_coded = height_lcu_num * (lcu_size);
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
	leftline_buf_ctrl_size_FE = ((VENUS_DMA_ALIGNMENT + 64 *
		(height_coded >> 4)) +
		(VENUS_DMA_ALIGNMENT << (num_vpp_pipes - 1)) - 1) &
		(~((VENUS_DMA_ALIGNMENT << (num_vpp_pipes - 1)) - 1)) * 1;
	leftline_buf_meta_recony = ((VENUS_DMA_ALIGNMENT + 64 *
		((height_coded) / (8 * (ten_bit ? 4 : 8)))) * num_vpp_pipes);
	leftline_buf_meta_recony = ALIGN(leftline_buf_meta_recony,
		VENUS_DMA_ALIGNMENT);
	linebuf_meta_recon_uv = ((VENUS_DMA_ALIGNMENT + 64 *
		((height_coded) / (4 * (ten_bit ? 4 : 8)))) * num_vpp_pipes);
	linebuf_meta_recon_uv = ALIGN(linebuf_meta_recon_uv,
		VENUS_DMA_ALIGNMENT);
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
	h265e_colrcbuf_size = (((width_lcu_num + 7) >> 3) *
		16 * 2 * height_lcu_num);
	h265e_colrcbuf_size = ALIGN(h265e_colrcbuf_size,
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
		(~31)) * 8);
	lambda_lut_size = ((((52 << 1) + 7) & (~7)) * 3);
	override_buffer_size = 16 * ((frame_num_lcu + 7) >> 3);
	override_buffer_size = ALIGN(override_buffer_size,
		VENUS_DMA_ALIGNMENT) * 2;
	ir_buffer_size = (((frame_num_lcu << 1) + 7) & (~7)) * 3;
	vpss_line_buf = ((16 * width_coded) + (16 * height_coded));
	topline_bufsize_fe_1stg_sao = (16 * (width_coded >> 5));
	topline_bufsize_fe_1stg_sao = ALIGN(topline_bufsize_fe_1stg_sao,
		VENUS_DMA_ALIGNMENT);
	size = line_buf_ctrl_size + line_buf_data_size +
		line_buf_ctrl_size_buffid2 + leftline_buf_ctrl_size +
		vpss_line_buf + col_mv_buf_size + topline_buf_ctrl_size_FE +
		leftline_buf_ctrl_size_FE + line_buf_recon_pix_size +
		leftline_buf_recon_pix_size + leftline_buf_meta_recony +
		linebuf_meta_recon_uv + h265e_colrcbuf_size +
		h265e_framerc_bufsize + h265e_lcubitcnt_bufsize +
		h265e_lcubitmap_bufsize + line_buf_sde_size +
		topline_bufsize_fe_1stg_sao + override_buffer_size +
		bse_reg_buffer_size + vpp_reg_buffer_size +
		sps_pps_slice_hdr + bse_slice_cmd_buffer_size +
		ir_buffer_size + slice_info_bufsize + lambda_lut_size +
		se_stats_bufsize + temp_scratch_mv_bufsize + output_mv_bufsize
		+ 1024;
	return size;
}

static inline u32 calculate_h264e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit,
	u32 num_vpp_pipes)
{
	return calculate_enc_scratch1_size(inst, width, height, 16,
		num_ref, ten_bit, num_vpp_pipes, false);
}

static inline u32 calculate_h265e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit,
	u32 num_vpp_pipes)
{
	return calculate_enc_scratch1_size(inst, width, height, 32,
		num_ref, ten_bit, num_vpp_pipes, true);
}

static inline u32 calculate_vp8e_scratch1_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit,
	u32 num_vpp_pipes)
{
	return calculate_enc_scratch1_size(inst, width, height, 16,
		num_ref, ten_bit, num_vpp_pipes, false);
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

static inline u32 calculate_enc_scratch2_size(struct msm_vidc_inst *inst,
	u32 width, u32 height, u32 num_ref, bool ten_bit)
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
		metadata_stride = hfi_ubwc_uv_metadata_plane_stride(width,
			64, HFI_COLOR_FORMAT_YUV420_NV12_UBWC_UV_TILE_WIDTH);
		meta_buf_height = hfi_ubwc_uv_metadata_plane_bufheight(
			height, 16,
			HFI_COLOR_FORMAT_YUV420_NV12_UBWC_UV_TILE_HEIGHT);
		meta_size_c = hfi_ubwc_metadata_plane_buffer_size(
			metadata_stride, meta_buf_height);
		size = (aligned_height + chroma_height) * aligned_width +
			meta_size_y + meta_size_c;
		size = (size * ((num_ref)+1)) + 4096;
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
			HFI_COLOR_FORMAT_YUV420_NV12_UBWC_Y_TILE_WIDTH);
		meta_buf_height = hfi_ubwc_metadata_plane_bufheight(height,
			VENUS_METADATA_HEIGHT_MULTIPLE,
			HFI_COLOR_FORMAT_YUV420_NV12_UBWC_Y_TILE_HEIGHT);
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
		size = (size * ((num_ref)+1)) + 4096;
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

	size = ALIGN((SIZE_SLIST_BUF_H264 * NUM_SLIST_BUF_H264),
			VENUS_DMA_ALIGNMENT);
	return size;
}

static inline u32 calculate_h265d_persist1_size(void)
{
	u32 size = 0;

	size = ALIGN((SIZE_SLIST_BUF_H265 * NUM_SLIST_BUF_H265 + H265_NUM_TILE
			* sizeof(u32)), VENUS_DMA_ALIGNMENT);
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
