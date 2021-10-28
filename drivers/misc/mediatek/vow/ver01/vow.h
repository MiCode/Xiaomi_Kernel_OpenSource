/* SPDX-License-Identifier: GPL-2.0 */
/*
 * vow.h  --  VoW platform driver definition
 *
 * Copyright (c) 2020 MediaTek Inc.
 * Author: Michael Hsiao <michael.hsiao@mediatek.com>
 */

#ifndef __VOW_H__
#define __VOW_H__

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include "vow_scp.h"

/*****************************************************************************
 * VOW Type Define
 *****************************************************************************/
#define DEBUG_VOWDRV 1

#if DEBUG_VOWDRV
#define VOWDRV_DEBUG(format, args...) pr_info(format, ##args)
#else
#define VOWDRV_DEBUG(format, args...)
#endif

#if IS_ENABLED(CONFIG_MTK_VOW_GVA_SUPPORT)
#define VOW_GOOGLE_MODEL 1
#else
#define VOW_GOOGLE_MODEL 0
#endif

#if IS_ENABLED(CONFIG_MTK_VOW_AMAZON_SUPPORT)
#define VOW_AMAZON_MODEL 1
#else
#define VOW_AMAZON_MODEL 0
#endif

#define VOW_DEVNAME                    "vow"
#define VOW_IOC_MAGIC                  'V'
#define VOW_PRE_LEARN_MODE             1
#define MAX_VOW_SPEAKER_MODEL          2
#define VOW_WAITCHECK_INTERVAL_MS      1
#define MAX_VOW_INFO_LEN               7
#define VOW_VOICE_RECORD_THRESHOLD     2560 /* 80ms */
#define VOW_VOICE_RECORD_BIG_THRESHOLD 8320 /* 260ms */
#define VOW_IPI_SEND_CNT_TIMEOUT       500 /* 500ms */
/* UBM_V1:0xA000, UBM_V2:0xDC00, UBM_V3: 2*0x11000 */
#define VOW_MODEL_SIZE                 0x11000
#define VOW_VOICEDATA_OFFSET           (VOW_MODEL_SIZE * MAX_VOW_SPEAKER_MODEL)
#define VOW_VOICEDATA_SIZE             0x12500 /* 74880, need over 2.3sec */
#define WORD_H                         8
#define WORD_L                         8
#define WORD_H_MASK                    0xFF00
#define WORD_L_MASK                    0x00FF
/* multiplier of cycle to ns in 13m clock */
#define CYCLE_TO_NS                    77
#define VOW_STOP_DUMP_WAIT             50
#define FRAME_BUF_SIZE                 8192
#define RESERVED_DATA                  4
#define VOW_RECOVERY_WAIT              100

/* length limitation sync by audio hal */
#if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT)
#define VOW_VBUF_LENGTH      (0x12E80 * 2)  /*(0x12480 + 0x0A00) * 2*/
#else
#define VOW_VBUF_LENGTH      (0x12E80)  /* 0x12480 + 0x0A00 */
#endif

#if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT)
#define VOW_RECOGDATA_OFFSET    (VOW_VOICEDATA_OFFSET + 2 * VOW_VOICEDATA_SIZE)
#else
#define VOW_RECOGDATA_OFFSET        (VOW_VOICEDATA_OFFSET + VOW_VOICEDATA_SIZE)
#endif
#define VOW_RECOGDATA_SIZE             0x2800

#define VOW_PCM_DUMP_BYTE_SIZE         0xA00 /* 320 * 8 */
#define VOW_ENGINE_INFO_LENGTH_BYTE    40

/* below is control message */
#define VOW_SET_CONTROL               _IOW(VOW_IOC_MAGIC, 0x03, unsigned int)
#define VOW_SET_SPEAKER_MODEL         _IOW(VOW_IOC_MAGIC, 0x04, unsigned int)
#define VOW_CLR_SPEAKER_MODEL         _IOW(VOW_IOC_MAGIC, 0x05, unsigned int)
#define VOW_SET_APREG_INFO            _IOW(VOW_IOC_MAGIC, 0x09, unsigned int)
#define VOW_BARGEIN_ON                _IOW(VOW_IOC_MAGIC, 0x0A, unsigned int)
#define VOW_BARGEIN_OFF               _IOW(VOW_IOC_MAGIC, 0x0B, unsigned int)
#define VOW_CHECK_STATUS              _IOW(VOW_IOC_MAGIC, 0x0C, unsigned int)
#define VOW_RECOG_ENABLE              _IOW(VOW_IOC_MAGIC, 0x0D, unsigned int)
#define VOW_RECOG_DISABLE             _IOW(VOW_IOC_MAGIC, 0x0E, unsigned int)
#define VOW_MODEL_START               _IOW(VOW_IOC_MAGIC, 0x0F, unsigned int)
#define VOW_MODEL_STOP                _IOW(VOW_IOC_MAGIC, 0x10, unsigned int)
#define VOW_GET_ALEXA_ENGINE_VER      _IOW(VOW_IOC_MAGIC, 0x11, unsigned int)
#define VOW_GET_GOOGLE_ENGINE_VER     _IOW(VOW_IOC_MAGIC, 0x12, unsigned int)
#define VOW_GET_GOOGLE_ARCH           _IOW(VOW_IOC_MAGIC, 0x13, unsigned int)
#define VOW_READ_VOICE_DATA           _IOW(VOW_IOC_MAGIC, 0x17, unsigned int)

#ifdef VOW_ECHO_SW_SRC
#define VOW_BARGEIN_DUMP_OFFSET 0x1E00
#else
#define VOW_BARGEIN_DUMP_OFFSET 0xA00
#endif
#define VOW_BARGEIN_DUMP_SIZE    0x3C00

#define KERNEL_VOW_DRV_VER "1.1.1"
struct dump_package_t {
	uint32_t dump_data_type;
	uint32_t mic_offset;
	uint32_t mic_data_size;
	uint32_t recog_data_offset;
	uint32_t recog_data_size;
#if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT)
	uint32_t mic_offset_R;
	uint32_t mic_data_size_R;
	uint32_t recog_data_offset_R;
	uint32_t recog_data_size_R;
#endif  /* #if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT) */
	uint32_t echo_offset;
	uint32_t echo_data_size;
};

struct dump_queue_t {
	struct dump_package_t dump_package[256];
	uint8_t idx_r;
	uint8_t idx_w;
};

struct dump_work_t {
	struct work_struct work;
	uint32_t mic_offset;
	uint32_t mic_data_size;
	uint32_t recog_data_offset;
	uint32_t recog_data_size;
#if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT)
	uint32_t mic_offset_R;
	uint32_t mic_data_size_R;
	uint32_t recog_data_offset_R;
	uint32_t recog_data_size_R;
#endif  /* #if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT) */
	uint32_t echo_offset;
	uint32_t echo_data_size;
};

enum { /* dump_data_t */
	DUMP_RECOG = 0,
	DUMP_BARGEIN,
	NUM_DUMP_DATA,
};


/*****************************************************************************
 * VOW Enum
 *****************************************************************************/
enum vow_control_cmd_t {
	VOWControlCmd_Init = 0,
	VOWControlCmd_EnableDebug,
	VOWControlCmd_DisableDebug,
	VOWControlCmd_EnableSeamlessRecord,
	VOWControlCmd_EnableDump,
	VOWControlCmd_DisableDump,
	VOWControlCmd_Reset,
};

enum vow_ipi_msgid_t {
	IPIMSG_VOW_ENABLE = 0,
	IPIMSG_VOW_DISABLE = 1,
	IPIMSG_VOW_SETMODE = 2,
	IPIMSG_VOW_APREGDATA_ADDR = 3,
	IPIMSG_VOW_SET_MODEL = 4,
	IPIMSG_VOW_SET_FLAG = 5,
	IPIMSG_VOW_SET_SMART_DEVICE = 6,
	IPIMSG_VOW_SET_BARGEIN_ON = 10,
	IPIMSG_VOW_SET_BARGEIN_OFF = 11,
	IPIMSG_VOW_PCM_DUMP_ON = 12,
	IPIMSG_VOW_PCM_DUMP_OFF = 13,
	IPIMSG_VOW_COMBINED_INFO = 17,
	IPIMSG_VOW_MODEL_START = 18,
	IPIMSG_VOW_MODEL_STOP = 19,
	IPIMSG_VOW_RETURN_VALUE = 20,
	IPIMSG_VOW_SET_SKIP_SAMPLE_COUNT = 21,
	IPIMSG_VOW_GET_ALEXA_ENGINE_VER = 22,
	IPIMSG_VOW_GET_GOOGLE_ENGINE_VER = 23,
	IPIMSG_VOW_GET_GOOGLE_ARCH = 24,
	IPIMSG_VOW_ALEXA_ENGINE_VER = 25,
	IPIMSG_VOW_GOOGLE_ENGINE_VER = 26,
	IPIMSG_VOW_GOOGLE_ARCH = 27
};

enum vow_eint_status_t {
	VOW_EINT_DISABLE = -2,
	VOW_EINT_FAIL = -1,
	VOW_EINT_PASS = 0,
	VOW_EINT_RETRY = 1,
	NUM_OF_VOW_EINT_STATUS = 4
};

enum vow_flag_type_t {
	VOW_FLAG_DEBUG = 0,
	VOW_FLAG_PRE_LEARN,
	VOW_FLAG_DMIC_LOWPOWER,
	VOW_FLAG_PERIODIC_ENABLE,
	VOW_FLAG_FORCE_PHASE1_DEBUG,
	VOW_FLAG_FORCE_PHASE2_DEBUG,
	VOW_FLAG_SWIP_LOG_PRINT,
	VOW_FLAG_MTKIF_TYPE,
	VOW_FLAG_SEAMLESS,
	VOW_FLAG_DUAL_MIC_LCH,
	NUM_OF_VOW_FLAG_TYPE
};

enum vow_pwr_status_t {
	VOW_PWR_OFF = 0,
	VOW_PWR_ON = 1,
	NUM_OF_VOW_PWR_STATUS
};

enum vow_ipi_result_t {
	VOW_IPI_SUCCESS = 0,
	VOW_IPI_CLR_SMODEL_ID_NOTMATCH,
	VOW_IPI_SET_SMODEL_NO_FREE_SLOT,
};

enum vow_force_phase_t {
	NO_FORCE = 0,
	FORCE_PHASE1,
	FORCE_PHASE2,
};

enum vow_model_type_t {
	VOW_MODEL_INIT = 0,    /* no use */
	VOW_MODEL_SPEAKER = 1,
	VOW_MODEL_NOISE = 2,   /* no use */
	VOW_MODEL_FIR = 3,      /* no use */
	VOW_MODEL_CLEAR = 4
};

enum vow_mtkif_type_t {
	VOW_MTKIF_NONE = 0,
	VOW_MTKIF_AMIC = 1,
	VOW_MTKIF_DMIC = 2,
	VOW_MTKIF_DMIC_LP = 3,
};

enum vow_channel_t {
	VOW_MONO = 0,
	VOW_STEREO = 1,
};

enum vow_model_status_t {
	VOW_MODEL_STATUS_START = 1,
	VOW_MODEL_STATUS_STOP = 0
};

enum vow_model_control_t {
	VOW_SET_MODEL = 0,
	VOW_CLEAN_MODEL = 1
};

struct vow_engine_version_t {
	long return_size_addr;
	long data_addr;
};

struct vow_engine_version_kernel_t {
	compat_size_t return_size_addr;
	compat_size_t data_addr;
};

enum {
	VENDOR_ID_MTK = 77,     //'M'
	VENDOR_ID_AMAZON = 65,  //'A'
	VENDOR_ID_OTHERS = 71,
	VENDOR_ID_NONE = 0
};

/*****************************************************************************
 * VOW Structure Define
 *****************************************************************************/
struct vow_ipi_msg_t {
	short id;
	short size;
	short *buf;
};

struct vow_eint_data_struct_t {
	int size;        /* size of data section */
	int eint_status; /* eint status */
	int id;          /* user id */
	char data[RESERVED_DATA];    /* reserved for future extension */
};


#if IS_ENABLED(CONFIG_COMPAT)

struct vow_speaker_model_t {
	void *model_ptr;
	int  id;
	int  keyword;
	int  uuid;
	int  flag;
	int  enabled;
	unsigned int model_size;
	unsigned int confidence_lv;
};

struct vow_model_info_t {
	long  id;
	long  keyword;
	long  addr;
	long  size;
	long  return_size_addr;
	long  uuid;
	void *data;
};

struct vow_model_start_t {
	long handle;
	long confidence_level;
};

struct vow_speaker_model_kernel_t {
	compat_uptr_t *model_ptr;
	compat_size_t  id;
	compat_size_t  enabled;
};

struct vow_model_info_kernel_t {
	compat_size_t  id;
	compat_size_t  keyword;
	compat_size_t  addr;
	compat_size_t  size;
	compat_size_t  return_size_addr;
	compat_size_t  uuid;
	compat_uptr_t *data;
};

struct vow_model_start_kernel_t {
	compat_size_t handle;
	compat_size_t confidence_level;
};

struct vow_engine_info_t {
	long return_size_addr;
	long data_addr;
};

struct vow_engine_info_kernel_t {
	compat_size_t return_size_addr;
	compat_size_t data_addr;
};

#else  /* #if IS_ENABLED(CONFIG_COMPAT) */

struct vow_speaker_model_t {
	void *model_ptr;
	int  id;
	int  uuid;
	int  flag;
	int  enabled;
	unsigned int model_size;
	unsigned int confidence_lv;
};

struct vow_model_info_t {
	long  id;
	long  keyword;
	long  addr;
	long  size;
	long  return_size_addr;
	long  uuid;
	void *data;
};

struct vow_model_start_t {
	long handle;
	long confidence_level;
};

struct vow_engine_version_t {
	long return_size_addr;
	long data_addr;
};
#endif  /* #if IS_ENABLED(CONFIG_COMPAT) */

enum ipi_type_flag_t {
	RECOG_OK_IDX = 0,
	DEBUG_DUMP_IDX = 1,
	RECOG_DUMP_IDX = 2,
	BARGEIN_DUMP_INFO_IDX = 3,
	BARGEIN_DUMP_IDX = 4
};

#define RECOG_OK_IDX_MASK           (0x01 << RECOG_OK_IDX)
#define DEBUG_DUMP_IDX_MASK         (0x01 << DEBUG_DUMP_IDX)
#define RECOG_DUMP_IDX_MASK         (0x01 << RECOG_DUMP_IDX)
#define BARGEIN_DUMP_INFO_IDX_MASK  (0x01 << BARGEIN_DUMP_INFO_IDX)
#define BARGEIN_DUMP_IDX_MASK       (0x01 << BARGEIN_DUMP_IDX)

struct vow_ipi_combined_info_t {
	unsigned int ipi_type_flag;
	/* IPIMSG_VOW_RECOGNIZE_OK */
	unsigned int recog_ret_info;
	unsigned int recog_ok_uuid;
	unsigned int confidence_lv;
	unsigned long long recog_ok_os_timer;
	/* IPIMSG_VOW_DATAREADY */
	unsigned int voice_buf_offset;
	unsigned int voice_length;
	/* IPIMSG_VOW_BARGEIN_DUMP_INFO */
	unsigned int dump_frm_cnt;
	unsigned int voice_sample_delay;
	/* IPIMSG_VOW_BARGEIN_PCMDUMP_OK */
	unsigned int mic_dump_size;
	unsigned int mic_offset;
#if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT)
	unsigned int mic_dump_size_R;
	unsigned int mic_offset_R;
#endif  /* #if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT) */
	unsigned int echo_dump_size;
	unsigned int echo_offset;
	unsigned int recog_dump_size;
	unsigned int recog_dump_offset;
#if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT)
	unsigned int recog_dump_size_R;
	unsigned int recog_dump_offset_R;
#endif  /* #if IS_ENABLED(CONFIG_MTK_VOW_DUAL_MIC_SUPPORT) */
};

#endif /*__VOW_H__ */
