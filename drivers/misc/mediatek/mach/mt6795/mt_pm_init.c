#include <linux/pm.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/spinlock.h>
#include <linux/interrupt.h>
#include <linux/types.h>
#include <linux/xlog.h>

#include <asm/io.h>
#include <asm/uaccess.h>

#include "mach/irqs.h"
#include "mach/sync_write.h"
#include "mach/mt_reg_base.h"
#include "mach/mt_typedefs.h"
#include "mach/mt_spm.h"
#include "mach/mt_sleep.h"
#include "mach/mt_dcm.h"
#include "mach/mt_clkmgr.h"
#include "mach/mt_cpufreq.h"
#include "mach/mt_gpufreq.h"
#include "mach/mt_dormant.h"
#include "mach/mt_cpuidle.h"
#include "mach/hotplug.h"
#include <mach/mt_boot.h>                   //mt_get_chip_sw_ver
#include <mach/upmu_common.h>               //mt6331_upmu_set_rg_vsram_dvfs1_en, mt6332_upmu_set_rg_vsram_dvfs2_en
#include <mach/mt_spm_mtcmos_internal.h>    //VCA15_PWR_ISO
#include <mach/mt_clkbuf_ctl.h>

#define pminit_write(addr, val)         mt_reg_sync_writel((val), ((void *)(addr)))
#define pminit_read(addr)               __raw_readl(IOMEM(addr))

extern int mt_clkmgr_init(void);
extern void mt_idle_init(void);
extern void mt_power_off(void);
extern void mt_dcm_init(void);
extern int set_da9210_buck_en(int en_bit); //definition in mediatek/platform/mt6595/kernel/drivers/power/da9210.c
extern void bigcore_power_off(void); //definition in mediatek/platform/mt6595/kernel/drivers/power/da9210.c
extern void bigcore_power_on(void); //definition in mediatek/platform/mt6595/kernel/drivers/power/da9210.c

/*******************************************************************************
* ca15l vproc(ext buck)
*******************************************************************************/
static void _power_off_ca15l_vproc_vsram(void)
{
    /* enable ca15l power isolation */
    pminit_write(SPM_SLEEP_DUAL_VCORE_PWR_CON, pminit_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) | VCA15_PWR_ISO);
#if 1
    printk("bigcore_power_off\n");
    bigcore_power_off();
#else
    printk("_power_off_ca15l_vproc_vsram\n");

    /* turn off ca15l vproc(ext buck) */
    set_da9210_buck_en(0);

    /* turn off ca15l vsram */
    mt6331_upmu_set_rg_vsram_dvfs1_en(0);
#endif
}

static void _power_on_ca15l_vproc_vsram(void)
{
#if 1
    printk("bigcore_power_on\n");
    bigcore_power_on();
#else
    printk("_power_on_ca15l_vproc_vsram\n");

    /* turn on ca15l vsram */
    mt6331_upmu_set_rg_vsram_dvfs1_en(1);

    /* turn on ca15l vproc(ext buck) */
    set_da9210_buck_en(1);
#endif
    /* disable ca15l power isolation */
    pminit_write(SPM_SLEEP_DUAL_VCORE_PWR_CON, pminit_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) & ~VCA15_PWR_ISO);
}

static int ext_buck_read(struct seq_file *m, void *v)
{
    if ((pminit_read(SPM_SLEEP_DUAL_VCORE_PWR_CON) & VCA15_PWR_ISO) == VCA15_PWR_ISO)
        seq_printf(m, "0\n");
    else
        seq_printf(m, "1\n");

    return 0;
}

static int ext_buck_write(struct file *file, const char __user *buffer,
                size_t count, loff_t *data)
{
    char desc[128];
    int len = 0;
    int val;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d", &val) == 1) {
        switch(val)
        {
            case 0:
                _power_off_ca15l_vproc_vsram();
                break;
            case 1:
                _power_on_ca15l_vproc_vsram();
                break;
            default:
                break;
        }
    }
    return count;
}

static int proc_ext_buck_open(struct inode *inode, struct file *file)
{
    return single_open(file, ext_buck_read, NULL);
}

static const struct file_operations ext_buck_fops = {
    .owner = THIS_MODULE,
    .open  = proc_ext_buck_open,
    .read  = seq_read,
    .write = ext_buck_write,
};



#define TOPCK_LDVT

#ifdef TOPCK_LDVT
/***************************
*For TOPCKGen Meter LDVT Test
****************************/
unsigned int ckgen_meter(int val)
{
	int output = 0, i = 0;
    unsigned int temp, clk26cali_0, clk_cfg_9, clk_misc_cfg_1;

    clk26cali_0 = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

    clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
    pminit_write(CLK_MISC_CFG_1, 0x00FFFFFF); // select divider

    clk_cfg_9 = DRV_Reg32(CLK_CFG_9);
    pminit_write(CLK_CFG_9, (val << 16)); // select abist_cksw

    temp = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, temp | 0x10); // start fmeter

    /* wait frequency meter finish */
    while (DRV_Reg32(CLK26CALI_0) & 0x10)
    {
        printk("%d, wait for frequency meter finish, CLK26CALI = 0x%x\n", val, DRV_Reg32(CLK26CALI_0));
        mdelay(10);
        i++;
        if(i > 10)
        	break;
    }

    temp = DRV_Reg32(CLK26CALI_2) & 0xFFFF;

    output = (temp * 26000) / 1024; // Khz

    pminit_write(CLK_CFG_9, clk_cfg_9);
    pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
    pminit_write(CLK26CALI_0, clk26cali_0);

    if(i>10)
        return 0;
    else
        return output;
}

unsigned int abist_meter(int val)
{
    int output = 0, i = 0;
    unsigned int temp, clk26cali_0, clk_cfg_8, clk_misc_cfg_1, /*clk26cali_1,*/ ap_pll_con0;

    ap_pll_con0 = DRV_Reg32(AP_PLL_CON0);
    pminit_write(AP_PLL_CON0, ap_pll_con0 | 0x50);//turn on MIPI26M and SSUSB26M

    clk26cali_0 = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

    clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
    pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00); // select divider

    clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
    pminit_write(CLK_CFG_8, (val << 8)); // select abist_cksw

    temp = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, temp | 0x1); // start fmeter

    /* wait frequency meter finish */
    while (DRV_Reg32(CLK26CALI_0) & 0x1)
    {
        printk("%d, wait for frequency meter finish, CLK26CALI = 0x%x\n", val, DRV_Reg32(CLK26CALI_0));
        mdelay(10);
        i++;
        if(i > 10)
        	break;
    }

    temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

    output = (temp * 26000) / 1024; // Khz

    pminit_write(CLK_CFG_8, clk_cfg_8);
    pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
    pminit_write(CLK26CALI_0, clk26cali_0);
    pminit_write(AP_PLL_CON0, ap_pll_con0);

    if(i>10)
        return 0;
    else
        return output;
}

static int ckgen_meter_read(struct seq_file *m, void *v)
{
	int i;

	for(i=1; i<38; i++)
    	seq_printf(m, "%d\n", ckgen_meter(i));

    return 0;
}

static int ckgen_meter_write(struct file *file, const char __user *buffer,
                size_t count, loff_t *data)
{
    char desc[128];
    int len = 0;
    int val;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d", &val) == 1) {
        printk("ckgen_meter %d is %d\n", val, ckgen_meter(val));
    }
    return count;
}


static int abist_meter_read(struct seq_file *m, void *v)
{
	int i;

	for(i=1; i<48; i++)
    	seq_printf(m, "%d\n", abist_meter(i));

    return 0;
}
static int abist_meter_write(struct file *file, const char __user *buffer,
                size_t count, loff_t *data)
{
    char desc[128];
    int len = 0;
    int val;

    len = (count < (sizeof(desc) - 1)) ? count : (sizeof(desc) - 1);
    if (copy_from_user(desc, buffer, len)) {
        return 0;
    }
    desc[len] = '\0';

    if (sscanf(desc, "%d", &val) == 1) {
        printk("abist_meter %d is %d\n", val, abist_meter(val));
    }
    return count;
}

static int proc_abist_meter_open(struct inode *inode, struct file *file)
{
    return single_open(file, abist_meter_read, NULL);
}
static const struct file_operations abist_meter_fops = {
    .owner = THIS_MODULE,
    .open  = proc_abist_meter_open,
    .read  = seq_read,
    .write = abist_meter_write,
};

static int proc_ckgen_meter_open(struct inode *inode, struct file *file)
{
    return single_open(file, ckgen_meter_read, NULL);
}
static const struct file_operations ckgen_meter_fops = {
    .owner = THIS_MODULE,
    .open  = proc_ckgen_meter_open,
    .read  = seq_read,
    .write = ckgen_meter_write,
};

#endif

/*********************************************************************
 * FUNCTION DEFINATIONS
 ********************************************************************/

unsigned int mt_get_emi_freq(void)
{
    int output = 0;
    unsigned int temp, clk26cali_0, clk_cfg_8, clk_misc_cfg_1/*, clk26cali_1*/;

    clk26cali_0 = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

    clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
    pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00); // select divider

    clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
    pminit_write(CLK_CFG_8, (24 << 8)); // select abist_cksw

    temp = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, temp | 0x1); // start fmeter

    /* wait frequency meter finish */
    while (DRV_Reg32(CLK26CALI_0) & 0x1)
    {
        printk("wait for frequency meter finish, CLK26CALI = 0x%x\n", DRV_Reg32(CLK26CALI_0));
        //mdelay(10);
    }

    temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

    output = (temp * 26000) / 1024; // Khz

    pminit_write(CLK_CFG_8, clk_cfg_8);
    pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
    pminit_write(CLK26CALI_0, clk26cali_0);

    //print("CLK26CALI = 0x%x, mem frequency = %d Khz\n", temp, output);

    return output;
}
EXPORT_SYMBOL(mt_get_emi_freq);

unsigned int mt_get_bus_freq(void)
{
#if 1
    int output = 0;
    unsigned int temp, clk26cali_0, clk_cfg_9, clk_misc_cfg_1/*, clk26cali_2*/;

    clk26cali_0 = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

    clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
    pminit_write(CLK_MISC_CFG_1, 0x00FFFFFF); // select divider

    clk_cfg_9 = DRV_Reg32(CLK_CFG_9);
    pminit_write(CLK_CFG_9, (1 << 16)); // select ckgen_cksw

    temp = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, temp | 0x10); // start fmeter

    /* wait frequency meter finish */
    while (DRV_Reg32(CLK26CALI_0) & 0x10)
    {
        //printk("wait for bus frequency meter finish, CLK26CALI = 0x%x\n", DRV_Reg32(CLK26CALI_0));
        mdelay(10);
    }

    temp = DRV_Reg32(CLK26CALI_2) & 0xFFFF;

    output = (temp * 26000) / 1024; // Khz

    pminit_write(CLK_CFG_9, clk_cfg_9);
    pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
    pminit_write(CLK26CALI_0, clk26cali_0);

    //printk("CLK26CALI = 0x%x, bus frequency = %d Khz\n", temp, output);

    return output;
#else
    unsigned int mainpll_con0, mainpll_con1, main_diff;
    unsigned int clk_cfg_0, bus_clk;
    unsigned int output_freq = 0;

    clk_cfg_0 = DRV_Reg32(CLK_CFG_0);

    mainpll_con0 = DRV_Reg32(MAINPLL_CON0);
    mainpll_con1 = DRV_Reg32(MAINPLL_CON1);

    //main_diff = ((mainpll_con1 >> 12) - 0x8009A) / 2;
    main_diff = (((mainpll_con1 & 0x1FFFFF) >> 12) - 0x9A) / 2;

    if ((mainpll_con0 & 0xFF) == 0x01)
    {
        output_freq = 1001 + (main_diff * 13); // Mhz
    }

    if ((clk_cfg_0 & 0x7) == 1) // SYSPLL1_D2 = MAINPLL / 2 / 2
    {
        bus_clk = ((output_freq * 1000) / 2) / 2;
    }
    else if ((clk_cfg_0 & 0x7) == 2) // SYSPLL_D5 = MAINPLL / 5
    {
        bus_clk = (output_freq * 1000) / 5;
    }
    else if ((clk_cfg_0 & 0x7) == 3) // SYSPLL1_D4 = MAINPLL / 2 / 4
    {
        bus_clk = ((output_freq * 1000) / 2) / 4;
    }
    else if ((clk_cfg_0 & 0x7) == 4) // UNIVPLL_D5 = UNIVPLL / 5
    {
        bus_clk = (1248 * 1000) / 5;
    }
    else if ((clk_cfg_0 & 0x7) == 5) // UNIVPLL2_D2 = UNIVPLL / 3 / 2
    {
        bus_clk = ((1248 * 1000) / 3) / 2;
    }
    else if ((clk_cfg_0 & 0x7) == 6) // DMPLL_CK = DMPLL /2
    {
        bus_clk = (533 * 1000) / 2;
    }
    else if ((clk_cfg_0 & 0x7) == 7) // DMPLL_D2 = DMPLL / 2 /2
    {
        bus_clk = ((533 * 1000) / 2) / 2 ;
    }
    else // CLKSQ
    {
        bus_clk = 26 * 1000;
    }

    //printk("bus frequency = %d Khz\n", bus_clk);

    return bus_clk; // Khz
#endif
}
EXPORT_SYMBOL(mt_get_bus_freq);

unsigned int mt_get_smallcpu_freq(void)
{
    int output = 0;
    unsigned int temp, clk26cali_0, clk_cfg_8, clk_misc_cfg_1/*, clk26cali_1*/;
    unsigned int top_ckmuxsel, top_ckdiv1, ir_rosc_ctl;

    clk26cali_0 = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

    clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
    pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00); // select divider

    clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
    pminit_write(CLK_CFG_8, (46 << 8)); // select armpll_occ_mon

    top_ckmuxsel = DRV_Reg32(TOP_CKMUXSEL);
    pminit_write(TOP_CKMUXSEL, (top_ckmuxsel & 0xFFFFFFFC) | 0x1);

    top_ckdiv1 = DRV_Reg32(TOP_CKDIV1);
    pminit_write(TOP_CKDIV1, (top_ckdiv1 & 0xFFFFFFE0) | 0xb);

    ir_rosc_ctl = DRV_Reg32(IR_ROSC_CTL);
    pminit_write(IR_ROSC_CTL, ir_rosc_ctl | 0x08100000);

    temp = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, temp | 0x1); // start fmeter

    /* wait frequency meter finish */
    while (DRV_Reg32(CLK26CALI_0) & 0x1)
    {
        printk("wait for frequency meter finish, CLK26CALI = 0x%x\n", DRV_Reg32(CLK26CALI_0));
        //mdelay(10);
    }

    temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

    output = ((temp * 26000) / 1024) * 4; // Khz

    pminit_write(CLK_CFG_8, clk_cfg_8);
    pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
    pminit_write(CLK26CALI_0, clk26cali_0);
    pminit_write(TOP_CKMUXSEL, top_ckmuxsel);
    pminit_write(TOP_CKDIV1, top_ckdiv1);
    pminit_write(IR_ROSC_CTL, ir_rosc_ctl);

    //print("CLK26CALI = 0x%x, cpu frequency = %d Khz\n", temp, output);

    return output;
}
EXPORT_SYMBOL(mt_get_smallcpu_freq);

unsigned int mt_get_bigcpu_freq(void)
{
    int output = 0;
    unsigned int temp, clk26cali_0, clk_cfg_8, clk_misc_cfg_1/*, clk26cali_1*/;
    unsigned int top_ckmuxsel, top_ckdiv1, ir_rosc_ctl, ca15l_mon_sel;

    clk26cali_0 = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

    clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
    pminit_write(CLK_MISC_CFG_1, 0xFFFFFF00); // select divider

    clk_cfg_8 = DRV_Reg32(CLK_CFG_8);
    pminit_write(CLK_CFG_8, (46 << 8)); // select abist_cksw

    top_ckmuxsel = DRV_Reg32(TOP_CKMUXSEL);
    pminit_write(TOP_CKMUXSEL, (top_ckmuxsel & 0xFFFFFFF3) | (0x1<<2));

    top_ckdiv1 = DRV_Reg32(TOP_CKDIV1);
    pminit_write(TOP_CKDIV1, (top_ckdiv1 & 0xFFFFFC1F) | (0xb<<5));

    ca15l_mon_sel = DRV_Reg32(CA15L_MON_SEL);
    DRV_WriteReg32(CA15L_MON_SEL, ca15l_mon_sel | 0x00000500);

    ir_rosc_ctl = DRV_Reg32(IR_ROSC_CTL);
    pminit_write(IR_ROSC_CTL, ir_rosc_ctl | 0x10000000);

    temp = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, temp | 0x1); // start fmeter

    /* wait frequency meter finish */
    while (DRV_Reg32(CLK26CALI_0) & 0x1)
    {
        printk("wait for frequency meter finish, CLK26CALI = 0x%x\n", DRV_Reg32(CLK26CALI_0));
        //mdelay(10);
    }

    temp = DRV_Reg32(CLK26CALI_1) & 0xFFFF;

    output = ((temp * 26000) / 1024) * 4; // Khz

    pminit_write(CLK_CFG_8, clk_cfg_8);
    pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
    pminit_write(CLK26CALI_0, clk26cali_0);
    pminit_write(TOP_CKMUXSEL, top_ckmuxsel);
    pminit_write(TOP_CKDIV1, top_ckdiv1);
    DRV_WriteReg32(CA15L_MON_SEL, ca15l_mon_sel);
    pminit_write(IR_ROSC_CTL, ir_rosc_ctl);

    //print("CLK26CALI = 0x%x, cpu frequency = %d Khz\n", temp, output);

    return output;
}
EXPORT_SYMBOL(mt_get_bigcpu_freq);


unsigned int mt_get_mmclk_freq(void)
{
    int output = 0;
    unsigned int temp, clk26cali_0, clk_cfg_9, clk_misc_cfg_1;

    clk26cali_0 = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

    clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
    pminit_write(CLK_MISC_CFG_1, 0x00FFFFFF); // select divider

    clk_cfg_9 = DRV_Reg32(CLK_CFG_9);
    pminit_write(CLK_CFG_9, (5 << 16)); // select abist_cksw

    temp = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, temp | 0x10); // start fmeter

    /* wait frequency meter finish */
    while (DRV_Reg32(CLK26CALI_0) & 0x10)
    {
        printk("wait for emi frequency meter finish, CLK26CALI = 0x%x\n", DRV_Reg32(CLK26CALI_0));
        mdelay(10);
    }

    temp = DRV_Reg32(CLK26CALI_2) & 0xFFFF;

    output = (temp * 26000) / 1024; // Khz

    pminit_write(CLK_CFG_9, clk_cfg_9);
    pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
    pminit_write(CLK26CALI_0, clk26cali_0);

    return output;
}
EXPORT_SYMBOL(mt_get_mmclk_freq);

unsigned int mt_get_mfgclk_freq(void)
{
    int output = 0;
    unsigned int temp, clk26cali_0, clk_cfg_9, clk_misc_cfg_1;

    clk26cali_0 = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, clk26cali_0 | 0x80); // enable fmeter_en

    clk_misc_cfg_1 = DRV_Reg32(CLK_MISC_CFG_1);
    pminit_write(CLK_MISC_CFG_1, 0x00FFFFFF); // select divider

    clk_cfg_9 = DRV_Reg32(CLK_CFG_9);
    pminit_write(CLK_CFG_9, (9 << 16)); // select abist_cksw

    temp = DRV_Reg32(CLK26CALI_0);
    pminit_write(CLK26CALI_0, temp | 0x10); // start fmeter

    /* wait frequency meter finish */
    while (DRV_Reg32(CLK26CALI_0) & 0x10)
    {
        printk("wait for emi frequency meter finish, CLK26CALI = 0x%x\n", DRV_Reg32(CLK26CALI_0));
        mdelay(10);
    }

    temp = DRV_Reg32(CLK26CALI_2) & 0xFFFF;

    output = (temp * 26000) / 1024; // Khz

    pminit_write(CLK_CFG_9, clk_cfg_9);
    pminit_write(CLK_MISC_CFG_1, clk_misc_cfg_1);
    pminit_write(CLK26CALI_0, clk26cali_0);

    return output;
}
EXPORT_SYMBOL(mt_get_mfgclk_freq);

static int smallcpu_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_smallcpu_freq());
    return 0;
}

static int bigcpu_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_bigcpu_freq());
    return 0;
}

static int emi_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_emi_freq());
    return 0;
}

static int bus_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_bus_freq());
    return 0;
}

static int mmclk_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_mmclk_freq());
    return 0;
}

static int mfgclk_speed_dump_read(struct seq_file *m, void *v)
{
    seq_printf(m, "%d\n", mt_get_mfgclk_freq());
    return 0;
}

static int proc_smallcpu_open(struct inode *inode, struct file *file)
{
    return single_open(file, smallcpu_speed_dump_read, NULL);
}
static const struct file_operations smallcpu_fops = {
    .owner = THIS_MODULE,
    .open  = proc_smallcpu_open,
    .read  = seq_read,
};

static int proc_bigcpu_open(struct inode *inode, struct file *file)
{
    return single_open(file, bigcpu_speed_dump_read, NULL);
}
static const struct file_operations bigcpu_fops = {
    .owner = THIS_MODULE,
    .open  = proc_bigcpu_open,
    .read  = seq_read,
};

static int proc_emi_open(struct inode *inode, struct file *file)
{
    return single_open(file, emi_speed_dump_read, NULL);
}
static const struct file_operations emi_fops = {
    .owner = THIS_MODULE,
    .open  = proc_emi_open,
    .read  = seq_read,
};

static int proc_bus_open(struct inode *inode, struct file *file)
{
    return single_open(file, bus_speed_dump_read, NULL);
}
static const struct file_operations bus_fops = {
    .owner = THIS_MODULE,
    .open  = proc_bus_open,
    .read  = seq_read,
};

static int proc_mmclk_open(struct inode *inode, struct file *file)
{
    return single_open(file, mmclk_speed_dump_read, NULL);
}
static const struct file_operations mmclk_fops = {
    .owner = THIS_MODULE,
    .open  = proc_mmclk_open,
    .read  = seq_read,
};

static int proc_mfgclk_open(struct inode *inode, struct file *file)
{
    return single_open(file, mfgclk_speed_dump_read, NULL);
}
static const struct file_operations mfgclk_fops = {
    .owner = THIS_MODULE,
    .open  = proc_mfgclk_open,
    .read  = seq_read,
};



static int __init mt_power_management_init(void)
{
    struct proc_dir_entry *entry = NULL;
    struct proc_dir_entry *pm_init_dir = NULL;
    CHIP_SW_VER ver = mt_get_chip_sw_ver();

    pm_power_off = mt_power_off;

    #if !defined (CONFIG_FPGA_CA7)
     //FIXME: for FPGA early porting
    #if 0
    xlog_printk(ANDROID_LOG_INFO, "Power/PM_INIT", "Bus Frequency = %d KHz\n", mt_get_bus_freq());
    #endif

    //cpu dormant driver init
    mt_cpu_dormant_init();

    // SPM driver init
    spm_module_init();

    // Sleep driver init (for suspend)
    slp_module_init();

    mt_clkmgr_init();

    //dummy invoke enable_pll(ARMCA15PLL, "ca15pll") because ARMCA15PLL is default enabled
    if (ver == CHIP_SW_VER_02)
        enable_pll(ARMCA15PLL, "ca15pll");

    //mt_pm_log_init(); // power management log init

    //FIXME: for FPGA early porting


   	mt_dcm_init(); // dynamic clock management init

    pm_init_dir = proc_mkdir("pm_init", NULL);
    pm_init_dir = proc_mkdir("pm_init", NULL);
    if (!pm_init_dir)
    {
        pr_err("[%s]: mkdir /proc/pm_init failed\n", __FUNCTION__);
    }
    else
    {
        entry = proc_create("smallcpu_speed_dump", S_IRUGO, pm_init_dir, &smallcpu_fops);

        entry = proc_create("bigcpu_speed_dump", S_IRUGO, pm_init_dir, &bigcpu_fops);

        entry = proc_create("emi_speed_dump", S_IRUGO, pm_init_dir, &emi_fops);

        entry = proc_create("bus_speed_dump", S_IRUGO, pm_init_dir, &bus_fops);

        entry = proc_create("mmclk_speed_dump", S_IRUGO, pm_init_dir, &mmclk_fops);

        entry = proc_create("mfgclk_speed_dump", S_IRUGO, pm_init_dir, &mfgclk_fops);
#ifdef TOPCK_LDVT
        entry = proc_create("abist_meter_test", S_IRUGO|S_IWUSR, pm_init_dir, &abist_meter_fops);
        entry = proc_create("ckgen_meter_test", S_IRUGO|S_IWUSR, pm_init_dir, &ckgen_meter_fops);
#endif

        entry = proc_create("ext_buck", S_IRUGO, pm_init_dir, &ext_buck_fops);
    }

    #endif

    return 0;
}

arch_initcall(mt_power_management_init);


static void _pmic_late_init(void)
{
    CHIP_SW_VER ver = mt_get_chip_sw_ver();

    pr_warn("mt_get_chip_sw_ver: %d\n", ver);
    if (ver == CHIP_SW_VER_01)
    {
#ifndef CONFIG_MTK_FORCE_CLUSTER1
        enable_pll(ARMCA15PLL, "pll");
        disable_pll(ARMCA15PLL, "pll");

        _power_off_ca15l_vproc_vsram();
#endif //#ifndef CONFIG_MTK_FORCE_CLUSTER1
    }
}

static int __init mt_pm_late_init(void)
{
#if !defined (MT_DORMANT_UT)
    mt_idle_init ();
#endif //#if !defined (MT_DORMANT_UT)
    _pmic_late_init();
    clk_buf_init();
    return 0;
}

late_initcall(mt_pm_late_init);


MODULE_DESCRIPTION("MTK Power Management Init Driver");
MODULE_LICENSE("GPL");
