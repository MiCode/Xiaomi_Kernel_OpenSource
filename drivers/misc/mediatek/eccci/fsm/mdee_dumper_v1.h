/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2015 MediaTek Inc.
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
struct _exception_record_header_t {
	u8 ex_type;
	u8 ex_nvram;
	u16 ex_serial_num;
} __packed;

/* MODEM MAUI Environment information (164 bytes) */
struct _ex_environment_info_t {
	u8 boot_mode;		/* offset: +0x10 */
	u8 reserved1[8];
	u8 execution_unit[8];
	u8 status;		/* offset: +0x21, length: 1 */
	u8 ELM_status;		/* offset: +0x22, length: 1 */
	u8 reserved2[145];
} __packed;

/* MODEM MAUI Special for fatal error (8 bytes)*/
struct ex_fatalerror_code {
	u32 code1;
	u32 code2;
} __packed;

/* MODEM MAUI fatal error (296 bytes)*/
struct ex_fatalerror {
	struct ex_fatalerror_code error_code;
	u8 reserved1[288];
} __packed;

/* MODEM MAUI Assert fail (296 bytes)*/
struct ex_assert_fail {
	u8 filename[24];
	u32 linenumber;
	u32 parameters[3];
	u8 reserved1[256];
} __packed;
/* enlarge file name zone only for C2K */
struct ex_c2k_assert_fail {
	u8 filename[64];
	u32 linenumber;
	u32 parameters[3];
	u8 reserved1[216];
} __packed EX_C2K_ASSERTFAIL_T;

/* MODEM MAUI Globally exported data structure (300 bytes) */
union EX_CONTENT_T {
	struct ex_fatalerror fatalerr;
	struct ex_assert_fail assert;
	struct ex_c2k_assert_fail c2k_assert;
} __packed;

/* MODEM MAUI Standard structure of an exception log ( */
struct ex_log_t {
	struct _exception_record_header_t header;
	u8 reserved1[12];
	struct _ex_environment_info_t envinfo;
	u8 reserved2[36];
	union EX_CONTENT_T content;
} __packed;

struct ccci_msg_t {
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
} __packed;

struct debug_info_t {
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
		struct ccci_msg_t data;
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
};
struct mdee_dumper_v1 {
	struct debug_info_t debug_info;
	struct ex_log_t ex_info;
	unsigned int more_info;
};
#endif	/* __MDEE_DUMPER_V1_H__ */
