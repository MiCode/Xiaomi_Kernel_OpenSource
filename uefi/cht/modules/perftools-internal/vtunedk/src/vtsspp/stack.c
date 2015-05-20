/*
  Copyright (C) 2010-2014 Intel Corporation.  All Rights Reserved.

  This file is part of SEP Development Kit

  SEP Development Kit is free software; you can redistribute it
  and/or modify it under the terms of the GNU General Public License
  version 2 as published by the Free Software Foundation.

  SEP Development Kit is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with SEP Development Kit; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA

  As a special exception, you may use this file as part of a free software
  library without restriction.  Specifically, if other files instantiate
  templates or use macros or inline functions from this file, or you compile
  this file and link it with other files to produce an executable, this
  file does not by itself cause the resulting executable to be covered by
  the GNU General Public License.  This exception does not however
  invalidate any other reasons why the executable file might be covered by
  the GNU General Public License.
*/
#include "vtss_config.h"
#include "stack.h"
#include "regs.h"
#include "globals.h"
#include "record.h"
#include "user_vm.h"
#include "time.h"
#include "lbr.h"

#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/highmem.h>      /* for kmap()/kunmap() */
#include <linux/pagemap.h>      /* for page_cache_release() */
#include <asm/page.h>
#include <asm/processor.h>
#include <linux/nmi.h>
#include <linux/module.h>
#include "unwind.c"


#if defined(CONFIG_X86_64)
const unsigned long vtss_koffset = (unsigned long)__START_KERNEL_map;
const unsigned long vtss_max_user_space = 0x7fffffffffff;
#else
const unsigned long vtss_koffset = (unsigned long)PAGE_OFFSET;
const unsigned long vtss_max_user_space = 0x7fffffff;
#endif

#if defined(CONFIG_X86_64)
const unsigned long vtss_kstart = (unsigned long)__START_KERNEL_map + ((CONFIG_PHYSICAL_START + (CONFIG_PHYSICAL_ALIGN - 1)) & ~(CONFIG_PHYSICAL_ALIGN - 1));
#else
const unsigned long vtss_kstart = (unsigned long)PAGE_OFFSET + ((CONFIG_PHYSICAL_START + (CONFIG_PHYSICAL_ALIGN - 1)) & ~(CONFIG_PHYSICAL_ALIGN - 1));
#endif

#include <asm/stacktrace.h>

#define VTSS_MOD(mod_rm) ((mod_rm) >> 6)
#define VTSS_RM(mod_rm) ((mod_rm) & 7)
#define VTSS_SIB(mod_rm) ((VTSS_MOD(mod_rm) != 3) && (VTSS_RM(mod_rm) == 4))
#define VTSS_SIB_BASE(sib) ((sib) & 7)

#if defined(CONFIG_X86_64)
#define VTSS_INDIRECT_CALL(loc) (((loc)[0] == 0xff) && ((((loc)[1] >> 3) & 7) == 2))
#else
#define VTSS_INDIRECT_CALL(loc) (((loc)[0] == 0xff) && (((((loc)[1] >> 3) & 7) == 2) || \
                                 ((((loc)[1] >> 3) & 7) == 3)))
#endif
/* number of bytes for displacement */
static unsigned char displacement (unsigned char mod_rm)
{
    switch (VTSS_MOD(mod_rm))
    {
    case 0:
        if (VTSS_RM(mod_rm) == 5)
        {
            return 4;
        }
        else
        {
           return 0;
        }
    case 1:
        return 1;
    case 2:
        return 4;
    case 3:
        return 0;
    default:
        return 0;
    }
}
#define VTSS_IS_NEAR_DIRECT_CALL(loc) ((*((loc) - 3) == 0xe8) || (*((loc) - 5) == 0xe8))

#if defined(CONFIG_X86_64)
#define VTSS_IS_FAR_DIRECT_CALL(loc) (0)
#else
#define VTSS_IS_FAR_DIRECT_CALL(loc) ((*((loc) - 4) == 0x9a) || (*((loc) - 6) == 0x9a))
#endif

#define VTSS_IS_INDIRECT_CALL2(loc) \
    (VTSS_INDIRECT_CALL((loc) - 2) && !VTSS_SIB(loc[-1]) && !displacement((loc)[-1]))
    
#define VTSS_IS_INDIRECT_CALL3(loc) \
    (VTSS_INDIRECT_CALL((loc) - 3) && \
    ((VTSS_SIB((loc)[-2]) && !displacement((loc)[-2])) || (!VTSS_SIB((loc)[-2]) && displacement((loc)[-2]) == 1)))
#define VTSS_IS_INDIRECT_CALL4(loc) \
    (VTSS_INDIRECT_CALL((loc) - 4) && \
    ((VTSS_SIB((loc)[-3]) && displacement((loc)[-3]) == 1) || \
    ((VTSS_SIB((loc)[-3])) && ((VTSS_SIB_BASE((loc)[-2]) == 5) && (VTSS_MOD((loc)[-3]) == 1)))))
#define VTSS_IS_INDIRECT_CALL6(loc) \
    (VTSS_INDIRECT_CALL((loc) - 6) && \
    (!VTSS_SIB((loc)[-5]) && displacement((loc)[-5]) == 4))
#define VTSS_IS_INDIRECT_CALL7(loc) \
    (VTSS_INDIRECT_CALL((loc) - 7) && \
    ((VTSS_SIB((loc)[-6]) && displacement((loc)[-6]) == 4) || \
    (VTSS_SIB((loc)[-6]) && ((VTSS_SIB_BASE((loc)[-5]) == 5) && ((VTSS_MOD((loc)[-6]) == 0) || \
                                                                (VTSS_MOD((loc)[-6]) == 2))))))
#define VTSS_CHECK_PREVIOUS_CALL_INSTRUCTION(loc) \
    (VTSS_IS_NEAR_DIRECT_CALL((loc)) || VTSS_IS_FAR_DIRECT_CALL((loc)) || \
     VTSS_IS_INDIRECT_CALL2((loc)) || VTSS_IS_INDIRECT_CALL3((loc)) || \
     VTSS_IS_INDIRECT_CALL4((loc)) || VTSS_IS_INDIRECT_CALL6((loc)) || \
     VTSS_IS_INDIRECT_CALL7((loc)))

#ifdef VTSS_AUTOCONF_STACKTRACE_OPS_WARNING
static void vtss_warning(void *data, char *msg)
{
}

static void vtss_warning_symbol(void *data, char *msg, unsigned long symbol)
{
}
#endif

typedef struct kernel_stack_control_t
{
    unsigned long bp;
    unsigned char* kernel_callchain;
    int* kernel_callchain_size;
    int* kernel_callchain_pos;
    unsigned long prev_addr;
    int done;
//    struct vtss_transport_data* trnd;
} kernel_stack_control_t;


static int vtss_stack_stack(void *data, char *name)
{
    kernel_stack_control_t* stk = (kernel_stack_control_t*)data;
    if (!stk) return -1;
    if (stk->done){
        ERROR("Error happens during stack processing");
        return -1;
    }
    return 0;
}

static void vtss_stack_address(void *data, unsigned long addr, int reliable)
{
    unsigned long addr_diff;
    int sign;
    char prefix = 0;
    int j;
    kernel_stack_control_t* stk = (kernel_stack_control_t*)data;
    TRACE("%s%pB %d", reliable ? "" : "? ", (void*)addr, *stk->kernel_callchain_pos);
    touch_nmi_watchdog();
    if (!reliable){
#if 0
//#ifndef CONFIG_FRAME_POINTER
     INFO ("addr = %lx", addr);
     if (addr < vtss_kstart+7){
         INFO("addr < kstart (%lx<%lx)", addr, vtss_kstart);
         return;
     }
     if ((__virt_addr_valid(addr)&&__virt_addr_valid(addr-7))||(__module_text_address(addr)&&__module_text_address(addr-7)))
     {
         unsigned char* loc = (unsigned char*)addr;
         if (!VTSS_CHECK_PREVIOUS_CALL_INSTRUCTION((unsigned char*)addr)){
             INFO("!prev instr is not a call");
             return;
         }
         INFO("value = %2x%2x%2x%2x%2x%2x%2x%2x", (int)loc[-7], (int)loc[-6], (int)loc[-5], (int)loc[-4], (int)loc[-3], (int)loc[-2], (int)loc[-1], (int)loc[0]);
         dump_stack();
     }else{
         INFO("!virt_addr_valid");
         return;
     }
#else
     return;
#endif
    }
    if (!stk || !stk->kernel_callchain_size || !stk->kernel_callchain_pos){
        return;
    }
    if ((*stk->kernel_callchain_size) <= (*stk->kernel_callchain_pos)) {
        return;
    }
#ifndef CONFIG_FRAME_POINTER
    if (addr < vtss_koffset) return;
#endif
    if (stk->done) return;
    addr_diff = addr - stk->prev_addr;
    sign = (addr_diff & (((size_t)1) << ((sizeof(size_t) << 3) - 1))) ? 0xff : 0;
    for (j = sizeof(void*) - 1; j >= 0; j--)
    {
        if(((addr_diff >> (j << 3)) & 0xff) != sign)
        {
           break;
        }
    }
    prefix |= sign ? 0x40 : 0;
    prefix |= j + 1;

    if ((*stk->kernel_callchain_size) <= (*stk->kernel_callchain_pos)+1+j+1) {
        return;
    }

    stk->kernel_callchain[*stk->kernel_callchain_pos] = prefix;
    (*stk->kernel_callchain_pos)++;

    *(unsigned long*)&(stk->kernel_callchain[*stk->kernel_callchain_pos]) = addr_diff;
    (*stk->kernel_callchain_pos) += j + 1;
    stk->prev_addr = addr;
}

#if defined(VTSS_AUTOCONF_STACKTRACE_OPS_WALK_STACK)
static unsigned long vtss_stack_walk(
    struct thread_info *tinfo,
    unsigned long *stack,
    unsigned long bp,
    const struct stacktrace_ops *ops,
    void *data,
    unsigned long *end,
    int *graph)
{
    kernel_stack_control_t* stk = (kernel_stack_control_t*)data;
//    unsigned long* pbp = &stk->bp;
    if (!stk){
        ERROR("Internal error!");
        stk->done=1;
        return bp;
    }
    if (!stack){
        ERROR("Broken stack pointer!");
        stk->done=1;
        return bp;
    }
    if (stack <= (unsigned long*)vtss_max_user_space){
//        char dbgmsg[100];
//        snprintf(dbgmsg, sizeof(dbgmsg)-1, "vtss_stack_walk: stack_ptr=%p, vtss_koffset=%lx, vtss_kstart=%lx", stack, vtss_koffset, vtss_kstart);
//        vtss_record_debug_info(stk->trnd, dbgmsg, 0);
        ERROR("Stack pointer belongs user space. We will not process it. stack_ptr=%p", stack);
        if (stk->kernel_callchain_pos && *stk->kernel_callchain_pos == 0){
            ERROR("Most probably stack pointer intitialization is wrong. No one stack address is resolved.");
        }
        stk->done=1;
        return bp;
    }
    TRACE("bp=0x%p, stack=0x%p, end=0x%p", (void*)stk->bp, stack, end);
    if (stk->done){
        return bp;
    }
    bp = print_context_stack(tinfo, stack, stk->bp, ops, data, end, graph);
    if (stk != NULL && bp < vtss_kstart) {
        TRACE("user bp=0x%p", (void*)bp);
        stk->bp = bp;
    }
    return bp;
}
#endif

static const struct stacktrace_ops vtss_stack_ops = {
#ifdef VTSS_AUTOCONF_STACKTRACE_OPS_WARNING
    .warning        = vtss_warning,
    .warning_symbol = vtss_warning_symbol,
#endif
    .stack          = vtss_stack_stack,
    .address        = vtss_stack_address,
#if defined(VTSS_AUTOCONF_STACKTRACE_OPS_WALK_STACK)
    .walk_stack     = vtss_stack_walk,
#endif
};

int vtss_stack_dump(struct vtss_transport_data* trnd, stack_control_t* stk, struct task_struct* task, struct pt_regs* regs_in, void* reg_fp, int in_irq)
{
    int rc;
    user_vm_accessor_t* acc;
    void* stack_base = stk->bp.vdp;
    void *reg_ip, *reg_sp;
    int kernel_stack = 0;
    struct pt_regs* regs = regs_in;
    if ((!regs && reg_fp >= (void*)vtss_koffset) || (regs && (!user_mode_vm(regs)))) kernel_stack = 1;
#ifndef CONFIG_FRAME_POINTER
    if (!regs) kernel_stack = 0; 
#endif
    if (unlikely(regs == NULL)) {
        regs = task_pt_regs(task);
    }
    if (unlikely(regs == NULL)) {
        
        strcat(stk->dbgmsg, "Stack_dump1: dump start!");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "Stack_dump2: tid=0x%08x, cpu=0x%08x: incorrect regs",
                        task->pid, smp_processor_id());
        if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
            stk->dbgmsg[rc] = '\0';
            vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        }
        return -EFAULT;
    }
    stk->dbgmsg[0] = '\0';

    /* Get IP and SP registers from current space */
    reg_ip = (void*)REG(ip, regs);
    reg_sp = (void*)REG(sp, regs);
    stk->ip.vdp = reg_ip;
    stk->sp.vdp = reg_sp;
    stk->fp.vdp = reg_fp;

    if (kernel_stack)
    { /* Unwind kernel stack and get user BP if possible */
        kernel_stack_control_t k_stk;

        if ((unsigned long)reg_fp < 0x1000 || (unsigned long)reg_fp == (unsigned long)-1) reg_fp = 0;//error instead of bp;
        k_stk.bp = (unsigned long)reg_fp;
        
        k_stk.kernel_callchain = stk->kernel_callchain;
        k_stk.prev_addr = 0;
        k_stk.kernel_callchain_size = &stk->kernel_callchain_size;
        k_stk.kernel_callchain_pos =  &stk->kernel_callchain_pos;
        *k_stk.kernel_callchain_pos = 0;
        k_stk.done = 0;
        TRACE("ip=0x%p, sp=0x%p, fp=0x%p, stk->kernel_callchain_pos=%d", reg_ip, reg_sp, reg_fp, stk->kernel_callchain_pos);
#ifdef VTSS_AUTOCONF_DUMP_TRACE_HAVE_BP
        dump_trace(task, regs_in , NULL, 0, &vtss_stack_ops, &k_stk);
#else
//        dump_trace(task, regs, reg_sp, &vtss_stack_ops, &k_stk);
        dump_trace(task, regs_in, NULL, &vtss_stack_ops, &k_stk);
#endif
        TRACE("ip=0x%p, sp=0x%p, fp=0x%p, stk->kernel_callchain_pos=%d", reg_ip, reg_sp, reg_fp, stk->kernel_callchain_pos);
        reg_fp = k_stk.bp ? (void*)k_stk.bp : reg_fp;
#ifdef VTSS_DEBUG_TRACE
        if (reg_fp > (void*)vtss_kstart) {
            printk("Warning: bp=0x%p in kernel\n", reg_fp);
            dump_stack();
            rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p]: User bp=0x%p inside kernel space",
                            task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp);
            if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
                stk->dbgmsg[rc] = '\0';
                vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            }
        }
#endif
    }
    else
    {
        stk->kernel_callchain_pos = 0;
    }

    if (!user_mode_vm(regs)) {
        /* kernel mode regs, so get a user mode regs */
#if defined(CONFIG_X86_64) || LINUX_VERSION_CODE > KERNEL_VERSION(2,6,32) || (!defined CONFIG_HIGHMEM)
        regs = task_pt_regs(task); /*< get user mode regs */
        if (regs == NULL || !user_mode_vm(regs))
#endif
        {
#ifdef VTSS_DEBUG_TRACE
            strcat(stk->dbgmsg, "Cannot get user mode regs");
            vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            dump_stack();
#endif
            strcat(stk->dbgmsg, "Stack_dump3: cannot get user mode registers!");


            vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            return -EFAULT;
        }
    }
    /* Get IP and SP registers from user space */
    reg_ip = (void*)REG(ip, regs);
    reg_sp = (void*)REG(sp, regs);
#ifdef CONFIG_FRAME_POINTER
    if (reg_fp >= (void*)vtss_koffset)
#endif
        reg_fp = (void*)REG(bp, regs);

    if (reg_fp >= (void*)vtss_koffset){
        reg_fp = reg_sp;
    }

    { /* Check for correct stack range in task->mm */
        struct vm_area_struct* vma;

#ifdef VTSS_CHECK_IP_IN_MAP
        /* Check IP in module map */
        vma = find_vma(task->mm, (unsigned long)reg_ip);
        if (likely(vma != NULL)) {
            unsigned long vm_start = vma->vm_start;
            unsigned long vm_end   = vma->vm_end;

            if ((unsigned long)reg_ip < vm_start ||
                (!((vma->vm_flags & (VM_EXEC | VM_WRITE)) == VM_EXEC &&
                    vma->vm_file && vma->vm_file->f_dentry) &&
                 !(vma->vm_mm && vma->vm_start == (long)vma->vm_mm->context.vdso)))
            {
#ifdef VTSS_DEBUG_TRACE
                rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p, found_vma=[0x%lx,0x%lx]: Unable to find executable module",
                                task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp, vm_start, vm_end);
                if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
                    stk->dbgmsg[rc] = '\0';
                    vtss_record_debug_info(trnd, stk->dbgmsg, 0);
                }
#endif
                strcat(stk->dbgmsg, "Stack_dump4: not valid vma!");
                vtss_record_debug_info(trnd, stk->dbgmsg, 0);
                return -EFAULT;
            }
        } else {
#ifdef VTSS_DEBUG_TRACE
            rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p: Unable to find executable region",
                            task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp);
            if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
                stk->dbgmsg[rc] = '\0';
                vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            }
#endif
            strcat(stk->dbgmsg, "Stack_dump5: no vma on ip!");
            vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            return -EFAULT;
        }
#endif /* VTSS_CHECK_IP_IN_MAP */

        /* Check SP in module map */
        vma = find_vma(task->mm, (unsigned long)reg_sp);
        if (likely(vma != NULL)) {
            unsigned long vm_start = vma->vm_start + ((vma->vm_flags & VM_GROWSDOWN) ? PAGE_SIZE : 0UL);
            unsigned long vm_end   = vma->vm_end;
            unsigned long stack_limit = (unsigned long)stack_base;

//            TRACE("vma=[0x%lx - 0x%lx], flags=0x%lx", vma->vm_start, vma->vm_end, vma->vm_flags);
            if ((unsigned long)reg_sp < vm_start ||
                (vma->vm_flags & (VM_READ | VM_WRITE)) != (VM_READ | VM_WRITE))
            {
//#ifdef VTSS_DEBUG_TRACE
                rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p, found_vma=[0x%lx,0x%lx]: Unable to find user stack boundaries",
                                task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp, vm_start, vm_end);
                if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
                    stk->dbgmsg[rc] = '\0';
                    vtss_record_debug_info(trnd, stk->dbgmsg, 0);
                }
//#endif
                strcat(stk->dbgmsg, "Stack_dump6: not valid fma!");
                vtss_record_debug_info(trnd, stk->dbgmsg, 0);
                return -EFAULT;
            }
            if (!((unsigned long)stack_base >= vm_start &&
                  (unsigned long)stack_base <= vm_end)  ||
                 ((unsigned long)stack_base <= (unsigned long)reg_sp))
            {
                if ((unsigned long)stack_base != 0UL) {
                    TRACE("Fixup stack base to 0x%lx instead of 0x%lx", vm_end, (unsigned long)stack_base);
                }
                stack_base = (void*)vm_end;
                stk->clear(stk);
                }
            //INFO("stk size = %lx, stack_base = %p, regsp = %p, req_stk_size=%lx", (unsigned long)stack_base - (unsigned long)reg_sp, stack_base, reg_sp, reqcfg.stk_sz[vtss_stk_user]);
            if ((unsigned long)stack_base - (unsigned long)reg_sp > reqcfg.stk_sz[vtss_stk_user]){
                unsigned long stack_base_calc = min((unsigned long)stack_base, ((unsigned long)reg_sp + reqcfg.stk_sz[vtss_stk_user])&(~(reqcfg.stk_pg_sz[vtss_stk_user]-1)));
                if (stack_base_calc < (unsigned long)stack_base){
                    TRACE("Limiting stack base to 0x%lx instead of 0x%lx, drop 0x%lx bytes", stack_base_calc, (unsigned long)stack_base, ((unsigned long)stack_base - stack_base_calc));
              //      INFO("Limiting stack base to 0x%lx instead of 0x%lx, drop 0x%lx bytes", stack_base_calc, (unsigned long)stack_base, ((unsigned long)stack_base - stack_base_calc));
                    stack_base = (void*)stack_base_calc;
                }
            }
            } else {
            rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p: Unable to find executable region 2",
                            task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp);
            if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
                stk->dbgmsg[rc] = '\0';
                vtss_record_debug_info(trnd, stk->dbgmsg, 0);
                }
            strcat(stk->dbgmsg, "Stack_dump5: no vma on sp!");
            vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            return -EFAULT;
            }
        }

#ifdef VTSS_DEBUG_TRACE
    /* Create a common header for debug message */
    rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p: USER STACK: ",
                    task->pid, smp_processor_id(), reg_ip, reg_sp, stack_base, reg_fp);
    if (!(rc > 0 && rc < sizeof(stk->dbgmsg)-1))
        rc = 0;
    stk->dbgmsg[rc] = '\0';
#else
    stk->dbgmsg[0] = '\0';
#endif

    if (stk->user_ip.vdp == reg_ip &&
        stk->user_sp.vdp == reg_sp &&
        stk->bp.vdp == stack_base &&
        stk->user_fp.vdp == reg_fp)
    {
        strcat(stk->dbgmsg, "Stack_dump7: The same context");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
//        return 0; /* Assume that nothing was changed */
    }

    /* Try to lock vm accessor */
//    if (stk->acc == NULL)
//    {
//        stk->acc = vtss_user_vm_accessor_init(in_irq, vtss_time_limit);
 //   }
//    acc = vtss_user_vm_accessor_init(in_irq, vtss_time_limit);
    if (unlikely((stk->acc == NULL) || stk->acc->trylock(stk->acc, task))) {
//        vtss_user_vm_accessor_fini(stk->acc);
//        stk->acc=NULL;
        strcat(stk->dbgmsg, "Stack_dump8:Unable to lock vm accessor");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        return -EBUSY;
    }

    /* stk->setup(stk, acc, reg_ip, reg_sp, stack_base, reg_fp, stk->wow64); */
  //  stk->acc    = acc;
    stk->user_ip.vdp = reg_ip;
    stk->user_sp.vdp = reg_sp;
    stk->bp.vdp = stack_base;
    stk->user_fp.vdp = reg_fp;
    TRACE("user: ip=0x%p, sp=0x%p, fp=0x%p", stk->user_ip.vdp, stk->user_sp.vdp, stk->user_fp.vdp);
    VTSS_PROFILE(unw, rc = stk->unwind(stk));
    /* Check unwind result */
    if (unlikely(rc == VTSS_ERR_NOMEMORY)) {
        /* Try again with realloced buffer */
        while (rc == VTSS_ERR_NOMEMORY && !stk->realloc(stk)) {
            VTSS_PROFILE(unw, rc = stk->unwind(stk));
        }
        if (rc == VTSS_ERR_NOMEMORY) {
            strcat(stk->dbgmsg, "Not enough memory - ");
        }
    }
    stk->acc->unlock(stk->acc);
//    vtss_user_vm_accessor_fini(acc);
    if (unlikely(rc)) {
        stk->clear(stk);
        strcat(stk->dbgmsg, "Unwind error");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
    }
    TRACE("end, rc = %d", rc);
    return rc;
}

int vtss_stack_record_kernel(struct vtss_transport_data* trnd, stack_control_t* stk, pid_t tid, int cpu, unsigned long long stitch_id, int is_safe)
{
    int rc = -EFAULT;
    int stklen = stk->kernel_callchain_pos;
    void* entry;
#ifdef VTSS_USE_UEC
    stk_trace_kernel_record_t stkrec;
    if (stklen == 0)
    {
        // kernel is empty
        strcat(stk->dbgmsg, "Stack_record_k1: Unable to lock vm accessor");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        return 0;
    }
    TRACE("ip=0x%p, sp=0x%p, fp=0x%p: Trace %d bytes", stk->ip.vdp, stk->sp.vdp, stk->fp.vdp, stklen);
    //implementation is done for UEC NOT USED
    /// save current alt. stack:
    /// [flagword - 4b][residx]
    /// ...[sampled address - 8b][systrace{sts}]
    ///                       [length - 2b][type - 2b]...
    stkrec.flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_SYSTRACE;
    stkrec.residx   = tid;
    stkrec.size     = 4 /*+ sizeof(stkrec.size) + sizeof(stkrec.type)*/;
    stkrec.type     = (sizeof(void*) == 8) ? UECSYSTRACE_CLEAR_STACK64 : UECSYSTRACE_CLEAR_STACK32;
    stkrec.size += sizeof(unsigned int);
    stkrec.idx   = -1;
    /// correct the size of systrace
    stkrec.size += (unsigned short)sktlen;
    if (vtss_transport_record_write(trnd, &stkrec, sizeof(stkrec) - (stk->wow64*8), stk->kernel_callchain, stklen, is_safe)) {
        TRACE("STACK_record_write() FAIL");
        strcat(stk->dbgmsg, "Stack_record_k2: Unable to wrie the record");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        rc = -EFAULT;
    }

#else
    stk_trace_kernel_record_t* stkrec;
    if (stklen == 0)
    {
        // kernel is empty
        strcat(stk->dbgmsg, "Stack_record_k3: Stack size is 0");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        return 0;
    }
    TRACE("ip=0x%p, sp=0x%p, fp=0x%p: Trace %d bytes", stk->ip.vdp, stk->sp.vdp, stk->fp.vdp, stklen);
    //implementation is done for UEC NOT USED
    stkrec = (stk_trace_kernel_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(stk_trace_kernel_record_t) + stklen);
    if (likely(stkrec)) {
    /// save current alt. stack:
    /// [flagword - 4b][residx]
    /// ...[sampled address - 8b][systrace{sts}]
    ///                       [length - 2b][type - 2b]...
    stkrec->flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_SYSTRACE;
    stkrec->residx   = tid;
    stkrec->size     = (unsigned short)stklen + sizeof(stkrec->size) + sizeof(stkrec->type);
    stkrec->type     = (sizeof(void*) == 8) ? UECSYSTRACE_CLEAR_STACK64 : UECSYSTRACE_CLEAR_STACK32;
    stkrec->size += sizeof(unsigned int);
    stkrec->idx   = -1;
    memcpy((char*)stkrec+sizeof(stk_trace_kernel_record_t), stk->kernel_callchain, stklen);
    rc = vtss_transport_record_commit(trnd, entry, is_safe);
    }
#endif
    return rc;

}
int vtss_stack_record(struct vtss_transport_data* trnd, stack_control_t* stk, pid_t tid, int cpu, int is_safe)
{
    int rc = -EFAULT;
    unsigned short sample_type;
    int sktlen;

    /// collect LBR call stacks if so requested
    if(reqcfg.trace_cfg.trace_flags & VTSS_CFGTRACE_LBRCSTK)
    {
//        strcat(stk->dbgmsg, "Stack_record_u1: Write lbrs");
 //       vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        return vtss_stack_record_lbr(trnd, stk, tid, cpu, is_safe);
    }
    sktlen = stk->compress(stk);

    if (stk->kernel_callchain_pos!=0)
    {
        rc = vtss_stack_record_kernel(trnd, stk, tid, cpu, 0, is_safe);
    }
    if (unlikely(sktlen == 0)) {
        rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p: Huge or Zero stack after compression",
                        tid, smp_processor_id(), stk->user_ip.vdp, stk->user_sp.vdp, stk->bp.vdp, stk->user_fp.vdp);
        if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
            stk->dbgmsg[rc] = '\0';
            vtss_record_debug_info(trnd, stk->dbgmsg, is_safe);
        }
        strcat(stk->dbgmsg, "Stack_record_u2: Write strlen = 0");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        return -EFAULT;
    }

    if (stk->is_full(stk)) { /* full stack */
        sample_type = (sizeof(void*) == 8 && !stk->wow64) ? UECSYSTRACE_STACK_CTX64_V0 : UECSYSTRACE_STACK_CTX32_V0;
    } else { /* incremental stack */
        sample_type = (sizeof(void*) == 8 && !stk->wow64) ? UECSYSTRACE_STACK_CTXINC64_V0 : UECSYSTRACE_STACK_CTXINC32_V0;
    }

#ifdef VTSS_USE_UEC
    {
        stk_trace_record_t stkrec;
        /// save current alt. stack:
        /// [flagword - 4b][residx][cpuidx - 4b][tsc - 8b]
        /// ...[sampled address - 8b][systrace{sts}]
        ///                       [length - 2b][type - 2b]...
        stkrec.flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_EXECADDR | UECL1_SYSTRACE;
        stkrec.residx   = tid;
        stkrec.cpuidx   = cpu;
        stkrec.cputsc   = vtss_time_cpu();
        stkrec.execaddr = (unsigned long long)stk->ip.szt;
        stkrec.type     = sample_type;

        if (!stk->wow64) {
            stkrec.size = 4 + sizeof(void*) + sizeof(void*);
            stkrec.sp   = stk->user_sp.szt;
            stkrec.fp   = stk->user_fp.szt;
        } else { /// a 32-bit stack in a 32-bit process on a 64-bit system
            stkrec.size = 4 + sizeof(unsigned int) + sizeof(unsigned int);
            stkrec.sp32 = (unsigned int)stk->user_sp.szt;
            stkrec.fp32 = (unsigned int)stk->user_fp.szt;
        }
        rc = 0;
        if (sktlen > 0xfffb) {
            lstk_trace_record_t lstkrec;

            TRACE("ip=0x%p, sp=0x%p, fp=0x%p: Large Trace %d bytes", stk->user_ip.vdp, stk->user_sp.vdp, stk->user_fp.vdp, sktlen);
            lstkrec.size = (unsigned int)(stkrec.size + sktlen + 2); /* 2 = sizeof(int) - sizeof(short) */
            lstkrec.flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_EXECADDR | UECL1_LARGETRACE;
            lstkrec.residx   = stkrec.residx;
            lstkrec.cpuidx   = stkrec.cpuidx;
            lstkrec.cputsc   = stkrec.cputsc;
            lstkrec.execaddr = stkrec.execaddr;
            lstkrec.type     = stkrec.type;
            lstkrec.sp       = stkrec.sp;
            lstkrec.fp       = stkrec.fp;
            if (vtss_transport_record_write(trnd, &lstkrec, sizeof(lstkrec) - (stk->wow64*8), stk->data(stk), sktlen, is_safe)) {
                strcat(stk->dbgmsg, "Record was not written");
                vtss_record_debug_info(trnd, stk->dbgmsg, 0);
                TRACE("STACK_record_write() FAIL");
                rc = -EFAULT;
            }
        } else {
            /// correct the size of systrace
            stkrec.size += (unsigned short)sktlen;
            if (vtss_transport_record_write(trnd, &stkrec, sizeof(stkrec) - (stk->wow64*8), stk->data(stk), sktlen, is_safe)) {
                TRACE("STACK_record_write() FAIL");
                strcat(stk->dbgmsg, "Record was not written");
                vtss_record_debug_info(trnd, stk->dbgmsg, 0);
                rc = -EFAULT;
            }
        }
    }
#else  /* VTSS_USE_UEC */
    if (unlikely(sktlen > 0xfffb)) {
        rc = snprintf(stk->dbgmsg, sizeof(stk->dbgmsg)-1, "tid=0x%08x, cpu=0x%08x, ip=0x%p, sp=[0x%p,0x%p], fp=0x%p: Large Stack Trace %d bytes",
                        tid, smp_processor_id(), stk->user_ip.vdp, stk->user_sp.vdp, stk->bp.vdp, stk->user_fp.vdp, sktlen);
        if (rc > 0 && rc < sizeof(stk->dbgmsg)-1) {
            stk->dbgmsg[rc] = '\0';
            vtss_record_debug_info(trnd, stk->dbgmsg, is_safe);
        }
        strcat(stk->dbgmsg, "Stack4: Too big stk len");
        vtss_record_debug_info(trnd, stk->dbgmsg, 0);
        return -EFAULT;
    } else {
        void* entry;
        stk_trace_record_t* stkrec = (stk_trace_record_t*)vtss_transport_record_reserve(trnd, &entry, sizeof(stk_trace_record_t) - (stk->wow64*8) + sktlen);
        if (likely(stkrec)) {
            /// save current alt. stack:
            /// [flagword - 4b][residx][cpuidx - 4b][tsc - 8b]
            /// ...[sampled address - 8b][systrace{sts}]
            ///                       [length - 2b][type - 2b]...
            stkrec->flagword = UEC_LEAF1 | UECL1_VRESIDX | UECL1_CPUIDX | UECL1_CPUTSC | UECL1_EXECADDR | UECL1_SYSTRACE;
            stkrec->residx   = tid;
            stkrec->cpuidx   = cpu;
            stkrec->cputsc   = vtss_time_cpu();
            stkrec->execaddr = (unsigned long long)stk->user_ip.szt;
            stkrec->size     = (unsigned short)sktlen + sizeof(stkrec->size) + sizeof(stkrec->type);
            stkrec->type     = sample_type;
            if (!stk->wow64) {
                stkrec->size += sizeof(void*) + sizeof(void*);
                stkrec->sp   = stk->user_sp.szt;
                stkrec->fp   = stk->user_fp.szt;
            } else { /* a 32-bit stack in a 32-bit process on a 64-bit system */
                stkrec->size += sizeof(unsigned int) + sizeof(unsigned int);
                stkrec->sp32 = (unsigned int)stk->user_sp.szt;
                stkrec->fp32 = (unsigned int)stk->user_fp.szt;
            }
            memcpy((char*)stkrec+sizeof(stk_trace_record_t)-(stk->wow64*8), stk->compressed, sktlen);
            rc = vtss_transport_record_commit(trnd, entry, is_safe);
            if (rc != 0){
               strcat(stk->dbgmsg, "Stack_record5: Cannot write the record");
               vtss_record_debug_info(trnd, stk->dbgmsg, 0);
            }
        }
    }
#endif /* VTSS_USE_UEC */
    return rc;
}

