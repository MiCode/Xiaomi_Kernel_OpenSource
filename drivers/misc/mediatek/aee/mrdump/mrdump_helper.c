// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (C) 2015 MediaTek Inc.
 */

#include <linux/delay.h>
#include <linux/init.h>
#include <linux/memblock.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/of_fdt.h>
#include <linux/percpu.h>
#include <linux/sched/signal.h>
#include <linux/string.h>
#include "../../../../kernel/sched/sched.h"

#include <asm/memory.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/smp_plat.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>

#include <mt-plat/aee.h>
#include <mt-plat/mboot_params.h>
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

/*** *** *** Print sched debug information at aee *** *** ***/
#define NO_EXPORT 0
#define TRYLOCK_NUM 10

/*
 * from kernel/sched/debug.c
 * Ease the printing of nsec fields:
 */
static long long nsec_high(unsigned long long nsec)
{
	if ((long long)nsec < 0) {
		nsec = -nsec;
		do_div(nsec, 1000000);
		return -nsec;
	}
	do_div(nsec, 1000000);

	return nsec;
}

static unsigned long nsec_low(unsigned long long nsec)
{
	if ((long long)nsec < 0)
		nsec = -nsec;

	return do_div(nsec, 1000000);
}

#define SPLIT_NS(x) nsec_high(x), nsec_low(x)

#ifdef CONFIG_CGROUP_SCHED
static char group_path[PATH_MAX];

static char *task_group_path(struct task_group *tg)
{
	if (autogroup_path(tg, group_path, PATH_MAX))
		return group_path;

	cgroup_path(tg->css.cgroup, group_path, PATH_MAX);

	return group_path;
}
#endif

static DEFINE_SPINLOCK(sched_debug_lock);

static const char * const sched_tunable_scaling_names[] = {
	"none",
	"logarithmic",
	"linear"
}; /* kernel/sched/debug.c */

char print_at_AEE_buffer[160];

#define SEQ_printf_at_AEE(m, x...)		\
do {						\
	snprintf(print_at_AEE_buffer, sizeof(print_at_AEE_buffer), x);	\
	aee_sram_fiq_log(print_at_AEE_buffer);	\
} while (0)

static void
print_task_at_AEE(struct seq_file *m, struct rq *rq, struct task_struct *p)
{
#ifdef CONFIG_SCHEDSTATS
	if (rq->curr == p) {
#ifdef CONFIG_CGROUP_SCHED
		SEQ_printf_at_AEE(m, "R%15s %5d %9lld.%06ld %9lld ",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw));

		SEQ_printf_at_AEE(m, "%5d ", p->prio);

		SEQ_printf_at_AEE(m, "%9lld.%06ld %9lld.%06ld %9lld.%06ld ",
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));

		SEQ_printf_at_AEE(m, "%s\n", task_group_path(task_group(p)));

#else
		SEQ_printf_at_AEE(m, "R%15s %5d %9lld.%06ld %9lld ",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw));

		SEQ_printf_at_AEE(m, "%5d ", p->prio);

		SEQ_printf_at_AEE(m, "%9lld.%06ld %9lld.%06ld %9lld.%06ld ",
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));
#endif
#ifdef CONFIG_NUMA_BALANCING
		SEQ_printf_at_AEE(m, " %d %d",
			task_node(p), task_numa_group_id(p));
#endif
	} else {
#ifdef CONFIG_CGROUP_SCHED
		SEQ_printf_at_AEE(m, " %15s %5d %9lld.%06ld %9lld ",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw));

		SEQ_printf_at_AEE(m, "%5d ", p->prio);

		SEQ_printf_at_AEE(m, "%9lld.%06ld %9lld.%06ld %9lld.%06ld ",
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));

		SEQ_printf_at_AEE(m, "%s\n", task_group_path(task_group(p)));
#else
		SEQ_printf_at_AEE(m, "% 15s %5d %9lld.%06ld %9lld ",
			p->comm,
			task_pid_nr(p),
			SPLIT_NS(p->se.vruntime),
			(long long)(p->nvcsw + p->nivcsw));
		SEQ_printf_at_AEE(m, "%5d ", p->prio);

		SEQ_printf_at_AEE(m, "%9lld.%06ld %9lld.%06ld %9lld.%06ld ",
			SPLIT_NS(p->se.statistics.wait_sum),
			SPLIT_NS(p->se.sum_exec_runtime),
			SPLIT_NS(p->se.statistics.sum_sleep_runtime));
#endif
#ifdef CONFIG_NUMA_BALANCING
		SEQ_printf_at_AEE(m, " %d %d",
			task_node(p), task_numa_group_id(p));
#endif
	}
#else
	SEQ_printf_at_AEE(m, "%9lld.%06ld %9lu %9lld.%06ld",
		0LL, 0L,
		SPLIT_NS(p->se.sum_exec_runtime),
		0LL, 0L);
#endif
}

/* sched: add aee log */
#define read_trylock_irqsave(lock, flags) \
	({ \
	 typecheck(unsigned long, flags); \
	 local_irq_save(flags); \
	 read_trylock(lock) ? \
	 1 : ({ local_irq_restore(flags); 0; }); \
	 })

int read_trylock_n_irqsave(rwlock_t *lock,
		unsigned long *flags, struct seq_file *m, char *msg)
{
	int locked, trylock_cnt = 0;

	do {
		locked = read_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);
	} while ((!locked) && (trylock_cnt < TRYLOCK_NUM));

	if (!locked) {
#ifdef CONFIG_DEBUG_SPINLOCK
		struct task_struct *owner = NULL;
#endif
		SEQ_printf_at_AEE(m, "Warning: fail to get lock in %s\n", msg);
#ifdef CONFIG_DEBUG_SPINLOCK
		if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
			owner = lock->owner;
#ifdef CONFIG_SMP
		SEQ_printf_at_AEE(m, " lock: %p, .magic: %08x, .owner: %s/%d",
				lock, lock->magic,
				owner ? owner->comm : "<<none>>",
				owner ? task_pid_nr(owner) : -1);

#ifdef CONFIG_ARM64
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			lock->owner_cpu, lock->raw_lock.wait_lock.locked);
#else
		SEQ_printf_at_AEE(m, ".owner_cpu: %d\n", lock->owner_cpu);
#endif

#else
		SEQ_printf_at_AEE(m, " lock: %p, .magic: %08x, .owner: %s/%d",
			   lock, lock->magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d\n", lock->owner_cpu);
#endif
#endif
	}

	return locked;
}

int raw_spin_trylock_n_irqsave(raw_spinlock_t *lock,
		unsigned long *flags, struct seq_file *m, char *msg)
{
	int locked, trylock_cnt = 0;

	do {
		locked = raw_spin_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);
	} while ((!locked) && (trylock_cnt < TRYLOCK_NUM));

	if (!locked) {
#ifdef CONFIG_DEBUG_SPINLOCK
		struct task_struct *owner = NULL;
#endif
		SEQ_printf_at_AEE(m, "Warning: fail to get lock in %s\n", msg);
#ifdef CONFIG_DEBUG_SPINLOCK
		if (lock->owner && lock->owner != SPINLOCK_OWNER_INIT)
			owner = lock->owner;
#ifdef CONFIG_ARM64
#ifdef CONFIG_SMP
		SEQ_printf_at_AEE(m, " lock: %lx, .magic: %08x, .owner: %s/%d",
			   (long)lock, lock->magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d",
			   lock->owner_cpu);
#else
		SEQ_printf_at_AEE(m, " lock: %lx, .magic: %08x, .owner: %s/%d",
			   (long)lock, lock->magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			   lock->owner_cpu, lock->raw_lock.slock);
#endif
#else
		SEQ_printf_at_AEE(m, " lock: %x, .magic: %08x, .owner: %s/%d",
			   (int)lock, lock->magic,
				owner ? owner->comm : "<<none>>",
				owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
				lock->owner_cpu, lock->raw_lock.slock);
#endif
#endif
	}

	return locked;
}

int spin_trylock_n_irqsave(spinlock_t *lock,
		unsigned long *flags, struct seq_file *m, char *msg)
{
	int locked, trylock_cnt = 0;

	do {
		locked = spin_trylock_irqsave(lock, *flags);
		trylock_cnt++;
		mdelay(10);

	} while ((!locked) && (trylock_cnt < TRYLOCK_NUM));

	if (!locked) {
#ifdef CONFIG_DEBUG_SPINLOCK
		raw_spinlock_t rlock = lock->rlock;
		struct task_struct *owner = NULL;
#endif
		SEQ_printf_at_AEE(m, "Warning: fail to get lock in %s\n", msg);
#ifdef CONFIG_DEBUG_SPINLOCK
		if (rlock.owner && rlock.owner != SPINLOCK_OWNER_INIT)
			owner = rlock.owner;
#ifdef CONFIG_ARM64
#ifdef CONFIG_SMP
		SEQ_printf_at_AEE(m, " lock: %lx, .magic: %08x, .owner: %s/%d",
			   (long)&rlock, rlock.magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, " .owner_cpu: %d, pending: %u",
			   rlock.owner_cpu,
			   rlock.raw_lock.pending);
#else
		SEQ_printf_at_AEE(m, " lock: %lx, .magic: %08x, .owner: %s/%d",
			   (long)&rlock, rlock.magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			   rlock.owner_cpu, rlock.raw_lock.slock);
#endif
#else
		SEQ_printf_at_AEE(m, " lock: %x, .magic: %08x, .owner: %s/%d",
			   (int)&rlock, rlock.magic,
			   owner ? owner->comm : "<<none>>",
			   owner ? task_pid_nr(owner) : -1);
		SEQ_printf_at_AEE(m, ".owner_cpu: %d, value: %d\n",
			    rlock.owner_cpu, rlock.raw_lock.slock);
#endif
#endif
	}

	return locked;
}
static void print_rq_at_AEE(struct seq_file *m, struct rq *rq, int rq_cpu)
{
	struct task_struct *g, *p;

	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "runnable tasks:\n");
	SEQ_printf_at_AEE(m,
	"            task   PID         tree-key  switches  prio        wait-time         sum-exec        sum-sleep\n");
	SEQ_printf_at_AEE(m, "---------------------------------------------------\n");

	rcu_read_lock();
	for_each_process_thread(g, p) {
		/*
		 * if (task_cpu(p) != rq_cpu)
		 * sched: only output the runnable tasks,
		 * rather than ALL tasks in runqueues
		 */
		if (!p->on_rq || task_cpu(p) != rq_cpu)
			continue;

		print_task_at_AEE(m, rq, p);
	}
	rcu_read_unlock();
}

#ifdef CONFIG_FAIR_GROUP_SCHED
static void print_cfs_group_stats_at_AEE(struct seq_file *m,
		int cpu, struct task_group *tg)
{
	struct sched_entity *se = tg->se[cpu];

#define P(F)		SEQ_printf_at_AEE(m, "  .%-30s: %lld\n",	#F, (long long)F)
#define PN(F)		SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #F, SPLIT_NS((long long)F))

	if (!se)
		return;

	PN(se->exec_start);
	PN(se->vruntime);
	PN(se->sum_exec_runtime);
#if NO_EXPORT
#define P_SCHEDSTAT(F)	\
	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n",        #F, (long long)schedstat_val(F))
#define PN_SCHEDSTAT(F)	\
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #F, SPLIT_NS((long long)schedstat_val(F)))
	if (schedstat_enabled()) {
		PN_SCHEDSTAT(se->statistics.wait_start);
		PN_SCHEDSTAT(se->statistics.sleep_start);
		PN_SCHEDSTAT(se->statistics.block_start);
		PN_SCHEDSTAT(se->statistics.sleep_max);
		PN_SCHEDSTAT(se->statistics.block_max);
		PN_SCHEDSTAT(se->statistics.exec_max);
		PN_SCHEDSTAT(se->statistics.slice_max);
		PN_SCHEDSTAT(se->statistics.wait_max);
		PN_SCHEDSTAT(se->statistics.wait_sum);
		P_SCHEDSTAT(se->statistics.wait_count);
	}
#endif
	P(se->load.weight);
	P(se->runnable_weight);
#ifdef CONFIG_SMP
	P(se->avg.load_avg);
	P(se->avg.util_avg);
#endif
#undef PN
#undef P
}
#endif

void print_cfs_rq_at_AEE(struct seq_file *m, int cpu, struct cfs_rq *cfs_rq)
{
#if NO_EXPORT
	s64 MIN_vruntime = -1, min_vruntime, max_vruntime = -1,
		spread, rq0_min_vruntime, spread0;
	struct rq *rq = cpu_rq(cpu);
	struct sched_entity *last;
	unsigned long flags;
	int locked;
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "cfs_rq[%d]:%s\n", cpu, task_group_path(cfs_rq->tg));
#else
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "cfs_rq[%d]:\n", cpu);
#endif
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "exec_clock",
			SPLIT_NS(cfs_rq->exec_clock));

#if NO_EXPORT
	locked = raw_spin_trylock_n_irqsave(&rq->lock,
			&flags, m, "print_cfs_rq_at_AEE");
	if (rb_first_cached(&cfs_rq->tasks_timeline))
		MIN_vruntime = (__pick_first_entity(cfs_rq))->vruntime;
	last = __pick_last_entity(cfs_rq);
	if (last)
		max_vruntime = last->vruntime;

	min_vruntime = cfs_rq->min_vruntime;
	rq0_min_vruntime = cpu_rq(0)->cfs.min_vruntime;
	if (locked)
		raw_spin_unlock_irqrestore(&rq->lock, flags);
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "MIN_vruntime",
			SPLIT_NS(MIN_vruntime));
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "min_vruntime",
			SPLIT_NS(min_vruntime));
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "max_vruntime",
			SPLIT_NS(max_vruntime));

	spread = max_vruntime - MIN_vruntime;
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "spread",
			SPLIT_NS(spread));
	spread0 = min_vruntime - rq0_min_vruntime;
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "spread0",
		SPLIT_NS(spread0));
	SEQ_printf_at_AEE(m, "  .%-30s: %d\n", "nr_spread_over",
		cfs_rq->nr_spread_over);
#endif

	SEQ_printf_at_AEE(m, "  .%-30s: %d\n",
			"nr_running", cfs_rq->nr_running);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "load", cfs_rq->load.weight);
#ifdef CONFIG_SMP
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "load_avg",
			cfs_rq->avg.load_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "util_avg",
			cfs_rq->avg.util_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %u\n", "util_est_enqueued",
			cfs_rq->avg.util_est.enqueued);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "removed.load_avg",
			cfs_rq->removed.load_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "removed.util_avg",
			cfs_rq->removed.util_avg);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "removed.runnable_avg",
			cfs_rq->removed.runnable_avg);

#ifdef CONFIG_FAIR_GROUP_SCHED
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", "tg_load_avg_contrib",
			cfs_rq->tg_load_avg_contrib);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", "tg_load_avg",
			atomic_long_read(&cfs_rq->tg->load_avg));
#endif
#endif
#ifdef CONFIG_CFS_BANDWIDTH
	SEQ_printf_at_AEE(m, "  .%-30s: %d\n", "throttled",
			cfs_rq->throttled);
	SEQ_printf_at_AEE(m, "  .%-30s: %d\n", "throttle_count",
			cfs_rq->throttle_count);
#endif

#ifdef CONFIG_FAIR_GROUP_SCHED
	print_cfs_group_stats_at_AEE(m, cpu, cfs_rq->tg);
#endif
}

#define for_each_leaf_cfs_rq(rq, cfs_rq) \
	list_for_each_entry_rcu(cfs_rq, &rq->leaf_cfs_rq_list, leaf_cfs_rq_list)


void print_cfs_stats_at_AEE(struct seq_file *m, int cpu)
{
	struct cfs_rq *cfs_rq;

	rcu_read_lock();
	cfs_rq = &cpu_rq(cpu)->cfs;
	/* sched: only output / cgroup schedule info */
	print_cfs_rq_at_AEE(m, cpu, cfs_rq);
	rcu_read_unlock();
}

void print_rt_rq_at_AEE(struct seq_file *m, int cpu, struct rt_rq *rt_rq)
{
#ifdef CONFIG_RT_GROUP_SCHED
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "rt_rq[%d]:%s\n", cpu, task_group_path(rt_rq->tg));
#else
	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "rt_rq[%d]:\n", cpu);
#endif


#define P(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", #x, (long long)(rt_rq->x))
#define PU(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #x, (unsigned long)(rt_rq->x))
#define PN(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #x, SPLIT_NS(rt_rq->x))

	P(rt_nr_running);
#ifdef CONFIG_SMP
	PU(rt_nr_migratory);
#endif

	P(rt_throttled);
	PN(rt_time);
	PN(rt_runtime);

#undef PN
#undef PU
#undef P
}


#ifdef CONFIG_RT_GROUP_SCHED

static inline struct task_group *next_task_group(struct task_group *tg)
{
	do {
		tg = list_entry_rcu(tg->list.next,
			typeof(struct task_group), list);
	} while (&tg->list != &task_groups && task_group_is_autogroup(tg));

	if (&tg->list == &task_groups)
		tg = NULL;

	return tg;
}

#define for_each_rt_rq(rt_rq, iter, rq)					\
	for (iter = container_of(&task_groups, typeof(*iter), list);	\
		(iter = next_task_group(iter)) &&			\
		(rt_rq = iter->rt_rq[cpu_of(rq)]);)

#else /* !CONFIG_RT_GROUP_SCHED */

#define for_each_rt_rq(rt_rq, iter, rq) \
	for ((void) iter, rt_rq = &rq->rt; rt_rq; rt_rq = NULL)

#endif

void print_rt_stats_at_AEE(struct seq_file *m, int cpu)
{
	struct rt_rq *rt_rq;

	rt_rq = &cpu_rq(cpu)->rt;

	rcu_read_lock();
	/* sched: only output / cgroup schedule info */
	print_rt_rq_at_AEE(m, cpu, rt_rq);
	rcu_read_unlock();
}

void print_dl_rq_at_AEE(struct seq_file *m, int cpu, struct dl_rq *dl_rq)
{
	struct dl_bw *dl_bw;

	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "dl_rq[%d]:\n", cpu);

#define PU(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #x, (unsigned long)(dl_rq->x))

	PU(dl_nr_running);
#ifdef CONFIG_SMP
	PU(dl_nr_migratory);
	dl_bw = &cpu_rq(cpu)->rd->dl_bw;
#else
	dl_bw = &dl_rq->dl_bw;
#endif
	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", "dl_bw->bw", dl_bw->bw);
	SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", "dl_bw->total_bw", dl_bw->total_bw);

#undef PU
}

void print_dl_stats_at_AEE(struct seq_file *m, int cpu)
{
	print_dl_rq_at_AEE(m, cpu, &cpu_rq(cpu)->dl);
}

static void print_cpu_at_AEE(struct seq_file *m, int cpu)
{
	struct rq *rq = cpu_rq(cpu);
	unsigned long flags;
	int locked;

#ifdef CONFIG_X86
	{
		unsigned int freq = cpu_khz ? : 1;

		SEQ_printf_at_AEE(m, "cpu#%d, %u.%03u MHz\n",
			   cpu, freq / 1000, (freq % 1000));
	}
#else
	/* sched: add cpu info */
	SEQ_printf_at_AEE(m, "cpu#%d: %s\n", cpu,
			cpu_is_offline(cpu) ? "Offline" : "Online");
#endif

#define P(x) \
do { \
	if (sizeof(rq->x) == 4) \
		SEQ_printf_at_AEE(m, "  .%-30s: %ld\n", \
		#x, (long)(rq->x)); \
	else \
		SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", \
		#x, (long long)(rq->x)); \
} while (0)

#define PN(x) \
	SEQ_printf_at_AEE(m, "  .%-30s: %lu\n", #x, SPLIT_NS(rq->x))

	P(nr_running);
	P(nr_switches);
	P(nr_uninterruptible);
	PN(next_balance);
	SEQ_printf_at_AEE(m, "  .%-30s: %ld\n",
			"curr->pid", (long)(task_pid_nr(rq->curr)));
	PN(clock);
	PN(clock_task);
#undef P
#undef PN

#ifdef CONFIG_SMP
#define P64(n) SEQ_printf_at_AEE(m, "  .%-30s: %lld\n", #n, rq->n)
	P64(avg_idle);
	P64(max_idle_balance_cost);
#undef P64
#endif

#if NO_EXPORT
#define P(n) SEQ_printf_at_AEE(m, "  .%-30s: %d\n", #n, schedstat_val(rq->n))
	if (schedstat_enabled()) {
		P(yld_count);
		P(sched_count);
		P(sched_goidle);
		P(ttwu_count);
		P(ttwu_local);
	}
#undef P
#endif

	locked = spin_trylock_n_irqsave(&sched_debug_lock,
			&flags, m, "print_cpu_at_AEE");
	print_cfs_stats_at_AEE(m, cpu);
	print_rt_stats_at_AEE(m, cpu);
	print_dl_stats_at_AEE(m, cpu);

	rcu_read_lock();
	print_rq_at_AEE(m, rq, cpu);
	SEQ_printf_at_AEE(m, "============================================\n");
	rcu_read_unlock();

	if (locked)
		spin_unlock_irqrestore(&sched_debug_lock, flags);
}

static void sched_debug_header_at_AEE(struct seq_file *m)
{
	u64 ktime, sched_clk, cpu_clk;
	unsigned long flags;

	local_irq_save(flags);
	ktime = ktime_to_ns(ktime_get());
	sched_clk = sched_clock();
	cpu_clk = local_clock();
	local_irq_restore(flags);

	SEQ_printf_at_AEE(m, "Sched Debug Version: v0.11, %s %.*s\n",
		init_utsname()->release,
		(int)strcspn(init_utsname()->version, " "),
		init_utsname()->version);

#define P(x) \
	SEQ_printf_at_AEE(m, "%-40s: %lld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf_at_AEE(m, "%-40s: %lu\n", #x, SPLIT_NS(x))
	PN(ktime);
	PN(sched_clk);
	PN(cpu_clk);
	P(jiffies);
#ifdef CONFIG_HAVE_UNSTABLE_SCHED_CLOCK
	P(sched_clock_stable());
#endif
#undef PN
#undef P

	SEQ_printf_at_AEE(m, "\n");
	SEQ_printf_at_AEE(m, "sysctl_sched\n");

#if NO_EXPORT
#define P(x) \
	SEQ_printf_at_AEE(m, "  .%-40s: %lld\n", #x, (long long)(x))
#define PN(x) \
	SEQ_printf_at_AEE(m, "  .%-40s: %lu\n", #x, SPLIT_NS(x))
	PN(sysctl_sched_latency);
	PN(sysctl_sched_min_granularity);
	PN(sysctl_sched_wakeup_granularity);
	P(sysctl_sched_child_runs_first);
	P(sysctl_sched_features);
#undef PN
#undef P

	SEQ_printf_at_AEE(m, "  .%-40s: %d (%s)\n",
		"sysctl_sched_tunable_scaling",
		sysctl_sched_tunable_scaling,
		sched_tunable_scaling_names[sysctl_sched_tunable_scaling]);
#endif
	SEQ_printf_at_AEE(m, "\n");
}

void sysrq_sched_debug_show_at_AEE(void)
{
	int cpu;
#if NO_EXPORT
	unsigned long flags;
	int locked;
#endif

	sched_debug_header_at_AEE(NULL);
#if NO_EXPORT
	locked = read_trylock_n_irqsave(&tasklist_lock,
			&flags, NULL, "sched_debug_show_at_AEE");
#endif
	rcu_read_lock();
	for_each_possible_cpu(cpu) {
		print_cpu_at_AEE(NULL, cpu);
	}
	rcu_read_unlock();
#if NO_EXPORT
	if (locked)
		read_unlock_irqrestore(&tasklist_lock, flags);
#endif
}
EXPORT_SYMBOL(sysrq_sched_debug_show_at_AEE);
/*** *** *** Print sched debug information at aee END *** *** ***/
