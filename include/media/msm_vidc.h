#ifndef _MSM_VIDC_H_
#define _MSM_VIDC_H_

#ifdef __KERNEL__

#include <linux/poll.h>
#include <linux/videodev2.h>

enum core_id {
	MSM_VIDC_CORE_0 = 0,
	MSM_VIDC_CORE_1,      /* for Q6 core */
	MSM_VIDC_CORES_MAX,
};
enum session_type {
	MSM_VIDC_ENCODER = 0,
	MSM_VIDC_DECODER,
	MSM_VIDC_MAX_DEVICES,
};
void *msm_vidc_open(int core_id, int session_type);
int msm_vidc_close(void *instance);
int msm_vidc_querycap(void *instance, struct v4l2_capability *cap);
int msm_vidc_enum_fmt(void *instance, struct v4l2_fmtdesc *f);
int msm_vidc_s_fmt(void *instance, struct v4l2_format *f);
int msm_vidc_g_fmt(void *instance, struct v4l2_format *f);
int msm_vidc_s_ctrl(void *instance, struct v4l2_control *a);
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
void *msm_vidc_get_resources(void *instance);
int msm_vidc_subscribe_event(void *instance,
		const struct v4l2_event_subscription *sub);
int msm_vidc_unsubscribe_event(void *instance,
		const struct v4l2_event_subscription *sub);
int msm_vidc_dqevent(void *instance, struct v4l2_event *event);
int msm_vidc_wait(void *instance);
int msm_vidc_s_parm(void *instance, struct v4l2_streamparm *a);
int msm_vidc_enum_framesizes(void *instance, struct v4l2_frmsizeenum *fsize);
#endif
struct msm_vidc_extradata_header {
	unsigned int size;
	unsigned int:32; /** Keeping binary compatibility */
	unsigned int:32; /* with firmware and OpenMAX IL **/
	unsigned int type; /* msm_vidc_extradata_type */
	unsigned int data_size;
	unsigned char data[1];
};
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
	unsigned int color_descp;
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
enum msm_vidc_extradata_type_ { /* Legacy enumeration */
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
	EXTRADATA_MULTISLICE_INFO = 0x7F100000,
	EXTRADATA_NUM_CONCEALED_MB = 0x7F100001,
	EXTRADATA_INDEX = 0x7F100002,
	EXTRADATA_ASPECT_RATIO = 0x7F100003,
	EXTRADATA_METADATA_FILLER = 0x7FE00002,
};
enum msm_vidc_extradata_type {
	MSM_VIDC_EXTRADATA_NONE = 0x00000000,
	MSM_VIDC_EXTRADATA_MB_QUANTIZATION = 0x00000001,
	MSM_VIDC_EXTRADATA_INTERLACE_VIDEO = 0x00000002,
	MSM_VIDC_EXTRADATA_VC1_FRAMEDISP = 0x00000003,
	MSM_VIDC_EXTRADATA_VC1_SEQDISP = 0x00000004,
	MSM_VIDC_EXTRADATA_TIMESTAMP = 0x00000005,
	MSM_VIDC_EXTRADATA_S3D_FRAME_PACKING = 0x00000006,
	MSM_VIDC_EXTRADATA_FRAME_RATE = 0x00000007,
	MSM_VIDC_EXTRADATA_PANSCAN_WINDOW = 0x00000008,
	MSM_VIDC_EXTRADATA_RECOVERY_POINT_SEI = 0x00000009,
	MSM_VIDC_EXTRADATA_MPEG2_SEQDISP = 0x0000000D,
	MSM_VIDC_EXTRADATA_MULTISLICE_INFO = 0x7F100000,
	MSM_VIDC_EXTRADATA_NUM_CONCEALED_MB = 0x7F100001,
	MSM_VIDC_EXTRADATA_INDEX = 0x7F100002,
	MSM_VIDC_EXTRADATA_ASPECT_RATIO = 0x7F100003,
	MSM_VIDC_EXTRADATA_METADATA_FILLER = 0x7FE00002,
};
enum msm_vidc_interlace_type_ { /* Legacy enumeration */
	INTERLACE_FRAME_PROGRESSIVE = 0x01,
	INTERLACE_INTERLEAVE_FRAME_TOPFIELDFIRST = 0x02,
	INTERLACE_INTERLEAVE_FRAME_BOTTOMFIELDFIRST = 0x04,
	INTERLACE_FRAME_TOPFIELDFIRST = 0x08,
	INTERLACE_FRAME_BOTTOMFIELDFIRST = 0x10,
};
enum msm_vidc_interlace_type {
	MSM_VIDC_INTERLACE_FRAME_PROGRESSIVE = 0x01,
	MSM_VIDC_INTERLACE_INTERLEAVE_FRAME_TOPFIELDFIRST = 0x02,
	MSM_VIDC_INTERLACE_INTERLEAVE_FRAME_BOTTOMFIELDFIRST = 0x04,
	MSM_VIDC_INTERLACE_FRAME_TOPFIELDFIRST = 0x08,
	MSM_VIDC_INTERLACE_FRAME_BOTTOMFIELDFIRST = 0x10,
};
enum msm_vidc_recovery_sei_ { /* Legacy enumeration */
	FRAME_RECONSTRUCTION_INCORRECT = 0x0,
	FRAME_RECONSTRUCTION_CORRECT = 0x01,
	FRAME_RECONSTRUCTION_APPROXIMATELY_CORRECT = 0x02,
};
enum msm_vidc_recovery_sei {
	MSM_VIDC_FRAME_RECONSTRUCTION_INCORRECT = 0x0,
	MSM_VIDC_FRAME_RECONSTRUCTION_CORRECT = 0x01,
	MSM_VIDC_FRAME_RECONSTRUCTION_APPROXIMATELY_CORRECT = 0x02,
};
#endif
