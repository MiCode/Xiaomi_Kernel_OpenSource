#ifndef _MSM_VIDC_H_
#define _MSM_VIDC_H_

#ifdef __KERNEL__

#include <linux/poll.h>
#include <linux/videodev2.h>

enum core_id {
	MSM_VIDC_CORE_VENUS = 0,
	MSM_VIDC_CORE_Q6,
	MSM_VIDC_CORES_MAX,
};

enum session_type {
	MSM_VIDC_ENCODER = 0,
	MSM_VIDC_DECODER,
	MSM_VIDC_MAX_DEVICES,
};

/* NOTE: if you change this enum you MUST update the
 * "buffer-type-tz-usage-table" for any affected target
 * in arch/arm/boot/dts/<arch>.dtsi
 */
enum hal_buffer {
	HAL_BUFFER_INPUT = 0x1,
	HAL_BUFFER_OUTPUT = 0x2,
	HAL_BUFFER_OUTPUT2 = 0x4,
	HAL_BUFFER_EXTRADATA_INPUT = 0x8,
	HAL_BUFFER_EXTRADATA_OUTPUT = 0x10,
	HAL_BUFFER_EXTRADATA_OUTPUT2 = 0x20,
	HAL_BUFFER_INTERNAL_SCRATCH = 0x40,
	HAL_BUFFER_INTERNAL_SCRATCH_1 = 0x80,
	HAL_BUFFER_INTERNAL_SCRATCH_2 = 0x100,
	HAL_BUFFER_INTERNAL_PERSIST = 0x200,
	HAL_BUFFER_INTERNAL_PERSIST_1 = 0x400,
	HAL_BUFFER_INTERNAL_CMD_QUEUE = 0x800,
};

struct msm_smem {
	int mem_type;
	size_t size;
	void *kvaddr;
	unsigned long device_addr;
	u32 flags;
	void *smem_priv;
	enum hal_buffer buffer_type;
};

enum smem_cache_ops {
	SMEM_CACHE_CLEAN,
	SMEM_CACHE_INVALIDATE,
	SMEM_CACHE_CLEAN_INVALIDATE,
};

void *msm_vidc_open(int core_id, int session_type);
int msm_vidc_close(void *instance);
int msm_vidc_querycap(void *instance, struct v4l2_capability *cap);
int msm_vidc_enum_fmt(void *instance, struct v4l2_fmtdesc *f);
int msm_vidc_s_fmt(void *instance, struct v4l2_format *f);
int msm_vidc_g_fmt(void *instance, struct v4l2_format *f);
int msm_vidc_s_ctrl(void *instance, struct v4l2_control *a);
int msm_vidc_s_ext_ctrl(void *instance, struct v4l2_ext_controls *a);
int msm_vidc_g_ctrl(void *instance, struct v4l2_control *a);
int msm_vidc_reqbufs(void *instance, struct v4l2_requestbuffers *b);
int msm_vidc_prepare_buf(void *instance, struct v4l2_buffer *b);
int msm_vidc_release_buffers(void *instance, int buffer_type);
int msm_vidc_qbuf(void *instance, struct v4l2_buffer *b);
int msm_vidc_dqbuf(void *instance, struct v4l2_buffer *b);
int msm_vidc_streamon(void *instance, enum v4l2_buf_type i);
int msm_vidc_streamoff(void *instance, enum v4l2_buf_type i);
int msm_vidc_decoder_cmd(void *instance, struct v4l2_decoder_cmd *dec);
int msm_vidc_encoder_cmd(void *instance, struct v4l2_encoder_cmd *enc);
int msm_vidc_poll(void *instance, struct file *filp,
		struct poll_table_struct *pt);
int msm_vidc_get_iommu_domain_partition(void *instance, u32 flags,
		enum v4l2_buf_type, int *domain, int *partition);
int msm_vidc_subscribe_event(void *instance,
		struct v4l2_event_subscription *sub);
int msm_vidc_unsubscribe_event(void *instance,
		struct v4l2_event_subscription *sub);
int msm_vidc_dqevent(void *instance, struct v4l2_event *event);
int msm_vidc_wait(void *instance);
int msm_vidc_s_parm(void *instance, struct v4l2_streamparm *a);
int msm_vidc_enum_framesizes(void *instance, struct v4l2_frmsizeenum *fsize);
struct msm_smem *msm_vidc_smem_alloc(void *instance,
			size_t size, u32 align, u32 flags,
			enum hal_buffer buffer_type, int map_kernel);
void msm_vidc_smem_free(void *instance, struct msm_smem *mem);
int msm_vidc_smem_cache_operations(void *instance,
		struct msm_smem *mem, enum smem_cache_ops);
struct msm_smem *msm_vidc_smem_user_to_kernel(void *instance,
			int fd, u32 offset, enum hal_buffer buffer_type);
int msm_vidc_smem_get_domain_partition(void *instance,
		u32 flags, enum hal_buffer buffer_type,
		int *domain_num, int *partition_num);
void *msm_vidc_smem_get_client(void *instance);
#endif
struct msm_vidc_interlace_payload {
	unsigned int format;
};
struct msm_vidc_framerate_payload {
	unsigned int frame_rate;
};
struct msm_vidc_ts_payload {
	unsigned int timestamp_lo;
	unsigned int timestamp_hi;
};
struct msm_vidc_concealmb_payload {
	unsigned int num_mbs;
};
struct msm_vidc_recoverysei_payload {
	unsigned int flags;
};

struct msm_vidc_aspect_ratio_payload {
	unsigned int size;
	unsigned int version;
	unsigned int port_index;
	unsigned int aspect_width;
	unsigned int aspect_height;
};

struct msm_vidc_mpeg2_seqdisp_payload {
	unsigned int video_format;
	bool color_descp;
	unsigned int color_primaries;
	unsigned int transfer_char;
	unsigned int matrix_coeffs;
	unsigned int disp_width;
	unsigned int disp_height;
};

struct msm_vidc_panscan_window {
	unsigned int panscan_height_offset;
	unsigned int panscan_width_offset;
	unsigned int panscan_window_width;
	unsigned int panscan_window_height;
};
struct msm_vidc_panscan_window_payload {
	unsigned int num_panscan_windows;
	struct msm_vidc_panscan_window wnd[1];
};
struct msm_vidc_s3d_frame_packing_payload {
	unsigned int fpa_id;
	unsigned int cancel_flag;
	unsigned int fpa_type;
	unsigned int quin_cunx_flag;
	unsigned int content_interprtation_type;
	unsigned int spatial_flipping_flag;
	unsigned int frame0_flipped_flag;
	unsigned int field_views_flag;
	unsigned int current_frame_is_frame0_flag;
	unsigned int frame0_self_contained_flag;
	unsigned int frame1_self_contained_flag;
	unsigned int frame0_graid_pos_x;
	unsigned int frame0_graid_pos_y;
	unsigned int frame1_graid_pos_x;
	unsigned int frame1_graid_pos_y;
	unsigned int fpa_reserved_byte;
	unsigned int fpa_repetition_period;
	unsigned int fpa_extension_flag;
};
struct msm_vidc_frame_qp_payload {
	unsigned int frame_qp;
};

enum msm_vidc_extradata_type {
	EXTRADATA_NONE = 0x00000000,
	EXTRADATA_MB_QUANTIZATION = 0x00000001,
	EXTRADATA_INTERLACE_VIDEO = 0x00000002,
	EXTRADATA_VC1_FRAMEDISP = 0x00000003,
	EXTRADATA_VC1_SEQDISP = 0x00000004,
	EXTRADATA_TIMESTAMP = 0x00000005,
	EXTRADATA_S3D_FRAME_PACKING = 0x00000006,
	EXTRADATA_FRAME_RATE = 0x00000007,
	EXTRADATA_PANSCAN_WINDOW = 0x00000008,
	EXTRADATA_RECOVERY_POINT_SEI = 0x00000009,
	EXTRADATA_MPEG2_SEQDISP = 0x0000000D,
	EXTRADATA_FRAME_QP = 0x0000000F,
	EXTRADATA_MULTISLICE_INFO = 0x7F100000,
	EXTRADATA_NUM_CONCEALED_MB = 0x7F100001,
	EXTRADATA_INDEX = 0x7F100002,
	EXTRADATA_ASPECT_RATIO = 0x7F100003,
	EXTRADATA_METADATA_FILLER = 0x7FE00002,
	MSM_VIDC_EXTRADATA_METADATA_LTR = 0x7F100004,
};
enum msm_vidc_interlace_type {
	INTERLACE_FRAME_PROGRESSIVE = 0x01,
	INTERLACE_INTERLEAVE_FRAME_TOPFIELDFIRST = 0x02,
	INTERLACE_INTERLEAVE_FRAME_BOTTOMFIELDFIRST = 0x04,
	INTERLACE_FRAME_TOPFIELDFIRST = 0x08,
	INTERLACE_FRAME_BOTTOMFIELDFIRST = 0x10,
};
enum msm_vidc_recovery_sei {
	FRAME_RECONSTRUCTION_INCORRECT = 0x0,
	FRAME_RECONSTRUCTION_CORRECT = 0x01,
	FRAME_RECONSTRUCTION_APPROXIMATELY_CORRECT = 0x02,
};

#endif
