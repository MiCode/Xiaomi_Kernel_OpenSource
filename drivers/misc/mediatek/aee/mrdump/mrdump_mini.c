// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2016 MediaTek Inc.
 */

#include <linux/android_debug_symbols.h>
#include <linux/bug.h>
#include <linux/compiler.h>
#include <linux/elf.h>
#include <linux/highmem.h>
#include <linux/init.h>
#include <linux/kdebug.h>
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
#include <printk/printk_ringbuffer.h>
#include <sched/sched.h>
#include "mrdump_mini.h"
#include "mrdump_private.h"

static struct mrdump_mini_elf_header *mrdump_mini_ehdr;

#ifdef MODULE
#if !IS_ENABLED(CONFIG_ARM64)
/*
 * Build error of 32bit KO:
 * ERROR: modpost: "init_mm" [../mrdump/mrdump.ko] undefined!
 */
extern struct mm_struct init_mm __weak;
#endif
#endif


#ifdef CONFIG_MODULES
struct module_sect_attr {
	struct bin_attribute battr;
	unsigned long address;
};

struct module_sect_attrs {
	struct attribute_group grp;
	unsigned int nsections;
	struct module_sect_attr attrs[];
};

struct module_notes_attrs {
	struct kobject *dir;
	unsigned int notes;
	struct bin_attribute attrs[];
};

#define MAX_KO_NAME_LEN 40
#define MAX_KO_NUM 400
#define LEN_BUILD_ID 20
#define KO_INFO_VERSION "AEE01"

struct ko_info {
	char name[MAX_KO_NAME_LEN];
	u64 text_addr;
	u64 init_text_addr;
	u32 core_size;
	u32 init_size;
	u8 build_id[LEN_BUILD_ID];
} __packed __aligned(8);

struct ko_info_all {
	char version[8];
	struct ko_info ko_list[MAX_KO_NUM];
} __packed __aligned(8);

static struct ko_info_all *ko_infos;
static struct ko_info *ko_info_list;

static spinlock_t kolist_lock;

static void fill_ko_list(unsigned int idx, struct module *mod)
{
	unsigned long text_addr = 0;
	unsigned long init_addr = 0;
	const void *build_id;
	struct elf_note *note;
	int i, search_nm, build_id_sz = 0;

	if (idx >= MAX_KO_NUM)
		return;

	search_nm = 2;
	for (i = 0; i < mod->sect_attrs->nsections; i++) {
		if (!strcmp(mod->sect_attrs->attrs[i].battr.attr.name,
			    ".text")) {
			text_addr = mod->sect_attrs->attrs[i].address;
			search_nm--;
		} else if (!strcmp(mod->sect_attrs->attrs[i].battr.attr.name,
				   ".init.text")) {
			init_addr = mod->sect_attrs->attrs[i].address;
			search_nm--;
		}
		if (!search_nm)
			break;
	}

	for (i = 0; i < mod->notes_attrs->notes; i++) {
		if (!strcmp(mod->notes_attrs->attrs[i].attr.name,
			    ".note.gnu.build-id")) {
			note = mod->notes_attrs->attrs[i].private;
			build_id = (void *)round_up(((unsigned long)(note + 1)
						     + note->n_namesz), 4);
			build_id_sz = note->n_descsz;
			break;
		}
	}

	if(snprintf(ko_info_list[idx].name,
		    MAX_KO_NAME_LEN, "%s", mod->name) > 0) {
		ko_info_list[idx].text_addr = text_addr;
		ko_info_list[idx].init_text_addr = init_addr;
		ko_info_list[idx].core_size = mod->core_layout.size;
		ko_info_list[idx].init_size = mod->init_layout.size;
		if (build_id_sz && build_id_sz <= LEN_BUILD_ID)
			memcpy(ko_info_list[idx].build_id, build_id,
					build_id_sz);
	} else {
		memset(&ko_info_list[i], 0, sizeof(struct ko_info));
	}
}

void load_ko_addr_list(struct module *module)
{
	unsigned int i;
	unsigned long flags;

	if (!ko_info_list)
		return;

	spin_lock_irqsave(&kolist_lock, flags);
	for (i = 0; i < MAX_KO_NUM; i++) {
		if (!ko_info_list[i].text_addr)
			break;
		if (!strcmp(ko_info_list[i].name, module->name))
			break;
	}

	if (i >= MAX_KO_NUM) {
		spin_unlock_irqrestore(&kolist_lock, flags);
		pr_info("no spare room for new ko: %s", module->name);
		return;
	}

	fill_ko_list(i, module);
	spin_unlock_irqrestore(&kolist_lock, flags);
}

void unload_ko_addr_list(struct module *module)
{
	unsigned int i;
	unsigned long flags;

	if (!ko_info_list)
		return;

	spin_lock_irqsave(&kolist_lock, flags);
	for (i = 0; i < MAX_KO_NUM; i++)
		if (!strcmp(ko_info_list[i].name, module->name))
			break;

	if (i >= MAX_KO_NUM) {
		spin_unlock_irqrestore(&kolist_lock, flags);
		pr_info("un-recorded module: %s", module->name);
		return;
	}

	memset(&ko_info_list[i], 0, sizeof(struct ko_info));
	spin_unlock_irqrestore(&kolist_lock, flags);
}

void init_ko_addr_list_late(void)
{
	struct module *mod;
	struct list_head *p_modules = aee_get_modules();
	int start = 0;

	if (!ko_info_list)
		return;

	if (!p_modules) {
		pr_info("%s failed", __func__);
		return;
	}

	list_for_each_entry_rcu(mod, p_modules, list) {
		if (mod->state == MODULE_STATE_UNFORMED)
			continue;
		if (!start) {
			/* only update the early KOs */
			if (!strcmp(mod->name, "mrdump"))
				start = 1;
			continue;
		}
		load_ko_addr_list(mod);
	}
}
#endif

#define MIN_MARGIN PAGE_OFFSET

#ifdef __aarch64__
static unsigned long virt_2_pfn(unsigned long addr)
{
	u64 mpt = mrdump_get_mpt() + kimage_voffset;
	pgd_t *pgd = pgd_offset_pgd((pgd_t *)mpt, addr);
	pgd_t _pgd_val = {0};
	p4d_t *p4d, _p4d_val = {0};
	pud_t *pud, _pud_val = {0};
	pmd_t *pmd, _pmd_val = {0};
	pte_t *ptep, _pte_val = {0};
	unsigned long pfn = ~0UL;

	if (get_kernel_nofault(_pgd_val, pgd) || pgd_none(_pgd_val))
		goto OUT;
	p4d = p4d_offset(pgd, addr);
	if (get_kernel_nofault(_p4d_val, p4d) || p4d_none(_p4d_val))
		goto OUT;
	pud = pud_offset(p4d, addr);
	if (get_kernel_nofault(_pud_val, pud) || pud_none(_pud_val))
		goto OUT;
	if (pud_sect(_pud_val)) {
		pfn = pud_pfn(_pud_val) + ((addr&~PUD_MASK) >> PAGE_SHIFT);
	} else if (pud_table(_pud_val)) {
		pmd = pmd_offset(pud, addr);
		if (get_kernel_nofault(_pmd_val, pmd) || pmd_none(_pmd_val))
			goto OUT;
		if (pmd_sect(_pmd_val)) {
			pfn = pmd_pfn(_pmd_val) +
				((addr&~PMD_MASK) >> PAGE_SHIFT);
		} else if (pmd_table(_pmd_val)) {
			ptep = pte_offset_map(pmd, addr);
			if (get_kernel_nofault(_pte_val, ptep)
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
	pgd_t *pgd = pgd_offset_k(addr), _pgd_val = {0};
#ifdef CONFIG_ARM_LPAE
	pud_t *pud, _pud_val = {0};
#else
	pud_t *pud, _pud_val = {{0} };
#endif
	pmd_t *pmd, _pmd_val = 0;
	pte_t *ptep, _pte_val = 0;
	unsigned long pfn = ~0UL;

	if (get_kernel_nofault(_pgd_val, pgd) || pgd_none(_pgd_val))
		goto OUT;
	pud = pud_offset((p4d_t *)pgd, addr);
	if (get_kernel_nofault(_pud_val, pud) || pud_none(_pud_val))
		goto OUT;
	pmd = pmd_offset(pud, addr);
	if (get_kernel_nofault(_pmd_val, pmd) || pmd_none(_pmd_val))
		goto OUT;
	if (pmd_sect(_pmd_val)) {
		pfn = pmd_pfn(_pmd_val) + ((addr&~PMD_MASK) >> PAGE_SHIFT);
	} else if (pmd_table(_pmd_val)) {
		ptep = pte_offset_map(pmd, addr);
		if (get_kernel_nofault(_pte_val, ptep)
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

	strncpy(psinfo->pr_psargs, "proc_cmdline", ELF_PRARGSZ - 1);
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
			if (strncmp(name, MRDUMP_MINI_MISC_LOAD, 4) == 0)
				continue;
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
#ifdef __aarch64__
	addr = __tag_reset(addr);
#endif
	if (addr < MIN_MARGIN)
		return 0;

	return pfn_valid(virt_2_pfn(addr));
}

static void mrdump_mini_build_task_info(struct pt_regs *regs)
{
#define MAX_STACK_TRACE_DEPTH 64
	unsigned long ipanic_stack_entries[MAX_STACK_TRACE_DEPTH];
	char symbol[SZ_128] = {'\0'};
	int sz;
#ifdef CONFIG_STACKTRACE
	unsigned int nr_entries;
	int off, plen;
	int i;
#endif
	struct task_struct *tsk;
	struct task_struct *previous;
	struct aee_process_info *cur_proc;

	if (!mrdump_mini_ehdr) {
		pr_notice("mrdump: ehder invalid\n");
		return;
	}

	tsk = current;
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
		sz += snprintf(symbol + sz, SZ_128 - sz, "[%s, %d]", tsk->comm,
				tsk->pid);
		if (sz >= SZ_128) {
			sz = SZ_128;
			break;
		}
		previous = tsk;
		tsk = tsk->real_parent;
		if (!mrdump_virt_addr_valid(tsk)) {
			pr_notice("tsk(0x%lx) invalid (previous: [%s, %d])\n",
					tsk, previous->comm, previous->pid);
			break;
		}
	} while (tsk && (tsk->pid != 0) && (tsk->pid != 1));
	if (!strncmp(cur_proc->process_path, symbol, sz)) {
		pr_notice("same process path\n");
		return;
	}

	memset_io(cur_proc, 0, sizeof(struct aee_process_info));
	memcpy(cur_proc->process_path, symbol, sz);

	if (regs) {
		cur_proc->ke_frame.pc = (__u64) regs->reg_pc;
		cur_proc->ke_frame.lr = (__u64) regs->reg_lr;
	}
#ifdef CONFIG_STACKTRACE
	nr_entries = stack_trace_save(ipanic_stack_entries,
			ARRAY_SIZE(ipanic_stack_entries), 4);
	if (!regs) {
		/* in case panic() is called without die */
		/* Todo: a UT for this */
		cur_proc->ke_frame.pc = ipanic_stack_entries[0];
		cur_proc->ke_frame.lr = ipanic_stack_entries[1];
	}
	/* Skip the entries -
	 * ipanic_save_current_tsk_info/save_stack_trace_tsk
	 */
	for (i = 0; i < nr_entries; i++) {
		off = strlen(cur_proc->backtrace);
		plen = AEE_BACKTRACE_LENGTH - ALIGN(off, 8);
		if (plen > 16) {
			if (ipanic_stack_entries[i] != cur_proc->ke_frame.pc)
				ipanic_stack_entries[i] -= 4;
			sz = snprintf(symbol, SZ_128, "[<%px>] %pS\n",
				      (void *)ipanic_stack_entries[i],
				      (void *)ipanic_stack_entries[i]);
			if (sz >= SZ_128) {
				sz = SZ_128;
				memset_io(symbol + ALIGN(sz, 8) - 1, '\n', 1);
			}
			if (ALIGN(sz, 8) - sz) {
				memset_io(symbol + sz - 1, ' ',
						ALIGN(sz, 8) - sz);
				memset_io(symbol + ALIGN(sz, 8) - 1, '\n', 1);
			}
			if (ALIGN(sz, 8) < plen)
				memcpy(cur_proc->backtrace + ALIGN(off, 8),
						symbol, ALIGN(sz, 8));
		}
	}
#endif
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

/*
 * mrdump_mini_add_extra_file - add a file named SYS_#name#_RAW to KE DB
 * @vaddr:	start vaddr of target memory
 * @paddr:	start paddr of target memory
 * @size:	size of target memory
 * @name:	file name
 *
 * the size sould be no more than 512K, and the less the better.
 */
int mrdump_mini_add_extra_file(unsigned long vaddr, unsigned long paddr,
	unsigned long size, const char *name)
{
	char name_buf[SZ_128] = {0};

	if (!name) {
		pr_info("mrdump: invalid file name\n");
		return -1;
	}
	if (!mrdump_mini_ehdr) {
		pr_info("mrdump: failed to add %s, aee not ready\n", name);
		return -1;
	}
	if (!size) {
		pr_info("mrdump: failed to add %s, invalid size\n", name);
		return -1;
	}

	if (size > SZ_512K)
		pr_warn("mrdump: file size of %s is too large 0x%lx\n",
			name, size);

	snprintf(name_buf, SZ_128, "_%s_", name);
	mrdump_mini_add_misc_pa(vaddr, paddr, size, 0, name_buf);
	return 0;
}
EXPORT_SYMBOL(mrdump_mini_add_extra_file);

typedef void (*dump_func_t)(unsigned long *vaddr, unsigned long *size);
dump_func_t p_ufs_mtk_dbg_get_aee_buffer;
dump_func_t p_mmc_mtk_dbg_get_aee_buffer;
dump_func_t p_mtk_btag_get_aee_buffer;
dump_func_t p_mtk_adsp_get_aee_buffer;
dump_func_t p_mtk_ccu_get_aee_buffer;

void mrdump_set_extra_dump(enum AEE_EXTRA_FILE_ID id,
		void (*fn)(unsigned long *vaddr, unsigned long *size))
{
	if (!mrdump_mini_ehdr) {
		pr_notice("mrdump: ehdr invalid");
		return;
	}

	switch (id) {
	case AEE_EXTRA_FILE_UFS:
		p_ufs_mtk_dbg_get_aee_buffer = fn;
		break;
	case AEE_EXTRA_FILE_MMC:
		p_mmc_mtk_dbg_get_aee_buffer = fn;
		break;
	case AEE_EXTRA_FILE_BLOCKIO:
		p_mtk_btag_get_aee_buffer = fn;
		break;
	case AEE_EXTRA_FILE_ADSP:
		p_mtk_adsp_get_aee_buffer = fn;
		break;
	case AEE_EXTRA_FILE_CCU:
		p_mtk_ccu_get_aee_buffer = fn;
		break;
	default:
		pr_info("mrdump: unknown extra file id\n");
		break;
	}
}
EXPORT_SYMBOL(mrdump_set_extra_dump);

void mrdump_mini_add_extra_misc(void)
{
	unsigned long vaddr = 0;
	unsigned long size = 0;

	if (!mrdump_mini_ehdr) {
		pr_notice("mrdump: ehdr invalid");
		return;
	}

	if (p_ufs_mtk_dbg_get_aee_buffer) {
		p_ufs_mtk_dbg_get_aee_buffer(&vaddr, &size);
		mrdump_mini_add_extra_file(vaddr, __pa_nodebug(vaddr), size,
					   "EXTRA_UFS");
	}

	if (p_mmc_mtk_dbg_get_aee_buffer) {
		p_mmc_mtk_dbg_get_aee_buffer(&vaddr, &size);
		mrdump_mini_add_extra_file(vaddr, __pa_nodebug(vaddr), size,
					   "EXTRA_MMC");
	}

	if (p_mtk_btag_get_aee_buffer) {
		vaddr = 0;
		size = 0;
		p_mtk_btag_get_aee_buffer(&vaddr, &size);
		mrdump_mini_add_extra_file(vaddr, __pa_nodebug(vaddr), size,
					   "EXTRA_BLOCKIO");
	}

	if (p_mtk_adsp_get_aee_buffer) {
		vaddr = 0;
		size = 0;
		p_mtk_adsp_get_aee_buffer(&vaddr, &size);
		mrdump_mini_add_extra_file(vaddr, __pa_nodebug(vaddr), size,
					   "EXTRA_ADSP");
	}

	if (p_mtk_ccu_get_aee_buffer) {
		vaddr = 0;
		size = 0;
		p_mtk_ccu_get_aee_buffer(&vaddr, &size);
		mrdump_mini_add_extra_file(vaddr, __pa_nodebug(vaddr), size,
					   "EXTRA_CCU");
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

void mrdump_mini_add_klog(void)
{
	struct mrdump_mini_elf_misc misc;
	struct printk_ringbuffer **pprb;
	struct printk_ringbuffer *prb;
	unsigned int cnt;

	pprb = (struct printk_ringbuffer **)aee_log_buf_addr_get();
	if (!pprb || !*pprb)
		return;
	prb = *pprb;

	cnt = 1 << prb->desc_ring.count_bits;

	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	misc.vaddr = (unsigned long)prb->desc_ring.descs;
	misc.size = (unsigned long)(cnt * sizeof(struct prb_desc));
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start,
			     "_KERNEL_LOG_DESCS_");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	misc.vaddr = (unsigned long)prb->desc_ring.infos;
	misc.size = (unsigned long)(cnt * sizeof(struct printk_info));
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start,
			     "_KERNEL_LOG_INFOS_");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	misc.vaddr = (unsigned long)prb->text_data_ring.data;
	misc.size = (unsigned long)(1 << prb->text_data_ring.size_bits);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start,
			     "_KERNEL_LOG_DATA_");
	mrdump_mini_add_misc((unsigned long)prb,
			     sizeof(struct printk_ringbuffer),
			     0, "_KERNEL_LOG_TP_");
}

void mrdump_mini_add_kallsyms(void)
{
	unsigned long size, vaddr;

	vaddr = aee_get_kallsyms_addresses();
	vaddr = round_down(vaddr, PAGE_SIZE);
	size = aee_get_kti_addresses() - vaddr + 512;
	size = round_up(size, PAGE_SIZE);
	if (vaddr)
		mrdump_mini_add_misc_pa(vaddr, __pa_nodebug(vaddr),
				size, 0, MRDUMP_MINI_MISC_LOAD);
}

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
	/* could also use the kernel log in pstore for LKM case */
#ifndef MODULE
	mrdump_mini_add_klog();
#endif
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	get_mbootlog_buffer(&misc.vaddr, &misc.size, &misc.start);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_LAST_KMSG");
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	aee_rr_get_desc_info(&misc.vaddr, &misc.size, &misc.start);
	mrdump_mini_add_misc(misc.vaddr, misc.size, misc.start, "_RR_DESC_");
#ifdef CONFIG_MODULES
	spin_lock_init(&kolist_lock);
	ko_infos = kzalloc(sizeof(struct ko_info_all), GFP_KERNEL);
	if (ko_infos) {
		ko_info_list = ko_infos->ko_list;
		strlcpy(ko_infos->version, KO_INFO_VERSION,
		       sizeof(ko_infos->version));
		mrdump_mini_add_misc_pa((unsigned long)ko_infos,
				(unsigned long)__pa_nodebug(
						(unsigned long)ko_infos),
				sizeof(struct ko_info_all),
				0, "_MODULES_INFO_");
	}
#endif
	memset_io(&misc, 0, sizeof(struct mrdump_mini_elf_misc));
	misc.vaddr = (unsigned long)android_debug_symbol(ADS_LINUX_BANNER);
	misc.size = strlen((char *)misc.vaddr);
	mrdump_mini_add_misc(misc.vaddr, misc.size, 0, "_VERSION_BR");
}

void mrdump_mini_add_hang_raw(unsigned long vaddr, unsigned long size)
{
	pr_notice("mrdump: hang data 0x%lx size:0x%lx\n", vaddr, size);
	if (!mrdump_mini_ehdr) {
		pr_notice("mrdump: ehdr invalid");
		return;
	}
	mrdump_mini_add_misc_pa(vaddr, __pa_nodebug(vaddr),
				size, 0, "_HANG_DETECT_");
}
EXPORT_SYMBOL(mrdump_mini_add_hang_raw);

void mrdump_mini_ke_cpu_regs(struct pt_regs *regs)
{
	mrdump_mini_build_task_info(regs);
}

void *remap_lowmem(phys_addr_t start, phys_addr_t size)
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

int __init mrdump_mini_init(const struct mrdump_params *mparams)
{
	int i, cpu;
	unsigned long size, offset, vaddr;
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
	for (i = 0; i < AEE_MTK_CPU_NUMS; i++) {
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
		mrdump_mini_add_misc_pa((unsigned long)mrdump_cblock,
				mparams->cb_addr, mparams->cb_size,
				0, MRDUMP_MINI_MISC_LOAD);
#ifndef MODULE
		mrdump_mini_add_kallsyms();
#endif
	}

	vaddr = round_down((unsigned long)__per_cpu_offset, PAGE_SIZE);
	mrdump_mini_add_misc_pa(vaddr, __pa_nodebug(vaddr),
			PAGE_SIZE * 2, 0, MRDUMP_MINI_MISC_LOAD);

	for (cpu = 0; cpu < nr_cpu_ids; cpu++) {
		vaddr = (unsigned long)cpu_rq(cpu);
		vaddr = round_down(vaddr, PAGE_SIZE);
		mrdump_mini_add_misc(vaddr, MRDUMP_MINI_SECTION_SIZE,
				0, MRDUMP_MINI_MISC_LOAD);
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
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
