#include <linux/device.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <mach/mt_boot.h>
#ifdef CONFIG_SMP
#include <mach/hotplug.h>
#include <linux/cpu.h>
#endif

#define UNLOCK_KEY 0xC5ACCE55
#define HDBGEN (1 << 14)
#define MDBGEN (1 << 15)
#define DBGLAR 0xF0170FB0
#define DBGOSLAR 0xF0170300
#define DBGDSCR 0xF0170088
#define DBGLAR2 0xF0190FB0
#define DBGOSLAR2 0xF0190300
#define DBGDSCR2 0xF0190088
#define DBGWVR_BASE 0xF0170180
#define DBGWCR_BASE 0xF01701C0
#define DBGBVR_BASE 0xF0170100
#define DBGBCR_BASE 0xF0170140
#define DBGWVR_BASE2 0xF0190180
#define DBGWCR_BASE2 0xF01901C0
#define DBGBVR_BASE2 0xF0190100
#define DBGBCR_BASE2 0xF0190140

#define DBGWFAR 0xF0170018
#define MAX_NR_WATCH_POINT 4
#define MAX_NR_BREAK_POINT 6
#define NUM_CPU 4   // # of cpu in a cluster
extern void save_dbg_regs(unsigned int data[]);
extern void restore_dbg_regs(unsigned int data[]);
extern int get_cluster_core_count(void);

#define ARM_DBG_READ(N, M, OP2, VAL) do {\
		asm volatile("mrc p14, 0, %0, " #N "," #M ", " #OP2 : "=r" (VAL));\
} while (0)

#define ARM_DBG_WRITE(N, M, OP2, VAL) do {\
		asm volatile("mcr p14, 0, %0, " #N "," #M ", " #OP2 : : "r" (VAL));\
} while (0)



void save_dbg_regs(unsigned int data[])
{
	int i;

	// actually only cpu0 will execute this function

	data[0] = readl(IOMEM(DBGDSCR));
	for(i = 0; i < MAX_NR_WATCH_POINT; i++) {
		data[i*2+1] = readl(IOMEM(DBGWVR_BASE + i * sizeof(unsigned int *)));
		data[i*2+2] = readl(IOMEM(DBGWCR_BASE + i * sizeof(unsigned int *)));
	}

	for(i = 0; i < MAX_NR_BREAK_POINT; i++) {
		data[i*2+9] = readl(IOMEM(DBGBVR_BASE + i * sizeof(unsigned int *)));
		data[i*2+10] = readl(IOMEM(DBGBCR_BASE + i * sizeof(unsigned int *)));
	}
}

void restore_dbg_regs(unsigned int data[])
{
	int i;

	// actually only cpu0 will execute this function
	
	mt_reg_sync_writel(UNLOCK_KEY, DBGLAR);
	mt_reg_sync_writel(~UNLOCK_KEY, DBGOSLAR);
	mt_reg_sync_writel(data[0], DBGDSCR);

	for(i = 0; i < MAX_NR_WATCH_POINT; i++) {
		mt_reg_sync_writel(data[i*2+1], DBGWVR_BASE + i * sizeof(unsigned int *));
		mt_reg_sync_writel(data[i*2+2], DBGWCR_BASE + i * sizeof(unsigned int *));
	} 
		
	for(i = 0; i < MAX_NR_BREAK_POINT; i++) {
		mt_reg_sync_writel(data[i*2+9], DBGBVR_BASE + i * sizeof(unsigned int *));
		mt_reg_sync_writel(data[i*2+10], DBGBCR_BASE + i * sizeof(unsigned int *));
	}
}


#ifdef CONFIG_SMP
int __cpuinit
dbgregs_hotplug_callback(struct notifier_block *nfb, unsigned long action, void *hcpu)
{
//		printk(KERN_ALERT "In hotplug callback\n");
	int i;
	register unsigned int tmp;
	unsigned int cpu = (unsigned int) hcpu;
	CHIP_SW_VER ver = mt_get_chip_sw_ver();
	unsigned cluster_id = cpu / get_cluster_core_count();
	
	//printk("regs_hotplug_callback cpu = %d\n", cpu);
	switch (action) {
	case CPU_STARTING:
		// for cluster 0
		if (ver == CHIP_SW_VER_01) {
			if(cluster_id == 0) {
				//printk("cpu = %d, cluster = %d\n", cpu, cluster_id);
				#ifndef CONFIG_MTK_FORCE_CLUSTER1
				mt_reg_sync_writel(UNLOCK_KEY, DBGLAR + cpu *0x2000);
				//printk("after write UNLOCK to DBGLAR\n");

				ARM_DBG_WRITE(c1, c0, 4, ~UNLOCK_KEY);
				//printk("after write ~UNLOCK to DBGOSLAR\n");

				ARM_DBG_READ(c0, c2, 2, tmp);
				tmp |= readl(IOMEM(DBGDSCR));
				ARM_DBG_WRITE(c0, c2, 2, tmp);
				//printk("after write to DBGDSCR: 0x%x\n", readl(DBGDSCR + cpu *0x2000));
				#else
				mt_reg_sync_writel(UNLOCK_KEY, DBGLAR2 + cpu *0x2000);
				//printk("after write UNLOCK to DBGLAR\n");
				
				ARM_DBG_WRITE(c1, c0, 4, ~UNLOCK_KEY);
				//printk("after write ~UNLOCK to DBGOSLAR\n");
				
				ARM_DBG_READ(c0, c2, 2, tmp);;
				tmp |= readl(IOMEM(DBGDSCR2));
				ARM_DBG_WRITE(c0, c2, 2, tmp);
				
				isb();
				//printk("after write to DBGDSCR: 0x%x\n", readl(DBGDSCR + cpu *0x2000));
				#endif
							
				#ifndef CONFIG_MTK_FORCE_CLUSTER1 
				ARM_DBG_WRITE(c0, c0, 6, readl(IOMEM(DBGWVR_BASE + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 6, readl(IOMEM(DBGWVR_BASE + 1 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c2, 6, readl(IOMEM(DBGWVR_BASE + 2 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c3, 6, readl(IOMEM(DBGWVR_BASE + 3 * sizeof(unsigned int*))));
	   
				ARM_DBG_WRITE(c0, c0, 7, readl(IOMEM(DBGWCR_BASE + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 7, readl(IOMEM(DBGWCR_BASE + 1 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c2, 7, readl(IOMEM(DBGWCR_BASE + 2 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c3, 7, readl(IOMEM(DBGWCR_BASE + 3 * sizeof(unsigned int*))));  
					
				#else
				ARM_DBG_WRITE(c0, c0, 6, readl(IOMEM(DBGWVR_BASE2 + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 6, readl(IOMEM(DBGWVR_BASE2 + 1 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c2, 6, readl(IOMEM(DBGWVR_BASE2 + 2 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c3, 6, readl(IOMEM(DBGWVR_BASE2 + 3 * sizeof(unsigned int*))));
	   
				ARM_DBG_WRITE(c0, c0, 7, readl(IOMEM(DBGWCR_BASE2 + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 7, readl(IOMEM(DBGWCR_BASE2 + 1 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c2, 7, readl(IOMEM(DBGWCR_BASE2 + 2 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c3, 7, readl(IOMEM(DBGWCR_BASE2 + 3 * sizeof(unsigned int*))));  	
				#endif

				#ifndef CONFIG_MTK_FORCE_CLUSTER1
				ARM_DBG_WRITE(c0, c0, 4, readl(IOMEM(DBGBVR_BASE + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 4, readl(IOMEM(DBGBVR_BASE + 1 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c2, 4, readl(IOMEM(DBGBVR_BASE + 2 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c3, 4, readl(IOMEM(DBGBVR_BASE + 3 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c4, 4, readl(IOMEM(DBGBVR_BASE + 4 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c5, 4, readl(IOMEM(DBGBVR_BASE + 5 * sizeof(unsigned int*))));
					
				ARM_DBG_WRITE(c0, c0, 5, readl(IOMEM(DBGBCR_BASE + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 5, readl(IOMEM(DBGBCR_BASE + 1 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c2, 5, readl(IOMEM(DBGBCR_BASE + 2 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c3, 5, readl(IOMEM(DBGBCR_BASE + 3 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c4, 5, readl(IOMEM(DBGBCR_BASE + 4 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c5, 5, readl(IOMEM(DBGBCR_BASE + 5 * sizeof(unsigned int*)))); 				
				#else
				ARM_DBG_WRITE(c0, c0, 4, readl(IOMEM(DBGBVR_BASE2 + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 4, readl(IOMEM(DBGBVR_BASE2 + 1 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c2, 4, readl(IOMEM(DBGBVR_BASE2 + 2 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c3, 4, readl(IOMEM(DBGBVR_BASE2 + 3 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c4, 4, readl(IOMEM(DBGBVR_BASE2 + 4 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c5, 4, readl(IOMEM(DBGBVR_BASE2 + 5 * sizeof(unsigned int*))));
					
				ARM_DBG_WRITE(c0, c0, 5, readl(IOMEM(DBGBCR_BASE2 + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 5, readl(IOMEM(DBGBCR_BASE2 + 1 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c2, 5, readl(IOMEM(DBGBCR_BASE2 + 2 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c3, 5, readl(IOMEM(DBGBCR_BASE2 + 3 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c4, 5, readl(IOMEM(DBGBCR_BASE2 + 4 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c5, 5, readl(IOMEM(DBGBCR_BASE2 + 5 * sizeof(unsigned int*)))); 
				#endif
				isb();
				//printk("after write to cpu = %d, cluster = %d\n", cpu, cluster_id);
			}
		} else{
			if(cluster_id == 0) {
				//printk("cpu = %d, cluster = %d\n", cpu, cluster_id);
				mt_reg_sync_writel(UNLOCK_KEY, DBGLAR + cpu *0x2000);
			} else {
				cpu = cpu % get_cluster_core_count();
				mt_reg_sync_writel(UNLOCK_KEY, DBGLAR2 + cpu *0x2000);
			}
				//printk("after write UNLOCK to DBGLAR\n");
				ARM_DBG_WRITE(c1, c0, 4, ~UNLOCK_KEY);
				//printk("after write ~UNLOCK to DBGOSLAR\n");

				ARM_DBG_READ(c0, c2, 2, tmp);
				tmp |= readl(IOMEM(DBGDSCR));
				ARM_DBG_WRITE(c0, c2, 2, tmp);
				//printk("after write to DBGDSCR: 0x%x\n", readl(DBGDSCR + cpu *0x2000));
							
				ARM_DBG_WRITE(c0, c0, 6, readl(IOMEM(DBGWVR_BASE + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 6, readl(IOMEM(DBGWVR_BASE + 1 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c2, 6, readl(IOMEM(DBGWVR_BASE + 2 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c3, 6, readl(IOMEM(DBGWVR_BASE + 3 * sizeof(unsigned int*))));
	   
				ARM_DBG_WRITE(c0, c0, 7, readl(IOMEM(DBGWCR_BASE + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 7, readl(IOMEM(DBGWCR_BASE + 1 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c2, 7, readl(IOMEM(DBGWCR_BASE + 2 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c3, 7, readl(IOMEM(DBGWCR_BASE + 3 * sizeof(unsigned int*))));  
					
				ARM_DBG_WRITE(c0, c0, 4, readl(IOMEM(DBGBVR_BASE + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 4, readl(IOMEM(DBGBVR_BASE + 1 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c2, 4, readl(IOMEM(DBGBVR_BASE + 2 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c3, 4, readl(IOMEM(DBGBVR_BASE + 3 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c4, 4, readl(IOMEM(DBGBVR_BASE + 4 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c5, 4, readl(IOMEM(DBGBVR_BASE + 5 * sizeof(unsigned int*))));
					
				ARM_DBG_WRITE(c0, c0, 5, readl(IOMEM(DBGBCR_BASE + 0 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c1, 5, readl(IOMEM(DBGBCR_BASE + 1 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c2, 5, readl(IOMEM(DBGBCR_BASE + 2 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c3, 5, readl(IOMEM(DBGBCR_BASE + 3 * sizeof(unsigned int*))));
				ARM_DBG_WRITE(c0, c4, 5, readl(IOMEM(DBGBCR_BASE + 4 * sizeof(unsigned int*))));	
				ARM_DBG_WRITE(c0, c5, 5, readl(IOMEM(DBGBCR_BASE + 5 * sizeof(unsigned int*)))); 				
				isb();
				//printk("after write to cpu = %d, cluster = %d\n", cpu, cluster_id);		
			
		}
		
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
