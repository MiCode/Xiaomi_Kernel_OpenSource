/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (c) 2019 MediaTek Inc.
 */

#ifndef _VENC_IPI_MSG_H_
#define _VENC_IPI_MSG_H_

#include <linux/videodev2.h>
#include "vcodec_ipi_msg.h"

#define MTK_MAX_ENC_CODECS_SUPPORT       (64)
#define AP_IPIMSG_VENC_BASE 0xC000
#define VCU_IPIMSG_VENC_BASE 0xD000
#define VCU_IPIMSG_VENC_SEND_BASE 0xE000

#define VENC_MAX_FB_NUM              VIDEO_MAX_FRAME
#define VENC_MAX_BS_NUM              VIDEO_MAX_FRAME


/**
 * enum venc_ipi_msg_id - message id between AP and VCU
 * (ipi stands for inter-processor interrupt)
 * @AP_IPIMSG_ENC_XXX:          AP to VCU cmd message id
 * @VCU_IPIMSG_ENC_XXX_DONE:    VCU ack AP cmd message id
 */
enum venc_ipi_msg_id {
	AP_IPIMSG_ENC_INIT = AP_IPIMSG_VENC_BASE,
	AP_IPIMSG_ENC_SET_PARAM,
	AP_IPIMSG_ENC_ENCODE,
	AP_IPIMSG_ENC_DEINIT,
	AP_IPIMSG_ENC_QUERY_CAP,

	VCU_IPIMSG_ENC_INIT_DONE = VCU_IPIMSG_VENC_BASE,
	VCU_IPIMSG_ENC_SET_PARAM_DONE,
	VCU_IPIMSG_ENC_ENCODE_DONE,
	VCU_IPIMSG_ENC_DEINIT_DONE,
	VCU_IPIMSG_ENC_QUERY_CAP_ACK,
	VCU_IPIMSG_ENC_ENCODE_ACK,

	VCU_IPIMSG_ENC_POWER_ON = VCU_IPIMSG_VENC_SEND_BASE,
	VCU_IPIMSG_ENC_POWER_OFF,
	VCU_IPIMSG_ENC_WAIT_ISR,
	VCU_IPIMSG_ENC_PUT_BUFFER
};

/* enum venc_get_param_type - The type of set parameter used in
 *                            venc_if_get_param()
 * VENC_GET_PARAM_CAPABILITY_SUPPORTED_FORMATS: get codec supported format capability
 * VENC_GET_PARAM_CAPABILITY_FRAME_SIZES:
 *         get codec supported frame size & alignment info
 */
enum venc_get_param_type {
	VENC_GET_PARAM_CAPABILITY_SUPPORTED_FORMATS,
	VENC_GET_PARAM_CAPABILITY_FRAME_SIZES,
	VENC_GET_PARAM_FREE_BUFFERS,
	VENC_GET_PARAM_ROI_RC_QP,
	VENC_GET_PARAM_RESOLUTION_CHANGE,
	VENC_GET_PARAM_REFBUF_FRAME_NUM,
};

/*
 * enum venc_set_param_type - The type of set parameter used in
 *                                                    venc_if_set_param()
 * (VCU related: If you change the order, you must also update the VCU codes.)
 * @VENC_SET_PARAM_ENC: set encoder parameters
 * @VENC_SET_PARAM_FORCE_INTRA: force an intra frame
 * @VENC_SET_PARAM_ADJUST_BITRATE: adjust bitrate (in bps)
 * @VENC_SET_PARAM_ADJUST_FRAMERATE: set frame rate
 * @VENC_SET_PARAM_GOP_SIZE: set IDR interval
 * @VENC_SET_PARAM_INTRA_PERIOD: set I frame interval
 * @VENC_SET_PARAM_SKIP_FRAME: set H264 skip one frame
 * @VENC_SET_PARAM_PREPEND_HEADER: set H264 prepend SPS/PPS before IDR
 * @VENC_SET_PARAM_TS_MODE: set VP8 temporal scalability mode
 * @VENC_SET_PARAM_SCENARIO: set encoder scenario mode for different RC control
 * @VENC_SET_PARAM_NONREFP: set encoder non reference P period
 * @VENC_SET_PARAM_LOG: set encoder log
 */
enum venc_set_param_type {
	VENC_SET_PARAM_ENC,
	VENC_SET_PARAM_FORCE_INTRA,
	VENC_SET_PARAM_ADJUST_BITRATE,
	VENC_SET_PARAM_ADJUST_FRAMERATE,
	VENC_SET_PARAM_GOP_SIZE,
	VENC_SET_PARAM_INTRA_PERIOD,
	VENC_SET_PARAM_SKIP_FRAME,
	VENC_SET_PARAM_PREPEND_HEADER,
	VENC_SET_PARAM_TS_MODE,
	VENC_SET_PARAM_SCENARIO,
	VENC_SET_PARAM_NONREFP,
	VENC_SET_PARAM_DETECTED_FRAMERATE,
	VENC_SET_PARAM_RFS_ON,
	VENC_SET_PARAM_PREPEND_SPSPPS_TO_IDR,
	VENC_SET_PARAM_OPERATION_RATE,
	VENC_SET_PARAM_BITRATE_MODE,
	VENC_SET_PARAM_ROI_ON,
	VENC_SET_PARAM_HEIF_GRID_SIZE,
	VENC_SET_PARAM_COLOR_DESC,
	VENC_SET_PARAM_SEC_MODE,
	VENC_SET_PARAM_TSVC,
	VENC_SET_PARAM_NONREFPFREQ,
	VENC_SET_PARAM_ADJUST_MAX_QP,
	VENC_SET_PARAM_ADJUST_MIN_QP,
	VENC_SET_PARAM_ADJUST_I_P_QP_DELTA,
	VENC_SET_PARAM_ADJUST_FRAME_LEVEL_QP,
	VENC_SET_PARAM_MAX_REFP_NUM,
	VENC_SET_PARAM_REFP_DISTANCE,
	VENC_SET_PARAM_REFP_FRMNUM,
	VENC_SET_PARAM_LOG,
	VENC_SET_PARAM_ENABLE_DUMMY_NAL,
};

/**
 * struct venc_ap_ipi_msg_init - AP to VCU init cmd structure
 * @msg_id:     message id (AP_IPIMSG_XXX_ENC_INIT)
 * @reserved:   reserved for future use. vcu is running in 32bit. Without
 *              this reserved field, if kernel run in 64bit. this struct size
 *              will be different between kernel and vcu
 * @venc_inst:  AP encoder instance
 *              (struct venc_vp8_inst/venc_h264_inst *)
 */
struct venc_ap_ipi_msg_init {
	__u32 msg_id;
	__u32 reserved;
	__u64 venc_inst;
};

/**
 * struct venc_ap_ipi_query_cap - for AP_IPIMSG_ENC_QUERY_CAP
 * @msg_id        : AP_IPIMSG_ENC_QUERY_CAP
 * @id      : query capability type
 * @vdec_inst     : AP query data address
 */
struct venc_ap_ipi_query_cap {
	__u32 msg_id;
	__u32 id;
#ifndef CONFIG_64BIT
	union {
		__u64 ap_inst_addr_64;
		__u32 ap_inst_addr;
	};
	union {
		__u64 ap_data_addr_64;
		__u32 ap_data_addr;
	};
#else
	__u64 ap_inst_addr;
	__u64 ap_data_addr;
#endif
};

/**
 * struct venc_vcu_ipi_query_cap_ack - for VCU_IPIMSG_ENC_QUERY_CAP_ACK
 * @msg_id      : VCU_IPIMSG_ENC_QUERY_CAP_ACK
 * @status      : VCU exeuction result
 * @ap_data_addr   : AP query data address
 * @vcu_data_addr  : VCU query data address
 */
struct venc_vcu_ipi_query_cap_ack {
	__u32 msg_id;
	__s32 status;
#ifndef CONFIG_64BIT
	union {
		__u64 ap_inst_addr_64;
		__u32 ap_inst_addr;
	};
	__u32 id;
	union {
		__u64 ap_data_addr_64;
		__u32 ap_data_addr;
	};
#else
	__u64 ap_inst_addr;
	__u32 id;
	__u64 ap_data_addr;
#endif
	__u32 vcu_data_addr;
};

/**
 * struct venc_ap_ipi_msg_set_param - AP to VCU set_param cmd structure
 * @msg_id:     message id (AP_IPIMSG_XXX_ENC_SET_PARAM)
 * @vcu_inst_addr:      VCU encoder instance addr
 *                      (struct venc_vp8_vsi/venc_h264_vsi *)
 * @param_id:   parameter id (venc_set_param_type)
 * @data_item:  number of items in the data array
 * @data[8]:    data array to store the set parameters
 */
struct venc_ap_ipi_msg_set_param {
	__u32 msg_id;
	__u32 vcu_inst_addr;
	__u32 param_id;
	__u32 data_item;
	__u32 data[8];
};

/**
 * struct venc_ap_ipi_msg_enc - AP to VCU enc cmd structure
 * @msg_id:     message id (AP_IPIMSG_XXX_ENC_ENCODE)
 * @vcu_inst_addr:      VCU encoder instance addr
 *                      (struct venc_vp8_vsi/venc_h264_vsi *)
 * @bs_mode:    bitstream mode for h264
 *              (H264_BS_MODE_SPS/H264_BS_MODE_PPS/H264_BS_MODE_FRAME)
 * @input_addr: pointer to input image buffer plane
 * @input_size: image buffer size
 * @bs_addr:    pointer to output bit stream buffer
 * @bs_size:    bit stream buffer size
 * @fb_num_planes:      image buffer plane number
 */
struct venc_ap_ipi_msg_enc {
	__u32 msg_id;
	__u32 vcu_inst_addr;
	__u32 input_addr[3];
	__u32 input_size[3];
	__u32 bs_addr;
	__u32 bs_size;
	__u32 data_offset[3];
	__s16 input_fd[3];
	__s16 bs_fd;
	__u8 fb_num_planes;
	__u8 bs_mode;
	__u32 meta_size;
	__s16 meta_fd;
	__u32 qpmap;
};

/**
 * struct venc_ap_ipi_msg_deinit - AP to VCU deinit cmd structure
 * @msg_id:     message id (AP_IPIMSG_XXX_ENC_DEINIT)
 * @vcu_inst_addr:      VCU encoder instance addr
 *                      (struct venc_vp8_vsi/venc_h264_vsi *)
 */
struct venc_ap_ipi_msg_deinit {
	__u32 msg_id;
	__u32 vcu_inst_addr;
};

/**
 * enum venc_ipi_msg_status - VCU ack AP cmd status
 */
enum venc_ipi_msg_status {
	VENC_IPI_MSG_STATUS_OK,
	VENC_IPI_MSG_STATUS_FAIL,
};

/**
 * struct venc_vcu_ipi_msg_common - VCU ack AP cmd common structure
 * @msg_id:     message id (VCU_IPIMSG_XXX_DONE)
 * @status:     cmd status (venc_ipi_msg_status)
 * @venc_inst:  AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 */
struct venc_vcu_ipi_msg_common {
	__u32 msg_id;
	__s32 status;
	__u64 venc_inst;
};

/**
 * struct venc_vcu_ipi_msg_init - VCU ack AP init cmd structure
 * @msg_id:     message id (VCU_IPIMSG_XXX_ENC_SET_PARAM_DONE)
 * @status:     cmd status (venc_ipi_msg_status)
 * @venc_inst:  AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 * @vcu_inst_addr:      VCU encoder instance addr
 *                      (struct venc_vp8_vsi/venc_h264_vsi *)
 * @reserved:   reserved for future use. vcu is running in 32bit. Without
 *              this reserved field, if kernel run in 64bit. this struct size
 *              will be different between kernel and vcu
 */
struct venc_vcu_ipi_msg_init {
	__u32 msg_id;
	__s32 status;
	__u64 venc_inst;
	__u32 vcu_inst_addr;
	__u32 reserved;
};

/**
 * struct venc_vcu_ipi_msg_set_param - VCU ack AP set_param cmd structure
 * @msg_id:     message id (VCU_IPIMSG_XXX_ENC_SET_PARAM_DONE)
 * @status:     cmd status (venc_ipi_msg_status)
 * @venc_inst:  AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 * @param_id:   parameter id (venc_set_param_type)
 * @data_item:  number of items in the data array
 * @data[6]:    data array to store the return result
 */
struct venc_vcu_ipi_msg_set_param {
	__u32 msg_id;
	__s32 status;
	__u64 venc_inst;
	__u32 param_id;
	__u32 data_item;
	__u32 data[6];
};

/**
 * enum venc_ipi_msg_enc_state - Type of encode state
 * VEN_IPI_MSG_ENC_STATE_FRAME: one frame being encoded
 * VEN_IPI_MSG_ENC_STATE_PART:  bit stream buffer full
 * VEN_IPI_MSG_ENC_STATE_SKIP:  encoded skip frame
 * VEN_IPI_MSG_ENC_STATE_ERROR: encounter error
 */
enum venc_ipi_msg_enc_state {
	VEN_IPI_MSG_ENC_STATE_FRAME,
	VEN_IPI_MSG_ENC_STATE_PART,
	VEN_IPI_MSG_ENC_STATE_SKIP,
	VEN_IPI_MSG_ENC_STATE_ERROR,
};

/**
 * struct venc_vcu_ipi_msg_enc - VCU ack AP enc cmd structure
 * @msg_id:     message id (VCU_IPIMSG_XXX_ENC_ENCODE_DONE)
 * @status:     cmd status (venc_ipi_msg_status)
 * @venc_inst:  AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 * @state:      encode state (venc_ipi_msg_enc_state)
 * @is_key_frm: whether the encoded frame is key frame
 * @bs_size:    encoded bitstream size
 * @reserved:   reserved for future use. vcu is running in 32bit. Without
 *              this reserved field, if kernel run in 64bit. this struct size
 *              will be different between kernel and vcu
 */
struct venc_vcu_ipi_msg_enc {
	__u32 msg_id;
	__s32 status;
	__u64 venc_inst;
	__u32 state;
	__u32 is_key_frm;
	__u32 bs_size;
	__u32 reserved;
};

/**
 * struct venc_vcu_ipi_msg_deinit - VCU ack AP deinit cmd structure
 * @msg_id:   message id (VCU_IPIMSG_XXX_ENC_DEINIT_DONE)
 * @status:   cmd status (venc_ipi_msg_status)
 * @venc_inst:  AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 */
struct venc_vcu_ipi_msg_deinit {
	__u32 msg_id;
	__s32 status;
	__u64 venc_inst;
};

/**
 * struct venc_vcu_ipi_msg_waitisr - VCU ack AP wait isr cmd structure
 * @msg_id:   message id (VCU_IPIMSG_XXX_ENC_DEINIT_DONE)
 * @status:   cmd status (venc_ipi_msg_status)
 * @venc_inst:  AP encoder instance (struct venc_vp8_inst/venc_h264_inst *)
 * @irq_status: encoder irq status
 * @timeout: 1 indicate encode timeout, 0 indicate no error
 */
struct venc_vcu_ipi_msg_waitisr {
	__u32 msg_id;
	__s32 status;
	__u64 venc_inst;
	__u32 irq_status;
	__u32 timeout;
};

/*
 * struct venc_vcu_config - Structure for encoder configuration
 *                               AP-W/R : AP is writer/reader on this item
 *                               VCU-W/R: VCU is write/reader on this item
 * @input_fourcc: input fourcc
 * @bitrate: target bitrate (in bps)
 * @pic_w: picture width. Picture size is visible stream resolution, in pixels,
 *         to be used for display purposes; must be smaller or equal to buffer
 *         size.
 * @pic_h: picture height
 * @buf_w: buffer width. Buffer size is stream resolution in pixels aligned to
 *         hardware requirements.
 * @buf_h: buffer height
 * @gop_size: group of picture size (idr frame)
 * @intra_period: intra frame period
 * @framerate: frame rate in fps
 * @profile: as specified in standard
 * @level: as specified in standard
 * @wfd: WFD mode 1:on, 0:off
 */
struct venc_vcu_config {
	__u32 input_fourcc;
	__u32 bitrate;
	__u32 pic_w;
	__u32 pic_h;
	__u32 buf_w;
	__u32 buf_h;
	__u32 gop_size;
	__u32 intra_period;
	__u32 framerate;
	__u32 profile;
	__u32 level;
	__u32 wfd;
	__u32 operationrate;
	__u32 scenario;
	__u32 prependheader;
	__u32 bitratemode;
	__u32 roi_rc_qp;
	__u32 roion;
	__u32 heif_grid_size;
	struct mtk_color_desc color_desc;
	__u32 resolutionChange;
	__u32 max_w;
	__u32 max_h;
	__u32 num_b_frame;
	__u32 slbc_ready;
	__u32 i_qp;
	__u32 p_qp;
	__u32 b_qp;
	__u32 svp_mode;
	__u32 svp_is_hal_secure_handle;
	__u32 tsvc;
	__u32 max_qp;
	__u32 min_qp;
	__u32 i_p_qp_delta;
	__u32 qp_control_mode;
	__u32 frame_level_qp;
	__u32 maxrefpnum;
	__u32 maxrefbufFrameNum;
	__u32 dummynal;
};

/**
 * enum venc_bs_mode - for bs_mode argument in venc_bs_mode
 */
enum venc_bs_mode {
	VENC_BS_MODE_SPS = 0,
	VENC_BS_MODE_PPS,
	VENC_BS_MODE_SEQ_HDR,
	VENC_BS_MODE_FRAME,
	VENC_BS_MODE_FRAME_FINAL,
	VENC_BS_MODE_MAX
};

/**
 * struct venc_info - encode information
 * @bs_dma		: Input bit-stream buffer dma address
 * @bs_fd           : Input bit-stream buffer dmabuf fd
 * @fb_dma		    : Y frame buffer dma address
 * @fb_fd           : Y frame buffer dmabuf fd
 * @venc_bs_va		: VDEC bitstream buffer struct virtual address
 * @venc_fb_va		: VDEC frame buffer struct virtual address
 * @fb_num_planes	: frame buffer plane count
 * @reserved		: reserved variable for 64bit align
 */
struct venc_info {
	__u64 bs_dma;
	__u64 bs_fd;
	__u64 fb_dma[VIDEO_MAX_PLANES];
	__u64 fb_fd[VIDEO_MAX_PLANES];
	__u64 venc_bs_va;
	__u64 venc_fb_va;
	__u32 fb_num_planes;
	__u32 index;
	__u64 timestamp;
	__u32 roimap;
	__u32 qpmap;
};

/**
 * struct ring_input_list - ring input buffer list
 * @venc_bs_va_list   : bitstream buffer arrary
 * @venc_fb_va_list   : frame buffer arrary
 * @read_idx  : read index
 * @write_idx : write index
 * @count     : buffer count in list
 */
struct ring_input_list {
	__u64 venc_bs_va_list[VENC_MAX_FB_NUM];
	__u64 venc_fb_va_list[VENC_MAX_FB_NUM];
	__u32 bs_size[VENC_MAX_FB_NUM];
	__u32 is_key_frm[VENC_MAX_FB_NUM];
	__s32 read_idx;
	__s32 write_idx;
	__s32 count;
	__s32 reserved;
};

/*
 * struct venc_vsi - Structure for VCU driver control and info share
 *                        AP-W/R : AP is writer/reader on this item
 *                        VCU-W/R: VCU is write/reader on this item
 * This structure is allocated in VCU side and shared to AP side.
 * @config: h264 encoder configuration
 * @work_bufs: working buffer information in VCU side
 * The work_bufs here is for storing the 'size' info shared to AP side.
 * The similar item in struct venc_inst is for memory allocation
 * in AP side. The AP driver will copy the 'size' from here to the one in
 * struct mtk_vcodec_mem, then invoke mtk_vcodec_mem_alloc to allocate
 * the buffer. After that, bypass the 'dma_addr' to the 'iova' field here for
 * register setting in VCU side.
 */
struct venc_vsi {
	struct venc_vcu_config config;
	__u32  sizeimage[VIDEO_MAX_PLANES];
	struct ring_input_list list_free;
	struct venc_info       venc;
	__u32 sync_mode;
	__u32 meta_size;
	__u64 meta_addr;
	__s16 meta_fd;
};

#endif /* _VENC_IPI_MSG_H_ */
