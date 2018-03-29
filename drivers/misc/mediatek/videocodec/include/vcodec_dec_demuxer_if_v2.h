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


#ifndef VCODEC_DEC_DEMUXER_V2_IF_H
#define VCODEC_DEC_DEMUXER_V2_IF_H


typedef struct {
	unsigned char *u4Address;
	unsigned int u4Length;
	int fgValid;
} RM_DECODER_PAYLOAD_INFO_T;

typedef struct {
	unsigned int u4PayloadNumber;
	RM_DECODER_PAYLOAD_INFO_T *pu1PayloadAddress;
} RM_DECODER_INPUT_PARAM_T;

typedef enum {
	RV8 = 0,
	RV9,
	RV10
} RM_CODEC_VERSION_T;

#define MAX_NUM_RPR_SIZES 8
typedef struct {
	RM_CODEC_VERSION_T eDecoderVersion;
	unsigned int u4MaxDimWidth;
	unsigned int u4MaxDimHeight;
	unsigned int u4NumRPRSizes;
	unsigned int au4RPRSizes[2 * MAX_NUM_RPR_SIZES];
} RM_DECODER_INIT_PARAM_T;

typedef struct {
	unsigned int MaxSupportWidthForYUV420_ASP;
	unsigned int MaxSupportHeightForYUV420_ASP;
	unsigned short u2FrameWidthInContainer;
	unsigned short u2FrameHeightInContainer;
} MPEG4_DECODER_PRIVATE_PARAM_T;

/* The H264 uses the private data to transfer NAL units */
/* The related data structure information are defined as below */
/*  */

typedef struct {
	unsigned char *u4Address;
	unsigned int u4Length;
} H264_DECODER_PAYLOAD_INFO_T;

typedef struct {
	unsigned int u4PayloadNumber;
	H264_DECODER_PAYLOAD_INFO_T *pu1PayloadAddress;
} H264_DECODER_INPUT_PARAM_T;

typedef struct {
	unsigned char *u4Address;
	unsigned int u4Length;
} VC1_DECODER_PAYLOAD_INFO_T;

typedef struct {
	unsigned int u4PayloadNumber;
	VC1_DECODER_PAYLOAD_INFO_T *pu1PayloadAddress;
} VC1_DECODER_INPUT_PARAM_T;

typedef struct {
	unsigned char *u4Address;
	unsigned int u4Length;
	unsigned short u2FrameWidthInContainer;
	unsigned short u2FrameHeightInContainer;
} MPEG4_DECODER_PAYLOAD_INFO_T;

typedef struct {
	unsigned int u4PayloadNumber;
	MPEG4_DECODER_PAYLOAD_INFO_T *pu1PayloadAddress;
} MPEG4_DECODER_INPUT_PARAM_T;
typedef struct {
	unsigned char *u4Address;
	unsigned int u4Length;
} MPEG4VT_DECODER_PAYLOAD_INFO_T;

typedef struct {
	unsigned int u4PayloadNumber;
	MPEG4VT_DECODER_PAYLOAD_INFO_T *pu1PayloadAddress;
} MPEG4VT_DECODER_INPUT_PARAM_T;

typedef struct {
	VCODEC_BUFFER_T     rPayload;
	unsigned int        u4Length;
} VP8_DECODER_INPUT_UNIT_T;

typedef struct {
	unsigned char *u4Address;
	unsigned int u4Length;
} HEVC_DECODER_PAYLOAD_INFO_T;

typedef struct {
	unsigned int u4PayloadNumber;
	HEVC_DECODER_PAYLOAD_INFO_T *pu1Payload;
} HEVC_DECODER_INPUT_PARAM_T;
#endif /* VCODEC_DEC_DEMUXER_IF_H */
