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

#ifndef __ADSP_EXCEP_H__
#define __ADSP_EXCEP_H__

#include <linux/sizes.h>

#define AED_LOG_PRINT_SIZE      SZ_16K

enum adsp_excep_id {
	EXCEP_LOAD_FIRMWARE = 0,
	EXCEP_RESET,
	EXCEP_BOOTUP,
	EXCEP_RUNTIME,
	EXCEP_KERNEL,
	ADSP_NR_EXCEP,
};

extern void adsp_aed(enum adsp_excep_id type, enum adsp_core_id id);
extern void adsp_aed_reset(enum adsp_excep_id type, enum adsp_core_id id);
extern void adsp_aed_reset_inplace(enum adsp_excep_id type,
				   enum adsp_core_id id);
extern uint32_t adsp_dump_pc(void);
extern int adsp_get_trax_initiated(void);
extern int adsp_get_trax_done(void);
extern int adsp_get_trax_length(void);

extern void aed_scp_exception_api(const int *log, int log_size, const int *phy,
				   int phy_size, const char *detail,
				   const int db_opt);
extern void adsp_excep_cleanup(void);
extern struct mutex adsp_sw_reset_mutex;
enum { r0, r1, r2, r3, r12, lr, pc, psr};

struct TaskContextType {
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;
	unsigned int sp;       /* after pop r0-r3, lr, pc, xpsr */
	unsigned int pc;       /* pc before exception */
	unsigned int control;  /* nPRIV bit&FPCA bit meaningful,SPSEL bit = 0 */
	unsigned int exc_return; /* current lr */
	unsigned int msp;        /* msp */
};

#define CRASH_SUMMARY_LENGTH 12
#define CRASH_MEMORY_HEADER_SIZE  (8 * 1024)
#define CRASH_MEMORY_LENGTH (68 * 1024)  /* 36k+32k */
#define CRASH_MEMORY_OFFSET  (0x800)
#define CRASH_CORE_REG_SIZE  (9 * 32)
#define CRASH_CFG_REG_SIZE   (64 * 1024)

#include <linux/elf.h>
#define ELF_NGREGS 18
#define CORE_STR "CORE"
#define ELF_PRARGSZ 80
#define ELF_CORE_EFLAGS 0
#define EM_ARM 40
static inline void elf_setup_eident(unsigned char e_ident[EI_NIDENT],
				    unsigned char elfclasz)
{
	memcpy(e_ident, ELFMAG, SELFMAG);
	e_ident[EI_CLASS] = elfclasz;
	e_ident[EI_DATA] = ELFDATA2LSB;
	e_ident[EI_VERSION] = EV_CURRENT;
	e_ident[EI_OSABI] = ELFOSABI_NONE;
	memset(e_ident + EI_PAD, 0, EI_NIDENT - EI_PAD);
}

struct elf_siginfo {
	int     si_signo;
	int     si_code;
	int     si_errno;
};

struct elf32_note_t {
	Elf32_Word   n_namesz;       /* Name size */
	Elf32_Word   n_descsz;       /* Content size */
	Elf32_Word   n_type;         /* Content type */
};

struct elf32_timeval {
	int32_t tv_sec;
	int32_t tv_usec;
};
struct elf32_prstatus {
	struct elf_siginfo pr_info;
	short pr_cursig;
	uint32_t pr_sigpend;
	uint32_t pr_sighold;

	int32_t pr_pid;
	int32_t pr_ppid;
	int32_t pr_pgrp;

	int32_t pr_sid;
	struct elf32_timeval pr_utime;
	struct elf32_timeval pr_stime;
	struct elf32_timeval pr_cutime;
	struct elf32_timeval pr_cstime;

	uint32_t pr_reg[ELF_NGREGS];

	int32_t pr_fpvalid;
};

struct elf32_prpsinfo {
	char pr_state;
	char pr_sname;
	char pr_zomb;
	char pr_nice;
	uint32_t pr_flag;

	uint16_t pr_uid;
	uint16_t pr_gid;

	int32_t pr_pid;
	int32_t pr_ppid;
	int32_t pr_pgrp;
	int32_t pr_sid;

	char pr_fname[16];
	char pr_psargs[ELF_PRARGSZ];
};

/* adsp reg dump */
struct adsp_coredump {
	u32 reserved_0[67];
	u32 pc;
	u32 exccause;
	u32 excvaddr;
	u32 reserved_1[7];
	u8 task_name[16];
	u32 reserved_2[47];
	u8 assert_log[512];
};

struct MemoryDump {
	struct elf32_hdr elf;
	struct elf32_phdr nhdr;
	struct elf32_phdr phdr;
	char notes[CRASH_MEMORY_HEADER_SIZE - sizeof(struct elf32_hdr)
		   - sizeof(struct elf32_phdr) - sizeof(struct elf32_phdr)];
	/* ram dump total header size (elf+nhdr+phdr
	 * must be fixed at CRASH_MEMORY_HEADER_SIZE
	 */
	char memory[CRASH_MEMORY_LENGTH];    /* adsp tcm */
	char core_reg[CRASH_CORE_REG_SIZE];  /* core reg */
	char cfg_reg[CRASH_CFG_REG_SIZE];    /* cfg reg */
};

#endif
