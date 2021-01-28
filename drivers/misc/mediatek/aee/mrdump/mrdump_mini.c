// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/elf.h>
#include <linux/fs.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/kdebug.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include <linux/printk.h>
#include <linux/sizes.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/stacktrace.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include "../../../../kernel/sched/sched.h"

#include <asm/irq.h>
#include <asm/kexec.h>
#include <asm/page.h>
#include <asm/pgtable.h>
#include <asm/setup.h>
#include <asm/stacktrace.h>
#include <asm-generic/percpu.h>
#include <asm-generic/sections.h>

#include <mrdump.h>
#include <mt-plat/aee.h>
#include "mrdump_mini.h"
#include "mrdump_private.h"

static struct mrdump_mini_elf_header *mrdump_mini_ehdr;
#ifdef CONFIG_MODULES
static char *modules_info_buf;
#endif

static bool dump_all_cpus;

#ifdef MODULE
static char __aee_cmdline[COMMAND_LINE_SIZE];
static char *aee_cmdline = __aee_cmdline;

const char *mrdump_get_cmd(void)
{
	struct file *fd;
	mm_segment_t fs;
	loff_t pos = 0;

	if (__aee_cmdline[0] != 0)
		return aee_cmdline;

	fs = get_fs();
	set_fs(KERNEL_DS);
	fd = filp_open("/proc/cmdline", O_RDONLY, 0);
	if (IS_ERR(fd)) {
		pr_info("kedump: Unable to open /proc/cmdline (%ld)",
			PTR_ERR(fd));
		set_fs(fs);
		return aee_cmdline;
	}
	vfs_read(fd, (void *)aee_cmdline, COMMAND_LINE_SIZE, &pos);
	filp_close(fd, NULL);
	fd = NULL;
	set_fs(fs);
	return aee_cmdline;
}
EXPORT_SYMBOL(mrdump_get_cmd);
#else
const char *mrdump_get_cmd(void)
{
	return saved_command_line;
}
EXPORT_SYMBOL(mrdump_get_cmd);

#endif

__weak void get_gz_log_buffer(unsigned long *addr, unsigned long *paddr,
			unsigned long *size, unsigned long *start)
{
	*addr = *paddr = *size = *start = 0;
}

__weak void get_disp_err_buffer(unsigned long *addr, unsigned long *size,
		unsigned long *start)
{
}


__weak void get_disp_fence_buffer(unsigned long *addr, unsigned long *size,
		unsigned long *start)
{
}

__weak void get_disp_dbg_buffer(unsigned long *addr, unsigned long *size,
		unsigned long *start)
{
}

__weak void get_disp_dump_buffer(unsigned long *addr, unsigned long *size,
		unsigned long *start)
{
}

__weak void get_pidmap_aee_buffer(unsigned long *addr, unsigned long *size)
{
}

#ifdef __aarch64__
#define MIN_MARGIN KIMAGE_VADDR
#else
#define MIN_MARGIN PAGE_OFFSET
#endif

#ifdef __aarch64__
static unsigned long virt_2_pfn(unsigned long addr)
{
	pgd_t *pgd = aee_pgd_offset_k(addr), _pgd_val = {0};
	pud_t *pud, _pud_val = {0};
	pmd_t *pmd, _pmd_val = {0};
	pte_t *ptep, _pte_val = {0};
	unsigned long pfn = ~0UL;

#ifdef CONFIG_ARM64
	if (addr < VA_START)
		goto OUT;
#endif
	if (probe_kernel_address(pgd, _pgd_val) || pgd_none(_pgd_val))
		goto OUT;
	pud = pud_offset(pgd, addr);
	if (probe_kernel_address(pud, _pud_val) || pud_none(_pud_val))
		goto OUT;
	if (pud_sect(_pud_val)) {
		pfn = pud_pfn(_pud_val) + ((addr&~PUD_MASK) >> PAGE_SHIFT);
	} else if (pud_table(_pud_val)) {
		pmd = pmd_offset(pud, addr);
		if (probe_kernel_address(pmd, _pmd_val) || pmd_none(_pmd_val))
			goto OUT;
		if (pmd_sect(_pmd_val)) {
			pfn = pmd_pfn(_pmd_val) +
				((addr&~PMD_MASK) >> PAGE_SHIFT);
		} else if (pmd_table(_pmd_val)) {
			ptep = pte_offset_map(pmd, addr);
			if (probe_kernel_address(ptep, _pte_val)
				|| !pte_present(_pte_val)) {
				pte_unmap(ptep);
				goto OUT;
			}
			pfn = pte_pfn(_pte_val);
			pte_unmap(ptep);
		}
	}
OUT:
	return pfn;

}
#else
#ifndef pmd_sect
#define pmd_sect(pmd)	(pmd & PMD_TYPE_SECT)
#endif
#ifndef pmd_table
#define pmd_table(pmd)	(pmd & PMD_TYPE_TABLE)
#endif
#ifndef pmd_pfn
#define pmd_pfn(pmd)	(((pmd_val(pmd) & PMD_MASK) & PHYS_MASK) >> PAGE_SHIFT)
#endif
static unsigned long virt_2_pfn(unsigned long addr)
{
	pgd_t *pgd = aee_pgd_offset_k(addr), _pgd_val = {0};
#ifdef CONFIG_ARM_LPAE
	pud_t *pud, _pud_val = {0};
#else
	pud_t *pud, _pud_val = {{0} };
#endif
	pmd_t *pmd, _pmd_val = 0;
	pte_t *ptep, _pte_val = 0;
	unsigned long pfn = ~0UL;

	if (probe_kernel_address(pgd, _pgd_val) || pgd_none(_pgd_val))
		goto OUT;
	pud = pud_offset(pgd, addr);
	if (probe_kernel_address(pud, _pud_val) || pud_none(_pud_val))
		goto OUT;
	pmd = pmd_offset(pud, addr);
	if (probe_kernel_address(pmd, _pmd_val) || pmd_none(_pmd_val))
		goto OUT;
	if (pmd_sect(_pmd_val)) {
		pfn = pmd_pfn(_pmd_val) + ((addr&~PMD_MASK) >> PAGE_SHIFT);
	} else if (pmd_table(_pmd_val)) {
		ptep = pte_offset_map(pmd, addr);
		if (probe_kernel_address(ptep, _pte_val)
			|| !pte_present(_pte_val)) {
			pte_unmap(ptep);
			goto OUT;
		}
		pfn = pte_pfn(_pte_val);
		pte_unmap(ptep);
	}
OUT:
	return pfn;
}
#endif

/* copy from fs/binfmt_elf.c */
static void fill_elf_header(struct elfhdr *elf, int segs)
{
	memcpy(elf->e_ident, ELFMAG, SELFMAG);
	elf->e_ident[EI_CLASS] = ELF_CLASS;
	elf->e_ident[EI_DATA] = ELF_DATA;
	elf->e_ident[EI_VERSION] = EV_CURRENT;
	elf->e_ident[EI_OSABI] = ELF_OSABI;

	elf->e_type = ET_CORE;
	elf->e_machine = ELF_ARCH;
	elf->e_version = EV_CURRENT;
	elf->e_phoff = sizeof(struct elfhdr);
#ifndef ELF_CORE_EFLAGS
#define ELF_CORE_EFLAGS	0
#endif
	elf->e_flags = ELF_CORE_EFLAGS;
	elf->e_ehsize = sizeof(struct elfhdr);
	elf->e_phentsize = sizeof(struct elf_phdr);
	elf->e_phnum = segs;

}

static void fill_elf_note_phdr(struct elf_phdr *phdr, int sz, loff_t offset)
{
	phdr->p_type = PT_NOTE;
	phdr->p_offset = offset;
	phdr->p_vaddr = 0;
	phdr->p_paddr = 0;
	phdr->p_filesz = sz;
	phdr->p_memsz = 0;
	phdr->p_flags = 0;
	phdr->p_align = 0;
}

static void fill_elf_load_phdr(struct elf_phdr *phdr, int sz,
			       unsigned long vaddr, unsigned long paddr)
{
	phdr->p_type = PT_LOAD;
	phdr->p_vaddr = vaddr;
	phdr->p_paddr = paddr;
	phdr->p_filesz = sz;
	phdr->p_memsz = 0;
	phdr->p_flags = 0;
	phdr->p_align = 0;
}

static noinline void fill_note(struct elf_note *note, const char *name,
		int type, unsigned int sz, unsigned int namesz)
{
	char *n_name = (char *)note + sizeof(struct elf_note);

	note->n_namesz = namesz;
	note->n_type = type;
	note->n_descsz = sz;
	strncpy(n_name, name, note->n_namesz);
}

static void fill_note_L(struct elf_note *note, const char *name, int type,
		unsigned int sz)
{
	fill_note(note, name, type, sz, NOTE_NAME_LONG);
}

static void fill_note_S(struct elf_note *note, const char *name, int type,
		unsigned int sz)
{
	fill_note(note, name, type, sz, NOTE_NAME_SHORT);
}

/*
 * fill up all the fields in prstatus from the given task struct, except
 * registers which need to be filled up separately.
 */
static void fill_prstatus(struct elf_prstatus *prstatus, struct pt_regs *regs,
			  struct task_struct *p, unsigned long pid)
{
	elf_core_copy_regs(&prstatus->pr_reg, regs);
	prstatus->pr_pid = pid;
	prstatus->pr_ppid = nr_cpu_ids;
	prstatus->pr_sigpend = (uintptr_t)p;
}

static int fill_psinfo(struct elf_prpsinfo *psinfo)
{
	unsigned int i;

	strncpy(psinfo->pr_psargs, mrdump_get_cmd(), ELF_PRARGSZ - 1);
	for (i = 0; i < ELF_PRARGSZ - 1; i++)
		if (psinfo->pr_psargs[i] == 0)
			psinfo->pr_psargs[i] = ' ';
	psinfo->pr_psargs[ELF_PRARGSZ - 1] = 0;
	strncpy(psinfo->pr_fname, "vmlinux", sizeof(psinfo->pr_fname));
	return 0;
}

#ifndef __pa_nodebug
#ifdef __pa_symbol_nodebug
#define __pa_nodebug __pa_symbol_nodebug
#else
#define __pa_nodebug __pa
#endif
#endif
void mrdump_mini_add_misc_pa(unsigned long va, unsigned long pa,
		unsigned long size, unsigned long start, char *name)
{
	int i;
	struct elf_note *note;

	for (i = 0; i < MRDUMP_MINI_NR_MISC; i++) {
		note = &mrdump_mini_ehdr->misc[i].note;
		if (note->n_type == NT_IPANIC_MISC) {
			if (strncmp(mrdump_mini_ehdr->misc[i].name, name, 16)
					!= 0)
				continue;
		}
		mrdump_mini_ehdr->misc[i].data.vaddr = va;
		mrdump_mini_ehdr->misc[i].data.paddr = pa;
		mrdump_mini_ehdr->misc[i].data.size = size;
		mrdump_mini_ehdr->misc[i].data.start =
		    mrdump_virt_addr_valid((void *)start) ?
			__pa_nodebug(start) : 0;
		fill_note_L(note, name, NT_IPANIC_MISC,
				sizeof(struct mrdump_mini_elf_misc));
		break;
	}
}

static void mrdump_mini_add_misc(unsigned long addr, unsigned long size,
		unsigned long start, char *name)
{
	if (!mrdump_virt_addr_valid((void *)addr))
		return;
	mrdump_mini_add_misc_pa(addr, __pa_nodebug(addr), size, start, name);
}

int kernel_addr_valid(unsigned long addr)
{
	if (addr < MIN_MARGIN)
		return 0;

	return pfn_valid(virt_2_pfn(addr));
}

static void mrdump_mini_add_entry_ext(unsigned long start, unsigned long end,
		unsigned long pa)
{
	unsigned long laddr, haddr;
	struct elf_phdr *phdr;
	int i;

	for (i = 0; i < MRDUMP_MINI_NR_SECTION; i++) {
		phdr = &mrdump_mini_ehdr->phdrs[i];
		if (phdr->p_type == PT_NULL)
			break;
		if (phdr->p_type != PT_LOAD)
			continue;
		laddr = phdr->p_vaddr;
		haddr = laddr + phdr->p_filesz;
		if (start >= laddr && end <= haddr)
			return;
		if (start >= haddr || end <= laddr)
			continue;
		if (laddr < start) {
			start = laddr;
			pa = phdr->p_paddr;
		}
		if (haddr > end)
			end = haddr;
		break;
	}
	if (i < MRDUMP_MINI_NR_SECTION)
		fill_elf_load_phdr(phdr, end - start, start, pa);
	else
		pr_notice("mrdump: MINI_NR_SECTION overflow!\n");
}

void mrdump_mini_add_entry(unsigned long addr, unsigned long size)
{
	unsigned long start = 0, __end, _pfn;
	unsigned long laddr;

	if (!pfn_valid(virt_2_pfn(addr)))
		return;

	if (size > PAGE_SIZE) {
		__end = ALIGN(addr + size / 2, PAGE_SIZE);
		laddr = __end - ALIGN(size, PAGE_SIZE);

		start = addr & PAGE_MASK;
		while (start >= laddr) {
			_pfn = virt_2_pfn(start - PAGE_SIZE);
			if (!pfn_valid(_pfn)) {
				laddr = start;
				break;
			}
			start -= PAGE_SIZE;
		}

		start = addr & PAGE_MASK;
		while (start < __end) {
			start += PAGE_SIZE;
			_pfn = virt_2_pfn(start);
			if (!pfn_valid(_pfn)) {
				__end = start;
				break;
			}
		}

		if (pfn_valid(virt_2_pfn(laddr))
			&& __end > laddr) {
			mrdump_mini_add_entry_ext(laddr, __end,
					__pfn_to_phys(virt_2_pfn(laddr)));
		} else {
			/* should never be here just in case and 1 page safe*/
			start = addr & PAGE_MASK;
			mrdump_mini_add_entry_ext(start, start + PAGE_SIZE,
					__pfn_to_phys(virt_2_pfn(addr)));
		}
	} else {
		start = addr & PAGE_MASK;
		mrdump_mini_add_entry_ext(start, start + PAGE_SIZE,
				__pfn_to_phys(virt_2_pfn(addr)));
	}
}

static void mrdump_mini_add_tsk_ti(int cpu, struct pt_regs *regs,
		struct task_struct *tsk, int stack)
{
	struct thread_info *ti = NULL;
	unsigned long *bottom;
	unsigned long *top;
	unsigned long *p;

	if (!mrdump_virt_addr_valid(tsk)) {
		pr_notice("mrdump: cpu:[%d] invalid task pointer:%p\n",
				cpu, tsk);
		if (cpu < num_possible_cpus())
			tsk = aee_cpu_curr(cpu);
		else
			pr_notice("mrdump: cpu:[%d] overflow with total:%d\n",
					cpu, num_possible_cpus());
	}
	if (!mrdump_virt_addr_valid(tsk))
		pr_notice("mrdump: cpu:[%d] CAN'T get a valid task pointer:%p\n",
				cpu, tsk);
	else
		ti = (struct thread_info *)tsk->stack;

	bottom = (unsigned long *)regs->reg_sp;
	mrdump_mini_add_entry(regs->reg_sp, MRDUMP_MINI_SECTION_SIZE);
	mrdump_mini_add_entry((unsigned long)ti, MRDUMP_MINI_SECTION_SIZE);
	mrdump_mini_add_entry((unsigned long)tsk, MRDUMP_MINI_SECTION_SIZE);
	pr_notice("mrdump: cpu[%d] tsk:%p ti:%p\n", cpu, tsk, ti);
	if (!stack)
		return;
	if (ti == NULL)
		return;
#ifdef __aarch64__
	if (aee_on_irq_stack((unsigned long)bottom, NULL))
		/* TODO: correct me */
		top = 0;
		/* top = (unsigned long *)IRQ_STACK_PTR(cpu); */
	else {
		top = (unsigned long *)ALIGN((unsigned long)bottom,
					THREAD_SIZE);
		p = (unsigned long *)((void *)ti + THREAD_SIZE);
		if ((!mrdump_virt_addr_valid(top)
				&& !(((unsigned long)top >= VMALLOC_START)
				&& ((unsigned long)top <= VMALLOC_END)))
			|| (!mrdump_virt_addr_valid(bottom)
				&& !(((unsigned long)bottom >= VMALLOC_START)
				&& ((unsigned long)bottom <= VMALLOC_END)))
			|| top != p || bottom > top) {
			pr_notice(
				"mrdump: unexpected case bottom:%p top:%p ti + THREAD_SIZE:%p\n"
				, bottom, top, p);
			return;
		}
	}
#else
	top = (unsigned long *)((void *)ti + THREAD_SIZE);
	if (!mrdump_virt_addr_valid(ti) || !mrdump_virt_addr_valid(top)
		|| bottom < (unsigned long *)ti
		|| bottom > top)
		return;
#endif

	for (p = (unsigned long *)ALIGN((unsigned long)bottom,
				sizeof(unsigned long)); p < top; p++) {
		if (!mrdump_virt_addr_valid(*p))
			continue;
		if (*p >= (unsigned long)ti && *p <= (unsigned long)top)
			continue;
		if (*p >= aee_get_stext() && *p <=
				(unsigned long)aee_get_etext())
			continue;
		mrdump_mini_add_entry(*p, MRDUMP_MINI_SECTION_SIZE);
	}
}

static int mrdump_mini_cpu_regs(int cpu, struct pt_regs *regs,
		struct task_struct *tsk, int main)
{
	char name[NOTE_NAME_SHORT];
	int id;

	if (!mrdump_mini_ehdr) {
		pr_notice("mrdump: invalid ehdr");
		return -1;
	}
	if (cpu >= nr_cpu_ids) {
		pr_notice("mrdump: invalid cpu - %d", cpu);
		return -1;
	}
	if (!regs) {
		pr_notice("mrdump: invalid regs");
		return -1;
	}
	id = main ? 0 : cpu + 1;
	if (strncmp(mrdump_mini_ehdr->prstatus[id].name, "NA", 2))
		return -1;
	snprintf(name, NOTE_NAME_SHORT - 1, main ? "ke%d" : "core%d", cpu);
	fill_prstatus(&mrdump_mini_ehdr->prstatus[id].data, regs, tsk,
			id ? id : (100 + cpu));
	fill_note_S(&mrdump_mini_ehdr->prstatus[id].note, name, NT_PRSTATUS,
		    sizeof(struct elf_prstatus));
	return 0;
}

void mrdump_mini_per_cpu_regs(int cpu, struct pt_regs *regs,
		struct task_struct *tsk)
{
	mrdump_mini_cpu_regs(cpu, regs, tsk, 0);
}
EXPORT_SYMBOL(mrdump_mini_per_cpu_regs);

static void mrdump_mini_build_task_info(struct pt_regs *regs)
{
#define MAX_STACK_TRACE_DEPTH 64
	unsigned long ipanic_stack_entries[MAX_STACK_TRACE_DEPTH];
	char symbol[96] = {'\0'};
	int sz;
#ifdef CONFIG_STACKTRACE
	int off, plen;
	struct stack_trace trace;
	int i;
#endif
	struct task_struct *tsk, *cur;
	struct task_struct *previous;
	struct aee_process_info *cur_proc;

	if (!mrdump_mini_ehdr) {
		pr_notice("mrdump: ehder invalid\n");
		return;
	}

	if (!mrdump_virt_addr_valid(current_thread_info())) {
		pr_notice("current thread info invalid\n");
		return;
	}
	cur = current;
	tsk = cur;
	if (!mrdump_virt_addr_valid(tsk)) {
		pr_notice("tsk invalid\n");
		return;
	}
	cur_proc = (struct aee_process_info *)((void *)mrdump_mini_ehdr +
			MRDUMP_MINI_HEADER_SIZE);
	/* Current panic user tasks */
	sz = 0;
	do {
		if (!tsk) {
			pr_notice("No tsk info\n");
			memset_io(cur_proc, 0x0,
				sizeof(struct aee_process_info));
			break;
		}
		/* FIXME: Check overflow ? */
		sz += snprintf(symbol + sz, 96 - sz, "[%s, %d]", tsk->comm,
				tsk->pid);
		previous = tsk;
		tsk = tsk->real_parent;
		if (!mrdump_virt_addr_valid(tsk)) {
			pr_notice("tsk(%p) invalid (previous: [%s, %d])\n", tsk,
					previous->comm, previous->pid);
			break;
		}
	} while (tsk && (tsk->pid != 0) && (tsk->pid != 1));
	if (!strncmp(cur_proc->process_path, symbol, sz)) {
		pr_notice("same process path\n");
		return;
	}

	memset_io(cur_proc, 0, sizeof(struct aee_process_info));
	memcpy(cur_proc->process_path, symbol, sz);

#ifdef CONFIG_STACKTRACE
	/* Grab kernel task stack trace */
	trace.nr_entries = 0;
	trace.max_entries = MAX_STACK_TRACE_DEPTH;
	trace.entries = ipanic_stack_entries;
	/* the value is only from experience and without strict rules
	 * need to pay attention to the value
	 */
	trace.skip = 4;
	save_stack_trace_tsk(cur, &trace);
	/* Skip the entries -
	 * ipanic_save_current_tsk_info/save_stack_trace_tsk
	 */
	for (i = 0; i < trace.nr_entries; i++) {
		off = strlen(cur_proc->backtrace);
		plen = AEE_BACKTRACE_LENGTH - ALIGN(off, 8);
		if (plen > 16) {
			sz = snprintf(symbol, 96, "[<%px>] %pS\n",
				      (void *)ipanic_stack_entries[i],
				      (void *)ipanic_stack_entries[i]);
			if (ALIGN(sz, 8) - sz) {
				memset_io(symbol + sz - 1, ' ',
						ALIGN(sz, 8) - sz);
				memset_io(symbol + ALIGN(sz, 8) - 1, '\n', 1);
			}
			if (ALIGN(sz, 8) <= plen)
				memcpy(cur_proc->backtrace + ALIGN(off, 8),
						symbol, ALIGN(sz, 8));
		}
	}
#endif
	if (regs) {
		cur_proc->ke_frame.pc = (__u64) regs->reg_pc;
		cur_proc->ke_frame.lr = (__u64) regs->reg_lr;
	} else {
		/* in case panic() is called without die */
		/* Todo: a UT for this */
		cur_proc->ke_frame.pc = ipanic_stack_entries[0];
		cur_proc->ke_frame.lr = ipanic_stack_entries[1];
	}
	if (mrdump_virt_addr_valid(cur_proc->ke_frame.pc))
		snprintf(cur_proc->ke_frame.pc_symbol, AEE_SZ_SYMBOL_S,
			"[<%px>] %pS",
			(void *)(unsigned long)cur_proc->ke_frame.pc,
			(void *)(unsigned long)cur_proc->ke_frame.pc);
	else
		pr_info("[<%llu>] invalid pc", cur_proc->ke_frame.pc);
	if (mrdump_virt_addr_valid(cur_proc->ke_frame.lr))
		snprintf(cur_proc->ke_frame.lr_symbol, AEE_SZ_SYMBOL_L,
			"[<%px>] %pS",
			(void *)(unsigned long)cur_proc->ke_frame.lr,
			(void *)(unsigned long)cur_proc->ke_frame.lr);
	else
		pr_info("[<%llu>] invalid lr", cur_proc->ke_frame.lr);

}

int mrdump_task_info(unsigned char *buffer, size_t sz_buf)
{
	if (sz_buf < sizeof(struct aee_process_info))
		return -1;
	memcpy(buffer, (void *)mrdump_mini_ehdr + MRDUMP_MINI_HEADER_SIZE,
	       sizeof(struct aee_process_info));
	return sizeof(struct aee_process_info);
}

static int mrdump_modules_info(unsigned char *buffer, size_t sz_buf)
{
#ifdef CONFIG_MODULES
	int sz;

	sz = aee_save_modules(modules_info_buf, MODULES_INFO_BUF_SIZE);
	if (sz <= 0 || sz_buf < sz || !buffer)
		return -1;
	memcpy(buffer, modules_info_buf, sz);
	return sz;
#else
	return -1;
#endif
}

#define EXTRA_MISC(func, name, max_size) \
	__weak void func(unsigned long *vaddr, unsigned long *size) \
	{ \
		if (size) \
			*size = 0; \
	}
#include "mrdump_mini_extra_misc.h"

#undef EXTRA_MISC
#define EXTRA_MISC(func, name, max_size) \
	{func, name, max_size},

static struct mrdump_mini_extra_misc extra_members[] = {
	#include "mrdump_mini_extra_misc.h"
};

void *extra_members_buf;

#define EXTRA_TOTAL_NUM ((sizeof(extra_members)) / (sizeof(extra_members[0])))
static size_t __maybe_unused dummy_check(void)
{
	size_t dummy;

	dummy = BUILD_BUG_ON_ZERO(EXTRA_TOTAL_NUM > 10);
	return dummy;
}

static int _mrdump_mini_add_extra_misc(unsigned long vaddr, unsigned long size,
	const char *name)
{
	char name_buf[SZ_128];

	if (!mrdump_mini_ehdr ||
		!size ||
		size > SZ_512K ||
		!name)
		return -1;
	snprintf(name_buf, SZ_128, "_EXTRA_%s_", name);
	mrdump_mini_add_misc(vaddr, size, 0, name_buf);
	return 0;
}

void mrdump_mini_add_extra_misc(void)
{
	static int once;
	int i;
	unsigned long vaddr = 0;
	unsigned long size = 0;
	int ret;

	if (!once) {
		once = 1;
		for (i = 0; i < EXTRA_TOTAL_NUM; i++) {
			extra_members[i].dump_func(&vaddr, &size);
			if (size > extra_members[i].max_size)
				continue;
			ret = _mrdump_mini_add_extra_misc(vaddr, size,
					extra_members[i].dump_name);
			if (ret < 0)
				pr_notice("mrdump: add %s:0x%lx sz:0x%lx failed\n",
					extra_members[i].dump_name,
					vaddr, size);
		}
		extra_members_buf = kzalloc(sizeof(extra_members), GFP_KERNEL);
		if (!extra_members_buf)
			return;
		memcpy(extra_members_buf, extra_members, sizeof(extra_members));
		_mrdump_mini_add_extra_misc((unsigned long)extra_members_buf,
			sizeof(extra_members), "ALL");
	}
}
EXPORT_SYMBOL(mrdump_mini_add_extra_misc);

static void mrdump_mini_fatal(const char *str)
{
	pr_notice("minirdump: FATAL:%s\n", str);
}

static unsigned int mrdump_mini_addr;
static unsigned int mrdump_mini_size;
void mrdump_mini_set_addr_size(unsigned int addr, unsigned int size)
{
	mrdump_mini_addr = addr;
	mrdump_mini_size = size;
}
EXPORT_SYMBOL(mrdump_mini_set_addr_size);

static void mrdump_mini_build_elf_misc(void)
{
	struct mrdump_mini_elf_misc misc;
	unsigned long task_info_va =
	    (unsigned long)((void *)mrdump_mini_ehdr + MRDUMP_MINI_HEADER_SIZE);
	unsigned long task_info_pa = 0;

	if (mrdump_mini_addr
		&& mrdump_mini_size
		&& MRDUMP_MINI_HEADER_SIZE < mrdump_mini_size) {
		task_info_pa = (unsigned long)(mrdump_mini_addr +
				MRDUMP_MINI_HEADER_SIZE);
	} else {
		pr_notice("minirdump: unexpected addr:0x%x, size:0x%x(0x%x)\n",
			mrdump_mini_addr, mrdump_mini_size,
			(unsigned int)MRDUMP_MINI_HEADER_SIZE);
		mrdump_mini_fatal("illegal addr size");
	}
	mrdump_mini_add_misc_pa(task_info_va, task_info_pa,
			sizeof(struct aee_process_info), 0, "PROC_CUR_TSK");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	/* could also use the kernel log in pstore for LKM case */
	misc.vaddr = (unsigned long)aee_log_buf_addr_get();
	misc.size = (unsigned long)aee_log_buf_len_get();
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_KERNEL_LOG_");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	get_mbootlog_buffer(&misc.vaddr, &misc.size, &misc.start);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_LAST_KMSG");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	aee_rr_get_desc_info(&misc.vaddr, &misc.size, &misc.start);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_RR_DESC_");
#if IS_ENABLED(CONFIG_HAVE_MTK_GZ_LOG)
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	get_gz_log_buffer(&misc.vaddr, &misc.paddr, &misc.size, &misc.start);
	if (misc.paddr)
		mrdump_mini_add_misc_pa(misc.vaddr, misc.paddr, misc.size,
					misc.start, "_GZ_LOG_");
#endif
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	get_disp_err_buffer(&misc.vaddr, &misc.size, &misc.start);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_DISP_ERR_");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	get_disp_dump_buffer(&misc.vaddr, &misc.size, &misc.start);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_DISP_DUMP_");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	get_disp_fence_buffer(&misc.vaddr, &misc.size, &misc.start);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_DISP_FENCE_");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	get_disp_dbg_buffer(&misc.vaddr, &misc.size, &misc.start);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_DISP_DBG_");
#ifdef CONFIG_MODULES
	modules_info_buf = kzalloc(MODULES_INFO_BUF_SIZE, GFP_KERNEL);
	if (modules_info_buf)
		mrdump_mini_add_misc_pa((unsigned long)modules_info_buf,
			(unsigned long)__pa_nodebug(
					(unsigned long)modules_info_buf),
			MODULES_INFO_BUF_SIZE, 0, "SYS_MODULES_INFO");
#endif

	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	get_pidmap_aee_buffer(&misc.vaddr, &misc.size);
	misc.start = 0;
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_PIDMAP_");
}

static void mrdump_mini_add_loads(void)
{
	int cpu, i, id;
	struct pt_regs regs;
	struct elf_prstatus *prstatus;
	struct task_struct *tsk;
	struct thread_info *ti;

	if (!mrdump_mini_ehdr)
		return;
	for (id = 0; id < nr_cpu_ids + 1; id++) {
		if (!strncmp(mrdump_mini_ehdr->prstatus[id].name, "NA", 2))
			continue;
		prstatus = &mrdump_mini_ehdr->prstatus[id].data;
		tsk = (prstatus->pr_sigpend) ?
			(struct task_struct *)prstatus->pr_sigpend : current;
		memcpy(&regs, &prstatus->pr_reg, sizeof(prstatus->pr_reg));
		if (prstatus->pr_pid >= 100) {
			for (i = 0; i < ELF_NGREG; i++)
				mrdump_mini_add_entry(
						((unsigned long *)&regs)[i],
						MRDUMP_MINI_SECTION_SIZE);
			cpu = prstatus->pr_pid - 100;
			mrdump_mini_add_tsk_ti(cpu, &regs, tsk, 1);
			mrdump_mini_add_entry((unsigned long)aee_cpu_rq(cpu),
					MRDUMP_MINI_SECTION_SIZE);
		} else if (prstatus->pr_pid <= nr_cpu_ids) {
			cpu = prstatus->pr_pid - 1;
			mrdump_mini_add_tsk_ti(cpu, &regs, tsk, 0);
			for (i = 0; i < ELF_NGREG; i++) {
				mrdump_mini_add_entry(
					((unsigned long *)&regs)[i],
					MRDUMP_MINI_SECTION_SIZE);
			}
		} else {
			pr_notice("mrdump: wrong pr_pid: %d\n",
					prstatus->pr_pid);
		}
	}

	mrdump_mini_add_entry((unsigned long)__per_cpu_offset,
			MRDUMP_MINI_SECTION_SIZE);
	if (dump_all_cpus) {
		for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
			tsk = aee_cpu_curr(cpu);
			if (mrdump_virt_addr_valid(tsk))
				ti = (struct thread_info *)tsk->stack;
			else
				ti = NULL;
			mrdump_mini_add_entry((unsigned long)aee_cpu_rq(cpu),
					MRDUMP_MINI_SECTION_SIZE);
			mrdump_mini_add_entry((unsigned long)tsk,
					MRDUMP_MINI_SECTION_SIZE);
			mrdump_mini_add_entry((unsigned long)ti,
					MRDUMP_MINI_SECTION_SIZE);
		}
	}
}

static void mrdump_mini_clear_loads(void)
{
	struct elf_phdr *phdr;
	int i;

	for (i = 0; i < MRDUMP_MINI_NR_SECTION; i++) {
		phdr = &mrdump_mini_ehdr->phdrs[i];
		if (phdr->p_type == PT_NULL)
			continue;
		if (phdr->p_type == PT_LOAD)
			phdr->p_type = PT_NULL;
	}
}

void mrdump_mini_add_hang_raw(unsigned long vaddr, unsigned long size)
{
	pr_notice("mrdump: hang data 0x%lx size:0x%lx\n", vaddr, size);
	if (!mrdump_mini_ehdr) {
		pr_notice("mrdump: ehdr invalid");
		return;
	}
	mrdump_mini_add_misc(vaddr, size, 0, "_HANG_DETECT_");
	/* hang only remove mini rdump loads info to save storage space */
	mrdump_mini_clear_loads();
}
EXPORT_SYMBOL(mrdump_mini_add_hang_raw);

void mrdump_mini_ke_cpu_regs(struct pt_regs *regs)
{
	int cpu;
	struct pt_regs context;

	if (!regs) {
		regs = &context;
		crash_setup_regs(regs, NULL);
	}
	cpu = get_HW_cpuid();
	mrdump_mini_cpu_regs(cpu, regs, current, 1);
	mrdump_mini_add_loads();
	mrdump_mini_build_task_info(regs);
	mrdump_modules_info(NULL, -1);
	mrdump_mini_add_extra_misc();
}
EXPORT_SYMBOL(mrdump_mini_ke_cpu_regs);

static void *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages)
		return NULL;

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, PAGE_KERNEL);
	kfree(pages);
	if (!vaddr) {
		pr_notice("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}

static void __init mrdump_mini_elf_header_init(void)
{
	if (mrdump_mini_addr && mrdump_mini_size) {
		mrdump_mini_ehdr =
		    remap_lowmem(mrdump_mini_addr,
				 mrdump_mini_size);
		pr_notice("minirdump: [DT] reserved 0x%x+0x%lx->%p\n",
			mrdump_mini_addr,
			(unsigned long)mrdump_mini_size,
			mrdump_mini_ehdr);
	} else {
		pr_notice("minirdump: [DT] illegal value 0x%x(0x%x)\n",
				mrdump_mini_addr,
				mrdump_mini_size);
		mrdump_mini_fatal("illegal addr size");
		return;
	}
	if (!mrdump_mini_ehdr) {
		pr_notice("mrdump mini reserve buffer fail");
		mrdump_mini_fatal("header null pointer");
		return;
	}
	memset_io(mrdump_mini_ehdr, 0, MRDUMP_MINI_HEADER_SIZE +
			sizeof(struct aee_process_info));
	fill_elf_header(&mrdump_mini_ehdr->ehdr, MRDUMP_MINI_NR_SECTION);
}

int __init mrdump_mini_init(void)
{
	int i;
	unsigned long size, offset;
	struct pt_regs regs;

	mrdump_mini_elf_header_init();
	if (!mrdump_mini_ehdr) {
		pr_notice("mrdump: mini init fail\n");
		return -1;
	}
	fill_psinfo(&mrdump_mini_ehdr->psinfo.data);
	fill_note_S(&mrdump_mini_ehdr->psinfo.note, "vmlinux", NT_PRPSINFO,
		    sizeof(struct elf_prpsinfo));

	memset_io(&regs, 0, sizeof(struct pt_regs));
	for (i = 0; i < AEE_MTK_CPU_NUMS + 1; i++) {
		fill_prstatus(&mrdump_mini_ehdr->prstatus[i].data, &regs,
				NULL, i);
		fill_note_S(&mrdump_mini_ehdr->prstatus[i].note, "NA",
				NT_PRSTATUS, sizeof(struct elf_prstatus));
	}

	offset = offsetof(struct mrdump_mini_elf_header, psinfo);
	size = sizeof(mrdump_mini_ehdr->psinfo) +
		sizeof(mrdump_mini_ehdr->prstatus);
	fill_elf_note_phdr(&mrdump_mini_ehdr->phdrs[0], size, offset);

	for (i = 0; i < MRDUMP_MINI_NR_MISC; i++)
		fill_note_L(&mrdump_mini_ehdr->misc[i].note, "NA", 0,
			    sizeof(struct mrdump_mini_elf_misc));
	mrdump_mini_build_elf_misc();
	fill_elf_note_phdr(&mrdump_mini_ehdr->phdrs[1],
			sizeof(mrdump_mini_ehdr->misc),
			offsetof(struct mrdump_mini_elf_header, misc));

	if (mrdump_cblock) {

		mrdump_mini_add_entry_ext(
		  (unsigned long)mrdump_cblock,
		  (unsigned long)mrdump_cblock + mrdump_sram_cb.size,
		  mrdump_sram_cb.start_addr
		);

		mrdump_mini_add_entry(
		  (unsigned long)mrdump_cblock,
		  sizeof(struct mrdump_control_block) + 2 * PAGE_SIZE
		);

		mrdump_mini_add_entry(
		  (aee_get_kallsyms_addresses() +
		  (mrdump_cblock->machdesc.kallsyms.size / 2 - PAGE_SIZE)),
		  mrdump_cblock->machdesc.kallsyms.size + 2 * PAGE_SIZE);
	}

	return 0;
}

int mini_rdump_reserve_memory(struct reserved_mem *rmem)
{
	pr_info("[memblock]%s: 0x%llx - 0x%llx (0x%llx)\n",
		"mediatek,minirdump",
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->base +
		 (unsigned long long)rmem->size,
		 (unsigned long long)rmem->size);
	return 0;
}

RESERVEDMEM_OF_DECLARE(reserve_memory_minirdump, "mediatek,minirdump",
		       mini_rdump_reserve_memory);

/* 0644: S_IRUGO | S_IWUSR */
module_param(dump_all_cpus, bool, 0644);
