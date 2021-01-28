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
#include "../../../../kernel/sched/sched.h"

#include <asm/memory.h>
#include <asm/pgtable.h>
#include <asm/sections.h>
#include <asm/smp_plat.h>
#include <asm/stacktrace.h>
#include <asm/system_misc.h>

#include <mt-plat/aee.h>

struct module_sect_attr {
	struct module_attribute mattr;
	char *name;
	unsigned long address;
};

struct module_sect_attrs {
	struct attribute_group grp;
	unsigned int nsections;
	struct module_sect_attr attrs[0];
};

#ifdef MODULE
struct aee_sym {
	char name[KSYM_NAME_LEN];
	unsigned long addr;
};

static int (*aee_koes)(int (*fn)(void *, const char *,
				struct module *,
				unsigned long),
			void *data);

static int addr_ok(void *data, const char *namebuf,
		struct module *mod, unsigned long addr)
{
	struct aee_sym *aee_sym = (struct aee_sym *)data;

	if (strcmp(namebuf, aee_sym->name) == 0) {
		aee_sym->addr = addr;
		return 1;
	}
	return 0;
}

#define SYM_PT "kallsyms_on_each_symbol+0x%X/0x%X"
#define FIX_OFF 0x72C
static void find_koes(void)
{
	char namebuf[KSYM_NAME_LEN] = {0};
	unsigned long addr_tmp = (unsigned long)sprint_symbol;
	int off;
	int fsize;
	int ret;

	if (aee_koes)
		return;

	addr_tmp -= FIX_OFF;
	sprint_symbol(namebuf, addr_tmp);
	ret = sscanf(namebuf, SYM_PT, &off, &fsize);
	if (ret == 2) {
		if (off > 0 && off <= fsize)
			addr_tmp -= off;
		aee_koes = (void *)addr_tmp;
		return;
	}
}

static int need_init = 1;
unsigned long aee_addr_find(const char *name)
{
	struct aee_sym sym;

	if (need_init) {
		need_init = 0;
		find_koes();
	}
	if (!aee_koes)
		return 0;

	memset(&sym, 0x0, sizeof(sym));
	if (snprintf(sym.name, sizeof(sym.name), "%s", name) < 0)
		return 0;

	if (aee_koes(addr_ok, (void *)(&sym)))
		return sym.addr;

	return 0;
}
EXPORT_SYMBOL(aee_addr_find);

/* for mrdump.ko */
static void (*p_show_regs)(struct pt_regs *);
void aee_show_regs(struct pt_regs *regs)
{
	if (p_show_regs) {
		p_show_regs(regs);
		return;
	}

	p_show_regs = (void *)aee_addr_find("__show_regs");
	if (!p_show_regs)
		pr_info("%s failed", __func__);
	else
		p_show_regs(regs);
}

#ifdef CONFIG_ARM64
static int (*p_unwind_frame)(struct task_struct *tsk,
				struct stackframe *frame);
int aee_unwind_frame(struct task_struct *tsk, struct stackframe *frame)
{
	if (p_unwind_frame)
		return p_unwind_frame(tsk, frame);

	p_unwind_frame = (void *)aee_addr_find("unwind_frame");
	if (p_unwind_frame)
		return p_unwind_frame(tsk, frame);

	pr_info("%s failed", __func__);
	return -1;
}
#else
static int (*p_unwind_frame)(struct stackframe *frame);
int aee_unwind_frame(struct stackframe *frame)
{
	if (p_unwind_frame)
		return p_unwind_frame(frame);

	p_unwind_frame = (void *)aee_addr_find("unwind_frame");
	if (p_unwind_frame)
		return p_unwind_frame(frame);

	pr_info("%s failed", __func__);
	return -1;
}
#endif

static unsigned long p_stext;
unsigned long aee_get_stext(void)
{
	if (p_stext != 0)
		return p_stext;

	p_stext = aee_addr_find("_stext");
	if (p_stext == 0)
		pr_info("%s failed", __func__);
	return p_stext;
}

static unsigned long p_etext;
unsigned long aee_get_etext(void)
{
	if (p_etext != 0)
		return p_etext;

	p_etext = aee_addr_find("_etext");
	if (p_etext == 0)
		pr_info("%s failed", __func__);
	return p_etext;
}

static unsigned long p_text;
unsigned long aee_get_text(void)
{
	if (p_text != 0)
		return p_text;

	p_text = aee_addr_find("_text");
	if (p_text == 0)
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

#if defined(CONFIG_ARM64)
static unsigned long *p_kimage_vaddr;
unsigned long aee_get_kimage_vaddr(void)
{
	if (p_kimage_vaddr)
		return *p_kimage_vaddr;

	p_kimage_vaddr = (void *)aee_addr_find("kimage_vaddr");
	if (!p_kimage_vaddr) {
		pr_info("%s failed", __func__);
		return 0;
	}
	return *p_kimage_vaddr;
}
#endif

#ifdef CONFIG_SYSFS
static struct kset **p_module_kset;
struct kset *aee_get_module_kset(void)
{
	if (p_module_kset)
		return *p_module_kset;

	p_module_kset = (void *)aee_addr_find("module_kset");
	if (!p_module_kset) {
		pr_info("%s failed", __func__);
		return NULL;
	}
	return *p_module_kset;
}
#endif

static phys_addr_t (*p_memblock_start_of_DRAM)(void);
phys_addr_t aee_memblock_start_of_DRAM(void)
{
	if (p_memblock_start_of_DRAM)
		return p_memblock_start_of_DRAM();

	p_memblock_start_of_DRAM =
			(void *)aee_addr_find("memblock_start_of_DRAM");
	if (p_memblock_start_of_DRAM)
		return p_memblock_start_of_DRAM();

	pr_info("%s failed", __func__);
	return 0;
}

static phys_addr_t (*p_memblock_end_of_DRAM)(void);
phys_addr_t aee_memblock_end_of_DRAM(void)
{
	if (p_memblock_end_of_DRAM)
		return p_memblock_end_of_DRAM();

	p_memblock_end_of_DRAM = (void *)aee_addr_find("memblock_end_of_DRAM");
	if (p_memblock_end_of_DRAM)
		return p_memblock_end_of_DRAM();

	pr_info("%s failed", __func__);
	return 0;

}

static void (*p_print_modules)(void);
void aee_print_modules(void)
{
	if (p_print_modules) {
		p_print_modules();
		return;
	}

	p_print_modules = (void *)aee_addr_find("print_modules");
	if (!p_print_modules) {
		pr_info("%s failed", __func__);
		return;
	}

	p_print_modules();
}

static struct list_head *p_modules;
/* MUST ensure called when preempt disabled already */
int aee_save_modules(char *mbuf, int mbufsize)
{
	struct module *mod;
	int sz = 0;
	unsigned long text_addr = 0;
	unsigned long init_addr = 0;
	int i, search_nm;

	if (!mbuf || mbufsize <= 0) {
		pr_info("mrdump: module info buffer wrong(sz:%d)\n", mbufsize);
		return sz;
	}

	if (!p_modules)
		p_modules = (void *)aee_addr_find("modules");

	if (!p_modules) {
		pr_info("%s failed", __func__);
		return sz;
	}

	memset(mbuf, '\0', mbufsize);
	sz += snprintf(mbuf + sz, mbufsize - sz, "Modules linked in:");
	list_for_each_entry_rcu(mod, p_modules, list) {
		if (mod->state == MODULE_STATE_UNFORMED)
			continue;
		if (sz >= mbufsize) {
			pr_info("mrdump: module info buffer full(sz:%d)\n",
					mbufsize);
			break;
		}
		text_addr = (unsigned long)mod->core_layout.base;
		init_addr = (unsigned long)mod->init_layout.base;
		search_nm = 2;
		for (i = 0; i < mod->sect_attrs->nsections; i++) {
			if (!strcmp(mod->sect_attrs->attrs[i].name, ".text")) {
				text_addr = mod->sect_attrs->attrs[i].address;
				search_nm--;
			} else if (!strcmp(mod->sect_attrs->attrs[i].name,
					   ".init.text")) {
				init_addr = mod->sect_attrs->attrs[i].address;
				search_nm--;
			}
			if (!search_nm)
				break;
		}
		sz += snprintf(mbuf + sz, mbufsize - sz,
				" %s 0x%lx 0x%lx %d %d",
				mod->name,
				text_addr,
				init_addr,
				mod->core_layout.size,
				mod->init_layout.size);
	}
	if (sz < mbufsize)
		sz += snprintf(mbuf + sz, mbufsize - sz, "\n");
	return sz;
}

#ifdef __aarch64__
static u64 *p__cpu_logical_map;
static u64 *aee_cpu_logical_map(void)
#else
static u32 *p__cpu_logical_map;
static u32 *aee_cpu_logical_map(void)
#endif
{
	if (p__cpu_logical_map)
		return p__cpu_logical_map;


	p__cpu_logical_map = (void *)aee_addr_find("__cpu_logical_map");
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

#ifdef __aarch64__
static unsigned long *p_irq_stack_ptr;
static unsigned long *aee_irq_stack_ptr(void)
{
	if (p_irq_stack_ptr)
		return p_irq_stack_ptr;

	p_irq_stack_ptr = (void *)aee_addr_find("irq_stack_ptr");
	if (!p_irq_stack_ptr) {
		pr_info("%s failed", __func__);
		return NULL;
	}
	return p_irq_stack_ptr;
}

/* NOTICE: this function should be the same with on_irq_stack() */
bool aee_on_irq_stack(unsigned long sp, struct stack_info *info)
{
	unsigned long *isp = aee_irq_stack_ptr();
	unsigned long low, high;

	if (!isp)
		return false;

	low = (unsigned long)raw_cpu_read(*isp);
	high = low + IRQ_STACK_SIZE;

	if (!low)
		return false;

	if (sp < low || sp >= high)
		return false;

	if (info) {
		info->low = low;
		info->high = high;
		info->type = STACK_TYPE_IRQ;
	}

	return true;
}
#endif

static struct mm_struct *p_init_mm;
static struct mm_struct *aee_init_mm(void)
{
	if (p_init_mm)
		return p_init_mm;

	p_init_mm = (void *)aee_addr_find("init_mm");
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

static struct rq *p_runqueues;
static struct rq *aee_runqueues(void)
{
	if (p_runqueues)
		return p_runqueues;

	p_runqueues = (void *)aee_addr_find("runqueues");
	if (!p_runqueues) {
		pr_info("%s failed", __func__);
		return NULL;
	}
	return p_runqueues;
}

unsigned long aee_cpu_rq(int cpu)
{
	struct rq *p_runqueues = aee_runqueues();

	if (p_runqueues)
		return (unsigned long)(&per_cpu(*p_runqueues, (cpu)));
	return 0;
}

struct task_struct *aee_cpu_curr(int cpu)
{
	struct rq *rq = (struct rq *)aee_cpu_rq(cpu);

	if (rq)
		return (struct task_struct *)(rq->curr);
	return NULL;
}

unsigned long aee_get_swapper_pg_dir(void)
{
	/* FIX ME: symbol in *ABS* section */
	return 0;
}

unsigned long aee_get_kallsyms_addresses(void)
{
	/* FIX ME: symbol in *UND* section */
	return 0;
}

static void (*p__flush_dcache_area)(void *addr, size_t len);
void aee__flush_dcache_area(void *addr, size_t len)
{
	if (p__flush_dcache_area) {
		p__flush_dcache_area(addr, len);
		return;
	}

	p__flush_dcache_area = (void *)aee_addr_find("__flush_dcache_area");
	if (!p__flush_dcache_area) {
		pr_info("%s failed", __func__);
		return;
	}

	p__flush_dcache_area(addr, len);
}

raw_spinlock_t *p_logbuf_lock;
struct semaphore *p_console_sem;
void aee_zap_locks(void)
{
	if (!p_logbuf_lock) {
		p_logbuf_lock = (void *)aee_addr_find("logbuf_lock");
		if (!p_logbuf_lock) {
			aee_sram_printk("%s failed to get logbuf lock\n",
					__func__);
			return;
		}
	}
	if (!p_console_sem) {
		p_console_sem = (void *)aee_addr_find("console_sem");
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

/* for aee_aed.ko */
static const char *(*p_arch_vma_name)(struct vm_area_struct *vma);
const char *aee_arch_vma_name(struct vm_area_struct *vma)
{
	if (p_arch_vma_name)
		return p_arch_vma_name(vma);

	p_arch_vma_name = (void *)aee_addr_find("arch_vma_name");
	if (p_arch_vma_name)
		return p_arch_vma_name(vma);

	pr_info("%s failed", __func__);
	return NULL;
}
EXPORT_SYMBOL(aee_arch_vma_name);

#else /* #ifdef MODULE*/
/* for mrdump.ko */
void aee_show_regs(struct pt_regs *regs)
{
	__show_regs(regs);
}

#ifdef CONFIG_ARM64
int aee_unwind_frame(struct task_struct *tsk, struct stackframe *frame)
{
	return unwind_frame(tsk, frame);
}
#else
int aee_unwind_frame(struct stackframe *frame)
{
	return unwind_frame(frame);
}
#endif

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

#if defined(CONFIG_ARM64)
unsigned long aee_get_kimage_vaddr(void)
{
	return (unsigned long)kimage_vaddr;
}
#endif

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

void aee_print_modules(void)
{
	print_modules();
}

static struct list_head *p_modules;
/* MUST ensure called when preempt disabled already */
int aee_save_modules(char *mbuf, int mbufsize)
{
	struct module *mod;
	int sz = 0;
	unsigned long text_addr = 0;
	unsigned long init_addr = 0;
	int i, search_nm;

	if (!mbuf || mbufsize <= 0) {
		pr_info("mrdump: module info buffer wrong(sz:%d)\n", mbufsize);
		return sz;
	}

	if (!p_modules)
		p_modules = (void *)kallsyms_lookup_name("modules");

	if (!p_modules) {
		pr_info("%s failed", __func__);
		return sz;
	}

	memset(mbuf, '\0', mbufsize);
	sz += snprintf(mbuf + sz, mbufsize - sz, "Modules linked in:");
	list_for_each_entry_rcu(mod, p_modules, list) {
		if (mod->state == MODULE_STATE_UNFORMED)
			continue;
		if (sz >= mbufsize) {
			pr_info("mrdump: module info buffer full(sz:%d)\n",
					mbufsize);
			break;
		}
		text_addr = (unsigned long)mod->core_layout.base;
		init_addr = (unsigned long)mod->init_layout.base;
		search_nm = 2;
		for (i = 0; i < mod->sect_attrs->nsections; i++) {
			if (!strcmp(mod->sect_attrs->attrs[i].name, ".text")) {
				text_addr = mod->sect_attrs->attrs[i].address;
				search_nm--;
			} else if (!strcmp(mod->sect_attrs->attrs[i].name,
					   ".init.text")) {
				init_addr = mod->sect_attrs->attrs[i].address;
				search_nm--;
			}
			if (!search_nm)
				break;
		}
		sz += snprintf(mbuf + sz, mbufsize - sz,
				" %s 0x%lx 0x%lx %d %d",
				mod->name,
				text_addr,
				init_addr,
				mod->core_layout.size,
				mod->init_layout.size);
	}
	if (sz < mbufsize)
		sz += snprintf(mbuf + sz, mbufsize - sz, "\n");
	return sz;
}

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

#ifdef __aarch64__
bool aee_on_irq_stack(unsigned long sp, struct stack_info *info)
{
	return on_irq_stack(sp, info);
}
#endif

pgd_t *aee_pgd_offset_k(unsigned long addr)
{
	return (pgd_t *)pgd_offset_k(addr);
}

unsigned long aee_cpu_rq(int cpu)
{
	return (unsigned long)cpu_rq(cpu);
}

struct task_struct *aee_cpu_curr(int cpu)
{
	return (struct task_struct *)cpu_curr(cpu);
}

unsigned long aee_get_swapper_pg_dir(void)
{
	return (unsigned long)(&swapper_pg_dir);
}

extern const unsigned long kallsyms_addresses[] __weak;
unsigned long aee_get_kallsyms_addresses(void)
{
	return (unsigned long)&kallsyms_addresses;
}

/* workaround for 32bit kernel, waiting for cache implement */
__weak void __flush_dcache_area(void *addr, size_t len)
{
	pr_info("%s weak function", __func__);
}

extern void __flush_dcache_area(void *addr, size_t len);
void aee__flush_dcache_area(void *addr, size_t len)
{
	__flush_dcache_area(addr, len);
}

void aee_zap_locks(void)
{
	aee_wdt_zap_locks();
}

/* for aee_aed.ko */
const char *aee_arch_vma_name(struct vm_area_struct *vma)
{
	return arch_vma_name(vma);
}
EXPORT_SYMBOL(aee_arch_vma_name);

#endif
