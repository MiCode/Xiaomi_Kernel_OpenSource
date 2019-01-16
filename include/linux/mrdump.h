#if !defined(__MRDUMP_H__)
#define __MRDUMP_H__

#include <stdarg.h>
#include <linux/elf.h>
#include <linux/elfcore.h>
#include <asm/ptrace.h>
#include <linux/aee.h>

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

#define MRDUMP_CPU_MAX 16

#define MRDUMP_DEV_NULL 0
#define MRDUMP_DEV_SDCARD 1
#define MRDUMP_DEV_EMMC 2

#define MRDUMP_FS_NULL 0
#define MRDUMP_FS_VFAT 1
#define MRDUMP_FS_EXT4 2

typedef uint32_t arm32_gregset_t[18];
typedef uint64_t aarch64_gregset_t[34];

struct mrdump_crash_record {
	int reboot_mode;

	char msg[128];
	char backtrace[512];

	uint32_t fault_cpu;
 
	union {
		arm32_gregset_t arm32_cpu_regs[MRDUMP_CPU_MAX];
		aarch64_gregset_t aarch64_cpu_regs[MRDUMP_CPU_MAX];
	};
};

struct mrdump_machdesc {
	uint32_t crc;

	uint32_t output_device;

	uint32_t nr_cpus;

	uint64_t page_offset;
	uint64_t high_memory;

	uint64_t vmalloc_start;
	uint64_t vmalloc_end;

	uint64_t modules_start;
	uint64_t modules_end;

	uint64_t phys_offset;
	uint64_t master_page_table;

	uint32_t output_fstype;
	uint32_t output_lbaooo;
};

struct mrdump_cblock_result {
	char status[128];

	uint32_t log_size;
	char log_buf[2048];
};

struct mrdump_control_block {
	char sig[8];

	struct mrdump_machdesc machdesc;
	struct mrdump_crash_record crash_record;
	struct mrdump_cblock_result result;
};

struct mrdump_platform {
	void (*hw_enable) (bool enabled);
	void (*reboot) (void);
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

#define MRDUMP_MINI_NR_SECTION 40
#define MRDUMP_MINI_SECTION_SIZE (32 * 1024)
#define NT_IPANIC_MISC 4095
#define MRDUMP_MINI_NR_MISC 20

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
	struct mrdump_mini_elf_prstatus prstatus[NR_CPUS + 1];
	struct mrdump_mini_elf_note misc[MRDUMP_MINI_NR_MISC];
};

#define MRDUMP_MINI_HEADER_SIZE ALIGN(sizeof(struct mrdump_mini_elf_header), PAGE_SIZE)
#define MRDUMP_MINI_DATA_SIZE (MRDUMP_MINI_NR_SECTION * MRDUMP_MINI_SECTION_SIZE)
#define MRDUMP_MINI_BUF_SIZE (MRDUMP_MINI_HEADER_SIZE + MRDUMP_MINI_DATA_SIZE)

#ifdef CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR
#define MRDUMP_MINI_BUF_PADDR (CONFIG_MTK_RAM_CONSOLE_DRAM_ADDR + 0xf0000)
#else
#define MRDUMP_MINI_BUF_PADDR 0
#endif

#if defined(CONFIG_MTK_AEE_MRDUMP)
void mrdump_reserve_memory(void);

void mrdump_platform_init(struct mrdump_control_block *cblock,
			  const struct mrdump_platform *plafrom);

void __mrdump_create_oops_dump(AEE_REBOOT_MODE reboot_mode, struct pt_regs *regs, const char *msg,
			       ...);

void aee_kdump_reboot(AEE_REBOOT_MODE, const char *msg, ...);

#else
static inline void mrdump_reserve_memory(void)
{
}

static inline void mrdump_platform_init(struct mrdump_control_block *cblock,
					void (*hw_enable) (bool enabled))
{
}

static inline void __mrdump_create_oops_dump(AEE_REBOOT_MODE reboot_mode, struct pt_regs *regs,
					     const char *msg, ...)
{
}

static inline void aee_kdump_reboot(AEE_REBOOT_MODE reboot_mode, const char *msg, ...)
{
}

#endif

typedef int (*mrdump_write)(void *buf, int off, int len, int encrypt);
#if defined(CONFIG_MTK_AEE_IPANIC)
int mrdump_mini_create_oops_dump(AEE_REBOOT_MODE reboot_mode, mrdump_write write,
				 loff_t sd_offset, const char *msg, va_list ap);
void mrdump_mini_reserve_memory(void);
#else
static inline int mrdump_mini_create_oops_dump(AEE_REBOOT_MODE reboot_mode, mrdump_write write,
					       loff_t sd_offset, const char *msg, va_list ap)
{
	return 0;
}

static inline void mrdump_mini_reserve_memory(void)
{
}
#endif

#endif
