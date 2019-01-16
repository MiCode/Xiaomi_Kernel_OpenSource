/*
 * ARM Cortex-A7 save/restore for suspend to disk (Hibernation)
 */
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
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/tlbflush.h>
#include <asm/suspend.h>

extern void save_cp15(void *pointer);
extern void save_control_registers(void *pointer, int is_secure);
extern void save_mmu(void *pointer);
extern void save_fault_status(void *pointer);
extern void save_generic_timer(void *pointer, int is_hyp);
extern void restore_control_registers(void *pointer, int is_secure);
extern void restore_cp15(void *pointer);
extern void restore_mmu(void *pointer);
extern void restore_fault_status(void *pointer);
extern void restore_generic_timer(void *pointer, int is_hyp);
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

typedef struct gen_tmr_ctx {
    unsigned cntfrq;
    unsigned long long cntvoff;
    unsigned cnthctl;
    unsigned cntkctl;
    unsigned long long cntp_cval;
    unsigned cntp_tval;
    unsigned cntp_ctl;
    unsigned long long cntv_cval;
    unsigned cntv_tval;
    unsigned cntv_ctl;
    unsigned long long cnthp_cval;
    unsigned cnthp_tval;
    unsigned cnthp_ctl;
} generic_timer_context;


static banked_cp15_context saved_cp15_context;
static generic_timer_context saved_cp15_timer_ctx;

static void __save_processor_state(struct ns_banked_cp15_context *ctxt)
{
	/* save preempt state and disable it */
	preempt_disable();

    // The 32-bit Generic timer context
    save_generic_timer(&saved_cp15_timer_ctx, 0x0);

    save_cp15(ctxt->cp15_misc_regs);
    save_control_registers(ctxt->cp15_ctrl_regs, 0x0);
    save_mmu(ctxt->cp15_mmu_regs);
    save_fault_status(&ctxt->ns_cp15_fault_regs);
}

void notrace save_processor_state(void)
{
    __save_processor_state(&saved_cp15_context);
}

static void __restore_processor_state(struct ns_banked_cp15_context *ctxt)
{
    restore_fault_status(&ctxt->ns_cp15_fault_regs);
    restore_mmu(ctxt->cp15_mmu_regs);
    restore_control_registers(ctxt->cp15_ctrl_regs, 0x0);
    restore_cp15(ctxt->cp15_misc_regs);

    // The 32-bit Generic timer context
    restore_generic_timer(&saved_cp15_timer_ctx, 0x0);

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

EXPORT_SYMBOL(save_processor_state);
EXPORT_SYMBOL(restore_processor_state);
extern const void __nosave_begin, __nosave_end;

#define __pa_symbol(x)__pa(RELOC_HIDE((unsigned long)(x), 0))
int pfn_is_nosave(unsigned long pfn)
{
    unsigned long begin_pfn = __pa_symbol(&__nosave_begin) >> PAGE_SHIFT;
    unsigned long end_pfn = PAGE_ALIGN(__pa_symbol(&__nosave_end)) >> PAGE_SHIFT;

    return (pfn >= begin_pfn) && (pfn < end_pfn);
}
