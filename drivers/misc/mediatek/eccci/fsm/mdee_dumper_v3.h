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

#ifndef __MDEE_DUMPER_V3_H__
#define __MDEE_DUMPER_V3_H__
#include "ccci_fsm_internal.h"

#define MD_HS1_FAIL_DUMP_SIZE  (2048)/*(512)*/

#define EE_BUF_LEN_UMOLY		(0x700)
#define AED_STR_LEN		(2048)/* 0x800 */
#define EE_BUF_LEN		(256)/* 0x100 */

#if (MD_GENERATION >= 6293)
#define MD_CORE_TOTAL_NUM   (8)
#else
#define MD_CORE_TOTAL_NUM   (9)
#endif
#define MD_CORE_NAME_LEN    (11)
/* +1 for end '\0', +5 for 16, +16 for str TDD FDD */
#define MD_CORE_NAME_DEBUG  (MD_CORE_NAME_LEN + 1 + 5 + 16)
#define EX_BRIEF_FATALERR_SIZE	(276)

typedef struct EX_STEP_V3 {
	u32 step;
	u32 timestap;
} __packed EX_STEP_T;

typedef struct EX_ASSERT_V3 {
	char	filepath[256];
	u32	line_number;
	u32	para1;
	u32	para2;
	u32	para3;
	u32	lr;
} __packed EX_ASSERT_V3_T;

typedef struct EX_FATAL_V3 {
	u32	code1;
	u32	code2;
	u32	code3;
	char	offender[8]; /* #define EX_UNIT_NAME_LEN 8 */
	u8	is_cadefa_supported;
	u8	is_filename_supported;
	u8	error_section;
	u8	pad[5];
	u32	error_status;
	u32	error_sp;
	u32	error_pc;
	u32	error_lr;
	u32	error_address;
	u32	error_cause;
	char	filename[0];
} __packed EX_FATAL_V3_T;

typedef union {
	EX_FATAL_V3_T fatalerr;
	EX_ASSERT_V3_T assert;
} __packed EX_MAIN_CONTENT_T;

typedef enum {
	MD_EX_CLASS_ASSET,
	MD_EX_CLASS_FATAL,
	MD_EX_CLASS_CUSTOM,
	MD_EX_CLASS_INVALID,
} EXCEPTION_CLASS;

typedef struct ex_brief_maininfo_t_v3 {
	u16 ex_type;
	u8 e_type_format;
	u8 maincontent_type;
	u8 elm_status;
	u8 system_info1;/* vpe */
	u8 system_info2;/* tc */
	u8 pad;
	EX_MAIN_CONTENT_T info;
} __packed EX_BRIEF_MAININFO_T;

typedef struct ex_main_reason_v3_t {
	char core_name[11];/* MD_CORE_NAME_LEN */
	u8 is_offender;
} __packed EX_MAIN_REASON_V3_T;

typedef struct ex_overview_t {
	u32 overview_verno;
	u32 core_num;
	EX_MAIN_REASON_V3_T main_reson[MD_CORE_TOTAL_NUM];
	EX_BRIEF_MAININFO_T ex_info;
	u32 mips_vpe_num;/* value == 7 */
	EX_STEP_T ex_step_logging[6];/* 6 == mips_vpe_num - 1 */
	u32 ect_status;
	u32 cs_offending_core;
	u32 md32_offending_status;
	u32 pad;
	u32 core_offset[MD_CORE_TOTAL_NUM];
} __packed EX_OVERVIEW_T;

typedef enum {
	/* mips exception codes in cause[exccode] */
	INTERRUPT_EXCEPTION = 0x0,
	TLB_MOD_EXCEPTION = 0x1,
	TLB_MISS_LOAD_EXCEPTION = 0x2,
	TLB_MISS_STORE_EXCEPTION = 0x3,
	ADDRESS_ERROR_LOAD_EXCEPTION = 0x4,
	ADDRESS_ERROR_STORE_EXCEPTION = 0x5,
	INSTR_BUS_ERROR = 0x6,
	DATA_BUS_ERROR = 0x7,
	SYSTEM_CALL_EXCEPTION = 0x8,
	BREAKPOINT_EXCEPTION = 0x9,
	RESERVED_INSTRUCTION_EXCEPTION = 0xA,
	COPROCESSORS_UNUSABLE_EXCEPTION = 0xB,
	INTEGER_OVERFLOW_EXCEPTION = 0xC,
	TRAP_EXCEPTION = 0xD,
	MSA_FLOATING_POINT_EXCEPTION = 0xE,
	FLOATING_POINT_EXCEPTION = 0xF,
	COPROCESSOR_2_IS_1_EXCEPTION = 0x10,
	COR_EXTEND_UNUSABLE_EXCEPTION = 0x11,
	COPROCESSOR_2_EXCEPTION = 0x12,
	TLB_READ_INHIBIT_EXCEPTION = 0x13,
	TLB_EXECUTE_INHIBIT_EXCEPTION = 0x14,
	MSA_UNUSABLE_EXCEPTION = 0x15,
	MDMX_EXCEPTION = 0x16,
	WATCH_EXCEPTION = 0x17,
	MCHECK_EXCEPTION = 0x18,
	THREAD_EXCEPTION = 0x19,
	DSP_UNUSABLE_EXCEPTION = 0x1A,
	RESERVED_27_EXCEPTION = 0x1B,
	RESERVED_28_EXCEPTION = 0x1C,
	MPU_NOT_ALLOW = 0x1D,
	CACHE_ERROR_EXCEPTION_DBG_MODE = 0x1E,
	RESERVED_31_EXCEPTION = 0x1F,

	/* exception types for nmi and cache error exception vectors */
	NMI_EXCEPTION = 0x20,
	CACHE_ERROR_EXCEPTION = 0x21,

	/* These are used to replace TLB_MISS_LOAD/STORE_EXCEPTION
	 * codes when using tlb refill exception vector.
	 * TLB_MISS_LOAD/STORE_EXCEPTION code is used for tlb invalid
	 */
	TLB_REFILL_LOAD_EXCEPTION = 0x22,
	TLB_REFILL_STORE_EXCEPTION = 0x23,
	TLB_REFILL_MAX_NUM,

	END_CPU_EXCEPTION_TYPE = 0x2F,

	STACKACCESS_EXCEPTION = 0x30,
	SYS_FATALERR_EXT_TASK_EXCEPTION = 0x31,
	SYS_FATALERR_EXT_BUF_EXCEPTION = 0x32,
	SYS_FATALERR_MAX_NUM,
	/* Assertion */
	ASSERT_FAIL_EXCEPTION = 0x50,
	ASSERT_DUMP_EXTENDED_RECORD = 0x51,
	ASSERT_FAIL_NATIVE = 0x52,
	ASSERT_CUSTOM_ADDR = 0x53,
	ASSERT_CUSTOM_MODID = 0x54,
	ASSERT_FAIL_MAX_NUM,
	/* cross core triggered */
	CC_INVALID_EXCEPTION = 0x60,
	CC_CS_EXCEPTION = 0x61,
	CC_MD32_EXCEPTION = 0x62,
	CC_C2K_EXCEPTION = 0x63,
	CC_VOLTE_EXCEPTION = 0x64,
	CC_USIP_EXCEPTION = 0x65,
	CC_SCQ_EXCEPTION = 0x66,
	CC_EXCEPTION_MAX_NUM,
	/* HW triggered */
	EMI_MPU_VIOLATION_EXCEPTION = 0x70,

	MAX_EXCEPTION_NUM,
	END_EXCEPTION_TYPE = 0xFFFF,
} exception_type;

typedef struct dump_info_assert {
	char file_name[256]; /* use pCore: file path, contain file name */
	int line_num;
	unsigned int parameters[3];
} DUMP_INFO_ASSERT;

typedef struct dump_info_fatal {
	int err_code1;
	int err_code2;
	int err_code3;
	unsigned int error_address;
	unsigned int error_pc;
	char *ExStr;
	char *err_sec;
	char offender[64];
	/*must be larger than EX_FATAL_V3_T filename:
	 * pre-fix + fatal_fname
	 */
	char fatal_fname[EX_BRIEF_FATALERR_SIZE];
} DUMP_INFO_FATAL;

enum {
	MD_EE_DATA_IN_SMEM,
	MD_EE_DATA_IN_GPD,
};

typedef struct dump_debug_info {
	unsigned int type;
	unsigned int ex_type;
	u8 par_data_source;
	char core_name[MD_CORE_NAME_DEBUG];
	char *name;/* exception name */
	char *ELM_status;
	union {
		DUMP_INFO_ASSERT dump_assert;
		DUMP_INFO_FATAL dump_fatal;
	};
	void *ext_mem;
	size_t ext_size;
} DEBUG_INFO_T;

struct mdee_dumper_v3 {
	unsigned int more_info;
	DEBUG_INFO_T debug_info;
	unsigned char ex_core_num;
	unsigned char ex_type;
	/* request by modem, change to 2k: include EX_PL_LOG_T*/
	unsigned char ex_pl_info[MD_HS1_FAIL_DUMP_SIZE];
};
#endif	/* __MDEE_DUMPER_V3_H__ */

