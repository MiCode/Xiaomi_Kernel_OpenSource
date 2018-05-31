#ifndef _UAPI_MSM_VIDC_DEC_H_
#define _UAPI_MSM_VIDC_DEC_H_

#include <linux/types.h>
#include <linux/ioctl.h>

/* STATUS CODES */
/* Base value for status codes */
#define VDEC_S_BASE	0x40000000
/* Success */
#define VDEC_S_SUCCESS	(VDEC_S_BASE)
/* General failure */
#define VDEC_S_EFAIL	(VDEC_S_BASE + 1)
/* Fatal irrecoverable  failure. Need to  tear down session. */
#define VDEC_S_EFATAL   (VDEC_S_BASE + 2)
/* Error detected in the passed  parameters */
#define VDEC_S_EBADPARAM	(VDEC_S_BASE + 3)
/* Command called in invalid  state. */
#define VDEC_S_EINVALSTATE	(VDEC_S_BASE + 4)
 /* Insufficient OS  resources - thread, memory etc. */
#define VDEC_S_ENOSWRES	(VDEC_S_BASE + 5)
 /* Insufficient HW resources -  core capacity  maxed  out. */
#define VDEC_S_ENOHWRES	(VDEC_S_BASE + 6)
/* Invalid command  called */
#define VDEC_S_EINVALCMD	(VDEC_S_BASE + 7)
/* Command timeout. */
#define VDEC_S_ETIMEOUT	(VDEC_S_BASE + 8)
/* Pre-requirement is  not met for API. */
#define VDEC_S_ENOPREREQ	(VDEC_S_BASE + 9)
/* Command queue is full. */
#define VDEC_S_ECMDQFULL	(VDEC_S_BASE + 10)
/* Command is not supported  by this driver */
#define VDEC_S_ENOTSUPP	(VDEC_S_BASE + 11)
/* Command is not implemented by thedriver. */
#define VDEC_S_ENOTIMPL	(VDEC_S_BASE + 12)
/* Command is not implemented by the driver.  */
#define VDEC_S_BUSY	(VDEC_S_BASE + 13)
#define VDEC_S_INPUT_BITSTREAM_ERR (VDEC_S_BASE + 14)

#define VDEC_INTF_VER	1
#define VDEC_MSG_BASE	0x0000000
/*
 *Codes to identify asynchronous message responses and events that driver
 *wants to communicate to the app.
 */
#define VDEC_MSG_INVALID	(VDEC_MSG_BASE + 0)
#define VDEC_MSG_RESP_INPUT_BUFFER_DONE	(VDEC_MSG_BASE + 1)
#define VDEC_MSG_RESP_OUTPUT_BUFFER_DONE	(VDEC_MSG_BASE + 2)
#define VDEC_MSG_RESP_INPUT_FLUSHED	(VDEC_MSG_BASE + 3)
#define VDEC_MSG_RESP_OUTPUT_FLUSHED	(VDEC_MSG_BASE + 4)
#define VDEC_MSG_RESP_FLUSH_INPUT_DONE	(VDEC_MSG_BASE + 5)
#define VDEC_MSG_RESP_FLUSH_OUTPUT_DONE	(VDEC_MSG_BASE + 6)
#define VDEC_MSG_RESP_START_DONE	(VDEC_MSG_BASE + 7)
#define VDEC_MSG_RESP_STOP_DONE	(VDEC_MSG_BASE + 8)
#define VDEC_MSG_RESP_PAUSE_DONE	(VDEC_MSG_BASE + 9)
#define VDEC_MSG_RESP_RESUME_DONE	(VDEC_MSG_BASE + 10)
#define VDEC_MSG_RESP_RESOURCE_LOADED	(VDEC_MSG_BASE + 11)
#define VDEC_EVT_RESOURCES_LOST	(VDEC_MSG_BASE + 12)
#define VDEC_MSG_EVT_CONFIG_CHANGED	(VDEC_MSG_BASE + 13)
#define VDEC_MSG_EVT_HW_ERROR	(VDEC_MSG_BASE + 14)
#define VDEC_MSG_EVT_INFO_CONFIG_CHANGED	(VDEC_MSG_BASE + 15)
#define VDEC_MSG_EVT_INFO_FIELD_DROPPED	(VDEC_MSG_BASE + 16)
#define VDEC_MSG_EVT_HW_OVERLOAD	(VDEC_MSG_BASE + 17)
#define VDEC_MSG_EVT_MAX_CLIENTS	(VDEC_MSG_BASE + 18)
#define VDEC_MSG_EVT_HW_UNSUPPORTED	(VDEC_MSG_BASE + 19)

/*Buffer flags bits masks.*/
#define VDEC_BUFFERFLAG_EOS	0x00000001
#define VDEC_BUFFERFLAG_DECODEONLY	0x00000004
#define VDEC_BUFFERFLAG_DATACORRUPT	0x00000008
#define VDEC_BUFFERFLAG_ENDOFFRAME	0x00000010
#define VDEC_BUFFERFLAG_SYNCFRAME	0x00000020
#define VDEC_BUFFERFLAG_EXTRADATA	0x00000040
#define VDEC_BUFFERFLAG_CODECCONFIG	0x00000080

/*Post processing flags bit masks*/
#define VDEC_EXTRADATA_NONE 0x001
#define VDEC_EXTRADATA_QP 0x004
#define VDEC_EXTRADATA_MB_ERROR_MAP 0x008
#define VDEC_EXTRADATA_SEI 0x010
#define VDEC_EXTRADATA_VUI 0x020
#define VDEC_EXTRADATA_VC1 0x040

#define VDEC_EXTRADATA_EXT_DATA          0x0800
#define VDEC_EXTRADATA_USER_DATA         0x1000
#define VDEC_EXTRADATA_EXT_BUFFER        0x2000

#define VDEC_CMDBASE	0x800
#define VDEC_CMD_SET_INTF_VERSION	(VDEC_CMDBASE)

#define VDEC_IOCTL_MAGIC 'v'

struct vdec_ioctl_msg {
	void __user *in;
	void __user *out;
};

/*
 * CMD params: InputParam:enum vdec_codec
 * OutputParam: struct vdec_profile_level
 */
#define VDEC_IOCTL_GET_PROFILE_LEVEL_SUPPORTED \
	_IOWR(VDEC_IOCTL_MAGIC, 0, struct vdec_ioctl_msg)

/*
 * CMD params:InputParam: NULL
 * OutputParam: uint32_t(bitmask)
 */
#define VDEC_IOCTL_GET_INTERLACE_FORMAT \
	_IOR(VDEC_IOCTL_MAGIC, 1, struct vdec_ioctl_msg)

/*
 * CMD params: InputParam:  enum vdec_codec
 * OutputParam: struct vdec_profile_level
 */
#define VDEC_IOCTL_GET_CURRENT_PROFILE_LEVEL \
	_IOWR(VDEC_IOCTL_MAGIC, 2, struct vdec_ioctl_msg)

/*
 * CMD params: SET: InputParam: enum vdec_output_fromat  OutputParam: NULL
 * GET:  InputParam: NULL OutputParam: enum vdec_output_fromat
 */
#define VDEC_IOCTL_SET_OUTPUT_FORMAT \
	_IOWR(VDEC_IOCTL_MAGIC, 3, struct vdec_ioctl_msg)
#define VDEC_IOCTL_GET_OUTPUT_FORMAT \
	_IOWR(VDEC_IOCTL_MAGIC, 4, struct vdec_ioctl_msg)

/*
 * CMD params: SET: InputParam: enum vdec_codec OutputParam: NULL
 * GET: InputParam: NULL OutputParam: enum vdec_codec
 */
#define VDEC_IOCTL_SET_CODEC \
	_IOW(VDEC_IOCTL_MAGIC, 5, struct vdec_ioctl_msg)
#define VDEC_IOCTL_GET_CODEC \
	_IOR(VDEC_IOCTL_MAGIC, 6, struct vdec_ioctl_msg)

/*
 * CMD params: SET: InputParam: struct vdec_picsize outputparam: NULL
 * GET: InputParam: NULL outputparam: struct vdec_picsize
 */
#define VDEC_IOCTL_SET_PICRES \
	_IOW(VDEC_IOCTL_MAGIC, 7, struct vdec_ioctl_msg)
#define VDEC_IOCTL_GET_PICRES \
	_IOR(VDEC_IOCTL_MAGIC, 8, struct vdec_ioctl_msg)

#define VDEC_IOCTL_SET_EXTRADATA \
	_IOW(VDEC_IOCTL_MAGIC, 9, struct vdec_ioctl_msg)
#define VDEC_IOCTL_GET_EXTRADATA \
	_IOR(VDEC_IOCTL_MAGIC, 10, struct vdec_ioctl_msg)

#define VDEC_IOCTL_SET_SEQUENCE_HEADER \
	_IOW(VDEC_IOCTL_MAGIC, 11, struct vdec_ioctl_msg)

/*
 * CMD params: SET: InputParam - vdec_allocatorproperty, OutputParam - NULL
 * GET: InputParam - NULL, OutputParam - vdec_allocatorproperty
 */
#define VDEC_IOCTL_SET_BUFFER_REQ \
	_IOW(VDEC_IOCTL_MAGIC, 12, struct vdec_ioctl_msg)
#define VDEC_IOCTL_GET_BUFFER_REQ \
	_IOR(VDEC_IOCTL_MAGIC, 13, struct vdec_ioctl_msg)
/* CMD params: InputParam - vdec_buffer, OutputParam - uint8_t** */
#define VDEC_IOCTL_ALLOCATE_BUFFER \
	_IOWR(VDEC_IOCTL_MAGIC, 14, struct vdec_ioctl_msg)
/* CMD params: InputParam - uint8_t *, OutputParam - NULL.*/
#define VDEC_IOCTL_FREE_BUFFER \
	_IOW(VDEC_IOCTL_MAGIC, 15, struct vdec_ioctl_msg)

/*CMD params: CMD: InputParam - struct vdec_setbuffer_cmd, OutputParam - NULL*/
#define VDEC_IOCTL_SET_BUFFER \
	_IOW(VDEC_IOCTL_MAGIC, 16, struct vdec_ioctl_msg)

/* CMD params: InputParam - struct vdec_fillbuffer_cmd, OutputParam - NULL*/
#define VDEC_IOCTL_FILL_OUTPUT_BUFFER \
	_IOW(VDEC_IOCTL_MAGIC, 17, struct vdec_ioctl_msg)

/*CMD params: InputParam - struct vdec_frameinfo , OutputParam - NULL*/
#define VDEC_IOCTL_DECODE_FRAME \
	_IOW(VDEC_IOCTL_MAGIC, 18, struct vdec_ioctl_msg)

#define VDEC_IOCTL_LOAD_RESOURCES _IO(VDEC_IOCTL_MAGIC, 19)
#define VDEC_IOCTL_CMD_START _IO(VDEC_IOCTL_MAGIC, 20)
#define VDEC_IOCTL_CMD_STOP _IO(VDEC_IOCTL_MAGIC, 21)
#define VDEC_IOCTL_CMD_PAUSE _IO(VDEC_IOCTL_MAGIC, 22)
#define VDEC_IOCTL_CMD_RESUME _IO(VDEC_IOCTL_MAGIC, 23)

/*CMD params: InputParam - enum vdec_bufferflush , OutputParam - NULL */
#define VDEC_IOCTL_CMD_FLUSH _IOW(VDEC_IOCTL_MAGIC, 24, struct vdec_ioctl_msg)

/* ========================================================
 * IOCTL for getting asynchronous notification from driver
 * ========================================================
 */

/*IOCTL params: InputParam - NULL, OutputParam - struct vdec_msginfo*/
#define VDEC_IOCTL_GET_NEXT_MSG \
	_IOR(VDEC_IOCTL_MAGIC, 25, struct vdec_ioctl_msg)

#define VDEC_IOCTL_STOP_NEXT_MSG _IO(VDEC_IOCTL_MAGIC, 26)

#define VDEC_IOCTL_GET_NUMBER_INSTANCES \
	_IOR(VDEC_IOCTL_MAGIC, 27, struct vdec_ioctl_msg)

#define VDEC_IOCTL_SET_PICTURE_ORDER \
	_IOW(VDEC_IOCTL_MAGIC, 28, struct vdec_ioctl_msg)

#define VDEC_IOCTL_SET_FRAME_RATE \
	_IOW(VDEC_IOCTL_MAGIC, 29, struct vdec_ioctl_msg)

#define VDEC_IOCTL_SET_H264_MV_BUFFER \
	_IOW(VDEC_IOCTL_MAGIC, 30, struct vdec_ioctl_msg)

#define VDEC_IOCTL_FREE_H264_MV_BUFFER \
	_IOW(VDEC_IOCTL_MAGIC, 31, struct vdec_ioctl_msg)

#define VDEC_IOCTL_GET_MV_BUFFER_SIZE  \
	_IOR(VDEC_IOCTL_MAGIC, 32, struct vdec_ioctl_msg)

#define VDEC_IOCTL_SET_IDR_ONLY_DECODING \
	_IO(VDEC_IOCTL_MAGIC, 33)

#define VDEC_IOCTL_SET_CONT_ON_RECONFIG  \
	_IO(VDEC_IOCTL_MAGIC, 34)

#define VDEC_IOCTL_SET_DISABLE_DMX \
	_IOW(VDEC_IOCTL_MAGIC, 35, struct vdec_ioctl_msg)

#define VDEC_IOCTL_GET_DISABLE_DMX \
	_IOR(VDEC_IOCTL_MAGIC, 36, struct vdec_ioctl_msg)

#define VDEC_IOCTL_GET_DISABLE_DMX_SUPPORT \
	_IOR(VDEC_IOCTL_MAGIC, 37, struct vdec_ioctl_msg)

#define VDEC_IOCTL_SET_PERF_CLK \
	_IOR(VDEC_IOCTL_MAGIC, 38, struct vdec_ioctl_msg)

#define VDEC_IOCTL_SET_META_BUFFERS \
	_IOW(VDEC_IOCTL_MAGIC, 39, struct vdec_ioctl_msg)

#define VDEC_IOCTL_FREE_META_BUFFERS \
	_IO(VDEC_IOCTL_MAGIC, 40)

enum vdec_picture {
	PICTURE_TYPE_I,
	PICTURE_TYPE_P,
	PICTURE_TYPE_B,
	PICTURE_TYPE_BI,
	PICTURE_TYPE_SKIP,
	PICTURE_TYPE_IDR,
	PICTURE_TYPE_UNKNOWN
};

enum vdec_buffer {
	VDEC_BUFFER_TYPE_INPUT,
	VDEC_BUFFER_TYPE_OUTPUT
};

struct vdec_allocatorproperty {
	enum vdec_buffer buffer_type;
	uint32_t mincount;
	uint32_t maxcount;
	uint32_t actualcount;
	size_t buffer_size;
	uint32_t alignment;
	uint32_t buf_poolid;
	size_t meta_buffer_size;
};

struct vdec_bufferpayload {
	void __user *bufferaddr;
	size_t buffer_len;
	int pmem_fd;
	size_t offset;
	size_t mmaped_size;
};

struct vdec_setbuffer_cmd {
	enum vdec_buffer buffer_type;
	struct vdec_bufferpayload buffer;
};

struct vdec_fillbuffer_cmd {
	struct vdec_bufferpayload buffer;
	void *client_data;
};

enum vdec_bufferflush {
	VDEC_FLUSH_TYPE_INPUT,
	VDEC_FLUSH_TYPE_OUTPUT,
	VDEC_FLUSH_TYPE_ALL
};

enum vdec_codec {
	VDEC_CODECTYPE_H264 = 0x1,
	VDEC_CODECTYPE_H263 = 0x2,
	VDEC_CODECTYPE_MPEG4 = 0x3,
	VDEC_CODECTYPE_DIVX_3 = 0x4,
	VDEC_CODECTYPE_DIVX_4 = 0x5,
	VDEC_CODECTYPE_DIVX_5 = 0x6,
	VDEC_CODECTYPE_DIVX_6 = 0x7,
	VDEC_CODECTYPE_XVID = 0x8,
	VDEC_CODECTYPE_MPEG1 = 0x9,
	VDEC_CODECTYPE_MPEG2 = 0xa,
	VDEC_CODECTYPE_VC1 = 0xb,
	VDEC_CODECTYPE_VC1_RCV = 0xc,
	VDEC_CODECTYPE_HEVC = 0xd,
	VDEC_CODECTYPE_MVC = 0xe,
	VDEC_CODECTYPE_VP8 = 0xf,
	VDEC_CODECTYPE_VP9 = 0x10,
};

enum vdec_mpeg2_profile {
	VDEC_MPEG2ProfileSimple = 0x1,
	VDEC_MPEG2ProfileMain = 0x2,
	VDEC_MPEG2Profile422 = 0x4,
	VDEC_MPEG2ProfileSNR = 0x8,
	VDEC_MPEG2ProfileSpatial = 0x10,
	VDEC_MPEG2ProfileHigh = 0x20,
	VDEC_MPEG2ProfileKhronosExtensions = 0x6F000000,
	VDEC_MPEG2ProfileVendorStartUnused = 0x7F000000,
	VDEC_MPEG2ProfileMax = 0x7FFFFFFF
};

enum vdec_mpeg2_level {

	VDEC_MPEG2LevelLL = 0x1,
	VDEC_MPEG2LevelML = 0x2,
	VDEC_MPEG2LevelH14 = 0x4,
	VDEC_MPEG2LevelHL = 0x8,
	VDEC_MPEG2LevelKhronosExtensions = 0x6F000000,
	VDEC_MPEG2LevelVendorStartUnused = 0x7F000000,
	VDEC_MPEG2LevelMax = 0x7FFFFFFF
};

enum vdec_mpeg4_profile {
	VDEC_MPEG4ProfileSimple = 0x01,
	VDEC_MPEG4ProfileSimpleScalable = 0x02,
	VDEC_MPEG4ProfileCore = 0x04,
	VDEC_MPEG4ProfileMain = 0x08,
	VDEC_MPEG4ProfileNbit = 0x10,
	VDEC_MPEG4ProfileScalableTexture = 0x20,
	VDEC_MPEG4ProfileSimpleFace = 0x40,
	VDEC_MPEG4ProfileSimpleFBA = 0x80,
	VDEC_MPEG4ProfileBasicAnimated = 0x100,
	VDEC_MPEG4ProfileHybrid = 0x200,
	VDEC_MPEG4ProfileAdvancedRealTime = 0x400,
	VDEC_MPEG4ProfileCoreScalable = 0x800,
	VDEC_MPEG4ProfileAdvancedCoding = 0x1000,
	VDEC_MPEG4ProfileAdvancedCore = 0x2000,
	VDEC_MPEG4ProfileAdvancedScalable = 0x4000,
	VDEC_MPEG4ProfileAdvancedSimple = 0x8000,
	VDEC_MPEG4ProfileKhronosExtensions = 0x6F000000,
	VDEC_MPEG4ProfileVendorStartUnused = 0x7F000000,
	VDEC_MPEG4ProfileMax = 0x7FFFFFFF
};

enum vdec_mpeg4_level {
	VDEC_MPEG4Level0 = 0x01,
	VDEC_MPEG4Level0b = 0x02,
	VDEC_MPEG4Level1 = 0x04,
	VDEC_MPEG4Level2 = 0x08,
	VDEC_MPEG4Level3 = 0x10,
	VDEC_MPEG4Level4 = 0x20,
	VDEC_MPEG4Level4a = 0x40,
	VDEC_MPEG4Level5 = 0x80,
	VDEC_MPEG4LevelKhronosExtensions = 0x6F000000,
	VDEC_MPEG4LevelVendorStartUnused = 0x7F000000,
	VDEC_MPEG4LevelMax = 0x7FFFFFFF
};

enum vdec_avc_profile {
	VDEC_AVCProfileBaseline = 0x01,
	VDEC_AVCProfileMain = 0x02,
	VDEC_AVCProfileExtended = 0x04,
	VDEC_AVCProfileHigh = 0x08,
	VDEC_AVCProfileHigh10 = 0x10,
	VDEC_AVCProfileHigh422 = 0x20,
	VDEC_AVCProfileHigh444 = 0x40,
	VDEC_AVCProfileKhronosExtensions = 0x6F000000,
	VDEC_AVCProfileVendorStartUnused = 0x7F000000,
	VDEC_AVCProfileMax = 0x7FFFFFFF
};

enum vdec_avc_level {
	VDEC_AVCLevel1 = 0x01,
	VDEC_AVCLevel1b = 0x02,
	VDEC_AVCLevel11 = 0x04,
	VDEC_AVCLevel12 = 0x08,
	VDEC_AVCLevel13 = 0x10,
	VDEC_AVCLevel2 = 0x20,
	VDEC_AVCLevel21 = 0x40,
	VDEC_AVCLevel22 = 0x80,
	VDEC_AVCLevel3 = 0x100,
	VDEC_AVCLevel31 = 0x200,
	VDEC_AVCLevel32 = 0x400,
	VDEC_AVCLevel4 = 0x800,
	VDEC_AVCLevel41 = 0x1000,
	VDEC_AVCLevel42 = 0x2000,
	VDEC_AVCLevel5 = 0x4000,
	VDEC_AVCLevel51 = 0x8000,
	VDEC_AVCLevelKhronosExtensions = 0x6F000000,
	VDEC_AVCLevelVendorStartUnused = 0x7F000000,
	VDEC_AVCLevelMax = 0x7FFFFFFF
};

enum vdec_divx_profile {
	VDEC_DIVXProfile_qMobile = 0x01,
	VDEC_DIVXProfile_Mobile = 0x02,
	VDEC_DIVXProfile_HD = 0x04,
	VDEC_DIVXProfile_Handheld = 0x08,
	VDEC_DIVXProfile_Portable = 0x10,
	VDEC_DIVXProfile_HomeTheater = 0x20
};

enum vdec_xvid_profile {
	VDEC_XVIDProfile_Simple = 0x1,
	VDEC_XVIDProfile_Advanced_Realtime_Simple = 0x2,
	VDEC_XVIDProfile_Advanced_Simple = 0x4
};

enum vdec_xvid_level {
	VDEC_XVID_LEVEL_S_L0 = 0x1,
	VDEC_XVID_LEVEL_S_L1 = 0x2,
	VDEC_XVID_LEVEL_S_L2 = 0x4,
	VDEC_XVID_LEVEL_S_L3 = 0x8,
	VDEC_XVID_LEVEL_ARTS_L1 = 0x10,
	VDEC_XVID_LEVEL_ARTS_L2 = 0x20,
	VDEC_XVID_LEVEL_ARTS_L3 = 0x40,
	VDEC_XVID_LEVEL_ARTS_L4 = 0x80,
	VDEC_XVID_LEVEL_AS_L0 = 0x100,
	VDEC_XVID_LEVEL_AS_L1 = 0x200,
	VDEC_XVID_LEVEL_AS_L2 = 0x400,
	VDEC_XVID_LEVEL_AS_L3 = 0x800,
	VDEC_XVID_LEVEL_AS_L4 = 0x1000
};

enum vdec_h263profile {
	VDEC_H263ProfileBaseline = 0x01,
	VDEC_H263ProfileH320Coding = 0x02,
	VDEC_H263ProfileBackwardCompatible = 0x04,
	VDEC_H263ProfileISWV2 = 0x08,
	VDEC_H263ProfileISWV3 = 0x10,
	VDEC_H263ProfileHighCompression = 0x20,
	VDEC_H263ProfileInternet = 0x40,
	VDEC_H263ProfileInterlace = 0x80,
	VDEC_H263ProfileHighLatency = 0x100,
	VDEC_H263ProfileKhronosExtensions = 0x6F000000,
	VDEC_H263ProfileVendorStartUnused = 0x7F000000,
	VDEC_H263ProfileMax = 0x7FFFFFFF
};

enum vdec_h263level {
	VDEC_H263Level10 = 0x01,
	VDEC_H263Level20 = 0x02,
	VDEC_H263Level30 = 0x04,
	VDEC_H263Level40 = 0x08,
	VDEC_H263Level45 = 0x10,
	VDEC_H263Level50 = 0x20,
	VDEC_H263Level60 = 0x40,
	VDEC_H263Level70 = 0x80,
	VDEC_H263LevelKhronosExtensions = 0x6F000000,
	VDEC_H263LevelVendorStartUnused = 0x7F000000,
	VDEC_H263LevelMax = 0x7FFFFFFF
};

enum vdec_wmv_format {
	VDEC_WMVFormatUnused = 0x01,
	VDEC_WMVFormat7 = 0x02,
	VDEC_WMVFormat8 = 0x04,
	VDEC_WMVFormat9 = 0x08,
	VDEC_WMFFormatKhronosExtensions = 0x6F000000,
	VDEC_WMFFormatVendorStartUnused = 0x7F000000,
	VDEC_WMVFormatMax = 0x7FFFFFFF
};

enum vdec_vc1_profile {
	VDEC_VC1ProfileSimple = 0x1,
	VDEC_VC1ProfileMain = 0x2,
	VDEC_VC1ProfileAdvanced = 0x4
};

enum vdec_vc1_level {
	VDEC_VC1_LEVEL_S_Low = 0x1,
	VDEC_VC1_LEVEL_S_Medium = 0x2,
	VDEC_VC1_LEVEL_M_Low = 0x4,
	VDEC_VC1_LEVEL_M_Medium = 0x8,
	VDEC_VC1_LEVEL_M_High = 0x10,
	VDEC_VC1_LEVEL_A_L0 = 0x20,
	VDEC_VC1_LEVEL_A_L1 = 0x40,
	VDEC_VC1_LEVEL_A_L2 = 0x80,
	VDEC_VC1_LEVEL_A_L3 = 0x100,
	VDEC_VC1_LEVEL_A_L4 = 0x200
};

struct vdec_profile_level {
	uint32_t profiles;
	uint32_t levels;
};

enum vdec_interlaced_format {
	VDEC_InterlaceFrameProgressive = 0x1,
	VDEC_InterlaceInterleaveFrameTopFieldFirst = 0x2,
	VDEC_InterlaceInterleaveFrameBottomFieldFirst = 0x4
};

#define VDEC_YUV_FORMAT_NV12_TP10_UBWC \
	VDEC_YUV_FORMAT_NV12_TP10_UBWC

enum vdec_output_fromat {
	VDEC_YUV_FORMAT_NV12 = 0x1,
	VDEC_YUV_FORMAT_TILE_4x2 = 0x2,
	VDEC_YUV_FORMAT_NV12_UBWC = 0x3,
	VDEC_YUV_FORMAT_NV12_TP10_UBWC = 0x4
};

enum vdec_output_order {
	VDEC_ORDER_DISPLAY = 0x1,
	VDEC_ORDER_DECODE = 0x2
};

struct vdec_picsize {
	uint32_t frame_width;
	uint32_t frame_height;
	uint32_t stride;
	uint32_t scan_lines;
};

struct vdec_seqheader {
	void __user *ptr_seqheader;
	size_t seq_header_len;
	int pmem_fd;
	size_t pmem_offset;
};

struct vdec_mberror {
	void __user *ptr_errormap;
	size_t err_mapsize;
};

struct vdec_input_frameinfo {
	void __user *bufferaddr;
	size_t offset;
	size_t datalen;
	uint32_t flags;
	int64_t timestamp;
	void *client_data;
	int pmem_fd;
	size_t pmem_offset;
	void __user *desc_addr;
	uint32_t desc_size;
};

struct vdec_framesize {
	uint32_t   left;
	uint32_t   top;
	uint32_t   right;
	uint32_t   bottom;
};

struct vdec_aspectratioinfo {
	uint32_t aspect_ratio;
	uint32_t par_width;
	uint32_t par_height;
};

struct vdec_sep_metadatainfo {
	void __user *metabufaddr;
	uint32_t size;
	int fd;
	int offset;
	uint32_t buffer_size;
};

struct vdec_output_frameinfo {
	void __user *bufferaddr;
	size_t offset;
	size_t len;
	uint32_t flags;
	int64_t time_stamp;
	enum vdec_picture pic_type;
	void *client_data;
	void *input_frame_clientdata;
	struct vdec_picsize picsize;
	struct vdec_framesize framesize;
	enum vdec_interlaced_format interlaced_format;
	struct vdec_aspectratioinfo aspect_ratio_info;
	struct vdec_sep_metadatainfo metadata_info;
};

union vdec_msgdata {
	struct vdec_output_frameinfo output_frame;
	void *input_frame_clientdata;
};

struct vdec_msginfo {
	uint32_t status_code;
	uint32_t msgcode;
	union vdec_msgdata msgdata;
	size_t msgdatasize;
};

struct vdec_framerate {
	unsigned long fps_denominator;
	unsigned long fps_numerator;
};

struct vdec_h264_mv {
	size_t size;
	int count;
	int pmem_fd;
	int offset;
};

struct vdec_mv_buff_size {
	int width;
	int height;
	int size;
	int alignment;
};

struct vdec_meta_buffers {
	size_t size;
	int count;
	int pmem_fd;
	int pmem_fd_iommu;
	int offset;
};

#endif /* end of macro _VDECDECODER_H_ */
