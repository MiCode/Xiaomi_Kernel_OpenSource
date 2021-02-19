// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/percpu.h>
#include <linux/string.h>
#include "../../../../kernel/sched/sched.h"

#include <asm/memory.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/smp_plat.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>

#include <mt-plat/aee.h>
#include "mrdump_private.h"

#ifdef MODULE

#define KV		kimage_vaddr
#define S_MAX		SZ_64M
#define S_START_OFFSET	SZ_2M
#define SM_SIZE		28
#define TT_SIZE		256
#define NAME_LEN	128

static unsigned long *mrdump_ka;
static int *mrdump_ko;
static unsigned long *mrdump_krb;
static unsigned int *mrdump_kns;
static u8 *mrdump_kn;
static unsigned int *mrdump_km;
static u8 *mrdump_ktt;
static u16 *mrdump_kti;

static void *mrdump_abt_addr(void *ssa)
{
	void *pos;
	u8 abt[SM_SIZE];
	int i;
	unsigned long s_left;

	for (i = 0; i < SM_SIZE; i++) {
		if (i % 2)
			abt[i] = 0;
		else
			abt[i] = 0x61 + i / 2;
	}

	if ((unsigned long)ssa >= KV + S_MAX) {
		pr_info("out of range: 0x%lx\n", ssa);
		return NULL;
	}

	pos = ssa;
	s_left = KV + S_MAX - (unsigned long)ssa;
	while ((u64)pos < (u64)(KV + S_MAX)) {
		pos = memchr(pos, 'a', s_left);

		if (!pos) {
			pr_info("fail at: 0x%lx @ 0x%x\n", ssa, s_left);
			return NULL;
		}
		s_left = KV + S_MAX - (unsigned long)pos;

		if (!memcmp(pos, (const void *)abt, sizeof(abt)))
			return pos;

		pos += 1;
	}

	pr_info("fail at end: 0x%lx @ 0x%lx\n", ssa, s_left);
	return NULL;
}

static unsigned long *mrdump_krb_addr(void)
{
	void *abt_addr = (void *)KV;
	void *ssa = (void *)KV + S_START_OFFSET;
	unsigned long *pos;

	while ((u64)ssa < KV + S_MAX) {
		abt_addr = mrdump_abt_addr(ssa);
		if (!abt_addr) {
			pr_info("krb not found: 0x%lx\n", ssa);
			return NULL;
		}

		abt_addr = (void *)round_up((unsigned long)abt_addr, 8);
		for (pos = (unsigned long *)abt_addr;
		     (u64)pos > (u64)ssa ; pos--) {
			if ((u64)pos == (u64)&kimage_vaddr)
				break;
			if (*pos == KV)
				return pos;
		}
		ssa = abt_addr + 1;
	}

	pr_info("krb not found: 0x%lx\n", ssa);
	return NULL;
}

static unsigned int *mrdump_km_addr(void)
{
	const u8 *name = mrdump_kn;
	unsigned int loop = *mrdump_kns;

	while (loop--)
		name = name + (*name) + 1;

	return (unsigned int *)round_up((unsigned long)name, 8);
}

static u16 *mrdump_kti_addr(void)
{
	const u8 *pch = mrdump_ktt;
	int loop = TT_SIZE;

	while (loop--) {
		for (; *pch; pch++)
			;
		pch++;
	}

	return (u16 *)round_up((unsigned long)pch, 8);
}

static void aee_base_addrs_init(void);
int mrdump_ka_init(void)
{
	unsigned int kns;

	mrdump_krb = mrdump_krb_addr();
	if (!mrdump_krb)
		return -EINVAL;

	mrdump_kns = (unsigned int *)(mrdump_krb + 1);
	mrdump_kn = (u8 *)(mrdump_krb + 2);
	kns = *mrdump_kns;
	mrdump_ko = (int *)(mrdump_krb - (round_up(kns, 2) / 2));
	mrdump_km = mrdump_km_addr();
	mrdump_ktt = (u8 *)round_up((unsigned long)(mrdump_km +
				    (round_up(kns, 256) / 256)), 8);
	mrdump_kti = mrdump_kti_addr();

	aee_base_addrs_init();
	return 0;
}

static unsigned int mrdump_checking_names(unsigned int off,
					   char *namebuf, size_t buflen)
{
	int len, skipped_first = 0;
	const u8 *tptr, *data;

	data = mrdump_kn + off;
	len = *data;
	data++;
	off += len + 1;

	while (len) {
		tptr = mrdump_ktt + *(mrdump_kti + *data);
		data++;
		len--;

		while (*tptr) {
			if (skipped_first) {
				if (buflen <= 1)
					goto tail;
				*namebuf = *tptr;
				namebuf++;
				buflen--;
			} else
				skipped_first = 1;
			tptr++;
		}
	}

tail:
	if (buflen)
		*namebuf = '\0';

	return off;
}

static unsigned long mrdump_idx2addr(int idx)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return *(mrdump_ka + idx);

	if (!IS_ENABLED(CONFIG_KALLSYMS_ABSOLUTE_PERCPU))
		return *mrdump_krb + (u32)(*(mrdump_ko + idx));

	if (*(mrdump_ko + idx) >= 0)
		return *(mrdump_ko + idx);

	return *mrdump_krb - 1 - *(mrdump_ko + idx);
}

static unsigned long aee_addr_find(const char *name)
{
	char strbuf[NAME_LEN];
	unsigned long i;
	unsigned int off;

	for (i = 0, off = 0; i < *mrdump_kns; i++) {
		off = mrdump_checking_names(off, strbuf, ARRAY_SIZE(strbuf));

		if (strcmp(strbuf, name) == 0)
			return mrdump_idx2addr(i);
	}
	return 0;
}

/* for mrdump.ko */
#ifdef CONFIG_ARM64
static void print_pstate(struct pt_regs *regs)
{
	u64 pstate = regs->pstate;

	if (compat_user_mode(regs)) {
		pr_info("pstate: %08llx (%c%c%c%c %c %s %s %c%c%c)\n",
			pstate,
			pstate & PSR_AA32_N_BIT ? 'N' : 'n',
			pstate & PSR_AA32_Z_BIT ? 'Z' : 'z',
			pstate & PSR_AA32_C_BIT ? 'C' : 'c',
			pstate & PSR_AA32_V_BIT ? 'V' : 'v',
			pstate & PSR_AA32_Q_BIT ? 'Q' : 'q',
			pstate & PSR_AA32_T_BIT ? "T32" : "A32",
			pstate & PSR_AA32_E_BIT ? "BE" : "LE",
			pstate & PSR_AA32_A_BIT ? 'A' : 'a',
			pstate & PSR_AA32_I_BIT ? 'I' : 'i',
			pstate & PSR_AA32_F_BIT ? 'F' : 'f');
	} else {
		pr_info("pstate: %08llx (%c%c%c%c %c%c%c%c %cPAN %cUAO)\n",
			pstate,
			pstate & PSR_N_BIT ? 'N' : 'n',
			pstate & PSR_Z_BIT ? 'Z' : 'z',
			pstate & PSR_C_BIT ? 'C' : 'c',
			pstate & PSR_V_BIT ? 'V' : 'v',
			pstate & PSR_D_BIT ? 'D' : 'd',
			pstate & PSR_A_BIT ? 'A' : 'a',
			pstate & PSR_I_BIT ? 'I' : 'i',
			pstate & PSR_F_BIT ? 'F' : 'f',
			pstate & PSR_PAN_BIT ? '+' : '-',
			pstate & PSR_UAO_BIT ? '+' : '-');
	}
}

void aee_show_regs(struct pt_regs *regs)
{
	int i, top_reg;
	u64 lr, sp;

	if (compat_user_mode(regs)) {
		lr = regs->compat_lr;
		sp = regs->compat_sp;
		top_reg = 12;
	} else {
		lr = regs->regs[30];
		sp = regs->sp;
		top_reg = 29;
	}

	print_pstate(regs);

	if (!user_mode(regs)) {
		pr_info("pc : %pS\n", (void *)regs->pc);
		pr_info("lr : %pS\n", (void *)lr);
	} else {
		pr_info("pc : %016llx\n", regs->pc);
		pr_info("lr : %016llx\n", lr);
	}

	pr_info("sp : %016llx\n", sp);

	if (system_uses_irq_prio_masking())
		pr_info("pmr_save: %08llx\n", regs->pmr_save);

	i = top_reg;

	while (i >= 0) {
		pr_info("x%-2d: %016llx ", i, regs->regs[i]);
		i--;

		if (i % 2 == 0) {
			pr_cont("x%-2d: %016llx ", i, regs->regs[i]);
			i--;
		}

		pr_cont("\n");
	}
}
#else
void aee_show_regs(struct pt_regs *regs)
{
	pr_info("PC is at %pS\n", (void *)instruction_pointer(regs));
	pr_info("LR is at %pS\n", (void *)regs->ARM_lr);
	pr_info("pc : [<%08lx>]    lr : [<%08lx>]    psr: %08lx\n",
		regs->ARM_pc, regs->ARM_lr, regs->ARM_cpsr);
	pr_info("sp : %08lx  ip : %08lx  fp : %08lx\n",
		regs->ARM_sp, regs->ARM_ip, regs->ARM_fp);
	pr_info("r10: %08lx  r9 : %08lx  r8 : %08lx\n",
		regs->ARM_r10, regs->ARM_r9,
		regs->ARM_r8);
	pr_info("r7 : %08lx  r6 : %08lx  r5 : %08lx  r4 : %08lx\n",
		regs->ARM_r7, regs->ARM_r6,
		regs->ARM_r5, regs->ARM_r4);
	pr_info("r3 : %08lx  r2 : %08lx  r1 : %08lx  r0 : %08lx\n",
		regs->ARM_r3, regs->ARM_r2,
		regs->ARM_r1, regs->ARM_r0);
}
#endif

static unsigned long p_stext;
unsigned long aee_get_stext(void)
{
	if (p_stext)
		return p_stext;

	p_stext = aee_addr_find("_stext");

	if (!p_stext)
		pr_info("%s failed", __func__);
	return p_stext;
}

static unsigned long p_etext;
unsigned long aee_get_etext(void)
{
	if (p_etext)
		return p_etext;

	p_etext = aee_addr_find("_etext");

	if (!p_etext)
		pr_info("%s failed", __func__);
	return p_etext;
}

static unsigned long p_text;
unsigned long aee_get_text(void)
{
	if (p_text)
		return p_text;

	p_text = aee_addr_find("_text");

	if (!p_text)
		pr_info("%s failed", __func__);
	return p_text;
}

/* _sdata, _edata is in *ABS* section and kallsyms API can not find it */
unsigned long aee_get_sdata(void)
{
	return 0;
}

unsigned long aee_get_edata(void)
{
	return 0;
}

#ifdef CONFIG_SYSFS
static struct kset **p_module_kset;
struct kset *aee_get_module_kset(void)
{
	if (p_module_kset)
		return *p_module_kset;

	p_module_kset = (void *)(aee_addr_find("module_kset"));

	if (!p_module_kset) {
		pr_info("%s failed", __func__);
		return NULL;
	}
	return *p_module_kset;
}
#endif

static struct memblock *p_memblock;
static struct memblock *aee_memblock(void)
{
	if (p_memblock)
		return p_memblock;

	p_memblock = (void *)(aee_addr_find("memblock"));

	if (!p_memblock) {
		pr_info("%s failed", __func__);
		return NULL;
	}
	return p_memblock;
}

phys_addr_t aee_memblock_start_of_DRAM(void)
{
	struct memblock *memblockp = aee_memblock();

	if (!memblockp) {
		pr_info("%s failed", __func__);
		return 0;
	}

	return memblockp->memory.regions[0].base;
}

phys_addr_t aee_memblock_end_of_DRAM(void)
{
	struct memblock *memblockp = aee_memblock();
	int idx;

	if (!memblockp) {
		pr_info("%s failed", __func__);
		return 0;
	}

	idx = memblockp->memory.cnt - 1;

	return (memblockp->memory.regions[idx].base +
		memblockp->memory.regions[idx].size);
}

#ifdef CONFIG_MODULES
static struct list_head *p_modules;
struct list_head *aee_get_modules(void)
{

	if (p_modules)
		return p_modules;

	p_modules = (void *)aee_addr_find("modules");

	if (!p_modules) {
		pr_info("%s failed", __func__);
		return NULL;
	}

	return p_modules;
}
#endif

static phys_addr_t *p__cpu_logical_map;
static phys_addr_t *aee_cpu_logical_map(void)
{
	if (p__cpu_logical_map)
		return p__cpu_logical_map;

	p__cpu_logical_map = (void *)(aee_addr_find("__cpu_logical_map"));

	if (p__cpu_logical_map)
		return p__cpu_logical_map;

	return NULL;
}

int get_HW_cpuid(void)
{
	u64 mpidr;
	int cpu;

	if (!aee_cpu_logical_map())
		return -EINVAL;

	mpidr = read_cpuid_mpidr();
	/*
	 * NOTICE: the logic of following code sould be the same with
	 * get_logical_index() of linux kenrel
	 */
	mpidr &= MPIDR_HWID_BITMASK;

	for (cpu = 0; cpu < nr_cpu_ids; cpu++)
		if (p__cpu_logical_map[cpu] == mpidr)
			return cpu;
	return -EINVAL;
}
EXPORT_SYMBOL(get_HW_cpuid);

u32 aee_log_buf_len_get(void)
{
	u32 log_buf_len = 1 << CONFIG_LOG_BUF_SHIFT;

	if (log_buf_len > 0 && log_buf_len <= (1 << 25))
		return log_buf_len;

	return 0;
}

static char *p__log_buf;
char *aee_log_buf_addr_get(void)
{
	if (p__log_buf)
		return p__log_buf;

	p__log_buf = (void *)(aee_addr_find("__log_buf"));

	if (p__log_buf)
		return p__log_buf;

	pr_info("%s failed", __func__);
	return NULL;
}
EXPORT_SYMBOL(aee_log_buf_addr_get);

static struct mm_struct *p_init_mm;
static struct mm_struct *aee_init_mm(void)
{
	if (p_init_mm)
		return p_init_mm;

	p_init_mm = (void *)(aee_addr_find("init_mm"));

	if (!p_init_mm) {
		pr_info("%s failed", __func__);
		return NULL;
	}

	return p_init_mm;
}

pgd_t *aee_pgd_offset_k(unsigned long addr)
{
	struct mm_struct *pim = aee_init_mm();

	if (!pim)
		return NULL;
	return (pgd_t *)pgd_offset(pim, addr);
}

unsigned long aee_get_kallsyms_addresses(void)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return (unsigned long)mrdump_ka;
	return (unsigned long)mrdump_ko;
}

unsigned long aee_get_kti_addresses(void)
{
	return (unsigned long)mrdump_kti;
}

raw_spinlock_t *p_logbuf_lock;
struct semaphore *p_console_sem;
void aee_zap_locks(void)
{
	if (!p_logbuf_lock) {
		p_logbuf_lock =
			(void *)(aee_addr_find("logbuf_lock"));

		if (!p_logbuf_lock) {
			aee_sram_printk("%s failed to get logbuf lock\n",
					__func__);
			return;
		}
	}
	if (!p_console_sem) {
		p_console_sem =
			(void *)(aee_addr_find("console_sem"));

		if (!p_console_sem) {
			aee_sram_printk("%s failed to get console_sem\n",
					__func__);
			return;
		}
	}
	debug_locks_off();
	/* If a crash is occurring, make sure we can't deadlock */
	raw_spin_lock_init(p_logbuf_lock);
	/* And make sure that we print immediately */
	sema_init(p_console_sem, 1);
}

static raw_spinlock_t *p_die_lock;
void aee_reinit_die_lock(void)
{
	if (!p_die_lock) {
		p_die_lock = (void *)aee_addr_find("die_lock");
		if (!p_die_lock) {
			aee_sram_printk("%s failed to get die_lock\n",
					__func__);
			return;
		}
	}

	/* If a crash is occurring, make sure we can't deadlock */
	raw_spin_lock_init(p_die_lock);
}

/* for aee_aed.ko */
#ifdef __aarch64__
const char *aee_arch_vma_name(struct vm_area_struct *vma)
{
	return NULL;
}
#else
#ifdef CONFIG_KUSER_HELPERS
static struct vm_area_struct *p_gate_vma;
static struct vm_area_struct *aee_get_gate_vma(void)
{
	if (p_gate_vma)
		return p_gate_vma;

	p_gate_vma = (void *)(aee_addr_find("gate_vma"));

	if (!p_gate_vma) {
		pr_info("%s failed", __func__);
		return NULL;
	}
	return p_gate_vma;
}

const char *aee_arch_vma_name(struct vm_area_struct *vma)
{
	struct vm_area_struct *gate_vma_p = aee_get_gate_vma();

	return vma == gate_vma_p ? "[vectors]" : NULL;
}
#else /* #ifdef CONFIG_KUSER_HELPERS */
const char *aee_arch_vma_name(struct vm_area_struct *vma)
{
	return NULL;
}
#endif
#endif
EXPORT_SYMBOL(aee_arch_vma_name);

/* find the addrs needed during driver init stage */
static void aee_base_addrs_init(void)
{
	char strbuf[NAME_LEN];
	unsigned long i;
	unsigned int off;
	unsigned int search_num = 8;

#ifndef CONFIG_SYSFS
	search_num--;
#endif
#ifndef CONFIG_MODULES
	search_num--;
#endif

	for (i = 0, off = 0; i < *mrdump_kns; i++) {
		if (!search_num)
			return;
		off = mrdump_checking_names(off, strbuf, ARRAY_SIZE(strbuf));

#ifdef CONFIG_SYSFS
		if (!p_module_kset && strcmp(strbuf, "module_kset") == 0) {
			p_module_kset = (void *)mrdump_idx2addr(i);
			search_num--;
			continue;
		}
#endif
#ifdef CONFIG_MODULES
		if (!p_modules && strcmp(strbuf, "modules") == 0) {
			p_modules = (void *)mrdump_idx2addr(i);
			search_num--;
			continue;
		}
#endif
		if (!p_memblock && strcmp(strbuf, "memblock") == 0) {
			p_memblock = (void *)mrdump_idx2addr(i);
			search_num--;
			continue;
		}

		if (!p_etext && strcmp(strbuf, "_etext") == 0) {
			p_etext = mrdump_idx2addr(i);
			search_num--;
			continue;
		}

		if (!p_stext && strcmp(strbuf, "_stext") == 0) {
			p_stext = mrdump_idx2addr(i);
			search_num--;
			continue;
		}

		if (!p_text && strcmp(strbuf, "_text") == 0) {
			p_text = mrdump_idx2addr(i);
			search_num--;
			continue;
		}

		if (!p_init_mm && strcmp(strbuf, "init_mm") == 0) {
			p_init_mm = (void *)mrdump_idx2addr(i);
			search_num--;
			continue;
		}

		if (!p__log_buf && strcmp(strbuf, "__log_buf") == 0) {
			p__log_buf = (void *)mrdump_idx2addr(i);
			search_num--;
			continue;
		}
	}
	if (search_num)
		pr_info("mrdump addr init incomplete %d\n", search_num);
}

#else /* #ifdef MODULE*/
/* for mrdump.ko */
void aee_show_regs(struct pt_regs *regs)
{
	__show_regs(regs);
}

unsigned long aee_get_stext(void)
{
	return (unsigned long)_stext;
}

unsigned long aee_get_etext(void)
{
	return (unsigned long)_etext;
}

unsigned long aee_get_text(void)
{
	return (unsigned long)_text;
}

unsigned long aee_get_sdata(void)
{
	return (unsigned long)_sdata;
}

unsigned long aee_get_edata(void)
{
	return (unsigned long)_edata;
}

#ifdef CONFIG_SYSFS
struct kset *aee_get_module_kset(void)
{
	return module_kset;
}
#endif

phys_addr_t aee_memblock_start_of_DRAM(void)
{
	return memblock_start_of_DRAM();
}

phys_addr_t aee_memblock_end_of_DRAM(void)
{
	return memblock_end_of_DRAM();
}

#ifdef CONFIG_MODULES
static struct list_head *p_modules;
struct list_head *aee_get_modules(void)
{

	if (p_modules)
		return p_modules;

	p_modules = (void *)kallsyms_lookup_name("modules");

	if (!p_modules) {
		pr_info("%s failed", __func__);
		return NULL;
	}

	return p_modules;
}
#endif

int get_HW_cpuid(void)
{
	u64 mpidr;

	mpidr = read_cpuid_mpidr();
	return get_logical_index(mpidr & MPIDR_HWID_BITMASK);
}

u32 aee_log_buf_len_get(void)
{
	return log_buf_len_get();
}

char *aee_log_buf_addr_get(void)
{
	return log_buf_addr_get();
}

pgd_t *aee_pgd_offset_k(unsigned long addr)
{
	return (pgd_t *)pgd_offset_k(addr);
}

unsigned long aee_get_kallsyms_addresses(void)
{
	if (!IS_ENABLED(CONFIG_KALLSYMS_BASE_RELATIVE))
		return (unsigned long)kallsyms_addresses;
	return (unsigned long)kallsyms_offsets;
}

unsigned long aee_get_kti_addresses(void)
{
	return (unsigned long)kallsyms_token_index;
}

raw_spinlock_t *p_logbuf_lock;
struct semaphore *p_console_sem;
void aee_zap_locks(void)
{
	if (!p_logbuf_lock) {
		p_logbuf_lock = (void *)kallsyms_lookup_name("logbuf_lock");
		if (!p_logbuf_lock) {
			aee_sram_printk("%s failed to get logbuf lock\n",
					__func__);
			return;
		}
	}
	if (!p_console_sem) {
		p_console_sem = (void *)kallsyms_lookup_name("console_sem");
		if (!p_console_sem) {
			aee_sram_printk("%s failed to get console_sem\n",
					__func__);
			return;
		}
	}

	debug_locks_off();
	/* If a crash is occurring, make sure we can't deadlock */
	raw_spin_lock_init(p_logbuf_lock);
	/* And make sure that we print immediately */
	sema_init(p_console_sem, 1);
}


static raw_spinlock_t *p_die_lock;
void aee_reinit_die_lock(void)
{
	if (!p_die_lock) {
		p_die_lock = (void *)kallsyms_lookup_name("die_lock");
		if (!p_die_lock) {
			aee_sram_printk("%s failed to get die_lock\n",
					__func__);
			return;
		}
	}

	/* If a crash is occurring, make sure we can't deadlock */
	raw_spin_lock_init(p_die_lock);
}

/* for aee_aed.ko */
const char *aee_arch_vma_name(struct vm_area_struct *vma)
{
	return arch_vma_name(vma);
}
EXPORT_SYMBOL(aee_arch_vma_name);

#endif
