#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/suspend.h>
#include <linux/console.h>
#include <linux/aee.h>
#include <linux/proc_fs.h>
#include <linux/fs.h>
#include <linux/vmalloc.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/time.h>
#include <linux/kthread.h>

#include <mach/sync_write.h>
#include <mach/mt_sleep.h>
#include <mach/mt_spm.h>
#include <mach/mt_spm_sleep.h>
#include <mach/mt_spm_idle.h>
#include <mach/mt_spm_mtcmos.h>
#include <mach/mt_clkmgr.h>
#include <mach/mt_gpio.h>
#include <mach/systracker.h>
#include <mach/battery_common.h>

#include <cust_gpio_usage.h>

#include <mach/mt_boot.h>

/**************************************
 * only for internal debug
 **************************************/
#ifdef CONFIG_MTK_LDVT
#define SLP_SLEEP_DPIDLE_EN         1
#define SLP_REPLACE_DEF_WAKESRC     1
#define SLP_SUSPEND_LOG_EN          1
#else
#define SLP_SLEEP_DPIDLE_EN         0
#define SLP_REPLACE_DEF_WAKESRC     0
#define SLP_SUSPEND_LOG_EN          0
#endif

#ifndef MTK_DUAL_VCORE_SUSPEND
#define MTK_DUAL_VCORE_SUSPEND 0
#endif

/**************************************
 * SW code for suspend
 **************************************/
#define slp_read(addr)              (*(volatile u32 *)(addr))
#define slp_write(addr, val)        mt65xx_reg_sync_writel(val, addr)

#define slp_emerg(fmt, args...)     printk(KERN_EMERG "[SLP] " fmt, ##args)
#define slp_alert(fmt, args...)     printk(KERN_ALERT "[SLP] " fmt, ##args)
#define slp_crit(fmt, args...)      printk(KERN_CRIT "[SLP] " fmt, ##args)
#define slp_error(fmt, args...)     printk(KERN_ERR "[SLP] " fmt, ##args)
#define slp_warning(fmt, args...)   printk(KERN_WARNING "[SLP] " fmt, ##args)
#define slp_notice(fmt, args...)    printk(KERN_NOTICE "[SLP] " fmt, ##args)
#define slp_info(fmt, args...)      printk(KERN_INFO "[SLP] " fmt, ##args)
#define slp_debug(fmt, args...)     printk(KERN_DEBUG "[SLP] " fmt, ##args)

#define slp_crit2(fmt, args...)     \
do {                                \
    aee_sram_printk(fmt, ##args);   \
    slp_crit(fmt, ##args);          \
} while (0)

extern void mt_power_gs_dump_suspend(void);

static DEFINE_SPINLOCK(slp_lock);

static wake_reason_t slp_wake_reason = WR_NONE;

static bool slp_ck26m_on = 0;
static bool slp_pars_dpd = 1;
static bool slp_dual_vcore = 0;
static bool slp_dual_vcore_ready = MTK_DUAL_VCORE_SUSPEND;
static bool slp_dual_vcore_lte = 1;

static bool slp_chk_golden = 1;
static bool slp_dump_gpio = 0;
static bool slp_dump_regs = 1;


static bool slp_auto_suspend_resume = 0;
static u32 slp_auto_suspend_resume_cnt = 0;
struct wake_lock spm_suspend_lock;
static struct hrtimer slp_auto_suspend_resume_hrtimer;
struct task_struct *slp_auto_suspend_resume_thread = NULL;
static int slp_auto_suspend_resume_timer_flag = 0;
static DECLARE_WAIT_QUEUE_HEAD(slp_auto_suspend_resume_timer_waiter);
static u32 slp_time = 30;

static u32 slp_spm_flags = {
//FIXME: for bring up
#if 1
#if defined(CONFIG_MTK_DT_USB_SUPPORT) || defined(CONFIG_MTK_ICUSB_SUPPORT)
	/* disable infra pdn to avoid usb20 host pdn */
	/*SPM_CPU_PDN_DIS |*/ SPM_INFRA_PDN_DIS | /*SPM_DDRPHY_PDN_DIS |*/
	SPM_DUALVCORE_PDN_DIS | SPM_PASR_DIS | SPM_DPD_DIS /*| SPM_CPU_DVS_DIS*/
#else
	/*SPM_CPU_PDN_DIS | SPM_INFRA_PDN_DIS | SPM_DDRPHY_PDN_DIS |*/
	SPM_DUALVCORE_PDN_DIS | SPM_PASR_DIS | SPM_DPD_DIS /*| SPM_CPU_DVS_DIS*/
#endif
#else
	0
#endif
};

#if SLP_SLEEP_DPIDLE_EN
static u32 slp_spm_deepidle_flags = {
	SPM_CPU_PDN_DIS
};
#endif


static u32 slp_spm_data = 0;

static enum hrtimer_restart slp_auto_suspend_resume_timer_func(struct hrtimer *timer)
{
    slp_crit2("do slp_auto_suspend_resume_timer_func\n");

    slp_auto_suspend_resume = 0;
    slp_auto_suspend_resume_cnt =0;

#if 0
    charging_suspend_enable();
#else
    slp_auto_suspend_resume_timer_flag = 1;
    wake_up_interruptible(&slp_auto_suspend_resume_timer_waiter);
#endif

	return HRTIMER_NORESTART;
}

static int slp_auto_suspend_resume_thread_handler(void *unused)
{
    do
    {
        wait_event_interruptible(slp_auto_suspend_resume_timer_waiter, slp_auto_suspend_resume_timer_flag != 0);
        slp_auto_suspend_resume_timer_flag = 0;

        charging_suspend_enable();
        slp_crit2("slp_auto_suspend_resume_thread_handler charging_suspend_enable\n");

    } while (!kthread_should_stop());

    return 0;
}

static void slp_dump_pm_regs(void)
{
//FIXME: for bring up
#if 0

    /* PLL/TOPCKGEN register */
    slp_debug("AP_PLL_CON0     0x%x = 0x%x\n", AP_PLL_CON0        , slp_read(AP_PLL_CON0));
    slp_debug("AP_PLL_CON1     0x%x = 0x%x\n", AP_PLL_CON1        , slp_read(AP_PLL_CON1));
    slp_debug("AP_PLL_CON2     0x%x = 0x%x\n", AP_PLL_CON2        , slp_read(AP_PLL_CON2));
    slp_debug("UNIVPLL_CON0    0x%x = 0x%x\n", UNIVPLL_CON0       , slp_read(UNIVPLL_CON0));
    slp_debug("UNIVPLL_PWR_CON 0x%x = 0x%x\n", UNIVPLL_PWR_CON0   , slp_read(UNIVPLL_PWR_CON0));
    slp_debug("MMPLL_CON0      0x%x = 0x%x\n", MMPLL_CON0         , slp_read(MMPLL_CON0));
    slp_debug("MMPLL_PWR_CON   0x%x = 0x%x\n", MMPLL_PWR_CON0     , slp_read(MMPLL_PWR_CON0));
    slp_debug("CLK_SCP_CFG_0   0x%x = 0x%x\n", CLK_SCP_CFG_0      , slp_read(CLK_SCP_CFG_0));
    slp_debug("CLK_SCP_CFG_1   0x%x = 0x%x\n", CLK_SCP_CFG_1      , slp_read(CLK_SCP_CFG_1));

    /* INFRA/PERICFG register */
    slp_debug("INFRA_PDN_STA   0x%x = 0x%x\n", INFRA_PDN_STA      , slp_read(INFRA_PDN_STA));
    slp_debug("PERI_PDN0_STA   0x%x = 0x%x\n", PERI_PDN0_STA      , slp_read(PERI_PDN0_STA));

    /* SPM register */
    slp_debug("POWER_ON_VAL0   0x%x = 0x%x\n", SPM_POWER_ON_VAL0  , slp_read(SPM_POWER_ON_VAL0));
    slp_debug("POWER_ON_VAL1   0x%x = 0x%x\n", SPM_POWER_ON_VAL1  , slp_read(SPM_POWER_ON_VAL1));
    slp_debug("SPM_PCM_CON1    0x%x = 0x%x\n", SPM_PCM_CON1       , slp_read(SPM_PCM_CON1));
    slp_debug("PCM_PWR_IO_EN   0x%x = 0x%x\n", SPM_PCM_PWR_IO_EN  , slp_read(SPM_PCM_PWR_IO_EN));
    slp_debug("PCM_REG0_DATA   0x%x = 0x%x\n", SPM_PCM_REG0_DATA  , slp_read(SPM_PCM_REG0_DATA));
    slp_debug("PCM_REG7_DATA   0x%x = 0x%x\n", SPM_PCM_REG7_DATA  , slp_read(SPM_PCM_REG7_DATA));
    slp_debug("PCM_REG13_DATA  0x%x = 0x%x\n", SPM_PCM_REG13_DATA , slp_read(SPM_PCM_REG13_DATA));
    slp_debug("CLK_CON         0x%x = 0x%x\n", SPM_CLK_CON        , slp_read(SPM_CLK_CON));
    slp_debug("AP_DVFS_CON     0x%x = 0x%x\n", SPM_AP_DVFS_CON_SET, slp_read(SPM_AP_DVFS_CON_SET));
    slp_debug("PWR_STATUS      0x%x = 0x%x\n", SPM_PWR_STATUS     , slp_read(SPM_PWR_STATUS));
    slp_debug("SPM_PCM_SRC_REQ 0x%x = 0x%x\n", SPM_PCM_SRC_REQ    , slp_read(SPM_PCM_SRC_REQ));
#endif
}

static void slp_dualvcore_pdn(u8 pdn)
{
    if (!slp_dual_vcore_lte)
        spm_force_lte_onoff(0);

    if (pdn)
        slp_spm_flags &= ~SPM_DUALVCORE_PDN_DIS;
    else
        slp_spm_flags |= SPM_DUALVCORE_PDN_DIS;
}

static int slp_suspend_ops_valid(suspend_state_t state)
{
    return state == PM_SUSPEND_MEM;
}

static int slp_suspend_ops_begin(suspend_state_t state)
{
    /* legacy log */
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");
    slp_notice("Chip_pm_begin(%u)(%u)\n", is_cpu_pdn(slp_spm_flags), is_infra_pdn(slp_spm_flags));
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");

    slp_wake_reason = WR_NONE;

    return 0;
}

static int slp_suspend_ops_prepare(void)
{
    /* legacy log */
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");
    slp_crit2("Chip_pm_prepare\n");
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");

//FIXME: for FPGA early porting
#if 1
    if (slp_chk_golden)
        mt_power_gs_dump_suspend();
#endif
    return 0;
}

#ifdef CONFIG_MTKPASR
/* PASR/DPD Preliminary operations */
extern void mtkpasr_phaseone_ops(void);
static int slp_suspend_ops_prepare_late(void)
{
	slp_notice("[%s]\n",__FUNCTION__);
	mtkpasr_phaseone_ops();
	return 0;
}
static void slp_suspend_ops_wake(void)
{
	slp_notice("[%s]\n",__FUNCTION__);
}

/* PASR/DPD SW operations */
extern int configure_mrw_pasr(u32 segment_rank0, u32 segment_rank1);
extern int pasr_enter(u32 *sr, u32 *dpd);
extern int pasr_exit(void);
extern unsigned long mtkpasr_enable_sr;
extern void enter_pasr_dpd_config(unsigned char segment_rank0, unsigned char segment_rank1);
extern void exit_pasr_dpd_config(void);
static int enter_pasrdpd(void)
{
	int error = 0;
	u32 sr = 0, dpd = 0;

    	slp_notice("@@@@@@@@@@@@@@@@@@@@\n");
	slp_crit2("[%s]\n",__FUNCTION__);
    	slp_notice("@@@@@@@@@@@@@@@@@@@@\n");
	
	/* Setup SPM wakeup event firstly */
	spm_set_wakeup_src_check();

	/* Start PASR/DPD SW operations */
	error = pasr_enter(&sr, &dpd);

	if (error) {
		slp_crit2("[PM_WAKEUP] Failed to enter PASR!\n");
	} else {	
		/* Call SPM/DPD control API */
		slp_crit2("MR17[0x%x] DPD[0x%x]\n",sr,dpd);
		/* Should configure SR */
		if (mtkpasr_enable_sr == 0) {
			sr = 0x0;
			slp_crit2("[%s][%d] No configuration on SR\n",__FUNCTION__,__LINE__);
		}
		/* Configure PASR */
		enter_pasr_dpd_config((sr & 0xFF), (sr >> 0x8));
		//if (mrw_error) {
		//	printk(KERN_ERR "[%s][%d] PM: Failed to configure MRW PASR [%d]!\n",__FUNCTION__,__LINE__,mrw_error);
		//}
	}
	slp_crit2("Bye [%s]\n",__FUNCTION__);

	return error;
}
static void leave_pasrdpd(void)
{
  	slp_notice("@@@@@@@@@@@@@@@@@@@@\n");
	slp_crit2("[%s]\n",__FUNCTION__);
   	slp_notice("@@@@@@@@@@@@@@@@@@@@\n");

	/* Disable PASR */
	exit_pasr_dpd_config();
	
	slp_crit2("[%d]\n",__LINE__);

	/* End PASR/DPD SW operations */
	pasr_exit();
	
	slp_crit2("Bye [%s]\n",__FUNCTION__);
}
#endif



static int slp_suspend_ops_enter(suspend_state_t state)
{
    int ret = 0;

#ifdef CONFIG_MTKPASR
    /* PASR SW operations */
    enter_pasrdpd();
#endif

    /* legacy log */
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");
    slp_crit2("Chip_pm_enter\n");
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");

    if (slp_dump_gpio)
        gpio_dump_regs();

    if (slp_dump_regs)
        slp_dump_pm_regs();

    if (CHIP_SW_VER_02 <= mt_get_chip_sw_ver())
    {
        if (!spm_cpusys0_can_power_down()) {
            slp_error("CANNOT SLEEP DUE TO CPU1/2/3 PON\n");
            //return -EPERM;
            ret = -EPERM;
            goto LEAVE_SLEEP;
        }
    }
    else
    {
#ifdef CONFIG_MTK_FORCE_CLUSTER1
    if (!spm_cpusys1_can_power_down()) {
#else
    if (!spm_cpusys0_can_power_down()) {
#endif
        slp_error("CANNOT SLEEP DUE TO CPU1/2/3 PON\n");
        //return -EPERM;
        ret = -EPERM;
        goto LEAVE_SLEEP;
    }
    }

    if (is_infra_pdn(slp_spm_flags) && !is_cpu_pdn(slp_spm_flags)) {
        slp_error("CANNOT SLEEP DUE TO INFRA PDN BUT CPU PON\n");
        //return -EPERM;
        ret = -EPERM;
        goto LEAVE_SLEEP;
    }

    slp_dualvcore_pdn(slp_dual_vcore);
    
    /* only for test */
    #if 0
    slp_pasr_en(1, 0x0);
    slp_dpd_en(1);
    #endif

#if SLP_SLEEP_DPIDLE_EN
    if (slp_ck26m_on)
        slp_wake_reason = spm_go_to_sleep_dpidle(slp_spm_deepidle_flags, slp_spm_data);
    else
#endif
        slp_wake_reason = spm_go_to_sleep(slp_spm_flags, slp_spm_data);

LEAVE_SLEEP:
#ifdef CONFIG_MTKPASR
    /* PASR SW operations */
    leave_pasrdpd();
#endif

#ifdef CONFIG_MTK_SYSTRACKER
    enable_systracker();
#endif

    return ret;
}

static void slp_suspend_ops_finish(void)
{
    /* legacy log */
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");
    slp_crit2("Chip_pm_finish\n");
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");
}

static void slp_suspend_ops_end(void)
{
    /* legacy log */
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");
    slp_notice("Chip_pm_end\n");
    slp_notice("@@@@@@@@@@@@@@@@@@@@\n");

    if (1 == slp_auto_suspend_resume)
    {
        slp_crit2("slp_auto_suspend_resume_cnt = %d \n", slp_auto_suspend_resume_cnt);
        slp_auto_suspend_resume_cnt++;
        
        if (10 < slp_auto_suspend_resume_cnt)
        {
            slp_crit2("do spm_usb_resume\n");

            wake_lock(&spm_suspend_lock);
            slp_auto_suspend_resume = 0;
            slp_auto_suspend_resume_cnt =0;
        }
    }
}

static struct platform_suspend_ops slp_suspend_ops = {
    .valid      = slp_suspend_ops_valid,
    .begin      = slp_suspend_ops_begin,
    .prepare    = slp_suspend_ops_prepare,
    .enter      = slp_suspend_ops_enter,
    .finish     = slp_suspend_ops_finish,
    .end        = slp_suspend_ops_end,
#ifdef CONFIG_MTKPASR
    .prepare_late = slp_suspend_ops_prepare_late,
    .wake	  = slp_suspend_ops_wake,
#endif
};

/*
 * wakesrc : WAKE_SRC_XXX
 * enable  : enable or disable @wakesrc
 * ck26m_on: if true, mean @wakesrc needs 26M to work
 */
int slp_set_wakesrc(u32 wakesrc, bool enable, bool ck26m_on)
{
    int r;
    unsigned long flags;

    slp_notice("wakesrc = 0x%x, enable = %u, ck26m_on = %u\n",
               wakesrc, enable, ck26m_on);

#if SLP_REPLACE_DEF_WAKESRC
    if (wakesrc & WAKE_SRC_CFG_KEY)
#else
    if (!(wakesrc & WAKE_SRC_CFG_KEY))
#endif
        return -EPERM;

    spin_lock_irqsave(&slp_lock, flags);
#if SLP_REPLACE_DEF_WAKESRC
    if (ck26m_on)
	    r = spm_set_dpidle_wakesrc(wakesrc, enable, true);
	else
    r = spm_set_sleep_wakesrc(wakesrc, enable, true);
#else
    if (ck26m_on)
	    r = spm_set_dpidle_wakesrc(wakesrc & ~WAKE_SRC_CFG_KEY, enable, false);
	else
    r = spm_set_sleep_wakesrc(wakesrc & ~WAKE_SRC_CFG_KEY, enable, false);
#endif

    if (!r)
        slp_ck26m_on = ck26m_on;
    spin_unlock_irqrestore(&slp_lock, flags);

    return r;
}

wake_reason_t slp_get_wake_reason(void)
{
    return slp_wake_reason;
}

bool slp_will_infra_pdn(void)
{
    return is_infra_pdn(slp_spm_flags);
}

/*
 * en: 1: enable dual-vcore suspend, 0: disable dual-vcore suspend
 */
void slp_dualvcore_en(bool en)
{
    slp_dual_vcore = en;
}

bool slp_get_dualvcore(void)
{
    return slp_dual_vcore;
}

bool slp_dualvcore_ready(void)
{
    return slp_dual_vcore_ready;
}

/*
 * en: 1: enable pasr, 0: disable pasr
 * value: pasr setting (RK1, MR17 for RK0)
 */
void slp_pasr_en(bool en, u32 value)
{
    if (slp_pars_dpd)
    {
        if (en) {
            slp_spm_flags &= ~SPM_PASR_DIS;
            slp_spm_data = value;
        } else {
            slp_spm_flags |= SPM_PASR_DIS;
            slp_spm_data = 0;
        }
    }
}

/*
 * en: 1: enable DPD, 0: disable DPD
 */
void slp_dpd_en(bool en)
{
    if (slp_pars_dpd)
    {
        if (en)
            slp_spm_flags &= ~SPM_DPD_DIS;
        else
            slp_spm_flags |= SPM_DPD_DIS;
    }
}

void slp_module_init(void)
{
    spm_output_sleep_option();

    slp_notice("SLEEP_DPIDLE_EN:%d, REPLACE_DEF_WAKESRC:%d, SUSPEND_LOG_EN:%d\n",
               SLP_SLEEP_DPIDLE_EN, SLP_REPLACE_DEF_WAKESRC, SLP_SUSPEND_LOG_EN);

        suspend_set_ops(&slp_suspend_ops);

#if SLP_SUSPEND_LOG_EN
    console_suspend_enabled = 0;
#endif

    wake_lock_init(&spm_suspend_lock, WAKE_LOCK_SUSPEND, "spm_wakelock");
}

void slp_start_auto_suspend_resume_timer(u32 sec)
{
	ktime_t ktime;

    slp_auto_suspend_resume = 1;
    charging_suspend_disable();

    slp_time = sec;
	slp_crit2("slp_start_auto_suspend_resume_timer init = %d\n", slp_time);

	ktime = ktime_set(slp_time, 0);

	hrtimer_init(&slp_auto_suspend_resume_hrtimer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);

	slp_auto_suspend_resume_hrtimer.function = slp_auto_suspend_resume_timer_func;
	hrtimer_start(&slp_auto_suspend_resume_hrtimer,ktime,HRTIMER_MODE_REL);
}

void slp_create_auto_suspend_resume_thread(void)
{
    if (NULL == slp_auto_suspend_resume_thread)
        slp_auto_suspend_resume_thread = kthread_run(slp_auto_suspend_resume_thread_handler, 0, "auto suspend resume");
}

void slp_set_auto_suspend_wakelock(bool lock)
{
    if (lock)
        wake_lock(&spm_suspend_lock);
    else
        wake_unlock(&spm_suspend_lock);    
}

module_param(slp_ck26m_on, bool, 0644);
module_param(slp_pars_dpd, bool, 0644);
module_param(slp_dual_vcore, bool, 0644);
module_param(slp_dual_vcore_ready, bool, 0644);
module_param(slp_dual_vcore_lte, bool, 0644);
module_param(slp_spm_flags, uint, 0644);

module_param(slp_chk_golden, bool, 0644);
module_param(slp_dump_gpio, bool, 0644);
module_param(slp_dump_regs, bool, 0644);

MODULE_DESCRIPTION("Sleep Driver v0.1");
