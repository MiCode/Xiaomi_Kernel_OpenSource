#include <linux/spinlock.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <asm/system.h>
#include <asm/signal.h>
#include <asm/ptrace.h>
#include "hw_watchpoint_aarch32.h"
#include <mach/sync_write.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/of_address.h>
#include <linux/cpu.h>
#include "mt_dbg_aarch32.h"
#include <linux/delay.h>
extern unsigned read_clusterid(void);
struct wp_trace_context_t wp_tracer;
#ifdef WATCHPOINT_TEST_SUIT
 struct wp_event wp_event;
 int err;
 volatile int my_watch_data2;
 int wp_flag;
 int my_wp_handler1(unsigned int addr)
 {
     
     wp_flag++; 
     pr_notice("[MTK WP] Access my data from an instruction at 0x%x\n" ,addr);
     return 0;
 }

 int my_wp_handler2(unsigned int addr)
 {
   
     //this_cpu = get_cpu();
     pr_notice("[MTK WP] In my_wp_handler2 Access my data from an instruction at 0x%x\n",addr);
     /* trigger exception */
     return 0;
 }
 
 void wp_test1(void)
 {
     init_wp_event(&wp_event, (unsigned int)&my_watch_data2,(unsigned int) &my_watch_data2, WP_EVENT_TYPE_ALL, my_wp_handler1);
     err = add_hw_watchpoint(&wp_event);
     if (err) {
         pr_notice("[MTK WP] add hw watch point failed...\n");
         /* fail to add watchpoing */
     } else {
         /* the memory address is under watching */
         pr_notice("[MTK WP] add hw watch point success...\n");

         //del_hw_watchpoint(&wp_event);
     }
     /* test watchpoint */
     my_watch_data2 = 1;
 }
 
void smp_specific_write(int *p)
{
  *p=1;
  pr_notice("[MTK WP] wite data in specific address ok,addr=0x%p\n",p);
  return;
}
 void wp_test2(void)
 {
     int i;
     int ret;
     //this_cpu = get_cpu();
     wp_flag=0;
     pr_notice("[MTK WP] Init wp.. ");
     init_wp_event(&wp_event, (unsigned int)&my_watch_data2, (unsigned int)&my_watch_data2, WP_EVENT_TYPE_ALL, my_wp_handler1);
     err = add_hw_watchpoint(&wp_event);
     if (err) {
         pr_notice("[MTK WP] add hw watch point failed...\n");
         /* fail to add watchpoing */
     } else {
         /* the memory address is under watching */
         pr_notice("[MTK WP] add hw watch point success...\n");

         //del_hw_watchpoint(&wp_event);
     }
     pr_notice("[MTK WP] dump standard dbgsys setting\n");
     for(i=1 ; i<num_possible_cpus() ;i++){
     	ret=cpu_down(i);
     	if(ret!=0)
     	{
			pr_notice("[MTK WP] cpu %d power down failed\n",i);
	 	}
     	if(!cpu_online(i))
     	{
     		pr_notice("[MTK WP] cpu %d already power down \n",i);
     	}   
     }
     for(i=0 ;i<num_possible_cpus() ;i++){
     	ret=cpu_up(i);
     	if(ret!=0)
     	{
	 		pr_notice("[MTK WP] cpu %d power up failed\n",i);
     	}
     	if(cpu_online(i))
     	{
	 		pr_notice("[MTK WP] cpu %d already power up \n",i);
     	}
        pr_notice("[MTK WP] dump dbgsys setting restored\n");
#ifdef DBG_REG_DUMP
        dump_dbgregs(i);
        print_dbgregs(i);
#endif
    	ret=smp_call_function_single(i,(smp_call_func_t)smp_specific_write,(void *)&my_watch_data2,1);     
        if(ret==0)
        {
        	pr_notice("[MTK WP] cpu %d, wite data in specific address ok\n",i);
        }
        else
        {
            pr_notice("[MTK WP] cpu %d, wite data in specific address failed\n",i);
        }
     }
    if(wp_flag==num_possible_cpus())
    {
      pr_notice("[MTK WP] Watchpoint item2 verfication pass 0x%x\n",wp_flag);
    }
    else
    {
      pr_notice("[MTK WP] Watchpoint item2 verfication failed 0x%x\n",wp_flag);
    }
 }

void wp_test3(void)
{
    int i;
    int ret;
    wp_flag=0;
	for(i=0 ;i<num_possible_cpus() ;i++){
  		ret=cpu_up(i);
  		if(ret!=0)
    	{
    		pr_notice("[MTK WP] cpu %d power up failed\n",i);
    	}
    	if(cpu_online(i))
    	{
    		pr_notice("[MTK WP] cpu %d already power up \n",i);
    	}
    	pr_notice("[MTK WP] dump dbgsys setting restored\n");
#ifdef DBG_REG_DUMP
  	    dump_dbgregs(i);
   	    print_dbgregs(i);
#endif  
        ret=smp_call_function_single(i,(smp_call_func_t)smp_specific_write,(void *)&my_watch_data2,1);
        if(ret==0)
        {
            pr_notice("[MTK WP] cpu %d, wite data in specific address ok\n",i);
        }
        else
        {
            pr_notice("[MTK WP] cpu %d, wite data in specific address failed\n",i);
        }
	}
	if(wp_flag==num_possible_cpus())
	{
		pr_notice("[MTK WP] Watchpoint item3 verfication pass 0x%x\n",wp_flag);
	}
    else
	{
    	pr_notice("[MTK WP] Watchpoint item3 verfication failed 0x%x\n",wp_flag);
	}

} 
#endif

void smp_read_dbgdscr_callback(void *info)
{
    unsigned long tmp;
    unsigned long *val=info;
	ARM_DBG_READ(c0,c2,2, tmp);
    *val=tmp;
	return;
}

void smp_write_dbgdscr_callback(void *info)
{
    unsigned long *val=info;
    unsigned long tmp=*val;
    ARM_DBG_WRITE(c0,c2,2, tmp);
    return;
}

void smp_read_dbgoslsr_callback(void *info)
{
    unsigned long tmp;
    unsigned long *val=info;
    ARM_DBG_READ(c1,c1,4, tmp);
    *val=tmp;
    return;
}

void smp_write_dbgoslsr_callback(void *info)
{
    unsigned long *val=info;
    unsigned long tmp=*val;
    ARM_DBG_WRITE(c1,c1,4, tmp);
    return;
}

void smp_read_dbgvcr_callback(void *info)
{
    unsigned long tmp;
    unsigned long *val=info;
    ARM_DBG_READ(c0,c7,0, tmp);
    *val=tmp;
    return;
}

void smp_write_dbgvcr_callback(void *info)
{
    unsigned long *val=info;
    unsigned long tmp=*val;
    ARM_DBG_WRITE(c0,c7,0, tmp);
    return;
}



void smp_read_dbgsdsr_callback(void *info)
{
    unsigned long tmp;
    unsigned long *val=info;
     __asm__ __volatile__ ("mrc p15, 0, %0, c1, c3, 1  @SDSR \n\t" :"=r"(tmp));
    *val=tmp;
    return;
}

void smp_write_dbgsdsr_callback(void *info)
{
    unsigned long *val=info;
    unsigned long tmp=*val;
    __asm__ __volatile ("mcr p15, 0, %0, c1, c3, 1 \n\t" ::"r"(tmp));
    return;
}



static spinlock_t wp_lock;

int register_wp_context(struct wp_trace_context_t **wp_tracer_context )
{
  *wp_tracer_context=&wp_tracer;
  return 0;
}

/*
 * enable_hw_watchpoint: Enable the H/W watchpoint.
 * Return error code.
 */
int enable_hw_watchpoint(void)
{
    int i;
    unsigned int args;
    int oslsr;
    int dbglsr;
    pr_notice("[MTK WP] Hotplug disable\n");
    cpu_hotplug_disable();
    for(i = 0 ; i < num_possible_cpus() ; i++) {
        if(cpu_online(i)){
        //if (*(volatile unsigned int *)(DBGDSCR + i * 0x2000) & HDBGEN) {
        	smp_call_function_single(i,smp_read_dbgdscr_callback,&args,1);
            pr_notice("[MTK WP] cpu %d, DBGDSCR 0x%x\n",i,args);
        	if (args& HDBGEN) {
            	pr_notice( "[MTK WP] halting debug mode enabled. Unable to access hardware resources.\n");
           	    return -EPERM;
     	     }

	        //if (*(volatile unsigned int *)(DBGDSCR + i * 0x2000) & MDBGEN) {
	        if (args & MDBGEN) {
    	        /* already enabled */
                 pr_notice( "[MTK WP] already enabled, DBGDSCR = 0x%x\n", args);
       		 }
            cs_cpu_write(wp_tracer.debug_regs[i], DBGLAR ,UNLOCK_KEY);
			cs_cpu_write(wp_tracer.debug_regs[i], DBGOSLAR ,~UNLOCK_KEY);
	        args=args | MDBGEN;
    	    smp_call_function_single(i,smp_write_dbgdscr_callback,&args,1);
            smp_call_function_single(i,smp_read_dbgdscr_callback,&args,1);
            smp_call_function_single(i,smp_read_dbgoslsr_callback,&oslsr,1);
            dbglsr=cs_cpu_read(wp_tracer.debug_regs[i],DBGLSR);
            pr_notice("[MTK WP] cpu %d, DBGLSR 0x%x, DBGOSLSR 0x%x\n",i,dbglsr,oslsr);
            pr_notice("[MTK WP] cpu %d, DBGDSCR 0x%x. (after set dbgdscr)\n",i,args);
//            smp_call_function_single(i,smp_read_dbgsdsr_callback,&args,1);
//            pr_notice("[MTK WP] cpu %d, SDSR 0x%x \n",i,args);
      }
        else
        {
          	pr_notice("[MTK WP] cpu %d, power down(%d) so skip enable_hw_watchpoint\n",i,cpu_online(i));  
        }
    }
    pr_notice("[MTK WP] Hotplug enable\n");
    cpu_hotplug_enable();
    return 0;
}

/*
 * add_hw_watchpoint: add a watch point.
 * @wp_event: pointer to the struct wp_event.
 * Return error code.
 */
int add_hw_watchpoint(struct wp_event *wp_event)
{
    int ret, i, j;
    unsigned long flags;
    unsigned int ctl;

    if (!wp_event) {
        return -EINVAL;
    }
    if (!(wp_event->handler)) {
        return -EINVAL;
    }

    ret = enable_hw_watchpoint();
    if (ret) {
        return ret;
    }

    ctl = DBGWCR_VAL;
    if (wp_event->type == WP_EVENT_TYPE_READ) {
        ctl |= LSC_LDR;
    } else if (wp_event->type == WP_EVENT_TYPE_WRITE) {
        ctl |= LSC_STR;
    } else if (wp_event->type == WP_EVENT_TYPE_ALL) {
        ctl |= LSC_ALL;
    } else {
        return -EINVAL;
    }

    spin_lock_irqsave(&wp_lock, flags);
    for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
        if (!wp_tracer.wp_events[i].in_use) {
            wp_tracer.wp_events[i].in_use = 1;
            break;
        }
    }
    spin_unlock_irqrestore(&wp_lock, flags);

    if (i == MAX_NR_WATCH_POINT) {
        return -EAGAIN;
    }

    wp_tracer.wp_events[i].virt = wp_event->virt & ~3;    /* enforce word-aligned */
    wp_tracer.wp_events[i].phys = wp_event->phys; /* no use currently */
    wp_tracer.wp_events[i].type = wp_event->type;
    wp_tracer.wp_events[i].handler = wp_event->handler;
    wp_tracer.wp_events[i].auto_disable = wp_event->auto_disable;
    pr_notice("[MTK WP] Hotplug disable\n");
    cpu_hotplug_disable();
    pr_notice("[MTK WP] Add watchpoint %d at address 0x%x\n", i, wp_tracer.wp_events[i].virt);
    for(j = 0; j <  num_possible_cpus(); j++) {
        if(cpu_online(j)){
          cs_cpu_write(wp_tracer.debug_regs[j], DBGWVR + (i << 4),wp_tracer.wp_events[i].virt);
          cs_cpu_write(wp_tracer.debug_regs[j], DBGWCR + (i << 4),ctl);

          pr_notice("[MTK WP] cpu %d, DBGWVR%d, &0x%p=0x%x\n",j,i,wp_tracer.debug_regs[j]+ DBGWVR +(i << 4), \
          cs_cpu_read(wp_tracer.debug_regs[j],DBGWVR + (i << 4)));

          pr_notice("[MTK WP] cpu %d, DBGWCR%d, &0x%p=0x%x\n",j,i,wp_tracer.debug_regs[j]+ DBGWCR +(i << 4), \
          cs_cpu_read(wp_tracer.debug_regs[j],DBGWCR + (i << 4)));

        }
        else
        {
          pr_notice("[MTK WP] cpu %d, power down(%d) so skip adding watchpoint\n",j,cpu_online(j));
        }
    }
    pr_notice("[MTK WP] Hotplug enable\n");
    cpu_hotplug_enable();
    return 0;
}

/*
 * del_hw_watchpoint: delete a watch point.
 * @wp_event: pointer to the struct wp_event.
 * Return error code.
 */
int del_hw_watchpoint(struct wp_event *wp_event)
{
    unsigned long flags;
    int i, j;

    if (!wp_event) {
        return -EINVAL;
    }
    pr_notice("[MTK WP] Hotplug diable\n");
    cpu_hotplug_disable();
    spin_lock_irqsave(&wp_lock, flags);
    for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
        if (wp_tracer.wp_events[i].in_use && (wp_tracer.wp_events[i].virt == wp_event->virt)) {
            wp_tracer.wp_events[i].virt = 0;
            wp_tracer.wp_events[i].phys = 0;
            wp_tracer.wp_events[i].type = 0;
            wp_tracer.wp_events[i].handler = NULL;
            wp_tracer.wp_events[i].in_use = 0;
            for(j = 0; j < num_possible_cpus(); j++) {
                if(cpu_online(j)){
                	cs_cpu_write(wp_tracer.debug_regs[j], DBGWCR + (i << 4), \
                    cs_cpu_read(wp_tracer.debug_regs[j], DBGWCR + (i << 4)) & (~WP_EN));
                }
            }
            break;
        }
    }
    spin_unlock_irqrestore(&wp_lock, flags);
    pr_notice("[MTK WP] Hotplug enable\n");
    cpu_hotplug_enable();
    if (i == MAX_NR_WATCH_POINT) {
        return -EINVAL;
    } else {
        return 0;
    }
}

int watchpoint_handler(unsigned long addr, unsigned int fsr, struct pt_regs *regs)
{
    unsigned long wfar, daddr, iaddr;
    int i, ret, j;
#if defined (CONFIG_ARCH_MT6595) || defined(CONFIG_ARCH_MT6752)
/* Notes
 *v7 Debug the address of instruction that triggered the watchpoint is in DBGWFAR
 *v7.1 Debug the address is in DFAR
*/
    asm volatile(
    "MRC p15, 0, %0, c6, c0, 0\n"
    : "=r" (wfar)
    :
    : "cc"
    );
#else
    unsigned cluster_id = raw_smp_processor_id() / get_cluster_core_count();
    if (cluster_id == 0) {
          ARM_DBG_READ(c0,c6,0,wfar);
    }
    else if (cluster_id == 1) {
         ARM_DBG_READ(c0,c6,0,wfar);
    }
#endif
    daddr = addr & ~3;
    iaddr = regs->ARM_pc;
    pr_notice("[MTK WP] addr = 0x%lx, DBGWFAR/DFAR = 0x%lx\n", (unsigned long)addr, wfar);
    pr_notice("[MTK WP] daddr = 0x%lx, iaddr = 0x%lx\n", daddr, iaddr);

    /* update PC to avoid re-execution of the instruction under watching */
    regs->ARM_pc += thumb_mode(regs)? 2: 4;

    for (i = 0; i < MAX_NR_WATCH_POINT; i++) {
        if (wp_tracer.wp_events[i].in_use && wp_tracer.wp_events[i].virt == (daddr)) {
            pr_notice("[MTK WP] Watchpoint %d triggers.\n", i);
            if (wp_tracer.wp_events[i].handler) {
                if (wp_tracer.wp_events[i].auto_disable) {
                    for(j = 0; j < num_possible_cpus(); j++){
                        if(cpu_online(j)){
                          	cs_cpu_write(wp_tracer.debug_regs[j], DBGWCR + (i << 4), \
							cs_cpu_read(wp_tracer.debug_regs[j], DBGWCR + (i << 4)) & (~WP_EN)); 
                        }
                        else
                        {
                        	pr_notice("[MTK WP] cpu %d, power down(%d) so skip adding watchpoint auto-disable\n",j,cpu_online(j));
                        }
                	}
                }
                ret = wp_tracer.wp_events[i].handler(iaddr);
                if (wp_tracer.wp_events[i].auto_disable) {
                    for(j = 0; j < num_possible_cpus(); j++){
                        if(cpu_online(j)){
                            cs_cpu_write(wp_tracer.debug_regs[j], DBGWCR + (i << 4), \
                            cs_cpu_read(wp_tracer.debug_regs[j], DBGWCR + (i << 4)) | WP_EN );
                        }
                        else
                        {
                          pr_notice("[MTK WP] cpu %d, power down(%d) so skip watchpoint auto-disable\n",j,cpu_online(j));
                        }
					}

                }
                return ret;
            } else {
                pr_notice("[MTK WP] No watchpoint handler. Ignore.\n");                
                return 0;
            }
        }
    }

    return 0;
}
int wp_probe(struct platform_device *pdev)
{
    int ret=0;
    int i;
    pr_notice("[MTK WP] watchpoint_probe\n");
    of_property_read_u32(pdev->dev.of_node, "num", &wp_tracer.nr_dbg);
    pr_notice("[MTK WP] get %d debug interface\n",wp_tracer.nr_dbg);
    wp_tracer.debug_regs = kmalloc(sizeof(void *) * (unsigned long )wp_tracer.nr_dbg, GFP_KERNEL);
    if (!wp_tracer.debug_regs) {
		pr_err("[MTK WP] Failed to allocate watchpoint register array\n");
		ret = -ENOMEM;
        goto out;
        }

    for(i = 0; i < wp_tracer.nr_dbg; i++) {
        wp_tracer.debug_regs[i] = of_iomap(pdev->dev.of_node, i);
         if(wp_tracer.debug_regs[i]==NULL)
         {
           pr_notice("[MTK WP] debug_interface %d devicetree mapping failed\n",i);
         }
         else
         {
           pr_notice("[MTK WP] debug_interface %d @ vm:0x%p pm:0x%x \n", i, wp_tracer.debug_regs[i],IO_VIRT_TO_PHYS((unsigned int)wp_tracer.debug_regs[i]));
         }
    }
    ARM_DBG_READ(c0,c0,0,wp_tracer.dbgdidr); 
    wp_tracer.wp_nr=((wp_tracer.dbgdidr & (0xf <<28))>>28);
    wp_tracer.bp_nr=((wp_tracer.dbgdidr & (0xf <<24))>>24);

out:
	return ret;
}
static const struct of_device_id dbg_of_ids[] = {
    {   .compatible = "mediatek,DBG_DEBUG", },
    {}
};

static struct platform_driver wp_driver =
{
        .probe = wp_probe,
        .driver = {
                .name = "wp",
                .bus = &platform_bus_type,
                .owner = THIS_MODULE,
      		    .of_match_table = dbg_of_ids,
        },
};



#ifdef WATCHPOINT_TEST_SUIT
static ssize_t wp_test_suit_show(struct device_driver *driver, char *buf)
{
   return snprintf(buf, PAGE_SIZE, "==Watchpoint test==\n"
                                   "1. test watchpoints in online cpu\n"
                                   "2. verified all cpu's watchpoint after hotplug\n"
                                   "3. verified all cpu's watchpoing after suspend/resume. Not:you have to run test item 1 or 2 to init watchpoint first\n"
   );
}

static ssize_t wp_test_suit_store(struct device_driver *driver, const char *buf, size_t count)
{
    char *p = (char *)buf;
    unsigned int num;

    num = simple_strtoul(p, &p, 10);
        switch(num){
            /* Test Systracker Function */
            case 1:
                wp_test1();
                break;
            case 2:
                wp_test2();
                break;
            case 3:
                wp_test3();
            default:
                break;
        }
    return count;
}
DRIVER_ATTR(wp_test_suit, 0664, wp_test_suit_show, wp_test_suit_store);
#endif
static int __init hw_watchpoint_init(void)
{
    int err;
    int ret;
    spin_lock_init(&wp_lock);
    err = platform_driver_register(&wp_driver);
    if (err) {
                pr_err("[MTK WP] watchpoint registration failed\n");
                return err;
        }
#ifdef WATCHPOINT_TEST_SUIT
    ret  = driver_create_file(&wp_driver.driver , &driver_attr_wp_test_suit);
    if (ret) {
        pr_err("[MTK WP] Fail to create systracker_drv sysfs files");
    }
#endif
#ifdef CONFIG_ARM_LPAE
    hook_fault_code(0x22, watchpoint_handler, SIGTRAP, 0, "watchpoint debug exception");
#else
    hook_fault_code(0x02, watchpoint_handler, SIGTRAP, 0, "watchpoint debug exception");
#endif
    pr_notice("[MTK WP] watchpoint handler init. \n");

    return 0;
}

//EXPORT_SYMBOL(add_hw_watchpoint);
arch_initcall(hw_watchpoint_init);
