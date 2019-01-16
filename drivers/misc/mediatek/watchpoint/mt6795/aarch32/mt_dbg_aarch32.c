#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <mach/mt_boot.h>
#ifdef CONFIG_SMP
#include <mach/hotplug.h>
#include <linux/cpu.h>
#endif
#include "hw_watchpoint_aarch32.h"
#include "mt_dbg_aarch32.h"
extern int get_cluster_core_count(void);
struct dbgreg_set dbgregs[8];

#ifdef DBG_REG_DUMP
void dump_dbgregs(int cpuid)
{

	struct wp_trace_context_t *wp_context;
	int i; 
	int oslsr;
	int dbglsr;
	register_wp_context(&wp_context);
	cs_cpu_write(wp_context->debug_regs[cpuid], DBGLAR ,UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[cpuid], DBGOSLAR ,~UNLOCK_KEY);
	smp_call_function_single(cpuid,smp_read_dbgoslsr_callback,&oslsr,1);
	dbglsr=cs_cpu_read(wp_context->debug_regs[cpuid],DBGLSR);
	pr_notice("[MTK WP]dump_dbgregs: cpu %d, DBGLSR 0x%x, DBGOSLSR 0x%x\n",cpuid,dbglsr,oslsr);

	isb();
	
	smp_call_function_single(cpuid,smp_read_dbgdscr_callback,&dbgregs[cpuid].DBGDSCRext,1);
	
	for(i=1; i<1+(wp_context->bp_nr);i++)
	{
	   dbgregs[cpuid].regs[i]=cs_cpu_read(wp_context->debug_regs[cpuid],(DBGBVR + ((i-1)<<4)));
	}

	for(i=7; i<7+(wp_context->bp_nr);i++)
	{
	   dbgregs[cpuid].regs[i]=cs_cpu_read(wp_context->debug_regs[cpuid],(DBGBCR + ((i-7)<<4)));
	}

	for(i=13; i<13+(wp_context->wp_nr);i++)
	{
	   dbgregs[cpuid].regs[i]=cs_cpu_read(wp_context->debug_regs[cpuid],(DBGWVR + ((i-13)<<4)));
	   pr_notice("[MTK WP]dump_dbgregs:DBGWVR &0x%p=0x%x\n",wp_context->debug_regs[cpuid]+(DBGWVR + ((i-13)<<4)),\
	   cs_cpu_read(wp_context->debug_regs[cpuid],(DBGWVR + ((i-13)<<4))));
	}

	for(i=17; i<17+(wp_context->wp_nr);i++)
	{
	   dbgregs[cpuid].regs[i]=cs_cpu_read(wp_context->debug_regs[cpuid],(DBGWCR + ((i-17)<<4)));
	}

	smp_call_function_single(cpuid,smp_read_dbgvcr_callback,&dbgregs[cpuid].DBGVCR,1);
//	smp_call_function_single(cpuid,smp_read_dbgsdsr_callback,&dbgregs[cpuid].SDSR,1);
 
	isb();
   
}

void print_dbgregs(int cpuid)
{

	pr_notice("[MTK WP] cpu %d, DBGDSCR 0x%lx\n",cpuid,dbgregs[cpuid].DBGDSCRext);
	pr_notice("[MTK WP] cpu %d, DBGBVR0 0x%lx\n",cpuid,dbgregs[cpuid].DBGBVR0);
	pr_notice("[MTK WP] cpu %d, DBGBVR1 0x%lx\n",cpuid,dbgregs[cpuid].DBGBVR1);
	pr_notice("[MTK WP] cpu %d, DBGBVR2 0x%lx\n",cpuid,dbgregs[cpuid].DBGBVR2);
	pr_notice("[MTK WP] cpu %d, DBGBVR3 0x%lx\n",cpuid,dbgregs[cpuid].DBGBVR3);
	pr_notice("[MTK WP] cpu %d, DBGBVR4 0x%lx\n",cpuid,dbgregs[cpuid].DBGBVR4);
	pr_notice("[MTK WP] cpu %d, DBGBVR5 0x%lx\n",cpuid,dbgregs[cpuid].DBGBVR5);

	pr_notice("[MTK WP] cpu %d, DBGBCR0 0x%lx\n",cpuid,dbgregs[cpuid].DBGBCR0);
	pr_notice("[MTK WP] cpu %d, DBGBCR1 0x%lx\n",cpuid,dbgregs[cpuid].DBGBCR1);
	pr_notice("[MTK WP] cpu %d, DBGBCR2 0x%lx\n",cpuid,dbgregs[cpuid].DBGBCR2);
	pr_notice("[MTK WP] cpu %d, DBGBCR3 0x%lx\n",cpuid,dbgregs[cpuid].DBGBCR3);
	pr_notice("[MTK WP] cpu %d, DBGBCR4 0x%lx\n",cpuid,dbgregs[cpuid].DBGBCR4);
	pr_notice("[MTK WP] cpu %d, DBGBCR5 0x%lx\n",cpuid,dbgregs[cpuid].DBGBCR5);


	pr_notice("[MTK WP] cpu %d, DBGWVR0 0x%lx\n",cpuid,dbgregs[cpuid].DBGWVR0);
	pr_notice("[MTK WP] cpu %d, DBGWVR1 0x%lx\n",cpuid,dbgregs[cpuid].DBGWVR1);
	pr_notice("[MTK WP] cpu %d, DBGWVR2 0x%lx\n",cpuid,dbgregs[cpuid].DBGWVR2);
	pr_notice("[MTK WP] cpu %d, DBGWVR3 0x%lx\n",cpuid,dbgregs[cpuid].DBGWVR3);

	pr_notice("[MTK WP] cpu %d, DBGWCR0 0x%lx\n",cpuid,dbgregs[cpuid].DBGWCR0);
	pr_notice("[MTK WP] cpu %d, DBGWCR1 0x%lx\n",cpuid,dbgregs[cpuid].DBGWCR1);
	pr_notice("[MTK WP] cpu %d, DBGWCR2 0x%lx\n",cpuid,dbgregs[cpuid].DBGWCR2);
	pr_notice("[MTK WP] cpu %d, DBGWCR3 0x%lx\n",cpuid,dbgregs[cpuid].DBGWCR3);

	pr_notice("[MTK WP] cpu %d, DBGVCR 0x%lx\n",cpuid,dbgregs[cpuid].DBGVCR);
	pr_notice("[MTK WP] cpu %d, SDSR 0x%lx\n",cpuid,dbgregs[cpuid].SDSR);
}

#endif 

unsigned int *mt_save_dbg_regs(unsigned int *p, unsigned int cpuid)
{
	struct wp_trace_context_t *wp_context;
	register_wp_context(&wp_context);
	cs_cpu_write(wp_context->debug_regs[cpuid], DBGLAR ,UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[cpuid], DBGOSLAR ,~UNLOCK_KEY);
	if (*p == ~0x0){
		pr_err("[MTK WP]restore pointer is NULL\n");
		return 0;
	}


	isb();
	// save register
	__asm__ __volatile__ (
		"mrc p14, 0, r4, c0, c2, 2  @DBGDSCR_ext \n\t"
		"stm %0!, {r4} \n\t"


		"mrc p14, 0, r4, c0, c0, 4  @DBGBVR \n\t"
		"mrc p14, 0, r5, c0, c1, 4  @DBGBVR \n\t"
		"mrc p14, 0, r6, c0, c2, 4  @DBGBVR \n\t"
		"mrc p14, 0, r7, c0, c3, 4  @DBGBVR \n\t"
		"mrc p14, 0, r8, c0, c4, 4  @DBGBVR \n\t"
		"mrc p14, 0, r9, c0, c5, 4  @DBGBVR \n\t"
		"stm %0!, {r4-r9} \n\t"


		"mrc p14, 0, r4, c0, c0, 5  @DBGBCR \n\t"
		"mrc p14, 0, r5, c0, c1, 5  @DBGBCR \n\t"
		"mrc p14, 0, r6, c0, c2, 5  @DBGBCR \n\t"
		"mrc p14, 0, r7, c0, c3, 5  @DBGBCR \n\t"
		"mrc p14, 0, r8, c0, c4, 5  @DBGBCR \n\t"
		"mrc p14, 0, r9, c0, c5, 5  @DBGBCR \n\t"
		"stm %0!, {r4-r9} \n\t"

		"mrc p14, 0, r4, c0, c0, 6  @DBGWVR \n\t"
		"mrc p14, 0, r5, c0, c1, 6  @DBGWVR \n\t"
		"mrc p14, 0, r6, c0, c2, 6  @DBGWVR \n\t"
		"mrc p14, 0, r7, c0, c3, 6  @DBGWVR \n\t"
		"stm %0!, {r4-r7} \n\t"


		"mrc p14, 0, r4, c0, c0, 7  @DBGWCR \n\t"
		"mrc p14, 0, r5, c0, c1, 7  @DBGWCR \n\t"
		"mrc p14, 0, r6, c0, c2, 7  @DBGWCR \n\t"
		"mrc p14, 0, r7, c0, c3, 7  @DBGWCR \n\t"
		"stm %0!, {r4-r7} \n\t"

		"mrc p14, 0, r4, c0, c7, 0  @DBGVCR \n\t"
		"stm %0!, {r4} \n\t"
		
		:"+r"(p)
		:
		:"r4", "r5", "r6", "r7", "r8", "r9"
		);
		isb();

	return p;
}

void mt_restore_dbg_regs(unsigned int *p, unsigned int cpuid)
{
	unsigned int dscr;
	struct wp_trace_context_t *wp_context;
	register_wp_context(&wp_context);
	// the dbg container is invalid
	if (*p == ~0x0){
	   pr_err("[MTK WP]restore pointer is NULL\n");
	   return;
	}
	cs_cpu_write(wp_context->debug_regs[cpuid], DBGLAR ,UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[cpuid], DBGOSLAR ,~UNLOCK_KEY);

	isb();


	// restore register
	__asm__ __volatile__ (
		"ldm %0!, {r4} \n\t"
		"mov %1, r4 \n\t"
		"mcr p14, 0, r4, c0, c2, 2  @DBGDSCR \n\t"


		"ldm %0!, {r4-r9} \n\t"
		"mcr p14, 0, r4, c0, c0, 4  @DBGBVR \n\t"
		"mcr p14, 0, r5, c0, c1, 4  @DBGBVR \n\t"
		"mcr p14, 0, r6, c0, c2, 4  @DBGBVR \n\t"
		"mcr p14, 0, r7, c0, c3, 4  @DBGBVR \n\t"
		"mcr p14, 0, r8, c0, c4, 4  @DBGBVR \n\t"
		"mcr p14, 0, r9, c0, c5, 4  @DBGBVR \n\t"

		"ldm %0!, {r4-r9} \n\t"
		"mcr p14, 0, r4, c0, c0, 5  @DBGBCR \n\t"
		"mcr p14, 0, r5, c0, c1, 5  @DBGBCR \n\t"
		"mcr p14, 0, r6, c0, c2, 5  @DBGBCR \n\t"
		"mcr p14, 0, r7, c0, c3, 5  @DBGBCR \n\t"
		"mcr p14, 0, r8, c0, c4, 5  @DBGBCR \n\t"
		"mcr p14, 0, r9, c0, c5, 5  @DBGBCR \n\t"

		"ldm %0!, {r4-r7} \n\t"
		"mcr p14, 0, r4, c0, c0, 6  @DBGWVR \n\t"
		"mcr p14, 0, r5, c0, c1, 6  @DBGWVR \n\t"
		"mcr p14, 0, r6, c0, c2, 6  @DBGWVR \n\t"
		"mcr p14, 0, r7, c0, c3, 6  @DBGWVR \n\t"

		"ldm %0!, {r4-r7} \n\t"
		"mcr p14, 0, r4, c0, c0, 7  @DBGWCR \n\t"
		"mcr p14, 0, r5, c0, c1, 7  @DBGWCR \n\t"
		"mcr p14, 0, r6, c0, c2, 7  @DBGWCR \n\t"
		"mcr p14, 0, r7, c0, c3, 7  @DBGWCR \n\t"

		"ldm %0!, {r4} \n\t"
		"mcr p14, 0, r4, c0, c7, 0  @DBGVCR \n\t"

		:"+r"(p), "=r"(dscr)
		:
		:"r4", "r5", "r6", "r7", "r8", "r9"
				);
//		cs_cpu_write(wp_context->debug_regs[cpuid], DBGOSLAR ,UNLOCK_KEY);
//		cs_cpu_write(wp_context->debug_regs[cpuid], DBGLAR ,~UNLOCK_KEY);
	isb();
}





/** to copy dbg registers from FROM to TO within the same cluster.
 * DBG_BASE is the common base of 2 cores debug register space.
 **/
void mt_copy_dbg_regs(int to, int from)
{
	unsigned long base_to, base_from;
	unsigned long args;
	struct wp_trace_context_t *wp_context;
	register_wp_context(&wp_context);
	base_to =(unsigned long) wp_context->debug_regs[to];
	base_from = (unsigned long)wp_context->debug_regs[from];

	// os unlock
	cs_cpu_write(wp_context->debug_regs[to], DBGLAR ,UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[to], DBGOSLAR ,~UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[from], DBGLAR ,UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[from], DBGOSLAR ,~UNLOCK_KEY);

	isb();

	smp_call_function_single(from,smp_read_dbgdscr_callback,&args,1);		
	smp_call_function_single(to,smp_write_dbgdscr_callback,&args,1);
	isb();
	dbg_reg_copy(DBGBCR, base_to, base_from);
	dbg_reg_copy(DBGBCR+0x10, base_to, base_from);
	dbg_reg_copy(DBGBCR+0x20, base_to, base_from);
	dbg_reg_copy(DBGBCR+0x30, base_to, base_from);
	dbg_reg_copy(DBGBCR+0x40, base_to, base_from);
	dbg_reg_copy(DBGBCR+0x50, base_to, base_from);

	dbg_reg_copy(DBGBVR, base_to, base_from);
	dbg_reg_copy(DBGBVR+0x10, base_to, base_from);
	dbg_reg_copy(DBGBVR+0x20, base_to, base_from);
	dbg_reg_copy(DBGBVR+0x30, base_to, base_from);
	dbg_reg_copy(DBGBVR+0x40, base_to, base_from);
	dbg_reg_copy(DBGBVR+0x50, base_to, base_from);


	dbg_reg_copy(DBGWVR, base_to, base_from);
	dbg_reg_copy(DBGWVR+0x10, base_to, base_from);
	dbg_reg_copy(DBGWVR+0x20, base_to, base_from);
	dbg_reg_copy(DBGWVR+0x30, base_to, base_from);

	dbg_reg_copy(DBGWCR, base_to, base_from);
	dbg_reg_copy(DBGWCR+0x10, base_to, base_from);
	dbg_reg_copy(DBGWCR+0x20, base_to, base_from);
	dbg_reg_copy(DBGWCR+0x30, base_to, base_from);
	 
	smp_call_function_single(from,smp_read_dbgvcr_callback,&args,1);
	ARM_DBG_WRITE(c0,c7,0, args);
	isb();
}



#ifdef CONFIG_SMP
int __cpuinit
dbgregs_hotplug_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
	unsigned int this_cpu = (unsigned int) hcpu;
	unsigned long args;
	struct wp_trace_context_t *wp_context;
	register_wp_context(&wp_context);
	cs_cpu_write(wp_context->debug_regs[this_cpu], DBGLAR ,UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[this_cpu], DBGOSLAR ,~UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[0], DBGLAR ,UNLOCK_KEY);
	cs_cpu_write(wp_context->debug_regs[0], DBGOSLAR ,~UNLOCK_KEY);
	args = args & 0xf;
#ifdef DBG_REG_DUMP
    pr_notice("[MTK WP] cpu %x do %s,action: 0x%lx\n",this_cpu,__func__,action);
#endif
	switch (action) {
	case CPU_STARTING:
#ifdef DBG_REG_DUMP
        pr_notice("[MTK WP] cpu %x do %s, CPU0's _MDSCR_EL1=0x%lx, action 0x%lx\n",this_cpu,__func__,args,action);
#endif
		cs_cpu_read(wp_context->debug_regs[0],DBGWVR + (0 << 4));
		smp_call_function_single(0,smp_read_dbgdscr_callback,&args,1);
		ARM_DBG_WRITE(c0, c2, 2, args);
		ARM_DBG_WRITE(c0, c0, 6, cs_cpu_read(wp_context->debug_regs[0],DBGWVR + (0 << 4)));
		ARM_DBG_WRITE(c0, c1, 6, cs_cpu_read(wp_context->debug_regs[0],DBGWVR + (1 << 4)));
		ARM_DBG_WRITE(c0, c2, 6, cs_cpu_read(wp_context->debug_regs[0],DBGWVR + (2 << 4)));
		ARM_DBG_WRITE(c0, c3, 6, cs_cpu_read(wp_context->debug_regs[0],DBGWVR + (3 << 4)));
	   
		ARM_DBG_WRITE(c0, c0, 7, cs_cpu_read(wp_context->debug_regs[0],DBGWCR + (0 << 4)));
		ARM_DBG_WRITE(c0, c1, 7, cs_cpu_read(wp_context->debug_regs[0],DBGWCR + (1 << 4)));	
		ARM_DBG_WRITE(c0, c2, 7, cs_cpu_read(wp_context->debug_regs[0],DBGWCR + (2 << 4)));	
		ARM_DBG_WRITE(c0, c3, 7, cs_cpu_read(wp_context->debug_regs[0],DBGWCR + (3 << 4)));  
			
		ARM_DBG_WRITE(c0, c0, 4, cs_cpu_read(wp_context->debug_regs[0],DBGBVR + (0 << 4)));
		ARM_DBG_WRITE(c0, c1, 4, cs_cpu_read(wp_context->debug_regs[0],DBGBVR + (1 << 4)));
		ARM_DBG_WRITE(c0, c2, 4, cs_cpu_read(wp_context->debug_regs[0],DBGBVR + (2 << 4)));
		ARM_DBG_WRITE(c0, c3, 4, cs_cpu_read(wp_context->debug_regs[0],DBGBVR + (3 << 4)));
		ARM_DBG_WRITE(c0, c4, 4, cs_cpu_read(wp_context->debug_regs[0],DBGBVR + (4 << 4)));
		ARM_DBG_WRITE(c0, c5, 4, cs_cpu_read(wp_context->debug_regs[0],DBGBVR + (5 << 4)));
			
		ARM_DBG_WRITE(c0, c0, 5, cs_cpu_read(wp_context->debug_regs[0],DBGBCR + (0 << 4)));
		ARM_DBG_WRITE(c0, c1, 5, cs_cpu_read(wp_context->debug_regs[0],DBGBCR + (1 << 4)));	
		ARM_DBG_WRITE(c0, c2, 5, cs_cpu_read(wp_context->debug_regs[0],DBGBCR + (2 << 4)));	
		ARM_DBG_WRITE(c0, c3, 5, cs_cpu_read(wp_context->debug_regs[0],DBGBCR + (3 << 4)));
		ARM_DBG_WRITE(c0, c4, 5, cs_cpu_read(wp_context->debug_regs[0],DBGBCR + (4 << 4)));	
		ARM_DBG_WRITE(c0, c5, 5, cs_cpu_read(wp_context->debug_regs[0],DBGBCR + (5 << 4))); 				
		isb();
		smp_call_function_single(0,smp_read_dbgvcr_callback,&args,1);
		ARM_DBG_WRITE(c0,c7,0, args);
//		smp_call_function_single(0,smp_read_dbgsdsr_callback,&args,1);
//		smp_write_dbgsdsr_callback(&args);
	break;

	default: 
	break;
	}

		return NOTIFY_OK;
}

static struct notifier_block __cpuinitdata cpu_nfb = {
		.notifier_call = dbgregs_hotplug_callback
};

static int __init regs_backup(void)
{
	
	register_cpu_notifier(&cpu_nfb);

	return 0;
}

module_init(regs_backup);
#endif
