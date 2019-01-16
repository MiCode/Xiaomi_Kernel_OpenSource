#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/poll.h>
#include <linux/delay.h>
#include <linux/sysrq.h>
#include <linux/proc_fs.h>
#include <linux/pm.h>
#include <linux/device.h>
#include <linux/suspend.h>
#include <linux/mm.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/tlbflush.h>
#include <asm/cacheflush.h>
#include <asm/system_misc.h>
#include <asm/mmu.h>
#include <asm/suspend.h>
#include <mach/mt_hibernate.h>

extern unsigned *mt_save_generic_timer(unsigned int *container, int sw);
extern void mt_restore_generic_timer(int *container, int sw);
extern unsigned int *mt_save_banked_registers(unsigned int *container);
extern void mt_restore_banked_registers(unsigned int *container);
#ifdef CONFIG_MTK_ETM
extern void trace_start_dormant(void);
#endif

typedef struct fault_regs {
    unsigned dfar;
    unsigned ifar;
    unsigned ifsr;
    unsigned dfsr;
    unsigned adfsr;
    unsigned aifsr;
} cp15_fault_regs;

typedef struct ns_banked_cp15_context {
    unsigned int cp15_misc_regs[2]; /* cp15 miscellaneous registers */
    unsigned int cp15_ctrl_regs[20];    /* cp15 control registers */
    unsigned int cp15_mmu_regs[16]; /* cp15 mmu registers */
    cp15_fault_regs ns_cp15_fault_regs; /* cp15 fault status registers */
} banked_cp15_context;

typedef struct ns_hib_core_context {
	unsigned int banked_regs[32];
	unsigned int timer_data[8]; /* Global timers if the NS world has access to them */
} hib_core_context;

static banked_cp15_context saved_cp15_context;
static hib_core_context saved_core_context;

extern const void __nosave_begin, __nosave_end;
#define __pa_symbol(x)__pa(RELOC_HIDE((unsigned long)(x), 0))
int pfn_is_nosave(unsigned long pfn)
{
    unsigned long begin_pfn = __pa_symbol(&__nosave_begin) >> PAGE_SHIFT;
    unsigned long end_pfn = PAGE_ALIGN(__pa_symbol(&__nosave_end)) >> PAGE_SHIFT;

    return (pfn >= begin_pfn) && (pfn < end_pfn);
}

static void __save_processor_state(struct ns_banked_cp15_context *ctxt)
{
	/* save preempt state and disable it */
	preempt_disable();

	mt_save_generic_timer(saved_core_context.timer_data, 0x0);
	mt_save_banked_registers(saved_core_context.banked_regs);
}

void notrace save_processor_state(void)
{
    __save_processor_state(&saved_cp15_context);
}
EXPORT_SYMBOL(save_processor_state);

static void __restore_processor_state(struct ns_banked_cp15_context *ctxt)
{
	mt_restore_banked_registers(saved_core_context.banked_regs);
	mt_restore_generic_timer(saved_core_context.timer_data, 0x0);

    // restore ETM module
#ifdef CONFIG_MTK_ETM
    trace_start_dormant();
#endif
	/* restore preempt state */
	preempt_enable();
}

void notrace restore_processor_state(void)
{
    __restore_processor_state(&saved_cp15_context);
}
EXPORT_SYMBOL(restore_processor_state);

extern void cpu_resume(void);
typedef void (*phys_reset_t)(unsigned long);
static int swsusp_saved = 0;

/*
 * Snapshot kernel memory and reset the system.
 * After resume, the hibernation snapshot is written out.
 */
int notrace swsusp_arch_save_image(unsigned long unused)
{
	extern int swsusp_save(void);
	int ret;

	ret = swsusp_save();
	swsusp_saved = (ret == 0) ? 1 : 0;
	return ret;
}

/*
 * Save the current CPU state before suspend / poweroff.
 */
int notrace swsusp_arch_suspend(void)
{
	int retval = 0;

	retval = cpu_suspend(POWERMODE_HIBERNATE);
	if (swsusp_saved)
		retval = 0;

	return retval;
}

/*
 * The framework loads the hibernation image into a linked list anchored
 * at restore_pblist, for swsusp_arch_resume() to copy back to the proper
 * destinations.
 *
 * To make this work if resume is triggered from initramfs, the
 * pagetables need to be switched to allow writes to kernel mem.
 */
static void notrace __swsusp_arch_restore_image(void *unused)
{
	extern void __disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2(void);
	extern struct pbe *restore_pblist;
	phys_reset_t phys_reset;
	struct pbe *pbe;

	for (pbe = restore_pblist; pbe; pbe = pbe->next)
		copy_page(pbe->orig_address, pbe->address);

#if 0 // [ALPS01496758]  since CA17 has cache bug, replace with the modified assemlby version
	/* Clean and invalidate caches */
	flush_cache_all();

	/* Turn D-cache off */
	cpu_cache_off();

	/* Push out any further dirty data, and ensure cache is empty */
	flush_cache_all();
#else
	__disable_dcache__inner_flush_dcache_L1__inner_flush_dcache_L2();
#endif

	/* Take out a flat memory mapping. */
	setup_mm_for_reboot();
	phys_reset = (phys_reset_t)(unsigned long)virt_to_phys(cpu_reset);
	/* Return from cpu_suspend/swsusp_arch_suspend */
	phys_reset((unsigned long)virt_to_phys(cpu_resume));

	/* Should never get here. */
	BUG();
}

__attribute__((aligned(16))) static u8 __swsusp_resume_stk[PAGE_SIZE/2] __nosavedata;

/*
 * Resume from the hibernation image.
 * Due to the kernel heap / data restore, stack contents change underneath
 * and that would make function calls impossible; switch to a temporary
 * stack within the nosave region to avoid that problem.
 */
int swsusp_arch_resume(void)
{
	extern void call_with_stack(void (*fn)(void *), void *arg, void *sp);
#if 0 /* FIX ME: cpu_init in arm64 implementation */
	cpu_init();	/* get a clean PSR */
#endif
	call_with_stack(__swsusp_arch_restore_image, 0,
		__swsusp_resume_stk + sizeof(__swsusp_resume_stk));
	return 0;
}
