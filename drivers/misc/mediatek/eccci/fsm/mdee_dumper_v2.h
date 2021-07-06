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

#ifndef __MDEE_DUMPER_V2_H__
#define __MDEE_DUMPER_V2_H__
#include "ccci_fsm_internal.h"

#define MD_HS1_FAIL_DUMP_SIZE  (2048)

#define EE_BUF_LEN_UMOLY		(0x700)
#define AED_STR_LEN		(2048)
#define EE_BUF_LEN		(256)

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

enum {
	MD_EX_DUMP_INVALID = 0,
	MD_EX_DUMP_ASSERT = 1,
	MD_EX_DUMP_3P_EX = 2,
	MD_EX_DUMP_2P_EX = 3,

	MD_EX_DUMP_EMI_CHECK = MD_EX_TYPE_EMI_CHECK,

	/*MD_EX_C2K_FATAL_ERROR = 0x3000,*/
	MD_EX_DUMP_UNKNOWN,
};

/* MD32 exception struct */
enum {
	CMIF_MD32_EX_INVALID = 0,
	CMIF_MD32_EX_ASSERT_LINE,
	CMIF_MD32_EX_ASSERT_EXT,
	CMIF_MD32_EX_FATAL_ERROR,
	CMIF_MD32_EX_FATAL_ERROR_EXT,
}; /* CMIF_MD32_EX_TYPE */

struct ex_fatalerr_md32 {
	unsigned int ex_code[2];
	unsigned int ifabtpc;
	unsigned int ifabtcau;
	unsigned int daabtcau;
	unsigned int daabtpc;
	unsigned int daabtad;
	unsigned int daabtsp;
	unsigned int lr;
	unsigned int sp;
	unsigned int interrupt_count;
	unsigned int vic_mask;
	unsigned int vic_pending;
	unsigned int cirq_mask_31_0;
	unsigned int cirq_mask_63_32;
	unsigned int cirq_pend_31_0;
	unsigned int cirq_pend_63_32;
} __packed;

struct ex_assertfail_md32 {
	u32 ex_code[3];
	u32 line_num;
	char file_name[64];
} __packed;

union ex_md32_content {
	struct ex_fatalerr_md32 fatalerr;
	struct ex_assertfail_md32 assert;
} __packed;

#define MD32_FDD_ROCODE   "FDD_ROCODE"
#define MD32_TDD_ROCODE   "TDD_ROCODE"
struct ex_md32_log {
	u32 finish_fill;
	u32 except_type;
	union ex_md32_content except_content;
	unsigned int ex_log_mem_addr;
	unsigned int md32_active_mode;
} __packed;

/* CoreSonic exception struct */
enum {
	CS_EXCEPTION_ASSERTION = 0x45584300,
	CS_EXCEPTION_FATAL_ERROR = 0x45584301,
	CS_EXCEPTION_CTI_EVENT = 0x45584302,
	CS_EXCEPTION_UNKNOWN = 0x45584303,
};/* CS_EXCEPTION_TYPE_T */

struct ex_fatalerr_cs {
	u32 error_status;
	u32 error_pc;
	u32 error_lr;
	u32 error_address;
	u32 error_code1;
	u32 error_code2;
} __packed;

struct ex_assertfail_cs {
	u32 line_num;
	u32 para1;
	u32 para2;
	u32 para3;
	char file_name[128];
} __packed;

union ex_cs_content {
	struct ex_fatalerr_cs fatalerr;
	struct ex_assertfail_cs assert;
} __packed;

struct ex_cs_log {
	u32 except_type;
	union ex_cs_content except_content;
} __packed;

/* PCORE, L1CORE exception struct */
enum {
	MD_EX_PL_INVALID = 0,
	MD_EX_PL_UNDEF = 1,
	MD_EX_PL_SWI = 2,
	MD_EX_PL_PREF_ABT = 3,
	MD_EX_PL_DATA_ABT = 4,
	MD_EX_PL_STACKACCESS = 5,

	MD_EX_PL_FATALERR_TASK = 6,
	MD_EX_PL_FATALERR_BUF = 7,
	MD_EX_PL_FATALE_TOTAL,
	MD_EX_PL_ASSERT_FAIL = 16,
	MD_EX_PL_ASSERT_DUMP = 17,
	MD_EX_PL_ASSERT_NATIVE = 18,
	MD_EX_CC_INVALID_EXCEPTION = 0x20,
	MD_EX_CC_PCORE_EXCEPTION = 0x21,
	MD_EX_CC_L1CORE_EXCEPTION = 0x22,
	MD_EX_CC_CS_EXCEPTION = 0x23,
	MD_EX_CC_MD32_EXCEPTION = 0x24,
	MD_EX_CC_C2K_EXCEPTION = 0x25,
	MD_EX_CC_ARM7_EXCEPTION = 0x26,
	MD_EX_OTHER_CORE_EXCEPTIN,

	EMI_MPU_VIOLATION = 0x30,
	/* NUM_EXCEPTION, */

};

/* MD core list */
enum {
	MD_PCORE,
	MD_L1CORE,
	MD_CS_ICC,
	MD_CS_IMC,
	MD_CS_MPC,
	MD_MD32_DFE,
	MD_MD32_BRP,
	MD_MD32_RAKE,
	MD_CORE_NUM
};/* MD_CORE_NAME */
struct exp_pl_header {
	u32 ex_core_id;
	u8 ex_type;
	u8 ex_nvram;
	u16 ex_serial_num;
} __packed;
struct ex_time_stamp {
	u32 USCNT;
	u32 GLB_TS;
};
struct ex_pl_environment_info {
	struct ex_time_stamp ex_timestamp;
	u8 boot_mode;		/* offset: +0x10 */
	u8 execution_unit[8];
	u8 status;		/* offset: +0x21, length: 1 */
	u8 ELM_status;		/* offset: +0x22, length: 1 */
	u8 reserved2;
	unsigned int stack_ptr;
	u8 stack_dump[40];
	u16 ext_queue_pending_cnt;
	u16 interrupt_mask3;
	u8 ext_queue_pending[80];
	u8 interrupt_mask[8];
	u32 processing_lisr;
	u32 lr;
} __packed;
struct ex_pl_fatalerror_code {
	u32 code1;
	u32 code2;
	u32 code3;
} __packed;

struct ex_pl_analysis {
	u32 trace;
	u8 param[40];
	u8 owner[8];
	unsigned char core[7];
	u8 is_cadefa_sup;
} __packed;

struct ex_pl_fatalerror {
	struct ex_pl_fatalerror_code error_code;
	u8 description[20];
	struct ex_pl_analysis ex_analy;
	u8 reserved1[356];
} __packed;

struct ex_pl_assert {
	u8 filepath[256];
	u8 filename[64];
	u32 linenumber;
	u32 para[3];
	u8 reserved1[368];
	u8 guard[4];
} __packed;

struct ex_pl_diagnosisinfo {
	u8 diagnosis;
	char owner[8];
	u8 reserve[3];
	u8 timing_check[24];
} __packed;

union ex_pl_content {
	struct ex_pl_fatalerror fatalerr;
	struct ex_pl_assert assert;
} __packed;

struct ex_PL_log {
	struct exp_pl_header header; /* 8 bytes */
	char sw_ver[32]; /* 4 bytes: 8 */
	char sw_project_name[32];  /* 8: 12 */
	char sw_flavor[32];/* 8:20 */
	char sw_buildtime[16];/* 4: 28 */
	struct ex_pl_environment_info envinfo;/* : 32 */
	struct ex_pl_diagnosisinfo diagnoinfo; /* 36:  */
	union ex_pl_content content;
} __packed;

/* exception overview struct */
#define MD_CORE_TOTAL_NUM   (8)
#define MD_CORE_NAME_LEN    (11)
/* +5 for 16, +16 for str TDD FDD */
#define MD_CORE_NAME_DEBUG  (MD_CORE_NAME_LEN + 5 + 16)
#define ECT_SRC_NONE    (0x0)
#define ECT_SRC_PS      (0x1 << 0)
#define ECT_SRC_L1      (0x1 << 1)
#define ECT_SRC_MD32    (0x1 << 2)
#define ECT_SRC_CS      (0x1 << 3)
#define ECT_SRC_ARM7    (0x1 << 10)
#define ECT_SRC_RMPU    (0x1 << 11)
#define ECT_SRC_C2K     (0x1 << 12)

struct ex_main_reason {
	u32 core_offset;
	u8 is_offender;
	char core_name[MD_CORE_NAME_LEN];
} __packed;

struct ex_overview_t {
	u32 core_num;
	struct ex_main_reason main_reson[MD_CORE_TOTAL_NUM];
	u32 ect_status;
	u32 cs_status;
	u32 md32_status;
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
	char core_name[MD_CORE_NAME_DEBUG];
	union {
		struct {
			/* use pCore: file path, contain file name */
			char file_name[256];
			int line_num;
			unsigned int parameters[3];
		} assert;
		struct {
			int err_code1;
			int err_code2;
			int err_code3;
			char *ExStr;
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
};

struct mdee_dumper_v2 {
	unsigned int more_info;
	struct debug_info_t debug_info[MD_CORE_NUM];
	unsigned char ex_core_num;
	/* request by modem, change to 2k: include struct ex_PL_log*/
	unsigned char ex_pl_info[MD_HS1_FAIL_DUMP_SIZE];
};

extern int Is_MD_EMI_voilation(void);

#endif	/* __MDEE_DUMPER_V2_H__ */
