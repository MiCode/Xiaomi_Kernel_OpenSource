#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/cpu.h>

#include <linux/types.h>
#include <linux/string.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/mtk_gpu_utility.h>
#include <linux/spinlock.h>
#include <linux/cpumask.h>

#include <mach/mt_cirq.h>
#include <asm/system_misc.h>
#include <mach/mt_typedefs.h>
#include <mach/sync_write.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_dcm.h>
#include <mach/mt_gpt.h>
#include <mach/mt_cpuxgpt.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_spm_sleep.h>
#include <mach/hotplug.h>
#include <mach/mt_cpufreq.h>
#include <mach/mt_power_gs.h>
#include <mach/mt_ptp.h>
#include <mach/mt_timer.h>
#include <mach/irqs.h>
#include <mach/mt_thermal.h>
#include <mach/mt_idle.h>
#include <mach/mt_boot.h>

//#define CCI400_CLOCK_SWITCH

#define USING_XLOG

#ifdef USING_XLOG
#include <linux/xlog.h>

#define IDLE_TAG     "Power/swap"

#define idle_err(fmt, args...)       \
    xlog_printk(ANDROID_LOG_ERROR, IDLE_TAG, fmt, ##args)
#define idle_warn(fmt, args...)      \
    xlog_printk(ANDROID_LOG_WARN, IDLE_TAG, fmt, ##args)
#define idle_info(fmt, args...)      \
    xlog_printk(ANDROID_LOG_INFO, IDLE_TAG, fmt, ##args)
#define idle_dbg(fmt, args...)       \
    xlog_printk(ANDROID_LOG_DEBUG, IDLE_TAG, fmt, ##args)
#define idle_ver(fmt, args...)       \
    xlog_printk(ANDROID_LOG_VERBOSE, IDLE_TAG, fmt, ##args)

#else /* !USING_XLOG */

#define IDLE_TAG     "[Power/swap] "

#define idle_err(fmt, args...)       \
    printk(KERN_ERR IDLE_TAG);           \
    printk(KERN_CONT fmt, ##args)
#define idle_warn(fmt, args...)      \
    printk(KERN_WARNING IDLE_TAG);       \
    printk(KERN_CONT fmt, ##args)
#define idle_info(fmt, args...)      \
    printk(KERN_NOTICE IDLE_TAG);        \
    printk(KERN_CONT fmt, ##args)
#define idle_dbg(fmt, args...)       \
    printk(KERN_INFO IDLE_TAG);          \
    printk(KERN_CONT fmt, ##args)
#define idle_ver(fmt, args...)       \
    printk(KERN_DEBUG IDLE_TAG);         \
    printk(KERN_CONT fmt, ##args)

#endif

#define idle_gpt GPT4

#define idle_readl(addr) \
    DRV_Reg32(addr)

#define idle_writel(addr, val)   \
    mt65xx_reg_sync_writel(val, addr)

#define idle_setl(addr, val) \
    mt65xx_reg_sync_writel(idle_readl(addr) | (val), addr)

#define idle_clrl(addr, val) \
    mt65xx_reg_sync_writel(idle_readl(addr) & ~(val), addr)


extern unsigned long localtimer_get_counter(void);
extern int localtimer_set_next_event(unsigned long evt);
extern void hp_enable_timer(int enable);

static unsigned long rgidle_cnt[NR_CPUS] = {0};
static bool mt_idle_chk_golden = 0;
static bool mt_dpidle_chk_golden = 0;


#define INVALID_GRP_ID(grp) (grp < 0 || grp >= NR_GRPS)
bool __attribute__((weak))
clkmgr_idle_can_enter(unsigned int *condition_mask, unsigned int *block_mask)
{
    return false;
}

//FIXME: for ROME-Plus build error
void __attribute__((weak))
tscpu_cancel_thermal_timer(void)
{
    return;
}
void __attribute__((weak))
tscpu_start_thermal_timer(void)
{
    return;
}
void __attribute__((weak))
bus_dcm_enable(void)
{
	//FIXME: ROME plus
}
void __attribute__((weak))
bus_dcm_disable(void)
{
	//FIXME: ROME plus
}

enum {
    IDLE_TYPE_DP = 0,
    IDLE_TYPE_SO = 1,
    IDLE_TYPE_MC = 2,
    IDLE_TYPE_SL = 3,
    IDLE_TYPE_RG = 4,
    NR_TYPES = 5,
};

enum {
    BY_CPU = 0,
    BY_CLK = 1,
    BY_TMR = 2,
    BY_OTH = 3,
    BY_VTG = 4,
    NR_REASONS = 5
};

/*Idle handler on/off*/
static int idle_switch[NR_TYPES] = {
    1,  //dpidle switch
    1,  //soidle switch
    0,	//mcidle switch
    0,  //slidle switch
    1,  //rgidle switch
};

static unsigned int dpidle_condition_mask[NR_GRPS] = {
    0x2ff61ffd, //PERI0:
    0x0000a080, //INFRA:
    0xffffffff, //DISP0:
    0x000003ff, //DISP1:
    0x00000be1, //IMAGE:
    0x0000000f, //MFG:
    0x00000000, //AUDIO:
    0x00000001, //VDEC0:
    0x00000001, //VDEC1:
    0x0000000f, //MJC:
    0x00001111, //VENC_JPEG:
};

static unsigned int soidle_condition_mask[NR_GRPS] = {
    0x2ff60ffd, //PERI0:
    0x0000a080, //INFRA:
    0x757a7ffc, //DISP0:
    0x000003cc, //DISP1:
    0x00000be1, //IMAGE:
    0x0000000f, //MFG:
    0x00000000, //AUDIO:
    0x00000001, //VDEC0:
    0x00000001, //VDEC1:
    0x0000000f, //MJC:
    0x00001111, //VENC_JPEG
};

static unsigned int slidle_condition_mask[NR_GRPS] = {
    0x0f801000, //PERI0:
    0x00000000, //INFRA:
    0x00000000, //DISP0:
    0x00000000, //DISP1:
    0x00000000, //IMAGE:
    0x00000000, //MFG:
    0x00000000, //AUDIO:
    0x00000000, //VDEC0:
    0x00000000, //VDEC1:
    0x00000000, //MJC:
    0x00000000, //VENC_JPEG:
};



static const char *idle_name[NR_TYPES] = {
    "dpidle",
    "soidle",
    "mcidle",
    "slidle",
    "rgidle",
};

static const char *reason_name[NR_REASONS] = {
    "by_cpu",
    "by_clk",
    "by_tmr",
    "by_oth",
    "by_vtg",
};


/*Slow Idle*/
static unsigned int     slidle_block_mask[NR_GRPS] = {0x0};
static unsigned long    slidle_cnt[NR_CPUS] = {0};
static unsigned long    slidle_block_cnt[NR_REASONS] = {0};
/*SODI*/
static unsigned int     soidle_block_mask[NR_GRPS] = {0x0};
static unsigned int     soidle_timer_left;
static unsigned int     soidle_timer_left2;
#ifndef CONFIG_SMP
static unsigned int     soidle_timer_cmp;
#endif
static unsigned int     soidle_time_critera = 26000; //FIXME: need fine tune
static unsigned long    soidle_cnt[NR_CPUS] = {0};
static unsigned long    soidle_block_cnt[NR_CPUS][NR_REASONS] = {{0}};
/*DeepIdle*/
static unsigned int     dpidle_block_mask[NR_GRPS] = {0x0};
static unsigned int     dpidle_timer_left;
static unsigned int     dpidle_timer_left2;
#ifndef CONFIG_SMP
static unsigned int     dpidle_timer_cmp;
#endif
static unsigned int     dpidle_time_critera = 26000;
static unsigned int     dpidle_block_time_critera = 30000;//default 30sec
static unsigned long    dpidle_cnt[NR_CPUS] = {0};
static unsigned long    dpidle_block_cnt[NR_REASONS] = {0};
static unsigned long long dpidle_block_prev_time = 0;
static bool             dpidle_by_pass_cg = 0;
/*MCDI*/
static unsigned int     mcidle_timer_left[NR_CPUS];
static unsigned int     mcidle_timer_left2[NR_CPUS];
static unsigned int     mcidle_time_critera = 39000;//3ms
static unsigned long    mcidle_cnt[NR_CPUS] = {0};
static unsigned long    mcidle_block_cnt[NR_CPUS][NR_REASONS] = {{0}};
u64                     mcidle_timer_before_wfi[NR_CPUS];
static unsigned int 	idle_spm_lock = 0;

static long int idle_get_current_time_ms(void)
{
    struct timeval t;
    do_gettimeofday(&t);
    return ((t.tv_sec & 0xFFF) * 1000000 + t.tv_usec) / 1000;
}

static DEFINE_SPINLOCK(idle_spm_spin_lock);

void idle_lock_spm(enum idle_lock_spm_id id)
{
    unsigned long flags;
    spin_lock_irqsave(&idle_spm_spin_lock, flags);
    idle_spm_lock|=(1<<id);
    spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}

void idle_unlock_spm(enum idle_lock_spm_id id)
{
    unsigned long flags;
    spin_lock_irqsave(&idle_spm_spin_lock, flags);
    idle_spm_lock&=~(1<<id);
    spin_unlock_irqrestore(&idle_spm_spin_lock, flags);
}


/************************************************
 * SODI part
 ************************************************/
static DEFINE_MUTEX(soidle_locked);

static void enable_soidle_by_mask(int grp, unsigned int mask)
{
    mutex_lock(&soidle_locked);
    soidle_condition_mask[grp] &= ~mask;
    mutex_unlock(&soidle_locked);
}

static void disable_soidle_by_mask(int grp, unsigned int mask)
{
    mutex_lock(&soidle_locked);
    soidle_condition_mask[grp] |= mask;
    mutex_unlock(&soidle_locked);
}

void enable_soidle_by_bit(int id)
{
    int grp = id / 32;
    unsigned int mask = 1U << (id % 32);
    BUG_ON(INVALID_GRP_ID(grp));
    enable_soidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_soidle_by_bit);

void disable_soidle_by_bit(int id)
{
    int grp = id / 32;
    unsigned int mask = 1U << (id % 32);
    BUG_ON(INVALID_GRP_ID(grp));
    disable_soidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_soidle_by_bit);

extern int disp_od_is_enabled(void);
bool soidle_can_enter(int cpu)
{
    int reason = NR_REASONS;

#ifdef CONFIG_SMP
    if ((atomic_read(&is_in_hotplug) == 1)||(num_online_cpus() != 1)) {
    //if ((atomic_read(&is_in_hotplug) == 1)||(atomic_read(&hotplug_cpu_count) != 1)) {
        reason = BY_CPU;
        goto out;
    }
#endif
    if(idle_spm_lock){
        reason = BY_VTG;
        goto out;
    }


    if(spm_get_sodi_en()==0){
        reason = BY_OTH;
        goto out;
	}

    memset(soidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
    if (!clkmgr_idle_can_enter(soidle_condition_mask, soidle_block_mask)) {
        reason = BY_CLK;
        goto out;
    }

#ifdef CONFIG_SMP
    soidle_timer_left = localtimer_get_counter();
    if ((int)soidle_timer_left < soidle_time_critera ||
            ((int)soidle_timer_left) < 0) {
        reason = BY_TMR;
        goto out;
    }
#else
    gpt_get_cnt(GPT1, &soidle_timer_left);
    gpt_get_cmp(GPT1, &soidle_timer_cmp);
    if((soidle_timer_cmp-soidle_timer_left)<soidle_time_critera)
    {
        reason = BY_TMR;
        goto out;
    }
#endif


out:
    if (reason < NR_REASONS) {
        soidle_block_cnt[cpu][reason]++;
        return false;
    } else {
        return true;
    }
}

void soidle_before_wfi(int cpu)
{
#ifdef CONFIG_SMP
    soidle_timer_left2 = localtimer_get_counter();

    if( (int)soidle_timer_left2 <=0 )
    {
        gpt_set_cmp(idle_gpt, 1);//Trigger idle_gpt Timerout imediately
    }
    else
        gpt_set_cmp(idle_gpt, soidle_timer_left2);

    start_gpt(idle_gpt);
#else
    gpt_get_cnt(GPT1, &soidle_timer_left2);
#endif

}

void soidle_after_wfi(int cpu)
{
#ifdef CONFIG_SMP
    if (gpt_check_and_ack_irq(idle_gpt)) {
        localtimer_set_next_event(1);
    } else {
        /* waked up by other wakeup source */
        unsigned int cnt, cmp;
        gpt_get_cnt(idle_gpt, &cnt);
        gpt_get_cmp(idle_gpt, &cmp);
        if (unlikely(cmp < cnt)) {
            idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n", __func__,
                    idle_gpt + 1, cnt, cmp);
            BUG();
        }

        localtimer_set_next_event(cmp-cnt);
        stop_gpt(idle_gpt);
    }
#endif
    soidle_cnt[cpu]++;
}

/************************************************
 * multi-core idle part
 ************************************************/
static DEFINE_MUTEX(mcidle_locked);
bool mcidle_can_enter(int cpu)
{
    int reason = NR_REASONS;

    if(cpu==0){
        reason = BY_VTG;
        goto mcidle_out;
    }

    if(idle_spm_lock){
        reason = BY_VTG;
        goto mcidle_out;
    }

    if (num_online_cpus() == 1) {
        reason = BY_CPU;
        goto mcidle_out;
    }
    
//    if (atomic_read(&hotplug_cpu_count) == 1) {
//        reason = BY_CPU;
//        goto mcidle_out;
//    }

    if(spm_mcdi_can_enter()==0)
    {
        reason = BY_OTH;
        goto mcidle_out;

    }

    mcidle_timer_left[cpu] = localtimer_get_counter();
    if (mcidle_timer_left[cpu] < mcidle_time_critera ||
            ((int)mcidle_timer_left[cpu]) < 0) {
        reason = BY_TMR;
        goto mcidle_out;
    }


mcidle_out:
    if (reason < NR_REASONS) {
        mcidle_block_cnt[cpu][reason]++;
        return false;
    } else {
        return true;
    }
}
bool spm_mcdi_xgpt_timeout[NR_CPUS];

void mcidle_before_wfi(int cpu)
{
    u64 set_count=0;

    spm_mcdi_xgpt_timeout[cpu]=0;

    mcidle_timer_left2[cpu] = localtimer_get_counter();
    mcidle_timer_before_wfi[cpu] =localtimer_get_phy_count();

   	set_count = mcidle_timer_before_wfi[cpu]+(int)mcidle_timer_left2[cpu];

    cpu_xgpt_set_cmp(cpu,set_count);
}
int mcdi_xgpt_wakeup_cnt[NR_CPUS];
void mcidle_after_wfi(int cpu)
{
    u64 cmp;

    cpu_xgpt_irq_dis(cpu);//ack cpuxgpt, api need refine from Weiqi

    cmp = (localtimer_get_phy_count()-mcidle_timer_before_wfi[cpu]);

    if( cmp < (int)mcidle_timer_left2[cpu] )
        localtimer_set_next_event(mcidle_timer_left2[cpu]-cmp);
    else
        localtimer_set_next_event(1);
}
extern unsigned int g_SPM_MCDI_Abnormal_WakeUp;
unsigned int g_pre_SPM_MCDI_Abnormal_WakeUp = 0;
static void go_to_mcidle(int cpu)
{
    if(spm_mcdi_wfi(cpu)==1)
        mcidle_cnt[cpu]+=1;
    if(g_SPM_MCDI_Abnormal_WakeUp!=g_pre_SPM_MCDI_Abnormal_WakeUp)
    {
        printk("SPM-MCDI Abnormal %x\n",g_SPM_MCDI_Abnormal_WakeUp);
        g_pre_SPM_MCDI_Abnormal_WakeUp = g_SPM_MCDI_Abnormal_WakeUp;
    }
}

static void mcidle_idle_pre_handler(int cpu)
{
    if(cpu==0)
    {
        if((num_online_cpus() == 1)||(idle_switch[IDLE_TYPE_MC]==0))    	
        //if((atomic_read(&hotplug_cpu_count) == 1)||(idle_switch[IDLE_TYPE_MC]==0))
            spm_mcdi_switch_on_off(SPM_MCDI_IDLE,0);
        else
            spm_mcdi_switch_on_off(SPM_MCDI_IDLE,1);
    }
}


/************************************************
 * deep idle part
 ************************************************/
static DEFINE_MUTEX(dpidle_locked);

static void enable_dpidle_by_mask(int grp, unsigned int mask)
{
    mutex_lock(&dpidle_locked);
    dpidle_condition_mask[grp] &= ~mask;
    mutex_unlock(&dpidle_locked);
}

static void disable_dpidle_by_mask(int grp, unsigned int mask)
{
    mutex_lock(&dpidle_locked);
    dpidle_condition_mask[grp] |= mask;
    mutex_unlock(&dpidle_locked);
}

void enable_dpidle_by_bit(int id)
{
    int grp = id / 32;
    unsigned int mask = 1U << (id % 32);
    BUG_ON(INVALID_GRP_ID(grp));
    enable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_dpidle_by_bit);

void disable_dpidle_by_bit(int id)
{
    int grp = id / 32;
    unsigned int mask = 1U << (id % 32);
    BUG_ON(INVALID_GRP_ID(grp));
    disable_dpidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_dpidle_by_bit);


static bool dpidle_can_enter(void)
{
    int reason = NR_REASONS;
    int i = 0;
    unsigned long long dpidle_block_curr_time = 0;

    if (!mt_cpufreq_earlysuspend_status_get()){
        reason = BY_VTG;
        goto out;
    }


#ifdef CONFIG_SMP
    if ((atomic_read(&is_in_hotplug) >= 1)||(num_online_cpus() != 1)) {
    //if ((atomic_read(&is_in_hotplug) >= 1)||(atomic_read(&hotplug_cpu_count) != 1)) {
        reason = BY_CPU;
        goto out;
    }
#endif

    if(idle_spm_lock){
        reason = BY_VTG;
        goto out;
    }

    if(dpidle_by_pass_cg==0){
        memset(dpidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
        if (!clkmgr_idle_can_enter(dpidle_condition_mask, dpidle_block_mask)) {
            reason = BY_CLK;
            goto out;
        }
    }
#ifdef CONFIG_SMP
    dpidle_timer_left = localtimer_get_counter();
    if ((int)dpidle_timer_left < dpidle_time_critera ||
            ((int)dpidle_timer_left) < 0) {
        reason = BY_TMR;
        goto out;
    }
#else
    gpt_get_cnt(GPT1, &dpidle_timer_left);
    gpt_get_cmp(GPT1, &dpidle_timer_cmp);
    if((dpidle_timer_cmp-dpidle_timer_left)<dpidle_time_critera)
    {
        reason = BY_TMR;
        goto out;
    }
#endif

out:
    if (reason < NR_REASONS) {
        if( dpidle_block_prev_time == 0 )
            dpidle_block_prev_time = idle_get_current_time_ms();

        dpidle_block_curr_time = idle_get_current_time_ms();
        if((dpidle_block_curr_time - dpidle_block_prev_time) > dpidle_block_time_critera)
        {
            if ((smp_processor_id() == 0))
            {
                for (i = 0; i < nr_cpu_ids; i++) {
                    idle_ver("dpidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
                            i, dpidle_cnt[i], i, rgidle_cnt[i]);
                }

                for (i = 0; i < NR_REASONS; i++) {
                    idle_ver("[%d]dpidle_block_cnt[%s]=%lu\n", i, reason_name[i],
                            dpidle_block_cnt[i]);
                }

                for (i = 0; i < NR_GRPS; i++) {
                    idle_ver("[%02d]dpidle_condition_mask[%-8s]=0x%08x\t\t"
                            "dpidle_block_mask[%-8s]=0x%08x\n", i,
                            grp_get_name(i), dpidle_condition_mask[i],
                            grp_get_name(i), dpidle_block_mask[i]);
                }
                memset(dpidle_block_cnt, 0, sizeof(dpidle_block_cnt));
                dpidle_block_prev_time = idle_get_current_time_ms();

            }


        }
        dpidle_block_cnt[reason]++;
        return false;
    } else {
        dpidle_block_prev_time = idle_get_current_time_ms();
        return true;
    }

}

static unsigned int clk_cfg_4 = 0;

#define faudintbus_pll2sq() \
do {    \
    clk_cfg_4 = idle_readl(CLK_CFG_4);\
    idle_writel(CLK_CFG_4, clk_cfg_4 & 0xF8FFFFFF);  \
} while (0);

#define faudintbus_sq2pll() \
do {    \
    idle_writel(CLK_CFG_4, clk_cfg_4);  \
} while (0);

void spm_dpidle_before_wfi(void)
{

    if (TRUE == mt_dpidle_chk_golden)
    {
    //FIXME:
#if 0
        mt_power_gs_dump_dpidle();
#endif
    }
    bus_dcm_enable();
    //clkmux_sel(MT_MUX_AUDINTBUS, 0, "Deepidle"); //select 26M
    faudintbus_pll2sq();


#ifdef CONFIG_SMP
    dpidle_timer_left2 = localtimer_get_counter();

    if( (int)dpidle_timer_left2 <=0 )
        gpt_set_cmp(idle_gpt, 1);//Trigger GPT4 Timerout imediately
    else
        gpt_set_cmp(idle_gpt, dpidle_timer_left2);

    start_gpt(idle_gpt);
#else
    gpt_get_cnt(idle_gpt, &dpidle_timer_left2);
#endif

}

void spm_dpidle_after_wfi(void)
{

#ifdef CONFIG_SMP
        //if (gpt_check_irq(GPT4)) {
        if (gpt_check_and_ack_irq(idle_gpt)) {
            /* waked up by WAKEUP_GPT */
            localtimer_set_next_event(1);
        } else {
            /* waked up by other wakeup source */
            unsigned int cnt, cmp;
            gpt_get_cnt(idle_gpt, &cnt);
            gpt_get_cmp(idle_gpt, &cmp);
            if (unlikely(cmp < cnt)) {
                idle_err("[%s]GPT%d: counter = %10u, compare = %10u\n", __func__,
                        idle_gpt + 1, cnt, cmp);
                BUG();
            }

            localtimer_set_next_event(cmp-cnt);
            stop_gpt(idle_gpt);
            //GPT_ClearCount(WAKEUP_GPT);
        }
#endif

    //clkmux_sel(MT_MUX_AUDINTBUS, 1, "Deepidle"); //mainpll
    faudintbus_sq2pll();
    bus_dcm_disable();

    dpidle_cnt[0]++;
}


/************************************************
 * slow idle part
 ************************************************/
static DEFINE_MUTEX(slidle_locked);


static void enable_slidle_by_mask(int grp, unsigned int mask)
{
    mutex_lock(&slidle_locked);
    slidle_condition_mask[grp] &= ~mask;
    mutex_unlock(&slidle_locked);
}

static void disable_slidle_by_mask(int grp, unsigned int mask)
{
    mutex_lock(&slidle_locked);
    slidle_condition_mask[grp] |= mask;
    mutex_unlock(&slidle_locked);
}

void enable_slidle_by_bit(int id)
{
    int grp = id / 32;
    unsigned int mask = 1U << (id % 32);
    BUG_ON(INVALID_GRP_ID(grp));
    enable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(enable_slidle_by_bit);

void disable_slidle_by_bit(int id)
{
    int grp = id / 32;
    unsigned int mask = 1U << (id % 32);
    BUG_ON(INVALID_GRP_ID(grp));
    disable_slidle_by_mask(grp, mask);
}
EXPORT_SYMBOL(disable_slidle_by_bit);
//FIXME: for FPGA early porting
#if 1
#if EN_PTP_OD
extern u32 ptp_data[3];
#endif
#endif

#ifdef CCI400_CLOCK_SWITCH
extern int mt_irq_mask_all(struct mtk_irq_mask *mask);
extern int mt_irq_mask_restore(struct mtk_irq_mask *mask);
static unsigned int clk_cfg_6 = 0;

#define cci400_clk_26MHz() \
	do {	\
		clk_cfg_6 = idle_readl(CLK_CFG_6);\
		idle_writel(CLK_CFG_6, clk_cfg_6 & 0xFFF8FFFF);  \
	} while (0);

#define cci400_clk_restore() \
	do {	\
		idle_writel(CLK_CFG_6, clk_cfg_6);	\
	} while (0);
#endif

static bool slidle_can_enter(void)
{
    int reason = NR_REASONS;
    if ((atomic_read(&is_in_hotplug) == 1)||(num_online_cpus() != 1)) {
    //if (atomic_read(&hotplug_cpu_count) != 1) {
        reason = BY_CPU;
        goto out;
    }

    memset(slidle_block_mask, 0, NR_GRPS * sizeof(unsigned int));
    if (!clkmgr_idle_can_enter(slidle_condition_mask, slidle_block_mask)) {
        reason = BY_CLK;
        goto out;
    }
//FIXME: for FPGA early porting
#if 1
#if EN_PTP_OD
    if (ptp_data[0]) {
        reason = BY_OTH;
        goto out;
    }
#endif
#endif

out:
    if (reason < NR_REASONS) {
        slidle_block_cnt[reason]++;
        return false;
    } else {
        return true;
    }
}

static void slidle_before_wfi(int cpu)
{
    //struct mtk_irq_mask mask;
    bus_dcm_enable();
#ifdef CCI400_CLOCK_SWITCH
#ifdef CONFIG_SMP
    if ((atomic_read(&is_in_hotplug) == 0)&&(atomic_read(&hotplug_cpu_count) == 1))
#endif
    {
        mt_irq_mask_all(&mask);
        mt_cirq_clone_gic();
        mt_cirq_enable();

        cci400_clk_26MHz();

        mt_cirq_flush();
        mt_cirq_disable();
        mt_irq_mask_restore(&mask);

    }
#endif
}

static void slidle_after_wfi(int cpu)
{
#ifdef CCI400_CLOCK_SWITCH
    struct mtk_irq_mask mask;
#ifdef CONFIG_SMP
    if ((atomic_read(&is_in_hotplug) == 0)&&(atomic_read(&hotplug_cpu_count) == 1))
#endif
    {
        mt_irq_mask_all(&mask);
        mt_cirq_clone_gic();
        mt_cirq_enable();

        cci400_clk_restore();

        mt_cirq_flush();
        mt_cirq_disable();
        mt_irq_mask_restore(&mask);
    }
#endif
    bus_dcm_disable();
    slidle_cnt[cpu]++;
}

static void go_to_slidle(int cpu)
{
    slidle_before_wfi(cpu);

    dsb();
    __asm__ __volatile__("wfi" ::: "memory");

    slidle_after_wfi(cpu);
}


/************************************************
 * regular idle part
 ************************************************/
static void rgidle_before_wfi(int cpu)
{
#ifdef CCI400_CLOCK_SWITCH
	struct mtk_irq_mask mask;
#ifdef CONFIG_SMP
    if ((atomic_read(&is_in_hotplug) == 0)&&(atomic_read(&hotplug_cpu_count) == 1))
#endif
    {

        mt_irq_mask_all(&mask);
        mt_cirq_clone_gic();
        mt_cirq_enable();
        cci400_clk_26MHz();
        mt_cirq_flush();
        mt_cirq_disable();
        mt_irq_mask_restore(&mask);
    }
#endif
}

static void rgidle_after_wfi(int cpu)
{
#ifdef CCI400_CLOCK_SWITCH
    struct mtk_irq_mask mask;
#ifdef CONFIG_SMP
    if ((atomic_read(&is_in_hotplug) == 0)&&(atomic_read(&hotplug_cpu_count) == 1))
#endif
    {
        mt_irq_mask_all(&mask);
        mt_cirq_clone_gic();
        mt_cirq_enable();
        cci400_clk_restore();
        mt_cirq_flush();
        mt_cirq_disable();
        mt_irq_mask_restore(&mask);
    }
#endif
    rgidle_cnt[cpu]++;
}

static void noinline go_to_rgidle(int cpu)
{
    rgidle_before_wfi(cpu);

    dsb();
    __asm__ __volatile__("wfi" ::: "memory");

    rgidle_after_wfi(cpu);
}

/************************************************
 * idle task flow part
 ************************************************/

/*
 * xxidle_handler return 1 if enter and exit the low power state
 */

static inline int mcidle_handler(int cpu)
{

    if (idle_switch[IDLE_TYPE_MC]) {
        if (mcidle_can_enter(cpu)) {
            go_to_mcidle(cpu);
            return 1;
        }
    }

    return 0;
}
static u32 slp_spm_SODI_flags = {
	//SPM_CPU_DVS_DIS// not verfication yet
	0
};

static inline int soidle_handler(int cpu)
{
    if (idle_switch[IDLE_TYPE_SO]) {
        if (soidle_can_enter(cpu)) {
            //printk("SPM-Enter SODI\n");
            spm_go_to_sodi(slp_spm_SODI_flags, 0);
#ifdef CONFIG_SMP
            idle_ver("SO:timer_left=%d, timer_left2=%d, delta=%d\n",
                soidle_timer_left, soidle_timer_left2, soidle_timer_left-soidle_timer_left2);
#else
            idle_ver("SO:timer_left=%d, timer_left2=%d, delta=%d,timeout val=%d\n",
                soidle_timer_left, soidle_timer_left2, soidle_timer_left2-soidle_timer_left,soidle_timer_cmp-soidle_timer_left);
#endif
            return 1;
        }
    }

    return 0;
}
static u32 slp_spm_deepidle_flags = {
    0//SPM_CPU_DVS_DIS
};

static inline void dpidle_pre_handler(void)
{
//FIXME: for ROME-Plus build error
#if 1
    //cancel thermal hrtimer for power saving
    tscpu_cancel_thermal_timer();

    // disable gpu dvfs timer
    mtk_enable_gpu_dvfs_timer(false);
#endif    

    // disable cpu dvfs timer
    // hp_enable_timer(0); // TODO: FIXME: disable first

}
static inline void dpidle_post_handler(void)
{
//FIXME: for ROME-Plus build error
#if 1
    // enable gpu dvfs timer
    mtk_enable_gpu_dvfs_timer(true);

    //restart thermal hrtimer for update temp info
    tscpu_start_thermal_timer();
#endif

    // disable cpu dvfs timer
    // hp_enable_timer(1); // TODO: FIXME: disable first

}

static inline int dpidle_handler(int cpu)
{
    int ret = 0;
    if (idle_switch[IDLE_TYPE_DP]) {
        if (dpidle_can_enter()) {
            dpidle_pre_handler();
            spm_go_to_dpidle(slp_spm_deepidle_flags, 0);
            dpidle_post_handler();
            ret = 1;
#ifdef CONFIG_SMP
            idle_ver("DP:timer_left=%d, timer_left2=%d, delta=%d\n",
                dpidle_timer_left, dpidle_timer_left2, dpidle_timer_left-dpidle_timer_left2);
#else
            idle_ver("DP:timer_left=%d, timer_left2=%d, delta=%d, timeout val=%d\n",
                dpidle_timer_left, dpidle_timer_left2, dpidle_timer_left2-dpidle_timer_left,dpidle_timer_cmp-dpidle_timer_left);
#endif
        }
    }

    return ret;
}

static inline int slidle_handler(int cpu)
{
    int ret = 0;
    if (idle_switch[IDLE_TYPE_SL]) {
        if (slidle_can_enter()) {
            go_to_slidle(cpu);
            ret = 1;
        }
    }

    return ret;
}

static inline int rgidle_handler(int cpu)
{
    int ret = 0;
    if (idle_switch[IDLE_TYPE_RG]) {
        go_to_rgidle(cpu);
        ret = 1;
    }

    return ret;
}

static int (*idle_handlers[NR_TYPES])(int) = {
    dpidle_handler,
    soidle_handler,
    mcidle_handler,
    slidle_handler,
    rgidle_handler,
};

void arch_idle(void)
{
    int cpu = smp_processor_id();
    int i;

    //dynamic on/offload between single/multi core deepidles
    mcidle_idle_pre_handler(cpu);

    for (i = 0; i < NR_TYPES; i++) {
        if (idle_handlers[i](cpu))
            break;
    }
}

#define idle_attr(_name)                         \
static struct kobj_attribute _name##_attr = {   \
    .attr = {                                   \
        .name = __stringify(_name),             \
        .mode = 0644,                           \
    },                                          \
    .show = _name##_show,                       \
    .store = _name##_store,                     \
}

extern struct kobject *power_kobj;

static ssize_t mcidle_state_show(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
    int len = 0;
    char *p = buf;

    int cpus, reason;

    p += sprintf(p, "*********** multi-core idle state ************\n");
    p += sprintf(p, "mcidle_time_critera=%u\n", mcidle_time_critera);

    for (cpus = 0; cpus < nr_cpu_ids; cpus++) {
        p += sprintf(p, "cpu:%d\n", cpus);
        for (reason = 0; reason < NR_REASONS; reason++) {
            p += sprintf(p, "[%d]mcidle_block_cnt[%s]=%lu\n", reason,
                    reason_name[reason], mcidle_block_cnt[cpus][reason]);
        }
        p += sprintf(p, "\n");
    }

    p += sprintf(p, "\n********** mcidle command help **********\n");
    p += sprintf(p, "mcidle help:   cat /sys/power/mcidle_state\n");
    p += sprintf(p, "switch on/off: echo [mcidle] 1/0 > /sys/power/mcidle_state\n");
    p += sprintf(p, "modify tm_cri: echo time value(dec) > /sys/power/mcidle_state\n");

    len = p - buf;
    return len;
}

static ssize_t mcidle_state_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t n)
{
    char cmd[32];
    int param;

    if (sscanf(buf, "%s %d", cmd, &param) == 2) {
        if (!strcmp(cmd, "mcidle")) {
            idle_switch[IDLE_TYPE_MC] = param;
        }
        else if (!strcmp(cmd, "time")) {
            mcidle_time_critera = param;
        }
        return n;
    } else if (sscanf(buf, "%d", &param) == 1) {
        idle_switch[IDLE_TYPE_MC] = param;
        return n;
    }

    return -EINVAL;
}
idle_attr(mcidle_state);

static ssize_t soidle_state_show(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
    int len = 0;
    char *p = buf;

    int cpus, reason, i;

    p += sprintf(p, "*********** Screen on idle state ************\n");
    p += sprintf(p, "soidle_time_critera=%u\n", soidle_time_critera);

    for (cpus = 0; cpus < nr_cpu_ids; cpus++) {
        p += sprintf(p, "cpu:%d\n", cpus);
        for (reason = 0; reason < NR_REASONS; reason++) {
            p += sprintf(p, "[%d]soidle_block_cnt[%s]=%lu\n", reason,
                    reason_name[reason], soidle_block_cnt[cpus][reason]);
        }
        p += sprintf(p, "\n");
    }

    for (i = 0; i < NR_GRPS; i++) {
        p += sprintf(p, "[%02d]soidle_condition_mask[%-8s]=0x%08x\t\t"
                "soidle_block_mask[%-8s]=0x%08x\n", i,
                grp_get_name(i), soidle_condition_mask[i],
                grp_get_name(i), soidle_block_mask[i]);
    }


    p += sprintf(p, "\n********** soidle command help **********\n");
    p += sprintf(p, "soidle help:   cat /sys/power/soidle_state\n");
    p += sprintf(p, "switch on/off: echo [soidle] 1/0 > /sys/power/soidle_state\n");
    p += sprintf(p, "en_so_by_bit:  echo enable id > /sys/power/soidle_state\n");
    p += sprintf(p, "dis_so_by_bit: echo disable id > /sys/power/soidle_state\n");
    p += sprintf(p, "modify tm_cri: echo time value(dec) > /sys/power/soidle_state\n");

    len = p - buf;
    return len;
}

static ssize_t soidle_state_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t n)
{
    char cmd[32];
    int param;

    if (sscanf(buf, "%s %d", cmd, &param) == 2) {
        if (!strcmp(cmd, "soidle")) {
            idle_switch[IDLE_TYPE_SO] = param;
        } else if (!strcmp(cmd, "enable")) {
            enable_soidle_by_bit(param);
        } else if (!strcmp(cmd, "disable")) {
            disable_soidle_by_bit(param);
        } else if (!strcmp(cmd, "time")) {
            soidle_time_critera = param;
        }
        return n;
    } else if (sscanf(buf, "%d", &param) == 1) {
        idle_switch[IDLE_TYPE_SO] = param;
        return n;
    }

    return -EINVAL;
}
idle_attr(soidle_state);

static ssize_t dpidle_state_show(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
    int len = 0;
    char *p = buf;

    int i;

    p += sprintf(p, "*********** deep idle state ************\n");
    p += sprintf(p, "dpidle_time_critera=%u\n", dpidle_time_critera);

    for (i = 0; i < NR_REASONS; i++) {
        p += sprintf(p, "[%d]dpidle_block_cnt[%s]=%lu\n", i, reason_name[i],
                dpidle_block_cnt[i]);
    }

    p += sprintf(p, "\n");

    for (i = 0; i < NR_GRPS; i++) {
        p += sprintf(p, "[%02d]dpidle_condition_mask[%-8s]=0x%08x\t\t"
                "dpidle_block_mask[%-8s]=0x%08x\n", i,
                grp_get_name(i), dpidle_condition_mask[i],
                grp_get_name(i), dpidle_block_mask[i]);
    }

    p += sprintf(p, "\n*********** dpidle command help  ************\n");
    p += sprintf(p, "dpidle help:   cat /sys/power/dpidle_state\n");
    p += sprintf(p, "switch on/off: echo [dpidle] 1/0 > /sys/power/dpidle_state\n");
    p += sprintf(p, "cpupdn on/off: echo cpupdn 1/0 > /sys/power/dpidle_state\n");
    p += sprintf(p, "en_dp_by_bit:  echo enable id > /sys/power/dpidle_state\n");
    p += sprintf(p, "dis_dp_by_bit: echo disable id > /sys/power/dpidle_state\n");
    p += sprintf(p, "modify tm_cri: echo time value(dec) > /sys/power/dpidle_state\n");
    p += sprintf(p, "bypass cg:     echo bypass 1/0 > /sys/power/dpidle_state\n");

    len = p - buf;
    return len;
}

static ssize_t dpidle_state_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t n)
{
    char cmd[32];
    int param;

    if (sscanf(buf, "%s %d", cmd, &param) == 2) {
        if (!strcmp(cmd, "dpidle")) {
            idle_switch[IDLE_TYPE_DP] = param;
        } else if (!strcmp(cmd, "enable")) {
            enable_dpidle_by_bit(param);
        } else if (!strcmp(cmd, "disable")) {
            disable_dpidle_by_bit(param);
        } else if (!strcmp(cmd, "time")) {
            dpidle_time_critera = param;
        } else if (!strcmp(cmd, "bypass")) {
            dpidle_by_pass_cg = param;
        }

        return n;
    } else if (sscanf(buf, "%d", &param) == 1) {
        idle_switch[IDLE_TYPE_DP] = param;
        return n;
    }

    return -EINVAL;
}
idle_attr(dpidle_state);

static ssize_t slidle_state_show(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
    int len = 0;
    char *p = buf;

    int i;

    p += sprintf(p, "*********** slow idle state ************\n");
    for (i = 0; i < NR_REASONS; i++) {
        p += sprintf(p, "[%d]slidle_block_cnt[%s]=%lu\n",
                i, reason_name[i], slidle_block_cnt[i]);
    }

    p += sprintf(p, "\n");

    for (i = 0; i < NR_GRPS; i++) {
        p += sprintf(p, "[%02d]slidle_condition_mask[%-8s]=0x%08x\t\t"
                "slidle_block_mask[%-8s]=0x%08x\n", i,
                grp_get_name(i), slidle_condition_mask[i],
                grp_get_name(i), slidle_block_mask[i]);
    }

    p += sprintf(p, "\n********** slidle command help **********\n");
    p += sprintf(p, "slidle help:   cat /sys/power/slidle_state\n");
    p += sprintf(p, "switch on/off: echo [slidle] 1/0 > /sys/power/slidle_state\n");

    len = p - buf;
    return len;
}

static ssize_t slidle_state_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t n)
{
    char cmd[32];
    int param;

    if (sscanf(buf, "%s %d", cmd, &param) == 2) {
        if (!strcmp(cmd, "slidle")) {
            idle_switch[IDLE_TYPE_SL] = param;
        } else if (!strcmp(cmd, "enable")) {
            enable_slidle_by_bit(param);
        } else if (!strcmp(cmd, "disable")) {
            disable_slidle_by_bit(param);
        }
        return n;
    } else if (sscanf(buf, "%d", &param) == 1) {
        idle_switch[IDLE_TYPE_SL] = param;
        return n;
    }

    return -EINVAL;
}
idle_attr(slidle_state);

static ssize_t rgidle_state_show(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
    int len = 0;
    char *p = buf;

    p += sprintf(p, "*********** regular idle state ************\n");
    p += sprintf(p, "\n********** rgidle command help **********\n");
    p += sprintf(p, "rgidle help:   cat /sys/power/rgidle_state\n");
    p += sprintf(p, "switch on/off: echo [rgidle] 1/0 > /sys/power/rgidle_state\n");

    len = p - buf;
    return len;
}

static ssize_t rgidle_state_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t n)
{
    char cmd[32];
    int param;

    if (sscanf(buf, "%s %d", cmd, &param) == 2) {
        if (!strcmp(cmd, "rgidle")) {
            idle_switch[IDLE_TYPE_RG] = param;
        }
        return n;
    } else if (sscanf(buf, "%d", &param) == 1) {
        idle_switch[IDLE_TYPE_RG] = param;
        return n;
    }

    return -EINVAL;
}
idle_attr(rgidle_state);

static ssize_t idle_state_show(struct kobject *kobj,
                struct kobj_attribute *attr, char *buf)
{
    int len = 0;
    char *p = buf;

    int i;

    p += sprintf(p, "********** idle state dump **********\n");

    for (i = 0; i < nr_cpu_ids; i++) {
        p += sprintf(p, "soidle_cnt[%d]=%lu, dpidle_cnt[%d]=%lu, "
                "mcidle_cnt[%d]=%lu, slidle_cnt[%d]=%lu, rgidle_cnt[%d]=%lu\n",
                i, soidle_cnt[i], i, dpidle_cnt[i],
                i, mcidle_cnt[i], i, slidle_cnt[i], i, rgidle_cnt[i]);
    }

    p += sprintf(p, "\n********** variables dump **********\n");
    for (i = 0; i < NR_TYPES; i++) {
        p += sprintf(p, "%s_switch=%d, ", idle_name[i], idle_switch[i]);
    }
    p += sprintf(p, "\n");

    p += sprintf(p, "\n********** idle command help **********\n");
    p += sprintf(p, "status help:   cat /sys/power/idle_state\n");
    p += sprintf(p, "switch on/off: echo switch mask > /sys/power/idle_state\n");

    p += sprintf(p, "soidle help:   cat /sys/power/soidle_state\n");
    p += sprintf(p, "mcidle help:   cat /sys/power/mcidle_state\n");
    p += sprintf(p, "dpidle help:   cat /sys/power/dpidle_state\n");
    p += sprintf(p, "slidle help:   cat /sys/power/slidle_state\n");
    p += sprintf(p, "rgidle help:   cat /sys/power/rgidle_state\n");

    len = p - buf;
    return len;
}

static ssize_t idle_state_store(struct kobject *kobj,
                struct kobj_attribute *attr, const char *buf, size_t n)
{
    char cmd[32];
    int idx;
    int param;

    if (sscanf(buf, "%s %x", cmd, &param) == 2) {
        if (!strcmp(cmd, "switch")) {
            for (idx = 0; idx < NR_TYPES; idx++) {
                idle_switch[idx] = (param & (1U << idx)) ? 1 : 0;
            }
        }
        return n;
    }

    return -EINVAL;
}
idle_attr(idle_state);

void mt_idle_init(void)
{
    int err = 0;
    int i = 0;

    idle_info("[%s]entry!!\n", __func__);

    arm_pm_idle = arch_idle;

    err = request_gpt(idle_gpt, GPT_ONE_SHOT, GPT_CLK_SRC_SYS, GPT_CLK_DIV_1,
                0, NULL, GPT_NOAUTOEN);
    if (err) {
        idle_info("[%s]fail to request GPT%d\n", __func__,idle_gpt+1);
    }

    err = 0;

    for(i=0;i<NR_CPUS;i++){
        err |= cpu_xgpt_register_timer(i,NULL);
    }

    if (err) {
        idle_info("[%s]fail to request cpuxgpt\n", __func__);
    }

    err = sysfs_create_file(power_kobj, &idle_state_attr.attr);
    err |= sysfs_create_file(power_kobj, &soidle_state_attr.attr);
    err |= sysfs_create_file(power_kobj, &mcidle_state_attr.attr);
    err |= sysfs_create_file(power_kobj, &dpidle_state_attr.attr);
    err |= sysfs_create_file(power_kobj, &slidle_state_attr.attr);
    err |= sysfs_create_file(power_kobj, &rgidle_state_attr.attr);

    if (err) {
        idle_err("[%s]: fail to create sysfs\n", __func__);
    }
}

module_param(mt_idle_chk_golden, bool, 0644);
module_param(mt_dpidle_chk_golden, bool, 0644);
