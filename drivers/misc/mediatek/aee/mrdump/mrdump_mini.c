/*
 * Copyright (C) 2016 MediaTek Inc.
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

#include <linux/init.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/memblock.h>
#include <linux/elf.h>
#include <linux/kdebug.h>
#include <linux/module.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>
#include <linux/stacktrace.h>
#include <asm/pgtable.h>
#include <asm-generic/percpu.h>
#include <asm-generic/sections.h>
#include <asm/page.h>
#include <smp.h>
#include <mrdump.h>
#include <mt-plat/aee.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_reserved_mem.h>
#include "../../../../kernel/sched/sched.h"
#include "mrdump_mini.h"

#define LOG_DEBUG(fmt, ...)			\
	do {	\
		if (aee_in_nested_panic())			\
			aee_nested_printf(fmt, ##__VA_ARGS__);	\
		else						\
			pr_debug(fmt, ##__VA_ARGS__);	\
	} while (0)

#define LOG_ERROR(fmt, ...)			\
	do {	\
		if (aee_in_nested_panic())			\
			aee_nested_printf(fmt, ##__VA_ARGS__);	\
		else						\
			pr_err(fmt, ##__VA_ARGS__);	\
	} while (0)

#define LOGV(fmt, msg...)
#define LOGD LOG_DEBUG
#define LOGI LOG_DEBUG
#define LOGW LOG_ERROR
#define LOGE LOG_ERROR

static struct mrdump_mini_elf_header *mrdump_mini_ehdr;

static bool dump_all_cpus;

__weak void get_android_log_buffer(unsigned long *addr, unsigned long *size, unsigned long *start,
				   int type)
{
}

__weak void get_disp_err_buffer(unsigned long *addr, unsigned long *size, unsigned long *start)
{
}


__weak void get_disp_fence_buffer(unsigned long *addr, unsigned long *size, unsigned long *start)
{
}

__weak void get_disp_dbg_buffer(unsigned long *addr, unsigned long *size, unsigned long *start)
{
}

__weak void get_disp_dump_buffer(unsigned long *addr, unsigned long *size, unsigned long *start)
{
}

__weak struct vm_struct *find_vm_area(const void *addr)
{
	return NULL;
}

#undef virt_addr_valid
#ifdef CONFIG_MODULES
#define virt_addr_valid(kaddr) (((void *)(kaddr) >= (void *)PAGE_OFFSET && \
				(void *)(kaddr) < (void *)high_memory && \
				pfn_valid(__pa(kaddr) >> PAGE_SHIFT)) || \
				is_module_address((unsigned long)kaddr))
#else
#define virt_addr_valid(kaddr) ((void *)(kaddr) >= (void *)PAGE_OFFSET && \
				(void *)(kaddr) < (void *)high_memory && \
				pfn_valid(__pa(kaddr) >> PAGE_SHIFT))
#endif

void check_addr_valid(unsigned long addr, unsigned long *low, unsigned long *high)
{
	unsigned long l = *low;
	unsigned long h = *high;

	while (l < addr) {
		if (!virt_addr_valid(l)) {
			*low += PAGE_SIZE;
			LOGE("address(0x%lx), low is invalid(0x%lx), new low is 0x%lx\n", addr, l, *low);
		}
		l += PAGE_SIZE;
	}
	if (*low > addr)
		*low = addr;
	while (h > addr) {
		if (!virt_addr_valid(h)) {
			*high -= PAGE_SIZE;
			LOGE("address(0x%lx), high is invalid(0x%lx), new high is 0x%lx\n", addr, l, *high);
		}
		h -= PAGE_SIZE;
	}
	if (*high < addr)
		*high = addr;
}

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

static noinline void fill_note(struct elf_note *note, const char *name, int type,
			       unsigned int sz, unsigned int namesz)
{
	char *n_name = (char *)note + sizeof(struct elf_note);
	/* char *n_name = container_of(note, struct mrdump_mini_elf_psinfo, note)->name; */
	note->n_namesz = namesz;
	note->n_type = type;
	note->n_descsz = sz;
	strncpy(n_name, name, note->n_namesz);
}

static void fill_note_L(struct elf_note *note, const char *name, int type, unsigned int sz)
{
	fill_note(note, name, type, sz, NOTE_NAME_LONG);
}

static void fill_note_S(struct elf_note *note, const char *name, int type, unsigned int sz)
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
	prstatus->pr_ppid = NR_CPUS;
}

static int fill_psinfo(struct elf_prpsinfo *psinfo)
{
	unsigned int i;

	strncpy(psinfo->pr_psargs, saved_command_line, ELF_PRARGSZ - 1);
	for (i = 0; i < ELF_PRARGSZ - 1; i++)
		if (psinfo->pr_psargs[i] == 0)
			psinfo->pr_psargs[i] = ' ';
	psinfo->pr_psargs[ELF_PRARGSZ - 1] = 0;
	strncpy(psinfo->pr_fname, "vmlinux", sizeof(psinfo->pr_fname));
	return 0;
}

void mrdump_mini_add_misc_pa(unsigned long va, unsigned long pa, unsigned long size,
			     unsigned long start, char *name)
{
	int i;
	struct elf_note *note;

	for (i = 0; i < MRDUMP_MINI_NR_MISC; i++) {
		note = &mrdump_mini_ehdr->misc[i].note;
		if (note->n_type == NT_IPANIC_MISC) {
			if (strncmp(mrdump_mini_ehdr->misc[i].name, name, 16) != 0)
				continue;
		}
		mrdump_mini_ehdr->misc[i].data.vaddr = va;
		mrdump_mini_ehdr->misc[i].data.paddr = pa;
		mrdump_mini_ehdr->misc[i].data.size = size;
		mrdump_mini_ehdr->misc[i].data.start =
		    virt_addr_valid((void *)start) ? __pa(start) : 0;
		fill_note_L(note, name, NT_IPANIC_MISC, sizeof(struct mrdump_mini_elf_misc));
		break;
	}
}

void mrdump_mini_add_misc(unsigned long addr, unsigned long size, unsigned long start, char *name)
{
	if (!virt_addr_valid((void *)addr))
		return;
	mrdump_mini_add_misc_pa(addr, __pa(addr), size, start, name);
}

int kernel_addr_valid(unsigned long addr)
{
	pgd_t *pgd;
	pud_t *pud;
	pmd_t *pmd;
	pte_t *pte;

	if (addr < PAGE_OFFSET)
		return 0;

	pgd = pgd_offset_k(addr);
	if (pgd_none(*pgd))
		return 0;
	pr_err("[%08lx] *pgd=%08llx", addr, (long long)pgd_val(*pgd));

	pud = pud_offset(pgd, addr);
	if (pud_none(*pud))
		return 0;
	pr_err("*pud=%08llx", (long long)pud_val(*pud));

	pmd = pmd_offset(pud, addr);
	if (pmd_none(*pmd))
		return 0;
	pr_err("*pmd=%08llx", (long long)pmd_val(*pmd));

	pte = pte_offset_kernel(pmd, addr);
	if (pte_none(*pte))
		return 0;
	pr_err("*pte=%08llx", (long long)pte_val(*pte));

	return pfn_valid(pte_pfn(*pte));
}

#ifdef CONFIG_MODULES
void mrdump_mini_add_entry_module(unsigned long addr, unsigned long size)
{
	struct module *mod = NULL;
	unsigned long init_start = 0;
	unsigned long init_end = 0;
	unsigned long core_start = 0;
	unsigned long core_end = 0;
	struct elf_phdr *phdr;
	/* struct vm_area_struct *vma; */
	struct vm_struct *vm;
	unsigned long laddr, haddr, lnew, hnew;
	unsigned long hnewtmp, lcheckaddr;
	unsigned long paddr, pcheckaddr;
	int i, checkloop;
	bool flag;

	mod = __module_address(addr);
	if (NULL == mod) {
		pr_err("mrdump: find module failed for address:%lx\n", addr);
		return;
	}
	init_start = (unsigned long)mod->module_init;
	init_end = ALIGN((init_start + mod->init_size), PAGE_SIZE);
	core_start = (unsigned long)mod->module_core;
	core_end = ALIGN((core_start + mod->core_size), PAGE_SIZE);

	if ((init_start % PAGE_SIZE) || (core_start % PAGE_SIZE)) {
		pr_err("mrdump: unaligned init_start:%lx or core_start:%lx\n", init_start, core_start);
		return;
	}

	hnew = ALIGN(addr + size / 2, PAGE_SIZE);
	lnew = hnew - ALIGN(size, PAGE_SIZE);
	if (!virt_addr_valid(addr)) {
		pr_err("mrdump: never unexpected(module) case for address:%lx\n", addr);
		/* vma = find_vma(&init_mm, addr); */
		/* pr_err("mirdump: add: %p, vma: %x", addr, vma); */
		/* if (!vma) */
		/*      return; */
		/* pr_err("mirdump: (%p, %p), (%p, %p)", vma->vm_start, vma->vm_end, lnew, hnew);                */
		/* hnew = min(vma->vm_end, hnew); */
		/* lnew = max(vma->vm_start, lnew); */
		vm = find_vm_area((void *)addr);
		if (!vm)
			return;
		/* lnew = max((unsigned long)vm->addr, lnew); */
		/* hnew = min((unsigned long)vm->addr + vm->size - PAGE_SIZE, hnew); */
		/* only dump 1 page */
		lnew = max((unsigned long)vm->addr, PAGE_ALIGN(addr) - PAGE_SIZE);
		hnew = lnew + PAGE_SIZE;
		paddr = __pfn_to_phys(vmalloc_to_pfn((void *)lnew));
	} else {
		/* check_addr_valid(addr, &lnew, &hnew); */
		if (addr >= init_start && addr < init_end) {
			lnew = init_start < lnew ? lnew : init_start;
			hnew = init_end > hnew ? hnew : init_end;
			/* lnew = init_start; */
			/* hnew = init_end; */
		} else if (addr >= core_start && addr < core_end) {
			lnew = core_start < lnew ? lnew : core_start;
			hnew = core_end > hnew ? hnew : core_end;
			/* lnew = core_start; */
			/* hnew = core_end; */
		} else {
			pr_err("mrdump: unexpected case for address:%lx\n", addr);
			return;
		}

		if ((lnew % PAGE_SIZE) || (hnew % PAGE_SIZE)) {
			pr_err("mrdump: unaligned low:%lx or high:%lx\n", lnew, hnew);
			return;
		}

		paddr = __pfn_to_phys(vmalloc_to_pfn((void *)lnew));
		flag = true;
		while (flag) {
			for (checkloop = 1; checkloop < (hnew - lnew) / PAGE_SIZE; checkloop++) {
				lcheckaddr = lnew + checkloop * PAGE_SIZE;
				pcheckaddr = __pfn_to_phys(vmalloc_to_pfn((void *)lcheckaddr));
				if (pcheckaddr != (paddr + checkloop * PAGE_SIZE)) {
					/* pr_err("mrdump: non-continuous PA for address:%lx, lnew:%lx(%lx),
						lcheckaddr:%lx(%lx)\n", addr, lnew, paddr, lcheckaddr, pcheckaddr); */
					flag = false;
					break;
				}
			}

			hnewtmp = flag ? hnew : lcheckaddr;
			for (i = 0; i < MRDUMP_MINI_NR_SECTION; i++) {
				phdr = &mrdump_mini_ehdr->phdrs[i];
				if (phdr->p_type == PT_NULL)
					break;
				if (phdr->p_type != PT_LOAD)
					continue;
				laddr = phdr->p_vaddr;
				haddr = laddr + phdr->p_filesz;
				/* full overlap with exist */
				if (lnew >= laddr && hnewtmp <= haddr)
					goto skip_fill_load;
				/* no overlap, new */
				if (lnew >= haddr || hnewtmp <= laddr)
					continue;
				/* partial overlap with exist, joining */
				lnew = lnew < laddr ? lnew : laddr;
				hnewtmp = hnewtmp > haddr ? hnewtmp : haddr;
				paddr = __pfn_to_phys(vmalloc_to_pfn((void *)lnew));
				break;
			}
			if (i < MRDUMP_MINI_NR_SECTION) {
				fill_elf_load_phdr(phdr, hnewtmp - lnew, lnew, paddr);
			} else {
				/* break while loop when section is full */
				pr_err("mrdump: unexpected section full:%d\n", MRDUMP_MINI_NR_SECTION);
				break;
			}

skip_fill_load:
			if (!flag) {
				lnew = lcheckaddr;
				paddr = pcheckaddr;
				/* continue loop */
				flag = true;
			} else {
				/* finish loop */
				flag = false;
			}
		}
	}
}
#endif

void mrdump_mini_add_entry(unsigned long addr, unsigned long size)
{
	struct elf_phdr *phdr;
	/* struct vm_area_struct *vma; */
	struct vm_struct *vm;
	unsigned long laddr, haddr, lnew, hnew;
	unsigned long paddr;
	int i;

#ifdef CONFIG_MODULES
	if (is_module_address(addr)) {
		mrdump_mini_add_entry_module(addr, size);
		return;
	}
#endif
	if (addr < PAGE_OFFSET)
		return;
	hnew = ALIGN(addr + size / 2, PAGE_SIZE);
	lnew = hnew - ALIGN(size, PAGE_SIZE);
	if (!virt_addr_valid(addr)) {
		/* vma = find_vma(&init_mm, addr); */
		/* pr_err("mirdump: add: %p, vma: %x", addr, vma); */
		/* if (!vma) */
		/*      return; */
		/* pr_err("mirdump: (%p, %p), (%p, %p)", vma->vm_start, vma->vm_end, lnew, hnew);                */
		/* hnew = min(vma->vm_end, hnew); */
		/* lnew = max(vma->vm_start, lnew); */
		vm = find_vm_area((void *)addr);
		if (!vm)
			return;
		/* lnew = max((unsigned long)vm->addr, lnew); */
		/* hnew = min((unsigned long)vm->addr + vm->size - PAGE_SIZE, hnew); */
		/* only dump 1 page */
		lnew = max((unsigned long)vm->addr, PAGE_ALIGN(addr) - PAGE_SIZE);
		hnew = lnew + PAGE_SIZE;
		paddr = __pfn_to_phys(vmalloc_to_pfn((void *)lnew));
	} else {
		check_addr_valid(addr, &lnew, &hnew);
		lnew = max(lnew, PAGE_OFFSET);
		hnew = min_t(unsigned long, hnew, (unsigned long)high_memory);
		paddr = __pa(lnew);
	}
	for (i = 0; i < MRDUMP_MINI_NR_SECTION; i++) {
		phdr = &mrdump_mini_ehdr->phdrs[i];
		if (phdr->p_type == PT_NULL)
			break;
		if (phdr->p_type != PT_LOAD)
			continue;
		laddr = phdr->p_vaddr;
		haddr = laddr + phdr->p_filesz;
		/* full overlap with exist */
		if (lnew >= laddr && hnew <= haddr)
			return;
		/* no overlap, new */
		if (lnew >= haddr || hnew <= laddr)
			continue;
		/* partial overlap with exist, joining */
		lnew = lnew < laddr ? lnew : laddr;
		hnew = hnew > haddr ? hnew : haddr;
		paddr = __pa(lnew);
		break;
	}
	if (i < MRDUMP_MINI_NR_SECTION)
		fill_elf_load_phdr(phdr, hnew - lnew, lnew, paddr);
	else
		LOGE("mrdump: MINI_NR_SECTION overflow!\n");
}

static void mrdump_mini_add_tsk_ti(int cpu, struct pt_regs *regs, int stack)
{
	struct task_struct *tsk = NULL;
	struct thread_info *ti = NULL;
	unsigned long *bottom = NULL;
	unsigned long *top = NULL;
	unsigned long *p;

	if (virt_addr_valid(regs->reg_sp)) {
		ti = (struct thread_info *)(regs->reg_sp & ~(THREAD_SIZE - 1));
		tsk = ti->task;
		bottom = (unsigned long *)regs->reg_sp;
	}
	if (!(virt_addr_valid(tsk) && ti == (struct thread_info *)tsk->stack)
	    && virt_addr_valid(regs->reg_fp)) {
		ti = (struct thread_info *)(regs->reg_fp & ~(THREAD_SIZE - 1));
		tsk = ti->task;
		bottom = (unsigned long *)regs->reg_fp;
	}
	if (!virt_addr_valid(tsk) || ti != (struct thread_info *)tsk->stack) {
		tsk = cpu_curr(cpu);
		if (virt_addr_valid(tsk)) {
			ti = (struct thread_info *)tsk->stack;
			bottom = (unsigned long *)((void *)ti + sizeof(struct thread_info));
		}
	}

	mrdump_mini_add_entry(regs->reg_sp, MRDUMP_MINI_SECTION_SIZE);
	mrdump_mini_add_entry((unsigned long)ti, MRDUMP_MINI_SECTION_SIZE);
	mrdump_mini_add_entry((unsigned long)tsk, MRDUMP_MINI_SECTION_SIZE);
	LOGE("mrdump: cpu[%d] tsk:%p ti:%p\n", cpu, tsk, ti);
	if (!stack)
		return;
	top = (unsigned long *)((void *)ti + THREAD_SIZE);
	if (!virt_addr_valid(ti) || !virt_addr_valid(top) || bottom < (unsigned long *)ti
	    || bottom > top)
		return;

	for (p = (unsigned long *)ALIGN((unsigned long)bottom, sizeof(unsigned long)); p < top; p++) {
		if (!virt_addr_valid(*p))
			continue;
		if (*p >= (unsigned long)ti && *p <= (unsigned long)top)
			continue;
		if (*p >= (unsigned long)_stext && *p <= (unsigned long)_etext)
			continue;
		mrdump_mini_add_entry(*p, MRDUMP_MINI_SECTION_SIZE);
	}
}

static int mrdump_mini_cpu_regs(int cpu, struct pt_regs *regs, int main)
{
	char name[NOTE_NAME_SHORT];
	int id;

	if (mrdump_mini_ehdr == NULL)
		mrdump_mini_init();
	if (cpu >= NR_CPUS || mrdump_mini_ehdr == NULL)
		return -1;
	id = main ? 0 : cpu + 1;
	if (strncmp(mrdump_mini_ehdr->prstatus[id].name, "NA", 2))
		return -1;
	snprintf(name, NOTE_NAME_SHORT - 1, main ? "ke%d" : "core%d", cpu);
	fill_prstatus(&mrdump_mini_ehdr->prstatus[id].data, regs, 0, id ? id : (100 + cpu));
	fill_note_S(&mrdump_mini_ehdr->prstatus[id].note, name, NT_PRSTATUS,
		    sizeof(struct elf_prstatus));
	return 0;
}

void mrdump_mini_per_cpu_regs(int cpu, struct pt_regs *regs)
{
	mrdump_mini_cpu_regs(cpu, regs, 0);
}
EXPORT_SYMBOL(mrdump_mini_per_cpu_regs);

static inline void ipanic_save_regs(struct pt_regs *regs)
{
#ifdef __aarch64__
	__asm__ volatile ("stp	x0, x1, [sp,#-16]!\n\t"
			  "1: mov	x1, %0\n\t"
			  "add	x0, x1, #16\n\t"
			  "stp	x2, x3, [x0],#16\n\t"
			  "stp	x4, x5, [x0],#16\n\t"
			  "stp	x6, x7, [x0],#16\n\t"
			  "stp	x8, x9, [x0],#16\n\t"
			  "stp	x10, x11, [x0],#16\n\t"
			  "stp	x12, x13, [x0],#16\n\t"
			  "stp	x14, x15, [x0],#16\n\t"
			  "stp	x16, x17, [x0],#16\n\t"
			  "stp	x18, x19, [x0],#16\n\t"
			  "stp	x20, x21, [x0],#16\n\t"
			  "stp	x22, x23, [x0],#16\n\t"
			  "stp	x24, x25, [x0],#16\n\t"
			  "stp	x26, x27, [x0],#16\n\t"
			  "ldr	x1, [x29]\n\t"
			  "stp	x28, x1, [x0],#16\n\t"
			  "mov	x1, sp\n\t"
			  "stp	x30, x1, [x0],#16\n\t"
			  "mrs	x1, daif\n\t"
			  "adr	x30, 1b\n\t"
			  "stp	x30, x1, [x0],#16\n\t"
			  "sub	x1, x0, #272\n\t"
			  "ldr	x0, [sp]\n\t"
			  "str	x0, [x1]\n\t"
			  "ldr	x0, [sp, #8]\n\t"
			  "str	x0, [x1, #8]\n\t" "ldp	x0, x1, [sp],#16\n\t" :  : "r" (regs) : "cc");
#else
	asm volatile ("stmia %1, {r0 - r15}\n\t" "mrs %0, cpsr\n":"=r" (regs->uregs[16]) : "r"(regs) : "memory");
#endif
}

void mrdump_mini_build_task_info(struct pt_regs *regs)
{
#define MAX_STACK_TRACE_DEPTH 32
	unsigned long ipanic_stack_entries[MAX_STACK_TRACE_DEPTH];
	char symbol[96];
	int sz;
	int off, plen;
	struct stack_trace trace;
	int i;
	struct task_struct *tsk, *cur;
	struct aee_process_info *cur_proc;

	if (!virt_addr_valid(current_thread_info())) {
		LOGE("current thread info invalid\n");
		return;
	}
	cur = current_thread_info()->task;
	tsk = cur;
	if (!virt_addr_valid(tsk)) {
		LOGE("tsk invalid\n");
		return;
	}
	cur_proc = (struct aee_process_info *)((void *)mrdump_mini_ehdr + MRDUMP_MINI_HEADER_SIZE);
	/* Current panic user tasks */
	sz = 0;
	do {
		if (!tsk) {
			LOGE("No tsk info\n");
			memset_io(cur_proc, 0x0, sizeof(struct aee_process_info));
			break;
		}
		/* FIXME: Check overflow ? */
		sz += snprintf(symbol + sz, 96 - sz, "[%s, %d]", tsk->comm, tsk->pid);
		tsk = tsk->real_parent;
	} while (tsk && (tsk->pid != 0) && (tsk->pid != 1));
	if (strncmp(cur_proc->process_path, symbol, sz) == 0) {
		LOGE("same process path\n");
		return;
	}

	memset_io(cur_proc, 0, sizeof(struct aee_process_info));
	memcpy(cur_proc->process_path, symbol, sz);

	/* Grab kernel task stack trace */
	trace.nr_entries = 0;
	trace.max_entries = MAX_STACK_TRACE_DEPTH;
	trace.entries = ipanic_stack_entries;
	trace.skip = 8;
	save_stack_trace_tsk(cur, &trace);
	/* Skip the entries -  ipanic_save_current_tsk_info/save_stack_trace_tsk */
	for (i = 0; i < trace.nr_entries; i++) {
		off = strlen(cur_proc->backtrace);
		plen = AEE_BACKTRACE_LENGTH - ALIGN(off, 8);
		if (plen > 16) {
			sz = snprintf(symbol, 96, "[<%p>] %pS\n",
				      (void *)ipanic_stack_entries[i],
				      (void *)ipanic_stack_entries[i]);
			if (ALIGN(sz, 8) - sz) {
				memset_io(symbol + sz - 1, ' ', ALIGN(sz, 8) - sz);
				memset_io(symbol + ALIGN(sz, 8) - 1, '\n', 1);
			}
			if (ALIGN(sz, 8) <= plen)
				memcpy(cur_proc->backtrace + ALIGN(off, 8), symbol, ALIGN(sz, 8));
		}
	}
	if (regs) {
		cur_proc->ke_frame.pc = (__u64) regs->reg_pc;
		cur_proc->ke_frame.lr = (__u64) regs->reg_lr;
	} else {
		/* in case panic() is called without die */
		/* Todo: a UT for this */
		cur_proc->ke_frame.pc = ipanic_stack_entries[0];
		cur_proc->ke_frame.lr = ipanic_stack_entries[1];
	}
	snprintf(cur_proc->ke_frame.pc_symbol, AEE_SZ_SYMBOL_S, "[<%p>] %pS",
		 (void *)(unsigned long)cur_proc->ke_frame.pc,
		 (void *)(unsigned long)cur_proc->ke_frame.pc);
	snprintf(cur_proc->ke_frame.lr_symbol, AEE_SZ_SYMBOL_L, "[<%p>] %pS",
		 (void *)(unsigned long)cur_proc->ke_frame.lr,
		 (void *)(unsigned long)cur_proc->ke_frame.lr);
}

int mrdump_task_info(unsigned char *buffer, size_t sz_buf)
{
	if (sz_buf < sizeof(struct aee_process_info))
		return -1;
	memcpy(buffer, (void *)mrdump_mini_ehdr + MRDUMP_MINI_HEADER_SIZE,
	       sizeof(struct aee_process_info));
	return sizeof(struct aee_process_info);
}

static void mrdump_mini_add_loads(void);
void mrdump_mini_ke_cpu_regs(struct pt_regs *regs)
{
	int cpu;
	struct pt_regs context;

	if (!regs) {
		regs = &context;
		ipanic_save_regs(regs);
	}
	cpu = get_HW_cpuid();
	mrdump_mini_cpu_regs(cpu, regs, 1);
	mrdump_mini_add_loads();
	mrdump_mini_build_task_info(regs);
}
EXPORT_SYMBOL(mrdump_mini_ke_cpu_regs);

static void mrdump_mini_build_elf_misc(void)
{
	int i;
	struct mrdump_mini_elf_misc misc;
	char log_type[][16] = { "_MAIN_LOG_", "_EVENTS_LOG_", "_RADIO_LOG_", "_SYSTEM_LOG_" };
	unsigned long task_info_va =
	    (unsigned long)((void *)mrdump_mini_ehdr + MRDUMP_MINI_HEADER_SIZE);
	unsigned long task_info_pa =
	    MRDUMP_MINI_BUF_PADDR ? (MRDUMP_MINI_BUF_PADDR +
				     MRDUMP_MINI_HEADER_SIZE) : __pa(task_info_va);
	mrdump_mini_add_misc_pa(task_info_va, task_info_pa, sizeof(struct aee_process_info), 0,
				"PROC_CUR_TSK");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	get_kernel_log_buffer(&misc.vaddr, &misc.size, &misc.start);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_KERNEL_LOG_");
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
	for (i = 0; i < 4; i++) {
		memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
		get_android_log_buffer(&misc.vaddr, &misc.size, &misc.start, i + 1);
		mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, log_type[i]);
	}
}

static void mrdump_mini_add_loads(void)
{
	int cpu, i, id;
	struct pt_regs regs;
	struct elf_prstatus *prstatus;
	struct task_struct *tsk = NULL;
	struct thread_info *ti = NULL;

	if (mrdump_mini_ehdr == NULL)
		return;
	for (id = 0; id < NR_CPUS + 1; id++) {
		if (!strncmp(mrdump_mini_ehdr->prstatus[id].name, "NA", 2))
			continue;
		prstatus = &mrdump_mini_ehdr->prstatus[id].data;
		memcpy(&regs, &prstatus->pr_reg, sizeof(prstatus->pr_reg));
		if (prstatus->pr_pid >= 100) {
			for (i = 0; i < ELF_NGREG; i++)
				mrdump_mini_add_entry(((unsigned long *)&regs)[i],
						      MRDUMP_MINI_SECTION_SIZE);
			cpu = prstatus->pr_pid - 100;
			mrdump_mini_add_tsk_ti(cpu, &regs, 1);
			mrdump_mini_add_entry((unsigned long)cpu_rq(cpu), MRDUMP_MINI_SECTION_SIZE);
		} else if (prstatus->pr_pid <= NR_CPUS) {
			cpu = prstatus->pr_pid - 1;
			mrdump_mini_add_tsk_ti(cpu, &regs, 0);
			for (i = 0; i < ELF_NGREG; i++) {
				mrdump_mini_add_entry(((unsigned long *)&regs)[i],
					MRDUMP_MINI_SECTION_SIZE);
			}
		} else {
			LOGE("mrdump: wrong pr_pid: %d\n", prstatus->pr_pid);
		}
	}

	mrdump_mini_add_entry((unsigned long)__per_cpu_offset, MRDUMP_MINI_SECTION_SIZE);
	mrdump_mini_add_entry((unsigned long)&mem_map, MRDUMP_MINI_SECTION_SIZE);
	mrdump_mini_add_entry((unsigned long)mem_map, MRDUMP_MINI_SECTION_SIZE);
	if (dump_all_cpus) {
		for (cpu = 0; cpu < NR_CPUS; cpu++) {
			tsk = cpu_curr(cpu);
			if (virt_addr_valid(tsk))
				ti = (struct thread_info *)tsk->stack;
			else
				ti = NULL;
			mrdump_mini_add_entry((unsigned long)cpu_rq(cpu), MRDUMP_MINI_SECTION_SIZE);
			mrdump_mini_add_entry((unsigned long)tsk, MRDUMP_MINI_SECTION_SIZE);
			mrdump_mini_add_entry((unsigned long)ti, MRDUMP_MINI_SECTION_SIZE);
		}
	}
#if 0
	if (logbuf_lock.owner_cpu < NR_CPUS) {
		tsk = cpu_curr(logbuf_lock.owner_cpu);
		if (virt_addr_valid(tsk))
			ti = (struct thread_info *)tsk->stack;
		else
			ti = NULL;
		mrdump_mini_add_entry((unsigned long)tsk, MRDUMP_MINI_SECTION_SIZE);
		mrdump_mini_add_entry((unsigned long)ti, MRDUMP_MINI_SECTION_SIZE);
	}

	mrdump_mini_add_entry((unsigned long)stack_trace, 256 * 1024);
#endif
}

static void mrdump_mini_dump_loads(loff_t offset, mrdump_write write)
{
	int errno;
	unsigned long start, size;
	int i;
	struct elf_phdr *phdr;
	loff_t pos = MRDUMP_MINI_HEADER_SIZE;

	for (i = 0; i < MRDUMP_MINI_NR_SECTION; i++) {
		phdr = &mrdump_mini_ehdr->phdrs[i];
		if (phdr->p_type == PT_NULL)
			break;
		if (phdr->p_type == PT_LOAD) {
			/* mrdump_mini_dump_phdr(phdr, &pos); */
			start = phdr->p_vaddr;
			size = ALIGN(phdr->p_filesz, SZ_512);
			phdr->p_offset = pos;
			errno = write((void *)start, pos + offset, size, 1);
			pos += size;
			if (IS_ERR(ERR_PTR(errno)))
				LOGD("mirdump: write fail");
		}
	}
}

int mrdump_mini_create_oops_dump(AEE_REBOOT_MODE reboot_mode, mrdump_write write,
				 loff_t sd_offset, const char *msg, va_list ap)
{
	mrdump_mini_dump_loads(sd_offset, write);
	write((void *)mrdump_mini_ehdr, sd_offset, MRDUMP_MINI_HEADER_SIZE, 1);
	return MRDUMP_MINI_BUF_SIZE;
}
EXPORT_SYMBOL(mrdump_mini_create_oops_dump);

void mrdump_mini_ipanic_done(void)
{
	mrdump_mini_ehdr->ehdr.e_ident[0] = 0;
}

static void __init *remap_lowmem(phys_addr_t start, phys_addr_t size)
{
	struct page **pages;
	phys_addr_t page_start;
	unsigned int page_count;
	pgprot_t prot;
	unsigned int i;
	void *vaddr;

	page_start = start - offset_in_page(start);
	page_count = DIV_ROUND_UP(size + offset_in_page(start), PAGE_SIZE);

	prot = pgprot_noncached(PAGE_KERNEL);

	pages = kmalloc_array(page_count, sizeof(struct page *), GFP_KERNEL);
	if (!pages) {
		LOGE("%s: Failed to allocate array for %u pages\n", __func__, page_count);
		return NULL;
	}

	for (i = 0; i < page_count; i++) {
		phys_addr_t addr = page_start + i * PAGE_SIZE;

		pages[i] = pfn_to_page(addr >> PAGE_SHIFT);
	}
	vaddr = vmap(pages, page_count, VM_MAP, prot);
	kfree(pages);
	if (!vaddr) {
		LOGE("%s: Failed to map %u pages\n", __func__, page_count);
		return NULL;
	}

	return vaddr + offset_in_page(start);
}

#define TASK_INFO_SIZE PAGE_SIZE
#define PSTORE_SIZE 0x8000
static void __init mrdump_mini_elf_header_init(void)
{
	if (MRDUMP_MINI_BUF_PADDR)
		mrdump_mini_ehdr =
		    remap_lowmem(MRDUMP_MINI_BUF_PADDR,
				 MRDUMP_MINI_HEADER_SIZE + TASK_INFO_SIZE + PSTORE_SIZE);
	else
		mrdump_mini_ehdr = kmalloc(MRDUMP_MINI_HEADER_SIZE, GFP_KERNEL);
	if (mrdump_mini_ehdr == NULL) {
		LOGE("mrdump mini reserve buffer fail");
		return;
	}
	LOGE("mirdump: reserved %x+%lx->%p", MRDUMP_MINI_BUF_PADDR,
	     (unsigned long)MRDUMP_MINI_HEADER_SIZE, mrdump_mini_ehdr);
	memset_io(mrdump_mini_ehdr, 0, MRDUMP_MINI_HEADER_SIZE + sizeof(struct aee_process_info));
	fill_elf_header(&mrdump_mini_ehdr->ehdr, MRDUMP_MINI_NR_SECTION);
}

int mrdump_mini_init(void)
{
	int i;
	unsigned long size, offset;
	struct pt_regs regs;

	mrdump_mini_elf_header_init();

	fill_psinfo(&mrdump_mini_ehdr->psinfo.data);
	fill_note_S(&mrdump_mini_ehdr->psinfo.note, "vmlinux", NT_PRPSINFO,
		    sizeof(struct elf_prpsinfo));

	memset_io(&regs, 0, sizeof(struct pt_regs));
	for (i = 0; i < NR_CPUS + 1; i++) {
		fill_prstatus(&mrdump_mini_ehdr->prstatus[i].data, &regs, 0, i);
		fill_note_S(&mrdump_mini_ehdr->prstatus[i].note, "NA", NT_PRSTATUS,
			    sizeof(struct elf_prstatus));
	}

	offset = offsetof(struct mrdump_mini_elf_header, psinfo);
	size = sizeof(mrdump_mini_ehdr->psinfo) + sizeof(mrdump_mini_ehdr->prstatus);
	fill_elf_note_phdr(&mrdump_mini_ehdr->phdrs[0], size, offset);

	for (i = 0; i < MRDUMP_MINI_NR_MISC; i++)
		fill_note_L(&mrdump_mini_ehdr->misc[i].note, "NA", 0,
			    sizeof(struct mrdump_mini_elf_misc));
	mrdump_mini_build_elf_misc();
	fill_elf_note_phdr(&mrdump_mini_ehdr->phdrs[1], sizeof(mrdump_mini_ehdr->misc),
			   offsetof(struct mrdump_mini_elf_header, misc));

	return 0;
}

arch_initcall(mrdump_mini_init);

void mrdump_mini_reserve_memory(void)
{
	if (MRDUMP_MINI_BUF_PADDR)
		memblock_reserve(MRDUMP_MINI_BUF_PADDR,
				 MRDUMP_MINI_HEADER_SIZE + TASK_INFO_SIZE + PSTORE_SIZE);
}

int mini_rdump_reserve_memory(struct reserved_mem *rmem)
{
	pr_alert("[memblock]%s: 0x%llx - 0x%llx (0x%llx)\n", "mini_rdump",
		 (unsigned long long)rmem->base,
		 (unsigned long long)rmem->base + (unsigned long long)rmem->size,
		 (unsigned long long)rmem->size);
	return 0;
}

RESERVEDMEM_OF_DECLARE(reserve_memory_minirdump, "mediatek,minirdump",
		       mini_rdump_reserve_memory);

module_param(dump_all_cpus, bool, S_IRUGO | S_IWUSR);
