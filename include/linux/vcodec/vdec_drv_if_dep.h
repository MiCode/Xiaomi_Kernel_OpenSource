#ifndef VDEC_DRV_IF_DEP_H // for 6572 only
#define VDEC_DRV_IF_DEP_H

/*=============================================================================
 *                              Include Files
 *===========================================================================*/

#include "val_types_private.h"
#include "vcodec_if_v2.h"
//#include "rv_format_info.h"
#include "wmc_type.h"
//#include "strm_iem.h"
#include "vcodec_dec_demuxer_if_v2.h"
//#include "ts_vcodec_common.h"
#define DumpInput__
#ifdef DumpInput__
#include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/*=============================================================================
 *                              Definition
 *===========================================================================*/
#define MAX_BUFFER_SIZE 21
/*typedef struct
{
    // for speedy mode
    VAL_UINT32_T    nBufferStatus;
    VAL_INT64_T     llLastVideoTime;
    VAL_INT64_T     llCurrentPlayTime;
} DRIVER_HANDLER_T;
*/
/*
typedef struct __RV9_DRV_DATA_T
{
    VAL_UINT32_T            uStreamHdrWidth;
    VAL_UINT32_T            uStreamHdrHeight;
    RM_DECODER_INPUT_PARAM_T rRM_INPUT_Data;
    payload_inf_st payload_inf_tab_rv9[200]; //set up 200
    RM_DECODER_PAYLOAD_INFO_T payload_inf_tab_rv9_MAUI[200]; //set up 200
} RV9_DRV_DATA_T, *P_RV9_DRV_DATA_T;
*/

typedef struct __H264_DRV_DATA_T
{
    H264_DECODER_PAYLOAD_INFO_T     prH264Payload;
    H264_DECODER_INPUT_PARAM_T      prInputData;
    H264_DECODER_PRIVATE_PARAM_T    rPrivateData;
} H264_DRV_DATA_T, *P_H264_DRV_DATA_T;

typedef struct __MPEG4_DRV_DATA_T
{
    MPEG4_DECODER_PAYLOAD_INFO_T     prMPEG4Payload;
    MPEG4_DECODER_INPUT_PARAM_T      prInputData;
} MPEG4_DRV_DATA_T, *P_MPEG4_DRV_DATA_T;

typedef struct __VP8_DRV_DATA_T
{
    //VP8_DEC_CUSTOM_SETTING_T        VP8CustSetting;
} VP8_DRV_DATA_T, *P_VP8_DRV_DATA_T;

typedef struct __VP9_DRV_DATA_T
{
    //VP9_DEC_CUSTOM_SETTING_T        VP9CustSetting;
} VP9_DRV_DATA_T, *P_VP9_DRV_DATA_T;

typedef struct __VC1_DRV_DATA_T
{
    VAL_BOOL_T                      bVC1FirstDecode;
    VC1_DECODER_PAYLOAD_INFO_T      prVC1Payload;
    VC1_DECODER_INPUT_PARAM_T       prInputData;
    TEMP_INTERFACE                  VC1TempInterface;
} VC1_DRV_DATA_T, *P_VC1_DRV_DATA_T;

typedef struct __VDEC_DRV_BUF_STATUS_T
{
    VAL_BOOL_T          bDisplay;
    VAL_BOOL_T          bFree;
    VDEC_DRV_FRAMEBUF_T *pFrameBuf;
} VDEC_DRV_BUF_STATUS_T, *P_VDEC_DRV_BUF_STATUS_T;

typedef enum
{
    VDEC_DRV_STATUS_OPEN_DONE   = 0x00000001,
    VDEC_DRV_STATUS_INIT_DONE   = 0x00000002,
    VDEC_DRV_STATUS_DECODE_EVER = 0x00000004
} VDEC_DRV_STATUS;


typedef struct __VDEC_HANDLE_T
{
    VAL_HANDLE_T            hHalHandle;    ///< HAL data.
    VAL_HANDLE_T            vHandle;       ///< for MMSYS power on/off
    VAL_MEMORY_T            rHandleMem;    ///< Save handle memory information to be used in release.
    VAL_BOOL_T              bFirstDecoded; /// < already pass first video data to codec
    VAL_BOOL_T              bHeaderPassed; /// < already pass video header to codec
    VAL_BOOL_T              bFlushAll;
    VAL_BOOL_T              bNewMemory;    /// allocate buffer for first DOU
    VAL_MEMORY_T            HeaderBuf;
    VAL_MEMORY_T            HeaderBufwithFrame;
    VAL_HANDLE_T            hCodec;
    DRIVER_HANDLER_T        hDrv;
    VIDEO_WRAP_HANDLE_T     hWrapper;
    VAL_UINT32_T            CustomSetting;
    VCODEC_BUFFER_T         rVideoBitBuf;
    VCODEC_DEC_INPUT_YUV_INFO_T rVideoFrameBuf;
    VCODEC_MEMORY_TYPE_T    rVideoDecMemType;
    VAL_UINT32_T            YUVBuffer[MAX_BUFFER_SIZE];
    VAL_UINT32_T            nYUVBufferIndex;
    VAL_UINT32_T            nDrvStatus;
    VDEC_DRV_BUF_STATUS_T   pFrameBufArray[MAX_BUFFER_SIZE];
    VDEC_DRV_FRAMEBUF_T     *DispFrameBuf, *FreeFrameBuf;
    VCODEC_OPEN_SETTING_T           codecOpenSetting;
    VCODEC_DEC_INPUT_T              rInputUnit;
    VIDEO_DECODER_INPUT_NC_T        rVideoDecInputNC;
    VCODEC_DEC_OUTPUT_PARAM_T       *rVideoDecOutputParam;
    VCODEC_DEC_PRIVATE_OUTPUT_T     rVideoDecOutput;
    VCODEC_DEC_OUTPUT_BUFFER_PARAM_T   rVideoDecYUVBufferParameter;
    VCODEC_DEC_INPUT_BUFFER_PARAM_T    rBitStreamParam;
    // for seek and thumbnail mode optimization
    VAL_BOOL_T                      bFirstDecodeForThumbnail;
    VAL_BOOL_T                      bThumbnailModeOK;
    VDEC_DRV_SET_DECODE_MODE_T      rSetDecodeMode;
    // for no VOS header when MPEG4
    VAL_UINT16_T            nDefWidth;
    VAL_UINT16_T            nDefHeight;

    VDEC_DRV_VIDEO_FORMAT_T CodecFormat;
    VAL_VOID_T              *prExtraData;  ///< Driver private data pointer.
    VAL_MEMORY_T             rExtraDataMem; ///< Save extra data memory information to be used in release.
    VCODEC_DEC_PRIVATE_OUTPUT_EXTRA_T prExtraDecOutput;
#ifdef DumpInput__
    FILE *pf_out;
#endif

    // Morris Yang 20111101 [
    VAL_UINT32_T  nOmxTids;
    // ]
#if 1   //defined(MT6572)     //VCODEC_MULTI_THREAD
    // Jackal Chen [
    VAL_VOID_T              *pDrvModule;    ///< used for dlopen and dlclose
    // ]
#endif
    VAL_BOOL_T              fgValInitFlag; ///< hValHandle is available or not
} VDEC_HANDLE_T;

/*=============================================================================
  *                             Function Declaration
  *===========================================================================*/

#ifdef __cplusplus
}
#endif

#endif /* VDEC_DRV_IF_DEP_H */

