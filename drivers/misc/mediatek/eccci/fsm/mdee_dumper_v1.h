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

#ifndef __MDEE_DUMPER_V1_H__
#define __MDEE_DUMPER_V1_H__
#include "ccci_fsm_internal.h"

#define EE_BUF_LEN		(256)
#define AED_STR_LEN		(512)

#define CCCI_EXREC_OFFSET_OFFENDER 288

enum {
	MD_EX_TYPE_INVALID = 0,
	MD_EX_TYPE_UNDEF = 1,
	MD_EX_TYPE_SWI = 2,
	MD_EX_TYPE_PREF_ABT = 3,
	MD_EX_TYPE_DATA_ABT = 4,
	MD_EX_TYPE_ASSERT = 5,
	MD_EX_TYPE_FATALERR_TASK = 6,
	MD_EX_TYPE_FATALERR_BUF = 7,
	MD_EX_TYPE_LOCKUP = 8,
	MD_EX_TYPE_ASSERT_DUMP = 9,
	MD_EX_TYPE_ASSERT_FAIL = 10,
	DSP_EX_TYPE_ASSERT = 11,
	DSP_EX_TYPE_EXCEPTION = 12,
	DSP_EX_FATAL_ERROR = 13,

	/*cross core trigger exception, only md3 will trigger this exception*/
	CC_MD1_EXCEPTION = 15,

	NUM_EXCEPTION,
	MD_EX_TYPE_C2K_ERROR = 0x25,
	MD_EX_TYPE_EMI_CHECK = 99,
	MD_EX_C2K_FATAL_ERROR = 0x3000,
};

/* MODEM MAUI Exception header (4 bytes)*/
typedef struct _exception_record_header_t {
	u8 ex_type;
	u8 ex_nvram;
	u16 ex_serial_num;
} __packed EX_HEADER_T;

/* MODEM MAUI Environment information (164 bytes) */
typedef struct _ex_environment_info_t {
	u8 boot_mode;		/* offset: +0x10 */
	u8 reserved1[8];
	u8 execution_unit[8];
	u8 status;		/* offset: +0x21, length: 1 */
	u8 ELM_status;		/* offset: +0x22, length: 1 */
	u8 reserved2[145];
} __packed EX_ENVINFO_T;

/* MODEM MAUI Special for fatal error (8 bytes)*/
typedef struct _ex_fatalerror_code_t {
	u32 code1;
	u32 code2;
} __packed EX_FATALERR_CODE_T;

/* MODEM MAUI fatal error (296 bytes)*/
typedef struct _ex_fatalerror_t {
	EX_FATALERR_CODE_T error_code;
	u8 reserved1[288];
} __packed EX_FATALERR_T;

/* MODEM MAUI Assert fail (296 bytes)*/
typedef struct _ex_assert_fail_t {
	u8 filename[24];
	u32 linenumber;
	u32 parameters[3];
	u8 reserved1[256];
} __packed EX_ASSERTFAIL_T;
/* enlarge file name zone only for C2K */
typedef struct _ex_c2k_assert_fail_t {
	u8 filename[64];
	u32 linenumber;
	u32 parameters[3];
	u8 reserved1[216];
} __packed EX_C2K_ASSERTFAIL_T;

/* MODEM MAUI Globally exported data structure (300 bytes) */
typedef union {
	EX_FATALERR_T fatalerr;
	EX_ASSERTFAIL_T assert;
	EX_C2K_ASSERTFAIL_T c2k_assert;
} __packed EX_CONTENT_T;

/* MODEM MAUI Standard structure of an exception log ( */
typedef struct _ex_exception_log_t {
	EX_HEADER_T header;
	u8 reserved1[12];
	EX_ENVINFO_T envinfo;
	u8 reserved2[36];
	EX_CONTENT_T content;
} __packed EX_LOG_T;

typedef struct _ccci_msg {
	union {
		u32 magic;	/* For mail box magic number */
		u32 addr;	/* For stream start addr */
		u32 data0;	/* For ccci common data[0] */
	};
	union {
		u32 id;		/* For mail box message id */
		u32 len;	/* For stream len */
		u32 data1;	/* For ccci common data[1] */
	};
	u32 channel;
	u32 reserved;
} __packed ccci_msg_t;

typedef struct dump_debug_info {
	unsigned int type;
	char *name;
	union {
		struct {
			char file_name[30];
			int line_num;
			unsigned int parameters[3];
		} assert;
		struct {
			int err_code1;
			int err_code2;
			char offender[9];
		} fatal_error;
		ccci_msg_t data;
		struct {
			unsigned char execution_unit[9];	/* 8+1 */
			char file_name[30];
			int line_num;
			unsigned int parameters[3];
		} dsp_assert;
		struct {
			unsigned char execution_unit[9];
			unsigned int code1;
		} dsp_exception;
		struct {
			unsigned char execution_unit[9];
			unsigned int err_code[2];
		} dsp_fatal_err;
	};
	void *ext_mem;
	size_t ext_size;
	void *md_image;
	size_t md_size;
} DEBUG_INFO_T;
struct mdee_dumper_v1 {
	DEBUG_INFO_T debug_info;
	EX_LOG_T ex_info;
	unsigned int more_info;
};
#endif	/* __MDEE_DUMPER_V1_H__ */
