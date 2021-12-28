/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#if !defined(__MRDUMP_H__)
#define __MRDUMP_H__

#include <asm/ptrace.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <stdarg.h>
#include <mt-plat/aee.h>

#ifdef __aarch64__
#define reg_pc	pc
#define reg_lr	regs[30]
#define reg_sp	sp
#define reg_fp	regs[29]
#else
#define reg_pc	ARM_pc
#define reg_lr	ARM_lr
#define reg_sp	ARM_sp
#define reg_ip	ARM_ip
#define reg_fp	ARM_fp
#endif

#define MRDUMP_CPU_MAX 12

#define MRDUMP_GO_DUMP "MRDUMP11"

#define KSYM_32        1
#define KSYM_64        2

typedef uint32_t arm32_gregset_t[18];
typedef uint64_t aarch64_gregset_t[34];

struct arm32_ctrl_regs {
	uint32_t sctlr;
	uint64_t ttbcr;
	uint64_t ttbr0;
	uint64_t ttbr1;
};

struct aarch64_ctrl_regs {
	uint64_t sctlr_el1;
	uint64_t sctlr_el2;
	uint64_t sctlr_el3;

	uint64_t tcr_el1;
	uint64_t tcr_el2;
	uint64_t tcr_el3;

	uint64_t ttbr0_el1;
	uint64_t ttbr0_el2;
	uint64_t ttbr0_el3;

	uint64_t ttbr1_el1;

	uint64_t sp_el[4];
};

struct mrdump_arm32_reg {
	arm32_gregset_t arm32_regs;
	struct arm32_ctrl_regs arm32_creg;
};

struct mrdump_arm64_reg {
	aarch64_gregset_t arm64_regs;
	struct aarch64_ctrl_regs arm64_creg;
};

struct mrdump_crash_record {
	int reboot_mode;

	char msg[128];

	uint32_t fault_cpu;

	union {
		struct mrdump_arm32_reg arm32_reg;
		struct mrdump_arm64_reg arm64_reg;
	} cpu_reg[0];
};

/* mrdump_ksyms_param->flag */
#define MKP_BIT_SHIFT_ARCH64		0
#define MKP_BIT_SHIFT_ABS_PERCPU	1
#define MKP_BIT_SHIFT_RELATIVE		31

struct mrdump_ksyms_param {
	char     tag[4];
	uint32_t flag;
	uint32_t crc;
	uint64_t start_addr;
	uint32_t size;
	uint32_t addresses_off;
	uint32_t num_syms_off;
	uint32_t names_off;
	uint32_t markers_off;
	uint32_t token_table_off;
	uint32_t token_index_off;
} __packed;

struct mrdump_machdesc {
	uint32_t nr_cpus;

	uint64_t page_offset;
	uint64_t tcr_el1_t1sz;

	uint64_t kimage_vaddr;
	uint64_t dram_start; /* deprecated */
	uint64_t dram_end; /* deprecated */
	uint64_t kimage_stext;
	uint64_t kimage_etext;
	uint64_t kimage_stext_real;
	uint64_t kimage_voffset;
	uint64_t kernel_pac_mask;
	uint64_t unused1;

	uint64_t vmalloc_start;
	uint64_t vmalloc_end;

	uint64_t modules_start;
	uint64_t modules_end;

	uint64_t phys_offset;
	uint64_t master_page_table;

	uint64_t memmap;
	uint64_t pageflags;
	uint32_t struct_page_size;

	uint64_t dfdmem_pa;

	struct mrdump_ksyms_param kallsyms;
};

struct mrdump_control_block {
	char sig[8];

	struct mrdump_machdesc machdesc;
	uint32_t machdesc_crc;

	uint32_t unused0;
	uint32_t output_fs_lbaooo;

	struct mrdump_crash_record crash_record;
};

/* NOTE!! any change to this struct should be compatible in aed */
struct mrdump_mini_reg_desc {
	unsigned long reg;	/* register value */
	unsigned long kstart;	/* start kernel addr of memory dump */
	unsigned long kend;	/* end kernel addr of memory dump */
	unsigned long pos;	/* next pos to dump */
	int valid;		/* 1: valid regiser, 0: invalid regiser */
	int pad;		/* reserved */
	loff_t offset;		/* offset in buffer */
};

/* it should always be smaller than MRDUMP_MINI_HEADER_SIZE */
struct mrdump_mini_header {
	struct mrdump_mini_reg_desc reg_desc[ELF_NGREG];
};

#define MRDUMP_MINI_NR_SECTION 60
#define MRDUMP_MINI_SECTION_SIZE (32 * 1024)
#define NT_IPANIC_MISC 4095
#define MRDUMP_MINI_NR_MISC 40
#define MRDUMP_MINI_MISC_LOAD "load"

struct mrdump_mini_elf_misc {
	unsigned long vaddr;
	unsigned long paddr;
	unsigned long start;
	unsigned long size;
};

#define NOTE_NAME_SHORT 12
#define NOTE_NAME_LONG  20

struct mrdump_mini_elf_psinfo {
	struct elf_note note;
	char name[NOTE_NAME_SHORT];
	struct elf_prpsinfo data;
};

struct mrdump_mini_elf_prstatus {
	struct elf_note note;
	char name[NOTE_NAME_SHORT];
	struct elf_prstatus data;
};

struct mrdump_mini_elf_note {
	struct elf_note note;
	char name[NOTE_NAME_LONG];
	struct mrdump_mini_elf_misc data;
};

struct mrdump_mini_elf_header {
	struct elfhdr ehdr;
	struct elf_phdr phdrs[MRDUMP_MINI_NR_SECTION];
	struct mrdump_mini_elf_psinfo psinfo;
	struct mrdump_mini_elf_prstatus prstatus[AEE_MTK_CPU_NUMS];
	struct mrdump_mini_elf_note misc[MRDUMP_MINI_NR_MISC];
};


#define MRDUMP_MINI_HEADER_SIZE	\
	ALIGN(sizeof(struct mrdump_mini_elf_header), PAGE_SIZE)
#define MRDUMP_MINI_DATA_SIZE	\
	(MRDUMP_MINI_NR_SECTION * MRDUMP_MINI_SECTION_SIZE)
#define MRDUMP_MINI_BUF_SIZE (MRDUMP_MINI_HEADER_SIZE + MRDUMP_MINI_DATA_SIZE)

enum AEE_EXTRA_FILE_ID {
	AEE_EXTRA_FILE_UFS,
	AEE_EXTRA_FILE_MMC,
	AEE_EXTRA_FILE_BLOCKIO,
	AEE_EXTRA_FILE_ADSP,
	AEE_EXTRA_FILE_CCU,
	AEE_EXTRA_FILE_NUM
};

int mrdump_init(void);
void mrdump_save_ctrlreg(int cpu);
void mrdump_save_per_cpu_reg(int cpu, struct pt_regs *regs);

int mrdump_common_die(int reboot_reason, const char *msg, struct pt_regs *regs);
void mrdump_mini_add_hang_raw(unsigned long vaddr, unsigned long size);
void mrdump_mini_add_extra_misc(void);
#if IS_ENABLED(CONFIG_MTK_AEE_IPANIC)
int mrdump_mini_add_extra_file(unsigned long vaddr, unsigned long paddr,
	unsigned long size, const char *name);
extern void mrdump_set_extra_dump(enum AEE_EXTRA_FILE_ID id,
		void (*fn)(unsigned long *vaddr, unsigned long *size));
#else
static inline int mrdump_mini_add_extra_file(unsigned long vaddr,
	unsigned long paddr, unsigned long size, const char *name)
{
	return -1;
}

static inline void mrdump_set_extra_dump(enum AEE_EXTRA_FILE_ID id,
		void (*fn)(unsigned long *vaddr, unsigned long *size))
{
}
#endif
extern void mlog_get_buffer(char **ptr, int *size)__attribute__((weak));
extern void get_msdc_aee_buffer(unsigned long *buff,
	unsigned long *size)__attribute__((weak));
#if IS_ENABLED(CONFIG_MTK_AEE_HANGDET)
extern void kwdt_regist_irq_info(void (*fn)(void));
#endif
#endif

