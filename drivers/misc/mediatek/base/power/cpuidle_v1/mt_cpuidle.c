/*
* Copyright (c) 2015 MediaTek Inc.
* Author: Cheng-En Chung <cheng-en.chung@mediatek.com>
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*/

#include <asm/cacheflush.h>
#include <asm/irqflags.h>
#include <asm/neon.h>
#include <asm/psci.h>
#include <asm/suspend.h>

#include <linux/of_address.h>
#include <linux/of.h>
#include <linux/irqchip/mt-gic.h>

#include <mt-plat/mt_dbg.h>
#include <mt-plat/mt_io.h>
#include <mt-plat/sync_write.h>

#include "mt_cpuidle.h"
#include "mt_spm.h"
#include "smp.h"

#include <mach/irqs.h>
#include <mach/mt_spm_mtcmos.h>
#if defined(CONFIG_MTK_RAM_CONSOLE) || defined(CONFIG_TRUSTONIC_TEE_SUPPORT)
#include <mach/mt_secure_api.h>
#endif
#if defined(CONFIG_TRUSTY) && defined(CONFIG_ARCH_MT6580)
#include <mach/mt_trusty_api.h>
#endif

#define TAG "[Power-Dormant] "

#define dormant_err(fmt, args...)	pr_err(TAG fmt, ##args)
#define dormant_warn(fmt, args...)	pr_warn(TAG fmt, ##args)
#define dormant_debug(fmt, args...)	pr_debug(TAG fmt, ##args)

#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M) || defined(CONFIG_ARCH_MT6753)
#define CONFIG_ARCH_MT6735_SERIES
#endif


#ifdef CONFIG_MTK_RAM_CONSOLE
unsigned long *sleep_aee_rec_cpu_dormant_pa;
unsigned long *sleep_aee_rec_cpu_dormant_va;
#endif

#ifdef CONFIG_ARCH_MT6580
static unsigned long mcucfg_base;
static unsigned long infracfg_ao_base;
static unsigned long gic_id_base;
static unsigned long gic_ci_base;

#else
static unsigned long gic_id_base;
#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
static unsigned long biu_base;
#endif /* #if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M) */

#endif /* #ifdef CONFIG_ARCH_MT6580 */

static unsigned int kp_irq_bit;
static unsigned int conn_wdt_irq_bit;
static unsigned int lowbattery_irq_bit;
static unsigned int md1_wdt_irq_bit;
#ifdef CONFIG_MTK_MD3_SUPPORT
#if CONFIG_MTK_MD3_SUPPORT /* Using this to check >0 */
static unsigned int c2k_wdt_irq_bit;
#endif
#endif

#if defined(CONFIG_ARCH_MT6735_SERIES) || defined(CONFIG_ARCH_MT6580)
#define CPUIDLE_CPU_IDLE_STA SPM_SLEEP_TIMER_STA
#define CPUIDLE_CPU_IDLE_STA_OFFSET 16
#define CPUIDLE_SPM_WAKEUP_MISC SPM_SLEEP_WAKEUP_MISC
#define CPUIDLE_SPM_WAKEUP_STA SPM_SLEEP_ISR_RAW_STA
#define CPUIDLE_WAKE_SRC_R12_KP_IRQ_B WAKE_SRC_KP
#define CPUIDLE_WAKE_SRC_R12_CONN_WDT_IRQ_B WAKE_SRC_CONN_WDT
#define CPUIDLE_WAKE_SRC_R12_LOWBATTERY_IRQ_B WAKE_SRC_LOW_BAT
#if defined(CONFIG_ARCH_MT6735_SERIES)
#define CPUIDLE_WAKE_SRC_R12_MD1_WDT_B WAKE_SRC_MD_WDT
#else
#define CPUIDLE_WAKE_SRC_R12_MD1_WDT_B WAKE_SRC_MD1_WDT
#endif
#define CPUIDLE_WAKE_SRC_R12_C2K_WDT_IRQ_B WAKE_SRC_C2K_WDT

#elif defined(CONFIG_ARCH_MT6755) || defined(CONFIG_ARCH_MT6797)
#define CPUIDLE_CPU_IDLE_STA CPU_IDLE_STA
#define CPUIDLE_CPU_IDLE_STA_OFFSET 10
#define CPUIDLE_SPM_WAKEUP_MISC SPM_WAKEUP_MISC
#define CPUIDLE_SPM_WAKEUP_STA SPM_WAKEUP_STA
#define CPUIDLE_WAKE_SRC_R12_KP_IRQ_B WAKE_SRC_R12_KP_IRQ_B
#define CPUIDLE_WAKE_SRC_R12_CONN_WDT_IRQ_B WAKE_SRC_R12_CONN_WDT_IRQ_B
#define CPUIDLE_WAKE_SRC_R12_LOWBATTERY_IRQ_B WAKE_SRC_R12_LOWBATTERY_IRQ_B
#define CPUIDLE_WAKE_SRC_R12_MD1_WDT_B WAKE_SRC_R12_MD1_WDT_B
#define CPUIDLE_WAKE_SRC_R12_C2K_WDT_IRQ_B WAKE_SRC_R12_C2K_WDT_IRQ_B
#endif


#define MAX_CORES 4
#define MAX_CLUSTER 2

#ifdef CONFIG_ARCH_MT6580
#define MP0_CACHE_CONFIG	(mcucfg_base + 0)
#define MP1_CACHE_CONFIG	(mcucfg_base + 0x200)
#define L2RSTDISABLE		BIT(4)

#define DMT_BOOTROM_PWR_CTRL	((void *) (infracfg_ao_base + 0x804))
#define DMT_BOOTROM_BOOT_ADDR	((void *) (infracfg_ao_base + 0x800))
#define SW_ROM_PD		BIT(31)
#endif

#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
#define BIU_CONTROL		(biu_base)
#define CMD_QUEUE_EN		BIT(0)
#define DCM_EN			BIT(1)
#define TLB_ULTRA_EN		BIT(8)
#endif

#define reg_read(addr)		__raw_readl(IOMEM(addr))
#define reg_write(addr, val)	mt_reg_sync_writel(val, addr)
#define _and(a, b)		((a) & (b))
#define _or(a, b)		((a) | (b))
#define _aor(a, b, c)		_or(_and(a, b), (c))

struct core_context {
	volatile u64 timestamp[5];
	unsigned long timer_data[8];
};

struct cluster_context {
	struct core_context core[MAX_CORES] ____cacheline_aligned;
	unsigned long dbg_data[40];
	int l2rstdisable;
	int l2rstdisable_rfcnt;
};

struct system_context {
	struct cluster_context cluster[MAX_CLUSTER];
	struct _data_poc {
		void (*cpu_resume_phys)(void);
		unsigned long l2ctlr;
	} poc ____cacheline_aligned;
};

struct system_context dormant_data[1];
static int mt_dormant_initialized;

#define SPM_CORE_ID() core_idx()
#define SPM_IS_CPU_IRQ_OCCUR(core_id)						\
	({									\
		(!!(spm_read(CPUIDLE_SPM_WAKEUP_MISC) & ((0x101<<(core_id)))));	\
	})

#ifdef CONFIG_MTK_RAM_CONSOLE
#define DORMANT_LOG(cid, pattern) (sleep_aee_rec_cpu_dormant_va[cid] = pattern)
#else
#define DORMANT_LOG(cid, pattern)
#endif

#define core_idx()							\
	({								\
		((read_cluster_id() >> 6) | read_cpu_id());		\
	})

inline void read_id(int *cpu_id, int *cluster_id)
{
	*cpu_id = read_cpu_id();
	*cluster_id = read_cluster_id();
}

#define system_cluster(system, clusterid)	(&((struct system_context *)system)->cluster[clusterid])
#define cluster_core(cluster, cpuid)		(&((struct cluster_context *)cluster)->core[cpuid])

void *_get_data(int core_or_cluster)
{
	int cpuid, clusterid;
	struct cluster_context *cluster;
	struct core_context *core;

	read_id(&cpuid, &clusterid);

	cluster = system_cluster(dormant_data, clusterid);
	if (core_or_cluster == 1)
		return (void *)cluster;

	core = cluster_core(cluster, cpuid);

	return (void *)core;
}

#define GET_CORE_DATA()		((struct core_context *)_get_data(0))
#define GET_CLUSTER_DATA()	((struct cluster_context *)_get_data(1))

void stop_generic_timer(void)
{
	write_cntpctl(read_cntpctl() & ~1);
}

void start_generic_timer(void)
{
	write_cntpctl(read_cntpctl() | 1);
}

#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
static inline void biu_reconfig(void)
{
	int val;

	val = reg_read(BIU_CONTROL);
	val |= TLB_ULTRA_EN;
	val |= DCM_EN;
	val |= CMD_QUEUE_EN;
	reg_write(BIU_CONTROL, val);
}
#endif

struct set_and_clear_regs {
	volatile unsigned int set[32], clear[32];
};

unsigned int __weak *mt_save_dbg_regs(unsigned int *p, unsigned int cpuid)
{
	return p;
}
void __weak mt_restore_dbg_regs(unsigned int *p, unsigned int cpuid) { }
void __weak mt_copy_dbg_regs(int to, int from) { }
void __weak mt_save_banked_registers(unsigned int *container) { }
void __weak mt_restore_banked_registers(unsigned int *container) { }
void __weak mt_gic_cpu_init_for_low_power(void) { }

struct interrupt_distributor {
	volatile unsigned int control;			/* 0x000 */
	const unsigned int controller_type;
	const unsigned int implementer;
	const char padding1[116];
	volatile unsigned int security[32];		/* 0x080 */
	struct set_and_clear_regs enable;		/* 0x100 */
	struct set_and_clear_regs pending;		/* 0x200 */
	struct set_and_clear_regs active;		/* 0x300 */
	volatile unsigned int priority[256];		/* 0x400 */
	volatile unsigned int target[256];		/* 0x800 */
	volatile unsigned int configuration[64];	/* 0xC00 */
	const char padding3[256];			/* 0xD00 */
	volatile unsigned int non_security_access_control[64]; /* 0xE00 */
	volatile unsigned int software_interrupt;	/* 0xF00 */
	volatile unsigned int sgi_clr_pending[4];	/* 0xF10 */
	volatile unsigned int sgi_set_pending[4];	/* 0xF20 */
	const char padding4[176];

	unsigned const int peripheral_id[4];		/* 0xFE0 */
	unsigned const int primecell_id[4];		/* 0xFF0 */
};

static void restore_gic_spm_irq(struct interrupt_distributor *id, int wake_src, int *irq_bit)
{
	int i, j;

	if (reg_read(CPUIDLE_SPM_WAKEUP_STA) & wake_src) {
		i = *irq_bit / GIC_PRIVATE_SIGNALS;
		j = *irq_bit % GIC_PRIVATE_SIGNALS;
		id->pending.set[i] |= (1 << j);
	}
}

static void restore_edge_gic_spm_irq(unsigned long gic_distributor_address)
{
	struct interrupt_distributor *id = (struct interrupt_distributor *) gic_distributor_address;
	unsigned int backup;

	backup = id->control;
	id->control = 0;

	/* Set the pending bit for spm wakeup source that is edge triggerd */
	restore_gic_spm_irq(id, CPUIDLE_WAKE_SRC_R12_KP_IRQ_B, &kp_irq_bit);
	restore_gic_spm_irq(id, CPUIDLE_WAKE_SRC_R12_CONN_WDT_IRQ_B, &conn_wdt_irq_bit);
	restore_gic_spm_irq(id, CPUIDLE_WAKE_SRC_R12_LOWBATTERY_IRQ_B, &lowbattery_irq_bit);
	restore_gic_spm_irq(id, CPUIDLE_WAKE_SRC_R12_MD1_WDT_B, &md1_wdt_irq_bit);
#ifdef CONFIG_MTK_MD3_SUPPORT
#if CONFIG_MTK_MD3_SUPPORT /* Using this to check >0 */
	restore_gic_spm_irq(id, CPUIDLE_WAKE_SRC_R12_C2K_WDT_IRQ_B, &c2k_wdt_irq_bit);
#endif
#endif

	id->control = backup;
}

#if defined(CONFIG_ARCH_MT6580)

struct cpu_interface {
	volatile unsigned int control;			/* 0x00 */
	volatile unsigned int priority_mask;		/* 0x04 */
	volatile unsigned int binary_point;		/* 0x08 */

	volatile unsigned const int interrupt_ack;	/* 0x0c */
	volatile unsigned int end_of_interrupt;		/* 0x10 */

	volatile unsigned const int running_priority;	/* 0x14 */
	volatile unsigned const int highest_pending;	/* 0x18 */
	volatile unsigned int aliased_binary_point;	/* 0x1c */

	volatile unsigned const int aliased_interrupt_ack; /* 0x20 */
	volatile unsigned int alias_end_of_interrupt;	/* 0x24 */
	volatile unsigned int aliased_highest_pending;	/* 0x28 */
};

struct gic_cpu_context {
	unsigned int gic_cpu_if_regs[32];	/* GIC context local to the CPU */
	unsigned int gic_dist_if_pvt_regs[32];	/* GIC SGI/PPI context local to the CPU */
	unsigned int gic_dist_if_regs[512];	/* GIC distributor context to be saved by the last cpu. */
};

struct gic_cpu_context gic_data[1];
#define gic_data_base() ((struct gic_cpu_context *)&gic_data[0])

/*
 * Saves the GIC CPU interface context
 * Requires 3 words of memory
 */
static void save_gic_interface(u32 *pointer, unsigned long gic_interface_address)
{
	struct cpu_interface *ci = (struct cpu_interface *) gic_interface_address;

	pointer[0] = ci->control;
	pointer[1] = ci->priority_mask;
	pointer[2] = ci->binary_point;
	pointer[3] = ci->aliased_binary_point;
	pointer[4] = ci->aliased_highest_pending;
}

/*
 * Saves this CPU's banked parts of the distributor
 * Returns non-zero if an SGI/PPI interrupt is pending (after saving all required context)
 * Requires 19 words of memory
 */
static void save_gic_distributor_private(u32 *pointer,
					 unsigned long gic_distributor_address)
{
	struct interrupt_distributor *id =
	    (struct interrupt_distributor *) gic_distributor_address;
	unsigned int *ptr = 0x0;

	/*  Save SGI,PPI enable status */
	*pointer = id->enable.set[0];
	++pointer;
	/*  Save SGI,PPI priority status */
	pointer = copy_words(pointer, id->priority, 8);
	/*  Save SGI,PPI target status */
	pointer = copy_words(pointer, id->target, 8);
	/*  Save just the PPI configurations (SGIs are not configurable) */
	*pointer = id->configuration[1];
	++pointer;
	/*  Save SGI,PPI security status */
	*pointer = id->security[0];
	++pointer;

	/*  Save SGI Non-security status (PPI is read-only) */
	*pointer = id->non_security_access_control[0] & 0x0ffff;
	++pointer;

	/*  Save SGI,PPI pending status */
	*pointer = id->pending.set[0];
	++pointer;

	/*
	 * IPIs are different and can be replayed just by saving
	 * and restoring the set/clear pending registers
	 */
	ptr = pointer;
	copy_words(pointer, id->sgi_set_pending, 4);
	pointer += 8;

	/*
	 * Clear the pending SGIs on this cpuif so that they don't
	 * interfere with the wfi later on.
	 */
	copy_words(id->sgi_clr_pending, ptr, 4);
}

/*
 * Saves the shared parts of the distributor
 * Requires 1 word of memory, plus 20 words for each block of 32 SPIs (max 641 words)
 * Returns non-zero if an SPI interrupt is pending (after saving all required context)
 */
static void save_gic_distributor_shared(u32 *pointer,
					unsigned long gic_distributor_address)
{
	struct interrupt_distributor *id =
	    (struct interrupt_distributor *) gic_distributor_address;
	unsigned num_spis, *saved_pending;

	/* Calculate how many SPIs the GIC supports */
	num_spis = 32 * (id->controller_type & 0x1f);

	/* TODO: add nonsecure stuff */

	/* Save rest of GIC configuration */
	if (num_spis) {
		pointer =
		    copy_words(pointer, id->enable.set + 1, num_spis / 32);
		pointer = copy_words(pointer, id->priority + 8, num_spis / 4);
		pointer = copy_words(pointer, id->target + 8, num_spis / 4);
		pointer =
		    copy_words(pointer, id->configuration + 2, num_spis / 16);
		pointer = copy_words(pointer, id->security + 1, num_spis / 32);
		saved_pending = pointer;
		pointer =
		    copy_words(pointer, id->pending.set + 1, num_spis / 32);

		pointer =
		    copy_words(pointer, id->non_security_access_control + 1,
			       num_spis / 16);
	}

	/* Save control register */
	*pointer = id->control;
}

static void restore_gic_interface(u32 *pointer, unsigned long gic_interface_address)
{
	struct cpu_interface *ci = (struct cpu_interface *) gic_interface_address;

	ci->priority_mask = pointer[1];
	ci->binary_point = pointer[2];
	ci->aliased_binary_point = pointer[3];

	ci->aliased_highest_pending = pointer[4];

	/* Restore control register last */
	ci->control = pointer[0];
}

static void restore_gic_distributor_private(u32 *pointer,
					    unsigned long gic_distributor_address)
{
	struct interrupt_distributor *id =
	    (struct interrupt_distributor *) gic_distributor_address;
	unsigned tmp;
	/* unsigned ctr, prev_val = 0, prev_ctr = 0; */

	/* First disable the distributor so we can write to its config registers */
	tmp = id->control;
	id->control = 0;
	/* Restore SGI,PPI enable status */
	id->enable.set[0] = *pointer;
	++pointer;
	/* Restore SGI,PPI priority  status */
	copy_words(id->priority, pointer, 8);
	pointer += 8;
	/* Restore SGI,PPI target status */
	copy_words(id->target, pointer, 8);
	pointer += 8;
	/* Restore just the PPI configurations (SGIs are not configurable) */
	id->configuration[1] = *pointer;
	++pointer;
	/* Restore SGI,PPI security status */
	id->security[0] = *pointer;
	++pointer;

	/* restore SGI Non-security status (PPI is read-only) */
	id->non_security_access_control[0] =
	    (id->non_security_access_control[0] & 0x0ffff0000) | (*pointer);
	++pointer;

	/*  Restore SGI,PPI pending status */
	id->pending.set[0] = *pointer;
	++pointer;

	/*
	 * Restore pending SGIs
	 */
	copy_words(id->sgi_set_pending, pointer, 4);
	pointer += 4;

	id->control = tmp;
}

static void restore_gic_distributor_shared(u32 *pointer,
					   unsigned long gic_distributor_address)
{
	struct interrupt_distributor *id = (struct interrupt_distributor *) gic_distributor_address;
	unsigned num_spis;

	/* First disable the distributor so we can write to its config registers */
	id->control = 0;

	/* Calculate how many SPIs the GIC supports */
	num_spis = 32 * ((id->controller_type) & 0x1f);

	/* Restore rest of GIC configuration */
	if (num_spis) {
		copy_words(id->enable.set + 1, pointer, num_spis / 32);
		pointer += num_spis / 32;
		copy_words(id->priority + 8, pointer, num_spis / 4);
		pointer += num_spis / 4;
		copy_words(id->target + 8, pointer, num_spis / 4);
		pointer += num_spis / 4;
		copy_words(id->configuration + 2, pointer, num_spis / 16);
		pointer += num_spis / 16;
		copy_words(id->security + 1, pointer, num_spis / 32);
		pointer += num_spis / 32;
		copy_words(id->pending.set + 1, pointer, num_spis / 32);
		pointer += num_spis / 32;

		copy_words(id->non_security_access_control + 1, pointer,
			   num_spis / 16);
		pointer += num_spis / 16;

		restore_edge_gic_spm_irq(gic_distributor_address);

	}

	/* We assume the I and F bits are set in the CPSR so that we will not respond to interrupts! */
	/* Restore control register */
	id->control = *pointer;
}

static void gic_cpu_save(void)
{
	save_gic_interface(gic_data_base()->gic_cpu_if_regs, gic_ci_base);
	/*
	 * TODO:
	 * Is it safe for the secondary cpu to save its context
	 * while the GIC distributor is on. Should be as its
	 * banked context and the cpu itself is the only one
	 * who can change it. Still have to consider cases e.g
	 * SGIs/Localtimers becoming pending.
	 */
	/* Save distributoer interface private context */
	save_gic_distributor_private(gic_data_base()->gic_dist_if_pvt_regs,
				     gic_id_base);
}

static void gic_cpu_restore(void)
{
	/*restores the private context  */
	restore_gic_distributor_private(gic_data_base()->gic_dist_if_pvt_regs,
					gic_id_base);
	/* Restore GIC context */
	restore_gic_interface(gic_data_base()->gic_cpu_if_regs,
			      gic_ci_base);
}

static void gic_dist_save(void)
{
	/* Save distributoer interface global context */
	save_gic_distributor_shared(gic_data_base()->gic_dist_if_regs,
				    gic_id_base);
}

static void gic_dist_restore(void)
{
	/*restores the global context  */
	restore_gic_distributor_shared(gic_data_base()->gic_dist_if_regs,
				       gic_id_base);
}

DEFINE_SPINLOCK(mp0_l2rstd_lock);
DEFINE_SPINLOCK(mp1_l2rstd_lock);

static inline void mp0_l2rstdisable(int flags)
{
	unsigned int read_back;
	int reg_val;

	spin_lock(&mp0_l2rstd_lock); /* avoid MCDI racing on */

	read_back = reg_read(MP0_CACHE_CONFIG);
	reg_val = _aor(read_back, ~L2RSTDISABLE,
		       IS_DORMANT_INNER_OFF(flags) ? 0 : L2RSTDISABLE);

	reg_write(MP0_CACHE_CONFIG, reg_val);

	if (GET_CLUSTER_DATA()->l2rstdisable_rfcnt++ == 0)
		GET_CLUSTER_DATA()->l2rstdisable = read_back & L2RSTDISABLE;

	spin_unlock(&mp0_l2rstd_lock);
}

static inline void mp1_l2rstdisable(int flags)
{
	unsigned int read_back;
	int reg_val;

	spin_lock(&mp1_l2rstd_lock); /* avoid MCDI racing on */

	read_back = reg_read(MP1_CACHE_CONFIG);
	reg_val =
	    _aor(read_back, ~L2RSTDISABLE,
		 IS_DORMANT_INNER_OFF(flags) ? 0 : L2RSTDISABLE);

	reg_write(MP1_CACHE_CONFIG, reg_val);

	if (GET_CLUSTER_DATA()->l2rstdisable_rfcnt++ == 0)
		GET_CLUSTER_DATA()->l2rstdisable = read_back & L2RSTDISABLE;

	spin_unlock(&mp1_l2rstd_lock);
}

static inline void mp0_l2rstdisable_restore(int flags)
{
	unsigned int read_back;
	int reg_val;

	spin_lock(&mp0_l2rstd_lock); /* avoid MCDI racing on */
	GET_CLUSTER_DATA()->l2rstdisable_rfcnt--;
	if (GET_CLUSTER_DATA()->l2rstdisable_rfcnt == 0) {
		read_back = reg_read(MP0_CACHE_CONFIG);
		reg_val = _aor(read_back, ~L2RSTDISABLE, GET_CLUSTER_DATA()->l2rstdisable);

		reg_write(MP0_CACHE_CONFIG, reg_val);
	}

	spin_unlock(&mp0_l2rstd_lock); /* avoid MCDI racing on */
}

static inline void mp1_l2rstdisable_restore(int flags)
{
	unsigned int read_back;
	int reg_val;

	spin_lock(&mp1_l2rstd_lock); /* avoid MCDI racing on */
	GET_CLUSTER_DATA()->l2rstdisable_rfcnt--;
	if (GET_CLUSTER_DATA()->l2rstdisable_rfcnt == 0) {
		read_back = reg_read(MP1_CACHE_CONFIG);
		reg_val = _aor(read_back, ~L2RSTDISABLE,
			 GET_CLUSTER_DATA()->l2rstdisable);

		reg_write(MP1_CACHE_CONFIG, reg_val);
	}
	spin_unlock(&mp1_l2rstd_lock); /* avoid MCDI racing on */
}

static void mt_cluster_save(int flags)
{
	if (read_cluster_id() == 0)
		mp0_l2rstdisable(flags);
	else
		mp1_l2rstdisable(flags);
}

#endif

static void mt_cluster_restore(int flags)
{
#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
	biu_reconfig();
#elif defined(CONFIG_ARCH_MT6797)
	mt_gic_cpu_init_for_low_power();
#elif defined(CONFIG_ARCH_MT6580)
	if (read_cluster_id() == 0)
		mp0_l2rstdisable_restore(flags);
	else
		mp1_l2rstdisable_restore(flags);
#endif
}

void mt_cpu_save(void)
{
	struct core_context *core;
	struct cluster_context *cluster;
	unsigned int sleep_sta;
	int cpuid, clusterid;

	read_id(&cpuid, &clusterid);

	core = GET_CORE_DATA();

	mt_save_generic_timer((unsigned int *)core->timer_data, 0x0);
	stop_generic_timer();

	if (clusterid == 0)
		sleep_sta = (spm_read(CPUIDLE_CPU_IDLE_STA) >> CPUIDLE_CPU_IDLE_STA_OFFSET) & 0x0f;
	else
		sleep_sta = (spm_read(CPUIDLE_CPU_IDLE_STA) >> (CPUIDLE_CPU_IDLE_STA_OFFSET + 4)) & 0x0f;

	if ((sleep_sta | (1 << cpuid)) == 0x0f) { /* last core */
		cluster = GET_CLUSTER_DATA();
		mt_save_dbg_regs((unsigned int *)cluster->dbg_data, cpuid + (clusterid * 4));
	}
}

void mt_cpu_restore(void)
{
	struct core_context *core;
	struct cluster_context *cluster;
	unsigned int sleep_sta;
	int cpuid, clusterid;

	read_id(&cpuid, &clusterid);

	core = GET_CORE_DATA();

	if (clusterid == 0)
		sleep_sta = (spm_read(CPUIDLE_CPU_IDLE_STA) >> CPUIDLE_CPU_IDLE_STA_OFFSET) & 0x0f;
	else
		sleep_sta = (spm_read(CPUIDLE_CPU_IDLE_STA) >> (CPUIDLE_CPU_IDLE_STA_OFFSET + 4)) & 0x0f;

	sleep_sta = (sleep_sta | (1 << cpuid));

	if (sleep_sta == 0x0f) { /* first core */
		cluster = GET_CLUSTER_DATA();
		mt_restore_dbg_regs((unsigned int *)cluster->dbg_data, cpuid + (clusterid * 4));
	} else {
		int any = __builtin_ffs(~sleep_sta) - 1;

		mt_copy_dbg_regs(cpuid + (clusterid * 4), any + (clusterid * 4));
	}

	mt_restore_generic_timer((unsigned int *)core->timer_data, 0x0);
}

void mt_platform_save_context(int flags)
{
	mt_cpu_save();

#if defined(CONFIG_ARCH_MT6580)
	mt_cluster_save(flags);

	if (IS_DORMANT_GIC_OFF(flags)) {
		gic_cpu_save();
		gic_dist_save();
	}
#endif
}

void mt_platform_restore_context(int flags)
{
	mt_cluster_restore(flags);
	mt_cpu_restore();


#if defined(CONFIG_ARCH_MT6580)
	if (IS_DORMANT_GIC_OFF(flags)) {
		gic_dist_restore();
		gic_cpu_restore();
	}
#else
	if (IS_DORMANT_GIC_OFF(flags))
		restore_edge_gic_spm_irq(gic_id_base);
#endif
}

#if !defined(CONFIG_ARM64) && !defined(CONFIG_ARCH_MT6580)
int mt_cpu_dormant_psci(unsigned long flags)
{
	int ret = 1;
	int cpuid, clusterid;

	struct psci_power_state pps = {
		.type = PSCI_POWER_STATE_TYPE_POWER_DOWN,
		.affinity_level = 1,
	};

	read_id(&cpuid, &clusterid);

	if (psci_ops.cpu_suspend) {
		DORMANT_LOG(clusterid * MAX_CORES + cpuid, 0x203);
		ret = psci_ops.cpu_suspend(pps, virt_to_phys(cpu_resume));
	}

	BUG();

	return ret;
}
#endif

#if defined(CONFIG_ARCH_MT6580)
int mt_cpu_dormant_reset(unsigned long flags)
{
	int ret = 1; /* dormant abort */

	int cpuid, clusterid;

	read_id(&cpuid, &clusterid);

	disable_dcache_safe(!!IS_DORMANT_INNER_OFF(flags));

	amp();

	DORMANT_LOG(clusterid * 4 + cpuid, 0x301);

	wfi();

	smp();

	DORMANT_LOG(clusterid * 4 + cpuid, 0x302);

	__enable_dcache();

	DORMANT_LOG(clusterid * 4 + cpuid, 0x303);

	return ret;
}
#endif

static int mt_cpu_dormant_abort(unsigned long index)
{
#if defined(CONFIG_ARCH_MT6580)
	int cpuid, clusterid;

	read_id(&cpuid, &clusterid);

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
	if (cpuid == 0)
		mt_secure_call(MC_FC_SLEEP_CANCELLED, 0, 0, 0);
#elif defined(CONFIG_TRUSTY) && defined(CONFIG_ARCH_MT6580)
	if (cpuid == 0)
		mt_trusty_call(SMC_FC_CPU_DORMANT_CANCEL, 0, 0, 0);
#endif

	/* restore l2rstdisable setting */
	if (read_cluster_id() == 0)
		mp0_l2rstdisable_restore(index);
	else
		mp1_l2rstdisable_restore(index);

#endif

#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
	biu_reconfig();
#endif

	start_generic_timer();

	return 0;
}

int mt_cpu_dormant(unsigned long flags)
{
	int ret;
	int cpuid, clusterid;

	if (!mt_dormant_initialized)
		return MT_CPU_DORMANT_BYPASS;

	read_id(&cpuid, &clusterid);

	DORMANT_LOG(clusterid * MAX_CORES + cpuid, 0x101);

	BUG_ON(!irqs_disabled());

	/* to mark as cpu clobs vfp register.*/
	kernel_neon_begin();

	mt_platform_save_context(flags);

	DORMANT_LOG(clusterid * MAX_CORES + cpuid, 0x102);

#if !defined(CONFIG_ARM64) && !defined(CONFIG_ARCH_MT6580)
	ret = cpu_suspend(flags, mt_cpu_dormant_psci);
#elif !defined(CONFIG_ARCH_MT6580)
	ret = cpu_suspend(2);
#else
	dormant_data[0].poc.cpu_resume_phys = (void (*)(void))(long)virt_to_phys(cpu_resume);
#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
	mt_secure_call(MC_FC_MTK_SLEEP, virt_to_phys(cpu_resume), cpuid, 0);
#elif defined(CONFIG_TRUSTY) && defined(CONFIG_ARCH_MT6580)
	mt_trusty_call(SMC_FC_CPU_DORMANT, virt_to_phys(cpu_resume), cpuid, 0);
#else
	writel_relaxed(virt_to_phys(cpu_resume), DMT_BOOTROM_BOOT_ADDR);
#endif
	ret = cpu_suspend(flags, mt_cpu_dormant_reset);
#endif
	DORMANT_LOG(clusterid * MAX_CORES + cpuid, 0x601);

#if defined(CONFIG_ARCH_MT6580)
	if (IS_DORMANT_INNER_OFF(flags)) {
		reg_write(DMT_BOOTROM_BOOT_ADDR, virt_to_phys(cpu_wake_up_errata_802022));

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
		mt_secure_call(MC_FC_SET_RESET_VECTOR, virt_to_phys(cpu_wake_up_errata_802022), 1, 0);
		if (num_possible_cpus() == 4) {
			mt_secure_call(MC_FC_SET_RESET_VECTOR, virt_to_phys(cpu_wake_up_errata_802022), 2, 0);
			mt_secure_call(MC_FC_SET_RESET_VECTOR, virt_to_phys(cpu_wake_up_errata_802022), 3, 0);
		}
#elif defined(CONFIG_TRUSTY) && defined(CONFIG_ARCH_MT6580)
		mt_trusty_call(SMC_FC_CPU_ON, virt_to_phys(cpu_wake_up_errata_802022), 1, 1);
		if (num_possible_cpus() == 4) {
			mt_trusty_call(SMC_FC_CPU_ON, virt_to_phys(cpu_wake_up_errata_802022), 2, 1);
			mt_trusty_call(SMC_FC_CPU_ON, virt_to_phys(cpu_wake_up_errata_802022), 3, 1);
		}
#endif
		spm_mtcmos_ctrl_cpu1(STA_POWER_ON, 1);
		if (num_possible_cpus() == 4) {
			spm_mtcmos_ctrl_cpu2(STA_POWER_ON, 1);
			spm_mtcmos_ctrl_cpu3(STA_POWER_ON, 1);

			spm_mtcmos_ctrl_cpu3(STA_POWER_DOWN, 1);
			spm_mtcmos_ctrl_cpu2(STA_POWER_DOWN, 1);
		}
		spm_mtcmos_ctrl_cpu1(STA_POWER_DOWN, 1);

#ifdef CONFIG_TRUSTONIC_TEE_SUPPORT
		mt_secure_call(MC_FC_ERRATA_808022, 0, 0, 0);
#elif defined(CONFIG_TRUSTY) && defined(CONFIG_ARCH_MT6580)
		mt_trusty_call(SMC_FC_CPU_ERRATA_802022, 0, 0, 0);
#endif
	}
#endif

	switch (ret) {
	case 0: /* back from dormant reset */
		mt_platform_restore_context(flags);
		ret = MT_CPU_DORMANT_RESET;
		break;

	case 1: /* back from dormant abort, */
		mt_cpu_dormant_abort(flags);
		ret = MT_CPU_DORMANT_ABORT;
		break;
	case 2:
		mt_cpu_dormant_abort(flags);
		ret = MT_CPU_DORMANT_BREAK_V(IRQ_PENDING_3);
		break;
	default: /* back from dormant break, do nothing for return */
		dormant_err("EOPNOTSUPP\n");
		break;
	}

	DORMANT_LOG(clusterid * MAX_CORES + cpuid, 0x602);

	local_fiq_enable();

	kernel_neon_end();

	DORMANT_LOG(clusterid * MAX_CORES + cpuid, 0x0);

	return ret & 0x0ff;
}

static unsigned long get_dts_node_address(char *node_compatible, int index)
{
	unsigned long node_address = 0;
	struct device_node *node;

	if (!node_compatible)
		return 0;

	node = of_find_compatible_node(NULL, NULL, node_compatible);
	if (!node) {
		dormant_err("error: cannot find node [%s]\n", node_compatible);
		BUG();
	}
	node_address = (unsigned long)of_iomap(node, index);
	if (!node_address) {
		dormant_err("error: cannot iomap [%s]\n", node_compatible);
		BUG();
	}
	of_node_put(node);

	return node_address;
}

static u32 get_dts_node_irq_bit(char *node_compatible, const int int_size, int int_offset)
{
	struct device_node *node;
	u32 node_interrupt[int_size];
	unsigned int irq_bit;

	if (!node_compatible)
		return 0;

	node = of_find_compatible_node(NULL, NULL, node_compatible);
	if (!node) {
		dormant_err("error: cannot find node [%s]\n", node_compatible);
		BUG();
	}
	if (of_property_read_u32_array(node, "interrupts", node_interrupt, int_size)) {
		dormant_err("error: cannot property_read [%s]\n", node_compatible);
		BUG();
	}
	/* irq[0] = 0 => spi */
	irq_bit = ((1 - node_interrupt[int_offset]) << 5) + node_interrupt[int_offset+1];
	of_node_put(node);
	dormant_debug("compatible = %s, irq_bit = %u\n", node_compatible, irq_bit);

	return irq_bit;
}

#ifdef CONFIG_ARCH_MT6580
static void get_dts_nodes_address(void)
{
	mcucfg_base = get_dts_node_address("mediatek,mt6580-mcucfg", 0);
	infracfg_ao_base = get_dts_node_address("mediatek,INFRACFG_AO", 0);
	gic_id_base = get_dts_node_address("arm,cortex-a7-gic", 0);
	gic_ci_base = get_dts_node_address("arm,cortex-a7-gic", 1);
}

static void get_dts_nodes_irq_bit(void)
{
	kp_irq_bit = get_dts_node_irq_bit("mediatek,mt6580-keypad", 3, 0);
	conn_wdt_irq_bit = get_dts_node_irq_bit("mediatek,mt6580-consys", 6, 3);
	lowbattery_irq_bit = get_dts_node_irq_bit("mediatek,mt6735-auxadc", 3, 0);
	md1_wdt_irq_bit = get_dts_node_irq_bit("mediatek,ap_ccif0", 6, 3);
}
#elif defined(CONFIG_ARCH_MT6735_SERIES)
static void get_dts_nodes_address(void)
{
#if defined(CONFIG_ARCH_MT6735) || defined(CONFIG_ARCH_MT6735M)
	biu_base = get_dts_node_address("mediatek,mt6735-mcu_biu", 0);
#endif
	gic_id_base = get_dts_node_address("mediatek,mt6735-gic", 0);
}

static void get_dts_nodes_irq_bit(void)
{
	kp_irq_bit = get_dts_node_irq_bit("mediatek,mt6735-keypad", 3, 0);
	conn_wdt_irq_bit = get_dts_node_irq_bit("mediatek,mt6735-consys", 6, 3);
	lowbattery_irq_bit = get_dts_node_irq_bit("mediatek,mt6735-auxadc", 3, 0);
	md1_wdt_irq_bit = get_dts_node_irq_bit("mediatek,mdcldma", 9, 6);
#ifdef CONFIG_MTK_MD3_SUPPORT
#if CONFIG_MTK_MD3_SUPPORT /* Using this to check >0 */
	c2k_wdt_irq_bit = get_dts_node_irq_bit("mediatek,mdc2k", 3, 0);
#endif
#endif
}
#elif defined(CONFIG_ARCH_MT6755)
static void get_dts_nodes_address(void)
{
	gic_id_base = get_dts_node_address("mediatek,mt6735-gic", 0);
}

static void get_dts_nodes_irq_bit(void)
{
	kp_irq_bit = get_dts_node_irq_bit("mediatek,mt6755-keypad", 3, 0);
	conn_wdt_irq_bit = get_dts_node_irq_bit("mediatek,mt6755-consys", 6, 3);
	lowbattery_irq_bit = get_dts_node_irq_bit(NULL, 3, 0);
	md1_wdt_irq_bit = get_dts_node_irq_bit("mediatek,mdcldma", 9, 6);
#ifdef CONFIG_MTK_MD3_SUPPORT
#if CONFIG_MTK_MD3_SUPPORT /* Using this to check >0 */
	c2k_wdt_irq_bit = get_dts_node_irq_bit("mediatek,ap2c2k_ccif", 6, 3);
#endif
#endif
}
#elif defined(CONFIG_ARCH_MT6797)
static void get_dts_nodes_address(void)
{
	gic_id_base = get_dts_node_address("arm,gic-v3", 0);
}

static void get_dts_nodes_irq_bit(void)
{
	kp_irq_bit = get_dts_node_irq_bit("mediatek,mt6797-keypad", 3, 0);
	conn_wdt_irq_bit = get_dts_node_irq_bit("mediatek,mt6797-consys", 6, 3);
	lowbattery_irq_bit = get_dts_node_irq_bit("mediatek,mt6797-auxadc", 3, 0);
	md1_wdt_irq_bit = get_dts_node_irq_bit("mediatek,mdcldma", 9, 6);
#ifdef CONFIG_MTK_MD3_SUPPORT
#if CONFIG_MTK_MD3_SUPPORT /* Using this to check >0 */
	c2k_wdt_irq_bit = get_dts_node_irq_bit("mediatek,ap2c2k_ccif", 6, 3);
#endif
#endif
}
#endif

static int mt_dormant_dts_map(void)
{
	get_dts_nodes_address();
	get_dts_nodes_irq_bit();

	return 0;
}

int mt_cpu_dormant_init(void)
{
	int cpuid, clusterid;

	read_id(&cpuid, &clusterid);

	if (mt_dormant_initialized == 1)
		return MT_CPU_DORMANT_BYPASS;

	mt_dormant_dts_map();

#if defined(CONFIG_ARCH_MT6580)
	/* enable bootrom power down mode */
	reg_write(DMT_BOOTROM_PWR_CTRL, reg_read(DMT_BOOTROM_PWR_CTRL) | SW_ROM_PD);

	mt_save_l2ctlr(dormant_data[0].poc.l2ctlr);
#endif

#ifdef CONFIG_MTK_RAM_CONSOLE
	sleep_aee_rec_cpu_dormant_va = aee_rr_rec_cpu_dormant();
	sleep_aee_rec_cpu_dormant_pa = aee_rr_rec_cpu_dormant_pa();

	BUG_ON(!sleep_aee_rec_cpu_dormant_va || !sleep_aee_rec_cpu_dormant_pa);

#if !defined(CONFIG_ARCH_MT6580)
#if defined(CONFIG_ARM_PSCI) || defined(CONFIG_MTK_PSCI)
	kernel_smc_msg(0, 2, (long) sleep_aee_rec_cpu_dormant_pa);
#endif
#endif

	dormant_debug("init aee_rec_cpu_dormant: va:%p pa:%p\n",
		    sleep_aee_rec_cpu_dormant_va, sleep_aee_rec_cpu_dormant_pa);
#endif

	mt_dormant_initialized = 1;

	return 0;
}
