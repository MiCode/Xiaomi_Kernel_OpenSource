#ifndef _MSM_VIDC_ENC_H_
#define _MSM_VIDC_ENC_H_

#include <linux/types.h>
#include <linux/ioctl.h>

/** STATUS CODES*/
/* Base value for status codes */
#define VEN_S_BASE	0x00000000
#define VEN_S_SUCCESS	(VEN_S_BASE)/* Success */
#define VEN_S_EFAIL	(VEN_S_BASE+1)/* General failure */
#define VEN_S_EFATAL	(VEN_S_BASE+2)/* Fatal irrecoverable failure*/
#define VEN_S_EBADPARAM	(VEN_S_BASE+3)/* Error passed parameters*/
/*Command called in invalid state*/
#define VEN_S_EINVALSTATE	(VEN_S_BASE+4)
#define VEN_S_ENOSWRES	(VEN_S_BASE+5)/* Insufficient OS resources*/
#define VEN_S_ENOHWRES	(VEN_S_BASE+6)/*Insufficient HW resources */
#define VEN_S_EBUFFREQ	(VEN_S_BASE+7)/* Buffer requirements were not met*/
#define VEN_S_EINVALCMD	(VEN_S_BASE+8)/* Invalid command called */
#define VEN_S_ETIMEOUT	(VEN_S_BASE+9)/* Command timeout. */
/*Re-attempt was made when multiple invocation not supported for API.*/
#define VEN_S_ENOREATMPT	(VEN_S_BASE+10)
#define VEN_S_ENOPREREQ	(VEN_S_BASE+11)/*Pre-requirement is not met for API*/
#define VEN_S_ECMDQFULL	(VEN_S_BASE+12)/*Command queue is full*/
#define VEN_S_ENOTSUPP	(VEN_S_BASE+13)/*Command not supported*/
#define VEN_S_ENOTIMPL	(VEN_S_BASE+14)/*Command not implemented.*/
#define VEN_S_ENOTPMEM	(VEN_S_BASE+15)/*Buffer is not from PMEM*/
#define VEN_S_EFLUSHED	(VEN_S_BASE+16)/*returned buffer was flushed*/
#define VEN_S_EINSUFBUF	(VEN_S_BASE+17)/*provided buffer size insufficient*/
#define VEN_S_ESAMESTATE	(VEN_S_BASE+18)
#define VEN_S_EINVALTRANS	(VEN_S_BASE+19)

#define VEN_INTF_VER			 1

/*Asynchronous messages from driver*/
#define VEN_MSG_INDICATION	0
#define VEN_MSG_INPUT_BUFFER_DONE	1
#define VEN_MSG_OUTPUT_BUFFER_DONE	2
#define VEN_MSG_NEED_OUTPUT_BUFFER	3
#define VEN_MSG_FLUSH_INPUT_DONE	4
#define VEN_MSG_FLUSH_OUPUT_DONE	5
#define VEN_MSG_START	6
#define VEN_MSG_STOP	7
#define VEN_MSG_PAUSE	8
#define VEN_MSG_RESUME	9
#define VEN_MSG_STOP_READING_MSG	10
#define VEN_MSG_LTRUSE_FAILED	    11


/*Buffer flags bits masks*/
#define VEN_BUFFLAG_EOS	0x00000001
#define VEN_BUFFLAG_ENDOFFRAME	0x00000010
#define VEN_BUFFLAG_SYNCFRAME	0x00000020
#define VEN_BUFFLAG_EXTRADATA	0x00000040
#define VEN_BUFFLAG_CODECCONFIG	0x00000080

/*Post processing flags bit masks*/
#define VEN_EXTRADATA_NONE          0x001
#define VEN_EXTRADATA_QCOMFILLER    0x002
#define VEN_EXTRADATA_SLICEINFO     0x100
#define VEN_EXTRADATA_LTRINFO       0x200
#define VEN_EXTRADATA_MBINFO        0x400

/*ENCODER CONFIGURATION CONSTANTS*/

/*Encoded video frame types*/
#define VEN_FRAME_TYPE_I	1/* I frame type */
#define VEN_FRAME_TYPE_P	2/* P frame type */
#define VEN_FRAME_TYPE_B	3/* B frame type */

/*Video codec types*/
#define VEN_CODEC_MPEG4	1/* MPEG4 Codec */
#define VEN_CODEC_H264	2/* H.264 Codec */
#define VEN_CODEC_H263	3/* H.263 Codec */

/*Video codec profile types.*/
#define VEN_PROFILE_MPEG4_SP      1/* 1 - MPEG4 SP profile      */
#define VEN_PROFILE_MPEG4_ASP     2/* 2 - MPEG4 ASP profile     */
#define VEN_PROFILE_H264_BASELINE 3/* 3 - H264 Baseline profile	*/
#define VEN_PROFILE_H264_MAIN     4/* 4 - H264 Main profile     */
#define VEN_PROFILE_H264_HIGH     5/* 5 - H264 High profile     */
#define VEN_PROFILE_H263_BASELINE 6/* 6 - H263 Baseline profile */

/*Video codec profile level types.*/
#define VEN_LEVEL_MPEG4_0	 0x1/* MPEG4 Level 0  */
#define VEN_LEVEL_MPEG4_1	 0x2/* MPEG4 Level 1  */
#define VEN_LEVEL_MPEG4_2	 0x3/* MPEG4 Level 2  */
#define VEN_LEVEL_MPEG4_3	 0x4/* MPEG4 Level 3  */
#define VEN_LEVEL_MPEG4_4	 0x5/* MPEG4 Level 4  */
#define VEN_LEVEL_MPEG4_5	 0x6/* MPEG4 Level 5  */
#define VEN_LEVEL_MPEG4_3b	 0x7/* MPEG4 Level 3b */
#define VEN_LEVEL_MPEG4_6	 0x8/* MPEG4 Level 6  */

#define VEN_LEVEL_H264_1	 0x9/* H.264 Level 1   */
#define VEN_LEVEL_H264_1b        0xA/* H.264 Level 1b  */
#define VEN_LEVEL_H264_1p1	 0xB/* H.264 Level 1.1 */
#define VEN_LEVEL_H264_1p2	 0xC/* H.264 Level 1.2 */
#define VEN_LEVEL_H264_1p3	 0xD/* H.264 Level 1.3 */
#define VEN_LEVEL_H264_2	 0xE/* H.264 Level 2   */
#define VEN_LEVEL_H264_2p1	 0xF/* H.264 Level 2.1 */
#define VEN_LEVEL_H264_2p2	0x10/* H.264 Level 2.2 */
#define VEN_LEVEL_H264_3	0x11/* H.264 Level 3   */
#define VEN_LEVEL_H264_3p1	0x12/* H.264 Level 3.1 */
#define VEN_LEVEL_H264_3p2	0x13/* H.264 Level 3.2 */
#define VEN_LEVEL_H264_4	0x14/* H.264 Level 4   */

#define VEN_LEVEL_H263_10	0x15/* H.263 Level 10  */
#define VEN_LEVEL_H263_20	0x16/* H.263 Level 20  */
#define VEN_LEVEL_H263_30	0x17/* H.263 Level 30  */
#define VEN_LEVEL_H263_40	0x18/* H.263 Level 40  */
#define VEN_LEVEL_H263_45	0x19/* H.263 Level 45  */
#define VEN_LEVEL_H263_50	0x1A/* H.263 Level 50  */
#define VEN_LEVEL_H263_60	0x1B/* H.263 Level 60  */
#define VEN_LEVEL_H263_70	0x1C/* H.263 Level 70  */

/*Entropy coding model selection for H.264 encoder.*/
#define VEN_ENTROPY_MODEL_CAVLC	1
#define VEN_ENTROPY_MODEL_CABAC	2
/*Cabac model number (0,1,2) for encoder.*/
#define VEN_CABAC_MODEL_0	1/* CABAC Model 0. */
#define VEN_CABAC_MODEL_1	2/* CABAC Model 1. */
#define VEN_CABAC_MODEL_2	3/* CABAC Model 2. */

/*Deblocking filter control type for encoder.*/
#define VEN_DB_DISABLE	1/* 1 - Disable deblocking filter*/
#define VEN_DB_ALL_BLKG_BNDRY	2/* 2 - All blocking boundary filtering*/
#define VEN_DB_SKIP_SLICE_BNDRY	3/* 3 - Filtering except sliceboundary*/

/*Different methods of Multi slice selection.*/
#define VEN_MSLICE_OFF	1
#define VEN_MSLICE_CNT_MB	2 /*number of MBscount per slice*/
#define VEN_MSLICE_CNT_BYTE	3 /*number of bytes count per slice.*/
#define VEN_MSLICE_GOB	4 /*Multi slice by GOB for H.263 only.*/

/*Different modes for Rate Control.*/
#define VEN_RC_OFF	1
#define VEN_RC_VBR_VFR	2
#define VEN_RC_VBR_CFR	3
#define VEN_RC_CBR_VFR	4
#define VEN_RC_CBR_CFR	5

/*Different modes for flushing buffers*/
#define VEN_FLUSH_INPUT	1
#define VEN_FLUSH_OUTPUT	2
#define VEN_FLUSH_ALL	3

/*Different input formats for YUV data.*/
#define VEN_INPUTFMT_NV12	1/* NV12 Linear */
#define VEN_INPUTFMT_NV21	2/* NV21 Linear */
#define VEN_INPUTFMT_NV12_16M2KA	3/* NV12 Linear */

/*Different allowed rotation modes.*/
#define VEN_ROTATION_0	1/* 0 degrees */
#define VEN_ROTATION_90	2/* 90 degrees */
#define VEN_ROTATION_180	3/* 180 degrees */
#define VEN_ROTATION_270	4/* 270 degrees */

/*IOCTL timeout values*/
#define VEN_TIMEOUT_INFINITE	0xffffffff

/*Different allowed intra refresh modes.*/
#define VEN_IR_OFF	1
#define VEN_IR_CYCLIC	2
#define VEN_IR_RANDOM	3

/*IOCTL BASE CODES Not to be used directly by the client.*/
/* Base value for ioctls that are not related to encoder configuration.*/
#define VEN_IOCTLBASE_NENC	0x800
/* Base value for encoder configuration ioctls*/
#define VEN_IOCTLBASE_ENC	0x850

struct venc_ioctl_msg{
	void __user *in;
	void __user *out;
};

/*NON ENCODER CONFIGURATION IOCTLs*/

/*IOCTL params:SET: InputData - unsigned long, OutputData - NULL*/
#define VEN_IOCTL_SET_INTF_VERSION \
	_IOW(VEN_IOCTLBASE_NENC, 0, struct venc_ioctl_msg)

/*IOCTL params:CMD: InputData - venc_timeout, OutputData - venc_msg*/
#define VEN_IOCTL_CMD_READ_NEXT_MSG \
	_IOWR(VEN_IOCTLBASE_NENC, 1, struct venc_ioctl_msg)

/*IOCTL params:CMD: InputData - NULL, OutputData - NULL*/
#define VEN_IOCTL_CMD_STOP_READ_MSG	_IO(VEN_IOCTLBASE_NENC, 2)

/*IOCTL params:SET: InputData - venc_allocatorproperty, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_allocatorproperty*/
#define VEN_IOCTL_SET_INPUT_BUFFER_REQ \
	_IOW(VEN_IOCTLBASE_NENC, 3, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_INPUT_BUFFER_REQ \
	_IOR(VEN_IOCTLBASE_NENC, 4, struct venc_ioctl_msg)

/*IOCTL params:CMD: InputData - venc_bufferpayload, OutputData - NULL*/
#define VEN_IOCTL_CMD_ALLOC_INPUT_BUFFER \
	_IOW(VEN_IOCTLBASE_NENC, 5, struct venc_ioctl_msg)

/*IOCTL params:CMD: InputData - venc_bufferpayload, OutputData - NULL*/
#define VEN_IOCTL_SET_INPUT_BUFFER \
	_IOW(VEN_IOCTLBASE_NENC, 6, struct venc_ioctl_msg)

/*IOCTL params: CMD: InputData - venc_bufferpayload, OutputData - NULL*/
#define VEN_IOCTL_CMD_FREE_INPUT_BUFFER \
	_IOW(VEN_IOCTLBASE_NENC, 7, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_allocatorproperty, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_allocatorproperty*/
#define VEN_IOCTL_SET_OUTPUT_BUFFER_REQ \
	_IOW(VEN_IOCTLBASE_NENC, 8, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_OUTPUT_BUFFER_REQ \
	_IOR(VEN_IOCTLBASE_NENC, 9, struct venc_ioctl_msg)

/*IOCTL params:CMD: InputData - venc_bufferpayload, OutputData - NULL*/
#define VEN_IOCTL_CMD_ALLOC_OUTPUT_BUFFER \
	_IOW(VEN_IOCTLBASE_NENC, 10, struct venc_ioctl_msg)


/*IOCTL params:CMD: InputData - venc_bufferpayload, OutputData - NULL*/
#define VEN_IOCTL_SET_OUTPUT_BUFFER \
	_IOW(VEN_IOCTLBASE_NENC, 11, struct venc_ioctl_msg)

/*IOCTL params:CMD: InputData - venc_bufferpayload, OutputData - NULL.*/
#define VEN_IOCTL_CMD_FREE_OUTPUT_BUFFER \
	_IOW(VEN_IOCTLBASE_NENC, 12, struct venc_ioctl_msg)


/* Asynchronous respone message code:* VEN_MSG_START*/
#define VEN_IOCTL_CMD_START	_IO(VEN_IOCTLBASE_NENC, 13)


/*IOCTL params:CMD: InputData - venc_buffer, OutputData - NULL
 Asynchronous respone message code:VEN_MSG_INPUT_BUFFER_DONE*/
#define VEN_IOCTL_CMD_ENCODE_FRAME \
	_IOW(VEN_IOCTLBASE_NENC, 14, struct venc_ioctl_msg)


/*IOCTL params:CMD: InputData - venc_buffer, OutputData - NULL
 Asynchronous response message code:VEN_MSG_OUTPUT_BUFFER_DONE*/
#define VEN_IOCTL_CMD_FILL_OUTPUT_BUFFER \
	_IOW(VEN_IOCTLBASE_NENC, 15, struct venc_ioctl_msg)

/*IOCTL params:CMD: InputData - venc_bufferflush, OutputData - NULL
 * Asynchronous response message code:VEN_MSG_INPUT_BUFFER_DONE*/
#define VEN_IOCTL_CMD_FLUSH \
	_IOW(VEN_IOCTLBASE_NENC, 16, struct venc_ioctl_msg)


/*Asynchronous respone message code:VEN_MSG_PAUSE*/
#define VEN_IOCTL_CMD_PAUSE	_IO(VEN_IOCTLBASE_NENC, 17)

/*Asynchronous respone message code:VEN_MSG_RESUME*/
#define VEN_IOCTL_CMD_RESUME _IO(VEN_IOCTLBASE_NENC, 18)

/* Asynchronous respone message code:VEN_MSG_STOP*/
#define VEN_IOCTL_CMD_STOP _IO(VEN_IOCTLBASE_NENC, 19)

#define VEN_IOCTL_SET_RECON_BUFFER \
	_IOW(VEN_IOCTLBASE_NENC, 20, struct venc_ioctl_msg)

#define VEN_IOCTL_FREE_RECON_BUFFER \
	_IOW(VEN_IOCTLBASE_NENC, 21, struct venc_ioctl_msg)

#define VEN_IOCTL_GET_RECON_BUFFER_SIZE \
	_IOW(VEN_IOCTLBASE_NENC, 22, struct venc_ioctl_msg)



/*ENCODER PROPERTY CONFIGURATION & CAPABILITY IOCTLs*/

/*IOCTL params:SET: InputData - venc_basecfg, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_basecfg*/
#define VEN_IOCTL_SET_BASE_CFG \
	_IOW(VEN_IOCTLBASE_ENC, 1, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_BASE_CFG \
	_IOR(VEN_IOCTLBASE_ENC, 2, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_switch, OutputData - NULL
  GET: InputData - NULL, OutputData - venc_switch*/
#define VEN_IOCTL_SET_LIVE_MODE \
	_IOW(VEN_IOCTLBASE_ENC, 3, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_LIVE_MODE \
	_IOR(VEN_IOCTLBASE_ENC, 4, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_profile, OutputData - NULL
  GET: InputData - NULL, OutputData - venc_profile*/
#define VEN_IOCTL_SET_CODEC_PROFILE \
	_IOW(VEN_IOCTLBASE_ENC, 5, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_CODEC_PROFILE \
	_IOR(VEN_IOCTLBASE_ENC, 6, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - ven_profilelevel, OutputData - NULL
  GET: InputData - NULL, OutputData - ven_profilelevel*/
#define VEN_IOCTL_SET_PROFILE_LEVEL \
	_IOW(VEN_IOCTLBASE_ENC, 7, struct venc_ioctl_msg)

#define VEN_IOCTL_GET_PROFILE_LEVEL \
	_IOR(VEN_IOCTLBASE_ENC, 8, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_switch, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_switch*/
#define VEN_IOCTL_SET_SHORT_HDR \
	_IOW(VEN_IOCTLBASE_ENC, 9, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_SHORT_HDR \
	_IOR(VEN_IOCTLBASE_ENC, 10, struct venc_ioctl_msg)


/*IOCTL params: SET: InputData - venc_sessionqp, OutputData - NULL
  GET: InputData - NULL, OutputData - venc_sessionqp*/
#define VEN_IOCTL_SET_SESSION_QP \
	_IOW(VEN_IOCTLBASE_ENC, 11, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_SESSION_QP \
	_IOR(VEN_IOCTLBASE_ENC, 12, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_intraperiod, OutputData - NULL
  GET: InputData - NULL, OutputData - venc_intraperiod*/
#define VEN_IOCTL_SET_INTRA_PERIOD \
	_IOW(VEN_IOCTLBASE_ENC, 13, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_INTRA_PERIOD \
	_IOR(VEN_IOCTLBASE_ENC, 14, struct venc_ioctl_msg)


/* Request an Iframe*/
#define VEN_IOCTL_CMD_REQUEST_IFRAME _IO(VEN_IOCTLBASE_ENC, 15)

/*IOCTL params:GET: InputData - NULL, OutputData - venc_capability*/
#define VEN_IOCTL_GET_CAPABILITY \
	_IOR(VEN_IOCTLBASE_ENC, 16, struct venc_ioctl_msg)


/*IOCTL params:GET: InputData - NULL, OutputData - venc_seqheader*/
#define VEN_IOCTL_GET_SEQUENCE_HDR \
	_IOR(VEN_IOCTLBASE_ENC, 17, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_entropycfg, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_entropycfg*/
#define VEN_IOCTL_SET_ENTROPY_CFG \
	_IOW(VEN_IOCTLBASE_ENC, 18, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_ENTROPY_CFG \
	_IOR(VEN_IOCTLBASE_ENC, 19, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_dbcfg, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_dbcfg*/
#define VEN_IOCTL_SET_DEBLOCKING_CFG \
	_IOW(VEN_IOCTLBASE_ENC, 20, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_DEBLOCKING_CFG \
	_IOR(VEN_IOCTLBASE_ENC, 21, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_intrarefresh, OutputData - NULL
  GET: InputData - NULL, OutputData - venc_intrarefresh*/
#define VEN_IOCTL_SET_INTRA_REFRESH \
	_IOW(VEN_IOCTLBASE_ENC, 22, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_INTRA_REFRESH \
	_IOR(VEN_IOCTLBASE_ENC, 23, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_multiclicecfg, OutputData - NULL
  GET: InputData - NULL, OutputData - venc_multiclicecfg*/
#define VEN_IOCTL_SET_MULTI_SLICE_CFG \
	_IOW(VEN_IOCTLBASE_ENC, 24, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_MULTI_SLICE_CFG \
	_IOR(VEN_IOCTLBASE_ENC, 25, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_ratectrlcfg, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_ratectrlcfg*/
#define VEN_IOCTL_SET_RATE_CTRL_CFG \
	_IOW(VEN_IOCTLBASE_ENC, 26, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_RATE_CTRL_CFG \
	_IOR(VEN_IOCTLBASE_ENC, 27, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_voptimingcfg, OutputData - NULL
  GET: InputData - NULL, OutputData - venc_voptimingcfg*/
#define VEN_IOCTL_SET_VOP_TIMING_CFG \
	_IOW(VEN_IOCTLBASE_ENC, 28, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_VOP_TIMING_CFG \
	_IOR(VEN_IOCTLBASE_ENC, 29, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_framerate, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_framerate*/
#define VEN_IOCTL_SET_FRAME_RATE \
	_IOW(VEN_IOCTLBASE_ENC, 30, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_FRAME_RATE \
	_IOR(VEN_IOCTLBASE_ENC, 31, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_targetbitrate, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_targetbitrate*/
#define VEN_IOCTL_SET_TARGET_BITRATE \
	_IOW(VEN_IOCTLBASE_ENC, 32, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_TARGET_BITRATE \
	_IOR(VEN_IOCTLBASE_ENC, 33, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_rotation, OutputData - NULL
  GET: InputData - NULL, OutputData - venc_rotation*/
#define VEN_IOCTL_SET_ROTATION \
	_IOW(VEN_IOCTLBASE_ENC, 34, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_ROTATION \
	_IOR(VEN_IOCTLBASE_ENC, 35, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_headerextension, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_headerextension*/
#define VEN_IOCTL_SET_HEC \
	_IOW(VEN_IOCTLBASE_ENC, 36, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_HEC \
	_IOR(VEN_IOCTLBASE_ENC, 37, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_switch, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_switch*/
#define VEN_IOCTL_SET_DATA_PARTITION \
	_IOW(VEN_IOCTLBASE_ENC, 38, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_DATA_PARTITION \
	_IOR(VEN_IOCTLBASE_ENC, 39, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_switch, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_switch*/
#define VEN_IOCTL_SET_RVLC \
	_IOW(VEN_IOCTLBASE_ENC, 40, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_RVLC \
	_IOR(VEN_IOCTLBASE_ENC, 41, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_switch, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_switch*/
#define VEN_IOCTL_SET_AC_PREDICTION \
	_IOW(VEN_IOCTLBASE_ENC, 42, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_AC_PREDICTION \
	_IOR(VEN_IOCTLBASE_ENC, 43, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - venc_qprange, OutputData - NULL
 GET: InputData - NULL, OutputData - venc_qprange*/
#define VEN_IOCTL_SET_QP_RANGE \
	_IOW(VEN_IOCTLBASE_ENC, 44, struct venc_ioctl_msg)
#define VEN_IOCTL_GET_QP_RANGE \
	_IOR(VEN_IOCTLBASE_ENC, 45, struct venc_ioctl_msg)

#define VEN_IOCTL_GET_NUMBER_INSTANCES \
	_IOR(VEN_IOCTLBASE_ENC, 46, struct venc_ioctl_msg)

#define VEN_IOCTL_SET_METABUFFER_MODE \
	_IOW(VEN_IOCTLBASE_ENC, 47, struct venc_ioctl_msg)


/*IOCTL params:SET: InputData - unsigned int, OutputData - NULL.*/
#define VEN_IOCTL_SET_EXTRADATA \
	_IOW(VEN_IOCTLBASE_ENC, 48, struct venc_ioctl_msg)
/*IOCTL params:GET: InputData - NULL, OutputData - unsigned int.*/
#define VEN_IOCTL_GET_EXTRADATA \
	_IOR(VEN_IOCTLBASE_ENC, 49, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - NULL, OutputData - NULL.*/
#define VEN_IOCTL_SET_SLICE_DELIVERY_MODE \
	_IO(VEN_IOCTLBASE_ENC, 50)

#define VEN_IOCTL_SET_H263_PLUSPTYPE \
	_IOW(VEN_IOCTLBASE_ENC, 51, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_range, OutputData - NULL.*/
#define VEN_IOCTL_SET_CAPABILITY_LTRCOUNT \
	_IOW(VEN_IOCTLBASE_ENC, 52, struct venc_ioctl_msg)
/*IOCTL params:GET: InputData - NULL, OutputData - venc_range.*/
#define VEN_IOCTL_GET_CAPABILITY_LTRCOUNT \
	_IOR(VEN_IOCTLBASE_ENC, 53, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_ltrmode, OutputData - NULL.*/
#define VEN_IOCTL_SET_LTRMODE \
	_IOW(VEN_IOCTLBASE_ENC, 54, struct venc_ioctl_msg)
/*IOCTL params:GET: InputData - NULL, OutputData - venc_ltrmode.*/
#define VEN_IOCTL_GET_LTRMODE \
	_IOR(VEN_IOCTLBASE_ENC, 55, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_ltrcount, OutputData - NULL.*/
#define VEN_IOCTL_SET_LTRCOUNT \
	_IOW(VEN_IOCTLBASE_ENC, 56, struct venc_ioctl_msg)
/*IOCTL params:GET: InputData - NULL, OutputData - venc_ltrcount.*/
#define VEN_IOCTL_GET_LTRCOUNT \
	_IOR(VEN_IOCTLBASE_ENC, 57, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_ltrperiod, OutputData - NULL.*/
#define VEN_IOCTL_SET_LTRPERIOD \
	_IOW(VEN_IOCTLBASE_ENC, 58, struct venc_ioctl_msg)
/*IOCTL params:GET: InputData - NULL, OutputData - venc_ltrperiod.*/
#define VEN_IOCTL_GET_LTRPERIOD \
	_IOR(VEN_IOCTLBASE_ENC, 59, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_ltruse, OutputData - NULL.*/
#define VEN_IOCTL_SET_LTRUSE \
	_IOW(VEN_IOCTLBASE_ENC, 60, struct venc_ioctl_msg)
/*IOCTL params:GET: InputData - NULL, OutputData - venc_ltruse.*/
#define VEN_IOCTL_GET_LTRUSE \
	_IOR(VEN_IOCTLBASE_ENC, 61, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - venc_ltrmark, OutputData - NULL.*/
#define VEN_IOCTL_SET_LTRMARK \
	_IOW(VEN_IOCTLBASE_ENC, 62, struct venc_ioctl_msg)
/*IOCTL params:GET: InputData - NULL, OutputData - venc_ltrmark.*/
#define VEN_IOCTL_GET_LTRMARK \
	_IOR(VEN_IOCTLBASE_ENC, 63, struct venc_ioctl_msg)

/*IOCTL params:SET: InputData - unsigned int, OutputData - NULL*/
#define VEN_IOCTL_SET_SPS_PPS_FOR_IDR \
	_IOW(VEN_IOCTLBASE_ENC, 64, struct venc_ioctl_msg)

struct venc_range {
	unsigned long	max;
	unsigned long	min;
	unsigned long	step_size;
};

struct venc_switch{
	unsigned char	status;
};

struct venc_allocatorproperty{
	unsigned long	 mincount;
	unsigned long	 maxcount;
	unsigned long	 actualcount;
	unsigned long	 datasize;
	unsigned long	 suffixsize;
	unsigned long	 alignment;
	unsigned long	 bufpoolid;
};

struct venc_bufferpayload{
	unsigned char *pbuffer;
	size_t	sz;
	int	fd;
	unsigned int	offset;
	unsigned int	maped_size;
	unsigned long	filled_len;
};

struct venc_buffer{
 unsigned char *ptrbuffer;
 unsigned long	sz;
 unsigned long	len;
 unsigned long	offset;
 long long	timestamp;
 unsigned long	flags;
 void	*clientdata;
};

struct venc_basecfg{
	unsigned long	input_width;
	unsigned long	input_height;
	unsigned long	dvs_width;
	unsigned long	dvs_height;
	unsigned long	codectype;
	unsigned long	fps_num;
	unsigned long	fps_den;
	unsigned long	targetbitrate;
	unsigned long	inputformat;
};

struct venc_profile{
	unsigned long	profile;
};
struct ven_profilelevel{
	unsigned long	level;
};

struct venc_sessionqp{
	unsigned long	iframeqp;
	unsigned long	pframqp;
};

struct venc_qprange{
	unsigned long	maxqp;
	unsigned long	minqp;
};

struct venc_plusptype {
	unsigned long	plusptype_enable;
};

struct venc_intraperiod{
	unsigned long	num_pframes;
	unsigned long	num_bframes;
};
struct venc_seqheader{
	unsigned char *hdrbufptr;
	unsigned long	bufsize;
	unsigned long	hdrlen;
};

struct venc_capability{
	unsigned long	codec_types;
	unsigned long	maxframe_width;
	unsigned long	maxframe_height;
	unsigned long	maxtarget_bitrate;
	unsigned long	maxframe_rate;
	unsigned long	input_formats;
	unsigned char	dvs;
};

struct venc_entropycfg{
	unsigned longentropysel;
	unsigned long	cabacmodel;
};

struct venc_dbcfg{
	unsigned long	db_mode;
	unsigned long	slicealpha_offset;
	unsigned long	slicebeta_offset;
};

struct venc_intrarefresh{
	unsigned long	irmode;
	unsigned long	mbcount;
};

struct venc_multiclicecfg{
	unsigned long	mslice_mode;
	unsigned long	mslice_size;
};

struct venc_bufferflush{
	unsigned long	flush_mode;
};

struct venc_ratectrlcfg{
	unsigned long	rcmode;
};

struct	venc_voptimingcfg{
	unsigned long	voptime_resolution;
};
struct venc_framerate{
	unsigned long	fps_denominator;
	unsigned long	fps_numerator;
};

struct venc_targetbitrate{
	unsigned long	target_bitrate;
};


struct venc_rotation{
	unsigned long	rotation;
};

struct venc_timeout{
	 unsigned long	millisec;
};

struct venc_headerextension{
	 unsigned long	header_extension;
};

struct venc_msg{
	unsigned long	statuscode;
	unsigned long	msgcode;
	struct venc_buffer	buf;
	unsigned long	msgdata_size;
};

struct venc_recon_addr{
	unsigned char *pbuffer;
	unsigned long buffer_size;
	unsigned long pmem_fd;
	unsigned long offset;
};

struct venc_recon_buff_size{
	int width;
	int height;
	int size;
	int alignment;
};

struct venc_ltrmode {
	unsigned long   ltr_mode;
};

struct venc_ltrcount {
	unsigned long   ltr_count;
};

struct venc_ltrperiod {
	unsigned long   ltr_period;
};

struct venc_ltruse {
	unsigned long   ltr_id;
	unsigned long   ltr_frames;
};

#endif /* _MSM_VIDC_ENC_H_ */
