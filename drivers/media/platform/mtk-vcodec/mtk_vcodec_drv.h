/* SPDX-License-Identifier: GPL-2.0 */
/*
* Copyright (c) 2016 MediaTek Inc.
* Author: PC Chen <pc.chen@mediatek.com>
*         Tiffany Lin <tiffany.lin@mediatek.com>
*/

#ifndef _MTK_VCODEC_DRV_H_
#define _MTK_VCODEC_DRV_H_

#include <linux/platform_device.h>
#include <linux/videodev2.h>
#include <linux/semaphore.h>
#include <linux/regulator/consumer.h>
#include <linux/interconnect-provider.h>
#include <linux/types.h>
#include <linux/list.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-ioctl.h>
#include <media/videobuf2-core.h>
#include "mtk_vcodec_util.h"
#include "vcodec_ipi_msg.h"
#include "mtk_vcodec_pm.h"
#include "vcodec_dvfs.h"
#include "vcodec_bw.h"
#include "mtk_dma_contig.h"
#include "slbc_ops.h"

#define MTK_VCODEC_DRV_NAME     "mtk_vcodec_drv"
#define MTK_VCODEC_DEC_NAME     "mtk-vcodec-dec"
#define MTK_VCODEC_ENC_NAME     "mtk-vcodec-enc"
#define MTK_VCU_FW_VERSION      "0.2.14"

#define MTK_SLOWMOTION_GCE_TH   120
#define MTK_VCODEC_MAX_PLANES   3
#define WAIT_INTR_TIMEOUT_MS    500
#define SUSPEND_TIMEOUT_CNT     5000
#define MTK_MAX_CTRLS_HINT      64
#define V4L2_BUF_FLAG_OUTPUT_NOT_GENERATED 0x02000000

#define MAX_CODEC_FREQ_STEP	10
#define MTK_VDEC_PORT_NUM	32
#define MTK_VENC_PORT_NUM	64
#define MTK_MAX_METADATA_NUM    8

#define DEBUG_GKI 1

enum mtk_vcodec_ipm {
	VCODEC_IPM_V1 = 1,
	VCODEC_IPM_V2 = 2,
	VCODEC_IPM_MAX,
};

/**
 * enum mtk_instance_type - The type of an MTK Vcodec instance.
 */
enum mtk_instance_type {
	MTK_INST_DECODER                = 0,
	MTK_INST_ENCODER                = 1,
};

/**
 * enum mtk_instance_state - The state of an MTK Vcodec instance.
 * @MTK_STATE_FREE - default state when instance is created
 * @MTK_STATE_INIT - vcodec instance is initialized
 * @MTK_STATE_HEADER - vdec had sps/pps header parsed or venc
 *                      had sps/pps header encoded
 * @MTK_STATE_FLUSH - vdec is flushing. Only used by decoder
 * @MTK_STATE_ABORT - vcodec should be aborted
 */
enum mtk_instance_state {
	MTK_STATE_FREE = 0,
	MTK_STATE_INIT = 1,
	MTK_STATE_HEADER = 2,
	MTK_STATE_FLUSH = 3,
	MTK_STATE_ABORT = 4,
};

enum mtk_codec_type {
	VDEC_VENC_UNKNOWN = 0,
	VDEC_H264,
	VDEC_H265,
	VDEC_HEIF,
	VDEC_VP8,
	VDEC_VP9,
	VDEC_MPEG4,
	VDEC_H263,
	VDEC_MPEG12,
	VDEC_WMV,
	VDEC_RV30,
	VDEC_RV40,
	VDEC_AV1,
	VENC_H264,
	VENC_H265,
	VENC_HEIF,
	VENC_VP8,
	VENC_MPEG4,
	VENC_HYBRID_H264,
	VENC_H263,
	VDEC_VENC_MAX
};

/**
 * struct mtk_encode_param - General encoding parameters type
 */
enum mtk_encode_param {
	MTK_ENCODE_PARAM_NONE = 0,
	MTK_ENCODE_PARAM_BITRATE = (1 << 0),
	MTK_ENCODE_PARAM_FRAMERATE = (1 << 1),
	MTK_ENCODE_PARAM_INTRA_PERIOD = (1 << 2),
	MTK_ENCODE_PARAM_FORCE_INTRA = (1 << 3),
	MTK_ENCODE_PARAM_GOP_SIZE = (1 << 4),
	MTK_ENCODE_PARAM_SCENARIO = (1 << 5),
	MTK_ENCODE_PARAM_NONREFP = (1 << 6),
	MTK_ENCODE_PARAM_DETECTED_FRAMERATE = (1 << 7),
	MTK_ENCODE_PARAM_RFS_ON = (1 << 8),
	MTK_ENCODE_PARAM_PREPEND_SPSPPS_TO_IDR = (1 << 9),
	MTK_ENCODE_PARAM_OPERATION_RATE = (1 << 10),
	MTK_ENCODE_PARAM_BITRATE_MODE = (1 << 11),
	MTK_ENCODE_PARAM_ROI_ON = (1 << 12),
	MTK_ENCODE_PARAM_GRID_SIZE = (1 << 13),
	MTK_ENCODE_PARAM_COLOR_DESC = (1 << 14),
	MTK_ENCODE_PARAM_SEC_ENCODE = (1 << 15),
	MTK_ENCODE_PARAM_TSVC = (1 << 16),
	MTK_ENCODE_PARAM_NONREFPFREQ = (1 << 17),
	MTK_ENCODE_PARAM_HIGHQUALITY = (1 << 18),
	MTK_ENCODE_PARAM_MAXQP = (1 << 19),
	MTK_ENCODE_PARAM_MINQP = (1 << 20),
	MTK_ENCODE_PARAM_FRAMELVLQP = (1 << 21),
	MTK_ENCODE_PARAM_IP_QPDELTA = (1 << 22),
	MTK_ENCODE_PARAM_QP_CTRL_MODE = (1 << 23),
	MTK_ENCODE_PARAM_DUMMY_NAL = (1 << 24),
};

/*
 * enum venc_yuv_fmt - The type of input yuv format
 * (VCU related: If you change the order, you must also update the VCU codes.)
 * @VENC_YUV_FORMAT_I420: I420 YUV format
 * @VENC_YUV_FORMAT_YV12: YV12 YUV format
 * @VENC_YUV_FORMAT_NV12: NV12 YUV format
 * @VENC_YUV_FORMAT_NV21: NV21 YUV format
 */
enum venc_yuv_fmt {
	VENC_YUV_FORMAT_I420 = 3,
	VENC_YUV_FORMAT_YV12 = 5,
	VENC_YUV_FORMAT_NV12 = 6,
	VENC_YUV_FORMAT_NV21 = 7,
	VENC_YUV_FORMAT_24bitRGB888 = 11,
	VENC_YUV_FORMAT_24bitBGR888 = 12,
	VENC_YUV_FORMAT_32bitRGBA8888 = 13,
	VENC_YUV_FORMAT_32bitBGRA8888 = 14,
	VENC_YUV_FORMAT_32bitARGB8888 = 15,
	VENC_YUV_FORMAT_32bitABGR8888 = 16,
	VENC_YUV_FORMAT_32bitRGBA1010102 = 17,
	VENC_YUV_FORMAT_32bitBGRA1010102 = 18,
	VENC_YUV_FORMAT_32bitARGB1010102 = 19,
	VENC_YUV_FORMAT_32bitABGR1010102 = 20,
	VENC_YUV_FORMAT_32bitRGBA8888_AFBC = 21,
	VENC_YUV_FORMAT_32bitBGRA8888_AFBC = 22,
	VENC_YUV_FORMAT_32bitRGBA1010102_AFBC = 23,
	VENC_YUV_FORMAT_32bitBGRA1010102_AFBC = 24,
	VENC_YUV_FORMAT_MT10 = 25,
	VENC_YUV_FORMAT_P010 = 26,
	VENC_YUV_FORMAT_NV12_AFBC = 27,
	VENC_YUV_FORMAT_NV12_10B_AFBC = 28,
	VENC_YUV_FORMAT_NV21_AFBC = 29,
};

/**
 * struct mtk_q_type - Type of queue
 */
enum mtk_q_type {
	MTK_Q_DATA_SRC = 0,
	MTK_Q_DATA_DST = 1,
};

/**
 * enum input_driven_mode  - decoder different input driven mode
 * @NON_INPUT_DRIVEN     : non input driven, bs/frm pairwise trigger ipi
 * @INPUT_DRIVEN_CB_FRM      : input driven, codec callback get frame
 * @INPUT_DRIVEN_PUT_FRM : input driven, v4l2 initiatively put frame to codec
 */
enum vdec_input_driven_mode {
	NON_INPUT_DRIVEN = 0,
	INPUT_DRIVEN_CB_FRM = 1,
	INPUT_DRIVEN_PUT_FRM = 2,
};

/**
 * struct mtk_q_data - Structure used to store information about queue
 */
struct mtk_q_data {
	unsigned int    visible_width;
	unsigned int    visible_height;
	unsigned int    coded_width;
	unsigned int    coded_height;
	enum v4l2_field field;
	unsigned int    bytesperline[MTK_VCODEC_MAX_PLANES];
	unsigned int    sizeimage[MTK_VCODEC_MAX_PLANES];
	struct mtk_video_fmt    *fmt;
};

enum mtk_dec_param {
	MTK_DEC_PARAM_NONE = 0,
	MTK_DEC_PARAM_DECODE_MODE = (1 << 0),
	MTK_DEC_PARAM_FRAME_SIZE = (1 << 1),
	MTK_DEC_PARAM_FIXED_MAX_FRAME_SIZE = (1 << 2),
	MTK_DEC_PARAM_CRC_PATH = (1 << 3),
	MTK_DEC_PARAM_GOLDEN_PATH = (1 << 4),
	MTK_DEC_PARAM_WAIT_KEY_FRAME = (1 << 5),
	MTK_DEC_PARAM_NAL_SIZE_LENGTH = (1 << 6),
	MTK_DEC_PARAM_FIXED_MAX_OUTPUT_BUFFER = (1 << 7),
	MTK_DEC_PARAM_SEC_DECODE = (1 << 8),
	MTK_DEC_PARAM_OPERATING_RATE = (1 << 9)
};

enum venc_lock {
	VENC_LOCK_NONE,
	VENC_LOCK_NORMAL,
	VENC_LOCK_SEC
};

struct mtk_dec_params {
	unsigned int    decode_mode;
	unsigned int    frame_size_width;
	unsigned int    frame_size_height;
	unsigned int    fixed_max_frame_size_width;
	unsigned int    fixed_max_frame_size_height;
	char            *crc_path;
	char            *golden_path;
	unsigned int    fb_num_planes;
	unsigned int	wait_key_frame;
	unsigned int	nal_size_length;
	unsigned int	svp_mode;
	unsigned int	operating_rate;
	u64	timestamp;
	unsigned int	total_frame_bufq_count;
	unsigned int	queued_frame_buf_count;
	int		priority;
};

/**
 * struct mtk_enc_params - General encoding parameters
 * @bitrate: target bitrate in bits per second
 * @num_b_frame: number of b frames between p-frame
 * @rc_frame: frame based rate control
 * @rc_mb: macroblock based rate control
 * @seq_hdr_mode: H.264 sequence header is encoded separately or joined
 *                with the first frame
 * @intra_period: I frame period
 * @gop_size: group of picture size, it's used as the intra frame period
 * @framerate_num: frame rate numerator. ex: framerate_num=30 and
 *                 framerate_denom=1 menas FPS is 30
 * @framerate_denom: frame rate denominator. ex: framerate_num=30 and
 *                   framerate_denom=1 menas FPS is 30
 * @h264_max_qp: Max value for H.264 quantization parameter
 * @h264_profile: V4L2 defined H.264 profile
 * @h264_level: V4L2 defined H.264 level
 * @force_intra: force/insert intra frame
 */
struct mtk_enc_params {
	unsigned int    bitrate;
	unsigned int    num_b_frame;
	unsigned int    rc_frame;
	unsigned int    rc_mb;
	unsigned int    seq_hdr_mode;
	unsigned int    intra_period;
	unsigned int    gop_size;
	unsigned int    framerate_num;
	unsigned int    framerate_denom;
	unsigned int    h264_max_qp;
	unsigned int    profile;
	unsigned int    level;
	unsigned int    tier;
	unsigned int    force_intra;
	unsigned int    scenario;
	unsigned int    nonrefp;
	unsigned int    detectframerate;
	unsigned int    rfs;
	unsigned int    prependheader;
	unsigned int    operationrate;
	unsigned int    bitratemode;
	unsigned int    roion;
	unsigned int    heif_grid_size;
	struct mtk_color_desc color_desc; // data from userspace
	unsigned int    max_w;
	unsigned int    max_h;
	unsigned int    slbc_ready;
	unsigned int    i_qp;
	unsigned int    p_qp;
	unsigned int    b_qp;
	unsigned int    svp_mode;
	unsigned int    tsvc;
	unsigned int    nonrefpfreq;
	unsigned int    highquality;
	int             max_qp;
	int             min_qp;
	int             framelvl_qp;
	int             ip_qpdelta;
	unsigned int	qp_control_mode;
	unsigned int	dummynal;
	int		priority;
};

/*
 * struct venc_enc_prm - encoder settings for VENC_SET_PARAM_ENC used in
 *                                        venc_if_set_param()
 * @input_fourcc: input yuv format
 * @h264_profile: V4L2 defined H.264 profile
 * @h264_level: V4L2 defined H.264 level
 * @width: image width
 * @height: image height
 * @buf_width: buffer width
 * @buf_height: buffer height
 * @frm_rate: frame rate in fps
 * @intra_period: intra frame period
 * @bitrate: target bitrate in bps
 * @gop_size: group of picture size
 * @sizeimage: image size for each plane
 */
struct venc_enc_param {
	enum venc_yuv_fmt input_yuv_fmt;
	unsigned int profile;
	unsigned int level;
	unsigned int tier;
	unsigned int width;
	unsigned int height;
	unsigned int buf_width;
	unsigned int buf_height;
	unsigned int frm_rate;
	unsigned int intra_period;
	unsigned int bitrate;
	unsigned int gop_size;
	unsigned int scenario;
	unsigned int nonrefp;
	unsigned int detectframerate;
	unsigned int rfs;
	unsigned int prependheader;
	unsigned int operationrate;
	unsigned int bitratemode;
	unsigned int roion;
	unsigned int heif_grid_size;
	// pointed to mtk_enc_params::color_desc
	struct mtk_color_desc *color_desc;
	unsigned int sizeimage[MTK_VCODEC_MAX_PLANES];
	unsigned int max_w;
	unsigned int max_h;
	unsigned int num_b_frame;
	unsigned int slbc_ready;
	unsigned int i_qp;
	unsigned int p_qp;
	unsigned int b_qp;
	unsigned int svp_mode;
	unsigned int tsvc;
	unsigned int nonrefpfreq;
	unsigned int highquality;
	unsigned int max_qp;
	unsigned int min_qp;
	unsigned int framelvl_qp;
	unsigned int qp_control_mode;
	unsigned int ip_qpdelta;
	unsigned int dummynal;
	unsigned int slbc_addr;
	char set_vcp_buf[1024];
	char property_buf[1024];
};

/*
 * struct venc_frm_buf - frame buffer information used in venc_if_encode()
 * @fb_addr: plane frame buffer addresses
 * @num_planes: frmae buffer plane num
 */
struct venc_frm_buf {
	struct mtk_vcodec_mem fb_addr[MTK_VCODEC_MAX_PLANES];
	unsigned int num_planes;
	u64 timestamp;
	bool has_meta;
	struct dma_buf *meta_dma;
	struct dma_buf_attachment *buf_att;
	struct sg_table *sgt;
	dma_addr_t meta_addr;
	struct dma_buf_attachment *qpmap_dma_att;
	struct sg_table *qpmap_sgt;
	unsigned int meta_offset;
	bool has_qpmap;
	struct dma_buf *qpmap_dma;
	dma_addr_t qpmap_dma_addr;
	struct dma_buf *metabuffer_dma;
	dma_addr_t metabuffer_addr;
};

enum metadata_type {
	METADATA_HDR               = 0,
	METADATA_QPMAP             = 1
};

struct meta_describe {
	uint8_t invalid;  //1: valid 0:invalid
	uint8_t fd_flag;  //whether pass with fd - 1:yes 0:no
	uint32_t type;
	uint32_t size;  //size of metadata (total in 32 bits length)
	uint32_t value; //fd number or memory offset from the begginning of metadata buffer
};

struct metadata_info {
	struct meta_describe metadata_dsc[MTK_MAX_METADATA_NUM];
};

/**
 * struct mtk_vcodec_ctx - Context (instance) private data.
 *
 * @type: type of the instance - decoder or encoder
 * @dev: pointer to the mtk_vcodec_dev of the device
 * @list: link to ctx_list of mtk_vcodec_dev
 * @fh: struct v4l2_fh
 * @m2m_ctx: pointer to the v4l2_m2m_ctx of the context
 * @q_data: store information of input and output queue
 *          of the context
 * @id: index of the context that this structure describes
 * @state: state of the context
 * @dec_param_change: indicate decode parameter type
 * @dec_params: decoding parameters
 * @param_change: indicate encode parameter type
 * @enc_params: encoding parameters
 * @dec_if: hooked decoder driver interface
 * @enc_if: hoooked encoder driver interface
 * @drv_handle: driver handle for specific decode/encode instance
 *
 * @picinfo: store picture info after header parsing
 * @dpb_size: store dpb count after header parsing
 * @int_cond: variable used by the waitqueue
 * @int_type: type of the last interrupt
 * @queue: waitqueue that can be used to wait for this context to
 *         finish
 * @irq_status: irq status
 *
 * @ctrl_hdl: handler for v4l2 framework
 * @decode_work: worker for the decoding
 * @encode_work: worker for the encoding
 * @last_decoded_picinfo: pic information get from latest decode
 * @dec_flush_buf: a fake size-1 output buffer that indicates flush
 * @enc_flush_buf: a fake size-1 output buffer that indicates flush
 * @oal_vcodec: 1: oal encoder, 0:non-oal encoder
 * @pend_src_buf: pending source buffer
 *
 * @colorspace: enum v4l2_colorspace; supplemental to pixelformat
 * @ycbcr_enc: enum v4l2_ycbcr_encoding, Y'CbCr encoding
 * @quantization: enum v4l2_quantization, colorspace quantization
 * @xfer_func: enum v4l2_xfer_func, colorspace transfer function
 * @lock: protect variables accessed by V4L2 threads and worker thread such as
 *        mtk_video_dec_buf.
 */
struct mtk_vcodec_ctx {
	enum mtk_instance_type type;
	struct mtk_vcodec_dev *dev;
	struct list_head list;

	struct v4l2_fh fh;
	struct v4l2_m2m_ctx *m2m_ctx;
	struct mtk_q_data q_data[2];
	int id;
	enum mtk_instance_state state;
	enum mtk_dec_param dec_param_change;
	struct mtk_dec_params dec_params;
	enum mtk_encode_param param_change;
	struct mtk_enc_params enc_params;

	const struct vdec_common_if *dec_if;
	const struct venc_common_if *enc_if;
	unsigned long drv_handle;

	struct vdec_pic_info picinfo;
	int dpb_size;
	int last_dpb_size;
	int is_hdr;
	int last_is_hdr;
	unsigned int errormap_info[VB2_MAX_FRAME];
	s64 input_max_ts;

	bool is_flushing;
	unsigned int eos_type;
	u64 early_eos_ts;

	int int_cond[MTK_VDEC_HW_NUM];
	int int_type;
	wait_queue_head_t queue[MTK_VDEC_HW_NUM];
	unsigned int irq_status;

	struct v4l2_ctrl_handler ctrl_hdl;
	struct work_struct decode_work;
	struct work_struct encode_work;
	struct vdec_pic_info last_decoded_picinfo;
	struct mtk_video_dec_buf *dec_flush_buf;
	struct mtk_video_enc_buf *enc_flush_buf;
	struct vb2_buffer *pend_src_buf;
	wait_queue_head_t fm_wq;
	unsigned char *ipi_blocked;
	enum vdec_input_driven_mode input_driven;

	/* for user lock HW case release check */
	struct mutex hw_status;
	int hw_locked[MTK_VDEC_HW_NUM];
	int core_locked[MTK_VENC_HW_NUM];
	int async_mode;
	int oal_vcodec;

	enum v4l2_colorspace colorspace;
	enum v4l2_ycbcr_encoding ycbcr_enc;
	enum v4l2_quantization quantization;
	enum v4l2_xfer_func xfer_func;

	int init_cnt;
	int decoded_frame_cnt;
	struct mutex buf_lock;
	struct mutex worker_lock;
	struct slbc_data sram_data;
	struct mutex q_mutex;
	int use_slbc;
	unsigned int slbc_addr;
};

/*
 * struct venc_frm_buf - frame buffer information used in venc_if_encode()
 * @fb_addr: plane frame buffer addresses
 * @num_planes: frmae buffer plane num
 */
struct venc_larb_port {
	unsigned int total_port_num;
	unsigned int port_id[MTK_VENC_PORT_NUM];
	unsigned int ram_type[MTK_VENC_PORT_NUM];
};


/**
 * struct mtk_vcodec_dev - driver data
 * @v4l2_dev: V4L2 device to register video devices for.
 * @vfd_dec: Video device for decoder
 * @vfd_enc: Video device for encoder.
 *
 * @m2m_dev_dec: m2m device for decoder
 * @m2m_dev_enc: m2m device for encoder.
 * @plat_dev: platform device
 * @vcu_plat_dev: mtk vcu platform device
 * @ctx_list: list of struct mtk_vcodec_ctx
 * @irqlock: protect data access by irq handler and work thread
 * @curr_ctx: The context that is waiting for codec hardware
 *
 * @reg_base: Mapped address of MTK Vcodec registers.
 *
 * @id_counter: used to identify current opened instance
 *
 * @encode_workqueue: encode work queue
 *
 * @int_cond: used to identify interrupt condition happen
 * @int_type: used to identify what kind of interrupt condition happen
 * @dev_mutex: video_device lock
 * @queue: waitqueue for waiting for completion of device commands
 *
 * @dec_irq: decoder irq resource
 * @enc_irq: h264 encoder irq resource
 * @enc_lt_irq: vp8 encoder irq resource
 *
 * @dec_sem: decoder hw lock. Use sem for gce different thread lock unlock
 * @enc_sem: encoder hw lock. Use sem for gce different thread lock unlock
 *
 * @pm: power management control
 * @dec_capability: used to identify decode capability, ex: 4k
 * @enc_capability: used to identify encode capability
 */
struct mtk_vcodec_dev {
	struct v4l2_device v4l2_dev;
	struct video_device *vfd_dec;
	struct video_device *vfd_enc;
	struct iommu_domain *io_domain;

	const char *platform;
	enum mtk_vcodec_ipm vdec_hw_ipm;
	enum mtk_vcodec_ipm venc_hw_ipm;

	struct v4l2_m2m_dev *m2m_dev_dec;
	struct v4l2_m2m_dev *m2m_dev_enc;
	struct platform_device *plat_dev;
	struct platform_device *vcu_plat_dev;
	struct list_head ctx_list;
	struct notifier_block vcp_notify;
	spinlock_t irqlock;
	struct mtk_vcodec_ctx *curr_dec_ctx[MTK_VDEC_HW_NUM];
	struct mtk_vcodec_ctx *curr_enc_ctx[MTK_VENC_HW_NUM];
	void __iomem *dec_reg_base[NUM_MAX_VDEC_REG_BASE];
	void __iomem *enc_reg_base[NUM_MAX_VENC_REG_BASE];

	bool dec_is_power_on[MTK_VDEC_HW_NUM];
	bool enc_is_power_on[MTK_VENC_HW_NUM];
	spinlock_t dec_power_lock[MTK_VDEC_HW_NUM];
	spinlock_t enc_power_lock[MTK_VENC_HW_NUM];
	int dec_m4u_ports[NUM_MAX_VDEC_M4U_PORT];

	unsigned long id_counter;

	struct workqueue_struct *decode_workqueue;
	struct workqueue_struct *encode_workqueue;
	int int_cond;
	int int_type;
	struct mutex ctx_mutex;
	struct mutex dev_mutex;
	struct mutex ipi_mutex;
	struct mutex ipi_mutex_res;
	struct mtk_vcodec_msgq mq;

	int dec_irq[MTK_VDEC_HW_NUM];
	int enc_irq[MTK_VENC_HW_NUM];
	int enc_lt_irq;

	struct semaphore dec_sem[MTK_VDEC_HW_NUM];
	struct semaphore enc_sem[MTK_VENC_HW_NUM];

	struct mutex dec_dvfs_mutex;
	struct mutex enc_dvfs_mutex;

	struct mtk_vcodec_pm pm;
	struct notifier_block pm_notifier;
	bool is_codec_suspending;

	int dec_cnt;
	int enc_cnt;

	struct share_obj dec_ipi_data;
	struct share_obj enc_ipi_data;
	int *dec_mem_slot_stat;
	int *enc_mem_slot_stat;

	int dec_hw_cnt;
	int enc_hw_cnt;

	struct plist_head vdec_rlist[MTK_VDEC_HW_NUM];
	struct plist_head venc_rlist[MTK_VENC_HW_NUM];
	struct icc_path *vdec_qos_req[MTK_VDEC_PORT_NUM];
	struct icc_path *venc_qos_req[MTK_VENC_PORT_NUM];

	int vdec_freq_cnt;
	int venc_freq_cnt;
	unsigned long vdec_freqs[MAX_CODEC_FREQ_STEP];
	unsigned long venc_freqs[MAX_CODEC_FREQ_STEP];

	struct regulator *vdec_reg;
	struct regulator *venc_reg;
	struct venc_larb_port venc_ports[MTK_VENC_HW_NUM];

	int vdec_op_rate_cnt;
	//int venc_op_rate_cnt;
	int vdec_tput_cnt;
	int venc_tput_cnt;
	int vdec_cfg_cnt;
	int venc_cfg_cnt;
	int vdec_port_cnt;
	int venc_port_cnt;
	int vdec_port_idx[MTK_VDEC_HW_NUM];
	int venc_port_idx[MTK_VENC_HW_NUM];
	struct vcodec_perf *vdec_tput;
	struct vcodec_perf *venc_tput;
	//struct vcodec_config *vdec_cfg;
	struct vcodec_config *venc_cfg;
	struct vcodec_op_rate *vdec_dflt_op_rate;
	//struct vcodec_op_rate *venc_dflt_op_rate;
	struct list_head vdec_dvfs_inst;
	struct list_head venc_dvfs_inst;
	struct dvfs_params vdec_dvfs_params;
	struct dvfs_params venc_dvfs_params;
	struct vcodec_port_bw *vdec_port_bw;
	struct vcodec_port_bw *venc_port_bw;
/**
 *	struct ion_client *ion_vdec_client;
 *	struct ion_client *ion_venc_client;
 **/
	struct list_head log_param_list;
	struct list_head prop_param_list;
	struct mutex log_param_mutex;
	struct mutex prop_param_mutex;
	enum venc_lock enc_hw_locked[MTK_VENC_HW_NUM];

	unsigned int svp_mtee;
	unsigned int unique_domain;
};

static inline struct mtk_vcodec_ctx *fh_to_ctx(struct v4l2_fh *fh)
{
	return container_of(fh, struct mtk_vcodec_ctx, fh);
}

static inline struct mtk_vcodec_ctx *ctrl_to_ctx(struct v4l2_ctrl *ctrl)
{
	return container_of(ctrl->handler, struct mtk_vcodec_ctx, ctrl_hdl);
}

    /* 10-bit for RGB, 2-bit for A; 32-bit per-pixel;  */
#define V4L2_PIX_FMT_ARGB1010102  v4l2_fourcc('A', 'B', '3', '0')
#define V4L2_PIX_FMT_ABGR1010102  v4l2_fourcc('A', 'R', '3', '0')
#define V4L2_PIX_FMT_RGBA1010102  v4l2_fourcc('R', 'A', '3', '0')
#define V4L2_PIX_FMT_BGRA1010102  v4l2_fourcc('B', 'A', '3', '0')

    /* 10-bit; each pixel needs 2 bytes, and LSB 6-bit is not used */
#define V4L2_PIX_FMT_P010M   v4l2_fourcc('P', '0', '1', '0')

#define V4L2_PIX_FMT_H264_SLICE v4l2_fourcc('S', '2', '6', '4') /* H264 parsed slices */
#define V4L2_PIX_FMT_H265     v4l2_fourcc('H', '2', '6', '5') /* H265 with start codes */
#define V4L2_PIX_FMT_HEIF     v4l2_fourcc('H', 'E', 'I', 'F') /* HEIF */

#define V4L2_PIX_FMT_VP8_FRAME v4l2_fourcc('V', 'P', '8', 'F') /* VP8 parsed frames */
#define V4L2_PIX_FMT_WMV1     v4l2_fourcc('W', 'M', 'V', '1') /* WMV7 */
#define V4L2_PIX_FMT_WMV2     v4l2_fourcc('W', 'M', 'V', '2') /* WMV8 */
#define V4L2_PIX_FMT_WMV3     v4l2_fourcc('W', 'M', 'V', '3') /* WMV9 */
#define V4L2_PIX_FMT_WMVA     v4l2_fourcc('W', 'M', 'V', 'A') /* WMVA */
#define V4L2_PIX_FMT_WVC1     v4l2_fourcc('W', 'V', 'C', '1') /* VC1 */
#define V4L2_PIX_FMT_RV30     v4l2_fourcc('R', 'V', '3', '0') /* RealVideo 8 */
#define V4L2_PIX_FMT_RV40     v4l2_fourcc('R', 'V', '4', '0') /* RealVideo 9/10 */
#define V4L2_PIX_FMT_AV1      v4l2_fourcc('A', 'V', '1', '0') /* AV1 */

	/* Mediatek compressed block mode  */
#define V4L2_PIX_FMT_MT21         v4l2_fourcc('M', 'M', '2', '1')
	/* MTK 8-bit block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT2110T      v4l2_fourcc('M', 'T', '2', 'T')
	/* MTK 10-bit tile block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT2110R      v4l2_fourcc('M', 'T', '2', 'R')
	/* MTK 10-bit raster block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT21C10T     v4l2_fourcc('M', 'T', 'C', 'T')
	/* MTK 10-bit tile compressed block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT21C10R     v4l2_fourcc('M', 'T', 'C', 'R')
	/* MTK 10-bit raster compressed block mode, two non-contiguous planes */
#define V4L2_PIX_FMT_MT21CS       v4l2_fourcc('M', '2', 'C', 'S')
	/* MTK 8-bit compressed block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S        v4l2_fourcc('M', '2', '1', 'S')
	/* MTK 8-bit block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S10T     v4l2_fourcc('M', 'T', 'S', 'T')
	/* MTK 10-bit tile block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S10R     v4l2_fourcc('M', 'T', 'S', 'R')
	/* MTK 10-bit raster block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CS10T    v4l2_fourcc('M', 'C', 'S', 'T')
	/* MTK 10-bit tile compressed block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CS10R    v4l2_fourcc('M', 'C', 'S', 'R')
	/* MTK 10-bit raster compressed block mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CSA      v4l2_fourcc('M', 'A', 'C', 'S')
	/* MTK 8-bit compressed block au offset mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S10TJ    v4l2_fourcc('M', 'J', 'S', 'T')
	/* MTK 10-bit tile block jump mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21S10RJ    v4l2_fourcc('M', 'J', 'S', 'R')
	/* MTK 10-bit raster block jump mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CS10TJ   v4l2_fourcc('J', 'C', 'S', 'T')
	/* MTK 10-bit tile compressed block jump mode, two contiguous planes */
#define V4L2_PIX_FMT_MT21CS10RJ   v4l2_fourcc('J', 'C', 'S', 'R')
	/* MTK 10-bit raster compressed block jump mode, two cont. planes */
#define V4L2_PIX_FMT_MT10S     v4l2_fourcc('M', '1', '0', 'S')
	/* MTK 10-bit compressed mode, three contiguous planes */
#define V4L2_PIX_FMT_MT10     v4l2_fourcc('M', 'T', '1', '0')
	/* MTK 10-bit compressed mode, three non-contiguous planes */
#define V4L2_PIX_FMT_P010S   v4l2_fourcc('P', '0', '1', 'S')
	/* 10-bit each pixel needs 2 bytes, LSB 6-bit is not used contiguous*/

	/* MTK 8-bit frame buffer compressed mode, single plane */
#define V4L2_PIX_FMT_RGB32_AFBC         v4l2_fourcc('M', 'C', 'R', '8')
#define V4L2_PIX_FMT_BGR32_AFBC         v4l2_fourcc('M', 'C', 'B', '8')
	/* MTK 10-bit frame buffer compressed mode, single plane */
#define V4L2_PIX_FMT_RGBA1010102_AFBC   v4l2_fourcc('M', 'C', 'R', 'X')
#define V4L2_PIX_FMT_BGRA1010102_AFBC   v4l2_fourcc('M', 'C', 'B', 'X')
	/* MTK 8-bit frame buffer compressed mode, two planes */
#define V4L2_PIX_FMT_NV12_AFBC          v4l2_fourcc('M', 'C', 'N', '8')
	/* MTK 8-bit frame buffer compressed mode, two planes */
#define V4L2_PIX_FMT_NV21_AFBC          v4l2_fourcc('M', 'N', '2', '8')
	/* MTK 10-bit frame buffer compressed mode, two planes */
#define V4L2_PIX_FMT_NV12_10B_AFBC      v4l2_fourcc('M', 'C', 'N', 'X')
/* Vendor specific - Mediatek ISP compressed formats */
#define V4L2_PIX_FMT_MTISP_B8	v4l2_fourcc('M', 'T', 'B', '8') /* 8 bit */


/* Reference freed flags*/
#define V4L2_BUF_FLAG_REF_FREED			0x00000200
/* Crop changed flags*/
#define V4L2_BUF_FLAG_CROP_CHANGED		0x00008000
#define V4L2_BUF_FLAG_CSD					0x00200000
#define V4L2_BUF_FLAG_ROI					0x00400000
#define V4L2_BUF_FLAG_HDR_META			0x00800000
#define V4L2_BUF_FLAG_QP_META			0x01000000
#define V4L2_BUF_FLAG_HAS_META			0x4000000

#define V4L2_EVENT_MTK_VCODEC_START	(V4L2_EVENT_PRIVATE_START + 0x00002000)
#define V4L2_EVENT_MTK_VDEC_ERROR	(V4L2_EVENT_MTK_VCODEC_START + 1)
#define V4L2_EVENT_MTK_VDEC_NOHEADER	(V4L2_EVENT_MTK_VCODEC_START + 2)
#define V4L2_EVENT_MTK_VENC_ERROR	(V4L2_EVENT_MTK_VCODEC_START + 3)

/* Mediatek control IDs */
#define V4L2_CID_MPEG_MTK_BASE \
	(V4L2_CTRL_CLASS_MPEG | 0x2000)
#define V4L2_CID_MPEG_MTK_FRAME_INTERVAL \
	(V4L2_CID_MPEG_MTK_BASE+0)
#define V4L2_CID_MPEG_MTK_ERRORMB_MAP \
	(V4L2_CID_MPEG_MTK_BASE+1)
#define V4L2_CID_MPEG_MTK_DECODE_MODE \
	(V4L2_CID_MPEG_MTK_BASE+2)
#define V4L2_CID_MPEG_MTK_FRAME_SIZE \
	(V4L2_CID_MPEG_MTK_BASE+3)
#define V4L2_CID_MPEG_MTK_FIXED_MAX_FRAME_BUFFER \
	(V4L2_CID_MPEG_MTK_BASE+4)
#define V4L2_CID_MPEG_MTK_CRC_PATH \
	(V4L2_CID_MPEG_MTK_BASE+5)
#define V4L2_CID_MPEG_MTK_GOLDEN_PATH \
	(V4L2_CID_MPEG_MTK_BASE+6)
#define V4L2_CID_MPEG_MTK_COLOR_DESC \
	(V4L2_CID_MPEG_MTK_BASE+7)
#define V4L2_CID_MPEG_MTK_ASPECT_RATIO \
	(V4L2_CID_MPEG_MTK_BASE+8)
#define V4L2_CID_MPEG_MTK_SET_WAIT_KEY_FRAME \
	(V4L2_CID_MPEG_MTK_BASE+9)
#define V4L2_CID_MPEG_MTK_SET_NAL_SIZE_LENGTH \
	(V4L2_CID_MPEG_MTK_BASE+10)
#define V4L2_CID_MPEG_MTK_SEC_DECODE \
	(V4L2_CID_MPEG_MTK_BASE+11)
#define V4L2_CID_MPEG_MTK_FIX_BUFFERS \
	(V4L2_CID_MPEG_MTK_BASE+12)
#define V4L2_CID_MPEG_MTK_FIX_BUFFERS_SVP \
	(V4L2_CID_MPEG_MTK_BASE+13)
#define V4L2_CID_MPEG_MTK_INTERLACING \
	(V4L2_CID_MPEG_MTK_BASE+14)
#define V4L2_CID_MPEG_MTK_CODEC_TYPE \
	(V4L2_CID_MPEG_MTK_BASE+15)
#define V4L2_CID_MPEG_MTK_OPERATING_RATE \
	(V4L2_CID_MPEG_MTK_BASE+16)
#define V4L2_CID_MPEG_MTK_SEC_ENCODE \
	(V4L2_CID_MPEG_MTK_BASE+17)
#define V4L2_CID_MPEG_MTK_QUEUED_FRAMEBUF_COUNT \
	(V4L2_CID_MPEG_MTK_BASE+18)
#define V4L2_CID_MPEG_MTK_UFO_MODE \
	(V4L2_CID_MPEG_MTK_BASE+19)
#define V4L2_CID_MPEG_MTK_ENCODE_SCENARIO \
	(V4L2_CID_MPEG_MTK_BASE+20)
#define V4L2_CID_MPEG_MTK_ENCODE_NONREFP \
	(V4L2_CID_MPEG_MTK_BASE+21)
#define V4L2_CID_MPEG_MTK_ENCODE_DETECTED_FRAMERATE \
	(V4L2_CID_MPEG_MTK_BASE+22)
#define V4L2_CID_MPEG_MTK_ENCODE_RFS_ON \
	(V4L2_CID_MPEG_MTK_BASE+23)
#define V4L2_CID_MPEG_MTK_ENCODE_OPERATION_RATE \
	(V4L2_CID_MPEG_MTK_BASE+24)
#define V4L2_CID_MPEG_MTK_ENCODE_ROI_RC_QP \
	(V4L2_CID_MPEG_MTK_BASE+25)
#define V4L2_CID_MPEG_MTK_ENCODE_ROI_ON \
	(V4L2_CID_MPEG_MTK_BASE+26)
#define V4L2_CID_MPEG_MTK_ENCODE_GRID_SIZE \
	(V4L2_CID_MPEG_MTK_BASE+27)
#define V4L2_CID_MPEG_MTK_RESOLUTION_CHANGE \
	(V4L2_CID_MPEG_MTK_BASE+28)
#define V4L2_CID_MPEG_MTK_MAX_WIDTH \
	(V4L2_CID_MPEG_MTK_BASE+29)
#define V4L2_CID_MPEG_MTK_MAX_HEIGHT \
	(V4L2_CID_MPEG_MTK_BASE+30)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_I_FRAME_QP \
	(V4L2_CID_MPEG_MTK_BASE+31)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_P_FRAME_QP \
	(V4L2_CID_MPEG_MTK_BASE+32)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_B_FRAME_QP \
	(V4L2_CID_MPEG_MTK_BASE+33)
#define V4L2_CID_MPEG_VIDEO_ENABLE_TSVC \
	(V4L2_CID_MPEG_MTK_BASE+34)
#define V4L2_CID_MPEG_MTK_ENCODE_NONREFP_FREQ \
	(V4L2_CID_MPEG_MTK_BASE+35)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_MAX_QP \
	(V4L2_CID_MPEG_MTK_BASE+36)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_MIN_QP \
	(V4L2_CID_MPEG_MTK_BASE+37)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_I_P_QP_DELTA \
	(V4L2_CID_MPEG_MTK_BASE+38)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_QP_CONTROL_MODE \
	(V4L2_CID_MPEG_MTK_BASE+39)
#define V4L2_CID_MPEG_MTK_ENCODE_RC_FRAME_LEVEL_QP \
	(V4L2_CID_MPEG_MTK_BASE+40)

#define V4L2_CID_MPEG_MTK_ENCODE_ENABLE_HIGHQUALITY \
	(V4L2_CID_MPEG_MTK_BASE+45)
#define V4L2_CID_MPEG_MTK_LOG \
	(V4L2_CID_MPEG_MTK_BASE+46)
#define V4L2_CID_MPEG_MTK_ENCODE_ENABLE_DUMMY_NAL \
	(V4L2_CID_MPEG_MTK_BASE+47)
#define V4L2_CID_MPEG_MTK_REAL_TIME_PRIORITY \
	(V4L2_CID_MPEG_MTK_BASE+48)
#define V4L2_CID_MPEG_MTK_VCP_PROP \
	(V4L2_CID_MPEG_MTK_BASE+49)

#endif /* _MTK_VCODEC_DRV_H_ */
