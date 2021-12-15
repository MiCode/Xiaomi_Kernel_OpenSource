/* SPDX-License-Identifier: GPL-2.0 */
//
// Copyright (c) 2015 MediaTek Inc.

#ifndef _MT_FDVT_H
#define _MT_FDVT_H

#include <linux/ioctl.h>

#ifdef CONFIG_COMPAT
/* 64 bit */
#include <linux/fs.h>
#include <linux/compat.h>
#endif

/*
 *   enforce kernel log enable
 */
#define KERNEL_LOG		/* enable debug log flag if defined */

#define MAX_FDVT_FRAME_REQUEST 32
#define MAX_FDVT_REQUEST_RING_SIZE 32


#define SIG_ERESTARTSYS 512	/* ERESTARTSYS */
/*
 *
 */
#define FDVT_DEV_MAJOR_NUMBER    258

#define FDVT_MAGIC               'N'

#define FDVT_REG_RANGE           (0x1000)

#ifdef CONFIG_MACH_MT6781
#define FDVT_BASE_HW   0x1C001000
#else
#define FDVT_BASE_HW   0x1B001000
#endif

#define MAX_FACE_NUM   1024

/*This macro is for setting irq status represnted
 * by a local variable,FDVTInfo.IrqInfo.status[FDVT_IRQ_TYPE_INT_FDVT_ST]
 */
#define FDVT_INT_ST                 (1<<0)


struct FDVT_REG_STRUCT {
	unsigned int module;
	unsigned int addr;	/* register's addr */
	unsigned int val;	/* register's value */
};
#define FDVT_REG_STRUCT struct FDVT_REG_STRUCT

struct FDVT_REG_IO_STRUCT {
	FDVT_REG_STRUCT *pData;	/* pointer to FDVT_REG_STRUCT */
	unsigned int count;	/* count */
};
#define FDVT_REG_IO_STRUCT struct FDVT_REG_IO_STRUCT

/*
 *   interrupt clear type
 */
enum FDVT_IRQ_CLEAR_ENUM {
	FDVT_IRQ_CLEAR_NONE,	/* non-clear wait, clear after wait */
	FDVT_IRQ_CLEAR_WAIT,	/* clear wait, clear before and after wait */
/* wait the signal and clear it, avoid the hw executime is too s hort. */
	FDVT_IRQ_WAIT_CLEAR,
	FDVT_IRQ_CLEAR_STATUS,	/* clear specific status only */
	FDVT_IRQ_CLEAR_ALL	/* clear all status */
};
#define FDVT_IRQ_CLEAR_ENUM enum FDVT_IRQ_CLEAR_ENUM
/*
 *   module's interrupt , each module should have its own isr.
 *   note:
 *   mapping to isr table,ISR_TABLE when using no device tree
 */
enum FDVT_IRQ_TYPE_ENUM {
	FDVT_IRQ_TYPE_INT_FDVT_ST,	/* FDVT */
	FDVT_IRQ_TYPE_AMOUNT
};
#define FDVT_IRQ_TYPE_ENUM enum FDVT_IRQ_TYPE_ENUM

struct FDVT_WAIT_IRQ_STRUCT {
	FDVT_IRQ_CLEAR_ENUM clear;
	FDVT_IRQ_TYPE_ENUM type;
	unsigned int status;	/*IRQ status */
	unsigned int timeout;
	int user_key;		/* user key for doing interrupt operation */
	int process_id;		/* user process_id (will filled in kernel) */
	unsigned int dump_reg;	/* check dump register or not */
	bool isSecure;
};
#define FDVT_WAIT_IRQ_STRUCT struct FDVT_WAIT_IRQ_STRUCT

struct FDVT_CLEAR_IRQ_STRUCT {
	FDVT_IRQ_TYPE_ENUM type;
	int user_key;		/* user key for doing interrupt operation */
	unsigned int status;	/* Input */
};
#define FDVT_CLEAR_IRQ_STRUCT struct FDVT_CLEAR_IRQ_STRUCT

struct FDVT_ROI {
	unsigned int x1;
	unsigned int y1;
	unsigned int x2;
	unsigned int y2;
};

struct FDVT_PADDING {
	unsigned int left;
	unsigned int right;
	unsigned int down;
	unsigned int up;
};

enum FDVTFORMAT {
	FMT_NA = 0,
	FMT_YUV_2P = 1,
	FMT_YVU_2P = 2,
	FMT_YUYV = 3, //1plane
	FMT_YVYU = 4, //1plane
	FMT_UYVY = 5, //1plane
	FMT_VYUY = 6, //1plane
	FMT_MONO = 7  //AIE2.0
};

#define FDVTFORMAT enum FDVTFORMAT

struct FDVT_MetaDataToGCE {
	unsigned int ImgSrcY_Handler;
	unsigned int ImgSrcUV_Handler;
	unsigned int YUVConfig_Handler;
	unsigned int YUVOutBuf_Handler;
	unsigned int RSConfig_Handler;
	unsigned int RSOutBuf_Handler;
	unsigned int FDConfig_Handler;
	unsigned int FDOutBuf_Handler;
	unsigned int FD_POSE_Config_Handler;
	unsigned int FDResultBuf_MVA;
	unsigned int ImgSrc_Y_Size;
	unsigned int ImgSrc_UV_Size;
	unsigned int YUVConfigSize;
	unsigned int YUVOutBufSize;
	unsigned int RSConfigSize;
	unsigned int RSOutBufSize;
	unsigned int FDConfigSize;
	unsigned int FD_POSE_ConfigSize;
	unsigned int FDOutBufSize;
	unsigned int FDResultBufSize;
	unsigned int FDMode;
	unsigned int srcImgFmt;
	unsigned int srcImgWidth;
	unsigned int srcImgHeight;
	unsigned int maxWidth;
	unsigned int maxHeight;
	unsigned int rotateDegree;
	unsigned short featureTH;
	unsigned short SecMemType;
	unsigned int enROI;
	struct FDVT_ROI src_roi;
	unsigned int enPadding;
	struct FDVT_PADDING src_padding;
	unsigned int SRC_IMG_STRIDE;
	unsigned int pyramid_width;
	unsigned int pyramid_height;
	bool isReleased;
};
#define FDVT_MetaDataToGCE struct FDVT_MetaDataToGCE

struct FDRESULT {
	unsigned short anchor_x0[MAX_FACE_NUM];
	unsigned short anchor_x1[MAX_FACE_NUM];
	unsigned short anchor_y0[MAX_FACE_NUM];
	unsigned short anchor_y1[MAX_FACE_NUM];
	unsigned short landmark_x0[MAX_FACE_NUM];
	unsigned short landmark_x1[MAX_FACE_NUM];
	unsigned short landmark_x2[MAX_FACE_NUM];
	unsigned short landmark_x3[MAX_FACE_NUM];
	unsigned short landmark_x4[MAX_FACE_NUM];
	unsigned short landmark_x5[MAX_FACE_NUM];
	unsigned short landmark_x6[MAX_FACE_NUM];
	unsigned short landmark_y0[MAX_FACE_NUM];
	unsigned short landmark_y1[MAX_FACE_NUM];
	unsigned short landmark_y2[MAX_FACE_NUM];
	unsigned short landmark_y3[MAX_FACE_NUM];
	unsigned short landmark_y4[MAX_FACE_NUM];
	unsigned short landmark_y5[MAX_FACE_NUM];
	unsigned short landmark_y6[MAX_FACE_NUM];
	signed short anchor_score[MAX_FACE_NUM];
	signed short landmark_score0[MAX_FACE_NUM];
	signed short landmark_score1[MAX_FACE_NUM];
	signed short landmark_score2[MAX_FACE_NUM];
	signed short landmark_score3[MAX_FACE_NUM];
	signed short landmark_score4[MAX_FACE_NUM];
	signed short landmark_score5[MAX_FACE_NUM];
	signed short landmark_score6[MAX_FACE_NUM];

	signed short rip_landmark_score0[MAX_FACE_NUM];
	signed short rip_landmark_score1[MAX_FACE_NUM];
	signed short rip_landmark_score2[MAX_FACE_NUM];
	signed short rip_landmark_score3[MAX_FACE_NUM];
	signed short rip_landmark_score4[MAX_FACE_NUM];
	signed short rip_landmark_score5[MAX_FACE_NUM];
	signed short rip_landmark_score6[MAX_FACE_NUM];

	signed short rop_landmark_score0[MAX_FACE_NUM];
	signed short rop_landmark_score1[MAX_FACE_NUM];
	signed short rop_landmark_score2[MAX_FACE_NUM];
	unsigned short face_result_index[MAX_FACE_NUM];
	unsigned short anchor_index[MAX_FACE_NUM];
	unsigned int fd_partial_result;
};

struct FD_RESULT {
	struct FDRESULT PYRAMID0_RESULT;
	struct FDRESULT PYRAMID1_RESULT;
	struct FDRESULT PYRAMID2_RESULT;
	unsigned short FD_TOTAL_NUM;
	unsigned long *FD_raw_result_0_va; // AIE2.0 for bit-true test only
	unsigned long *FD_raw_result_1_va; // AIE2.0 for bit-true test only
	unsigned long *FD_raw_result_2_va; // AIE2.0 for bit-true test only
	unsigned long *FD_raw_result_run2_0_va; // AIE2.0 for bit-true test only
	unsigned long *FD_raw_result_run2_1_va; // AIE2.0 for bit-true test only
	unsigned long *FD_raw_result_run2_2_va; // AIE2.0 for bit-true test only
};

struct RACERESULT {
	signed short RESULT[4][64]; // RESULT[Channel][Feature]
};

struct GENDERRESULT {
	signed short RESULT[2][64]; // RESULT[Channel][Feature]
};

struct RIPRESULT {
	signed short RESULT[7][64]; // RESULT[Channel][Feature]
};

struct ROPRESULT {
	signed short RESULT[3][64]; // RESULT[Channel][Feature]
};

struct MERGED_RACERESULT {  // AIE2.0
	signed short RESULT[4]; // RESULT[Feature]
};

struct MERGED_GENDERRESULT {  // AIE2.0
	signed short RESULT[2]; // RESULT[Feature]
};

struct MERGED_AGERESULT {  // AIE2.0
	signed short RESULT[2]; // RESULT[Feature]
};

struct MERGED_IS_INDIANRESULT {  // AIE2.0
	signed short RESULT[2]; // RESULT[Feature]
};

struct MERGED_RIPRESULT {  // AIE2.0
	signed short RESULT[7]; // RESULT[Feature]
};

struct MERGED_ROPRESULT {  // AIE2.0
	signed short RESULT[3]; // RESULT[Feature]
};

struct ATTRIBUTE_RESULT {
	struct GENDERRESULT GENDER_RESULT;
	struct RACERESULT RACE_RESULT;
	struct MERGED_AGERESULT MERGED_AGE_RESULT;
	struct MERGED_GENDERRESULT MERGED_GENDER_RESULT;
	struct MERGED_IS_INDIANRESULT MERGED_IS_INDIAN_RESULT;
	struct MERGED_RACERESULT MERGED_RACE_RESULT;
	unsigned long *ATTR_raw_result_0_va; // AIE2.0 for bit-true test only
	unsigned long *ATTR_raw_result_1_va; // AIE2.0 for bit-true test only
	unsigned long *ATTR_raw_result_2_va; // AIE2.0 for bit-true test only
	unsigned long *ATTR_raw_result_3_va; // AIE2.0 for bit-true test only
};

struct POSE_RESULT {
	struct RIPRESULT RIP_RESULT;
	struct ROPRESULT ROP_RESULT;
	struct MERGED_RIPRESULT MERGED_RIP_RESULT;
	struct MERGED_ROPRESULT MERGED_ROP_RESULT;
};


struct fdvt_config {
	/*fdvt_reg_t REG_STRUCT;*/
	unsigned int FDVT_RSCON_BASE_ADR;
	unsigned int FDVT_YUV2RGB;
	unsigned int FDVT_YUV2RGBCON_BASE_ADR;
	unsigned int FDVT_FD_CON_BASE_ADR;
	unsigned int FDVT_FD_POSE_CON_BASE_ADR;
	unsigned int FDVT_YUV_SRC_WD_HT;
	unsigned int FD_MODE;
	unsigned int RESULT;
	unsigned int RESULT1;
	unsigned int FDVT_IS_SECURE;
	unsigned int FDVT_RSCON_BUFSIZE;
	unsigned int FDVT_YUV2RGBCON_BUFSIZE;
	unsigned int FDVT_FD_CON_BUFSIZE;
	unsigned int FDVT_FD_POSE_CON_BUFSIZE;
	unsigned int FDVT_LOOPS_OF_FDMODE;
	unsigned int FDVT_NUMBERS_OF_PYRAMID;
	struct FD_RESULT *FDOUTPUT;
	struct ATTRIBUTE_RESULT *ATTRIBUTEOUTPUT;
	struct POSE_RESULT *POSEOUTPUT;
	FDVT_MetaDataToGCE FDVT_METADATA_TO_GCE;
	unsigned int *FDVT_IMG_Y_VA;
	unsigned int *FDVT_IMG_UV_VA;
	unsigned int FDVT_IMG_Y_FD;
	unsigned int FDVT_IMG_UV_FD;
	unsigned int FDVT_IMG_Y_OFFSET;
	unsigned int FDVT_IMG_UV_OFFSET;
	unsigned int SRC_IMG_STRIDE;
	struct FDVT_ROI src_roi;
	FDVTFORMAT SRC_IMG_FMT;
	unsigned int enROI;
	unsigned int IS_LEGACY;
};
#define FDVT_Config struct fdvt_config

/*
 *
 */
enum FDVT_CMD_ENUM {
	FDVT_CMD_RESET,		/* Reset */
	FDVT_CMD_DUMP_REG,	/* Dump FDVT Register */
	FDVT_CMD_DUMP_ISR_LOG,	/* Dump FDVT ISR log */
	FDVT_CMD_READ_REG,	/* Read register from driver */
	FDVT_CMD_WRITE_REG,	/* Write register to driver */
	FDVT_CMD_WAIT_IRQ,	/* Wait IRQ */
	FDVT_CMD_CLEAR_IRQ,	/* Clear IRQ */
	FDVT_CMD_ENQUE_NUM,	/* FDVT Enque Number */
	FDVT_CMD_ENQUE,		/* FDVT Enque */
	FDVT_CMD_ENQUE_REQ,	/* FDVT Enque Request */
	FDVT_CMD_DEQUE_NUM,	/* FDVT Deque Number */
	FDVT_CMD_DEQUE,		/* FDVT Deque */
	FDVT_CMD_DEQUE_REQ,	/* FDVT Deque Request */
	FDVT_CMD_TOTAL,
};
/*  */

struct FDVT_Request {
	unsigned int m_ReqNum;
	FDVT_Config *m_pFdvtConfig;
};
#define FDVT_Request struct FDVT_Request

#ifdef CONFIG_COMPAT
struct compat_FDVT_REG_IO_STRUCT {
	compat_uptr_t pData;
	unsigned int count;	/* count */
};
#define compat_FDVT_REG_IO_STRUCT struct compat_FDVT_REG_IO_STRUCT

struct compat_FDVT_Request {
	unsigned int m_ReqNum;
	compat_uptr_t m_pFdvtConfig;
};
#define compat_FDVT_Request struct compat_FDVT_Request

#endif

#define FDVT_RESET           _IO(FDVT_MAGIC, FDVT_CMD_RESET)
#define FDVT_DUMP_REG        _IO(FDVT_MAGIC, FDVT_CMD_DUMP_REG)
#define FDVT_DUMP_ISR_LOG    _IO(FDVT_MAGIC, FDVT_CMD_DUMP_ISR_LOG)


#define FDVT_READ_REGISTER \
	_IOWR(FDVT_MAGIC, FDVT_CMD_READ_REG,        FDVT_REG_IO_STRUCT)
#define FDVT_WRITE_REGISTER \
	_IOWR(FDVT_MAGIC, FDVT_CMD_WRITE_REG,       FDVT_REG_IO_STRUCT)
#define FDVT_WAIT_IRQ \
	_IOW(FDVT_MAGIC, FDVT_CMD_WAIT_IRQ,        FDVT_WAIT_IRQ_STRUCT)
#define FDVT_CLEAR_IRQ \
	_IOW(FDVT_MAGIC, FDVT_CMD_CLEAR_IRQ,       FDVT_CLEAR_IRQ_STRUCT)

#define FDVT_ENQNUE_NUM  _IOW(FDVT_MAGIC, FDVT_CMD_ENQUE_NUM,    int)
#define FDVT_ENQUE      _IOWR(FDVT_MAGIC, FDVT_CMD_ENQUE,      FDVT_Config)
#define FDVT_ENQUE_REQ  _IOWR(FDVT_MAGIC, FDVT_CMD_ENQUE_REQ,  FDVT_Request)

#define FDVT_DEQUE_NUM  _IOR(FDVT_MAGIC, FDVT_CMD_DEQUE_NUM,    int)
#define FDVT_DEQUE      _IOWR(FDVT_MAGIC, FDVT_CMD_DEQUE,      FDVT_Config)
#define FDVT_DEQUE_REQ  _IOWR(FDVT_MAGIC, FDVT_CMD_DEQUE_REQ,  FDVT_Request)


#ifdef CONFIG_COMPAT
#define COMPAT_FDVT_WRITE_REGISTER \
	_IOWR(FDVT_MAGIC, FDVT_CMD_WRITE_REG,     compat_FDVT_REG_IO_STRUCT)
#define COMPAT_FDVT_READ_REGISTER \
	_IOWR(FDVT_MAGIC, FDVT_CMD_READ_REG,      compat_FDVT_REG_IO_STRUCT)

#define COMPAT_FDVT_ENQUE_REQ \
	_IOWR(FDVT_MAGIC, FDVT_CMD_ENQUE_REQ,  compat_FDVT_Request)
#define COMPAT_FDVT_DEQUE_REQ \
	_IOWR(FDVT_MAGIC, FDVT_CMD_DEQUE_REQ,  compat_FDVT_Request)

#endif

extern int m4u_gz_sec_init(int);
/*  */
#endif
