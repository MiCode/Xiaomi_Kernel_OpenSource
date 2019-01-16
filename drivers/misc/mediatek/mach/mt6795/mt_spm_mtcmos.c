#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/delay.h>    //udelay
#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_irq.h>
#include <linux/of_address.h>
#endif

#include <mach/mt_typedefs.h>
#include <mach/mt_spm_cpu.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_spm_mtcmos_internal.h>
#include <mach/hotplug.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_dcm.h>
#include <mach/mt_boot.h> //mt_get_chip_sw_ver

/**************************************
 * for CPU MTCMOS
 **************************************/
extern void bigcore_power_off(void);    //definition in mediatek/platform/mt6595/kernel/drivers/power/da9210.c
extern void bigcore_power_on(void);     //definition in mediatek/platform/mt6595/kernel/drivers/power/da9210.c
extern struct mutex cpufreq_mutex;
extern int ptp2_pre_init(void);
extern void turn_off_FBB(void);
extern void turn_off_SPARK(void);
extern void dump_dcc_regs(void);

static DEFINE_SPINLOCK(spm_cpu_lock);
#ifdef CONFIG_OF
void __iomem *spm_cpu_base;
#endif //#ifdef CONFIG_OF

int spm_mtcmos_cpu_init(void)
{
#ifdef CONFIG_OF
    struct device_node *node;

    node = of_find_compatible_node(NULL, NULL, "mediatek,SLEEP");
    if (!node) {
        pr_err("find SLEEP node failed\n");
        return -EINVAL;
    }
    spm_cpu_base = of_iomap(node, 0);
    if (!spm_cpu_base) {
        pr_err("base spm_cpu_base failed\n");
        return -EINVAL;
    }

    return 0;
#else //#ifdef CONFIG_OF
    return -EINVAL;
#endif //#ifdef CONFIG_OF
}

void spm_mtcmos_cpu_lock(unsigned long *flags)
{
	mutex_lock(&cpufreq_mutex);
	spin_lock_irqsave(&spm_cpu_lock, *flags);
}

void spm_mtcmos_cpu_unlock(unsigned long *flags)
{
	spin_unlock_irqrestore(&spm_cpu_lock, *flags);
	mutex_unlock(&cpufreq_mutex);
}

typedef int (*spm_cpu_mtcmos_ctrl_func)(int state, int chkWfiBeforePdn);
static spm_cpu_mtcmos_ctrl_func spm_cpu_mtcmos_ctrl_funcs[] =
{
    spm_mtcmos_ctrl_cpu0,
    spm_mtcmos_ctrl_cpu1,
    spm_mtcmos_ctrl_cpu2,
    spm_mtcmos_ctrl_cpu3,
    spm_mtcmos_ctrl_cpu4,
    spm_mtcmos_ctrl_cpu5,
    spm_mtcmos_ctrl_cpu6,
    spm_mtcmos_ctrl_cpu7
};
int spm_mtcmos_ctrl_cpu(unsigned int cpu, int state, int chkWfiBeforePdn)
{
    return (*spm_cpu_mtcmos_ctrl_funcs[cpu])(state, chkWfiBeforePdn);
}

int spm_mtcmos_ctrl_cpu0(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN)
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU0_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU0_L1_PDN, spm_read(SPM_CA7_CPU0_L1_PDN) | L1_PDN);
        while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA7_CPU0_PWR_CON, (spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU0) != CA7_CPU0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU0) != CA7_CPU0));
        
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~PWR_ISO);

        spm_write(SPM_CA7_CPU0_L1_PDN, spm_read(SPM_CA7_CPU0_L1_PDN) & ~L1_PDN);
        while ((spm_read(SPM_CA7_CPU0_L1_PDN) & L1_PDN_ACK) != 0);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) & ~SRAM_CKISO);

        spm_write(SPM_CA7_CPU0_PWR_CON, spm_read(SPM_CA7_CPU0_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu1(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN)
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU1_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU1_L1_PDN, spm_read(SPM_CA7_CPU1_L1_PDN) | L1_PDN);
        while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA7_CPU1_PWR_CON, (spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU1) != CA7_CPU1) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU1) != CA7_CPU1));
        
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~PWR_ISO);

        spm_write(SPM_CA7_CPU1_L1_PDN, spm_read(SPM_CA7_CPU1_L1_PDN) & ~L1_PDN);
        while ((spm_read(SPM_CA7_CPU1_L1_PDN) & L1_PDN_ACK) != 0);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) & ~SRAM_CKISO);

        spm_write(SPM_CA7_CPU1_PWR_CON, spm_read(SPM_CA7_CPU1_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu2(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU2_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU2_L1_PDN, spm_read(SPM_CA7_CPU2_L1_PDN) | L1_PDN);
        while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA7_CPU2_PWR_CON, (spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU2) != CA7_CPU2) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU2) != CA7_CPU2));
        
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~PWR_ISO);

        spm_write(SPM_CA7_CPU2_L1_PDN, spm_read(SPM_CA7_CPU2_L1_PDN) & ~L1_PDN);
        while ((spm_read(SPM_CA7_CPU2_L1_PDN) & L1_PDN_ACK) != 0);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) & ~SRAM_CKISO);

        spm_write(SPM_CA7_CPU2_PWR_CON, spm_read(SPM_CA7_CPU2_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu3(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPU3_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU3_L1_PDN, spm_read(SPM_CA7_CPU3_L1_PDN) | L1_PDN);
        while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) != L1_PDN_ACK);
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA7_CPU3_PWR_CON, (spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPU3) != CA7_CPU3) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPU3) != CA7_CPU3));
        
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~PWR_ISO);

        spm_write(SPM_CA7_CPU3_L1_PDN, spm_read(SPM_CA7_CPU3_L1_PDN) & ~L1_PDN);
        while ((spm_read(SPM_CA7_CPU3_L1_PDN) & L1_PDN_ACK) != 0);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) & ~SRAM_CKISO);

        spm_write(SPM_CA7_CPU3_PWR_CON, spm_read(SPM_CA7_CPU3_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu4(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU0_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(INFRACFG_AO_BASE + 0x8, 0x160);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU0_CA15_L1_PDN);
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU0_CA15_L1_PDN_ACK) != CPU0_CA15_L1_PDN_ACK);
        udelay(20);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU0_CA15_L1_PDN_ISO);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ON_2ND);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x0);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU0) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU0) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
        
        if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU1 | CA15_CPU2 | CA15_CPU3)) && 
            !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU1 | CA15_CPU2 | CA15_CPU3)))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
    } 
    else /* STA_POWER_ON */
    {
        if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
            !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_ON_2ND);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x160);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU0) != CA15_CPU0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU0) != CA15_CPU0));
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_CLK_DIS);
        
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU0_CA15_L1_PDN_ISO);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU0_CA15_L1_PDN);
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU0_CA15_L1_PDN_ACK) != 0);
        udelay(20);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_RST_B);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x0);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu5(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN)
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU1_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(INFRACFG_AO_BASE + 0x8, 0x160);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU1_CA15_L1_PDN);
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU1_CA15_L1_PDN_ACK) != CPU1_CA15_L1_PDN_ACK);
        udelay(20);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU1_CA15_L1_PDN_ISO);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ON_2ND);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x0);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU1) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU1) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
        
        if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CPU2 | CA15_CPU3)) && 
            !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU0 | CA15_CPU2 | CA15_CPU3)))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
    } 
    else /* STA_POWER_ON */
    {
        if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
            !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_ON_2ND);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x160);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU1) != CA15_CPU1) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU1) != CA15_CPU1));
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_CLK_DIS);
        
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU1_CA15_L1_PDN_ISO);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU1_CA15_L1_PDN);
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU1_CA15_L1_PDN_ACK) != 0);
        udelay(20);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_RST_B);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x0);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu6(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU2_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(INFRACFG_AO_BASE + 0x8, 0x160);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU2_CA15_L1_PDN);
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU2_CA15_L1_PDN_ACK) != CPU2_CA15_L1_PDN_ACK);
        udelay(20);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU2_CA15_L1_PDN_ISO);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ON_2ND);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x0);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU2) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU2) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
        
        if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU3)) && 
            !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU3)))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
    } 
    else /* STA_POWER_ON */
    {
        if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
            !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_ON_2ND);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x160);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU2) != CA15_CPU2) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU2) != CA15_CPU2));
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_CLK_DIS);
        
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU2_CA15_L1_PDN_ISO);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU2_CA15_L1_PDN);
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU2_CA15_L1_PDN_ACK) != 0);
        udelay(20);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_RST_B);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x0);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpu7(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPU3_STANDBYWFI) == 0);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(INFRACFG_AO_BASE + 0x8, 0x160);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU3_CA15_L1_PDN);
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU3_CA15_L1_PDN_ACK) != CPU3_CA15_L1_PDN_ACK);
        udelay(20);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) | CPU3_CA15_L1_PDN_ISO);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_CLK_DIS);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ON_2ND);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x0);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU3) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU3) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
        
        if (!(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU2)) && 
            !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU2)))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
    } 
    else /* STA_POWER_ON */
    {
        if (!(spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) &&
            !(spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP))
            spm_mtcmos_ctrl_cpusys1(state, chkWfiBeforePdn);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_ON_2ND);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x160);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPU3) != CA15_CPU3) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPU3) != CA15_CPU3));
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_CLK_DIS);
        
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU3_CA15_L1_PDN_ISO);
        spm_write(SPM_CA15_L1_PWR_CON, spm_read(SPM_CA15_L1_PWR_CON) & ~CPU3_CA15_L1_PDN);
        while ((spm_read(SPM_CA15_L1_PWR_CON) & CPU3_CA15_L1_PDN_ACK) != 0);
        udelay(20);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_RST_B);
        spm_write(INFRACFG_AO_BASE + 0x8, 0x0);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_dbg0(int state)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA7_DBG_PWR_CON, (spm_read(SPM_CA7_DBG_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_DBG) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_DBG) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_DBG) != CA7_DBG) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_DBG) != CA7_DBG));
        
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~SRAM_CKISO);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_CA7_DBG_PWR_CON, spm_read(SPM_CA7_DBG_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}

#if 0 //There is no dbgsys wrapper in ca15 cpusys
int spm_mtcmos_ctrl_dbg1(int state)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~SRAM_ISOINT_B);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_ISO);
        spm_write(SPM_MP1_DBG_PWR_CON, (spm_read(SPM_MP1_DBG_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_ON);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & MP1_DBG) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & MP1_DBG) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & MP1_DBG) != MP1_DBG) || ((spm_read(SPM_PWR_STATUS_2ND) & MP1_DBG) != MP1_DBG));
        
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~SRAM_CKISO);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MP1_DBG_PWR_CON, spm_read(SPM_MP1_DBG_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
    }
    
    return 0;
}
#endif //There is no dbgsys wrapper in ca15 cpusys

int spm_mtcmos_ctrl_cpusys0(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        //TODO: add per cpu power status check?
        
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA7_CPUTOP_STANDBYWFI) == 0);
        
        //if (((spm_read(SPM_PWR_STATUS) & MP1_DBG) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & MP1_DBG) != 0))
        spm_mtcmos_ctrl_dbg0(state);
        
        spm_topaxi_prot(CA7_PDN_REQ, 1);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
    #if 1
        spm_write(SPM_CA7_CPUTOP_L2_PDN, spm_read(SPM_CA7_CPUTOP_L2_PDN) | L2_SRAM_PDN);
        while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) != L2_SRAM_PDN_ACK);
    #else
        spm_write(SPM_CA7_CPUTOP_L2_SLEEP, spm_read(SPM_CA7_CPUTOP_L2_SLEEP) & ~L2_SRAM_SLEEP_B);
        while ((spm_read(SPM_CA7_CPUTOP_L2_SLEEP) & L2_SRAM_SLEEP_B_ACK) != 0);
    #endif
        
        /***********************************************************************
         * 20140123 
         * Due to there is no bus protect for CA7 L2 share SRAM ADB,
         * adjust the CA7 CPUTOP power off sequence
         **********************************************************************/
        //XXX: original sequence
        //spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ISO);
        //spm_write(SPM_CA7_CPUTOP_PWR_CON, (spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_CLK_DIS);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_RST_B);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ISO);
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
    } 
    else /* STA_POWER_ON */
    {
        //enable L2 cache hardware reset
        spm_write(CA7_CACHE_CONFIG, spm_read(CA7_CACHE_CONFIG) & ~CA7_RG_L2RSTDISABLE);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA7_CPUTOP) != CA7_CPUTOP) || ((spm_read(SPM_PWR_STATUS_2ND) & CA7_CPUTOP) != CA7_CPUTOP));
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
        
    #if 1
        spm_write(SPM_CA7_CPUTOP_L2_PDN, spm_read(SPM_CA7_CPUTOP_L2_PDN) & ~L2_SRAM_PDN);
        while ((spm_read(SPM_CA7_CPUTOP_L2_PDN) & L2_SRAM_PDN_ACK) != 0);
    #else
        spm_write(SPM_CA7_CPUTOP_L2_SLEEP, spm_read(SPM_CA7_CPUTOP_L2_SLEEP) | L2_SRAM_SLEEP_B);
        while ((spm_read(SPM_CA7_CPUTOP_L2_SLEEP) & L2_SRAM_SLEEP_B_ACK) != L2_SRAM_SLEEP_B_ACK);
    #endif
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_CA7_CPUTOP_PWR_CON, spm_read(SPM_CA7_CPUTOP_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
        
        spm_topaxi_prot(CA7_PDN_REQ, 0);
        
        spm_mtcmos_ctrl_dbg0(state);
    }
    
    return 0;
}

int spm_mtcmos_ctrl_cpusys1(int state, int chkWfiBeforePdn)
{
    unsigned long flags;
    
    /* enable register control */
    spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    
    if (state == STA_POWER_DOWN) 
    {
        turn_off_SPARK();
        if (chkWfiBeforePdn)
            while ((spm_read(SPM_SLEEP_TIMER_STA) & CA15_CPUTOP_STANDBYWFI) == 0);
        
        spm_topaxi_prot(CA15_PDN_REQ, 1);
        
        spm_mtcmos_cpu_lock(&flags);

        turn_off_FBB();

        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_CKISO);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_ISOINT_B);
    #if 1
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_PDN);
        while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) != CA15_L2_PDN_ACK);
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_PDN_ISO);
    #else
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~L2_SRAM_SLEEP_B);
        while ((spm_read(SPM_CA15_L2_PWR_CON) & L2_SRAM_SLEEP_B_ACK) != 0);
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | CA15_L2_SLEEPB_ISO);
    #endif
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ISO);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, (spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_CLK_DIS) & ~PWR_RST_B);
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) != 0) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP) != 0));
        
        spm_mtcmos_cpu_unlock(&flags);
        
        if ((spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) & VCA15_PWR_ISO) == 0)
        {
			//enable dcm
			if(CHIP_SW_VER_02 == mt_get_chip_sw_ver()){
                enable_cpu_dcm();
            }

            spm_write(SPM_SLEEP_DUAL_VCORE_PWR_CON, spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) | VCA15_PWR_ISO);
            bigcore_power_off();
            disable_pll(ARMCA15PLL, "ca15pll");
        }
    } 
    else /* STA_POWER_ON */
    {
        if ((spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) & VCA15_PWR_ISO) == VCA15_PWR_ISO)
        {
            enable_pll(ARMCA15PLL, "ca15pll");
            bigcore_power_on();
            spm_write(SPM_SLEEP_DUAL_VCORE_PWR_CON, spm_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) & ~VCA15_PWR_ISO);

            //disable dcm
			if(CHIP_SW_VER_02 == mt_get_chip_sw_ver()){
            	disable_cpu_dcm();
            }
        }
        
        //enable L2 cache hardware reset
        spm_write(CA15L_RST_CTL, spm_read(CA15L_RST_CTL) & ~CA15L_L2RSTDISABLE);
        
        spm_mtcmos_cpu_lock(&flags);
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON);
        udelay(1);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_ON_2ND);
        while (((spm_read(SPM_PWR_STATUS) & CA15_CPUTOP) != CA15_CPUTOP) || ((spm_read(SPM_PWR_STATUS_2ND) & CA15_CPUTOP) != CA15_CPUTOP));
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_CLK_DIS);
        
    #if 1
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_PDN_ISO);
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_PDN);
        while ((spm_read(SPM_CA15_L2_PWR_CON) & CA15_L2_PDN_ACK) != 0);
    #else
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) & ~CA15_L2_SLEEPB_ISO);
        spm_write(SPM_CA15_L2_PWR_CON, spm_read(SPM_CA15_L2_PWR_CON) | L2_SRAM_SLEEP_B);
        while ((spm_read(SPM_CA15_L2_PWR_CON) & L2_SRAM_SLEEP_B_ACK) != L2_SRAM_SLEEP_B_ACK);
    #endif
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | SRAM_ISOINT_B);
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~SRAM_CKISO);
        
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) & ~PWR_ISO);
        ptp2_pre_init();
        spm_write(SPM_CA15_CPUTOP_PWR_CON, spm_read(SPM_CA15_CPUTOP_PWR_CON) | PWR_RST_B);
        
        spm_mtcmos_cpu_unlock(&flags);
        
        spm_topaxi_prot(CA15_PDN_REQ, 0);
    }
    
    return 0;
}

void spm_mtcmos_ctrl_cpusys1_init_1st_bring_up(int state)
{

    if (state == STA_POWER_DOWN) 
    {
        spm_mtcmos_ctrl_cpu7(STA_POWER_DOWN, 0);
        spm_mtcmos_ctrl_cpu6(STA_POWER_DOWN, 0);
        spm_mtcmos_ctrl_cpu5(STA_POWER_DOWN, 0);
        spm_mtcmos_ctrl_cpu4(STA_POWER_DOWN, 0);
    } 
    else /* STA_POWER_ON */
    {
        spm_mtcmos_ctrl_cpu4(STA_POWER_ON, 1);
        spm_mtcmos_ctrl_cpu5(STA_POWER_ON, 1);
        spm_mtcmos_ctrl_cpu6(STA_POWER_ON, 1);
        spm_mtcmos_ctrl_cpu7(STA_POWER_ON, 1);
        //spm_mtcmos_ctrl_dbg1(STA_POWER_ON);
    }

    //unsigned long flags;
    //
    ///* enable register control */
    //spm_write(SPM_POWERON_CONFIG_SET, (SPM_PROJECT_CODE << 16) | (1U << 0));
    //
    //spm_mtcmos_cpu_lock(&flags);
    //
    //spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) & ~PWR_CLK_DIS);
    //spm_write(SPM_CA15_CPU0_PWR_CON, spm_read(SPM_CA15_CPU0_PWR_CON) | PWR_RST_B);
    //spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) & ~PWR_CLK_DIS);
    //spm_write(SPM_CA15_CPU1_PWR_CON, spm_read(SPM_CA15_CPU1_PWR_CON) | PWR_RST_B);
    //spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) & ~PWR_CLK_DIS);
    //spm_write(SPM_CA15_CPU2_PWR_CON, spm_read(SPM_CA15_CPU2_PWR_CON) | PWR_RST_B);
    //spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) & ~PWR_CLK_DIS);
    //spm_write(SPM_CA15_CPU3_PWR_CON, spm_read(SPM_CA15_CPU3_PWR_CON) | PWR_RST_B);
    //
    //spm_mtcmos_cpu_unlock(&flags);
    //
    //spm_mtcmos_ctrl_dbg1(STA_POWER_ON);
}

bool spm_cpusys0_can_power_down(void)
{
    return !(spm_read(SPM_PWR_STATUS) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU2 | CA15_CPU3 | CA15_CPUTOP | CA7_CPU1 | CA7_CPU2 | CA7_CPU3)) &&
           !(spm_read(SPM_PWR_STATUS_2ND) & (CA15_CPU0 | CA15_CPU1 | CA15_CPU2 | CA15_CPU3 | CA15_CPUTOP | CA7_CPU1 | CA7_CPU2 | CA7_CPU3));
}

bool spm_cpusys1_can_power_down(void)
{
    return !(spm_read(SPM_PWR_STATUS) & (CA7_DBG | CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3 | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 | CA15_CPU3)) &&
           !(spm_read(SPM_PWR_STATUS_2ND) & (CA7_DBG | CA7_CPU0 | CA7_CPU1 | CA7_CPU2 | CA7_CPU3 | CA7_CPUTOP | CA15_CPU1 | CA15_CPU2 | CA15_CPU3));
}


/**************************************
 * for non-CPU MTCMOS
 **************************************/
static DEFINE_SPINLOCK(spm_noncpu_lock);

#if 0
void spm_mtcmos_noncpu_lock(unsigned long *flags)
{
    spin_lock_irqsave(&spm_noncpu_lock, *flags);
}

void spm_mtcmos_noncpu_unlock(unsigned long *flags)
{
    spin_unlock_irqrestore(&spm_noncpu_lock, *flags);
}
#else
#define spm_mtcmos_noncpu_lock(flags)   \
do {    \
    spin_lock_irqsave(&spm_noncpu_lock, flags);  \
} while (0)

#define spm_mtcmos_noncpu_unlock(flags) \
do {    \
    spin_unlock_irqrestore(&spm_noncpu_lock, flags);    \
} while (0)

#endif

#define AUD_PWR_STA_MASK    (0x1 << 24)
#define MFG_ASYNC_PWR_STA_MASK (0x1 << 23)
#define MFG_2D_PWR_STA_MASK (0x1 << 22)
#define VEN_PWR_STA_MASK    (0x1 << 21)
#define MJC_PWR_STA_MASK    (0x1 << 20) 
#define VDE_PWR_STA_MASK    (0x1 << 7)
//#define IFR_PWR_STA_MASK    (0x1 << 6)
#define ISP_PWR_STA_MASK    (0x1 << 5)
#define MFG_PWR_STA_MASK    (0x1 << 4)
#define DIS_PWR_STA_MASK    (0x1 << 3)
//#define DPY_PWR_STA_MASK    (0x1 << 2)
#define MD1_PWR_STA_MASK    (0x1 << 0)

#if 0
#define PWR_RST_B           (0x1 << 0)
#define PWR_ISO             (0x1 << 1)
#define PWR_ON              (0x1 << 2)
#define PWR_ON_2ND          (0x1 << 3)
#define PWR_CLK_DIS         (0x1 << 4)
#endif

#define SRAM_PDN            (0xf << 8)
#define MFG_SRAM_PDN        (0x3f << 8) 
#define MD_SRAM_PDN         (0x1 << 8)

/*
#define VDE_SRAM_ACK        (0x1 << 12)
#define IFR_SRAM_ACK        (0xf << 12)
#define ISP_SRAM_ACK        (0x3 << 12)
#define DIS_SRAM_ACK        (0xf << 12)
#define MFG_SRAM_ACK        (0x1 << 12)
*/
#define VDE_SRAM_ACK        (0x1 << 12)
#define VEN_SRAM_ACK        (0xf << 12)
#define ISP_SRAM_ACK        (0x3 << 12)
#define DIS_SRAM_ACK        (0x1 << 12)
#define MFG_SRAM_ACK        (0x3f << 16)
#define MFG_2D_SRAM_ACK     (0x3 << 12)
#define MJC_SRAM_ACK        (0x1 << 12)
#define AUD_SRAM_ACK        (0xf << 12)

#define MD1_PROT_MASK     0x04B8//bit 3,4,5,7,10
//#define DISP_PROT_MASK    0x0046//bit 1,2,6
#define DISP_PROT_MASK    0x0006//bit 1,2
#define MFG_2D_PROT_MASK  0x00E04000//bit 14,21,22,23


int spm_mtcmos_ctrl_vdec(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_VDE_PWR_CON) & VDE_SRAM_ACK) != VDE_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }    
        }

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_VDE_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_VDE_PWR_CON, val);

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & VDE_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ON);
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & VDE_PWR_STA_MASK)
                || !(spm_read(SPM_PWR_STATUS_2ND) & VDE_PWR_STA_MASK)) {
        }

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) | PWR_RST_B);

        spm_write(SPM_VDE_PWR_CON, spm_read(SPM_VDE_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_VDE_PWR_CON) & VDE_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
            	clk_stat_check(SYS_DIS);
                BUG();	
            }    
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_venc(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;    

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_VEN_PWR_CON) & VEN_SRAM_ACK) != VEN_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }    
        }

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_VEN_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_VEN_PWR_CON, val);

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & VEN_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ON);
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & VEN_PWR_STA_MASK)
                || !(spm_read(SPM_PWR_STATUS_2ND) & VEN_PWR_STA_MASK)) {
        }

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) | PWR_RST_B);

        spm_write(SPM_VEN_PWR_CON, spm_read(SPM_VEN_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_VEN_PWR_CON) & VEN_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_isp(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_ISP_PWR_CON) & ISP_SRAM_ACK) != ISP_SRAM_ACK) {
        }

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_ISP_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_ISP_PWR_CON, val);

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & ISP_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ON);
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & ISP_PWR_STA_MASK)
                || !(spm_read(SPM_PWR_STATUS_2ND) & ISP_PWR_STA_MASK)) {
        }

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) | PWR_RST_B);

        spm_write(SPM_ISP_PWR_CON, spm_read(SPM_ISP_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_ISP_PWR_CON) & ISP_SRAM_ACK)) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}
#if 0
int spm_mtcmos_ctrl_disp(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | DISP_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) != DISP_PROT_MASK) {
        }
        
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | SRAM_PDN);
#if 0
        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK) != DIS_SRAM_ACK) {
        }
#endif
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_DIS_PWR_CON);
        //val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        val = val | PWR_CLK_DIS;
        spm_write(SPM_DIS_PWR_CON, val);

        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

#if 0
        udelay(1); 
        if (spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) { 
            err = 1;
        }
#else
        //while ((spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)
        //        || (spm_read(SPM_PWR_STATUS_S) & DIS_PWR_STA_MASK)) {
        //}
#endif
    } else {    /* STA_POWER_ON */
        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON);
        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON_2ND);
#if 0
        udelay(1);
#else
        //while (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) 
        //        || !(spm_read(SPM_PWR_STATUS_S) & DIS_PWR_STA_MASK)) {
        //}
#endif
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_ISO);
        //spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_RST_B);

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~SRAM_PDN);

#if 0
        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK)) {
        }
#endif

#if 0
        udelay(1); 
        if (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)) { 
            err = 1;
        }
#endif
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~DISP_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

#else

int spm_mtcmos_ctrl_disp(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | DISP_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) != DISP_PROT_MASK) {
        }
        
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK) != DIS_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }       	
        }

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_DIS_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_DIS_PWR_CON, val);

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & DIS_PWR_STA_MASK) 
                || !(spm_read(SPM_PWR_STATUS_2ND) & DIS_PWR_STA_MASK)) {
        }

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) | PWR_RST_B);

        spm_write(SPM_DIS_PWR_CON, spm_read(SPM_DIS_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_DIS_PWR_CON) & DIS_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmm_clk, CLK_CFG_0 = 0x%x\n", spm_read(CLK_CFG_0));
            }
            if (count > 2000) {
                clk_stat_check(SYS_DIS);
                BUG();
            }    
        }

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~DISP_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & DISP_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}
#endif

int spm_mtcmos_ctrl_mfg(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MFG_2D_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) != MFG_2D_PROT_MASK) {
        }

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | MFG_SRAM_PDN);

        while ((spm_read(SPM_MFG_PWR_CON) & MFG_SRAM_ACK) != MFG_SRAM_ACK) {
        }

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MFG_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MFG_PWR_CON, val);

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MFG_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ON);
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MFG_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MFG_PWR_STA_MASK)) {
        }

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MFG_PWR_CON, spm_read(SPM_MFG_PWR_CON) & ~MFG_SRAM_PDN);

        while ((spm_read(SPM_MFG_PWR_CON) & MFG_SRAM_ACK)) {
        }

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MFG_2D_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & MFG_2D_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mfg_2D(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_MFG_2D_PWR_CON) & MFG_2D_SRAM_ACK) != MFG_2D_SRAM_ACK) {
        }

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MFG_2D_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MFG_2D_PWR_CON, val);

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MFG_2D_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MFG_2D_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_ON);
        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MFG_2D_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MFG_2D_PWR_STA_MASK)) {
        }

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MFG_2D_PWR_CON, spm_read(SPM_MFG_2D_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_MFG_2D_PWR_CON) & MFG_2D_SRAM_ACK)) {
        }

    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mfg_ASYNC(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | SRAM_PDN);

//        while ((spm_read(MFG_ASYNC_PWR_CON) & MFG_SRAM_ACK) != MFG_SRAM_ACK) {
//        }

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MFG_ASYNC_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MFG_ASYNC_PWR_CON, val);

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ON);
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MFG_ASYNC_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MFG_ASYNC_PWR_STA_MASK)) {
        }

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MFG_ASYNC_PWR_CON, spm_read(SPM_MFG_ASYNC_PWR_CON) & ~SRAM_PDN);

//        while ((spm_read(MFG_ASYNC_PWR_CON) & MFG_SRAM_ACK)) {
//        }

    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mdsys1(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | MD1_PROT_MASK);
        while ((spm_read(TOPAXI_PROT_STA1) & MD1_PROT_MASK) != MD1_PROT_MASK) {
            count++;
            if(count>1000)
                break;
        }

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | MD_SRAM_PDN);

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MD_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MD_PWR_CON, val);

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MD1_PWR_STA_MASK)
                || (spm_read(SPM_PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
        }
//#ifdef MTK_LTE_SUPPORT
//        spm_write(AP_PLL_CON7, (spm_read(AP_PLL_CON7) | 0xF)); //force off LTE
//#endif
    } else {    /* STA_POWER_ON */
//#ifdef MTK_LTE_SUPPORT
//        spm_write(AP_PLL_CON7, (spm_read(AP_PLL_CON7) & (~0xF))); //turn on LTE
//#endif
        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_ON);
        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MD1_PWR_STA_MASK) 
                || !(spm_read(SPM_PWR_STATUS_2ND) & MD1_PWR_STA_MASK)) {
        }

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MD_PWR_CON, spm_read(SPM_MD_PWR_CON) & ~MD_SRAM_PDN);

        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~MD1_PROT_MASK);
        while (spm_read(TOPAXI_PROT_STA1) & MD1_PROT_MASK) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_mjc(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;
    int count = 0;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_MJC_PWR_CON) & MJC_SRAM_ACK) != MJC_SRAM_ACK) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmjc_clk, CLK_CFG_5 = 0x%x\n", spm_read(CLK_CFG_5));
            }
            if (count > 2000)
                BUG();
        }

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_MJC_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_MJC_PWR_CON, val);

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & MJC_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & MJC_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
    	spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_ON);
        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & MJC_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & MJC_PWR_STA_MASK)) {
        }

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) | PWR_RST_B);

        spm_write(SPM_MJC_PWR_CON, spm_read(SPM_MJC_PWR_CON) & ~SRAM_PDN);

        while ((spm_read(SPM_MJC_PWR_CON) & MJC_SRAM_ACK)) {
            count++;
            if (count > 1000 && count<1010) {
                printk("there is no fmjc_clk, CLK_CFG_5 = 0x%x\n", spm_read(CLK_CFG_5));
            }
            if (count > 2000)
                BUG();	
        }
    }
 
    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_mtcmos_ctrl_aud(int state)
{
    int err = 0;
    volatile unsigned int val;
    unsigned long flags;

    spm_mtcmos_noncpu_lock(flags);

    if (state == STA_POWER_DOWN) {

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | SRAM_PDN);

        while ((spm_read(SPM_AUDIO_PWR_CON) & AUD_SRAM_ACK) != AUD_SRAM_ACK) {
        }

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ISO);

        val = spm_read(SPM_AUDIO_PWR_CON);
        val = (val & ~PWR_RST_B) | PWR_CLK_DIS;
        spm_write(SPM_AUDIO_PWR_CON, val);

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~(PWR_ON | PWR_ON_2ND));

        while ((spm_read(SPM_PWR_STATUS) & AUD_PWR_STA_MASK) 
                || (spm_read(SPM_PWR_STATUS_2ND) & AUD_PWR_STA_MASK)) {
        }
    } else {    /* STA_POWER_ON */
    	spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ON);
        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_ON_2ND);

        while (!(spm_read(SPM_PWR_STATUS) & AUD_PWR_STA_MASK) || 
                !(spm_read(SPM_PWR_STATUS_2ND) & AUD_PWR_STA_MASK)) {
        }

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~PWR_CLK_DIS);
        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~PWR_ISO);
        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) | PWR_RST_B);

        spm_write(SPM_AUDIO_PWR_CON, spm_read(SPM_AUDIO_PWR_CON) & ~SRAM_PDN);
        
        while ((spm_read(SPM_AUDIO_PWR_CON) & AUD_SRAM_ACK)) {
        }
    }

    spm_mtcmos_noncpu_unlock(flags);

    return err;
}

int spm_topaxi_prot(int bit, int en)
{
    unsigned long flags;	
    spm_mtcmos_noncpu_lock(flags);

    if (en == 1) {
        spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) | (1<<bit));
        while ((spm_read(TOPAXI_PROT_STA1) & (1<<bit)) != (1<<bit)) {
        }
    } else {
   	    spm_write(TOPAXI_PROT_EN, spm_read(TOPAXI_PROT_EN) & ~(1<<bit));
        while (spm_read(TOPAXI_PROT_STA1) & (1<<bit)) {
        }
    }    

    spm_mtcmos_noncpu_unlock(flags);    

    return 0;
}
