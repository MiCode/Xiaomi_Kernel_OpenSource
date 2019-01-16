#include <linux/init.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/delay.h>
#include <asm/localtimer.h>
#include <asm/fiq_glue.h>
#include <mach/mt_reg_base.h>
#include <mach/smp.h>
#include <mach/sync_write.h>
#include <mach/hotplug.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_spm_idle.h>
#include <mach/wd_api.h>
#include <mach/mt_boot.h>                   //mt_get_chip_sw_ver
#include <mach/mt_clkmgr.h>

#define SLAVE1_MAGIC_REG (SRAMROM_BASE+0x38)
#define SLAVE2_MAGIC_REG (SRAMROM_BASE+0x38)
#define SLAVE3_MAGIC_REG (SRAMROM_BASE+0x38)
#define SLAVE4_MAGIC_REG (SRAMROM_BASE+0x3C)
#define SLAVE5_MAGIC_REG (SRAMROM_BASE+0x3C)
#define SLAVE6_MAGIC_REG (SRAMROM_BASE+0x3C)
#define SLAVE7_MAGIC_REG (SRAMROM_BASE+0x3C)

#define SLAVE1_MAGIC_NUM 0x534C4131
#define SLAVE2_MAGIC_NUM 0x4C415332
#define SLAVE3_MAGIC_NUM 0x41534C33
#define SLAVE4_MAGIC_NUM 0x534C4134
#define SLAVE5_MAGIC_NUM 0x4C415335
#define SLAVE6_MAGIC_NUM 0x41534C36
#define SLAVE7_MAGIC_NUM 0x534C4137

#define SLAVE_JUMP_REG  (SRAMROM_BASE+0x34)


#define CA15L_TYPEID 0x410FC0D0
#define CA7_TYPEID 0x410FC070
#define CPU_TYPEID_MASK 0xfffffff0
#define read_midr()							\
	({								\
		register unsigned int ret;				\
		__asm__ __volatile__ ("mrc   p15, 0, %0, c0, c0, 0 \n\t" \
				      :"=r"(ret));			\
		ret;							\
	})

//inline int is_cpu_type(int type) 
#define is_cpu_type(type)						\
	({								\
		((read_midr() & CPU_TYPEID_MASK) == type) ? 1 : 0;	\
	})

extern void mt_secondary_startup(void);
extern void irq_raise_softirq(const struct cpumask *mask, unsigned int irq);
extern void mt_gic_secondary_init(void);
extern u32 get_devinfo_with_index(u32 index);


extern unsigned int irq_total_secondary_cpus;
static unsigned int is_secondary_cpu_first_boot;
//static DEFINE_SPINLOCK(boot_lock);


/*
 * Write pen_release in a way that is guaranteed to be visible to all
 * observers, irrespective of whether they're taking part in coherency
 * or not.  This is necessary for the hotplug code to work reliably.
 */
static void __cpuinit write_pen_release(int val)
{
    pen_release = val;
    smp_wmb();
    __cpuc_flush_dcache_area((void *)&pen_release, sizeof(pen_release));
    outer_clean_range(__pa(&pen_release), __pa(&pen_release + 1));
}

/*
 * 20140512 marc.huang
 * 1. only need to get core count if !defined(CONFIG_OF)
 * 2. only set possible cpumask in mt_smp_init_cpus() if !defined(CONFIG_OF)
 */
#if !defined(CONFIG_OF)
static int _mt_smp_get_core_count(void)
{
    unsigned int cores = 0;

    //asm volatile(
    //"MRC p15, 1, %0, c9, c0, 2\n"
    //: "=r" (cores)
    //:
    //: "cc"
    //);
    //
    //cores = cores >> 24;
    //cores += 1;

    //TODO: use efuse api to get core numbers?
    cores = 4;

    return cores;  
}
#endif //#if !defined(CONFIG_OF)

void __cpuinit mt_smp_secondary_init(unsigned int cpu)
{
    struct wd_api *wd_api = NULL;

    pr_debug("Slave cpu init\n");
    HOTPLUG_INFO("platform_secondary_init, cpu: %d\n", cpu);

    mt_gic_secondary_init();

    /*
     * let the primary processor know we're out of the
     * pen, then head off into the C entry point
     */
    write_pen_release(-1);

    get_wd_api(&wd_api);
    if (wd_api)
        wd_api->wd_cpu_hot_plug_on_notify(cpu);

    fiq_glue_resume();

    /*
     * Synchronise with the boot thread.
     */
    //spin_lock(&boot_lock);
    //spin_unlock(&boot_lock);
}

int __cpuinit mt_smp_boot_secondary(unsigned int cpu, struct task_struct *idle)
{
    unsigned long timeout;
    CHIP_SW_VER ver = mt_get_chip_sw_ver();
    unsigned int phy_cpu = cpu;

    pr_crit("Boot slave CPU\n");

    atomic_inc(&hotplug_cpu_count);

    if (ver == CHIP_SW_VER_01)
    {
    #ifdef CONFIG_MTK_FORCE_CLUSTER1
        phy_cpu += 4;
    #endif
    }

    /*
     * Set synchronisation state between this boot processor
     * and the secondary one
     */
    //spin_lock(&boot_lock);

    HOTPLUG_INFO("mt_smp_boot_secondary, cpu: %d\n", cpu);
    /*
     * The secondary processor is waiting to be released from
     * the holding pen - release it, then wait for it to flag
     * that it has been released by resetting pen_release.
     *
     * Note that "pen_release" is the hardware CPU ID, whereas
     * "cpu" is Linux's internal ID.
     */
    /*
     * This is really belt and braces; we hold unintended secondary
     * CPUs in the holding pen until we're ready for them.  However,
     * since we haven't sent them a soft interrupt, they shouldn't
     * be there.
     */
    write_pen_release(cpu);

    mt_smp_set_boot_addr(virt_to_phys(mt_secondary_startup), cpu);
    switch(cpu)
    {
        case 1:
            if (is_secondary_cpu_first_boot)
            {
                --is_secondary_cpu_first_boot;
                if ((REG_READ(BOOTROM_SEC_CTRL) & SW_ROM_PD) == SW_ROM_PD)
                {
                    spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
                }
                else
                {
                    mt_reg_sync_writel(SLAVE1_MAGIC_NUM, SLAVE1_MAGIC_REG);
                    HOTPLUG_INFO("SLAVE1_MAGIC_NUM:%x\n", SLAVE1_MAGIC_NUM);
                }
            }
        #ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
            else
            {
                spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
            }
        #endif
            break;
        case 2:
            if (is_secondary_cpu_first_boot)
            {
                --is_secondary_cpu_first_boot;
                if ((REG_READ(BOOTROM_SEC_CTRL) & SW_ROM_PD) == SW_ROM_PD)
                {
                    spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
                }
                else
                {
                    mt_reg_sync_writel(SLAVE2_MAGIC_NUM, SLAVE2_MAGIC_REG);
                    HOTPLUG_INFO("SLAVE2_MAGIC_NUM:%x\n", SLAVE2_MAGIC_NUM);
                }
            }
        #ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
            else
            {
                spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
            }
        #endif
            break;
        case 3:
            if (is_secondary_cpu_first_boot)
            {
                --is_secondary_cpu_first_boot;
                if ((REG_READ(BOOTROM_SEC_CTRL) & SW_ROM_PD) == SW_ROM_PD)
                {
                    spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
                }
                else
                {
                    mt_reg_sync_writel(SLAVE3_MAGIC_NUM, SLAVE3_MAGIC_REG);
                    HOTPLUG_INFO("SLAVE3_MAGIC_NUM:%x\n", SLAVE3_MAGIC_NUM);
                }
            }
        #ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
            else
            {
                spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
            }
        #endif
            break;
        case 4:
            if (is_secondary_cpu_first_boot)
            {
                --is_secondary_cpu_first_boot;
                if ((REG_READ(BOOTROM_SEC_CTRL) & SW_ROM_PD) == SW_ROM_PD)
                {
                    spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
                }
                else
                {
                    mt_reg_sync_writel(SLAVE4_MAGIC_NUM, SLAVE4_MAGIC_REG);
                    HOTPLUG_INFO("SLAVE4_MAGIC_NUM:%x\n", SLAVE4_MAGIC_NUM);
                }
            }
        #ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
            else
            {
                spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
            }
        #endif
            break;

        case 5:
            if (is_secondary_cpu_first_boot)
            {
                --is_secondary_cpu_first_boot;
                if ((REG_READ(BOOTROM_SEC_CTRL) & SW_ROM_PD) == SW_ROM_PD)
                {
                    spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
                }
                else
                {
                    mt_reg_sync_writel(SLAVE5_MAGIC_NUM, SLAVE5_MAGIC_REG);
                    HOTPLUG_INFO("SLAVE5_MAGIC_NUM:%x\n", SLAVE5_MAGIC_NUM);
                }
            }
        #ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
            else
            {
                if ((cpu_online(4) == 0) && (cpu_online(6) == 0) && (cpu_online(7) == 0))
                {
                    HOTPLUG_INFO("up CPU%d fail, please up CPU4 first\n", cpu);
                    //spin_unlock(&boot_lock);
                    atomic_dec(&hotplug_cpu_count);
                    return -ENOSYS;
                }
                spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
            }
        #endif
            break;
        case 6:
            if (is_secondary_cpu_first_boot)
            {
                --is_secondary_cpu_first_boot;
                if ((REG_READ(BOOTROM_SEC_CTRL) & SW_ROM_PD) == SW_ROM_PD)
                {
                    spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
                }
                else
                {
                    mt_reg_sync_writel(SLAVE6_MAGIC_NUM, SLAVE6_MAGIC_REG);
                    HOTPLUG_INFO("SLAVE6_MAGIC_NUM:%x\n", SLAVE6_MAGIC_NUM);
                }
            }
        #ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
            else
            {
                if ((cpu_online(4) == 0) && (cpu_online(5) == 0) && (cpu_online(7) == 0))
                {
                    HOTPLUG_INFO("up CPU%d fail, please up CPU4 first\n", cpu);
                    //spin_unlock(&boot_lock);
                    atomic_dec(&hotplug_cpu_count);
                    return -ENOSYS;
                }
                spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
            }
        #endif
            break;

        case 7:
            if (is_secondary_cpu_first_boot)
            {
                --is_secondary_cpu_first_boot;
                if ((REG_READ(BOOTROM_SEC_CTRL) & SW_ROM_PD) == SW_ROM_PD)
                {
                    spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
                }
                else
                {
                    mt_reg_sync_writel(SLAVE7_MAGIC_NUM, SLAVE7_MAGIC_REG);
                    HOTPLUG_INFO("SLAVE7_MAGIC_NUM:%x\n", SLAVE7_MAGIC_NUM);
                }
            }
        #ifdef CONFIG_HOTPLUG_WITH_POWER_CTRL
            else
            {
                if ((cpu_online(4) == 0) && (cpu_online(5) == 0) && (cpu_online(6) == 0))
                {
                    HOTPLUG_INFO("up CPU%d fail, please up CPU4 first\n", cpu);
                    //spin_unlock(&boot_lock);
                    atomic_dec(&hotplug_cpu_count);
                    return -ENOSYS;
                }
                spm_mtcmos_ctrl_cpu(phy_cpu, STA_POWER_ON, 1);
            }
        #endif
            break;

        default:
            break;

    }

    smp_cross_call(cpumask_of(cpu));

    /*
     * Now the secondary core is starting up let it run its
     * calibrations, then wait for it to finish
     */
    //spin_unlock(&boot_lock);

    timeout = jiffies + (1 * HZ);
    while (time_before(jiffies, timeout)) {
        smp_rmb();
        if (pen_release == -1)
            break;

        udelay(10);
    }

    if (pen_release == -1)
    {
    #if 0
        pr_emerg("SPM_CA7_CPU0_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPU0_PWR_CON));
        pr_emerg("SPM_CA7_CPU1_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPU1_PWR_CON));
        pr_emerg("SPM_CA7_CPU2_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPU2_PWR_CON));
        pr_emerg("SPM_CA7_CPU3_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPU3_PWR_CON));
        pr_emerg("SPM_CA7_DBG_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_DBG_PWR_CON));
        pr_emerg("SPM_CA7_CPUTOP_PWR_CON: 0x%08x\n", REG_READ(SPM_CA7_CPUTOP_PWR_CON));
        pr_emerg("SPM_CA15_CPU0_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPU0_PWR_CON));
        pr_emerg("SPM_CA15_CPU1_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPU1_PWR_CON));
        pr_emerg("SPM_CA15_CPU2_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPU2_PWR_CON));
        pr_emerg("SPM_CA15_CPU3_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPU3_PWR_CON));
        pr_emerg("SPM_CA15_CPUTOP_PWR_CON: 0x%08x\n", REG_READ(SPM_CA15_CPUTOP_PWR_CON));
    #endif
        return 0;
    }
    else
    {
        //FIXME: consider E1 case
        if (is_cpu_type(CA7_TYPEID))
        {
            //write back stage pc on ca7
            mt_reg_sync_writel(cpu + 8, DBG_MON_CTL);
            pr_emerg("CPU%u, DBG_MON_CTL: 0x%08x, DBG_MON_DATA: 0x%08x\n", cpu, *(volatile u32 *)(DBG_MON_CTL), *(volatile u32 *)(DBG_MON_DATA));
        }
        else
        {
            //decode statge pc on ca15l
            mt_reg_sync_writel(3+ (cpu - 4) * 4, CA15L_MON_SEL);
            pr_emerg("CPU%u, CA15L_MON_SEL: 0x%08x, CA15L_MON: 0x%08x\n", cpu, *(volatile u32 *)(CA15L_MON_SEL), *(volatile u32 *)(CA15L_MON));
        }

        on_each_cpu((smp_call_func_t)dump_stack, NULL, 0);
        atomic_dec(&hotplug_cpu_count);
        return -ENOSYS;
    }
}

#if defined (MT_SMP_VIRTUAL_BOOT_ADDR)
/** this routine is to solve BOOT_ADDR concurrence:
 * 1. to provide a boot_addr array for every CPUs as 2nd boot_addr, 
 *    for both purpose of dormant and hotplug.
 * 2. a common wakeup routine stocked in BOOT_ADDR, 
 *    to jump 2nd boot_addr.
 **/

static unsigned boot_addr_array[8 /* NR_CPUS */];
/** the common wakeup routine:
 * has one assumption:  4 cores per cluster. 
 **/
__naked void mt_smp_boot(void)
{
	__asm__ __volatile__ (
		"mrc p15, 0, r0, c0, c0, 5	@ pmidr \n\t" 
		"ubfx r1, r0, #8, #4 		@ tid \n\t"
		"and r0, r0, #0xf		@ cid \n\t"
		"add r2, r0, r1, lsl #2		@ idx = tid*4+cid \n\t"
		"adr r3, 1f			@ pa1 \n\t"
		"ldm r3, {r4-r5}		@ va1, va2 \n\t"
		"sub r4, r3, r4			@ off=pa1-va1 \n\t"
		"add r3, r5, r4 		@ pa2=v2+off \n\t"
		"ldr lr, [r3, r2, lsl #2]	@ baddr= array+idx*2 \n\t"
		"bx  lr				@ jump \n\t"
		"1: .long . 		\n\t"
		".long boot_addr_array	\n\t"
		); 
}

void mt_smp_set_boot_addr(u32 addr, int cpu)
{
    boot_addr_array[cpu] = addr;
    __cpuc_flush_dcache_area(boot_addr_array, sizeof(boot_addr_array));
}
#endif

void __init mt_smp_init_cpus(void)
{
    unsigned int i = 0;
    if(mt_get_chip_hw_code()==0x6595)
    {
    	iomap_infra();
    }

    CHIP_SW_VER ver = mt_get_chip_sw_ver();

    if (ver == CHIP_SW_VER_01)
    {
    #ifdef CONFIG_MTK_FORCE_CLUSTER1
        /* Enable CA15L snoop function */
        REG_WRITE(MISCDBG, (REG_READ(MISCDBG) & ~CA15L_AINACTS) & ~CA15L_ACINACTM);

        spm_mtcmos_ctrl_cpusys0(STA_POWER_DOWN, 1);
    #else //#ifdef CONFIG_MTK_FORCE_CLUSTER1
        /* Enable CA7 snoop function */
        REG_WRITE(BUS_CONFIG, REG_READ(BUS_CONFIG) & ~CA7_ACINACTM);

        /* Enables DVM */
        asm volatile(
            "MRC p15, 0, %0, c1, c0, 1\n"
            "BIC %0, %0, #1 << 15\n"        /* DDVM: bit15 */
            "MCR p15, 0, %0, c1, c0, 1\n"
            : "+r"(i)
            :
            : "cc"
        );
        //FIXME: arm Errata 812076?

        /* Enable snoop requests and DVM message requests*/
        REG_WRITE(CCI400_SI4_SNOOP_CONTROL, REG_READ(CCI400_SI4_SNOOP_CONTROL) | (SNOOP_REQ | DVM_MSG_REQ));
        while (REG_READ(CCI400_STATUS) & CHANGE_PENDING);
    #endif //#ifdef CONFIG_MTK_FORCE_CLUSTER1
    }
    else
    {
        /* Enable CA7 snoop function */
        REG_WRITE(BUS_CONFIG, REG_READ(BUS_CONFIG) & ~CA7_ACINACTM);

        /* Enables DVM */
        asm volatile(
            "MRC p15, 0, %0, c1, c0, 1\n"
            "BIC %0, %0, #1 << 15\n"        /* DDVM: bit15 */
            "MCR p15, 0, %0, c1, c0, 1\n"
            : "+r"(i)
            :
            : "cc"
        );
        //FIXME: arm Errata 812076?

        /* Enable snoop requests and DVM message requests*/
        REG_WRITE(CCI400_SI4_SNOOP_CONTROL, REG_READ(CCI400_SI4_SNOOP_CONTROL) | (SNOOP_REQ | DVM_MSG_REQ));
        while (REG_READ(CCI400_STATUS) & CHANGE_PENDING);

        /* Enable cross trigger */
        REG_WRITE(MCUSYS_CONFIG, REG_READ(MCUSYS_CONFIG) | (CA15_EVENTI_SEL | CA7_EVENTI_SEL));
    }

    /* Set CONFIG_RES[31:0]=32h000F_FFFF to disable rguX reset wait for cpuX L1 pdn ack */
    REG_WRITE(CONFIG_RES, 0x000FFFFF);

    /*
     * 20140512 marc.huang
     * 1. only need to get core count if !defined(CONFIG_OF)
     * 2. only set possible cpumask in mt_smp_init_cpus() if !defined(CONFIG_OF)
     */
#if !defined(CONFIG_OF)
    {
    unsigned int ncores;
    ncores = _mt_smp_get_core_count();
    if (ncores > NR_CPUS) {
        pr_warn(
               "L2CTLR core count (%d) > NR_CPUS (%d)\n", ncores, NR_CPUS);
        pr_warn(
               "set nr_cores to NR_CPUS (%d)\n", NR_CPUS);
        ncores = NR_CPUS;
    }
    
    for (i = 0; i < ncores; i++)
        set_cpu_possible(i, true);
    }
#endif //#if !defined(CONFIG_OF)

    irq_total_secondary_cpus = num_possible_cpus() - 1;
    is_secondary_cpu_first_boot = num_possible_cpus() - 1;

    set_smp_cross_call(irq_raise_softirq);

    //XXX: asssume only boot cpu power on and all non-boot cpus power off after preloader stage
    //if (ncores > 4)
    //    spm_mtcmos_ctrl_cpusys1_init_1st_bring_up(STA_POWER_ON);
    //else
    //    spm_mtcmos_ctrl_cpusys1_init_1st_bring_up(STA_POWER_DOWN);
}

void __init mt_smp_prepare_cpus(unsigned int max_cpus)
{
    /*
     * 20140512 marc.huang
     * 1. only need to get core count if !defined(CONFIG_OF)
     * 2. only set possible cpumask in mt_smp_init_cpus() if !defined(CONFIG_OF)
     */
    int i;

    for (i = 0; i < max_cpus; i++)
        set_cpu_present(i, true);

    /* write the address of slave startup into the system-wide flags register */
    mt_reg_sync_writel(virt_to_phys(mt_secondary_startup), SLAVE_JUMP_REG);

    /* write the address of slave startup into boot address register for bootrom power down mode */
#if defined (MT_SMP_VIRTUAL_BOOT_ADDR)
    mt_reg_sync_writel(virt_to_phys(mt_smp_boot), BOOTROM_BOOT_ADDR);
#else
    mt_reg_sync_writel(virt_to_phys(mt_secondary_startup), BOOTROM_BOOT_ADDR);
#endif
    /* initial spm_mtcmos memory map */
    spm_mtcmos_cpu_init();
}

struct smp_operations __initdata mt_smp_ops = {
    .smp_init_cpus          = mt_smp_init_cpus,
    .smp_prepare_cpus       = mt_smp_prepare_cpus,
    .smp_secondary_init     = mt_smp_secondary_init,
    .smp_boot_secondary     = mt_smp_boot_secondary,
#ifdef CONFIG_HOTPLUG_CPU
    .cpu_kill               = mt_cpu_kill,
    .cpu_die                = mt_cpu_die,
    .cpu_disable            = mt_cpu_disable,
#endif
};
