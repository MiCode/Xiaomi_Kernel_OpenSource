/*
 * Copyright (C) 2017 MediaTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See http://www.gnu.org/licenses/gpl-2.0.html for more details.
 */

#ifndef __VOW_H__
#define __VOW_H__

#include <linux/compat.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

/*****************************************************************************
 * VOW Type Define
 *****************************************************************************/
#define DEBUG_VOWDRV 1

#if DEBUG_VOWDRV
#define VOWDRV_DEBUG(format, args...) pr_debug(format, ##args)
#else
#define VOWDRV_DEBUG(format, args...)
#endif


#define VOW_DEVNAME                    "vow"
#define VOW_IOC_MAGIC                  'V'
#define VOW_PRE_LEARN_MODE             1
#define MAX_VOW_SPEAKER_MODEL          10
#define VOW_WAITCHECK_INTERVAL_MS      1
#define MAX_VOW_INFO_LEN               5
#define VOW_VOICE_RECORD_THRESHOLD     2560 /* 80ms */
#define VOW_VOICE_RECORD_BIG_THRESHOLD 8000 /* 250ms */
#define VOW_IPI_SEND_CNT_TIMEOUT       500 /* 500ms */
#define VOW_VOICEDATA_OFFSET           0xA000 /* VOW VOICE DATA DRAM OFFSET */
#define WORD_H                         8
#define WORD_L                         8
#define WORD_H_MASK                    0xFF00
#define WORD_L_MASK                    0x00FF

/* below is control message */
#define VOW_SET_CONTROL               _IOW(VOW_IOC_MAGIC, 0x03, unsigned int)
#define VOW_SET_SPEAKER_MODEL         _IOW(VOW_IOC_MAGIC, 0x04, unsigned int)
#define VOW_CLR_SPEAKER_MODEL         _IOW(VOW_IOC_MAGIC, 0x05, unsigned int)
#define VOW_SET_APREG_INFO            _IOW(VOW_IOC_MAGIC, 0x09, unsigned int)
#define VOW_CHECK_STATUS              _IOW(VOW_IOC_MAGIC, 0x0C, unsigned int)

/*****************************************************************************
 * VOW Enum
 *****************************************************************************/
enum vow_control_cmd_t {
	VOWControlCmd_Init = 0,
	VOWControlCmd_ReadVoiceData,
	VOWControlCmd_EnableDebug,
	VOWControlCmd_DisableDebug,
};

enum vow_ipi_msgid_t {
	IPIMSG_VOW_ENABLE = 0,
	IPIMSG_VOW_DISABLE = 1,
	IPIMSG_VOW_SETMODE = 2,
	IPIMSG_VOW_APREGDATA_ADDR = 3,
	IPIMSG_VOW_SET_MODEL = 4,
	IPIMSG_VOW_SET_FLAG = 5,
	IPIMSG_VOW_SET_SMART_DEVICE = 6,
	IPIMSG_VOW_DATAREADY_ACK = 7,
	IPIMSG_VOW_DATAREADY = 8,
	IPIMSG_VOW_RECOGNIZE_OK = 9,
};

enum vow_eint_status_t {
	VOW_EINT_DISABLE = -2,
	VOW_EINT_FAIL = -1,
	VOW_EINT_PASS = 0,
	VOW_EINT_RETRY = 1,
	NUM_OF_VOW_EINT_STATUS
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
	VOW_MODEL_FIR = 3      /* no use */
};

enum vow_mtkif_type_t {
	VOW_MTKIF_NONE = 0,
	VOW_MTKIF_AMIC = 1,
	VOW_MTKIF_DMIC = 2,
	VOW_MTKIF_DMIC_LP = 3,
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
	int id;
	char *data;      /* reserved for future extension */
};


#ifdef CONFIG_COMPAT

struct vow_speaker_model_t {
	void *model_ptr;
	int  id;
	int  enabled;
};

struct vow_model_info_t {
	long  id;
	long  addr;
	long  size;
	long  return_size_addr;
	void *data;
};

struct vow_speaker_model_kernel_t {
	compat_uptr_t *model_ptr;
	compat_size_t  id;
	compat_size_t  enabled;
};

struct vow_model_info_kernel_t {
	compat_size_t  id;
	compat_size_t  addr;
	compat_size_t  size;
	compat_size_t  return_size_addr;
	compat_uptr_t *data;
};

#else

struct vow_speaker_model_t {
	void *model_ptr;
	int  id;
	int  enabled;
};

struct vow_model_info_t {
	long  id;
	long  addr;
	long  size;
	long  return_size_addr;
	void *data;
};
#endif


#endif /*__VOW_H__ */
