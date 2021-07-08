/* SPDX-License-Identifier: GPL-2.0 */
//
// Copyright (c) 2018 MediaTek Inc.

#ifndef __MTK_AIE_H__
#define __MTK_AIE_H__

#include <linux/completion.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/pm_opp.h>
#include <linux/regulator/consumer.h>
#include <linux/types.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/videobuf2-v4l2.h>

#include "mtk-interconnect.h"

#define V4L2_META_FMT_MTFD_RESULT  v4l2_fourcc('M', 'T', 'f', 'd')

#define MTK_AIE_OPP_SET			1
#define MTK_AIE_CLK_LEVEL_CNT		4

#define FD_VERSION 1946050
#define ATTR_VERSION 1929401

#define Y2R_CONFIG_SIZE 32
#define RS_CONFIG_SIZE 28
#define FD_CONFIG_SIZE 54

#define Y2R_SRC_DST_FORMAT 0
#define Y2R_IN_W_H 1
#define Y2R_OUT_W_H 2
#define Y2R_RA0_RA1_EN 3
#define Y2R_IN_X_Y_SIZE0 4
#define Y2R_IN_STRIDE0_BUS_SIZE0 5
#define Y2R_IN_X_Y_SIZE1 6
#define Y2R_IN_STRIDE1_BUS_SIZE1 7
#define Y2R_OUT_X_Y_SIZE0 8
#define Y2R_OUT_STRIDE0_BUS_SIZE0 9
#define Y2R_OUT_X_Y_SIZE1 10
#define Y2R_OUT_STRIDE1_BUS_SIZE1 11
#define Y2R_OUT_X_Y_SIZE2 12
#define Y2R_OUT_STRIDE2_BUS_SIZE2 13
#define Y2R_IN_0 14
#define Y2R_IN_1 15
#define Y2R_OUT_0 16
#define Y2R_OUT_1 17
#define Y2R_OUT_2 18
#define Y2R_RS_SEL_SRZ_EN 19
#define Y2R_X_Y_MAG 20
#define Y2R_SRZ_HORI_STEP 22
#define Y2R_SRZ_VERT_STEP 23
#define Y2R_PADDING_EN_UP_DOWN 26
#define Y2R_PADDING_RIGHT_LEFT 27
#define Y2R_CO2_FMT_MODE_EN 28 /* AIE3.0 new */
#define Y2R_CO2_CROP_X 29      /* AIE3.0 new */
#define Y2R_CO2_CROP_Y 30      /* AIE3.0 new */

#define RS_IN_0 22
#define RS_IN_1 23
#define RS_IN_2 24
#define RS_OUT_0 25
#define RS_OUT_1 26
#define RS_OUT_2 27
#define RS_X_Y_MAG 1
#define RS_SRZ_HORI_STEP 3
#define RS_SRZ_VERT_STEP 4
#define RS_INPUT_W_H 7
#define RS_OUTPUT_W_H 8
#define RS_IN_X_Y_SIZE0 10
#define RS_IN_STRIDE0 11
#define RS_IN_X_Y_SIZE1 12
#define RS_IN_STRIDE1 13
#define RS_IN_X_Y_SIZE2 14
#define RS_IN_STRIDE2 15
#define RS_OUT_X_Y_SIZE0 16
#define RS_OUT_STRIDE0 17
#define RS_OUT_X_Y_SIZE1 18
#define RS_OUT_STRIDE1 19
#define RS_OUT_X_Y_SIZE2 20
#define RS_OUT_STRIDE2 21

#define FD_INPUT_ROTATE 1 /* AIE3.0 new */
#define FD_CONV_WIDTH_MOD6 2
#define FD_CONV_IMG_W_H 4

#define FD_IN_IMG_W_H 5
#define FD_OUT_IMG_W_H 6

#define FD_IN_X_Y_SIZE0 9
#define FD_IN_X_Y_SIZE1 11
#define FD_IN_X_Y_SIZE2 13
#define FD_IN_X_Y_SIZE3 15

#define FD_IN_STRIDE0_BUS_SIZE0 10
#define FD_IN_STRIDE1_BUS_SIZE1 12
#define FD_IN_STRIDE2_BUS_SIZE2 14
#define FD_IN_STRIDE3_BUS_SIZE3 16

#define FD_OUT_X_Y_SIZE0 17
#define FD_OUT_X_Y_SIZE1 19
#define FD_OUT_X_Y_SIZE2 21
#define FD_OUT_X_Y_SIZE3 23

#define FD_OUT_STRIDE0_BUS_SIZE0 18
#define FD_OUT_STRIDE1_BUS_SIZE1 20
#define FD_OUT_STRIDE2_BUS_SIZE2 22
#define FD_OUT_STRIDE3_BUS_SIZE3 24

#define FD_IN_0 27
#define FD_IN_1 28
#define FD_IN_2 29
#define FD_IN_3 30

#define FD_OUT_0 31
#define FD_OUT_1 32
#define FD_OUT_2 33
#define FD_OUT_3 34

#define FD_KERNEL_0 35
#define FD_KERNEL_1 36

#define FD_RPN_SET 37
#define FD_IMAGE_COORD 38
#define FD_IMAGE_COORD_XY_OFST 39   /* AIE3.0 new */
#define FD_BIAS_ACCU 47		    /* AIE3.0 new */
#define FD_SRZ_FDRZ_RS 48	   /* AIE3.0 new */
#define FD_SRZ_HORI_STEP 49	 /* AIE3.0 new */
#define FD_SRZ_VERT_STEP 50	 /* AIE3.0 new */
#define FD_SRZ_HORI_SUB_INT_OFST 51 /* AIE3.0 new */
#define FD_SRZ_VERT_SUB_INT_OFST 52 /* AIE3.0 new */

#define FDRZ_BIT ((0x0 << 16) | (0x0 << 12) | (0x0 << 8) | 0x0)
#define SRZ_BIT ((0x1 << 16) | (0x1 << 12) | (0x1 << 8) | 0x1)

/* config size */
#define fd_rs_confi_size 224 /* AIE3.0: 112*2=224 */
#define fd_fd_confi_size 20736 /* AIE3.0: 54*4=216, 216*96=20736*/
#define fd_yuv2rgb_confi_size 128 /* AIE3.0:128 */

#define attr_fd_confi_size 5616     /* AIE3.0:216*26=5616 */
#define attr_yuv2rgb_confi_size 128 /* AIE3.0:128 */

#define result_size 49152 /* 384 * 1024 / 8 */ /* AIE2.0 and AIE3.0 */

#define fd_loop_num 87
#define rpn0_loop_num 86
#define rpn1_loop_num 57
#define rpn2_loop_num 28

#define pym0_start_loop 58
#define pym1_start_loop 29
#define pym2_start_loop 0

#define attr_loop_num 26
#define age_out_rgs 17
#define gender_out_rgs 20
#define indian_out_rgs 22
#define race_out_rgs 25

#define input_WDMA_WRA_num 4
#define output_WDMA_WRA_num 4
#define kernel_RDMA_RA_num 2

#define MAX_ENQUE_FRAME_NUM 10
#define PYM_NUM 3
#define COLOR_NUM 3

#define ATTR_MODE_PYRAMID_WIDTH 128
#define ATTR_OUT_SIZE 32

/* AIE 3.0 register offset */
#define AIE_START_REG 0x000
#define AIE_ENABLE_REG 0x004
#define AIE_LOOP_REG 0x008
#define AIE_YUV2RGB_CON_BASE_ADR_REG 0x00c
#define AIE_RS_CON_BASE_ADR_REG 0x010
#define AIE_FD_CON_BASE_ADR_REG 0x014
#define AIE_INT_EN_REG 0x018
#define AIE_INT_REG 0x01c
#define AIE_RESULT_0_REG 0x08c
#define AIE_RESULT_1_REG 0x090
#define AIE_DMA_CTL_REG 0x094

#define MTK_FD_OUTPUT_MIN_WIDTH 0U
#define MTK_FD_OUTPUT_MIN_HEIGHT 0U
#define MTK_FD_OUTPUT_MAX_WIDTH 4096U
#define MTK_FD_OUTPUT_MAX_HEIGHT 4096U

#define MTK_FD_HW_TIMEOUT 33 /* 33 msec */
#define MAX_FACE_NUM 1024
#define RLT_NUM 48
#define GENDER_OUT 32

#define RACE_RST_X_NUM 4
#define RACE_RST_Y_NUM 64
#define GENDER_RST_X_NUM 2
#define GENDER_RST_Y_NUM 64
#define MRACE_RST_NUM 4
#define MGENDER_RST_NUM 2
#define MAGE_RST_NUM 2
#define MINDIAN_RST_NUM 2

#define POSE_LOOP_NUM 3

struct aie_static_info {
	unsigned int fd_wdma_size[fd_loop_num][output_WDMA_WRA_num];
	unsigned int out_xsize_plus_1[fd_loop_num];
	unsigned int out_height[fd_loop_num];
	unsigned int out_ysize_plus_1_stride2[fd_loop_num];
	unsigned int out_stride[fd_loop_num];
	unsigned int out_stride_stride2[fd_loop_num];
	unsigned int out_width[fd_loop_num];
	unsigned int img_width[fd_loop_num];
	unsigned int img_height[fd_loop_num];
	unsigned int stride2_out_width[fd_loop_num];
	unsigned int stride2_out_height[fd_loop_num];
	unsigned int out_xsize_plus_1_stride2[fd_loop_num];
	unsigned int input_xsize_plus_1[fd_loop_num];
};

enum aie_state {
	STATE_NA = 0,
	STATE_INIT = 1
};

enum aie_mode { FDMODE = 0, ATTRIBUTEMODE = 1, POSEMODE = 2 };

enum aie_format {
	FMT_NA = 0,
	FMT_YUV_2P = 1,
	FMT_YVU_2P = 2,
	FMT_YUYV = 3,
	FMT_YVYU = 4,
	FMT_UYVY = 5,
	FMT_VYUY = 6,
	FMT_MONO = 7,
	FMT_YUV420_2P = 8,
	FMT_YUV420_1P = 9
};

enum aie_input_degree {
	DEGREE_0 = 0,
	DEGREE_90 = 1,
	DEGREE_270 = 2,
	DEGREE_180 = 3
};

struct aie_init_info {
	u16 max_img_width;
	u16 max_img_height;
	s16 feature_threshold;
	u16 pyramid_width;
	u16 pyramid_height;
	u32 is_secure;
	u32 sec_mem_type;
};

/* align v4l2 user space interface */
struct fd_result {
	u16 fd_pyramid0_num;
	u16 fd_pyramid1_num;
	u16 fd_pyramid2_num;
	u16 fd_total_num;
	u8 rpn31_rlt[MAX_FACE_NUM][RLT_NUM];
	u8 rpn63_rlt[MAX_FACE_NUM][RLT_NUM];
	u8 rpn95_rlt[MAX_FACE_NUM][RLT_NUM];
};

/* align v4l2 user space interface */
struct attr_result {
	u8 rpn17_rlt[GENDER_OUT];
	u8 rpn20_rlt[GENDER_OUT];
	u8 rpn22_rlt[GENDER_OUT];
	u8 rpn25_rlt[GENDER_OUT];
};

struct aie_roi {
	u32 x1;
	u32 y1;
	u32 x2;
	u32 y2;
};

struct aie_padding {
	u32 left;
	u32 right;
	u32 down;
	u32 up;
};

/* align v4l2 user space interface */
struct aie_enq_info {
	u32 sel_mode;
	u32 src_img_fmt;
	u16 src_img_width;
	u16 src_img_height;
	u16 src_img_stride;
	u32 pyramid_base_width;
	u32 pyramid_base_height;
	u32 number_of_pyramid;
	u32 rotate_degree;
	u32 en_roi;
	struct aie_roi src_roi;
	u32 en_padding;
	struct aie_padding src_padding;
	u32 freq_level;
	u32 src_img_addr;
	u32 src_img_addr_uv;
	u32 fd_version;
	u32 attr_version;
	u32 pose_version;
	struct fd_result fd_out;
	struct attr_result attr_out;
};

struct aie_reg_cfg {
	u32 rs_adr;
	u32 yuv2rgb_adr;
	u32 fd_adr;
	u32 fd_pose_adr;
	u32 fd_mode;
	u32 hw_result;
	u32 hw_result1;
};

struct aie_para {
	u32 sel_mode;
	u16 max_img_width;
	u16 max_img_height;
	u16 img_width;
	u16 img_height;
	u16 crop_width;
	u16 crop_height;
	u32 src_img_fmt;
	u32 rotate_degree;
	s16 rpn_anchor_thrd;
	u16 pyramid_width;
	u16 pyramid_height;
	u16 max_pyramid_width;
	u16 max_pyramid_height;
	u16 number_of_pyramid;
	u32 src_img_addr;
	u32 src_img_addr_uv;

	void *fd_fd_cfg_va;
	void *fd_rs_cfg_va;
	void *fd_yuv2rgb_cfg_va;
	void *fd_fd_pose_cfg_va;

	void *attr_fd_cfg_va[MAX_ENQUE_FRAME_NUM];
	void *attr_yuv2rgb_cfg_va[MAX_ENQUE_FRAME_NUM];

	void *rs_pym_rst_va[PYM_NUM][COLOR_NUM];

	dma_addr_t fd_fd_cfg_pa;
	dma_addr_t fd_rs_cfg_pa;
	dma_addr_t fd_yuv2rgb_cfg_pa;
	dma_addr_t fd_fd_pose_cfg_pa;

	dma_addr_t attr_fd_cfg_pa[MAX_ENQUE_FRAME_NUM];
	dma_addr_t attr_yuv2rgb_cfg_pa[MAX_ENQUE_FRAME_NUM];

	dma_addr_t rs_pym_rst_pa[PYM_NUM][COLOR_NUM];
};

struct aie_attr_para {
	u32 w_idx;
	u32 r_idx;
	u32 sel_mode[MAX_ENQUE_FRAME_NUM];
	u16 img_width[MAX_ENQUE_FRAME_NUM];
	u16 img_height[MAX_ENQUE_FRAME_NUM];
	u16 crop_width[MAX_ENQUE_FRAME_NUM];
	u16 crop_height[MAX_ENQUE_FRAME_NUM];
	u32 src_img_fmt[MAX_ENQUE_FRAME_NUM];
	u32 rotate_degree[MAX_ENQUE_FRAME_NUM];
	u32 src_img_addr[MAX_ENQUE_FRAME_NUM];
	u32 src_img_addr_uv[MAX_ENQUE_FRAME_NUM];
};

struct aie_fd_dma_para {
	void *fd_out_hw_va[fd_loop_num][output_WDMA_WRA_num];
	void *fd_kernel_va[fd_loop_num][kernel_RDMA_RA_num];
	void *attr_out_hw_va[attr_loop_num][output_WDMA_WRA_num];
	void *attr_kernel_va[attr_loop_num][kernel_RDMA_RA_num];

	void *age_out_hw_va[MAX_ENQUE_FRAME_NUM];
	void *gender_out_hw_va[MAX_ENQUE_FRAME_NUM];
	void *isIndian_out_hw_va[MAX_ENQUE_FRAME_NUM];
	void *race_out_hw_va[MAX_ENQUE_FRAME_NUM];

	void *fd_pose_out_hw_va[POSE_LOOP_NUM][output_WDMA_WRA_num];

	dma_addr_t fd_out_hw_pa[fd_loop_num][output_WDMA_WRA_num];
	dma_addr_t fd_kernel_pa[fd_loop_num][kernel_RDMA_RA_num];
	dma_addr_t attr_out_hw_pa[attr_loop_num][output_WDMA_WRA_num];
	dma_addr_t attr_kernel_pa[attr_loop_num][kernel_RDMA_RA_num];

	dma_addr_t age_out_hw_pa[MAX_ENQUE_FRAME_NUM];
	dma_addr_t gender_out_hw_pa[MAX_ENQUE_FRAME_NUM];
	dma_addr_t isIndian_out_hw_pa[MAX_ENQUE_FRAME_NUM];
	dma_addr_t race_out_hw_pa[MAX_ENQUE_FRAME_NUM];

	dma_addr_t fd_pose_out_hw_pa[POSE_LOOP_NUM][output_WDMA_WRA_num];
};

struct imem_buf_info {
	void *va;
	dma_addr_t pa;
	unsigned int size;
};

struct fd_buffer {
	__u32 dma_addr; /* used by DMA HW */
} __packed;

struct user_init {
	unsigned int max_img_width;
	unsigned int max_img_height;
	unsigned int pyramid_width;
	unsigned int pyramid_height;
	unsigned int feature_thread;
} __packed;
struct user_param {
	unsigned int fd_mode;
	unsigned int src_img_fmt;
	unsigned int src_img_width;
	unsigned int src_img_height;
	unsigned int src_img_stride;
	unsigned int pyramid_base_width;
	unsigned int pyramid_base_height;
	unsigned int number_of_pyramid;
	unsigned int rotate_degree;
	int en_roi;
	unsigned int src_roi_x1;
	unsigned int src_roi_y1;
	unsigned int src_roi_x2;
	unsigned int src_roi_y2;
	int en_padding;
	unsigned int src_padding_left;
	unsigned int src_padding_right;
	unsigned int src_padding_down;
	unsigned int src_padding_up;
	unsigned int freq_level;
} __packed;

struct mtk_aie_user_para {
	signed int feature_threshold;
	unsigned int is_secure;
	unsigned int sec_mem_type;
	struct user_param user_param;
} __packed;

struct fd_enq_param {
	struct fd_buffer src_img[2];
	struct fd_buffer user_result;
	struct user_param user_param;
} __packed;

struct mtk_aie_dvfs {
	struct device *dev;
	struct regulator *reg;
	unsigned int clklv_num[MTK_AIE_OPP_SET];
	unsigned int clklv[MTK_AIE_OPP_SET][MTK_AIE_CLK_LEVEL_CNT];
	unsigned int voltlv[MTK_AIE_OPP_SET][MTK_AIE_CLK_LEVEL_CNT];
	unsigned int clklv_idx[MTK_AIE_OPP_SET];
	unsigned int clklv_target[MTK_AIE_OPP_SET];
	unsigned int cur_volt;
};

struct mtk_aie_qos_path {
	struct icc_path *path;	/* cmdq event enum value */
	char dts_name[256];
	unsigned long long bw;
};

struct mtk_aie_qos {
	struct device *dev;
	struct mtk_aie_qos_path *qos_path;
};

struct mtk_aie_req_work {
	struct work_struct work;
	struct mtk_aie_dev *fd_dev;
};

struct mtk_aie_dev {
	struct device *dev;
	struct mtk_aie_ctx *ctx;
	struct v4l2_device v4l2_dev;
	struct v4l2_m2m_dev *m2m_dev;
	struct media_device mdev;
	struct video_device vfd;
	struct clk *img_ipe;
	struct clk *ipe_fdvt;
	struct clk *ipe_smi_larb12;
	struct clk *ipe_top;
	struct device *larb;

	/* Lock for V4L2 operations */
	struct mutex vfd_lock;

	void __iomem *fd_base;

	u32 fd_stream_count;
	struct completion fd_job_finished;
	struct delayed_work job_timeout_work;

	struct aie_enq_info *aie_cfg;
	struct aie_reg_cfg reg_cfg;

	/* Input Buffer Pointer */
	struct imem_buf_info rs_cfg_data;
	struct imem_buf_info fd_cfg_data;
	struct imem_buf_info pose_cfg_data;
	struct imem_buf_info yuv2rgb_cfg_data;
	/* HW Output Buffer Pointer */
	struct imem_buf_info rs_output_hw;
	struct imem_buf_info fd_dma_hw;
	struct imem_buf_info fd_dma_result_hw;
	struct imem_buf_info fd_kernel_hw;
	struct imem_buf_info fd_attr_dma_hw;

	/* DRAM Buffer Size */
	unsigned int fd_rs_cfg_size;
	unsigned int fd_fd_cfg_size;
	unsigned int fd_yuv2rgb_cfg_size;
	unsigned int fd_pose_cfg_size;
	unsigned int attr_fd_cfg_size;
	unsigned int attr_yuv2rgb_cfg_size;

	/* HW Output Buffer Size */
	unsigned int rs_pym_out_size[PYM_NUM];
	unsigned int fd_dma_max_size;
	unsigned int fd_dma_rst_max_size;
	unsigned int fd_fd_kernel_size;
	unsigned int fd_attr_kernel_size;
	unsigned int fd_attr_dma_max_size;
	unsigned int fd_attr_dma_rst_max_size;

	unsigned int pose_height;

	/*DMA Buffer*/
	struct dma_buf *dmabuf;
	unsigned long long kva;
	int map_count;

	struct aie_para *base_para;
	struct aie_attr_para *attr_para;
	struct aie_fd_dma_para *dma_para;

	struct aie_static_info st_info;
	unsigned int fd_state;
	struct mtk_aie_dvfs dvfs_info;
	struct mtk_aie_qos qos_info;

	wait_queue_head_t flushing_waitq;
	atomic_t num_composing;

	struct workqueue_struct *frame_done_wq;
	struct mtk_aie_req_work req_work;
};

struct mtk_aie_ctx {
	struct mtk_aie_dev *fd_dev;
	struct device *dev;
	struct v4l2_fh fh;
	struct v4l2_ctrl_handler hdl;
	struct v4l2_pix_format_mplane src_fmt;
	struct v4l2_meta_format dst_fmt;
};

/**************************************************************************/
/*                   C L A S S    D E C L A R A T I O N                   */
/**************************************************************************/

void aie_reset(struct mtk_aie_dev *fd);
int aie_init(struct mtk_aie_dev *fd, struct aie_init_info init_info);
void aie_uninit(struct mtk_aie_dev *fd);
int aie_prepare(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
void aie_execute(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
void aie_execute_pose(struct mtk_aie_dev *fd);
void aie_irqhandle(struct mtk_aie_dev *fd);
void aie_get_fd_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);
void aie_get_attr_result(struct mtk_aie_dev *fd, struct aie_enq_info *aie_cfg);

#endif /*__MTK_AIE_H__*/
