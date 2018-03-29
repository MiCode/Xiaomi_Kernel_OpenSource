/*
 * Copyright (C) 2015 MediaTek Inc.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */


#ifndef VCODEC_IF_V2_H
#define VCODEC_IF_V2_H

/***************************************************
*
*   Chip definitions and Series definitions
*
***************************************************/


#ifndef TRUE
#define TRUE 1
#elif TRUE != 1
#error TRUE is not equal to 1
#endif

#ifndef FALSE
#define FALSE 0
#elif FALSE != 0
#error FALSE is not equal to 0
#endif
#define IN
#define OUT
#define INOUT
typedef void *HANDLE;
#ifdef WIN32
typedef unsigned __int64    UINT64;
#else
typedef unsigned long long  UINT64;
#endif

typedef enum {
	VCODEC_DECODER,
	VCODEC_ENCODER,
	NUM_OF_CODEC_TYPE
} VCODEC_CODEC_TYPE_T;

typedef struct {
	unsigned int u4YStride;
	unsigned int u4UVStride;
} VCODEC_YUV_STRIDE_T;

typedef enum {
	VCODEC_COLOR_FORMAT_YUV420,
	VCODEC_COLOR_FORMAT_YV12,
} VCODEC_COLOR_FORMAT_T;

typedef struct {
	unsigned int                MaxSupportWidth;
	unsigned int                MaxSupportHeight;
	unsigned int                eFlag;                   /* VCODEC_DEC_INPUT_FLAG_T */
	unsigned int                ExternalMEMSize;
	int                         OutBufferNum;            /* -1: .inf */
	VCODEC_YUV_STRIDE_T         stride;
	VCODEC_COLOR_FORMAT_T       eColorFormat;
	void *PrivateData[4];
} VCODEC_OPEN_SETTING_T;

typedef struct {
	unsigned int MaxSupportWidthForYUV420_BP;
	unsigned int MaxSupportHeightForYUV420_BP;
	unsigned int MaxSupportWidthForYUV420_MPHP;
	unsigned int MaxSupportHeightForYUV420_MPHP;
	unsigned int ExternalMEMSize;
	unsigned int DPBSize;
} H264_DEC_CUSTOM_SETTING_T;

typedef struct {
	unsigned int MaxSupportWidthForYUV420_MPHP;
	unsigned int MaxSupportHeightForYUV420_MPHP;
	unsigned int NormalMaxWidthForYUV420_MPHP;
	unsigned int NormalMaxHeightForYUV420_MPHP;
	unsigned int u4dpbSizes;
	void *reserved[4];
} H264_DECODER_PRIVATE_PARAM_T;
/*
typedef struct
{
    unsigned int MaxSupportWidthForYUV420_SP;
    unsigned int MaxSupportHeightForYUV420_SP;
    unsigned int MaxSupportWidthForYUV420_ASP;
    unsigned int MaxSupportHeightForYUV420_ASP;
    unsigned int ExternalMEMSize;
} MPEG4_DEC_CUSTOM_SETTING_T;

typedef struct
{
  unsigned int MaxSupportWidthForYUV420;
  unsigned int MaxSupportHeightForYUV420;
  unsigned int eFlag;                   //VCODEC_DEC_INPUT_FLAG_T
  unsigned int ExternalMEMSize;
  void *PrivateData[4];
} VC1_DEC_CUSTOM_SETTING_T;
*/

typedef enum {
	VCODEC_FRAMETYPE_I,
	VCODEC_FRAMETYPE_NS_I, /* non-seek I, non-IDR frame */
	VCODEC_FRAMETYPE_P,
	VCODEC_FRAMETYPE_B,
	VCODEC_HEADER,
	VCODEC_FRM_DROPPED,
	VCODEC_UNKNOWN_TYPE,
	NUM_OF_FRAME_TYPE
} VCODEC_FRAME_TYPE_T;



typedef enum {
	VA_NON_CACHED   = 0x0,
	VA_CACHED       = 0x1,
} VCODEC_BUFFER_ATTRIBUTE_T;

typedef enum {
	VCODEC_BUFFER_CACHEABLE           = 0,
	VCODEC_BUFFER_NON_CACHEABLE     = 1,
	VCODEC_BUFFER_MCI_SHARABLE      = 2
} VCODEC_MEMORY_TYPE_T;
typedef struct {
	unsigned int u4InternalSize;
	unsigned int u4ExternalSize;
} VCODEC_MEMORY_SIZE_T;

typedef struct {
	unsigned char           *pu1Buffer_VA;
	unsigned char           *pu1Buffer_PA;
	unsigned int             eBufferStatus;/* VCODEC_BUFFER_ATTRIBUTE_T */
} VCODEC_BUFFER_T;

typedef enum {
	VCODEC_DEC_ERROR_NONE,
	VCODEC_DEC_ERROR_DECODE_ERROR,
	VCODEC_DEC_ERROR_ASSERT_FAIL,
	VCODEC_DEC_ERROR_FATAL_ERROR,
	VCODEC_DEC_ERROR_NOT_SUPPORT,
	VCODEC_DEC_ERROR_NOT_ENOUGH_MEM,
	VCODEC_DEC_ERROR_PAYLOAD_DATA_ERROR,
	VCODEC_DEC_ERROR_OAL_CHECK_VERSION_FAIL,
	VCODEC_DEC_ERROR_DIMENSION_CHANGE,
	NUM_OF_DEC_ERROR_TYPE
} VCODEC_DEC_ERROR_T;


typedef enum {
	CUSTOM_SETTING, /* custom setting */
	BEST_QUALITY,    /* standard mode */
	FAVOR_QUALITY,   /* adaptive control decode mode , quality first */
	FAVOR_FLUENCY,   /* adaptive control decode mode , fluency first */
	BEST_FLUENCY,    /* fast mode */
	NUM_OF_DECODE_MODE,
} VCODEC_DEC_DECODE_MODE_T;



typedef enum {
	VCODEC_DEC_PARAM_EOF,
	VCODEC_DEC_PARAM_QUERY_RESOLUTION_AHEAD,
	VCODEC_DEC_PARAM_QUERY_RESOLUTION,
	VCODEC_DEC_PARAM_QUERY_PREDICATION_TIME,
	VCODEC_DEC_PARAM_MEMORY_REQUIREMENT,
	VCODEC_DEC_PARAM_CAPABILITY,
	VCODEC_DEC_PARAM_NOT_BUFFERING,
	VCODEC_DEC_PARAM_BUFFERING,
	VCODEC_DEC_PARAM_BITRATE,
	VCODEC_DEC_PARAM_FRAMERATE,
	VCODEC_DEC_PARAM_EXCLUDE_BUF_NUM,
	VCODEC_DEC_PARAM_NO_OUTPUT_REORDERING,
	VCODEC_DEC_PARAM_FLUSH_BUFFER,
	VCODEC_DEC_PARAM_SET_DECRYPTION_MODE,
	VCODEC_DEC_PARAM_SET_DECODE_MODE,
	VCODEC_DEC_PARAM_GET_DECODE_MODE,
	VCODEC_DEC_PARAM_CTRL_VOS,
	VCODEC_DEC_PARAM_GET_SBSFLAG,
	VCODEC_DEC_PARAM_CONCEAL_LEVEL,
	VCODEC_DEC_PARAM_NUM_OF_HW_CTRL_THID,
	/*Get registered HW control thread id , output structure : VCODEC_REG_HW_CTRL_THID_T*/
	VCODEC_DEC_PARAM_GET_REG_HW_CTRL_THID,
	VCODEC_DEC_PARAM_SET_COLOR_FORMAT,
	/* VCODEC_DEC_PARAM_SET_STRIDE_ALIGNMENT, */
	VCODEC_DEC_PARAM_SET_AVAILABLE_CPU_NUM,
	VCODEC_DEC_PARAM_SET_MCI,                             /* enable or disable MCI mechanism */
	NUM_OF_DEC_PARAM_TYPE,
} VCODEC_DEC_PARAM_TYPE_T;

typedef enum {
	VCODEC_DEC_DISPLAY_CONCEALED_FRAME_DURING_PLAYBACK  = 0x01,
	VCODEC_DEC_DISPLAY_CONCEALED_FRAME_BEFORE_FIRST_I   = 0X02,
	VCODEC_DEC_DISPLAY_CONCEALED_FRAME_AFTER_FIRST_I   = 0X04,
	NUM_OF_DEC_CONCEAL_LEVEL_TYPE,
} VCODEC_DEC_CONCEAL_LEVEL_TYPE_T;
typedef enum {
	/* VCODEC_DEC_QUERY_INFO_AVAILABLE_YUV,                    //Total available YUV buffer */
	/* VCODEC_DEC_QUERY_INFO_TOTAL_YUV,                        //Total number of YUV buffer */
	/* //Total available display frame(without frame repeat) */
	/* VCODEC_DEC_QUERY_INFO_AVAILABLE_DISPLAY_FRAME,      */
	/* //Total real available display frame(including frame repeat) */
	/* VCODEC_DEC_QUERY_INFO_REAL_AVAILABLE_DISPLAY_FRAME,  */
	VCODEC_DEC_QUERY_INFO_OAL_FUNCTION,                 /* Query OAL Function pointer */
	VCODEC_DEC_QUERY_INFO_CURRENT_TIME,                 /* Current play time */
	VCODEC_DEC_QUERY_INFO_LAST_VIDEO_TIME,                 /* Last delivered frame time */
	/* VCODEC_DEC_QUERY_INFO_OAL_FUNCTION_SMP,               //Query OAL Function pointer */
	NUM_OF_QUERY_INFO_TYPE
} VCODEC_DEC_QUERY_INFO_TYPE_T;

typedef struct {
	VCODEC_COLOR_FORMAT_T eColorFormat;
	unsigned int          u4MaxWidth;
	unsigned int          u4MaxHeight;
	unsigned int          MaxVideoCodingResolution;
} VCODEC_ENC_GENERAL_SETTING_T;

typedef struct {
	VCODEC_COLOR_FORMAT_T eColorFormat;
	unsigned int         u4MaxWidth;
	unsigned int         u4MaxHeight;
	unsigned int         MaxVideoCodingResolution;
	unsigned int         complexityIndex;
} VCODEC_ENC_MPEG4_SETTING_T;

typedef union {
	VCODEC_ENC_MPEG4_SETTING_T     rMPEG4;
	VCODEC_ENC_GENERAL_SETTING_T   rVT;
	VCODEC_ENC_GENERAL_SETTING_T   rH264;
	VCODEC_ENC_GENERAL_SETTING_T   rHEVC;
	VCODEC_ENC_GENERAL_SETTING_T   rVP9;
	VCODEC_ENC_GENERAL_SETTING_T   rVP8;
} VCODEC_ENC_SETTING_T;

typedef struct {
	unsigned char *pu1ParamStream;
	unsigned int  u4ParamLength;
	unsigned int  u4Width;
	unsigned int  u4Height;
} VCODEC_DEC_QUERY_FRAME_SIZE_TYPE_T;

typedef enum {
	DISPLAY_CURRENT,  /* Normal dispaly */
	REPEAT_LAST,      /* Frame skipping , error handling */
	NOT_DISPLAY,      /* for vp8, error handling */
	LAST_FRAME,       /* EOF */
	NO_PIC,           /* buffering */
	NOT_USED,         /* H.264 multi-slice */
	DISPLAY_CURRENT_INTERLACE,    /* interlace dispaly */
	NUM_OF_DISPLAY_FRAME_STATUS
} VCODEC_DEC_DISPLAY_FRAME_STATUS;

typedef struct {
	int                      i4AspectRatioWidth;  /* width aspect ratio */
	int                      i4AspectRatioHeight; /* height aspect ratio */
	/* unsigned int                        u4IntraMBNum; */
	/* unsigned int                        u4InterFMBNum; */
	/* unsigned int                        u4InterBMBNum; */
	/* unsigned int                        u4SkipMBNum; */
	void                     *prExtra;
} VCODEC_DEC_PRIVATE_OUTPUT_EXTRA_T;

typedef struct {
	UINT64                             u8TimeStamp;
	int                                fgUpdateTime;   /* update time stamp */
	unsigned short                     u2FrameWidth;   /* Full Frame Size */
	unsigned short                     u2FrameHeight;  /* Full Frame Size */
	unsigned short                     u2ClipTop;
	unsigned short                     u2ClipBottom;
	unsigned short                     u2ClipLeft;
	unsigned short                     u2ClipRight;
	VCODEC_FRAME_TYPE_T                eFrameType;
	VCODEC_BUFFER_T                    rYStartAddr;   /*YUV buffer start address, include padding up and left*/
	VCODEC_BUFFER_T                    rUStartAddr;
	VCODEC_BUFFER_T                    rVStartAddr;
	VCODEC_DEC_DISPLAY_FRAME_STATUS    eDisplayFrameStatus;
	void                               *prExtra;
} VCODEC_DEC_PRIVATE_OUTPUT_T;



typedef void VCODEC_DEC_INPUT_DATA_T;

typedef enum {
	INPUT_FLAG_STREAM_DATA_TYPE     =  0x01,  /* Bit 0 = 1: Slice base data(non-frame base) ; 0: Frame base data*/
	INPUT_FLAG_STARTTIME            =  0x02,  /* seek start time at end of seek */
	INPUT_FLAG_DECODEONLY           =  0x04,  /* seek */
	/* H.264 for SPS,PPS issue, send first frame bitstream for set parameter*/
	INPUT_FLAG_PARAMETERSET         =  0x08,
	INPUT_FLAG_CUSTOM_SETTING       =  0x10,  /* Get max external memory size(VE)*/
	INPUT_FLAG_DECODE_INTRA_ONLY    =  0x20,  /* Only Decode Intra Frame */
	INPUT_FLAG_OPENAPI              =  0x40,  /* OPENAPI */
	INPUT_FLAG_DECODE_MODE          =  0x80,  /* Decode Mode */
	INPUT_FLAG_LEGACY_MODE          =  0x100, /* legacy Mode */
	INPUT_FLAG_MAX_DEC
} VCODEC_DEC_INPUT_FLAG_T;

typedef struct {
	UINT64                      u8TimeStamp;
	unsigned int                eFlags;           /* VCODEC_DEC_INPUT_FLAG_T */
	VCODEC_DEC_INPUT_DATA_T *prInputData;
	VCODEC_BUFFER_T             *prBuffer;        /* Input data address */
	unsigned int                 u4BuffSize;      /* Input buffer total size */
	void                        *prExtra;
} VCODEC_DEC_INPUT_T;






typedef struct {
	unsigned int    u4Width;                /* Full size 16 byte align */
	unsigned int    u4Height;               /* Full size 16 byte align */
	unsigned short  u2ClipTop;
	unsigned short  u2ClipBottom;
	unsigned short  u2ClipLeft;
	unsigned short  u2ClipRight;
	unsigned int    u4Offset;               /* Offset of YUV buffer start address */
	unsigned int    u4ReduceLength;         /* Padding size(End of YUV buffer pool) */
	unsigned char   u1Alignment;            /* YUV buffer address alignment */
	VCODEC_MEMORY_TYPE_T rYUVBUfferMemType; /* YUV buffer memory type */
	unsigned int    u4MaxBufferNum;
	unsigned int    u4ExtraBufferNum;
	void            *prExtra;
} VCODEC_DEC_OUTPUT_BUFFER_PARAM_T;

typedef struct {
	VCODEC_MEMORY_TYPE_T rBitStreamBufferMemType; /* bitstream buffer memory type */
	unsigned int    u4MaxBufferNum;
	void *PrivateData[4];
} VCODEC_DEC_INPUT_BUFFER_PARAM_T;

typedef struct {
	VCODEC_BUFFER_T rYBuffer;
	VCODEC_BUFFER_T rUBuffer;
	VCODEC_BUFFER_T rVBuffer;
} VCODEC_DEC_INPUT_YUV_INFO_T;

/* non-callback */
#define MAX_BITSTREAM_BUFFER_INFO_NUM 10
#define MAX_REF_FREE_YUV_BUFFER_NUM 18
typedef struct {
	VCODEC_BUFFER_T       *prRetBitsBuf;    /* for mt6575, mt6577 */
	unsigned int          u4ReturnInputCnt;
	VCODEC_BUFFER_T       rReturnInput[MAX_BITSTREAM_BUFFER_INFO_NUM];
	unsigned int          u4RefFreeYUVBufCnt;
	VCODEC_DEC_INPUT_YUV_INFO_T       parRefFreeYUVBuf[MAX_REF_FREE_YUV_BUFFER_NUM];
} VCODEC_DEC_OUTPUT_PARAM_T;
/* ~non-callback */

typedef struct {
	unsigned int u4SupportWidth;
	unsigned int u4SupportHeight;
	unsigned int u4SupportResolution;
	unsigned int u4SupportProfile;
	unsigned int u4SupportLevel;
} VCODEC_DEC_CAPABILITY_T;

typedef struct {
	void (*pfnMalloc)(IN HANDLE                             /* hDrv */,
			  IN unsigned int                       /* u4Size */,
			  IN unsigned int                       /*u4AlignSize*/,
			  IN VCODEC_MEMORY_TYPE_T                     /* fgCacheable */,
			  OUT VCODEC_BUFFER_T *                  /* prBuf */
			 );  /* buffer address must cache line align */

	void (*pfnIntMalloc)(IN HANDLE                             /* hDrv */,
			     IN unsigned int                       /* u4Size */,
			     IN unsigned int                       /*u4AlignSize*/,
			     OUT VCODEC_BUFFER_T *                 /* pBuffer_adr */
			    );

	void (*pfnFree)(IN HANDLE                             /* hDrv */,
			IN VCODEC_BUFFER_T *                   /* prBuf */
		       );  /* same memory type with malloc */

	void (*pfnIntFree)(IN HANDLE                             /* hDrv */,
			   IN VCODEC_BUFFER_T *                 /* pBuffer_adr */
			  );

	VCODEC_DEC_ERROR_T (*pfnSetYUVBuffer)(IN HANDLE                              /* hDrv */,
					     IN VCODEC_DEC_OUTPUT_BUFFER_PARAM_T *  /* prYUVParam */
					    );

	VCODEC_DEC_ERROR_T (*pfnGetYUVBuffer)(IN  HANDLE                             /* hDrv */,
					     OUT VCODEC_DEC_INPUT_YUV_INFO_T *      /* prYUVBuf */
					    );

	void (*pfnRefFreeYUVBuffer)(IN HANDLE                             /* hDrv */,
				    IN VCODEC_DEC_INPUT_YUV_INFO_T *      /* prYUVBuf */
				   );

	VCODEC_DEC_ERROR_T (*pfnQueryInfo)(IN HANDLE                              /* hDrv */,
					  IN VCODEC_DEC_QUERY_INFO_TYPE_T        /* query id*/,
					  OUT void *                           /* pvParamData*/ /* */
					 );

	void (*pfnReturnBitstream)(IN HANDLE /* hDrv */,
				   IN VCODEC_BUFFER_T  * /* prBuffer */  /* Input buffer address */,
				   IN unsigned int /* u4BuffSize */ /* Input buffer total size */
				  );

} VCODEC_DEC_CALLBACK_T;


/* non-callback */
typedef struct {
	VCODEC_DEC_INPUT_T              *prInputData;
	VCODEC_DEC_INPUT_YUV_INFO_T     *prYUVBufAddr;   /* prYUVBuf */
} VIDEO_DECODER_INPUT_NC_T;



typedef struct {
	VCODEC_DEC_ERROR_T (*pfnGetCodecRequired)(IN    VCODEC_DEC_INPUT_T *                 /* prInput */ ,
						 OUT   VCODEC_MEMORY_SIZE_T *               /* prMemSize */,
						 OUT   VCODEC_DEC_OUTPUT_BUFFER_PARAM_T *   /* prBufferParameter*/,
						 OUT   VCODEC_DEC_INPUT_BUFFER_PARAM_T *    /* prBitStreamParameter */,
						 INOUT void *                               /* reserve */
						);


	/********************************************************
	*  wrapped for smart phone
	********************************************************/
	VCODEC_DEC_ERROR_T (*pfnOpen)(IN HANDLE,                                             /* hDrv */
				     IN VCODEC_DEC_CALLBACK_T *,        /* pfnCallback */
				     IN void *,                            /* open setting */
				     OUT HANDLE * ,                         /* hCodec */
				     INOUT void *                                  /* reserve */
				    );

	VCODEC_DEC_ERROR_T (*pfnClose)(IN HANDLE   ,                                          /* hCodec */
				      INOUT void *                           /* reserve */
				     );

	VCODEC_DEC_ERROR_T (*pfnInit)(IN HANDLE   ,                                           /* hCodec */
				     INOUT void *                          /* reserve */
				    );

	/********************************************************
	*  wrapped for smart phone
	********************************************************/
	VCODEC_DEC_ERROR_T (*pfnDeInit)(
		IN HANDLE, /* hCodec */
		IN HANDLE, /* hWrap */

		OUT VCODEC_DEC_OUTPUT_PARAM_T * *, /* for smart phone */
		INOUT void * /* reserve */);

	VCODEC_DEC_ERROR_T (*pfnGetParameter)(IN HANDLE,                                     /* hCodec */
					     IN VCODEC_DEC_PARAM_TYPE_T,
					     INOUT void *                          /* pvParamData */
					    );
	/********************************************************
	*  wrapped for smart phone
	********************************************************/
	VCODEC_DEC_ERROR_T (*pfnSetParameter)(IN HANDLE,                                     /* hCodec */
					     IN HANDLE,                            /* hWrap */
					     IN VCODEC_DEC_PARAM_TYPE_T,           /* eParamType */

					     IN void *  ,                           /* pvParamData */
					     INOUT void *                          /* reserve */
					    );

	/********************************************************
	*  wrapped for smart phone
	********************************************************/
	VCODEC_DEC_ERROR_T (*pfnDecodeOneUnit)(

		IN HANDLE, /* hCodec */
		IN HANDLE, /* hWrap */

		IN VIDEO_DECODER_INPUT_NC_T *, /* prInput */
		OUT VCODEC_DEC_OUTPUT_PARAM_T * *, /* for smart phone */
		INOUT void * /* reserve */
	);

	/********************************************************
	*  wrapped for smart phone
	********************************************************/
	VCODEC_DEC_ERROR_T (*pfnGetNextDisplay)(IN HANDLE,                                   /* hCodec */
					       IN HANDLE,                            /* hWrap */

					       OUT VCODEC_DEC_PRIVATE_OUTPUT_T * ,    /* prPrivOutput */
					       INOUT void *                          /* reserve */
					      );
} VIDEO_DEC_API_T;
/* ~non-callback */







typedef struct {

	VCODEC_DEC_ERROR_T (*pfnGetCodecRequired)(IN    VCODEC_DEC_INPUT_T *                 /* prInput */ ,
						 OUT   VCODEC_MEMORY_SIZE_T *               /* prMemSize */,
						 /* prBufferParameter*/
						 OUT   VCODEC_DEC_OUTPUT_BUFFER_PARAM_T *      ,
						 /*prBitStreamParameter */
						 OUT   VCODEC_DEC_INPUT_BUFFER_PARAM_T *       ,
						 INOUT void *                              /* reserve */
						);


	VCODEC_DEC_ERROR_T (*pfnOpen)(IN HANDLE                             /* hDrv */,
				     IN VCODEC_DEC_CALLBACK_T *            /* pfnCallback */,
				     IN void *                             /* prOpenSetting */,
				     OUT HANDLE *                          /* hCodec */  ,
				     INOUT void *                          /* reserve */
				    );

	VCODEC_DEC_ERROR_T (*pfnClose)(IN HANDLE                             /* hCodec */,
				      INOUT void *                          /* reserve */
				     );

	VCODEC_DEC_ERROR_T (*pfnInit)(IN HANDLE                             /* hCodec */,
				     INOUT void *                          /* reserve */
				    );

	VCODEC_DEC_ERROR_T (*pfnDeInit)(IN HANDLE                             /* hCodec */,
				       INOUT void *                          /* reserve */
				      );

	VCODEC_DEC_ERROR_T (*pfnGetParameter)(IN HANDLE                             /* hCodec */,
					     IN VCODEC_DEC_PARAM_TYPE_T            /* eParamType */,
					     INOUT void *                          /* pvParamData */,
					     INOUT void *                          /* reserve */
					    );

	VCODEC_DEC_ERROR_T (*pfnSetParameter)(IN HANDLE                             /* hCodec */,
					     IN VCODEC_DEC_PARAM_TYPE_T            /* eParamType */,
					     IN void *                             /* pvParamData */,
					     INOUT void *                          /* reserve */
					    );

	VCODEC_DEC_ERROR_T (*pfnDecodeOneUnit)(IN    HANDLE                         /* hCodec */,
					      IN    VCODEC_DEC_INPUT_T *           /* prInput */,
					      INOUT void *                         /* reserve */
					     );

	VCODEC_DEC_ERROR_T (*pfnGetNextDisplay)(IN    HANDLE                         /* hCodec */,
					       OUT   VCODEC_DEC_PRIVATE_OUTPUT_T *  /* prPrivOutput */,
					       INOUT void *                         /* reserve */
					      );




} VCODEC_DEC_API_T;

typedef struct {
	VIDEO_DEC_API_T  rVideoDecAPI;
	VCODEC_DEC_API_T *pfnVcodecDecAPI;
	VCODEC_DEC_INPUT_YUV_INFO_T rGetYUVBuf;
	VCODEC_BUFFER_T rRetBitsBuf;
	VCODEC_DEC_OUTPUT_PARAM_T rDecoderOutputParam;
	unsigned int fgTookYUVBuff;
	HANDLE hDriver;
} VIDEO_WRAP_HANDLE_T;


#define VCODEC_ENC_MAX_PKTS_IN_SET        99
#define VCODEC_ENC_MAX_NALS_IN_SET        10

typedef struct {
	void        *u4Addr;
	unsigned int u4Size;
} VCODEC_ENC_CODEC_PACKET_INFO_T;


/* Note the first two items in the next structure must be (in order): */
/* 1. number of Packets */
/* 2. pointer to the packet info */
typedef struct {
	unsigned int                                    u4NumOfPkts;
	VCODEC_ENC_CODEC_PACKET_INFO_T  arPktInfo[VCODEC_ENC_MAX_PKTS_IN_SET];
} VCODEC_ENC_PACKET_SET_T;


typedef struct {
	int          u4NalUnitType;
	void        *u4Addr;        /* p_payload */
	unsigned int u4Size;        /* i_payload */
} VCODEC_ENC_CODEC_NAL_INFO_T;

typedef struct {
	unsigned int                    u4NumOfNals;
	VCODEC_ENC_CODEC_NAL_INFO_T     arNalInfo[VCODEC_ENC_MAX_NALS_IN_SET];
} VCODEC_ENC_NAL_SET_T;

typedef enum {
	MPEG4_RECODER,
	MPEG4_VT,
	H264_RECODER,
	H264_VT,
	NUM_OF_ENC_CODEC_TYPE
} VCODEC_ENC_CODEC_TYPE_T;

typedef struct {
	VCODEC_FRAME_TYPE_T      eFrameType;
	/* added to merge remained individual parameters in the phototype */
	VCODEC_BUFFER_T          rBitstreamAddr;
	unsigned int             u4BitstreamLength;
	int                      fgEndOfFrame;
	void                     *prChassis;
	VCODEC_ENC_CODEC_TYPE_T   eCodecType;
	UINT64                    u8TimeStamp;
	void                      *reserved[4];
} VCODEC_ENC_GENERAL_OUTPUT_T;

typedef struct {
	VCODEC_BUFFER_T rStartAddr;
	VCODEC_BUFFER_T rEndAddr;
	VCODEC_BUFFER_T rWriteAddr;
	VCODEC_BUFFER_T rReadAddr;
	unsigned int    u4BufferLength;
} VCODEC_ENC_BUFFER_INFO_T;


typedef enum {
	INPUT_FLAG_YUVBUFFER          = 0x01,
	INPUT_FLAG_NO_INPUT           = 0x02,
	INPUT_FLAG_NO_MORE_INPUT    = 0x03,
	INPUT_FLAG_MAX_ENC
} VCODEC_ENC_INPUT_FLAG_T;

typedef struct {
	VCODEC_BUFFER_T rYUVBuffer;
	unsigned int u4Length;
} VCODEC_ENC_INPUT_INFO_T;

typedef struct {
	VCODEC_BUFFER_T rYBuffer;
	VCODEC_BUFFER_T rUBuffer;
	VCODEC_BUFFER_T rVBuffer;
	unsigned int u4Length;
} VCODEC_ENC_INPUT_YUV_INFO_T;



typedef struct {
	UINT64                 u8TimeStamp;
	VCODEC_ENC_INPUT_FLAG_T  eFlags;
	VCODEC_ENC_INPUT_INFO_T  rInput;
} VCODEC_ENC_INPUT_PARAM_T;

typedef struct {
	UINT64                       u8TimeStamp;
	VCODEC_ENC_INPUT_FLAG_T      eFlags;
	VCODEC_ENC_INPUT_YUV_INFO_T rInput;
	void *prExtra;
} VCODEC_ENC_INPUT_YUV_PARAM_T;

typedef struct {
	VCODEC_BUFFER_T  rWp;
	int            fgSliceContained;
} VCODEC_ENC_UPDATE_WP_INTO_T;

typedef enum {
	VCODEC_ENC_ERROR_NONE,
	VCODEC_ENC_ERROR,
	VCODEC_ENC_ASSERT_FAIL,
	VCODEC_ENC_BS_BUFFER_NOT_ENOUGH,
	VCODEC_ENC_INPUT_REJECT,
	VCODEC_ENC_PARAM_NOT_SUPPORT,
	NUM_OF_ENC_ERROR_TYPE
} VCODEC_ENC_ERROR_T;


typedef enum {
	/* Query ext/int memory requirement for adaptation */
	VCODEC_ENC_PARAM_MEMORY_REQUIREMENT,
	/* Query the prefer memory type of bitstream buffer, return true means cacheable buffer is preferred */
	VCODEC_ENC_PARAM_BITSTREAM_IN_CACHE,
	/* Query the alignment needed on frame buffer for codec */
	VCODEC_ENC_PARAM_FRM_BUFFER_ALIGNMENT,
	VCODEC_ENC_PARAM_HOLD_RES_TILL_RELEASE_FRM,
	VCODEC_ENC_PARAM_IS_BLOCKBASED_YUV,
	VCODEC_ENC_PARAM_DECODER_CONFIGURATION_RECORD,
	VCODEC_ENC_PARAM_IF_ADAPTOR_MODIFY_TIMESTAMP,
	VCODEC_ENC_PARAM_WIDTH,
	VCODEC_ENC_PARAM_HEIGHT,
	VCODEC_ENC_PARAM_BITRATE,
	VCODEC_ENC_PARAM_FRAME_RATE,
	VCODEC_ENC_PARAM_FRAME_RATE_NUM,
	VCODEC_ENC_PARAM_FRAME_RATE_DEN,
	VCDOEC_ENC_PARAM_AUD,
	VCODEC_ENC_PARAM_REPEAD_HEADERS,
	VCODEC_ENC_PARAM_ANNEXB,
	VCODEC_ENC_PARAM_GEN_HEADER_FRM_RATE,
	VCODEC_ENC_PARAM_SHORT_HEADER,
	VCODEC_ENC_PARAM_SYNC_INTERVAL,
	VCODEC_ENC_PARAM_MAX_PKG_SIZE,
	VCODEC_ENC_PARAM_FORCE_ENCODE_I,
	VCODEC_ENC_PARAM_QUALITY,
	VCODEC_ENC_PARAM_SCENARIO,
	VCODEC_ENC_PARAM_CODEC_TYPE,
	VCODEC_ENC_PARAM_VT_RESTART,
	VCODEC_ENC_PARAM_ROTATE,
	VCODEC_ENC_PARAM_SET_CALLBACK,
	VCODEC_ENC_PARAM_SET_NO_MORE_INPUT,
	VCODEC_ENC_PARAM_NUM_OF_HW_CTRL_THID,
	/* Get registered HW control thread id , output structure : VCODEC_REG_HW_CTRL_THID_T */
	VCODEC_ENC_PARAM_GET_REG_HW_CTRL_THID,
	VCODEC_ENC_PARAM_SET_COLOR_FORMAT,
	VCODEC_ENC_PARAM_SET_YUV_STRIDE_ALIGNMENT,
	VCODEC_ENC_PARAM_SET_AVAILABLE_CPU_NUM,
	/* enable or disable MCI mechanism */
	VCODEC_ENC_PARAM_SET_MCI,
	VCODEC_ENC_PARAM_WPP,
	VCODEC_ENC_PARAM_CONSTQ,
	VCODEC_ENC_PARAM_RC_VERSION,
	VCODEC_ENC_PARAM_INIT_QP,
	VCODEC_ENC_PARAM_MAX_QP,
	VCODEC_ENC_PARAM_MIN_QP,
	VCODEC_ENC_PARAM_NUM_OF_SLICE,
	VCODEC_ENC_PARAM_PROFILE,
	VCODEC_ENC_PARAM_LEVEL,
	VCODEC_ENC_PARAM_THREADS,
	VCODEC_ENC_PARAM_VP8_TOKEN_PARTITIONS,
	VCODEC_ENC_PARAM_VP9_ENABLE_TILES,
	VCODEC_ENC_PARAM_VPX_ERR_RESILIENT,
	VCODEC_ENC_PARAM_VPX_NUMBER_OF_LAYERS,
	VCODEC_ENC_PARAM_VPX_MODE,
	VCODEC_ENC_PARAM_VPX_CPU_USED,
	VCODEC_ENC_PARAM_YUV_STRIDE,
	NUM_OF_ENC_PARAM_TYPE
} VCODEC_ENC_PARAM_TYPE_T;

typedef enum {
	VCODEC_ENC_QUERY_INFO_TOTAL_FRAME_BUFFER,      /* Total frame buffer size */
	VCODEC_ENC_QUERY_INFO_FRAMES_QUEUED,           /* Number of frames waited to encoder */
	VCODEC_ENC_QUERY_INFO_VTBUFFER_FULLNESS_DENOM, /* Denominator of VT buffer fullness */
	VCODEC_ENC_QUERY_INFO_VTBUFFER_FULLNESS_NUM,   /* Numerator of VT buffer fullness */
	VCODEC_ENC_QUERY_INFO_INIT_Q,                  /* Used by codec */
	VCODEC_ENC_QUERY_INFO_MIN_QP,                  /* Used by codec */
	VCODEC_ENC_QUERY_INFO_MAX_QP,                  /* Used by codec */
	VCODEC_ENC_QUERY_INFO_INTRA_VOP_RATE,          /* Used by MED/codec */
	VCODEC_ENC_QUERY_INFO_ALGORITHM,               /* Used by codec */
	VCODEC_ENC_QUERY_INFO_BIT_RATE,                /* Used by MED/codec */
	VCODEC_ENC_QUERY_INFO_RATE_HARD_LIMIT,         /* Used by codec */
	VCODEC_ENC_QUERY_INFO_RATE_BALANCE,            /* Used by codec */
	VCODEC_ENC_QUERY_INFO_DYNAMIC_RANGE_REDUCTION,
	VCODEC_ENC_QUERY_INFO_IF_CUSTOMER_SET_TABLE,
	VCODEC_ENC_QUERY_INFO_DYNAMIC_RANGE_TABLE,
	VCODEC_ENC_QUERY_INFO_OAL_FUNCTION,
	VCODEC_ENC_QUERY_INFO_ENCODER_FRAME_RATE,      /* Used by H.264 recoder */
	VCODEC_ENC_QUERY_INFO_TARGET_COMPLEXITY,       /* Used by H.264 recoder */
	VCODEC_ENC_QUERY_INFO_THRESHOLD_AVG_LOW,       /* Used by H.264 recoder */
	VCODEC_ENC_QUERY_INFO_THRESHOLD_AVG_HIGH,      /* Used by H.264 recoder */
	VCODEC_ENC_QUERY_INFO_THRESHOLD_CUR_LOW,       /* Used by H.264 recoder */
	VCODEC_ENC_QUERY_INFO_THRESHOLD_CUR_HIGH,       /* Used by H.264 recoder */
	/* VCODEC_ENC_QUERY_INFO_OAL_FUNCTION_SMP, */
	VCODEC_ENC_QUERY_INFO_VPX_CPU_USED,
	VCODEC_ENC_QUERY_INFO_VPX_MODE,
	VCODEC_ENC_QUERY_INFO_FIXED_QP,
	VCODEC_ENC_QUERY_INFO_SCENARIO,
	NUM_OF_ENC_QUERY_INFO_TYPE
} VCODEC_ENC_QUERY_INFO_TYPE_T;

/**********************************************************************


		       Encoder enumerations

 **********************************************************************/

/* clock-wise */
typedef enum {
	VCODEC_ENC_ROTATE_0     =   0,
	VCODEC_ENC_ROTATE_90    =   1,
	VCODEC_ENC_ROTATE_180   =   2,
	VCODEC_ENC_ROTATE_270   =   3
} VCODEC_ENC_ROTATE_ANGLE_T;

typedef enum {
	VCODEC_ENC_QUALITY_NONE,
	VCODEC_ENC_QUALITY_LOW,
	VCODEC_ENC_QUALITY_NORMAL,
	VCODEC_ENC_QUALITY_GOOD,
	VCODEC_ENC_QUALITY_FINE,
	VCODEC_ENC_QUALITY_CUSTOM
} VCODEC_ENC_QUALITY_T;

typedef enum {
	VCODEC_ENC_CODEC_TYPE_NONE,
	VCODEC_ENC_CODEC_TYPE_MPEG4,
	VCODEC_ENC_CODEC_TYPE_H263,
	VCODEC_ENC_CODEC_TYPE_H264
} VCODEC_ENC_CODEC_T;

typedef struct {

	void (*pfnMalloc)(IN HANDLE                             /* hDrv */,
			  IN unsigned int                       /* u4Size */,
			  IN unsigned int                       /*u4AlignSize*/,
			  IN VCODEC_MEMORY_TYPE_T                     /* fgCacheable */,
			  OUT VCODEC_BUFFER_T *                  /* prBuf */
			 );   /*buffer address must cache line align */

	void (*pfnIntMalloc)(IN HANDLE                             /* hDrv */,
			     IN unsigned int                       /* u4Size */,
			     IN unsigned int                       /*u4AlignSize*/,
			     OUT VCODEC_BUFFER_T *                      /* prBuffer_adr */
			    );


	void (*pfnFree)(IN HANDLE                             /* hDrv */,
			IN VCODEC_BUFFER_T *                   /* prBuf */
		       );  /* same memory type with malloc */


	void (*pfnIntFree)(IN HANDLE                             /* hDrv */,
			   IN VCODEC_BUFFER_T *                   /* prBuffer_adr */
			  );

	void (*pfnReleaseYUV)(IN HANDLE                             /* hDrv */,
			      IN VCODEC_BUFFER_T *                   /* prYUVBuf */
			     );

	void (*pfnPaused)(IN HANDLE                              /* hDrv */,
			  IN VCODEC_BUFFER_T *                    /* prBitstreamBuf */
			 );

	void (*pfnAllocateBitstreamBuffer)(IN HANDLE                              /* hDrv */,
					   OUT VCODEC_ENC_BUFFER_INFO_T *      /* prBitstreamBuf */
					  );

	void (*pfnUpdateBitstreamWP)(IN HANDLE                               /* hDrv */,
				     IN VCODEC_ENC_UPDATE_WP_INTO_T *     /* prUpdateWritePointer */
				    );
	VCODEC_ENC_ERROR_T(*pfnQueryInfo)(IN HANDLE                              /* hDrv */,
					  IN VCODEC_ENC_QUERY_INFO_TYPE_T        /* query id*/,
					  OUT void *                           /* pvParamData*/
					 );


} VCODEC_ENC_CALLBACK_T;

/* non-callback */
typedef struct {
	UINT64                       u8TimeStamp;
	VCODEC_ENC_INPUT_FLAG_T      eFlags;
	VCODEC_ENC_INPUT_INFO_T      rInput;
	VCODEC_ENC_BUFFER_INFO_T     pBuffInfo;
	VCODEC_COLOR_FORMAT_T       eVEncFormat;
	unsigned int                 u4Width;
	unsigned int                 u4Height;
	unsigned int                 u4YStride;
	unsigned int                 u4UVStride;
	unsigned int                 u4SliceHeight;
	void                         *reserved[4];
} VIDEO_ENCODER_INPUT_PARAM_NC_T;
/* non-callback */
typedef struct {
	UINT64                       u8TimeStamp;
	VCODEC_ENC_INPUT_FLAG_T      eFlags;
	VCODEC_ENC_INPUT_YUV_INFO_T  rInput;
	VCODEC_ENC_BUFFER_INFO_T     pBuffInfo;
	VCODEC_COLOR_FORMAT_T        eVEncFormat;
	unsigned int                 u4Width;
	unsigned int                 u4Height;
	unsigned int                 u4YStride;
	unsigned int                 u4UVStride;
	unsigned int                 u4SliceHeight;
	void                         *reserved[4];
} VIDEO_ENCODER_INPUT_YUV_PARAM_NC_T;

typedef struct {
	VCODEC_ENC_ERROR_T(*pfnGetMemoryRequired)(
		IN VCODEC_ENC_SETTING_T * rCodecFormat,
		OUT VCODEC_MEMORY_SIZE_T *prExternalSize
	);

	/********************************************************
	*  wrapped for smart phone
	********************************************************/
	VCODEC_ENC_ERROR_T(*pfnOpen)(IN HANDLE hDrv,
				     IN HANDLE hWrapper,
				     IN VCODEC_ENC_CALLBACK_T * ,
				     OUT HANDLE *hCodec
				    );

	VCODEC_ENC_ERROR_T(*pfnInit)(IN HANDLE hCodec
				    );

	VCODEC_ENC_ERROR_T(*pfnGetParameter)(IN HANDLE hCodec,
					     OUT VCODEC_ENC_PARAM_TYPE_T, /*VIDEO_ENC_MEMORY_T,*/

					     void *
					    );

	VCODEC_ENC_ERROR_T(*pfnSetParameter)(IN HANDLE hCodec,
					     /*VIDEO_ENCODER_QUALITY_T,
					     VIDEO_ENCODER_CODEC_T,
					     VIDEO_CODEC_ROTATE_ANGLE_T,*/
					     IN VCODEC_ENC_PARAM_TYPE_T,
					     void *
					    );

	/********************************************************
	*  wrapped for smart phone
	********************************************************/
	VCODEC_ENC_ERROR_T(*pfnGenerateHeader)(IN HANDLE hCodec,
					       IN HANDLE hWrapper,
					       IN VCODEC_ENC_BUFFER_INFO_T *prBufferInfo /* for smart phone */
					      );

	/********************************************************
	*  wrapped for smart phone
	********************************************************/
	VCODEC_ENC_ERROR_T(*pfnEncodeOneUnit)(IN HANDLE hCodec,
					      IN HANDLE hWrapper,
					      IN VIDEO_ENCODER_INPUT_YUV_PARAM_NC_T * prEncoderInputParamNC);

	VCODEC_ENC_ERROR_T(*pfnDeInit)(IN HANDLE hCodec
				      );

	VCODEC_ENC_ERROR_T(*pfnClose)(IN HANDLE hCodec
				     );

	VCODEC_ENC_ERROR_T(*pfnGetNextBitstream)(IN HANDLE hCodec,
						 OUT VCODEC_ENC_GENERAL_OUTPUT_T *
						);
} VIDEO_ENC_API_T;


/* ~non-callback */

typedef struct {
	VCODEC_ENC_ERROR_T(*pfnGetMemoryRequired)(IN VCODEC_ENC_SETTING_T *,          /*prInput*/
						  OUT VCODEC_MEMORY_SIZE_T *          /*prExternalSize*/
						 );

	VCODEC_ENC_ERROR_T(*pfnOpen)(IN HANDLE                            /*  hDrv  */,
				     IN VCODEC_ENC_CALLBACK_T *        /* pfnCallback */,
				     OUT HANDLE *                          /* hCodec */
				    );

	VCODEC_ENC_ERROR_T(*pfnInit)(IN HANDLE                                          /* hCodec */
				    );

	VCODEC_ENC_ERROR_T(*pfnGetParameter)(IN HANDLE                            /* hCodec */,
					     IN VCODEC_ENC_PARAM_TYPE_T       /*VIDEO_ENC_MEMORY_T,*/,
					     OUT void *                           /* pvParamData */
					    );

	VCODEC_ENC_ERROR_T(*pfnSetParameter)(IN HANDLE                            /* hCodec */,
					     /*VCODEC_ENC_QUALITY_T,VCODEC_ENC_CODEC_T,VIDEO_CODEC_ROTATE_ANGLE_T,*/
					     IN VCODEC_ENC_PARAM_TYPE_T        /* rEncodeParam*/,
					     IN void *                            /* pvParamData */
					    );

	VCODEC_ENC_ERROR_T(*pfnGenerateHeader)(IN HANDLE                                 /* hCodec */
					      );

	VCODEC_ENC_ERROR_T(*pfnEncodeOneUnit)(IN HANDLE                             /* hCodec */,
					      /*prInput*/  /*VCODEC_ENC_INPUT_YUV_INFO_T , VCODEC_ENC_INPUT_INFO_T*/
					      IN void *
					     );
	VCODEC_ENC_ERROR_T(*pfnDeInit)(IN HANDLE                                         /* hCodec */
				      );

	VCODEC_ENC_ERROR_T(*pfnClose)(IN HANDLE                                          /* hCodec */
				     );

	VCODEC_ENC_ERROR_T(*pfnGetNextBitstream)(IN HANDLE                             /* hCodec */,
						 OUT VCODEC_ENC_GENERAL_OUTPUT_T *  /* prPrivOutput*/
						);
} VCODEC_ENC_API_T;

typedef struct {
	VIDEO_ENC_API_T             rVideoEncAPI;
	VCODEC_ENC_API_T            *pfnVcodecEncAPI;
	HANDLE                      hDriver;
	VCODEC_BUFFER_T             rReleaseYUV;
	VCODEC_ENC_BUFFER_INFO_T    rEncoderBuffInfoNC;
} VIDEO_ENC_WRAP_HANDLE_T;

typedef struct {
	unsigned int u4TimeIncrResolution;
	unsigned int      u4BufferSize;
	VCODEC_BUFFER_T    *prBuffer;
} MPEG4_VT_ENCODER_GEN_VT_HEADER_INPUT_T;

VCODEC_ENC_ERROR_T MPEG4EncoderGenerateVTHeader(
	IN    MPEG4_VT_ENCODER_GEN_VT_HEADER_INPUT_T * prInput,
	OUT   unsigned int *pu4EncodedSize
);






#endif /* VCODEC_IF_H */
